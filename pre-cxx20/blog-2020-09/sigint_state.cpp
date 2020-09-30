#include "sigint_state.hpp"

#include <boost/core/ignore_unused.hpp>
#include <fmt/printf.h>

namespace project
{
    using namespace std::literals;

    sigint_state::sigint_state(const sigint_state::executor_type &exec)
    : exec_(exec)
    , sigs_(get_executor())
    , timer_(get_executor())
    {
    }

    auto
    sigint_state::get_executor() const -> executor_type const &
    {
        return exec_;
    }

    void
    sigint_state::start()
    {
        net::dispatch(get_executor(), [this] {
            sigs_.add(SIGINT);
            await_signal();
            state_ = waiting;
            fmt::print(stdout, "Press ctrl-c to interrupt.\n");
        });
    }

    void
    sigint_state::stop()
    {
        net::dispatch(get_executor(), [this] { sigs_.cancel(); });
    }

    void
    sigint_state::handle_signal(error_code const &ec, int sig)
    {
        if (ec)
        {
            if (ec != net::error::operation_aborted)
            {
                fmt::print(stderr, "Fatal error in signal handler: {}", ec);
                std::exit(100);
            }
        }
        else
        {
            boost::ignore_unused(sig);
            BOOST_ASSERT(sig == SIGINT);
            event_sigint();
        }
    }

    void
    sigint_state::event_sigint()
    {
        switch (state_)
        {
        case waiting:
            fmt::print(stderr,
                       "Interrupt detected. Press ctrl-c again within 5 "
                       "seconds to exit\n");
            state_ = confirming;
            await_timer();
            await_signal();
            break;

        case confirming:
            fmt::print(stderr, "Interrupt confirmed. Shutting down\n");
            timer_.cancel();
            state_ = exit_state;
            emit_signals();
            break;

        case inactive:
        case exit_state:
            break;
        }
    }

    void
    sigint_state::handle_timer(const error_code &ec)
    {
        if (ec)
            return;

        switch (state_)
        {
        case waiting:
        case inactive:
        case exit_state:
            break;
        case confirming:
            fmt::print(stderr, "Interrupt unconfirmed. Ignoring\n");
            state_ = waiting;
        }
    }

    void
    sigint_state::await_signal()
    {
        sigs_.async_wait(
            [this](error_code const &ec, int sig) { handle_signal(ec, sig); });
    }

    void
    sigint_state::await_timer()
    {
        timer_.expires_after(5s);
        timer_.async_wait([this](error_code const &ec) { handle_timer(ec); });
    }

    void
    sigint_state::emit_signals()
    {
        BOOST_ASSERT(state_ == exit_state);
        auto sigs = std::move(signals_);
        for (auto &sig : sigs)
            sig();
    }

    void
    sigint_state::add_slot(std::function< void() > slot)
    {
        net::dispatch(get_executor(), [this, slot = std::move(slot)]() mutable {
            switch (state_)
            {
            case exit_state:
                slot();
                break;

            case inactive:
            case confirming:
            case waiting:
                signals_.push_back(std::move(slot));
                break;
            }
        });
    }

}   // namespace project