#ifndef GLOBAL_READY_JOBS_TRACKER_HPP
#define GLOBAL_READY_JOBS_TRACKER_HPP

#include <algorithm>
#include <vector>
#include "jobs.hpp"
#include "index_set.hpp"
#include "global/state_space_data.hpp"

namespace NP {
namespace Global {

template<class Time> class State_space_data;

/**
 * @brief Tracks ready successor jobs (all predecessors completed, not yet dispatched)
 * 
 * This list is maintained in sorted order for efficient merging and lookup.
 */
template<class Time>
class Ready_jobs_tracker
{
    typedef Index_set Job_set; 
    typedef const Job<Time>* Job_ref;
	typedef std::vector<Job_ref> Ready_jobs_list;
	using Inter_job_constraints = typename State_space_data<Time>::Inter_job_constraints;
	typedef std::vector<Inter_job_constraints> Constraints;

public:
	
	/**
	 * @brief Default constructor creates an empty list.
	 */
	Ready_jobs_tracker() = default;
	
	/**
	 * @brief Get the list of ready successor jobs.
	 * 
	 * @return Vector of job pointers sorted by priority (non-increasing)
	 */
	const Ready_jobs_list& get_ready_successors() const
	{
		return ready_successor_jobs;
	}
	
	/**
	 * @brief Update ready successors after dispatching a job.
	 * 
	 * Maintains priority-sorted order of ready jobs.
	 * 
	 * @param from Previous tracker state
	 * @param dispatched_job Index of job being dispatched
	 * @param constraints Inter-job constraints data
	 * @param scheduled_jobs Set of jobs that have been dispatched
     */
	void update(
		const Ready_jobs_tracker& from,
		Job_index dispatched_job,
		const Constraints& constraints,
        const Job_set& scheduled_jobs)
	{
		const auto& job_constraints = constraints[dispatched_job];
		
		ready_successor_jobs.clear();
		ready_successor_jobs.reserve(
			from.ready_successor_jobs.size() + 
			job_constraints.start_to_successors_start.size() +
			job_constraints.finish_to_successors_start.size());
		
		// update list of ready jobs by excluding dispatched job and 
        // adding the ready successors of the dispatced job
		// Both lists are sorted by priority (non-increasing)
		auto old_it = from.ready_successor_jobs.begin();
		auto old_end = from.ready_successor_jobs.end();
		
		// Skip the dispatched job if it was in the ready list
		while (old_it != old_end && (*old_it)->get_job_index() == dispatched_job) {
			++old_it;
		}
		
		auto start_succ_it = job_constraints.start_to_successors_start.begin();
		auto start_succ_end = job_constraints.start_to_successors_start.end();
		
		auto finish_succ_it = job_constraints.finish_to_successors_start.begin();
		auto finish_succ_end = job_constraints.finish_to_successors_start.end();
		
        auto get_next_ready_job = [&](auto& it, const auto it_end) {
            while (it != it_end)
            {
                if (is_ready(it->reference_job->get_job_index(), constraints, scheduled_jobs))
                    break;
                ++it;
            }
        };

        get_next_ready_job(start_succ_it, start_succ_end);
        get_next_ready_job(finish_succ_it, finish_succ_end);
		// Three-way merge: old ready jobs, start successors, finish successors
		while (old_it != old_end || start_succ_it != start_succ_end || 
		       finish_succ_it != finish_succ_end)
		{
			Job_ref candidate = nullptr;
			int source = 0; // 0=old, 1=start_succ, 2=finish_succ
			
			// Find highest priority among available candidates
			if (old_it != old_end) {
				candidate = *old_it;
			}			
			if (start_succ_it != start_succ_end) {
				Job_ref start_job = start_succ_it->reference_job;
				if (candidate == nullptr || 
                    start_job->higher_priority_than(*candidate)) {
                    candidate = start_job;
                    source = 1;
				}
			}			
			if (finish_succ_it != finish_succ_end) {
				Job_ref finish_job = finish_succ_it->reference_job;
				if (candidate == nullptr || 
                    finish_job->higher_priority_than(*candidate)) {
                    candidate = finish_job;
                    source = 2;
                }
			}
			
			// Add candidate and advance appropriate iterator
			ready_successor_jobs.push_back(candidate);
			if (source == 0) {
				++old_it;
				// Skip if it is the job we just dispatched
				while (old_it != old_end && (*old_it)->get_job_index() == dispatched_job) {
					++old_it;
				}
			} else if (source == 1) {
				++start_succ_it;
                get_next_ready_job(start_succ_it, start_succ_end);
			} else if (source == 2) {
				++finish_succ_it;
                get_next_ready_job(finish_succ_it, finish_succ_end);
			} else {
				break; // should never reach here
			}
		}
	}
	
	/**
	 * @brief Clear all tracked jobs.
	 */
	void clear()
	{
		ready_successor_jobs.clear();
	}

private:
	// Jobs that have all predecessors completed and are not dispatched yet
	// Sorted by priority (non-increasing)
	Ready_jobs_list ready_successor_jobs;

	/**
	 * @brief Check if a job is ready (all predecessors dispatched).
	 */
	bool is_ready(
		Job_index job_idx,
		const Constraints& constraints,
		const Job_set& scheduled_jobs) const
	{
		const auto& job_constraints = constraints[job_idx];
		
		// Check start-to-start predecessors
		for (const auto& pred : job_constraints.predecessors_start_to_start) {
			if (!scheduled_jobs.contains(pred.reference_job->get_job_index())) {
				return false;
			}
		}
		
		// Check finish-to-start predecessors
		for (const auto& pred : job_constraints.predecessors_finish_to_start) {
			if (!scheduled_jobs.contains(pred.reference_job->get_job_index())) {
				return false;
			}
		}
		
		return true;
	}
};

} // namespace Global
} // namespace NP

#endif // GLOBAL_READY_JOBS_TRACKER_HPP
