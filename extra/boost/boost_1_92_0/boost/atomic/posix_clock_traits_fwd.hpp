/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2025 Andrey Semashev
 */
/*!
 * \file   atomic/posix_clock_traits_fwd.hpp
 *
 * This header contains declaration of the \c posix_clock_traits class template.
 */

#ifndef BOOST_ATOMIC_POSIX_CLOCK_TRAITS_FWD_HPP_INCLUDED_
#define BOOST_ATOMIC_POSIX_CLOCK_TRAITS_FWD_HPP_INCLUDED_

#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {

/*!
 * \brief The structure contains traits for compatibility between chrono clocks and POSIX clocks.
 *
 * This class template is meant to be specialized for clock types that are compatible with `std::chrono`
 * requirements and are based on one of the POSIX clocks identified by
 * a [`clockid_t`](https://man7.org/linux/man-pages/man3/clockid_t.3type.html) constant.
 *
 * Every specialization of this class must support the following interface:
 *
 * ```
 * // POSIX clock identifier
 * static constexpr clockid_t clock_id = ...;
 *
 * // Function that converts a time point to a timespec structure
 * static timespec to_timespec(Clock::time_point time_point) noexcept;
 * ```
 *
 * Note that the `timespec` structure returned from `to_timespec` must use the identified POSIX
 * clock epoch and time units. There are no invalid input `time_point` values, so `to_timespec`
 * must never fail with an exception.
 *
 * The second template parameter of this class template may be used by partial specializations
 * to leverage SFINAE to selectively enable it for a set of clock types.
 */
template< typename Clock, typename = void >
struct posix_clock_traits;

} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_POSIX_CLOCK_TRAITS_FWD_HPP_INCLUDED_
