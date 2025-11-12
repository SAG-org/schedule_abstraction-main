#ifndef GLOBAL_PENDING_SUCCESSORS_TRACKER_HPP
#define GLOBAL_PENDING_SUCCESSORS_TRACKER_HPP

#include <algorithm>
#include <vector>
#include "jobs.hpp"
#include "index_set.hpp"
#include "global/inter_job_constraints.hpp"

namespace NP {
namespace Global {

/**
 * @brief Tracks dispatched jobs that have unscheduled successors (based on start-to-start or finish-to-start precedence and mutual exclusion constraints).
 *
 * This class holds two lists:
 * - jobs_with_pending_start_succ: jobs that have at least one unscheduled successor
 *   with a start-to-start constraint
 * - jobs_with_pending_finish_succ: jobs that have at least one unscheduled successor
 *   with a finish-to-start constraint
 */
template<class Time>
class Pending_successors_tracker
{
public:
    typedef Index_set Job_set;
    typedef std::vector<Job_index> Pending_jobs_list;
    typedef enum {start_constraints, finish_constraints} Constraint_type;

    /**
	 * @brief Default constructor creates empty lists.
	 */
    Pending_successors_tracker() = default;

    /**
     * @brief Get jobs with pending start successors.
     *
     * @return Vector of job indices
     */
    const Pending_jobs_list& get_jobs_with_pending_start_successors() const
    {
        return jobs_with_pending_start_succ;
    }

    /**
     * @brief Get jobs with pending finish successors.
     *
     * @return Vector of job indices
     */
    const Pending_jobs_list& get_jobs_with_pending_finish_successors() const
    {
        return jobs_with_pending_finish_succ;
    }

    /**
     * @brief Update jobs with pending successors after dispatching a job.
     *
     * @tparam Constraints Type providing inter-job constraint information
     * @param from Previous tracker state
     * @param dispatched_job Index of job being dispatched
     * @param constraints Inter-job constraints
     * @param scheduled_jobs Set of all dispatched jobs
     */
    void update(
        const Pending_successors_tracker& from,
        Job_index dispatched_job,
        const Inter_job_constraints<Time>& constraints,
        const Job_set& scheduled_jobs)
    {
        const auto& job_constraints = constraints[dispatched_job];

        // Update start successors
        jobs_with_pending_start_succ.clear();
        update_pending_list(
            from.jobs_with_pending_start_succ,
            dispatched_job,
            constraints,
            start_constraints,
            scheduled_jobs,
            jobs_with_pending_start_succ);

        // Update finish successors
        jobs_with_pending_finish_succ.clear();
        update_pending_list(
            from.jobs_with_pending_finish_succ,
            dispatched_job,
            constraints,
            finish_constraints,
            scheduled_jobs,
            jobs_with_pending_finish_succ);
    }

    void clear()
    {
        jobs_with_pending_start_succ.clear();
        jobs_with_pending_finish_succ.clear();
    }

private:
    // Jobs with at least one unscheduled successor with start-to-start constraint
    Pending_jobs_list jobs_with_pending_start_succ;

    // Jobs with at least one unscheduled successor with finish-to-start constraint
    Pending_jobs_list jobs_with_pending_finish_succ;

    /**
     * @brief Update a pending successors list after dispatching a job.
     *
     * @param old_pending Previous list of jobs with pending successors
     * @param dispatched_job Index of job being dispatched
     * @param constraints Inter-job constraints
     * @param constraint_type Type of constraints to consider (start_to_start or finish_to_start)
     * @param scheduled_jobs Set of all dispatched jobs
     * @param result Updated list of jobs with pending successors
     */
    void update_pending_list(
        const Pending_jobs_list& old_pending,
        Job_index dispatched_job,
        const Inter_job_constraints<Time>& constraints,
        Constraint_type constraint_type,
        const Job_set& scheduled_jobs,
        Pending_jobs_list& result)
    {
        result.reserve(old_pending.size() + 1);
        const auto& disp_job_constraints = constraints[dispatched_job];
        // We must add j to result only if it has successors of the type constraint_type 
        // or mutual exclusion constraints with jobs that were not yest dispatched
        bool added_j = constraint_type == start_constraints ? 
                        disp_job_constraints.start_to_successors_start.empty() : 
                        disp_job_constraints.finish_to_successors_start.empty();
        if (added_j) {
            const auto& mutual_exclusions = constraint_type == start_constraints ? 
                                        disp_job_constraints.between_starts : 
                                        disp_job_constraints.between_executions;
            for (const auto& excl : mutual_exclusions) {
                // if it has started then we account for the constraint
                if (!scheduled_jobs.contains(excl.reference_job->get_job_index()))
                {
                    added_j = false;
                    break;
                }
            }
        }

        // Copy over jobs from old list that still have pending successors
        for (Job_index job : old_pending) {
            // keep the list sorted
            if (!added_j && job > dispatched_job) {
                result.push_back(dispatched_job);
                added_j = true;
            }

            // Check if this job still has pending successors
            const auto& job_constraints = constraints[job];
            const auto& successors = constraint_type == start_constraints ?
                job_constraints.start_to_successors_start :
                job_constraints.finish_to_successors_start;

            bool successor_pending = false;
            for (const auto& succ : successors) {
                if (!scheduled_jobs.contains(succ.reference_job->get_job_index())) {
                    successor_pending = true;
                    break;
                }
            }
            if (successor_pending) {
                result.push_back(job);
            }
        }
        if (!added_j) {
            result.push_back(dispatched_job);
        }
    }
};

} // namespace Global
} // namespace NP

#endif // GLOBAL_PENDING_SUCCESSORS_TRACKER_HPP
