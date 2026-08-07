/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_BLOCK_HPP
#define BOOST_BLOOM_BLOCK_HPP

#include <boost/bloom/detail/block_base.hpp>
#include <boost/bloom/detail/block_ops.hpp>
#include <boost/bloom/detail/block_fpr_base.hpp>
#include <cstddef>
#include <cstdint>

namespace boost{
namespace bloom{

template<typename Block,std::size_t K>
struct block:
  public detail::block_fpr_base<K>,
  private detail::block_base<Block,K>
{
  static constexpr std::size_t k=K;
  using value_type=Block;

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline void mark(value_type& x,std::uint64_t hash)
  {
    loop(hash,[&](std::uint64_t h){block_ops::set(x,h&mask);});
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline bool check(const value_type& x,std::uint64_t hash)
  {
    return check(x,hash,typename block_ops::is_extended_block{});
  }

private:
  using super=detail::block_base<Block,K>;
  using super::mask;
  using super::loop;
  using super::loop_while;
  using block_ops=detail::block_ops<Block>;

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline bool check(
    const value_type& x,std::uint64_t hash,
    std::false_type /* non-extended block */)
  {
    Block fp;
    block_ops::zero(fp);
    mark(fp,hash);
    return block_ops::testc(x,fp);
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline bool check(
    const value_type& x,std::uint64_t hash,
    std::true_type /* extended block */)
  {
    int res=1;
    loop(hash,[&](std::uint64_t h){
      res&=block_ops::get_at_lsb(x,h&mask);
    });
    return res;
  }
};

} /* namespace bloom */
} /* namespace boost */
#endif
