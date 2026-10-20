// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CONFIG_HPP
#define BOOST_DECIMAL_DETAIL_CONFIG_HPP

// 3.4.7 evaluation format:
// This is defined at top level because it has ramifications for every successive header
//
// We add an extra definition here in case users want to pick and choose headers
#ifndef BOOST_DECIMAL_DEC_EVAL_METHOD
#  define BOOST_DECIMAL_DEC_EVAL_METHOD 0
#endif

// Fundamental CUDACC options
#if defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA)
#  define BOOST_DECIMAL_HOST_DEVICE __host__ __device__
#  define BOOST_DECIMAL_CUDA_CONSTEXPR __host__ __device__ constexpr
#  define BOOST_DECIMAL_DETAIL_INT128_ENABLE_CUDA
#  define BOOST_DECIMAL_DISABLE_EXCEPTIONS
#  define BOOST_DECIMAL_DISABLE_CASSERT
#else
#  define BOOST_DECIMAL_HOST_DEVICE
#  define BOOST_DECIMAL_CUDA_CONSTEXPR constexpr
#endif

// Determine endianness
#if defined(_WIN32)

#define BOOST_DECIMAL_ENDIAN_BIG_BYTE 0
#define BOOST_DECIMAL_ENDIAN_LITTLE_BYTE 1

#elif defined(__BYTE_ORDER__)

#define BOOST_DECIMAL_ENDIAN_BIG_BYTE (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BOOST_DECIMAL_ENDIAN_LITTLE_BYTE (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#else

#error Could not determine endian type. Please file an issue at https://github.com/boostorg/decimal with your architecture

#endif // Determine endianness

#if __has_include(<bit>)
#  if __cplusplus >= 201806L || (defined(_MSVC_LANG) && (_MSVC_LANG >= 201806L))
#    ifndef BOOST_DECIMAL_BUILD_MODULE
#      include <bit>
#    endif
#    define BOOST_DECIMAL_HAS_STDBIT
#    if defined(__cpp_lib_bit_cast) && (__cpp_lib_bit_cast >= 201806L)
#      define BOOST_DECIMAL_HAS_CONSTEXPR_BITCAST
#    endif
#  endif
#endif

// Constexpr bit cast is broken on clang-10 and 32-bit platforms
#if defined(BOOST_DECIMAL_HAS_CONSTEXPR_BITCAST) && ((defined(__clang__) && __clang_major__ == 10) || defined(__i386__))
#  undef BOOST_DECIMAL_HAS_CONSTEXPR_BITCAST
#endif

#ifdef BOOST_DECIMAL_HAS_CONSTEXPR_BITCAST
#  define BOOST_DECIMAL_CXX20_CONSTEXPR constexpr
#endif

#ifndef BOOST_DECIMAL_CXX20_CONSTEXPR
#  define BOOST_DECIMAL_CXX20_CONSTEXPR inline
#endif

// Include intrinsics if available
// This section allows us to disable any of the following independently.
//   Use #define BOOST_DECIMAL_DISABLE_CASSERT to disable uses of assert.
//   Use #define BOOST_DECIMAL_DISABLE_IOSTREAM to disable uses of I/O streaming.
//   Use #define BOOST_DECIMAL_DISABLE_CLIB to disable uses of both assert as well as I/O streaming (and all oother heavyweight C-LIB artifacts).

#if (!defined(BOOST_DECIMAL_DISABLE_CASSERT) && !defined(BOOST_DECIMAL_DISABLE_CLIB))
#  ifndef BOOST_DECIMAL_BUILD_MODULE
#    include <cassert>
#  endif
#endif

#ifndef BOOST_DECIMAL_DISABLE_CASSERT
#  define BOOST_DECIMAL_ASSERT(x) assert(x)
#  define BOOST_DECIMAL_ASSERT_MSG(expr, msg) assert((expr)&&(msg))
#else
#  define BOOST_DECIMAL_ASSERT(x)
#  define BOOST_DECIMAL_ASSERT_MSG(expr, msg)
#endif

#ifdef BOOST_DECIMAL_DISABLE_CLIB
#  ifndef BOOST_DECIMAL_DISABLE_IOSTREAM
#    define BOOST_DECIMAL_DISABLE_IOSTREAM
#  endif
#  ifndef BOOST_DECIMAL_DISABLE_CASSERT
#    undef BOOST_DECIMAL_ASSERT
#    define BOOST_DECIMAL_ASSERT(x)
#  endif
#endif

#if defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA)

// Include intrinsics if available
#if defined(_MSC_VER)
#  ifndef BOOST_DECIMAL_BUILD_MODULE
#    include <intrin.h>
#  endif
#  if defined(_M_AMD64)
#    define BOOST_DECIMAL_HAS_MSVC_64BIT_INTRINSICS
#  else
#    define BOOST_DECIMAL_HAS_MSVC_32BIT_INTRINSICS
#  endif
#  if defined(__ADX__) && defined(BOOST_DECIMAL_HAS_MSVC_64BIT_INTRINSICS)
#    define BOOST_DECIMAL_ADD_CARRY _addcarryx_u64
#    define BOOST_DECIMAL_SUB_BORROW _subborrow_u64
#  elif defined(BOOST_DECIMAL_HAS_MSVC_64BIT_INTRINSICS)
#    define BOOST_DECIMAL_ADD_CARRY _addcarry_u64
#    define BOOST_DECIMAL_SUB_BORROW _subborrow_u64
#  endif
#elif defined(__x86_64__)
#  ifndef BOOST_DECIMAL_BUILD_MODULE
#    include <x86intrin.h>
#  endif
#  define BOOST_DECIMAL_HAS_X64_INTRINSICS
#  ifdef __ADX__
#    define BOOST_DECIMAL_ADD_CARRY _addcarryx_u64
#    define BOOST_DECIMAL_SUB_BORROW _subborrow_u64
#  else
#    define BOOST_DECIMAL_ADD_CARRY _addcarry_u64
#    define BOOST_DECIMAL_SUB_BORROW _subborrow_u64
#  endif
#elif defined(__ARM_NEON__)
#  ifndef BOOST_DECIMAL_BUILD_MODULE
#    include <arm_neon.h>
#  endif
#  define BOOST_DECIMAL_HAS_ARM_INTRINSICS
#elif defined(__i386__)
#  ifndef BOOST_DECIMAL_BUILD_MODULE
#    include <immintrin.h>
#  endif
#  define BOOST_DECIMAL_HAS_x86_INTRINSICS
#else
#  define BOOST_DECIMAL_HAS_NO_INTRINSICS
#endif

#else
#  define BOOST_DECIMAL_HAS_NO_INTRINSICS
#endif

// Use 128-bit integers and suppress warnings for using extensions
#if (defined(BOOST_HAS_INT128) || (defined(__SIZEOF_INT128__) && !defined(_MSC_VER))) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA))

namespace boost { namespace decimal { namespace detail {

#  ifdef __GNUC__
__extension__ typedef __int128 builtin_int128_t;
__extension__ typedef unsigned __int128 builtin_uint128_t;
#  else
typedef __int128 builtin_int128_t;
typedef unsigned __int128 builtin_uint128_t;
#  endif

} // namespace detail
} // namespace decimal
} // namespace boost

#  define BOOST_DECIMAL_HAS_INT128
#  define BOOST_DECIMAL_INT128_MAX  static_cast<boost::decimal::detail::builtin_int128_t>((static_cast<boost::decimal::detail::builtin_uint128_t>(1) << 127) - 1)
#  define BOOST_DECIMAL_INT128_MIN  (-BOOST_DECIMAL_INT128_MAX - 1)
#  define BOOST_DECIMAL_UINT128_MAX ((2 * static_cast<boost::decimal::detail::builtin_uint128_t>(BOOST_DECIMAL_INT128_MAX)) + 1)
#endif

// 128-bit floats
#if (defined(BOOST_HAS_FLOAT128) || defined(__SIZEOF_FLOAT128__)) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA))
#  define BOOST_DECIMAL_HAS_FLOAT128
#endif

#if defined(__has_builtin)
#define BOOST_DECIMAL_HAS_BUILTIN(x) __has_builtin(x)
#else
#define BOOST_DECIMAL_HAS_BUILTIN(x) false
#endif

// Detection for C++23 fixed width floating point types
// All of these types are optional so check for each of them individually
#ifdef __has_include
#  if __has_include(<stdfloat>)
#    if __cplusplus > 202002L || (defined(_MSVC_LANG) && _MSVC_LANG > 202002L)
#      ifndef BOOST_DECIMAL_BUILD_MODULE
#        include <stdfloat>
#      endif
#    endif
#  endif
#endif
#ifdef __STDCPP_FLOAT16_T__
#  define BOOST_DECIMAL_HAS_FLOAT16
#endif
#ifdef __STDCPP_FLOAT32_T__
#  define BOOST_DECIMAL_HAS_FLOAT32
#endif
#ifdef __STDCPP_FLOAT64_T__
#  define BOOST_DECIMAL_HAS_FLOAT64
#endif
#ifdef __STDCPP_FLOAT128_T__
#  define BOOST_DECIMAL_HAS_STDFLOAT128
#endif
#ifdef __STDCPP_BFLOAT16_T__
#  define BOOST_DECIMAL_HAS_BRAINFLOAT16
#endif

#define BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION

#if __has_cpp_attribute(maybe_unused)
#  define BOOST_DECIMAL_ATTRIBUTE_UNUSED [[maybe_unused]]
#else
#   if defined(___GNUC__) || defined(__clang__)
#       define BOOST_DECIMAL_ATTRIBUTE_UNUSED __attribute__((__unused__))
#   elif defined(_MSC_VER)
#       define BOOST_DECIMAL_ATTRIBUTE_UNUSED __pragma(warning(suppress: 4189))
#   else
#       define BOOST_DECIMAL_ATTRIBUTE_UNUSED
#   endif
#endif

#if !defined(__cpp_if_constexpr) || (__cpp_if_constexpr < 201606L)
#  define BOOST_DECIMAL_NO_CXX17_IF_CONSTEXPR
#endif

#ifndef BOOST_DECIMAL_NO_CXX17_IF_CONSTEXPR
#  define BOOST_DECIMAL_IF_CONSTEXPR if constexpr
#else
#  define BOOST_DECIMAL_IF_CONSTEXPR if
#endif

#if BOOST_DECIMAL_HAS_BUILTIN(__builtin_expect)
#  define BOOST_DECIMAL_LIKELY(x) __builtin_expect(x, 1)
#  define BOOST_DECIMAL_UNLIKELY(x) __builtin_expect(x, 0)
#else
#  define BOOST_DECIMAL_LIKELY(x) x
#  define BOOST_DECIMAL_UNLIKELY(x) x
#endif

#if defined(__cpp_lib_three_way_comparison) && __cpp_lib_three_way_comparison >= 201907L
#  ifndef BOOST_DECIMAL_BUILD_MODULE
#    include <compare>
#  endif
#  define BOOST_DECIMAL_HAS_SPACESHIP_OPERATOR
#endif

// Is constant evaluated detection
#ifdef __cpp_lib_is_constant_evaluated
#  define BOOST_DECIMAL_HAS_IS_CONSTANT_EVALUATED
#endif

#ifdef __has_builtin
#  if __has_builtin(__builtin_is_constant_evaluated)
#    define BOOST_DECIMAL_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#  endif
#endif

// Clang < 12 will detect availability, but has issues
#ifdef BOOST_DECIMAL_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#  if defined(__clang__) && __clang_major__ < 12
#    ifdef BOOST_DECIMAL_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#      undef BOOST_DECIMAL_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#    endif
#    ifdef BOOST_DECIMAL_HAS_IS_CONSTANT_EVALUATED
#      undef BOOST_DECIMAL_HAS_IS_CONSTANT_EVALUATED
#    endif
#  endif
#endif

//
// MSVC also supports __builtin_is_constant_evaluated if it's recent enough:
//
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 192528326)
#  define BOOST_DECIMAL_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#endif

//
// As does GCC-9:
//
#if defined(__GNUC__) && (__GNUC__ >= 9) && !defined(BOOST_DECIMAL_HAS_BUILTIN_IS_CONSTANT_EVALUATED)
#  define BOOST_DECIMAL_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#endif

#if defined(BOOST_DECIMAL_HAS_IS_CONSTANT_EVALUATED) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA))
#  define BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x) std::is_constant_evaluated()
#elif defined(BOOST_DECIMAL_HAS_BUILTIN_IS_CONSTANT_EVALUATED) && !(defined(__CUDACC__) && defined(BOOST_DECIMAL_ENABLE_CUDA))
#  define BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x) __builtin_is_constant_evaluated()
#else
#  define BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x) false
#  define BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
#endif

// https://github.com/llvm/llvm-project/issues/55638
#if defined(__clang__) && __cplusplus > 202002L && __clang_major__ < 17
#  undef BOOST_DECIMAL_IS_CONSTANT_EVALUATED
#  define BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x) false
#  define BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
#endif

#if defined(__clang__) && __clang_major__ < 19
#  define BOOST_DECIMAL_CLANG_STATIC static
#else
#  define BOOST_DECIMAL_CLANG_STATIC
#endif

#ifdef BOOST_DECIMAL_BUILD_MODULE
#  define BOOST_DECIMAL_EXPORT export
#else
#  define BOOST_DECIMAL_EXPORT
#endif

#if defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L
#  define BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE inline constexpr
#  define BOOST_DECIMAL_CONSTEXPR_VARIABLE_SPECIALIZATION BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE
#  define BOOST_DECIMAL_INLINE_VARIABLE inline
#else
#  define BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE static constexpr
#  define BOOST_DECIMAL_CONSTEXPR_VARIABLE_SPECIALIZATION BOOST_DECIMAL_CLANG_STATIC constexpr
#  define BOOST_DECIMAL_INLINE_VARIABLE static
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define BOOST_DECIMAL_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#  define BOOST_DECIMAL_UNREACHABLE __assume(0)
#else
#  define BOOST_DECIMAL_UNREACHABLE std::abort()
#endif

#if defined(_MSC_VER)
#  define BOOST_DECIMAL_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#  define BOOST_DECIMAL_FORCE_INLINE __attribute__((always_inline)) inline
#else
#  define BOOST_DECIMAL_FORCE_INLINE inline
#endif

#ifdef __FAST_MATH__
#  define BOOST_DECIMAL_FAST_MATH
#endif

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#  if __has_include(<charconv>)
     // We don't need all of charconv, just: std::to_chars_result, std::from_chars_result, and std::chars_format
     // These compilers and versions give us what we need
#    if (defined(__clang_major__) && __clang_major__ >= 13) || (defined(__GNUC__) && __GNUC__ >= 10) || defined(_MSC_VER)
#      ifndef BOOST_DECIMAL_BUILD_MODULE
#        include <charconv>
#      endif
#      define BOOST_DECIMAL_HAS_STD_CHARCONV
#    endif
#  endif

#  if __has_include(<string_view>)
#    ifndef BOOST_DECIMAL_BUILD_MODULE
#      include <string_view>
#    endif
#    if __cpp_lib_string_view >= 201606L
#      define BOOST_DECIMAL_HAS_STD_STRING_VIEW
#    endif
#  endif

#endif

// Since we should not be able to pull these in from the STL in module mode define them ourselves
// This is also low risk since they are not supposed to be exported
#ifdef BOOST_DECIMAL_BUILD_MODULE
#  ifndef UINT64_C
#    define UINT64_C(x) (x ## ULL)
#  endif
#  ifndef UINT32_C
#    define UINT32_C(x) (x ## UL)
#  endif
#endif

// Detect if we can throw or not
// First check if the user said no explicitly
// Then check if it's been disabled elsewhere

#ifdef BOOST_DECIMAL_DISABLE_EXCEPTIONS

#  define BOOST_DECIMAL_THROW_EXCEPTION(expr)

#else

#  ifdef _MSC_VER
#    ifdef _CPPUNWIND
#      define BOOST_DECIMAL_THROW_EXCEPTION(expr) throw expr;
#    else
#      define BOOST_DECIMAL_THROW_EXCEPTION(expr)
#      define BOOST_DECIMAL_DISABLE_EXCEPTIONS
#    endif
#  else
#    ifdef __EXCEPTIONS
#      define BOOST_DECIMAL_THROW_EXCEPTION(expr) throw expr;
#    else
#      define BOOST_DECIMAL_THROW_EXCEPTION(expr)
#      define BOOST_DECIMAL_DISABLE_EXCEPTIONS
#    endif
#endif

#endif // Exceptions

#ifndef BOOST_DECIMAL_BULID_MODULE
#  include <cfloat>
#  include <type_traits>
#endif

// Check for PPC64LE with IEEE long double (which is an alias to __float128) or x64 with "-mlong-double-128"
//
// IBM128 has 106 Mantissa Digits whereas IEEE128 has 113
// https://developers.redhat.com/articles/2023/05/16/benefits-fedora-38-long-double-transition-ppc64le#
#if ((defined(__ppc64__) || defined(__PPC64__) || defined(__ppc64le__) || defined(__PPC64LE__)) && (defined(__LONG_DOUBLE_IEEE128__) || LDBL_MANT_DIG == 113))

#define BOOST_DECIMAL_LDBL_IS_FLOAT128
static_assert(std::is_same<long double, __float128>::value, "__float128 should be an alias to long double. Please open an issue at: https://github.com/boostorg/decimal");

#endif

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
#  define BOOST_DECIMAL_HAS_CHAR8_T
#endif

#if defined(__has_cpp_attribute) && __has_cpp_attribute(fallthrough) >= 201603L
#  define BOOST_DECIMAL_FALLTHROUGH [[fallthrough]];
#elif defined(__clang__)
#  define BOOST_DECIMAL_FALLTHROUGH [[clang::fallthrough]];
#elif defined(__GNUC__)
#  define BOOST_DECIMAL_FALLTHROUGH __attribute__ ((fallthrough));
#else
#  define BOOST_DECIMAL_FALLTHROUGH
#endif

#endif // BOOST_DECIMAL_DETAIL_CONFIG_HPP
