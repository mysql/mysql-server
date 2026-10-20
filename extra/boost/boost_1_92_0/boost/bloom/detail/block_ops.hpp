/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_BLOCK_OPS_HPP
#define BOOST_BLOOM_DETAIL_BLOCK_OPS_HPP

#include <boost/config.hpp>
#include <cstdint>
#include <type_traits>

namespace boost{
namespace bloom{
namespace detail{

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

template<typename Block>
struct block_ops
{
  using is_extended_block=std::false_type;
  using value_type=Block;

  static BOOST_FORCEINLINE void zero(Block& x)
  {
    x=0;
  }

  static BOOST_FORCEINLINE void set(value_type& x,std::uint64_t n)
  {
    x|=Block(1)<<n;
  }

  static BOOST_FORCEINLINE int get_at_lsb(const value_type& x,std::uint64_t n)
  {
    return static_cast<int>(x>>n);
  }

  static BOOST_FORCEINLINE void reduce(
    int& res,const value_type& x,std::uint64_t n)
  {
    res&=get_at_lsb(x,n);
  }

  static BOOST_FORCEINLINE bool testc(const value_type& x,const value_type& y)
  {
    return (x&y)==y;
  }
};

template<typename Block,std::size_t N>
struct block_ops<Block[N]>
{
  using is_extended_block=std::true_type;
  using value_type=Block[N];

  static BOOST_FORCEINLINE void zero(value_type& x)
  {
    for(std::size_t i=0;i<N;++i)x[i]=0;
  }

  static BOOST_FORCEINLINE void set(value_type& x,std::uint64_t n)
  {
    x[n%N]|=Block(1)<<(n/N);
  }

  static BOOST_FORCEINLINE int get_at_lsb(const value_type& x,std::uint64_t n)
  {
    return static_cast<int>(x[n%N]>>(n/N));
  }

  static BOOST_FORCEINLINE void reduce(
    int& res,const value_type& x,std::uint64_t n)
  {
    res&=get_at_lsb(x,n);
  }
};

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif


} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */

#endif
