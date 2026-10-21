/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_FAST_MULTIBLOCK64_AVX2_HPP
#define BOOST_BLOOM_DETAIL_FAST_MULTIBLOCK64_AVX2_HPP

#include <boost/bloom/detail/avx2.hpp>
#include <boost/bloom/detail/multiblock_fpr_base.hpp>
#include <boost/bloom/detail/mulx64.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <cstddef>
#include <cstdint>

namespace boost{
namespace bloom{

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

namespace detail{

struct m256ix2
{
  __m256i lo,hi;
};

} /* namespace detail */

template<std::size_t K>
struct fast_multiblock64:detail::multiblock_fpr_base<K>
{
  static constexpr std::size_t k=K;
  using value_type=detail::m256ix2[(k+7)/8];
  static constexpr std::size_t used_value_size=sizeof(std::uint64_t)*k;

  static BOOST_FORCEINLINE void mark(value_type& x,std::uint64_t hash)
  {
    for(int i=0;i<k/8;++i){
      mark_m256ix2(x[i],hash,8);
      hash=detail::mulx64(hash);
    }
    if(k%8){
      mark_m256ix2(x[k/8],hash,k%8);
    }
  }

  static BOOST_FORCEINLINE bool check(const value_type& x,std::uint64_t hash)
  {
    bool res=true;
    for(int i=0;i<k/8;++i){
      res&=check_m256ix2(x[i],hash,8);
      hash=detail::mulx64(hash);
    }
    if(k%8){
      res&=check_m256ix2(x[k/8],hash,k%8);
    }
    return res;
  }

private:
  static BOOST_FORCEINLINE detail::m256ix2 make_m256ix2(
    std::uint64_t hash,std::size_t kp)
  {
    const detail::m256ix2 ones[8]={
      {_mm256_set_epi64x(0,0,0,1),_mm256_set_epi64x(0,0,0,0)},
      {_mm256_set_epi64x(0,0,1,1),_mm256_set_epi64x(0,0,0,0)},
      {_mm256_set_epi64x(0,1,1,1),_mm256_set_epi64x(0,0,0,0)},
      {_mm256_set_epi64x(1,1,1,1),_mm256_set_epi64x(0,0,0,0)},
      {_mm256_set_epi64x(1,1,1,1),_mm256_set_epi64x(0,0,0,1)},
      {_mm256_set_epi64x(1,1,1,1),_mm256_set_epi64x(0,0,1,1)},
      {_mm256_set_epi64x(1,1,1,1),_mm256_set_epi64x(0,1,1,1)},
      {_mm256_set_epi64x(1,1,1,1),_mm256_set_epi64x(1,1,1,1)},
    };

    __m256i h=_mm256_set1_epi64x(hash);
    h=_mm256_sllv_epi64(h,_mm256_set_epi64x(18,12,6,0));
    h=_mm256_srli_epi32(h,32-6);
    return {
      _mm256_sllv_epi64(
        ones[kp-1].lo,_mm256_cvtepu32_epi64(_mm256_extracti128_si256(h,0))),
      kp<=4?
      _mm256_set1_epi64x(0):
      _mm256_sllv_epi64(
        ones[kp-1].hi,_mm256_cvtepu32_epi64(_mm256_extracti128_si256(h,1)))
    };
  }

  static BOOST_FORCEINLINE void mark_m256ix2(
    detail::m256ix2& x,std::uint64_t hash,std::size_t kp)
  {
    detail::m256ix2 h=make_m256ix2(hash,kp);
    x.lo=_mm256_or_si256(x.lo,h.lo);
    if(kp>4)x.hi=_mm256_or_si256(x.hi,h.hi);
  }

#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
/* 'int': forcing value to bool 'true' or 'false' */
#pragma warning(push)
#pragma warning(disable:4800)
#endif

  static BOOST_FORCEINLINE bool check_m256ix2(
    const detail::m256ix2& x,std::uint64_t hash,std::size_t kp)
  {
    detail::m256ix2 h=make_m256ix2(hash,kp);
    auto res=_mm256_testc_si256(x.lo,h.lo);
    if(kp>4)res&=_mm256_testc_si256(x.hi,h.hi);
    return res;
  }

#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
#pragma warning(pop) /* C4800 */
#endif
};

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

} /* namespace bloom */
} /* namespace boost */

#endif
