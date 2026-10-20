/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/wait_ops_dragonfly_umtx.hpp
 *
 * This header contains implementation of the waiting/notifying atomic operations based on DragonFly BSD umtx.
 * https://man.dragonflybsd.org/?command=umtx&section=2
 */

#ifndef BOOST_ATOMIC_DETAIL_WAIT_OPS_DRAGONFLY_UMTX_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_WAIT_OPS_DRAGONFLY_UMTX_HPP_INCLUDED_

#include <unistd.h>
#include <cstdint>
#include <limits>
#include <chrono>
#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/chrono.hpp>
#include <boost/atomic/detail/wait_operations_fwd.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

template< typename Base, bool Interprocess >
struct wait_operations< Base, sizeof(int), true, Interprocess > :
    public Base
{
    using base_type = Base;
    using storage_type = typename base_type::storage_type;

public:
    static constexpr bool always_has_native_wait_notify = true;

    static BOOST_FORCEINLINE bool has_native_wait_notify(storage_type const volatile&) noexcept
    {
        return true;
    }

    static BOOST_FORCEINLINE storage_type wait(storage_type const volatile& storage, storage_type old_val, memory_order order) noexcept
    {
        storage_type new_val = base_type::load(storage, order);
        while (new_val == old_val)
        {
            umtx_sleep(reinterpret_cast< int* >(const_cast< storage_type* >(&storage)), static_cast< int >(old_val), 0);
            new_val = base_type::load(storage, order);
        }

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
        while (new_val == old_val)
        {
            const std::int64_t usec = atomics::detail::chrono::ceil< std::chrono::microseconds >(timeout - now).count();
            if (usec <= 0)
            {
                timed_out = true;
                break;
            }

            umtx_sleep
            (
                reinterpret_cast< int* >(const_cast< storage_type* >(&storage)),
                static_cast< int >(old_val),
                usec <= (std::numeric_limits< int >::max)() ? static_cast< int >(usec) : (std::numeric_limits< int >::max)()
            );

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

    static BOOST_FORCEINLINE void notify_one(storage_type volatile& storage) noexcept
    {
        umtx_wakeup(reinterpret_cast< int* >(const_cast< storage_type* >(&storage)), 1);
    }

    static BOOST_FORCEINLINE void notify_all(storage_type volatile& storage) noexcept
    {
        umtx_wakeup(reinterpret_cast< int* >(const_cast< storage_type* >(&storage)), 0);
    }
};

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_WAIT_OPS_DRAGONFLY_UMTX_HPP_INCLUDED_
