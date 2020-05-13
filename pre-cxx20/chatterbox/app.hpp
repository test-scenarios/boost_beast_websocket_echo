#pragma once

#include "config.hpp"
#include "connection_pool.hpp"
#include "console.hpp"

namespace project {
    /// The application object.
    /// There shall be one.
    /// So no need to be owned by a shared ptr
    struct app {
        app(net::executor exec);

        void run();

    private:

        void handle_run();
        void initiate_console();
        void handle_console(error_code ec, console_event event);

        // The application's executor.
        // In a multi-threaded application this would be a strand.
        net::executor exec_;
        net::signal_set signals_;
        connection_pool clients_;
        console console_;
    };
}
