#ifndef BOOST_LEAF_ON_ERROR_HPP_INCLUDED
#define BOOST_LEAF_ON_ERROR_HPP_INCLUDED

// Copyright 2018-2025 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/error.hpp>

namespace boost { namespace leaf {

class error_monitor
{
#if !defined(BOOST_LEAF_NO_EXCEPTIONS) && BOOST_LEAF_STD_UNCAUGHT_EXCEPTIONS
    int const uncaught_exceptions_;
#endif
    int const err_id_;

public:

    error_monitor() noexcept:
#if !defined(BOOST_LEAF_NO_EXCEPTIONS) && BOOST_LEAF_STD_UNCAUGHT_EXCEPTIONS
        uncaught_exceptions_(std::uncaught_exceptions()),
#endif
        err_id_(detail::current_id())
    {
    }

    int check_id() const noexcept
    {
        int err_id = detail::current_id();
        if( err_id != err_id_ )
            return err_id;
        else
        {
#ifndef BOOST_LEAF_NO_EXCEPTIONS
#   if BOOST_LEAF_STD_UNCAUGHT_EXCEPTIONS
            if( std::uncaught_exceptions() > uncaught_exceptions_ )
#   else
            if( std::uncaught_exception() )
#   endif
                return detail::new_id();
#endif
            return 0;
        }
    }

    int get_id() const noexcept
    {
        int err_id = detail::current_id();
        if( err_id != err_id_ )
            return err_id;
        else
            return detail::new_id();
    }

    error_id check() const noexcept
    {
        return detail::make_error_id(check_id());
    }

    error_id assigned_error_id() const noexcept
    {
        return detail::make_error_id(get_id());
    }
}; // class error_monitor

////////////////////////////////////////

namespace detail
{
    template <int I, class Tup>
    struct tuple_for_each_preload
    {
        BOOST_LEAF_CONSTEXPR static void trigger( Tup & tup, int err_id ) noexcept
        {
            BOOST_LEAF_ASSERT((err_id&3) == 1);
            tuple_for_each_preload<I-1,Tup>::trigger(tup,err_id);
            std::get<I-1>(tup).trigger(err_id);
        }

#if BOOST_LEAF_CFG_CAPTURE
        static void reserve( Tup const & tup, dynamic_allocator & da )
        {
            tuple_for_each_preload<I-1,Tup>::reserve(tup,da);
            std::get<I-1>(tup).reserve(da);
        }
#endif
    };

    template <class Tup>
    struct tuple_for_each_preload<0, Tup>
    {
        BOOST_LEAF_CONSTEXPR static void trigger( Tup const &, int ) noexcept { }

#if BOOST_LEAF_CFG_CAPTURE
        static void reserve( Tup const &, dynamic_allocator & ) { }
#endif
    };

    template <class E>
    class preloaded_item
    {
        using E_decayed = typename std::decay<E>::type;
        E_decayed e_;

    public:

        BOOST_LEAF_CONSTEXPR preloaded_item( E && e ):
            e_(std::forward<E>(e))
        {
        }

        BOOST_LEAF_CONSTEXPR void trigger( int err_id ) noexcept
        {
            if( slot<E_decayed> * p = tls::read_ptr<slot<E_decayed>>() )
                if( !p->has_value(err_id) )
                    (void) p->load(err_id, std::move(e_));
        }

#if BOOST_LEAF_CFG_CAPTURE
        void reserve( dynamic_allocator & da ) const
        {
            da.reserve<E_decayed>();
        }
#endif
    };

    template <class F, class ReturnType = typename function_traits<F>::return_type>
    class deferred_item
    {
        using E_decayed = typename std::decay<ReturnType>::type;
        F f_;

    public:

        BOOST_LEAF_CONSTEXPR deferred_item( F && f ):
            f_(std::forward<F>(f))
        {
        }

        void trigger( int err_id ) noexcept
        {
            if( slot<E_decayed> * p = tls::read_ptr<slot<E_decayed>>() )
                if( !p->has_value(err_id) )
                {
#ifndef BOOST_LEAF_NO_EXCEPTIONS
                    try
                    {
#endif
                        (void) p->load(err_id, f_());
#ifndef BOOST_LEAF_NO_EXCEPTIONS
                    }
                    catch(...)
                    {
                    }
#endif
                }
        }

#if BOOST_LEAF_CFG_CAPTURE
        void reserve( dynamic_allocator & da ) const
        {
            da.reserve<E_decayed>();
        }
#endif
    };

    template <class F>
    class deferred_item<F, void>
    {
        F f_;

    public:

        BOOST_LEAF_CONSTEXPR deferred_item( F && f ) noexcept:
            f_(std::forward<F>(f))
        {
        }

        void trigger( int ) noexcept
        {
#ifndef BOOST_LEAF_NO_EXCEPTIONS
            try
            {
#endif
                f_();
#ifndef BOOST_LEAF_NO_EXCEPTIONS
            }
            catch(...)
            {
            }
#endif
        }

#if BOOST_LEAF_CFG_CAPTURE
        void reserve( dynamic_allocator & ) const
        {
        }
#endif
    };

    template <class F, class A0 = fn_arg_type<F,0>, int arity = function_traits<F>::arity>
    class accumulating_item;

    template <class F, class A0>
    class accumulating_item<F, A0 &, 1>
    {
        using E_decayed = typename std::decay<A0>::type;
        F f_;

    public:

        BOOST_LEAF_CONSTEXPR accumulating_item( F && f ):
            f_(std::forward<F>(f))
        {
        }

        void trigger( int err_id ) noexcept
        {
            if( slot<E_decayed> * p = tls::read_ptr<slot<E_decayed>>() )
            {
#ifndef BOOST_LEAF_NO_EXCEPTIONS
                try
                {
#endif
                    if( E_decayed * v = p->has_value(err_id) )
                        (void) std::move(f_)(*v);
                    else
                        (void) std::move(f_)(p->load(err_id, E_decayed()));
#ifndef BOOST_LEAF_NO_EXCEPTIONS
                }
                catch(...)
                {
                }
#endif
            }
        }

#if BOOST_LEAF_CFG_CAPTURE
        void reserve( dynamic_allocator & da ) const
        {
            da.reserve<E_decayed>();
        }
#endif
    };

    template <class... Item>
    class preloaded
    {
        preloaded( preloaded const & ) = delete;
        preloaded & operator=( preloaded const & ) = delete;

        std::tuple<Item...> p_;
        error_monitor id_;
#if __cplusplus < 201703L
        bool moved_ = false;
#endif

    public:

        BOOST_LEAF_CONSTEXPR explicit preloaded( Item && ... i ):
            p_(std::forward<Item>(i)...)
        {
        }

#if __cplusplus < 201703L
        BOOST_LEAF_CONSTEXPR preloaded( preloaded && x ) noexcept:
            p_(std::move(x.p_)),
            id_(std::move(x.id_))
        {
            x.moved_ = true;
        }
#endif

        ~preloaded() noexcept
        {
#if __cplusplus < 201703L
            if( moved_ )
                return;
#endif
            if( int const err_id = id_.check_id() )
            {
#if BOOST_LEAF_CFG_CAPTURE
                if( dynamic_allocator * da = get_dynamic_allocator() )
#   ifndef BOOST_LEAF_NO_EXCEPTIONS
                    try
                    {
#   endif
                        tuple_for_each_preload<sizeof...(Item),decltype(p_)>::reserve(p_, *da);
#   ifndef BOOST_LEAF_NO_EXCEPTIONS
                    }
                    catch(...)
                    {
                    }
#   endif
#endif
                tuple_for_each_preload<sizeof...(Item),decltype(p_)>::trigger(p_, err_id);
            }
        }
    };

    template <class T, int arity = function_traits<T>::arity>
    struct deduce_item_type;

    template <class T>
    struct deduce_item_type<T, -1>
    {
        using type = preloaded_item<T>;
    };

    template <class F>
    struct deduce_item_type<F, 0>
    {
        using type = deferred_item<F>;
    };

    template <class F>
    struct deduce_item_type<F, 1>
    {
        using type = accumulating_item<F>;
    };
} // namespace detail

template <class... Item>
BOOST_LEAF_ATTRIBUTE_NODISCARD BOOST_LEAF_CONSTEXPR inline
detail::preloaded<typename detail::deduce_item_type<Item>::type...>
on_error( Item && ... i )
{
    return detail::preloaded<typename detail::deduce_item_type<Item>::type...>(std::forward<Item>(i)...);
}

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_ON_ERROR_HPP_INCLUDED
