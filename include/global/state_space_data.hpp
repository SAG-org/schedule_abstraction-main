#ifndef PROBLEM_DATA_H
#define PROBLEM_DATA_H

#include <algorithm>
#include <deque>
#include <forward_list>
#include <map>
#include <unordered_map>
#include <vector>

#include <cassert>
#include <iostream>
#include <ostream>

#include "problem.hpp"
#include "global/state.hpp"

namespace NP {
	namespace Global {

		template<class Time> class Schedule_state;
		template<class Time> class Schedule_node;

		template<class Time> class State_space_data
		{
		public:

			typedef Scheduling_problem<Time> Problem;
			typedef typename Scheduling_problem<Time>::Workload Workload;
			typedef typename Scheduling_problem<Time>::Precedence_constraints Precedence_constraints;
			typedef typename Scheduling_problem<Time>::Abort_actions Abort_actions;
			typedef Schedule_state<Time> State;
			typedef Schedule_node<Time> Node;
			typedef typename std::vector<Interval<Time>> CoreAvailability;
			typedef typename Schedule_state<Time>::Job_ref Job_ref;
			typedef typename Schedule_state<Time>::Suspensions_list Suspensions_list;
			typedef typename Schedule_state<Time>::Inter_job_constraints Inter_job_constraints;
			typedef std::vector<Job_index> Job_precedence_set;

		private:
			typedef std::multimap<Time, const NP::Job<Time>*> By_time_map;

			// not touched after initialization
			By_time_map _successor_jobs_by_latest_arrival;
			By_time_map _sequential_source_jobs_by_latest_arrival;
			By_time_map _gang_source_jobs_by_latest_arrival;
			By_time_map _jobs_by_earliest_arrival;
			By_time_map _jobs_by_deadline;
			std::vector<Job_precedence_set> _predecessors;
			std::vector<Job_precedence_set> _finish_to_start_predecessors;

			// not touched after initialization
			std::vector<Inter_job_constraints> _suspensions;

			// list of actions when a job is aborted
			std::vector<const Abort_action<Time>*> abort_actions;

			// number of cores
			const unsigned int num_cpus;
		
		public:
			// use these const references to ensure read-only access
			const Workload& jobs;
			const By_time_map& jobs_by_earliest_arrival;
			const By_time_map& jobs_by_deadline;
			const By_time_map& successor_jobs_by_latest_arrival;
			const By_time_map& sequential_source_jobs_by_latest_arrival;
			const By_time_map& gang_source_jobs_by_latest_arrival;
			const std::vector<Job_precedence_set>& predecessors;
			const std::vector<Inter_job_constraints>& suspensions;
			const std::vector<Job_precedence_set>& finish_to_start_predecessors;

			State_space_data(const Workload& jobs,
				const Precedence_constraints& edges,
				const Abort_actions& aborts,
				unsigned int num_cpus)
				: jobs(jobs)
				, num_cpus(num_cpus)
				, successor_jobs_by_latest_arrival(_successor_jobs_by_latest_arrival)
				, sequential_source_jobs_by_latest_arrival(_sequential_source_jobs_by_latest_arrival)
				, gang_source_jobs_by_latest_arrival(_gang_source_jobs_by_latest_arrival)
				, jobs_by_earliest_arrival(_jobs_by_earliest_arrival)
				, jobs_by_deadline(_jobs_by_deadline)
				, _predecessors(jobs.size())
				, predecessors(_predecessors)
				, _suspensions(jobs.size())
				, suspensions(_suspensions)
				, _finish_to_start_predecessors(jobs.size())
				, finish_to_start_predecessors(_finish_to_start_predecessors)
				, abort_actions(jobs.size(), NULL)
			{
				for (const auto& e : edges) {
					if (e.get_type() == start_to_start) {
						_suspensions[e.get_fromIndex()].start_before_start.push_back({ &jobs[e.get_toIndex()], e.get_suspension() });
						_suspensions[e.get_toIndex()].start_after_start.push_back({ &jobs[e.get_fromIndex()], e.get_suspension() });
					}
					if (e.get_type() == finish_to_start) {
						_suspensions[e.get_fromIndex()].finish_before_start.push_back({ &jobs[e.get_toIndex()], e.get_suspension() });
						_suspensions[e.get_toIndex()].start_after_finish.push_back({ &jobs[e.get_fromIndex()], e.get_suspension() });
						_finish_to_start_predecessors[e.get_toIndex()].push_back(e.get_fromIndex());
					}
					_predecessors[e.get_toIndex()].push_back(e.get_fromIndex());
				}

				for (const Job<Time>& j : jobs) {
					const Inter_job_constraints &job_suspensions = _suspensions[j.get_job_index()];
					if (job_suspensions.start_after_finish.size() + job_suspensions.start_after_start.size() > 0) {
						_successor_jobs_by_latest_arrival.insert({ j.latest_arrival(), &j });
					}
					else if (j.get_min_parallelism() == 1) {
						_sequential_source_jobs_by_latest_arrival.insert({ j.latest_arrival(), &j });
						_jobs_by_earliest_arrival.insert({ j.earliest_arrival(), &j });
					}
					else {
						_gang_source_jobs_by_latest_arrival.insert({ j.latest_arrival(), &j });
						_jobs_by_earliest_arrival.insert({ j.earliest_arrival(), &j });
					}					
					_jobs_by_deadline.insert({ j.get_deadline(), &j });
				}

				for (const Abort_action<Time>& a : aborts) {
					const Job<Time>& j = lookup<Time>(jobs, a.get_id());
					abort_actions[j.get_job_index()] = &a;
				}
			}

			size_t num_jobs() const
			{
				return jobs.size();
			}

			const Job_precedence_set& predecessors_of(const Job<Time>& j) const
			{
				return predecessors[j.get_job_index()];
			}

			const Job_precedence_set& predecessors_of(Job_index j) const
			{
				return predecessors[j];
			}

			const Job_precedence_set& finish_to_start_predecessors_of(Job_index j) const
			{
				return finish_to_start_predecessors[j];
			}

			const Abort_action<Time>* abort_action_of(Job_index j) const
			{
				return abort_actions[j];
			}

			// returns the ready time interval of `j` in `s`
			// assumes all predecessors of j are completed
			Interval<Time> ready_times(const State& s, const Job<Time>& j) const
			{
				Interval<Time> r = j.arrival_window();
				for (const auto& pred : suspensions[j.get_job_index()].start_after_start)
				{
					Interval<Time> ft{ 0, 0 };
					s.get_start_times(pred.first->get_job_index(), ft);
					r.lower_bound(ft.min() + pred.second.min());
					r.extend_to(ft.max() + pred.second.max());
				}
				for (const auto& pred : suspensions[j.get_job_index()].start_after_finish)
				{
					Interval<Time> ft{ 0, 0 };
					s.get_finish_times(pred.first->get_job_index(), ft);
					r.lower_bound(ft.min() + pred.second.min());
					r.extend_to(ft.max() + pred.second.max());
				}
				return r;
			}

			// returns the ready time interval of `j` in `s` when dispatched on `ncores`
			// assumes all predecessors of j have been dispatched
			// ignores the finish times of some common predecessors of j and lower-priority job j_low
			Interval<Time> ready_times(
				const State& s, const Schedule_node<Time> &n, const Job<Time>& j,
				const Job_index j_low,
				const unsigned int ncores) const
			{
				Time avail_min = s.earliest_finish_time();
				Interval<Time> r = j.arrival_window();

				// if the minimum parallelism of j is more than ncores, then 
				// for j to be released and have its successors completed 
				// is not enough to interfere with a lower priority job.
				// It must also have enough cores free.
				if (j.get_min_parallelism() > ncores)
				{
					// max {rj_max,Amax(sjmin)}
					r.extend_to(s.core_availability(j.get_min_parallelism()).max());
				}

				for (const auto& pred : suspensions[j.get_job_index()].start_after_start)
				{
					// Since pred must have been dispatched already, we can disregard it when there is no suspension
					if (pred.second.max() == 0) continue;

					// skip if its also a predecessor of j_low and the suspension to j_low can not be smaller than to j
					Time min_low_suspension = suspensions[j_low].get_min_start_after_start_suspension(pred.first->get_job_index());
					if (min_low_suspension != Time_model::constants<Time>::infinity() && min_low_suspension >= pred.second.max()) continue;

					min_low_suspension = suspensions[j_low].get_min_start_after_finish_suspension(pred.first->get_job_index());
					if (min_low_suspension != Time_model::constants<Time>::infinity() && min_low_suspension + pred.first->least_exec_time() >= pred.second.max()) continue;

					// if there is no suspension time and there is a single core, then
					// predecessors are finished as soon as the processor becomes available
					if (num_cpus == 1 && pred.second.max() == 0)
					{
						r.lower_bound(avail_min);
						r.extend_to(avail_min);
					}
					else
					{
						Interval<Time> ft{ 0, 0 }; // TODO Also try the max_bound trick here
						s.get_start_times(pred.first->get_job_index(), ft);
						r.lower_bound(ft.min() + pred.second.min());
						r.extend_to(ft.max() + pred.second.max());
					}
				}

				for (const auto& pred : suspensions[j.get_job_index()].start_after_finish)
				{
					// Skip finished jobs without suspension
					if (pred.second.max() == 0 && n.get_certainly_finished_jobs().contains(pred.first->get_job_index())) continue;

					// skip if its also a predecessor of j_low and the suspension to j_low can not be smaller than to j
					Time min_low_suspension = suspensions[j_low].get_min_start_after_finish_suspension(pred.first->get_job_index());
					if (min_low_suspension != Time_model::constants<Time>::infinity() && min_low_suspension >= pred.second.max()) continue;

					min_low_suspension = suspensions[j_low].get_min_start_after_start_suspension(pred.first->get_job_index());
					if (min_low_suspension != Time_model::constants<Time>::infinity() && min_low_suspension >= pred.second.max() + pred.first->least_exec_time()) continue;

					// if there is no suspension time and there is a single core, then
					// predecessors are finished as soon as the processor becomes available
					if (num_cpus == 1 && pred.second.max() == 0)
					{
						r.lower_bound(avail_min);
						r.extend_to(avail_min);
					}
					else
					{
						Interval<Time> ft{ 0, 0 };
						s.get_finish_times(pred.first->get_job_index(), ft);
						const Time min_bound = ft.min() + pred.second.min();
						const Time max_bound = ft.max() + pred.second.max();
						if (
							ncores == 1 && num_cpus >= 2 && min_bound == s.core_availability(1).min() && 
							max_bound == s.core_availability(1).max() && max_bound <= s.core_availability(2).min()
						) continue;
						r.lower_bound(min_bound);
						r.extend_to(max_bound);
					}
				}
				return r;
			}

			// returns the latest time at which `j` may become ready in `s`
			// assumes all predecessors of `j` are completed
			Time latest_ready_time(const State& s, const Job<Time>& j) const
			{
				return ready_times(s, j).max();
			}

			// returns the latest time at which `j_hp` may become ready in `s` when executing on `ncores`
			// ignoring the finish time of all predecessors `j_hp` has in common with `j_ref`.
			// assumes all predecessors of `j_hp` are completed
			Time latest_ready_time(
				const State& s, const Schedule_node<Time> &n, Time earliest_ref_ready,
				const Job<Time>& j_hp, const Job<Time>& j_ref,
				const unsigned int ncores = 1) const
			{
				auto rt = ready_times(s, n, j_hp, j_ref.get_job_index(), ncores);
				return std::max(rt.max(), earliest_ref_ready);
			}

			// returns the earliest time at which `j` may become ready in `s`
			// assumes all predecessors of `j` are completed
			Time earliest_ready_time(const State& s, const Job<Time>& j) const
			{
				return ready_times(s, j).min(); // std::max(s.core_availability().min(), j.arrival_window().min());
			}

			// Find next time by which a sequential source job (i.e., 
			// a job without predecessors that can execute on a single core) 
			// of higher priority than the reference_job
			// is certainly released in any state in the node 'n'. 
			Time next_certain_higher_priority_seq_source_job_release(
				const Node& n,
				const Job<Time>& reference_job,
				Time until = Time_model::constants<Time>::infinity()) const
			{
				Time when = until;

				// a higher priority source job cannot be released before 
				// a source job of any priority is released
				Time t_earliest = n.get_next_certain_source_job_release();

				for (auto it = sequential_source_jobs_by_latest_arrival.lower_bound(t_earliest);
					it != sequential_source_jobs_by_latest_arrival.end(); it++)
				{
					const Job<Time>& j = *(it->second);

					// check if we can stop looking
					if (when < j.latest_arrival())
						break; // yep, nothing can lower 'when' at this point

					// j is not relevant if it is already scheduled or not of higher priority
					if (unfinished(n, j) && j.higher_priority_than(reference_job))
					{
						when = j.latest_arrival();
						// Jobs are ordered by latest_arrival, so next jobs are later. 
						// We can thus stop searching.
						break;
					}
				}
				return when;
			}

			// Find next time by which a gang source job (i.e., 
			// a job without predecessors that cannot execute on a single core) 
			// of higher priority than the reference_job
			// is certainly released in state 's' of node 'n'. 
			Time next_certain_higher_priority_gang_source_job_ready_time(
				const Node& n,
				const State& s,
				const Job<Time>& reference_job,
				const unsigned int ncores,
				Time until = Time_model::constants<Time>::infinity()) const
			{
				Time when = until;

				// a higher priority source job cannot be released before 
				// a source job of any priority is released
				Time t_earliest = n.get_next_certain_source_job_release();

				for (auto it = gang_source_jobs_by_latest_arrival.lower_bound(t_earliest);
					it != gang_source_jobs_by_latest_arrival.end(); it++)
				{
					const Job<Time>& j = *(it->second);

					// check if we can stop looking
					if (when < j.latest_arrival())
						break; // yep, nothing can lower 'when' at this point

					// j is not relevant if it is already scheduled or not of higher priority
					if (unfinished(n, j) && j.higher_priority_than(reference_job))
					{
						// if the minimum parallelism of j is more than ncores, then 
						// for j to be released and have its successors completed 
						// is not enough to interfere with a lower priority job.
						// It must also have enough cores free.
						if (j.get_min_parallelism() > ncores)
						{
							// max {rj_max,Amax(sjmin)}
							when = std::min(when, std::max(j.latest_arrival(), s.core_availability(j.get_min_parallelism()).max()));
							// no break as other jobs may require less cores to be available and thus be ready earlier
						}
						else
						{
							when = std::min(when, j.latest_arrival());
							// jobs are ordered in non-decreasing latest arrival order, 
							// => nothing tested after can be ready earlier than j
							// => we break
							break;
						}
					}
				}
				return when;
			}

			// Find next time by which a successor job (i.e., a job with predecessors) 
			// of higher priority than the reference_job
			// is certainly released in system state 's' at or before a time 'until'.
			Time next_certain_higher_priority_successor_job_ready_time(
				const Node& n,
				const State& s,
				const Job<Time>& reference_job,
				const unsigned int ncores,
				Time until = Time_model::constants<Time>::infinity()) const
			{
				auto ready_min = earliest_ready_time(s, reference_job);
				Time when = until;

				// a higer priority successor job cannot be ready before 
				// a job of any priority is released
				for (auto it = n.get_ready_successor_jobs().begin();
					it != n.get_ready_successor_jobs().end(); it++)
				{
					const Job<Time>& j = **it;

					// j is not relevant if it is already scheduled or not of higher priority
					if (j.higher_priority_than(reference_job)) {
						// does it beat what we've already seen?
						when = std::min(when,
							latest_ready_time(s, n, ready_min, j, reference_job, ncores));
						// No break, as later jobs might have less suspension or require less cores to start executing.
					}
				}
				return when;
			}

			// Find the earliest possible job release of all jobs in a node except for the ignored job
			Time earliest_possible_job_release(
				const Node& n,
				const Job<Time>& ignored_job) const
			{
				DM("      - looking for earliest possible job release starting from: "
					<< n.earliest_job_release() << std::endl);

				for (auto it = jobs_by_earliest_arrival.lower_bound(n.earliest_job_release());
					it != jobs_by_earliest_arrival.end(); 	it++)
				{
					const Job<Time>& j = *(it->second);

					DM("         * looking at " << j << std::endl);

					// skip if it is the one we're ignoring or if it was dispatched already
					if (&j == &ignored_job || !unfinished(n, j))
						continue;

					DM("         * found it: " << j.earliest_arrival() << std::endl);
					// it's incomplete and not ignored => found the earliest
					return j.earliest_arrival();
				}

				DM("         * No more future releases" << std::endl);
				return Time_model::constants<Time>::infinity();
			}

			// Find the earliest certain job release of all sequential source jobs 
			// (i.e., without predecessors and with minimum parallelism = 1) 
			// in a node except for the ignored job
			Time earliest_certain_sequential_source_job_release(
				const Node& n,
				const Job<Time>& ignored_job) const
			{
				DM("      - looking for earliest certain source job release starting from: "
					<< n.get_next_certain_source_job_release() << std::endl);

				for (auto it = sequential_source_jobs_by_latest_arrival.lower_bound(n.get_next_certain_source_job_release());
					it != sequential_source_jobs_by_latest_arrival.end(); it++)
				{
					const Job<Time>* jp = it->second;
					DM("         * looking at " << *jp << std::endl);

					// skip if it is the one we're ignoring or the job was dispatched already
					if (jp == &ignored_job || !unfinished(n, *jp))
						continue;

					DM("         * found it: " << jp->latest_arrival() << std::endl);
					// it's incomplete and not ignored => found the earliest
					return jp->latest_arrival();
				}
				DM("         * No more future source job releases" << std::endl);
				return Time_model::constants<Time>::infinity();
			}

			// Find the earliest certain job release of all source jobs (i.e., without predecessors) 
			// in a node except for the ignored job
			Time earliest_certain_source_job_release(
				const Node& n,
				const Job<Time>& ignored_job) const
			{
				DM("      - looking for earliest certain source job release starting from: "
					<< n.get_next_certain_source_job_release() << std::endl);

				Time rmax = earliest_certain_sequential_source_job_release(n, ignored_job);

				for (auto it = gang_source_jobs_by_latest_arrival.lower_bound(n.get_next_certain_source_job_release());
					it != gang_source_jobs_by_latest_arrival.end(); it++)
				{
					const Job<Time>* jp = it->second;
					DM("         * looking at " << *jp << std::endl);

					// skip if it is the one we're ignoring or the job was dispatched already
					if (jp == &ignored_job || !unfinished(n, *jp))
						continue;

					DM("         * found it: " << jp->latest_arrival() << std::endl);
					// it's incomplete and not ignored => found the earliest
					return std::min(rmax, jp->latest_arrival());
				}

				DM("         * No more future releases" << std::endl);
				return rmax;
			}

			Time get_earliest_job_arrival() const
			{
				if (jobs_by_earliest_arrival.empty())
					return Time_model::constants<Time>::infinity();
				else
					return jobs_by_earliest_arrival.begin()->first;
			}

			// Find the earliest certain job release of all sequential source jobs
			// (i.e., without predecessors and with minimum parallelism = 1) when
			// the system starts
			Time get_earliest_certain_seq_source_job_release() const
			{
				if (sequential_source_jobs_by_latest_arrival.empty())
					return Time_model::constants<Time>::infinity();
				else
					return sequential_source_jobs_by_latest_arrival.begin()->first;
			}

			// Find the earliest certain job release of all gang source jobs
			// (i.e., without predecessors and with possible parallelism > 1) when
			// the system starts
			Time get_earliest_certain_gang_source_job_release() const
			{
				if (gang_source_jobs_by_latest_arrival.empty())
					return Time_model::constants<Time>::infinity();
				else
					return gang_source_jobs_by_latest_arrival.begin()->first;
			}

		private:

			bool unfinished(const Node& n, const Job<Time>& j) const
			{
				return n.job_incomplete(j.get_job_index());
			}

			State_space_data(const State_space_data& origin) = delete;
		};
	}
}
#endif
