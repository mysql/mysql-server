#ifndef MY_COMPILER_INCLUDED
#define MY_COMPILER_INCLUDED

/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file include/my_compiler.h
  Header for compiler-dependent features.

  Intended to contain a set of reusable wrappers for preprocessor
  macros, attributes, pragmas, and any other features that are
  specific to a target compiler.
*/

#ifndef MYSQL_ABI_CHECK
#include <assert.h>
#include <stddef.h> /* size_t */
#endif

#include "my_config.h"

#include "mysql/attribute.h"

/*
  The macros below are borrowed from include/linux/compiler.h in the
  Linux kernel. Use them to indicate the likelihood of the truthfulness
  of a condition. This serves two purposes - newer versions of gcc will be
  able to optimize for branch predication, which could yield significant
  performance gains in frequently executed sections of the code, and the
  other reason to use them is for documentation
*/
#ifdef HAVE_BUILTIN_EXPECT

// likely/unlikely are likely to clash with other symbols, do not #define
#if defined(__cplusplus)
constexpr bool likely(bool expr) { return __builtin_expect(expr, true); }
constexpr bool unlikely(bool expr) { return __builtin_expect(expr, false); }
#else
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#endif

#else /* HAVE_BUILTIN_EXPECT */

#if defined(__cplusplus)
constexpr bool likely(bool expr) { return expr; }
constexpr bool unlikely(bool expr) { return expr; }
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#endif /* HAVE_BUILTIN_EXPECT */

/* Communicate to the compiler the unreachability of the code. */
#ifdef HAVE_BUILTIN_UNREACHABLE
#define MY_ASSERT_UNREACHABLE() __builtin_unreachable()
#else
#define MY_ASSERT_UNREACHABLE() \
  do {                          \
    assert(0);                  \
  } while (0)
#endif

/* Visual Studio requires '__inline' for C code */
#if !defined(__cplusplus) && defined(_MSC_VER)
#define inline __inline
#endif

/* Provide __func__ macro definition for Visual Studio. */
#if defined(_MSC_VER)
#define __func__ __FUNCTION__
#endif

#if defined(_MSC_VER)
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

#if defined(_MSC_VER)
#define NO_INLINE __declspec(noinline)
#else
#define NO_INLINE __attribute__((noinline))
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef SUPPRESS_UBSAN
// clang -fsanitize=undefined
#if defined(HAVE_UBSAN) && defined(__clang__)
#define SUPPRESS_UBSAN MY_ATTRIBUTE((no_sanitize("undefined")))
// gcc -fsanitize=undefined
#elif defined(HAVE_UBSAN) && __has_attribute(no_sanitize_undefined)
#define SUPPRESS_UBSAN MY_ATTRIBUTE((no_sanitize_undefined))
#else
#define SUPPRESS_UBSAN
#endif
#endif /* SUPPRESS_UBSAN */

#ifndef SUPPRESS_TSAN
#if defined(HAVE_TSAN) && defined(__clang__)
#define SUPPRESS_TSAN MY_ATTRIBUTE((no_sanitize("thread")))
#elif defined(HAVE_TSAN) && __has_attribute(no_sanitize_thread)
#define SUPPRESS_TSAN MY_ATTRIBUTE((no_sanitize_thread))
#else
#define SUPPRESS_TSAN
#endif
#endif /* SUPPRESS_TSAN */

#ifdef _WIN32
#define STDCALL __stdcall
#else
#define STDCALL
#endif

/**
 * stringify parameters for C99/C++11 _Pragma().
 */
#define MY_COMPILER_CPP11_PRAGMA(X) _Pragma(#X)
/**
 * pass parameters to MSVC's __pragma() as is.
 */
#define MY_COMPILER_MSVC_PRAGMA(X) __pragma(X)

// enable compiler specified 'diagnostic' pragmas.
//
// 1. clang on windows defines both clang and msvc pragmas and generates the
// same warnings
// 2. clang defines both __clang__ and __GNUC__, but doesn't support all GCC
// warnings with the same name
//
//         +---------------------+
//         | enabled diagnostics |
//         +------+-------+------+
//         |  gcc | clang | msvc |
// +-------+------+-------+------+
// | gcc   |   x  |   -   |   -  |
// | clang |   -  |   x   |   x  |
// | msvc  |   -  |   -   |   x  |
// +-------+------+-------+------+
//    ^^^
//     +----- current compiler
//
// suppressions that aren't supported by the compiler are disabled to avoid
// "unsupported pragmas" warnings:
//
// @code
// // on GCC, clang-specific diagnostic pragmas are disabled
// MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wdocumentation")
// @endcode

#if defined(__clang__)
#define MY_COMPILER_CLANG_DIAGNOSTIC_PUSH() \
  MY_COMPILER_CPP11_PRAGMA(clang diagnostic push)
#define MY_COMPILER_CLANG_DIAGNOSTIC_POP() \
  MY_COMPILER_CPP11_PRAGMA(clang diagnostic pop)
/**
 * ignore a compiler warning.
 *
 * @param X warning option to disable, must be quoted like "-Wdocumentation"
 */
#define MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE(X) \
  MY_COMPILER_CPP11_PRAGMA(clang diagnostic ignored X)
/**
 * turn a compiler warning into an error.
 *
 * @param X warning option to turn into an error, must be a quoted string like
 * "-Wdocumentation"
 */
#define MY_COMPILER_CLANG_DIAGNOSTIC_ERROR(X) \
  MY_COMPILER_CPP11_PRAGMA(clang diagnostic error X)

#elif defined(__GNUC__)
#define MY_COMPILER_GCC_DIAGNOSTIC_PUSH() \
  MY_COMPILER_CPP11_PRAGMA(GCC diagnostic push)
#define MY_COMPILER_GCC_DIAGNOSTIC_POP() \
  MY_COMPILER_CPP11_PRAGMA(GCC diagnostic pop)
/**
 * ignore a compiler warning.
 *
 * @param X warning option to disable, must be quoted like "-Wdocumentation"
 */
#define MY_COMPILER_GCC_DIAGNOSTIC_IGNORE(X) \
  MY_COMPILER_CPP11_PRAGMA(GCC diagnostic ignored X)
/**
 * turn a compiler warning into an error.
 *
 * @param X warning option to turn into an error, must be quoted like
 * "-Wdocumentation"
 */
#define MY_COMPILER_GCC_DIAGNOSTIC_ERROR(X) \
  MY_COMPILER_CPP11_PRAGMA(GCC diagnostic error X)

#endif  // defined(__GNUC__)

#if defined(_MSC_VER)
#define MY_COMPILER_MSVC_DIAGNOSTIC_PUSH() \
  MY_COMPILER_MSVC_PRAGMA(warning(push))
#define MY_COMPILER_MSVC_DIAGNOSTIC_POP() MY_COMPILER_MSVC_PRAGMA(warning(pop))
/**
 * ignore a compiler warning.
 *
 * @param X warning number to disable
 */
#define MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(X) \
  MY_COMPILER_MSVC_PRAGMA(warning(disable : X))
#define MY_COMPILER_MSVC_DIAGNOSTIC_ERROR(X) \
  MY_COMPILER_MSVC_PRAGMA(warning(error : X))

#endif  // defined(_MSC_VER)

#if !defined(MY_COMPILER_CLANG_DIAGNOSTIC_ERROR)
#define MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE(X)
#define MY_COMPILER_CLANG_DIAGNOSTIC_ERROR(X)
#endif

#if !defined(MY_COMPILER_GCC_DIAGNOSTIC_ERROR)
#define MY_COMPILER_GCC_DIAGNOSTIC_IGNORE(X)
#define MY_COMPILER_GCC_DIAGNOSTIC_ERROR(X)
#endif

#if !defined(MY_COMPILER_MSVC_DIAGNOSTIC_ERROR)
#define MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(X)
#define MY_COMPILER_MSVC_DIAGNOSTIC_ERROR(X)
#endif

/**
 * @def MY_COMPILER_DIAGNOSTIC_PUSH()
 *
 * save the compiler's diagnostic (enabled warnings, errors, ...) state
 *
 * @see MY_COMPILER_DIAGNOSTIC_POP()
 */

/**
 * @def MY_COMPILER_DIAGNOSTIC_POP()
 *
 * restore the compiler's diagnostic (enabled warnings, errors, ...) state
 *
 * @see MY_COMPILER_DIAGNOSTIC_PUSH()
 */

#if defined(__clang__)
#define MY_COMPILER_DIAGNOSTIC_PUSH() MY_COMPILER_CLANG_DIAGNOSTIC_PUSH()
#define MY_COMPILER_DIAGNOSTIC_POP() MY_COMPILER_CLANG_DIAGNOSTIC_POP()
#elif defined(__GNUC__)
#define MY_COMPILER_DIAGNOSTIC_PUSH() MY_COMPILER_GCC_DIAGNOSTIC_PUSH()
#define MY_COMPILER_DIAGNOSTIC_POP() MY_COMPILER_GCC_DIAGNOSTIC_POP()
#elif defined(_MSC_VER)
#define MY_COMPILER_DIAGNOSTIC_PUSH() MY_COMPILER_MSVC_DIAGNOSTIC_PUSH()
#define MY_COMPILER_DIAGNOSTIC_POP() MY_COMPILER_MSVC_DIAGNOSTIC_POP()
#else
#define MY_COMPILER_DIAGNOSTIC_PUSH()
#define MY_COMPILER_DIAGNOSTIC_POP()
#endif

/**
 * ignore -Wdocumentation compiler warnings for \@tparam.
 *
 * @code
 * MY_COMPILER_DIAGNOSTIC_PUSH()
 * MY_COMPILER_CLANG_WORKAROUND_TPARAM_DOCBUG()
 * ...
 * MY_COMPILER_DIAGNOSTIC_POP()
 * @endcode
 *
 * @see MY_COMPILER_DIAGNOSTIC_PUSH()
 * @see MY_COMPILER_DIAGNOSTIC_POP()
 *
 * allows to work around false positives -Wdocumentation warnings like:
 *
 * - \@tparam and explicitly instantiated templates
 *   https://bugs.llvm.org/show_bug.cgi?id=35144
 *
 */
#define MY_COMPILER_CLANG_WORKAROUND_TPARAM_DOCBUG() \
  MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wdocumentation")

/**
 * ignore -Wdocumentation compiler warnings for \@see \@ref
 *
 * @code
 * MY_COMPILER_DIAGNOSTIC_PUSH()
 * MY_COMPILER_CLANG_WORKAROUND_REF_DOCBUG()
 * ...
 * MY_COMPILER_DIAGNOSTIC_POP()
 * @endcode
 *
 * @see MY_COMPILER_DIAGNOSTIC_PUSH()
 * @see MY_COMPILER_DIAGNOSTIC_POP()
 *
 * allows to work around false positives -Wdocumentation warnings like:
 *
 * - \@sa \@ref
 * - \@see \@ref
 * - \@return \@ref
 *   https://bugs.llvm.org/show_bug.cgi?id=38905
 *
 */
#define MY_COMPILER_CLANG_WORKAROUND_REF_DOCBUG() \
  MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wdocumentation")

/**
 * ignore -Wunused-variable compiler warnings for \@see \@ref
 *
 * @code
 * MY_COMPILER_DIAGNOSTIC_PUSH()
 * MY_COMPILER_CLANG_WORKAROUND_FALSE_POSITIVE_UNUSED_VARIABLE_WARNING()
 * ...
 * MY_COMPILER_DIAGNOSTIC_POP()
 * @endcode
 *
 * @see MY_COMPILER_DIAGNOSTIC_PUSH()
 * @see MY_COMPILER_DIAGNOSTIC_POP()
 *
 * allows to work around false positives -Wunused-variable warnings like:
 *
 * - \@sa \@ref
 * - \@see \@ref
 * - \@return \@ref
 *   https://bugs.llvm.org/show_bug.cgi?id=46035
 *
 */
#define MY_COMPILER_CLANG_WORKAROUND_FALSE_POSITIVE_UNUSED_VARIABLE_WARNING() \
  MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Wunused-variable")

#endif /* MY_COMPILER_INCLUDED */
