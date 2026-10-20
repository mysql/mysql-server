#ifndef BOOST_DESCRIBE_ENUM_TO_STRING_HPP_INCLUDED
#define BOOST_DESCRIBE_ENUM_TO_STRING_HPP_INCLUDED

// Copyright 2020, 2021, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/detail/config.hpp>

#if defined(BOOST_DESCRIBE_CXX14)

#include <boost/describe/enumerators.hpp>
#include <boost/mp11/algorithm.hpp>

#if defined(_MSC_VER) && _MSC_VER == 1900
# pragma warning(push)
# pragma warning(disable: 4100) // unreferenced formal parameter
#endif

namespace boost
{
namespace describe
{

namespace detail
{

// [&](auto D){ if( e == D.value ) r = D.name; }
// except constexpr under C++14

template<class E> struct ets_lambda
{
    E e;
    char const** pr;

    template<class D> constexpr void operator()( D d ) const noexcept
    {
        if( e == d.value ) *pr = d.name;
    }
};

} // namespace detail

template<class E, class De = describe_enumerators<E>>
#if !( defined(_MSC_VER) && _MSC_VER == 1900 )
constexpr
#endif
char const* enum_to_string( E e, char const* def ) noexcept
{
    char const* r = def;

    mp11::mp_for_each<De>( detail::ets_lambda<E>{ e, &r } );

    return r;
}

} // namespace describe
} // namespace boost

#if defined(_MSC_VER) && _MSC_VER == 1900
# pragma warning(pop)
#endif

#endif // defined(BOOST_DESCRIBE_CXX14)

#endif // #ifndef BOOST_DESCRIBE_ENUM_TO_STRING_HPP_INCLUDED
