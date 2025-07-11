#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "global\state.hpp"
#include <deque>
#include <memory>

namespace NP {
	namespace Global {

		template <typename Time>
		class SAG_logger {
			typedef std::shared_ptr<Schedule_node<Time>> Node_ptr;
			typedef std::shared_ptr<Schedule_state<Time>> State_ptr;
			struct Edge {
				const Job<Time>* scheduled;
				const Node_ptr source, target;
				const Interval<Time> finish_range;
				const unsigned int parallelism;

				Edge(const Job<Time>* s, const Node_ptr& src, const Node_ptr& tgt,
					const Interval<Time>& fr, unsigned int parallelism = 1)
					: scheduled(s)
					, source(src)
					, target(tgt)
					, finish_range(fr)
					, parallelism(parallelism)
				{
				}

				bool deadline_miss_possible() const
				{
					return scheduled->exceeds_deadline(finish_range.upto());
				}

				Time earliest_finish_time() const
				{
					return finish_range.from();
				}

				Time latest_finish_time() const
				{
					return finish_range.upto();
				}

				Time earliest_start_time() const
				{
					return finish_range.from() - scheduled->least_exec_time();
				}

				Time latest_start_time() const
				{
					return finish_range.upto() - scheduled->maximal_exec_time();
				}

				unsigned int parallelism_level() const
				{
					return parallelism;
				}
			};

			std::deque<Edge> edges;

			// the logger stores the state transitions based on the following conditions
			// - the monitored_interval interval in which the job is dispatched
			Interval<Time> monitored_interval;
			// - the monitored_depth of the SAG (how many monitored_jobs have been dispatched so far)
			Interval<unsigned long> monitored_depth;
			// - the task dispatched
			std::set<unsigned long> monitored_tasks;
			// - the job dispatched
			std::set<Job_index> monitored_jobs;
			// - the set of monitored_jobs that have been dispatched so far
			//std::set<Job_index> dispatched;
			// - the set of monitored_jobs that have not been dispatched so far
			//std::set<Job_index> not_dispatched;
			// - whether the job misses its deadline or not
			bool deadline_miss;
			bool always_log = false;

		public:
			// default constructor (monitors everything)
			SAG_logger()
				: monitored_interval(0, Time_model::constants<Time>::infinity())
				, monitored_depth(0, 0-1)
				, deadline_miss(false)
				, always_log(true)
			{
			}

			SAG_logger(Interval<Time> t, Interval<unsigned long> depth, std::set<unsigned long> tasks, 
				std::set<Job_index> jobs, 
				//std::set<Job_index> dispatched, std::set<Job_index> not_dispatched, 
				bool deadline_miss)
			: monitored_interval(t)
			, monitored_depth(depth)
			, monitored_tasks(tasks)
			, monitored_jobs(jobs)
			//, dispatched(dispatched)
			//, not_dispatched(not_dispatched)
			, deadline_miss(deadline_miss)
			{
				// check if we should log everything
				if(deadline_miss==false && monitored_depth.min() == 0 && monitored_depth.max() == 0-1 
					&& monitored_interval.min() == 0 && monitored_interval.max() == Time_model::constants<Time>::infinity()
					&& monitored_tasks.empty() && monitored_jobs.empty())
				
				{
					always_log = true;
				}
			}

			// save the dispatch of job j between old_node and new_node
			void log_job_dispatched(const Node_ptr& old_node, const Job<Time>& dispatched_j,
				const Interval<Time>& start, Interval<Time> finish, unsigned int parallelism,
				const Node_ptr& new_node, unsigned long depth) {
				if (always_log || must_log(dispatched_j, start, dispatched_j.exceeds_deadline(finish.upto()), depth)) {
					// create an edge and add it to the edge queue
					edges.emplace_back(&dispatched_j, old_node, new_node, finish, parallelism);
				}
			}

			void print_dot_file(std::ostream& out,
				const typename Job<Time>::Job_set& jobs)
			{
				std::map<const Schedule_node<Time>*, unsigned int> node_id;
				unsigned int i = 0;

				out << "digraph {" << std::endl;
				for (const Edge& e : edges) {
					// check if we already printed the nodes
					if (node_id.count(e.source.get()) == 0) {
						node_id[e.source.get()] = i;
						print_vertex(out, i, e.source, jobs);
						i++;
					}
					if (node_id.count(e.target.get()) == 0) {
						node_id[e.target.get()] = i;
						print_vertex(out, i, e.target, jobs);
						i++;
					}

					print_edge(out, e, node_id[e.source.get()], node_id[e.target.get()]);
				}
				out << "}" << std::endl;
			}

		private:
			bool must_log(const Job<Time>& dispatched_j, const Interval<Time>& start, 
				bool deadline_missed, unsigned long d) const
			{
				return (!deadline_miss || (deadline_miss && deadline_missed))
					&& (d >= monitored_depth.min() && d <= monitored_depth.max())
					&& (start.max() >= monitored_interval.min() && start.min() <= monitored_interval.max())
					&& (monitored_jobs.empty() || monitored_jobs.find(dispatched_j.get_job_index()) != monitored_jobs.end())
					&& (monitored_tasks.empty() || monitored_tasks.find(dispatched_j.get_task_id()) != monitored_tasks.end());
			}

			void print_vertex(std::ostream& out, int id, const Node_ptr& n,
				const typename Job<Time>::Job_set& jobs) const
			{
				out << "\tN" << id
					<< "[label=\"N" << id << ": {";
				const auto* n_states = n->get_states();

				for (const State_ptr& s : *n_states)
				{
					out << "[";
					s->print_vertex_label(out, jobs);
					out << "]\\n";
				}
				out << "}"
					<< "\\nER=";
				if (n->earliest_job_release() ==
					Time_model::constants<Time>::infinity()) {
					out << "N/A";
				}
				else {
					out << n->earliest_job_release();
				}
				out << "\"];"
					<< std::endl;
			}

			void print_edge(std::ostream& out, const Edge& e, int source, int target) const
			{
				out << "\tN" << source
					<< " -> "
					<< "N" << target
					<< "[label=\""
					<< "T" << e.scheduled->get_task_id()
					<< " J" << e.scheduled->get_job_id()
					<< "\\nDL=" << e.scheduled->get_deadline()
					<< "\\nES=" << e.earliest_start_time()
					<< "\\nLS=" << e.latest_start_time()
					<< "\\nEF=" << e.earliest_finish_time()
					<< "\\nLF=" << e.latest_finish_time()
					<< "\"";
				if (e.deadline_miss_possible()) {
					out << ",color=Red,fontcolor=Red";
				}
				out << ",fontsize=8" << "]"
					<< ";"
					<< std::endl;
				if (e.deadline_miss_possible()) {
					out << "N" << target
						<< "[color=Red];"
						<< std::endl;
				}
			}
		};
	}
}
#endif // LOGGER_HPP