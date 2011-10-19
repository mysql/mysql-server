/* Copyright (C) 2006 MySQL AB

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

/* Page cache variable structures */

#ifndef _ma_pagecache_h
#define _ma_pagecache_h
C_MODE_START

#include "ma_loghandler_lsn.h"
#include <m_string.h>
#include <hash.h>

/* Type of the page */
enum pagecache_page_type
{
  /*
    Used only for control page type changing during debugging. This define
    should only be using when using DBUG.
  */
  PAGECACHE_EMPTY_PAGE,
  /* the page does not contain LSN */
  PAGECACHE_PLAIN_PAGE,
  /* the page contain LSN (maria tablespace page) */
  PAGECACHE_LSN_PAGE,
  /* Page type used when scanning file and we don't care about the type */
  PAGECACHE_READ_UNKNOWN_PAGE
};

/*
  This enum describe lock status changing. every type of page cache will
  interpret WRITE/READ lock as it need.
*/
enum pagecache_page_lock
{
  PAGECACHE_LOCK_LEFT_UNLOCKED,       /* free  -> free  */
  PAGECACHE_LOCK_LEFT_READLOCKED,     /* read  -> read  */
  PAGECACHE_LOCK_LEFT_WRITELOCKED,    /* write -> write */
  PAGECACHE_LOCK_READ,                /* free  -> read  */
  PAGECACHE_LOCK_WRITE,               /* free  -> write */
  PAGECACHE_LOCK_READ_UNLOCK,         /* read  -> free  */
  PAGECACHE_LOCK_WRITE_UNLOCK,        /* write -> free  */
  PAGECACHE_LOCK_WRITE_TO_READ        /* write -> read  */
};
/*
  This enum describe pin status changing
*/
enum pagecache_page_pin
{
  PAGECACHE_PIN_LEFT_PINNED,   /* pinned   -> pinned   */
  PAGECACHE_PIN_LEFT_UNPINNED, /* unpinned -> unpinned */
  PAGECACHE_PIN,               /* unpinned -> pinned   */
  PAGECACHE_UNPIN              /* pinned   -> unpinned */
};
/* How to write the page */
enum pagecache_write_mode
{
  /* do not write immediately, i.e. it will be dirty page */
  PAGECACHE_WRITE_DELAY,
  /* page already is in the file. (key cache insert analogue) */
  PAGECACHE_WRITE_DONE
};

/* page number for maria */
typedef ulonglong pgcache_page_no_t;

/* file descriptor for Maria */
typedef struct st_pagecache_file
{
  File file;
  /** Cannot be NULL */
  my_bool (*read_callback)(uchar *page, pgcache_page_no_t offset,
                           uchar *data);
  /** Cannot be NULL */
  my_bool (*write_callback)(uchar *page, pgcache_page_no_t offset,
                            uchar *data);
  void (*write_fail)(uchar *data);
  /** Cannot be NULL */
  my_bool (*flush_log_callback)(uchar *page, pgcache_page_no_t offset,
                                uchar *data);
  uchar *callback_data;
} PAGECACHE_FILE;

/* declare structures that is used by  st_pagecache */

struct st_pagecache_block_link;
typedef struct st_pagecache_block_link PAGECACHE_BLOCK_LINK;
struct st_pagecache_page;
typedef struct st_pagecache_page PAGECACHE_PAGE;
struct st_pagecache_hash_link;
typedef struct st_pagecache_hash_link PAGECACHE_HASH_LINK;

#include <wqueue.h>

#define PAGECACHE_CHANGED_BLOCKS_HASH 128  /* must be power of 2 */
#define PAGECACHE_PRIORITY_LOW 0
#define PAGECACHE_PRIORITY_DEFAULT 3
#define PAGECACHE_PRIORITY_HIGH 6

/*
  The page cache structure
  It also contains read-only statistics parameters.
*/

typedef struct st_pagecache
{
  size_t mem_size;               /* specified size of the cache memory       */
  ulong min_warm_blocks;         /* min number of warm blocks;               */
  ulong age_threshold;           /* age threshold for hot blocks             */
  ulonglong time;                /* total number of block link operations    */
  ulong hash_entries;            /* max number of entries in the hash table  */
  long hash_links;               /* max number of hash links                 */
  long hash_links_used;   /* number of hash links taken from free links pool */
  long disk_blocks;              /* max number of blocks in the cache        */
  ulong blocks_used;           /* maximum number of concurrently used blocks */
  ulong blocks_unused;           /* number of currently unused blocks        */
  ulong blocks_changed;          /* number of currently dirty blocks         */
  ulong warm_blocks;             /* number of blocks in warm sub-chain       */
  ulong cnt_for_resize_op;       /* counter to block resize operation        */
  ulong blocks_available;     /* number of blocks available in the LRU chain */
  long blocks;                   /* max number of blocks in the cache        */
  uint32 block_size;             /* size of the page buffer of a cache block */
  PAGECACHE_HASH_LINK **hash_root;/* arr. of entries into hash table buckets */
  PAGECACHE_HASH_LINK *hash_link_root;/* memory for hash table links         */
  PAGECACHE_HASH_LINK *free_hash_list;/* list of free hash links             */
  PAGECACHE_BLOCK_LINK *free_block_list;/* list of free blocks               */
  PAGECACHE_BLOCK_LINK *block_root;/* memory for block links                 */
  uchar *block_mem;              /* memory for block buffers                 */
  PAGECACHE_BLOCK_LINK *used_last;/* ptr to the last block of the LRU chain  */
  PAGECACHE_BLOCK_LINK *used_ins;/* ptr to the insertion block in LRU chain  */
  mysql_mutex_t cache_lock;      /* to lock access to the cache structure    */
  WQUEUE resize_queue; /* threads waiting during resize operation  */
  WQUEUE waiting_for_hash_link;/* waiting for a free hash link     */
  WQUEUE waiting_for_block;   /* requests waiting for a free block */
  /* hash for dirty file bl.*/
  PAGECACHE_BLOCK_LINK *changed_blocks[PAGECACHE_CHANGED_BLOCKS_HASH];
  /* hash for other file bl.*/
  PAGECACHE_BLOCK_LINK *file_blocks[PAGECACHE_CHANGED_BLOCKS_HASH];

  /*
    The following variables are and variables used to hold parameters for
    initializing the key cache.
  */

  ulonglong param_buff_size;    /* size the memory allocated for the cache  */
  ulong param_block_size;       /* size of the blocks in the key cache      */
  ulong param_division_limit;   /* min. percentage of warm blocks           */
  ulong param_age_threshold;    /* determines when hot block is downgraded  */

  /* Statistics variables. These are reset in reset_pagecache_counters().    */
  ulong global_blocks_changed;	/* number of currently dirty blocks          */
  ulonglong global_cache_w_requests;/* number of write requests (write hits) */
  ulonglong global_cache_write;     /* number of writes from cache to files  */
  ulonglong global_cache_r_requests;/* number of read requests (read hits)   */
  ulonglong global_cache_read;      /* number of reads from files to cache   */

  uint shift;                       /* block size = 2 ^ shift                */
  myf  readwrite_flags;             /* Flags to pread/pwrite() */
  myf  org_readwrite_flags;         /* Flags to pread/pwrite() at init */
  my_bool inited;
  my_bool resize_in_flush;       /* true during flush of resize operation    */
  my_bool can_be_used;           /* usage of cache for read/write is allowed */
  my_bool in_init;		/* Set to 1 in MySQL during init/resize     */
  my_bool extra_debug;	        /* set to 1 if one wants extra logging */
  HASH    files_in_flush;       /**< files in flush_pagecache_blocks_int() */
} PAGECACHE;

/** @brief Return values for PAGECACHE_FLUSH_FILTER */
enum pagecache_flush_filter_result
{
  FLUSH_FILTER_SKIP_TRY_NEXT= 0,/**< skip page and move on to next one */
  FLUSH_FILTER_OK,              /**< flush page and move on to next one */
  FLUSH_FILTER_SKIP_ALL         /**< skip page and all next ones */
};
/** @brief a filter function type for flush_pagecache_blocks_with_filter() */
typedef enum pagecache_flush_filter_result
(*PAGECACHE_FLUSH_FILTER)(enum pagecache_page_type type,
                          pgcache_page_no_t page,
                          LSN rec_lsn, void *arg);

/* The default key cache */
extern PAGECACHE dflt_pagecache_var, *dflt_pagecache;

extern ulong init_pagecache(PAGECACHE *pagecache, size_t use_mem,
                            uint division_limit, uint age_threshold,
                            uint block_size, myf my_read_flags);
extern ulong resize_pagecache(PAGECACHE *pagecache,
                              size_t use_mem, uint division_limit,
                              uint age_threshold);
extern void change_pagecache_param(PAGECACHE *pagecache, uint division_limit,
                                   uint age_threshold);

extern uchar *pagecache_read(PAGECACHE *pagecache,
                             PAGECACHE_FILE *file,
                             pgcache_page_no_t pageno,
                             uint level,
                             uchar *buff,
                             enum pagecache_page_type type,
                             enum pagecache_page_lock lock,
                             PAGECACHE_BLOCK_LINK **link);

#define  pagecache_write(P,F,N,L,B,T,O,I,M,K,R) \
   pagecache_write_part(P,F,N,L,B,T,O,I,M,K,R,0,(P)->block_size)

#define  pagecache_inject(P,F,N,L,B,T,O,I,K,R) \
   pagecache_write_part(P,F,N,L,B,T,O,I,PAGECACHE_WRITE_DONE, \
                        K,R,0,(P)->block_size)

extern my_bool pagecache_write_part(PAGECACHE *pagecache,
                                    PAGECACHE_FILE *file,
                                    pgcache_page_no_t pageno,
                                    uint level,
                                    uchar *buff,
                                    enum pagecache_page_type type,
                                    enum pagecache_page_lock lock,
                                    enum pagecache_page_pin pin,
                                    enum pagecache_write_mode write_mode,
                                    PAGECACHE_BLOCK_LINK **link,
                                    LSN first_REDO_LSN_for_page,
                                    uint offset,
                                    uint size);
extern void pagecache_unlock(PAGECACHE *pagecache,
                             PAGECACHE_FILE *file,
                             pgcache_page_no_t pageno,
                             enum pagecache_page_lock lock,
                             enum pagecache_page_pin pin,
                             LSN first_REDO_LSN_for_page,
                             LSN lsn, my_bool was_changed);
extern void pagecache_unlock_by_link(PAGECACHE *pagecache,
                                     PAGECACHE_BLOCK_LINK *block,
                                     enum pagecache_page_lock lock,
                                     enum pagecache_page_pin pin,
                                     LSN first_REDO_LSN_for_page,
                                     LSN lsn, my_bool was_changed,
                                     my_bool any);
extern void pagecache_unpin(PAGECACHE *pagecache,
                            PAGECACHE_FILE *file,
                            pgcache_page_no_t pageno,
                            LSN lsn);
extern void pagecache_unpin_by_link(PAGECACHE *pagecache,
                                    PAGECACHE_BLOCK_LINK *link,
                                    LSN lsn);
extern void pagecache_set_write_on_delete_by_link(PAGECACHE_BLOCK_LINK *block);


/* Results of flush operation (bit field in fact) */

/* The flush is done. */
#define PCFLUSH_OK 0
/* There was errors during the flush process. */
#define PCFLUSH_ERROR 1
/* Pinned blocks was met and skipped. */
#define PCFLUSH_PINNED 2
/* PCFLUSH_ERROR and PCFLUSH_PINNED. */
#define PCFLUSH_PINNED_AND_ERROR (PCFLUSH_ERROR|PCFLUSH_PINNED)

#define pagecache_file_init(F,RC,WC,WF,GLC,D) \
  do{ \
    (F).read_callback= (RC); (F).write_callback= (WC); \
    (F).write_fail= (WF); \
    (F).flush_log_callback= (GLC); (F).callback_data= (uchar*)(D); \
  } while(0)

#define flush_pagecache_blocks(A,B,C)                   \
  flush_pagecache_blocks_with_filter(A,B,C,NULL,NULL)
extern int flush_pagecache_blocks_with_filter(PAGECACHE *keycache,
                                              PAGECACHE_FILE *file,
                                              enum flush_type type,
                                              PAGECACHE_FLUSH_FILTER filter,
                                              void *filter_arg);
extern my_bool pagecache_delete(PAGECACHE *pagecache,
                                PAGECACHE_FILE *file,
                                pgcache_page_no_t pageno,
                                enum pagecache_page_lock lock,
                                my_bool flush);
extern my_bool pagecache_delete_by_link(PAGECACHE *pagecache,
					PAGECACHE_BLOCK_LINK *link,
					enum pagecache_page_lock lock,
					my_bool flush);
extern my_bool pagecache_delete_pages(PAGECACHE *pagecache,
                                      PAGECACHE_FILE *file,
                                      pgcache_page_no_t pageno,
                                      uint page_count,
                                      enum pagecache_page_lock lock,
                                      my_bool flush);
extern void end_pagecache(PAGECACHE *keycache, my_bool cleanup);
extern my_bool pagecache_collect_changed_blocks_with_lsn(PAGECACHE *pagecache,
                                                         LEX_STRING *str,
                                                         LSN *min_lsn);
extern int reset_pagecache_counters(const char *name, PAGECACHE *pagecache);
extern uchar *pagecache_block_link_to_buffer(PAGECACHE_BLOCK_LINK *block);

extern uint pagecache_pagelevel(PAGECACHE_BLOCK_LINK *block);
extern void pagecache_add_level_by_link(PAGECACHE_BLOCK_LINK *block,
					uint level);

/* Functions to handle multiple key caches */
extern my_bool multi_pagecache_init(void);
extern void multi_pagecache_free(void);
extern PAGECACHE *multi_pagecache_search(uchar *key, uint length,
                                         PAGECACHE *def);
extern my_bool multi_pagecache_set(const uchar *key, uint length,
				   PAGECACHE *pagecache);
extern void multi_pagecache_change(PAGECACHE *old_data,
				   PAGECACHE *new_data);
extern int reset_pagecache_counters(const char *name,
                                    PAGECACHE *pagecache);
#ifndef DBUG_OFF
void pagecache_file_no_dirty_page(PAGECACHE *pagecache, PAGECACHE_FILE *file);
#else
#define pagecache_file_no_dirty_page(A,B) {}
#endif

C_MODE_END
#endif /* _keycache_h */
