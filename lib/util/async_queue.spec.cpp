#include <catch2/catch.hpp>

#include "async_queue.hpp"

using namespace beast_fun_times::util;

TEST_CASE("async_queue")
{
    auto ioc = net::io_context(1);
    auto e = ioc.get_executor();
    auto q = async_queue<std::string>(e);

    auto ioc2 = net::io_context(1);
    auto e2 = ioc2.get_executor();

    error_code error;
    std::string value;

    auto run = [](net::io_context& ioc)
    {
        auto s = ioc.run();
        ioc.restart();
        return s;
    };

    auto poll = [](net::io_context& ioc)
    {
        auto s = ioc.poll();
        ioc.restart();
        return s;
    };

    auto make_handler = [&]()
    {
        return bind_executor(e2, [&](error_code ec, std::string s) {
            error = ec;
            value = s;
        });
    };

    SECTION("stop")
    {
        q.async_pop(make_handler());
        q.stop();

        CHECK(poll(ioc) == 2);
        CHECK(run(ioc2) == 1);
        CHECK(run(ioc) == 0);

        CHECK(error.message() == "Operation canceled");
    }

    SECTION("3 values")
    {
        q.push("a");
        q.push("b");
        q.push("c");
        CHECK(poll(ioc) == 3);

        q.async_pop(make_handler());
        CHECK(poll(ioc) == 1);
        CHECK(run(ioc2) == 1);
        CHECK(error.message() == "Success");
        CHECK(value == "a");

        q.async_pop(make_handler());
        CHECK(poll(ioc) == 1);
        CHECK(run(ioc2) == 1);
        CHECK(error.message() == "Success");
        CHECK(value == "b");

        q.stop();
        CHECK(poll(ioc) == 1);
        q.async_pop(make_handler());
        CHECK(poll(ioc) == 1);
        CHECK(run(ioc2) == 1);
        CHECK(error.message() == "Operation canceled");
        CHECK(value == "");


    }

}