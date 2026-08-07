/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2012 Hartmut Kaiser
 * Copyright (c) 2014-2018, 2020-2025 Andrey Semashev
 */
/*!
 * \file   atomic/detail/config.hpp
 *
 * This header defines configuraion macros for Boost.Atomic
 */

#ifndef BOOST_ATOMIC_DETAIL_CONFIG_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_CONFIG_HPP_INCLUDED_

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if defined(__SANITIZE_THREAD__)
#define BOOST_ATOMIC_DETAIL_TSAN
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define BOOST_ATOMIC_DETAIL_TSAN
#endif
#endif

// Instrumentation macros to make TSan aware of the memory order semantics of asm blocks
#if defined(BOOST_ATOMIC_DETAIL_TSAN)
extern "C" {
void __tsan_acquire(void*);
void __tsan_release(void*);
} // extern "C"
#define BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(ptr, mo) \
    { if ((static_cast< unsigned int >(mo) & static_cast< unsigned int >(memory_order_acquire)) != 0u) __tsan_acquire((void*)(ptr)); }
#define BOOST_ATOMIC_DETAIL_TSAN_RELEASE(ptr, mo) \
    { if ((static_cast< unsigned int >(mo) & static_cast< unsigned int >(memory_order_release)) != 0u) __tsan_release((void*)(ptr)); }
#else // defined(BOOST_ATOMIC_DETAIL_TSAN)
#define BOOST_ATOMIC_DETAIL_TSAN_ACQUIRE(ptr, mo)
#define BOOST_ATOMIC_DETAIL_TSAN_RELEASE(ptr, mo)
#endif // defined(BOOST_ATOMIC_DETAIL_TSAN)

#if defined(__CUDACC__)
// nvcc does not support alternatives ("q,m") in asm statement constraints
#define BOOST_ATOMIC_DETAIL_NO_ASM_CONSTRAINT_ALTERNATIVES
// nvcc does not support condition code register ("cc") clobber in asm statements
#define BOOST_ATOMIC_DETAIL_NO_ASM_CLOBBER_CC
#endif

#if !defined(BOOST_ATOMIC_DETAIL_NO_ASM_CLOBBER_CC)
#define BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC "cc"
#define BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "cc",
#else
#define BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
#define BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA
#endif

#if (defined(__i386__) || defined(__x86_64__)) && (defined(__clang__) || (defined(BOOST_GCC) && BOOST_GCC < 40500) || defined(__SUNPRO_CC))
// This macro indicates that the compiler does not support allocating eax:edx or rax:rdx register pairs ("A") in asm blocks
#define BOOST_ATOMIC_DETAIL_X86_NO_ASM_AX_DX_PAIRS
#endif

#if defined(__i386__) && (defined(__PIC__) || defined(__PIE__)) && !(defined(__clang__) || (defined(BOOST_GCC) && BOOST_GCC >= 50100))
// This macro indicates that asm blocks should preserve ebx value unchanged. Some compilers are able to maintain ebx themselves
// around the asm blocks. For those compilers we don't need to save/restore ebx in asm blocks.
#define BOOST_ATOMIC_DETAIL_X86_ASM_PRESERVE_EBX
#endif

#if defined(BOOST_NO_CXX11_ALIGNAS) ||\
    (defined(BOOST_GCC) && BOOST_GCC < 40900) ||\
    (defined(BOOST_MSVC) && BOOST_MSVC < 1910 && defined(_M_IX86))
// gcc prior to 4.9 doesn't support alignas with a constant expression as an argument.
// MSVC 14.0 does support alignas, but in 32-bit mode emits "error C2719: formal parameter with requested alignment of N won't be aligned" for N > 4,
// when aligned types are used in function arguments, even though the std::max_align_t type has alignment of 8.
#define BOOST_ATOMIC_DETAIL_NO_CXX11_ALIGNAS
#endif

// Enable pointer/reference casts between storage and value when possible.
// Note: Despite that MSVC does not employ strict aliasing rules for optimizations
// and does not require an explicit markup for types that may alias, we still don't
// enable the optimization for this compiler because at least MSVC-8 and 9 are known
// to generate broken code sometimes when casts are used.
#define BOOST_ATOMIC_DETAIL_MAY_ALIAS BOOST_MAY_ALIAS
#if !defined(BOOST_NO_MAY_ALIAS)
#define BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS
#endif

#if defined(__GCC_ASM_FLAG_OUTPUTS__)
// The compiler supports output values in flag registers.
// See: https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html, Section 6.44.3.
#define BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS
#endif

#if defined(__has_builtin)
#if __has_builtin(__builtin_constant_p)
#define BOOST_ATOMIC_DETAIL_IS_CONSTANT(x) __builtin_constant_p(x)
#endif
#if __has_builtin(__builtin_clear_padding)
#define BOOST_ATOMIC_DETAIL_CLEAR_PADDING(x) __builtin_clear_padding(x)
#elif __has_builtin(__builtin_zero_non_value_bits)
#define BOOST_ATOMIC_DETAIL_CLEAR_PADDING(x) __builtin_zero_non_value_bits(x)
#endif
#endif

#if !defined(BOOST_ATOMIC_DETAIL_IS_CONSTANT) && defined(__GNUC__)
#define BOOST_ATOMIC_DETAIL_IS_CONSTANT(x) __builtin_constant_p(x)
#endif

#if !defined(BOOST_ATOMIC_DETAIL_IS_CONSTANT)
#define BOOST_ATOMIC_DETAIL_IS_CONSTANT(x) false
#endif

#if !defined(BOOST_ATOMIC_DETAIL_CLEAR_PADDING) && defined(BOOST_MSVC) && BOOST_MSVC >= 1927
// Note that as of MSVC 19.29 this intrinsic does not clear padding in unions:
// https://developercommunity.visualstudio.com/t/__builtin_zero_non_value_bits-does-not-c/1551510
#define BOOST_ATOMIC_DETAIL_CLEAR_PADDING(x) __builtin_zero_non_value_bits(x)
#endif

#if !defined(BOOST_ATOMIC_DETAIL_CLEAR_PADDING)
#define BOOST_ATOMIC_NO_CLEAR_PADDING
#define BOOST_ATOMIC_DETAIL_CLEAR_PADDING(x)
#endif

#if (defined(__BYTE_ORDER__) && defined(__FLOAT_WORD_ORDER__) && __BYTE_ORDER__ == __FLOAT_WORD_ORDER__) ||\
    defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_AMD64) || defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
// This macro indicates that integer and floating point endianness is the same
#define BOOST_ATOMIC_DETAIL_INT_FP_ENDIAN_MATCH
#endif

#endif // BOOST_ATOMIC_DETAIL_CONFIG_HPP_INCLUDED_
