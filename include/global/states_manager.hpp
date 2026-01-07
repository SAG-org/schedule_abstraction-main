#ifndef STATES_MANAGER_HPP
#define STATES_MANAGER_HPP

#include <memory>
#include "global/node.hpp"
#include "global/state.hpp"
#include "object_pool.hpp"
#include "global/state_space_data.hpp"

#ifdef CONFIG_PARALLEL
#include <unordered_map>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_queue.h>
#else
#include <unordered_map>
#endif

namespace NP {
	namespace Global {

		/**
		 * @brief Manages node and state allocation, caching, and cleanup
		 * 
		 * Encapsulates the complexity of object pool management and node caching.
		 * This class is responsible for:
		 * - Allocating nodes and states from pools
		 * - Caching nodes by their lookup keys
		 * - Finding cached nodes based on keys and scheduled jobs
		 * - Releasing nodes and states back to pools
		 */
		template<class Time>
		class States_manager {
		public:
			typedef Schedule_node<Time> Node;
			typedef std::shared_ptr<Node> Node_ref;
			typedef Schedule_state<Time> State;
			typedef std::shared_ptr<State> State_ref;
#ifdef CONFIG_PARALLEL
			// Thread-safe concurrent queue for storing nodes during parallel execution
			typedef tbb::concurrent_queue<Node_ref> Node_refs;
			typedef tbb::concurrent_vector<Node_ref> Nodes_vec;
			typedef tbb::concurrent_unordered_map<hash_value_t, Nodes_vec> Nodes_map;
#else
			typedef std::deque<Node_ref> Node_refs;
			typedef std::unordered_map<hash_value_t, Node_refs> Nodes_map;
#endif
			typedef std::vector<Nodes_map> Nodes_map_per_depth;
			typedef std::vector<Node_refs> Nodes_storage;

			// default constructor
			States_manager() = default;

			/**
			 * @brief Construct a new States Manager object
			 * @param size Maximum size for node storage (number of layers of the SAG it keeps track of)
			 */
			States_manager(unsigned int size)
			: nodes_storage(size)
			, nodes_by_key(size)
			{}

			/**
			 * @brief Set the maximum size for node storage (number of layers of the SAG it keeps track of)
			 */
			void set_size(size_t size)
			{
				nodes_storage.resize(size);
				nodes_by_key.resize(size);
			}

			/**
			 * @brief Return a new node from the pool
			 * @param depth Layer of the SAG the nodes belongs to
			 * @param args Arguments to forward to the Node constructor
			 * @return Shared pointer to the allocated Node
			 */
			template <typename... Args>
			Node_ref new_node(size_t depth, Args&&... args)
			{
				auto d = depth % nodes_storage.size();
				auto n = node_pool.acquire(std::forward<Args>(args)...);
				assert(n);
#ifdef CONFIG_PARALLEL
				nodes_storage[d].push(n);
#else
				nodes_storage[d].push_back(n);
#endif
				cache_node(n, d);
				return n;
			}

			/**
			 * @brief Return a new state from the pool
			 * @param args Arguments to forward to the State constructor
			 * @return Shared pointer to the allocated State
			 */
			template <typename... Args>
			State_ref new_state(Args&&... args)
			{
				return state_pool.acquire(std::forward<Args>(args)...);
			}

			/**
			 * @brief Release a node back to the pool
			 * @param n Shared pointer to the Node to release
			 */
			void release_node(const Node_ref& n)
			{
				assert(n);
				node_pool.release(n);
			}

			/**
			 * @brief Release a state back to the pool
			 * @param s Shared pointer to the State to release
			 */
			void release_state(const State_ref& s)
			{
				assert(s);
				state_pool.release(s);
			}

			/**
			 * @brief Get all stored nodes at a specific depth (i.e., a specific layer of the SAG)
			 * @param depth Layer of the SAG from which the nodes must be recovered
			 * @return Reference to the collection of nodes at the specified depth
			 */
			Node_refs& get_nodes_at_depth(size_t depth)
			{
				return nodes_storage[depth % nodes_storage.size()];
			}

			/**
			 * @brief Find a cached node with matching key and scheduled jobs
			 * 
			 * @param key Lookup key for the node
			 * @param depth Layer of the SAG where the node is stored
			 * @param scheduled_jobs Set of jobs that are marked as dispatched in the node we look for
			 * @return Pointer to matching node, or nullptr if not found
			 */
			Node_ref find_node(hash_value_t key, size_t depth, const Job_set& scheduled_jobs)
			{
				auto d = depth % nodes_storage.size();
				const auto pair_it = nodes_by_key[d].find(key);
				if (pair_it != nodes_by_key[d].end()) {
					for (Node_ref other : pair_it->second) {
						if (other->get_scheduled_jobs() == scheduled_jobs) {
							return other;
						}
					}
				}
				return nullptr;
			}

			/**
			 * @brief Find a cached node with matching key and scheduled jobs including a new job
			 * 
			 * The function looks for a node with the correct lookup key. It then checks if the 
			 * scheduled jobs set in the found node matches the set formed by adding `new_job` to
			 * `old_scheduled_jobs`. This function is slighlty more efficient than constructing the 
			 * new Job_set externally before calling the other find_cached_node function.
			 * 
			 * @param key Lookup key for the node
			 * @param depth Layer of the SAG where the node is stored
			 * @param old_scheduled_jobs Set of jobs that are marked as dispatched before adding the new job
			 * @param new_job Index of the new job to include in the scheduled jobs
			 * @return Pointer to matching node, or nullptr if not found
			 */
			Node_ref find_node(hash_value_t key, size_t depth, const Job_set& old_scheduled_jobs, Job_index new_job)
			{
				auto d = depth % nodes_storage.size();
				const auto pair_it = nodes_by_key[d].find(key);
				if (pair_it != nodes_by_key[d].end()) {
					Job_set new_sched_jobs{ old_scheduled_jobs, new_job };
					for (Node_ref other : pair_it->second) {
						if (other->get_scheduled_jobs() == new_sched_jobs) {
							return other;
						}
					}
				}
				return nullptr;
			}

			/**
			 * @brief Clear the node cache
			 */
			void clear_cache()
			{
				for (auto& map : nodes_by_key) {
					map.clear();
				}
			}

			/**
			 * @brief Clear all stored nodes at all depths
			 */
			void clear_storage()
			{
				for (auto& node_list : nodes_storage) {
					node_list.clear();
				}
			}

			/**
			 * @brief Clear cached nodes at a specific depth
			 * @param depth Layer of the SAG to clear
			 */
			void clear_cache_at_depth(size_t depth)
			{
				auto d = depth % nodes_by_key.size();
				nodes_by_key[d].clear();
			}

			/**
			 * @brief Clear stored nodes at a specific depth
			 * @param depth Layer of the SAG to clear
			 */
			void clear_storage_at_depth(size_t depth)
			{
				auto d = depth % nodes_storage.size();
				nodes_storage[d].clear();
			}

			/**
			 * @brief Clear all cached and stored nodes
			 */
			void clear()
			{
				clear_cache();
				clear_storage();
			}

			/**
			 * @brief Release all states associated with a node
			 * @param node Shared pointer to the Node whose states are to be released
			 */
			void release_states_of(Node_ref node)
			{
				assert(node);
				if (!node) 
					return;
				auto states = node->get_states();
				for (auto s = states->begin(); s != states->end(); s++) {
					release_state(*s);
				}
			}

		private:
			Object_pool<Node> node_pool;
			Object_pool<State> state_pool;
			Nodes_map_per_depth nodes_by_key;
			Nodes_storage nodes_storage;

			/**
			 * @brief Cache a node by its lookup key
			 * 
			 * Adds the node to the nodes_by_key map for quick lookup.
			 * 
			 * @param n Shared pointer to the Node to cache
			 * @param d Depth index in the cache where the node is stored
			 */
			void cache_node(Node_ref n, size_t d)
			{
				assert(n);
				if (!n) 
					return;
				// create a new list if needed, or lookup if already existing
#ifdef CONFIG_PARALLEL
				auto res = nodes_by_key[d].emplace(n->get_key(), Nodes_vec());
#else
				auto res = nodes_by_key[d].emplace(n->get_key(), Node_refs());
#endif
				auto pair_it = res.first;
				auto& list = pair_it->second;

				list.push_back(n);
			}
		};

	}
}

#endif // STATES_MANAGER_HPP
