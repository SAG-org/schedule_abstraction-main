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

#ifdef CONFIG_ANALYSIS_EXTENSIONS
#include "global/extension/state_space_data_extension.hpp"
#endif // CONFIG_ANALYSIS_EXTENSIONS

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
			typedef typename Scheduling_problem<Time>::Mutex_constraints Mutex_constraints;
			typedef typename Scheduling_problem<Time>::Abort_actions Abort_actions;
			typedef Schedule_state<Time> State;
			typedef Schedule_node<Time> Node;
			typedef std::vector<Interval<Time>> CoreAvailability;
			typedef const NP::Job<Time>* Job_ref;
			struct Job_delay {
				Job_ref reference_job;
				Interval<Time> delay;
			};
			typedef std::vector<Job_delay> Delay_list;
			typedef std::vector<Job_index> Job_precedence_set;

			struct Inter_job_constraints {
				// Delay constraints that must be respected between the start of j's predecessors and the start of j
				// This means j can't start until all jobs in predecessors_start_to_start have started.
				Delay_list predecessors_start_to_start;

				// Delay constraints that must be respected between the completion of j's predecessors and the start of j
				// This means j can't start until all jobs in predecessors_finish_to_start have finished.
				Delay_list predecessors_finish_to_start;

				// Delay constraints that must be respected between the start of j and the start of its successors
				// This means none of the jobs in start_to_successors_start can start before j starts.
				Delay_list start_to_successors_start;

				// Delay constraints that must be respected between the completion of j and the start of its successors
				// This means none of the jobs in finish_to_successors_start can start before j has finished.
				Delay_list finish_to_successors_start;

				// Delay constraints that must be respected between the start of j and the start of any other job in that list.
				// There is no ordering constraint. Job j may start before or after the jobs in the list, but the distance between starts must be respected.
				Delay_list between_starts;

				// Delay constraints that must be respected between the completion of j and the start of any other job in that list, or between the completion of the jobs in the list and the start of j.
				// There is no ordering constraint. Job j may start before or after the jobs in the list, but the distance between finish and starts of jobs must be respected.
				Delay_list between_executions;
			
				Time get_min_delay_after_start_of(Job_index predecessor) const {
					for (const auto &pred : predecessors_start_to_start) {
						if (pred.reference_job->get_job_index() == predecessor) return pred.delay.min();
					}
					for (const auto &excl : between_starts) {
						if (excl.reference_job->get_job_index() == predecessor) return excl.delay.min();
					}
					return -1;
				}

				Time get_min_delay_after_finish_of(Job_index predecessor) const {
					for (const auto &pred : predecessors_finish_to_start) {
						if (pred.reference_job->get_job_index() == predecessor) return pred.delay.min();
					}
					for (const auto &excl : between_executions) {
						if (excl.reference_job->get_job_index() == predecessor) return excl.delay.min();
					}
					return -1;
				}
			};

		private:
			typedef std::multimap<Time, Job_ref> By_time_map;

			// not touched after initialization
			By_time_map _successor_jobs_by_latest_arrival;
			By_time_map _sequential_source_jobs_by_latest_arrival;
			By_time_map _gang_source_jobs_by_latest_arrival;
			By_time_map _jobs_by_earliest_arrival;
			By_time_map _jobs_by_deadline;
			std::vector<Job_precedence_set> _must_be_finished_jobs;
			std::vector<Inter_job_constraints> _inter_job_constraints;

			// list of actions when a job is aborted
			std::vector<const Abort_action<Time>*> abort_actions;

			// number of cores
			const unsigned int num_cpus;

#ifdef CONFIG_ANALYSIS_EXTENSIONS
			// possible extensions of the state space data (e.g., for task chains analysis)
			State_space_data_extensions extensions;
#endif // CONFIG_ANALYSIS_EXTENSIONS
					
		public:
			// use these const references to ensure read-only access
			const Workload& jobs;
			const By_time_map& jobs_by_earliest_arrival;
			const By_time_map& jobs_by_deadline;
			const By_time_map& successor_jobs_by_latest_arrival;
			const By_time_map& sequential_source_jobs_by_latest_arrival;
			const By_time_map& gang_source_jobs_by_latest_arrival;
			const std::vector<Job_precedence_set>& must_be_finished_jobs;
			const std::vector<Inter_job_constraints>& inter_job_constraints;

			State_space_data(const Workload& jobs,
				const Precedence_constraints& edges,
				const Abort_actions& aborts,
				const Mutex_constraints& mutexes,
				unsigned int num_cpus)
				: jobs(jobs)
				, num_cpus(num_cpus)
				, successor_jobs_by_latest_arrival(_successor_jobs_by_latest_arrival)
				, sequential_source_jobs_by_latest_arrival(_sequential_source_jobs_by_latest_arrival)
				, gang_source_jobs_by_latest_arrival(_gang_source_jobs_by_latest_arrival)
				, jobs_by_earliest_arrival(_jobs_by_earliest_arrival)
				, jobs_by_deadline(_jobs_by_deadline)
				, _must_be_finished_jobs(jobs.size())
				, must_be_finished_jobs(_must_be_finished_jobs)
				, _inter_job_constraints(jobs.size())
				, inter_job_constraints(_inter_job_constraints)
				, abort_actions(jobs.size(), NULL)
			{
				for (const auto& e : edges) {
					if (e.get_type() == start_to_start) {
						_inter_job_constraints[e.get_fromIndex()].start_to_successors_start.push_back({ &jobs[e.get_toIndex()], e.get_delay() });
						_inter_job_constraints[e.get_toIndex()].predecessors_start_to_start.push_back({ &jobs[e.get_fromIndex()], e.get_delay() });
					}
					if (e.get_type() == finish_to_start) {
						_inter_job_constraints[e.get_fromIndex()].finish_to_successors_start.push_back({ &jobs[e.get_toIndex()], e.get_delay() });
						_inter_job_constraints[e.get_toIndex()].predecessors_finish_to_start.push_back({ &jobs[e.get_fromIndex()], e.get_delay() });
						_must_be_finished_jobs[e.get_toIndex()].push_back(e.get_fromIndex());
					}
				}

				for (const Job<Time>& j : jobs) {
					const Inter_job_constraints& job_suspensions = _inter_job_constraints[j.get_job_index()];
					if (job_suspensions.predecessors_finish_to_start.size() > 0 || job_suspensions.predecessors_start_to_start.size() > 0) {
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

				for (const auto& m : mutexes) {
					// NOTE: we exclude jobs with delay_max == 0 because the start constraint does not constrain anything then
					if (m.get_type() == start_exclusion and m.get_max_delay() > 0) {
						_inter_job_constraints[m.get_jobA_index()].between_starts.push_back({ &jobs[m.get_jobB_index()], m.get_delay() });
						_inter_job_constraints[m.get_jobB_index()].between_starts.push_back({ &jobs[m.get_jobA_index()], m.get_delay() });
					}
					if (m.get_type() == exec_exclusion) {
						_inter_job_constraints[m.get_jobA_index()].between_executions.push_back({ &jobs[m.get_jobB_index()], m.get_delay() });
						_inter_job_constraints[m.get_jobB_index()].between_executions.push_back({ &jobs[m.get_jobA_index()], m.get_delay() });
						_must_be_finished_jobs[m.get_jobA_index()].push_back(m.get_jobB_index());
						_must_be_finished_jobs[m.get_jobB_index()].push_back(m.get_jobA_index());
					}
				}
			}

#ifdef CONFIG_ANALYSIS_EXTENSIONS
			const State_space_data_extensions& get_extensions() const
			{
				return extensions;
			}
#endif // CONFIG_ANALYSIS_EXTENSIONS

			size_t get_num_cpus() const
			{
				return num_cpus;
			}

			size_t num_jobs() const
			{
				return jobs.size();
			}

			const Job_precedence_set& get_finished_jobs_if_starts(Job_index j) const
			{
				return must_be_finished_jobs[j];
			}

			const Abort_action<Time>* abort_action_of(Job_index j) const
			{
				return abort_actions[j];
			}

			// returns the ready time interval of `j` in `s`
			// assumes all predecessors of j are dispatched
			Interval<Time> ready_times(const Node& n, const State& s, const Job<Time>& j) const
			{
				Interval<Time> r = j.arrival_window();
				const auto& cstr = inter_job_constraints[j.get_job_index()];
				for (const auto& pred : cstr.predecessors_start_to_start)
				{
					Interval<Time> st{ 0, 0 };
					s.get_start_times(pred.reference_job->get_job_index(), st);
					r.lower_bound(st.min() + pred.delay.min());
					r.extend_to(st.max() + pred.delay.max());
				}
				for (const auto& excl : cstr.between_starts)
				{
					if (!dispatched(n, *excl.reference_job))
						continue; // doesn't constrain anything if the other job is not dispatched yet
					Interval<Time> st{ 0, 0 };
					s.get_start_times(excl.reference_job->get_job_index(), st);
					r.lower_bound(st.min() + excl.delay.min());
					r.extend_to(st.max() + excl.delay.max());
				}
				for (const auto& pred : cstr.predecessors_finish_to_start)
				{
					Interval<Time> ft{ 0, 0 };
					s.get_finish_times(pred.reference_job->get_job_index(), ft);
					r.lower_bound(ft.min() + pred.delay.min());
					r.extend_to(ft.max() + pred.delay.max());
				}
				for (const auto& excl : cstr.between_executions)
				{
					if (!dispatched(n, *excl.reference_job))
						continue; // doesn't constrain anything if the other job is not dispatched yet
					Interval<Time> ft{ 0, 0 };
					s.get_start_times(excl.reference_job->get_job_index(), ft);
					r.lower_bound(ft.min() + excl.delay.min());
					r.extend_to(ft.max() + excl.delay.max());
				}
				return r;
			}

			// Assume: `j` is already dispatched
			// return: true if `j` is certainly finished when the first core becomes available, false otherwise
			bool is_certainly_finished(const Node& n, const State& s, const Job_index j, const Interval<Time>& finish_time) const
			{
				// If there is a single core, `j` must have finished when the core becomes available,
				// since we assumed that `j` was already dispatched.
				if (num_cpus == 1) 
					return true;

				// The optimization above can be generalized to multiple cores, using the following knowledge:
				// We will prove the following claim: (ft(j) denotes the finish time of j and ca(n) denotes core_availability(n))
				// If ft(j).max() < ca(2).min() then no core can be available before j is finished.
				// Proof:
				// (A) Assume for a contradiction that a core becomes available at time T before j is finished at time F > T.
				//
				// (B) Since a core became available at time T, it must hold that ca(1).min <= T <= ca(1).max().
				//
				// (C) Since j finishes at time F > T, we know that at least 2 cores must be available at time F:
				//     - the one that became available at time T, and
				//     - the one used by j
				//
				// (D) So ca(2).min() <= F <= ft(j).max() hence ca(2).min() <= ft(j).max().
				//
				// (E) this yields a contradiction with the condition ft(j).max() < ca(2).min().
				if (finish_time.max() < s.core_availability(2).min())
					return true; // by the proof above j is certainly finished when the first core becomes available

				// If at least one successor of j has already been dispatched, then j must have finished already.
				for (const auto& s : inter_job_constraints[j].finish_to_successors_start) {
					if (dispatched(n, *s.reference_job))
						return true;
				}

				return false;
			}

			// Assuming that:
			// - `j_low` is dispatched next, and
			// - `j_high` is of higher priority than `j_low`, and
			// - all predecessors of `j_high` have been dispatched
			//
			// this function computes the latest ready time of `j_high` in system state 's'.
			//
			// Let `ready_low` denote the earliest time at which `j_low` becomes ready
			// and let `latest_ready_high` denote the return value of this function.
			//
			// If `latest_ready_high <= `ready_low`, the assumption that `j_low` is dispatched next lead to a contradiction,
			// hence `j_low` cannot be dispatched next. In this case, the exact value of `latest_ready_high` is meaningless,
			// except that it must be at most `ready_low`. After all, it was computed under an assumption that cannot happen.
			Time conditional_latest_ready_time(
				const Node& n, const State& s,
				const Job<Time>& j_high, const Job_index j_low,
				const unsigned int j_low_required_cores = 1) const
			{
				Time latest_ready_high = j_high.arrival_window().max();

				// if the minimum parallelism of j_high is more than j_low_required_cores, then
				// for j to be released and have its successors completed
				// is not enough to interfere with a lower priority job.
				// It must also have enough cores free.
				if (j_high.get_min_parallelism() > j_low_required_cores)
				{
					// max {rj_max,Amax(sjmin)}
					latest_ready_high = std::max(latest_ready_high, s.core_availability(j_high.get_min_parallelism()).max());
				}

				// j_high is not ready until all its predecessors have completed, and their corresponding delays are over.
				// But, since we are assuming that `j_low` is dispatched next and all predecessors of `j_high` have been dispatched,
				// we can disregard some of the predecessors.
				const auto& cstr = inter_job_constraints[j_high.get_job_index()];
				for (const auto& prec : cstr.predecessors_start_to_start)
				{
					const auto delay_max = prec.delay.max();
					// Since all predecessors must have been dispatched already, we can disregard
					// this constraint if there is no delay between the predecessor's start and j_high's start
					if (delay_max == 0) 
						continue;

					const auto pred_idx = prec.reference_job->get_job_index();
					// skip if its also a predecessor of j_low and the delay to j_low's start can not be smaller than the delay to j_high's start
					Time min_s2s_delay = inter_job_constraints[j_low].get_min_delay_after_start_of(pred_idx);
					if (min_s2s_delay != -1 && min_s2s_delay >= delay_max) 
						continue;

					Time min_f2s_delay = inter_job_constraints[j_low].get_min_delay_after_finish_of(pred_idx);
					if (min_f2s_delay != -1 && min_f2s_delay + prec.reference_job->least_exec_time() >= delay_max) 
						continue;

					Interval<Time> st{ 0, 0 };
					s.get_start_times(pred_idx, st);
					latest_ready_high = std::max(latest_ready_high, st.max() + delay_max);
				}

				for (const auto& excl : cstr.between_starts)
				{
					// disregard this exclusion constraint if the job is not dispatched yet
					if (!dispatched(n, *excl.reference_job))
						continue; 

					const auto delay_max = excl.delay.max();
					const auto other_idx = excl.reference_job->get_job_index();
					// skip if its also a predecessor of j_low and the delay to j_low's start can not be smaller than the delay to j_high's start
					Time min_s2s_delay = inter_job_constraints[j_low].get_min_delay_after_start_of(other_idx);
					if (min_s2s_delay != -1 && min_s2s_delay >= delay_max) 
						continue;

					Time min_f2s_delay = inter_job_constraints[j_low].get_min_delay_after_finish_of(other_idx);
					if (min_f2s_delay != -1 && min_f2s_delay + excl.reference_job->least_exec_time() >= delay_max) 
						continue;

					Interval<Time> st{ 0, 0 };
					s.get_start_times(other_idx, st);
					latest_ready_high = std::max(latest_ready_high, st.max() + delay_max);
				}

				for (const auto& prec : cstr.predecessors_finish_to_start)
				{
					const auto delay_max = prec.delay.max();
					auto pred_idx = prec.reference_job->get_job_index();
					Interval<Time> ft{ 0, 0 };
					s.get_finish_times(pred_idx, ft);

					// If the delay is 0 and j_pred is certainly finished when j_low is dispatched, then j_pred cannot postpone
					// the (latest) ready time of j_high.
					if (delay_max == 0 && is_certainly_finished(n, s, pred_idx, ft)) 
						continue;

					// If j_pred is a predecessor of both j_high and j_low, we can disregard it if the maximum delay from j_pred to j_high
					// is at most the minimum delay from j_pred to j_low: delay_max(j_pred -> j_high) <= delay_min(j_pred -> j_low).
					Time min_f2s_delay = inter_job_constraints[j_low].get_min_delay_after_finish_of(pred_idx);
					if (min_f2s_delay != -1 && min_f2s_delay >= delay_max) 
						continue;

					Time min_s2s_delay = inter_job_constraints[j_low].get_min_delay_after_start_of(pred_idx);
					if (min_s2s_delay != -1 && min_s2s_delay >= delay_max + prec.reference_job->least_exec_time()) 
						continue;

					latest_ready_high = std::max(latest_ready_high, ft.max() + delay_max);
				}

				for (const auto& excl : cstr.between_executions)
				{
					auto other_idx = excl.reference_job->get_job_index();
					if (!dispatched(n, *excl.reference_job))
						continue; // disregard this exclusion constraint if the job is not dispatched yet

					Interval<Time> ft{ 0, 0 };
					s.get_finish_times(other_idx, ft);
					const auto delay_max = excl.delay.max();
					// If the delay is 0 and j_pred is certainly finished when j_low is dispatched, then j_pred cannot postpone
					// the (latest) ready time of j_high.
					if (delay_max == 0 && is_certainly_finished(n, s, other_idx, ft))
						continue;

					// If j_pred is a predecessor of both j_high and j_low, we can disregard it if the maximum delay from j_pred to j_high
					// is at most the minimum delay from j_pred to j_low: delay_max(j_pred -> j_high) <= delay_min(j_pred -> j_low).
					Time min_f2s_delay = inter_job_constraints[j_low].get_min_delay_after_finish_of(other_idx);
					if (min_f2s_delay != -1 && min_f2s_delay >= delay_max) 
						continue;

					Time min_s2s_delay = inter_job_constraints[j_low].get_min_delay_after_start_of(other_idx);
					if (min_s2s_delay != -1 && min_s2s_delay >= delay_max + excl.reference_job->least_exec_time())
						continue;

					latest_ready_high = std::max(latest_ready_high, ft.max() + delay_max);
				}
				return latest_ready_high;
			}

			// returns the earliest time at which `j` may become ready in `s`
			// assumes all predecessors of `j` are completed
			Time earliest_ready_time(const Node& n, const State& s, const Job<Time>& j) const
			{
				return ready_times(n, s, j).min();
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
					if (not_dispatched(n, j) && j.higher_priority_than(reference_job))
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
					if (not_dispatched(n, j) && j.higher_priority_than(reference_job))
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

			// Assuming that `reference_job` is dispatched next, find the earliest time by which a successor job (i.e., a job with predecessors) 
			// of higher priority than the reference_job is certainly ready in system state 's'.
			//
			// Let `ready_min` denote the earliest time at which `reference_job` becomes ready
			// and let `latest_ready_high` denote the return value of this function.
			//
			// If `latest_ready_high <= `ready_min`, the assumption that `reference_job` is dispatched next lead to a contradiction,
			// hence `reference_job` cannot be dispatched next. In this case, the exact value of `latest_ready_high` is meaningless,
			// except that it must be at most `ready_min`. After all, it was computed under an assumption that cannot happen.
			Time next_certain_higher_priority_successor_job_ready_time(
				const Node& n,
				const State& s,
				const Job<Time>& reference_job,
				const unsigned int ncores
			) const {
				auto ready_min = earliest_ready_time(n, s, reference_job);
				Time latest_ready_high = Time_model::constants<Time>::infinity();

				// a higer priority successor job cannot be ready before 
				// a job of any priority is released
				for (auto it = n.get_ready_successor_jobs().begin();
					it != n.get_ready_successor_jobs().end(); it++)
				{
					const Job<Time>& j_high = **it;

					// j_high is not relevant if it is already scheduled or not of higher priority
					if (j_high.higher_priority_than(reference_job)) {
						// does it beat what we've already seen?
						latest_ready_high = std::min(latest_ready_high, conditional_latest_ready_time(n, s, j_high, reference_job.get_job_index(), ncores));
						if (latest_ready_high <= ready_min) break;
					}
				}
				return latest_ready_high;
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
					if (&j == &ignored_job || dispatched(n, j))
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
					if (jp == &ignored_job || dispatched(n, *jp))
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
					if (jp == &ignored_job || dispatched(n, *jp))
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

			bool not_dispatched(const Node& n, const Job<Time>& j) const
			{
				return n.job_not_dispatched(j.get_job_index());
			}

			bool dispatched(const Node& n, const Job<Time>& j) const
			{
				return n.job_dispatched(j.get_job_index());
			}

			State_space_data(const State_space_data& origin) = delete;
		};
	}
}
#endif
