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
			Precedence_type type = finish_to_start)
			: from(from)
			, to(to)
			, delay(delay)
			, type(type)
		{
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

		void set_toIndex(Job_index index)
		{
			toIndex = index;
		}

		Job_index get_toIndex() const
		{
			return toIndex;
		}

		void set_fromIndex(Job_index index)
		{
			fromIndex = index;
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
	void validate_prec_cstrnts(std::vector<Precedence_constraint<Time>>& precs,
		const typename Job<Time>::Job_set jobs)
	{

		for (Precedence_constraint<Time>& prec : precs) {
			const Job<Time>& fromJob = lookup<Time>(jobs, prec.get_fromID());
			const Job<Time>& toJob = lookup<Time>(jobs, prec.get_toID());
			// set toIndex and fromIndex here.
			// TODO: get rid of this. Dangerous that job index is changed after construction !
			prec.set_toIndex((Job_index)(&toJob - &(jobs[0])));
			prec.set_fromIndex((Job_index)(&fromJob - &(jobs[0])));
			if (prec.get_max_delay() < prec.get_min_delay()) {
				throw InvalidPrecParameter(prec.get_fromID());
			}
		}
	}
}

#endif
