#pragma once
#include <unordered_map>
#include <functional>

namespace project
{
    struct stop_token;

    using stop_id = std::uint64_t;

    struct stop_register
    {
        stop_token
        add(std::function< void() > f);

        void
        remove(stop_id id);

        void notify_all();

      private:

        using stop_map = std::unordered_map< stop_id, std::function< void() > >;
        stop_id  next_id_ = 0;
        stop_map map_;
    };

    struct stop_token
    {
        explicit stop_token(stop_id id = 0, stop_register *r = nullptr) noexcept;

        stop_token(stop_token &&other) noexcept;

        stop_token &
        operator=(stop_token &&other) noexcept;

        ~stop_token() noexcept;

        void
        swap(stop_token &other) noexcept;

        void reset() noexcept;

      private:
        stop_id        id_;
        stop_register *register_;
    };


}   // namespace project