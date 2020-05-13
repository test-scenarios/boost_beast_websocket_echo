#include "config.hpp"

#include <boost/algorithm/string.hpp>
#include <memory>

namespace project
{
    struct console;

    enum class console_event
    {
        quit
    };

    struct run_op_base
    {
        virtual void cancel() = 0;
    };

    template < class Executor, class Handler >
    struct run_op
    : asio::coroutine
    , run_op_base
    , std::enable_shared_from_this< run_op< Executor, Handler > >
    {
        // net composed operation interface components
        using executor_type = Executor;
        executor_type get_executor() const { return exec_; }

        auto myself()
        {
            return net::bind_executor(get_executor(), [self = this->shared_from_this()](auto &&... args) {
                self->operator()(std::forward< decltype(args) >(args)...);
            });
        }
        run_op(console *pcon, Executor exec, Handler h);

        void operator()(error_code ec = {}, std::size_t bytes_transferred = 0, bool cont = true);
        void cancel() override;

      private:
        console *                            pconsole_;
        Executor                             exec_;
        net::executor_work_guard< Executor > wg_;
        Handler                              handler_;
        std::string                          rxstore_;
        error_code                           ec_;
        console_event                        last_event_ = console_event::quit;
    };

    struct console
    {
        console(net::executor exec)
        : fdin_(exec, ::dup(STDIN_FILENO))
        {
        }

        using executor_type = net::executor;
        executor_type get_executor() { return fdin_.get_executor(); }

        template < class CompletionToken >
        auto async_run(CompletionToken &&token);

        void cancel()
        {
            auto op = std::atomic_load(&run_op_);
            if (op)
                op->cancel();
        }

      private:
        asio::posix::stream_descriptor fdin_;
        std::shared_ptr< run_op_base > run_op_;

        template < class Executor, class Handler >
        friend struct run_op;
    };
}   // namespace project

#include <iostream>
#include <regex>

namespace project
{
    template < class CompletionToken >
    auto console::async_run(CompletionToken &&token)
    {
        return asio::async_initiate< CompletionToken, void(error_code, console_event) >(
            [&](auto &&handler) {
                assert(!this->run_op_);
                using handler_type = std::decay_t< decltype(handler) >;
                auto exec          = net::get_associated_executor(handler, this->get_executor());
                using exec_type    = decltype(exec);
                auto p        = std::make_shared< run_op< exec_type, handler_type > >(this, exec, std::move(handler));
                this->run_op_ = p;
                (*p)(error_code(), 0, false);
            },
            token);
    }

    template < class Executor, class Handler >
    run_op< Executor, Handler >::run_op(console *pcon, Executor exec, Handler h)
    : pconsole_(pcon)
    , exec_(exec)
    , wg_(exec)
    , handler_(std::move(h))
    {
    }

    template < class Executor, class Handler >
    void run_op< Executor, Handler >::cancel()
    {
        net::dispatch(net::bind_executor(this->get_executor(), [self = this->shared_from_this()]() {
            self->ec_ = net::error::operation_aborted;
            self->pconsole_->fdin_.cancel();
        }));
    }

#include <boost/asio/yield.hpp>
    template < class Executor, class Handler >
    void run_op< Executor, Handler >::operator()(error_code ec, std::size_t bytes_transferred, bool cont)
    {
        if (ec && !ec_)
            ec_ = ec;
        static std::regex reg_quit("^(quit|exit|done)$", std::regex_constants::icase);
        reenter(this)
        {
            while (!ec_)
            {
                yield net::async_read_until(pconsole_->fdin_, net::dynamic_buffer(this->rxstore_), "\n", myself());
                {
                    auto line = rxstore_.substr(0, bytes_transferred);
                    rxstore_.erase(0, bytes_transferred);
                    boost::trim(line);
                    if (std::regex_match(line, reg_quit))
                    {
                        last_event_ = console_event::quit;
                        goto done;
                    }
                    std::cout << "unrecognised command\n";
                }
            }
        done:
            if (!cont)
            {
                yield
                {
                    post(bind_executor(get_executor(), [self = this->shared_from_this()] { (*self); }));
                }
            }
            auto h = std::move(handler_);
            std::atomic_exchange(&pconsole_->run_op_, std::shared_ptr< run_op_base >());
            h(ec_, last_event_);
        }
    }
#include <boost/asio/unyield.hpp>

}   // namespace project
