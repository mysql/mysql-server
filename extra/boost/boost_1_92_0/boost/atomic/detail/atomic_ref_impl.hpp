/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/atomic_ref_impl.hpp
 *
 * This header contains implementation of \c atomic_ref template.
 */

#ifndef BOOST_ATOMIC_DETAIL_ATOMIC_REF_IMPL_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_ATOMIC_REF_IMPL_HPP_INCLUDED_

#include <cstddef>
#include <chrono>
#include <utility>
#include <type_traits>
#include <boost/assert.hpp>
#include <boost/memory_order.hpp>
#include <boost/atomic/wait_result.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/addressof.hpp>
#include <boost/atomic/detail/storage_traits.hpp>
#include <boost/atomic/detail/bitwise_cast.hpp>
#include <boost/atomic/detail/core_operations.hpp>
#include <boost/atomic/detail/wait_operations.hpp>
#include <boost/atomic/detail/extra_operations.hpp>
#include <boost/atomic/detail/core_operations_emulated.hpp>
#include <boost/atomic/detail/memory_order_utils.hpp>
#include <boost/atomic/detail/type_traits/is_signed.hpp>
#include <boost/atomic/detail/type_traits/alignment_of.hpp>
#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)
#include <boost/atomic/detail/bitwise_fp_cast.hpp>
#include <boost/atomic/detail/fp_operations.hpp>
#include <boost/atomic/detail/extra_fp_operations.hpp>
#endif
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

/*
 * IMPLEMENTATION NOTE: All interface functions MUST be declared with BOOST_FORCEINLINE,
 *                      see comment for convert_memory_order_to_gcc in gcc_atomic_memory_order_utils.hpp.
 */

namespace boost {
namespace atomics {
namespace detail {

template< typename T, bool Signed, bool Interprocess >
struct is_atomic_ref_lock_free
{
    using value_type = T;
    using core_operations = atomics::detail::core_operations< sizeof(value_type), Signed, Interprocess >;
    using storage_type = typename core_operations::storage_type;

    static constexpr bool value = sizeof(value_type) == sizeof(storage_type) && core_operations::is_always_lock_free;
};

template< typename T, bool Signed, bool Interprocess >
class base_atomic_ref_common
{
public:
    using value_type = T;

protected:
    using unqualified_value_type = typename std::remove_cv< value_type >::type;
    using core_operations = typename std::conditional<
        atomics::detail::is_atomic_ref_lock_free< T, Signed, Interprocess >::value,
        atomics::detail::core_operations< sizeof(value_type), Signed, Interprocess >,
        atomics::detail::core_operations_emulated< sizeof(value_type), atomics::detail::alignment_of< value_type >::value, Signed, Interprocess >
    >::type;
    using wait_operations = atomics::detail::wait_operations< core_operations >;
    using value_arg_type = typename std::conditional< sizeof(value_type) <= sizeof(void*), value_type, value_type const& >::type;
    using storage_type = typename core_operations::storage_type;
    static_assert(sizeof(storage_type) == sizeof(value_type), "Boost.Atomic internal error: atomic_ref storage size doesn't match the value size");

public:
    static constexpr std::size_t required_alignment =
        atomics::detail::alignment_of< value_type >::value <= core_operations::storage_alignment ?
            core_operations::storage_alignment : atomics::detail::alignment_of< value_type >::value;
    static constexpr bool is_always_lock_free = core_operations::is_always_lock_free;
    static constexpr bool always_has_native_wait_notify = wait_operations::always_has_native_wait_notify;

protected:
    value_type* m_value;

public:
    BOOST_FORCEINLINE explicit base_atomic_ref_common(value_type& v) noexcept : m_value(atomics::detail::addressof(v))
    {
        BOOST_ATOMIC_DETAIL_CLEAR_PADDING(const_cast< unqualified_value_type* >(m_value));
    }

    BOOST_FORCEINLINE value_type& value() const noexcept { return *m_value; }

protected:
    BOOST_FORCEINLINE storage_type& storage() const noexcept
    {
        return *reinterpret_cast< storage_type* >(const_cast< unqualified_value_type* >(m_value));
    }

public:
    BOOST_FORCEINLINE bool is_lock_free() const noexcept
    {
        // C++20 specifies that is_lock_free returns true if operations on *all* objects of the atomic_ref<T> type are lock-free.
        // This does not allow to return true or false depending on the referenced object runtime alignment. Currently, Boost.Atomic
        // follows this specification, although we may support runtime alignment checking in the future.
        return is_always_lock_free;
    }

    BOOST_FORCEINLINE bool has_native_wait_notify() const noexcept
    {
        return wait_operations::has_native_wait_notify(this->storage());
    }

    BOOST_FORCEINLINE void notify_one() const noexcept
    {
        wait_operations::notify_one(this->storage());
    }

    BOOST_FORCEINLINE void notify_all() const noexcept
    {
        wait_operations::notify_all(this->storage());
    }
};

#if defined(BOOST_NO_CXX17_INLINE_VARIABLES)
template< typename T, bool Signed, bool Interprocess >
constexpr std::size_t base_atomic_ref_common< T, Signed, Interprocess >::required_alignment;
template< typename T, bool Signed, bool Interprocess >
constexpr bool base_atomic_ref_common< T, Signed, Interprocess >::is_always_lock_free;
template< typename T, bool Signed, bool Interprocess >
constexpr bool base_atomic_ref_common< T, Signed, Interprocess >::always_has_native_wait_notify;
#endif


template< typename T, typename Kind, bool Interprocess >
class base_atomic_ref;

//! General template. Implementation for user-defined types, such as structs, and pointers to non-object types
template< typename T, bool Interprocess >
class base_atomic_ref< T, void, Interprocess > :
    public base_atomic_ref_common< T, false, Interprocess >
{
private:
    using base_type = base_atomic_ref_common< T, false, Interprocess >;

public:
    using value_type = typename base_type::value_type;

protected:
    using core_operations = typename base_type::core_operations;
    using wait_operations = typename base_type::wait_operations;
    using storage_type = typename base_type::storage_type;
    using value_arg_type = typename base_type::value_arg_type;

private:
    using cxchg_use_bitwise_cast =
#if !defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS) || !defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
        std::true_type;
#else
        std::integral_constant< bool, atomics::detail::alignment_of< value_type >::value <= core_operations::storage_alignment >;
#endif

public:
    base_atomic_ref(base_atomic_ref const&) = default;
    BOOST_FORCEINLINE explicit base_atomic_ref(value_type& v) noexcept : base_type(v)
    {
    }

    base_atomic_ref& operator=(base_atomic_ref const&) = delete;

    BOOST_FORCEINLINE void store(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        core_operations::store(this->storage(), atomics::detail::bitwise_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(core_operations::load(this->storage(), order));
    }

    BOOST_FORCEINLINE value_type exchange(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::exchange(this->storage(), atomics::detail::bitwise_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_strong_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_weak_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE value_type wait(value_arg_type old_val, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(wait_operations::wait(this->storage(), atomics::detail::bitwise_cast< storage_type >(old_val), order));
    }

    template< typename Clock, typename Duration >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_until(value_arg_type old_val, std::chrono::time_point< Clock, Duration > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_until(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_until(this->storage(), atomics::detail::bitwise_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_cast< value_type >(new_value), timed_out);
    }

    template< typename Rep, typename Period >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_for(value_arg_type old_val, std::chrono::duration< Rep, Period > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_for(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_for(this->storage(), atomics::detail::bitwise_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_cast< value_type >(new_value), timed_out);
    }

private:
    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_strong(
            this->storage(), reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = atomics::detail::bitwise_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_strong(
            this->storage(), old_value, atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_weak(
            this->storage(), reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = atomics::detail::bitwise_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_weak(
            this->storage(), old_value, atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }
};


//! Implementation for enums
template< typename T, bool Interprocess >
class base_atomic_ref< T, const int, Interprocess > :
    public base_atomic_ref_common< T, false, Interprocess >
{
private:
    using base_type = base_atomic_ref_common< T, false, Interprocess >;

public:
    using value_type = typename base_type::value_type;

protected:
    using core_operations = typename base_type::core_operations;
    using wait_operations = typename base_type::wait_operations;
    using extra_operations = atomics::detail::extra_operations< core_operations >;
    using storage_type = typename base_type::storage_type;
    using value_arg_type = typename base_type::value_arg_type;

private:
    using cxchg_use_bitwise_cast =
#if !defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS) || !defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
        std::true_type;
#else
        std::integral_constant< bool, atomics::detail::alignment_of< value_type >::value <= core_operations::storage_alignment >;
#endif

public:
    base_atomic_ref(base_atomic_ref const&) = default;
    BOOST_FORCEINLINE explicit base_atomic_ref(value_type& v) noexcept : base_type(v)
    {
    }

    base_atomic_ref& operator=(base_atomic_ref const&) = delete;

    BOOST_FORCEINLINE void store(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        core_operations::store(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(core_operations::load(this->storage(), order));
    }

    BOOST_FORCEINLINE value_type exchange(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::exchange(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_strong_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_weak_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE value_type fetch_and(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_and(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type fetch_or(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_or(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type fetch_xor(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_xor(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type fetch_complement(memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::fetch_complement(this->storage(), order));
    }

    BOOST_FORCEINLINE value_type bitwise_and(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::bitwise_and(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type bitwise_or(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::bitwise_or(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type bitwise_xor(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::bitwise_xor(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type bitwise_complement(memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::bitwise_complement(this->storage(), order));
    }

    BOOST_FORCEINLINE void opaque_and(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_and(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_or(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_or(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_xor(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_xor(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_complement(memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_complement(this->storage(), order);
    }

    BOOST_FORCEINLINE bool and_and_test(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::and_and_test(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool or_and_test(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::or_and_test(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool xor_and_test(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::xor_and_test(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool complement_and_test(memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::complement_and_test(this->storage(), order);
    }

    BOOST_FORCEINLINE bool bit_test_and_set(unsigned int bit_number, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(bit_number < sizeof(value_type) * 8u);
        return extra_operations::bit_test_and_set(this->storage(), bit_number, order);
    }

    BOOST_FORCEINLINE bool bit_test_and_reset(unsigned int bit_number, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(bit_number < sizeof(value_type) * 8u);
        return extra_operations::bit_test_and_reset(this->storage(), bit_number, order);
    }

    BOOST_FORCEINLINE bool bit_test_and_complement(unsigned int bit_number, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(bit_number < sizeof(value_type) * 8u);
        return extra_operations::bit_test_and_complement(this->storage(), bit_number, order);
    }

    BOOST_FORCEINLINE value_type operator&=(value_type v) const noexcept
    {
        return bitwise_and(v);
    }

    BOOST_FORCEINLINE value_type operator|=(value_type v) const noexcept
    {
        return bitwise_or(v);
    }

    BOOST_FORCEINLINE value_type operator^=(value_type v) const noexcept
    {
        return bitwise_xor(v);
    }

    BOOST_FORCEINLINE value_type wait(value_arg_type old_val, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(wait_operations::wait(this->storage(), static_cast< storage_type >(old_val), order));
    }

    template< typename Clock, typename Duration >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_until(value_arg_type old_val, std::chrono::time_point< Clock, Duration > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_until(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_until(this->storage(), static_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_cast< value_type >(new_value), timed_out);
    }

    template< typename Rep, typename Period >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_for(value_arg_type old_val, std::chrono::duration< Rep, Period > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_for(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_for(this->storage(), static_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_cast< value_type >(new_value), timed_out);
    }

private:
    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_strong(
            this->storage(), reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_strong(this->storage(), old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_weak(
            this->storage(), reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_weak(this->storage(), old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }
};


//! Implementation for integers
template< typename T, bool Interprocess >
class base_atomic_ref< T, int, Interprocess > :
    public base_atomic_ref_common< T, atomics::detail::is_signed< T >::value, Interprocess >
{
private:
    using base_type = base_atomic_ref_common< T, atomics::detail::is_signed< T >::value, Interprocess >;

public:
    using value_type = typename base_type::value_type;
    using difference_type = typename base_type::value_type;

protected:
    using core_operations = typename base_type::core_operations;
    using wait_operations = typename base_type::wait_operations;
    using extra_operations = atomics::detail::extra_operations< core_operations >;
    using storage_type = typename base_type::storage_type;
    using value_arg_type = value_type;

private:
    using cxchg_use_bitwise_cast =
#if !defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS) || !defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
        std::true_type;
#else
        std::integral_constant< bool, atomics::detail::alignment_of< value_type >::value <= core_operations::storage_alignment >;
#endif

public:
    base_atomic_ref(base_atomic_ref const&) = default;
    BOOST_FORCEINLINE explicit base_atomic_ref(value_type& v) noexcept : base_type(v)
    {
    }

    base_atomic_ref& operator=(base_atomic_ref const&) = delete;

    // Standard methods
    BOOST_FORCEINLINE void store(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        core_operations::store(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(core_operations::load(this->storage(), order));
    }

    BOOST_FORCEINLINE value_type fetch_add(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_add(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type fetch_sub(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_sub(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type exchange(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::exchange(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_strong_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_weak_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE value_type fetch_and(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_and(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type fetch_or(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_or(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type fetch_xor(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_xor(this->storage(), static_cast< storage_type >(v), order));
    }

    // Boost.Atomic extensions
    BOOST_FORCEINLINE value_type fetch_negate(memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::fetch_negate(this->storage(), order));
    }

    BOOST_FORCEINLINE value_type fetch_complement(memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::fetch_complement(this->storage(), order));
    }

    BOOST_FORCEINLINE value_type add(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::add(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type sub(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::sub(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type negate(memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::negate(this->storage(), order));
    }

    BOOST_FORCEINLINE value_type bitwise_and(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::bitwise_and(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type bitwise_or(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::bitwise_or(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type bitwise_xor(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::bitwise_xor(this->storage(), static_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE value_type bitwise_complement(memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::bitwise_complement(this->storage(), order));
    }

    BOOST_FORCEINLINE void opaque_add(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_add(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_sub(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_sub(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_negate(memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_negate(this->storage(), order);
    }

    BOOST_FORCEINLINE void opaque_and(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_and(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_or(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_or(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_xor(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_xor(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE void opaque_complement(memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_complement(this->storage(), order);
    }

    BOOST_FORCEINLINE bool add_and_test(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::add_and_test(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool sub_and_test(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::sub_and_test(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool negate_and_test(memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::negate_and_test(this->storage(), order);
    }

    BOOST_FORCEINLINE bool and_and_test(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::and_and_test(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool or_and_test(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::or_and_test(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool xor_and_test(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::xor_and_test(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool complement_and_test(memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::complement_and_test(this->storage(), order);
    }

    BOOST_FORCEINLINE bool bit_test_and_set(unsigned int bit_number, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(bit_number < sizeof(value_type) * 8u);
        return extra_operations::bit_test_and_set(this->storage(), bit_number, order);
    }

    BOOST_FORCEINLINE bool bit_test_and_reset(unsigned int bit_number, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(bit_number < sizeof(value_type) * 8u);
        return extra_operations::bit_test_and_reset(this->storage(), bit_number, order);
    }

    BOOST_FORCEINLINE bool bit_test_and_complement(unsigned int bit_number, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(bit_number < sizeof(value_type) * 8u);
        return extra_operations::bit_test_and_complement(this->storage(), bit_number, order);
    }

    // Operators
    BOOST_FORCEINLINE value_type operator++(int) const noexcept
    {
        return fetch_add(1);
    }

    BOOST_FORCEINLINE value_type operator++() const noexcept
    {
        return add(1);
    }

    BOOST_FORCEINLINE value_type operator--(int) const noexcept
    {
        return fetch_sub(1);
    }

    BOOST_FORCEINLINE value_type operator--() const noexcept
    {
        return sub(1);
    }

    BOOST_FORCEINLINE value_type operator+=(difference_type v) const noexcept
    {
        return add(v);
    }

    BOOST_FORCEINLINE value_type operator-=(difference_type v) const noexcept
    {
        return sub(v);
    }

    BOOST_FORCEINLINE value_type operator&=(value_type v) const noexcept
    {
        return bitwise_and(v);
    }

    BOOST_FORCEINLINE value_type operator|=(value_type v) const noexcept
    {
        return bitwise_or(v);
    }

    BOOST_FORCEINLINE value_type operator^=(value_type v) const noexcept
    {
        return bitwise_xor(v);
    }

    BOOST_FORCEINLINE value_type wait(value_arg_type old_val, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(wait_operations::wait(this->storage(), static_cast< storage_type >(old_val), order));
    }

    template< typename Clock, typename Duration >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_until(value_arg_type old_val, std::chrono::time_point< Clock, Duration > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_until(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_until(this->storage(), static_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_cast< value_type >(new_value), timed_out);
    }

    template< typename Rep, typename Period >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_for(value_arg_type old_val, std::chrono::duration< Rep, Period > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_for(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_for(this->storage(), static_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_cast< value_type >(new_value), timed_out);
    }

private:
    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_strong(
            this->storage(), reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_strong(this->storage(), old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_weak(
            this->storage(), reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_weak(this->storage(), old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }
};

//! Implementation for bool
template< bool Interprocess >
class base_atomic_ref< bool, int, Interprocess > :
    public base_atomic_ref_common< bool, false, Interprocess >
{
private:
    using base_type = base_atomic_ref_common< bool, false, Interprocess >;

public:
    using value_type = bool;

protected:
    using core_operations = typename base_type::core_operations;
    using wait_operations = typename base_type::wait_operations;
    using storage_type = typename base_type::storage_type;
    using value_arg_type = value_type;

private:
    using cxchg_use_bitwise_cast =
#if !defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS) || !defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
        std::true_type;
#else
        std::integral_constant< bool, atomics::detail::alignment_of< value_type >::value <= core_operations::storage_alignment >;
#endif

public:
    base_atomic_ref(base_atomic_ref const&) = default;
    BOOST_FORCEINLINE explicit base_atomic_ref(value_type& v) noexcept : base_type(v)
    {
    }

    base_atomic_ref& operator=(base_atomic_ref const&) = delete;

    // Standard methods
    BOOST_FORCEINLINE void store(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        core_operations::store(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return !!core_operations::load(this->storage(), order);
    }

    BOOST_FORCEINLINE value_type exchange(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return !!core_operations::exchange(this->storage(), static_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_strong_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_weak_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE value_type wait(value_arg_type old_val, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return !!wait_operations::wait(this->storage(), static_cast< storage_type >(old_val), order);
    }

    template< typename Clock, typename Duration >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_until(value_arg_type old_val, std::chrono::time_point< Clock, Duration > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_until(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_until(this->storage(), static_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(!!new_value, timed_out);
    }

    template< typename Rep, typename Period >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_for(value_arg_type old_val, std::chrono::duration< Rep, Period > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_for(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_for(this->storage(), static_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(!!new_value, timed_out);
    }

private:
    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_strong(
            this->storage(), reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_strong(this->storage(), old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = !!old_value;
        return res;
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_weak(
            this->storage(), reinterpret_cast< storage_type& >(expected), static_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = static_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_weak(this->storage(), old_value, static_cast< storage_type >(desired), success_order, failure_order);
        expected = !!old_value;
        return res;
    }
};


#if !defined(BOOST_ATOMIC_NO_FLOATING_POINT)

//! Implementation for floating point types
template< typename T, bool Interprocess >
class base_atomic_ref< T, float, Interprocess > :
    public base_atomic_ref_common< T, false, Interprocess >
{
private:
    using base_type = base_atomic_ref_common< T, false, Interprocess >;

public:
    using value_type = typename base_type::value_type;
    using difference_type = typename base_type::value_type;

protected:
    using core_operations = typename base_type::core_operations;
    using wait_operations = typename base_type::wait_operations;
    using extra_operations = atomics::detail::extra_operations< core_operations >;
    using fp_operations = atomics::detail::fp_operations< extra_operations, value_type >;
    using extra_fp_operations = atomics::detail::extra_fp_operations< fp_operations >;
    using storage_type = typename base_type::storage_type;
    using value_arg_type = value_type;

private:
#if defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS) || defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
    using has_padding_bits = std::integral_constant< bool, atomics::detail::value_size_of< value_type >::value != sizeof(storage_type) >;
#endif
    using cxchg_use_bitwise_cast =
#if !defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS) || !defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
        std::true_type;
#else
        std::integral_constant< bool, has_padding_bits::value || atomics::detail::alignment_of< value_type >::value <= core_operations::storage_alignment >;
#endif

public:
    base_atomic_ref(base_atomic_ref const&) = default;
    BOOST_FORCEINLINE explicit base_atomic_ref(value_type& v) noexcept : base_type(v)
    {
        // We only need to call clear_padding_bits if the compiler does not implement
        // BOOST_ATOMIC_DETAIL_CLEAR_PADDING, which is called in the base class constructor.
#if defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
        this->clear_padding_bits(has_padding_bits());
#endif // defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
    }

    base_atomic_ref& operator=(base_atomic_ref const&) = delete;

    BOOST_FORCEINLINE void store(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        core_operations::store(this->storage(), atomics::detail::bitwise_fp_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_fp_cast< value_type >(core_operations::load(this->storage(), order));
    }

    BOOST_FORCEINLINE value_type fetch_add(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return fp_operations::fetch_add(this->storage(), v, order);
    }

    BOOST_FORCEINLINE value_type fetch_sub(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return fp_operations::fetch_sub(this->storage(), v, order);
    }

    BOOST_FORCEINLINE value_type exchange(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_fp_cast< value_type >(core_operations::exchange(this->storage(), atomics::detail::bitwise_fp_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_strong_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_weak_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    // Boost.Atomic extensions
    BOOST_FORCEINLINE value_type fetch_negate(memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_fp_operations::fetch_negate(this->storage(), order);
    }

    BOOST_FORCEINLINE value_type add(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_fp_operations::add(this->storage(), v, order);
    }

    BOOST_FORCEINLINE value_type sub(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_fp_operations::sub(this->storage(), v, order);
    }

    BOOST_FORCEINLINE value_type negate(memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_fp_operations::negate(this->storage(), order);
    }

    BOOST_FORCEINLINE void opaque_add(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_fp_operations::opaque_add(this->storage(), v, order);
    }

    BOOST_FORCEINLINE void opaque_sub(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_fp_operations::opaque_sub(this->storage(), v, order);
    }

    BOOST_FORCEINLINE void opaque_negate(memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_fp_operations::opaque_negate(this->storage(), order);
    }

    // Operators
    BOOST_FORCEINLINE value_type operator+=(difference_type v) const noexcept
    {
        return add(v);
    }

    BOOST_FORCEINLINE value_type operator-=(difference_type v) const noexcept
    {
        return sub(v);
    }

    BOOST_FORCEINLINE value_type wait(value_arg_type old_val, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_fp_cast< value_type >(wait_operations::wait(this->storage(), atomics::detail::bitwise_fp_cast< storage_type >(old_val), order));
    }

    template< typename Clock, typename Duration >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_until(value_arg_type old_val, std::chrono::time_point< Clock, Duration > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_until(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_until(this->storage(), atomics::detail::bitwise_fp_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_fp_cast< value_type >(new_value), timed_out);
    }

    template< typename Rep, typename Period >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_for(value_arg_type old_val, std::chrono::duration< Rep, Period > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_for(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_for(this->storage(), atomics::detail::bitwise_fp_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_fp_cast< value_type >(new_value), timed_out);
    }

private:
#if defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
    BOOST_FORCEINLINE void clear_padding_bits(std::false_type) const noexcept
    {
    }

    BOOST_FORCEINLINE void clear_padding_bits(std::true_type) const noexcept
    {
        atomics::detail::clear_tail_padding_bits< atomics::detail::value_size_of< value_type >::value >(this->storage());
    }
#endif // defined(BOOST_ATOMIC_NO_CLEAR_PADDING)

    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_strong(
            this->storage(), reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_fp_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = atomics::detail::bitwise_fp_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_strong(
            this->storage(), old_value, atomics::detail::bitwise_fp_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_fp_cast< value_type >(old_value);
        return res;
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_weak(
            this->storage(), reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_fp_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = atomics::detail::bitwise_fp_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_weak(
            this->storage(), old_value, atomics::detail::bitwise_fp_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_fp_cast< value_type >(old_value);
        return res;
    }
};

#endif // !defined(BOOST_ATOMIC_NO_FLOATING_POINT)


//! Implementation for pointers to object types
template< typename T, bool Interprocess >
class base_atomic_ref< T*, void*, Interprocess > :
    public base_atomic_ref_common< T*, false, Interprocess >
{
private:
    using base_type = base_atomic_ref_common< T*, false, Interprocess >;

public:
    using value_type = typename base_type::value_type;
    using difference_type = std::ptrdiff_t;

protected:
    using core_operations = typename base_type::core_operations;
    using wait_operations = typename base_type::wait_operations;
    using extra_operations = atomics::detail::extra_operations< core_operations >;
    using storage_type = typename base_type::storage_type;
    using value_arg_type = value_type;

private:
    using cxchg_use_bitwise_cast =
#if !defined(BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS) || !defined(BOOST_ATOMIC_NO_CLEAR_PADDING)
        std::true_type;
#else
        std::integral_constant< bool, atomics::detail::alignment_of< value_type >::value <= core_operations::storage_alignment >;
#endif

public:
    base_atomic_ref(base_atomic_ref const&) = default;
    BOOST_FORCEINLINE explicit base_atomic_ref(value_type& v) noexcept : base_type(v)
    {
    }

    base_atomic_ref& operator=(base_atomic_ref const&) = delete;

    // Standard methods
    BOOST_FORCEINLINE void store(value_arg_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_consume);
        BOOST_ASSERT(order != memory_order_acquire);
        BOOST_ASSERT(order != memory_order_acq_rel);

        core_operations::store(this->storage(), atomics::detail::bitwise_cast< storage_type >(v), order);
    }

    BOOST_FORCEINLINE value_type load(memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(core_operations::load(this->storage(), order));
    }

    BOOST_FORCEINLINE value_type fetch_add(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_add(this->storage(), static_cast< storage_type >(v * sizeof(T)), order));
    }

    BOOST_FORCEINLINE value_type fetch_sub(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::fetch_sub(this->storage(), static_cast< storage_type >(v * sizeof(T)), order));
    }

    BOOST_FORCEINLINE value_type exchange(value_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(core_operations::exchange(this->storage(), atomics::detail::bitwise_cast< storage_type >(v), order));
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_strong_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_strong(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_strong(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order) const noexcept
    {
        BOOST_ASSERT(failure_order != memory_order_release);
        BOOST_ASSERT(failure_order != memory_order_acq_rel);
        BOOST_ASSERT(cas_failure_order_must_not_be_stronger_than_success_order(success_order, failure_order));

        return compare_exchange_weak_impl(expected, desired, success_order, failure_order, cxchg_use_bitwise_cast());
    }

    BOOST_FORCEINLINE bool compare_exchange_weak(value_type& expected, value_arg_type desired, memory_order order = memory_order_seq_cst) const noexcept
    {
        return compare_exchange_weak(expected, desired, order, atomics::detail::deduce_failure_order(order));
    }

    // Boost.Atomic extensions
    BOOST_FORCEINLINE value_type add(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::add(this->storage(), static_cast< storage_type >(v * sizeof(T)), order));
    }

    BOOST_FORCEINLINE value_type sub(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return atomics::detail::bitwise_cast< value_type >(extra_operations::sub(this->storage(), static_cast< storage_type >(v * sizeof(T)), order));
    }

    BOOST_FORCEINLINE void opaque_add(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_add(this->storage(), static_cast< storage_type >(v * sizeof(T)), order);
    }

    BOOST_FORCEINLINE void opaque_sub(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        extra_operations::opaque_sub(this->storage(), static_cast< storage_type >(v * sizeof(T)), order);
    }

    BOOST_FORCEINLINE bool add_and_test(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::add_and_test(this->storage(), static_cast< storage_type >(v * sizeof(T)), order);
    }

    BOOST_FORCEINLINE bool sub_and_test(difference_type v, memory_order order = memory_order_seq_cst) const noexcept
    {
        return extra_operations::sub_and_test(this->storage(), static_cast< storage_type >(v * sizeof(T)), order);
    }

    // Operators
    BOOST_FORCEINLINE value_type operator++(int) const noexcept
    {
        return fetch_add(1);
    }

    BOOST_FORCEINLINE value_type operator++() const noexcept
    {
        return add(1);
    }

    BOOST_FORCEINLINE value_type operator--(int) const noexcept
    {
        return fetch_sub(1);
    }

    BOOST_FORCEINLINE value_type operator--() const noexcept
    {
        return sub(1);
    }

    BOOST_FORCEINLINE value_type operator+=(difference_type v) const noexcept
    {
        return add(v);
    }

    BOOST_FORCEINLINE value_type operator-=(difference_type v) const noexcept
    {
        return sub(v);
    }

    BOOST_FORCEINLINE value_type wait(value_arg_type old_val, memory_order order = memory_order_seq_cst) const noexcept
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        return atomics::detail::bitwise_cast< value_type >(wait_operations::wait(this->storage(), atomics::detail::bitwise_cast< storage_type >(old_val), order));
    }

    template< typename Clock, typename Duration >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_until(value_arg_type old_val, std::chrono::time_point< Clock, Duration > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_until(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_until(this->storage(), atomics::detail::bitwise_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_cast< value_type >(new_value), timed_out);
    }

    template< typename Rep, typename Period >
    BOOST_FORCEINLINE wait_result< value_type >
    wait_for(value_arg_type old_val, std::chrono::duration< Rep, Period > timeout, memory_order order = memory_order_seq_cst) const
        noexcept(noexcept(wait_operations::wait_for(
            std::declval< storage_type const volatile& >(), std::declval< storage_type >(), timeout, order, std::declval< bool& >())))
    {
        BOOST_ASSERT(order != memory_order_release);
        BOOST_ASSERT(order != memory_order_acq_rel);

        bool timed_out = false;
        storage_type new_value = wait_operations::wait_for(this->storage(), atomics::detail::bitwise_cast< storage_type >(old_val), timeout, order, timed_out);
        return wait_result< value_type >(atomics::detail::bitwise_cast< value_type >(new_value), timed_out);
    }

private:
    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_strong(
            this->storage(), reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_strong_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = atomics::detail::bitwise_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_strong(
            this->storage(), old_value, atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::false_type) const noexcept
    {
        return core_operations::compare_exchange_weak(
            this->storage(), reinterpret_cast< storage_type& >(expected), atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
    }

    BOOST_FORCEINLINE bool
    compare_exchange_weak_impl(value_type& expected, value_arg_type desired, memory_order success_order, memory_order failure_order, std::true_type) const noexcept
    {
        storage_type old_value = atomics::detail::bitwise_cast< storage_type >(expected);
        const bool res = core_operations::compare_exchange_weak(
            this->storage(), old_value, atomics::detail::bitwise_cast< storage_type >(desired), success_order, failure_order);
        expected = atomics::detail::bitwise_cast< value_type >(old_value);
        return res;
    }
};

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_ATOMIC_REF_IMPL_HPP_INCLUDED_
