// Copyright Kevlin Henney, 2000-2005.
// Copyright Alexander Nasonov, 2006-2010.
// Copyright Antony Polukhin, 2011-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// what:  lexical_cast custom keyword cast
// who:   contributed by Kevlin Henney,
//        enhanced with contributions from Terje Slettebo,
//        with additional fixes and suggestions from Gennaro Prota,
//        Beman Dawes, Dave Abrahams, Daryle Walker, Peter Dimov,
//        Alexander Nasonov, Antony Polukhin, Justin Viiret, Michael Hofmann,
//        Cheng Yang, Matthew Bradbury, David W. Birdsall, Pavel Korzh and other Boosters
// when:  November 2000, March 2003, June 2005, June 2006, March 2011 - 2016

#ifndef BOOST_LEXICAL_CAST_DETAIL_CONVERTER_NUMERIC_HPP
#define BOOST_LEXICAL_CAST_DETAIL_CONVERTER_NUMERIC_HPP

#include <boost/lexical_cast/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_LEXICAL_CAST_INTERFACE_UNIT)

#ifndef BOOST_LEXICAL_CAST_INTERFACE_UNIT
#include <boost/config.hpp>
#ifdef BOOST_HAS_PRAGMA_ONCE
#   pragma once
#endif

#include <type_traits>
#include <boost/core/cmath.hpp>
#include <boost/limits.hpp>

#endif  // #ifndef BOOST_LEXICAL_CAST_INTERFACE_UNIT

#include <boost/lexical_cast/detail/type_traits.hpp>

namespace boost { namespace detail {

template <class Source, class Target>
bool ios_numeric_comparer_float(Source x, Source y) noexcept {
    return x == y
        || (boost::core::isnan(x) && boost::core::isnan(y))
        || (x < (std::numeric_limits<Target>::min)())
    ;
}

template <class RangeType, class T>
constexpr bool is_out_of_range_for(T value) noexcept {
    return value > static_cast<T>((std::numeric_limits<RangeType>::max)())
        || value < static_cast<T>((std::numeric_limits<RangeType>::min)())
        || boost::core::isnan(value);
}


// integral -> integral
template <typename Target, typename Source>
typename std::enable_if<
    !std::is_floating_point<Source>::value && !std::is_floating_point<Target>::value, bool
>::type noexcept_numeric_convert(Source arg, Target& result) noexcept {
    const Target target_tmp = static_cast<Target>(arg);
    const Source arg_restored = static_cast<Source>(target_tmp);
    if (arg == arg_restored) {
        result = target_tmp;
        return true;
    }
    return false;
}

// integral -> floating point
template <typename Target, typename Source>
typename std::enable_if<
    !std::is_floating_point<Source>::value && std::is_floating_point<Target>::value, bool
>::type noexcept_numeric_convert(Source arg, Target& result) noexcept {
    const Target target_tmp = static_cast<Target>(arg);
    result = target_tmp;
    return true;
}


// floating point -> floating point
template <typename Target, typename Source>
typename std::enable_if<
    std::is_floating_point<Source>::value && std::is_floating_point<Target>::value, bool
>::type noexcept_numeric_convert(Source arg, Target& result) noexcept {
    const Target target_tmp = static_cast<Target>(arg);
    const Source arg_restored = static_cast<Source>(target_tmp);
    if (detail::ios_numeric_comparer_float<Source, Target>(arg, arg_restored)) {
        result = target_tmp;
        return true;
    }

    return false;
}

// floating point -> integral
template <typename Target, typename Source>
typename std::enable_if<
    std::is_floating_point<Source>::value && !std::is_floating_point<Target>::value, bool
>::type noexcept_numeric_convert(Source arg, Target& result) noexcept {
    if (detail::is_out_of_range_for<Target>(arg)) {
        return false;
    }

    const Target target_tmp = static_cast<Target>(arg);
    const Source arg_restored = static_cast<Source>(target_tmp);
    if (arg == arg_restored /* special values are handled in detail::is_out_of_range_for */) {
        result = target_tmp;
        return true;
    }

    return false;
}

struct lexical_cast_dynamic_num_not_ignoring_minus
{
    template <typename Target, typename Source>
    static inline bool try_convert(Source arg, Target& result) noexcept {
        return boost::detail::noexcept_numeric_convert<Target, Source >(arg, result);
    }
};

struct lexical_cast_dynamic_num_ignoring_minus
{
    template <typename Target, typename Source>
#if defined(__clang__) && (__clang_major__ > 3 || __clang_minor__ > 6)
    __attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
    static inline bool try_convert(Source arg, Target& result) noexcept {
        typedef typename std::conditional<
                std::is_floating_point<Source>::value,
                std::conditional<true, Source, Source>,  // std::type_identity emulation
                boost::detail::lcast::make_unsigned<Source>
        >::type usource_lazy_t;
        typedef typename usource_lazy_t::type usource_t;

        if (arg < 0) {
            const bool res = boost::detail::noexcept_numeric_convert<Target, usource_t>(
                static_cast<usource_t>(0u - static_cast<usource_t>(arg)), result
            );
            result = static_cast<Target>(0u - result);
            return res;
        } else {
            return boost::detail::noexcept_numeric_convert<Target, usource_t>(arg, result);
        }
    }
};

/*
 * dynamic_num_converter_impl follows the rules:
 * 1) If Source can be converted to Target without precision loss and
 * without overflows, then assign Source to Target and return
 *
 * 2) If Source is less than 0 and Target is an unsigned integer,
 * then negate Source, check the requirements of rule 1) and if
 * successful, assign static_casted Source to Target and return
 *
 * 3) Otherwise throw a bad_lexical_cast exception
 *
 *
 * Rule 2) required because boost::lexical_cast has the behavior of
 * stringstream, which uses the rules of scanf for conversions. And
 * in the C99 standard for unsigned input value minus sign is
 * optional, so if a negative number is read, no errors will arise
 * and the result will be the two's complement.
 */
template <typename Target, typename Source>
struct dynamic_num_converter_impl
{
    static inline bool try_convert(Source arg, Target& result) noexcept {
        typedef typename std::conditional<
            boost::detail::lcast::is_unsigned<Target>::value &&
            (boost::detail::lcast::is_signed<Source>::value || std::is_floating_point<Source>::value) &&
            !(std::is_same<Source, bool>::value) &&
            !(std::is_same<Target, bool>::value),
            lexical_cast_dynamic_num_ignoring_minus,
            lexical_cast_dynamic_num_not_ignoring_minus
        >::type caster_type;

        return caster_type::try_convert(arg, result);
    }
};

}} // namespace boost::detail

#endif  // #if !defined(BOOST_USE_MODULES) || defined(BOOST_LEXICAL_CAST_INTERFACE_UNIT)

#endif // BOOST_LEXICAL_CAST_DETAIL_CONVERTER_NUMERIC_HPP

