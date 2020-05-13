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
        net::awaitable<void> handle_run();
        void handle_stop();

        void initiate_send();
        net::awaitable<void> maybe_send_next();

      private:
        stream stream_;

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