//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_SEARCH_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_SEARCH_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_iterator_traits.hpp>
#include <boost/container/experimental/segmented_find_if.hpp>
#include <boost/container/experimental/segmented_mismatch.hpp>

namespace boost {
namespace container {

template <class FwdIt1, class Sent1, class FwdIt2, class Sent2>
FwdIt1 segmented_search
   (FwdIt1 first, Sent1 last, FwdIt2 s_first, Sent2 s_last);

namespace detail_algo {

/* ---------------------------------------------------------------------------
   Original (non-recursive) implementation. Kept here for reference.
   It only exploits one level of segmentation on the [first, last) haystack
   and no segmentation on the [s_first, s_last) needle. The verification
   step (matches_at) walks the fully composed segmented iterator one element
   at a time, which pays the segmented-increment cost on every step.

template <class FwdIt1, class Sent, class FwdIt2, class Sent2>
bool matches_at(FwdIt1 pos, Sent last, FwdIt2 s_first, Sent2 s_last)
{
   for(; s_first != s_last; ++s_first, ++pos)
      if(pos == last || !(*pos == *s_first))
         return false;
   return true;
}

// Non-recursive: match verification (matches_at) must span across segment
// boundaries using the full segmented iterator.
template <class SegIter, class FwdIt2, class Sent2>
SegIter segmented_search_dispatch
   (SegIter first, SegIter last, FwdIt2 s_first, Sent2 s_last, segmented_iterator_tag)
{
   if(s_first == s_last) return first;

   typedef segmented_iterator_traits<SegIter> traits;
   typedef typename traits::local_iterator    local_iterator;
   typedef typename traits::segment_iterator  segment_iterator;

   segment_iterator scur  = traits::segment(first);
   segment_iterator slast = traits::segment(last);
   local_iterator   lcur  = traits::local(first);

   if(scur == slast) {
      local_iterator lend = traits::local(last);
      for(; lcur != lend; ++lcur) {
         if(*lcur == *s_first) {
            SegIter pos = traits::compose(scur, lcur);
            if(matches_at(pos, last, s_first, s_last))
               return pos;
         }
      }
   }
   else {
      {
         local_iterator lend = traits::end(scur);
         for(; lcur != lend; ++lcur) {
            if(*lcur == *s_first) {
               SegIter pos = traits::compose(scur, lcur);
               if(matches_at(pos, last, s_first, s_last))
                  return pos;
            }
         }
      }
      for(++scur; scur != slast; ++scur) {
         local_iterator lb = traits::begin(scur);
         local_iterator le = traits::end(scur);
         for(lcur = lb; lcur != le; ++lcur) {
            if(*lcur == *s_first) {
               SegIter pos = traits::compose(scur, lcur);
               if(matches_at(pos, last, s_first, s_last))
                  return pos;
            }
         }
      }
      {
         local_iterator lb = traits::begin(scur);
         local_iterator ll = traits::local(last);
         for(lcur = lb; lcur != ll; ++lcur) {
            if(*lcur == *s_first) {
               SegIter pos = traits::compose(scur, lcur);
               if(matches_at(pos, last, s_first, s_last))
                  return pos;
            }
         }
      }
   }
   return last;
}
--------------------------------------------------------------------------- */

//////////////////////////////////////////////////////////////////////////////
// Recursive segmented dispatch: find-then-verify.
//
// 1. segmented_find_if locates a candidate c where *c == *s_first, walking
//    the source recursively (exploits every level of segmentation and uses
//    the unrolled RA fast path at the leaves).
// 2. segmented_mismatch_bounded_dispatch verifies the match starting at
//    (c, s_first), bounded on both sides, recursive on both sides.
//
// The equal_to_deref predicate keeps the search proxy-safe: *s_first is
// re-evaluated on every comparison, so a prvalue proxy never outlives the
// call that produced it.
//////////////////////////////////////////////////////////////////////////////
template <class SegIter, class FwdIt2, class Sent2>
SegIter segmented_search_dispatch
   (SegIter first, SegIter last, FwdIt2 s_first, Sent2 s_last, segmented_iterator_tag)
{
   if (s_first == s_last)
      return first;

   typedef typename iterator_traits<SegIter>::iterator_category cat_t;

   equal_to_deref<FwdIt2> eq(s_first);

   while (first != last) {
      //Search for the first element of the needle. This exploits segmentation
      first = boost::container::segmented_find_if(first, last, eq);
      if (first == last)   // no match for the first needle element -> no match at all
         return last;

      //Verify the rest of the needle, bounded on both sides. This exploits segmentation
      segduo<SegIter, FwdIt2> r = (segmented_mismatch_bounded_dispatch)
         (first, last, s_first, s_last, mismatch_equal(), segmented_iterator_tag(), cat_t());

      if (r.second == s_last)
         return first;          // full needle consumed -> match
      if (r.first == last)
         return last;           // source exhausted before needle
      ++first;
   }
   return last;
}

template <class FwdIt1, class Sent1, class FwdIt2, class Sent2, class Tag>
typename algo_enable_if_c<
   !Tag::value || is_sentinel<Sent1, FwdIt1>::value, FwdIt1>::type
segmented_search_dispatch
   (FwdIt1 first, Sent1 last, FwdIt2 s_first, Sent2 s_last, Tag)
{
   if (s_first == s_last)
      return first;

   equal_to_deref<FwdIt2> eq(s_first);

   while (first != last) {
      first = boost::container::segmented_find_if(first, last, eq);
      if (first == last)
         return last;

      FwdIt1 it = first;
      FwdIt2 s_it = s_first;
      for(;;) {
         ++it;
         ++s_it;
         if(s_it == s_last)
            return first;
         if(it == last)
            return last;
         if(!(*it == *s_it))
            break;
      }
      ++first;
   }
   return last;
}

} // namespace detail_algo

//! Finds the first occurrence of the subsequence [s_first, s_last) in [first, last).
//! Returns an iterator to the beginning of the found subsequence, or \c last if not found.
//! Exploits segmentation recursively on both ranges.
template <class FwdIt1, class Sent1, class FwdIt2, class Sent2>
BOOST_CONTAINER_FORCEINLINE
FwdIt1 segmented_search(FwdIt1 first, Sent1 last, FwdIt2 s_first, Sent2 s_last)
{
   typedef segmented_iterator_traits<FwdIt1> traits;
   return detail_algo::segmented_search_dispatch
      (first, last, s_first, s_last, typename traits::is_segmented_iterator());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_SEARCH_HPP
