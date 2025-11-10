#ifndef NODE_HPP
#define NODE_HPP
#include <algorithm>
#include <cassert>
#include <iostream>
#include <ostream>
#include <set>

#ifdef CONFIG_PARALLEL
#include <tbb/spin_rw_mutex.h>
#include <tbb/spin_mutex.h>
#endif

#include "global/state.hpp"
#include "global/state_space_data.hpp"
#include "global/ready_jobs_tracker.hpp"
#include "global/pending_successors_tracker.hpp"
#include "jobs.hpp"
#include "util.hpp"

namespace NP {

	namespace Global {

		typedef Index_set Job_set;
		typedef std::vector<Job_index> Job_precedence_set;

		template<class Time> class State_space_data;
		template<class Time> class Schedule_state;

		template<class Time> class Schedule_node
		{
		private:
			using Workload = typename State_space_data<Time>::Workload;
			typedef Schedule_state<Time> State;
			typedef std::shared_ptr<State> State_ref;
			typedef std::vector<Interval<Time>> Core_availability;
			using Inter_job_constraints = typename State_space_data<Time>::Inter_job_constraints;

			Time earliest_pending_release;
			Time next_certain_successor_jobs_dispatch;
			Time next_certain_source_job_release;
			Time next_certain_sequential_source_job_release;
			Time next_certain_gang_source_job_dispatch;

			Job_set scheduled_jobs;
			// set of jobs that have all their predecessors completed and were not dispatched yet
			Ready_jobs_tracker<Time> ready_successor_jobs;
			// set of jobs that have at least one unscheduled successor with a start-to-start or finish-to-start constraint
			Pending_successors_tracker<Time> jobs_with_pending_successors;

			hash_value_t lookup_key;
			Interval<Time> finish_time;
			Time a_max;
			unsigned int num_cpus;
			unsigned int num_jobs_scheduled;

#ifdef CONFIG_PARALLEL
			// Thread-safe state container and synchronization
			mutable tbb::spin_rw_mutex states_mutex;
#endif

			// no accidental copies
			Schedule_node(const Schedule_node& origin) = delete;

			struct eft_compare
			{
				bool operator() (State_ref x, State_ref y) const
				{
					return x->earliest_core_availability() < y->earliest_core_availability();
				}
			};

			typedef typename std::multiset<State_ref, eft_compare> State_ref_queue;
			State_ref_queue states;

		public:

			// initial node (for convenience for unit tests)
			Schedule_node(unsigned int num_cores)
				: lookup_key{ 0 }
				, num_cpus(num_cores)
				, finish_time{ 0,0 }
				, a_max{ 0 }
				, num_jobs_scheduled(0)
				, earliest_pending_release{ 0 }
				, next_certain_source_job_release{ Time_model::constants<Time>::infinity() }
				, next_certain_successor_jobs_dispatch{ Time_model::constants<Time>::infinity() }
				, next_certain_sequential_source_job_release{ Time_model::constants<Time>::infinity() }
				, next_certain_gang_source_job_dispatch{ Time_model::constants<Time>::infinity() }
			{
			}

			// initial node
			Schedule_node(unsigned int num_cores, const State_space_data<Time>& state_space_data)
				: lookup_key{ 0 }
				, num_cpus(num_cores)
				, finish_time{ 0,0 }
				, a_max{ 0 }
				, num_jobs_scheduled(0)
				, earliest_pending_release{ state_space_data.get_earliest_job_arrival() }
				, next_certain_successor_jobs_dispatch{ Time_model::constants<Time>::infinity() }
				, next_certain_sequential_source_job_release{ state_space_data.get_earliest_certain_seq_source_job_release() }
				, next_certain_gang_source_job_dispatch{ Time_model::constants<Time>::infinity() }
			{
				next_certain_source_job_release = std::min(next_certain_sequential_source_job_release, state_space_data.get_earliest_certain_gang_source_job_release());
			}

			Schedule_node(const std::vector<Interval<Time>>& proc_initial_state, const State_space_data<Time>& state_space_data)
				: lookup_key{ 0 }
				, num_cpus(proc_initial_state.size())
				, finish_time{ 0, 0 }
				, a_max{ Time_model::constants<Time>::infinity() }
				, num_jobs_scheduled(0)
				, earliest_pending_release{ state_space_data.get_earliest_job_arrival() }
				, next_certain_successor_jobs_dispatch{ Time_model::constants<Time>::infinity() }
				, next_certain_sequential_source_job_release{ state_space_data.get_earliest_certain_seq_source_job_release() }
				, next_certain_gang_source_job_dispatch{ Time_model::constants<Time>::infinity() }
			{
				Time a_min = Time_model::constants<Time>::infinity();
				for (const auto& a : proc_initial_state) {
					a_max = std::min(a_max, a.max());
					a_min = std::min(a_min, a.min());
				}
				finish_time.extend_to(a_max);
				finish_time.lower_bound(a_min);

				next_certain_source_job_release = std::min(next_certain_sequential_source_job_release, state_space_data.get_earliest_certain_gang_source_job_release());
			}


			// transition: new node by scheduling a job 'j' in an existing node 'from'
			Schedule_node(
				const Schedule_node& from,
				const Job<Time>& j,
				std::size_t idx,
				const State_space_data<Time>& state_space_data,
				const Time next_earliest_release,
				const Time next_certain_source_job_release, // the next time a job without predecessor is certainly released
				const Time next_certain_sequential_source_job_release // the next time a job without predecessor that can execute on a single core is certainly released
			)
				: scheduled_jobs{ from.scheduled_jobs, idx }
				, lookup_key{ from.next_key(j) }
				, num_cpus(from.num_cpus)
				, num_jobs_scheduled(from.num_jobs_scheduled + 1)
				, finish_time{ 0, Time_model::constants<Time>::infinity() }
				, a_max{ Time_model::constants<Time>::infinity() }
				, earliest_pending_release{ next_earliest_release }
				, next_certain_source_job_release{ next_certain_source_job_release }
				, next_certain_successor_jobs_dispatch{ Time_model::constants<Time>::infinity() }
				, next_certain_sequential_source_job_release{ next_certain_sequential_source_job_release }
				, next_certain_gang_source_job_dispatch{ Time_model::constants<Time>::infinity() }
			{
				ready_successor_jobs.update(from.ready_successor_jobs, idx, state_space_data.inter_job_constraints, scheduled_jobs);
				jobs_with_pending_successors.update(from.jobs_with_pending_successors, idx, state_space_data.inter_job_constraints, this->scheduled_jobs);
			}

			void reset(const std::vector<Interval<Time>>& proc_initial_state, const State_space_data<Time>& state_space_data)
			{
#ifdef CONFIG_PARALLEL
				tbb::spin_rw_mutex::scoped_lock lock(states_mutex, true); // write lock
#endif
				states.clear();

				lookup_key = 0;
				num_cpus = proc_initial_state.size();
				finish_time = { 0,0 };
				a_max = Time_model::constants<Time>::infinity();
				Time a_min = Time_model::constants<Time>::infinity();
				for (const auto& a : proc_initial_state) {
					a_max = std::min(a_max, a.max());
					a_min = std::min(a_min, a.min());
				}
				finish_time.extend_to(a_max);
				finish_time.lower_bound(a_min);

				scheduled_jobs.clear();
				num_jobs_scheduled = 0;
				ready_successor_jobs.clear();
				jobs_with_pending_successors.clear();
				earliest_pending_release = state_space_data.get_earliest_job_arrival();
				next_certain_successor_jobs_dispatch = Time_model::constants<Time>::infinity();
				next_certain_sequential_source_job_release = state_space_data.get_earliest_certain_seq_source_job_release();
				next_certain_gang_source_job_dispatch = Time_model::constants<Time>::infinity();
				next_certain_source_job_release = std::min(next_certain_sequential_source_job_release, state_space_data.get_earliest_certain_gang_source_job_release());
			}

			void reset(unsigned int num_cores, const State_space_data<Time>& state_space_data)
			{
#ifdef CONFIG_PARALLEL
				tbb::spin_rw_mutex::scoped_lock lock(states_mutex, true); // write lock
#endif
				states.clear();

				lookup_key = 0;
				num_cpus = num_cores;
				finish_time = { 0,0 };
				a_max = 0;
				scheduled_jobs.clear();
				num_jobs_scheduled = 0;
				ready_successor_jobs.clear();
				jobs_with_pending_successors.clear();
				earliest_pending_release = state_space_data.get_earliest_job_arrival();
				next_certain_successor_jobs_dispatch = Time_model::constants<Time>::infinity();
				next_certain_sequential_source_job_release = state_space_data.get_earliest_certain_seq_source_job_release();
				next_certain_gang_source_job_dispatch = Time_model::constants<Time>::infinity();
				next_certain_source_job_release = std::min(next_certain_sequential_source_job_release, state_space_data.get_earliest_certain_gang_source_job_release());
			}

			// transition: new node by scheduling a job 'j' in an existing node 'from'
			void reset(
				const Schedule_node& from,
				const Job<Time>& j,
				std::size_t idx,
				const State_space_data<Time>& state_space_data,
				const Time next_earliest_release,
				const Time next_certain_source_job_release, // the next time a job without predecessor is certainly released
				const Time next_certain_sequential_source_job_release // the next time a job without predecessor that can execute on a single core is certainly released
			)
			{
#ifdef CONFIG_PARALLEL
				tbb::spin_rw_mutex::scoped_lock lock(states_mutex, true); // write lock
#endif
				states.clear();
				scheduled_jobs.set(from.scheduled_jobs, idx);
				lookup_key = from.next_key(j);
				num_cpus = from.num_cpus;
				num_jobs_scheduled = from.num_jobs_scheduled + 1;
				finish_time = { 0, Time_model::constants<Time>::infinity() };
				a_max = Time_model::constants<Time>::infinity();
				earliest_pending_release = next_earliest_release;
				this->next_certain_source_job_release = next_certain_source_job_release;
				next_certain_successor_jobs_dispatch = Time_model::constants<Time>::infinity();
				this->next_certain_sequential_source_job_release = next_certain_sequential_source_job_release;
				next_certain_gang_source_job_dispatch = Time_model::constants<Time>::infinity();
				ready_successor_jobs.update(from.ready_successor_jobs, idx, state_space_data.inter_job_constraints, scheduled_jobs);
				jobs_with_pending_successors.update(from.jobs_with_pending_successors, idx, state_space_data.inter_job_constraints, this->scheduled_jobs);
			}

			const unsigned int number_of_scheduled_jobs() const
			{
				return num_jobs_scheduled;
			}

			Time earliest_job_release() const
			{
				return earliest_pending_release;
			}

			Time get_next_certain_source_job_release() const
			{
				return next_certain_source_job_release;
			}

			Time get_next_certain_sequential_source_job_release() const
			{
				return next_certain_sequential_source_job_release;
			}

			Time next_certain_job_ready_time() const
			{
				return std::min(next_certain_successor_jobs_dispatch,
					std::min(next_certain_sequential_source_job_release,
						next_certain_gang_source_job_dispatch));
			}

			const std::vector<const Job<Time>*>& get_ready_successor_jobs() const
			{
				return ready_successor_jobs.get_ready_successors();
			}

			const std::vector<Job_index>& get_jobs_with_pending_start_successors() const
			{
				return jobs_with_pending_successors.get_jobs_with_pending_start_successors();
			}

			const std::vector<Job_index>& get_jobs_with_pending_finish_successors() const
			{
				return jobs_with_pending_successors.get_jobs_with_pending_finish_successors();
			}

			hash_value_t get_key() const
			{
				return lookup_key;
			}

			const Job_set& get_scheduled_jobs() const
			{
				return scheduled_jobs;
			}

			const bool job_not_dispatched(Job_index j) const
			{
				return !scheduled_jobs.contains(j);
			}

			const bool job_dispatched(Job_index j) const
			{
				return scheduled_jobs.contains(j);
			}

			bool is_ready(const Job_index j, const Inter_job_constraints& constraints) const
			{
				for (const auto& pred : constraints.predecessors_start_to_start)
				{
					if (!scheduled_jobs.contains(pred.reference_job->get_job_index()))
						return false;
				}
				for (const auto& pred : constraints.predecessors_finish_to_start)
				{
					if (!scheduled_jobs.contains(pred.reference_job->get_job_index()))
						return false;
				}
				return true;
			}

			bool matches(const Schedule_node& other) const
			{
				return lookup_key == other.lookup_key &&
					scheduled_jobs == other.scheduled_jobs;
			}

			hash_value_t next_key(const Job<Time>& j) const
			{
				return get_key() ^ j.get_key();
			}

			//  finish_range / finish_time contains information about the
			//     earliest and latest core availability for core 0.
			//     whenever a state is changed (through merge) or added,
			//     that interval should be adjusted.
			Interval<Time> finish_range() const
			{
				return finish_time;
			}

			Time latest_core_availability() const
			{
				return a_max;
			}


			void add_state(State_ref s)
			{
#ifdef CONFIG_PARALLEL
				tbb::spin_rw_mutex::scoped_lock lock(states_mutex, true); // write lock
#endif
				update_internal_variables(s);
				states.insert(s);
			}

			// export the node information to the stream
			void export_node(std::ostream& stream, const Workload& jobs)
			{
				stream << "=====Node=====\n"
					<< "Ready successors: [[<task_id>,<job_id>], ...]\n"
					<< "[";
                // Ready successor jobs: [<task_id>,<job_id>]
				unsigned int i = 0;
				const auto& ready_successors = ready_successor_jobs.get_ready_successors();
				const auto num_ready_succ = ready_successors.size();
                for (const auto* job : ready_successors) {
                    stream << "[" << job->get_task_id() << "," << job->get_job_id() << "]";
					++i;
					if (i < num_ready_succ) 
						stream << ",";
                }
				stream << "]\n"
					<< "Scheduled jobs: [[<task_id>,<job_id>], ...]\n"
					<< "[";
                // Scheduled jobs: <task_id>,<job_id>\n
                // We need to iterate over n.scheduled_jobs.
				i = 0;
				for (int idx = 0; idx < scheduled_jobs.size(); ++idx) {
					if (scheduled_jobs.contains(idx)) {
						const auto& j = jobs[idx];
						stream << "[" << j.get_task_id() << "," << j.get_job_id() << "]";
						i++;
						if (i < num_jobs_scheduled) 
							stream << ",";
					}
                }
				stream << "]\n";
            }

			//return the number of states in the node
			int states_size() const
			{
#ifdef CONFIG_PARALLEL
				tbb::spin_rw_mutex::scoped_lock lock(states_mutex, false); // read lock
#endif
				return states.size();
			}

			const State_ref get_first_state() const
			{
#ifdef CONFIG_PARALLEL
				tbb::spin_rw_mutex::scoped_lock lock(states_mutex, false); // read lock
#endif
				auto first = states.begin();
				return *first;
			}

			const State_ref get_last_state() const
			{
#ifdef CONFIG_PARALLEL
				tbb::spin_rw_mutex::scoped_lock lock(states_mutex, false); // read lock
#endif
				auto last = --(states.end());
				return *last;
			}

			const State_ref_queue* get_states() const
			{
#ifdef CONFIG_PARALLEL
				// Note: This method returns a pointer to the internal container,
				// which is inherently not thread-safe. The caller must ensure
				// proper synchronization when accessing the returned pointer.
#endif
				return &states;
			}

			// try to merge state 's' with up to 'budget' states already recorded in this node. 
			// The option 'conservative' allow a merge of two states to happen only if the availability 
			// this_intervals of one state are constained in the availability this_intervals of the other state. If
			// the conservative option is used, the budget parameter is ignored.
			// The option 'use_job_finish_times' controls whether or not the job finish time this_intervals of jobs 
			// with pending successors must overlap to allow two states to merge. Setting it to true should 
			// increase accurracy of the analysis but increases runtime significantly.
			// The 'budget' defines how many states can be merged at once. If 'budget = -1', then there is no limit. 
			// Returns the number of existing states the new state was merged with.
			int merge_states(const Schedule_state<Time>& s, bool conservative, bool use_job_finish_times = false, int budget = 1)
			{
#ifdef CONFIG_PARALLEL
				tbb::spin_rw_mutex::scoped_lock lock(states_mutex, true); // write lock
#endif
				// if we do not use a conservative merge, try to merge with up to 'budget' states if possible.
				int merge_budget = conservative ? 1 : budget;

				State_ref last_state_merged;
				bool result = false;
				for (auto it = states.begin(); it != states.end();)
				{
					State_ref state = *it;
					if (result == false)
					{
						if (state->try_to_merge(s, conservative, use_job_finish_times))
						{
							// Update the node finish_time
							finish_time.widen(s.core_availability());
							a_max = std::max(a_max, s.core_availability(num_cpus).max());
							//update the certain next job ready time
							next_certain_successor_jobs_dispatch = std::max(next_certain_successor_jobs_dispatch, s.next_certain_successor_jobs_dispatch());
							next_certain_gang_source_job_dispatch = std::max(next_certain_gang_source_job_dispatch, s.next_certain_gang_source_job_dispatch());

							result = true;

							// Try to merge with a few more states.
							merge_budget--;
							if (merge_budget == 0)
								break;

							last_state_merged = state;
						}
						++it;
					}
					else // if we already merged with one state at least
					{
						if (last_state_merged->try_to_merge(*state, conservative, use_job_finish_times))
						{
							// the state was merged => we can thus remove the old one from the list of states
							it = states.erase(it);

							// Try to merge with a few more states.
							// std::cerr << "Merged with " << merge_budget << " of " << states.size() << " states left.\n";
							merge_budget--;
							if (merge_budget == 0)
								break;
						}
						else
							++it;
					}
				}

				if (conservative)
					return result ? 1 : 0;
				else
					return (budget - merge_budget);
			}

		private:
			void update_internal_variables(const State_ref& s)
			{
				Interval<Time> ft = s->core_availability();
				if (states.empty()) {
					finish_time = ft;
					a_max = s->core_availability(num_cpus).max();
					next_certain_successor_jobs_dispatch = s->next_certain_successor_jobs_dispatch();
					next_certain_gang_source_job_dispatch = s->next_certain_gang_source_job_dispatch();
				}
				else {
					finish_time.widen(ft);
					a_max = std::max(a_max, s->core_availability(num_cpus).max());
					next_certain_successor_jobs_dispatch = std::max(next_certain_successor_jobs_dispatch, s->next_certain_successor_jobs_dispatch());
					next_certain_gang_source_job_dispatch = std::max(next_certain_gang_source_job_dispatch, s->next_certain_gang_source_job_dispatch());
				}
			}
		};

	}
}

#endif