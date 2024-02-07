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

/** @file include/detail/ut/large_page_alloc-osx.h
 OSX-specific implementation bits and pieces for large (huge) page
 allocations. */

#ifndef detail_ut_large_page_alloc_osx_h
#define detail_ut_large_page_alloc_osx_h

#include <mach/vm_statistics.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "mysqld_error.h"
#include "storage/innobase/include/detail/ut/helper.h"
#include "storage/innobase/include/ut0log.h"

extern const size_t large_page_default_size;

namespace ut {
namespace detail {

/** Superpage size to be used (2MB). */
static constexpr auto SUPER_PAGE_SIZE = VM_FLAGS_SUPERPAGE_SIZE_2MB;

/** Allocates memory backed by large (huge) pages.

    @param[in] n_bytes Size of storage (in bytes) requested to be allocated.
    @return Pointer to the allocated storage. nullptr if allocation failed.
*/
inline void *large_page_aligned_alloc(size_t n_bytes) {
  // mmap on OSX requires for n_bytes to be a multiple of large-page size
  size_t n_bytes_rounded = pow2_round(n_bytes + (large_page_default_size - 1),
                                      large_page_default_size);
  void *ptr = mmap(nullptr, n_bytes_rounded, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, SUPER_PAGE_SIZE, 0);
  if (unlikely(ptr == (void *)-1)) {
    ib::log_warn(ER_IB_MSG_856)
        << "large_page_aligned_alloc mmap(" << n_bytes_rounded
        << " bytes) failed;"
           " errno "
        << errno;
  }
  return (ptr != (void *)-1) ? ptr : nullptr;
}

/** Releases memory backed by large (huge) pages.

    @param[in] ptr Pointer to large (huge) page aligned storage.
    @param[in] n_bytes Size of the storage.
    @return Returns true if releasing the large (huge) page succeeded.
 */
inline bool large_page_aligned_free(void *ptr, size_t n_bytes) {
  if (unlikely(!ptr)) return false;
  // Freeing huge-pages require size to be the multiple of huge-page size
  size_t n_bytes_rounded = pow2_round(n_bytes + (large_page_default_size - 1),
                                      large_page_default_size);
  auto ret = munmap(ptr, n_bytes_rounded);
  if (unlikely(ret != 0)) {
    ib::log_error(ER_IB_MSG_858)
        << "large_page_aligned_free munmap(" << ptr << ", " << n_bytes_rounded
        << ") failed;"
           " errno "
        << errno;
  }
  return ret == 0;
}

/** Queries the current size of large (huge) pages on running system.

    @return Large (huge) page size in bytes.
*/
inline size_t large_page_size() {
  // Return value of this function is hard-coded because of the way how
  // large_page_aligned_alloc is allocating superpages
  // (VM_FLAGS_SUPERPAGE_SIZE_2MB).
  //
  // Nonetheless we can make a compile-time check to detect situations when and
  // if that value may change, in which case we can at least provide a
  // guideline through static_assert message.
  static_assert(
      SUPER_PAGE_SIZE == VM_FLAGS_SUPERPAGE_SIZE_2MB,
      "superpage size is not the one that has been expected (2MB). In case this \
      is a wanted change, please tweak this static_assert _and_ modify this function \
      to return appropriate new superpage size value.");
  return 2 * 1024 * 1024;
}

}  // namespace detail
}  // namespace ut

#endif
