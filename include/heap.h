/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* This file should be included when using heap_database_functions */
/* Author: Michael Widenius */

/**
  @file include/heap.h
*/

#ifndef _heap_h
#define _heap_h

#ifndef _my_base_h
#include <my_base.h>
#endif

#include <sys/types.h>
#include <thr_lock.h>
#include <time.h>

#include "my_compare.h"
#include "my_inttypes.h"
#include "my_list.h"
#include "my_tree.h"

#ifdef	__cplusplus
extern "C" {
#endif

	/* defines used by heap-funktions */

#define HP_MAX_LEVELS	4		/* 128^5 records is enough */
#define HP_PTRS_IN_NOD	128

	/* struct used with heap_funktions */

typedef struct st_heapinfo		/* Struct from heap_info */
{
  ulong records;			/* Records in database */
  ulong deleted;			/* Deleted records in database */
  ulong max_records;
  ulonglong data_length;
  ulonglong index_length;
  uint reclength;			/* Length of one record */
  int errkey;
  ulonglong auto_increment;
  time_t create_time;
} HEAPINFO;


	/* Structs used by heap-database-handler */

typedef struct st_heap_ptrs
{
  uchar *blocks[HP_PTRS_IN_NOD];		/* pointers to HP_PTRS or records */
} HP_PTRS;

struct st_level_info
{
  /* Number of unused slots in *last_blocks HP_PTRS block (0 for 0th level) */
  uint free_ptrs_in_block;
  
  /*
    Maximum number of records that can be 'contained' inside of each element
    of last_blocks array. For level 0 - 1, for level 1 - HP_PTRS_IN_NOD, for 
    level 2 - HP_PTRS_IN_NOD^2 and so forth.
  */
  ulong records_under_level;

  /*
    Ptr to last allocated HP_PTRS (or records buffer for level 0) on this 
    level.
  */
  HP_PTRS *last_blocks;			
};


/*
  Heap table records and hash index entries are stored in HP_BLOCKs.
  HP_BLOCK is used as a 'growable array' of fixed-size records. Size of record
  is recbuffer bytes.
  The internal representation is as follows:
  HP_BLOCK is a hierarchical structure of 'blocks'.
  A block at level 0 is an array records_in_block records. 
  A block at higher level is an HP_PTRS structure with pointers to blocks at 
  lower levels.
  At the highest level there is one top block. It is stored in HP_BLOCK::root.

  See hp_find_block for a description of how record pointer is obtained from 
  its index.
  See hp_get_new_block 
*/

typedef struct st_heap_block
{
  HP_PTRS *root;                        /* Top-level block */ 
  struct st_level_info level_info[HP_MAX_LEVELS+1];
  uint levels;                          /* number of used levels */
  uint records_in_block;		/* Records in one heap-block */
  uint recbuffer;			/* Length of one saved record */
  ulong last_allocated; /* number of records there is allocated space for */
} HP_BLOCK;

struct st_heap_info;			/* For referense */

typedef struct st_hp_keydef		/* Key definition with open */
{
  uint flag;				/* HA_NOSAME | HA_NULL_PART_KEY */
  uint keysegs;				/* Number of key-segment */
  uint length;				/* Length of key (automatic) */
  uint8 algorithm;			/* HASH / BTREE */
  HA_KEYSEG *seg;
  HP_BLOCK block;			/* Where keys are saved */
  /*
    Number of buckets used in hash table. Used only to provide
    #records estimates for heap key scans.
  */
  ha_rows hash_buckets; 
  TREE rb_tree;
  int (*write_key)(struct st_heap_info *info, struct st_hp_keydef *keyinfo,
		   const uchar *record, uchar *recpos);
  int (*delete_key)(struct st_heap_info *info, struct st_hp_keydef *keyinfo,
		   const uchar *record, uchar *recpos, int flag);
  uint (*get_key_length)(struct st_hp_keydef *keydef, const uchar *key);
} HP_KEYDEF;

typedef struct st_heap_share
{
  HP_BLOCK block;
  HP_KEYDEF  *keydef;
  ulong min_records,max_records;	/* Params to open */
  ulonglong data_length,index_length,max_table_size;
  uint key_stat_version;                /* version to indicate insert/delete */
  uint records;				/* records */
  uint blength;				/* records rounded up to 2^n */
  uint deleted;				/* Deleted records in database */
  uint reclength;			/* Length of one record */
  uint changed;
  uint keys,max_key_length;
  uint currently_disabled_keys;    /* saved value from "keys" when disabled */
  uint open_count;
  uchar *del_link;			/* Link to next block with del. rec */
  char * name;			/* Name of "memory-file" */
  time_t create_time;
  THR_LOCK lock;
  bool delete_on_close;
  LIST open_list;
  uint auto_key;
  uint auto_key_type;			/* real type of the auto key segment */
  ulonglong auto_increment;
} HP_SHARE;

struct st_hp_hash_info;

typedef struct st_heap_info
{
  HP_SHARE *s;
  uchar *current_ptr;
  struct st_hp_hash_info *current_hash_ptr;
  ulong current_record,next_block;
  int lastinx,errkey;
  int  mode;				/* Mode of file (READONLY..) */
  uint opt_flag,update;
  uchar *lastkey;			/* Last used key with rkey */
  uchar *recbuf;                         /* Record buffer for rb-tree keys */
  enum ha_rkey_function last_find_flag;
  TREE_ELEMENT *parents[MAX_TREE_HEIGHT+1];
  TREE_ELEMENT **last_pos;
  uint lastkey_len;
  bool implicit_emptied;
  THR_LOCK_DATA lock;
  LIST open_list;
} HP_INFO;


typedef struct st_heap_create_info
{
  HP_KEYDEF *keydef;
  ulong max_records;
  ulong min_records;
  uint auto_key;                        /* keynr [1 - maxkey] for auto key */
  uint auto_key_type;
  uint keys;
  uint reclength;
  ulonglong max_table_size;
  ulonglong auto_increment;
  bool with_auto_increment;
  bool single_instance;
  bool delete_on_close;
  /*
    TRUE if heap_create should 'pin' the created share by setting
    open_count to 1. Is only looked at if not internal_table.
  */
  bool pin_share;
} HP_CREATE_INFO;

	/* Prototypes for heap-functions */

extern HP_INFO *heap_open(const char *name, int mode);
extern HP_INFO *heap_open_from_share(HP_SHARE *share, int mode);
extern HP_INFO *heap_open_from_share_and_register(HP_SHARE *share, int mode);
extern void heap_release_share(HP_SHARE *share, bool single_instance);
extern int heap_close(HP_INFO *info);
extern int heap_write(HP_INFO *info,const uchar *buff);
extern int heap_update(HP_INFO *info,const uchar *old,const uchar *newdata);
extern int heap_rrnd(HP_INFO *info,uchar *buf,uchar *pos);
extern int heap_scan_init(HP_INFO *info);
extern int heap_scan(HP_INFO *info, uchar *record);
extern int heap_delete(HP_INFO *info,const uchar *buff);
extern int heap_info(HP_INFO *info,HEAPINFO *x,int flag);
extern int heap_create(const char *name,
                       HP_CREATE_INFO *create_info, HP_SHARE **share,
                       bool *created_new_share);
extern int heap_delete_table(const char *name);
extern void heap_drop_table(HP_INFO *info);
extern int heap_extra(HP_INFO *info,enum ha_extra_function function);
extern int heap_reset(HP_INFO *info);
extern int heap_rename(const char *old_name,const char *new_name);
extern int heap_panic(enum ha_panic_function flag);
extern int heap_rsame(HP_INFO *info,uchar *record,int inx);
extern int heap_rnext(HP_INFO *info,uchar *record);
extern int heap_rprev(HP_INFO *info,uchar *record);
extern int heap_rfirst(HP_INFO *info,uchar *record,int inx);
extern int heap_rlast(HP_INFO *info,uchar *record,int inx);
extern void heap_clear(HP_INFO *info);
extern void heap_clear_keys(HP_INFO *info);
extern int heap_disable_indexes(HP_INFO *info);
extern int heap_enable_indexes(HP_INFO *info);
extern int heap_indexes_are_disabled(HP_INFO *info);
extern void heap_update_auto_increment(HP_INFO *info, const uchar *record);
ha_rows hp_rb_records_in_range(HP_INFO *info, int inx, key_range *min_key,
                               key_range *max_key);
int hp_panic(enum ha_panic_function flag);
int heap_rkey(HP_INFO *info, uchar *record, int inx, const uchar *key,
              key_part_map keypart_map, enum ha_rkey_function find_flag);
extern uchar * heap_find(HP_INFO *info,int inx,const uchar *key);
extern int heap_check_heap(HP_INFO *info, bool print_status);
extern uchar *heap_position(HP_INFO *info);

typedef uchar *HEAP_PTR;

#ifdef	__cplusplus
}
#endif
#endif
