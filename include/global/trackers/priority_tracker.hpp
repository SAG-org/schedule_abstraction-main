#ifndef GLOBAL_PRIORITY_TRACKER_HPP
#define GLOBAL_PRIORITY_TRACKER_HPP

#include "jobs.hpp"
#include "global/state_space_data.hpp"
#include "inter_job_constraints.hpp"
#include "conditional_dispatch_constraints.hpp"
#include "global/state.hpp"
#include "index_set.hpp"

namespace NP {
namespace Global {

template<class Time> class State_space_data;
template<class Time> class Schedule_state;

/**
 * @brief Tracks the minimum priority job that can be dispatched next.
 * 
 * This class maintains information about the job with lowest priority
 * that is  dispatchable when the next core becomes available.
 * This is used for priority-based pruning during the analysis.
 */
template<class Time>
class Priority_tracker
{
public:
	typedef const Job<Time>* Job_ref;
	
	/**
	 * @brief Default constructor initializes with no job.
	 */
	Priority_tracker()
		: min_priority_job(nullptr)
	{
	}
	
	/**
	 * @brief Get the minimum priority job that can be dispatched next.
	 * 
	 * @return Pointer to job, or nullptr if no such job is known
	 */
	Job_ref get_min_priority_job() const
	{
		return min_priority_job;
	}
	
	/**
	 * @brief Check if a minimum priority job is known.
	 * 
	 * @return true if min priority job is set, false otherwise
	 */
	bool has_min_priority_job() const
	{
		return min_priority_job != nullptr;
	}
	
	/**
	 * @brief Merge priority information from another tracker.
	 * 
	 * When merging states, we keep the lower priority job (or null
	 * if either tracker has null). This ensures conservative analysis.
	 * 
	 * @param other The other tracker to merge with
	 */
	void merge(const Priority_tracker& other)
	{
		if (min_priority_job == nullptr || other.min_priority_job == nullptr) {
			// If either is unknown, result is unknown
			min_priority_job = nullptr;
		} else if (min_priority_job->higher_priority_than(*other.min_priority_job)) {
			// Keep the lower priority job
			min_priority_job = other.min_priority_job;
		}
	}
	
	/**
	 * @brief Reset to no known job.
	 */
	void reset()
	{
		min_priority_job = nullptr;
	}

	/**
	 * @brief Update the tracked minimum-priority next-dispatchable job.
	 *
	 * This method inspects the provided ready-successor job list and the
	 * scheduling constraints to select the job that will be the lowest-
	 * priority candidate guaranteed to be ready when a core becomes free.
     * 
     * @param state The current schedule state
     * @param constraints The inter-job constraints in the system
	 * @param cond_constr The conditional dispatch constraints (set of incompatible jobs and conditional siblings)
     * @param ready_succ_jobs The list of ready-successor jobs sorted by priority
     * @param scheduled_jobs The set of jobs that have already been scheduled
	 * @param num_cores The number of cores in the system.
	 */
	void update(const Schedule_state<Time>& state,
				const Inter_job_constraints<Time>& constraints,
				const Conditional_dispatch_constraints<Time>& cond_constr,
				const std::vector<const Job<Time>*>& ready_succ_jobs,
				const Index_set& scheduled_jobs,
				unsigned int num_cores)
	{
		min_priority_job = nullptr;
		std::vector<const Job<Time>*> predecessors;
		predecessors.reserve(4 * num_cores);
		// we must find num_cores jobs that will be ready as soon as their predecessors complete.
		unsigned int c = num_cores;
		unsigned int rem_rjobs = (unsigned int) ready_succ_jobs.size();

		for (auto j : ready_succ_jobs) {
			// skip gang jobs that require more than one core
			// TODO: this is pessimistic but safe. We should check if we can handle gang jobs more accurately.
			if (j->get_min_parallelism() > 1) {
				rem_rjobs--;
				continue;
			}
			// skip jobs with conditional siblings
			// TODO: this is very pessimistic but safe. Handling conditional siblings here would significatly improve performances
			if (cond_constr.has_conditional_siblings(j->get_job_index())){
				rem_rjobs--;
				continue;
			}
			// if the job is certainly ready, we know it will be ready to dispatch when the first core becomes free
			if (state.certainly_ready(j, constraints, scheduled_jobs)) {
				min_priority_job = j;
				// since ready_succ_jobs is sorted in decreasing priority order,
				// we found the highest priority ready job, no need to continue
				return;
			}
			else if (rem_rjobs >= c) {
				// check if it has predecessors that are also predecessors of another ready job
                // and if j certainly arrives before all its predecessors finish 
                const auto& pred_j = constraints[j->get_job_index()].predecessors_finish_to_start;
				bool overlap = false;
				bool arrives_before_finish = false;
				bool suspending = false;
				for (const auto& p : pred_j) {
					if (p.delay.max() > 0) {
						suspending = true;
						break;
					}
					auto jp = p.reference_job;
					if (std::find(predecessors.begin(), predecessors.end(), jp) != predecessors.end()) {
						overlap = true;
						break;
					}
					if (!arrives_before_finish) {
						Interval<Time> ftimes(0, 0);
						if (state.get_finish_times(jp->get_job_index(), ftimes) && ftimes.min() >= j->latest_arrival()) {
							// if the predecessor is not finished before j arrives, there is no delay between jp finish time and j's ready time
							arrives_before_finish = true;
						}
					}
				}
				if (!overlap && !suspending) {
					const auto& s_pred_j = constraints[j->get_job_index()].predecessors_start_to_start;
					for (const auto& p : s_pred_j) {
						if (p.delay.max() > 0) {
							suspending = true;
							break;
						}
						auto jp = p.reference_job;
						if (std::find(predecessors.begin(), predecessors.end(), jp) != predecessors.end()) {
							overlap = true;
							break;
						}
						if (!arrives_before_finish) {
							Interval<Time> stimes(0, 0);
							if (state.get_start_times(jp->get_job_index(), stimes) && stimes.min() >= j->latest_arrival()) {
								// if the predecessor is not started before j arrives, there is no delay between jp start time and j's ready time
								arrives_before_finish = true;
							}
						}
					}
				}
				if (!overlap && !suspending) {
					const auto& between_starts = constraints[j->get_job_index()].between_starts;
					for (const auto& p : between_starts) {
						if (!scheduled_jobs.contains(p.reference_job->get_job_index()))
							continue;
						if (p.delay.max() > 0) {
							suspending = true;
							break;
						}
						auto jp = p.reference_job;
						if (std::find(predecessors.begin(), predecessors.end(), jp) != predecessors.end()) {
							overlap = true;
							break;
						}
						if (!arrives_before_finish) {
							Interval<Time> stimes(0, 0);
							if (state.get_start_times(jp->get_job_index(), stimes) && stimes.min() >= j->latest_arrival()) {
								// if the predecessor is not started before j arrives, there is no delay between jp start time and j's ready time
								arrives_before_finish = true;
							}
						}
					}
				}
				if (!overlap && !suspending) {
					const auto& between_executions = constraints[j->get_job_index()].between_executions;
					for (const auto& p : between_executions) {
						if (!scheduled_jobs.contains(p.reference_job->get_job_index()))
							continue;
						if (p.delay.max() > 0) {
							suspending = true;
							break;
						}
						auto jp = p.reference_job;
						if (std::find(predecessors.begin(), predecessors.end(), jp) != predecessors.end()) {
							overlap = true;
							break;
						}
						if (!arrives_before_finish) {
							Interval<Time> ftimes(0, 0);
							if (state.get_finish_times(jp->get_job_index(), ftimes) && ftimes.min() >= j->latest_arrival()) {
								// if the predecessor is not finished before j arrives, there is no delay between jp finish time and j's ready time
								arrives_before_finish = true;
							}
						}
					}
				}

				if (!overlap && !suspending && arrives_before_finish) {
					c--;
					// record the predecessors of j if they have more than one successor
					for (const auto& p : pred_j) {
						auto jp = p.reference_job;
						if (constraints[jp->get_job_index()].start_to_successors_start.size() + constraints[jp->get_job_index()].finish_to_successors_start.size() > 1)
							predecessors.push_back(jp);
					}
					// if we already have num_cpus job that will certainly be ready when a core becomes free, 
                    // then we know one of those job will be ready when the first job becoes free. 
                    // In the worst-case, it is the lowest priority job.
                    if (c == 0) {
						min_priority_job = j;
						// since ready_succ_jobs is sorted in decreasing priority order, 
						// we found our job, no need to continue
						return;
					}
				}
			}
			rem_rjobs--;
		}
	}

private:
	// The job with minimum priority that will certainly be ready
	// when the next core becomes available
	Job_ref min_priority_job;
};

} // namespace Global
} // namespace NP

#endif // GLOBAL_PRIORITY_TRACKER_HPP
