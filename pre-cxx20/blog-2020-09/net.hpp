#pragma once

#include <boost/asio.hpp>

namespace project
{
    namespace asio = boost::asio;
    namespace net
    {
        using namespace asio;

        // 1.74 and 1.73 compatibility
#if BOOST_ASIO_VERSION <= 101601
        using any_io_executor = executor;
#endif
    }   // namespace net

    using error_code   = boost::system::error_code;
    using system_error = boost::system::system_error;

}   // namespace project