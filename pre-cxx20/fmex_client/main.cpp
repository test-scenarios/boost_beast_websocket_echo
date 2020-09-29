#include "config.hpp"
#include "fmex_connection.hpp"

namespace project
{
    struct app
    {
        using executor_type = net::strand< net::io_context::executor_type >;

        app(net::io_context::executor_type const &underlying,
            ssl::context &                        ssl_ctx)
        : exec_(underlying)
        , ssl_context_(ssl_ctx)
        , signals_(exec_)
        {
        }

        void
        start()
        {
            net::dispatch(exec_, [this] {
                signals_.add(SIGINT);
                signals_.async_wait([this](error_code const &ec, int sig) {
                    on_signal(ec, sig);
                });
                start_connections();
            });
        }

        void
        stop()
        {
            net::dispatch(exec_, [this] {
                signals_.cancel();
                stop_connections();
            });
        }

      private:
        void
        on_signal(error_code const &ec, int sig)
        {
            if (ec)
            {
                if (ec == net::error::operation_aborted)
                    return;

                fmt::print(stderr, "fatal error in signal handling: {}\n", ec);
                std::exit(100);
            }

            if (sig == SIGINT)
            {
                fmt::print(stdout, "ctrl-c detected\n");
                stop_connections();
            }
        }

        void
        start_connections()
        {
            connections_.push_back(std::make_unique< ExchangeConnection >(
                exec_.get_inner_executor(), ssl_context_));
            connections_.back()->start();
        };
        void
        stop_connections()
        {
            for (auto &&con : connections_)
                con->stop();
        }

        executor_type                                    exec_;
        ssl::context &                                   ssl_context_;
        boost::asio::signal_set                          signals_;
        std::vector< std::unique_ptr< ConnectionBase > > connections_;
    };
}   // namespace project

int
main(int argc, char const *argv[])
{
    using namespace project;

    net::io_context ioc;

    ssl::context ctx { ssl::context::tlsv12_client };

    auto myapp = app(ioc.get_executor(), ctx);
    myapp.start();

    ioc.run();

    return 0;
}
