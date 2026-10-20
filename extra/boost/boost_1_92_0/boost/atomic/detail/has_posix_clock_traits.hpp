/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2025 Andrey Semashev
 */
/*!
 * \file   atomic/has_posix_clock_traits.hpp
 *
 * This header contains utilities for working with \c posix_clock_traits.
 */

#ifndef BOOST_ATOMIC_DETAIL_HAS_POSIX_CLOCK_TRAITS_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_HAS_POSIX_CLOCK_TRAITS_HPP_INCLUDED_

#include <sys/types.h> // clockid_t
#include <type_traits>
#include <boost/atomic/posix_clock_traits_fwd.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

template< typename Clock >
struct has_posix_clock_traits_impl
{
    template< typename T, clockid_t = posix_clock_traits< T >::clock_id >
    static std::true_type check_posix_clock_traits_clock_id(T*);
    static std::false_type check_posix_clock_traits_clock_id(...);

    using type = decltype(has_posix_clock_traits_impl< Clock >::check_posix_clock_traits_clock_id(static_cast< Clock* >(nullptr)));
};

//! Checks if there exists a specialization of \c posix_clock_traits for \c Clock
template< typename Clock >
using has_posix_clock_traits = typename has_posix_clock_traits_impl< Clock >::type;

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_HAS_POSIX_CLOCK_TRAITS_HPP_INCLUDED_
