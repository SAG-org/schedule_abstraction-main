#ifndef PRECEDENCE_HPP
#define PRECEDENCE_HPP

#include <set>
#include <vector>
#include "jobs.hpp"

namespace NP {

	enum Precedence_type {
		start_to_start,
		finish_to_start
	};

	/** 
	 * @brief Class representing a precedence constraint between two jobs.
	 */
	template<class Time>
	class Precedence_constraint {
	public:
		/** 
		 * @brief Constructor for Precedence_constraint.
		 * @param from Source job ID.
		 * @param to Destination job ID.
		 * @param delay Delay interval between the finish and start or the start and start of the two jobs.
		 * @param jobs_lookup Lookup table to map JobID to Job_index.
		 */
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
				from_index = jobs_lookup.at(from);
				to_index = jobs_lookup.at(to);
			}
			catch (const std::out_of_range&) {
				throw InvalidJobReference(from);
			}
		}

		/** 
		 * @brief Constructor for Precedence_constraint.
		 * @param from Source job ID.
		 * @param to Destination job ID.
		 * @param delay Delay interval between the finish and start or the start and start of the two jobs.
		 * @param type Type of precedence constraint (start-to-start or finish-to-start).
		 * @param jobs_lookup Lookup table to map JobID to Job_index.
		 */
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
				from_index = jobs_lookup.at(from);
				to_index = jobs_lookup.at(to);
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
			from_index = (Job_index)(&jobA - &(jobs[0]));
			to_index = (Job_index)(&jobB - &(jobs[0]));
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
			from_index = (Job_index)(&jobA - &(jobs[0]));
			to_index = (Job_index)(&jobB - &(jobs[0]));
		}

		/** 
		 * @brief Get the JobID of the source job.
		 */
		JobID get_fromID() const
		{
			return from;
		}

		/** 
		 * @brief Get the JobID of the destination job.
		 */
		JobID get_toID() const
		{
			return to;
		}

		/** 
		 * @brief Get the minimum delay between the finish and start or the start and start of the two jobs.
		 */
		Time get_min_delay() const
		{
			return delay.from();
		}

		/** 
		 * @brief Get the maximum delay between the finish and start or the start and start of the two jobs.
		 */
		Time get_max_delay() const
		{
			return delay.until();
		}

		/** 
		 * @brief Get the delay interval between the finish and start or the start and start of the two jobs.
		 */
		Interval<Time> get_delay() const
		{
			return delay;
		}

		/** 
		 * @brief Get the index of the destination job in the job set.
		 */
		Job_index get_toIndex() const
		{
			return to_index;
		}

		/** 
		 * @brief Get the index of the source job in the job set.
		 */
		Job_index get_fromIndex() const
		{
			return from_index;
		}

		/** 
		 * @brief Get the type of precedence constraint (start-to-start or finish-to-start).
		 */
		Precedence_type get_type() const
		{
			return type;
		}

	private:
		JobID from; // source job ID
		JobID to; // destination job ID
		Job_index to_index; // index of the destination job in the job set
		Job_index from_index; // index of the source job in the job set
		Interval<Time> delay; // delay interval between the finish and start or the start and start of the two jobs
		Precedence_type type; // type of precedence constraint (finish-to-start or start-to-start)
	};

	template<class Time>
	using Precedence_constraints = std::vector<Precedence_constraint<Time>>;

	/** 
	 * @brief Exception class for invalid precedence constraint parameters.
	 */
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

	/** 
	 * @brief Validate the parameters of a set of precedence constraints.
	 * @param precs Set of precedence constraints to validate.
	 * @throws InvalidPrecParameter if any constraint has invalid parameters.
	 */
	template<class Time>
	void validate_prec_cstrnts(const Precedence_constraints<Time>& precs)
	{
		for (const Precedence_constraint<Time>& prec : precs) {
			if (prec.get_max_delay() < prec.get_min_delay()) {
				throw InvalidPrecParameter(prec.get_fromID());
			}

			if (prec.get_fromIndex() == (Job_index)(-1) || prec.get_toIndex() == (Job_index)(-1)) {
				throw std::invalid_argument("Invalid job indices in precedence constraints");
			}
		}
	}

	/** 
	 * @brief Structure representing a directed acyclic graph (DAG) of job precedence constraints.
	 */
	struct DAG_graph {
		std::vector<std::vector<Job_index>> successors;
		std::vector<std::vector<Job_index>> predecessors;
		std::size_t size() const {
			assert(successors.size() == predecessors.size());
			return successors.size();
		}
	};

	/** 
	 * @brief Build lookup maps and adjacency lists once for efficient access
	 * @param n_jobs Number of jobs in the graph.
	 * @param edges List of precedence constraints representing edges in the graph.
	 * @return DAG_Graph defining the predecessors and successors of each job.
	 */
	template<class Time>
	DAG_graph build_graph(const size_t n_jobs, const Precedence_constraints<Time>& edges) {
		validate_prec_cstrnts<Time>(edges);
		DAG_graph graph;
		graph.successors.resize(n_jobs);
		graph.predecessors.resize(n_jobs);

		// Build adjacency lists
		for (const auto& edge : edges) {
			auto from_it = edge.get_fromIndex();
			auto to_it = edge.get_toIndex();
			graph.successors[from_it].push_back(to_it);
			graph.predecessors[to_it].push_back(from_it);
		}
		return graph;
	}

	/** 
	 * @brief Get all ancestors of a job in the DAG.
	 * @param job_index The index of the job.
	 * @param graph The DAG graph structure.
	 * @return The job indices of the set of ancestors.
	 */
	inline std::set<Job_index> get_ancestors(Job_index job_index, const DAG_graph& graph) {
		std::set<Job_index> ancestors;
		std::vector<Job_index> stack;
		stack.push_back(job_index);

		while (!stack.empty()) {
			Job_index current = stack.back();
			stack.pop_back();

			for (Job_index pred : graph.predecessors[current]) {
				if (ancestors.insert(pred).second) {
					stack.push_back(pred);
				}
			}
		}
		return ancestors;
	}

	/**
	 * @brief Get all descendants of a job in the DAG.
	 * @param job_index The index of the job.
	 * @param graph The DAG graph structure.
	 * @return The job indices of the set of descendants.
	 */
	inline std::set<Job_index> get_descendants(Job_index job_index, const DAG_graph& graph) {
		std::set<Job_index> descendants;
		std::vector<Job_index> stack;
		stack.push_back(job_index);
		while (!stack.empty()) {
			Job_index current = stack.back();
			stack.pop_back();

			for (Job_index succ : graph.successors[current]) {
				if (descendants.insert(succ).second) {
					stack.push_back(succ);
				}
			}
		}
		return descendants;
	}

	/**
	 * @brief Get rank of every job in a precedence graph using Kahn's algorithm (topological sort)
	 * @param graph DAG defining the predecessors and successors of each job.
	 * @return Vector of ranks indexed by job index.
	 */
	template<class Time>
	std::vector<unsigned int> get_jobs_ranks(const DAG_graph& graph) {
		const std::size_t n = graph.successors.size();

		std::vector<std::size_t> in_degree(n, 0);
		std::vector<unsigned int> rank(n, 0);
		// Calculate in-degrees
		for (std::size_t i = 0; i < n; ++i) {
			in_degree[i] = graph.predecessors[i].size();
		}
		// Find all source nodes (no predecessors)
		std::vector<Job_index> queue;
		for (std::size_t i = 0; i < n; ++i) {
			if (in_degree[i] == 0) {
				queue.push_back(i);
				rank[i] = 1;
			}
		}
		// Process nodes in topological order
		std::size_t head = 0;
		while (head < queue.size()) {
			Job_index current = queue[head++];
			for (Job_index succ : graph.successors[current]) {
				// Update rank: max of all predecessors + 1
				rank[succ] = std::max(rank[succ], rank[current] + 1);
				
				if (--in_degree[succ] == 0) {
					queue.push_back(succ);
				}
			}
		}		
		return rank;
	}
}

#endif
