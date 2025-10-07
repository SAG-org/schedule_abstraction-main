#ifndef STATE_EXTENSION_HPP
#define STATE_EXTENSION_HPP

#include <vector>
#include <memory>
#include <typeinfo>
#include <type_traits>
#include "jobs.hpp"
#include "interval.hpp"
#include "global/state_space_data.hpp"
#include "global/state.hpp"

namespace NP {
	namespace Global {
		typedef Index_set Job_set;

		template<class Time> class Schedule_state;
		template<class Time> class State_space_data;

		template<class Time>
		class State_extension
		{
		public:
			State_extension() = default;
			virtual ~State_extension() = default;

			virtual void construct(const size_t extension_id, 
				const size_t state_space_data_ext_id, 
				const Schedule_state<Time>& new_state, 
				const unsigned int num_processors, 
				const State_space_data<Time>& state_space_data) 
			{
			}
			
			virtual void construct(const size_t extension_id,
				const size_t state_space_data_ext_id,
				const Schedule_state<Time>& new_state,
				const std::vector<Interval<Time>>& proc_initial_state, 
				const State_space_data<Time>& state_space_data)
			{
			}

			virtual void construct(const size_t extension_id,
				const size_t state_space_data_ext_id,
				const Schedule_state<Time>& new_state,
				const Schedule_state<Time>& from,
				Job_index j,
				const Interval<Time>& start_times,
				const Interval<Time>& finish_times,
				const Job_set& scheduled_jobs,
				const std::vector<Job_index>& jobs_with_pending_start_succ,
				const std::vector<Job_index>& jobs_with_pending_finish_succ,
				const std::vector<const Job<Time>*>& ready_succ_jobs,
				const State_space_data<Time>& state_space_data,
				Time next_source_job_rel,
				unsigned int ncores = 1)
			{
			}
			
			virtual void reset(const size_t extension_id,
				const size_t state_space_data_ext_id,
				const Schedule_state<Time>& new_state,
				const unsigned int num_processors,
				const State_space_data<Time>& state_space_data)
			{
			}

			virtual void reset(const size_t extension_id,
				const size_t state_space_data_ext_id,
				const Schedule_state<Time>& new_state,
				const std::vector<Interval<Time>>& proc_initial_state,
				const State_space_data<Time>& state_space_data)
			{
			}

			virtual void reset(const size_t extension_id,
				const size_t state_space_data_ext_id,
				const Schedule_state<Time>& new_state,
				const Schedule_state<Time>& from,
				Job_index j,
				const Interval<Time>& start_times,
				const Interval<Time>& finish_times,
				const Job_set& scheduled_jobs,
				const std::vector<Job_index>& jobs_with_pending_start_succ,
				const std::vector<Job_index>& jobs_with_pending_finish_succ,
				const std::vector<const Job<Time>*>& ready_succ_jobs,
				const State_space_data<Time>& state_space_data,
				Time next_source_job_rel,
				unsigned int ncores = 1)
			{
			}
			
			// merge this extension with another one of the same type
			virtual void merge(size_t extension_id, const Schedule_state<Time>& this_state, const Schedule_state<Time>& other) {}
		};

		// Registry for State_extension types - holds creators and mappings
		// Owned by State_space_data or similar owning class
		template<class Time>
		class State_extension_registry {
		public:
			using Creator = std::function<std::unique_ptr<State_extension<Time>>()>;

			State_extension_registry() = default;

			// Register an extension type with its state_space_data extension ID
			// Returns the index of the newly registered extension
			size_t register_extension(size_t state_space_data_ext_id, Creator creator) {
				creators_.push_back(std::move(creator));
				state_space_data_ext_ids_.push_back(state_space_data_ext_id);
				return creators_.size() - 1;
			}

			// Template helper for registering extension types
			template<class Extension>
			size_t register_extension(size_t state_space_data_ext_id) {
				return register_extension(state_space_data_ext_id, 
					[]() { return std::make_unique<Extension>(); });
			}

			const std::vector<Creator>& creators() const { return creators_; }
			const std::vector<size_t>& state_space_data_ext_ids() const { return state_space_data_ext_ids_; }

		private:
			std::vector<Creator> creators_;
			std::vector<size_t> state_space_data_ext_ids_;
		};

		template<class Time>
		class State_extensions {
		public:
			// Construct State_extensions from a registry
			explicit State_extensions(const State_extension_registry<Time>& registry) {
				create_extensions(registry);
			}

			// Default constructor creates empty extensions (for backwards compatibility)
			explicit State_extensions() = default;

			// returns a const pointer to the extension with id, or nullptr if not found
			template<class Extension>
			const Extension* operator[](size_t id) const {
				if (id < extensions.size()) {
					return static_cast<Extension*>(extensions[id].get());
				}
				return nullptr; // Out of bounds
			}

			// returns a const pointer to the extension with id, or nullptr if not found
			template<class Extension>
			const Extension* get(size_t id) const {
				if (id < extensions.size()) {
					return static_cast<Extension*>(extensions[id].get());
				}
				return nullptr; // Out of bounds
			}

			// returns a pointer to the extension of type Extension, or nullptr if not found
			template<class Extension>
			Extension* get() {
				// Find the extension by comparing type_info
				for (const auto& ext : extensions) {
					if (auto casted = dynamic_cast<Extension*>(ext.get())) {
						// If found, cast the void* back to the correct type
						return casted;
					}
				}
				return nullptr; // Not found
			}

			template<class... Args>
			void construct(Args&&... args) {
				for (size_t i = 0; i < extensions.size(); i++) {
					extensions[i]->construct(i, state_space_data_extensions[i], std::forward<Args>(args)...);
				}
			}

			template<class... Args>
			void reset(Args&&... args) {
				for (size_t i = 0; i < extensions.size(); i++) {
					extensions[i]->reset(i, state_space_data_extensions[i], std::forward<Args>(args)...);
				}
			}

			// merge all extensions with their counter-part in `other`
			void merge(const Schedule_state<Time>& this_state, const Schedule_state<Time>& other)
			{
				for (size_t i = 0; i < extensions.size(); i++) {
					extensions[i]->merge(i, this_state, other);
				}
			}

		private:			
			// vector of all registered extensions after their construction
			std::vector<std::unique_ptr<State_extension<Time>>> extensions;
			// vector of state space data extension IDs for each registered extension
			std::vector<size_t> state_space_data_extensions;

			// construct all registered extensions from the registry
			void create_extensions(const State_extension_registry<Time>& registry) {
				state_space_data_extensions = registry.state_space_data_ext_ids();
				for (auto& c : registry.creators()) {
					extensions.push_back(c());
				}
			}
		};
	}
}
#endif // !STATE_EXTENSION_HPP
