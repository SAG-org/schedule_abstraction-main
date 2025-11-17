#ifndef GLOBAL_RESOURCE_MONITOR_HPP
#define GLOBAL_RESOURCE_MONITOR_HPP

#include "clock.hpp"
#include "mem.hpp"

namespace NP {
namespace Global {

/**
 * @brief Monitors resource usage and enforces limits during exploration.
 * 
 * This class centralizes all resource limit checking, including:
 * - CPU time limits
 * - Memory usage limits  
 * - Exploration depth limits
 * 
 * It provides a clean interface for checking abort conditions and
 * separates resource management from exploration logic.
 */
class Resource_monitor
{
public:
	/**
	 * @brief Construct a resource monitor with specified limits.
	 * 
	 * @param max_cpu_time Maximum CPU time in seconds (0 = unlimited)
	 * @param max_memory Maximum memory in KiB (0 = unlimited)
	 * @param max_depth Maximum exploration depth (0 = unlimited)
	 */
	Resource_monitor(
		unsigned long long max_cpu_time = 0,
		unsigned long long max_memory = 0,
		unsigned long long max_depth = 0)
		: max_cpu_time(max_cpu_time)
		, max_memory(max_memory)
		, max_depth(max_depth)
		, timed_out(false)
		, out_of_memory(false)
		, depth_exceeded(false)
	{
	}
	
	/**
	 * @brief Start the CPU time clock.
	 */
	void start_timing()
	{
		cpu_clock.start();
	}
	
	/**
	 * @brief Stop the CPU time clock.
	 */
	void stop_timing()
	{
		cpu_clock.stop();
	}
	
	/**
	 * @brief Get elapsed CPU time in seconds.
	 */
	double get_cpu_time() const
	{
		return cpu_clock;
	}
	
	/**
	 * @brief Get current memory usage in KiB.
	 */
	long get_memory_usage() const
	{
		return memory_monitor;
	}
	
	/**
	 * @brief Check if CPU time limit has been exceeded.
	 * 
	 * @return true if timeout occurred, false otherwise
	 */
	bool check_timeout()
	{
		if (max_cpu_time > 0 && get_cpu_time() > max_cpu_time) {
			timed_out = true;
			return true;
		}
		return false;
	}
	
	/**
	 * @brief Check if memory limit has been exceeded.
	 * 
	 * @return true if out of memory, false otherwise
	 */
	bool check_out_of_memory()
	{
		if (max_memory > 0 && get_memory_usage() > max_memory) {
			out_of_memory = true;
			return true;
		}
		return false;
	}
	
	/**
	 * @brief Check if depth limit has been exceeded.
	 * 
	 * @param current_depth Current exploration depth
	 * @return true if depth exceeded, false otherwise
	 */
	bool check_depth(unsigned long long current_depth)
	{
		if (max_depth > 0 && current_depth > max_depth) {
			depth_exceeded = true;
			return true;
		}
		return false;
	}
	
	/**
	 * @brief Check if any resource limit has been exceeded.
	 * 
	 * @param current_depth Current exploration depth
	 * @return true if any limit exceeded, false otherwise
	 */
	bool should_abort(unsigned int current_depth)
	{
		return check_timeout() || 
		       check_out_of_memory() ||
		       check_depth(current_depth);
	}
	
	/**
	 * @brief Check if timeout occurred.
	 */
	bool is_timed_out() const
	{
		return timed_out;
	}
	
	/**
	 * @brief Check if out of memory.
	 */
	bool is_out_of_memory() const
	{
		return out_of_memory;
	}
	
	/**
	 * @brief Check if depth was exceeded.
	 */
	bool is_depth_exceeded() const
	{
		return depth_exceeded;
	}
	
	/**
	 * @brief Reset all abort flags.
	 */
	void reset()
	{
		timed_out = false;
		out_of_memory = false;
		depth_exceeded = false;
	}

private:
	const unsigned long long max_cpu_time;  // in seconds
	const unsigned long long max_memory;      // in KiB
	const unsigned long long max_depth;
	
	Processor_clock cpu_clock;
	Memory_monitor memory_monitor;
	
	bool timed_out;
	bool out_of_memory;
	bool depth_exceeded;
};

} // namespace Global
} // namespace NP

#endif // GLOBAL_RESOURCE_MONITOR_HPP
