#include "doctest.h"

#include <iostream>
#include <sstream>

#include "io.hpp"
#include "global/space.hpp"

const std::string ts1_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      1,      1,           0,        6000,     5000,     9000,    30000,    30000\n"
"      1,      2,           0,        6000,     3000,     6000,    30000,    30000\n"
"      1,      3,           0,        6000,     2000,    15000,    30000,    30000\n"
"      2,      1,           0,        3000,     5000,    10000,    30000,    30000\n"
"      2,      2,           0,        3000,     3000,     5000,    30000,    30000\n";

const std::string ts1_edges =
"From TID, From JID,   To TID,   To JID\n"
"       1,        1,        1,        2\n"
"       1,        1,        1,        3\n"
"       2,        1,        2,        2\n";

const std::string ts2_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      1,      1,           0,           0,     2000,     5000,    40000,    40000\n"
"      1,      2,           0,           0,     3000,    10000,    40000,    40000\n"
"      1,      3,           0,           0,     3000,    10000,    40000,    40000\n"
"      1,      4,           0,           0,     3000,    10000,    40000,    40000\n"
"      1,      5,           0,           0,     5000,    15000,    40000,    40000\n"
"      2,      1,           0,       40000,        0,    10000,    80000,    80000\n"
"      1,     11,       40000,       40000,     2000,     5000,    80000,    80000\n"
"      1,     12,       40000,       40000,     3000,    10000,    80000,    80000\n"
"      1,     13,       40000,       40000,     3000,    10000,    80000,    80000\n"
"      1,     14,       40000,       40000,     3000,    10000,    80000,    80000\n"
"      1,     15,       40000,       40000,     5000,    15000,    80000,    80000\n";

const std::string ts2_edges =
"From TID, From JID,   To TID,   To JID\n"
"       1,        1,        1,        2\n"
"       1,        1,        1,        3\n"
"       1,        1,        1,        4\n"
"       1,        2,        1,        5\n"
"       1,        3,        1,        5\n"
"       1,        4,        1,        5\n"
"       1,       11,        1,       12\n"
"       1,       11,        1,       13\n"
"       1,       11,        1,       14\n"
"       1,       12,        1,       15\n"
"       1,       13,        1,       15\n"
"       1,       14,        1,       15\n";

const std::string ts3_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,          10,          10,       80,       80,      110,        2\n"
"      1,      0,         200,         200,       20,       20,     8000,        4\n"
"      2,      0,         200,         200,       20,       20,     8000,        5\n"
"      3,      0,         200,         200,       40,       40,     8000,        3\n"
"      0,      1,         210,         210,       80,       80,     310,         2\n";

const std::string ts3_edges =
"From TID, From JID,   To TID,   To JID\n"
"       1,        0,        2,        0\n"
"       2,        0,        3,        0\n";

TEST_CASE("[global-prec] taskset-1") {
	auto dag_in = std::istringstream(ts1_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts1_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};
	NP::Analysis_options opts;

	prob.num_processors = 2;
	opts.be_naive = true;
	auto nspace2 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK_FALSE(nspace2->is_schedulable()); // ISSUE: true

	opts.be_naive = false;
	auto space2 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK_FALSE(space2->is_schedulable()); // ISSUE: true

	prob.num_processors = 3;
	opts.be_naive = true;
	auto nspace3 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK(nspace3->is_schedulable());  // ISSUE: false

	opts.be_naive = false;
	auto space3 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK(space3->is_schedulable()); // ISSUE: false

	for (const NP::Job<dtime_t>& j : jobs) {
		CHECK(nspace3->get_finish_times(j) == space3->get_finish_times(j));
		CHECK(nspace3->get_finish_times(j).from() != 0);  // ISSUE: 0
	}

	delete nspace2;
	delete nspace3;
	delete space2;
	delete space3;
}

TEST_CASE("[global-prec] taskset-2") {
	auto dag_in = std::istringstream(ts2_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts2_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};
	NP::Analysis_options opts;

	prob.num_processors = 2;
	opts.be_naive = true;
	auto nspace2 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK(nspace2->is_schedulable()); // ISSUE: false

	opts.be_naive = false;
	auto space2 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK(space2->is_schedulable()); // ISSUE: false

	for (const NP::Job<dtime_t>& j : jobs) {
		CHECK(nspace2->get_finish_times(j) == space2->get_finish_times(j));
		if (j.least_exec_time() != 0)
		  CHECK(nspace2->get_finish_times(j).from() != 0);  // ISSUE: 0
	}

	prob.num_processors = 3;
	opts.be_naive = true;
	auto nspace3 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK(nspace3->is_schedulable());  // ISSUE: false

	opts.be_naive = false;
	auto space3 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK(space3->is_schedulable());  // ISSUE: false

	for (const NP::Job<dtime_t>& j : jobs) {
		CHECK(nspace3->get_finish_times(j) == space3->get_finish_times(j));
		if (j.least_exec_time() != 0)
		  CHECK(nspace3->get_finish_times(j).from() != 0);  // ISSUE: 0
	}

	delete nspace2;
	delete nspace3;
	delete space2;
	delete space3;
}

TEST_CASE("[global-prec] taskset-3") {
	auto dag_in = std::istringstream(ts3_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts3_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};
	NP::Analysis_options opts;

	prob.num_processors = 1;
	opts.be_naive = false;
	auto space = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK(space->is_schedulable());

	delete space;
}

const std::string ts5_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           0,       10,       10,      10,        0\n"
"      1,      1,         0,           0,       10,       10,      20,        1\n"
"      2,      2,         0,           0,        5,        5,      26,        2\n";

// This problem is infeasible since the first precedence constraint has a worst-case suspension of 100
const std::string ts5_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max\n"
"       0,        0,        1,        1,            0,        100\n"
"       0,        0,        2,        2\n"
;

TEST_CASE("[global-prec] taskset-5 false negative") {
	auto dag_in = std::istringstream(ts5_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);
	auto in = std::istringstream(ts5_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);
	REQUIRE(prec[0].get_maxsus() == 100);
	NP::Scheduling_problem<dtime_t> prob{jobs, prec};
	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// The job dispatch order without deadline misses is J0 -> J1 -> J2
// But, in an execution scenario where the suspension from J0 to J1 is 1, J2 would go before J1, causing J1 to miss its deadline.
// Therefor, it should be unschedulable.
const std::string ts6_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           0,       10,       10,      10,        0\n"
"      1,      1,         0,           0,       10,       10,      21,        1\n"
"      2,      2,         0,           0,        5,        5,      26,        2\n";

const std::string ts6_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max\n"
"       0,        0,        1,        1            0,        1\n"
"       0,        0,        2,        2\n";

TEST_CASE("[global-prec] taskset-6 false negative") {
	auto dag_in = std::istringstream(ts6_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);
	auto in = std::istringstream(ts6_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);
	NP::Scheduling_problem<dtime_t> prob{jobs, prec};
	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// Core 2 is continuously occupied by T99J99, so only core 1 is interesting
// The only possible job ordering on core 1 is:
// - J68 starts at time 0
// - J72 starts right after J68 is finished somewhere between [10, 50]
// - J69 starts right after J72 is finished somewhere between [20, 60]
// - J64 starts right after J69 is finished somewhere between [30, 70]
// - J44 starts right after J64 is finished somewhere between [40, 80]
const std::string ts21_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     65,     68,           0,           0,       10,       50,       50,        0 \n"
"     65,     72,           0,           0,       10,       10,       60,        1 \n"
"     65,     69,           0,           0,       10,       10,       81,        2 \n"
"     65,     64,           0,           0,       10,       10,       81,        3 \n"
"     65,     44,           0,           0,       50,       50,      130,        4 \n"
"     99,     99,           0,           0,      200,      200,      200,        0 \n"
;

const std::string ts21_edges =
"From TID, From JID,   To TID,   To JID \n"
"      65,       68,       65,       72 \n"
"      65,       72,       65,       69 \n"
"      65,       69,       65,       44 \n"
"      65,       68,       65,       64 \n"
;

TEST_CASE("[global-prec] taskset-21 check transitivity pessimism (5)") {
	auto dag_in = std::istringstream(ts21_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);
	auto in = std::istringstream(ts21_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);
	NP::Scheduling_problem<dtime_t> prob{jobs, prec, 2};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	// By adding a suspension delay of 21 time units between J68 and J64, it becomes possible that J44 is ready *before* J64,
	// causing J64 to miss its deadline.
	prob.prec[3] = NP::Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[3].get_id(), Interval<dtime_t>(0, 21));
	validate_prec_cstrnts(prob.prec, prob.jobs);
	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// The correct and only possible order on core 1 is J68 -> J72 -> J64
// Core 2 is continuously occupied by T99J99
const std::string ts22_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     65,     68,           0,           0,       10,       50,       50,        0 \n"
"     65,     72,           0,           0,       10,       10,       60,        1 \n"
"     65,     64,           0,           0,       10,       10,       80,        3 \n"
"     99,     99,           0,           0,       99,       99,       99,        0 \n"
;

const std::string ts22_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max \n"
"      65,       68,       65,       72,           0,        0 \n"
;

TEST_CASE("[global-prec] taskset-22 check 2-core pessimism") {
	auto dag_in = std::istringstream(ts22_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts22_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec, 2};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	prob.prec[0] = NP::Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>(0, 1));
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}
