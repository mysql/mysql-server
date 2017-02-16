/* Copyright (c) 2000, 2013, Oracle and/or its affiliates.

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
  @file 
  The file contains the following modules:

    Simple Key Cache Module

    Partitioned Key Cache Module

    Key Cache Interface Module
     
*/

#include "mysys_priv.h"
#include "mysys_err.h"
#include <keycache.h>
#include "my_static.h"
#include <m_string.h>
#include <my_bit.h>
#include <errno.h>
#include <stdarg.h>
#include "probes_mysql.h"

/****************************************************************************** 
  Simple Key Cache Module

  The module contains implementations of all key cache interface functions
  employed by partitioned key caches. 
     
******************************************************************************/

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

  Key Cache Locking
  =================

  All key cache locking is done with a single mutex per key cache:
  keycache->cache_lock. This mutex is locked almost all the time
  when executing code in this file (mf_keycache.c).
  However it is released for I/O and some copy operations.

  The cache_lock is also released when waiting for some event. Waiting
  and signalling is done via condition variables. In most cases the
  thread waits on its thread->suspend condition variable. Every thread
  has a my_thread_var structure, which contains this variable and a
  '*next' and '**prev' pointer. These pointers are used to insert the
  thread into a wait queue.

  A thread can wait for one block and thus be in one wait queue at a
  time only.

  Before starting to wait on its condition variable with
  mysql_cond_wait(), the thread enters itself to a specific wait queue
  with link_into_queue() (double linked with '*next' + '**prev') or
  wait_on_queue() (single linked with '*next').

  Another thread, when releasing a resource, looks up the waiting thread
  in the related wait queue. It sends a signal with
  mysql_cond_signal() to the waiting thread.

  NOTE: Depending on the particular wait situation, either the sending
  thread removes the waiting thread from the wait queue with
  unlink_from_queue() or release_whole_queue() respectively, or the waiting
  thread removes itself.

  There is one exception from this locking scheme when one thread wants
  to reuse a block for some other address. This works by first marking
  the block reserved (status= BLOCK_IN_SWITCH) and then waiting for all
  threads that are reading the block to finish. Each block has a
  reference to a condition variable (condvar). It holds a reference to
  the thread->suspend condition variable for the waiting thread (if such
  a thread exists). When that thread is signaled, the reference is
  cleared. The number of readers of a block is registered in
  block->hash_link->requests. See wait_for_readers() / remove_reader()
  for details. This is similar to the above, but it clearly means that
  only one thread can wait for a particular block. There is no queue in
  this case. Strangely enough block->convar is used for waiting for the
  assigned hash_link only. More precisely it is used to wait for all
  requests to be unregistered from the assigned hash_link.

  The resize_queue serves two purposes:
  1. Threads that want to do a resize wait there if in_resize is set.
     This is not used in the server. The server refuses a second resize
     request if one is already active. keycache->in_init is used for the
     synchronization. See set_var.cc.
  2. Threads that want to access blocks during resize wait here during
     the re-initialization phase.
  When the resize is done, all threads on the queue are signalled.
  Hypothetical resizers can compete for resizing, and read/write
  requests will restart to request blocks from the freshly resized
  cache. If the cache has been resized too small, it is disabled and
  'can_be_used' is false. In this case read/write requests bypass the
  cache. Since they increment and decrement 'cnt_for_resize_op', the
  next resizer can wait on the queue 'waiting_for_resize_cnt' until all
  I/O finished.
*/

/* declare structures that is used by st_key_cache */

struct st_block_link;
typedef struct st_block_link BLOCK_LINK;
struct st_keycache_page;
typedef struct st_keycache_page KEYCACHE_PAGE;
struct st_hash_link;
typedef struct st_hash_link HASH_LINK;

/* info about requests in a waiting queue */
typedef struct st_keycache_wqueue
{
  struct st_my_thread_var *last_thread;  /* circular list of waiting threads */
} KEYCACHE_WQUEUE;

#define CHANGED_BLOCKS_HASH 128             /* must be power of 2 */

/* Control block for a simple (non-partitioned) key cache */

typedef struct st_simple_key_cache_cb
{
  my_bool key_cache_inited;      /* <=> control block is allocated           */
  my_bool in_resize;             /* true during resize operation             */
  my_bool resize_in_flush;       /* true during flush of resize operation    */
  my_bool can_be_used;           /* usage of cache for read/write is allowed */
  size_t key_cache_mem_size;     /* specified size of the cache memory       */
  uint key_cache_block_size;     /* size of the page buffer of a cache block */
  ulong min_warm_blocks;         /* min number of warm blocks;               */
  ulong age_threshold;           /* age threshold for hot blocks             */
  ulonglong keycache_time;       /* total number of block link operations    */
  uint hash_entries;             /* max number of entries in the hash table  */
  int hash_links;                /* max number of hash links                 */
  int hash_links_used;           /* number of hash links currently used      */
  int disk_blocks;               /* max number of blocks in the cache        */
  ulong blocks_used;           /* maximum number of concurrently used blocks */
  ulong blocks_unused;           /* number of currently unused blocks        */
  ulong blocks_changed;          /* number of currently dirty blocks         */
  ulong warm_blocks;             /* number of blocks in warm sub-chain       */
  ulong cnt_for_resize_op;       /* counter to block resize operation        */
  long blocks_available;      /* number of blocks available in the LRU chain */
  HASH_LINK **hash_root;         /* arr. of entries into hash table buckets  */
  HASH_LINK *hash_link_root;     /* memory for hash table links              */
  HASH_LINK *free_hash_list;     /* list of free hash links                  */
  BLOCK_LINK *free_block_list;   /* list of free blocks                      */
  BLOCK_LINK *block_root;        /* memory for block links                   */
  uchar *block_mem;              /* memory for block buffers                 */
  BLOCK_LINK *used_last;         /* ptr to the last block of the LRU chain   */
  BLOCK_LINK *used_ins;          /* ptr to the insertion block in LRU chain  */
  mysql_mutex_t cache_lock;      /* to lock access to the cache structure    */
  KEYCACHE_WQUEUE resize_queue;  /* threads waiting during resize operation  */
  /*
    Waiting for a zero resize count. Using a queue for symmetry though
    only one thread can wait here.
  */
  KEYCACHE_WQUEUE waiting_for_resize_cnt;
  KEYCACHE_WQUEUE waiting_for_hash_link; /* waiting for a free hash link     */
  KEYCACHE_WQUEUE waiting_for_block;    /* requests waiting for a free block */
  BLOCK_LINK *changed_blocks[CHANGED_BLOCKS_HASH]; /* hash for dirty file bl.*/
  BLOCK_LINK *file_blocks[CHANGED_BLOCKS_HASH];    /* hash for other file bl.*/

  /* Statistics variables. These are reset in reset_key_cache_counters(). */
  ulong global_blocks_changed;      /* number of currently dirty blocks      */
  ulonglong global_cache_w_requests;/* number of write requests (write hits) */
  ulonglong global_cache_write;     /* number of writes from cache to files  */
  ulonglong global_cache_r_requests;/* number of read requests (read hits)   */
  ulonglong global_cache_read;      /* number of reads from files to cache   */

  int blocks;                   /* max number of blocks in the cache        */
  uint hash_factor;             /* factor used to calculate hash function   */
  my_bool in_init;		/* Set to 1 in MySQL during init/resize     */
} SIMPLE_KEY_CACHE_CB;

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
  - to substitute calls of mysql_cond_wait for calls of
    mysql_cond_timedwait (wait with timeout set up);
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

#define STRUCT_PTR(TYPE, MEMBER, a)                                           \
          (TYPE *) ((char *) (a) - offsetof(TYPE, MEMBER))

/* types of condition variables */
#define  COND_FOR_REQUESTED 0
#define  COND_FOR_SAVED     1
#define  COND_FOR_READERS   2

typedef mysql_cond_t KEYCACHE_CONDVAR;

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
#define BLOCK_ERROR           1 /* an error occured when performing file i/o */
#define BLOCK_READ            2 /* file block is in the block buffer         */
#define BLOCK_IN_SWITCH       4 /* block is preparing to read new page       */
#define BLOCK_REASSIGNED      8 /* blk does not accept requests for old page */
#define BLOCK_IN_FLUSH       16 /* block is selected for flush               */
#define BLOCK_CHANGED        32 /* block buffer contains a dirty page        */
#define BLOCK_IN_USE         64 /* block is not free                         */
#define BLOCK_IN_EVICTION   128 /* block is selected for eviction            */
#define BLOCK_IN_FLUSHWRITE 256 /* block is in write to file                 */
#define BLOCK_FOR_UPDATE    512 /* block is selected for buffer modification */

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
  uchar *buffer;           /* buffer for the block page                       */
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

static int flush_all_key_blocks(SIMPLE_KEY_CACHE_CB *keycache);
static void end_simple_key_cache(SIMPLE_KEY_CACHE_CB *keycache, my_bool cleanup);
static void wait_on_queue(KEYCACHE_WQUEUE *wqueue,
                          mysql_mutex_t *mutex);
static void release_whole_queue(KEYCACHE_WQUEUE *wqueue);
static void free_block(SIMPLE_KEY_CACHE_CB *keycache, BLOCK_LINK *block);
#ifndef DBUG_OFF
static void test_key_cache(SIMPLE_KEY_CACHE_CB *keycache,
                           const char *where, my_bool lock);
#endif
#define KEYCACHE_BASE_EXPR(f, pos)                                            \
  ((ulong) ((pos) / keycache->key_cache_block_size) +	 (ulong) (f))
#define KEYCACHE_HASH(f, pos)                                                 \
  ((KEYCACHE_BASE_EXPR(f, pos) / keycache->hash_factor) &                     \
      (keycache->hash_entries-1))
#define FILE_HASH(f)                 ((uint) (f) & (CHANGED_BLOCKS_HASH-1))

#define DEFAULT_KEYCACHE_DEBUG_LOG  "keycache_debug.log"

#if defined(KEYCACHE_DEBUG) && ! defined(KEYCACHE_DEBUG_LOG)
#define KEYCACHE_DEBUG_LOG  DEFAULT_KEYCACHE_DEBUG_LOG
#endif

#if defined(KEYCACHE_DEBUG_LOG)
static FILE *keycache_debug_log=NULL;
static void keycache_debug_print(const char *fmt,...);
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
static int keycache_pthread_cond_wait(mysql_cond_t *cond,
                                      mysql_mutex_t *mutex);
#else
#define keycache_pthread_cond_wait(C, M) mysql_cond_wait(C, M)
#endif

#if defined(KEYCACHE_DEBUG)
static int keycache_pthread_mutex_lock(mysql_mutex_t *mutex);
static void keycache_pthread_mutex_unlock(mysql_mutex_t *mutex);
static int keycache_pthread_cond_signal(mysql_cond_t *cond);
#else
#define keycache_pthread_mutex_lock(M) mysql_mutex_lock(M)
#define keycache_pthread_mutex_unlock(M) mysql_mutex_unlock(M)
#define keycache_pthread_cond_signal(C) mysql_cond_signal(C)
#endif /* defined(KEYCACHE_DEBUG) */

#if !defined(DBUG_OFF)
#if defined(inline)
#undef inline
#endif
#define inline  /* disabled inline for easier debugging */
static int fail_block(BLOCK_LINK *block);
static int fail_hlink(HASH_LINK *hlink);
static int cache_empty(SIMPLE_KEY_CACHE_CB *keycache);
#endif


static inline uint next_power(uint value)
{
  return (uint) my_round_up_to_next_power((uint32) value) << 1;
}


/*
  Initialize a simple key cache

  SYNOPSIS
    init_simple_key_cache()
    keycache                pointer to the control block of a simple key cache 
    key_cache_block_size    size of blocks to keep cached data
    use_mem                 memory to use for the key cache buferrs/structures
    division_limit          division limit (may be zero)
    age_threshold           age threshold (may be zero)

  DESCRIPTION
    This function is the implementation of the init_key_cache interface
    function that is employed by simple (non-partitioned) key caches.
    The function builds a simple key cache and initializes the control block
    structure of the type SIMPLE_KEY_CACHE_CB that is used for this key cache. 
    The parameter keycache is supposed to point to this structure. 
    The parameter key_cache_block_size specifies the size of the blocks in
    the key cache to be built. The parameters division_limit and age_threshhold
    determine the initial values of those characteristics of the key cache
    that are used for midpoint insertion strategy. The parameter use_mem
    specifies the total amount of memory to be allocated for key cache blocks
    and auxiliary structures.       

  RETURN VALUE
    number of blocks in the key cache, if successful,
    <= 0 - otherwise.

  NOTES.
    if keycache->key_cache_inited != 0 we assume that the key cache
    is already initialized.  This is for now used by myisamchk, but shouldn't
    be something that a program should rely on!

    It's assumed that no two threads call this function simultaneously
    referring to the same key cache handle.
*/

static
int init_simple_key_cache(SIMPLE_KEY_CACHE_CB *keycache, uint key_cache_block_size,
		          size_t use_mem, uint division_limit,
		          uint age_threshold)
{
  ulong blocks, hash_links;
  size_t length;
  int error;
  DBUG_ENTER("init_simple_key_cache");
  DBUG_ASSERT(key_cache_block_size >= 512);

  KEYCACHE_DEBUG_OPEN;
  if (keycache->key_cache_inited && keycache->disk_blocks > 0)
  {
    DBUG_PRINT("warning",("key cache already in use"));
    DBUG_RETURN(0);
  }

  keycache->blocks_used= keycache->blocks_unused= 0;
  keycache->global_blocks_changed= 0;
  keycache->global_cache_w_requests= keycache->global_cache_r_requests= 0;
  keycache->global_cache_read= keycache->global_cache_write= 0;
  keycache->disk_blocks= -1;
  if (! keycache->key_cache_inited)
  {
    keycache->key_cache_inited= 1;
    keycache->hash_factor= 1;
    /*
      Initialize these variables once only.
      Their value must survive re-initialization during resizing.
    */
    keycache->in_resize= 0;
    keycache->resize_in_flush= 0;
    keycache->cnt_for_resize_op= 0;
    keycache->waiting_for_resize_cnt.last_thread= NULL;
    keycache->in_init= 0;
    mysql_mutex_init(key_KEY_CACHE_cache_lock,
                     &keycache->cache_lock, MY_MUTEX_INIT_FAST);
    keycache->resize_queue.last_thread= NULL;
  }

  keycache->key_cache_mem_size= use_mem;
  keycache->key_cache_block_size= key_cache_block_size;
  DBUG_PRINT("info", ("key_cache_block_size: %u",
		      key_cache_block_size));

  blocks= (ulong) (use_mem / (sizeof(BLOCK_LINK) + 2 * sizeof(HASH_LINK) +
                              sizeof(HASH_LINK*) * 5/4 + key_cache_block_size));
  /* It doesn't make sense to have too few blocks (less than 8) */
  if (blocks >= 8)
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
	     ((size_t) blocks * keycache->key_cache_block_size) > use_mem)
        blocks--;
      /* Allocate memory for cache page buffers */
      if ((keycache->block_mem=
	   my_large_malloc((size_t) blocks * keycache->key_cache_block_size,
			  MYF(0))))
      {
        /*
	  Allocate memory for blocks, hash_links and hash entries;
	  For each block 2 hash links are allocated
        */
        if ((keycache->block_root= (BLOCK_LINK*) my_malloc(length,
                                                           MYF(0))))
          break;
        my_large_free(keycache->block_mem);
        keycache->block_mem= 0;
      }
      if (blocks < 8)
      {
        my_errno= ENOMEM;
        my_error(EE_OUTOFMEMORY, MYF(ME_FATALERROR),
                 blocks * keycache->key_cache_block_size);
        goto err;
      }
      blocks= blocks / 4*3;
    }
    keycache->blocks_unused= blocks;
    keycache->disk_blocks= (int) blocks;
    keycache->hash_links= hash_links;
    keycache->hash_root= (HASH_LINK**) ((char*) keycache->block_root +
				        ALIGN_SIZE(blocks*sizeof(BLOCK_LINK)));
    keycache->hash_link_root= (HASH_LINK*) ((char*) keycache->hash_root +
				            ALIGN_SIZE((sizeof(HASH_LINK*) *
							keycache->hash_entries)));
    bzero((uchar*) keycache->block_root,
	  keycache->disk_blocks * sizeof(BLOCK_LINK));
    bzero((uchar*) keycache->hash_root,
          keycache->hash_entries * sizeof(HASH_LINK*));
    bzero((uchar*) keycache->hash_link_root,
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

    keycache->can_be_used= 1;

    keycache->waiting_for_hash_link.last_thread= NULL;
    keycache->waiting_for_block.last_thread= NULL;
    DBUG_PRINT("exit",
	       ("disk_blocks: %d  block_root: 0x%lx  hash_entries: %d\
 hash_root: 0x%lx  hash_links: %d  hash_link_root: 0x%lx",
		keycache->disk_blocks,  (long) keycache->block_root,
		keycache->hash_entries, (long) keycache->hash_root,
		keycache->hash_links,   (long) keycache->hash_link_root));
    bzero((uchar*) keycache->changed_blocks,
	  sizeof(keycache->changed_blocks[0]) * CHANGED_BLOCKS_HASH);
    bzero((uchar*) keycache->file_blocks,
	  sizeof(keycache->file_blocks[0]) * CHANGED_BLOCKS_HASH);
  }
  else
  {
    /* key_buffer_size is specified too small. Disable the cache. */
    keycache->can_be_used= 0;
  }

  keycache->blocks= keycache->disk_blocks > 0 ? keycache->disk_blocks : 0;
  DBUG_RETURN((int) keycache->disk_blocks);

err:
  error= my_errno;
  keycache->disk_blocks= 0;
  keycache->blocks=  0;
  if (keycache->block_mem)
  {
    my_large_free((uchar*) keycache->block_mem);
    keycache->block_mem= NULL;
  }
  if (keycache->block_root)
  {
    my_free(keycache->block_root);
    keycache->block_root= NULL;
  }
  my_errno= error;
  keycache->can_be_used= 0;
  DBUG_RETURN(0);
}


/*
  Prepare for resizing a simple key cache

  SYNOPSIS
    prepare_resize_simple_key_cache()
    keycache                pointer to the control block of a simple key cache	
    release_lock            <=> release the key cache lock before return

  DESCRIPTION
    This function flushes all dirty pages from a simple key cache and after
    this it destroys the key cache calling end_simple_key_cache. The function 
    takes the parameter keycache as a pointer to the control block 
    structure of the type SIMPLE_KEY_CACHE_CB for this key cache.
    The parameter release_lock says whether the key cache lock must be 
    released before return from the function.

  RETURN VALUE
    0 - on success,
    1 - otherwise.

  NOTES
    This function is the called by resize_simple_key_cache and
    resize_partitioned_key_cache that resize simple and partitioned key caches
    respectively. 
*/

static 
int prepare_resize_simple_key_cache(SIMPLE_KEY_CACHE_CB *keycache,
                                    my_bool release_lock)
{
  int res= 0;
  DBUG_ENTER("prepare_resize_simple_key_cache"); 
 
  keycache_pthread_mutex_lock(&keycache->cache_lock);

  /*
    We may need to wait for another thread which is doing a resize
    already. This cannot happen in the MySQL server though. It allows
    one resizer only. In set_var.cc keycache->in_init is used to block
    multiple attempts.
  */
  while (keycache->in_resize)
  {
    /* purecov: begin inspected */
    wait_on_queue(&keycache->resize_queue, &keycache->cache_lock);
    /* purecov: end */
  }

  /*
    Mark the operation in progress. This blocks other threads from doing
    a resize in parallel. It prohibits new blocks to enter the cache.
    Read/write requests can bypass the cache during the flush phase.
  */
  keycache->in_resize= 1;

  /* Need to flush only if keycache is enabled. */
  if (keycache->can_be_used)
  {
    /* Start the flush phase. */
    keycache->resize_in_flush= 1;

    if (flush_all_key_blocks(keycache))
    {
      /* TODO: if this happens, we should write a warning in the log file ! */
      keycache->resize_in_flush= 0;
      keycache->can_be_used= 0;
      res= 1;
      goto finish;
    }
    DBUG_ASSERT(cache_empty(keycache));

    /* End the flush phase. */
    keycache->resize_in_flush= 0;
  }

  /*
    Some direct read/write operations (bypassing the cache) may still be
    unfinished. Wait until they are done. If the key cache can be used,
    direct I/O is done in increments of key_cache_block_size. That is,
    every block is checked if it is in the cache. We need to wait for
    pending I/O before re-initializing the cache, because we may change
    the block size. Otherwise they could check for blocks at file
    positions where the new block division has none. We do also want to
    wait for I/O done when (if) the cache was disabled. It must not
    run in parallel with normal cache operation.
  */
  while (keycache->cnt_for_resize_op)
    wait_on_queue(&keycache->waiting_for_resize_cnt, &keycache->cache_lock);
  
  end_simple_key_cache(keycache, 0);

finish:
  if (release_lock)
    keycache_pthread_mutex_unlock(&keycache->cache_lock);     
  DBUG_RETURN(res);
}


/*
  Finalize resizing a simple key cache

  SYNOPSIS
    finish_resize_simple_key_cache()
    keycache                pointer to the control block of a simple key cache		
    acquire_lock            <=> acquire the key cache lock at start

  DESCRIPTION
    This function performs finalizing actions for the operation of 
    resizing a simple key cache. The function takes the parameter
    keycache as a pointer to the control block structure of the type
    SIMPLE_KEY_CACHE_CB for this key cache. The function sets the flag
    in_resize in this structure to FALSE.
    The parameter acquire_lock says whether the key cache lock must be
    acquired at the start of the function.

  RETURN VALUE
    none

  NOTES
    This function is the called by resize_simple_key_cache and
    resize_partitioned_key_cache that resize simple and partitioned key caches
    respectively. 
*/

static 
void finish_resize_simple_key_cache(SIMPLE_KEY_CACHE_CB *keycache,
                                    my_bool acquire_lock)
{
  DBUG_ENTER("finish_resize_simple_key_cache");

  if (acquire_lock)
    keycache_pthread_mutex_lock(&keycache->cache_lock); 
  
  mysql_mutex_assert_owner(&keycache->cache_lock);
			   
  /*
    Mark the resize finished. This allows other threads to start a
    resize or to request new cache blocks.
  */
  keycache->in_resize= 0;
  

  /* Signal waiting threads. */
  release_whole_queue(&keycache->resize_queue);


  keycache_pthread_mutex_unlock(&keycache->cache_lock);

  DBUG_VOID_RETURN;
}


/*
  Resize a simple key cache

  SYNOPSIS
    resize_simple_key_cache()
    keycache                pointer to the control block of a simple key cache
    key_cache_block_size    size of blocks to keep cached data
    use_mem                 memory to use for the key cache buffers/structures
    division_limit          new division limit (if not zero)
    age_threshold           new age threshold (if not zero)

  DESCRIPTION
    This function is the implementation of the resize_key_cache interface
    function that is employed by simple (non-partitioned) key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type SIMPLE_KEY_CACHE_CB for the simple key
    cache to be resized. 
    The parameter key_cache_block_size specifies the new size of the blocks in
    the key cache. The parameters division_limit and age_threshold
    determine the new initial values of those characteristics of the key cache
    that are used for midpoint insertion strategy. The parameter use_mem
    specifies the total amount of memory to be allocated for key cache blocks
    and auxiliary structures in the new key cache.           

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.

  NOTES.
    The function first calls the function prepare_resize_simple_key_cache
    to flush all dirty blocks from key cache, to free memory used
    for key cache blocks and auxiliary structures. After this the
    function builds a new key cache with new parameters.

    This implementation doesn't block the calls and executions of other
    functions from the key cache interface. However it assumes that the
    calls of resize_simple_key_cache itself are serialized.

    The function starts the operation only when all other threads
    performing operations with the key cache let her to proceed
    (when cnt_for_resize=0).
*/

static
int resize_simple_key_cache(SIMPLE_KEY_CACHE_CB *keycache, uint key_cache_block_size,
		            size_t use_mem, uint division_limit,
		            uint age_threshold)
{
  int blocks= 0;
  DBUG_ENTER("resize_simple_key_cache");

  if (!keycache->key_cache_inited)
    DBUG_RETURN(blocks);

  /*
    Note that the cache_lock mutex and the resize_queue are left untouched.
    We do not lose the cache_lock and will release it only at the end of 
    this function.
  */
  if (prepare_resize_simple_key_cache(keycache, 0))
    goto finish;

  /* The following will work even if use_mem is 0 */ 
  blocks= init_simple_key_cache(keycache, key_cache_block_size, use_mem,
			        division_limit, age_threshold);

finish:
  finish_resize_simple_key_cache(keycache, 0);

  DBUG_RETURN(blocks);
}


/*
  Increment counter blocking resize key cache operation
*/
static inline void inc_counter_for_resize_op(SIMPLE_KEY_CACHE_CB *keycache)
{
  keycache->cnt_for_resize_op++;
}


/*
  Decrement counter blocking resize key cache operation;
  Signal the operation to proceed when counter becomes equal zero
*/
static inline void dec_counter_for_resize_op(SIMPLE_KEY_CACHE_CB *keycache)
{
  if (!--keycache->cnt_for_resize_op)
    release_whole_queue(&keycache->waiting_for_resize_cnt);
}


/*
  Change key cache parameters of a simple key cache

  SYNOPSIS
    change_simple_key_cache_param()
    keycache                pointer to the control block of a simple key cache	
    division_limit          new division limit (if not zero)
    age_threshold           new age threshold (if not zero)

  DESCRIPTION
    This function is the implementation of the change_key_cache_param interface
    function that is employed by simple (non-partitioned) key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type SIMPLE_KEY_CACHE_CB for the simple key
    cache where new values of the division limit and the age threshold used
    for midpoint insertion strategy are to be set.  The parameters
    division_limit and age_threshold provide these new values.

  RETURN VALUE
    none

  NOTES.
    Presently the function resets the key cache parameters concerning
    midpoint insertion strategy - division_limit and age_threshold.
    This function changes some parameters of a given key cache without
    reformatting it. The function does not touch the contents the key 
    cache blocks.    
*/

static
void change_simple_key_cache_param(SIMPLE_KEY_CACHE_CB *keycache, uint division_limit,
			           uint age_threshold)
{
  DBUG_ENTER("change_simple_key_cache_param");
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
  Destroy a simple key cache 

  SYNOPSIS
    end_simple_key_cache()
    keycache                pointer to the control block of a simple key cache
    cleanup                 <=> complete free (free also mutex for key cache)

  DESCRIPTION
    This function is the implementation of the end_key_cache interface
    function that is employed by simple (non-partitioned) key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type SIMPLE_KEY_CACHE_CB for the simple key
    cache to be destroyed.
    The function frees the memory allocated for the key cache blocks and
    auxiliary structures. If the value of the parameter cleanup is TRUE 
    then even the key cache mutex is freed.

  RETURN VALUE
    none
*/

static
void end_simple_key_cache(SIMPLE_KEY_CACHE_CB *keycache, my_bool cleanup)
{
  DBUG_ENTER("end_simple_key_cache");
  DBUG_PRINT("enter", ("key_cache: 0x%lx", (long) keycache));

  if (!keycache->key_cache_inited)
    DBUG_VOID_RETURN;

  if (keycache->disk_blocks > 0)
  {
    if (keycache->block_mem)
    {
      my_large_free((uchar*) keycache->block_mem);
      keycache->block_mem= NULL;
      my_free(keycache->block_root);
      keycache->block_root= NULL;
    }
    keycache->disk_blocks= -1;
    /* Reset blocks_changed to be safe if flush_all_key_blocks is called */
    keycache->blocks_changed= 0;
  }

  DBUG_PRINT("status", ("used: %lu  changed: %lu  w_requests: %lu  "
                        "writes: %lu  r_requests: %lu  reads: %lu",
                        keycache->blocks_used, keycache->global_blocks_changed,
                        (ulong) keycache->global_cache_w_requests,
                        (ulong) keycache->global_cache_write,
                        (ulong) keycache->global_cache_r_requests,
                        (ulong) keycache->global_cache_read));

  /*
    Reset these values to be able to detect a disabled key cache.
    See Bug#44068 (RESTORE can disable the MyISAM Key Cache).
  */
  keycache->blocks_used= 0;
  keycache->blocks_unused= 0;

  if (cleanup)
  {
    mysql_mutex_destroy(&keycache->cache_lock);
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

  DBUG_ASSERT(!thread->next && !thread->prev);
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
  DBUG_ASSERT(thread->next && thread->prev);
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
#if !defined(DBUG_OFF)
  /*
    This makes it easier to see it's not in a chain during debugging.
    And some DBUG_ASSERT() rely on it.
  */
  thread->prev= NULL;
#endif
}


/*
  Add a thread to single-linked queue of waiting threads

  SYNOPSIS
    wait_on_queue()
      wqueue            Pointer to the queue structure.
      mutex             Cache_lock to acquire after awake.

  RETURN VALUE
    none

  NOTES.
    Queue is represented by a circular list of the thread structures
    The list is single-linked of the type (*next), accessed by a pointer
    to the last element.

    The function protects against stray signals by verifying that the
    current thread is unlinked from the queue when awaking. However,
    since several threads can wait for the same event, it might be
    necessary for the caller of the function to check again if the
    condition for awake is indeed matched.
*/

static void wait_on_queue(KEYCACHE_WQUEUE *wqueue,
                          mysql_mutex_t *mutex)
{
  struct st_my_thread_var *last;
  struct st_my_thread_var *thread= my_thread_var;

  /* Add to queue. */
  DBUG_ASSERT(!thread->next);
  DBUG_ASSERT(!thread->prev); /* Not required, but must be true anyway. */
  if (! (last= wqueue->last_thread))
    thread->next= thread;
  else
  {
    thread->next= last->next;
    last->next= thread;
  }
  wqueue->last_thread= thread;

  /*
    Wait until thread is removed from queue by the signalling thread.
    The loop protects against stray signals.
  */
  do
  {
    KEYCACHE_DBUG_PRINT("wait", ("suspend thread %ld", thread->id));
    keycache_pthread_cond_wait(&thread->suspend, mutex);
  }
  while (thread->next);
}


/*
  Remove all threads from queue signaling them to proceed

  SYNOPSIS
    release_whole_queue()
      wqueue            pointer to the queue structure

  RETURN VALUE
    none

  NOTES.
    See notes for wait_on_queue().
    When removed from the queue each thread is signaled via condition
    variable thread->suspend.
*/

static void release_whole_queue(KEYCACHE_WQUEUE *wqueue)
{
  struct st_my_thread_var *last;
  struct st_my_thread_var *next;
  struct st_my_thread_var *thread;

  /* Queue may be empty. */
  if (!(last= wqueue->last_thread))
    return;

  next= last->next;
  do
  {
    thread=next;
    KEYCACHE_DBUG_PRINT("release_whole_queue: signal",
                        ("thread %ld", thread->id));
    /* Signal the thread. */
    keycache_pthread_cond_signal(&thread->suspend);
    /* Take thread from queue. */
    next=thread->next;
    thread->next= NULL;
  }
  while (thread != last);

  /* Now queue is definitely empty. */
  wqueue->last_thread= NULL;
}


/*
  Unlink a block from the chain of dirty/clean blocks
*/

static inline void unlink_changed(BLOCK_LINK *block)
{
  DBUG_ASSERT(block->prev_changed && *block->prev_changed == block);
  if (block->next_changed)
    block->next_changed->prev_changed= block->prev_changed;
  *block->prev_changed= block->next_changed;

#if !defined(DBUG_OFF)
  /*
    This makes it easier to see it's not in a chain during debugging.
    And some DBUG_ASSERT() rely on it.
  */
  block->next_changed= NULL;
  block->prev_changed= NULL;
#endif
}


/*
  Link a block into the chain of dirty/clean blocks
*/

static inline void link_changed(BLOCK_LINK *block, BLOCK_LINK **phead)
{
  DBUG_ASSERT(!block->next_changed);
  DBUG_ASSERT(!block->prev_changed);
  block->prev_changed= phead;
  if ((block->next_changed= *phead))
    (*phead)->prev_changed= &block->next_changed;
  *phead= block;
}


/*
  Link a block in a chain of clean blocks of a file.

  SYNOPSIS
    link_to_file_list()
      keycache		Key cache handle
      block             Block to relink
      file              File to be linked to
      unlink            If to unlink first

  DESCRIPTION
    Unlink a block from whichever chain it is linked in, if it's
    asked for, and link it to the chain of clean blocks of the
    specified file.

  NOTE
    Please do never set/clear BLOCK_CHANGED outside of
    link_to_file_list() or link_to_changed_list().
    You would risk to damage correct counting of changed blocks
    and to find blocks in the wrong hash.

  RETURN
    void
*/

static void link_to_file_list(SIMPLE_KEY_CACHE_CB *keycache,
                              BLOCK_LINK *block, int file,
                              my_bool unlink_block)
{
  DBUG_ASSERT(block->status & BLOCK_IN_USE);
  DBUG_ASSERT(block->hash_link && block->hash_link->block == block);
  DBUG_ASSERT(block->hash_link->file == file);
  if (unlink_block)
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
  Re-link a block from the clean chain to the dirty chain of a file.

  SYNOPSIS
    link_to_changed_list()
      keycache		key cache handle
      block             block to relink

  DESCRIPTION
    Unlink a block from the chain of clean blocks of a file
    and link it to the chain of dirty blocks of the same file.

  NOTE
    Please do never set/clear BLOCK_CHANGED outside of
    link_to_file_list() or link_to_changed_list().
    You would risk to damage correct counting of changed blocks
    and to find blocks in the wrong hash.

  RETURN
    void
*/

static void link_to_changed_list(SIMPLE_KEY_CACHE_CB *keycache,
                                 BLOCK_LINK *block)
{
  DBUG_ASSERT(block->status & BLOCK_IN_USE);
  DBUG_ASSERT(!(block->status & BLOCK_CHANGED));
  DBUG_ASSERT(block->hash_link && block->hash_link->block == block);

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
    The LRU ring is represented by a circular list of block structures.
    The list is double-linked of the type (**prev,*next) type.
    The LRU ring is divided into two parts - hot and warm.
    There are two pointers to access the last blocks of these two
    parts. The beginning of the warm part follows right after the
    end of the hot part.
    Only blocks of the warm part can be used for eviction.
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

    It is also possible that the block is selected for eviction and thus
    not linked in the LRU ring.
*/

static void link_block(SIMPLE_KEY_CACHE_CB *keycache, BLOCK_LINK *block,
                       my_bool hot, my_bool at_end)
{
  BLOCK_LINK *ins;
  BLOCK_LINK **pins;

  DBUG_ASSERT((block->status & ~BLOCK_CHANGED) == (BLOCK_READ | BLOCK_IN_USE));
  DBUG_ASSERT(block->hash_link); /*backptr to block NULL from free_block()*/
  DBUG_ASSERT(!block->requests);
  DBUG_ASSERT(block->prev_changed && *block->prev_changed == block);
  DBUG_ASSERT(!block->next_used);
  DBUG_ASSERT(!block->prev_used);
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
    /*
      NOTE: We assigned the block to the hash_link and signalled the
      requesting thread(s). But it is possible that other threads runs
      first. These threads see the hash_link assigned to a block which
      is assigned to another hash_link and not marked BLOCK_IN_SWITCH.
      This can be a problem for functions that do not select the block
      via its hash_link: flush and free. They do only see a block which
      is in a "normal" state and don't know that it will be evicted soon.

      We cannot set BLOCK_IN_SWITCH here because only one of the
      requesting threads must handle the eviction. All others must wait
      for it to complete. If we set the flag here, the threads would not
      know who is in charge of the eviction. Without the flag, the first
      thread takes the stick and sets the flag.

      But we need to note in the block that is has been selected for
      eviction. It must not be freed. The evicting thread will not
      expect the block in the free list. Before freeing we could also
      check if block->requests > 1. But I think including another flag
      in the check of block->status is slightly more efficient and
      probably easier to read.
    */
    block->status|= BLOCK_IN_EVICTION;
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
    /* The LRU ring is empty. Let the block point to itself. */
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

static void unlink_block(SIMPLE_KEY_CACHE_CB *keycache, BLOCK_LINK *block)
{
  DBUG_ASSERT((block->status & ~BLOCK_CHANGED) == (BLOCK_READ | BLOCK_IN_USE));
  DBUG_ASSERT(block->hash_link); /*backptr to block NULL from free_block()*/
  DBUG_ASSERT(!block->requests);
  DBUG_ASSERT(block->prev_changed && *block->prev_changed == block);
  DBUG_ASSERT(block->next_used && block->prev_used &&
              (block->next_used->prev_used == &block->next_used) &&
              (*block->prev_used == block));
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
#if !defined(DBUG_OFF)
  /*
    This makes it easier to see it's not in a chain during debugging.
    And some DBUG_ASSERT() rely on it.
  */
  block->prev_used= NULL;
#endif

  KEYCACHE_THREAD_TRACE("unlink_block");
#if defined(KEYCACHE_DEBUG)
  KEYCACHE_DBUG_ASSERT(keycache->blocks_available != 0);
  keycache->blocks_available--;
  KEYCACHE_DBUG_PRINT("unlink_block",
    ("unlinked block %u  status=%x   #requests=%u  #available=%u",
     BLOCK_NUMBER(block), block->status,
     block->requests, keycache->blocks_available));
#endif
}


/*
  Register requests for a block.

  SYNOPSIS
    reg_requests()
      keycache          Pointer to a key cache data structure.
      block             Pointer to the block to register a request on.
      count             Number of requests. Always 1.

  NOTE
    The first request unlinks the block from the LRU ring. This means
    that it is protected against eveiction.

  RETURN
    void
*/
static void reg_requests(SIMPLE_KEY_CACHE_CB *keycache,
                         BLOCK_LINK *block, int count)
{
  DBUG_ASSERT(block->status & BLOCK_IN_USE);
  DBUG_ASSERT(block->hash_link);

  if (!block->requests)
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
    Every linking to the LRU ring decrements by one a special block
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

    It is also possible that the block is selected for eviction and thus
    not linked in the LRU ring.
*/

static void unreg_request(SIMPLE_KEY_CACHE_CB *keycache,
                          BLOCK_LINK *block, int at_end)
{
  DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
  DBUG_ASSERT(block->hash_link); /*backptr to block NULL from free_block()*/
  DBUG_ASSERT(block->requests);
  DBUG_ASSERT(block->prev_changed && *block->prev_changed == block);
  DBUG_ASSERT(!block->next_used);
  DBUG_ASSERT(!block->prev_used);
  /*
    Unregister the request, but do not link erroneous blocks into the
    LRU ring.
  */
  if (!--block->requests && !(block->status & BLOCK_ERROR))
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
      KEYCACHE_DBUG_PRINT("unreg_request", ("#warm_blocks: %lu",
                           keycache->warm_blocks));
    }
    link_block(keycache, block, hot, (my_bool)at_end);
    block->last_hit_time= keycache->keycache_time;
    keycache->keycache_time++;
    /*
      At this place, the block might be in the LRU ring or not. If an
      evicter was waiting for a block, it was selected for eviction and
      not linked in the LRU ring.
    */

    /*
      Check if we should link a hot block to the warm block sub-chain.
      It is possible that we select the same block as above. But it can
      also be another block. In any case a block from the LRU ring is
      selected. In other words it works even if the above block was
      selected for eviction and not linked in the LRU ring. Since this
      happens only if the LRU ring is empty, the block selected below
      would be NULL and the rest of the function skipped.
    */
    block= keycache->used_ins;
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
      KEYCACHE_DBUG_PRINT("unreg_request", ("#warm_blocks: %lu",
                           keycache->warm_blocks));
    }
  }
}

/*
  Remove a reader of the page in block
*/

static void remove_reader(BLOCK_LINK *block)
{
  DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
  DBUG_ASSERT(block->hash_link && block->hash_link->block == block);
  DBUG_ASSERT(block->prev_changed && *block->prev_changed == block);
  DBUG_ASSERT(!block->next_used);
  DBUG_ASSERT(!block->prev_used);
  DBUG_ASSERT(block->hash_link->requests);
  if (! --block->hash_link->requests && block->condvar)
    keycache_pthread_cond_signal(block->condvar);
}


/*
  Wait until the last reader of the page in block
  signals on its termination
*/

static void wait_for_readers(SIMPLE_KEY_CACHE_CB *keycache,
                             BLOCK_LINK *block)
{
  struct st_my_thread_var *thread= my_thread_var;
  DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
  DBUG_ASSERT(!(block->status & (BLOCK_IN_FLUSH | BLOCK_CHANGED)));
  DBUG_ASSERT(block->hash_link);
  DBUG_ASSERT(block->hash_link->block == block);
  /* Linked in file_blocks or changed_blocks hash. */
  DBUG_ASSERT(block->prev_changed && *block->prev_changed == block);
  /* Not linked in LRU ring. */
  DBUG_ASSERT(!block->next_used);
  DBUG_ASSERT(!block->prev_used);
  while (block->hash_link->requests)
  {
    KEYCACHE_DBUG_PRINT("wait_for_readers: wait",
                        ("suspend thread %ld  block %u",
                         thread->id, BLOCK_NUMBER(block)));
    /* There must be no other waiter. We have no queue here. */
    DBUG_ASSERT(!block->condvar);
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

static void unlink_hash(SIMPLE_KEY_CACHE_CB *keycache, HASH_LINK *hash_link)
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

static HASH_LINK *get_hash_link(SIMPLE_KEY_CACHE_CB *keycache,
                                int file, my_off_t filepos)
{
  reg1 HASH_LINK *hash_link, **start;
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
      KEYCACHE_PAGE page;
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

static BLOCK_LINK *find_key_block(SIMPLE_KEY_CACHE_CB *keycache,
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
  DBUG_PRINT("enter", ("fd: %d  pos: %lu  wrmode: %d",
                       file, (ulong) filepos, wrmode));
  KEYCACHE_DBUG_PRINT("find_key_block", ("fd: %d  pos: %lu  wrmode: %d",
                                         file, (ulong) filepos,
                                         wrmode));
#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_keycache2",
               test_key_cache(keycache, "start of find_key_block", 0););
#endif

restart:
  /*
    If the flush phase of a resize operation fails, the cache is left
    unusable. This will be detected only after "goto restart".
  */
  if (!keycache->can_be_used)
    DBUG_RETURN(0);

  /*
    Find the hash_link for the requested file block (file, filepos). We
    do always get a hash_link here. It has registered our request so
    that no other thread can use it for another file block until we
    release the request (which is done by remove_reader() usually). The
    hash_link can have a block assigned to it or not. If there is a
    block, it may be assigned to this hash_link or not. In cases where a
    block is evicted from the cache, it is taken from the LRU ring and
    referenced by the new hash_link. But the block can still be assigned
    to its old hash_link for some time if it needs to be flushed first,
    or if there are other threads still reading it.

    Summary:
      hash_link is always returned.
      hash_link->block can be:
      - NULL or
      - not assigned to this hash_link or
      - assigned to this hash_link. If assigned, the block can have
        - invalid data (when freshly assigned) or
        - valid data. Valid data can be
          - changed over the file contents (dirty) or
          - not changed (clean).
  */
  hash_link= get_hash_link(keycache, file, filepos);
  DBUG_ASSERT((hash_link->file == file) && (hash_link->diskpos == filepos));

  page_status= -1;
  if ((block= hash_link->block) &&
      block->hash_link == hash_link && (block->status & BLOCK_READ))
  {
    /* Assigned block with valid (changed or unchanged) contents. */
    page_status= PAGE_READ;
  }
  /*
    else (page_status == -1)
      - block == NULL or
      - block not assigned to this hash_link or
      - block assigned but not yet read from file (invalid data).
  */

  if (keycache->in_resize)
  {
    /* This is a request during a resize operation */

    if (!block)
    {
      struct st_my_thread_var *thread;

      /*
        The file block is not in the cache. We don't need it in the
        cache: we are going to read or write directly to file. Cancel
        the request. We can simply decrement hash_link->requests because
        we did not release cache_lock since increasing it. So no other
        thread can wait for our request to become released.
      */
      if (hash_link->requests == 1)
      {
        /*
          We are the only one to request this hash_link (this file/pos).
          Free the hash_link.
        */
        hash_link->requests--;
        unlink_hash(keycache, hash_link);
        DBUG_RETURN(0);
      }

      /*
        More requests on the hash_link. Someone tries to evict a block
        for this hash_link (could have started before resizing started).
        This means that the LRU ring is empty. Otherwise a block could
        be assigned immediately. Behave like a thread that wants to
        evict a block for this file/pos. Add to the queue of threads
        waiting for a block. Wait until there is one assigned.

        Refresh the request on the hash-link so that it cannot be reused
        for another file/pos.
      */
      thread= my_thread_var;
      thread->opt_info= (void *) hash_link;
      link_into_queue(&keycache->waiting_for_block, thread);
      do
      {
        KEYCACHE_DBUG_PRINT("find_key_block: wait",
                            ("suspend thread %ld", thread->id));
        keycache_pthread_cond_wait(&thread->suspend,
                                   &keycache->cache_lock);
      } while (thread->next);
      thread->opt_info= NULL;
      /*
        A block should now be assigned to the hash_link. But it may
        still need to be evicted. Anyway, we should re-check the
        situation. page_status must be set correctly.
      */
      hash_link->requests--;
      goto restart;
    } /* end of if (!block) */

    /*
      There is a block for this file/pos in the cache. Register a
      request on it. This unlinks it from the LRU ring (if it is there)
      and hence protects it against eviction (if not already in
      eviction). We need this for returning the block to the caller, for
      calling remove_reader() (for debugging purposes), and for calling
      free_block(). The only case where we don't need the request is if
      the block is in eviction. In that case we have to unregister the
      request later.
    */
    reg_requests(keycache, block, 1);

    if (page_status != PAGE_READ)
    {
      /*
        - block not assigned to this hash_link or
        - block assigned but not yet read from file (invalid data).

        This must be a block in eviction. It will be read soon. We need
        to wait here until this happened. Otherwise the caller could
        access a wrong block or a block which is in read. While waiting
        we cannot lose hash_link nor block. We have registered a request
        on the hash_link. Everything can happen to the block but changes
        in the hash_link -> block relationship. In other words:
        everything can happen to the block but free or another completed
        eviction.

        Note that we bahave like a secondary requestor here. We just
        cannot return with PAGE_WAIT_TO_BE_READ. This would work for
        read requests and writes on dirty blocks that are not in flush
        only. Waiting here on COND_FOR_REQUESTED works in all
        situations.
      */
      DBUG_ASSERT(((block->hash_link != hash_link) &&
                   (block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH))) ||
                  ((block->hash_link == hash_link) &&
                   !(block->status & BLOCK_READ)));
      wait_on_queue(&block->wqueue[COND_FOR_REQUESTED], &keycache->cache_lock);
      /*
        Here we can trust that the block has been assigned to this
        hash_link (block->hash_link == hash_link) and read into the
        buffer (BLOCK_READ). The worst things possible here are that the
        block is in free (BLOCK_REASSIGNED). But the block is still
        assigned to the hash_link. The freeing thread waits until we
        release our request on the hash_link. The block must not be
        again in eviction because we registered an request on it before
        starting to wait.
      */
      DBUG_ASSERT(block->hash_link == hash_link);
      DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
      DBUG_ASSERT(!(block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH)));
    }
    /*
      The block is in the cache. Assigned to the hash_link. Valid data.
      Note that in case of page_st == PAGE_READ, the block can be marked
      for eviction. In any case it can be marked for freeing.
    */

    if (!wrmode)
    {
      /* A reader can just read the block. */
      *page_st= PAGE_READ;
      DBUG_ASSERT((hash_link->file == file) &&
                  (hash_link->diskpos == filepos) &&
                  (block->hash_link == hash_link));
      DBUG_RETURN(block);
    }

    /*
      This is a writer. No two writers for the same block can exist.
      This must be assured by locks outside of the key cache.
    */
    DBUG_ASSERT(!(block->status & BLOCK_FOR_UPDATE) || fail_block(block));

    while (block->status & BLOCK_IN_FLUSH)
    {
      /*
        Wait until the block is flushed to file. Do not release the
        request on the hash_link yet to prevent that the block is freed
        or reassigned while we wait. While we wait, several things can
        happen to the block, including another flush. But the block
        cannot be reassigned to another hash_link until we release our
        request on it. But it can be marked BLOCK_REASSIGNED from free
        or eviction, while they wait for us to release the hash_link.
      */
      wait_on_queue(&block->wqueue[COND_FOR_SAVED], &keycache->cache_lock);
      /*
        If the flush phase failed, the resize could have finished while
        we waited here.
      */
      if (!keycache->in_resize)
      {
        remove_reader(block);
        unreg_request(keycache, block, 1);
        goto restart;
      }
      DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
      DBUG_ASSERT(!(block->status & BLOCK_FOR_UPDATE) || fail_block(block));
      DBUG_ASSERT(block->hash_link == hash_link);
    }

    if (block->status & BLOCK_CHANGED)
    {
      /*
        We want to write a block with changed contents. If the cache
        block size is bigger than the callers block size (e.g. MyISAM),
        the caller may replace part of the block only. Changes of the
        other part of the block must be preserved. Since the block has
        not yet been selected for flush, we can still add our changes.
      */
      *page_st= PAGE_READ;
      DBUG_ASSERT((hash_link->file == file) &&
                  (hash_link->diskpos == filepos) &&
                  (block->hash_link == hash_link));
      DBUG_RETURN(block);
    }

    /*
      This is a write request for a clean block. We do not want to have
      new dirty blocks in the cache while resizing. We will free the
      block and write directly to file. If the block is in eviction or
      in free, we just let it go.

      Unregister from the hash_link. This must be done before freeing
      the block. And it must be done if not freeing the block. Because
      we could have waited above, we need to call remove_reader(). Other
      threads could wait for us to release our request on the hash_link.
    */
    remove_reader(block);

    /* If the block is not in eviction and not in free, we can free it. */
    if (!(block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH |
                           BLOCK_REASSIGNED)))
    {
      /*
        Free block as we are going to write directly to file.
        Although we have an exlusive lock for the updated key part,
        the control can be yielded by the current thread as we might
        have unfinished readers of other key parts in the block
        buffer. Still we are guaranteed not to have any readers
        of the key part we are writing into until the block is
        removed from the cache as we set the BLOCK_REASSIGNED
        flag (see the code below that handles reading requests).
      */
      free_block(keycache, block);
    }
    else
    {
      /*
        The block will be evicted/freed soon. Don't touch it in any way.
        Unregister the request that we registered above.
      */
      unreg_request(keycache, block, 1);

      /*
        The block is still assigned to the hash_link (the file/pos that
        we are going to write to). Wait until the eviction/free is
        complete. Otherwise the direct write could complete before all
        readers are done with the block. So they could read outdated
        data.

        Since we released our request on the hash_link, it can be reused
        for another file/pos. Hence we cannot just check for
        block->hash_link == hash_link. As long as the resize is
        proceeding the block cannot be reassigned to the same file/pos
        again. So we can terminate the loop when the block is no longer
        assigned to this file/pos.
      */
      do
      {
        wait_on_queue(&block->wqueue[COND_FOR_SAVED],
                      &keycache->cache_lock);
        /*
          If the flush phase failed, the resize could have finished
          while we waited here.
        */
        if (!keycache->in_resize)
          goto restart;
      } while (block->hash_link &&
               (block->hash_link->file == file) &&
               (block->hash_link->diskpos == filepos));
    }
    DBUG_RETURN(0);
  }

  if (page_status == PAGE_READ &&
      (block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH |
                        BLOCK_REASSIGNED)))
  {
    /*
      This is a request for a block to be removed from cache. The block
      is assigned to this hash_link and contains valid data, but is
      marked for eviction or to be freed. Possible reasons why it has
      not yet been evicted/freed can be a flush before reassignment
      (BLOCK_IN_SWITCH), readers of the block have not finished yet
      (BLOCK_REASSIGNED), or the evicting thread did not yet awake after
      the block has been selected for it (BLOCK_IN_EVICTION).
    */

    KEYCACHE_DBUG_PRINT("find_key_block",
                        ("request for old page in block %u "
                         "wrmode: %d  block->status: %d",
                         BLOCK_NUMBER(block), wrmode, block->status));
    /*
       Only reading requests can proceed until the old dirty page is flushed,
       all others are to be suspended, then resubmitted
    */
    if (!wrmode && !(block->status & BLOCK_REASSIGNED))
    {
      /*
        This is a read request and the block not yet reassigned. We can
        register our request and proceed. This unlinks the block from
        the LRU ring and protects it against eviction.
      */
      reg_requests(keycache, block, 1);
    }
    else
    {
      /*
        Either this is a write request for a block that is in eviction
        or in free. We must not use it any more. Instead we must evict
        another block. But we cannot do this before the eviction/free is
        done. Otherwise we would find the same hash_link + block again
        and again.

        Or this is a read request for a block in eviction/free that does
        not require a flush, but waits for readers to finish with the
        block. We do not read this block to let the eviction/free happen
        as soon as possible. Again we must wait so that we don't find
        the same hash_link + block again and again.
      */
      DBUG_ASSERT(hash_link->requests);
      hash_link->requests--;
      KEYCACHE_DBUG_PRINT("find_key_block",
                          ("request waiting for old page to be saved"));
      wait_on_queue(&block->wqueue[COND_FOR_SAVED], &keycache->cache_lock);
      KEYCACHE_DBUG_PRINT("find_key_block",
                          ("request for old page resubmitted"));
      /*
        The block is no longer assigned to this hash_link.
        Get another one.
      */
      goto restart;
    }
  }
  else
  {
    /*
      This is a request for a new block or for a block not to be removed.
      Either
      - block == NULL or
      - block not assigned to this hash_link or
      - block assigned but not yet read from file,
      or
      - block assigned with valid (changed or unchanged) data and
      - it will not be reassigned/freed.
    */
    if (! block)
    {
      /* No block is assigned to the hash_link yet. */
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
          size_t block_mem_offset;
          /* There are some never used blocks, take first of them */
          DBUG_ASSERT(keycache->blocks_used <
                      (ulong) keycache->disk_blocks);
          block= &keycache->block_root[keycache->blocks_used];
          block_mem_offset= 
           ((size_t) keycache->blocks_used) * keycache->key_cache_block_size;
          block->buffer= ADD_TO_PTR(keycache->block_mem,
                                    block_mem_offset,
                                    uchar*);
          keycache->blocks_used++;
          DBUG_ASSERT(!block->next_used);
        }
        DBUG_ASSERT(!block->prev_used);
        DBUG_ASSERT(!block->next_changed);
        DBUG_ASSERT(!block->prev_changed);
        DBUG_ASSERT(!block->hash_link);
        DBUG_ASSERT(!block->status);
        DBUG_ASSERT(!block->requests);
        keycache->blocks_unused--;
        block->status= BLOCK_IN_USE;
        block->length= 0;
        block->offset= keycache->key_cache_block_size;
        block->requests= 1;
        block->temperature= BLOCK_COLD;
        block->hits_left= init_hits_left;
        block->last_hit_time= 0;
        block->hash_link= hash_link;
        hash_link->block= block;
        link_to_file_list(keycache, block, file, 0);
        page_status= PAGE_TO_BE_READ;
        KEYCACHE_DBUG_PRINT("find_key_block",
                            ("got free or never used block %u",
                             BLOCK_NUMBER(block)));
      }
      else
      {
	/*
          There are no free blocks and no never used blocks, use a block
          from the LRU ring.
        */

        if (! keycache->used_last)
        {
          /*
            The LRU ring is empty. Wait until a new block is added to
            it. Several threads might wait here for the same hash_link,
            all of them must get the same block. While waiting for a
            block, after a block is selected for this hash_link, other
            threads can run first before this one awakes. During this
            time interval other threads find this hash_link pointing to
            the block, which is still assigned to another hash_link. In
            this case the block is not marked BLOCK_IN_SWITCH yet, but
            it is marked BLOCK_IN_EVICTION.
          */

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
          /* Assert that block has a request registered. */
          DBUG_ASSERT(hash_link->block->requests);
          /* Assert that block is not in LRU ring. */
          DBUG_ASSERT(!hash_link->block->next_used);
          DBUG_ASSERT(!hash_link->block->prev_used);
        }
        /*
          If we waited above, hash_link->block has been assigned by
          link_block(). Otherwise it is still NULL. In the latter case
          we need to grab a block from the LRU ring ourselves.
        */
        block= hash_link->block;
        if (! block)
        {
          /* Select the last block from the LRU ring. */
          block= keycache->used_last->next_used;
          block->hits_left= init_hits_left;
          block->last_hit_time= 0;
          hash_link->block= block;
          /*
            Register a request on the block. This unlinks it from the
            LRU ring and protects it against eviction.
          */
          DBUG_ASSERT(!block->requests);
          reg_requests(keycache, block,1);
          /*
            We do not need to set block->status|= BLOCK_IN_EVICTION here
            because we will set block->status|= BLOCK_IN_SWITCH
            immediately without releasing the lock in between. This does
            also support debugging. When looking at the block, one can
            see if the block has been selected by link_block() after the
            LRU ring was empty, or if it was grabbed directly from the
            LRU ring in this branch.
          */
        }

        /*
          If we had to wait above, there is a small chance that another
          thread grabbed this block for the same file block already. But
          in most cases the first condition is true.
        */
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
            if (block->status & BLOCK_IN_FLUSH)
            {
              /*
                The block is marked for flush. If we do not wait here,
                it could happen that we write the block, reassign it to
                another file block, then, before the new owner can read
                the new file block, the flusher writes the cache block
                (which still has the old contents) to the new file block!
              */
              wait_on_queue(&block->wqueue[COND_FOR_SAVED],
                            &keycache->cache_lock);
              /*
                The block is marked BLOCK_IN_SWITCH. It should be left
                alone except for reading. No free, no write.
              */
              DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
              DBUG_ASSERT(!(block->status & (BLOCK_REASSIGNED |
                                             BLOCK_CHANGED |
                                             BLOCK_FOR_UPDATE)));
            }
            else
            {
              block->status|= BLOCK_IN_FLUSH | BLOCK_IN_FLUSHWRITE;
              /*
                BLOCK_IN_EVICTION may be true or not. Other flags must
                have a fixed value.
              */
              DBUG_ASSERT((block->status & ~BLOCK_IN_EVICTION) ==
                          (BLOCK_READ | BLOCK_IN_SWITCH |
                           BLOCK_IN_FLUSH | BLOCK_IN_FLUSHWRITE |
                           BLOCK_CHANGED | BLOCK_IN_USE));
              DBUG_ASSERT(block->hash_link);

              keycache_pthread_mutex_unlock(&keycache->cache_lock);
              /*
                The call is thread safe because only the current
                thread might change the block->hash_link value
              */
              error= my_pwrite(block->hash_link->file,
                               block->buffer + block->offset,
                               block->length - block->offset,
                               block->hash_link->diskpos + block->offset,
                               MYF(MY_NABP | MY_WAIT_IF_FULL));
              keycache_pthread_mutex_lock(&keycache->cache_lock);

              /* Block status must not have changed. */
              DBUG_ASSERT((block->status & ~BLOCK_IN_EVICTION) ==
                          (BLOCK_READ | BLOCK_IN_SWITCH |
                           BLOCK_IN_FLUSH | BLOCK_IN_FLUSHWRITE |
                           BLOCK_CHANGED | BLOCK_IN_USE) || fail_block(block));
              keycache->global_cache_write++;
            }
          }

          block->status|= BLOCK_REASSIGNED;
          /*
            The block comes from the LRU ring. It must have a hash_link
            assigned.
          */
          DBUG_ASSERT(block->hash_link);
          if (block->hash_link)
          {
            /*
              All pending requests for this page must be resubmitted.
              This must be done before waiting for readers. They could
              wait for the flush to complete. And we must also do it
              after the wait. Flushers might try to free the block while
              we wait. They would wait until the reassignment is
              complete. Also the block status must reflect the correct
              situation: The block is not changed nor in flush any more.
              Note that we must not change the BLOCK_CHANGED flag
              outside of link_to_file_list() so that it is always in the
              correct queue and the *blocks_changed counters are
              correct.
            */
            block->status&= ~(BLOCK_IN_FLUSH | BLOCK_IN_FLUSHWRITE);
            link_to_file_list(keycache, block, block->hash_link->file, 1);
            release_whole_queue(&block->wqueue[COND_FOR_SAVED]);
            /*
              The block is still assigned to its old hash_link.
	      Wait until all pending read requests
	      for this page are executed
	      (we could have avoided this waiting, if we had read
	      a page in the cache in a sweep, without yielding control)
            */
            wait_for_readers(keycache, block);
            DBUG_ASSERT(block->hash_link && block->hash_link->block == block &&
                        block->prev_changed);
            /* The reader must not have been a writer. */
            DBUG_ASSERT(!(block->status & BLOCK_CHANGED));

            /* Wake flushers that might have found the block in between. */
            release_whole_queue(&block->wqueue[COND_FOR_SAVED]);

            /* Remove the hash link for the old file block from the hash. */
            unlink_hash(keycache, block->hash_link);

            /*
              For sanity checks link_to_file_list() asserts that block
              and hash_link refer to each other. Hence we need to assign
              the hash_link first, but then we would not know if it was
              linked before. Hence we would not know if to unlink it. So
              unlink it here and call link_to_file_list(..., FALSE).
            */
            unlink_changed(block);
          }
          block->status= error ? BLOCK_ERROR : BLOCK_IN_USE ;
          block->length= 0;
          block->offset= keycache->key_cache_block_size;
          block->hash_link= hash_link;
          link_to_file_list(keycache, block, file, 0);
          page_status= PAGE_TO_BE_READ;

          KEYCACHE_DBUG_ASSERT(block->hash_link->block == block);
          KEYCACHE_DBUG_ASSERT(hash_link->block->hash_link == hash_link);
        }
        else
        {
          /*
            Either (block->hash_link == hash_link),
	    or     (block->status & BLOCK_IN_SWITCH).

            This is for secondary requests for a new file block only.
            Either it is already assigned to the new hash_link meanwhile
            (if we had to wait due to empty LRU), or it is already in
            eviction by another thread. Since this block has been
            grabbed from the LRU ring and attached to this hash_link,
            another thread cannot grab the same block from the LRU ring
            anymore. If the block is in eviction already, it must become
            attached to the same hash_link and as such destined for the
            same file block.
          */
          KEYCACHE_DBUG_PRINT("find_key_block",
                              ("block->hash_link: %p  hash_link: %p  "
                               "block->status: %u", block->hash_link,
                               hash_link, block->status ));
          page_status= (((block->hash_link == hash_link) &&
                         (block->status & BLOCK_READ)) ?
                        PAGE_READ : PAGE_WAIT_TO_BE_READ);
        }
      }
    }
    else
    {
      /*
        Block is not NULL. This hash_link points to a block.
        Either
        - block not assigned to this hash_link (yet) or
        - block assigned but not yet read from file,
        or
        - block assigned with valid (changed or unchanged) data and
        - it will not be reassigned/freed.

        The first condition means hash_link points to a block in
        eviction. This is not necessarily marked by BLOCK_IN_SWITCH yet.
        But then it is marked BLOCK_IN_EVICTION. See the NOTE in
        link_block(). In both cases it is destined for this hash_link
        and its file block address. When this hash_link got its block
        address, the block was removed from the LRU ring and cannot be
        selected for eviction (for another hash_link) again.

        Register a request on the block. This is another protection
        against eviction.
      */
      DBUG_ASSERT(((block->hash_link != hash_link) &&
                   (block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH))) ||
                  ((block->hash_link == hash_link) &&
                   !(block->status & BLOCK_READ)) ||
                  ((block->status & BLOCK_READ) &&
                   !(block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH))));
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
  /* Same assert basically, but be very sure. */
  KEYCACHE_DBUG_ASSERT(block);
  /* Assert that block has a request and is not in LRU ring. */
  DBUG_ASSERT(block->requests);
  DBUG_ASSERT(!block->next_used);
  DBUG_ASSERT(!block->prev_used);
  /* Assert that we return the correct block. */
  DBUG_ASSERT((page_status == PAGE_WAIT_TO_BE_READ) ||
              ((block->hash_link->file == file) &&
               (block->hash_link->diskpos == filepos)));
  *page_st=page_status;
  KEYCACHE_DBUG_PRINT("find_key_block",
                      ("fd: %d  pos: %lu  block->status: %u  page_status: %d",
                       file, (ulong) filepos, block->status,
                       page_status));

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

static void read_block(SIMPLE_KEY_CACHE_CB *keycache,
                       BLOCK_LINK *block, uint read_length,
                       uint min_length, my_bool primary)
{
  size_t got_length;

  /* On entry cache_lock is locked */

  KEYCACHE_THREAD_TRACE("read_block");
  if (primary)
  {
    /*
      This code is executed only by threads that submitted primary
      requests. Until block->status contains BLOCK_READ, all other
      request for the block become secondary requests. For a primary
      request the block must be properly initialized.
    */
    DBUG_ASSERT(((block->status & ~BLOCK_FOR_UPDATE) == BLOCK_IN_USE) ||
                fail_block(block));
    DBUG_ASSERT((block->length == 0) || fail_block(block));
    DBUG_ASSERT((block->offset == keycache->key_cache_block_size) ||
                fail_block(block));
    DBUG_ASSERT((block->requests > 0) || fail_block(block));

    KEYCACHE_DBUG_PRINT("read_block",
                        ("page to be read by primary request"));

    keycache->global_cache_read++;
    /* Page is not in buffer yet, is to be read from disk */
    keycache_pthread_mutex_unlock(&keycache->cache_lock);
    /*
      Here other threads may step in and register as secondary readers.
      They will register in block->wqueue[COND_FOR_REQUESTED].
    */
    got_length= my_pread(block->hash_link->file, block->buffer,
                         read_length, block->hash_link->diskpos, MYF(0));
    keycache_pthread_mutex_lock(&keycache->cache_lock);
    /*
      The block can now have been marked for free (in case of
      FLUSH_RELEASE). Otherwise the state must be unchanged.
    */
    DBUG_ASSERT(((block->status & ~(BLOCK_REASSIGNED |
                                    BLOCK_FOR_UPDATE)) == BLOCK_IN_USE) ||
                fail_block(block));
    DBUG_ASSERT((block->length == 0) || fail_block(block));
    DBUG_ASSERT((block->offset == keycache->key_cache_block_size) ||
                fail_block(block));
    DBUG_ASSERT((block->requests > 0) || fail_block(block));

    if (got_length < min_length)
      block->status|= BLOCK_ERROR;
    else
    {
      block->status|= BLOCK_READ;
      block->length= got_length;
      /*
        Do not set block->offset here. If this block is marked
        BLOCK_CHANGED later, we want to flush only the modified part. So
        only a writer may set block->offset down from
        keycache->key_cache_block_size.
      */
    }
    KEYCACHE_DBUG_PRINT("read_block",
                        ("primary request: new page in cache"));
    /* Signal that all pending requests for this page now can be processed */
    release_whole_queue(&block->wqueue[COND_FOR_REQUESTED]);
  }
  else
  {
    /*
      This code is executed only by threads that submitted secondary
      requests. At this point it could happen that the cache block is
      not yet assigned to the hash_link for the requested file block.
      But at awake from the wait this should be the case. Unfortunately
      we cannot assert this here because we do not know the hash_link
      for the requested file block nor the file and position. So we have
      to assert this in the caller.
    */
    KEYCACHE_DBUG_PRINT("read_block",
                      ("secondary request waiting for new page to be read"));
    wait_on_queue(&block->wqueue[COND_FOR_REQUESTED], &keycache->cache_lock);
    KEYCACHE_DBUG_PRINT("read_block",
                        ("secondary request: new page in cache"));
  }
}


/*
  Read a block of data from a simple key cache into a buffer

  SYNOPSIS

    simple_key_cache_read()
    keycache            pointer to the control block of a simple key cache
    file                handler for the file for the block of data to be read
    filepos             position of the block of data in the file
    level               determines the weight of the data
    buff                buffer to where the data must be placed
    length              length of the buffer
    block_length        length of the read data from a key cache block 
    return_buffer       return pointer to the key cache buffer with the data

  DESCRIPTION
    This function is the implementation of the key_cache_read interface
    function that is employed by simple (non-partitioned) key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type SIMPLE_KEY_CACHE_CB for a simple key
    cache.
    In a general case the function reads a block of data from the key cache
    into the buffer buff of the size specified by the parameter length. The
    beginning of the  block of data to be read is specified by the parameters
    file and filepos. The length of the read data is the same as the length
    of the buffer. The data is read into the buffer in key_cache_block_size
    increments. If the next portion of the data is not found in any key cache
    block, first it is read from file into the key cache.
    If the parameter return_buffer is not ignored and its value is TRUE, and 
    the data to be read of the specified size block_length can be read from one
    key cache buffer, then the function returns a pointer to the data in the
    key cache buffer.
    The function takse into account parameters block_length and return buffer
    only in a single-threaded environment.
    The parameter 'level' is used only by the midpoint insertion strategy 
    when the data or its portion cannot be found in the key cache. 
   
  RETURN VALUE
    Returns address from where the data is placed if successful, 0 - otherwise.

  NOTES
    Filepos must be a multiple of 'block_length', but it doesn't
    have to be a multiple of key_cache_block_size;
*/

uchar *simple_key_cache_read(SIMPLE_KEY_CACHE_CB *keycache,
                             File file, my_off_t filepos, int level,
                             uchar *buff, uint length,
                             uint block_length __attribute__((unused)),
                             int return_buffer __attribute__((unused)))
{
  my_bool locked_and_incremented= FALSE;
  int error=0;
  uchar *start= buff;
  DBUG_ENTER("simple_key_cache_read");
  DBUG_PRINT("enter", ("fd: %u  pos: %lu  length: %u",
               (uint) file, (ulong) filepos, length));

  if (keycache->key_cache_inited)
  {
    /* Key cache is used */
    reg1 BLOCK_LINK *block;
    uint read_length;
    uint offset;
    int page_st;

    if (MYSQL_KEYCACHE_READ_START_ENABLED())
    {
      MYSQL_KEYCACHE_READ_START(my_filename(file), length,
                                (ulong) (keycache->blocks_used *
                                         keycache->key_cache_block_size),
                                (ulong) (keycache->blocks_unused *
                                         keycache->key_cache_block_size));
    }
  
    /*
      When the key cache is once initialized, we use the cache_lock to
      reliably distinguish the cases of normal operation, resizing, and
      disabled cache. We always increment and decrement
      'cnt_for_resize_op' so that a resizer can wait for pending I/O.
    */
    keycache_pthread_mutex_lock(&keycache->cache_lock);
    /*
      Cache resizing has two phases: Flushing and re-initializing. In
      the flush phase read requests are allowed to bypass the cache for
      blocks not in the cache. find_key_block() returns NULL in this
      case.

      After the flush phase new I/O requests must wait until the
      re-initialization is done. The re-initialization can be done only
      if no I/O request is in progress. The reason is that
      key_cache_block_size can change. With enabled cache, I/O is done
      in chunks of key_cache_block_size. Every chunk tries to use a
      cache block first. If the block size changes in the middle, a
      block could be missed and old data could be read.
    */
    while (keycache->in_resize && !keycache->resize_in_flush)
      wait_on_queue(&keycache->resize_queue, &keycache->cache_lock);
    /* Register the I/O for the next resize. */
    inc_counter_for_resize_op(keycache);
    locked_and_incremented= TRUE;
    /* Requested data may not always be aligned to cache blocks. */
    offset= (uint) (filepos % keycache->key_cache_block_size);
    /* Read data in key_cache_block_size increments */
    do
    {
      /* Cache could be disabled in a later iteration. */
      if (!keycache->can_be_used)
      {
        KEYCACHE_DBUG_PRINT("key_cache_read", ("keycache cannot be used"));
        goto no_key_cache;
      }
      /* Start reading at the beginning of the cache block. */
      filepos-= offset;
      /* Do not read beyond the end of the cache block. */
      read_length= length;
      set_if_smaller(read_length, keycache->key_cache_block_size-offset);
      KEYCACHE_DBUG_ASSERT(read_length > 0);

      /* Request the cache block that matches file/pos. */
      keycache->global_cache_r_requests++;

      MYSQL_KEYCACHE_READ_BLOCK(keycache->key_cache_block_size);

      block=find_key_block(keycache, file, filepos, level, 0, &page_st);
      if (!block)
      {
        /*
          This happens only for requests submitted during key cache
          resize. The block is not in the cache and shall not go in.
          Read directly from file.
        */
        keycache->global_cache_read++;
        keycache_pthread_mutex_unlock(&keycache->cache_lock);
        error= (my_pread(file, (uchar*) buff, read_length,
                         filepos + offset, MYF(MY_NABP)) != 0);
        keycache_pthread_mutex_lock(&keycache->cache_lock);
        goto next_block;
      }
      if (!(block->status & BLOCK_ERROR))
      {
        if (page_st != PAGE_READ)
        {
          MYSQL_KEYCACHE_READ_MISS();
          /* The requested page is to be read into the block buffer */
          read_block(keycache, block,
                     keycache->key_cache_block_size, read_length+offset,
                     (my_bool)(page_st == PAGE_TO_BE_READ));
          /*
            A secondary request must now have the block assigned to the
            requested file block. It does not hurt to check it for
            primary requests too.
          */
          DBUG_ASSERT(keycache->can_be_used);
          DBUG_ASSERT(block->hash_link->file == file);
          DBUG_ASSERT(block->hash_link->diskpos == filepos);
          DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
        }
        else if (block->length < read_length + offset)
        {
          /*
            Impossible if nothing goes wrong:
            this could only happen if we are using a file with
            small key blocks and are trying to read outside the file
          */
          my_errno= -1;
          block->status|= BLOCK_ERROR;
        }
        else
        {
          MYSQL_KEYCACHE_READ_HIT();
        }
      }

      /* block status may have added BLOCK_ERROR in the above 'if'. */
      if (!(block->status & BLOCK_ERROR))
      {
        {
          DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
#if !defined(SERIALIZED_READ_FROM_CACHE)
          keycache_pthread_mutex_unlock(&keycache->cache_lock);
#endif

          /* Copy data from the cache buffer */
          memcpy(buff, block->buffer+offset, (size_t) read_length);

#if !defined(SERIALIZED_READ_FROM_CACHE)
          keycache_pthread_mutex_lock(&keycache->cache_lock);
          DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
#endif
        }
      }

      remove_reader(block);

      /* Error injection for coverage testing. */
      DBUG_EXECUTE_IF("key_cache_read_block_error",
                      block->status|= BLOCK_ERROR;);

      /* Do not link erroneous blocks into the LRU ring, but free them. */
      if (!(block->status & BLOCK_ERROR))
      {
        /*
          Link the block into the LRU ring if it's the last submitted
          request for the block. This enables eviction for the block.
        */
        unreg_request(keycache, block, 1);
      }
      else
      {
        free_block(keycache, block);
        error= 1;
        break;
      }

    next_block:
      buff+= read_length;
      filepos+= read_length+offset;
      offset= 0;

    } while ((length-= read_length));
    if (MYSQL_KEYCACHE_READ_DONE_ENABLED())
    {
      MYSQL_KEYCACHE_READ_DONE((ulong) (keycache->blocks_used *
                                        keycache->key_cache_block_size),
                               (ulong) (keycache->blocks_unused *
                                        keycache->key_cache_block_size));
    }
    goto end;
  }
  KEYCACHE_DBUG_PRINT("key_cache_read", ("keycache not initialized"));

no_key_cache:
  /* Key cache is not used */

  keycache->global_cache_r_requests++;
  keycache->global_cache_read++;

  if (locked_and_incremented)
    keycache_pthread_mutex_unlock(&keycache->cache_lock);
  if (my_pread(file, (uchar*) buff, length, filepos, MYF(MY_NABP)))
    error= 1;
  if (locked_and_incremented)
    keycache_pthread_mutex_lock(&keycache->cache_lock);

end:
  if (locked_and_incremented)
  {
    dec_counter_for_resize_op(keycache);
    keycache_pthread_mutex_unlock(&keycache->cache_lock);
  }
  DBUG_PRINT("exit", ("error: %d", error ));
  DBUG_RETURN(error ? (uchar*) 0 : start);
}


/*
  Insert a block of file data from a buffer into a simple key cache

  SYNOPSIS
    simple_key_cache_insert()
    keycache            pointer to the control block of a simple key cache 
    file                handler for the file to insert data from
    filepos             position of the block of data in the file to insert
    level               determines the weight of the data
    buff                buffer to read data from
    length              length of the data in the buffer

  DESCRIPTION
    This function is the implementation of the key_cache_insert interface
    function that is employed by simple (non-partitioned) key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type SIMPLE_KEY_CACHE_CB for a simple key
    cache.
    The function writes a block of file data from a buffer into the key cache.
    The buffer is specified with the parameters buff and length - the pointer
    to the beginning of the buffer and its size respectively. It's assumed
    the buffer contains the data from 'file' allocated from the position
    filepos. The data is copied from the buffer in key_cache_block_size
    increments.
    The parameter level is used to set one characteristic for the key buffers
    loaded with the data from buff. The characteristic is used only by the
    midpoint insertion strategy.  
   
  RETURN VALUE
    0 if a success, 1 - otherwise.

  NOTES
    The function is used by MyISAM to move all blocks from a index file to 
    the key cache. It can be performed in parallel with reading the file data
    from the key buffers by other threads.

*/

static
int simple_key_cache_insert(SIMPLE_KEY_CACHE_CB *keycache,
                            File file, my_off_t filepos, int level,
                            uchar *buff, uint length)
{
  int error= 0;
  DBUG_ENTER("key_cache_insert");
  DBUG_PRINT("enter", ("fd: %u  pos: %lu  length: %u",
               (uint) file,(ulong) filepos, length));

  if (keycache->key_cache_inited)
  {
    /* Key cache is used */
    reg1 BLOCK_LINK *block;
    uint read_length;
    uint offset;
    int page_st;
    my_bool locked_and_incremented= FALSE;

    /*
      When the keycache is once initialized, we use the cache_lock to
      reliably distinguish the cases of normal operation, resizing, and
      disabled cache. We always increment and decrement
      'cnt_for_resize_op' so that a resizer can wait for pending I/O.
    */
    keycache_pthread_mutex_lock(&keycache->cache_lock);
    /*
      We do not load index data into a disabled cache nor into an
      ongoing resize.
    */
    if (!keycache->can_be_used || keycache->in_resize)
	goto no_key_cache;
    /* Register the pseudo I/O for the next resize. */
    inc_counter_for_resize_op(keycache);
    locked_and_incremented= TRUE;
    /* Loaded data may not always be aligned to cache blocks. */
    offset= (uint) (filepos % keycache->key_cache_block_size);
    /* Load data in key_cache_block_size increments. */
    do
    {
      /* Cache could be disabled or resizing in a later iteration. */
      if (!keycache->can_be_used || keycache->in_resize)
	goto no_key_cache;
      /* Start loading at the beginning of the cache block. */
      filepos-= offset;
      /* Do not load beyond the end of the cache block. */
      read_length= length;
      set_if_smaller(read_length, keycache->key_cache_block_size-offset);
      KEYCACHE_DBUG_ASSERT(read_length > 0);

      /* The block has been read by the caller already. */
      keycache->global_cache_read++;
      /* Request the cache block that matches file/pos. */
      keycache->global_cache_r_requests++;
      block= find_key_block(keycache, file, filepos, level, 0, &page_st);
      if (!block)
      {
        /*
          This happens only for requests submitted during key cache
          resize. The block is not in the cache and shall not go in.
          Stop loading index data.
        */
        goto no_key_cache;
      }
      if (!(block->status & BLOCK_ERROR))
      {
        if ((page_st == PAGE_WAIT_TO_BE_READ) ||
            ((page_st == PAGE_TO_BE_READ) &&
             (offset || (read_length < keycache->key_cache_block_size))))
        {
          /*
            Either

            this is a secondary request for a block to be read into the
            cache. The block is in eviction. It is not yet assigned to
            the requested file block (It does not point to the right
            hash_link). So we cannot call remove_reader() on the block.
            And we cannot access the hash_link directly here. We need to
            wait until the assignment is complete. read_block() executes
            the correct wait when called with primary == FALSE.

            Or

            this is a primary request for a block to be read into the
            cache and the supplied data does not fill the whole block.

            This function is called on behalf of a LOAD INDEX INTO CACHE
            statement, which is a read-only task and allows other
            readers. It is possible that a parallel running reader tries
            to access this block. If it needs more data than has been
            supplied here, it would report an error. To be sure that we
            have all data in the block that is available in the file, we
            read the block ourselves.

            Though reading again what the caller did read already is an
            expensive operation, we need to do this for correctness.
          */
          read_block(keycache, block, keycache->key_cache_block_size,
                     read_length + offset, (page_st == PAGE_TO_BE_READ));
          /*
            A secondary request must now have the block assigned to the
            requested file block. It does not hurt to check it for
            primary requests too.
          */
          DBUG_ASSERT(keycache->can_be_used);
          DBUG_ASSERT(block->hash_link->file == file);
          DBUG_ASSERT(block->hash_link->diskpos == filepos);
          DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
        }
        else if (page_st == PAGE_TO_BE_READ)
        {
          /*
            This is a new block in the cache. If we come here, we have
            data for the whole block.
          */
          DBUG_ASSERT(block->hash_link->requests);
          DBUG_ASSERT(block->status & BLOCK_IN_USE);
          DBUG_ASSERT((page_st == PAGE_TO_BE_READ) ||
                      (block->status & BLOCK_READ));

#if !defined(SERIALIZED_READ_FROM_CACHE)
          keycache_pthread_mutex_unlock(&keycache->cache_lock);
          /*
            Here other threads may step in and register as secondary readers.
            They will register in block->wqueue[COND_FOR_REQUESTED].
          */
#endif

          /* Copy data from buff */
          memcpy(block->buffer+offset, buff, (size_t) read_length);

#if !defined(SERIALIZED_READ_FROM_CACHE)
          keycache_pthread_mutex_lock(&keycache->cache_lock);
          DBUG_ASSERT(block->status & BLOCK_IN_USE);
          DBUG_ASSERT((page_st == PAGE_TO_BE_READ) ||
                      (block->status & BLOCK_READ));
#endif
          /*
            After the data is in the buffer, we can declare the block
            valid. Now other threads do not need to register as
            secondary readers any more. They can immediately access the
            block.
          */
          block->status|= BLOCK_READ;
          block->length= read_length+offset;
          /*
            Do not set block->offset here. If this block is marked
            BLOCK_CHANGED later, we want to flush only the modified part. So
            only a writer may set block->offset down from
            keycache->key_cache_block_size.
          */
          KEYCACHE_DBUG_PRINT("key_cache_insert",
                              ("primary request: new page in cache"));
          /* Signal all pending requests. */
          release_whole_queue(&block->wqueue[COND_FOR_REQUESTED]);
        }
        else
        {
          /*
            page_st == PAGE_READ. The block is in the buffer. All data
            must already be present. Blocks are always read with all
            data available on file. Assert that the block does not have
            less contents than the preloader supplies. If the caller has
            data beyond block->length, it means that a file write has
            been done while this block was in cache and not extended
            with the new data. If the condition is met, we can simply
            ignore the block.
          */
          DBUG_ASSERT((page_st == PAGE_READ) &&
                      (read_length + offset <= block->length));
        }

        /*
          A secondary request must now have the block assigned to the
          requested file block. It does not hurt to check it for primary
          requests too.
        */
        DBUG_ASSERT(block->hash_link->file == file);
        DBUG_ASSERT(block->hash_link->diskpos == filepos);
        DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
      } /* end of if (!(block->status & BLOCK_ERROR)) */

      remove_reader(block);

      /* Error injection for coverage testing. */
      DBUG_EXECUTE_IF("key_cache_insert_block_error",
                      block->status|= BLOCK_ERROR; errno=EIO;);

      /* Do not link erroneous blocks into the LRU ring, but free them. */
      if (!(block->status & BLOCK_ERROR))
      {
        /*
          Link the block into the LRU ring if it's the last submitted
          request for the block. This enables eviction for the block.
        */
        unreg_request(keycache, block, 1);
      }
      else
      {
        free_block(keycache, block);
        error= 1;
        break;
      }

      buff+= read_length;
      filepos+= read_length+offset;
      offset= 0;

    } while ((length-= read_length));

  no_key_cache:
    if (locked_and_incremented)
      dec_counter_for_resize_op(keycache);
    keycache_pthread_mutex_unlock(&keycache->cache_lock);
  }
  DBUG_RETURN(error);
}


/*
  Write a buffer into a simple key cache

  SYNOPSIS

    simple_key_cache_write()
    keycache            pointer to the control block of a simple key cache
    file                handler for the file to write data to
    file_extra          maps of key cache partitions containing 
                        dirty pages from file 
    filepos             position in the file to write data to
    level               determines the weight of the data
    buff                buffer with the data
    length              length of the buffer
    dont_write          if is 0 then all dirty pages involved in writing
                        should have been flushed from key cache

  DESCRIPTION
    This function is the implementation of the key_cache_write interface
    function that is employed by simple (non-partitioned) key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type SIMPLE_KEY_CACHE_CB for a simple key
    cache.
    In a general case the function copies data from a buffer into the key
    cache. The buffer is specified with the parameters buff and length -
    the pointer to the beginning of the buffer and its size respectively.
    It's assumed the buffer contains the data to be written into 'file'
    starting from the position filepos. The data is copied from the buffer
    in key_cache_block_size increments.
    If the value of the parameter dont_write is FALSE then the function
    also writes the data into file.
    The parameter level is used to set one characteristic for the key buffers
    filled with the data from buff. The characteristic is employed only by
    the midpoint insertion strategy.
    The parameter file_extra currently makes sense only for simple key caches
    that are elements of a partitioned key cache. It provides a pointer to the
    shared bitmap of the partitions that may contains dirty pages for the file.
    This bitmap is used to optimize the function 
    flush_partitioned_key_cache_blocks. 
      
  RETURN VALUE
    0 if a success, 1 - otherwise.

  NOTES
    This implementation exploits the fact that the function is called only
    when a thread has got an exclusive lock for the key file.
*/

static
int simple_key_cache_write(SIMPLE_KEY_CACHE_CB *keycache,
                           File file, void *file_extra __attribute__((unused)),                       
                           my_off_t filepos, int level,
                           uchar *buff, uint length,
                           uint block_length  __attribute__((unused)),
                           int dont_write)
{
  my_bool locked_and_incremented= FALSE;
  int error=0;
  DBUG_ENTER("simple_key_cache_write");
  DBUG_PRINT("enter",
             ("fd: %u  pos: %lu  length: %u  block_length: %u"
              "  key_block_length: %u",
              (uint) file, (ulong) filepos, length, block_length,
              keycache ? keycache->key_cache_block_size : 0));

  if (!dont_write)
  {
    /* purecov: begin inspected */
    /* Not used in the server. */
    /* Force writing from buff into disk. */
    keycache->global_cache_w_requests++;
    keycache->global_cache_write++;
    if (my_pwrite(file, buff, length, filepos, MYF(MY_NABP | MY_WAIT_IF_FULL)))
      DBUG_RETURN(1);
    /* purecov: end */
  }

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_keycache",
               test_key_cache(keycache, "start of key_cache_write", 1););
#endif

  if (keycache->key_cache_inited)
  {
    /* Key cache is used */
    reg1 BLOCK_LINK *block;
    uint read_length;
    uint offset;
    int page_st;

    if (MYSQL_KEYCACHE_WRITE_START_ENABLED())
    {
      MYSQL_KEYCACHE_WRITE_START(my_filename(file), length,
                                 (ulong) (keycache->blocks_used *
                                          keycache->key_cache_block_size),
                                 (ulong) (keycache->blocks_unused *
                                          keycache->key_cache_block_size));
    }

    /*
      When the key cache is once initialized, we use the cache_lock to
      reliably distinguish the cases of normal operation, resizing, and
      disabled cache. We always increment and decrement
      'cnt_for_resize_op' so that a resizer can wait for pending I/O.
    */
    keycache_pthread_mutex_lock(&keycache->cache_lock);
    /*
      Cache resizing has two phases: Flushing and re-initializing. In
      the flush phase write requests can modify dirty blocks that are
      not yet in flush. Otherwise they are allowed to bypass the cache.
      find_key_block() returns NULL in both cases (clean blocks and
      non-cached blocks).

      After the flush phase new I/O requests must wait until the
      re-initialization is done. The re-initialization can be done only
      if no I/O request is in progress. The reason is that
      key_cache_block_size can change. With enabled cache I/O is done in
      chunks of key_cache_block_size. Every chunk tries to use a cache
      block first. If the block size changes in the middle, a block
      could be missed and data could be written below a cached block.
    */
    while (keycache->in_resize && !keycache->resize_in_flush)
      wait_on_queue(&keycache->resize_queue, &keycache->cache_lock);
    /* Register the I/O for the next resize. */
    inc_counter_for_resize_op(keycache);
    locked_and_incremented= TRUE;
    /* Requested data may not always be aligned to cache blocks. */
    offset= (uint) (filepos % keycache->key_cache_block_size);
    /* Write data in key_cache_block_size increments. */
    do
    {
      /* Cache could be disabled in a later iteration. */
      if (!keycache->can_be_used)
	goto no_key_cache;

      MYSQL_KEYCACHE_WRITE_BLOCK(keycache->key_cache_block_size);
      /* Start writing at the beginning of the cache block. */
      filepos-= offset;
      /* Do not write beyond the end of the cache block. */
      read_length= length;
      set_if_smaller(read_length, keycache->key_cache_block_size-offset);
      KEYCACHE_DBUG_ASSERT(read_length > 0);

      /* Request the cache block that matches file/pos. */
      keycache->global_cache_w_requests++;
      block= find_key_block(keycache, file, filepos, level, 1, &page_st);
      if (!block)
      {
        /*
          This happens only for requests submitted during key cache
          resize. The block is not in the cache and shall not go in.
          Write directly to file.
        */
        if (dont_write)
        {
          /* Used in the server. */
          keycache->global_cache_write++;
          keycache_pthread_mutex_unlock(&keycache->cache_lock);
          if (my_pwrite(file, (uchar*) buff, read_length, filepos + offset,
                        MYF(MY_NABP | MY_WAIT_IF_FULL)))
            error=1;
          keycache_pthread_mutex_lock(&keycache->cache_lock);
        }
        goto next_block;
      }
      /*
        Prevent block from flushing and from being selected for to be
        freed. This must be set when we release the cache_lock.
        However, we must not set the status of the block before it is
        assigned to this file/pos.
      */
      if (page_st != PAGE_WAIT_TO_BE_READ)
        block->status|= BLOCK_FOR_UPDATE;
      /*
        We must read the file block first if it is not yet in the cache
        and we do not replace all of its contents.

        In cases where the cache block is big enough to contain (parts
        of) index blocks of different indexes, our request can be
        secondary (PAGE_WAIT_TO_BE_READ). In this case another thread is
        reading the file block. If the read completes after us, it
        overwrites our new contents with the old contents. So we have to
        wait for the other thread to complete the read of this block.
        read_block() takes care for the wait.
      */
      if (!(block->status & BLOCK_ERROR) &&
          ((page_st == PAGE_TO_BE_READ &&
            (offset || read_length < keycache->key_cache_block_size)) ||
           (page_st == PAGE_WAIT_TO_BE_READ)))
      {
        read_block(keycache, block,
                   offset + read_length >= keycache->key_cache_block_size?
                   offset : keycache->key_cache_block_size,
                   offset, (page_st == PAGE_TO_BE_READ));
        DBUG_ASSERT(keycache->can_be_used);
        DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
        /*
          Prevent block from flushing and from being selected for to be
          freed. This must be set when we release the cache_lock.
          Here we set it in case we could not set it above.
        */
        block->status|= BLOCK_FOR_UPDATE;
      }
      /*
        The block should always be assigned to the requested file block
        here. It need not be BLOCK_READ when overwriting the whole block.
      */
      DBUG_ASSERT(block->hash_link->file == file);
      DBUG_ASSERT(block->hash_link->diskpos == filepos);
      DBUG_ASSERT(block->status & BLOCK_IN_USE);
      DBUG_ASSERT((page_st == PAGE_TO_BE_READ) || (block->status & BLOCK_READ));
      /*
        The block to be written must not be marked BLOCK_REASSIGNED.
        Otherwise it could be freed in dirty state or reused without
        another flush during eviction. It must also not be in flush.
        Otherwise the old contens may have been flushed already and
        the flusher could clear BLOCK_CHANGED without flushing the
        new changes again.
      */
      DBUG_ASSERT(!(block->status & BLOCK_REASSIGNED));

      while (block->status & BLOCK_IN_FLUSHWRITE)
      {
        /*
          Another thread is flushing the block. It was dirty already.
          Wait until the block is flushed to file. Otherwise we could
          modify the buffer contents just while it is written to file.
          An unpredictable file block contents would be the result.
          While we wait, several things can happen to the block,
          including another flush. But the block cannot be reassigned to
          another hash_link until we release our request on it.
        */
        wait_on_queue(&block->wqueue[COND_FOR_SAVED], &keycache->cache_lock);
        DBUG_ASSERT(keycache->can_be_used);
        DBUG_ASSERT(block->status & (BLOCK_READ | BLOCK_IN_USE));
        /* Still must not be marked for free. */
        DBUG_ASSERT(!(block->status & BLOCK_REASSIGNED));
        DBUG_ASSERT(block->hash_link && (block->hash_link->block == block));
      }

      /*
        We could perhaps release the cache_lock during access of the
        data like in the other functions. Locks outside of the key cache
        assure that readers and a writer do not access the same range of
        data. Parallel accesses should happen only if the cache block
        contains multiple index block(fragment)s. So different parts of
        the buffer would be read/written. An attempt to flush during
        memcpy() is prevented with BLOCK_FOR_UPDATE.
      */
      if (!(block->status & BLOCK_ERROR))
      {
#if !defined(SERIALIZED_READ_FROM_CACHE)
        keycache_pthread_mutex_unlock(&keycache->cache_lock);
#endif
        memcpy(block->buffer+offset, buff, (size_t) read_length);

#if !defined(SERIALIZED_READ_FROM_CACHE)
        keycache_pthread_mutex_lock(&keycache->cache_lock);
#endif
      }

      if (!dont_write)
      {
        /* Not used in the server. buff has been written to disk at start. */
        if ((block->status & BLOCK_CHANGED) &&
            (!offset && read_length >= keycache->key_cache_block_size))
             link_to_file_list(keycache, block, block->hash_link->file, 1);
      }
      else if (! (block->status & BLOCK_CHANGED))
        link_to_changed_list(keycache, block);
      block->status|=BLOCK_READ;
      /*
        Allow block to be selected for to be freed. Since it is marked
        BLOCK_CHANGED too, it won't be selected for to be freed without
        a flush.
      */
      block->status&= ~BLOCK_FOR_UPDATE;
      set_if_smaller(block->offset, offset);
      set_if_bigger(block->length, read_length+offset);

      /* Threads may be waiting for the changes to be complete. */
      release_whole_queue(&block->wqueue[COND_FOR_REQUESTED]);

      /*
        If only a part of the cache block is to be replaced, and the
        rest has been read from file, then the cache lock has been
        released for I/O and it could be possible that another thread
        wants to evict or free the block and waits for it to be
        released. So we must not just decrement hash_link->requests, but
        also wake a waiting thread.
      */
      remove_reader(block);

      /* Error injection for coverage testing. */
      DBUG_EXECUTE_IF("key_cache_write_block_error",
                      block->status|= BLOCK_ERROR;);

      /* Do not link erroneous blocks into the LRU ring, but free them. */
      if (!(block->status & BLOCK_ERROR))
      {
        /*
          Link the block into the LRU ring if it's the last submitted
          request for the block. This enables eviction for the block.
        */
        unreg_request(keycache, block, 1);
      }
      else
      {
        /* Pretend a "clean" block to avoid complications. */
        block->status&= ~(BLOCK_CHANGED);
        free_block(keycache, block);
        error= 1;
        break;
      }

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
    /* Used in the server. */
    keycache->global_cache_w_requests++;
    keycache->global_cache_write++;
    if (locked_and_incremented)
      keycache_pthread_mutex_unlock(&keycache->cache_lock);
    if (my_pwrite(file, (uchar*) buff, length, filepos,
		  MYF(MY_NABP | MY_WAIT_IF_FULL)))
      error=1;
    if (locked_and_incremented)
      keycache_pthread_mutex_lock(&keycache->cache_lock);
  }

end:
  if (locked_and_incremented)
  {
    dec_counter_for_resize_op(keycache);
    keycache_pthread_mutex_unlock(&keycache->cache_lock);
  }
  
  if (MYSQL_KEYCACHE_WRITE_DONE_ENABLED())
  {
    MYSQL_KEYCACHE_WRITE_DONE((ulong) (keycache->blocks_used *
                                       keycache->key_cache_block_size),
                              (ulong) (keycache->blocks_unused *
                                       keycache->key_cache_block_size));
  }
  
#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("exec",
               test_key_cache(keycache, "end of key_cache_write", 1););
#endif
  DBUG_RETURN(error);
}


/*
  Free block.

  SYNOPSIS
    free_block()
      keycache          Pointer to a key cache data structure
      block             Pointer to the block to free

  DESCRIPTION
    Remove reference to block from hash table.
    Remove block from the chain of clean blocks.
    Add block to the free list.

  NOTE
    Block must not be free (status == 0).
    Block must not be in free_block_list.
    Block must not be in the LRU ring.
    Block must not be in eviction (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH).
    Block must not be in free (BLOCK_REASSIGNED).
    Block must not be in flush (BLOCK_IN_FLUSH).
    Block must not be dirty (BLOCK_CHANGED).
    Block must not be in changed_blocks (dirty) hash.
    Block must be in file_blocks (clean) hash.
    Block must refer to a hash_link.
    Block must have a request registered on it.
*/

static void free_block(SIMPLE_KEY_CACHE_CB *keycache, BLOCK_LINK *block)
{
  KEYCACHE_THREAD_TRACE("free block");
  KEYCACHE_DBUG_PRINT("free_block",
                      ("block %u to be freed, hash_link %p  status: %u",
                       BLOCK_NUMBER(block), block->hash_link,
                       block->status));
  /*
    Assert that the block is not free already. And that it is in a clean
    state. Note that the block might just be assigned to a hash_link and
    not yet read (BLOCK_READ may not be set here). In this case a reader
    is registered in the hash_link and free_block() will wait for it
    below.
  */
  DBUG_ASSERT((block->status & BLOCK_IN_USE) &&
              !(block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH |
                                 BLOCK_REASSIGNED | BLOCK_IN_FLUSH |
                                 BLOCK_CHANGED | BLOCK_FOR_UPDATE)));
  /* Assert that the block is in a file_blocks chain. */
  DBUG_ASSERT(block->prev_changed && *block->prev_changed == block);
  /* Assert that the block is not in the LRU ring. */
  DBUG_ASSERT(!block->next_used && !block->prev_used);
  /*
    IMHO the below condition (if()) makes no sense. I can't see how it
    could be possible that free_block() is entered with a NULL hash_link
    pointer. The only place where it can become NULL is in free_block()
    (or before its first use ever, but for those blocks free_block() is
    not called). I don't remove the conditional as it cannot harm, but
    place an DBUG_ASSERT to confirm my hypothesis. Eventually the
    condition (if()) can be removed.
  */
  DBUG_ASSERT(block->hash_link && block->hash_link->block == block);
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
    /*
      The block must not have been freed by another thread. Repeat some
      checks. An additional requirement is that it must be read now
      (BLOCK_READ).
    */
    DBUG_ASSERT(block->hash_link && block->hash_link->block == block);
    DBUG_ASSERT((block->status & (BLOCK_READ | BLOCK_IN_USE |
                                  BLOCK_REASSIGNED)) &&
                !(block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH |
                                   BLOCK_IN_FLUSH | BLOCK_CHANGED |
                                   BLOCK_FOR_UPDATE)));
    DBUG_ASSERT(block->prev_changed && *block->prev_changed == block);
    DBUG_ASSERT(!block->prev_used);
    /*
      Unset BLOCK_REASSIGNED again. If we hand the block to an evicting
      thread (through unreg_request() below), other threads must not see
      this flag. They could become confused.
    */
    block->status&= ~BLOCK_REASSIGNED;
    /*
      Do not release the hash_link until the block is off all lists.
      At least not if we hand it over for eviction in unreg_request().
    */
  }

  /*
    Unregister the block request and link the block into the LRU ring.
    This enables eviction for the block. If the LRU ring was empty and
    threads are waiting for a block, then the block wil be handed over
    for eviction immediately. Otherwise we will unlink it from the LRU
    ring again, without releasing the lock in between. So decrementing
    the request counter and updating statistics are the only relevant
    operation in this case. Assert that there are no other requests
    registered.
  */
  DBUG_ASSERT(block->requests == 1);
  unreg_request(keycache, block, 0);
  /*
    Note that even without releasing the cache lock it is possible that
    the block is immediately selected for eviction by link_block() and
    thus not added to the LRU ring. In this case we must not touch the
    block any more.
  */
  if (block->status & BLOCK_IN_EVICTION)
    return;

  /* Error blocks are not put into the LRU ring. */
  if (!(block->status & BLOCK_ERROR))
  {
    /* Here the block must be in the LRU ring. Unlink it again. */
    DBUG_ASSERT(block->next_used && block->prev_used &&
                *block->prev_used == block);
    unlink_block(keycache, block);
  }
  if (block->temperature == BLOCK_WARM)
    keycache->warm_blocks--;
  block->temperature= BLOCK_COLD;

  /* Remove from file_blocks hash. */
  unlink_changed(block);

  /* Remove reference to block from hash table. */
  unlink_hash(keycache, block->hash_link);
  block->hash_link= NULL;

  block->status= 0;
  block->length= 0;
  block->offset= keycache->key_cache_block_size;
  KEYCACHE_THREAD_TRACE("free block");
  KEYCACHE_DBUG_PRINT("free_block", ("block is freed"));

  /* Enforced by unlink_changed(), but just to be sure. */
  DBUG_ASSERT(!block->next_changed && !block->prev_changed);
  /* Enforced by unlink_block(): not in LRU ring nor in free_block_list. */
  DBUG_ASSERT(!block->next_used && !block->prev_used);
  /* Insert the free block in the free list. */
  block->next_used= keycache->free_block_list;
  keycache->free_block_list= block;
  /* Keep track of the number of currently unused blocks. */
  keycache->blocks_unused++;

  /* All pending requests for this page must be resubmitted. */
  release_whole_queue(&block->wqueue[COND_FOR_SAVED]);
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

static int flush_cached_blocks(SIMPLE_KEY_CACHE_CB *keycache,
                               File file, BLOCK_LINK **cache,
                               BLOCK_LINK **end,
                               enum flush_type type)
{
  int error;
  int last_errno= 0;
  uint count= (uint) (end-cache);

  /* Don't lock the cache during the flush */
  keycache_pthread_mutex_unlock(&keycache->cache_lock);
  /*
     As all blocks referred in 'cache' are marked by BLOCK_IN_FLUSH
     we are guarunteed no thread will change them
  */
  my_qsort((uchar*) cache, count, sizeof(*cache), (qsort_cmp) cmp_sec_link);

  keycache_pthread_mutex_lock(&keycache->cache_lock);
  /*
    Note: Do not break the loop. We have registered a request on every
    block in 'cache'. These must be unregistered by free_block() or
    unreg_request().
  */
  for ( ; cache != end ; cache++)
  {
    BLOCK_LINK *block= *cache;

    KEYCACHE_DBUG_PRINT("flush_cached_blocks",
                        ("block %u to be flushed", BLOCK_NUMBER(block)));
    /*
      If the block contents is going to be changed, we abandon the flush
      for this block. flush_key_blocks_int() will restart its search and
      handle the block properly.
    */
    if (!(block->status & BLOCK_FOR_UPDATE))
    {
      /* Blocks coming here must have a certain status. */
      DBUG_ASSERT(block->hash_link);
      DBUG_ASSERT(block->hash_link->block == block);
      DBUG_ASSERT(block->hash_link->file == file);
      DBUG_ASSERT((block->status & ~BLOCK_IN_EVICTION) ==
                  (BLOCK_READ | BLOCK_IN_FLUSH | BLOCK_CHANGED | BLOCK_IN_USE));
      block->status|= BLOCK_IN_FLUSHWRITE;
      keycache_pthread_mutex_unlock(&keycache->cache_lock);
      error= my_pwrite(file, block->buffer + block->offset,
                       block->length - block->offset,
                       block->hash_link->diskpos + block->offset,
                       MYF(MY_NABP | MY_WAIT_IF_FULL));
      keycache_pthread_mutex_lock(&keycache->cache_lock);
      keycache->global_cache_write++;
      if (error)
      {
        block->status|= BLOCK_ERROR;
        if (!last_errno)
          last_errno= errno ? errno : -1;
      }
      block->status&= ~BLOCK_IN_FLUSHWRITE;
      /* Block must not have changed status except BLOCK_FOR_UPDATE. */
      DBUG_ASSERT(block->hash_link);
      DBUG_ASSERT(block->hash_link->block == block);
      DBUG_ASSERT(block->hash_link->file == file);
      DBUG_ASSERT((block->status & ~(BLOCK_FOR_UPDATE | BLOCK_IN_EVICTION)) ==
                  (BLOCK_READ | BLOCK_IN_FLUSH | BLOCK_CHANGED | BLOCK_IN_USE));
      /*
        Set correct status and link in right queue for free or later use.
        free_block() must not see BLOCK_CHANGED and it may need to wait
        for readers of the block. These should not see the block in the
        wrong hash. If not freeing the block, we need to have it in the
        right queue anyway.
      */
      link_to_file_list(keycache, block, file, 1);
    }
    block->status&= ~BLOCK_IN_FLUSH;
    /*
      Let to proceed for possible waiting requests to write to the block page.
      It might happen only during an operation to resize the key cache.
    */
    release_whole_queue(&block->wqueue[COND_FOR_SAVED]);
    /* type will never be FLUSH_IGNORE_CHANGED here */
    if (!(type == FLUSH_KEEP || type == FLUSH_FORCE_WRITE) &&
        !(block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH |
                           BLOCK_FOR_UPDATE)))
    {
      /*
        Note that a request has been registered against the block in
        flush_key_blocks_int().
      */
      free_block(keycache, block);
    }
    else
    {
      /*
        Link the block into the LRU ring if it's the last submitted
        request for the block. This enables eviction for the block.
        Note that a request has been registered against the block in
        flush_key_blocks_int().
      */
      unreg_request(keycache, block, 1);
    }

  } /* end of for ( ; cache != end ; cache++) */
  return last_errno;
}


/*
  Flush all key blocks for a file to disk, but don't do any mutex locks

  SYNOPSIS
    flush_key_blocks_int()
      keycache            pointer to a key cache data structure
      file                handler for the file to flush to
      flush_type          type of the flush

  NOTES
    This function doesn't do any mutex locks because it needs to be called both
    from flush_key_blocks and flush_all_key_blocks (the later one does the
    mutex lock in the resize_key_cache() function).

    We do only care about changed blocks that exist when the function is
    entered. We do not guarantee that all changed blocks of the file are
    flushed if more blocks change while this function is running.

  RETURN
    0   ok
    1  error
*/

static int flush_key_blocks_int(SIMPLE_KEY_CACHE_CB *keycache,
				File file, enum flush_type type)
{
  BLOCK_LINK *cache_buff[FLUSH_CACHE],**cache;
  int last_errno= 0;
  int last_errcnt= 0;
  DBUG_ENTER("flush_key_blocks_int");
  DBUG_PRINT("enter",("file: %d  blocks_used: %lu  blocks_changed: %lu",
              file, keycache->blocks_used, keycache->blocks_changed));

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_keycache",
               test_key_cache(keycache, "start of flush_key_blocks", 0););
#endif

  DBUG_ASSERT(type != FLUSH_KEEP_LAZY);
  cache= cache_buff;
  if (keycache->disk_blocks > 0 &&
      (!my_disable_flush_key_blocks || type != FLUSH_KEEP))
  {
    /* Key cache exists and flush is not disabled */
    int error= 0;
    uint count= FLUSH_CACHE;
    BLOCK_LINK **pos,**end;
    BLOCK_LINK *first_in_switch= NULL;
    BLOCK_LINK *last_in_flush;
    BLOCK_LINK *last_for_update;
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
      count= 0;
      for (block= keycache->changed_blocks[FILE_HASH(file)] ;
           block ;
           block= block->next_changed)
      {
        if ((block->hash_link->file == file) &&
            !(block->status & BLOCK_IN_FLUSH))
        {
          count++;
          KEYCACHE_DBUG_ASSERT(count<= keycache->blocks_used);
        }
      }
      /*
        Allocate a new buffer only if its bigger than the one we have.
        Assure that we always have some entries for the case that new
        changed blocks appear while we need to wait for something.
      */
      if ((count > FLUSH_CACHE) &&
          !(cache= (BLOCK_LINK**) my_malloc(sizeof(BLOCK_LINK*)*count,
                                            MYF(0))))
        cache= cache_buff;
      /*
        After a restart there could be more changed blocks than now.
        So we should not let count become smaller than the fixed buffer.
      */
      if (cache == cache_buff)
        count= FLUSH_CACHE;
    }

    /* Retrieve the blocks and write them to a buffer to be flushed */
restart:
    last_in_flush= NULL;
    last_for_update= NULL;
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
        if (!(block->status & (BLOCK_IN_FLUSH | BLOCK_FOR_UPDATE)))
        {
          /*
            Note: The special handling of BLOCK_IN_SWITCH is obsolete
            since we set BLOCK_IN_FLUSH if the eviction includes a
            flush. It can be removed in a later version.
          */
          if (!(block->status & BLOCK_IN_SWITCH))
          {
            /*
              We care only for the blocks for which flushing was not
              initiated by another thread and which are not in eviction.
              Registering a request on the block unlinks it from the LRU
              ring and protects against eviction.
            */
            reg_requests(keycache, block, 1);
            if (type != FLUSH_IGNORE_CHANGED)
            {
              /* It's not a temporary file */
              if (pos == end)
              {
                /*
                  This should happen relatively seldom. Remove the
                  request because we won't do anything with the block
                  but restart and pick it again in the next iteration.
                */
                unreg_request(keycache, block, 0);
                /*
                  This happens only if there is not enough
                  memory for the big block
                */
                if ((error= flush_cached_blocks(keycache, file, cache,
                                                end,type)))
                {
                  /* Do not loop infinitely trying to flush in vain. */
                  if ((last_errno == error) && (++last_errcnt > 5))
                    goto err;
                  last_errno= error;
                }
                /*
                  Restart the scan as some other thread might have changed
                  the changed blocks chain: the blocks that were in switch
                  state before the flush started have to be excluded
                */
                goto restart;
              }
              /*
                Mark the block with BLOCK_IN_FLUSH in order not to let
                other threads to use it for new pages and interfere with
                our sequence of flushing dirty file pages. We must not
                set this flag before actually putting the block on the
                write burst array called 'cache'.
              */
              block->status|= BLOCK_IN_FLUSH;
              /* Add block to the array for a write burst. */
              *pos++= block;
            }
            else
            {
              /* It's a temporary file */
              DBUG_ASSERT(!(block->status & BLOCK_REASSIGNED));
              /*
                free_block() must not be called with BLOCK_CHANGED. Note
                that we must not change the BLOCK_CHANGED flag outside of
                link_to_file_list() so that it is always in the correct
                queue and the *blocks_changed counters are correct.
              */
              link_to_file_list(keycache, block, file, 1);
              if (!(block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH)))
              {
                /* A request has been registered against the block above. */
                free_block(keycache, block);
              }
              else
              {
                /*
                  Link the block into the LRU ring if it's the last
                  submitted request for the block. This enables eviction
                  for the block. A request has been registered against
                  the block above.
                */
                unreg_request(keycache, block, 1);
              }
            }
          }
          else
          {
            /*
              Link the block into a list of blocks 'in switch'.

              WARNING: Here we introduce a place where a changed block
              is not in the changed_blocks hash! This is acceptable for
              a BLOCK_IN_SWITCH. Never try this for another situation.
              Other parts of the key cache code rely on changed blocks
              being in the changed_blocks hash.
            */
            unlink_changed(block);
            link_changed(block, &first_in_switch);
          }
        }
        else if (type != FLUSH_KEEP)
        {
          /*
            During the normal flush at end of statement (FLUSH_KEEP) we
            do not need to ensure that blocks in flush or update by
            other threads are flushed. They will be flushed by them
            later. In all other cases we must assure that we do not have
            any changed block of this file in the cache when this
            function returns.
          */
          if (block->status & BLOCK_IN_FLUSH)
          {
            /* Remember the last block found to be in flush. */
            last_in_flush= block;
          }
          else
          {
            /* Remember the last block found to be selected for update. */
            last_for_update= block;
          }
        }
      }
    }
    if (pos != cache)
    {
      if ((error= flush_cached_blocks(keycache, file, cache, pos, type)))
      {
        /* Do not loop inifnitely trying to flush in vain. */
        if ((last_errno == error) && (++last_errcnt > 5))
          goto err;
        last_errno= error;
      }
      /*
        Do not restart here during the normal flush at end of statement
        (FLUSH_KEEP). We have now flushed at least all blocks that were
        changed when entering this function. In all other cases we must
        assure that we do not have any changed block of this file in the
        cache when this function returns.
      */
      if (type != FLUSH_KEEP)
        goto restart;
    }
    if (last_in_flush)
    {
      /*
        There are no blocks to be flushed by this thread, but blocks in
        flush by other threads. Wait until one of the blocks is flushed.
        Re-check the condition for last_in_flush. We may have unlocked
        the cache_lock in flush_cached_blocks(). The state of the block
        could have changed.
      */
      if (last_in_flush->status & BLOCK_IN_FLUSH)
        wait_on_queue(&last_in_flush->wqueue[COND_FOR_SAVED],
                      &keycache->cache_lock);
      /* Be sure not to lose a block. They may be flushed in random order. */
      goto restart;
    }
    if (last_for_update)
    {
      /*
        There are no blocks to be flushed by this thread, but blocks for
        update by other threads. Wait until one of the blocks is updated.
        Re-check the condition for last_for_update. We may have unlocked
        the cache_lock in flush_cached_blocks(). The state of the block
        could have changed.
      */
      if (last_for_update->status & BLOCK_FOR_UPDATE)
        wait_on_queue(&last_for_update->wqueue[COND_FOR_REQUESTED],
                      &keycache->cache_lock);
      /* The block is now changed. Flush it. */
      goto restart;
    }

    /*
      Wait until the list of blocks in switch is empty. The threads that
      are switching these blocks will relink them to clean file chains
      while we wait and thus empty the 'first_in_switch' chain.
    */
    while (first_in_switch)
    {
#if defined(KEYCACHE_DEBUG)
      cnt= 0;
#endif
      wait_on_queue(&first_in_switch->wqueue[COND_FOR_SAVED],
                    &keycache->cache_lock);
#if defined(KEYCACHE_DEBUG)
      cnt++;
      KEYCACHE_DBUG_ASSERT(cnt <= keycache->blocks_used);
#endif
      /*
        Do not restart here. We have flushed all blocks that were
        changed when entering this function and were not marked for
        eviction. Other threads have now flushed all remaining blocks in
        the course of their eviction.
      */
    }

    if (! (type == FLUSH_KEEP || type == FLUSH_FORCE_WRITE))
    {
      BLOCK_LINK *last_for_update= NULL;
      BLOCK_LINK *last_in_switch= NULL;
      uint total_found= 0;
      uint found;

      /*
        Finally free all clean blocks for this file.
        During resize this may be run by two threads in parallel.
      */
      do
      {
        found= 0;
        for (block= keycache->file_blocks[FILE_HASH(file)] ;
             block ;
             block= next)
        {
          /* Remember the next block. After freeing we cannot get at it. */
          next= block->next_changed;

          /* Changed blocks cannot appear in the file_blocks hash. */
          DBUG_ASSERT(!(block->status & BLOCK_CHANGED));
          if (block->hash_link->file == file)
          {
            /* We must skip blocks that will be changed. */
            if (block->status & BLOCK_FOR_UPDATE)
            {
              last_for_update= block;
              continue;
            }

            /*
              We must not free blocks in eviction (BLOCK_IN_EVICTION |
              BLOCK_IN_SWITCH) or blocks intended to be freed
              (BLOCK_REASSIGNED).
            */
            if (!(block->status & (BLOCK_IN_EVICTION | BLOCK_IN_SWITCH |
                                   BLOCK_REASSIGNED)))
            {
              struct st_hash_link *UNINIT_VAR(next_hash_link);
              my_off_t UNINIT_VAR(next_diskpos);
              File UNINIT_VAR(next_file);
              uint UNINIT_VAR(next_status);
              uint UNINIT_VAR(hash_requests);

              total_found++;
              found++;
              KEYCACHE_DBUG_ASSERT(found <= keycache->blocks_used);

              /*
                Register a request. This unlinks the block from the LRU
                ring and protects it against eviction. This is required
                by free_block().
              */
              reg_requests(keycache, block, 1);

              /*
                free_block() may need to wait for readers of the block.
                This is the moment where the other thread can move the
                'next' block from the chain. free_block() needs to wait
                if there are requests for the block pending.
              */
              if (next && (hash_requests= block->hash_link->requests))
              {
                /* Copy values from the 'next' block and its hash_link. */
                next_status=    next->status;
                next_hash_link= next->hash_link;
                next_diskpos=   next_hash_link->diskpos;
                next_file=      next_hash_link->file;
                DBUG_ASSERT(next == next_hash_link->block);
              }

              free_block(keycache, block);
              /*
                If we had to wait and the state of the 'next' block
                changed, break the inner loop. 'next' may no longer be
                part of the current chain.

                We do not want to break the loop after every free_block(),
                not even only after waits. The chain might be quite long
                and contain blocks for many files. Traversing it again and
                again to find more blocks for this file could become quite
                inefficient.
              */
              if (next && hash_requests &&
                  ((next_status    != next->status) ||
                   (next_hash_link != next->hash_link) ||
                   (next_file      != next_hash_link->file) ||
                   (next_diskpos   != next_hash_link->diskpos) ||
                   (next           != next_hash_link->block)))
                break;
            }
            else
            {
              last_in_switch= block;
            }
          }
        } /* end for block in file_blocks */
      } while (found);

      /*
        If any clean block has been found, we may have waited for it to
        become free. In this case it could be possible that another clean
        block became dirty. This is possible if the write request existed
        before the flush started (BLOCK_FOR_UPDATE). Re-check the hashes.
      */
      if (total_found)
        goto restart;

      /*
        To avoid an infinite loop, wait until one of the blocks marked
        for update is updated.
      */
      if (last_for_update)
      {
        /* We did not wait. Block must not have changed status. */
        DBUG_ASSERT(last_for_update->status & BLOCK_FOR_UPDATE);
        wait_on_queue(&last_for_update->wqueue[COND_FOR_REQUESTED],
                      &keycache->cache_lock);
        goto restart;
      }

      /*
        To avoid an infinite loop wait until one of the blocks marked
        for eviction is switched.
      */
      if (last_in_switch)
      {
        /* We did not wait. Block must not have changed status. */
        DBUG_ASSERT(last_in_switch->status & (BLOCK_IN_EVICTION |
                                              BLOCK_IN_SWITCH |
                                              BLOCK_REASSIGNED));
        wait_on_queue(&last_in_switch->wqueue[COND_FOR_SAVED],
                      &keycache->cache_lock);
        goto restart;
      }

    } /* if (! (type == FLUSH_KEEP || type == FLUSH_FORCE_WRITE)) */

  } /* if (keycache->disk_blocks > 0 */

#ifndef DBUG_OFF
  DBUG_EXECUTE("check_keycache",
               test_key_cache(keycache, "end of flush_key_blocks", 0););
#endif
err:
  if (cache != cache_buff)
    my_free(cache);
  if (last_errno)
    errno=last_errno;                /* Return first error */
  DBUG_RETURN(last_errno != 0);
}


/*
  Flush all blocks for a file from key buffers of a simple key cache 

  SYNOPSIS

    flush_simple_key_blocks()
    keycache            pointer to the control block of a simple key cache
    file                handler for the file to flush to
    file_extra          maps of key cache partitions containing 
                        dirty pages from file (not used)         
    flush_type          type of the flush operation

  DESCRIPTION
    This function is the implementation of the flush_key_blocks interface
    function that is employed by simple (non-partitioned) key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type S_KEY_CACHE_CB for a simple key
    cache.
    In a general case the function flushes the data from all dirty key
    buffers related to the file 'file' into this file. The function does
    exactly this if the value of the parameter type is FLUSH_KEEP. If the
    value of this parameter is FLUSH_RELEASE, the function additionally 
    releases the key buffers containing data from 'file' for new usage.
    If the value of the parameter type is FLUSH_IGNORE_CHANGED the function
    just releases the key buffers containing data from 'file'.  
    The parameter file_extra currently is not used by this function.
      
  RETURN
    0   ok
    1  error

  NOTES
    This implementation exploits the fact that the function is called only
    when a thread has got an exclusive lock for the key file.
*/

static
int flush_simple_key_cache_blocks(SIMPLE_KEY_CACHE_CB *keycache,
                                  File file,
                                  void *file_extra __attribute__((unused)),
                                  enum flush_type type)
{
  int res= 0;
  DBUG_ENTER("flush_key_blocks");
  DBUG_PRINT("enter", ("keycache: 0x%lx", (long) keycache));

  if (!keycache->key_cache_inited)
    DBUG_RETURN(0);

  keycache_pthread_mutex_lock(&keycache->cache_lock);
  /* While waiting for lock, keycache could have been ended. */
  if (keycache->disk_blocks > 0)
  {
    inc_counter_for_resize_op(keycache);
    res= flush_key_blocks_int(keycache, file, type);
    dec_counter_for_resize_op(keycache);
  }
  keycache_pthread_mutex_unlock(&keycache->cache_lock);
  DBUG_RETURN(res);
}


/*
  Flush all blocks in the key cache to disk.

  SYNOPSIS
    flush_all_key_blocks()
      keycache                  pointer to key cache root structure

  DESCRIPTION

    Flushing of the whole key cache is done in two phases.

    1. Flush all changed blocks, waiting for them if necessary. Loop
    until there is no changed block left in the cache.

    2. Free all clean blocks. Normally this means free all blocks. The
    changed blocks were flushed in phase 1 and became clean. However we
    may need to wait for blocks that are read by other threads. While we
    wait, a clean block could become changed if that operation started
    before the resize operation started. To be safe we must restart at
    phase 1.

    When we can run through the changed_blocks and file_blocks hashes
    without finding a block any more, then we are done.

    Note that we hold keycache->cache_lock all the time unless we need
    to wait for something.

  RETURN
    0           OK
    != 0        Error
*/

static int flush_all_key_blocks(SIMPLE_KEY_CACHE_CB *keycache)
{
  BLOCK_LINK    *block;
  uint          total_found;
  uint          found;
  uint          idx;
  DBUG_ENTER("flush_all_key_blocks");

  do
  {
    mysql_mutex_assert_owner(&keycache->cache_lock);
    total_found= 0;

    /*
      Phase1: Flush all changed blocks, waiting for them if necessary.
      Loop until there is no changed block left in the cache.
    */
    do
    {
      found= 0;
      /* Step over the whole changed_blocks hash array. */
      for (idx= 0; idx < CHANGED_BLOCKS_HASH; idx++)
      {
        /*
          If an array element is non-empty, use the first block from its
          chain to find a file for flush. All changed blocks for this
          file are flushed. So the same block will not appear at this
          place again with the next iteration. New writes for blocks are
          not accepted during the flush. If multiple files share the
          same hash bucket, one of them will be flushed per iteration
          of the outer loop of phase 1.
        */
        if ((block= keycache->changed_blocks[idx]))
        {
          found++;
          /*
            Flush dirty blocks but do not free them yet. They can be used
            for reading until all other blocks are flushed too.
          */
          if (flush_key_blocks_int(keycache, block->hash_link->file,
                                   FLUSH_FORCE_WRITE))
            DBUG_RETURN(1);
        }
      }

    } while (found);

    /*
      Phase 2: Free all clean blocks. Normally this means free all
      blocks. The changed blocks were flushed in phase 1 and became
      clean. However we may need to wait for blocks that are read by
      other threads. While we wait, a clean block could become changed
      if that operation started before the resize operation started. To
      be safe we must restart at phase 1.
    */
    do
    {
      found= 0;
      /* Step over the whole file_blocks hash array. */
      for (idx= 0; idx < CHANGED_BLOCKS_HASH; idx++)
      {
        /*
          If an array element is non-empty, use the first block from its
          chain to find a file for flush. All blocks for this file are
          freed. So the same block will not appear at this place again
          with the next iteration. If multiple files share the
          same hash bucket, one of them will be flushed per iteration
          of the outer loop of phase 2.
        */
        if ((block= keycache->file_blocks[idx]))
        {
          total_found++;
          found++;
          if (flush_key_blocks_int(keycache, block->hash_link->file,
                                   FLUSH_RELEASE))
            DBUG_RETURN(1);
        }
      }

    } while (found);

    /*
      If any clean block has been found, we may have waited for it to
      become free. In this case it could be possible that another clean
      block became dirty. This is possible if the write request existed
      before the resize started (BLOCK_FOR_UPDATE). Re-check the hashes.
    */
  } while (total_found);

#ifndef DBUG_OFF
  /* Now there should not exist any block any more. */
  for (idx= 0; idx < CHANGED_BLOCKS_HASH; idx++)
  {
    DBUG_ASSERT(!keycache->changed_blocks[idx]);
    DBUG_ASSERT(!keycache->file_blocks[idx]);
  }
#endif

  DBUG_RETURN(0);
}


/*
  Reset the counters of a simple key cache

  SYNOPSIS
    reset_simple_key_cache_counters()
    name                the name of a key cache
    keycache            pointer to the control block of a simple key cache

  DESCRIPTION
    This function is the implementation of the reset_key_cache_counters
    interface function that is employed by simple (non-partitioned) key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type S_KEY_CACHE_CB for a simple key cache.
    This function resets the values of all statistical counters for the key
    cache to 0.
    The parameter name is currently not used.

  RETURN
    0 on success (always because it can't fail)
*/

static
int reset_simple_key_cache_counters(const char *name __attribute__((unused)),
                                    SIMPLE_KEY_CACHE_CB *keycache)
{
  DBUG_ENTER("reset_simple_key_cache_counters");
  if (!keycache->key_cache_inited)
  {
    DBUG_PRINT("info", ("Key cache %s not initialized.", name));
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("Resetting counters for key cache %s.", name));

  keycache->global_blocks_changed= 0;   /* Key_blocks_not_flushed */
  keycache->global_cache_r_requests= 0; /* Key_read_requests */
  keycache->global_cache_read= 0;       /* Key_reads */
  keycache->global_cache_w_requests= 0; /* Key_write_requests */
  keycache->global_cache_write= 0;      /* Key_writes */
  DBUG_RETURN(0);
}


#ifndef DBUG_OFF
/*
  Test if disk-cache is ok
*/
static
void test_key_cache(SIMPLE_KEY_CACHE_CB *keycache __attribute__((unused)),
                    const char *where __attribute__((unused)),
                    my_bool lock __attribute__((unused)))
{
  /* TODO */
}
#endif

#if defined(KEYCACHE_TIMEOUT)

#define KEYCACHE_DUMP_FILE  "keycache_dump.txt"
#define MAX_QUEUE_LEN  100


static void keycache_dump(SIMPLE_KEY_CACHE_CB *keycache)
{
  FILE *keycache_dump_file=fopen(KEYCACHE_DUMP_FILE, "w");
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


static int keycache_pthread_cond_wait(mysql_cond_t *cond,
                                      mysql_mutex_t *mutex)
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
 /*
   timeval uses microseconds.
   timespec uses nanoseconds.
   1 nanosecond = 1000 micro seconds
 */
  timeout.tv_nsec= now.tv_usec * 1000;
  KEYCACHE_THREAD_TRACE_END("started waiting");
#if defined(KEYCACHE_DEBUG)
  cnt++;
  if (cnt % 100 == 0)
    fprintf(keycache_debug_log, "waiting...\n");
    fflush(keycache_debug_log);
#endif
  rc= mysql_cond_timedwait(cond, mutex, &timeout);
  KEYCACHE_THREAD_TRACE_BEGIN("finished waiting");
  if (rc == ETIMEDOUT || rc == ETIME)
  {
#if defined(KEYCACHE_DEBUG)
    fprintf(keycache_debug_log,"aborted by keycache timeout\n");
    fclose(keycache_debug_log);
    abort();
#endif
    keycache_dump();
  }

#if defined(KEYCACHE_DEBUG)
  KEYCACHE_DBUG_ASSERT(rc != ETIMEDOUT);
#else
  assert(rc != ETIMEDOUT);
#endif
  return rc;
}
#else
#if defined(KEYCACHE_DEBUG)
static int keycache_pthread_cond_wait(mysql_cond_t *cond,
                                      mysql_mutex_t *mutex)
{
  int rc;
  KEYCACHE_THREAD_TRACE_END("started waiting");
  rc= mysql_cond_wait(cond, mutex);
  KEYCACHE_THREAD_TRACE_BEGIN("finished waiting");
  return rc;
}
#endif
#endif /* defined(KEYCACHE_TIMEOUT) && !defined(__WIN__) */

#if defined(KEYCACHE_DEBUG)


static int keycache_pthread_mutex_lock(mysql_mutex_t *mutex)
{
  int rc;
  rc= mysql_mutex_lock(mutex);
  KEYCACHE_THREAD_TRACE_BEGIN("");
  return rc;
}


static void keycache_pthread_mutex_unlock(mysql_mutex_t *mutex)
{
  KEYCACHE_THREAD_TRACE_END("");
  mysql_mutex_unlock(mutex);
}


static int keycache_pthread_cond_signal(mysql_cond_t *cond)
{
  int rc;
  KEYCACHE_THREAD_TRACE("signal");
  rc= mysql_cond_signal(cond);
  return rc;
}


#if defined(KEYCACHE_DEBUG_LOG)


static void keycache_debug_print(const char * fmt,...)
{
  va_list args;
  va_start(args,fmt);
  if (keycache_debug_log)
  {
    (void) vfprintf(keycache_debug_log, fmt, args);
    (void) fputc('\n',keycache_debug_log);
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

#if !defined(DBUG_OFF)
#define F_B_PRT(_f_, _v_) DBUG_PRINT("assert_fail", (_f_, _v_))

static int fail_block(BLOCK_LINK *block)
{
  F_B_PRT("block->next_used:    %lx\n", (ulong) block->next_used);
  F_B_PRT("block->prev_used:    %lx\n", (ulong) block->prev_used);
  F_B_PRT("block->next_changed: %lx\n", (ulong) block->next_changed);
  F_B_PRT("block->prev_changed: %lx\n", (ulong) block->prev_changed);
  F_B_PRT("block->hash_link:    %lx\n", (ulong) block->hash_link);
  F_B_PRT("block->status:       %u\n", block->status);
  F_B_PRT("block->length:       %u\n", block->length);
  F_B_PRT("block->offset:       %u\n", block->offset);
  F_B_PRT("block->requests:     %u\n", block->requests);
  F_B_PRT("block->temperature:  %u\n", block->temperature);
  return 0; /* Let the assert fail. */
}

static int fail_hlink(HASH_LINK *hlink)
{
  F_B_PRT("hlink->next:    %lx\n", (ulong) hlink->next);
  F_B_PRT("hlink->prev:    %lx\n", (ulong) hlink->prev);
  F_B_PRT("hlink->block:   %lx\n", (ulong) hlink->block);
  F_B_PRT("hlink->diskpos: %lu\n", (ulong) hlink->diskpos);
  F_B_PRT("hlink->file:    %d\n", hlink->file);
  return 0; /* Let the assert fail. */
}

static int cache_empty(SIMPLE_KEY_CACHE_CB *keycache)
{
  int errcnt= 0;
  int idx;
  if (keycache->disk_blocks <= 0)
    return 1;
  for (idx= 0; idx < keycache->disk_blocks; idx++)
  {
    BLOCK_LINK *block= keycache->block_root + idx;
    if (block->status || block->requests || block->hash_link)
    {
      fprintf(stderr, "block index: %u\n", idx);
      fail_block(block);
      errcnt++;
    }
  }
  for (idx= 0; idx < keycache->hash_links; idx++)
  {
    HASH_LINK *hash_link= keycache->hash_link_root + idx;
    if (hash_link->requests || hash_link->block)
    {
      fprintf(stderr, "hash_link index: %u\n", idx);
      fail_hlink(hash_link);
      errcnt++;
    }
  }
  if (errcnt)
  {
    fprintf(stderr, "blocks: %d  used: %lu\n",
            keycache->disk_blocks, keycache->blocks_used);
    fprintf(stderr, "hash_links: %d  used: %d\n",
            keycache->hash_links, keycache->hash_links_used);
    fprintf(stderr, "\n");
  }
  return !errcnt;
}
#endif


/*
  Get statistics for a simple key cache

  SYNOPSIS
    get_simple_key_cache_statistics()
    keycache            pointer to the control block of a simple key cache
    partition_no        partition number (not used)
    key_cache_stats OUT pointer to the structure for the returned statistics

  DESCRIPTION
    This function is the implementation of the get_key_cache_statistics
    interface function that is employed by simple (non-partitioned) key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type SIMPLE_KEY_CACHE_CB for a simple key
    cache. This function returns the statistical data for the key cache.
    The parameter partition_no is not used by this function.

  RETURN
    none
*/

static
void get_simple_key_cache_statistics(SIMPLE_KEY_CACHE_CB *keycache, 
                                     uint partition_no __attribute__((unused)), 
                                     KEY_CACHE_STATISTICS *keycache_stats)
{
  DBUG_ENTER("simple_get_key_cache_statistics");

  keycache_stats->mem_size= (longlong) keycache->key_cache_mem_size;
  keycache_stats->block_size= (longlong) keycache->key_cache_block_size;
  keycache_stats->blocks_used= keycache->blocks_used;
  keycache_stats->blocks_unused= keycache->blocks_unused;
  keycache_stats->blocks_changed= keycache->global_blocks_changed;
  keycache_stats->blocks_warm= keycache->warm_blocks;
  keycache_stats->read_requests= keycache->global_cache_r_requests;
  keycache_stats->reads= keycache->global_cache_read;
  keycache_stats->write_requests= keycache->global_cache_w_requests;
  keycache_stats->writes= keycache->global_cache_write;
  DBUG_VOID_RETURN;  
}


/* 
  The array of pointer to the key cache interface functions used for simple
  key caches. Any simple key cache objects including those incorporated into
  partitioned keys caches exploit this array.

  The current implementation of these functions allows to call them from 
  the MySQL server code directly. We don't do it though. 
*/
   
static KEY_CACHE_FUNCS simple_key_cache_funcs =
{
  (INIT_KEY_CACHE) init_simple_key_cache,
  (RESIZE_KEY_CACHE) resize_simple_key_cache,
  (CHANGE_KEY_CACHE_PARAM) change_simple_key_cache_param,      
  (KEY_CACHE_READ) simple_key_cache_read,
  (KEY_CACHE_INSERT) simple_key_cache_insert,
  (KEY_CACHE_WRITE) simple_key_cache_write,
  (FLUSH_KEY_BLOCKS) flush_simple_key_cache_blocks, 
  (RESET_KEY_CACHE_COUNTERS) reset_simple_key_cache_counters, 
  (END_KEY_CACHE) end_simple_key_cache, 
  (GET_KEY_CACHE_STATISTICS) get_simple_key_cache_statistics,
};


/****************************************************************************** 
  Partitioned Key Cache Module

  The module contains implementations of all key cache interface functions
  employed by partitioned key caches. 

  A partitioned key cache is a collection of structures for simple key caches
  called key cache partitions. Any page from a file can be placed into a buffer
  of only one partition. The number of the partition is calculated from
  the file number and the position of the page in the file, and it's always the
  same for the page. The function that maps pages into partitions takes care
  of even distribution of pages among partitions.

  Partition key cache mitigate one of the major problem of simple key cache:
  thread contention for key cache lock (mutex). Every call of a key cache 
  interface function must acquire this lock. So threads compete for this lock
  even in the case when they have acquired shared locks for the file and
  pages they want read from are in the key cache buffers.
  When working with a partitioned key cache any key cache interface function
  that needs only one page has to acquire the key cache lock only for the
  partition the page is ascribed to. This makes the chances for threads not
  compete for the same key cache lock better. Unfortunately if we use a
  partitioned key cache with N partitions for B-tree indexes we can't say
  that the chances becomes N times less. The fact is that any index lookup
  operation requires reading from the root page that, for any index, is always
  ascribed to the same partition. To resolve this problem we should have
  employed more sophisticated mechanisms of working with root pages.

  Currently the number of partitions in a partitioned key cache is limited 
  by 64. We could increase this limit. Simultaneously we would have to increase
  accordingly the size of the bitmap dirty_part_map from the MYISAM_SHARE
  structure.
     
******************************************************************************/

/* Control block for a partitioned key cache */

typedef struct st_partitioned_key_cache_cb
{
  my_bool key_cache_inited;     /*<=> control block is allocated            */ 
  SIMPLE_KEY_CACHE_CB **partition_array; /* the key cache partitions        */  
  size_t key_cache_mem_size;    /* specified size of the cache memory       */
  uint key_cache_block_size;    /* size of the page buffer of a cache block */ 
  uint partitions;              /* number of partitions in the key cache    */
} PARTITIONED_KEY_CACHE_CB;

static
void end_partitioned_key_cache(PARTITIONED_KEY_CACHE_CB *keycache,
                               my_bool cleanup);

static int
reset_partitioned_key_cache_counters(const char *name,
                                     PARTITIONED_KEY_CACHE_CB *keycache);

/*
  Determine the partition to which the index block to read is ascribed

  SYNOPSIS
    get_key_cache_partition()
    keycache            pointer to the control block of a partitioned key cache
    file                handler for the file for the block of data to be read
    filepos             position of the block of data in the file

  DESCRIPTION
    The function determines the number of the partition in whose buffer the 
    block from 'file' at the position filepos has to be placed for reading.
    The function returns the control block of the simple key cache for this
    partition to the caller.

  RETURN VALUE
    The pointer to the control block of the partition to which the specified
    file block is ascribed.
*/

static 
SIMPLE_KEY_CACHE_CB *
get_key_cache_partition(PARTITIONED_KEY_CACHE_CB *keycache, 
                        File file, my_off_t filepos)
{
  uint i= KEYCACHE_BASE_EXPR(file, filepos) % keycache->partitions;
  return keycache->partition_array[i];
}


/*
  Determine the partition to which the index block to write is ascribed

  SYNOPSIS
    get_key_cache_partition()
    keycache            pointer to the control block of a partitioned key cache
    file                handler for the file for the block of data to be read
    filepos             position of the block of data in the file
    dirty_part_map      pointer to the bitmap of dirty partitions for the file

  DESCRIPTION
    The function determines the number of the partition in whose buffer the 
    block from 'file' at the position filepos has to be placed for writing and
    marks the partition as dirty in the dirty_part_map bitmap.
    The function returns the control block of the simple key cache for this
    partition to the caller.

  RETURN VALUE
    The pointer to the control block of the partition to which the specified
    file block is ascribed.
*/

static SIMPLE_KEY_CACHE_CB 
*get_key_cache_partition_for_write(PARTITIONED_KEY_CACHE_CB *keycache, 
                                   File file, my_off_t filepos,
                                   ulonglong* dirty_part_map)
{
  uint i= KEYCACHE_BASE_EXPR( file, filepos) % keycache->partitions;
  *dirty_part_map|= 1ULL << i; 
  return keycache->partition_array[i];
}


/*
  Initialize a partitioned key cache

  SYNOPSIS
    init_partitioned_key_cache()
    keycache            pointer to the control block of a partitioned key cache
    key_cache_block_size    size of blocks to keep cached data
    use_mem             total memory to use for all key cache partitions 
    division_limit      division limit (may be zero)
    age_threshold       age threshold (may be zero)

  DESCRIPTION
    This function is the implementation of the init_key_cache interface function
    that is employed by partitioned key caches.
    The function builds and initializes an array of simple key caches, and then
    initializes the control block structure of the type PARTITIONED_KEY_CACHE_CB
    that is used for a partitioned key cache. The parameter keycache is
    supposed to point to this structure. The number of partitions in the
    partitioned key cache to be built must be passed through the field
    'partitions' of this structure. The parameter key_cache_block_size specifies
    the size of the  blocks in the the simple key caches to be built.
    The parameters division_limit and  age_threshold determine the initial
    values of those characteristics of the simple key caches that are used for
    midpoint insertion strategy. The parameter use_mem specifies the total
    amount of memory to be allocated for the key cache blocks in all simple key
    caches and for all auxiliary structures.       

  RETURN VALUE
    total number of blocks in key cache partitions, if successful,
    <= 0 - otherwise.

  NOTES
    If keycache->key_cache_inited != 0 then we assume that the memory for
    the array of partitions has been already allocated.

    It's assumed that no two threads call this function simultaneously
    referring to the same key cache handle.
*/

static
int init_partitioned_key_cache(PARTITIONED_KEY_CACHE_CB *keycache,
                               uint key_cache_block_size,
                               size_t use_mem, uint division_limit,
                               uint age_threshold)
{
  int i;
  size_t mem_per_cache;
  size_t mem_decr;
  int cnt;
  SIMPLE_KEY_CACHE_CB *partition;
  SIMPLE_KEY_CACHE_CB **partition_ptr;
  uint partitions= keycache->partitions;
  int blocks= 0;
  DBUG_ENTER("partitioned_init_key_cache");

  keycache->key_cache_block_size = key_cache_block_size;

  if (keycache->key_cache_inited)
    partition_ptr= keycache->partition_array;
  else
  {
    if(!(partition_ptr=
       (SIMPLE_KEY_CACHE_CB **) my_malloc(sizeof(SIMPLE_KEY_CACHE_CB *) *
                                          partitions, MYF(MY_WME))))
      DBUG_RETURN(-1);
    bzero(partition_ptr, sizeof(SIMPLE_KEY_CACHE_CB *) * partitions);
    keycache->partition_array= partition_ptr;
  }

  mem_per_cache = use_mem / partitions;
  mem_decr= mem_per_cache / 5;

  for (i= 0; i < (int) partitions; i++)
  {
    my_bool key_cache_inited= keycache->key_cache_inited;
    if (key_cache_inited)
      partition= *partition_ptr;
    else
    {
      if (!(partition=
              (SIMPLE_KEY_CACHE_CB *)  my_malloc(sizeof(SIMPLE_KEY_CACHE_CB),
						 MYF(MY_WME))))
        continue;
      partition->key_cache_inited= 0;
    }

    cnt= init_simple_key_cache(partition, key_cache_block_size, mem_per_cache, 
			       division_limit, age_threshold);
    if (cnt <= 0)
    {
      end_simple_key_cache(partition, 1);
      if (!key_cache_inited)
      {
        my_free(partition);
        partition= 0;
      }
      if ((i == 0 && cnt < 0) || i > 0)
      {
        /* 
          Here we have two cases: 
            1. i == 0 and cnt < 0
            cnt < 0 => mem_per_cache is not big enough to allocate minimal
            number of key blocks in the key cache of the partition.
            Decrease the the number of the partitions by 1 and start again.
            2. i > 0 
            There is not enough memory for one of the succeeding partitions.
            Just skip this partition decreasing the number of partitions in
            the key cache by one.
          Do not change the value of mem_per_cache in both cases.
	*/
        if (key_cache_inited)
	{
          my_free(partition);
          partition= 0;
          if(key_cache_inited) 
            memmove(partition_ptr, partition_ptr+1, 
                    sizeof(partition_ptr)*(partitions-i-1));
	}
        if (!--partitions)
          break;
      }
      else
      {
        /*
          We come here when i == 0 && cnt == 0.
          cnt == 0 => the memory allocator fails to allocate a block of
          memory of the size mem_per_cache. Decrease the value of
          mem_per_cache  without changing the current number of partitions
          and start again. Make sure that such a decrease may happen not
          more than 5 times in total.
	*/
        if (use_mem <= mem_decr)
          break;
        use_mem-= mem_decr;
      }
      i--;
      mem_per_cache= use_mem/partitions;
      continue;
    }
    else
    {
      blocks+= cnt;
      *partition_ptr++= partition;
    }
  } 

  keycache->partitions= partitions= partition_ptr-keycache->partition_array;
  keycache->key_cache_mem_size= mem_per_cache * partitions;
  for (i= 0; i < (int) partitions; i++)
    keycache->partition_array[i]->hash_factor= partitions;
  
  keycache->key_cache_inited= 1;

  if (!partitions)
    blocks= -1;

  DBUG_RETURN(blocks);
} 


/*
  Resize a partitioned key cache

  SYNOPSIS
    resize_partitioned_key_cache()
    keycache            pointer to the control block of a partitioned key cache
    key_cache_block_size    size of blocks to keep cached data
    use_mem             total memory to use for the new key cache
    division_limit      new division limit (if not zero)
    age_threshold       new age threshold (if not zero)

  DESCRIPTION
    This function is the implementation of the resize_key_cache interface
    function that is employed by partitioned key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type PARTITIONED_KEY_CACHE_CB for the
    partitioned key cache to be resized. 
    The parameter key_cache_block_size specifies the new size of the blocks in
    the simple key caches that comprise the partitioned key cache.
    The parameters division_limit and age_threshold determine the new initial
    values of those characteristics of the simple key cache that are used for
    midpoint insertion strategy. The parameter use-mem specifies the total
    amount of  memory to be allocated for the key cache blocks in all new
    simple key caches and for all auxiliary structures.

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.

  NOTES.
    The function first calls prepare_resize_simple_key_cache for each simple
    key cache effectively flushing all dirty pages from it and destroying
    the key cache. Then init_partitioned_key_cache is called. This call builds
    a new array of simple key caches containing the same number of elements
    as the old one. After this the function calls the function
    finish_resize_simple_key_cache for each simple key cache from this array. 

    This implementation doesn't block the calls and executions of other
    functions from the key cache interface. However it assumes that the
    calls of resize_partitioned_key_cache itself are serialized.
*/

static
int resize_partitioned_key_cache(PARTITIONED_KEY_CACHE_CB *keycache, 
                                 uint key_cache_block_size,
		                 size_t use_mem, uint division_limit,
		                 uint age_threshold)
{
  uint i;
  uint partitions= keycache->partitions;
  my_bool cleanup= use_mem == 0;
  int blocks= -1;
  int err= 0;
  DBUG_ENTER("partitioned_resize_key_cache");
  if (cleanup)
  {
    end_partitioned_key_cache(keycache, 0);
    DBUG_RETURN(-1);
  }
  for (i= 0; i < partitions; i++)
  {
    err|= prepare_resize_simple_key_cache(keycache->partition_array[i], 1);
  }
  if (!err) 
    blocks= init_partitioned_key_cache(keycache, key_cache_block_size,
                                       use_mem, division_limit, age_threshold);
  if (blocks > 0)
  {
    for (i= 0; i < partitions; i++)
    {
      finish_resize_simple_key_cache(keycache->partition_array[i], 1);
    }
  }
  DBUG_RETURN(blocks);
}


/*
  Change key cache parameters of a partitioned key cache

  SYNOPSIS
    partitioned_change_key_cache_param()
    keycache            pointer to the control block of a partitioned key cache
    division_limit      new division limit (if not zero)
    age_threshold       new age threshold (if not zero)

  DESCRIPTION
    This function is the implementation of the change_key_cache_param interface
    function that is employed by partitioned key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type PARTITIONED_KEY_CACHE_CB for the simple
    key cache where new values of the division limit and the age threshold used
    for midpoint insertion strategy are to be set.  The parameters
    division_limit and age_threshold provide these new values.

  RETURN VALUE
    none

  NOTES
    The function just calls change_simple_key_cache_param for each element from
    the array of simple caches that comprise the partitioned key cache. 
*/

static
void change_partitioned_key_cache_param(PARTITIONED_KEY_CACHE_CB *keycache,
                                        uint division_limit,
                                        uint age_threshold)
{
  uint i;
  uint partitions= keycache->partitions;
  DBUG_ENTER("partitioned_change_key_cache_param");
  for (i= 0; i < partitions; i++)
  {
    change_simple_key_cache_param(keycache->partition_array[i], division_limit,
                                  age_threshold);
  }
  DBUG_VOID_RETURN;
}


/*
  Destroy a partitioned key cache 

  SYNOPSIS
    end_partitioned_key_cache()
    keycache            pointer to the control block of a partitioned key cache
    cleanup             <=> complete free (free also control block structures
                            for all simple key caches)

  DESCRIPTION
    This function is the implementation of the end_key_cache interface
    function that is employed by partitioned key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type PARTITIONED_KEY_CACHE_CB for the
    partitioned key cache to be destroyed.
    The function frees the memory allocated for the cache blocks and
    auxiliary structures used by simple key caches that comprise the
    partitioned key cache. If the value of the parameter cleanup is TRUE 
    then even the memory used for control blocks of the simple key caches
    and the array of pointers to them are freed.

  RETURN VALUE
    none
*/

static
void end_partitioned_key_cache(PARTITIONED_KEY_CACHE_CB *keycache,
                               my_bool cleanup)
{
  uint i;
  uint partitions= keycache->partitions;
  DBUG_ENTER("partitioned_end_key_cache");
  DBUG_PRINT("enter", ("key_cache: 0x%lx", (long) keycache));

  for (i= 0; i < partitions; i++)
  {
    end_simple_key_cache(keycache->partition_array[i], cleanup);
  }
  if (cleanup)
  {
    for (i= 0; i < partitions; i++)
      my_free(keycache->partition_array[i]);
    my_free(keycache->partition_array);
    keycache->key_cache_inited= 0;
  }
  DBUG_VOID_RETURN;
}


/*
  Read a block of data from a partitioned key cache into a buffer

  SYNOPSIS

    partitioned_key_cache_read()
    keycache            pointer to the control block of a partitioned key cache  
    file                handler for the file for the block of data to be read
    filepos             position of the block of data in the file
    level               determines the weight of the data
    buff                buffer to where the data must be placed
    length              length of the buffer
    block_length        length of the read data from a key cache block 
    return_buffer       return pointer to the key cache buffer with the data

  DESCRIPTION
    This function is the implementation of the key_cache_read interface
    function that is employed by partitioned key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type PARTITIONED_KEY_CACHE_CB for a
    partitioned key cache.
    In a general case the function reads a block of data from the key cache
    into the buffer buff of the size specified by the parameter length. The
    beginning of the  block of data to be read is  specified by the parameters
    file and filepos. The length of the read data is the same as the length
    of the buffer. The data is read into the buffer in key_cache_block_size
    increments. To read each portion the function first finds out in what
    partition of the key cache this portion(page) is to be saved, and calls
    simple_key_cache_read with the pointer to the corresponding simple key as
    its first parameter. 
    If the parameter return_buffer is not ignored and its value is TRUE, and 
    the data to be read of the specified size block_length can be read from one
    key cache buffer, then the function returns a pointer to the data in the
    key cache buffer.
    The function takes into account parameters block_length and return buffer
    only in a single-threaded environment.
    The parameter 'level' is used only by the midpoint insertion strategy 
    when the data or its portion cannot be found in the key cache. 
   
  RETURN VALUE
    Returns address from where the data is placed if successful, 0 - otherwise.
*/

static
uchar *partitioned_key_cache_read(PARTITIONED_KEY_CACHE_CB *keycache,
                                  File file, my_off_t filepos, int level,
                                  uchar *buff, uint length,
                                  uint block_length __attribute__((unused)),
                                  int return_buffer __attribute__((unused)))
{
  uint r_length;
  uint offset= (uint) (filepos % keycache->key_cache_block_size);
  uchar *start= buff;
  DBUG_ENTER("partitioned_key_cache_read");
  DBUG_PRINT("enter", ("fd: %u  pos: %lu  length: %u",
               (uint) file, (ulong) filepos, length));


  /* Read data in key_cache_block_size increments */
  do
  {
    SIMPLE_KEY_CACHE_CB *partition= get_key_cache_partition(keycache, 
                                                            file, filepos);
    uchar *ret_buff= 0;
    r_length= length;
    set_if_smaller(r_length, keycache->key_cache_block_size - offset);
    ret_buff= simple_key_cache_read((void *) partition, 
                                    file, filepos, level,
                                    buff, r_length,
                                    block_length, return_buffer);
    if (ret_buff == 0) 
      DBUG_RETURN(0);
    filepos+= r_length;
    buff+= r_length;
    offset= 0;
  } while ((length-= r_length));
  
  DBUG_RETURN(start);
}


/*
  Insert a block of file data from a buffer into a partitioned key cache

  SYNOPSIS
    partitioned_key_cache_insert()
    keycache            pointer to the control block of a partitioned key cache 
    file                handler for the file to insert data from
    filepos             position of the block of data in the file to insert
    level               determines the weight of the data
    buff                buffer to read data from
    length              length of the data in the buffer

  DESCRIPTION
    This function is the implementation of the key_cache_insert interface
    function that is employed by partitioned key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type PARTITIONED_KEY_CACHE_CB for a
    partitioned key cache.
    The function writes a block of file data from a buffer into the key cache.
    The buffer is specified with the parameters buff and length - the pointer
    to the beginning of the buffer and its size respectively. It's assumed
    that the buffer contains the data from 'file' allocated from the position
    filepos. The data is copied from the buffer in key_cache_block_size 
    increments. For every portion of data the function finds out in what simple
    key cache from the array of partitions the data must be stored, and after
    this calls simple_key_cache_insert to copy the data into a key buffer of
    this simple key cache.
    The parameter level is used to set one characteristic for the key buffers
    loaded with the data from buff. The characteristic is used only by the
    midpoint insertion strategy. 
   
  RETURN VALUE
    0 if a success, 1 - otherwise.

  NOTES
    The function is used by MyISAM to move all blocks from a index file to 
    the key cache. It can be performed in parallel with reading the file data
    from the key buffers by other threads.
*/

static
int partitioned_key_cache_insert(PARTITIONED_KEY_CACHE_CB *keycache,
                                 File file, my_off_t filepos, int level,
                                 uchar *buff, uint length)
{
  uint w_length;
  uint offset= (uint) (filepos % keycache->key_cache_block_size);
  DBUG_ENTER("partitioned_key_cache_insert");
  DBUG_PRINT("enter", ("fd: %u  pos: %lu  length: %u",
               (uint) file,(ulong) filepos, length));


  /* Write data in key_cache_block_size increments */
  do
  {
    SIMPLE_KEY_CACHE_CB *partition= get_key_cache_partition(keycache, 
                                                            file, filepos);
    w_length= length;
    set_if_smaller(w_length, keycache->key_cache_block_size - offset);
    if (simple_key_cache_insert((void *) partition,
                                file, filepos, level,
                                buff, w_length)) 
      DBUG_RETURN(1);

    filepos+= w_length;
    buff+= w_length;
    offset = 0;
  } while ((length-= w_length));
  
  DBUG_RETURN(0);
}


/*
  Write data from a buffer into a partitioned key cache

  SYNOPSIS

    partitioned_key_cache_write()
    keycache            pointer to the control block of a partitioned key cache
    file                handler for the file to write data to
    filepos             position in the file to write data to
    level               determines the weight of the data
    buff                buffer with the data
    length              length of the buffer
    dont_write          if is 0 then all dirty pages involved in writing
                        should have been flushed from key cache
    file_extra          maps of key cache partitions containing 
                        dirty pages from file 

  DESCRIPTION
    This function is the implementation of the key_cache_write interface
    function that is employed by partitioned key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type PARTITIONED_KEY_CACHE_CB for a
    partitioned key cache.
    In a general case the function copies data from a buffer into the key
    cache. The buffer is specified with the parameters buff and length -
    the pointer to the beginning of the buffer and its size respectively.
    It's assumed the buffer contains the data to be written into 'file'
    starting from the position filepos. The data is copied from the buffer
    in key_cache_block_size increments. For every portion of data the
    function finds out in what simple key cache from the array of partitions
    the data must be stored, and after this calls simple_key_cache_write to
    copy the data into a key buffer of this simple key cache.
    If the value of the parameter dont_write is FALSE then the function
    also writes the data into file.
    The parameter level is used to set one characteristic for the key buffers
    filled with the data from buff. The characteristic is employed only by
    the midpoint insertion strategy.
    The parameter file_expra provides a pointer to the shared bitmap of
    the partitions that may contains dirty pages for the file. This bitmap
    is used to optimize the function flush_partitioned_key_cache_blocks. 

  RETURN VALUE
    0 if a success, 1 - otherwise.

  NOTES
    This implementation exploits the fact that the function is called only
    when a thread has got an exclusive lock for the key file.
*/

static
int partitioned_key_cache_write(PARTITIONED_KEY_CACHE_CB *keycache,
                                File file, void *file_extra,
                                my_off_t filepos, int level,
                                uchar *buff, uint length,
                                uint block_length  __attribute__((unused)),
                                int dont_write)
{
  uint w_length;
  ulonglong *part_map= (ulonglong *) file_extra;
  uint offset= (uint) (filepos % keycache->key_cache_block_size);
  DBUG_ENTER("partitioned_key_cache_write");
  DBUG_PRINT("enter",
             ("fd: %u  pos: %lu  length: %u  block_length: %u"
              "  key_block_length: %u",
              (uint) file, (ulong) filepos, length, block_length,
              keycache ? keycache->key_cache_block_size : 0));


  /* Write data in key_cache_block_size increments */
  do
  {
    SIMPLE_KEY_CACHE_CB *partition= get_key_cache_partition_for_write(keycache, 
                                                                      file,
                                                                      filepos,
                                                                      part_map);
    w_length = length;
    set_if_smaller(w_length, keycache->key_cache_block_size - offset );
    if (simple_key_cache_write(partition,
                               file, 0, filepos, level,
                               buff, w_length, block_length,
                               dont_write))
      DBUG_RETURN(1);

    filepos+= w_length;
    buff+= w_length;
    offset= 0;
  } while ((length-= w_length));

  DBUG_RETURN(0);
}


/*
  Flush all blocks for a file from key buffers of a partitioned key cache 

  SYNOPSIS

    flush_partitioned_key_cache_blocks()
    keycache            pointer to the control block of a partitioned key cache
    file                handler for the file to flush to
    file_extra          maps of key cache partitions containing 
                        dirty pages from file (not used)         
    flush_type          type of the flush operation

  DESCRIPTION
    This function is the implementation of the flush_key_blocks interface
    function that is employed by partitioned key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type PARTITIONED_KEY_CACHE_CB for a
    partitioned key cache.
    In a general case the function flushes the data from all dirty key
    buffers related to the file 'file' into this file. The function does
    exactly this if the value of the parameter type is FLUSH_KEEP. If the
    value of this parameter is FLUSH_RELEASE, the function additionally 
    releases the key buffers containing data from 'file' for new usage.
    If the value of the parameter type is FLUSH_IGNORE_CHANGED the function
    just releases the key buffers containing data from 'file'.
    The function performs the operation by calling the function 
    flush_simple_key_cache_blocks for the elements of the array of the
    simple key caches that comprise the partitioned key_cache. If the value
    of the parameter type is FLUSH_KEEP s_flush_key_blocks is called only
    for the partitions with possibly dirty pages marked in the bitmap
    pointed to by the parameter file_extra.    
      
  RETURN
    0   ok
    1  error

  NOTES
    This implementation exploits the fact that the function is called only
    when a thread has got an exclusive lock for the key file.
*/

static
int flush_partitioned_key_cache_blocks(PARTITIONED_KEY_CACHE_CB *keycache,
                                       File file, void *file_extra,
                                       enum flush_type type)
{
  uint i;
  uint partitions= keycache->partitions;
  int err= 0;
  ulonglong *dirty_part_map= (ulonglong *) file_extra;
  DBUG_ENTER("partitioned_flush_key_blocks");
  DBUG_PRINT("enter", ("keycache: 0x%lx", (long) keycache));

  for (i= 0; i < partitions; i++)
  {
    SIMPLE_KEY_CACHE_CB *partition= keycache->partition_array[i];
    if ((type == FLUSH_KEEP || type == FLUSH_FORCE_WRITE) &&
        !((*dirty_part_map) & ((ulonglong) 1 << i)))
      continue;
    err|= test(flush_simple_key_cache_blocks(partition, file, 0, type));
  }
  *dirty_part_map= 0;

  DBUG_RETURN(err);
}


/*
  Reset the counters of a partitioned key cache

  SYNOPSIS
    reset_partitioned_key_cache_counters()
    name                the name of a key cache
    keycache            pointer to the control block of a partitioned key cache

  DESCRIPTION
    This function is the implementation of the reset_key_cache_counters
    interface function that is employed by partitioned key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type PARTITIONED_KEY_CACHE_CB for a partitioned
    key cache.
    This function resets the values of the statistical counters of the simple
    key caches comprising partitioned key cache to 0. It does it by calling 
    reset_simple_key_cache_counters for each key  cache partition. 
    The parameter name is currently not used.

  RETURN
    0 on success (always because it can't fail)
*/

static int
reset_partitioned_key_cache_counters(const char *name __attribute__((unused)),
                                     PARTITIONED_KEY_CACHE_CB *keycache)
{
  uint i;
  uint partitions= keycache->partitions;
  DBUG_ENTER("partitioned_reset_key_cache_counters");

  for (i = 0; i < partitions; i++)
  {
    reset_simple_key_cache_counters(name,  keycache->partition_array[i]);
  }
  DBUG_RETURN(0);
}


/*
  Get statistics for a partition key cache 

  SYNOPSIS
    get_partitioned_key_cache_statistics()
    keycache            pointer to the control block of a partitioned key cache
    partition_no        partition number to get statistics for
    key_cache_stats OUT pointer to the structure for the returned statistics

  DESCRIPTION
    This function is the implementation of the get_key_cache_statistics
    interface function that is employed by partitioned key caches.
    The function takes the parameter keycache as a pointer to the
    control block structure of the type PARTITIONED_KEY_CACHE_CB for
    a partitioned key cache.
    If the value of the parameter partition_no is equal to 0 then aggregated
    statistics for all partitions is returned in the fields of the
    structure key_cache_stat of the type KEY_CACHE_STATISTICS . Otherwise
    the function returns data for the partition number partition_no of the
    key cache in the structure key_cache_stat. (Here partitions are numbered
    starting from 1.)

  RETURN
    none
*/

static
void
get_partitioned_key_cache_statistics(PARTITIONED_KEY_CACHE_CB *keycache,
                                     uint partition_no, 
                                     KEY_CACHE_STATISTICS *keycache_stats)
{
  uint i;
  SIMPLE_KEY_CACHE_CB *partition;
  uint partitions= keycache->partitions;
  DBUG_ENTER("get_partitioned_key_cache_statistics");

  if (partition_no != 0)
  { 
    partition= keycache->partition_array[partition_no-1];
    get_simple_key_cache_statistics((void *) partition, 0, keycache_stats);
    DBUG_VOID_RETURN;
  }
  bzero(keycache_stats, sizeof(KEY_CACHE_STATISTICS));  
  keycache_stats->mem_size= (longlong) keycache->key_cache_mem_size;
  keycache_stats->block_size= (longlong) keycache->key_cache_block_size;
  for (i = 0; i < partitions; i++)
  {
    partition= keycache->partition_array[i];
    keycache_stats->blocks_used+= partition->blocks_used;
    keycache_stats->blocks_unused+= partition->blocks_unused;
    keycache_stats->blocks_changed+= partition->global_blocks_changed;
    keycache_stats->blocks_warm+= partition->warm_blocks;
    keycache_stats->read_requests+= partition->global_cache_r_requests;
    keycache_stats->reads+= partition->global_cache_read;
    keycache_stats->write_requests+= partition->global_cache_w_requests;
    keycache_stats->writes+= partition->global_cache_write;
  }
  DBUG_VOID_RETURN;  
}

/* 
  The array of pointers to the key cache interface functions used by 
  partitioned key caches. Any partitioned key cache object caches exploits
  this array.
 
  The current implementation of these functions does not allow to call
  them from the MySQL server code directly. The key cache interface
  wrappers must be used for this purpose. 
*/

static KEY_CACHE_FUNCS partitioned_key_cache_funcs =
{
  (INIT_KEY_CACHE) init_partitioned_key_cache,
  (RESIZE_KEY_CACHE) resize_partitioned_key_cache,
  (CHANGE_KEY_CACHE_PARAM) change_partitioned_key_cache_param,      
  (KEY_CACHE_READ) partitioned_key_cache_read,
  (KEY_CACHE_INSERT) partitioned_key_cache_insert,
  (KEY_CACHE_WRITE) partitioned_key_cache_write,
  (FLUSH_KEY_BLOCKS) flush_partitioned_key_cache_blocks, 
  (RESET_KEY_CACHE_COUNTERS) reset_partitioned_key_cache_counters, 
  (END_KEY_CACHE) end_partitioned_key_cache, 
  (GET_KEY_CACHE_STATISTICS) get_partitioned_key_cache_statistics,
};


/****************************************************************************** 
  Key Cache Interface Module

  The module contains wrappers for all key cache interface functions. 
  
  Currently there are key caches of two types: simple key caches and
  partitioned key caches. Each type (class) has its own implementation of the
  basic key cache operations used the MyISAM storage engine. The pointers
  to the implementation functions are stored in two static structures of the
  type KEY_CACHE_FUNC: simple_key_cache_funcs - for simple key caches, and
  partitioned_key_cache_funcs - for partitioned key caches. When a key cache
  object is created the constructor procedure init_key_cache places a pointer
  to the corresponding table into one of its fields. The procedure also
  initializes a control block for the key cache oject and saves the pointer
  to this block in another field of the key cache object.
  When a key cache wrapper function is invoked for a key cache object to
  perform a basic key cache operation it looks into the interface table
  associated with the key cache oject and calls the corresponding
  implementation of the operation. It passes the saved key cache control
  block to this implementation. If, for some reasons, the control block
  has not been fully initialized yet, the wrapper function either does not
  do anything or, in the case when it perform a read/write operation, the
  function do it directly through the system i/o functions.

  As we can see the model with which the key cache interface is supported
  as quite conventional for interfaces in general.
          
******************************************************************************/

static
int repartition_key_cache_internal(KEY_CACHE *keycache,
                                   uint key_cache_block_size, size_t use_mem,
                                   uint division_limit, uint age_threshold,
                                   uint partitions, my_bool use_op_lock);

/*
  Initialize a key cache : internal

  SYNOPSIS
    init_key_cache_internal()
    keycache           pointer to the key cache to be initialized
    key_cache_block_size    size of blocks to keep cached data
    use_mem             total memory to use for cache buffers/structures 
    division_limit      division limit (may be zero)
    age_threshold       age threshold (may be zero)
    partitions          number of partitions in the key cache
    use_op_lock        if TRUE use keycache->op_lock, otherwise - ignore it

  DESCRIPTION
    The function performs the actions required from init_key_cache().
    It has an additional parameter: use_op_lock. When the parameter
    is TRUE than the function initializes keycache->op_lock if needed,
    then locks it, and unlocks it before the return. Otherwise the actions
    with the lock are omitted. 

  RETURN VALUE
    total number of blocks in key cache partitions, if successful,
    <= 0 - otherwise.

  NOTES
    if keycache->key_cache_inited != 0 we assume that the memory
    for the control block of the key cache has been already allocated.
*/

static
int init_key_cache_internal(KEY_CACHE *keycache, uint key_cache_block_size,
		            size_t use_mem, uint division_limit,
		            uint age_threshold, uint partitions,
                            my_bool use_op_lock)
{
  void *keycache_cb;
  int blocks;
  if (keycache->key_cache_inited)
  {
    if (use_op_lock)
      pthread_mutex_lock(&keycache->op_lock);
    keycache_cb= keycache->keycache_cb;
  }
  else
  {
    if (partitions == 0)
    {
      if (!(keycache_cb= (void *)  my_malloc(sizeof(SIMPLE_KEY_CACHE_CB),
                                             MYF(0)))) 
        return 0;
      ((SIMPLE_KEY_CACHE_CB *) keycache_cb)->key_cache_inited= 0;
      keycache->key_cache_type= SIMPLE_KEY_CACHE;
      keycache->interface_funcs= &simple_key_cache_funcs;
    }
    else
    {
      if (!(keycache_cb= (void *)  my_malloc(sizeof(PARTITIONED_KEY_CACHE_CB),
                                             MYF(0)))) 
        return 0;
      ((PARTITIONED_KEY_CACHE_CB *) keycache_cb)->key_cache_inited= 0;
      keycache->key_cache_type= PARTITIONED_KEY_CACHE;
      keycache->interface_funcs= &partitioned_key_cache_funcs;
    }
    /*
      Initialize op_lock if it's not initialized before. 
      The mutex may have been initialized before if we are being called
      from repartition_key_cache_internal().
    */
    if (use_op_lock)
      pthread_mutex_init(&keycache->op_lock, MY_MUTEX_INIT_FAST);      
    keycache->keycache_cb= keycache_cb;
    keycache->key_cache_inited= 1;
    if (use_op_lock)
      pthread_mutex_lock(&keycache->op_lock);
  }

  if (partitions != 0)
  {
    ((PARTITIONED_KEY_CACHE_CB *) keycache_cb)->partitions= partitions;
  }
  keycache->can_be_used= 0;
  blocks= keycache->interface_funcs->init(keycache_cb, key_cache_block_size,
                                          use_mem, division_limit,
                                          age_threshold);
  keycache->partitions= partitions ? 
                        ((PARTITIONED_KEY_CACHE_CB *) keycache_cb)->partitions :
                        0;
  DBUG_ASSERT(partitions <= MAX_KEY_CACHE_PARTITIONS);
  keycache->key_cache_mem_size=
    keycache->partitions ?
    ((PARTITIONED_KEY_CACHE_CB *) keycache_cb)->key_cache_mem_size :
    ((SIMPLE_KEY_CACHE_CB *) keycache_cb)->key_cache_mem_size;
  if (blocks > 0)
    keycache->can_be_used= 1;
  if (use_op_lock)
    pthread_mutex_unlock(&keycache->op_lock);
  return blocks;
}


/*
  Initialize a key cache

  SYNOPSIS
    init_key_cache()
    keycache           pointer to the key cache to be initialized
    key_cache_block_size    size of blocks to keep cached data
    use_mem             total memory to use for cache buffers/structures 
    division_limit      division limit (may be zero)
    age_threshold       age threshold (may be zero)
    partitions          number of partitions in the key cache

  DESCRIPTION
    The function creates a control block structure for a key cache and
    places the pointer to this block in the structure keycache. 
    If the value of the parameter 'partitions' is 0 then a simple key cache
    is created. Otherwise a partitioned key cache with the specified number
    of partitions is created.  
    The parameter key_cache_block_size specifies the size of the blocks in
    the key cache to be created. The parameters division_limit and
    age_threshold determine the initial values of those characteristics of
    the key cache that are used for midpoint insertion strategy. The parameter
    use_mem  specifies the total amount of memory to be allocated for the
    key cache buffers and for all auxiliary structures.  
    The function calls init_key_cache_internal() to perform all these actions
    with the last parameter set to TRUE.     

  RETURN VALUE
    total number of blocks in key cache partitions, if successful,
    <= 0 - otherwise.

  NOTES
    It's assumed that no two threads call this function simultaneously
    referring to the same key cache handle.
*/

int init_key_cache(KEY_CACHE *keycache, uint key_cache_block_size,
		   size_t use_mem, uint division_limit,
		   uint age_threshold, uint partitions)
{
  return init_key_cache_internal(keycache,  key_cache_block_size, use_mem,
				 division_limit, age_threshold, partitions, 1);
}


/*
  Resize a key cache

  SYNOPSIS
    resize_key_cache()
    keycache            pointer to the key cache to be resized
    key_cache_block_size    size of blocks to keep cached data
    use_mem             total memory to use for the new key cache
    division_limit      new division limit (if not zero)
    age_threshold       new age threshold (if not zero)

  DESCRIPTION
    The function operates over the key cache key cache.
    The parameter key_cache_block_size specifies the new size of the block
    buffers in the key cache. The parameters division_limit and age_threshold
    determine the new initial values of those characteristics of the key cache
    that are used for midpoint insertion strategy. The parameter use_mem
    specifies the total amount of  memory to be allocated for the key cache
    buffers and for all auxiliary structures.

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.

  NOTES
    The function does not block the calls and executions of other functions
    from the key cache interface. However it assumes that the calls of 
    resize_key_cache itself are serialized.

    Currently the function is called when the values of the variables
    key_buffer_size and/or key_cache_block_size are being reset for
    the key cache keycache.
*/

int resize_key_cache(KEY_CACHE *keycache, uint key_cache_block_size,
		     size_t use_mem, uint division_limit, uint age_threshold)
{
  int blocks= -1;
  if (keycache->key_cache_inited)
  {
    pthread_mutex_lock(&keycache->op_lock);
    if ((uint) keycache->param_partitions != keycache->partitions && use_mem)
      blocks= repartition_key_cache_internal(keycache,
                                             key_cache_block_size, use_mem,
                                             division_limit, age_threshold, 
                                             (uint) keycache->param_partitions,
                                             0);
    else
    {
      blocks= keycache->interface_funcs->resize(keycache->keycache_cb,
                                                key_cache_block_size,
                                                use_mem, division_limit,
                                                age_threshold);

      if (keycache->partitions)
        keycache->partitions=
          ((PARTITIONED_KEY_CACHE_CB *)(keycache->keycache_cb))->partitions;
    }

    keycache->key_cache_mem_size=
    keycache->partitions ?
    ((PARTITIONED_KEY_CACHE_CB *)(keycache->keycache_cb))->key_cache_mem_size :
    ((SIMPLE_KEY_CACHE_CB *)(keycache->keycache_cb))->key_cache_mem_size;

    keycache->can_be_used= (blocks >= 0);
    pthread_mutex_unlock(&keycache->op_lock);
  } 
  return blocks;
}


/*
  Change key cache parameters of a key cache

  SYNOPSIS
    change_key_cache_param()
    keycache            pointer to the key cache to change parameters for
    division_limit      new division limit (if not zero)
    age_threshold       new age threshold (if not zero)

  DESCRIPTION
    The function sets new values of the division limit and the age threshold 
    used when the key cache keycach employs midpoint insertion strategy.
    The parameters division_limit and age_threshold provide these new values.

  RETURN VALUE
    none

  NOTES
    Currently the function is called when the values of the variables
    key_cache_division_limit and/or key_cache_age_threshold are being reset
    for the key cache keycache.
*/

void change_key_cache_param(KEY_CACHE *keycache, uint division_limit,
			    uint age_threshold)
{
  if (keycache->key_cache_inited)
  {
    pthread_mutex_lock(&keycache->op_lock);    
    keycache->interface_funcs->change_param(keycache->keycache_cb,
                                            division_limit,
                                            age_threshold);    
    pthread_mutex_unlock(&keycache->op_lock);
  }
}


/*
  Destroy a key cache : internal

  SYNOPSIS
    end_key_cache_internal()
    keycache            pointer to the key cache to be destroyed
    cleanup             <=> complete free 
    use_op_lock         if TRUE use keycache->op_lock, otherwise - ignore it

  DESCRIPTION
    The function performs the actions required from end_key_cache().
    It has an additional parameter: use_op_lock. When the parameter
    is TRUE than the function destroys keycache->op_lock if cleanup is true.
    Otherwise the action with the lock is omitted. 

  RETURN VALUE
    none
*/

static
void end_key_cache_internal(KEY_CACHE *keycache, my_bool cleanup,
                            my_bool use_op_lock)
{
  if (keycache->key_cache_inited)
  {
    keycache->interface_funcs->end(keycache->keycache_cb, cleanup);
    if (cleanup)
    {
      if (keycache->keycache_cb)
      {
        my_free(keycache->keycache_cb);
        keycache->keycache_cb= 0;
      }
      /*
        We do not destroy op_lock if we are going to reuse the same key cache.
        This happens if we are called from  repartition_key_cache_internal().
      */
      if (use_op_lock)
        pthread_mutex_destroy(&keycache->op_lock);
      keycache->key_cache_inited= 0;
    }
    keycache->can_be_used= 0;
  }
}


/*
  Destroy a key cache 

  SYNOPSIS
    end_key_cache()
    keycache            pointer to the key cache to be destroyed
    cleanup             <=> complete free 

  DESCRIPTION
    The function frees the memory allocated for the cache blocks and
    auxiliary structures used by the key cache keycache. If the value
    of the parameter cleanup is TRUE then all resources used by the key
    cache are to be freed.
    The function calls end_key_cache_internal() to perform all these actions
    with the last parameter set to TRUE.     

  RETURN VALUE
    none
*/

void end_key_cache(KEY_CACHE *keycache, my_bool cleanup)
{
  end_key_cache_internal(keycache, cleanup, 1);
}


/*
  Read a block of data from a key cache into a buffer

  SYNOPSIS

    key_cache_read()
    keycache            pointer to the key cache to read data from  
    file                handler for the file for the block of data to be read
    filepos             position of the block of data in the file
    level               determines the weight of the data
    buff                buffer to where the data must be placed
    length              length of the buffer
    block_length        length of the data read from a key cache block 
    return_buffer       return pointer to the key cache buffer with the data

  DESCRIPTION
    The function operates over buffers of the key cache keycache.
    In a general case the function reads a block of data from the key cache
    into the buffer buff of the size specified by the parameter length. The
    beginning of the block of data to be read is specified by the parameters
    file and filepos. The length of the read data is the same as the length
    of the buffer.
    If the parameter return_buffer is not ignored and its value is TRUE, and 
    the data to be read of the specified size block_length can be read from one
    key cache buffer, then the function returns a pointer to the data in the
    key cache buffer.
    The parameter 'level' is used only by the midpoint insertion strategy 
    when the data or its portion cannot be found in the key cache.
    The function reads data into the buffer directly from file if the control
    block of the key cache has not been initialized yet. 
   
  RETURN VALUE
    Returns address from where the data is placed if successful, 0 - otherwise.

  NOTES.
    Filepos must be a multiple of 'block_length', but it doesn't
    have to be a multiple of key_cache_block_size;
*/

uchar *key_cache_read(KEY_CACHE *keycache, 
                      File file, my_off_t filepos, int level,
                      uchar *buff, uint length,
		      uint block_length, int return_buffer)
{
  if (keycache->can_be_used)
    return keycache->interface_funcs->read(keycache->keycache_cb,
                                           file, filepos, level,
                                           buff, length,
                                           block_length, return_buffer);
 
  /* We can't use mutex here as the key cache may not be initialized */

  if (my_pread(file, (uchar*) buff, length, filepos, MYF(MY_NABP)))
    return (uchar *) 0;
  
  return buff;
}


/*
  Insert a block of file data from a buffer into a key cache

  SYNOPSIS
    key_cache_insert()
    keycache            pointer to the key cache to insert data into 
    file                handler for the file to insert data from
    filepos             position of the block of data in the file to insert
    level               determines the weight of the data
    buff                buffer to read data from
    length              length of the data in the buffer

  DESCRIPTION
    The function operates over buffers of the key cache keycache.
    The function writes a block of file data from a buffer into the key cache.
    The buffer is specified with the parameters buff and length - the pointer
    to the beginning of the buffer and its size respectively. It's assumed
    that the buffer contains the data from 'file' allocated from the position
    filepos.
    The parameter level is used to set one characteristic for the key buffers
    loaded with the data from buff. The characteristic is used only by the
    midpoint insertion strategy. 
   
  RETURN VALUE
    0 if a success, 1 - otherwise.

  NOTES
    The function is used by MyISAM to move all blocks from a index file to 
    the key cache. 
    It is assumed that it may be performed in parallel with reading the file
    data from the key buffers by other threads.
*/

int key_cache_insert(KEY_CACHE *keycache,
                     File file, my_off_t filepos, int level,
                     uchar *buff, uint length)
{
  if (keycache->can_be_used)
    return keycache->interface_funcs->insert(keycache->keycache_cb,
                                             file, filepos, level,
                                             buff, length);
  return 0;
}


/*
  Write data from a buffer into a key cache

  SYNOPSIS

    key_cache_write()
    keycache            pointer to the key cache to write data to
    file                handler for the file to write data to
    filepos             position in the file to write data to
    level               determines the weight of the data
    buff                buffer with the data
    length              length of the buffer
    dont_write          if is 0 then all dirty pages involved in writing
                        should have been flushed from key cache
    file_extra          pointer to optional file attributes

  DESCRIPTION
    The function operates over buffers of the key cache keycache.
    In a general case the function writes data from a buffer into the key
    cache. The buffer is specified with the parameters buff and length -
    the pointer to the beginning of the buffer and its size respectively.
    It's assumed the buffer contains the data to be written into 'file'
    starting from the position filepos. 
    If the value of the parameter dont_write is FALSE then the function
    also writes the data into file.
    The parameter level is used to set one characteristic for the key buffers
    filled with the data from buff. The characteristic is employed only by
    the midpoint insertion strategy.
    The parameter file_expra may point to additional file attributes used
    for optimization or other purposes.
    The function writes data from the buffer directly into file if the control
    block of the key cache has not been initialized yet.      

  RETURN VALUE
    0 if a success, 1 - otherwise.

  NOTES
    This implementation may exploit the fact that the function is called only
    when a thread has got an exclusive lock for the key file.
*/

int key_cache_write(KEY_CACHE *keycache,
                    File file, void *file_extra,
                    my_off_t filepos, int level,
                    uchar *buff, uint length,
		    uint block_length, int force_write)
{
  if (keycache->can_be_used)
    return keycache->interface_funcs->write(keycache->keycache_cb,
                                            file, file_extra,
                                            filepos, level,
                                            buff, length,
                                            block_length, force_write);
  
  /* We can't use mutex here as the key cache may not be initialized */
  if (my_pwrite(file, buff, length, filepos, MYF(MY_NABP | MY_WAIT_IF_FULL)))
    return 1;

  return 0;
}


/*
  Flush all blocks for a file from key buffers of a key cache 

  SYNOPSIS

    flush_key_blocks()
    keycache            pointer to the key cache whose blocks are to be flushed
    file                handler for the file to flush to
    file_extra          maps of key cache (used for partitioned key caches)
    flush_type          type of the flush operation

  DESCRIPTION
    The function operates over buffers of the key cache keycache.
    In a general case the function flushes the data from all dirty key
    buffers related to the file 'file' into this file. The function does
    exactly this if the value of the parameter type is FLUSH_KEEP. If the
    value of this parameter is FLUSH_RELEASE, the function additionally 
    releases the key buffers containing data from 'file' for new usage.
    If the value of the parameter type is FLUSH_IGNORE_CHANGED the function
    just releases the key buffers containing data from 'file'.
    If the value of the parameter type is FLUSH_KEEP the function may use
    the value of the parameter file_extra pointing to possibly dirty
    partitions to optimize the operation for partitioned key caches.
      
  RETURN
    0   ok
    1  error

  NOTES
    Any implementation of the function may exploit the fact that the function
    is called only when a thread has got an exclusive lock for the key file.
*/

int flush_key_blocks(KEY_CACHE *keycache,
                     int file, void *file_extra,
                     enum flush_type type)
{
  if (keycache->can_be_used)
    return keycache->interface_funcs->flush(keycache->keycache_cb,
                                            file, file_extra, type);
  return 0;  
}


/*
  Reset the counters of a key cache

  SYNOPSIS
    reset_key_cache_counters()
    name          the name of a key cache (unused)
    keycache      pointer to the key cache for which to reset counters

  DESCRIPTION
    This function resets the values of the statistical counters for the key
    cache keycache.
    The parameter name is currently not used.

  RETURN
    0 on success (always because it can't fail)

  NOTES
   This procedure is used by process_key_caches() to reset the counters of all
   currently used key caches, both the default one and the named ones.
*/

int reset_key_cache_counters(const char *name __attribute__((unused)),
                             KEY_CACHE *keycache,
                             void *unused __attribute__((unused)))
{
  int rc= 0;
  if (keycache->key_cache_inited)
  {
    pthread_mutex_lock(&keycache->op_lock);
    rc= keycache->interface_funcs->reset_counters(name,
                                                  keycache->keycache_cb);
    pthread_mutex_unlock(&keycache->op_lock);
  }
  return rc;
}


/*
  Get statistics for a key cache

  SYNOPSIS
    get_key_cache_statistics()
    keycache            pointer to the key cache to get statistics for
    partition_no        partition number to get statistics for
    key_cache_stats OUT pointer to the structure for the returned statistics

  DESCRIPTION
    If the value of the parameter partition_no is equal to 0 then statistics
    for the whole key cache keycache (aggregated statistics) is returned in the
    fields of the structure key_cache_stat of the type KEY_CACHE_STATISTICS.
    Otherwise the value of the parameter partition_no makes sense only for
    a partitioned key cache. In this case the function returns statistics
    for the partition with the specified number partition_no.   
  
  RETURN
    none
*/

void get_key_cache_statistics(KEY_CACHE *keycache, uint partition_no, 
                              KEY_CACHE_STATISTICS *key_cache_stats)
{
  if (keycache->key_cache_inited)
  {    
    pthread_mutex_lock(&keycache->op_lock);
    keycache->interface_funcs->get_stats(keycache->keycache_cb,
                                         partition_no, key_cache_stats);
    pthread_mutex_unlock(&keycache->op_lock);
  }
}


/*
  Repartition a key cache : internal

  SYNOPSIS
    repartition_key_cache_internal()
    keycache           pointer to the key cache to be repartitioned
    key_cache_block_size    size of blocks to keep cached data
    use_mem             total memory to use for the new key cache
    division_limit      new division limit (if not zero)
    age_threshold       new age threshold (if not zero)
    partitions          new number of partitions in the key cache 
    use_op_lock         if TRUE use keycache->op_lock, otherwise - ignore it

  DESCRIPTION
    The function performs the actions required from repartition_key_cache().
    It has an additional parameter: use_op_lock. When the parameter
    is TRUE then the function locks keycache->op_lock at start and
    unlocks it before the return. Otherwise the actions with the lock
    are omitted. 

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.
*/

static
int repartition_key_cache_internal(KEY_CACHE *keycache,
                                   uint key_cache_block_size, size_t use_mem,
                                   uint division_limit, uint age_threshold,
                                   uint partitions, my_bool use_op_lock)
{
  uint blocks= -1;
  if (keycache->key_cache_inited)
  {
    if (use_op_lock)
      pthread_mutex_lock(&keycache->op_lock);
    keycache->interface_funcs->resize(keycache->keycache_cb,
                                      key_cache_block_size, 0,
                                      division_limit, age_threshold);
    end_key_cache_internal(keycache, 1, 0);
    blocks= init_key_cache_internal(keycache, key_cache_block_size, use_mem,
                                    division_limit, age_threshold, partitions,
                                    0);
    if (use_op_lock)
      pthread_mutex_unlock(&keycache->op_lock);
  } 
  return blocks;
}

/*
  Repartition a key cache

  SYNOPSIS
    repartition_key_cache()
    keycache           pointer to the key cache to be repartitioned
    key_cache_block_size    size of blocks to keep cached data
    use_mem             total memory to use for the new key cache
    division_limit      new division limit (if not zero)
    age_threshold       new age threshold (if not zero)
    partitions          new number of partitions in the key cache 

  DESCRIPTION
    The function operates over the key cache keycache.
    The parameter partitions specifies the number of partitions in the key
    cache after repartitioning. If the value of this parameter is 0 then
    a simple key cache must be created instead of the old one. 
    The parameter key_cache_block_size specifies the new size of the block
    buffers in the key cache. The parameters division_limit and age_threshold
    determine the new initial values of those characteristics of the key cache
    that are used for midpoint insertion strategy. The parameter use_mem
    specifies the total amount of  memory to be allocated for the new key
    cache buffers and for all auxiliary structures.
    The function calls repartition_key_cache_internal() to perform all these
    actions with the last parameter set to TRUE.     

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.

  NOTES
    Currently the function is called when the value of the variable
    key_cache_partitions is being reset for the key cache keycache.
*/

int repartition_key_cache(KEY_CACHE *keycache, uint key_cache_block_size,
		          size_t use_mem, uint division_limit,
                          uint age_threshold, uint partitions)
{
  return repartition_key_cache_internal(keycache, key_cache_block_size, use_mem,
			                division_limit, age_threshold,
                                        partitions, 1);
}

