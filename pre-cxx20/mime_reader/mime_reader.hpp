#pragma once

#include "config.hpp"

#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

namespace program { namespace mime_reader {
    struct error
    {
        enum mime_error
        {
            no_content_length = 1,
        };
    };

    inline boost::system::error_category const &
    get_mime_error_category()
    {
        static const struct : boost::system::error_category
        {
            const char *
            name() const noexcept
            {
                return "mime_error";
            }

            std::string
            message(int code) const
            {
                switch (static_cast< error::mime_error >(code))
                {
                case error::mime_error::no_content_length:
                    return "no content length";
                }
            }
        } _;
        return _;
    }
}}   // namespace program::mime_reader

namespace program { namespace mime_reader {
    inline std::size_t
    to_size(beast::string_view sv)
    {
        // this function may need some work to cater for extra white space in
        // erroneous headers
        auto s = std::string(sv.begin(), sv.end());
        return std::size_t(std::atoi(s.c_str()));
    }
    struct mime_part
    {
        http::fields headers;
        std::string  body;

        void
        clear()
        {
            headers.clear();
            body.clear();
        }
    };

    // keep implementation data at a stable address by allocating on the
    // heap
    struct impl_data
    {
        impl_data(std::string                      boundary,
                  std::function< void(mime_part) > on_part)
        : boundary(std::move(boundary))
        , on_part(std::move(on_part))
        {
        }

        std::string                      boundary;
        std::function< void(mime_part) > on_part;
        char                             boundary_end[2] {};
        std::regex  header_regex { R"regex(^([^:]+):\s*(.*)$)regex" };
        std::size_t remaining = 0;
        mime_part   part;

        bool
        boundary_ok() const
        {
            return boundary_end[0] == '\r' && boundary_end[1] == '\n';
        }
    };

    template < class Stream, class DynamicBuffer_v2 >
    struct read_mime_op : asio::coroutine
    {
        Stream &                     stream_;
        DynamicBuffer_v2             buffer_;
        std::unique_ptr< impl_data > impl_;

        read_mime_op(Stream &                   stream,
                     std::string                boundary,
                     std::function< void(mime_part) > output,
                     DynamicBuffer_v2           buffer)
        : stream_(stream)
        , buffer_(buffer)
        , impl_(new impl_data("--" + boundary, std::move(output)))
        {
        }

#include <boost/asio/yield.hpp>
        template < class Self >
        void operator()(Self &      self,
                        error_code  ec                = {},
                        std::size_t bytes_transferred = 0)
        {
            auto &impl = *impl_;

            if (ec)
                return self.complete(ec);

            reenter(*this) for (;;)
            {
                // prepare the next part output
                impl.part.clear();

                // looking for a boundary
                yield net::async_read_until(
                    stream_, buffer_, impl.boundary, std::move(self));
                buffer_.consume(bytes_transferred);

                spdlog::debug("found boundary");

                // now looking for CRLF or -- after boundary
                if (buffer_.size() < 2)
                {
                    yield net::async_read(
                        stream_,
                        buffer_,
                        net::transfer_exactly(2 - buffer_.size()),
                        std::move(self));
                }

                net::buffer_copy(net::buffer(impl.boundary_end),
                                 buffer_.data(0, 2));
                buffer_.consume(2);
                if (not impl.boundary_ok())
                    yield self.complete(error_code());

                spdlog::debug("found start of part headers");

            // read each header and decompose it
            next_header:
                yield net::async_read_until(
                    stream_, buffer_, "\r\n", std::move(self));
                if (bytes_transferred == 2)
                    buffer_.consume(2);
                else
                {
                    auto               dat = buffer_.data(0, bytes_transferred);
                    beast::string_view line { reinterpret_cast< const char * >(
                                                  dat.data()),
                                              dat.size() - 2 };
                    std::cmatch        match;
                    if (!std::regex_match(
                            line.begin(), line.end(), match, impl.header_regex))
                        return self.complete(http::error::bad_field);
                    impl.part.headers.insert(match[1].str(), match[2].str());
                    buffer_.consume(bytes_transferred);
                    goto next_header;
                }

                spdlog::debug("headers done");

                // now consume the mime jobbie. For now we're going to assume
                // that all mime parts have a content length

                if (!impl.part.headers.count(http::field::content_length))
                    yield self.complete(http::error::bad_content_length);

                impl.remaining =
                    to_size(impl.part.headers[http::field::content_length]);

                while (impl.remaining)
                {
                    {
                        auto dest = net::dynamic_buffer(impl.part.body);
                        auto xfer = std::min(impl.remaining, buffer_.size());
                        dest.grow(xfer);
                        auto xferred = net::buffer_copy(
                            dest.data(dest.size() - xfer, xfer),
                            buffer_.data(0, xfer));
                        dest.shrink(xfer - xferred);
                        buffer_.consume(xferred);
                        impl.remaining -= xferred;
                    }

                    if (impl.remaining)
                    {
                        yield net::async_read(
                            stream_,
                            buffer_,
                            net::transfer_exactly(impl.remaining),
                            std::move(self));
                    }
                }

                impl.on_part(std::move(impl.part));
            }
        }
#include <boost/asio/unyield.hpp>
    };

    template < class Stream,
               class DynamicBuffer_v2,
               BOOST_ASIO_COMPLETION_HANDLER_FOR(void(error_code))
                   CompletionHandler >
    BOOST_ASIO_INITFN_RESULT_TYPE(CompletionHandler, void(error_code))
    async_read_mime(Stream &                         stream,
                    std::string                      boundary,
                    std::function< void(mime_part) > output,
                    DynamicBuffer_v2                 dynbuf,
                    CompletionHandler &&             token)
    {
        return asio::async_compose< CompletionHandler, void(error_code) >(
            read_mime_op< Stream, DynamicBuffer_v2 >(
                stream, std::move(boundary), std::move(output), dynbuf),
            token,
            stream);
    }
}}   // namespace program::mime_reader