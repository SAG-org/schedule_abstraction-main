#ifndef GLOBAL_JOB_TIMING_TRACKER_HPP
#define GLOBAL_JOB_TIMING_TRACKER_HPP

#include <algorithm>
#include <vector>
#include "interval.hpp"
#include "jobs.hpp"

namespace NP {
namespace Global {

/**
 * @brief Tracks start and finish times for jobs with pending successors.
 * 
 * This class maintains sorted vectors of job timing information, allowing
 * efficient lookup and update of start/finish times. It's used to track
 * the timing of jobs that have successors that haven't been dispatched yet,
 * which is necessary for evaluating precedence constraints.
 * 
 * Invariants:
 * - The job timing vectors are always sorted by job index
 * - Each job appears at most once in each vector
 * - Only jobs with pending successors are tracked
 */
template<class Time>
class Job_timing_tracker
{
public:
	struct Job_time_entry {
		Job_index job_idx;
		Interval<Time> time_interval;
		
		Job_time_entry(Job_index idx, Interval<Time> interval)
			: job_idx(idx), time_interval(interval)
		{}
	};
	
	typedef std::vector<Job_time_entry> Job_times;
	
	/**
	 * @brief Default constructor creates an empty tracker.
	 */
	Job_timing_tracker() = default;
	
	/**
	 * @brief Get all job time entries.
	 */
	const Job_times& get() const
	{
		return job_times;
	}
	
	/**
	 * @brief Look up the time interval for a specific job.
	 * 
	 * @param job_idx The job to look up
	 * @param[out] result The time interval if found
	 * @return true if the job's time is tracked, false otherwise
	 */
	bool get(Job_index job_idx, Interval<Time>& result) const
	{
		int offset = find(job_idx);
		if (offset < job_times.size() && job_times[offset].job_idx == job_idx) {
			result = job_times[offset].time_interval;
			return true;
		}
		result = Interval<Time>{0, Time_model::constants<Time>::infinity()};
		return false;
	}
	
	/**
	 * @brief Merge timing information from another tracker.
	 * 
	 * Widens each tracked job's time interval to include the other's.
	 * Used when merging states.
	 * 
	 * @param other The other tracker to merge with
	 */
	void merge(const Job_timing_tracker& other)
	{
		widen_intervals(job_times, other.job_times);
	}
	
	/**
	 * @brief Check if jobs timings overlap with another tracker.
	 * 
	 * @param other The other tracker to compare with
	 * @param conservative If true, requires containment rather than overlap
	 * @param other_in_this True if other is contained in this
	 * @return true if intervals overlap/contain as required
	 */
	bool overlap_with(
		const Job_timing_tracker& other,
		bool conservative,
		bool other_in_this) const
	{
		return check_times_overlap(job_times, other.job_times,
		                           conservative, other_in_this);
	}
	
	/**
	 * @brief Clear all tracked timing information.
	 */
	void clear()
	{
		job_times.clear();
	}

protected:
	Job_times job_times;

private:
	
	/**
	 * @brief Binary search for job index in job times.
	 * 
	 * @return The offset where the job is or should be inserted
	 */
	int find(Job_index job_idx) const
	{
		int start = 0;
		int end = job_times.size();
		while (start < end) {
			int mid = (start + end) / 2;
			if (job_times[mid].job_idx == job_idx) {
				return mid;
			} else if (job_times[mid].job_idx < job_idx) {
				start = mid + 1;
			} else {
				end = mid;
			}
		}
		return start;
	}
	
	/**
	 * @brief Check if two time vectors overlap.
	 */
	bool check_times_overlap(
		const Job_times& this_times,
		const Job_times& other_times,
		bool conservative,
		bool other_in_this) const
	{
		auto this_it = this_times.begin();
		auto other_it = other_times.begin();
		
		while (this_it != this_times.end() && other_it != other_times.end()) {
			if (this_it->job_idx == other_it->job_idx) {
				// Check if intervals overlap/contain as required
				if (conservative) {
					if (other_in_this) {
						if (!this_it->time_interval.contains(other_it->time_interval)) {
							return false;
						}
					} else {
						if (!other_it->time_interval.contains(this_it->time_interval)) {
							return false;
						}
					}
				} else {
					if (!this_it->time_interval.intersects(other_it->time_interval)) {
						return false;
					}
				}
				++this_it;
				++other_it;
			} else if (conservative) {
				// Conservative mode requires matching job lists
				return false;
			} else if (this_it->job_idx < other_it->job_idx) {
				++this_it;
			} else {
				++other_it;
			}
		}
		return true;
	}
	
	/**
	 * @brief Widen the target job timing intervals by merging with the other intervals.
	 */
	void widen_intervals(Job_times& target, const Job_times& other)
	{
		auto target_it = target.begin();
		auto source_it = other.begin();
		
		while (target_it != target.end() && source_it != other.end()) {
			if (target_it->job_idx == source_it->job_idx) {
				target_it->time_interval.widen(source_it->time_interval);
				++target_it;
				++source_it;
			} else if (target_it->job_idx < source_it->job_idx) {
				++target_it;
			} else {
				++source_it;
			}
		}
	}
};

} // namespace Global
} // namespace NP

#endif // GLOBAL_JOB_TIMING_TRACKER_HPP
