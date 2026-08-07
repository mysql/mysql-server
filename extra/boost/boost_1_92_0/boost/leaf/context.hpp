#ifndef BOOST_LEAF_CONTEXT_HPP_INCLUDED
#define BOOST_LEAF_CONTEXT_HPP_INCLUDED

// Copyright 2018-2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/detail/diagnostics_writer.hpp>
#include <boost/leaf/error.hpp>

#if !defined(BOOST_LEAF_NO_THREADS) && !defined(NDEBUG)
#   include <thread>
#endif

namespace boost { namespace leaf {

class error_info;
class diagnostic_info;
class diagnostic_details;

template <class>
struct is_predicate: std::false_type
{
};

namespace detail
{
    template <class T>
    struct is_exception: std::is_base_of<std::exception, typename std::decay<T>::type>
    {
    };

    template <class E>
    struct handler_argument_traits;

    template <class E, bool IsPredicate = is_predicate<E>::value>
    struct handler_argument_traits_defaults;

    template <class E>
    struct handler_argument_traits_defaults<E, false>
    {
        using error_type = typename std::decay<E>::type;
        using context_types = leaf_detail_mp11::mp_list<error_type>;
        constexpr static bool always_available = false;

        template <class Tup>
        BOOST_LEAF_CONSTEXPR static error_type const * check( Tup const &, error_info const & ) noexcept;

        template <class Tup>
        BOOST_LEAF_CONSTEXPR static error_type * check( Tup &, error_info const & ) noexcept;

        template <class Context>
        BOOST_LEAF_CONSTEXPR static E get( Context & ctx, error_info const & ei ) noexcept
        {
            return *check(ctx.tup(), ei);
        }

        static_assert(!is_predicate<error_type>::value, "Handlers must take predicate arguments by value");
        static_assert(!std::is_same<E, error_info>::value, "Handlers must take leaf::error_info arguments by const &");
        static_assert(!std::is_same<E, diagnostic_info>::value, "Handlers must take leaf::diagnostic_info arguments by const &");
        static_assert(!std::is_same<E, diagnostic_details>::value, "Handlers must take leaf::diagnostic_details arguments by const &");
    };

    template <class Pred>
    struct handler_argument_traits_defaults<Pred, true>: handler_argument_traits<typename Pred::error_type>
    {
        using base = handler_argument_traits<typename Pred::error_type>;
        static_assert(!base::always_available, "Predicates can't use types that are always_available");

        template <class Tup>
        BOOST_LEAF_CONSTEXPR static bool check( Tup const & tup, error_info const & ei ) noexcept
        {
            auto e = base::check(tup, ei);
            return e && Pred::evaluate(*e);
        }

        template <class Context>
        BOOST_LEAF_CONSTEXPR static Pred get( Context const & ctx, error_info const & ei ) noexcept
        {
            return Pred{*base::check(ctx.tup(), ei)};
        }
    };

    template <class... E>
    struct handler_argument_always_available
    {
        using context_types = leaf_detail_mp11::mp_list<E...>;
        constexpr static bool always_available = true;

        template <class Tup>
        BOOST_LEAF_CONSTEXPR static bool check( Tup &, error_info const & ) noexcept
        {
            return true;
        }
    };

    template <class E>
    struct handler_argument_traits: handler_argument_traits_defaults<E>
    {
    };

    template <>
    struct handler_argument_traits<void>
    {
        using context_types = leaf_detail_mp11::mp_list<>;
        constexpr static bool always_available = false;

        template <class Tup>
        BOOST_LEAF_CONSTEXPR static std::exception const * check( Tup const &, error_info const & ) noexcept;
    };

    template <class E>
    struct handler_argument_traits<E &&>
    {
        static_assert(sizeof(E) == 0, "Error handlers may not take rvalue ref arguments");
    };

    template <class E>
    struct handler_argument_traits<E *>: handler_argument_always_available<typename std::remove_const<E>::type>
    {
        template <class Context>
        BOOST_LEAF_CONSTEXPR static E * get( Context & ctx, error_info const & ei) noexcept
        {
            return handler_argument_traits_defaults<E>::check(ctx.tup(), ei);
        }
    };

    template <class E>
    struct handler_argument_traits_require_by_value
    {
        static_assert(sizeof(E) == 0, "Error handlers must take this type by value");
    };
} // namespace detail

////////////////////////////////////////

namespace detail
{
    template <class T>
    struct get_dispatch
    {
        static BOOST_LEAF_CONSTEXPR T const * get(T const * x) noexcept
        {
            return x;
        }
        static BOOST_LEAF_CONSTEXPR T const * get(void const *) noexcept
        {
            return nullptr;
        }
    };

    template <class T>
    BOOST_LEAF_CONSTEXPR inline T * find_in_tuple(std::tuple<> const &)
    {
        return nullptr;
    }

    template <class T, int I = 0, class... Tp>
    BOOST_LEAF_CONSTEXPR inline typename std::enable_if<I == sizeof...(Tp) - 1, T>::type const *
    find_in_tuple(std::tuple<Tp...> const & t) noexcept
    {
        return get_dispatch<T>::get(&std::get<I>(t));
    }

    template<class T, int I = 0, class... Tp>
    BOOST_LEAF_CONSTEXPR inline typename std::enable_if<I < sizeof...(Tp) - 1, T>::type const *
    find_in_tuple(std::tuple<Tp...> const & t) noexcept
    {
        if( T const * x = get_dispatch<T>::get(&std::get<I>(t)) )
            return x;
        else
            return find_in_tuple<T, I+1, Tp...>(t);
    }
} // namespace detail

////////////////////////////////////////

namespace detail
{
    template <int I, class Tup>
    struct tuple_for_each
    {
        BOOST_LEAF_CONSTEXPR static void activate( Tup & tup ) noexcept
        {
            static_assert(!std::is_same<error_info, typename std::decay<decltype(std::get<I-1>(tup))>::type>::value, "Bug in LEAF: context type deduction");
            tuple_for_each<I-1,Tup>::activate(tup);
            std::get<I-1>(tup).activate();
        }

        BOOST_LEAF_CONSTEXPR static void deactivate( Tup & tup ) noexcept
        {
            static_assert(!std::is_same<error_info, typename std::decay<decltype(std::get<I-1>(tup))>::type>::value, "Bug in LEAF: context type deduction");
            std::get<I-1>(tup).deactivate();
            tuple_for_each<I-1,Tup>::deactivate(tup);
        }

        BOOST_LEAF_CONSTEXPR static void unload( Tup & tup, int err_id ) noexcept(!BOOST_LEAF_CFG_CAPTURE)
        {
            static_assert(!std::is_same<error_info, typename std::decay<decltype(std::get<I-1>(tup))>::type>::value, "Bug in LEAF: context type deduction");
            BOOST_LEAF_ASSERT(err_id != 0);
            auto & sl = std::get<I-1>(tup);
            sl.unload(err_id);
            tuple_for_each<I-1,Tup>::unload(tup, err_id);
        }

        template <class Encoder>
        static void serialize_to(Encoder & e, void const * tup, error_id id)
        {
            BOOST_LEAF_ASSERT(tup != nullptr);
            tuple_for_each<I-1,Tup>::serialize_to(e, tup, id);
            std::get<I-1>(*static_cast<Tup const *>(tup)).serialize_to(e, id);
        }
    };

    template <class Tup>
    struct tuple_for_each<0, Tup>
    {
        BOOST_LEAF_CONSTEXPR static void activate( Tup & ) noexcept { }
        BOOST_LEAF_CONSTEXPR static void deactivate( Tup & ) noexcept { }
        BOOST_LEAF_CONSTEXPR static void unload( Tup &, int ) noexcept { }
        BOOST_LEAF_CONSTEXPR static void serialize_to(encoder &, void const *, error_id) { }
    };

    class context_base;

    template <class Context>
    void serialize_context_to(encoder & e, context_base const & ctx, error_id id)
    {
        static_cast<Context const &>(ctx).serialize_to(e, id);
    }
} // namespace detail

////////////////////////////////////////

namespace detail
{
    template <class T> struct does_not_participate_in_context_deduction: std::is_abstract<T> { };
    template <> struct does_not_participate_in_context_deduction<error_id>: std::true_type { };

    template <class L>
    struct deduce_e_type_list;

    template <template<class...> class L, class... T>
    struct deduce_e_type_list<L<T...>>
    {
        using type =
            leaf_detail_mp11::mp_remove_if<
                leaf_detail_mp11::mp_unique<
                    leaf_detail_mp11::mp_append<typename handler_argument_traits<T>::context_types...>
                >,
                does_not_participate_in_context_deduction
            >;
    };

    template <class L>
    struct deduce_e_tuple_impl;

    template <template <class...> class L, class... E>
    struct deduce_e_tuple_impl<L<E...>>
    {
        using type = std::tuple<slot<E>...>;
    };

    template <class... E>
    using deduce_e_tuple = typename deduce_e_tuple_impl<typename deduce_e_type_list<leaf_detail_mp11::mp_list<E...>>::type>::type;
} // namespace detail

////////////////////////////////////////

namespace detail
{
    // The context_base type is able to walk up the parent error handling
    // scopes to collect relevant error objects, so they can be serialized or
    // printed by diagnostic_details.
    //
    // This introduces a bit of overhead which is semantically similar to the
    // rest of the overhead introduced by BOOST_LEAF_CFG_CAPTURE=1, namely
    // allocating unhandled error objects dynamically if diagnostic_details is
    // handled.
    class context_base
    {
#if BOOST_LEAF_CFG_CAPTURE
        context_base( context_base const & ) = delete;
        context_base & operator=( context_base const & ) = delete;

    protected:

        context_base * parent_ = nullptr;

        context_base()
        {
            tls::reserve_ptr<context_base>();
        }

        context_base( context_base && ) noexcept = default;
        ~context_base() noexcept = default;

        void link() noexcept
        {
            parent_ = tls::read_ptr<context_base>();
            tls::write_ptr<context_base>(this);
        }

        void unlink() noexcept
        {
            BOOST_LEAF_ASSERT(tls::read_ptr<context_base>() == this);
            tls::write_ptr<context_base>(parent_);
        }

    public:

        virtual void serialize_to_( encoder &, error_id ) const = 0;
#endif
    }; // class context_base
} // namespace detail


template <class... E>
class context final:
    public detail::context_base
{
    context( context const & ) = delete;
    context & operator=( context const & ) = delete;

    using Tup = detail::deduce_e_tuple<E...>;
    Tup tup_;
    bool is_active_;

#if !defined(BOOST_LEAF_NO_THREADS) && !defined(NDEBUG)
    std::thread::id thread_id_;
#endif

    class raii_deactivator
    {
        raii_deactivator( raii_deactivator const & ) = delete;
        raii_deactivator & operator=( raii_deactivator const & ) = delete;
        context * ctx_;
    public:
        explicit BOOST_LEAF_CONSTEXPR BOOST_LEAF_ALWAYS_INLINE raii_deactivator(context & ctx) noexcept:
            ctx_(ctx.is_active() ? nullptr : &ctx)
        {
            if( ctx_ )
                ctx_->activate();
        }
#if __cplusplus < 201703L
        BOOST_LEAF_CONSTEXPR BOOST_LEAF_ALWAYS_INLINE raii_deactivator( raii_deactivator && x ) noexcept:
            ctx_(x.ctx_)
        {
            x.ctx_ = nullptr;
        }
#endif
        BOOST_LEAF_ALWAYS_INLINE ~raii_deactivator() noexcept
        {
            if( ctx_ && ctx_->is_active() )
                ctx_->deactivate();
        }
    };

#if BOOST_LEAF_CFG_CAPTURE
    void serialize_to_( detail::encoder & e, error_id id ) const override
    {
        detail::tuple_for_each<std::tuple_size<Tup>::value, Tup>::serialize_to(e, &tup_, id);
        if( parent_ )
            parent_->serialize_to_(e, id);
    }
#endif

public:

    template <class Encoder>
    void serialize_to( Encoder & e, error_id id ) const
    {
        detail::tuple_for_each<std::tuple_size<Tup>::value, Tup>::serialize_to(e, &tup_, id);
#if BOOST_LEAF_CFG_CAPTURE
        if( parent_ )
            parent_->serialize_to_(e, id);
#endif
    }

    BOOST_LEAF_CONSTEXPR context( context && x ) noexcept:
        context_base(std::move(x)),
        tup_(std::move(x.tup_)),
        is_active_(false)
    {
        BOOST_LEAF_ASSERT(!x.is_active());
    }

    BOOST_LEAF_CONSTEXPR context():
        is_active_(false)
    {
    }

    ~context() noexcept
    {
        BOOST_LEAF_ASSERT(!is_active());
    }

    BOOST_LEAF_CONSTEXPR Tup const & tup() const noexcept
    {
        return tup_;
    }

    BOOST_LEAF_CONSTEXPR Tup & tup() noexcept
    {
        return tup_;
    }

    BOOST_LEAF_CONSTEXPR void activate() noexcept
    {
        using namespace detail;
        BOOST_LEAF_ASSERT(!is_active());
#if !defined(BOOST_LEAF_NO_THREADS) && !defined(NDEBUG)
        thread_id_ = std::this_thread::get_id();
#endif
#if BOOST_LEAF_CFG_CAPTURE
        context_base::link();
#endif
        tuple_for_each<std::tuple_size<Tup>::value,Tup>::activate(tup_);
        is_active_ = true;
    }

    BOOST_LEAF_CONSTEXPR void deactivate() noexcept
    {
        using namespace detail;
        BOOST_LEAF_ASSERT(is_active());
#if !defined(BOOST_LEAF_NO_THREADS) && !defined(NDEBUG)
        BOOST_LEAF_ASSERT(std::this_thread::get_id() == thread_id_);
        thread_id_ = std::thread::id();
#endif
        is_active_ = false;
        tuple_for_each<std::tuple_size<Tup>::value,Tup>::deactivate(tup_);
#if BOOST_LEAF_CFG_CAPTURE
        context_base::unlink();
#endif
    }

    void unload(error_id id) noexcept(!BOOST_LEAF_CFG_CAPTURE)
    {
        BOOST_LEAF_ASSERT(!is_active());
        tls::write_current_error_id(static_cast<unsigned>(id.value()));
        detail::tuple_for_each<std::tuple_size<Tup>::value,Tup>::unload(tup_, id.value());
    }

    BOOST_LEAF_CONSTEXPR bool is_active() const noexcept
    {
        return is_active_;
    }

    template <class T>
    BOOST_LEAF_CONSTEXPR T const * get(error_id err) const noexcept
    {
        detail::slot<T> const * e = detail::find_in_tuple<detail::slot<T>>(tup_);
        return e ? e->has_value(err.value()) : nullptr;
    }

    template <class R, class... H>
    BOOST_LEAF_CONSTEXPR R handle_error( error_id, H && ... ) const;

    template <class R, class... H>
    BOOST_LEAF_CONSTEXPR R handle_error( error_id, H && ... );

    friend BOOST_LEAF_CONSTEXPR BOOST_LEAF_ALWAYS_INLINE raii_deactivator activate_context(context & ctx) noexcept
    {
        return raii_deactivator(ctx);
    }
}; // template context

////////////////////////////////////////

namespace detail
{
    template <class TypeList>
    struct deduce_context_impl;

    template <template <class...> class L, class... E>
    struct deduce_context_impl<L<E...>>
    {
        using type = context<E...>;
    };

    template <class TypeList>
    using deduce_context = typename deduce_context_impl<TypeList>::type;

    template <class H>
    struct fn_mp_args_fwd
    {
        using type = fn_mp_args<H>;
    };

    template <class... H>
    struct fn_mp_args_fwd<std::tuple<H...> &>: fn_mp_args_fwd<std::tuple<H...>> { };

    template <class... H>
    struct fn_mp_args_fwd<std::tuple<H...> const &>: fn_mp_args_fwd<std::tuple<H...>> { };

    template <class... H>
    struct fn_mp_args_fwd<std::tuple<H...>>
    {
        using type = leaf_detail_mp11::mp_append<typename fn_mp_args_fwd<H>::type...>;
    };

    template <class... H>
    struct context_type_from_handlers_impl
    {
        using type = deduce_context<leaf_detail_mp11::mp_append<typename fn_mp_args_fwd<H>::type...>>;
    };
} // namespace detail

template <class... H>
using context_type_from_handlers = typename detail::context_type_from_handlers_impl<H...>::type;

////////////////////////////////////////

template <class...  H>
BOOST_LEAF_CONSTEXPR inline context_type_from_handlers<H...> make_context() noexcept
{
    return { };
}

template <class...  H>
BOOST_LEAF_CONSTEXPR inline context_type_from_handlers<H...> make_context( H && ... ) noexcept
{
    return { };
}

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_CONTEXT_HPP_INCLUDED
