#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace project {
namespace net       = boost::asio;
namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
using error_code    = beast::error_code;

}   // namespace project