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

/** @file include/detail/ut/large_page_alloc-win.h
 Windows-specific implementation bits and pieces for large (huge) page
 allocations. */

#ifndef detail_ut_large_page_alloc_win_h
#define detail_ut_large_page_alloc_win_h

#include <windows.h>
// _must_ go after windows.h, this comment makes clang-format to preserve
// the include order
#include <memoryapi.h>

#include "mysqld_error.h"
#include "storage/innobase/include/detail/ut/helper.h"
#include "storage/innobase/include/ut0log.h"

extern const size_t large_page_default_size;

namespace ut {
namespace detail {

/** Allocates memory backed by large (huge) pages.

    @param[in] n_bytes Size of storage (in bytes) requested to be allocated.
    @return Pointer to the allocated storage. nullptr if allocation failed.
*/
inline void *large_page_aligned_alloc(size_t n_bytes) {
  // VirtualAlloc requires for n_bytes to be a multiple of large-page size
  size_t n_bytes_rounded = pow2_round(n_bytes + (large_page_default_size - 1),
                                      large_page_default_size);
  void *ptr =
      VirtualAlloc(nullptr, n_bytes_rounded,
                   MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
  if (unlikely(!ptr)) {
    ib::log_warn(ER_IB_MSG_856)
        << "large_page_aligned_alloc VirtualAlloc(" << n_bytes_rounded
        << " bytes) failed; Windows error " << GetLastError();
  }
  return ptr;
}

/** Releases memory backed by large (huge) pages.

    @param[in] ptr Pointer to large (huge) page aligned storage.
    @param[in] n_bytes Size of the storage.
    @return Returns true if releasing the large (huge) page succeeded.
 */
inline bool large_page_aligned_free(void *ptr, size_t n_bytes) {
  if (unlikely(!ptr)) return false;
  auto ret = VirtualFree(ptr, 0, MEM_RELEASE);
  (void)n_bytes;
  if (unlikely(ret == 0)) {
    ib::log_error(ER_IB_MSG_858)
        << "large_page_aligned_free VirtualFree(" << ptr
        << ")  failed;"
           " Windows error "
        << GetLastError();
  }
  return ret != 0;
}

/** Queries the current size of large (huge) pages on running system.

    @return Large (huge) page size in bytes.
*/
inline size_t large_page_size() { return GetLargePageMinimum(); }

}  // namespace detail
}  // namespace ut

#endif
