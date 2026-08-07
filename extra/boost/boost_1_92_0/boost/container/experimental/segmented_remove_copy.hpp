//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_REMOVE_COPY_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_REMOVE_COPY_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_iterator_traits.hpp>
#include <boost/container/detail/iterator.hpp>
#include <boost/move/utility_core.hpp>

namespace boost {
namespace container {

template <class InIter, class Sent, class OutIter, class T>
OutIter segmented_remove_copy(InIter first, Sent last, OutIter result, const T& value);

namespace detail_algo {

template <bool Move, class SrcIter, class Sent, class DstIter, class DstSent, class T, class DstTag, class SrcCat>
BOOST_CONTAINER_FORCEINLINE typename algo_enable_if_c<!DstTag::value, segduo<SrcIter, DstIter> >::type
segmented_remove_copy_dst_bounded
   (SrcIter first, Sent last, DstIter dst_first, DstSent dst_last, const T& value, DstTag, SrcCat)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; first != last; ++first) {
      if(!(*first == value)) {
         if(dst_first == dst_last)
            goto out_path;
         transfer_op<Move>::apply(*dst_first, *first);
         ++dst_first;
      }
   }
   out_path:
   return segduo<SrcIter, DstIter>(first, dst_first);
}

template <bool Move, class SrcIter, class Sent, class SegDstIter, class T, class SrcCat>
segduo<SrcIter, SegDstIter> segmented_remove_copy_dst_bounded
   (SrcIter first, Sent last, SegDstIter dst_first, SegDstIter dst_last, const T& value,
    segmented_iterator_tag, SrcCat)
{
   typedef segmented_iterator_traits<SegDstIter>  dst_traits;
   typedef typename dst_traits::local_iterator    dst_local_iterator;
   typedef typename dst_traits::segment_iterator  dst_segment_iterator;
   typedef typename segmented_iterator_traits<dst_local_iterator>::is_segmented_iterator dst_is_local_seg_t;

   dst_segment_iterator       sfirst = dst_traits::segment(dst_first);
   const dst_segment_iterator slast  = dst_traits::segment(dst_last);

   if(sfirst == slast) {
      segduo<SrcIter, dst_local_iterator> r = (segmented_remove_copy_dst_bounded<Move>)
         (first, last, dst_traits::local(dst_first), dst_traits::local(dst_last), value, dst_is_local_seg_t(), SrcCat());
      return segduo<SrcIter, SegDstIter>(r.first, dst_traits::compose(sfirst, r.second));
   }
   else {
      segduo<SrcIter, dst_local_iterator> r = (segmented_remove_copy_dst_bounded<Move>)
         (first, last, dst_traits::local(dst_first), dst_traits::end(sfirst), value, dst_is_local_seg_t(), SrcCat());
      first = r.first;
      if(first == last)
         return segduo<SrcIter, SegDstIter>(first, dst_traits::compose(sfirst, r.second));

      for(++sfirst; sfirst != slast; ++sfirst) {
         r = (segmented_remove_copy_dst_bounded<Move>)
            (first, last, dst_traits::begin(sfirst), dst_traits::end(sfirst), value, dst_is_local_seg_t(), SrcCat());
         first = r.first;
         if(first == last)
            return segduo<SrcIter, SegDstIter>(first, dst_traits::compose(sfirst, r.second));
      }

      r = (segmented_remove_copy_dst_bounded<Move>)
         (first, last, dst_traits::begin(slast), dst_traits::local(dst_last), value, dst_is_local_seg_t(), SrcCat());
      return segduo<SrcIter, SegDstIter>(r.first, dst_traits::compose(sfirst, r.second));
   }
}

//////////////////////////////////////////////////////////////////////////////
// Destination dispatch
//////////////////////////////////////////////////////////////////////////////

template <bool Move, class SrcIter, class Sent, class DstIter, class T, class Cat>
BOOST_CONTAINER_FORCEINLINE DstIter segmented_remove_copy_dst_dispatch
   (SrcIter first, Sent last, DstIter result, const T& value,
    const non_segmented_iterator_tag &, Cat)
{
   return (segmented_remove_copy_dst_bounded<Move>)
      (first, last, result, unreachable_sentinel_t(), value, non_segmented_iterator_tag(), Cat()).second;
}

template <bool Move, class SrcIter, class Sent, class SegDstIter, class T, class Cat>
SegDstIter segmented_remove_copy_dst_dispatch
   (SrcIter first, const Sent last, SegDstIter result, const T& value,
    const segmented_iterator_tag &, Cat)
{
   typedef segmented_iterator_traits<SegDstIter>  dst_traits;
   typedef typename dst_traits::local_iterator    dst_local_iterator;
   typedef typename dst_traits::segment_iterator  dst_segment_iterator;
   typedef typename segmented_iterator_traits<dst_local_iterator>::is_segmented_iterator dst_is_local_seg_t;

   if(first == last)
      return result;

   dst_segment_iterator dst_seg   = dst_traits::segment(result);
   dst_local_iterator   dst_local = dst_traits::local(result);

   while(1) {
      const segduo<SrcIter, dst_local_iterator> r = (segmented_remove_copy_dst_bounded<Move>)
         (first, last, dst_local, dst_traits::end(dst_seg), value, dst_is_local_seg_t(), Cat());
      if(r.first != last) {
         first = r.first;
         ++dst_seg;
         dst_local = dst_traits::begin(dst_seg);
      }
      else {
         dst_local = r.second;
         break;
      }
   }
   return dst_traits::compose(dst_seg, dst_local);
}

//////////////////////////////////////////////////////////////////////////////
// Source dispatch
//////////////////////////////////////////////////////////////////////////////

template <bool Move, class SrcIter, class Sent, class OutIter, class T, class Tag, class Cat>
BOOST_CONTAINER_FORCEINLINE 
typename algo_enable_if_c<
   !Tag::value || is_sentinel<Sent, SrcIter>::value, OutIter>::type
segmented_remove_copy_dispatch
   (SrcIter first, Sent last, OutIter result, const T& value, Tag, Cat)
{
#if !defined(BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO)
   typedef segmented_iterator_traits<OutIter> dst_traits;
   return (segmented_remove_copy_dst_dispatch<Move>)
      (first, last, result, value, typename dst_traits::is_segmented_iterator(), Cat());
#else
   return (segmented_remove_copy_dst_dispatch<Move>)
      (first, last, result, value, non_segmented_iterator_tag(), Cat());
#endif
}

template <bool Move, class SegIter, class OutIter, class T, class Cat>
OutIter segmented_remove_copy_dispatch
   (SegIter first, SegIter last, OutIter result, const T& value, segmented_iterator_tag, Cat)
{
   typedef segmented_iterator_traits<SegIter>  src_traits;
   typedef typename src_traits::local_iterator   src_local_iterator;
   typedef typename src_traits::segment_iterator src_segment_iterator;
   typedef typename segmented_iterator_traits<src_local_iterator>::is_segmented_iterator src_is_local_seg_t;
   typedef typename iterator_traits<src_local_iterator>::iterator_category src_local_cat_t;

   src_segment_iterator sfirst = src_traits::segment(first);
   const src_segment_iterator slast  = src_traits::segment(last);

   if(sfirst == slast) {
      return (segmented_remove_copy_dispatch<Move>)
         (src_traits::local(first), src_traits::local(last), result, value, src_is_local_seg_t(), src_local_cat_t());
   }
   else {
      result = (segmented_remove_copy_dispatch<Move>)
         (src_traits::local(first), src_traits::end(sfirst), result, value, src_is_local_seg_t(), src_local_cat_t());

      for(++sfirst; sfirst != slast; ++sfirst)
         result = (segmented_remove_copy_dispatch<Move>)
            (src_traits::begin(sfirst), src_traits::end(sfirst), result, value, src_is_local_seg_t(), src_local_cat_t());

      return (segmented_remove_copy_dispatch<Move>)
         (src_traits::begin(slast), src_traits::local(last), result, value, src_is_local_seg_t(), src_local_cat_t());
   }
}

} // namespace detail_algo

//! Copies elements from [first, last) to the range beginning at \c result,
//! skipping elements equal to \c value. Returns the output iterator past
//! the last element written.
template <class InIter, class Sent, class OutIter, class T>
BOOST_CONTAINER_FORCEINLINE
OutIter segmented_remove_copy(InIter first, Sent last, OutIter result, const T& value)
{
   typedef segmented_iterator_traits<InIter> traits;
   return detail_algo::segmented_remove_copy_dispatch<false>
      (first, last, result, value, typename traits::is_segmented_iterator(), typename iterator_traits<InIter>::iterator_category());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_REMOVE_COPY_HPP
