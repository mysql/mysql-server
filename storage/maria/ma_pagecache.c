/* Copyright (C) 2000-2008 MySQL AB, 2008-2011 Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  These functions handle page caching for Maria tables.

  One cache can handle many files.
  It must contain buffers of the same blocksize.
  init_pagecache() should be used to init cache handler.

  The free list (free_block_list) is a stack like structure.
  When a block is freed by free_block(), it is pushed onto the stack.
  When a new block is required it is first tried to pop one from the stack.
  If the stack is empty, it is tried to get a never-used block from the pool.
  If this is empty too, then a block is taken from the LRU ring, flushing it
  to disk, if necessary. This is handled in find_block().
  With the new free list, the blocks can have three temperatures:
  hot, warm and cold (which is free). This is remembered in the block header
  by the enum PCBLOCK_TEMPERATURE temperature variable. Remembering the
  temperature is necessary to correctly count the number of warm blocks,
  which is required to decide when blocks are allowed to become hot. Whenever
  a block is inserted to another (sub-)chain, we take the old and new
  temperature into account to decide if we got one more or less warm block.
  blocks_unused is the sum of never used blocks in the pool and of currently
  free blocks. blocks_used is the number of blocks fetched from the pool and
  as such gives the maximum number of in-use blocks at any time.

  TODO: Write operation locks whole cache till the end of the operation.
    Should be fixed.
*/

#include "maria_def.h"
#include <m_string.h>
#include "ma_pagecache.h"
#include "ma_blockrec.h"
#include <my_bit.h>
#include <errno.h>

/*
  Some compilation flags have been added specifically for this module
  to control the following:
  - not to let a thread to yield the control when reading directly
    from page cache, which might improve performance in many cases;
    to enable this add:
    #define SERIALIZED_READ_FROM_CACHE
  - to set an upper bound for number of threads simultaneously
    using the page cache; this setting helps to determine an optimal
    size for hash table and improve performance when the number of
    blocks in the page cache much less than the number of threads
    accessing it;
    to set this number equal to <N> add
      #define MAX_THREADS <N>
  - to substitute calls of pthread_cond_wait for calls of
    pthread_cond_timedwait (wait with timeout set up);
    this setting should be used only when you want to trap a deadlock
    situation, which theoretically should not happen;
    to set timeout equal to <T> seconds add
      #define PAGECACHE_TIMEOUT <T>
  - to enable the module traps and to send debug information from
    page cache module to a special debug log add:
      #define PAGECACHE_DEBUG
    the name of this debug log file <LOG NAME> can be set through:
      #define PAGECACHE_DEBUG_LOG  <LOG NAME>
    if the name is not defined, it's set by default;
    if the PAGECACHE_DEBUG flag is not set up and we are in a debug
    mode, i.e. when ! defined(DBUG_OFF), the debug information from the
    module is sent to the regular debug log.

  Example of the settings:
    #define SERIALIZED_READ_FROM_CACHE
    #define MAX_THREADS   100
    #define PAGECACHE_TIMEOUT  1
    #define PAGECACHE_DEBUG
    #define PAGECACHE_DEBUG_LOG  "my_pagecache_debug.log"
*/

/*
  In key cache we have external raw locking here we use
  SERIALIZED_READ_FROM_CACHE to avoid problem of reading
  not consistent data from the page.
  (keycache functions (key_cache_read(), key_cache_insert() and
  key_cache_write()) rely on external MyISAM lock, we don't)
*/
#define SERIALIZED_READ_FROM_CACHE yes

#define PCBLOCK_INFO(B) \
  DBUG_PRINT("info", \
             ("block: 0x%lx  fd: %lu  page: %lu  s: %0x  hshL: " \
              " 0x%lx  req: %u/%u wrlocks: %u  rdlocks %u  " \
              "rdlocks_q: %u  pins: %u  status: %u  type: %s", \
              (ulong)(B), \
              (ulong)((B)->hash_link ? \
                      (B)->hash_link->file.file : \
                      0), \
              (ulong)((B)->hash_link ? \
                      (B)->hash_link->pageno : \
                      0), \
              (B)->status, \
              (ulong)(B)->hash_link, \
              (uint) (B)->requests, \
              (uint)((B)->hash_link ? \
                     (B)->hash_link->requests : \
                       0), \
              block->wlocks, block->rlocks, block->rlocks_queue, \
              (uint)(B)->pins, (uint)(B)->status, \
              page_cache_page_type_str[(B)->type]))

/* TODO: put it to my_static.c */
my_bool my_disable_flush_pagecache_blocks= 0;

#define STRUCT_PTR(TYPE, MEMBER, a)                                           \
          (TYPE *) ((char *) (a) - offsetof(TYPE, MEMBER))

/* types of condition variables */
#define  COND_FOR_REQUESTED 0  /* queue of thread waiting for read operation */
#define  COND_FOR_SAVED     1  /* queue of thread waiting for flush */
#define  COND_FOR_WRLOCK    2  /* queue of write lock */
#define  COND_SIZE          3  /* number of COND_* queues */

typedef pthread_cond_t KEYCACHE_CONDVAR;

/* descriptor of the page in the page cache block buffer */
struct st_pagecache_page
{
  PAGECACHE_FILE file;    /* file to which the page belongs to  */
  pgcache_page_no_t pageno; /* number of the page in the file   */
};

/* element in the chain of a hash table bucket */
struct st_pagecache_hash_link
{
  struct st_pagecache_hash_link
    *next, **prev;                   /* to connect links in the same bucket  */
  struct st_pagecache_block_link
    *block;                          /* reference to the block for the page: */
  PAGECACHE_FILE file;               /* from such a file                     */
  pgcache_page_no_t pageno;            /* this page                            */
  uint requests;                     /* number of requests for the page      */
};

/* simple states of a block */
#define PCBLOCK_ERROR       1 /* an error occurred when performing disk i/o  */
#define PCBLOCK_READ        2 /* the is page in the block buffer             */
#define PCBLOCK_IN_SWITCH   4 /* block is preparing to read new page         */
#define PCBLOCK_REASSIGNED  8 /* block does not accept requests for old page */
#define PCBLOCK_IN_FLUSH   16 /* block is in flush operation                 */
#define PCBLOCK_CHANGED    32 /* block buffer contains a dirty page          */
#define PCBLOCK_DIRECT_W   64 /* possible direct write to the block          */
#define PCBLOCK_DEL_WRITE 128 /* should be written on delete                 */

/* page status, returned by find_block */
#define PAGE_READ               0
#define PAGE_TO_BE_READ         1
#define PAGE_WAIT_TO_BE_READ    2

/* block temperature determines in which (sub-)chain the block currently is */
enum PCBLOCK_TEMPERATURE { PCBLOCK_COLD /*free*/ , PCBLOCK_WARM , PCBLOCK_HOT };

/* debug info */
#ifndef DBUG_OFF
static const char *page_cache_page_type_str[]=
{
  /* used only for control page type changing during debugging */
  "EMPTY",
  "PLAIN",
  "LSN",
  "READ_UNKNOWN"
};

static const char *page_cache_page_write_mode_str[]=
{
  "DELAY",
  "DONE"
};

static const char *page_cache_page_lock_str[]=
{
  "free -> free",
  "read -> read",
  "write -> write",
  "free -> read",
  "free -> write",
  "read -> free",
  "write -> free",
  "write -> read"
};

static const char *page_cache_page_pin_str[]=
{
  "pinned -> pinned",
  "unpinned -> unpinned",
  "unpinned -> pinned",
  "pinned -> unpinned"
};


typedef struct st_pagecache_pin_info
{
  struct st_pagecache_pin_info *next, **prev;
  struct st_my_thread_var *thread;
}  PAGECACHE_PIN_INFO;

/*
  st_pagecache_lock_info structure should be kept in next, prev, thread part
  compatible with st_pagecache_pin_info to be compatible in functions.
*/

typedef struct st_pagecache_lock_info
{
  struct st_pagecache_lock_info *next, **prev;
  struct st_my_thread_var *thread;
  my_bool write_lock;
} PAGECACHE_LOCK_INFO;


/* service functions maintain debugging info about pin & lock */


/*
  Links information about thread pinned/locked the block to the list

  SYNOPSIS
    info_link()
    list                 the list to link in
    node                 the node which should be linked
*/

static void info_link(PAGECACHE_PIN_INFO **list, PAGECACHE_PIN_INFO *node)
{
  if ((node->next= *list))
    node->next->prev= &(node->next);
  *list= node;
  node->prev= list;
}


/*
  Unlinks information about thread pinned/locked the block from the list

  SYNOPSIS
    info_unlink()
    node                 the node which should be unlinked
*/

static void info_unlink(PAGECACHE_PIN_INFO *node)
{
  if ((*node->prev= node->next))
   node->next->prev= node->prev;
}


/*
  Finds information about given thread in the list of threads which
  pinned/locked this block.

  SYNOPSIS
    info_find()
    list                 the list where to find the thread
    thread               thread ID (reference to the st_my_thread_var
                         of the thread)
    any                  return any thread of the list

  RETURN
    0 - the thread was not found
    pointer to the information node of the thread in the list, or, if 'any',
    to any thread of the list.
*/

static PAGECACHE_PIN_INFO *info_find(PAGECACHE_PIN_INFO *list,
                                     struct st_my_thread_var *thread,
                                     my_bool any)
{
  register PAGECACHE_PIN_INFO *i= list;
  if (any)
    return i;
  for(; i != 0; i= i->next)
    if (i->thread == thread)
      return i;
  return 0;
}

#endif /* !DBUG_OFF */

/* page cache block */
struct st_pagecache_block_link
{
  struct st_pagecache_block_link
    *next_used, **prev_used;   /* to connect links in the LRU chain (ring)   */
  struct st_pagecache_block_link
    *next_changed, **prev_changed; /* for lists of file dirty/clean blocks   */
  struct st_pagecache_hash_link
    *hash_link;           /* backward ptr to referring hash_link             */
#ifndef DBUG_OFF
  PAGECACHE_PIN_INFO *pin_list;
  PAGECACHE_LOCK_INFO *lock_list;
#endif
  KEYCACHE_CONDVAR *condvar; /* condition variable for 'no readers' event    */
  uchar *buffer;           /* buffer for the block page                      */
  pthread_t write_locker;

  ulonglong last_hit_time; /* timestamp of the last hit                      */
  WQUEUE
    wqueue[COND_SIZE];    /* queues on waiting requests for new/old pages    */
  uint32 requests;        /* number of requests for the block                */
  uint32 pins;            /* pin counter                                     */
  uint32 wlocks;          /* write locks counter                             */
  uint32 rlocks;          /* read locks counter                              */
  uint32 rlocks_queue;    /* rd. locks waiting wr. lock of this thread       */
  uint16 status;          /* state of the block                              */
  int16  error;           /* error code for block in case of error */
  enum PCBLOCK_TEMPERATURE temperature; /* block temperature: cold, warm, hot*/
  enum pagecache_page_type type; /* type of the block                        */
  uint hits_left;         /* number of hits left until promotion             */
  /** @brief LSN when first became dirty; LSN_MAX means "not yet set"        */
  LSN rec_lsn;
};

/** @brief information describing a run of flush_pagecache_blocks_int() */
struct st_file_in_flush
{
  File file;
  /**
     @brief threads waiting for the thread currently flushing this file to be
     done
  */
  WQUEUE flush_queue;
  /**
     @brief if the thread currently flushing the file has a non-empty
     first_in_switch list.
  */
  my_bool first_in_switch;
};

#ifndef DBUG_OFF
/* debug checks */

#ifdef NOT_USED
static my_bool info_check_pin(PAGECACHE_BLOCK_LINK *block,
                              enum pagecache_page_pin mode
                              __attribute__((unused)))
{
  struct st_my_thread_var *thread= my_thread_var;
  PAGECACHE_PIN_INFO *info= info_find(block->pin_list, thread);
  DBUG_ENTER("info_check_pin");
  DBUG_PRINT("enter", ("thread: 0x%lx  pin: %s",
                       (ulong) thread, page_cache_page_pin_str[mode]));
  if (info)
  {
    if (mode == PAGECACHE_PIN_LEFT_UNPINNED)
    {
      DBUG_PRINT("info",
                 ("info_check_pin: thread: 0x%lx  block: 0x%lx  ; LEFT_UNPINNED!!!",
                  (ulong)thread, (ulong)block));
      DBUG_RETURN(1);
    }
    else if (mode == PAGECACHE_PIN)
    {
      DBUG_PRINT("info",
                 ("info_check_pin: thread: 0x%lx  block: 0x%lx  ; PIN!!!",
                  (ulong)thread, (ulong)block));
      DBUG_RETURN(1);
    }
  }
  else
  {
    if (mode == PAGECACHE_PIN_LEFT_PINNED)
    {
      DBUG_PRINT("info",
                 ("info_check_pin: thread: 0x%lx  block: 0x%lx  ; LEFT_PINNED!!!",
                  (ulong)thread, (ulong)block));
      DBUG_RETURN(1);
    }
    else if (mode == PAGECACHE_UNPIN)
    {
      DBUG_PRINT("info",
                 ("info_check_pin: thread: 0x%lx  block: 0x%lx  ; UNPIN!!!",
                  (ulong)thread, (ulong)block));
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


/*
  Debug function which checks current lock/pin state and requested changes

  SYNOPSIS
    info_check_lock()
    lock                 requested lock changes
    pin                  requested pin changes

  RETURN
    0 - OK
    1 - Error
*/

static my_bool info_check_lock(PAGECACHE_BLOCK_LINK *block,
                               enum pagecache_page_lock lock,
                               enum pagecache_page_pin pin)
{
  struct st_my_thread_var *thread= my_thread_var;
  PAGECACHE_LOCK_INFO *info=
    (PAGECACHE_LOCK_INFO *) info_find((PAGECACHE_PIN_INFO *) block->lock_list,
                                      thread);
  DBUG_ENTER("info_check_lock");
  switch(lock) {
  case PAGECACHE_LOCK_LEFT_UNLOCKED:
    if (pin != PAGECACHE_PIN_LEFT_UNPINNED ||
        info)
      goto error;
    break;
  case PAGECACHE_LOCK_LEFT_READLOCKED:
    if ((pin != PAGECACHE_PIN_LEFT_UNPINNED &&
         pin != PAGECACHE_PIN_LEFT_PINNED) ||
        info == 0 || info->write_lock)
      goto error;
    break;
  case PAGECACHE_LOCK_LEFT_WRITELOCKED:
    if (pin != PAGECACHE_PIN_LEFT_PINNED ||
        info == 0 || !info->write_lock)
      goto error;
    break;
  case PAGECACHE_LOCK_READ:
    if ((pin != PAGECACHE_PIN_LEFT_UNPINNED &&
         pin != PAGECACHE_PIN) ||
        info != 0)
      goto error;
    break;
  case PAGECACHE_LOCK_WRITE:
    if (pin != PAGECACHE_PIN ||
        info != 0)
      goto error;
    break;
  case PAGECACHE_LOCK_READ_UNLOCK:
    if ((pin != PAGECACHE_PIN_LEFT_UNPINNED &&
         pin != PAGECACHE_UNPIN) ||
        info == 0 || info->write_lock)
      goto error;
    break;
  case PAGECACHE_LOCK_WRITE_UNLOCK:
    if (pin != PAGECACHE_UNPIN ||
        info == 0 || !info->write_lock)
      goto error;
    break;
  case PAGECACHE_LOCK_WRITE_TO_READ:
    if ((pin != PAGECACHE_PIN_LEFT_PINNED &&
         pin != PAGECACHE_UNPIN) ||
        info == 0 || !info->write_lock)
      goto error;
    break;
  }
  DBUG_RETURN(0);
error:
  DBUG_PRINT("info",
             ("info_check_lock: thread: 0x%lx block 0x%lx: info: %d wrt: %d,"
              "to lock: %s, to pin: %s",
              (ulong)thread, (ulong)block, test(info),
              (info ? info->write_lock : 0),
              page_cache_page_lock_str[lock],
              page_cache_page_pin_str[pin]));
  DBUG_RETURN(1);
}
#endif /* NOT_USED */
#endif /* !DBUG_OFF */

#define FLUSH_CACHE         2000            /* sort this many blocks at once */

static void free_block(PAGECACHE *pagecache, PAGECACHE_BLOCK_LINK *block);
#ifndef DBUG_OFF
static void test_key_cache(PAGECACHE *pagecache,
                           const char *where, my_bool lock);
#endif

#define PAGECACHE_HASH(p, f, pos) (((ulong) (pos) +                          \
                                    (ulong) (f).file) & (p->hash_entries-1))
#define FILE_HASH(f) ((uint) (f).file & (PAGECACHE_CHANGED_BLOCKS_HASH - 1))

#define DEFAULT_PAGECACHE_DEBUG_LOG  "pagecache_debug.log"

#if defined(PAGECACHE_DEBUG) && ! defined(PAGECACHE_DEBUG_LOG)
#define PAGECACHE_DEBUG_LOG  DEFAULT_PAGECACHE_DEBUG_LOG
#endif

#if defined(PAGECACHE_DEBUG_LOG)
static FILE *pagecache_debug_log= NULL;
static void pagecache_debug_print _VARARGS((const char *fmt, ...));
#define PAGECACHE_DEBUG_OPEN                                                  \
          if (!pagecache_debug_log)                                           \
          {                                                                   \
            pagecache_debug_log= fopen(PAGECACHE_DEBUG_LOG, "w");             \
            (void) setvbuf(pagecache_debug_log, NULL, _IOLBF, BUFSIZ);        \
          }

#define PAGECACHE_DEBUG_CLOSE                                                 \
          if (pagecache_debug_log)                                            \
          {                                                                   \
            fclose(pagecache_debug_log);                                      \
            pagecache_debug_log= 0;                                           \
          }
#else
#define PAGECACHE_DEBUG_OPEN
#define PAGECACHE_DEBUG_CLOSE
#endif /* defined(PAGECACHE_DEBUG_LOG) */

#if defined(PAGECACHE_DEBUG_LOG) && defined(PAGECACHE_DEBUG)
#define KEYCACHE_DBUG_PRINT(l, m)                                             \
            { if (pagecache_debug_log)                                        \
                fprintf(pagecache_debug_log, "%s: ", l);                      \
              pagecache_debug_print m; }

#define KEYCACHE_DBUG_ASSERT(a)                                               \
            { if (! (a) && pagecache_debug_log)                               \
                fclose(pagecache_debug_log);                                  \
              assert(a); }
#else
#define KEYCACHE_DBUG_PRINT(l, m)  DBUG_PRINT(l, m)
#define KEYCACHE_DBUG_ASSERT(a)    DBUG_ASSERT(a)
#endif /* defined(PAGECACHE_DEBUG_LOG) && defined(PAGECACHE_DEBUG) */

#if defined(PAGECACHE_DEBUG) || !defined(DBUG_OFF)
#ifdef THREAD
static long pagecache_thread_id;
#define KEYCACHE_THREAD_TRACE(l)                                              \
             KEYCACHE_DBUG_PRINT(l,("|thread %ld",pagecache_thread_id))

#define KEYCACHE_THREAD_TRACE_BEGIN(l)                                        \
            { struct st_my_thread_var *thread_var= my_thread_var;             \
              pagecache_thread_id= thread_var->id;                            \
              KEYCACHE_DBUG_PRINT(l,("[thread %ld",pagecache_thread_id)) }

#define KEYCACHE_THREAD_TRACE_END(l)                                          \
            KEYCACHE_DBUG_PRINT(l,("]thread %ld",pagecache_thread_id))
#else /* THREAD */
#define KEYCACHE_THREAD_TRACE(l)        KEYCACHE_DBUG_PRINT(l,(""))
#define KEYCACHE_THREAD_TRACE_BEGIN(l)  KEYCACHE_DBUG_PRINT(l,(""))
#define KEYCACHE_THREAD_TRACE_END(l)    KEYCACHE_DBUG_PRINT(l,(""))
#endif /* THREAD */
#else
#define KEYCACHE_THREAD_TRACE_BEGIN(l)
#define KEYCACHE_THREAD_TRACE_END(l)
#define KEYCACHE_THREAD_TRACE(l)
#endif /* defined(PAGECACHE_DEBUG) || !defined(DBUG_OFF) */

#define PCBLOCK_NUMBER(p, b)                                                    \
  ((uint) (((char*)(b)-(char *) p->block_root)/sizeof(PAGECACHE_BLOCK_LINK)))
#define PAGECACHE_HASH_LINK_NUMBER(p, h)                                      \
  ((uint) (((char*)(h)-(char *) p->hash_link_root)/                           \
           sizeof(PAGECACHE_HASH_LINK)))

#if (defined(PAGECACHE_TIMEOUT) && !defined(__WIN__)) || defined(PAGECACHE_DEBUG)
static int pagecache_pthread_cond_wait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex);
#else
#define  pagecache_pthread_cond_wait pthread_cond_wait
#endif

#if defined(PAGECACHE_DEBUG)
static int ___pagecache_pthread_mutex_lock(pthread_mutex_t *mutex);
static void ___pagecache_pthread_mutex_unlock(pthread_mutex_t *mutex);
static int ___pagecache_pthread_cond_signal(pthread_cond_t *cond);
#define pagecache_pthread_mutex_lock(M) \
{ DBUG_PRINT("lock", ("mutex lock 0x%lx %u", (ulong)(M), __LINE__)); \
  ___pagecache_pthread_mutex_lock(M);}
#define pagecache_pthread_mutex_unlock(M) \
{ DBUG_PRINT("lock", ("mutex unlock 0x%lx %u", (ulong)(M), __LINE__)); \
  ___pagecache_pthread_mutex_unlock(M);}
#define pagecache_pthread_cond_signal(M) \
{ DBUG_PRINT("lock", ("signal 0x%lx %u", (ulong)(M), __LINE__)); \
  ___pagecache_pthread_cond_signal(M);}
#else
#define pagecache_pthread_mutex_lock pthread_mutex_lock
#define pagecache_pthread_mutex_unlock pthread_mutex_unlock
#define pagecache_pthread_cond_signal pthread_cond_signal
#endif /* defined(PAGECACHE_DEBUG) */

extern my_bool translog_flush(TRANSLOG_ADDRESS lsn);

/*
  Write page to the disk

  SYNOPSIS
    pagecache_fwrite()
    pagecache - page cache pointer
    filedesc  - pagecache file descriptor structure
    buffer    - buffer which we will write
    type      - page type (plain or with LSN)
    flags     - MYF() flags

  RETURN
    0   - OK
    1   - Error
*/

static my_bool pagecache_fwrite(PAGECACHE *pagecache,
                                PAGECACHE_FILE *filedesc,
                                uchar *buffer,
                                pgcache_page_no_t pageno,
                                enum pagecache_page_type type
                                __attribute__((unused)),
                                myf flags)
{
  DBUG_ENTER("pagecache_fwrite");
  DBUG_ASSERT(type != PAGECACHE_READ_UNKNOWN_PAGE);

  /* Todo: Integrate this with write_callback so we have only one callback */
  if ((*filedesc->flush_log_callback)(buffer, pageno, filedesc->callback_data))
    DBUG_RETURN(1);
  DBUG_PRINT("info", ("write_callback: 0x%lx  data: 0x%lx",
                      (ulong) filedesc->write_callback,
                      (ulong) filedesc->callback_data));
  if ((*filedesc->write_callback)(buffer, pageno, filedesc->callback_data))
  {
    DBUG_PRINT("error", ("write callback problem"));
    DBUG_RETURN(1);
  }
  if (my_pwrite(filedesc->file, buffer, pagecache->block_size,
                ((my_off_t) pageno << pagecache->shift), flags))
  {
    (*filedesc->write_fail)(filedesc->callback_data);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  Read page from the disk

  SYNOPSIS
    pagecache_fread()
    pagecache - page cache pointer
    filedesc  - pagecache file descriptor structure
    buffer    - buffer in which we will read
    pageno    - page number
    flags     - MYF() flags
*/
#define pagecache_fread(pagecache, filedesc, buffer, pageno, flags) \
  my_pread((filedesc)->file, buffer, pagecache->block_size,         \
           ((my_off_t) pageno << pagecache->shift), flags)


/**
  @brief set rec_lsn of pagecache block (if it is needed)

  @param block                   block where to set rec_lsn
  @param first_REDO_LSN_for_page the LSN to set
*/

static inline void pagecache_set_block_rec_lsn(PAGECACHE_BLOCK_LINK *block,
                                               LSN first_REDO_LSN_for_page)
{
  if (block->rec_lsn == LSN_MAX)
    block->rec_lsn= first_REDO_LSN_for_page;
  else
    DBUG_ASSERT(cmp_translog_addr(block->rec_lsn,
                                  first_REDO_LSN_for_page) <= 0);
}


/*
  next_power(value) is 2 at the power of (1+floor(log2(value)));
  e.g. next_power(2)=4, next_power(3)=4.
*/
static inline uint next_power(uint value)
{
  return (uint) my_round_up_to_next_power((uint32) value) << 1;
}


/*
  Initialize a page cache

  SYNOPSIS
    init_pagecache()
    pagecache			pointer to a page cache data structure
    key_cache_block_size	size of blocks to keep cached data
    use_mem                     total memory to use for the key cache
    division_limit		division limit (may be zero)
    age_threshold		age threshold (may be zero)
    block_size                  size of block (should be power of 2)
    my_read_flags		Flags used for all pread/pwrite calls
			        Usually MY_WME in case of recovery

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.

  NOTES.
    if pagecache->inited != 0 we assume that the key cache
    is already initialized.  This is for now used by myisamchk, but shouldn't
    be something that a program should rely on!

    It's assumed that no two threads call this function simultaneously
    referring to the same key cache handle.

*/

ulong init_pagecache(PAGECACHE *pagecache, size_t use_mem,
                     uint division_limit, uint age_threshold,
                     uint block_size, myf my_readwrite_flags)
{
  ulong blocks, hash_links, length;
  int error;
  DBUG_ENTER("init_pagecache");
  DBUG_ASSERT(block_size >= 512);

  PAGECACHE_DEBUG_OPEN;
  if (pagecache->inited && pagecache->disk_blocks > 0)
  {
    DBUG_PRINT("warning",("key cache already in use"));
    DBUG_RETURN(0);
  }

  pagecache->global_cache_w_requests= pagecache->global_cache_r_requests= 0;
  pagecache->global_cache_read= pagecache->global_cache_write= 0;
  pagecache->disk_blocks= -1;
  if (! pagecache->inited)
  {
    if (pthread_mutex_init(&pagecache->cache_lock, MY_MUTEX_INIT_FAST) ||
        hash_init(&pagecache->files_in_flush, &my_charset_bin, 32,
                  offsetof(struct st_file_in_flush, file),
                  sizeof(((struct st_file_in_flush *)NULL)->file),
                  NULL, NULL, 0))
      goto err;
    pagecache->inited= 1;
    pagecache->in_init= 0;
    pagecache->resize_queue.last_thread= NULL;
  }

  pagecache->mem_size= use_mem;
  pagecache->block_size= block_size;
  pagecache->shift= my_bit_log2(block_size);
  pagecache->readwrite_flags= my_readwrite_flags | MY_NABP | MY_WAIT_IF_FULL;
  pagecache->org_readwrite_flags= pagecache->readwrite_flags;
  DBUG_PRINT("info", ("block_size: %u", block_size));
  DBUG_ASSERT(((uint)(1 << pagecache->shift)) == block_size);

  blocks= (ulong) (use_mem / (sizeof(PAGECACHE_BLOCK_LINK) +
                              2 * sizeof(PAGECACHE_HASH_LINK) +
                              sizeof(PAGECACHE_HASH_LINK*) *
                              5/4 + block_size));
  /*
    We need to support page cache with just one block to be able to do
    scanning of rows-in-block files
  */
  for ( ; ; )
  {
    if (blocks < 8)
    {
      my_message(ENOMEM, "Not enough memory to allocate 8 pagecache pages",
                 MYF(0));
      my_errno= ENOMEM;
      goto err;
    }
    /* Set my_hash_entries to the next bigger 2 power */
    if ((pagecache->hash_entries= next_power(blocks)) <
        (blocks) * 5/4)
      pagecache->hash_entries<<= 1;
    hash_links= 2 * blocks;
#if defined(MAX_THREADS)
    if (hash_links < MAX_THREADS + blocks - 1)
      hash_links= MAX_THREADS + blocks - 1;
#endif
    while ((length= (ALIGN_SIZE(blocks * sizeof(PAGECACHE_BLOCK_LINK)) +
                     ALIGN_SIZE(hash_links * sizeof(PAGECACHE_HASH_LINK)) +
                     ALIGN_SIZE(sizeof(PAGECACHE_HASH_LINK*) *
                                pagecache->hash_entries))) +
           (blocks << pagecache->shift) > use_mem)
      blocks--;
    /* Allocate memory for cache page buffers */
    if ((pagecache->block_mem=
         my_large_malloc((ulong) blocks * pagecache->block_size,
                         MYF(MY_WME))))
    {
      /*
        Allocate memory for blocks, hash_links and hash entries;
        For each block 2 hash links are allocated
      */
      if ((pagecache->block_root=
           (PAGECACHE_BLOCK_LINK*) my_malloc((size_t) length, MYF(0))))
        break;
      my_large_free(pagecache->block_mem, MYF(0));
      pagecache->block_mem= 0;
    }
    blocks= blocks / 4*3;
  }
  pagecache->blocks_unused= blocks;
  pagecache->disk_blocks= (long) blocks;
  pagecache->hash_links= hash_links;
  pagecache->hash_root=
    (PAGECACHE_HASH_LINK**) ((char*) pagecache->block_root +
                             ALIGN_SIZE(blocks*sizeof(PAGECACHE_BLOCK_LINK)));
  pagecache->hash_link_root=
    (PAGECACHE_HASH_LINK*) ((char*) pagecache->hash_root +
                            ALIGN_SIZE((sizeof(PAGECACHE_HASH_LINK*) *
                                        pagecache->hash_entries)));
  bzero((uchar*) pagecache->block_root,
        pagecache->disk_blocks * sizeof(PAGECACHE_BLOCK_LINK));
  bzero((uchar*) pagecache->hash_root,
        pagecache->hash_entries * sizeof(PAGECACHE_HASH_LINK*));
  bzero((uchar*) pagecache->hash_link_root,
        pagecache->hash_links * sizeof(PAGECACHE_HASH_LINK));
  pagecache->hash_links_used= 0;
  pagecache->free_hash_list= NULL;
  pagecache->blocks_used= pagecache->blocks_changed= 0;

  pagecache->global_blocks_changed= 0;
  pagecache->blocks_available=0;		/* For debugging */

  /* The LRU chain is empty after initialization */
  pagecache->used_last= NULL;
  pagecache->used_ins= NULL;
  pagecache->free_block_list= NULL;
  pagecache->time= 0;
  pagecache->warm_blocks= 0;
  pagecache->min_warm_blocks= (division_limit ?
                               blocks * division_limit / 100 + 1 :
                               blocks);
  pagecache->age_threshold= (age_threshold ?
                             blocks * age_threshold / 100 :
                             blocks);

  pagecache->cnt_for_resize_op= 0;
  pagecache->resize_in_flush= 0;
  pagecache->can_be_used= 1;

  pagecache->waiting_for_hash_link.last_thread= NULL;
  pagecache->waiting_for_block.last_thread= NULL;
  DBUG_PRINT("exit",
             ("disk_blocks: %ld  block_root: 0x%lx  hash_entries: %ld\
 hash_root: 0x%lx  hash_links: %ld  hash_link_root: 0x%lx",
              pagecache->disk_blocks, (long) pagecache->block_root,
              pagecache->hash_entries, (long) pagecache->hash_root,
              pagecache->hash_links, (long) pagecache->hash_link_root));
  bzero((uchar*) pagecache->changed_blocks,
        sizeof(pagecache->changed_blocks[0]) *
        PAGECACHE_CHANGED_BLOCKS_HASH);
  bzero((uchar*) pagecache->file_blocks,
        sizeof(pagecache->file_blocks[0]) *
        PAGECACHE_CHANGED_BLOCKS_HASH);

  pagecache->blocks= pagecache->disk_blocks > 0 ? pagecache->disk_blocks : 0;
  DBUG_RETURN((ulong) pagecache->disk_blocks);

err:
  error= my_errno;
  pagecache->disk_blocks= 0;
  pagecache->blocks=  0;
  if (pagecache->block_mem)
  {
    my_large_free(pagecache->block_mem, MYF(0));
    pagecache->block_mem= NULL;
  }
  if (pagecache->block_root)
  {
    my_free(pagecache->block_root, MYF(0));
    pagecache->block_root= NULL;
  }
  my_errno= error;
  pagecache->can_be_used= 0;
  DBUG_RETURN(0);
}


/*
  Flush all blocks in the key cache to disk
*/

#ifdef NOT_USED
static int flush_all_key_blocks(PAGECACHE *pagecache)
{
#if defined(PAGECACHE_DEBUG)
  uint cnt=0;
#endif
  while (pagecache->blocks_changed > 0)
  {
    PAGECACHE_BLOCK_LINK *block;
    for (block= pagecache->used_last->next_used ; ; block=block->next_used)
    {
      if (block->hash_link)
      {
#if defined(PAGECACHE_DEBUG)
        cnt++;
        KEYCACHE_DBUG_ASSERT(cnt <= pagecache->blocks_used);
#endif
        if (flush_pagecache_blocks_int(pagecache, &block->hash_link->file,
                                       FLUSH_RELEASE, NULL, NULL))
          return 1;
        break;
      }
      if (block == pagecache->used_last)
        break;
    }
  }
  return 0;
}
#endif /* NOT_USED */

/*
  Resize a key cache

  SYNOPSIS
    resize_pagecache()
    pagecache                   pointer to a page cache data structure
    use_mem			total memory to use for the new key cache
    division_limit		new division limit (if not zero)
    age_threshold		new age threshold (if not zero)

  RETURN VALUE
    number of blocks in the key cache, if successful,
    0 - otherwise.

  NOTES.
    The function first compares the memory size parameter
    with the key cache value.

    If they differ the function free the the memory allocated for the
    old key cache blocks by calling the end_pagecache function and
    then rebuilds the key cache with new blocks by calling
    init_key_cache.

    The function starts the operation only when all other threads
    performing operations with the key cache let her to proceed
    (when cnt_for_resize=0).

     Before being usable, this function needs:
     - to receive fixes for BUG#17332 "changing key_buffer_size on a running
     server can crash under load" similar to those done to the key cache
     - to have us (Sanja) look at the additional constraints placed on
     resizing, due to the page locking specific to this page cache.
     So we disable it for now.
*/
#if NOT_USED /* keep disabled until code is fixed see above !! */
ulong resize_pagecache(PAGECACHE *pagecache,
                       size_t use_mem, uint division_limit,
                       uint age_threshold)
{
  ulong blocks;
#ifdef THREAD
  struct st_my_thread_var *thread;
  WQUEUE *wqueue;

#endif
  DBUG_ENTER("resize_pagecache");

  if (!pagecache->inited)
    DBUG_RETURN(pagecache->disk_blocks);

  if(use_mem == pagecache->mem_size)
  {
    change_pagecache_param(pagecache, division_limit, age_threshold);
    DBUG_RETURN(pagecache->disk_blocks);
  }

  pagecache_pthread_mutex_lock(&pagecache->cache_lock);

#ifdef THREAD
  wqueue= &pagecache->resize_queue;
  thread= my_thread_var;
  wqueue_link_into_queue(wqueue, thread);

  while (wqueue->last_thread->next != thread)
  {
    pagecache_pthread_cond_wait(&thread->suspend, &pagecache->cache_lock);
  }
#endif

  pagecache->resize_in_flush= 1;
  if (flush_all_key_blocks(pagecache))
  {
    /* TODO: if this happens, we should write a warning in the log file ! */
    pagecache->resize_in_flush= 0;
    blocks= 0;
    pagecache->can_be_used= 0;
    goto finish;
  }
  pagecache->resize_in_flush= 0;
  pagecache->can_be_used= 0;
#ifdef THREAD
  while (pagecache->cnt_for_resize_op)
  {
    KEYCACHE_DBUG_PRINT("resize_pagecache: wait",
                        ("suspend thread %ld", thread->id));
    pagecache_pthread_cond_wait(&thread->suspend, &pagecache->cache_lock);
  }
#else
  KEYCACHE_DBUG_ASSERT(pagecache->cnt_for_resize_op == 0);
#endif

  end_pagecache(pagecache, 0);			/* Don't free mutex */
  /* The following will work even if use_mem is 0 */
  blocks= init_pagecache(pagecache, pagecache->block_size, use_mem,
			 division_limit, age_threshold,
                         pagecache->readwrite_flags);

finish:
#ifdef THREAD
  wqueue_unlink_from_queue(wqueue, thread);
  /* Signal for the next resize request to proceeed if any */
  if (wqueue->last_thread)
  {
    KEYCACHE_DBUG_PRINT("resize_pagecache: signal",
                        ("thread %ld", wqueue->last_thread->next->id));
    pagecache_pthread_cond_signal(&wqueue->last_thread->next->suspend);
  }
#endif
  pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
  DBUG_RETURN(blocks);
}
#endif /* 0 */


/*
  Increment counter blocking resize key cache operation
*/
static inline void inc_counter_for_resize_op(PAGECACHE *pagecache)
{
  safe_mutex_assert_owner(&pagecache->cache_lock);
  pagecache->cnt_for_resize_op++;
}


/*
  Decrement counter blocking resize key cache operation;
  Signal the operation to proceed when counter becomes equal zero
*/
static inline void dec_counter_for_resize_op(PAGECACHE *pagecache)
{
#ifdef THREAD
  struct st_my_thread_var *last_thread;
  safe_mutex_assert_owner(&pagecache->cache_lock);
  if (!--pagecache->cnt_for_resize_op &&
      (last_thread= pagecache->resize_queue.last_thread))
  {
    KEYCACHE_DBUG_PRINT("dec_counter_for_resize_op: signal",
                        ("thread %ld", last_thread->next->id));
    pagecache_pthread_cond_signal(&last_thread->next->suspend);
  }
#else
  pagecache->cnt_for_resize_op--;
#endif
}

/*
  Change the page cache parameters

  SYNOPSIS
    change_pagecache_param()
    pagecache			pointer to a page cache data structure
    division_limit		new division limit (if not zero)
    age_threshold		new age threshold (if not zero)

  RETURN VALUE
    none

  NOTES.
    Presently the function resets the key cache parameters
    concerning midpoint insertion strategy - division_limit and
    age_threshold.
*/

void change_pagecache_param(PAGECACHE *pagecache, uint division_limit,
			    uint age_threshold)
{
  DBUG_ENTER("change_pagecache_param");

  pagecache_pthread_mutex_lock(&pagecache->cache_lock);
  if (division_limit)
    pagecache->min_warm_blocks= (pagecache->disk_blocks *
				division_limit / 100 + 1);
  if (age_threshold)
    pagecache->age_threshold=   (pagecache->disk_blocks *
				age_threshold / 100);
  pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
  DBUG_VOID_RETURN;
}


/*
  Check that pagecache was used and cleaned up properly.
*/

#ifndef DBUG_OFF
void check_pagecache_is_cleaned_up(PAGECACHE *pagecache)
{
  DBUG_ENTER("check_pagecache_is_cleaned_up");
  /*
    Ensure we called inc_counter_for_resize_op and dec_counter_for_resize_op
    the same number of times. (If not, a resize() could never happen.
  */
  DBUG_ASSERT(pagecache->cnt_for_resize_op == 0);

  if (pagecache->disk_blocks > 0)
  {
    if (pagecache->block_mem)
    {
      uint i;
      for (i=0 ; i < pagecache->blocks_used ; i++)
      {
        DBUG_ASSERT(pagecache->block_root[i].status == 0);
        DBUG_ASSERT(pagecache->block_root[i].type == PAGECACHE_EMPTY_PAGE);
      }
    }
  }
  DBUG_VOID_RETURN;
}
#endif


/*
  Removes page cache from memory. Does NOT flush pages to disk.

  SYNOPSIS
    end_pagecache()
    pagecache		page cache handle
    cleanup		Complete free (Free also mutex for key cache)

  RETURN VALUE
    none
*/

void end_pagecache(PAGECACHE *pagecache, my_bool cleanup)
{
  DBUG_ENTER("end_pagecache");
  DBUG_PRINT("enter", ("key_cache: 0x%lx", (long) pagecache));

  if (!pagecache->inited)
    DBUG_VOID_RETURN;

  if (pagecache->disk_blocks > 0)
  {
#ifndef DBUG_OFF
    check_pagecache_is_cleaned_up(pagecache);
#endif

    if (pagecache->block_mem)
    {
      my_large_free(pagecache->block_mem, MYF(0));
      pagecache->block_mem= NULL;
      my_free(pagecache->block_root, MYF(0));
      pagecache->block_root= NULL;
    }
    pagecache->disk_blocks= -1;
    /* Reset blocks_changed to be safe if flush_all_key_blocks is called */
    pagecache->blocks_changed= 0;
  }

  DBUG_PRINT("status", ("used: %lu  changed: %lu  w_requests: %lu  "
                        "writes: %lu  r_requests: %lu  reads: %lu",
                        pagecache->blocks_used,
                        pagecache->global_blocks_changed,
                        (ulong) pagecache->global_cache_w_requests,
                        (ulong) pagecache->global_cache_write,
                        (ulong) pagecache->global_cache_r_requests,
                        (ulong) pagecache->global_cache_read));

  if (cleanup)
  {
    hash_free(&pagecache->files_in_flush);
    pthread_mutex_destroy(&pagecache->cache_lock);
    pagecache->inited= pagecache->can_be_used= 0;
    PAGECACHE_DEBUG_CLOSE;
  }
  DBUG_VOID_RETURN;
} /* end_pagecache */


/*
  Unlink a block from the chain of dirty/clean blocks
*/

static inline void unlink_changed(PAGECACHE_BLOCK_LINK *block)
{
  if (block->next_changed)
    block->next_changed->prev_changed= block->prev_changed;
  *block->prev_changed= block->next_changed;
}


/*
  Link a block into the chain of dirty/clean blocks
*/

static inline void link_changed(PAGECACHE_BLOCK_LINK *block,
                                PAGECACHE_BLOCK_LINK **phead)
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

static void link_to_file_list(PAGECACHE *pagecache,
                              PAGECACHE_BLOCK_LINK *block,
                              PAGECACHE_FILE *file, my_bool unlink_flag)
{
  if (unlink_flag)
    unlink_changed(block);
  link_changed(block, &pagecache->file_blocks[FILE_HASH(*file)]);
  if (block->status & PCBLOCK_CHANGED)
  {
    block->status&= ~(PCBLOCK_CHANGED | PCBLOCK_DEL_WRITE);
    block->rec_lsn= LSN_MAX;
    pagecache->blocks_changed--;
    pagecache->global_blocks_changed--;
  }
}


/*
  Unlink a block from the chain of clean blocks for the specified
  file and link it to the chain of dirty blocks for this file
*/

static inline void link_to_changed_list(PAGECACHE *pagecache,
                                        PAGECACHE_BLOCK_LINK *block)
{
  unlink_changed(block);
  link_changed(block,
               &pagecache->changed_blocks[FILE_HASH(block->hash_link->file)]);
  block->status|=PCBLOCK_CHANGED;
  pagecache->blocks_changed++;
  pagecache->global_blocks_changed++;
}


/*
  Link a block to the LRU chain at the beginning or at the end of
  one of two parts.

  SYNOPSIS
    link_block()
      pagecache            pointer to a page cache data structure
      block               pointer to the block to link to the LRU chain
      hot                 <-> to link the block into the hot subchain
      at_end              <-> to link the block at the end of the subchain

  RETURN VALUE
    none

  NOTES.
    The LRU chain is represented by a circular list of block structures.
    The list is double-linked of the type (**prev,*next) type.
    The LRU chain is divided into two parts - hot and warm.
    There are two pointers to access the last blocks of these two
    parts. The beginning of the warm part follows right after the
    end of the hot part.
    Only blocks of the warm part can be used for replacement.
    The first block from the beginning of this subchain is always
    taken for eviction (pagecache->last_used->next)

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

static void link_block(PAGECACHE *pagecache, PAGECACHE_BLOCK_LINK *block,
                       my_bool hot, my_bool at_end)
{
  PAGECACHE_BLOCK_LINK *ins;
  PAGECACHE_BLOCK_LINK **ptr_ins;

  PCBLOCK_INFO(block);
  KEYCACHE_DBUG_ASSERT(! (block->hash_link && block->hash_link->requests));
#ifdef THREAD
  if (!hot && pagecache->waiting_for_block.last_thread)
  {
    /* Signal that in the LRU warm sub-chain an available block has appeared */
    struct st_my_thread_var *last_thread=
                               pagecache->waiting_for_block.last_thread;
    struct st_my_thread_var *first_thread= last_thread->next;
    struct st_my_thread_var *next_thread= first_thread;
    PAGECACHE_HASH_LINK *hash_link=
      (PAGECACHE_HASH_LINK *) first_thread->opt_info;
    struct st_my_thread_var *thread;
    do
    {
      thread= next_thread;
      next_thread= thread->next;
      /*
         We notify about the event all threads that ask
         for the same page as the first thread in the queue
      */
      if ((PAGECACHE_HASH_LINK *) thread->opt_info == hash_link)
      {
        KEYCACHE_DBUG_PRINT("link_block: signal", ("thread: %ld", thread->id));
        pagecache_pthread_cond_signal(&thread->suspend);
        wqueue_unlink_from_queue(&pagecache->waiting_for_block, thread);
        block->requests++;
      }
    }
    while (thread != last_thread);
    hash_link->block= block;
    KEYCACHE_THREAD_TRACE("link_block: after signaling");
#if defined(PAGECACHE_DEBUG)
    KEYCACHE_DBUG_PRINT("link_block",
        ("linked,unlinked block: %u  status: %x  #requests: %u  #available: %u",
         PCBLOCK_NUMBER(pagecache, block), block->status,
         block->requests, pagecache->blocks_available));
#endif
    return;
  }
#else /* THREAD */
  KEYCACHE_DBUG_ASSERT(! (!hot && pagecache->waiting_for_block.last_thread));
  /* Condition not transformed using DeMorgan, to keep the text identical */
#endif /* THREAD */
  ptr_ins= hot ? &pagecache->used_ins : &pagecache->used_last;
  ins= *ptr_ins;
  if (ins)
  {
    ins->next_used->prev_used= &block->next_used;
    block->next_used= ins->next_used;
    block->prev_used= &ins->next_used;
    ins->next_used= block;
    if (at_end)
      *ptr_ins= block;
  }
  else
  {
    /* The LRU chain is empty */
    pagecache->used_last= pagecache->used_ins= block->next_used= block;
    block->prev_used= &block->next_used;
  }
  KEYCACHE_THREAD_TRACE("link_block");
#if defined(PAGECACHE_DEBUG)
  pagecache->blocks_available++;
  KEYCACHE_DBUG_PRINT("link_block",
                      ("linked block: %u:%1u  status: %x  #requests: %u  #available: %u",
                       PCBLOCK_NUMBER(pagecache, block), at_end, block->status,
                       block->requests, pagecache->blocks_available));
  KEYCACHE_DBUG_ASSERT((ulong) pagecache->blocks_available <=
                       pagecache->blocks_used);
#endif
}


/*
  Unlink a block from the LRU chain

  SYNOPSIS
    unlink_block()
      pagecache            pointer to a page cache data structure
      block               pointer to the block to unlink from the LRU chain

  RETURN VALUE
    none

  NOTES.
    See NOTES for link_block
*/

static void unlink_block(PAGECACHE *pagecache, PAGECACHE_BLOCK_LINK *block)
{
  DBUG_ENTER("unlink_block");
  DBUG_PRINT("unlink_block", ("unlink 0x%lx", (ulong)block));
  DBUG_ASSERT(block->next_used != NULL);
  if (block->next_used == block)
  {
    /* The list contains only one member */
    pagecache->used_last= pagecache->used_ins= NULL;
  }
  else
  {
    block->next_used->prev_used= block->prev_used;
    *block->prev_used= block->next_used;
    if (pagecache->used_last == block)
      pagecache->used_last= STRUCT_PTR(PAGECACHE_BLOCK_LINK,
                                       next_used, block->prev_used);
    if (pagecache->used_ins == block)
      pagecache->used_ins= STRUCT_PTR(PAGECACHE_BLOCK_LINK,
                                      next_used, block->prev_used);
  }
  block->next_used= NULL;

  KEYCACHE_THREAD_TRACE("unlink_block");
#if defined(PAGECACHE_DEBUG)
  KEYCACHE_DBUG_ASSERT(pagecache->blocks_available != 0);
  pagecache->blocks_available--;
  KEYCACHE_DBUG_PRINT("unlink_block",
                      ("unlinked block: 0x%lx (%u)  status: %x   #requests: %u  #available: %u",
                       (ulong)block, PCBLOCK_NUMBER(pagecache, block),
                       block->status,
                       block->requests, pagecache->blocks_available));
  PCBLOCK_INFO(block);
#endif
  DBUG_VOID_RETURN;
}


/*
  Register requests for a block

  SYNOPSIS
    reg_requests()
    pagecache            this page cache reference
    block                the block we request reference
    count                how many requests we register (it is 1 everywhere)

  NOTE
  Registration of request means we are going to use this block so we exclude
  it from the LRU if it is first request
*/
static void reg_requests(PAGECACHE *pagecache, PAGECACHE_BLOCK_LINK *block,
                         int count)
{
  DBUG_ENTER("reg_requests");
  DBUG_PRINT("enter", ("block: 0x%lx (%u)  status: %x  reqs: %u",
		       (ulong)block, PCBLOCK_NUMBER(pagecache, block),
                       block->status, block->requests));
  PCBLOCK_INFO(block);
  if (! block->requests)
    /* First request for the block unlinks it */
    unlink_block(pagecache, block);
  block->requests+= count;
  DBUG_VOID_RETURN;
}


/*
  Unregister request for a block
  linking it to the LRU chain if it's the last request

  SYNOPSIS
    unreg_request()
    pagecache            pointer to a page cache data structure
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

static void unreg_request(PAGECACHE *pagecache,
                          PAGECACHE_BLOCK_LINK *block, int at_end)
{
  DBUG_ENTER("unreg_request");
  DBUG_PRINT("enter", ("block 0x%lx (%u)  status: %x  reqs: %u",
		       (ulong)block, PCBLOCK_NUMBER(pagecache, block),
                       block->status, block->requests));
  PCBLOCK_INFO(block);
  DBUG_ASSERT(block->requests > 0);
  if (! --block->requests)
  {
    my_bool hot;
    if (block->hits_left)
      block->hits_left--;
    hot= !block->hits_left && at_end &&
      pagecache->warm_blocks > pagecache->min_warm_blocks;
    if (hot)
    {
      if (block->temperature == PCBLOCK_WARM)
        pagecache->warm_blocks--;
      block->temperature= PCBLOCK_HOT;
      KEYCACHE_DBUG_PRINT("unreg_request", ("#warm_blocks: %lu",
                           pagecache->warm_blocks));
    }
    link_block(pagecache, block, hot, (my_bool)at_end);
    block->last_hit_time= pagecache->time;
    pagecache->time++;

    block= pagecache->used_ins;
    /* Check if we should link a hot block to the warm block */
    if (block && pagecache->time - block->last_hit_time >
	pagecache->age_threshold)
    {
      unlink_block(pagecache, block);
      link_block(pagecache, block, 0, 0);
      if (block->temperature != PCBLOCK_WARM)
      {
        pagecache->warm_blocks++;
        block->temperature= PCBLOCK_WARM;
      }
      KEYCACHE_DBUG_PRINT("unreg_request", ("#warm_blocks: %lu",
                           pagecache->warm_blocks));
    }
  }
  DBUG_VOID_RETURN;
}

/*
  Remove a reader of the page in block
*/

static inline void remove_reader(PAGECACHE_BLOCK_LINK *block)
{
  DBUG_ENTER("remove_reader");
  PCBLOCK_INFO(block);
  DBUG_ASSERT(block->hash_link->requests > 0);
#ifdef THREAD
  if (! --block->hash_link->requests && block->condvar)
    pagecache_pthread_cond_signal(block->condvar);
#else
  --block->hash_link->requests;
#endif
  DBUG_VOID_RETURN;
}


/*
  Wait until the last reader of the page in block
  signals on its termination
*/

static inline void wait_for_readers(PAGECACHE *pagecache
                                    __attribute__((unused)),
                                    PAGECACHE_BLOCK_LINK *block)
{
#ifdef THREAD
  struct st_my_thread_var *thread= my_thread_var;
  while (block->hash_link->requests)
  {
    KEYCACHE_DBUG_PRINT("wait_for_readers: wait",
                        ("suspend thread: %ld  block: %u",
                         thread->id, PCBLOCK_NUMBER(pagecache, block)));
    block->condvar= &thread->suspend;
    pagecache_pthread_cond_wait(&thread->suspend, &pagecache->cache_lock);
    block->condvar= NULL;
  }
#else
  KEYCACHE_DBUG_ASSERT(block->hash_link->requests == 0);
#endif
}


/*
  Add a hash link to a bucket in the hash_table
*/

static inline void link_hash(PAGECACHE_HASH_LINK **start,
                             PAGECACHE_HASH_LINK *hash_link)
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

static void unlink_hash(PAGECACHE *pagecache, PAGECACHE_HASH_LINK *hash_link)
{
  KEYCACHE_DBUG_PRINT("unlink_hash", ("fd: %u  pos_ %lu  #requests=%u",
      (uint) hash_link->file.file, (ulong) hash_link->pageno,
      hash_link->requests));
  KEYCACHE_DBUG_ASSERT(hash_link->requests == 0);
  if ((*hash_link->prev= hash_link->next))
    hash_link->next->prev= hash_link->prev;
  hash_link->block= NULL;
#ifdef THREAD
  if (pagecache->waiting_for_hash_link.last_thread)
  {
    /* Signal that a free hash link has appeared */
    struct st_my_thread_var *last_thread=
                               pagecache->waiting_for_hash_link.last_thread;
    struct st_my_thread_var *first_thread= last_thread->next;
    struct st_my_thread_var *next_thread= first_thread;
    PAGECACHE_PAGE *first_page= (PAGECACHE_PAGE *) (first_thread->opt_info);
    struct st_my_thread_var *thread;

    hash_link->file= first_page->file;
    DBUG_ASSERT(first_page->pageno < ((ULL(1)) << 40));
    hash_link->pageno= first_page->pageno;
    do
    {
      PAGECACHE_PAGE *page;
      thread= next_thread;
      page= (PAGECACHE_PAGE *) thread->opt_info;
      next_thread= thread->next;
      /*
         We notify about the event all threads that ask
         for the same page as the first thread in the queue
      */
      if (page->file.file == hash_link->file.file &&
          page->pageno == hash_link->pageno)
      {
        KEYCACHE_DBUG_PRINT("unlink_hash: signal", ("thread %ld", thread->id));
        pagecache_pthread_cond_signal(&thread->suspend);
        wqueue_unlink_from_queue(&pagecache->waiting_for_hash_link, thread);
      }
    }
    while (thread != last_thread);
    link_hash(&pagecache->hash_root[PAGECACHE_HASH(pagecache,
                                                   hash_link->file,
                                                   hash_link->pageno)],
              hash_link);
    return;
  }
#else /* THREAD */
  KEYCACHE_DBUG_ASSERT(! (pagecache->waiting_for_hash_link.last_thread));
#endif /* THREAD */
  hash_link->next= pagecache->free_hash_list;
  pagecache->free_hash_list= hash_link;
}


/*
  Get the hash link for the page if it is in the cache (do not put the
  page in the cache if it is absent there)

  SYNOPSIS
    get_present_hash_link()
    pagecache            Pagecache reference
    file                 file ID
    pageno               page number in the file
    start                where to put pointer to found hash bucket (for
                         direct referring it)

  RETURN
    found hashlink pointer
*/

static PAGECACHE_HASH_LINK *get_present_hash_link(PAGECACHE *pagecache,
                                                  PAGECACHE_FILE *file,
                                                  pgcache_page_no_t pageno,
                                                  PAGECACHE_HASH_LINK ***start)
{
  reg1 PAGECACHE_HASH_LINK *hash_link;
#if defined(PAGECACHE_DEBUG)
  int cnt;
#endif
  DBUG_ENTER("get_present_hash_link");

  KEYCACHE_DBUG_PRINT("get_present_hash_link", ("fd: %u  pos: %lu",
                      (uint) file->file, (ulong) pageno));

  /*
     Find the bucket in the hash table for the pair (file, pageno);
     start contains the head of the bucket list,
     hash_link points to the first member of the list
  */
  hash_link= *(*start= &pagecache->hash_root[PAGECACHE_HASH(pagecache,
                                                            *file, pageno)]);
#if defined(PAGECACHE_DEBUG)
  cnt= 0;
#endif
  /* Look for an element for the pair (file, pageno) in the bucket chain */
  while (hash_link &&
         (hash_link->pageno != pageno ||
          hash_link->file.file != file->file))
  {
    hash_link= hash_link->next;
#if defined(PAGECACHE_DEBUG)
    cnt++;
    if (! (cnt <= pagecache->hash_links_used))
    {
      int i;
      for (i=0, hash_link= **start ;
           i < cnt ; i++, hash_link= hash_link->next)
      {
        KEYCACHE_DBUG_PRINT("get_present_hash_link", ("fd: %u  pos: %lu",
            (uint) hash_link->file.file, (ulong) hash_link->pageno));
      }
    }
    KEYCACHE_DBUG_ASSERT(cnt <= pagecache->hash_links_used);
#endif
  }
  if (hash_link)
  {
    /* Register the request for the page */
    hash_link->requests++;
  }
  /*
    As soon as the caller will release the page cache's lock, "hash_link"
    will be potentially obsolete (unusable) information.
  */
  DBUG_RETURN(hash_link);
}


/*
  Get the hash link for a page
*/

static PAGECACHE_HASH_LINK *get_hash_link(PAGECACHE *pagecache,
                                          PAGECACHE_FILE *file,
                                          pgcache_page_no_t pageno)
{
  reg1 PAGECACHE_HASH_LINK *hash_link;
  PAGECACHE_HASH_LINK **start;

  KEYCACHE_DBUG_PRINT("get_hash_link", ("fd: %u  pos: %lu",
                      (uint) file->file, (ulong) pageno));

restart:
  /* try to find the page in the cache */
  hash_link= get_present_hash_link(pagecache, file, pageno,
                                   &start);
  if (!hash_link)
  {
    /* There is no hash link in the hash table for the pair (file, pageno) */
    if (pagecache->free_hash_list)
    {
      hash_link= pagecache->free_hash_list;
      pagecache->free_hash_list= hash_link->next;
    }
    else if (pagecache->hash_links_used < pagecache->hash_links)
    {
      hash_link= &pagecache->hash_link_root[pagecache->hash_links_used++];
    }
    else
    {
#ifdef THREAD
      /* Wait for a free hash link */
      struct st_my_thread_var *thread= my_thread_var;
      PAGECACHE_PAGE page;
      KEYCACHE_DBUG_PRINT("get_hash_link", ("waiting"));
      page.file= *file;
      page.pageno= pageno;
      thread->opt_info= (void *) &page;
      wqueue_link_into_queue(&pagecache->waiting_for_hash_link, thread);
      KEYCACHE_DBUG_PRINT("get_hash_link: wait",
                        ("suspend thread %ld", thread->id));
      pagecache_pthread_cond_wait(&thread->suspend,
                                 &pagecache->cache_lock);
      thread->opt_info= NULL;
#else
      KEYCACHE_DBUG_ASSERT(0);
#endif
      DBUG_PRINT("info", ("restarting..."));
      goto restart;
    }
    hash_link->file= *file;
    DBUG_ASSERT(pageno < ((ULL(1)) << 40));
    hash_link->pageno= pageno;
    link_hash(start, hash_link);
    /* Register the request for the page */
    hash_link->requests++;
  }

  return hash_link;
}


/*
  Get a block for the file page requested by a pagecache read/write operation;
  If the page is not in the cache return a free block, if there is none
  return the lru block after saving its buffer if the page is dirty.

  SYNOPSIS

    find_block()
      pagecache            pointer to a page cache data structure
      file                handler for the file to read page from
      pageno              number of the page in the file
      init_hits_left      how initialize the block counter for the page
      wrmode              <-> get for writing
      reg_req             Register request to thye page
      page_st        out  {PAGE_READ,PAGE_TO_BE_READ,PAGE_WAIT_TO_BE_READ}

  RETURN VALUE
    Pointer to the found block if successful, 0 - otherwise

  NOTES.
    For the page from file positioned at pageno the function checks whether
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
    If an error occurs THE PCBLOCK_ERROR bit is set in the block status.
    It might happen that there are no blocks in LRU chain (in warm part) -
    all blocks  are unlinked for some read/write operations. Then the function
    waits until first of this operations links any block back.
*/

static PAGECACHE_BLOCK_LINK *find_block(PAGECACHE *pagecache,
                                        PAGECACHE_FILE *file,
                                        pgcache_page_no_t pageno,
                                        int init_hits_left,
                                        my_bool wrmode,
                                        my_bool reg_req,
                                        int *page_st)
{
  PAGECACHE_HASH_LINK *hash_link;
  PAGECACHE_BLOCK_LINK *block;
  int error= 0;
  int page_status;

  DBUG_ENTER("find_block");
  KEYCACHE_THREAD_TRACE("find_block:begin");
  DBUG_PRINT("enter", ("fd: %d  pos: %lu  wrmode: %d",
                       file->file, (ulong) pageno, wrmode));
  KEYCACHE_DBUG_PRINT("find_block", ("fd: %d  pos: %lu  wrmode: %d",
                                     file->file, (ulong) pageno,
                                     wrmode));
#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_pagecache",
               test_key_cache(pagecache, "start of find_block", 0););
#endif

restart:
  /* Find the hash link for the requested page (file, pageno) */
  hash_link= get_hash_link(pagecache, file, pageno);

  page_status= -1;
  if ((block= hash_link->block) &&
      block->hash_link == hash_link && (block->status & PCBLOCK_READ))
    page_status= PAGE_READ;

  if (wrmode && pagecache->resize_in_flush)
  {
    /* This is a write request during the flush phase of a resize operation */

    if (page_status != PAGE_READ)
    {
      /* We don't need the page in the cache: we are going to write on disk */
      DBUG_ASSERT(hash_link->requests > 0);
      hash_link->requests--;
      unlink_hash(pagecache, hash_link);
      return 0;
    }
    if (!(block->status & PCBLOCK_IN_FLUSH))
    {
      DBUG_ASSERT(hash_link->requests > 0);
      hash_link->requests--;
      /*
        Remove block to invalidate the page in the block buffer
        as we are going to write directly on disk.
        Although we have an exclusive lock for the updated key part
        the control can be yielded by the current thread as we might
        have unfinished readers of other key parts in the block
        buffer. Still we are guaranteed not to have any readers
        of the key part we are writing into until the block is
        removed from the cache as we set the PCBLOCK_REASSIGNED
        flag (see the code below that handles reading requests).
      */
      free_block(pagecache, block);
      return 0;
    }
    /* Wait until the page is flushed on disk */
    DBUG_ASSERT(hash_link->requests > 0);
    hash_link->requests--;
    {
#ifdef THREAD
      struct st_my_thread_var *thread= my_thread_var;
      wqueue_add_to_queue(&block->wqueue[COND_FOR_SAVED], thread);
      do
      {
        KEYCACHE_DBUG_PRINT("find_block: wait",
                            ("suspend thread %ld", thread->id));
        pagecache_pthread_cond_wait(&thread->suspend,
                                   &pagecache->cache_lock);
      }
      while(thread->next);
#else
      KEYCACHE_DBUG_ASSERT(0);
      /*
        Given the use of "resize_in_flush", it seems impossible
        that this whole branch is ever entered in single-threaded case
        because "(wrmode && pagecache->resize_in_flush)" cannot be true.
        TODO: Check this, and then put the whole branch into the
        "#ifdef THREAD" guard.
      */
#endif
    }
    /* Invalidate page in the block if it has not been done yet */
    if (block->status)
      free_block(pagecache, block);
    return 0;
  }

  if (page_status == PAGE_READ &&
      (block->status & (PCBLOCK_IN_SWITCH | PCBLOCK_REASSIGNED)))
  {
    /* This is a request for a page to be removed from cache */

    KEYCACHE_DBUG_PRINT("find_block",
                        ("request for old page in block: %u  "
                         "wrmode: %d  block->status: %d",
                         PCBLOCK_NUMBER(pagecache, block), wrmode,
                         block->status));
    /*
       Only reading requests can proceed until the old dirty page is flushed,
       all others are to be suspended, then resubmitted
    */
    if (!wrmode && !(block->status & PCBLOCK_REASSIGNED))
    {
      if (reg_req)
        reg_requests(pagecache, block, 1);
    }
    else
    {
      DBUG_ASSERT(hash_link->requests > 0);
      hash_link->requests--;
      KEYCACHE_DBUG_PRINT("find_block",
                          ("request waiting for old page to be saved"));
      {
#ifdef THREAD
        struct st_my_thread_var *thread= my_thread_var;
        /* Put the request into the queue of those waiting for the old page */
        wqueue_add_to_queue(&block->wqueue[COND_FOR_SAVED], thread);
        /* Wait until the request can be resubmitted */
        do
        {
          KEYCACHE_DBUG_PRINT("find_block: wait",
                              ("suspend thread %ld", thread->id));
          pagecache_pthread_cond_wait(&thread->suspend,
                                     &pagecache->cache_lock);
        }
        while(thread->next);
#else
        KEYCACHE_DBUG_ASSERT(0);
          /* No parallel requests in single-threaded case */
#endif
      }
      KEYCACHE_DBUG_PRINT("find_block",
                          ("request for old page resubmitted"));
      DBUG_PRINT("info", ("restarting..."));
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
      if (pagecache->blocks_unused)
      {
        if (pagecache->free_block_list)
        {
          /* There is a block in the free list. */
          block= pagecache->free_block_list;
          pagecache->free_block_list= block->next_used;
          block->next_used= NULL;
        }
        else
        {
          /* There are some never used blocks, take first of them */
          block= &pagecache->block_root[pagecache->blocks_used];
          block->buffer= ADD_TO_PTR(pagecache->block_mem,
                                    ((ulong) pagecache->blocks_used*
                                     pagecache->block_size),
                                    uchar*);
          pagecache->blocks_used++;
        }
        pagecache->blocks_unused--;
        DBUG_ASSERT(block->wlocks == 0);
        DBUG_ASSERT(block->rlocks == 0);
        DBUG_ASSERT(block->rlocks_queue == 0);
        DBUG_ASSERT(block->pins == 0);
        block->status= 0;
#ifndef DBUG_OFF
        block->type= PAGECACHE_EMPTY_PAGE;
#endif
        block->requests= 1;
        block->temperature= PCBLOCK_COLD;
        block->hits_left= init_hits_left;
        block->last_hit_time= 0;
        block->rec_lsn= LSN_MAX;
        link_to_file_list(pagecache, block, file, 0);
        block->hash_link= hash_link;
        hash_link->block= block;
        page_status= PAGE_TO_BE_READ;
        DBUG_PRINT("info", ("page to be read set for page 0x%lx",
                            (ulong)block));
        KEYCACHE_DBUG_PRINT("find_block",
                            ("got free or never used block %u",
                             PCBLOCK_NUMBER(pagecache, block)));
      }
      else
      {
	/* There are no never used blocks, use a block from the LRU chain */

        /*
          Wait until a new block is added to the LRU chain;
          several threads might wait here for the same page,
          all of them must get the same block
        */

#ifdef THREAD
        if (! pagecache->used_last)
        {
          struct st_my_thread_var *thread= my_thread_var;
          thread->opt_info= (void *) hash_link;
          wqueue_link_into_queue(&pagecache->waiting_for_block, thread);
          do
          {
            KEYCACHE_DBUG_PRINT("find_block: wait",
                                ("suspend thread %ld", thread->id));
            pagecache_pthread_cond_wait(&thread->suspend,
                                       &pagecache->cache_lock);
          }
          while (thread->next);
          thread->opt_info= NULL;
        }
#else
        KEYCACHE_DBUG_ASSERT(pagecache->used_last);
#endif
        block= hash_link->block;
        if (! block)
        {
          /*
             Take the first block from the LRU chain
             unlinking it from the chain
          */
          block= pagecache->used_last->next_used;
          block->hits_left= init_hits_left;
          block->last_hit_time= 0;
	  if (reg_req)
            reg_requests(pagecache, block, 1);
          hash_link->block= block;
        }
        PCBLOCK_INFO(block);
        DBUG_ASSERT(block->wlocks == 0);
        DBUG_ASSERT(block->rlocks == 0);
        DBUG_ASSERT(block->rlocks_queue == 0);
        DBUG_ASSERT(block->pins == 0);

        if (block->hash_link != hash_link &&
	    ! (block->status & PCBLOCK_IN_SWITCH) )
        {
	  /* this is a primary request for a new page */
          DBUG_ASSERT(block->wlocks == 0);
          DBUG_ASSERT(block->rlocks == 0);
          DBUG_ASSERT(block->rlocks_queue == 0);
          DBUG_ASSERT(block->pins == 0);
          block->status|= PCBLOCK_IN_SWITCH;

          KEYCACHE_DBUG_PRINT("find_block",
                              ("got block %u for new page",
                               PCBLOCK_NUMBER(pagecache, block)));

          if (block->status & PCBLOCK_CHANGED)
          {
	    /* The block contains a dirty page - push it out of the cache */

            KEYCACHE_DBUG_PRINT("find_block", ("block is dirty"));

            /*
	      The call is thread safe because only the current
	      thread might change the block->hash_link value
            */
            DBUG_ASSERT(block->pins == 0);
            pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
            error= pagecache_fwrite(pagecache,
                                    &block->hash_link->file,
                                    block->buffer,
                                    block->hash_link->pageno,
                                    block->type,
                                    pagecache->readwrite_flags);
            pagecache_pthread_mutex_lock(&pagecache->cache_lock);
	    pagecache->global_cache_write++;
          }

          block->status|= PCBLOCK_REASSIGNED;
          if (block->hash_link)
          {
            /*
	      Wait until all pending read requests
	      for this page are executed
	      (we could have avoided this waiting, if we had read
	      a page in the cache in a sweep, without yielding control)
            */
            wait_for_readers(pagecache, block);

            /* Remove the hash link for this page from the hash table */
            unlink_hash(pagecache, block->hash_link);
            /* All pending requests for this page must be resubmitted */
#ifdef THREAD
            if (block->wqueue[COND_FOR_SAVED].last_thread)
              wqueue_release_queue(&block->wqueue[COND_FOR_SAVED]);
#endif
          }
          link_to_file_list(pagecache, block, file,
                            (my_bool)(block->hash_link ? 1 : 0));
          PCBLOCK_INFO(block);
          block->status= error ? PCBLOCK_ERROR : 0;
          block->error=  (int16) my_errno;
#ifndef DBUG_OFF
          block->type= PAGECACHE_EMPTY_PAGE;
          if (error)
            my_debug_put_break_here();
#endif
          block->hash_link= hash_link;
          page_status= PAGE_TO_BE_READ;
          DBUG_PRINT("info", ("page to be read set for page 0x%lx",
                              (ulong)block));

          KEYCACHE_DBUG_ASSERT(block->hash_link->block == block);
          KEYCACHE_DBUG_ASSERT(hash_link->block->hash_link == hash_link);
        }
        else
        {
          /* This is for secondary requests for a new page only */
          KEYCACHE_DBUG_PRINT("find_block",
                              ("block->hash_link: %p  hash_link: %p  "
                               "block->status: %u", block->hash_link,
                               hash_link, block->status ));
          page_status= (((block->hash_link == hash_link) &&
                         (block->status & PCBLOCK_READ)) ?
                        PAGE_READ : PAGE_WAIT_TO_BE_READ);
        }
      }
    }
    else
    {
      if (reg_req)
	reg_requests(pagecache, block, 1);
      KEYCACHE_DBUG_PRINT("find_block",
                          ("block->hash_link: %p  hash_link: %p  "
                           "block->status: %u", block->hash_link,
                           hash_link, block->status ));
      page_status= (((block->hash_link == hash_link) &&
                     (block->status & PCBLOCK_READ)) ?
                    PAGE_READ : PAGE_WAIT_TO_BE_READ);
    }
  }

  KEYCACHE_DBUG_ASSERT(page_status != -1);
  *page_st= page_status;
  DBUG_PRINT("info",
             ("block: 0x%lx  fd: %u  pos: %lu  block->status: %u  page_status: %u",
              (ulong) block, (uint) file->file,
              (ulong) pageno, block->status, (uint) page_status));
  KEYCACHE_DBUG_PRINT("find_block",
                      ("block: 0x%lx  fd: %d  pos: %lu  block->status: %u  page_status: %d",
                       (ulong) block,
                       file->file, (ulong) pageno, block->status,
                       page_status));

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_pagecache",
               test_key_cache(pagecache, "end of find_block",0););
#endif
  KEYCACHE_THREAD_TRACE("find_block:end");
  DBUG_RETURN(block);
}


static void add_pin(PAGECACHE_BLOCK_LINK *block)
{
  DBUG_ENTER("add_pin");
  DBUG_PRINT("enter", ("block: 0x%lx  pins: %u",
                       (ulong) block,
                       block->pins));
  PCBLOCK_INFO(block);
  block->pins++;
#ifndef DBUG_OFF
  {
    PAGECACHE_PIN_INFO *info=
      (PAGECACHE_PIN_INFO *)my_malloc(sizeof(PAGECACHE_PIN_INFO), MYF(0));
    info->thread= my_thread_var;
    info_link(&block->pin_list, info);
  }
#endif
  DBUG_VOID_RETURN;
}

static void remove_pin(PAGECACHE_BLOCK_LINK *block, my_bool any
#ifdef DBUG_OFF
                       __attribute__((unused))
#endif
                       )
{
  DBUG_ENTER("remove_pin");
  DBUG_PRINT("enter", ("block: 0x%lx  pins: %u  any: %d",
                       (ulong) block,
                       block->pins, (int)any));
  PCBLOCK_INFO(block);
  DBUG_ASSERT(block->pins > 0);
  block->pins--;
#ifndef DBUG_OFF
  {
    PAGECACHE_PIN_INFO *info= info_find(block->pin_list, my_thread_var, any);
    DBUG_ASSERT(info != 0);
    info_unlink(info);
    my_free(info, MYF(0));
  }
#endif
  DBUG_VOID_RETURN;
}
#ifndef DBUG_OFF
static void info_add_lock(PAGECACHE_BLOCK_LINK *block, my_bool wl)
{
  PAGECACHE_LOCK_INFO *info=
    (PAGECACHE_LOCK_INFO *)my_malloc(sizeof(PAGECACHE_LOCK_INFO), MYF(0));
  info->thread= my_thread_var;
  info->write_lock= wl;
  info_link((PAGECACHE_PIN_INFO **)&block->lock_list,
	    (PAGECACHE_PIN_INFO *)info);
}
static void info_remove_lock(PAGECACHE_BLOCK_LINK *block)
{
  PAGECACHE_LOCK_INFO *info=
    (PAGECACHE_LOCK_INFO *)info_find((PAGECACHE_PIN_INFO *)block->lock_list,
                                     my_thread_var, FALSE);
  DBUG_ASSERT(info != 0);
  info_unlink((PAGECACHE_PIN_INFO *)info);
  my_free(info, MYF(0));
}
static void info_change_lock(PAGECACHE_BLOCK_LINK *block, my_bool wl)
{
  PAGECACHE_LOCK_INFO *info=
    (PAGECACHE_LOCK_INFO *)info_find((PAGECACHE_PIN_INFO *)block->lock_list,
                                     my_thread_var, FALSE);
  DBUG_ASSERT(info != 0);
  DBUG_ASSERT(info->write_lock != wl);
  info->write_lock= wl;
}
#else
#define info_add_lock(B,W)
#define info_remove_lock(B)
#define info_change_lock(B,W)
#endif


/**
  @brief waiting for lock for read and write lock

  @parem pagecache       pointer to a page cache data structure
  @parem block           the block to work with
  @param file            file of the block when it was locked
  @param pageno          page number of the block when it was locked
  @param lock_type       MY_PTHREAD_LOCK_READ or MY_PTHREAD_LOCK_WRITE

  @retval 0 OK
  @retval 1 Can't lock this block, need retry
*/

static my_bool pagecache_wait_lock(PAGECACHE *pagecache,
                                  PAGECACHE_BLOCK_LINK *block,
                                  PAGECACHE_FILE file,
                                  pgcache_page_no_t pageno,
                                  uint lock_type)
{
  /* Lock failed we will wait */
#ifdef THREAD
  struct st_my_thread_var *thread= my_thread_var;
  DBUG_ENTER("pagecache_wait_lock");
  DBUG_PRINT("info", ("fail to lock, waiting... 0x%lx", (ulong)block));
  thread->lock_type= lock_type;
  wqueue_add_to_queue(&block->wqueue[COND_FOR_WRLOCK], thread);
  dec_counter_for_resize_op(pagecache);
  do
  {
    KEYCACHE_DBUG_PRINT("get_wrlock: wait",
                        ("suspend thread %ld", thread->id));
    pagecache_pthread_cond_wait(&thread->suspend,
                                &pagecache->cache_lock);
  }
  while(thread->next);
  inc_counter_for_resize_op(pagecache);
#else
  DBUG_ASSERT(0);
#endif
  PCBLOCK_INFO(block);
  if ((block->status & (PCBLOCK_REASSIGNED | PCBLOCK_IN_SWITCH)) ||
      !block->hash_link ||
      file.file != block->hash_link->file.file ||
      pageno != block->hash_link->pageno)
  {
    DBUG_PRINT("info", ("the block 0x%lx changed => need retry "
                        "status: %x  files %d != %d or pages %lu != %lu",
                        (ulong)block, block->status,
                        file.file,
                        block->hash_link ? block->hash_link->file.file : -1,
                        (ulong) pageno,
                        (ulong) (block->hash_link ? block->hash_link->pageno : 0)));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

/**
  @brief Put on the block write lock

  @parem pagecache       pointer to a page cache data structure
  @parem block           the block to work with

  @note We have loose scheme for locking by the same thread:
    * Downgrade to read lock if no other locks are taken
    * Our scheme of locking allow for the same thread
      - the same kind of lock
      - taking read lock if write lock present
      - downgrading to read lock if still other place the same
        thread keep write lock
    * But unlock operation number should be the same to lock operation.
    * If we try to get read lock having active write locks we put read
      locks to queue, and as soon as write lock(s) gone the read locks
      from queue came in force.
    * If read lock is unlocked earlier then it came to force it
      just removed from the queue

  @retval 0 OK
  @retval 1 Can't lock this block, need retry
*/

static my_bool get_wrlock(PAGECACHE *pagecache,
                          PAGECACHE_BLOCK_LINK *block)
{
  PAGECACHE_FILE file= block->hash_link->file;
  pgcache_page_no_t pageno= block->hash_link->pageno;
  pthread_t locker= pthread_self();
  DBUG_ENTER("get_wrlock");
  DBUG_PRINT("info", ("the block 0x%lx "
                      "files %d(%d)  pages %lu(%lu)",
                      (ulong) block,
                      file.file, block->hash_link->file.file,
                      (ulong) pageno, (ulong) block->hash_link->pageno));
  PCBLOCK_INFO(block);
  /*
    We assume that the same thread will try write lock on block on which it
    has already read lock.
  */
  while ((block->wlocks && !pthread_equal(block->write_locker, locker)) ||
         block->rlocks)
  {
    /* Lock failed we will wait */
    if (pagecache_wait_lock(pagecache, block, file, pageno,
                           MY_PTHREAD_LOCK_WRITE))
      DBUG_RETURN(1);
  }
  /* we are doing it by global cache mutex protection, so it is OK */
  block->wlocks++;
  block->write_locker= locker;
  DBUG_PRINT("info", ("WR lock set, block 0x%lx", (ulong)block));
  DBUG_RETURN(0);
}


/*
  @brief Put on the block read lock

  @param pagecache       pointer to a page cache data structure
  @param block           the block to work with
  @param user_file	 Unique handler per handler file. Used to check if
			 we request many write locks withing the same
                         statement

  @note see note for get_wrlock().

  @retvalue 0 OK
  @retvalue 1 Can't lock this block, need retry
*/

static my_bool get_rdlock(PAGECACHE *pagecache,
                          PAGECACHE_BLOCK_LINK *block)
{
  PAGECACHE_FILE file= block->hash_link->file;
  pgcache_page_no_t pageno= block->hash_link->pageno;
  pthread_t locker= pthread_self();
  DBUG_ENTER("get_rdlock");
  DBUG_PRINT("info", ("the block 0x%lx "
                      "files %d(%d)  pages %lu(%lu)",
                      (ulong) block,
                      file.file, block->hash_link->file.file,
                      (ulong) pageno, (ulong) block->hash_link->pageno));
  PCBLOCK_INFO(block);
  while (block->wlocks && !pthread_equal(block->write_locker, locker))
  {
    /* Lock failed we will wait */
    if (pagecache_wait_lock(pagecache, block, file, pageno,
                           MY_PTHREAD_LOCK_READ))
      DBUG_RETURN(1);
  }
  /* we are doing it by global cache mutex protection, so it is OK */
  if (block->wlocks)
  {
    DBUG_ASSERT(pthread_equal(block->write_locker, locker));
    block->rlocks_queue++;
    DBUG_PRINT("info", ("RD lock put into queue, block 0x%lx", (ulong)block));
  }
  else
  {
    block->rlocks++;
    DBUG_PRINT("info", ("RD lock set, block 0x%lx", (ulong)block));
  }
  DBUG_RETURN(0);
}


/*
  @brief Remove write lock from the block

  @param pagecache       pointer to a page cache data structure
  @param block           the block to work with
  @param read_lock       downgrade to read lock

  @note see note for get_wrlock().
*/

static void release_wrlock(PAGECACHE_BLOCK_LINK *block, my_bool read_lock)
{
  DBUG_ENTER("release_wrlock");
  PCBLOCK_INFO(block);
  DBUG_ASSERT(block->wlocks > 0);
  DBUG_ASSERT(block->rlocks == 0);
  DBUG_ASSERT(block->pins > 0);
  if (read_lock)
    block->rlocks_queue++;
  if (block->wlocks == 1)
  {
    block->rlocks= block->rlocks_queue;
    block->rlocks_queue= 0;
  }
  block->wlocks--;
  if (block->wlocks > 0)
    DBUG_VOID_RETURN;                      /* Multiple write locked */
  DBUG_PRINT("info", ("WR lock reset, block 0x%lx", (ulong)block));
#ifdef THREAD
  /* release all threads waiting for read lock or one waiting for write */
  if (block->wqueue[COND_FOR_WRLOCK].last_thread)
    wqueue_release_one_locktype_from_queue(&block->wqueue[COND_FOR_WRLOCK]);
#endif
  PCBLOCK_INFO(block);
  DBUG_VOID_RETURN;
}

/*
  @brief Remove read lock from the block

  @param pagecache       pointer to a page cache data structure
  @param block           the block to work with

  @note see note for get_wrlock().
*/

static void release_rdlock(PAGECACHE_BLOCK_LINK *block)
{
  DBUG_ENTER("release_wrlock");
  PCBLOCK_INFO(block);
  if (block->wlocks)
  {
    DBUG_ASSERT(pthread_equal(block->write_locker, pthread_self()));
    DBUG_ASSERT(block->rlocks == 0);
    DBUG_ASSERT(block->rlocks_queue > 0);
    block->rlocks_queue--;
    DBUG_PRINT("info", ("RD lock queue decreased, block 0x%lx", (ulong)block));
    DBUG_VOID_RETURN;
  }
  DBUG_ASSERT(block->rlocks > 0);
  DBUG_ASSERT(block->rlocks_queue == 0);
  block->rlocks--;
  DBUG_PRINT("info", ("RD lock decreased, block 0x%lx", (ulong)block));
  if (block->rlocks > 0)
    DBUG_VOID_RETURN;                      /* Multiple write locked */
  DBUG_PRINT("info", ("RD lock reset, block 0x%lx", (ulong)block));
#ifdef THREAD
  /* release all threads waiting for read lock or one waiting for write */
  if (block->wqueue[COND_FOR_WRLOCK].last_thread)
    wqueue_release_one_locktype_from_queue(&block->wqueue[COND_FOR_WRLOCK]);
#endif
  PCBLOCK_INFO(block);
  DBUG_VOID_RETURN;
}

/**
  @brief Try to lock/unlock and pin/unpin the block

  @param pagecache       pointer to a page cache data structure
  @param block           the block to work with
  @param lock            lock change mode
  @param pin             pinchange mode
  @param file            File handler requesting pin
  @param any             allow unpinning block pinned by any thread; possible
                         only if not locked, see pagecache_unlock_by_link()

  @retval 0 OK
  @retval 1 Try to lock the block failed
*/

static my_bool make_lock_and_pin(PAGECACHE *pagecache,
                                 PAGECACHE_BLOCK_LINK *block,
                                 enum pagecache_page_lock lock,
                                 enum pagecache_page_pin pin,
                                 my_bool any)
{
  DBUG_ENTER("make_lock_and_pin");

  DBUG_PRINT("enter", ("block: 0x%lx", (ulong)block));
#ifndef DBUG_OFF
  if (block)
  {
    DBUG_PRINT("enter", ("block: 0x%lx (%u)  wrlocks: %u  rdlocks: %u  "
                         "rdlocks_q: %u  pins: %u  lock: %s  pin: %s any %d",
                         (ulong)block, PCBLOCK_NUMBER(pagecache, block),
                         block->wlocks, block->rlocks, block->rlocks_queue,
                         block->pins,
                         page_cache_page_lock_str[lock],
                         page_cache_page_pin_str[pin], (int)any));
    PCBLOCK_INFO(block);
  }
#endif

  DBUG_ASSERT(!any ||
              ((lock == PAGECACHE_LOCK_LEFT_UNLOCKED) &&
               (pin == PAGECACHE_UNPIN)));

  switch (lock) {
  case PAGECACHE_LOCK_WRITE:               /* free  -> write */
    /* Writelock and pin the buffer */
    if (get_wrlock(pagecache, block))
    {
      /* Couldn't lock because block changed status => need retry */
      goto retry;
    }

    /* The cache is locked so nothing afraid of */
    add_pin(block);
    info_add_lock(block, 1);
    break;
  case PAGECACHE_LOCK_WRITE_TO_READ:       /* write -> read  */
  case PAGECACHE_LOCK_WRITE_UNLOCK:        /* write -> free  */
    /* Removes write lock and puts read lock */
    release_wrlock(block, lock == PAGECACHE_LOCK_WRITE_TO_READ);
    /* fall through */
  case PAGECACHE_LOCK_READ_UNLOCK:         /* read  -> free  */
    if (lock == PAGECACHE_LOCK_READ_UNLOCK)
      release_rdlock(block);
    /* fall through */
  case PAGECACHE_LOCK_LEFT_READLOCKED:     /* read  -> read  */
    if (pin == PAGECACHE_UNPIN)
    {
      remove_pin(block, FALSE);
    }
    if (lock == PAGECACHE_LOCK_WRITE_TO_READ)
    {
      info_change_lock(block, 0);
    }
    else if (lock == PAGECACHE_LOCK_WRITE_UNLOCK ||
             lock == PAGECACHE_LOCK_READ_UNLOCK)
    {
      info_remove_lock(block);
    }
    break;
  case PAGECACHE_LOCK_READ:                /* free  -> read  */
    if (get_rdlock(pagecache, block))
    {
      /* Couldn't lock because block changed status => need retry */
      goto retry;
    }

    if (pin == PAGECACHE_PIN)
    {
      /* The cache is locked so nothing afraid off */
      add_pin(block);
    }
    info_add_lock(block, 0);
    break;
  case PAGECACHE_LOCK_LEFT_UNLOCKED:       /* free  -> free  */
    if (pin == PAGECACHE_UNPIN)
    {
      remove_pin(block, any);
    }
    /* fall through */
  case PAGECACHE_LOCK_LEFT_WRITELOCKED:    /* write -> write */
    break; /* do nothing */
  default:
    DBUG_ASSERT(0); /* Never should happened */
  }

#ifndef DBUG_OFF
  if (block)
    PCBLOCK_INFO(block);
#endif
  DBUG_RETURN(0);
retry:
  DBUG_PRINT("INFO", ("Retry block 0x%lx", (ulong)block));
  PCBLOCK_INFO(block);
  DBUG_ASSERT(block->hash_link->requests > 0);
  block->hash_link->requests--;
  PCBLOCK_INFO(block);
  DBUG_RETURN(1);

}


/*
  Read into a key cache block buffer from disk.

  SYNOPSIS

    read_block()
      pagecache           pointer to a page cache data structure
      block               block to which buffer the data is to be read
      primary             <-> the current thread will read the data

  RETURN VALUE
    None

  NOTES.
    The function either reads a page data from file to the block buffer,
    or waits until another thread reads it. What page to read is determined
    by a block parameter - reference to a hash link for this page.
    If an error occurs THE PCBLOCK_ERROR bit is set in the block status.

    On entry cache_lock is locked
*/

static void read_block(PAGECACHE *pagecache,
                       PAGECACHE_BLOCK_LINK *block,
                       my_bool primary)
{

  DBUG_ENTER("read_block");
  DBUG_PRINT("enter", ("read block: 0x%lx  primary: %d",
                       (ulong)block, primary));
  if (primary)
  {
    size_t error;
    /*
      This code is executed only by threads
      that submitted primary requests
    */

    pagecache->global_cache_read++;
    /*
      Page is not in buffer yet, is to be read from disk
      Here other threads may step in and register as secondary readers.
      They will register in block->wqueue[COND_FOR_REQUESTED].
    */
    pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
    error= pagecache_fread(pagecache, &block->hash_link->file,
                           block->buffer,
                           block->hash_link->pageno,
                           pagecache->readwrite_flags);
    pagecache_pthread_mutex_lock(&pagecache->cache_lock);
    if (error)
    {
      block->status|= PCBLOCK_ERROR;
      block->error=   (int16) my_errno;
      my_debug_put_break_here();
    }
    else
    {
      block->status|= PCBLOCK_READ;
      if ((*block->hash_link->file.read_callback)(block->buffer,
                                                  block->hash_link->pageno,
                                                  block->hash_link->
                                                  file.callback_data))
      {
        DBUG_PRINT("error", ("read callback problem"));
        block->status|= PCBLOCK_ERROR;
        block->error=  (int16) my_errno;
        my_debug_put_break_here();
      }
    }
    DBUG_PRINT("read_block",
               ("primary request: new page in cache"));
    /* Signal that all pending requests for this page now can be processed */
#ifdef THREAD
    if (block->wqueue[COND_FOR_REQUESTED].last_thread)
      wqueue_release_queue(&block->wqueue[COND_FOR_REQUESTED]);
#endif
  }
  else
  {
    /*
      This code is executed only by threads
      that submitted secondary requests
    */

#ifdef THREAD
      struct st_my_thread_var *thread= my_thread_var;
      /* Put the request into a queue and wait until it can be processed */
      wqueue_add_to_queue(&block->wqueue[COND_FOR_REQUESTED], thread);
      do
      {
        DBUG_PRINT("read_block: wait",
                  ("suspend thread %ld", thread->id));
        pagecache_pthread_cond_wait(&thread->suspend,
                                   &pagecache->cache_lock);
      }
      while (thread->next);
#else
      KEYCACHE_DBUG_ASSERT(0);
      /* No parallel requests in single-threaded case */
#endif
    DBUG_PRINT("read_block",
               ("secondary request: new page in cache"));
  }
  DBUG_VOID_RETURN;
}


/**
   @brief Set LSN on the page to the given one if the given LSN is bigger

   @param  pagecache        pointer to a page cache data structure
   @param  lsn              LSN to set
   @param  block            block to check and set
*/

static void check_and_set_lsn(PAGECACHE *pagecache,
                              LSN lsn, PAGECACHE_BLOCK_LINK *block)
{
  LSN old;
  DBUG_ENTER("check_and_set_lsn");
  /*
    In recovery, we can _ma_unpin_all_pages() to put a LSN on page, though
    page would be PAGECACHE_PLAIN_PAGE (transactionality temporarily disabled
    to not log REDOs).
  */
  DBUG_ASSERT((block->type == PAGECACHE_LSN_PAGE) || maria_in_recovery);
  old= lsn_korr(block->buffer);
  DBUG_PRINT("info", ("old lsn: (%lu, 0x%lx)  new lsn: (%lu, 0x%lx)",
                      LSN_IN_PARTS(old), LSN_IN_PARTS(lsn)));
  if (cmp_translog_addr(lsn, old) > 0)
  {

    DBUG_ASSERT(block->type != PAGECACHE_READ_UNKNOWN_PAGE);
    lsn_store(block->buffer, lsn);
    /* we stored LSN in page so we dirtied it */
    if (!(block->status & PCBLOCK_CHANGED))
      link_to_changed_list(pagecache, block);
  }
  DBUG_VOID_RETURN;
}


/**
  @brief Unlock/unpin page and put LSN stamp if it need

  @param pagecache      pointer to a page cache data structure
  @pagam file           handler for the file for the block of data to be read
  @param pageno         number of the block of data in the file
  @param lock           lock change
  @param pin            pin page
  @param first_REDO_LSN_for_page do not set it if it is zero
  @param lsn            if it is not LSN_IMPOSSIBLE (0) and it
                        is bigger then LSN on the page it will be written on
                        the page
  @param was_changed    should be true if the page was write locked with
                        direct link giving and the page was changed

  @note
    Pininig uses requests registration mechanism it works following way:
                                | beginnig    | ending        |
                                | of func.    | of func.      |
    ----------------------------+-------------+---------------+
    PAGECACHE_PIN_LEFT_PINNED   |      -      |       -       |
    PAGECACHE_PIN_LEFT_UNPINNED | reg request | unreg request |
    PAGECACHE_PIN               | reg request |       -       |
    PAGECACHE_UNPIN             |      -      | unreg request |


*/

void pagecache_unlock(PAGECACHE *pagecache,
                      PAGECACHE_FILE *file,
                      pgcache_page_no_t pageno,
                      enum pagecache_page_lock lock,
                      enum pagecache_page_pin pin,
                      LSN first_REDO_LSN_for_page,
                      LSN lsn, my_bool was_changed)
{
  PAGECACHE_BLOCK_LINK *block;
  int page_st;
  DBUG_ENTER("pagecache_unlock");
  DBUG_PRINT("enter", ("fd: %u  page: %lu  %s  %s",
                       (uint) file->file, (ulong) pageno,
                       page_cache_page_lock_str[lock],
                       page_cache_page_pin_str[pin]));
  /* we do not allow any lock/pin increasing here */
  DBUG_ASSERT(pin != PAGECACHE_PIN);
  DBUG_ASSERT(lock != PAGECACHE_LOCK_READ);
  DBUG_ASSERT(lock != PAGECACHE_LOCK_WRITE);

  pagecache_pthread_mutex_lock(&pagecache->cache_lock);
  /*
    As soon as we keep lock cache can be used, and we have lock because want
    to unlock.
  */
  DBUG_ASSERT(pagecache->can_be_used);

  inc_counter_for_resize_op(pagecache);
  /* See NOTE for pagecache_unlock about registering requests */
  block= find_block(pagecache, file, pageno, 0, 0,
                    pin == PAGECACHE_PIN_LEFT_UNPINNED, &page_st);
  PCBLOCK_INFO(block);
  DBUG_ASSERT(block != 0 && page_st == PAGE_READ);
  if (first_REDO_LSN_for_page)
  {
    DBUG_ASSERT(lock == PAGECACHE_LOCK_WRITE_UNLOCK);
    DBUG_ASSERT(pin == PAGECACHE_UNPIN);
    pagecache_set_block_rec_lsn(block, first_REDO_LSN_for_page);
  }
  if (lsn != LSN_IMPOSSIBLE)
    check_and_set_lsn(pagecache, lsn, block);

  /* if we lock for write we must link the block to changed blocks */
  DBUG_ASSERT((block->status & PCBLOCK_DIRECT_W) == 0 ||
              (lock == PAGECACHE_LOCK_WRITE_UNLOCK ||
               lock == PAGECACHE_LOCK_WRITE_TO_READ ||
               lock == PAGECACHE_LOCK_LEFT_WRITELOCKED));
  /*
    if was_changed then status should be PCBLOCK_DIRECT_W or marked
    as dirty
  */
  DBUG_ASSERT(!was_changed || (block->status & PCBLOCK_DIRECT_W) ||
              (block->status & PCBLOCK_CHANGED));
  if ((block->status & PCBLOCK_DIRECT_W) &&
      (lock == PAGECACHE_LOCK_WRITE_UNLOCK ||
       lock == PAGECACHE_LOCK_WRITE_TO_READ))
  {
    if (!(block->status & PCBLOCK_CHANGED) && was_changed)
      link_to_changed_list(pagecache, block);
    block->status&= ~PCBLOCK_DIRECT_W;
    DBUG_PRINT("info", ("Drop PCBLOCK_DIRECT_W for block: 0x%lx",
                        (ulong) block));
  }

  if (make_lock_and_pin(pagecache, block, lock, pin, FALSE))
  {
    DBUG_ASSERT(0); /* should not happend */
  }

  remove_reader(block);
  /*
    Link the block into the LRU chain if it's the last submitted request
    for the block and block will not be pinned.
    See NOTE for pagecache_unlock about registering requests.
  */
  if (pin != PAGECACHE_PIN_LEFT_PINNED)
    unreg_request(pagecache, block, 1);

  dec_counter_for_resize_op(pagecache);

  pagecache_pthread_mutex_unlock(&pagecache->cache_lock);

  DBUG_VOID_RETURN;
}


/*
  Unpin page

  SYNOPSIS
    pagecache_unpin()
    pagecache           pointer to a page cache data structure
    file                handler for the file for the block of data to be read
    pageno              number of the block of data in the file
    lsn                 if it is not LSN_IMPOSSIBLE (0) and it
                        is bigger then LSN on the page it will be written on
                        the page
*/

void pagecache_unpin(PAGECACHE *pagecache,
                     PAGECACHE_FILE *file,
                     pgcache_page_no_t pageno,
                     LSN lsn)
{
  PAGECACHE_BLOCK_LINK *block;
  int page_st;
  DBUG_ENTER("pagecache_unpin");
  DBUG_PRINT("enter", ("fd: %u  page: %lu",
                       (uint) file->file, (ulong) pageno));
  pagecache_pthread_mutex_lock(&pagecache->cache_lock);
  /*
    As soon as we keep lock cache can be used, and we have lock bacause want
    aunlock.
  */
  DBUG_ASSERT(pagecache->can_be_used);

  inc_counter_for_resize_op(pagecache);
  /* See NOTE for pagecache_unlock about registering requests */
  block= find_block(pagecache, file, pageno, 0, 0, 0, &page_st);
  DBUG_ASSERT(block != 0);
  DBUG_ASSERT(page_st == PAGE_READ);
  /* we can't unpin such page without unlock */
  DBUG_ASSERT((block->status & PCBLOCK_DIRECT_W) == 0);

  if (lsn != LSN_IMPOSSIBLE)
    check_and_set_lsn(pagecache, lsn, block);

  /*
    we can just unpin only with keeping read lock because:
    a) we can't pin without any lock
    b) we can't unpin keeping write lock
  */
  if (make_lock_and_pin(pagecache, block,
                        PAGECACHE_LOCK_LEFT_READLOCKED,
                        PAGECACHE_UNPIN, FALSE))
    DBUG_ASSERT(0);                           /* should not happend */

  remove_reader(block);
  /*
    Link the block into the LRU chain if it's the last submitted request
    for the block and block will not be pinned.
    See NOTE for pagecache_unlock about registering requests
  */
  unreg_request(pagecache, block, 1);

  dec_counter_for_resize_op(pagecache);

  pagecache_pthread_mutex_unlock(&pagecache->cache_lock);

  DBUG_VOID_RETURN;
}


/**
  @brief Unlock/unpin page and put LSN stamp if it need
  (uses direct block/page pointer)

  @param pagecache       pointer to a page cache data structure
  @param link            direct link to page (returned by read or write)
  @param lock            lock change
  @param pin             pin page
  @param first_REDO_LSN_for_page do not set it if it is LSN_IMPOSSIBLE (0)
  @param lsn             if it is not LSN_IMPOSSIBLE and it is bigger then
                         LSN on the page it will be written on the page
  @param was_changed     should be true if the page was write locked with
                         direct link giving and the page was changed
  @param any             allow unpinning block pinned by any thread; possible
                         only if not locked

  @note 'any' is a hack so that _ma_bitmap_unpin_all() is allowed to unpin
  non-locked bitmap pages pinned by other threads. Because it always uses
  PAGECACHE_LOCK_LEFT_UNLOCKED and PAGECACHE_UNPIN
  (see write_changed_bitmap()), the hack is limited to these conditions.
*/

void pagecache_unlock_by_link(PAGECACHE *pagecache,
                              PAGECACHE_BLOCK_LINK *block,
                              enum pagecache_page_lock lock,
                              enum pagecache_page_pin pin,
                              LSN first_REDO_LSN_for_page,
                              LSN lsn, my_bool was_changed,
                              my_bool any)
{
  DBUG_ENTER("pagecache_unlock_by_link");
  DBUG_PRINT("enter", ("block: 0x%lx  fd: %u  page: %lu  changed: %d  %s  %s",
                       (ulong) block,
                       (uint) block->hash_link->file.file,
                       (ulong) block->hash_link->pageno, was_changed,
                       page_cache_page_lock_str[lock],
                       page_cache_page_pin_str[pin]));
  /*
    We do not allow any lock/pin increasing here and page can't be
    unpinned because we use direct link.
  */
  DBUG_ASSERT(pin != PAGECACHE_PIN);
  DBUG_ASSERT(pin != PAGECACHE_PIN_LEFT_UNPINNED);
  DBUG_ASSERT(lock != PAGECACHE_LOCK_READ);
  DBUG_ASSERT(lock != PAGECACHE_LOCK_WRITE);
  pagecache_pthread_mutex_lock(&pagecache->cache_lock);
  if (pin == PAGECACHE_PIN_LEFT_UNPINNED &&
      lock == PAGECACHE_LOCK_READ_UNLOCK)
  {
    if (make_lock_and_pin(pagecache, block, lock, pin, FALSE))
      DBUG_ASSERT(0);                         /* should not happend */
    pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
    DBUG_VOID_RETURN;
  }

  /*
    As soon as we keep lock cache can be used, and we have lock because want
    unlock.
  */
  DBUG_ASSERT(pagecache->can_be_used);

  inc_counter_for_resize_op(pagecache);
  if (was_changed)
  {
    if (first_REDO_LSN_for_page != LSN_IMPOSSIBLE)
    {
      /*
        LOCK_READ_UNLOCK is ok here as the page may have first locked
        with WRITE lock that was temporarly converted to READ lock before
        it's unpinned
      */
      DBUG_ASSERT(lock == PAGECACHE_LOCK_WRITE_UNLOCK ||
                  lock == PAGECACHE_LOCK_READ_UNLOCK);
      DBUG_ASSERT(pin == PAGECACHE_UNPIN);
      pagecache_set_block_rec_lsn(block, first_REDO_LSN_for_page);
    }
    if (lsn != LSN_IMPOSSIBLE)
      check_and_set_lsn(pagecache, lsn, block);
    /*
      Reset error flag. Mark also that page is active; This may not have
      been the case if there was an error reading the page
    */
    block->status= (block->status & ~PCBLOCK_ERROR) | PCBLOCK_READ;
  }

  /* if we lock for write we must link the block to changed blocks */
  DBUG_ASSERT((block->status & PCBLOCK_DIRECT_W) == 0 ||
              (lock == PAGECACHE_LOCK_WRITE_UNLOCK ||
               lock == PAGECACHE_LOCK_WRITE_TO_READ ||
               lock == PAGECACHE_LOCK_LEFT_WRITELOCKED));
  /*
    If was_changed then status should be PCBLOCK_DIRECT_W or marked
    as dirty
  */
  DBUG_ASSERT(!was_changed || (block->status & PCBLOCK_DIRECT_W) ||
              (block->status & PCBLOCK_CHANGED));
  if ((block->status & PCBLOCK_DIRECT_W) &&
      (lock == PAGECACHE_LOCK_WRITE_UNLOCK ||
       lock == PAGECACHE_LOCK_WRITE_TO_READ))
  {
    if (!(block->status & PCBLOCK_CHANGED) && was_changed)
      link_to_changed_list(pagecache, block);
    block->status&= ~PCBLOCK_DIRECT_W;
    DBUG_PRINT("info", ("Drop PCBLOCK_DIRECT_W for block: 0x%lx",
                        (ulong) block));
  }

  if (make_lock_and_pin(pagecache, block, lock, pin, any))
    DBUG_ASSERT(0);                           /* should not happend */

  /*
    Link the block into the LRU chain if it's the last submitted request
    for the block and block will not be pinned.
    See NOTE for pagecache_unlock about registering requests.
  */
  if (pin != PAGECACHE_PIN_LEFT_PINNED)
    unreg_request(pagecache, block, 1);

  dec_counter_for_resize_op(pagecache);

  pagecache_pthread_mutex_unlock(&pagecache->cache_lock);

  DBUG_VOID_RETURN;
}


/*
  Unpin page
  (uses direct block/page pointer)

  SYNOPSIS
    pagecache_unpin_by_link()
    pagecache           pointer to a page cache data structure
    link                direct link to page (returned by read or write)
    lsn                 if it is not LSN_IMPOSSIBLE (0) and it
                        is bigger then LSN on the page it will be written on
                        the page
*/

void pagecache_unpin_by_link(PAGECACHE *pagecache,
                             PAGECACHE_BLOCK_LINK *block,
                             LSN lsn)
{
  DBUG_ENTER("pagecache_unpin_by_link");
  DBUG_PRINT("enter", ("block: 0x%lx  fd: %u page: %lu",
                       (ulong) block,
                       (uint) block->hash_link->file.file,
                       (ulong) block->hash_link->pageno));

  pagecache_pthread_mutex_lock(&pagecache->cache_lock);
  /*
    As soon as we keep lock cache can be used, and we have lock because want
    unlock.
  */
  DBUG_ASSERT(pagecache->can_be_used);
  /* we can't unpin such page without unlock */
  DBUG_ASSERT((block->status & PCBLOCK_DIRECT_W) == 0);

  inc_counter_for_resize_op(pagecache);

  if (lsn != LSN_IMPOSSIBLE)
    check_and_set_lsn(pagecache, lsn, block);

  /*
    We can just unpin only with keeping read lock because:
    a) we can't pin without any lock
    b) we can't unpin keeping write lock
  */
  if (make_lock_and_pin(pagecache, block,
                        PAGECACHE_LOCK_LEFT_READLOCKED,
                        PAGECACHE_UNPIN, FALSE))
    DBUG_ASSERT(0); /* should not happend */

  /*
    Link the block into the LRU chain if it's the last submitted request
    for the block and block will not be pinned.
    See NOTE for pagecache_unlock about registering requests.
  */
  unreg_request(pagecache, block, 1);

  dec_counter_for_resize_op(pagecache);

  pagecache_pthread_mutex_unlock(&pagecache->cache_lock);

  DBUG_VOID_RETURN;
}

/* description of how to change lock before and after read/write */
struct rw_lock_change
{
  my_bool need_lock_change; /* need changing of lock at the end */
  enum pagecache_page_lock new_lock; /* lock at the beginning */
  enum pagecache_page_lock unlock_lock; /* lock at the end */
};

/* description of how to change pin before and after read/write */
struct rw_pin_change
{
  enum pagecache_page_pin new_pin; /* pin status at the beginning */
  enum pagecache_page_pin unlock_pin; /* pin status at the end */
};

/**
  Depending on the lock which the user wants in pagecache_read(), we
  need to acquire a first type of lock at start of pagecache_read(), and
  downgrade it to a second type of lock at end. For example, if user
  asked for no lock (PAGECACHE_LOCK_LEFT_UNLOCKED) this translates into
  taking first a read lock PAGECACHE_LOCK_READ (to rightfully block on
  existing write locks) then read then unlock the lock i.e. change lock
  to PAGECACHE_LOCK_READ_UNLOCK (the "1" below tells that a change is
  needed).
*/ 

static struct rw_lock_change lock_to_read[8]=
{
  { /*PAGECACHE_LOCK_LEFT_UNLOCKED*/
    1,
    PAGECACHE_LOCK_READ, PAGECACHE_LOCK_READ_UNLOCK
  },
  { /*PAGECACHE_LOCK_LEFT_READLOCKED*/
    0,
    PAGECACHE_LOCK_LEFT_READLOCKED, PAGECACHE_LOCK_LEFT_READLOCKED
  },
  { /*PAGECACHE_LOCK_LEFT_WRITELOCKED*/
    0,
    PAGECACHE_LOCK_LEFT_WRITELOCKED, PAGECACHE_LOCK_LEFT_WRITELOCKED
  },
  { /*PAGECACHE_LOCK_READ*/
    1,
    PAGECACHE_LOCK_READ, PAGECACHE_LOCK_LEFT_READLOCKED
  },
  { /*PAGECACHE_LOCK_WRITE*/
    1,
    PAGECACHE_LOCK_WRITE, PAGECACHE_LOCK_LEFT_WRITELOCKED
  },
  { /*PAGECACHE_LOCK_READ_UNLOCK*/
    1,
    PAGECACHE_LOCK_LEFT_READLOCKED, PAGECACHE_LOCK_READ_UNLOCK
  },
  { /*PAGECACHE_LOCK_WRITE_UNLOCK*/
    1,
    PAGECACHE_LOCK_LEFT_WRITELOCKED, PAGECACHE_LOCK_WRITE_UNLOCK
  },
  { /*PAGECACHE_LOCK_WRITE_TO_READ*/
    1,
    PAGECACHE_LOCK_LEFT_WRITELOCKED, PAGECACHE_LOCK_WRITE_TO_READ
  }
};

/**
  Two sets of pin modes (every as for lock upper but for pinning). The
  difference between sets if whether we are going to provide caller with
  reference on the block or not
*/

static struct rw_pin_change lock_to_pin[2][8]=
{
  {
    { /*PAGECACHE_LOCK_LEFT_UNLOCKED*/
      PAGECACHE_PIN_LEFT_UNPINNED,
      PAGECACHE_PIN_LEFT_UNPINNED
    },
    { /*PAGECACHE_LOCK_LEFT_READLOCKED*/
      PAGECACHE_PIN_LEFT_UNPINNED,
      PAGECACHE_PIN_LEFT_UNPINNED,
    },
    { /*PAGECACHE_LOCK_LEFT_WRITELOCKED*/
      PAGECACHE_PIN_LEFT_PINNED,
      PAGECACHE_PIN_LEFT_PINNED
    },
    { /*PAGECACHE_LOCK_READ*/
      PAGECACHE_PIN_LEFT_UNPINNED,
      PAGECACHE_PIN_LEFT_UNPINNED
    },
    { /*PAGECACHE_LOCK_WRITE*/
      PAGECACHE_PIN,
      PAGECACHE_PIN_LEFT_PINNED
    },
    { /*PAGECACHE_LOCK_READ_UNLOCK*/
      PAGECACHE_PIN_LEFT_UNPINNED,
      PAGECACHE_PIN_LEFT_UNPINNED
    },
    { /*PAGECACHE_LOCK_WRITE_UNLOCK*/
      PAGECACHE_PIN_LEFT_PINNED,
      PAGECACHE_UNPIN
    },
    { /*PAGECACHE_LOCK_WRITE_TO_READ*/
      PAGECACHE_PIN_LEFT_PINNED,
      PAGECACHE_UNPIN
    }
  },
  {
    { /*PAGECACHE_LOCK_LEFT_UNLOCKED*/
      PAGECACHE_PIN_LEFT_UNPINNED,
      PAGECACHE_PIN_LEFT_UNPINNED
    },
    { /*PAGECACHE_LOCK_LEFT_READLOCKED*/
      PAGECACHE_PIN_LEFT_UNPINNED,
      PAGECACHE_PIN_LEFT_UNPINNED,
    },
    { /*PAGECACHE_LOCK_LEFT_WRITELOCKED*/
      PAGECACHE_PIN_LEFT_PINNED,
      PAGECACHE_PIN_LEFT_PINNED
    },
    { /*PAGECACHE_LOCK_READ*/
      PAGECACHE_PIN,
      PAGECACHE_PIN_LEFT_PINNED
    },
    { /*PAGECACHE_LOCK_WRITE*/
      PAGECACHE_PIN,
      PAGECACHE_PIN_LEFT_PINNED
    },
    { /*PAGECACHE_LOCK_READ_UNLOCK*/
      PAGECACHE_PIN_LEFT_UNPINNED,
      PAGECACHE_PIN_LEFT_UNPINNED
    },
    { /*PAGECACHE_LOCK_WRITE_UNLOCK*/
      PAGECACHE_PIN_LEFT_PINNED,
      PAGECACHE_UNPIN
    },
    { /*PAGECACHE_LOCK_WRITE_TO_READ*/
      PAGECACHE_PIN_LEFT_PINNED,
      PAGECACHE_PIN_LEFT_PINNED,
    }
  }
};


/*
  @brief Read a block of data from a cached file into a buffer;

  @param pagecache      pointer to a page cache data structure
  @param file           handler for the file for the block of data to be read
  @param pageno         number of the block of data in the file
  @param level          determines the weight of the data
  @param buff           buffer to where the data must be placed
  @param type           type of the page
  @param lock           lock change
  @param link           link to the page if we pin it

  @return address from where the data is placed if successful, 0 - otherwise.

  @note Pin will be chosen according to lock parameter (see lock_to_pin)

  @note 'buff', if not NULL, must be long-aligned.

  @note  If buff==0 then we provide reference on the page so should keep the
  page pinned.
*/

uchar *pagecache_read(PAGECACHE *pagecache,
                      PAGECACHE_FILE *file,
                      pgcache_page_no_t pageno,
                      uint level,
                      uchar *buff,
                      enum pagecache_page_type type,
                      enum pagecache_page_lock lock,
                      PAGECACHE_BLOCK_LINK **page_link)
{
  my_bool error= 0;
  enum pagecache_page_pin
    new_pin= lock_to_pin[buff==0][lock].new_pin,
    unlock_pin= lock_to_pin[buff==0][lock].unlock_pin;
  PAGECACHE_BLOCK_LINK *fake_link;
  my_bool reg_request;
#ifndef DBUG_OFF
  char llbuf[22];
  DBUG_ENTER("pagecache_read");
  DBUG_PRINT("enter", ("fd: %u  page: %s  buffer: 0x%lx level: %u  "
                       "t:%s  (%d)%s->%s  %s->%s",
                       (uint) file->file, ullstr(pageno, llbuf),
                       (ulong) buff, level,
                       page_cache_page_type_str[type],
                       lock_to_read[lock].need_lock_change,
                       page_cache_page_lock_str[lock_to_read[lock].new_lock],
                       page_cache_page_lock_str[lock_to_read[lock].unlock_lock],
                       page_cache_page_pin_str[new_pin],
                       page_cache_page_pin_str[unlock_pin]));
  DBUG_ASSERT(buff != 0 || (buff == 0 && (unlock_pin == PAGECACHE_PIN ||
                                          unlock_pin == PAGECACHE_PIN_LEFT_PINNED)));
  DBUG_ASSERT(pageno < ((ULL(1)) << 40));
#endif

  if (!page_link)
    page_link= &fake_link;
  *page_link= 0;                                 /* Catch errors */

restart:

  if (pagecache->can_be_used)
  {
    /* Key cache is used */
    PAGECACHE_BLOCK_LINK *block;
    uint status;
    int page_st;

    pagecache_pthread_mutex_lock(&pagecache->cache_lock);
    if (!pagecache->can_be_used)
    {
      pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
      goto no_key_cache;
    }

    inc_counter_for_resize_op(pagecache);
    pagecache->global_cache_r_requests++;
    /* See NOTE for pagecache_unlock about registering requests. */
    reg_request= ((new_pin == PAGECACHE_PIN_LEFT_UNPINNED) ||
                  (new_pin == PAGECACHE_PIN));
    block= find_block(pagecache, file, pageno, level,
                      lock == PAGECACHE_LOCK_WRITE,
                      reg_request, &page_st);
    DBUG_PRINT("info", ("Block type: %s current type %s",
                        page_cache_page_type_str[block->type],
                        page_cache_page_type_str[type]));
    if (((block->status & PCBLOCK_ERROR) == 0) && (page_st != PAGE_READ))
    {
      /* The requested page is to be read into the block buffer */
      read_block(pagecache, block,
                 (my_bool)(page_st == PAGE_TO_BE_READ));
      DBUG_PRINT("info", ("read is done"));
    }
    /*
      Assert after block is read. Imagine two concurrent SELECTs on same
      table (thread1 and 2), which want to pagecache_read() the same
      pageno/fileno. Thread1 calls find_block(), decides to evict a dirty
      page from LRU; while it's writing this dirty page to disk, it is
      pre-empted and thread2 runs its find_block(), gets the block (in
      PAGE_TO_BE_READ state). This block is still containing the in-eviction
      dirty page so has an its type, which cannot be tested.
      So thread2 has to wait for read_block() to finish (when it wakes up in
      read_block(), it's woken up by read_block() of thread1, which implies
      that block's type was set to EMPTY by thread1 as part of find_block()).
    */
    DBUG_ASSERT(block->type == PAGECACHE_EMPTY_PAGE ||
                block->type == type ||
                type == PAGECACHE_LSN_PAGE ||
                type == PAGECACHE_READ_UNKNOWN_PAGE ||
                block->type == PAGECACHE_READ_UNKNOWN_PAGE);
    if (type != PAGECACHE_READ_UNKNOWN_PAGE ||
        block->type == PAGECACHE_EMPTY_PAGE)
      block->type= type;

    if (make_lock_and_pin(pagecache, block, lock_to_read[lock].new_lock,
                          new_pin, FALSE))
    {
      /*
        We failed to write lock the block, cache is unlocked,
        we will try to get the block again.
      */
      if (reg_request)
        unreg_request(pagecache, block, 1);
      pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
      DBUG_PRINT("info", ("restarting..."));
      goto restart;
    }

    status= block->status;
    if (!buff)
    {
      buff=  block->buffer;
      /* possibly we will write here (resolved on unlock) */
      if ((lock == PAGECACHE_LOCK_WRITE ||
           lock == PAGECACHE_LOCK_LEFT_WRITELOCKED) &&
          !(block->status & PCBLOCK_CHANGED))
      {
        block->status|= PCBLOCK_DIRECT_W;
        DBUG_PRINT("info", ("Set PCBLOCK_DIRECT_W for block: 0x%lx",
                            (ulong) block));
      }
    }
    else
    {
      if (!(status & PCBLOCK_ERROR))
      {
#if !defined(SERIALIZED_READ_FROM_CACHE)
        pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
#endif

        DBUG_ASSERT((pagecache->block_size & 511) == 0);
        /* Copy data from the cache buffer */
        bmove512(buff, block->buffer, pagecache->block_size);

#if !defined(SERIALIZED_READ_FROM_CACHE)
        pagecache_pthread_mutex_lock(&pagecache->cache_lock);
#endif
      }
      else
        my_errno= block->error;
    }

    remove_reader(block);
    if (lock_to_read[lock].need_lock_change)
    {
      if (make_lock_and_pin(pagecache, block,
                            lock_to_read[lock].unlock_lock,
                            unlock_pin, FALSE))
        DBUG_ASSERT(0);
    }
    /*
      Link the block into the LRU chain if it's the last submitted request
      for the block and block will not be pinned.
      See NOTE for pagecache_unlock about registering requests.
    */
    if (unlock_pin == PAGECACHE_PIN_LEFT_UNPINNED ||
        unlock_pin == PAGECACHE_UNPIN)
      unreg_request(pagecache, block, 1);
    else
      *page_link= block;

    dec_counter_for_resize_op(pagecache);

    pagecache_pthread_mutex_unlock(&pagecache->cache_lock);

    if (status & PCBLOCK_ERROR)
    {
      DBUG_ASSERT(my_errno != 0);
      DBUG_PRINT("error", ("Got error %d when doing page read", my_errno));
      DBUG_RETURN((uchar *) 0);
    }

    DBUG_RETURN(buff);
  }

no_key_cache:					/* Key cache is not used */

  /* We can't use mutex here as the key cache may not be initialized */
  pagecache->global_cache_r_requests++;
  pagecache->global_cache_read++;
  if (pagecache_fread(pagecache, file, buff, pageno,
                      pagecache->readwrite_flags))
    error= 1;
  DBUG_RETURN(error ? (uchar*) 0 : buff);
}


/*
  @brief Set/reset flag that page always should be flushed on delete

  @param pagecache      pointer to a page cache data structure
  @param link           direct link to page (returned by read or write)
  @param write          write on delete flag value

*/

void pagecache_set_write_on_delete_by_link(PAGECACHE_BLOCK_LINK *block)
{
  DBUG_ENTER("pagecache_set_write_on_delete_by_link");
  DBUG_PRINT("enter", ("fd: %d block 0x%lx  %d -> TRUE",
                       block->hash_link->file.file,
                       (ulong) block,
                       (int) block->status & PCBLOCK_DEL_WRITE));
  DBUG_ASSERT(block->pins); /* should be pinned */
  DBUG_ASSERT(block->wlocks); /* should be write locked */

  block->status|= PCBLOCK_DEL_WRITE;

  DBUG_VOID_RETURN;
}


/*
  @brief Delete page from the buffer (common part for link and file/page)

  @param pagecache      pointer to a page cache data structure
  @param block          direct link to page (returned by read or write)
  @param page_link      hash link of the block
  @param flush          flush page if it is dirty

  @retval 0 deleted or was not present at all
  @retval 1 error

*/

static my_bool pagecache_delete_internal(PAGECACHE *pagecache,
                                         PAGECACHE_BLOCK_LINK *block,
                                         PAGECACHE_HASH_LINK *page_link,
                                         my_bool flush)
{
  my_bool error= 0;
  if (block->status & PCBLOCK_IN_FLUSH)
  {
    /*
      this call is just 'hint' for the cache to free the page so we will
      not interferes with flushing process but must return success
    */
    goto out;
  }
  if (block->status & PCBLOCK_CHANGED)
  {
    flush= (flush || (block->status & PCBLOCK_DEL_WRITE));
    if (flush)
    {
      /* The block contains a dirty page - push it out of the cache */

      KEYCACHE_DBUG_PRINT("find_block", ("block is dirty"));

      /*
        The call is thread safe because only the current
        thread might change the block->hash_link value
      */
      DBUG_ASSERT(block->pins == 1);
      pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
      error= pagecache_fwrite(pagecache,
                              &block->hash_link->file,
                              block->buffer,
                              block->hash_link->pageno,
                              block->type,
                              pagecache->readwrite_flags);
      pagecache_pthread_mutex_lock(&pagecache->cache_lock);
      pagecache->global_cache_write++;

      if (error)
      {
        block->status|= PCBLOCK_ERROR;
        block->error=   (int16) my_errno;
        my_debug_put_break_here();
        goto out;
      }
    }
    else
    {
      PAGECACHE_FILE *filedesc= &block->hash_link->file;
      /* We are not going to write the page but have to call callbacks */
      DBUG_PRINT("info", ("flush_callback :0x%lx"
                          "write_callback: 0x%lx  data: 0x%lx",
                          (ulong) filedesc->flush_log_callback,
                          (ulong) filedesc->write_callback,
                          (ulong) filedesc->callback_data));
      if ((*filedesc->flush_log_callback)
          (block->buffer, block->hash_link->pageno, filedesc->callback_data) ||
          (*filedesc->write_callback)
          (block->buffer, block->hash_link->pageno, filedesc->callback_data))
      {
        DBUG_PRINT("error", ("flush or write callback problem"));
        error= 1;
        goto out;
      }
    }
    pagecache->blocks_changed--;
    pagecache->global_blocks_changed--;
    /*
      free_block() will change the status and rec_lsn of the block so no
      need to change them here.
    */
  }
  /* Cache is locked, so we can relese page before freeing it */
  if (make_lock_and_pin(pagecache, block,
                        PAGECACHE_LOCK_WRITE_UNLOCK,
                        PAGECACHE_UNPIN, FALSE))
    DBUG_ASSERT(0);
  DBUG_ASSERT(block->hash_link->requests > 0);
  page_link->requests--;
  /* See NOTE for pagecache_unlock about registering requests. */
  free_block(pagecache, block);
  dec_counter_for_resize_op(pagecache);
  return 0;

out:
  /* Cache is locked, so we can relese page before freeing it */
  if (make_lock_and_pin(pagecache, block,
                        PAGECACHE_LOCK_WRITE_UNLOCK,
                        PAGECACHE_UNPIN, FALSE))
    DBUG_ASSERT(0);
  page_link->requests--;
  unreg_request(pagecache, block, 1);
  dec_counter_for_resize_op(pagecache);
  return error;
}


/*
  @brief Delete page from the buffer by link

  @param pagecache      pointer to a page cache data structure
  @param link           direct link to page (returned by read or write)
  @param lock           lock change
  @param flush          flush page if it is dirty

  @retval 0 deleted or was not present at all
  @retval 1 error

  @note lock  can be only PAGECACHE_LOCK_LEFT_WRITELOCKED (page was
  write locked before) or PAGECACHE_LOCK_WRITE (delete will write
  lock page before delete)
*/

my_bool pagecache_delete_by_link(PAGECACHE *pagecache,
                                 PAGECACHE_BLOCK_LINK *block,
                                 enum pagecache_page_lock lock,
                                 my_bool flush)
{
  my_bool error= 0;
  enum pagecache_page_pin pin= PAGECACHE_PIN_LEFT_PINNED;
  DBUG_ENTER("pagecache_delete_by_link");
  DBUG_PRINT("enter", ("fd: %d block 0x%lx  %s  %s",
                       block->hash_link->file.file,
                       (ulong) block,
                       page_cache_page_lock_str[lock],
                       page_cache_page_pin_str[pin]));
  DBUG_ASSERT(lock == PAGECACHE_LOCK_WRITE ||
              lock == PAGECACHE_LOCK_LEFT_WRITELOCKED);
  DBUG_ASSERT(block->pins != 0); /* should be pinned */

  if (pagecache->can_be_used)
  {
    pagecache_pthread_mutex_lock(&pagecache->cache_lock);
    if (!pagecache->can_be_used)
      goto end;

    /*
      This block should be pinned (i.e. has not zero request counter) =>
      Such block can't be chosen for eviction.
    */
    DBUG_ASSERT((block->status &
                 (PCBLOCK_IN_SWITCH | PCBLOCK_REASSIGNED)) == 0);

    inc_counter_for_resize_op(pagecache);
    /*
      make_lock_and_pin() can't fail here, because we are keeping pin on the
      block and it can't be evicted (which is cause of lock fail and retry)
    */
    if (make_lock_and_pin(pagecache, block, lock, pin, FALSE))
      DBUG_ASSERT(0);

    /*
      get_present_hash_link() side effect emulation before call
      pagecache_delete_internal()
    */
    block->hash_link->requests++;

    error= pagecache_delete_internal(pagecache, block, block->hash_link,
                                     flush);
end:
    pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
  }

  DBUG_RETURN(error);
}


/**
  @brief Returns "hits" for promotion

  @return "hits" for promotion
*/

uint pagecache_pagelevel(PAGECACHE_BLOCK_LINK *block)
{
  return block->hits_left;
}

/*
  @brief Adds "hits" to the page

  @param link           direct link to page (returned by read or write)
  @param level          number of "hits" which we add to the page
*/

void pagecache_add_level_by_link(PAGECACHE_BLOCK_LINK *block,
                                 uint level)
{
  DBUG_ASSERT(block->pins != 0); /* should be pinned */
  /*
    Operation is just for statistics so it is not really important
    if it interfere with other hit increasing => we are doing it without
    locking the pagecache.
  */
  block->hits_left+= level;
}

/*
  @brief Delete page from the buffer

  @param pagecache      pointer to a page cache data structure
  @param file           handler for the file for the block of data to be read
  @param pageno         number of the block of data in the file
  @param lock           lock change
  @param flush          flush page if it is dirty

  @retval 0 deleted or was not present at all
  @retval 1 error

  @note lock  can be only PAGECACHE_LOCK_LEFT_WRITELOCKED (page was
  write locked before) or PAGECACHE_LOCK_WRITE (delete will write
  lock page before delete)
*/
static enum pagecache_page_pin lock_to_pin_one_phase[8]=
{
  PAGECACHE_PIN_LEFT_UNPINNED /*PAGECACHE_LOCK_LEFT_UNLOCKED*/,
  PAGECACHE_PIN_LEFT_UNPINNED /*PAGECACHE_LOCK_LEFT_READLOCKED*/,
  PAGECACHE_PIN_LEFT_PINNED   /*PAGECACHE_LOCK_LEFT_WRITELOCKED*/,
  PAGECACHE_PIN_LEFT_UNPINNED /*PAGECACHE_LOCK_READ*/,
  PAGECACHE_PIN               /*PAGECACHE_LOCK_WRITE*/,
  PAGECACHE_PIN_LEFT_UNPINNED /*PAGECACHE_LOCK_READ_UNLOCK*/,
  PAGECACHE_UNPIN             /*PAGECACHE_LOCK_WRITE_UNLOCK*/,
  PAGECACHE_UNPIN             /*PAGECACHE_LOCK_WRITE_TO_READ*/
};

my_bool pagecache_delete(PAGECACHE *pagecache,
                         PAGECACHE_FILE *file,
                         pgcache_page_no_t pageno,
                         enum pagecache_page_lock lock,
                         my_bool flush)
{
  my_bool error= 0;
  enum pagecache_page_pin pin= lock_to_pin_one_phase[lock];
  DBUG_ENTER("pagecache_delete");
  DBUG_PRINT("enter", ("fd: %u  page: %lu  %s  %s",
                       (uint) file->file, (ulong) pageno,
                       page_cache_page_lock_str[lock],
                       page_cache_page_pin_str[pin]));
  DBUG_ASSERT(lock == PAGECACHE_LOCK_WRITE ||
              lock == PAGECACHE_LOCK_LEFT_WRITELOCKED);
  DBUG_ASSERT(pin == PAGECACHE_PIN ||
              pin == PAGECACHE_PIN_LEFT_PINNED);
restart:

  DBUG_ASSERT(pageno < ((ULL(1)) << 40));
  if (pagecache->can_be_used)
  {
    /* Key cache is used */
    reg1 PAGECACHE_BLOCK_LINK *block;
    PAGECACHE_HASH_LINK **unused_start, *page_link;

    pagecache_pthread_mutex_lock(&pagecache->cache_lock);
    if (!pagecache->can_be_used)
      goto end;

    inc_counter_for_resize_op(pagecache);
    page_link= get_present_hash_link(pagecache, file, pageno, &unused_start);
    if (!page_link)
    {
      DBUG_PRINT("info", ("There is no such page in the cache"));
      dec_counter_for_resize_op(pagecache);
      pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
      DBUG_RETURN(0);
    }
    block= page_link->block;
    if (block->status & (PCBLOCK_REASSIGNED | PCBLOCK_IN_SWITCH))
    {
      DBUG_PRINT("info", ("Block 0x%0lx already is %s",
                          (ulong) block,
                          ((block->status & PCBLOCK_REASSIGNED) ?
                           "reassigned" : "in switch")));
      PCBLOCK_INFO(block);
      page_link->requests--;
      dec_counter_for_resize_op(pagecache);
      goto end;
    }
    /* See NOTE for pagecache_unlock about registering requests. */
    if (pin == PAGECACHE_PIN)
      reg_requests(pagecache, block, 1);
    DBUG_ASSERT(block != 0);
    if (make_lock_and_pin(pagecache, block, lock, pin, FALSE))
    {
      /*
        We failed to writelock the block, cache is unlocked, and last write
        lock is released, we will try to get the block again.
      */
      if (pin == PAGECACHE_PIN)
        unreg_request(pagecache, block, 1);
      pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
      DBUG_PRINT("info", ("restarting..."));
      goto restart;
    }

    /* we can't delete with opened direct link for write */
    DBUG_ASSERT((block->status & PCBLOCK_DIRECT_W) == 0);

    error= pagecache_delete_internal(pagecache, block, page_link, flush);
end:
    pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
  }

  DBUG_RETURN(error);
}


my_bool pagecache_delete_pages(PAGECACHE *pagecache,
                               PAGECACHE_FILE *file,
                               pgcache_page_no_t pageno,
                               uint page_count,
                               enum pagecache_page_lock lock,
                               my_bool flush)
{
  pgcache_page_no_t page_end;
  DBUG_ENTER("pagecache_delete_pages");
  DBUG_ASSERT(page_count > 0);

  page_end= pageno + page_count;
  do
  {
    if (pagecache_delete(pagecache, file, pageno,
                         lock, flush))
      DBUG_RETURN(1);
  } while (++pageno != page_end);
  DBUG_RETURN(0);
}


/**
  @brief Writes a buffer into a cached file.

  @param pagecache       pointer to a page cache data structure
  @param file            handler for the file to write data to
  @param pageno          number of the block of data in the file
  @param level           determines the weight of the data
  @param buff            buffer with the data
  @param type            type of the page
  @param lock            lock change
  @param pin             pin page
  @param write_mode      how to write page
  @param link            link to the page if we pin it
  @param first_REDO_LSN_for_page the lsn to set rec_lsn
  @param offset          offset in the page
  @param size            size of data
  @param validator       read page validator
  @param validator_data  the validator data

  @retval 0 if a success.
  @retval 1 Error.
*/

static struct rw_lock_change write_lock_change_table[]=
{
  {1,
   PAGECACHE_LOCK_WRITE,
   PAGECACHE_LOCK_WRITE_UNLOCK} /*PAGECACHE_LOCK_LEFT_UNLOCKED*/,
  {0, /*unsupported (we can't write having the block read locked) */
   PAGECACHE_LOCK_LEFT_UNLOCKED,
   PAGECACHE_LOCK_LEFT_UNLOCKED} /*PAGECACHE_LOCK_LEFT_READLOCKED*/,
  {0, PAGECACHE_LOCK_LEFT_WRITELOCKED, 0} /*PAGECACHE_LOCK_LEFT_WRITELOCKED*/,
  {1,
   PAGECACHE_LOCK_WRITE,
   PAGECACHE_LOCK_WRITE_TO_READ} /*PAGECACHE_LOCK_READ*/,
  {0, PAGECACHE_LOCK_WRITE, 0} /*PAGECACHE_LOCK_WRITE*/,
  {0, /*unsupported (we can't write having the block read locked) */
   PAGECACHE_LOCK_LEFT_UNLOCKED,
   PAGECACHE_LOCK_LEFT_UNLOCKED} /*PAGECACHE_LOCK_READ_UNLOCK*/,
  {1,
   PAGECACHE_LOCK_LEFT_WRITELOCKED,
   PAGECACHE_LOCK_WRITE_UNLOCK } /*PAGECACHE_LOCK_WRITE_UNLOCK*/,
  {1,
   PAGECACHE_LOCK_LEFT_WRITELOCKED,
   PAGECACHE_LOCK_WRITE_TO_READ} /*PAGECACHE_LOCK_WRITE_TO_READ*/
};


static struct rw_pin_change write_pin_change_table[]=
{
  {PAGECACHE_PIN_LEFT_PINNED,
   PAGECACHE_PIN_LEFT_PINNED} /*PAGECACHE_PIN_LEFT_PINNED*/,
  {PAGECACHE_PIN,
   PAGECACHE_UNPIN} /*PAGECACHE_PIN_LEFT_UNPINNED*/,
  {PAGECACHE_PIN,
   PAGECACHE_PIN_LEFT_PINNED} /*PAGECACHE_PIN*/,
  {PAGECACHE_PIN_LEFT_PINNED,
   PAGECACHE_UNPIN} /*PAGECACHE_UNPIN*/
};


/**
  @note 'buff', if not NULL, must be long-aligned.
*/

my_bool pagecache_write_part(PAGECACHE *pagecache,
                             PAGECACHE_FILE *file,
                             pgcache_page_no_t pageno,
                             uint level,
                             uchar *buff,
                             enum pagecache_page_type type,
                             enum pagecache_page_lock lock,
                             enum pagecache_page_pin pin,
                             enum pagecache_write_mode write_mode,
                             PAGECACHE_BLOCK_LINK **page_link,
                             LSN first_REDO_LSN_for_page,
                             uint offset, uint size)
{
  PAGECACHE_BLOCK_LINK *block= NULL;
  PAGECACHE_BLOCK_LINK *fake_link;
  my_bool error= 0;
  int need_lock_change= write_lock_change_table[lock].need_lock_change;
  my_bool reg_request;
#ifndef DBUG_OFF
  char llbuf[22];
  DBUG_ENTER("pagecache_write_part");
  DBUG_PRINT("enter", ("fd: %u  page: %s  level: %u  type: %s  lock: %s  "
                       "pin: %s   mode: %s  offset: %u  size %u",
                       (uint) file->file, ullstr(pageno, llbuf), level,
                       page_cache_page_type_str[type],
                       page_cache_page_lock_str[lock],
                       page_cache_page_pin_str[pin],
                       page_cache_page_write_mode_str[write_mode],
                       offset, size));
  DBUG_ASSERT(type != PAGECACHE_READ_UNKNOWN_PAGE);
  DBUG_ASSERT(lock != PAGECACHE_LOCK_LEFT_READLOCKED);
  DBUG_ASSERT(lock != PAGECACHE_LOCK_READ_UNLOCK);
  DBUG_ASSERT(offset + size <= pagecache->block_size);
  DBUG_ASSERT(pageno < ((ULL(1)) << 40));
#endif

  if (!page_link)
    page_link= &fake_link;
  *page_link= 0;

restart:

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("check_pagecache",
               test_key_cache(pagecache, "start of key_cache_write", 1););
#endif

  if (pagecache->can_be_used)
  {
    /* Key cache is used */
    int page_st;
    my_bool need_page_ready_signal= FALSE;

    pagecache_pthread_mutex_lock(&pagecache->cache_lock);
    if (!pagecache->can_be_used)
    {
      pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
      goto no_key_cache;
    }

    inc_counter_for_resize_op(pagecache);
    pagecache->global_cache_w_requests++;
    /* See NOTE for pagecache_unlock about registering requests. */
    reg_request= ((pin == PAGECACHE_PIN_LEFT_UNPINNED) ||
                  (pin == PAGECACHE_PIN));
    block= find_block(pagecache, file, pageno, level,
                      TRUE,
                      reg_request, &page_st);
    if (!block)
    {
      DBUG_ASSERT(write_mode != PAGECACHE_WRITE_DONE);
      /* It happens only for requests submitted during resize operation */
      dec_counter_for_resize_op(pagecache);
      pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
      /* Write to the disk key cache is in resize at the moment*/
      goto no_key_cache;
    }
    DBUG_PRINT("info", ("page status: %d", page_st));
    if (!(block->status & PCBLOCK_ERROR) &&
        ((page_st == PAGE_TO_BE_READ &&
          (offset || size < pagecache->block_size)) ||
         (page_st == PAGE_WAIT_TO_BE_READ)))
    {
      /* The requested page is to be read into the block buffer */
      read_block(pagecache, block,
                 (my_bool)(page_st == PAGE_TO_BE_READ));
      DBUG_PRINT("info", ("read is done"));
    }
    else if (page_st == PAGE_TO_BE_READ)
    {
      need_page_ready_signal= TRUE;
    }

    DBUG_ASSERT(block->type == PAGECACHE_EMPTY_PAGE ||
                block->type == PAGECACHE_READ_UNKNOWN_PAGE ||
                block->type == type ||
                /* this is for when going to non-trans to trans */
                (block->type == PAGECACHE_PLAIN_PAGE &&
                 type == PAGECACHE_LSN_PAGE));
    block->type= type;
    /* we write to the page so it has no sense to keep the flag */
    block->status&= ~PCBLOCK_DIRECT_W;
    DBUG_PRINT("info", ("Drop PCBLOCK_DIRECT_W for block: 0x%lx",
                        (ulong) block));

    if (make_lock_and_pin(pagecache, block,
                          write_lock_change_table[lock].new_lock,
                          (need_lock_change ?
                           write_pin_change_table[pin].new_pin :
                           pin), FALSE))
    {
      /*
        We failed to writelock the block, cache is unlocked, and last write
        lock is released, we will try to get the block again.
      */
      if (reg_request)
        unreg_request(pagecache, block, 1);
      pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
      DBUG_PRINT("info", ("restarting..."));
      goto restart;
    }

    if (write_mode == PAGECACHE_WRITE_DONE)
    {
      if (block->status & PCBLOCK_ERROR)
      {
        my_debug_put_break_here();
        DBUG_PRINT("warning", ("Writing on page with error"));
      }
      else
      {
        /* Copy data from buff */
        if (!(size & 511))
          bmove512(block->buffer + offset, buff, size);
        else
          memcpy(block->buffer + offset, buff, size);
        block->status= PCBLOCK_READ;
        /*
          The read_callback can change the page content (removing page
          protection) so it have to be called
        */
        DBUG_PRINT("info", ("read_callback: 0x%lx  data: 0x%lx",
                            (ulong) block->hash_link->file.read_callback,
                            (ulong) block->hash_link->file.callback_data));
        if ((*block->hash_link->file.read_callback)(block->buffer,
                                                    block->hash_link->pageno,
                                                    block->hash_link->
                                                    file.callback_data))
        {
          DBUG_PRINT("error", ("read callback problem"));
          block->status|= PCBLOCK_ERROR;
          block->error= (int16) my_errno;
          my_debug_put_break_here();
        }
        KEYCACHE_DBUG_PRINT("key_cache_insert",
                            ("Page injection"));
#ifdef THREAD
        /* Signal that all pending requests for this now can be processed. */
        if (block->wqueue[COND_FOR_REQUESTED].last_thread)
          wqueue_release_queue(&block->wqueue[COND_FOR_REQUESTED]);
#endif
      }
    }
    else
    {
      if (! (block->status & PCBLOCK_CHANGED))
          link_to_changed_list(pagecache, block);

      if (!(size & 511))
        bmove512(block->buffer + offset, buff, size);
      else
        memcpy(block->buffer + offset, buff, size);
      block->status|= PCBLOCK_READ;
      /* Page is correct again if we made a full write in it */
      if (size == pagecache->block_size)
        block->status&= ~PCBLOCK_ERROR;
    }

#ifdef THREAD
    if (need_page_ready_signal &&
        block->wqueue[COND_FOR_REQUESTED].last_thread)
      wqueue_release_queue(&block->wqueue[COND_FOR_REQUESTED]);
#endif

    if (first_REDO_LSN_for_page)
    {
      /* single write action of the last write action */
      DBUG_ASSERT(lock == PAGECACHE_LOCK_WRITE_UNLOCK ||
                  lock == PAGECACHE_LOCK_LEFT_UNLOCKED);
      DBUG_ASSERT(pin == PAGECACHE_UNPIN ||
                  pin == PAGECACHE_PIN_LEFT_UNPINNED);
      pagecache_set_block_rec_lsn(block, first_REDO_LSN_for_page);
    }

    if (need_lock_change)
    {
      /*
        We don't set rec_lsn of the block; this is ok as for the
        Maria-block-record's pages, we always keep pages pinned here.
      */
      if (make_lock_and_pin(pagecache, block,
                            write_lock_change_table[lock].unlock_lock,
                            write_pin_change_table[pin].unlock_pin, FALSE))
        DBUG_ASSERT(0);
    }

    /* Unregister the request */
    DBUG_ASSERT(block->hash_link->requests > 0);
    block->hash_link->requests--;
    /* See NOTE for pagecache_unlock about registering requests. */
    if (pin == PAGECACHE_PIN_LEFT_UNPINNED || pin == PAGECACHE_UNPIN)
      unreg_request(pagecache, block, 1);
    else
      *page_link= block;

    if (block->status & PCBLOCK_ERROR)
    {
      error= 1;
      my_debug_put_break_here();
    }

    dec_counter_for_resize_op(pagecache);

    pagecache_pthread_mutex_unlock(&pagecache->cache_lock);

    goto end;
  }

no_key_cache:
  /*
    We can't by pass the normal page cache operations because need
    whole page for calling callbacks & so on.
    This branch should not be used for now (but it is fixed as it
    should be just to avoid confusing)
  */
  DBUG_ASSERT(0);
  /* Key cache is not used */
  if (write_mode == PAGECACHE_WRITE_DELAY)
  {
    /* We can't use mutex here as the key cache may not be initialized */
    pagecache->global_cache_w_requests++;
    pagecache->global_cache_write++;
    if (offset != 0 || size != pagecache->block_size)
    {
      uchar *page_buffer= (uchar *) alloca(pagecache->block_size);

      pagecache->global_cache_read++;
      if ((error= (pagecache_fread(pagecache, file,
                                   page_buffer,
                                   pageno,
                                   pagecache->readwrite_flags) != 0)))
        goto end;
      if ((file->read_callback)(page_buffer, pageno, file->callback_data))
      {
        DBUG_PRINT("error", ("read callback problem"));
        error= 1;
        goto end;
      }
      memcpy((char *)page_buffer + offset, buff, size);
      buff= page_buffer;
    }
    if (pagecache_fwrite(pagecache, file, buff, pageno, type,
                         pagecache->readwrite_flags))
      error= 1;
  }

end:
#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
  DBUG_EXECUTE("exec",
               test_key_cache(pagecache, "end of key_cache_write", 1););
#endif
  if (block)
    PCBLOCK_INFO(block);
  else
    DBUG_PRINT("info", ("No block"));
  DBUG_RETURN(error);
}


/*
  Free block: remove reference to it from hash table,
  remove it from the chain file of dirty/clean blocks
  and add it to the free list.
*/

static void free_block(PAGECACHE *pagecache, PAGECACHE_BLOCK_LINK *block)
{
  KEYCACHE_THREAD_TRACE("free block");
  KEYCACHE_DBUG_PRINT("free_block",
                      ("block: %u  hash_link 0x%lx",
                       PCBLOCK_NUMBER(pagecache, block),
                       (long) block->hash_link));
  safe_mutex_assert_owner(&pagecache->cache_lock);
  if (block->hash_link)
  {
    /*
      While waiting for readers to finish, new readers might request the
      block. But since we set block->status|= PCBLOCK_REASSIGNED, they
      will wait on block->wqueue[COND_FOR_SAVED]. They must be signalled
      later.
    */
    block->status|= PCBLOCK_REASSIGNED;
    wait_for_readers(pagecache, block);
    unlink_hash(pagecache, block->hash_link);
  }

  unlink_changed(block);
  DBUG_ASSERT(block->wlocks == 0);
  DBUG_ASSERT(block->rlocks == 0);
  DBUG_ASSERT(block->rlocks_queue == 0);
  DBUG_ASSERT(block->pins == 0);
  DBUG_ASSERT((block->status & ~(PCBLOCK_ERROR | PCBLOCK_READ | PCBLOCK_IN_FLUSH | PCBLOCK_CHANGED | PCBLOCK_REASSIGNED)) == 0);
  block->status= 0;
#ifndef DBUG_OFF
  block->type= PAGECACHE_EMPTY_PAGE;
#endif
  block->rec_lsn= LSN_MAX;
  KEYCACHE_THREAD_TRACE("free block");
  KEYCACHE_DBUG_PRINT("free_block",
                      ("block is freed"));
  unreg_request(pagecache, block, 0);
  DBUG_ASSERT(block->requests == 0);
  DBUG_ASSERT(block->next_used != 0);
  block->hash_link= NULL;

  /* Remove the free block from the LRU ring. */
  unlink_block(pagecache, block);
  if (block->temperature == PCBLOCK_WARM)
    pagecache->warm_blocks--;
  block->temperature= PCBLOCK_COLD;
  /* Insert the free block in the free list. */
  block->next_used= pagecache->free_block_list;
  pagecache->free_block_list= block;
  /* Keep track of the number of currently unused blocks. */
  pagecache->blocks_unused++;

#ifdef THREAD
  /* All pending requests for this page must be resubmitted. */
  if (block->wqueue[COND_FOR_SAVED].last_thread)
    wqueue_release_queue(&block->wqueue[COND_FOR_SAVED]);
#endif
}


static int cmp_sec_link(PAGECACHE_BLOCK_LINK **a, PAGECACHE_BLOCK_LINK **b)
{
  return (((*a)->hash_link->pageno < (*b)->hash_link->pageno) ? -1 :
      ((*a)->hash_link->pageno > (*b)->hash_link->pageno) ? 1 : 0);
}


/**
  @brief Flush a portion of changed blocks to disk, free used blocks
  if requested

  @param pagecache       This page cache reference.
  @param file            File which should be flushed
  @param cache           Beginning of array of the block.
  @param end             Reference to the block after last in the array.
  @param flush_type      Type of the flush.
  @param first_errno     Where to store first errno of the flush.


  @return Operation status
  @retval PCFLUSH_OK OK
  @retval PCFLUSH_ERROR There was errors during the flush process.
  @retval PCFLUSH_PINNED Pinned blocks was met and skipped.
  @retval PCFLUSH_PINNED_AND_ERROR PCFLUSH_ERROR and PCFLUSH_PINNED.
*/

static int flush_cached_blocks(PAGECACHE *pagecache,
                               PAGECACHE_FILE *file,
                               PAGECACHE_BLOCK_LINK **cache,
                               PAGECACHE_BLOCK_LINK **end,
                               enum flush_type type,
                               int *first_errno)
{
  int rc= PCFLUSH_OK;
  my_bool error;
  uint count= (uint) (end-cache);
  DBUG_ENTER("flush_cached_blocks");
  *first_errno= 0;

  /* Don't lock the cache during the flush */
  pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
  /*
     As all blocks referred in 'cache' are marked by PCBLOCK_IN_FLUSH
     we are guaranteed that no thread will change them
  */
  qsort((uchar*) cache, count, sizeof(*cache), (qsort_cmp) cmp_sec_link);

  pagecache_pthread_mutex_lock(&pagecache->cache_lock);
  for (; cache != end; cache++)
  {
    PAGECACHE_BLOCK_LINK *block= *cache;

    /*
      In the case of non_transactional tables we want to flush also
      block pinned with reads. This is becasue we may have other
      threads reading the block during flush, as non transactional
      tables can have many readers while the one writer is doing the
      flush.
      We don't want to do flush pinned blocks during checkpoint.
      We detect the checkpoint case by checking if type is LAZY.
    */
    if ((type == FLUSH_KEEP_LAZY && block->pins) || block->wlocks)
    {
      KEYCACHE_DBUG_PRINT("flush_cached_blocks",
                          ("block: %u (0x%lx)  pinned",
                           PCBLOCK_NUMBER(pagecache, block), (ulong)block));
      DBUG_PRINT("info", ("block: %u (0x%lx)  pinned",
                          PCBLOCK_NUMBER(pagecache, block), (ulong)block));
      PCBLOCK_INFO(block);
      /* undo the mark put by flush_pagecache_blocks_int(): */
      block->status&= ~PCBLOCK_IN_FLUSH;
      rc|= PCFLUSH_PINNED;
      DBUG_PRINT("warning", ("Page pinned"));
      unreg_request(pagecache, block, 1);
      if (!*first_errno)
        *first_errno= HA_ERR_INTERNAL_ERROR;
      continue;
    }
    if (make_lock_and_pin(pagecache, block,
                          PAGECACHE_LOCK_READ, PAGECACHE_PIN, FALSE))
      DBUG_ASSERT(0);

    KEYCACHE_DBUG_PRINT("flush_cached_blocks",
                        ("block: %u (0x%lx)  to be flushed",
                         PCBLOCK_NUMBER(pagecache, block), (ulong)block));
    DBUG_PRINT("info", ("block: %u (0x%lx)  to be flushed",
                        PCBLOCK_NUMBER(pagecache, block), (ulong)block));
    PCBLOCK_INFO(block);
    DBUG_PRINT("info", ("block: %u (0x%lx)  pins: %u",
                        PCBLOCK_NUMBER(pagecache, block), (ulong)block,
                        block->pins));
    /**
       @todo IO If page is contiguous with next page to flush, group flushes
       in one single my_pwrite().
    */
    /**
      It is important to use block->hash_link->file below and not 'file', as
      the first one is right and the second may have different out-of-date
      content (see StaleFilePointersInFlush in ma_checkpoint.c).
      @todo change argument of functions to be File.
    */
    pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
    error= pagecache_fwrite(pagecache, &block->hash_link->file,
                            block->buffer,
                            block->hash_link->pageno,
                            block->type,
                            pagecache->readwrite_flags);
    pagecache_pthread_mutex_lock(&pagecache->cache_lock);

    if (make_lock_and_pin(pagecache, block,
                          PAGECACHE_LOCK_READ_UNLOCK,
                          PAGECACHE_UNPIN, FALSE))
      DBUG_ASSERT(0);

    pagecache->global_cache_write++;
    if (error)
    {
      block->status|= PCBLOCK_ERROR;
      block->error=   (int16) my_errno;
      my_debug_put_break_here();
      if (!*first_errno)
        *first_errno= my_errno ? my_errno : -1;
      rc|= PCFLUSH_ERROR;
    }
#ifdef THREAD
    /*
      Let to proceed for possible waiting requests to write to the block page.
      It might happen only during an operation to resize the key cache.
    */
    if (block->wqueue[COND_FOR_SAVED].last_thread)
      wqueue_release_queue(&block->wqueue[COND_FOR_SAVED]);
#endif
    /* type will never be FLUSH_IGNORE_CHANGED here */
    if (! (type == FLUSH_KEEP || type == FLUSH_KEEP_LAZY ||
           type == FLUSH_FORCE_WRITE))
    {
      pagecache->blocks_changed--;
      pagecache->global_blocks_changed--;
      free_block(pagecache, block);
    }
    else
    {
      block->status&= ~PCBLOCK_IN_FLUSH;
      link_to_file_list(pagecache, block, file, 1);
      unreg_request(pagecache, block, 1);
    }
  }
  DBUG_RETURN(rc);
}


/**
   @brief flush all blocks for a file to disk but don't do any mutex locks

   @param  pagecache       pointer to a pagecache data structure
   @param  file            handler for the file to flush to
   @param  flush_type      type of the flush
   @param  filter          optional function which tells what blocks to flush;
                           can be non-NULL only if FLUSH_KEEP, FLUSH_KEEP_LAZY
                           or FLUSH_FORCE_WRITE.
   @param  filter_arg      an argument to pass to 'filter'. Information about
                           the block will be passed too.

   @note
     Flushes all blocks having the same OS file descriptor as 'file->file', so
     can flush blocks having '*block->hash_link->file' != '*file'.

   @note
     This function doesn't do any mutex locks because it needs to be called
     both from flush_pagecache_blocks and flush_all_key_blocks (the later one
     does the mutex lock in the resize_pagecache() function).

   @note
     This function can cause problems if two threads call it
     concurrently on the same file (look for "PageCacheFlushConcurrencyBugs"
     in ma_checkpoint.c); to avoid them, it has internal logic to serialize in
     this situation.

   @return Operation status
   @retval PCFLUSH_OK OK
   @retval PCFLUSH_ERROR There was errors during the flush process.
   @retval PCFLUSH_PINNED Pinned blocks was met and skipped.
   @retval PCFLUSH_PINNED_AND_ERROR PCFLUSH_ERROR and PCFLUSH_PINNED.
*/

static int flush_pagecache_blocks_int(PAGECACHE *pagecache,
                                      PAGECACHE_FILE *file,
                                      enum flush_type type,
                                      PAGECACHE_FLUSH_FILTER filter,
                                      void *filter_arg)
{
  PAGECACHE_BLOCK_LINK *cache_buff[FLUSH_CACHE],**cache;
  int last_errno= 0;
  int rc= PCFLUSH_OK;
  DBUG_ENTER("flush_pagecache_blocks_int");
  DBUG_PRINT("enter",
             ("fd: %d  blocks_used: %lu  blocks_changed: %lu  type: %d",
              file->file, pagecache->blocks_used, pagecache->blocks_changed,
              type));

#if !defined(DBUG_OFF) && defined(EXTRA_DEBUG)
    DBUG_EXECUTE("check_pagecache",
                 test_key_cache(pagecache,
                                "start of flush_pagecache_blocks", 0););
#endif

  cache= cache_buff;
  if (pagecache->disk_blocks > 0 &&
      (!my_disable_flush_pagecache_blocks ||
       (type != FLUSH_KEEP && type != FLUSH_KEEP_LAZY)))
  {
    /*
      Key cache exists. If my_disable_flush_pagecache_blocks is true it
      disables the operation but only FLUSH_KEEP[_LAZY]: other flushes still
      need to be allowed: FLUSH_RELEASE has to free blocks, and
      FLUSH_FORCE_WRITE is to overrule my_disable_flush_pagecache_blocks.
    */
    int error= 0;
    uint count= 0;
    PAGECACHE_BLOCK_LINK **pos, **end;
    PAGECACHE_BLOCK_LINK *first_in_switch= NULL;
    PAGECACHE_BLOCK_LINK *block, *next;
#if defined(PAGECACHE_DEBUG)
    uint cnt= 0;
#endif

#ifdef THREAD
    struct st_file_in_flush us_flusher, *other_flusher;
    us_flusher.file= file->file;
    us_flusher.flush_queue.last_thread= NULL;
    us_flusher.first_in_switch= FALSE;
    while ((other_flusher= (struct st_file_in_flush *)
            hash_search(&pagecache->files_in_flush, (uchar *)&file->file,
                        sizeof(file->file))))
    {
      /*
        File is in flush already: wait, unless FLUSH_KEEP_LAZY. "Flusher"
        means "who can mark PCBLOCK_IN_FLUSH", i.e. caller of
        flush_pagecache_blocks_int().
      */
      struct st_my_thread_var *thread;
      if (type == FLUSH_KEEP_LAZY)
      {
        DBUG_PRINT("info",("FLUSH_KEEP_LAZY skips"));
        DBUG_RETURN(0);
      }
      thread= my_thread_var;
      wqueue_add_to_queue(&other_flusher->flush_queue, thread);
      do
      {
        KEYCACHE_DBUG_PRINT("flush_pagecache_blocks_int: wait1",
                            ("suspend thread %ld", thread->id));
        pagecache_pthread_cond_wait(&thread->suspend,
                                    &pagecache->cache_lock);
      }
      while (thread->next);
    }
    /* we are the only flusher of this file now */
    while (my_hash_insert(&pagecache->files_in_flush, (uchar *)&us_flusher))
    {
      /*
        Out of memory, wait for flushers to empty the hash and retry; should
        rarely happen. Other threads are flushing the file; when done, they
        are going to remove themselves from the hash, and thus memory will
        appear again. However, this memory may be stolen by yet another thread
        (for a purpose unrelated to page cache), before we retry
        hash_insert(). So the loop may run for long. Only if the thread was
        killed do we abort the loop, returning 1 (error) which can cause the
        table to be marked as corrupted (cf maria_chk_size(), maria_close())
        and thus require a table check.
      */
      DBUG_ASSERT(0);
      pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
      if (my_thread_var->abort)
        DBUG_RETURN(1);		/* End if aborted by user */
      sleep(10);
      pagecache_pthread_mutex_lock(&pagecache->cache_lock);
    }
#endif

    if (type != FLUSH_IGNORE_CHANGED)
    {
      /*
        Count how many key blocks we have to cache to be able
        to flush all dirty pages with minimum seek moves.
      */
      for (block= pagecache->changed_blocks[FILE_HASH(*file)] ;
           block;
           block= block->next_changed)
      {
        if (block->hash_link->file.file == file->file)
        {
          count++;
          KEYCACHE_DBUG_ASSERT(count<= pagecache->blocks_used);
        }
      }
      count++;    /* Allocate one extra for easy end-of-buffer test */
      /* Allocate a new buffer only if its bigger than the one we have */
      if (count > FLUSH_CACHE &&
          !(cache=
            (PAGECACHE_BLOCK_LINK**)
            my_malloc(sizeof(PAGECACHE_BLOCK_LINK*)*count, MYF(0))))
      {
        cache= cache_buff;
        count= FLUSH_CACHE;
      }
    }

    /* Retrieve the blocks and write them to a buffer to be flushed */
restart:
    end= (pos= cache)+count;
    for (block= pagecache->changed_blocks[FILE_HASH(*file)] ;
         block;
         block= next)
    {
#if defined(PAGECACHE_DEBUG)
      cnt++;
      KEYCACHE_DBUG_ASSERT(cnt <= pagecache->blocks_used);
#endif
      next= block->next_changed;
      if (block->hash_link->file.file != file->file)
        continue;
      if (filter != NULL)
      {
        int filter_res= (*filter)(block->type, block->hash_link->pageno,
                                  block->rec_lsn, filter_arg);
        DBUG_PRINT("info",("filter returned %d", filter_res));
        if (filter_res == FLUSH_FILTER_SKIP_TRY_NEXT)
          continue;
        if (filter_res == FLUSH_FILTER_SKIP_ALL)
          break;
        DBUG_ASSERT(filter_res == FLUSH_FILTER_OK);
      }
      {
        DBUG_ASSERT(!(block->status & PCBLOCK_IN_FLUSH));
        /*
          We care only for the blocks for which flushing was not
          initiated by other threads as a result of page swapping
        */
        if (! (block->status & PCBLOCK_IN_SWITCH))
        {
          /*
            Mark the block with BLOCK_IN_FLUSH in order not to let
            other threads to use it for new pages and interfere with
            our sequence of flushing dirty file pages
          */
          block->status|= PCBLOCK_IN_FLUSH;

          reg_requests(pagecache, block, 1);
          if (type != FLUSH_IGNORE_CHANGED)
          {
            *pos++= block;
	    /* It's not a temporary file */
            if (pos == end)
            {
	      /*
		This happens only if there is not enough
		memory for the big block
              */
              if ((rc|= flush_cached_blocks(pagecache, file, cache,
                                            end, type, &error)) &
                  (PCFLUSH_ERROR | PCFLUSH_PINNED))
                last_errno=error;
              DBUG_PRINT("info", ("restarting..."));
              /*
		Restart the scan as some other thread might have changed
		the changed blocks chain: the blocks that were in switch
		state before the flush started have to be excluded
              */
              goto restart;
            }
          }
          else
          {
            /* It's a temporary file */
            pagecache->blocks_changed--;
	    pagecache->global_blocks_changed--;
            free_block(pagecache, block);
          }
        }
        else if (type != FLUSH_KEEP_LAZY)
        {
          /*
            Link the block into a list of blocks 'in switch', and then we will
            wait for this list to be empty, which means they have been flushed
          */
          unlink_changed(block);
          link_changed(block, &first_in_switch);
          us_flusher.first_in_switch= TRUE;
        }
      }
    }
    if (pos != cache)
    {
      if ((rc|= flush_cached_blocks(pagecache, file, cache, pos, type,
                                    &error)) &
          (PCFLUSH_ERROR | PCFLUSH_PINNED))
        last_errno= error;
    }
    /* Wait until list of blocks in switch is empty */
    while (first_in_switch)
    {
#if defined(PAGECACHE_DEBUG)
      cnt= 0;
#endif
      block= first_in_switch;
      {
#ifdef THREAD
        struct st_my_thread_var *thread= my_thread_var;
        wqueue_add_to_queue(&block->wqueue[COND_FOR_SAVED], thread);
        do
        {
          KEYCACHE_DBUG_PRINT("flush_pagecache_blocks_int: wait2",
                              ("suspend thread %ld", thread->id));
          pagecache_pthread_cond_wait(&thread->suspend,
                                     &pagecache->cache_lock);
        }
        while (thread->next);
#else
        KEYCACHE_DBUG_ASSERT(0);
        /* No parallel requests in single-threaded case */
#endif
      }
#if defined(PAGECACHE_DEBUG)
      cnt++;
      KEYCACHE_DBUG_ASSERT(cnt <= pagecache->blocks_used);
#endif
    }
    us_flusher.first_in_switch= FALSE;
    /* The following happens very seldom */
    if (! (type == FLUSH_KEEP || type == FLUSH_KEEP_LAZY ||
           type == FLUSH_FORCE_WRITE))
    {
      /*
        this code would free all blocks while filter maybe handled only a
        few, that is not possible.
      */
      DBUG_ASSERT(filter == NULL);
#if defined(PAGECACHE_DEBUG)
      cnt=0;
#endif
      for (block= pagecache->file_blocks[FILE_HASH(*file)] ;
           block;
           block= next)
      {
#if defined(PAGECACHE_DEBUG)
        cnt++;
        KEYCACHE_DBUG_ASSERT(cnt <= pagecache->blocks_used);
#endif
        next= block->next_changed;
        if (block->hash_link->file.file == file->file &&
            (! (block->status & PCBLOCK_CHANGED)
             || type == FLUSH_IGNORE_CHANGED))
        {
          reg_requests(pagecache, block, 1);
          free_block(pagecache, block);
        }
      }
    }
#ifdef THREAD
    /* wake up others waiting to flush this file */
    hash_delete(&pagecache->files_in_flush, (uchar *)&us_flusher);
    if (us_flusher.flush_queue.last_thread)
      wqueue_release_queue(&us_flusher.flush_queue);
#endif
  }

#ifndef DBUG_OFF
  DBUG_EXECUTE("check_pagecache",
               test_key_cache(pagecache, "end of flush_pagecache_blocks", 0););
#endif
  if (cache != cache_buff)
    my_free(cache, MYF(0));
  if (rc != 0)
  {
    if (last_errno)
      my_errno= last_errno;                /* Return first error */
    DBUG_PRINT("error", ("Got error: %d", my_errno));
  }
  DBUG_RETURN(rc);
}


/**
   @brief flush all blocks for a file to disk

   @param  pagecache       pointer to a pagecache data structure
   @param  file            handler for the file to flush to
   @param  flush_type      type of the flush
   @param  filter          optional function which tells what blocks to flush;
                           can be non-NULL only if FLUSH_KEEP, FLUSH_KEEP_LAZY
                           or FLUSH_FORCE_WRITE.
   @param  filter_arg      an argument to pass to 'filter'. Information about
                           the block will be passed too.

   @return Operation status
   @retval PCFLUSH_OK OK
   @retval PCFLUSH_ERROR There was errors during the flush process.
   @retval PCFLUSH_PINNED Pinned blocks was met and skipped.
   @retval PCFLUSH_PINNED_AND_ERROR PCFLUSH_ERROR and PCFLUSH_PINNED.
*/

int flush_pagecache_blocks_with_filter(PAGECACHE *pagecache,
                                       PAGECACHE_FILE *file,
                                       enum flush_type type,
                                       PAGECACHE_FLUSH_FILTER filter,
                                       void *filter_arg)
{
  int res;
  DBUG_ENTER("flush_pagecache_blocks_with_filter");
  DBUG_PRINT("enter", ("pagecache: 0x%lx", (long) pagecache));

  if (pagecache->disk_blocks <= 0)
    DBUG_RETURN(0);
  pagecache_pthread_mutex_lock(&pagecache->cache_lock);
  inc_counter_for_resize_op(pagecache);
  res= flush_pagecache_blocks_int(pagecache, file, type, filter, filter_arg);
  dec_counter_for_resize_op(pagecache);
  pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
  DBUG_RETURN(res);
}


/*
  Reset the counters of a key cache.

  SYNOPSIS
    reset_pagecache_counters()
    name       the name of a key cache
    pagecache  pointer to the pagecache to be reset

  DESCRIPTION
    This procedure is used to reset the counters of all currently used key
    caches, both the default one and the named ones.

  RETURN
    0 on success (always because it can't fail)
*/

int reset_pagecache_counters(const char *name __attribute__((unused)),
                             PAGECACHE *pagecache)
{
  DBUG_ENTER("reset_pagecache_counters");
  if (!pagecache->inited)
  {
    DBUG_PRINT("info", ("Key cache %s not initialized.", name));
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("Resetting counters for key cache %s.", name));

  pagecache->global_blocks_changed= 0;   /* Key_blocks_not_flushed */
  pagecache->global_cache_r_requests= 0; /* Key_read_requests */
  pagecache->global_cache_read= 0;       /* Key_reads */
  pagecache->global_cache_w_requests= 0; /* Key_write_requests */
  pagecache->global_cache_write= 0;      /* Key_writes */
  DBUG_RETURN(0);
}


/**
   @brief Allocates a buffer and stores in it some info about all dirty pages

   Does the allocation because the caller cannot know the size itself.
   Memory freeing is to be done by the caller (if the "str" member of the
   LEX_STRING is not NULL).
   Ignores all pages of another type than PAGECACHE_LSN_PAGE, because they
   are not interesting for a checkpoint record.
   The caller has the intention of doing checkpoints.

   @param       pagecache   pointer to the page cache
   @param[out]  str         pointer to where the allocated buffer, and
                            its size, will be put
   @param[out]  min_rec_lsn pointer to where the minimum rec_lsn of all
                            relevant dirty pages will be put
   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

my_bool pagecache_collect_changed_blocks_with_lsn(PAGECACHE *pagecache,
                                                  LEX_STRING *str,
                                                  LSN *min_rec_lsn)
{
  my_bool error= 0;
  ulong stored_list_size= 0;
  uint file_hash;
  char *ptr;
  LSN minimum_rec_lsn= LSN_MAX;
  DBUG_ENTER("pagecache_collect_changed_blocks_with_LSN");

  DBUG_ASSERT(NULL == str->str);
  /*
    We lock the entire cache but will be quick, just reading/writing a few MBs
    of memory at most.
  */
  pagecache_pthread_mutex_lock(&pagecache->cache_lock);
#ifdef THREAD
  for (;;)
  {
    struct st_file_in_flush *other_flusher;
    for (file_hash= 0;
         (other_flusher= (struct st_file_in_flush *)
          hash_element(&pagecache->files_in_flush, file_hash)) != NULL &&
           !other_flusher->first_in_switch;
         file_hash++)
    {}
    if (other_flusher == NULL)
      break;
    /*
      other_flusher.first_in_switch is true: some thread is flushing a file
      and has removed dirty blocks from changed_blocks[] while they were still
      dirty (they were being evicted (=>flushed) by yet another thread, which
      may not have flushed the block yet so it may still be dirty).
      If Checkpoint proceeds now, it will not see the page. If there is a
      crash right after writing the checkpoint record, before the page is
      flushed, at recovery the page will be wrongly ignored because it won't
      be in the dirty pages list in the checkpoint record. So wait.
    */
    {
      struct st_my_thread_var *thread= my_thread_var;
      wqueue_add_to_queue(&other_flusher->flush_queue, thread);
      do
      {
        KEYCACHE_DBUG_PRINT("pagecache_collect_changed_blocks_with_lsn: wait",
                            ("suspend thread %ld", thread->id));
        pagecache_pthread_cond_wait(&thread->suspend,
                                    &pagecache->cache_lock);
      }
      while (thread->next);
    }
  }
#endif

  /* Count how many dirty pages are interesting */
  for (file_hash= 0; file_hash < PAGECACHE_CHANGED_BLOCKS_HASH; file_hash++)
  {
    PAGECACHE_BLOCK_LINK *block;
    for (block= pagecache->changed_blocks[file_hash] ;
         block;
         block= block->next_changed)
    {
      /*
        Q: is there something subtle with block->hash_link: can it be NULL?
        does it have to be == hash_link->block... ?
      */
      DBUG_ASSERT(block->hash_link != NULL);
      DBUG_ASSERT(block->status & PCBLOCK_CHANGED);
      /*
        Note that we don't store bitmap pages, or pages from non-transactional
        (like temporary) tables. Don't checkpoint during Recovery which uses
        PAGECACHE_PLAIN_PAGE.
      */
      if (block->type != PAGECACHE_LSN_PAGE)
        continue; /* no need to store it */
      stored_list_size++;
    }
  }

  compile_time_assert(sizeof(pagecache->blocks) <= 8);
  str->length= 8 + /* number of dirty pages */
    (2 + /* table id */
     1 + /* data or index file */
     5 + /* pageno */
     LSN_STORE_SIZE /* rec_lsn */
     ) * stored_list_size;
  if (NULL == (str->str= my_malloc(str->length, MYF(MY_WME))))
    goto err;
  ptr= str->str;
  int8store(ptr, (ulonglong)stored_list_size);
  ptr+= 8;
  DBUG_PRINT("info", ("found %lu dirty pages", stored_list_size));
  if (stored_list_size == 0)
    goto end;
  for (file_hash= 0; file_hash < PAGECACHE_CHANGED_BLOCKS_HASH; file_hash++)
  {
    PAGECACHE_BLOCK_LINK *block;
    for (block= pagecache->changed_blocks[file_hash] ;
         block;
         block= block->next_changed)
    {
      uint16 table_id;
      MARIA_SHARE *share;
      if (block->type != PAGECACHE_LSN_PAGE)
        continue; /* no need to store it in the checkpoint record */
      share= (MARIA_SHARE *)(block->hash_link->file.callback_data);
      table_id= share->id;
      int2store(ptr, table_id);
      ptr+= 2;
      ptr[0]= (share->kfile.file == block->hash_link->file.file);
      ptr++;
      DBUG_ASSERT(block->hash_link->pageno < ((ULL(1)) << 40));
      page_store(ptr, block->hash_link->pageno);
      ptr+= PAGE_STORE_SIZE;
      lsn_store(ptr, block->rec_lsn);
      ptr+= LSN_STORE_SIZE;
      if (block->rec_lsn != LSN_MAX)
      {
        DBUG_ASSERT(LSN_VALID(block->rec_lsn));
        if (cmp_translog_addr(block->rec_lsn, minimum_rec_lsn) < 0)
          minimum_rec_lsn= block->rec_lsn;
      } /* otherwise, some trn->rec_lsn should hold the correct info */
    }
  }
end:
  pagecache_pthread_mutex_unlock(&pagecache->cache_lock);
  *min_rec_lsn= minimum_rec_lsn;
  DBUG_RETURN(error);

err:
  error= 1;
  goto end;
}


#ifndef DBUG_OFF

/**
  Verifies that a file has no dirty pages.
*/

void pagecache_file_no_dirty_page(PAGECACHE *pagecache, PAGECACHE_FILE *file)
{
  File fd= file->file;
  PAGECACHE_BLOCK_LINK *block;
  for (block= pagecache->changed_blocks[FILE_HASH(*file)];
       block != NULL;
       block= block->next_changed)
    if (block->hash_link->file.file == fd)
    {
      DBUG_PRINT("info", ("pagecache_file_not_in error"));
      PCBLOCK_INFO(block);
      DBUG_ASSERT(0);
    }
}


/*
  Test if disk-cache is ok
*/
static void test_key_cache(PAGECACHE *pagecache __attribute__((unused)),
                           const char *where __attribute__((unused)),
                           my_bool lock __attribute__((unused)))
{
  /* TODO */
}
#endif

uchar *pagecache_block_link_to_buffer(PAGECACHE_BLOCK_LINK *block)
{
  return block->buffer;
}

#if defined(PAGECACHE_TIMEOUT)

#define KEYCACHE_DUMP_FILE  "pagecache_dump.txt"
#define MAX_QUEUE_LEN  100


static void pagecache_dump(PAGECACHE *pagecache)
{
  FILE *pagecache_dump_file=fopen(KEYCACHE_DUMP_FILE, "w");
  struct st_my_thread_var *last;
  struct st_my_thread_var *thread;
  PAGECACHE_BLOCK_LINK *block;
  PAGECACHE_HASH_LINK *hash_link;
  PAGECACHE_PAGE *page;
  uint i;

  fprintf(pagecache_dump_file, "thread:%u\n", thread->id);

  i=0;
  thread=last=waiting_for_hash_link.last_thread;
  fprintf(pagecache_dump_file, "queue of threads waiting for hash link\n");
  if (thread)
    do
    {
      thread= thread->next;
      page= (PAGECACHE_PAGE *) thread->opt_info;
      fprintf(pagecache_dump_file,
              "thread:%u, (file,pageno)=(%u,%lu)\n",
              thread->id,(uint) page->file.file,(ulong) page->pageno);
      if (++i == MAX_QUEUE_LEN)
        break;
    }
    while (thread != last);

  i=0;
  thread=last=waiting_for_block.last_thread;
  fprintf(pagecache_dump_file, "queue of threads waiting for block\n");
  if (thread)
    do
    {
      thread=thread->next;
      hash_link= (PAGECACHE_HASH_LINK *) thread->opt_info;
      fprintf(pagecache_dump_file,
        "thread:%u hash_link:%u (file,pageno)=(%u,%lu)\n",
        thread->id, (uint) PAGECACHE_HASH_LINK_NUMBER(pagecache, hash_link),
        (uint) hash_link->file.file,(ulong) hash_link->pageno);
      if (++i == MAX_QUEUE_LEN)
        break;
    }
    while (thread != last);

  for (i=0 ; i < pagecache->blocks_used ; i++)
  {
    int j;
    block= &pagecache->block_root[i];
    hash_link= block->hash_link;
    fprintf(pagecache_dump_file,
            "block:%u hash_link:%d status:%x #requests=%u waiting_for_readers:%d\n",
            i, (int) (hash_link ?
                      PAGECACHE_HASH_LINK_NUMBER(pagecache, hash_link) :
                      -1),
            block->status, block->requests, block->condvar ? 1 : 0);
    for (j=0 ; j < COND_SIZE; j++)
    {
      PAGECACHE_WQUEUE *wqueue=&block->wqueue[j];
      thread= last= wqueue->last_thread;
      fprintf(pagecache_dump_file, "queue #%d\n", j);
      if (thread)
      {
        do
        {
          thread=thread->next;
          fprintf(pagecache_dump_file,
                  "thread:%u\n", thread->id);
          if (++i == MAX_QUEUE_LEN)
            break;
        }
        while (thread != last);
      }
    }
  }
  fprintf(pagecache_dump_file, "LRU chain:");
  block= pagecache= used_last;
  if (block)
  {
    do
    {
      block= block->next_used;
      fprintf(pagecache_dump_file,
              "block:%u, ", PCBLOCK_NUMBER(pagecache, block));
    }
    while (block != pagecache->used_last);
  }
  fprintf(pagecache_dump_file, "\n");

  fclose(pagecache_dump_file);
}

#endif /* defined(PAGECACHE_TIMEOUT) */

#if defined(PAGECACHE_TIMEOUT) && !defined(__WIN__)


static int pagecache_pthread_cond_wait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex)
{
  int rc;
  struct timeval  now;            /* time when we started waiting        */
  struct timespec timeout;        /* timeout value for the wait function */
  struct timezone tz;
#if defined(PAGECACHE_DEBUG)
  int cnt=0;
#endif

  /* Get current time */
  gettimeofday(&now, &tz);
  /* Prepare timeout value */
  timeout.tv_sec= now.tv_sec + PAGECACHE_TIMEOUT;
 /*
   timeval uses microseconds.
   timespec uses nanoseconds.
   1 nanosecond = 1000 micro seconds
 */
  timeout.tv_nsec= now.tv_usec * 1000;
  KEYCACHE_THREAD_TRACE_END("started waiting");
#if defined(PAGECACHE_DEBUG)
  cnt++;
  if (cnt % 100 == 0)
    fprintf(pagecache_debug_log, "waiting...\n");
    fflush(pagecache_debug_log);
#endif
  rc= pthread_cond_timedwait(cond, mutex, &timeout);
  KEYCACHE_THREAD_TRACE_BEGIN("finished waiting");
  if (rc == ETIMEDOUT || rc == ETIME)
  {
#if defined(PAGECACHE_DEBUG)
    fprintf(pagecache_debug_log,"aborted by pagecache timeout\n");
    fclose(pagecache_debug_log);
    abort();
#endif
    pagecache_dump();
  }

#if defined(PAGECACHE_DEBUG)
  KEYCACHE_DBUG_ASSERT(rc != ETIMEDOUT);
#else
  assert(rc != ETIMEDOUT);
#endif
  return rc;
}
#else
#if defined(PAGECACHE_DEBUG)
static int pagecache_pthread_cond_wait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex)
{
  int rc;
  KEYCACHE_THREAD_TRACE_END("started waiting");
  rc= pthread_cond_wait(cond, mutex);
  KEYCACHE_THREAD_TRACE_BEGIN("finished waiting");
  return rc;
}
#endif
#endif /* defined(PAGECACHE_TIMEOUT) && !defined(__WIN__) */

#if defined(PAGECACHE_DEBUG)
static int ___pagecache_pthread_mutex_lock(pthread_mutex_t *mutex)
{
  int rc;
  rc= pthread_mutex_lock(mutex);
  KEYCACHE_THREAD_TRACE_BEGIN("");
  return rc;
}


static void ___pagecache_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  KEYCACHE_THREAD_TRACE_END("");
  pthread_mutex_unlock(mutex);
}


static int ___pagecache_pthread_cond_signal(pthread_cond_t *cond)
{
  int rc;
  KEYCACHE_THREAD_TRACE("signal");
  rc= pthread_cond_signal(cond);
  return rc;
}


#if defined(PAGECACHE_DEBUG_LOG)


static void pagecache_debug_print(const char * fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  if (pagecache_debug_log)
  {
    VOID(vfprintf(pagecache_debug_log, fmt, args));
    VOID(fputc('\n',pagecache_debug_log));
  }
  va_end(args);
}
#endif /* defined(PAGECACHE_DEBUG_LOG) */

#if defined(PAGECACHE_DEBUG_LOG)


void pagecache_debug_log_close(void)
{
  if (pagecache_debug_log)
    fclose(pagecache_debug_log);
}
#endif /* defined(PAGECACHE_DEBUG_LOG) */

#endif /* defined(PAGECACHE_DEBUG) */
