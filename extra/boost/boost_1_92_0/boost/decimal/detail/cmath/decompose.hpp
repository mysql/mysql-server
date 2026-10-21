// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_DECOMPOSE_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_DECOMPOSE_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/components.hpp>

namespace boost { namespace decimal {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
BOOST_DECIMAL_CUDA_CONSTEXPR auto decompose(const T value) noexcept
{
    return value.to_components();
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_DECOMPOSE_HPP
