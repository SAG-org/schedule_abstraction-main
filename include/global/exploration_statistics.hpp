#ifndef GLOBAL_EXPLORATION_STATISTICS_HPP
#define GLOBAL_EXPLORATION_STATISTICS_HPP

#include <vector>
#include <utility>

#ifdef CONFIG_PARALLEL
#include <atomic>
#endif

namespace NP {
namespace Global {

/**
 * @brief Collects statistics during state-space exploration.
 * 
 * This class tracks various metrics about the exploration process,
 * including counts of nodes, states, and edges, as well as the
 * evolution of the exploration front width over time.
 * 
 * Thread-safe when CONFIG_PARALLEL is defined.
 */
class Exploration_statistics
{
public:
	/**
	 * @brief Construct with initial job count.
	 * 
	 * @param num_jobs Total number of jobs in the workload
	 */
	explicit Exploration_statistics(size_t num_jobs)
		: num_nodes(0)
		, num_states(0)
		, num_edges(0)
		, max_width(0)
		, width_evolution(num_jobs, {0, 0})
	{
	}
	
	/**
	 * @brief Increment the node counter.
	 */
	void count_node()
	{
#ifdef CONFIG_PARALLEL
		num_nodes.fetch_add(1, std::memory_order_relaxed);
#else
		++num_nodes;
#endif
	}
	
	/**
	 * @brief Increment the state counter.
	 */
	void count_state()
	{
#ifdef CONFIG_PARALLEL
		num_states.fetch_add(1, std::memory_order_relaxed);
#else
		++num_states;
#endif
	}
	
	/**
	 * @brief Increment the state counter by a specific amount.
	 */
	void count_states(unsigned long count)
	{
#ifdef CONFIG_PARALLEL
		num_states.fetch_add(count, std::memory_order_relaxed);
#else
		num_states += count;
#endif
	}
	
	/**
	 * @brief Decrement the state counter (when states are merged).
	 */
	void remove_states(unsigned long count)
	{
#ifdef CONFIG_PARALLEL
		num_states.fetch_sub(count, std::memory_order_relaxed);
#else
		num_states -= count;
#endif
	}
	
	/**
	 * @brief Increment the edge counter.
	 */
	void count_edge()
	{
#ifdef CONFIG_PARALLEL
		num_edges.fetch_add(1, std::memory_order_relaxed);
#else
		++num_edges;
#endif
	}
	
	/**
	 * @brief Get the total number of nodes explored.
	 */
	unsigned long get_num_nodes() const
	{
#ifdef CONFIG_PARALLEL
		return num_nodes.load(std::memory_order_relaxed);
#else
		return num_nodes;
#endif
	}
	
	/**
	 * @brief Get the total number of states created.
	 */
	unsigned long get_num_states() const
	{
#ifdef CONFIG_PARALLEL
		return num_states.load(std::memory_order_relaxed);
#else
		return num_states;
#endif
	}
	
	/**
	 * @brief Get the total number of edges (transitions) explored.
	 */
	unsigned long get_num_edges() const
	{
#ifdef CONFIG_PARALLEL
		return num_edges.load(std::memory_order_relaxed);
#else
		return num_edges;
#endif
	}
	
	/**
	 * @brief Get the maximum exploration front width observed.
	 */
	unsigned long get_max_width() const
	{
		return max_width;
	}
	
	/**
	 * @brief Get the evolution of exploration front width over time.
	 * 
	 * @return Vector of (front_width, new_states) pairs, one per job scheduled
	 */
	const std::vector<std::pair<unsigned long, unsigned long>>& 
	get_width_evolution() const
	{
		return width_evolution;
	}
	
	/**
	 * @brief Record the exploration front width at a specific depth.
	 * 
	 * This should be called once per job scheduling level.
	 * 
	 * @param depth Current depth (job count)
	 * @param front_width Number of nodes in exploration front
	 * @param states_added Number of new states created at this level
	 */
	void record_width(
		unsigned long depth,
		unsigned long front_width,
		unsigned long states_added)
	{
		if (depth < width_evolution.size()) {
			width_evolution[depth] = {front_width, states_added};
		}
		
		if (front_width > max_width) {
			max_width = front_width;
		}
	}
	
	/**
	 * @brief Reset all statistics to zero.
	 */
	void reset()
	{
#ifdef CONFIG_PARALLEL
		num_nodes.store(0, std::memory_order_relaxed);
		num_states.store(0, std::memory_order_relaxed);
		num_edges.store(0, std::memory_order_relaxed);
#else
		num_nodes = 0;
		num_states = 0;
		num_edges = 0;
#endif
		max_width = 0;
		for (auto& entry : width_evolution) {
			entry = {0, 0};
		}
	}

private:
#ifdef CONFIG_PARALLEL
	std::atomic<unsigned long> num_nodes;
	std::atomic<unsigned long> num_states;
	std::atomic<unsigned long> num_edges;
#else
	unsigned long num_nodes;
	unsigned long num_states;
	unsigned long num_edges;
#endif
	
	// These are only updated by main thread, no synchronization needed
	unsigned long max_width;
	std::vector<std::pair<unsigned long, unsigned long>> width_evolution;
};

} // namespace Global
} // namespace NP

#endif // GLOBAL_EXPLORATION_STATISTICS_HPP
