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

/* This file should be included when using merge_isam_funktions */
/* Author: Michael Widenius */

#ifndef _merge_h
#define _merge_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _my_base_h
#include <my_base.h>
#endif
#ifndef _nisam_h
#include <nisam.h>
#endif

#define MRG_NAME_EXT	".MRG"

	/* Param to/from mrg_info */

typedef struct st_mrg_info		/* Struct from h_info */
{
  ulonglong records;			/* Records in database */
  ulonglong deleted;			/* Deleted records in database */
  ulonglong recpos;			/* Pos for last used record */
  ulonglong data_file_length;
  uint	reclength;			/* Recordlength */
  int	errkey;				/* With key was dupplicated on err */
  uint	options;			/* HA_OPTIONS_... used */
} MERGE_INFO;

typedef struct st_mrg_table_info
{
  N_INFO *table;
  ulonglong file_offset;
} MRG_TABLE;

typedef struct st_merge
{
  MRG_TABLE *open_tables,*current_table,*end_table,*last_used_table;
  ulonglong records;			/* records in tables */
  ulonglong del;			/* Removed records */
  ulonglong data_file_length;
  uint	 tables,options,reclength;
  my_bool cache_in_use;
  LIST	open_list;
} MRG_INFO;

typedef ulong mrg_off_t;

	/* Prototypes for merge-functions */

extern int mrg_close(MRG_INFO *file);
extern int mrg_delete(MRG_INFO *file,const byte *buff);
extern MRG_INFO *mrg_open(const char *name,int mode,int wait_if_locked);
extern int mrg_panic(enum ha_panic_function function);
extern int mrg_rfirst(MRG_INFO *file,byte *buf,int inx);
extern int mrg_rkey(MRG_INFO *file,byte *buf,int inx,const byte *key,
		       uint key_len, enum ha_rkey_function search_flag);
extern int mrg_rrnd(MRG_INFO *file,byte *buf, mrg_off_t pos);
extern int mrg_rsame(MRG_INFO *file,byte *record,int inx);
extern int mrg_update(MRG_INFO *file,const byte *old,const byte *new_rec);
extern int mrg_info(MRG_INFO *file,MERGE_INFO *x,int flag);
extern int mrg_lock_database(MRG_INFO *file,int lock_type);
extern int mrg_create(const char *name,const char **table_names);
extern int mrg_extra(MRG_INFO *file,enum ha_extra_function function);
extern ha_rows mrg_records_in_range(MRG_INFO *info,int inx,
				    const byte *start_key,uint start_key_len,
				    enum ha_rkey_function start_search_flag,
				    const byte *end_key,uint end_key_len,
				    enum ha_rkey_function end_search_flag);

extern mrg_off_t mrg_position(MRG_INFO *info);
#ifdef	__cplusplus
}
#endif
#endif
