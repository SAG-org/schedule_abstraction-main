#include "doctest.h"

#include <iostream>
#include <sstream>

#include "io.hpp"
#include "global/state.hpp"
#include "global/space.hpp"
#include "problem.hpp"

const std::string platform_spec_csv_partial =
"n_cores, EFT1, LFT1, EFT2, LFT2\n"
"3      , 0   , 4   , 1   , 3";

const std::string platform_spec_csv_complete =
"n_cores, EFT1, LFT1, EFT2, LFT2, EFT3, LFT3\n"
"3      , 0   , 4   , 1   , 3   , 0   , 0";

const std::string platform_spec_yaml_partial =
"platform:\n"
"  cores: 3\n"
"  core_availabilities:\n"
"    - [0, 4]\n"
"    - {Amin: 1, Amax: 3}\n";

const std::string platform_spec_yaml_complete =
"platform:\n"
"  cores: 3\n"
"  core_availabilities:\n"
"    - [0, 4]\n"
"    - {Amin: 1, Amax: 3}\n"
"    - Amin: 0\n"
"      Amax: 0";

TEST_CASE("[cores init] read csv and yml") {
	NP::Job<dtime_t>::Job_set jobs{
		NP::Job<dtime_t>{1, Interval<dtime_t>(0,  0),  Interval<dtime_t>(1,  5),  100, 100, 0, 0},
		NP::Job<dtime_t>{2, Interval<dtime_t>(9,  9),  Interval<dtime_t>(1, 15),   25, 25, 1, 1},
		NP::Job<dtime_t>{3, Interval<dtime_t>(30, 30),  Interval<dtime_t>(5,  5),  100, 100, 2, 2},
	};

	auto plat_csv1_in = std::istringstream(platform_spec_csv_partial);
	auto plat_csv1 = NP::parse_platform_spec_csv<dtime_t>(plat_csv1_in);

	NP::Scheduling_problem<dtime_t> prob_csv1{ jobs, plat_csv1 };
	NP::Analysis_options opts;
	auto space_csv1 = NP::Global::State_space<dtime_t>::explore(prob_csv1, opts);

	auto plat_csv2_in = std::istringstream(platform_spec_csv_complete);
	auto plat_csv2 = NP::parse_platform_spec_csv<dtime_t>(plat_csv2_in);

	NP::Scheduling_problem<dtime_t> prob_csv2{ jobs, plat_csv2 };
	auto space_csv2 = NP::Global::State_space<dtime_t>::explore(prob_csv2, opts);

	for(const auto& job : jobs) {
		CHECK(space_csv1->get_finish_times(job).from() == space_csv2->get_finish_times(job).from());
		CHECK(space_csv1->get_finish_times(job).until() == space_csv2->get_finish_times(job).until());
	}

	auto plat_yml1_in = std::istringstream(platform_spec_yaml_partial);
	auto plat_yml1 = NP::parse_platform_spec_yaml<dtime_t>(plat_yml1_in);

	NP::Scheduling_problem<dtime_t> prob_yml1{ jobs, plat_yml1 };
	auto space_yml1 = NP::Global::State_space<dtime_t>::explore(prob_yml1, opts);

	for (const auto& job : jobs) {
		CHECK(space_yml1->get_finish_times(job).from() == space_csv2->get_finish_times(job).from());
		CHECK(space_yml1->get_finish_times(job).until() == space_csv2->get_finish_times(job).until());
	}

	auto plat_yml2_in = std::istringstream(platform_spec_yaml_complete);
	auto plat_yml2 = NP::parse_platform_spec_yaml<dtime_t>(plat_yml2_in);

	NP::Scheduling_problem<dtime_t> prob_yml2{ jobs, plat_yml2 };
	auto space_yml2 = NP::Global::State_space<dtime_t>::explore(prob_yml2, opts);

	for (const auto& job : jobs) {
		CHECK(space_yml2->get_finish_times(job).from() == space_csv2->get_finish_times(job).from());
		CHECK(space_yml2->get_finish_times(job).until() == space_csv2->get_finish_times(job).until());
	}
}

const std::string uniproc_sn_susp_jobs_file =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"1      , 1      , 0          , 0          , 1        , 2        , 10      , 1      \n"
"1      , 2      , 10         , 10         , 1        , 2        , 20      , 2      \n"
"1      , 3      , 20         , 20         , 1        , 2        , 50      , 3      \n"
"1      , 4      , 30         , 30         , 1        , 2        , 60      , 4      \n"
"1      , 5      , 40         , 40         , 1        , 2        , 70      , 5      \n"
"1      , 6      , 50         , 50         , 1        , 2        , 80      , 6      \n"
"2      , 7      , 0          , 0          , 7        , 7        , 30      , 8      \n"
"2      , 8      , 30         , 30         , 7        , 7        , 100     , 9      \n"
"3      , 9      , 0          , 0          , 3        , 13       , 60      , 7      ";

const std::string uniproc_sn_susp_dag_file =
"Predecessor TID, Predecessor JID, Successor TID, Successor JID, Min_Sus, Max_Sus\n"
"2               , 7              , 1            , 3            , 10     , 12     ";

const std::string uniproc_sn_susp_jobs_file_minus_1 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"1      , 2      , 10         , 10         , 1        , 2        , 20      , 2      \n"
"1      , 3      , 20         , 20         , 1        , 2        , 50      , 3      \n"
"1      , 4      , 30         , 30         , 1        , 2        , 60      , 4      \n"
"1      , 5      , 40         , 40         , 1        , 2        , 70      , 5      \n"
"1      , 6      , 50         , 50         , 1        , 2        , 80      , 6      \n"
"2      , 7      , 0          , 0          , 7        , 7        , 30      , 8      \n"
"2      , 8      , 30         , 30         , 7        , 7        , 100     , 9      \n"
"3      , 9      , 0          , 0          , 3        , 13       , 60      , 7      ";

const std::string platform_spec_one_core =
"n_cores, EFT1, LFT1\n"
"1      , 1   , 2";

const std::string platform_spec_two_cores_1 =
"n_cores, EFT1, LFT1\n"
"2      , 1   , 2";

const std::string sn_susp_jobs_file_minus_2 =
"Task ID, Job ID, Arrival min, Arrival max, Cost min, Cost max, Deadline, Priority\n"
"1      , 2      , 10         , 10         , 1        , 2        , 20      , 2      \n"
"1      , 3      , 20         , 20         , 1        , 2        , 50      , 3      \n"
"1      , 4      , 30         , 30         , 1        , 2        , 60      , 4      \n"
"1      , 5      , 40         , 40         , 1        , 2        , 70      , 5      \n"
"1      , 6      , 50         , 50         , 1        , 2        , 80      , 6      \n"
"2      , 7      , 0          , 0          , 7        , 7        , 30      , 8      \n"
"2      , 8      , 30         , 30         , 7        , 7        , 100     , 9      ";

const std::string platform_spec_two_cores_2 =
"n_cores, EFT1, LFT1, EFT2, LFT2\n"
"2      , 1   , 2   , 3   , 13";


TEST_CASE("[cores init] Uniproc equivalence between analyses") {
	auto susp_dag_in = std::istringstream(uniproc_sn_susp_dag_file);
	auto in = std::istringstream(uniproc_sn_susp_jobs_file);
	auto dag_in = std::istringstream("\n");
	auto aborts_in = std::istringstream("\n");

	NP::Scheduling_problem<dtime_t> prob0{
		NP::parse_csv_job_file<dtime_t>(in),
		NP::parse_precedence_file<dtime_t>(susp_dag_in) };

	NP::Analysis_options opts;

	opts.be_naive = false;
	auto space0 = NP::Global::State_space<dtime_t>::explore(prob0, opts);
	CHECK(space0->is_schedulable());

	auto in_minus1 = std::istringstream(uniproc_sn_susp_jobs_file_minus_1);
	auto susp_dag_in_1 = std::istringstream(uniproc_sn_susp_dag_file);
	auto pltform_spec_one_core_in = std::istringstream(platform_spec_one_core);

	NP::Scheduling_problem<dtime_t> prob1{
		NP::parse_csv_job_file<dtime_t>(in_minus1),
		NP::parse_precedence_file<dtime_t>(susp_dag_in_1),
		NP::parse_platform_spec_csv<dtime_t>(pltform_spec_one_core_in)};

	opts.be_naive = false;
	auto space1 = NP::Global::State_space<dtime_t>::explore(prob1, opts);
	CHECK(space1->is_schedulable());

	for (const NP::Job<dtime_t>& j1 : prob1.jobs) {
		for (const NP::Job<dtime_t>& j0 : prob0.jobs) {
			if (j0.get_id() == j1.get_id()) {
				CHECK(space0->get_finish_times(j0).from() == space1->get_finish_times(j1).from());
				CHECK(space0->get_finish_times(j0).until() == space1->get_finish_times(j1).until());
				break;
			}
		}
	}
}

TEST_CASE("[cores init] Multiproc equivalence between analyses") {
	auto susp_dag_in = std::istringstream(uniproc_sn_susp_dag_file);
	auto in = std::istringstream(uniproc_sn_susp_jobs_file);
	auto dag_in = std::istringstream("\n");
	auto aborts_in = std::istringstream("\n");

	NP::Scheduling_problem<dtime_t> prob0{
		NP::parse_csv_job_file<dtime_t>(in),
		NP::parse_precedence_file<dtime_t>(susp_dag_in),
		2 };

	NP::Analysis_options opts;

	opts.be_naive = false;
	auto space0 = NP::Global::State_space<dtime_t>::explore(prob0, opts);
	CHECK(space0->is_schedulable());

	auto in_minus1 = std::istringstream(uniproc_sn_susp_jobs_file_minus_1);
	auto susp_dag_in_1 = std::istringstream(uniproc_sn_susp_dag_file);
	auto platform_spec_two_cores_in_1 = std::istringstream(platform_spec_two_cores_1);

	NP::Scheduling_problem<dtime_t> prob1{
		NP::parse_csv_job_file<dtime_t>(in_minus1),
		NP::parse_precedence_file<dtime_t>(susp_dag_in_1),
		NP::parse_platform_spec_csv<dtime_t>(platform_spec_two_cores_in_1) };

	opts.be_naive = false;
	auto space1 = NP::Global::State_space<dtime_t>::explore(prob1, opts);
	CHECK(space1->is_schedulable());

	auto in_minus2 = std::istringstream(sn_susp_jobs_file_minus_2);
	susp_dag_in_1 = std::istringstream(uniproc_sn_susp_dag_file);
	auto platform_spec_two_cores_in_2 = std::istringstream(platform_spec_two_cores_2);

	NP::Scheduling_problem<dtime_t> prob2{
		NP::parse_csv_job_file<dtime_t>(in_minus2),
		NP::parse_precedence_file<dtime_t>(susp_dag_in_1),
		NP::parse_platform_spec_csv<dtime_t>(platform_spec_two_cores_in_2) };

	opts.be_naive = false;
	auto space2 = NP::Global::State_space<dtime_t>::explore(prob2, opts);
	CHECK(space1->is_schedulable());

	for (const NP::Job<dtime_t>& j1 : prob1.jobs) {
		for (const NP::Job<dtime_t>& j0 : prob0.jobs) {
			if (j0.get_id() == j1.get_id()) {
				CHECK(space0->get_finish_times(j0).from() == space1->get_finish_times(j1).from());
				CHECK(space0->get_finish_times(j0).until() == space1->get_finish_times(j1).until());
				break;
			}
		}
	}

	for (const NP::Job<dtime_t>& j1 : prob2.jobs) {
		for (const NP::Job<dtime_t>& j0 : prob0.jobs) {
			if (j0.get_id() == j1.get_id()) {
				CHECK(space0->get_finish_times(j0).from() == space2->get_finish_times(j1).from());
				CHECK(space0->get_finish_times(j0).until() == space2->get_finish_times(j1).until());
				break;
			}
		}
	}
}
