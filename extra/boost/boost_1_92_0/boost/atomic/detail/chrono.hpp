/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/chrono.hpp
 *
 * This header contains \c std::chrono utilities.
 */

#ifndef BOOST_ATOMIC_DETAIL_CHRONO_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_CHRONO_HPP_INCLUDED_

#include <time.h>
#include <chrono>
#if !defined(__cpp_lib_chrono) || (__cpp_lib_chrono < 201510l)
#include <ratio>
#include <type_traits>
#endif // !defined(__cpp_lib_chrono) || (__cpp_lib_chrono < 201510l)
#if defined(CLOCK_REALTIME)
#include <boost/atomic/posix_clock_traits_fwd.hpp>
#endif // defined(CLOCK_REALTIME)
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {
namespace chrono {

#if defined(__cpp_lib_chrono) && (__cpp_lib_chrono >= 201510l)

using std::chrono::ceil;

#else // defined(__cpp_lib_chrono) && (__cpp_lib_chrono >= 201510l)

template< typename To, typename Rep, typename Period >
inline constexpr To ceil(std::chrono::duration< Rep, Period > from) noexcept
{
    using conv_ratio = std::ratio_divide< Period, typename To::period >;
    using common_rep = typename std::common_type< Rep, typename To::rep, decltype(conv_ratio::num) >::type;
    return To(static_cast< typename To::rep >((static_cast< common_rep >(from.count()) * conv_ratio::num) / conv_ratio::den +
        static_cast< common_rep >(((static_cast< common_rep >(from.count()) * conv_ratio::num) % conv_ratio::den) != static_cast< common_rep >(0))));
}

#endif // defined(__cpp_lib_chrono) && (__cpp_lib_chrono >= 201510l)

} // namespace chrono
} // namespace detail

#if defined(CLOCK_REALTIME)

//! Integrate `std::chrono::system_clock` with POSIX clocks
template< >
struct posix_clock_traits< std::chrono::system_clock >
{
    //! POSIX clock identifier
    static constexpr clockid_t clock_id = CLOCK_REALTIME;

    //! Function that converts a time point to a timespec structure
    static timespec to_timespec(std::chrono::system_clock::time_point time_point) noexcept
    {
        timespec ts{};
        std::chrono::nanoseconds::rep time_ns = std::chrono::duration_cast< std::chrono::nanoseconds >(time_point.time_since_epoch()).count();
        // Note: The standard doesn't require that std::chrono::system_clock epoch matches the POSIX CLOCK_REALTIME epoch. Also, std::chrono::system_clock::to_time_t
        //       is allowed to round or truncate the time point when converting to time_t resolution, which means to_time_t may return a time before or after time_point.
        ts.tv_sec = std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point()) + static_cast< decltype(ts.tv_sec) >(time_ns / 1000000000);
        time_ns %= 1000000000;
        if (BOOST_UNLIKELY(time_ns < 0))
        {
            --ts.tv_sec;
            time_ns += 1000000000;
        }
        ts.tv_nsec = static_cast< decltype(ts.tv_nsec) >(time_ns);
        return ts;
    }
};

#endif // defined(CLOCK_REALTIME)

} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_CHRONO_HPP_INCLUDED_
