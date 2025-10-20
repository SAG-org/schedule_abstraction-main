#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include "yaml-cpp/yaml.h"

#include "mem.hpp"
#include "OptionParser.h"
#include "config.h"
#include "problem.hpp"
#include "global/space.hpp"
#include "io.hpp"
#include "clock.hpp"

#ifdef CONFIG_ANALYSIS_EXTENSIONS
#include "global/extension/mk-firm/mk_firm.hpp"
#include "global/extension/mk-firm/mk_extension.hpp"
#endif

#define MAX_PROCESSORS 512

// command line options
static bool want_verbose;
static bool want_naive;
static bool merge_conservative;
static bool merge_use_job_finish_times;
static int merge_depth;
static bool want_dense;
static bool want_header;

#ifdef CONFIG_PARALLEL
static bool want_parallel = true;
static unsigned int num_threads = 0;
#endif

#ifdef CONFIG_PRUNING
static bool want_focus = false;
static std::string focus_file;
#endif

#ifdef CONFIG_ANALYSIS_EXTENSIONS
static std::string mk_file;
static bool want_mk = false;
#endif

static bool want_precedence = false;
static std::string precedence_file;

static bool want_mutexes = false;
static std::string excl_file;

static bool want_aborts = false;
static std::string aborts_file;

static bool want_multiprocessor = false;
static unsigned int num_processors = 1;

static bool platform_defined = false;
static std::string platform_file;

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
static bool want_dot_graph;
static std::string dot_file;
#endif

static double timeout;
static long mem_max = 0; // in KiB
static unsigned int max_depth = 0;

static bool want_rta_file;
static bool want_width_file;
static bool want_deadline_miss_info;

static bool continue_after_dl_miss = false;

// store jobs file from config if provided
static std::string config_jobs_file;

static void print_header() {
	std::cout << "# file name"
		<< ", schedulable?(1Y/0N)"
		<< ", #jobs"
		<< ", #nodes"
		<< ", #states"
		<< ", #edges"
		<< ", max width"
		<< ", CPU time"
		<< ", memory"
		<< ", timeout"
		<< ", mem_out"
		<< ", #CPUs"
		<< std::endl;
}

struct Analysis_result {
	bool schedulable;
	bool timeout;
	bool out_of_memory;
	unsigned long long number_of_nodes, number_of_states, number_of_edges, max_width, number_of_jobs;
	double cpu_time;
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
	std::string graph;
#endif
	std::string response_times_csv;
	std::string width_evolution_csv;
	std::string deadline_mis_info;
#ifdef CONFIG_ANALYSIS_EXTENSIONS
	std::string mk_results;
#endif
};


template<class Time, class Space>
static Analysis_result analyze(
	std::istream& in,
	std::istream& prec_in,
	std::istream& excl_in,
	std::istream& aborts_in,
	std::istream& platform_in,
	bool in_is_yaml,
	bool dag_is_yaml,
	bool excl_is_yaml,
	bool aborts_is_yaml,
	bool plat_is_yaml)
{
	// Parse input files and create NP scheduling problem description
	typename NP::Job<Time>::Job_set jobs = in_is_yaml ? NP::parse_yaml_job_file<Time>(in) : NP::parse_csv_job_file<Time>(in);
	auto jobs_lookup_table = NP::make_job_lookup_table<Time>(jobs);

	// Parse precedence constraints
	std::vector<NP::Precedence_constraint<Time>> edges = dag_is_yaml ? NP::parse_yaml_dag_file<Time>(prec_in, jobs_lookup_table) : NP::parse_precedence_file<Time>(prec_in, jobs_lookup_table);

	// Parse exclusion (mutex) constraints
	std::vector<NP::Exclusion_constraint<Time>> mutexes;
	if (want_mutexes) {
		mutexes = excl_is_yaml ? NP::parse_yaml_mutex_file<Time>(excl_in, jobs_lookup_table) : NP::parse_mutex_file<Time>(excl_in, jobs_lookup_table);
	}

	// Parse platform specification if provided
	std::vector<Interval<Time>> platform_spec;
	if (platform_defined) {
		platform_spec = plat_is_yaml ? NP::parse_platform_spec_yaml<Time>(platform_in) : NP::parse_platform_spec_csv<Time>(platform_in);
		// Update num_processors based on platform specification
		num_processors = platform_spec.size();
	}
	else {
		platform_spec.resize(num_processors, { 0,0 });
	}

	NP::Scheduling_problem<Time> problem{
		jobs,
		edges,
		NP::parse_abort_file<Time>(aborts_in),
		mutexes,
		platform_spec };

	// Set common analysis options
	NP::Analysis_options opts;
	opts.verbose = want_verbose;
	opts.timeout = timeout;
	opts.max_memory_usage = mem_max;
	opts.max_depth = max_depth;
	opts.early_exit = !continue_after_dl_miss;
	opts.be_naive = want_naive;
	opts.merge_conservative = merge_conservative;
	opts.merge_depth = merge_depth;
	opts.merge_use_job_finish_times = merge_use_job_finish_times;
#ifdef CONFIG_PARALLEL
	opts.parallel_enabled = want_parallel;
	opts.num_threads = num_threads;
#endif
#ifdef CONFIG_ANALYSIS_EXTENSIONS
	auto mk_stream = std::ifstream();
	if (want_mk) {
		mk_stream.open(mk_file);
		//check the extension of the file
		std::string ext = mk_file.substr(mk_file.find_last_of(".") + 1);
		if (ext == "csv" or ext == "CSV") {
			std::cerr << "Error: YAML mk file is not supported yet, use CSV format instead." << std::endl;
			exit(1);
		}
		// Parse the task chain file
		auto mk_constraints = NP::Global::MK_analysis::parse_mk_constraints_csv(mk_stream);
		// Register the task chain analysis extension in the scheduling problem definition
		problem.problem_extensions.template register_extension<NP::Global::MK_analysis::MK_problem_extension>(mk_constraints);
	}
#endif
#ifdef CONFIG_PRUNING
	// set pruning options if requested
	if (want_focus && !focus_file.empty()) {
		opts.pruning_active = true;
		std::ifstream focus_in(focus_file);
		if (!focus_in) {
			std::cerr << "Error: could not open focus YAML file: " << focus_file << std::endl;
			exit(1);
		}
		opts.pruning_cond = NP::parse_focused_expl_spec_yaml<Time>(focus_in, jobs);
	}
#endif
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
	NP::Global::Log_options<Time> log_opts;
	log_opts.log = want_dot_graph;
	NP::Global::Dot_file_config log_print_config;
	if (not dot_file.empty()) {
		auto dot_config_stream = std::ifstream();
		dot_config_stream.open(dot_file);
		if (!dot_config_stream) {
			std::cerr << "Error: could not open log config YAML file: " << dot_file << std::endl;
			exit(1);
		}
		log_opts.log_cond = NP::parse_log_config_yaml<Time>(dot_config_stream, jobs, log_print_config);
	}
	// Actually call the analysis engine
	auto space = Space::explore(problem, opts, log_opts);
#else
	// Actually call the analysis engine
	auto space = Space::explore(problem, opts);
#endif

	// Extract the analysis results
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
	auto graph = std::ostringstream();
	if (want_dot_graph)
		space->print_dot_file(graph, log_print_config);
#endif
	auto rta = std::ostringstream();
	if (want_rta_file) {
		rta << "Task ID, Job ID, BCCT, WCCT, BCRT, WCRT" << std::endl;
		for (const auto& j : problem.jobs) {
			Interval<Time> finish = space->get_finish_times(j);
			rta << j.get_task_id() << ", "
			    << j.get_job_id() << ", "
			    << finish.from() << ", "
			    << finish.until() << ", "
			    << std::max<long long>(0,
			                           (finish.from() - j.earliest_arrival()))
			    << ", "
			    << (finish.until() - j.earliest_arrival())
			    << std::endl;
		}
	}

	auto width_stream = std::ostringstream();
	if (want_width_file) {
		width_stream << "Depth, Width (#Nodes), Width (#States)" << std::endl;
		const std::vector<std::pair<unsigned long, unsigned long>>& width = space->evolution_exploration_front_width();
		for (int d = 0; d < problem.jobs.size(); d++) {
			width_stream << d << ", "
					   << width[d].first
					   << ", "
					   << width[d].second
					   << std::endl;
		}
	}

	auto deadline_miss_stream = std::ostringstream();
	bool schedulable = space->is_schedulable();
	if(want_deadline_miss_info && !schedulable) {
		auto deadline_miss_state = space->get_deadline_miss_state();
		if (deadline_miss_state.first != nullptr) {
			deadline_miss_state.first->export_node(deadline_miss_stream, jobs);
		}
		if (deadline_miss_state.second != nullptr) {
			deadline_miss_state.second->export_state(deadline_miss_stream, jobs);
		}
	}

#ifdef CONFIG_ANALYSIS_EXTENSIONS
	auto mk_results = std::ostringstream();
	if (want_mk) {
		mk_results = space->template get_results<NP::Global::MK_analysis::MK_sp_data_extension<Time>>();
	}
#endif

	Analysis_result results = Analysis_result{
		space->is_schedulable(),
		space->was_timed_out(),
		space->out_of_memory(),
		space->number_of_nodes(),
		space->number_of_states(),
		space->number_of_edges(),
		space->max_exploration_front_width(),
		(unsigned long)(problem.jobs.size()),
		space->get_cpu_time(),
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
		graph.str(),
#endif
		rta.str(),
		width_stream.str(),
		deadline_miss_stream.str()
#ifdef CONFIG_ANALYSIS_EXTENSIONS
		, mk_results.str()
#endif
	};
	return results;
}

static Analysis_result process_stream(
	std::istream &in,
	std::istream &prec_in,
	std::istream &excl_in,
	std::istream &aborts_in,
	std::istream &platform_in,
	bool in_is_yaml = false, 
	bool dag_is_yaml = false, 
	bool excl_is_yaml = false,
	bool aborts_is_yaml = false, 
	bool plat_is_yaml = false)
{
	if (want_multiprocessor && want_dense)
		return analyze<dense_t, NP::Global::State_space<dense_t>>(in, prec_in, excl_in, aborts_in, platform_in, in_is_yaml, dag_is_yaml, excl_is_yaml, aborts_is_yaml, plat_is_yaml);
	else if (want_multiprocessor && !want_dense)
		return analyze<dtime_t, NP::Global::State_space<dtime_t>>(in, prec_in, excl_in, aborts_in, platform_in, in_is_yaml, dag_is_yaml, excl_is_yaml, aborts_is_yaml, plat_is_yaml);
	else if (want_dense)
		return analyze<dense_t, NP::Global::State_space<dense_t>>(in, prec_in, excl_in, aborts_in, platform_in, in_is_yaml, dag_is_yaml, excl_is_yaml, aborts_is_yaml, plat_is_yaml);
	else
		return analyze<dtime_t, NP::Global::State_space<dtime_t>>(in, prec_in, excl_in, aborts_in, platform_in, in_is_yaml, dag_is_yaml, excl_is_yaml, aborts_is_yaml, plat_is_yaml);
}

static void process_file(const std::string& fname)
{
	try {
		Analysis_result result;

		auto empty_dag_stream = std::istringstream("\n");
		auto empty_aborts_stream = std::istringstream("\n");
		auto empty_platform_stream = std::istringstream("\n");
		auto empty_excl_stream = std::istringstream("\n");
		auto dag_stream = std::ifstream();
		auto excl_stream = std::ifstream();
		auto aborts_stream = std::ifstream();
		auto platform_stream = std::ifstream();

	bool in_is_yaml = false, dag_is_yaml = false, excl_is_yaml = false, aborts_is_yaml = false, plat_is_yaml = false;
		
		// check the extension of the job input file
		if (fname != "-") {
			std::string ext = fname.substr(fname.find_last_of(".") + 1);
			if (ext == "yaml" || ext == "yml") {
				in_is_yaml = true;
			}
		}

		if (in_is_yaml && want_precedence) {
			std::cerr << "Error: conflict between job input file and precedence constraints file. Job input file in YAML formal already contains precedence constraints specification." << std::endl;
			exit(1);
		}
		
		if (want_precedence) {
			dag_stream.open(precedence_file);
			if (!dag_stream) {
				std::cerr << "Error: could not open precedence constraints file: " << precedence_file << std::endl;
				exit(1);
			}
		} 
		else if (in_is_yaml) {
			// if precedence file is not given, but input file is in YAML, we assume that the precedence constraints are in the input file
			dag_stream.open(fname);
			want_precedence = true;
			dag_is_yaml = true;
		} 

		if (want_aborts) {
			aborts_stream.open(aborts_file);
			if (!aborts_stream) {
				std::cerr << "Error: could not open aborts file: " << aborts_file << std::endl;
				exit(1);
			}
			//check the extension of the file
			std::string ext = aborts_file.substr(aborts_file.find_last_of(".") + 1);
			if (ext == "yaml" || ext == "yml") {
				aborts_is_yaml = true;
				std::cerr << "Error: YAML abort file is not supported yet, use CSV format instead." << std::endl;
				exit(1);
			}
		}

		if (want_mutexes) {
			excl_stream.open(excl_file);
			if (!excl_stream) {
				std::cerr << "Error: could not open mutex constraints file: " << excl_file << std::endl;
				exit(1);
			}
			std::string ext = excl_file.substr(excl_file.find_last_of(".") + 1);
			if (ext == "yaml" || ext == "yml") {
				excl_is_yaml = true;
			}
		}

		if (platform_defined) {
			platform_stream.open(platform_file);
			if (!platform_stream) {
				std::cerr << "Error: could not open platform specification file: " << platform_file << std::endl;
				exit(1);
			}
			//check the extension of the file
			std::string ext = platform_file.substr(platform_file.find_last_of(".") + 1);
			if (ext == "yaml" || ext == "yml") {
				plat_is_yaml = true;
			}
		}

		std::istream &dag_in = want_precedence ?
			static_cast<std::istream&>(dag_stream) :
			static_cast<std::istream&>(empty_dag_stream);

		std::istream &excl_in = want_mutexes ?
			static_cast<std::istream&>(excl_stream) :
			static_cast<std::istream&>(empty_excl_stream);

		std::istream &aborts_in = want_aborts ?
			static_cast<std::istream&>(aborts_stream) :
			static_cast<std::istream&>(empty_aborts_stream);

		std::istream &platform_in = platform_defined ?
			static_cast<std::istream&>(platform_stream) :
			static_cast<std::istream&>(empty_platform_stream);

		if (fname == "-")
		{
			result = process_stream(std::cin, dag_in, excl_in, aborts_in, platform_in, in_is_yaml, dag_is_yaml, false, aborts_is_yaml, plat_is_yaml);
		}
		else {
	            auto in = std::ifstream(fname, std::ios::in);
			result = process_stream(in, dag_in, excl_in, aborts_in, platform_in, in_is_yaml, dag_is_yaml, excl_is_yaml, aborts_is_yaml, plat_is_yaml);

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
			if (want_dot_graph) {
				DM("\nDot graph being made\n");
				std::string dot_name = fname;
				auto p = dot_name.find_last_of(".");
				if (p != std::string::npos) {
					dot_name.replace(p, std::string::npos, ".dot");
					auto out = std::ofstream(dot_name, std::ios::out);
					out << result.graph;
					out.close();
				}
			}
#endif

			if (want_rta_file) {
				std::string rta_name = fname;
				auto p = rta_name.find_last_of(".");
				if (p != std::string::npos) {
					rta_name.replace(p, std::string::npos, ".rta.csv");
					auto out  = std::ofstream(rta_name,  std::ios::out);
					out << result.response_times_csv;
					out.close();
				}
			}

			if (want_width_file) {
				std::string width_file_name = fname;
				auto p = width_file_name.find_last_of(".");
				if (p != std::string::npos) {
					width_file_name.replace(p, std::string::npos, ".width.csv");
					auto out = std::ofstream(width_file_name, std::ios::out);
					out << result.width_evolution_csv;
					out.close();
				}
			}

			if (want_deadline_miss_info && !result.schedulable && !result.deadline_mis_info.empty()) {
				std::string dl_miss_file_name = fname;
				auto p = dl_miss_file_name.find_last_of(".");
				if (p != std::string::npos) {
					dl_miss_file_name.replace(p, std::string::npos, ".dl_miss.txt");
					auto out = std::ofstream(dl_miss_file_name, std::ios::out);
					out << result.deadline_mis_info;
					out.close();
				}
			}
		}

		if (want_header) {
			print_header();
			want_header = false; // only print header once
		}

		Memory_monitor mem;
		long mem_used = mem;

		std::cout << fname;

		if (max_depth && max_depth < result.number_of_jobs)
			// mark result as invalid due to debug abort
			std::cout << ",  X";
		else
			std::cout << ",  " << (int) result.schedulable;

		std::cout << ",  " << result.number_of_jobs
			  << ",  " << result.number_of_nodes
		          << ",  " << result.number_of_states
		          << ",  " << result.number_of_edges
		          << ",  " << result.max_width
		          << ",  " << std::fixed << result.cpu_time
		          << ",  " << ((double) mem_used) / (1024.0)
		          << ",  " << (int) result.timeout
				  << ",  " << (int)result.out_of_memory
		          << ",  " << num_processors
		          << std::endl;
	} catch (std::ios_base::failure& ex) {
		std::cerr << fname;
		if (want_precedence)
			std::cerr << " + " << precedence_file;
		std::cerr <<  ": parse error" << std::endl;
		exit(1);
	} catch (NP::InvalidJobReference& ex) {
		std::cerr << precedence_file << ": bad job reference: job "
				  << ex.ref.job << " of task " << ex.ref.task
				  << " is not part of the job set given in "
				  << fname
				  << std::endl;
		exit(3);
	} catch (NP::InvalidAbortParameter& ex) {
		std::cerr << aborts_file << ": invalid abort parameter: job "
				  << ex.ref.job << " of task " << ex.ref.task
				  << " has an impossible abort time (abort before release)"
				  << std::endl;
		exit(4);
	} catch (NP::InvalidPrecParameter& ex) {
		std::cerr << precedence_file << ": invalid self-suspending parameter: job "
				  << ex.ref.job << " of task " << ex.ref.task
				  << " has an invalid self-suspending time"
				  << std::endl;
		exit(5);
	} catch (std::exception& ex) {
		std::cerr << fname << ": '" << ex.what() << "'" << std::endl;
		exit(1);
	}
}

// Helper function to parse time limit string (e.g., "1d2h15m30s" -> seconds)
static long long parse_time_limit(const std::string& time_str) {
	if (time_str.empty()) return 0;
	
	long long total_seconds = 0;
	std::string num_str;
	
	for (char c : time_str) {
		if (isdigit(c) || c == '.') {
			num_str += c;
		} else {
			if (!num_str.empty()) {
				double value = std::stod(num_str);
				switch (c) {
					case 'd': total_seconds += static_cast<long long>(value * 86400); break;
					case 'h': total_seconds += static_cast<long long>(value * 3600); break;
					case 'm': total_seconds += static_cast<long long>(value * 60); break;
					case 's': total_seconds += static_cast<long long>(value); break;
					default:
						std::cerr << "Unknown time unit: " << c << std::endl;
						return 0;
				}
				num_str.clear();
			}
		}
	}
	
	// If there's a remaining number without unit, treat it as seconds
	if (!num_str.empty()) {
		total_seconds += static_cast<long long>(std::stod(num_str));
	}
	
	return total_seconds;
}

// Helper function to parse memory limit string (e.g., "512M" -> KiB)
static long parse_memory_limit(const std::string& mem_str) {
	if (mem_str.empty()) return 0;
	
	std::string num_str;
	char unit = 0;
	
	for (char c : mem_str) {
		if (isdigit(c) || c == '.') {
			num_str += c;
		} else {
			unit = c;
			break;
		}
	}
	
	if (num_str.empty()) return 0;
	
	double value = std::stod(num_str);
	
	switch (unit) {
		case 'K': case 'k': return static_cast<long>(value);
		case 'M': case 'm': return static_cast<long>(value * 1024);
		case 'G': case 'g': return static_cast<long>(value * 1024 * 1024);
		default: return static_cast<long>(value); // assume KiB if no unit
	}
}

// Function to parse config YAML file and set options
static void parse_config_file(const std::string& config_file, optparse::Values& options) {
    try {
        YAML::Node config = YAML::LoadFile(config_file);

        // Parse analysis_config section
        if (config["analysis_config"]) {
            auto analysis = config["analysis_config"];

            // Time model
            if (analysis["time_model"] && !options.is_set_by_user("time_model")) {
                options["time_model"] = analysis["time_model"].as<std::string>();
            }

            // Stop at first deadline miss
            if (analysis["stop_at_first_deadline_miss"] && !options.is_set_by_user("go_on_after_dl")) {
                bool stop_at_first = analysis["stop_at_first_deadline_miss"].as<bool>();
                options["go_on_after_dl"] = stop_at_first ? "0" : "1";
            }

            // Merge strategy
            if (analysis["merge_strategy"] && !options.is_set_by_user("merge_opts")) {
                options["merge_opts"] = analysis["merge_strategy"].as<std::string>();
            }

            // Focused exploration
            if (analysis["focused_exploration"] && !options.is_set_by_user("focus_file")) {
                bool want_focus_config = analysis["focused_exploration"].as<bool>();
                if (want_focus_config && config["input"] && config["input"]["focused_expl_config"]) {
                    options["focus_file"] = config["input"]["focused_expl_config"].as<std::string>();
                }
            }

            // Extensions (prepare but don't set anything yet)
            if (analysis["extensions"]) {
                auto ext = analysis["extensions"];
				if (config["input"] && config["input"]["extensions"]) {
					// Future: mk_constraints, task_chains
					auto ext_input = config["input"]["extensions"];
					// Example of parsing for future analysis extensions
					if (ext["activate_mk_analysis"]) {
						bool want_mk = ext["activate_mk_analysis"].as<bool>();
						if (want_mk ) {
							if (ext_input["mk_constraints"])
								options["mk_file"] = ext_input["mk_constraints"].as<std::string>();
							else
								std::cerr << "A mk constraint file must be specified if the mk analysis is activated.";
						}
					}
					/*if (ext["activate_task_chains"]) {
						bool want_tc = ext["activate_task_chains"].as<bool>();
						if (want_tc && ext_input["task_chains"]) {
							options["task_chains_file"] = ext_input["task_chains"].as<std::string>();
						}
					}*/
				}
			}
		}

        // Parse input section
        if (config["input"]) {
            auto input = config["input"];

            // Jobs input
            if (input["jobs"]) {
                std::string jobs = input["jobs"].as<std::string>();
                if (!jobs.empty() && jobs != "none") {
                    config_jobs_file = jobs;
                }
            }

            // Precedence constraints
            if (input["precedence_constraints"] && !options.is_set_by_user("precedence_file")) {
                std::string prec = input["precedence_constraints"].as<std::string>();
                if (prec != "none" && !prec.empty()) {
                    options["precedence_file"] = prec;
                }
            }

            // Exclusion constraints
            if (input["exclusion_constraints"] && !options.is_set_by_user("excl_file")) {
                std::string excl = input["exclusion_constraints"].as<std::string>();
                if (excl != "none" && !excl.empty()) {
                    options["excl_file"] = excl;
                }
            }

            // Abort actions
            if (input["abort_actions"] && !options.is_set_by_user("abort_file")) {
                std::string aborts = input["abort_actions"].as<std::string>();
                if (aborts != "none" && !aborts.empty()) {
                    options["abort_file"] = aborts;
                }
            }

            // Platform
            if (input["platform"] && !options.is_set_by_user("platform_file") && !options.is_set_by_user("num_processors")) {
                std::string platform = input["platform"].as<std::string>();
                if (platform != "none" && !platform.empty()) {
                    options["platform_file"] = platform;
                }
            }
		}

        // Parse output section
        if (config["output"]) {
            auto output = config["output"];
            
            // Response time
            if (output["response_time"] && !options.is_set_by_user("rta")) {
                want_rta_file = output["response_time"].as<bool>();
            }
            
            // Graph width
            if (output["graph_width"] && !options.is_set_by_user("rta")) {
                want_width_file = output["graph_width"].as<bool>();
            }
            
            // Miss state info
            if (output["miss_state_info"] && !options.is_set_by_user("miss_info")) {
                bool want_miss_info = output["miss_state_info"].as<bool>();
                options["miss_info"] = want_miss_info ? "1" : "0";
            }
            
            // Save SAG
            if (output["save_sag"]) {
                auto sag = output["save_sag"];
                if (sag["activate"] && !options.is_set_by_user("want_dot")) {
                    bool want_graph = sag["activate"].as<bool>();
                    options["want_dot"] = want_graph ? "1" : "0";
                }
                if (sag["config"] && !options.is_set_by_user("dot_file")) {
					std::string dot_cfg = sag["config"].as<std::string>();
					if (dot_cfg != "none" && !dot_cfg.empty()) {
                        options["dot_file"] = dot_cfg;
                    }
                }
            }
        }
        
        // Parse limits section
        if (config["limits"]) {
            auto limits = config["limits"];
            
            // Memory usage
            if (limits["memory_usage"] && !options.is_set_by_user("mem_max")) {
                std::string mem_limit = limits["memory_usage"].as<std::string>();
				if (mem_limit != "none") {
					long mem_kib = parse_memory_limit(mem_limit);
					options["mem_max"] = std::to_string(mem_kib);
				}
            }
            
            // Runtime
            if (limits["runtime"] && !options.is_set_by_user("timeout")) {
                std::string time_limit = limits["runtime"].as<std::string>();
				if (time_limit != "none") {
					long long time_sec = parse_time_limit(time_limit);
					options["timeout"] = std::to_string(time_sec);
				}
            }
            
            // Depth
            if (limits["depth"] && !options.is_set_by_user("depth")) {
				std::string depth_limit = limits["depth"].as<std::string>();
				if (depth_limit != "none") {
					options["depth"] = depth_limit;
				}
            }
        }
        
        // Parse pretty_printing section
        if (config["pretty_printing"]) {
            auto pp = config["pretty_printing"];
            
            // Header
            if (pp["header"] && !options.is_set_by_user("print_header")) {
                bool want_header = pp["header"].as<bool>();
                options["print_header"] = want_header ? "1" : "0";
            }
            
            // Verbose
            if (pp["verbose"] && !options.is_set_by_user("verbose")) {
                bool want_verbose = pp["verbose"].as<bool>();
                options["verbose"] = want_verbose ? "1" : "0";
            }
        }
        
        // Parse parallel_exploration section
        if (config["parallel_exploration"]) {
            auto parallel = config["parallel_exploration"];
            
            // Active
            if (parallel["active"] && !options.is_set_by_user("parallel")) {
                bool want_parallel = parallel["active"].as<bool>();
                options["parallel"] = want_parallel ? "1" : "0";
            }
            
            // Threads
            if (parallel["threads"] && !options.is_set_by_user("num_threads")) {
                options["num_threads"] = parallel["threads"].as<std::string>();
            }
        }
		
    } catch (const YAML::Exception& e) {
        std::cerr << "Error parsing config file " << config_file << ": " << e.what() << std::endl;
        exit(1);
    }
}

int main(int argc, char** argv)
{
    auto parser = optparse::OptionParser();

    parser.description("Exact NP Schedulability Tester");
    parser.usage("usage: %prog [OPTIONS]... [JOB SET FILES]...");

    // add an option to show the version
    parser.add_option("-v", "--version").dest("version")
        .action("store_true").set_default("0")
        .help("show program's version number and exit");

    parser.add_option("--config").dest("config_file")
        .metavar("CONFIG-FILE")
        .help("path to YAML configuration file")
        .set_default("");

    parser.add_option("--merge").dest("merge_opts")
		.metavar("MERGE-LEVEL")
		.choices({ "no", "c1", "c2", "l1", "l2", "l3", "lmax"}).choices({ "no", "c1", "c2","l1","l2","l3","lmax"}).set_default("l1")
		.help("choose type of state merging approach used during the analysis. 'no': no merging, 'c1': conservative level 1, 'c2': conservative level 2, 'lx': lossy with depth=x, 'lmax': lossy with max depth. (default: l1)");

	parser.add_option("-t", "--time").dest("time_model")
	      .metavar("TIME-MODEL")
	      .choices({"dense", "discrete"}).set_default("discrete")
	      .help("choose 'discrete' or 'dense' time (default: discrete)");

	parser.add_option("-l", "--time-limit").dest("timeout")
	      .help("maximum CPU time allowed (in seconds, zero means no limit)")
	      .set_default("0");

	parser.add_option("--mem-limit").dest("mem_max")
		.help("maximum memory consumption allowed (in KiB, zero means no limit)")
		.set_default("0");

	parser.add_option("-d", "--depth-limit").dest("depth")
	      .help("abort graph exploration after reaching given depth (>= 2)")
	      .set_default("0");

	parser.add_option("-p", "--precedence").dest("precedence_file")
	      .help("name of the file that contains the job set's precedence DAG")
	      .set_default("");

	parser.add_option("--excl").dest("excl_file")
	      .help("[experimental] name of the file that contains the job set's exclusion (mutex) constraints")
	      .set_default("");

	parser.add_option("-a", "--abort-actions").dest("abort_file")
	      .help("name of the file that contains the job set's abort actions")
	      .set_default("");

	parser.add_option("-m", "--multiprocessor").dest("num_processors")
	      .help("set the number of processors of the platform")
	      .set_default("1");

	parser.add_option("--platform").dest("platform_file")
	      .help("name of the file that contains the platform specification (CSV or YAML)")
	      .set_default("");

	parser.add_option("--header").dest("print_header")
	      .help("print a column header")
	      .action("store_const").set_const("1")
	      .set_default("0");

	parser.add_option("--verbose").dest("verbose").set_default("0")
		.action("store_const").set_const("1")
		.help("show the current status of the analysis (default: off)");

	parser.add_option("-r", "--report").dest("rta").set_default("0")
	      .action("store_const").set_const("1")
	      .help("Reporting: store the best- and worst-case response times and store the evolution of the width of the graph (default: off)");

	parser.add_option("--miss_info").dest("miss_info").set_default("0")
		.action("store_const").set_const("1")
		.help("Report information on the deadline miss state if a deadline miss occurs (default: off)");

	parser.add_option("-c", "--continue-after-deadline-miss")
	      .dest("go_on_after_dl").set_default("0")
	      .action("store_const").set_const("1")
	      .help("do not abort the analysis on the first deadline miss "
	            "(default: off)");

	parser.add_option("-g", "--save-graph").dest("want_dot").set_default("0")
		.action("store_const").set_const("1")
		.help("Records the state graph in Graphviz dot format (default: off).");

	parser.add_option("--log_opts").dest("dot_file")
		.help("If 'save-graph is set', allows to pass a YAML file configuring what part of the state graph is recorded and what is printed in the state graph. Default: complete state space is recorded and default info is printed.")
		.set_default("");

	parser.add_option("--mk").dest("mk_file")
		.help("[experimental] name of the file that contains the mk-firm specifications (CSV)")
		.set_default("");

#ifdef CONFIG_PARALLEL
	parser.add_option("--parallel").dest("parallel").set_default("1")
		.action("store_const").set_const("1")
		.help("enable parallel execution (default: on when compiled with CONFIG_PARALLEL)");

	parser.add_option("--no-parallel").dest("parallel").set_default("1")
		.action("store_const").set_const("0")
		.help("disable parallel execution");

	parser.add_option("--threads").dest("num_threads")
		.help("number of threads to use (0 = auto-detect)")
		.set_default("0");
#endif

	parser.add_option("-f", "--focus").dest("focus_file")
		.help("If 'USE_PRUNING' is set, allows to send a YAML file specifying what part of the state-space to focus on or prune during the exploration.")
		.set_default("");

	auto options = parser.parse_args(argc, argv);
	
	// Parse config file if provided (command line args take precedence)
	if (options.is_set("config_file")) {
		std::string config_file = (const std::string&)options.get("config_file");
		if (!config_file.empty()) {
			parse_config_file(config_file, options);
		}
	}
	
	//all the options that could have been entered above are processed below and appropriate variables
	// are assigned their respective values.

	if(options.get("version")){
		std::cout << parser.prog() <<" version "
				  	<< VERSION_MAJOR << "."
					<< VERSION_MINOR << "."
					<< VERSION_PATCH << std::endl;
		return 0;
	}

	std::string merge_opts = (const std::string&)options.get("merge_opts");
	want_naive = (merge_opts == "no");
	if (merge_opts == "c1") {
		merge_conservative = true;
		merge_use_job_finish_times = false;
	}
	else if (merge_opts == "c2") {
		merge_conservative = true;
		merge_use_job_finish_times = true;
	}
	else if (merge_opts == "l2")
		merge_depth = 2;
	else if (merge_opts == "l3")
		merge_depth = 3;
	else if (merge_opts == "lmax")
		merge_depth = -1;
	else
		merge_depth = 1;

	std::string time_model = (const std::string&)options.get("time_model");
	want_dense = time_model == "dense";

	timeout = options.get("timeout");

	mem_max = options.get("mem_max");

	max_depth = options.get("depth");
	if (options.is_set_by_user("depth")) {
		if (max_depth <= 1) {
			std::cerr << "Error: invalid depth argument\n" << std::endl;
			return 1;
		}
		max_depth -= 1;
	}

	want_precedence = options.is_set("precedence_file");
	if (want_precedence && parser.args().size() > 1) {
		std::cerr << "[!!] Warning: multiple job sets "
		          << "with a single precedence DAG specified."
		          << std::endl;
	}
	precedence_file = (const std::string&) options.get("precedence_file");

	want_aborts = options.is_set("abort_file");
	if (want_aborts && parser.args().size() > 1) {
		std::cerr << "[!!] Warning: multiple job sets "
		          << "with a single abort action list specified."
		          << std::endl;
	}
	aborts_file = (const std::string&) options.get("abort_file");

	want_mutexes = options.is_set("excl_file");
	if (want_mutexes && parser.args().size() > 1) {
		std::cerr << "[!!] Warning: multiple job sets "
		          << "with a single exclusion constraints file specified." 
		          << std::endl;
	}
	else if (want_mutexes)
		std::cout << "[**] Note: support for mutex constraints is only experimental for now." << std::endl;
	excl_file = (const std::string&) options.get("excl_file");

#ifdef CONFIG_ANALYSIS_EXTENSIONS
	want_mk = options.is_set("mk_file");
	if (want_mk && parser.args().size() > 1) {
		std::cerr << "[!!] Warning: multiple job sets "
		          << "with a single mk-firm specifications file specified."
		          << std::endl;
	}
	mk_file = (const std::string&) options.get("mk_file");
#else
	if (options.is_set("mk_file")) {
		std::cerr << "Error: mk-firm analysis support must be enabled "
		          << "during compilation (ANALYSIS_EXTENSIONS must be set)."
		          << std::endl;
		return 2;
	}
#endif

	platform_defined = options.is_set("platform_file");
	if (platform_defined) {
		want_multiprocessor = true;
		platform_file = (const std::string&) options.get("platform_file");
		if (platform_file.empty()) {
			std::cerr << "Error: platform file not specified" << std::endl;
			return 1;
		}
		if (options.get("num_processors")) {
			std::cerr << "Error: options --platform and -m are exclusive." << std::endl;
			return 1; // num_processors is set by platform file, not by user
		}
	}
	else {
		want_multiprocessor = options.get("num_processors");
		num_processors = options.get("num_processors");
		if (!num_processors || num_processors > MAX_PROCESSORS) {
			std::cerr << "Error: invalid number of processors\n" << std::endl;
			return 1;
		}
	}

	want_rta_file = options.get("rta");
	want_width_file = options.get("rta");
	want_deadline_miss_info = options.get("miss_info");

	want_verbose = options.get("verbose");

	continue_after_dl_miss = options.get("go_on_after_dl");

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
	want_dot_graph = options.get("want_dot");
	if (want_dot_graph)
		dot_file = (const std::string&)options.get("dot_file");
	else if (options.get("dot_file")) {
		std::cerr << "Warning: log options are set but log of state graph is not activated ('-g' or '--save-graph' not set). "
			<< "The content of the file passed in argument with '--log_opts' will be ignored." << std::endl;
	}
#else
	if (options.get("want_dot")) {
		std::cerr << "Error: graph collection support must be enabled "
			<< "during compilation (CONFIG_COLLECT_SCHEDULE_GRAPH "
			<< "is not set)." << std::endl;
		return 2;
	}
#endif

#ifdef CONFIG_PARALLEL
	want_parallel = options.get("parallel");
	num_threads = options.get("num_threads");
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
	if (want_parallel && want_dot_graph) {
		std::cerr << "Warning: Parallel execution is not compatible with "
		          << "schedule graph collection. Disabling parallel execution." 
		          << std::endl;
		want_parallel = false;
	}
#endif
#else
	if (options.get("parallel") || options.is_set_by_user("num_threads")) {
		std::cerr << "Error: parallel execution support must be enabled "
				  << "during compilation (CONFIG_PARALLEL is not set)."
				  << std::endl;
		return 2;
	}
#endif
#ifdef CONFIG_PRUNING
	want_focus = options.get("focus_file");
	focus_file = (const std::string&)options.get("focus_file");
#else
	auto focus_file = (const std::string&)options.get("focus_file");
	if(!focus_file.empty())
	{
		std::cerr << "Error: Focused exploration support must be enabled "
				  << "during compilation (CONFIG_PRUNING is not set)."
				  << std::endl;
		return 2;
	}
#endif

	// this prints the header in the output on the console
	want_header = options.get("print_header");

	// process_file is given the arguments that have been passed
    if (!parser.args().empty()) {
        for (auto f : parser.args())
            process_file(f);
    } else if (!config_jobs_file.empty()) {
        process_file(config_jobs_file);
    } else {
        process_file("-");
    }

	return 0;
}
