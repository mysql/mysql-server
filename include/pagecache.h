/* Copyright (C) 2006 MySQL AB

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

/* Page cache variable structures */

#ifndef _pagecache_h
#define _pagecache_h
C_MODE_START

/* Type of the page */
enum pagecache_page_type
{
#ifndef DBUG_OFF
  /* used only for control page type chenging during debugging */
  PAGECACHE_EMPTY_PAGE,
#endif
  /* the page does not contain LSN */
  PAGECACHE_PLAIN_PAGE,
  /* the page contain LSN (maria tablespace page) */
  PAGECACHE_LSN_PAGE
};

/*
  This enum describe lock status changing. every typr of page cache will
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
  /* write page to the file and put it to the cache */
  PAGECACHE_WRITE_NOW,
  /* page already is in the file. (key cache insert analogue) */
  PAGECACHE_WRITE_DONE
};

typedef void *PAGECACHE_PAGE_LINK;

/* TODO: move to loghandler emulator */
typedef void LOG_HANDLER;
typedef void *LSN;

/* file descriptor for Maria */
typedef struct st_pagecache_file
{
  int file; /* it is for debugging purposes then it will be uint32 file_no */
} PAGECACHE_FILE;

/* page number for maria */
typedef uint32 maria_page_no_t;

/* declare structures that is used by  st_pagecache */

struct st_pagecache_block_link;
typedef struct st_pagecache_block_link PAGECACHE_BLOCK_LINK;
struct st_pagecache_page;
typedef struct st_pagecache_page PAGECACHE_PAGE;
struct st_pagecache_hash_link;
typedef struct st_pagecache_hash_link PAGECACHE_HASH_LINK;

/* info about requests in a waiting queue */
typedef struct st_pagecache_wqueue
{
  struct st_my_thread_var *last_thread;  /* circular list of waiting threads */
} PAGECACHE_WQUEUE;

#define PAGECACHE_CHANGED_BLOCKS_HASH 128  /* must be power of 2 */

/*
  The page cache structure
  It also contains read-only statistics parameters.
*/

typedef struct st_pagecache
{
  my_bool inited;
  my_bool resize_in_flush;       /* true during flush of resize operation    */
  my_bool can_be_used;           /* usage of cache for read/write is allowed */
  uint shift;                    /* block size = 2 ^ shift                   */
  my_size_t mem_size;            /* specified size of the cache memory       */
  uint32 block_size;             /* size of the page buffer of a cache block */
  ulong min_warm_blocks;         /* min number of warm blocks;               */
  ulong age_threshold;           /* age threshold for hot blocks             */
  ulonglong time;                /* total number of block link operations    */
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
  PAGECACHE_HASH_LINK **hash_root;/* arr. of entries into hash table buckets */
  PAGECACHE_HASH_LINK *hash_link_root;/* memory for hash table links         */
  PAGECACHE_HASH_LINK *free_hash_list;/* list of free hash links             */
  PAGECACHE_BLOCK_LINK *free_block_list;/* list of free blocks               */
  PAGECACHE_BLOCK_LINK *block_root;/* memory for block links                 */
  byte HUGE_PTR *block_mem;      /* memory for block buffers                 */
  PAGECACHE_BLOCK_LINK *used_last;/* ptr to the last block of the LRU chain  */
  PAGECACHE_BLOCK_LINK *used_ins;/* ptr to the insertion block in LRU chain  */
  pthread_mutex_t cache_lock;    /* to lock access to the cache structure    */
  PAGECACHE_WQUEUE resize_queue; /* threads waiting during resize operation  */
  PAGECACHE_WQUEUE waiting_for_hash_link;/* waiting for a free hash link     */
  PAGECACHE_WQUEUE waiting_for_block;   /* requests waiting for a free block */
  /* hash for dirty file bl.*/
  PAGECACHE_BLOCK_LINK *changed_blocks[PAGECACHE_CHANGED_BLOCKS_HASH];
  /* hash for other file bl.*/
  PAGECACHE_BLOCK_LINK *file_blocks[PAGECACHE_CHANGED_BLOCKS_HASH];

  LOG_HANDLER *loghandler;       /* loghandler structure */

  /*
    The following variables are and variables used to hold parameters for
    initializing the key cache.
  */

  ulonglong param_buff_size;    /* size the memory allocated for the cache  */
  ulong param_block_size;       /* size of the blocks in the key cache      */
  ulong param_division_limit;   /* min. percentage of warm blocks           */
  ulong param_age_threshold;    /* determines when hot block is downgraded  */

  /* Statistics variables. These are reset in reset_key_cache_counters(). */
  ulong global_blocks_changed;	/* number of currently dirty blocks         */
  ulonglong global_cache_w_requests;/* number of write requests (write hits) */
  ulonglong global_cache_write;     /* number of writes from cache to files  */
  ulonglong global_cache_r_requests;/* number of read requests (read hits)   */
  ulonglong global_cache_read;      /* number of reads from files to cache   */

  int blocks;                   /* max number of blocks in the cache        */
  my_bool in_init;		/* Set to 1 in MySQL during init/resize     */
} PAGECACHE;

extern int init_pagecache(PAGECACHE *pagecache, my_size_t use_mem,
                          uint division_limit, uint age_threshold,
                          uint block_size,
                          LOG_HANDLER *loghandler);
extern int resize_pagecache(PAGECACHE *pagecache,
                            my_size_t use_mem, uint division_limit,
                            uint age_threshold);
extern void change_pagecache_param(PAGECACHE *pagecache, uint division_limit,
                                   uint age_threshold);
extern byte *pagecache_read(PAGECACHE *pagecache,
                            PAGECACHE_FILE *file,
                            maria_page_no_t pageno,
                            uint level,
                            byte *buff,
                            enum pagecache_page_type type,
                            enum pagecache_page_lock lock,
                            PAGECACHE_PAGE_LINK *link);
extern my_bool pagecache_write(PAGECACHE *pagecache,
                               PAGECACHE_FILE *file,
                               maria_page_no_t pageno,
                               uint level,
                               byte *buff,
                               enum pagecache_page_type type,
                               enum pagecache_page_lock lock,
                               enum pagecache_page_pin pin,
                               enum pagecache_write_mode write_mode,
                               PAGECACHE_PAGE_LINK *link);
void pagecache_unlock_page(PAGECACHE *pagecache,
                           PAGECACHE_FILE *file,
                           maria_page_no_t pageno,
                           enum pagecache_page_lock lock,
                           enum pagecache_page_pin pin,
                           my_bool stamp_this_page,
                           LSN first_REDO_LSN_for_page);
void pagecache_unlock(PAGECACHE *pagecache,
                      PAGECACHE_PAGE_LINK *link,
                      enum pagecache_page_lock lock,
                      enum pagecache_page_pin pin,
                      my_bool stamp_this_page,
                      LSN first_REDO_LSN_for_page);
void pagecache_unpin_page(PAGECACHE *pagecache,
                          PAGECACHE_FILE *file,
                          maria_page_no_t pageno);
void pagecache_unpin(PAGECACHE *pagecache,
                     PAGECACHE_PAGE_LINK *link);
extern int flush_pagecache_blocks(PAGECACHE *keycache,
                                  PAGECACHE_FILE *file,
                                  enum flush_type type);
my_bool pagecache_delete_page(PAGECACHE *pagecache,
                              PAGECACHE_FILE *file,
                              maria_page_no_t pageno,
                              enum pagecache_page_lock lock,
                              my_bool flush);
extern void end_pagecache(PAGECACHE *keycache, my_bool cleanup);

C_MODE_END
#endif /* _keycache_h */
