#include "doctest.h"

#include "conditional_dispatch_constraints.hpp"
#include "interval.hpp"
#include "time.hpp"

using namespace NP;

namespace {
using Time = dtime_t;
using JobT = Job<Time>;
using Workload = std::vector<JobT>;
using Constraints = std::vector<Precedence_constraint<Time>>;

JobT make_job(unsigned long id, JobT::Job_type type, Job_index idx) {
    return JobT{id, Interval<Time>(0, 0), Interval<Time>(1, 1), 10, 0, idx, 0, type};
}

std::set<Job_index> to_set(const std::vector<Job_index>& v) {
    return {v.begin(), v.end()};
}
} // namespace

TEST_CASE("conditional siblings are mutually incompatible") {
    Workload jobs;
    jobs.push_back(make_job(1, JobT::Job_type::C_FORK, 0));
    jobs.push_back(make_job(2, JobT::Job_type::NORMAL, 1));
    jobs.push_back(make_job(3, JobT::Job_type::NORMAL, 2));
    jobs.push_back(make_job(4, JobT::Job_type::C_JOIN, 3));

    Constraints precs;
    precs.emplace_back(JobID(1, 0), JobID(2, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(1, 0), JobID(3, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(2, 0), JobID(4, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(3, 0), JobID(4, 0), Interval<Time>(0, 0), jobs);

    Conditional_dispatch_constraints<Time> constraints(jobs, precs);

    CHECK(constraints.has_conditional_siblings(1));
    CHECK(constraints.has_conditional_siblings(2));
    CHECK_FALSE(constraints.has_conditional_siblings(0));
    CHECK_FALSE(constraints.has_conditional_siblings(3));

    auto a_incompat = to_set(constraints.get_incompatible_jobs(1));
    auto b_incompat = to_set(constraints.get_incompatible_jobs(2));

    CHECK(a_incompat.count(1) == 1);
    CHECK(a_incompat.count(2) == 1);
    CHECK(a_incompat.count(3) == 0);

    CHECK(b_incompat.count(2) == 1);
    CHECK(b_incompat.count(1) == 1);
    CHECK(b_incompat.count(3) == 0);
}

TEST_CASE("all conditional branches exclude one another") {
    Workload jobs;
    jobs.push_back(make_job(10, JobT::Job_type::C_FORK, 0));
    jobs.push_back(make_job(11, JobT::Job_type::NORMAL, 1));
    jobs.push_back(make_job(12, JobT::Job_type::NORMAL, 2));
    jobs.push_back(make_job(13, JobT::Job_type::NORMAL, 3));

    Constraints precs;
    precs.emplace_back(JobID(10, 0), JobID(11, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(10, 0), JobID(12, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(10, 0), JobID(13, 0), Interval<Time>(0, 0), jobs);

    Conditional_dispatch_constraints<Time> constraints(jobs, precs);
    CHECK(constraints.has_conditional_siblings(1));
    CHECK(constraints.has_conditional_siblings(2));
    CHECK(constraints.has_conditional_siblings(3));
    CHECK_FALSE(constraints.has_conditional_siblings(0));

    std::set<Job_index> siblings{1, 2, 3};

    auto sib1 = to_set(*constraints.get_conditional_siblings(1));
    auto sib2 = to_set(*constraints.get_conditional_siblings(2));
    auto sib3 = to_set(*constraints.get_conditional_siblings(3));
    CHECK(sib1 == siblings);
    CHECK(sib2 == siblings);
    CHECK(sib3 == siblings); 

    auto sib0 = constraints.get_conditional_siblings(0);
    CHECK(sib0 == nullptr);

    auto inc1 = to_set(constraints.get_incompatible_jobs(1));
    auto inc2 = to_set(constraints.get_incompatible_jobs(2));
    auto inc3 = to_set(constraints.get_incompatible_jobs(3));

    CHECK(inc1 == siblings);
    CHECK(inc2 == siblings);
    CHECK(inc3 == siblings);
}

TEST_CASE("complex c-dag incompatible jobs") {
    /**
     * Construct the following conditional DAG:
     *           (0)
     *            |
     *        (1,C_FORK)
     *      /     |      \
     *   (2)  (3,C_FORK)  (4)
     *    |     /    \     |
     *    |   (5)    (6)   |
     *    |    /       \   |
     *  (7,C_JOIN)   (8,C_JOIN)
     *      \           /   \
     *       \        (9)  (10)    
     *        \       /     |
     *       (11,C_JOIN)    (12)
     * 
     * Incompatible jobs:
     * - Job 0: {0}
     * - Job 1: {1}
     * - Job 2: {2,3,4,5,6,8,9,10,12}
     * - Job 3: {2,3,4}
     * - Job 4: {2,3,4,5,6,7}
     * - Job 5 : {5,6,8,9,10,12}
     * - Job 6 : {5,6,7}
     * - Job 7 : {7}
     * - Job 8 : {8}
     * - ...
     */
    Workload jobs;
    jobs.push_back(make_job(0, JobT::Job_type::NORMAL, 0));
    jobs.push_back(make_job(1, JobT::Job_type::C_FORK, 1));
    jobs.push_back(make_job(2, JobT::Job_type::NORMAL, 2));
    jobs.push_back(make_job(3, JobT::Job_type::C_FORK, 3));
    jobs.push_back(make_job(4, JobT::Job_type::NORMAL, 4));
    jobs.push_back(make_job(5, JobT::Job_type::NORMAL, 5));
    jobs.push_back(make_job(6, JobT::Job_type::NORMAL, 6));
    jobs.push_back(make_job(7, JobT::Job_type::C_JOIN, 7));
    jobs.push_back(make_job(8, JobT::Job_type::C_JOIN, 8));
    jobs.push_back(make_job(9, JobT::Job_type::NORMAL, 9));
    jobs.push_back(make_job(10, JobT::Job_type::NORMAL, 10));
    jobs.push_back(make_job(11, JobT::Job_type::C_JOIN, 11));
    jobs.push_back(make_job(12, JobT::Job_type::NORMAL, 12));


    Constraints precs;
    precs.emplace_back(JobID(0, 0), JobID(1, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(1, 0), JobID(2, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(1, 0), JobID(3, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(1, 0), JobID(4, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(3, 0), JobID(5, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(3, 0), JobID(6, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(2, 0), JobID(7, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(5, 0), JobID(7, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(6, 0), JobID(8, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(4, 0), JobID(8, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(7, 0), JobID(11, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(8, 0), JobID(9, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(8, 0), JobID(10, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(9, 0), JobID(11, 0), Interval<Time>(0, 0), jobs);
    precs.emplace_back(JobID(10, 0), JobID(12, 0), Interval<Time>(0, 0), jobs);

    Conditional_dispatch_constraints<Time> constraints(jobs, precs);
    std::set<Job_index> expected_siblings_1 = {2,3,4};
    CHECK(constraints.has_conditional_siblings(2));
    CHECK(to_set(*constraints.get_conditional_siblings(2)) == expected_siblings_1);
    CHECK(constraints.has_conditional_siblings(3));
    CHECK(to_set(*constraints.get_conditional_siblings(3)) == expected_siblings_1);
    CHECK(constraints.has_conditional_siblings(4));
    CHECK(to_set(*constraints.get_conditional_siblings(4)) == expected_siblings_1);

    std::set<Job_index> expected_siblings_2 = {5,6};
    CHECK(constraints.has_conditional_siblings(5));
    CHECK(to_set(*constraints.get_conditional_siblings(5)) == expected_siblings_2);
    CHECK(constraints.has_conditional_siblings(6));
    CHECK(to_set(*constraints.get_conditional_siblings(6)) == expected_siblings_2);

    CHECK_FALSE(constraints.has_conditional_siblings(0));
    CHECK_FALSE(constraints.has_conditional_siblings(1));
    CHECK_FALSE(constraints.has_conditional_siblings(7));
    CHECK_FALSE(constraints.has_conditional_siblings(8));
    CHECK_FALSE(constraints.has_conditional_siblings(9));
    CHECK_FALSE(constraints.has_conditional_siblings(10));
    CHECK_FALSE(constraints.has_conditional_siblings(11));
    CHECK_FALSE(constraints.has_conditional_siblings(12));

    std::vector<std::set<Job_index>> expected_incompatibles = {
        {0},
        {1},
        {2,3,4,5,6,8,9,10,12},
        {2,3,4},
        {2,3,4,5,6,7},
        {5,6,8,9,10,12},
        {5,6,7},
        {7},
        {8},
        {9},
        {10},
        {11},
        {12}
    };
    for (Job_index j = 0; j < jobs.size(); ++j) {
        auto incompat_set = to_set(constraints.get_incompatible_jobs(j));
        CHECK(incompat_set == expected_incompatibles[j]);
    }
}