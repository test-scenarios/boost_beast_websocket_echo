#pragma once
#include <memory>

namespace beast_fun_times::util
{
    namespace detail
    {
        union sbo_storage
        {
            sbo_storage() noexcept
            {
            }

            ~sbo_storage()
            {
            }

            void *                             short_[6];
            std::unique_ptr< unsigned char[] > long_;
        };

        template < class Ret, class... Args >
        struct poly_handler_vtable
        {
            // move-construct from value - may throw
            virtual void
            move_construct(sbo_storage &storage, void *source) const = 0;

            // move construct from other SBO. May not throw
            virtual void
            move_construct(sbo_storage &storage,
                           sbo_storage &source) const noexcept = 0;

            virtual Ret
            invoke(sbo_storage &storage, Args... args) const = 0;

            virtual void
            destroy(sbo_storage &storage) const noexcept = 0;
        };

        template < class Actual, class Ret, class... Args >
        static auto
        make_small_poly_handler_vtable()
        {
            static_assert(sizeof(Actual) <= sizeof(sbo_storage));

            static const struct : poly_handler_vtable< Ret, Args... >
            {
                static Actual &
                realise(sbo_storage &storage) noexcept
                {
                    return *reinterpret_cast< Actual * >(&storage.short_[0]);
                }

                void
                move_construct(sbo_storage &storage,
                               void *       source) const override
                {
                    new (&realise(storage)) Actual(
                        std::move(*reinterpret_cast< Actual * >(source)));
                }

                void
                move_construct(sbo_storage &storage,
                               sbo_storage &source) const noexcept override
                {
                    new (&realise(storage)) Actual(std::move(realise(source)));
                }

                Ret
                invoke(sbo_storage &storage, Args... args) const override
                {
                    auto act = std::move(realise(storage));
                    destroy(storage);
                    return act(std::move(args)...);
                }

                void
                destroy(sbo_storage &storage) const noexcept override
                {
                    realise(storage).~Actual();
                }
            } x;
            return &x;
        }

        template < class Actual, class Ret, class... Args >
        auto
        make_big_poly_handler_vtable()
        {
            static_assert(sizeof(Actual) > sizeof(sbo_storage));

            static const struct : poly_handler_vtable< Ret, Args... >
            {
                void
                move_construct(sbo_storage &storage,
                               void *       source) const override
                {
                    constexpr auto size = sizeof(Actual);
                    new (&storage.long_) std::unique_ptr< unsigned char[] >();
                    storage.long_.reset(new unsigned char[size]);
                    new (storage.long_.get()) Actual(
                        std::move(*reinterpret_cast< Actual * >(source)));
                }

                void
                move_construct(sbo_storage &storage,
                               sbo_storage &source) const noexcept override
                {
                    new (&storage.long_) std::unique_ptr<unsigned char[]>(std::move(source.long_));
                }

                Ret
                invoke(sbo_storage &storage, Args... args) const override
                {
                    auto act = std::move(*reinterpret_cast<Actual*>(storage.long_.get()));
                    storage.long_.reset();
                    destroy(storage);
                    return act(std::move(args)...);
                }

                void
                destroy(sbo_storage &storage) const noexcept override
                {
                    if (storage.long_)
                        reinterpret_cast<Actual*>(storage.long_.get())->~Actual();
                    storage.long_.~unique_ptr< unsigned char[] >();
                }
            } x;
            return &x;
        }

    }   // namespace detail
    /// A polymorphic completion handler
    /// \tparam Sig
    template < class Sig >
    struct poly_handler;

    template < class Ret, class... Args >
    class poly_handler< Ret(Args...) >
    {
        detail::sbo_storage                                storage_;
        detail::poly_handler_vtable< Ret, Args... > const *kind_;

      public:
        poly_handler()
        : storage_ {}
        , kind_(nullptr)
        {
        }

        template <
            class Actual,
            std::enable_if_t< !std::is_same_v< Actual, poly_handler > &&
                              std::is_invocable_r_v< Ret, Actual, Args... > >
                * = nullptr >
        poly_handler(Actual actual)
        {
            constexpr auto size = sizeof(Actual);
            if constexpr (size > sizeof(storage_))
            {
                // non-sbo
                auto kind = detail::
                    make_big_poly_handler_vtable< Actual, Ret, Args... >();
                kind->move_construct(storage_, std::addressof(actual));
                kind_ = kind;
            }
            else
            {
                // sbo
                auto kind = detail::
                    make_small_poly_handler_vtable< Actual, Ret, Args... >();
                kind->move_construct(storage_, std::addressof(actual));
                kind_ = kind;
            }
        }

        poly_handler(poly_handler &&other) noexcept
        : storage_ {}
        , kind_(std::exchange(other.kind_, nullptr))
        {
            if(kind_)
                kind_->move_construct(storage_, other.storage_);
        }

        poly_handler &
        operator=(poly_handler &&other) noexcept
        {
            this->~poly_handler();
            new (this) poly_handler(std::move(other));
            return *this;
        }

        ~poly_handler()
        {
            if (kind_)
                kind_->destroy(storage_);
        }

        Ret
        operator()(Args... args)
        {
            auto k = std::exchange(kind_, nullptr);
            if (k)
                return k->invoke(storage_, std::move(args)...);
            else
                throw std::bad_function_call();
        }

        bool
        has_value() const
        {
            return kind_ != nullptr;
        }
        operator bool() const
        {
            return has_value();
        }
    };
}   // namespace beast_fun_times::util