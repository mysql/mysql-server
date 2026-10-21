/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2018-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/type_traits/is_trivially_copyable.hpp
 *
 * This header defines \c is_trivially_copyable type trait
 */

#ifndef BOOST_ATOMIC_DETAIL_TYPE_TRAITS_IS_TRIVIALLY_COPYABLE_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_TYPE_TRAITS_IS_TRIVIALLY_COPYABLE_HPP_INCLUDED_

#include <type_traits>
#include <boost/atomic/detail/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

#if !defined(BOOST_LIBSTDCXX_VERSION) || (BOOST_LIBSTDCXX_VERSION >= 50100)

using std::is_trivially_copyable;

#else // !defined(BOOST_LIBSTDCXX_VERSION) || (BOOST_LIBSTDCXX_VERSION >= 50100)

template< typename T >
using is_trivially_copyable_impl = std::integral_constant<
    bool,
    std::is_trivially_destructible< T >::value && std::has_trivial_copy_constructor< T >::value && std::has_trivial_copy_assign< T >::value
>;

template< typename T >
struct is_trivially_copyable :
    public is_trivially_copyable_impl< typename std::remove_cv< typename std::remove_all_extents< T >::type >::type >
{
};

#endif // !defined(BOOST_LIBSTDCXX_VERSION) || (BOOST_LIBSTDCXX_VERSION >= 50100)

} // namespace detail
} // namespace atomics
} // namespace boost

#endif // BOOST_ATOMIC_DETAIL_TYPE_TRAITS_IS_TRIVIALLY_COPYABLE_HPP_INCLUDED_
