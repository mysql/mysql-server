/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file include/my_alloc.h
  Data structures for mysys/my_alloc.c (root memory allocator).
*/

#ifndef _my_alloc_h
#define _my_alloc_h

#include <stdbool.h>

/*
  How much overhead does malloc have. The code often allocates
  something like 1024-MALLOC_OVERHEAD bytes
*/
#define MALLOC_OVERHEAD 8

/* Typical record cache */
#define RECORD_CACHE_SIZE      (uint) (64*1024-MALLOC_OVERHEAD)

#define ALLOC_MAX_BLOCK_TO_DROP			4096
#define ALLOC_MAX_BLOCK_USAGE_BEFORE_DROP	10

#include <string.h>
#include <sys/types.h>

#include "my_compiler.h"
#include "my_inttypes.h"
#include "mysql/psi/psi_memory.h"

#include "mem_root_fwd.h"  // Contains the typedef to MEM_ROOT. IWYU pragma: keep

#ifdef __cplusplus

#include <memory>
#include <new>

extern "C" {

extern void free_root(MEM_ROOT *root, myf MyFLAGS);
extern void *alloc_root(MEM_ROOT *mem_root, size_t Size);
extern void init_alloc_root(PSI_memory_key key,
                            MEM_ROOT *mem_root, size_t block_size,
                            size_t pre_alloc_size);

}

#endif

typedef struct st_used_mem
{				   /* struct for once_alloc (block) */
  struct st_used_mem *next;	   /* Next block in use */
  unsigned int	left;		   /* memory left in block  */
  unsigned int	size;		   /* size of block */
} USED_MEM;


struct st_mem_root
{
#ifdef __cplusplus
  st_mem_root() :
    free(nullptr), used(nullptr), pre_alloc(nullptr),
    min_malloc(0) // for alloc_root_inited()
  {}

  st_mem_root(PSI_memory_key key, size_t block_size, size_t pre_alloc_size)
  {
    init_alloc_root(key, this, block_size, pre_alloc_size);
  }

  // Make the class movable but not copyable.
  st_mem_root(const st_mem_root &) = delete;
  st_mem_root(st_mem_root &&other) noexcept {
    memcpy(this, &other, sizeof(*this));
    other.free= other.used= other.pre_alloc= nullptr;
    other.min_malloc= 0;
  }

  st_mem_root& operator= (const st_mem_root &) = delete;
  st_mem_root& operator= (st_mem_root &&other) noexcept {
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
  ~st_mem_root()
  {
    free_root(this, MYF(0));
  }

#endif

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

#ifdef __cplusplus
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

template <typename T, typename... Args>
unique_ptr_destroy_only<T>
make_unique_destroy_only(MEM_ROOT *mem_root, Args&&... args)
{
  return
    unique_ptr_destroy_only<T>(new (mem_root) T(std::forward<Args>(args)...));
}

#endif  // __cplusplus

#endif
