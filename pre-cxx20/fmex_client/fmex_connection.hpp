#pragma once
#include "connection_base.hpp"

namespace project
{
    struct ExchangeConnection : ConnectionBase
    {
        ExchangeConnection(net::io_context::executor_type const &exec,
                           ssl::context &                        ssl_context)
            : ConnectionBase(exec, ssl_context, "Fmex")
            , ping_timer_(get_executor())
        {
        }

      private:
        void
        handle_connect_command() override
        {
            notify_connect("api.fmex.com", "443", "/v2/ws");
        }

        void
        on_error(error_code const &ec) override
        {
            fmt::print(stderr, "{} reports error: {}\n", name, ec);
            ping_stop();
        }

        void
        on_transport_up() override
        {
            ping_start();
        }

        void
        on_text_frame(std::string_view frame) override
        {
            auto j = json::parse(frame, nullptr, true);

            const std::string j_type = j["type"];
            if (j_type == "hello")
            {
                json j_out = { { "cmd", "sub" },
                               { "args", { "ticker.btcusd_p" } },
                               { "id", "random_id.me.hk" } };
                notify_send(j_out.dump());
            }
            else
            {
                if (j_type == "ticker.btcusd_p")
                {
                    int64_t now =
                        std::chrono::duration_cast< std::chrono::milliseconds >(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
                    int64_t server_ts = j["ts"];
                    fmt::print("now: {}, server_ts: {}. lag: {}\n",
                               now,
                               server_ts,
                               now - server_ts);
                }
            }
        }

      private:
        //
        // JSON ping is an orthogonal region, active while there is a connection
        //

        boost::asio::high_resolution_timer ping_timer_;

        enum ping_state
        {
            ping_not_active,
            ping_waiting_timer,
            ping_exited
        } ping_state_ = ping_not_active;

        // enter the ping state
        void
        ping_start()
        {
            assert(ping_state_ == ping_not_active);
            ping_enter_waiting_state();
        }

        // exit the ping state
        void
        ping_stop()
        {
            switch (ping_state_)
            {
            case ping_not_active:
            case ping_exited:
                break;
            case ping_waiting_timer:
                ping_timer_.cancel();
            }
            ping_state_ = ping_exited;
        }

        void
        ping_enter_waiting_state()
        {
            using namespace std::literals;

            ping_state_ = ping_waiting_timer;

            ping_timer_.expires_after(5s);
            ping_timer_.async_wait([this](boost::system::error_code const &ec) {
                ping_on_timer(ec);
            });
        }

        void
        ping_on_timer(boost::system::error_code const &ec)
        {
            // If the ping state has exited, ec will be operation_aborted so we
            // can use this rather than checking for a state transition during
            // the wait period
            if (ec)
            {
                ping_state_ = ping_exited;
                return fail(name, ec, "ping_on_timer");
            }

            json j_out = {
                { "cmd", "ping" },
                { "args",
                         { std::chrono::duration_cast< std::chrono::milliseconds >(
                             std::chrono::system_clock::now().time_since_epoch())
                               .count() } },
                { "id", "random_id.me.hk" }
            };

            // send the "send" event into the "send" orthogonal region
            notify_send(j_out.dump());

            // and re-enter the waiting state
            ping_enter_waiting_state();
        }
    };

}