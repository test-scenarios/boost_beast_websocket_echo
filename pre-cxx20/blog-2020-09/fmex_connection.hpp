#include "json.hpp"
#include "wss_transport.hpp"

namespace project
{
    struct fmex_connection : wss_transport
    {
        fmex_connection(net::io_context::executor_type exec,
                        ssl::context &                 ssl_ctx);

        void
        on_start() override;

      private:
        void
        on_transport_up() override;

        void
        on_transport_error(std::exception_ptr ep) override;

        void
        on_text_frame(std::string_view frame) override;

        void
        on_close() override;

        // fmex protocol management

        void
        on_hello();

        void
        request_ticker(std::string_view ticker);

        // ping state
        // fmex requires the client to send a json ping every so often to keep
        // the connection alive. We will represent this concept as a state

        enum ping_state
        {
            ping_not_started,
            ping_wait,
            ping_sending,
            ping_waiting_pong,
            ping_finished
        } ping_state_ = ping_not_started;

        net::high_resolution_timer ping_timer_;

        void
        ping_enter_state();

        void
        ping_enter_wait();

        void
        ping_event_timeout();

        void
        ping_event_pong(json::value const &frame);
    };
}   // namespace project