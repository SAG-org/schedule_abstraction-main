#ifndef PRUNING_COND_HPP
#define PRUNING_COND_HPP

#include <map>
#include <vector>

#include "jobs.hpp"
#include "index_set.hpp"

namespace NP {

    /**
     * @brief Conditions used for pruning during state space exploration.
     *
     * This structure holds information about which jobs to prune,
     * specific job sets that trigger stopping conditions
     */
    struct Pruning_condition
    {
    public:
        Pruning_condition() : time_limit(0) {}

        Pruning_condition(size_t num_jobs)
            : prune_jobs(num_jobs), time_limit(0)
        {
        }

        Pruning_condition(const Index_set& prune_jobs,
            const std::multimap<Job_index, std::vector<Job_index>>& stop_job_sets,
            long long time_limit = 0)
            : prune_jobs(prune_jobs),
            stop_job_sets(stop_job_sets),
            time_limit(time_limit)
        {
        }

        Index_set prune_jobs;
        std::multimap<Job_index, std::vector<Job_index>> stop_job_sets;
        long long time_limit; // 0 means no time limit
    };
}
#endif // PRUNING_COND_HPP