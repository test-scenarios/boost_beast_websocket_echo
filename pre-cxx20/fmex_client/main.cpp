#define FMT_HEADER_ONLY
#include <fmt/format.h>
#include <fmt/color.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

#include <string>
#include <memory>
#include <array>
#include <vector>
#include <chrono>
#include <algorithm>
#include <functional>
#include <numeric>
#include <thread>
#include <utility>
#include <deque>

inline void fail(const std::string& exchange, beast::error_code ec, char const* what) {
    fmt::print(stderr, fg(fmt::color::crimson), "{}: {}: {}\n", exchange, what, ec.message());
}

inline void fail(const std::string& exchange, char const* what) {
    fmt::print(stderr, fg(fmt::color::crimson), "{}: {}\n", exchange, what);
}

inline void succeed(const std::string& exchange, char const* what) {
    fmt::print(fg(fmt::color::steel_blue), "{}: {}\n", exchange, what);
}

class Exchange {
    // create a strand to act as the default executor for all io objects in this
    // Exchange object
    net::strand<net::io_context::executor_type> exec_;

    std::string ws_host;
    const std::string ws_port;
    const std::string ws_target;

    tcp::resolver resolver;
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws;
    beast::flat_buffer buffer;

    bool activated = false;

    // A queue of text frames to send. Note that a std::deque's elements have a stable address
    // even after insertion/removal. This means we can reliably use front() and back()
    // during async operations
    std::deque<std::string> tx_queue_;

    //
    // Record the state of the "send" orthogonal region
    enum send_state
    {
        send_idle,
        send_sending,
    } send_state_ = send_idle;

  private:

    void fail(const std::string& exchange, beast::error_code ec, char const* what) {
        ping_stop();
        fmt::print(stderr, fg(fmt::color::crimson), "{}: {}: {}\n", exchange, what, ec.message());
    }

    void fail(const std::string& exchange, char const* what) {
        ping_stop();
        fmt::print(stderr, fg(fmt::color::crimson), "{}: {}\n", exchange, what);
    }

    void succeed(const std::string& exchange, char const* what) {
        fmt::print(fg(fmt::color::steel_blue), "{}: {}\n", exchange, what);
    }


  public:
    const std::string name;

    Exchange(net::io_context& ioc, ssl::context& ctx) :
        exec_(net::make_strand(ioc.get_executor())),
        name("Fmex"),
        ws_host("api.fmex.com"),
        ws_port("443"),
        ws_target("/v2/ws"),
        resolver(exec_),
        ws(exec_, ctx),
        ping_timer_(exec_) {
    }
    virtual ~Exchange(){};

    void connect_websocket_server() {
        resolver.async_resolve(ws_host, ws_port,
                               beast::bind_front_handler(
                                   &Exchange::on_resolve,
                                   this));
    }

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
        if (ec) {
            return fail(name, ec, "resolve");
        }
        succeed(name, "resolve");

        beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

        beast::get_lowest_layer(ws).async_connect(results,
                                                  beast::bind_front_handler(
                                                      &Exchange::on_connect,
                                                      this));
    }

    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
        if (ec) {
            return fail(name, ec, "connect");
        }
        succeed(name, "connect");

        ws_host += ':' + std::to_string(ep.port());

        beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

        ws.next_layer().async_handshake(
            ssl::stream_base::client,
            beast::bind_front_handler(
                &Exchange::on_ssl_handshake,
                this));
    }

    void on_ssl_handshake(beast::error_code ec) {
        if (ec) {
            return fail(name, ec, "ssl_handshake");
        }
        succeed(name, "ssl_handshake");

        beast::get_lowest_layer(ws).expires_never();

        ws.set_option(
            websocket::stream_base::timeout::suggested(beast::role_type::client));

        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(http::field::user_agent,
                        "custom-websocket-client-async");
            }));

        ws.async_handshake(ws_host, ws_target,
                           beast::bind_front_handler(
                               &Exchange::on_handshake,
                               this));
    }

    void on_handshake(beast::error_code ec) {
        if (ec) {
            return fail(name, ec, "handshake");
        }
        succeed(name, "handshake");

        // Enter the "ping" orthogonal region
        ping_start();

        // There is not specific call to enter the "send" orthogonal region, but
        // if there were, it would be here.

        // enter the "read" orthogonal region
        ws.async_read(buffer,
                      beast::bind_front_handler(
                          &Exchange::on_read,
                          this));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            return fail(name, ec, "read");
        }
        succeed(name, "read");

        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        fmt::print("received: {}\n", beast::buffers_to_string(buffer.data()));

        auto j = json::parse(beast::buffers_to_string(buffer.data()), nullptr, false);
        buffer.consume(buffer.size());
        if (j.is_discarded()) {
            return fail(name, "json parse");
        }

        const std::string j_type = j["type"];
        if (j_type == "hello") {
            json j_out =
                {{"cmd", "sub"},
                 {"args", {"ticker.btcusd_p"}},
                 {"id", "random_id.me.hk"}};
            notify_send(j_out.dump());
        } else {
            if (j_type == "ticker.btcusd_p") {
                int64_t server_ts = j["ts"];
                fmt::print("now: {}, server_ts: {}. lag: {}\n", now, server_ts, now - server_ts);
            }
        }

        ws.async_read(buffer,
                      beast::bind_front_handler(
                          &Exchange::on_read,
                          this));

        // ws.async_close(websocket::close_code::normal,
        //                beast::bind_front_handler(
        //                    &Exchange::on_close,
        //                    this));
    }

    void on_close(beast::error_code ec) {
        if (ec) {
            return fail(name, ec, "close");
        }
        succeed(name, "close");
    }

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
    void ping_start()
    {
        assert(ping_state_ == ping_not_active);
        ping_enter_waiting_state();
    }

    // exit the ping state
    void ping_stop()
    {
        switch(ping_state_)
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
        ping_timer_.async_wait([this](boost::system::error_code const& ec)
                               {
                                  ping_on_timer(ec);
                               });
    }

    void ping_on_timer(boost::system::error_code const& ec)
    {
        // If the ping state has exited, ec will be operation_aborted so we can use this
        // rather than checking for a state transition during the wait period
        if (ec)
        {
            ping_state_ = ping_exited;
            return fail(name, ec, "ping_on_timer");
        }

        json j_out = {
            {"cmd", "ping"},
            {"args", {std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()}},
            {"id", "random_id.me.hk"}};

        // send the "send" event into the "send" orthogonal region
        notify_send(j_out.dump());

        // and re-enter the waiting state
        ping_enter_waiting_state();
    }

    //
    // "send" state - orthogonal region active while connected
    //

    void notify_send(std::string frame)
    {
        tx_queue_.push_back(std::move(frame));
        if (send_state_ == send_idle)
        {
            initiate_send();
        }
    }

    void initiate_send()
    {
        assert(send_state_ == send_idle);
        assert(!tx_queue_.empty());
        send_state_ = send_sending;

        // write the data at the front of the queue
        ws.async_write(boost::asio::buffer(tx_queue_.front()),
                       beast::bind_front_handler(
                           &Exchange::on_write,
                           this));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        // whether there was an error or not, set the state to idle
        // and remove the previous frame from the send queue
        send_state_ = send_idle;
        assert(!tx_queue_.empty());
        tx_queue_.pop_front();

        // error check
        if (ec) {
            return fail(name, ec, "write");
        }
        succeed(name, "write");

        // guard condition: if there is more to send, re-enter sending state,
        // otherwise allow to go idle
        if (!tx_queue_.empty())
            initiate_send();
    }

};

int main(int argc, char const* argv[]) {
    net::io_context ioc;

    ssl::context ctx{ssl::context::tlsv12_client};

    Exchange fmex(ioc, ctx);

    fmex.connect_websocket_server();

    ioc.run();

    return 0;
}
