#ifndef EXTENSION_HPP
#define EXTENSION_HPP

#include "global/extension/state_space_data_extension.hpp"
#include "global/extension/state_extension.hpp"

namespace NP {
	namespace Global {

		template<class Time, class State_ext, class State_space_data_ext>
		class Extension
		{
		public:
			Extension() = default;
			virtual ~Extension() = default;

			// Register the extension in the State_extensions and State_space_data_extensions manager
			template<class... Args>
			static void activate(State_space_data<Time>& state_space_data, Args&&... args) {
				// Register the state space data extension
				auto ext_id = state_space_data.get_extensions().register_extension<State_space_data_ext>(std::forward<Args>(args)...);
				
				// Register the state extension type with the state extension registry
				state_space_data.get_state_extension_registry().template register_extension<State_ext>(ext_id);
			}
		};
	}
}

#endif // !EXTENSION_HPP
