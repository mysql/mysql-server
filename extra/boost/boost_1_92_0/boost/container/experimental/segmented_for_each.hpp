//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_FOR_EACH_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_FOR_EACH_HPP

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

template <class InpIter, class Sent, class F>
F segmented_for_each(InpIter first, Sent last, F f);

namespace detail_algo {

template <class InpIter, class Sent, class F, class Tag, class Cat>
typename algo_enable_if_c<
   !Tag::value || is_sentinel<Sent, InpIter>::value, F>::type
BOOST_CONTAINER_FORCEINLINE
segmented_for_each_dispatch
   (InpIter first, Sent last, F f, Tag, Cat)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; first != last; ++first)
      f(*first);
   return f;
}

template <class SegIter, class F, class Cat>
F segmented_for_each_dispatch
   (SegIter first, SegIter last, F f, segmented_iterator_tag, Cat)
{
   typedef segmented_iterator_traits<SegIter> traits;
   typedef typename traits::local_iterator    local_iterator;
   typedef typename traits::segment_iterator  segment_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category local_cat_t;

   segment_iterator sfirst = traits::segment(first);
   segment_iterator slast  = traits::segment(last);

   if(sfirst == slast) {
      return (segmented_for_each_dispatch)(traits::local(first), traits::local(last), boost::move(f), is_local_seg_t(), local_cat_t());
   }
   else {
      f = (segmented_for_each_dispatch)(traits::local(first), traits::end(sfirst), boost::move(f), is_local_seg_t(), local_cat_t());
      for(++sfirst; sfirst != slast; ++sfirst)
         f = (segmented_for_each_dispatch)(traits::begin(sfirst), traits::end(sfirst), boost::move(f), is_local_seg_t(), local_cat_t());
      return (segmented_for_each_dispatch)(traits::begin(sfirst), traits::local(last), boost::move(f), is_local_seg_t(), local_cat_t());
   }
}

} // namespace detail_algo

//! Applies \c f to every element in [first, last).
//! Returns the (possibly modified) function object.
//! When \c Iter is a segmented iterator, exploits segmentation
//! to reduce per-element overhead.
template <class InpIter, class Sent, class F>
BOOST_CONTAINER_FORCEINLINE
F segmented_for_each(InpIter first, Sent last, F f)
{
   typedef segmented_iterator_traits<InpIter> traits;
   return detail_algo::segmented_for_each_dispatch
      (first, last, boost::move(f), typename traits::is_segmented_iterator(), typename iterator_traits<InpIter>::iterator_category());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_FOR_EACH_HPP
