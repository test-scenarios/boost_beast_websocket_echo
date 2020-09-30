#include "application.hpp"
#include "ssl.hpp"

#include <fmt/printf.h>

int
main()
{
    using namespace project;

    auto ioc     = net::io_context();
    auto ssl_ctx = ssl::context(ssl::context::sslv3_client);

    try
    {
        auto app = application(ioc.get_executor(), ssl_ctx);
        app.start();
        ioc.run();
    }
    catch (std::exception &e)
    {
        fmt::print(stderr, "Fatal: uncaught exception: {}", e.what());
        std::exit(100);
    }

    return 0;
}