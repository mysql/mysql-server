//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_FIND_LAST_IF_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_FIND_LAST_IF_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/iterator.hpp>
#include <boost/container/experimental/segmented_iterator_traits.hpp>

namespace boost {
namespace container {

template <class FwdIt, class Sent, class Pred>
FwdIt segmented_find_last_if(FwdIt first, Sent last, Pred pred);

namespace detail_algo {

//////////////////////////////////////////////
// Non-segmented scans
//////////////////////////////////////////////

template <class FwdIt, class Sent, class Pred>
BOOST_CONTAINER_FORCEINLINE
FwdIt find_last_if_scan(FwdIt first, Sent last, Pred pred,
                        non_segmented_iterator_tag, const std::forward_iterator_tag&)
{
   FwdIt result = last;
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for (; first != last; ++first)
      if (pred(*first))
         result = first;
   return result;
}

template <class BidirIt, class Pred>
BOOST_CONTAINER_FORCEINLINE
BidirIt find_last_if_scan(BidirIt first, BidirIt last, Pred pred,
                          non_segmented_iterator_tag, const std::bidirectional_iterator_tag&)
{
   BidirIt cur = last;
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   while (cur != first) {
      --cur;
      if (pred(*cur))
         return cur;
   }
   return last;
}

//////////////////////////////////////////////
// Segmented forward scan
//////////////////////////////////////////////

template <class SegIt, class Pred>
SegIt find_last_if_scan(SegIt first, SegIt last, Pred pred,
                        segmented_iterator_tag, const std::forward_iterator_tag&)
{
   typedef segmented_iterator_traits<SegIt>  traits;
   typedef typename traits::local_iterator   local_iterator;
   typedef typename traits::segment_iterator segment_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category               local_cat_t;

   SegIt result = last;
   segment_iterator       sfirst = traits::segment(first);
   const segment_iterator slast  = traits::segment(last);

   if (sfirst == slast) {
      return traits::compose
         (sfirst, find_last_if_scan(traits::local(first), traits::local(last), pred, is_local_seg_t(), local_cat_t()));
   }
   else {
      {  // First segment
         const local_iterator le = traits::end(sfirst);
         const local_iterator r = find_last_if_scan(traits::local(first), le, pred, is_local_seg_t(), local_cat_t());
         if (r != le)
            result = traits::compose(sfirst, r);
      }
         // Middle segments
      for (++sfirst; sfirst != slast; ++sfirst) {
         const local_iterator le = traits::end(sfirst);
         const local_iterator r = find_last_if_scan(traits::begin(sfirst), le, pred, is_local_seg_t(), local_cat_t());
         if (r != le)
            result = traits::compose(sfirst, r);
      }
      // Last segment
      return traits::compose
         (sfirst, find_last_if_scan(traits::begin(slast), traits::local(last), pred, is_local_seg_t(), local_cat_t()));
   }
}

//////////////////////////////////////////////
// Segmented bidirectional scan
//////////////////////////////////////////////

template <class SegIt, class Pred>
SegIt find_last_if_scan(SegIt first, SegIt last, Pred pred, segmented_iterator_tag, const std::bidirectional_iterator_tag&)
{
   typedef segmented_iterator_traits<SegIt>  traits;
   typedef typename traits::local_iterator   local_iterator;
   typedef typename traits::segment_iterator segment_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category local_cat_t;

   segment_iterator const sfirst = traits::segment(first);
   segment_iterator       slast  = traits::segment(last);
   const local_iterator ll = traits::local(last);

   if (sfirst == slast) {
      return traits::compose
         (sfirst, find_last_if_scan(traits::local(first), ll, pred, is_local_seg_t(), local_cat_t()));
   }

   {  // Last segment (partial): [begin(slast), local(last))
      local_iterator r = find_last_if_scan(traits::begin(slast), ll, pred, is_local_seg_t(), local_cat_t());
      if (r != ll)
         return traits::compose(slast, r);
   }

   // Middle segments in reverse
   for (--slast; slast != sfirst; --slast) {
      const local_iterator le = traits::end(slast);
      const local_iterator r = find_last_if_scan(traits::begin(slast), le, pred, is_local_seg_t(), local_cat_t());
      if (r != le)
         return traits::compose(slast, r);
   }

   {  // First segment (partial): [local(first), end(sfirst))
      const local_iterator le = traits::end(sfirst);
      const local_iterator r  = find_last_if_scan(traits::local(first), le, pred, is_local_seg_t(), local_cat_t());
      if (r != le)
         return traits::compose(sfirst, r);
   }

   return last;
}

//////////////////////////////////////////////
// Sentinel / generic fallback
//////////////////////////////////////////////

} // namespace detail_algo

//! Returns an iterator to the last element satisfying \c pred
//! in [first, last), or \c last if not found.
//! For bidirectional iterators, scans backward for early exit.
//! For forward iterators, scans the entire range and remembers
//! the last match.
template <class FwdIt, class Sent, class Pred>
BOOST_CONTAINER_FORCEINLINE
FwdIt segmented_find_last_if(FwdIt first, Sent last, Pred pred)
{
   typedef detail_algo::sent_filter<FwdIt, Sent> sf;
   return detail_algo::find_last_if_scan
      ( first, last, pred, typename sf::seg_t(), typename sf::cat_t());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_FIND_LAST_IF_HPP
