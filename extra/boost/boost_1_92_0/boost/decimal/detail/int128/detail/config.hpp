// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_CONFIG_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_CONFIG_HPP

#if defined(BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION) && !defined(BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE)
#  define BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE
#endif

// Use 128-bit integers
#if defined(BOOST_HAS_INT128) || (defined(__SIZEOF_INT128__) && !defined(_MSC_VER)) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_BUILTIN_INT128)

#define BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

#define BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR constexpr

namespace boost {
namespace int128 {
namespace detail {

// Avoids pedantic warnings
#ifdef __GNUC__

__extension__ using builtin_i128 = __int128 ;
__extension__ using builtin_u128 = unsigned __int128 ;

#else

using builtin_i128 = __int128 ;
using builtin_u128 = unsigned __int128;

#endif

} // namespace detail
} // namespace int128
} // namespace boost

#elif __has_include(<__msvc_int128.hpp>) && _MSVC_LANG >= 202002L

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE
#include <__msvc_int128.hpp>
#endif

#define BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

#define BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR inline

namespace boost {
namespace int128 {
namespace detail {

using builtin_i128 = std::_Signed128;
using builtin_u128 = std::_Unsigned128;

} // namespace detail
} // namespace int128
} // namespace boost

#endif // builtin 128-bit detection

// Determine endianness
#if defined(_WIN32)

#define BOOST_DECIMAL_DETAIL_INT128_ENDIAN_BIG_BYTE 0
#define BOOST_DECIMAL_DETAIL_INT128_ENDIAN_LITTLE_BYTE 1

#elif defined(__BYTE_ORDER__)

#define BOOST_DECIMAL_DETAIL_INT128_ENDIAN_BIG_BYTE (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BOOST_DECIMAL_DETAIL_INT128_ENDIAN_LITTLE_BYTE (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#else

#error Could not determine endian type. Please file an issue at https://github.com/cppalliance/INT128 with your architecture

#endif // Determine endianness

// Is constant evaluated detection
#ifdef __cpp_lib_is_constant_evaluated
#  define BOOST_DECIMAL_DETAIL_INT128_HAS_IS_CONSTANT_EVALUATED
#endif

#ifdef __has_builtin
#  if __has_builtin(__builtin_is_constant_evaluated)
#    define BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#  endif
#endif

//
// MSVC also supports __builtin_is_constant_evaluated if it's recent enough:
//
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 192528326)
#  define BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#endif

//
// As does GCC-9:
//
#if defined(__GNUC__) && (__GNUC__ >= 9) && !defined(BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN_IS_CONSTANT_EVALUATED)
#  define BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#endif

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_IS_CONSTANT_EVALUATED) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))
#  define BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x) std::is_constant_evaluated()
#elif defined(BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN_IS_CONSTANT_EVALUATED) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA))
#  define BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x) __builtin_is_constant_evaluated()
#else
#  define BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x) false
#  define BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION
#endif

// https://github.com/llvm/llvm-project/issues/55638
#if defined(__clang__) && __cplusplus > 202002L && __clang_major__ < 17
#  undef BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED
#  define BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x) false
#  define BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION
#endif

#if defined(_MSC_VER)
#  define BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#  define BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE __attribute__((always_inline)) inline
#else
#  define BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE inline
#endif

#ifdef __x86_64__

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE
#  include <x86intrin.h>
#  include <emmintrin.h>
#endif

#  ifdef __ADX__
#    define BOOST_DECIMAL_DETAIL_INT128_ADD_CARRY _addcarryx_u64
#    define BOOST_DECIMAL_DETAIL_INT128_SUB_BORROW _subborrow_u64
#  else
#    define BOOST_DECIMAL_DETAIL_INT128_ADD_CARRY _addcarry_u64
#    define BOOST_DECIMAL_DETAIL_INT128_SUB_BORROW _subborrow_u64
#  endif

#elif defined(_M_AMD64)

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE
#  include <intrin.h>
#endif

#  ifdef __ADX__
#    define BOOST_DECIMAL_DETAIL_INT128_ADD_CARRY _addcarryx_u64
#    define BOOST_DECIMAL_DETAIL_INT128_SUB_BORROW _subborrow_u64
#  else
#    define BOOST_DECIMAL_DETAIL_INT128_ADD_CARRY _addcarry_u64
#    define BOOST_DECIMAL_DETAIL_INT128_SUB_BORROW _subborrow_u64
#  endif

#elif defined(__i386__)

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE
#  include <emmintrin.h>
#endif

#elif defined(_M_IX86)

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE
#  include <intrin.h>
#endif

#endif // Platform macros

// The builtin is only constexpr from clang-7 or GCC-10
#ifdef __has_builtin
#  if __has_builtin(__builtin_sub_overflow) && ((defined(__clang__) && __clang_major__ >= 7) || (defined(__GNUC__) && __GNUC__ >= 10))
#    define BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN_SUB_OVERFLOW
#  endif
#  if __has_builtin(__builtin_add_overflow) && ((defined(__clang__) && __clang_major__ >= 7) || (defined(__GNUC__) && __GNUC__ >= 10))
#    define BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN_ADD_OVERFLOW
#  endif
#endif

#if defined(__cpp_if_constexpr) && __cpp_if_constexpr >= 201606L
#  define BOOST_DECIMAL_DETAIL_INT128_HAS_IF_CONSTEXPR
#endif // if constexpr detection

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE
#include <cassert>
#endif

#define BOOST_DECIMAL_DETAIL_INT128_ASSERT(x) assert(x)
#define BOOST_DECIMAL_DETAIL_INT128_ASSERT_MSG(expr, msg) assert((expr)&&(msg))

#ifdef _MSC_VER
#  define BOOST_DECIMAL_DETAIL_INT128_ASSUME(expr) __assume(expr)
#elif defined(__clang__)
#  define BOOST_DECIMAL_DETAIL_INT128_ASSUME(expr) __builtin_assume(expr)
#elif defined(__GNUC__)
#  if __GNUC__ >= 5 && __GNUC__ < 13
#    define BOOST_DECIMAL_DETAIL_INT128_ASSUME(expr) if (expr) {} else { __builtin_unreachable(); }
#  else
#    define BOOST_DECIMAL_DETAIL_INT128_ASSUME(expr) __attribute__((assume(expr)))
#  endif
#elif defined(__has_cpp_attribute)
#  if __has_cpp_attribute(assume)
#    define BOOST_DECIMAL_DETAIL_INT128_ASSUME(expr) [[assume(expr)]]
#  else
#    define BOOST_DECIMAL_DETAIL_INT128_ASSUME(expr) BOOST_DECIMAL_DETAIL_INT128_ASSERT(expr)
#  endif
#else
#  define BOOST_DECIMAL_DETAIL_INT128_ASSUME(expr) BOOST_DECIMAL_DETAIL_INT128_ASSERT(expr)
#endif

#if defined(__has_builtin)
#define BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(x) __has_builtin(x)
#else
#define BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(x) false
#endif

#if BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_expect)
#  define BOOST_DECIMAL_DETAIL_INT128_LIKELY(x) __builtin_expect(x, 1)
#  define BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(x) __builtin_expect(x, 0)
#else
#  define BOOST_DECIMAL_DETAIL_INT128_LIKELY(x) x
#  define BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(x) x
#endif

#if !defined(__cpp_if_constexpr) || (__cpp_if_constexpr < 201606L)
#  define BOOST_DECIMAL_DETAIL_INT128_NO_CXX17_IF_CONSTEXPR
#endif

#ifndef BOOST_DECIMAL_DETAIL_INT128_NO_CXX17_IF_CONSTEXPR
#  define BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR if constexpr
#else
#  define BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR if
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define BOOST_DECIMAL_DETAIL_INT128_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#  define BOOST_DECIMAL_DETAIL_INT128_UNREACHABLE __assume(0)
#else
#  define BOOST_DECIMAL_DETAIL_INT128_UNREACHABLE std::abort()
#endif

#ifdef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE
#  define BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR inline constexpr
#  define BOOST_DECIMAL_DETAIL_INT128_EXPORT export
#else
#  define BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR static constexpr
#  define BOOST_DECIMAL_DETAIL_INT128_EXPORT
#endif

// Detect if we can throw or not
// First check if the user said no explicitly
// Then check if it's been disabled elsewhere

#ifdef BOOST_DECIMAL_DETAIL_INT128_DISABLE_EXCEPTIONS

#  define BOOST_DECIMAL_DETAIL_INT128_THROW_EXCEPTION(expr)

#else

#  ifdef _MSC_VER
#    ifdef _CPPUNWIND
#      define BOOST_DECIMAL_DETAIL_INT128_THROW_EXCEPTION(expr) throw expr;
#    else
#      define BOOST_DECIMAL_DETAIL_INT128_THROW_EXCEPTION(expr)
#      define BOOST_DECIMAL_DETAIL_INT128_DISABLE_EXCEPTIONS
#    endif
#  else
#    ifdef __EXCEPTIONS
#      define BOOST_DECIMAL_DETAIL_INT128_THROW_EXCEPTION(expr) throw expr;
#    else
#      define BOOST_DECIMAL_DETAIL_INT128_THROW_EXCEPTION(expr)
#      define BOOST_DECIMAL_DETAIL_INT128_DISABLE_EXCEPTIONS
#    endif
#endif

#endif // Exceptions

#if defined(__cpp_impl_three_way_comparison) && __cpp_impl_three_way_comparison >= 201907L && __has_include(<compare>)
#  define BOOST_DECIMAL_DETAIL_INT128_HAS_SPACESHIP_OPERATOR
#  ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE
#    include <compare>
#  endif
#endif

#if defined(__CUDACC__) && defined(BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA)
#  define BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE __host__ __device__
#else
#  define BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE
#endif

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_CONFIG_HPP
