// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_IS_POWER_OF_10_HPP
#define BOOST_DECIMAL_DETAIL_IS_POWER_OF_10_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/remove_trailing_zeros.hpp>
#include <boost/decimal/detail/concepts.hpp>

namespace boost {
namespace decimal {
namespace detail {

template <typename T>
BOOST_DECIMAL_CUDA_CONSTEXPR auto is_power_of_10(const T x) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_unsigned_v, T, bool)
{
    const auto removed_zeros {detail::remove_trailing_zeros(x)};
    return removed_zeros.trimmed_number == 1U;
}

} // namespace detail    
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_IS_POWER_OF_10_HPP
