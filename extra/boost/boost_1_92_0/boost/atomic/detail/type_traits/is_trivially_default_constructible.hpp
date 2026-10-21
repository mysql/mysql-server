/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2018-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/type_traits/is_trivially_default_constructible.hpp
 *
 * This header defines \c is_trivially_default_constructible type trait
 */

#ifndef BOOST_ATOMIC_DETAIL_TYPE_TRAITS_IS_TRIVIALLY_DEFAULT_CONSTRUCTIBLE_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_TYPE_TRAITS_IS_TRIVIALLY_DEFAULT_CONSTRUCTIBLE_HPP_INCLUDED_

#include <type_traits>
#include <boost/atomic/detail/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

#if !defined(BOOST_LIBSTDCXX_VERSION) || (BOOST_LIBSTDCXX_VERSION >= 50100)

using std::is_trivially_default_constructible;

#else // !defined(BOOST_LIBSTDCXX_VERSION) || (BOOST_LIBSTDCXX_VERSION >= 50100)

template< typename T >
struct is_trivially_default_constructible :
    public std::has_trivial_default_constructor< typename std::remove_cv< typename std::remove_all_extents< T >::type >::type >::type
{
};

#endif // !defined(BOOST_LIBSTDCXX_VERSION) || (BOOST_LIBSTDCXX_VERSION >= 50100)

} // namespace detail
} // namespace atomics
} // namespace boost

#endif // BOOST_ATOMIC_DETAIL_TYPE_TRAITS_IS_TRIVIALLY_DEFAULT_CONSTRUCTIBLE_HPP_INCLUDED_
