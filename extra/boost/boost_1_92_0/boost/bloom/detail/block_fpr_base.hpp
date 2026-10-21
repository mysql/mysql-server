/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_BLOCK_FPR_BASE_HPP
#define BOOST_BLOOM_DETAIL_BLOCK_FPR_BASE_HPP

#include <cmath>
#include <cstddef>

namespace boost{
namespace bloom{
namespace detail{

template<std::size_t K>
struct block_fpr_base
{
  static double fpr(std::size_t i,std::size_t w)
  {
    return std::pow(1.0-std::pow(1.0-1.0/w,(double)K*i),(double)K);
  }
};

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */
#endif
