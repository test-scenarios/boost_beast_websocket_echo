#include "connection_base.hpp"

#include "connect_transport_op.hpp"

#define FMT_HEADER_ONLY
#include <fmt/color.h>
#include <fmt/format.h>

namespace project
{
    void
    ConnectionBase::notify_connect(std::string host,
                                   std::string port,
                                   std::string target)
    {
        auto op = connect_transport_op(ws,
                                       stop_register_,
                                       std::move(host),
                                       std::move(port),
                                       std::move(target));

        auto handler = [this](error_code const &ec) {
            if (ec)
                this->fail(name, ec, "transport");
            else
            {
                this->on_transport_up();
                my_stop_token_ = stop_register_.add([this]{
                                                  initiate_close();
                                               });
                this->enter_read_state();
            }
        };

        net::async_compose< decltype(handler), void(error_code) >(
            std::move(op), handler, *this);
    }

    void
    ConnectionBase::fail(const std::string &exchange,
                         beast::error_code  ec,
                         const char *       what)
    {
        fmt::print(stderr,
                   fg(fmt::color::crimson),
                   "{}: {}: {}\n",
                   exchange,
                   what,
                   ec.message());
        on_error(ec);
    }
    void
    ConnectionBase::fail(const std::string &exchange, const char *what)
    {
        fmt::print(stderr, fg(fmt::color::crimson), "{}: {}\n", exchange, what);
        on_error(net::error::fault);
    }
    void
    ConnectionBase::succeed(const std::string &exchange, const char *what)
    {
        fmt::print(fg(fmt::color::steel_blue), "{}: {}\n", exchange, what);
    }
    void
    ConnectionBase::enter_read_state()
    {
        ws.async_read(
            buffer, beast::bind_front_handler(&ConnectionBase::on_read, this));
    }
    void
    ConnectionBase::on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
        {
            return fail(name, ec, "read");
        }
        succeed(name, "read");

        int64_t now = std::chrono::duration_cast< std::chrono::milliseconds >(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
        fmt::print("received: {}\n", beast::buffers_to_string(buffer.data()));

        try
        {
            on_text_frame([&] {
                auto d = buffer.data();
                return std::string_view(
                    reinterpret_cast< const char * >(d.data()), d.size());
            }());
            buffer.consume(buffer.size());
            enter_read_state();
        }
        catch (system_error &se)
        {
            fail(name, se.code(), "on_text_frame");
            on_error(se.code());
            return;
        }
        catch (std::exception &e)
        {
            using namespace std::literals;
            fail(name, ("on_text_frame: "s + e.what()).c_str());
            on_error(net::error::basic_errors::fault);
            return;
        }
    }

    void ConnectionBase::initiate_close()
    {
        ws.async_close(websocket::close_code::going_away, [this](error_code const& ec){
            on_close(ec);
        });
    }

    void
    ConnectionBase::on_close(beast::error_code ec)
    {
        if (ec)
        {
            return fail(name, ec, "close");
        }
        succeed(name, "close");
    }

    void
    ConnectionBase::notify_send(std::string frame)
    {
        tx_queue_.push_back(std::move(frame));
        if (send_state_ == send_idle)
        {
            initiate_send();
        }
    }
    void
    ConnectionBase::initiate_send()
    {
        assert(send_state_ == send_idle);
        assert(!tx_queue_.empty());
        send_state_ = send_sending;

        // write the data at the front of the queue
        ws.async_write(
            boost::asio::buffer(tx_queue_.front()),
            beast::bind_front_handler(&ConnectionBase::on_write, this));
    }
    void
    ConnectionBase::on_write(beast::error_code ec,
                             std::size_t       bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // whether there was an error or not, set the state to idle
        // and remove the previous frame from the send queue
        send_state_ = send_idle;
        assert(!tx_queue_.empty());
        tx_queue_.pop_front();

        // error check
        if (ec)
        {
            return fail(name, ec, "write");
        }
        succeed(name, "write");

        // guard condition: if there is more to send, re-enter sending
        // state, otherwise allow to go idle
        if (!tx_queue_.empty())
            initiate_send();
    }

    void
    ConnectionBase::start()
    {
        // in a multithreaded environment, be sure we are on the correct thread
        net::dispatch(exec_, [this] { handle_connect_command(); });
    }
    void
    ConnectionBase::stop()
    {
        // in a multithreaded environment, be sure we are on the correct thread
        net::dispatch(exec_, [this] { stop_register_.notify_all(); });
    }

    ConnectionBase::ConnectionBase(
        const boost::asio::io_context::executor_type &exec,
        boost::asio::ssl::context &                   ctx,
        std::string                                   name)
    : exec_(net::make_strand(exec))
    , ws(exec_, ctx)
    , name(std::move(name))
    {
    }

}   // namespace project
