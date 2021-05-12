#include "connection_pool.hpp"

#include <iostream>

namespace project
{
    connection_pool::connection_pool(net::any_io_executor exec)
    : timer_(exec)
    {
    }

    void connection_pool::run()
    {
        net::dispatch(net::bind_executor(timer_.get_executor(), [this] { this->handle_run(); }));
    }

    void connection_pool::stop()
    {
        net::dispatch(net::bind_executor(timer_.get_executor(), [this] { this->handle_stop(); }));
    }

    void connection_pool::handle_run()
    {
        ec_.clear();
        another_connection();
    }

    void connection_pool::handle_timer(error_code ec)
    {
        if (ec_)
            std::cout << "server is stopped. abandoning\n";
        else if (ec == net::error::operation_aborted)
        {
            // result of calling cancel() on acceptor
        }
        else if (ec)
        {
            // this is a real error
            std::cout << "timer error:" << ec.message() << "\n";
        }
        else
        {
            another_connection();
        }
    }
    void connection_pool::initiate_timer()
    {
        if (!ec_ && connections_.size() < 100)
        {
            timer_.expires_after(std::chrono::seconds(1));
            timer_.async_wait(
                net::bind_executor(timer_.get_executor(), [this](error_code ec) { this->handle_timer(ec); }));
        }
    }
    void connection_pool::handle_stop()
    {
        ec_ = net::error::operation_aborted;
        timer_.cancel();
        for (auto &[ep, weak_conn] : connections_)
            if (auto conn = weak_conn.lock())
            {
                std::cout << "stopping connection on " << ep << std::endl;
                conn->stop();
            }
        connections_.clear();
    }

    void connection_pool::another_connection() {
        auto con = std::make_shared<connection_impl>(net::make_strand(timer_.get_executor()));
        con->run();
        connections_[con->local_endpoint()] = con;
    }
}   // namespace project