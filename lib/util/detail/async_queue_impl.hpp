#pragma once
#include "util/net.hpp"
#include "util/poly_handler.hpp"

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <deque>
#include <queue>

namespace beast_fun_times::util::detail {
template < class T, class Executor >
struct async_queue_impl
: boost::intrusive_ref_counter< async_queue_impl< T, Executor > >
{
    using value_type    = T;
    using executor_type = Executor;
    using ptr           = boost::intrusive_ptr< async_queue_impl >;

    enum waiting_state
    {
        not_waiting,
        waiting
    };

    async_queue_impl(executor_type exec)
    : state_(not_waiting)
    , handler_()
    , handler_executor_()
    , values_()
    , default_executor_(exec)
    {
    }

    template < BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, value_type))
                   WaitHandler >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WaitHandler,
                                       void(error_code, value_type))
    async_pop(WaitHandler &&handler);

    static ptr
    construct(executor_type exec);

    void
    push(value_type v);

    void
    stop();

  private:
    void
    maybe_complete();

  private:
    std::atomic< waiting_state >                 state_ = not_waiting;
    poly_handler< void(error_code, value_type) > handler_;
    net::any_io_executor                         handler_executor_;

    using queue_impl = std::queue< T, std::deque< T > >;
    queue_impl    values_;
    error_code    ec_;   // error state of the queue
    executor_type default_executor_;
};
}   // namespace beast_fun_times::util::detail

namespace beast_fun_times::util::detail {
template < class T, class Executor >
template < BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, value_type))
               WaitHandler >
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WaitHandler, void(error_code, value_type))
async_queue_impl< T, Executor >::async_pop(WaitHandler &&handler)
{
    assert(this->state_ == not_waiting);

    auto initiate = [this](auto &&deduced_handler) {
        auto hexec = net::get_associated_executor(deduced_handler,
                                                  this->default_executor_);
        this->handler_executor_ =
            net::prefer(hexec, net::execution::outstanding_work.tracked);
        this->handler_ =
            [wg = handler_executor_,
             dh = std::move(deduced_handler)](auto &&...args) mutable -> void {
            dh(std::forward< decltype(args) >(args)...);
        };

        this->state_ = waiting;

        net::dispatch(net::bind_executor(
            this->default_executor_,
            [self = boost::intrusive_ptr(this)]() { self->maybe_complete(); }));
    };

    return net::async_initiate< WaitHandler, void(error_code, value_type) >(
        initiate, handler);
}

template < class T, class Executor >
auto
async_queue_impl< T, Executor >::construct(executor_type exec) -> ptr
{
    return ptr(new async_queue_impl(exec));
}

template < class T, class Executor >
void
async_queue_impl< T, Executor >::push(value_type v)
{
    net::dispatch(net::bind_executor(
        this->default_executor_,
        [self = boost::intrusive_ptr(this), v = std::move(v)]() mutable {
            self->values_.push(std::move(v));
            self->maybe_complete();
        }));
}

template < class T, class Executor >
void
async_queue_impl< T, Executor >::maybe_complete()
{
    // running in default executor...
    if (values_.empty() and not ec_)
        return;
    if (state_.exchange(not_waiting) != waiting)
        return;

    if (ec_)
    {
        net::post(
            net::bind_executor(this->handler_executor_,
                               [h  = std::move(this->handler_),
                                ec = ec_]() mutable { h(ec, value_type()); }));
        ec_.clear();
    }
    else
    {
        net::post(net::bind_executor(this->handler_executor_,
                                     [v = std::move(this->values_.front()),
                                      h = std::move(this->handler_)]() mutable {
                                         h(error_code(), std::move(v));
                                     }));
        values_.pop();
    }
}

template < class T, class Executor >
void
async_queue_impl< T, Executor >::stop()
{
    net::dispatch(net::bind_executor(
        this->default_executor_, [self = boost::intrusive_ptr(this)]() mutable {
            self->ec_ = net::error::operation_aborted;
            while (not self->values_.empty())
                self->values_.pop();
            self->maybe_complete();
        }));
}

}   // namespace beast_fun_times::util::detail