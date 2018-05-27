/*****************************************************************************

Copyright (c) 2015, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/os0numa.h
 NUMA API wrapper over various operating system specific APIs.

 The os_numa*() functions in this file mimic the numa*() Linux API that is
 documented in numa(3). They take the same arguments, have the same return
 type and behave in the same way. There are two purposes behind this:
 1. Have zero learning curve for developers already familiar with the Linux API.
 2. Linux's numa*() functions are documented in more detail than ours
    os_numa*(). Should any doubt arise about the behavior, the Linux docs should
    be referred.

 Created Jul 16, 2015 Vasil Dimov
 *******************************************************/

#ifndef os0numa_h
#define os0numa_h

#include "univ.i"

#ifdef HAVE_LIBNUMA
#include <numa.h>
#endif /* HAVE_LIBNUMA */

#ifdef HAVE_SCHED_GETCPU
#include <sched.h>
#endif /* HAVE_SCHED_GETCPU */

#ifdef _WIN32

/* https://msdn.microsoft.com/en-us/library/windows/desktop/dd405494(v=vs.85).aspx
 */
#define _WIN32_WINNT 0x0601
#include <WinBase.h>

#define HAVE_WINNUMA

#endif

/** Check if NUMA is available. This function must be called before any
other os_numa_*() functions and it must return != -1, otherwise the behavior
of the rest of the functions is undefined.
@return != -1 if available. */
inline int os_numa_available() {
#if defined(HAVE_LIBNUMA)
  return (numa_available());
#elif defined(HAVE_WINNUMA)
  /* See this page for a description of the NUMA Windows API:
  "NUMA Support"
  https://msdn.microsoft.com/en-us/library/windows/desktop/aa363804(v=vs.85).aspx
  */
  ULONG highest_node;

  if (!GetNumaHighestNodeNumber(&highest_node)) {
    return (-1);
  }

  if (highest_node > 0) {
    return (1);
  } else {
    return (-1);
  }
#else
  return (-1);
#endif /* HAVE_LIBNUMA */
}

/** Get the number of CPUs in the system, including disabled ones.
@return number of CPUs */
inline int os_numa_num_configured_cpus() {
#if defined(HAVE_LIBNUMA)
  return (numa_num_configured_cpus());
#elif defined(HAVE_WINNUMA)
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *buf;
  DWORD buf_bytes = 0;

  if (GetLogicalProcessorInformationEx(RelationGroup, NULL, &buf_bytes)) {
    /* GetLogicalProcessorInformationEx() unexpectedly succeeded. */
    return (1);
  }

  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    /* GetLogicalProcessorInformationEx() failed with unexpected
    error code. */
    return (1);
  }

  /* Now 'buf_bytes' contains the necessary size of buf (in bytes!). */

  buf = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(
      LocalAlloc(LMEM_FIXED, buf_bytes));

  if (buf == NULL) {
    return (1);
  }

  if (!GetLogicalProcessorInformationEx(RelationGroup, buf, &buf_bytes)) {
    /* GetLogicalProcessorInformationEx() unexpectedly failed. */
    LocalFree(buf);
    return (1);
  }

  int n_cpus = 0;
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *buf_orig = buf;

  /* Maybe this loop will iterate just once, but this is not mentioned
  explicitly anywhere in the GetLogicalProcessorInformationEx()
  documentation (when the first argument is RelationGroup). If we are
  sure that it will iterate just once, then this code could be
  simplified. */
  for (DWORD offset = 0; offset < buf_bytes;) {
    for (WORD i = 0; i < buf->Group.ActiveGroupCount; i++) {
      n_cpus += buf->Group.GroupInfo[i].ActiveProcessorCount;
    }

    offset += buf->Size;
    buf = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(
        reinterpret_cast<char *>(buf) + buf->Size);
  }

  LocalFree(buf_orig);

  return (n_cpus);
#else
  /* Consider
  boost::thread::hardware_concurrency() or
  std::thread::hardware_concurrency() (C++11) */
  ut_error;
  return (-1);
#endif
}

/** Get the NUMA node of a given CPU.
@param[in]	cpu	CPU whose NUMA node to return, must be obtained
using os_getcpu().
@return NUMA node id */
inline int os_numa_node_of_cpu(int cpu) {
#if defined(HAVE_LIBNUMA)
  return (numa_node_of_cpu(cpu));
#elif defined(HAVE_WINNUMA)
  PROCESSOR_NUMBER p;
  USHORT node;

  p.Group = cpu >> 6;
  p.Number = cpu & 63;

  if (GetNumaProcessorNodeEx(&p, &node)) {
    return (static_cast<int>(node));
  } else {
    return (0);
  }
#else
  ut_error;
  return (-1);
#endif
}

/** Allocate a memory on a given NUMA node.
@param[in]	size	number of bytes to allocate
@param[in]	node	NUMA node on which to allocate the memory
@return pointer to allocated memory or NULL if allocation failed */
inline void *os_numa_alloc_onnode(size_t size, int node) {
#if defined(HAVE_LIBNUMA)
  return (numa_alloc_onnode(size, node));
#elif defined(HAVE_WINNUMA)
  return (VirtualAllocExNuma(GetCurrentProcess(), NULL, size,
                             MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE, node));
#else
  ut_error;
  return (NULL);
#endif
}

/** Free a memory allocated by os_numa_alloc_onnode().
@param[in]	ptr	pointer to memory to free
@param[in]	size	size of the memory */
inline void os_numa_free(void *ptr, size_t size) {
#if defined(HAVE_LIBNUMA)
  numa_free(ptr, size);
#elif defined(HAVE_WINNUMA)
  VirtualFreeEx(GetCurrentProcess(), ptr, 0, MEM_DECOMMIT | MEM_RELEASE);
#else
  ut_error;
#endif
}

#if defined(HAVE_SCHED_GETCPU) || defined(HAVE_WINNUMA)

#define HAVE_OS_GETCPU

/** Get the number of the CPU that executes the current thread now.
@return CPU number */
inline int os_getcpu() {
#if defined(HAVE_SCHED_GETCPU)
  return (sched_getcpu());
#elif defined(HAVE_WINNUMA)
  PROCESSOR_NUMBER p;

  GetCurrentProcessorNumberEx(&p);

  return (static_cast<int>(p.Group << 6 | p.Number));
#endif
}
#endif /* HAVE_SCHED_GETCPU || HAVE_WINNUMA */

#endif /* os0numa_h */
