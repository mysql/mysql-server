/* Copyright (C) 2003 MySQL AB

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

/* Key cache variable structures */

#ifndef _keycache_h
#define _keycache_h
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
  ulonglong read_requests;  /* number of read requests (read hits)           */
  ulonglong reads;        /* number of actual reads from files into buffers  */
  ulonglong write_requests; /* number of write requests (write hits)         */
  ulonglong writes;       /* number of actual writes from buffers into files */
} KEY_CACHE_STATISTICS;

/* The type of a key cache object */
typedef enum key_cache_type
{
  SIMPLE_KEY_CACHE,         
  PARTITIONED_KEY_CACHE
} KEY_CACHE_TYPE;


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
  int    (*init)    (void *, uint key_cache_block_size,
                     size_t use_mem, uint division_limit, uint age_threshold);
  int    (*resize)  (void *, uint key_cache_block_size,
                     size_t use_mem, uint division_limit, uint age_threshold);
  void   (*change_param) (void *keycache_cb,
                          uint division_limit, uint age_threshold);      
  uchar* (*read)    (void *keycache_cb,
                     File file, my_off_t filepos, int level,
                     uchar *buff, uint length,
                     uint block_length, int return_buffer);
  int   (*insert)   (void *keycache_cb,
                     File file, my_off_t filepos, int level,
                     uchar *buff, uint length);
  int   (*write)    (void *keycache_cb,
                     File file, void *file_extra,
                     my_off_t filepos, int level,
                     uchar *buff, uint length, 
                     uint block_length, int force_write);
  int   (*flush)    (void *keycache_cb,
                     int file, void *file_extra,
                     enum flush_type type); 
  int (*reset_counters) (const char *name, void *keycache_cb); 
  void (*end)    (void *keycache_cb, my_bool cleanup);
  void (*get_stats) (void *keycache_cb, uint partition_no, 
                     KEY_CACHE_STATISTICS *key_cache_stats); 
  ulonglong (*get_stat_val) (void *keycache_cb, uint var_no);
} KEY_CACHE_FUNCS;


typedef struct st_key_cache
{
  KEY_CACHE_TYPE key_cache_type; /* type of the key cache used for debugging */
  void *keycache_cb;             /* control block of the used key cache      */
  KEY_CACHE_FUNCS *interface_funcs; /* interface functions of the key cache  */
  ulonglong param_buff_size;     /* size the memory allocated for the cache  */
  ulong param_block_size;        /* size of the blocks in the key cache      */
  ulong param_division_limit;    /* min. percentage of warm blocks           */
  ulong param_age_threshold;     /* determines when hot block is downgraded  */
  ulong param_partitions;        /* number of the key cache partitions       */
  my_bool key_cache_inited;      /* <=> key cache has been created           */
  my_bool can_be_used;           /* usage of cache for read/write is allowed */
  my_bool in_init;		 /* Set to 1 in MySQL during init/resize     */
  uint partitions;               /* actual number of partitions              */
  size_t key_cache_mem_size;     /* specified size of the cache memory       */
  ulong blocks_used;           /* maximum number of concurrently used blocks */
  ulong blocks_unused;           /* number of currently unused blocks        */
  ulong global_blocks_changed;	 /* number of currently dirty blocks         */
  ulonglong global_cache_w_requests;/* number of write requests (write hits) */
  ulonglong global_cache_write;     /* number of writes from cache to files  */
  ulonglong global_cache_r_requests;/* number of read requests (read hits)   */
  ulonglong global_cache_read;      /* number of reads from files to cache   */
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
extern ulonglong get_key_cache_stat_value(KEY_CACHE *keycache, uint var_no);

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
                                    KEY_CACHE *key_cache);
extern int repartition_key_cache(KEY_CACHE *keycache,
                                 uint key_cache_block_size,
			         size_t use_mem, 
                                 uint division_limit,
			         uint age_threshold,
                                 uint partitions);
C_MODE_END
#endif /* _keycache_h */
