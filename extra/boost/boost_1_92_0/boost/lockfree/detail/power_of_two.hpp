//  boost lockfree: small power-of-two helper
//
//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_LOCKFREE_DETAIL_POWER_OF_TWO_HPP_INCLUDED
#define BOOST_LOCKFREE_DETAIL_POWER_OF_TWO_HPP_INCLUDED

#include <cstddef>

namespace boost { namespace lockfree { namespace detail {

inline constexpr bool is_power_of_two( size_t n ) noexcept
{
    return n && ( n & ( n - 1 ) ) == 0;
}

}}} // namespace boost::lockfree::detail

#endif /* BOOST_LOCKFREE_DETAIL_POWER_OF_TWO_HPP_INCLUDED */
