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

// Should use 2 processors, and fails when used without precedence constraints: T2 could arrive before T0/1, and occupy a processor until after their deadline
const std::string ts4_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           5,       10,       10,      15,        2\n"
"      1,      0,         0,           5,       10,       10,      15,        4\n"
"      2,      0,         0,           0,      100,      100,     200,        5\n";

// These precedence constraints are not good because T1 has to wait for T0, causing it to miss its deadline
const std::string ts4_edges_bad1 =
"From TID, From JID,   To TID,   To JID\n"
"       0,        0,        1,        0\n"
"       0,        0,        2,        0\n";

// These precedence constraints will work because T1 can start as soon as T0 starts, and T2 needs to wait
const std::string ts4_edges_good =
"From TID, From JID,   To TID,   To JID, 	Sus Min,	Sus Max,	Signal At\n"
"       0,        0,        1,        0,		0,			0,			s\n"
"       0,        0,        2,        0,		0,			0,			f\n";

// These precedence constraints will NOT work because T1 is suspended 1 time unit after T0 starts, causing it to miss its deadline by 1 time unit
const std::string ts4_edges_bad2 =
"From TID, From JID,   To TID,   To JID, 	Sus Min,	Sus Max,	Signal At\n"
"       0,        0,        1,        0,		0,			1,			s\n"
"       0,        0,        2,        0,		0,			0,			f\n";

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

TEST_CASE("[global-prec] taskset-4 with signal-at-start") {
	auto dag_bad1 = std::istringstream(ts4_edges_bad1);
	auto dag_bad2 = std::istringstream(ts4_edges_bad2);
	auto dag_good = std::istringstream(ts4_edges_good);
	auto prec_bad1 = NP::parse_precedence_file<dtime_t>(dag_bad1);
	auto prec_bad2 = NP::parse_precedence_file<dtime_t>(dag_bad2);
	auto prec_good = NP::parse_precedence_file<dtime_t>(dag_good);

	auto in = std::istringstream(ts4_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob_bad0{jobs, {}};
	NP::Scheduling_problem<dtime_t> prob_bad1{jobs, prec_bad1};
	NP::Scheduling_problem<dtime_t> prob_bad2{jobs, prec_bad2};
	NP::Scheduling_problem<dtime_t> prob_good{jobs, prec_good};
	NP::Analysis_options opts;

	prob_bad0.num_processors = 2;
	prob_bad1.num_processors = 2;
	prob_bad2.num_processors = 2;
	prob_good.num_processors = 2;
	auto space_bad0 = NP::Global::State_space<dtime_t>::explore(prob_bad0, opts);
	auto space_bad1 = NP::Global::State_space<dtime_t>::explore(prob_bad1, opts);
	auto space_bad2 = NP::Global::State_space<dtime_t>::explore(prob_bad2, opts);
	auto space_good = NP::Global::State_space<dtime_t>::explore(prob_good, opts);

	CHECK_FALSE(space_bad0->is_schedulable());
	CHECK_FALSE(space_bad1->is_schedulable());
	CHECK_FALSE(space_bad2->is_schedulable());
	CHECK(space_good->is_schedulable());

	delete space_bad0;
	delete space_bad1;
	delete space_bad2;
	delete space_good;
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

TEST_CASE("[global-prec] taskset-5 regression test for false schedulable conclusion (1)") {
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

// The order without deadline misses is J0 -> J1 -> J2
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

TEST_CASE("[global-prec] taskset-6 regression test for false schedulable conclusion (2)") {
	auto dag_in = std::istringstream(ts6_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts6_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});

	CHECK(!space->is_schedulable());

	delete space;
}

// There are 2 orders in which the jobs can be dispatched.
// In both cases, J0 starts at time 0 and finishes at time 10.
// J0 -> J1 -> J2:
//  - When J1 goes before J2, the suspension from J0 to J1 can be at most 5, since J2 would go first otherwise
//  - So J1 starts no later than time 15, and finishes no later than time 25
//  - J2 will start no later than time 25, and finishes no later than time 35
// J0 -> J2 -> J1:
//  - J2 arrives no later than time 15, so it finishes no later than time 25
//  - J1 starts no later than time 25, and finishes no later than time 35
const std::string ts7_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           0,       10,       10,      10,        0   \n"
"      1,      1,         0,           0,       10,       10,      35,        1   \n"
"      2,      2,        10,          15,       10,       10,      35,        2   \n";

const std::string ts7_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max \n"
"       0,        0,        1,        1            0,       15 \n"
"       0,        0,        2,        2\n";

TEST_CASE("[global-prec] taskset-7 check suspension pessimism (1)") {
	auto dag_in = std::istringstream(ts7_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts7_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	prob.prec[1] = NP::Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>(0, 16));
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// The possible dispatch orderings are:
//  - J0 -> J1 -> J2 -> J3: this happens when J1 is dispatched exactly at time 15,
//    hence J0 must have finished at time 10 since the minimum suspension between J0 and J1 is 5.
//    J1 finishes at time 15 + 10 = 25, and the maximum suspension between J0 and J2 is 15,
//    hence J2 starts no later than time max(25, 10 + 15) = 25, and finishes at time 35.
//    Finally, J3 starts at time 35 and finishes at time 45.
//  - J0 -> J3 -> J1 -> J2: this happens when J1 is not ready at time 15, so J3 is dispatched somewhere
//    between time 10 and 15, and finishes no later than time 25.
//    J1 starts at time 25 and finishes at time 35.
//    J2 starts at time 35 and finishes at time 45.
const std::string ts8_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           0,       10,       15,      15,        0   \n"
"      1,      1,         0,           0,       10,       10,      35,        1   \n"
"      2,      2,        10,          15,       10,       10,      45,        2   \n"
"      3,      3,        10,          15,       10,       10,      45,        3   \n"
;

const std::string ts8_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max \n"
"       0,        0,        1,        1            5,       10 \n"
"       0,        0,        2,        2,          10,       15 \n";

TEST_CASE("[global-prec] taskset-8 check suspension pessimism (2)") {
	auto dag_in = std::istringstream(ts8_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts8_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;
}

// The possible dispatch orderings are:
//  - J0 -> J4 -> J1 -> J2 -> J3: this happens when J1 is dispatched exactly at time 15,
//    hence J0 must have finished at time 10 since the minimum suspension between J0 and J1 is 5.
//    J1 finishes at time 15 + 10 = 25, and the maximum suspension between J0 and J2 is 15,
//    hence J2 starts no later than time max(25, 10 + 15) = 25, and finishes at time 35.
//    Finally, J3 starts at time 35 and finishes at time 45.
//  - J0 -> J4 -> J3 -> J1 -> J2: this happens when J1 is not ready at time 15, so J3 is dispatched somewhere
//    between time 10 and 15, and finishes between time 20 and 25.
//    J1 starts at time 25 and finishes at time 35.
//    J2 starts at time 35 and finishes at time 45.
const std::string ts9_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           0,        9,       14,      14,        0   \n"
"      1,      1,         0,           0,       10,       10,      35,        1   \n"
"      2,      2,        10,          15,       10,       10,      45,        2   \n"
"      3,      3,        10,          15,       10,       10,      45,        3   \n"
"      4,      4,         0,           0,        1,        1,      15,        0   \n"
;

const std::string ts9_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max \n"
"       0,        0,        1,        1            6,       11 \n"
// The next constraint should be redundant, but the analysis is not smart enough to see this...
"       0,        0,        2,        2,          11,       16 \n"
"       4,        4,        2,        2,          10,       15 \n"
"       0,        0,        4,        4\n"
;

TEST_CASE("[global-prec] taskset-9 check suspension pessimism (3)") {
	auto dag_in = std::istringstream(ts9_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts9_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});

	CHECK(space->is_schedulable());

	delete space;
}

const std::string ts10_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           0,       10,       10,      10,        0\n"
"      1,      1,         0,           0,       10,       10,      20,        1\n"
"      2,      2,         0,           0,        5,        5,      26,        2\n";

// This problem is infeasible since the first precedence constraint has a worst-case suspension of 100
const std::string ts10_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max,   Signal at \n"
"       0,        0,        1,        1,            0,        100,        s \n"
"       0,        0,        2,        2\n"
;

TEST_CASE("[global-prec] taskset-10 regression test for false start-to-start schedulable conclusion (1)") {
	auto dag_in = std::istringstream(ts10_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts10_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// The order without deadline misses is J0 -> J1 -> J2
// But, in an execution scenario where the suspension from J0 to J1 is 11, J2 would go before J1, causing J1 to miss its deadline.
// Therefor, it should be unschedulable.
const std::string ts11_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           0,       10,       10,      10,        0\n"
"      1,      1,         0,           0,       10,       10,      21,        1\n"
"      2,      2,         0,           0,        5,        5,      26,        2\n";

const std::string ts11_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"       0,        0,        1,        1            0,       11,         s \n"
"       0,        0,        2,        2\n";

TEST_CASE("[global-prec] taskset-11 regression test for false start-to-start schedulable conclusion (2)") {
	auto dag_in = std::istringstream(ts11_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts11_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});

	CHECK(!space->is_schedulable());

	delete space;
}

// There are 2 orders in which the jobs can be dispatched.
// In both cases, J0 starts at time 0 and finishes at time 10.
// J0 -> J1 -> J2:
//  - When J1 goes before J2, the suspension from J0 to J1 can be at most 15, since J2 would go first otherwise
//  - So J1 starts no later than time 15, and finishes no later than time 25
//  - J2 will start no later than time 25, and finishes no later than time 35
// J0 -> J2 -> J1:
//  - J2 arrives no later than time 15, so it finishes no later than time 25
//  - J1 starts no later than time 25, and finishes no later than time 35
const std::string ts12_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           0,       10,       10,      10,        0   \n"
"      1,      1,         0,           0,       10,       10,      35,        1   \n"
"      2,      2,        10,          15,       10,       10,      35,        2   \n";

const std::string ts12_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"       0,        0,        1,        1            0,       25,         s \n"
"       0,        0,        2,        2\n";

TEST_CASE("[global-prec] taskset-12 check start-to-start suspension pessimism (1)") {
	auto dag_in = std::istringstream(ts12_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts12_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	prob.prec[1] = NP::Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>(0, 26), NP::start_to_start);
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// The possible dispatch orderings are:
//  - J0 -> J1 -> J2 -> J3: this happens when J1 is dispatched exactly at time 15,
//    hence J0 must have been released at time 0.
//    J1 finishes at time 15 + 10 = 25, and the maximum suspension between J0 and J2 is 25,
//    hence J2 starts no later than time max(25, 0 + 25) = 25, and finishes at time 35.
//    Finally, J3 starts at time 35 and finishes at time 45.
//  - J0 -> J3 -> J1 -> J2: this happens when J1 is not ready at time 15, so J3 is dispatched somewhere
//    between time 10 and 15, and finishes no later than time 25.
//    J1 starts at time 25 and finishes at time 35.
//    J2 starts at time 35 and finishes at time 45.
const std::string ts13_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           5,       10,       10,      15,        0   \n"
"      1,      1,         0,           0,       10,       10,      35,        1   \n"
"      2,      2,        10,          15,       10,       10,      45,        2   \n"
"      3,      3,        10,          15,       10,       10,      45,        3   \n"
;

const std::string ts13_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"       0,        0,        1,        1           15,       20,         s \n"
"       0,        0,        2,        2,          20,       25,         s \n";

TEST_CASE("[global-prec] taskset-13 check start-to-start suspension pessimism (2)") {
	auto dag_in = std::istringstream(ts13_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts13_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;
}

// The possible dispatch orderings are:
//  - J0 -> J1 -> J2 -> J3: this happens when J1 is dispatched exactly at time 15,
//    hence J0 must have been released at time 0.
//    J1 finishes at time 15 + 10 = 25, and the maximum suspension between J0 and J2 is 25,
//    hence J2 starts no later than time max(25, 0 + 25) = 25, and finishes at time 35.
//    Finally, J3 starts at time 35 and finishes at time 45.
//  - J0 -> J3 -> J1 -> J2: this happens when J1 is not ready at time 15, so J3 is dispatched somewhere
//    between time 10 and 15, and finishes no later than time 25.
//    J1 starts at time 25 and finishes at time 35.
//    J2 starts at time 35 and finishes at time 45.
const std::string ts14_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"      0,      0,         0,           5,       10,       10,      15,        0   \n"
"      1,      1,         0,           0,       10,       10,      35,        1   \n"
"      2,      2,        10,          15,       10,       10,      45,        2   \n"
"      3,      3,        10,          15,       10,       10,      45,        3   \n"
;

const std::string ts14_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"       0,        0,        1,        1           15,       20,         s \n"
"       0,        0,        2,        2,          10,       15,         f \n";

TEST_CASE("[global-prec] taskset-14 check mixed start-to-start and finish-to-start suspension pessimism (1)") {
	auto dag_in = std::istringstream(ts14_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts14_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	prob.prec[1] = NP::Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(9, 15));
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

const std::string ts15_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"       0,        0,        1,        1            5,       10,         f \n"
"       0,        0,        2,        2,          20,       25,         s \n";

TEST_CASE("[global-prec] taskset-15 check mixed start-to-start and finish-to-start suspension pessimism (2)") {
	auto dag_in = std::istringstream(ts15_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts14_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	prob.prec[1] = NP::Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(19, 25), NP::start_to_start);
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// The correct and only possible order on thread 1 is J68 -> J72 -> J64
// Thread 2 is continuously occupied by T99J99
const std::string ts16_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     65,     68,           0,           0,       10,       50,       50,        0 \n"
"     65,     72,           0,           0,       10,       10,       60,        1 \n"
"     65,     64,           0,           0,       10,       10,       80,        3 \n"
"     99,     99,           0,           0,       99,       99,       99,        0 \n"
;

const std::string ts16_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"      65,       68,       65,       72,           0,        0,         f \n"
"      65,       68,       65,       64,           0,        0,         s \n"
;

TEST_CASE("[global-prec] taskset-16 check 2-core pessimism") {
	auto dag_in = std::istringstream(ts16_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts16_jobs);
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

// Thread 2 is continuously occupied by T99J99, so only thread 1 is interesting
// There are 2 possible job orderings for thread 1:
//  - J68 -> J72 -> J69 -> J64 -> J44
//  - J68 -> J72 -> J69 -> J44 -> J64 (only possible when J68 takes less than 41 time units and J64 has long suspension)
const std::string ts17_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     65,     68,           0,           0,       10,       50,       50,        0 \n"
"     65,     72,           0,           0,       10,       10,       60,        1 \n"
"     65,     69,           0,           0,       10,       10,       70,        2 \n"
"     65,     64,           0,           0,       10,       10,       80,        3 \n"
"     65,     44,           0,           0,       10,       10,       90,        4 \n"
"     99,     99,           0,           0,       99,       99,       99,        0 \n"
;

const std::string ts17_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"      65,       68,       65,       72,           0,        0,         f \n"
"      65,       72,       65,       69,           0,        0,         f \n"
"      65,       69,       65,       44,           0,        0,         s \n"
"      65,       68,       65,       64,           0,       61,         s \n"
;

TEST_CASE("[global-prec] taskset-17 check transitivity pessimism (1)") {
	auto dag_in = std::istringstream(ts17_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts17_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec, 2};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	prob.prec[3] = NP::Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[3].get_id(), Interval<dtime_t>(0, 62), NP::start_to_start);
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// Thread 2 is continuously occupied by T99, so only thread 1 is interesting
// Thread 1:
// - first job is 68
// - second job should be 72, but the analysis hallucinates that it can also be 64, which is fine
// - third job should be 69, but hallucination is possible, which is fine
// - fourth job must be 64, unless 64 was already dispatched/hallunicated
// - last job must be 44
const std::string ts18_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"     65,     68,           0,           0,       10,       50,       50,        0 \n"
"     65,     72,           0,           0,       10,       10,       70,        1 \n"
"     65,     69,           0,           0,       10,       10,       80,        2 \n"
"     65,     64,           0,           0,       10,       10,       81,        3 \n"
"     65,     44,           0,           0,       10,       50,      130,        4 \n"
"     99,     10,           0,           0,        9,        9,       99,        0 \n"
"     99,     11,           9,           9,        9,        9,       99,        0 \n"
"     99,     12,          18,          18,        9,        9,       99,        0 \n"
"     99,     13,          27,          27,        9,        9,       99,        0 \n"
"     99,     14,          36,          36,        9,        9,       99,        0 \n"
"     99,     15,          45,          45,        9,        9,       99,        0 \n"
"     99,     16,          54,          54,        9,        9,       99,        0 \n"
"     99,     17,          63,          63,        9,        9,       99,        0 \n"
"     99,     18,          72,          72,        9,        9,       99,        0 \n"
"     99,     19,          81,          81,        9,        9,       99,        0 \n"
"     99,     20,          90,          90,        9,        9,      110,        0 \n" // this last deadline needs to be pessimistic, for some reason
;

const std::string ts18_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"      65,       68,       65,       72,           0,        0,         f \n"
"      65,       72,       65,       69,           0,        0,         f \n"
"      65,       69,       65,       44,           0,        0,         s \n"
"      65,       68,       65,       64,           0,        0,         s \n"
;

TEST_CASE("[global-prec] taskset-18 check transitivity pessimism (2)") {
	auto dag_in = std::istringstream(ts18_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts18_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec, 2};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	prob.prec.push_back(NP::Precedence_constraint<dtime_t>(jobs[2].get_id(), jobs[3].get_id(), Interval<dtime_t>(0, 0), NP::start_to_start));
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	prob.prec[4] = NP::Precedence_constraint<dtime_t>(jobs[2].get_id(), jobs[3].get_id(), Interval<dtime_t>(0, 1), NP::start_to_start);
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// T99 should start at time 0 and occupy the second core the whole time
// J0, J1, and J2 should start whenever J0 is released
// J3 should start last
const std::string ts19_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"      0,      0,           0,           5,       90,       90,       99,        0 \n"
"      1,      1,           0,           0,       90,       90,       99,        1 \n"
"      2,      2,           0,           0,       90,       90,       99,        2 \n"
"      3,      3,           0,           0,       99,       99,      200,        3 \n"
"     99,     99,           0,           0,        1,      200,      200,        0 \n"
;

const std::string ts19_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"       0,        0,        1,        1,           0,        0,         s \n"
"       0,        0,        2,        2,           0,        0,         s \n"
"       1,        1,        3,        3,           0,        0,         s \n"
;

TEST_CASE("[global-prec] taskset-19 check transitivity pessimism (3)") {
	auto dag_in = std::istringstream(ts19_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts19_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec, 4};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	// If the constraint from J0 to J2 gets suspension, it becomes possible that J3 starts before J2,
	// causing J2 to miss its deadline...
	prob.prec[1] = NP::Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(0, 1), NP::start_to_start);
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}

// T99 should start at time 0 and occupy the second core the whole time
// J0 should start whenever it is released
// J1, and J2 should start whenever J0 is finished
// J3 should start last
const std::string ts20_jobs =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority \n"
"      0,      0,           0,           5,        1,        1,        6,        0 \n"
"      1,      1,           0,           0,       90,       90,       99,        1 \n"
"      2,      2,           0,           0,       90,       90,       99,        2 \n"
"      3,      3,           0,           0,       99,       99,      200,        3 \n"
"     99,     99,           0,           0,        1,      200,      200,        0 \n"
;

const std::string ts20_edges =
"From TID, From JID,   To TID,   To JID,    Sus. min, Sus. max, Signal at \n"
"       0,        0,        1,        1,           0,        0,         f \n"
"       0,        0,        2,        2,           0,        0,         f \n"
"       1,        1,        3,        3,           0,        0,         s \n"
;

TEST_CASE("[global-prec] taskset-20 check transitivity pessimism (4)") {
	auto dag_in = std::istringstream(ts20_edges);
	auto prec = NP::parse_precedence_file<dtime_t>(dag_in);

	auto in = std::istringstream(ts20_jobs);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	NP::Scheduling_problem<dtime_t> prob{jobs, prec, 3};

	auto space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(space->is_schedulable());
	delete space;

	// If the constraint from J0 to J2 gets suspension, it becomes possible that J3 starts before J2,
	// causing J2 to miss its deadline...
	prob.prec[1] = NP::Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(0, 1), NP::start_to_start);
	validate_prec_cstrnts(prob.prec, prob.jobs);

	space = NP::Global::State_space<dtime_t>::explore(prob, {});
	CHECK(!space->is_schedulable());
	delete space;
}
