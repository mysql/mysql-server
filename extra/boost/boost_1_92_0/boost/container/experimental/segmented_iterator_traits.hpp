//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_ITERATOR_TRAITS_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_ITERATOR_TRAITS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/iterator.hpp>

#include <boost/move/utility_core.hpp>

namespace boost {
namespace container {

//! Tag type indicating a segmented iterator.
struct segmented_iterator_tag
{
   static const bool value = true;
};

//! Tag type indicating a non-segmented (flat) iterator.
struct non_segmented_iterator_tag
{
   static const bool value = false;
};

//! Sentinel whose comparison with any iterator always yields false (not equal).
//! Passes through segmented_remove_if_result_bounded to express an unbounded
//! destination, letting the compiler eliminate the dead destination-full branch.
template<class T1, class T2>
struct segduo
{
   T1 first;
   T2 second;

   BOOST_CONTAINER_FORCEINLINE segduo() {}

   template<class U1, class U2>
   BOOST_CONTAINER_FORCEINLINE segduo(const U1 &f, const U2 &s) : first(f), second(s) {}

   template<class U1, class U2>
   BOOST_CONTAINER_FORCEINLINE segduo(const segduo<U1, U2> &sd) : first(sd.first), second(sd.second) {}
};

template<class T1, class T2, class T3>
struct segtrio
{
   T1 first;
   T2 second;
   T3 third;

   BOOST_CONTAINER_FORCEINLINE segtrio() {}

   template<class U1, class U2, class U3>
   BOOST_CONTAINER_FORCEINLINE segtrio(const U1 &f, const U2 &s, const U3 &t)
      : first(f), second(s), third(t) {}

   template<class U1, class U2, class U3>
   BOOST_CONTAINER_FORCEINLINE segtrio(const segtrio<U1, U2, U3> &sd)
      : first(sd.first), second(sd.second), third(sd.third) {}
};

struct unreachable_sentinel_t
{
   template <class It>
   BOOST_CONTAINER_FORCEINLINE friend bool operator==(const It&, unreachable_sentinel_t) { return false; }
   
   template <class It>
   BOOST_CONTAINER_FORCEINLINE friend bool operator==(unreachable_sentinel_t, const It&) { return false; }
   
   template <class It>
   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(const It&, unreachable_sentinel_t) { return true; }
   
   template <class It>
   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(unreachable_sentinel_t, const It&) { return true; }
};

//! Disambiguator tag passed by the dual-RA fast-path overload of certain
//! bounded helpers (e.g. segmented_copy_if_dst_bounded,
//! segmented_remove_copy_if_dst_bounded) when it has decided that the
//! remaining source does not fit the destination capacity and a real
//! bounded scan is required.  The tag's only role is to perturb the
//! signature of the recursive call so that the dual-RA overload itself
//! cannot re-match: only the unrolled / generic terminal overloads remain
//! viable, leaving the random-access category of both iterators intact.
struct dual_ra_skip_t {};

namespace detail_algo {

//! Default less-than function object used by segmented algorithms when the
//! user does not provide an explicit comparator.  Centralised here to avoid
//! duplicating the same struct across every algorithm header.
struct segmented_default_less
{
   template <class T>
   BOOST_CONTAINER_FORCEINLINE
   bool operator()(const T& a, const T& b) const { return a < b; }
};

//! Proxy-safe "equal to *it" predicate.  Holds the iterator by value and
//! re-dereferences on every invocation, so a prvalue proxy returned by
//! operator*() only lives for the full expression of the comparison and
//! never outlives it.  This avoids the lifetime hazard of caching *it in
//! a const-reference member (as equal_to_value would).
template <class Iter>
struct equal_to_deref
{
   Iter it_;

   BOOST_CONTAINER_FORCEINLINE explicit equal_to_deref(Iter it)
      : it_(it)
   {}

   template <class U>
   BOOST_CONTAINER_FORCEINLINE bool operator()(const U &u) const
   {  return u == *it_; }
};

template <bool B> struct void_if_true;
template <> struct void_if_true<true> { typedef void type; };

template <class T> struct make_void { typedef void type; };

template <bool, class T = void> struct algo_enable_if_c {};
template <class T> struct algo_enable_if_c<true, T> { typedef T type; };

template <class T, class Enable = void>
struct has_iterator_category { static const bool value = false; };

template <class T>
struct has_iterator_category<T, typename make_void<typename T::iterator_category>::type>
{ static const bool value = true; };

template <class T>
struct constref_generator
{
    const T& value;
    BOOST_CONTAINER_FORCEINLINE explicit constref_generator(const T& v)
       : value(v)
    {}

    BOOST_CONTAINER_FORCEINLINE const T& operator()() const { return value; }
};

//////////////////////////////////////////////////////////////////////////////
// Transfer helper: copy (Move=false) or move (Move=true) a single element.
//////////////////////////////////////////////////////////////////////////////

template <bool> struct transfer_op;
template <> struct transfer_op<false>
{
   template <class D, class S>
   BOOST_CONTAINER_FORCEINLINE static void apply(D& d, S& s) { d = s; }
};
template <> struct transfer_op<true>
{
   template <class D, class S>
   BOOST_CONTAINER_FORCEINLINE static void apply(D& d, S& s) { d = boost::move(s); }
};

} // namespace detail_algo

//! Traits class to detect and decompose segmented iterators.
//!
//! The default definition marks all iterators as non-segmented.
//! Specializations for segmented iterator types must provide:
//!
//!   typedef segmented_iterator_tag         is_segmented_iterator;
//!   typedef <implementation-defined>       segment_iterator;
//!   typedef <implementation-defined>       local_iterator;
//!
//!   static segment_iterator segment(Iterator it);
//!   static local_iterator   local(Iterator it);
//!   static Iterator         compose(segment_iterator s, local_iterator l);
//!   static local_iterator   begin(segment_iterator s);
//!   static local_iterator   end(segment_iterator s);
//!
//! An explicit specialization is not required when the iterator type
//! provides the intrusive interface:
//!
//!   typedef segmented_iterator_tag         is_segmented_iterator;
//!   typedef <implementation-defined>       segment_iterator;
//!   typedef <implementation-defined>       local_iterator;
//!   segment_iterator segment() const;
//!   local_iterator   local()   const;
//!   Iterator(segment_iterator s, local_iterator l);  // composing constructor
//!
//! and segment_iterator provides begin()/end() returning local_iterator.
//!
//! Based on: M. Austern, "Segmented Iterators and Hierarchical Algorithms"
template <class Iterator>
struct segmented_iterator_traits
{
   typedef non_segmented_iterator_tag is_segmented_iterator;
};
 
//! Detects whether \c Sent is a true sentinel for \c Iter.
//! A sentinel is a type that is not the same as the iterator and
//! does not model an iterator (lacks \c iterator_category).
//! When \c is_sentinel is false, algorithms use the \c (Iter, Iter)
//! overload, avoiding extra template instantiations from mixed
//! iterator types such as \c iterator / \c const_iterator.
template <class Sent, class Iter>
struct is_sentinel
{
   static const bool value = !detail_algo::has_iterator_category<Sent>::value;
};

template <class Iter>
struct is_sentinel<Iter, Iter>
{
   static const bool value = false;
};

namespace detail_algo {

//////////////////////////////////////////////////////////////////////////////
// sent_filter: when [first, last) is closed by a true sentinel
// (Sent != Iter), segment walkers cannot decompose Sent and bidirectional
// algorithms cannot step backwards from it; in that case both the iterator
// category and the segmentation tag are downgraded to
// (std::forward_iterator_tag, non_segmented_iterator_tag) so that algorithms
// route through the flat, forward-only path.  When Sent == Iter, the
// iterator's natural category and segmentation are preserved.
//
// Used by bidirectional/segmented algorithms (e.g. segmented_partition,
// segmented_find_last_if) that need a single dispatch point covering both
// the (Iter, Iter) and (Iter, Sentinel) call signatures.
//////////////////////////////////////////////////////////////////////////////

template <class Iter, class Sent, bool = is_sentinel<Sent, Iter>::value>
struct sent_filter
{
   typedef std::forward_iterator_tag                          cat_t;
   typedef non_segmented_iterator_tag                         seg_t;
};

template <class Iter>
struct sent_filter<Iter, Iter, false>
{
   typedef typename boost::container::iterator_traits<Iter>
      ::iterator_category                                     cat_t;
   typedef typename segmented_iterator_traits<Iter>
      ::is_segmented_iterator                                 seg_t;
};

template <class Iter, class Sent>
struct sent_filter<Iter, Sent, false>
   : public sent_filter<Iter, Iter, false>
{};

// Compile-time metafunction that unwraps a (possibly recursively) segmented
// iterator to its deepest non-segmented `local_iterator` type.
//
//   deepest_local_iterator<It>::type == It                         (non-segmented)
//   deepest_local_iterator<It>::type ==
//      deepest_local_iterator<segmented_iterator_traits<It>::local_iterator>::type
//                                                                   (segmented)
//
// Used by segmented algorithms that need to hold cross-segment state of
// iterator type rather than element type (e.g. a "previous element" pointer
// that survives the boundary between sub-ranges), by reducing that state to a
// single unified type across all levels of recursion.
template <bool IsSeg, class It>
struct deepest_local_iterator_impl;

template <class It>
struct deepest_local_iterator_impl<false, It>
{
   typedef It type;
   BOOST_CONTAINER_FORCEINLINE static type get(It it) { return it; }
};

template <class It>
struct deepest_local_iterator_impl<true, It>
{
   private:

   typedef typename segmented_iterator_traits<It>::local_iterator local_t;
   typedef deepest_local_iterator_impl
      < segmented_iterator_traits<local_t>::is_segmented_iterator::value
      , local_t> next_layer_t;

   public:

   typedef typename next_layer_t::type type;

   BOOST_CONTAINER_FORCEINLINE static typename next_layer_t::type get(It it)
   {  return next_layer_t::get(segmented_iterator_traits<It>::local(it)); }
};

template <class It>
struct deepest_local_iterator
{
   typedef deepest_local_iterator_impl
      < segmented_iterator_traits<It>::is_segmented_iterator::value
      , It> next_layer_t;

   public:

   typedef typename next_layer_t::type type;

   BOOST_CONTAINER_FORCEINLINE static typename next_layer_t::type get(It it)
   {  return next_layer_t::get(it);  }
};

} // namespace detail_algo

} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

//////////////////////////////////////////////////////////////////////////////
// BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO
//
// When defined, segmented algorithms that write to an output iterator will
// NOT exploit segmentation on the output side.  Only input (source) iterator
// segmentation is used.  This is useful for benchmarking the advantage of
// the segmented output optimisation in isolation.
//
// Define BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO before including any
// segmented algorithm header to disable the optimisation.
//////////////////////////////////////////////////////////////////////////////

//#define BOOST_CONTAINER_DISABLE_MULTI_SEGMENTED_ALGO

// When defined, segmented algorithms that count elements (e.g. segmented_count,
// segmented_count_if) will use a branchless counting strategy in their
// innermost loops, incrementing the count by the boolean result of the
// predicate rather than testing the predicate result in a branch and conditionally
// incrementing.  This can improve performance on some platforms by avoiding branch mispredictions,
// at the cost of potentially increased instruction count and/or reduced vectorization opportunities.
// 
// However, the optimal strategy is highly platform- and algorithm-specific, so this is disabled by default.
// There are regressions on some algorithms and platforms when this is enabled, and the performance impact
// varies widely across different scenarios.
//
//#define BOOST_CONTAINER_SEGMENTED_COUNT_BRANCHLESS

//#define BOOST_CONTAINER_SEGMENTED_DISABLE_PRAGMA_UNROLL
//#define BOOST_CONTAINER_SEGMENTED_ENABLE_PRAGMA_UNROLL

#if defined(BOOST_CONTAINER_SEGMENTED_DISABLE_PRAGMA_UNROLL) && defined (BOOST_CONTAINER_SEGMENTED_ENABLE_PRAGMA_UNROLL)
#error "Cannot define both BOOST_CONTAINER_SEGMENTED_DISABLE_PRAGMA_UNROLL and BOOST_CONTAINER_SEGMENTED_ENABLE_PRAGMA_UNROLL"
#endif

#if !defined(BOOST_CONTAINER_SEGMENTED_DISABLE_PRAGMA_UNROLL) && !defined (BOOST_CONTAINER_SEGMENTED_ENABLE_PRAGMA_UNROLL)
   #if defined(BOOST_CLANG)
      //Let clang decide when to unroll loops, as it can auto-vectorize more aggressively when unrolling is disabled.
      #define BOOST_CONTAINER_SEGMENTED_DISABLE_PRAGMA_UNROLL
   #else
      #define BOOST_CONTAINER_SEGMENTED_ENABLE_PRAGMA_UNROLL
   #endif
#endif

#if defined(BOOST_CONTAINER_SEGMENTED_ENABLE_PRAGMA_UNROLL)
   #define BOOST_CONTAINER_SEGMENTED_UNROLL(N) BOOST_CONTAINER_UNROLL(N)
   #define BOOST_CONTAINER_SEGMENTED_AUTO_UNROLL BOOST_CONTAINER_AUTO_UNROLL
#elif defined(BOOST_CONTAINER_SEGMENTED_DISABLE_PRAGMA_UNROLL)
   #define BOOST_CONTAINER_SEGMENTED_UNROLL(N)
   #define BOOST_CONTAINER_SEGMENTED_AUTO_UNROLL
#else
#error "Must define either BOOST_CONTAINER_SEGMENTED_ENABLE_PRAGMA_UNROLL or BOOST_CONTAINER_SEGMENTED_DISABLE_PRAGMA_UNROLL"
#endif

#endif // BOOST_CONTAINER_EXPERIMENTAL_SEGMENTED_ITERATOR_TRAITS_HPP
