#ifndef MK_PROBLEM_EXTENSION
#define MK_PROBLEM_EXTENSION

#include "global/extension/problem_extension.hpp"

namespace NP {
	namespace Global {
		namespace MK_analysis {
			class MK_problem_extension : public Problem_extension
			{
				MK_constraints mk_constraints;
			public:
				MK_problem_extension(const MK_constraints& mk_constraints)
					: mk_constraints(mk_constraints)
				{}
				const MK_constraints& get_mk_constraints() const {
					return mk_constraints;
				}
			};
		}
	}
}

#endif // !MK_PROBLEM_EXTENSION
