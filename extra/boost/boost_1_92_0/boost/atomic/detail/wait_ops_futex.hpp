/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/wait_ops_futex.hpp
 *
 * This header contains implementation of the waiting/notifying atomic operations based on futexes.
 */

#ifndef BOOST_ATOMIC_DETAIL_WAIT_OPS_FUTEX_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_WAIT_OPS_FUTEX_HPP_INCLUDED_

#include <unistd.h> // _POSIX_MONOTONIC_CLOCK
#include <time.h>
#include <cstdint>
#include <cerrno>
#include <limits>
#include <chrono>
#include <type_traits>
#include <boost/memory_order.hpp>
#include <boost/atomic/posix_clock_traits_fwd.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/chrono.hpp>
#include <boost/atomic/detail/futex.hpp>
#include <boost/atomic/detail/has_posix_clock_traits.hpp>
#include <boost/atomic/detail/wait_operations_fwd.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

#if defined(BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET)

struct futex_wait_fallback {};

//! The type trait selects futex timed wait implementation tag
template< typename Clock, bool = has_posix_clock_traits< Clock >::value >
struct select_futex_wait
{
    using type = futex_wait_fallback;
};

template< clockid_t ClockId >
struct futex_wait_clock;

template< clockid_t ClockId >
struct select_futex_wait_impl
{
    using type = futex_wait_fallback;
};

#if defined(_POSIX_MONOTONIC_CLOCK) && (_POSIX_MONOTONIC_CLOCK >= 0)
template< >
struct futex_wait_clock< CLOCK_MONOTONIC >
{
    static constexpr int futex_flags = 0;
};

template< >
struct select_futex_wait_impl< CLOCK_MONOTONIC >
{
    using type = futex_wait_clock< CLOCK_MONOTONIC >;
};
#endif // defined(_POSIX_MONOTONIC_CLOCK) && (_POSIX_MONOTONIC_CLOCK >= 0)

#if defined(BOOST_ATOMIC_DETAIL_FUTEX_CLOCK_REALTIME)
template< >
struct futex_wait_clock< CLOCK_REALTIME >
{
    static constexpr int futex_flags = BOOST_ATOMIC_DETAIL_FUTEX_CLOCK_REALTIME;
};

template< >
struct select_futex_wait_impl< CLOCK_REALTIME >
{
    using type = futex_wait_clock< CLOCK_REALTIME >;
};
#endif // defined(BOOST_ATOMIC_DETAIL_FUTEX_CLOCK_REALTIME)

template< typename Clock >
struct select_futex_wait< Clock, true >
{
    using type = typename select_futex_wait_impl< posix_clock_traits< Clock >::clock_id >::type;
};

#endif // defined(BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET)

template< typename Base, bool Interprocess >
struct wait_operations< Base, 4u, true, Interprocess > :
    public Base
{
    using base_type = Base;
    using storage_type = typename base_type::storage_type;

    static constexpr bool always_has_native_wait_notify = true;

private:
    static constexpr int futex_private_flag = Interprocess ? 0 : BOOST_ATOMIC_DETAIL_FUTEX_PRIVATE_FLAG;

public:
    static BOOST_FORCEINLINE bool has_native_wait_notify(storage_type const volatile&) noexcept
    {
        return true;
    }

    static BOOST_FORCEINLINE storage_type wait(storage_type const volatile& storage, storage_type old_val, memory_order order) noexcept
    {
        storage_type new_val = base_type::load(storage, order);
        while (new_val == old_val)
        {
            atomics::detail::futex_wait(const_cast< storage_type* >(&storage), old_val, futex_private_flag);
            new_val = base_type::load(storage, order);
        }

        return new_val;
    }

private:
    template< typename Clock >
    static BOOST_FORCEINLINE storage_type wait_until_fallback
    (
        storage_type const volatile& storage,
        storage_type old_val,
        typename Clock::time_point timeout,
        typename Clock::time_point now,
        memory_order order,
        bool& timed_out
    ) noexcept(noexcept(Clock::now()))
    {
        futex_timespec ts{};
        storage_type new_val = base_type::load(storage, order);
        while (new_val == old_val)
        {
            const std::int64_t nsec = atomics::detail::chrono::ceil< std::chrono::nanoseconds >(timeout - now).count();
            if (nsec <= 0)
            {
                timed_out = true;
                break;
            }

            const std::int64_t sec = nsec / 1000000000;
            if (BOOST_LIKELY(sec <= (std::numeric_limits< decltype(ts.tv_sec) >::max)()))
            {
                ts.tv_sec = static_cast< decltype(ts.tv_sec) >(sec);
                ts.tv_nsec = static_cast< decltype(ts.tv_nsec) >(nsec % 1000000000);
            }
            else
            {
                ts.tv_sec = (std::numeric_limits< decltype(ts.tv_sec) >::max)();
                ts.tv_nsec = static_cast< decltype(ts.tv_nsec) >(999999999);
            }

            atomics::detail::futex_wait_for(const_cast< storage_type* >(&storage), old_val, ts, futex_private_flag);

            now = Clock::now();
            new_val = base_type::load(storage, order);
        }

        return new_val;
    }

#if defined(BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET)

    template< typename Clock, clockid_t ClockId >
    static BOOST_FORCEINLINE storage_type wait_until_abs_timeout
    (
        storage_type const volatile& storage,
        storage_type old_val,
        typename Clock::time_point timeout,
        memory_order order,
        bool& timed_out
    ) noexcept(noexcept(Clock::now()))
    {
        futex_timespec ts(posix_clock_traits< Clock >::to_timespec(timeout));
        storage_type new_val = base_type::load(storage, order);
        if (BOOST_LIKELY(ts.tv_sec >= 0))
        {
            while (new_val == old_val)
            {
                int err = atomics::detail::futex_wait_until
                (
                    const_cast< storage_type* >(&storage),
                    old_val,
                    ts,
                    futex_private_flag | futex_wait_clock< ClockId >::futex_flags
                );
                if (err < 0)
                {
                    err = errno;
                    if (err == ETIMEDOUT)
                    {
                        new_val = base_type::load(storage, order);
                        timed_out = new_val == old_val;
                        break;
                    }

                    if (BOOST_UNLIKELY(err == ENOSYS))
                        return wait_until_fallback< Clock >(storage, old_val, timeout, Clock::now(), order, timed_out);
                }

                new_val = base_type::load(storage, order);
            }
        }
        else
        {
            timed_out = new_val == old_val;
        }

        return new_val;
    }

    template< typename Clock >
    static BOOST_FORCEINLINE storage_type wait_until_dispatch
    (
        storage_type const volatile& storage,
        storage_type old_val,
        typename Clock::time_point timeout,
        memory_order order,
        bool& timed_out,
        futex_wait_fallback
    ) noexcept(noexcept(Clock::now()))
    {
        return wait_until_fallback< Clock >(storage, old_val, timeout, Clock::now(), order, timed_out);
    }

    template< typename Clock, clockid_t ClockId >
    static BOOST_FORCEINLINE storage_type wait_until_dispatch
    (
        storage_type const volatile& storage,
        storage_type old_val,
        typename Clock::time_point timeout,
        memory_order order,
        bool& timed_out,
        futex_wait_clock< ClockId >
    ) noexcept(noexcept(Clock::now()))
    {
        return wait_until_abs_timeout< Clock, ClockId >(storage, old_val, timeout, order, timed_out);
    }

public:
    template< typename Clock, typename Duration >
    static BOOST_FORCEINLINE storage_type wait_until
    (
        storage_type const volatile& storage,
        storage_type old_val,
        std::chrono::time_point< Clock, Duration > timeout,
        memory_order order,
        bool& timed_out
    ) noexcept(noexcept(wait_until_dispatch< Clock >(storage, old_val, timeout, order, timed_out, typename select_futex_wait< Clock >::type())))
    {
        return wait_until_dispatch< Clock >(storage, old_val, timeout, order, timed_out, typename select_futex_wait< Clock >::type());
    }

#else // defined(BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET)

public:
    template< typename Clock, typename Duration >
    static BOOST_FORCEINLINE storage_type wait_until
    (
        storage_type const volatile& storage,
        storage_type old_val,
        std::chrono::time_point< Clock, Duration > timeout,
        memory_order order,
        bool& timed_out
    ) noexcept(noexcept(wait_until_fallback< Clock >(storage, old_val, timeout, Clock::now(), order, timed_out)))
    {
        return wait_until_fallback< Clock >(storage, old_val, timeout, Clock::now(), order, timed_out);
    }

#endif // defined(BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET)

    template< typename Rep, typename Period >
    static BOOST_FORCEINLINE storage_type wait_for
    (
        storage_type const volatile& storage,
        storage_type old_val,
        std::chrono::duration< Rep, Period > timeout,
        memory_order order,
        bool& timed_out
    ) noexcept
    {
#if defined(BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET) && defined(_POSIX_MONOTONIC_CLOCK) && (_POSIX_MONOTONIC_CLOCK >= 0)
        if (BOOST_LIKELY(timeout.count() >= 0))
        {
            timespec now{};
            if (BOOST_LIKELY(clock_gettime(CLOCK_MONOTONIC, &now) == 0))
            {
                const std::int64_t nsec = static_cast< std::int64_t >(now.tv_nsec) + atomics::detail::chrono::ceil< std::chrono::nanoseconds >(timeout).count();
                const std::int64_t sec = static_cast< std::int64_t >(now.tv_sec) + nsec / 1000000000;
                if (BOOST_LIKELY(sec <= (std::numeric_limits< decltype(futex_timespec::tv_sec) >::max)()))
                {
                    futex_timespec ts{};
                    ts.tv_sec = static_cast< decltype(ts.tv_sec) >(sec);
                    ts.tv_nsec = static_cast< decltype(ts.tv_nsec) >(nsec % 1000000000);

                    storage_type new_val = base_type::load(storage, order);
                    while (new_val == old_val)
                    {
                        int err = atomics::detail::futex_wait_until(const_cast< storage_type* >(&storage), old_val, ts, futex_private_flag);
                        if (err < 0)
                        {
                            err = errno;
                            if (err == ETIMEDOUT)
                            {
                                new_val = base_type::load(storage, order);
                                timed_out = new_val == old_val;
                                break;
                            }

                            if (BOOST_UNLIKELY(err == ENOSYS))
                                goto use_wait_until_fallback;
                        }

                        new_val = base_type::load(storage, order);
                    }

                    return new_val;
                }
            }
        }
    use_wait_until_fallback:
#endif // defined(BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET) && defined(_POSIX_MONOTONIC_CLOCK) && (_POSIX_MONOTONIC_CLOCK >= 0)

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        return wait_until_fallback< std::chrono::steady_clock >(storage, old_val, now + timeout, now, order, timed_out);
    }

    static BOOST_FORCEINLINE void notify_one(storage_type volatile& storage) noexcept
    {
        atomics::detail::futex_signal(const_cast< storage_type* >(&storage), futex_private_flag);
    }

    static BOOST_FORCEINLINE void notify_all(storage_type volatile& storage) noexcept
    {
        atomics::detail::futex_broadcast(const_cast< storage_type* >(&storage), futex_private_flag);
    }
};

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_WAIT_OPS_FUTEX_HPP_INCLUDED_
