#include "doctest.h"

#include <iostream>
#include <sstream>

#include "io.hpp"
#include "global/space.hpp"
#include "interval.hpp"

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

	prob.processors_initial_state = std::vector<Interval<dtime_t>>(2, Interval<dtime_t>(0, 0));
	opts.be_naive = true;
	auto nspace2 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK_FALSE(nspace2->is_schedulable()); // ISSUE: true

	opts.be_naive = false;
	auto space2 = NP::Global::State_space<dtime_t>::explore(prob, opts);

	CHECK_FALSE(space2->is_schedulable()); // ISSUE: true

	prob.processors_initial_state = std::vector<Interval<dtime_t>>(3, Interval<dtime_t>(0, 0));
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

	prob.processors_initial_state = std::vector<Interval<dtime_t>>(2, Interval<dtime_t>(0, 0));
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

	prob.processors_initial_state = std::vector<Interval<dtime_t>>(3, Interval<dtime_t>(0, 0));
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

	prob.processors_initial_state = std::vector<Interval<dtime_t>>(1, Interval<dtime_t>(0, 0));
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

TEST_CASE("[global-prec] taskset-22 check 2-core pessimism (1)") {
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

// Job 0 is always dispatched first at time 0.
// Ideally, job 1 and job 2 would start right after job 0 is finished.
// However, job 3 will start in the meantime, blocking one of them for too long!
const std::string ts23_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,     0,           0,           0,       50,       50,       50,        0 \n"
"     1,     1,           0,           0,       10,       10,       60,        1 \n"
"     2,     2,           0,           0,       10,       10,       60,        2 \n"
"     3,     3,           0,           0,      100,      100,      200,        3 \n"
;

const std::string ts23_edges =
"From TID, From JID,   To TID,   To JID \n"
"      0,       0,       1,       1,    \n"
"      0,       0,       2,       2,    \n"
;

TEST_CASE("[global-prec] taskset-23 check 2-core optimism (1)") {
	auto dag_in = std::istringstream(ts23_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts23_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec, 2};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	CHECK(space->get_finish_times(3).min() == 100);
	delete space;
}

const std::string ts24_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,     0,           0,           0,       49,       50,       50,        0 \n"
"     1,     1,           0,           0,       10,       10,       60,        1 \n"
"     2,     2,           0,           0,       10,       10,       60,        2 \n"
"     3,     3,           0,           0,      100,      100,      200,        3 \n"
;

TEST_CASE("[global-prec] taskset-24 check 2-core optimism (2)") {
	auto dag_in = std::istringstream(ts23_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts24_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec, 2};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	CHECK(space->get_finish_times(3).min() == 100);
	delete space;
}

// The correct and only possible order on core 1 is J0 -> J68 -> J72 -> J64
// Core 2 is continuously occupied by T99J99
const std::string ts25_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     65,      0,           0,           0,        0,      100,       100,        0 \n"
"     65,     68,           0,           0,       50,       50,       150,        1 \n"
"     65,     72,           0,           0,       10,       10,       160,        2 \n"
"     65,     64,           0,           0,       10,       10,       180,        3 \n"
"     99,     99,           0,           0,      199,      199,       199,        0 \n"
;

const std::string ts25_edges =
"From TID, From JID,   To TID,   To JID \n"
"      65,        0,       65,       68 \n"
"      65,        0,       65,       72 \n"
;

TEST_CASE("[global-prec] taskset-25 check 2-core pessimism (2)") {
	auto dag_in = std::istringstream(ts25_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts25_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec, 2};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;
}


// In the PREcedence MOnster test cases, there are 5 important jobs:
// - T0J0 and T1J0 both start at time 0 because T0J0, T1J0, T2J0, and T3J0 are ready, and T0 and T1 have the smallest priority
// - T0J1 starts after T0J0
// - T1J1 starts after T1J0
// - T2J0 and T3J0 are the last two jobs that start
//
// If T2J0 and T3J0 would start before T0J1 or T1J1, then T0J1 or T1J1 would miss its deadline.
// Even though that is not possible, earlier versions of the SAG explored such cases due to pessimism.
const std::string premo_jobs1 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,      0,           0,           0,        10,       15,       15,        0 \n"
"     0,      1,           0,          10,        10,       15,       30,        0 \n"
"     1,      0,           0,           0,        10,       15,       15,        1 \n"
"     1,      1,           0,          10,        10,       15,       30,        1 \n"
"     2,      0,           0,           0,       100,      100,      130,        2 \n"
"     3,      0,           0,           0,       100,      100,      130,        3 \n"
;

const std::string premo_edges1 =
"From TID, From JID,   To TID,   To JID \n"
"       0,        0,        0,        1 \n"
"       1,        0,        1,        1 \n"
;

TEST_CASE("[global-prec] precedence monster (1)") {
	auto dag_in = std::istringstream(premo_edges1);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs1);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;
}

// This is an unschedulable variant of precedence monster because the priority of T1J1 is larger
const std::string premo_jobs2 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,      0,           0,           0,        10,       15,       15,        0 \n"
"     0,      1,           0,          10,        10,       15,       30,        0 \n"
"     1,      0,           0,           0,        10,       15,       15,        1 \n"
"     1,      1,           0,          10,        10,       15,       30,        3 \n"
"     2,      0,           0,           0,       100,      100,      130,        2 \n"
"     3,      0,           0,           0,       100,      100,      130,        2 \n"
;

TEST_CASE("[global-prec] precedence monster (2)") {
	auto dag_in = std::istringstream(premo_edges1);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs2);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// This is an unschedulable variant of precedence monster because the latest arrival of T0J1 is 11 rather than 10
const std::string premo_jobs3 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,      0,           0,           0,        10,       15,       15,        0 \n"
"     0,      1,           0,          11,        10,       15,       30,        0 \n"
"     1,      0,           0,           0,        10,       15,       15,        1 \n"
"     1,      1,           0,          10,        10,       15,       30,        1 \n"
"     2,      0,           0,           0,       100,      100,      130,        2 \n"
"     3,      0,           0,           0,       100,      100,      130,        3 \n"
;

TEST_CASE("[global-prec] precedence monster (3)") {
	auto dag_in = std::istringstream(premo_edges1);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs3);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// This is an unschedulable variant of precedence monster because T0J1 and T1J1 both need to wait
// on both T0J0 and T1J0
const std::string premo_edges4 =
"From TID, From JID,   To TID,   To JID \n"
"       0,        0,        0,        1 \n"
"       0,        0,        1,        1 \n"
"       1,        0,        0,        1 \n"
"       1,        0,        1,        1 \n"
;

TEST_CASE("[global-prec] precedence monster (4)") {
	auto dag_in = std::istringstream(premo_edges4);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs1);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	CHECK(space->get_finish_times(1).max() == 30);
	CHECK(space->get_finish_times(3).max() == 45);
	delete space;
}

// This is an unschedulable variant of precedence monster because T0J1 needs to wait on both T0J0 and T1J0
const std::string premo_edges5 =
"From TID, From JID,   To TID,   To JID \n"
"       0,        0,        0,        1 \n"
"       1,        0,        0,        1 \n"
"       1,        0,        1,        1 \n"
;

TEST_CASE("[global-prec] precedence monster (5)") {
	auto dag_in = std::istringstream(premo_edges5);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs1);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	CHECK(space->get_finish_times(1).max() == 30);
	CHECK(space->get_finish_times(3).max() == 45);
	delete space;
}

// This is a longer variant of precedence monster that should still be schedulable, and a weaker variant of (7)
const std::string premo_jobs6 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,      0,           0,           0,        10,       15,       15,        0 \n"
"     0,      1,           0,          10,        10,       15,       30,        0 \n"
"     0,      2,          15,          20,        10,       15,       45,        0 \n"
"     0,      3,          20,          30,        10,       15,       75,        0 \n"
"     1,      0,           0,           0,        10,       15,       15,        1 \n"
"     1,      1,          10,          10,        10,       15,       30,        1 \n"
"     1,      2,           0,           5,        10,       15,       75,        1 \n"
"     1,      3,          30,          30,        10,       15,       90,        1 \n"
"     2,      0,           0,           0,       100,      100,      190,        2 \n"
"     3,      0,           0,           0,       100,      100,      190,        3 \n"
;

const std::string premo_edges6 =
"From TID, From JID,   To TID,   To JID \n"
"       0,        0,        0,        1 \n"
"       0,        1,        0,        2 \n"
"       0,        2,        0,        3 \n"
"       1,        0,        1,        1 \n"
"       1,        1,        1,        2 \n"
"       1,        2,        1,        3 \n"
;

TEST_CASE("[global-prec] precedence monster (6)") {
	auto dag_in = std::istringstream(premo_edges6);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs6);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;
}

// This is a longer variant of precedence monster that should still be schedulable
const std::string premo_jobs7 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,      0,           0,           0,        10,       15,       15,        0 \n"
"     0,      1,           0,          10,        10,       15,       30,        0 \n"
"     0,      2,          15,          20,        10,       15,       45,        0 \n"
"     0,      3,          20,          30,        10,       15,       60,        0 \n"
"     1,      0,           0,           0,        10,       15,       15,        1 \n"
"     1,      1,          10,          10,        10,       15,       30,        1 \n"
"     1,      2,           0,           5,        10,       15,       45,        1 \n"
"     1,      3,          30,          30,        10,       15,       60,        1 \n"
"     2,      0,           0,           0,       100,      100,      160,        2 \n"
"     3,      0,           0,           0,       100,      100,      160,        3 \n"
;

const std::string premo_edges7 =
"From TID, From JID,   To TID,   To JID \n"
"       0,        0,        0,        1 \n"
"       0,        1,        0,        2 \n"
"       0,        2,        0,        3 \n"
"       1,        0,        1,        1 \n"
"       1,        1,        1,        2 \n"
"       1,        2,        1,        3 \n"
;

TEST_CASE("[global-prec] precedence monster (7)") {
	auto dag_in = std::istringstream(premo_edges7);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs7);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	// I believe this should be schedulable, but the SAG is currently not able to conclude this
	//CHECK(space->is_schedulable());
	// Variant (6) is a weaker variant of this test, which is deemed schedulable
	delete space;
}

// This variant is unschedulable because there is suspension
const std::string premo_edges8 =
"From TID, From JID,   To TID,   To JID, Sus. Min, Sus. Max \n"
"       0,        0,        0,        1         0,        1 \n"
"       1,        0,        1,        1         0,        0 \n"
;

TEST_CASE("[global-prec] precedence monster (8)") {
	auto dag_in = std::istringstream(premo_edges8);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs1);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// This is a schedulable variant with an extra core that is occupied by a long high-prio job
const std::string premo_jobs9 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,      0,           0,           0,        10,       15,       15,        0 \n"
"     0,      1,           0,          10,        10,       15,       30,        0 \n"
"     1,      0,           0,           0,        10,       15,       15,        1 \n"
"     1,      1,           0,          10,        10,       15,       30,        1 \n"
"     2,      0,           0,           0,       100,      100,      130,        2 \n"
"     3,      0,           0,           0,       100,      100,      130,        3 \n"
"     4,      0,           0,           0,       100,      100,      100,        0 \n"
;

TEST_CASE("[global-prec] precedence monster (9)") {
	auto dag_in = std::istringstream(premo_edges1);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs9);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 3 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	// I think this should be schedulable, but the SAG currently does not conclude this
	//CHECK(space->is_schedulable());
	delete space;
}

// This is a schedulable variant where T1J0 is split into two jobs and T1J2 has a redundant precedence constraint
const std::string premo_jobs10 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,      0,           0,           0,         5,        8,       15,        0 \n"
"     0,      1,           3,           5,         5,        7,       15,        0 \n"
"     0,      2,           0,          10,        10,       15,       30,        0 \n"
"     1,      0,           0,           0,        10,       15,       15,        1 \n"
"     1,      1,           0,          10,        10,       15,       45,        1 \n"
"     2,      0,           0,           0,       100,      100,      130,        2 \n"
"     3,      0,           0,           0,       100,      100,      145,        3 \n"
;

const std::string premo_edges10 =
"From TID, From JID,   To TID,   To JID \n"
"       0,        0,        0,        1 \n"
"       0,        1,        0,        2 \n"
"       0,        0,        0,        2 \n"
"       1,        0,        1,        1 \n"
;

TEST_CASE("[global-prec] precedence monster (10)") {
	auto dag_in = std::istringstream(premo_edges10);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs10);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };
	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->get_finish_times(0).max() == 8);
	CHECK(space->get_finish_times(1).max() == 15);
	CHECK(space->get_finish_times(2).max() == 30);
	CHECK(space->get_finish_times(3).max() == 15);
	CHECK(space->get_finish_times(4).max() <= 45); // Should be 30, but we are not accurate enough yet
	CHECK(space->get_finish_times(5).max() == 130);
	CHECK(space->get_finish_times(6).max() == 130);
	CHECK(space->is_schedulable());
	delete space;
}

// This is an unschedulable variant because T3J0 has a priority of 0 instead of 3
const std::string premo_jobs11 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,      0,           0,           0,        10,       15,       15,        0 \n"
"     0,      1,           0,          10,        10,       15,       30,        0 \n"
"     1,      0,           0,           0,        10,       15,       15,        1 \n"
"     1,      1,           0,          10,        10,       15,       30,        1 \n"
"     2,      0,           0,           0,       100,      100,      130,        2 \n"
"     3,      0,           0,           0,       100,      100,      130,        0 \n"
;

TEST_CASE("[global-prec] precedence monster (11)") {
	auto dag_in = std::istringstream(premo_edges1);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs11);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	CHECK(space->get_finish_times(3).max() > 100);
	delete space;
}

// This is a schedulable variant where some other unrelated jobs are certainly finished
// before the interesting jobs start, and some of these early jobs are predecessors
// of the important jobs.
const std::string premo_jobs12 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     0,      0,         100,         100,        10,       15,      115,        0 \n"
"     0,      1,         100,         110,        10,       15,      130,        0 \n"
"     1,      0,         100,         100,        10,       15,      115,        1 \n"
"     1,      1,         100,         110,        10,       15,      130,        1 \n"
"     2,      0,         100,         100,       100,      100,      230,        2 \n"
"     3,      0,         100,         100,       100,      100,      230,        3 \n"
"     4,      0,           0,          10,        10,       15,      100,        0 \n"
"     4,      1,           5,          20,        10,       15,      100,        0 \n"
"     4,      2,          15,          75,        10,       15,      100,        0 \n"
;

const std::string premo_edges12 =
"From TID, From JID,   To TID,   To JID \n"
"       0,        0,        0,        1 \n"
"       1,        0,        1,        1 \n"
"       4,        0,        0,        0 \n"
"       4,        1,        1,        1 \n"
"       4,        2,        3,        0 \n"
;

TEST_CASE("[global-prec] precedence monster (12)") {
	auto dag_in = std::istringstream(premo_edges12);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(premo_jobs12);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{ jobs, prec, 2 };

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;
}