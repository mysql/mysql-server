// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_PROMOTE_SIGNIFICAND_HPP
#define BOOST_DECIMAL_DETAIL_PROMOTE_SIGNIFICAND_HPP

#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <type_traits>
#include <limits>

namespace boost {
namespace decimal {
namespace detail {

namespace impl {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, BOOST_DECIMAL_INTEGRAL Integer>
struct promote_significand
{
    using type = std::conditional_t<std::numeric_limits<Integer>::digits10 < std::numeric_limits<typename DecimalType::significand_type>::digits10,
                                    typename DecimalType::significand_type, detail::make_unsigned_t<Integer>>;
};

} // namespace impl

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, BOOST_DECIMAL_INTEGRAL Integer>
using promote_significand_t = typename impl::promote_significand<DecimalType, Integer>::type;

} // namespace detail
} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_PROMOTE_SIGNIFICAND_HPP
