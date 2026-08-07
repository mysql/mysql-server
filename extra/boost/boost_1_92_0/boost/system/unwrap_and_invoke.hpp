#ifndef BOOST_SYSTEM_UNWRAP_AND_INVOKE_HPP_INCLUDED
#define BOOST_SYSTEM_UNWRAP_AND_INVOKE_HPP_INCLUDED

// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/system/detail/is_aggregate.hpp>
#include <boost/compat/type_traits.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/utility.hpp>
#include <type_traits>
#include <utility>

namespace boost
{
namespace system
{

// unwrap_and_invoke

namespace detail
{

// get_error_type

template<class... T> using first_if_same =
    mp11::mp_if<mp11::mp_same<T...>, mp11::mp_first<mp11::mp_list<T...>>>;

template<class... A> using get_error_type =
    mp11::mp_apply<first_if_same,
        mp11::mp_transform<mp11::mp_second,
            mp11::mp_copy_if<mp11::mp_list<A...>, is_result>
        >
    >;

// invoke_unwrap

template< class T, class En = typename std::enable_if< !is_result< compat::remove_cvref_t<T> >::value >::type >
auto invoke_unwrap( T&& t ) noexcept -> T&&
{
    return std::forward<T>( t );
}

template< class T, class = void, class En = typename std::enable_if< is_result< compat::remove_cvref_t<T> >::value >::type >
auto invoke_unwrap( T&& t ) noexcept -> decltype( std::forward<T>( t ).unsafe_value() )
{
    return std::forward<T>( t ).unsafe_value();
}

// invoke_test

template<class R, class A> int invoke_test( R&, A const& )
{
    return 0;
}

template<class R, class T, class E> int invoke_test( R& r, result<T, E> const& r2 )
{
    if( r && r2.has_error() ) r = r2.error();
    return 0;
}

} // namespace detail

template<class F, class... A,
    class R = decltype( compat::invoke( std::declval<F>(), detail::invoke_unwrap( std::declval<A>() )... ) ),
    class E = detail::get_error_type<compat::remove_cvref_t<A>...>,
    class En = typename std::enable_if< !std::is_void<R>::value >::type
>
auto unwrap_and_invoke( F&& f, A&&... a ) -> result<R, E>
{
    {
        result<void, E> r;

        using Q = int[];
        (void)Q{ detail::invoke_test( r, a )... };

        if( !r ) return r.error();
    }

    return compat::invoke( std::forward<F>(f), detail::invoke_unwrap( std::forward<A>(a) )... );
}

template<class F, class... A,
    class R = decltype( compat::invoke( std::declval<F>(), detail::invoke_unwrap( std::declval<A>() )... ) ),
    class E = detail::get_error_type<compat::remove_cvref_t<A>...>,
    class En1 = void,
    class En2 = typename std::enable_if< std::is_void<R>::value >::type
>
auto unwrap_and_invoke( F&& f, A&&... a ) -> result<R, E>
{
    {
        result<void, E> r;

        using Q = int[];
        (void)Q{ detail::invoke_test( r, a )... };

        if( !r ) return r.error();
    }

    compat::invoke( std::forward<F>(f), detail::invoke_unwrap( std::forward<A>(a) )... );

    return {};
}

// unwrap_and_construct

namespace detail
{

template<class T> struct construct
{
private:

    template<class... A> static inline T call_impl( std::false_type, A&&... a )
    {
        return T( std::forward<A>(a)... );
    }

    template<class... A> static inline T call_impl( std::true_type, A&&... a )
    {
        return T{ std::forward<A>(a)... };
    }

public:

    template<class... A> inline T operator()( A&&... a ) const
    {
        return this->call_impl( detail::is_aggregate<T>(), std::forward<A>(a)... );
    }
};

} // namespace detail

template<class T, class... A>
auto unwrap_and_construct( A&&... a )
-> decltype( unwrap_and_invoke( detail::construct<T>(), std::forward<A>(a)... ) )
{
    return unwrap_and_invoke( detail::construct<T>(), std::forward<A>(a)... );
}

} // namespace system
} // namespace boost

#endif // #ifndef BOOST_SYSTEM_UNWRAP_AND_INVOKE_HPP_INCLUDED
