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
class st_select_lex;

/* Order clause list element */

typedef struct st_order {
  struct st_order *next;
  Item	 **item;			/* Point at item in select fields */
  Item	 *item_ptr;			/* Storage for initial item */
  Item   **item_copy;			/* For SPs; the original item ptr */
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


/* Table cache entry struct */

class Field_timestamp;
class Field_blob;
class Table_triggers_list;

struct st_table {
  handler *file;
  Field **field;			/* Pointer to fields */
  Field_blob **blob_field;		/* Pointer to blob fields */
  /* hash of field names (contains pointers to elements of field array) */
  HASH	name_hash;
  byte *record[2];			/* Pointer to records */
  byte *default_values;         	/* Default values for INSERT */
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
  key_map quick_keys, used_keys, keys_in_use_for_query;
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
    These two members hold offset in record + 1 for TIMESTAMP field
    with NOW() as default value or/and with ON UPDATE NOW() option. 
    If 0 then such field is absent in this table or auto-set for default
    or/and on update should be temporaly disabled for some reason.
    These values is setup to offset value for each statement in open_table()
    and turned off in statement processing code (see mysql_update as example).
  */
  ulong timestamp_default_now;
  ulong timestamp_on_update_now;
  /* Index of auto-updated TIMESTAMP field in field array */
  uint timestamp_field_offset;
  
  uint next_number_index;
  uint blob_ptr_size;			/* 4 or 8 */
  uint next_number_key_offset;
  int current_lock;			/* Type of lock on table */
  enum tmp_table_type tmp_table;
  my_bool copy_blobs;			/* copy_blobs when storing */
  my_bool null_row;			/* All columns are null */
  my_bool maybe_null,outer_join;	/* Used with OUTER JOIN */
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
  my_string comment;			/* Comment about table */
  CHARSET_INFO *table_charset;		/* Default charset of string fields */
  REGINFO reginfo;			/* field connections */
  MEM_ROOT mem_root;
  GRANT_INFO grant;
  /* Table's triggers, 0 if there are no of them */
  Table_triggers_list *triggers;

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
  uint          temp_pool_slot;		/* Used by intern temp tables */
  struct st_table_list *pos_in_table_list;/* Element referring to this table */
  /* number of select if it is derived table */
  uint          derived_select_number;
  THD		*in_use;		/* Which thread uses this */
  struct st_table *next,*prev;
};


#define JOIN_TYPE_LEFT	1
#define JOIN_TYPE_RIGHT	2

#define VIEW_ALGORITHM_UNDEFINED	0
#define VIEW_ALGORITHM_TMEPTABLE	1
#define VIEW_ALGORITHM_MERGE		2

struct st_lex;

typedef struct st_table_list
{
  /* link in a local table list (used by SQL_LIST) */
  struct st_table_list *next_local;
  /* link in a global list of all queries tables */
  struct st_table_list *next_global, **prev_global;
  char		*db, *alias, *real_name;
  char          *option;                /* Used by cache index  */
  Item		*on_expr;		/* Used with outer join */
  struct st_table_list *natural_join;	/* natural join on this table*/
  /* ... join ... USE INDEX ... IGNORE INDEX */
  List<String>	*use_index, *ignore_index;
  TABLE         *table;                 /* opened table */
  /*
    Reference from aux_tables to local list entry of main select of
    multi-delete statement:
    delete t1 from t2,t1 where t1.a<'B' and t2.b=t1.b;
    here it will be reference of first occurrence of t1 to second (as you
    can see this lists can't be merged)
  */
  st_table_list	*correspondent_table;
  st_select_lex_unit *derived;		/* SELECT_LEX_UNIT of derived table */
  /* link to select_lex where this table was used */
  st_select_lex	*select_lex;
  st_lex	*view;			/* link on VIEW lex for merging */
  Item		**field_translation;	/* array of VIEW fields */
  /* ancestor of this table (VIEW merge algorithm) */
  st_table_list	*ancestor;
  Item          *where;                 /* VIEW WHERE clause condition */
  LEX_STRING	query;			/* text of (CRETE/SELECT) statement */
  LEX_STRING	md5;			/* md5 of query tesxt */
  LEX_STRING	source;			/* source of CREATE VIEW */
  LEX_STRING	view_db;		/* save view database */
  LEX_STRING	view_name;		/* save view name */
  LEX_STRING	timestamp;		/* GMT time stamp of last operation */
  ulonglong	file_version;		/* version of file's field set */
  ulonglong	revision;		/* revision control number */
  ulonglong	updatable;		/* Is VIEW updateable */
  ulonglong	algorithm;		/* 0 any, 1 tmp tables , 2 merging */
  uint          effective_algorithm;    /* which algorithm was really used */
  GRANT_INFO	grant;
  thr_lock_type lock_type;
  uint		outer_join;		/* Which join type */
  uint		shared;			/* Used in multi-upd */
  uint32        db_length, real_name_length;
  bool		straight;		/* optimize with prev table */
  bool          updating;               /* for replicate-do/ignore table */
  bool		force_index;		/* prefer index over table scan */
  bool          ignore_leaves;          /* preload only non-leaf nodes */
  table_map     dep_tables;             /* tables the table depends on      */
  table_map     on_expr_dep_tables;     /* tables on expression depends on  */
  struct st_nested_join *nested_join;   /* if the element is a nested join  */
  st_table_list *embedding;             /* nested join containing the table */
  List<struct st_table_list> *join_list;/* join list the table belongs to   */
  bool		cacheable_table;	/* stop PS caching */
  /* used in multi-upd/views privelege check */
  bool		table_in_first_from_clause;
  bool		skip_temporary;		/* this table shouldn't be temporary */
  bool          setup_is_done;          /* setup_tables() is done */
  /* do view contain auto_increment field */
  bool          contain_auto_increment;
  char		timestamp_buffer[20];	/* buffer for timestamp (19+1) */

  void calc_md5(char *buffer);
  void set_ancestor();
  bool setup_ancestor(THD *thd, Item **conds);
  bool placeholder() {return derived || view; }
  void print(THD *thd, String *str);
} TABLE_LIST;

class Item;

class Field_iterator: public Sql_alloc
{
public:
  virtual ~Field_iterator() {}
  virtual void set(TABLE_LIST *)= 0;
  virtual void next()= 0;
  virtual bool end()= 0;
  virtual const char *name()= 0;
  virtual Item *item(THD *)= 0;
  virtual Field *field()= 0;
};


class Field_iterator_table: public Field_iterator
{
  Field **ptr;
public:
  Field_iterator_table() :ptr(0) {}
  void set(TABLE_LIST *table) { ptr= table->table->field; }
  void set_table(TABLE *table) { ptr= table->field; }
  void next() { ptr++; }
  bool end() { return test(*ptr); }
  const char *name();
  Item *item(THD *thd);
  Field *field() { return *ptr; }
};


class Field_iterator_view: public Field_iterator
{
  Item **ptr, **array_end;
public:
  Field_iterator_view() :ptr(0), array_end(0) {}
  void set(TABLE_LIST *table);
  void next() { ptr++; }
  bool end() { return ptr < array_end; }
  const char *name();
  Item *item(THD *thd) { return *ptr; }
  Field *field() { return 0; }
};

typedef struct st_nested_join
{
  List<TABLE_LIST>  join_list;       /* list of elements in the nested join */
  table_map         used_tables;     /* bitmap of tables in the nested join */
  table_map         not_null_tables; /* tables that rejects nulls           */
  struct st_join_table *first_nested;/* the first nested table in the plan  */
  uint              counter;         /* to count tables in the nested join  */
} NESTED_JOIN;

typedef struct st_changed_table_list
{
  struct	st_changed_table_list *next;
  char		*key;
  uint32        key_length;
} CHANGED_TABLE_LIST;

typedef struct st_open_table_list{
  struct st_open_table_list *next;
  char	*db,*table;
  uint32 in_use,locked;
} OPEN_TABLE_LIST;


