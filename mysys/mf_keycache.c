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
  These functions are to handle keyblock cacheing
  for NISAM, MISAM and PISAM databases.
  One cache can handle many files.
  It must contain buffers of the same blocksize.
  init_key_cache() should be used to init cache handler.
*/

#include "mysys_priv.h"
#include "my_static.h"
#include <m_string.h>
#include <errno.h>
#include <assert.h>
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

/* info about requests in a waiting queue */
typedef struct st_keycache_wqueue
{
  struct st_my_thread_var *last_thread;  /* circular list of waiting threads */
} KEYCACHE_WQUEUE;

/* descriptor of the page in the key cache block buffer */
typedef struct st_keycache_page
{
  int file;               /* file to which the page belongs to  */
  my_off_t filepos;       /* position of the page in the file   */
} KEYCACHE_PAGE;

/* element in the chain of a hash table bucket */
typedef struct st_hash_link
{
  struct st_hash_link *next, **prev; /* to connect links in the same bucket  */
  struct st_block_link *block;       /* reference to the block for the page: */
  File file;                         /* from such a file                     */
  my_off_t diskpos;                  /* with such an offset                  */
  uint requests;                     /* number of requests for the page      */
} HASH_LINK;            /* offset is always alighed for key_cache_block_size */

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

/* key cache block */
typedef struct st_block_link
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
  KEYCACHE_CONDVAR *condvar; /* condition variable for 'no readers' event    */
} BLOCK_LINK;

static int flush_all_key_blocks();
static void test_key_cache(const char *where, my_bool lock);

uint key_cache_block_size=       /* size of the page buffer of a cache block */
                          DEFAULT_KEYCACHE_BLOCK_SIZE;
static uint key_cache_shift;

#define CHANGED_BLOCKS_HASH 128             /* must be power of 2            */
#define FLUSH_CACHE         2000            /* sort this many blocks at once */

static KEYCACHE_WQUEUE
  waiting_for_hash_link;   /* queue of requests waiting for a free hash link */
static KEYCACHE_WQUEUE
  waiting_for_block;       /* queue of requests waiting for a free block     */

static HASH_LINK **my_hash_root; /* arr. of entries into hash table buckets */
static uint my_hash_entries;     /* max number of entries in the hash table */
static HASH_LINK *my_hash_link_root; /* memory for hash table links         */
static int my_hash_links;            /* max number of hash links            */
static int my_hash_links_used;       /* number of hash links currently used */
static HASH_LINK *my_free_hash_list; /* list of free hash links             */
static BLOCK_LINK *my_block_root;    /* memory for block links              */
static int my_disk_blocks;           /* max number of blocks in the cache   */
static byte HUGE_PTR *my_block_mem;  /* memory for block buffers            */
static BLOCK_LINK *my_used_last; /* ptr to the last block of the LRU chain  */
ulong  my_blocks_used,             /* number of currently used blocks       */
       my_blocks_changed;          /* number of currently dirty blocks      */
#if defined(KEYCACHE_DEBUG)
static
ulong  my_blocks_available; /* number of blocks available in the LRU chain  */
#endif /* defined(KEYCACHE_DEBUG) */
ulong  my_cache_w_requests, my_cache_write, /* counters                     */
       my_cache_r_requests, my_cache_read;  /* for statistics               */
static BLOCK_LINK
  *changed_blocks[CHANGED_BLOCKS_HASH]; /* hash table for file dirty blocks  */
static BLOCK_LINK
  *file_blocks[CHANGED_BLOCKS_HASH];    /* hash table for other file blocks  */
                                        /* that are not free                 */
#define KEYCACHE_HASH(f, pos)                                                 \
  (((ulong) ((pos) >> key_cache_shift)+(ulong) (f)) & (my_hash_entries-1))
#define FILE_HASH(f)                 ((uint) (f) & (CHANGED_BLOCKS_HASH-1))

#define DEFAULT_KEYCACHE_DEBUG_LOG  "keycache_debug.log"

#if defined(KEYCACHE_DEBUG) && ! defined(KEYCACHE_DEBUG_LOG)
#define KEYCACHE_DEBUG_LOG  DEFAULT_KEYCACHE_DEBUG_LOG
#endif

#if defined(KEYCACHE_DEBUG_LOG)
static FILE *keycache_debug_log=NULL;
static void keycache_debug_print _VARARGS((const char *fmt,...));
#define KEYCACHE_DEBUG_OPEN                                                   \
          keycache_debug_log=fopen(KEYCACHE_DEBUG_LOG, "w")

#define KEYCACHE_DEBUG_CLOSE                                                  \
          if (keycache_debug_log) fclose(keycache_debug_log)
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
            { struct st_my_thread_var *thread_var =my_thread_var;             \
              keycache_thread_id=my_thread_var->id;                           \
              KEYCACHE_DBUG_PRINT(l,("[thread %ld",keycache_thread_id)) }

#define KEYCACHE_THREAD_TRACE_END(l)                                          \
            KEYCACHE_DBUG_PRINT(l,("]thread %ld",keycache_thread_id))
#else
#define KEYCACHE_THREAD_TRACE_BEGIN(l)
#define KEYCACHE_THREAD_TRACE_END(l)
#define KEYCACHE_THREAD_TRACE(l)
#endif /* defined(KEYCACHE_DEBUG) || !defined(DBUG_OFF) */

#define BLOCK_NUMBER(b)                                                       \
        ((uint) (((char*)(b) - (char *) my_block_root) / sizeof(BLOCK_LINK)))
#define HASH_LINK_NUMBER(h)                                                   \
     ((uint) (((char*)(h) - (char *) my_hash_link_root) / sizeof(HASH_LINK)))

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
static int keycache_pthread_cond_broadcast(pthread_cond_t *cond);
#else
#define keycache_pthread_mutex_lock pthread_mutex_lock
#define keycache_pthread_mutex_unlock pthread_mutex_unlock
#define keycache_pthread_cond_signal pthread_cond_signal
#define keycache_pthread_cond_broadcast pthread_cond_broadcast
#endif /* defined(KEYCACHE_DEBUG) */

static uint next_power(uint value)
{
  uint old_value=1;
  while (value)
  {
    old_value=value;
    value&= value-1;
  }
  return (old_value << 1);
}


/*
  Initialize the key cache,
  return number of blocks in it
*/

int init_key_cache(ulong use_mem)
{
  uint blocks, hash_links, length;
  int error;

  DBUG_ENTER("init_key_cache");

  KEYCACHE_DEBUG_OPEN;
  if (key_cache_inited && my_disk_blocks > 0)
  {
    DBUG_PRINT("warning",("key cache already in use"));
    DBUG_RETURN(0);
  }
  if (! key_cache_inited)
  {
    key_cache_inited=TRUE;
    my_disk_blocks= -1;
    key_cache_shift=my_bit_log2(key_cache_block_size);
    DBUG_PRINT("info",("key_cache_block_size: %u",
               key_cache_block_size));
  }

  my_cache_w_requests= my_cache_r_requests= my_cache_read= my_cache_write=0;

  my_block_mem=NULL;
  my_block_root=NULL;

  blocks= (uint) (use_mem/(sizeof(BLOCK_LINK)+2*sizeof(HASH_LINK)+
                           sizeof(HASH_LINK*)*5/4+key_cache_block_size));
  /* It doesn't make sense to have too few blocks (less than 8) */
  if (blocks >= 8 && my_disk_blocks < 0)
  {
    for (;;)
    {
      /* Set my_hash_entries to the next bigger 2 power */
      if ((my_hash_entries=next_power(blocks)) < blocks*5/4)
        my_hash_entries<<=1;
      hash_links=2*blocks;
#if defined(MAX_THREADS)
      if (hash_links < MAX_THREADS + blocks - 1)
        hash_links=MAX_THREADS + blocks - 1;
#endif
      while ((length=(ALIGN_SIZE(blocks*sizeof(BLOCK_LINK))+
		      ALIGN_SIZE(hash_links*sizeof(HASH_LINK))+
		      ALIGN_SIZE(sizeof(HASH_LINK*)*my_hash_entries)))+
	     ((ulong) blocks << key_cache_shift) > use_mem)
        blocks--;
      /* Allocate memory for cache page buffers */
      if ((my_block_mem=my_malloc_lock((ulong) blocks*key_cache_block_size,
				       MYF(0))))
      {
        /*
           Allocate memory for blocks, hash_links and hash entries;
           For each block 2 hash links are allocated
        */
        if ((my_block_root=(BLOCK_LINK*) my_malloc((uint) length,MYF(0))))
          break;
        my_free_lock(my_block_mem,MYF(0));
      }
      if (blocks < 8)
      {
        my_errno=ENOMEM;
        goto err;
      }
      blocks=blocks/4*3;
    }
    my_disk_blocks=(int) blocks;
    my_hash_links=hash_links;
    my_hash_root= (HASH_LINK**) ((char*) my_block_root +
				 ALIGN_SIZE(blocks*sizeof(BLOCK_LINK)));
    my_hash_link_root= (HASH_LINK*) ((char*) my_hash_root +
				     ALIGN_SIZE((sizeof(HASH_LINK*) *
						  my_hash_entries)));
    bzero((byte*) my_block_root, my_disk_blocks*sizeof(BLOCK_LINK));
    bzero((byte*) my_hash_root, my_hash_entries*sizeof(HASH_LINK*));
    bzero((byte*) my_hash_link_root, my_hash_links*sizeof(HASH_LINK));
    my_hash_links_used=0;
    my_free_hash_list=NULL;
    my_blocks_used= my_blocks_changed=0;
#if defined(KEYCACHE_DEBUG)
    my_blocks_available=0;
#endif
    /* The LRU chain is empty after initialization */
    my_used_last=NULL;

    waiting_for_hash_link.last_thread=NULL;
    waiting_for_block.last_thread=NULL;
    DBUG_PRINT("exit",
      ("disk_blocks: %d  block_root: %lx  hash_entries: %d  hash_root: %lx  \
       hash_links: %d hash_link_root %lx",
       my_disk_blocks, my_block_root, my_hash_entries, my_hash_root,
       my_hash_links, my_hash_link_root));
  }
  bzero((gptr) changed_blocks,sizeof(changed_blocks[0])*CHANGED_BLOCKS_HASH);
  bzero((gptr) file_blocks,sizeof(file_blocks[0])*CHANGED_BLOCKS_HASH);

  DBUG_RETURN((int) blocks);

err:
  error=my_errno;
  if (my_block_mem)
    my_free_lock((gptr) my_block_mem,MYF(0));
  if (my_block_mem)
    my_free((gptr) my_block_root,MYF(0));
  my_errno=error;
  DBUG_RETURN(0);
}


/*
  Resize the key cache
*/
int resize_key_cache(ulong use_mem)
{
  int blocks;
  keycache_pthread_mutex_lock(&THR_LOCK_keycache);
  if (flush_all_key_blocks())
  {
    /* TODO: if this happens, we should write a warning in the log file ! */
    keycache_pthread_mutex_unlock(&THR_LOCK_keycache);
    return 0;
  }
  end_key_cache();
  /* the following will work even if memory is 0 */
  blocks=init_key_cache(use_mem);
  keycache_pthread_mutex_unlock(&THR_LOCK_keycache);
  return blocks;
}


/*
  Remove key_cache from memory
*/

void end_key_cache(void)
{
  DBUG_ENTER("end_key_cache");
  if (my_disk_blocks > 0)
  {
    if (my_block_mem)
    {
      my_free_lock((gptr) my_block_mem,MYF(0));
      my_free((gptr) my_block_root,MYF(0));
    }
    my_disk_blocks= -1;
  }
  KEYCACHE_DEBUG_CLOSE;
  key_cache_inited=0;
  DBUG_PRINT("status",
             ("used: %d  changed: %d  w_requests: %ld  \
              writes: %ld  r_requests: %ld  reads: %ld",
              my_blocks_used, my_blocks_changed, my_cache_w_requests,
              my_cache_write, my_cache_r_requests, my_cache_read));
  DBUG_VOID_RETURN;
} /* end_key_cache */


/*
  Link a thread into double-linked queue of waiting threads
*/

static inline void link_into_queue(KEYCACHE_WQUEUE *wqueue,
                                   struct st_my_thread_var *thread)
{
  struct st_my_thread_var *last;
  if (! (last=wqueue->last_thread))
  {
    /* Queue is empty */
    thread->next=thread;
    thread->prev=&thread->next;
  }
  else
  {
    thread->prev=last->next->prev;
    last->next->prev=&thread->next;
    thread->next=last->next;
    last->next=thread;
  }
  wqueue->last_thread=thread;
}

/*
  Unlink a thread from double-linked queue of waiting threads
*/

static inline void unlink_from_queue(KEYCACHE_WQUEUE *wqueue,
                                     struct st_my_thread_var *thread)
{
  KEYCACHE_DBUG_PRINT("unlink_from_queue", ("thread %ld", thread->id));
  if (thread->next == thread)
    /* The queue contains only one member */
    wqueue->last_thread=NULL;
  else
  {
    thread->next->prev=thread->prev;
    *thread->prev=thread->next;
    if (wqueue->last_thread == thread)
      wqueue->last_thread=STRUCT_PTR(struct st_my_thread_var, next,
                                     thread->prev);
  }
  thread->next=NULL;
}


/*
  Add a thread to single-linked queue of waiting threads
*/

static inline void add_to_queue(KEYCACHE_WQUEUE *wqueue,
                                struct st_my_thread_var *thread)
{
  struct st_my_thread_var *last;
  if (! (last=wqueue->last_thread))
    thread->next=thread;
  else
  {
    thread->next=last->next;
    last->next=thread;
  }
  wqueue->last_thread=thread;
}


/*
  Remove all threads from queue signaling them to proceed
*/

static void release_queue(KEYCACHE_WQUEUE *wqueue)
{
  struct st_my_thread_var *last=wqueue->last_thread;
  struct st_my_thread_var *next=last->next;
  struct st_my_thread_var *thread;
  do
  {
    thread=next;
    keycache_pthread_cond_signal(&thread->suspend);
    KEYCACHE_DBUG_PRINT("release_queue: signal", ("thread %ld", thread->id));
    next=thread->next;
    thread->next=NULL;
  }
  while (thread != last);
  wqueue->last_thread=NULL;
}


/*
  Unlink a block from the chain of dirty/clean blocks
*/

static inline void unlink_changed(BLOCK_LINK *block)
{
  if (block->next_changed)
    block->next_changed->prev_changed=block->prev_changed;
  *block->prev_changed=block->next_changed;
}


/*
  Link a block into the chain of dirty/clean blocks
*/

static inline void link_changed(BLOCK_LINK *block, BLOCK_LINK **phead)
{
  block->prev_changed=phead;
  if ((block->next_changed=*phead))
    (*phead)->prev_changed= &block->next_changed;
  *phead=block;
}


/*
  Unlink a block from the chain of dirty/clean blocks, if it's asked for,
  and link it to the chain of clean blocks for the specified file
*/

static void link_to_file_list(BLOCK_LINK *block,int file,
			      my_bool unlink)
{
  if (unlink)
    unlink_changed(block);
  link_changed(block,&file_blocks[FILE_HASH(file)]);
  if (block->status & BLOCK_CHANGED)
  {
    block->status&=~BLOCK_CHANGED;
    my_blocks_changed--;
  }
}


/*
  Unlink a block from the chain of clean blocks for the specified
  file and link it to the chain of dirty blocks for this file
*/

static inline void link_to_changed_list(BLOCK_LINK *block)
{
  unlink_changed(block);
  link_changed(block,&changed_blocks[FILE_HASH(block->hash_link->file)]);
  block->status|=BLOCK_CHANGED;
  my_blocks_changed++;
}


/*
  Link a block to the LRU chain at the beginning or at the end
*/

static void link_block(BLOCK_LINK *block, my_bool at_end)
{
  KEYCACHE_DBUG_ASSERT(! (block->hash_link && block->hash_link->requests));
  if (waiting_for_block.last_thread) {
    /* Signal that in the LRU chain an available block has appeared */
    struct st_my_thread_var *last_thread=waiting_for_block.last_thread;
    struct st_my_thread_var *first_thread=last_thread->next;
    struct st_my_thread_var *next_thread=first_thread;
    HASH_LINK *hash_link= (HASH_LINK *) first_thread->opt_info;
    struct st_my_thread_var *thread;
    do
    {
      thread=next_thread;
      next_thread=thread->next;
      /*
         We notify about the event all threads that ask
         for the same page as the first thread in the queue
      */
      if ((HASH_LINK *) thread->opt_info == hash_link)
      {
        keycache_pthread_cond_signal(&thread->suspend);
        unlink_from_queue(&waiting_for_block, thread);
        block->requests++;
      }
    }
    while (thread != last_thread);
    hash_link->block=block;
    KEYCACHE_THREAD_TRACE("link_block: after signaling");
#if defined(KEYCACHE_DEBUG)
    KEYCACHE_DBUG_PRINT("link_block",
        ("linked,unlinked block %u  status=%x  #requests=%u  #available=%u",
         BLOCK_NUMBER(block),block->status,
         block->requests, my_blocks_available));
#endif
    return;
  }
  if (my_used_last)
  {
    my_used_last->next_used->prev_used=&block->next_used;
    block->next_used= my_used_last->next_used;
    block->prev_used= &my_used_last->next_used;
    my_used_last->next_used=block;
    if (at_end)
      my_used_last=block;
  }
  else
  {
    /* The LRU chain is empty */
    my_used_last=block->next_used=block;
    block->prev_used=&block->next_used;
  }
  KEYCACHE_THREAD_TRACE("link_block");
#if defined(KEYCACHE_DEBUG)
  my_blocks_available++;
  KEYCACHE_DBUG_PRINT("link_block",
      ("linked block %u:%1u  status=%x  #requests=%u  #available=%u",
       BLOCK_NUMBER(block),at_end,block->status,
       block->requests, my_blocks_available));
  KEYCACHE_DBUG_ASSERT(my_blocks_available <= my_blocks_used);
#endif
}


/*
  Unlink a block from the LRU chain
*/

static void unlink_block(BLOCK_LINK *block)
{
  if (block->next_used == block)
    /* The list contains only one member */
    my_used_last=NULL;
  else
  {
    block->next_used->prev_used=block->prev_used;
    *block->prev_used=block->next_used;
    if (my_used_last == block)
      my_used_last=STRUCT_PTR(BLOCK_LINK, next_used, block->prev_used);
  }
  block->next_used=NULL;

  KEYCACHE_THREAD_TRACE("unlink_block");
#if defined(KEYCACHE_DEBUG)
  my_blocks_available--;
  KEYCACHE_DBUG_PRINT("unlink_block",
    ("unlinked block %u  status=%x   #requests=%u  #available=%u",
     BLOCK_NUMBER(block),block->status,
     block->requests, my_blocks_available));
  KEYCACHE_DBUG_ASSERT(my_blocks_available >= 0);
#endif
}


/*
  Register requests for a block
*/
static void reg_requests(BLOCK_LINK *block, int count)
{
  if (! block->requests)
    /* First request for the block unlinks it */
    unlink_block(block);
  block->requests+=count;
}


/*
  Unregister request for a block
  linking it to the LRU chain if it's the last request
*/

static inline void unreg_request(BLOCK_LINK *block, int at_end)
{
  if (! --block->requests)
    link_block(block, (my_bool)at_end);
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
static inline void wait_for_readers(BLOCK_LINK *block)
{
  struct st_my_thread_var *thread=my_thread_var;
  while (block->hash_link->requests)
  {
    block->condvar=&thread->suspend;
    keycache_pthread_cond_wait(&thread->suspend,&THR_LOCK_keycache);
    block->condvar=NULL;
  }
}


/*
  Add a hash link to a bucket in the hash_table
*/

static inline void link_hash(HASH_LINK **start, HASH_LINK *hash_link)
{
  if (*start)
    (*start)->prev=&hash_link->next;
  hash_link->next=*start;
  hash_link->prev=start;
  *start=hash_link;
}


/*
  Remove a hash link from the hash table
*/

static void unlink_hash(HASH_LINK *hash_link)
{
  KEYCACHE_DBUG_PRINT("unlink_hash", ("file %u, filepos %lu #requests=%u",
      (uint) hash_link->file,(ulong) hash_link->diskpos, hash_link->requests));
  KEYCACHE_DBUG_ASSERT(hash_link->requests == 0);
  if ((*hash_link->prev=hash_link->next))
    hash_link->next->prev=hash_link->prev;
  hash_link->block=NULL;
  if (waiting_for_hash_link.last_thread)
  {
    /* Signal that A free hash link appeared */
    struct st_my_thread_var *last_thread=waiting_for_hash_link.last_thread;
    struct st_my_thread_var *first_thread=last_thread->next;
    struct st_my_thread_var *next_thread=first_thread;
    KEYCACHE_PAGE *first_page= (KEYCACHE_PAGE *) (first_thread->opt_info);
    struct st_my_thread_var *thread;

    hash_link->file=first_page->file;
    hash_link->diskpos=first_page->filepos;
    do
    {
      KEYCACHE_PAGE *page;
      thread=next_thread;
      page= (KEYCACHE_PAGE *) thread->opt_info;
      next_thread=thread->next;
      /*
         We notify about the event all threads that ask
         for the same page as the first thread in the queue
      */
      if (page->file == hash_link->file && page->filepos == hash_link->diskpos)
      {
        keycache_pthread_cond_signal(&thread->suspend);
        unlink_from_queue(&waiting_for_hash_link, thread);
      }
    }
    while (thread != last_thread);
    link_hash(&my_hash_root[KEYCACHE_HASH(hash_link->file,
					  hash_link->diskpos)], hash_link);
    return;
  }
  hash_link->next= my_free_hash_list;
  my_free_hash_list=hash_link;
}


/*
  Get the hash link for a page
*/

static HASH_LINK *get_hash_link(int file, my_off_t filepos)
{
  reg1 HASH_LINK *hash_link, **start;
  KEYCACHE_PAGE page;
#if defined(KEYCACHE_DEBUG)
  int cnt;
#endif

  KEYCACHE_DBUG_PRINT("get_hash_link", ("file %u, filepos %lu",
                      (uint) file,(ulong) filepos));

restart:
  /*
     Find the bucket in the hash table for the pair (file, filepos);
     start contains the head of the bucket list,
     hash_link points to the first member of the list
  */
  hash_link= *(start= &my_hash_root[KEYCACHE_HASH(file, filepos)]);
#if defined(KEYCACHE_DEBUG)
  cnt=0;
#endif
  /* Look for an element for the pair (file, filepos) in the bucket chain */
  while (hash_link &&
         (hash_link->diskpos != filepos || hash_link->file != file))
  {
    hash_link= hash_link->next;
#if defined(KEYCACHE_DEBUG)
    cnt++;
    if (! (cnt <= my_hash_links_used))
    {
      int i;
      for (i=0, hash_link=*start ;
           i < cnt ; i++, hash_link=hash_link->next)
      {
        KEYCACHE_DBUG_PRINT("get_hash_link", ("file %u, filepos %lu",
            (uint) hash_link->file,(ulong) hash_link->diskpos));
      }
    }
    KEYCACHE_DBUG_ASSERT(n <= my_hash_links_used);
#endif
  }
  if (! hash_link)
  {
    /* There is no hash link in the hash table for the pair (file, filepos) */
    if (my_free_hash_list)
    {
      hash_link= my_free_hash_list;
      my_free_hash_list=hash_link->next;
    }
    else if (my_hash_links_used < my_hash_links)
    {
      hash_link= &my_hash_link_root[my_hash_links_used++];
    }
    else
    {
      /* Wait for a free hash link */
      struct st_my_thread_var *thread=my_thread_var;
      KEYCACHE_DBUG_PRINT("get_hash_link", ("waiting"));
      page.file=file; page.filepos=filepos;
      thread->opt_info= (void *) &page;
      link_into_queue(&waiting_for_hash_link, thread);
      keycache_pthread_cond_wait(&thread->suspend,&THR_LOCK_keycache);
      thread->opt_info=NULL;
      goto restart;
    }
    hash_link->file=file;
    hash_link->diskpos=filepos;
    link_hash(start, hash_link);
  }
  /* Register the request for the page */
  hash_link->requests++;

  return hash_link;
}


/*
  Get a block for the file page requested by a keycache read/write operation;
  If the page is not in the cache return a free block, if there is none
  return the lru block after saving its buffer if the page is dirty
*/

static BLOCK_LINK *find_key_block(int file, my_off_t filepos,
                                  int wrmode, int *page_st)
{
  HASH_LINK *hash_link;
  BLOCK_LINK *block;
  int error=0;
  int page_status;

  DBUG_ENTER("find_key_block");
  KEYCACHE_THREAD_TRACE("find_key_block:begin");
  DBUG_PRINT("enter", ("file %u, filepos %lu, wrmode %lu",
               (uint) file,(ulong) filepos,(uint) wrmode));
  KEYCACHE_DBUG_PRINT("find_key_block", ("file %u, filepos %lu, wrmode %lu",
                      (uint) file,(ulong) filepos,(uint) wrmode));
#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_keycache2",test_key_cache("start of find_key_block",0););
#endif

restart:
  /* Find the hash link for the requested page (file, filepos) */
  hash_link=get_hash_link(file, filepos);

  page_status=-1;
  if ((block=hash_link->block) &&
      block->hash_link == hash_link && (block->status & BLOCK_READ))
    page_status=PAGE_READ;

  if (page_status == PAGE_READ && (block->status & BLOCK_IN_SWITCH))
  {
    /* This is a request for a page to be removed from cache */
    KEYCACHE_DBUG_PRINT("find_key_block",
             ("request for old page in block %u",BLOCK_NUMBER(block)));
    /*
       Only reading requests can proceed until the old dirty page is flushed,
       all others are to be suspended, then resubmitted
    */
    if (!wrmode && !(block->status & BLOCK_REASSIGNED))
      reg_requests(block,1);
    else
    {
      hash_link->requests--;
      KEYCACHE_DBUG_PRINT("find_key_block",
                          ("request waiting for old page to be saved"));
      {
        struct st_my_thread_var *thread=my_thread_var;
        /* Put the request into the queue of those waiting for the old page */
        add_to_queue(&block->wqueue[COND_FOR_SAVED], thread);
        /* Wait until the request can be resubmitted */
        do
        {
          keycache_pthread_cond_wait(&thread->suspend, &THR_LOCK_keycache);
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
      if (my_blocks_used < (uint) my_disk_blocks)
      {
	/* There are some never used blocks, take first of them */
        hash_link->block=block= &my_block_root[my_blocks_used];
        block->buffer=ADD_TO_PTR(my_block_mem,
                             ((ulong) my_blocks_used*key_cache_block_size),
                             byte*);
        block->status=0;
        block->length=0;
        block->offset=key_cache_block_size;
        block->requests=1;
        my_blocks_used++;
        link_to_file_list(block, file, 0);
        block->hash_link=hash_link;
        page_status=PAGE_TO_BE_READ;
        KEYCACHE_DBUG_PRINT("find_key_block",
                            ("got never used block %u",BLOCK_NUMBER(block)));
      }
      else
      {
	/* There are no never used blocks, use a block from the LRU chain */
        /*
           Wait until a new block is added to the LRU chain;
           several threads might wait here for the same page,
           all of them must get the same block
        */

        if (! my_used_last)
        {
          struct st_my_thread_var *thread=my_thread_var;
          thread->opt_info=(void *) hash_link;
          link_into_queue(&waiting_for_block, thread);
          do
          {
            keycache_pthread_cond_wait(&thread->suspend,&THR_LOCK_keycache);
          }
          while (thread->next);
          thread->opt_info=NULL;
        }
        block=hash_link->block;
        if (! block)
        {
          /*
             Take the first block from the LRU chain
             unlinking it from the chain
          */
          block= my_used_last->next_used;
          reg_requests(block,1);
          hash_link->block=block;
        }

        if (block->hash_link != hash_link &&
	    ! (block->status & BLOCK_IN_SWITCH) )
        {
	  /* this is a primary request for a new page */
          block->status|=BLOCK_IN_SWITCH;

          KEYCACHE_DBUG_PRINT("find_key_block",
                        ("got block %u for new page",BLOCK_NUMBER(block)));

          if (block->status & BLOCK_CHANGED)
          {
	    /* The block contains a dirty page - push it out of the cache */

            KEYCACHE_DBUG_PRINT("find_key_block",("block is dirty"));

            keycache_pthread_mutex_unlock(&THR_LOCK_keycache);
            /*
	      The call is thread safe because only the current
	      thread might change the block->hash_link value
            */
            error=my_pwrite(block->hash_link->file,block->buffer,
                            block->length,block->hash_link->diskpos,
                            MYF(MY_NABP | MY_WAIT_IF_FULL));
            keycache_pthread_mutex_lock(&THR_LOCK_keycache);
            my_cache_write++;
          }

          block->status|=BLOCK_REASSIGNED;
          if (block->hash_link)
          {
            /*
	      Wait until all pending read requests
	      for this page are executed
	      (we could have avoided this waiting, if we had read
	      a page in the cache in a sweep, without yielding control)
            */
            wait_for_readers(block);

            /* Remove the hash link for this page from the hash table */
            unlink_hash(block->hash_link);
            /* All pending requests for this page must be resubmitted */
            if (block->wqueue[COND_FOR_SAVED].last_thread)
              release_queue(&block->wqueue[COND_FOR_SAVED]);
          }
          link_to_file_list(block, file, (my_bool)(block->hash_link ? 1 : 0));
          block->status=error? BLOCK_ERROR : 0;
          block->length=0;
          block->offset=key_cache_block_size;
          block->hash_link=hash_link;
          page_status=PAGE_TO_BE_READ;

          KEYCACHE_DBUG_ASSERT(block->hash_link->block == block);
          KEYCACHE_DBUG_ASSERT(hash_link->block->hash_link == hash_link);
        }
        else
        {
          /* This is for secondary requests for a new page only */
            page_status = block->hash_link == hash_link &&
                        (block->status & BLOCK_READ) ?
                          PAGE_READ : PAGE_WAIT_TO_BE_READ;
        }
      }

      my_cache_read++;
    }
    else
    {
      reg_requests(block,1);
      page_status = block->hash_link == hash_link &&
                    (block->status & BLOCK_READ) ?
                      PAGE_READ : PAGE_WAIT_TO_BE_READ;
    }
  }

  KEYCACHE_DBUG_ASSERT(page_status != -1);
  *page_st=page_status;

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_keycache2",test_key_cache("end of find_key_block",0););
#endif
  KEYCACHE_THREAD_TRACE("find_key_block:end");
  DBUG_RETURN(block);
}


/*
  Read into a key cache block buffer from disk;
  do not to report error when the size of successfully read
  portion is less than read_length, but not less than min_length
*/

static void read_block(BLOCK_LINK *block, uint read_length,
                       uint min_length, my_bool primary)
{
  uint got_length;

  /* On entry THR_LOCK_keycache is locked */

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
    keycache_pthread_mutex_unlock(&THR_LOCK_keycache);
    got_length=my_pread(block->hash_link->file,block->buffer,
                        read_length,block->hash_link->diskpos,MYF(0));
    keycache_pthread_mutex_lock(&THR_LOCK_keycache);
    if (got_length < min_length)
      block->status|=BLOCK_ERROR;
    else
    {
      block->status=BLOCK_READ;
      block->length=got_length;
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
      struct st_my_thread_var *thread=my_thread_var;
      /* Put the request into a queue and wait until it can be processed */
      add_to_queue(&block->wqueue[COND_FOR_REQUESTED],thread);
      do
      {
        keycache_pthread_cond_wait(&thread->suspend,&THR_LOCK_keycache);
      }
      while (thread->next);
    }
    KEYCACHE_DBUG_PRINT("read_block",
                        ("secondary request: new page in cache"));
  }
}


/*
  Read a block of data from a cached file into a buffer;
  if return_buffer is set then the cache buffer is returned if
  it can be used;
  filepos must be a multiple of 'block_length', but it doesn't
  have to be a multiple of key_cache_block_size;
  returns adress from where data is read
*/

byte *key_cache_read(File file, my_off_t filepos, byte *buff, uint length,
		     uint block_length __attribute__((unused)),
		     int return_buffer __attribute__((unused)))
{
  int error=0;
  DBUG_ENTER("key_cache_read");
  DBUG_PRINT("enter", ("file %u, filepos %lu, length %u",
               (uint) file,(ulong) filepos,length));

  if (my_disk_blocks > 0)
  {
    /* Key cache is used */
    reg1 BLOCK_LINK *block;
    uint offset= (uint) (filepos & (key_cache_block_size-1));
    byte *start=buff;
    uint read_length;
    uint status;
    int page_st;

#ifndef THREAD
    if (block_length > key_cache_block_size || offset)
      return_buffer=0;
#endif

    /* Read data in key_cache_block_size increments */
    filepos-= offset;
    do
    {
      read_length= length > key_cache_block_size ?
                   key_cache_block_size : length;
      KEYCACHE_DBUG_ASSERT(read_length > 0);
      keycache_pthread_mutex_lock(&THR_LOCK_keycache);
      my_cache_r_requests++;
      block=find_key_block(file,filepos,0,&page_st);
      if (page_st != PAGE_READ)
      {
        /* The requested page is to be read into the block buffer */
        read_block(block,key_cache_block_size,read_length+offset,
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
        my_errno=-1;
        block->status|=BLOCK_ERROR;
      }

      if (! ((status=block->status) & BLOCK_ERROR))
      {
#ifndef THREAD
        if (! return_buffer)
#endif
        {
#if !defined(SERIALIZED_READ_FROM_CACHE)
          keycache_pthread_mutex_unlock(&THR_LOCK_keycache);
#endif

          /* Copy data from the cache buffer */
          if (!(read_length & 511))
            bmove512(buff,block->buffer+offset,read_length);
          else
            memcpy(buff,block->buffer+offset,(size_t) read_length);

#if !defined(SERIALIZED_READ_FROM_CACHE)
          keycache_pthread_mutex_lock(&THR_LOCK_keycache);
#endif
        }
      }

      remove_reader(block);
      /*
         Link the block into the LRU chain
         if it's the last submitted request for the block
      */
      unreg_request(block,1);

      keycache_pthread_mutex_unlock(&THR_LOCK_keycache);

      if (status & BLOCK_ERROR)
        DBUG_RETURN((byte *) 0);

#ifndef THREAD
      if (return_buffer)
          return (block->buffer);
#endif

      buff+=read_length;
      filepos+=read_length;
      offset=0;

    } while ((length-= read_length));
    DBUG_RETURN(start);
  }

  /* Key cache is not used */
  statistic_increment(my_cache_r_requests,&THR_LOCK_keycache);
  statistic_increment(my_cache_read,&THR_LOCK_keycache);
  if (my_pread(file,(byte*) buff,length,filepos,MYF(MY_NABP)))
    error=1;
  DBUG_RETURN(error? (byte*) 0 : buff);
}


/*
  Write a buffer into disk;
  filepos must be a multiple of 'block_length', but it doesn't
  have to be a multiple of key cache block size;
  if !dont_write then all dirty pages involved in writing should
  have been flushed from key cache before the function starts
*/

int key_cache_write(File file, my_off_t filepos, byte *buff, uint length,
                    uint block_length  __attribute__((unused)),
                    int dont_write)
{
  reg1 BLOCK_LINK *block;
  int error=0;

  DBUG_ENTER("key_cache_write");
  DBUG_PRINT("enter", ("file %u, filepos %lu, length %u block_length %u",
               (uint) file,(ulong) filepos,length,block_length));

  if (!dont_write)
  {
    /* Force writing from buff into disk */
    statistic_increment(my_cache_write, &THR_LOCK_keycache);
    if (my_pwrite(file,buff,length,filepos,MYF(MY_NABP | MY_WAIT_IF_FULL)))
      DBUG_RETURN(1);
  }

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_keycache",test_key_cache("start of key_cache_write",1););
#endif

  if (my_disk_blocks > 0)
  {
    /* Key cache is used */
    uint read_length;
    uint offset= (uint) (filepos & (key_cache_block_size-1));
    int page_st;

    /* Write data in key_cache_block_size increments */
    filepos-= offset;
    do
    {
      read_length= length > key_cache_block_size ?
                   key_cache_block_size : length;
      KEYCACHE_DBUG_ASSERT(read_length > 0);
      keycache_pthread_mutex_lock(&THR_LOCK_keycache);
      my_cache_w_requests++;
      block=find_key_block(file, filepos, 1, &page_st);
      if (page_st != PAGE_READ &&
          (offset || read_length < key_cache_block_size))
        read_block(block,
                   offset + read_length >= key_cache_block_size?
                   offset : key_cache_block_size,
                   offset,(my_bool)(page_st == PAGE_TO_BE_READ));

      if (!dont_write)
      {
	/* buff has been written to disk at start */
        if ((block->status & BLOCK_CHANGED) &&
            (!offset && read_length >= key_cache_block_size))
             link_to_file_list(block, block->hash_link->file, 1);
      }
      else if (! (block->status & BLOCK_CHANGED))
        link_to_changed_list(block);

      set_if_smaller(block->offset,offset)
      set_if_bigger(block->length,read_length+offset);

      if (! (block->status & BLOCK_ERROR))
      {
        if (!(read_length & 511))
             bmove512(block->buffer+offset,buff,read_length);
        else
          memcpy(block->buffer+offset,buff,(size_t) read_length);
      }

      block->status|=BLOCK_READ;

      /* Unregister the request */
      block->hash_link->requests--;
      unreg_request(block,1);

      if (block->status & BLOCK_ERROR)
      {
        keycache_pthread_mutex_unlock(&THR_LOCK_keycache);
        error=1;
        break;
      }

      keycache_pthread_mutex_unlock(&THR_LOCK_keycache);

      buff+=read_length;
      filepos+=read_length;
      offset=0;

    } while ((length-= read_length));
  }
  else
  {
    /* Key cache is not used */
    if (dont_write)
    {
      statistic_increment(my_cache_w_requests, &THR_LOCK_keycache);
      statistic_increment(my_cache_write, &THR_LOCK_keycache);
      if (my_pwrite(file,(byte*) buff,length,filepos,MYF(MY_NABP | MY_WAIT_IF_FULL)))
        error=1;
    }
  }

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("exec",test_key_cache("end of key_cache_write",1););
#endif
  DBUG_RETURN(error);
}


/*
  Free block: remove reference to it from hash table,
  remove it from the chain file of dirty/clean blocks
  and add it at the beginning of the LRU chain
*/

static void free_block(BLOCK_LINK *block)
{
  KEYCACHE_THREAD_TRACE("free block");
  KEYCACHE_DBUG_PRINT("free_block",
                      ("block %u to be freed",BLOCK_NUMBER(block)));
  if (block->hash_link)
  {
    block->status|=BLOCK_REASSIGNED;
    wait_for_readers(block);
    unlink_hash(block->hash_link);
  }

  unlink_changed(block);
  block->status=0;
  block->length=0;
  block->offset=key_cache_block_size;
  KEYCACHE_THREAD_TRACE("free block");
  KEYCACHE_DBUG_PRINT("free_block",
                      ("block is freed"));
  unreg_request(block,0);
  block->hash_link=NULL;
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

static int flush_cached_blocks(File file, BLOCK_LINK **cache,
                               BLOCK_LINK **end,
                               enum flush_type type)
{
  int error;
  int last_errno=0;
  uint count=end-cache;

  /* Don't lock the cache during the flush */
  keycache_pthread_mutex_unlock(&THR_LOCK_keycache);
  /*
     As all blocks referred in 'cache' are marked by BLOCK_IN_FLUSH
     we are guarunteed no thread will change them
  */
  qsort((byte*) cache,count,sizeof(*cache),(qsort_cmp) cmp_sec_link);

  keycache_pthread_mutex_lock(&THR_LOCK_keycache);
  for ( ; cache != end ; cache++)
  {
    BLOCK_LINK *block= *cache;

    KEYCACHE_DBUG_PRINT("flush_cached_blocks",
                        ("block %u to be flushed", BLOCK_NUMBER(block)));
    keycache_pthread_mutex_unlock(&THR_LOCK_keycache);
    error=my_pwrite(file,block->buffer+block->offset,block->length,
                    block->hash_link->diskpos,MYF(MY_NABP | MY_WAIT_IF_FULL));
    keycache_pthread_mutex_lock(&THR_LOCK_keycache);
    my_cache_write++;
    if (error)
    {
      block->status|= BLOCK_ERROR;
      if (!last_errno)
        last_errno=errno ? errno : -1;
    }
    /* type will never be FLUSH_IGNORE_CHANGED here */
    if (! (type == FLUSH_KEEP || type == FLUSH_FORCE_WRITE))
    {
      my_blocks_changed--;
      free_block(block);
    }
    else
    {
      block->status&=~BLOCK_IN_FLUSH;
      link_to_file_list(block,file,1);
      unreg_request(block,1);
    }

  }
  return last_errno;
}


/*
  Flush all blocks for a file to disk
*/

int flush_key_blocks(File file, enum flush_type type)
{
  int last_errno=0;
  BLOCK_LINK *cache_buff[FLUSH_CACHE],**cache;
  DBUG_ENTER("flush_key_blocks");
  DBUG_PRINT("enter",("file: %d  blocks_used: %d  blocks_changed: %d",
              file, my_blocks_used, my_blocks_changed));

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
    DBUG_EXECUTE("check_keycache",test_key_cache("start of flush_key_blocks",0););
#endif

  keycache_pthread_mutex_lock(&THR_LOCK_keycache);

  cache=cache_buff;
  if (my_disk_blocks > 0 &&
      (!my_disable_flush_key_blocks || type != FLUSH_KEEP))
  {
    /* Key cache exists and flush is not disabled */
    int error=0;
    uint count=0;
    BLOCK_LINK **pos,**end;
    BLOCK_LINK *first_in_switch=NULL;
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
      for (block=changed_blocks[FILE_HASH(file)] ;
           block ;
           block=block->next_changed)
      {
        if (block->hash_link->file == file)
        {
          count++;
          KEYCACHE_DBUG_ASSERT(count<= my_blocks_used);
        }
      }
      /* Allocate a new buffer only if its bigger than the one we have */
      if (count > FLUSH_CACHE &&
          !(cache=(BLOCK_LINK**) my_malloc(sizeof(BLOCK_LINK*)*count,MYF(0))))
      {
        cache=cache_buff;
        count=FLUSH_CACHE;
      }
    }

    /* Retrieve the blocks and write them to a buffer to be flushed */
restart:
    end=(pos=cache)+count;
    for (block=changed_blocks[FILE_HASH(file)] ;
         block ;
         block=next)
    {
#if defined(KEYCACHE_DEBUG)
      cnt++;
      KEYCACHE_DBUG_ASSERT(cnt <= my_blocks_used);
#endif
      next=block->next_changed;
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
          reg_requests(block,1);
          if (type != FLUSH_IGNORE_CHANGED)
          {
	    /* It's not a temporary file */
            if (pos == end)
            {
	      /*
		This happens only if there is not enough
		memory for the big block
              */
              if ((error=flush_cached_blocks(file,cache,end,type)))
                last_errno=error;
              /*
		Restart the scan as some other thread might have changed
		the changed blocks chain: the blocks that were in switch
		state before the flush started have to be excluded
              */
              goto restart;
            }
            *pos++=block;
          }
          else
          {
            /* It's a temporary file */
            my_blocks_changed--;
            free_block(block);
          }
        }
        else
        {
	  /* Link the block into a list of blocks 'in switch' */
          unlink_changed(block);
          link_changed(block,&first_in_switch);
        }
      }
    }
    if (pos != cache)
    {
      if ((error=flush_cached_blocks(file,cache,pos,type)))
        last_errno=error;
    }
    /* Wait until list of blocks in switch is empty */
    while (first_in_switch)
    {
#if defined(KEYCACHE_DEBUG)
      cnt=0;
#endif
      block=first_in_switch;
      {
        struct st_my_thread_var *thread=my_thread_var;
        add_to_queue(&block->wqueue[COND_FOR_SAVED], thread);
        do
        {
          keycache_pthread_cond_wait(&thread->suspend,&THR_LOCK_keycache);
        }
        while (thread->next);
      }
#if defined(KEYCACHE_DEBUG)
      cnt++;
      KEYCACHE_DBUG_ASSERT(cnt <= my_blocks_used);
#endif
    }
    /* The following happens very seldom */
    if (! (type == FLUSH_KEEP || type == FLUSH_FORCE_WRITE))
    {
#if defined(KEYCACHE_DEBUG)
      cnt=0;
#endif
      for (block=file_blocks[FILE_HASH(file)] ;
           block ;
           block=next)
      {
#if defined(KEYCACHE_DEBUG)
        cnt++;
        KEYCACHE_DBUG_ASSERT(cnt <= my_blocks_used);
#endif
        next=block->next_changed;
        if (block->hash_link->file == file &&
            (! (block->status & BLOCK_CHANGED)
             || type == FLUSH_IGNORE_CHANGED))
        {
          reg_requests(block,1);
          free_block(block);
        }
      }
    }
  }

  keycache_pthread_mutex_unlock(&THR_LOCK_keycache);

#ifndef DBUG_OFF
    DBUG_EXECUTE("check_keycache",
                 test_key_cache("end of flush_key_blocks",0););
#endif
  if (cache != cache_buff)
    my_free((gptr) cache,MYF(0));
  if (last_errno)
    errno=last_errno;                /* Return first error */
  DBUG_RETURN(last_errno != 0);
}


/*
  Flush all blocks in the key cache to disk
*/

static int flush_all_key_blocks()
{
#if defined(KEYCACHE_DEBUG)
  uint cnt=0;
#endif
  while (my_blocks_changed > 0)
  {
    BLOCK_LINK *block;
    for (block= my_used_last->next_used ; ; block=block->next_used)
    {
      if (block->hash_link)
      {
#if defined(KEYCACHE_DEBUG)
        cnt++;
        KEYCACHE_DBUG_ASSERT(cnt <= my_blocks_used);
#endif
        if (flush_key_blocks(block->hash_link->file, FLUSH_RELEASE))
          return 1;
        break;
      }
      if (block == my_used_last)
        break;
    }
  }
  return 0;
}


#ifndef DBUG_OFF
/*
  Test if disk-cache is ok
*/
static void test_key_cache(const char *where __attribute__((unused)),
                           my_bool lock __attribute__((unused)))
{
  /* TODO */
}
#endif

#if defined(KEYCACHE_TIMEOUT)

#define KEYCACHE_DUMP_FILE  "keycache_dump.txt"
#define MAX_QUEUE_LEN  100


static void keycache_dump()
{
  FILE *keycache_dump_file=fopen(KEYCACHE_DUMP_FILE, "w");
  struct st_my_thread_var *thread_var =my_thread_var;
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

  for (i=0 ; i< my_blocks_used ; i++)
  {
    int j;
    block= &my_block_root[i];
    hash_link= block->hash_link;
    fprintf(keycache_dump_file,
            "block:%u hash_link:%d status:%x #requests=%u waiting_for_readers:%d\n",
            i, (int) (hash_link ? HASH_LINK_NUMBER(hash_link) : -1),
            block->status, block->requests, block->condvar ? 1 : 0);
    for (j=0 ; j < 2; j++)
    {
      KEYCACHE_WQUEUE *wqueue=&block->wqueue[j];
      thread=last=wqueue->last_thread;
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
  block= my_used_last;
  if (block)
  {
    do
    {
      block=block->next_used;
      fprintf(keycache_dump_file,
              "block:%u, ", BLOCK_NUMBER(block));
    }
    while (block != my_used_last);
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
  timeout.tv_sec = now.tv_sec + KEYCACHE_TIMEOUT;
  timeout.tv_nsec = now.tv_usec * 1000; /* timeval uses microseconds.         */
                                        /* timespec uses nanoseconds.         */
                                        /* 1 nanosecond = 1000 micro seconds. */
  KEYCACHE_THREAD_TRACE_END("started waiting");
#if defined(KEYCACHE_DEBUG)
  cnt++;
  if (cnt % 100 == 0)
    fprintf(keycache_debug_log, "waiting...\n");
    fflush(keycache_debug_log);
#endif
  rc = pthread_cond_timedwait(cond, mutex, &timeout);
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
  rc = pthread_cond_wait(cond, mutex);
  KEYCACHE_THREAD_TRACE_BEGIN("finished waiting");
  return rc;
}
#endif
#endif /* defined(KEYCACHE_TIMEOUT) && !defined(__WIN__) */

#if defined(KEYCACHE_DEBUG)


static int keycache_pthread_mutex_lock(pthread_mutex_t *mutex)
{
  int rc;
  rc=pthread_mutex_lock(mutex);
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
  rc=pthread_cond_signal(cond);
  return rc;
}


static int keycache_pthread_cond_broadcast(pthread_cond_t *cond)
{
  int rc;
  KEYCACHE_THREAD_TRACE("signal");
  rc=pthread_cond_broadcast(cond);
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
