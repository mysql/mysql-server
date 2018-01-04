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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_alloc.cc
  Routines to handle mallocing of results which will be freed the same time.
*/

#include <stdarg.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include "my_alloc.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_sys.h"
#include "mysql/psi/psi_memory.h"
#include "mysql/service_mysql_alloc.h"
#include "mysys_err.h"

static inline bool is_mem_available(MEM_ROOT *mem_root, size_t size);

/*
  For instrumented code: don't preallocate memory in alloc_root().
  This gives a lot more memory chunks, each with a red-zone around them.
 */
#if !defined(HAVE_VALGRIND) && !defined(HAVE_ASAN)
#define PREALLOCATE_MEMORY_CHUNKS
#endif


/*
  Initialize memory root

  SYNOPSIS
    init_alloc_root()
      mem_root       - memory root to initialize
      block_size     - size of chunks (blocks) used for memory allocation
                       (It is external size of chunk i.e. it should include
                        memory required for internal structures, thus it
                        should be no less than ALLOC_ROOT_MIN_BLOCK_SIZE)
      pre_alloc_size - if non-0, then size of block that should be
                       pre-allocated during memory root initialization.

  DESCRIPTION
    This function prepares memory root for further use, sets initial size of
    chunk for memory allocation and pre-allocates first block if specified.
    Altough error can happen during execution of this function if
    pre_alloc_size is non-0 it won't be reported. Instead it will be
    reported as error in first alloc_root() on this memory root.
*/

void init_alloc_root(PSI_memory_key key,
                     MEM_ROOT *mem_root, size_t block_size,
		     size_t pre_alloc_size MY_ATTRIBUTE((unused)))
{
  DBUG_ENTER("init_alloc_root");
  DBUG_PRINT("enter",("root: %p", mem_root));

  mem_root->free= mem_root->used= mem_root->pre_alloc= 0;
  mem_root->min_malloc= 32;
  mem_root->block_size= block_size - ALLOC_ROOT_MIN_BLOCK_SIZE;
  mem_root->error_handler= 0;
  mem_root->block_num= 4;			/* We shift this with >>2 */
  mem_root->first_block_usage= 0;
  mem_root->m_psi_key= key;
  mem_root->max_capacity= 0;
  mem_root->allocated_size= 0;
  mem_root->error_for_capacity_exceeded= FALSE;

#if defined(PREALLOCATE_MEMORY_CHUNKS)
  if (pre_alloc_size)
  {
    if ((mem_root->free= mem_root->pre_alloc=
	 (USED_MEM*) my_malloc(key,
                               pre_alloc_size+ ALIGN_SIZE(sizeof(USED_MEM)),
			       MYF(0))))
    {
      mem_root->free->size= (uint)(pre_alloc_size+ALIGN_SIZE(sizeof(USED_MEM)));
      mem_root->free->left= (uint)pre_alloc_size;
      mem_root->free->next= 0;
      mem_root->allocated_size+= pre_alloc_size+ ALIGN_SIZE(sizeof(USED_MEM));
    }
  }
#endif
  DBUG_VOID_RETURN;
}

/** This is a no-op unless the build is debug or for Valgrind. */
#define TRASH_MEM(X) TRASH(((char*)(X) + ((X)->size-(X)->left)), (X)->left)


/*
  SYNOPSIS
    reset_root_defaults()
    mem_root        memory root to change defaults of
    block_size      new value of block size. Must be greater or equal
                    than ALLOC_ROOT_MIN_BLOCK_SIZE (this value is about
                    68 bytes and depends on platform and compilation flags)
    pre_alloc_size  new size of preallocated block. If not zero,
                    must be equal to or greater than block size,
                    otherwise means 'no prealloc'.
  DESCRIPTION
    Function aligns and assigns new value to block size; then it tries to
    reuse one of existing blocks as prealloc block, or malloc new one of
    requested size. If no blocks can be reused, all unused blocks are freed
    before allocation.
*/

void reset_root_defaults(MEM_ROOT *mem_root, size_t block_size,
                         size_t pre_alloc_size MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(alloc_root_inited(mem_root));

  mem_root->block_size= block_size - ALLOC_ROOT_MIN_BLOCK_SIZE;
#if defined(PREALLOCATE_MEMORY_CHUNKS)
  if (pre_alloc_size)
  {
    size_t size= pre_alloc_size + ALIGN_SIZE(sizeof(USED_MEM));
    if (!mem_root->pre_alloc || mem_root->pre_alloc->size != size)
    {
      USED_MEM *mem, **prev= &mem_root->free;
      /*
        Free unused blocks, so that consequent calls
        to reset_root_defaults won't eat away memory.
      */
      while (*prev)
      {
        mem= *prev;
        if (mem->size == (uint)size)
        {
          /* We found a suitable block, no need to do anything else */
          mem_root->pre_alloc= mem;
          return;
        }
        if (mem->left + ALIGN_SIZE(sizeof(USED_MEM)) == mem->size)
        {
          /* remove block from the list and free it */
          *prev= mem->next;
          {
            mem->left= mem->size;
            mem_root->allocated_size-= mem->size;
            TRASH_MEM(mem);
            my_free(mem);
          }
        }
        else
          prev= &mem->next;
      }
      /* Allocate new prealloc block and add it to the end of free list */
      if (is_mem_available(mem_root, size) &&
          (mem= (USED_MEM *) my_malloc(mem_root->m_psi_key,
                                       size, MYF(0))))
      {
        mem->size= (uint)size;
        mem->left= (uint)pre_alloc_size;
        mem->next= *prev;
        *prev= mem_root->pre_alloc= mem;
        mem_root->allocated_size+= size;
      }
      else
      {
        mem_root->pre_alloc= 0;
      }
    }
  }
  else
#endif
    mem_root->pre_alloc= 0;
}


/**
  Function allocates the requested memory in the mem_root specified.
  If max_capacity is defined for the mem_root, it only allocates
  if the requested size can be allocated without exceeding the limit.
  However, when error_for_capacity_exceeded is set, an error is flagged
  (see set_error_reporting), but allocation is still performed.

  @param mem_root           memory root to allocate memory from
  @param length             size to be allocated

  @returns                  Pointer to the memory thats been allocated
  @retval
  NULL                      Memory is not available.
*/

void *alloc_root(MEM_ROOT *mem_root, size_t length)
{
#if !defined(PREALLOCATE_MEMORY_CHUNKS)
  USED_MEM *next;
  DBUG_ENTER("alloc_root");
  DBUG_PRINT("enter",("root: 0x%lx", (long) mem_root));

  DBUG_ASSERT(alloc_root_inited(mem_root));

  DBUG_EXECUTE_IF("simulate_out_of_memory",
                  {
                    if (mem_root->error_handler)
                      (*mem_root->error_handler)();
                    DBUG_SET("-d,simulate_out_of_memory");
                    DBUG_RETURN((void*) 0); /* purecov: inspected */
                  });

  length+=ALIGN_SIZE(sizeof(USED_MEM));
  if (!is_mem_available(mem_root, length))
  {
    if (mem_root->error_for_capacity_exceeded)
      my_error(EE_CAPACITY_EXCEEDED, MYF(0),
               (ulonglong) mem_root->max_capacity);
    else
      DBUG_RETURN(NULL);
  }
  if (!(next = (USED_MEM*) my_malloc(mem_root->m_psi_key,
                                     length,MYF(MY_WME | ME_FATALERROR))))
  {
    if (mem_root->error_handler)
      (*mem_root->error_handler)();
    DBUG_RETURN((uchar*) 0);			/* purecov: inspected */
  }
  mem_root->allocated_size+= length;
  next->next= mem_root->used;
  next->size= (uint)length;
  next->left= (uint)(length - ALIGN_SIZE(sizeof(USED_MEM)));
  mem_root->used= next;
  DBUG_PRINT("exit",("ptr: 0x%lx", (long) (((char*) next)+
                                           ALIGN_SIZE(sizeof(USED_MEM)))));
  DBUG_RETURN((uchar*) (((char*) next)+ALIGN_SIZE(sizeof(USED_MEM))));
#else
  size_t get_size, block_size;
  uchar* point;
  USED_MEM *next= 0;
  USED_MEM **prev;
  DBUG_ENTER("alloc_root");
  DBUG_PRINT("enter",("root: %p", mem_root));
  DBUG_ASSERT(alloc_root_inited(mem_root));

  DBUG_EXECUTE_IF("simulate_out_of_memory",
                  {
                    /* Avoid reusing an already allocated block */
                    if (mem_root->error_handler)
                      (*mem_root->error_handler)();
                    DBUG_SET("-d,simulate_out_of_memory");
                    DBUG_RETURN((void*) 0); /* purecov: inspected */
                  });
  length= ALIGN_SIZE(length);
  if ((*(prev= &mem_root->free)) != NULL)
  {
    if ((*prev)->left < length &&
	mem_root->first_block_usage++ >= ALLOC_MAX_BLOCK_USAGE_BEFORE_DROP &&
	(*prev)->left < ALLOC_MAX_BLOCK_TO_DROP)
    {
      next= *prev;
      *prev= next->next;			/* Remove block from list */
      next->next= mem_root->used;
      mem_root->used= next;
      mem_root->first_block_usage= 0;
    }
    for (next= *prev ; next && next->left < length ; next= next->next)
      prev= &next->next;
  }
  if (! next)
  {						/* Time to alloc new block */
    block_size= mem_root->block_size * (mem_root->block_num >> 2);
    get_size= length+ALIGN_SIZE(sizeof(USED_MEM));
    get_size= MY_MAX(get_size, block_size);

    if (!is_mem_available(mem_root, get_size))
    {
      if (mem_root->error_for_capacity_exceeded)
        my_error(EE_CAPACITY_EXCEEDED, MYF(0),
                 (ulonglong) mem_root->max_capacity);
      else
        DBUG_RETURN(NULL);
    }
    if (!(next = (USED_MEM*) my_malloc(mem_root->m_psi_key,
                                       get_size,MYF(MY_WME | ME_FATALERROR))))
    {
      if (mem_root->error_handler)
	(*mem_root->error_handler)();
      DBUG_RETURN((void*) 0);                      /* purecov: inspected */
    }
    mem_root->allocated_size+= get_size;
    mem_root->block_num++;
    next->next= *prev;
    next->size= (uint)get_size;
    next->left= (uint)(get_size-ALIGN_SIZE(sizeof(USED_MEM)));
    *prev=next;
  }

  point= (uchar*) ((char*) next+ (next->size-next->left));
  /*TODO: next part may be unneded due to mem_root->first_block_usage counter*/
  if ((next->left-= (uint)length) < mem_root->min_malloc)
  {						/* Full block */
    *prev= next->next;				/* Remove block from list */
    next->next= mem_root->used;
    mem_root->used= next;
    mem_root->first_block_usage= 0;
  }
  DBUG_PRINT("exit",("ptr: %p", point));
  DBUG_RETURN((void*) point);
#endif
}


/*
  Allocate many pointers at the same time.

  DESCRIPTION
    ptr1, ptr2, etc all point into big allocated memory area.

  SYNOPSIS
    multi_alloc_root()
      root               Memory root
      ptr1, length1      Multiple arguments terminated by a NULL pointer
      ptr2, length2      ...
      ...
      NULL

  RETURN VALUE
    A pointer to the beginning of the allocated memory block
    in case of success or NULL if out of memory.
*/

void *multi_alloc_root(MEM_ROOT *root, ...)
{
  va_list args;
  char **ptr, *start, *res;
  size_t tot_length, length;
  DBUG_ENTER("multi_alloc_root");

  va_start(args, root);
  tot_length= 0;
  while ((ptr= va_arg(args, char **)))
  {
    length= va_arg(args, uint);
    tot_length+= ALIGN_SIZE(length);
  }
  va_end(args);

  if (!(start= (char*) alloc_root(root, tot_length)))
    DBUG_RETURN(0);                            /* purecov: inspected */

  va_start(args, root);
  res= start;
  while ((ptr= va_arg(args, char **)))
  {
    *ptr= res;
    length= va_arg(args, uint);
    res+= ALIGN_SIZE(length);
  }
  va_end(args);
  DBUG_RETURN((void*) start);
}

/* Mark all data in blocks free for reusage */

static inline void mark_blocks_free(MEM_ROOT* root)
{
  USED_MEM *next;
  USED_MEM **last;

  /* iterate through (partially) free blocks, mark them free */
  last= &root->free;
  for (next= root->free; next; next= *(last= &next->next))
  {
    next->left= next->size - (uint)ALIGN_SIZE(sizeof(USED_MEM));
    TRASH_MEM(next);
  }

  /* Combine the free and the used list */
  *last= next=root->used;

  /* now go through the used blocks and mark them free */
  for (; next; next= next->next)
  {
    next->left= next->size - (uint)ALIGN_SIZE(sizeof(USED_MEM));
    TRASH_MEM(next);
  }

  /* Now everything is set; Indicate that nothing is used anymore */
  root->used= 0;
  root->first_block_usage= 0;
}

void claim_root(MEM_ROOT *root)
{
  USED_MEM *next,*old;
  DBUG_ENTER("claim_root");
  DBUG_PRINT("enter",("root: %p", root));

  for (next=root->used; next ;)
  {
    old=next; next= next->next ;
    my_claim(old);
  }

  for (next=root->free ; next ;)
  {
    old=next; next= next->next;
    my_claim(old);
  }

  DBUG_VOID_RETURN;
}


/*
  Deallocate everything used by alloc_root or just move
  used blocks to free list if called with MY_USED_TO_FREE

  SYNOPSIS
    free_root()
      root		Memory root
      MyFlags		Flags for what should be freed:

        MY_MARK_BLOCKS_FREED	Don't free blocks, just mark them free
        MY_KEEP_PREALLOC	If this is not set, then free also the
        		        preallocated block

  NOTES
    One can call this function either with root block initialised with
    init_alloc_root() or with a zero()-ed block.
    It's also safe to call this multiple times with the same mem_root.
*/

void free_root(MEM_ROOT *root, myf MyFlags)
{
  USED_MEM *next,*old;
  DBUG_ENTER("free_root");
  DBUG_PRINT("enter",("root: %p  flags: %u", root, (uint) MyFlags));

  if (MyFlags & MY_MARK_BLOCKS_FREE)
  {
    mark_blocks_free(root);
    DBUG_VOID_RETURN;
  }
  if (!(MyFlags & MY_KEEP_PREALLOC))
    root->pre_alloc=0;

  for (next=root->used; next ;)
  {
    old=next; next= next->next ;
    if (old != root->pre_alloc)
    {
      old->left= old->size;
      TRASH_MEM(old);
      my_free(old);
    }
  }
  for (next=root->free ; next ;)
  {
    old=next; next= next->next;
    if (old != root->pre_alloc)
    {
      old->left= old->size;
      TRASH_MEM(old);
      my_free(old);
    }
  }
  root->used=root->free=0;
  if (root->pre_alloc)
  {
    root->free=root->pre_alloc;
    root->free->left=root->pre_alloc->size-(uint)ALIGN_SIZE(sizeof(USED_MEM));
    root->allocated_size= root->pre_alloc->size;
    TRASH_MEM(root->pre_alloc);
    root->free->next=0;
  }
  else
    root->allocated_size= 0;
  root->block_num= 4;
  root->first_block_usage= 0;
  DBUG_VOID_RETURN;
}


extern "C" char *strdup_root(MEM_ROOT *root, const char *str)
{
  return strmake_root(root, str, strlen(str));
}


extern "C" char *safe_strdup_root(MEM_ROOT *root, const char *str)
{
  return str ? strdup_root(root, str) : 0;
}


char *strmake_root(MEM_ROOT *root, const char *str, size_t len)
{
  char *pos;
  if ((pos= static_cast<char*>(alloc_root(root,len+1))))
  {
    if (len > 0)
      memcpy(pos,str,len);
    pos[len]=0;
  }
  return pos;
}


void *memdup_root(MEM_ROOT *root, const void *str, size_t len)
{
  char *pos;
  if ((pos= (char*)alloc_root(root,len)))
    memcpy(pos,str,len);
  return pos;
}

/**
  Check if the set max_capacity is exceeded if the requested
  size is allocated

  @param mem_root           memory root to check for allocation
  @param size               requested size to be allocated

  @retval
  1 Memory is available
  @retval
  0 Memory is not available
*/
static inline bool is_mem_available(MEM_ROOT *mem_root, size_t size)
{
  if (mem_root->max_capacity)
  {
    if ((mem_root->allocated_size + size) > mem_root->max_capacity)
      return 0;
  }
  return 1;
}

/**
  set max_capacity for this mem_root. Should be called after init_alloc_root.
  If the max_capacity specified is less than what is already pre_alloced
  in init_alloc_root, only the future allocations are affected.

  @param mem_root         memory root to set the max capacity
  @param max_value        Maximum capacity this mem_root can hold
*/
void set_memroot_max_capacity(MEM_ROOT *mem_root, size_t max_value)
{
  DBUG_ASSERT(alloc_root_inited(mem_root));
  mem_root->max_capacity= max_value;
}

/**
  Enable/disable error reporting for exceeding max_capacity. If error
  reporting is enabled, an error is flagged to indicate that the capacity
  is exceeded. However allocation will still happen for the requested memory.

  @param mem_root        memory root
  @param report_error    set to true if error should be reported
                         else set to false
*/
void set_memroot_error_reporting(MEM_ROOT *mem_root, bool report_error)
{
  DBUG_ASSERT(alloc_root_inited(mem_root));
  mem_root->error_for_capacity_exceeded= report_error;
}

