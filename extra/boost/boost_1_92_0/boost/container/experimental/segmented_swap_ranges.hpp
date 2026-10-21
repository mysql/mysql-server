//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_SWAP_RANGES_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_SWAP_RANGES_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_iterator_traits.hpp>
#include <boost/move/adl_move_swap.hpp>

namespace boost {
namespace container {

template <class FwdIt1, class Sent, class FwdIt2>
FwdIt2 segmented_swap_ranges(FwdIt1 first1, Sent last1, FwdIt2 first2);

namespace detail_algo {


template <class FwdIt1, class Sent, class FwdIt2, class Tag>
typename algo_enable_if_c<
   !Tag::value || is_sentinel<Sent, FwdIt1>::value, FwdIt2>::type
segmented_swap_ranges_dispatch (FwdIt1 first1, Sent last1, FwdIt2 first2, Tag)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; first1 != last1; ++first1, ++first2) {
      boost::adl_move_swap(*first1, *first2);
   }
   return first2;
}

template <class SegIter, class FwdIt2>
FwdIt2 segmented_swap_ranges_dispatch (SegIter first1, SegIter last1, FwdIt2 first2, segmented_iterator_tag)
{
   typedef segmented_iterator_traits<SegIter>   traits;
   typedef typename traits::local_iterator    local_iterator;
   typedef typename traits::segment_iterator  segment_iterator;
   typedef typename segmented_iterator_traits
      <local_iterator>::is_segmented_iterator is_local_seg_t;

   segment_iterator       sfirst = traits::segment(first1);
   segment_iterator const slast  = traits::segment(last1);

   if(sfirst == slast) {
      first2 = (segmented_swap_ranges_dispatch)(traits::local(first1), traits::local(last1), first2, is_local_seg_t());
   }
   else {
      first2 = (segmented_swap_ranges_dispatch)(traits::local(first1), traits::end(sfirst), first2, is_local_seg_t());

      for(++sfirst; sfirst != slast; ++sfirst)
         first2 = (segmented_swap_ranges_dispatch)(traits::begin(sfirst), traits::end(sfirst), first2, is_local_seg_t());

      first2 = (segmented_swap_ranges_dispatch)(traits::begin(sfirst), traits::local(last1), first2, is_local_seg_t());
   }
   return first2;
}

} // namespace detail_algo

//! Swaps elements in [first1, last1) with the range starting at \c first2.
//! Returns an iterator past the last swapped element in the second range.
//! Segmentation is exploited on the first range.
template <class FwdIt1, class Sent, class FwdIt2>
BOOST_CONTAINER_FORCEINLINE
FwdIt2 segmented_swap_ranges(FwdIt1 first1, Sent last1, FwdIt2 first2)
{
   typedef segmented_iterator_traits<FwdIt1> traits;
   return detail_algo::segmented_swap_ranges_dispatch
      (first1, last1, first2, typename traits::is_segmented_iterator());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_SWAP_RANGES_HPP
