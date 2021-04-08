#include "server.hpp"

#include <iostream>

namespace project {
server::server(net::executor exec)
: acceptor_(exec)
{
    auto ep = net::ip::tcp::endpoint(net::ip::address_v4::any(), 4321);
    acceptor_.open(ep.protocol());
    acceptor_.set_option(net::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(ep);
    acceptor_.listen();
}

void
server::run()
{
    net::dispatch(net::bind_executor(acceptor_.get_executor(),
                                     [this] { this->handle_run(); }));
}

void
server::stop()
{
    net::dispatch(net::bind_executor(acceptor_.get_executor(),
                                     [this] { this->handle_stop(); }));
}

void
server::handle_run()
{
    ec_.clear();
    initiate_accept();
}

void
server::handle_accept(error_code ec, net::ip::tcp::socket sock)
{
    if (ec_)
        std::cout << "server is stopped. abandoning\n";
    else if (ec == net::error::connection_aborted)
    {
        // this is not an error condition
        std::cerr << "info: " << ec.message() << "\n";
        initiate_accept();
    }
    else if (ec == net::error::operation_aborted)
    {
        // result of calling cancel() on acceptor
    }
    else if (ec)
    {
        // this is a real error
        std::cout << "acceptor error:" << ec.message() << "\n";
    }
    else
    {
        // no error
        initiate_accept();

        auto ep   = sock.remote_endpoint();
        auto conn = std::make_shared< connection_impl >(std::move(sock));
        // cache the connection
        connections_[ep] = conn;
        conn->run();
        conn->send("Welcome to my websocket server!\n");
        conn->send("You are visitor number " +
                   std::to_string(connections_.size()) + "\n");
        conn->send("You connected from " + ep.address().to_string() + ":" +
                   std::to_string(ep.port()) + "\n");
        conn->send("Be good!\n");
    }
}
void
server::initiate_accept()
{
    acceptor_.async_accept([this](error_code ec, net::ip::tcp::socket sock) {
        this->handle_accept(ec, std::move(sock));
    });
}
void
server::handle_stop()
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