#pragma once
#include "config.hpp"

#include <boost/beast/core/tcp_stream.hpp>

namespace project
{
    struct connect_transport_op : boost::asio::coroutine
    {
        using layer_0 = beast::tcp_stream;
        using layer_1 = beast::ssl_stream< layer_0 >;
        using websock = websocket::stream< layer_1 >;

        using executor_type = websock::executor_type;

        using protocol = net::ip::tcp;

        using resolver = net::ip::basic_resolver< protocol, executor_type >;
        using resolver_results = typename resolver::results_type;

        struct state_data
        {
            state_data(websock &      ws,
                       stop_register &sr,
                       std::string && host,
                       std::string && port,
                       std::string && path)
            : exec_(ws.get_executor())
            , stop_register_(sr)
            , resolver_(exec_)
            , websock_(ws)
            , host_(std::move(host))
            , port_(std::move(port))
            , path_(std::move(path))
            {
            }

            executor_type exec_;

            stop_register &stop_register_;
            stop_token     stop_token_;

            resolver         resolver_;
            resolver_results resolve_results_;

            websock &websock_;

            std::string host_;
            std::string port_;
            std::string path_;
        };
        std::unique_ptr< state_data > state_;

        connect_transport_op(websock &   ws,
                             stop_register &sr,
                             std::string host,
                             std::string port,
                             std::string path)
        : state_(std::make_unique< state_data >(ws,
                                                sr,
                                                std::move(host),
                                                std::move(port),
                                                std::move(path)))
        {
        }

        auto
        get_executor() const -> executor_type
        {
            return state_->exec_;
        }

        template < class Self >
        void
        operator()(Self &self, error_code const &ec, resolver_results results)
        {
            state_->resolve_results_ = std::move(results);
            (*this)(self, ec);
        }

        template < class Self >
        void
        operator()(Self &            self,
                   error_code const &ec,
                   layer_0::endpoint_type const &)
        {
            (*this)(self, ec);
        }

        template < class Self >
        void operator()(Self &self, error_code const &ec = {}, std::size_t = 0)
        {
            if (ec)
                return self.complete(ec);

            auto &state = *state_;

            BOOST_ASIO_CORO_REENTER(*this)
            {
                state.stop_token_ = state.stop_register_.add([&state]{
                                                           state.resolver_.cancel();
                                                        });
                BOOST_ASIO_CORO_YIELD
                state.resolver_.async_resolve(
                    state.host_, state.port_, std::move(self));

                state.stop_token_ = state.stop_register_.add([&state]{
                    state.websock_.next_layer().next_layer().cancel();
                });

                BOOST_ASIO_CORO_YIELD
                state.websock_.next_layer().next_layer().async_connect(
                    state.resolve_results_, std::move(self));

                if (!SSL_set_tlsext_host_name(
                        state.websock_.next_layer().native_handle(),
                        state.host_.c_str()))
                    return self.complete(
                        error_code(static_cast< int >(::ERR_get_error()),
                                   net::error::get_ssl_category()));

                BOOST_ASIO_CORO_YIELD
                state.websock_.next_layer().async_handshake(layer_1::client,
                                                            std::move(self));

                state.websock_.set_option(
                    websocket::stream_base::timeout::suggested(
                        beast::role_type::client));

                state.websock_.set_option(websocket::stream_base::decorator(
                    [](websocket::request_type &req) {
                        req.set(http::field::user_agent,
                                "custom-websocket-client-async");
                    }));

                BOOST_ASIO_CORO_YIELD
                state.websock_.async_handshake(
                    state.host_, state.path_, std::move(self));

                state.stop_token_.reset();

                return self.complete(ec);
            }
        }
    };

}   // namespace project