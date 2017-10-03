/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file include/my_alloc.h
  Data structures for mysys/my_alloc.c (root memory allocator).
*/

#ifndef _my_alloc_h
#define _my_alloc_h

/*
  How much overhead does malloc have. The code often allocates
  something like 1024-MALLOC_OVERHEAD bytes
*/
#define MALLOC_OVERHEAD 8

/* Typical record cache */
#define RECORD_CACHE_SIZE      (uint) (64*1024-MALLOC_OVERHEAD)

#define ALLOC_MAX_BLOCK_TO_DROP			4096
#define ALLOC_MAX_BLOCK_USAGE_BEFORE_DROP	10

#include <stdbool.h>
#include <string.h>
#include <sys/types.h>

#include <memory>
#include <new>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "mysql/psi/psi_memory.h"

struct MEM_ROOT;

extern "C" {

extern void free_root(MEM_ROOT *root, myf MyFLAGS);
extern void *alloc_root(MEM_ROOT *mem_root, size_t Size) MY_ATTRIBUTE((malloc));
extern void init_alloc_root(PSI_memory_key key,
                            MEM_ROOT *mem_root, size_t block_size,
                            size_t pre_alloc_size);

}

struct USED_MEM
{				   /* struct for once_alloc (block) */
  USED_MEM *next;	   /* Next block in use */
  unsigned int	left;		   /* memory left in block  */
  unsigned int	size;		   /* size of block */
};


struct MEM_ROOT
{
  MEM_ROOT() : MEM_ROOT(0, 512, 0) {}  // 0 = PSI_NOT_INSTRUMENTED

  MEM_ROOT(PSI_memory_key key, size_t block_size, size_t pre_alloc_size)
  {
    init_alloc_root(key, this, block_size, pre_alloc_size);
  }

  // Make the class movable but not copyable.
  MEM_ROOT(const MEM_ROOT &) = delete;
  MEM_ROOT(MEM_ROOT &&other) noexcept {
    memcpy(this, &other, sizeof(*this));
    other.free= other.used= other.pre_alloc= nullptr;
    other.min_malloc= 0;
  }

  MEM_ROOT& operator= (const MEM_ROOT &) = delete;
  MEM_ROOT& operator= (MEM_ROOT &&other) noexcept {
    memcpy(this, &other, sizeof(*this));
    other.free= other.used= other.pre_alloc= nullptr;
    other.min_malloc= 0;
    return *this;
  }

  /**
    From C code where this destructor doesn't exist, use free_root() manually.
    It's harmless to call free_root() multiple times, and thus also to call
    free_root() on a to-be-destructed object.
  */
  ~MEM_ROOT()
  {
    free_root(this, MYF(0));
  }

  USED_MEM *free;                  /* blocks with free memory in it */
  USED_MEM *used;                  /* blocks almost without free memory */
  USED_MEM *pre_alloc;             /* preallocated block */
  /* if block have less memory it will be put in 'used' list */
  size_t min_malloc;
  size_t block_size;               /* initial block size */
  unsigned int block_num;          /* allocated blocks counter */
  /* 
     first free block in queue test counter (if it exceed 
     MAX_BLOCK_USAGE_BEFORE_DROP block will be dropped in 'used' list)
  */
  unsigned int first_block_usage;

  /*
    Maximum amount of memory this mem_root can hold. A value of 0
    implies there is no limit.
  */
  size_t max_capacity;

  /* Allocated size for this mem_root */

  size_t allocated_size;

  /* Enable this for error reporting if capacity is exceeded */
  bool error_for_capacity_exceeded;

  void (*error_handler)(void);

  PSI_memory_key m_psi_key;
};

/**
  Allocate an object of the given type. Use like this:

    Foo *foo = new (mem_root) Foo();

  Note that unlike regular operator new, this will not throw exceptions.
  However, it can return nullptr if the capacity of the MEM_ROOT has been
  reached. This is allowed since it is not a replacement for global operator
  new, and thus isn't used automatically by e.g. standard library containers.

  TODO: This syntax is confusing in that it could look like allocating
  a MEM_ROOT using regular placement new. We should make a less ambiguous
  syntax, e.g. new (On(mem_root)) Foo().
*/
inline void *operator new(size_t size, MEM_ROOT *mem_root,
                   const std::nothrow_t &arg MY_ATTRIBUTE((unused))
                   = std::nothrow) noexcept
{
  return alloc_root(mem_root, size);
}

inline void *operator new[](size_t size, MEM_ROOT *mem_root,
                     const std::nothrow_t &arg MY_ATTRIBUTE((unused))
                     = std::nothrow) noexcept
{
  return alloc_root(mem_root, size);
}

inline void operator delete(void*, MEM_ROOT*,
                     const std::nothrow_t&) noexcept
{
  /* never called */
}

inline void operator delete[](void*, MEM_ROOT*,
                       const std::nothrow_t&) noexcept
{
  /* never called */
}

template<class T>
inline void destroy(T *ptr) { if (ptr != nullptr) ptr->~T(); }

template<class T>
inline void destroy_array(T *ptr, size_t count)
{
  if (ptr != nullptr)
    for (size_t i= 0; i < count; ++i)
      destroy(&ptr[i]);
}

/*
  For std::unique_ptr with objects allocated on a MEM_ROOT, you shouldn't use
  Default_deleter; use this deleter instead.
*/
template<class T>
class Destroy_only
{
public:
  void operator() (T *ptr) const { destroy(ptr); }
};

/** std::unique_ptr, but only destroying. */
template<class T>
using unique_ptr_destroy_only = std::unique_ptr<T, Destroy_only<T>>;

#endif
