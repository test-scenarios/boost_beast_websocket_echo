#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <fmt/ostream.h>
#include <regex>
#include <spdlog/spdlog.h>

namespace program {
namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;
namespace net   = asio;

using boost::system::error_code;
using boost::system::system_error;
}   // namespace program