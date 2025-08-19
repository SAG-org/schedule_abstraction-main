#include "problem_extension.hpp"
#include "state_space_data_extension.hpp"

namespace NP {
    namespace Global {

        std::vector<std::unique_ptr<Problem_extension_base>> Problem_extensions::extensions;
        std::vector<std::unique_ptr<State_space_data_extension>> State_space_data_extensions::extensions;
        
    } // namespace Global   
} // namespace NP