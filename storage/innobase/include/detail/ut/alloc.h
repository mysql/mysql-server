/*****************************************************************************

Copyright (c) 2021, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/detail/ut/alloc.h
    Implementation bits and pieces for PFS and non-PFS variants for normal
    allocations and deallocations through new, delete, malloc, zalloc, free etc.
 */

#ifndef detail_ut_alloc_h
#define detail_ut_alloc_h

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>

#include "my_compiler.h"
#include "mysql/psi/mysql_memory.h"
#include "storage/innobase/include/detail/ut/allocator_traits.h"
#include "storage/innobase/include/detail/ut/helper.h"
#include "storage/innobase/include/detail/ut/pfs.h"

namespace ut {
namespace detail {

/** Allocation routines for non-extended alignment types, as opposed to
    Aligned_alloc for example.

    These are only a mere wrappers around standard allocation routines so
    memory layout representation doesn't look any other than the following:

     --------------------------------
     |         ... DATA ...         |
     --------------------------------
      ^
      |
      |
    ptr to be returned to call-site

    DATA segment is a segment that will be returned to the call-site.
 */
struct Alloc : public allocator_traits<false> {
  /** Dynamically allocates storage of given size.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @return Pointer to the allocated storage. nullptr if dynamic storage
      allocation failed.
   */
  template <bool Zero_initialized>
  static inline void *alloc(std::size_t size) noexcept {
    return Alloc_fn::alloc<Zero_initialized>(size);
  }

  /** Reallocates the given area of memory. Behaves as std::realloc()
      implementation on given platform.

      @param[in] ptr  Pointer to the memory to be reallocated.
      @param[in] size New size of storage (in bytes) requested to be allocated.
      @return Pointer to the reallocated storage. Or nullptr if realloc
      operation failed.
   */
  static inline void *realloc(void *ptr, std::size_t size) noexcept {
    return Alloc_fn::realloc(ptr, size);
  }

  /** Releases storage dynamically allocated through
      Alloc::alloc() or Alloc::realloc().

      @param[in] ptr Pointer to storage allocated through
      Alloc::alloc() or Alloc::realloc()
   */
  static inline void free(void *ptr) noexcept { Alloc_fn::free(ptr); }
};

/** Specialization of allocation routines for non-extended alignment types
    but which in comparison to Alloc are providing support for arrays.

    To provide support for arrays, these allocation routines will allocate extra
    (metadata) space so that they can serialize the requested size of an array
    (in bytes) into the memory. That will enable higher-kinded functions,
    implemented on top of Alloc, to take necessary actions such as cleaning up
    the resources by invoking appropriate number of destructors of
    non-trivially-destructible types. Otherwise, this would create a burden on
    end users by having to remember and carry the array size all around the
    code. This is equivalent to what we find in other standard implementations.
    For example, new int x[10] is always released without passing the array
    size: delete[] x; The same holds with this design.

    Memory layout representation looks like the following:

     ---------------------------------------
     | ALLOC-ARR-META |    ... DATA ...    |
     ---------------------------------------
       ^               ^
       |               |
       |               |
       |              ptr to be returned to call-site
       |
      -----------------
      |    DATALEN    |
      -----------------
       \               \
        0               \
                  alignof(max_align_t) - 1

    DATALEN segment encodes the total length of DATA segment, which is the
    actual allocation size that client code has requested.

    DATA segment is a segment that will be returned to the call-site.
 */
struct Alloc_arr : public allocator_traits<false> {
  /** This is how much the metadata (ALLOC-ARR-META) segment will be big. */
  static constexpr auto metadata_len = alignof(max_align_t);

  /** This is the type we will be using to store the size of an array. */
  using datalen_t = size_t;

  /** Sanity check so that we can be sure that our metadata segment can fit
      the datalen_t.
    */
  static_assert(sizeof(datalen_t) <= metadata_len, "Metadata does not fit!");

  /** Sanity check so that we can be sure that the size of our metadata segment
      is such so that the pointer to DATA segment is always suitably aligned
      (multiple of alignof(max_align_t).
    */
  static_assert(metadata_len % alignof(max_align_t) == 0,
                "metadata_len must be divisible by alignof(max_align_t)");

  /** Dynamically allocates storage of given size.

    @param[in] size Size of storage (in bytes) requested to be allocated.
    @return Pointer to the allocated storage. nullptr if dynamic storage
    allocation failed.
 */
  template <bool Zero_initialized>
  static inline void *alloc(std::size_t size) noexcept {
    const auto total_len = size + Alloc_arr::metadata_len;
    auto mem = Alloc_fn::alloc<Zero_initialized>(total_len);
    *(static_cast<datalen_t *>(mem)) = size;
    return static_cast<uint8_t *>(mem) + Alloc_arr::metadata_len;
  }

  /** Releases storage dynamically allocated through Alloc_arr::alloc().

      @param[in] ptr Pointer to storage allocated through Alloc_arr::alloc().
   */
  static inline void free(void *ptr) noexcept {
    if (unlikely(!ptr)) return;
    Alloc_fn::free(deduce(ptr));
  }

  /** Returns the size of an array in bytes.

      @param[in] ptr Pointer to storage allocated through
      Alloc_arr::alloc().
      @return Size of an array in bytes (or number of bytes allocated).
   */
  static inline datalen_t datalen(void *ptr) {
    return *reinterpret_cast<datalen_t *>(deduce(ptr));
  }

 private:
  /** Helper function which deduces the original pointer returned by
      Alloc_arr::alloc() from a pointer which is passed to us by the call-site.
   */
  static inline void *deduce(void *ptr) noexcept {
    return static_cast<uint8_t *>(ptr) - Alloc_arr::metadata_len;
  }
};

/** Allocation routines for non-extended alignment types, as opposed to
    Aligned_alloc_pfs for example, but which are instrumented through PFS
    (performance-schema).

    Implemented in terms of PFS_metadata.

    Memory layout representation looks like the following:

     --------------------------------------------------
     | PFS-META | VARLEN | PFS-META-OFFSET |   DATA   |
     --------------------------------------------------
      ^    ^                                ^
      |    |                                |
      |   ---------------------------       |
      |   | OWNER |  DATALEN  | KEY |       |
      |   ---------------------------       |
      |                                     |
   ptr returned by                          |
      Alloc_fn                              |
                                            |
                               ptr to be returned to call-site
                                   will be pointing here

    OWNER field encodes the owning thread.
    DATALEN field encodes total size of memory consumed and not only the size of
    the DATA segment.
    KEY field encodes the PFS/PSI key.

    VARLEN is the leftover variable-length segment that specialized
    implementations can further make use of by deducing its size from the
    following formulae: abs(alignof(max_align_t) - sizeof(PFS-META-OFFSET) -
    sizeof(PFS-META)). In code that would be std::abs(alignof(max_align_t) -
    PFS_metadata::size). Not used by this implementation.

    PFS-META-OFFSET, strictly speaking, isn't necessary in this case of
    non-extended alignments, where alignment is always known in compile-time and
    thus the offset we will be storing into the PFS-META-OFFSET field is always
    going to be the same for the given platform. So, rather than serializing
    this piece of information into the memory as we do right now, we could very
    well be storing it into the compile-time evaluated constexpr constant. The
    reason why we don't do it is that there is no advantage (*) of doing so
    while we would be introducing a disadvantage of having to maintain separate
    specialization of PFS_metadata and code would be somewhat more fragmented.

      (*) Extra space that we need to allocate in order to be able to fit the
          PFS_metadata is going to be the same regardless if there is
          PFS-META-OFFSET field or not. This is due to the fact that PFS-META
          segment alone is larger than alignof(max_align_t) so in order to
          keep the DATA segment suitably aligned (% alignof(max_align_t) == 0)
          we must choose the size for the whole PFS segment that is a multiple
          of alignof(max_align_t).

    PFS-META-OFFSET is a field which allows us to recover the pointer to
    PFS-META segment from a pointer to DATA segment.

    DATA is an actual segment which will keep the user data.
*/
struct Alloc_pfs : public allocator_traits<true> {
  using pfs_metadata = PFS_metadata;

  /** This is how much the metadata (PFS-META | VARLEN | PFS-META-OFFSET)
      segment will be big.
    */
  static constexpr auto metadata_len =
      calc_align(pfs_metadata::size, alignof(max_align_t));

  /** Dynamically allocates storage of given size at the address aligned to the
      requested alignment.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @param[in] key PSI memory key to be used for PFS memory instrumentation.
      @return Pointer to the allocated storage. nullptr if dynamic storage
      allocation failed.
   */
  template <bool Zero_initialized>
  static inline void *alloc(std::size_t size,
                            pfs_metadata::pfs_memory_key_t key) {
    const auto total_len = size + Alloc_pfs::metadata_len;
    auto mem = Alloc_fn::alloc<Zero_initialized>(total_len);
    if (unlikely(!mem)) return nullptr;

#ifdef HAVE_PSI_MEMORY_INTERFACE
    // The point of this allocator variant is to trace the memory allocations
    // through PFS (PSI) so do it.
    pfs_metadata::pfs_owning_thread_t owner;
    key = PSI_MEMORY_CALL(memory_alloc)(key, total_len, &owner);
    // To be able to do the opposite action of tracing when we are releasing the
    // memory, we need right about the same data we passed to the tracing
    // memory_alloc function. Let's encode this it into our allocator so we
    // don't have to carry and keep this data around.
    pfs_metadata::pfs_owning_thread(mem, owner);
    pfs_metadata::pfs_datalen(mem, total_len);
    pfs_metadata::pfs_key(mem, key);
    pfs_metadata::pfs_metaoffset(mem, Alloc_pfs::metadata_len);
#endif

    return static_cast<uint8_t *>(mem) + Alloc_pfs::metadata_len;
  }

  /** Reallocates the given area of memory, which if not nullptr, must be
      previously allocated by Alloc_pfs::alloc() or Alloc_pfs::realloc().

      Mimics unfortunate realloc() design so that:
        * If pointer passed is nullptr, then behavior is as if
          Alloc_pfs::alloc() had been called.
        * If new size of storage requested is 0, then behavior is as if
          Alloc_pfs::free() had been called.

      @param[in] data Pointer to the memory to be reallocated.
      @param[in] size New size of storage (in bytes) requested to be allocated.
      @param[in] key PSI memory key to be used for PFS memory instrumentation.
      @return Pointer to the reallocated storage. nullptr if dynamic storage
      allocation or reallocation failed or if new size requested was 0.
   */
  static inline void *realloc(PFS_metadata::data_segment_ptr data,
                              std::size_t size,
                              pfs_metadata::pfs_memory_key_t key) {
    // Allocate memory if pointer passed in is nullptr
    if (!data) {
      return Alloc_pfs::alloc<false>(size, key);
    }

    // Free the memory if passed in size is zero
    if (size == 0) {
      Alloc_pfs::free(data);
      return nullptr;
    }

#ifdef HAVE_PSI_MEMORY_INTERFACE
    // Deduce the PFS data we encoded in Alloc_pfs::alloc()
    auto key_curr = pfs_metadata::pfs_key(data);
    auto owner_curr = pfs_metadata::pfs_owning_thread(data);
    auto datalen_curr = pfs_metadata::pfs_datalen(data);
    // With the deduced PFS data, now trace the memory release action.
    PSI_MEMORY_CALL(memory_free)
    (key_curr, datalen_curr, owner_curr);
#endif

    // Otherwise, continue with the plain realloc
    const auto total_len = size + Alloc_pfs::metadata_len;
    auto mem = Alloc_fn::realloc(deduce(data), total_len);
    if (unlikely(!mem)) return nullptr;

#ifdef HAVE_PSI_MEMORY_INTERFACE
    // The point of this allocator variant is to trace the memory allocations
    // through PFS (PSI) so do it.
    pfs_metadata::pfs_owning_thread_t owner;
    key = PSI_MEMORY_CALL(memory_alloc)(key, total_len, &owner);
    // To be able to do the opposite action of tracing when we are releasing the
    // memory, we need right about the same data we passed to the tracing
    // memory_alloc function. Let's encode this it into our allocator so we
    // don't have to carry and keep this data around.
    pfs_metadata::pfs_owning_thread(mem, owner);
    pfs_metadata::pfs_datalen(mem, total_len);
    pfs_metadata::pfs_key(mem, key);
    pfs_metadata::pfs_metaoffset(mem, Alloc_pfs::metadata_len);
#endif

    return static_cast<uint8_t *>(mem) + Alloc_pfs::metadata_len;
  }

  /** Releases storage dynamically allocated through
      Alloc_pfs::alloc().

      @param[in] data Pointer to storage allocated through
      Alloc_pfs::alloc()
   */
  static inline void free(PFS_metadata::data_segment_ptr data) noexcept {
    if (unlikely(!data)) return;

#ifdef HAVE_PSI_MEMORY_INTERFACE
    // Deduce the PFS data we encoded in Alloc_pfs::alloc()
    auto key = pfs_metadata::pfs_key(data);
    auto owner = pfs_metadata::pfs_owning_thread(data);
    auto datalen = pfs_metadata::pfs_datalen(data);
    // With the deduced PFS data, now trace the memory release action.
    PSI_MEMORY_CALL(memory_free)
    (key, datalen, owner);
#endif

    // Here we make use of the offset which has been encoded by
    // Alloc_pfs::alloc() to be able to deduce the original pointer and
    // simply forward it to std::free.
    Alloc_fn::free(deduce(data));
  }

  /** Returns the number of bytes requested to be allocated.

    @param[in] data Pointer to storage allocated through
    Alloc_pfs::alloc()
    @return Number of bytes.
   */
  static inline size_t datalen(PFS_metadata::data_segment_ptr data) {
    return pfs_metadata::pfs_datalen(data) - Alloc_pfs::metadata_len;
  }

 private:
  /** Helper function which deduces the original pointer returned by
      Alloc_pfs::alloc() from a pointer which is passed to us by the call-site.
   */
  static inline void *deduce(PFS_metadata::data_segment_ptr data) noexcept {
    return pfs_metadata::deduce_pfs_meta(data);
  }
};

/** Simple utility metafunction which selects appropriate allocator variant
    (implementation) depending on the input parameter(s).
  */
template <bool Pfs_memory_instrumentation_on, bool Array_specialization>
struct select_malloc_impl {};

template <>
struct select_malloc_impl<false, false> {
  using type = Alloc;  // When PFS is OFF, pick ordinary, non-PFS, variant
};

template <>
struct select_malloc_impl<false, true> {
  using type = Alloc_arr;  // When needed, take special care to pick the variant
                           // which specializes for arrays
};

template <bool Array_specialization>
struct select_malloc_impl<true, Array_specialization> {
  using type = Alloc_pfs;  // Otherwise, pick PFS variant
};

/** Just a small helper type which saves us some keystrokes. */
template <bool Pfs_memory_instrumentation_on, bool Array_specialization>
using select_malloc_impl_t =
    typename select_malloc_impl<Pfs_memory_instrumentation_on,
                                Array_specialization>::type;

/** Small wrapper which utilizes SFINAE to dispatch the call to appropriate
    allocator implementation.
  */
template <typename Impl>
struct Alloc_ {
  template <bool Zero_initialized, typename T = Impl>
  static inline typename std::enable_if<T::is_pfs_instrumented_v, void *>::type
  alloc(size_t size, PSI_memory_key key) {
    return Impl::template alloc<Zero_initialized>(size, key);
  }
  template <bool Zero_initialized, typename T = Impl>
  static inline typename std::enable_if<!T::is_pfs_instrumented_v, void *>::type
  alloc(size_t size, PSI_memory_key /*key*/) {
    return Impl::template alloc<Zero_initialized>(size);
  }
  template <typename T = Impl>
  static inline typename std::enable_if<T::is_pfs_instrumented_v, void *>::type
  realloc(void *ptr, size_t size, PSI_memory_key key) {
    return Impl::realloc(ptr, size, key);
  }
  template <typename T = Impl>
  static inline typename std::enable_if<!T::is_pfs_instrumented_v, void *>::type
  realloc(void *ptr, size_t size, PSI_memory_key /*key*/) {
    return Impl::realloc(ptr, size);
  }
  static inline void free(void *ptr) { Impl::free(ptr); }
  static inline size_t datalen(void *ptr) { return Impl::datalen(ptr); }
  template <typename T = Impl>
  static inline typename std::enable_if<T::is_pfs_instrumented_v, size_t>::type
  pfs_overhead() {
    return Alloc_pfs::metadata_len;
  }
  template <typename T = Impl>
  static inline typename std::enable_if<!T::is_pfs_instrumented_v, size_t>::type
  pfs_overhead() {
    return 0;
  }
};

}  // namespace detail
}  // namespace ut

#endif
