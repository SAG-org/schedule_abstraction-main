#ifndef CONDITIONAL_CONSTRAINTS_HPP
#define CONDITIONAL_CONSTRAINTS_HPP
#include <vector>
#include <set>
#include <algorithm>
#include <memory>
#include "jobs.hpp"
#include "precedence.hpp"

namespace NP {

    template<class Time>
    class Conditional_dispatch_constraints {
		using Workload   = typename Job<Time>::Job_set;
        typedef std::vector<Job_index> Incompatible_jobs_set;
        typedef std::vector<Job_index> Conditional_siblings;

		// The set of conditional siblings for each job (nullptr if the job has no conditional siblings). If a job has conditional siblings then the set includes the job itself.
        std::vector<std::shared_ptr<Conditional_siblings>> siblings;
		// The set of jobs that will never be dispatched after the given job is dispatched. The job itself is thus included in the set.
        std::vector<Incompatible_jobs_set> incompatible_jobs;

    public:
        Conditional_dispatch_constraints(const Workload& jobs, const Precedence_constraints<Time>& precs) 
            : incompatible_jobs(jobs.size())
            , siblings(jobs.size(), nullptr)
        {
            DAG_graph dag = build_graph<Time>(jobs.size(), precs);
            build_siblings(jobs, dag);
			find_all_incompatible_jobs(dag);
        }

        /**
         * @brief Check if a job has conditional siblings.
         */
        bool has_conditional_siblings(Job_index j) const {
            return siblings[j] != nullptr;
        }

        /**
         * @brief Get the set of conditional siblings for a job. Note that if the job has conditional siblings then the set includes the job itself.
         */
        std::shared_ptr<Conditional_siblings> get_conditional_siblings(Job_index j) const {
            return siblings[j];
        }

		/**
		 * @brief Get the set of jobs that are incompatible with the given job.
		 */
		const Incompatible_jobs_set& get_incompatible_jobs(Job_index j) const {
			return incompatible_jobs[j];
		}

    private:
        /** 
         * @brief Build the mapping of conditional siblings.
         * Add all successor jobs of each conditional fork to the same set of conditional siblings.
         * @param jobs The set of jobs in the workload.
         * @param precs The inter-job precedence constraints.
         */
        void build_siblings(const Workload& jobs, const DAG_graph& graph) {
            for (Job_index j = 0; j < jobs.size(); ++j) {
                const auto& job = jobs[j];
                if (job.get_type() == Job<Time>::C_FORK) {
                    auto sibling_set = std::make_shared<Conditional_siblings>();
                    // find all siblings
                    const auto& successors = graph.successors[j];
                    for (const auto& succ_index : successors) {
                        sibling_set->push_back(succ_index);
                        siblings[succ_index] = sibling_set;
                    }
                }
            }
        }

		 /**
         * @brief Compute incompatible jobs for all jobs in the workload.
         * @param graph The inter-job precedence constraints graph.
         */
		void find_all_incompatible_jobs(const DAG_graph& graph) {
			const std::size_t n = graph.size();
			incompatible_jobs.resize(n);
			for (std::size_t i = 0; i < n; ++i) {
				// if we did not compute the incompatible jobs for job i yet
				if (incompatible_jobs[i].empty()) {
					// compute incompatible jobs for job i
					if (has_conditional_siblings(i)) {
						// compute incompatible jobs for all siblings at once
						const auto& sibs = *(siblings[i]);
						std::vector<std::set<Job_index>> sib_descendants(sibs.size());
						// union of all descendants of all siblings and siblings themsleves
						std::set<Job_index> union_set;
						for (std::size_t s = 0; s < sibs.size(); ++s) {
							sib_descendants[s] = get_descendants(sibs[s], graph);
							union_set.insert(sib_descendants[s].begin(), sib_descendants[s].end());
							union_set.insert(sibs[s]); // include the sibling itself
						}
						// for each sibling, the incompatible jobs are the union minus its own descendants
						for (std::size_t s = 0; s < sibs.size(); ++s) {
							Job_index sib_index = sibs[s];
							for (Job_index uj : union_set) {
								if (sib_descendants[s].count(uj) == 0) {
									incompatible_jobs[sib_index].push_back(uj);
								}
							}
						}
					} else {
						// non-conditional siblings have only themselves as incompatible
						incompatible_jobs[i].push_back(i); 
					}
				}
			}
		}
    };
        

} // namespace NP

#endif // CONDITIONAL_CONSTRAINTS_HPP