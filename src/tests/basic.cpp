#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <iostream>

#include "index_set.hpp"
#include "jobs.hpp"
#include "global/space.hpp"

using namespace NP;

static const auto inf = Time_model::constants<dtime_t>::infinity();

TEST_CASE("Intervals") {
	auto i1 = Interval<dtime_t>{10, 20};
	auto i2 = Interval<dtime_t>{15, 25};
	//to check if i1 and  i3 are not disjoint as they are consecutive intervals
	auto i3 = Interval<dtime_t>{21, 30};
	auto i4 = Interval<dtime_t>{5,  45};
	//to check if i1 and i5 are disjoint as they are not consecutive nor are they joint
	auto i5 = Interval<dtime_t>{22, 30};

	Interval<dtime_t> ivals[]{i1, i2, i3, i4};

	CHECK(i1.intersects(i2));
	CHECK(i2.intersects(i3));
	//to check if i1 and  i3 are not disjoint as they are consecutive intervals
	CHECK(i1.intersects(i3));
	//to check if i1 and i5 are disjoint as they are not consecutive nor are they joint
	CHECK(i1.disjoint(i5));

	for (auto i : ivals)
		CHECK(i.intersects(i4));

	CHECK(i1.merge(i2).merge(i3) == Interval<dtime_t>(10, 30));

	//to check if i1 and  i3 are not disjoint as they are consecutive intervals
	CHECK(i1.merge(i3) == Interval<dtime_t>(10,30));	

	CHECK(Interval<dtime_t>(10, 20).intersects(Interval<dtime_t>(10, 20)));
}



TEST_CASE("Job hashes work") {
	Job<dtime_t> j1{9,  Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 13), 60, 60, 0, 0};
	Job<dtime_t> j2{9,  Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 13), 60, 60, 0, 0};
	Job<dtime_t> j3{10, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 13), 60, 60, 0, 2};

	auto h = std::hash<Job<dtime_t>>{};

	CHECK(h(j1) == h(j2)); // The job index (last argument) can be used to make the hash uniq.
	CHECK(h(j3) != h(j1));
}



TEST_CASE("state space") {

	NP::Global::Schedule_node<dtime_t> n0{1};

	CHECK(n0.finish_range().from() == 0);
	CHECK(n0.finish_range().until() == 0);

	Job<dtime_t> j1{10, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 13), 60, 60, 0};

	CHECK(j1.least_exec_time() == 3);
	CHECK(j1.maximal_exec_time() == 13);
	CHECK(j1.earliest_arrival() == 0);
	CHECK(j1.latest_arrival() == 0);
}


TEST_CASE("bool vector assumptions") {
	std::vector<bool> v1(100);

	CHECK(v1.size() == 100);
	CHECK(!v1[10]);
	v1[10] = true;

	std::vector<bool> v2(400);

	v2 = v1;
	CHECK(v2.size() == 100);
	CHECK(v2.capacity() > v1.capacity());

	v1.resize(150);
	CHECK(v1.size() == 150);
	CHECK(!v1[149]);

	std::vector<bool> v3(400);
	std::copy(v1.begin(), v1.end(), v3.begin());
	CHECK(v3.size() == 400);
	CHECK(v3.capacity() > v1.capacity());
	CHECK(v3[10]);
}


TEST_CASE("[basic] index set")
{
	NP::Index_set empty;
	NP::Index_set all;

	CHECK(empty.is_subset_of(all));
	CHECK(empty.size() == 0);

	all.add(10);
	all.add(20);
	all.add(30);

	CHECK(all.contains(10));
	CHECK(!all.contains(29));
	//CHECK(all.size() == 3); //the definition of size was changed

	CHECK(!all.is_subset_of(empty));

	NP::Index_set some;
	some.add(10);
	some.add(20);

	CHECK(some.is_subset_of(all));
	CHECK(!all.is_subset_of(some));
	//CHECK(some.size() == 2); //the definition of size was changed

	std::vector<std::size_t> a{10, 20};
	std::vector<std::size_t> b{30, 20};
	std::vector<std::size_t> c{30, 40};

	CHECK(all.includes(a));
	CHECK(all.includes(b));
	CHECK(!all.includes(c));
}

