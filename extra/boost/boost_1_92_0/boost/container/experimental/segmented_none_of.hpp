//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_NONE_OF_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_NONE_OF_HPP

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

template <class InpIter, class Sent, class Pred>
bool segmented_none_of(InpIter first, Sent last, Pred pred);

namespace detail_algo {

template <class InpIter, class Sent, class Pred, class Tag, class Cat>
BOOST_CONTAINER_FORCEINLINE
typename algo_enable_if_c<
   !Tag::value || is_sentinel<Sent, InpIter>::value, bool>::type
segmented_none_of_dispatch(InpIter first, Sent last, Pred pred, Tag, Cat)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; first != last; ++first)
      if(pred(*first))
         return false;
   return true;
}

template <class SegIter, class Pred, class Cat>
bool segmented_none_of_dispatch
   (SegIter first, SegIter last, Pred pred, segmented_iterator_tag, Cat)
{
   typedef segmented_iterator_traits<SegIter> traits;
   typedef typename traits::local_iterator    local_iterator;
   typedef typename traits::segment_iterator  segment_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category local_cat_t;

   segment_iterator       sfirst = traits::segment(first);
   const segment_iterator slast  = traits::segment(last);

   if(sfirst == slast) {
      if(!(segmented_none_of_dispatch)(traits::local(first), traits::local(last), pred, is_local_seg_t(), local_cat_t()))
         return false;
   }
   else {
      //First segment
      if(!(segmented_none_of_dispatch)(traits::local(first), traits::end(sfirst), pred, is_local_seg_t(), local_cat_t()))
         return false;
      //Middle segments
      for (++sfirst; sfirst != slast; ++sfirst) {
         if(!(segmented_none_of_dispatch)(traits::begin(sfirst), traits::end(sfirst), pred, is_local_seg_t(), local_cat_t()))
            return false;
      }
      //Last segment
      if(!(segmented_none_of_dispatch)(traits::begin(slast), traits::local(last), pred, is_local_seg_t(), local_cat_t()))
         return false;
   }
   return true;
}

} // namespace detail_algo

//! Returns \c true if \c pred returns false for all elements
//! in [first, last), or if the range is empty.
template <class InpIter, class Sent, class Pred>
BOOST_CONTAINER_FORCEINLINE
bool segmented_none_of(InpIter first, Sent last, Pred pred)
{
   typedef segmented_iterator_traits<InpIter> traits;
   return detail_algo::segmented_none_of_dispatch
      (first, last, pred, typename traits::is_segmented_iterator(), typename iterator_traits<InpIter>::iterator_category());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_NONE_OF_HPP
