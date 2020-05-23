#pragma once
#include "detail/async_queue_impl.hpp"
#include "net.hpp"

namespace beast_fun_times::util
{
    template < class T, class Executor >
    struct basic_async_queue
    {
        using executor_type = Executor;

        using value_type = T;

        basic_async_queue(net::executor exec);
        basic_async_queue(basic_async_queue &&other);
        basic_async_queue &
        operator=(basic_async_queue &&other);
        ~basic_async_queue();

        /// Initiate an asynchronous wait on the queue.
        ///
        /// The function will return immediately. The WaitHandler will be
        /// invoked, as if by post on its associated executor when an in the
        /// queue is ready for delivery \tparam WaitHandler A completion token
        /// or handler whose signature matches void(error_code, T) \param
        /// handler A completion token or handler whose signature matches
        /// void(error_code, T) \return DEDUCED
        template < BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, value_type))
                       WaitHandler >
        BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WaitHandler,
                                           void(error_code, value_type))
        async_pop(WaitHandler &&handler
                      BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

        /// Push an item onto the queue.
        /// This function will return quickly, and delivery of the payload is
        /// not guaranteed to have heppened before the function returns. \param
        /// arg the value to push onto the queue.
        void
        push(T arg);

        /// Put the queue into an error state, clear data from the queue and cause
        /// all subsequent async_wait operations to fail
        void
        stop();

      private:
        using impl_class          = detail::async_queue_impl< T, Executor >;
        using implementation_type = typename impl_class::ptr;

      private:
        implementation_type impl_;
    };

    template < class T >
    using async_queue = basic_async_queue< T, net::executor >;

}   // namespace beast_fun_times::util


namespace beast_fun_times::util
{

    template<class T, class Executor>
    basic_async_queue<T, Executor>::basic_async_queue(net::executor exec)
    : impl_(impl_class::construct(exec))
    {
    }

    template<class T, class Executor>
    basic_async_queue<T, Executor>::basic_async_queue(basic_async_queue &&other)
        : impl_(std::exchange(other.impl_, nullptr))
    {}

    template<class T, class Executor>
    auto
    basic_async_queue<T, Executor>::operator=(basic_async_queue &&other)-> basic_async_queue &
    {
        auto tmp = std::move(other);
        std::swap(impl_, tmp.impl_);
        return *this;
    }

    template<class T, class Executor>
    basic_async_queue<T, Executor>::~basic_async_queue()
    {
        if (impl_)
            impl_->stop();
    }


    template<class T, class Executor>
    template < BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code, value_type))
    WaitHandler >
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WaitHandler,
                                       void(error_code, value_type))
    basic_async_queue<T, Executor>::async_pop(WaitHandler &&handler)
    {
        return impl_->async_pop(std::forward<WaitHandler>(handler));
    }

    template<class T, class Executor>
    void basic_async_queue<T, Executor>::push(value_type v)
    {
        return impl_->push(std::move(v));
    }

    template<class T, class Executor>
    void basic_async_queue<T, Executor>::stop()
    {
        return impl_->stop();
    }

}