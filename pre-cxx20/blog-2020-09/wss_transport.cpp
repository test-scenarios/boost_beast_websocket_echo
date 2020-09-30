#include "wss_transport.hpp"

namespace project
{
    wss_transport::wss_transport(net::io_context::executor_type exec,
                                 ssl::context &                 ssl_ctx)
    : exec_(exec)
    , websock_(get_executor(), ssl_ctx)
    {
    }

    void
    wss_transport::start()
    {
        net::dispatch(get_executor(), [this] {
            switch (state_)
            {
            case not_started:
                on_start();
                break;
            case connecting:
            case connected:
            case closing:
            case finished:
                BOOST_ASSERT(false);
                break;
            }
        });
    }
    void
    wss_transport::stop()
    {
        // to be continued
    }

    void
    wss_transport::initiate_connect(std::string host,
                                    std::string port,
                                    std::string target)
    {
    }
    void
    wss_transport::send_text_frame(std::string frame)
    {
    }
    void
    wss_transport::initiate_close()
    {
    }
    void
    wss_transport::on_start()
    {
    }
    void
    wss_transport::on_transport_up()
    {
    }
    void
    wss_transport::on_transport_error(std::exception_ptr ep)
    {
    }

    void
    wss_transport::on_text_frame(std::string_view frame)
    {
    }

    void
    wss_transport::on_close()
    {
    }

    auto
    wss_transport::get_executor() const -> const executor_type &
    {
        return exec_;
    }
}   // namespace project