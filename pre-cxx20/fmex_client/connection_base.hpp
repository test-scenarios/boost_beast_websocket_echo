#pragma once
#include "config.hpp"
#include "stop_register.hpp"

#include <boost/beast/core.hpp>
#include <deque>

namespace project
{
    class ConnectionBase
    {
      public:
        using executor_type = net::strand< net::io_context::executor_type >;

      private:
        // create a strand to act as the default executor for all io objects in
        // this Exchange object
        executor_type exec_;

        websocket::stream< beast::ssl_stream< beast::tcp_stream > > ws;
        beast::flat_buffer                                          buffer {};

        // A queue of text frames to send. Note that a std::deque's elements
        // have a stable address even after insertion/removal. This means we can
        // reliably use front() and back() during async operations
        std::deque< std::string > tx_queue_ {};

        //
        // Record the state of the "send" orthogonal region
        enum send_state
        {
            send_idle,
            send_sending,
        } send_state_ = send_idle;

        //
        // Create a mechanism to safely stop the connection
        //

        stop_register stop_register_;
        stop_token    my_stop_token_;

      protected:
        void
        fail(const std::string &exchange,
             beast::error_code  ec,
             char const *       what);

        void
        fail(const std::string &exchange, char const *what);

        void
        succeed(const std::string &exchange, char const *what);

      protected:
        void
        notify_name(std::string arg);

        void
        notify_connect(std::string host, std::string port, std::string target);

      private:
        virtual void
        on_transport_up() = 0;

        virtual void
        on_error(error_code const &ec) = 0;

        virtual void
        on_text_frame(std::string_view frame) = 0;

      public:
        auto
        get_executor() const -> executor_type
        {
            return exec_;
        }

        const std::string name;

        ConnectionBase(net::io_context::executor_type const &exec,
                       ssl::context &                        ctx,
                       std::string                           name);

        virtual ~ConnectionBase() {};

        void
        start();

        void
        stop();

      private:
        virtual void
        handle_connect_command() = 0;

        void
        initiate_close();

        void
        enter_read_state();

        void
        on_read(beast::error_code ec, std::size_t bytes_transferred);

        void
        on_close(beast::error_code ec);

      protected:
        //
        // "send" state - orthogonal region active while connected
        //

        void
        notify_send(std::string frame);

      private:
        void
        initiate_send();

        void
        on_write(beast::error_code ec, std::size_t bytes_transferred);
    };

}   // namespace project