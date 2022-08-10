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

/** @file include/detail/ut/large_page_alloc-solaris.h
 Solaris-specific implementation bits and pieces for large (huge) page
 allocations. */

#ifndef detail_ut_large_page_alloc_solaris_h
#define detail_ut_large_page_alloc_solaris_h

#include <sys/mman.h>
#include <sys/types.h>

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
  // mmap on Solaris requires for n_bytes to be a multiple of large-page size
  size_t n_bytes_rounded = pow2_round(n_bytes + (large_page_default_size - 1),
                                      large_page_default_size);
  void *ptr = mmap(nullptr, n_bytes_rounded, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
  if (unlikely(ptr == (void *)-1)) {
    ib::log_warn(ER_IB_MSG_856)
        << "large_page_aligned_alloc mmap(" << n_bytes_rounded
        << " bytes) failed;"
           " errno "
        << errno;
  }
  // We also must do additional step to make it happen
  struct memcntl_mha m = {};
  m.mha_cmd = MHA_MAPSIZE_VA;
  m.mha_pagesize = large_page_default_size;
  int ret = memcntl(ptr, n_bytes_rounded, MC_HAT_ADVISE, (caddr_t)&m, 0, 0);
  if (unlikely(ret == -1)) {
    ib::log_warn(ER_IB_MSG_856)
        << "large_page_aligned_alloc memcntl(ptr, " << n_bytes_rounded
        << " bytes) failed;"
           " errno "
        << errno;
    return nullptr;
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

/** Queries all possible page-sizes. Solaris allows picking one at _runtime_
    which is contrary to how Linux, Windows and OSX does.

    @return std::vector of supported page sizes (in bytes).
*/
inline std::vector<size_t> large_page_all_supported_sizes() {
  int nr_of_pages = getpagesizes(NULL, 0);
  std::vector<size_t> supported_page_sizes(nr_of_pages);
  if (nr_of_pages > 0) {
    getpagesizes(supported_page_sizes.data(), nr_of_pages);
  }
  return supported_page_sizes;
}

/** Queries the page-size that is next to the minimum supported page-size
    Lowest supported page size is usually 4K on x86_64 whereas it's 8K on SPARC.

    @return Minimum supported large (huge) page size in bytes.
*/
inline size_t large_page_size() { return large_page_all_supported_sizes()[1]; }

}  // namespace detail
}  // namespace ut

#endif
