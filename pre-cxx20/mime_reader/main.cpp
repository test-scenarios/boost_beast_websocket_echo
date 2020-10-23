#include "config.hpp"
#include "mime_reader.hpp"

#include <boost/beast/_experimental/test/stream.hpp>
#include <spdlog/spdlog.h>

namespace program {
struct test_host : std::enable_shared_from_this< test_host >
{
    using executor_type = net::io_context::executor_type;
    using stream_type   = beast::test::stream;

    test_host(executor_type e, stream_type s)
    : exec_(e)
    , stream_(std::move(s))
    {
    }

    void
    initiate_read()
    {
        // it's a good habit tp marshall interface initiating functions onto
        // this objects' executor, as we may want to use a strand in future
        auto self = shared_from_this();

        net::dispatch(get_executor(), [self] {
            http::async_read_header(
                self->stream_,
                self->header_buffer_,
                self->parser_,
                net::bind_executor(self->get_executor(),
                                   [self](error_code ec, std::size_t bt) {
                                       self->handle_header(ec, bt);
                                   }));
        });
    }

    auto
    get_executor() const -> executor_type const &
    {
        return exec_;
    }

  private:
    void
    handle_header(error_code ec, std::size_t bytes_transferred)
    {
        assert(exec_.running_in_this_thread());
        auto &hdr = parser_.get().base();
        spdlog::debug(
            "received header of length {}:\n{}", bytes_transferred, hdr);

        auto re = std::regex(
            R"regex(^multipart/x-mixed-replace\s*;\s*boundary="([^"]+)"$)regex",
            std::regex_constants::icase);

        auto content_type = hdr[http::field::content_type];
        auto match        = std::cmatch();
        if (!std::regex_match(
                content_type.begin(), content_type.end(), match, re))
        {
            spdlog::error("failed to match {}", content_type);
            return;
        }

        boundary_ = match[1].str();
        spdlog::debug("matching on boundary: {}", boundary_);

        // consume the bytes used in the header from the header_buffer
        spdlog::debug("remaining bytes: {}\n{}",
                     header_buffer_.size(),
                     beast::buffers_to_string(header_buffer_.data()));

        // we are going to switch from beast's pass-by-reference dynanmic
        // buffers to asio's pass-by value versions to simplify the code in
        // the reader. These v2 dynamic buffers were introduced after beast
        // and we haven't caught up yet

        rx_storage_ = beast::buffers_to_string(header_buffer_.data());

        spdlog::debug("rest of message: {}", rx_storage_);

        read_next_part();
    }

    void
    read_next_part()
    {
        auto self = shared_from_this();

        mime_reader::async_read_mime(
            stream_,
            boundary_,
            [](mime_reader::mime_part part) {
                spdlog::info(
                    "received part length: {}, type: {}, contents:\n{}\n",
                    part.headers[http::field::content_length],
                    part.headers[http::field::content_type],
                    part.body);
            },
            net::dynamic_buffer(rx_storage_),
            net::bind_executor(
                get_executor(), [self](error_code ec) {
                    if (ec)
                        spdlog::error("mime error: {}", ec.message());
                    else
                        spdlog::debug("parts finished");
                }));
    }

    executor_type exec_;
    stream_type   stream_;

    // reading the header
    http::response_parser< http::empty_body > parser_;
    beast::flat_buffer                        header_buffer_;

    // reading the stream
    std::string            boundary_;
    mime_reader::mime_part part_;
    std::string            rx_storage_;
};
}   // namespace program

int
main()
{
    using namespace program;

    spdlog::set_level(spdlog::level::info);

    net::io_context ioc;
    auto exec = ioc.get_executor();

    auto local =
        beast::test::stream(ioc);   // simulate the local socket/tls stream
    auto remote = connect(
        local);   // simulate the remote socket so we can push in test data

    // now simulate our host's read

    auto our_host = std::make_shared< test_host >(exec, std::move(local));
    our_host->initiate_read();

    auto test_data = beast::string_view(
        "HTTP/1.1 200 OK\r\n"
        "Date: Thu, 22 Oct 2020 20:26:01 GMT\r\n"
        "Content-Type: "
        "multipart/x-mixed-replace;boundary=\"FrameSeparator\"\r\n"
        "\r\n"
        "\r\n"
        "--FrameSeparator\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "abc\r\n"
        "--FrameSeparator\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "def\r\n"
        "--FrameSeparator--\r\n");

    // simulate the remote end sending us some data
    auto written =
        remote.write_some(net::buffer(test_data.data(), test_data.size()));
    spdlog::debug("remote writes {} bytes", written);
    remote.close();
    spdlog::debug("shuts down stream", written);

    // now run the io loop
    ioc.run();
}