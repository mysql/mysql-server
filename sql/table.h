/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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


/* Structs that defines the TABLE */

class Item;				/* Needed by ORDER */
class GRANT_TABLE;

/* Order clause list element */

typedef struct st_order {
  struct st_order *next;
  Item	 **item;			/* Point at item in select fields */
  bool	 asc;				/* true if ascending */
  bool	 free_me;			/* true if item isn't shared  */
  bool	 in_field_list;			/* true if in select field list */
  Field  *field;			/* If tmp-table group */
  char	 *buff;				/* If tmp-table group */
  table_map used,depend_map;
} ORDER;

typedef struct st_grant_info
{
  GRANT_TABLE *grant_table;
  uint version;
  uint privilege;
  uint want_privilege;
} GRANT_INFO;

/* Table cache entry struct */

class Field_timestamp;
class Field_blob;

struct st_table {
  handler *file;
  Field **field;			/* Pointer to fields */
  Field_blob **blob_field;		/* Pointer to blob fields */
  HASH	name_hash;			/* hash of field names */
  byte *record[3];			/* Pointer to records */
  uint fields;				/* field count */
  uint reclength;			/* Recordlength */
  uint rec_buff_length;
  uint keys,key_parts,primary_key,max_key_length,max_unique_length;
  uint uniques;
  uint null_fields;			/* number of null fields */
  uint blob_fields;			/* number of blob fields */
  key_map keys_in_use, keys_in_use_for_query;
  KEY  *key_info;			/* data of keys in database */
  TYPELIB keynames;			/* Pointers to keynames */
  ha_rows max_rows;			/* create information */
  ha_rows min_rows;			/* create information */
  ulong   avg_row_length;		/* create information */
  ulong   raid_chunksize;
  TYPELIB fieldnames;			/* Pointer to fieldnames */
  TYPELIB *intervals;			/* pointer to interval info */
  enum db_type db_type;			/* table_type for handler */
  enum row_type row_type;		/* How rows are stored */
  uint db_create_options;		/* Create options from database */
  uint db_options_in_use;		/* Options in use */
  uint db_record_offset;		/* if HA_REC_IN_SEQ */
  uint db_stat;				/* mode of file as in handler.h */
  uint raid_type,raid_chunks;
  uint status;				/* Used by postfix.. */
  uint system;				/* Set if system record */
  ulong time_stamp;			/* Set to offset+1 of record */
  uint timestamp_field_offset;
  uint next_number_index;
  uint blob_ptr_size;			/* 4 or 8 */
  uint next_number_key_offset;
  int current_lock;			/* Type of lock on table */
  my_bool copy_blobs;			/* copy_blobs when storing */
  my_bool null_row;			/* All columns are null */
  my_bool maybe_null,outer_join;	/* Used with OUTER JOIN */
  my_bool distinct,tmp_table,const_table;
  my_bool key_read;
  my_bool crypted;
  my_bool db_low_byte_first;		/* Portable row format */
  my_bool locked_by_flush;
  my_bool locked_by_name;
  my_bool crashed;
  my_bool is_view;
  Field *next_number_field,		/* Set if next_number is activated */
	*found_next_number_field,	/* Set on open */
        *rowid_field;
  Field_timestamp *timestamp_field;
  my_string comment;			/* Comment about table */
  REGINFO reginfo;			/* field connections */
  MEM_ROOT mem_root;
  GRANT_INFO grant;

  char		*table_cache_key;
  char		*table_name,*real_name,*path;
  uint		key_length;		/* Length of key */
  uint		tablenr,used_fields,null_bytes;
  table_map	map;
  ulong		version,flush_version;
  uchar		*null_flags;
  IO_CACHE	*io_cache;			/* If sorted trough file*/
  byte		*record_pointers;		/* If sorted in memory */
  ha_rows	found_records;			/* How many records in sort */
  ORDER		*group;
  key_map	quick_keys, used_keys, ref_primary_key;
  ha_rows	quick_rows[MAX_KEY];
  uint		quick_key_parts[MAX_KEY];
  key_part_map  const_key_parts[MAX_KEY];
  ulong		query_id;

  THD		*in_use;			/* Which thread uses this */
  struct st_table *next,*prev;
};


#define JOIN_TYPE_LEFT	1
#define JOIN_TYPE_RIGHT	2

typedef struct st_table_list {
  struct	st_table_list *next;
  char		*db,*name,*real_name;
  Item		*on_expr;			/* Used with outer join */
  struct st_table_list *natural_join;		/* natural join on this table*/
  List<String>	*use_index,*ignore_index;
  TABLE		*table;
  GRANT_INFO	grant;
  thr_lock_type lock_type;
  uint		outer_join;			/* Which join type */
  bool		straight;			/* optimize with prev table */
  bool          updating;     /* for replicate-do/ignore table */
} TABLE_LIST;
