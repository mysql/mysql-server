/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/wait_ops_generic.hpp
 *
 * This header contains generic (lock-based) implementation of the waiting/notifying atomic operations.
 *
 * This backend is used when lock-free atomic operations are available but native waiting/notifying operations are not.
 */

#ifndef BOOST_ATOMIC_DETAIL_WAIT_OPS_GENERIC_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_WAIT_OPS_GENERIC_HPP_INCLUDED_

#include <cstddef>
#include <chrono>
#include <boost/memory_order.hpp>
#include <boost/atomic/thread_pause.hpp>
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

//! Generic implementation of waiting/notifying operations
template< typename Base, bool Interprocess >
struct wait_operations_generic;

template< typename Base >
struct wait_operations_generic< Base, false > :
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

    static BOOST_FORCEINLINE storage_type wait(storage_type const volatile& storage, storage_type old_val, memory_order order) noexcept
    {
        storage_type new_val = base_type::load(storage, order);
        if (new_val == old_val)
        {
            scoped_wait_state wait_state(&storage);
            new_val = base_type::load(storage, order);
            while (new_val == old_val)
            {
                wait_state.wait();
                new_val = base_type::load(storage, order);
            }
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
        storage_type new_val = base_type::load(storage, order);
        if (new_val == old_val)
        {
            scoped_wait_state wait_state(&storage);
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
                new_val = base_type::load(storage, order);
            }
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
        memory_order order,
        bool& timed_out
    ) noexcept
    {
        storage_type new_val = base_type::load(storage, order);
        if (new_val == old_val)
        {
            scoped_wait_state wait_state(&storage);
            const timespec abs_timeout(posix_clock_traits< Clock >::to_timespec(timeout));
            if (BOOST_LIKELY(abs_timeout.tv_sec >= 0))
            {
                while (new_val == old_val)
                {
                    const bool wait_timed_out = wait_state.wait_until(posix_clock_traits< Clock >::clock_id, abs_timeout);
                    new_val = base_type::load(storage, order);

                    if (wait_timed_out)
                        goto timeout_expired;
                }
            }
            else
            {
            timeout_expired:
                timed_out = new_val == old_val;
            }
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
        std::true_type
    ) noexcept
    {
        return wait_until_abs_timeout< Clock >(storage, old_val, timeout, order, timed_out);
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

#else // !defined(BOOST_WINDOWS)

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

#endif // !defined(BOOST_WINDOWS)

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
        return wait_until_fallback< std::chrono::steady_clock >(storage, old_val, now + timeout, now, order, timed_out);
    }

    static BOOST_FORCEINLINE void notify_one(storage_type volatile& storage) noexcept
    {
        scoped_lock lock(&storage);
        lock_pool::notify_one(lock.get_lock_state(), &storage);
    }

    static BOOST_FORCEINLINE void notify_all(storage_type volatile& storage) noexcept
    {
        scoped_lock lock(&storage);
        lock_pool::notify_all(lock.get_lock_state(), &storage);
    }
};

template< typename Base >
struct wait_operations_generic< Base, true > :
    public Base
{
    using base_type = Base;
    using storage_type = typename base_type::storage_type;

    static constexpr bool always_has_native_wait_notify = false;

private:
    static constexpr unsigned int fast_loop_count = 16u;

public:
    static BOOST_FORCEINLINE bool has_native_wait_notify(storage_type const volatile&) noexcept
    {
        return false;
    }

    static BOOST_FORCEINLINE storage_type wait(storage_type const volatile& storage, storage_type old_val, memory_order order) noexcept
    {
        storage_type new_val = base_type::load(storage, order);
        if (new_val == old_val)
        {
            for (unsigned int i = 0u; i < fast_loop_count; ++i)
            {
                atomics::thread_pause();
                new_val = base_type::load(storage, order);
                if (new_val != old_val)
                    goto finish;
            }

            do
            {
                atomics::detail::wait_some();
                new_val = base_type::load(storage, order);
            }
            while (new_val == old_val);
        }

    finish:
        return new_val;
    }

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
        storage_type new_val = base_type::load(storage, order);
        if (new_val == old_val)
        {
            for (unsigned int i = 0u; i < fast_loop_count; ++i)
            {
                if ((now - timeout).count() >= 0)
                {
                    timed_out = true;
                    goto finish;
                }

                atomics::thread_pause();
                now = Clock::now();

                new_val = base_type::load(storage, order);
                if (new_val != old_val)
                    goto finish;
            }

            do
            {
                if ((now - timeout).count() >= 0)
                {
                    timed_out = true;
                    goto finish;
                }

                atomics::detail::wait_some();
                now = Clock::now();
                new_val = base_type::load(storage, order);
            }
            while (new_val == old_val);
        }

    finish:
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
    ) noexcept(noexcept(wait_until_impl< Clock >(storage, old_val, timeout, Clock::now(), order, timed_out)))
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

    static BOOST_FORCEINLINE void notify_one(storage_type volatile&) noexcept
    {
    }

    static BOOST_FORCEINLINE void notify_all(storage_type volatile&) noexcept
    {
    }
};

template< typename Base, std::size_t Size, bool Interprocess >
struct wait_operations< Base, Size, true, Interprocess > :
    public wait_operations_generic< Base, Interprocess >
{
};

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_WAIT_OPS_GENERIC_HPP_INCLUDED_
