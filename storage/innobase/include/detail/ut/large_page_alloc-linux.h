/*****************************************************************************

Copyright (c) 2021, Oracle and/or its affiliates.

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

/** @file include/detail/ut/large_page_alloc-linux.h
 Linux-specific implementation bits and pieces for large (huge) page
 allocations. */

#ifndef detail_ut_large_page_alloc_linux_h
#define detail_ut_large_page_alloc_linux_h

#include <sys/mman.h>
#include <sys/types.h>

#include "storage/innobase/include/detail/ut/helper.h"

extern const size_t large_page_default_size;

namespace ut {
namespace detail {

/** Allocates memory backed by large (huge) pages.

    @param[in] n_bytes Size of storage (in bytes) requested to be allocated.
    @return Pointer to the allocated storage. nullptr if allocation failed.
*/
inline void *large_page_aligned_alloc(size_t n_bytes) {
  // mmap will internally round n_bytes to the multiple of huge-page size if it
  // is not already
  void *ptr = mmap(nullptr, n_bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON | MAP_HUGETLB, -1, 0);
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
  auto ret = munmap(ptr, pow2_round(n_bytes + (large_page_default_size - 1),
                                    large_page_default_size));
  return ret == 0;
}

/** Queries the current size of large (huge) pages on running system.

    @return Large (huge) page size in bytes.
*/
inline size_t large_page_size() {
  std::string key;
  std::ifstream meminfo("/proc/meminfo");
  if (meminfo) {
    while (meminfo >> key) {
      if (key == "Hugepagesize:") {
        unsigned long long value;
        meminfo >> value;
        if (value == 0 && meminfo.fail()) return 0;
        return value * 1024;
      }
      meminfo.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
  }
  return 0;
}

}  // namespace detail
}  // namespace ut

#endif
