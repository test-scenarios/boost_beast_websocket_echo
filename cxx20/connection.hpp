#pragma once

#include "config.hpp"
#include "states.hpp"

#include <deque>
#include <iostream>
#include <memory>
#include <queue>

namespace project
{
    struct connection_traits
    {
        using base_executor_type = net::io_context::executor_type;
        using coro_executor_type = net::use_awaitable_t<base_executor_type>::executor_with_default<base_executor_type>;
        using socket_type = net::basic_stream_socket<net::ip::tcp, coro_executor_type>;

    };

    struct connection_impl
    : chat_state< net::ip::tcp::socket>
    , std::enable_shared_from_this< connection_impl >
    {
        connection_impl(net::ip::tcp::socket sock);

        //
        // external events
        //

        /// Run the chat implementation until completion
        void
        run();

        /// Gracefully stop the chat implementation
        void
        stop();

        /// Queue a message to be sent at the earliest opportunity
        void
        send(std::string msg);

      private:
        /// Construct a completion handler for any coroutine running in this
        /// implementation
        ///
        /// Functions:
        /// - maintain a live shared_ptr to this implementation in order to
        /// sustain its lifetime
        /// - catch and log any exceptions that occur during the execution of
        /// the coroutine \param context a refernce to _static string data_
        /// whose lifetime must outlive the lifetime of the function object
        /// returned by this function. \return a function object to be used as
        /// the 3rd argument to net::co_spawn
        auto
        spawn_handler(std::string_view context)
        {
            return [self = shared_from_this(), context](std::exception_ptr ep) {
                try
                {
                    if (ep)
                        std::rethrow_exception(ep);
                    std::cout << context << ": done\n";
                }
                catch (system_error &se)
                {
                    if (se.code() != net::error::operation_aborted &&
                        se.code() != websocket::error::closed)
                    {
                        std::cout << context << ": error in " << context
                                  << " : " << se.what() << std::endl;
                    }
                    else
                    {
                        std::cout << context << ": graceful shutdown\n";
                    }
                }
                catch (std::exception &e)
                {
                    std::cout << "error in " << context << " : " << e.what()
                              << std::endl;
                }
            };
        }
    };
}   // namespace project