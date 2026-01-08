#ifndef GLOBAL_START_TIMES_TRACKER_HPP
#define GLOBAL_START_TIMES_TRACKER_HPP

#include "global/trackers/job_timing_tracker.hpp"
#include "inter_job_constraints.hpp"
namespace NP {
namespace Global {
/**
 * @brief Tracks job start times for jobs with pending successors.
 * 
 * This class extends Job_timing_tracker to specifically manage
 * start times of jobs. It provides methods to merge timing
 * information and check for overlaps during state merging.
 */
template<class Time>
class Start_times_tracker : public Job_timing_tracker<Time>
{
public:
    Start_times_tracker() = default;

    /**
	 * @brief Update start times after dispatching a job.
	 * 
	 * This method updates the start time tracking based on:
	 * - The previous tracker state
	 * - The newly dispatched job (if it has pending successors)
	 * - Jobs whose successors are still pending
	 * 
	 * @param from Previous tracker state
	 * @param dispatched_job The job being dispatched
	 * @param start_time_interval When the job starts
	 * @param jobs_with_pending_succ Jobs that still have pending successors
	 * @param constraints Precedence and mutual exclusion constraints of the dispatched job
	 */
	void update(
		const Start_times_tracker& from,
		Job_index dispatched_job,
		const Interval<Time>& start_time_interval,
		const std::vector<Job_index>& jobs_with_pending_succ,
		const Job_constraints<Time>& constraints)
	{
		Job_timing_tracker<Time>::job_times.clear();
		Job_timing_tracker<Time>::job_times.reserve(jobs_with_pending_succ.size());

		auto it = from.job_times.begin();
		for (Job_index job : jobs_with_pending_succ) {
			if (job == dispatched_job) {
				Job_timing_tracker<Time>::job_times.emplace_back(job, start_time_interval);
			} else {
				// Find this job in the previous tracker
				// Note that with conditional DAGs, it is *NOT TRUE* anymore that if `job` has non-completed successors in the new state,
                // it must have had non-completed successors in the previous state too.
                while (it != from.job_times.end() && it->job_idx != job) {
					++it;
				}
				if (it == from.job_times.end() || it->job_idx > job)
					continue;
				
				// ST^min(job,v') = ST^min(job,v)
				// ST^max(job,v') = 
				// \begin{cases}
				//	 min( ST^max(job,v), LST(j,v) - delay_min(job,j) ) & job \in Pred^s(j) U Mutx^s(j) \\
				//	 ST^max(job,v) & \text{otherwise}
				// \end{cases}

				// if job was already dispatched and job must start at least delay_min time units before the newly dispatched job starts,
				// then job should certainly have started delay_min time units before the newly dispatched job certainly starts
				Time min_delay = constraints.get_min_delay_after_start_of(job);
				if (min_delay >= 0) {
					Time job_est = it->time_interval.min();
					Time job_lst = std::min(it->time_interval.max(), start_time_interval.max() - min_delay);
					Job_timing_tracker<Time>::job_times.emplace_back(job, Interval<Time>{job_est, job_lst});
				} else {
					// no constraint on start time, keep previous start time interval
					Job_timing_tracker<Time>::job_times.push_back(*it);
				}
			}
		}
	}
};

} // namespace Global
} // namespace NP
#endif // GLOBAL_START_TIMES_TRACKER_HPP