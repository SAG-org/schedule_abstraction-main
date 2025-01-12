#ifndef RECONFIGURATION_AGENT_H
#define RECONFIGURATION_AGENT_H

#include "attachment.hpp"
#include "global/state.hpp"
#include "jobs.hpp"

namespace NP::Reconfiguration {
	template <class Time> class Agent {
	public:
		virtual ~Agent() = default;

		virtual Attachment* create_initial_node_attachment() { return nullptr; }

		virtual Attachment* create_next_node_attachment(
				const Global::Schedule_node<Time> &parent_node, Job<Time> next_job
		) { return nullptr; }

		virtual void merge_node_attachments(
				Global::Schedule_node<Time>* destination_node,
				const Global::Schedule_node<Time> &parent_node,
				Job<Time> next_job
		) { }

		virtual void missed_deadline(const Global::Schedule_node<Time> &failed_node, const Job<Time> &late_job) { }

		virtual void encountered_dead_end(const Global::Schedule_node<Time> &dead_node) { }

		virtual void mark_as_leaf_node(const Global::Schedule_node<Time> &leaf_node) { }

		virtual bool allow_merge(
				const Global::Schedule_node<Time> &parent_node,
				const Job<Time> &taken_job,
				const Global::Schedule_node<Time> &destination_node
		) {
			return true;
		}

		virtual bool should_explore(const Global::Schedule_node<Time> &node) { return true; }
	};
}

#endif
