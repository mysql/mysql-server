/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_CONSTEXPR_BIT_WIDTH_HPP
#define BOOST_BLOOM_DETAIL_CONSTEXPR_BIT_WIDTH_HPP

#include <cstddef>

namespace boost{
namespace bloom{
namespace detail{

/* boost::core::bit_width is not always C++11 constexpr */

constexpr std::size_t constexpr_bit_width(std::size_t x) 
{
  return x?1+constexpr_bit_width(x>>1):0;
}

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */
#endif
