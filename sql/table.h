#ifndef TABLE_INCLUDED
#define TABLE_INCLUDED

/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */

#ifndef MYSQL_CLIENT

#include "hash.h"          // HASH
#include "datadict.h"      // frm_type_enum
#include "handler.h"       // row_type
#include "mdl.h"           // MDL_wait_for_subgraph
#include "opt_costmodel.h" // Cost_model_table
#include "sql_bitmap.h"    // Bitmap
#include "sql_sort.h"      // Filesort_info
#include "table_id.h"      // Table_id
#include "lock.h"          // Tablespace_hash_set

/* Structs that defines the TABLE */
class File_parser;
class Item_subselect;
class Item_field;
class GRANT_TABLE;
class st_select_lex_unit;
class COND_EQUAL;
class Security_context;
class ACL_internal_schema_access;
class ACL_internal_table_access;
class Table_cache_element;
class Table_trigger_dispatcher;
class Query_result_union;
class Temp_table_param;
class Index_hint;
struct Name_resolution_context;
struct LEX;
typedef int8 plan_idx;
class Opt_hints_qb;
class Opt_hints_table;

#define store_record(A,B) memcpy((A)->B,(A)->record[0],(size_t) (A)->s->reclength)
#define restore_record(A,B) memcpy((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define cmp_record(A,B) memcmp((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define empty_record(A) { \
                          restore_record((A),s->default_values); \
                          memset((A)->null_flags, 255, (A)->s->null_bytes);\
                        }

/*
  Used to identify NESTED_JOIN structures within a join (applicable to
  structures representing outer joins that have not been simplified away).
*/
typedef ulonglong nested_join_map;


#define tmp_file_prefix "#sql"			/**< Prefix for tmp tables */
#define tmp_file_prefix_length 4
#define TMP_TABLE_KEY_EXTRA 8

/**
  Enumerate possible types of a table from re-execution
  standpoint.
  TABLE_LIST class has a member of this type.
  At prepared statement prepare, this member is assigned a value
  as of the current state of the database. Before (re-)execution
  of a prepared statement, we check that the value recorded at
  prepare matches the type of the object we obtained from the
  table definition cache.

  @sa check_and_update_table_version()
  @sa Execute_observer
  @sa Prepared_statement::reprepare()
*/

enum enum_table_ref_type
{
  /** Initial value set by the parser */
  TABLE_REF_NULL= 0,
  TABLE_REF_VIEW,
  TABLE_REF_BASE_TABLE,
  TABLE_REF_I_S_TABLE,
  TABLE_REF_TMP_TABLE
};

/**
 Enumerate possible status of a identifier name while determining
 its validity
*/
enum enum_ident_name_check
{
  IDENT_NAME_OK,
  IDENT_NAME_WRONG,
  IDENT_NAME_TOO_LONG
};

/*************************************************************************/

/**
 Object_creation_ctx -- interface for creation context of database objects
 (views, stored routines, events, triggers). Creation context -- is a set
 of attributes, that should be fixed at the creation time and then be used
 each time the object is parsed or executed.
*/

class Object_creation_ctx
{
public:
  Object_creation_ctx *set_n_backup(THD *thd);

  void restore_env(THD *thd, Object_creation_ctx *backup_ctx);

protected:
  Object_creation_ctx() {}
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const = 0;

  virtual void change_env(THD *thd) const = 0;

public:
  virtual ~Object_creation_ctx()
  { }
};

/*************************************************************************/

/**
 Default_object_creation_ctx -- default implementation of
 Object_creation_ctx.
*/

class Default_object_creation_ctx : public Object_creation_ctx
{
public:
  const CHARSET_INFO *get_client_cs()
  {
    return m_client_cs;
  }

  const CHARSET_INFO *get_connection_cl()
  {
    return m_connection_cl;
  }

protected:
  Default_object_creation_ctx(THD *thd);

  Default_object_creation_ctx(const CHARSET_INFO *client_cs,
                              const CHARSET_INFO *connection_cl);

protected:
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const;

  virtual void change_env(THD *thd) const;

protected:
  /**
    client_cs stores the value of character_set_client session variable.
    The only character set attribute is used.

    Client character set is included into query context, because we save
    query in the original character set, which is client character set. So,
    in order to parse the query properly we have to switch client character
    set on parsing.
  */
  const CHARSET_INFO *m_client_cs;

  /**
    connection_cl stores the value of collation_connection session
    variable. Both character set and collation attributes are used.

    Connection collation is included into query context, becase it defines
    the character set and collation of text literals in internal
    representation of query (item-objects).
  */
  const CHARSET_INFO *m_connection_cl;
};


/**
 View_creation_ctx -- creation context of view objects.
*/

class View_creation_ctx : public Default_object_creation_ctx,
                          public Sql_alloc
{
public:
  static View_creation_ctx *create(THD *thd);

  static View_creation_ctx *create(THD *thd,
                                   TABLE_LIST *view);

private:
  View_creation_ctx(THD *thd)
    : Default_object_creation_ctx(thd)
  { }
};

/*************************************************************************/

/** Order clause list element */

typedef struct st_order {
  struct st_order *next;
  Item   **item;                        /* Point at item in select fields */
  Item   *item_ptr;                     /* Storage for initial item */

  enum enum_order {
    ORDER_NOT_RELEVANT,
    ORDER_ASC,
    ORDER_DESC
  };

  enum_order direction;                 /* Requested direction of ordering */
  bool   in_field_list;                 /* true if in select field list */
  /**
     Tells whether this ORDER element was referenced with an alias or with an
     expression, in the query:
     SELECT a AS foo GROUP BY foo: true.
     SELECT a AS foo GROUP BY a: false.
  */
  bool   used_alias;
  Field  *field;                        /* If tmp-table group */
  char   *buff;                         /* If tmp-table group */
  table_map used, depend_map;
  bool is_position;  /* An item expresses a position in a ORDER clause */
} ORDER;

/**
  State information for internal tables grants.
  This structure is part of the TABLE_LIST, and is updated
  during the ACL check process.
  @sa GRANT_INFO
*/
struct st_grant_internal_info
{
  /** True if the internal lookup by schema name was done. */
  bool m_schema_lookup_done;
  /** Cached internal schema access. */
  const ACL_internal_schema_access *m_schema_access;
  /** True if the internal lookup by table name was done. */
  bool m_table_lookup_done;
  /** Cached internal table access. */
  const ACL_internal_table_access *m_table_access;
};
typedef struct st_grant_internal_info GRANT_INTERNAL_INFO;

/**
   @brief The current state of the privilege checking process for the current
   user, SQL statement and SQL object.

   @details The privilege checking process is divided into phases depending on
   the level of the privilege to be checked and the type of object to be
   accessed. Due to the mentioned scattering of privilege checking
   functionality, it is necessary to keep track of the state of the
   process. This information is stored in privilege and want_privilege.

   A GRANT_INFO also serves as a cache of the privilege hash tables. Relevant
   members are grant_table and version.
 */
struct GRANT_INFO
{
  GRANT_INFO();
  /**
     @brief A copy of the privilege information regarding the current host,
     database, object and user.

     @details The version of this copy is found in GRANT_INFO::version.
   */
  GRANT_TABLE *grant_table;
  /**
     @brief Used for cache invalidation when caching privilege information.

     @details The privilege information is stored on disk, with dedicated
     caches residing in memory: table-level and column-level privileges,
     respectively, have their own dedicated caches.

     The GRANT_INFO works as a level 1 cache with this member updated to the
     current value of the global variable @c grant_version (@c static variable
     in sql_acl.cc). It is updated Whenever the GRANT_INFO is refreshed from
     the level 2 cache. The level 2 cache is the @c column_priv_hash structure
     (@c static variable in sql_acl.cc)

     @see grant_version
   */
  uint version;
  /**
     @brief The set of privileges that the current user has fulfilled for a
     certain host, database, and object.
     
     @details This field is continually updated throughout the access checking
     process. In each step the "wanted privilege" is checked against the
     fulfilled privileges. When/if the intersection of these sets is empty,
     access is granted.

     The set is implemented as a bitmap, with the bits defined in sql_acl.h.
   */
  ulong privilege;
#ifndef DBUG_OFF
  /**
     @brief the set of privileges that the current user needs to fulfil in
     order to carry out the requested operation. Used in debug build to
     ensure individual column privileges are assigned consistently.
     @todo remove this member in 5.8.
   */
  ulong want_privilege;
#endif
  /** The grant state for internal tables. */
  GRANT_INTERNAL_INFO m_internal;
};

enum tmp_table_type
{
  NO_TMP_TABLE, NON_TRANSACTIONAL_TMP_TABLE, TRANSACTIONAL_TMP_TABLE,
  INTERNAL_TMP_TABLE, SYSTEM_TMP_TABLE
};


/**
  Category of table found in the table share.
*/
enum enum_table_category
{
  /**
    Unknown value.
  */
  TABLE_UNKNOWN_CATEGORY=0,

  /**
    Temporary table.
    The table is visible only in the session.
    Therefore,
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    do not apply to this table.
    Note that LOCK TABLE t FOR READ/WRITE
    can be used on temporary tables.
    Temporary tables are not part of the table cache.
  */
  TABLE_CATEGORY_TEMPORARY=1,

  /**
    User table.
    These tables do honor:
    - LOCK TABLE t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    User tables are cached in the table cache.
  */
  TABLE_CATEGORY_USER=2,

  /**
    System table, maintained by the server.
    These tables do honor:
    - LOCK TABLE t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    Typically, writes to system tables are performed by
    the server implementation, not explicitly be a user.
    System tables are cached in the table cache.
  */
  TABLE_CATEGORY_SYSTEM=3,

  /**
    Information schema tables.
    These tables are an interface provided by the system
    to inspect the system metadata.
    These tables do *not* honor:
    - LOCK TABLE t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    as there is no point in locking explicitly
    an INFORMATION_SCHEMA table.
    Nothing is directly written to information schema tables.
    Note that this value is not used currently,
    since information schema tables are not shared,
    but implemented as session specific temporary tables.
  */
  /*
    TODO: Fixing the performance issues of I_S will lead
    to I_S tables in the table cache, which should use
    this table type.
  */
  TABLE_CATEGORY_INFORMATION=4,

  /**
    Log tables.
    These tables are an interface provided by the system
    to inspect the system logs.
    These tables do *not* honor:
    - LOCK TABLE t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    as there is no point in locking explicitly
    a LOG table.
    An example of LOG tables are:
    - mysql.slow_log
    - mysql.general_log,
    which *are* updated even when there is either
    a GLOBAL READ LOCK or a GLOBAL READ_ONLY in effect.
    User queries do not write directly to these tables
    (there are exceptions for log tables).
    The server implementation perform writes.
    Log tables are cached in the table cache.
  */
  TABLE_CATEGORY_LOG=5,

  /**
    Performance schema tables.
    These tables are an interface provided by the system
    to inspect the system performance data.
    These tables do *not* honor:
    - LOCK TABLE t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    as there is no point in locking explicitly
    a PERFORMANCE_SCHEMA table.
    An example of PERFORMANCE_SCHEMA tables are:
    - performance_schema.*
    which *are* updated (but not using the handler interface)
    even when there is either
    a GLOBAL READ LOCK or a GLOBAL READ_ONLY in effect.
    User queries do not write directly to these tables
    (there are exceptions for SETUP_* tables).
    The server implementation perform writes.
    Performance tables are cached in the table cache.
  */
  TABLE_CATEGORY_PERFORMANCE=6,

  /**
    Replication Information Tables.
    These tables are used to store replication information.
    These tables do *not* honor:
    - LOCK TABLE t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    as there is no point in locking explicitly
    a Replication Information table.
    An example of replication tables are:
    - mysql.slave_master_info
    - mysql.slave_relay_log_info,
    which *are* updated even when there is either
    a GLOBAL READ LOCK or a GLOBAL READ_ONLY in effect.
    User queries do not write directly to these tables.
    Replication tables are cached in the table cache.
  */
  TABLE_CATEGORY_RPL_INFO=7,

  /**
    Gtid Table.
    The table is used to store gtids.
    The table does *not* honor:
    - LOCK TABLE t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    as there is no point in locking explicitly
    a Gtid table.
    An example of gtid_executed table is:
    - mysql.gtid_executed,
    which is updated even when there is either
    a GLOBAL READ LOCK or a GLOBAL READ_ONLY in effect.
    Gtid table is cached in the table cache.
  */
  TABLE_CATEGORY_GTID=8
};
typedef enum enum_table_category TABLE_CATEGORY;

extern ulong refresh_version;

typedef struct st_table_field_type
{
  LEX_STRING name;
  LEX_STRING type;
  LEX_STRING cset;
} TABLE_FIELD_TYPE;


typedef struct st_table_field_def
{
  uint count;
  const TABLE_FIELD_TYPE *field;
} TABLE_FIELD_DEF;


class Table_check_intact
{
protected:
  bool has_keys;
  virtual void report_error(uint code, const char *fmt, ...)= 0;

public:
  Table_check_intact() : has_keys(FALSE) {}
  virtual ~Table_check_intact() {}

  /** Checks whether a table is intact. */
  bool check(TABLE *table, const TABLE_FIELD_DEF *table_def);
};


/**
  Class representing the fact that some thread waits for table
  share to be flushed. Is used to represent information about
  such waits in MDL deadlock detector.
*/

class Wait_for_flush : public MDL_wait_for_subgraph
{
  MDL_context *m_ctx;
  TABLE_SHARE *m_share;
  uint m_deadlock_weight;
public:
  Wait_for_flush(MDL_context *ctx_arg, TABLE_SHARE *share_arg,
               uint deadlock_weight_arg)
    : m_ctx(ctx_arg), m_share(share_arg),
      m_deadlock_weight(deadlock_weight_arg)
  {}

  MDL_context *get_ctx() const { return m_ctx; }

  virtual bool accept_visitor(MDL_wait_for_graph_visitor *dvisitor);

  virtual uint get_deadlock_weight() const;

  /**
    Pointers for participating in the list of waiters for table share.
  */
  Wait_for_flush *next_in_share;
  Wait_for_flush **prev_in_share;
};


typedef I_P_List <Wait_for_flush,
                  I_P_List_adapter<Wait_for_flush,
                                   &Wait_for_flush::next_in_share,
                                   &Wait_for_flush::prev_in_share> >
                 Wait_for_flush_list;


/**
  This structure is shared between different table objects. There is one
  instance of table share per one table in the database.
*/

struct TABLE_SHARE
{
  TABLE_SHARE() {}                    /* Remove gcc warning */

  /** Category of this table. */
  TABLE_CATEGORY table_category;

  /* hash of field names (contains pointers to elements of field array) */
  HASH	name_hash;			/* hash of field names */
  MEM_ROOT mem_root;
  TYPELIB keynames;			/* Pointers to keynames */
  TYPELIB fieldnames;			/* Pointer to fieldnames */
  TYPELIB *intervals;			/* pointer to interval info */
  mysql_mutex_t LOCK_ha_data;           /* To protect access to ha_data */
  TABLE_SHARE *next, **prev;            /* Link to unused shares */
  /**
    Array of table_cache_instances pointers to elements of table caches
    respresenting this table in each of Table_cache instances.
    Allocated along with the share itself in alloc_table_share().
    Each element of the array is protected by Table_cache::m_lock in the
    corresponding Table_cache. False sharing should not be a problem in
    this case as elements of this array are supposed to be updated rarely.
  */
  Table_cache_element **cache_element;

  /* The following is copied to each TABLE on OPEN */
  Field **field;
  Field **found_next_number_field;
  KEY  *key_info;			/* data of keys defined for the table */
  uint	*blob_field;			/* Index to blobs in Field arrray*/

  uchar	*default_values;		/* row with default values */
  LEX_STRING comment;			/* Comment about table */
  LEX_STRING compress;			/* Compression algorithm */
  LEX_STRING encrypt_type;		/* encryption algorithm */
  const CHARSET_INFO *table_charset;	/* Default charset of string fields */

  MY_BITMAP all_set;
  /*
    Key which is used for looking-up table in table cache and in the list
    of thread's temporary tables. Has the form of:
      "database_name\0table_name\0" + optional part for temporary tables.

    Note that all three 'table_cache_key', 'db' and 'table_name' members
    must be set (and be non-zero) for tables in table cache. They also
    should correspond to each other.
    To ensure this one can use set_table_cache() methods.
  */
  LEX_STRING table_cache_key;
  LEX_STRING db;                        /* Pointer to db */
  LEX_STRING table_name;                /* Table name (for open) */
  LEX_STRING path;                	/* Path to .frm file (from datadir) */
  LEX_STRING normalized_path;		/* unpack_filename(path) */
  LEX_STRING connect_string;

  /* 
     Set of keys in use, implemented as a Bitmap.
     Excludes keys disabled by ALTER TABLE ... DISABLE KEYS.
  */
  key_map keys_in_use;
  key_map keys_for_keyread;
  ha_rows min_rows, max_rows;		/* create information */
  ulong   avg_row_length;		/* create information */
  /**
    TABLE_SHARE version, if changed the TABLE_SHARE must be reopened.
    NOTE: The TABLE_SHARE will not be reopened during LOCK TABLES in
    close_thread_tables!!!
  */
  ulong   version;
  ulong   mysql_version;		/* 0 if .frm is created before 5.0 */
  ulong   reclength;			/* Recordlength */
  ulong   stored_rec_length;            /* Stored record length 
                                           (no generated-only generated fields) */

  plugin_ref db_plugin;			/* storage engine plugin */
  inline handlerton *db_type() const	/* table_type for handler */
  { 
    // DBUG_ASSERT(db_plugin);
    return db_plugin ? plugin_data<handlerton*>(db_plugin) : NULL;
  }
  enum row_type row_type;		/* How rows are stored */
  enum tmp_table_type tmp_table;

  uint ref_count;                       /* How many TABLE objects uses this */
  uint key_block_size;			/* create key_block_size, if used */
  uint stats_sample_pages;		/* number of pages to sample during
					stats estimation, if used, otherwise 0. */
  enum_stats_auto_recalc stats_auto_recalc; /* Automatic recalc of stats. */
  uint null_bytes, last_null_bit_pos;
  uint fields;				/* Number of fields */
  uint stored_fields;                   /* Number of stored fields 
                                           (i.e. without generated-only ones) */
  uint rec_buff_length;                 /* Size of table->record[] buffer */
  uint keys;                            /* Number of keys defined for the table*/
  uint key_parts;                       /* Number of key parts of all keys
                                           defined for the table
                                        */
  uint max_key_length;                  /* Length of the longest key */
  uint max_unique_length;               /* Length of the longest unique key */
  uint total_key_length;
  uint uniques;                         /* Number of UNIQUE index */
  uint null_fields;			/* number of null fields */
  uint blob_fields;			/* number of blob fields */
  uint varchar_fields;                  /* number of varchar fields */
  uint db_create_options;		/* Create options from database */
  uint db_options_in_use;		/* Options in use */
  uint db_record_offset;		/* if HA_REC_IN_SEQ */
  uint rowid_field_offset;		/* Field_nr +1 to rowid field */
  /* Primary key index number, used in TABLE::key_info[] */
  uint primary_key;                     
  uint next_number_index;               /* autoincrement key number */
  uint next_number_key_offset;          /* autoinc keypart offset in a key */
  uint next_number_keypart;             /* autoinc keypart number in a key */
  uint error, open_errno, errarg;       /* error from open_table_def() */
  uint column_bitmap_size;
  uchar frm_version;
  uint vfields;                         /* Number of generated fields */
  bool null_field_first;
  bool system;                          /* Set if system table (one record) */
  bool crypted;                         /* If .frm file is crypted */
  bool db_low_byte_first;		/* Portable row format */
  bool crashed;
  bool is_view;
  bool m_open_in_progress;              /* True: alloc'ed, false: def opened */
  Table_id table_map_id;                   /* for row-based replication */

  /*
    Cache for row-based replication table share checks that does not
    need to be repeated. Possible values are: -1 when cache value is
    not calculated yet, 0 when table *shall not* be replicated, 1 when
    table *may* be replicated.
  */
  int cached_row_logging_check;

  /*
    Storage media to use for this table (unless another storage
    media has been specified on an individual column - in versions
    where that is supported)
  */
  enum ha_storage_media default_storage_media;

  /* Name of the tablespace used for this table */
  char *tablespace;

  /* filled in when reading from frm */
  bool auto_partitioned;
  char *partition_info_str;
  uint  partition_info_str_len;
  uint  partition_info_buffer_size;
  handlerton *default_part_db_type;

  /**
    Cache the checked structure of this table.

    The pointer data is used to describe the structure that
    a instance of the table must have. Each element of the
    array specifies a field that must exist on the table.

    The pointer is cached in order to perform the check only
    once -- when the table is loaded from the disk.
  */
  const TABLE_FIELD_DEF *table_field_def_cache;

  /** Main handler's share */
  Handler_share *ha_share;

  /** Instrumentation for this table share. */
  PSI_table_share *m_psi;

  /**
    List of tickets representing threads waiting for the share to be flushed.
  */
  Wait_for_flush_list m_flush_tickets;

  /**
    For shares representing views File_parser object with view
    definition read from .FRM file.
  */ 
  const File_parser *view_def;


  /*
    Set share's table cache key and update its db and table name appropriately.

    SYNOPSIS
      set_table_cache_key()
        key_buff    Buffer with already built table cache key to be
                    referenced from share.
        key_length  Key length.

    NOTES
      Since 'key_buff' buffer will be referenced from share it should has same
      life-time as share itself.
      This method automatically ensures that TABLE_SHARE::table_name/db have
      appropriate values by using table cache key as their source.
  */

  void set_table_cache_key(char *key_buff, size_t key_length)
  {
    table_cache_key.str= key_buff;
    table_cache_key.length= key_length;
    /*
      Let us use the fact that the key is "db/0/table_name/0" + optional
      part for temporary tables.
    */
    db.str=            table_cache_key.str;
    db.length=         strlen(db.str);
    table_name.str=    db.str + db.length + 1;
    table_name.length= strlen(table_name.str);
  }


  /*
    Set share's table cache key and update its db and table name appropriately.

    SYNOPSIS
      set_table_cache_key()
        key_buff    Buffer to be used as storage for table cache key
                    (should be at least key_length bytes).
        key         Value for table cache key.
        key_length  Key length.

    NOTE
      Since 'key_buff' buffer will be used as storage for table cache key
      it should has same life-time as share itself.
  */

  void set_table_cache_key(char *key_buff, const char *key, size_t key_length)
  {
    memcpy(key_buff, key, key_length);
    set_table_cache_key(key_buff, key_length);
  }

  inline bool honor_global_locks()
  {
    return ((table_category == TABLE_CATEGORY_USER)
            || (table_category == TABLE_CATEGORY_SYSTEM));
  }

  inline ulonglong get_table_def_version()
  {
    return table_map_id;
  }


  /** Is this table share being expelled from the table definition cache?  */
  inline bool has_old_version() const
  {
    return version != refresh_version;
  }
  /**
    Convert unrelated members of TABLE_SHARE to one enum
    representing its type.

    @todo perhaps we need to have a member instead of a function.
  */
  enum enum_table_ref_type get_table_ref_type() const
  {
    if (is_view)
      return TABLE_REF_VIEW;
    switch (tmp_table) {
    case NO_TMP_TABLE:
      return TABLE_REF_BASE_TABLE;
    case SYSTEM_TMP_TABLE:
      return TABLE_REF_I_S_TABLE;
    default:
      return TABLE_REF_TMP_TABLE;
    }
  }
  /**
    Return a table metadata version.
     * for base tables and views, we return table_map_id.
       It is assigned from a global counter incremented for each
       new table loaded into the table definition cache (TDC).
     * for temporary tables it's table_map_id again. But for
       temporary tables table_map_id is assigned from
       thd->query_id. The latter is assigned from a thread local
       counter incremented for every new SQL statement. Since
       temporary tables are thread-local, each temporary table
       gets a unique id.
     * for everything else (e.g. information schema tables),
       the version id is zero.

   This choice of version id is a large compromise
   to have a working prepared statement validation in 5.1. In
   future version ids will be persistent, as described in WL#4180.

   Let's try to explain why and how this limited solution allows
   to validate prepared statements.

   Firstly, sets (in mathematical sense) of version numbers
   never intersect for different table types. Therefore,
   version id of a temporary table is never compared with
   a version id of a view, and vice versa.

   Secondly, for base tables and views, we know that each DDL flushes
   the respective share from the TDC. This ensures that whenever
   a table is altered or dropped and recreated, it gets a new
   version id.
   Unfortunately, since elements of the TDC are also flushed on
   LRU basis, this choice of version ids leads to false positives.
   E.g. when the TDC size is too small, we may have a SELECT
   * FROM INFORMATION_SCHEMA.TABLES flush all its elements, which
   in turn will lead to a validation error and a subsequent
   reprepare of all prepared statements.  This is
   considered acceptable, since as long as prepared statements are
   automatically reprepared, spurious invalidation is only
   a performance hit. Besides, no better simple solution exists.

   For temporary tables, using thd->query_id ensures that if
   a temporary table was altered or recreated, a new version id is
   assigned. This suits validation needs very well and will perhaps
   never change.

   Metadata of information schema tables never changes.
   Thus we can safely assume 0 for a good enough version id.

   Finally, by taking into account table type, we always
   track that a change has taken place when a view is replaced
   with a base table, a base table is replaced with a temporary
   table and so on.

   @sa TABLE_LIST::is_table_ref_id_equal()
  */
  ulonglong get_table_ref_version() const
  {
    return (tmp_table == SYSTEM_TMP_TABLE) ? 0 : table_map_id.id();
  }

  bool visit_subgraph(Wait_for_flush *waiting_ticket,
                      MDL_wait_for_graph_visitor *gvisitor);

  bool wait_for_old_version(THD *thd, struct timespec *abstime,
                            uint deadlock_weight);
  /** Release resources and free memory occupied by the table share. */
  void destroy();
};


/**
   Class is used as a BLOB field value storage for
   intermediate GROUP_CONCAT results. Used only for
   GROUP_CONCAT with  DISTINCT or ORDER BY options.
 */

class Blob_mem_storage: public Sql_alloc
{
private:
  MEM_ROOT storage;
  /**
    Sign that some values were cut
    during saving into the storage.
  */
  bool truncated_value;
public:
  Blob_mem_storage() :truncated_value(false)
  {
    init_alloc_root(key_memory_blob_mem_storage,
                    &storage, MAX_FIELD_VARCHARLENGTH, 0);
  }
  ~ Blob_mem_storage()
  {
    free_root(&storage, MYF(0));
  }
  void reset()
  {
    free_root(&storage, MYF(MY_MARK_BLOCKS_FREE));
    truncated_value= false;
  }
  /**
     Fuction creates duplicate of 'from'
     string in 'storage' MEM_ROOT.

     @param from           string to copy
     @param length         string length

     @retval Pointer to the copied string.
     @retval 0 if an error occured.
  */
  char *store(const char *from, size_t length)
  {
    return (char*) memdup_root(&storage, from, length);
  }
  void set_truncated_value(bool is_truncated_value)
  {
    truncated_value= is_truncated_value;
  }
  bool is_truncated_value() { return truncated_value; }
};


/**
  Flags for TABLE::status (maximum 8 bits). Do NOT add new ones.
  @todo: GARBAGE and NOT_FOUND could be unified. UPDATED and DELETED could be
  changed to "bool current_row_has_already_been_modified" in the
  multi_update/delete objects (one such bool per to-be-modified table).
  @todo aim at removing the status. There should be more local ways.
*/
#define STATUS_GARBAGE          1
/**
   Means we were searching for a row and didn't find it. This is used by
   storage engines (@see handler::index_read_map()) and the Server layer.
*/
#define STATUS_NOT_FOUND        2
/// Reserved for use by multi-table update. Means the row has been updated.
#define STATUS_UPDATED          16
/**
   Means that table->null_row is set. This is an artificial NULL-filled row
   (one example: in outer join, if no match has been found in inner table).
*/
#define STATUS_NULL_ROW         32
/// Reserved for use by multi-table delete. Means the row has been deleted.
#define STATUS_DELETED          64


/* Information for one open table */
enum index_hint_type
{
  INDEX_HINT_IGNORE,
  INDEX_HINT_USE,
  INDEX_HINT_FORCE
};

/* Bitmap of table's fields */
typedef Bitmap<MAX_FIELDS> Field_map;

struct TABLE
{
  TABLE() {}                               /* Remove gcc warning */
  /*
    Since TABLE instances are often cleared using memset(), do not
    add virtual members and do not inherit from TABLE.
    Otherwise memset() will start overwriting the vtable pointer.
  */

  TABLE_SHARE	*s;
  handler	*file;
  TABLE *next, *prev;

private:
  /**
     Links for the lists of used/unused TABLE objects for the particular
     table in the specific instance of Table_cache (in other words for
     specific Table_cache_element object).
     Declared as private to avoid direct manipulation with those objects.
     One should use methods of I_P_List template instead.
  */
  TABLE *cache_next, **cache_prev;

  /*
    Give Table_cache_element access to the above two members to allow
    using them for linking TABLE objects in a list.
  */
  friend class Table_cache_element;

public:

  THD	*in_use;                        /* Which thread uses this */
  Field **field;			/* Pointer to fields */
  /// Count of hidden fields, if internal temporary table; 0 otherwise.
  uint hidden_field_count;

  uchar *record[2];			/* Pointer to records */
  uchar *write_row_record;		/* Used as optimisation in
					   THD::write_row */
  uchar *insert_values;                  /* used by INSERT ... UPDATE */
  /* 
    Map of keys that can be used to retrieve all data from this table 
    needed by the query without reading the row.
  */
  key_map covering_keys;
  key_map quick_keys, merge_keys;
  
  /*
    possible_quick_keys is a superset of quick_keys to use with EXPLAIN of
    JOIN-less commands (single-table UPDATE and DELETE).
    
    When explaining regular JOINs, we use JOIN_TAB::keys to output the 
    "possible_keys" column value. However, it is not available for
    single-table UPDATE and DELETE commands, since they don't use JOIN
    optimizer at the top level. OTOH they directly use the range optimizer,
    that collects all keys usable for range access here.
  */
  key_map possible_quick_keys;

  /*
    A set of keys that can be used in the query that references this
    table.

    All indexes disabled on the table's TABLE_SHARE (see TABLE::s) will be 
    subtracted from this set upon instantiation. Thus for any TABLE t it holds
    that t.keys_in_use_for_query is a subset of t.s.keys_in_use. Generally we 
    must not introduce any new keys here (see setup_tables).

    The set is implemented as a bitmap.
  */
  key_map keys_in_use_for_query;
  /* Map of keys that can be used to calculate GROUP BY without sorting */
  key_map keys_in_use_for_group_by;
  /* Map of keys that can be used to calculate ORDER BY without sorting */
  key_map keys_in_use_for_order_by;
  KEY  *key_info;			/* data of keys defined for the table */

  Field *next_number_field;		/* Set if next_number is activated */
  Field *found_next_number_field;	/* Set on open */
  Field **vfield;                       /* Pointer to generated fields*/
  Field *hash_field;                    /* Field used by unique constraint */
  Field *fts_doc_id_field;              /* Set if FTS_DOC_ID field is present */

  /* Table's triggers, 0 if there are no of them */
  Table_trigger_dispatcher *triggers;
  TABLE_LIST *pos_in_table_list;/* Element referring to this table */
  /* Position in thd->locked_table_list under LOCK TABLES */
  TABLE_LIST *pos_in_locked_tables;
  ORDER		*group;
  const char	*alias;            	  /* alias or table name */
  uchar		*null_flags;
  my_bitmap_map	*bitmap_init_value;
  MY_BITMAP     def_read_set, def_write_set, tmp_set; /* containers */
  /*
    Bitmap of fields that one or more query condition refers to. Only
    used if optimizer_condition_fanout_filter is turned 'on'.
    Currently, only the WHERE clause and ON clause of inner joins is
    taken into account but not ON conditions of outer joins.
    Furthermore, HAVING conditions apply to groups and are therefore
    not useful as table condition filters.
  */
  MY_BITMAP     cond_set;

  /**
    Bitmap of table fields (columns), which are explicitly set in the
    INSERT INTO statement. It is declared here to avoid memory allocation
    on MEM_ROOT).

    @sa fields_set_during_insert.
  */
  MY_BITMAP     def_fields_set_during_insert;

  MY_BITMAP     *read_set, *write_set;          /* Active column sets */

  /**
    A pointer to the bitmap of table fields (columns), which are explicitly set
    in the INSERT INTO statement.

    fields_set_during_insert points to def_fields_set_during_insert
    for base (non-temporary) tables. In other cases, it is NULL.
    Triggers can not be defined for temporary tables, so this bitmap does not
    matter for temporary tables.

    @sa def_fields_set_during_insert.
  */
  MY_BITMAP     *fields_set_during_insert;

  /*
   The ID of the query that opened and is using this table. Has different
   meanings depending on the table type.

   Temporary tables:

   table->query_id is set to thd->query_id for the duration of a statement
   and is reset to 0 once it is closed by the same statement. A non-zero
   table->query_id means that a statement is using the table even if it's
   not the current statement (table is in use by some outer statement).

   Non-temporary tables:

   Under pre-locked or LOCK TABLES mode: query_id is set to thd->query_id
   for the duration of a statement and is reset to 0 once it is closed by
   the same statement. A non-zero query_id is used to control which tables
   in the list of pre-opened and locked tables are actually being used.
  */
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

  uint          lock_position;          /* Position in MYSQL_LOCK.table */
  uint          lock_data_start;        /* Start pos. in MYSQL_LOCK.locks */
  uint          lock_count;             /* Number of locks */
  uint          temp_pool_slot;		/* Used by intern temp tables */
  uint		db_stat;		/* mode of file as in handler.h */
  int		current_lock;           /* Type of lock on table */

private:
  /**
    If true, this table is inner w.r.t. some outer join operation, all columns
    are nullable (in the query), and null_row may be true.
  */
  my_bool nullable;

public:
  /*
    If true, the current table row is considered to have all columns set to 
    NULL, including columns declared as "not null" (see nullable).
    @todo make it private, currently join buffering changes it through a pointer
  */
  my_bool null_row;

  uint8   status;                       /* What's in record[0] */
  my_bool copy_blobs;                   /* copy_blobs when storing */

  /*
    TODO: Each of the following flags take up 8 bits. They can just as easily
    be put into one single unsigned long and instead of taking up 18
    bytes, it would take up 4.
  */
  my_bool force_index;

  /**
    Flag set when the statement contains FORCE INDEX FOR ORDER BY
    See TABLE_LIST::process_index_hints().
  */
  my_bool force_index_order;

  /**
    Flag set when the statement contains FORCE INDEX FOR GROUP BY
    See TABLE_LIST::process_index_hints().
  */
  my_bool force_index_group;
  my_bool distinct;
  my_bool const_table;
  my_bool no_rows;

  /**
     If set, the optimizer has found that row retrieval should access index 
     tree only.
   */
  my_bool key_read;
  /**
     Certain statements which need the full row, set this to ban index-only
     access.
  */
  my_bool no_keyread;
  my_bool locked_by_logger;
  /**
    If set, indicate that the table is not replicated by the server.
  */
  my_bool no_replicate;
  my_bool locked_by_name;
  my_bool fulltext_searched;
  my_bool no_cache;
  /* To signal that the table is associated with a HANDLER statement */
  my_bool open_by_handler;
  /*
    To indicate that a non-null value of the auto_increment field
    was provided by the user or retrieved from the current record.
    Used only in the MODE_NO_AUTO_VALUE_ON_ZERO mode.
  */
  my_bool auto_increment_field_not_null;
  my_bool insert_or_update;             /* Can be used by the handler */
  my_bool alias_name_used;		/* true if table_name is alias */
  my_bool get_fields_in_item_tree;      /* Signal to fix_field */
  /**
    This table must be reopened and is not to be reused.
    NOTE: The TABLE will not be reopened during LOCK TABLES in
    close_thread_tables!!!
  */
  my_bool m_needs_reopen;
private:
  bool created; /* For tmp tables. TRUE <=> tmp table has been instantiated.*/
public:
  uint max_keys; /* Size of allocated key_info array. */

  struct /* field connections */
  {
    class JOIN_TAB *join_tab;
    class QEP_TAB *qep_tab;
    enum thr_lock_type lock_type;		/* How table is used */
    bool not_exists_optimize;
    /*
      TRUE <=> range optimizer found that there is no rows satisfying
      table conditions.
    */
    bool impossible_range;
  } reginfo;

  /**
     @todo This member should not be declared in-line. That makes it
     impossible for any function that does memory allocation to take a const
     reference to a TABLE object.
   */
  MEM_ROOT mem_root;
  /**
     Initialized in Item_func_group_concat::setup for appropriate
     temporary table if GROUP_CONCAT is used with ORDER BY | DISTINCT
     and BLOB field count > 0.
   */
  Blob_mem_storage *blob_storage;
  GRANT_INFO grant;
  Filesort_info sort;
  partition_info *part_info;            /* Partition related information */
  /* If true, all partitions have been pruned away */
  bool all_partitions_pruned_away;
  MDL_ticket *mdl_ticket;

private:
  /// Cost model object for operations on this table
  Cost_model_table m_cost_model;
public:

  void init(THD *thd, TABLE_LIST *tl);
  bool fill_item_list(List<Item> *item_list) const;
  void reset_item_list(List<Item> *item_list) const;
  void clear_column_bitmaps(void);
  void prepare_for_position(void);

  void mark_column_used(THD *thd, Field *field, enum enum_mark_columns mark);
  void mark_columns_used_by_index_no_reset(uint index, MY_BITMAP *map,
                                           uint key_parts= 0);
  void mark_columns_used_by_index(uint index);
  void mark_auto_increment_column(void);
  void mark_columns_needed_for_update(bool mark_binlog_columns);
  void mark_columns_needed_for_delete(void);
  void mark_columns_needed_for_insert(void);
  void mark_columns_per_binlog_row_image(void);
  void mark_generated_columns(bool is_update);
  bool is_field_used_by_generated_columns(uint field_index);
  void mark_gcol_in_maps(Field *field);
  inline void column_bitmaps_set(MY_BITMAP *read_set_arg,
                                 MY_BITMAP *write_set_arg)
  {
    read_set= read_set_arg;
    write_set= write_set_arg;
    if (file && created)
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
  /** Should this instance of the table be reopened? */
  inline bool needs_reopen()
  { return !db_stat || m_needs_reopen; }
  /// @returns first non-hidden column
  Field **visible_field_ptr() const
  { return field + hidden_field_count; }
  /// @returns count of visible fields
  uint visible_field_count() const
  { return s->fields - hidden_field_count; }
  bool alloc_keys(uint key_count);
  bool add_tmp_key(Field_map *key_parts, char *key_name);
  void use_index(int key_to_save);

  void set_keyread(bool flag)
  {
    DBUG_ASSERT(file);
    if (flag && !key_read)
    {
      key_read= 1;
      if (is_created())
        file->extra(HA_EXTRA_KEYREAD);
    }
    else if (!flag && key_read)
    {
      key_read= 0;
      if (is_created())
        file->extra(HA_EXTRA_NO_KEYREAD);
    }
  }

  /**
    Check whether the given index has a virtual generated columns.

    @param index_no        the given index to check

    @returns true if if index is defined over at least one virtual generated
    column
  */
  inline bool index_contains_some_virtual_gcol(uint index_no)
  {
    DBUG_ASSERT(index_no < s->keys);
    return key_info[index_no].flags & HA_VIRTUAL_GEN_KEY;
  }
  bool update_const_key_parts(Item *conds);

  bool check_read_removal(uint index);

  my_ptrdiff_t default_values_offset() const
  { return (my_ptrdiff_t) (s->default_values - record[0]); }

  /// Return true if table is instantiated, and false otherwise.
  bool is_created() const { return created; }

  /**
    Set the table as "created", and enable flags in storage engine
    that could not be enabled without an instantiated table.
  */
  void set_created()
  {
    if (created)
      return;
    if (key_read)
      file->extra(HA_EXTRA_KEYREAD);
    created= true;
  }
  /**
    Set the contents of table to be "deleted", ie "not created", after having
    deleted the contents.
  */
  void set_deleted()
  {
    created= false;
  }
  /// Set table as nullable, ie it is inner wrt some outer join
  void set_nullable() { nullable= TRUE; }

  /// Return whether table is nullable
  bool is_nullable() const { return nullable; }

  /// @return true if table contains one or more generated columns
  bool has_gcol() const { return vfield; }

  /// @return true if table contains one or more virtual generated columns
  bool has_virtual_gcol() const;

  /// Set current row as "null row", for use in null-complemented outer join
  void set_null_row()
  {
    null_row= TRUE;
    status|= STATUS_NULL_ROW;
    memset(null_flags, 255, s->null_bytes);
  }

  /// Clear "null row" status for the current row
  void reset_null_row()
  {
    null_row= FALSE;
    status&= ~STATUS_NULL_ROW;
  }

  /// @return true if current row is null-extended
  bool has_null_row() const { return null_row; }

  /**
    Initialize the optimizer cost model.
 
    This function should be called each time a new query is started.

    @param cost_model_server the main cost model object for the query
  */
  void init_cost_model(const Cost_model_server* cost_model_server)
  {
    m_cost_model.init(cost_model_server, this);
  }

  /**
    Return the cost model object for this table.
  */
  const Cost_model_table* cost_model() const { return &m_cost_model; }

  /**
    Fix table's generated columns' (GC) expressions
   
    @details When a table is opened from the dictionary, the GCs' expressions
    are fixed during opening (see fix_fields_gcol_func()). After query
    execution, Item::cleanup() is called on them (see cleanup_gc_items()). When
    the table is opened from the table cache, the GCs need to be fixed again
    and this function does that.

    @param[in] thd     the current thread
    @return true if error, else false
  */
  bool refix_gc_items(THD *thd);
  
  /**
    Clean any state in items associated with generated columns to be ready for
    the next statement.
  */
  void cleanup_gc_items();

 /**
   Check if table contains any records.

   @param      thd     The thread object
   @param[out] retval  Pointer to boolean value (true if table is not empty).

   @returns  false for success, true for error
 */
 bool contains_records(THD *thd, bool *retval);

  /**
    Virtual fields of type BLOB have a flag m_keep_old_value. This flag is set
    to false for all such fields in this table.
  */
  void blobs_need_not_keep_old_value();
};


enum enum_schema_table_state
{ 
  NOT_PROCESSED= 0,
  PROCESSED_BY_CREATE_SORT_INDEX,
  PROCESSED_BY_JOIN_EXEC
};

typedef struct st_foreign_key_info
{
  LEX_STRING *foreign_id;
  LEX_STRING *foreign_db;
  LEX_STRING *foreign_table;
  LEX_STRING *referenced_db;
  LEX_STRING *referenced_table;
  LEX_STRING *update_method;
  LEX_STRING *delete_method;
  LEX_STRING *referenced_key_name;
  List<LEX_STRING> foreign_fields;
  List<LEX_STRING> referenced_fields;
} FOREIGN_KEY_INFO;

#define MY_I_S_MAYBE_NULL 1
#define MY_I_S_UNSIGNED   2


#define SKIP_OPEN_TABLE 0                // do not open table
#define OPEN_FRM_ONLY   1                // open FRM file only
#define OPEN_FULL_TABLE 2                // open FRM,MYD, MYI files

typedef struct st_field_info
{
  /** 
      This is used as column name. 
  */
  const char* field_name;
  /**
     For string-type columns, this is the maximum number of
     characters. Otherwise, it is the 'display-length' for the column.
     For the data type MYSQL_TYPE_DATETIME this field specifies the
     number of digits in the fractional part of time value.
  */
  uint field_length;
  /**
     This denotes data type for the column. For the most part, there seems to
     be one entry in the enum for each SQL data type, although there seem to
     be a number of additional entries in the enum.
  */
  enum enum_field_types field_type;
  int value;
  /**
     This is used to set column attributes. By default, columns are @c NOT
     @c NULL and @c SIGNED, and you can deviate from the default
     by setting the appopriate flags. You can use either one of the flags
     @c MY_I_S_MAYBE_NULL and @cMY_I_S_UNSIGNED or
     combine them using the bitwise or operator @c |. Both flags are
     defined in table.h.
   */
  uint field_flags;        // Field atributes(maybe_null, signed, unsigned etc.)
  const char* old_name;
  /**
     This should be one of @c SKIP_OPEN_TABLE,
     @c OPEN_FRM_ONLY or @c OPEN_FULL_TABLE.
  */
  uint open_method;
} ST_FIELD_INFO;


struct TABLE_LIST;

typedef struct st_schema_table
{
  const char* table_name;
  ST_FIELD_INFO *fields_info;
  /* Create information_schema table */
  TABLE *(*create_table)  (THD *thd, TABLE_LIST *table_list);
  /* Fill table with data */
  int (*fill_table) (THD *thd, TABLE_LIST *tables, Item *cond);
  /* Handle fileds for old SHOW */
  int (*old_format) (THD *thd, struct st_schema_table *schema_table);
  int (*process_table) (THD *thd, TABLE_LIST *tables, TABLE *table,
                        bool res, LEX_STRING *db_name, LEX_STRING *table_name);
  int idx_field1, idx_field2; 
  bool hidden;
  uint i_s_requested_object;  /* the object we need to open(TABLE | VIEW) */
} ST_SCHEMA_TABLE;


#define JOIN_TYPE_LEFT	1
#define JOIN_TYPE_RIGHT	2

/**
  Strategy for how to process a view or derived table (merge or materialization)
*/
enum enum_view_algorithm {
  VIEW_ALGORITHM_UNDEFINED = 0,
  VIEW_ALGORITHM_TEMPTABLE = 1,
  VIEW_ALGORITHM_MERGE     = 2
};

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

/** The threshold size a blob field buffer before it is freed */
#define MAX_TDC_BLOB_SIZE 65536

/**
  Struct that describes an expression selected from a derived table or view.
*/
struct Field_translator
{
  /**
    Points to an item that represents the expression.
    If the item is determined to be unused, the pointer is set to NULL.
  */
  Item *item;
  /// Name of selected expression
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
  Item_field       *table_field; /* Column reference of table or temp view. */
  TABLE_LIST *table_ref; /* Original base table/view reference. */
  /*
    True if a common join column of two NATURAL/USING join operands. Notice
    that when we have a hierarchy of nested NATURAL/USING joins, a column can
    be common at some level of nesting but it may not be common at higher
    levels of nesting. Thus this flag may change depending on at which level
    we are looking at some column.
  */
  bool is_common;
public:
  Natural_join_column(Field_translator *field_param, TABLE_LIST *tab);
  Natural_join_column(Item_field *field_param, TABLE_LIST *tab);
  const char *name();
  Item *create_item(THD *thd);
  Field *field();
  const char *table_name();
  const char *db_name();
  GRANT_INFO *grant();
};


/*
  This structure holds the specifications relating to
  ALTER user ... PASSWORD EXPIRE ...
*/
typedef struct st_lex_alter {
  bool update_password_expired_fields;
  bool update_password_expired_column;
  bool use_default_password_lifetime;
  uint16 expire_after_days;
  bool update_account_locked_column;
  bool account_locked;
} LEX_ALTER;

typedef struct	st_lex_user {
  LEX_CSTRING user;
  LEX_CSTRING host;
  LEX_CSTRING plugin;
  LEX_CSTRING auth;
  bool uses_identified_by_clause;
  bool uses_identified_with_clause;
  bool uses_authentication_string_clause;
  bool uses_identified_by_password_clause;
  LEX_ALTER alter_status;
} LEX_USER;


/**
  Derive type of metadata lock to be requested for table used by a DML
  statement from the type of THR_LOCK lock requested for this table.
*/

inline enum enum_mdl_type mdl_type_for_dml(enum thr_lock_type lock_type)
{
  return lock_type >= TL_WRITE_ALLOW_WRITE ?
         (lock_type == TL_WRITE_LOW_PRIORITY ?
          MDL_SHARED_WRITE_LOW_PRIO : MDL_SHARED_WRITE) :
         MDL_SHARED_READ;
}

/**
   Type of table which can be open for an element of table list.
*/

enum enum_open_type
{
  OT_TEMPORARY_OR_BASE= 0, OT_TEMPORARY_ONLY, OT_BASE_ONLY
};

/**
  This structure is used to keep info about possible key for the result table
  of a derived table/view.
  The 'referenced_by' is the table map of tables to which this possible
    key corresponds.
  The 'used_field' is a map of fields of which this key consists of.
  See also the comment for the TABLE_LIST::update_derived_keys function.
*/

class Derived_key: public Sql_alloc {
public:
  table_map referenced_by;
  Field_map used_fields;
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
     - temptable(TABLE_LIST::effective_algorithm == VIEW_ALGORITHM_TEMPTABLE)
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
     - semi-join
       ;
*/

struct TABLE_LIST
{
  TABLE_LIST() {}                          /* Remove gcc warning */

  /**
    Prepare TABLE_LIST that consists of one table instance to use in
    simple_open_and_lock_tables
  */
  inline void init_one_table(const char *db_name_arg,
                             size_t db_length_arg,
                             const char *table_name_arg,
                             size_t table_name_length_arg,
                             const char *alias_arg,
                             enum thr_lock_type lock_type_arg,
                             enum enum_mdl_type mdl_type_arg)
  {
    memset(this, 0, sizeof(*this));
    m_map= 1;
    db= (char*) db_name_arg;
    db_length= db_length_arg;
    table_name= (char*) table_name_arg;
    table_name_length= table_name_length_arg;
    alias= (char*) alias_arg;
    lock_type= lock_type_arg;
    MDL_REQUEST_INIT(&mdl_request,
                     MDL_key::TABLE, db, table_name,
                     mdl_type_arg,
                     MDL_TRANSACTION);
    callback_func= 0;
    opt_hints_table= NULL;
    opt_hints_qb= NULL;
  }

  inline void init_one_table(const char *db_name_arg,
                             size_t db_length_arg,
                             const char *table_name_arg,
                             size_t table_name_length_arg,
                             const char *alias_arg,
                             enum thr_lock_type lock_type_arg)
  {
    init_one_table(db_name_arg, db_length_arg,
                   table_name_arg, table_name_length_arg,
                   alias_arg, lock_type_arg,
                   mdl_type_for_dml(lock_type_arg));
  }

  /// Create a TABLE_LIST object representing a nested join
  static TABLE_LIST *new_nested_join(MEM_ROOT *allocator,
                                     const char *alias,
                                     TABLE_LIST *embedding,
                                     List<TABLE_LIST> *belongs_to,
                                     class st_select_lex *select);

  Item         **join_cond_ref() { return &m_join_cond; }
  Item          *join_cond() const { return m_join_cond; }
  void          set_join_cond(Item *val)
  {
    // If optimization has started, it's too late to change m_join_cond.
    DBUG_ASSERT(m_join_cond_optim == NULL ||
                m_join_cond_optim == (Item*)1);
    m_join_cond= val;
  }
  Item *join_cond_optim() const { return m_join_cond_optim; }
  void set_join_cond_optim(Item *cond)
  {
    /*
      Either we are setting to "empty", or there must pre-exist a
      permanent condition.
    */
    DBUG_ASSERT(cond == NULL || cond == (Item*)1 ||
                m_join_cond != NULL);
    m_join_cond_optim= cond;
  }
  Item **join_cond_optim_ref() { return &m_join_cond_optim; }

  /// Get the semi-join condition for a semi-join nest, NULL otherwise
  Item *sj_cond() const { return m_sj_cond; }

  /// Set the semi-join condition for a semi-join nest
  void set_sj_cond(Item *cond)
  {
    DBUG_ASSERT(m_sj_cond == NULL);
    m_sj_cond= cond;
  }

  /// Merge tables from a query block into a nested join structure
  bool merge_underlying_tables(class st_select_lex *select);

  /// Reset table
  void reset();

  void calc_md5(char *buffer);

  /// Evaluate the check option of a view
  int view_check_option(THD *thd) const;

  /// Cleanup field translations for a view
  void cleanup_items();

  /**
    Check whether the table is a placeholder, ie a derived table, a view or
    a schema table.
    A table is also considered to be a placeholder if it does not have a
    TABLE object for some other reason.
  */
  bool is_placeholder() const
  {
    return is_view_or_derived() || schema_table || !table;
  }

  /// Produce a textual identification of this object
  void print(THD *thd, String *str, enum_query_type query_type) const;

  /// Check which single table inside a view that matches a table map
  bool check_single_table(TABLE_LIST **table_ref, table_map map);

  /// Allocate a buffer for inserted column values
  bool set_insert_values(MEM_ROOT *mem_root);

  TABLE_LIST *first_leaf_for_name_resolution();
  TABLE_LIST *last_leaf_for_name_resolution();
  bool is_leaf_for_name_resolution() const;

  /// Return the outermost view this table belongs to, or itself
  inline const TABLE_LIST *top_table() const
    { return belong_to_view ? belong_to_view : this; }

  inline TABLE_LIST *top_table() 
  {
    return
      const_cast<TABLE_LIST*>(const_cast<const TABLE_LIST*>(this)->top_table());
  }

  /// Prepare check option for a view
  bool prepare_check_option(THD *thd, bool is_cascaded= false);

  /// Merge WHERE condition of view or derived table into outer query
  bool merge_where(THD *thd);

  /// Prepare replace filter for a view (used for REPLACE command)
  bool prepare_replace_filter(THD *thd);

  /// Return true if this represents a named view
  bool is_view() const
  {
    return view != NULL;
  }

  /// Return true if this represents a derived table (an unnamed view)
  bool is_derived() const
  {
    return derived != NULL && view == NULL;
  }

  /// Return true if this represents a named view or a derived table
  bool is_view_or_derived() const
  {
    return derived != NULL;
  }

  /// Return true if view or derived table and can be merged
  bool is_mergeable() const;

  /**
    @returns true if materializable table contains one or zero rows.

    Returning true implies that the table is materialized during optimization,
    so it need not be optimized during execution.
  */
  bool materializable_is_const() const;

  /// Return true if this is a derived table or view that is merged
  bool is_merged() const
  {
    return effective_algorithm == VIEW_ALGORITHM_MERGE;
  }

  /// Set table to be merged
  void set_merged()
  {
    DBUG_ASSERT(effective_algorithm == VIEW_ALGORITHM_UNDEFINED);
    effective_algorithm= VIEW_ALGORITHM_MERGE;
  }

  /// Return true if this is a materializable derived table/view
  bool uses_materialization() const
  {
    return effective_algorithm == VIEW_ALGORITHM_TEMPTABLE;
  }

  /// Set table to be materialized
  void set_uses_materialization()
  {
    // @todo We should do this only once, but currently we cannot:
    //DBUG_ASSERT(effective_algorithm == VIEW_ALGORITHM_UNDEFINED);
    DBUG_ASSERT(effective_algorithm != VIEW_ALGORITHM_MERGE);
    effective_algorithm= VIEW_ALGORITHM_TEMPTABLE;
  }

  /// Return true if table is updatable
  bool is_updatable() const { return m_updatable; }

  /// Set table as updatable. (per default, a table is non-updatable)
  void set_updatable() { m_updatable= true; }

  /// Return true if table is insertable-into
  bool is_insertable() const { return m_insertable; }

  /// Set table as insertable-into. (per default, a table is not insertable)
  void set_insertable() { m_insertable= true; }

  /**
    Return true if this is a view or derived table that is defined over
    more than one base table, and false otherwise.
  */
  bool is_multiple_tables() const
  {
    if (is_view_or_derived())
    {
      DBUG_ASSERT(is_merged());         // Cannot be a materialized view
      return leaf_tables_count() > 1;
    }
    else
    {
      DBUG_ASSERT(nested_join == NULL); // Must be a base table
      return false;
    }
  }

  /// Return no. of base tables a merged view or derived table is defined over.
  uint leaf_tables_count() const;

  /// Return first leaf table of a base table or a view/derived table
  TABLE_LIST *first_leaf_table()
  {
    TABLE_LIST *tr= this;
    while (tr->merge_underlying_list)
      tr= tr->merge_underlying_list;
    return tr;
  }

  /// Return any leaf table that is not an inner table of an outer join
  /// @todo when WL#6570 is implemented, replace with first_leaf_table()
  TABLE_LIST *any_outer_leaf_table()
  {
    TABLE_LIST *tr= this;
    while (tr->merge_underlying_list)
    {
      tr= tr->merge_underlying_list;
      /*
        "while" is used, however, an "if" might be sufficient since there is
        no more than one inner table in a join nest (with outer_join true).
      */
      while (tr->outer_join)
        tr= tr->next_local;
    }
    return tr;
  }
  /**
    Set the LEX object of a view (will also define this as a view).
    @note: The value 1 is used to indicate a view but without a valid
           query object. Use only if the LEX object is not going to
           be used in later processing.
  */
  void set_view_query(LEX *lex)
  {
    view= lex;
  }

  /// Return the valid LEX object for a view.
  LEX *view_query() const
  {
    DBUG_ASSERT(view != NULL && view != (LEX *)1);
    return view;
  }

  /**
    Set the query expression of a derived table or view.
    (Will also define this as a derived table, unless it is a named view.)
  */
  void set_derived_unit(st_select_lex_unit *query_expr)
  {
    derived= query_expr;
  }

  /// Return the query expression of a derived table or view.
  st_select_lex_unit *derived_unit() const
  {
    DBUG_ASSERT(derived);
    return derived;
  }

  /// Set temporary name from underlying temporary table:
  void set_name_temporary()
  {
    DBUG_ASSERT(is_view_or_derived() && uses_materialization());
    table_name= table->s->table_name.str;
    table_name_length= table->s->table_name.length;
    db= (char *)"";
    db_length= 0;
  }

  /// Reset original name for temporary table.
  void reset_name_temporary()
  {
    DBUG_ASSERT(is_view_or_derived() && uses_materialization());
    DBUG_ASSERT(db != view_db.str && table_name != view_name.str);
    if (is_view())
    {
      db= view_db.str;
      db_length= view_db.length;
    }
    table_name= view_name.str;
    table_name_length= view_name.length;
  }

  /// Resolve a derived table or view reference
  bool resolve_derived(THD *thd, bool apply_semijoin);

  /// Optimize the query expression representing a derived table/view 
  bool optimize_derived(THD *thd);

  /// Create result table for a materialized derived table/view
  bool create_derived(THD *thd);

  /// Materialize derived table
  bool materialize_derived(THD *thd);

  /// Clean up the query expression for a materialized derived table
  bool cleanup_derived();

  /// Set wanted privilege for subsequent column privilege checking
  void set_want_privilege(ulong want_privilege);

  /// Prepare security context for a view
  bool prepare_security(THD *thd);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  Security_context *find_view_security_context(THD *thd);
  bool prepare_view_securety_context(THD *thd);
#endif

  /// Cleanup for re-execution in a prepared statement or a stored procedure.
  void reinit_before_use(THD *thd);

  /**
    Compiles the tagged hints list and fills up TABLE::keys_in_use_for_query,
    TABLE::keys_in_use_for_group_by, TABLE::keys_in_use_for_order_by,
    TABLE::force_index and TABLE::covering_keys.
  */
  bool process_index_hints(TABLE *table);

  /**
    Compare the version of metadata from the previous execution
    (if any) with values obtained from the current table
    definition cache element.

    @sa check_and_update_table_version()
  */
  bool is_table_ref_id_equal(TABLE_SHARE *s) const
  {
    return (m_table_ref_type == s->get_table_ref_type() &&
            m_table_ref_version == s->get_table_ref_version());
  }

  /**
    Record the value of metadata version of the corresponding
    table definition cache element in this parse tree node.

    @sa check_and_update_table_version()
  */
  void set_table_ref_id(TABLE_SHARE *s)
  { set_table_ref_id(s->get_table_ref_type(), s->get_table_ref_version()); }

  void set_table_ref_id(enum_table_ref_type table_ref_type_arg,
                        ulonglong table_ref_version_arg)
  {
    m_table_ref_type= table_ref_type_arg;
    m_table_ref_version= table_ref_version_arg;
  }

  /// returns query block id for derived table, and zero if not derived.
  uint query_block_id() const;

  /**
     @brief Returns the name of the database that the referenced table belongs
     to.
  */
  const char *get_db_name() const { return view != NULL ? view_db.str : db; }

  /**
     @brief Returns the name of the table that this TABLE_LIST represents.

     @details The unqualified table name or view name for a table or view,
     respectively.
   */
  const char *get_table_name() const
  {
    return view != NULL ? view_name.str : table_name;
  }
  int fetch_number_of_rows();
  bool update_derived_keys(Field*, Item**, uint);
  bool generate_keys();

  /// Setup a derived table to use materialization
  bool setup_materialized_derived(THD *thd);

  bool create_field_translation(THD *thd);

  /**
    @brief Returns the outer join nest that this TABLE_LIST belongs to, if any.

    @details There are two kinds of join nests, outer-join nests and semi-join 
    nests.  This function returns non-NULL in the following cases:
      @li 1. If this table/nest is embedded in a nest and this nest IS NOT a 
             semi-join nest.  (In other words, it is an outer-join nest.)
      @li 2. If this table/nest is embedded in a nest and this nest IS a 
             semi-join nest, but this semi-join nest is embedded in another 
             nest. (This other nest will be an outer-join nest, since all inner 
             joined nested semi-join nests have been merged in 
             @c simplify_joins() ).
    Note: This function assumes that @c simplify_joins() has been performed.
    Before that, join nests will be present for all types of join.

    @return outer join nest, or NULL if none.
  */

  TABLE_LIST *outer_join_nest() const
  {
    if (!embedding)
      return NULL;
    if (embedding->sj_cond())
      return embedding->embedding;
    return embedding;
  }
  /**
    Return true if this table is an inner table of some outer join.

    Examine all the embedding join nests of the table.
    @note This function works also before redundant join nests have been
          eliminated.

    @return true if table is an inner table of some outer join, false otherwise.
  */

  bool is_inner_table_of_outer_join() const
  {
    if (outer_join)
      return true;
    for (TABLE_LIST *emb= embedding; emb; emb= emb->embedding)
    {
      if (emb->outer_join)
        return true;
    }
    return false;
  }

  /**
    Return the base table entry of an updatable table.
    In DELETE and UPDATE, a view used as a target table must be mergeable,
    updatable and defined over a single table.
  */
  TABLE_LIST *updatable_base_table()
  {
    TABLE_LIST *tbl= this;
    DBUG_ASSERT(tbl->is_updatable() && !tbl->is_multiple_tables());
    while (tbl->is_view_or_derived())
    {
      tbl= tbl->merge_underlying_list;
      DBUG_ASSERT(tbl->is_updatable() && !tbl->is_multiple_tables());
    }
    return tbl;
  }


  /**
    Set granted privileges for a table.

    Can be used when generating temporary tables that are also used in
    resolver process, such as when generating a UNION table

    @param privilege   Privileges granted for this table.
  */
  void set_privileges(ulong privilege)
  {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    grant.privilege|= privilege;
    if (table)
      table->grant.privilege|= privilege;
#endif
  }
  /*
    List of tables local to a subquery or the top-level SELECT (used by
    SQL_I_List). Considers views as leaves (unlike 'next_leaf' below).
    Created at parse time in st_select_lex::add_table_to_list() ->
    table_list.link_in_list().
  */
  TABLE_LIST *next_local;
  /* link in a global list of all queries tables */
  TABLE_LIST *next_global, **prev_global;
  const char *db, *table_name, *alias;
  /*
    Target tablespace name: When creating or altering tables, this
    member points to the tablespace_name in the HA_CREATE_INFO struct.
  */
  LEX_CSTRING target_tablespace_name;
  char *schema_table_name;
  char *option;                /* Used by cache index  */

  /** Table level optimizer hints for this table.  */
  Opt_hints_table *opt_hints_table;
  /* Hints for query block of this table. */
  Opt_hints_qb *opt_hints_qb;

private:
  /**
    The members below must be kept aligned so that (1 << m_tableno) == m_map.
    A table that takes part in a join operation must be assigned a unique
    table number.
  */
  uint          m_tableno;              ///< Table number within query block
  table_map     m_map;                  ///< Table map, derived from m_tableno
  /**
     If this table or join nest is the Y in "X [LEFT] JOIN Y ON C", this
     member points to C. May also be generated from JOIN ... USING clause.
     It may be modified only by permanent transformations (permanent = done
     once for all executions of a prepared statement).
  */
  Item		*m_join_cond;
  Item          *m_sj_cond;               ///< Synthesized semijoin condition
public:
  /*
    (Valid only for semi-join nests) Bitmap of tables that are within the
    semi-join (this is different from bitmap of all nest's children because
    tables that were pulled out of the semi-join nest remain listed as
    nest's children).
  */
  table_map     sj_inner_tables;

  /*
    During parsing - left operand of NATURAL/USING join where 'this' is
    the right operand. After parsing (this->natural_join == this) iff
    'this' represents a NATURAL or USING join operation. Thus after
    parsing 'this' is a NATURAL/USING join iff (natural_join != NULL).
  */
  TABLE_LIST *natural_join;
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
  TABLE_LIST *next_name_resolution_table;
  /* Index names in a "... JOIN ... USE/IGNORE INDEX ..." clause. */
  List<Index_hint> *index_hints;
  TABLE        *table;                          /* opened table */
  Table_id table_id; /* table id (from binlog) for opened table */
  /*
    Query_result for derived table to pass it from table creation to table
    filling procedure
  */
  Query_result_union  *derived_result;
  /*
    Reference from aux_tables to local list entry of main select of
    multi-delete statement:
    delete t1 from t2,t1 where t1.a<'B' and t2.b=t1.b;
    here it will be reference of first occurrence of t1 to second (as you
    can see this lists can't be merged)
  */
  TABLE_LIST	*correspondent_table;
private:
  /**
     This field is set to non-null for derived tables and views. It points
     to the SELECT_LEX_UNIT representing the derived table/view.
     E.g. for a query
     
     @verbatim SELECT * FROM (SELECT a FROM t1) b @endverbatim
     
     @note Inside views, a subquery in the @c FROM clause is not allowed.
  */
  st_select_lex_unit *derived;		/* SELECT_LEX_UNIT of derived table */

public:
  ST_SCHEMA_TABLE *schema_table;        /* Information_schema table */
  st_select_lex	*schema_select_lex;
  /*
    True when the view field translation table is used to convert
    schema table fields for backwards compatibility with SHOW command.
  */
  bool schema_table_reformed;
  Temp_table_param *schema_table_param;
  /* link to select_lex where this table was used */
  st_select_lex	*select_lex;

private:
  LEX *view;                    /* link on VIEW lex for merging */

public:
  /// Array of selected expressions from a derived table or view.
  Field_translator *field_translation;

  /// pointer to element after last one in translation table above
  Field_translator *field_translation_end;
  /*
    List (based on next_local) of underlying tables of this view. I.e. it
    does not include the tables of subqueries used in the view. Is set only
    for merged views.
  */
  TABLE_LIST	*merge_underlying_list;
  /*
    - 0 for base tables
    - in case of the view it is the list of all (not only underlying
    tables but also used in subquery ones) tables of the view.
  */
  List<TABLE_LIST> *view_tables;
  /* most upper view this table belongs to */
  TABLE_LIST	*belong_to_view;
  /*
    The view directly referencing this table
    (non-zero only for merged underlying tables of a view).
  */
  TABLE_LIST	*referencing_view;
  /* Ptr to parent MERGE table list item. See top comment in ha_myisammrg.cc */
  TABLE_LIST    *parent_l;
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
    leaves. Created in setup_tables() -> make_leaf_tables().
  */
  bool allowed_show;
  TABLE_LIST    *next_leaf;
  Item          *derived_where_cond;    ///< WHERE condition from derived table
  Item          *check_option;          ///< WITH CHECK OPTION condition
  Item          *replace_filter;        ///< Filter for REPLACE command
  LEX_STRING    select_stmt;            ///< text of (CREATE/SELECT) statement
  LEX_STRING    md5;                    ///< md5 of query text
  LEX_STRING    source;                 ///< source of CREATE VIEW
  LEX_CSTRING   view_db;                ///< saved view database
  LEX_CSTRING   view_name;              ///< saved view name
  LEX_STRING    timestamp;              ///< GMT time stamp of last operation
  st_lex_user   definer;                ///< definer of view
  ulonglong     file_version;           ///< version of file's field set
  /**
    @note: This field is currently not reliable when read from dictionary:
    If an underlying view is changed, updatable_view is not changed,
    due to lack of dependency checking in dictionary implementation.
    Prefer to use is_updatable() during preparation and optimization.
  */
  ulonglong     updatable_view;         ///< VIEW can be updated
  /** 
      @brief The declared algorithm, if this is a view.
      @details One of
      - VIEW_ALGORITHM_UNDEFINED
      - VIEW_ALGORITHM_TEMPTABLE
      - VIEW_ALGORITHM_MERGE
      @to do Replace with an enum 
  */
  ulonglong     algorithm;
  ulonglong     view_suid;              ///< view is suid (TRUE by default)
  ulonglong     with_check;             ///< WITH CHECK OPTION

private:
  /// The view algorithm that is actually used, if this is a view.
  enum_view_algorithm effective_algorithm;
public:
  GRANT_INFO	grant;
  /* data need by some engines in query cache*/
  ulonglong     engine_data;
  /* call back function for asking handler about caching in query cache */
  qc_engine_callback callback_func;
  thr_lock_type lock_type;
  uint		outer_join;		/* Which join type */
  uint		shared;			/* Used in multi-upd */
  size_t        db_length;
  size_t        table_name_length;
private:
  bool          m_updatable;		/* VIEW/TABLE can be updated */
  bool          m_insertable;           /* VIEW/TABLE can be inserted into */
public:
  bool		straight;		/* optimize with prev table */
  bool          updating;               /* for replicate-do/ignore table */
  bool		force_index;		/* prefer index over table scan */
  bool          ignore_leaves;          /* preload only non-leaf nodes */
  table_map     dep_tables;             /* tables the table depends on      */
  table_map     on_expr_dep_tables;     /* tables on expression depends on  */
  struct st_nested_join *nested_join;   /* if the element is a nested join  */
  TABLE_LIST *embedding;             /* nested join containing the table */
  List<TABLE_LIST> *join_list;/* join list the table belongs to   */
  bool		cacheable_table;	/* stop PS caching */
  /* used in multi-upd/views privilege check */
  bool		table_in_first_from_clause;
  /**
     Specifies which kind of table should be open for this element
     of table list.
  */
  enum enum_open_type open_type;
  /* TRUE if this merged view contain auto_increment field */
  bool          contain_auto_increment;
  /// TRUE <=> VIEW CHECK OPTION condition is processed (also for prep. stmts)
  bool          check_option_processed;
  /// TRUE <=> Filter condition is processed
  bool          replace_filter_processed;
  /* FRMTYPE_ERROR if any type is acceptable */
  enum frm_type_enum required_type;
  char		timestamp_buffer[20];	/* buffer for timestamp (19+1) */
  /*
    This TABLE_LIST object is just placeholder for prelocking, it will be
    used for implicit LOCK TABLES only and won't be used in real statement.
  */
  bool          prelocking_placeholder;
  /**
     Indicates that if TABLE_LIST object corresponds to the table/view
     which requires special handling.
  */
  enum
  {
    /* Normal open. */
    OPEN_NORMAL= 0,
    /* Associate a table share only if the the table exists. */
    OPEN_IF_EXISTS,
    /*
      Associate a table share only if the the table exists.
      Also upgrade metadata lock to exclusive if table doesn't exist.
    */
    OPEN_FOR_CREATE,
    /* Don't associate a table share. */
    OPEN_STUB
  } open_strategy;
  bool          internal_tmp_table;
  /** TRUE if an alias for this table was specified in the SQL. */
  bool          is_alias;
  /** TRUE if the table is referred to in the statement using a fully
      qualified name (<db_name>.<table_name>).
  */
  bool          is_fqtn;


  /* View creation context. */

  View_creation_ctx *view_creation_ctx;

  /*
    Attributes to save/load view creation context in/from frm-file.

    Ther are required only to be able to use existing parser to load
    view-definition file. As soon as the parser parsed the file, view
    creation context is initialized and the attributes become redundant.

    These attributes MUST NOT be used for any purposes but the parsing.
  */

  LEX_STRING view_client_cs_name;
  LEX_STRING view_connection_cl_name;

  /*
    View definition (SELECT-statement) in the UTF-form.
  */

  LEX_STRING view_body_utf8;

   /* End of view definition context. */
  /* List of possible keys. Valid only for materialized derived tables/views. */
  List<Derived_key> derived_key_list;

  /**
    Indicates what triggers we need to pre-load for this TABLE_LIST
    when opening an associated TABLE. This is filled after
    the parsed tree is created.
  */
  uint8 trg_event_map;
  uint i_s_requested_object;
  bool has_db_lookup_value;
  bool has_table_lookup_value;
  uint table_open_method;
  enum enum_schema_table_state schema_table_state;

  MDL_request mdl_request;

  /// if true, EXPLAIN can't explain view due to insufficient rights.
  bool view_no_explain;

  /* List to carry partition names from PARTITION (...) clause in statement */
  List<String> *partition_names;

  /// Set table number
  void set_tableno(uint tableno)
  {
    DBUG_ASSERT(tableno < MAX_TABLES);
    m_tableno= tableno;
    m_map= (table_map)1 << tableno;
  }
  /// Return table number
  uint tableno() const { return m_tableno; }

  /// Return table map derived from table number
  table_map map() const
  {
    DBUG_ASSERT(((table_map)1 << m_tableno) == m_map);
    return m_map;
  }

private:
  /*
    A group of members set and used only during JOIN::optimize().
  */
  /**
     Optimized copy of m_join_cond (valid for one single
     execution). Initialized by SELECT_LEX::get_optimizable_conditions().
     @todo it would be good to reset it in reinit_before_use(), if
     reinit_stmt_before_use() had a loop including join nests.
  */
  Item          *m_join_cond_optim;
public:

  COND_EQUAL    *cond_equal;            ///< Used with outer join
  /// true <=> this table is a const one and was optimized away.
  bool optimized_away;
  /**
    true <=> all possible keys for a derived table were collected and
    could be re-used while statement re-execution.
  */
  bool derived_keys_ready;
  // End of group for optimization

private:
  /** See comments for set_metadata_id() */
  enum enum_table_ref_type m_table_ref_type;
  /** See comments for TABLE_SHARE::get_table_ref_version() */
  ulonglong m_table_ref_version;
};


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


/**
  Iterator over the fields of a merged derived table or view.
*/

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
  const char *get_table_name();
  const char *get_db_name();
  GRANT_INFO *grant();
  Item *create_item(THD *thd) { return field_it->create_item(thd); }
  Field *field() { return field_it->field(); }
  Natural_join_column *get_or_create_column_ref(THD *thd, TABLE_LIST *parent_table_ref);
  Natural_join_column *get_natural_column_ref();
};

/**
  Semijoin_mat_optimize collects data used when calculating the cost of
  executing a semijoin operation using a materialization strategy.
  It is used during optimization phase only.
*/

struct Semijoin_mat_optimize
{
  /// Optimal join order calculated for inner tables of this semijoin op.
  struct st_position *positions;
  /// True if data types allow the MaterializeLookup semijoin strategy
  bool lookup_allowed;
  /// True if data types allow the MaterializeScan semijoin strategy
  bool scan_allowed;
  /// Expected #rows in the materialized table
  double expected_rowcount;
  /// Materialization cost - execute sub-join and write rows to temp.table
  Cost_estimate materialization_cost;
  /// Cost to make one lookup in the temptable
  Cost_estimate lookup_cost;
  /// Cost of scanning the materialized table
  Cost_estimate scan_cost;
  /// Array of pointers to fields in the materialized table.
  Item_field **mat_fields;
};


/**
  Struct st_nested_join is used to represent how tables are connected through
  outer join operations and semi-join operations to form a query block.
  Out of the parser, inner joins are also represented by st_nested_join
  structs, but these are later flattened out by simplify_joins().
  Some outer join nests are also flattened, when it can be determined that
  they can be processed as inner joins instead of outer joins.
*/
typedef struct st_nested_join
{
  List<TABLE_LIST>  join_list;       /* list of elements in the nested join */
  table_map         used_tables;     /* bitmap of tables in the nested join */
  table_map         not_null_tables; /* tables that rejects nulls           */
  /**
    Used for pointing out the first table in the plan being covered by this
    join nest. It is used exclusively within make_outerjoin_info().
   */
  plan_idx first_nested;
  /**
    Set to true when natural join or using information has been processed.
  */
  bool natural_join_processed;
  /**
    Number of tables and outer join nests administered by this nested join
    object for the sake of cost analysis. Includes direct member tables as
    well as tables included through semi-join nests, but notice that semi-join
    nests themselves are not counted.
  */
  uint              nj_total;
  /**
    Used to count tables in the nested join in 2 isolated places:
    1. In make_outerjoin_info(). 
    2. check_interleaving_with_nj/backout_nj_state (these are called
       by the join optimizer. 
    Before each use the counters are zeroed by SELECT_LEX::reset_nj_counters.
  */
  uint              nj_counter;
  /**
    Bit identifying this nested join. Only nested joins representing the
    outer join structure need this, other nests have bit set to zero.
  */
  nested_join_map   nj_map;
  /**
    Tables outside the semi-join that are used within the semi-join's
    ON condition (ie. the subquery WHERE clause and optional IN equalities).
  */
  table_map         sj_depends_on;
  /**
    Outer non-trivially correlated tables, a true subset of sj_depends_on
  */
  table_map         sj_corr_tables;
  /**
    Query block id if this struct is generated from a subquery transform.
  */
  uint query_block_id;

  /// Bitmap of which strategies are enabled for this semi-join nest
  uint sj_enabled_strategies;

  /*
    Lists of trivially-correlated expressions from the outer and inner tables
    of the semi-join, respectively.
  */
  List<Item>        sj_outer_exprs, sj_inner_exprs;
  Semijoin_mat_optimize sjm;
} NESTED_JOIN;


typedef struct st_open_table_list{
  struct st_open_table_list *next;
  char	*db,*table;
  uint32 in_use,locked;
} OPEN_TABLE_LIST;


static inline my_bitmap_map *tmp_use_all_columns(TABLE *table,
                                                 MY_BITMAP *bitmap)
{
  my_bitmap_map *old= bitmap->bitmap;
  bitmap->bitmap= table->s->all_set.bitmap;// does not repoint last_word_ptr
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


/* 
  Variant of the above : handle both read and write sets.
  Provide for the possiblity of the read set being the same as the write set
*/
static inline void dbug_tmp_use_all_columns(TABLE *table,
                                            my_bitmap_map **save,
                                            MY_BITMAP *read_set,
                                            MY_BITMAP *write_set)
{
#ifndef DBUG_OFF
  save[0]= read_set->bitmap;
  save[1]= write_set->bitmap;
  (void) tmp_use_all_columns(table, read_set);
  (void) tmp_use_all_columns(table, write_set);
#endif
}


static inline void dbug_tmp_restore_column_maps(MY_BITMAP *read_set,
                                                MY_BITMAP *write_set,
                                                my_bitmap_map **old)
{
#ifndef DBUG_OFF
  tmp_restore_column_map(read_set, old[0]);
  tmp_restore_column_map(write_set, old[1]);
#endif
}


size_t max_row_length(TABLE *table, const uchar *data);


void init_mdl_requests(TABLE_LIST *table_list);

int open_table_from_share(THD *thd, TABLE_SHARE *share, const char *alias,
                          uint db_stat, uint prgflag, uint ha_open_flags,
                          TABLE *outparam, bool is_create_table);
TABLE_SHARE *alloc_table_share(TABLE_LIST *table_list, const char *key,
                               size_t key_length);
void init_tmp_table_share(THD *thd, TABLE_SHARE *share, const char *key,
                          size_t key_length,
                          const char *table_name, const char *path);
void free_table_share(TABLE_SHARE *share);


/**
  Get the tablespace name for a table.

  This function will open the .FRM file for the given TABLE_LIST element
  and fill Tablespace_hash_set with the tablespace name used by table and
  table partitions, if present. For NDB tables with version before 50120,
  the function will ask the SE for the tablespace name, because for these
  tables, the tablespace name is not stored in the.FRM file, but only
  within the SE itself.

  @note The function does *not* consider errors. If the file is not present,
        this does not raise an error. The reason is that this function will
        be used for tables that may not exist, e.g. in the context of
        'DROP TABLE IF EXISTS', which does not care whether the table
        exists or not. The function returns success in this case.

  @note Strings inserted into hash are allocated in the memory
        root of the thd, and will be freed implicitly.

  @param thd    - Thread context.
  @param table  - Table from which we read the tablespace names.
  @param tablespace_set (OUT)- Hash set to be filled with tablespace names.

  @retval true  - On failure, especially due to memory allocation errors
                  and partition string parse errors.
  @retval false - On success. Even if tablespaces are not used by table.
*/

bool get_table_and_parts_tablespace_names(
       THD *thd,
       TABLE_LIST *table,
       Tablespace_hash_set *tablespace_set);

int open_table_def(THD *thd, TABLE_SHARE *share, uint db_flags);
void open_table_error(TABLE_SHARE *share, int error, int db_errno, int errarg);
void update_create_info_from_table(HA_CREATE_INFO *info, TABLE *form);
enum_ident_name_check check_and_convert_db_name(LEX_STRING *db,
                                                bool preserve_lettercase);
bool check_column_name(const char *name);
enum_ident_name_check check_table_name(const char *name, size_t length,
                                       bool check_for_path_chars);
int rename_file_ext(const char * from,const char * to,const char * ext);
char *get_field(MEM_ROOT *mem, Field *field);
bool get_field(MEM_ROOT *mem, Field *field, class String *res);

int closefrm(TABLE *table, bool free_share);
int read_string(File file, uchar* *to, size_t length);
void free_blobs(TABLE *table);
void free_blob_buffers_and_reset(TABLE *table, uint32 size);
int set_zone(int nr,int min_zone,int max_zone);
ulong make_new_entry(File file,uchar *fileinfo,TYPELIB *formnames,
		     const char *newname);
ulong next_io_size(ulong pos);
void append_unescaped(String *res, const char *pos, size_t length);
File create_frm(THD *thd, const char *name, const char *db,
                const char *table, uint reclength, uchar *fileinfo,
  		HA_CREATE_INFO *create_info, uint keys, KEY *key_info);
char *fn_rext(char *name);

/* performance schema */
extern LEX_STRING PERFORMANCE_SCHEMA_DB_NAME;

extern LEX_STRING GENERAL_LOG_NAME;
extern LEX_STRING SLOW_LOG_NAME;

/* information schema */
extern LEX_STRING INFORMATION_SCHEMA_NAME;
extern LEX_STRING MYSQL_SCHEMA_NAME;

/* replication's tables */
extern LEX_STRING RLI_INFO_NAME;
extern LEX_STRING MI_INFO_NAME;
extern LEX_STRING WORKER_INFO_NAME;

inline bool is_infoschema_db(const char *name, size_t len)
{
  return (INFORMATION_SCHEMA_NAME.length == len &&
          !my_strcasecmp(system_charset_info,
                         INFORMATION_SCHEMA_NAME.str, name));
}

inline bool is_infoschema_db(const char *name)
{
  return !my_strcasecmp(system_charset_info,
                        INFORMATION_SCHEMA_NAME.str, name);
}

inline bool is_perfschema_db(const char *name, size_t len)
{
  return (PERFORMANCE_SCHEMA_DB_NAME.length == len &&
          !my_strcasecmp(system_charset_info,
                         PERFORMANCE_SCHEMA_DB_NAME.str, name));
}

inline bool is_perfschema_db(const char *name)
{
  return !my_strcasecmp(system_charset_info,
                        PERFORMANCE_SCHEMA_DB_NAME.str, name);
}

TYPELIB *typelib(MEM_ROOT *mem_root, List<String> &strings);

/**
  return true if the table was created explicitly.
*/
inline bool is_user_table(TABLE * table)
{
  const char *name= table->s->table_name.str;
  return strncmp(name, tmp_file_prefix, tmp_file_prefix_length);
}

bool is_simple_order(ORDER *order);

void repoint_field_to_record(TABLE *table, uchar *old_rec, uchar *new_rec);
bool update_generated_write_fields(const MY_BITMAP *bitmap, TABLE *table);
bool update_generated_read_fields(uchar *buf, TABLE *table,
                                  uint active_index= MAX_KEY);

#endif /* MYSQL_CLIENT */

#endif /* TABLE_INCLUDED */
