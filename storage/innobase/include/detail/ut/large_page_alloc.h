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

/** @file include/detail/ut/large_page_alloc.h
 Implementation bits and pieces for large (huge) page allocations. */

#ifndef detail_ut_large_page_alloc_h
#define detail_ut_large_page_alloc_h

#include <cstddef>
#include <fstream>

#include "my_compiler.h"
#ifdef _WIN32
#include "storage/innobase/include/detail/ut/large_page_alloc-win.h"
#elif defined(__APPLE__)
#include "storage/innobase/include/detail/ut/large_page_alloc-osx.h"
#elif defined(__sun)
#include "storage/innobase/include/detail/ut/large_page_alloc-solaris.h"
#else
#include "storage/innobase/include/detail/ut/large_page_alloc-linux.h"
#endif
#include "storage/innobase/include/detail/ut/allocator_traits.h"
#include "storage/innobase/include/detail/ut/helper.h"
#include "storage/innobase/include/detail/ut/page_metadata.h"
#include "storage/innobase/include/detail/ut/pfs.h"

extern const size_t large_page_default_size;

namespace ut {
namespace detail {

/** Allocation routines which are purposed for allocating memory through the
    means of what is known as large (huge) pages.

    large_page_aligned_alloc() and large_page_aligned_free() are taking care of
    OS specific details and Large_page_alloc is a convenience wrapper which only
    makes the use of large pages more ergonomic so that it serializes the actual
    size being allocated into the raw memory. This size is then automagically
    deduced when large page memory is being freed. Otherwise, client code would
    have been responsible to store and keep that value somewhere until it frees
    the large page memory segment. Additionally, information on type of page
    used to back up requested allocation is also serialized into the memory
    allowing to build higher-kinded abstractions more easily. See
    ut::malloc_large_page with option to fallback to regular pages through
    ut::malloc_page.

    Cost associated with this abstraction is the size of a single CPU page. In
    terms of virtual memory, especially in 64-bit address space, this cost is
    negligible. In practice this means that for each N huge-page sized
    allocation request, application code will get to use CPU_PAGE_SIZE bytes
    less. In other words, for a request that is backed up by three 2MiB
    huge-pages, application code will get to use 3 * 2MiB - CPU_PAGE_SIZE of
    total bytes. CPU_PAGE_SIZE is normally 4K but some architectures such as
    SPARC have it set to 8K. ARM64 can be set to 4K, 8K or 64K.

    Memory layout representation looks like the following:

     -------------------------------------------
     | PAGE-ALLOC-METADATA |    ... DATA ...   |
     -------------------------------------------
       ^                    ^
       |                    |
       |                    |
       |           ptr (large-page) to be returned to call-site
       |
      --------------------------------
      | DATALEN | PAGE-TYPE | VARLEN |
      --------------------------------
       \                              \
        0                              \
                               CPU_PAGE_SIZE - 1

    For details on DATALEN, PAGE-TYPE and VARLEN fields see Page_alloc_metadata.

    DATA is an actual page-aligned (!) segment backed by large (huge) page
    memory that will be returned to the call-site and which the client code will
    be able to use for the application data.
 */
struct Large_page_alloc : public allocator_traits<false> {
  using page_allocation_metadata = Page_alloc_metadata;

  /** Allocates memory through large-page support.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @return Pointer to the allocated storage. nullptr if allocation failed.
   */
  static inline void *alloc(std::size_t size) {
    auto total_len = round_to_next_multiple(
        size + page_allocation_metadata::len, large_page_default_size);
    auto mem = large_page_aligned_alloc(total_len);
    if (unlikely(!mem)) return nullptr;
    page_allocation_metadata::datalen(mem, total_len);
    page_allocation_metadata::page_type(mem, Page_type::large_page);
    return static_cast<uint8_t *>(mem) + page_allocation_metadata::len;
  }

  /** Releases storage allocated through
      Large_page_alloc::alloc().

      @param[in] data Pointer to storage allocated through
      Large_page_alloc::alloc()
      @return True if releasing the memory was successful.
   */
  static inline bool free(void *data) noexcept {
    if (unlikely(!data)) return false;
    ut_ad(page_type(data) == Page_type::large_page);
    return large_page_aligned_free(deduce(data),
                                   page_allocation_metadata::datalen(data));
  }

  /** Returns the number of bytes that have been allocated.

      @param[in] data Pointer to storage allocated through
      Large_page_alloc::alloc()
      @return Number of bytes.
   */
  static inline page_allocation_metadata::datalen_t datalen(void *data) {
    ut_ad(page_type(data) == Page_type::large_page);
    return page_allocation_metadata::datalen(data) -
           page_allocation_metadata::len;
  }

  /** Returns the type of the page.

      @param[in] data Pointer to storage allocated through
      Large_page_alloc::alloc()
      @return Page type.
   */
  static inline Page_type page_type(void *data) {
    return page_allocation_metadata::page_type(data);
  }

  /** Retrieves the pointer and size of the allocation provided by the OS. It is
      a low level information, and is needed only to call low level
      memory-related OS functions.

      @param[in] data Pointer to storage allocated through
      Large_page_alloc::alloc()
      @return Low level allocation info.
   */
  static inline allocation_low_level_info low_level_info(void *data) {
    ut_ad(page_type(data) == Page_type::large_page);
    return {deduce(data), page_allocation_metadata::datalen(data)};
  }

 private:
  /** Helper function which deduces the original pointer returned by
      Large_page_alloc from a pointer which is passed to us by the call-site.
   */
  static inline void *deduce(void *data) noexcept {
    ut_ad(page_type(data) == Page_type::large_page);
    const auto res = reinterpret_cast<void *>(static_cast<uint8_t *>(data) -
                                              page_allocation_metadata::len);
    ut_ad(reinterpret_cast<std::uintptr_t>(res) % large_page_size() == 0);
    return res;
  }
};

/** Allocation routines which are purposed for allocating memory through the
    means of what is known as large (huge) pages. This is a PFS
    (performance-schema) variant of Large_page_alloc. Implemented in terms
    of Page_alloc_metadata_pfs.

    large_page_aligned_alloc() and large_page_aligned_free() are taking care of
    OS specific details and Large_page_alloc is a convenience wrapper which only
    makes the use of large pages more ergonomic so that it serializes all the
    relevant PFS details into the raw memory. Otherwise, client code would have
    been responsible to store and keep those details somewhere until the memory
    segment is freed. Additionally, information on type of page used to back up
    requested allocation is also serialized into the memory allowing to build
    higher-kinded abstractions more easily. See ut::malloc_large_page with
    option to fallback to regular pages through ut::malloc_page.

    Cost associated with this abstraction is the size of a single CPU page. In
    terms of virtual memory, especially in 64-bit address space, this cost is
    negligible. In practice this means that for each N huge-page sized
    allocation request, application code will get to use CPU_PAGE_SIZE bytes
    less. In other words, for a request that is backed up by three 2MiB
    huge-pages, application code will get to use 3 * 2MiB - CPU_PAGE_SIZE of
    total bytes. CPU_PAGE_SIZE is normally 4K but some architectures such as
    SPARC have it set to 8K. ARM64 can be set to 4K, 8K or 64K.

    Memory layout representation looks like the following:

     ----------------------------------------------
     | PAGE-ALLOC-METADATA-PFS |   ... DATA ...   |
     ----------------------------------------------
      ^                         ^
      |                         |
      |                         |
      |               ptr (large-page) to be
      |                returned to call-site
      |
     ---------------------------------------------------
     | PFS-META | PAGE-TYPE | VARLEN | PFS-META-OFFSET |
     ---------------------------------------------------
      ^   ^
      |   |
      |  ---------------------------
      |  | OWNER |  DATALEN  | KEY |
      |  ---------------------------
      |
     ptr returned by
    large_page_aligned_alloc

    For details on PFS-META, PAGE-TYPE, VARLEN and PFS-META-OFFSET fields
    see Page_alloc_metadata_pfs.

    DATA is an actual page-aligned (!) segment backed by large (huge) page
    memory that will be returned to the call-site and which the client code will
    be able to use for the application data.
 */
struct Large_page_alloc_pfs : public allocator_traits<true> {
  using page_allocation_metadata = Page_alloc_metadata_pfs;

  /** Allocates memory through large-page support.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @param[in] key PSI memory key to be used for PFS memory instrumentation.
      @return Pointer to the allocated storage. nullptr if allocation failed.
    */
  static inline void *alloc(
      std::size_t size,
      page_allocation_metadata::pfs_metadata::pfs_memory_key_t key) {
    auto total_len = round_to_next_multiple(
        size + page_allocation_metadata::len, large_page_default_size);
    auto mem = large_page_aligned_alloc(total_len);
    if (unlikely(!mem)) return nullptr;

#ifdef HAVE_PSI_MEMORY_INTERFACE
    // The point of this allocator variant is to trace the memory allocations
    // through PFS (PSI) so do it.
    page_allocation_metadata::pfs_metadata::pfs_owning_thread_t owner;
    key = PSI_MEMORY_CALL(memory_alloc)(key, total_len, &owner);
    // To be able to do the opposite action of tracing when we are releasing the
    // memory, we need right about the same data we passed to the tracing
    // memory_alloc function. Let's encode this it into our allocator so we
    // don't have to carry and keep this data around.
    page_allocation_metadata::pfs_metadata::pfs_owning_thread(mem, owner);
    page_allocation_metadata::pfs_metadata::pfs_datalen(mem, total_len);
    page_allocation_metadata::pfs_metadata::pfs_key(mem, key);
    page_allocation_metadata::pfs_metadata::pfs_metaoffset(
        mem, page_allocation_metadata::len);
    page_allocation_metadata::page_type(mem, Page_type::large_page);
#endif

    return static_cast<uint8_t *>(mem) + page_allocation_metadata::len;
  }

  /** Releases storage allocated through Large_page_alloc_pfs::alloc().

      @param[in] data Pointer to storage allocated through
      Large_page_alloc_pfs::alloc()
      @return True if releasing the memory was successful.
   */
  static inline bool free(PFS_metadata::data_segment_ptr data) noexcept {
    if (unlikely(!data)) return false;
    ut_ad(page_type(data) == Page_type::large_page);

    PFS_metadata::pfs_datalen_t total_len = {};
#ifdef HAVE_PSI_MEMORY_INTERFACE
    // Deduce the PFS data we encoded in Large_page_alloc_pfs::alloc()
    auto key = page_allocation_metadata::pfs_metadata::pfs_key(data);
    auto owner =
        page_allocation_metadata::pfs_metadata::pfs_owning_thread(data);
    total_len = page_allocation_metadata::pfs_metadata::pfs_datalen(data);
    // With the deduced PFS data, now trace the memory release action.
    PSI_MEMORY_CALL(memory_free)
    (key, total_len, owner);
#endif

    return large_page_aligned_free(deduce(data), total_len);
  }

  /** Returns the number of bytes that have been allocated.

      @param[in] data Pointer to storage allocated through
      Large_page_alloc_pfs::alloc()
      @return Number of bytes.
   */
  static inline size_t datalen(PFS_metadata::data_segment_ptr data) {
    ut_ad(page_type(data) == Page_type::large_page);
    return page_allocation_metadata::pfs_metadata::pfs_datalen(data) -
           page_allocation_metadata::len;
  }

  /** Returns the Page_type.

      @param[in] data Pointer to storage allocated through
      Large_page_alloc_pfs::alloc()
      @return Page type.
   */
  static inline Page_type page_type(PFS_metadata::data_segment_ptr data) {
    return page_allocation_metadata::page_type(data);
  }

  /** Retrieves the pointer and size of the allocation provided by the OS. It is
      a low level information, and is needed only to call low level
      memory-related OS functions.

      @param[in] data Pointer to storage allocated through
      Large_page_alloc_pfs::alloc()
      @return Low level allocation info.
   */
  static inline allocation_low_level_info low_level_info(void *data) {
    ut_ad(page_type(data) == Page_type::large_page);
    return {deduce(data),
            page_allocation_metadata::pfs_metadata::pfs_datalen(data)};
  }

 private:
  /** Helper function which deduces the original pointer returned by
      Large_page_alloc_pfs from a pointer which is passed to us by the
      call-site.
   */
  static inline void *deduce(PFS_metadata::data_segment_ptr data) noexcept {
    ut_ad(page_type(data) == Page_type::large_page);
    const auto res =
        page_allocation_metadata::pfs_metadata::deduce_pfs_meta(data);
    ut_ad(reinterpret_cast<std::uintptr_t>(res) % large_page_size() == 0);
    return res;
  }
};

/** Simple utility metafunction which selects appropriate allocator variant
    (implementation) depending on the input parameter(s).
  */
template <bool Pfs_memory_instrumentation_on>
struct select_large_page_alloc_impl {
  using type =
      Large_page_alloc;  // When PFS is OFF, pick ordinary, non-PFS, variant
};

template <>
struct select_large_page_alloc_impl<true> {
  using type = Large_page_alloc_pfs;  // Otherwise, pick PFS variant
};

/** Just a small helper type which saves us some keystrokes. */
template <bool Pfs_memory_instrumentation_on>
using select_large_page_alloc_impl_t =
    typename select_large_page_alloc_impl<Pfs_memory_instrumentation_on>::type;

/** Small wrapper which utilizes SFINAE to dispatch the call to appropriate
    aligned allocator implementation.
  */
template <typename Impl>
struct Large_alloc_ {
  template <typename T = Impl>
  static inline typename std::enable_if<T::is_pfs_instrumented_v, void *>::type
  alloc(size_t size, PSI_memory_key key) {
    return Impl::alloc(size, key);
  }
  template <typename T = Impl>
  static inline typename std::enable_if<!T::is_pfs_instrumented_v, void *>::type
  alloc(size_t size, PSI_memory_key /*key*/) {
    return Impl::alloc(size);
  }
  static inline bool free(void *ptr) { return Impl::free(ptr); }
  static inline size_t datalen(void *ptr) { return Impl::datalen(ptr); }
  static inline Page_type page_type(void *ptr) { return Impl::page_type(ptr); }
  static inline allocation_low_level_info low_level_info(void *ptr) {
    return Impl::low_level_info(ptr);
  }
};

}  // namespace detail
}  // namespace ut

#endif
