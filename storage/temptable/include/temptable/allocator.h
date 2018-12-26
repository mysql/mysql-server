/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file storage/temptable/include/temptable/allocator.h
TempTable custom allocator. */

#ifndef TEMPTABLE_ALLOCATOR_H
#define TEMPTABLE_ALLOCATOR_H

#include <fcntl.h>

#include <algorithm> /* std::max */
#include <atomic>    /* std::atomic */
#include <cstddef>   /* size_t */
#include <cstdlib>   /* malloc(), free() */
#include <limits>    /* std::numeric_limits */
#include <new>       /* new */
#include <sstream>   /* std::stringstream */
#include <utility>   /* std::forward */
#include <vector>    /* std::vector */

#ifndef DBUG_OFF
#include <unordered_set>
#endif /* DBUG_OFF */

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

#include "my_config.h"

#include "memory_debugging.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/psi_base.h"
#include "mysql/psi/psi_memory.h"
#include "sql/mysqld.h"
#include "storage/temptable/include/temptable/constants.h"
#include "storage/temptable/include/temptable/result.h"
#include "storage/temptable/include/temptable/misc.h"

#ifdef HAVE_LIBNUMA
#define TEMPTABLE_USE_LINUX_NUMA
#endif /* HAVE_LIBNUMA */

#ifdef TEMPTABLE_USE_LINUX_NUMA
#include <numa.h> /* numa_*() */
#endif            /* TEMPTABLE_USE_LINUX_NUMA */

#ifdef HAVE_PSI_MEMORY_INTERFACE
#define TEMPTABLE_PFS_MEMORY

/* Enabling this causes ~ 4% performance drop in sysbench distinct ranges. */
//#define TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
#endif /* HAVE_PSI_MEMORY_INTERFACE */

namespace temptable {

#if defined(HAVE_WINNUMA)
extern DWORD win_page_size;
#endif /* HAVE_WINNUMA */

#ifdef TEMPTABLE_PFS_MEMORY
#ifdef TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
/** PFS key to account logical memory allocations and deallocations. Logical
 * is any request for new memory that arrives to the allocator. */
extern PSI_memory_key mem_key_logical;
#endif /* TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL */

/** PFS key to account physical allocations and deallocations from disk. After
 * we have allocated more than `temptable_max_ram` we start taking memory from
 * the OS disk, using mmap()'ed files. */
extern PSI_memory_key mem_key_physical_disk;

/** PFS key to account physical allocations and deallocations from RAM. Before
 * we have allocated more than `temptable_max_ram` we take memory from the OS
 * RAM, using e.g. malloc(). */
extern PSI_memory_key mem_key_physical_ram;

/** Array of PFS keys. */
extern PSI_memory_info pfs_info[];

/** Number of elements inside `pfs_info[]`. */
extern const size_t pfs_info_num_elements;
#endif /* TEMPTABLE_PFS_MEMORY */

#ifdef TEMPTABLE_USE_LINUX_NUMA
/** Set to true if Linux's numa_available() reports "available" (!= -1). */
extern bool linux_numa_available;
#endif /* TEMPTABLE_USE_LINUX_NUMA */

/** Custom memory allocator. All dynamic memory used by the TempTable engine
 * is allocated through this allocator.
 *
 * The purpose of this allocator is to minimize the number of calls to the OS
 * for allocating new memory (e.g. malloc()) and to improve the spatial
 * locality of reference. The most common use case, for which it is optimized,
 * is to have the following performed by a single thread:
 * - allocate many times (creation of a temp table and inserting data into it).
 * - use the allocated memory (selects on the temp table).
 * - free all the pieces (drop of the temp table).
 *
 * The allocator allocates memory from the OS in large blocks (e.g. a few MiB)
 * and uses these blocks to feed allocation requests. A block consists of a
 * header and a sequence of chunks:
 * - bytes [0, 7]: 8 bytes for the block size (set at block creation and never
 *   changed later).
 * - bytes [8, 15]: 8 bytes for the number of used/allocated chunks from this
 *   block (set to 0 at block creation).
 * - bytes [16, 23]: 8 bytes for the offset of the first byte from the block
 *   start that is free and can be used by the next allocation request (set
 *   to 24 at block creation (3 * 8 bytes)). We call this first pristine offset.
 * - bytes [24, block size) a sequence of chunks appended to each other.
 * A chunk structure is:
 * - bytes [0, 7]: 8 bytes that designate the offset of the chunk from
 *   the start of the block. This is used in order to be able to deduce
 *   the block start from a given chunk. The offset of the first chunk is
 *   24 (appended after the block size (8), number of allocated chunks (8)
 *   and the first pristine offset (8)).
 * - bytes [8, chunk size): user data, pointer to this is returned to the
 *   user after a successfull allocation request.
 *
 * With the above structure we can deduce the block into which a user pointer
 * (returned by an allocation request) belongs to in a constant time. Each
 * allocation has an overhead of 8 bytes.
 *
 * We do not store a list of all allocated blocks, only a pointer to the current
 * block which has not yet been entirely filled up and the overall number of
 * blocks. We do not store a list of the chunks inside a block either.
 *
 * Allocation:
 * - if the current block does not have enough space:
 *     create a new block and make it the current (lose the pointer to the
 *     previous current block).
 * - increment the number of allocated chunks by 1.
 * - in the first pristine location - write its offset from the block
 *   start (8 bytes).
 * - increment the first pristine offset with 8 + requested bytes by the user.
 * - return a pointer to the previous first pristine + 8 to the user.
 *
 * Deallocation:
 * - read 8 bytes before the provided pointer and derive the block start.
 * - decrement the number of used chunks by 1.
 * - if this was the last chunk in the block and this is not the last block:
 *     destroy the block, returning the memory to the OS.
 * - keep the last block for reuse even if all chunks from it are removed, it
 *   will be destroyed when the thread terminates. When the last chunk from
 *   the last block is removed, instead of destroying the block reset its first
 *   pristine byte offset to 24. */
template <class T>
class Allocator {
 public:
  typedef T *pointer;
  typedef const T *const_pointer;
  typedef T &reference;
  typedef const T &const_reference;
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  template <class U>
  struct rebind {
    typedef Allocator<U> other;
  };

  /** Constructor. */
  Allocator();

  /** Constructor from allocator of another type. The state is copied into the
   * new object. */
  template <class U>
  Allocator(
      /** [in] Source Allocator object. */
      const Allocator<U> &other);

  /** Move constructor from allocator of another type. */
  template <class U>
  Allocator(
      /** [in,out] Source Allocator object. */
      Allocator<U> &&other);

  /** Destructor. */
  ~Allocator();

  /** Assignment operator, not used, thus disabled. */
  template <class U>
  void operator=(const Allocator<U> &) = delete;

  /** Move operator, not used, thus disabled. */
  template <class U>
  void operator=(const Allocator<U> &&) = delete;

  /** Equality operator.
   * @return true if equal */
  template <class U>
  bool operator==(
      /** [in] Object to compare with. */
      const Allocator<U> &rhs) const;

  /** Inequality operator.
   * @return true if not equal */
  template <class U>
  bool operator!=(
      /** [in] Object to compare with. */
      const Allocator<U> &rhs) const;

  /** Allocate memory for storing `n_elements` number of elements. */
  T *allocate(
      /** [in] Number of elements that must be allocated. */
      size_t n_elements);

  /** Free a memory allocated by allocate(). */
  void deallocate(
      /** [in,out] Pointer to memory to free. */
      T *ptr,
      /** [in] Number of elements allocated. */
      size_t n_elements);

  /** Construct one object of type `U` on an already allocated chunk of memory,
   * which must be large enough to store it. */
  template <class U, class... Args>
  void construct(
      /** [in] Memory where to create the object. */
      U *mem,
      /** Arguments to pass to U's constructor. */
      Args &&... args);

  /** Destroy an object of type `U`. The memory is not returned to the OS, this
   * is the counterpart of `construct()`. */
  template <class U>
  void destroy(
      /** [in, out] Object to destroy. */
      U *p);

  /** Initialize necessary structures. Called once in the OS process lifetime,
   * before other methods. */
  static void init();

  /** Clean up the shared block used by all Allocator objects in the current OS
   * thread. Called once per OS thread destruction. */
  static void end_thread();

  /** Current not-yet-full block to feed allocations from. */
  uint8_t *m_current_block;

  /** Number of created blocks so far (by this Allocator object). This can go
   * negative because of the way std:: containers use the Allocator:
   * - start with an Allocator a1
   * - copy-construct a2 from a1
   * - allocate a chunk of memory using a2
   * - destroy a2
   * - copy-construct a3 from a1
   * - ask a3 to deallocate the chunk of memory allocated by a2.
   * We use this number only as a hint as to how big block to create when a new
   * block needs to be created. If it is negative, then we proceed as if it is
   * 0. */
  int64_t m_number_of_blocks;

 private:
  /** Type of memory allocated. */
  enum class Mem_type : uintptr_t {
    /** Memory is allocated on disk, using mmap()'ed file. */
    DISK,
    /** Memory is allocated from RAM, using malloc() for example. */
    RAM,
  };

  /** The numeric type used for storing numbers inside the block (eg block
   * size). We use word-sized type to ease with alignment, otherwise uint32_t
   * would suffice too. */
  typedef uintptr_t Block_offset;

  /** Memory allocated by the Allocator is aligned to this number of bytes. */
  static constexpr size_t ALIGN_TO = alignof(void *);

  /** Block header size. Each block has a header of 3 numbers:
   * - block size
   * - number of used (allocated) chunks in this block
   * - the offset of the first pristine byte (ie first free byte that can be
   *   used by the next allocation request). */
  static constexpr size_t BLOCK_HEADER_SIZE = 3 * sizeof(Block_offset);

  /** Meta bytes per allocated chunk. At the start of each chunk we put a
   * number that designates the offset from the block start. It is used to
   * deduce the block, given a chunk inside this block. */
  static constexpr size_t BLOCK_META_BYTES_PER_CHUNK = sizeof(Block_offset);

#ifndef DBUG_OFF
  /** Check if a pointer is aligned.
   * @return true if aligned */
  static bool is_aligned(
      /** [in] Pointer to check. */
      const void *ptr);
#endif /* DBUG_OFF */

  /** Fetch (allocate) memory either from RAM or from disk.
   * @return pointer to allocated memory or nullptr */
  void *mem_fetch(
      /** [in] Number of bytes to allocate. */
      size_t bytes);

  /** Drop (deallocate) memory returned by `fetch()`. */
  static void mem_drop(
      /** [in, out] Pointer returned by `fetch()`. */
      void *ptr,
      /** [in] Size of the allocated memory, as given to `fetch()`. */
      size_t bytes);

  /** Fetch (allocate) memory from RAM. The returned pointer must be passed to
   * `mem_drop_from_ram()` when no longer needed.
   * @return pointer to allocated memory or nullptr */
  void *mem_fetch_from_ram(
      /** [in] Number of bytes to allocate. */
      size_t bytes);

  /** Drop (deallocate) memory returned by `mem_fetch_from_ram()`. */
  static void mem_drop_from_ram(
      /** [in, out] Pointer returned by `mem_fetch_from_ram()`. */
      void *ptr,
      /** [in] Size of the memory, as given to `mem_fetch_from_ram()`. */
      size_t bytes);

  /** Fetch (allocate) memory from disk. The returned pointer must be passed
   * to `mem_drop_from_disk()` when no longer needed.
   * @return pointer to allocated memory or nullptr */
  void *mem_fetch_from_disk(
      /** [in] Number of bytes to allocate. */
      size_t bytes);

  /** Drop (deallocate) memory returned by `mem_fetch_from_disk()`. */
  static void mem_drop_from_disk(
      /** [in, out] Pointer returned by `mem_fetch_from_disk()`. */
      void *ptr,
      /** [in] Size of the memory, as given to `mem_fetch_from_disk()`. */
      size_t bytes);

  /** Get a pointer to the number that stores the size of a block.
   * @return pointer to the size */
  static Block_offset *block_size_ptr(
      /** [in] Block to query. */
      uint8_t *block);

  /** Get a pointer to the number that stores the count of allocated chunks
   * from a block.
   * @return pointer to the count */
  Block_offset *block_number_of_used_chunks_ptr(
      /** [in] Block to query. */
      uint8_t *block) const;

  /** Get a pointer to the number that designates the first pristine offset of
   * a block. It is the offset of the first byte that is not occupied and
   * everything after it until the end of the block can be used to feed
   * allocations.
   * @return pointer to the count */
  Block_offset *block_first_pristine_offset_ptr(
      /** [in] Block to query. */
      uint8_t *block) const;

  /** Generate a human readable string that describes a block.
   * @return human readable string */
  std::string block_to_string(
      /** [in] Block whose metadata to convert to string. */
      uint8_t *block) const;

  /** Check if a given number of bytes can be allocated from a block.
   * @return true if can be allocated */
  bool block_can_accommodate(
      /** [in] Block to check for enough space. */
      uint8_t *block,
      /** [in] Desired allocation size in bytes. */
      size_t desired_size) const;

  /** Create a block.
   * @return the newly created block */
  uint8_t *block_create(
      /** The number of bytes that first will be allocated from this block, it
       * will be able to satisfy at least one allocation of that size. */
      size_t first_alloc_size);

  /** Destroy a block created by `block_create()`. The number of allocated
   * chunks on this block must be 0 when this method is called. If it is not
   * then this means that some chunk was not removed from the block and the
   * pointer to it will become stale after this method completes. */
  void block_destroy(
      /** [in, out] Block to destroy. */
      uint8_t *block);

  /** Just free the memory occupied by a block, returning it to the OS as the
   * last part of destroying a block. */
  static void block_destroy_mem_free(
      /** [in, out] Block to destroy. */
      uint8_t *block);

  /** Allocate some bytes from a given block. When the caller no longer needs
   * the memory returned by this method he should call
   * `block_decrement_used_chunks()` on the same block (which can be derived
   * from the pointer with `block_deduce_from_ptr()`).
   * @return a pointer to the allocated chunk */
  void *block_allocate_from(
      /** [in] Block to allocate from. */
      uint8_t *block,
      /** [in] Number of bytes to allocate. */
      size_t requested_size);

  /** Derive a block from a pointer returned by `block_allocate_from()`.
   * @return block that contains `ptr` */
  uint8_t *block_deduce_from_ptr(
      /** [in] Chunk returned by `block_allocate_from()`. */
      void *ptr) const;

  /** Decrement the number of used chunks in a block by one.
   * @return new number of used chunks after the decrement */
  Block_offset block_decrement_used_chunks(
      /** [in, out] Block whose number of used chunks to decrement. */
      uint8_t *block);
};

/** Block that is shared between different tables within a given OS thread. */
extern thread_local uint8_t *shared_block;

/** Total bytes allocated so far by all threads in RAM. This is used to check if
 * we have reached the `temptable_max_ram` threshold. */
extern std::atomic<size_t> bytes_allocated_in_ram;

/* Implementation of inlined methods. */

template <class T>
inline Allocator<T>::Allocator()
    : m_current_block(nullptr), m_number_of_blocks(0) {}

template <class T>
template <class U>
inline Allocator<T>::Allocator(const Allocator<U> &other)
    : m_current_block(other.m_current_block),
      m_number_of_blocks(other.m_number_of_blocks) {}

template <class T>
template <class U>
inline Allocator<T>::Allocator(Allocator<U> &&other)
    : m_current_block(other.m_current_block),
      m_number_of_blocks(other.m_number_of_blocks) {
  other.m_current_block = nullptr;
  other.m_number_of_blocks = 0;
}

template <class T>
inline Allocator<T>::~Allocator() {}

template <class T>
template <class U>
inline bool Allocator<T>::operator==(const Allocator<U> &) const {
  return true;
}

template <class T>
template <class U>
inline bool Allocator<T>::operator!=(const Allocator<U> &rhs) const {
  return !(*this == rhs);
}

template <class T>
inline T *Allocator<T>::allocate(size_t n_elements) {
  static_assert(sizeof(T) > 0, "Zero sized objects are not supported");
  DBUG_ASSERT(n_elements <= std::numeric_limits<size_type>::max() / sizeof(T));

  DBUG_EXECUTE_IF("temptable_allocator_oom", throw Result::OUT_OF_MEM;);

  size_t size_bytes = n_elements * sizeof(T);

  if (size_bytes == 0) {
    return nullptr;
  }

  /* Round up to the next multiple of ALIGN_TO. */
  size_bytes = (size_bytes + ALIGN_TO - 1) & ~(ALIGN_TO - 1);

  uint8_t *b;

  if (shared_block == nullptr) {
    shared_block = block_create(size_bytes);
    b = shared_block;
  } else if (block_can_accommodate(shared_block, size_bytes)) {
    b = shared_block;
  } else if (m_current_block == nullptr ||
             !block_can_accommodate(m_current_block, size_bytes)) {
    m_current_block = block_create(size_bytes);
    b = m_current_block;
  } else {
    b = m_current_block;
  }

  return reinterpret_cast<T *>(block_allocate_from(b, size_bytes));
}

template <class T>
inline void Allocator<T>::deallocate(T *ptr, size_t n_elements) {
  if (ptr == nullptr) {
    return;
  }

  size_t size_bytes = n_elements * sizeof(T);

  /* Round up to the next multiple of ALIGN_TO. */
  size_bytes = (size_bytes + ALIGN_TO - 1) & ~(ALIGN_TO - 1);

#ifdef TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
  PSI_MEMORY_CALL(memory_free)(mem_key_logical, size_bytes, nullptr);
#endif /* TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL */

  uint8_t *block = block_deduce_from_ptr(ptr);

  DBUG_PRINT("temptable_allocator",
             ("deallocate from block: ptr=%p, size=%zu, deduced_block=(%s)",
              ptr, size_bytes, block_to_string(block).c_str()));

  /* If we are freeing the rightmost chunk in this block, then lower the
   * first_pristine_offset mark, so that the memory region can be reused. */
  if (*block_first_pristine_offset_ptr(block) ==
      reinterpret_cast<uint8_t *>(ptr) - block + size_bytes) {
    *block_first_pristine_offset_ptr(block) -=
        BLOCK_META_BYTES_PER_CHUNK + size_bytes;
  }

  const auto remaining_chunks = block_decrement_used_chunks(block);

  if (remaining_chunks == 0) {
    if (block == shared_block) {
      *block_first_pristine_offset_ptr(block) = BLOCK_HEADER_SIZE;
    } else {
      block_destroy(block);

      if (block == m_current_block) {
        m_current_block = nullptr;
      }
    }
  }
}

template <class T>
template <class U, class... Args>
inline void Allocator<T>::construct(U *mem, Args &&... args) {
  new (mem) U(std::forward<Args>(args)...);
}

template <class T>
template <class U>
inline void Allocator<T>::destroy(U *p) {
  p->~U();
}

template <class T>
inline void Allocator<T>::init() {
#ifdef TEMPTABLE_PFS_MEMORY
  PSI_MEMORY_CALL(register_memory)
  ("temptable", pfs_info, pfs_info_num_elements);
#endif /* TEMPTABLE_PFS_MEMORY */

#if defined(TEMPTABLE_USE_LINUX_NUMA)
  linux_numa_available = numa_available() != -1;
#endif
}

template <class T>
inline void Allocator<T>::end_thread() {
  if (shared_block != nullptr) {
    block_destroy_mem_free(shared_block);
    shared_block = nullptr;
  }
}

#ifndef DBUG_OFF
template <class T>
inline bool Allocator<T>::is_aligned(const void *ptr) {
  return reinterpret_cast<uintptr_t>(ptr) % ALIGN_TO == 0;
}
#endif /* DBUG_OFF */

template <class T>
inline void *Allocator<T>::mem_fetch(size_t bytes) {
  DBUG_ASSERT(bytes <=
              std::numeric_limits<decltype(bytes)>::max() - sizeof(Mem_type));
  bytes += sizeof(Mem_type);

  Mem_type t;

  if (bytes_allocated_in_ram > temptable_max_ram) {
    t = Mem_type::DISK;
  } else {
    const size_t new_bytes_allocated_in_ram =
        bytes_allocated_in_ram.fetch_add(bytes) + bytes;

    DBUG_ASSERT(new_bytes_allocated_in_ram - bytes <=
                std::numeric_limits<decltype(bytes)>::max() - bytes);

    if (new_bytes_allocated_in_ram <= temptable_max_ram) {
      t = Mem_type::RAM;
    } else {
      t = Mem_type::DISK;
      bytes_allocated_in_ram.fetch_sub(bytes);
    }
  }

  void *ptr;

  if (t == Mem_type::RAM) {
    ptr = mem_fetch_from_ram(bytes);
    if (ptr == nullptr) {
      throw Result::OUT_OF_MEM;
    }
  } else {
    ptr = mem_fetch_from_disk(bytes);
    if (ptr == nullptr) {
      throw Result::RECORD_FILE_FULL;
    }
  }

  *reinterpret_cast<Mem_type *>(ptr) = t;

#ifdef TEMPTABLE_PFS_MEMORY
  const PSI_memory_key psi_key =
      t == Mem_type::RAM ? mem_key_physical_ram : mem_key_physical_disk;

  PSI_thread *owner_thread;

#ifndef DBUG_OFF
  PSI_memory_key got_key =
#endif /* DBUG_OFF */
      PSI_MEMORY_CALL(memory_alloc)(psi_key, bytes, &owner_thread);

  DBUG_ASSERT(got_key == psi_key || got_key == PSI_NOT_INSTRUMENTED);
#endif /* TEMPTABLE_PFS_MEMORY */

  static_assert(sizeof(Mem_type) % ALIGN_TO == 0,
                "Mem_type must be multiple (or equal) to ALIGN_TO");
  DBUG_ASSERT(is_aligned(ptr));

  return reinterpret_cast<Mem_type *>(ptr) + 1;
}

template <class T>
inline void Allocator<T>::mem_drop(void *ptr, size_t bytes) {
  bytes += sizeof(Mem_type);

  ptr = reinterpret_cast<Mem_type *>(ptr) - 1;

  const Mem_type t = *reinterpret_cast<Mem_type *>(ptr);

  if (t == Mem_type::RAM) {
    bytes_allocated_in_ram.fetch_sub(bytes);
    mem_drop_from_ram(ptr, bytes);
  } else {
    mem_drop_from_disk(ptr, bytes);
  }

#ifdef TEMPTABLE_PFS_MEMORY
  PSI_MEMORY_CALL(memory_free)
  (t == Mem_type::RAM ? mem_key_physical_ram : mem_key_physical_disk, bytes,
   nullptr);
#endif /* TEMPTABLE_PFS_MEMORY */
}

template <class T>
inline void *Allocator<T>::mem_fetch_from_ram(size_t bytes) {
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

template <class T>
inline void Allocator<T>::mem_drop_from_ram(void *ptr,
                                            size_t bytes TEMPTABLE_UNUSED) {
#if defined(TEMPTABLE_USE_LINUX_NUMA)
  if (linux_numa_available) {
    numa_free(ptr, bytes);
  } else {
    free(ptr);
  }
#elif defined(HAVE_WINNUMA)
  auto ret = VirtualFree(ptr, 0, MEM_RELEASE);
  assert(ret != 0);
#else
  free(ptr);
#endif
}

template <class T>
inline void *Allocator<T>::mem_fetch_from_disk(size_t bytes) {
  /* Save the file descriptor at the beginning, we will need it in
   * mem_drop_from_disk(). */
  DBUG_ASSERT(bytes <= std::numeric_limits<decltype(bytes)>::max() - ALIGN_TO);

  DBUG_EXECUTE_IF("temptable_fetch_from_disk_return_null", return nullptr;);

  bytes += ALIGN_TO;

  static_assert(sizeof(File) <= ALIGN_TO, "The type 'File' is too large.");

#ifdef _WIN32
  const int mode = _O_RDWR;
#else
  const int mode = O_RDWR;
#endif /* _WIN32 */

  char file_path[FN_REFLEN];
  File f = create_temp_file(file_path, nullptr, "mysql_temptable.", mode,
                            MYF(MY_WME));

  if (f >= 0) {
    unlink(file_path);
  } else {
    return nullptr;
  }

  /* This will write `bytes` 0x0 bytes to the file on disk. */
  if (my_fallocator(f, bytes, 0x0, MYF(MY_WME)) != 0 ||
      my_seek(f, 0, MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR) {
    my_close(f, MYF(MY_WME));
    return nullptr;
  }

  void *ptr = my_mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0);

  if (ptr == MAP_FAILED) {
    my_close(f, MYF(MY_WME));
    return nullptr;
  }

  *reinterpret_cast<File *>(ptr) = f;

  return reinterpret_cast<char *>(ptr) + ALIGN_TO;
}

template <class T>
inline void Allocator<T>::mem_drop_from_disk(void *ptr, size_t bytes) {
  bytes += ALIGN_TO;

  ptr = reinterpret_cast<char *>(ptr) - ALIGN_TO;

  File f = *reinterpret_cast<File *>(ptr);

  my_munmap(ptr, bytes);

  my_close(f, MYF(MY_WME));
}

template <class T>
inline typename Allocator<T>::Block_offset *Allocator<T>::block_size_ptr(
    uint8_t *block) {
  DBUG_ASSERT(is_aligned(block));
  return reinterpret_cast<Block_offset *>(block);
}

template <class T>
inline typename Allocator<T>::Block_offset *
Allocator<T>::block_number_of_used_chunks_ptr(uint8_t *block) const {
  DBUG_ASSERT(is_aligned(block));
  return reinterpret_cast<Block_offset *>(block + sizeof(Block_offset));
}

template <class T>
inline typename Allocator<T>::Block_offset *
Allocator<T>::block_first_pristine_offset_ptr(uint8_t *block) const {
  DBUG_ASSERT(is_aligned(block));
  return reinterpret_cast<Block_offset *>(block + 2 * sizeof(Block_offset));
}

template <class T>
inline std::string Allocator<T>::block_to_string(uint8_t *block) const {
  std::stringstream s;
  s << "address=" << static_cast<void *>(block)
    << ", size=" << *block_size_ptr(block)
    << ", num_chunks=" << *block_number_of_used_chunks_ptr(block)
    << ", first_pristine=" << *block_first_pristine_offset_ptr(block);
  return s.str();
}

template <class T>
inline bool Allocator<T>::block_can_accommodate(uint8_t *block,
                                                size_t desired_size) const {
  const auto block_size = *block_size_ptr(block);
  const auto first_pristine_offset = *block_first_pristine_offset_ptr(block);

  DBUG_ASSERT(first_pristine_offset <=
              std::numeric_limits<decltype(block_size)>::max() -
                  BLOCK_META_BYTES_PER_CHUNK - desired_size);

  return first_pristine_offset + BLOCK_META_BYTES_PER_CHUNK + desired_size <=
         block_size;
}

template <class T>
inline uint8_t *Allocator<T>::block_create(size_t first_alloc_size) {
  /* By default we allocate blocks of a few MiB each:
   * 1 MiB,
   * 2 MiB,
   * 4 MiB,
   * 8 MiB,
   * 16 MiB,
   * 32 MiB,
   * ...,
   * ALLOCATOR_MAX_BLOCK_BYTES,
   * ALLOCATOR_MAX_BLOCK_BYTES,
   * ...
   * But if the user has requested bigger size than what we intend to allocate,
   * then of course, we allocate whatever size the user requested. */

  /* Treat negative m_number_of_blocks as 0. */
  const size_t number_of_blocks =
      static_cast<size_t>(m_number_of_blocks <= 0 ? 0 : m_number_of_blocks);

  /* Our intended size in bytes:
   * 2 ^ 0 MiB,
   * 2 ^ 1 MiB,
   * 2 ^ 2 MiB,
   * ...,
   * 2 ^ ALLOCATOR_MAX_BLOCK_MB_EXP MiB. */
  size_t intended_bytes;
  if (number_of_blocks < ALLOCATOR_MAX_BLOCK_MB_EXP) {
    intended_bytes = (1ULL << number_of_blocks) * 1_MiB;
  } else {
    intended_bytes = ALLOCATOR_MAX_BLOCK_BYTES;
  }
  /* We decrease default size by amount of extra bytes added by mem_fetch(),
   * which result in default block sizes to allocate being multiplies of whole
   * pages. */
  intended_bytes -= sizeof(Mem_type);

  /* If the user wants to allocate larger piece of memory than our
   * intended_bytes, then we must of course allocate a block big enough to store
   * the user's piece of memory. This variable denotes the required bytes for
   * that. */
  const size_t required_bytes =
      BLOCK_HEADER_SIZE + BLOCK_META_BYTES_PER_CHUNK + first_alloc_size;

  const size_t size = std::max(required_bytes, intended_bytes);

  uint8_t *block = static_cast<uint8_t *>(mem_fetch(size));

  *block_size_ptr(block) = size;
  *block_number_of_used_chunks_ptr(block) = 0;
  *block_first_pristine_offset_ptr(block) = BLOCK_HEADER_SIZE;

  /* Prevent writes to the memory which we took from the OS but still have not
   * shipped outside of the Allocator. This will also prevent reads, but reads
   * would have been reported even without this because the memory we took from
   * the OS is "undefined" by default. */
  MEM_NOACCESS(block + BLOCK_HEADER_SIZE, size - BLOCK_HEADER_SIZE);

  ++m_number_of_blocks;

  DBUG_PRINT("temptable_allocator",
             ("block create: first_alloc_size=%zu, new_block=(%s)",
              first_alloc_size, block_to_string(block).c_str()));

  return block;
}

template <class T>
inline void Allocator<T>::block_destroy(uint8_t *block) {
  DBUG_ASSERT(*block_number_of_used_chunks_ptr(block) == 0);

  --m_number_of_blocks;

  block_destroy_mem_free(block);
}

template <class T>
inline void Allocator<T>::block_destroy_mem_free(uint8_t *block) {
  const auto block_size = *block_size_ptr(block);
  mem_drop(block, block_size);
}

template <class T>
inline void *Allocator<T>::block_allocate_from(uint8_t *block,
                                               size_t requested_size) {
  DBUG_ASSERT(requested_size % ALIGN_TO == 0);
  DBUG_ASSERT(block_can_accommodate(block, requested_size));

  ++*block_number_of_used_chunks_ptr(block);

  auto first_pristine_offset = *block_first_pristine_offset_ptr(block);
  DBUG_ASSERT(first_pristine_offset % ALIGN_TO == 0);

  uint8_t *p = block + first_pristine_offset;

  /* Remove the "no access" flag we set on this memory during block creation.
   * Relax it to report read+depend_on_contents. */
  MEM_UNDEFINED(p, BLOCK_META_BYTES_PER_CHUNK + requested_size);

  *reinterpret_cast<Block_offset *>(p) = first_pristine_offset;

  *block_first_pristine_offset_ptr(block) +=
      BLOCK_META_BYTES_PER_CHUNK + requested_size;

#ifdef TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
  PSI_thread *owner_thread;

#ifndef DBUG_OFF
  PSI_memory_key key =
#endif /* DBUG_OFF */
      PSI_MEMORY_CALL(memory_alloc)(mem_key_logical, requested_size,
                                    &owner_thread);

  DBUG_ASSERT(key == mem_key_logical || key == PSI_NOT_INSTRUMENTED);
#endif /* TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL */

  void *ret = static_cast<void *>(p + BLOCK_META_BYTES_PER_CHUNK);

  DBUG_PRINT(
      "temptable_allocator",
      ("allocate from block: requested_size=%zu, from_block=(%s); return=%p",
       requested_size, block_to_string(block).c_str(), ret));

  return ret;
}

template <class T>
inline uint8_t *Allocator<T>::block_deduce_from_ptr(void *ptr) const {
  uint8_t *offset_ptr = static_cast<uint8_t *>(ptr) - sizeof(Block_offset);

  DBUG_ASSERT(is_aligned(offset_ptr));

  return offset_ptr - *reinterpret_cast<Block_offset *>(offset_ptr);
}

template <class T>
inline typename Allocator<T>::Block_offset
Allocator<T>::block_decrement_used_chunks(uint8_t *block) {
  DBUG_ASSERT(*block_number_of_used_chunks_ptr(block) > 0);
  return --*block_number_of_used_chunks_ptr(block);
}

class End_thread {
 public:
  ~End_thread();
};

inline End_thread::~End_thread() { Allocator<uint8_t>::end_thread(); }

thread_local static End_thread end_thread;

} /* namespace temptable */

#endif /* TEMPTABLE_ALLOCATOR_H */
