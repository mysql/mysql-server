/*****************************************************************************

Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

/** @file include/detail/ut/aligned_alloc.h
 Implementation bits and pieces for aligned allocations. */

#ifndef detail_ut_aligned_alloc_h
#define detail_ut_aligned_alloc_h

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

#include "my_compiler.h"
#include "storage/innobase/include/detail/ut/allocator_traits.h"
#include "storage/innobase/include/detail/ut/helper.h"
#include "storage/innobase/include/detail/ut/pfs.h"

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

  /** Alias that we will be using to denote ptr to DATA segment. */
  using data_segment_ptr = void *;

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

           ------------------------------
           | VARLEN |   META  |   DATA  |
           ------------------------------
           128      144       160     170

      DATA is an actual data which has been requested with given size (10) and
      alignment (32).
      META is the alignof(std::max_align_t) segment that can always be freely
      used by other implementations.
      VARLEN is the leftover variable-length segment that specialized
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
  template <bool Zero_initialized>
  static inline std::pair<data_segment_ptr, std::size_t> alloc(
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
    void *mem = Alloc_fn::alloc<Zero_initialized>(data_len);
    if (unlikely(!mem)) return {nullptr, 0};

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
    [[maybe_unused]] auto ret = std::align(alignment, size, buf, buf_size);
    assert(ret != nullptr);

    return {buf, reinterpret_cast<std::uintptr_t>(buf) -
                     reinterpret_cast<std::uintptr_t>(mem)};
  }

  /** Releases storage allocated through alloc().

      @param[in] ptr data_segment_pointer decreased by offset bytes. Both are
      obtained through alloc().
  */
  static inline void free(void *ptr) noexcept { Alloc_fn::free(ptr); }
};

/** Memory layout representation of metadata segment guaranteed by
    the inner workings of Aligned_alloc_impl.

     ----------------------------------------------------
     | VARLEN | ALIGNED-ALLOC-META |    ... DATA ...    |
     ----------------------------------------------------
                 ^                  ^
                 |                  |
                 |                  |
                 |         ptr returned by Aligned_alloc_impl
                 |
                --------------------------------
                |    META_2    |     META_1    |
                --------------------------------
                 \                              \
                  0                              \
                                                  \
                                        alignof(max_align_t) - 1

   VARLEN and ALIGNED-ALLOC-META are direct byproduct of Aligned_alloc_impl
   layout and guarantees.

   VARLEN is the leftover variable-length segment that specialized
   implementations can further make use of by deducing its size from returned
   offset. Not used by this implementation.

   ALIGNED-ALLOC-META is the segment which this abstraction is about. It
   can hold up to sizeof(META_1) + sizeof(META_2) bytes which is, due to
   Aligned_alloc_impl guarantees, at most alignof(max_align_t) bytes large.
   Providing larger data than supported is not possible and it is guarded
   through the means of static_assert. META_1 and META_2 fields can be
   arbitrarily sized meaning that they can even be of different sizes each.

   DATA is an actual segment which will keep the user data.
  */
template <typename Meta_1_type, typename Meta_2_type>
struct Aligned_alloc_metadata {
  /** Convenience types that we will be using to serialize necessary details
      into the metadata segment.
   */
  using meta_1_t = Meta_1_type;
  using meta_2_t = Meta_2_type;
  using unaligned_meta_2_t = meta_2_t;

  /** Metadata size */
  static constexpr auto allocator_metadata_size =
      sizeof(meta_1_t) + sizeof(meta_2_t);
  /** Max metadata size */
  static constexpr auto max_metadata_size = Aligned_alloc_impl::metadata_size;
  /** Bail out if we cannot fit the requested data size. */
  static_assert(allocator_metadata_size <= max_metadata_size,
                "Aligned_alloc_impl provides a strong guarantee "
                "of only up to Aligned_alloc_impl::metadata_size bytes.");

  /** Helper function which stores user-provided detail into the META_1 field.
   */
  static inline void meta_1(Aligned_alloc_impl::data_segment_ptr data,
                            std::size_t meta_1_v) noexcept {
    assert(meta_1_v <= std::numeric_limits<meta_1_t>::max());
    *ptr_to_meta_1(data) = meta_1_v;
  }
  /** Helper function which stores user-provided detail into the META_2 field.
   */
  static inline void meta_2(Aligned_alloc_impl::data_segment_ptr data,
                            std::size_t meta_2_v) noexcept {
    assert(meta_2_v <= std::numeric_limits<meta_2_t>::max());
    meta_2_t meta_2_v_typed = meta_2_v;
    memcpy(ptr_to_meta_2(data), &meta_2_v_typed, sizeof(meta_2_t));
  }
  /** Helper function which recovers the information user previously stored in
      META_1 field.
   */
  static inline meta_1_t meta_1(
      Aligned_alloc_impl::data_segment_ptr data) noexcept {
    return *ptr_to_meta_1(data);
  }
  /** Helper function which recovers the information user previously stored in
      META_2 field.
   */
  static inline meta_2_t meta_2(
      Aligned_alloc_impl::data_segment_ptr data) noexcept {
    meta_2_t meta_2_v;
    memcpy(&meta_2_v, ptr_to_meta_2(data), sizeof(meta_2_t));
    return meta_2_v;
  }

 private:
  /** Helper accessor function to metadata (offset). */
  static inline meta_1_t *ptr_to_meta_1(
      Aligned_alloc_impl::data_segment_ptr data) noexcept {
    return reinterpret_cast<meta_1_t *>(data) - 1;
  }
  /** Helper accessor function to metadata (data length). */
  static inline unaligned_meta_2_t *ptr_to_meta_2(
      Aligned_alloc_impl::data_segment_ptr data) noexcept {
    return reinterpret_cast<meta_2_t *>(ptr_to_meta_1(data)) - 1;
  }
};

/** Aligned allocation routines.

    They're implemented in terms of Aligned_alloc_impl (and
    Aligned_alloc_metadata), and given the guarantees it provides,
    Aligned_alloc::alloc() is able to encode offset and requested
    allocation datalen into the metadata section without sacrificing memory or
    making the implementation or end usage more complex.

    Serializing offset into the metadata is what will enable
    Aligned_alloc::free() to later on recover original pointer returned by
    the underlying Aligned_alloc_impl allocation mechanism (std::malloc,
    std::calloc) and consequently be able to appropriately release it
    (std::free).

    Serializing requested allocation datalen into the metadata is what will
    enable higher-kinded functions, implemented on top of Aligned_alloc, to
    take necessary actions such as cleaning up the resources by invoking
    appropriate number of destructors of non-trivially-destructible types.
    Otherwise, this would create a burden on end users by having to remember and
    carry the array size all around the code. This is equivalent to what we find
    in other standard implementations. For example, new int x[10] is always
    released without passing the array size: delete[] x; The same holds with
    this design.

    Memory layout representation looks like the following:

     ----------------------------------------------------
     | VARLEN | ALIGNED-ALLOC-META |    ... DATA ...    |
     ----------------------------------------------------
                 ^                  ^
                 |                  |
                 |                  |
                 |          ptr returned by Aligned_alloc_impl
                 |
                -----------------------------------
                |   DATALEN    |   VARLEN-OFFSET  |
                -----------------------------------
                 \                                 \
                  0                                 \
                                                     \
                                           alignof(max_align_t) - 1

    VARLEN and ALIGNED-ALLOC-META are direct byproduct of Aligned_alloc_impl
    layout and guarantees.

    VARLEN is the leftover variable segment of bytes that specialized
    implementations can further make use of by deducing its size from returned
    offset. Not used by this implementation.

    DATALEN field in ALIGNED-ALLOC-META segment encodes the total length of DATA
    segment, which is the actual allocation size that client code has requested.

    VARLEN-OFFSET in ALIGNED-ALLOC-META segment encodes the offset to VARLEN
    segment which represents the original pointer obtained by underlying
    allocation Aligned_alloc_impl mechanism.
 */
struct Aligned_alloc : public allocator_traits<false> {
  using allocator_metadata = Aligned_alloc_metadata<uint32_t, uint32_t>;

  /** Dynamically allocates storage of given size and at the address aligned
      to the requested alignment.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @param[in] alignment Alignment requirement for storage to be allocated.
      @return Pointer to the allocated storage. nullptr if dynamic storage
      allocation failed.
   */
  template <bool Zero_initialized>
  static inline void *alloc(std::size_t size, std::size_t alignment) {
    auto ret = Aligned_alloc_impl::alloc<Zero_initialized>(size, alignment);
    if (likely(ret.first)) {
      // We are here taking advantage of Aligned_alloc_impl(S, A) for which
      // we know that it will always return pointer P and offset O such that:
      //   1. (P - O) expression is always well-defined.
      //   2. And O is never less than alignof(std::max_align_t), that is
      //      Aligned_alloc_impl::metadata_size.
      //
      // Practically, this means that we can encode whatever metadata we want
      // into [P - O, P> segment of memory whose length corresponds to
      // the value of alignof(std::max_align_t). Commonly, this value is 8 bytes
      // on 32-bit platforms and 16 bytes on 64-bit platforms but not always
      // (e.g. Windows) and which is why we have to handle it in more generic
      // way to be fully portable.
      //
      // Here we encode the offset so we can later on recover the
      // original pointer, P' = (P - O), from within Aligned_alloc::free(P)
      // context. Similarly, we encode the requested allocation datalen.
      allocator_metadata::meta_1(ret.first, ret.second);
      allocator_metadata::meta_2(ret.first, size);
    }
    return ret.first;
  }

  /** Releases storage dynamically allocated through
      Aligned_alloc::alloc().

      @param[in] data Pointer to storage allocated through
      Aligned_alloc::alloc()
   */
  static inline void free(Aligned_alloc_impl::data_segment_ptr data) noexcept {
    if (unlikely(!data)) return;
    // Here we make use of the offset which has been encoded by
    // Aligned_alloc::alloc() to be able to deduce the original pointer and
    // simply forward it to std::free.
    Aligned_alloc_impl::free(deduce(data));
  }

  /** Returns the number of bytes requested to be allocated.

      @param[in] data Pointer to storage allocated through
      Aligned_alloc::alloc()
      @return Number of bytes.
   */
  static inline allocator_metadata::meta_2_t datalen(
      Aligned_alloc_impl::data_segment_ptr data) {
    // Deducing the datalen field is straightforward, provided that
    // data points to the DATA segment, which it is expected to do so.
    return allocator_metadata::meta_2(data);
  }

 private:
  /** Helper function which deduces the original pointer returned by
      Aligned_alloc_impl from a pointer which is passed to us by the call-site.
   */
  static inline void *deduce(
      Aligned_alloc_impl::data_segment_ptr data) noexcept {
    // To recover the original pointer we just need to read the offset
    // we have serialized into the first (allocator) metadata field.
    auto offset = allocator_metadata::meta_1(data);
    return reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(data) -
                                    offset);
  }
};

/** Aligned allocation routines which are instrumented through PFS
    (performance-schema).

    They're implemented in terms of Aligned_alloc_impl (and
    Aligned_alloc_metadata), and given the guarantees it provides,
    Aligned_alloc::alloc() is able to encode offset and requested
    allocation datalen into the metadata section without sacrificing memory or
    making the implementation or end usage more complex.

    Serializing offset into the metadata is what will enable
    Aligned_alloc_pfs::free() to later on recover original pointer returned
    by the underlying Aligned_alloc_impl allocation mechanism (std::malloc,
    std::calloc) and consequently be able to appropriately release it
    (std::free).

    Serializing requested allocation datalen into the metadata is what will
    enable higher-kinded functions, implemented on top of
    Aligned_alloc_pfs, to take necessary actions such as cleaning up the
    resources by invoking appropriate number of destructors of
    non-trivially-destructible types. Otherwise, this would create a burden on
    end users by having to remember and carry the array size all around the
    code. This is equivalent to what we find in other standard implementations.
    For example, new int x[10] is always released without passing the array
    size: delete[] x; The same holds with this design.

    PFS-wise this allocation routine will be storing the information that PFS
    needs to do its own work:
     - Owning thread
     - Total length of bytes allocated
     - Key

    Memory layout representation looks like the following:

  ------------------------------------------------------------------------------
  | VARLEN1 | ALIGNED-ALLOC-META | PFS-META | VARLEN2 | PFS-META-OFFSET | DATA |
  ------------------------------------------------------------------------------
                ^                 ^    ^                                 ^
                |                 |    |                                 |
                |                 |   ---------------------------        |
                |                 |   | OWNER |  DATALEN' | KEY |        |
                |                 |   ---------------------------        |
                |                 |                                      |
                |            ptr returned by                             |
                |           Aligned_alloc_impl                           |
                |                                                        |
                |                                ptr to be returned to call-site
                |                                   will be pointing here
                |
               ------------------------------
               |  DATALEN  | VARLEN1-OFFSET |
               ------------------------------
                \                            \
                 0                            \
                                               \
                                     alignof(max_align_t) - 1

    VARLEN1 and ALIGNED-ALLOC-META are direct byproduct of Aligned_alloc_impl
    (and Aligned_alloc_metadata) layout and guarantees.

    VARLEN1 is the leftover variable-length segment that specialized
    implementations can further make use of by deducing its size from returned
    offset. Not used by this implementation.

    DATALEN field in ALIGNED-ALLOC-META segment encodes the total length of DATA
    segment, which is the actual allocation size that client code has requested.

    VARLEN1-OFFSET in ALIGNED-ALLOC-META segment encodes the offset to VARLEN1
    segment which represents the original pointer obtained by underlying
    allocation Aligned_alloc_impl mechanism.

    PFS-META, VARLEN2 and PFS-META-OFFSET are memory layout representation of
    PFS_metadata.

    OWNER field encode the owning thread. DATALEN' field encodes total size
    of memory consumed and not only the size of the DATA segment. KEY field
    encodes the PFS/PSI key.

    VARLEN2 is the leftover variable-length segment that specialized
    implementations can further make use of by deducing its size from the
    following formulae: requested_alignment - sizeof(PFS-META-OFFSET) -
    sizeof(PFS-META). In code that would be alignment -
    PFS_metadata::size. Not used by this implementation.

    PFS-META-OFFSET is a field which allows us to recover the pointer to
    PFS-META segment from a pointer to DATA segment. Having a pointer to
    PFS-META segment allows us to deduce the VARLEN1-OFFSET field from
    ALIGNED-ALLOC-META segment which finally gives us a pointer obtained by the
    underlying allocation Aligned_alloc_impl mechanism.
 */
struct Aligned_alloc_pfs : public allocator_traits<true> {
  using allocator_metadata = Aligned_alloc_metadata<uint32_t, uint32_t>;
  using pfs_metadata = PFS_metadata;

  /** Dynamically allocates storage of given size at the address aligned to the
      requested alignment.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @param[in] alignment Alignment requirement for storage to be allocated.
      @param[in] key PSI memory key to be used for PFS memory instrumentation.
      @return Pointer to the allocated storage. nullptr if dynamic storage
      allocation failed.
   */
  template <bool Zero_initialized>
  static inline void *alloc(std::size_t size, std::size_t alignment,
                            pfs_metadata::pfs_memory_key_t key) {
    // We must take special care to allocate enough extra space to hold the
    // PFS metadata (PFS-META + PFS-META-OFFSET) but we also need to take
    // special care that the pointer which will be returned to the callee by
    // this function will still be suitably over-aligned as requested. Both of
    // these requirements can be fulfilled by finding the smallest multiple of
    // requested alignment that is not smaller than actual PFS metadata size.
    const auto metadata_len = calc_align(pfs_metadata::size, alignment);
    const auto total_len = size + metadata_len;
    auto ret =
        Aligned_alloc_impl::alloc<Zero_initialized>(total_len, alignment);
    if (unlikely(!ret.first)) return nullptr;

    // Same as we do with non-PFS variant of Aligned_alloc::alloc(), here we
    // encode the offset so we can later on recover the original pointer, P' =
    // (P - O), from within Aligned_alloc_pfs::free(P) context. Similarly, we
    // encode the requested allocation datalen.
    allocator_metadata::meta_1(ret.first, ret.second);
    allocator_metadata::meta_2(ret.first, size);

#ifdef HAVE_PSI_MEMORY_INTERFACE
    // When computing total number of bytes allocated. we must not only account
    // for the size that we have requested (total_len) but we also need
    // to account for extra memory Aligned_alloc_impl may have allocated in
    // order to be able to accommodate the request. Amount of extra memory
    // allocated corresponds to the offset value returned by Aligned_alloc_impl.
    const auto datalen = total_len + ret.second;
    // The point of this allocator variant is to trace the memory allocations
    // through PFS (PSI) so do it.
    pfs_metadata::pfs_owning_thread_t owner;
    key = PSI_MEMORY_CALL(memory_alloc)(key, datalen, &owner);
    // To be able to do the opposite action of tracing when we are releasing the
    // memory, we need right about the same data we passed to the tracing
    // memory_alloc function. Let's encode this it into our allocator so we
    // don't have to carry and keep this data around.
    pfs_metadata::pfs_owning_thread(ret.first, owner);
    pfs_metadata::pfs_datalen(ret.first, datalen);
    pfs_metadata::pfs_key(ret.first, key);
    pfs_metadata::pfs_metaoffset(ret.first, metadata_len);
#endif

    return static_cast<uint8_t *>(ret.first) + metadata_len;
  }

  /** Releases storage dynamically allocated through
      Aligned_alloc_pfs::alloc().

      @param[in] data Pointer to storage allocated through
      Aligned_alloc_pfs::alloc()
   */
  static inline void free(PFS_metadata::data_segment_ptr data) noexcept {
    if (unlikely(!data)) return;

#ifdef HAVE_PSI_MEMORY_INTERFACE
    // Deduce the PFS data we encoded in Aligned_alloc_pfs::alloc()
    auto key = pfs_metadata::pfs_key(data);
    auto owner = pfs_metadata::pfs_owning_thread(data);
    auto datalen = pfs_metadata::pfs_datalen(data);
    // With the deduced PFS data, now trace the memory release action.
    PSI_MEMORY_CALL(memory_free)
    (key, datalen, owner);
#endif

    // Here we make use of the offset which has been encoded by
    // Aligned_alloc_pfs::alloc() to be able to deduce the original pointer and
    // simply forward it to std::free.
    Aligned_alloc_impl::free(deduce(data));
  }

  /** Returns the number of bytes requested to be allocated.

      @param[in] data Pointer to storage allocated through
      Aligned_alloc_pfs::alloc()
      @return Number of bytes.
   */
  static inline allocator_metadata::meta_2_t datalen(
      PFS_metadata::data_segment_ptr data) {
    // In order to be able to deduce the datalen field, we have to deduce the
    // beginning of PFS metadata segment first.
    return allocator_metadata::meta_2(pfs_metadata::deduce_pfs_meta(data));
  }

 private:
  /** Helper function which deduces the original pointer returned by
      Aligned_alloc_impl from a pointer which is passed to us by the call-site.
   */
  static inline void *deduce(PFS_metadata::data_segment_ptr data) noexcept {
    // To recover the original pointer we need to read the offset
    // we have serialized into the first (allocator) metadata field. But to read
    // that offset we have to jump over the PFS metadata first. We use PFS meta
    // offset for that.
    auto pfs_meta = pfs_metadata::deduce_pfs_meta(data);
    auto offset = allocator_metadata::meta_1(pfs_meta);
    return reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(pfs_meta) -
                                    offset);
  }
};

/** Simple utility metafunction which selects appropriate allocator variant
    (implementation) depending on the input parameter(s).
  */
template <bool Pfs_memory_instrumentation_on>
struct select_alloc_impl {
  using type =
      Aligned_alloc;  // When PFS is OFF, pick ordinary, non-PFS, variant
};

template <>
struct select_alloc_impl<true> {
  using type = Aligned_alloc_pfs;  // Otherwise, pick PFS variant
};

/** Just a small helper type which saves us some keystrokes. */
template <bool Pfs_memory_instrumentation_on>
using select_alloc_impl_t =
    typename select_alloc_impl<Pfs_memory_instrumentation_on>::type;

/** Small wrapper which utilizes SFINAE to dispatch the call to appropriate
    aligned allocator implementation.
  */
template <typename Impl>
struct Aligned_alloc_ {
  template <bool Zero_initialized, typename T = Impl>
  static inline typename std::enable_if<T::is_pfs_instrumented_v, void *>::type
  alloc(size_t size, size_t alignment, PSI_memory_key key) {
    return Impl::template alloc<Zero_initialized>(size, alignment, key);
  }
  template <bool Zero_initialized, typename T = Impl>
  static inline typename std::enable_if<!T::is_pfs_instrumented_v, void *>::type
  alloc(size_t size, size_t alignment, PSI_memory_key /*key*/) {
    return Impl::template alloc<Zero_initialized>(size, alignment);
  }
  static inline void free(void *ptr) { Impl::free(ptr); }
  static inline size_t datalen(void *ptr) { return Impl::datalen(ptr); }
};

}  // namespace detail
}  // namespace ut

#endif
