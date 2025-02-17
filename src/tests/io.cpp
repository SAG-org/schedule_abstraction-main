#include "doctest.h"

#include <iostream>
#include <sstream>

#include "io.hpp"

const std::string one_line = "       920,          6,              50000.0,              50010.0,   23.227497252002234,    838.6724123730141,              60000.0,                    1";

const std::string bad_line = "       920,          6,              foo, bar";

const std::string four_lines =
"   Task ID,     Job ID,          Arrival min,          Arrival max,             Cost min,             Cost max,             Deadline,             Priority\n"
"       920,          1,                  0.0,                 10.0,   23.227497252002234,    838.6724123730141,              10000.0,                    1\n"
"       920,          2,              10000.0,              10010.0,   23.227497252002234,    838.6724123730141,              20000.0,                    1\n"
"       920,          3,              20000.0,              20010.0,   23.227497252002234,    838.6724123730141,              30000.0,                    1\n";


TEST_CASE("[dense time] job parser") {
	auto in = std::istringstream(one_line);

	NP::Job<dense_t> j = NP::parse_job<dense_t>(in,0);

	CHECK(j.get_job_id() == 6);
	CHECK(j.get_priority() == 1);
	CHECK(j.get_deadline() == 60000);
}

TEST_CASE("[dense time] job parser exception") {
	auto in = std::istringstream(bad_line);

	REQUIRE_THROWS_AS(NP::parse_job<dense_t>(in,0), std::ios_base::failure);
}

TEST_CASE("[dense time] file parser") {
	auto in = std::istringstream(four_lines);

	auto jobs = NP::parse_csv_job_file<dense_t>(in);

	CHECK(jobs.size() == 3);

	for (auto j : jobs) {
		CHECK(j.get_priority() == 1);
		CHECK(j.get_task_id() == 920);
	}

	CHECK(jobs[0].get_job_id() == 1);
	CHECK(jobs[1].get_job_id() == 2);
	CHECK(jobs[2].get_job_id() == 3);

	CHECK(jobs[0].earliest_arrival() ==     0);
	CHECK(jobs[1].earliest_arrival() == 10000);
	CHECK(jobs[2].earliest_arrival() == 20000);

	CHECK(jobs[0].get_deadline() == 10000);
	CHECK(jobs[1].get_deadline() == 20000);
	CHECK(jobs[2].get_deadline() == 30000);
}

TEST_CASE("[disc time] don't parse dense files") {
	auto in = std::istringstream(four_lines);

	REQUIRE_THROWS_AS(NP::parse_job<dtime_t>(in,0), std::ios_base::failure);
}

const std::string precedence_line = "1, 2, 3, 5";

const std::string bad_precedence_line = "1, 2, 3,";
const std::string bad_precedence_line2 = "1, 2, 3x, 5";

TEST_CASE("[parser] JobID") {
	auto in = std::istringstream(precedence_line);

	auto id = NP::parse_job_id(in);

	CHECK(id.job  == 2);
	CHECK(id.task == 1);
}

TEST_CASE("[parser] precedence constraint") {
	auto in = std::istringstream(precedence_line);

	auto c = NP::parse_precedence_constraint<dtime_t>(in);

	CHECK(c.get_fromID().job == 2);
	CHECK(c.get_fromID().task == 1);
	CHECK(c.get_toID().job  == 5);
	CHECK(c.get_toID().task == 3);
}

TEST_CASE("[parser] too-short precedence constraint") {
	auto in = std::istringstream(bad_precedence_line);

	REQUIRE_THROWS_AS(NP::parse_precedence_constraint<dtime_t>(in), std::ios_base::failure);
}

TEST_CASE("[parser] invalid precedence constraint") {
	auto in = std::istringstream(bad_precedence_line2);

	REQUIRE_THROWS_AS(NP::parse_precedence_constraint<dtime_t>(in), std::ios_base::failure);
}

const std::string precedence_file =
"Predecessor TID,	Predecessor JID,	Successor TID,	Successor JID\n"
"              1,                 1,               1,             2\n"
"              1,                 1,               2,             1\n"
"              2,                 1,               3,            13\n";

TEST_CASE("[parser] precedence file") {
	auto in = std::istringstream(precedence_file);

	auto prec = NP::parse_precedence_file<dtime_t>(in);

	REQUIRE(prec.size() == 3);
	CHECK(prec[0].get_fromID().task  == 1);
	CHECK(prec[0].get_fromID().job   == 1);
	CHECK(prec[0].get_toID().task == 1);
	CHECK(prec[0].get_toID().job  == 2);

	CHECK(prec[1].get_fromID().task  == 1);
	CHECK(prec[1].get_fromID().job   == 1);
	CHECK(prec[1].get_toID().task == 2);
	CHECK(prec[1].get_toID().job  == 1);

	CHECK(prec[2].get_fromID().task  == 2);
	CHECK(prec[2].get_fromID().job   == 1);
	CHECK(prec[2].get_toID().task == 3);
	CHECK(prec[2].get_toID().job  == 13);

	for (const auto &constraint : prec) CHECK(constraint.get_type() == NP::finish_to_start);
}

const std::string precedence_file_with_signal_at =
"Predecessor TID,	Predecessor JID,	Successor TID,	Successor JID,	Sus Min,	Sus Max,	Type\n"
"              1,                 1,               1,             2,	1,			2,			s\n"
"              1,                 1,               2,             1,	3,			4,			f\n"
"              2,                 1,               3,            13,	0,			0,			s\n";

TEST_CASE("[parser] precedence file with signal at column") {
	auto in = std::istringstream(precedence_file_with_signal_at);

	auto prec = NP::parse_precedence_file<dtime_t>(in);

	REQUIRE(prec.size() == 3);
	CHECK(prec[0].get_fromID().task  == 1);
	CHECK(prec[0].get_fromID().job   == 1);
	CHECK(prec[0].get_toID().task == 1);
	CHECK(prec[0].get_toID().job  == 2);
	CHECK(prec[0].get_minsus() == 1);
	CHECK(prec[0].get_maxsus() == 2);
	CHECK(prec[0].get_type() == NP::start_to_start);

	CHECK(prec[1].get_fromID().task  == 1);
	CHECK(prec[1].get_fromID().job   == 1);
	CHECK(prec[1].get_toID().task == 2);
	CHECK(prec[1].get_toID().job  == 1);
	CHECK(prec[1].get_minsus() == 3);
	CHECK(prec[1].get_maxsus() == 4);
	CHECK(prec[1].get_type() == NP::finish_to_start);

	CHECK(prec[2].get_fromID().task  == 2);
	CHECK(prec[2].get_fromID().job   == 1);
	CHECK(prec[2].get_toID().task == 3);
	CHECK(prec[2].get_toID().job  == 13);
	CHECK(prec[2].get_minsus() == 0);
	CHECK(prec[2].get_maxsus() == 0);
	CHECK(prec[2].get_type() == NP::start_to_start);
}

TEST_CASE("[parser] invalid precedence reference") {
	auto dag_in = std::istringstream(precedence_file);
	auto prec = NP::parse_precedence_file<dense_t>(dag_in);

	auto in = std::istringstream(four_lines);
	auto jobs = NP::parse_csv_job_file<dense_t>(in);

	REQUIRE_THROWS_AS(NP::validate_prec_cstrnts<dense_t>(prec, jobs), NP::InvalidJobReference);
}

const std::string sequential_task_prec_file =
"Predecessor TID,	Predecessor JID,	Successor TID,	Successor JID\n"
"            920,                 1,             920,             2\n"
"            920,                 2,             920,             3\n";

TEST_CASE("[parser] valid precedence reference") {
	auto dag_in = std::istringstream(sequential_task_prec_file);
	auto prec = NP::parse_precedence_file<dense_t>(dag_in);

	auto in = std::istringstream(four_lines);
	auto jobs = NP::parse_csv_job_file<dense_t>(in);

	NP::validate_prec_cstrnts<dense_t>(prec, jobs);
	// dummy check; real check is that previous line didn't throw an exception
	CHECK(true);
}
