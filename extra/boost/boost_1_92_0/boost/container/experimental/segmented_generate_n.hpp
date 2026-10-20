//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_GENERATE_N_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_GENERATE_N_HPP

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
#include <boost/container/detail/std_fwd.hpp>

namespace boost {
namespace container {

template <class OutIt, class Size, class Generator>
OutIt segmented_generate_n(OutIt first, Size count, Generator gen);

namespace detail_algo {

template <class OutIter, class Size, class Generator>
BOOST_CONTAINER_FORCEINLINE
segduo<OutIter, Size> generate_n_scan
   ( OutIter first, OutIter last, Size count
   , Generator &BOOST_RESTRICT gen, non_segmented_iterator_tag, const std::random_access_iterator_tag &)
{
   const Size range_sz = static_cast<Size>(last - first);

   const Size length = count < (Size)range_sz ? count : (Size)range_sz;
   count -= length;
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for (Size cnt = 0; cnt != length; ++first, ++cnt){
      *first = gen();
   }
   return segduo<OutIter, Size>(first, count);
}

template <class OutIter, class Size, class Generator, class Cat>
BOOST_CONTAINER_FORCEINLINE
segduo<OutIter, Size> generate_n_scan
   (OutIter first, OutIter last, Size count, Generator &BOOST_RESTRICT gen, non_segmented_iterator_tag, const Cat &)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for (; count > 0 && first != last; ++first, --count)
      *first = gen();

   return segduo<OutIter, Size>(first, count);
}

template <class SegIt, class Size, class Generator, class Cat>
segduo<SegIt, Size> generate_n_scan(SegIt first, SegIt last, Size count, Generator& BOOST_RESTRICT gen, segmented_iterator_tag, const Cat &)
{
   typedef segmented_iterator_traits<SegIt>                                            traits;
   typedef typename traits::local_iterator                                             local_iterator;
   typedef typename traits::segment_iterator                                           segment_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator   is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category                 local_cat_t;

   segment_iterator       scur  = traits::segment(first);
   segment_iterator const slast = traits::segment(last);

   if(scur == slast) {
      const local_iterator ll = traits::local(last);
      const segduo<local_iterator, Size> r = (generate_n_scan)(traits::local(first), ll, count, gen, is_local_seg_t(), local_cat_t());
      return segduo<SegIt, Size>((r.first != ll) ? traits::compose(scur, r.first) : last, r.second);
   }
   else {
      segduo<local_iterator, Size> r = generate_n_scan(traits::local(first), traits::end(scur), count, gen, is_local_seg_t(), local_cat_t());
      count = r.second;
      if (!count)
         return segduo<SegIt, Size>(traits::compose(scur, r.first), count);

      for (++scur; scur != slast; ++scur) {
         r = generate_n_scan(traits::begin(scur), traits::end(scur), count, gen, is_local_seg_t(), local_cat_t());
         count = r.second;
         if (!count)
            return segduo<SegIt, Size>(traits::compose(scur, r.first), count);
      }
      const local_iterator ll = traits::local(last);
      r = generate_n_scan(traits::begin(slast), ll, count, gen, is_local_seg_t(), local_cat_t());
      return segduo<SegIt, Size>((r.first != ll) ? traits::compose(scur, r.first) : last, r.second);
   }
}

template <class SegIter, class Size, class Generator>
SegIter segmented_generate_n_ref
   (SegIter first, Size count, Generator& BOOST_RESTRICT gen, segmented_iterator_tag)
{
   typedef segmented_iterator_traits<SegIter> traits;
   typedef typename traits::local_iterator    local_iterator;
   typedef typename traits::segment_iterator  segment_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category local_cat_t;

   segment_iterator scur = traits::segment(first);
   local_iterator   lcur;

   {
      const segduo<local_iterator, Size> r = generate_n_scan(traits::local(first), traits::end(scur), count, gen, is_local_seg_t(), local_cat_t());
      lcur  = r.first;
      count = r.second;
   }

   if(count){
      ++scur;
      while(1) {
         const segduo<local_iterator, Size> r = generate_n_scan(traits::begin(scur), traits::end(scur), count, gen, is_local_seg_t(), local_cat_t());
         lcur  = r.first;
         count = r.second;
         if(count == 0)
            break;
         ++scur;
      }
   }
   return traits::compose(scur, lcur);
}

template <class OutIt, class Size, class Generator>
OutIt segmented_generate_n_ref
   (OutIt first, Size count, Generator& BOOST_RESTRICT gen, non_segmented_iterator_tag)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; count > 0; ++first, --count)
      *first = gen();
   return first;
}

} // namespace detail_algo

//! Assigns the result of successive calls to \c gen to the first
//! \c count elements starting at \c first. Generator state is
//! preserved across segment boundaries.
//! Returns an iterator past the last generated element.
template <class OutIt, class Size, class Generator>
BOOST_CONTAINER_FORCEINLINE
OutIt segmented_generate_n(OutIt first, Size count, Generator gen)
{
   typedef segmented_iterator_traits<OutIt> traits;
   return detail_algo::segmented_generate_n_ref(first, count, gen,
      typename traits::is_segmented_iterator());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_GENERATE_N_HPP
