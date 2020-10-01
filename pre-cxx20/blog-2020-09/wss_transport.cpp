#include "wss_transport.hpp"
#include <fmt/printf.h>

namespace project
{
    using namespace std::literals;

    wss_transport::wss_transport(net::io_context::executor_type exec,
                                 ssl::context &                 ssl_ctx)
    : exec_(exec)
    , websock_(get_executor(), ssl_ctx)
    {
    }

    void
    wss_transport::start()
    {
        net::dispatch(get_executor(), [this] {
            switch (state_)
            {
            case not_started:
                on_start();
                break;
            case connecting:
            case connected:
            case closing:
            case finished:
                BOOST_ASSERT(false);
                break;
            }
        });
    }
    void
    wss_transport::stop()
    {
        // to be continued
    }

    struct wss_transport::connect_op : asio::coroutine
    {
        using executor_type = wss_transport::executor_type;
        using websock       = wss_transport::websock;

        struct impl_data
        {
            impl_data(websock &   ws,
                      std::string host,
                      std::string port,
                      std::string target)
            : ws(ws)
            , resolver(ws.get_executor())
            , host(host)
            , port(port)
            , target(target)
            {
            }

            layer_0 &
            tcp_layer() const
            {
                return ws.next_layer().next_layer();
            }

            layer_1 &
            ssl_layer() const
            {
                return ws.next_layer();
            }

            websock &                            ws;
            net::ip::tcp::resolver               resolver;
            net::ip::tcp::resolver::results_type endpoints;
            std::string                          host, port, target;
        };

        connect_op(websock &   ws,
                   std::string host,
                   std::string port,
                   std::string target)
        : impl_(std::make_unique< impl_data >(ws, host, port, target))
        {
        }

        template < class Self >
        void
        operator()(Self &                               self,
                   error_code                           ec,
                   net::ip::tcp::resolver::results_type results)
        {
            impl_->endpoints = results;
            (*this)(self, ec);
        }

        template < class Self >
        void
        operator()(Self &self, error_code ec, net::ip::tcp::endpoint const &)
        {
            (*this)(self, ec);
        }

        template < class Self >
        void operator()(Self &self, error_code ec = {}, std::size_t = 0)
        {
            if (ec)
                self.complete(ec);

            auto &impl = *impl_;

#include <boost/asio/yield.hpp>
            reenter(*this)
            {
                yield impl.resolver.async_resolve(
                    impl.host, impl.port, std::move(self));

                impl.tcp_layer().expires_after(15s);
                yield impl.tcp_layer().async_connect(impl.endpoints,
                                                     std::move(self));

                if (!SSL_set_tlsext_host_name(impl.ssl_layer().native_handle(),
                                              impl.host.c_str()))
                    return self.complete(
                        error_code(static_cast< int >(::ERR_get_error()),
                                   net::error::get_ssl_category()));

                impl.tcp_layer().expires_after(15s);
                yield impl.ssl_layer().async_handshake(ssl::stream_base::client,
                                                       std::move(self));

                impl.tcp_layer().expires_after(15s);
                yield impl.ws.async_handshake(
                    impl.host, impl.target, std::move(self));

                impl.tcp_layer().expires_never();
                yield self.complete(ec);
            }
#include <boost/asio/unyield.hpp>
        }

        std::unique_ptr< impl_data > impl_;
    };

    void
    wss_transport::initiate_connect(std::string host,
                                    std::string port,
                                    std::string target)
    {
        switch (state_)
        {
        case state_type::not_started:
            break;

        case connecting:
        case connected:
        case closing:
        case finished:
            return;
        }

        state_ = connecting;

        auto handler = [this](error_code const &ec) {
            if (ec)
                event_transport_error(ec);
            else
                event_transport_up();
        };

        net::async_compose< decltype(handler), void(error_code) >(
            connect_op(
                websock_, std::move(host), std::move(port), std::move(target)),
            handler,
            get_executor());
    }

    void
    wss_transport::send_text_frame(std::string frame)
    {
        if (state_ != connected)
            return;

        send_queue_.push_back(std::move(frame));
        start_sending();
    }

    void
    wss_transport::initiate_close()
    {
    }

    void
    wss_transport::on_start()
    {
    }

    void
    wss_transport::on_transport_up()
    {
    }

    void
    wss_transport::on_transport_error(std::exception_ptr ep)
    {
    }

    void
    wss_transport::on_text_frame(std::string_view frame)
    {
    }

    void
    wss_transport::on_close()
    {
    }

    auto
    wss_transport::get_executor() const -> const executor_type &
    {
        return exec_;
    }

    void
    wss_transport::event_transport_up()
    {
        state_ = connected;
        on_transport_up();
        start_sending();
        start_reading();
    }

    void
    wss_transport::event_transport_error(const error_code &ec)
    {
        event_transport_error(std::make_exception_ptr(system_error(ec)));
    }

    void
    wss_transport::event_transport_error(std::exception_ptr ep)
    {
        switch (state_)
        {
        case connecting:
        case connected:
        case closing:
            state_ = state_type::finished;
            on_transport_error(std::move(ep));
            break;

        default:
            break;
        }
    }

    void
    wss_transport::start_sending()
    {
        if (state_ == connected && send_state_ == not_sending &&
            !send_queue_.empty())
        {
            send_state_ = sending;
            websock_.async_write(net::buffer(send_queue_.front()),
                                 [this](error_code const &ec, std::size_t bt) {
                                     handle_send(ec, bt);
                                 });
        }
    }

    void
    wss_transport::handle_send(const error_code &ec, std::size_t)
    {
        send_state_ = not_sending;

        send_queue_.pop_front();

        if (ec)
            event_transport_error(ec);
        else
            start_sending();
    }

    void
    wss_transport::start_reading()
    {
        if (state_ != connected)
            return;

        websock_.async_read(rx_buffer_,
                            [this](error_code const &ec, std::size_t bt) {
                                handle_read(ec, bt);
                            });
    }

    void
    wss_transport::handle_read(error_code const &ec,
                               std::size_t       bytes_transferred)
    {
        if (ec)
            event_transport_error(ec);
        else
            try
            {
                auto bytes = rx_buffer_.data();
                if (websock_.text())
                {
                    auto frame = std::string_view(
                        reinterpret_cast< const char * >(bytes.data()),
                        bytes_transferred);
                    on_text_frame(frame);
                }
                else
                {
                    // @todo: add support for binary frames
                }
                rx_buffer_.consume(bytes_transferred);
                start_reading();
            }
            catch (...)
            {
                event_transport_error(std::current_exception());
            }
    }

}   // namespace project