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
class partition_info;
class COND_EQUAL;
class Security_context;

/* Order clause list element */

typedef struct st_order {
  struct st_order *next;
  Item	 **item;			/* Point at item in select fields */
  Item	 *item_ptr;			/* Storage for initial item */
  Item   **item_copy;			/* For SPs; the original item ptr */
  int    counter;                       /* position in SELECT list, correct
                                           only if counter_used is true*/
  bool	 asc;				/* true if ascending */
  bool	 free_me;			/* true if item isn't shared  */
  bool	 in_field_list;			/* true if in select field list */
  bool   counter_used;                  /* parameter was counter of columns */
  Field  *field;			/* If tmp-table group */
  char	 *buff;				/* If tmp-table group */
  table_map used, depend_map;
} ORDER;

typedef struct st_grant_info
{
  GRANT_TABLE *grant_table;
  uint version;
  ulong privilege;
  ulong want_privilege;
  /*
    Stores the requested access acl of top level tables list. Is used to
    check access rights to the underlying tables of a view.
  */
  ulong orig_want_privilege;
} GRANT_INFO;

enum tmp_table_type
{
  NO_TMP_TABLE, TMP_TABLE, TRANSACTIONAL_TMP_TABLE,
  INTERNAL_TMP_TABLE, SYSTEM_TMP_TABLE
};

enum frm_type_enum
{
  FRMTYPE_ERROR= 0,
  FRMTYPE_TABLE,
  FRMTYPE_VIEW
};

enum release_type { RELEASE_NORMAL, RELEASE_WAIT_FOR_DROP };

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

class Field_timestamp;
class Field_blob;
class Table_triggers_list;

/*
  This structure is shared between different table objects. There is one
  instance of table share per one table in the database.
*/

typedef struct st_table_share
{
  /* hash of field names (contains pointers to elements of field array) */
  HASH	name_hash;			/* hash of field names */
  MEM_ROOT mem_root;
  TYPELIB keynames;			/* Pointers to keynames */
  TYPELIB fieldnames;			/* Pointer to fieldnames */
  TYPELIB *intervals;			/* pointer to interval info */
  pthread_mutex_t mutex;                /* For locking the share  */
  pthread_cond_t cond;			/* To signal that share is ready */
  struct st_table_share *next,		/* Link to unused shares */
    **prev;
#ifdef NOT_YET
  struct st_table *open_tables;		/* link to open tables */
#endif

  /* The following is copied to each TABLE on OPEN */
  Field **field;
  Field **found_next_number_field;
  Field *timestamp_field;               /* Used only during open */
  KEY  *key_info;			/* data of keys in database */
  uint	*blob_field;			/* Index to blobs in Field arrray*/

  byte	*default_values;		/* row with default values */
  LEX_STRING comment;			/* Comment about table */
  CHARSET_INFO *table_charset;		/* Default charset of string fields */

  MY_BITMAP all_set;
  /* A pair "database_name\0table_name\0", widely used as simply a db name */
  LEX_STRING table_cache_key;
  LEX_STRING db;                        /* Pointer to db */
  LEX_STRING table_name;                /* Table name (for open) */
  LEX_STRING path;                	/* Path to .frm file (from datadir) */
  LEX_STRING normalized_path;		/* unpack_filename(path) */
  LEX_STRING connect_string;
  key_map keys_in_use;                  /* Keys in use for table */
  key_map keys_for_keyread;
  ha_rows min_rows, max_rows;		/* create information */
  ulong   avg_row_length;		/* create information */
  ulong   raid_chunksize;
  ulong   version, flush_version, mysql_version;
  ulong   timestamp_offset;		/* Set to offset+1 of record */
  ulong   reclength;			/* Recordlength */

  handlerton *db_type;			/* table_type for handler */
  enum row_type row_type;		/* How rows are stored */
  enum tmp_table_type tmp_table;

  uint ref_count;                       /* How many TABLE objects uses this */
  uint open_count;			/* Number of tables in open list */
  uint blob_ptr_size;			/* 4 or 8 */
  uint key_block_size;			/* create key_block_size, if used */
  uint null_bytes, last_null_bit_pos;
  uint fields;				/* Number of fields */
  uint rec_buff_length;                 /* Size of table->record[] buffer */
  uint keys, key_parts;
  uint max_key_length, max_unique_length, total_key_length;
  uint uniques;                         /* Number of UNIQUE index */
  uint null_fields;			/* number of null fields */
  uint blob_fields;			/* number of blob fields */
  uint timestamp_field_offset;		/* Field number for timestamp field */
  uint varchar_fields;                  /* number of varchar fields */
  uint db_create_options;		/* Create options from database */
  uint db_options_in_use;		/* Options in use */
  uint db_record_offset;		/* if HA_REC_IN_SEQ */
  uint raid_type, raid_chunks;
  uint rowid_field_offset;		/* Field_nr +1 to rowid field */
  /* Index of auto-updated TIMESTAMP field in field array */
  uint primary_key;
  uint next_number_index;
  uint next_number_key_offset;
  uint error, open_errno, errarg;       /* error from open_table_def() */
  uint column_bitmap_size;
  uchar frm_version;
  bool null_field_first;
  bool system;                          /* Set if system table (one record) */
  bool crypted;                         /* If .frm file is crypted */
  bool db_low_byte_first;		/* Portable row format */
  bool crashed;
  bool is_view;
  bool name_lock, replace_with_name_lock;
  bool waiting_on_cond;                 /* Protection against free */
  ulong table_map_id;                   /* for row-based replication */
  ulonglong table_map_version;

  /*
    Cache for row-based replication table share checks that does not
    need to be repeated. Possible values are: -1 when cache value is
    not calculated yet, 0 when table *shall not* be replicated, 1 when
    table *may* be replicated.
  */
  int cached_row_logging_check;

  /*
    TRUE if this is a system table like 'mysql.proc', which we want to be
    able to open and lock even when we already have some tables open and
    locked. To avoid deadlocks we have to put certain restrictions on
    locking of this table for writing. FALSE - otherwise.
  */
  bool system_table;
  /*
    This flag is set for the log tables. Used during FLUSH instances to skip
    log tables, while closing tables (since logs must be always available)
  */
  bool log_table;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  bool auto_partitioned;
  const uchar *partition_info;
  uint  partition_info_len;
  const uchar *part_state;
  uint part_state_len;
  handlerton *default_part_db_type;
#endif
} TABLE_SHARE;


/* Information for one open table */

struct st_table {
  st_table() {}                               /* Remove gcc warning */

  TABLE_SHARE	*s;
  handler	*file;
#ifdef NOT_YET
  struct st_table *used_next, **used_prev;	/* Link to used tables */
#endif
  struct st_table *open_next, **open_prev;	/* Link to open tables */
  struct st_table *next, *prev;

  THD	*in_use;                        /* Which thread uses this */
  Field **field;			/* Pointer to fields */

  byte *record[2];			/* Pointer to records */
  byte *write_row_record;		/* Used as optimisation in
					   THD::write_row */
  byte *insert_values;                  /* used by INSERT ... UPDATE */
  key_map quick_keys, used_keys, keys_in_use_for_query, merge_keys;
  KEY  *key_info;			/* data of keys in database */

  Field *next_number_field;		/* Set if next_number is activated */
  Field *found_next_number_field;	/* Set on open */
  Field_timestamp *timestamp_field;

  /* Table's triggers, 0 if there are no of them */
  Table_triggers_list *triggers;
  struct st_table_list *pos_in_table_list;/* Element referring to this table */
  ORDER		*group;
  const char	*alias;            	  /* alias or table name */
  uchar		*null_flags;
  my_bitmap_map	*bitmap_init_value;
  MY_BITMAP     def_read_set, def_write_set, tmp_set; /* containers */
  MY_BITMAP     *read_set, *write_set;          /* Active column sets */
  query_id_t	query_id;

  /* 
    For each key that has quick_keys.is_set(key) == TRUE: estimate of #records
    and max #key parts that range access would use.
  */
  ha_rows	quick_rows[MAX_KEY];

  /* Bitmaps of key parts that =const for the entire join. */
  key_part_map  const_key_parts[MAX_KEY];

  uint		quick_key_parts[MAX_KEY];
  uint		quick_n_ranges[MAX_KEY];

  /* 
    Estimate of number of records that satisfy SARGable part of the table
    condition, or table->file->records if no SARGable condition could be
    constructed.
    This value is used by join optimizer as an estimate of number of records
    that will pass the table condition (condition that depends on fields of 
    this table and constants)
  */
  ha_rows       quick_condition_rows;

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
  table_map	map;                    /* ID bit of table (1,2,4,8,16...) */

  uint          lock_position;          /* Position in MYSQL_LOCK.table */
  uint          lock_data_start;        /* Start pos. in MYSQL_LOCK.locks */
  uint          lock_count;             /* Number of locks */
  uint		tablenr,used_fields;
  uint          temp_pool_slot;		/* Used by intern temp tables */
  uint		status;                 /* What's in record[0] */
  uint		db_stat;		/* mode of file as in handler.h */
  /* number of select if it is derived table */
  uint          derived_select_number;
  int		current_lock;           /* Type of lock on table */
  my_bool copy_blobs;			/* copy_blobs when storing */

  /*
    0 or JOIN_TYPE_{LEFT|RIGHT}. Currently this is only compared to 0.
    If maybe_null !=0, this table is inner w.r.t. some outer join operation,
    and null_row may be true.
  */
  uint maybe_null;
  /*
    If true, the current table row is considered to have all columns set to 
    NULL, including columns declared as "not null" (see maybe_null).
  */
  my_bool null_row;
  my_bool force_index;
  my_bool distinct,const_table,no_rows;
  my_bool key_read, no_keyread;
  my_bool locked_by_flush;
  my_bool locked_by_logger;
  my_bool locked_by_name;
  my_bool fulltext_searched;
  my_bool no_cache;
  /* To signal that we should reset query_id for tables and cols */
  my_bool clear_query_id;
  my_bool auto_increment_field_not_null;
  my_bool insert_or_update;             /* Can be used by the handler */
  my_bool alias_name_used;		/* true if table_name is alias */
  my_bool get_fields_in_item_tree;      /* Signal to fix_field */

  REGINFO reginfo;			/* field connections */
  MEM_ROOT mem_root;
  GRANT_INFO grant;
  FILESORT_INFO sort;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  partition_info *part_info;            /* Partition related information */
  bool no_partitions_used; /* If true, all partitions have been pruned away */
#endif

  bool fill_item_list(List<Item> *item_list) const;
  void reset_item_list(List<Item> *item_list) const;
  void clear_column_bitmaps(void);
  void prepare_for_position(void);
  void mark_columns_used_by_index_no_reset(uint index, MY_BITMAP *map);
  void mark_columns_used_by_index(uint index);
  void restore_column_maps_after_mark_index();
  void mark_auto_increment_column(void);
  void mark_columns_needed_for_update(void);
  void mark_columns_needed_for_delete(void);
  void mark_columns_needed_for_insert(void);
  inline void column_bitmaps_set(MY_BITMAP *read_set_arg,
                                 MY_BITMAP *write_set_arg)
  {
    read_set= read_set_arg;
    write_set= write_set_arg;
    if (file)
      file->column_bitmaps_signal();
  }
  inline void column_bitmaps_set_no_signal(MY_BITMAP *read_set_arg,
                                           MY_BITMAP *write_set_arg)
  {
    read_set= read_set_arg;
    write_set= write_set_arg;
  }
  inline void use_all_columns()
  {
    column_bitmaps_set(&s->all_set, &s->all_set);
  }
  inline void default_column_bitmaps()
  {
    read_set= &def_read_set;
    write_set= &def_write_set;
  }

};


typedef struct st_foreign_key_info
{
  LEX_STRING *forein_id;
  LEX_STRING *referenced_db;
  LEX_STRING *referenced_table;
  LEX_STRING *update_method;
  LEX_STRING *delete_method;
  List<LEX_STRING> foreign_fields;
  List<LEX_STRING> referenced_fields;
} FOREIGN_KEY_INFO;

/*
  Make sure that the order of schema_tables and enum_schema_tables are the same.
*/

enum enum_schema_tables
{
  SCH_CHARSETS= 0,
  SCH_COLLATIONS,
  SCH_COLLATION_CHARACTER_SET_APPLICABILITY,
  SCH_COLUMNS,
  SCH_COLUMN_PRIVILEGES,
  SCH_ENGINES,
  SCH_EVENTS,
  SCH_FILES,
  SCH_KEY_COLUMN_USAGE,
  SCH_OPEN_TABLES,
  SCH_PARTITIONS,
  SCH_PLUGINS,
  SCH_PROCESSLIST,
  SCH_REFERENTIAL_CONSTRAINTS,
  SCH_PROCEDURES,
  SCH_SCHEMATA,
  SCH_SCHEMA_PRIVILEGES,
  SCH_STATISTICS,
  SCH_STATUS,
  SCH_TABLES,
  SCH_TABLE_CONSTRAINTS,
  SCH_TABLE_NAMES,
  SCH_TABLE_PRIVILEGES,
  SCH_TRIGGERS,
  SCH_USER_PRIVILEGES,
  SCH_VARIABLES,
  SCH_VIEWS
};


typedef struct st_field_info
{
  const char* field_name;
  uint field_length;
  enum enum_field_types field_type;
  int value;
  bool maybe_null;
  const char* old_name;
} ST_FIELD_INFO;


struct st_table_list;
typedef class Item COND;

typedef struct st_schema_table
{
  const char* table_name;
  ST_FIELD_INFO *fields_info;
  /* Create information_schema table */
  TABLE *(*create_table)  (THD *thd, struct st_table_list *table_list);
  /* Fill table with data */
  int (*fill_table) (THD *thd, struct st_table_list *tables, COND *cond);
  /* Handle fileds for old SHOW */
  int (*old_format) (THD *thd, struct st_schema_table *schema_table);
  int (*process_table) (THD *thd, struct st_table_list *tables,
                        TABLE *table, bool res, const char *base_name,
                        const char *file_name);
  int idx_field1, idx_field2; 
  bool hidden;
} ST_SCHEMA_TABLE;


#define JOIN_TYPE_LEFT	1
#define JOIN_TYPE_RIGHT	2

#define VIEW_ALGORITHM_UNDEFINED        0
#define VIEW_ALGORITHM_TMPTABLE         1
#define VIEW_ALGORITHM_MERGE            2

#define VIEW_SUID_INVOKER               0
#define VIEW_SUID_DEFINER               1
#define VIEW_SUID_DEFAULT               2

/* view WITH CHECK OPTION parameter options */
#define VIEW_CHECK_NONE       0
#define VIEW_CHECK_LOCAL      1
#define VIEW_CHECK_CASCADED   2

/* result of view WITH CHECK OPTION parameter check */
#define VIEW_CHECK_OK         0
#define VIEW_CHECK_ERROR      1
#define VIEW_CHECK_SKIP       2

struct st_lex;
class select_union;
class TMP_TABLE_PARAM;

Item *create_view_field(THD *thd, st_table_list *view, Item **field_ref,
                        const char *name);

struct Field_translator
{
  Item *item;
  const char *name;
};


/*
  Column reference of a NATURAL/USING join. Since column references in
  joins can be both from views and stored tables, may point to either a
  Field (for tables), or a Field_translator (for views).
*/

class Natural_join_column: public Sql_alloc
{
public:
  Field_translator *view_field;  /* Column reference of merge view. */
  Field            *table_field; /* Column reference of table or temp view. */
  st_table_list *table_ref; /* Original base table/view reference. */
  /*
    True if a common join column of two NATURAL/USING join operands. Notice
    that when we have a hierarchy of nested NATURAL/USING joins, a column can
    be common at some level of nesting but it may not be common at higher
    levels of nesting. Thus this flag may change depending on at which level
    we are looking at some column.
  */
  bool is_common;
public:
  Natural_join_column(Field_translator *field_param, st_table_list *tab);
  Natural_join_column(Field *field_param, st_table_list *tab);
  const char *name();
  Item *create_item(THD *thd);
  Field *field();
  const char *table_name();
  const char *db_name();
  GRANT_INFO *grant();
};


/*
  Table reference in the FROM clause.

  These table references can be of several types that correspond to
  different SQL elements. Below we list all types of TABLE_LISTs with
  the necessary conditions to determine when a TABLE_LIST instance
  belongs to a certain type.

  1) table (TABLE_LIST::view == NULL)
     - base table
       (TABLE_LIST::derived == NULL)
     - subquery - TABLE_LIST::table is a temp table
       (TABLE_LIST::derived != NULL)
     - information schema table
       (TABLE_LIST::schema_table != NULL)
       NOTICE: for schema tables TABLE_LIST::field_translation may be != NULL
  2) view (TABLE_LIST::view != NULL)
     - merge    (TABLE_LIST::effective_algorithm == VIEW_ALGORITHM_MERGE)
           also (TABLE_LIST::field_translation != NULL)
     - tmptable (TABLE_LIST::effective_algorithm == VIEW_ALGORITHM_TMPTABLE)
           also (TABLE_LIST::field_translation == NULL)
  3) nested table reference (TABLE_LIST::nested_join != NULL)
     - table sequence - e.g. (t1, t2, t3)
       TODO: how to distinguish from a JOIN?
     - general JOIN
       TODO: how to distinguish from a table sequence?
     - NATURAL JOIN
       (TABLE_LIST::natural_join != NULL)
       - JOIN ... USING
         (TABLE_LIST::join_using_fields != NULL)
*/

typedef struct st_table_list
{
  st_table_list() {}                          /* Remove gcc warning */
  /*
    List of tables local to a subquery (used by SQL_LIST). Considers
    views as leaves (unlike 'next_leaf' below). Created at parse time
    in st_select_lex::add_table_to_list() -> table_list.link_in_list().
  */
  struct st_table_list *next_local;
  /* link in a global list of all queries tables */
  struct st_table_list *next_global, **prev_global;
  char		*db, *alias, *table_name, *schema_table_name;
  char          *option;                /* Used by cache index  */
  Item		*on_expr;		/* Used with outer join */
  /*
    The structure of ON expression presented in the member above
    can be changed during certain optimizations. This member
    contains a snapshot of AND-OR structure of the ON expression
    made after permanent transformations of the parse tree, and is
    used to restore ON clause before every reexecution of a prepared
    statement or stored procedure.
  */
  Item          *prep_on_expr;
  COND_EQUAL    *cond_equal;            /* Used with outer join */
  /*
    During parsing - left operand of NATURAL/USING join where 'this' is
    the right operand. After parsing (this->natural_join == this) iff
    'this' represents a NATURAL or USING join operation. Thus after
    parsing 'this' is a NATURAL/USING join iff (natural_join != NULL).
  */
  struct st_table_list *natural_join;
  /*
    True if 'this' represents a nested join that is a NATURAL JOIN.
    For one of the operands of 'this', the member 'natural_join' points
    to the other operand of 'this'.
  */
  bool is_natural_join;
  /* Field names in a USING clause for JOIN ... USING. */
  List<String> *join_using_fields;
  /*
    Explicitly store the result columns of either a NATURAL/USING join or
    an operand of such a join.
  */
  List<Natural_join_column> *join_columns;
  /* TRUE if join_columns contains all columns of this table reference. */
  bool is_join_columns_complete;

  /*
    List of nodes in a nested join tree, that should be considered as
    leaves with respect to name resolution. The leaves are: views,
    top-most nodes representing NATURAL/USING joins, subqueries, and
    base tables. All of these TABLE_LIST instances contain a
    materialized list of columns. The list is local to a subquery.
  */
  struct st_table_list *next_name_resolution_table;
  /* Index names in a "... JOIN ... USE/IGNORE INDEX ..." clause. */
  List<String> *use_index, *ignore_index;
  TABLE        *table;                          /* opened table */
  uint          table_id; /* table id (from binlog) for opened table */
  /*
    select_result for derived table to pass it from table creation to table
    filling procedure
  */
  select_union  *derived_result;
  /*
    Reference from aux_tables to local list entry of main select of
    multi-delete statement:
    delete t1 from t2,t1 where t1.a<'B' and t2.b=t1.b;
    here it will be reference of first occurrence of t1 to second (as you
    can see this lists can't be merged)
  */
  st_table_list	*correspondent_table;
  st_select_lex_unit *derived;		/* SELECT_LEX_UNIT of derived table */
  ST_SCHEMA_TABLE *schema_table;        /* Information_schema table */
  st_select_lex	*schema_select_lex;
  bool is_schema_table_processed;
  /*
    True when the view field translation table is used to convert
    schema table fields for backwards compatibility with SHOW command.
  */
  bool schema_table_reformed;
  TMP_TABLE_PARAM *schema_table_param;
  /* link to select_lex where this table was used */
  st_select_lex	*select_lex;
  st_lex	*view;			/* link on VIEW lex for merging */
  Field_translator *field_translation;	/* array of VIEW fields */
  /* pointer to element after last one in translation table above */
  Field_translator *field_translation_end;
  /*
    List (based on next_local) of underlying tables of this view. I.e. it
    does not include the tables of subqueries used in the view. Is set only
    for merged views.
  */
  st_table_list	*merge_underlying_list;
  /*
    - 0 for base tables
    - in case of the view it is the list of all (not only underlying
    tables but also used in subquery ones) tables of the view.
  */
  List<st_table_list> *view_tables;
  /* most upper view this table belongs to */
  st_table_list	*belong_to_view;
  /*
    The view directly referencing this table
    (non-zero only for merged underlying tables of a view).
  */
  st_table_list	*referencing_view;
  /*
    Security  context (non-zero only for tables which belong
    to view with SQL SECURITY DEFINER)
  */
  Security_context *security_ctx;
  /*
    This view security context (non-zero only for views with
    SQL SECURITY DEFINER)
  */
  Security_context *view_sctx;
  /*
    List of all base tables local to a subquery including all view
    tables. Unlike 'next_local', this in this list views are *not*
    leaves. Created in setup_tables() -> make_leaves_list().
  */
  st_table_list	*next_leaf;
  Item          *where;                 /* VIEW WHERE clause condition */
  Item          *check_option;          /* WITH CHECK OPTION condition */
  LEX_STRING	query;			/* text of (CRETE/SELECT) statement */
  LEX_STRING	md5;			/* md5 of query text */
  LEX_STRING	source;			/* source of CREATE VIEW */
  LEX_STRING	view_db;		/* saved view database */
  LEX_STRING	view_name;		/* saved view name */
  LEX_STRING	timestamp;		/* GMT time stamp of last operation */
  st_lex_user   definer;                /* definer of view */
  ulonglong	file_version;		/* version of file's field set */
  ulonglong     updatable_view;         /* VIEW can be updated */
  ulonglong	revision;		/* revision control number */
  ulonglong	algorithm;		/* 0 any, 1 tmp tables , 2 merging */
  ulonglong     view_suid;              /* view is suid (TRUE dy default) */
  ulonglong     with_check;             /* WITH CHECK OPTION */
  /*
    effective value of WITH CHECK OPTION (differ for temporary table
    algorithm)
  */
  uint8         effective_with_check;
  uint8         effective_algorithm;    /* which algorithm was really used */
  GRANT_INFO	grant;
  /* data need by some engines in query cache*/
  ulonglong     engine_data;
  /* call back function for asking handler about caching in query cache */
  qc_engine_callback callback_func;
  thr_lock_type lock_type;
  uint		outer_join;		/* Which join type */
  uint		shared;			/* Used in multi-upd */
  uint          db_length;
  uint32        table_name_length;
  bool          updatable;		/* VIEW/TABLE can be updated now */
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
  /* used in multi-upd/views privilege check */
  bool		table_in_first_from_clause;
  bool		skip_temporary;		/* this table shouldn't be temporary */
  /* TRUE if this merged view contain auto_increment field */
  bool          contain_auto_increment;
  bool          multitable_view;        /* TRUE iff this is multitable view */
  bool          compact_view_format;    /* Use compact format for SHOW CREATE VIEW */
  /* view where processed */
  bool          where_processed;
  /* FRMTYPE_ERROR if any type is acceptable */
  enum frm_type_enum required_type;
  handlerton	*db_type;		/* table_type for handler */
  char		timestamp_buffer[20];	/* buffer for timestamp (19+1) */
  /*
    This TABLE_LIST object is just placeholder for prelocking, it will be
    used for implicit LOCK TABLES only and won't be used in real statement.
  */
  bool          prelocking_placeholder;

  void calc_md5(char *buffer);
  void set_underlying_merge();
  int view_check_option(THD *thd, bool ignore_failure);
  bool setup_underlying(THD *thd);
  void cleanup_items();
  bool placeholder() {return derived || view; }
  void print(THD *thd, String *str);
  bool check_single_table(st_table_list **table, table_map map,
                          st_table_list *view);
  bool set_insert_values(MEM_ROOT *mem_root);
  void hide_view_error(THD *thd);
  st_table_list *find_underlying_table(TABLE *table);
  st_table_list *first_leaf_for_name_resolution();
  st_table_list *last_leaf_for_name_resolution();
  bool is_leaf_for_name_resolution();
  inline st_table_list *top_table()
    { return belong_to_view ? belong_to_view : this; }
  inline bool prepare_check_option(THD *thd)
  {
    bool res= FALSE;
    if (effective_with_check)
      res= prep_check_option(thd, effective_with_check);
    return res;
  }
  inline bool prepare_where(THD *thd, Item **conds,
                            bool no_where_clause)
  {
    if (effective_algorithm == VIEW_ALGORITHM_MERGE)
      return prep_where(thd, conds, no_where_clause);
    return FALSE;
  }

  void register_want_access(ulong want_access);
  bool prepare_security(THD *thd);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *find_view_security_context(THD *thd);
  bool prepare_view_securety_context(THD *thd);
#endif
  /*
    Cleanup for re-execution in a prepared statement or a stored
    procedure.
  */
  void reinit_before_use(THD *thd);

private:
  bool prep_check_option(THD *thd, uint8 check_opt_type);
  bool prep_where(THD *thd, Item **conds, bool no_where_clause);
  /*
    Cleanup for re-execution in a prepared statement or a stored
    procedure.
  */
} TABLE_LIST;

class Item;

/*
  Iterator over the fields of a generic table reference.
*/

class Field_iterator: public Sql_alloc
{
public:
  Field_iterator() {}                         /* Remove gcc warning */
  virtual ~Field_iterator() {}
  virtual void set(TABLE_LIST *)= 0;
  virtual void next()= 0;
  virtual bool end_of_fields()= 0;              /* Return 1 at end of list */
  virtual const char *name()= 0;
  virtual Item *create_item(THD *)= 0;
  virtual Field *field()= 0;
};


/* 
  Iterator over the fields of a base table, view with temporary
  table, or subquery.
*/

class Field_iterator_table: public Field_iterator
{
  Field **ptr;
public:
  Field_iterator_table() :ptr(0) {}
  void set(TABLE_LIST *table) { ptr= table->table->field; }
  void set_table(TABLE *table) { ptr= table->field; }
  void next() { ptr++; }
  bool end_of_fields() { return *ptr == 0; }
  const char *name();
  Item *create_item(THD *thd);
  Field *field() { return *ptr; }
};


/* Iterator over the fields of a merge view. */

class Field_iterator_view: public Field_iterator
{
  Field_translator *ptr, *array_end;
  TABLE_LIST *view;
public:
  Field_iterator_view() :ptr(0), array_end(0) {}
  void set(TABLE_LIST *table);
  void next() { ptr++; }
  bool end_of_fields() { return ptr == array_end; }
  const char *name();
  Item *create_item(THD *thd);
  Item **item_ptr() {return &ptr->item; }
  Field *field() { return 0; }
  inline Item *item() { return ptr->item; }
  Field_translator *field_translator() { return ptr; }
};


/*
  Field_iterator interface to the list of materialized fields of a
  NATURAL/USING join.
*/

class Field_iterator_natural_join: public Field_iterator
{
  List_iterator_fast<Natural_join_column> column_ref_it;
  Natural_join_column *cur_column_ref;
public:
  Field_iterator_natural_join() :cur_column_ref(NULL) {}
  ~Field_iterator_natural_join() {}
  void set(TABLE_LIST *table);
  void next();
  bool end_of_fields() { return !cur_column_ref; }
  const char *name() { return cur_column_ref->name(); }
  Item *create_item(THD *thd) { return cur_column_ref->create_item(thd); }
  Field *field() { return cur_column_ref->field(); }
  Natural_join_column *column_ref() { return cur_column_ref; }
};


/*
  Generic iterator over the fields of an arbitrary table reference.

  DESCRIPTION
    This class unifies the various ways of iterating over the columns
    of a table reference depending on the type of SQL entity it
    represents. If such an entity represents a nested table reference,
    this iterator encapsulates the iteration over the columns of the
    members of the table reference.

  IMPLEMENTATION
    The implementation assumes that all underlying NATURAL/USING table
    references already contain their result columns and are linked into
    the list TABLE_LIST::next_name_resolution_table.
*/

class Field_iterator_table_ref: public Field_iterator
{
  TABLE_LIST *table_ref, *first_leaf, *last_leaf;
  Field_iterator_table        table_field_it;
  Field_iterator_view         view_field_it;
  Field_iterator_natural_join natural_join_it;
  Field_iterator *field_it;
  void set_field_iterator();
public:
  Field_iterator_table_ref() :field_it(NULL) {}
  void set(TABLE_LIST *table);
  void next();
  bool end_of_fields()
  { return (table_ref == last_leaf && field_it->end_of_fields()); }
  const char *name() { return field_it->name(); }
  const char *table_name();
  const char *db_name();
  GRANT_INFO *grant();
  Item *create_item(THD *thd) { return field_it->create_item(thd); }
  Field *field() { return field_it->field(); }
  Natural_join_column *get_or_create_column_ref(TABLE_LIST *parent_table_ref);
  Natural_join_column *get_natural_column_ref();
};


typedef struct st_nested_join
{
  List<TABLE_LIST>  join_list;       /* list of elements in the nested join */
  table_map         used_tables;     /* bitmap of tables in the nested join */
  table_map         not_null_tables; /* tables that rejects nulls           */
  struct st_join_table *first_nested;/* the first nested table in the plan  */
  /* 
    Used to count tables in the nested join in 2 isolated places:
    1. In make_outerjoin_info(). 
    2. check_interleaving_with_nj/restore_prev_nj_state (these are called
       by the join optimizer. 
    Before each use the counters are zeroed by reset_nj_counters.
  */
  uint              counter;
  nested_join_map   nj_map;          /* Bit used to identify this nested join*/
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

typedef struct st_table_field_w_type
{
  LEX_STRING name;
  LEX_STRING type;
  LEX_STRING cset;
} TABLE_FIELD_W_TYPE;


my_bool
table_check_intact(TABLE *table, const uint table_f_count,
                   const TABLE_FIELD_W_TYPE * const table_def,
                   time_t *last_create_time, int error_num);

static inline my_bitmap_map *tmp_use_all_columns(TABLE *table,
                                                 MY_BITMAP *bitmap)
{
  my_bitmap_map *old= bitmap->bitmap;
  bitmap->bitmap= table->s->all_set.bitmap;
  return old;
}


static inline void tmp_restore_column_map(MY_BITMAP *bitmap,
                                          my_bitmap_map *old)
{
  bitmap->bitmap= old;
}

/* The following is only needed for debugging */

static inline my_bitmap_map *dbug_tmp_use_all_columns(TABLE *table,
                                                      MY_BITMAP *bitmap)
{
#ifndef DBUG_OFF
  return tmp_use_all_columns(table, bitmap);
#else
  return 0;
#endif
}

static inline void dbug_tmp_restore_column_map(MY_BITMAP *bitmap,
                                               my_bitmap_map *old)
{
#ifndef DBUG_OFF
  tmp_restore_column_map(bitmap, old);
#endif
}
