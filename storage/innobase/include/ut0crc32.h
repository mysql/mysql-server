/*****************************************************************************

Copyright (c) 2011, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0crc32.h
 CRC32 implementation

 Created Aug 10, 2011 Vasil Dimov
 *******************************************************/

#ifndef ut0crc32_h
#define ut0crc32_h

#include "univ.i"

/*
- 1. some things depend on the compiling environment
  - is it a compiler for which we even know how to use intrinsics?
  - is it x86, arm64, or mac-arm?
- 2. some depend on runtime environment
  - is it x86 which has crc32?
  - is it x86 which has pcmul?
  - is it arm which has crc32?
  - is it arm which has pcmul?
- 3. some depend on the runtime usage:
  - is it 0.5kb redo buffer, 16kb page, or something else?
  - do you need the variant with swapped byte order?
*/

#if defined(__GNUC__) && defined(__x86_64__) || defined(_WIN32)
#define CRC32_x86_64
#ifdef _WIN32
#define CRC32_x86_64_WIN
#else /* _WIN32 */
#define CRC32_x86_64_DEFAULT
#endif /* _WIN32 */
#elif defined(__aarch64__) && defined(__GNUC__)
#define CRC32_ARM64
#ifdef APPLE_ARM
#define CRC32_ARM64_APPLE
#else /* APPLE_ARM */
#define CRC32_ARM64_DEFAULT
#endif /* APPLE_ARM */
#else
#define CRC32_DEFAULT
#endif /* defined(__aarch64__) && defined(__GNUC__) */

/* At this point we have classified the system statically into exactly one of
the possible cases:

CRC32_x86_64
    An environment in which we can use `cpuid` instruction to detect if it has
    support for crc32 and pclmul instructions, which (if available) can be used
    via _mm_crc32_u64 and _mm_clmulepi64_si128 respectively exposed by
    nmmintrin.h and wmmintrin.h.
    This is narrowed further into one of:

    CRC32_x86_64_WIN
        An environment which seems to be like Visual Studio, so we expect
        intrin.h header exposing `__cpuid`, which we can use instead of inline
        assembly, which is good as Visual Studio dialect of asm is different.
        Also, __attribute__(target(...)) probably doesn't work on it.
    CRC32_x86_64_DEFAULT
        An environment which seems to be like gcc or clang, and thus we can use
        inline assembly to get `cpuid`.
        Also, we can/have to use __attribute__(target(...)) on functions which
        use intrinsics, and may need to use __attribute__(flatten) at top level
        to ensure that the run-time selection of target-specific variant of the
        function happens just once at the top, not in every leaf, which would
        break inlining and optimizations.
CRC32_ARM64
    An environment in which it is probable that __crc32cd and vmull_p64 could be
    used for hardware accelerated crc32 and polynomial multiplication
    computations, respectively. However we might need to perform some runtime
    checks via getauxval() to see if this particular processor on which we run
    supports them.
    This is narrowed further into one of:

    CRC32_ARM64_APPLE
        An environment which seems to be like Apple's M1 processor, and we don't
        expect to find sys/auxv.h header which defines getauxval() on it, yet we
        also expect the __crc32cd and vmull_p64 to "just work" on it, without
        checking getauxval().

    CRC32_ARM64_DEFAULT
        An environment which seems to be like a "regular" ARM64. Note that this
        is not very specific term, as there are ARMv7-A, ARMv8-A, and the later
        has two execution states AArch32 and AArch64. FWIW we use __aarch64__ to
        detect this case. We still need to call getauxval() to see if particular
        instruction set is available. We assume we run in 64-bit execution state
        thus we use AT_HWCAP (as opposed to AT_HWCAP2).
CRC32_DEFAULT
    An environment in which we don't even know how to ask if the hardware
    supports crc32 or polynomial multiplication and even if it does we don't
    know how to ask it to do it anyway. We use software implementation of crc32.
*/

/** Initializes the data structures used by ut_crc32*(). Does not do any
 allocations, would not hurt if called twice, but would be pointless. */
void ut_crc32_init();

/** The CRC-32C polynomial without the implicit highest 1 at x^32 */
constexpr uint32_t CRC32C_POLYNOMIAL{0x1EDC6F41};

/** Calculates CRC32.
 @param ptr - data over which to calculate CRC32.
 @param len - data length in bytes.
 @return calculated hash */
typedef uint32_t (*ut_crc32_func_t)(const byte *ptr, size_t len);

/** Pointer to standard-compliant CRC32-C (using the GF(2) primitive polynomial
0x11EDC6F41) calculation function picked by ut_crc32_init() as the fastest
implementation for the current environment. */
extern ut_crc32_func_t ut_crc32;

/** Calculates CRC32 using legacy algorithm, which uses big-endian byte ordering
when converting byte sequence to integers - flips each full aligned 8-byte chunk
within the buf, but not the initial and trailing unaligned fragments.
ut_crc32_init() needs to be called at least once before calling this function.
@param[in]      buf     data over which to calculate CRC32
@param[in]      len     data length
@return calculated hash */
uint32_t ut_crc32_legacy_big_endian(const byte *buf, size_t len);

/** Flag that tells whether the CPU supports CRC32 or not. */
extern bool ut_crc32_cpu_enabled;

/** Flag that tells whether the CPU supports polynomial multiplication or not.*/
extern bool ut_poly_mul_cpu_enabled;

#endif /* ut0crc32_h */
