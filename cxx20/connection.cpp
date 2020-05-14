#include "connection.hpp"

#include <iostream>

namespace project
{
    connection_impl::connection_impl(net::ip::tcp::socket sock)
    : stream_(std::move(sock))
    {
    }

    void connection_impl::run()
    {
        net::co_spawn(
            stream_.get_executor(),
            [self = this->shared_from_this()]() -> net::awaitable< void > {
                try
                {
                    co_await self->handle_run();
                }
                catch (system_error &se)
                {
                    auto code = se.code();
                    if (code == net::error::operation_aborted || code == websocket::error::closed)
                    {
                        // fine
                    }
                    else
                    {
                        std::cout << "connection error: " << code.message() << "\n";
                    }
                }
                catch (std::exception &e)
                {
                    std::cout << "connection error: " << e.what() << "\n";
                }
            },
            net::detached);
    }

    net::awaitable< void > connection_impl::handle_run()
    {
        co_await stream_.async_accept(net::use_awaitable);
        // handle the case where we were stopped before this coroutine completed
        if (ec_)
            co_return;

        state_ = chatting;
        initiate_send();

        beast::flat_buffer rxbuffer;
        while (1)
        {
            auto bytes   = co_await stream_.async_read(rxbuffer, net::use_awaitable);
            auto message = beast::buffers_to_string(rxbuffer.data());
            std::cout << " received: " << message << "\n";
            rxbuffer.consume(message.size());

            // in this case we are merely going to echo the message back.
            // but we'll use the public interface in order to demonstrate it
            send(std::move(message));
        }
    }

    void connection_impl::stop()
    {
        net::dispatch(net::bind_executor(stream_.get_executor(), [self = shared_from_this()] { self->handle_stop(); }));
    }

    void connection_impl::handle_stop()
    {
        // we set an error code in order to handle the crossing case where the accept has completed but its handler
        // has not yet been sceduled for invoacation
        ec_ = net::error::operation_aborted;

        if (state_ == chatting)
            stream_.async_close(websocket::close_code::going_away, [self = shared_from_this()](error_code ec) {
                // very important that we captured self here!
                // the websocket stream must stay alive while there is an outstanding async op
                std::cout << "result of close: " << ec.message() << std::endl;
            });
        else
            stream_.next_layer().cancel();
    }

    void connection_impl::send(std::string msg)
    {
        net::co_spawn(
            stream_.get_executor(),
            [self = shared_from_this(), msg = std::move(msg)]() mutable -> net::awaitable< void > {
                try
                {
                    self->tx_queue_.push(std::move(msg));
                    co_await self->maybe_send_next();
                }
                catch (...)
                {
                }
            },
            net::detached);
    }

    void connection_impl::initiate_send()
    {
        net::co_spawn(
            stream_.get_executor(),
            [self = shared_from_this()]() -> net::awaitable< void > {
                try
                {
                    co_await self->maybe_send_next();
                }
                catch (...)
                {
                }
            },
            net::detached);
    }

    net::awaitable< void > connection_impl::maybe_send_next()
    {
        while (!ec_ && state_ == chatting && sending_state_ != sending && !tx_queue_.empty())
        {
            sending_state_ = sending;
            co_await stream_.async_write(net::buffer(tx_queue_.front()), net::use_awaitable);
            tx_queue_.pop();
            sending_state_ = send_idle;
        }
        co_return;
    }
}   // namespace project
