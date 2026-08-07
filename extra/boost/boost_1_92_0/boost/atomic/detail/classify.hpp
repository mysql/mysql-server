/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/classify.hpp
 *
 * This header contains type traits for type classification.
 */

#ifndef BOOST_ATOMIC_DETAIL_CLASSIFY_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_CLASSIFY_HPP_INCLUDED_

#include <type_traits>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/type_traits/is_integral.hpp>
#include <boost/atomic/detail/type_traits/is_floating_point.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

template< typename T, bool IsFunction = std::is_function< T >::value >
struct classify_pointer
{
    using type = void*;
};

template< typename T >
struct classify_pointer< T, true >
{
    using type = void;
};

template<
    typename T,
    bool IsInt = atomics::detail::is_integral< T >::value,
    bool IsFloat = atomics::detail::is_floating_point< T >::value,
    bool IsEnum = std::is_enum< T >::value
>
struct classify
{
    using type = void;
};

template< typename T >
struct classify< T, true, false, false > { using type = int; };

#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)
template< typename T >
struct classify< T, false, true, false > { using type = float; };
#endif

template< typename T >
struct classify< T, false, false, true > { using type = const int; };

template< typename T >
struct classify< T*, false, false, false > { using type = typename classify_pointer< T >::type; };

template< >
struct classify< void*, false, false, false > { using type = void; };

template< >
struct classify< const void*, false, false, false > { using type = void; };

template< >
struct classify< volatile void*, false, false, false > { using type = void; };

template< >
struct classify< const volatile void*, false, false, false > { using type = void; };

template< typename T, typename U >
struct classify< T U::*, false, false, false > { using type = void; };

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_CLASSIFY_HPP_INCLUDED_
