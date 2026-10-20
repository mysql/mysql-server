/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2014-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/core_operations_emulated.hpp
 *
 * This header contains lock pool-based implementation of the core atomic operations.
 */

#ifndef BOOST_ATOMIC_DETAIL_CORE_OPERATIONS_EMULATED_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_CORE_OPERATIONS_EMULATED_HPP_INCLUDED_

#include <cstddef>
#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/storage_traits.hpp>
#include <boost/atomic/detail/core_operations_emulated_fwd.hpp>
#include <boost/atomic/detail/lock_pool.hpp>
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

template< std::size_t Size, std::size_t Alignment, bool = Alignment >= storage_traits< Size >::native_alignment >
struct core_operations_emulated_base
{
    using storage_type = typename storage_traits< Size >::type;
};

template< std::size_t Size, std::size_t Alignment >
struct core_operations_emulated_base< Size, Alignment, false >
{
    using storage_type = buffer_storage< Size, Alignment >;
};

//! Emulated implementation of core atomic operations
template< std::size_t Size, std::size_t Alignment, bool Signed, bool Interprocess >
struct core_operations_emulated :
    public core_operations_emulated_base< Size, Alignment >
{
    using base_type = core_operations_emulated_base< Size, Alignment >;

    // Define storage_type to have alignment not greater than Alignment. This will allow operations to work with value_types
    // that possibly have weaker alignment requirements than storage_traits< Size >::type would. This is important for atomic_ref<>.
    // atomic<> will allow higher alignment requirement than its value_type.
    // Note that storage_type should be an integral type, if possible, so that arithmetic and bitwise operations are possible.
    using storage_type = typename base_type::storage_type;

    static constexpr std::size_t storage_size = Size;
    static constexpr std::size_t storage_alignment = Alignment >= storage_traits< Size >::alignment ? storage_traits< Size >::alignment : Alignment;

    static constexpr bool is_signed = Signed;
    static constexpr bool is_interprocess = Interprocess;
    static constexpr bool full_cas_based = false;

    static constexpr bool is_always_lock_free = false;

    using scoped_lock = lock_pool::scoped_lock< storage_alignment >;

    static void store(storage_type volatile& storage, storage_type v, memory_order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        scoped_lock lock(&storage);
        const_cast< storage_type& >(storage) = v;
    }

    static storage_type load(storage_type const volatile& storage, memory_order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        scoped_lock lock(&storage);
        return const_cast< storage_type const& >(storage);
    }

    static storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        storage_type& s = const_cast< storage_type& >(storage);
        scoped_lock lock(&storage);
        storage_type old_val = s;
        s += v;
        return old_val;
    }

    static storage_type fetch_sub(storage_type volatile& storage, storage_type v, memory_order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        storage_type& s = const_cast< storage_type& >(storage);
        scoped_lock lock(&storage);
        storage_type old_val = s;
        s -= v;
        return old_val;
    }

    static storage_type exchange(storage_type volatile& storage, storage_type v, memory_order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        storage_type& s = const_cast< storage_type& >(storage);
        scoped_lock lock(&storage);
        storage_type old_val = s;
        s = v;
        return old_val;
    }

    static bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order, memory_order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        storage_type& s = const_cast< storage_type& >(storage);
        scoped_lock lock(&storage);
        storage_type old_val = s;
        const bool res = old_val == expected;
        if (res)
            s = desired;
        expected = old_val;

        return res;
    }

    static bool compare_exchange_weak(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order, memory_order) noexcept
    {
        // Note: This function is the exact copy of compare_exchange_strong. The reason we're not just forwarding the call
        // is that MSVC-12 ICEs in this case.
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        storage_type& s = const_cast< storage_type& >(storage);
        scoped_lock lock(&storage);
        storage_type old_val = s;
        const bool res = old_val == expected;
        if (res)
            s = desired;
        expected = old_val;

        return res;
    }

    static storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        storage_type& s = const_cast< storage_type& >(storage);
        scoped_lock lock(&storage);
        storage_type old_val = s;
        s &= v;
        return old_val;
    }

    static storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        storage_type& s = const_cast< storage_type& >(storage);
        scoped_lock lock(&storage);
        storage_type old_val = s;
        s |= v;
        return old_val;
    }

    static storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        storage_type& s = const_cast< storage_type& >(storage);
        scoped_lock lock(&storage);
        storage_type old_val = s;
        s ^= v;
        return old_val;
    }

    static BOOST_FORCEINLINE bool test_and_set(storage_type volatile& storage, memory_order order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        return !!exchange(storage, (storage_type)1, order);
    }

    static BOOST_FORCEINLINE void clear(storage_type volatile& storage, memory_order order) noexcept
    {
        static_assert(!is_interprocess, "Boost.Atomic: operation invoked on a non-lock-free inter-process atomic object");
        store(storage, (storage_type)0, order);
    }
};

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_CORE_OPERATIONS_EMULATED_HPP_INCLUDED_
