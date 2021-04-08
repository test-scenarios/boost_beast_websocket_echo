#pragma once

#include <util/st/detail/stop_token.hpp>

namespace util { namespace st {

    /// The stop_source and stop_token is a simple signal/slot device to
    /// indicate
    /// a one-time event. The slots are invoked on the same thread as the
    /// signal.

    using stop_slot = detail::stop_slot;
    struct stop_source;
    struct stop_token;

    struct stop_connection
    {
        void
        disconnect()
        {
            if (auto s = std::exchange(source_, {}))
                s->disconnect(id_);
        }

      private:
        friend stop_token;

        std::shared_ptr< detail::stop_source_impl > source_;
        std::uint64_t                               id_;

        stop_connection(std::shared_ptr< detail::stop_source_impl > source,
                        std::uint64_t                               id)
        : source_(std::move(source))
        , id_(id)
        {
        }
    };

    struct stop_token
    {
        stop_token()
        : source_()
        {
        }

        bool
        stopped() const
        {
            return source_ ? source_->stopped() : false;
        }

        stop_connection
        connect(stop_slot slot)
        {
            assert(source_);
            return stop_connection(source_, source_->connect(std::move(slot)));
        }

      private:
        friend stop_source;

        stop_token(std::shared_ptr< detail::stop_source_impl > source)
        : source_(std::move(source))
        {
        }

        std::shared_ptr< detail::stop_source_impl > source_;
    };

    struct stop_source
    {
        stop_source()
        : impl_(std::make_shared< detail::stop_source_impl >())
        {
        }

        stop_token
        make_token()
        {
            return stop_token(impl_);
        }

        void
        stop()
        {
            if (impl_)
                impl_->stop();
        }

      private:
        std::shared_ptr< detail::stop_source_impl > impl_;
    };

}}   // namespace util::st