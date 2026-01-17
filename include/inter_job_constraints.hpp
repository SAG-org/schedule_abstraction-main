#ifndef INTER_JOB_CONSTRAINTS_HPP
#define INTER_JOB_CONSTRAINTS_HPP

#include <vector>
#include <algorithm>
#include "interval.hpp"
#include "jobs.hpp"
#include "problem.hpp"

namespace NP {

        /**
         * @brief Represents a delay constraint associated with a job
         * = how much time since/until the referenced job may start/finish must elapse
         */
		template<class Time>
		struct Job_delay {
			const Job<Time>* reference_job;
			Interval<Time> delay;
		};

        template<class Time> class Inter_job_constraints;

        /**
         * @brief Represents inter-job constraints for a specific job
         * 
         * Contains various types of precedence and exclusion constraints.
         * Delay constraints with predecessors affect when the job can start based on other jobs.
         * Delay constraints with successors affect when successor jobs can start based on this job.
         * Mutual exclusion constraints affect when both jobs can start based on each other.
         */
		template<class Time>
		class Job_constraints {
            friend Inter_job_constraints<Time>;
        public:
            typedef std::vector<Job_delay<Time>> Delay_list;
            typedef std::unordered_map<Job_index, Time> Delay_map;

            // const reference to all constraint lists to avoid modifying them from outside
            const Delay_list& predecessors_start_to_start;
            const Delay_list& predecessors_finish_to_start;
            const Delay_list& start_to_successors_start;
            const Delay_list& finish_to_successors_start;
            const Delay_list& between_starts;
            const Delay_list& between_executions;
            const Delay_map& min_start_delay_after_start_of;
            const Delay_map& min_start_delay_after_finish_of;

            /**
             * @brief Constructor initializing all constraint lists
             */
            Job_constraints()
            : predecessors_start_to_start(_predecessors_start_to_start)
            , predecessors_finish_to_start(_predecessors_finish_to_start)
            , start_to_successors_start(_start_to_successors_start)
            , finish_to_successors_start(_finish_to_successors_start)
            , between_starts(_between_starts)
            , between_executions(_between_executions)
            , min_start_delay_after_start_of(_min_start_delay_after_start_of)
            , min_start_delay_after_finish_of(_min_start_delay_after_finish_of)
            {}

            /**
             * @brief Get the minimum delay after the start of a predecessor job after which this job can start
             * @param predecessor The predecessor job index
             * @return The minimum delay, or -1 if no such constraint exists
             */
            Time get_min_delay_after_start_of(Job_index predecessor) const {
                auto it = min_start_delay_after_start_of.find(predecessor);
                if (it != min_start_delay_after_start_of.end()) {
                    return it->second;
                }
                return -1;
			}

            /**
             * @brief Get the minimum delay after the finish of a predecessor job after which this job can start
             * @param predecessor The predecessor job index
             * @return The minimum delay, or -1 if no such constraint exists
             */
			Time get_min_delay_after_finish_of(Job_index predecessor) const {
                auto it = min_start_delay_after_finish_of.find(predecessor);
                if (it != min_start_delay_after_finish_of.end()) {
                    return it->second;
                }
				return -1;
			}

        private:
			// Delay constraints that must be respected between the start of j's predecessors and the start of j
			// This means j can't start until all jobs in predecessors_start_to_start have started.
			Delay_list _predecessors_start_to_start;

			// Delay constraints that must be respected between the completion of j's predecessors and the start of j
			// This means j can't start until all jobs in predecessors_finish_to_start have finished.
			Delay_list _predecessors_finish_to_start;

			// Delay constraints that must be respected between the start of j and the start of its successors
			// This means none of the jobs in start_to_successors_start can start before j starts.
			Delay_list _start_to_successors_start;

			// Delay constraints that must be respected between the completion of j and the start of its successors
			// This means none of the jobs in finish_to_successors_start can start before j has finished.
			Delay_list _finish_to_successors_start;
			// Delay constraints that must be respected between the start of j and the start of any other job in that list.
			// There is no ordering constraint. Job j may start before or after the jobs in the list, but the distance between starts must be respected.
			Delay_list _between_starts;

			// Delay constraints that must be respected between the completion of j and the start of any other job in that list, or between the completion of the jobs in the list and the start of j.
			// There is no ordering constraint. Job j may start before or after the jobs in the list, but the distance between finish and starts of jobs must be respected.
			Delay_list _between_executions;

            // Maps for quick access to minimum time this job must wait after the start of other jobs
            Delay_map _min_start_delay_after_start_of;
            // Maps for quick access to minimum time this job must wait after the finish of other jobs
            Delay_map _min_start_delay_after_finish_of;

            /**
             * @brief Finalize the constraint lists by populating the delay maps and sorting the lists by job priority
             */
            void finalize() {
                for (const auto& jd : _predecessors_start_to_start) {
                    if (_min_start_delay_after_start_of.find(jd.reference_job->get_job_index()) == _min_start_delay_after_start_of.end())
                        _min_start_delay_after_start_of[jd.reference_job->get_job_index()] = jd.delay.min();
                    else
                        _min_start_delay_after_start_of[jd.reference_job->get_job_index()] = 
                            std::min(_min_start_delay_after_start_of[jd.reference_job->get_job_index()], jd.delay.min());
                }
                for (const auto& jd : _between_starts) {
                    if (_min_start_delay_after_start_of.find(jd.reference_job->get_job_index()) == _min_start_delay_after_start_of.end())
                        _min_start_delay_after_start_of[jd.reference_job->get_job_index()] = jd.delay.min();
                    else
                        _min_start_delay_after_start_of[jd.reference_job->get_job_index()] = 
                            std::min(_min_start_delay_after_start_of[jd.reference_job->get_job_index()], jd.delay.min());
                }
                for (const auto& jd : _predecessors_finish_to_start) {
                    if (_min_start_delay_after_finish_of.find(jd.reference_job->get_job_index()) == _min_start_delay_after_finish_of.end())
                        _min_start_delay_after_finish_of[jd.reference_job->get_job_index()] = jd.delay.min();
                    else
                        _min_start_delay_after_finish_of[jd.reference_job->get_job_index()] = 
                            std::min(_min_start_delay_after_finish_of[jd.reference_job->get_job_index()], jd.delay.min());
                }
                for (const auto& jd : _between_executions) {
                    if (_min_start_delay_after_finish_of.find(jd.reference_job->get_job_index()) == _min_start_delay_after_finish_of.end())
                        _min_start_delay_after_finish_of[jd.reference_job->get_job_index()] = jd.delay.min();
                    else
                        _min_start_delay_after_finish_of[jd.reference_job->get_job_index()] = 
                            std::min(_min_start_delay_after_finish_of[jd.reference_job->get_job_index()], jd.delay.min());
                }
                sort_by_priority();
            }

			/**
             * @brief Sort all delay lists by job priority (non-increasing order)
             */
            void sort_by_priority() {
                auto priority_comparator = [](const Job_delay<Time>& a, const Job_delay<Time>& b) {
                    return a.reference_job->higher_priority_than(*(b.reference_job));
                };

                std::sort(_predecessors_finish_to_start.begin(), _predecessors_finish_to_start.end(), priority_comparator);
                std::sort(_finish_to_successors_start.begin(), _finish_to_successors_start.end(), priority_comparator);
                std::sort(_predecessors_start_to_start.begin(), _predecessors_start_to_start.end(), priority_comparator);
                std::sort(_start_to_successors_start.begin(), _start_to_successors_start.end(), priority_comparator);
                std::sort(_between_starts.begin(), _between_starts.end(), priority_comparator);
                std::sort(_between_executions.begin(), _between_executions.end(), priority_comparator);
            }
		};

        /**
         * @brief Represents all inter-job constraints for a set of jobs. All constraints lists are sorted by job priority.
         * 
         * Contains the Job_constraints for each job in the workload.
         */
        template<class Time>
        class Inter_job_constraints {
            typedef Job_constraints<Time> Constraints;
            using Workload = typename Scheduling_problem<Time>::Workload;
            using Precedence_constraints = typename Scheduling_problem<Time>::Precedence_constraints;
            using Mutex_constraints = typename Scheduling_problem<Time>::Mutex_constraints;

        public:
            /**
             * @brief Constructor initializing constraints for all jobs in the workload
             * 
             * @param jobs The set of jobs in the workload
             * @param edges The precedence constraints between jobs
             * @param mutexes The mutual exclusion constraints between jobs
             */
            Inter_job_constraints(
                const Workload& jobs,
                const Precedence_constraints& edges,
                const Mutex_constraints& mutexes)
            : constraints{jobs.size()}
            , has_start2start_constraints(false)
            , has_finish2start_constraints(false)
            , has_precedence_cstrs(false)
            {
                build_precedence_constraints(jobs, edges);
                build_mutex_constraints(jobs, mutexes);
                for (auto& c : constraints) 
                    c.finalize();
            }

            /**
             * @brief Get the constraints for a specific job
             * 
             * @param j The job index
             * @return The Job_constraints for the specified job
             */
            const Constraints& get_job_constraints(Job_index j) const {
                return constraints[j];
            }

            /**
             * @brief Get the constraints for a specific job
             * 
             * @param j The job index
             * @return The Job_constraints for the specified job
             */
            const Constraints& operator[](Job_index j) const {
                return constraints[j];
            }

            /**
             * @brief Check if there are any start-to-start constraint (i.e., dispatch ordering or mutual start exclusion) for any job
             */
            bool has_start_to_start_constraints() const {
                return has_start2start_constraints;
            }

            /**
             * @brief Check if there are any finish-to-start constraint (i.e., precedence or mutual execution exclusion) for any job
             */
            bool has_finish_to_start_constraints() const {
                return has_finish2start_constraints;
            }

            /**
             * @brief Check if there are any precedence constraints (of any type) for any job
             */
            bool has_precedence_constraints() const {
                return has_precedence_cstrs;
            }
            
        private:
            std::vector<Constraints> constraints;
            bool has_start2start_constraints;
            bool has_finish2start_constraints;
            bool has_precedence_cstrs;

            /**
             * @brief Build precedence constraints from the given set of jobs and edges
             * 
             * @param jobs The set of jobs in the workload
             * @param edges The precedence constraints between jobs
             */
            void build_precedence_constraints(
				const Workload& jobs,
				const Precedence_constraints& edges)
			{
                has_precedence_cstrs = !edges.empty();
                
				for (const auto& e : edges) {
					const Job<Time>& from_job = jobs[e.get_fromIndex()];
					const Job<Time>& to_job = jobs[e.get_toIndex()];

					if (e.get_type() == start_to_start) {
						constraints[e.get_fromIndex()]._start_to_successors_start.push_back({ &to_job, e.get_delay() });
						constraints[e.get_toIndex()]._predecessors_start_to_start.push_back({ &from_job, e.get_delay() });
                        has_start2start_constraints = true;
					}
					else if (e.get_type() == finish_to_start) {
						constraints[e.get_fromIndex()]._finish_to_successors_start.push_back({ &to_job, e.get_delay() });
						constraints[e.get_toIndex()]._predecessors_finish_to_start.push_back({ &from_job, e.get_delay() });
						has_finish2start_constraints = true;
					}
				}
			}

            /**
             * @brief Build mutex constraints from the given set of jobs and mutexes
             * 
             * @param jobs The set of jobs in the workload
             * @param mutexes The mutual exclusion constraints between jobs
             */
			void build_mutex_constraints(
				const Workload& jobs,
				const Mutex_constraints& mutexes)
			{
				for (const auto& m : mutexes) {
					const Job<Time>& job_a = jobs[m.get_jobA_index()];
					const Job<Time>& job_b = jobs[m.get_jobB_index()];

					// NOTE: we exclude jobs with delay_max == 0 because the start constraint does not constrain anything then
					if (m.get_type() == start_exclusion && m.get_max_delay() > 0) {
						constraints[m.get_jobA_index()]._between_starts.push_back({ &job_b, m.get_delay() });
						constraints[m.get_jobB_index()]._between_starts.push_back({ &job_a, m.get_delay() });
                        has_start2start_constraints = true;
					}
					else if (m.get_type() == exec_exclusion) {
						constraints[m.get_jobA_index()]._between_executions.push_back({ &job_b, m.get_delay() });
						constraints[m.get_jobB_index()]._between_executions.push_back({ &job_a, m.get_delay() });
                        has_finish2start_constraints = true;
					}
				}
			}
        };
} // namespace NP

#endif // INTER_JOB_CONSTRAINTS_HPP
