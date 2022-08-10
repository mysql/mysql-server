/*****************************************************************************

Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

/** @file include/detail/ut/page_alloc.h
 Implementation bits and pieces for page-aligned allocations. */

#ifndef detail_ut_page_alloc_h
#define detail_ut_page_alloc_h

#ifdef _WIN32
#include <windows.h>
// _must_ go after windows.h, this comment makes clang-format to preserve
// the include order
#include <memoryapi.h>
#else
#include <sys/mman.h>
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>

#include "my_compiler.h"
#include "my_config.h"
#include "mysqld_error.h"
#include "storage/innobase/include/detail/ut/allocator_traits.h"
#include "storage/innobase/include/detail/ut/helper.h"
#include "storage/innobase/include/detail/ut/page_metadata.h"
#include "storage/innobase/include/detail/ut/pfs.h"
#include "storage/innobase/include/ut0log.h"

namespace ut {
namespace detail {

/** Allocates system page-aligned memory.

    @param[in] n_bytes Size of storage (in bytes) requested to be allocated.
    @return Pointer to the allocated storage. nullptr if allocation failed.
*/
inline void *page_aligned_alloc(size_t n_bytes) {
#ifdef _WIN32
  // With lpAddress set to nullptr, VirtualAlloc will internally round n_bytes
  // to the multiple of system page size if it is not already
  void *ptr =
      VirtualAlloc(nullptr, n_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (unlikely(!ptr)) {
    ib::log_warn(ER_IB_MSG_856) << "page_aligned_alloc VirtualAlloc(" << n_bytes
                                << " bytes) failed;"
                                   " Windows error "
                                << GetLastError();
  }
  return ptr;
#else
  // With addr set to nullptr, mmap will internally round n_bytes to the
  // multiple of system page size if it is not already
  void *ptr = mmap(nullptr, n_bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
  if (unlikely(ptr == (void *)-1)) {
    ib::log_warn(ER_IB_MSG_856) << "page_aligned_alloc mmap(" << n_bytes
                                << " bytes) failed;"
                                   " errno "
                                << errno;
  }
  return (ptr != (void *)-1) ? ptr : nullptr;
#endif
}

/** Releases system page-aligned storage.

    @param[in] ptr Pointer to system page-aligned storage.
    @param[in] n_bytes Size of the storage.
    @return True if releasing the page-aligned memory was successful.
 */
inline bool page_aligned_free(void *ptr, size_t n_bytes [[maybe_unused]]) {
  if (unlikely(!ptr)) return false;
#ifdef _WIN32
  auto ret = VirtualFree(ptr, 0, MEM_RELEASE);
  if (unlikely(ret == 0)) {
    ib::log_error(ER_IB_MSG_858)
        << "large_page_aligned_free VirtualFree(" << ptr
        << ")  failed;"
           " Windows error "
        << GetLastError();
  }
  return ret != 0;
#else
  // length aka n_bytes does not need to be aligned to page-size
  auto ret = munmap(ptr, n_bytes);
  if (unlikely(ret != 0)) {
    ib::log_error(ER_IB_MSG_858)
        << "page_aligned_free munmap(" << ptr << ", " << n_bytes
        << ") failed;"
           " errno "
        << errno;
  }
  return ret == 0;
#endif
}

/** Allocation routines which are purposed for allocating system page-aligned
    memory.

    page_aligned_alloc() and page_aligned_free() are taking care of
    OS specific details and Page_alloc is a convenience wrapper which only
    makes the use of system page-aligned memory more ergonomic so that it
    serializes the actual size being allocated into the raw memory. This size
    is then automagically deduced when system page-aligned memory is being
    freed. Otherwise, client code would have been responsible to store and keep
    that value somewhere until the memory segment is freed. Additionally,
    information on type of page used to back up requested allocation is also
    serialized into the memory allowing to build higher-kinded abstractions more
    easily. See ut::malloc_large_page with option to fallback to regular pages
    through ut::malloc_page.

    Cost associated with this abstraction is the size of a single CPU page. In
    terms of virtual memory, especially in 64-bit address space, this cost is
    negligible. In practice this means that for each N pages allocation
    request there will be N+1 pages allocated beneath.

    Memory layout representation looks like the following:

     ------------------------------------------
     | PAGE-ALLOC-METADATA |   ... DATA ...   |
     ------------------------------------------
      ^                     ^
      |                     |
      |                     |
      |          ptr (system-page) to be
      |           returned to call-site
      |
     --------------------------------
     | DATALEN | PAGE-TYPE | VARLEN |
     --------------------------------
      ^
      |
      |
     ptr returned by
    page_aligned_alloc


    For details on DATALEN, PAGE-TYPE and VARLEN fields see Page_alloc_metadata.

    DATA is an actual page-aligned segment that will be returned to the
    call-site and which the client code will be able to use for the application
    data.
 */
struct Page_alloc : public allocator_traits<false> {
  using page_allocation_metadata = Page_alloc_metadata;

  /** Allocates memory through large-page support.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @return Pointer to the allocated storage. nullptr if allocation failed.
   */
  static inline void *alloc(std::size_t size) {
    auto total_len = round_to_next_multiple(
        size + page_allocation_metadata::len, CPU_PAGE_SIZE);
    auto mem = page_aligned_alloc(total_len);
    if (unlikely(!mem)) return nullptr;
    page_allocation_metadata::datalen(mem, total_len);
    page_allocation_metadata::page_type(mem, Page_type::system_page);
    return static_cast<uint8_t *>(mem) + page_allocation_metadata::len;
  }

  /** Releases storage allocated through Page_alloc::alloc().

      @param[in] data Pointer to storage allocated through Page_alloc::alloc()
      @return True if releasing the page-aligned memory was successful.
   */
  static inline bool free(void *data) noexcept {
    if (unlikely(!data)) return false;
    ut_ad(page_type(data) == Page_type::system_page);
    return page_aligned_free(deduce(data),
                             page_allocation_metadata::datalen(data));
  }

  /** Returns the number of bytes that have been allocated.

      @param[in] data Pointer to storage allocated through Page_alloc::alloc()
      @return Number of bytes.
   */
  static inline page_allocation_metadata::datalen_t datalen(void *data) {
    ut_ad(page_type(data) == Page_type::system_page);
    return page_allocation_metadata::datalen(data) -
           page_allocation_metadata::len;
  }

  /** Returns the the type of the page.

      @param[in] data Pointer to storage allocated through Page_alloc::alloc()
      @return Page type.
   */
  static inline Page_type page_type(void *data) {
    ut_ad(page_allocation_metadata::page_type(data) == Page_type::system_page);
    return page_allocation_metadata::page_type(data);
  }

  /** Retrieves the pointer and size of the allocation provided by the OS. It is
      a low level information, and is needed only to call low level
      memory-related OS functions.

      @param[in] data Pointer to storage allocated through Page_alloc::alloc()
      @return Low level allocation info.
   */
  static inline allocation_low_level_info low_level_info(void *data) {
    ut_ad(page_type(data) == Page_type::system_page);
    return {deduce(data), page_allocation_metadata::datalen(data)};
  }

 private:
  /** Helper function which deduces the original pointer returned by
      Page_alloc from a pointer which is passed to us by the call-site.
   */
  static inline void *deduce(void *data) noexcept {
    ut_ad(page_type(data) == Page_type::system_page);
    const auto res = reinterpret_cast<void *>(static_cast<uint8_t *>(data) -
                                              page_allocation_metadata::len);
    ut_ad(reinterpret_cast<std::uintptr_t>(res) % CPU_PAGE_SIZE == 0);
    return res;
  }
};

/** Allocation routines which are purposed for allocating system page-aligned
    memory. This is a PFS (performance-schema) variant of Page_alloc.
    Implemented in terms of Page_alloc_metadata_pfs.

    page_aligned_alloc() and page_aligned_free() are taking care of
    OS specific details and Page_alloc_pfs is a convenience wrapper which
    only makes the use of system page-aligned memory more ergonomic so that
    it serializes all the relevant PFS details into the raw memory. Otherwise,
    client code would have been responsible to store and keep those details
    somewhere until the memory segment is freed. Additionally, information on
    type of page used to back up requested allocation is also serialized into
    the memory allowing to build higher-kinded abstractions more easily. See
    ut::malloc_large_page with option to fallback to regular pages through
    ut::malloc_page.

    Cost associated with this abstraction is the size of a single CPU page. In
    terms of virtual memory, especially in 64-bit address space, this cost is
    negligible. In practice this means that for each N pages allocation
    request there will be N+1 pages allocated beneath.

    Memory layout representation looks like the following:

     ----------------------------------------------
     | PAGE-ALLOC-METADATA-PFS |   ... DATA ...   |
     ----------------------------------------------
      ^                         ^
      |                         |
      |                         |
      |               ptr (system-page) to be
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
    page_aligned_alloc

    For details on PFS-META, PAGE-TYPE, VARLEN and PFS-META-OFFSET fields
    see Page_alloc_metadata_pfs.

    DATA is an actual page-aligned segment that will be returned to the
    call-site and which the client code will be able to use for the application
    data.
 */
struct Page_alloc_pfs : public allocator_traits<true> {
  using page_allocation_metadata = Page_alloc_metadata_pfs;

  /** Allocates system page-aligned memory.

      @param[in] size Size of storage (in bytes) requested to be allocated.
      @param[in] key PSI memory key to be used for PFS memory instrumentation.
      @return Pointer to the allocated storage. nullptr if allocation failed.
    */
  static inline void *alloc(
      std::size_t size,
      page_allocation_metadata::pfs_metadata::pfs_memory_key_t key) {
    auto total_len = round_to_next_multiple(
        size + page_allocation_metadata::len, CPU_PAGE_SIZE);
    auto mem = page_aligned_alloc(total_len);
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
    page_allocation_metadata::page_type(mem, Page_type::system_page);
#endif

    return static_cast<uint8_t *>(mem) + page_allocation_metadata::len;
  }

  /** Releases storage allocated through Page_alloc_pfs::alloc().

      @param[in] data Pointer to storage allocated through
      Page_alloc_pfs::alloc()
      @return True if releasing the page-aligned memory was successful.
   */
  static inline bool free(PFS_metadata::data_segment_ptr data) noexcept {
    if (unlikely(!data)) return false;
    ut_ad(page_type(data) == Page_type::system_page);

    PFS_metadata::pfs_datalen_t total_len = {};
#ifdef HAVE_PSI_MEMORY_INTERFACE
    // Deduce the PFS data we encoded in Page_alloc_pfs::alloc()
    auto key = page_allocation_metadata::pfs_metadata::pfs_key(data);
    auto owner =
        page_allocation_metadata::pfs_metadata::pfs_owning_thread(data);
    total_len = page_allocation_metadata::pfs_metadata::pfs_datalen(data);
    // With the deduced PFS data, now trace the memory release action.
    PSI_MEMORY_CALL(memory_free)
    (key, total_len, owner);
#endif

    return page_aligned_free(deduce(data), total_len);
  }

  /** Returns the number of bytes that have been allocated.

      @param[in] data Pointer to storage allocated through
      Page_alloc_pfs::alloc()
      @return Number of bytes.
   */
  static inline size_t datalen(PFS_metadata::data_segment_ptr data) {
    ut_ad(page_type(data) == Page_type::system_page);
    return page_allocation_metadata::pfs_metadata::pfs_datalen(data) -
           page_allocation_metadata::len;
  }

  /** Returns the Page_type.

      @param[in] data Pointer to storage allocated through
      Page_alloc_pfs::alloc()
      @return Page type.
   */
  static inline Page_type page_type(PFS_metadata::data_segment_ptr data) {
    ut_ad(page_allocation_metadata::page_type(data) == Page_type::system_page);
    return page_allocation_metadata::page_type(data);
  }

  /** Retrieves the pointer and size of the allocation provided by the OS. It is
      a low level information, and is needed only to call low level
      memory-related OS functions.

      @param[in] data Pointer to storage allocated through
      Page_alloc_pfs::alloc()
      @return Low level allocation info.
   */
  static inline allocation_low_level_info low_level_info(void *data) {
    ut_ad(page_type(data) == Page_type::system_page);
    return {deduce(data),
            page_allocation_metadata::pfs_metadata::pfs_datalen(data)};
  }

 private:
  /** Helper function which deduces the original pointer returned by
      Page_alloc_pfs from a pointer which is passed to us by the call-site.
   */
  static inline void *deduce(PFS_metadata::data_segment_ptr data) noexcept {
    ut_ad(page_type(data) == Page_type::system_page);
    const auto res =
        page_allocation_metadata::pfs_metadata::deduce_pfs_meta(data);
    ut_ad(reinterpret_cast<std::uintptr_t>(res) % CPU_PAGE_SIZE == 0);
    return res;
  }
};

/** Simple utility meta-function which selects appropriate allocator variant
    (implementation) depending on the input parameter(s).
  */
template <bool Pfs_memory_instrumentation_on>
struct select_page_alloc_impl {
  using type = Page_alloc;  // When PFS is OFF, pick ordinary, non-PFS, variant
};

template <>
struct select_page_alloc_impl<true> {
  using type = Page_alloc_pfs;  // Otherwise, pick PFS variant
};

/** Just a small helper type which saves us some keystrokes. */
template <bool Pfs_memory_instrumentation_on>
using select_page_alloc_impl_t =
    typename select_page_alloc_impl<Pfs_memory_instrumentation_on>::type;

/** Small wrapper which utilizes SFINAE to dispatch the call to appropriate
    aligned allocator implementation.
  */
template <typename Impl>
struct Page_alloc_ {
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
