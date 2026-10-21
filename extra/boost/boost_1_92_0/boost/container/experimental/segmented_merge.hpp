//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_MERGE_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_MERGE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_iterator_traits.hpp>
#include <boost/container/experimental/segmented_copy.hpp>
#include <boost/container/detail/iterator.hpp>

namespace boost {
namespace container {

template <class InIter1, class Sent1, class InIter2, class Sent2, class OutIter, class Comp>
OutIter segmented_merge
   (InIter1 first1, Sent1 last1, InIter2 first2, Sent2 last2, OutIter result, Comp comp);

template <class InIter1, class Sent1, class InIter2, class Sent2, class OutIter>
OutIter segmented_merge
   (InIter1 first1, Sent1 last1, InIter2 first2, Sent2 last2, OutIter result);

namespace detail_algo {

//////////////////////////////////////////////////////////////////////////////
// Bounded destination helper: merges from [first1, last1) and [first2, last2)
// into [dst_first, dst_last), stopping when source 1, source 2, or
// destination is exhausted.  Returns segtrio<Iter1, Iter2, DstIter>
// with the final positions of all three iterators.  Recursively walks
// destination segments when dst is segmented.
//
// When dst_last is unreachable_sentinel_t the destination-full check
// is optimised away, giving the same code as an unbounded loop.
//
// Segmentation of the second range is exploited at a higher layer
// (merge_seg2_dispatch), which feeds this leaf one flat first2 segment at
// a time so the dual-RA / loop-unrolling fast paths can fire on a
// per-segment basis.
//////////////////////////////////////////////////////////////////////////////

template <class Iter1, class Sent1, class Iter2, class Sent2, class DstIter, class DstSent,
          class Comp, class DstTag, class SrcCat>
BOOST_CONTAINER_FORCEINLINE
typename algo_enable_if_c<!DstTag::value, segtrio<Iter1, Iter2, DstIter> >::type
merge_dst_bounded
   (Iter1 first1, Sent1 last1, Iter2 first2, Sent2 last2,
    DstIter dst_first, DstSent dst_last, Comp comp, DstTag, SrcCat)
{
   // Per-iteration the loop only checks exhaustion on the side that was
   // just consumed (the other side and dst position are unchanged by the
   // current step).  This matches the STL textbook merge structure and
   // produces tight branch-based code on MSVC/GCC: each branch of the
   // merge step ends with `break`, so the loop-bottom test reflects the
   // consumed side only and MSVC emits a single cmp+branch per element.
   // On clang the same shape is preserved, and because each branch ends
   // in a different exhaustion check, if-conversion to a cmov chain is
   // prevented (cmov would serialise the iteration and cost ~3x on
   // predictable data).  When dst_last is unreachable_sentinel_t the
   // dst-full check folds away.
   if(first1 != last1 && first2 != last2 && dst_first != dst_last) {
      while(true) {
         if(comp(*first2, *first1)) {
            *dst_first = *first2;
            ++first2;
            ++dst_first;
            if(first2 == last2 || dst_first == dst_last)
               break;
         }
         else {
            *dst_first = *first1;
            ++first1;
            ++dst_first;
            if(first1 == last1 || dst_first == dst_last)
               break;
         }
      }
   }
   return segtrio<Iter1, Iter2, DstIter>(first1, first2, dst_first);
}

template <class Iter1, class Sent1, class Iter2, class Sent2, class SegDstIter,
          class Comp, class SrcCat>
segtrio<Iter1, Iter2, SegDstIter> merge_dst_bounded
   (Iter1 first1, Sent1 last1, Iter2 first2, Sent2 last2,
    SegDstIter dst_first, SegDstIter dst_last, Comp comp,
    segmented_iterator_tag, SrcCat)
{
   typedef segmented_iterator_traits<SegDstIter>  dst_traits;
   typedef typename dst_traits::local_iterator    dst_local_iterator;
   typedef typename dst_traits::segment_iterator  dst_segment_iterator;
   typedef typename segmented_iterator_traits<dst_local_iterator>::is_segmented_iterator dst_is_local_seg_t;
   typedef segtrio<Iter1, Iter2, dst_local_iterator>  local_result_t;
   typedef segtrio<Iter1, Iter2, SegDstIter>          result_t;

   dst_segment_iterator       sfirst = dst_traits::segment(dst_first);
   const dst_segment_iterator slast  = dst_traits::segment(dst_last);

   if(sfirst == slast) {
      local_result_t r = (merge_dst_bounded)
         ( first1, last1, first2, last2, dst_traits::local(dst_first)
         , dst_traits::local(dst_last), comp, dst_is_local_seg_t(), SrcCat());
      return result_t(r.first, r.second, dst_traits::compose(sfirst, r.third));
   }
   else {
      local_result_t r = (merge_dst_bounded)
         ( first1, last1, first2, last2, dst_traits::local(dst_first)
         , dst_traits::end(sfirst), comp, dst_is_local_seg_t(), SrcCat());
      if(r.first == last1 || r.second == last2)
         goto exit;

      for(++sfirst; sfirst != slast; ++sfirst) {
         r = (merge_dst_bounded)
            ( r.first, last1, r.second, last2, dst_traits::begin(sfirst)
            , dst_traits::end(sfirst), comp, dst_is_local_seg_t(), SrcCat());
         if (r.first == last1 || r.second == last2)
            goto exit;
      }

      r = (merge_dst_bounded)
         ( first1, last1, first2, last2, dst_traits::begin(slast)
         , dst_traits::local(dst_last), comp, dst_is_local_seg_t(), SrcCat());
      exit:
      return result_t(r.first, r.second, dst_traits::compose(sfirst, r.third));
   }
}

//////////////////////////////////////////////////////////////////////////////
// merge_until_exhausts: primitive merge that drains [first1, last1) and
// [first2, last2) into result, stopping as soon as first1 OR first2 is
// exhausted.  No first1 draining is performed on return; the caller is
// responsible for handling any residual first1 elements (via segmented_copy
// or by continuing to a fresh first2 segment).  Returns
// segtrio<Iter1, Iter2, DstIter> with the final positions of all
// three iterators.
//
// Non-segmented destination: single bounded merge with unreachable_sentinel
//   so the dst-full check is folded away.
// Segmented destination: walk dst segments, merging bounded into each, until
//   first1 or first2 exhausts.  Composes the final SegDstIter on return.
//////////////////////////////////////////////////////////////////////////////

template <class Iter1, class Sent1, class Iter2, class Sent2, class DstIter,
          class Comp, class Cat, class Tag>
BOOST_CONTAINER_FORCEINLINE
segtrio<Iter1, Iter2, DstIter> merge_until_exhausts
   (Iter1 first1, Sent1 last1, Iter2 first2, Sent2 last2, DstIter result, Comp comp,
    const /*non_segmented_iterator_tag*/Tag &, const Cat &src1_cat)
{
   return (merge_dst_bounded)
      (first1, last1, first2, last2, result, unreachable_sentinel_t(),
       comp, non_segmented_iterator_tag(), src1_cat);
}

template <class Iter1, class Sent1, class Iter2, class Sent2, class SegDstIter,
          class Comp, class Cat>
segtrio<Iter1, Iter2, SegDstIter> merge_until_exhausts
   (Iter1 first1, Sent1 last1, Iter2 first2, Sent2 last2, SegDstIter result, Comp comp,
    const segmented_iterator_tag &, const Cat &src1_cat)
{
   typedef segmented_iterator_traits<SegDstIter>  dst_traits;
   typedef typename dst_traits::local_iterator    dst_local_iterator;
   typedef typename dst_traits::segment_iterator  dst_segment_iterator;
   typedef typename segmented_iterator_traits<dst_local_iterator>::is_segmented_iterator dst_is_local_seg_t;
   typedef segtrio<Iter1, Iter2, dst_local_iterator>  bounded_t;
   typedef segtrio<Iter1, Iter2, SegDstIter>          result_t;

   if(first1 == last1 || first2 == last2)
      return result_t(first1, first2, result);

   dst_segment_iterator dst_seg   = dst_traits::segment(result);
   dst_local_iterator   dst_local = dst_traits::local(result);

   while(1) {
      const dst_local_iterator dst_end = dst_traits::end(dst_seg);
      const bounded_t r = (merge_dst_bounded)
         (first1, last1, first2, last2, dst_local, dst_end, comp,
          dst_is_local_seg_t(), src1_cat);
      first1    = r.first;
      first2    = r.second;
      dst_local = r.third;

      if(dst_local != dst_end) {
         return result_t(first1, first2, dst_traits::compose(dst_seg, dst_local));
      }
      // dst segment full; advance to the next.
      ++dst_seg;
      dst_local = dst_traits::begin(dst_seg);
   }
}

//////////////////////////////////////////////////////////////////////////////
// merge_seg2_dispatch: merges [first1, last1) and [first2, last2) into
// result, stopping as soon as first1 OR first2 is exhausted.  No draining
// of source residue is performed at this layer; the outermost
// segmented_merge handles that via segmented_copy.
//
// Non-segmented first2: forwards directly to merge_until_exhausts.
//
// Segmented first2: walks the segments of first2 so each inner call sees
// a local (typically flat, random-access) first2 iterator, recovering the
// dual-RA / loop-unrolling fast paths on a per-segment basis.  Recursion
// is on the local iterator's segmentation tag, so nested (multi-level)
// segmentation in first2 is handled the same way merge_scan handles
// nested segmentation in first1.
//
// Returns segtrio<Iter1, Iter2, OutIter> with the final positions of all
// three iterators.  When first2 is segmented the second component is the
// composed segmented iterator.
//////////////////////////////////////////////////////////////////////////////

template <class Iter1, class Sent1, class Iter2, class Sent2, class OutIter, class Comp, class Cat>
BOOST_CONTAINER_FORCEINLINE segtrio<Iter1, Iter2, OutIter> merge_seg2_dispatch
(Iter1 first1, Sent1 last1, Iter2 first2, Sent2 last2, OutIter result, Comp comp,
   non_segmented_iterator_tag, const Cat& src1_cat)
{
#if !defined(BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO)
   typedef segmented_iterator_traits<OutIter>  out_traits;
   typedef typename out_traits::is_segmented_iterator is_out_seg_t;
   return (merge_until_exhausts)
      (first1, last1, first2, last2, result, comp, is_out_seg_t(), src1_cat);
#else
   return (merge_dst_bounded)
      (first1, last1, first2, last2, result, unreachable_sentinel_t(),
       comp, non_segmented_iterator_tag(), src1_cat);
#endif
}

template <class Iter1, class Sent1, class SegIter2, class OutIter, class Comp, class Cat>
segtrio<Iter1, SegIter2, OutIter> merge_seg2_dispatch
   (Iter1 first1, Sent1 last1, SegIter2 first2, SegIter2 last2, OutIter result, Comp comp,
    segmented_iterator_tag, const Cat & cat)
{
   typedef segmented_iterator_traits<SegIter2>          src2_traits;
   typedef typename src2_traits::local_iterator         src2_local_iterator;
   typedef typename src2_traits::segment_iterator       src2_segment_iterator;
   typedef typename segmented_iterator_traits
      <src2_local_iterator>::is_segmented_iterator      src2_is_local_seg_t;
   typedef segtrio<Iter1, src2_local_iterator, OutIter> local_result_t;
   typedef segtrio<Iter1, SegIter2, OutIter>            result_t;

   if(first1 == last1 || first2 == last2)
      return result_t(first1, first2, result);

   src2_segment_iterator       sf2 = src2_traits::segment(first2);
   const src2_segment_iterator sl2 = src2_traits::segment(last2);
   src2_local_iterator         lf2 = src2_traits::local(first2);

   if(sf2 == sl2) {
      local_result_t r = (merge_seg2_dispatch)
         (first1, last1, lf2, src2_traits::local(last2), result, comp,
          src2_is_local_seg_t(), cat);
      return result_t(r.first, src2_traits::compose(sf2, r.second), r.third);
   }
   else {
      // First (partial) segment of first2.
      local_result_t r = (merge_seg2_dispatch)
         (first1, last1, lf2, src2_traits::end(sf2), result, comp,
          src2_is_local_seg_t(), cat);
      if (r.first == last1)
         goto exit;

      for(++sf2; sf2 != sl2; ++sf2) {
         r = (merge_seg2_dispatch)
            (r.first, last1, src2_traits::begin(sf2), src2_traits::end(sf2),
             r.third, comp, src2_is_local_seg_t(), cat);
         if(r.first == last1)
            goto exit;
      }

      // Final (partial) segment of first2.
      r = (merge_seg2_dispatch)
         (r.first, last1, src2_traits::begin(sf2), src2_traits::local(last2),
          r.third, comp, src2_is_local_seg_t(), cat);
      exit:
      return result_t(r.first, src2_traits::compose(sf2, r.second), r.third);
   }
}

//////////////////////////////////////////////////////////////////////////////
// merge_scan: merges [first1, last1) and [first2, last2) into result,
// stopping as soon as first1 OR first2 is exhausted.  No draining of source
// residue is performed at this layer; the outermost segmented_merge handles
// that via segmented_copy.
//
// Non-segmented first1: forwards to merge_seg2_dispatch (which exploits
// segmentation in first2 and dst).
//
// Segmented first1: walks the segments of first1 so each inner call sees a
// local first1 iterator, recursing on the local iterator's segmentation
// tag.  Bails out early as soon as first2 exhausts, so the remaining
// first1 segments are not visited and the residue is left to the
// outermost segmented_copy.
//
// Returns segtrio<Iter1, Iter2, OutIter> with the final positions of all
// three iterators.  When first1 is segmented the first component is the
// composed segmented iterator.
//////////////////////////////////////////////////////////////////////////////

template <class FwdIt, class Sent, class InIter2, class Sent2, class OutIter, class Comp>
BOOST_CONTAINER_FORCEINLINE segtrio<FwdIt, InIter2, OutIter> merge_scan
   (FwdIt first1, Sent last1, InIter2 first2, Sent2 last2, OutIter result, Comp comp,
    non_segmented_iterator_tag)
{
   typedef sent_filter<FwdIt, Sent> sf1;
#if !defined(BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO)
   typedef sent_filter<InIter2, Sent2> sf2;

   return (merge_seg2_dispatch)
      (first1, last1, first2, last2, result, comp,
       typename sf2::seg_t(),
       typename sf1::cat_t());
#else
   return (merge_until_exhausts)
      (first1, last1, first2, last2, result, comp,
       non_segmented_iterator_tag(), typename sf1::cat_t());
#endif
}

template <class SegIt, class InIter2, class Sent2, class OutIter, class Comp>
segtrio<SegIt, InIter2, OutIter> merge_scan
   (SegIt first, SegIt last, InIter2 first2, Sent2 last2, OutIter result, Comp comp, segmented_iterator_tag)
{
   typedef segmented_iterator_traits<SegIt>   traits;
   typedef typename traits::local_iterator    local_iterator;
   typedef typename traits::segment_iterator  segment_iterator;
   typedef typename segmented_iterator_traits
      <local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef segtrio<local_iterator, InIter2, OutIter> local_result_t;
   typedef segtrio<SegIt, InIter2, OutIter>          result_t;

   if(first == last || first2 == last2)
      return result_t(first, first2, result);

   segment_iterator       scur  = traits::segment(first);
   segment_iterator const slast = traits::segment(last);
   local_iterator         lcur  = traits::local(first);

   if(scur == slast) {
      local_result_t r = merge_scan
         (lcur, traits::local(last), first2, last2, result, comp, is_local_seg_t());
      return result_t(traits::compose(scur, r.first), r.second, r.third);
   }
   else {
      // First (partial) segment of first1.
      local_result_t r = merge_scan
         (lcur, traits::end(scur), first2, last2, result, comp, is_local_seg_t());
      if(r.second == last2)
         return result_t(traits::compose(scur, r.first), r.second, r.third);

      for(++scur; scur != slast; ++scur) {
         r = merge_scan
            (traits::begin(scur), traits::end(scur), r.second, last2, r.third, comp, is_local_seg_t());
         if(r.second == last2)
            return result_t(traits::compose(scur, r.first), r.second, r.third);
      }

      // Final (partial) segment of first1.
      r = merge_scan
         (traits::begin(scur), traits::local(last), r.second, last2, r.third, comp, is_local_seg_t());
      return result_t(traits::compose(scur, r.first), r.second, r.third);
   }
}

} // namespace detail_algo

// Top-level entry points are intentionally NOT BOOST_CONTAINER_FORCEINLINE.
// They compose the entire merge (scan + per-segment dispatch + dual drains),
// so the resulting body is large enough that force-inlining it into a hot
// caller (e.g. an unrolled benchmark Duff's device, or any size-tight
// outer loop) blows out MSVC's decoded-uop cache (DSB).  The standard
// library's std::merge is outlined by MSVC for the same reason and that's
// part of why it benchmarks well.  We mirror that policy here: leave the
// top-level call as a regular template function and let the compiler
// decide whether to inline at each call site.  All inner helpers
// (merge_scan, merge_seg2_dispatch, merge_until_exhausts, merge_dst_bounded)
// remain force-inlined so a single out-of-line top-level call still
// produces a tight, fully-monomorphised body.
template <class InIter1, class Sent1, class InIter2, class Sent2, class OutIter, class Comp>
inline OutIter segmented_merge
   (InIter1 first1, Sent1 last1, InIter2 first2, Sent2 last2, OutIter result, Comp comp)
{
   typedef detail_algo::sent_filter<InIter1, Sent1> sf;
   segtrio<InIter1, InIter2, OutIter> r = detail_algo::merge_scan
      ( first1, last1, first2, last2, result
      , comp, typename sf::seg_t());
   // Drain both source residues; at most one of these is non-empty.
   result = r.third;
   if(r.first != last1)
      result = (segmented_copy)(r.first, last1, result);
   return (r.second == last2) ? result : (segmented_copy)(r.second, last2, result);
}

template <class InIter1, class Sent1, class InIter2, class Sent2, class OutIter>
inline OutIter segmented_merge (InIter1 first1, Sent1 last1, InIter2 first2, Sent2 last2, OutIter result)
{
   typedef detail_algo::sent_filter<InIter1, Sent1> sf;
   segtrio<InIter1, InIter2, OutIter> r = detail_algo::merge_scan
      ( first1, last1, first2, last2, result
      , detail_algo::segmented_default_less()
      , typename sf::seg_t());
   // Drain both source residues; at most one of these is non-empty.
   result = r.third;
   if(r.first != last1)
      result = (segmented_copy)(r.first, last1, result);
   return (r.second == last2) ? result : (segmented_copy)(r.second, last2, result);
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_MERGE_HPP
