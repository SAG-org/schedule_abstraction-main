#ifndef UNI_IIP_SPACE_H
#define UNI_IIP_SPACE_H

#include <unordered_set>
#include <unordered_map>
#include <map>
#include <vector>
#include <deque>
#include <list>
#include <algorithm>

#include <iostream>
#include <ostream>
#include <cassert>

#include "config.h"
#include "problem.hpp"
#include "jobs.hpp"
#include "precedence.hpp"
#include "clock.hpp"

#include "uni_iip/state_iip.hpp"

namespace NP {

	namespace UniprocIIP {

		template<class Time> class Null_IIP;

		template<class Time, class IIP = Null_IIP<Time>> class State_space
		{
			public:

			typedef Scheduling_problem<Time> Problem;
			typedef typename Scheduling_problem<Time>::Workload Workload;
			typedef typename Scheduling_problem<Time>::Abort_actions Abort_actions;
			typedef Schedule_state<Time> State;
			typedef Schedule_node<Time> Node;

			// This is the explore() that is called from the nptest file.
			// This function processes the Problem and based on the analysis options determines 
			// a State_space 's' that is returned. the cpu_time computes the time taken to run the analysis
			static State_space explore(
					const Problem& prob,
					const Analysis_options& opts)
			{
				// this is a uniprocessor analysis
				assert(prob.num_processors == 1);

				auto s = State_space(prob.jobs, prob.dag, prob.aborts,
				                     opts.timeout, opts.max_depth,
				                     opts.num_buckets, opts.early_exit);
				s.cpu_time.start();
				if (opts.be_naive)
					s.explore_naively();
				else
					s.explore();
				s.cpu_time.stop();
				return s;
			}

			// convenience interface for tests
			static State_space explore_naively(const Workload& jobs)
			{
				Problem p{jobs};
				Analysis_options o;
				o.be_naive = true;
				return explore(p, o);
			}

			// convenience interface for tests
			static State_space explore(const Workload& jobs)
			{
				Problem p{jobs};
				Analysis_options o;
				return explore(p, o);
			}

			// get_finish_times searches rta (stores the response time of all the jobs) for job j 
			// and returns the finish time interval if j is found else returns the interval[0,infinity]
			Interval<Time> get_finish_times(const Job<Time>& j) const
			{
				auto rbounds = rta.find(j.get_id());
				if (rbounds == rta.end()) {
					return Interval<Time>{0, Time_model::constants<Time>::infinity()};
				} else {
					return rbounds->second;
				}
			}

			bool is_schedulable() const
			{
				return !aborted && !observed_deadline_miss;
			}

			bool was_timed_out() const
			{
				return timed_out;
			}

			unsigned long number_of_nodes() const
			{
				return num_nodes;
			}

			unsigned long number_of_states() const
			{
				return num_states;
			}

			unsigned long number_of_edges() const
			{
				return num_edges;
			}

			unsigned long max_exploration_front_width() const
			{
				return width;
			}

			double get_cpu_time() const
			{
				return cpu_time;
			}

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH

			// The structure of an edge and all edge related functionalities are only run if CONFIG_COLLECT_SCHEDULE_GRAPH
			// is set. This is because in the analysis edge has no meaning or importance. 
			// There is no information stored in an edge that is necessary for building the graph or furthering the analysis.
			// It is only required when a DOT file is to be generated so that the graphical version of the
			// abstraction graph can be represented with edges.
			struct Edge {
				const Job<Time>* scheduled;
				const Node* source;
				const Node* target;
				const Interval<Time> finish_range;

				Edge(const Job<Time>* s, const Node* src, const Node* tgt,
				     const Interval<Time>& fr)
				: scheduled(s)
				, source(src)
				, target(tgt)
				, finish_range(fr)
				{
				}

				bool deadline_miss_possible() const
				{
					return scheduled->exceeds_deadline(finish_range.upto());
				}

				Time earliest_finish_time() const
				{
					return finish_range.from();
				}

				Time latest_finish_time() const
				{
					return finish_range.upto();
				}

				Time earliest_start_time() const
				{
					return finish_range.from() - scheduled->least_cost();
				}

				Time latest_start_time() const
				{
					return finish_range.upto() - scheduled->maximal_cost();
				}

			};

			const std::deque<Edge>& get_edges() const
			{
				return edges;
			}

			const std::deque<Node>& get_nodes() const
			{
				return nodes;
			}

#endif
			private:

			typedef Job_set Scheduled;

			typedef std::deque<Node> Nodes;

			// All the states created hrough the making of the schedule abstrcation graph are stored in States.
			// This is done in the function new_state()
			typedef std::deque<State> States;

			// Iterators are defined for the to iterate over the elemnts in the Nodes and States deque 
			typedef typename std::deque<Node>::iterator Node_ref;
			typedef typename std::deque<State>::iterator State_ref;

			// The Nodes_map typedef allows for storing a key-pair of hash_value_t and Node_ref. The hash_value_t is the
			// typedef defined in jobs.hpp that allows for creating a unique key for each node. A variable called the
			// nodes_by_key is created using Nodes_map. When a new node is made it is added to the map.
			// The use of Nodes_map can be found in new_node(), done_with_current_state() ad schedule().
			typedef std::unordered_multimap<hash_value_t, Node_ref> Nodes_map;

			typedef const Job<Time>* Job_ref;

			// By_time_map stores ll the jobs along with a partiular time. there are three variables of type By_time_map.
			// The three variables jobs_by_latest_arrival, jobs_by_earliest_arrival and jobs_by_deadline each store the job
			// along with the latest arrival time, earliest arrival time and the deadline respectively.
			typedef std::multimap<Time, Job_ref> By_time_map;

			// The Todo_queue stores the references of all the nodes that are edge nodes of the graph that are yet to
			// be processed to check if the graph can be built further. There are usually 2 Todo_queues defined. This is 
			// because the graph is built in a breadth first manner and so the one todo_queue is for the new breadth that 
			// is made while building the graph and the other todo_queue stores the nodes left to be processed in the 
			// current breadth.
			typedef std::deque<Node_ref> Todo_queue;

			// Response_times stores and updates the finish time intervals of the jobs as the graph is being built
			typedef std::unordered_map<JobID, Interval<Time> > Response_times;

			typedef std::vector<std::size_t> Job_precedence_set;


#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
			std::deque<Edge> edges;
#endif

			IIP iip;
			friend IIP;

			Response_times rta;
			bool aborted;
			bool timed_out;

			const Workload& jobs;

			std::vector<Job_precedence_set> job_precedence_sets;

			By_time_map jobs_by_latest_arrival;
			By_time_map jobs_by_earliest_arrival;
			By_time_map jobs_by_deadline;

			std::vector<const Abort_action<Time>*> abort_actions;

			Nodes nodes;
			States states;
			unsigned long num_nodes, num_states, num_edges, width;
			Nodes_map nodes_by_key;

			static const std::size_t num_todo_queues = 3;

			Todo_queue todo[num_todo_queues];
			int todo_idx;
			unsigned long current_job_count;

			Processor_clock cpu_time;
			double timeout;

			unsigned int max_depth;

			bool early_exit;
			bool observed_deadline_miss;

			// Constructor of the State_space class
			State_space(const Workload& jobs,
			            const Precedence_constraints &dag_edges,
			            const Abort_actions& aborts,
			            double max_cpu_time = 0,
			            unsigned int max_depth = 0,
			            std::size_t num_buckets = 1000,
			            bool early_exit = true)
			: jobs(jobs)
			, aborted(false)
			, timed_out(false)
			, timeout(max_cpu_time)
			, max_depth(max_depth)
			, iip(*this, jobs)
			, num_nodes(0)
			, num_states(0)
			, num_edges(0)
			, width(0)
			, todo_idx(0)
			, current_job_count(0)
			, job_precedence_sets(jobs.size())
			, early_exit(early_exit)
			, observed_deadline_miss(false)
			, abort_actions(jobs.size(), NULL)
			{
				for (const Job<Time>& j : jobs) {
					jobs_by_latest_arrival.insert({j.latest_arrival(), &j});
					jobs_by_earliest_arrival.insert({j.earliest_arrival(), &j});
					jobs_by_deadline.insert({j.get_deadline(), &j});
				}
				for (auto e : dag_edges) {
					const Job<Time>& from = lookup<Time>(jobs, e.first);
					const Job<Time>& to   = lookup<Time>(jobs, e.second);
					job_precedence_sets[index_of(to)].push_back(index_of(from));
				}
				for (const Abort_action<Time>& a : aborts) {
					const Job<Time>& j = lookup<Time>(jobs, a.get_id());
					abort_actions[index_of(j)] = &a;
				}
			}

			private:

			// For all the jobs of Workload, the maximum deadline amongst all the jobs is returned
			static Time max_deadline(const Workload &jobs)
			{
				Time dl = 0;
				for (auto j : jobs)
					dl = std::max(dl, j.get_deadline());
				return dl;
			}

			// When a path in the graph has a job scheduled on it, the job and the finish time interval of the job on that 
			// path are passed as parameters to update_finish_times. If this job already exists in rts, then the finish time
			// stored in rta is widened with the new finish time 'range'. If no such job is present in rta, the job is added
			// to rta along with its finish time interval
			void update_finish_times(const Job<Time>& j, Interval<Time> range)
			{
				auto rbounds = rta.find(j.get_id());
				if (rbounds == rta.end()) {
					rta.emplace(j.get_id(), range);
					if (j.exceeds_deadline(range.upto()))
						observed_deadline_miss = true;
				} else {
					rbounds->second.widen(range);
					if (j.exceeds_deadline(rbounds->second.upto()))
						observed_deadline_miss = true;
				}
				DM("      New finish time range for " << j
				   << ": " << rta.find(j.get_id())->second << std::endl);

				if (early_exit && observed_deadline_miss)
					aborted = true;
			}

			// The address of j is subtracted from the first elemet of jobs to find the distance between the two ie., index
			std::size_t index_of(const Job<Time>& j) const
			{
				return (std::size_t) (&j - &(jobs[0]));
			}

			// incomplete() checks to see that a job j jas not been scheduled yet.
			bool incomplete(const Scheduled &scheduled, const Job<Time>& j) const
			{
				return !scheduled.contains(index_of(j));
			}

			bool incomplete(const Node& n, const Job<Time>& j) const
			{
				return incomplete(n.get_scheduled_jobs(), j);
			}

			// find next time by which a job is certainly released
			// The next certainly released job is found for a particular node, irrespective of prioirty. It is 
			// first checked to see if it in incomplete and then if IIP exists, it checks for IIp eligibility 
			// and also prioirty eligibility in case of IIP. The value returned is either the latest arrival 
			// of a job found and if no job is found infinity is returned.
			Time next_certain_job_release(const Node& n, const State& s)
			{
				const Scheduled &already_scheduled = n.get_scheduled_jobs();

				for (auto it = jobs_by_latest_arrival
				               .lower_bound(s.earliest_finish_time());
				     it != jobs_by_latest_arrival.end(); it++) {
					const Job<Time>& j = *(it->second);

					DM(__FUNCTION__ << " considering:: "  << j << std::endl);

					// not relevant if already scheduled
					if (!incomplete(already_scheduled, j))
						continue;

					// If the job is not IIP-eligible when it is certainly
					// released, then there exists a schedule where it doesn't
					// count, so skip it.
					if (!iip_eligible(n, s, j, std::max(j.latest_arrival(), s.latest_finish_time())))
						continue;

					// It must be priority-eligible when released, too.
					// Relevant only if we have an IIP, otherwise the job is
					// trivially priority-eligible.
					if (iip.can_block &&
					    !priority_eligible(n, s, j, std::max(j.latest_arrival(), s.latest_finish_time())))
						continue;

					// great, this job fits the bill
					return j.latest_arrival();
				}

				return Time_model::constants<Time>::infinity();
			}

			// find next time by which a job of higher priority
			// is certainly released on or after a given point in time
			// This function is very similar to the next_certain_job release, except that it checks if a job is higher
			// prioirty job instead of checking for iip eligibility. It also looks for a higher prioirty certain next 
			// release which means that it is not looking for the earliest certain release in the entire node, but the 
			// earliest higher prioirty certain job release based on job reference_job
			Time next_certain_higher_priority_job_release(
				const Node& n,
				const State& s,
				const Job<Time>& reference_job)
			{

				for (auto it = jobs_by_latest_arrival
				               .lower_bound(s.earliest_finish_time());
				     it != jobs_by_latest_arrival.end(); it++) {
					const Job<Time>& j = *(it->second);

					// not relevant if already scheduled
					if (!incomplete(n, j))
						continue;

					// irrelevant if not of sufficient priority
					if (!j.higher_priority_than(reference_job))
						continue;

					// great, this job fits the bill
					return j.latest_arrival();
				}

				return Time_model::constants<Time>::infinity();
			}

// define a couple of iteration helpers

// For all the jobs that arrives after the earliest job release of a node n, check if these jobs are still incomplete 
#define foreach_possibly_pending_job(ppj_macro_local_n, ppj_macro_local_j) 	\
	for (auto ppj_macro_local_it = jobs_by_earliest_arrival			\
                     .lower_bound((ppj_macro_local_n).earliest_job_release()); \
	     ppj_macro_local_it != jobs_by_earliest_arrival.end() 				\
	        && (ppj_macro_local_j = ppj_macro_local_it->second); 	\
	     ppj_macro_local_it++) \
		if (incomplete(ppj_macro_local_n, *ppj_macro_local_j))

// Iterate over all incomplete jobs that are released no later than ppju_macro_local_until
// For all the jobs that arrive after the earliest job release of a node n, and before the ppju_macro_local_until time,
// check if they are incomplete
#define foreach_possbly_pending_job_until(ppju_macro_local_n, ppju_macro_local_j, ppju_macro_local_until) 	\
	for (auto ppju_macro_local_it = jobs_by_earliest_arrival			\
                     .lower_bound((ppju_macro_local_n).earliest_job_release()); \
	     ppju_macro_local_it != jobs_by_earliest_arrival.end() 				\
	        && (ppju_macro_local_j = ppju_macro_local_it->second, ppju_macro_local_j->earliest_arrival() <= (ppju_macro_local_until)); 	\
	     ppju_macro_local_it++) \
		if (incomplete(ppju_macro_local_n, *ppju_macro_local_j))

// Iterare over all incomplete jobs that are certainly released no later than
// cpju_macro_local_until remove state HERE 
// for_each_possibly_pending_job also check if their latest arrival occurs before the cpju_macro_local_until
#define foreach_certainly_pending_job_until(cpju_macro_local_n, cpju_macro_local_j, cpju_macro_local_until) \
	foreach_possbly_pending_job_until(cpju_macro_local_n, cpju_macro_local_j, (cpju_macro_local_until)) \
		if (cpju_macro_local_j->latest_arrival() <= (cpju_macro_local_until))

			// returns true if there is certainly some pending job of higher
			// priority at the given time ready to be scheduled
			bool exists_certainly_released_higher_prio_job(
				const Node& n,
				const State& s,
				const Job<Time>& reference_job,
				Time at)
			{
				auto ts_min = s.earliest_finish_time();
				assert(at >= ts_min);

				DM("    ? " << __FUNCTION__ << " at " << at << ": "
				   << reference_job << std::endl);

				auto rel_min = n.earliest_job_release();

				// consider all possibly pending jobs and check if they have a higher prioirty than the 
				// reference job while also having no predecessor jobs pending.
				const Job<Time>* jp;
				foreach_certainly_pending_job_until(n, jp, at) {
					const Job<Time>& j = *jp;
					DM("        - considering " << j << std::endl);
					// skip reference job
					if (&j == &reference_job)
						continue;
					// ignore jobs that aren't yet ready, they have predecessor jobs
					if (!ready(n, j))
						continue;
					// check priority
					if (j.higher_priority_than(reference_job)) {
						DM("          => found one: " << j << " <<HP<< "
						   << reference_job << std::endl);
						return true;
					}
				}
				return false;
			}

			// Find the ealrliest possible job release of all jobs in a node except for the ignored job
			Time earliest_possible_job_release(
				const Node& n,
				const Job<Time>& ignored_job)
			{
				DM("      - looking for earliest possible job release starting from: "
				   << n.earliest_job_release() << std::endl);
				const Job<Time>* jp;
				foreach_possibly_pending_job(n, jp) {
					const Job<Time>& j = *jp;

					DM("         * looking at " << j << std::endl);

					// skip if it is the one we're ignoring
					if (&j == &ignored_job)
						continue;

					DM("         * found it: " << j.earliest_arrival() << std::endl);

					// it's incomplete and not ignored => found the earliest
					return j.earliest_arrival();
				}

				DM("         * No more future releases" << std::endl);

				return Time_model::constants<Time>::infinity();
			}

			// Ensures that time t, is smaller tthan the latest_permissible start time. Thei latest permissible
			// start time depends on the IIP methodology, if Null_IIP, infinity is released in which case
			// iip_eligible will always return true.
			bool iip_eligible(const Node &n, const State &s, const Job<Time> &j, Time t)
			{
				return !iip.can_block || t <= iip.latest_start(j, t, s, n);
			}

			// Ensure that there is no higher prioirty job that has certainly released
			bool priority_eligible(const Node& n, const State &s, const Job<Time> &j, Time t)
			{
				return !exists_certainly_released_higher_prio_job(n, s, j, t);
			}

			// returns true if there is certainly some pending job in this state
			bool exists_certainly_pending_job(const Node& n, const State &s)
			{
				auto ts_min = s.earliest_finish_time();

				const Job<Time>* jp;
				foreach_certainly_pending_job_until(n, jp, ts_min) {
					const Job<Time>& j = *jp;
					if ((!iip.can_block || priority_eligible(n, s, j, ts_min))
					    && iip_eligible(n, s, j, ts_min)) {
					    DM("\t\t\t\tcertainly released by "
					       << ts_min << ":" << j << std::endl);
						return true;
					}
				}
				return false;
			}

			//checks if the job j can be the next potential job for a state 
			bool potentially_next(const Node &n, const State &s, const Job<Time> &j)
			{
				auto t_latest = s.latest_finish_time();

				// if t_latest >=  j.earliest_arrival(), then the
				// job is trivially potentially next, so check the other case.

				if (t_latest < j.earliest_arrival()) {
					Time r = next_certain_job_release(n,s);
					// if something else is certainly released before j and IIP-
					// eligible at the time of certain release, then j can't
					// possibly be next
					if (r < j.earliest_arrival())
						return false;
				}
				return true;
			}

			// returns true if all predecessors of j have completed in state s
			bool ready(const Node &n, const Job<Time> &j)
			{
				const Job_precedence_set &preds =
					job_precedence_sets[index_of(j)];
				// check that all predecessors have completed
				return n.get_scheduled_jobs().includes(preds);
			}

			bool is_eligible_successor(const Node &n, const State &s, const Job<Time> &j)
			{
				// has been scheduled already, then false
				if (!incomplete(n, j)) {
					DM("  --> already complete"  << std::endl);
					return false;
				}
				// has predecessors yet to be scheduled, then false
				if (!ready(n, j)) {
					DM("  --> not ready"  << std::endl);
					return false;
				}
				// Job j is not the highespt prioirty job to be scheduled, hen false
				auto t_s = next_earliest_start_time(s, j);
				if (!priority_eligible(n, s, j, t_s)) {
					DM("  --> not prio eligible"  << std::endl);
					return false;
				}

				// if not potentially next, then false
				if (!potentially_next(n, s, j)) {
					DM("  --> not potentially next" <<  std::endl);
					return false;
				}

				// If not IIP eligible, then false
				if (!iip_eligible(n, s, j, t_s)) {
					DM("  --> not IIP eligible" << std::endl);
					return false;
				}

				// passes all the checks abovethen true
				return true;
			}

			void make_initial_node()
			{
				// construct initial node
				new_node();
			}

			// create a new node by adding an elemen to nodes, creating a new state and adding this new state to the node,
			// add this new node to the todo queue as it now a leaf node in the graph, yet to be processed. Add the node
			// along with its key to nodes_by_key. 
			template <typename... Args>
			Node& new_node(Args&&... args)
			{
				nodes.emplace_back(std::forward<Args>(args)...);
				Node_ref n_ref = --nodes.end();

				State &st = (n_ref->get_key()==0) ?
					new_state() :
					new_state(n_ref->finish_range());

				n_ref->add_state(&st);

				auto njobs = n_ref->get_scheduled_jobs().size();
				assert (
					(!njobs && num_nodes == 0) // initial state
				    || (njobs == current_job_count + 1) // normal State
				    || (njobs == current_job_count + 2 && aborted) // deadline miss
				);
				auto idx = njobs % num_todo_queues;
				todo[idx].push_back(n_ref);
				nodes_by_key.insert(std::make_pair(n_ref->get_key(), n_ref));
				num_nodes++;
				width = std::max(width, (unsigned long) todo[idx].size() - 1);
				return *n_ref;
			}

			// Create a new state, either the first intial state with no parameters or a new state in the graph with 
			// finish times ftimes. States created by adding an element to states
			template <typename... Args>
			State& new_state()
			{
				states.emplace_back();
				State_ref s_ref = --states.end();
				num_states++;
				return *s_ref;
			}

			template <typename... Args>
			State& new_state(Interval<Time> ftimes)
			{
				states.emplace_back(ftimes);
				State_ref s_ref = --states.end();
				num_states++;
				return *s_ref;
			}

			// The todo queue contains the nodes that have to be processed in order to build the graph further,
			// there are currently two todo queues being used, one is for the nodes that have 'current_job_count' 
			// scheduled jobs and the other queue is for nodes that have 'current_job_count+1' scheduled jobs. 
			// In the function below, the first queue with 'current_job_count' scheduled jobs is checked to see if 
			// it is empty or not. If it is empty then the second queue with 'current_job_count++' scheduled jobs 
			// is checked to see if it empty or not.If both queues are empty then all nodes have been processed and 
			// the function returns false

			bool not_done()
			{
				// if the curent queue is empty, move on to the next
				if (todo[todo_idx].empty()) {
					current_job_count++;
					todo_idx = current_job_count % num_todo_queues;
					return !todo[todo_idx].empty();
				} else
					return true;
			}

			const Node& next_node()
			{
				auto n = todo[todo_idx].front();
				return *n;
			}

			bool in_todo(Node_ref n)
			{
				for (auto it : todo)
					if (it == next_earliest_job_abortion)
						return true;
				return false;
			}

			void check_cpu_timeout()
			{
				if (timeout && get_cpu_time() > timeout) {
					aborted = true;
					timed_out = true;
				}
			}

			void check_depth_abort()
			{
				if (max_depth && current_job_count == max_depth
				    && todo[todo_idx].empty()) {
					aborted = true;
				}
			}

			// If the a node has been processed, it can be removed from the todo queue as the todo queue only contains
			// nodes that are yet to be processed.
			void done_with_current_node()
			{
				Node_ref n = todo[todo_idx].front();
				// remove from TODO list
				todo[todo_idx].pop_front();

#ifndef CONFIG_COLLECT_SCHEDULE_GRAPH
				// If we don't need to collect all states, we can remove
				// all those that we are done with, which saves a lot of
				// memory.

				// remove from lookup map
				auto matches = nodes_by_key.equal_range(n->get_key());
				bool deleted = false;
				for (auto it = matches.first; it != matches.second; it++)
					if (it->second == n) {
						nodes_by_key.erase(it);
						deleted = true;
						break;
					}
				assert(deleted);

				// delete from master sequence to free up memory
				assert(nodes.begin() == n);
				nodes.pop_front();
#endif
			}

			//////////////////////////////////////
			// Rules for finding the next state //
			//////////////////////////////////////

			Time next_earliest_start_time(const State &s, const Job<Time>& j)
			{
				// t_S in paper, see definition 6.
				return std::max(s.earliest_finish_time(), j.earliest_arrival());
			}

			Time next_earliest_finish_time(const State &s, const Job<Time>& j)
			{
				Time earliest_start = next_earliest_start_time(s, j);

				// e_k, equation 5
				return earliest_start + j.least_cost();
			}

			// For all the jobs sorted by their latest arrival, check if they have already been scheduled, 
			Time next_eligible_job_ready(const Node& n,const State& s) {
			    const Scheduled& already_scheduled = n.get_scheduled_jobs();

			    for (auto it = jobs_by_latest_arrival.begin(); it != jobs_by_latest_arrival.end(); it++) {
			        const Job<Time>& j = *(it->second);

			        // not relevant if already scheduled
			        if (!incomplete(already_scheduled, j))
			            continue;

			        auto t = std::max(j.latest_arrival(), s.latest_finish_time());

			        if (priority_eligible(n, s, j, t) && iip_eligible(n, s, j, t))
			            return j.latest_arrival();

			    }

			    return Time_model::constants<Time>::infinity();
			}

			// l_k, equation 6
			Time next_latest_finish_time(
				const Node &n,
				const State &s,
				const Job<Time>& j)
			{
				Time other_certain_start =
					next_certain_higher_priority_job_release(n, s, j);

				Time t_s = next_earliest_start_time(s, j);

				Time iip_latest_start = iip.latest_start(j, t_s, s, n);

				// t_s'
				// t_L is t_wc
				Time own_latest_start = std::max(s.latest_finish_time(),
                                                 next_eligible_job_ready(n,s));

				// t_R, t_I
				Time last_start_before_other = std::min(
					other_certain_start - Time_model::constants<Time>::epsilon(),
					iip_latest_start);

				return std::min(own_latest_start, last_start_before_other)
					   + j.maximal_cost();
			}

			Time next_earliest_job_abortion(const Abort_action<Time> &a)
			{
				return a.earliest_trigger_time() + a.least_cleanup_cost();
			}

			Time next_latest_job_abortion(const Abort_action<Time> &a)
			{
				return a.latest_trigger_time() + a.maximum_cleanup_cost();
			}

			Interval<Time> next_finish_times(const Node& n, const State &s, const Job<Time> &j)
			{
				auto i = index_of(j);

				if (abort_actions[i]) {
					// complicated case -- need to take aborts into account

					auto et = abort_actions[i]->earliest_trigger_time();

					// Rule: if we're certainly past the trigger, the job is
					//       completely skipped.

					if (s.earliest_finish_time() >= et)
						// job doesn't even start, is skipped immediately
						return s.finish_range();

					// Otherwise, it might start execution. Let's compute the
					// regular and aborted completion times.

					auto eft = next_earliest_finish_time(s, j);
					auto lft = next_latest_finish_time(n, s, j);

					auto eat = next_earliest_job_abortion(*abort_actions[i]);
					auto lat = next_latest_job_abortion(*abort_actions[i]);

					return Interval<Time>{
						std::min(eft, eat),
						std::min(lft, lat)
					};

				} else {
					// standard case -- this job is never aborted or skipped
					return Interval<Time>{
						next_earliest_finish_time(s, j),
						next_latest_finish_time(n, s, j)
					};
				}
			}

			void process_new_edge(
				const Node& from,
				const Node& to,
				const Job<Time>& j,
				const Interval<Time>& finish_range)
			{
				// update response times
				update_finish_times(j, finish_range);
				// update statistics
				num_edges++;
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
				edges.emplace_back(&j, &from, &to, finish_range);
#endif
			}

			// naive: no state merging
			void schedule_job(const Node &n, const State& s, const Job<Time> &j)
			{
				const Node& next =
					new_node(n, j, index_of(j),
					          next_finish_times(n, s, j),
					          earliest_possible_job_release(n, j));
				DM("      -----> N" << (nodes.end() - nodes.begin())
				   << std::endl);
				process_new_edge(n, next, j, next.finish_range());
			}

			// Start by making the intial node. While the graph has not finished building and is not aborted, we check to
			// see how the states within each node is manipulated
			void explore_naively()
			{
				make_initial_node();

				while (not_done() && !aborted) {
					const Node& n = next_node();

					DM("\n==================================================="
					   << std::endl);
					DM("Looking at: N"
					   << (todo[todo_idx].front() - nodes.begin() + 1)
					   << " " << n << std::endl);
					const auto *n_states = n.get_states();

					// for each state in the node n
					for(State *s : *n_states)
					{
						// Identify relevant interval for next job
						// relevant job buckets
						auto ts_min = s->earliest_finish_time();
						auto rel_min = n.earliest_job_release();
						auto t_l = std::max(next_eligible_job_ready(n,*s), s->latest_finish_time());

						Interval<Time> next_range{std::min(ts_min, rel_min), t_l};

						DM("ts_min = " << ts_min << std::endl << std::endl <<
						   "latest_finish = " <<s->latest_finish_time() << std::endl);
						DM("=> next range = " << next_range << std::endl);

						bool found_at_least_one = false;

						DM("\n---\nChecking for pending and later-released jobs:"
						   << std::endl);
						const Job<Time>* jp;
						foreach_possbly_pending_job_until(n, jp, next_range.upto()) {
							const Job<Time>& j = *jp;
							DM("+ " << j << std::endl);
							// if it can be scheduled next...
							if (is_eligible_successor(n, *s, j)) {
								DM("  --> can be next "  << std::endl);
								// create the relevant state and continue
								schedule_job(n, *s, j);
								found_at_least_one = true;
							}
						}

						DM("---\nDone iterating over all jobs." << std::endl);

						// check for a dead end
						if (!found_at_least_one &&
						    n.get_scheduled_jobs().size() != jobs.size()) {
							// out of options and we didn't schedule all jobs
							observed_deadline_miss = true;
							if (early_exit)
								aborted = true;
						}

						check_cpu_timeout();
						check_depth_abort();
					}
					done_with_current_node();
				}
			}


			void schedule(const Node& n, const State& s, const Job<Time> &j)
			{
				Interval<Time> finish_range = next_finish_times(n, s, j);

				auto k = n.next_key(j);

				auto r = nodes_by_key.equal_range(k);

				if (r.first != r.second) {
					Job_set sched_jobs{n.get_scheduled_jobs(), index_of(j)};

					for (auto it = r.first; it != r.second; it++) {
						Node &found = *it->second;
						Node_ref n_ref = it->second;

						// key collision if the job sets don't match exactly
						if (found.get_scheduled_jobs() != sched_jobs)
							continue;

						// cannot merge without loss of accuracy if the
						// intervals do not overlap

						if(!found.merge_states(finish_range))
						{
							DM("State not merged but added to the node");
							State &st = new_state(finish_range);
							n_ref->add_state(&st);
						}
						process_new_edge(n, found, j, finish_range);
						return;
					}
				}

				// If we reach here, we didn't find a match and need to create
				// a new state.
				DM("Creating a new node"<<std::endl);
				const Node& next =
					new_node(n, j, index_of(j),
					          finish_range,
					          earliest_possible_job_release(n, j));
				DM("      -----> N" << (nodes.end() - nodes.begin()) << " " <<(todo[todo_idx].front() - nodes.begin() + 1)
				   << std::endl);
				process_new_edge(n, next, j, finish_range);
			}

			void explore()
			{
				make_initial_node();

				while (not_done() && !aborted) {
					const Node& n = next_node();

					DM("\n==================================================="
					   << std::endl);
					DM("Looking at: N"
					   << (todo[todo_idx].front() - nodes.begin() + 1) 
					   << " " << n << std::endl);

					// Identify relevant interval for next job
					// relevant job buckets

					const auto *n_states = n.get_states();

					for(State *s: *n_states)
					{
						auto ts_min = s->earliest_finish_time();
						auto rel_min = n.earliest_job_release();
						auto t_l = std::max(next_eligible_job_ready(n,*s), s->latest_finish_time());

						Interval<Time> next_range{std::min(ts_min, rel_min), t_l};

						DM("ts_min = " << ts_min << std::endl <<
						   "rel_min = " << rel_min << std::endl  <<
						   "latest_finish = " << s->latest_finish_time() << std::endl);
						DM("=> next range = " << next_range << std::endl);

						bool found_at_least_one = false;

						DM("\n---\nChecking for pending and later-released jobs:"
						   << std::endl);
						const Job<Time>* jp;
						foreach_possbly_pending_job_until(n, jp, next_range.upto()) {
							const Job<Time>& j = *jp;
							DM("+ " << j << std::endl);
							// if it can be scheduled next...
							if (is_eligible_successor(n, *s, j)) {
								DM("  --> can be next "  << std::endl);
								// create the relevant state and continue
								schedule(n, *s, j);
								found_at_least_one = true;
							}
						}

						DM("---\nDone iterating over all jobs." << std::endl);

						// check for a dead end
						if (!found_at_least_one &&
						    n.get_scheduled_jobs().size() != jobs.size()) {
							// out of options and we didn't schedule all jobs
							observed_deadline_miss = true;
							if (early_exit)
								aborted = true;
							DM(":: Didn't find any possible successors." << std::endl);
						}

						check_cpu_timeout();
						check_depth_abort();
					}
					done_with_current_node();
				}
			}

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
			friend std::ostream& operator<< (std::ostream& out,
			                                 const State_space<Time, IIP>& space)
			{
					std::map<const Schedule_node<Time>*, unsigned int> node_id;
					unsigned int i = 1;
					out << "digraph {" << std::endl;
					for (const Schedule_node<Time>& n : space.get_nodes()) {
						node_id[&n] = i++;
						out << "\tN" << node_id[&n]
						    << "[label=\"N" << node_id[&n] << ": {";
						const auto *n_states = n.get_states();

						for(State *s: *n_states)
						{
							out << "["
						    	<< s->earliest_finish_time()
						    	<< ", "
						    	<< s->latest_finish_time()
						    	<< "]\\n";
						}
						out << "}"
						    << "\\nER=";
						if (n.earliest_job_release() ==
						    Time_model::constants<Time>::infinity()) {
							out << "N/A";
						} else {
							out << n.earliest_job_release();
						}
						out << "\"];"
							<< std::endl;
					}
					for (auto e : space.get_edges ()) {
						out << "\tN" << node_id[e.source]
						    << " -> "
						    << "N" << node_id[e.target]
						    << "[label=\""
						    << "T" << e.scheduled->get_task_id()
						    << " J" << e.scheduled->get_job_id()
						    << "\\nDL=" << e.scheduled->get_deadline()
						    << "\\nES=" << e.earliest_start_time()
 						    << "\\nLS=" << e.latest_start_time()
						    << "\\nEF=" << e.earliest_finish_time()
						    << "\\nLF=" << e.latest_finish_time()
						    << "\"";
						if (e.deadline_miss_possible()) {
							out << ",color=Red,fontcolor=Red";
						}
						out << ",fontsize=8" << "]"
						    << ";"
						    << std::endl;
						if (e.deadline_miss_possible()) {
							out << "N" << node_id[e.target]
								<< "[color=Red];"
								<< std::endl;
						}
					}
					out << "}" << std::endl;
				return out;
			}
#endif
		};

	}
}

#include "uni_iip/iip.hpp"

namespace std
{
	template<class Time> struct hash<NP::UniprocIIP::Schedule_node<Time>>
    {
		std::size_t operator()(NP::UniprocIIP::Schedule_node<Time> const& n) const
        {
            return n.get_key();
        }
    };
}


#endif