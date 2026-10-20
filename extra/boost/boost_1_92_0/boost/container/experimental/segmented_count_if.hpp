//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_COUNT_IF_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_COUNT_IF_HPP

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

template <class InpIter, class Sent, class Pred>
typename boost::container::iterator_traits<InpIter>::difference_type
segmented_count_if(InpIter first, Sent last, Pred pred);

namespace detail_algo {

template <class InpIter, class Sent, class Pred, class Tag, class Cat>
typename algo_enable_if_c<
   !Tag::value || is_sentinel<Sent, InpIter>::value,
   typename boost::container::iterator_traits<InpIter>::difference_type>::type
segmented_count_if_dispatch
   (InpIter first, Sent last, Pred pred, Tag, Cat)
{
   typedef typename boost::container::iterator_traits<InpIter>::difference_type diff_t;
   diff_t n = 0;

   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for (; first != last; ++first)
   #if defined(BOOST_CONTAINER_SEGMENTED_COUNT_BRANCHLESS)
      n += static_cast<diff_t>(pred(*first));
   #else
      if (pred(*first)) ++n;
   #endif
   return n;
}

template <class SegIter, class Pred, class Cat>
typename boost::container::iterator_traits<SegIter>::difference_type
   segmented_count_if_dispatch(SegIter first, SegIter last, Pred pred, segmented_iterator_tag, Cat)
{
   typedef segmented_iterator_traits<SegIter> traits;
   typedef typename traits::segment_iterator  segment_iterator;
   typedef typename traits::local_iterator    local_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category local_cat_t;

   segment_iterator sfirst = traits::segment(first);
   segment_iterator slast  = traits::segment(last);

   if(sfirst == slast) {
      return (segmented_count_if_dispatch)(traits::local(first), traits::local(last), pred, is_local_seg_t(), local_cat_t());
   }
   else {
      typename boost::container::iterator_traits<SegIter>::difference_type result = 0;
      result += (segmented_count_if_dispatch)(traits::local(first), traits::end(sfirst), pred, is_local_seg_t(), local_cat_t());

      for(++sfirst; sfirst != slast; ++sfirst)
         result += (segmented_count_if_dispatch)(traits::begin(sfirst), traits::end(sfirst), pred, is_local_seg_t(), local_cat_t());

      return result += (segmented_count_if_dispatch)(traits::begin(sfirst), traits::local(last), pred, is_local_seg_t(), local_cat_t());
   }
}

} // namespace detail_algo

//! Returns the number of elements satisfying \c pred in [first, last).
template <class InpIter, class Sent, class Pred>
BOOST_CONTAINER_FORCEINLINE
typename boost::container::iterator_traits<InpIter>::difference_type
   segmented_count_if(InpIter first, Sent last, Pred pred)
{
   typedef segmented_iterator_traits<InpIter> traits;
   return detail_algo::segmented_count_if_dispatch(first, last, pred,
      typename traits::is_segmented_iterator(), typename iterator_traits<InpIter>::iterator_category());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_COUNT_IF_HPP
