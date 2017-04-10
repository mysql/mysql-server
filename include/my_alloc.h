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

#include "my_inttypes.h"
#include "mysql/psi/psi_memory.h"

#include "mem_root_fwd.h"  // Contains the typedef to MEM_ROOT. IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif

typedef struct st_used_mem
{				   /* struct for once_alloc (block) */
  struct st_used_mem *next;	   /* Next block in use */
  unsigned int	left;		   /* memory left in block  */
  unsigned int	size;		   /* size of block */
} USED_MEM;


struct st_mem_root
{
#if defined(__cplusplus) && (__cplusplus >= 201103L || defined(_MSC_VER))
  // Make the class movable but not copyable.
  st_mem_root() :
  min_malloc(0) // for alloc_root_inited()
  {}
  st_mem_root(const st_mem_root &) = delete;
  st_mem_root(st_mem_root &&other) {
    memcpy(this, &other, sizeof(*this));
    other.free= other.used= other.pre_alloc= nullptr;
    other.min_malloc= 0;
  }

  st_mem_root& operator= (const st_mem_root &) = delete;
  st_mem_root& operator= (st_mem_root &&other) {
    memcpy(this, &other, sizeof(*this));
    other.free= other.used= other.pre_alloc= nullptr;
    other.min_malloc= 0;
    return *this;
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

#ifdef  __cplusplus
}
#endif

#endif
