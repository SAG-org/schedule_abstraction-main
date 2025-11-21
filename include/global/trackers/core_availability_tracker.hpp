#ifndef GLOBAL_CORE_AVAILABILITY_TRACKER_HPP
#define GLOBAL_CORE_AVAILABILITY_TRACKER_HPP

#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>
#include "interval.hpp"
#include "jobs.hpp"

namespace NP {
namespace Global {

/**
 * @brief Manages core availability tracking and updates during state transitions.
 * 
 * This class encapsulates all logic related to tracking when processor cores
 * become available. It maintains a sorted list of availability intervals,
 * one per core, and provides efficient queries and updates.
 * 
 * Invariants:
 * - The availability vector is always sorted by increasing availability times
 * - The vector size equals the number of cores in the system
 * - Each interval represents when a specific core becomes available
 */
template<class Time>
class Core_availability_tracker
{
public:
	typedef std::vector<Interval<Time>> Core_availability;
	
	/**
	 * @brief Construct a manager for a system with num_cores processors,
	 *        all initially available at time 0.
	 */
	explicit Core_availability_tracker(unsigned int num_cores)
		: core_avail(num_cores, Interval<Time>(Time(0), Time(0)))
	{
		assert(num_cores > 0);
	}
	
	/**
	 * @brief Construct a manager with specific initial core availability.
	 * 
	 * @param initial_state Vector of availability intervals, one per core.
	 *                      The vector is sorted during construction.
	 */
	explicit Core_availability_tracker(const std::vector<Interval<Time>>& initial_state)
		: core_avail(initial_state.size())
	{
		assert(initial_state.size() > 0);
		initialize_from_intervals(initial_state);
	}
	
	/**
	 * @brief Get the number of cores being managed.
	 */
	unsigned int num_cores() const
	{
		return (unsigned int) core_avail.size();
	}
	
	/**
	 * @brief Get read-only access to all core availability intervals.
	 * 
	 * The intervals are sorted such that core_avail[0] is the first core
	 * to become available, core_avail[1] is the second, etc.
	 */
	const Core_availability& get_all_intervals() const
	{
		return core_avail;
	}
	
	/**
	 * @brief Get the availability interval for the p-th available core.
	 * 
	 * @param p The parallelism level (1-indexed). p=1 means first available core,
	 *          p=2 means second available core, etc.
	 * @return Interval representing when the p-th core becomes available.
	 * 
	 * @pre 1 <= p <= num_cores()
	 */
	Interval<Time> get_availability(unsigned int p = 1) const
	{
		assert(p > 0 && p <= core_avail.size());
		return core_avail[p - 1];
	}
	
	/**
	 * @brief Get the earliest time when the first core becomes available.
	 */
	Time earliest_availability() const
	{
		return core_avail[0].min();
	}
	
	/**
	 * @brief Get the latest time when the last core becomes available.
	 */
	Time latest_all_cores_available() const
	{
		return core_avail.back().max();
	}
	
	/**
	 * @brief Update core availability after dispatching a job.
	 * 
	 * This method updates the core availability based on dispatching job j
	 * on ncores_used cores. It accounts for:
	 * - Cores used by finished predecessors
	 * - Job's execution time
	 * - Earliest and latest start/finish times
	 * 
	 * @param from Previous core availability state
	 * @param ncores_freed Number of cores used by finished predecessors
	 * @param start_times When the job can start
	 * @param finish_times When the job will finish
	 * @param ncores_used Number of cores the job uses
	 */
	void update(
		const Core_availability_tracker& from,
		const Interval<Time>& start_times,
		const Interval<Time>& finish_times,
		unsigned int ncores_used,
		unsigned int ncores_freed)
	{
		const unsigned int n_cores = (unsigned int) from.core_avail.size();
		core_avail.clear();
		core_avail.reserve(n_cores);
		
		const Time est = start_times.min();
		const Time lst = start_times.max();
		const Time eft = finish_times.min();
		const Time lft = finish_times.max();
		
		// Stack allocation for small core counts, heap for larger
		constexpr int STACK_THRESHOLD = 64;
		Time ca_stack[STACK_THRESHOLD];
		Time pa_stack[STACK_THRESHOLD];
		
		Time* ca;  // certainly available times
		Time* pa;  // possibly available times
		std::unique_ptr<Time[]> ca_heap;
		std::unique_ptr<Time[]> pa_heap;
		
		// use stack allocation for small arrays, heap allocation for large ones
		if (n_cores <= STACK_THRESHOLD) {
			ca = ca_stack;
			pa = pa_stack;
		} else {
			ca_heap = std::make_unique<Time[]>(n_cores);
			pa_heap = std::make_unique<Time[]>(n_cores);
			ca = ca_heap.get();
			pa = pa_heap.get();
		}
		
		unsigned int ca_idx = 0, pa_idx = 0;
		bool eft_added_to_pa = false;
		bool lft_added_to_ca = false;
		
		// if there are more freed cores than cores used by the dispatched job, the additional cores must be available when the job starts
		if (ncores_freed > ncores_used) {
			for (unsigned int i = ncores_used; i < ncores_freed; i++) {
				pa[pa_idx++] = est;
				ca[ca_idx++] = std::min(lst, std::max(est, from.core_avail[i].max()));
			}
		}
		
		// Process remaining cores, inserting job finish times
		for (unsigned int i = std::max(ncores_freed, ncores_used); 
				i < from.core_avail.size(); i++) 
		{
			// Add job's finish time to possibly-available list
			if (!eft_added_to_pa && eft < from.core_avail[i].min()) {
				// Insert earliest finish time of the dispatched jobs `ncores_used` times since it occupies that many cores
				for (unsigned int p = 0; p < ncores_used; p++) {
					pa[pa_idx++] = eft;
				}
				eft_added_to_pa = true;
			}
			pa[pa_idx++] = std::max(est, from.core_avail[i].min());
			
			// Add job's finish time to certainly-available list
			if (!lft_added_to_ca && lft < from.core_avail[i].max()) {
				// Insert latest finish time of the dispatched jobs `ncores_used` times since it occupies that many cores
				for (unsigned int p = 0; p < ncores_used; p++) {
					ca[ca_idx++] = lft;
				}
				lft_added_to_ca = true;
			}
			ca[ca_idx++] = std::max(est, from.core_avail[i].max());
		}
		
		// Ensure job finish times are added if not yet
		if (!eft_added_to_pa) {
			for (unsigned int p = 0; p < ncores_used; p++) {
				pa[pa_idx++] = eft;
			}
		}
		if (!lft_added_to_ca) {
			for (unsigned int p = 0; p < ncores_used; p++) {
				ca[ca_idx++] = lft;
			}
		}
		
		// Construct final availability intervals
		for (unsigned int i = 0; i < n_cores; i++) {
			core_avail.emplace_back(pa[i], ca[i]);
		}
	}
	
	/**
	 * @brief Merge availability from another manager.
	 * 
	 * Widens each core's availability interval to include the other's.
	 * This is used when merging states.
	 * 
	 * @param other The other manager to merge with
	 * @pre Both managers have the same number of cores
	 */
	void merge(const Core_availability_tracker& other)
	{
		assert(core_avail.size() == other.core_avail.size());
		for (unsigned int i = 0; i < core_avail.size(); i++) {
			core_avail[i] |= other.core_avail[i];
		}
	}
	
	/**
	 * @brief Check if the core availabilities overlap with the core availabilities 
	 * tracked by another tracker.
	 * 
	 * @param other The other manager to compare with
	 * @param conservative If true, requires complete containment rather than overlap
	 * @param other_in_this Output parameter: true if other is contained in this
	 * @return true if the availabilities overlap/contain each other as required
	 */
	bool overlap_with(
		const Core_availability_tracker& other,
		bool conservative,
		bool& other_in_this) const
	{
		assert(core_avail.size() == other.core_avail.size());
		other_in_this = false;
		const auto& other_avail = other.core_avail;

		if (conservative) {
			// Check if other is contained in this
			bool contained = true;
			for (unsigned int i = 0; i < core_avail.size(); i++) {
				if (!core_avail[i].contains(other_avail[i])) {
					contained = false;
					break;
				}
			}
			if (contained) {
				other_in_this = true;
				return true;
			}
			
			// Check if this is contained in other
			for (unsigned int i = 0; i < core_avail.size(); i++) {
				if (!other_avail[i].contains(core_avail[i])) {
					return false;
				}
			}
			return true;
		} else {
			// Simple overlap check
			for (unsigned int i = 0; i < core_avail.size(); i++) {
				if (!core_avail[i].intersects(other_avail[i])) {
					return false;
				}
			}
			return true;
		}
	}
	
	/**
	 * @brief Reset to initial state with all cores available at time 0.
	 */
	void reset(unsigned int num_cores)
	{
		assert(num_cores > 0);
		core_avail.assign(num_cores, Interval<Time>(Time(0), Time(0)));
	}
	
	/**
	 * @brief Reset to specific initial availability.
	 */
	void reset(const std::vector<Interval<Time>>& initial_state)
	{
		assert(initial_state.size() > 0);
		core_avail.resize(initial_state.size());
		initialize_from_intervals(initial_state);
	}

private:
	Core_availability core_avail;
	
	/**
	 * @brief Helper to initialize from unsorted interval vector.
	 * 
	 * Extracts min and max times, sorts them, and constructs
	 * the availability intervals.
	 */
	void initialize_from_intervals(const std::vector<Interval<Time>>& intervals)
	{
		const size_t n = intervals.size();
		std::vector<Time> amin, amax;
		amin.reserve(n);
		amax.reserve(n);
		
		for (const auto& interval : intervals) {
			amin.push_back(interval.min());
			amax.push_back(interval.max());
		}
		
		std::sort(amin.begin(), amin.end());
		std::sort(amax.begin(), amax.end());
		
		for (size_t i = 0; i < n; i++) {
			core_avail[i] = Interval<Time>(amin[i], amax[i]);
		}
	}
};

} // namespace Global
} // namespace NP

#endif // GLOBAL_CORE_AVAILABILITY_TRACKER_HPP
