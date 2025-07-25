#include "doctest.h"

#include <iostream>
#include <sstream>

#include "io.hpp"
#include "global/state.hpp"
#include "global/space.hpp"

const std::string fig1a_jobs_file =
"   Task ID,     Job ID,          Arrival min,          Arrival max,             Cost min,             Cost max,             Deadline,             Priority\n"
"1, 1,  0,  0, 1,  2, 10, 10\n"
"1, 2, 10, 10, 1,  2, 20, 20\n"
"1, 3, 20, 20, 1,  2, 30, 30\n"
"1, 4, 30, 30, 1,  2, 40, 40\n"
"1, 5, 40, 40, 1,  2, 50, 50\n"
"1, 6, 50, 50, 1,  2, 60, 60\n"
"2, 7,  0,  0, 7,  8, 30, 30\n"
"2, 8, 30, 30, 7,  7, 60, 60\n"
"3, 9,  0,  0, 3, 13, 60, 60\n";


/*TEST_CASE("[global-prec] basic state evolution (RTSS18-Fig-3)") {
	NP::Global::Schedule_state<dtime_t> init(2);

	CHECK(init.core_availability().min() == 0);
	CHECK(init.core_availability().max() == 0);

	NP::Global::Schedule_state<dtime_t> v1{ init, 1, {}, {0, 0}, {5, 15}, {}, {}, {} };

	CHECK(v1.core_availability().min() == 0);
	CHECK(v1.core_availability().max() == 0);

	NP::Global::Schedule_state<dtime_t> vp{v1, 2, {}, {0, 0}, {12, 30}, {}, {}, {} };

	CHECK(vp.core_availability().min() ==  5);
	CHECK(vp.core_availability().max() == 15);

	CHECK(!vp.can_merge_with(init));
	CHECK(!vp.can_merge_with(v1));

	NP::Global::Schedule_state<dtime_t> v2{init, 2, {}, {0, 0}, {10, 25}, {}, {}, {} };

	CHECK(v1.core_availability().min() == 0);
	CHECK(v1.core_availability().max() == 0);

	// With nodes, scheduled_jobs is part of a node, such that state doesn't have a key.
	// As a result, the merge will work.
	CHECK(!v2.can_merge_with(v1)); // ISSUE
	CHECK(!v2.try_to_merge(v1));   // ISSUE

	NP::Global::Schedule_state<dtime_t> vq{v2, 1, {}, {0, 0}, {8, 20}, { }, {}, {} };

	CHECK(vq.core_availability().min() ==  8);  // ISSUE: 5
	CHECK(vq.core_availability().max() == 20);

	CHECK(vq.can_merge_with(vp));
	CHECK(vp.can_merge_with(vq));

	CHECK(vp.try_to_merge(vq));

	CHECK(vq.core_availability().min() ==  8);  // ISSUE: 5
	CHECK(vq.core_availability().max() == 20);

	CHECK(vp.core_availability().min() ==  5);
	CHECK(vp.core_availability().max() == 20);
}*/

TEST_CASE("[global] RTSS17-Fig-1a") {
	auto in = std::istringstream(fig1a_jobs_file);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	auto num_cpus = 2;

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs,
		num_cpus);

	CHECK(nspace->is_schedulable());

	auto space = NP::Global::State_space<dtime_t>::explore(jobs, num_cpus);

	CHECK(space->is_schedulable());


	num_cpus = 1;
	auto nspace2 = NP::Global::State_space<dtime_t>::explore_naively(jobs,
		num_cpus);

	CHECK_FALSE(nspace2->is_schedulable());

	auto space2 = NP::Global::State_space<dtime_t>::explore(jobs, num_cpus);

	CHECK_FALSE(space2->is_schedulable());
	delete space;
	delete nspace;
	delete space2;
	delete nspace2;
}

const std::string global_fig1_file =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"1, 1, 0, 0, 2, 4, 7, 1\n"
"2, 1, 0, 0, 10, 15, 20, 2\n"
"3, 1, 5, 5, 1, 7, 15, 3\n"
"4, 1, 8, 8, 2, 3, 20, 4\n"
"5, 1, 8, 8, 1, 1, 14, 5\n";


TEST_CASE("[global] ECRTS18-Fig-1") {
	auto in = std::istringstream(global_fig1_file);
	auto jobs = NP::parse_csv_job_file<dtime_t>(in);

	auto num_cpus = 2;

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs,
		num_cpus);

	CHECK_FALSE(nspace->is_schedulable());  

	auto space = NP::Global::State_space<dtime_t>::explore(jobs,
		num_cpus);

	CHECK_FALSE(space->is_schedulable()); 
	delete space;
	delete nspace;
}

static const auto inf = Time_model::constants<dtime_t>::infinity();

TEST_CASE("[global] Find all next jobs") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>( 0,  0), Interval<dtime_t>(3, 8), 100, 1, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>( 7,  7), Interval<dtime_t>(5, 5),  100, 2, 1, 1},
		NP::Job<dtime_t>{3, Interval<dtime_t>(10, 10), Interval<dtime_t>(1, 11),  100, 3, 2, 2},
	};

	NP::Scheduling_problem<dtime_t> prob{jobs};
	NP::Analysis_options opts;

	//prob.num_processors = 1;

	SUBCASE("Naive exploration") {
		auto space = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
		CHECK(space->is_schedulable());

		CHECK(space->get_finish_times(jobs[0]).from()  == 3);
		CHECK(space->get_finish_times(jobs[0]).until() == 8);

		CHECK(space->get_finish_times(jobs[1]).from()  == 12);
		CHECK(space->get_finish_times(jobs[1]).until() == 13); 

		CHECK(space->get_finish_times(jobs[2]).from()  == 13);
		CHECK(space->get_finish_times(jobs[2]).until() == 24); 
		delete space;
	}

	SUBCASE("Exploration with merging") {
		auto space = NP::Global::State_space<dtime_t>::explore(jobs, 1);
		CHECK(space->is_schedulable());

		CHECK(space->get_finish_times(jobs[0]).from()  == 3);
		CHECK(space->get_finish_times(jobs[0]).until() == 8);

		CHECK(space->get_finish_times(jobs[1]).from()  == 12);
		CHECK(space->get_finish_times(jobs[1]).until() == 13);

		CHECK(space->get_finish_times(jobs[2]).from()  == 13);
		CHECK(space->get_finish_times(jobs[2]).until() == 24); 
		delete space;
	}

}

TEST_CASE("[global] Consider large enough interval") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>( 0,  0), Interval<dtime_t>(3, 10),  100, 3, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>( 7,  7),  Interval<dtime_t>(5, 5),  100, 2, 1, 1},
		NP::Job<dtime_t>{3, Interval<dtime_t>(10, 10),  Interval<dtime_t>(5, 5),  100, 1, 2, 2},
	};

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
	CHECK(nspace->is_schedulable());

	CHECK(nspace->get_finish_times(jobs[0]).from()  ==  3);
	CHECK(nspace->get_finish_times(jobs[0]).until() == 10);

	CHECK(nspace->get_finish_times(jobs[1]).from()  == 12);
	CHECK(nspace->get_finish_times(jobs[1]).until() == 20);  

	CHECK(nspace->get_finish_times(jobs[2]).from()  == 15);  
	CHECK(nspace->get_finish_times(jobs[2]).until() == 19);  

	auto space = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
	CHECK(space->is_schedulable());

	CHECK(space->get_finish_times(jobs[0]).from()  ==  3);
	CHECK(space->get_finish_times(jobs[0]).until() == 10);

	CHECK(space->get_finish_times(jobs[1]).from()  == 12);
	CHECK(space->get_finish_times(jobs[1]).until() == 20); 

	CHECK(space->get_finish_times(jobs[2]).from()  == 15); 
	CHECK(space->get_finish_times(jobs[2]).until() == 19); 
	delete space;
	delete nspace;
}

TEST_CASE("[global] Respect priorities") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>( 0,  0), Interval<dtime_t>(3, 10),  100, 2, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>( 0,  0),  Interval<dtime_t>(5, 5),  100, 1, 1, 1},
	};

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
	CHECK(nspace->is_schedulable());

	CHECK(nspace->get_finish_times(jobs[0]).from()  ==  8);
	CHECK(nspace->get_finish_times(jobs[0]).until() == 15);

	CHECK(nspace->get_finish_times(jobs[1]).from()  ==  5);
	CHECK(nspace->get_finish_times(jobs[1]).until() ==  5);

	auto space = NP::Global::State_space<dtime_t>::explore(jobs, 1);
	CHECK(space->is_schedulable());

	CHECK(space->get_finish_times(jobs[0]).from()  ==  8);
	CHECK(space->get_finish_times(jobs[0]).until() == 15);

	CHECK(space->get_finish_times(jobs[1]).from()  ==  5);
	CHECK(space->get_finish_times(jobs[1]).until() ==  5);
	delete space;
	delete nspace;
}

TEST_CASE("[global] Respect jitter") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>( 0,  1), Interval<dtime_t>(3, 10),  100, 2, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>( 0,  1),  Interval<dtime_t>(5, 5),  100, 1, 1, 1},
	};

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
	CHECK(nspace->is_schedulable());

	CHECK(nspace->get_finish_times(jobs[0]).from()  ==  3);
	CHECK(nspace->get_finish_times(jobs[0]).until() == 16); 

	CHECK(nspace->get_finish_times(jobs[1]).from()  ==  5);
	CHECK(nspace->get_finish_times(jobs[1]).until() == 15); 

	auto space = NP::Global::State_space<dtime_t>::explore(jobs, 1);
	CHECK(space->is_schedulable());

	CHECK(space->get_finish_times(jobs[0]).from()  ==  3);
	CHECK(space->get_finish_times(jobs[0]).until() == 16); 

	CHECK(space->get_finish_times(jobs[1]).from()  ==  5);
	CHECK(space->get_finish_times(jobs[1]).until() == 15);
	delete space;
	delete nspace;
}

TEST_CASE("[global] Be eager") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>( 0,  0),  Interval<dtime_t>(1,  5),  100, 2, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>( 0,  0),  Interval<dtime_t>(1, 20),  100, 3, 1, 1},
		NP::Job<dtime_t>{3, Interval<dtime_t>(10, 10),  Interval<dtime_t>(5,  5),  100, 1, 2, 2},
	};

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
	CHECK(nspace->is_schedulable());

	CHECK(nspace->get_finish_times(jobs[0]).from()  ==  1);
	CHECK(nspace->get_finish_times(jobs[0]).until() ==  5);

	CHECK(nspace->get_finish_times(jobs[1]).from()  ==  2);
	CHECK(nspace->get_finish_times(jobs[1]).until() ==  25); 

	CHECK(nspace->get_finish_times(jobs[2]).from()  ==  15);
	CHECK(nspace->get_finish_times(jobs[2]).until() ==  30); 

	auto space = NP::Global::State_space<dtime_t>::explore(jobs, 1);
	CHECK(space->is_schedulable());

	CHECK(space->get_finish_times(jobs[0]).from()  ==  1);
	CHECK(space->get_finish_times(jobs[0]).until() ==  5);

	CHECK(space->get_finish_times(jobs[1]).from()  ==  2);
	CHECK(space->get_finish_times(jobs[1]).until() ==  25); 

	CHECK(space->get_finish_times(jobs[2]).from()  ==  15);
	CHECK(space->get_finish_times(jobs[2]).until() ==  30); 
	delete space;
	delete nspace;
}


TEST_CASE("[global] Be eager, with short deadline") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>( 0,  0),  Interval<dtime_t>(1,  5),  100, 2, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>( 9,  9),  Interval<dtime_t>(1, 15),   25, 3, 1, 1},
		NP::Job<dtime_t>{3, Interval<dtime_t>(30, 30),  Interval<dtime_t>(5,  5),  100, 1, 2, 2},
	};

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
	CHECK(nspace->is_schedulable());

	CHECK(nspace->get_finish_times(jobs[0]).from()  ==  1);
	CHECK(nspace->get_finish_times(jobs[0]).until() ==  5);

	CHECK(nspace->get_finish_times(jobs[1]).from()  ==  10);
	CHECK(nspace->get_finish_times(jobs[1]).until() ==  24);

	CHECK(nspace->get_finish_times(jobs[2]).from()  ==  35);
	CHECK(nspace->get_finish_times(jobs[2]).until() ==  35);

	auto space = NP::Global::State_space<dtime_t>::explore(jobs, 1);
	CHECK(space->is_schedulable());

	CHECK(space->get_finish_times(jobs[0]).from()  ==  1);
	CHECK(space->get_finish_times(jobs[0]).until() ==  5);

	CHECK(space->get_finish_times(jobs[1]).from()  ==  10);
	CHECK(space->get_finish_times(jobs[1]).until() ==  24);

	CHECK(space->get_finish_times(jobs[2]).from()  ==  35);
	CHECK(space->get_finish_times(jobs[2]).until() ==  35);
	delete space;
	delete nspace;
}


TEST_CASE("[global] Treat equal-priority jobs correctly") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>(    0,    10),  Interval<dtime_t>( 2,    50),  2000, 1, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>(    0,    10),  Interval<dtime_t>(50,  1200),  5000, 2, 1, 1},
		NP::Job<dtime_t>{3, Interval<dtime_t>( 1000,  1010),  Interval<dtime_t>( 2,    50),  3000, 1, 2, 2},
	};

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
	CHECK(nspace->is_schedulable());

	CHECK(nspace->get_finish_times(jobs[0]).from()  ==  2);
	CHECK(nspace->get_finish_times(jobs[0]).until() ==  1259);  

	CHECK(nspace->get_finish_times(jobs[1]).from()  ==  50);
	CHECK(nspace->get_finish_times(jobs[1]).until() ==  1260); 

	CHECK(nspace->get_finish_times(jobs[2]).from()  ==  1002);
	CHECK(nspace->get_finish_times(jobs[2]).until() ==  1310); 

	auto space = NP::Global::State_space<dtime_t>::explore(jobs, 1);
	CHECK(space->is_schedulable());

	CHECK(nspace->get_finish_times(jobs[0]).from()  ==  2);
	CHECK(nspace->get_finish_times(jobs[0]).until() ==  1259); 

	CHECK(nspace->get_finish_times(jobs[1]).from()  ==  50);
	CHECK(nspace->get_finish_times(jobs[1]).until() ==  1260); 

	CHECK(nspace->get_finish_times(jobs[2]).from()  ==  1002);
	CHECK(nspace->get_finish_times(jobs[2]).until() ==  1310); 
	delete space;
	delete nspace;
}

TEST_CASE("[global] Equal-priority simultaneous arrivals") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>(    0,    10),  Interval<dtime_t>(  2,    50),  2000, 2000, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>(    0,    10),  Interval<dtime_t>(100,   150),  2000, 2000, 1, 1},
	};

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
	CHECK(nspace->is_schedulable());

	CHECK(nspace->get_finish_times(jobs[0]).from()  ==  2);
	CHECK(nspace->get_finish_times(jobs[0]).until() ==   9 + 150 + 50); 

	CHECK(nspace->get_finish_times(jobs[1]).from()  ==  100);
	CHECK(nspace->get_finish_times(jobs[1]).until() ==  10 + 50 + 150); 

	auto space = NP::Global::State_space<dtime_t>::explore(jobs, 1);
	CHECK(space->is_schedulable());

	CHECK(nspace->get_finish_times(jobs[0]).from()  ==  2);
	CHECK(nspace->get_finish_times(jobs[0]).until() ==   9 + 150 + 50);  

	CHECK(nspace->get_finish_times(jobs[1]).from()  ==  100);
	CHECK(nspace->get_finish_times(jobs[1]).until() ==  10 + 50 + 150);  
	delete space;
	delete nspace;
}

TEST_CASE("[global] don't skip over deadline-missing jobs") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>(  100,   100),  Interval<dtime_t>(   2,    50),   200, 1, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>(    0,     0),  Interval<dtime_t>(1200,  1200),  5000, 2, 1, 1},
		NP::Job<dtime_t>{3, Interval<dtime_t>(  200,   250),  Interval<dtime_t>( 2,    50),    6000, 3, 2, 2},
		NP::Job<dtime_t>{4, Interval<dtime_t>(  200,   250),  Interval<dtime_t>( 2,    50),    6000, 4, 3, 3},
		NP::Job<dtime_t>{5, Interval<dtime_t>(  200,   250),  Interval<dtime_t>( 2,    50),    6000, 5, 4, 4},
	};

	auto nspace = NP::Global::State_space<dtime_t>::explore_naively(jobs, 1);
	CHECK(!nspace->is_schedulable());

	CHECK(nspace->number_of_edges() == 2);  
	CHECK(nspace->number_of_states() == 3); 

	auto space = NP::Global::State_space<dtime_t>::explore(jobs, 1);
	CHECK(!space->is_schedulable());

	CHECK(space->number_of_edges() == 2); 
	CHECK(space->number_of_states() == 3); 
	delete space;
	delete nspace;
}

TEST_CASE("[global] explore across bucket boundaries") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>(  100,   100),  Interval<dtime_t>(  50,   50),  10000, 1, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>( 3000,  3000),  Interval<dtime_t>(4000, 4000),  10000, 2, 1, 1},
		NP::Job<dtime_t>{3, Interval<dtime_t>( 6000,  6000),  Interval<dtime_t>(   2,    2),  10000, 3, 2, 2},
	};

	NP::Scheduling_problem<dtime_t> prob{jobs};
	NP::Analysis_options opts;

	opts.be_naive = true;
	auto nspace = NP::Global::State_space<dtime_t>::explore(prob, opts);
	CHECK(nspace->is_schedulable());

	CHECK(nspace->number_of_edges() == 3);

	opts.be_naive = false;
	auto space = NP::Global::State_space<dtime_t>::explore(prob, opts);
	CHECK(space->is_schedulable());

	CHECK(space->number_of_edges() == 3);
	delete space;
	delete nspace;
}

