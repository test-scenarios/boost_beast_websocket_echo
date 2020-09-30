#include "application.hpp"

namespace project
{
    application::application(const net::io_context::executor_type &exec,
                             ssl::context &                        ssl_ctx)
    : exec_(exec)
    , ssl_ctx_(ssl_ctx)
    , sigint_state_(get_executor())
    {
    }

    void
    application::start()
    {
        net::dispatch(get_executor(), [this] {
            fmt::print(stdout, "Application starting\n");
            sigint_state_.start();
        });
    }

    auto
    application::get_executor() const -> const application::executor_type &
    {
        return exec_;
    }
}   // namespace project