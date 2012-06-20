/* Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

/* Key cache variable structures */

#ifndef _keycache_h
#define _keycache_h

#include "my_sys.h"                             /* flush_type */

C_MODE_START

/* 
  Currently the default key cache is created as non-partitioned at 
  the start of the server unless the server is started with the parameter 
  --key-cache-partitions that is greater than 0
*/

#define DEFAULT_KEY_CACHE_PARTITIONS    0

/* 
  MAX_KEY_CACHE_PARTITIONS cannot be greater than 
  sizeof(MYISAM_SHARE::dirty_part_map)
  Currently sizeof(MYISAM_SHARE::dirty_part_map)=sizeof(ulonglong)
*/

#define MAX_KEY_CACHE_PARTITIONS    64

/* The structure to get statistical data about a key cache */

typedef struct st_key_cache_statistics
{
  ulonglong mem_size;       /* memory for cache buffers/auxiliary structures */
  ulonglong block_size;     /* size of the each buffers in the key cache     */
  ulonglong blocks_used;    /* maximum number of used blocks/buffers         */ 
  ulonglong blocks_unused;  /* number of currently unused blocks             */
  ulonglong blocks_changed; /* number of currently dirty blocks              */
  ulonglong blocks_warm;    /* number of blocks in warm sub-chain            */
  ulonglong read_requests;  /* number of read requests (read hits)           */
  ulonglong reads;        /* number of actual reads from files into buffers  */
  ulonglong write_requests; /* number of write requests (write hits)         */
  ulonglong writes;       /* number of actual writes from buffers into files */
} KEY_CACHE_STATISTICS;

#define NUM_LONG_KEY_CACHE_STAT_VARIABLES 3

/* The type of a key cache object */
typedef enum key_cache_type
{
  SIMPLE_KEY_CACHE,         
  PARTITIONED_KEY_CACHE
} KEY_CACHE_TYPE;


typedef
  int    (*INIT_KEY_CACHE)  
           (void *, uint key_cache_block_size,
            size_t use_mem, uint division_limit, uint age_threshold);
typedef
  int    (*RESIZE_KEY_CACHE)
           (void *, uint key_cache_block_size,
            size_t use_mem, uint division_limit, uint age_threshold);
typedef
  void   (*CHANGE_KEY_CACHE_PARAM)
           (void *keycache_cb,
            uint division_limit, uint age_threshold);
typedef
  uchar* (*KEY_CACHE_READ)
           (void *keycache_cb,
            File file, my_off_t filepos, int level,
            uchar *buff, uint length,
            uint block_length, int return_buffer);
typedef
  int    (*KEY_CACHE_INSERT)
           (void *keycache_cb,
            File file, my_off_t filepos, int level,
            uchar *buff, uint length);
typedef
  int    (*KEY_CACHE_WRITE)
           (void *keycache_cb,
            File file, void *file_extra,
            my_off_t filepos, int level,
            uchar *buff, uint length, 
            uint block_length, int force_write);
typedef
  int    (*FLUSH_KEY_BLOCKS)
           (void *keycache_cb,
            int file, void *file_extra,
            enum flush_type type); 
typedef
  int    (*RESET_KEY_CACHE_COUNTERS)
           (const char *name, void *keycache_cb); 
typedef
  void   (*END_KEY_CACHE)
           (void *keycache_cb, my_bool cleanup);
typedef
  void   (*GET_KEY_CACHE_STATISTICS)
           (void *keycache_cb, uint partition_no, 
            KEY_CACHE_STATISTICS *key_cache_stats); 

/*
  An object of the type KEY_CACHE_FUNCS contains pointers to all functions
  from the key cache interface.
  Currently a key cache can be of two types: simple and partitioned.
  For each of them its own static structure of the type KEY_CACHE_FUNCS is
  defined . The structures contain the pointers to the implementations of
  the interface functions used by simple key caches and partitioned key
  caches respectively. Pointers to these structures are assigned to key cache
  objects at the time of their creation.
*/   

typedef struct st_key_cache_funcs 
{
  INIT_KEY_CACHE init;
  RESIZE_KEY_CACHE         resize;
  CHANGE_KEY_CACHE_PARAM   change_param;     
  KEY_CACHE_READ           read;
  KEY_CACHE_INSERT         insert;
  KEY_CACHE_WRITE          write;
  FLUSH_KEY_BLOCKS         flush;
  RESET_KEY_CACHE_COUNTERS reset_counters; 
  END_KEY_CACHE            end;
  GET_KEY_CACHE_STATISTICS get_stats; 
} KEY_CACHE_FUNCS;


typedef struct st_key_cache
{
  KEY_CACHE_TYPE key_cache_type; /* type of the key cache used for debugging */
  void *keycache_cb;             /* control block of the used key cache      */
  KEY_CACHE_FUNCS *interface_funcs; /* interface functions of the key cache  */
  ulonglong param_buff_size;     /* size the memory allocated for the cache  */
  ulonglong param_block_size;    /* size of the blocks in the key cache      */
  ulonglong param_division_limit;/* min. percentage of warm blocks           */
  ulonglong param_age_threshold; /* determines when hot block is downgraded  */
  ulonglong param_partitions;    /* number of the key cache partitions       */
  my_bool key_cache_inited;      /* <=> key cache has been created           */
  my_bool can_be_used;           /* usage of cache for read/write is allowed */
  my_bool in_init;               /* set to 1 in MySQL during init/resize     */
  uint partitions;               /* actual number of partitions              */
  size_t key_cache_mem_size;     /* specified size of the cache memory       */
  pthread_mutex_t op_lock;       /* to serialize operations like 'resize'    */
} KEY_CACHE;


/* The default key cache */
extern KEY_CACHE dflt_key_cache_var, *dflt_key_cache;

extern int init_key_cache(KEY_CACHE *keycache, uint key_cache_block_size,
			  size_t use_mem, uint division_limit,
			  uint age_threshold, uint partitions);
extern int resize_key_cache(KEY_CACHE *keycache, uint key_cache_block_size,
			    size_t use_mem, uint division_limit,
			    uint age_threshold);
extern void change_key_cache_param(KEY_CACHE *keycache, uint division_limit,
				   uint age_threshold);
extern uchar *key_cache_read(KEY_CACHE *keycache,
                            File file, my_off_t filepos, int level,
                            uchar *buff, uint length,
			    uint block_length,int return_buffer);
extern int key_cache_insert(KEY_CACHE *keycache,
                            File file, my_off_t filepos, int level,
                            uchar *buff, uint length);
extern int key_cache_write(KEY_CACHE *keycache,
                           File file, void *file_extra,
                           my_off_t filepos, int level,
                           uchar *buff, uint length,
			   uint block_length, int force_write);
extern int flush_key_blocks(KEY_CACHE *keycache,
                            int file, void *file_extra,
                            enum flush_type type);
extern void end_key_cache(KEY_CACHE *keycache, my_bool cleanup);
extern void get_key_cache_statistics(KEY_CACHE *keycache,
                                     uint partition_no, 
                                     KEY_CACHE_STATISTICS *key_cache_stats);

/* Functions to handle multiple key caches */
extern my_bool multi_keycache_init(void);
extern void multi_keycache_free(void);
extern KEY_CACHE *multi_key_cache_search(uchar *key, uint length,
                                         KEY_CACHE *def);
extern my_bool multi_key_cache_set(const uchar *key, uint length,
				   KEY_CACHE *key_cache);
extern void multi_key_cache_change(KEY_CACHE *old_data,
				   KEY_CACHE *new_data);
extern int reset_key_cache_counters(const char *name,
                                    KEY_CACHE *key_cache, void *);
extern int repartition_key_cache(KEY_CACHE *keycache,
                                 uint key_cache_block_size,
			         size_t use_mem, 
                                 uint division_limit,
			         uint age_threshold,
                                 uint partitions);
C_MODE_END
#endif /* _keycache_h */
