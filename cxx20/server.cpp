#include "server.hpp"

#include <iostream>

namespace project
{
    server::server(net::any_io_executor exec)
    : acceptor_(exec)
    {
        auto ep = net::ip::tcp::endpoint(net::ip::address_v4::any(), 4321);
        acceptor_.open(ep.protocol());
        acceptor_.set_option(net::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(ep);
        acceptor_.listen();
    }

    void server::run()
    {
        net::co_spawn(
            acceptor_.get_executor(),
            [this]() -> net::awaitable< void > { co_await this->handle_run(); },
            net::detached);
    }

    void server::stop()
    {
        net::dispatch(net::bind_executor(acceptor_.get_executor(), [this] { this->handle_stop(); }));
    }

    net::awaitable< void > server::handle_run()
    {
        while (!ec_)
            try
            {
                auto sock = co_await acceptor_.async_accept(net::use_awaitable);
                auto ep   = sock.remote_endpoint();
                auto conn = std::make_shared< connection_impl >(std::move(sock));
                // cache the connection
                connections_[ep] = conn;
                conn->run();
                conn->send("Welcome to my websocket server!\n");
                conn->send("You are visitor number " + std::to_string(connections_.size()) + "\n");
                conn->send("You connected from " + ep.address().to_string() + ":" + std::to_string(ep.port()) + "\n");
                conn->send("Be good!\n");
            }
            catch (system_error &se)
            {
                if (se.code() != net::error::connection_aborted)
                    throw;
            }
    }

    void server::handle_stop()
    {
        ec_ = net::error::operation_aborted;
        acceptor_.cancel();
        for (auto &[ep, weak_conn] : connections_)
            if (auto conn = weak_conn.lock())
            {
                std::cout << "stopping connection on " << ep << std::endl;
                conn->stop();
            }
        connections_.clear();
    }
}   // namespace project