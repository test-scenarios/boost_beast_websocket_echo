#pragma once

#include "config.hpp"

#include <deque>
#include <memory>
#include <queue>

namespace project
{
    struct connection_impl : std::enable_shared_from_this< connection_impl >
    {
        using transport = net::ip::tcp::socket;
        using stream    = websocket::stream< transport >;

        connection_impl(net::ip::tcp::socket sock);

        void run();
        void stop();

        void send(std::string msg);

      private:
        void handle_run();
        void handle_stop();

        void handle_accept(error_code ec);

        void initiate_rx();
        void handle_rx(error_code ec, std::size_t bytes_transferred);

        void maybe_send_next();
        void initiate_tx();
        void handle_tx(error_code ec);

      private:
        stream stream_;

        beast::flat_buffer rxbuffer_;

        // elements in a std deque have a stable address, so this means we don't need t make copies of messages
        std::queue< std::string, std::deque< std::string > > tx_queue_;

        error_code ec_;

        enum
        {
            handshaking,
            chatting,
        } state_ = handshaking;

        enum
        {
            send_idle,
            sending
        } sending_state_ = send_idle;
    };
}   // namespace project