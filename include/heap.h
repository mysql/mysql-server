/* Copyright (C) 2000,2004 MySQL AB

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

/* This file should be included when using heap_database_functions */
/* Author: Michael Widenius */

#ifndef _heap_h
#define _heap_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _my_base_h
#include <my_base.h>
#endif
#ifdef THREAD
#include <my_pthread.h>
#include <thr_lock.h>
#endif

#include "my_handler.h"
#include "my_tree.h"

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
} HEAPINFO;


	/* Structs used by heap-database-handler */

typedef struct st_heap_ptrs
{
  byte *blocks[HP_PTRS_IN_NOD];		/* pointers to HP_PTRS or records */
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
  uint records_under_level;

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
  uint flag;				/* HA_NOSAME | HA_NULL_PART_KEY */
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
		   const byte *record, byte *recpos);
  int (*delete_key)(struct st_heap_info *info, struct st_hp_keydef *keyinfo,
		   const byte *record, byte *recpos, int flag);
  uint (*get_key_length)(struct st_hp_keydef *keydef, const byte *key);
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
  byte *del_link;			/* Link to next block with del. rec */
  my_string name;			/* Name of "memory-file" */
#ifdef THREAD
  THR_LOCK lock;
  pthread_mutex_t intern_lock;		/* Locking for use with _locking */
#endif
  my_bool delete_on_close;
  LIST open_list;
  uint auto_key;
  uint auto_key_type;			/* real type of the auto key segment */
  ulonglong auto_increment;
} HP_SHARE;

struct st_hp_hash_info;

typedef struct st_heap_info
{
  HP_SHARE *s;
  byte *current_ptr;
  struct st_hp_hash_info *current_hash_ptr;
  ulong current_record,next_block;
  int lastinx,errkey;
  int  mode;				/* Mode of file (READONLY..) */
  uint opt_flag,update;
  byte *lastkey;			/* Last used key with rkey */
  byte *recbuf;                         /* Record buffer for rb-tree keys */
  enum ha_rkey_function last_find_flag;
  TREE_ELEMENT *parents[MAX_TREE_HEIGHT+1];
  TREE_ELEMENT **last_pos;
  uint lastkey_len;
  my_bool implicit_emptied;
#ifdef THREAD
  THR_LOCK_DATA lock;
#endif
  LIST open_list;
} HP_INFO;


typedef struct st_heap_create_info
{
  uint auto_key;                        /* keynr [1 - maxkey] for auto key */
  uint auto_key_type;
  ulonglong max_table_size;
  ulonglong auto_increment;
  my_bool with_auto_increment;
} HP_CREATE_INFO;

	/* Prototypes for heap-functions */

extern HP_INFO *heap_open(const char *name, int mode);
extern int heap_close(HP_INFO *info);
extern int heap_write(HP_INFO *info,const byte *buff);
extern int heap_update(HP_INFO *info,const byte *old,const byte *newdata);
extern int heap_rrnd(HP_INFO *info,byte *buf,byte *pos);
extern int heap_scan_init(HP_INFO *info);
extern int heap_scan(register HP_INFO *info, byte *record);
extern int heap_delete(HP_INFO *info,const byte *buff);
extern int heap_info(HP_INFO *info,HEAPINFO *x,int flag);
extern int heap_create(const char *name, uint keys, HP_KEYDEF *keydef,
		       uint reclength, ulong max_records, ulong min_records,
		       HP_CREATE_INFO *create_info);
extern int heap_delete_table(const char *name);
extern int heap_extra(HP_INFO *info,enum ha_extra_function function);
extern int heap_rename(const char *old_name,const char *new_name);
extern int heap_panic(enum ha_panic_function flag);
extern int heap_rsame(HP_INFO *info,byte *record,int inx);
extern int heap_rnext(HP_INFO *info,byte *record);
extern int heap_rprev(HP_INFO *info,byte *record);
extern int heap_rfirst(HP_INFO *info,byte *record,int inx);
extern int heap_rlast(HP_INFO *info,byte *record,int inx);
extern void heap_clear(HP_INFO *info);
extern void heap_clear_keys(HP_INFO *info);
extern int heap_disable_indexes(HP_INFO *info);
extern int heap_enable_indexes(HP_INFO *info);
extern int heap_indexes_are_disabled(HP_INFO *info);
extern void heap_update_auto_increment(HP_INFO *info, const byte *record);
ha_rows hp_rb_records_in_range(HP_INFO *info, int inx, key_range *min_key,
                               key_range *max_key);
int heap_rkey(HP_INFO *info, byte *record, int inx, const byte *key, 
              uint key_len, enum ha_rkey_function find_flag);
extern gptr heap_find(HP_INFO *info,int inx,const byte *key);
extern int heap_check_heap(HP_INFO *info, my_bool print_status);
extern byte *heap_position(HP_INFO *info);

/* The following is for programs that uses the old HEAP interface where
   pointer to rows where a long instead of a (byte*).
*/

#if defined(WANT_OLD_HEAP_VERSION) || defined(OLD_HEAP_VERSION)
extern int heap_rrnd_old(HP_INFO *info,byte *buf,ulong pos);
extern ulong heap_position_old(HP_INFO *info);
#endif
#ifdef OLD_HEAP_VERSION
typedef ulong HEAP_PTR;
#define heap_position(A) heap_position_old(A)
#define heap_rrnd(A,B,C) heap_rrnd_old(A,B,C)
#else
typedef byte *HEAP_PTR;
#endif

#ifdef	__cplusplus
}
#endif
#endif
