#include "doctest.h"

#include <algorithm>
#include <iostream>
#include <memory>

#include "index_set.hpp"

using namespace NP;


TEST_CASE("Index set: Set Subtract") {

    SUBCASE("Standard subtraction") {
        // Sets to subtract
        Index_set a;
        Index_set b;

        // Control set
        Index_set d;

        // Fill sets
        a.add(1);
        a.add(2);
        a.add(4);
        b.add(2);
        b.add(3);

        // {1, 2, 4} - {2, 3} = {1, 4}
        d.add(1);
        d.add(4);

        std::unique_ptr<Index_set> c = a.set_subtract(b);
        CHECK(*c == d);
    }

    SUBCASE("Empty b") {
        // Sets to subtract
        Index_set a;
        Index_set b;

        // Control set
        Index_set d;
        a.add(1);
        a.add(2);
        a.add(4);

        // {1, 2, 4} - {} = {1, 2, 4}
        d.add(1);
        d.add(2);
        d.add(4);

        std::unique_ptr<Index_set> c = a.set_subtract(b);
        CHECK(*c == d);
    }

    SUBCASE("Sets without any intersection") {
        // Sets to subtract
        Index_set a;
        Index_set b;

        // Control set
        Index_set d;
        a.add(2);
        a.add(4);

        b.add(1);
        b.add(3);

      
        // {2, 4} - {1, 3} = {2, 4}
        d.add(2);
        d.add(4);

        std::unique_ptr<Index_set> c = a.set_subtract(b);
        CHECK(*c == d);
    }
    
    SUBCASE("Empty a") {
        // Sets to subtract
        Index_set a;
        Index_set b;

        // Control set
        Index_set d;

        b.add(1);
        b.add(3);
      
        // {} - {1, 3} = {}

        std::unique_ptr<Index_set> c = a.set_subtract(b);
        CHECK(*c == d);
    }

}