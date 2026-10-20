//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_EQUAL_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_EQUAL_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_iterator_traits.hpp>
#include <boost/container/detail/iterators.hpp>

namespace boost {
namespace container {

template <class InpIter1, class Sent, class InpIter2, class BinaryPred>
bool segmented_equal(InpIter1 first1, Sent last1, InpIter2 first2, BinaryPred pred);

template <class InpIter1, class Sent, class InpIter2>
bool segmented_equal(InpIter1 first1, Sent last1, InpIter2 first2);

namespace detail_algo {

struct equal_pred
{
   template <class T, class U>
   BOOST_CONTAINER_FORCEINLINE bool operator()(const T& a, const U& b) const { return a == b; }
};

//////////////////////////////////////////////////////////////////////////////
// Bounded iter2 helper: compares source [first1, last1) against
// [first2, iter2_last), stopping when source, iter2, or a mismatch
// is encountered.
// Returns segduo<SrcIter, Iter2> with the final positions of both
// iterators.  Recursively walks iter2 segments when iter2 is segmented.
//
// When iter2_last is unreachable_sentinel_t the segment-boundary check
// is optimised away, giving the same code as an unbounded loop.
//////////////////////////////////////////////////////////////////////////////

template <class SrcIter, class Sent, class Iter2, class Iter2Sent, class BinaryPred, class Iter2Tag, class SrcCat>
BOOST_CONTAINER_FORCEINLINE
typename algo_enable_if_c<!Iter2Tag::value, segduo<SrcIter, Iter2> >::type
segmented_equal_iter2_bounded
   (SrcIter first1, Sent last1, Iter2 first2, Iter2Sent iter2_last, BinaryPred pred, Iter2Tag, SrcCat)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; first1 != last1; ++first1) {
      if(first2 == iter2_last)
         goto out_path;
      if(!pred(*first1, *first2))
         return segduo<SrcIter, Iter2>(first1, first2);
      ++first2;
   }
   out_path:
   return segduo<SrcIter, Iter2>(first1, first2);
}

template <class RASrcIter, class RAIter2, class BinaryPred>
BOOST_CONTAINER_FORCEINLINE
typename iterator_enable_if_tag
   <RAIter2, std::random_access_iterator_tag, segduo<RASrcIter, RAIter2> >::type
segmented_equal_iter2_bounded
   (RASrcIter first1, RASrcIter last1, RAIter2 first2, RAIter2 iter2_last, BinaryPred pred,
    const non_segmented_iterator_tag &, const std::random_access_iterator_tag &src_tag)
{
   typedef typename iterator_traits<RASrcIter>::difference_type difference_type;
   const difference_type src_n  = last1 - first1;
   const difference_type iter2_n = difference_type(iter2_last - first2);
   const difference_type n = src_n < iter2_n ? src_n : iter2_n;
   return (segmented_equal_iter2_bounded)(first1, first1 + n, first2, unreachable_sentinel_t(),
      pred, non_segmented_iterator_tag(), src_tag);
}

template <class SrcIter, class Sent, class SegIter2, class BinaryPred, class SrcCat>
segduo<SrcIter, SegIter2> segmented_equal_iter2_bounded
   (SrcIter first1, Sent last1, SegIter2 iter2_first, SegIter2 iter2_last, BinaryPred pred,
    segmented_iterator_tag, SrcCat)
{
   typedef segmented_iterator_traits<SegIter2>  iter2_traits;
   typedef typename iter2_traits::local_iterator    iter2_local_iterator;
   typedef typename iter2_traits::segment_iterator  iter2_segment_iterator;
   typedef typename segmented_iterator_traits<iter2_local_iterator>::is_segmented_iterator iter2_is_local_seg_t;

   iter2_segment_iterator       sfirst = iter2_traits::segment(iter2_first);
   const iter2_segment_iterator slast  = iter2_traits::segment(iter2_last);

   if(sfirst == slast) {
      segduo<SrcIter, iter2_local_iterator> r = (segmented_equal_iter2_bounded)
         (first1, last1, iter2_traits::local(iter2_first), iter2_traits::local(iter2_last), pred, iter2_is_local_seg_t(), SrcCat());
      return segduo<SrcIter, SegIter2>(r.first, iter2_traits::compose(sfirst, r.second));
   }
   else {
      iter2_local_iterator end2 = iter2_traits::end(sfirst);
      segduo<SrcIter, iter2_local_iterator> r = (segmented_equal_iter2_bounded)
         (first1, last1, iter2_traits::local(iter2_first), end2, pred, iter2_is_local_seg_t(), SrcCat());
      first1 = r.first;
      iter2_local_iterator loc2 = r.second;
      if(first1 != last1 && loc2 != end2)
         return segduo<SrcIter, SegIter2>(first1, iter2_traits::compose(sfirst, loc2));
      if(first1 == last1)
         return segduo<SrcIter, SegIter2>(first1, iter2_traits::compose(sfirst, loc2));

      for(++sfirst; sfirst != slast; ++sfirst) {
         end2 = iter2_traits::end(sfirst);
         r = (segmented_equal_iter2_bounded)
            (first1, last1, iter2_traits::begin(sfirst), end2, pred, iter2_is_local_seg_t(), SrcCat());
         first1 = r.first;
         loc2 = r.second;
         if(first1 != last1 && loc2 != end2)
            return segduo<SrcIter, SegIter2>(first1, iter2_traits::compose(sfirst, loc2));
         if(first1 == last1)
            return segduo<SrcIter, SegIter2>(first1, iter2_traits::compose(sfirst, loc2));
      }

      r = (segmented_equal_iter2_bounded)
         (first1, last1, iter2_traits::begin(slast), iter2_traits::local(iter2_last), pred, iter2_is_local_seg_t(), SrcCat());
      return segduo<SrcIter, SegIter2>(r.first, iter2_traits::compose(sfirst, r.second));
   }
}

//////////////////////////////////////////////////////////////////////////////
// Iter2 dispatch: routes to bounded helper.
// Non-segmented iter2: single unbounded call (unreachable_sentinel_t).
// Segmented iter2: loop over iter2 segments, bounded per segment.
//////////////////////////////////////////////////////////////////////////////

template <class SrcIter, class Sent, class InpIter2, class BinaryPred, class Cat>
BOOST_CONTAINER_FORCEINLINE segduo<bool, InpIter2> segmented_equal_iter2_dispatch
   (SrcIter first1, Sent last1, InpIter2 first2, BinaryPred pred,
    const non_segmented_iterator_tag &, Cat)
{
   segduo<SrcIter, InpIter2> r = (segmented_equal_iter2_bounded)
      (first1, last1, first2, unreachable_sentinel_t(), pred, non_segmented_iterator_tag(), Cat());
   return segduo<bool, InpIter2>(r.first == last1, r.second);
}

template <class SrcIter, class Sent, class SegIter2, class BinaryPred, class Cat>
segduo<bool, SegIter2> segmented_equal_iter2_dispatch
   (SrcIter first1, Sent last1, SegIter2 first2, BinaryPred pred,
    const segmented_iterator_tag &, Cat)
{
   typedef segmented_iterator_traits<SegIter2>  iter2_traits;
   typedef typename iter2_traits::local_iterator    iter2_local_iterator;
   typedef typename iter2_traits::segment_iterator  iter2_segment_iterator;
   typedef typename segmented_iterator_traits<iter2_local_iterator>::is_segmented_iterator iter2_is_local_seg_t;

   if(first1 == last1)
      return segduo<bool, SegIter2>(true, first2);

   iter2_segment_iterator seg2 = iter2_traits::segment(first2);
   iter2_local_iterator   loc2 = iter2_traits::local(first2);

   while(first1 != last1) {
      iter2_local_iterator end2 = iter2_traits::end(seg2);
      segduo<SrcIter, iter2_local_iterator> r = (segmented_equal_iter2_bounded)
         (first1, last1, loc2, end2, pred, iter2_is_local_seg_t(), Cat());
      first1 = r.first;
      loc2 = r.second;
      if(first1 != last1 && loc2 != end2)
         return segduo<bool, SegIter2>(false, iter2_traits::compose(seg2, loc2));
      if(first1 != last1) {
         ++seg2;
         loc2 = iter2_traits::begin(seg2);
      }
   }
   return segduo<bool, SegIter2>(true, iter2_traits::compose(seg2, loc2));
}

//////////////////////////////////////////////////////////////////////////////
// Source dispatch: walks the source (first1) segments
//////////////////////////////////////////////////////////////////////////////

template <class SrcIter, class Sent, class InpIter2, class BinaryPred, class Tag, class Cat>
BOOST_CONTAINER_FORCEINLINE
typename algo_enable_if_c
   < !Tag::value || is_sentinel<Sent, SrcIter>::value
   , segduo<bool, InpIter2>
   >::type
segmented_equal_dispatch(SrcIter first1, Sent last1, InpIter2 first2, BinaryPred pred, Tag, Cat)
{
#if !defined(BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO)
   typedef segmented_iterator_traits<InpIter2> iter2_traits;
   return (segmented_equal_iter2_dispatch)
      (first1, last1, first2, pred, typename iter2_traits::is_segmented_iterator(), Cat());
#else
   return (segmented_equal_iter2_dispatch)
      (first1, last1, first2, pred, non_segmented_iterator_tag(), Cat());
#endif
}

template <class SegIter, class InpIter2, class BinaryPred, class Cat>
segduo<bool, InpIter2> segmented_equal_dispatch
   (SegIter first1, SegIter last1, InpIter2 first2, BinaryPred pred, segmented_iterator_tag, Cat)
{
   typedef segmented_iterator_traits<SegIter>  traits;
   typedef typename traits::local_iterator     local_iterator;
   typedef typename traits::segment_iterator   segment_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category local_cat_t;

   segment_iterator       sfirst = traits::segment(first1);
   segment_iterator const slast  = traits::segment(last1);

   if(sfirst == slast) {
      return (segmented_equal_dispatch)
         (traits::local(first1), traits::local(last1), first2, pred, is_local_seg_t(), local_cat_t());
   }
   else {
      segduo<bool, InpIter2> r = (segmented_equal_dispatch)
         (traits::local(first1), traits::end(sfirst), first2, pred, is_local_seg_t(), local_cat_t());
      if(!r.first)
         return r;

      for(++sfirst; sfirst != slast; ++sfirst) {
         r = (segmented_equal_dispatch)
            (traits::begin(sfirst), traits::end(sfirst), r.second, pred, is_local_seg_t(), local_cat_t());
         if(!r.first)
            return r;
      }

      return (segmented_equal_dispatch)
         (traits::begin(slast), traits::local(last1), r.second, pred, is_local_seg_t(), local_cat_t());
   }
}

} // namespace detail_algo

//! Returns \c true if elements in [first1, last1) are equal to the
//! range starting at \c first2 according to \c pred.
//! Exploits segmentation on both ranges.
template <class InpIter1, class Sent, class InpIter2, class BinaryPred>
BOOST_CONTAINER_FORCEINLINE
bool segmented_equal(InpIter1 first1, Sent last1, InpIter2 first2, BinaryPred pred)
{
   typedef segmented_iterator_traits<InpIter1> traits;
   return detail_algo::segmented_equal_dispatch
      (first1, last1, first2, pred, typename traits::is_segmented_iterator(), typename iterator_traits<InpIter1>::iterator_category()).first;
}

//! Returns \c true if elements in [first1, last1) are equal to the
//! range starting at \c first2. Exploits segmentation on both ranges.
template <class InpIter1, class Sent, class InpIter2>
BOOST_CONTAINER_FORCEINLINE
bool segmented_equal(InpIter1 first1, Sent last1, InpIter2 first2)
{
   return boost::container::segmented_equal(first1, last1, first2, detail_algo::equal_pred());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_EQUAL_HPP
