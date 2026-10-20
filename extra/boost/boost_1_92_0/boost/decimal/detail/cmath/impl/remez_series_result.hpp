// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_REMEZ_SERIES_RESULT_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_REMEZ_SERIES_RESULT_HPP

#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/cmath/fma.hpp>

namespace boost {
namespace decimal {
namespace detail {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T, typename Array>
constexpr auto remez_series_result(T x, const Array &coeffs) noexcept
{
    T result {};

    result = coeffs[0];
    for (std::size_t i {1}; i < coeffs.size(); ++i)
    {
        result = unchecked_fma(result, x, coeffs[i]);
    }

    return result;
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_REMEZ_SERIES_RESULT_HPP
