/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "util/require.h"
#include "ndb_config.h"

#ifdef _WIN32
#include <malloc.h> // _aligned_alloc
#include <Windows.h>
#else
#include <errno.h>
#include <stdlib.h> // aligned_alloc or posix_memalign
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h> // explict_bzero or memset_s
#include <sys/mman.h>
#include <unistd.h> // sysconf
#endif

#include <NdbMem.h>

int NdbMem_MemLockAll(int i){
  if (i == 1)
  {
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT) && defined (MCL_FUTURE)
    return mlockall(MCL_CURRENT | MCL_FUTURE);
#else
    return -1;
#endif
  }
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  return mlockall(MCL_CURRENT);
#else
  return -1;
#endif
}

int NdbMem_MemUnlockAll(){
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  return munlockall();
#else
  return -1;
#endif
}

int NdbMem_MemLock(const void * ptr [[maybe_unused]],
                   size_t len [[maybe_unused]])
{
#if defined(HAVE_MLOCK)
  return mlock(ptr, len);
#else
  return -1;
#endif
}

#if defined(VM_TRACE) && !defined(__APPLE__)

/**
 * Experimental functions to manage virtual memory without backing, nor
 * in memory nor on disk.
 *
 * These functions can be used in debug builds to test some extreme memory
 * configuration not normally possible since not enough physical memory
 * pages to map to whole address space.
 *
 * Likely these functions will not work as expected on all platforms, nor
 * does them consider large pages or NUMA in any way.
 *
 */

/**
 * NdbMem_ReserveSpace
 *
 * Should reserve only an address space in the process virtual memory,
 * without any physical pages reserved neither in memory nor on disk.
 *
 * Especially the address space should not be dumped if process crash.
 *
 * Also if lock all memory is configured, there should be no memory
 * locked to the reserved address space.
 */
int NdbMem_ReserveSpace(void** ptr, size_t len)
{
  void * p;
  if (ptr == nullptr)
  {
    return -1;
  }
#ifdef _WIN32
  p = VirtualAlloc(*ptr, len, MEM_RESERVE, PAGE_NOACCESS);
  *ptr = p;
  return (p == NULL) ? -1 : 0;
#elif defined(MAP_NORESERVE)
  /*
   * MAP_NORESERVE is essential to not reserve swap space on Solaris.
   * If this code is activated on operating systems not having the non standard
   * MAP_NORESERVE one need to find out how to do it instead or make sure these
   * functions (NdbMem_ReserveSpace(), NdbMem_PopulateSpace(),
   * NdbMem_FreeSpace()) are not used on that platform.
   *
   * This code is currently only used in debug build there can be called with a
   * huge length as 128TB, not all intended to be backed my storage.
   *
   * Even when we start using these function in production code and all mapped
   * memory should be backed it makes sense not to enforce swap space
   * reservation since swapping the memory of data node is not recommended, on
   * the contrary it should be locked to RAM if possible.
   *
   * If only memory that should be backed is mapped one could consider skipping
   * MAP_NORESERVE to have an early error (for badly configured systems) rather
   * than have some undefined behaviour in later calls to NdbMem_PopulateSpace.
   */
  p = mmap(*ptr,
           len,
           PROT_NONE,
           MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE,
           -1,
           0);
  if (p == MAP_FAILED)
  {
    *ptr = nullptr;
    return -1;
  }
#if defined(MADV_DONTDUMP)
  if (-1 == madvise(p, len, MADV_DONTDUMP))
  {
    require(0 == munmap(p, len));
    *ptr = nullptr;
    return -1;
  }
#endif
  *ptr = p;
  return 0;
#elif defined(MAP_GUARD) /* FreeBSD */
  p = mmap(*ptr,
           len,
           PROT_NONE,
           MAP_ANONYMOUS | MAP_PRIVATE | MAP_GUARD,
           -1,
           0);
  *ptr = p;
  return 0;
#else
#error Need mmap() to not reserve swap for mapping.
#endif
}
#endif

/**
 * NdbMem_PopulateSpace
 *
 * Make sure there will be memory reserved and initialized for requested
 * address range.
 *
 * The range should be dumped if process crash, and locked if that is
 * configured.
 *
 * The range must be a subrange of what is returned by a preceding call
 * to NdbMem_ReserveSpace.
 *
 * Range must also be aligned to page boundaries.
 */
int NdbMem_PopulateSpace(void* ptr, size_t len)
{
#ifdef _WIN32
  void* p = VirtualAlloc(ptr, len, MEM_COMMIT, PAGE_READWRITE);
  return (p == NULL) ? -1 : 0;
#elif defined(MAP_GUARD) /* FreeBSD */
  void* p = mmap(ptr,
                 len,
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                 -1,
                 0);
  if (p == MAP_FAILED)
  {
    return -1;
  }
  require(p == ptr);
  return 0;
#else /* Linux, Solaris */
  int ret = mprotect(ptr, len, PROT_READ | PROT_WRITE);
  if (ret == 0)
  {
    char* p = (char*)ptr;
    size_t i;
    // Assume page size is a multiple of 4096 bytes
    for(i=0; i < len; i += 4096)
    {
      p[i]=0;
    }
#ifdef MADV_DODUMP
    ret = madvise(ptr, len, MADV_DODUMP);
    if (ret == -1)
    {
#ifdef __sun
      if (errno == EINVAL)
      {
        /*
         * Assume reservation of space was done without MADV_DONTDUMP too.
         * Probably not using NdbMem_ReserveSpace but by calling mmap in some
         * other way.
         * In that case all memory is dumped anyway.
         *
         * This was a problem when compiling on a newer Solaris supporting
         * MADV_DONTDUMP and MADV_DODUMP and then running on an older Solaris
         * not supporting these.
         */
        errno = 0;
        return 0;
      }
#endif
      /* Unexpected failure, make memory unaccessible again. */
      (void) mprotect(ptr, len, PROT_NONE);
    }
#endif
  }
  return ret;
#endif
}

#if defined(VM_TRACE) && !defined(__APPLE__)

/**
 * NdbMem_FreeSpace
 *
 * Release address space previously reserved by NdbMem_ReserveSpace.
 *
 * The input arguments must be exactly what was returned and used for
 * NdbMem_ReserveSpace call.
 *
 * Any memory reserved with NdbMem_PopulateMemory will be released without
 * further action.
 */
int NdbMem_FreeSpace(void* ptr, size_t len)
{
#ifdef _WIN32
  const BOOL ok = VirtualFree(ptr, 0, MEM_RELEASE);
  (void)len;
  return ok ? 0 : -1;
#else
  return munmap(ptr, len);
#endif
}

#endif

void* NdbMem_AlignedAlloc(size_t alignment, size_t size)
{
  void* p = nullptr;
#if defined(_ISOC11_SOURCE)
  p = aligned_alloc(alignment, size);
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
  int err = posix_memalign(&p, alignment, size);
  if (err != 0) errno = err;
#elif defined(_WIN32)
  p = _aligned_malloc(size, alignment);
#else
  if (alignment < sizeof(void*))
  {
    alignment = sizeof(void*);
  }
  char*charp = (char*) malloc(size + alignment);
  if (charp != nullptr)
  {
    void* q = (void*)(charp + (alignment - ((uintptr_t)charp % alignment)));
    void** qp = (void**)q;
    qp[-1] = p;
    p = q;
  }
#endif
  return p;
}

void NdbMem_AlignedFree(void* p)
{
#if defined(_ISOC11_SOURCE) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L)
  free(p);
#elif defined(_WIN32)
  _aligned_free(p);
#else
  void** qp = (void**)p;
  p = qp[-1];
  free(p);
#endif
}

size_t NdbMem_GetSystemPageSize()
{
#ifndef _WIN32
  return (size_t) sysconf(_SC_PAGESIZE);
#else
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
#endif
}

void NdbMem_SecureClear(void* ptr, size_t len)
{
#if defined(_WIN32)
  SecureZeroMemory(ptr, len);
#elif defined(HAVE_MEMSET_S)
  memset_s(ptr, len, 0, len);

  /*
   * Solaris 11.4 SRU 12 explicit_bzero was introduced.
   *
   * But since we allow builds on such new Solaris to run on older Solaris 11.4
   * versions there system libraries does not have explicit_bzero we can get a
   * runtime link error.
   *
   * To avoid that we will avoid explicit_bzero on Solaris.
   */
#elif defined(HAVE_EXPLICIT_BZERO) && !defined(__sun)
  explicit_bzero(ptr, len);
#else
  /*
   * As long as no compiler take the effort and optimize away calls to
   * NdbMem_SecureClear, the memset should always be part of
   * NdbMem_SecureClear since whether cleared area will be further accessed or
   * not is beyond knowledge in this scope.
   */
  memset(ptr, 0, len);
#endif
}
