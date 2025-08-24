#include "doctest.h"

#include <iostream>
#include <sstream>

#include "io.hpp"
#include "exclusion.hpp"

const std::string one_excl_line = "1, 2, 3, 4"; // taskA, jobA, taskB, jobB (no delays)

const std::string excl_line_with_delay_and_type = "1, 2, 3, 4, 5, 6, s"; // start exclusion with delays

const std::string excl_file =
"Job A TID,\tJob A JID,\tJob B TID,\tJob B JID\n"
"              1,                 2,               3,             4\n"
"              5,                 6,               7,             8\n";

TEST_CASE("[parser] single exclusion constraint minimal") {
    auto in = std::istringstream(one_excl_line);
    NP::Job<dtime_t>::Job_set jobs;
    jobs.push_back(NP::Job<dtime_t>{2, {0,0}, {{1,{0,0}}}, 10, 1, 0, 1}); // job id 2 task 1
    jobs.push_back(NP::Job<dtime_t>{4, {0,0}, {{1,{0,0}}}, 10, 1, 1, 3}); // job id 4 task 3
    auto ex = NP::parse_mutex_constraint<dtime_t>(in, jobs);

    CHECK(ex.get_jobA().task == 1);
    CHECK(ex.get_jobA().job == 2);
    CHECK(ex.get_jobB().task == 3);
    CHECK(ex.get_jobB().job == 4);
    CHECK(ex.get_min_delay() == 0);
    CHECK(ex.get_max_delay() == 0);
    CHECK(ex.get_type() == NP::exec_exclusion);
}

TEST_CASE("[parser] exclusion constraint with delay and start type") {
    auto in = std::istringstream(excl_line_with_delay_and_type);
    NP::Job<dtime_t>::Job_set jobs;
    jobs.push_back(NP::Job<dtime_t>{2, {0,0}, {{1,{0,0}}}, 10, 1, 0, 1}); // job id 2 task 1
    jobs.push_back(NP::Job<dtime_t>{4, {0,0}, {{1,{0,0}}}, 10, 1, 1, 3}); // job id 4 task 3
    auto ex = NP::parse_mutex_constraint<dtime_t>(in, jobs);

    CHECK(ex.get_min_delay() == 5);
    CHECK(ex.get_max_delay() == 6);
    CHECK(ex.get_type() == NP::start_exclusion);
}

TEST_CASE("[parser] exclusion file CSV") {
    auto in = std::istringstream(excl_file);
    NP::Job<dtime_t>::Job_set jobs;
    jobs.push_back(NP::Job<dtime_t>{2, {0,0}, {{1,{0,0}}}, 10, 1, 0, 1}); // job id 2 task 1
    jobs.push_back(NP::Job<dtime_t>{4, {0,0}, {{1,{0,0}}}, 10, 1, 1, 3}); // job id 4 task 3
    jobs.push_back(NP::Job<dtime_t>{6, {0,0}, {{1,{0,0}}}, 10, 1, 2, 5}); // job id 6 task 5
    jobs.push_back(NP::Job<dtime_t>{8, {0,0}, {{1,{0,0}}}, 10, 1, 3, 7}); // job id 8 task 7
    auto exs = NP::parse_mutex_file<dtime_t>(in, jobs);

    REQUIRE(exs.size() == 2);
    CHECK(exs[0].get_jobA().task == 1);
    CHECK(exs[0].get_jobA().job == 2);
    CHECK(exs[1].get_jobA().task == 5);
    CHECK(exs[1].get_jobA().job == 6);
}

TEST_CASE("[parser] invalid exclusion delay throws") {
    // max < min should throw when validated
    auto in = std::istringstream("TIDA, JIDA, TIDB, JIDB, delaymin, delaymax\n 1, 2, 3, 4, 5, 10");
    
    // create a simple jobset to validate against; jobs must include referenced jobs
    NP::Job<dtime_t>::Job_set jobs;
    jobs.push_back(NP::Job<dtime_t>{2, {0,0}, {{1,{0,0}}}, 10, 1, 0, 1}); // job id 2 task 1
    jobs.push_back(NP::Job<dtime_t>{4, {0,0}, {{1,{0,0}}}, 10, 1, 1, 2}); // job id 4 task 2

	// should throw because jobB do not exist in jobs
    CHECK_THROWS_AS(NP::parse_mutex_file<dtime_t>(in, jobs), NP::InvalidJobReference);

	// add the missing job
    jobs.push_back(NP::Job<dtime_t>{4, {0,0}, {{1,{0,0}}}, 10, 1, 1, 3}); // job id 4 task 3
	// should not throw anymore now
    CHECK_NOTHROW(NP::parse_mutex_file<dtime_t>(in, jobs));

	// a start exclusion constraint with delaymax == 0 should throw an error
	// because it does not constrain anything
    in = std::istringstream("TIDA, JIDA, TIDB, JIDB, delaymin, delaymax, type\n 1, 2, 3, 4, 0, 0, s");
    auto exs = NP::parse_mutex_file<dtime_t>(in, jobs);
    CHECK_THROWS_AS(NP::validate_excl_cstrnts<dtime_t>(exs), NP::InvalidExclusionParameter);

	// a start exclusion constraint with delaymax > 0 should not throw an error
    in = std::istringstream("TIDA, JIDA, TIDB, JIDB, delaymin, delaymax, type\n 1, 2, 3, 4, 0, 2, s");
    exs = NP::parse_mutex_file<dtime_t>(in, jobs);
    CHECK_NOTHROW(NP::validate_excl_cstrnts<dtime_t>(exs));
}
