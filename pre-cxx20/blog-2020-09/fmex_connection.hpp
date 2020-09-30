#include "wss_transport.hpp"

namespace project
{

    struct fmex_connection : wss_transport
    {
        using wss_transport::wss_transport;

        void on_start() override;

      private:
        void
        on_transport_up() override;
        void
        on_transport_error(std::exception_ptr ep) override;
        void
        on_text_frame(std::string_view frame) override;
        void
        on_close() override;

        // ping state
        // fmex requires the client to send a json ping every so often to keep
        // the connection alive. We will represent this concept as a state

        enum ping_state
        {
            ping_not_started,
            ping_wait,
            ping_sending,
            ping_pending_pong,
            ping_finished
        };

    };
}