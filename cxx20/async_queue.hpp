#pragma once
#include "config.hpp"

#include <memory>
#include <unordered_map>

namespace project
{

    template < class T >
    struct async_queue
    {
        using value_type = T;

        struct impl;
        struct connection;

        async_queue(net::executor exec);
        async_queue(async_queue &&other);
        async_queue &
        operator=(async_queue &&other);
        ~async_queue();

        connection
        connect(net::executor exec);

        void push(T arg);
        void cancel();

      private:
        std::shared_ptr< impl > impl_;
    };

    template < class T >
    struct async_queue< T >::connection
    {
        struct impl;

        connection(net::executor exec);
        connection(connection &&other);
        connection &
        operator=(connection &&other);
        ~connection();

        template < class WaitHandler >
        auto
        async_wait(WaitHandler &&token) ->
            typename net::async_result< std::decay_t< WaitHandler >,
                                        void(error_code, T) >::result_type;

      private:
        std::shared_ptr< impl > impl_;
    };

    template < class T >
    struct async_queue< T >::connection::impl
    {
        net::executor                             exec_;
        std::queue< T >                           items_;
        poly_handler< void(error_code, T) >       handler_ = {};
        std::atomic< bool >                       waiting_ = false;
        std::shared_ptr< async_queue< T >::impl > impl_;
    };

    template < class T >
    struct async_queue< T >::impl : std::enable_shared_from_this< impl >
    {
        using connection_impl = typename async_queue< T >::connection::impl;

        std::unordered_map< connection_impl *,
                            std::weak_ptr< connection_impl > >
            connections_;
    };

    template < class T >
    auto
    async_queue< T >::connect(net::executor exec) -> connection
    {
        return connection(impl_->make_connection(exec));
    }

}   // namespace project