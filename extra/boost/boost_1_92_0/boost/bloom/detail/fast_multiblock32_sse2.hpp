/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_FAST_MULTIBLOCK32_SSE2_HPP
#define BOOST_BLOOM_DETAIL_FAST_MULTIBLOCK32_SSE2_HPP

#include <boost/bloom/detail/multiblock_fpr_base.hpp>
#include <boost/bloom/detail/mulx64.hpp>
#include <boost/bloom/detail/sse2.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <cstddef>
#include <cstdint>

#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

namespace boost{
namespace bloom{

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

namespace detail{

struct m128ix2
{
  __m128i lo,hi;
};

/* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
static inline int mm_testc_si128(__m128i x,__m128i y)
{
#ifdef __SSE4_1__
  return _mm_testc_si128(x,y);
#else
  return _mm_movemask_epi8(_mm_cmpeq_epi32(_mm_and_si128(x,y),y))==0xFFFF;
#endif
}

} /* namespace detail */

template<std::size_t K>
struct fast_multiblock32:detail::multiblock_fpr_base<K>
{
  static constexpr std::size_t k=K;
  using value_type=detail::m128ix2[(k+7)/8];
  static constexpr std::size_t used_value_size=sizeof(std::uint32_t)*k;

  static BOOST_FORCEINLINE void mark(value_type& x,std::uint64_t hash)
  {
    for(std::size_t i=0;i<k/8;++i){
      mark_m128ix2(x[i],hash,8);
      hash=detail::mulx64(hash);
    }
    if(k%8){
      mark_m128ix2(x[k/8],hash,k%8);
    }
  }

  static BOOST_FORCEINLINE bool check(const value_type& x,std::uint64_t hash)
  {
    bool res=true;
    for(std::size_t i=0;i<k/8;++i){
      res&=check_m128ix2(x[i],hash,8);
      hash=detail::mulx64(hash);
    }
    if(k%8){
      res&=check_m128ix2(x[k/8],hash,k%8);
    }
    return res;
  }

private:
  static BOOST_FORCEINLINE detail::m128ix2 make_m128ix2(
    std::uint64_t hash,std::size_t kp)
  {
    const std::uint32_t mask=std::uint32_t(31)<<23,
                          exp=std::uint32_t(127)<<23;
    const __m128i exps[4]={
      _mm_set_epi32( 0 , 0 , 0 ,exp),
      _mm_set_epi32( 0 , 0 ,exp,exp),
      _mm_set_epi32( 0 ,exp,exp,exp),
      _mm_set_epi32(exp,exp,exp,exp),
    };

    if(kp<=4){
      __m128i h_lo=_mm_set_epi64x(hash<<5,hash);
      h_lo=_mm_and_si128(h_lo,_mm_set1_epi32(mask));
      h_lo=_mm_add_epi32(h_lo,exps[kp-1]);
      return {
        _mm_cvttps_epi32(*(__m128*)&h_lo),
        _mm_set1_epi32(0)
      };
    }
    else{
      __m128i h_lo=_mm_set_epi64x(hash<<5,hash),
              h_hi=_mm_slli_si128(h_lo,2);
      h_lo=_mm_and_si128(h_lo,_mm_set1_epi32(mask));
      h_hi=_mm_and_si128(h_hi,_mm_set1_epi32(mask));
      h_lo=_mm_add_epi32(h_lo,_mm_set1_epi32(exp));
      h_hi=_mm_add_epi32(h_hi,exps[kp-5]);
      return {
        _mm_cvttps_epi32(*(__m128*)&h_lo),
        _mm_cvttps_epi32(*(__m128*)&h_hi)
      };
    }
  }

  static BOOST_FORCEINLINE void mark_m128ix2(
    detail::m128ix2& x,std::uint64_t hash,std::size_t kp)
  {
    detail::m128ix2 h=make_m128ix2(hash,kp);
    x.lo=_mm_or_si128(x.lo,h.lo);
    if(kp>4)x.hi=_mm_or_si128(x.hi,h.hi);
  }

#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
/* 'int': forcing value to bool 'true' or 'false' */
#pragma warning(push)
#pragma warning(disable:4800)
#endif

  static BOOST_FORCEINLINE bool check_m128ix2(
    const detail::m128ix2& x,std::uint64_t hash,std::size_t kp)
  {
    detail::m128ix2 h=make_m128ix2(hash,kp);
    auto res=detail::mm_testc_si128(x.lo,h.lo);
    if(kp>4)res&=detail::mm_testc_si128(x.hi,h.hi);
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
