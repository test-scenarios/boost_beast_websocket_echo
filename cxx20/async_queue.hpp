#pragma once
#include "config.hpp"

#include <memory>
#include <unordered_map>

namespace project
{
    template < class Sig >
    struct poly_handler;

    template < class... Args >
    class poly_handler< void(Args...) >
    {
        using storage_type = union
        {
            void *                      short_[4];
            std::unique_ptr< void *[] > long_;
        };

        struct handler
        {
            virtual void
            move_construct(storage_type &storage, void *source) = 0;
            virtual void
            invoke(storage_type &storage, Args... args) = 0;
            virtual void
            destroy(storage_type &storage) = 0;
        };

        storage_type storage_;
        handler *    kind;

        template < class Actual >
        static handler *
        make_sbo_handler()
        {
            static const struct : handler
            {
                static Actual &
                realise(storage_type &storage)
                {
                    return *reinterpret_cast< Actual * >(&storage.short_[0]);
                }

                void
                move_construct(storage_type &storage, void *source) override
                {
                    new (&realise(storage)) Actual(
                        std::move(*reinterpret_cast< Actual * >(source)));
                }

                void
                move_construct(storage_type &storage,
                               storage_type &source) override
                {
                    new (&realise(storage)) Actual(std::move(realise(source)));
                }

                void
                invoke(storage_type &storage, Args... args) override
                {
                    auto act = std::move(realise(storage));
                    destroy(storage);
                    act(std::move(args)...);
                }

                void
                destroy(storage_type &storage) override
                {
                    realise(storage).~Actual();
                }
            } x;
            return &x;
        }

        template < class Actual >
        static handler *
        make_long_handler()
        {
            static const struct : handler
            {
                static Actual &
                realise(storage_type &storage)
                {
                    return *reinterpret_cast< Actual * >(&storage.long_[0]);
                }

                void
                move_construct(storage_type &storage, void *source) override
                {
                    auto blocks =
                        sizeof(Actual) + sizeof(void *) - 1 / sizeof(void *);
                    storage.long_ = std::make_unique< void *[] >(blocks);
                    new (&realise(storage)) Actual(
                        std::move(*reinterpret_cast< Actual * >(source)));
                }

                void
                move_construct(storage_type &storage,
                               storage_type &source) override
                {
                    new (&storage.long_)
                        std::unique_ptr< void *[] >(std::move(source.long_));
                }

                void
                invoke(storage_type &storage, Args... args) override
                {
                    auto act = std::move(realise(storage));
                    storage.long_.reset();
                    destroy(storage);
                    act(std::move(args)...);
                }

                void
                destroy(storage_type &storage) override
                {
                    if (storage_.long_)
                        realise().~Actual();
                    storage_.long_.~unique_ptr< void *[] >();
                }
            } x;
            return &x;
        }

      public:
        poly_handler()
        : storage_ {}
        , kind(nullptr)
        {
        }

        template < class Actual,
                   std::enable_if_t< !std::is_same_v< Actual, poly_handler > >
                       * = nullptr >
        poly_handler(Actual actual)
        {
            using actual_type = Actual;
            if constexpr (sizeof(actual_type) > sizeof(storage_.short_))
            {
                // non-sbo
                kind = make_long_handler< actual_type >();
            }
            else
            {
                // sbo
                kind = make_sbo_handler< actual_type >();
            }
            kind->move_construct(storage_, std::addressof(actual));
        }

        poly_handler(poly_handler &&other)
        : storage_ {}
        , kind(std::exchange(other.kind, nullptr))
        {
            kind->move_construct(storage_, other.storage_);
        }

        ~poly_handler()
        {
            if (kind)
                kind->destroy(storage_);
        }

        void
        operator()(Args... args)
        {
            auto k = std::exchange(kind, nullptr);
            assert(k);
            k->invoke(std::move(args)...);
        }
    };

    template < class T >
    struct async_queue
    {
        using value_type = T;

        struct impl;
        struct connection;

        async_queue(net::executor exec);
        async_queue(async_queue &&other);
        async_queue &
        operator=(async_queue &&other);
        ~async_queue();

        connection
        connect(net::executor exec);

        void push(T arg);
        void cancel();

      private:
        std::shared_ptr< impl > impl_;
    };

    template < class T >
    struct async_queue< T >::connection
    {
        struct impl;

        connection(net::executor exec);
        connection(connection &&other);
        connection &
        operator=(connection &&other);
        ~connection();

        template < class WaitHandler >
        auto
        async_wait(WaitHandler &&token) ->
            typename net::async_result< std::decay_t< WaitHandler >,
                                        void(error_code, T) >::result_type;

      private:
        std::shared_ptr< impl > impl_;
    };

    template < class T >
    struct async_queue< T >::connection::impl
    {
        net::executor                             exec_;
        std::queue< T >                           items_;
        poly_handler< void(error_code, T) >       handler_ = {};
        std::atomic< bool >                       waiting_ = false;
        std::shared_ptr< async_queue< T >::impl > impl_;
    };

    template < class T >
    struct async_queue< T >::impl : std::enable_shared_from_this< impl >
    {
        using connection_impl = typename async_queue< T >::connection::impl;

        std::unordered_map< connection_impl *,
                            std::weak_ptr< connection_impl > >
            connections_;
    };

    template < class T >
    auto
    async_queue< T >::connect(net::executor exec) -> connection
    {
        return connection(impl_->make_connection(exec));
    }

}   // namespace project