/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file os/os0proc.cc
 The interface to the operating system
 process control primitives

 Created 9/30/1995 Heikki Tuuri
 *******************************************************/

#include "my_config.h"

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

#if defined HAVE_LINUX_LARGE_PAGES && defined HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include "ha_prototypes.h"
#include "os0proc.h"
#include "srv0srv.h"
#include "ut0byte.h"
#include "ut0mem.h"

/* FreeBSD for example has only MAP_ANON, Linux has MAP_ANONYMOUS and
MAP_ANON but MAP_ANON is marked as deprecated */
#if defined(MAP_ANONYMOUS)
#define OS_MAP_ANON MAP_ANONYMOUS
#elif defined(MAP_ANON)
#define OS_MAP_ANON MAP_ANON
#endif

/** The total amount of memory currently allocated from the operating
system with os_mem_alloc_large(). */
ulint os_total_large_mem_allocated = 0;

/** Whether to use large pages in the buffer pool */
bool os_use_large_pages;

/** Large page size. This may be a boot-time option on some platforms */
uint os_large_page_size;

/** Converts the current process id to a number.
@return process id as a number */
ulint os_proc_get_number(void) {
#ifdef _WIN32
  return (static_cast<ulint>(GetCurrentProcessId()));
#else
  return (static_cast<ulint>(getpid()));
#endif
}

/*
Returns the next large page size smaller or equal to the passed in size.

The search starts at srv_large_page_sizes[*start].

Assumes srv_get_large_page_sizes has been initialised

For first use, have *start=0. There is no need to increment *start.

@param   sz size to be searched for.
@param   start ptr to int representing offset in my_large_page_sizes to start from.
*start is updated during search and can be used to search again if 0 isn't returned.

@returns the next size found. *start will be incremented to the next potential size.
@retval  a large page size that is valid on this system or 0 if no large page size possible.
*/
static size_t os_next_large_page_size(size_t sz, int *start)
{
#if defined HAVE_LINUX_MULTIPLE_LARGE_PAGES
  size_t cur;

  while (*start < srv_large_page_sizes_length
         && srv_large_page_sizes[*start] > 0)
  {
    cur= *start;
    (*start)++;
    if (srv_large_page_sizes[cur] <= sz)
    {
      return srv_large_page_sizes[cur];
    }
  }
#endif
  return 0;
}

static inline uint os_bit_size_t_log2(size_t value)
{
  uint bit;
  for (bit=0 ; value > 1 ; value>>=1, bit++) ;
  return bit;
}

/** Allocates large pages memory.
@param[in,out]	n	Number of bytes to allocate
@return allocated memory */
void *os_mem_alloc_large(ulint *n) {
  void *ptr = NULL;
  ulint size;
#if defined HAVE_LINUX_LARGE_PAGES && defined UNIV_LINUX
  int mapflag, i= 0;
  size_t adjusted_size, large_page_size;

  if (!os_use_large_pages) {
    goto skip;
  }
#ifdef HAVE_LINUX_MULTIPLE_LARGE_PAGES
  if (!os_large_page_size) {
    /* advance i to be a smaller or equal to os_large_page_size */
    os_next_large_page_size(os_large_page_size, &i);
  }
  large_page_size = os_next_large_page_size(*n, &i);
#else
  large_page_size = os_large_page_size;
#endif
  if (!large_page_size)
    goto skip;

  ut_ad(ut_is_2pow(large_page_size));

#if defined HAVE_LINUX_MULTIPLE_LARGE_PAGES
  do
#endif
  {
    mapflag = MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB;
#if defined HAVE_LINUX_MULTIPLE_LARGE_PAGES
    /* MAP_HUGE_SHIFT added linux-3.8. Take largest HUGEPAGE size */
    mapflag |= os_bit_size_t_log2(large_page_size) << MAP_HUGE_SHIFT;
#endif
    /* Align block size to large_page_size */
    adjusted_size = ut_2pow_round(*n + (large_page_size - 1), large_page_size);
    ptr = mmap(NULL, adjusted_size, PROT_READ | PROT_WRITE, mapflag, -1, 0);
    if (ptr != (void*)-1) {
#if defined HAVE_LINUX_MULTIPLE_LARGE_PAGES
      break;
    } else {
      ptr = NULL;
      if (errno == ENOMEM) {
        /* no memory at this size, try next size */
        continue;
      }
#else
    } else {
#endif
      ptr = NULL;
      ib::warn(ER_IB_MSG_852)
          << "Failed to allocate " << adjusted_size << " bytes. pagesize " << large_page_size
          << " bytes. errno " << errno;
    }
  }
#if defined HAVE_LINUX_MULTIPLE_LARGE_PAGES
  while ((large_page_size = os_next_large_page_size(*n, &i)));
#endif

  if (ptr) {
    *n = adjusted_size;
    os_atomic_increment_ulint(&os_total_large_mem_allocated, adjusted_size);

    UNIV_MEM_ALLOC(ptr, adjusted_size);
    return (ptr);
  }

  ib::warn(ER_IB_MSG_854) << "Using conventional memory pool";
skip:
#endif /* HAVE_LINUX_LARGE_PAGES && UNIV_LINUX */

#ifdef _WIN32
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);

  /* Align block size to system page size */
  ut_ad(ut_is_2pow(system_info.dwPageSize));
  /* system_info.dwPageSize is only 32-bit. Casting to ulint is required
  on 64-bit Windows. */
  size = *n = ut_2pow_round(*n + (system_info.dwPageSize - 1),
                            (ulint)system_info.dwPageSize);
  ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!ptr) {
    ib::info(ER_IB_MSG_855) << "VirtualAlloc(" << size
                            << " bytes) failed;"
                               " Windows error "
                            << GetLastError();
  } else {
    os_atomic_increment_ulint(&os_total_large_mem_allocated, size);
    UNIV_MEM_ALLOC(ptr, size);
  }
#else
  size = getpagesize();
  /* Align block size to system page size */
  ut_ad(ut_is_2pow(size));
  size = *n = ut_2pow_round(*n + (size - 1), size);
  ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | OS_MAP_ANON, -1,
             0);
  if (UNIV_UNLIKELY(ptr == (void *)-1)) {
    ib::error(ER_IB_MSG_856) << "mmap(" << size
                             << " bytes) failed;"
                                " errno "
                             << errno;
    ptr = NULL;
  } else {
    os_atomic_increment_ulint(&os_total_large_mem_allocated, size);
    UNIV_MEM_ALLOC(ptr, size);
  }
#endif
  return (ptr);
}

/** Frees large pages memory.
@param[in]	ptr	pointer returned by os_mem_alloc_large()
@param[in]	size	size returned by os_mem_alloc_large() */
void os_mem_free_large(void *ptr, ulint size) {
  ut_a(os_total_large_mem_allocated >= size);

#ifdef _WIN32
  /* When RELEASE memory, the size parameter must be 0.
  Do not use MEM_RELEASE with MEM_DECOMMIT. */
  if (!VirtualFree(ptr, 0, MEM_RELEASE)) {
    ib::error(ER_IB_MSG_857) << "VirtualFree(" << ptr << ", " << size
                             << ") failed; Windows error " << GetLastError();
  } else {
    os_atomic_decrement_ulint(&os_total_large_mem_allocated, size);
    UNIV_MEM_FREE(ptr, size);
  }
#elif !defined OS_MAP_ANON
  ut_free(ptr);
#else
#if defined(UNIV_SOLARIS)
  if (munmap(static_cast<caddr_t>(ptr), size)) {
#else
  if (munmap(ptr, size)) {
#endif /* UNIV_SOLARIS */
    ib::error(ER_IB_MSG_858) << "munmap(" << ptr << ", " << size
                             << ") failed;"
                                " errno "
                             << errno;
  } else {
    os_atomic_decrement_ulint(&os_total_large_mem_allocated, size);
    UNIV_MEM_FREE(ptr, size);
  }
#endif
}
