/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/memutils.h
Memory utilities for temptable-allocator. */

#ifndef TEMPTABLE_MEM_H
#define TEMPTABLE_MEM_H

#include "my_config.h"

#include <fcntl.h>  // MAP_FAILED, etc.
#include <cstddef>  // size_t
#include <cstdlib>  // malloc(), free()

// clang-format off
#ifdef _WIN32
/*
https://msdn.microsoft.com/en-us/library/windows/desktop/dd405487(v=vs.85).aspx
https://msdn.microsoft.com/en-us/library/windows/desktop/dd405494(v=vs.85).aspx
https://msdn.microsoft.com/en-us/library/windows/desktop/aa366891(v=vs.85).aspx
 */
#define _WIN32_WINNT 0x0601
#include <Windows.h>

#define HAVE_WINNUMA
#endif /* _WIN32 */
// clang-format on

#ifdef HAVE_LIBNUMA
#define TEMPTABLE_USE_LINUX_NUMA
#include <numa.h> /* numa_*() */
#endif            /* HAVE_LIBNUMA */

#include "my_dbug.h"
#include "my_io.h"       // FN_REFLEN
#include "my_sys.h"      // my_xyz(), create_temp_file(), ...
#include "sql/mysqld.h"  // temptable_use_mmap
#include "storage/temptable/include/temptable/result.h"

namespace temptable {

#if defined(HAVE_WINNUMA)
extern DWORD win_page_size;
#endif /* HAVE_WINNUMA */

/** Type of memory allocated. */
enum class Source {
  /** Memory is allocated on disk, using mmap()'ed file. */
  MMAP_FILE,
  /** Memory is allocated from RAM, using malloc() for example. */
  RAM,
};

/** Primary-template (functor) class for memory-utils. */
template <Source>
struct Memory {
  static void *allocate(size_t bytes);
  static void deallocate(void *ptr, size_t bytes);
};

/** Template specialization for RAM-based allocation/deallocation. */
template <>
struct Memory<Source::RAM> {
  /** Allocates memory from RAM.
   *
   * Linux:
   *    - If support for NUMA is compiled in, and NUMA is available on the
   *      platform, NUMA allocation will be used.
   *    - If support for NUMA is compiled in, but NUMA is NOT available on the
   *      platform, non-NUMA allocation will be used (fallback to e.g. malloc).
   * Windows:
   *    - If support for NUMA is compiled in, NUMA allocation will be used.
   *
   * Throws Result::OUT_OF_MEM if allocation was unsuccessful.
   *
   * [in] Size of the memory to be allocated.
   * @return Pointer to allocated memory. */
  static void *allocate(size_t bytes) {
    void *memory = fetch(bytes);
    if (memory == nullptr) {
      throw Result::OUT_OF_MEM;
    }
    return memory;
  }

  /** Deallocates memory from RAM.
   *
   * [in] Pointer to the memory to be deallocated..
   * [in] Size of the memory to be deallocated. */
  static void deallocate(void *ptr, size_t bytes) { drop(ptr, bytes); }

 private:
  static void *fetch(size_t bytes);
  static void drop(void *ptr, size_t bytes);

 private:
#ifdef TEMPTABLE_USE_LINUX_NUMA
  /** Set to true if Linux's numa_available() reports "available" (!= -1). */
  static const bool linux_numa_available;
#endif /* TEMPTABLE_USE_LINUX_NUMA */
};

/** Template specialization for MMAP-based allocation/deallocation. */
template <>
struct Memory<Source::MMAP_FILE> {
  /** Allocates memory from MMAP-ed file. Throws Result::RECORD_FILE_FULL
   * if temptable_use_map is not enabled or if allocation was unsuccessful.
   *
   * [in] Size of the memory to be allocated.
   * @return Pointer to allocated memory. */
  static void *allocate(size_t bytes) {
    void *memory = fetch(bytes);
    if (memory == nullptr) {
      throw Result::RECORD_FILE_FULL;
    }
    return memory;
  }

  /** Deallocates memory from MMAP-ed file.
   *
   * [in] Pointer to the memory to be deallocated..
   * [in] Size of the memory to be deallocated. */
  static void deallocate(void *ptr, size_t bytes) { drop(ptr, bytes); }

 private:
  static void *fetch(size_t bytes);
  static void drop(void *ptr, size_t bytes);
};

inline void *Memory<Source::RAM>::fetch(size_t bytes) {
#if defined(TEMPTABLE_USE_LINUX_NUMA)
  if (linux_numa_available) {
    return numa_alloc_local(bytes);
  } else {
    return malloc(bytes);
  }
#elif defined(HAVE_WINNUMA)
  PROCESSOR_NUMBER processorNumber;
  USHORT numaNodeId;
  GetCurrentProcessorNumberEx(&processorNumber);
  GetNumaProcessorNodeEx(&processorNumber, &numaNodeId);
  bytes =
      (bytes + win_page_size - 1) & ~(static_cast<size_t>(win_page_size) - 1);
  return VirtualAllocExNuma(GetCurrentProcess(), nullptr, bytes,
                            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE,
                            numaNodeId);
#else
  return malloc(bytes);
#endif
}

inline void Memory<Source::RAM>::drop(void *ptr, size_t bytes) {
  (void)bytes;
#if defined(TEMPTABLE_USE_LINUX_NUMA)
  if (linux_numa_available) {
    numa_free(ptr, bytes);
  } else {
    free(ptr);
  }
#elif defined(HAVE_WINNUMA)
  BOOL ret [[maybe_unused]] = VirtualFree(ptr, 0, MEM_RELEASE);
  assert(ret != 0);
#else
  free(ptr);
#endif
}

inline void *Memory<Source::MMAP_FILE>::fetch(size_t bytes) {
  DBUG_EXECUTE_IF("temptable_fetch_from_disk_return_null", return nullptr;);

#ifdef _WIN32
  const int mode = _O_RDWR;
#else
  const int mode = O_RDWR;
#endif /* _WIN32 */

  char file_path[FN_REFLEN];
  File f = create_temp_file(file_path, mysql_tmpdir, "mysql_temptable.", mode,
                            UNLINK_FILE, MYF(MY_WME));
  if (f < 0) {
    return nullptr;
  }

  /* This will write `bytes` 0xa bytes to the file on disk. */
  if (my_fallocator(f, bytes, 0xa, MYF(MY_WME)) != 0 ||
      my_seek(f, 0, MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR) {
    my_close(f, MYF(MY_WME));
    return nullptr;
  }

  void *ptr = my_mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0);

  // NOTE: Closing the file-descriptor __immediately__ after MMAP-ing it does
  // not impact the MMAP functionality at all. Both POSIX and Microsoft
  // implementations keep the reference to the file-descriptor in their internal
  // structures so we are free to get rid of it.
  //
  // What does it mean to us? We don't have to keep the file-descriptor
  // ourselves in memory and that results with much more simplified
  // implementation of fetch/drop.
  //
  // References:
  // POSIX (http://pubs.opengroup.org/onlinepubs/7908799/xsh/mmap.html)
  //    The mmap() function adds an extra reference to the file
  //    associated with the file descriptor fildes which is not removed
  //    by a subsequent close() on that file descriptor. This reference
  //    is removed when there are no more mappings to the file.
  //
  // From MSDN
  // (https://docs.microsoft.com/en-us/windows/desktop/api/memoryapi/nf-memoryapi-unmapviewoffile)
  //    Although an application may close the file handle used to create
  //    a file mapping object, the system holds the corresponding file
  //    open until the last view of the file is unmapped.
  my_close(f, MYF(MY_WME));

  return (ptr == MAP_FAILED) ? nullptr : ptr;
}

inline void Memory<Source::MMAP_FILE>::drop(void *ptr, size_t bytes) {
  my_munmap(ptr, bytes);
}

} /* namespace temptable */

#endif /* TEMPTABLE_MEM_H */
