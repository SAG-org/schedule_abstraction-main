#ifndef SECATEUR_HPP
#define SECATEUR_HPP

#include <map>
#include <vector>

#include "pruning_cond.hpp"
#include "jobs.hpp"
#include "index_set.hpp"
#include "global/state.hpp"

namespace NP {

    namespace Global {

        template<class Time>
        class Secateur
        {
        public:
            Secateur(const Pruning_condition& cond)
                : prune_jobs(cond.prune_jobs),
                stop_job_sets(cond.stop_job_sets),
                time_limit(cond.time_limit)
            {
            }

            bool prune_branch(const Job<Time>& job) const
            {
                Job_index job_index = job.get_job_index();

                if (prune_jobs.contains(job_index)) {
                    return true;
                }
               
                return false;
            }

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
                if (time_limit > 0 && (job.earliest_arrival() > static_cast<Time>(time_limit) || n.finish_range().min() > static_cast<Time>(time_limit))) {
                    return true;
                }

                return false;
            }

        private:
            Index_set prune_jobs;
            std::multimap<Job_index, std::vector<Job_index>> stop_job_sets;
            long long time_limit; // 0 means no time limit
        };
    }
}

#endif