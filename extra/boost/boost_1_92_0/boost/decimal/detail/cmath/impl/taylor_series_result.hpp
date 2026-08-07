// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_TAYLOR_SERIES_RESULT_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_TAYLOR_SERIES_RESULT_HPP

#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/cmath/fma.hpp>

namespace boost {
namespace decimal {
namespace detail {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T, typename Array>
constexpr auto taylor_series_result(T x, const Array &coeffs) noexcept
{
    const std::size_t N = coeffs.size();

    auto result = coeffs[N - 1];

    for (std::size_t i = N - 1; i-- > 0;)
    {
        result = unchecked_fma(result, x, coeffs[i]);
    }

    return result;
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_IMPL_TAYLOR_SERIES_RESULT_HPP
