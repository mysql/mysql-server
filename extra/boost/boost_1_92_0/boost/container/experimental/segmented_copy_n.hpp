//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_COPY_N_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_COPY_N_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_iterator_traits.hpp>
#include <boost/container/detail/std_fwd.hpp>
#include <boost/container/detail/iterator.hpp>

namespace boost {
namespace container {

template <class InIter, class Size, class OutIter>
OutIter segmented_copy_n(InIter first, Size count, OutIter result);

namespace detail_algo {

//////////////////////////////////////////////////////////////////////////////
// Bounded destination helper: copies from source count elements into
// [dst_first, dst_last), stopping when count reaches zero or destination
// is full.  Returns segduo<SrcIter, DstIter> with the final positions of
// both iterators.  count is advanced by reference so the caller knows
// how many elements remain.
// Recursively walks destination segments when dst is segmented.
//
// When dst_last is unreachable_sentinel_t the destination-full check
// is optimised away, giving the same code as an unbounded loop.
//////////////////////////////////////////////////////////////////////////////

template <class SrcIter, class SrcSent, class Size, class DstIter, class DstSent>
BOOST_CONTAINER_FORCEINLINE segduo<SrcIter, DstIter>
segmented_copy_n_src_dst_bounded
   (SrcIter first, SrcSent last, Size& BOOST_RESTRICT count, DstIter dst_first, DstSent dst_last)
{
   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; count > 0; ++first, --count) {
      if(dst_first == dst_last || first == last)
         goto out_path;
      *dst_first = *first;
      ++dst_first;
   }
   out_path:
   return segduo<SrcIter, DstIter>(first, dst_first);
}

template <class SrcIter, class SrcSent, class Size, class DstIter, class DstSent>
BOOST_CONTAINER_FORCEINLINE
   typename iterator_disable_if_tag
      <DstIter, std::random_access_iterator_tag, segduo<SrcIter, DstIter> >::type
segmented_copy_n_dst_bounded
   (SrcIter first, SrcSent last, Size& BOOST_RESTRICT count, DstIter dst_first, DstSent dst_last, const non_segmented_iterator_tag &)
{
   return (segmented_copy_n_src_dst_bounded)(first, last, count, dst_first, dst_last);
}

template <class RASrcIter, class SrcSent, class Size, class RADstIter>
BOOST_CONTAINER_FORCEINLINE
typename iterator_enable_if_tag
   <RADstIter, std::random_access_iterator_tag, segduo<RASrcIter, RADstIter> >::type
segmented_copy_n_dst_bounded
   (RASrcIter first, SrcSent last, Size& BOOST_RESTRICT count, RADstIter dst_first, RADstIter dst_last, const non_segmented_iterator_tag &)
{
   typedef typename iterator_traits<RADstIter>::difference_type difference_type;
   const difference_type dst_n = dst_last - dst_first;
   Size n = count < Size(dst_n) ? count : Size(dst_n);
   const Size n_initial = n;
   //n is passed by reference and decremented per element copied; the
   //inner helper may exit early if first == last (bounded src), so
   //we must adjust count by the actual number of elements written.
   const segduo<RASrcIter, RADstIter> r = (segmented_copy_n_src_dst_bounded)
      (first, last, n, dst_first, unreachable_sentinel_t());
   count -= (n_initial - n);
   return r;
}

template <class SrcIter, class SrcSent, class Size, class SegDstIter>
segduo<SrcIter, SegDstIter> segmented_copy_n_dst_bounded
   (SrcIter first, SrcSent last, Size& BOOST_RESTRICT count, SegDstIter dst_first, SegDstIter dst_last, const segmented_iterator_tag &)
{
   typedef segmented_iterator_traits<SegDstIter>  dst_traits;
   typedef typename dst_traits::local_iterator    dst_local_iterator;
   typedef typename dst_traits::segment_iterator  dst_segment_iterator;
   typedef typename segmented_iterator_traits<dst_local_iterator>::is_segmented_iterator dst_is_local_seg_t;

   dst_segment_iterator       sdfirst = dst_traits::segment(dst_first);
   const dst_segment_iterator sdlast  = dst_traits::segment(dst_last);

   if(sdfirst == sdlast) {
      segduo<SrcIter, dst_local_iterator> r = (segmented_copy_n_dst_bounded)
         (first, last, count, dst_traits::local(dst_first), dst_traits::local(dst_last), dst_is_local_seg_t());
      return segduo<SrcIter, SegDstIter>(r.first, dst_traits::compose(sdfirst, r.second));
   }
   else {
      segduo<SrcIter, dst_local_iterator> r = (segmented_copy_n_dst_bounded)
         (first, last, count, dst_traits::local(dst_first), dst_traits::end(sdfirst), dst_is_local_seg_t());
      first = r.first;
      if (count != 0 && first != last) {
         for (++sdfirst; sdfirst != sdlast; ++sdfirst) {
            r = (segmented_copy_n_dst_bounded)
               (first, last, count, dst_traits::begin(sdfirst), dst_traits::end(sdfirst), dst_is_local_seg_t());
            first = r.first;
            if (count == 0 || first == last)
               return segduo<SrcIter, SegDstIter>(first, dst_traits::compose(sdfirst, r.second));
         }

         r = (segmented_copy_n_dst_bounded)
            (first, last, count, dst_traits::begin(sdlast), dst_traits::local(dst_last), dst_is_local_seg_t());
      }
      return segduo<SrcIter, SegDstIter>(r.first, dst_traits::compose(sdfirst, r.second));
   }
}

//////////////////////////////////////////////////////////////////////////////
// Destination dispatch: routes to bounded helper.
// Non-segmented destination: single unbounded call (unreachable_sentinel_t).
// Segmented destination: loop over destination segments, bounded per segment.
//////////////////////////////////////////////////////////////////////////////

template <class SrcIter, class SrcSent, class Size, class DstIter>
BOOST_CONTAINER_FORCEINLINE DstIter segmented_copy_n_dst_dispatch
   (SrcIter first, SrcSent last, Size& BOOST_RESTRICT count, DstIter result, const non_segmented_iterator_tag &)
{
   return (segmented_copy_n_src_dst_bounded)
      (first, last, count, result, unreachable_sentinel_t()).second;
}

template <class SrcIter, class SrcSent, class Size, class SegDstIter>
SegDstIter segmented_copy_n_dst_dispatch
   (SrcIter first, SrcSent last, Size& BOOST_RESTRICT count, SegDstIter result, const segmented_iterator_tag &)
{
   typedef segmented_iterator_traits<SegDstIter>  dst_traits;
   typedef typename dst_traits::local_iterator    dst_local_iterator;
   typedef typename dst_traits::segment_iterator  dst_segment_iterator;
   typedef typename segmented_iterator_traits<dst_local_iterator>::is_segmented_iterator dst_is_local_seg_t;

   if(count <= 0)
      return result;

   dst_segment_iterator dst_seg   = dst_traits::segment(result);
   dst_local_iterator   dst_local = dst_traits::local(result);

   while(1) {
      const segduo<SrcIter, dst_local_iterator> r = (segmented_copy_n_dst_bounded)
         (first, last, count, dst_local, dst_traits::end(dst_seg), dst_is_local_seg_t());
      first = r.first;
      //Exit when count is satisfied or the source range is exhausted.
      //The src-exhaustion check matters when this routine is invoked with
      //a per-segment src range (count typically larger than the range);
      //it returns control to the outer source-segment loop.
      if(count == 0 || first == last)
         return dst_traits::compose(dst_seg, r.second);
      ++dst_seg;
      dst_local = dst_traits::begin(dst_seg);
   }
}

//////////////////////////////////////////////////////////////////////////////
// copy_n_scan: scans through source segments, clipping to both segment
// boundaries and count, then delegates to the destination dispatch layer.
//////////////////////////////////////////////////////////////////////////////

template <class InIter, class Size, class OutIter>
BOOST_CONTAINER_FORCEINLINE
OutIter copy_n_scan
   ( InIter first, InIter last, Size& BOOST_RESTRICT count
   , OutIter result, non_segmented_iterator_tag, const std::random_access_iterator_tag &)
{
   const Size range_sz = Size(last - first);
   Size min_count = count <= range_sz ? count : range_sz;
   count -= min_count;
   #if !defined(BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO)
      typedef segmented_iterator_traits<OutIter> dst_traits;
      return (segmented_copy_n_dst_dispatch)
         (first, unreachable_sentinel_t(), min_count, result, typename dst_traits::is_segmented_iterator());
   #else
      return (segmented_copy_n_src_dst_bounded)
         (first, unreachable_sentinel_t(), min_count, result, unreachable_sentinel_t()).second;
   #endif
}

template <class InIter, class Size, class OutIter, class Cat>
BOOST_CONTAINER_FORCEINLINE
OutIter copy_n_scan
   (InIter first, InIter last, Size& BOOST_RESTRICT count, OutIter result, non_segmented_iterator_tag, const Cat &)
{
   #if !defined(BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO)
      typedef segmented_iterator_traits<OutIter> dst_traits;
      return (segmented_copy_n_dst_dispatch)
         (first, last, count, result, typename dst_traits::is_segmented_iterator());
   #else
      return (segmented_copy_n_dst_dispatch)
         (first, last, count, result, non_segmented_iterator_tag());
   #endif
}

template <class SegIt, class Size, class OutIter, class Cat>
OutIter copy_n_scan(SegIt first, SegIt last, Size& BOOST_RESTRICT count, OutIter result, segmented_iterator_tag, const Cat &)
{
   typedef segmented_iterator_traits<SegIt>                                            traits;
   typedef typename traits::local_iterator                                             local_iterator;
   typedef typename traits::segment_iterator                                           segment_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator   is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category                 local_cat_t;

   segment_iterator scur  = traits::segment(first);
   segment_iterator slast = traits::segment(last);
   local_iterator   lcur  = traits::local(first);

   if(scur == slast) {
      return copy_n_scan(lcur, traits::local(last), count, result, is_local_seg_t(), local_cat_t());
   }
   else {
      result = copy_n_scan(lcur, traits::end(scur), count, result, is_local_seg_t(), local_cat_t());

      if (!count)
         return result;

      for (++scur; scur != slast; ++scur) {
         result = copy_n_scan(traits::begin(scur), traits::end(scur), count, result, is_local_seg_t(), local_cat_t());
         if (!count)
            return result;
      }

      return copy_n_scan(traits::begin(scur), traits::local(last), count, result, is_local_seg_t(), local_cat_t());
   }
}

//////////////////////////////////////////////////////////////////////////////
// Source dispatch: walks the source (read pointer) segments
//////////////////////////////////////////////////////////////////////////////

template <class SegIter, class Size, class OutIter, class Cat>
OutIter segmented_copy_n_dispatch
   (SegIter first, Size count, OutIter result, const segmented_iterator_tag &, Cat)
{
   typedef segmented_iterator_traits<SegIter> traits;
   typedef typename traits::local_iterator   local_iterator;
   typedef typename traits::segment_iterator segment_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category local_cat_t;

   if(count <= 0) return result;

   segment_iterator scur = traits::segment(first);
   local_iterator   lcur = traits::local(first);

   while(1) {
      result = copy_n_scan(lcur, traits::end(scur), count, result, is_local_seg_t(), local_cat_t());

      if(count == 0)
         break;
      ++scur;
      lcur = traits::begin(scur);
   }
   return result;
}

template <class InIter, class Size, class OutIter, class Cat>
BOOST_CONTAINER_FORCEINLINE OutIter segmented_copy_n_dispatch
   (InIter first, Size count, OutIter result, const non_segmented_iterator_tag &, const Cat&)
{
#if !defined(BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO)
   typedef segmented_iterator_traits<OutIter> dst_traits;
   return (segmented_copy_n_dst_dispatch)
      (first, unreachable_sentinel_t(), count, result, typename dst_traits::is_segmented_iterator());
#else
   return (segmented_copy_n_src_dst_bounded)
      (first, unreachable_sentinel_t(), count, result, unreachable_sentinel_t()).second;
#endif
}

} // namespace detail_algo

//! Copies \c count elements from the range beginning at \c first to
//! the range beginning at \c result. Exploits segmentation on both
//! input and output.
template <class InIter, class Size, class OutIter>
BOOST_CONTAINER_FORCEINLINE
OutIter segmented_copy_n(InIter first, Size count, OutIter result)
{
   typedef segmented_iterator_traits<InIter> traits;
   return detail_algo::segmented_copy_n_dispatch(first, count, result,
      typename traits::is_segmented_iterator(), typename iterator_traits<InIter>::iterator_category());
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_COPY_N_HPP
