#ifndef BOOST_LEAF_DETAIL_DEMANGLE_HPP_INCLUDED
#define BOOST_LEAF_DETAIL_DEMANGLE_HPP_INCLUDED

// Copyright 2018-2026 Emil Dotchevski and Reverge Studios, Inc.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This file is based on boost::core::demangle
//
// Copyright 2014 Peter Dimov
// Copyright 2014 Andrey Semashev
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/leaf/config.hpp>
#include <iosfwd>
#include <cstdlib>
#include <cstring>

// __has_include is currently supported by GCC and Clang. However GCC 4.9 may have issues and
// returns 1 for 'defined( __has_include )', while '__has_include' is actually not supported:
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63662
#if defined(__has_include) && (!defined(__GNUC__) || defined(__clang__) || (__GNUC__ + 0) >= 5)
#   if __has_include(<cxxabi.h>)
#       define BOOST_LEAF_HAS_CXXABI_H
#   endif
#elif defined(__GLIBCXX__) || defined(__GLIBCPP__)
#   define BOOST_LEAF_HAS_CXXABI_H
#endif

#if defined(BOOST_LEAF_HAS_CXXABI_H)
#   include <cxxabi.h>
//  For some architectures (mips, mips64, x86, x86_64) cxxabi.h in Android NDK is implemented by gabi++ library
//  (https://android.googlesource.com/platform/ndk/+/master/sources/cxx-stl/gabi++/), which does not implement
//  abi::__cxa_demangle(). We detect this implementation by checking the include guard here.
#   if defined(__GABIXX_CXXABI_H__)
#       undef BOOST_LEAF_HAS_CXXABI_H
#   endif
#endif

namespace boost { namespace leaf {

namespace detail
{
    // The functions below are C++11 constexpr, but we use BOOST_LEAF_ALWAYS_INLINE to control object file
    // section count / template bloat.

    template <int S1, int S2, int I, bool = S1 >= S2>
    struct cpp11_prefix
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&)[S1], char const (&)[S2]) noexcept
        {
            return false;
        }
    };
    template <int S1, int S2, int I>
    struct cpp11_prefix<S1, S2, I, true>
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&str)[S1], char const (&prefix)[S2]) noexcept
        {
            return str[I] == prefix[I] && cpp11_prefix<S1, S2, I - 1>::check(str, prefix);
        }
    };
    template <int S1, int S2>
    struct cpp11_prefix<S1, S2, 0, true>
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&str)[S1], char const (&prefix)[S2]) noexcept
        {
            return str[0] == prefix[0];
        }
    };
    template <int S1, int S2>
    BOOST_LEAF_ALWAYS_INLINE constexpr int check_prefix(char const (&str)[S1], char const (&prefix)[S2]) noexcept
    {
        return cpp11_prefix<S1, S2, S2 - 2>::check(str, prefix) ? S2 - 1 : 0;
    }

    ////////////////////////////////////////

    template <int S1, int S2, int I1, int I2, bool = S1 >= S2>
    struct cpp11_suffix
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&)[S1], char const (&)[S2]) noexcept
        {
            return false;
        }
    };
    template <int S1, int S2, int I1, int I2>
    struct cpp11_suffix<S1, S2, I1, I2, true>
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&str)[S1], char const (&suffix)[S2]) noexcept
        {
            return str[I1] == suffix[I2] && cpp11_suffix<S1, S2, I1 - 1, I2 - 1>::check(str, suffix);
        }
    };
    template <int S1, int S2, int I1>
    struct cpp11_suffix<S1, S2, I1, 0, true>
    {
        BOOST_LEAF_ALWAYS_INLINE constexpr static bool check(char const (&str)[S1], char const (&suffix)[S2]) noexcept
        {
            return str[I1] == suffix[0];
        }
    };
    template <int S1, int S2>
    BOOST_LEAF_ALWAYS_INLINE constexpr int check_suffix(char const (&str)[S1], char const (&suffix)[S2]) noexcept
    {
        return cpp11_suffix<S1, S2, S1 - 2, S2 - 2>::check(str, suffix) ? S1 - S2 : 0;
    }

    ////////////////////////////////////////

    template <std::size_t S>
    BOOST_LEAF_ALWAYS_INLINE std::size_t compute_hash(char const (&str)[S], std::size_t begin, std::size_t end) noexcept
    {
        std::size_t h = 2166136261u;
        for( std::size_t i = begin; i != end; ++i )
            h = (h ^ static_cast<std::size_t>(str[i])) * 16777619u;
        return h;
    }
} // namespace detail

namespace n
{
    struct r
    {
        char const * name_not_zero_terminated_at_length;
        std::size_t length;
        std::size_t hash;
    };

#ifdef _MSC_VER
#   define BOOST_LEAF_CDECL __cdecl
#else
#   define BOOST_LEAF_CDECL
#endif

    template <class T>
    BOOST_LEAF_ALWAYS_INLINE r BOOST_LEAF_CDECL p()
    {
        // C++11 compile-time parsing of __PRETTY_FUNCTION__/__FUNCSIG__. The sizeof hacks are a
        // workaround for older GCC versions, where __PRETTY_FUNCTION__ is not constexpr, which triggers
        // compile errors when used in constexpr expressinos, yet evaluating a sizeof exrpession works.

        // We don't try to recognize the compiler based on compiler-specific macros. Any compiler/version
        // is supported as long as it uses one of the formats we recognize.

        // Unrecognized __PRETTY_FUNCTION__/__FUNCSIG__ formats will result in compiler diagnostics.
        // In that case, please file an issue on https://github.com/boostorg/leaf.

#define BOOST_LEAF_P(P) (sizeof(char[1 + detail::check_prefix(BOOST_LEAF_PRETTY_FUNCTION, P)]) - 1)
        // clang style:
        std::size_t const p01 = BOOST_LEAF_P("r boost::leaf::n::p() [T = ");
        std::size_t const p02 = BOOST_LEAF_P("r __cdecl boost::leaf::n::p(void) [T = ");
        // old clang style:
        std::size_t const p03 = BOOST_LEAF_P("boost::leaf::n::r boost::leaf::n::p() [T = ");
        std::size_t const p04 = BOOST_LEAF_P("boost::leaf::n::r __cdecl boost::leaf::n::p(void) [T = ");
        // gcc style:
        std::size_t const p05 = BOOST_LEAF_P("boost::leaf::n::r boost::leaf::n::p() [with T = ");
        std::size_t const p06 = BOOST_LEAF_P("boost::leaf::n::r __cdecl boost::leaf::n::p() [with T = ");
        // msvc style, struct:
        std::size_t const p07 = BOOST_LEAF_P("struct boost::leaf::n::r __cdecl boost::leaf::n::p<struct ");
        // msvc style, class:
        std::size_t const p08 = BOOST_LEAF_P("struct boost::leaf::n::r __cdecl boost::leaf::n::p<class ");
        // msvc style, enum:
        std::size_t const p09 = BOOST_LEAF_P("struct boost::leaf::n::r __cdecl boost::leaf::n::p<enum ");
        // msvc style, built-in type:
        std::size_t const p10 = BOOST_LEAF_P("struct boost::leaf::n::r __cdecl boost::leaf::n::p<");
#undef BOOST_LEAF_P

#define BOOST_LEAF_S(S) (sizeof(char[1 + detail::check_suffix(BOOST_LEAF_PRETTY_FUNCTION, S)]) - 1)
        // clang/gcc style:
        std::size_t const s01 = BOOST_LEAF_S("]");
        // msvc style:
        std::size_t const s02 = BOOST_LEAF_S(">(void)");
#undef BOOST_LEAF_S

        char static_assert_unrecognized_pretty_function_format_please_file_github_issue[sizeof(
            char[
                (s01 && (1 == (!!p01 + !!p02 + !!p03 + !!p04 + !!p05 + !!p06)))
                ||
                (s02 && (1 == (!!p07 + !!p08 + !!p09)))
                ||
                (s02 && !!p10)
            ]
        ) * 2 - 1];
        (void) static_assert_unrecognized_pretty_function_format_please_file_github_issue;

        if( std::size_t const p = sizeof(char[1 + !!s01 * (p01 + p02 + p03 + p04 + p05 + p06)]) - 1 )
            return { BOOST_LEAF_PRETTY_FUNCTION + p, s01 - p, detail::compute_hash(BOOST_LEAF_PRETTY_FUNCTION, p, s01) };

        if( std::size_t const p = sizeof(char[1 + !!s02 * (p07 + p08 + p09)]) - 1 )
            return { BOOST_LEAF_PRETTY_FUNCTION + p, s02 - p, detail::compute_hash(BOOST_LEAF_PRETTY_FUNCTION, p, s02) };

        std::size_t const p = sizeof(char[1 + !!s02 * p10]) - 1;
        return { BOOST_LEAF_PRETTY_FUNCTION + p, s02 - p, detail::compute_hash(BOOST_LEAF_PRETTY_FUNCTION, p, s02) };
    }

#undef BOOST_LEAF_CDECL

} // namespace n

} } // namespace boost::leaf

////////////////////////////////////////

namespace boost { namespace leaf {

namespace detail
{
    class demangler
    {
        char const * mangled_name_;
#ifdef BOOST_LEAF_HAS_CXXABI_H
        char * demangled_name_ = nullptr;
#endif

    public:

        explicit demangler(char const * mangled_name) noexcept:
            mangled_name_(mangled_name)
        {
            BOOST_LEAF_ASSERT(mangled_name_);
#ifdef BOOST_LEAF_HAS_CXXABI_H
            int status = 0;
            demangled_name_ = abi::__cxa_demangle(mangled_name_, nullptr, nullptr, &status);
#endif
        }

        ~demangler() noexcept
        {
#ifdef BOOST_LEAF_HAS_CXXABI_H
            std::free(demangled_name_);
#endif
        }

        char const * get() const noexcept
        {
#ifdef BOOST_LEAF_HAS_CXXABI_H
            if( demangled_name_ )
                return demangled_name_;
#endif
            return mangled_name_;
        }
    };
} // namespace detail

} } // namespace boost::leaf

#endif // #ifndef BOOST_LEAF_DETAIL_DEMANGLE_HPP_INCLUDED
