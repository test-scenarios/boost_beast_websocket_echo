#include <catch2/catch.hpp>

#include "util/poly_handler.hpp"

using namespace beast_fun_times::util;
using namespace std::literals;

TEST_CASE("util::poly_handler")
{
    std::string target;
    auto small = [&target](std::string s)
    {
        target = s;
    };

    auto big = [&target, x="x"s, y="y"s, z="z"s](std::string s)
    {
        target = s + " " + x + y + z;
    };

    auto f = poly_handler<void(std::string)>();
    CHECK(f.has_value() == false);
    CHECK(bool(f) == false);

    CHECK_THROWS_AS(f("test"s), std::bad_function_call);

    f = std::move(small);
    CHECK(f.has_value() == true);
    CHECK(bool(f) == true);
    CHECK_NOTHROW(f("test"s));
    CHECK(target == "test");
    CHECK(f.has_value() == false);

    f = std::move(big);
    CHECK(f.has_value() == true);
    CHECK(bool(f) == true);
    CHECK_NOTHROW(f("test"s));
    CHECK(target == "test xyz");
    CHECK(f.has_value() == false);

}