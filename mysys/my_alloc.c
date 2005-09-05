/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Routines to handle mallocing of results which will be freed the same time */

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#undef EXTRA_DEBUG
#define EXTRA_DEBUG


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

void init_alloc_root(MEM_ROOT *mem_root, uint block_size,
		     uint pre_alloc_size __attribute__((unused)))
{
  DBUG_ENTER("init_alloc_root");
  DBUG_PRINT("enter",("root: 0x%lx", mem_root));
  mem_root->free= mem_root->used= mem_root->pre_alloc= 0;
  mem_root->min_malloc= 32;
  mem_root->block_size= block_size - ALLOC_ROOT_MIN_BLOCK_SIZE;
  mem_root->error_handler= 0;
  mem_root->block_num= 4;			/* We shift this with >>2 */
  mem_root->first_block_usage= 0;

#if !(defined(HAVE_purify) && defined(EXTRA_DEBUG))
  if (pre_alloc_size)
  {
    if ((mem_root->free= mem_root->pre_alloc=
	 (USED_MEM*) my_malloc(pre_alloc_size+ ALIGN_SIZE(sizeof(USED_MEM)),
			       MYF(0))))
    {
      mem_root->free->size= pre_alloc_size+ALIGN_SIZE(sizeof(USED_MEM));
      mem_root->free->left= pre_alloc_size;
      mem_root->free->next= 0;
    }
  }
#endif
  DBUG_VOID_RETURN;
}


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

void reset_root_defaults(MEM_ROOT *mem_root, uint block_size,
                         uint pre_alloc_size __attribute__((unused)))
{
  DBUG_ASSERT(alloc_root_inited(mem_root));

  mem_root->block_size= block_size - ALLOC_ROOT_MIN_BLOCK_SIZE;
#if !(defined(HAVE_purify) && defined(EXTRA_DEBUG))
  if (pre_alloc_size)
  {
    uint size= pre_alloc_size + ALIGN_SIZE(sizeof(USED_MEM));
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
        if (mem->size == size)
        {
          /* We found a suitable block, no need to do anything else */
          mem_root->pre_alloc= mem;
          return;
        }
        if (mem->left + ALIGN_SIZE(sizeof(USED_MEM)) == mem->size)
        {
          /* remove block from the list and free it */
          *prev= mem->next;
          my_free((gptr) mem, MYF(0));
        }
        else
          prev= &mem->next;
      }
      /* Allocate new prealloc block and add it to the end of free list */
      if ((mem= (USED_MEM *) my_malloc(size, MYF(0))))
      {
        mem->size= size; 
        mem->left= pre_alloc_size;
        mem->next= *prev;
        *prev= mem_root->pre_alloc= mem; 
      }
    }
  }
  else
#endif
    mem_root->pre_alloc= 0;
}


gptr alloc_root(MEM_ROOT *mem_root,unsigned int Size)
{
#if defined(HAVE_purify) && defined(EXTRA_DEBUG)
  reg1 USED_MEM *next;
  DBUG_ENTER("alloc_root");
  DBUG_PRINT("enter",("root: 0x%lx", mem_root));

  DBUG_ASSERT(alloc_root_inited(mem_root));

  Size+=ALIGN_SIZE(sizeof(USED_MEM));
  if (!(next = (USED_MEM*) my_malloc(Size,MYF(MY_WME))))
  {
    if (mem_root->error_handler)
      (*mem_root->error_handler)();
    DBUG_RETURN((gptr) 0);			/* purecov: inspected */
  }
  next->next= mem_root->used;
  next->size= Size;
  mem_root->used= next;
  DBUG_PRINT("exit",("ptr: 0x%lx", (((char*) next)+
                                    ALIGN_SIZE(sizeof(USED_MEM)))));
  DBUG_RETURN((gptr) (((char*) next)+ALIGN_SIZE(sizeof(USED_MEM))));
#else
  uint get_size, block_size;
  gptr point;
  reg1 USED_MEM *next= 0;
  reg2 USED_MEM **prev;
  DBUG_ENTER("alloc_root");
  DBUG_PRINT("enter",("root: 0x%lx", mem_root));
  DBUG_ASSERT(alloc_root_inited(mem_root));

  Size= ALIGN_SIZE(Size);
  if ((*(prev= &mem_root->free)) != NULL)
  {
    if ((*prev)->left < Size &&
	mem_root->first_block_usage++ >= ALLOC_MAX_BLOCK_USAGE_BEFORE_DROP &&
	(*prev)->left < ALLOC_MAX_BLOCK_TO_DROP)
    {
      next= *prev;
      *prev= next->next;			/* Remove block from list */
      next->next= mem_root->used;
      mem_root->used= next;
      mem_root->first_block_usage= 0;
    }
    for (next= *prev ; next && next->left < Size ; next= next->next)
      prev= &next->next;
  }
  if (! next)
  {						/* Time to alloc new block */
    block_size= mem_root->block_size * (mem_root->block_num >> 2);
    get_size= Size+ALIGN_SIZE(sizeof(USED_MEM));
    get_size= max(get_size, block_size);

    if (!(next = (USED_MEM*) my_malloc(get_size,MYF(MY_WME))))
    {
      if (mem_root->error_handler)
	(*mem_root->error_handler)();
      return((gptr) 0);				/* purecov: inspected */
    }
    mem_root->block_num++;
    next->next= *prev;
    next->size= get_size;
    next->left= get_size-ALIGN_SIZE(sizeof(USED_MEM));
    *prev=next;
  }

  point= (gptr) ((char*) next+ (next->size-next->left));
  /*TODO: next part may be unneded due to mem_root->first_block_usage counter*/
  if ((next->left-= Size) < mem_root->min_malloc)
  {						/* Full block */
    *prev= next->next;				/* Remove block from list */
    next->next= mem_root->used;
    mem_root->used= next;
    mem_root->first_block_usage= 0;
  }
  DBUG_PRINT("exit",("ptr: 0x%lx", (ulong) point));
  DBUG_RETURN(point);
#endif
}

#define TRASH_MEM(X) TRASH(((char*)(X) + ((X)->size-(X)->left)), (X)->left)

/* Mark all data in blocks free for reusage */

static inline void mark_blocks_free(MEM_ROOT* root)
{
  reg1 USED_MEM *next;
  reg2 USED_MEM **last;

  /* iterate through (partially) free blocks, mark them free */
  last= &root->free;
  for (next= root->free; next; next= *(last= &next->next))
  {
    next->left= next->size - ALIGN_SIZE(sizeof(USED_MEM));
    TRASH_MEM(next);
  }

  /* Combine the free and the used list */
  *last= next=root->used;

  /* now go through the used blocks and mark them free */
  for (; next; next= next->next)
  {
    next->left= next->size - ALIGN_SIZE(sizeof(USED_MEM));
    TRASH_MEM(next);
  }

  /* Now everything is set; Indicate that nothing is used anymore */
  root->used= 0;
  root->first_block_usage= 0;
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
    init_alloc_root() or with a bzero()-ed block.
    It's also safe to call this multiple times with the same mem_root.
*/

void free_root(MEM_ROOT *root, myf MyFlags)
{
  reg1 USED_MEM *next,*old;
  DBUG_ENTER("free_root");
  DBUG_PRINT("enter",("root: 0x%lx  flags: %u", root, (uint) MyFlags));

  if (!root)					/* QQ: Should be deleted */
    DBUG_VOID_RETURN; /* purecov: inspected */
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
      my_free((gptr) old,MYF(0));
  }
  for (next=root->free ; next ;)
  {
    old=next; next= next->next;
    if (old != root->pre_alloc)
      my_free((gptr) old,MYF(0));
  }
  root->used=root->free=0;
  if (root->pre_alloc)
  {
    root->free=root->pre_alloc;
    root->free->left=root->pre_alloc->size-ALIGN_SIZE(sizeof(USED_MEM));
    TRASH_MEM(root->pre_alloc);
    root->free->next=0;
  }
  root->block_num= 4;
  root->first_block_usage= 0;
  DBUG_VOID_RETURN;
}

/*
  Find block that contains an object and set the pre_alloc to it
*/

void set_prealloc_root(MEM_ROOT *root, char *ptr)
{
  USED_MEM *next;
  for (next=root->used; next ; next=next->next)
  {
    if ((char*) next <= ptr && (char*) next + next->size > ptr)
    {
      root->pre_alloc=next;
      return;
    }
  }
  for (next=root->free ; next ; next=next->next)
  {
    if ((char*) next <= ptr && (char*) next + next->size > ptr)
    {
      root->pre_alloc=next;
      return;
    }
  }
}


char *strdup_root(MEM_ROOT *root,const char *str)
{
  return strmake_root(root, str, (uint) strlen(str));
}

char *strmake_root(MEM_ROOT *root,const char *str, uint len)
{
  char *pos;
  if ((pos=alloc_root(root,len+1)))
  {
    memcpy(pos,str,len);
    pos[len]=0;
  }
  return pos;
}


char *memdup_root(MEM_ROOT *root,const char *str,uint len)
{
  char *pos;
  if ((pos=alloc_root(root,len)))
    memcpy(pos,str,len);
  return pos;
}
