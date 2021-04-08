#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>

namespace util { namespace st { namespace detail {

    using stop_slot_sig = void();
    using stop_slot     = std::function< stop_slot_sig >;

    struct stop_source_impl
    {
        static constexpr auto stopped_value =
            std::numeric_limits< std::uint64_t >::max();

        auto
        connect(stop_slot slot) -> std::uint64_t
        {
            auto id = next_id_;
            if (id == stopped_value)
            {
                slot();
            }
            else
            {
                ++next_id_;
                slots_.emplace(id, std::move(slot));
            }
            return id;
        }

        void
        disconnect(std::uint64_t id)
        {
            slots_.erase(id);
        }

        void
        stop()
        {
            next_id_   = stopped_value;
            auto slots = std::move(slots_);
            slots_.clear();
            for (auto &[id, slot] : slots)
            {
                slot();
            }
        }

        bool
        stopped() const
        {
            return next_id_ == stopped_value;
        }

      private:
        std::uint64_t                                  next_id_ = 0;
        std::unordered_map< std::uint64_t, stop_slot > slots_;
    };
}}}   // namespace util::st::detail