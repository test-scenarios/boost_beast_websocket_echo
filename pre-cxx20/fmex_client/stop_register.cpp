#include "stop_register.hpp"

#include <boost/core/exchange.hpp>

namespace project
{
    auto
    stop_register::add(std::function< void() > f) -> stop_token
    {
        auto id = ++next_id_;
        map_.emplace(id, std::move(f));
        return stop_token(id, this);
    }

    void
    stop_register::remove(stop_id id)
    {
        map_.erase(id);
    }

    stop_token::~stop_token() noexcept
    {
        reset();
    }

    void
    stop_token::swap(stop_token &other) noexcept
    {
        using std::swap;
        swap(id_, other.id_);
        swap(register_, other.register_);
    }

    stop_token &
    stop_token::operator=(stop_token &&other) noexcept
    {
        auto tmp = stop_token(std::move(other));
        swap(tmp);
        return *this;
    }

    stop_token::stop_token(stop_token &&other) noexcept
    : id_(boost::exchange(other.id_, 0))
    , register_(other.register_)
    {
    }

    stop_token::stop_token(stop_id id, stop_register *r) noexcept
    : id_(id)
    , register_(r)
    {
    }

    void
    stop_register::notify_all()
    {
        auto m = std::move(map_);
        map_.clear();
        for (auto &elem : m)
            elem.second();
    }

    void
    stop_token::reset() noexcept
    {
        if (auto id = boost::exchange(id_, 0))
            register_->remove(id);
    }
}   // namespace project