#pragma once

#include "config.hpp"
#include "server.hpp"

namespace project {
    /// The application object.
    /// There shall be one.
    /// So no need to be owned by a shared ptr
    struct app {
        app(net::executor exec);

        void run();

    private:

        void handle_run();

        // The application's executor.
        // In a multi-threaded application this would be a strand.
        net::executor exec_;
        net::signal_set signals_;
        server server_;
    };
}
