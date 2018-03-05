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
#include "sql_plist.h"
#include "sql_list.h"                           /* Sql_alloc */
#include "mdl.h"
#include "datadict.h"

#ifndef MYSQL_CLIENT

#include "hash.h"                               /* HASH */
#include "handler.h"                /* row_type, ha_choice, handler */
#include "mysql_com.h"              /* enum_field_types */
#include "thr_lock.h"                  /* thr_lock_type */

/* Structs that defines the TABLE */

class Item;				/* Needed by ORDER */
class Item_subselect;
class Item_field;
class GRANT_TABLE;
class st_select_lex_unit;
class st_select_lex;
class partition_info;
class COND_EQUAL;
class Security_context;
struct TABLE_LIST;
class ACL_internal_schema_access;
class ACL_internal_table_access;
class Field;

/*
  Used to identify NESTED_JOIN structures within a join (applicable only to
  structures that have not been simplified away and embed more the one
  element)
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
  CHARSET_INFO *get_client_cs()
  {
    return m_client_cs;
  }

  CHARSET_INFO *get_connection_cl()
  {
    return m_connection_cl;
  }

protected:
  Default_object_creation_ctx(THD *thd);

  Default_object_creation_ctx(CHARSET_INFO *client_cs,
                              CHARSET_INFO *connection_cl);

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
  CHARSET_INFO *m_client_cs;

  /**
    connection_cl stores the value of collation_connection session
    variable. Both character set and collation attributes are used.

    Connection collation is included into query context, becase it defines
    the character set and collation of text literals in internal
    representation of query (item-objects).
  */
  CHARSET_INFO *m_connection_cl;
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

/* Order clause list element */

typedef struct st_order {
  struct st_order *next;
  Item	 **item;			/* Point at item in select fields */
  Item	 *item_ptr;			/* Storage for initial item */
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
   process. This information is stored in privilege, want_privilege, and
   orig_want_privilege.

   A GRANT_INFO also serves as a cache of the privilege hash tables. Relevant
   members are grant_table and version.
 */
typedef struct st_grant_info
{
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
  /**
     @brief the set of privileges that the current user needs to fulfil in
     order to carry out the requested operation.
   */
  ulong want_privilege;
  /**
    Stores the requested access acl of top level tables list. Is used to
    check access rights to the underlying tables of a view.
  */
  ulong orig_want_privilege;
  /** The grant state for internal tables. */
  GRANT_INTERNAL_INFO m_internal;
} GRANT_INFO;

enum tmp_table_type
{
  NO_TMP_TABLE, NON_TRANSACTIONAL_TMP_TABLE, TRANSACTIONAL_TMP_TABLE,
#ifndef MCP_WL3749
  INTERNAL_TMP_TABLE, SYSTEM_TMP_TABLE, TMP_TABLE_FRM_FILE_ONLY
#else
  INTERNAL_TMP_TABLE, SYSTEM_TMP_TABLE
#endif
};
enum release_type { RELEASE_NORMAL, RELEASE_WAIT_FOR_DROP };

typedef struct st_filesort_info
{
  IO_CACHE *io_cache;           /* If sorted through filesort */
  uchar     **sort_keys;        /* Buffer for sorting keys */
  size_t    sort_keys_size;     /* Number of bytes allocated */
  uchar     *buffpek;           /* Buffer for buffpek structures */
  uint      buffpek_len;        /* Max number of buffpeks in the buffer */
  uchar     *addon_buf;         /* Pointer to a buffer if sorted with fields */
  size_t    addon_length;       /* Length of the buffer */
  struct st_sort_addon_field *addon_field;     /* Pointer to the fields info */
  void    (*unpack)(struct st_sort_addon_field *, uchar *); /* To unpack back */
  uchar     *record_pointers;    /* If sorted in memory */
  ha_rows   found_records;      /* How many records in sort */
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
  TABLE_CATEGORY_PERFORMANCE=6
};
typedef enum enum_table_category TABLE_CATEGORY;

TABLE_CATEGORY get_table_category(const LEX_STRING *db,
                                  const LEX_STRING *name);


struct TABLE_share;

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


#ifdef WITH_PARTITION_STORAGE_ENGINE
/**
  Partition specific ha_data struct.
*/
typedef struct st_ha_data_partition
{
  bool auto_inc_initialized;
  mysql_mutex_t LOCK_auto_inc;                 /**< protecting auto_inc val */
  ulonglong next_auto_inc_val;                 /**< first non reserved value */
} HA_DATA_PARTITION;
#endif


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

  /*
    Doubly-linked (back-linked) lists of used and unused TABLE objects
    for this share.
  */
  I_P_List <TABLE, TABLE_share> used_tables;
  I_P_List <TABLE, TABLE_share> free_tables;

  /* The following is copied to each TABLE on OPEN */
  Field **field;
  Field **found_next_number_field;
  Field *timestamp_field;               /* Used only during open */
  KEY  *key_info;			/* data of keys in database */
  uint	*blob_field;			/* Index to blobs in Field arrray*/

  uchar	*default_values;		/* row with default values */
  LEX_STRING comment;			/* Comment about table */
  CHARSET_INFO *table_charset;		/* Default charset of string fields */

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
  ulong   version, mysql_version;
  ulong   reclength;			/* Recordlength */

  plugin_ref db_plugin;			/* storage engine plugin */
  inline handlerton *db_type() const	/* table_type for handler */
  { 
    // DBUG_ASSERT(db_plugin);
    return db_plugin ? plugin_data(db_plugin, handlerton*) : NULL;
  }
  enum row_type row_type;		/* How rows are stored */
  enum tmp_table_type tmp_table;

  uint ref_count;                       /* How many TABLE objects uses this */
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
  uint rowid_field_offset;		/* Field_nr +1 to rowid field */
  /* Primary key index number, used in TABLE::key_info[] */
  uint primary_key;                     
  uint next_number_index;               /* autoincrement key number */
  uint next_number_key_offset;          /* autoinc keypart offset in a key */
  uint next_number_keypart;             /* autoinc keypart number in a key */
  uint error, open_errno, errarg;       /* error from open_table_def() */
  uint column_bitmap_size;
  uchar frm_version;
  bool null_field_first;
  bool system;                          /* Set if system table (one record) */
  bool crypted;                         /* If .frm file is crypted */
  bool db_low_byte_first;		/* Portable row format */
  bool crashed;
  bool is_view;
  ulong table_map_id;                   /* for row-based replication */

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

#ifdef WITH_PARTITION_STORAGE_ENGINE
  /* filled in when reading from frm */
  bool auto_partitioned;
  char *partition_info_str;
  uint  partition_info_str_len;
  uint  partition_info_buffer_size;
  handlerton *default_part_db_type;
#endif

  /**
    Cache the checked structure of this table.

    The pointer data is used to describe the structure that
    a instance of the table must have. Each element of the
    array specifies a field that must exist on the table.

    The pointer is cached in order to perform the check only
    once -- when the table is loaded from the disk.
  */
  const TABLE_FIELD_DEF *table_field_def_cache;

  /** place to store storage engine specific data */
  void *ha_data;
  void (*ha_data_destroy)(void *); /* An optional destructor for ha_data */

#ifdef WITH_PARTITION_STORAGE_ENGINE
  /** place to store partition specific data, LOCK_ha_data hold while init. */
  HA_DATA_PARTITION *ha_part_data;
  /* Destructor for ha_part_data */
  void (*ha_part_data_destroy)(HA_DATA_PARTITION *);
#endif


  /** Instrumentation for this table share. */
  PSI_table_share *m_psi;

  /**
    List of tickets representing threads waiting for the share to be flushed.
  */
  Wait_for_flush_list m_flush_tickets;

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

  void set_table_cache_key(char *key_buff, uint key_length)
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

  void set_table_cache_key(char *key_buff, const char *key, uint key_length)
  {
    memcpy(key_buff, key, key_length);
    set_table_cache_key(key_buff, key_length);
  }

  inline bool honor_global_locks()
  {
    return ((table_category == TABLE_CATEGORY_USER)
            || (table_category == TABLE_CATEGORY_SYSTEM));
  }

  inline bool require_write_privileges()
  {
    return (table_category == TABLE_CATEGORY_LOG);
  }

  inline ulong get_table_def_version()
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
     * for base tables, we return table_map_id.
       It is assigned from a global counter incremented for each
       new table loaded into the table definition cache (TDC).
     * for temporary tables it's table_map_id again. But for
       temporary tables table_map_id is assigned from
       thd->query_id. The latter is assigned from a thread local
       counter incremented for every new SQL statement. Since
       temporary tables are thread-local, each temporary table
       gets a unique id.
     * for everything else (views, information schema tables),
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

   Secondly, for base tables, we know that each DDL flushes the
   respective share from the TDC. This ensures that whenever
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

   Views are a special and tricky case. A view is always inlined
   into the parse tree of a prepared statement at prepare.
   Thus, when we execute a prepared statement, the parse tree
   will not get modified even if the view is replaced with another
   view.  Therefore, we can safely choose 0 for version id of
   views and effectively never invalidate a prepared statement
   when a view definition is altered. Note, that this leads to
   wrong binary log in statement-based replication, since we log
   prepared statement execution in form Query_log_events
   containing conventional statements. But since there is no
   metadata locking for views, the very same problem exists for
   conventional statements alone, as reported in Bug#25144. The only
   difference between prepared and conventional execution is,
   effectively, that for prepared statements the race condition
   window is much wider.
   In 6.0 we plan to support view metadata locking (WL#3726) and
   extend table definition cache to cache views (WL#4298).
   When this is done, views will be handled in the same fashion
   as the base tables.

   Finally, by taking into account table type, we always
   track that a change has taken place when a view is replaced
   with a base table, a base table is replaced with a temporary
   table and so on.

   @sa TABLE_LIST::is_table_ref_id_equal()
  */
  ulong get_table_ref_version() const
  {
    return (tmp_table == SYSTEM_TMP_TABLE || is_view) ? 0 : table_map_id;
  }

  bool visit_subgraph(Wait_for_flush *waiting_ticket,
                      MDL_wait_for_graph_visitor *gvisitor);

  bool wait_for_old_version(THD *thd, struct timespec *abstime,
                            uint deadlock_weight);
  /** Release resources and free memory occupied by the table share. */
  void destroy();
};


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

  TABLE_SHARE	*s;
  handler	*file;
  TABLE *next, *prev;

private:
  /**
     Links for the lists of used/unused TABLE objects for this share.
     Declared as private to avoid direct manipulation with those objects.
     One should use methods of I_P_List template instead.
  */
  TABLE *share_next, **share_prev;

  friend struct TABLE_share;

public:

  THD	*in_use;                        /* Which thread uses this */
  Field **field;			/* Pointer to fields */

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
  KEY  *key_info;			/* data of keys in database */

  Field *next_number_field;		/* Set if next_number is activated */
  Field *found_next_number_field;	/* Set on open */
  Field_timestamp *timestamp_field;

  /* Table's triggers, 0 if there are no of them */
  Table_triggers_list *triggers;
  TABLE_LIST *pos_in_table_list;/* Element referring to this table */
  /* Position in thd->locked_table_list under LOCK TABLES */
  TABLE_LIST *pos_in_locked_tables;
  ORDER		*group;
  const char	*alias;            	  /* alias or table name */
  uchar		*null_flags;
  my_bitmap_map	*bitmap_init_value;
  MY_BITMAP     def_read_set, def_write_set, tmp_set; /* containers */
  MY_BITMAP     *read_set, *write_set;          /* Active column sets */
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
  my_bool distinct,const_table,no_rows;

  /**
     If set, the optimizer has found that row retrieval should access index 
     tree only.
   */
  my_bool key_read;
  my_bool no_keyread;
  my_bool locked_by_logger;
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
  my_bool m_needs_reopen;

  REGINFO reginfo;			/* field connections */
  MEM_ROOT mem_root;
  GRANT_INFO grant;
  FILESORT_INFO sort;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  partition_info *part_info;            /* Partition related information */
  bool no_partitions_used; /* If true, all partitions have been pruned away */
#endif
  MDL_ticket *mdl_ticket;

  void init(THD *thd, TABLE_LIST *tl);
  bool fill_item_list(List<Item> *item_list) const;
  void reset_item_list(List<Item> *item_list) const;
  void clear_column_bitmaps(void);
  void prepare_for_position(void);
  void mark_columns_used_by_index_no_reset(uint index, MY_BITMAP *map);
  void mark_columns_used_by_index(uint index);
  void add_read_columns_used_by_index(uint index);
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
  /** Should this instance of the table be reopened? */
  inline bool needs_reopen()
  { return !db_stat || m_needs_reopen; }

  inline void set_keyread(bool flag)
  {
    DBUG_ASSERT(file);
    if (flag && !key_read)
    {
      key_read= 1;
      file->extra(HA_EXTRA_KEYREAD);
    }
    else if (!flag && key_read)
    {
      key_read= 0;
      file->extra(HA_EXTRA_NO_KEYREAD);
    }
  }

  bool update_const_key_parts(COND *conds);
};


/**
   Helper class which specifies which members of TABLE are used for
   participation in the list of used/unused TABLE objects for the share.
*/

struct TABLE_share
{
  static inline TABLE **next_ptr(TABLE *l)
  {
    return &l->share_next;
  }
  static inline TABLE ***prev_ptr(TABLE *l)
  {
    return &l->share_prev;
  }
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
typedef class Item COND;

typedef struct st_schema_table
{
  const char* table_name;
  ST_FIELD_INFO *fields_info;
  /* Create information_schema table */
  TABLE *(*create_table)  (THD *thd, TABLE_LIST *table_list);
  /* Fill table with data */
  int (*fill_table) (THD *thd, TABLE_LIST *tables, COND *cond);
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

/** The threshold size a blob field buffer before it is freed */
#define MAX_TDC_BLOB_SIZE 65536

class select_union;
class TMP_TABLE_PARAM;

Item *create_view_field(THD *thd, TABLE_LIST *view, Item **field_ref,
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


/**
   Type of table which can be open for an element of table list.
*/

enum enum_open_type
{
  OT_TEMPORARY_OR_BASE= 0, OT_TEMPORARY_ONLY, OT_BASE_ONLY
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

struct LEX;
class Index_hint;
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
                             enum thr_lock_type lock_type_arg)
  {
    bzero((char*) this, sizeof(*this));
    db= (char*) db_name_arg;
    db_length= db_length_arg;
    table_name= (char*) table_name_arg;
    table_name_length= table_name_length_arg;
    alias= (char*) alias_arg;
    lock_type= lock_type_arg;
    mdl_request.init(MDL_key::TABLE, db, table_name,
                     (lock_type >= TL_WRITE_ALLOW_WRITE) ?
                     MDL_SHARED_WRITE : MDL_SHARED_READ,
                     MDL_TRANSACTION);
  }

  /*
    List of tables local to a subquery (used by SQL_I_List). Considers
    views as leaves (unlike 'next_leaf' below). Created at parse time
    in st_select_lex::add_table_to_list() -> table_list.link_in_list().
  */
  TABLE_LIST *next_local;
  /* link in a global list of all queries tables */
  TABLE_LIST *next_global, **prev_global;
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
  TABLE_LIST	*correspondent_table;
  /**
     @brief Normally, this field is non-null for anonymous derived tables only.

     @details This field is set to non-null for 
     
     - Anonymous derived tables, In this case it points to the SELECT_LEX_UNIT
     representing the derived table. E.g. for a query
     
     @verbatim SELECT * FROM (SELECT a FROM t1) b @endverbatim
     
     For the @c TABLE_LIST representing the derived table @c b, @c derived
     points to the SELECT_LEX_UNIT representing the result of the query within
     parenteses.
     
     - Views. This is set for views with @verbatim ALGORITHM = TEMPTABLE
     @endverbatim by mysql_make_view().
     
     @note Inside views, a subquery in the @c FROM clause is not allowed.
     @note Do not use this field to separate views/base tables/anonymous
     derived tables. Use TABLE_LIST::is_anonymous_derived_table().
  */
  st_select_lex_unit *derived;		/* SELECT_LEX_UNIT of derived table */
  ST_SCHEMA_TABLE *schema_table;        /* Information_schema table */
  st_select_lex	*schema_select_lex;
  /*
    True when the view field translation table is used to convert
    schema table fields for backwards compatibility with SHOW command.
  */
  bool schema_table_reformed;
  TMP_TABLE_PARAM *schema_table_param;
  /* link to select_lex where this table was used */
  st_select_lex	*select_lex;
  LEX *view;                    /* link on VIEW lex for merging */
  Field_translator *field_translation;	/* array of VIEW fields */
  /* pointer to element after last one in translation table above */
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
    leaves. Created in setup_tables() -> make_leaves_list().
  */
  bool allowed_show;
  TABLE_LIST	*next_leaf;
  Item          *where;                 /* VIEW WHERE clause condition */
  Item          *check_option;          /* WITH CHECK OPTION condition */
  LEX_STRING	select_stmt;		/* text of (CREATE/SELECT) statement */
  LEX_STRING	md5;			/* md5 of query text */
  LEX_STRING	source;			/* source of CREATE VIEW */
  LEX_STRING	view_db;		/* saved view database */
  LEX_STRING	view_name;		/* saved view name */
  LEX_STRING	timestamp;		/* GMT time stamp of last operation */
  st_lex_user   definer;                /* definer of view */
  ulonglong	file_version;		/* version of file's field set */
  ulonglong     updatable_view;         /* VIEW can be updated */
  /** 
      @brief The declared algorithm, if this is a view.
      @details One of
      - VIEW_ALGORITHM_UNDEFINED
      - VIEW_ALGORITHM_TMPTABLE
      - VIEW_ALGORITHM_MERGE
      @to do Replace with an enum 
  */
  ulonglong	algorithm;
  ulonglong     view_suid;              /* view is suid (TRUE dy default) */
  ulonglong     with_check;             /* WITH CHECK OPTION */
  /*
    effective value of WITH CHECK OPTION (differ for temporary table
    algorithm)
  */
  uint8         effective_with_check;
  /** 
      @brief The view algorithm that is actually used, if this is a view.
      @details One of
      - VIEW_ALGORITHM_UNDEFINED
      - VIEW_ALGORITHM_TMPTABLE
      - VIEW_ALGORITHM_MERGE
      @to do Replace with an enum 
  */
  uint8         effective_algorithm;
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
  bool          updatable;		/* VIEW/TABLE can be updated now */
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
  bool          multitable_view;        /* TRUE iff this is multitable view */
  bool          compact_view_format;    /* Use compact format for SHOW CREATE VIEW */
  /* view where processed */
  bool          where_processed;
  /* TRUE <=> VIEW CHECK OPTION expression has been processed */
  bool          check_option_processed;
  /* FRMTYPE_ERROR if any type is acceptable */
  enum frm_type_enum required_type;
  handlerton	*db_type;		/* table_type for handler */
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
    /* Don't associate a table share. */
    OPEN_STUB
  } open_strategy;
  /* For transactional locking. */
  int           lock_timeout;           /* NOWAIT or WAIT [X]               */
  bool          lock_transactional;     /* If transactional lock requested. */
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

  /**
    Indicates what triggers we need to pre-load for this TABLE_LIST
    when opening an associated TABLE. This is filled after
    the parsed tree is created.
  */
  uint8 trg_event_map;
  /* TRUE <=> this table is a const one and was optimized away. */
  bool optimized_away;

  uint i_s_requested_object;
  bool has_db_lookup_value;
  bool has_table_lookup_value;
  uint table_open_method;
  enum enum_schema_table_state schema_table_state;

  MDL_request mdl_request;

  void calc_md5(char *buffer);
  void set_underlying_merge();
  int view_check_option(THD *thd, bool ignore_failure);
  bool setup_underlying(THD *thd);
  void cleanup_items();
  bool placeholder()
  {
    return derived || view || schema_table || !table;
  }
  void print(THD *thd, String *str, enum_query_type query_type);
  bool check_single_table(TABLE_LIST **table, table_map map,
                          TABLE_LIST *view);
  bool set_insert_values(MEM_ROOT *mem_root);
  void hide_view_error(THD *thd);
  TABLE_LIST *find_underlying_table(TABLE *table);
  TABLE_LIST *first_leaf_for_name_resolution();
  TABLE_LIST *last_leaf_for_name_resolution();
  bool is_leaf_for_name_resolution();
  inline TABLE_LIST *top_table()
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
  Item_subselect *containing_subselect();

  /* 
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
  inline
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
  inline
  void set_table_ref_id(TABLE_SHARE *s)
  { set_table_ref_id(s->get_table_ref_type(), s->get_table_ref_version()); }

  inline
  void set_table_ref_id(enum_table_ref_type table_ref_type_arg,
                        ulong table_ref_version_arg)
  {
    m_table_ref_type= table_ref_type_arg;
    m_table_ref_version= table_ref_version_arg;
  }

  /**
     @brief True if this TABLE_LIST represents an anonymous derived table,
     i.e.  the result of a subquery.
  */
  bool is_anonymous_derived_table() const { return derived && !view; }

  /**
     @brief Returns the name of the database that the referenced table belongs
     to.
  */
  char *get_db_name() { return view != NULL ? view_db.str : db; }

  /**
     @brief Returns the name of the table that this TABLE_LIST represents.

     @details The unqualified table name or view name for a table or view,
     respectively.
   */
  char *get_table_name() { return view != NULL ? view_name.str : table_name; }

private:
  bool prep_check_option(THD *thd, uint8 check_opt_type);
  bool prep_where(THD *thd, Item **conds, bool no_where_clause);
  /** See comments for set_metadata_id() */
  enum enum_table_ref_type m_table_ref_type;
  /** See comments for set_metadata_id() */
  ulong m_table_ref_version;
};

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
  const char *get_table_name();
  const char *get_db_name();
  GRANT_INFO *grant();
  Item *create_item(THD *thd) { return field_it->create_item(thd); }
  Field *field() { return field_it->field(); }
  Natural_join_column *get_or_create_column_ref(THD *thd, TABLE_LIST *parent_table_ref);
  Natural_join_column *get_natural_column_ref();
};


typedef struct st_nested_join
{
  List<TABLE_LIST>  join_list;       /* list of elements in the nested join */
  table_map         used_tables;     /* bitmap of tables in the nested join */
  table_map         not_null_tables; /* tables that rejects nulls           */
  /**
    Used for pointing out the first table in the plan being covered by this
    join nest. It is used exclusively within make_outerjoin_info().
   */
  struct st_join_table *first_nested;
  /* 
    Used to count tables in the nested join in 2 isolated places:
    1. In make_outerjoin_info(). 
    2. check_interleaving_with_nj/restore_prev_nj_state (these are called
       by the join optimizer. 
    Before each use the counters are zeroed by reset_nj_counters.
  */
  uint              counter;
  nested_join_map   nj_map;          /* Bit used to identify this nested join*/
  /**
     True if this join nest node is completely covered by the query execution
     plan. This means two things.

     1. All tables on its @c join_list are covered by the plan.

     2. All child join nest nodes are fully covered.
   */
  bool is_fully_covered() const { return join_list.elements == counter; }
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

#ifndef MCP_WL3749
/**
  Opening modes for open_temporary_table and open_table_from_share
*/
enum open_table_mode
{
  OTM_OPEN= 0,
  OTM_CREATE= 1,
  OTM_ALTER= 2
};
#endif
int open_table_from_share(THD *thd, TABLE_SHARE *share, const char *alias,
                          uint db_stat, uint prgflag, uint ha_open_flags,
#ifndef MCP_WL3749
                          TABLE *outparam, open_table_mode open_mode= OTM_OPEN);
#else
                          TABLE *outparam, bool is_create_table);
#endif
TABLE_SHARE *alloc_table_share(TABLE_LIST *table_list, char *key,
                               uint key_length);
void init_tmp_table_share(THD *thd, TABLE_SHARE *share, const char *key,
                          uint key_length,
                          const char *table_name, const char *path);
void free_table_share(TABLE_SHARE *share);
int open_table_def(THD *thd, TABLE_SHARE *share, uint db_flags);
void open_table_error(TABLE_SHARE *share, int error, int db_errno, int errarg);
void update_create_info_from_table(HA_CREATE_INFO *info, TABLE *form);
bool check_and_convert_db_name(LEX_STRING *db, bool preserve_lettercase);
bool check_db_name(LEX_STRING *db);
bool check_column_name(const char *name);
bool check_table_name(const char *name, size_t length, bool check_for_path_chars);
int rename_file_ext(const char * from,const char * to,const char * ext);
char *get_field(MEM_ROOT *mem, Field *field);
bool get_field(MEM_ROOT *mem, Field *field, class String *res);

int closefrm(TABLE *table, bool free_share);
int read_string(File file, uchar* *to, size_t length);
void free_blobs(TABLE *table);
void free_field_buffers_larger_than(TABLE *table, uint32 size);
int set_zone(int nr,int min_zone,int max_zone);
ulong get_form_pos(File file, uchar *head, TYPELIB *save_names);
ulong make_new_entry(File file,uchar *fileinfo,TYPELIB *formnames,
		     const char *newname);
ulong next_io_size(ulong pos);
void append_unescaped(String *res, const char *pos, uint length);
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

TYPELIB *typelib(MEM_ROOT *mem_root, List<String> &strings);

/**
  return true if the table was created explicitly.
*/
inline bool is_user_table(TABLE * table)
{
  const char *name= table->s->table_name.str;
  return strncmp(name, tmp_file_prefix, tmp_file_prefix_length);
}

inline void mark_as_null_row(TABLE *table)
{
  table->null_row=1;
  table->status|=STATUS_NULL_ROW;
  bfill(table->null_flags,table->s->null_bytes,255);
}

bool is_simple_order(ORDER *order);

#endif /* MYSQL_CLIENT */

#endif /* TABLE_INCLUDED */
