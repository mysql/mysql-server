//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_SEARCH_N_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_SEARCH_N_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/experimental/segmented_iterator_traits.hpp>
#include <boost/container/experimental/segmented_find.hpp>

namespace boost {
namespace container {

template <class FwdIt, class Sent, class Size, class T>
FwdIt segmented_search_n
   (FwdIt first, Sent last, Size count, const T& BOOST_RESTRICT value);

namespace detail_algo {

// search_n_scan_segment scans a single (sub-)segment for a run of "count"
// consecutive elements equal to "value", threading cross-segment state
// through the "consecutive" parameter. The result is a segtrio:
//
//   first  : true if the full run was completed inside this scan
//   second : current consecutive count at end of scan (carries to next segment)
//   third  : start of the run when first==true; otherwise the position where
//            a new run started inside this scan, or "lend" (sentinel) when
//            no new run started in this scan (i.e. the in-progress run, if
//            any, started in a previous segment).
//
// Used both as the recursive worker driving segmented walks and as the
// single entry point invoked by segmented_search_n.

// (non-segmented, random-access, same-typed iterators)
// Boyer-Moore-style "skip-by-count" scan with cross-segment carry-over.
//
// Phase 1: if "consecutive" carries a partial run from the previous
//   segment, probe at "lcur + ncheck - 1" (the last position needed to
//   complete the carry-over, clamped by the segment) and verify
//   backward on a hit. On success return; on a mismatch reset
//   "consecutive" to 0 and fall through to phase 2 from the position
//   past the mismatch -- with "consecutive > 0", no in-segment run can
//   start at or before the probe, so the entire range up to the probe
//   can be skipped without inspection on a probe miss.
// Phase 2: probe at "lcur + (count - 1)". On match, verify backward to
//   find the run start; on mismatch advance the probe by "count"
//   positions instead of 1 (Boyer-Moore). The mismatched element cannot
//   belong to any complete run that fits in [probe - count + 1, probe],
//   so the next candidate run can only start past it. This makes the
//   scan O(n / count) on the value-sparse portion of the range.
// Phase 3: once "probe >= lend", scan the tail to detect a partial
//   trailing run that the next segment may extend. Bounded by phase 2's
//   exit position: "*(probe - count) != value" is known, so the scan
//   range is (probe - count, lend) of length "lend - probe + count - 1",
//   which is in [0, count - 1] -- often well below count - 1, and
//   exactly 0 when the last probe landed at "lend - 1" and missed.
template <class LocalIter, class Size, class T>
segtrio<bool, Size, LocalIter> search_n_scan_segment
   (LocalIter lcur, LocalIter const lend,
    Size consecutive, Size count, const T& BOOST_RESTRICT value,
    const non_segmented_iterator_tag &, const std::random_access_iterator_tag &)
{
   typedef segtrio<bool, Size, LocalIter> result_t;
   typedef typename iterator_traits<LocalIter>::difference_type difference_type;

   // Phase 1: extend a partial run (consecutive > 0) carried from the previous segment.
   //
   // Single-cursor verify mirroring phase 2's V3 shape: `lcur` is stepped
   // to "verify_probe = original_lcur + ncheck - 1" (the last position the
   // carry-over needs to cover) and the inner loop sweeps the ncheck
   // positions [original_lcur, verify_probe] backward via "--lcur".
   //
   // OOB safety: exactly ncheck - 1 decrements (not ncheck) are executed
   // on the full-match path, exiting via "if (--left == 0)" before the
   // final --lcur. This avoids forming "original_lcur - 1" -- UB for raw
   // pointers, which the segmented deque path always supplies as
   // chunk-start pointers.
   if(consecutive > 0) {
      // Handles the case where the segment too short to complete the run.
      const difference_type need   = static_cast<difference_type>(count - consecutive);
      const difference_type avail  = lend - lcur;
      const difference_type ncheck = need < avail ? need : avail;
      if(BOOST_UNLIKELY(ncheck == 0))   // Empty segment; carry-over preserved for the next one.
         return result_t(false, consecutive, lend);

      // Step "lcur" itself to the verify probe position; no separate cursor.
      lcur += (ncheck - 1);
      Size left = static_cast<Size>(ncheck);
      BOOST_CONTAINER_UNROLL(4)
      do {
         if(!(*lcur == value))
            goto phase1_mismatch; // Mismatch, jump to cleanup logic
         if (--left == 0) {
            goto full_match;
         }
         --lcur;
      } while(true);

      full_match:
      // Full backwards verification: run start = original_lcur.
      // Put "start of the run" return to sentinel so the outer
      // match_start is not updated.
      consecutive += static_cast<Size>(ncheck);
      return result_t(consecutive == count, consecutive, lend);

      // Mismatch, reset consecutive counter and step past the failing position.
      phase1_mismatch:
      consecutive = 0;
      ++lcur;
   }

   const difference_type dcount    = static_cast<difference_type>(count);
   const difference_type remaining = lend - lcur;

   // Phase 2: Only meaningful if a full run fits (dcount <= remaining)
   //          No partial run. Skip-by-count probing.
   //
   // Single-cursor Boyer-Moore: at the top of each outer iteration `lcur`
   // sits at the verify probe position. The inner verify checks *lcur
   // (its first iteration replaces an outer "if(*probe == value)" gate)
   // then walks down via "--lcur", sweeping the count positions
   // [verify_probe - (count - 1), verify_probe].
   //
   // After an inner mismatch at iter K (K in [1, count]) `lcur` lands at
   // LKNM = verify_probe - (K - 1); the next outer iter's verify_probe is
   // verify_probe + (count - K + 1), so the advance from LKNM is
   // (count - K + 1) + (K - 1) = count -- a constant `dcount`, regardless
   // of K. The BM acceleration is entirely encoded in how far the inner
   // loop decrements `lcur` before mismatching, and the outer step is just
   // dcount.
   //
   // OOB safety: `lend_safe = lend - dcount` is precomputed once (well-formed
   // because dcount <= remaining = lend - original_lcur). The guard
   // `lcur >= lend_safe` is a pointer comparison only -- no new pointer is
   // ever formed past `lend`. When `lcur < lend_safe`, `lcur + dcount`
   // lands at most at `lend - 1`, which is dereferenceable.
   if(dcount <= remaining) {
      // First verify probe: original_lcur + dcount - 1, in [original_lcur, lend).
      lcur += (dcount - 1);
      const LocalIter lend_safe = lend - dcount;  // largest lcur that can advance
      while (true) {
         // Inner verify sweeps the count positions [verify_probe - (count - 1),
         // verify_probe] backward. Doing exactly count - 1 decrements (not
         // count) avoids forming "original_lcur - 1" when the run lands
         // right at the start of the segment.
         Size left = count;
         BOOST_CONTAINER_UNROLL(4)
         do {  //count is always >= 2 because it's tested in the outer function
            if(!(*lcur == value))
               goto phase2_mismatch;
            if (--left == 0)
               // Run start: lcur = verify_probe - (count - 1).
               return result_t(true, count, lcur);
            --lcur;
         } while (true);

         phase2_mismatch: ;
         // lcur at LKNM (failed check). OOB-safe advance to next verify probe.
         if (lcur >= lend_safe)
            break;
         lcur += dcount;
      }
      // Exit: lcur at LKNM. The phase-3 tail scan starts one past it.
      ++lcur;
   }

   // Phase 3: partial trailing run that could extend into the next segment.
   //
   // Scans [lcur, lend) backward. Mirrors phase 2's verify-loop pattern
   {
      LocalIter tail = lend;
      if (tail == lcur)            // empty tail, nothing to scan.
         return result_t(false, 0, lend);

      BOOST_CONTAINER_UNROLL(4)
      do {
         --tail;
         if (!(*tail == value))
            goto tail_mismatch;
      } while (tail != lcur);

      // Full tail run: every element in [lcur, lend) equals value.
      // tail == lcur and (lend - lcur) > 0, so the run is non-empty.
      return result_t(false, static_cast<Size>(lend - tail), tail);

      // Mismatch at "tail"; the partial run starts one past it. On a
      // first-iteration mismatch ("*(lend - 1) != value") the post-"++tail"
      // value collapses to "lend", which the caller already interprets as
      // "no match position" via "r.third != lend", so the assignment is
      // safe to do unconditionally.
      tail_mismatch:
      LocalIter match_start = tail;
      ++match_start;
      return result_t(false, static_cast<Size>(lend - tail), match_start);
   }
}

// Initial value for "match_start" used by the forward scan below.
//
// When invoked with same-typed iterators (the recursive segmented caller's
// path) we return "lend" so the caller can detect "no new run started in
// this scan" via "r.third != lend". With a real sentinel this trick isn't
// available (Sent cannot be assigned to LocalIter), but it isn't needed
// either: that path is reached only at the top level, where the caller only
// consults "r.third" when "r.first == true", in which case "match_start"
// has been set to a real position inside the loop.
template <class LocalIter>
BOOST_CONTAINER_FORCEINLINE
LocalIter search_n_init_match_start(LocalIter, LocalIter lend) { return lend; }

template <class LocalIter, class Sent>
BOOST_CONTAINER_FORCEINLINE
typename algo_enable_if_c<is_sentinel<Sent, LocalIter>::value, LocalIter>::type
search_n_init_match_start(LocalIter lcur, Sent) { return lcur; }

// Forward scan: handles both the same-typed (recursive segmented caller) and
// the real-sentinel (top-level) cases with one body. The only behavioral
// difference -- the initial value of "match_start" -- is folded into the
// search_n_init_match_start helper above.
template <class LocalIter, class Sent, class Size, class T, class Tag, class Cat>
BOOST_CONTAINER_FORCEINLINE
typename algo_enable_if_c<!Tag::value || is_sentinel<Sent, LocalIter>::value,
                          segtrio<bool, Size, LocalIter> >::type
search_n_scan_segment
   (LocalIter lcur, Sent lend,
    Size consecutive, Size count, const T& BOOST_RESTRICT value, Tag, Cat)
{
   typedef segtrio<bool, Size, LocalIter> result_t;

   LocalIter match_start = search_n_init_match_start(lcur, lend);

   BOOST_CONTAINER_SEGMENTED_UNROLL(4)
   for(; lcur != lend; ++lcur) {
      if(*lcur == value) {
         if(consecutive == 0)
            match_start = lcur;
         ++consecutive;
         if(consecutive == count)
            return result_t(true, consecutive, match_start);
      }
      else {
         consecutive = 0;
      }
   }
   return result_t(false, consecutive, match_start);
}

// (segmented, same-typed iterators)
// Walks sub-segments and recursively delegates to search_n_scan_segment so
// that every level of segmentation is exploited and the innermost level
// gets the RA fast path.  Cross-segment state (consecutive count and
// match_start) is threaded through the segtrio result.
template <class SegIter, class Size, class T, class Cat>
segtrio<bool, Size, SegIter> search_n_scan_segment
   (SegIter first, SegIter last,
    Size consecutive, Size count, const T& BOOST_RESTRICT value,
    segmented_iterator_tag, Cat)
{
   typedef segmented_iterator_traits<SegIter>     traits;
   typedef typename traits::segment_iterator      segment_iterator;
   typedef typename traits::local_iterator        local_iterator;
   typedef typename segmented_iterator_traits<local_iterator>::is_segmented_iterator is_local_seg_t;
   typedef typename iterator_traits<local_iterator>::iterator_category local_cat_t;

   typedef segtrio<bool, Size, local_iterator>  sub_result_t;
   typedef segtrio<bool, Size, SegIter>         result_t;

   SegIter match_start = last;

   segment_iterator scur  = traits::segment(first);
   segment_iterator slast = traits::segment(last);

   if(scur == slast) {
      local_iterator lend = traits::local(last);
      const sub_result_t r = search_n_scan_segment(traits::local(first), lend,
                               consecutive, count, value, is_local_seg_t(), local_cat_t());
      if(r.third != lend)
         match_start = traits::compose(scur, r.third);
      return result_t(r.first, r.second, match_start);
   }
   else {
      local_iterator lend = traits::end(scur);
      sub_result_t r = search_n_scan_segment(traits::local(first), lend,
                               consecutive, count, value, is_local_seg_t(), local_cat_t());
      if(r.third != lend)
         match_start = traits::compose(scur, r.third);
      if(r.first)
         return result_t(true, r.second, match_start);

      for(++scur; scur != slast; ++scur) {
         lend = traits::end(scur);
         r = search_n_scan_segment(traits::begin(scur), lend,
                               r.second, count, value, is_local_seg_t(), local_cat_t());
         if(r.third != lend)
            match_start = traits::compose(scur, r.third);
         if(r.first)
            return result_t(true, r.second, match_start);
      }
      {
         lend = traits::local(last);
         r = search_n_scan_segment(traits::begin(scur), lend,
                               r.second, count, value, is_local_seg_t(), local_cat_t());
         if(r.third != lend)
            match_start = traits::compose(scur, r.third);
         return result_t(r.first, r.second, match_start);
      }
   }
}

// Cheap O(1) size guard available when first/last are a same-typed RA pair.
template <class FwdIt, class Size>
BOOST_CONTAINER_FORCEINLINE
bool search_n_range_shorter_than
   (FwdIt first, FwdIt last, Size count, const std::random_access_iterator_tag &)
{
   typedef typename iterator_traits<FwdIt>::difference_type difference_type;
   return (last - first) < static_cast<difference_type>(count);
}

template <class FwdIt, class Sent, class Size, class Cat>
BOOST_CONTAINER_FORCEINLINE
bool search_n_range_shorter_than
   (FwdIt, Sent, Size, Cat)
{
   return false;
}

// Top-level dispatch for the count >= 2 case.
//
// Generic fallback: delegates to the recursive walker via search_n_scan_segment
// and unwraps the segtrio.
template <class FwdIt, class Sent, class Size, class T, class Tag, class Cat>
BOOST_CONTAINER_FORCEINLINE
FwdIt search_n_top_dispatch
   (FwdIt first, Sent last, Size count, const T& BOOST_RESTRICT value, Tag tag, Cat cat)
{
   const segtrio<bool, Size, FwdIt> r = search_n_scan_segment
      (first, last, (Size)0, count, value, tag, cat);
   if (r.first)
      return r.third;
   return last;
}

// Top-level non-segmented RA fast path. Phase 1 (carry-over from a previous
// segment) is dead because "consecutive == 0" at the top level. Phase 3
// (trailing partial run that the next segment may extend) is dead because
// there is no next segment. Reduces to pure Boyer-Moore skip-by-count
// probing, returning the iterator directly without going through the
// segtrio packaging. Picked over the generic overload by partial ordering
// when the iterator is non-segmented, RA, and same-typed (FwdIt == Sent).
//
// Driven by a running probe iterator advanced in place: for chunked
// iterators (e.g. wrapped deque_iterator) this collapses to a single
// register add inside a chunk, with chunk-boundary work amortized over
// chunk_size/count probes -- vastly cheaper than recomputing
// "first + pidx" (which forces a chunk-base load on the critical path of
// every probe).
template <class FwdIt, class Size, class T>
BOOST_CONTAINER_FORCEINLINE
FwdIt search_n_top_dispatch
   (FwdIt first, const FwdIt last, Size count, const T& BOOST_RESTRICT value,
    const non_segmented_iterator_tag &, const std::random_access_iterator_tag &)
{
   typedef typename iterator_traits<FwdIt>::difference_type difference_type;
   const difference_type dcount = static_cast<difference_type>(count);
   // dcount <= last - first guaranteed by search_n_range_shorter_than.
   //
   // Single-cursor Boyer-Moore: `first` (a by-value parameter, private to
   // this frame) is repurposed as the running probe. After the initial
   // advance to "first + dcount - 1" it no longer means "start of the
   // search range" -- it tracks the verify probe through the rest of the
   // function. `last_safe = last - dcount` is precomputed before the
   // mutation while `first` still names the original lower bound.
   //
   // The inner verify checks *first (its first iteration replaces the old
   // "if (*probe == value)" outer check) then walks down via "--first",
   // sweeping the count positions [verify_probe - (count - 1), verify_probe].
   //
   // After an inner mismatch at iter K (K in [1, count]) `first` lands at
   // LKNM = verify_probe - (K - 1); the next outer iter's verify_probe is
   // verify_probe + (count - K + 1), so the advance from LKNM is
   // (count - K + 1) + (K - 1) = count -- a constant `dcount`, regardless
   // of K. So the BM acceleration is entirely encoded in how far the inner
   // loop decrements `first` before mismatching, and the outer step is just
   // dcount.
   //
   // OOB safety: the guard `first >= last_safe` is a pure pointer
   // comparison -- no new pointer is ever formed past `last`. When
   // `first < last_safe`, `first + dcount` lands at most at `last - 1`,
   // which is dereferenceable.
   const FwdIt last_safe = last - dcount;  // largest probe pos that can advance
   first += (dcount - 1);                  // first becomes the verify probe
   while (true) {
      // Inner verify sweeps the count positions [verify_probe - (count - 1),
      // verify_probe] backward. Doing exactly count - 1 decrements (not
      // count) avoids forming "original_first - 1" when the run lands right
      // at the start of the range -- the OLD `back = probe; Size left =
      // count - 1` shape, but with `first` itself as the cursor.
      Size left = count;
      BOOST_CONTAINER_UNROLL(4)
      do {
         if (!(*first == value))
            goto probe_mismatch;
         if (--left == 0)
            return first;             // run start: first = verify_probe - (count - 1)
         --first;
      } while (true);

      probe_mismatch: ;
      // first at LKNM (failed check). OOB-safe advance to next verify probe:
      // last_safe = last - dcount is precomputed, so the test forms no new
      // pointer. When first < last_safe, first + dcount lands at most at
      // last - 1, which is dereferenceable.
      if (first >= last_safe)
         return last;
      first += dcount;
   }
}

} // namespace detail_algo

//! Finds the first occurrence of \c count consecutive elements equal to \c value
//! in [first, last). Returns an iterator to the start of the run, or \c last if not found.
//!
//! Trivial counts (<= 0 or == 1) are handled directly. Otherwise an O(1) size
//! guard is applied when available, then the work is delegated to
//! search_n_scan_segment with consecutive=0. Partial ordering routes to the
//! right overload by tag (segmented walker, non-seg RA skip-by-count, or
//! forward/sentinel scan), and the segtrio is unwrapped to the public
//! iterator return.
template <class FwdIt, class Sent, class Size, class T>
inline FwdIt segmented_search_n
   (FwdIt first, Sent last, Size count, const T& BOOST_RESTRICT value)
{
   typedef segmented_iterator_traits<FwdIt> traits;
   typedef typename traits::is_segmented_iterator              is_seg_t;
   typedef typename iterator_traits<FwdIt>::iterator_category  cat_t;

   if (BOOST_UNLIKELY(count <= 0))
      return first;
   else if (count == 1) {
      return detail_algo::segmented_find_dispatch(first, last, value, is_seg_t(), cat_t());
   }
   else if (BOOST_UNLIKELY(detail_algo::search_n_range_shorter_than(first, last, count, cat_t())))
      return last;   
   else {
      // count >= 2. Tag-dispatched: non-segmented RA top-level calls take a Phase-2-only
      // fast path (Phase 1 is dead because "consecutive == 0" and Phase 3
      // is dead because there is no next segment). Otherwise (segmented,
      // forward-only, sentinel) the generic overload delegates to
      // search_n_scan_segment + segtrio unwrap.
      return detail_algo::search_n_top_dispatch(first, last, count, value, is_seg_t(), cat_t());
   }
}

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_SEARCH_N_HPP
