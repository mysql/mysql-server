/*
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 *
 * (C) Copyright 2013 Tim Blechmann
 * (C) Copyright 2013, 2020-2025 Andrey Semashev
 */

#ifndef BOOST_ATOMIC_THREAD_PAUSE_HPP_INCLUDED_
#define BOOST_ATOMIC_THREAD_PAUSE_HPP_INCLUDED_

#include <boost/atomic/detail/config.hpp>
#if defined(_MSC_VER)
#include <boost/atomic/detail/ops_msvc_common.hpp>
#endif
#include <boost/atomic/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if defined(_MSC_VER)
// (sigh, shake head) _M_ARM64EC and _M_AMD64 may be defined both
// https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
#if defined(_M_ARM64) || defined(_M_ARM64EC)
extern "C" void __isb(unsigned int);
#if defined(BOOST_MSVC)
#pragma intrinsic(__isb)
#endif
#elif defined(_M_ARM)
extern "C" void __yield(void);
#if defined(BOOST_MSVC)
#pragma intrinsic(__yield)
#endif
#elif defined(_M_AMD64) || defined(_M_IX86)
extern "C" void _mm_pause(void);
#if defined(BOOST_MSVC)
#pragma intrinsic(_mm_pause)
#endif
#endif
#endif

#if defined(sun) || defined(__sun)
// Avoid including synch.h
extern "C" void smt_pause(void);
#endif

namespace boost {
namespace atomics {

//! The function pauses for a number of CPU cycles, potentially freeing CPU resources, allowing sibling threads to progress. May be a no-op.
BOOST_FORCEINLINE void thread_pause() noexcept
{
#if defined(_MSC_VER)

    BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();
#if defined(_M_ARM64) || defined(_M_ARM64EC)
    __isb(0xF); // ISB SY
#elif defined(_M_ARM)
    __yield();
#elif defined(_M_AMD64) || defined(_M_IX86)
    _mm_pause();
#endif
    BOOST_ATOMIC_DETAIL_COMPILER_BARRIER();

#elif defined(__GNUC__)

#if defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__("pause" : : : "memory");
#elif defined(__aarch64__)
    // https://github.com/rust-lang/rust/commit/c064b6560b7ce0adeb9bbf5d7dcf12b1acb0c807
    // https://web.archive.org/web/20231004132033/https://bugs.java.com/bugdatabase/view_bug.do?bug_id=8258604
    __asm__ __volatile__("isb" : : : "memory");
#elif (defined(__ARM_ARCH) && __ARM_ARCH >= 8) || defined(__ARM_ARCH_8A__)
    __asm__ __volatile__("yield" : : : "memory");
#elif (defined(__POWERPC__) || defined(__PPC__))
#if defined(__APPLE__)
    __asm__ __volatile__("or r27,r27,r27" : : : "memory"); // yield pseudo-instruction
#else
    __asm__ __volatile__("or 27,27,27" : : : "memory"); // yield pseudo-instruction
#endif
#elif defined(__riscv) && (__riscv_xlen == 64)
#if defined(__riscv_zihintpause)
    __asm__ __volatile__("pause" : : : "memory");
#else
    // Encoding of the pause instruction
    __asm__ __volatile__ (".4byte 0x100000F" : : : "memory");
#endif
#elif defined(sun) || defined(__sun)
    smt_pause();
#endif

#elif defined(sun) || defined(__sun)

    smt_pause();

#endif
}

} // namespace atomics
} // namespace boost

#include <boost/atomic/detail/footer.hpp>

#endif // BOOST_ATOMIC_THREAD_PAUSE_HPP_INCLUDED_
