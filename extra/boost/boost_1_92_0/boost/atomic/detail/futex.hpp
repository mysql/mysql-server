/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/futex.hpp
 *
 * This header defines wrappers around futex syscall.
 *
 * http://man7.org/linux/man-pages/man2/futex.2.html
 * https://man.openbsd.org/futex
 */

#ifndef BOOST_ATOMIC_DETAIL_FUTEX_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_FUTEX_HPP_INCLUDED_

#include <boost/atomic/detail/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if defined(__linux__)

#include <sys/syscall.h>

#if defined(SYS_futex)
#define BOOST_ATOMIC_DETAIL_SYS_FUTEX SYS_futex
#elif defined(SYS_futex_time64)
// On some 32-bit targets (e.g. riscv32) SYS_futex is not defined and instead SYS_futex_time64 is implemented,
// which is equivalent to SYS_futex but uses 64-bit time_t.
#define BOOST_ATOMIC_DETAIL_SYS_FUTEX SYS_futex_time64
#define BOOST_ATOMIC_DETAIL_FUTEX_TIME64
#elif defined(__NR_futex)
// Some Android NDKs (Google NDK and older Crystax.NET NDK versions) don't define SYS_futex.
#define BOOST_ATOMIC_DETAIL_SYS_FUTEX __NR_futex
#endif

#elif defined(__OpenBSD__)

// OpenBSD provides futex(2) function wrapper since OpenBSD 6.2 (https://man.openbsd.org/OpenBSD-6.2/futex.2).
// It has also removed syscall(2) interface:
// https://github.com/openbsd/src/commit/cafeb892b121ee89c39c2b940e8ccd6950f50009

#include <sys/param.h>
#include <cerrno>

#if OpenBSD >= 201711
#define BOOST_ATOMIC_DETAIL_OPENBSD_FUTEX
#endif // OpenBSD >= 201711

#elif defined(__NETBSD__) || defined(__NetBSD__)

#include <sys/syscall.h>

#if defined(SYS___futex)
// NetBSD defines SYS___futex, which has slightly different parameters. Basically, it has decoupled timeout and val2 parameters:
// int __futex(int *addr1, int op, int val1, const struct timespec *timeout, int *addr2, int val2, int val3);
// https://ftp.netbsd.org/pub/NetBSD/NetBSD-current/src/sys/sys/syscall.h
// http://bxr.su/NetBSD/sys/kern/sys_futex.c
#define BOOST_ATOMIC_DETAIL_SYS_FUTEX SYS___futex
#define BOOST_ATOMIC_DETAIL_NETBSD_FUTEX
#endif // defined(SYS___futex)

#endif

#if defined(BOOST_ATOMIC_DETAIL_SYS_FUTEX) || defined(BOOST_ATOMIC_DETAIL_OPENBSD_FUTEX)

#if defined(__linux__)
#include <linux/futex.h>
#else
#include <sys/futex.h>
#endif
#include <time.h> // timespec
#include <cstdint>
#include <boost/atomic/detail/intptr.hpp>
#include <boost/atomic/detail/header.hpp>

#define BOOST_ATOMIC_DETAIL_HAS_FUTEX

// Note: On Android, futex.h is lacking many definitions, but the actual Linux kernel supports the API in full.
#if defined(FUTEX_WAIT_BITSET)
#define BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET FUTEX_WAIT_BITSET
#elif defined(__ANDROID__)
#define BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET 9
#endif

#if defined(FUTEX_PRIVATE_FLAG)
#define BOOST_ATOMIC_DETAIL_FUTEX_PRIVATE_FLAG FUTEX_PRIVATE_FLAG
#elif defined(__ANDROID__)
#define BOOST_ATOMIC_DETAIL_FUTEX_PRIVATE_FLAG 128
#else
#define BOOST_ATOMIC_DETAIL_FUTEX_PRIVATE_FLAG 0
#endif

#if defined(FUTEX_CLOCK_REALTIME)
#define BOOST_ATOMIC_DETAIL_FUTEX_CLOCK_REALTIME FUTEX_CLOCK_REALTIME
#elif defined(__ANDROID__)
#define BOOST_ATOMIC_DETAIL_FUTEX_CLOCK_REALTIME 256
#endif

namespace boost {
namespace atomics {
namespace detail {

#if defined(BOOST_ATOMIC_DETAIL_FUTEX_TIME64)

//! An equivalent of `timespec` that uses 64-bit members when the userland `timespec` is 32-bit
struct futex_timespec
{
    std::int64_t tv_sec;
    std::int64_t tv_nsec;

    futex_timespec() = default;
    explicit futex_timespec(::timespec ts) noexcept :
        tv_sec(ts.tv_sec), tv_nsec(ts.tv_nsec)
    {}
};

#else // defined(BOOST_ATOMIC_DETAIL_FUTEX_TIME64)

using futex_timespec = ::timespec;

#endif // defined(BOOST_ATOMIC_DETAIL_FUTEX_TIME64)

//! Invokes an operation on the futex
BOOST_FORCEINLINE int futex_invoke(void* addr1, int op, unsigned int val1, const futex_timespec* timeout = nullptr, void* addr2 = nullptr, unsigned int val3 = 0u) noexcept
{
#if defined(BOOST_ATOMIC_DETAIL_OPENBSD_FUTEX)
    return ::futex
    (
        static_cast< volatile std::uint32_t* >(addr1),
        op,
        static_cast< int >(val1),
        timeout,
        static_cast< volatile std::uint32_t* >(addr2)
    );
#elif defined(BOOST_ATOMIC_DETAIL_NETBSD_FUTEX)
    // Pass 0 in val2.
    return ::syscall(BOOST_ATOMIC_DETAIL_SYS_FUTEX, addr1, op, val1, timeout, addr2, 0u, val3);
#else
    return ::syscall(BOOST_ATOMIC_DETAIL_SYS_FUTEX, addr1, op, val1, timeout, addr2, val3);
#endif
}

//! Invokes an operation on the futex
BOOST_FORCEINLINE int futex_invoke(void* addr1, int op, unsigned int val1, unsigned int val2, void* addr2 = nullptr, unsigned int val3 = 0u) noexcept
{
#if defined(BOOST_ATOMIC_DETAIL_OPENBSD_FUTEX)
    return ::futex
    (
        static_cast< volatile std::uint32_t* >(addr1),
        op,
        static_cast< int >(val1),
        reinterpret_cast< const futex_timespec* >(static_cast< atomics::detail::uintptr_t >(val2)),
        static_cast< volatile std::uint32_t* >(addr2)
    );
#elif defined(BOOST_ATOMIC_DETAIL_NETBSD_FUTEX)
    // Pass nullptr in timeout.
    return ::syscall(BOOST_ATOMIC_DETAIL_SYS_FUTEX, addr1, op, val1, static_cast< void* >(nullptr), addr2, val2, val3);
#else
    return ::syscall(BOOST_ATOMIC_DETAIL_SYS_FUTEX, addr1, op, val1, static_cast< atomics::detail::uintptr_t >(val2), addr2, val3);
#endif
}

//! Checks that the value \c pval is \c expected and blocks
BOOST_FORCEINLINE int futex_wait(void* pval, unsigned int expected, int flags) noexcept
{
    int res = futex_invoke(pval, FUTEX_WAIT | flags, expected);
#if defined(OpenBSD) && (OpenBSD < 202111)
    // In older OpenBSD versions, futex(2) returned error code directly instead of setting errno and returning -1.
    // This was fixed in OpenBSD 7.0 (https://github.com/openbsd/src/commit/3288ea8fbfe504db25b57dd18b664a1aa377e4bf).
    // This primarily affects FUTEX_WAIT. For FUTEX_WAKE and FUTEX_REQUEUE the returned value may be positive
    // on successful completion of the call and there seem to be no errors that can be returned. Other functions
    // are not supported on OpenBSD 7.0 and older.
    if (res > 0)
    {
        errno = res;
        res = -1;
    }
#endif // defined(OpenBSD) && (OpenBSD < 202111)
    return res;
}

//! Checks that the value \c pval is \c expected and blocks until timeout
BOOST_FORCEINLINE int futex_wait_for(void* pval, unsigned int expected, futex_timespec const& timeout, int flags) noexcept
{
    int res = futex_invoke(pval, FUTEX_WAIT | flags, expected, &timeout);
#if defined(OpenBSD) && (OpenBSD < 202111)
    // See the comment in futex_wait
    if (res > 0)
    {
        errno = res;
        res = -1;
    }
#endif // defined(OpenBSD) && (OpenBSD < 202111)
    return res;
}

#if defined(BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET)

//! Checks that the value \c pval is \c expected and blocks until timeout
BOOST_FORCEINLINE int futex_wait_until(void* pval, unsigned int expected, futex_timespec const& timeout, int flags) noexcept
{
    return futex_invoke(pval, BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET | flags, expected, &timeout, nullptr, ~static_cast< unsigned int >(0u));
}

#endif // defined(BOOST_ATOMIC_DETAIL_FUTEX_WAIT_BITSET)

//! Wakes the specified number of threads waiting on the futex
BOOST_FORCEINLINE int futex_signal(void* pval, int flags, unsigned int count = 1u) noexcept
{
    return futex_invoke(pval, FUTEX_WAKE | flags, count);
}

//! Wakes all threads waiting on the futex
BOOST_FORCEINLINE int futex_broadcast(void* pval, int flags) noexcept
{
    return futex_signal(pval, flags, (~static_cast< unsigned int >(0u)) >> 1u);
}

//! Wakes the wake_count threads waiting on the futex pval1 and requeues up to requeue_count of the blocked threads onto another futex pval2
BOOST_FORCEINLINE int futex_requeue
(
    void* pval1,
    void* pval2,
    int flags,
    unsigned int wake_count = 1u,
    unsigned int requeue_count = (~static_cast< unsigned int >(0u)) >> 1u
) noexcept
{
    return futex_invoke(pval1, FUTEX_REQUEUE | flags, wake_count, requeue_count, pval2);
}

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // defined(BOOST_ATOMIC_DETAIL_SYS_FUTEX) || defined(BOOST_ATOMIC_DETAIL_OPENBSD_FUTEX)

#endif // BOOST_ATOMIC_DETAIL_FUTEX_HPP_INCLUDED_
