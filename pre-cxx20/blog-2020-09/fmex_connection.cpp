#include "fmex_connection.hpp"

#include "json.hpp"

#include <fmt/printf.h>

namespace project
{
    using namespace std::literals;

    auto
    timestamp() -> std::uint64_t
    {
        return std::chrono::duration_cast< std::chrono::milliseconds >(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    fmex_connection::fmex_connection(net::io_context::executor_type exec,
                                     ssl::context &                 ssl_ctx)
    : wss_transport(exec, ssl_ctx)
    , ping_timer_(get_executor())
    {
    }

    void
    fmex_connection::on_start()
    {
        fmt::print(stdout, "fmex: initiating connection\n");
        initiate_connect("api.fmex.com", "443", "/v2/ws");
    }
    void
    fmex_connection::on_transport_up()
    {
        fmt::print(stdout, "fmex: transport up\n");
        ping_enter_state();
    }

    void
    fmex_connection::on_transport_error(std::exception_ptr ep)
    {
        ping_timer_.cancel();
        try
        {
            std::rethrow_exception(ep);
        }
        catch (system_error &se)
        {
            fmt::print(stderr,
                       "fmex: transport error : {} : {} : {}\n",
                       se.code().category().name(),
                       se.code().value(),
                       se.code().message());
        }
        catch (std::exception &e)
        {
            fmt::print(stderr, "fmex: transport exception : {}\n", e.what());
        }
    }

    void
    fmex_connection::on_text_frame(std::string_view frame)
    try
    {
        auto jframe =
            json::parse(json::string_view(frame.data(), frame.size()));

        // dispatch on frame type

        auto &type = jframe.as_object().at("type");
        if (type == "hello")
        {
            on_hello();
        }
        else if (type == "ping")
        {
            ping_event_pong(jframe);
        }
        else if (type.as_string().starts_with("ticker."))
        {
            fmt::print(stdout,
                       "fmex: tick {} : {}\n",
                       type.as_string().subview(7),
                       jframe.as_object().at("ticker"));
        }
    }
    catch (...)
    {
        fmt::print(stderr, "text frame is not json : {}\n", frame);
        throw;
    }

    void
    fmex_connection::on_close()
    {
        fmt::print(stdout, "fmex: closed\n");
    }

    void
    fmex_connection::ping_enter_state()
    {
        BOOST_ASSERT(ping_state_ == ping_not_started);
        ping_enter_wait();
    }

    void
    fmex_connection::ping_enter_wait()
    {
        ping_state_ = ping_wait;

        ping_timer_.expires_after(5s);

        ping_timer_.async_wait([this](error_code const &ec) {
            if (!ec)
                ping_event_timeout();
        });
    }

    void
    fmex_connection::ping_event_timeout()
    {
        ping_state_ = ping_waiting_pong;

        auto  frame = json::value();
        auto &o     = frame.emplace_object();
        o["cmd"]    = "ping";
        o["id"]     = "my_ping_ident";
        o["args"].emplace_array().push_back(timestamp());
        send_text_frame(json::serialize(frame));
    }

    void
    fmex_connection::ping_event_pong(json::value const &frame)
    {
        ping_enter_wait();
    }

    void
    fmex_connection::on_hello()
    {
        fmt::print(stdout, "fmex: hello\n");
        request_ticker("btcusd_p");
    }

    void
    fmex_connection::request_ticker(std::string_view ticker)
    {
        auto  frame = json::value();
        auto &o     = frame.emplace_object();
        o["cmd"]    = "sub";
        o["id"]     = "some_id";
        o["args"].emplace_array().push_back([&] {
            auto result = json::string("ticker.");
            result.append(ticker.begin(), ticker.end());
            return result;
        }());
        send_text_frame(json::serialize(frame));
    }
}   // namespace project