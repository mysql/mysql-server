/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_BLOCK_BASE_HPP
#define BOOST_BLOOM_DETAIL_BLOCK_BASE_HPP

#include <boost/config.hpp>
#include <boost/bloom/detail/constexpr_bit_width.hpp>
#include <boost/bloom/detail/mulx64.hpp>
#include <boost/bloom/detail/type_traits.hpp>
#include <cstddef>
#include <cstdint>

namespace boost{
namespace bloom{
namespace detail{

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

/* Validates type Block and provides common looping facilities for block
 * and multiblock.
 */

template<typename Block,std::size_t K>
struct block_base
{
  static_assert(
    is_unsigned_integral_or_extended_unsigned_integral<Block>::value||
    (
      is_array_of<
        Block,is_unsigned_integral_or_extended_unsigned_integral>::value&&
      is_power_of_two<array_size<Block>::value>::value
    ),
    "Block must be an (extended) unsigned integral type or an array T[N] "
    "with T an (extended) unsigned integral type and N a power of two");
  static constexpr std::size_t k=K;
  static constexpr std::size_t hash_width=sizeof(std::uint64_t)*CHAR_BIT;
  static constexpr std::size_t block_width=sizeof(Block)*CHAR_BIT;
  static constexpr std::size_t mask=block_width-1;
  static constexpr std::size_t shift=constexpr_bit_width(mask);
  static constexpr std::size_t rehash_k=(hash_width-shift)/shift;

  template<typename F>
  static BOOST_FORCEINLINE void loop(std::uint64_t hash,F f)
  {
    for(std::size_t i=0;i<k/rehash_k;++i){
      auto h=hash;
      for(std::size_t j=0;j<rehash_k;++j){
        h>>=shift;
        f(h);
      }
      hash=detail::mulx64(hash);
    }
    auto h=hash;
    for(std::size_t i=0;i<k%rehash_k;++i){
      h>>=shift;
      f(h);
    }
  }

  template<typename F>
  static BOOST_FORCEINLINE bool loop_while(std::uint64_t hash,F f)
  {
    for(std::size_t i=0;i<k/rehash_k;++i){
      auto h=hash;
      for(std::size_t j=0;j<rehash_k;++j){
        h>>=shift;
        if(!f(h))return false;
      }
      hash=detail::mulx64(hash);
    }
    auto h=hash;
    for(std::size_t i=0;i<k%rehash_k;++i){
      h>>=shift;
      if(!f(h))return false;
    }
    return true;
  }
};

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */
#endif
