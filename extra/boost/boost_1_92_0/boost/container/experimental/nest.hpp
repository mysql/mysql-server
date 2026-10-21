
//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Joaquin M Lopez Munoz 2025-2026.
// (C) Copyright Ion Gaztanaga 2025-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_EXPERIMENTAL_NEST_HPP
#define BOOST_CONTAINER_EXPERIMENTAL_NEST_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

// container
#include <boost/container/container_fwd.hpp>
#include <boost/container/options.hpp>
#include <boost/container/new_allocator.hpp>
#include <boost/container/allocator_traits.hpp>
// container/detail
#include <boost/container/detail/aligned_allocation.hpp> //portable (over)aligned nothrow alloc
#include <boost/container/detail/bit_utilities.hpp>
#include <boost/container/detail/compare_functors.hpp>
#include <boost/container/detail/iterator.hpp>
#include <boost/container/detail/iterators.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/type_traits.hpp>
// move
#include <boost/move/utility_core.hpp>
#include <boost/move/core.hpp>
#include <boost/move/traits.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/iterator.hpp>
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#  include <boost/move/detail/fwd_macros.hpp>
#endif
#include <boost/move/detail/move_helpers.hpp>
#include <boost/move/detail/to_raw_pointer.hpp>
// intrusive
#include <boost/intrusive/pointer_traits.hpp>
//
#include <boost/assert.hpp>
#include <boost/container/detail/addressof.hpp>
// std
#include <algorithm>
#include <cstddef>
#include <functional>
#include <new>
#include <utility>
#include <boost/cstdint.hpp>
#include <climits>

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
#include <initializer_list>
#endif

#if !defined(BOOST_CONTAINER_NEST_DISABLE_SSE2)
#if defined(BOOST_CONTAINER_NEST_ENABLE_SSE2)|| \
    defined(__SSE2__) || \
    defined(_M_X64) || (defined(_M_IX86_FP)&&_M_IX86_FP>=2)
#define BOOST_CONTAINER_NEST_SSE2
#endif
#endif

#if defined(BOOST_CONTAINER_NEST_SSE2)
#include <emmintrin.h>
#endif


// Fancy-pointer-aware wrapper over BOOST_CONTAINER_PREFETCH (defined in
// workaround.hpp): it converts 'p' to a raw pointer before prefetching.
#define BOOST_CONTAINER_NEST_PREFETCH(p) \
   BOOST_CONTAINER_PREFETCH(boost::movelib::to_raw_pointer(p))

#define BOOST_CONTAINER_NEST_PREFETCH_BLOCK(pbb) \
   do{                                                    \
      BOOST_CONTAINER_NEST_PREFETCH(static_cast<block_type&>(*(pbb)).data());\
   } while(0)\
//


#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) // marked as __forceinline not inlined
#endif

namespace boost {
namespace container {

////////////////////////////////////////////////////////////////
//
//
//          OPTIONS FOR NEST CONTAINER (EXPERIMENTAL)
//
//
////////////////////////////////////////////////////////////////

#if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

template<bool StoreDataInBlock, bool Prefetch>
struct nest_opt
{
   BOOST_STATIC_CONSTEXPR bool store_data_in_block    = StoreDataInBlock;
   BOOST_STATIC_CONSTEXPR bool prefetch               = Prefetch;
};

typedef nest_opt<false, true> nest_null_opt;

#endif   //   !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

//! This option specifies whether block data (value storage) is inlined
//! in each block (true) or allocated separately via the allocator (false).
//! Inline data can improve locality but increases block size.
//!
//!\tparam Enabled A boolean value.
BOOST_INTRUSIVE_OPTION_CONSTANT(store_data_in_block, bool, Enabled, store_data_in_block)

//! This option specifies if hardware prefetch instructions
//! are emitted during iteration and other traversal operations.
//! When enabled, prefetching can improve performance by reducing
//! cache misses. The default value is true.
//!
//!\tparam Enabled A boolean value. True to enable prefetching.
BOOST_INTRUSIVE_OPTION_CONSTANT(prefetch, bool, Enabled, prefetch)

//! Helper metafunction to combine options into a single type to be used
//! by \c boost::container::nest.
#if defined(BOOST_CONTAINER_DOXYGEN_INVOKED) || defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void, class O4 = void>
#endif
struct nest_options
{
   /// @cond
   typedef typename ::boost::intrusive::pack_options
      < nest_null_opt,
      #if !defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
      O1, O2, O3, O4
      #else
      Options...
      #endif
      >::type packed_options;
   typedef nest_opt<
      packed_options::store_data_in_block,
      packed_options::prefetch> implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

template<class ...Options>
using nest_options_t = typename boost::container::nest_options<Options...>::type;

#endif

#endif   //   !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

template <class T
         ,class Allocator = void
         ,class Options = void>
class nest;

template<class ValuePointer, bool StoreDataInBlock, bool Prefetch>
class nest_iterator;

template<class T, class Allocator, class Options, class Predicate>
typename nest<T, Allocator, Options>::size_type
erase_if(nest<T, Allocator, Options>&, Predicate);

template<class T, class Allocator, class Options, class F>
F for_each(nest<T, Allocator, Options>&, F);

template<class T, class Allocator, class Options, class F>
F for_each(const nest<T, Allocator, Options>&, F);

template<class ValuePointer, bool StoreDataInBlock, bool Prefetch, class F>
std::pair< nest_iterator<ValuePointer, StoreDataInBlock, Prefetch>, F >
   for_each_while
      ( nest_iterator<ValuePointer, StoreDataInBlock, Prefetch>
      , nest_iterator<ValuePointer, StoreDataInBlock, Prefetch>
      , F);

namespace nest_detail {

//////////////////////////////////////////////
//
//      pointer_rebind helper
//
//////////////////////////////////////////////

template<class Pointer, class T>
struct pointer_rebind
{
   typedef typename boost::intrusive::pointer_traits<Pointer>::
      template rebind_pointer<T>::type type;
};

//////////////////////////////////////////////
//
//      bit manipulation helpers
//
//////////////////////////////////////////////

//Shared bit helpers live in boost::container::dtl (detail/bit_utilities.hpp).


//////////////////////////////////////////////
//
//      find_if_not (C++03-compatible)
//
//////////////////////////////////////////////

template<class It, class Pred>
It find_if_not(It first, It last, Pred pred)
{
   for (; first != last; ++first)
      if (!pred(*first))
         break;
   return first;
}

//////////////////////////////////////////////
//
//      block_base / block
//
//////////////////////////////////////////////

template<class VoidPointer>
struct block_base
{
   typedef typename pointer_rebind<VoidPointer, block_base>::type       pointer;
   typedef typename pointer_rebind<VoidPointer, const block_base>::type const_pointer;
   typedef boost::uint64_t mask_type;

   BOOST_STATIC_CONSTEXPR std::size_t N = sizeof(mask_type)*CHAR_BIT;
   BOOST_STATIC_CONSTEXPR mask_type full = (mask_type)(-1);

   BOOST_CONTAINER_FORCEINLINE static pointer pointer_to(block_base& x) BOOST_NOEXCEPT
   {
      return boost::intrusive::pointer_traits<pointer>::pointer_to(x);
   }

   BOOST_CONTAINER_FORCEINLINE static const_pointer pointer_to(const block_base& x) BOOST_NOEXCEPT
   {
      return boost::intrusive::pointer_traits<const_pointer>::pointer_to(x);
   }

   block_base()
   {
      this->reset();
      mask = 1; // sentinel
      //mask = 0; // sentinel
   }

   BOOST_CONTAINER_FORCEINLINE void link_available_before(pointer p) BOOST_NOEXCEPT
   {
      next_available = p;
      prev_available = p->prev_available;
      pointer const pthis = pointer_to(*this);
      next_available->prev_available = pthis;
      prev_available->next_available = pthis;
   }

   BOOST_CONTAINER_FORCEINLINE void link_available_after(pointer p) BOOST_NOEXCEPT
   {
      prev_available = p;
      next_available = p->next_available;
      pointer const pthis = pointer_to(*this);
      next_available->prev_available = pthis;
      prev_available->next_available = pthis;
   }

   BOOST_CONTAINER_FORCEINLINE void unlink_available() BOOST_NOEXCEPT
   {
      prev_available->next_available = next_available;
      next_available->prev_available = prev_available;
   }

   BOOST_CONTAINER_FORCEINLINE void link_before(pointer p) BOOST_NOEXCEPT
   {
      next = p;
      prev = p->prev;
      pointer const pthis = pointer_to(*this);
      next->prev = pthis;
      prev->next = pthis;
   }

   BOOST_CONTAINER_FORCEINLINE void unlink() BOOST_NOEXCEPT
   {
      prev->next = next;
      next->prev = prev;
   }

   BOOST_CONTAINER_FORCEINLINE pointer header() BOOST_NOEXCEPT
   {
      return pointer_to(*this);
   }

   BOOST_CONTAINER_FORCEINLINE const_pointer header() const BOOST_NOEXCEPT
   {
      return pointer_to(*this);
   }

   BOOST_CONTAINER_FORCEINLINE void link_at_back(pointer pb) BOOST_NOEXCEPT
   {
      pb->link_before(header());
   }

   BOOST_CONTAINER_FORCEINLINE void link_before(
      pointer pbx, pointer pby) BOOST_NOEXCEPT
   {
      pbx->link_before(pby);
   }

   BOOST_CONTAINER_FORCEINLINE static void unlink(pointer pb) BOOST_NOEXCEPT
   {
      pb->unlink();
   }

   BOOST_CONTAINER_FORCEINLINE void link_available_at_back(pointer pb) BOOST_NOEXCEPT
   {
      pb->link_available_before(header());
   }

   BOOST_CONTAINER_FORCEINLINE void link_available_at_front(pointer pb) BOOST_NOEXCEPT
   {
      pb->link_available_after(header());
   }

   BOOST_CONTAINER_FORCEINLINE void unlink_available(pointer pb) BOOST_NOEXCEPT
   {
      pb->unlink_available();
   }

   void reset() BOOST_NOEXCEPT
   {
      pointer const h = header();
      prev_available = h;
      next_available = h;
      prev = h;
      next = h;
   }

   block_base(BOOST_RV_REF(block_base) x) BOOST_NOEXCEPT
   {
      mask = 1; // sentinel
      this->operator=(boost::move(x));
   }

   block_base& operator=(BOOST_RV_REF(block_base) x) BOOST_NOEXCEPT
   {
      pointer const x_header = x.header();
      pointer const t_header = this->header();

      if (x.next_available != x_header) {
         prev_available = x.prev_available;
         next_available = x.next_available;
         next_available->prev_available = t_header;
         prev_available->next_available = t_header;
      }
      else {
         prev_available = t_header;
         next_available = t_header;
      }

      if (x.prev != x_header) {
         prev = x.prev;
         next = x.next;
         next->prev = t_header;
         prev->next = t_header;
      }
      else {
         prev = t_header;
         next = t_header;
      }

      x.reset();
      return *this;
   }

   //////////////////////////////////////////////
   //
   //   block_base linked-list memberships
   //
   //////////////////////////////////////////////
   //
   //Each block_base is simultaneously a node in TWO doubly-linked,
   //circular lists. Both lists are anchored at the per-nest `blist`
   //sentinel (itself a block_base, embedded inside `nest` and with no
   //associated data array) returned by `header()`.
   //
   //A given block participates in each list depending on the value of
   //its own `mask`:
   //
   //                    | main list | available list
   //   -----------------+-----------+----------------
   //    mask == 0       |    no     |     yes        (empty,  reusable)
   //    0 < mask < full |   yes     |     yes        (partial)
   //    mask == full    |   yes     |     no         (saturated)
   //
   //(1) MAIN LIST    -- members `prev` / `next`
   //
   //    Spans every block that holds at least one live element
   //    (mask != 0). Defines the user-visible block ordering: the
   //    nest's `begin()` returns an iterator to the first set bit
   //    of `blist.next->mask`; `nest_iterator::operator++` walks
   //    to the successor block via `pbb->next`, and
   //    `operator--` via `pbb->prev`. Thus iteration order is
   //    exactly the order in which blocks first became non-empty
   //    (link_at_back appends a freshly-non-empty block at the
   //    tail). The sentinel `header()` itself does NOT belong to
   //    the user-visible sequence; reaching `next == header()`
   //    signals end-of-range.
   //
   //    Maintenance points:
   //      * link_at_back(pb) on the empty->non-empty transition
   //        (priv_insert_range_copy, emplace's "mask was 0" branch).
   //      * unlink(pb) on the non-empty->empty transition
   //        (priv_erase_impl when mask becomes 0).
   //      * priv_reset's second sweep walks `blist.next` to deal
   //        with the blocks that the first available-list sweep
   //        left behind, which by then must all be saturated.
   //
   //(2) AVAILABLE LIST -- members `prev_available` / `next_available`
   //
   //    Spans every block that has at least one free slot
   //    (mask != full): empty blocks and partial blocks. This is
   //    the freelist consulted on every insertion by
   //    `priv_retrieve_available_block`, which returns
   //    `blist.next_available` and lands the new element on the
   //    lowest 0-bit of that block's mask. As long as the list is
   //    non-empty, insertion is O(1) and never reaches the
   //    allocator.
   //
   //    INVARIANT: the available list is partitioned so that all
   //    PARTIAL blocks come first and all EMPTY (reserved) blocks come
   //    last:  [ partial ... partial | empty ... empty ]. This lets
   //    `trim_capacity` walk the list backwards from `prev_available`
   //    and stop at the first non-empty block, making it linear on the
   //    number of reserved blocks rather than on the whole list.
   //
   //    Insertion-order policy that preserves the partition:
   //      * empty blocks (freshly allocated, or just drained by an
   //        erase/compaction) are linked at the BACK
   //        (link_available_at_back). Freshly allocated empties are
   //        thus also consumed last, after existing partials.
   //      * a saturated block that becomes partial again because an
   //        element was erased is linked at the FRONT
   //        (link_available_at_front, in priv_erase_impl and
   //        priv_erase_range). This favors reuse of cache-hot slots
   //        the program just vacated and keeps partials ahead of
   //        empties.
   //
   //    Maintenance points:
   //      * link_available_at_back(pb) on block creation and on every
   //        non-empty->empty transition (priv_erase_impl,
   //        priv_erase_range, priv_compact_*), preceded by
   //        unlink_available(pb) when the block was already partial so
   //        it is relocated to the back rather than duplicated.
   //      * link_available_at_front(pb) on the full->partial
   //        transition (priv_erase_impl, priv_erase_range when a
   //        block had been saturated).
   //      * unlink_available(pb) on the partial->full transition
   //        (emplace / priv_insert_range_copy when `mask + 1 == 0`).
   //      * priv_reset's first sweep walks `blist.header()->next_available`
   //        to destroy empty and partial blocks and free their data.
   //
   //Note that a partial block is on BOTH lists at once; its four
   //pointers are independent (the available chain may skip past full
   //blocks that the main chain visits, and vice versa).
   pointer   prev_available;
   pointer   next_available;
   pointer   prev;
   pointer   next;
   mask_type mask;
private:
   BOOST_MOVABLE_BUT_NOT_COPYABLE(block_base)
};

template<class ValuePointer, bool StoreDataInBlock>
struct block
   : block_base<typename pointer_rebind<ValuePointer, void>::type>
{
   typedef block_base<typename pointer_rebind<ValuePointer, void>::type> block_base_type;
   typedef typename boost::intrusive::pointer_traits<ValuePointer>::element_type value_type;
   typedef typename pointer_rebind<ValuePointer, block>::type            pointer;

   BOOST_CONTAINER_FORCEINLINE ValuePointer data() BOOST_NOEXCEPT { return data_; }
   BOOST_CONTAINER_FORCEINLINE void set_data_null() BOOST_NOEXCEPT { data_ = ValuePointer(); }

   block()
      : block_base_type(), data_()
   {}

   block(BOOST_RV_REF(block) x) BOOST_NOEXCEPT
      : block_base_type(BOOST_MOVE_BASE(block_base_type, x)), data_()
   {}

   block& operator=(BOOST_RV_REF(block) x) BOOST_NOEXCEPT
   {
      this->block_base_type::operator=(boost::move(x));
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE static pointer
   static_cast_block_pointer(typename block_base_type::pointer pbb) BOOST_NOEXCEPT
   {
      return boost::intrusive::pointer_traits<pointer>::pointer_to(
         static_cast<block&>(*pbb));
   }

   ValuePointer data_;
private:
   BOOST_MOVABLE_BUT_NOT_COPYABLE(block)
};

template<class ValuePointer>
struct block<ValuePointer, true>
   : block_base<typename pointer_rebind<ValuePointer, void>::type>
{
   typedef block_base<typename pointer_rebind<ValuePointer, void>::type> block_base_type;
   typedef typename boost::intrusive::pointer_traits<ValuePointer>::element_type value_type;
   typedef typename pointer_rebind<ValuePointer, block>::type            pointer;

   BOOST_CONTAINER_FORCEINLINE ValuePointer data() BOOST_NOEXCEPT { return static_cast<ValuePointer>(static_cast<void*>(&data_stor)); }
   BOOST_CONTAINER_FORCEINLINE void set_data_null() BOOST_NOEXCEPT {}

   block()
      : block_base_type()
   {}

   block(BOOST_RV_REF(block) x) BOOST_NOEXCEPT
      : block_base_type(BOOST_MOVE_BASE(block_base_type, x))
   {}

   block& operator=(BOOST_RV_REF(block) x) BOOST_NOEXCEPT
   {
      this->block_base_type::operator=(boost::move(x));
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE static pointer
   static_cast_block_pointer(typename block_base_type::pointer pbb) BOOST_NOEXCEPT
   {
      return boost::intrusive::pointer_traits<pointer>::pointer_to(
         static_cast<block&>(*pbb));
   }

   typename dtl::aligned_storage<sizeof(value_type)*64u, dtl::alignment_of<value_type>::value>::type data_stor;
private:
   BOOST_MOVABLE_BUT_NOT_COPYABLE(block)
};

template<class ValuePointer, bool StoreDataInBlock>
void swap_payload(block<ValuePointer, StoreDataInBlock>& x, block<ValuePointer, StoreDataInBlock>& y) BOOST_NOEXCEPT;

template<class ValuePointer>
BOOST_CONTAINER_FORCEINLINE void swap_payload(block<ValuePointer, true>& x, block<ValuePointer, true>& y) BOOST_NOEXCEPT
{
   boost::adl_move_swap(x.mask, y.mask);
}

template<class ValuePointer>
BOOST_CONTAINER_FORCEINLINE void swap_payload(block<ValuePointer, false>& x, block<ValuePointer, false>& y) BOOST_NOEXCEPT
{
   boost::adl_move_swap(x.mask, y.mask);
   boost::adl_move_swap(x.data_, y.data_);
}

BOOST_CONTAINER_FORCEINLINE int first_in_mask(boost::uint64_t m)
{
   return dtl::unchecked_countr_zero(m);
}

BOOST_CONTAINER_FORCEINLINE int last_in_mask(boost::uint64_t m)
{
   return 63 - dtl::unchecked_countl_zero(m);
}

} // namespace nest_detail

//////////////////////////////////////////////
//
//      nest_iterator
//
//////////////////////////////////////////////

template<class ValuePointer, bool StoreDataInBlock, bool Prefetch>
class nest_iterator
{
   typedef typename boost::intrusive::pointer_traits<ValuePointer>::element_type element_type;


public:
   typedef typename dtl::remove_const<element_type>::type                     value_type;
   typedef typename boost::intrusive::pointer_traits<ValuePointer>::difference_type difference_type;
   typedef ValuePointer                                                       pointer;
   typedef element_type&                                                      reference;
   typedef std::bidirectional_iterator_tag                                    iterator_category;

   struct nat
   {
      nat() : pbb(), n() {}
      pointer pbb;
      int n;
   };

   typedef typename nest_detail::pointer_rebind<pointer, value_type>::type maybe_nonconst_pointer;

   typedef typename dtl::if_c< boost::move_detail::is_const<element_type>::value
                             , nest_iterator< maybe_nonconst_pointer, StoreDataInBlock, Prefetch >
                             , nat>::type                            maybe_nonconst_iterator;

   BOOST_CONTAINER_FORCEINLINE nest_iterator() BOOST_NOEXCEPT
      : pbb(), n(0)
   {}
   
   BOOST_CONTAINER_FORCEINLINE nest_iterator(const nest_iterator& x) BOOST_NOEXCEPT
      : pbb(x.pbb), n(x.n)
   {}
   
   BOOST_CONTAINER_FORCEINLINE nest_iterator(const maybe_nonconst_iterator& x) BOOST_NOEXCEPT
      : pbb(x.pbb), n(x.n)
   {}

   BOOST_CONTAINER_FORCEINLINE nest_iterator& operator=(const nest_iterator& x) BOOST_NOEXCEPT
   {
      pbb = x.pbb;
      n = x.n;
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE nest_iterator& operator=(const maybe_nonconst_iterator& x) BOOST_NOEXCEPT
   {
      pbb = x.pbb;
      n = x.n;
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE pointer operator->() const BOOST_NOEXCEPT
   {
      return static_cast<block_type&>(*pbb).data() + n;
   }

   BOOST_CONTAINER_FORCEINLINE reference operator*() const BOOST_NOEXCEPT
   {
      return static_cast<block_type&>(*pbb).data()[n];
   }

   BOOST_CONTAINER_FORCEINLINE nest_iterator& operator++() BOOST_NOEXCEPT
   {
      mask_type mask = pbb->mask & (full << 1 << n);
      if(BOOST_UNLIKELY(mask == 0)) {
         pbb = pbb->next;
         BOOST_IF_CONSTEXPR(Prefetch) {
            //Load next critical metadata
            block_base_type& pbn = static_cast<block_base_type&>(*pbb->next);
            //Prefetch the next block's metadata plus the next metadata
            BOOST_CONTAINER_NEST_PREFETCH(&pbn.next->next);
            BOOST_CONTAINER_NEST_PREFETCH_BLOCK(&pbn);
         }
         mask = pbb->mask;
      }
      n = nest_detail::first_in_mask(mask);
      return *this;
   }
   BOOST_CONTAINER_FORCEINLINE nest_iterator operator++(int) BOOST_NOEXCEPT
   {
      nest_iterator tmp(*this);
      this->operator++();
      return tmp;
   }

   BOOST_CONTAINER_FORCEINLINE nest_iterator& operator--() BOOST_NOEXCEPT
   {
      mask_type mask = pbb->mask & (full >> 1 >> (((int)N - 1) - n));
      if (BOOST_UNLIKELY(mask == 0)) {
         pbb = pbb->prev;
         BOOST_IF_CONSTEXPR(Prefetch) {
            //Load previous data
            block_base_type& pbn = static_cast<block_base_type&>(*pbb->prev);
            BOOST_CONTAINER_NEST_PREFETCH(&pbn.prev->prev);
            BOOST_CONTAINER_NEST_PREFETCH_BLOCK(&pbn);
         }
         mask = pbb->mask;
      }
      n = nest_detail::last_in_mask(mask);
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE nest_iterator operator--(int) BOOST_NOEXCEPT
   {
      nest_iterator tmp(*this);
      this->operator--();
      return tmp;
   }

   BOOST_CONTAINER_FORCEINLINE friend bool operator==(const nest_iterator& x, const nest_iterator& y) BOOST_NOEXCEPT
   {
      return x.pbb == y.pbb && x.n == y.n;
   }

   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(const nest_iterator& x, const nest_iterator& y) BOOST_NOEXCEPT
   {
      return !(x == y);
   }

private:
   template<class, bool, bool> friend class nest_iterator;
   template<class, class, class> friend class boost::container::nest;
   template<class VP, bool SDIB, bool Pf, class FF>
   friend std::pair< nest_iterator<VP, SDIB, Pf>, FF >
      for_each_while
         ( nest_iterator<VP, SDIB, Pf>
         , nest_iterator<VP, SDIB, Pf>
         , FF);

   typedef typename nest_detail::pointer_rebind<ValuePointer, void>::type  void_pointer;
   typedef nest_detail::block_base<void_pointer>                           block_base_type;
   typedef typename nest_detail::pointer_rebind
         <ValuePointer, block_base_type>::type                             block_base_pointer;
   typedef typename nest_detail::pointer_rebind
         <ValuePointer, const block_base_type>::type                       const_block_base_pointer;
   typedef typename nest_detail::pointer_rebind
         <ValuePointer, value_type>::type                                  nonconst_pointer;
   typedef nest_detail::block<nonconst_pointer, StoreDataInBlock>          block_type;
   typedef typename block_base_type::mask_type                             mask_type;

   BOOST_STATIC_CONSTEXPR std::size_t  N = block_base_type::N;
   BOOST_STATIC_CONSTEXPR mask_type full = block_base_type::full;
   BOOST_STATIC_CONSTEXPR bool prefetch_enabled = Prefetch;

   BOOST_CONTAINER_FORCEINLINE nest_iterator(const_block_base_pointer pbb_, int n_) BOOST_NOEXCEPT
      : pbb(const_cast_block_base_pointer(pbb_)), n(n_)
   {}

   BOOST_CONTAINER_FORCEINLINE static block_base_pointer
   const_cast_block_base_pointer(const_block_base_pointer pbb_) BOOST_NOEXCEPT
   {
      return block_base_type::pointer_to(const_cast<block_base_type&>(*pbb_));
   }

   block_base_pointer pbb;
   int                n;
};

namespace nest_detail {

//////////////////////////////////////////////
//
//      sort_iterator
//
//////////////////////////////////////////////

template<class T, std::size_t N>
struct sort_iterator
{
   typedef T                                 value_type;
   typedef std::ptrdiff_t                    difference_type;
   typedef T*                                pointer;
   typedef T&                                reference;
   typedef std::random_access_iterator_tag   iterator_category;

   sort_iterator(T** pp_, difference_type index_)
      : pp(pp_), index(index_)
   {}

   pointer operator->() const BOOST_NOEXCEPT
   { return pp[(std::size_t)index / N] + ((std::size_t)index % N); }

   reference operator*() const BOOST_NOEXCEPT
   { return *operator->(); }

   sort_iterator& operator++() BOOST_NOEXCEPT       { ++index; return *this; }
   sort_iterator  operator++(int) BOOST_NOEXCEPT     { sort_iterator t(*this); ++index; return t; }
   sort_iterator& operator--() BOOST_NOEXCEPT        { --index; return *this; }
   sort_iterator  operator--(int) BOOST_NOEXCEPT     { sort_iterator t(*this); --index; return t; }

   friend difference_type
   operator-(const sort_iterator& x, const sort_iterator& y) BOOST_NOEXCEPT
   { return x.index - y.index; }

   sort_iterator& operator+=(difference_type d) BOOST_NOEXCEPT
   { index += d; return *this; }

   friend sort_iterator
   operator+(const sort_iterator& x, difference_type d) BOOST_NOEXCEPT
   { return sort_iterator(x.pp, x.index + d); }

   friend sort_iterator
   operator+(difference_type d, const sort_iterator& x) BOOST_NOEXCEPT
   { return sort_iterator(x.pp, d + x.index); }

   sort_iterator& operator-=(difference_type d) BOOST_NOEXCEPT
   { index -= d; return *this; }

   friend sort_iterator
   operator-(const sort_iterator& x, difference_type d) BOOST_NOEXCEPT
   { return sort_iterator(x.pp, x.index - d); }

   reference operator[](difference_type d) const BOOST_NOEXCEPT
   { return *(*this + d); }

   friend bool operator==(const sort_iterator& x, const sort_iterator& y) BOOST_NOEXCEPT
   { return x.index == y.index; }
   friend bool operator!=(const sort_iterator& x, const sort_iterator& y) BOOST_NOEXCEPT
   { return x.index != y.index; }
   friend bool operator< (const sort_iterator& x, const sort_iterator& y) BOOST_NOEXCEPT
   { return x.index <  y.index; }
   friend bool operator> (const sort_iterator& x, const sort_iterator& y) BOOST_NOEXCEPT
   { return x.index >  y.index; }
   friend bool operator<=(const sort_iterator& x, const sort_iterator& y) BOOST_NOEXCEPT
   { return x.index <= y.index; }
   friend bool operator>=(const sort_iterator& x, const sort_iterator& y) BOOST_NOEXCEPT
   { return x.index >= y.index; }

   T**             pp;
   difference_type index;
};

//////////////////////////////////////////////
//
//      RAII helpers (replacing unique_ptr)
//
//////////////////////////////////////////////

template<class T, class Allocator>
struct buffer
{
   typedef boost::container::allocator_traits<Allocator> alloc_traits;

   buffer(std::size_t n_, Allocator al_) BOOST_NOEXCEPT
      : al(al_), begin_idx(0), end_idx(0), cap(0), data(0)
   {
      allocate_data(n_);
      if(data) cap = n_;
   }

   ~buffer()
   {
      if(data) {
         for(; begin_idx != end_idx; ++begin_idx) {
            alloc_traits::destroy(al, data + begin_idx);
         }
         deallocate_data();
      }
   }

   T* begin() const BOOST_NOEXCEPT { return data + begin_idx; }
   T* end()   const BOOST_NOEXCEPT { return data + end_idx; }

   void push_back_move(T& v)
   {
      BOOST_ASSERT(data && end_idx != cap);
      alloc_traits::construct(al, data + end_idx, boost::move(v));
      ++end_idx;
   }

   void erase_front() BOOST_NOEXCEPT
   {
      BOOST_ASSERT(data && begin_idx != end_idx);
      alloc_traits::destroy(al, data + begin_idx);
      ++begin_idx;
   }

   Allocator   al;
   std::size_t begin_idx;
   std::size_t end_idx;
   std::size_t cap;
   T*          data;

private:
   buffer(const buffer&);
   buffer& operator=(const buffer&);

   //Portable, nothrow, (over)aligned allocation: this is the same primitive
   //boost::container::new_allocator relies on for overalignment, so there is no
   //need to special-case __cpp_aligned_new here. The nothrow null return lets
   //the sort routines fall back to a leaner algorithm when this (possibly large)
   //scratch buffer cannot be allocated.
   void allocate_data(std::size_t m)
   {
      data = static_cast<T*>(dtl::aligned_allocate(dtl::alignment_of<T>::value, m * sizeof(T)));
   }

   void deallocate_data() { dtl::aligned_deallocate(static_cast<void*>(data)); }
};

// RAII wrapper for raw memory (replaces unique_ptr with nodtor_deleter)
struct raw_memory_holder
{
   explicit raw_memory_holder(void* p_) BOOST_NOEXCEPT : p(p_) {}
   ~raw_memory_holder() { if(p) ::operator delete(p); }
   void release() BOOST_NOEXCEPT { p = 0; }
   void* get() const BOOST_NOEXCEPT { return p; }
   void* p;
private:
   raw_memory_holder(const raw_memory_holder&);
   raw_memory_holder& operator=(const raw_memory_holder&);
};

//////////////////////////////////////////////
//
//    allocator propagation helpers
//
//////////////////////////////////////////////

template<class T>
void copy_assign_if(dtl::true_type, T& x, const T& y) { x = y; }

template<class T>
void copy_assign_if(dtl::false_type, T&, const T&) {}

template<class T>
void move_assign_if(dtl::true_type, T& x, T& y) { x = boost::move(y); }

template<class T>
void move_assign_if(dtl::false_type, T&, T&) {}

//////////////////////////////////////////////
//
//    block_typedefs
//
//////////////////////////////////////////////

template<class ValueAllocator, bool StoreDataInBlock>
struct block_typedefs
{
   typedef boost::container::allocator_traits<ValueAllocator>   val_alloc_traits;
   typedef typename val_alloc_traits::pointer                   value_pointer;
   typedef typename pointer_rebind<value_pointer, void>::type   void_pointer;

   typedef nest_detail::block_base<void_pointer>                 block_base_t;
   typedef typename pointer_rebind<
      value_pointer, block_base_t>::type                        block_base_pointer;
   typedef typename pointer_rebind<
      value_pointer, const block_base_t>::type                  const_block_base_pointer;

   typedef nest_detail::block<value_pointer, StoreDataInBlock> block_type;
   typedef typename pointer_rebind<
      value_pointer, block_type>::type                             block_pointer;

   typedef typename val_alloc_traits::
      template portable_rebind_alloc<block_type>::type             block_allocator;
};

//////////////////////////////////////////////
//
//    predicate adaptor for unique()
//
//////////////////////////////////////////////

template<class T, class BinaryPredicate>
struct unique_pred_adaptor
{
   const T*         value_ptr;
   BinaryPredicate& pred;
   unique_pred_adaptor(const T* v, BinaryPredicate& p) : value_ptr(v), pred(p) {}
   bool operator()(const T& x) const { return pred(x, *value_ptr); }
};

//////////////////////////////////////////////
//
//    sort_proxy_comparator
//
//////////////////////////////////////////////

template<class T, class Compare>
struct sort_proxy_comparator
{
   Compare& comp;
   explicit sort_proxy_comparator(Compare& c) : comp(c) {}

   template<class SortProxy>
   bool operator()(const SortProxy& x, const SortProxy& y) const
   {
      return comp(
         const_cast<const T&>(*x.p),
         const_cast<const T&>(*y.p));
   }
};

//////////////////////////////////////////////
//
//    for_each adaptor: wraps a unary function so it can be used as the
//    predicate of for_each_while (always returns true).
//
//////////////////////////////////////////////

template<class F, class T>
struct for_each_adaptor
{
   F& f;
   BOOST_CONTAINER_FORCEINLINE explicit for_each_adaptor(F& f_) : f(f_) {}
   BOOST_CONTAINER_FORCEINLINE bool operator()(T& x) const { f(x); return true; }
};

//////////////////////////////////////////////
//
//    ref_predicate_adaptor: holds the wrapped predicate by reference so its
//    state is preserved when the adaptor is copied around by the visiting
//    machinery.
//
//////////////////////////////////////////////

template<class F, class T>
struct ref_predicate_adaptor
{
   F& f;
   BOOST_CONTAINER_FORCEINLINE explicit ref_predicate_adaptor(F& f_) : f(f_) {}
   BOOST_CONTAINER_FORCEINLINE bool operator()(T& x) const { return f(x); }
};

} // namespace nest_detail

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

template<class Options>
struct get_nest_opt
{
   typedef nest_opt<
      Options::store_data_in_block,
      Options::prefetch> type;
};

template<>
struct get_nest_opt<void>
{
   typedef nest_null_opt type;
};

template<bool B, bool P>
struct get_nest_opt<nest_opt<B, P> >
{
   typedef nest_opt<B, P> type;
};

#endif // BOOST_CONTAINER_DOXYGEN_INVOKED

//////////////////////////////////////////////////////////////////////////////
//
//                                nest
//
//////////////////////////////////////////////////////////////////////////////

//! A nest is a node-like container with a fixed block size of 64 elements
//! and bitmask-based slot management. It provides stable iterators and
//! efficient insertion and erasure operations in constant time.
//!
//! \tparam T The type of object stored in the nest
//! \tparam Allocator The allocator used for all internal memory management, use void
//!   for the default allocator
//! \tparam Options A type produced from \c boost::container::nest_options (e.g. \c store_data_in_block).
#ifdef BOOST_CONTAINER_DOXYGEN_INVOKED
template <class T, class Allocator = void, class Options = void>
#else
template <class T, class Allocator, class Options>
#endif
class nest
   //EBO: nest derives directly from its (block) allocator
   : private nest_detail::block_typedefs<
        typename real_allocator<T, Allocator>::type
      , get_nest_opt<Options>::type::store_data_in_block
     >::block_allocator
{
   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   typedef typename real_allocator<T, Allocator>::type             ValueAllocator;
   typedef typename get_nest_opt<Options>::type                    options_type;
   BOOST_STATIC_CONSTEXPR bool store_data_in_block    = options_type::store_data_in_block;
   BOOST_STATIC_CONSTEXPR bool prefetch_enabled       = options_type::prefetch;
   typedef boost::container::allocator_traits<ValueAllocator>      allocator_traits_type;
   typedef nest_detail::block_typedefs<ValueAllocator, store_data_in_block> btd;
   typedef typename btd::block_base_t                              block_base;
   typedef typename btd::block_base_pointer                        block_base_pointer;
   typedef typename btd::const_block_base_pointer                  const_block_base_pointer;
   typedef typename btd::block_type                                block_type;
   typedef typename btd::block_pointer                             block_pointer;
   typedef typename btd::block_allocator                           block_allocator;
   typedef block_allocator                                         allocator_base;
   typedef typename block_base::mask_type                          mask_type;
   typedef boost::container::allocator_traits<block_allocator>     block_alloc_traits;

   BOOST_STATIC_CONSTEXPR std::size_t     N    = block_base::N;
   BOOST_STATIC_CONSTEXPR mask_type full = block_base::full;

   BOOST_COPYABLE_AND_MOVABLE(nest)
   #endif // BOOST_CONTAINER_DOXYGEN_INVOKED

   public:
   //////////////////////////////////////////////
   //
   //                    types
   //
   //////////////////////////////////////////////

   typedef T                                                                value_type;
   typedef ValueAllocator                                                   allocator_type;
   typedef typename allocator_traits_type::pointer                          pointer;
   typedef typename allocator_traits_type::const_pointer                    const_pointer;
   typedef T&                                                               reference;
   typedef const T&                                                         const_reference;
   typedef typename allocator_traits_type::size_type                        size_type;
   typedef typename allocator_traits_type::difference_type                  difference_type;
   typedef BOOST_CONTAINER_IMPDEF(nest_iterator<pointer BOOST_MOVE_I store_data_in_block BOOST_MOVE_I prefetch_enabled>)            iterator;
   typedef BOOST_CONTAINER_IMPDEF(nest_iterator<const_pointer BOOST_MOVE_I store_data_in_block BOOST_MOVE_I prefetch_enabled>)      const_iterator;
   typedef BOOST_CONTAINER_IMPDEF(boost::container::reverse_iterator<iterator>)       reverse_iterator;
   typedef BOOST_CONTAINER_IMPDEF(boost::container::reverse_iterator<const_iterator>) const_reverse_iterator;

   //////////////////////////////////////////////
   //
   //          construct/copy/destroy
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Default constructs a nest.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor throws.
   //!
   //! <b>Complexity</b>: Constant.
   nest() BOOST_NOEXCEPT_IF(dtl::is_nothrow_default_constructible<ValueAllocator>::value)
      : allocator_base()
      , blist()
      , num_blocks(0)
      , size_(0)
   {}

   //! <b>Effects</b>: Constructs a nest taking the allocator as parameter.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   explicit nest(const allocator_type& a) BOOST_NOEXCEPT_OR_NOTHROW
      : allocator_base(block_allocator(a))
      , blist()
      , num_blocks(0)
      , size_(0)
   {}

   //! <b>Effects</b>: Constructs a nest and inserts n value-initialized elements.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's default constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   explicit nest(size_type n, const allocator_type& a = allocator_type())
      : allocator_base(block_allocator(a))
      , blist()
      , num_blocks(0)
      , size_(0)
   {
      priv_insert_n_default(n);
   }

   //! <b>Effects</b>: Constructs a nest and inserts n copies of x.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   nest(size_type n, const T& x, const allocator_type& a = allocator_type())
      : allocator_base(block_allocator(a))
      , blist()
      , num_blocks(0)
      , size_(0)
   {
      insert(n, x);
   }

   //! <b>Effects</b>: Constructs a nest and inserts a copy of [first, last).
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's constructor taking a dereferenced InpIt throws.
   //!
   //! <b>Complexity</b>: Linear to the range [first, last).
   template<class InpIt>
   nest(InpIt first, InpIt last
      ,const allocator_type& a = allocator_type()
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_convertible<InpIt, size_type>::type* = 0
      #endif
      )
      : allocator_base(block_allocator(a))
      , blist()
      , num_blocks(0)
      , size_(0)
   {
      insert(first, last);
   }

   //! <b>Effects</b>: Copy constructs a nest.
   //!
   //! <b>Postcondition</b>: x == *this.
   //!
   //! <b>Throws</b>: If allocator_type's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the elements x contains.
   nest(const nest& x)
      : allocator_base(block_allocator(
           allocator_traits_type::select_on_container_copy_construction(x.priv_alloc())))
      , blist()
      , num_blocks(0)
      , size_(0)
   {
      insert(x.begin(), x.end());
   }

   //! <b>Effects</b>: Move constructor. Moves x's resources to *this.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   nest(BOOST_RV_REF(nest) x) BOOST_NOEXCEPT_OR_NOTHROW
      : allocator_base(boost::move(x.al()))
      , blist(boost::move(x.blist))
      , num_blocks(x.num_blocks)
      , size_(x.size_)
   {
      x.num_blocks = 0;
      x.size_ = 0;
   }

   //! <b>Effects</b>: Copy constructs a nest using the specified allocator.
   //!
   //! <b>Postcondition</b>: x == *this.
   //!
   //! <b>Throws</b>: If allocator_type's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the elements x contains.
   nest(const nest& x, const BOOST_CONTAINER_DOC1ST(allocator_type, typename dtl::type_identity<allocator_type>::type)& a)
      : allocator_base(block_allocator(a))
      , blist()
      , num_blocks(0)
      , size_(0)
   {
      insert(x.begin(), x.end());
   }

   //! <b>Effects</b>: Move constructor using the specified allocator.
   //!                 Moves x's resources to *this.
   //!
   //! <b>Throws</b>: If allocation or value_type's copy constructor throws.
   //!
   //! <b>Complexity</b>: Constant if a == x.get_allocator(), linear otherwise.
   nest(BOOST_RV_REF(nest) x, const BOOST_CONTAINER_DOC1ST(allocator_type, typename dtl::type_identity<allocator_type>::type)& a)
      : allocator_base(block_allocator(a))
      , blist()
      , num_blocks(0)
      , size_(0)
   {
      if(al() == x.al()){
         blist = boost::move(x.blist);
         num_blocks = x.num_blocks;
         size_ = x.size_;
         x.num_blocks = 0;
         x.size_ = 0;
      }
      else{
         priv_insert_range_move(x.begin(), x.end());
         x.priv_reset();
      }
   }

   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Effects</b>: Constructs a nest and inserts elements from il.
   //!
   //! <b>Complexity</b>: Linear to the range [il.begin(), il.end()).
   nest(std::initializer_list<value_type> il, const allocator_type& a = allocator_type())
      : allocator_base(block_allocator(a))
      , blist()
      , num_blocks(0)
      , size_(0)
   {
      insert(il.begin(), il.end());
   }
   #endif

   //! <b>Effects</b>: Destroys the nest. All stored values are destroyed
   //!   and used memory is deallocated.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements.
   ~nest() BOOST_NOEXCEPT_OR_NOTHROW
   {
      priv_reset();
   }

   //! <b>Effects</b>: Makes *this contain the same elements as x.
   //!
   //! <b>Throws</b>: If memory allocation throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in x.
   nest& operator=(BOOST_COPY_ASSIGN_REF(nest) x)
   {
      if (BOOST_LIKELY(this != &x)) {
         dtl::bool_<allocator_traits_type::
            propagate_on_container_copy_assignment::value> pocca;
         if(priv_alloc() != x.priv_alloc() && pocca.value) {
            priv_reset();
            nest_detail::copy_assign_if(pocca, al(), x.al());
            insert(x.begin(), x.end());
         }
         else{
            nest_detail::copy_assign_if(pocca, al(), x.al());
            assign(x.begin(), x.end());
         }
      }
      return *this;
   }

   //! <b>Effects</b>: Move assignment. All x's values are transferred to *this.
   //!
   //! <b>Throws</b>: If allocator_traits_type::propagate_on_container_move_assignment
   //!   is false and (allocation throws or value_type's move constructor throws).
   //!
   //! <b>Complexity</b>: Constant if allocator_traits_type::
   //!   propagate_on_container_move_assignment is true or
   //!   this->get_allocator() == x.get_allocator(). Linear otherwise.
   nest& operator=(BOOST_RV_REF(nest) x)
      BOOST_NOEXCEPT_IF(allocator_traits_type::propagate_on_container_move_assignment::value
                                  || allocator_traits_type::is_always_equal::value)
   {
      if (BOOST_LIKELY(this != &x)) {
         BOOST_STATIC_CONSTEXPR bool can_steal =
            allocator_traits_type::propagate_on_container_move_assignment::value ||
            allocator_traits_type::is_always_equal::value;
         priv_move_assign(x, dtl::bool_<can_steal>());
      }
      return *this;
   }

   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Effects</b>: Assigns the elements of il to *this.
   //!
   //! <b>Complexity</b>: Linear.
   nest& operator=(std::initializer_list<value_type> il)
   {
      assign(il.begin(), il.end());
      return *this;
   }
   #endif

   //! <b>Effects</b>: Assigns [first, last) to *this.
   //!
   //! <b>Complexity</b>: Linear.
   template<class InpIt>
   void assign(InpIt first, InpIt last
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_convertible<InpIt, size_type>::type* = 0
      #endif
      )
   {
      priv_range_assign(first, last);
   }

   //! <b>Effects</b>: Assigns n copies of val to *this.
   //!
   //! <b>Complexity</b>: Linear.
   void assign(size_type n, const T& val)
   {
      priv_assign_n(n, val);
   }

   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Effects</b>: Assigns the elements of il to *this.
   //!
   //! <b>Complexity</b>: Linear.
   void assign(std::initializer_list<value_type> il)
   { assign(il.begin(), il.end()); }
   #endif

   //! <b>Effects</b>: Returns a copy of the allocator.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   allocator_type get_allocator() const BOOST_NOEXCEPT_OR_NOTHROW
   { return allocator_type(al()); }

   //////////////////////////////////////////////
   //
   //                iterators
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Returns an iterator to the first element.
   //!
   //! <b>Complexity</b>: Constant.
   iterator               begin()        BOOST_NOEXCEPT { return ++end(); }
   //! <b>Effects</b>: Returns a const_iterator to the first element.
   const_iterator         begin()  const BOOST_NOEXCEPT { return ++end(); }
   //! <b>Effects</b>: Returns an iterator to the end.
   iterator               end()          BOOST_NOEXCEPT { return iterator(blist.header(), 0); }
   //! <b>Effects</b>: Returns a const_iterator to the end.
   const_iterator         end()    const BOOST_NOEXCEPT { return const_iterator(blist.header(), 0); }
   //! <b>Effects</b>: Returns a reverse_iterator to the rbegin.
   reverse_iterator       rbegin()       BOOST_NOEXCEPT { return reverse_iterator(end()); }
   //! <b>Effects</b>: Returns a const_reverse_iterator to the rbegin.
   const_reverse_iterator rbegin() const BOOST_NOEXCEPT { return const_reverse_iterator(end()); }
   //! <b>Effects</b>: Returns a reverse_iterator to the rend.
   reverse_iterator       rend()         BOOST_NOEXCEPT { return reverse_iterator(begin()); }
   //! <b>Effects</b>: Returns a const_reverse_iterator to the rend.
   const_reverse_iterator rend()   const BOOST_NOEXCEPT { return const_reverse_iterator(begin()); }
   //! <b>Effects</b>: Returns a const_iterator to the first element.
   const_iterator         cbegin() const BOOST_NOEXCEPT { return begin(); }
   //! <b>Effects</b>: Returns a const_iterator to the end.
   const_iterator         cend()   const BOOST_NOEXCEPT { return end(); }
   //! <b>Effects</b>: Returns a const_reverse_iterator to the rbegin.
   const_reverse_iterator crbegin()const BOOST_NOEXCEPT { return rbegin(); }
   //! <b>Effects</b>: Returns a const_reverse_iterator to the rend.
   const_reverse_iterator crend()  const BOOST_NOEXCEPT { return rend(); }

   //////////////////////////////////////////////
   //
   //                capacity
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Returns true if the nest contains no elements.
   //!
   //! <b>Complexity</b>: Constant.
   bool      empty()    const BOOST_NOEXCEPT { return size_ == 0; }

   //! <b>Effects</b>: Returns the number of elements.
   //!
   //! <b>Complexity</b>: Constant.
   size_type size()     const BOOST_NOEXCEPT { return size_; }

   //! <b>Effects</b>: Returns the maximum possible number of elements.
   //!
   //! <b>Complexity</b>: Constant.
   size_type max_size() const BOOST_NOEXCEPT
   {
      std::size_t bs = (std::size_t)block_alloc_traits::max_size(al()) * sizeof(block_type);
      allocator_type val_al(al());
      std::size_t vs = (std::size_t)allocator_traits_type::max_size(val_al) * sizeof(T);
      return (size_type)((std::min)(bs, vs) / (sizeof(block_type) + sizeof(T) * N) * N);
   }

   //! <b>Effects</b>: Returns the total number of slots (used and unused).
   //!
   //! <b>Complexity</b>: Constant.
   size_type capacity() const BOOST_NOEXCEPT { return num_blocks * N; }

   //! <b>Effects</b>: Reserves space for at least n elements.
   //!
   //! <b>Complexity</b>: Linear.
   void reserve(size_type n)
   {
      if(n > max_size()) {
         throw_length_error("Requested capacity greater than max_size()");
      }
      while(capacity() < n) (void)priv_create_new_available_block();
   }

   //! <b>Effects</b>: Compacts elements and removes unused blocks.
   //!
   //! <b>Complexity</b>: Linear.
   void shrink_to_fit()
   {
      priv_compact_noop_track();
      trim_capacity();
   }

   //! <b>Effects</b>: Releases all reserved (empty) blocks.
   //!
   //! <b>Complexity</b>: Linear on the number of reserved (empty) blocks.
   void trim_capacity() BOOST_NOEXCEPT { trim_capacity(0); }

   //! <b>Effects</b>: Releases reserved blocks until capacity() <= n.
   //!
   //! <b>Complexity</b>: Linear on the number of reserved (empty) blocks released.
   void trim_capacity(size_type n) BOOST_NOEXCEPT
   {
      if(capacity() <= n) return;

      //The available list is partitioned as [partial... | empty...], so all
      //reserved (mask == 0) blocks sit at the back. Walk it backwards from
      //prev_available and stop at the first non-empty block: this makes the
      //operation linear on the number of reserved blocks, not on the whole
      //available list.
      block_base_pointer pbb = blist.header()->prev_available;
      while((capacity() - n >= N) && pbb != blist.header() && pbb->mask == 0) {
         block_pointer pb = static_cast_block_pointer(pbb);
         pbb = pbb->prev_available;
         blist.unlink_available(pb);
         priv_delete_block(pb);
         --num_blocks;
      }
   }

   //////////////////////////////////////////////
   //
   //                modifiers
   //
   //////////////////////////////////////////////

   #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   //! <b>Effects</b>: Inserts an element constructed in-place with args.
   //!
   //! <b>Returns</b>: An iterator to the inserted element.
   //!
   //! <b>Throws</b>: Nothing unless the element's constructor throws. If it does,
   //!   the container is left in its original state (strong exception guarantee):
   //!   any block speculatively allocated for the new element is freed.
   //!
   //! <b>Complexity</b>: Constant (amortized).
   template<class ...Args>
   BOOST_CONTAINER_FORCEINLINE iterator emplace(BOOST_FWD_REF(Args)... args)
   {
      block_base_pointer const pbb_prev = blist.next_available;
      int n;
      block_pointer const pb = priv_retrieve_available_block(n);
      //Store the mask on a register so that some compilers
      //don't reload it asumming construction can modify it.
      const mask_type m = pb->mask;
      this->priv_construct_or_restore_capacity(
         boost::movelib::to_raw_pointer(pb->data()) + n, pbb_prev,
         boost::forward<Args>(args)...);
      const mask_type new_mask = m | (m + 1u);
      pb->mask = new_mask;
      const mask_type mask_plus_one = new_mask + 1u;
      if (BOOST_UNLIKELY(mask_plus_one <= 2)) {
         // pb->mask == 0 (impossible), 1 or full
         if (mask_plus_one == 0) blist.unlink_available(pb);
         // pb->mask == 1
         else                    blist.link_at_back(pb);
      }

      ++size_;
      return iterator(pb, n);
   }

   //! <b>Effects</b>: Inserts an element constructed in-place with args (hint ignored).
   //!
   //! <b>Returns</b>: An iterator to the inserted element.
   //!
   //! <b>Complexity</b>: Constant (amortized).
   template<class ...Args>
   BOOST_CONTAINER_FORCEINLINE iterator emplace_hint(const_iterator, BOOST_FWD_REF(Args)... args)
   { return emplace(boost::forward<Args>(args)...); }

   //! <b>Effects</b>: Inserts an element constructed in-place with args.
   //!
   //! <b>Returns</b>: An iterator to the inserted element.
   //!
   //! <b>Throws</b>: Nothing unless the element's constructor throws.
   //!   Basic exception guarantee.
   //!
   //! <b>Complexity</b>: Constant (amortized).
   //! 
   //! <b>Note</b>: Experimental API.
   template<class ...Args>
   BOOST_CONTAINER_FORCEINLINE iterator quick_emplace(BOOST_FWD_REF(Args)... args)
   {
      //Read the available block's mask exactly once and keep it in a register.
      block_base_pointer const pbb = blist.next_available;
      block_pointer pb;
      mask_type m;
      int n;
      if (BOOST_LIKELY(pbb != blist.header())) {
         m = pbb->mask;
         n = dtl::unchecked_countr_one(m);
         pb = static_cast_block_pointer(pbb);
      }
      else {
         m = 0;   //freshly created: mask == 0, n = 0
         n = 0;
         pb = priv_create_new_available_block();
      }
      
      //If construct throws, the (possibly freshly allocated) block is left
      //linked as an empty available block: size_/mask are only updated below,
      //so no element is counted. This is the same state reserve() produces.
      //This decision simplifies the implementation and improves performance
      //for some compilers (no exception rollback code needed).
      block_alloc_traits::construct
         (al(), boost::movelib::to_raw_pointer(pb->data()) + n, boost::forward<Args>(args)...);

      const mask_type new_mask = m | (m + 1u);
      pb->mask = new_mask;
      const mask_type new_mask_plus_one = new_mask + 1u;
      if (BOOST_UNLIKELY(new_mask_plus_one <= 2u)) {
         // new_mask == full (block just filled)
         if (new_mask_plus_one == 0u) blist.unlink_available(pb);
         // new_mask == 1 (block went empty -> non-empty)
         else                         blist.link_at_back(pb);
      }

      ++size_;
      return iterator(pb, n);
   }

   #else // BOOST_NO_CXX11_VARIADIC_TEMPLATES

   #define BOOST_CONTAINER_NEST_QUICK_EMPLACE_BODY(N)                \
      block_base_pointer const pbb = blist.next_available;           \
      block_pointer pb;                                              \
      mask_type m;                                                   \
      int n;                                                         \
      if (BOOST_LIKELY(pbb != blist.header())) {                     \
         m = pbb->mask;                                              \
         n = dtl::unchecked_countr_one(m);                   \
         pb = static_cast_block_pointer(pbb);                        \
      }                                                              \
      else {                                                         \
         m = 0;                                                      \
         n = 0;                                                      \
         pb = priv_create_new_available_block();                     \
      }                                                              \
      block_alloc_traits::construct                                  \
         (al(), boost::movelib::to_raw_pointer(pb->data()) + n       \
         BOOST_MOVE_I##N BOOST_MOVE_FWD##N);                         \
      const mask_type new_mask = m | (m + 1u);                       \
      pb->mask = new_mask;                                           \
      const mask_type new_mask_plus_one = new_mask + 1u;             \
      if (BOOST_UNLIKELY(new_mask_plus_one <= 2u)) {                 \
         if (new_mask_plus_one == 0u) blist.unlink_available(pb);    \
         else                         blist.link_at_back(pb);        \
      }                                                              \
      ++size_;                                                       \
      return iterator(pb, n);                                        \
   //

   #define BOOST_CONTAINER_NEST_EMPLACE_CODE(N) \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N \
   BOOST_CONTAINER_FORCEINLINE iterator emplace(BOOST_MOVE_UREF##N)  \
   {                                                                 \
      block_base_pointer const pbb_prev = blist.next_available;      \
      int n;                                                         \
      block_pointer const pb = priv_retrieve_available_block(n);     \
      const mask_type m = pb->mask;                                  \
      this->priv_construct_or_restore_capacity(                      \
         boost::movelib::to_raw_pointer(pb->data()) + n, pbb_prev    \
         BOOST_MOVE_I##N BOOST_MOVE_FWD##N);                         \
      const mask_type new_mask = m | (m + 1u);                       \
      pb->mask = new_mask;                                           \
      const mask_type mask_plus_one = new_mask + 1u;                 \
      if (BOOST_UNLIKELY(mask_plus_one <= 2)) {                      \
         if (mask_plus_one == 0) blist.unlink_available(pb);         \
         else                    blist.link_at_back(pb);             \
      }                                                              \
      ++size_;                                                       \
      return iterator(pb, n);                                        \
   }                                                                 \
   \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N \
   BOOST_CONTAINER_FORCEINLINE iterator emplace_hint(const_iterator BOOST_MOVE_I##N BOOST_MOVE_UREF##N) \
   {  return emplace(BOOST_MOVE_FWD##N);  }                                 \
   \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N \
   BOOST_CONTAINER_FORCEINLINE iterator quick_emplace(BOOST_MOVE_UREF##N)  \
   {                                                                 \
      BOOST_CONTAINER_NEST_QUICK_EMPLACE_BODY(N)                     \
   }                                                                 \
   //
   BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_NEST_EMPLACE_CODE)
   #undef BOOST_CONTAINER_NEST_EMPLACE_CODE
   #undef BOOST_CONTAINER_NEST_QUICK_EMPLACE_BODY

   #endif // BOOST_NO_CXX11_VARIADIC_TEMPLATES

   //! <b>Effects</b>: Inserts a copy of x.
   //!
   //! <b>Returns</b>: An iterator to the inserted element.
   //!
   //! <b>Complexity</b>: Constant (amortized).
   BOOST_CONTAINER_FORCEINLINE iterator insert(const T& x)
   { return emplace(x); }

   //! <b>Effects</b>: Inserts a copy of x (hint ignored).
   BOOST_CONTAINER_FORCEINLINE iterator insert(const_iterator, const T& x)
   { return emplace(x); }

   //! <b>Effects</b>: Inserts x by moving.
   //!
   //! <b>Returns</b>: An iterator to the inserted element.
   BOOST_CONTAINER_FORCEINLINE iterator insert(BOOST_RV_REF(value_type) x)
   { return emplace(boost::move(x)); }

   //! <b>Effects</b>: Inserts x by moving (hint ignored).
   BOOST_CONTAINER_FORCEINLINE iterator insert(const_iterator, BOOST_RV_REF(value_type) x)
   { return emplace(boost::move(x)); }

   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Effects</b>: Inserts elements from il.
   //!
   //! <b>Complexity</b>: Linear.
   void insert(std::initializer_list<value_type> il)
   { insert(il.begin(), il.end()); }
   #endif

   //! <b>Effects</b>: Inserts copies of elements in [first, last).
   //!
   //! <b>Complexity</b>: Linear.
   template<class InpIt>
   void insert(InpIt first, InpIt last
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_convertible<InpIt, size_type>::type* = 0
      #endif
      )
   {
      priv_insert_range_copy(first, last);
   }

   //! <b>Effects</b>: Inserts n copies of x.
   //!
   //! <b>Complexity</b>: Linear.
   void insert(size_type n, const T& x)
   {
      priv_insert_n_copies(n, x);
   }

   //! <b>Effects</b>: Erases the element at position pos.
   //!
   //! <b>Returns</b>: An iterator to the element after the erased one.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE iterator erase(const_iterator pos)
   {
      block_base_pointer pbb = pos.pbb;
      int n = pos.n;
      ++pos;
      priv_erase_impl(pbb, n);
      return iterator(pos.pbb, pos.n);
   }

   //! <b>Effects</b>: Erases the element at pos without returning iterator.
   //!   Potentially faster than erase().
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE void erase_void(const_iterator pos)
   {
      priv_erase_impl(pos.pbb, pos.n);
   }

   //! <b>Effects</b>: Erases elements in [first, last).
   //!
   //! <b>Returns</b>: An iterator to the element after the erased range.
   //!
   //! <b>Complexity</b>: Linear to the range size.
   iterator erase(const_iterator first, const_iterator last)
   {
      {
         block_base_pointer pbb_first = first.pbb;
         while(first != last) {
            first = erase(first);
            if(first.pbb != pbb_first) break;
         }
      }
      block_base_pointer pbb = first.pbb;
      if(pbb != last.pbb){
         do {
            block_pointer const pb = static_cast_block_pointer(pbb);
            const mask_type m      = pb->mask;
            pbb = pb->next;
            pb->mask = 0;
            BOOST_IF_CONSTEXPR(prefetch_enabled) {
               BOOST_CONTAINER_NEST_PREFETCH(static_cast<block_type&>(*pbb).data());
            }
            size_ -= priv_destroy_all_in_nonempty_block(boost::movelib::to_raw_pointer(pb->data()), m);
            blist.unlink(pb);
            //Block is being fully emptied. If it was partial it is already in
            //the available list, so take it out first; then (re)link it at the
            //back, where all reserved (empty) blocks are kept grouped.
            if(BOOST_LIKELY(m != full))
               blist.unlink_available(pb);
            blist.link_available_at_back(pb);
         } while(pbb != last.pbb);
         first = const_iterator(pbb, nest_detail::first_in_mask(pbb->mask));
      }

      while(first != last)
         first = erase(first);
      return iterator(last.pbb, last.n);
   }

   //! <b>Effects</b>: Swaps the contents of *this and x.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   void swap(nest& x)
      BOOST_NOEXCEPT_IF(allocator_traits_type::propagate_on_container_swap::value
                                  || allocator_traits_type::is_always_equal::value)
   {
      BOOST_STATIC_CONSTEXPR bool pocs_val =
         allocator_traits_type::propagate_on_container_swap::value;
      priv_swap_impl(x, dtl::bool_<pocs_val>());
   }

   //! <b>Effects</b>: Erases all elements.
   //!
   //! <b>Complexity</b>: Linear.
   void clear() BOOST_NOEXCEPT
   {
      block_base_pointer const hdr = blist.header();
#define BOOST_CONTAINER_NEST_CLEAR_BIDIR
#if defined(BOOST_CONTAINER_NEST_CLEAR_BIDIR)
      // Dual-cursor (bidirectional) traversal. A single cold next-pointer chase
      // is address-serial: the address of block i+1 is produced by the load on
      // block i, so it runs at one full DRAM miss per block and software
      // prefetch cannot help. The main list is doubly linked, so we walk it from
      // both ends toward the middle: the front (->next) and back (->prev) chains
      // are independent, keeping two block-header misses in flight at once
      // (memory-level parallelism) which hides the latency. clear() visits every
      // block and block order is irrelevant, so this is a pure scheduling change
      // with the same end state (main list emptied, every block reserved).
      block_base_pointer f = blist.next;   // front cursor (advances via ->next)
      block_base_pointer b = blist.prev;   // back  cursor (advances via ->prev)

      while (f != hdr) {
         // Two independent dependency chains; issue both header loads up front.
         const mask_type          mf = f->mask;
         const mask_type          mb = b->mask;
         block_base_pointer const fn = f->next;   // capture advances before mutating
         block_base_pointer const bp = b->prev;
         f->mask = 0;
         b->mask = 0;

         BOOST_IF_CONSTEXPR(prefetch_enabled) {
            BOOST_CONTAINER_NEST_PREFETCH(fn);
            BOOST_CONTAINER_NEST_PREFETCH(bp);
            BOOST_IF_CONSTEXPR(!dtl::is_trivially_destructible<T>::value) {
               BOOST_CONTAINER_NEST_PREFETCH_BLOCK(fn);
               BOOST_CONTAINER_NEST_PREFETCH_BLOCK(bp);
            }
         }

         block_pointer const pf = static_cast_block_pointer(f);
         if (f == b) {                            // odd count: lone middle block
            priv_clear_block(pf, mf);
            break;
         }
         priv_clear_block(pf, mf);                                   // front
         priv_clear_block(static_cast_block_pointer(b), mb);        // back
         if (fn == b) break;                      // even count: that was the last pair
         f = fn;
         b = bp;
      }
#else
      block_base_pointer pbb = blist.next;

      while(pbb != hdr) {
         const mask_type m = pbb->mask;   //!= 0 (main-list membership)
         pbb->mask = 0;
         block_pointer const pb = static_cast_block_pointer(pbb);
         pbb = pbb->next;
         BOOST_IF_CONSTEXPR(prefetch_enabled) {
            BOOST_CONTAINER_NEST_PREFETCH(pbb->next);
            BOOST_IF_CONSTEXPR(!dtl::is_trivially_destructible<T>::value)
               BOOST_CONTAINER_NEST_PREFETCH_BLOCK(pbb);
         }
         priv_clear_block(pb, m);
      }
#endif

      blist.next = blist.prev = hdr;
      size_ = 0;
   }

   //////////////////////////////////////////////
   //
   //            hive operations
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Transfers all elements from x into *this.
   //!
   //! <b>Requires</b>: get_allocator() == x.get_allocator().
   //!
   //! <b>Complexity</b>: Linear in x.size().
   void splice(nest& x)
   {
      BOOST_ASSERT(this != &x);
      BOOST_ASSERT(priv_alloc() == x.priv_alloc());
      block_base_pointer pbb = x.blist.header()->next;
      while(pbb != x.blist.header()) {
         block_pointer pb = static_cast_block_pointer(pbb);
         pbb = pbb->next;
         if(pb->mask != full) {
            x.blist.unlink_available(pb);
            blist.link_available_at_front(pb);
         }
         x.blist.unlink(pb);
         blist.link_at_back(pb);
         --x.num_blocks;
         ++num_blocks;
         size_type const s = (size_type)dtl::popcount(pb->mask);
         x.size_ -= (size_type)s;
         size_ += (size_type)s;
      }
   }

   //! <b>Effects</b>: Transfers all elements from x into *this.
   void splice(BOOST_RV_REF(nest) x)
   { this->splice(static_cast<nest&>(x)); }

   //! <b>Effects</b>: Removes consecutive duplicate elements.
   //!
   //! <b>Returns</b>: The number of elements removed.
   //!
   //! <b>Complexity</b>: Linear.
   template<class BinaryPredicate>
   size_type unique(BinaryPredicate pred)
   {
      size_type s = size_;
      const_iterator first = cbegin();
      const_iterator last  = cend();
      while(first != last) {
         const_iterator next_it = first;
         ++next_it;
         nest_detail::unique_pred_adaptor<T, BinaryPredicate> adaptor(
            dtl::addressof(*first), pred);
         first = erase(next_it,
            nest_detail::find_if_not(next_it, last, adaptor));
      }
      return (size_type)(s - size_);
   }

   //! <b>Effects</b>: Removes consecutive equal elements.
   //!
   //! <b>Returns</b>: The number of elements removed.
   size_type unique()
   { return unique(std::equal_to<T>()); }

   #if defined(BOOST_MSVC)
   #pragma warning(push)
   #pragma warning(disable:4127) // conditional expression is constant
   #endif

   //! <b>Effects</b>: Sorts elements according to comp.
   //!
   //! <b>Complexity</b>: O(n log n).
   template<class Compare>
   void sort(Compare comp)
   {
      priv_sort_impl(comp);
   }

   #if defined(BOOST_MSVC)
   #pragma warning(pop) // C4127
   #endif

   //! <b>Effects</b>: Sorts elements in ascending order.
   //!
   //! <b>Complexity</b>: O(n log n).
   void sort()
   { sort(std::less<T>()); }

   //! <b>Effects</b>: Returns an iterator to the element pointed to by p.
   //!
   //! <b>Complexity</b>: Linear.
   iterator get_iterator(const_pointer p)
   {
      std::less<const T*> less_cmp;
      block_base_pointer pbb = blist.next;
      while(pbb != blist.header()) {
         block_pointer pb = static_cast_block_pointer(pbb);
         const T* raw_data = boost::movelib::to_raw_pointer(pb->data());
         const T* raw_p = boost::movelib::to_raw_pointer(p);
         if(!less_cmp(raw_p, raw_data) &&
             less_cmp(raw_p, raw_data + N)) {
            int const n = (int)(p - pb->data());
            BOOST_ASSERT_MSG(
               (pb->mask & ((mask_type)(1) << n)) != 0,
               "p points to an invalid element");
            return iterator(pb, n);
         }
         pbb = pbb->next;
      }
      BOOST_ASSERT_MSG(false, "p does not point into the extents of *this");
      #if defined(BOOST_ASSERT_HANDLER_IS_NORETURN)
      BOOST_UNREACHABLE_RETURN(end());
      #else
      return end();
      #endif
   }

   //! <b>Effects</b>: Returns a const_iterator to the element pointed to by p.
   //!
   //! <b>Complexity</b>: Linear.
   const_iterator get_iterator(const_pointer p) const
   {
      return const_cast<nest*>(this)->get_iterator(p);
   }

   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   private:

   template <class U, class A, class O, class P>
   friend typename nest<U, A, O>::size_type erase_if(nest<U, A, O>&, P);

   private:
   //////////////////////////////////////////////
   //
   //         private: allocator access
   //
   //////////////////////////////////////////////

   block_allocator&       al() BOOST_NOEXCEPT       { return static_cast<block_allocator&>(*this); }
   const block_allocator& al() const BOOST_NOEXCEPT { return static_cast<const block_allocator&>(*this); }

   allocator_type priv_alloc() const BOOST_NOEXCEPT
   { return allocator_type(al()); }

   //////////////////////////////////////////////
   //
   //         private: block management
   //
   //////////////////////////////////////////////

   BOOST_CONTAINER_FORCEINLINE  static block_pointer
   static_cast_block_pointer(block_base_pointer pbb) BOOST_NOEXCEPT
   {
      return block_type::static_cast_block_pointer(pbb);
   }

   void priv_allocate_block_data(block_pointer pb, dtl::bool_<false>)
   {
      BOOST_CONTAINER_TRY {
         allocator_type val_al(al());
         pb->data_ = allocator_traits_type::allocate(val_al, N);
      }
      BOOST_CONTAINER_CATCH(...) {
         block_alloc_traits::deallocate(al(), pb, 1);
         BOOST_CONTAINER_RETHROW;
      }
      BOOST_CONTAINER_CATCH_END
   }

   BOOST_CONTAINER_FORCEINLINE void priv_allocate_block_data(block_pointer, dtl::bool_<true>) BOOST_NOEXCEPT {}

   BOOST_CONTAINER_FORCEINLINE void priv_deallocate_block_data(block_pointer pb, dtl::bool_<false>) BOOST_NOEXCEPT
   {
      allocator_type val_al(al());
      allocator_traits_type::deallocate(val_al, pb->data(), N);
   }

   BOOST_CONTAINER_FORCEINLINE void priv_deallocate_block_data(block_pointer, dtl::bool_<true>) BOOST_NOEXCEPT {}

   block_pointer priv_create_new_available_block()
   {
      block_pointer pb = block_alloc_traits::allocate(al(), 1);
      pb->mask = 0;
      priv_allocate_block_data(pb, dtl::bool_<store_data_in_block>());
      blist.link_available_at_back(pb);
      ++num_blocks;
      return pb;
   }

   void priv_delete_block(block_pointer pb) BOOST_NOEXCEPT
   {
      priv_deallocate_block_data(pb, dtl::bool_<store_data_in_block>());
      block_alloc_traits::deallocate(al(), pb, 1);
   }

   BOOST_CONTAINER_FORCEINLINE block_pointer priv_retrieve_available_block(int& n)
   {
      if(BOOST_LIKELY(blist.next_available != blist.header())){
         block_pointer pb = static_cast_block_pointer(blist.next_available);
         n = dtl::unchecked_countr_one(pb->mask);
         return pb;
      }
      else {
         n = 0;
         return priv_create_new_available_block();
      }
   }

   // If the next_available block at the moment of failure is not the same as the one
   // observed before calling priv_retrieve_available_block, a new (and now empty) block
   // was freshly allocated for the failed construction: free it to restore capacity.
   BOOST_CONTAINER_NOINLINE void priv_restore_capacity_on_throw(block_base_pointer pbb_prev) BOOST_NOEXCEPT
   {
      if(blist.next_available != pbb_prev) {
         block_pointer const pb_new = static_cast_block_pointer(blist.next_available);
         blist.unlink_available(pb_new);
         priv_delete_block(pb_new);
         --num_blocks;
      }
   }

   // The try/catch lives here (not inline in emplace) on purpose: MSVC will not
   // inline a function that contains its own EH scope, so keeping emplace
   // EH-clean lets it be inlined into the caller's insert loop. This is a plain
   // (non-FORCEINLINE) helper so the EH scope stays out of emplace.
   #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
   template<class ...Args>
   inline void priv_construct_or_restore_capacity
      (T* p, block_base_pointer pbb_prev, BOOST_FWD_REF(Args)... args)
   {
      BOOST_CONTAINER_TRY{
         block_alloc_traits::construct(al(), p, boost::forward<Args>(args)...);
      }
      BOOST_CONTAINER_CATCH(...){
         this->priv_restore_capacity_on_throw(pbb_prev);
         BOOST_CONTAINER_RETHROW;
      }
      BOOST_CONTAINER_CATCH_END
   }
   #else
   #define BOOST_CONTAINER_NEST_CONSTRUCT_OR_RESTORE_CODE(N) \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N \
   inline void priv_construct_or_restore_capacity                     \
      (T* p, block_base_pointer pbb_prev BOOST_MOVE_I##N BOOST_MOVE_UREF##N) \
   {                                                                  \
      BOOST_CONTAINER_TRY{                                                      \
         block_alloc_traits::construct(al(), p BOOST_MOVE_I##N BOOST_MOVE_FWD##N); \
      }                                                               \
      BOOST_CONTAINER_CATCH(...){                                               \
         this->priv_restore_capacity_on_throw(pbb_prev);              \
         BOOST_CONTAINER_RETHROW;                                               \
      }                                                               \
      BOOST_CONTAINER_CATCH_END                                                 \
   }                                                                  \
   //
   BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_NEST_CONSTRUCT_OR_RESTORE_CODE)
   #undef BOOST_CONTAINER_NEST_CONSTRUCT_OR_RESTORE_CODE
   #endif

   //////////////////////////////////////////////
   //
   //   private: destruction helpers
   //
   //////////////////////////////////////////////

   BOOST_CONTAINER_FORCEINLINE size_type priv_destroy_all_in_nonempty_block(T* data, mask_type m) BOOST_NOEXCEPT
   {
      BOOST_ASSERT(m != 0);
      const size_type r = (size_type)dtl::popcount(m); (void)data;
      BOOST_IF_CONSTEXPR(!dtl::is_trivially_destructible<T>::value) {
         BOOST_CONTAINER_UNROLL(4)
         do {
            int n = nest_detail::first_in_mask(m);
            block_alloc_traits::destroy(al(), data + n);
            m &= m - 1;
         } while(m);
      }
      return r;
   }

   BOOST_CONTAINER_FORCEINLINE size_type priv_destroy_all_in_full_block(T* data, mask_type mask) BOOST_NOEXCEPT
   {
      (void)data; (void)mask;
      BOOST_ASSERT(mask == full);
      BOOST_IF_CONSTEXPR(!dtl::is_trivially_destructible<T>::value) {
         (void)mask;
         T* const data_end = data + N;

         BOOST_CONTAINER_UNROLL(4)
         for (; data != data_end; ++data) {
            block_alloc_traits::destroy(al(), data);
         }
      }
      return (size_type)N;
   }

   // Per-block work shared by clear()'s traversal variants: destroy the live
   // elements of 'pb' (using the captured pre-clear mask 'm') and, for a full
   // block (which is not on the available list), append it there. The caller is
   // responsible for resetting pb->mask to 0. Block order is irrelevant here.
   BOOST_CONTAINER_FORCEINLINE void priv_clear_block(block_pointer pb, mask_type m) BOOST_NOEXCEPT
   {
      T* const pd = boost::movelib::to_raw_pointer(pb->data());
      if (m == full) {
         //Full blocks are not on the available list; add them. Partial blocks
         //are already there. Every block becomes reserved (mask == 0).
         priv_destroy_all_in_full_block(pd, m);
         blist.link_available_at_back(pb);
      }
      else
         priv_destroy_all_in_nonempty_block(pd, m);
   }

   // Per-block work shared by priv_reset()'s traversal variants: destroy the
   // live elements of a main-list block (mask 'm' != 0; partial or full) and
   // free it. Empty/reserved blocks carry no elements and are freed directly
   // with priv_delete_block instead. priv_delete_block does not unlink, so the
   // caller may free blocks in any order provided list advances were captured
   // from still-live nodes beforehand.
   BOOST_CONTAINER_FORCEINLINE void priv_reset_block(block_pointer pb, mask_type m) BOOST_NOEXCEPT
   {
      BOOST_ASSERT(m != 0);
      T* const pd = boost::movelib::to_raw_pointer(pb->data());
      if (m == full)
         priv_destroy_all_in_full_block(pd, m);
      else
         priv_destroy_all_in_nonempty_block(pd, m);
      priv_delete_block(pb);
   }

   //////////////////////////////////////////////
   //
   //   private: reset (destroy all + free)
   //
   //////////////////////////////////////////////

   void priv_reset() BOOST_NOEXCEPT
   {
      // Phase A sequential (empties), then a bidirectional main-list
      // walk for partial+full blocks. The single forward next-pointer chase of
      // variant 0 is address-serial (one DRAM miss per block, no MLP); walking
      // the doubly-linked main list from both ends gives two independent
      // header-miss streams. NOTE: because reset frees as it goes, neighbor data
      // prefetch must not dereference an already-freed block, so the next-pair
      // data prefetch is issued only on non-terminal iterations (when fn/bp are
      // strictly inside the unprocessed (f, b) range and hence still alive).
      block_base_pointer const hdr = blist.header();

      // A) empty (reserved) blocks, from the back of the available list
      block_base_pointer pbb = hdr->prev_available;
      BOOST_CONTAINER_UNROLL(4)
      while(pbb != hdr && pbb->mask == 0) {
         block_pointer const pb = static_cast_block_pointer(pbb);
         pbb = pbb->prev_available;

         BOOST_IF_CONSTEXPR(prefetch_enabled)
            BOOST_CONTAINER_NEST_PREFETCH(pbb);

         priv_delete_block(pb);
      }

      // B) partial + full blocks, from both ends of the main list
      block_base_pointer f = blist.next;
      block_base_pointer b = blist.prev;
      while(f != hdr) {
         const mask_type          mf = f->mask;
         const mask_type          mb = b->mask;
         block_base_pointer const fn = f->next;
         block_base_pointer const bp = b->prev;
         block_pointer      const pf = static_cast_block_pointer(f);

         if(f == b) {                           // odd count: lone middle block
            priv_reset_block(pf, mf);
            break;
         }

         block_pointer const pbk       = static_cast_block_pointer(b);
         const bool          last_pair = (fn == b);   // even count: final pair

         BOOST_IF_CONSTEXPR(prefetch_enabled){
            if(!last_pair){                     // fn, bp strictly inside (f,b): alive
               BOOST_CONTAINER_NEST_PREFETCH(fn);
               BOOST_CONTAINER_NEST_PREFETCH(bp);
               BOOST_IF_CONSTEXPR(!dtl::is_trivially_destructible<T>::value){
                  BOOST_CONTAINER_NEST_PREFETCH_BLOCK(fn);
                  BOOST_CONTAINER_NEST_PREFETCH_BLOCK(bp);
               }
            }
         }

         priv_reset_block(pf, mf);
         priv_reset_block(pbk, mb);
         if(last_pair) break;
         f = fn;
         b = bp;
      }

      blist.reset();
      num_blocks = 0;
      size_ = 0;
   }

   //////////////////////////////////////////////
   //
   //   private: erase implementation
   //
   //////////////////////////////////////////////

   BOOST_CONTAINER_FORCEINLINE void priv_erase_impl(block_base_pointer pbb, int n) BOOST_NOEXCEPT
   {
      block_pointer const pb = static_cast_block_pointer(pbb);
      const mask_type m = pb->mask; //Load constant before any "opaque" operations like "destroy"
      const mask_type new_mask = m & ~((mask_type)(1) << n);
      pb->mask = new_mask;

      block_alloc_traits::destroy(al(), boost::movelib::to_raw_pointer(pb->data()) + n);

      if(BOOST_UNLIKELY(m == full))
         blist.link_available_at_front(pb);

      //With N > 1 both conditions can't be true
      else if(BOOST_UNLIKELY(new_mask == 0)) {
         //Block just became empty: take it out of the main list and move it
         //to the back of the available list so reserved blocks stay grouped
         //there (keeps trim_capacity linear on the number of reserved blocks).
         blist.unlink(pb);
         blist.unlink_available(pb);
         blist.link_available_at_back(pb);
      }

      --size_;
   }

   //////////////////////////////////////////////
   //
   //   private: range insert / assign
   //
   //////////////////////////////////////////////

   template<class InpIt>
   void priv_insert_range_copy(InpIt first, InpIt last)
   {
      while(first != last) {
         int  n;
         block_pointer pb = priv_retrieve_available_block(n);
         for(; ; ) {
            block_alloc_traits::construct(
               al(), boost::movelib::to_raw_pointer(pb->data() + n), *first);
            ++first;
            ++size_;
            if(BOOST_UNLIKELY(pb->mask == 0)) blist.link_at_back(pb);
            pb->mask |= pb->mask + 1;
            if(pb->mask == full){
               blist.unlink_available(pb);
               break;
            }
            else if(first == last) return;
            n = dtl::unchecked_countr_one(pb->mask);
         }
      }
   }

   template<class InpIt>
   void priv_insert_range_move(InpIt first, InpIt last)
   {
      this->priv_insert_range_copy( boost::make_move_iterator(first)
                                  , boost::make_move_iterator(last));
   }

   void priv_insert_n_copies(size_type count, const T& x)
   {
      typedef constant_iterator<T> const_it;
      this->priv_insert_range_copy(const_it(x, count), const_it());
   }

   void priv_insert_n_default(size_type count)
   {
      typedef value_init_construct_iterator<value_type> value_init_iterator;
      this->priv_insert_range_copy(value_init_iterator(count), value_init_iterator());
   }

   template<class InpIt>
   void priv_range_assign(InpIt first, InpIt last)
   {
      block_base_pointer pbb = blist.next;
      int  n = -1;
      if(first != last) {
         for(; pbb != blist.header(); pbb = pbb->next) {
            n = 0;
            block_pointer pb = static_cast_block_pointer(pbb);
            for(mask_type bit = 1; bit; bit <<= 1, ++n) {
               if(pb->mask & bit) {
                  pb->data()[n] = *first;
                  ++first;
               }
               else {
                  block_alloc_traits::construct(
                     al(), boost::movelib::to_raw_pointer(pb->data() + n), *first);
                  ++first;
                  ++size_;
                  pb->mask |= bit;
                  if(pb->mask == full) blist.unlink_available(pb);
               }
               if(first == last) goto exit;
            }
         }
      exit: ;
      }
      if(first != last) {
         priv_insert_range_copy(first, last);
      }
      else {
         erase( (n == -1) ? const_iterator(pbb, nest_detail::first_in_mask(pbb->mask)) : ++const_iterator(pbb, n)
              , cend());
      }
   }

   void priv_assign_n(size_type count, const T& val)
   {
      typedef constant_iterator<T> const_it;
      this->priv_range_assign(const_it(val, count), const_it());
   }

   //////////////////////////////////////////////
   //
   //   private: move assign
   //
   //////////////////////////////////////////////

   void priv_move_assign(nest& x, dtl::true_type /* can transfer */)
   {
      dtl::bool_<allocator_traits_type::
         propagate_on_container_move_assignment::value> pocma;
      priv_reset();
      nest_detail::move_assign_if(pocma, al(), x.al());
      blist = boost::move(x.blist);
      num_blocks = x.num_blocks;
      size_ = x.size_;
      x.num_blocks = 0;
      x.size_ = 0;
   }

   void priv_move_assign(nest& x, dtl::false_type /* maybe move data */)
   {
      if(al() == x.al()) {
         priv_move_assign(x, dtl::true_type());
      }
      else {
         // Move-assign element by element
         priv_move_assign_elements(x);
         x.priv_reset();
      }
   }

   void priv_move_assign_elements(nest& x)
   {
      this->priv_range_assign( boost::make_move_iterator(x.begin())
                             , boost::make_move_iterator(x.end()) );
      x.clear();
   }

   //////////////////////////////////////////////
   //
   //   private: swap
   //
   //////////////////////////////////////////////

   void priv_swap_impl(nest& x, dtl::true_type /* propagate alloc */)
   {
      boost::adl_move_swap(al(), x.al());
      boost::adl_move_swap(blist, x.blist);
      boost::adl_move_swap(num_blocks, x.num_blocks);
      boost::adl_move_swap(size_, x.size_);
   }

   void priv_swap_impl(nest& x, dtl::false_type /* don't propagate */)
   {
      BOOST_ASSERT(al() == x.al());
      boost::adl_move_swap(blist, x.blist);
      boost::adl_move_swap(num_blocks, x.num_blocks);
      boost::adl_move_swap(size_, x.size_);
   }

   //////////////////////////////////////////////
   //
   //   private: sort
   //
   //////////////////////////////////////////////

   struct sort_proxy
   {
      T*        p;
      size_type n;
   };

   template<class Compare>
   void priv_sort_impl(Compare comp)
   {
      // Try transfer_sort for small element types
      BOOST_IF_CONSTEXPR(sizeof(T) <= sizeof(sort_proxy)) {
         if(priv_transfer_sort(comp)) return;
      }
      else {
         BOOST_STATIC_CONSTEXPR std::size_t memory_threshold = 2 * 1024 * 1024;
         if((std::size_t)size_ * sizeof(sort_proxy) <= memory_threshold) {
            if(priv_proxy_sort(comp)) return;
         }
      }
      priv_compact_sort(comp);
   }

   template<class Compare>
   bool priv_transfer_sort(Compare comp)
   {
      if(size_ > 1) {
         nest_detail::buffer<T, block_allocator> buf(size_, al());
         if(!buf.data) return false;

         // Move all elements to buffer
         priv_visit_all_move_to_buffer(buf);
         std::sort(buf.begin(), buf.end(), comp);
         // Move sorted elements back
         priv_visit_all_move_from_buffer(buf);
      }
      return true;
   }

   void priv_visit_all_move_to_buffer(nest_detail::buffer<T, block_allocator>& buf)
   {
      block_base_pointer pbb = blist.next;
      while(pbb != blist.header()) {
         block_pointer pb = static_cast_block_pointer(pbb);
         pbb = pbb->next;
         mask_type m = pb->mask;
         while(m) {
            int n = nest_detail::first_in_mask(m);
            buf.push_back_move(pb->data()[n]);
            m &= m - 1;
         }
      }
   }

   void priv_visit_all_move_from_buffer(nest_detail::buffer<T, block_allocator>& buf)
   {
      block_base_pointer pbb = blist.next;
      while(pbb != blist.header()) {
         block_pointer pb = static_cast_block_pointer(pbb);
         pbb = pbb->next;
         mask_type m = pb->mask;
         while(m) {
            int n = nest_detail::first_in_mask(m);
            pb->data()[n] = boost::move(*buf.begin());
            buf.erase_front();
            m &= m - 1;
         }
      }
   }

   template<class Compare>
   bool priv_proxy_sort(Compare comp)
   {
      if(size_ > 1) {
         void* raw = ::operator new(size_ * sizeof(sort_proxy), std::nothrow);
         if(!raw) return false;
         nest_detail::raw_memory_holder holder(raw);
         sort_proxy* proxies = static_cast<sort_proxy*>(raw);

         size_type i = 0;
         block_base_pointer pbb = blist.next;
         while(pbb != blist.header()) {
            block_pointer pb = static_cast_block_pointer(pbb);
            pbb = pbb->next;
            mask_type m = pb->mask;
            while(m) {
               int n = nest_detail::first_in_mask(m);
               proxies[i].p = boost::movelib::to_raw_pointer(pb->data() + n);
               proxies[i].n = i;
               ++i;
               m &= m - 1;
            }
         }

         nest_detail::sort_proxy_comparator<T, Compare> proxy_comp(comp);
         std::sort(proxies, proxies + size_, proxy_comp);

         // Rearrange elements according to sorted proxy order
         for(i = 0; i < size_; ++i) {
            if(proxies[i].n != i) {
               T x = boost::move(*(proxies[i].p));
               size_type j = i;
               do {
                  size_type k = proxies[j].n;
                  *(proxies[j].p) = boost::move(*proxies[k].p);
                  proxies[j].n = j;
                  j = k;
               } while(proxies[j].n != i);
               *(proxies[j].p) = boost::move(x);
               proxies[j].n = j;
            }
         }
      }
      return true;
   }

   template<class Compare>
   void priv_compact_sort(Compare comp)
   {
      typedef nest_detail::sort_iterator<T, N> sort_iter;

      if(size_ > 1) {
         std::size_t nblocks = (std::size_t)((size_ + N - 1) / N);
         void* raw = ::operator new(nblocks * sizeof(T*));
         nest_detail::raw_memory_holder holder(raw);
         T** ptrs = static_cast<T**>(raw);

         std::size_t idx = 0;
         priv_compact_with_tracking(ptrs, idx);
         BOOST_ASSERT(idx == nblocks);

         std::sort( sort_iter(ptrs, 0)
                  , sort_iter(ptrs, (std::ptrdiff_t)size_), comp);
      }
   }

   //////////////////////////////////////////////
   //
   //   private: compact
   //
   //////////////////////////////////////////////

   void priv_compact_noop_track()
   {
      block_base_pointer pbbx = blist.next;
      while(pbbx != blist.header()) {
         block_pointer pbx = static_cast_block_pointer(pbbx);
         block_base_pointer pbby = pbbx->next;
         if(pbx->mask != full) {
            do {
               if(pbby->mask == full) {
                  do {
                     // skip full blocks (noop track)
                     pbby = pbby->next;
                  } while(pbby->mask == full);
                  blist.unlink(pbx);
                  blist.link_before(pbx, static_cast_block_pointer(pbby));
               }
               if(pbby == blist.header()) {
                  priv_compact_single(pbx);
                  return;
               }
               else {
                  block_pointer pby = static_cast_block_pointer(pbby);
                  priv_compact_pair(pbx, pby);
                  if(pby->mask == 0) {
                     pbby = pby->next;
                     //pby was non-full (hence in the available list) and has
                     //just been drained empty. Move it to the back so all
                     //reserved blocks stay grouped there for trim_capacity.
                     blist.unlink(pby);
                     blist.unlink_available(pby);
                     blist.link_available_at_back(pby);
                  }
               }
            } while(pbx->mask != full);
            blist.unlink_available(pbx);
         }
         pbbx = pbby;
      }
   }

   void priv_compact_with_tracking(T** ptrs, std::size_t& idx)
   {
      block_base_pointer pbbx = blist.next;
      while(pbbx != blist.header()) {
         block_pointer pbx = static_cast_block_pointer(pbbx);
         block_base_pointer pbby = pbbx->next;
         if(pbx->mask != full) {
            do {
               if(pbby->mask == full) {
                  do {
                     ptrs[idx++] = boost::movelib::to_raw_pointer(
                        static_cast_block_pointer(pbby)->data());
                     pbby = pbby->next;
                  } while(pbby->mask == full);
                  blist.unlink(pbx);
                  blist.link_before(pbx, static_cast_block_pointer(pbby));
               }
               if(pbby == blist.header()) {
                  priv_compact_single(pbx);
                  ptrs[idx++] = boost::movelib::to_raw_pointer(pbx->data());
                  return;
               }
               else {
                  block_pointer pby = static_cast_block_pointer(pbby);
                  priv_compact_pair(pbx, pby);
                  if(pby->mask == 0) {
                     pbby = pby->next;
                     //pby was non-full (hence in the available list) and has
                     //just been drained empty. Move it to the back so all
                     //reserved blocks stay grouped there for trim_capacity.
                     blist.unlink(pby);
                     blist.unlink_available(pby);
                     blist.link_available_at_back(pby);
                  }
               }
            } while(pbx->mask != full);
            blist.unlink_available(pbx);
         }
         ptrs[idx++] = boost::movelib::to_raw_pointer(pbx->data());
         pbbx = pbby;
      }
   }

   void priv_compact_pair(block_pointer pbx, block_pointer pby)
   {
      std::size_t cx = static_cast<std::size_t>(dtl::popcount(pbx->mask));
      std::size_t cy = static_cast<std::size_t>(dtl::popcount(pby->mask));
      if(cx < cy) {
         boost::adl_move_swap(cx, cy);
         nest_detail::swap_payload(*pbx, *pby);
      }
      std::size_t c = (std::min)(N - cx, cy);
      while(c--) {
         std::size_t n = static_cast<std::size_t>(dtl::unchecked_countr_one(pbx->mask));
         std::size_t m = static_cast<std::size_t>(nest_detail::last_in_mask(pby->mask));
         block_alloc_traits::construct(
            al(), boost::movelib::to_raw_pointer(pbx->data() + n),
            boost::move(pby->data()[m]));
         block_alloc_traits::destroy(
            al(), boost::movelib::to_raw_pointer(pby->data() + m));
         pbx->mask |= pbx->mask + 1;
         pby->mask &= ~((mask_type)(1) << m);
      }
   }

   void priv_compact_single(block_pointer pb)
   {
      for(; ;) {
         std::size_t n = (std::size_t)dtl::unchecked_countr_one(pb->mask);
         std::size_t m = static_cast<std::size_t>(nest_detail::last_in_mask(pb->mask));
         if(n > m) return;
         block_alloc_traits::construct(
            al(), boost::movelib::to_raw_pointer(pb->data() + n),
            boost::move(pb->data()[m]));
         block_alloc_traits::destroy(
            al(), boost::movelib::to_raw_pointer(pb->data() + m));
         pb->mask |= pb->mask + 1;
         pb->mask &= ~((mask_type)(1) << m);
      }
   }

   //////////////////////////////////////////////
   //
   //         private: erase_if impl
   //
   //////////////////////////////////////////////

   template<class Predicate>
   size_type priv_erase_if(Predicate pred)
   {
      size_type s = size_;
      block_base_pointer pbb = blist.next;
      while(pbb != blist.header()) {
         block_pointer pb = static_cast_block_pointer(pbb);
         pbb = pb->next;
         BOOST_IF_CONSTEXPR(prefetch_enabled) {
            BOOST_CONTAINER_NEST_PREFETCH(static_cast<block_type&>(*pbb).data());
         }
         mask_type m = pb->mask;
         BOOST_CONTAINER_UNROLL(4)
         do {
            int n = nest_detail::first_in_mask(m);
            if(pred(pb->data()[n])) priv_erase_impl(pb, n);
            m &= m - 1;
         } while(m);
      }
      return (size_type)(s - size_);
   }

   //////////////////////////////////////////////
   //
   //         private: data members
   //
   //////////////////////////////////////////////

   block_base blist;
   size_type  num_blocks;
   size_type  size_;

   #endif // BOOST_CONTAINER_DOXYGEN_INVOKED
};

//////////////////////////////////////////////
//
//       free functions
//
//////////////////////////////////////////////

//! <b>Effects</b>: Swaps x and y.
template<class T, class Allocator, class Options>
inline void swap(nest<T, Allocator, Options>& x, nest<T, Allocator, Options>& y)
   BOOST_NOEXCEPT_IF(BOOST_NOEXCEPT(x.swap(y)))
{
   x.swap(y);
}

//! <b>Effects</b>: Erases all elements for which pred returns true.
//!
//! <b>Returns</b>: The number of erased elements.
template<class T, class Allocator, class Options, class Predicate>
typename nest<T, Allocator, Options>::size_type
erase_if(nest<T, Allocator, Options>& x, Predicate pred)
{
   return x.priv_erase_if(pred);
}

//! <b>Effects</b>: Erases all elements equal to value.
//!
//! <b>Returns</b>: The number of erased elements.
template<class T, class Allocator, class Options>
typename nest<T, Allocator, Options>::size_type
erase(nest<T, Allocator, Options>& x, const T& value)
{
   return boost::container::erase_if(x, equal_to_value<T>(value));
}

//! <b>Effects</b>: Calls f(*it) for each iterator it in [first, last) until
//!   f(*it) returns false.
//!
//! <b>Returns</b>: A std::pair containing the iterator where visitation
//!   stopped (or last if all calls returned true) and the (possibly moved)
//!   functor f.
//!
//! <b>Complexity</b>: Linear in the distance between first and last.
template<class ValuePointer, bool StoreDataInBlock, bool Prefetch, class F>
std::pair< nest_iterator<ValuePointer, StoreDataInBlock, Prefetch>, F >
   for_each_while
      ( nest_iterator<ValuePointer, StoreDataInBlock, Prefetch> first
      , nest_iterator<ValuePointer, StoreDataInBlock, Prefetch> last
      , F f)
{
   typedef nest_iterator<ValuePointer, StoreDataInBlock, Prefetch> iter_t;
   typedef typename iter_t::block_base_pointer                     bbp_t;
   typedef typename iter_t::block_type                             block_t;
   typedef typename iter_t::nonconst_pointer                       value_ptr_t;
   typedef typename iter_t::mask_type                              mask_t;
   typedef std::pair<iter_t, F>                                    result_type;

   BOOST_STATIC_CONSTEXPR mask_t full = iter_t::full;

   if(BOOST_UNLIKELY(first == last))
      return result_type(last, f);

   bbp_t       pbb      = first.pbb;
   bbp_t const last_pbb = last.pbb;
   int const   last_n   = last.n;
   mask_t      m        = pbb->mask & (full << first.n);

   for(; ;) {
      block_t& pbn = static_cast<block_t&>(*pbb->next);
      BOOST_IF_CONSTEXPR(Prefetch)
         BOOST_CONTAINER_NEST_PREFETCH(&pbn.mask);
      //Mask the mask for the last block
      const mask_t is_last = mask_t(pbb == last_pbb);
      m &= (is_last << last_n) - mask_t(1);

      //Next block prefetch
      BOOST_IF_CONSTEXPR(Prefetch) {
         BOOST_CONTAINER_NEST_PREFETCH(&pbn.next->next);
         const int next_n = nest_detail::first_in_mask(pbn.mask);
         BOOST_CONTAINER_NEST_PREFETCH(pbn.data() + next_n);
      }

      //Mask can become zero if the last iterator is in the 0 position
      value_ptr_t const pd = block_t::static_cast_block_pointer(pbb)->data();

      BOOST_CONTAINER_UNROLL(4)
      while(m) {
         int n = nest_detail::first_in_mask(m);
         if (!f(pd[n]))
            return result_type(iter_t(pbb, n), f);
         m &= m - 1;
      }

      if(is_last)
         return result_type(last, f);

      pbb = pbb->next;
      m   = pbb->mask;
   }
}

//! <b>Effects</b>: Calls f(*it) for each iterator it in [first, last).
//!
//! <b>Returns</b>: The (possibly moved) functor f.
//!
//! <b>Complexity</b>: Linear in the distance between first and last.
template<class ValuePointer, bool StoreDataInBlock, bool Prefetch, class F>
F for_each
   ( nest_iterator<ValuePointer, StoreDataInBlock, Prefetch> first
   , nest_iterator<ValuePointer, StoreDataInBlock, Prefetch> last
   , F f)
{
   typedef typename boost::intrusive::pointer_traits<ValuePointer>::element_type
      element_t;
   nest_detail::for_each_adaptor<F, element_t> adaptor(f);
   (void)boost::container::for_each_while(first, last, adaptor);
   return f;
}

//! <b>Effects</b>: Calls f(x) for each element x in x.
//!
//! <b>Returns</b>: The (possibly moved) functor f.
//!
//! <b>Complexity</b>: Linear in the number of elements.
template<class T, class Allocator, class Options, class F>
F for_each(nest<T, Allocator, Options>& x, F f)
{
   nest_detail::for_each_adaptor<F, T> adaptor(f);
   (void)boost::container::for_each_while(x.begin(), x.end(), adaptor);
   return f;
}

//! <b>Effects</b>: Calls f(x) for each const element x in x.
//!
//! <b>Returns</b>: The (possibly moved) functor f.
//!
//! <b>Complexity</b>: Linear in the number of elements.
template<class T, class Allocator, class Options, class F>
F for_each(const nest<T, Allocator, Options>& x, F f)
{
   nest_detail::for_each_adaptor<F, const T> adaptor(f);
   (void)boost::container::for_each_while(x.begin(), x.end(), adaptor);
   return f;
}

//! <b>Effects</b>: Calls f(x) for each element x of x until f(x) returns false.
//!
//! <b>Returns</b>: A std::pair with the iterator where visitation stopped (or
//!   end() if all calls returned true) and the (possibly moved) functor f.
//!
//! <b>Complexity</b>: Linear in the number of elements.
template<class T, class Allocator, class Options, class F>
std::pair<typename nest<T, Allocator, Options>::iterator, F>
   for_each_while(nest<T, Allocator, Options>& x, F f)
{
   typedef typename nest<T, Allocator, Options>::iterator iter_t;
   nest_detail::ref_predicate_adaptor<F, T> adaptor(f);
   iter_t const it = boost::container::for_each_while(
      x.begin(), x.end(), adaptor).first;
   return std::pair<iter_t, F>(it, f);
}

//! <b>Effects</b>: Calls f(x) for each const element x of x until f(x)
//!   returns false.
//!
//! <b>Returns</b>: A std::pair with the const_iterator where visitation
//!   stopped (or end() if all calls returned true) and the (possibly moved)
//!   functor f.
//!
//! <b>Complexity</b>: Linear in the number of elements.
template<class T, class Allocator, class Options, class F>
std::pair<typename nest<T, Allocator, Options>::const_iterator, F>
   for_each_while(const nest<T, Allocator, Options>& x, F f)
{
   typedef typename nest<T, Allocator, Options>::const_iterator citer_t;
   nest_detail::ref_predicate_adaptor<F, const T> adaptor(f);
   citer_t const it = boost::container::for_each_while(
      x.begin(), x.end(), adaptor).first;
   return std::pair<citer_t, F>(it, f);
}

#ifndef BOOST_CONTAINER_NO_CXX17_CTAD

//! <b>Deduction guide</b>: allows a `nest` to be constructed from the iterator range
//! <code>[first, last)</code>, deducing the element type from the value type of
//! `InpIt` and optionally taking the allocator type from the supplied allocator.
template<
   class InpIt,
   class Allocator = void
>
nest(InpIt, InpIt, Allocator = Allocator())
   -> nest<
      typename iterator_traits<InpIt>::value_type,
      Allocator,
      void>;

#endif

}} // namespace boost::container

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_EXPERIMENTAL_NEST_HPP
