#ifndef PRECEDENCE_HPP
#define PRECEDENCE_HPP

#include "jobs.hpp"

namespace NP {

	enum Precedence_type {
		start_to_start,
		finish_to_start
	};

	template<class Time>
	class Precedence_constraint {
	public:
		Precedence_constraint(JobID from,
			JobID to,
			Interval<Time> delay,
			const Job_lookup_table& jobs_lookup)
			: from(from)
			, to(to)
			, delay(delay)
			, type(finish_to_start)
		{
			try {
				fromIndex = jobs_lookup.at(from);
				toIndex = jobs_lookup.at(to);
			}
			catch (const std::out_of_range&) {
				throw InvalidJobReference(from);
			}
		}

		Precedence_constraint(JobID from,
			JobID to,
			Interval<Time> delay,
			Precedence_type type, 
			const Job_lookup_table& jobs_lookup)
			: from(from)
			, to(to)
			, delay(delay)
			, type(type)
		{
			try {
				fromIndex = jobs_lookup.at(from);
				toIndex = jobs_lookup.at(to);
			}
			catch (const std::out_of_range&) {
				throw InvalidJobReference(from);
			}
		}

		// deprecated, for backward compatibility only
		Precedence_constraint(JobID from,
			JobID to,
			Interval<Time> delay,
			const typename Job<Time>::Job_set& jobs)
			: from(from)
			, to(to)
			, delay(delay)
			, type(finish_to_start)
		{
			const Job<Time>& jobA = lookup<Time>(jobs, from);
			const Job<Time>& jobB = lookup<Time>(jobs, to);
			fromIndex = (Job_index)(&jobA - &(jobs[0]));
			toIndex = (Job_index)(&jobB - &(jobs[0]));
		}

		// deprecated, for backward compatibility only
		Precedence_constraint(JobID from,
			JobID to,
			Interval<Time> delay,
			Precedence_type type, const typename Job<Time>::Job_set& jobs)
			: from(from)
			, to(to)
			, delay(delay)
			, type(type)
		{
			const Job<Time>& jobA = lookup<Time>(jobs, from);
			const Job<Time>& jobB = lookup<Time>(jobs, to);
			fromIndex = (Job_index)(&jobA - &(jobs[0]));
			toIndex = (Job_index)(&jobB - &(jobs[0]));
		}

		JobID get_fromID() const
		{
			return from;
		}

		JobID get_toID() const
		{
			return to;
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

		Job_index get_toIndex() const
		{
			return toIndex;
		}

		Job_index get_fromIndex() const
		{
			return fromIndex;
		}

		Precedence_type get_type() const
		{
			return type;
		}

	private:
		JobID from;
		JobID to;
		// toIndex and fromIndex are set during validation
		Job_index toIndex;
		Job_index fromIndex;
		Interval<Time> delay;
		Precedence_type type;
	};

	class InvalidPrecParameter : public std::exception
	{
	public:

		InvalidPrecParameter(const JobID& bad_id)
			: ref(bad_id)
		{}

		const JobID ref;

		virtual const char* what() const noexcept override
		{
			return "invalid precedence constraint parameters";
		}

	};

	template<class Time>
	void validate_prec_cstrnts(std::vector<Precedence_constraint<Time>>& precs)
	{
		for (Precedence_constraint<Time>& prec : precs) {
			if (prec.get_max_delay() < prec.get_min_delay()) {
				throw InvalidPrecParameter(prec.get_fromID());
			}

			if (prec.get_fromIndex() == (Job_index)(-1) || prec.get_toIndex() == (Job_index)(-1)) {
				throw std::invalid_argument("Invalid job indices in precedence constraints");
			}
		}
	}
}

#endif
