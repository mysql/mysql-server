// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_BUFFER_SIZING_HPP
#define BOOST_DECIMAL_DETAIL_BUFFER_SIZING_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/apply_sign.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#endif

namespace boost {
namespace decimal {
namespace detail {

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4127) // Conditional expression for BOOST_IF_CONSTEXPR will be constant in not C++17
#endif

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Dec>
BOOST_DECIMAL_CUDA_CONSTEXPR int get_real_precision(const int my_precision = -1) noexcept
{
    // If the user did not specify precision than we use the maximum representable amount
    // and remove trailing zeros at the end
    return my_precision == -1 ? std::numeric_limits<Dec>::max_digits10 : my_precision;
}

// We don't need to use the full digit counting since the range of the exponents is well-defined
template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, BOOST_DECIMAL_INTEGRAL Int>
BOOST_DECIMAL_CUDA_CONSTEXPR auto buffer_length_exp(Int x) noexcept -> std::enable_if_t<std::numeric_limits<DecimalType>::max_exponent >= 1000, int>
{
    if (x < 10) return 1;
    if (x < 100) return 2;
    if (x < 1000) return 3;
    return 4;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, BOOST_DECIMAL_INTEGRAL Int>
BOOST_DECIMAL_CUDA_CONSTEXPR auto buffer_length_exp(Int x) noexcept -> std::enable_if_t<std::numeric_limits<DecimalType>::max_exponent >= 100 && std::numeric_limits<DecimalType>::max_exponent < 1000, int>
{
    if (x < 10) return 1;
    if (x < 100) return 2;
    return 3;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, BOOST_DECIMAL_INTEGRAL Int>
BOOST_DECIMAL_CUDA_CONSTEXPR auto buffer_length_exp(Int x) noexcept -> std::enable_if_t<std::numeric_limits<DecimalType>::max_exponent >= 10 && std::numeric_limits<DecimalType>::max_exponent < 100, int>
{
    if (x < 10) return 1;
    return 2;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, BOOST_DECIMAL_INTEGRAL Int>
BOOST_DECIMAL_CUDA_CONSTEXPR int total_buffer_length(int real_precision, Int exp, bool signed_value) noexcept
{
    // Sign + integer part + '.' + precision of fraction part + e+/e- or p+/p- + exponent digits
    return static_cast<int>(signed_value) + 1 + real_precision + 2 + buffer_length_exp<DecimalType>(make_positive_unsigned(exp));
}

#ifdef _MSC_VER
# pragma warning(pop)
#endif

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_BUFFER_SIZING_HPP
