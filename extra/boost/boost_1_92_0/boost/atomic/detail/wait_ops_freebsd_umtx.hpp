/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/wait_ops_freebsd_umtx.hpp
 *
 * This header contains implementation of the waiting/notifying atomic operations based on FreeBSD _umtx_op syscall.
 * https://man.freebsd.org/cgi/man.cgi?query=_umtx_op&apropos=0&sektion=2&manpath=FreeBSD+11.0-RELEASE&arch=default&format=html
 */

#ifndef BOOST_ATOMIC_DETAIL_WAIT_OPS_FREEBSD_UMTX_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_WAIT_OPS_FREEBSD_UMTX_HPP_INCLUDED_

#include <sys/umtx.h>
#include <time.h>
#include <cstdint>
#include <cerrno>
#include <limits>
#include <chrono>
#include <type_traits>
#include <boost/memory_order.hpp>
#include <boost/atomic/posix_clock_traits_fwd.hpp>
#include <boost/atomic/detail/config.hpp>
#if defined(UMTX_ABSTIME)
#include <boost/atomic/detail/intptr.hpp>
#endif
#include <boost/atomic/detail/chrono.hpp>
#include <boost/atomic/detail/int_sizes.hpp>
#include <boost/atomic/detail/has_posix_clock_traits.hpp>
#include <boost/atomic/detail/wait_operations_fwd.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

// Brief evolution of _umtx_op in FreeBSD:
//
// * FreeBSD 6.0.
//   Initial version that supports UMTX_OP_WAIT and UMTX_OP_WAKE for long-sized futexes. Supports timed waits, where initially the timeout was absolute
//   against CLOCK_REALTIME (https://github.com/freebsd/freebsd-src/commit/cc1000ac5b235516dd312340fca34e8847add3d0), but later was changed to relative
//   timeouts that count against CLOCK_MONOTONIC (https://github.com/freebsd/freebsd-src/commit/b7be40d612b794ea9165b71a8afe07777dc9d31a). Presumably,
//   the initial version with absolute timeouts was not released, so we can ignore it.
// * FreeBSD 8.0.
//   Added UMTX_OP_WAIT_UINT (https://github.com/freebsd/freebsd-src/commit/110de0cf17923839e434ead831b7a7c74a7ce102) for int-sized futexes, as well as
//   UMTX_OP_WAIT_UINT_PRIVATE and UMTX_OP_WAKE_PRIVATE (https://github.com/freebsd/freebsd-src/commit/727158f6f64df04094d41ca5ee4b0641308c39d0) for
//   process-local futexes.
// * FreeBSD 10.0.
//   Added UMTX_ABSTIME (https://github.com/freebsd/freebsd-src/commit/df1f1bae9eac5f3f838c8939e4de2c5458aba001). By default, relative timeouts are now
//   counted against CLOCK_REALTIME (a breaking change), but the caller can now supply the timeout in the form of struct _umtx_time, which allows for
//   specifying flags (where UMTX_ABSTIME means absolute timeout) and the clock id. Presumably, all clocks supported by clock_gettime are supported
//   by this new API. Whether the caller is using this new API or the legacy API with a relative timeout in struct timespec is indicated by the uaddr
//   argument of _umtx_op, which must be the size of the timeout struct casted to a pointer. Previous FreeBSD releases ignored this argument, so
//   new binaries running on old FreeBSD versions will silently misbehave (and old binaries on new FreeBSD as well, due to the CLOCK_REALTIME change).

#if defined(UMTX_OP_WAIT_UINT) || defined(UMTX_OP_WAIT)

template< typename Base, typename UmtxOps >
struct wait_operations_freebsd_umtx_common :
    public Base
{
    using base_type = Base;
    using storage_type = typename base_type::storage_type;

    static constexpr bool always_has_native_wait_notify = true;

private:
    using umtx_ops = UmtxOps;

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
            _umtx_op(const_cast< storage_type* >(&storage), umtx_ops::wait_op, old_val, nullptr, nullptr);
            new_val = base_type::load(storage, order);
        }

        return new_val;
    }

#if defined(UMTX_ABSTIME)

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
        _umtx_time umt{};
        umt._clockid = CLOCK_MONOTONIC;
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
            if (BOOST_LIKELY(sec <= (std::numeric_limits< decltype(umt._timeout.tv_sec) >::max)()))
            {
                umt._timeout.tv_sec = static_cast< decltype(umt._timeout.tv_sec) >(sec);
                umt._timeout.tv_nsec = static_cast< decltype(umt._timeout.tv_nsec) >(nsec % 1000000000);
            }
            else
            {
                umt._timeout.tv_sec = (std::numeric_limits< decltype(umt._timeout.tv_sec) >::max)();
                umt._timeout.tv_nsec = static_cast< decltype(umt._timeout.tv_nsec) >(999999999);
            }

            _umtx_op
            (
                const_cast< storage_type* >(&storage),
                umtx_ops::wait_op,
                old_val,
                reinterpret_cast< void* >(static_cast< uintptr_t >(sizeof(umt))),
                &umt
            );

            now = Clock::now();
            new_val = base_type::load(storage, order);
        }

        return new_val;
    }

    static BOOST_FORCEINLINE storage_type wait_until_abs_timeout
    (
        storage_type const volatile& storage,
        storage_type old_val,
        _umtx_time const& umt,
        memory_order order,
        bool& timed_out
    ) noexcept
    {
        storage_type new_val = base_type::load(storage, order);
        while (new_val == old_val)
        {
            int err = _umtx_op
            (
                const_cast< storage_type* >(&storage),
                umtx_ops::wait_op,
                old_val,
                reinterpret_cast< void* >(static_cast< uintptr_t >(sizeof(umt))),
                const_cast< _umtx_time* >(&umt)
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
            }

            new_val = base_type::load(storage, order);
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
        std::false_type
    ) noexcept(noexcept(Clock::now()))
    {
        return wait_until_fallback< Clock >(storage, old_val, timeout, Clock::now(), order, timed_out);
    }

    template< typename Clock >
    static BOOST_FORCEINLINE storage_type wait_until_dispatch
    (
        storage_type const volatile& storage,
        storage_type old_val,
        typename Clock::time_point timeout,
        memory_order order,
        bool& timed_out,
        std::true_type
    ) noexcept
    {
        _umtx_time umt{};
        umt._timeout = posix_clock_traits< Clock >::to_timespec(timeout);
        if (BOOST_LIKELY(umt._timeout.tv_sec >= 0))
        {
            umt._flags = UMTX_ABSTIME;
            umt._clockid = posix_clock_traits< Clock >::clock_id;
            return wait_until_abs_timeout(storage, old_val, umt, order, timed_out);
        }
        else
        {
            storage_type new_val = base_type::load(storage, order);
            timed_out = new_val == old_val;
            return new_val;
        }
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
    ) noexcept(noexcept(wait_until_dispatch< Clock >(
        storage, old_val, timeout, order, timed_out, std::integral_constant< bool, has_posix_clock_traits< Clock >::value >())))
    {
        return wait_until_dispatch< Clock >(storage, old_val, timeout, order, timed_out, std::integral_constant< bool, has_posix_clock_traits< Clock >::value >());
    }

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
        if (BOOST_LIKELY(timeout.count() >= 0))
        {
            _umtx_time umt{};
            if (BOOST_LIKELY(clock_gettime(CLOCK_MONOTONIC, &umt._timeout) == 0))
            {
                const std::int64_t nsec = static_cast< std::int64_t >(umt._timeout.tv_nsec) + atomics::detail::chrono::ceil< std::chrono::nanoseconds >(timeout).count();
                const std::int64_t sec = static_cast< std::int64_t >(umt._timeout.tv_sec) + nsec / 1000000000;
                if (BOOST_LIKELY(sec <= (std::numeric_limits< decltype(timespec::tv_sec) >::max)()))
                {
                    umt._timeout.tv_sec = static_cast< decltype(umt._timeout.tv_sec) >(sec);
                    umt._timeout.tv_nsec = static_cast< decltype(umt._timeout.tv_nsec) >(nsec % 1000000000);
                    umt._flags = UMTX_ABSTIME;
                    umt._clockid = CLOCK_MONOTONIC;
                    return wait_until_abs_timeout(storage, old_val, umt, order, timed_out);
                }
            }
        }

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        return wait_until_fallback< std::chrono::steady_clock >(storage, old_val, now + timeout, now, order, timed_out);
    }

#else // defined(UMTX_ABSTIME)

private:
    template< typename Clock >
    static BOOST_FORCEINLINE storage_type wait_until_impl
    (
        storage_type const volatile& storage,
        storage_type old_val,
        typename Clock::time_point timeout,
        typename Clock::time_point now,
        memory_order order,
        bool& timed_out
    ) noexcept(noexcept(Clock::now()))
    {
        timespec ts{};
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

            _umtx_op(const_cast< storage_type* >(&storage), umtx_ops::wait_op, old_val, nullptr, &ts);

            now = Clock::now();
            new_val = base_type::load(storage, order);
        }

        return new_val;
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
    ) noexcept(noexcept(Clock::now()))
    {
        return wait_until_impl< Clock >(storage, old_val, timeout, Clock::now(), order, timed_out);
    }

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
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        return wait_until_impl< std::chrono::steady_clock >(storage, old_val, now + timeout, now, order, timed_out);
    }

#endif // defined(UMTX_ABSTIME)

    static BOOST_FORCEINLINE void notify_one(storage_type volatile& storage) noexcept
    {
        _umtx_op(const_cast< storage_type* >(&storage), umtx_ops::wake_op, 1u, nullptr, nullptr);
    }

    static BOOST_FORCEINLINE void notify_all(storage_type volatile& storage) noexcept
    {
        _umtx_op(const_cast< storage_type* >(&storage), umtx_ops::wake_op, (~static_cast< unsigned int >(0u)) >> 1u, nullptr, nullptr);
    }
};

#if defined(UMTX_OP_WAIT_UINT)

template< bool Interprocess >
struct uint_umtx_ops
{
#if defined(UMTX_OP_WAIT_UINT_PRIVATE) && defined(UMTX_OP_WAKE_PRIVATE)
    static constexpr int wait_op = Interprocess ? UMTX_OP_WAIT_UINT : UMTX_OP_WAIT_UINT_PRIVATE;
    static constexpr int wake_op = Interprocess ? UMTX_OP_WAKE : UMTX_OP_WAKE_PRIVATE;
#else
    static constexpr int wait_op = UMTX_OP_WAIT_UINT;
    static constexpr int wake_op = UMTX_OP_WAKE;
#endif
};

template< typename Base, bool Interprocess >
struct wait_operations< Base, sizeof(unsigned int), true, Interprocess > :
    public wait_operations_freebsd_umtx_common< Base, uint_umtx_ops< Interprocess > >
{
};

#endif // defined(UMTX_OP_WAIT_UINT)

#if defined(UMTX_OP_WAIT) && (!defined(UMTX_OP_WAIT_UINT) || BOOST_ATOMIC_DETAIL_SIZEOF_INT < BOOST_ATOMIC_DETAIL_SIZEOF_LONG)

struct ulong_umtx_ops
{
    static constexpr int wait_op = UMTX_OP_WAIT;
    static constexpr int wake_op = UMTX_OP_WAKE;
};

template< typename Base, bool Interprocess >
struct wait_operations< Base, sizeof(unsigned long), true, Interprocess > :
    public wait_operations_freebsd_umtx_common< Base, ulong_umtx_ops >
{
};

#endif // defined(UMTX_OP_WAIT) && (!defined(UMTX_OP_WAIT_UINT) || BOOST_ATOMIC_DETAIL_SIZEOF_INT < BOOST_ATOMIC_DETAIL_SIZEOF_LONG)

#endif // defined(UMTX_OP_WAIT_UINT) || defined(UMTX_OP_WAIT)

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_WAIT_OPS_FREEBSD_UMTX_HPP_INCLUDED_
