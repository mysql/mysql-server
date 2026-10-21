// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_CEIL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_CEIL_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/fpclassify.hpp>
#include <boost/decimal/detail/cmath/frexp10.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cmath>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto ceil BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (const T val) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using DivType = typename T::significand_type;
    constexpr int inverse_eps {std::is_same<DivType, std::uint32_t>::value ? 7 :
                                   std::is_same<DivType, std::uint64_t>::value ? 16 : 34};

    constexpr T zero {0, 0};
    constexpr T one {1, 0};
    constexpr T max_comp_value {1u, inverse_eps};
    const auto fp {fpclassify(val)};

    switch (fp)
    {
        case FP_ZERO:
        case FP_NAN:
        case FP_INFINITE:
            return val;
        default:
            static_cast<void>(val);
    }

    if (val > zero && val >= max_comp_value)
    {
        return val;
    }

    int exp_ptr {};
    auto new_sig {frexp10(val, &exp_ptr)};
    const auto abs_exp {detail::make_positive_unsigned(exp_ptr)};
    const bool is_neg {val < zero};

    const auto sig_dig {detail::precision_v<T>};
    auto decimal_digits {static_cast<unsigned>(sig_dig)};
    const auto zero_digits {detail::remove_trailing_zeros(new_sig).number_of_removed_zeros};
    const auto non_zero_exp {exp_ptr + static_cast<int>(zero_digits)};

    if (non_zero_exp >= 0)
    {
        // If the value is an integer, nothing should occur
        return val;
    }

    if (sig_dig > abs_exp)
    {
        decimal_digits = abs_exp;
    }
    else if (exp_ptr < 1 && abs_exp >= sig_dig)
    {
        return is_neg ? zero : one;
    }

    new_sig /= detail::pow10<DivType>(decimal_digits);
    if (!is_neg)
    {
        ++new_sig;
    }
    new_sig *= 10U;

    return T{new_sig, exp_ptr + static_cast<int>(decimal_digits) - 1, is_neg};
}

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_CEIL_HPP
