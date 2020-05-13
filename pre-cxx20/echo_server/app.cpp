#include "app.hpp"
#include <iostream>

namespace project {
    app::app(net::executor exec) : exec_(exec), signals_(exec, SIGINT, SIGHUP),
                                   server_(exec) {}

    void app::handle_run() {
        signals_.async_wait([this](error_code ec, int sig) {
            if (!ec) {
                std::cout << "signal: " << sig << std::endl;
                server_.stop();
            }
        });

        server_.run();
    }

    void app::run() {
        // get onto the correct executor. It saves confusion later.
        net::dispatch(net::bind_executor(exec_, [this] {
            handle_run();
        }));
    }
}

