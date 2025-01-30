#ifndef IO_HPP
#define IO_HPP

#include <iostream>
#include <utility>

#include "interval.hpp"
#include "time.hpp"
#include "jobs.hpp"
#include "precedence.hpp"
#include "aborts.hpp"
#include "yaml-cpp/yaml.h"

namespace NP {

	inline void skip_over(std::istream& in, char c)
	{
		while (in.good() && in.get() != (int) c)
			/* skip */;
	}

	inline bool skip_one(std::istream& in, char c)
	{
		if (in.good() && in.peek() == (int) c) {
			in.get(); /* skip */
			return true;
		} else
			return false;
	}

	inline void skip_all(std::istream& in, char c)
	{
		while (skip_one(in, c))
			/* skip */;
	}

	inline bool more_data(std::istream& in)
	{
		in.peek();
		return !in.eof();
	}

	inline bool more_fields_in_line(std::istream& in)
	{
		if (!in.good() || in.peek() == (int)'\n' || in.peek() == (int)'\r')
			return false;
		else
			return true;
	}

	inline void next_field(std::istream& in)
	{
	  	while(in.good() && (in.peek() == ',' || in.peek()==' '))
		{
			// eat up any trailing spaces
			skip_all(in, ' ');
			// eat up field separator
			skip_one(in, ',');
		}
	}

	inline void next_field(std::istream& in, char field_delimiter)
	{
		while (in.good() && (in.peek() == field_delimiter || in.peek() == ' '))
		{
			// eat up any trailing spaces
			skip_all(in, ' ');
			// eat up field separator
			skip_one(in, field_delimiter);
		}
	}

	inline void next_line(std::istream& in)
	{
		skip_over(in, '\n');
	}

	inline JobID parse_job_id(std::istream& in)
	{
		unsigned long jid, tid;

		in >> tid;
		next_field(in);
		in >> jid;
		return JobID(jid, tid);
	}

	template<class Time>
	void parse_job_cost(std::istream& in, std::map<unsigned int, Interval<Time>>& costs)
	{
		Time cost_min, cost_max;
		skip_all(in, ' ');
		if (in.peek() == '{') { // expected format: { paral:cost_min:cost_max; paral:cost_min:cost_max; ... }
			unsigned int paral;

			skip_one(in, '{');
			skip_all(in, ' ');
			while (in.peek() != '}') {
				in >> paral;
				next_field(in, ':');
				in >> cost_min;
				next_field(in, ':');
				in >> cost_max;
				costs.emplace(std::make_pair(paral, Interval<Time>{cost_min, cost_max}));
				skip_all(in, ' ');
				if (in.peek() == ';')
					skip_one(in, ';');
			}
			skip_one(in, '}');
		}
		else { 
			in >> cost_min;
			next_field(in);
			in >> cost_max;
			costs.emplace(std::make_pair(1, Interval<Time>{ cost_min, cost_max }));
		}			
	}

	//Functions that help parse selfsuspending tasks file
	template<class Time>
	Precedence_constraint<Time> parse_precedence_constraint(std::istream &in)
	{
		unsigned long from_tid, from_jid, to_tid, to_jid;
		Time sus_min=0, sus_max=0;
		std::string signal_type;
		Precedence_type type = finish_to_start;

		std::ios_base::iostate state_before = in.exceptions();

		in.exceptions(std::istream::failbit | std::istream::badbit);

		in >> from_tid;
		next_field(in);
		in >> from_jid;
		next_field(in);
		in >> to_tid;
		next_field(in);
		in >> to_jid;
		next_field(in);
		if (more_fields_in_line(in))
		{
			in >> sus_min;
			next_field(in);
			in >> sus_max;
		}

		next_field(in);
		if (more_fields_in_line(in))
		{
			in >> signal_type;
			std::transform(signal_type.begin(), signal_type.end(), signal_type.begin(), [](unsigned char c){ return std::tolower(c); });
			if (signal_type == "s") type = start_to_start;
			else if (signal_type != "f") throw std::invalid_argument("Unexpected type: must be 's' or 'f'");
		}

		in.exceptions(state_before);

		return Precedence_constraint<Time>{JobID{from_jid, from_tid},
											JobID{to_jid, to_tid},
											Interval<Time>{sus_min, sus_max},
											type};
	}

	template<class Time>
	std::vector<Precedence_constraint<Time>> parse_precedence_file(std::istream& in)
	{
		// skip column headers
		next_line(in);
		std::vector<Precedence_constraint<Time>> cstr;

		// parse all rows
		while (more_data(in)) {
			// each row contains one self-suspending constraint
			cstr.push_back(parse_precedence_constraint<Time>(in));
			next_line(in);
		}
		return cstr;
	}

	template<class Time>
	inline std::vector<Precedence_constraint<Time>> parse_yaml_dag_file(std::istream& in)
	{
		std::vector<Precedence_constraint<Time>> edges;
		// Clear any flags
		in.clear();
		// Move the pointer to the beginning
		in.seekg(0, std::ios::beg);
		try {
			// read the YAML file
			YAML::Node input_job_set = YAML::Load(in);
			auto const js = input_job_set["jobset"];
			// Iterate over each jobset entry
			for (auto const &j : js) {
				// Check if a job has a successor
				if (j["Successors"]) {
					auto from = JobID(j["Job ID"].as<unsigned long>(), j["Task ID"].as<unsigned long>());
					// Iterate over each successor
					for (const auto &succ: j["Successors"]) {
						// first, we need to check to see if it is written
						// in the compact form [TaskID, JobID]
						// or the expanded form
						// - Task ID: Int
						// 	 Job ID: Int
						if (succ.IsSequence()) {
							auto tid = succ[0].as<unsigned long>();
							auto jid = succ[1].as<unsigned long>();
							auto to = JobID(jid, tid);
							edges.push_back(Precedence_constraint<Time>(from, to, {0, 0}));
						} else {
							auto tid = succ["Task ID"].as<unsigned long>();
							auto jid = succ["Job ID"].as<unsigned long>();
							auto to = JobID(jid, tid);
							edges.push_back(Precedence_constraint<Time>(from, to, {0, 0}));
						}
					}
				}
			}

		} catch (const YAML::Exception& e) {
			std::cerr << "Error reading YAML file: " << e.what() << std::endl;
		}
		return edges;
	}

	template<class Time> 
	Job<Time> parse_job(std::istream& in, std::size_t idx)
	{
		unsigned long tid, jid;

		std::ios_base::iostate state_before = in.exceptions();

		Time arr_min, arr_max, dl, prio;
		std::map<unsigned int, Interval<Time>> cost;

		in.exceptions(std::istream::failbit | std::istream::badbit);

		in >> tid;
		next_field(in);
		in >> jid;
		next_field(in);
		in >> arr_min;
		next_field(in);
		in >> arr_max;
		next_field(in);
		parse_job_cost(in, cost);
		next_field(in);
		in >> dl;
		next_field(in);
		in >> prio;

		in.exceptions(state_before);

		return Job<Time> {jid, Interval<Time>{arr_min, arr_max},
						cost, dl, prio, idx, tid};
	}

	template<class Time>
	typename Job<Time>::Job_set parse_csv_job_file(std::istream& in)
	{
		// first row contains a comment, just skip it
		next_line(in);

		typename Job<Time>::Job_set jobs;
		std::size_t idx=0;

		while (more_data(in)) {
			jobs.push_back(parse_job<Time>(in, idx));
			idx++;
			// munge any trailing whitespace or extra columns
			next_line(in);
		}

		return jobs;
	}

	//Functions that help parse the abort actions file
	template<class Time>
	typename Job<Time>::Job_set parse_yaml_job_file(std::istream& in)
	{
		typename Job<Time>::Job_set jobs;
        unsigned long tid, jid;
        Time arr_min, arr_max, cost_min, cost_max, dl, prio;
		try {
			YAML::Node input_job_set = YAML::Load(in);

			auto const js = input_job_set["jobset"];
			for (auto const &j: js) {
				tid = j["Task ID"].as<unsigned long>();
				jid = j["Job ID"].as<unsigned long>();
				arr_min = j["Arrival min"].as<Time>();
				arr_max = j["Arrival max"].as<Time>();
				cost_min = j["Cost min"].as<Time>();
				cost_max = j["Cost max"].as<Time>();
				dl = j["Deadline"].as<Time>();
				prio = j["Priority"].as<Time>();

				jobs.push_back(Job<Time>{jid, Interval<Time>{arr_min, arr_max},
										 Interval<Time>{cost_min, cost_max}, dl, prio, tid});
			}
		} catch (const YAML::Exception& e) {
			std::cerr << "Error reading YAML file: " << e.what() << std::endl;
		}

		return jobs;
	}

	template<class Time>
	Abort_action<Time> parse_abort_action(std::istream& in)
	{
		unsigned long tid, jid;
		Time trig_min, trig_max, cleanup_min, cleanup_max;

		std::ios_base::iostate state_before = in.exceptions();

		in.exceptions(std::istream::failbit | std::istream::badbit);

		in >> tid;
		next_field(in);
		in >> jid;
		next_field(in);
		in >> trig_min;
		next_field(in);
		in >> trig_max;
		next_field(in);
		in >> cleanup_min;
		next_field(in);
		in >> cleanup_max;

		in.exceptions(state_before);

		return Abort_action<Time>{JobID{jid, tid},
		                          Interval<Time>{trig_min, trig_max},
		                          Interval<Time>{cleanup_min, cleanup_max}};
	}


	template<class Time>
	std::vector<Abort_action<Time>> parse_abort_file(std::istream& in)
	{
		// first row contains a comment, just skip it
		next_line(in);

		std::vector<Abort_action<Time>> abort_actions;

		while (more_data(in)) {
			abort_actions.push_back(parse_abort_action<Time>(in));
			// munge any trailing whitespace or extra columns
			next_line(in);
		}

		return abort_actions;
	}


}

#endif
