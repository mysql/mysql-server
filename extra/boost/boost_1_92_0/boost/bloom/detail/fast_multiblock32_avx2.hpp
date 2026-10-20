/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_FAST_MULTIBLOCK32_AVX2_HPP
#define BOOST_BLOOM_DETAIL_FAST_MULTIBLOCK32_AVX2_HPP

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

template<std::size_t K>
struct fast_multiblock32:detail::multiblock_fpr_base<K>
{
  static constexpr std::size_t k=K;
  using value_type=__m256i[(k+7)/8];
  static constexpr std::size_t used_value_size=sizeof(std::uint32_t)*k;

  static BOOST_FORCEINLINE void mark(value_type& x,std::uint64_t hash)
  {
    for(std::size_t i=0;i<k/8;++i){
      mark_m256i(x[i],hash,8);
      hash=detail::mulx64(hash);
    }
    if(k%8){
      mark_m256i(x[k/8],hash,k%8);
    }
  }

  static BOOST_FORCEINLINE bool check(const value_type& x,std::uint64_t hash)
  {
    bool res=true;
    for(std::size_t i=0;i<k/8;++i){
      res&=check_m256i(x[i],hash,8);
      hash=detail::mulx64(hash);
    }
    if(k%8){
      res&=check_m256i(x[k/8],hash,k%8);
    }
    return res;
  }

private:
  static BOOST_FORCEINLINE __m256i make_m256i(
    std::uint64_t hash,std::size_t kp)
  {
    const __m256i ones[8]={
      _mm256_set_epi32(0,0,0,0,0,0,0,1),
      _mm256_set_epi32(0,0,0,0,0,0,1,1),
      _mm256_set_epi32(0,0,0,0,0,1,1,1),
      _mm256_set_epi32(0,0,0,0,1,1,1,1),
      _mm256_set_epi32(0,0,0,1,1,1,1,1),
      _mm256_set_epi32(0,0,1,1,1,1,1,1),
      _mm256_set_epi32(0,1,1,1,1,1,1,1),
      _mm256_set_epi32(1,1,1,1,1,1,1,1),
    };

    __m256i h=_mm256_set1_epi64x(hash);
    h=_mm256_sllv_epi64(h,_mm256_set_epi64x(15,10,5,0));
    h=_mm256_srli_epi32(h,32-5);
    return _mm256_sllv_epi32(ones[kp-1],h);
  }

  static BOOST_FORCEINLINE void mark_m256i(
    __m256i& x,std::uint64_t hash,std::size_t kp)
  {
    __m256i h=make_m256i(hash,kp);
    x=_mm256_or_si256(x,h);
  }

#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
/* 'int': forcing value to bool 'true' or 'false' */
#pragma warning(push)
#pragma warning(disable:4800)
#endif

  static BOOST_FORCEINLINE bool check_m256i(
    const __m256i& x,std::uint64_t hash,std::size_t kp)
  {
    __m256i h=make_m256i(hash,kp);
    return _mm256_testc_si256(x,h);
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
