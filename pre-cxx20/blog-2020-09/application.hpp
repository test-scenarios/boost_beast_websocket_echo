#include "net.hpp"
#include "sigint_state.hpp"
#include "ssl.hpp"

#include <fmt/printf.h>

namespace project
{
    struct application
    {
        using executor_type = net::io_context::executor_type;

        application(net::io_context::executor_type const &exec,
                    ssl::context &                        ssl_ctx);

        void
        start();

      private:
        auto
        get_executor() const -> executor_type const &;

        executor_type exec_;
        ssl::context &ssl_ctx_;
        sigint_state  sigint_state_;
    };
}   // namespace project