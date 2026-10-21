//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_IS_SORTED_UNTIL_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_IS_SORTED_UNTIL_HPP

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

namespace detail_algo {

// Recursive dispatch for `segmented_is_sorted_until`.
//
// Cross-segment state is carried by value and returned in segduo<Iter, DeepIt>:
//   - `prev`      : iterator (of the deepest non-segmented local type) pointing
//                   at the most recently examined element.
//
// All sorted_until_rec overloads return segduo<Iter, DeepIt> where .first is
// the result iterator and .second is the updated prev value.  Passing prev by
// value instead of by reference avoids pointer aliasing and lets the compiler
// keep prev in a register throughout the inner loop.

// (1) Non-segmented base case.
//     At this level FwdIt == DeepIt (the deepest type computed from the top).
template <class FwdIt, class Sent, class Comp, class DeepIt, class Cat>
BOOST_CONTAINER_FORCEINLINE
segduo<FwdIt, DeepIt> sorted_until_rec
   (FwdIt first, Sent last, Comp comp,
    DeepIt prev, const non_segmented_iterator_tag &, Cat)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for (; first != last; ++first) {
      if (comp(*first, *prev))
         break;
      prev = detail_algo::deepest_local_iterator<FwdIt>::get(first);
   }
   return segduo<FwdIt, DeepIt>(first, prev);
}

// (2) Segmented iterator with a heterogeneous sentinel as end.
//     Cannot locate `last`'s segment, so falls back to a simple linear loop.
template <class SegIter, class Sent, class Comp, class DeepIt, class Tag, class Cat>
BOOST_CONTAINER_FORCEINLINE
typename algo_enable_if_c< is_sentinel<Sent, SegIter>::value
                         , segduo<SegIter, DeepIt> >::type
sorted_until_rec
   (SegIter first, Sent last, Comp comp,
    DeepIt prev, const Tag &, const Cat &cat)
{
   return sorted_until_rec
      (first, last, comp, prev, non_segmented_iterator_tag(), cat);
}

// (3) Segmented iterator with a matching segmented iterator as end.
//     Recursive: decomposes the range and dispatches each sub-range back to
//     `sorted_until_rec`, so recursive segmentation is exploited at every
//     level.  `prev` is threaded by value through the segduo return type.
template <class SegIter, class Comp, class DeepIt, class Cat>
segduo<SegIter, DeepIt> sorted_until_rec
   (SegIter first, SegIter last, Comp comp,
    DeepIt prev,
    const segmented_iterator_tag &, Cat)
{
   typedef segmented_iterator_traits<SegIter>                             traits;
   typedef typename traits::segment_iterator                              segment_iterator;
   typedef typename traits::local_iterator                                local_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator
                                                                          is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category    local_cat_t;
   typedef segduo<local_iterator, DeepIt>                                 local_result_t;

   segment_iterator       sfirst = traits::segment(first);
   const segment_iterator slast  = traits::segment(last);

   if (sfirst == slast) {
      const local_iterator ll = traits::local(last);
      const local_result_t r = (sorted_until_rec)
         (traits::local(first), ll, comp, prev, is_local_seg_t(), local_cat_t());
      return segduo<SegIter, DeepIt>(traits::compose(sfirst, r.first), r.second);
   }
   else {
      local_iterator le = traits::end(sfirst);
      local_result_t r = (sorted_until_rec)
         (traits::local(first), le, comp, prev, is_local_seg_t(), local_cat_t());
      if (r.first != le)
         return segduo<SegIter, DeepIt>(traits::compose(sfirst, r.first), r.second);

      for (++sfirst; sfirst != slast; ++sfirst) {
         le = traits::end(sfirst);
         r = (sorted_until_rec)
            (traits::begin(sfirst), le, comp, r.second, is_local_seg_t(), local_cat_t());
         if (r.first != le)
            return segduo<SegIter, DeepIt>(traits::compose(sfirst, r.first), r.second);
      }
      r = (sorted_until_rec)
         (traits::begin(slast), traits::local(last), comp, r.second, is_local_seg_t(), local_cat_t());
      return segduo<SegIter, DeepIt>(traits::compose(sfirst, r.first), r.second);
   }
}

} // namespace detail_algo

//! Returns an iterator to the first element that is less than its predecessor
//! in [first, last), using \c comp for comparison.
//! Returns \c last if the range is sorted.
template <class FwdIt, class Sent, class Comp>
inline
FwdIt segmented_is_sorted_until(FwdIt first, Sent last, Comp comp)
{
   if (first == last)
      return first;

   typedef segmented_iterator_traits<FwdIt>           traits;
   typedef detail_algo::deepest_local_iterator<FwdIt> deep_it_helper;
   typedef typename deep_it_helper::type              deep_it;

   deep_it prev = deep_it_helper::get(first);
   return detail_algo::sorted_until_rec
      (++first, last, comp, prev,
         typename traits::is_segmented_iterator(),
         typename iterator_traits<FwdIt>::iterator_category()).first;
}

//! Returns an iterator to the first element that is less than its predecessor
//! in [first, last), using operator<.
//! Returns \c last if the range is sorted.
template <class FwdIt, class Sent>
inline
FwdIt segmented_is_sorted_until(FwdIt first, Sent last)
{
   if (first == last)
      return first;

   typedef segmented_iterator_traits<FwdIt>           traits;
   typedef detail_algo::deepest_local_iterator<FwdIt> deep_it_helper;
   typedef typename deep_it_helper::type              deep_it;

   deep_it prev = deep_it_helper::get(first);
   return detail_algo::sorted_until_rec
      (++first, last, detail_algo::segmented_default_less(), prev,
         typename traits::is_segmented_iterator(),
         typename iterator_traits<FwdIt>::iterator_category()).first;
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_IS_SORTED_UNTIL_HPP