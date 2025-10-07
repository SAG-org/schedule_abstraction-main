#ifndef MK_EXTENSION_HPP
#define MK_EXTENSION_HPP
#include "global/extension/extension.hpp"
#include "global/extension/mk-firm/mk_sp_data_extension.hpp"
#include "global/extension/mk-firm/mk_state_extension.hpp"
#include "global/extension/mk-firm/mk_problem_extension.hpp"

namespace NP {
	namespace Global {
		namespace MK_analysis {
			template<class Time>
			class MK_extension : public Extension<Time, MK_state_extension<Time>, MK_sp_data_extension<Time>> 
			{
			};
		}
	}
}
#endif // !MK_EXTENSION_HPP
