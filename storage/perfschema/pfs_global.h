/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_GLOBAL_H
#define PFS_GLOBAL_H

#include "my_config.h"

#include <atomic>

#include <stddef.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <sys/types.h>
#ifdef WIN32
#include <ws2tcpip.h>  // socklen_t
#endif

#include "my_compiler.h"
#include "my_inttypes.h"
#include "sql/thr_malloc.h"

#include <mysql/components/services/mysql_server_telemetry_traces_service.h>

/**
  @file storage/perfschema/pfs_global.h
  Miscellaneous global dependencies (declarations).
*/

/** True when the performance schema is initialized. */
extern bool pfs_initialized;

#if defined(HAVE_POSIX_MEMALIGN) || defined(HAVE_MEMALIGN) || \
    defined(HAVE_ALIGNED_MALLOC)
#define PFS_ALIGNMENT 64
#define PFS_ALIGNED alignas(PFS_ALIGNMENT)
#else
/*
  Known platforms that do not provide aligned memory:
  - MacOSX Darwin (osx10.5)
  For these platforms, compile without the alignment optimization.
*/
#define PFS_ALIGNED
#endif /* HAVE_POSIX_MEMALIGN || HAVE_MEMALIGN || HAVE_ALIGNED_MALLOC */

#ifdef CPU_LEVEL1_DCACHE_LINESIZE
#define PFS_CACHE_LINE_SIZE CPU_LEVEL1_DCACHE_LINESIZE
#else
#define PFS_CACHE_LINE_SIZE 128
#endif

/**
  An atomic @c uint32 variable, guaranteed to be alone in a CPU cache line.
  This is for performance, for variables accessed very frequently.
*/
struct PFS_cacheline_atomic_uint32 {
  std::atomic<uint32> m_u32;
  char m_full_cache_line[PFS_CACHE_LINE_SIZE - sizeof(std::atomic<uint32>)];

  PFS_cacheline_atomic_uint32() : m_u32(0) {}
};

/**
  An atomic @c uint64 variable, guaranteed to be alone in a CPU cache line.
  This is for performance, for variables accessed very frequently.
*/
struct PFS_cacheline_atomic_uint64 {
  std::atomic<uint64> m_u64;
  char m_full_cache_line[PFS_CACHE_LINE_SIZE - sizeof(std::atomic<uint64>)];

  PFS_cacheline_atomic_uint64() : m_u64(0) {}
};

/**
  An atomic @c size_t variable, guaranteed to be alone in a CPU cache line.
  This is for performance, for variables accessed very frequently.
*/
struct PFS_cacheline_atomic_size_t {
  std::atomic<size_t> m_size_t;
  char m_full_cache_line[PFS_CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)];

  PFS_cacheline_atomic_size_t() : m_size_t(0) {}
};

/**
  An atomic<T> variable, guaranteed to be alone in a CPU cache line.
  This is for performance, for variables accessed very frequently.
*/
template <class T>
struct PFS_cacheline_atomic_ptr {
  std::atomic<T> m_ptr;
  char m_full_cache_line[PFS_CACHE_LINE_SIZE - sizeof(std::atomic<T>)];

  PFS_cacheline_atomic_ptr() : m_ptr(nullptr) {}
};

struct PFS_builtin_memory_class;

/** Memory allocation for the performance schema. */
void *pfs_malloc(PFS_builtin_memory_class *klass, size_t size, myf flags);

/** Allocate an array of structures with overflow check. */
void *pfs_malloc_array(PFS_builtin_memory_class *klass, size_t n, size_t size,
                       myf flags);

/**
  Helper, to allocate an array of structures.
  @param k memory class
  @param n number of elements in the array
  @param s size of array element
  @param T type of an element
  @param f flags to use when allocating memory
*/
#define PFS_MALLOC_ARRAY(k, n, s, T, f) \
  reinterpret_cast<T *>(pfs_malloc_array((k), (n), (s), (f)))

/** Free memory allocated with @sa pfs_malloc. */
void pfs_free(PFS_builtin_memory_class *klass, size_t size, void *ptr);

/** Free memory allocated with @sa pfs_malloc_array. */
void pfs_free_array(PFS_builtin_memory_class *klass, size_t n, size_t size,
                    void *ptr);

/**
  Helper, to free an array of structures.
  @param k memory class
  @param n number of elements in the array
  @param s size of array element
  @param p the array to free
*/
#define PFS_FREE_ARRAY(k, n, s, p) pfs_free_array((k), (n), (s), (p))

/** Detect multiplication overflow. */
bool is_overflow(size_t product, size_t n1, size_t n2);

uint pfs_get_socket_address(char *host, uint host_len, uint *port,
                            const struct sockaddr_storage *src_addr,
                            socklen_t src_len);

/**
  Helper to allocate an object from mem_root.
  @param CLASS Class to instantiate
*/
#define PFS_NEW(CLASS) (new (*THR_MALLOC) CLASS())

void pfs_print_error(const char *format, ...)
    MY_ATTRIBUTE((format(printf, 1, 2)));

/**
  Given an array defined as T ARRAY[MAX],
  check that an UNSAFE pointer actually points to an element
  within the array.
*/
#define SANITIZE_ARRAY_BODY(T, ARRAY, MAX, UNSAFE)         \
  intptr offset;                                           \
  if ((&ARRAY[0] <= UNSAFE) && (UNSAFE < &ARRAY[MAX])) {   \
    offset = ((intptr)UNSAFE - (intptr)ARRAY) % sizeof(T); \
    if (offset == 0) {                                     \
      return UNSAFE;                                       \
    }                                                      \
  }                                                        \
  return NULL

#endif
