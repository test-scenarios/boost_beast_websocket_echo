//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//----
//--------------------------------------------------------------------------------------------------------
//
// Example: WebSocket client, synchronous
//
//----
//--------------------------------------------------------------------------------------------------------

//[example_websocket_client

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace beast     = boost::beast;       // from <boost/beast.hpp>
namespace http      = beast::http;        // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;   // from <boost/beast/websocket.hpp>
namespace net       = boost::asio;        // from <boost/asio.hpp>
using tcp           = boost::asio::ip::tcp;   // from <boost/asio/ip/tcp.hpp>

// Sends a WebSocket message and prints the response
int
main(int argc, char **argv)
{
    std::atomic_bool finished { false };

    for (int i = 0; i < 30000; i++)
    {
        if ((i % 1000) == 0)
            printf("Thread spawn %i\n", i);

        std::thread([&] {
            try
            {
                std::string host = "192.168.0.55";
                std::string port = "6761";
                std::string text =
                    "a8aunpw39rvu0a9p8weufvpoaidsjfkav8w39unfvapwo9ep8UPAOISUND"
                    "PVIOUPN(*&"
                    "Np98unp938upoiuoivasdfliJNASDOIVp028upn9v8upsoinjun:"
                    "OIASJDVN";

                // The io_context is required for all I/O
                net::io_context ioc;

                // These objects perform our I/O
                tcp::resolver                    resolver { ioc };
                websocket::stream< tcp::socket > ws { ioc };

                boost::beast::websocket::permessage_deflate opt;
                opt.client_enable = true;

                ws.set_option(opt);

                // Look up the domain name
                auto const results = resolver.resolve(host, port);

                // Make the connection on the IP address we get from a lookup
                auto ep = net::connect(ws.next_layer(), results);

                // Update the host_ string. This will provide the value of the
                // Host HTTP header during the WebSocket handshake.
                // See https://tools.ietf.org/html/rfc7230#section-5.4
                host += ':' + std::to_string(ep.port());

                // Set a decorator to change the User-Agent of the handshake
                ws.set_option(websocket::stream_base::decorator(
                    [](websocket::request_type &req) {
                        req.set(http::field::user_agent,
                                std::string(BOOST_BEAST_VERSION_STRING) +
                                    " websocket-client-coro");
                    }));

                // Perform the websocket handshake
                ws.handshake(host, "/");

                // Send the message
                ws.write(net::buffer(std::string(text)));

                /// only leaks if this is defined
#define LARGE_SIMULTANEOUS_CONNECTIONS
#ifdef LARGE_SIMULTANEOUS_CONNECTIONS
                while (!finished)
                {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(1024));
                }
#endif   // LARGE_SIMULTANEOUS_CONNECTIONS

                /// this does nothing to change the nature of the leak
#ifdef GRACEFUL_SHUTDOWN
                // Close the WebSocket connection
                ws.close(websocket::close_code::normal);
#endif   // GRACEFUL_SHUTDOWN

                return EXIT_FAILURE;

                // If we get here then the connection is closed gracefully

                // The make_printable() function helps print a
                // ConstBufferSequence
            }
            catch (std::exception const &e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
                return EXIT_FAILURE;
            }
        }).detach();

        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    printf("Finished\n");
    finished = true;

    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1024));
    }

    return EXIT_SUCCESS;
}

//]