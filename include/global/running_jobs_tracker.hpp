#ifndef GLOBAL_RUNNING_JOBS_TRACKER_HPP
#define GLOBAL_RUNNING_JOBS_TRACKER_HPP

#include <algorithm>
#include <vector>
#include "interval.hpp"
#include "jobs.hpp"

namespace NP {
namespace Global {

/**
 * @brief Tracks jobs that are certainly running in the current system state.
 * 
 * This class maintains information about jobs that are known to be executing,
 * including their parallelism level and expected finish times. This information
 * is used for state merging and interference analysis.
 * 
 * Invariants:
 * - The running jobs vector is sorted by job index
 * - Each job appears at most once
 * - Only "certainly running" jobs are tracked (conservative estimate)
 */
template<class Time>
class Running_jobs_tracker
{
public:
	typedef Interval<unsigned int> Parallelism;
	
	struct Running_job {
		Job_index idx;
		Parallelism parallelism;
		Interval<Time> finish_time;
		
		Running_job(
			Job_index idx,
			Parallelism parallelism,
			Interval<Time> finish_time)
			: idx(idx)
			, parallelism(parallelism)
			, finish_time(finish_time)
		{}
	};
	
	typedef std::vector<Running_job> Running_jobs;
	
	/**
	 * @brief Default constructor creates an empty tracker.
	 */
	Running_jobs_tracker() = default;
	
	/**
	 * @brief Get all certainly running jobs.
	 */
	const Running_jobs& get_running_jobs() const
	{
		return running_jobs;
	}
	
	/**
	 * @brief Update running jobs after dispatching a new job.
	 * 
	 * This method:
	 * 1. Filters out jobs from previous state that are certainly finished
	 * 2. Adds the newly dispatched job
	 * 3. Keeps the vector sorted by job index
	 * 4. Counts core used by predecessors that were running in the previous state
	 * 
	 * @param from Previous tracker state
	 * @param dispatched_job The job being dispatched
	 * @param start_time_interval When the job starts
	 * @param finish_time_interval When the job finishes
	 * @param ncores_used Number of cores the job uses
	 * @param predecessors List of predecessor job indices
	 * @return Number of cores used by predecessors that were running in `from`
	 */
	int update_and_count_predecessors(
		const Running_jobs_tracker& from,
		Job_index dispatched_job,
		const Interval<Time>& start_time_interval,
		const Interval<Time>& finish_time_interval,
		unsigned int ncores_used,
		const std::vector<Job_index>& predecessors)
	{
		running_jobs.clear();
		running_jobs.reserve(from.running_jobs.size() + 1);
		
		const Time lst = start_time_interval.max();
		int predecessor_cores = 0;
		// update the set of certainly running jobs
		// keep them sorted to simplify merging
		bool added_job = false;		
		for (const auto& rj : from.running_jobs) {
			// Check if this is a predecessor
			auto running_job = rj.idx;
			bool is_predecessor = contains(predecessors, running_job);
			if (is_predecessor) {
				predecessor_cores += rj.parallelism.min();
			}
			else {
				// Not a predecessor - keep it if not finished
				if (lst < rj.finish_time.min()) {
					// Add new job in sorted order
					if (!added_job && running_job > dispatched_job) {
						running_jobs.emplace_back(dispatched_job, Parallelism{ncores_used, ncores_used}, finish_time_interval);
						added_job = true;
					}
					running_jobs.push_back(rj);
				}
			}
		}
		
		// Add new job at end if not yet added
		if (!added_job) {
			running_jobs.emplace_back(dispatched_job, Parallelism{ncores_used, ncores_used}, finish_time_interval);
		}
		
		return predecessor_cores;
	}
	
	/**
	 * @brief Merge with running jobs from another tracker.
	 * 
	 * Only jobs that are running in BOTH trackers are kept in the result
	 * (intersection). Their parallelism and finish time intervals are widened.
	 * 
	 * @param other The other tracker to merge with
	 */
	void merge(const Running_jobs_tracker& other)
	{
		Running_jobs merged;
		merged.reserve(std::min(running_jobs.size(), other.running_jobs.size()));
		
		auto this_it = running_jobs.begin();
		auto other_it = other.running_jobs.begin();
		
		while (this_it != running_jobs.end() && other_it != other.running_jobs.end()) {
			if (this_it->idx == other_it->idx) {
				// Same job running in both - merge information
				merged.emplace_back(
					this_it->idx,
					this_it->parallelism | other_it->parallelism,
					this_it->finish_time | other_it->finish_time);
				++this_it;
				++other_it;
			} else if (this_it->idx < other_it->idx) {
				// Job in this but not other - skip
				++this_it;
			} else {
				// Job in other but not this - skip
				++other_it;
			}
		}		
		running_jobs.swap(merged);
	}
	
	/**
	 * @brief Check if a specific job is certainly running.
	 * 
	 * @param job_idx The job to check
	 * @return true if the job is in the running jobs list
	 */
	bool is_running(Job_index job_idx) const
	{
		for (const auto& rj : running_jobs) {
			if (rj.idx == job_idx) {
				return true;
			}
			if (rj.idx > job_idx) {
				break; // List is sorted
			}
		}
		return false;
	}
	
	/**
	 * @brief Get the finish time of a running job.
	 * 
	 * @param job_idx The job to query
	 * @param[out] finish_time The finish time interval if found
	 * @return true if the job is running, false otherwise
	 */
	bool get_finish_time(Job_index job_idx, Interval<Time>& finish_time) const
	{
		for (const auto& rj : running_jobs) {
			if (rj.idx == job_idx) {
				finish_time = rj.finish_time;
				return true;
			}
			if (rj.idx > job_idx) {
				break;
			}
		}
		return false;
	}
	
	/**
	 * @brief Clear all running job information.
	 */
	void clear()
	{
		running_jobs.clear();
	}
	
	/**
	 * @brief Get the total number of cores certainly used.
	 * 
	 * @return Sum of minimum parallelism of all running jobs
	 */
	unsigned int total_cores_used() const
	{
		unsigned int total = 0;
		for (const auto& rj : running_jobs) {
			total += rj.parallelism.min();
		}
		return total;
	}

private:
	Running_jobs running_jobs;
	
	/**
	 * @brief Helper to check if a job index is in a vector.
	 */
	bool contains(const std::vector<Job_index>& vec, Job_index idx) const
	{
		return std::find(vec.begin(), vec.end(), idx) != vec.end();
	}
};

} // namespace Global
} // namespace NP

#endif // GLOBAL_RUNNING_JOBS_TRACKER_HPP
