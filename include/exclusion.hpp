#ifndef EXCLUSION_HPP
#define EXCLUSION_HPP

#include "jobs.hpp"

namespace NP {

	enum Exclusion_type {
		start_exclusion,
		exec_exclusion
	};

	template<class Time>
	class Exclusion_constraint {
	public:
		Exclusion_constraint(JobID jobA,
			JobID jobB,
			Interval<Time> delay, 
			const typename Job<Time>::Job_set& jobs)
			: jobA(jobA)
			, jobB(jobB)
			, delay(delay)
			, type(exec_exclusion)
		{
			const Job<Time>& jA = lookup<Time>(jobs, jobA);
			const Job<Time>& jB = lookup<Time>(jobs, jobB);
			jobA_idx = (Job_index)(&jA - &(jobs[0]));
			jobB_idx = (Job_index)(&jB - &(jobs[0]));
		}

		Exclusion_constraint(JobID jobA,
			JobID jobB,
			Interval<Time> delay,
			Exclusion_type type, const typename Job<Time>::Job_set& jobs)
			: jobA(jobA)
			, jobB(jobB)
			, delay(delay)
			, type(type)
		{
			const Job<Time>& jA = lookup<Time>(jobs, jobA);
			const Job<Time>& jB = lookup<Time>(jobs, jobB);
			jobA_idx = (Job_index)(&jA - &(jobs[0]));
			jobB_idx = (Job_index)(&jB - &(jobs[0]));
		}

		JobID get_jobA() const
		{
			return jobA;
		}

		JobID get_jobB() const
		{
			return jobB;
		}

		Time get_min_delay() const
		{
			return delay.from();
		}

		Time get_max_delay() const
		{
			return delay.until();
		}

		Interval<Time> get_delay() const
		{
			return delay;
		}

		Job_index get_jobB_index() const
		{
			return jobB_idx;
		}

		Job_index get_jobA_index() const
		{
			return jobA_idx;
		}

		Exclusion_type get_type() const
		{
			return type;
		}

	private:
		JobID jobA;
		JobID jobB;
		// toIndex and fromIndex are set during validation
		Job_index jobA_idx;
		Job_index jobB_idx;
		Interval<Time> delay;
		Exclusion_type type;
	};

	class InvalidExclusionParameter : public std::exception
	{
	public:

		InvalidExclusionParameter(const JobID& bad_id)
			: ref(bad_id)
		{}

		const JobID ref;

		virtual const char* what() const noexcept override
		{
			return "invalid exclusion constraint parameters";
		}

	};

	template<class Time>
	void validate_excl_cstrnts(std::vector<Exclusion_constraint<Time>>& excls)
	{
		for (Exclusion_constraint<Time>& excl : excls) {
			if (excl.get_max_delay() < excl.get_min_delay()) {
				throw InvalidExclusionParameter(excl.get_jobA());
			}
            // a start exclusion must have a non-zero max delay. Otherwise, it does not
            // constrain anything and thus does not exclude any execution scenario.
            if (excl.get_type() == start_exclusion && excl.get_max_delay() == 0) {
                throw InvalidExclusionParameter(excl.get_jobA());
            }

			if (excl.get_jobA_index() == (Job_index)(-1) || excl.get_jobB_index() == (Job_index)(-1)) {
				throw std::invalid_argument("Invalid job indices in exclusion constraints");
			}
		}
	}
}

#endif // EXCLUSION_HPP
