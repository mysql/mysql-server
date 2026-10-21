//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Joaquin M Lopez Munoz 2025-2026.
// (C) Copyright Ion Gaztanaga 2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_HUB_HPP
#define BOOST_CONTAINER_HUB_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

//container
#include <boost/container/container_fwd.hpp>
#include <boost/container/detail/bit_utilities.hpp>
#include <boost/container/detail/range_utils.hpp>   //from_range_t / from_range

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <boost/container/allocator_traits.hpp>
#include <boost/container/new_allocator.hpp>        //default allocator when Allocator = void
#include <boost/container/throw_exception.hpp>
#include <boost/container/detail/addressof.hpp>     //dtl::addressof (avoids <memory>)
#include <boost/container/detail/aligned_allocation.hpp> //portable (over)aligned nothrow alloc
#include <boost/container/detail/std_fwd.hpp>       //forward declares std::allocator
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/move/detail/to_raw_pointer.hpp>
#include <boost/move/unique_ptr.hpp>                //movelib::unique_ptr (avoids <memory>)
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>

#ifndef BOOST_NO_CXX17_HDR_MEMORY_RESOURCE
#include <memory_resource>
#endif

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

#if defined(BOOST_NO_CXX20_HDR_CONCEPTS) || defined(BOOST_NO_CXX20_HDR_RANGES)
#define BOOST_CONTAINER_HUB_NO_RANGES
#elif BOOST_WORKAROUND(BOOST_CLANG_VERSION, < 170100) && \
         defined(BOOST_LIBSTDCXX_VERSION)
//https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109647
//https://github.com/llvm/llvm-project/issues/49620
#define BOOST_CONTAINER_HUB_NO_RANGES
#endif

#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
//<iterator> (std::input_iterator, std::iter_value_t/iter_reference_t) is already
//included unconditionally below; only <concepts> (std::convertible_to) is extra.
#include <concepts>
#endif

//Software prefetch hint accepting a (possibly fancy) pointer: convert to a raw
//pointer and delegate to BOOST_CONTAINER_PREFETCH (workaround.hpp), which holds
//the per-compiler/-arch implementation (and is a macro, not a function, because
//of https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109985).
#define BOOST_CONTAINER_HUB_PREFETCH(p) \
   BOOST_CONTAINER_PREFETCH(boost::movelib::to_raw_pointer(p))

#define BOOST_CONTAINER_HUB_PREFETCH_BLOCK(pbb, Block) \
do{                                                    \
   auto p0 = &static_cast<Block&>(*(pbb));             \
   BOOST_CONTAINER_HUB_PREFETCH(p0->data());           \
} while(0)

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) //marked as __forceinline not inlined
#endif

#endif   //BOOST_CONTAINER_DOXYGEN_INVOKED

namespace boost {

namespace container {

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

//Default argument for Allocator is provided by container_fwd.hpp
template<typename T, typename Allocator>
class hub;

template<typename T, typename Allocator, typename Predicate>
typename hub<T, Allocator>::size_type erase_if(hub<T, Allocator>&, Predicate);

namespace hub_detail {

template<typename ValuePointer> class iterator;

}

template<typename T, typename Allocator, typename F>
F for_each(hub<T, Allocator>&, F);

template<typename T, typename Allocator, typename F>
std::pair<typename hub<T, Allocator>::iterator, F> for_each_while(
   hub<T, Allocator>&, F);

template<typename T, typename Allocator, typename F>
std::pair<typename hub<T, Allocator>::const_iterator, F> for_each_while(
   const hub<T, Allocator>&, F);

template<typename ValuePtr, typename F>
std::pair<hub_detail::iterator<ValuePtr>, F> for_each_while(
   hub_detail::iterator<ValuePtr>, hub_detail::iterator<ValuePtr>, F);

namespace hub_detail {

//Shared bit helpers live in boost::container::dtl (detail/bit_utilities.hpp).

//Use Boost.Intrusive's pointer_traits (modeled on std::pointer_traits) so the
//header does not depend on Boost.Core.
using boost::intrusive::pointer_traits;

//Thin alias templates over boost::container::allocator_traits replacing the
//former boost::core::allocator_access helpers. The propagate_on_* / is_always_equal
//members of boost::container::allocator_traits are Boost integral constants; wrap
//their value in std::integral_constant so the std::true_type/std::false_type-based
//tag-dispatch helpers (swap_if, if_constexpr, copy_assign_if, ...) keep matching.
template<class A>
using pocca_t = std::integral_constant<bool,
   allocator_traits<A>::propagate_on_container_copy_assignment::value>;
template<class A>
using pocma_t = std::integral_constant<bool,
   allocator_traits<A>::propagate_on_container_move_assignment::value>;
template<class A>
using pocs_t  = std::integral_constant<bool,
   allocator_traits<A>::propagate_on_container_swap::value>;
template<class A>
using is_always_equal_t = std::integral_constant<bool,
   allocator_traits<A>::is_always_equal::value>;
template<class A, class U>
using rebind_alloc_t = typename allocator_traits<A>::template portable_rebind_alloc<U>::type;

template<typename Pointer, typename T>
using pointer_rebind_t = 
   typename pointer_traits<Pointer>::template rebind<T>;

template<typename VoidPointer>
struct block_base
{
   using pointer = pointer_rebind_t<VoidPointer, block_base>;
   using const_pointer = pointer_rebind_t<VoidPointer, const block_base>;
   using mask_type = std::uint64_t;

   static constexpr int N = 64;
   static constexpr mask_type full = (mask_type)(-1);

   static pointer pointer_to(block_base& x) noexcept
   {
      return pointer_traits<pointer>::pointer_to(x);
   }

   static const_pointer pointer_to(const block_base& x) noexcept
   {
      return pointer_traits<const_pointer>::pointer_to(x);
   }

   BOOST_CONTAINER_FORCEINLINE void link_available_before(pointer p) noexcept
   {
      next_available = p;
      prev_available = p->prev_available;
      next_available->prev_available = pointer_to(*this);
      prev_available->next_available = pointer_to(*this);
   }

   BOOST_CONTAINER_FORCEINLINE void link_available_after(pointer p) noexcept
   {
      prev_available = p;
      next_available = p->next_available;
      next_available->prev_available = pointer_to(*this);
      prev_available->next_available = pointer_to(*this);
   }

   BOOST_CONTAINER_FORCEINLINE void unlink_available() noexcept
   {
      prev_available->next_available = next_available;
      next_available->prev_available = prev_available;
   }

   BOOST_CONTAINER_FORCEINLINE void link_before(pointer p) noexcept
   {
      next = p;
      prev = p->prev;
      next->prev = pointer_to(*this);
      prev->next = pointer_to(*this);
   }

   BOOST_CONTAINER_FORCEINLINE void unlink() noexcept
   {
      prev->next = next;
      next->prev = prev;
   }

   pointer   prev_available,
             next_available,
             prev,
             next;
   mask_type mask;
};

template<typename ValuePointer>
struct block: block_base<pointer_rebind_t<ValuePointer, void>>
{
   using super = block_base<pointer_rebind_t<ValuePointer, void>>;
   using pointer = pointer_rebind_t<ValuePointer, block>;

   ValuePointer data() noexcept { return data_; }
   ValuePointer data_;

   static pointer 
   static_cast_block_pointer(typename super::pointer pbb) noexcept
   {
      return pointer_traits<pointer>::pointer_to(static_cast<block&>(*pbb));
   }
};

template<typename ValuePointer>
void swap_payload(block<ValuePointer>& x, block<ValuePointer>& y) noexcept
{
   std::swap(x.mask, y.mask);
   std::swap(x.data_, y.data_);
}

template<typename ValuePointer>
struct block_list: block<ValuePointer>
{
   using block = hub_detail::block<ValuePointer>;
   using block_base = typename block::super;
   using block_base_pointer = typename block_base::pointer;
   using const_block_base_pointer = typename block_base::const_pointer;
   using block_pointer = typename block::pointer;
   using block_base::full;
   using block_base::pointer_to;
   using block_base::prev_available;
   using block_base::next_available;
   using block_base::prev;
   using block_base::next;
   using block_base::mask;
   using block::data_;

   block_list() 
   { 
      reset();
      mask = 1; //sentinel
      data_ = nullptr;
   }

   block_list(block_list&& x) noexcept: block_list{}
   {
      if(x.next_available != x.header()) {
         prev_available = x.prev_available;
         next_available = x.next_available;
         next_available->prev_available = header();
         prev_available->next_available = header();
      }
      if(x.prev != x.header()) {
         prev = x.prev;
         next = x.next;
         next->prev = header();
         prev->next = header();
      }
      x.reset();
   }

   block_list& operator=(block_list&& x) noexcept
   {
      reset();
      if(x.next_available != x.header()) {
         prev_available = x.prev_available;
         next_available = x.next_available;
         next_available->prev_available = header();
         prev_available->next_available = header();
      }
      if(x.prev != x.header()) {
         prev = x.prev;
         next = x.next;
         next->prev = header();
         prev->next = header();
      }
      x.reset();
      return *this;
   }

   void reset() noexcept
   {
      prev_available = header();
      next_available = header();
      prev = header();
      next = header();
   }

   block_base_pointer header() noexcept 
   {
      return pointer_to(static_cast<block_base&>(*this)); 
   }

   const_block_base_pointer header() const noexcept 
   {
      return pointer_to(static_cast<const block_base&>(*this)); 
   }

   BOOST_CONTAINER_FORCEINLINE void link_at_back(block_pointer pb) noexcept 
   {
      pb->link_before(header());
   }

   BOOST_CONTAINER_FORCEINLINE void link_before(
      block_pointer pbx, block_pointer pby) noexcept
   {
      pbx->link_before(pby);
   }

   BOOST_CONTAINER_FORCEINLINE static void unlink(block_pointer pb) noexcept
   {
      pb->unlink();
   }

   BOOST_CONTAINER_FORCEINLINE void link_available_at_back(block_pointer pb) noexcept 
   {
      pb->link_available_before(header());
   }

   BOOST_CONTAINER_FORCEINLINE void link_available_at_front(block_pointer pb) noexcept 
   {
      pb->link_available_after(header());
   }

   BOOST_CONTAINER_FORCEINLINE void unlink_available(block_pointer pb) noexcept
   {
      pb->unlink_available();
   }
};

template<typename ValuePointer>
class iterator
{
   using element_type = typename pointer_traits<ValuePointer>::element_type;
   template<typename Value2Pointer>
   using enable_if_consts_to_element_type_t =typename std::enable_if<
      std::is_same<
         const typename pointer_traits<Value2Pointer>::element_type, 
         element_type>::value
   >::type;

public:
   using value_type = typename std::remove_const<element_type>::type;
   using difference_type = 
      typename pointer_traits<ValuePointer>::difference_type;
   using pointer = ValuePointer;
   using reference = element_type&;
   using iterator_category = std::bidirectional_iterator_tag;

   BOOST_CONTAINER_FORCEINLINE iterator() = default;
   BOOST_CONTAINER_FORCEINLINE iterator(const iterator& x) noexcept: pbb{x.pbb}, n{x.n} {}

   template<
      typename Value2Pointer,
      typename = enable_if_consts_to_element_type_t<Value2Pointer>
   >
   BOOST_CONTAINER_FORCEINLINE iterator(const iterator<Value2Pointer>& x) noexcept: 
      pbb{x.pbb}, n{x.n} {}

   BOOST_CONTAINER_FORCEINLINE iterator& operator=(const iterator& x) noexcept
   {
      pbb = x.pbb;
      n = x.n;
      return *this;
   }

   template<
      typename Value2Pointer,
      typename = enable_if_consts_to_element_type_t<Value2Pointer>
   >
   BOOST_CONTAINER_FORCEINLINE iterator& operator=(
      const iterator<Value2Pointer>& x) noexcept
   {
      pbb = x.pbb;
      n = x.n;
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE pointer operator->() const noexcept
   {
      return static_cast<block&>(*pbb).data() + n;
   }

   BOOST_CONTAINER_FORCEINLINE reference operator*() const noexcept
   {
      return *operator->();
   }

   BOOST_CONTAINER_FORCEINLINE iterator& operator++() noexcept
   {
      auto mask = pbb->mask & (full << 1 << n);
      if(BOOST_UNLIKELY(mask == 0)) {
         pbb = pbb->next;
         BOOST_CONTAINER_HUB_PREFETCH(pbb->next->next);
         BOOST_CONTAINER_HUB_PREFETCH_BLOCK(pbb->next, block);
         mask = pbb->mask;
      }
      n = dtl::unchecked_countr_zero(mask);
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE iterator operator++(int) noexcept
   {
      iterator tmp(*this);
      this->operator++();
      return tmp;
   }

   BOOST_CONTAINER_FORCEINLINE iterator& operator--() noexcept
   {
      auto mask = pbb->mask & (full >> 1 >> (N - 1 - n));
      if(BOOST_UNLIKELY(mask == 0)) {
         pbb = pbb->prev;
         BOOST_CONTAINER_HUB_PREFETCH(pbb->prev->prev);
         BOOST_CONTAINER_HUB_PREFETCH_BLOCK(pbb->prev, block);
         mask = pbb->mask;
      }
      n = N - 1 - dtl::unchecked_countl_zero(mask);
      return *this;
   }

   BOOST_CONTAINER_FORCEINLINE iterator operator--(int) noexcept
   {
      iterator tmp(*this);
      this->operator--();
      return tmp;
   }

   BOOST_CONTAINER_FORCEINLINE friend bool operator==(
      const iterator& x, const iterator& y) noexcept
   {
      return x.pbb == y.pbb && x.n == y.n;
   }

   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(
      const iterator& x, const iterator& y) noexcept
   {
      return !(x == y);
   }

private:
   template<typename> friend class iterator;
   template<typename, typename> friend class container::hub;
   template<typename VP, typename F>
   friend std::pair<hub_detail::iterator<VP>, F> container::for_each_while(
      hub_detail::iterator<VP>, hub_detail::iterator<VP>, F);
   template<typename T, typename A, typename F>
   friend std::pair<typename hub<T, A>::iterator, F> container::for_each_while(
      hub<T, A>&, F);

   template<typename T>
   using pointer_rebind_t = hub_detail::pointer_rebind_t<ValuePointer, T>;
   using block_base = hub_detail::block_base<pointer_rebind_t<void>>;
   using block_base_pointer = pointer_rebind_t<block_base>;
   using const_block_base_pointer = pointer_rebind_t<const block_base>;
   using nonconst_pointer = pointer_rebind_t<value_type>; //used by Natvis
   using block = hub_detail::block<nonconst_pointer>;
   using mask_type = typename block_base::mask_type;

   static constexpr int N = block_base::N;
   static constexpr mask_type full = block_base::full;

   BOOST_CONTAINER_FORCEINLINE iterator(const_block_base_pointer pbb_, int n_) noexcept:
      pbb{const_cast_block_base_pointer(pbb_)}, n{n_} {}

   BOOST_CONTAINER_FORCEINLINE iterator(const_block_base_pointer pbb_) noexcept:
      pbb{const_cast_block_base_pointer(pbb_)}, 
      n{dtl::unchecked_countr_zero(pbb->mask)} 
   {}

   static block_base_pointer
   const_cast_block_base_pointer(const_block_base_pointer pbb_) noexcept
   {
      return block_base::pointer_to(const_cast<block_base&>(*pbb_));
   }

   block_base_pointer pbb = nullptr;
   int                n = 0;
};

template<class F>
struct inline_ref_caller
{
   F& f;

   template<typename T>
   BOOST_CONTAINER_FORCEINLINE auto operator()(T&& x) ->
      decltype(std::declval<F>()(std::declval<T&&>()))
   {
      return f(std::forward<T>(x));
   }
};

template<class F>
struct inline_ref_const_caller
{
   F& f;

   template<typename T>
   BOOST_CONTAINER_FORCEINLINE auto operator()(const T& x) ->
      decltype(std::declval<F>()(std::declval<const T&>()))
   {
      return f(x);
   }
};

template<class F>
struct inline_return_true_ref_caller
{
   F& f;

   template<typename T>
   BOOST_CONTAINER_FORCEINLINE bool operator()(T&& x)
   {
      f(std::forward<T>(x));
      return true;
   }
};

template<class F>
struct inline_return_true_ref_const_caller
{
   F& f;

   template<typename T>
   BOOST_CONTAINER_FORCEINLINE bool operator()(const T& x)
   {
      f(x);
      return true;
   }
};

template<typename T, std::size_t N>
struct sort_iterator
{
   using value_type = T;
   using difference_type = std::ptrdiff_t;
   using pointer = T*;
   using reference = T&;
   using iterator_category = std::random_access_iterator_tag;

   sort_iterator(T** pp_, difference_type index_): pp{pp_}, index{index_} {}

   pointer operator->() const noexcept
   {
      return pp[(std::size_t)index / N] + ((std::size_t)index % N);
   }

   reference operator*() const noexcept
   {
      return *operator->();
   }

   sort_iterator& operator++() noexcept
   {
      ++index;
      return *this;
   }

   sort_iterator operator++(int) noexcept
   {
      sort_iterator tmp(*this);
      ++index;
      return tmp;
   }

   sort_iterator& operator--() noexcept
   {
      --index;
      return *this;
   }

   sort_iterator operator--(int) noexcept
   {
      sort_iterator tmp(*this);
      --index;
      return tmp;
   }

   friend difference_type
   operator-(const sort_iterator& x, const sort_iterator& y) noexcept
   {
      return x.index - y.index;
   }

   sort_iterator& operator+=(difference_type n) noexcept
   {
      index += n;
      return *this;
   }

   friend sort_iterator
   operator+(const sort_iterator& x, difference_type n) noexcept
   {
      return {x.pp, x.index + n};
   }

   friend sort_iterator 
   operator+(difference_type n, const sort_iterator& x) noexcept
   {
      return {x.pp, n + x.index};
   }

   sort_iterator& operator-=(difference_type n) noexcept
   {
      index -= n;
      return *this;
   }

   friend sort_iterator 
   operator-(const sort_iterator& x, difference_type n) noexcept
   {
      return {x.pp, x.index - n};
   }

   reference operator[](difference_type n) const noexcept
   {
      return *(*this + n);
   }

   friend bool 
   operator==(const sort_iterator& x, const sort_iterator& y) noexcept
   {
      return x.index == y.index;
   }

   friend bool
   operator!=(const sort_iterator& x, const sort_iterator& y) noexcept
   {
      return x.index != y.index;
   }

   friend bool
   operator<(const sort_iterator& x, const sort_iterator& y) noexcept
   {
      return x.index < y.index;
   }

   friend bool
   operator>(const sort_iterator& x, const sort_iterator& y) noexcept
   {
      return x.index > y.index;
   }

   friend bool
   operator<=(const sort_iterator& x, const sort_iterator& y) noexcept
   {
      return x.index <= y.index;
   }

   friend bool
   operator>=(const sort_iterator& x, const sort_iterator& y) noexcept
   {
      return x.index >= y.index;
   }

   T**             pp;
   difference_type index;
};

template<typename T, typename Allocator>
struct buffer
{
   buffer(std::size_t n, Allocator al_) noexcept: al{al_} 
   {
      allocate_data(n);
      if(data) capacity = n;
   }

   ~buffer()
   {
      if(data) {
         for(; begin_ != end_; ++begin_) allocator_traits<Allocator>::destroy(al, begin());
         deallocate_data();
      }
   }

   T* begin() const noexcept { return data + begin_; }
   T* end() const noexcept { return data + end_; }

   template<typename... Args>
   void emplace_back(Args&&... args)
   {
      BOOST_ASSERT(data && end_ != capacity);
      allocator_traits<Allocator>::construct(al, end(), std::forward<Args>(args)...);
      ++end_;
   }

   void erase_front() noexcept
   {
      BOOST_ASSERT(data && begin_ != end_);
      allocator_traits<Allocator>::destroy(al, begin());
      ++begin_;
   }

   Allocator   al;
   std::size_t begin_ = 0, end_ = 0;
   std::size_t capacity = 0;
   T*          data = nullptr;

private:
   //Portable, nothrow, (over)aligned allocation: this is the same primitive
   //boost::container::new_allocator relies on for overalignment, so there is no
   //need to special-case __cpp_aligned_new here. The nothrow null return lets
   //sort() fall back to a leaner algorithm when this (possibly large) scratch
   //buffer cannot be allocated.
   void allocate_data(std::size_t n)
   {
      data = static_cast<T*>(dtl::aligned_allocate(alignof(T), n * sizeof(T)));
   }

   void deallocate_data() { dtl::aligned_deallocate(data); }
};

template<typename T>
struct nodtor_deleter
{
   using pointer = T*;
   void operator()(pointer p) noexcept { ::operator delete(p); }
};

template<typename T>
struct nodtor_deleter<T[]>
{
   using pointer = T*;
   void operator()(pointer p) noexcept { ::operator delete[](p); }
};

template<typename T>
using nodtor_unique_ptr = boost::movelib::unique_ptr<T, nodtor_deleter<T>>;

#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
//begin()/end() access (without <ranges> or std::begin/std::end) lives in
//boost::container::dtl::adl_range (range_utils.hpp); reuse it here.
template<class R>
using range_iterator_t = decltype(dtl::adl_range::adl_begin(std::declval<R&>()));

template<class R>
using range_reference_t = std::iter_reference_t<range_iterator_t<R> >;

template<class R>
using range_value_t = std::iter_value_t<range_iterator_t<R> >;

template<class R>
concept input_range_like =
   requires(R& r) {
      dtl::adl_range::adl_begin(r);
      dtl::adl_range::adl_end(r);
   } &&
   std::input_iterator<range_iterator_t<R> >;

template<class R, class T>
concept container_compatible_range =
   input_range_like<R> &&
   std::convertible_to<range_reference_t<R>, T>;

#endif

template<typename InputIterator>
using enable_if_is_input_iterator_t =
   typename std::enable_if<
      std::is_convertible<
         typename std::iterator_traits<InputIterator>::iterator_category,
         std::input_iterator_tag
      >::value
   >::type;

//std::pmr::polymorphic_allocator::destroy may be marked as deprecated.
//C&P from boost/core/allocator_access.hpp.
#if defined(_LIBCPP_SUPPRESS_DEPRECATED_PUSH)
_LIBCPP_SUPPRESS_DEPRECATED_PUSH
#endif
#if defined(_STL_DISABLE_DEPRECATED_WARNING)
_STL_DISABLE_DEPRECATED_WARNING
#endif
#if defined(__clang__) && defined(__has_warning)
# if __has_warning("-Wdeprecated-declarations")
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
# endif
#elif defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4996)
#elif defined(BOOST_GCC) && BOOST_GCC >= 40600
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

template<typename Allocator, typename Ptr, typename = void>
struct allocator_has_destroy: std::false_type {};

template<typename Allocator, typename Ptr>
struct allocator_has_destroy<
   Allocator, Ptr,
   decltype((void)std::declval<Allocator&>().destroy(std::declval<Ptr>()))
>: std::true_type {};

#if defined(__clang__) && defined(__has_warning)
# if __has_warning("-Wdeprecated-declarations")
#  pragma clang diagnostic pop
# endif
#elif defined(_MSC_VER)
# pragma warning(pop)
#elif defined(BOOST_GCC) && BOOST_GCC >= 40600
# pragma GCC diagnostic pop
#endif  
#if defined(_STL_RESTORE_DEPRECATED_WARNING)
_STL_RESTORE_DEPRECATED_WARNING
#endif
#if defined(_LIBCPP_SUPPRESS_DEPRECATED_POP)
_LIBCPP_SUPPRESS_DEPRECATED_POP
#endif

template<typename Allocator>
struct is_std_allocator: std::false_type {};

template<typename T>
struct is_std_allocator<std::allocator<T>>: std::true_type {};

template<typename Allocator>
struct is_std_pmr_polymorphic_allocator: std::false_type {};

#ifndef BOOST_NO_CXX17_HDR_MEMORY_RESOURCE
template<typename T>
struct is_std_pmr_polymorphic_allocator<std::pmr::polymorphic_allocator<T>>:
   std::true_type {};
#endif

struct if_constexpr_void_else{ void operator()() const {} };

template<typename F, typename G = if_constexpr_void_else>
void if_constexpr(std::true_type, F f, G = G{}) { f(); }

template<typename F, typename G = if_constexpr_void_else>
void if_constexpr(std::false_type, F, G g = G{}) { g(); }

template<typename T>
void copy_assign_if(std::true_type, T& x, const T& y) { x = y; }

template<typename T>
void copy_assign_if(std::false_type, T&, const T&) {}

template<typename T>
void move_assign_if(std::true_type, T& x, T& y) { x = std::move(y); }

template<typename T>
void move_assign_if(std::false_type, T&, T&) {}

template<typename T>
void swap_if(std::true_type, T& x, T& y) { using std::swap; swap(x, y); }

template<typename T>
void swap_if(std::false_type, T&, T&) {}

template<typename Allocator>
struct block_typedefs
{
   using pointer = typename allocator_traits<Allocator>::pointer;
   template<typename Q>
   using pointer_rebind_t = hub_detail::pointer_rebind_t<pointer, Q>;

   using block_base = hub_detail::block_base<pointer_rebind_t<void>>;
   using block_base_pointer = pointer_rebind_t<block_base>;
   using const_block_base_pointer = pointer_rebind_t<const block_base>;
   using block = hub_detail::block<pointer>;
   using block_pointer = pointer_rebind_t<block>;
   using block_allocator =
      typename allocator_traits<Allocator>::template portable_rebind_alloc<block>::type;
   using block_list = hub_detail::block_list<pointer>;
};

} //namespace container::hub_detail

#endif   //BOOST_CONTAINER_DOXYGEN_INVOKED

//! A hub is a container with constant-time insertion and erasure and element
//! stability: pointers and iterators to an element remain valid until the
//! element is erased. It is a nearly drop-in, more compact alternative to the
//! C++26 \c std::hive.
//!
//! Elements of a hub are stored in \e blocks of contiguous memory, each with a fixed
//! element capacity. The insertion position is chosen by the container,
//! which may reuse the memory of previously erased elements. A block with at
//! least one element is called \e active; an empty block kept internally for
//! future reuse is called \e reserved. Reserved blocks are not used until all
//! active blocks are full, and are only deallocated by \c shrink_to_fit,
//! \c trim_capacity or on container destruction. New blocks are allocated only
//! when every block is full or when the user issues a \c reserve operation.
//!
//! \c hub<T, Allocator> is a model of \c Container, \c ReversibleContainer,
//! \c AllocatorAwareContainer and \c SequenceContainer, with the following
//! exceptions: operators \c == and \c != are not provided, and positional
//! insertion of the form \c insert(position,\ ...) or \c emplace(position,\ ...)
//! is not provided or ignores the position argument. Its iterators model
//! \c LegacyBidirectionalIterator.
//!
//! \tparam T The cv-unqualified object type of the elements stored in the hub.
//! \tparam Allocator An allocator whose value type is \c T. If \c void (the
//!   default), \c boost::container::new_allocator<T> is used.
//!
//! <b>Exception safety</b>: Except when explicitly noted, all non-const member
//!   functions (and free functions taking \c hub by non-const reference) provide
//!   the basic exception guarantee, whereas all const member functions (and free
//!   functions taking \c hub by const reference) provide the strong guarantee.
#ifdef BOOST_CONTAINER_DOXYGEN_INVOKED
template<typename T, typename Allocator = void>
class hub
#else
template<typename T, typename AllocatorOrVoid>
class hub
   : hub_detail::block_typedefs<typename real_allocator<T, AllocatorOrVoid>::type>::block_allocator
#endif
{
   public:
   //! new_allocator<T> is if Allocator is void, an alias for Allocator otherwise.
   typedef BOOST_CONTAINER_IMPDEF
      (typename real_allocator<T BOOST_MOVE_I AllocatorOrVoid>::type)     allocator_type;

private:
   static_assert(
      !std::is_const<T>::value && !std::is_volatile<T>::value && 
      !std::is_function<T>::value && !std::is_reference<T>::value && 
      !std::is_void<T>::value,
      "T must be a cv-unqualified object type");
   static_assert(
      std::is_same<T, typename allocator_traits<allocator_type>::value_type>::value,
      "Allocator's value_type must be the same type as T");

public:
   using value_type = T;
   using pointer = typename allocator_traits<allocator_type>::pointer;
   using const_pointer = typename allocator_traits<allocator_type>::const_pointer;
   using reference = T&;
   using const_reference = const T&;
   using size_type = typename allocator_traits<allocator_type>::size_type;
   using difference_type = typename allocator_traits<allocator_type>::difference_type;
   using iterator = hub_detail::iterator<pointer>;
   using const_iterator = hub_detail::iterator<const_pointer>;
   using reverse_iterator = std::reverse_iterator<iterator>;
   using const_reverse_iterator = std::reverse_iterator<const_iterator>; 

   //! <b>Effects</b>: Constructs an empty hub, using \c allocator_type() as the allocator.
   //!
   //! <b>Requires</b>: \c allocator_type is DefaultConstructible.
   //!
   //! <b>Complexity</b>: Constant.
   hub() noexcept(noexcept(allocator_type())): hub{allocator_type()} {}

   //! <b>Effects</b>: Constructs an empty hub, using the specified allocator.
   //!
   //! <b>Complexity</b>: Constant.
   explicit hub(const allocator_type& al_) noexcept: 
      allocator_base(al_) {}

   //! <b>Effects</b>: Constructs a hub with n default-inserted elements, using
   //!   the specified allocator.
   //!
   //! <b>Requires</b>: T is DefaultInsertable into the hub.
   //!
   //! <b>Complexity</b>: Linear in n.
   explicit hub(size_type n, const allocator_type& al_ = allocator_type()): hub{al_}
   {
      range_insert_impl(size_type(0), n, [&, this] (T* p, size_type) {
         block_alloc_traits::construct(al(), p);
      });
   }

   //! <b>Effects</b>: Constructs a hub with n copies of value, using the
   //!   specified allocator.
   //!
   //! <b>Requires</b>: T is CopyInsertable into the hub.
   //!
   //! <b>Complexity</b>: Linear in n.
   hub(size_type n, const T& x, const allocator_type& al_ = allocator_type()): hub{al_}
   {
      insert(n, x);
   }

   //! <b>Effects</b>: Constructs a hub equal to the range [first, last), using
   //!   the specified allocator.
   //!
   //! <b>Complexity</b>: Linear in std::distance(first, last).
   template<
      typename InputIterator 
      BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I typename = hub_detail::enable_if_is_input_iterator_t<InputIterator>)
      >
   hub(
      InputIterator first, InputIterator last,
      const allocator_type& al_ = allocator_type()): hub{al_}
   {
      insert(first, last);
   }

#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
   //! <b>Effects</b>: Constructs a hub equal to the range rg, using the
   //!   specified allocator.
   //!
   //! <b>Complexity</b>: Linear in std::ranges::distance(rg).
   template< BOOST_CONTAINER_DOC1ST(std::ranges::input_range<T>, hub_detail::container_compatible_range<T>) R >
   hub(from_range_t, R&& rg, const allocator_type& al_ = allocator_type()): hub{al_}
   {
      insert_range(std::forward<R>(rg));
   }
#endif

   //! <b>Effects</b>: Constructs a hub equal to x. The second overload uses the
   //!   given allocator.
   //!
   //! <b>Requires</b>: T is CopyInsertable into the hub.
   //!
   //! <b>Complexity</b>: Linear in x.size().
   hub(const hub& x):
      hub{x, block_alloc_traits::select_on_container_copy_construction(x.al())} {}

   //! <b>Effects</b>: Constructs a hub equal to x, using the given allocator.
   //!
   //! <b>Requires</b>: T is CopyInsertable into the hub.
   //!
   //! <b>Complexity</b>: Linear in x.size().
   hub( const hub& x
      , const BOOST_CONTAINER_DOC1ST(allocator_type, dtl::type_identity_t<allocator_type>) &al_):
      hub(x.begin(), x.end(), al_) {}

   //! <b>Effects</b>: Move constructor. Element blocks are moved from x into
   //!   *this; pointers, references and iterators to elements of x remain valid
   //!   but now refer to *this.
   //!
   //! <b>Postcondition</b>: x.empty() is true.
   //!
   //! <b>Complexity</b>: Constant.
   hub(hub&& x) noexcept:
      hub{std::move(x), allocator_type(std::move(x.al())), std::true_type{}} {}

   //! <b>Effects</b>: allocator_type-extended move constructor. If alloc equals
   //!   x.get_allocator() the element blocks are moved (iterators/pointers to x
   //!   remain valid as members of *this); otherwise each element is moved into
   //!   *this and references, pointers and iterators to x are invalidated.
   //!
   //! <b>Requires</b>: T is MoveInsertable when the allocators may be unequal.
   //!
   //! <b>Postcondition</b>: x.empty() is true.
   //!
   //! <b>Complexity</b>: Constant, or linear in x.size() if elements are moved
   //!   one by one.
   hub( hub&& x
      , const BOOST_CONTAINER_DOC1ST(allocator_type, dtl::type_identity_t<allocator_type>) &al_):
      hub{std::move(x), al_, hub_detail::is_always_equal_t<allocator_type>{}} {}

   //! <b>Effects</b>: Constructs a hub equal to il, using the specified allocator.
   //!
   //! <b>Requires</b>: T is CopyInsertable into the hub.
   //!
   //! <b>Complexity</b>: Linear in il.size().
   hub(std::initializer_list<T> il, const allocator_type& al_ = allocator_type()):
      hub{il.begin(), il.end(), al_} {}

   //! <b>Effects</b>: Destroys all elements and deallocates all blocks.
   //!
   //! <b>Complexity</b>: Linear in size() plus the number of blocks.
   ~hub() { reset(); }

   //! <b>Effects</b>: Copy assignment. Existing elements are copy-assigned or
   //!   destroyed and the elements of x are copied into *this, keeping their
   //!   relative order.
   //!
   //! <b>Requires</b>: T is CopyInsertable into the hub and CopyAssignable.
   //!
   //! <b>Complexity</b>: Linear in size() + x.size().
   hub& operator=(const hub& x)
   {
      using pocca = hub_detail::pocca_t<allocator_type>;

      if(this != &x) {
         if(al() != x.al() && pocca::value) {
            reset();
            hub_detail::copy_assign_if(pocca{}, al(), x.al());
            insert(x.begin(), x.end());
         }
         else{
            hub_detail::copy_assign_if(pocca{}, al(), x.al());
            assign(x.begin(), x.end());
         }
      }
      return *this;
   }

   //! <b>Effects</b>: Move assignment. Existing elements are move-assigned or
   //!   destroyed. If the allocator propagates on move assignment or both
   //!   allocators are equal, element blocks are moved from x (iterators/pointers
   //!   to x remain valid as members of *this); otherwise each element of x is
   //!   moved into *this and references, pointers and iterators to x are
   //!   invalidated.
   //!
   //! <b>Requires</b>: T is MoveInsertable and MoveAssignable when elements are
   //!   moved one by one.
   //!
   //! <b>Postcondition</b>: x.empty() is true.
   //!
   //! <b>Complexity</b>: Linear in size(), plus linear in x.size() when elements
   //!   are moved one by one.
   hub& operator=(hub&& x)
      noexcept(
         hub_detail::pocma_t<allocator_type>::value ||
         hub_detail::is_always_equal_t<allocator_type>::value)
   {
      if(this != &x) {
         move_assign(
            x, 
            std::integral_constant<
               bool,
               hub_detail::pocma_t<allocator_type>::
                  value ||
               hub_detail::is_always_equal_t<allocator_type>::value>{});
      }
      return *this;
   }

   //! <b>Effects</b>: Replaces the contents of *this with a copy of il.
   //!
   //! <b>Complexity</b>: Linear in size() + il.size().
   hub& operator=(std::initializer_list<T> il)
   {
      assign(il);
      return *this;
   }

   //! <b>Effects</b>: Replaces the contents of *this with a copy of the range
   //!   [first, last).
   //!
   //! <b>Complexity</b>: Linear in size() + std::distance(first, last).
   template<
      typename InputIterator
      BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I typename = hub_detail::enable_if_is_input_iterator_t<InputIterator>)
      >
   void assign(InputIterator first, InputIterator last)
   {
      range_assign_impl(
         first, last,
         [this] (T* p, InputIterator it) { block_alloc_traits::construct(al(), p, *it); },
         [] (T* p, InputIterator it) { *p = *it; });
   }

#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
   //! <b>Effects</b>: Replaces the contents of *this with a copy of the elements
   //!   in the range rg.
   //!
   //! <b>Complexity</b>: Linear in size() + std::ranges::distance(rg).
   template< BOOST_CONTAINER_DOC1ST(std::ranges::input_range<T>, hub_detail::container_compatible_range<T>) R >
   void assign_range(R&& rg)
   {
      range_assign_impl(
         dtl::adl_range::adl_begin(rg), dtl::adl_range::adl_end(rg),
         [this] (T* p, auto it) { block_alloc_traits::construct(al(), p, *it); },
         [] (T* p, auto it) { *p = *it; });
   }
#endif

   //! <b>Effects</b>: Replaces the contents of *this with n copies of x.
   //!
   //! <b>Complexity</b>: Linear in size() + n.
   void assign(size_type n, const T& x)
   {
      range_assign_impl(
         size_type(0), n,
         [&, this] (T* p, size_type) { block_alloc_traits::construct(al(), p, x); },
         [&] (T* p, size_type) { *p = x; });
   }

   //! <b>Effects</b>: Replaces the contents of *this with a copy of il.
   //!
   //! <b>Complexity</b>: Linear in size() + il.size().
   void assign(std::initializer_list<T> il) { assign(il.begin(), il.end()); }

   //! <b>Effects</b>: Returns a copy of the allocator associated with *this.
   //!
   //! <b>Complexity</b>: Constant.
   allocator_type get_allocator() const noexcept { return al(); }

   //! <b>Effects</b>: Returns an iterator to the first element, or end() if empty.
   //!   The const overloads return a const_iterator. <b>Complexity</b>: Constant.
   iterator               begin() noexcept { return ++end(); }
   const_iterator         begin() const noexcept { return ++end(); }
   //! <b>Effects</b>: Returns the past-the-end iterator. The end iterator is
   //!   stable: it is not invalidated by insertion or erasure. The const overloads
   //!   return a const_iterator. <b>Complexity</b>: Constant.
   iterator               end() noexcept { return {blist.header(), 0}; }
   const_iterator         end() const noexcept { return {blist.header(), 0}; }
   //! <b>Effects</b>: Reverse and const iterator accessors, with the usual
   //!   semantics. <b>Complexity</b>: Constant.
   reverse_iterator       rbegin() noexcept { return reverse_iterator{end()}; }
   const_reverse_iterator rbegin() const noexcept
   {  return const_reverse_iterator{end()};  }
   reverse_iterator       rend() noexcept { return reverse_iterator{begin()}; }
   const_reverse_iterator rend() const noexcept
   {  return const_reverse_iterator{begin()};  }
   const_iterator         cbegin() const noexcept { return begin(); }
   const_iterator         cend() const noexcept { return end(); }
   const_reverse_iterator crbegin() const noexcept { return rbegin(); }
   const_reverse_iterator crend() const noexcept { return rend(); }

   //! <b>Effects</b>: Returns true if the hub contains no elements.
   //!
   //! <b>Complexity</b>: Constant.
   bool      empty() const noexcept { return size_ == 0; }

   //! <b>Effects</b>: Returns the number of elements in the hub.
   //!
   //! <b>Complexity</b>: Constant.
   size_type size() const noexcept { return size_; }

   //! <b>Effects</b>: Returns the largest possible size of the hub.
   //!
   //! <b>Complexity</b>: Constant.
   size_type max_size() const noexcept 
   {
      std::size_t
         bs = (std::size_t)block_alloc_traits::max_size(al()) * sizeof(block),
         vs = (std::size_t)value_alloc_traits::max_size(value_allocator(al())) * sizeof(T);
      return 
         (size_type)((std::min)(bs, vs) / (sizeof(block) + sizeof(T) * N) * N);
   }

   //! <b>Effects</b>: Returns the total number of elements that *this can hold
   //!   without requiring allocation of more element blocks.
   //!
   //! <b>Complexity</b>: Constant.
   size_type capacity() const noexcept { return num_blocks * N; }

   //! <b>Effects</b>: If n <= capacity() there are no effects; otherwise
   //!   increases capacity() by allocating reserved blocks.
   //!
   //! <b>Postcondition</b>: capacity() >= n.
   //!
   //! <b>Throws</b>: if n > max_size(), plus any exception
   //!   thrown by the allocator.
   //!
   //! <b>Complexity</b>: Linear in the number of reserved blocks allocated.
   //!
   //! <b>Note</b>: All references, pointers and iterators (including the
   //!   past-the-end iterator) remain valid.
   void reserve(size_type n)
   {
      if(n > max_size()) {
         throw_length_error("Requested capacity greater than max_size()");
      }
      while(capacity() < n) (void)create_new_available_block();
   }

   //! <b>Effects</b>: Reallocates elements if needed so that the number of active
   //!   blocks is minimized and deallocates the ensuing reserved blocks. If
   //!   capacity() already equals size() there are no effects. If T throws during
   //!   reallocation, the effects are unspecified.
   //!
   //! <b>Requires</b>: T is MoveInsertable into the hub.
   //!
   //! <b>Complexity</b>: Linear in size() if reallocation happens, plus linear in
   //!   the number of reserved blocks.
   //!
   //! <b>Note</b>: If reallocation happens, the order of the elements may change
   //!   and all references, pointers and iterators to elements are invalidated.
   void shrink_to_fit()
   {
      compact();
      trim_capacity();
   }

   //! <b>Effects</b>: Deallocates all reserved blocks, reducing capacity()
   //!   accordingly.
   //!
   //! <b>Complexity</b>: Linear in the number of reserved blocks deallocated.
   //!
   //! <b>Note</b>: All references, pointers and iterators (including the
   //!   past-the-end iterator) remain valid.
   void trim_capacity() noexcept { trim_capacity(0); }

   //! <b>Effects</b>: If n >= capacity() there are no effects; otherwise reduces
   //!   capacity() to no less than n by deallocating reserved blocks.
   //!
   //! <b>Complexity</b>: Linear in the number of reserved blocks deallocated.
   //!
   //! <b>Note</b>: All references, pointers and iterators (including the
   //!   past-the-end iterator) remain valid.
   void trim_capacity(size_type n) noexcept
   {
      if(capacity() <= n) return;
      for(auto pbb = blist.header()->prev_available;
            capacity() - n >= N && pbb != blist.header() && pbb->mask == 0; ) {
         auto pb = static_cast_block_pointer(pbb);
         pbb = pbb-> prev_available;
         blist.unlink_available(pb);
         delete_block(pb);
         --num_blocks;
      }
   }

   //! <b>Effects</b>: Inserts an object of type T constructed with
   //!   std::forward<Args>(args)... at a position chosen by the container. If an
   //!   exception is thrown there are no effects. args may directly or indirectly
   //!   refer to a value in *this.
   //!
   //! <b>Requires</b>: T is EmplaceConstructible into the hub from args.
   //!
   //! <b>Returns</b>: An iterator pointing to the new element.
   //!
   //! <b>Complexity</b>: Constant. Exactly one object of type T is constructed.
   template<typename... Args>
   BOOST_CONTAINER_FORCEINLINE iterator emplace(Args&&... args)
   {
      auto pbb = blist.next_available; //for construct_or_restore_capacity
      int  n;
      auto pb = retrieve_available_block(n);
      auto mask = pb->mask;
      construct_or_restore_capacity(
         boost::movelib::to_raw_pointer(pb->data() + n), pbb, std::forward<Args>(args)...);
      mask |= mask + 1;
      pb->mask = mask;
      auto mask_plus_one = mask + 1;
      if(BOOST_UNLIKELY(mask_plus_one <= 2)) {
         //pb->mask == 0 (impossible), 1 or full
         if(mask_plus_one == 0) blist.unlink_available(pb);
         else /* pb->mask == 1 */ blist.link_at_back(pb);
      }
      ++size_;
      return {pb, n};
   }

   //! <b>Effects</b>: Equivalent to emplace(std::forward<Args>(args)...); the
   //!   hint is ignored.
   //!
   //! <b>Returns</b>: An iterator pointing to the new element.
   //!
   //! <b>Complexity</b>: Constant.
   template<typename... Args>
   BOOST_CONTAINER_FORCEINLINE iterator emplace_hint(const_iterator, Args&&... args)
   {
      return emplace(std::forward<Args>(args)...);
   }

   //! <b>Effects</b>: Inserts a copy of x (or moves x) at a position chosen by
   //!   the container; overloads taking a hint ignore it. Equivalent to
   //!   emplace(std::forward<decltype(x)>(x)).
   //!
   //! <b>Returns</b>: An iterator pointing to the new element.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_FORCEINLINE iterator insert(const T& x) { return emplace(x); }
   BOOST_CONTAINER_FORCEINLINE iterator insert(const_iterator, const T& x)
   {  return emplace(x);  }
   BOOST_CONTAINER_FORCEINLINE iterator insert(T&& x) { return emplace(std::move(x)); }
   BOOST_CONTAINER_FORCEINLINE iterator insert(const_iterator, T&& x)
   {  return emplace(std::move(x));  }

   //! <b>Effects</b>: Inserts copies of the elements in il. Equivalent to
   //!   insert(il.begin(), il.end()).
   //!
   //! <b>Complexity</b>: Linear in il.size().
   void insert(std::initializer_list<T> il) { insert(il.begin(), il.end()); }

#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
   //! <b>Effects</b>: Inserts copies of the elements in rg. Each iterator in rg
   //!   is dereferenced exactly once.
   //!
   //! <b>Requires</b>: T is EmplaceConstructible into the hub from
   //!   *ranges::begin(rg) and rg does not overlap with *this.
   //!
   //! <b>Complexity</b>: Linear in the number of elements inserted; one object of
   //!   type T is constructed per element.
   template< BOOST_CONTAINER_DOC1ST(std::ranges::input_range<T>, hub_detail::container_compatible_range<T>) R >
   void insert_range(R&& rg)
   {
      range_insert_impl(
         dtl::adl_range::adl_begin(rg), dtl::adl_range::adl_end(rg),
         [this] (T* p, auto it) { block_alloc_traits::construct(al(), p, *it); });
   }
#endif

   //! <b>Effects</b>: Inserts copies of the elements in [first, last). Each
   //!   iterator in the range is dereferenced exactly once.
   //!
   //! <b>Requires</b>: T is EmplaceConstructible into the hub from *first and
   //!   [first, last) does not overlap with *this.
   //!
   //! <b>Complexity</b>: Linear in the number of elements inserted; one object of
   //!   type T is constructed per element.
   template<
      typename InputIterator
      BOOST_CONTAINER_DOCIGN(BOOST_MOVE_I typename = hub_detail::enable_if_is_input_iterator_t<InputIterator>)
   >
   void insert(InputIterator first, InputIterator last)
   {
      range_insert_impl(first, last, [this] (T* p, InputIterator it) {
         block_alloc_traits::construct(al(), p, *it);
      });
   }

   //! <b>Effects</b>: Inserts n copies of x.
   //!
   //! <b>Requires</b>: T is CopyInsertable into the hub.
   //!
   //! <b>Complexity</b>: Linear in n; one object of type T is constructed per
   //!   element.
   void insert(size_type n, const T& x)
   {
      range_insert_impl(size_type(0), n, [&, this] (T* p, size_type) {
         block_alloc_traits::construct(al(), p, x);
      });
   }

   //! <b>Effects</b>: Erases the element pointed to by pos.
   //!
   //! <b>Returns</b>: An iterator pointing to the element that followed the
   //!   erased one.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Invalidates references, pointers and iterators referring to
   //!   the erased element.
   BOOST_CONTAINER_FORCEINLINE iterator erase(const_iterator pos)
   {
      auto pbb = pos.pbb;
      auto n = pos.n;
      ++pos;
      erase_impl(pbb, n);
      return {pos.pbb, pos.n};
   }

   //! <b>Effects</b>: Erases the element pointed to by pos. Equivalent to
   //!   erase(pos) but returns nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Potentially faster than erase(pos) as no return iterator needs
   //!   to be computed. Invalidates references, pointers and iterators referring
   //!   to the erased element.
   BOOST_CONTAINER_FORCEINLINE void erase_void(const_iterator pos)
   {
      erase_impl(pos.pbb, pos.n);
   }

   //! <b>Effects</b>: Erases the elements in the range [first, last).
   //!
   //! <b>Returns</b>: An iterator pointing to the element that followed the last
   //!   erased element.
   //!
   //! <b>Complexity</b>: Linear in the number of elements erased.
   //!
   //! <b>Note</b>: Invalidates references, pointers and iterators referring to
   //!   the erased elements.
   iterator erase(const_iterator first, const_iterator last)
   {
      for(auto pbb = first.pbb; first != last; ) {
         first = erase(first);
         if(first.pbb != pbb) break;
      }
      auto pbb = first.pbb;
      if(pbb != last.pbb) {
         do {
            auto pb = static_cast_block_pointer(pbb);
            pbb = pb->next;
            BOOST_CONTAINER_HUB_PREFETCH_BLOCK(pbb, block);
            size_ -= destroy_all_in_nonempty_block(pb);
            blist.unlink(pb);
            if(BOOST_LIKELY(pb->mask != full)) blist.unlink_available(pb);
            blist.link_available_at_back(pb);
            pb->mask = 0;
         } while(pbb != last.pbb);
         first = {pbb};
      }
      while(first != last) first = erase(first);
      return {last.pbb, last.n};
   }

   //! <b>Effects</b>: Exchanges the contents and capacity() of *this with those
   //!   of x.
   //!
   //! <b>Complexity</b>: Constant.
   void swap(hub& x)
      noexcept(
         hub_detail::pocs_t<allocator_type>::value ||
         hub_detail::is_always_equal_t<allocator_type>::value)
   {
      using pocs = hub_detail::pocs_t<allocator_type>;

      hub_detail::if_constexpr(pocs{}, [&, this]{
         hub_detail::swap_if(pocs{}, al(), x.al());
      },
      [&, this]{ //else
         BOOST_ASSERT(al() == x.al());
         (void)this;
      });
      std::swap(blist, x.blist);
      std::swap(num_blocks, x.num_blocks);
      std::swap(size_, x.size_);
   }

   //! <b>Effects</b>: Erases all elements. Reserved blocks are kept.
   //!
   //! <b>Complexity</b>: Linear in size().
   void clear() noexcept { erase(begin(), end()); }

   //! <b>Effects</b>: Inserts the contents of x into *this, leaving x empty.
   //!   Pointers and references to the moved elements of x now refer to *this;
   //!   iterators continue to refer to their elements but behave as iterators
   //!   into *this. Reserved blocks of x are not transferred.
   //!
   //! <b>Requires</b>: get_allocator() == x.get_allocator() and
   //!   std::addressof(x) != this.
   //!
   //! <b>Complexity</b>: Linear in the number of blocks of x plus the number of
   //!   blocks of *this.
   void splice(hub& x)
   {
      BOOST_ASSERT(this != &x);
      BOOST_ASSERT(al() == x.al());
      for(auto pbb = x.blist.header()->next; pbb != x.blist.header(); ) {
         auto pb = static_cast_block_pointer(pbb);
         pbb = pbb->next;
         if(pb->mask != full) {
            x.blist.unlink_available(pb);
            blist.link_available_at_front(pb);
         }
         x.blist.unlink(pb);
         blist.link_at_back(pb);
         --x.num_blocks;
         ++num_blocks;
         auto s = dtl::popcount(pb->mask);
         x.size_ -= (size_type)s;
         size_ += (size_type)s;
      }
   }

   //! <b>Effects</b>: Equivalent to splice(x).
   void splice(hub&& x) { splice(x); }

   //! <b>Effects</b>: Erases all but the first element from every consecutive
   //!   group of equivalent elements, i.e. erases each element i in
   //!   [begin() + 1, end()) for which pred(*i, *(i - 1)) is true.
   //!
   //! <b>Requires</b>: pred is an equivalence relation.
   //!
   //! <b>Returns</b>: The number of elements erased.
   //!
   //! <b>Throws</b>: Nothing unless pred throws.
   //!
   //! <b>Complexity</b>: Exactly size() - 1 applications of pred for a non-empty
   //!   hub, otherwise none.
   //!
   //! <b>Note</b>: Invalidates references, pointers and iterators referring to
   //!   the erased elements.
   template<typename BinaryPredicate = std::equal_to<T>>
   size_type unique(BinaryPredicate pred = BinaryPredicate())
   {
      auto s = size_;
      for(auto first = cbegin(), last = cend(); first != last; ) {
         auto next = std::next(first);
         first = erase(
            next,
            std::find_if_not(next, last, [&] (const T& x) {
               return pred(x, *first);
            }));
      }
      return (size_type)(s - size_);
   }

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4127) //conditional expression is constant
#endif

   //! <b>Effects</b>: Sorts *this according to comp. If comp or any operation on
   //!   T throws, *this is left in a valid but unspecified state; if an exception
   //!   is thrown while allocating internal memory there are no effects.
   //!
   //! <b>Requires</b>: T is MoveInsertable into the hub, MoveConstructible,
   //!   MoveAssignable and Swappable.
   //!
   //! <b>Complexity</b>: O(N*log(N)) comparisons, where N is size().
   //!
   //! <b>Note</b>: May allocate. References, pointers and iterators to elements
   //!   may be invalidated. The sort is not stable.
   template<typename Compare = std::less<T>>
   void sort(Compare comp = Compare())
   {
      //transfer_sort is usually the fastest, but it consumes the most
      //auxiliary memory when sizeof(T) > sizeof(sort_proxy), so we restrict
      //its usage to the case sizeof(T) <= sizeof(sort_proxy).
      //compact_sort uses the least amount of auxiliary memory (by far), and
      //it's also faster than proxy_sort when the auxiliary memory of the latter
      //exceeds some threshold seemingly related to the size of the L2 cache;
      //we conventionally set the threshold to 2MB for lack of a more precise
      //estimation mechanism.
      //TODO: In 32-bit mode, the threshold policy is not so clear-cut and the
      //cost of moving elements around seems to play a role.
      BOOST_IF_CONSTEXPR(sizeof(T) <= sizeof(sort_proxy)) {
         if(transfer_sort(comp)) return;
      }
      else{
         static constexpr std::size_t memory_threshold = 2 * 1024 * 1024;
         if((std::size_t)size_ * sizeof(sort_proxy) <= memory_threshold) {
            if(proxy_sort(comp)) return;
         }
      }
      compact_sort(comp);
   }

#if defined(BOOST_MSVC)
#pragma warning(pop) //C4127
#endif

   //! <b>Effects</b>: Returns an iterator referring to the
   //!   same element as p.
   //!
   //! <b>Requires</b>: p points to an element in *this.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear in the number of active blocks in *this.
   iterator get_iterator(const_pointer p)
   {   
      std::less<const T*> less;
      for(auto pbb = blist.next; pbb != blist.header(); pbb = pbb-> next) {
         auto pb = static_cast_block_pointer(pbb);
         if(!less(boost::movelib::to_raw_pointer(p), boost::movelib::to_raw_pointer(pb->data())) &&
               less(boost::movelib::to_raw_pointer(p), boost::movelib::to_raw_pointer(pb->data() + N))) {
            int n = (int)(p - pb->data());
            BOOST_ASSERT_MSG(
               (pb->mask & ((mask_type)(1) << n)) != 0,
               "p points to an invalid element");
            return {pb, n};
         }
      }
      BOOST_ASSERT_MSG(false, "p does not point into the extents of *this");
#if defined(BOOST_ASSERT_HANDLER_IS_NORETURN)
      BOOST_UNREACHABLE_RETURN(end());
#else
      return end();
#endif
   }

   //! <b>Effects</b>: Returns a const_iterator referring to the
   //!   same element as p.
   //!
   //! <b>Requires</b>: p points to an element in *this.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear in the number of active blocks in *this.
   const_iterator get_iterator(const_pointer p) const
   {
      return const_cast<hub*>(this)->get_iterator(p);
   }

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
private:
   template<typename U, typename A, typename F>
   friend std::pair<typename hub<U, A>::iterator, F> for_each_while(
      hub<U, A>&, F);
   template<typename U, typename A, typename P>
   friend typename hub<U, A>::size_type erase_if(hub<U, A>&, P);

   using block_typedefs = hub_detail::block_typedefs<allocator_type>;
   using block_base = typename block_typedefs::block_base;
   using block_base_pointer = typename block_typedefs::block_base_pointer;
   using const_block_base_pointer = 
      typename block_typedefs::const_block_base_pointer;
   using block = typename block_typedefs::block;
   using block_pointer = typename block_typedefs::block_pointer;
   using block_allocator = typename block_typedefs::block_allocator;
   using block_list = typename block_typedefs::block_list;
   //EBO: hub derives directly from block_allocator (like dtl::vector_alloc_holder).
   using allocator_base = block_allocator;
   using block_alloc_traits = allocator_traits<block_allocator>;
   using value_allocator = hub_detail::rebind_alloc_t<allocator_type, value_type>;
   using value_alloc_traits = allocator_traits<value_allocator>;
   using mask_type = typename block_base::mask_type;

   static constexpr int N = block_base::N;
   static constexpr mask_type full = block_base::full;

   block_allocator&       al() noexcept
   { return static_cast<block_allocator&>(*this); }
   const block_allocator& al() const noexcept
   { return static_cast<const block_allocator&>(*this); }

   struct reset_on_exit
   {
      ~reset_on_exit() { x.reset(); }

      hub& x;
   };

   hub(
      hub&& x, const allocator_type& al_, std::true_type /* equal allocs */) noexcept:
      allocator_base(al_), blist{std::move(x.blist)},
      num_blocks{x.num_blocks}, size_{x.size_}
   {
      x.num_blocks = 0;
      x.size_ = 0;
   }

   hub(
      hub&& x, const allocator_type& al_, std::false_type /* maybe unequal allocs */):
      hub{al_}
   {
      if(al() == x.al()) {
         blist = std::move(x.blist);
         num_blocks = x.num_blocks;
         size_ = x.size_;
         x.num_blocks = 0;
         x.size_ = 0;
      }
      else {
         reset_on_exit on_exit{x}; (void)on_exit;
         range_insert_impl(x.begin(), x.end(), [this] (T* p, iterator it) {
            block_alloc_traits::construct(al(), p, std::move(*it));
         });
      }
   }

   void move_assign(hub& x, std::true_type /* transfer structure */)
   {
      using pocma =
         hub_detail::pocma_t<allocator_type>;

      reset();
      hub_detail::move_assign_if(pocma{}, al(), x.al());
      blist = std::move(x.blist);
      num_blocks = x.num_blocks;
      size_ = x.size_;
      x.num_blocks = 0;
      x.size_ = 0;
   }

   void move_assign(hub& x, std::false_type /* maybe move data */)
   {
      if(al() == x.al()) {
         move_assign(x, std::true_type{});
      }
      else {
         reset_on_exit on_exit{x}; (void)on_exit;
         range_assign_impl(
            x.begin(), x.end(),
            [this] (T* p, iterator it) 
               { block_alloc_traits::construct(al(), p, std::move(*it)); },
            [] (T* p, iterator it) 
               { *p = std::move(*it); });
      }
   }

   static block_pointer 
   static_cast_block_pointer(block_base_pointer pbb) noexcept
   {
      return block::static_cast_block_pointer(pbb);
   }

   block_pointer create_new_available_block()
   {
      auto pb = block_alloc_traits::allocate(al(), 1);
      pb->mask = 0;
      BOOST_CONTAINER_TRY {
         value_allocator val(al());
         pb->data_ = value_alloc_traits::allocate(val, N);
      }
      BOOST_CONTAINER_CATCH(...) {
         block_alloc_traits::deallocate(al(), pb, 1);
         BOOST_CONTAINER_RETHROW;
      }
      BOOST_CONTAINER_CATCH_END
      blist.link_available_at_back(pb);
      ++num_blocks;
      return pb;
   }

   void delete_block(block_pointer pb) noexcept
   {
      value_allocator val(al());
      value_alloc_traits::deallocate(val, pb->data(), N);
      block_alloc_traits::deallocate(al(), pb, 1);
   }

   BOOST_CONTAINER_FORCEINLINE block_pointer retrieve_available_block(int& n)
   {
      if(BOOST_LIKELY(blist.next_available != blist.header())) {
         auto pb = static_cast_block_pointer(blist.next_available);
         n = dtl::unchecked_countr_one(pb->mask);
         return pb;
      }
      else {
         n = 0;
         return create_new_available_block();
      }
   }

   BOOST_CONTAINER_FORCEINLINE size_type destroy_all_in_nonempty_block(
      block_pointer pb) noexcept
   {
      BOOST_ASSERT(pb->mask != 0);
      return destroy_all_in_nonempty_block(pb, std::integral_constant<bool,
         std::is_trivially_destructible<T>::value &&
         (  hub_detail::is_std_allocator<block_allocator>::value ||
            hub_detail::is_std_pmr_polymorphic_allocator<block_allocator>::value ||
           !hub_detail::allocator_has_destroy<block_allocator, T*>::value )>{});
   }

   BOOST_CONTAINER_FORCEINLINE size_type destroy_all_in_nonempty_block(
      block_pointer pb, std::true_type /* trivial destruction */) noexcept
   {
      return (size_type)dtl::popcount(pb->mask);
   }

   BOOST_CONTAINER_FORCEINLINE size_type destroy_all_in_nonempty_block(
      block_pointer pb, std::false_type /* use allocator destroy */) noexcept
   {
      size_type s = (size_type)dtl::popcount(pb->mask);
      auto      mask = pb->mask;
      auto      pd = boost::movelib::to_raw_pointer(pb->data());
      BOOST_CONTAINER_UNROLL(4)
      do {
         auto n = dtl::unchecked_countr_zero(mask);
         block_alloc_traits::destroy(al(), pd + n);
         mask &= mask - 1;
      } while(mask);
      return s;
   }

   BOOST_CONTAINER_FORCEINLINE size_type destroy_all_in_full_block(
      block_pointer pb) noexcept
   {
      BOOST_ASSERT(pb->mask == full);
      auto pd = boost::movelib::to_raw_pointer(pb->data());
      int  n = 0;
      BOOST_CONTAINER_UNROLL(4)
      for(; n < N; ++n) {
         block_alloc_traits::destroy(al(), pd + n);
      }
      return (size_type)N;
   }

   void reset() noexcept
   {
      //empty blocks
      auto pbb = blist.prev_available;
      BOOST_CONTAINER_UNROLL(4)
      while(pbb != blist.header() && pbb->mask == 0) {
         auto pb = static_cast_block_pointer(pbb);
         pbb = pb->prev_available;
         BOOST_CONTAINER_HUB_PREFETCH(pbb);
         delete_block(pb);
      }

      //non-empty blocks
      pbb = blist.next;
      while(pbb != blist.header()) {
         auto pb = static_cast_block_pointer(pbb);
         pbb = pb->next;
         BOOST_IF_CONSTEXPR(!std::is_trivially_destructible<T>::value) {
            BOOST_CONTAINER_HUB_PREFETCH_BLOCK(pbb, block);
         }
         if(pb->mask == full) destroy_all_in_full_block(pb);
         else                 destroy_all_in_nonempty_block(pb);
         delete_block(pb);
      }
      blist.reset();
      num_blocks = 0;
      size_ = 0;
   }

   template<typename... Args>
   inline void construct_or_restore_capacity(
      value_type* p, block_base_pointer pbb, Args&&... args)
   {
      BOOST_CONTAINER_TRY {
         block_alloc_traits::construct(al(), p, std::forward<Args>(args)...);
      }
      BOOST_CONTAINER_CATCH(...) {
         restore_capacity_on_throw(pbb);
         BOOST_CONTAINER_RETHROW
      }
      BOOST_CONTAINER_CATCH_END
   }

   BOOST_NOINLINE void restore_capacity_on_throw(
      block_base_pointer pbb) noexcept
   {
      auto pb = static_cast_block_pointer(blist.next_available);
      if(pb != pbb) { //block freshly allocated -> restore capacity
         blist.unlink_available(pb);
         delete_block(pb);
         --num_blocks;
      }
   }

   BOOST_CONTAINER_FORCEINLINE void erase_impl(block_base_pointer pbb, int n) noexcept
   {
      auto pb = static_cast_block_pointer(pbb);
      block_alloc_traits::destroy(al(), boost::movelib::to_raw_pointer(pb->data() + n));
      if(BOOST_UNLIKELY(pb->mask == full)) blist.link_available_at_front(pb);
      pb->mask &= ~((mask_type)(1) << n);
      if(BOOST_UNLIKELY(pb->mask == 0)) {
         blist.unlink(pb);
         blist.unlink_available(pb);
         blist.link_available_at_back(pb); 
      }
      --size_;
   }

   template<typename Incrementable, typename Sentinel, typename Construct>
   void range_insert_impl(
      Incrementable first, Sentinel last, Construct construct_)
   {
      while(first != last) {
         int  n;
         auto pb = retrieve_available_block(n);
         for(; ; ) {
            construct_(boost::movelib::to_raw_pointer(pb->data() + n), first++);
            ++size_;
            if(BOOST_UNLIKELY(pb->mask == 0)) blist.link_at_back(pb);
            pb->mask |= pb->mask +1;
            if(pb->mask == full) {
               blist.unlink_available(pb);
               break;
            }
            else if(first == last) return;
            n = dtl::unchecked_countr_one(pb->mask);
         }
      }
   }

   template<
      typename Incrementable, typename Sentinel, 
      typename Construct, typename Insert
   >
   void range_assign_impl(
      Incrementable first, Sentinel last, Construct construct_, Insert insert_)
   {
      auto pbb = blist.next;
      int  n = -1;
      if(first != last) {
         //consume active blocks
         for(; pbb != blist.header(); pbb = pbb->next) {
            auto pb = static_cast_block_pointer(pbb);
            n = 0;
            for(mask_type bit = 1; bit; bit <<= 1, ++n) {
               if(pb->mask & bit) { //full slot
                  insert_(boost::movelib::to_raw_pointer(pb->data() + n), first++);
               }
               else { //empty slot
                  construct_(boost::movelib::to_raw_pointer(pb->data() + n), first++);
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
         //all active blocks consumed, keep inserting
         range_insert_impl(first, last, construct_);
      }
      else{
         //erase remaining original elements
         auto it = (n == -1)? const_iterator{pbb}: ++const_iterator{pbb, n};
         erase(it, cend());
      }
   }

   template<typename Compare>
   bool transfer_sort(Compare comp)
   {
      //transfer to a buffer, sort and transfer back
      if(size_ > 1) {
         hub_detail::buffer<T,allocator_type> buf(size_, al());
         if(!buf.data) return false;

         container::for_each(*this, [&] (value_type& x) {
            buf.emplace_back(std::move(x));
         });
         std::sort(buf.begin(), buf.end(), comp);
         container::for_each(*this, [&] (value_type& x) { 
            x = std::move(*buf.begin());
            buf.erase_front();
         });
      }
      return true;
   }

   struct sort_proxy
   {
      T*        p;
      size_type n;
   };

   template<typename Compare>
   bool proxy_sort(Compare comp)
   {
      //sort an array of (pointer, index) pairs and relocate according to it
      if(size_ > 1) {
         hub_detail::nodtor_unique_ptr<sort_proxy[]> p
            {static_cast<sort_proxy*>(
               ::operator new[](size_ * sizeof(sort_proxy), std::nothrow))};
         if(!p) return false;

         size_type i = 0;
         container::for_each(*this, [&] (value_type& x) {
            p[i] = {dtl::addressof(x), i};
            ++i;
         });

         std::sort(
            p.get(), p.get() + size_, 
            [&] (const sort_proxy& x, const sort_proxy& y) {
               return comp(const_cast<const T&>(*x.p), const_cast<const T&>(*y.p));
            });

         i = 0;
         for(; i < size_; ++i) {
            if(p[i].n != i) {
               T    x = std::move(*(p[i].p));
               auto j = i;
               do {
                  auto k = p[j].n;
                  *(p[j].p) = std::move(*p[k].p);
                  p[j].n = j;
                  j = k;
               } while(p[j].n != i);
               *(p[j].p) = std::move(x);
               p[j].n = j;
            }
         }
      }
      return true;
   }

   template<typename Compare>
   void compact_sort(Compare comp)
   {
      //compact elements and build an array of pointers to data chunks of N
      using sort_iterator = hub_detail::sort_iterator<T, (std::size_t)N>;

      if(size_ > 1) {
         std::size_t n = (std::size_t)((size_ + N - 1) / N);
         hub_detail::nodtor_unique_ptr<T*[]> p
            {static_cast<T**>(::operator new[](n * sizeof(T*)))};
         std::size_t i = 0;
         compact([&] (block_pointer pb) { 
            p[i++] = boost::movelib::to_raw_pointer(pb->data()); 
         });
         BOOST_ASSERT(i == n);

         std::sort(
            sort_iterator{p.get(), 0},
            sort_iterator{p.get(), (std::ptrdiff_t)size_}, comp);
      }
   }

   void compact() { compact([] (block_pointer) {}); }

   template<typename Track>
   void compact(Track track)
   {
      for(auto pbbx = blist.next; pbbx != blist.header(); ) {
         auto pbx = static_cast_block_pointer(pbbx);
         auto pbby = pbbx->next;
         if(pbx->mask != full) {
            do{
               if(pbby->mask == full) {
                  do {
                     track(static_cast_block_pointer(pbby));
                     pbby = pbby->next;
                  } while(pbby->mask == full);
                  blist.unlink(pbx);
                  blist.link_before(pbx, static_cast_block_pointer(pbby));
               }
               if(pbby == blist.header()) {
                  compact(pbx);
                  track(pbx);
                  return;
               }
               else{
                  auto pby = static_cast_block_pointer(pbby);
                  compact(pbx,pby);
                  if(pby->mask == 0) {
                     pbby = pby->next;
                     blist.unlink(pby);
                     blist.unlink_available(pby);
                     blist.link_available_at_back(pby); 
                  }
               }
            }while(pbx->mask != full);
            blist.unlink_available(pbx);
         }
         track(pbx);
         pbbx = pbby;
      }
   }

   void compact(block_pointer pbx, block_pointer pby)
   {
      auto cx = dtl::popcount(pbx->mask),
           cy = dtl::popcount(pby->mask);
      if(cx < cy) {
         std::swap(cx, cy);
         swap_payload(*pbx, *pby);
      }
      auto c = (std::min)(N - cx, cy);
      while(c--) {
         auto n = dtl::unchecked_countr_one(pbx->mask);
         auto m = N - 1 - dtl::unchecked_countl_zero(pby->mask);
         block_alloc_traits::construct(
            al(), boost::movelib::to_raw_pointer(pbx->data() + n), std::move(pby->data()[m]));
         block_alloc_traits::destroy(al(), boost::movelib::to_raw_pointer(pby->data() + m));
         pbx->mask |= pbx->mask + 1;
         pby->mask &= ~((mask_type)(1) << m);
      }
   }

   void compact(block_pointer pb)
   {
      for(; ; ) {
         auto n = dtl::unchecked_countr_one(pb->mask);
         auto m = N - 1 - dtl::unchecked_countl_zero(pb->mask);
         if(n > m) return;
         block_alloc_traits::construct(
            al(), boost::movelib::to_raw_pointer(pb->data() + n), std::move(pb->data()[m]));
         block_alloc_traits::destroy(al(), boost::movelib::to_raw_pointer(pb->data() + m));
         pb->mask |= pb->mask + 1;
         pb->mask &= ~((mask_type)(1) << m);
      }
   }

   block_list blist;
   size_type  num_blocks = 0;
   size_type  size_ = 0;

#endif //BOOST_CONTAINER_DOXYGEN_INVOKED
};

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

#if !defined(BOOST_NO_CXX17_DEDUCTION_GUIDES)

template<typename InputIterator>
hub(InputIterator, InputIterator)
   -> hub<typename std::iterator_traits<InputIterator>::value_type>;

template<typename InputIterator, typename Allocator>
hub(InputIterator, InputIterator, Allocator)
   -> hub<typename std::iterator_traits<InputIterator>::value_type, Allocator>;

//The (initializer_list) case is covered by the implicit guide of the
//initializer_list constructor (the allocator defaults to void); an explicit
//guide is only needed to deduce a user-supplied allocator, since the resolved
//in-class allocator type is a non-deduced context.
template<typename T, typename Allocator>
hub(std::initializer_list<T>, Allocator) -> hub<T, Allocator>;

#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
template<hub_detail::input_range_like R>
hub(from_range_t, R&&) -> hub<hub_detail::range_value_t<R> >;

template<hub_detail::input_range_like R, typename Allocator>
hub(from_range_t, R&&, Allocator) -> hub<hub_detail::range_value_t<R>, Allocator>;

#endif   //BOOST_CONTAINER_HUB_NO_RANGES

#endif   //BOOST_NO_CXX17_DEDUCTION_GUIDES

#endif   //BOOST_CONTAINER_DOXYGEN_INVOKED

//! <b>Effects</b>: Equivalent to x.swap(y).
//!
//! <b>Complexity</b>: Constant.
template<typename T, typename Allocator>
void swap(hub<T, Allocator>& x, hub<T, Allocator>& y)
   noexcept(noexcept(x.swap(y)))
{
   x.swap(y);
}

//! <b>Effects</b>: Erases all elements of x equal to value. Equivalent to
//!   erase_if(x, [&](const auto& e){ return e == value; }).
//!
//! <b>Returns</b>: The number of erased elements.
//!
//! <b>Complexity</b>: Linear in x.size().
template<typename T, typename Allocator, typename U = T>
typename hub<T, Allocator>::size_type
erase(hub<T, Allocator>& x, const U& value)
{
   return container::erase_if(
      x, [&](const T& v) -> bool { return v == value; });
}

//! <b>Effects</b>: Erases all elements of x for which pred returns true.
//!
//! <b>Returns</b>: The number of erased elements.
//!
//! <b>Complexity</b>: Linear in x.size().
//!
//! <b>Note</b>: Potentially faster than the naive erase loop due to internal
//!   optimizations.
template<typename T, typename Allocator, typename Predicate>
typename hub<T, Allocator>::size_type
erase_if(hub<T, Allocator>& x, Predicate pred)
{
   using hub_container = hub<T, Allocator>;
   using size_type = typename hub_container::size_type;
   using block = typename hub_container::block;

   auto s = x.size_;
   for(auto pbb = x.blist.next; pbb != x.blist.header(); ) {
      auto pb = x.static_cast_block_pointer(pbb);
      pbb = pb->next;
      BOOST_CONTAINER_HUB_PREFETCH_BLOCK(pbb, block);
      auto mask = pb->mask;
      do {
         auto n = dtl::unchecked_countr_zero(mask);
         if(pred(pb->data()[n])) x.erase_impl(pb, n);
         mask &= mask - 1;
      } while(mask);
   }
   return (size_type)(s - x.size_);
}

//! <b>Effects</b>: Applies f to every element in [first, last), in order.
//!   Equivalent to: while(first != last) f(*first++); return f;
//!
//! <b>Requires</b>: decltype(first) is the `iterator` or `const_iterator` of an
//!   instantiation of hub and [first, last) is a valid range.
//!
//! <b>Returns</b>: std::move(f).
//!
//! <b>Note</b>: Potentially faster than the equivalent loop thanks to internal
//!   unrolling and prefetching.
template< BOOST_CONTAINER_DOC1ST(class ImplDefinedHubIterator, typename ValuePtr), typename F>
BOOST_CONTAINER_FORCEINLINE F for_each
   ( BOOST_CONTAINER_DOC1ST(ImplDefinedHubIterator, hub_detail::iterator<ValuePtr>) first
   , BOOST_CONTAINER_DOC1ST(ImplDefinedHubIterator, hub_detail::iterator<ValuePtr>) last
   , F f)
{
   container::for_each_while(
      first, last, hub_detail::inline_return_true_ref_caller<F>{f});
   return f;
}

//! <b>Effects</b>: Applies f to every element of x. Equivalent to
//!   for_each(x.begin(), x.end(), std::ref(f)).
//!
//! <b>Returns</b>: std::move(f).
//!
//! <b>Note</b>: Potentially faster than range iteration thanks to internal
//!   unrolling and prefetching.
template<typename T, typename Allocator, typename F>
BOOST_CONTAINER_FORCEINLINE F for_each(hub<T, Allocator>& x, F f)
{
   container::for_each_while(
      x, hub_detail::inline_return_true_ref_caller<F>{f});
   return f;
}

//! <b>Effects</b>: Applies f to every element of x. Equivalent to
//!   for_each(x.cbegin(), x.cend(), std::ref(f)).
//!
//! <b>Returns</b>: std::move(f).
//!
//! <b>Note</b>: Potentially faster than range iteration thanks to internal
//!   unrolling and prefetching.
template<typename T, typename Allocator, typename F>
BOOST_CONTAINER_FORCEINLINE F for_each(const hub<T, Allocator>& x, F f)
{
   container::for_each_while(
      const_cast<hub<T, Allocator>&>(x),
      hub_detail::inline_return_true_ref_const_caller<F>{f});
   return f;
}

//! <b>Effects</b>: Applies f to the elements of [first, last) in order while f
//!   returns true. Equivalent to:
//!   while(first != last && f(*first)) ++first; return {first, std::move(f)};
//!
//! <b>Requires</b>: decltype(first) is the `iterator` or `const_iterator` of an
//!   instantiation of hub and [first, last) is a valid range.
//!
//! <b>Returns</b>: A pair with the iterator past the last visited element and
//!   std::move(f).
//!
//! <b>Note</b>: Potentially faster than the equivalent loop thanks to internal
//!   unrolling and prefetching.
template< BOOST_CONTAINER_DOC1ST(class ImplDefinedHubIterator, typename ValuePtr), typename F>
std::pair<BOOST_CONTAINER_DOC1ST(ImplDefinedHubIterator, hub_detail::iterator<ValuePtr>), F>
   for_each_while
   ( BOOST_CONTAINER_DOC1ST(ImplDefinedHubIterator, hub_detail::iterator<ValuePtr>) first
   , BOOST_CONTAINER_DOC1ST(ImplDefinedHubIterator, hub_detail::iterator<ValuePtr>) last
   , F f)
{
   using iterator = hub_detail::iterator<ValuePtr>;
   using block = typename iterator::block;
   using mask_type = typename iterator::mask_type;
   static constexpr auto full = iterator::full;

   if(BOOST_UNLIKELY(first == last)) return {last, std::move(f)};

   auto pbb = first.pbb,
        last_pbb = last.pbb;
   auto last_n = last.n;
   auto mask = pbb->mask & (full << first.n);

   for(; ;) {
      BOOST_CONTAINER_HUB_PREFETCH(&pbb->next->mask);
      auto is_last = mask_type(pbb == last_pbb);
      mask &= (is_last << last_n) - mask_type(1);
      BOOST_CONTAINER_HUB_PREFETCH(
         block::static_cast_block_pointer(pbb->next)->data());
      auto next_n = dtl::unchecked_countr_zero(pbb->next->mask);
      BOOST_CONTAINER_HUB_PREFETCH(
         block::static_cast_block_pointer(pbb->next)->data() + next_n);
      auto pd = block::static_cast_block_pointer(pbb)->data();
      BOOST_CONTAINER_UNROLL(4)
      while(mask) {
         auto n = dtl::unchecked_countr_zero(mask);
         if (!f(pd[n])) return {{pbb, n}, std::move(f)};
         mask &= mask - 1;
      }
      if(BOOST_UNLIKELY(is_last != 0)) return {last, std::move(f)};
      pbb = pbb->next;
      mask = pbb->mask;
   }
}

//! <b>Effects</b>: Applies f to the elements of x while f returns true.
//!   Equivalent to for_each_while(x.begin(), x.end(), std::ref(f)) with f moved
//!   into the returned pair.
//!
//! <b>Returns</b>: A pair with the iterator past the last visited element and
//!   std::move(f).
//!
//! <b>Note</b>: Potentially faster than range iteration thanks to internal
//!   unrolling and prefetching.
template<typename T, typename Allocator, typename F>
std::pair<typename hub<T, Allocator>::iterator, F>
for_each_while(hub<T, Allocator>& x, F f)
{
   using iterator = typename hub<T, Allocator>::iterator;
   using block = typename iterator::block;

   auto last_pbb = x.blist.header();
   for(auto pbb = x.blist.next; pbb != last_pbb; ) {
      BOOST_CONTAINER_HUB_PREFETCH(&pbb->next->mask);
      BOOST_CONTAINER_HUB_PREFETCH(
         block::static_cast_block_pointer(pbb->next)->data());
      auto next_n = dtl::unchecked_countr_zero(pbb->next->mask);
      BOOST_CONTAINER_HUB_PREFETCH(
         block::static_cast_block_pointer(pbb->next)->data() + next_n);
      auto pd = block::static_cast_block_pointer(pbb)->data();
      auto mask = pbb->mask;
      BOOST_CONTAINER_UNROLL(4)
      while(mask) {
         auto n = dtl::unchecked_countr_zero(mask);
         if (!f(pd[n])) return {{pbb, n}, std::move(f)};
         mask &= mask - 1;
      }
      pbb = pbb->next;
      mask = pbb->mask;
   }
   return {{last_pbb}, std::move(f)};
}

//! <b>Effects</b>: Applies f to the elements of x while f returns true.
//!   Equivalent to for_each_while(x.cbegin(), x.cend(), std::ref(f)) with f moved
//!   into the returned pair.
//!
//! <b>Returns</b>: A pair with the iterator past the last visited element and
//!   std::move(f).
//!
//! <b>Note</b>: Potentially faster than range iteration thanks to internal
//!   unrolling and prefetching.
template<typename T, typename Allocator, typename F>
std::pair<typename hub<T, Allocator>::const_iterator, F>
BOOST_CONTAINER_FORCEINLINE for_each_while(const hub<T, Allocator>& x, F f)
{
   return {
      container::for_each_while(
         const_cast<hub<T, Allocator>&>(x), 
         hub_detail::inline_ref_const_caller<F>{f}).first,
      std::move(f)};
}

} //namespace container

} //namespace boost

#if defined(BOOST_MSVC)
#pragma warning(pop) //C4714
#endif

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_HUB_HPP

