#ifndef SECATEUR_HPP
#define SECATEUR_HPP

#include <map>
#include <vector>

#include "pruning_cond.hpp"
#include "jobs.hpp"
#include "index_set.hpp"
#include "global/state.hpp"
#include "global/node.hpp"

namespace NP {
    namespace Global {

        template<class Time>
        class Secateur
        {
        public:
            /** 
             * @brief Construct a Secateur with given pruning condition.
             * 
             * @param cond Pruning condition specifying which branches to prune in the SAG.
             */
            Secateur(const Pruning_condition& cond)
                : prune_jobs(cond.prune_jobs),
                stop_job_sets(cond.stop_job_sets),
                time_limit(cond.time_limit)
            {
            }

            /**
             * @brief Determine whether to prune a branch resulting from dispatching the given job.
             * @param job Job being dispatched
             * @return true if the branch should be pruned, false otherwise
             */
            bool prune_branch(const Job<Time>& job) const
            {
                Job_index job_index = job.get_job_index();

                if (prune_jobs.contains(job_index)) {
                    return true;
                }
               
                return false;
            }

            /**
             * @brief Determine whether to prune a branch resulting from dispatching the given job in the given node.
             * @param job Job being dispatched
             * @param n Current node
             * @return true if the branch should be pruned, false otherwise
             */
            bool prune_branch(const Job<Time>& job, const Schedule_node<Time>& n) const
            {
                Job_index job_index = job.get_job_index();

                if (prune_jobs.contains(job_index)) {
                    return true;
                }

                if (stop_job_sets.count(job_index) > 0) {
                    for (auto it = stop_job_sets.equal_range(job_index).first; it != stop_job_sets.equal_range(job_index).second; ++it) {
                        bool prune = true;
                        const auto& jobs = it->second;
                        for (const auto& j : jobs) {
                            if (j != job_index && !n.job_dispatched(j)) {
                                prune = false;
                                break;
                            }
                        }
                        if (prune)
                            return true;
                    }
                }

                // Time-based pruning: if the earliest time we can dispatch `job` in this node is after the time limit, prune
                if (time_limit > 0 && (job.earliest_arrival() > static_cast<Time>(time_limit) || n.earliest_core_availability() > static_cast<Time>(time_limit))) {
                    return true;
                }

                return false;
            }

        private:
            /** Set of job indices whose dispatching should trigger pruning. */
            Index_set prune_jobs;
            /** Map of job indices to sets of job indices; dispatching a job should trigger pruning if all jobs in any associated set have been dispatched. */
            std::multimap<Job_index, std::vector<Job_index>> stop_job_sets;
            /** Time such that if the earliest dispatch time of a job is beyond this limit the resulting branch of the SAG is pruned. 0 means no time limit.*/
            long long time_limit;
        };
    }
}

#endif