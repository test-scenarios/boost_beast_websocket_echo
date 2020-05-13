#include "connection.hpp"

#include <iostream>

namespace project
{
    namespace
    {
        const auto server_endpoint = net::ip::tcp::endpoint(net::ip::address_v4::loopback(), 4321);
    }

    connection_impl::connection_impl(net::executor exec)
    : stream_(net::ip::tcp::socket(exec))
    , delay_timer_(exec)
    {
        auto ep = net::ip::tcp::endpoint(net::ip::address_v4::any(), 0);
        stream_.next_layer().open(ep.protocol());

        // this is so the socket will actually have a local endpoint
        stream_.next_layer().bind(ep);
    }

    auto connection_impl::get_executor() -> net::executor { return stream_.get_executor(); }

    auto connection_impl::local_endpoint() -> net::ip::tcp::endpoint { return stream_.next_layer().local_endpoint(); }

    void connection_impl::run()
    {
        net::dispatch(stream_.get_executor(), [self = this->shared_from_this()] { self->handle_run(); });
    }

    void connection_impl::handle_run()
    {
        stream_.next_layer().async_connect(
            server_endpoint,
            net::bind_executor(get_executor(), [self = shared_from_this()](error_code ec) {
                self->handle_connect(ec);
            }));
    }

    void connection_impl::handle_connect(error_code ec)
    {
        if (!ec)
        {
            initiate_handshake();
        }
    }

    void connection_impl::initiate_handshake()
    {
        stream_.async_handshake(
            "localhost", "/", net::bind_executor(stream_.get_executor(), [self = shared_from_this()](error_code ec) {
                self->handle_handshake(ec);
            }));
    }

    void connection_impl::handle_handshake(error_code ec)
    {
        if (ec_)
        {
            // we've been stopped
        }
        else if (ec)
        {
            // connection error
        }
        else
        {
            // happy days
            state_ = chatting;
            initiate_rx();
            maybe_send_next();
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
    void connection_impl::initiate_rx()
    {
        assert(state_ == chatting);
        assert(!ec_);
        stream_.async_read(rxbuffer_, [self = this->shared_from_this()](error_code ec, std::size_t bytes_transferred) {
            self->handle_rx(ec, bytes_transferred);
        });
    }
    void connection_impl::handle_rx(error_code ec, std::size_t bytes_transferred)
    {
        if (ec)
        {
            std::cout << "rx error: " << ec.message() << std::endl;
        }
        else
        {
            // handle the read here
            auto message = beast::buffers_to_string(rxbuffer_.data());
            std::cout << local_endpoint() << " received: " << message << "\n";
            rxbuffer_.consume(message.size());

            // keep reading until error
            initiate_rx();

            // in this case we are merely going to echo the message back.
            // but we'll use the public interface in order to demonstrate it
            send(std::move(message));
        }
    }
    void connection_impl::send(std::string msg)
    {
        net::dispatch(
            net::bind_executor(get_executor(), [self = shared_from_this(), msg = std::move(msg)]() mutable {
                self->tx_queue_.push(std::move(msg));
                self->maybe_send_next();
            }));
    }
    void connection_impl::maybe_send_next()
    {
        if (ec_ || state_ != chatting || sending_state_ == sending || tx_queue_.empty())
            return;

        initiate_tx();
    }
    void connection_impl::initiate_tx()
    {
        assert(sending_state_ == send_idle);
        assert(!ec_);
        assert(!tx_queue_.empty());

        sending_state_ = sending;
        stream_.async_write(net::buffer(tx_queue_.front()), [self = shared_from_this()](error_code ec, std::size_t) {
            // we don't care about bytes_transferred
            self->handle_tx(ec);
        });
    }
    void connection_impl::handle_tx(error_code ec)
    {
        if (ec)
        {
            std::cout << "failed to send message: " << tx_queue_.front() << " because " << ec.message() << std::endl;
        }
        else
        {
            tx_queue_.pop();
            sending_state_ = send_idle;
            maybe_send_next();
        }
    }
}   // namespace project
