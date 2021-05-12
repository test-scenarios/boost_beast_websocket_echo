#pragma once

#include "config.hpp"
#include "connection.hpp"

#include <boost/functional/hash.hpp>
#include <unordered_map>


namespace project
{
    struct endpoint_hasher
    {
        void combine(std::size_t &seed, net::ip::address const &v) const
        {
            if (v.is_v4())
                boost::hash_combine(seed, v.to_v4().to_bytes());
            else if (v.is_v6())
                boost::hash_combine(seed, v.to_v6().to_bytes());
            else if (v.is_unspecified())
                boost::hash_combine(seed, 0x4751301174351161ul);
            else
                boost::hash_combine(seed, v.to_string());
        }

        std::size_t operator()(net::ip::tcp::endpoint const &ep) const
        {
            std::size_t seed = 0;
            boost::hash_combine(seed, ep.port());
            combine(seed, ep.address());
            return seed;
        }
    };

    struct connection_pool
    {
        connection_pool(net::any_io_executor exec);

        void run();

        void stop();

      private:
        void handle_run();

        void handle_stop();

      private:
        void initiate_timer();
        void handle_timer(error_code ec);
        void another_connection();

      private:
        net::system_timer timer_;
        std::unordered_map< net::ip::tcp::endpoint, std::weak_ptr< connection_impl >, endpoint_hasher, std::equal_to<> >
                   connections_;
        error_code ec_;
    };
}   // namespace project