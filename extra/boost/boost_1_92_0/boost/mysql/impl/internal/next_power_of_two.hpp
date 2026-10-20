//
// Copyright (c) 2026 Vladislav Soulgard (vsoulgard at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_NEXT_POWER_OF_TWO_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_NEXT_POWER_OF_TWO_HPP

#include <boost/core/bit.hpp>

#include <limits>

namespace boost {
namespace mysql {
namespace detail {

// Returns the smallest power of two greater than or equal to n.
// If n exceeds the largest power of two that fits in std::size_t, then
// std::numeric_limits<std::size_t>::max() is returned.
inline std::size_t next_power_of_two(std::size_t n) noexcept
{
    if (n < 2) return 1;

    if (n > ((std::numeric_limits<std::size_t>::max)() >> 1) + 1)
        return (std::numeric_limits<std::size_t>::max)();

    return static_cast<std::size_t>(1u) << boost::core::bit_width(n - 1);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
