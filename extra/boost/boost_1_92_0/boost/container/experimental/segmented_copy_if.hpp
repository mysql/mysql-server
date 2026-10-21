//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_COPY_IF_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_COPY_IF_HPP

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

namespace boost {
namespace container {

template <class InIter, class Sent, class OutIter, class Pred>
OutIter segmented_copy_if(InIter first, Sent last, OutIter result, Pred pred);

namespace detail_algo {

//////////////////////////////////////////////////////////////////////////////
// Bounded destination helper: copies matching elements from source into
// [dst_first, dst_last), stopping when source is exhausted or destination
// is full.  Returns segduo<SrcIter, DstIter> with the final positions of
// both iterators.  Recursively walks destination segments when dst is
// segmented.
//
// When dst_last is unreachable_sentinel_t the destination-full check
// is optimised away, giving the same code as an unbounded loop.
//////////////////////////////////////////////////////////////////////////////

template <class SrcIter, class Sent, class DstIter, class DstSent, class Pred, class DstTag, class SrcCat>
BOOST_CONTAINER_FORCEINLINE
typename algo_enable_if_c<!DstTag::value, segduo<SrcIter, DstIter> >::type
segmented_copy_if_dst_bounded
   (SrcIter first, Sent last, DstIter dst_first, DstSent dst_last, Pred pred, DstTag, SrcCat)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; first != last; ++first) {
      if(pred(*first)) {
         if(dst_first == dst_last)
            goto out_path;
         *dst_first = *first;
         ++dst_first;
      }
   }
   out_path:
   return segduo<SrcIter, DstIter>(first, dst_first);
}

template <class RASrcIter, class RADstIter, class Pred>
typename iterator_enable_if_tag
   <RADstIter, std::random_access_iterator_tag, segduo<RASrcIter, RADstIter> >::type
segmented_copy_if_dst_bounded
   (RASrcIter first, RASrcIter last, RADstIter dst_first, RADstIter dst_last, Pred pred,
    const non_segmented_iterator_tag &, const std::random_access_iterator_tag &src_tag)
{
   typedef typename iterator_traits<RASrcIter>::difference_type difference_type;

   (void)src_tag;
   RASrcIter cur = first;
   RADstIter dst_cur = dst_first;

   const difference_type B = 32;
   difference_type avail = last - cur;
   for(;;) {
      difference_type room  = dst_last - dst_cur;
      difference_type chunk = room < avail ? room : avail;
      if(chunk == 0)
         goto end;
      if(chunk >= B) {
         avail -= B;
         chunk = B;
         BOOST_CONTAINER_SEGMENTED_AUTO_UNROLL
         while(chunk) {
            --chunk;
            if(pred(*cur)) {
               *dst_cur = *cur;
               ++dst_cur;
            }
            ++cur;
         }
      }
      else{
         break;
      }
   }

   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   while(cur != last) {
      if(pred(*cur)) {
         if (dst_cur == dst_last)
            break;
         *dst_cur = *cur;
         ++dst_cur;
      }
      ++cur;
   }
   end:

   return segduo<RASrcIter, RADstIter>(cur, dst_cur);
}

template <class SrcIter, class Sent, class SegDstIter, class Pred, class SrcCat>
segduo<SrcIter, SegDstIter> segmented_copy_if_dst_bounded
   (SrcIter first, Sent last, SegDstIter dst_first, SegDstIter dst_last, Pred pred,
    segmented_iterator_tag, SrcCat)
{
   typedef segmented_iterator_traits<SegDstIter>  dst_traits;
   typedef typename dst_traits::local_iterator    dst_local_iterator;
   typedef typename dst_traits::segment_iterator  dst_segment_iterator;
   typedef typename segmented_iterator_traits<dst_local_iterator>::is_segmented_iterator dst_is_local_seg_t;

   dst_segment_iterator       dsfirst = dst_traits::segment(dst_first);
   const dst_segment_iterator dslast  = dst_traits::segment(dst_last);

   if(dsfirst == dslast) {
      segduo<SrcIter, dst_local_iterator> r = (segmented_copy_if_dst_bounded)
         (first, last, dst_traits::local(dst_first), dst_traits::local(dst_last), pred, dst_is_local_seg_t(), SrcCat());
      return segduo<SrcIter, SegDstIter>(r.first, dst_traits::compose(dsfirst, r.second));
   }
   else {
      segduo<SrcIter, dst_local_iterator> r = (segmented_copy_if_dst_bounded)
         (first, last, dst_traits::local(dst_first), dst_traits::end(dsfirst), pred, dst_is_local_seg_t(), SrcCat());
      if (r.first != last) {
         first = r.first;

         for (++dsfirst; dsfirst != dslast; ++dsfirst) {
            r = (segmented_copy_if_dst_bounded)
               (first, last, dst_traits::begin(dsfirst), dst_traits::end(dsfirst), pred, dst_is_local_seg_t(), SrcCat());
            first = r.first;
            if (first == last)
               return segduo<SrcIter, SegDstIter>(first, dst_traits::compose(dsfirst, r.second));
         }

         r = (segmented_copy_if_dst_bounded)
            (first, last, dst_traits::begin(dslast), dst_traits::local(dst_last), pred, dst_is_local_seg_t(), SrcCat());
      }
      return segduo<SrcIter, SegDstIter>(r.first, dst_traits::compose(dsfirst, r.second));
   }
}

//////////////////////////////////////////////////////////////////////////////
// Destination dispatch: routes to bounded helper.
// Non-segmented destination: single unbounded call (unreachable_sentinel_t).
// Segmented destination: loop over destination segments, bounded per segment.
//////////////////////////////////////////////////////////////////////////////

template <class SrcIter, class Sent, class DstIter, class Pred, class Cat>
BOOST_CONTAINER_FORCEINLINE DstIter segmented_copy_if_dst_dispatch
   (SrcIter first, Sent last, DstIter result, Pred pred,
    const non_segmented_iterator_tag &, Cat)
{
   return (segmented_copy_if_dst_bounded)
      (first, last, result, unreachable_sentinel_t(), pred, non_segmented_iterator_tag(), Cat()).second;
}

template <class SrcIter, class Sent, class SegDstIter, class Pred, class Cat>
SegDstIter segmented_copy_if_dst_dispatch
   (SrcIter first, Sent last, SegDstIter result, Pred pred,
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

   while (1) {
      const segduo<SrcIter, dst_local_iterator> r = (segmented_copy_if_dst_bounded)
         (first, last, dst_local, dst_traits::end(dst_seg), pred, dst_is_local_seg_t(), Cat());
      first = r.first;
      if(first != last) {
         ++dst_seg;
         dst_local = dst_traits::begin(dst_seg);
      }
      else {
         return dst_traits::compose(dst_seg, r.second);
      }
   }
}

//////////////////////////////////////////////////////////////////////////////
// Source dispatch: walks the source (read pointer) segments
//////////////////////////////////////////////////////////////////////////////

template <class SrcIter, class Sent, class OutIter, class Pred, class Tag, class Cat>
BOOST_CONTAINER_FORCEINLINE 
typename algo_enable_if_c<
   !Tag::value || is_sentinel<Sent, SrcIter>::value, OutIter>::type
   segmented_copy_if_dispatch(SrcIter first, Sent last, OutIter result, Pred pred, Tag, Cat)
{
#if !defined(BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO)
   typedef segmented_iterator_traits<OutIter> dst_traits;
   return (segmented_copy_if_dst_dispatch)
      (first, last, result, pred, typename dst_traits::is_segmented_iterator(), Cat());
#else
   return (segmented_copy_if_dst_dispatch)
      (first, last, result, pred, non_segmented_iterator_tag(), Cat());
#endif
}

template <class SegIter, class OutIter, class Pred, class Cat>
OutIter segmented_copy_if_dispatch
   (SegIter first, SegIter last, OutIter result, Pred pred, segmented_iterator_tag, Cat)
{
   typedef segmented_iterator_traits<SegIter>  src_traits;
   typedef typename src_traits::local_iterator   src_local_iterator;
   typedef typename src_traits::segment_iterator src_segment_iterator;
   typedef typename segmented_iterator_traits<src_local_iterator>::is_segmented_iterator src_is_local_seg_t;
   typedef typename iterator_traits<src_local_iterator>::iterator_category src_local_cat_t;

   src_segment_iterator sfirst = src_traits::segment(first);
   const src_segment_iterator slast  = src_traits::segment(last);

   if(sfirst == slast) {
      return (segmented_copy_if_dispatch)
         (src_traits::local(first), src_traits::local(last), result, pred, src_is_local_seg_t(), src_local_cat_t());
   }
   else {
      result = (segmented_copy_if_dispatch)
         (src_traits::local(first), src_traits::end(sfirst), result, pred, src_is_local_seg_t(), src_local_cat_t());

      for(++sfirst; sfirst != slast; ++sfirst)
         result = (segmented_copy_if_dispatch)
            (src_traits::begin(sfirst), src_traits::end(sfirst), result, pred, src_is_local_seg_t(), src_local_cat_t());

      return (segmented_copy_if_dispatch)
         (src_traits::begin(slast), src_traits::local(last), result, pred, src_is_local_seg_t(), src_local_cat_t());
   }
}

} // namespace detail_algo

//! Copies elements satisfying \c pred from [first, last) to the range
//! beginning at \c result. Segmentation is exploited on both input and
//! output ranges.
template <class InIter, class Sent, class OutIter, class Pred>
BOOST_CONTAINER_FORCEINLINE
OutIter segmented_copy_if(InIter first, Sent last, OutIter result, Pred pred)
{
   typedef segmented_iterator_traits<InIter> traits;
   return detail_algo::segmented_copy_if_dispatch(first, last, result, pred,
      typename traits::is_segmented_iterator(), typename iterator_traits<InIter>::iterator_category());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_COPY_IF_HPP
