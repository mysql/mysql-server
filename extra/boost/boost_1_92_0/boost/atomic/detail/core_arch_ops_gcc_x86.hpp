/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2009 Helge Bahmann
 * Copyright (c) 2012 Tim Blechmann
 * Copyright (c) 2014-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/core_arch_ops_gcc_x86.hpp
 *
 * This header contains implementation of the \c core_arch_operations template.
 */

#ifndef BOOST_ATOMIC_DETAIL_CORE_ARCH_OPS_GCC_X86_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_CORE_ARCH_OPS_GCC_X86_HPP_INCLUDED_

#include <cstddef>
#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/storage_traits.hpp>
#include <boost/atomic/detail/core_arch_operations_fwd.hpp>
#include <boost/atomic/detail/capabilities.hpp>
#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG8B) || defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG16B)
#include <cstdint>
#include <boost/atomic/detail/intptr.hpp>
#include <boost/atomic/detail/string_ops.hpp>
#include <boost/atomic/detail/core_ops_cas_based.hpp>
#endif
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace atomics {
namespace detail {

struct core_arch_operations_gcc_x86_base
{
    static constexpr bool full_cas_based = false;
    static constexpr bool is_always_lock_free = true;

    static BOOST_FORCEINLINE void fence_before(memory_order order) noexcept
    {
        if ((static_cast< unsigned int >(order) & static_cast< unsigned int >(memory_order_release)) != 0u)
            __asm__ __volatile__ ("" ::: "memory");
    }

    static BOOST_FORCEINLINE void fence_after(memory_order order) noexcept
    {
        if ((static_cast< unsigned int >(order) & (static_cast< unsigned int >(memory_order_consume) | static_cast< unsigned int >(memory_order_acquire))) != 0u)
            __asm__ __volatile__ ("" ::: "memory");
    }
};

template< std::size_t Size, bool Signed, bool Interprocess, typename Derived >
struct core_arch_operations_gcc_x86 :
    public core_arch_operations_gcc_x86_base
{
    using storage_type = typename storage_traits< Size >::type;

    static constexpr std::size_t storage_size = Size;
    static constexpr std::size_t storage_alignment = Size;
    static constexpr bool is_signed = Signed;
    static constexpr bool is_interprocess = Interprocess;

    static BOOST_FORCEINLINE void store(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        if (order != memory_order_seq_cst)
        {
            fence_before(order);
            storage = v;
            fence_after(order);
        }
        else
        {
            Derived::exchange(storage, v, order);
        }
    }

    static BOOST_FORCEINLINE storage_type load(storage_type const volatile& storage, memory_order order) noexcept
    {
        storage_type v = storage;
        fence_after(order);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type fetch_sub(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        return Derived::fetch_add(storage, -v, order);
    }

    static BOOST_FORCEINLINE bool compare_exchange_weak(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) noexcept
    {
        return Derived::compare_exchange_strong(storage, expected, desired, success_order, failure_order);
    }

    static BOOST_FORCEINLINE bool test_and_set(storage_type volatile& storage, memory_order order) noexcept
    {
        return !!Derived::exchange(storage, (storage_type)1, order);
    }

    static BOOST_FORCEINLINE void clear(storage_type volatile& storage, memory_order order) noexcept
    {
        store(storage, (storage_type)0, order);
    }
};

template< bool Signed, bool Interprocess >
struct core_arch_operations< 1u, Signed, Interprocess > :
    public core_arch_operations_gcc_x86< 1u, Signed, Interprocess, core_arch_operations< 1u, Signed, Interprocess > >
{
    using base_type = core_arch_operations_gcc_x86< 1u, Signed, Interprocess, core_arch_operations< 1u, Signed, Interprocess > >;
    using storage_type = typename base_type::storage_type;
    using temp_storage_type = typename storage_traits< 4u >::type;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        __asm__ __volatile__
        (
            "lock; xaddb %0, %1"
            : "+q" (v), "+m" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        __asm__ __volatile__
        (
            "xchgb %0, %1"
            : "+q" (v), "+m" (storage)
            :
            : "memory"
        );
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return v;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, success_order);
        storage_type previous = expected;
        bool success;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchgb %3, %1"
            : "+a" (previous), "+m" (storage), "=@ccz" (success)
            : "q" (desired)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#else // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchgb %3, %1\n\t"
            "sete %2"
            : "+a" (previous), "+m" (storage), "=q" (success)
            : "q" (desired)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        expected = previous;
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, (success ? success_order : failure_order));
        return success;
    }

#define BOOST_ATOMIC_DETAIL_CAS_LOOP(op, argument, result)\
    temp_storage_type new_val;\
    __asm__ __volatile__\
    (\
        ".align 16\n\t"\
        "1: mov %[arg], %2\n\t"\
        op " %%al, %b2\n\t"\
        "lock; cmpxchgb %b2, %[storage]\n\t"\
        "jne 1b"\
        : [res] "+a" (result), [storage] "+m" (storage), "=&q" (new_val)\
        : [arg] "ir" ((temp_storage_type)argument)\
        : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"\
    )

    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("andb", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("orb", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("xorb", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

#undef BOOST_ATOMIC_DETAIL_CAS_LOOP
};

template< bool Signed, bool Interprocess >
struct core_arch_operations< 2u, Signed, Interprocess > :
    public core_arch_operations_gcc_x86< 2u, Signed, Interprocess, core_arch_operations< 2u, Signed, Interprocess > >
{
    using base_type = core_arch_operations_gcc_x86< 2u, Signed, Interprocess, core_arch_operations< 2u, Signed, Interprocess > >;
    using storage_type = typename base_type::storage_type;
    using temp_storage_type = typename storage_traits< 4u >::type;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        __asm__ __volatile__
        (
            "lock; xaddw %0, %1"
            : "+q" (v), "+m" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        __asm__ __volatile__
        (
            "xchgw %0, %1"
            : "+q" (v), "+m" (storage)
            :
            : "memory"
        );
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return v;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, success_order);
        storage_type previous = expected;
        bool success;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchgw %3, %1"
            : "+a" (previous), "+m" (storage), "=@ccz" (success)
            : "q" (desired)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#else // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchgw %3, %1\n\t"
            "sete %2"
            : "+a" (previous), "+m" (storage), "=q" (success)
            : "q" (desired)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        expected = previous;
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, (success ? success_order : failure_order));
        return success;
    }

#define BOOST_ATOMIC_DETAIL_CAS_LOOP(op, argument, result)\
    temp_storage_type new_val;\
    __asm__ __volatile__\
    (\
        ".align 16\n\t"\
        "1: mov %[arg], %2\n\t"\
        op " %%ax, %w2\n\t"\
        "lock; cmpxchgw %w2, %[storage]\n\t"\
        "jne 1b"\
        : [res] "+a" (result), [storage] "+m" (storage), "=&q" (new_val)\
        : [arg] "ir" ((temp_storage_type)argument)\
        : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"\
    )

    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("andw", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("orw", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("xorw", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

#undef BOOST_ATOMIC_DETAIL_CAS_LOOP
};

template< bool Signed, bool Interprocess >
struct core_arch_operations< 4u, Signed, Interprocess > :
    public core_arch_operations_gcc_x86< 4u, Signed, Interprocess, core_arch_operations< 4u, Signed, Interprocess > >
{
    using base_type = core_arch_operations_gcc_x86< 4u, Signed, Interprocess, core_arch_operations< 4u, Signed, Interprocess > >;
    using storage_type = typename base_type::storage_type;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        __asm__ __volatile__
        (
            "lock; xaddl %0, %1"
            : "+r" (v), "+m" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        __asm__ __volatile__
        (
            "xchgl %0, %1"
            : "+r" (v), "+m" (storage)
            :
            : "memory"
        );
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return v;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, success_order);
        storage_type previous = expected;
        bool success;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchgl %3, %1"
            : "+a" (previous), "+m" (storage), "=@ccz" (success)
            : "r" (desired)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#else // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchgl %3, %1\n\t"
            "sete %2"
            : "+a" (previous), "+m" (storage), "=q" (success)
            : "r" (desired)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        expected = previous;
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, (success ? success_order : failure_order));
        return success;
    }

#define BOOST_ATOMIC_DETAIL_CAS_LOOP(op, argument, result)\
    storage_type new_val;\
    __asm__ __volatile__\
    (\
        ".align 16\n\t"\
        "1: mov %[arg], %[new_val]\n\t"\
        op " %%eax, %[new_val]\n\t"\
        "lock; cmpxchgl %[new_val], %[storage]\n\t"\
        "jne 1b"\
        : [res] "+a" (result), [storage] "+m" (storage), [new_val] "=&r" (new_val)\
        : [arg] "ir" (argument)\
        : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"\
    )

    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("andl", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("orl", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("xorl", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

#undef BOOST_ATOMIC_DETAIL_CAS_LOOP
};

#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG8B)

// Note: In the 32-bit PIC code guarded with BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX below we have to avoid using memory
// operand constraints because the compiler may choose to use ebx as the base register for that operand. At least, clang
// is known to do that. For this reason we have to pre-compute a pointer to storage and pass it in edi. For the same reason
// we cannot save ebx to the stack with a mov instruction, so we use esi as a scratch register and restore it afterwards.
// Alternatively, we could push/pop the register to the stack, but exchanging the registers is faster.
// The need to pass a pointer in edi is a bit wasteful because normally the memory operand would use a base pointer
// with an offset (e.g. `this` + offset). But unfortunately, there seems to be no way around it.

template< bool Signed, bool Interprocess >
struct gcc_dcas_x86
{
    using storage_type = typename storage_traits< 8u >::type;
    using aliasing_uint32_t = std::uint32_t BOOST_ATOMIC_DETAIL_MAY_ALIAS;
#if defined(__SSE2__)
    using xmm_t = std::uint32_t __attribute__((__vector_size__(16)));
#elif defined(__SSE__)
    using xmm_t = float __attribute__((__vector_size__(16)));
#endif

    static constexpr std::size_t storage_size = 8u;
    static constexpr std::size_t storage_alignment = 8u;
    static constexpr bool is_signed = Signed;
    static constexpr bool is_interprocess = Interprocess;
    static constexpr bool full_cas_based = true;
    static constexpr bool is_always_lock_free = true;

    static BOOST_FORCEINLINE void store(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);

        if (BOOST_LIKELY(order != memory_order_seq_cst && (((uintptr_t)&storage) & 7u) == 0u))
        {
#if defined(__SSE__)
#if defined(__SSE2__)
            xmm_t value = { static_cast< std::uint32_t >(v), static_cast< std::uint32_t >(v >> 32u), 0u, 0u };
#else
            xmm_t value;
            BOOST_ATOMIC_DETAIL_MEMSET(&value, 0, sizeof(value));
            BOOST_ATOMIC_DETAIL_MEMCPY(&value, &v, sizeof(v));
#endif
            __asm__ __volatile__
            (
#if defined(__AVX__)
                "vmovq %[value], %[storage]\n\t"
#elif defined(__SSE2__)
                "movq %[value], %[storage]\n\t"
#else
                "movlps %[value], %[storage]\n\t"
#endif
                : [storage] "=m" (storage)
                : [value] "x" (value)
                : "memory"
            );
#else // defined(__SSE__)
            __asm__ __volatile__
            (
                "fildll %[value]\n\t"
                "fistpll %[storage]\n\t"
                : [storage] "=m" (storage)
                : [value] "m" (v)
                : "memory"
            );
#endif // defined(__SSE__)
        }
        else
        {
#if defined(BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX)
            __asm__ __volatile__
            (
                "xchgl %%ebx, %%esi\n\t"
                "movl %%eax, %%ebx\n\t"
                "movl (%[dest]), %%eax\n\t"
                "movl 4(%[dest]), %%edx\n\t"
                ".align 16\n\t"
                "1: lock; cmpxchg8b (%[dest])\n\t"
                "jne 1b\n\t"
                "xchgl %%ebx, %%esi\n\t"
                :
                : "a" ((std::uint32_t)v), "c" ((std::uint32_t)(v >> 32u)), [dest] "D" (&storage)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "edx", "memory"
            );
#else // defined(BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX)
            __asm__ __volatile__
            (
                "movl %[dest_lo], %%eax\n\t"
                "movl %[dest_hi], %%edx\n\t"
                ".align 16\n\t"
                "1: lock; cmpxchg8b %[dest_lo]\n\t"
                "jne 1b\n\t"
                : [dest_lo] "=m" (storage), [dest_hi] "=m" (reinterpret_cast< volatile aliasing_uint32_t* >(&storage)[1])
                : [value_lo] "b" ((std::uint32_t)v), "c" ((std::uint32_t)(v >> 32u))
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "eax", "edx", "memory"
            );
#endif // defined(BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX)
        }
    }

    static BOOST_FORCEINLINE storage_type load(storage_type const volatile& storage, memory_order order) noexcept
    {
        storage_type value;

        if (BOOST_LIKELY((((uintptr_t)&storage) & 7u) == 0u))
        {
#if defined(__SSE__)
            xmm_t v;
            __asm__ __volatile__
            (
#if defined(__AVX__)
                "vmovq %[storage], %[value]\n\t"
#elif defined(__SSE2__)
                "movq %[storage], %[value]\n\t"
#else
                "xorps %[value], %[value]\n\t"
                "movlps %[storage], %[value]\n\t"
#endif
                : [value] "=x" (v)
                : [storage] "m" (storage)
                : "memory"
            );
#if defined(__SSE2__) && (!defined(BOOST_GCC) || BOOST_GCC >= 40800)
            // gcc prior to 4.8 don't support subscript operator for vector types
            value = static_cast< storage_type >(v[0]) | (static_cast< storage_type >(v[1]) << 32u);
#else
            BOOST_ATOMIC_DETAIL_MEMCPY(&value, &v, sizeof(value));
#endif
#else // defined(__SSE__)
            __asm__ __volatile__
            (
                "fildll %[storage]\n\t"
                "fistpll %[value]\n\t"
                : [value] "=m" (value)
                : [storage] "m" (storage)
                : "memory"
            );
#endif // defined(__SSE__)
        }
        else
        {
            // Note that despite const qualification cmpxchg8b below may issue a store to the storage. The storage value
            // will not change, but this prevents the storage to reside in read-only memory.

#if defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

            std::uint32_t value_bits[2];

            // We don't care for comparison result here; the previous value will be stored into value anyway.
            // Also we don't care for ebx and ecx values, they just have to be equal to eax and edx before cmpxchg8b.
            __asm__ __volatile__
            (
                "movl %%ebx, %%eax\n\t"
                "movl %%ecx, %%edx\n\t"
                "lock; cmpxchg8b %[storage]\n\t"
                : "=&a" (value_bits[0]), "=&d" (value_bits[1])
                : [storage] "m" (storage)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );
            value = static_cast< storage_type >(value_bits[0]) | (static_cast< storage_type >(value_bits[1]) << 32u);

#else // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

            // We don't care for comparison result here; the previous value will be stored into value anyway.
            // Also we don't care for ebx and ecx values, they just have to be equal to eax and edx before cmpxchg8b.
            __asm__ __volatile__
            (
                "movl %%ebx, %%eax\n\t"
                "movl %%ecx, %%edx\n\t"
                "lock; cmpxchg8b %[storage]\n\t"
                : "=&A" (value)
                : [storage] "m" (storage)
                : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
            );

#endif // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)
        }

        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);

        return value;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, success_order);

#if defined(__clang__)

        // Clang cannot allocate eax:edx register pairs but it has sync intrinsics
        storage_type old_expected = expected;
        expected = __sync_val_compare_and_swap(&storage, old_expected, desired);
        return expected == old_expected;

#elif defined(BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX)

        bool success;

#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "xchgl %%ebx, %%esi\n\t"
            "lock; cmpxchg8b (%[dest])\n\t"
            "xchgl %%ebx, %%esi\n\t"
            : "+A" (expected), [success] "=@ccz" (success)
            : "S" ((std::uint32_t)desired), "c" ((std::uint32_t)(desired >> 32u)), [dest] "D" (&storage)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#else // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "xchgl %%ebx, %%esi\n\t"
            "lock; cmpxchg8b (%[dest])\n\t"
            "xchgl %%ebx, %%esi\n\t"
            "sete %[success]\n\t"
            : "+A" (expected), [success] "=qm" (success)
            : "S" ((std::uint32_t)desired), "c" ((std::uint32_t)(desired >> 32u)), [dest] "D" (&storage)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)

        return success;

#else // defined(BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX)

        bool success;

#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchg8b %[dest]\n\t"
            : "+A" (expected), [dest] "+m" (storage), [success] "=@ccz" (success)
            : "b" ((std::uint32_t)desired), "c" ((std::uint32_t)(desired >> 32u))
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#else // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchg8b %[dest]\n\t"
            "sete %[success]\n\t"
            : "+A" (expected), [dest] "+m" (storage), [success] "=qm" (success)
            : "b" ((std::uint32_t)desired), "c" ((std::uint32_t)(desired >> 32u))
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)

        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, (success ? success_order : failure_order));

        return success;

#endif // defined(BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX)
    }

    static BOOST_FORCEINLINE bool compare_exchange_weak(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) noexcept
    {
        return compare_exchange_strong(storage, expected, desired, success_order, failure_order);
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);

        storage_type old_value;

#if defined(BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX)
#if defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        std::uint32_t old_bits[2];
        __asm__ __volatile__
        (
            "xchgl %%ebx, %%esi\n\t"
            "movl (%[dest]), %%eax\n\t"
            "movl 4(%[dest]), %%edx\n\t"
            ".align 16\n\t"
            "1: lock; cmpxchg8b (%[dest])\n\t"
            "jne 1b\n\t"
            "xchgl %%ebx, %%esi\n\t"
            : "=a" (old_bits[0]), "=d" (old_bits[1])
            : "S" ((std::uint32_t)v), "c" ((std::uint32_t)(v >> 32u)), [dest] "D" (&storage)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

        BOOST_ATOMIC_DETAIL_MEMCPY(&old_value, old_bits, sizeof(old_value));

#else // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        __asm__ __volatile__
        (
            "xchgl %%ebx, %%esi\n\t"
            "movl (%[dest]), %%eax\n\t"
            "movl 4(%[dest]), %%edx\n\t"
            ".align 16\n\t"
            "1: lock; cmpxchg8b (%[dest])\n\t"
            "jne 1b\n\t"
            "xchgl %%ebx, %%esi\n\t"
            : "=A" (old_value)
            : "S" ((std::uint32_t)v), "c" ((std::uint32_t)(v >> 32u)), [dest] "D" (&storage)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

#endif // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)
#else // defined(BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX)
#if defined(__MINGW32__) && ((__GNUC__+0) * 100 + (__GNUC_MINOR__+0)) < 407

        // MinGW gcc up to 4.6 has problems with allocating registers in the asm blocks below
        std::uint32_t old_bits[2];
        __asm__ __volatile__
        (
            "movl (%[dest]), %%eax\n\t"
            "movl 4(%[dest]), %%edx\n\t"
            ".align 16\n\t"
            "1: lock; cmpxchg8b (%[dest])\n\t"
            "jne 1b\n\t"
            : "=&a" (old_bits[0]), "=&d" (old_bits[1])
            : "b" ((std::uint32_t)v), "c" ((std::uint32_t)(v >> 32u)), [dest] "DS" (&storage)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

        BOOST_ATOMIC_DETAIL_MEMCPY(&old_value, old_bits, sizeof(old_value));

#elif defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        std::uint32_t old_bits[2];
        __asm__ __volatile__
        (
            "movl %[dest_lo], %%eax\n\t"
            "movl %[dest_hi], %%edx\n\t"
            ".align 16\n\t"
            "1: lock; cmpxchg8b %[dest_lo]\n\t"
            "jne 1b\n\t"
            : "=&a" (old_bits[0]), "=&d" (old_bits[1]), [dest_lo] "+m" (storage), [dest_hi] "+m" (reinterpret_cast< volatile aliasing_uint32_t* >(&storage)[1])
            : "b" ((std::uint32_t)v), "c" ((std::uint32_t)(v >> 32u))
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

        BOOST_ATOMIC_DETAIL_MEMCPY(&old_value, old_bits, sizeof(old_value));

#else // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        __asm__ __volatile__
        (
            "movl %[dest_lo], %%eax\n\t"
            "movl %[dest_hi], %%edx\n\t"
            ".align 16\n\t"
            "1: lock; cmpxchg8b %[dest_lo]\n\t"
            "jne 1b\n\t"
            : "=&A" (old_value), [dest_lo] "+m" (storage), [dest_hi] "+m" (reinterpret_cast< volatile aliasing_uint32_t* >(&storage)[1])
            : "b" ((std::uint32_t)v), "c" ((std::uint32_t)(v >> 32u))
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

#endif // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)
#endif // defined(BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX)

        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);

        return old_value;
    }
};

template< bool Signed, bool Interprocess >
struct core_arch_operations< 8u, Signed, Interprocess > :
    public core_operations_cas_based< gcc_dcas_x86< Signed, Interprocess > >
{
};

#elif defined(__x86_64__)

template< bool Signed, bool Interprocess >
struct core_arch_operations< 8u, Signed, Interprocess > :
    public core_arch_operations_gcc_x86< 8u, Signed, Interprocess, core_arch_operations< 8u, Signed, Interprocess > >
{
    using base_type = core_arch_operations_gcc_x86< 8u, Signed, Interprocess, core_arch_operations< 8u, Signed, Interprocess > >;
    using storage_type = typename base_type::storage_type;

    static BOOST_FORCEINLINE storage_type fetch_add(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        __asm__ __volatile__
        (
            "lock; xaddq %0, %1"
            : "+r" (v), "+m" (storage)
            :
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return v;
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        __asm__ __volatile__
        (
            "xchgq %0, %1"
            : "+r" (v), "+m" (storage)
            :
            : "memory"
        );
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return v;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, success_order);
        storage_type previous = expected;
        bool success;
#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchgq %3, %1"
            : "+a" (previous), "+m" (storage), "=@ccz" (success)
            : "r" (desired)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#else // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchgq %3, %1\n\t"
            "sete %2"
            : "+a" (previous), "+m" (storage), "=q" (success)
            : "r" (desired)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        expected = previous;
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, (success ? success_order : failure_order));
        return success;
    }

#define BOOST_ATOMIC_DETAIL_CAS_LOOP(op, argument, result)\
    storage_type new_val;\
    __asm__ __volatile__\
    (\
        ".align 16\n\t"\
        "1: movq %[arg], %[new_val]\n\t"\
        op " %%rax, %[new_val]\n\t"\
        "lock; cmpxchgq %[new_val], %[storage]\n\t"\
        "jne 1b"\
        : [res] "+a" (result), [storage] "+m" (storage), [new_val] "=&r" (new_val)\
        : [arg] "r" (argument)\
        : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"\
    )

    static BOOST_FORCEINLINE storage_type fetch_and(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("andq", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_or(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("orq", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

    static BOOST_FORCEINLINE storage_type fetch_xor(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);
        storage_type res = storage;
        BOOST_ATOMIC_DETAIL_CAS_LOOP("xorq", v, res);
        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
        return res;
    }

#undef BOOST_ATOMIC_DETAIL_CAS_LOOP
};

#endif

#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG16B)

template< bool Signed, bool Interprocess >
struct gcc_dcas_x86_64
{
    using storage_type = typename storage_traits< 16u >::type;
    using aliasing_uint64_t = std::uint64_t BOOST_ATOMIC_DETAIL_MAY_ALIAS;
#if defined(__AVX__)
    using xmm_t = std::uint64_t __attribute__((__vector_size__(16)));
#endif

    static constexpr std::size_t storage_size = 16u;
    static constexpr std::size_t storage_alignment = 16u;
    static constexpr bool is_signed = Signed;
    static constexpr bool is_interprocess = Interprocess;
    static constexpr bool full_cas_based = true;
    static constexpr bool is_always_lock_free = true;

    static BOOST_FORCEINLINE void store(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);

#if defined(__AVX__)
        if (BOOST_LIKELY(order != memory_order_seq_cst && (((uintptr_t)&storage) & 15u) == 0u))
        {
            // According to SDM rev.88 from June 2025, Volume 3, 10.1.1 Guaranteed Atomic Operations, processors supporting AVX guarantee
            // aligned vector moves to be atomic.
#if defined(BOOST_HAS_INT128)
            xmm_t value = { static_cast< std::uint64_t >(v), static_cast< std::uint64_t >(v >> 64u) };
#else
            xmm_t value;
            BOOST_ATOMIC_DETAIL_MEMCPY(&value, &v, sizeof(v));
#endif
            __asm__ __volatile__
            (
                "vmovdqa %[value], %[storage]\n\t"
                : [storage] "=m" (storage)
                : [value] "x" (value)
                : "memory"
            );

            return;
        }
#endif // defined(__AVX__)

        __asm__ __volatile__
        (
            "movq %[dest_lo], %%rax\n\t"
            "movq %[dest_hi], %%rdx\n\t"
            ".align 16\n\t"
            "1: lock; cmpxchg16b %[dest_lo]\n\t"
            "jne 1b\n\t"
            : [dest_lo] "=m" (storage), [dest_hi] "=m" (reinterpret_cast< volatile aliasing_uint64_t* >(&storage)[1])
            : "b" (reinterpret_cast< const aliasing_uint64_t* >(&v)[0]), "c" (reinterpret_cast< const aliasing_uint64_t* >(&v)[1])
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "rax", "rdx", "memory"
        );
    }

    static BOOST_FORCEINLINE storage_type load(storage_type const volatile& storage, memory_order order) noexcept
    {
#if defined(__AVX__)
        if (BOOST_LIKELY((((uintptr_t)&storage) & 15u) == 0u))
        {
            // According to SDM rev.88 from June 2025, Volume 3, 10.1.1 Guaranteed Atomic Operations, processors supporting AVX guarantee
            // aligned vector moves to be atomic.
            xmm_t v;
            __asm__ __volatile__
            (
                "vmovdqa %[storage], %[value]\n\t"
                : [value] "=x" (v)
                : [storage] "m" (storage)
                : "memory"
            );

#if defined(BOOST_HAS_INT128) && (!defined(BOOST_GCC) || BOOST_GCC >= 40800)
            // gcc prior to 4.8 don't support subscript operator for vector types
            storage_type value = static_cast< storage_type >(v[0]) | (static_cast< storage_type >(v[1]) << 64u);
#else
            storage_type value;
            BOOST_ATOMIC_DETAIL_MEMCPY(&value, &v, sizeof(v));
#endif
            BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);
            return value;
        }
#endif // defined(__AVX__)

        // Note that despite const qualification cmpxchg16b below may issue a store to the storage. The storage value
        // will not change, but this prevents the storage to reside in read-only memory.

        storage_type value;

#if defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        std::uint64_t value_bits[2];

        // We don't care for comparison result here; the previous value will be stored into value anyway.
        // Also we don't care for rbx and rcx values, they just have to be equal to rax and rdx before cmpxchg16b.
        __asm__ __volatile__
        (
            "movq %%rbx, %%rax\n\t"
            "movq %%rcx, %%rdx\n\t"
            "lock; cmpxchg16b %[storage]\n\t"
            : "=&a" (value_bits[0]), "=&d" (value_bits[1])
            : [storage] "m" (storage)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

#if defined(BOOST_HAS_INT128)
        value = static_cast< storage_type >(value_bits[0]) | (static_cast< storage_type >(value_bits[1]) << 64u);
#else
        BOOST_ATOMIC_DETAIL_MEMCPY(&value, value_bits, sizeof(value));
#endif

#else // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        // We don't care for comparison result here; the previous value will be stored into value anyway.
        // Also we don't care for rbx and rcx values, they just have to be equal to rax and rdx before cmpxchg16b.
        __asm__ __volatile__
        (
            "movq %%rbx, %%rax\n\t"
            "movq %%rcx, %%rdx\n\t"
            "lock; cmpxchg16b %[storage]\n\t"
            : "=&A" (value)
            : [storage] "m" (storage)
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

#endif // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);

        return value;
    }

    static BOOST_FORCEINLINE bool compare_exchange_strong(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, success_order);

#if defined(__clang__)

        // Clang cannot allocate rax:rdx register pairs but it has sync intrinsics
        storage_type old_expected = expected;
        expected = __sync_val_compare_and_swap(&storage, old_expected, desired);
        const bool success = expected == old_expected;

#elif defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        // Some compilers can't allocate rax:rdx register pair either but also don't support 128-bit __sync_val_compare_and_swap
        bool success;
        __asm__ __volatile__
        (
            "lock; cmpxchg16b %[dest]\n\t"
            "sete %[success]\n\t"
            : [dest] "+m" (storage), "+a" (reinterpret_cast< aliasing_uint64_t* >(&expected)[0]), "+d" (reinterpret_cast< aliasing_uint64_t* >(&expected)[1]), [success] "=q" (success)
            : "b" (reinterpret_cast< const aliasing_uint64_t* >(&desired)[0]), "c" (reinterpret_cast< const aliasing_uint64_t* >(&desired)[1])
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

#else // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        bool success;

#if defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchg16b %[dest]\n\t"
            : "+A" (expected), [dest] "+m" (storage), "=@ccz" (success)
            : "b" (reinterpret_cast< const aliasing_uint64_t* >(&desired)[0]), "c" (reinterpret_cast< const aliasing_uint64_t* >(&desired)[1])
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#else // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)
        __asm__ __volatile__
        (
            "lock; cmpxchg16b %[dest]\n\t"
            "sete %[success]\n\t"
            : "+A" (expected), [dest] "+m" (storage), [success] "=qm" (success)
            : "b" (reinterpret_cast< const aliasing_uint64_t* >(&desired)[0]), "c" (reinterpret_cast< const aliasing_uint64_t* >(&desired)[1])
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );
#endif // defined(BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS)

#endif // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, (success ? success_order : failure_order));

        return success;
    }

    static BOOST_FORCEINLINE bool compare_exchange_weak(
        storage_type volatile& storage, storage_type& expected, storage_type desired, memory_order success_order, memory_order failure_order) noexcept
    {
        return compare_exchange_strong(storage, expected, desired, success_order, failure_order);
    }

    static BOOST_FORCEINLINE storage_type exchange(storage_type volatile& storage, storage_type v, memory_order order) noexcept
    {
        BOOST_ATOMIC_DETAIL_TSAN_RELEASE(&storage, order);

        storage_type old_value;

#if defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)
        std::uint64_t old_bits[2];
        __asm__ __volatile__
        (
            "movq %[dest_lo], %%rax\n\t"
            "movq %[dest_hi], %%rdx\n\t"
            ".align 16\n\t"
            "1: lock; cmpxchg16b %[dest_lo]\n\t"
            "jne 1b\n\t"
            : [dest_lo] "+m" (storage), [dest_hi] "+m" (reinterpret_cast< volatile aliasing_uint64_t* >(&storage)[1]), "=&a" (old_bits[0]), "=&d" (old_bits[1])
            : "b" (reinterpret_cast< const aliasing_uint64_t* >(&v)[0]), "c" (reinterpret_cast< const aliasing_uint64_t* >(&v)[1])
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

#if defined(BOOST_HAS_INT128)
        old_value = static_cast< storage_type >(old_bits[0]) | (static_cast< storage_type >(old_bits[1]) << 64u);
#else
        BOOST_ATOMIC_DETAIL_MEMCPY(&old_value, old_bits, sizeof(old_value));
#endif

#else // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        __asm__ __volatile__
        (
            "movq %[dest_lo], %%rax\n\t"
            "movq %[dest_hi], %%rdx\n\t"
            ".align 16\n\t"
            "1: lock; cmpxchg16b %[dest_lo]\n\t"
            "jne 1b\n\t"
            : "=&A" (old_value), [dest_lo] "+m" (storage), [dest_hi] "+m" (reinterpret_cast< volatile aliasing_uint64_t* >(&storage)[1])
            : "b" (reinterpret_cast< const aliasing_uint64_t* >(&v)[0]), "c" (reinterpret_cast< const aliasing_uint64_t* >(&v)[1])
            : BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "memory"
        );

#endif // defined(BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS)

        BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(&storage, order);

        return old_value;
    }
};

template< bool Signed, bool Interprocess >
struct core_arch_operations< 16u, Signed, Interprocess > :
    public core_operations_cas_based< gcc_dcas_x86_64< Signed, Interprocess > >
{
};

#endif // defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG16B)

} // namespace detail
} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_DETAIL_CORE_ARCH_OPS_GCC_X86_HPP_INCLUDED_
