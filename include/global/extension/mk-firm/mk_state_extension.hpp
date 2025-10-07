#ifndef MK_STATE_EXTENSION_HPP
#define MK_STATE_EXTENSION_HPP
#include "global/extension/state_extension.hpp"
#include "global/extension/state_space_data_extension.hpp"
#include "global/extension/mk-firm/mk_firm.hpp"
#include "global/extension/mk-firm/mk_sp_data_extension.hpp"

namespace NP {
	namespace Global {
		namespace MK_analysis {
			template<class Time>
			class MK_state_extension : public State_extension<Time>
            {
				std::vector<Misses_window<Time>> sliding_misses;
                
			public:
                MK_state_extension() = default;

                // Initial state construction
                void construct(const size_t extension_id,
                    const size_t state_space_data_ext_id,
                    const Schedule_state<Time>& new_state,
                    const unsigned int num_processors,
                    const State_space_data<Time>& state_space_data) override
                {
                    auto spd_ext = state_space_data.get_extensions().get<MK_sp_data_extension<Time>>(state_space_data_ext_id);
					auto num_tasks = spd_ext->get_num_tasks();
                    for ( int i = 0; i < num_tasks; i++)
                        sliding_misses.emplace_back(spd_ext->get_mk_window_length(i));
                }

                void construct(const size_t extension_id,
                    const size_t state_space_data_ext_id,
                    const Schedule_state<Time>& new_state,
                    const std::vector<Interval<Time>>& proc_initial_state,
                    const State_space_data<Time>& state_space_data) override
                {
                    auto spd_ext = state_space_data.get_extensions().get<MK_sp_data_extension<Time>>(state_space_data_ext_id);
                    auto num_tasks = spd_ext->get_num_tasks();
                    for (int i = 0; i < num_tasks; i++)
                        sliding_misses.emplace_back(spd_ext->get_mk_window_length(i));
                }

                void construct(const size_t extension_id,
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
                    unsigned int ncores = 1) override
                {
                    auto from_ext = from.get_extensions().get<MK_state_extension<Time>>(extension_id);
                    sliding_misses = from_ext->sliding_misses;
                    const auto& job = state_space_data.jobs[j];
                    auto task = job.get_task_id();
                    if (finish_times.min() > job.get_deadline())
                        sliding_misses[task].add_certain_miss();
                    else if (finish_times.max() > job.get_deadline())
						sliding_misses[task].add_potential_miss();
                    else
                        sliding_misses[task].add_certain_hit();

                    const auto& extensions = state_space_data.get_extensions();
                    auto spd_ext = extensions.get<MK_sp_data_extension<Time>>(state_space_data_ext_id);
					spd_ext->submit_misses(j, sliding_misses[task].get_num_certain_misses(), sliding_misses[task].get_num_potential_misses());
                }

                // Reset variants
                void reset(const size_t extension_id,
                    const size_t state_space_data_ext_id,
                    const Schedule_state<Time>& new_state,
                    const unsigned int num_processors,
                    const State_space_data<Time>& state_space_data) override
                {
                    clear();
                    construct(extension_id, state_space_data_ext_id, new_state, num_processors, state_space_data);
                }

                void reset(const size_t extension_id,
                    const size_t state_space_data_ext_id,
                    const Schedule_state<Time>& new_state,
                    const std::vector<Interval<Time>>& proc_initial_state,
                    const State_space_data<Time>& state_space_data) override
                {
                    clear();
                    construct(extension_id, state_space_data_ext_id, new_state, proc_initial_state, state_space_data);
                }

                void reset(const size_t extension_id,
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
                    unsigned int ncores = 1) override
                {
                    construct(extension_id, state_space_data_ext_id, new_state, from, j, start_times, finish_times,
                        scheduled_jobs, jobs_with_pending_start_succ, jobs_with_pending_finish_succ, ready_succ_jobs, state_space_data, next_source_job_rel, ncores);
                }

                void merge(size_t extension_id, const Schedule_state<Time>& this_state, const Schedule_state<Time>& other) override
                {
                    auto other_ext = other.get_extensions().get<MK_state_extension<Time>>(extension_id);
                    assert(other_ext->sliding_misses.size() == sliding_misses.size());
                    for (size_t i = 0; i < sliding_misses.size(); i++) {
                        sliding_misses[i].merge(other_ext->sliding_misses[i]);
                    }
				}

            private:
                void clear() 
                {
                    sliding_misses.clear();
				}
			};
		}
	}
}

#endif // MK_STATE_EXTENSION_HPP