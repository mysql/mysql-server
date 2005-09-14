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

/*
  These functions handle keyblock cacheing for ISAM and MyISAM tables.

  One cache can handle many files.
  It must contain buffers of the same blocksize.
  init_key_cache() should be used to init cache handler.

  The free list (free_block_list) is a stack like structure.
  When a block is freed by free_block(), it is pushed onto the stack.
  When a new block is required it is first tried to pop one from the stack.
  If the stack is empty, it is tried to get a never-used block from the pool.
  If this is empty too, then a block is taken from the LRU ring, flushing it
  to disk, if neccessary. This is handled in find_key_block().
  With the new free list, the blocks can have three temperatures:
  hot, warm and cold (which is free). This is remembered in the block header
  by the enum BLOCK_TEMPERATURE temperature variable. Remembering the
  temperature is neccessary to correctly count the number of warm blocks,
  which is required to decide when blocks are allowed to become hot. Whenever
  a block is inserted to another (sub-)chain, we take the old and new
  temperature into account to decide if we got one more or less warm block.
  blocks_unused is the sum of never used blocks in the pool and of currently
  free blocks. blocks_used is the number of blocks fetched from the pool and
  as such gives the maximum number of in-use blocks at any time.
*/

#include "mysys_priv.h"
#include <keycache.h>
#include "my_static.h"
#include <m_string.h>
#include <errno.h>
#include <stdarg.h>

/*
  Some compilation flags have been added specifically for this module
  to control the following:
  - not to let a thread to yield the control when reading directly
    from key cache, which might improve performance in many cases;
    to enable this add:
    #define SERIALIZED_READ_FROM_CACHE
  - to set an upper bound for number of threads simultaneously
    using the key cache; this setting helps to determine an optimal
    size for hash table and improve performance when the number of
    blocks in the key cache much less than the number of threads
    accessing it;
    to set this number equal to <N> add
      #define MAX_THREADS <N>
  - to substitute calls of pthread_cond_wait for calls of
    pthread_cond_timedwait (wait with timeout set up);
    this setting should be used only when you want to trap a deadlock
    situation, which theoretically should not happen;
    to set timeout equal to <T> seconds add
      #define KEYCACHE_TIMEOUT <T>
  - to enable the module traps and to send debug information from
    key cache module to a special debug log add:
      #define KEYCACHE_DEBUG
    the name of this debug log file <LOG NAME> can be set through:
      #define KEYCACHE_DEBUG_LOG  <LOG NAME>
    if the name is not defined, it's set by default;
    if the KEYCACHE_DEBUG flag is not set up and we are in a debug
    mode, i.e. when ! defined(DBUG_OFF), the debug information from the
    module is sent to the regular debug log.

  Example of the settings:
    #define SERIALIZED_READ_FROM_CACHE
    #define MAX_THREADS   100
    #define KEYCACHE_TIMEOUT  1
    #define KEYCACHE_DEBUG
    #define KEYCACHE_DEBUG_LOG  "my_key_cache_debug.log"
*/

#if defined(MSDOS) && !defined(M_IC80386)
/* we nead much memory */
#undef my_malloc_lock
#undef my_free_lock
#define my_malloc_lock(A,B)  halloc((long) (A/IO_SIZE),IO_SIZE)
#define my_free_lock(A,B)    hfree(A)
#endif /* defined(MSDOS) && !defined(M_IC80386) */

#define STRUCT_PTR(TYPE, MEMBER, a)                                           \
          (TYPE *) ((char *) (a) - offsetof(TYPE, MEMBER))

/* types of condition variables */
#define  COND_FOR_REQUESTED 0
#define  COND_FOR_SAVED     1
#define  COND_FOR_READERS   2

typedef pthread_cond_t KEYCACHE_CONDVAR;

/* descriptor of the page in the key cache block buffer */
struct st_keycache_page
{
  int file;               /* file to which the page belongs to  */
  my_off_t filepos;       /* position of the page in the file   */
};

/* element in the chain of a hash table bucket */
struct st_hash_link
{
  struct st_hash_link *next, **prev; /* to connect links in the same bucket  */
  struct st_block_link *block;       /* reference to the block for the page: */
  File file;                         /* from such a file                     */
  my_off_t diskpos;                  /* with such an offset                  */
  uint requests;                     /* number of requests for the page      */
};

/* simple states of a block */
#define BLOCK_ERROR       1   /* an error occured when performing disk i/o   */
#define BLOCK_READ        2   /* the is page in the block buffer             */
#define BLOCK_IN_SWITCH   4   /* block is preparing to read new page         */
#define BLOCK_REASSIGNED  8   /* block does not accept requests for old page */
#define BLOCK_IN_FLUSH   16   /* block is in flush operation                 */
#define BLOCK_CHANGED    32   /* block buffer contains a dirty page          */

/* page status, returned by find_key_block */
#define PAGE_READ               0
#define PAGE_TO_BE_READ         1
#define PAGE_WAIT_TO_BE_READ    2

/* block temperature determines in which (sub-)chain the block currently is */
enum BLOCK_TEMPERATURE { BLOCK_COLD /*free*/ , BLOCK_WARM , BLOCK_HOT };

/* key cache block */
struct st_block_link
{
  struct st_block_link
    *next_used, **prev_used;   /* to connect links in the LRU chain (ring)   */
  struct st_block_link
    *next_changed, **prev_changed; /* for lists of file dirty/clean blocks   */
  struct st_hash_link *hash_link; /* backward ptr to referring hash_link     */
  KEYCACHE_WQUEUE wqueue[2]; /* queues on waiting requests for new/old pages */
  uint requests;          /* number of requests for the block                */
  byte *buffer;           /* buffer for the block page                       */
  uint offset;            /* beginning of modified data in the buffer        */
  uint length;            /* end of data in the buffer                       */
  uint status;            /* state of the block                              */
  enum BLOCK_TEMPERATURE temperature; /* block temperature: cold, warm, hot */
  uint hits_left;         /* number of hits left until promotion             */
  ulonglong last_hit_time; /* timestamp of the last hit                      */
  KEYCACHE_CONDVAR *condvar; /* condition variable for 'no readers' event    */
};

KEY_CACHE dflt_key_cache_var;
KEY_CACHE *dflt_key_cache= &dflt_key_cache_var;

#define FLUSH_CACHE         2000            /* sort this many blocks at once */

static int flush_all_key_blocks(KEY_CACHE *keycache);
static void link_into_queue(KEYCACHE_WQUEUE *wqueue,
                                   struct st_my_thread_var *thread);
static void unlink_from_queue(KEYCACHE_WQUEUE *wqueue,
                                     struct st_my_thread_var *thread);
static void free_block(KEY_CACHE *keycache, BLOCK_LINK *block);
static void test_key_cache(KEY_CACHE *keycache,
                           const char *where, my_bool lock);

#define KEYCACHE_HASH(f, pos)                                                 \
(((ulong) ((pos) >> keycache->key_cache_shift)+                               \
                                     (ulong) (f)) & (keycache->hash_entries-1))
#define FILE_HASH(f)                 ((uint) (f) & (CHANGED_BLOCKS_HASH-1))

#define DEFAULT_KEYCACHE_DEBUG_LOG  "keycache_debug.log"

#if defined(KEYCACHE_DEBUG) && ! defined(KEYCACHE_DEBUG_LOG)
#define KEYCACHE_DEBUG_LOG  DEFAULT_KEYCACHE_DEBUG_LOG
#endif

#if defined(KEYCACHE_DEBUG_LOG)
static FILE *keycache_debug_log=NULL;
static void keycache_debug_print _VARARGS((const char *fmt,...));
#define KEYCACHE_DEBUG_OPEN                                                   \
          if (!keycache_debug_log)                                            \
          {                                                                   \
            keycache_debug_log= fopen(KEYCACHE_DEBUG_LOG, "w");               \
            (void) setvbuf(keycache_debug_log, NULL, _IOLBF, BUFSIZ);         \
          }

#define KEYCACHE_DEBUG_CLOSE                                                  \
          if (keycache_debug_log)                                             \
          {                                                                   \
            fclose(keycache_debug_log);                                       \
            keycache_debug_log= 0;                                            \
          }
#else
#define KEYCACHE_DEBUG_OPEN
#define KEYCACHE_DEBUG_CLOSE
#endif /* defined(KEYCACHE_DEBUG_LOG) */

#if defined(KEYCACHE_DEBUG_LOG) && defined(KEYCACHE_DEBUG)
#define KEYCACHE_DBUG_PRINT(l, m)                                             \
            { if (keycache_debug_log) fprintf(keycache_debug_log, "%s: ", l); \
              keycache_debug_print m; }

#define KEYCACHE_DBUG_ASSERT(a)                                               \
            { if (! (a) && keycache_debug_log) fclose(keycache_debug_log);    \
              assert(a); }
#else
#define KEYCACHE_DBUG_PRINT(l, m)  DBUG_PRINT(l, m)
#define KEYCACHE_DBUG_ASSERT(a)    DBUG_ASSERT(a)
#endif /* defined(KEYCACHE_DEBUG_LOG) && defined(KEYCACHE_DEBUG) */

#if defined(KEYCACHE_DEBUG) || !defined(DBUG_OFF)
static long keycache_thread_id;
#define KEYCACHE_THREAD_TRACE(l)                                              \
             KEYCACHE_DBUG_PRINT(l,("|thread %ld",keycache_thread_id))

#define KEYCACHE_THREAD_TRACE_BEGIN(l)                                        \
            { struct st_my_thread_var *thread_var= my_thread_var;             \
              keycache_thread_id= thread_var->id;                             \
              KEYCACHE_DBUG_PRINT(l,("[thread %ld",keycache_thread_id)) }

#define KEYCACHE_THREAD_TRACE_END(l)                                          \
            KEYCACHE_DBUG_PRINT(l,("]thread %ld",keycache_thread_id))
#else
#define KEYCACHE_THREAD_TRACE_BEGIN(l)
#define KEYCACHE_THREAD_TRACE_END(l)
#define KEYCACHE_THREAD_TRACE(l)
#endif /* defined(KEYCACHE_DEBUG) || !defined(DBUG_OFF) */

#define BLOCK_NUMBER(b)                                                       \
  ((uint) (((char*)(b)-(char *) keycache->block_root)/sizeof(BLOCK_LINK)))
#define HASH_LINK_NUMBER(h)                                                   \
  ((uint) (((char*)(h)-(char *) keycache->hash_link_root)/sizeof(HASH_LINK)))

#if (defined(KEYCACHE_TIMEOUT) && !defined(__WIN__)) || defined(KEYCACHE_DEBUG)
static int keycache_pthread_cond_wait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex);
#else
#define  keycache_pthread_cond_wait pthread_cond_wait
#endif

#if defined(KEYCACHE_DEBUG)
static int keycache_pthread_mutex_lock(pthread_mutex_t *mutex);
static void keycache_pthread_mutex_unlock(pthread_mutex_t *mutex);
static int keycache_pthread_cond_signal(pthread_cond_t *cond);
#else
#define keycache_pthread_mutex_lock pthread_mutex_lock
#define keycache_pthread_mutex_unlock pthread_mutex_unlock
#define keycache_pthread_cond_signal pthread_cond_signal
#endif /* defined(KEYCACHE_DEBUG) */

static uint next_power(uint value)
{
  uint old_value= 1;
  while (value)
  {
    old_value= value;
    value&= value-1;
  }
  return (old_value << 1);
}


/*
  Initialize a key cache

  SYNOPSIS
    init_key_cache()
    keycache			pointer to a key cache data structure
    key_cache_block_size	size of blocks to keep cached data
    use_mem                 	total memory to use for the key cache
    division_limit		division limit (may be zero)
    age_threshold		age threshold (may be zero)

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.

  NOTES.
    if keycache->key_cache_inited != 0 we assume that the key cache
    is already initialized.  This is for now used by myisamchk, but shouldn't
    be something that a program should rely on!

    It's assumed that no two threads call this function simultaneously
    referring to the same key cache handle.

*/

int init_key_cache(KEY_CACHE *keycache, uint key_cache_block_size,
		   ulong use_mem, uint division_limit,
		   uint age_threshold)
{
  uint blocks, hash_links, length;
  int error;
  DBUG_ENTER("init_key_cache");
  DBUG_ASSERT(key_cache_block_size >= 512);

  KEYCACHE_DEBUG_OPEN;
  if (keycache->key_cache_inited && keycache->disk_blocks > 0)
  {
    DBUG_PRINT("warning",("key cache already in use"));
    DBUG_RETURN(0);
  }

  keycache->global_cache_w_requests= keycache->global_cache_r_requests= 0;
  keycache->global_cache_read= keycache->global_cache_write= 0;
  keycache->disk_blocks= -1;
  if (! keycache->key_cache_inited)
  {
    keycache->key_cache_inited= 1;
    keycache->in_init= 0;
    pthread_mutex_init(&keycache->cache_lock, MY_MUTEX_INIT_FAST);
    keycache->resize_queue.last_thread= NULL;
  }

  keycache->key_cache_mem_size= use_mem;
  keycache->key_cache_block_size= key_cache_block_size;
  keycache->key_cache_shift= my_bit_log2(key_cache_block_size);
  DBUG_PRINT("info", ("key_cache_block_size: %u",
		      key_cache_block_size));

  blocks= (uint) (use_mem / (sizeof(BLOCK_LINK) + 2 * sizeof(HASH_LINK) +
			     sizeof(HASH_LINK*) * 5/4 + key_cache_block_size));
  /* It doesn't make sense to have too few blocks (less than 8) */
  if (blocks >= 8 && keycache->disk_blocks < 0)
  {
    for ( ; ; )
    {
      /* Set my_hash_entries to the next bigger 2 power */
      if ((keycache->hash_entries= next_power(blocks)) < blocks * 5/4)
        keycache->hash_entries<<= 1;
      hash_links= 2 * blocks;
#if defined(MAX_THREADS)
      if (hash_links < MAX_THREADS + blocks - 1)
        hash_links= MAX_THREADS + blocks - 1;
#endif
      while ((length= (ALIGN_SIZE(blocks * sizeof(BLOCK_LINK)) +
		       ALIGN_SIZE(hash_links * sizeof(HASH_LINK)) +
		       ALIGN_SIZE(sizeof(HASH_LINK*) *
                                  keycache->hash_entries))) +
	     ((ulong) blocks << keycache->key_cache_shift) > use_mem)
        blocks--;
      /* Allocate memory for cache page buffers */
      if ((keycache->block_mem=
	   my_malloc_lock((ulong) blocks * keycache->key_cache_block_size,
			  MYF(0))))
      {
        /*
	  Allocate memory for blocks, hash_links and hash entries;
	  For each block 2 hash links are allocated
        */
        if ((keycache->block_root= (BLOCK_LINK*) my_malloc((uint) length,
                                                           MYF(0))))
          break;
        my_free_lock(keycache->block_mem, MYF(0));
        keycache->block_mem= 0;
      }
      if (blocks < 8)
      {
        my_errno= ENOMEM;
        goto err;
      }
      blocks= blocks / 4*3;
    }
    keycache->blocks_unused= (ulong) blocks;
    keycache->disk_blocks= (int) blocks;
    keycache->hash_links= hash_links;
    keycache->hash_root= (HASH_LINK**) ((char*) keycache->block_root +
				        ALIGN_SIZE(blocks*sizeof(BLOCK_LINK)));
    keycache->hash_link_root= (HASH_LINK*) ((char*) keycache->hash_root +
				            ALIGN_SIZE((sizeof(HASH_LINK*) *
							keycache->hash_entries)));
    bzero((byte*) keycache->block_root,
	  keycache->disk_blocks * sizeof(BLOCK_LINK));
    bzero((byte*) keycache->hash_root,
          keycache->hash_entries * sizeof(HASH_LINK*));
    bzero((byte*) keycache->hash_link_root,
	  keycache->hash_links * sizeof(HASH_LINK));
    keycache->hash_links_used= 0;
    keycache->free_hash_list= NULL;
    keycache->blocks_used= keycache->blocks_changed= 0;

    keycache->global_blocks_changed= 0;
    keycache->blocks_available=0;		/* For debugging */

    /* The LRU chain is empty after initialization */
    keycache->used_last= NULL;
    keycache->used_ins= NULL;
    keycache->free_block_list= NULL;
    keycache->keycache_time= 0;
    keycache->warm_blocks= 0;
    keycache->min_warm_blocks= (division_limit ?
				blocks * division_limit / 100 + 1 :
				blocks);
    keycache->age_threshold= (age_threshold ?
			      blocks * age_threshold / 100 :
			      blocks);

    keycache->cnt_for_resize_op= 0;
    keycache->resize_in_flush= 0;
    keycache->can_be_used= 1;

    keycache->waiting_for_hash_link.last_thread= NULL;
    keycache->waiting_for_block.last_thread= NULL;
    DBUG_PRINT("exit",
	       ("disk_blocks: %d  block_root: 0x%lx  hash_entries: %d\
 hash_root: 0x%lx  hash_links: %d  hash_link_root: 0x%lx",
		keycache->disk_blocks, keycache->block_root,
		keycache->hash_entries, keycache->hash_root,
		keycache->hash_links, keycache->hash_link_root));
    bzero((gptr) keycache->changed_blocks,
	  sizeof(keycache->changed_blocks[0]) * CHANGED_BLOCKS_HASH);
    bzero((gptr) keycache->file_blocks,
	  sizeof(keycache->file_blocks[0]) * CHANGED_BLOCKS_HASH);
  }

  keycache->blocks= keycache->disk_blocks > 0 ? keycache->disk_blocks : 0;
  DBUG_RETURN((int) keycache->disk_blocks);

err:
  error= my_errno;
  keycache->disk_blocks= 0;
  keycache->blocks=  0;
  if (keycache->block_mem)
  {
    my_free_lock((gptr) keycache->block_mem, MYF(0));
    keycache->block_mem= NULL;
  }
  if (keycache->block_root)
  {
    my_free((gptr) keycache->block_root, MYF(0));
    keycache->block_root= NULL;
  }
  my_errno= error;
  keycache->can_be_used= 0;
  DBUG_RETURN(0);
}


/*
  Resize a key cache

  SYNOPSIS
    resize_key_cache()
    keycache     	        pointer to a key cache data structure
    key_cache_block_size        size of blocks to keep cached data
    use_mem			total memory to use for the new key cache
    division_limit		new division limit (if not zero)
    age_threshold		new age threshold (if not zero)

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.

  NOTES.
    The function first compares the memory size and the block size parameters
    with the key cache values.

    If they differ the function free the the memory allocated for the
    old key cache blocks by calling the end_key_cache function and
    then rebuilds the key cache with new blocks by calling
    init_key_cache.

    The function starts the operation only when all other threads
    performing operations with the key cache let her to proceed
    (when cnt_for_resize=0).
*/

int resize_key_cache(KEY_CACHE *keycache, uint key_cache_block_size,
		     ulong use_mem, uint division_limit,
		     uint age_threshold)
{
  int blocks;
  struct st_my_thread_var *thread;
  KEYCACHE_WQUEUE *wqueue;
  DBUG_ENTER("resize_key_cache");

  if (!keycache->key_cache_inited)
    DBUG_RETURN(keycache->disk_blocks);

  if(key_cache_block_size == keycache->key_cache_block_size &&
     use_mem == keycache->key_cache_mem_size)
  {
    change_key_cache_param(keycache, division_limit, age_threshold);
    DBUG_RETURN(keycache->disk_blocks);
  }

  keycache_pthread_mutex_lock(&keycache->cache_lock);

  wqueue= &keycache->resize_queue;
  thread= my_thread_var;
  link_into_queue(wqueue, thread);

  while (wqueue->last_thread->next != thread)
  {
    keycache_pthread_cond_wait(&thread->suspend, &keycache->cache_lock);
  }

  keycache->resize_in_flush= 1;
  if (flush_all_key_blocks(keycache))
  {
    /* TODO: if this happens, we should write a warning in the log file ! */
    keycache->resize_in_flush= 0;
    blocks= 0;
    keycache->can_be_used= 0;
    goto finish;
  }
  keycache->resize_in_flush= 0;
  keycache->can_be_used= 0;
  while (keycache->cnt_for_resize_op)
  {
    KEYCACHE_DBUG_PRINT("resize_key_cache: wait",
                        ("suspend thread %ld", thread->id));
    keycache_pthread_cond_wait(&thread->suspend, &keycache->cache_lock);
  }

  end_key_cache(keycache, 0);			/* Don't free mutex */
  /* The following will work even if use_mem is 0 */
  blocks= init_key_cache(keycache, key_cache_block_size, use_mem,
			 division_limit, age_threshold);

finish:
  unlink_from_queue(wqueue, thread);
  /* Signal for the next resize request to proceeed if any */
  if (wqueue->last_thread)
  {
    KEYCACHE_DBUG_PRINT("resize_key_cache: signal",
                        ("thread %ld", wqueue->last_thread->next->id));
    keycache_pthread_cond_signal(&wqueue->last_thread->next->suspend);
  }
  keycache_pthread_mutex_unlock(&keycache->cache_lock);
  return blocks;
}


/*
  Increment counter blocking resize key cache operation
*/
static inline void inc_counter_for_resize_op(KEY_CACHE *keycache)
{
  keycache->cnt_for_resize_op++;
}


/*
  Decrement counter blocking resize key cache operation;
  Signal the operation to proceed when counter becomes equal zero
*/
static inline void dec_counter_for_resize_op(KEY_CACHE *keycache)
{
  struct st_my_thread_var *last_thread;
  if (!--keycache->cnt_for_resize_op &&
      (last_thread= keycache->resize_queue.last_thread))
  {
    KEYCACHE_DBUG_PRINT("dec_counter_for_resize_op: signal",
                        ("thread %ld", last_thread->next->id));
    keycache_pthread_cond_signal(&last_thread->next->suspend);
  }
}

/*
  Change the key cache parameters

  SYNOPSIS
    change_key_cache_param()
    keycache			pointer to a key cache data structure
    division_limit		new division limit (if not zero)
    age_threshold		new age threshold (if not zero)

  RETURN VALUE
    none

  NOTES.
    Presently the function resets the key cache parameters
    concerning midpoint insertion strategy - division_limit and
    age_threshold.
*/

void change_key_cache_param(KEY_CACHE *keycache, uint division_limit,
			    uint age_threshold)
{
  DBUG_ENTER("change_key_cache_param");

  keycache_pthread_mutex_lock(&keycache->cache_lock);
  if (division_limit)
    keycache->min_warm_blocks= (keycache->disk_blocks *
				division_limit / 100 + 1);
  if (age_threshold)
    keycache->age_threshold=   (keycache->disk_blocks *
				age_threshold / 100);
  keycache_pthread_mutex_unlock(&keycache->cache_lock);
  DBUG_VOID_RETURN;
}


/*
  Remove key_cache from memory

  SYNOPSIS
    end_key_cache()
    keycache		key cache handle
    cleanup		Complete free (Free also mutex for key cache)

  RETURN VALUE
    none
*/

void end_key_cache(KEY_CACHE *keycache, my_bool cleanup)
{
  DBUG_ENTER("end_key_cache");
  DBUG_PRINT("enter", ("key_cache: 0x%lx", keycache));

  if (!keycache->key_cache_inited)
    DBUG_VOID_RETURN;

  if (keycache->disk_blocks > 0)
  {
    if (keycache->block_mem)
    {
      my_free_lock((gptr) keycache->block_mem, MYF(0));
      keycache->block_mem= NULL;
      my_free((gptr) keycache->block_root, MYF(0));
      keycache->block_root= NULL;
    }
    keycache->disk_blocks= -1;
    /* Reset blocks_changed to be safe if flush_all_key_blocks is called */
    keycache->blocks_changed= 0;
  }

  DBUG_PRINT("status", ("used: %d  changed: %d  w_requests: %lu  "
                        "writes: %lu  r_requests: %lu  reads: %lu",
                        keycache->blocks_used, keycache->global_blocks_changed,
                        (ulong) keycache->global_cache_w_requests,
                        (ulong) keycache->global_cache_write,
                        (ulong) keycache->global_cache_r_requests,
                        (ulong) keycache->global_cache_read));

  if (cleanup)
  {
    pthread_mutex_destroy(&keycache->cache_lock);
    keycache->key_cache_inited= keycache->can_be_used= 0;
    KEYCACHE_DEBUG_CLOSE;
  }
  DBUG_VOID_RETURN;
} /* end_key_cache */


/*
  Link a thread into double-linked queue of waiting threads.

  SYNOPSIS
    link_into_queue()
      wqueue              pointer to the queue structure
      thread              pointer to the thread to be added to the queue

  RETURN VALUE
    none

  NOTES.
    Queue is represented by a circular list of the thread structures
    The list is double-linked of the type (**prev,*next), accessed by
    a pointer to the last element.
*/

static void link_into_queue(KEYCACHE_WQUEUE *wqueue,
                                   struct st_my_thread_var *thread)
{
  struct st_my_thread_var *last;
  if (! (last= wqueue->last_thread))
  {
    /* Queue is empty */
    thread->next= thread;
    thread->prev= &thread->next;
  }
  else
  {
    thread->prev= last->next->prev;
    last->next->prev= &thread->next;
    thread->next= last->next;
    last->next= thread;
  }
  wqueue->last_thread= thread;
}

/*
  Unlink a thread from double-linked queue of waiting threads

  SYNOPSIS
    unlink_from_queue()
      wqueue              pointer to the queue structure
      thread              pointer to the thread to be removed from the queue

  RETURN VALUE
    none

  NOTES.
    See NOTES for link_into_queue
*/

static void unlink_from_queue(KEYCACHE_WQUEUE *wqueue,
                                     struct st_my_thread_var *thread)
{
  KEYCACHE_DBUG_PRINT("unlink_from_queue", ("thread %ld", thread->id));
  if (thread->next == thread)
    /* The queue contains only one member */
    wqueue->last_thread= NULL;
  else
  {
    thread->next->prev= thread->prev;
    *thread->prev=thread->next;
    if (wqueue->last_thread == thread)
      wqueue->last_thread= STRUCT_PTR(struct st_my_thread_var, next,
                                      thread->prev);
  }
  thread->next= NULL;
}


/*
  Add a thread to single-linked queue of waiting threads

  SYNOPSIS
    add_to_queue()
      wqueue              pointer to the queue structure
      thread              pointer to the thread to be added to the queue

  RETURN VALUE
    none

  NOTES.
    Queue is represented by a circular list of the thread structures
    The list is single-linked of the type (*next), accessed by a pointer
    to the last element.
*/

static inline void add_to_queue(KEYCACHE_WQUEUE *wqueue,
                                struct st_my_thread_var *thread)
{
  struct st_my_thread_var *last;
  if (! (last= wqueue->last_thread))
    thread->next= thread;
  else
  {
    thread->next= last->next;
    last->next= thread;
  }
  wqueue->last_thread= thread;
}


/*
  Remove all threads from queue signaling them to proceed

  SYNOPSIS
    realease_queue()
      wqueue              pointer to the queue structure
      thread              pointer to the thread to be added to the queue

  RETURN VALUE
    none

  NOTES.
    See notes for add_to_queue
    When removed from the queue each thread is signaled via condition
    variable thread->suspend.
*/

static void release_queue(KEYCACHE_WQUEUE *wqueue)
{
  struct st_my_thread_var *last= wqueue->last_thread;
  struct st_my_thread_var *next= last->next;
  struct st_my_thread_var *thread;
  do
  {
    thread=next;
    KEYCACHE_DBUG_PRINT("release_queue: signal", ("thread %ld", thread->id));
    keycache_pthread_cond_signal(&thread->suspend);
    next=thread->next;
    thread->next= NULL;
  }
  while (thread != last);
  wqueue->last_thread= NULL;
}


/*
  Unlink a block from the chain of dirty/clean blocks
*/

static inline void unlink_changed(BLOCK_LINK *block)
{
  if (block->next_changed)
    block->next_changed->prev_changed= block->prev_changed;
  *block->prev_changed= block->next_changed;
}


/*
  Link a block into the chain of dirty/clean blocks
*/

static inline void link_changed(BLOCK_LINK *block, BLOCK_LINK **phead)
{
  block->prev_changed= phead;
  if ((block->next_changed= *phead))
    (*phead)->prev_changed= &block->next_changed;
  *phead= block;
}


/*
  Unlink a block from the chain of dirty/clean blocks, if it's asked for,
  and link it to the chain of clean blocks for the specified file
*/

static void link_to_file_list(KEY_CACHE *keycache,
                              BLOCK_LINK *block, int file, my_bool unlink)
{
  if (unlink)
    unlink_changed(block);
  link_changed(block, &keycache->file_blocks[FILE_HASH(file)]);
  if (block->status & BLOCK_CHANGED)
  {
    block->status&= ~BLOCK_CHANGED;
    keycache->blocks_changed--;
    keycache->global_blocks_changed--;
  }
}


/*
  Unlink a block from the chain of clean blocks for the specified
  file and link it to the chain of dirty blocks for this file
*/

static inline void link_to_changed_list(KEY_CACHE *keycache,
                                        BLOCK_LINK *block)
{
  unlink_changed(block);
  link_changed(block,
               &keycache->changed_blocks[FILE_HASH(block->hash_link->file)]);
  block->status|=BLOCK_CHANGED;
  keycache->blocks_changed++;
  keycache->global_blocks_changed++;
}


/*
  Link a block to the LRU chain at the beginning or at the end of
  one of two parts.

  SYNOPSIS
    link_block()
      keycache            pointer to a key cache data structure
      block               pointer to the block to link to the LRU chain
      hot                 <-> to link the block into the hot subchain
      at_end              <-> to link the block at the end of the subchain

  RETURN VALUE
    none

  NOTES.
    The LRU chain is represented by a curcular list of block structures.
    The list is double-linked of the type (**prev,*next) type.
    The LRU chain is divided into two parts - hot and warm.
    There are two pointers to access the last blocks of these two
    parts. The beginning of the warm part follows right after the
    end of the hot part.
    Only blocks of the warm part can be used for replacement.
    The first block from the beginning of this subchain is always
    taken for eviction (keycache->last_used->next)

    LRU chain:       +------+   H O T    +------+
                +----| end  |----...<----| beg  |----+
                |    +------+last        +------+    |
                v<-link in latest hot (new end)      |
                |     link in latest warm (new end)->^
                |    +------+  W A R M   +------+    |
                +----| beg  |---->...----| end  |----+
                     +------+            +------+ins
                  first for eviction
*/

static void link_block(KEY_CACHE *keycache, BLOCK_LINK *block, my_bool hot,
                       my_bool at_end)
{
  BLOCK_LINK *ins;
  BLOCK_LINK **pins;

  KEYCACHE_DBUG_ASSERT(! (block->hash_link && block->hash_link->requests));
  if (!hot && keycache->waiting_for_block.last_thread)
  {
    /* Signal that in the LRU warm sub-chain an available block has appeared */
    struct st_my_thread_var *last_thread=
                               keycache->waiting_for_block.last_thread;
    struct st_my_thread_var *first_thread= last_thread->next;
    struct st_my_thread_var *next_thread= first_thread;
    HASH_LINK *hash_link= (HASH_LINK *) first_thread->opt_info;
    struct st_my_thread_var *thread;
    do
    {
      thread= next_thread;
      next_thread= thread->next;
      /*
         We notify about the event all threads that ask
         for the same page as the first thread in the queue
      */
      if ((HASH_LINK *) thread->opt_info == hash_link)
      {
        KEYCACHE_DBUG_PRINT("link_block: signal", ("thread %ld", thread->id));
        keycache_pthread_cond_signal(&thread->suspend);
        unlink_from_queue(&keycache->waiting_for_block, thread);
        block->requests++;
      }
    }
    while (thread != last_thread);
    hash_link->block= block;
    KEYCACHE_THREAD_TRACE("link_block: after signaling");
#if defined(KEYCACHE_DEBUG)
    KEYCACHE_DBUG_PRINT("link_block",
        ("linked,unlinked block %u  status=%x  #requests=%u  #available=%u",
         BLOCK_NUMBER(block), block->status,
         block->requests, keycache->blocks_available));
#endif
    return;
  }
  pins= hot ? &keycache->used_ins : &keycache->used_last;
  ins= *pins;
  if (ins)
  {
    ins->next_used->prev_used= &block->next_used;
    block->next_used= ins->next_used;
    block->prev_used= &ins->next_used;
    ins->next_used= block;
    if (at_end)
      *pins= block;
  }
  else
  {
    /* The LRU chain is empty */
    keycache->used_last= keycache->used_ins= block->next_used= block;
    block->prev_used= &block->next_used;
  }
  KEYCACHE_THREAD_TRACE("link_block");
#if defined(KEYCACHE_DEBUG)
  keycache->blocks_available++;
  KEYCACHE_DBUG_PRINT("link_block",
      ("linked block %u:%1u  status=%x  #requests=%u  #available=%u",
       BLOCK_NUMBER(block), at_end, block->status,
       block->requests, keycache->blocks_available));
  KEYCACHE_DBUG_ASSERT((ulong) keycache->blocks_available <=
                       keycache->blocks_used);
#endif
}


/*
  Unlink a block from the LRU chain

  SYNOPSIS
    unlink_block()
      keycache            pointer to a key cache data structure
      block               pointer to the block to unlink from the LRU chain

  RETURN VALUE
    none

  NOTES.
    See NOTES for link_block
*/

static void unlink_block(KEY_CACHE *keycache, BLOCK_LINK *block)
{
  if (block->next_used == block)
    /* The list contains only one member */
    keycache->used_last= keycache->used_ins= NULL;
  else
  {
    block->next_used->prev_used= block->prev_used;
    *block->prev_used= block->next_used;
    if (keycache->used_last == block)
      keycache->used_last= STRUCT_PTR(BLOCK_LINK, next_used, block->prev_used);
    if (keycache->used_ins == block)
      keycache->used_ins=STRUCT_PTR(BLOCK_LINK, next_used, block->prev_used);
  }
  block->next_used= NULL;

  KEYCACHE_THREAD_TRACE("unlink_block");
#if defined(KEYCACHE_DEBUG)
  keycache->blocks_available--;
  KEYCACHE_DBUG_PRINT("unlink_block",
    ("unlinked block %u  status=%x   #requests=%u  #available=%u",
     BLOCK_NUMBER(block), block->status,
     block->requests, keycache->blocks_available));
  KEYCACHE_DBUG_ASSERT(keycache->blocks_available >= 0);
#endif
}


/*
  Register requests for a block
*/
static void reg_requests(KEY_CACHE *keycache, BLOCK_LINK *block, int count)
{
  if (! block->requests)
    /* First request for the block unlinks it */
    unlink_block(keycache, block);
  block->requests+=count;
}


/*
  Unregister request for a block
  linking it to the LRU chain if it's the last request

  SYNOPSIS
    unreg_request()
    keycache            pointer to a key cache data structure
    block               pointer to the block to link to the LRU chain
    at_end              <-> to link the block at the end of the LRU chain

  RETURN VALUE
    none

  NOTES.
    Every linking to the LRU chain decrements by one a special block
    counter (if it's positive). If the at_end parameter is TRUE the block is
    added either at the end of warm sub-chain or at the end of hot sub-chain.
    It is added to the hot subchain if its counter is zero and number of
    blocks in warm sub-chain is not less than some low limit (determined by
    the division_limit parameter). Otherwise the block is added to the warm
    sub-chain. If the at_end parameter is FALSE the block is always added
    at beginning of the warm sub-chain.
    Thus a warm block can be promoted to the hot sub-chain when its counter
    becomes zero for the first time.
    At the same time  the block at the very beginning of the hot subchain
    might be moved to the beginning of the warm subchain if it stays untouched
    for a too long time (this time is determined by parameter age_threshold).
*/

static void unreg_request(KEY_CACHE *keycache,
                          BLOCK_LINK *block, int at_end)
{
  if (! --block->requests)
  {
    my_bool hot;
    if (block->hits_left)
      block->hits_left--;
    hot= !block->hits_left && at_end &&
      keycache->warm_blocks > keycache->min_warm_blocks;
    if (hot)
    {
      if (block->temperature == BLOCK_WARM)
        keycache->warm_blocks--;
      block->temperature= BLOCK_HOT;
      KEYCACHE_DBUG_PRINT("unreg_request", ("#warm_blocks=%u",
                           keycache->warm_blocks));
    }
    link_block(keycache, block, hot, (my_bool)at_end);
    block->last_hit_time= keycache->keycache_time;
    keycache->keycache_time++;

    block= keycache->used_ins;
    /* Check if we should link a hot block to the warm block */
    if (block && keycache->keycache_time - block->last_hit_time >
	keycache->age_threshold)
    {
      unlink_block(keycache, block);
      link_block(keycache, block, 0, 0);
      if (block->temperature != BLOCK_WARM)
      {
        keycache->warm_blocks++;
        block->temperature= BLOCK_WARM;
      }
      KEYCACHE_DBUG_PRINT("unreg_request", ("#warm_blocks=%u",
                           keycache->warm_blocks));
    }
  }
}

/*
  Remove a reader of the page in block
*/

static inline void remove_reader(BLOCK_LINK *block)
{
  if (! --block->hash_link->requests && block->condvar)
    keycache_pthread_cond_signal(block->condvar);
}


/*
  Wait until the last reader of the page in block
  signals on its termination
*/

static inline void wait_for_readers(KEY_CACHE *keycache, BLOCK_LINK *block)
{
  struct st_my_thread_var *thread= my_thread_var;
  while (block->hash_link->requests)
  {
    KEYCACHE_DBUG_PRINT("wait_for_readers: wait",
                        ("suspend thread %ld  block %u",
                         thread->id, BLOCK_NUMBER(block)));
    block->condvar= &thread->suspend;
    keycache_pthread_cond_wait(&thread->suspend, &keycache->cache_lock);
    block->condvar= NULL;
  }
}


/*
  Add a hash link to a bucket in the hash_table
*/

static inline void link_hash(HASH_LINK **start, HASH_LINK *hash_link)
{
  if (*start)
    (*start)->prev= &hash_link->next;
  hash_link->next= *start;
  hash_link->prev= start;
  *start= hash_link;
}


/*
  Remove a hash link from the hash table
*/

static void unlink_hash(KEY_CACHE *keycache, HASH_LINK *hash_link)
{
  KEYCACHE_DBUG_PRINT("unlink_hash", ("fd: %u  pos_ %lu  #requests=%u",
      (uint) hash_link->file,(ulong) hash_link->diskpos, hash_link->requests));
  KEYCACHE_DBUG_ASSERT(hash_link->requests == 0);
  if ((*hash_link->prev= hash_link->next))
    hash_link->next->prev= hash_link->prev;
  hash_link->block= NULL;
  if (keycache->waiting_for_hash_link.last_thread)
  {
    /* Signal that a free hash link has appeared */
    struct st_my_thread_var *last_thread=
                               keycache->waiting_for_hash_link.last_thread;
    struct st_my_thread_var *first_thread= last_thread->next;
    struct st_my_thread_var *next_thread= first_thread;
    KEYCACHE_PAGE *first_page= (KEYCACHE_PAGE *) (first_thread->opt_info);
    struct st_my_thread_var *thread;

    hash_link->file= first_page->file;
    hash_link->diskpos= first_page->filepos;
    do
    {
      KEYCACHE_PAGE *page;
      thread= next_thread;
      page= (KEYCACHE_PAGE *) thread->opt_info;
      next_thread= thread->next;
      /*
         We notify about the event all threads that ask
         for the same page as the first thread in the queue
      */
      if (page->file == hash_link->file && page->filepos == hash_link->diskpos)
      {
        KEYCACHE_DBUG_PRINT("unlink_hash: signal", ("thread %ld", thread->id));
        keycache_pthread_cond_signal(&thread->suspend);
        unlink_from_queue(&keycache->waiting_for_hash_link, thread);
      }
    }
    while (thread != last_thread);
    link_hash(&keycache->hash_root[KEYCACHE_HASH(hash_link->file,
					         hash_link->diskpos)],
              hash_link);
    return;
  }
  hash_link->next= keycache->free_hash_list;
  keycache->free_hash_list= hash_link;
}


/*
  Get the hash link for a page
*/

static HASH_LINK *get_hash_link(KEY_CACHE *keycache,
                                int file, my_off_t filepos)
{
  reg1 HASH_LINK *hash_link, **start;
  KEYCACHE_PAGE page;
#if defined(KEYCACHE_DEBUG)
  int cnt;
#endif

  KEYCACHE_DBUG_PRINT("get_hash_link", ("fd: %u  pos: %lu",
                      (uint) file,(ulong) filepos));

restart:
  /*
     Find the bucket in the hash table for the pair (file, filepos);
     start contains the head of the bucket list,
     hash_link points to the first member of the list
  */
  hash_link= *(start= &keycache->hash_root[KEYCACHE_HASH(file, filepos)]);
#if defined(KEYCACHE_DEBUG)
  cnt= 0;
#endif
  /* Look for an element for the pair (file, filepos) in the bucket chain */
  while (hash_link &&
         (hash_link->diskpos != filepos || hash_link->file != file))
  {
    hash_link= hash_link->next;
#if defined(KEYCACHE_DEBUG)
    cnt++;
    if (! (cnt <= keycache->hash_links_used))
    {
      int i;
      for (i=0, hash_link= *start ;
           i < cnt ; i++, hash_link= hash_link->next)
      {
        KEYCACHE_DBUG_PRINT("get_hash_link", ("fd: %u  pos: %lu",
            (uint) hash_link->file,(ulong) hash_link->diskpos));
      }
    }
    KEYCACHE_DBUG_ASSERT(cnt <= keycache->hash_links_used);
#endif
  }
  if (! hash_link)
  {
    /* There is no hash link in the hash table for the pair (file, filepos) */
    if (keycache->free_hash_list)
    {
      hash_link= keycache->free_hash_list;
      keycache->free_hash_list= hash_link->next;
    }
    else if (keycache->hash_links_used < keycache->hash_links)
    {
      hash_link= &keycache->hash_link_root[keycache->hash_links_used++];
    }
    else
    {
      /* Wait for a free hash link */
      struct st_my_thread_var *thread= my_thread_var;
      KEYCACHE_DBUG_PRINT("get_hash_link", ("waiting"));
      page.file= file;
      page.filepos= filepos;
      thread->opt_info= (void *) &page;
      link_into_queue(&keycache->waiting_for_hash_link, thread);
      KEYCACHE_DBUG_PRINT("get_hash_link: wait",
                        ("suspend thread %ld", thread->id));
      keycache_pthread_cond_wait(&thread->suspend,
                                 &keycache->cache_lock);
      thread->opt_info= NULL;
      goto restart;
    }
    hash_link->file= file;
    hash_link->diskpos= filepos;
    link_hash(start, hash_link);
  }
  /* Register the request for the page */
  hash_link->requests++;

  return hash_link;
}


/*
  Get a block for the file page requested by a keycache read/write operation;
  If the page is not in the cache return a free block, if there is none
  return the lru block after saving its buffer if the page is dirty.

  SYNOPSIS

    find_key_block()
      keycache            pointer to a key cache data structure
      file                handler for the file to read page from
      filepos             position of the page in the file
      init_hits_left      how initialize the block counter for the page
      wrmode              <-> get for writing
      page_st        out  {PAGE_READ,PAGE_TO_BE_READ,PAGE_WAIT_TO_BE_READ}

  RETURN VALUE
    Pointer to the found block if successful, 0 - otherwise

  NOTES.
    For the page from file positioned at filepos the function checks whether
    the page is in the key cache specified by the first parameter.
    If this is the case it immediately returns the block.
    If not, the function first chooses  a block for this page. If there is
    no not used blocks in the key cache yet, the function takes the block
    at the very beginning of the warm sub-chain. It saves the page in that
    block if it's dirty before returning the pointer to it.
    The function returns in the page_st parameter the following values:
      PAGE_READ         - if page already in the block,
      PAGE_TO_BE_READ   - if it is to be read yet by the current thread
      WAIT_TO_BE_READ   - if it is to be read by another thread
    If an error occurs THE BLOCK_ERROR bit is set in the block status.
    It might happen that there are no blocks in LRU chain (in warm part) -
    all blocks  are unlinked for some read/write operations. Then the function
    waits until first of this operations links any block back.
*/

static BLOCK_LINK *find_key_block(KEY_CACHE *keycache,
                                  File file, my_off_t filepos,
                                  int init_hits_left,
                                  int wrmode, int *page_st)
{
  HASH_LINK *hash_link;
  BLOCK_LINK *block;
  int error= 0;
  int page_status;

  DBUG_ENTER("find_key_block");
  KEYCACHE_THREAD_TRACE("find_key_block:begin");
  DBUG_PRINT("enter", ("fd: %u  pos %lu  wrmode: %lu",
                       (uint) file, (ulong) filepos, (uint) wrmode));
  KEYCACHE_DBUG_PRINT("find_key_block", ("fd: %u  pos: %lu  wrmode: %lu",
                                         (uint) file, (ulong) filepos,
                                         (uint) wrmode));
#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_keycache2",
               test_key_cache(keycache, "start of find_key_block", 0););
#endif

restart:
  /* Find the hash link for the requested page (file, filepos) */
  hash_link= get_hash_link(keycache, file, filepos);

  page_status= -1;
  if ((block= hash_link->block) &&
      block->hash_link == hash_link && (block->status & BLOCK_READ))
    page_status= PAGE_READ;

  if (wrmode && keycache->resize_in_flush)
  {
    /* This is a write request during the flush phase of a resize operation */

    if (page_status != PAGE_READ)
    {
      /* We don't need the page in the cache: we are going to write on disk */
      hash_link->requests--;
      unlink_hash(keycache, hash_link);
      return 0;
    }
    if (!(block->status & BLOCK_IN_FLUSH))
    {
      hash_link->requests--;
      /*
        Remove block to invalidate the page in the block buffer
        as we are going to write directly on disk.
        Although we have an exlusive lock for the updated key part
        the control can be yieded by the current thread as we might
        have unfinished readers of other key parts in the block
        buffer. Still we are guaranteed not to have any readers
        of the key part we are writing into until the block is
        removed from the cache as we set the BLOCL_REASSIGNED
        flag (see the code below that handles reading requests).
      */
      free_block(keycache, block);
      return 0;
    }
    /* Wait intil the page is flushed on disk */
    hash_link->requests--;
    {
      struct st_my_thread_var *thread= my_thread_var;
      add_to_queue(&block->wqueue[COND_FOR_SAVED], thread);
      do
      {
        KEYCACHE_DBUG_PRINT("find_key_block: wait",
                            ("suspend thread %ld", thread->id));
        keycache_pthread_cond_wait(&thread->suspend,
                                   &keycache->cache_lock);
      }
      while(thread->next);
    }
    /* Invalidate page in the block if it has not been done yet */
    if (block->status)
      free_block(keycache, block);
    return 0;
  }

  if (page_status == PAGE_READ &&
      (block->status & (BLOCK_IN_SWITCH | BLOCK_REASSIGNED)))
  {
    /* This is a request for a page to be removed from cache */

    KEYCACHE_DBUG_PRINT("find_key_block",
                        ("request for old page in block %u "
                         "wrmode: %d  block->status: %d",
                         BLOCK_NUMBER(block), wrmode, block->status));
    /*
       Only reading requests can proceed until the old dirty page is flushed,
       all others are to be suspended, then resubmitted
    */
    if (!wrmode && !(block->status & BLOCK_REASSIGNED))
      reg_requests(keycache, block, 1);
    else
    {
      hash_link->requests--;
      KEYCACHE_DBUG_PRINT("find_key_block",
                          ("request waiting for old page to be saved"));
      {
        struct st_my_thread_var *thread= my_thread_var;
        /* Put the request into the queue of those waiting for the old page */
        add_to_queue(&block->wqueue[COND_FOR_SAVED], thread);
        /* Wait until the request can be resubmitted */
        do
        {
          KEYCACHE_DBUG_PRINT("find_key_block: wait",
                              ("suspend thread %ld", thread->id));
          keycache_pthread_cond_wait(&thread->suspend,
                                     &keycache->cache_lock);
        }
        while(thread->next);
      }
      KEYCACHE_DBUG_PRINT("find_key_block",
                          ("request for old page resubmitted"));
      /* Resubmit the request */
      goto restart;
    }
  }
  else
  {
    /* This is a request for a new page or for a page not to be removed */
    if (! block)
    {
      /* No block is assigned for the page yet */
      if (keycache->blocks_unused)
      {
        if (keycache->free_block_list)
        {
          /* There is a block in the free list. */
          block= keycache->free_block_list;
          keycache->free_block_list= block->next_used;
          block->next_used= NULL;
        }
        else
        {
          /* There are some never used blocks, take first of them */
          block= &keycache->block_root[keycache->blocks_used];
          block->buffer= ADD_TO_PTR(keycache->block_mem,
                                    ((ulong) keycache->blocks_used*
                                     keycache->key_cache_block_size),
                                    byte*);
          keycache->blocks_used++;
        }
        keycache->blocks_unused--;
        block->status= 0;
        block->length= 0;
        block->offset= keycache->key_cache_block_size;
        block->requests= 1;
        block->temperature= BLOCK_COLD;
        block->hits_left= init_hits_left;
        block->last_hit_time= 0;
        link_to_file_list(keycache, block, file, 0);
        block->hash_link= hash_link;
        hash_link->block= block;
        page_status= PAGE_TO_BE_READ;
        KEYCACHE_DBUG_PRINT("find_key_block",
                            ("got free or never used block %u",
                             BLOCK_NUMBER(block)));
      }
      else
      {
	/* There are no never used blocks, use a block from the LRU chain */

        /*
          Wait until a new block is added to the LRU chain;
          several threads might wait here for the same page,
          all of them must get the same block
        */

        if (! keycache->used_last)
        {
          struct st_my_thread_var *thread= my_thread_var;
          thread->opt_info= (void *) hash_link;
          link_into_queue(&keycache->waiting_for_block, thread);
          do
          {
            KEYCACHE_DBUG_PRINT("find_key_block: wait",
                                ("suspend thread %ld", thread->id));
            keycache_pthread_cond_wait(&thread->suspend,
                                       &keycache->cache_lock);
          }
          while (thread->next);
          thread->opt_info= NULL;
        }
        block= hash_link->block;
        if (! block)
        {
          /*
             Take the first block from the LRU chain
             unlinking it from the chain
          */
          block= keycache->used_last->next_used;
          block->hits_left= init_hits_left;
          block->last_hit_time= 0;
          reg_requests(keycache, block,1);
          hash_link->block= block;
        }

        if (block->hash_link != hash_link &&
	    ! (block->status & BLOCK_IN_SWITCH) )
        {
	  /* this is a primary request for a new page */
          block->status|= BLOCK_IN_SWITCH;

          KEYCACHE_DBUG_PRINT("find_key_block",
                        ("got block %u for new page", BLOCK_NUMBER(block)));

          if (block->status & BLOCK_CHANGED)
          {
	    /* The block contains a dirty page - push it out of the cache */

            KEYCACHE_DBUG_PRINT("find_key_block", ("block is dirty"));

            keycache_pthread_mutex_unlock(&keycache->cache_lock);
            /*
	      The call is thread safe because only the current
	      thread might change the block->hash_link value
            */
	    error= my_pwrite(block->hash_link->file,
			     block->buffer+block->offset,
			     block->length - block->offset,
			     block->hash_link->diskpos+ block->offset,
			     MYF(MY_NABP | MY_WAIT_IF_FULL));
            keycache_pthread_mutex_lock(&keycache->cache_lock);
	    keycache->global_cache_write++;
          }

          block->status|= BLOCK_REASSIGNED;
          if (block->hash_link)
          {
            /*
	      Wait until all pending read requests
	      for this page are executed
	      (we could have avoided this waiting, if we had read
	      a page in the cache in a sweep, without yielding control)
            */
            wait_for_readers(keycache, block);

            /* Remove the hash link for this page from the hash table */
            unlink_hash(keycache, block->hash_link);
            /* All pending requests for this page must be resubmitted */
            if (block->wqueue[COND_FOR_SAVED].last_thread)
              release_queue(&block->wqueue[COND_FOR_SAVED]);
          }
          link_to_file_list(keycache, block, file,
                            (my_bool)(block->hash_link ? 1 : 0));
          block->status= error? BLOCK_ERROR : 0;
          block->length= 0;
          block->offset= keycache->key_cache_block_size;
          block->hash_link= hash_link;
          page_status= PAGE_TO_BE_READ;

          KEYCACHE_DBUG_ASSERT(block->hash_link->block == block);
          KEYCACHE_DBUG_ASSERT(hash_link->block->hash_link == hash_link);
        }
        else
        {
          /* This is for secondary requests for a new page only */
          KEYCACHE_DBUG_PRINT("find_key_block",
                              ("block->hash_link: %p  hash_link: %p  "
                               "block->status: %u", block->hash_link,
                               hash_link, block->status ));
          page_status= (((block->hash_link == hash_link) &&
                         (block->status & BLOCK_READ)) ?
                        PAGE_READ : PAGE_WAIT_TO_BE_READ);
        }
      }
      keycache->global_cache_read++;
    }
    else
    {
      reg_requests(keycache, block, 1);
      KEYCACHE_DBUG_PRINT("find_key_block",
                          ("block->hash_link: %p  hash_link: %p  "
                           "block->status: %u", block->hash_link,
                           hash_link, block->status ));
      page_status= (((block->hash_link == hash_link) &&
                     (block->status & BLOCK_READ)) ?
                    PAGE_READ : PAGE_WAIT_TO_BE_READ);
    }
  }

  KEYCACHE_DBUG_ASSERT(page_status != -1);
  *page_st=page_status;
  KEYCACHE_DBUG_PRINT("find_key_block",
                      ("fd: %u  pos %lu  block->status %u  page_status %lu",
                       (uint) file, (ulong) filepos, block->status,
                       (uint) page_status));

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_keycache2",
               test_key_cache(keycache, "end of find_key_block",0););
#endif
  KEYCACHE_THREAD_TRACE("find_key_block:end");
  DBUG_RETURN(block);
}


/*
  Read into a key cache block buffer from disk.

  SYNOPSIS

    read_block()
      keycache            pointer to a key cache data structure
      block               block to which buffer the data is to be read
      read_length         size of data to be read
      min_length          at least so much data must be read
      primary             <-> the current thread will read the data

  RETURN VALUE
    None

  NOTES.
    The function either reads a page data from file to the block buffer,
    or waits until another thread reads it. What page to read is determined
    by a block parameter - reference to a hash link for this page.
    If an error occurs THE BLOCK_ERROR bit is set in the block status.
    We do not report error when the size of successfully read
    portion is less than read_length, but not less than min_length.
*/

static void read_block(KEY_CACHE *keycache,
                       BLOCK_LINK *block, uint read_length,
                       uint min_length, my_bool primary)
{
  uint got_length;

  /* On entry cache_lock is locked */

  KEYCACHE_THREAD_TRACE("read_block");
  if (primary)
  {
    /*
      This code is executed only by threads
      that submitted primary requests
    */

    KEYCACHE_DBUG_PRINT("read_block",
                        ("page to be read by primary request"));

    /* Page is not in buffer yet, is to be read from disk */
    keycache_pthread_mutex_unlock(&keycache->cache_lock);
    /*
      Here other threads may step in and register as secondary readers.
      They will register in block->wqueue[COND_FOR_REQUESTED].
    */
    got_length= my_pread(block->hash_link->file, block->buffer,
                         read_length, block->hash_link->diskpos, MYF(0));
    keycache_pthread_mutex_lock(&keycache->cache_lock);
    if (got_length < min_length)
      block->status|= BLOCK_ERROR;
    else
    {
      block->status= BLOCK_READ;
      block->length= got_length;
    }
    KEYCACHE_DBUG_PRINT("read_block",
                        ("primary request: new page in cache"));
    /* Signal that all pending requests for this page now can be processed */
    if (block->wqueue[COND_FOR_REQUESTED].last_thread)
      release_queue(&block->wqueue[COND_FOR_REQUESTED]);
  }
  else
  {
    /*
      This code is executed only by threads
      that submitted secondary requests
    */
    KEYCACHE_DBUG_PRINT("read_block",
                      ("secondary request waiting for new page to be read"));
    {
      struct st_my_thread_var *thread= my_thread_var;
      /* Put the request into a queue and wait until it can be processed */
      add_to_queue(&block->wqueue[COND_FOR_REQUESTED], thread);
      do
      {
        KEYCACHE_DBUG_PRINT("read_block: wait",
                            ("suspend thread %ld", thread->id));
        keycache_pthread_cond_wait(&thread->suspend,
                                   &keycache->cache_lock);
      }
      while (thread->next);
    }
    KEYCACHE_DBUG_PRINT("read_block",
                        ("secondary request: new page in cache"));
  }
}


/*
  Read a block of data from a cached file into a buffer;

  SYNOPSIS

    key_cache_read()
      keycache            pointer to a key cache data structure
      file                handler for the file for the block of data to be read
      filepos             position of the block of data in the file
      level               determines the weight of the data
      buff                buffer to where the data must be placed
      length              length of the buffer
      block_length        length of the block in the key cache buffer
      return_buffer       return pointer to the key cache buffer with the data

  RETURN VALUE
    Returns address from where the data is placed if sucessful, 0 - otherwise.

  NOTES.
    The function ensures that a block of data of size length from file
    positioned at filepos is in the buffers for some key cache blocks.
    Then the function either copies the data into the buffer buff, or,
    if return_buffer is TRUE, it just returns the pointer to the key cache
    buffer with the data.
    Filepos must be a multiple of 'block_length', but it doesn't
    have to be a multiple of key_cache_block_size;
*/

byte *key_cache_read(KEY_CACHE *keycache,
                     File file, my_off_t filepos, int level,
                     byte *buff, uint length,
		     uint block_length __attribute__((unused)),
		     int return_buffer __attribute__((unused)))
{
  int error=0;
  uint offset= 0;
  byte *start= buff;
  DBUG_ENTER("key_cache_read");
  DBUG_PRINT("enter", ("fd: %u  pos: %lu  length: %u",
               (uint) file, (ulong) filepos, length));

  if (keycache->can_be_used)
  {
    /* Key cache is used */
    reg1 BLOCK_LINK *block;
    uint read_length;
    uint status;
    int page_st;

    /* Read data in key_cache_block_size increments */
    do
    {
      keycache_pthread_mutex_lock(&keycache->cache_lock);
      if (!keycache->can_be_used)
      {
	keycache_pthread_mutex_unlock(&keycache->cache_lock);
	goto no_key_cache;
      }
      offset= (uint) (filepos & (keycache->key_cache_block_size-1));
      filepos-= offset;
      read_length= length;
      set_if_smaller(read_length, keycache->key_cache_block_size-offset);
      KEYCACHE_DBUG_ASSERT(read_length > 0);

#ifndef THREAD
      if (block_length > keycache->key_cache_block_size || offset)
	return_buffer=0;
#endif

      inc_counter_for_resize_op(keycache);
      keycache->global_cache_r_requests++;
      block=find_key_block(keycache, file, filepos, level, 0, &page_st);
      if (block->status != BLOCK_ERROR && page_st != PAGE_READ)
      {
        /* The requested page is to be read into the block buffer */
        read_block(keycache, block,
                   keycache->key_cache_block_size, read_length+offset,
                   (my_bool)(page_st == PAGE_TO_BE_READ));
      }
      else if (! (block->status & BLOCK_ERROR) &&
               block->length < read_length + offset)
      {
        /*
           Impossible if nothing goes wrong:
           this could only happen if we are using a file with
           small key blocks and are trying to read outside the file
        */
        my_errno= -1;
        block->status|= BLOCK_ERROR;
      }

      if (! ((status= block->status) & BLOCK_ERROR))
      {
#ifndef THREAD
        if (! return_buffer)
#endif
        {
#if !defined(SERIALIZED_READ_FROM_CACHE)
          keycache_pthread_mutex_unlock(&keycache->cache_lock);
#endif

          /* Copy data from the cache buffer */
          if (!(read_length & 511))
            bmove512(buff, block->buffer+offset, read_length);
          else
            memcpy(buff, block->buffer+offset, (size_t) read_length);

#if !defined(SERIALIZED_READ_FROM_CACHE)
          keycache_pthread_mutex_lock(&keycache->cache_lock);
#endif
        }
      }

      remove_reader(block);
      /*
         Link the block into the LRU chain
         if it's the last submitted request for the block
      */
      unreg_request(keycache, block, 1);

      dec_counter_for_resize_op(keycache);

      keycache_pthread_mutex_unlock(&keycache->cache_lock);

      if (status & BLOCK_ERROR)
        DBUG_RETURN((byte *) 0);

#ifndef THREAD
      /* This is only true if we where able to read everything in one block */
      if (return_buffer)
	return (block->buffer);
#endif
      buff+= read_length;
      filepos+= read_length+offset;

    } while ((length-= read_length));
    DBUG_RETURN(start);
  }

no_key_cache:					/* Key cache is not used */

  /* We can't use mutex here as the key cache may not be initialized */
  keycache->global_cache_r_requests++;
  keycache->global_cache_read++;
  if (my_pread(file, (byte*) buff, length, filepos+offset, MYF(MY_NABP)))
    error= 1;
  DBUG_RETURN(error ? (byte*) 0 : start);
}


/*
  Insert a block of file data from a buffer into key cache

  SYNOPSIS
    key_cache_insert()
    keycache            pointer to a key cache data structure
    file                handler for the file to insert data from
    filepos             position of the block of data in the file to insert
    level               determines the weight of the data
    buff                buffer to read data from
    length              length of the data in the buffer

  NOTES
    This is used by MyISAM to move all blocks from a index file to the key
    cache

  RETURN VALUE
    0 if a success, 1 - otherwise.
*/

int key_cache_insert(KEY_CACHE *keycache,
                     File file, my_off_t filepos, int level,
                     byte *buff, uint length)
{
  DBUG_ENTER("key_cache_insert");
  DBUG_PRINT("enter", ("fd: %u  pos: %lu  length: %u",
               (uint) file,(ulong) filepos, length));

  if (keycache->can_be_used)
  {
    /* Key cache is used */
    reg1 BLOCK_LINK *block;
    uint read_length;
    int page_st;
    int error;

    do
    {
      uint offset;
      keycache_pthread_mutex_lock(&keycache->cache_lock);
      if (!keycache->can_be_used)
      {
	keycache_pthread_mutex_unlock(&keycache->cache_lock);
	DBUG_RETURN(0);
      }
      offset= (uint) (filepos & (keycache->key_cache_block_size-1));
      /* Read data into key cache from buff in key_cache_block_size incr. */
      filepos-= offset;
      read_length= length;
      set_if_smaller(read_length, keycache->key_cache_block_size-offset);
      KEYCACHE_DBUG_ASSERT(read_length > 0);

      inc_counter_for_resize_op(keycache);
      keycache->global_cache_r_requests++;
      block= find_key_block(keycache, file, filepos, level, 0, &page_st);
      if (block->status != BLOCK_ERROR && page_st != PAGE_READ)
      {
        /* The requested page is to be read into the block buffer */
#if !defined(SERIALIZED_READ_FROM_CACHE)
        keycache_pthread_mutex_unlock(&keycache->cache_lock);
        /*
          Here other threads may step in and register as secondary readers.
          They will register in block->wqueue[COND_FOR_REQUESTED].
        */
#endif

        /* Copy data from buff */
        if (!(read_length & 511))
          bmove512(block->buffer+offset, buff, read_length);
        else
          memcpy(block->buffer+offset, buff, (size_t) read_length);

#if !defined(SERIALIZED_READ_FROM_CACHE)
        keycache_pthread_mutex_lock(&keycache->cache_lock);
        /* Here we are alone again. */
#endif
        block->status= BLOCK_READ;
        block->length= read_length+offset;
        KEYCACHE_DBUG_PRINT("key_cache_insert",
                            ("primary request: new page in cache"));
        /* Signal that all pending requests for this now can be processed. */
        if (block->wqueue[COND_FOR_REQUESTED].last_thread)
          release_queue(&block->wqueue[COND_FOR_REQUESTED]);
      }

      remove_reader(block);
      /*
         Link the block into the LRU chain
         if it's the last submitted request for the block
      */
      unreg_request(keycache, block, 1);

      error= (block->status & BLOCK_ERROR);

      dec_counter_for_resize_op(keycache);

      keycache_pthread_mutex_unlock(&keycache->cache_lock);

      if (error)
        DBUG_RETURN(1);

      buff+= read_length;
      filepos+= read_length+offset;

    } while ((length-= read_length));
  }
  DBUG_RETURN(0);
}


/*
  Write a buffer into a cached file.

  SYNOPSIS

    key_cache_write()
      keycache            pointer to a key cache data structure
      file                handler for the file to write data to
      filepos             position in the file to write data to
      level               determines the weight of the data
      buff                buffer with the data
      length              length of the buffer
      dont_write          if is 0 then all dirty pages involved in writing
                          should have been flushed from key cache

  RETURN VALUE
    0 if a success, 1 - otherwise.

  NOTES.
    The function copies the data of size length from buff into buffers
    for key cache blocks that are  assigned to contain the portion of
    the file starting with position filepos.
    It ensures that this data is flushed to the file if dont_write is FALSE.
    Filepos must be a multiple of 'block_length', but it doesn't
    have to be a multiple of key_cache_block_size;
*/

int key_cache_write(KEY_CACHE *keycache,
                    File file, my_off_t filepos, int level,
                    byte *buff, uint length,
                    uint block_length  __attribute__((unused)),
                    int dont_write)
{
  reg1 BLOCK_LINK *block;
  int error=0;
  DBUG_ENTER("key_cache_write");
  DBUG_PRINT("enter",
	     ("fd: %u  pos: %lu  length: %u  block_length: %u  key_block_length: %u",
	      (uint) file, (ulong) filepos, length, block_length,
	      keycache ? keycache->key_cache_block_size : 0));

  if (!dont_write)
  {
    /* Force writing from buff into disk */
    keycache->global_cache_write++;
    if (my_pwrite(file, buff, length, filepos, MYF(MY_NABP | MY_WAIT_IF_FULL)))
      DBUG_RETURN(1);
  }

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_keycache",
               test_key_cache(keycache, "start of key_cache_write", 1););
#endif

  if (keycache->can_be_used)
  {
    /* Key cache is used */
    uint read_length;
    int page_st;

    do
    {
      uint offset;
      keycache_pthread_mutex_lock(&keycache->cache_lock);
      if (!keycache->can_be_used)
      {
	keycache_pthread_mutex_unlock(&keycache->cache_lock);
	goto no_key_cache;
      }
      offset= (uint) (filepos & (keycache->key_cache_block_size-1));
      /* Write data in key_cache_block_size increments */
      filepos-= offset;
      read_length= length;
      set_if_smaller(read_length, keycache->key_cache_block_size-offset);
      KEYCACHE_DBUG_ASSERT(read_length > 0);

      inc_counter_for_resize_op(keycache);
      keycache->global_cache_w_requests++;
      block= find_key_block(keycache, file, filepos, level, 1, &page_st);
      if (!block)
      {
        /* It happens only for requests submitted during resize operation */
        dec_counter_for_resize_op(keycache);
	keycache_pthread_mutex_unlock(&keycache->cache_lock);
	if (dont_write)
        {
          keycache->global_cache_w_requests++;
          keycache->global_cache_write++;
          if (my_pwrite(file, (byte*) buff, length, filepos,
		        MYF(MY_NABP | MY_WAIT_IF_FULL)))
            error=1;
	}
        goto next_block;
      }

      if (block->status != BLOCK_ERROR && page_st != PAGE_READ &&
          (offset || read_length < keycache->key_cache_block_size))
        read_block(keycache, block,
                   offset + read_length >= keycache->key_cache_block_size?
                   offset : keycache->key_cache_block_size,
                   offset,(my_bool)(page_st == PAGE_TO_BE_READ));

      if (!dont_write)
      {
	/* buff has been written to disk at start */
        if ((block->status & BLOCK_CHANGED) &&
            (!offset && read_length >= keycache->key_cache_block_size))
             link_to_file_list(keycache, block, block->hash_link->file, 1);
      }
      else if (! (block->status & BLOCK_CHANGED))
        link_to_changed_list(keycache, block);

      set_if_smaller(block->offset, offset);
      set_if_bigger(block->length, read_length+offset);

      if (! (block->status & BLOCK_ERROR))
      {
        if (!(read_length & 511))
	  bmove512(block->buffer+offset, buff, read_length);
        else
          memcpy(block->buffer+offset, buff, (size_t) read_length);
      }

      block->status|=BLOCK_READ;

      /* Unregister the request */
      block->hash_link->requests--;
      unreg_request(keycache, block, 1);

      if (block->status & BLOCK_ERROR)
      {
        keycache_pthread_mutex_unlock(&keycache->cache_lock);
        error= 1;
        break;
      }

      dec_counter_for_resize_op(keycache);

      keycache_pthread_mutex_unlock(&keycache->cache_lock);

    next_block:
      buff+= read_length;
      filepos+= read_length+offset;
      offset= 0;

    } while ((length-= read_length));
    goto end;
  }

no_key_cache:
  /* Key cache is not used */
  if (dont_write)
  {
    keycache->global_cache_w_requests++;
    keycache->global_cache_write++;
    if (my_pwrite(file, (byte*) buff, length, filepos,
		  MYF(MY_NABP | MY_WAIT_IF_FULL)))
      error=1;
  }

end:
#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("exec",
               test_key_cache(keycache, "end of key_cache_write", 1););
#endif
  DBUG_RETURN(error);
}


/*
  Free block: remove reference to it from hash table,
  remove it from the chain file of dirty/clean blocks
  and add it to the free list.
*/

static void free_block(KEY_CACHE *keycache, BLOCK_LINK *block)
{
  KEYCACHE_THREAD_TRACE("free block");
  KEYCACHE_DBUG_PRINT("free_block",
                      ("block %u to be freed, hash_link %p",
                       BLOCK_NUMBER(block), block->hash_link));
  if (block->hash_link)
  {
    /*
      While waiting for readers to finish, new readers might request the
      block. But since we set block->status|= BLOCK_REASSIGNED, they
      will wait on block->wqueue[COND_FOR_SAVED]. They must be signalled
      later.
    */
    block->status|= BLOCK_REASSIGNED;
    wait_for_readers(keycache, block);
    unlink_hash(keycache, block->hash_link);
  }

  unlink_changed(block);
  block->status= 0;
  block->length= 0;
  block->offset= keycache->key_cache_block_size;
  KEYCACHE_THREAD_TRACE("free block");
  KEYCACHE_DBUG_PRINT("free_block",
                      ("block is freed"));
  unreg_request(keycache, block, 0);
  block->hash_link= NULL;

  /* Remove the free block from the LRU ring. */
  unlink_block(keycache, block);
  if (block->temperature == BLOCK_WARM)
    keycache->warm_blocks--;
  block->temperature= BLOCK_COLD;
  /* Insert the free block in the free list. */
  block->next_used= keycache->free_block_list;
  keycache->free_block_list= block;
  /* Keep track of the number of currently unused blocks. */
  keycache->blocks_unused++;

  /* All pending requests for this page must be resubmitted. */
  if (block->wqueue[COND_FOR_SAVED].last_thread)
    release_queue(&block->wqueue[COND_FOR_SAVED]);
}


static int cmp_sec_link(BLOCK_LINK **a, BLOCK_LINK **b)
{
  return (((*a)->hash_link->diskpos < (*b)->hash_link->diskpos) ? -1 :
      ((*a)->hash_link->diskpos > (*b)->hash_link->diskpos) ? 1 : 0);
}


/*
  Flush a portion of changed blocks to disk,
  free used blocks if requested
*/

static int flush_cached_blocks(KEY_CACHE *keycache,
                               File file, BLOCK_LINK **cache,
                               BLOCK_LINK **end,
                               enum flush_type type)
{
  int error;
  int last_errno= 0;
  uint count= end-cache;

  /* Don't lock the cache during the flush */
  keycache_pthread_mutex_unlock(&keycache->cache_lock);
  /*
     As all blocks referred in 'cache' are marked by BLOCK_IN_FLUSH
     we are guarunteed no thread will change them
  */
  qsort((byte*) cache, count, sizeof(*cache), (qsort_cmp) cmp_sec_link);

  keycache_pthread_mutex_lock(&keycache->cache_lock);
  for ( ; cache != end ; cache++)
  {
    BLOCK_LINK *block= *cache;

    KEYCACHE_DBUG_PRINT("flush_cached_blocks",
                        ("block %u to be flushed", BLOCK_NUMBER(block)));
    keycache_pthread_mutex_unlock(&keycache->cache_lock);
    error= my_pwrite(file,
		     block->buffer+block->offset,
		     block->length - block->offset,
                     block->hash_link->diskpos+ block->offset,
                     MYF(MY_NABP | MY_WAIT_IF_FULL));
    keycache_pthread_mutex_lock(&keycache->cache_lock);
    keycache->global_cache_write++;
    if (error)
    {
      block->status|= BLOCK_ERROR;
      if (!last_errno)
        last_errno= errno ? errno : -1;
    }
    /*
      Let to proceed for possible waiting requests to write to the block page.
      It might happen only during an operation to resize the key cache.
    */
    if (block->wqueue[COND_FOR_SAVED].last_thread)
      release_queue(&block->wqueue[COND_FOR_SAVED]);
    /* type will never be FLUSH_IGNORE_CHANGED here */
    if (! (type == FLUSH_KEEP || type == FLUSH_FORCE_WRITE))
    {
      keycache->blocks_changed--;
      keycache->global_blocks_changed--;
      free_block(keycache, block);
    }
    else
    {
      block->status&= ~BLOCK_IN_FLUSH;
      link_to_file_list(keycache, block, file, 1);
      unreg_request(keycache, block, 1);
    }

  }
  return last_errno;
}


/*
  flush all key blocks for a file to disk, but don't do any mutex locks

    flush_key_blocks_int()
      keycache            pointer to a key cache data structure
      file                handler for the file to flush to
      flush_type          type of the flush

  NOTES
    This function doesn't do any mutex locks because it needs to be called both
    from flush_key_blocks and flush_all_key_blocks (the later one does the
    mutex lock in the resize_key_cache() function).

  RETURN
    0   ok
    1  error
*/

static int flush_key_blocks_int(KEY_CACHE *keycache,
				File file, enum flush_type type)
{
  BLOCK_LINK *cache_buff[FLUSH_CACHE],**cache;
  int last_errno= 0;
  DBUG_ENTER("flush_key_blocks_int");
  DBUG_PRINT("enter",("file: %d  blocks_used: %d  blocks_changed: %d",
              file, keycache->blocks_used, keycache->blocks_changed));

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
    DBUG_EXECUTE("check_keycache",
                 test_key_cache(keycache, "start of flush_key_blocks", 0););
#endif

  cache= cache_buff;
  if (keycache->disk_blocks > 0 &&
      (!my_disable_flush_key_blocks || type != FLUSH_KEEP))
  {
    /* Key cache exists and flush is not disabled */
    int error= 0;
    uint count= 0;
    BLOCK_LINK **pos,**end;
    BLOCK_LINK *first_in_switch= NULL;
    BLOCK_LINK *block, *next;
#if defined(KEYCACHE_DEBUG)
    uint cnt=0;
#endif

    if (type != FLUSH_IGNORE_CHANGED)
    {
      /*
         Count how many key blocks we have to cache to be able
         to flush all dirty pages with minimum seek moves
      */
      for (block= keycache->changed_blocks[FILE_HASH(file)] ;
           block ;
           block= block->next_changed)
      {
        if (block->hash_link->file == file)
        {
          count++;
          KEYCACHE_DBUG_ASSERT(count<= keycache->blocks_used);
        }
      }
      /* Allocate a new buffer only if its bigger than the one we have */
      if (count > FLUSH_CACHE &&
          !(cache= (BLOCK_LINK**) my_malloc(sizeof(BLOCK_LINK*)*count,
                                            MYF(0))))
      {
        cache= cache_buff;
        count= FLUSH_CACHE;
      }
    }

    /* Retrieve the blocks and write them to a buffer to be flushed */
restart:
    end= (pos= cache)+count;
    for (block= keycache->changed_blocks[FILE_HASH(file)] ;
         block ;
         block= next)
    {
#if defined(KEYCACHE_DEBUG)
      cnt++;
      KEYCACHE_DBUG_ASSERT(cnt <= keycache->blocks_used);
#endif
      next= block->next_changed;
      if (block->hash_link->file == file)
      {
        /*
           Mark the block with BLOCK_IN_FLUSH in order not to let
           other threads to use it for new pages and interfere with
           our sequence ot flushing dirty file pages
        */
        block->status|= BLOCK_IN_FLUSH;

        if (! (block->status & BLOCK_IN_SWITCH))
        {
	  /*
	    We care only for the blocks for which flushing was not
	    initiated by other threads as a result of page swapping
          */
          reg_requests(keycache, block, 1);
          if (type != FLUSH_IGNORE_CHANGED)
          {
	    /* It's not a temporary file */
            if (pos == end)
            {
	      /*
		This happens only if there is not enough
		memory for the big block
              */
              if ((error= flush_cached_blocks(keycache, file, cache,
                                              end,type)))
                last_errno=error;
              /*
		Restart the scan as some other thread might have changed
		the changed blocks chain: the blocks that were in switch
		state before the flush started have to be excluded
              */
              goto restart;
            }
            *pos++= block;
          }
          else
          {
            /* It's a temporary file */
            keycache->blocks_changed--;
	    keycache->global_blocks_changed--;
            free_block(keycache, block);
          }
        }
        else
        {
	  /* Link the block into a list of blocks 'in switch' */
          unlink_changed(block);
          link_changed(block, &first_in_switch);
        }
      }
    }
    if (pos != cache)
    {
      if ((error= flush_cached_blocks(keycache, file, cache, pos, type)))
        last_errno= error;
    }
    /* Wait until list of blocks in switch is empty */
    while (first_in_switch)
    {
#if defined(KEYCACHE_DEBUG)
      cnt= 0;
#endif
      block= first_in_switch;
      {
        struct st_my_thread_var *thread= my_thread_var;
        add_to_queue(&block->wqueue[COND_FOR_SAVED], thread);
        do
        {
          KEYCACHE_DBUG_PRINT("flush_key_blocks_int: wait",
                              ("suspend thread %ld", thread->id));
          keycache_pthread_cond_wait(&thread->suspend,
                                     &keycache->cache_lock);
        }
        while (thread->next);
      }
#if defined(KEYCACHE_DEBUG)
      cnt++;
      KEYCACHE_DBUG_ASSERT(cnt <= keycache->blocks_used);
#endif
    }
    /* The following happens very seldom */
    if (! (type == FLUSH_KEEP || type == FLUSH_FORCE_WRITE))
    {
#if defined(KEYCACHE_DEBUG)
      cnt=0;
#endif
      for (block= keycache->file_blocks[FILE_HASH(file)] ;
           block ;
           block= next)
      {
#if defined(KEYCACHE_DEBUG)
        cnt++;
        KEYCACHE_DBUG_ASSERT(cnt <= keycache->blocks_used);
#endif
        next= block->next_changed;
        if (block->hash_link->file == file &&
            (! (block->status & BLOCK_CHANGED)
             || type == FLUSH_IGNORE_CHANGED))
        {
          reg_requests(keycache, block, 1);
          free_block(keycache, block);
        }
      }
    }
  }

#ifndef DBUG_OFF
  DBUG_EXECUTE("check_keycache",
               test_key_cache(keycache, "end of flush_key_blocks", 0););
#endif
  if (cache != cache_buff)
    my_free((gptr) cache, MYF(0));
  if (last_errno)
    errno=last_errno;                /* Return first error */
  DBUG_RETURN(last_errno != 0);
}


/*
  Flush all blocks for a file to disk

  SYNOPSIS

    flush_key_blocks()
      keycache            pointer to a key cache data structure
      file                handler for the file to flush to
      flush_type          type of the flush

  RETURN
    0   ok
    1  error
*/

int flush_key_blocks(KEY_CACHE *keycache,
                     File file, enum flush_type type)
{
  int res;
  DBUG_ENTER("flush_key_blocks");
  DBUG_PRINT("enter", ("keycache: 0x%lx", keycache));

  if (keycache->disk_blocks <= 0)
    DBUG_RETURN(0);
  keycache_pthread_mutex_lock(&keycache->cache_lock);
  inc_counter_for_resize_op(keycache);
  res= flush_key_blocks_int(keycache, file, type);
  dec_counter_for_resize_op(keycache);
  keycache_pthread_mutex_unlock(&keycache->cache_lock);
  DBUG_RETURN(res);
}


/*
  Flush all blocks in the key cache to disk
*/

static int flush_all_key_blocks(KEY_CACHE *keycache)
{
#if defined(KEYCACHE_DEBUG)
  uint cnt=0;
#endif
  while (keycache->blocks_changed > 0)
  {
    BLOCK_LINK *block;
    for (block= keycache->used_last->next_used ; ; block=block->next_used)
    {
      if (block->hash_link)
      {
#if defined(KEYCACHE_DEBUG)
        cnt++;
        KEYCACHE_DBUG_ASSERT(cnt <= keycache->blocks_used);
#endif
        if (flush_key_blocks_int(keycache, block->hash_link->file,
				 FLUSH_RELEASE))
          return 1;
        break;
      }
      if (block == keycache->used_last)
        break;
    }
  }
  return 0;
}


/*
  Reset the counters of a key cache.

  SYNOPSIS
    reset_key_cache_counters()
    name       the name of a key cache
    key_cache  pointer to the key kache to be reset

  DESCRIPTION
   This procedure is used by process_key_caches() to reset the counters of all
   currently used key caches, both the default one and the named ones.

  RETURN
    0 on success (always because it can't fail)
*/

int reset_key_cache_counters(const char *name, KEY_CACHE *key_cache)
{
  DBUG_ENTER("reset_key_cache_counters");
  if (!key_cache->key_cache_inited)
  {
    DBUG_PRINT("info", ("Key cache %s not initialized.", name));
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("Resetting counters for key cache %s.", name));

  key_cache->global_blocks_changed= 0;   /* Key_blocks_not_flushed */
  key_cache->global_cache_r_requests= 0; /* Key_read_requests */
  key_cache->global_cache_read= 0;       /* Key_reads */
  key_cache->global_cache_w_requests= 0; /* Key_write_requests */
  key_cache->global_cache_write= 0;      /* Key_writes */
  DBUG_RETURN(0);
}


#ifndef DBUG_OFF
/*
  Test if disk-cache is ok
*/
static void test_key_cache(KEY_CACHE *keycache __attribute__((unused)),
                           const char *where __attribute__((unused)),
                           my_bool lock __attribute__((unused)))
{
  /* TODO */
}
#endif

#if defined(KEYCACHE_TIMEOUT)

#define KEYCACHE_DUMP_FILE  "keycache_dump.txt"
#define MAX_QUEUE_LEN  100


static void keycache_dump(KEY_CACHE *keycache)
{
  FILE *keycache_dump_file=fopen(KEYCACHE_DUMP_FILE, "w");
  struct st_my_thread_var *thread_var= my_thread_var;
  struct st_my_thread_var *last;
  struct st_my_thread_var *thread;
  BLOCK_LINK *block;
  HASH_LINK *hash_link;
  KEYCACHE_PAGE *page;
  uint i;

  fprintf(keycache_dump_file, "thread:%u\n", thread->id);

  i=0;
  thread=last=waiting_for_hash_link.last_thread;
  fprintf(keycache_dump_file, "queue of threads waiting for hash link\n");
  if (thread)
    do
    {
      thread=thread->next;
      page= (KEYCACHE_PAGE *) thread->opt_info;
      fprintf(keycache_dump_file,
              "thread:%u, (file,filepos)=(%u,%lu)\n",
              thread->id,(uint) page->file,(ulong) page->filepos);
      if (++i == MAX_QUEUE_LEN)
        break;
    }
    while (thread != last);

  i=0;
  thread=last=waiting_for_block.last_thread;
  fprintf(keycache_dump_file, "queue of threads waiting for block\n");
  if (thread)
    do
    {
      thread=thread->next;
      hash_link= (HASH_LINK *) thread->opt_info;
      fprintf(keycache_dump_file,
        "thread:%u hash_link:%u (file,filepos)=(%u,%lu)\n",
        thread->id, (uint) HASH_LINK_NUMBER(hash_link),
        (uint) hash_link->file,(ulong) hash_link->diskpos);
      if (++i == MAX_QUEUE_LEN)
        break;
    }
    while (thread != last);

  for (i=0 ; i< keycache->blocks_used ; i++)
  {
    int j;
    block= &keycache->block_root[i];
    hash_link= block->hash_link;
    fprintf(keycache_dump_file,
            "block:%u hash_link:%d status:%x #requests=%u waiting_for_readers:%d\n",
            i, (int) (hash_link ? HASH_LINK_NUMBER(hash_link) : -1),
            block->status, block->requests, block->condvar ? 1 : 0);
    for (j=0 ; j < 2; j++)
    {
      KEYCACHE_WQUEUE *wqueue=&block->wqueue[j];
      thread= last= wqueue->last_thread;
      fprintf(keycache_dump_file, "queue #%d\n", j);
      if (thread)
      {
        do
        {
          thread=thread->next;
          fprintf(keycache_dump_file,
                  "thread:%u\n", thread->id);
          if (++i == MAX_QUEUE_LEN)
            break;
        }
        while (thread != last);
      }
    }
  }
  fprintf(keycache_dump_file, "LRU chain:");
  block= keycache= used_last;
  if (block)
  {
    do
    {
      block= block->next_used;
      fprintf(keycache_dump_file,
              "block:%u, ", BLOCK_NUMBER(block));
    }
    while (block != keycache->used_last);
  }
  fprintf(keycache_dump_file, "\n");

  fclose(keycache_dump_file);
}

#endif /* defined(KEYCACHE_TIMEOUT) */

#if defined(KEYCACHE_TIMEOUT) && !defined(__WIN__)


static int keycache_pthread_cond_wait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex)
{
  int rc;
  struct timeval  now;            /* time when we started waiting        */
  struct timespec timeout;        /* timeout value for the wait function */
  struct timezone tz;
#if defined(KEYCACHE_DEBUG)
  int cnt=0;
#endif

  /* Get current time */
  gettimeofday(&now, &tz);
  /* Prepare timeout value */
  timeout.tv_sec= now.tv_sec + KEYCACHE_TIMEOUT;
  timeout.tv_nsec= now.tv_usec * 1000; /* timeval uses microseconds.         */
                                        /* timespec uses nanoseconds.         */
                                        /* 1 nanosecond = 1000 micro seconds. */
  KEYCACHE_THREAD_TRACE_END("started waiting");
#if defined(KEYCACHE_DEBUG)
  cnt++;
  if (cnt % 100 == 0)
    fprintf(keycache_debug_log, "waiting...\n");
    fflush(keycache_debug_log);
#endif
  rc= pthread_cond_timedwait(cond, mutex, &timeout);
  KEYCACHE_THREAD_TRACE_BEGIN("finished waiting");
#if defined(KEYCACHE_DEBUG)
  if (rc == ETIMEDOUT)
  {
    fprintf(keycache_debug_log,"aborted by keycache timeout\n");
    fclose(keycache_debug_log);
    abort();
  }
#endif

  if (rc == ETIMEDOUT)
    keycache_dump();

#if defined(KEYCACHE_DEBUG)
  KEYCACHE_DBUG_ASSERT(rc != ETIMEDOUT);
#else
  assert(rc != ETIMEDOUT);
#endif
  return rc;
}
#else
#if defined(KEYCACHE_DEBUG)
static int keycache_pthread_cond_wait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex)
{
  int rc;
  KEYCACHE_THREAD_TRACE_END("started waiting");
  rc= pthread_cond_wait(cond, mutex);
  KEYCACHE_THREAD_TRACE_BEGIN("finished waiting");
  return rc;
}
#endif
#endif /* defined(KEYCACHE_TIMEOUT) && !defined(__WIN__) */

#if defined(KEYCACHE_DEBUG)


static int keycache_pthread_mutex_lock(pthread_mutex_t *mutex)
{
  int rc;
  rc= pthread_mutex_lock(mutex);
  KEYCACHE_THREAD_TRACE_BEGIN("");
  return rc;
}


static void keycache_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  KEYCACHE_THREAD_TRACE_END("");
  pthread_mutex_unlock(mutex);
}


static int keycache_pthread_cond_signal(pthread_cond_t *cond)
{
  int rc;
  KEYCACHE_THREAD_TRACE("signal");
  rc= pthread_cond_signal(cond);
  return rc;
}


#if defined(KEYCACHE_DEBUG_LOG)


static void keycache_debug_print(const char * fmt,...)
{
  va_list args;
  va_start(args,fmt);
  if (keycache_debug_log)
  {
    VOID(vfprintf(keycache_debug_log, fmt, args));
    VOID(fputc('\n',keycache_debug_log));
  }
  va_end(args);
}
#endif /* defined(KEYCACHE_DEBUG_LOG) */

#if defined(KEYCACHE_DEBUG_LOG)


void keycache_debug_log_close(void)
{
  if (keycache_debug_log)
    fclose(keycache_debug_log);
}
#endif /* defined(KEYCACHE_DEBUG_LOG) */

#endif /* defined(KEYCACHE_DEBUG) */
