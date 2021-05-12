#include "app.hpp"

#include <iostream>

namespace project
{
    app::app(net::any_io_executor exec)
    : exec_(exec)
    , signals_(exec, SIGINT, SIGHUP)
    , console_(exec)
    , clients_(exec)
    {
    }

    void app::handle_run()
    {
        signals_.async_wait([this](error_code ec, int sig) {
            if (!ec)
            {
                std::cout << "signal: " << sig << std::endl;
                clients_.stop();
                console_.cancel();
            }
        });

        clients_.run();
        initiate_console();
    }

    void app::run()
    {
        // get onto the correct executor. It saves confusion later.
        net::dispatch(net::bind_executor(exec_, [this] { handle_run(); }));
    }

    void app::initiate_console()
    {
        console_.async_run(
            net::bind_executor(exec_, [this](error_code ec, console_event event) { this->handle_console(ec, event); }));
    }

    void app::handle_console(error_code ec, console_event event)
    {
        if (!ec)
        {
            switch (event)
            {
            case console_event::quit:
                clients_.stop();
                signals_.cancel();
                break;
            }
        }
    }

}   // namespace project
