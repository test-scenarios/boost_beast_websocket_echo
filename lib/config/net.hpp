#pragma once

#include <boost/asio.hpp>

namespace beast_fun_times::config
{
    namespace net = boost::asio;
    using error_code = boost::system::error_code;
    using system_error = boost::system::system_error;
}