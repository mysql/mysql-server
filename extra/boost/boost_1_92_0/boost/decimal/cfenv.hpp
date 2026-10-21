// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_CFENV_HPP
#define BOOST_DECIMAL_CFENV_HPP

#include <boost/decimal/detail/config.hpp>

#ifdef BOOST_DECIMAL_FE_DEC_DOWNWARD
#  define BOOST_DECIMAL_MODE_1 1
#else
#  define BOOST_DECIMAL_MODE_1 0
#endif

#ifdef BOOST_DECIMAL_FE_DEC_TO_NEAREST
#  define BOOST_DECIMAL_MODE_2 1
#else
#  define BOOST_DECIMAL_MODE_2 0
#endif

#ifdef BOOST_DECIMAL_FE_DEC_TO_NEAREST_FROM_ZERO
#  define BOOST_DECIMAL_MODE_3 1
#else
#  define BOOST_DECIMAL_MODE_3 0
#endif

#ifdef BOOST_DECIMAL_FE_DEC_TOWARD_ZERO
#  define BOOST_DECIMAL_MODE_4 1
#else
#  define BOOST_DECIMAL_MODE_4 0
#endif

#ifdef BOOST_DECIMAL_FE_DEC_UPWARD
#  define BOOST_DECIMAL_MODE_5 1
#else
#  define BOOST_DECIMAL_MODE_5 0
#endif

// Now we can safely use arithmetic in preprocessor
#define BOOST_DECIMAL_MODE_COUNT \
(BOOST_DECIMAL_MODE_1 + BOOST_DECIMAL_MODE_2 + \
BOOST_DECIMAL_MODE_3 + BOOST_DECIMAL_MODE_4 + \
BOOST_DECIMAL_MODE_5)

#if BOOST_DECIMAL_MODE_COUNT > 1
#  error "Only one rounding mode macro can be defined"
#endif

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cfenv>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT enum class rounding_mode : unsigned
{
    fe_dec_downward = 1 << 0,
    fe_dec_to_nearest = 1 << 1,
    fe_dec_to_nearest_from_zero = 1 << 2,
    fe_dec_toward_zero = 1 << 3,
    fe_dec_upward = 1 << 4,
    fe_dec_default = fe_dec_to_nearest
};

BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE auto _boost_decimal_global_rounding_mode {
    #ifdef BOOST_DECIMAL_FE_DEC_DOWNWARD
    rounding_mode::fe_dec_downward
    #elif defined(BOOST_DECIMAL_FE_DEC_TO_NEAREST)
    rounding_mode::fe_dec_to_nearest
    #elif defined(BOOST_DECIMAL_FE_DEC_TO_NEAREST_FROM_ZERO)
    rounding_mode::fe_dec_to_nearest_from_zero
    #elif defined (BOOST_DECIMAL_FE_DEC_TOWARD_ZERO)
    rounding_mde::fe_dec_toward_zero
    #elif defined(BOOST_DECIMAL_FE_DEC_UPWARD)
    rounding_mode::fe_dec_upward
    #else
    rounding_mode::fe_dec_default
    #endif
};

BOOST_DECIMAL_INLINE_VARIABLE auto _boost_decimal_global_runtime_rounding_mode {_boost_decimal_global_rounding_mode};

BOOST_DECIMAL_EXPORT inline auto fegetround() noexcept -> rounding_mode
{
    return _boost_decimal_global_runtime_rounding_mode;
}

// We can only apply updates to the runtime rounding mode
// Return the current rounding mode to the user
// NOTE: This is only updated when we have the ability to make consteval branches,
// otherwise we don't update and still let the user know what the currently applied rounding mode is
BOOST_DECIMAL_EXPORT inline auto fesetround(BOOST_DECIMAL_ATTRIBUTE_UNUSED const rounding_mode round) noexcept -> rounding_mode
{
    #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
    _boost_decimal_global_runtime_rounding_mode = round;
    #endif

    return _boost_decimal_global_runtime_rounding_mode;
}

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_CFENV_HPP
