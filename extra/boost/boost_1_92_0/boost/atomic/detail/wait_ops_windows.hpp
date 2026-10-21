/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/wait_ops_windows.hpp
 *
 * This header contains implementation of the waiting/notifying atomic operations on Windows.
 */

#ifndef BOOST_ATOMIC_DETAIL_WAIT_OPS_WINDOWS_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_WAIT_OPS_WINDOWS_HPP_INCLUDED_

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/chrono.hpp>
#include <boost/atomic/detail/wait_operations_fwd.hpp>
#include <boost/atomic/detail/wait_capabilities.hpp>
#include <boost/winapi/wait_constants.hpp>
#include <boost/winapi/wait_on_address.hpp>
#if (defined(BOOST_ATOMIC_FORCE_AUTO_LINK) || (!defined(BOOST_ALL_NO_LIB) && !defined(BOOST_ATOMIC_NO_LIB)))
#define BOOST_LIB_NAME "synchronization"
#if defined(BOOST_AUTO_LINK_NOMANGLE)
#include <boost/config/auto_link.hpp>
#else // defined(BOOST_AUTO_LINK_NOMANGLE)
#define BOOST_AUTO_LINK_NOMANGLE
#include <boost/config/auto_link.hpp>
#undef BOOST_AUTO_LINK_NOMANGLE
#endif // defined(BOOST_AUTO_LINK_NOMANGLE)
#endif // (defined(BOOST_ATOMIC_FORCE_AUTO_LINK) || (!defined(BOOST_ALL_NO_LIB) && !defined(BOOST_ATOMIC_NO_LIB)))
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

template< typename Base, std::size_t Size >
struct wait_operations_windows :
    public Base
{
    using base_type = Base;
    using storage_type = typename base_type::storage_type;

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
            boost::winapi::WaitOnAddress(const_cast< storage_type* >(&storage), &old_val, Size, boost::winapi::infinite);
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
            const std::int64_t msec = atomics::detail::chrono::ceil< std::chrono::milliseconds >(timeout - now).count();
            if (msec <= 0)
            {
                timed_out = true;
                break;
            }

            boost::winapi::WaitOnAddress
            (
                const_cast< storage_type* >(&storage),
                &old_val,
                Size,
                msec <= static_cast< std::int64_t >(boost::winapi::max_non_infinite_wait) ?
                    static_cast< boost::winapi::DWORD_ >(msec) : boost::winapi::max_non_infinite_wait
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
        boost::winapi::WakeByAddressSingle(const_cast< storage_type* >(&storage));
    }

    static BOOST_FORCEINLINE void notify_all(storage_type volatile& storage) noexcept
    {
        boost::winapi::WakeByAddressAll(const_cast< storage_type* >(&storage));
    }
};

template< typename Base >
struct wait_operations< Base, 1u, true, false > :
    public wait_operations_windows< Base, 1u >
{
};

template< typename Base >
struct wait_operations< Base, 2u, true, false > :
    public wait_operations_windows< Base, 2u >
{
};

template< typename Base >
struct wait_operations< Base, 4u, true, false > :
    public wait_operations_windows< Base, 4u >
{
};

template< typename Base >
struct wait_operations< Base, 8u, true, false > :
    public wait_operations_windows< Base, 8u >
{
};

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_WAIT_OPS_WINDOWS_HPP_INCLUDED_
