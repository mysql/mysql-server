//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_CONFIG_HPP
#define BOOST_URL_DETAIL_CONFIG_HPP

#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <limits.h>
#include <stdint.h>

#if CHAR_BIT != 8
# error unsupported platform
#endif

// Determine if compiling as a dynamic library
#if (defined(BOOST_URL_DYN_LINK) || defined(BOOST_ALL_DYN_LINK)) && !defined(BOOST_URL_STATIC_LINK)
#    define BOOST_URL_BUILD_DLL
#endif

// Visibility flags
//
// BOOST_URL_DECL — class-level export/import for compiled classes
//
//   Static lib:       (nothing)
//   Shared, source:   BOOST_SYMBOL_EXPORT
//     Win: __declspec(dllexport)  ELF: visibility("default")
//   Shared, consumer: BOOST_SYMBOL_IMPORT
//     Win: __declspec(dllimport)  ELF: (nothing)
//
//   Always applied to the class, never to individual functions.
//
// BOOST_SYMBOL_VISIBLE — typeinfo identity across shared libs
//
//   On ELF with -fvisibility=hidden, each shared library gets
//   its own typeinfo copy at a different address. Operations
//   comparing typeinfo across boundaries silently fail:
//   dynamic_cast returns null, catch clauses don't match.
//   BOOST_SYMBOL_VISIBLE ensures a single typeinfo (and vtable,
//   if any). Use on any type whose identity is compared across
//   boundaries — e.g. error categories, API-facing base classes.
//     Win: (no-op)  ELF: visibility("default") on the class
//
// MSVC limitation with mixed classes
//
//   Unlike GCC and Clang, MSVC exports ALL members of a
//   __declspec(dllexport) class — including inline ones.
//   This causes LNK2005 (duplicate symbol) for any class
//   that mixes compiled and inline members, and there is no
//   portable workaround. Each class must therefore follow
//   exactly one of two policies:
//   (a) Fully compiled with `class BOOST_URL_DECL C`.
//       All members in .cpp files. No inline/constexpr members.
//   (b) Fully header-only with `class BOOST_SYMBOL_VISIBLE C`.
//       All inline/constexpr/template. No .cpp file.
//
#if !defined(BOOST_URL_BUILD_DLL)
#    define BOOST_URL_DECL /* static library */
#elif defined(BOOST_URL_SOURCE)
#    define BOOST_URL_DECL BOOST_SYMBOL_EXPORT /* source: dllexport/visibility */
#else
#    define BOOST_URL_DECL BOOST_SYMBOL_IMPORT /* header: dllimport */
#endif

// Set up auto-linker
# if !defined(BOOST_URL_SOURCE) && !defined(BOOST_ALL_NO_LIB) && !defined(BOOST_URL_NO_LIB)
#  define BOOST_LIB_NAME boost_url
#  if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_URL_DYN_LINK)
#   define BOOST_DYN_LINK
#  endif
#  include <boost/config/auto_link.hpp>
# endif

// Set up SSE2
#if ! defined(BOOST_URL_NO_SSE2) && \
    ! defined(BOOST_URL_USE_SSE2)
# if (defined(_M_IX86) && _M_IX86_FP == 2) || \
      defined(_M_X64) || defined(__SSE2__)
#  define BOOST_URL_USE_SSE2
# endif
#endif

// constexpr
#if BOOST_WORKAROUND( BOOST_GCC_VERSION, <= 72000 ) || \
    BOOST_WORKAROUND( BOOST_CLANG_VERSION, <= 35000 )
# define BOOST_URL_CONSTEXPR
#else
# define BOOST_URL_CONSTEXPR constexpr
#endif

// C++14 constexpr (relaxed constexpr)
#define BOOST_URL_CXX14_CONSTEXPR BOOST_CXX14_CONSTEXPR

#if defined(BOOST_NO_CXX14_CONSTEXPR)
# define BOOST_URL_CXX14_CONSTEXPR_OR_INLINE inline
# define BOOST_URL_CXX14_CONSTEXPR_OR_FORCEINLINE BOOST_FORCEINLINE
#else
# define BOOST_URL_CXX14_CONSTEXPR_OR_INLINE constexpr
# define BOOST_URL_CXX14_CONSTEXPR_OR_FORCEINLINE constexpr
#endif

// C++20 constexpr (constexpr virtual, constexpr destructors)
// Detection aligned with Boost.System's BOOST_SYSTEM_HAS_CXX20_CONSTEXPR:
// uses the feature-test macro for P1064R0 (constexpr virtual functions).
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201907L
# define BOOST_URL_HAS_CXX20_CONSTEXPR
#endif

#if BOOST_WORKAROUND(BOOST_CLANG_VERSION, < 110000)
# undef BOOST_URL_HAS_CXX20_CONSTEXPR
#endif

// MSVC ICEs on constexpr URL parsing (error C1001 in p1_init.c)
#if defined(BOOST_MSVC)
# undef BOOST_URL_HAS_CXX20_CONSTEXPR
#endif

#if defined(BOOST_URL_HAS_CXX20_CONSTEXPR)
# define BOOST_URL_CXX20_CONSTEXPR constexpr
# define BOOST_URL_CXX20_CONSTEXPR_OR_INLINE constexpr
#else
# define BOOST_URL_CXX20_CONSTEXPR
# define BOOST_URL_CXX20_CONSTEXPR_OR_INLINE inline
#endif

// __builtin_is_constant_evaluated detection
// Following the pattern from Boost.Hash2 and Boost.UUID.
#if defined(__has_builtin)
# if __has_builtin(__builtin_is_constant_evaluated)
#  define BOOST_URL_HAS_BUILTIN_IS_CONSTANT_EVALUATED
# endif
#endif

#if !defined(BOOST_URL_HAS_BUILTIN_IS_CONSTANT_EVALUATED) \
    && defined(BOOST_MSVC) && BOOST_MSVC >= 1925
# define BOOST_URL_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#endif

// Add source location to error codes
#define BOOST_URL_ERR(ev) \
     (::boost::system::error_code( (ev), [] { \
         static constexpr auto loc((BOOST_CURRENT_LOCATION)); \
         return &loc; }()))
#define BOOST_URL_RETURN_EC(ev) \
    static constexpr auto loc ## __LINE__((BOOST_CURRENT_LOCATION)); \
    return ::boost::system::error_code((ev), &loc ## __LINE__)
#define BOOST_URL_POS (BOOST_CURRENT_LOCATION)

// Error return for BOOST_URL_CXX20_CONSTEXPR functions.
//
// C++20: the function is constexpr, so we branch on
// is_constant_evaluated(). At compile time: returns the
// error enum directly. At runtime: attaches source location.
//
// Pre-C++20: the function is not constexpr
// (BOOST_URL_CXX20_CONSTEXPR is empty), so we always
// attach source location.
#if defined(BOOST_URL_HAS_CXX20_CONSTEXPR)
# define BOOST_URL_CONSTEXPR_RETURN_EC(ev) \
    do { \
        if (__builtin_is_constant_evaluated()) { \
            return (ev); \
        } \
        return [](auto e) { \
            BOOST_URL_RETURN_EC(e); \
        }(ev); \
    } while(0)
#else
# define BOOST_URL_CONSTEXPR_RETURN_EC(ev) \
    BOOST_URL_RETURN_EC(ev)
#endif

#if !defined(BOOST_NO_CXX20_HDR_CONCEPTS) && defined(__cpp_lib_concepts)
#define BOOST_URL_HAS_CONCEPTS
#endif

#ifdef  BOOST_URL_HAS_CONCEPTS
#define BOOST_URL_CONSTRAINT(C) C
#else
#define BOOST_URL_CONSTRAINT(C) class
#endif

// String token parameters
#ifndef BOOST_URL_STRTOK_TPARAM
#define BOOST_URL_STRTOK_TPARAM BOOST_URL_CONSTRAINT(string_token::StringToken) StringToken = string_token::return_string
#endif
#ifndef BOOST_URL_STRTOK_RETURN
#define BOOST_URL_STRTOK_RETURN typename StringToken::result_type
#endif
#ifndef BOOST_URL_STRTOK_ARG
#define BOOST_URL_STRTOK_ARG(name) StringToken&& token = {}
#endif

// Move
#if BOOST_WORKAROUND( BOOST_GCC_VERSION, < 80000 ) || \
    BOOST_WORKAROUND( BOOST_CLANG_VERSION, < 30900 )
#define BOOST_URL_RETURN(x) return std::move((x))
#else
#define BOOST_URL_RETURN(x) return (x)
#endif

// Limit tests
#ifndef BOOST_URL_MAX_SIZE
// leave room for a null terminator and
// fit within url_impl's 32-bit offsets
#define BOOST_URL_MAX_SIZE ((std::size_t)UINT32_MAX - 1)
#endif

// libstdcxx copy-on-write strings
#ifndef BOOST_URL_COW_STRINGS
#if defined(BOOST_LIBSTDCXX_VERSION) && (BOOST_LIBSTDCXX_VERSION < 60000 || (defined(_GLIBCXX_USE_CXX11_ABI) && _GLIBCXX_USE_CXX11_ABI == 0))
#define BOOST_URL_COW_STRINGS
#endif
#endif

// detect 32/64 bit
#if UINTPTR_MAX == UINT64_MAX
# define BOOST_URL_ARCH 64
#elif UINTPTR_MAX == UINT32_MAX
# define BOOST_URL_ARCH 32
#else
# error Unknown or unsupported architecture, please open an issue
#endif

// deprecated attribute
#if defined(BOOST_MSVC)
#define BOOST_URL_DEPRECATED(msg)
#else
#define BOOST_URL_DEPRECATED(msg) BOOST_DEPRECATED(msg)
#endif

// Implementation defined type for Doxygen while Clang
// can still parse the comments into the AST
#ifndef BOOST_URL_DOCS
#define BOOST_URL_IMPLEMENTATION_DEFINED(Type) Type
#else
#define BOOST_URL_IMPLEMENTATION_DEFINED(Type) __implementation_defined__
#endif

#ifndef BOOST_URL_DOCS
#define BOOST_URL_SEE_BELOW(Type) Type
#else
#define BOOST_URL_SEE_BELOW(Type) __see_below__
#endif

#ifdef __cpp_lib_array_constexpr
#define BOOST_URL_LIB_ARRAY_CONSTEXPR constexpr
#else
#define BOOST_URL_LIB_ARRAY_CONSTEXPR
#endif

// avoid Boost.TypeTraits for these traits
namespace boost {
namespace urls {

template<class...> struct make_void { typedef void type; };
template<class... Ts> using void_t = typename make_void<Ts...>::type;

} // urls
} // boost

#endif
