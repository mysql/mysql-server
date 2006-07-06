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
class st_select_lex_unit;

/* Order clause list element */

typedef struct st_order {
  struct st_order *next;
  Item	 **item;			/* Point at item in select fields */
  Item	 *item_ptr;			/* Storage for initial item */
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
  ulong privilege;
  ulong want_privilege;
} GRANT_INFO;

enum tmp_table_type {NO_TMP_TABLE=0, TMP_TABLE=1, TRANSACTIONAL_TMP_TABLE=2};

typedef struct st_filesort_info
{
  IO_CACHE *io_cache;           /* If sorted through filebyte                */
  byte     *addon_buf;          /* Pointer to a buffer if sorted with fields */
  uint      addon_length;       /* Length of the buffer                      */
  struct st_sort_addon_field *addon_field;     /* Pointer to the fields info */
  void    (*unpack)(struct st_sort_addon_field *, byte *); /* To unpack back */
  byte     *record_pointers;    /* If sorted in memory                       */
  ha_rows   found_records;      /* How many records in sort                  */
} FILESORT_INFO;


/*
  Values in this enum are used to indicate how a tables TIMESTAMP field
  should be treated. It can be set to the current timestamp on insert or
  update or both.
  WARNING: The values are used for bit operations. If you change the
  enum, you must keep the bitwise relation of the values. For example:
  (int) TIMESTAMP_AUTO_SET_ON_BOTH must be equal to
  (int) TIMESTAMP_AUTO_SET_ON_INSERT | (int) TIMESTAMP_AUTO_SET_ON_UPDATE.
  We use an enum here so that the debugger can display the value names.
*/
enum timestamp_auto_set_type
{
  TIMESTAMP_NO_AUTO_SET= 0, TIMESTAMP_AUTO_SET_ON_INSERT= 1,
  TIMESTAMP_AUTO_SET_ON_UPDATE= 2, TIMESTAMP_AUTO_SET_ON_BOTH= 3
};
#define clear_timestamp_auto_bits(_target_, _bits_) \
  (_target_)= (enum timestamp_auto_set_type)((int)(_target_) & ~(int)(_bits_))

/* Table cache entry struct */

class Field_timestamp;
class Field_blob;

struct st_table {
  handler *file;
  Field **field;			/* Pointer to fields */
  Field_blob **blob_field;		/* Pointer to blob fields */
  /* hash of field names (contains pointers to elements of field array) */
  HASH	name_hash;
  byte *record[2];			/* Pointer to records */
  byte *default_values;          	/* Default values for INSERT */
  byte *insert_values;                  /* used by INSERT ... UPDATE */
  uint fields;				/* field count */
  uint reclength;			/* Recordlength */
  uint rec_buff_length;
  uint keys,key_parts,primary_key,max_key_length,max_unique_length;
  uint total_key_length;
  uint uniques;
  uint null_fields;			/* number of null fields */
  uint blob_fields;			/* number of blob fields */
  key_map keys_in_use, keys_for_keyread, read_only_keys;
  key_map quick_keys;
  key_map used_keys;  /* keys that cover all used table fields */
  key_map keys_in_use_for_query;
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

  /*
    If this table has TIMESTAMP field with auto-set property (pointed by
    timestamp_field member) then this variable indicates during which
    operations (insert only/on update/in both cases) we should set this
    field to current timestamp. If there are no such field in this table
    or we should not automatically set its value during execution of current
    statement then the variable contains TIMESTAMP_NO_AUTO_SET (i.e. 0).

    Value of this variable is set for each statement in open_table() and
    if needed cleared later in statement processing code (see mysql_update()
    as example).
  */
  timestamp_auto_set_type timestamp_field_type;
  /* Index of auto-updated TIMESTAMP field in field array */
  uint timestamp_field_offset;
  
  uint next_number_index;
  uint blob_ptr_size;			/* 4 or 8 */
  uint next_number_key_offset;
  uint lock_position;                   /* Position in MYSQL_LOCK.table */
  uint lock_data_start;                 /* Start pos. in MYSQL_LOCK.locks */
  uint lock_count;                      /* Number of locks */
  int current_lock;			/* Type of lock on table */
  enum tmp_table_type tmp_table;
  my_bool copy_blobs;			/* copy_blobs when storing */
  /*
    Used in outer joins: if true, all columns are considered to have NULL
    values, including columns declared as "not null".
  */
  my_bool null_row;
  /* 0 or JOIN_TYPE_{LEFT|RIGHT}, same as TABLE_LIST::outer_join */
  my_bool outer_join;
  my_bool maybe_null;                   /* true if (outer_join != 0) */
  my_bool force_index;
  my_bool distinct,const_table,no_rows;
  my_bool key_read;
  my_bool crypted;
  my_bool db_low_byte_first;		/* Portable row format */
  my_bool locked_by_flush;
  my_bool locked_by_name;
  my_bool fulltext_searched;
  my_bool crashed;
  my_bool is_view;
  my_bool no_keyread, no_cache;
  my_bool clear_query_id;               /* To reset query_id for tables and cols */
  my_bool auto_increment_field_not_null;
  Field *next_number_field,		/* Set if next_number is activated */
	*found_next_number_field,	/* Set on open */
        *rowid_field;
  Field_timestamp *timestamp_field;
#if MYSQL_VERSION_ID < 40100
  /*
    Indicates whenever we have to set field_length members of all TIMESTAMP
    fields to 19 (to honour 'new_mode' variable) or to original
    field_length values.
  */
  my_bool timestamp_mode;
#endif
  my_string comment;			/* Comment about table */
  CHARSET_INFO *table_charset;		/* Default charset of string fields */
  REGINFO reginfo;			/* field connections */
  MEM_ROOT mem_root;
  GRANT_INFO grant;

  /* A pair "database_name\0table_name\0", widely used as simply a db name */
  char		*table_cache_key;
  char		*table_name,*real_name,*path;
  uint		key_length;		/* Length of key */
  uint		tablenr,used_fields,null_bytes;
  table_map	map;                    /* ID bit of table (1,2,4,8,16...) */
  ulong		version,flush_version;
  uchar		*null_flags;
  FILESORT_INFO sort;
  ORDER		*group;
  ha_rows	quick_rows[MAX_KEY];
  uint		quick_key_parts[MAX_KEY];
  key_part_map  const_key_parts[MAX_KEY];
  ulong		query_id;
  uchar		frm_version;

  union					/* Temporary variables */
  {
    uint        temp_pool_slot;		/* Used by intern temp tables */
    struct st_table_list *pos_in_table_list;
  };
  /* number of select if it is derived table */
  uint          derived_select_number;
  THD		*in_use;		/* Which thread uses this */
  struct st_table *next,*prev;
};


#define JOIN_TYPE_LEFT	1
#define JOIN_TYPE_RIGHT	2

typedef struct st_table_list
{
  struct	st_table_list *next;
  char		*db, *alias, *real_name;
  char          *option;                /* Used by cache index  */ 
  Item		*on_expr;		/* Used with outer join */
  struct st_table_list *natural_join;	/* natural join on this table*/
  /* ... join ... USE INDEX ... IGNORE INDEX */
  List<String>	*use_index, *ignore_index; 
  TABLE          *table;      /* opened table */
  st_table_list  *table_list; /* pointer to node of list of all tables */
  class st_select_lex_unit *derived;	/* SELECT_LEX_UNIT of derived table */
 GRANT_INFO	grant;
  thr_lock_type lock_type;
  uint		outer_join;		/* Which join type */
  uint		shared;			/* Used in multi-upd */
  uint32        db_length, real_name_length;
  bool		straight;		/* optimize with prev table */
  bool          updating;               /* for replicate-do/ignore table */
  bool		force_index;		/* Prefer index over table scan */
  bool          ignore_leaves;          /* Preload only non-leaf nodes */
  bool		cacheable_table;	/* stop PS caching */
  /* used in multi-upd privelege check */
  bool		table_in_update_from_clause;
  /*
    Cleanup for re-execution in a prepared statement or a stored
    procedure.
  */
  void reinit_before_use(THD *thd);
} TABLE_LIST;

typedef struct st_changed_table_list
{
  struct	st_changed_table_list *next;
  char		*key;
  uint32        key_length;
} CHANGED_TABLE_LIST;

typedef struct st_open_table_list
{
  struct st_open_table_list *next;
  char	*db,*table;
  uint32 in_use,locked;
} OPEN_TABLE_LIST;


