#ifndef JOBS_HPP
#define JOBS_HPP

#include <ostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <exception>
#include <string>
#include <memory>

#include "time.hpp"
#include "interval.hpp"

namespace NP {

	typedef std::size_t hash_value_t;
	typedef std::size_t Job_index;

	/** 
	 * @brief Unique identifier for a job, consisting of a job number and a task number.
	 */
	struct JobID {
		unsigned long job;
		unsigned long task;

		/** 
		 * @brief Constructor for JobID. 
		 * @param j_id Job number.
		 * @param t_id Task number.
		 */
		JobID(unsigned long j_id, unsigned long t_id)
		: job(j_id), task(t_id)
		{
		}

		/** 
		 * @brief Equality operator for JobID.
		 * @param other JobID to compare with.
		 * @return true if both job and task numbers are equal, false otherwise.
		 */
		bool operator==(const JobID& other) const
		{
			return this->task == other.task && this->job == other.job;
		}

		/**
		 * @brief Output stream operator for JobID.
		 * @param stream Output stream.
		 */
		friend std::ostream& operator<< (std::ostream& stream, const JobID& id)
		{
			stream << "T" << id.task << "J" << id.job;
			return stream;
		}
	};
}

namespace std {
	template<> struct hash<NP::JobID>
	{
		std::size_t operator()(NP::JobID const& id) const
		{
			std::hash<unsigned long> h;
			return (h(id.job) << 16) ^ h(id.task);
		}
	};
}

struct Analysis_config;

namespace NP {
	// forward declaration to allow friend declarations that reference Scheduling_problem<T>
	template<class Time> struct Scheduling_problem;

	/** 
	 * @brief Class representing a job. Encapsulates attributes such as arrival time, execution time, deadline, and priority.
	 */
	template<class Time> 
	class Job {
	public:
		typedef std::vector<Job<Time>> Job_set;
		typedef Time Priority; // Make it a time value to support EDF
		typedef std::map<unsigned int, Interval<Time>> Cost;
		// The type of a node in conditional DAGs (i.e., normal, conditional fork or condition join)
		// by default all jobs are normal jobs
		enum Job_type {
			NORMAL,
			C_FORK,
			C_JOIN
		};
	private:
		Interval<Time> arrival; // arrival time window
		Interval<unsigned int> parallelism; // on which range of core numbers can it run in parallel
		Cost exec_time; // execution time range depending on the number of cores assigned to the job when it executes
		Time bcet; // best-case execution time for any level of parallelism (derived from exec_time)
		Time wcet; // worst-case execution time for any level of parallelism (derived from exec_time)
		Time deadline; // absolute deadline
		Priority priority; // job priority
		JobID id; // unique job identifier
		hash_value_t key; // hash key for the job
		Job_index index;  // index in the jobs array of the workload.
		unsigned int rank; // rank of the job in its DAG (0 for source jobs)
		Job_type type; // type of the job (normal, conditional fork, or conditional join)
		
		/** 
		 * @brief Compute the hash key for the job based on its attributes.
		 */
		void compute_hash() {
			auto h = std::hash<Time>{};
			//RV: added index to the hash key, which seems to prevent collisions in state/node lookup keys.
			key = h((Time)index);
			key = (key << 4) ^ h(arrival.from());
			key = (key << 4) ^ h(id.task);
			key = (key << 4) ^ h(arrival.until());
			key = (key << 4) ^ h(exec_time.begin()->second.from());
			key = (key << 4) ^ h(deadline);
			key = (key << 4) ^ h(exec_time.begin()->second.upto());
			key = (key << 4) ^ h(id.job);
			key = (key << 4) ^ h(priority);
		}

	public:
		/** 
		 * @brief Constructor for Job.
		 * @param id Unique job identifier.
		 * @param arr Arrival time interval.
		 * @param costs Map of execution times based on the number of cores on which the job can run.
		 * @param dl Absolute deadline.
		 * @param prio Job priority.
		 * @param idx Index in the jobs array of the workload.
		 * @param tid Task identifier (default is 0).
		 * @param j_type Type of the job (default is NORMAL).
		 */
		Job(unsigned long id,
			Interval<Time> arr, const Cost& costs,
			Time dl, Priority prio,
			Job_index idx,
			unsigned long tid = 0,
			Job_type j_type = NORMAL)
		: arrival(arr), exec_time(costs), parallelism(costs.begin()->first, costs.rbegin()->first),
		  deadline(dl), priority(prio), id(id, tid), index(idx), type(j_type)
		{
			bcet = 0;
			wcet = 0;
			for (const auto& cost : costs) {
				if (cost.second.min() < bcet || bcet == 0)
					bcet = cost.second.min();
				if (cost.second.max() > wcet)
					wcet = cost.second.max();
			}
			compute_hash();
		}

		/** 
		 * @brief Constructor for a sequential job, i.e., that can only execute on a single core.
		 * @param id Unique job identifier.
		 * @param arr Arrival time interval.
		 * @param cost Execution time interval (assuming it can only execute on a single core).
		 * @param dl Absolute deadline.
		 * @param prio Job priority.
		 * @param idx Index in the jobs array of the workload.
		 * @param tid Task identifier (default is 0).
		 * @param j_type Type of the job (default is NORMAL).
		 */
		Job(unsigned long id,
			Interval<Time> arr, Interval<Time> cost,
			Time dl, Priority prio,
			Job_index idx,
			unsigned long tid = 0,
			Job_type j_type = NORMAL)
			: arrival(arr), parallelism(Interval<unsigned int>{ 1, 1 }),
			deadline(dl), priority(prio), id(id, tid), index(idx), type(j_type)
		{
			exec_time.emplace(1, cost);
			bcet = cost.min();
			wcet = cost.max();
			compute_hash();
		}

		/** 
		 * @brief Get the precomputed hash key for the job.
		 */
		hash_value_t get_key() const
		{
			return key;
		}

		/** 
		 * @brief Get the type of the job (normal, conditional fork, or conditional join).
		 */
		Job_type get_type() const
		{
			return type;
		}

		/** 
		 * @brief Get the earliest possible arrival time of the job.
		 */
		Time earliest_arrival() const
		{
			return arrival.from();
		}

		/** 
		 * @brief Get the latest possible arrival time of the job.
		 */
		Time latest_arrival() const
		{
			return arrival.until();
		}

		/** 
		 * @brief Get the arrival time window of the job.
		 */
		const Interval<Time>& arrival_window() const
		{
			return arrival;
		}

		/**
		* @brief Get the least execution time for a given level of parallelism.
		* @param ncores Number of cores the job is assumed to run on.
		*/
		Time least_exec_time(unsigned int ncores = 1) const
		{
			assert(ncores >= parallelism.min() && ncores <= parallelism.max());
			auto cost = exec_time.find(ncores);
			if (cost == exec_time.end())
				return Time_model::constants<Time>::infinity();
			else
				return cost->second.min();
		}

		/** 
		 * @brief Get the maximal execution time for a given level of parallelism.
		 * @param ncores Number of cores the job is assumed to run on.
		 */
		Time maximal_exec_time(unsigned int ncores = 1) const
		{
			assert(ncores >= parallelism.min() && ncores <= parallelism.max());
			auto cost = exec_time.find(ncores);
			if (cost == exec_time.end())
				return Time_model::constants<Time>::infinity();
			else
				return cost->second.max();
		}

		/** 
		 * @brief Get the best-case execution time for any level of parallelism.
		 */
		Time get_bcet() const
		{
			return bcet;
		}

		/** 
		 * @brief Get the worst-case execution time for any level of parallelism.
		 */
		Time get_wcet() const
		{
			return wcet;
		}

		/** 
		 * @brief Get the execution time interval for a given level of parallelism.
		 * @param ncores Number of cores the job is assumed to run on.
		 */
		Interval<Time> get_cost(unsigned int ncores = 1) const
		{
			assert(ncores >= parallelism.min() && ncores <= parallelism.max());
			auto cost = exec_time.find(ncores);
			if (cost == exec_time.end())
				return Interval<Time>{Time_model::constants<Time>::infinity(), Time_model::constants<Time>::infinity()};
			else
				return cost->second;
		}

		/** 
		 * @brief Get all execution time intervals for the job (for all levels of parallelism).
		 */
		const Cost& get_all_costs() const
		{
			return exec_time;
		}

		/** 
		 * @brief Get the next higher level of parallelism supported by the job.
		 * @param ncores a number of cores.
		 * @return First number of cores higher than ncores the job can run on, or -1 if none exists.
		 */
		int get_next_parallelism(unsigned int ncores) const
		{
			assert(ncores < parallelism.max());
			auto it = exec_time.upper_bound(ncores);
			if (it == exec_time.end())
				return -1;
			else
				return it->first;

		}

		/** 
		 * @brief Get the minimum level of parallelism supported by the job.
		 */
		unsigned int get_min_parallelism() const
		{
			return parallelism.min();
		}

		/** 
		 * @brief Get the maximum level of parallelism supported by the job.
		 */
		unsigned int get_max_parallelism() const
		{
			return parallelism.max();
		}

		/** 
		 * @brief Get the parallelism interval of the job.
		 */
		const Interval<unsigned int>& get_parallelism() const
		{
			return parallelism;
		}

		/** 
		 * @brief Get the absolute deadline of the job.
		 */
		Time get_deadline() const
		{
			return deadline;
		}

		/** 
		 * @brief Check if a given time exceeds the job's deadline, considering a tolerance for deadline misses.
		 * @param t Time to check.
		 * @return true if t exceeds the deadline beyond the tolerance, false otherwise.
		 */
		bool exceeds_deadline(Time t) const
		{
			return t > deadline
			       && (t - deadline) >
			          Time_model::constants<Time>::deadline_miss_tolerance();
		}

		/** 
		 * @brief Get the unique identifier of the job.
		 */
		JobID get_id() const
		{
			return id;
		}

		/** 
		 * @brief Get the job identifier.
		 */
		unsigned long get_job_id() const
		{
			return id.job;
		}

		/** 
		 * @brief Get the task identifier of the job.
		 */
		unsigned long get_task_id() const
		{
			return id.task;
		}

		/** 
		 * @brief Check if this job matches a given JobID.
		 * @param search_id JobID to compare with.
		 * @return true if the job's ID matches search_id, false otherwise.
		 */
		bool is(const JobID& search_id) const
		{
			return this->id == search_id;
		}

		/** 
		 * @brief Get the index of the job in the jobs array of the workload.
		 */
		Job_index get_job_index() const
		{
			return index;
		}

		/** 
		 * @brief Get the priority of the job.
		 */
		Priority get_priority() const
		{
			return priority;
		}

		/** 
		 * @brief Check if this job has higher priority than another job passed in parameter.
		 * @param other Job to compare with.
		 */
		bool higher_priority_than(const Job &other) const
		{
			return priority < other.priority
					// first tie-break by task ID
					|| (priority == other.priority
						&& id.task < other.id.task)
					// second tie-break by rank in DAG
					|| (priority == other.priority
						&& id.task == other.id.task
						&& rank < other.rank)
					// third, tie-break by job ID
					|| (priority == other.priority
						&& id.task == other.id.task
						&& rank == other.rank
						&& id.job < other.id.job);
		}

		/** 
		 * @brief Check if this job has at least the same priority as another job passed in parameter.
		 * @param other Job to compare with.
		 */
		bool priority_at_least_that_of(const Job &other) const
		{
			return priority <= other.priority;
		}

		/** 
		 * @brief Check if the job's priority exceeds a given priority level.
		 * @param prio_level Priority level to compare with.
		 */
		bool priority_exceeds(Priority prio_level) const
		{
			return priority < prio_level;
		}

		/** 
		 * @brief Check if the job's priority is at least a given priority level.
		 * @param prio_level Priority level to compare with.
		 */
		bool priority_at_least(Priority prio_level) const
		{
			return priority <= prio_level;
		}

		/** 
		 * @brief Get the scheduling window of the job as an interval [earliest_arrival, deadline).
		 */
		Interval<Time> scheduling_window() const
		{
			// inclusive interval, so take off one epsilon
			return Interval<Time>{
			                earliest_arrival(),
			                deadline - Time_model::constants<Time>::epsilon()};
		}

		/** 
		 * @brief Output stream operator for Job.
		 * @param stream Output stream.
		 */
		friend std::ostream& operator<< (std::ostream& stream, const Job& j)
		{
			stream << "Job{" << j.id.task << ", " << j.id.job << ", " << j.arrival << ", ";
			for (auto i : j.exec_time)
				stream << i.first << " cores: " << i.second << ", ";
			stream << j.deadline << ", " << j.priority << "," << j.type << "}";
			return stream;
		}

	private:
		/** 
		 * @brief Set the rank of the job in its DAG.
		 * @param r Rank to set.
		 */
		void set_rank(unsigned int r)
		{
			rank = r;
		}

		// allow problem_builder to set job ranks
		template<class T>
		friend std::unique_ptr<Scheduling_problem<T>> problem_builder(const std::string& jobs_file, 
																  const Analysis_config& config);
	};

	/** 
	 * @brief Exception class for invalid job references.
	 */
	class InvalidJobReference : public std::exception
	{
		public:

		InvalidJobReference(const JobID& bad_id)
		: ref(bad_id)
		{}

		const JobID ref;

		virtual const char* what() const noexcept override
		{
			return "invalid job reference";
		}

	};

	/** 
	 * @brief Lookup a job by its JobID in a set of jobs.
	 * @param jobs Set of jobs to search in.
	 * @param id JobID to search for.
	 * @return Reference to the job with the given JobID.
	 * @throws InvalidJobReference if no job with the given JobID exists.
	 */
	template<class Time>
	const Job<Time>& lookup(const typename Job<Time>::Job_set& jobs,
	                                 const JobID& id)
	{
		auto pos = std::find_if(jobs.begin(), jobs.end(),
		                        [id] (const Job<Time>& j) { return j.is(id); } );
		if (pos == jobs.end())
			throw InvalidJobReference(id);
		return *pos;
	}

	/** 
	 * @brief Check if a job with a given JobID exists in a set of jobs.
	 * @param jobs Set of jobs to search in.
	 * @param id JobID to search for.
	 * @return true if a job with the given JobID exists, false otherwise.
	 */
	template<class Time>
	bool contains_job_with_id(const typename Job<Time>::Job_set& jobs,
	                          const JobID& id)
	{
		auto pos = std::find_if(jobs.begin(), jobs.end(),
		                        [id] (const Job<Time>& j) { return j.is(id); } );
		return pos != jobs.end();
	}

	typedef std::unordered_map<JobID, Job_index> Job_lookup_table;

	/** 
	 * @brief Create a lookup table mapping JobIDs to job indices for a set of jobs.
	 * @param jobs Set of jobs to create the lookup table for.
	 * @return Job lookup table mapping JobIDs to job indices.
	 */
	template<class Time>
	Job_lookup_table make_job_lookup_table(const typename Job<Time>::Job_set& jobs)
	{
		Job_lookup_table lookup;
		for (const auto& j : jobs) {
			lookup[j.get_id()] = j.get_job_index();
		}
		return lookup;
	}

	/** 
	 * @brief Exception class for invalid job parallelism parameters.
	 */
	class InvalidJobParallelism : public std::exception
	{
	public:
		InvalidJobParallelism(const JobID& bad_id)
			: ref(bad_id)
		{
		}

		const JobID ref;

		virtual const char* what() const noexcept override
		{
			static std::string msg;
			msg = "invalid parallelism parameters for job T" + std::to_string(ref.task) + "J" + std::to_string(ref.job);
			return msg.c_str();
		}	

	};

	/** 
	 * @brief Validate the parallelism parameters of a set of jobs against the number of available cores.
	 * @param jobs Set of jobs to validate.
	 * @param num_cores Number of available cores.
	 * @throws InvalidJobParallelism if any job has invalid parallelism parameters.
	 */
	template<class Time>
	void validate_jobs(const typename Job<Time>::Job_set& jobs, const unsigned int num_cores)
	{
		for (const auto& j : jobs) {
			if (j.get_min_parallelism() > num_cores || j.get_max_parallelism() < j.get_min_parallelism()) {
				throw InvalidJobParallelism(j.get_id());
			}
		}
	}
}

namespace std {
	template<class T> struct hash<NP::Job<T>>
	{
		/**
		 * @brief Get the hash value for a job.
		 */
		std::size_t operator()(NP::Job<T> const& j) const
		{
			return j.get_key();
		}
	};
}

#endif
