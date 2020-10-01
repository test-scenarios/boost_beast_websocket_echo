#include "application.hpp"
#include "ssl.hpp"

#include <fmt/printf.h>

int
main()
{
    using namespace project;

    auto ioc     = net::io_context();
    auto ssl_ctx = ssl::context(ssl::context::tlsv12_client);
    ssl_ctx.set_default_verify_paths();
    ssl_ctx.set_verify_mode(ssl::context::verify_peer);

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