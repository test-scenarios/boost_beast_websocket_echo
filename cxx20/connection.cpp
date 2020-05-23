#include "connection.hpp"

#include "states.hpp"

#include <iostream>

namespace project
{
    connection_impl::connection_impl(net::ip::tcp::socket sock)
    : chat_state< net::ip::tcp::socket >::chat_state(std::move(sock))
    {
    }

    void connection_impl::run()
    {
        // callback which will happen zero or one times after websocket handshake
        // The rx state will not make progress until this function returns, so it should not block
        auto on_connect = [this]() {
            net::co_spawn(
                get_executor(),
                [this]() -> net::awaitable< void > { co_await run_state(tx_state, stream); },
                spawn_handler("tx_state"));
        };

        // callback which will happen zero or more times, as each message is received.
        // The rx state will not make progress until this function returns, so it should not block
        auto on_message = [this](std::string message) { this->send(std::move(message)); };

        net::co_spawn(
            this->get_executor(),
            [this, on_connect, on_message]() -> net::awaitable< void > {
                co_await run_state(*this, on_connect, on_message);
            },
            spawn_handler("run"));
    }

    void connection_impl::stop()
    {
        net::co_spawn(
            get_executor(),
            [this]() -> net::awaitable< void > { co_await notify_error(net::error::operation_aborted); },
            spawn_handler("stop"));
    }

    void connection_impl::send(std::string msg)
    {
        // this will "happen" on the correct executor
        tx_state.txqueue.push(std::move(msg));
    }

}   // namespace project
