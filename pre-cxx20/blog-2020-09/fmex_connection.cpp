#include "fmex_connection.hpp"

#include <fmt/printf.h>

namespace project
{
    void
    fmex_connection::on_start()
    {
        fmt::print(stdout, "fmex: initiating connection\n");
        initiate_connect("api.fmex.com", "443", "/v2/ws");
    }
    void
    fmex_connection::on_transport_up()
    {
        fmt::print(stdout, "fmex: transport up\n");
    }

    void
    fmex_connection::on_transport_error(std::exception_ptr ep)
    {
        try
        {
            std::rethrow_exception(ep);
        }
        catch (system_error &se)
        {
            fmt::print(stderr,
                       "fmex: transport error : {} : {} : {}\n",
                       se.code().category().name(),
                       se.code().value(),
                       se.code().message());
        }
        catch (std::exception &e)
        {
            fmt::print(stderr, "fmex: transport exception : {}\n", e.what());
        }
    }

    void
    fmex_connection::on_text_frame(std::string_view frame)
    {
        fmt::print(stdout, "fmex: text frame : {}\n", frame);
    }

    void
    fmex_connection::on_close()
    {
        fmt::print(stdout, "fmex: closed\n");
    }
}   // namespace project