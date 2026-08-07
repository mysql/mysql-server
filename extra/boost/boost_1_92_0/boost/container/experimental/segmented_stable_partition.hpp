//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_STABLE_PARTITION_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_STABLE_PARTITION_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_iterator_traits.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/container/detail/iterator.hpp>

namespace boost {
namespace container {

template <class BidirIt, class Pred>
BidirIt segmented_stable_partition(BidirIt first, BidirIt last, Pred pred);

namespace detail_algo {

template <class BidirIt, class T>
BidirIt stable_partition_shift( BidirIt it, BidirIt result, T& tmp)
{
   BidirIt cur = it;
   while(cur != result) {
      BidirIt prev = cur; --prev;
      *cur = boost::move(*prev);
      cur = prev;
   }
   *result = boost::move(tmp);
   return ++result;
}

// Composes a local iterator back to the outermost segmented iterator.
// At the top level, identity_composer just returns the iterator as-is.
// At each recursion level, chained_composer calls traits::compose to go
// one level up, then delegates to the outer composer.
template <class SegIter>
struct sp_identity_composer
{
   typedef SegIter result_type;
   BOOST_CONTAINER_FORCEINLINE result_type operator()(SegIter it) const { return it; }
};

template <class OuterComposer, class Traits>
struct sp_chained_composer
{
   typedef typename OuterComposer::result_type result_type;
   OuterComposer outer_;
   typename Traits::segment_iterator seg_;

   BOOST_CONTAINER_FORCEINLINE sp_chained_composer(OuterComposer o, typename Traits::segment_iterator s)
      : outer_(o), seg_(s) {}

   BOOST_CONTAINER_FORCEINLINE result_type operator()(typename Traits::local_iterator l) const
   { return outer_(Traits::compose(seg_, l)); }
};

template <class FwdIt, class OuterIter, class Composer, class Pred>
OuterIter stable_partition_scan(FwdIt first, FwdIt last, OuterIter result, Composer composer, Pred pred, non_segmented_iterator_tag)
{
   typedef typename boost::container::iterator_traits<OuterIter>::value_type value_type;
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; first != last; ++first) {
      if(pred(*first)) {
         value_type tmp = boost::move(*first);
         OuterIter it = composer(first);
         result = stable_partition_shift(it, result, tmp);
      }
   }
   return result;
}

template <class SegIt, class OuterIter, class Composer, class Pred>
OuterIter stable_partition_scan(SegIt first, SegIt last, OuterIter result,
   Composer composer, Pred& pred, segmented_iterator_tag)
{
   typedef segmented_iterator_traits<SegIt>   traits;
   typedef typename traits::local_iterator    local_iterator;
   typedef typename traits::segment_iterator  segment_iterator;
   typedef typename segmented_iterator_traits
      <local_iterator>::is_segmented_iterator is_local_seg_t;

   typedef sp_chained_composer<Composer, traits> composer_t;

   segment_iterator       scur  = traits::segment(first);
   segment_iterator const slast = traits::segment(last);
   local_iterator         lcur  = traits::local(first);

   if(scur == slast) {
      result = stable_partition_scan(lcur, traits::local(last), result, composer_t(composer, scur), pred, is_local_seg_t());
   }
   else {
      result = stable_partition_scan(lcur, traits::end(scur), result, composer_t(composer, scur), pred, is_local_seg_t());
      for(++scur; scur != slast; ++scur) {
         result = stable_partition_scan(traits::begin(scur), traits::end(scur), result, composer_t(composer, scur), pred, is_local_seg_t());
      }
      result = stable_partition_scan(traits::begin(scur), traits::local(last), result, composer_t(composer, scur), pred,is_local_seg_t());
   }
   return result;
}

template <class SegIter, class Pred>
BOOST_CONTAINER_FORCEINLINE
SegIter segmented_stable_partition_dispatch (SegIter first, SegIter last, Pred pred, segmented_iterator_tag)
{
   return (stable_partition_scan)(first, last, first, sp_identity_composer<SegIter>(), pred, segmented_iterator_tag());
}

template <class BidirIt, class Pred>
BOOST_CONTAINER_FORCEINLINE
BidirIt segmented_stable_partition_dispatch (BidirIt first, BidirIt last, Pred pred, non_segmented_iterator_tag)
{
   return (stable_partition_scan)(first, last, first, sp_identity_composer<BidirIt>(), pred, non_segmented_iterator_tag());
}

} // namespace detail_algo

//! Note: This version is suboptimal only supports bidirectional iterators,
//! as it relies on stable_partition_shift.
//! 
//! Reorders elements in [first, last) so that elements satisfying
//! \c pred come before those that do not, preserving relative order
//! within each group. Returns an iterator to the partition point.
template <class BidirIt, class Pred>
BOOST_CONTAINER_FORCEINLINE
BidirIt segmented_stable_partition(BidirIt first, BidirIt last, Pred pred)
{
   typedef segmented_iterator_traits<BidirIt> traits;
   return detail_algo::segmented_stable_partition_dispatch
      (first, last, pred, typename traits::is_segmented_iterator());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_STABLE_PARTITION_HPP
