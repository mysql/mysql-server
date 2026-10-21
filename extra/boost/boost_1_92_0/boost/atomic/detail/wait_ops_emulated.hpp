/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/wait_ops_emulated.hpp
 *
 * This header contains emulated (lock-based) implementation of the waiting and notifying atomic operations.
 */

#ifndef BOOST_ATOMIC_DETAIL_WAIT_OPS_EMULATED_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_WAIT_OPS_EMULATED_HPP_INCLUDED_

#include <cstddef>
#include <chrono>
#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#if !defined(BOOST_WINDOWS)
#include <time.h>
#include <type_traits>
#include <boost/atomic/posix_clock_traits_fwd.hpp>
#include <boost/atomic/detail/has_posix_clock_traits.hpp>
#endif
#include <boost/atomic/detail/chrono.hpp>
#include <boost/atomic/detail/lock_pool.hpp>
#include <boost/atomic/detail/wait_operations_fwd.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

//! Emulated implementation of waiting and notifying operations
template< typename Base >
struct wait_operations_emulated :
    public Base
{
    using base_type = Base;
    using storage_type = typename base_type::storage_type;
    using scoped_lock = lock_pool::scoped_lock< base_type::storage_alignment, true >;
    using scoped_wait_state = lock_pool::scoped_wait_state< base_type::storage_alignment >;

    static constexpr bool always_has_native_wait_notify = false;

    static BOOST_FORCEINLINE bool has_native_wait_notify(storage_type const volatile&) noexcept
    {
        return false;
    }

    static storage_type wait(storage_type const volatile& storage, storage_type old_val, memory_order) noexcept
    {
        static_assert(!base_type::is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        storage_type const& s = const_cast< storage_type const& >(storage);
        scoped_wait_state wait_state(&storage);
        storage_type new_val = s;
        while (new_val == old_val)
        {
            wait_state.wait();
            new_val = s;
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
        bool& timed_out
    ) noexcept(noexcept(Clock::now()))
    {
        storage_type const& s = const_cast< storage_type const& >(storage);
        scoped_wait_state wait_state(&storage);
        storage_type new_val = s;
        while (new_val == old_val)
        {
            const std::chrono::nanoseconds nsec = atomics::detail::chrono::ceil< std::chrono::nanoseconds >(timeout - now);
            if (nsec.count() <= 0)
            {
                timed_out = true;
                break;
            }

            wait_state.wait_for(nsec);

            now = Clock::now();
            new_val = s;
        }

        return new_val;
    }

#if !defined(BOOST_WINDOWS)

    template< typename Clock >
    static BOOST_FORCEINLINE storage_type wait_until_abs_timeout
    (
        storage_type const volatile& storage,
        storage_type old_val,
        typename Clock::time_point timeout,
        bool& timed_out
    ) noexcept
    {
        storage_type const& s = const_cast< storage_type const& >(storage);
        scoped_wait_state wait_state(&storage);
        storage_type new_val = s;
        const timespec abs_timeout(posix_clock_traits< Clock >::to_timespec(timeout));
        if (BOOST_LIKELY(abs_timeout.tv_sec >= 0))
        {
            while (new_val == old_val)
            {
                const bool wait_timed_out = wait_state.wait_until(posix_clock_traits< Clock >::clock_id, abs_timeout);
                new_val = s;

                if (wait_timed_out)
                    goto timeout_expired;
            }
        }
        else
        {
        timeout_expired:
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
        bool& timed_out,
        std::true_type
    ) noexcept
    {
        return wait_until_abs_timeout< Clock >(storage, old_val, timeout,timed_out);
    }

    template< typename Clock >
    static BOOST_FORCEINLINE storage_type wait_until_dispatch
    (
        storage_type const volatile& storage,
        storage_type old_val,
        typename Clock::time_point timeout,
        bool& timed_out,
        std::false_type
    ) noexcept(noexcept(Clock::now()))
    {
        return wait_until_fallback< Clock >(storage, old_val, timeout, Clock::now(), timed_out);
    }

public:
    template< typename Clock, typename Duration >
    static BOOST_FORCEINLINE storage_type wait_until
    (
        storage_type const volatile& storage,
        storage_type old_val,
        std::chrono::time_point< Clock, Duration > timeout,
        memory_order,
        bool& timed_out
    ) noexcept(noexcept(wait_until_dispatch< Clock >(storage, old_val, timeout, timed_out, std::integral_constant< bool, has_posix_clock_traits< Clock >::value >())))
    {
        static_assert(!base_type::is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        return wait_until_dispatch< Clock >(storage, old_val, timeout, timed_out, std::integral_constant< bool, has_posix_clock_traits< Clock >::value >());
    }

#else // !defined(BOOST_WINDOWS)

public:
    template< typename Clock, typename Duration >
    static BOOST_FORCEINLINE storage_type wait_until
    (
        storage_type const volatile& storage,
        storage_type old_val,
        std::chrono::time_point< Clock, Duration > timeout,
        memory_order,
        bool& timed_out
    ) noexcept(noexcept(wait_until_fallback< Clock >(storage, old_val, timeout, Clock::now(), timed_out)))
    {
        static_assert(!base_type::is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        return wait_until_fallback< Clock >(storage, old_val, timeout, Clock::now(), timed_out);
    }

#endif // !defined(BOOST_WINDOWS)

    template< typename Rep, typename Period >
    static BOOST_FORCEINLINE storage_type wait_for
    (
        storage_type const volatile& storage,
        storage_type old_val,
        std::chrono::duration< Rep, Period > timeout,
        memory_order,
        bool& timed_out
    ) noexcept
    {
        static_assert(!base_type::is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        return wait_until_fallback< std::chrono::steady_clock >(storage, old_val, now + timeout, now, timed_out);
    }

    static void notify_one(storage_type volatile& storage) noexcept
    {
        static_assert(!base_type::is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        scoped_lock lock(&storage);
        lock_pool::notify_one(lock.get_lock_state(), &storage);
    }

    static void notify_all(storage_type volatile& storage) noexcept
    {
        static_assert(!base_type::is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        scoped_lock lock(&storage);
        lock_pool::notify_all(lock.get_lock_state(), &storage);
    }
};

template< typename Base, std::size_t Size, bool Interprocess >
struct wait_operations< Base, Size, false, Interprocess > :
    public wait_operations_emulated< Base >
{
};

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_WAIT_OPS_EMULATED_HPP_INCLUDED_
