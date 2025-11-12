#include "doctest.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <memory>

#include "global/extension/mk-firm/mk_firm.hpp"
#include "global/extension/mk-firm/mk_sp_data_extension.hpp"
#include "global/extension/mk-firm/mk_state_extension.hpp"
#include "global/extension/mk-firm/mk_extension.hpp"
#include "global/extension/mk-firm/mk_problem_extension.hpp"
#include "global/space.hpp"
#include "global/state.hpp"
#include "io.hpp"

#ifdef CONFIG_ANALYSIS_EXTENSIONS

using namespace NP;
using namespace NP::Global;
using namespace NP::Global::MK_analysis;

// ============================================================================
// Test Suite: Misses_window
// ============================================================================

TEST_CASE("[MK] Misses_window - basic initialization") {
    Misses_window<dtime_t> window(5);
    CHECK(window.get_num_potential_misses() == 0);
}

TEST_CASE("[MK] Misses_window - add hits only") {
    Misses_window<dtime_t> window(3);

    window.add_certain_hit();
    CHECK(window.get_num_certain_misses() == 0);

    window.add_certain_hit();
    CHECK(window.get_num_certain_misses() == 0);

    window.add_certain_hit();
    CHECK(window.get_num_certain_misses() == 0);

    window.add_certain_hit();
    CHECK(window.get_num_certain_misses() == 0);
}

TEST_CASE("[MK] Misses_window - add misses only") {
    Misses_window<dtime_t> window(3);
    
    window.add_certain_miss();
    CHECK(window.get_num_certain_misses() == 1);

    window.add_certain_miss();
    CHECK(window.get_num_certain_misses() == 2);

    window.add_certain_miss();
    CHECK(window.get_num_certain_misses() == 3);
}

TEST_CASE("[MK] Misses_window - sliding window behavior") {
    Misses_window<dtime_t> window(3);
    
    // Add 3 misses
    window.add_certain_miss();
    window.add_certain_miss();
    window.add_certain_miss();
    CHECK(window.get_num_certain_misses() == 3);
    
    // Add a hit - oldest miss should slide out
    window.add_certain_hit();
    CHECK(window.get_num_certain_misses() == 2);
    
    // Add another hit
    window.add_certain_hit();
    CHECK(window.get_num_certain_misses() == 1);

    // Add another hit
    window.add_certain_hit();
    CHECK(window.get_num_certain_misses() == 0);
}

TEST_CASE("[MK] Misses_window - mixed hits and misses") {
    Misses_window<dtime_t> window(4);

    window.add_certain_hit();   // [0,0,0,0]
    CHECK(window.get_num_certain_misses() == 0);

    window.add_certain_miss();  // [0,0,0,1]
    CHECK(window.get_num_certain_misses() == 1);

    window.add_certain_hit();   // [0,0,1,0]
    CHECK(window.get_num_potential_misses() == 1);

    window.add_certain_miss();  // [0,1,0,1]
    CHECK(window.get_num_potential_misses() == 2);

    window.add_certain_hit();   // [1,0,1,0]
    CHECK(window.get_num_potential_misses() == 2);

    window.add_certain_hit();   // [0,1,0,0]
    CHECK(window.get_num_potential_misses() == 1);
}

TEST_CASE("[MK] Misses_window - window size 1") {
    Misses_window<dtime_t> window(1);

    window.add_certain_miss();
    CHECK(window.get_num_certain_misses() == 1);

    window.add_certain_hit();
    CHECK(window.get_num_certain_misses() == 0);

    window.add_certain_miss();
    CHECK(window.get_num_certain_misses() == 1);
}

TEST_CASE("[MK] Misses_window - large window") {
    Misses_window<dtime_t> window(10);
    
    // Add 7 misses and 3 hits
    for (int i = 0; i < 7; i++) {
        window.add_certain_miss();
    }
    for (int i = 0; i < 3; i++) {
        window.add_certain_hit();
    }
    CHECK(window.get_num_certain_misses() == 7);

    // Slide out all misses
    for (int i = 0; i < 7; i++) {
        window.add_certain_hit();
    }
    CHECK(window.get_num_certain_misses() == 0);
}

TEST_CASE("[MK] Misses_window - maximum window size (31)") {
    Misses_window<dtime_t> window(31);
    
    // Fill entire window with misses
    for (int i = 0; i < 31; i++) {
        window.add_certain_miss();
    }
    CHECK(window.get_num_certain_misses() == 31);

    // Slide them all out
    for (int i = 0; i < 31; i++) {
        window.add_certain_hit();
        CHECK(window.get_num_certain_misses() == 30 - i);
    }
}

// ============================================================================
// Test Suite: MK_constraint parsing
// ============================================================================

TEST_CASE("[MK] parse_mk_constraints_csv - basic parsing") {
    std::string csv_content = 
        "TaskID, MaxMisses (m), WindowSize (k)\n"
        "0, 3, 10\n"
        "1, 1, 5\n";
    
    std::istringstream in(csv_content);
    auto constraints = parse_mk_constraints_csv(in);
    
    CHECK(constraints.size() == 2);
    CHECK(constraints[0].misses == 3);
    CHECK(constraints[0].window_length == 10);
    CHECK(constraints[1].misses == 1);
    CHECK(constraints[1].window_length == 5);
}

TEST_CASE("[MK] parse_mk_constraints_csv - multiple tasks") {
    std::string csv_content = 
        "TaskID, MaxMisses (m), WindowSize (k)\n"
        "0, 3, 10\n"
        "1, 1, 5\n"
        "2, 2, 7\n"
        "3, 1, 8\n";
    
    std::istringstream in(csv_content);
    auto constraints = parse_mk_constraints_csv(in);
    
    CHECK(constraints.size() == 4);
    CHECK(constraints[0].misses == 3);
    CHECK(constraints[0].window_length == 10);
    CHECK(constraints[1].misses == 1);
    CHECK(constraints[1].window_length == 5);
    CHECK(constraints[2].misses == 2);
    CHECK(constraints[2].window_length == 7);
    CHECK(constraints[3].misses == 1);
    CHECK(constraints[3].window_length == 8);
}

TEST_CASE("[MK] parse_mk_constraints_csv - edge cases") {
    SUBCASE("Single task") {
        std::string csv_content = 
            "TaskID, MaxMisses (m), WindowSize (k)\n"
            "0, 0, 5\n";
        
        std::istringstream in(csv_content);
        auto constraints = parse_mk_constraints_csv(in);
        
        CHECK(constraints.size() == 1);
        CHECK(constraints[0].misses == 0);
        CHECK(constraints[0].window_length == 5);
    }
    
    SUBCASE("Zero misses allowed") {
        std::string csv_content = 
            "TaskID, MaxMisses (m), WindowSize (k)\n"
            "0, 0, 10\n";
        
        std::istringstream in(csv_content);
        auto constraints = parse_mk_constraints_csv(in);
        
        CHECK(constraints[0].misses == 0);
    }
    
    SUBCASE("Max misses equal to window size") {
        std::string csv_content = 
            "TaskID, MaxMisses (m), WindowSize (k)\n"
            "0, 5, 5\n";
        
        std::istringstream in(csv_content);
        auto constraints = parse_mk_constraints_csv(in);
        
        CHECK(constraints[0].misses == 5);
        CHECK(constraints[0].window_length == 5);
    }
}

// ============================================================================
// Test Suite: MK_constraint validation
// ============================================================================

TEST_CASE("[MK] validate_mk_constraints - valid constraints") {
    typename Job<dtime_t>::Job_set jobs;
    jobs.push_back(Job<dtime_t>(1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 2), 10, 1, 0, 0));
    jobs.push_back(Job<dtime_t>(2, Interval<dtime_t>(0, 0), Interval<dtime_t>(2, 3), 20, 2, 1, 1));
    
    MK_constraints constraints;
    constraints[0] = MK_constraint{2, 5};
    constraints[1] = MK_constraint{1, 3};
    
    CHECK_NOTHROW(validate_mk_constraints<dtime_t>(constraints, jobs));
}

TEST_CASE("[MK] validate_mk_constraints - missing constraint for task") {
    typename Job<dtime_t>::Job_set jobs;
    jobs.push_back(Job<dtime_t>(1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 2), 10, 1, 0, 0));
    jobs.push_back(Job<dtime_t>(2, Interval<dtime_t>(0, 0), Interval<dtime_t>(2, 3), 20, 2, 1, 1));
    
    MK_constraints constraints;
    constraints[0] = MK_constraint{2, 5};
    // Missing constraint for task 1
    
    CHECK_THROWS_AS(validate_mk_constraints<dtime_t>(constraints, jobs), std::runtime_error);
}

TEST_CASE("[MK] validate_mk_constraints - zero window length") {
    typename Job<dtime_t>::Job_set jobs;
    jobs.push_back(Job<dtime_t>(1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 2), 10, 1, 0, 0));
    
    MK_constraints constraints;
    constraints[0] = MK_constraint{1, 0};  // Invalid: window length cannot be 0
    
    CHECK_THROWS_AS(validate_mk_constraints<dtime_t>(constraints, jobs), std::runtime_error);
}

TEST_CASE("[MK] validate_mk_constraints - misses exceed window length") {
    typename Job<dtime_t>::Job_set jobs;
    jobs.push_back(Job<dtime_t>(1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 2), 10, 1, 0, 0));
    
    MK_constraints constraints;
    constraints[0] = MK_constraint{6, 5};  // Invalid: misses > window_length
    
    CHECK_THROWS_AS(validate_mk_constraints<dtime_t>(constraints, jobs), std::runtime_error);
}

// ============================================================================
// Test Suite: MK_sp_data_extension
// ============================================================================

TEST_CASE("[MK] MK_sp_data_extension - initialization") {
    MK_constraints constraints;
    constraints[0] = MK_constraint{2, 5};
    constraints[1] = MK_constraint{1, 3};
    
    size_t num_jobs = 10;
    MK_sp_data_extension<dtime_t> ext(num_jobs, constraints);
    
    CHECK(ext.get_num_tasks() == 2);
    CHECK(ext.get_mk_window_length(0) == 5);
    CHECK(ext.get_mk_window_length(1) == 3);
}

TEST_CASE("[MK] MK_sp_data_extension - submit and retrieve misses") {
    MK_constraints constraints;
    constraints[0] = MK_constraint{2, 5};
    
    size_t num_jobs = 5;
    MK_sp_data_extension<dtime_t> ext(num_jobs, constraints);
    
    // Initially all jobs should not have any misses recorded
    for (Job_index i = 0; i < num_jobs; i++) {
        CHECK(ext.get_misses(i).min() == -1);
        CHECK(ext.get_misses(i).max() == -1);
    }
    
    // Submit misses for job 0
    ext.submit_misses(0, 1, 1);
    CHECK(ext.get_misses(0).min() == 1);
    CHECK(ext.get_misses(0).max() == 1);
    
    // Submit higher misses count
    ext.submit_misses(1, 2, 2);
    CHECK(ext.get_misses(1).min() == 2);
    CHECK(ext.get_misses(1).max() == 2);
}

TEST_CASE("[MK] MK_sp_data_extension - multiple submissions for same job") {
    MK_constraints constraints;
    constraints[0] = MK_constraint{3, 7};
    
    size_t num_jobs = 3;
    MK_sp_data_extension<dtime_t> ext(num_jobs, constraints);
    
    // Submit misses multiple times - should extend the interval
    ext.submit_misses(0, 1, 1);
    CHECK(ext.get_misses(0).min() == 1);
    CHECK(ext.get_misses(0).max() == 1);

    ext.submit_misses(0, 2, 2);
    CHECK(ext.get_misses(0).min() == 1);  // min should stay at 1
    CHECK(ext.get_misses(0).max() == 2);  // max should extend to 2
    
    ext.submit_misses(0, 0, 1);
    CHECK(ext.get_misses(0).min() == 0);  // min should lower to 0
    CHECK(ext.get_misses(0).max() == 2);  // max should stay at 2
}

TEST_CASE("[MK] MK_sp_data_extension - get_results") {
    MK_constraints constraints;
    constraints[0] = MK_constraint{2, 5};
    constraints[1] = MK_constraint{1, 3};
    
    size_t num_jobs = 4;
    MK_sp_data_extension<dtime_t> ext(num_jobs, constraints);
    
    ext.submit_misses(0, 1, 1);
    ext.submit_misses(1, 2, 2);
    ext.submit_misses(2, 0, 0);
    ext.submit_misses(3, 1, 1);
    
    const auto& results = ext.get_results();
    CHECK(results.size() == num_jobs);
    CHECK(results[0].min() == 1);
    CHECK(results[1].min() == 2);
    CHECK(results[2].min() == 0);
    CHECK(results[3].min() == 1);
}

// ============================================================================
// Test Suite: Integration tests with full analysis
// ============================================================================

TEST_CASE("[MK] Full MK analysis - all jobs meet deadlines") {
    // Create a simple schedulable job set (2 tasks, 3 jobs each)
    typename Job<dtime_t>::Job_set jobs;
    
    // Task 0: 3 jobs, periods = 10, execution time = 2
    jobs.push_back(Job<dtime_t>(1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 2), 10, 1, 0, 0));
    jobs.push_back(Job<dtime_t>(2, Interval<dtime_t>(10, 10), Interval<dtime_t>(1, 2), 20, 2, 1, 0));
    jobs.push_back(Job<dtime_t>(3, Interval<dtime_t>(20, 20), Interval<dtime_t>(1, 2), 30, 3, 2, 0));
    
    // Task 1: 3 jobs, periods = 10, execution time = 1
    jobs.push_back(Job<dtime_t>(4, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 1), 10, 4, 3, 1));
    jobs.push_back(Job<dtime_t>(5, Interval<dtime_t>(10, 10), Interval<dtime_t>(1, 1), 20, 5, 4, 1));
    jobs.push_back(Job<dtime_t>(6, Interval<dtime_t>(20, 20), Interval<dtime_t>(1, 1), 30, 6, 5, 1));
    
    // MK constraints: allow some misses
    MK_constraints mk_constraints;
    mk_constraints[0] = MK_constraint{3, 10};  // Task 0: up to 3 misses in 10 jobs
    mk_constraints[1] = MK_constraint{1, 5};   // Task 1: up to 1 miss in 5 jobs
    
    validate_mk_constraints<dtime_t>(mk_constraints, jobs);
    
    // Create scheduling problem
    Scheduling_problem<dtime_t> problem(jobs, 2);  // 2 processors
    
    // Register the MK problem extension
    auto mk_prob_ext_id = problem.problem_extensions.register_extension(std::make_unique<MK_problem_extension>(mk_constraints));

    // Create analysis options
    Analysis_options opts;
    opts.timeout = 0;
    opts.max_depth = 0;
    opts.early_exit = false;
    opts.be_naive = false;
    
    // Run the analysis
    auto space = State_space<dtime_t>::explore(problem, opts);
    
    // Check if schedulable
    CHECK(space->is_schedulable());
    
    // Since the system is schedulable, the extension should have recorded 0 miss counts
    auto mk_res = space->get_results<MK_sp_data_extension<dtime_t>>();
    CHECK(mk_res.size() != 0);
    for (const auto& misses : mk_res) {
        CHECK(misses.min() == 0);
        CHECK(misses.max() == 0);
    }
}

TEST_CASE("[MK] Full MK analysis - simple example with deadline misses") {
    // Create a job set where some jobs will miss deadlines
    typename Job<dtime_t>::Job_set jobs;
    
    // Task 0: tight deadlines, will cause misses
    jobs.push_back(Job<dtime_t>(1, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 4), 5, 1, 0, 0));
    jobs.push_back(Job<dtime_t>(2, Interval<dtime_t>(5, 5), Interval<dtime_t>(3, 4), 10, 2, 1, 0));
    jobs.push_back(Job<dtime_t>(3, Interval<dtime_t>(10, 10), Interval<dtime_t>(3, 4), 15, 3, 2, 0));
    jobs.push_back(Job<dtime_t>(4, Interval<dtime_t>(15, 15), Interval<dtime_t>(3, 4), 20, 4, 3, 0));
    jobs.push_back(Job<dtime_t>(5, Interval<dtime_t>(20, 20), Interval<dtime_t>(3, 4), 25, 5, 4, 0));
    jobs.push_back(Job<dtime_t>(6, Interval<dtime_t>(25, 25), Interval<dtime_t>(3, 4), 30, 6, 5, 0));
	// Task 1 will block Task 0
	jobs.push_back(Job<dtime_t>(1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 3), 30, 6, 6, 1)); // blocks job 2 of task 0
	jobs.push_back(Job<dtime_t>(2, Interval<dtime_t>(10, 10), Interval<dtime_t>(1, 3), 60, 6, 7, 1)); // blocks job 4 of task 0
    
    // MK constraints
    MK_constraints mk_constraints;
    mk_constraints[0] = MK_constraint{2, 4};  // Allow 2 misses in 5 jobs
	mk_constraints[1] = MK_constraint{ 0, 1 };  // Allow 0 misses
    
    validate_mk_constraints<dtime_t>(mk_constraints, jobs);
    
    // Create scheduling problem with 1 processor
    Scheduling_problem<dtime_t> problem(jobs, 1);
    // Register the MK problem extension
    auto mk_prob_ext_id = problem.problem_extensions.register_extension(std::make_unique<MK_problem_extension>(mk_constraints));
    
    // Create analysis options
    Analysis_options opts;
    opts.timeout = 0;
    opts.max_depth = 0;
    opts.early_exit = false;
    opts.be_naive = false;
    
    // Run the analysis
    auto space = State_space<dtime_t>::explore(problem, opts);
    CHECK(!space->is_schedulable());  // Should be unschedulable due to tight deadlines and blocking
    
    // Retrieve MK results
    auto mk_res = space->get_results<MK_sp_data_extension<dtime_t>>();
    CHECK(mk_res.size() == jobs.size());
    
    // jobs of task 1
    CHECK(mk_res[0].min() == 0); // T1J1
	CHECK(mk_res[0].max() == 0);
	CHECK(mk_res[1].min() == 0); // T1J2
    CHECK(mk_res[1].max() == 1);
	CHECK(mk_res[2].min() == 0); // T1J3
    CHECK(mk_res[2].max() == 1);
	CHECK(mk_res[3].min() == 0); // T1J4
    CHECK(mk_res[3].max() == 2);
	CHECK(mk_res[4].min() == 0); // T1J5
    // NOTE: in discrete time the max number of misses should be 2, but the merge operation can lead to an over-approximation.
    // For dense time, the max number of misses is indeed 3.
	CHECK(mk_res[4].max() == 3);
    CHECK(mk_res[5].min() == 0); // T1J6
    // NOTE: in discrete time the max number of misses should be 1, but the merge operation can lead to an over-approximation.
    // For dense time, the max number of misses is indeed 2.
    CHECK(mk_res[5].max() == 2);
	// jobs of task 2
    CHECK(mk_res[6].min() == 0); // T2J1
    CHECK(mk_res[6].max() == 0);
    CHECK(mk_res[7].min() == 0); // T2J2
    CHECK(mk_res[7].max() == 0);
}

TEST_CASE("[MK] MK_constraint - struct basic functionality") {
    MK_constraint c1{3, 10};
    CHECK(c1.misses == 3);
    CHECK(c1.window_length == 10);
    
    MK_constraint c2{0, 5};
    CHECK(c2.misses == 0);
    CHECK(c2.window_length == 5);
}

TEST_CASE("[MK] MK_constraints - unordered_map operations") {
    MK_constraints constraints;
    constraints[0] = MK_constraint{2, 5};
    constraints[1] = MK_constraint{1, 3};
    constraints[2] = MK_constraint{3, 8};
    
    CHECK(constraints.size() == 3);
    CHECK(constraints.count(0) == 1);
    CHECK(constraints.count(1) == 1);
    CHECK(constraints.count(2) == 1);
    CHECK(constraints.count(3) == 0);
    
    CHECK(constraints[0].misses == 2);
    CHECK(constraints[1].window_length == 3);
}

// ============================================================================
// Test Suite: Edge cases and boundary conditions
// ============================================================================

TEST_CASE("[MK] Misses_window - alternating pattern") {
    Misses_window<dtime_t> window(6);
    
    // Alternating misses and hits: M H M H M H
    window.add_potential_miss();
    window.add_potential_hit();
    window.add_potential_miss();
    window.add_potential_hit();
    window.add_potential_miss();
    window.add_potential_hit();
    
    CHECK(window.get_num_potential_misses() == 3);
    
    // Continue alternating
    window.add_potential_miss();  // Slides out oldest miss
    CHECK(window.get_num_potential_misses() == 3);
    
    window.add_potential_hit();   // Slides out oldest hit
    CHECK(window.get_num_potential_misses() == 3);
}

TEST_CASE("[MK] parse_mk_constraints_csv - non-sequential task IDs") {
    std::string csv_content = 
        "TaskID, MaxMisses (m), WindowSize (k)\n"
        "5, 2, 10\n"
        "3, 1, 5\n"
        "7, 3, 8\n";
    
    std::istringstream in(csv_content);
    auto constraints = parse_mk_constraints_csv(in);
    
    CHECK(constraints.size() == 3);
    CHECK(constraints[5].misses == 2);
    CHECK(constraints[5].window_length == 10);
    CHECK(constraints[3].misses == 1);
    CHECK(constraints[3].window_length == 5);
    CHECK(constraints[7].misses == 3);
    CHECK(constraints[7].window_length == 8);
}

TEST_CASE("[MK] MK_sp_data_extension - boundary values") {
    SUBCASE("Single job") {
        MK_constraints constraints;
        constraints[0] = MK_constraint{1, 2};
        
        MK_sp_data_extension<dtime_t> ext(1, constraints);
        CHECK(ext.get_num_tasks() == 1);
        
        ext.submit_misses(0, 1, 1);
        CHECK(ext.get_misses(0).min() == 1);
    }
    
    SUBCASE("Many jobs") {
        MK_constraints constraints;
        constraints[0] = MK_constraint{10, 50};
        
        size_t num_jobs = 100;
        MK_sp_data_extension<dtime_t> ext(num_jobs, constraints);
        
        for (Job_index i = 0; i < num_jobs; i++) {
            ext.submit_misses(i, i % 11, i % 11);  // 0-10 misses
        }
        
        for (Job_index i = 0; i < num_jobs; i++) {
            CHECK(ext.get_misses(i).min() == i % 11);
            CHECK(ext.get_misses(i).max() == i % 11);
        }
    }
}

TEST_CASE("[MK] Misses_window - stress test with long sequence") {
    Misses_window<dtime_t> window(8);
    
    // Pattern: 5 hits, then 3 misses, repeated
    int expected_misses = 0;
    for (int cycle = 0; cycle < 10; cycle++) {
        // Add 5 hits
        for (int i = 0; i < 5; i++) {
            window.add_certain_hit();
            // After each addition, count should decrease if a miss slides out
            if (cycle > 0) {
                // Window is full of pattern, should stabilize
                CHECK(window.get_num_certain_misses() <= 3);
            }
        }
        
        // Add 3 misses
        for (int i = 0; i < 3; i++) {
            window.add_certain_miss();
            // Count should increase, but respect sliding window
            CHECK(window.get_num_certain_misses() <= 8);
        }
    }
}

#endif // CONFIG_ANALYSIS_EXTENSIONS