#pragma once

#include "ssl.hpp"
#include "websocket.hpp"

#include <boost/beast/ssl.hpp>
#include <deque>

namespace project
{
    struct wss_transport
    {
        using executor_type = net::any_io_executor;

        wss_transport(net::io_context::executor_type exec,
                      ssl::context &                 ssl_ctx);

        void start();

        void stop();

      protected:
        //
        // internal interface for derived classes
        //
        void
        initiate_connect(std::string host,
                         std::string port,
                         std::string target);

        void
        send_text_frame(std::string frame);

        void
        initiate_close();

        auto get_executor() const -> executor_type const&;

      private:
        //
        // virtual interface for communicating events to the derived class
        //

        virtual void on_start();

        /// called when the transport layer has connected
        virtual void
        on_transport_up();

        /// Called once on the first transport error to give the derived class
        /// a chance to clean up. (implies no on_close)
        virtual void
        on_transport_error(std::exception_ptr ep);

        /// Called to notify the derived class that a text frame has arrived
        virtual void
        on_text_frame(std::string_view frame);

        /// Called to notify the derived class that the transport has been
        /// gracefully closed (implies no error)
        virtual void
        on_close();

      private:

        void event_transport_up();

        void event_transport_error(error_code const& ec);
        void event_transport_error(std::exception_ptr ep);

        void start_sending();
        void handle_send(error_code const& ec, std::size_t bytes_transferred);
        void start_reading();
        void handle_read(error_code const& ec, std::size_t bytes_transferred);

      private:

        using layer_0 = beast::tcp_stream;
        using layer_1 = beast::ssl_stream< layer_0 >;
        using websock = websocket::stream< layer_1 >;

        executor_type exec_;
        websock       websock_;

        // send_state - data to control sending data

        std::deque<std::string> send_queue_;
        enum send_state
        {
            not_sending,
            sending
        } send_state_ = not_sending;

        // overall state of this transport

        enum state_type
        {
            not_started,
            connecting,
            connected,
            closing,
            finished
        } state_ = not_started;

        beast::flat_buffer rx_buffer_;

        // internal details

        struct connect_op;
    };
}   // namespace project