/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/pfs_global.cc
  Miscellaneous global dependencies (implementation).
*/

#include "storage/perfschema/pfs_global.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my_sys.h"
#include "sql/log.h"
#include "storage/perfschema/pfs_builtin_memory.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

bool pfs_initialized = false;

/**
  Memory allocation for the performance schema.
  The memory used internally in the performance schema implementation.
  It is allocated at startup, or during runtime with scalable buffers.
*/
void *pfs_malloc(PFS_builtin_memory_class *klass, size_t size, myf flags) {
  assert(klass != nullptr);
  assert(size > 0);

  void *ptr = nullptr;

#ifdef PFS_ALIGNEMENT
#ifdef HAVE_POSIX_MEMALIGN
  /* Linux */
  if (unlikely(posix_memalign(&ptr, PFS_ALIGNEMENT, size))) {
    return nullptr;
  }
#else
#ifdef HAVE_MEMALIGN
  /* Solaris */
  ptr = memalign(PFS_ALIGNEMENT, size);
  if (unlikely(ptr == NULL)) {
    return NULL;
  }
#else
#ifdef HAVE_ALIGNED_MALLOC
  /* Windows */
  ptr = _aligned_malloc(size, PFS_ALIGNEMENT);
  if (unlikely(ptr == NULL)) {
    return NULL;
  }
#else
#error "Missing implementation for PFS_ALIGNENT"
#endif /* HAVE_ALIGNED_MALLOC */
#endif /* HAVE_MEMALIGN */
#endif /* HAVE_POSIX_MEMALIGN */
#else  /* PFS_ALIGNMENT */
  /* Everything else */
  ptr = malloc(size);
  if (unlikely(ptr == NULL)) {
    return NULL;
  }
#endif

  klass->count_alloc(size);

  if (flags & MY_ZEROFILL) {
    memset(ptr, 0, size);
  }
  return ptr;
}

void pfs_free(PFS_builtin_memory_class *klass, size_t size, void *ptr) {
  if (ptr == nullptr) {
    return;
  }

#ifdef HAVE_POSIX_MEMALIGN
  /* Allocated with posix_memalign() */
  free(ptr);
#else
#ifdef HAVE_MEMALIGN
  /* Allocated with memalign() */
  free(ptr);
#else
#ifdef HAVE_ALIGNED_MALLOC
  /* Allocated with _aligned_malloc() */
  _aligned_free(ptr);
#else
  /* Allocated with malloc() */
  free(ptr);
#endif /* HAVE_ALIGNED_MALLOC */
#endif /* HAVE_MEMALIGN */
#endif /* HAVE_POSIX_MEMALIGN */

  klass->count_free(size);
}

/**
  Array allocation for the performance schema.
  Checks for overflow of n * size before allocating.
  @param klass performance schema memory class
  @param n     number of array elements
  @param size  element size
  @param flags malloc flags
  @return pointer to memory on success, else NULL
*/
void *pfs_malloc_array(PFS_builtin_memory_class *klass, size_t n, size_t size,
                       myf flags) {
  assert(klass != nullptr);
  assert(n > 0);
  assert(size > 0);
  void *ptr = nullptr;
  size_t array_size = n * size;
  /* Check for overflow before allocating. */
  if (is_overflow(array_size, n, size)) {
    log_errlog(WARNING_LEVEL, ER_PFS_MALLOC_ARRAY_OVERFLOW, n, size,
               klass->m_class.m_name.str());
    return nullptr;
  }

  if (nullptr == (ptr = pfs_malloc(klass, array_size, flags))) {
    log_errlog(WARNING_LEVEL, ER_PFS_MALLOC_ARRAY_OOM, array_size,
               klass->m_class.m_name.str());
  }
  return ptr;
}

/**
  Free array allocated by @sa pfs_malloc_array.
  @param klass performance schema memory class
  @param n     number of array elements
  @param size  element size
  @param ptr   pointer to memory
*/
void pfs_free_array(PFS_builtin_memory_class *klass, size_t n, size_t size,
                    void *ptr) {
  if (ptr == nullptr) {
    return;
  }
  size_t array_size = n * size;
  /* Overflow should have been detected by pfs_malloc_array. */
  assert(!is_overflow(array_size, n, size));
  return pfs_free(klass, array_size, ptr);
}

/**
  Detect multiplication overflow.
  @param product  multiplication product
  @param n1  operand
  @param n2  operand
  @return true if multiplication caused an overflow.
*/
bool is_overflow(size_t product, size_t n1, size_t n2) {
  if (n1 != 0 && (product / n1 != n2)) {
    return true;
  } else {
    return false;
  }
}

void pfs_print_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  /*
    Printing to anything else, like the error log, would generate even more
    recursive calls to the performance schema implementation
    (file I/O is instrumented), so that could lead to catastrophic results.
    Printing to something safe, and low level: stderr only.
  */
  vfprintf(stderr, format, args);
  va_end(args);
  fflush(stderr);
}

/** Convert raw ip address into readable format. Do not do a reverse DNS lookup.
 */

uint pfs_get_socket_address(char *host, uint host_len, uint *port,
                            const struct sockaddr_storage *src_addr,
                            socklen_t) {
  assert(host);
  assert(src_addr);
  assert(port);

  memset(host, 0, host_len);
  *port = 0;

  switch (src_addr->ss_family) {
    case AF_INET: {
      if (host_len < INET_ADDRSTRLEN + 1) {
        return 0;
      }
      const struct sockaddr_in *sa4 =
          pointer_cast<const struct sockaddr_in *>(src_addr);
#ifdef _WIN32
      /* Older versions of Windows do not support inet_ntop() */
      getnameinfo(pointer_cast<struct sockaddr *>(
                      const_cast<struct sockaddr_in *>(sa4)),
                  sizeof(struct sockaddr_in), host, host_len, NULL, 0,
                  NI_NUMERICHOST);
#else
      inet_ntop(AF_INET, &(sa4->sin_addr), host, INET_ADDRSTRLEN);
#endif
      *port = ntohs(sa4->sin_port);
    } break;

    case AF_INET6: {
      if (host_len < INET6_ADDRSTRLEN + 1) {
        return 0;
      }
      const struct sockaddr_in6 *sa6 =
          pointer_cast<const struct sockaddr_in6 *>(src_addr);
#ifdef _WIN32
      /* Older versions of Windows do not support inet_ntop() */
      getnameinfo(pointer_cast<struct sockaddr *>(
                      const_cast<struct sockaddr_in6 *>(sa6)),
                  sizeof(struct sockaddr_in6), host, host_len, NULL, 0,
                  NI_NUMERICHOST);
#else
      inet_ntop(AF_INET6, &(sa6->sin6_addr), host, INET6_ADDRSTRLEN);
#endif
      *port = ntohs(sa6->sin6_port);
    } break;

    default:
      break;
  }

  /* Return actual IP address string length */
  return (strlen((const char *)host));
}
