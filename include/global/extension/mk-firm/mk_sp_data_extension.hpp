#ifndef MK_STATE_SPACE_DATA_EXTENSION_HPP
#define MK_STATE_SPACE_DATA_EXTENSION_HPP

#include "global/state_space_data.hpp"
#include "global/extension/state_space_data_extension.hpp"
#include "global/extension/mk-firm/mk_firm.hpp"
#include "jobs.hpp"
#include "problem.hpp"

namespace NP {
	namespace Global {
		namespace MK_analysis {

			template<class Time>
			class MK_sp_data_extension : public State_space_data_extension<Time>
			{
				typedef Interval<unsigned int> Misses;
				std::vector<Misses>  misses_per_job; // min and max number of misses in the window ending with the dispatch of each job
				unsigned int num_tasks;
				MK_constraints mk_constraints;
			public:
				MK_sp_data_extension(size_t num_jobs, const MK_constraints& mk_constraints)
					: misses_per_job(num_jobs, Misses(-1, -1))
					, num_tasks((unsigned int) mk_constraints.size())
					, mk_constraints(mk_constraints)
				{
					assert(mk_constraints.size() > 0);
				}

				const Misses& get_misses(const Job_index& j_index) const
				{
					return misses_per_job[j_index];
				}

				const std::vector<Misses>& get_results() const
				{
					return misses_per_job;
				}

				std::ostringstream export_results(const State_space_data<Time>& sp_data) const override
				{
					auto ss = std::ostringstream();
					ss << "Task ID, RespectMK, Min Misses, Max Misses" << std::endl;
					const auto& jobs = sp_data.jobs;
					std::unordered_map<Task_index, Interval<unsigned int>> misses_per_task;
					for (const auto& j : jobs) {
						auto t = j.get_task_id();
						if (misses_per_task.find(t) == misses_per_task.end()) {
							misses_per_task[t] = misses_per_job[j.get_job_index()];
						}
						else {
							misses_per_task[t].extend_to(misses_per_job[j.get_job_index()].max());
							misses_per_task[t].reduce_to(misses_per_job[j.get_job_index()].min());
						}
					}
					for (const auto& [t, m] : misses_per_task) {
						ss << t << ", "
						   << (mk_constraints.at(t).misses >= m.max() ? "1" : "0") << ", "
						   << m.min() << ", "
						   << m.max() << std::endl;
					}
					ss << "Job ID, Task ID, Min Misses, Max Misses" << std::endl;					
					for (const auto& j : jobs) {
						ss << j.get_job_id() << ", "
						   << j.get_task_id() << ", "
						   << misses_per_job[j.get_task_id()].min() << ", "
						   << misses_per_job[j.get_task_id()].max() << std::endl;
					}
					return ss;
				}

				void submit_misses(const Job_index& j_index, const unsigned int min_misses, const unsigned int max_misses)
				{
					assert(j_index < misses_per_job.size());
					assert(min_misses <= max_misses);
					if (misses_per_job[j_index].min() == -1)
						misses_per_job[j_index] = Misses(min_misses, max_misses);
					else {
						misses_per_job[j_index].extend_to(max_misses);
						misses_per_job[j_index].reduce_to(min_misses);
					}
				}

				unsigned int get_num_tasks() const
				{
					return num_tasks;
				}

				unsigned int get_mk_window_length(unsigned int task) const
				{
					return mk_constraints.at(task).window_length;
				}
			};
		}
	}
}
#endif // MK_STATE_SPACE_DATA_EXTENSION_HPP