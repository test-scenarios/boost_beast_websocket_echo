#pragma once

#define FMT_HEADER_ONLY

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <nlohmann/json.hpp>

namespace project
{
    namespace beast = boost::beast;

    namespace http = beast::http;

    namespace websocket = beast::websocket;

    namespace net = boost::asio;

    namespace ssl = boost::asio::ssl;

    using tcp = boost::asio::ip::tcp;

    using error_code = beast::error_code;

    using system_error = beast::system_error;

    using json = nlohmann::json;

}   // namespace project