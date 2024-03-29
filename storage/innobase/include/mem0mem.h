/*****************************************************************************

Copyright (c) 1994, 2023, Oracle and/or its affiliates.

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

/** @file include/mem0mem.h
 The memory management

 Created 6/9/1994 Heikki Tuuri
 *******************************************************/

#ifndef mem0mem_h
#define mem0mem_h

#include "mach0data.h"
#include "ut0byte.h"
#include "ut0mem.h"
#include "ut0rnd.h"

#include <limits.h>

#include <functional>
#include <memory>
#include <type_traits>

/* -------------------- MEMORY HEAPS ----------------------------- */

/** A block of a memory heap consists of the info structure
followed by an area of memory */
typedef struct mem_block_info_t mem_block_t;

/** A memory heap is a nonempty linear list of memory blocks */
typedef mem_block_t mem_heap_t;

/** Types of allocation for memory heaps: DYNAMIC means allocation from the
dynamic memory pool of the C compiler, BUFFER means allocation from the
buffer pool; the latter method is used for very big heaps */

/** the most common type */
constexpr uint32_t MEM_HEAP_DYNAMIC = 0;
constexpr uint32_t MEM_HEAP_BUFFER = 1;
/** this flag can optionally be ORed to MEM_HEAP_BUFFER, in which case
 heap->free_block is used in some cases for memory allocations, and if it's
 NULL, the memory allocation functions can return NULL. */
constexpr uint32_t MEM_HEAP_BTR_SEARCH = 2;

/** Different type of heaps in terms of which data structure is using them */
constexpr uint32_t MEM_HEAP_FOR_BTR_SEARCH =
    MEM_HEAP_BTR_SEARCH | MEM_HEAP_BUFFER;
constexpr uint32_t MEM_HEAP_FOR_PAGE_HASH = MEM_HEAP_DYNAMIC;
constexpr uint32_t MEM_HEAP_FOR_RECV_SYS = MEM_HEAP_BUFFER;
constexpr uint32_t MEM_HEAP_FOR_LOCK_HEAP = MEM_HEAP_BUFFER;

/** The following start size is used for the first block in the memory heap if
the size is not specified, i.e., 0 is given as the parameter in the call of
create. The standard size is the maximum (payload) size of the blocks used for
allocations of small buffers. */
constexpr uint32_t MEM_BLOCK_START_SIZE = 64;

#define MEM_BLOCK_STANDARD_SIZE \
  (UNIV_PAGE_SIZE >= 16384 ? 8000 : MEM_MAX_ALLOC_IN_BUF)

/** If a memory heap is allowed to grow into the buffer pool, the following
is the maximum size for a single allocated buffer
(from UNIV_PAGE_SIZE we subtract MEM_BLOCK_HEADER_SIZE and 2*MEM_NO_MANS_LAND
since it's something we always need to put. Since in MEM_SPACE_NEEDED we round
n to the next multiple of UNIV_MEM_ALINGMENT, we need to cut from the rest the
part that cannot be divided by UNIV_MEM_ALINGMENT): */
#define MEM_MAX_ALLOC_IN_BUF                                         \
  ((UNIV_PAGE_SIZE - MEM_BLOCK_HEADER_SIZE - 2 * MEM_NO_MANS_LAND) & \
   ~(UNIV_MEM_ALIGNMENT - 1))

/* Before and after any allocated object we will put MEM_NO_MANS_LAND bytes of
some data (different before and after) which is supposed not to be modified by
anyone. This way it would be much easier to determine whether anyone was
writing on not his memory, especially that Valgrind can assure there was no
reads or writes to this memory. */
#ifdef UNIV_DEBUG
constexpr int MEM_NO_MANS_LAND = 16;
#else
constexpr int MEM_NO_MANS_LAND = 0;
#endif

/* Byte that we would put before allocated object MEM_NO_MANS_LAND times.*/
const byte MEM_NO_MANS_LAND_BEFORE_BYTE = 0xCE;
/* Byte that we would put after allocated object MEM_NO_MANS_LAND times.*/
const byte MEM_NO_MANS_LAND_AFTER_BYTE = 0xDF;

/** Space needed when allocating for a user a field of length N.
The space is allocated only in multiples of UNIV_MEM_ALIGNMENT. In debug mode
contains two areas of no mans lands before and after the buffer requested. */
static inline uint64_t MEM_SPACE_NEEDED(uint64_t N) {
  return ut_calc_align(N + 2 * MEM_NO_MANS_LAND, UNIV_MEM_ALIGNMENT);
}

/** Creates a memory heap.
A single user buffer of 'size' will fit in the block.
0 creates a default size block.
@param[in]      size            Desired start block size.
@param[in]      loc             Location where created
@param[in]      type            Heap type
@return own: memory heap, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
static inline mem_heap_t *mem_heap_create(ulint size, ut::Location loc,
                                          ulint type = MEM_HEAP_DYNAMIC);

/** Frees the space occupied by a memory heap.
@param[in]      heap    Heap to be freed */
static inline void mem_heap_free(mem_heap_t *heap);

/** Allocates and zero-fills n bytes of memory from a memory heap.
@param[in]      heap    memory heap
@param[in]      n       number of bytes; if the heap is allowed to grow into
the buffer pool, this must be <= MEM_MAX_ALLOC_IN_BUF
@return allocated, zero-filled storage */
static inline void *mem_heap_zalloc(mem_heap_t *heap, ulint n);

/** Allocates n bytes of memory from a memory heap.
@param[in]      heap    memory heap
@param[in]      n       number of bytes; if the heap is allowed to grow into
the buffer pool, this must be <= MEM_MAX_ALLOC_IN_BUF
@return allocated storage, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
static inline void *mem_heap_alloc(mem_heap_t *heap, ulint n);

/** Returns a pointer to the heap top.
@param[in]      heap            memory heap
@return pointer to the heap top */
static inline byte *mem_heap_get_heap_top(mem_heap_t *heap);

/** Frees the space in a memory heap exceeding the pointer given.
The pointer must have been acquired from mem_heap_get_heap_top.
The first memory block of the heap is not freed.
@param[in]      heap            heap from which to free
@param[in]      old_top         pointer to old top of heap */
static inline void mem_heap_free_heap_top(mem_heap_t *heap, byte *old_top);

/** Empties a memory heap.
The first memory block of the heap is not freed.
@param[in]      heap            heap to empty */
static inline void mem_heap_empty(mem_heap_t *heap);

/** Returns a pointer to the topmost element in a memory heap.
The size of the element must be given.
@param[in]      heap    memory heap
@param[in]      n       size of the topmost element
@return pointer to the topmost element */
static inline void *mem_heap_get_top(mem_heap_t *heap, ulint n);

/** Checks if a given chunk of memory is the topmost element stored in the
heap. If this is the case, then calling mem_heap_free_top() would free
that element from the heap.
@param[in]      heap    memory heap
@param[in]      buf     presumed topmost element
@param[in]      buf_sz  size of buf in bytes
@return true if topmost */
[[nodiscard]] static inline bool mem_heap_is_top(mem_heap_t *heap,
                                                 const void *buf, ulint buf_sz);

/** Allocate a new chunk of memory from a memory heap, possibly discarding the
topmost element. If the memory chunk specified with (top, top_sz) is the
topmost element, then it will be discarded, otherwise it will be left untouched
and this function will be equivallent to mem_heap_alloc().
@param[in,out]  heap    memory heap
@param[in]      top     chunk to discard if possible
@param[in]      top_sz  size of top in bytes
@param[in]      new_sz  desired size of the new chunk
@return allocated storage, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
static inline void *mem_heap_replace(mem_heap_t *heap, const void *top,
                                     ulint top_sz, ulint new_sz);

/** Allocate a new chunk of memory from a memory heap, possibly discarding the
topmost element and then copy the specified data to it. If the memory chunk
specified with (top, top_sz) is the topmost element, then it will be discarded,
otherwise it will be left untouched and this function will be equivalent to
mem_heap_dup().
@param[in,out]  heap    memory heap
@param[in]      top     chunk to discard if possible
@param[in]      top_sz  size of top in bytes
@param[in]      data    new data to duplicate
@param[in]      data_sz size of data in bytes
@return allocated storage, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
static inline void *mem_heap_dup_replace(mem_heap_t *heap, const void *top,
                                         ulint top_sz, const void *data,
                                         ulint data_sz);

/** Allocate a new chunk of memory from a memory heap, possibly discarding the
topmost element and then copy the specified string to it. If the memory chunk
specified with (top, top_sz) is the topmost element, then it will be discarded,
otherwise it will be left untouched and this function will be equivalent to
mem_heap_strdup().
@param[in,out]  heap    memory heap
@param[in]      top     chunk to discard if possible
@param[in]      top_sz  size of top in bytes
@param[in]      str     new data to duplicate
@return allocated string, NULL if did not succeed (only possible for
MEM_HEAP_BTR_SEARCH type heaps) */
static inline char *mem_heap_strdup_replace(mem_heap_t *heap, const void *top,
                                            ulint top_sz, const char *str);

/** Frees the topmost element in a memory heap.
@param[in]      heap    memory heap
@param[in]      n       size of the topmost element
The size of the element must be given. */
static inline void mem_heap_free_top(mem_heap_t *heap, ulint n);

/** Returns the space in bytes occupied by a memory heap. */
static inline size_t mem_heap_get_size(mem_heap_t *heap); /*!< in: heap */

/** Duplicates a NUL-terminated string.
@param[in]      str     string to be copied
@return own: a copy of the string, must be deallocated with ut_free */
static inline char *mem_strdup(const char *str);

/** Makes a NUL-terminated copy of a nonterminated string.
@param[in]      str     string to be copied
@param[in]      len     length of str, in bytes
@return own: a copy of the string, must be deallocated with ut_free */
static inline char *mem_strdupl(const char *str, ulint len);

/** Duplicates a NUL-terminated string, allocated from a memory heap.
@param[in]      heap    memory heap where string is allocated
@param[in]      str     string to be copied
@return own: a copy of the string */
char *mem_heap_strdup(mem_heap_t *heap, const char *str);

/** Makes a NUL-terminated copy of a nonterminated string, allocated from a
memory heap.
@param[in]      heap    memory heap where string is allocated
@param[in]      str     string to be copied
@param[in]      len     length of str, in bytes
@return own: a copy of the string */
static inline char *mem_heap_strdupl(mem_heap_t *heap, const char *str,
                                     ulint len);

/** Concatenate two strings and return the result, using a memory heap.
 @return own: the result */
char *mem_heap_strcat(
    mem_heap_t *heap, /*!< in: memory heap where string is allocated */
    const char *s1,   /*!< in: string 1 */
    const char *s2);  /*!< in: string 2 */

/** Duplicate a block of data, allocated from a memory heap.
 @return own: a copy of the data */
void *mem_heap_dup(
    mem_heap_t *heap, /*!< in: memory heap where copy is allocated */
    const void *data, /*!< in: data to be copied */
    ulint len);       /*!< in: length of data, in bytes */

/** A simple sprintf replacement that dynamically allocates the space for the
 formatted string from the given heap. This supports a very limited set of
 the printf syntax: types 's' and 'u' and length modifier 'l' (which is
 required for the 'u' type).
 @return heap-allocated formatted string */
char *mem_heap_printf(mem_heap_t *heap,   /*!< in: memory heap */
                      const char *format, /*!< in: format string */
                      ...) MY_ATTRIBUTE((format(printf, 2, 3)));

/** Checks that an object is a memory heap (or a block of it)
@param[in]      heap    Memory heap to check */
static inline void mem_block_validate(const mem_heap_t *heap);

#ifdef UNIV_DEBUG

/** Validates the contents of a memory heap.
Checks a memory heap for consistency, prints the contents if any error
is detected. A fatal error is logged if an error is detected.
@param[in]      heap    Memory heap to validate. */
void mem_heap_validate(const mem_heap_t *heap);

#endif /* UNIV_DEBUG */

/*#######################################################################*/

struct buf_block_t;

/** The info structure stored at the beginning of a heap block */
struct mem_block_info_t {
  /** Magic number for debugging. */
  uint64_t magic_n;
#ifdef UNIV_DEBUG
  /** File name where the mem heap was created. */
  char file_name[16];
  /** Line number where the mem heap was created. */
  ulint line;
#endif /* UNIV_DEBUG */
  /** This contains pointers to next and prev in the list. The first block
  allocated to the heap is also the first block in this list,
  though it also contains the base node of the list. */
  UT_LIST_NODE_T(mem_block_t) list;
  /** In the first block of the list this is the base node of the list of
  blocks; in subsequent blocks this is undefined. */
  UT_LIST_BASE_NODE_T_EXTERN(mem_block_t, list) base;
  /** Physical length of this block in bytes. */
  ulint len;
  /** Physical length in bytes of all blocks in the heap. This is defined only
  in the base node and is set to ULINT_UNDEFINED in others. */
  ulint total_size;
  /** Type of heap: MEM_HEAP_DYNAMIC, or MEM_HEAP_BUF possibly ORed to
  MEM_HEAP_BTR_SEARCH. */
  ulint type;
  /** Offset in bytes of the first free position for user data in the block. */
  ulint free;
  /** The value of the struct field 'free' at the creation of the block. */
  ulint start;
  /* This is not null iff the MEM_HEAP_BTR_SEARCH bit is set in type, and this
  is the heap root. This leads to a atomic pointer that can contain an allocated
  buffer frame, which can be appended as a free block to the heap, if we need
  more space. */
  std::atomic<buf_block_t *> *free_block_ptr;

  /* if this block has been allocated from the buffer pool, this contains the
  buf_block_t handle; otherwise, this is NULL */
  buf_block_t *buf_block;
};
/* We use the UT_LIST_BASE_NODE_T_EXTERN instead of simpler UT_LIST_BASE_NODE_T
because DevStudio12.6 initializes the pointer-to-member offset to 0 otherwise.*/
UT_LIST_NODE_GETTER_DEFINITION(mem_block_t, list)

constexpr uint64_t MEM_BLOCK_MAGIC_N = 0x445566778899AABB;
constexpr uint64_t MEM_FREED_BLOCK_MAGIC_N = 0xBBAA998877665544;

/* Header size for a memory heap block */
#define MEM_BLOCK_HEADER_SIZE \
  ut_calc_align(sizeof(mem_block_info_t), UNIV_MEM_ALIGNMENT)

#include "mem0mem.ic"

/** A C++ wrapper class to the mem_heap_t routines, so that it can be used
as an STL allocator */
template <typename T>
class mem_heap_allocator {
 public:
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T *pointer;
  typedef const T *const_pointer;
  typedef T &reference;
  typedef const T &const_reference;

  mem_heap_allocator(mem_heap_t *heap) : m_heap(heap) {}

  mem_heap_allocator(const mem_heap_allocator &other) : m_heap(other.m_heap) {
    // Do nothing
  }

  template <typename U>
  mem_heap_allocator(const mem_heap_allocator<U> &other)
      : m_heap(other.m_heap) {
    // Do nothing
  }

  ~mem_heap_allocator() { m_heap = nullptr; }

  size_type max_size() const { return (ULONG_MAX / sizeof(T)); }

  /** This function returns a pointer to the first element of a newly
  allocated array large enough to contain n objects of type T; only the
  memory is allocated, and the objects are not constructed. Moreover,
  an optional pointer argument (that points to an object already
  allocated by mem_heap_allocator) can be used as a hint to the
  implementation about where the new memory should be allocated in
  order to improve locality. */
  pointer allocate(size_type n, const_pointer hint [[maybe_unused]] = nullptr) {
    return (reinterpret_cast<pointer>(mem_heap_alloc(m_heap, n * sizeof(T))));
  }

  void deallocate(pointer, size_type) {}

  pointer address(reference r) const { return (&r); }

  const_pointer address(const_reference r) const { return (&r); }

  void construct(pointer p, const_reference t) {
    new (reinterpret_cast<void *>(p)) T(t);
  }

  void destroy(pointer p) { (reinterpret_cast<T *>(p))->~T(); }

  /** Allocators are required to supply the below template class member
  which enables the possibility of obtaining a related allocator,
  parametrized in terms of a different type. For example, given an
  allocator type IntAllocator for objects of type int, a related
  allocator type for objects of type long could be obtained using
  IntAllocator::rebind<long>::other */
  template <typename U>
  struct rebind {
    typedef mem_heap_allocator<U> other;
  };

  /** Get the underlying memory heap object.
  @return the underlying memory heap object. */
  mem_heap_t *get_mem_heap() const { return (m_heap); }

 private:
  mem_heap_t *m_heap;
  template <typename U>
  friend class mem_heap_allocator;
};

template <class T>
bool operator==(const mem_heap_allocator<T> &left,
                const mem_heap_allocator<T> &right) {
  return (left.get_mem_heap() == right.get_mem_heap());
}

template <class T>
bool operator!=(const mem_heap_allocator<T> &left,
                const mem_heap_allocator<T> &right) {
  return (left.get_mem_heap() != right.get_mem_heap());
}

/** Heap wrapper that destroys the heap instance when it goes out of scope. */
struct Scoped_heap {
  using Type = mem_heap_t;

  /** Default constructor. */
  Scoped_heap() : m_ptr(nullptr) {}

  /** Constructs heap with a free space of specified size.
  @param[in] n                  Initial size of the heap to allocate.
  @param[in] location           Location from where called. */
  Scoped_heap(size_t n, ut::Location location) noexcept
      : m_ptr(mem_heap_create(n, location, MEM_HEAP_DYNAMIC)) {}

  /** Destructor. */
  ~Scoped_heap() = default;

  /** Create the heap, it must not already be created.
  @param[in] n                  Initial size of the heap to allocate.
  @param[in] location           Location from where called. */
  void create(size_t n, ut::Location location) noexcept {
    ut_a(get() == nullptr);
    auto ptr = mem_heap_create(n, location, MEM_HEAP_DYNAMIC);
    reset(ptr);
  }

  /** @return the heap pointer. */
  Type *get() noexcept { return m_ptr.get(); }

  /** Set the pointer to p.
  @param[in] p                  New pointer value. */
  void reset(Type *p) {
    if (get() != p) {
      m_ptr.reset(p);
    }
  }

  /** Empty the heap. */
  void clear() noexcept {
    if (get() != nullptr) {
      mem_heap_empty(get());
    }
  }

  /** Allocate memory in the heap.
  @param[in] n_bytes            Number of bytes to allocate.
  @return pointer to allocated memory. */
  void *alloc(size_t n_bytes) noexcept {
    ut_ad(n_bytes > 0);
    return mem_heap_alloc(get(), n_bytes);
  }

 private:
  /** A functor with no state to be used for mem_heap destruction. */
  struct mem_heap_free_functor {
    void operator()(mem_heap_t *heap) { mem_heap_free(heap); }
  };

  using Ptr = std::unique_ptr<Type, mem_heap_free_functor>;

  /** Heap to use. */
  Ptr m_ptr{};

  Scoped_heap(Scoped_heap &&) = delete;
  Scoped_heap(const Scoped_heap &) = delete;
  Scoped_heap &operator=(Scoped_heap &&) = delete;
  Scoped_heap &operator=(const Scoped_heap &) = delete;
};

#endif
