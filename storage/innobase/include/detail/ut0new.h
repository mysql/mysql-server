/*****************************************************************************

Copyright (c) 2020 Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/detail/ut0new.h
 Implementation bits and pieces of include/ut0new.h */

#ifndef detail_ut0new_h
#define detail_ut0new_h

#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include "my_compiler.h"
#include "storage/innobase/include/ut0tuple.h"

namespace ut {
namespace detail {

struct Aligned_alloc_impl {
  /** Block of memory returned by this functor will have an additional
      (metadata) segment (at no additional cost of higher memory consumption)
      which is guaranteed to be this big, and which can be used to store
      whatever arbitrary data. See Aligned_alloc and Aligned_alloc_arr for
      exemplary usages of it.
   */
  static constexpr uint32_t metadata_size = alignof(max_align_t);

  /** Dynamically allocates storage of given size and at the address aligned to
      the requested alignment.

      It is guaranteed that storage allocated by this functor is always
      (size + alignment) big _and_ that there is always
      alignof(std::max_align_t) spare bytes within that segment which can be
      freely used. This means that pointer which is returned by this function
      can always be safely reversed by alignof(std::max_align_t) bytes and hence
      make this sub-segment accessible to any subsequent implementation.

      Reversing the pointer for a value which is bigger than
      alignof(std::max_align_t) bytes is in certain cases possible and can be
      checked by inspecting the returned offset value. Some more specialized
      implementations can take advantage of that fact too.

      This property is very important because it can be taken advantage of by
      other implementations so they are able to store whatever metadata they
      would like into this segment of alignof(std::max_align_t) bytes, or FWIW
      (pointer - offset) bytes in more specialized cases.

      For example, let's say that size=10, alignment=32, and underlying
      allocation function used by this function implementation returns a pointer
      at address 128. This address must be a multiple of
      alignof(std::max_align_t) which in this example is 16.

           ---------------------------
           | VAR |   META  |   DATA  |
           ---------------------------
           128   144       160     170

      DATA is an actual data which has been requested with given size (10) and
      alignment (32).
      META is the alignof(std::max_align_t) segment that can always be freely
      used by other implementations.
      VAR is the leftover variable segment of bytes that specialized
      implementations can further make use of by deducing its size from returned
      offset.

      See Aligned_alloc and Aligned_alloc_arr implementations to see an example
      of how META segment can be used.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @param[in] alignment Alignment requirement for storage to be allocated.
      Must be power of two and larger than alignof(std::max_align_t).
      @return {pointer, offset} where pointer is a pointer to dynamically
      allocated storage aligned to requested alignment, and offset is distance
      in bytes from pointer which has been originally returned by underlying
      dynamic allocation function (e.g. std::malloc). Otherwise {nullptr, 0} if
      dynamic storage allocation failed.
   */
  inline std::pair<void *, std::size_t> operator()(
      std::size_t size, std::size_t alignment) noexcept {
    // This API is only about the extended alignments. Non-extended are
    // already handled with std::malloc.
    assert(alignment > alignof(std::max_align_t));

    // Here we can take advantage of the fact that std::malloc is always
    // guaranteed to return suitably aligned pointer P. In other words, this
    // means that pointer P will be aligned to the value of
    // alignof(std::max_align_t). That value on Linux implementations is
    // typically 8 bytes (x86) or 16 bytes (x86-64) but this is completely
    // implementation-defined. Windows implementation, for example, defines this
    // value to be 8 bytes for both x86 _and_ x86-64.
    //
    // Question which arises is how many bytes do we need to ask std::malloc in
    // order to satisfy the requested alignment, A, and requested size, S.
    //
    // Short answer is (size + alignment) and you can skip the following section
    // if you're not interested in how we got to that value.
    //
    // Long answer is that we can model the problem mathematically as follows:
    //    A = alignment
    //    S = size
    //    P = std::malloc(N);
    //    P % alignof(std::max_align_t) = 0;
    // where
    //    N is our unknown and for which we can only say that:
    //      N = S + K
    //    is true where K is also some unknown value.
    //
    // (A) In cases when P % A = 0, we know that we are already at suitable
    // address so we can deduce that:
    //    K = 0
    // from which it follows that:
    //    N = S + K = S
    //
    // (B) In cases when P % A != 0, our K must be sufficiently large so that
    //    <P, P + K]
    // range contains value, P', so that
    //    P' % A = 0
    // If K = A, then
    //    <P, P + A]
    // which can be expanded as
    //    <P, P + 1, P + 2, P + 3, ..., P + A]
    //
    // If we know that
    //    P % A != 0
    // then we also know that
    //    (P + A) % A = P % A + A % A = P % A + 0 = P % A != 0
    // which implies that our P' value is in
    //    <P, P + A>
    // range.
    //
    // Given that
    //    P % alignof(std::max_align_t) = 0
    //    P' % A = 0
    //    A % alignof(std::max_align_t) = 0
    //    P' % alignof(std::max_align_t) = 0
    // it follows that our P' value can only be found at
    //    P' = P + L*alignof(std::max_align_t)
    // increments where L is some constant value in range of
    //    L = [1, x>
    // Expanding <P, P + A> into what we know so far:
    //    <P, ..., P + L*alignof(std::max_align_t), ..., P + A>
    // it follows that
    //    P + L*alignof(std::max_align_t) < P + A
    // which equals to
    //    L*alignof(std::max_align_t) < A
    //
    // We now just need to prove that
    //    L*alignof(std::max_align_t) < A
    // is always true for some L value in which case this would mean that our
    // <P, P + A> range contains at least one value for which P' % = 0.
    //
    // Picking the first available L value from L = [1, x> range,
    // we will get
    //    1*alignof(std::max_align_t) < A
    // Given that
    //    A > 2*alignof(std::max_align_t) - 1
    // must be true, it follows that
    //    1*alignof(std::max_align_t) < 2*alignof(std::max_align_t) - 1 < A
    // and
    //    1*alignof(std::max_align_t) < 2*alignof(std::max_align_t) - 1
    //    alignof(std::max_align_t) > 1
    // is always true.
    //
    // To conclude:
    //    (1) N = S       for P % A = 0
    //    (2) N = S + A   for P % A != 0
    //
    // Given that P is a runtime value which we cannot know upfront we must opt
    // for N = S + A.
    const std::size_t data_len = size + alignment;
    void *mem = std::malloc(data_len);
    if (!mem) return {nullptr, 0};

    // To guarantee that storage allocated by this function is as advertised
    // (exactly (size + alignment) big with at least alignof(std::max_align_t)
    // spare bytes to be used from within that segment) we must handle the N = S
    // case (see above) and offset the memory by 1 byte. Given the above proof
    // we know that the next suitable address aligned to requested alignment
    // must be at least alignof(std::max_align_t) bytes far away.
    void *buf =
        reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(mem) + 1);
    std::size_t buf_size = data_len - 1;

    // TODO(jbakamovic): wrap following std::align expression with ut_a(...)
    // after we clean up circular-dependencies from our headers. Until then, it
    // is not possible as including ut0dbg.h header makes the build fail in
    // mysterious ways.
    MY_ATTRIBUTE((unused))
    auto ret = std::align(alignment, size, buf, buf_size);
    assert(ret != nullptr);

    return {buf, reinterpret_cast<std::uintptr_t>(buf) -
                     reinterpret_cast<std::uintptr_t>(mem)};
  }
};

/** Aligned allocation routines.

    They're implemented in terms of Aligned_alloc_impl, and given the guarantees
    it provides, Aligned_alloc::alloc() will encode offset into the metadata
    section without sacrificing memory or making the implementation or end usage
    more complex.

    Serializing offset into the metadata is what will enable
    Aligned_alloc::free() to later on recover original pointer returned by
    the underlying Aligned_alloc_impl allocation mechanism (std::malloc) and
    consequently be able to appropriately release it (std::free).
 */
struct Aligned_alloc {
 private:
  /** Aligned_alloc memory layout representation of metadata segment.

      ---------------------------
      | VAR |   META  |   DATA  |
      ---------------------------
                  \
                   --------------------------------
                   |   NOT USED   |     OFFSET    |
                   --------------------------------
                    \              \               \
                     0              \               \
                         alignof(max_align_t) / 2    \
                                                      \
                                           alignof(max_align_t) - 1
   */
  struct Metadata {
    /** Convenience type that we will be using to serialize necessary details
        into the Aligned_alloc metadata segment.
     */
    using offset_t = std::uint32_t;

    /** Helper function which stores the metadata we want to persist (offset).
        It also makes sure that we do not touch more space than what is
        available and that is Aligned_alloc_impl::metadata_size bytes.
     */
    static inline void store(void *buf, std::size_t offset) noexcept {
      static_assert(sizeof(offset_t) <= Aligned_alloc_impl::metadata_size,
                    "Aligned_alloc_impl provides a strong guarantee "
                    "of only up to Aligned_alloc_impl::metadata_size bytes.");
      assert(offset <= std::numeric_limits<decltype(offset)>::max());
      *ptr_to_offset(buf) = offset;
    }
    /** Helper function which recovers the metadata (offset) we previously
        stored.
     */
    static inline offset_t offset(void *buf) noexcept {
      return *ptr_to_offset(buf);
    }
    /** Helper function which deduces the original pointer returned by
        Aligned_alloc_impl
     */
    static inline void *deduce(void *buf) noexcept {
      return reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(buf) -
                                      *ptr_to_offset(buf));
    }
    /** Helper accessor function to metadata (offset). */
    static inline offset_t *ptr_to_offset(void *buf) noexcept {
      return reinterpret_cast<offset_t *>(buf) - 1;
    }
  };

 public:
  /** Dynamically allocates storage of given size and at the address aligned
      to the requested alignment.

       @param[in] size Size of storage (in bytes) requested to be allocated.
       @param[in] alignment Alignment requirement for storage to be allocated.
       @return Pointer to the allocated storage. nullptr if dynamic storage
       allocation failed.
   */
  static inline void *alloc(std::size_t size, std::size_t alignment) {
    auto ret = Aligned_alloc_impl()(size, alignment);
    // We are here taking advantage of Aligned_alloc_impl(S, A) for which
    // we know that it will always return pointer P and offset O such that:
    //   1. (P - O) expression is always well-defined.
    //   2. And O is never less than alignof(std::max_align_t), that is
    //      Aligned_alloc_impl::metadata_size.
    //
    // Practically, this means that we can encode whatever metadata we want
    // into [P - O, P> segment of memory whose length corresponds to
    // the value of alignof(std::max_align_t). Commonly, this value is 8 bytes
    // on 32-bit platforms and 16 bytes on 64-bit platforms.
    //
    // Here we encode the offset so we can later on recover the
    // original pointer, P' = (P - O), from within Aligned_alloc::free(P)
    // context.
    if (ret.first) {
      Metadata::store(ret.first, ret.second);
    }
    return ret.first;
  }

  /** Releases storage dynamically allocated through Aligned_alloc::alloc().
      @param[in] ptr Pointer to storage allocated through Aligned_alloc::alloc()
   */
  static inline void free(void *ptr) noexcept {
    // Here we make use of the offset which has been encoded by
    // Aligned_alloc::alloc() to be able to deduce the original pointer and
    // simply forward it to std::free.
    std::free(Metadata::deduce(ptr));
  }
};

/** Aligned allocation routines specialized for arrays.

    They're implemented in terms of Aligned_alloc_impl, and given the
    guarantees it provides, Aligned_alloc_arr::alloc() will encode offset and
    number of array elements into the metadata section without sacrificing
    memory or making the implementation or end usage more complex.

    Serializing offset into the metadata is what will enable
    Aligned_alloc_arr::free() to later on recover original pointer returned by
    the underlying Aligned_alloc_impl allocation mechanism (std::malloc) and
    consequntly be able to appropriately release it (std::free).

    Serializing number of elements of an array is what will enable higher-kinded
    functions such as ut::aligned_array_delete() to:
       * Invoke neccessary number of destructors.
       * Remove burden from end users having to remember and carry the array
         size all around the code. This is equivalent to what we find in other
         standard implementations. For example, new int x[10] is always released
         without passing the array size: delete[] x; The same holds with this
         design.
 */
struct Aligned_alloc_arr {
 private:
  /** Aligned_alloc_arr memory layout representation of metadata segment.

      ---------------------------
      | VAR |   META  |   DATA  |
      ---------------------------
                  \
                   --------------------------------
                   |  N_ELEMENTS  |     OFFSET    |
                   --------------------------------
                    \              \               \
                     0              \               \
                         alignof(max_align_t) / 2    \
                                                      \
                                           alignof(max_align_t) - 1
   */
  struct Metadata {
    /** Convenience types that we will be using to serialize necessary details
        into the Aligned_alloc_arr metadata segment.
     */
    using offset_t = std::uint32_t;
    using n_elements_t = std::uint32_t;

    /** Helper function which stores the metadata we want to persist (offset and
        number of elements in an array). It also makes sure that we do not
        touch more space than what is available and that is
        Aligned_alloc_impl::metadata_size bytes.
     */
    static inline void store(void *buf, std::size_t offset,
                             std::size_t n_elements) noexcept {
      static_assert(sizeof(offset_t) + sizeof(n_elements_t) <=
                        Aligned_alloc_impl::metadata_size,
                    "Aligned_alloc_impl provides a strong guarantee "
                    "of only up to Aligned_alloc_impl::metadata_size bytes.");
      assert(offset <= std::numeric_limits<offset_t>::max());
      assert(n_elements <= std::numeric_limits<n_elements_t>::max());
      *ptr_to_offset(buf) = offset;
      *ptr_to_n_elements(buf) = n_elements;
    }
    /** Helper function which recovers the metadata (offset) we previously
        stored.
     */
    static inline offset_t offset(void *buf) noexcept {
      return *ptr_to_offset(buf);
    }
    /** Helper function which recovers the metadata (number of elements of an
        array) we previously stored
      */
    static inline n_elements_t n_elements(void *buf) noexcept {
      return *ptr_to_n_elements(buf);
    }
    /** Helper function which deduces the original pointer returned by
        Aligned_alloc_impl
     */
    static inline void *deduce(void *ptr) noexcept {
      return reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(ptr) -
                                      *ptr_to_offset(ptr));
    }
    /** Helper accessor function to metadata (offset). */
    static inline offset_t *ptr_to_offset(void *buf) noexcept {
      return reinterpret_cast<offset_t *>(buf) - 1;
    }
    /** Helper accessor function to metadata (number of elements of an array).
     */
    static inline n_elements_t *ptr_to_n_elements(void *buf) noexcept {
      return reinterpret_cast<n_elements_t *>(buf) - 2;
    }
  };

 public:
  /** Dynamically allocates storage of given size and count at the address
      aligned to the requested alignment.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @param[in] count Number of elements in an array.
      @param[in] alignment Alignment requirement for storage to be allocated.
      @return Pointer to the allocated storage. nullptr if dynamic storage
      allocation failed.
   */
  static inline void *alloc(std::size_t size, std::size_t count,
                            std::size_t alignment) noexcept {
    auto ret = Aligned_alloc_impl()(size * count, alignment);
    // Similarly to what we do in Aligned_alloc::alloc(S, A), here we encode
    // two different things:
    //   1. offset, so we can later on recover the original pointer,
    //      P' = (P - O), from within Aligned_alloc_arr::free(P) context.
    //   2. number of elements in array, so we can invoke corresponding number
    //      of element destructors and remove the burden from end users carrying
    //      this piece of information.
    if (ret.first) {
      Metadata::store(ret.first, ret.second, count);
    }
    return ret.first;
  }

  /** Releases storage dynamically allocated through Aligned_alloc_arr::alloc().

      @param[in] ptr Pointer to storage allocated through
      Aligned_alloc_arr::alloc()
   */
  static inline void free(void *ptr) noexcept {
    // Here we make use of the offset which has been encoded by
    // Aligned_alloc_arr::alloc() to be able to deduce the original pointer and
    // simply forward it to std::free.
    std::free(Metadata::deduce(ptr));
  }

  /** Returns the size of an array.

      @param[in] ptr Pointer to storage allocated through
      Aligned_alloc_arr::alloc()
      @return Number of elements of given array.
   */
  static inline Metadata::n_elements_t n_elements(void *ptr) {
    return Metadata::n_elements(ptr);
  }
};

/** Generic utility function which invokes placement-new statement on type T.
    Arguments to be passed to T constructor are contained within tuple-like
    container. To be able to unpack arguments from tuple-like container at
    compile-time, index sequence is used.
 */
template <typename T, typename Tuple, size_t... Args_index_seq>
inline void invoke_placement_new(void *mem, size_t offset, Tuple &&args,
                                 std::index_sequence<Args_index_seq...>) {
  new (reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(mem) + offset))
      T(std::get<Args_index_seq>(std::forward<Tuple>(args))...);
}

/** Compile-time for-loop which generates Count number of placement-new
    statements.
 */
template <size_t I, const size_t Count, typename T, size_t Offset,
          const size_t N_args_per_T, size_t Tuple_idx, typename Tuple>
struct Loop {
  static void run(void *mem, Tuple &&args) {
    using N_args_seq = std::make_index_sequence<N_args_per_T>;
    invoke_placement_new<T>(
        mem, Offset,
        select_from_tuple<Tuple_idx, Tuple_idx + N_args_per_T>(
            std::forward<Tuple>(args)),
        N_args_seq{});
    Loop<I + 1, Count, T, Offset + sizeof(T), N_args_per_T,
         Tuple_idx + N_args_per_T, Tuple>::run(mem, std::forward<Tuple>(args));
  }
};

/** Specialization which denotes that we are at the end of the loop. */
template <size_t Count, typename T, size_t Offset, size_t N_args_per_T,
          size_t Tuple_idx, typename Tuple>
struct Loop<Count, Count, T, Offset, N_args_per_T, Tuple_idx, Tuple> {
  static void run(void *mem, Tuple &&args) {}
};
}  // namespace detail
}  // namespace ut

#endif /* detail_ut0new_h */
