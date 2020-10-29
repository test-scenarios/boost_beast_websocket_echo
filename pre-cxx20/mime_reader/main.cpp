#include "config.hpp"
#include "http_stream.hpp"
#include "mime_reader.hpp"

#include <boost/beast/_experimental/test/stream.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

namespace program {

/// @brief Simple implementation to extract boundary. It's a little permissive
/// but it'll do
/// @param content_type
/// @return
boost::optional< std::string >
deduce_boundary(beast::string_view content_type)
{
    boost::optional< std::string > result;

    static std::regex r1 {
        R"regex(^multipart/x-mixed-replace\s*;\s*boundary="([^"]+)"$)regex",
        std::regex_constants::icase
    };
    static std::regex r2 {
        R"regex(^multipart/x-mixed-replace\s*;\s*boundary=([^"]+)$)regex",
        std::regex_constants::icase
    };

    auto match = std::cmatch();

    if (std::regex_match(content_type.begin(), content_type.end(), match, r1) ||
        std::regex_match(content_type.begin(), content_type.end(), match, r2))
    {
        result = match[1].str();
    }

    return result;
}

std::string
mimeify(std::string boundary, std::vector< std::string > docs)
{
    std::ostringstream oss;
    for (auto const &doc : docs)
    {
        oss << "--" << boundary << "\r\n";
        oss << "Content-Type: image/jpeg\r\n";
        oss << "Content-Length: " << doc.size() << "\r\n";
        oss << "\r\n";
        oss << doc << "\r\n";
    }
    oss << "--" << boundary << "--\r\n";
    return oss.str();
}

std::string
chunkify(int chunks, std::string input)
{
    std::ostringstream oss;

    auto chunk_len = (input.size() + (chunks - 1)) / chunks;
    auto pos       = std::size_t(0);
    while (pos < input.size())
    {
        auto chunk = input.substr(pos, chunk_len);
        oss << std::hex << chunk.size() << "\r\n";
        oss << chunk << "\r\n";
        pos += chunk.size();
    }
    oss << "0\r\n";
    return oss.str();
}

}   // namespace program

int
main()
{
    using namespace program;

    spdlog::set_level(spdlog::level::info);

    net::io_context ioc;
    auto            exec = ioc.get_executor();

    auto spin = [&ioc](std::string const &message) {
        std::cout << "debug: " << message << std::endl;
        ioc.restart();
        std::size_t count = 0;
        while (auto c = ioc.poll())
            count += c;
        return count;
    };

    auto local =
        beast::test::stream(ioc);   // simulate the local socket/tls stream
    auto remote = connect(
        local);   // simulate the remote socket so we can push in test data

    // now simulate our host's read

    auto docs     = std::vector< std::string > { "abc", "def" };
    auto payload1 = mimeify("FrameSeparator", docs);
    auto payload2 = chunkify(4, payload1);

    auto unchunked =
        std::string(
            "HTTP/1.1 200 OK\r\n"
            "Date: Thu, 22 Oct 2020 20:26:01 GMT\r\n"
            "Content-Type: "
            "multipart/x-mixed-replace;boundary=\"FrameSeparator\"\r\n"
            "\r\n") +
        payload1;

    auto chunked =
        std::string(
            "HTTP/1.1 200 OK\r\n"
            "Date: Thu, 22 Oct 2020 20:26:01 GMT\r\n"
            "Content-Type: "
            "multipart/x-mixed-replace;boundary=\"FrameSeparator\"\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n") +
        payload2;

    bool do_chunked = true;
    remote.write_some(do_chunked ? net::buffer(chunked)
                                 : net::buffer(unchunked));

    error_code  ec;
    std::size_t bytes_transferred = 0;
    auto        handler = [&](error_code ec_, std::size_t bytes_transferred_) {
        ec                = ec_;
        bytes_transferred = bytes_transferred_;
    };
    auto quit_check = [&] {
        if (ec)
        {
            std::cerr << "error: " << ec.message() << std::endl;
            std::exit(1);
        }
    };

    http::response_parser< http::buffer_body > parser;
    beast::multi_buffer                        rxbuf;

    //
    // This is where you read the header
    //

    parser.eager(false);
    http::async_read_header(local, rxbuf, parser, handler);
    spin("reading header");
    quit_check();

    auto boundary = deduce_boundary(parser.get()[http::field::content_type]);
    if (not boundary)
    {
        std::cerr << "Not mime\n";
    }

    std::cout << "detected mime boundary: " << *boundary << std::endl;
    std::cout << "chunked: " << (parser.chunked() ? "yes" : "no") << std::endl;

    //
    // now we want to scan the input looking for the boundary
    //

    std::string work_store;
    bool        first_part          = true;
    bool        expect_chunk_header = true;
    auto        outbuf              = asio::dynamic_buffer(work_store);

    auto on_chunk_body = [&](std::uint64_t      remain,
                             beast::string_view body,
                             error_code &       ec) {
        auto pos = outbuf.size();
        auto len = body.size();
        outbuf.grow(len);
        net::buffer_copy(outbuf.data(pos, len), net::buffer(body.data(), len));
        expect_chunk_header = (remain == 0);
        return len;
    };
    parser.on_chunk_body(on_chunk_body);

    auto        boundary_pos = std::string::size_type(0);
    std::string boundary_line;

next_boundary:
    do
    {
        if (parser.chunked())
        {
            while (expect_chunk_header)
            {
                http::async_read_some(local, rxbuf, parser, handler);
                spin("skipping chunk header");
            }
            http::async_read_some(local, rxbuf, parser, handler);
            spin("seeking boundary");
        }
        else
        {
            outbuf.grow(512);
            auto dat = outbuf.data(outbuf.size() - 512, outbuf.size());
            parser.get().body().data = dat.data();
            parser.get().body().size = dat.size();
            parser.get().body().more = false;
            http::async_read_some(local, rxbuf, parser, handler);
            spin("seeking boundary");
            outbuf.shrink(512 - bytes_transferred);
        }
        quit_check();
        boundary_line = "--" + *boundary + "\r\n";
        boundary_pos  = work_store.find(boundary_line);
        if (boundary_pos >= work_store.size())
        {
            boundary_line = "--" + *boundary + "--\r\n";
            boundary_pos  = work_store.find(boundary_line);
        }
    } while (boundary_pos >= work_store.size());

    if (!first_part)
    {
        // process the previous part
        std::cout << "***HERE IS A PART: "
                  << work_store.substr(0, boundary_pos - 2) << std::endl;
    }

    if (boundary_line == "--" + *boundary + "--\r\n")
    {
        std::cout << "End of Stream\n";
        std::exit(0);
    }

    work_store.erase(0, boundary_pos + boundary_line.size());

// now pick up headers
next_header:
    auto hdr_pos = work_store.find("\r\n");
    if (hdr_pos == std::string::npos)
    {
        if (parser.chunked())
        {
            while (expect_chunk_header)
            {
                http::async_read_some(local, rxbuf, parser, handler);
                spin("skipping chunk header");
            }
            http::async_read_some(local, rxbuf, parser, handler);
            spin("seeking boundary header");
        }
        else
        {
            outbuf.grow(512);
            auto dat = outbuf.data(outbuf.size() - 512, outbuf.size());
            parser.get().body().data = dat.data();
            parser.get().body().size = dat.size();
            parser.get().body().more = false;
            http::async_read_some(local, rxbuf, parser, handler);
            spin("seeking boundary header");
            outbuf.shrink(512 - parser.get().body().size);
        }
        quit_check();
        goto next_header;
    }
    if (hdr_pos == 0)
    {
        // end of headers
    }
    else
    {
        std::cout << "-- " << work_store.substr(0, hdr_pos) << std::endl;
        work_store.erase(0, hdr_pos + 2);
        goto next_header;
    }

    first_part = false;
    goto next_boundary;
}