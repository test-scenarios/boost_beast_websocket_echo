#include "net.hpp"

#include <functional>
#include <vector>

namespace project
{
    struct sigint_state
    {
        using executor_type = net::any_io_executor;

        auto
        get_executor() const -> executor_type const &;

        explicit sigint_state(executor_type const &exec);

        void
        start();

        void
        stop();

        void
        add_slot(std::function< void() > slot);

      private:
        void
        await_signal();

        void
        await_timer();

        void
        handle_signal(error_code const &ec, int sig);

        void
        handle_timer(error_code const &ec);

        void
        emit_signals();

        void
        event_sigint();

        executor_type                          exec_;
        asio::signal_set                       sigs_;
        net::high_resolution_timer             timer_;
        std::vector< std::function< void() > > signals_;

        enum state_type
        {
            inactive,
            waiting,
            confirming,
            exit_state
        } state_ = inactive;
    };
}   // namespace project