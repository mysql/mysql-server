/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* This file should be included when using heap_database_funktions */
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

	/* defines used by heap-funktions */

#define HP_MAX_LEVELS	4		/* 128^5 records is enough */
#define HP_PTRS_IN_NOD	128

	/* struct used with heap_funktions */

typedef struct st_heapinfo		/* Struct from heap_info */
{
  ulong records;			/* Records in database */
  ulong deleted;			/* Deleted records in database */
  ulong max_records;
  ulong data_length;
  ulong index_length;
  uint reclength;			/* Length of one record */
  int errkey;
} HEAPINFO;


	/* Structs used by heap-database-handler */

typedef struct st_heap_ptrs
{
  byte *blocks[HP_PTRS_IN_NOD];		/* pointers to HP_PTRS or records */
} HP_PTRS;

struct st_level_info
{
  uint free_ptrs_in_block,records_under_level;
  HP_PTRS *last_blocks;			/* pointers to HP_PTRS or records */
};

typedef struct st_heap_block		/* The data is saved in blocks */
{
  HP_PTRS *root;
  struct st_level_info level_info[HP_MAX_LEVELS+1];
  uint levels;
  uint records_in_block;		/* Records in a heap-block */
  uint recbuffer;			/* Length of one saved record */
  ulong last_allocated;			/* Blocks allocated, used by keys */
} HP_BLOCK;

typedef struct st_hp_keyseg		/* Key-portion */
{
  uint start;				/* Start of key in record (from 0) */
  uint length;				/* Keylength */
  uint type;
} HP_KEYSEG;

typedef struct st_hp_keydef		/* Key definition with open */
{
  uint flag;				/* NOSAME */
  uint keysegs;				/* Number of key-segment */
  uint length;				/* Length of key (automatic) */
  HP_KEYSEG *seg;
  HP_BLOCK block;			/* Where keys are saved */
} HP_KEYDEF;

typedef struct st_heap_share
{
  HP_BLOCK block;
  HP_KEYDEF  *keydef;
  ulong min_records,max_records;	/* Params to open */
  ulong data_length,index_length;
  uint records;				/* records */
  uint blength;
  uint deleted;				/* Deleted records in database */
  uint reclength;			/* Length of one record */
  uint changed;
  uint keys,max_key_length;
  uint open_count;
  byte *del_link;			/* Link to next block with del. rec */
  my_string name;			/* Name of "memory-file" */
#ifdef THREAD
  THR_LOCK lock;
  pthread_mutex_t intern_lock;		/* Locking for use with _locking */
#endif
  LIST open_list;
} HP_SHARE;

struct st_hash_info;

typedef struct st_heap_info
{
  HP_SHARE *s;
  byte *current_ptr;
  struct st_hash_info *current_hash_ptr;
  ulong current_record,next_block;
  int lastinx,errkey;
  int  mode;				/* Mode of file (READONLY..) */
  uint opt_flag,update;
  byte *lastkey;			/* Last used key with rkey */
#ifdef THREAD
  THR_LOCK_DATA lock;
#endif
  LIST open_list;
} HP_INFO;

	/* Prototypes for heap-functions */

extern HP_INFO* heap_open(const char *name,int mode,uint keys,
			  HP_KEYDEF *keydef,uint reclength,
			  ulong max_records,ulong min_reloc);
extern int heap_close(HP_INFO *info);
extern int heap_write(HP_INFO *info,const byte *buff);
extern int heap_update(HP_INFO *info,const byte *old,const byte *newdata);
extern int heap_rrnd(HP_INFO *info,byte *buf,byte *pos);
extern int heap_scan_init(HP_INFO *info);
extern int heap_scan(register HP_INFO *info, byte *record);
extern int heap_delete(HP_INFO *info,const byte *buff);
extern int heap_info(HP_INFO *info,HEAPINFO *x,int flag);
extern int heap_create(const char *name);
extern int heap_delete_all(const char *name);
extern int heap_extra(HP_INFO *info,enum ha_extra_function function);
extern int heap_rename(const char *old_name,const char *new_name);
extern int heap_panic(enum ha_panic_function flag);
extern int heap_rsame(HP_INFO *info,byte *record,int inx);
extern int heap_rnext(HP_INFO *info,byte *record);
extern int heap_rprev(HP_INFO *info,byte *record);
extern int heap_rfirst(HP_INFO *info,byte *record);
extern int heap_rlast(HP_INFO *info,byte *record);
extern void heap_clear(HP_INFO *info);
extern int heap_rkey(HP_INFO *info,byte *record,int inx,const byte *key);
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
