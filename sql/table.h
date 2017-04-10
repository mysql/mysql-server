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

#include <string.h>
#include <sys/types.h>

#include "binary_log_types.h"
#include "key.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sharedlib.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/psi_table.h"
#include "sql_alloc.h"
#include "sql_const.h"
#include "sql_list.h"
#include "sql_plist.h"
#include "sql_plugin_ref.h"
#include "system_variables.h"
#include "thr_lock.h"
#include "typelib.h"

class Field;
class Item;
class String;
class THD;
class partition_info;
struct TABLE;
struct TABLE_LIST;
struct TABLE_SHARE;

#include "enum_query_type.h" // enum_query_type
#include "handler.h"       // row_type
#include "mdl.h"           // MDL_wait_for_subgraph
#include "mem_root_array.h"
#include "opt_costmodel.h" // Cost_model_table
#include "record_buffer.h" // Record_buffer
#include "sql_bitmap.h"    // Bitmap
#include "sql_sort.h"      // Filesort_info
#include "table_id.h"      // Table_id

class ACL_internal_schema_access;
class ACL_internal_table_access;
class COND_EQUAL;
/* Structs that defines the TABLE */
class File_parser;
class GRANT_TABLE;
class Index_hint;
class Item_field;
class Query_result_union;
class SELECT_LEX_UNIT;
class Security_context;
class Table_cache_element;
class Table_trigger_dispatcher;
class Temp_table_param;
struct LEX;

typedef int8 plan_idx;
class Opt_hints_qb;
class Opt_hints_table;
class SELECT_LEX;

namespace dd {
  class Table;
  class View;

  enum class enum_table_type;
}
class Common_table_expr;
typedef Mem_root_array_YY<LEX_CSTRING> Create_col_name_list;

typedef int64 query_id_t;

#define store_record(A,B) memcpy((A)->B,(A)->record[0],(size_t) (A)->s->reclength)
#define restore_record(A,B) memcpy((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define cmp_record(A,B) memcmp((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define empty_record(A) { \
                          restore_record((A),s->default_values); \
                          if ((A)->s->null_bytes > 0) \
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
enum class Ident_name_check
{
  OK,
  WRONG,
  TOO_LONG
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
  bool is_explicit;  /* Whether ASC/DESC is explicitly specified */
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
     @todo remove this member in 8.0.
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

    2016-06-14 Contrary to what's written in these comments, the truth is:
    - tables created by CREATE TEMPORARY TABLE have TABLE_CATEGORY_USER
    - tables created by create_tmp_table() (internal ones) have
    TABLE_CATEGORY_TEMPORARY.
    ha_innodb.cc relies on this observation (so: grep it).  If you clean this
    up, you may also want to look at 'no_tmp_table'; its enum values' meanings
    have degraded over time: INTERNAL_TMP_TABLE is not used for some internal
    tmp tables (derived tables). Unification of both enums would be
    great. Whatever the result, we need to be able to distinguish the two
    types of temporary tables above, as usage patterns are more restricted for
    the second type, and allow more optimizations.
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
  TABLE_CATEGORY_GTID=8,

  /**
    A data dictionary table.
    Table's with this category will skip checking the
    TABLE_SHARE versions because these table structures
    are fixed upon server bootstrap.
  */
  TABLE_CATEGORY_DICTIONARY=9
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
  virtual void report_error(uint code, const char *fmt, ...)= 0;

public:
  Table_check_intact() {}
  virtual ~Table_check_intact() {}

  /**
    Checks whether a table is intact.

    @param thd Session.
    @param table Table to check.
    @param table_def Table definition struct.
  */
  bool check(THD *thd, TABLE *table, const TABLE_FIELD_DEF *table_def);
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

  /**
    The set of indexes that are not disabled for this table. I.e. it excludes
    indexes disabled by `ALTER TABLE ... DISABLE KEYS`, however it does
    include invisible indexes. The data dictionary populates this bitmap.
  */
  Key_map keys_in_use;

  /// The set of visible and enabled indexes for this table.
  Key_map visible_indexes;
  Key_map keys_for_keyread;
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
  /**
    Value of ROW_FORMAT option for the table as provided by user.
    Can be different from the real row format used by the storage
    engine. ROW_TYPE_DEFAULT value indicates that no explicit
    ROW_FORMAT was specified for the table. @sa real_row_type.
  */
  enum row_type row_type;
  /** Real row format used for the table by the storage engine. */
  enum row_type real_row_type;
  enum tmp_table_type tmp_table;

  /// How many TABLE objects use this.
  uint ref_count;
  /**
    Only for internal temporary tables.
    Count of TABLEs (having this TABLE_SHARE) which have a "handler"
    (table->file!=nullptr).
  */
  uint tmp_handler_count;

  uint key_block_size;			/* create key_block_size, if used */
  uint stats_sample_pages;		/* number of pages to sample during
					stats estimation, if used, otherwise 0. */
  enum_stats_auto_recalc stats_auto_recalc; /* Automatic recalc of stats. */
  uint null_bytes, last_null_bit_pos;
  uint fields;				/* Number of fields */
  uint rec_buff_length;                 /* Size of table->record[] buffer */
  uint keys;                            /* Number of keys defined for the table*/
  uint key_parts;                       /* Number of key parts of all keys
                                           defined for the table
                                        */
  uint max_key_length;                  /* Length of the longest key */
  uint max_unique_length;               /* Length of the longest unique key */
  uint total_key_length;
  uint null_fields;			/* number of null fields */
  uint blob_fields;			/* number of blob fields */
  uint varchar_fields;                  /* number of varchar fields */
  /**
    For materialized derived tables; @see add_derived_key().
    'first' means: having the lowest position in key_info.
  */
  uint first_unused_tmp_key;
  /**
     For materialized derived tables: maximum size of key_info array. Used for
     debugging purpose only.
  */
  uint max_tmp_keys;

/**
    Bitmap with flags representing some of table options/attributes.

    @sa HA_OPTION_PACK_RECORD, HA_OPTION_PACK_KEYS, ...

    @note This is basically copy of HA_CREATE_INFO::table_options bitmap
          at the time of table opening/usage.
  */
  uint db_create_options;
  /**
    Bitmap with flags representing some of table options/attributes which
    are in use by storage engine.

    @note db_options_in_use is normally copy of db_create_options but can
          be overriden by SE. E.g. MyISAM does this at handler::open() and
          hander::info() time.
  */
  uint db_options_in_use;
  uint db_record_offset;		/* if HA_REC_IN_SEQ */
  uint rowid_field_offset;		/* Field_nr +1 to rowid field */
  /* Primary key index number, used in TABLE::key_info[] */
  uint primary_key;                     
  uint next_number_index;               /* autoincrement key number */
  uint next_number_key_offset;          /* autoinc keypart offset in a key */
  uint next_number_keypart;             /* autoinc keypart number in a key */
  bool error;                           /* error during open_table_def() */
  uint column_bitmap_size;
  uint vfields;                         /* Number of generated fields */
  bool system;                          /* Set if system table (one record) */
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

  /**
    Partition meta data. Allocated from TABLE_SHARE::mem_root,
    created when reading from the dd tables,
    used as template for each TABLE instance.
    The reason for having it on the TABLE_SHARE is to be able to reuse the
    partition_elements containing partition names, values etc. instead of 
    allocating them for each TABLE instance.
    TODO: Currently it is filled in and then only used for generating
    the partition_info_str. The plan is to clone/copy/reference each
    TABLE::part_info instance from it.
    What is missing before it can be completed:
    1) The partition expression, currently created only during parsing which
       also needs the current TABLE instance as context for name resolution etc.
    2) The partition values, currently the DD stores them as text so it needs
       to be converted to field images (which is now done by first parsing the
       value text into an Item, then saving the Item result/value into a field
       and then finally copy the field image).
  */
  partition_info *m_part_info;
  // TODO: Remove these four variables:
  /**
    Filled in when reading from frm.
    This can simply be removed when removing the .frm support,
    since it is already stored in the new DD.
  */
  bool auto_partitioned;
  /**
    Storing the full partitioning clause (PARTITION BY ...) which is used
    when creating new partition_info object for each new TABLE object by
    parsing this string.
    These two will be needed until the missing parts above is fixed.
  */
  char *partition_info_str;
  uint  partition_info_str_len;

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
    View object holding view definition read from DD. This object is not
    cached, and is owned by the table share. We are not able to read it
    on demand since we may then get a cache miss while holding LOCK_OPEN.
  */
  const dd::View *view_object;

  /**
    Data-dictionary object describing explicit temporary table represented
    by this share. NULL for other table types (non-temporary tables, internal
    temporary tables). This object is owned by TABLE_SHARE and should be
    deleted along with it.
  */
  dd::Table *tmp_table_def;

  /// For materialized derived tables; @see add_derived_key().
  SELECT_LEX *owner_of_possible_tmp_keys;

  /**
    Set share's table cache key and update its db and table name appropriately.

    @param key_buff    Buffer with already built table cache key to be
                    referenced from share.
    @param key_length  Key length.

    @note
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


  /**
    Set share's table cache key and update its db and table name appropriately.

    @param key_buff    Buffer to be used as storage for table cache key
                    (should be at least key_length bytes).
    @param key         Value for table cache key.
    @param key_length  Key length.

    NOTE
      Since 'key_buff' buffer will be used as storage for table cache key
      it should has same life-time as share itself.
  */

  void set_table_cache_key(char *key_buff, const char *key, size_t key_length)
  {
    memcpy(key_buff, key, key_length);
    set_table_cache_key(key_buff, key_length);
  }

  ulonglong get_table_def_version() const
  {
    return table_map_id;
  }


  /** Is this table share being expelled from the table definition cache?  */
  bool has_old_version() const
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

   @retval  0        For schema tables, DD tables and system views.
            non-0    For bases tables, views and temporary tables.

   @sa TABLE_LIST::is_table_ref_id_equal()
  */
  ulonglong get_table_ref_version() const;

  /** Determine if the table is missing a PRIMARY KEY. */
  bool is_missing_primary_key() const
  {
    DBUG_ASSERT(primary_key <= MAX_KEY);
    return primary_key == MAX_KEY;
  }

  uint find_first_unused_tmp_key(const Key_map &k);

  bool visit_subgraph(Wait_for_flush *waiting_ticket,
                      MDL_wait_for_graph_visitor *gvisitor);

  bool wait_for_old_version(THD *thd, struct timespec *abstime,
                            uint deadlock_weight);

  /**
    The set of indexes that the optimizer may use when creating an execution
    plan.
   */
  Key_map usable_indexes() const
  {
    Key_map usable_indexes(keys_in_use);
    usable_indexes.intersect(visible_indexes);
    return usable_indexes;
  }

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
  Blob_mem_storage();
  ~Blob_mem_storage();

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
  bool is_truncated_value() const { return truncated_value; }
};


/**
  Flags for TABLE::m_status (maximum 8 bits).
  The flags define the state of the row buffer in TABLE::record[0].
*/
/**
  STATUS_NOT_STARTED is set when table is not accessed yet.
  Neither STATUS_NOT_FOUND nor STATUS_NULL_ROW can be set when this flag is set.
*/
#define STATUS_NOT_STARTED      1
/**
   Means we were searching for a row and didn't find it. This is used by
   storage engines (@see handler::index_read_map()) and the executor, both
   when doing an exact row lookup and advancing a scan (no more rows in range).
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

  /// Buffer for use in multi-row reads. Initially empty.
  Record_buffer m_record_buffer{0, 0, nullptr};

  /* 
    Map of keys that can be used to retrieve all data from this table 
    needed by the query without reading the row.
  */
  Key_map covering_keys;
  Key_map quick_keys, merge_keys;
  
  /*
    possible_quick_keys is a superset of quick_keys to use with EXPLAIN of
    JOIN-less commands (single-table UPDATE and DELETE).
    
    When explaining regular JOINs, we use JOIN_TAB::keys to output the 
    "possible_keys" column value. However, it is not available for
    single-table UPDATE and DELETE commands, since they don't use JOIN
    optimizer at the top level. OTOH they directly use the range optimizer,
    that collects all keys usable for range access here.
  */
  Key_map possible_quick_keys;

  /*
    A set of keys that can be used in the query that references this
    table.

    All indexes disabled on the table's TABLE_SHARE (see TABLE::s) will be 
    subtracted from this set upon instantiation. Thus for any TABLE t it holds
    that t.keys_in_use_for_query is a subset of t.s.keys_in_use. Generally we 
    must not introduce any new keys here (see setup_tables).

    The set is implemented as a bitmap.
  */
  Key_map keys_in_use_for_query;
  /* Map of keys that can be used to calculate GROUP BY without sorting */
  Key_map keys_in_use_for_group_by;
  /* Map of keys that can be used to calculate ORDER BY without sorting */
  Key_map keys_in_use_for_order_by;
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
  ORDER	        *group;
  const char    *alias;           ///< alias or table name
  uchar         *null_flags;      ///< Pointer to the null flags of record[0]
  uchar         *null_flags_saved;///< Saved null_flags while null_row is true
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
  uint		db_stat;		/* mode of file as in handler.h */
  int		current_lock;           /* Type of lock on table */

private:
  /**
    If true, this table is inner w.r.t. some outer join operation, all columns
    are nullable (in the query), and null_row may be true.
  */
  bool nullable;

  uint8   m_status;                     /* What's in record[0] */
public:
  /*
    If true, the current table row is considered to have all columns set to 
    NULL, including columns declared as "not null" (see nullable).
    @todo make it private, currently join buffering changes it through a pointer
  */
  bool null_row;

  bool copy_blobs;                   /* copy_blobs when storing */

  /*
    TODO: Each of the following flags take up 8 bits. They can just as easily
    be put into one single unsigned long and instead of taking up 18
    bytes, it would take up 4.
  */
  bool force_index;

  /**
    Flag set when the statement contains FORCE INDEX FOR ORDER BY
    See TABLE_LIST::process_index_hints().
  */
  bool force_index_order;

  /**
    Flag set when the statement contains FORCE INDEX FOR GROUP BY
    See TABLE_LIST::process_index_hints().
  */
  bool force_index_group;
  bool distinct;
  bool const_table;
  /// True if writes to this table should not write rows and just write keys.
  bool no_rows;

  /**
     If set, the optimizer has found that row retrieval should access index 
     tree only.
   */
  bool key_read;
  /**
     Certain statements which need the full row, set this to ban index-only
     access.
  */
  bool no_keyread;
  /**
    If set, indicate that the table is not replicated by the server.
  */
  bool no_replicate;
  bool fulltext_searched;
  bool no_cache;
  /* To signal that the table is associated with a HANDLER statement */
  bool open_by_handler;
  /*
    To indicate that a non-null value of the auto_increment field
    was provided by the user or retrieved from the current record.
    Used only in the MODE_NO_AUTO_VALUE_ON_ZERO mode.
  */
  bool auto_increment_field_not_null;
  bool alias_name_used;		/* true if table_name is alias */
  bool get_fields_in_item_tree;      /* Signal to fix_field */
  /**
    This table must be reopened and is not to be reused.
    NOTE: The TABLE will not be reopened during LOCK TABLES in
    close_thread_tables!!!
  */
  bool m_needs_reopen;
private:
  /**
    For tmp tables. TRUE <=> tmp table has been instantiated.
    Also indicates that table was successfully opened since
    we immediately delete tmp tables which we fail to open.
  */
  bool created;
public:
  /// For a materializable derived or SJ table: true if has been materialized
  bool materialized;
  struct /* field connections */
  {
    class JOIN_TAB *join_tab;
    class QEP_TAB *qep_tab;
    enum thr_lock_type lock_type;		/* How table is used */
    thr_locked_row_action locked_row_action;
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
  void mark_columns_needed_for_update(THD *thd, bool mark_binlog_columns);
  void mark_columns_needed_for_delete(THD *thd);
  void mark_columns_needed_for_insert(THD *thd);
  void mark_columns_per_binlog_row_image(THD *thd);
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
  bool alloc_tmp_keys(uint key_count, bool modify_share);
  bool add_tmp_key(Field_map *key_parts, char *key_name,
                   bool invisible,bool modify_share);
  void copy_tmp_key(int old_idx, bool modify_share);
  void drop_unused_tmp_keys(bool modify_share);

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
    created= materialized= false;
  }
  /// Set table as nullable, ie it is inner wrt some outer join
  void set_nullable() { nullable= TRUE; }

  /// Return whether table is nullable
  bool is_nullable() const { return nullable; }

  /// @return true if table contains one or more generated columns
  bool has_gcol() const { return vfield; }

  /**
   Life cycle of the row buffer is as follows:
   - The initial state is "not started".
   - When reading a row through the storage engine handler, the status is set
     as "has row" or "no row", depending on whether a row was found or not.
     The "not started" state is cleared, as well as the "null row" state,
     the updated state and the deleted state.
   - When making a row available in record[0], make sure to update row status
     similarly to how the storage engine handler does it.
   - If a NULL-extended row is needed in join execution, the "null row" state
     is set. Note that this can be combined with "has row" if a row was read
     but condition on it was evaluated to false (happens for single-row
     lookup), or "no row" if no more rows could be read.
     Note also that for the "null row" state, the NULL bits inside the
     row are set to one, so the row inside the row buffer is no longer usable,
     unless the NULL bits are saved in a separate buffer.
   - The "is updated" and "is deleted" states are set when row is updated or
     deleted, respectively.
  */
  /// Set status for row buffer as "not started"
  void set_not_started()
  {
    m_status= STATUS_NOT_STARTED | STATUS_NOT_FOUND;
    null_row= FALSE;
  }

  /// @return true if a row operation has been done
  bool is_started() const { return !(m_status & STATUS_NOT_STARTED); }

  /// Set status for row buffer: contains row
  void set_found_row()
  {
    m_status= 0;
    null_row= FALSE;
  }

  /**
    Set status for row buffer: contains no row. This is set when
     - A lookup operation finds no row
     - A scan operation scans past the last row of the range.
     - An error in generating key values before calling storage engine.
  */
  void set_no_row()
  {
    m_status= STATUS_NOT_FOUND;
    null_row= FALSE;
  }

  /**
    Set "row found" status from handler result

    @param status 0 if row was found, <> 0 if row was not found
  */
  void set_row_status_from_handler(int status)
  {
    m_status= status ? STATUS_NOT_FOUND : 0;
    null_row= FALSE;
  }

  /**
    Set current row as "null row", for use in null-complemented outer join.
    The row buffer may or may not contain a valid row.
    set_null_row() and reset_null_row() are used by the join executor to
    signal the presence or absence of a NULL-extended row for an outer joined
    table. Null rows may also be used to specify rows that are all NULL in
    grouing operations.
    @note this is a destructive operation since the NULL value bit vector
          is overwritten. Caching operations must be aware of this.
  */
  void set_null_row()
  {
    null_row= TRUE;
    m_status|= STATUS_NULL_ROW;
    if (s->null_bytes > 0)
      memset(null_flags, 255, s->null_bytes);
  }

  /// Clear "null row" status for the current row
  void reset_null_row()
  {
    null_row= FALSE;
    m_status&= ~STATUS_NULL_ROW;
  }

  /// Set "updated" property for the current row
  void set_updated_row()
  {
    DBUG_ASSERT(is_started() && has_row());
    m_status|= STATUS_UPDATED;
  }

  /// Set "deleted" property for the current row
  void set_deleted_row()
  {
    DBUG_ASSERT(is_started() && has_row());
    m_status|= STATUS_DELETED;
  }

  /// @return true if there is a row in row buffer
  bool has_row() const { return !(m_status & STATUS_NOT_FOUND); }

  /// @return true if current row is null-extended
  bool has_null_row() const { return null_row; }

  /// @return true if current row has been updated (multi-table update)
  bool has_updated_row() const { return m_status & STATUS_UPDATED; }

  /// @return true if current row has been deleted (multi-table delete)
  bool has_deleted_row() const { return m_status & STATUS_DELETED; }

  /// Save the NULL flags of the current row into the designated buffer
  void save_null_flags()
  {
    if (s->null_bytes > 0)
      memcpy(null_flags_saved, null_flags, s->null_bytes);
  }

  /// Restore the NULL flags of the current row from the designated buffer
  void restore_null_flags()
  {
    if (s->null_bytes > 0)
      memcpy(null_flags, null_flags_saved, s->null_bytes);
  }

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
     by setting the appropriate flags. You can use either one of the flags
     @c MY_I_S_MAYBE_NULL and @c MY_I_S_UNSIGNED or
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

  void cleanup()
  {
    update_password_expired_fields= false;
    update_password_expired_column= false;
    use_default_password_lifetime= true;
    expire_after_days= 0;
    update_account_locked_column= false;
    account_locked= false;
  }
} LEX_ALTER;

typedef struct	st_lex_user {
  LEX_CSTRING user;
  LEX_CSTRING host;
  LEX_CSTRING plugin;
  LEX_CSTRING auth;
  /* Below attributes defines the context in which this token parsed */
  bool uses_identified_by_clause;
  bool uses_identified_with_clause;
  bool uses_authentication_string_clause;
  bool uses_identified_by_password_clause;
  LEX_ALTER alter_status;

  static st_lex_user *alloc(THD *thd, LEX_STRING *user, LEX_STRING *host);
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
                             enum thr_lock_type lock_type_arg)
  {
    memset(this, 0, sizeof(*this));
    m_map= 1;
    db= (char*) db_name_arg;
    db_length= db_length_arg;
    table_name= (char*) table_name_arg;
    table_name_length= table_name_length_arg;
    alias= (char*) alias_arg;
    m_lock_descriptor.type= lock_type_arg;
    MDL_REQUEST_INIT(&mdl_request,
                     MDL_key::TABLE, db, table_name,
                     mdl_type_for_dml(m_lock_descriptor.type),
                     MDL_TRANSACTION);
  }


  /**
    Auxiliary method which prepares TABLE_LIST consisting of one table instance
    to be used in simple open_and_lock_tables and takes type of MDL lock on the
    table as explicit parameter.
  */

  inline void init_one_table(const char *db_name_arg,
                             size_t db_length_arg,
                             const char *table_name_arg,
                             size_t table_name_length_arg,
                             const char *alias_arg,
                             enum thr_lock_type lock_type_arg,
                             enum enum_mdl_type mdl_request_type)
  {
    init_one_table(db_name_arg, db_length_arg, table_name_arg,
                   table_name_length_arg, alias_arg, lock_type_arg);
    mdl_request.set_type(mdl_request_type);
  }

  /// Create a TABLE_LIST object representing a nested join
  static TABLE_LIST *new_nested_join(MEM_ROOT *allocator,
                                     const char *alias,
                                     TABLE_LIST *embedding,
                                     List<TABLE_LIST> *belongs_to,
                                     SELECT_LEX *select);

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
  bool merge_underlying_tables(SELECT_LEX *select);

  /// Reset table
  void reset();

  /// Evaluate the check option of a view
  int view_check_option(THD *thd) const;

  /// Cleanup field translations for a view
  void cleanup_items();

  /**
    Check whether the table is a placeholder, ie a derived table, a view or
    a schema table.
    A table is also considered to be a placeholder if it does not have a
    TABLE object for some other reason.
    A recursive reference in a CTE is also a placeholder: it doesn't need any
    locking, binary logging...
  */
  bool is_placeholder() const
  {
    return is_view_or_derived() || schema_table || !table ||
      m_is_recursive_reference;
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

  /**
     @returns true if this is a recursive reference inside the definition of a
     recursive CTE.
     @note that it starts its existence as a dummy derived table, until the
     end of resolution when it's not a derived table anymore, just a reference
     to the materialized temporary table. Whereas a non-recursive
     reference to the recursive CTE is a derived table.
  */
  bool is_recursive_reference() const { return m_is_recursive_reference; }

  /**
    @see is_recursive_reference().
    @returns true if error
  */
  bool set_recursive_reference();

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
    Set table as readonly, ie it is neither updatable, insertable nor
    deletable during this statement.
  */
  void set_readonly() { m_updatable= false; m_insertable= false; }

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
  void set_derived_unit(SELECT_LEX_UNIT *query_expr)
  {
    derived= query_expr;
  }

  /// Return the query expression of a derived table or view.
  SELECT_LEX_UNIT *derived_unit() const
  {
    DBUG_ASSERT(derived);
    return derived;
  }

  /// Save names of materialized table @see reset_name_temporary
  void save_name_temporary()
  {
    view_db.str= db;
    view_db.length= db_length;
    view_name.str= table_name;
    view_name.length= table_name_length;
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
    /*
      When printing a query using a view or CTE, we need the table's name and
      the alias; the name has been destroyed if the table was materialized,
      so we restore it:
    */
    DBUG_ASSERT(table_name != view_name.str);
    table_name= view_name.str;
    table_name_length= view_name.length;
    if (is_view()) // restore database's name too
    {
      DBUG_ASSERT(db != view_db.str);
      db= view_db.str;
      db_length= view_db.length;
    }
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

  Security_context *find_view_security_context(THD *thd);
  bool prepare_view_security_context(THD *thd);

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

  /**
     If a derived table, returns query block id of first underlying query block.
     Zero if not derived.
  */
  uint query_block_id() const;


  /**
     This is for showing in EXPLAIN.
     If a derived table, returns query block id of first underlying query block
     of first materialized TABLE_LIST instance. Zero if not derived.
  */
  uint query_block_id_for_explain() const;

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
  bool setup_materialized_derived_tmp_table(THD *thd);

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
    Mark that there is a NATURAL JOIN or JOIN ... USING between two tables.

      This function marks that table b should be joined with a either via
      a NATURAL JOIN or via JOIN ... USING. Both join types are special
      cases of each other, so we treat them together. The function
      setup_conds() creates a list of equal condition between all fields
      of the same name for NATURAL JOIN or the fields in
      TABLE_LIST::join_using_fields for JOIN ... USING.
      The list of equality conditions is stored
      either in b->join_cond(), or in JOIN::conds, depending on whether there
      was an outer join.

    EXAMPLE
    @verbatim
      SELECT * FROM t1 NATURAL LEFT JOIN t2
       <=>
      SELECT * FROM t1 LEFT JOIN t2 ON (t1.i=t2.i and t1.j=t2.j ... )

      SELECT * FROM t1 NATURAL JOIN t2 WHERE <some_cond>
       <=>
      SELECT * FROM t1, t2 WHERE (t1.i=t2.i and t1.j=t2.j and <some_cond>)

      SELECT * FROM t1 JOIN t2 USING(j) WHERE <some_cond>
       <=>
      SELECT * FROM t1, t2 WHERE (t1.j=t2.j and <some_cond>)
     @endverbatim

    @param b            Right join argument.
  */
  void add_join_natural(TABLE_LIST *b) { b->natural_join= this; }

  /**
    Set granted privileges for a table.

    Can be used when generating temporary tables that are also used in
    resolver process, such as when generating a UNION table

    @param privilege   Privileges granted for this table.
  */
  void set_privileges(ulong privilege)
  {
    grant.privilege|= privilege;
    if (table)
      table->grant.privilege|= privilege;
  }
  /*
    List of tables local to a subquery or the top-level SELECT (used by
    SQL_I_List). Considers views as leaves (unlike 'next_leaf' below).
    Created at parse time in SELECT_LEX::add_table_to_list() ->
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

  void set_lock(const Lock_descriptor &descriptor)
  {
    m_lock_descriptor= descriptor;
  }

  const Lock_descriptor &lock_descriptor() const
  {
    return m_lock_descriptor;
  }

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
  */
  SELECT_LEX_UNIT *derived;		/* SELECT_LEX_UNIT of derived table */

  /// If non-NULL, the CTE which this table is derived from.
  Common_table_expr *m_common_table_expr;
  /**
    If the user has specified column names with the syntaxes "table name
    parenthesis column names":
    WITH qn(column names) AS (select...)
    or
    FROM (select...) dt(column names)
    or
    CREATE VIEW v(column_names) AS ...
    then this points to the list of column names. NULL otherwise.
  */
  const Create_col_name_list *m_derived_column_names;

public:
  ST_SCHEMA_TABLE *schema_table;        /* Information_schema table */
  SELECT_LEX *schema_select_lex;
  /*
    True when the view field translation table is used to convert
    schema table fields for backwards compatibility with SHOW command.
  */
  bool schema_table_reformed;
  Temp_table_param *schema_table_param;
  /* link to select_lex where this table was used */
  SELECT_LEX *select_lex;

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
  TABLE_LIST    *next_leaf;
  Item          *derived_where_cond;    ///< WHERE condition from derived table
  Item          *check_option;          ///< WITH CHECK OPTION condition
  Item          *replace_filter;        ///< Filter for REPLACE command
  LEX_STRING    select_stmt;            ///< text of (CREATE/SELECT) statement
  LEX_STRING    source;                 ///< source of CREATE VIEW
  LEX_CSTRING   view_db;                ///< saved view database
  LEX_CSTRING   view_name;              ///< saved view name
  LEX_STRING    timestamp;              ///< GMT time stamp of last operation
  st_lex_user   definer;                ///< definer of view
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
      @todo Replace with an enum 
  */
  ulonglong     algorithm;
  ulonglong     view_suid;              ///< view is suid (TRUE by default)
  ulonglong     with_check;             ///< WITH CHECK OPTION

private:
  /// The view algorithm that is actually used, if this is a view.
  enum_view_algorithm effective_algorithm;
  Lock_descriptor m_lock_descriptor;
public:
  GRANT_INFO	grant;
  /* data need by some engines in query cache*/
  ulonglong     engine_data;
  /* call back function for asking handler about caching in query cache */
  qc_engine_callback callback_func;
public:
  uint		outer_join;		/* Which join type */
  uint		shared;			/* Used in multi-upd */
  size_t        db_length;
  size_t        table_name_length;
private:
  bool          m_updatable;		/* VIEW/TABLE can be updated */
  bool          m_insertable;           /* VIEW/TABLE can be inserted into */
public:
  bool		straight;		/* optimize with prev table */
  /**
    True for tables and views being changed in a data change statement.
    Also used by replication to filter out statements that can be ignored,
    especially important for multi-table UPDATE and DELETE.
  */
  bool          updating;
  bool		force_index;		/* prefer index over table scan */
  bool          ignore_leaves;          /* preload only non-leaf nodes */
  table_map     dep_tables;             /* tables the table depends on      */
  table_map     on_expr_dep_tables;     /* tables on expression depends on  */
  struct st_nested_join *nested_join;   /* if the element is a nested join  */
  TABLE_LIST *embedding;             /* nested join containing the table */
  List<TABLE_LIST> *join_list;/* join list the table belongs to   */
  bool		cacheable_table;	/* stop PS caching */
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

  dd::enum_table_type required_type;
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
      qualified name (@<db_name@>.@<table_name@>).
  */
  bool          is_fqtn;


  /* View creation context. */

  View_creation_ctx *view_creation_ctx;

  /*
    Attributes to save/load view creation context in/from frm-file.

    They are required only to be able to use existing parser to load
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

  // True, If this is a system view
  bool is_system_view;

  /*
    Set to 'true' if this is a DD table being opened in the context of a
    dictionary operation. Note that when 'false', this may still be a DD
    table when opened in a non-DD context, e.g. as part of an I_S view
    query.
  */
  bool is_dd_ctx_table;

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

  /// If non-NULL, the CTE which this table is derived from.
  Common_table_expr *common_table_expr() const { return m_common_table_expr; }
  void set_common_table_expr(Common_table_expr *c)
  { m_common_table_expr= c; }
  /// @see m_derived_column_names
  const Create_col_name_list *derived_column_names() const
  { return m_derived_column_names; }
  void set_derived_column_names(const Create_col_name_list *d)
  { m_derived_column_names= d; }


private:
  /*
    A group of members set and used only during JOIN::optimize().
  */
  /**
     Optimized copy of m_join_cond (valid for one single
     execution). Initialized by SELECT_LEX::get_optimizable_conditions().
     @todo it would be goo dto reset it in reinit_before_use(), if
     reinit_stmt_before_use() had a loop including join nests.
  */
  Item          *m_join_cond_optim;
public:

  COND_EQUAL    *cond_equal;            ///< Used with outer join
  /// true <=> this table is a const one and was optimized away.
  bool          optimized_away;
  /**
    true <=> all possible keys for a derived table were collected and
    could be re-used while statement re-execution.
  */
  bool          derived_keys_ready;
private:
  /// If a recursive reference inside the definition of a CTE.
  bool          m_is_recursive_reference;
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


/**
  Generic iterator over the fields of an arbitrary table reference.

    This class unifies the various ways of iterating over the columns
    of a table reference depending on the type of SQL entity it
    represents. If such an entity represents a nested table reference,
    this iterator encapsulates the iteration over the columns of the
    members of the table reference.

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
  An iterator over an intrusive list in TABLE_LIST objects. Can be used for
  iterating an intrusive list in e.g. range-based for loops.

  @tparam Next_pointer The intrusive list's "next" member.
*/
template <TABLE_LIST *TABLE_LIST::*Next_pointer>
class Table_list_iterator
{
public:

  /**
    Constructs an iterator.

    @param start The TABLE_LIST where that the iterator will start iterating
    from.
  */
  Table_list_iterator(TABLE_LIST *start) : m_current(start) {}

  TABLE_LIST *operator++() { return m_current= m_current->*Next_pointer; }

  TABLE_LIST *operator*() { return m_current; }

  bool operator!=(const Table_list_iterator &other) const
  {
    return m_current != other.m_current;
  }

private:
  TABLE_LIST *m_current;
};

typedef Table_list_iterator<&TABLE_LIST::next_local> Local_tables_iterator;
typedef Table_list_iterator<&TABLE_LIST::next_global> Global_tables_iterator;

/**
  Provides a list interface on TABLE_LIST objects. The interface is similar to
  std::vector, but has only the bare minimum to allow for iteration.

  @tparam Iterator_type Must have an implicit constructor from a TABLE_LIST
  pointer, and support pre-increment, non-equality and dereference operators.
*/
template<typename Iterator_type>
class Table_list_adapter
{
public:

  /**
    Constructs the list adapter.

    @param first The TABLE_LIST that is considered first in the list.
  */
  Table_list_adapter(TABLE_LIST *first) : m_first(first) {}

  /// An iterator pointing to the first TABLE_LIST.
  Iterator_type begin() { return m_first; }

  /// A past-the-end iterator.
  Iterator_type end() { return nullptr; }

private:
  TABLE_LIST *m_first;
};


/// A list interface over the TABLE_LIST::next_local pointer.
typedef Table_list_adapter<Local_tables_iterator> Local_tables_list;

/// A list interface over the TABLE_LIST::next_global pointer.
typedef Table_list_adapter<Global_tables_iterator> Global_tables_list;

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
  /// Expected number of rows in the materialized table
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

static inline
my_bitmap_map *dbug_tmp_use_all_columns(TABLE *table MY_ATTRIBUTE((unused)),
                                        MY_BITMAP *bitmap MY_ATTRIBUTE((unused)))
{
#ifndef DBUG_OFF
  return tmp_use_all_columns(table, bitmap);
#else
  return 0;
#endif
}

static inline
void dbug_tmp_restore_column_map(MY_BITMAP *bitmap MY_ATTRIBUTE((unused)),
                                 my_bitmap_map *old MY_ATTRIBUTE((unused)))
{
#ifndef DBUG_OFF
  tmp_restore_column_map(bitmap, old);
#endif
}


/* 
  Variant of the above : handle both read and write sets.
  Provide for the possiblity of the read set being the same as the write set
*/
static inline
void dbug_tmp_use_all_columns(TABLE *table MY_ATTRIBUTE((unused)),
                              my_bitmap_map **save MY_ATTRIBUTE((unused)),
                              MY_BITMAP *read_set MY_ATTRIBUTE((unused)),
                              MY_BITMAP *write_set MY_ATTRIBUTE((unused)))
{
#ifndef DBUG_OFF
  save[0]= read_set->bitmap;
  save[1]= write_set->bitmap;
  (void) tmp_use_all_columns(table, read_set);
  (void) tmp_use_all_columns(table, write_set);
#endif
}


static inline
void dbug_tmp_restore_column_maps(MY_BITMAP *read_set MY_ATTRIBUTE((unused)),
                                  MY_BITMAP *write_set MY_ATTRIBUTE((unused)),
                                  my_bitmap_map **old MY_ATTRIBUTE((unused)))
{
#ifndef DBUG_OFF
  tmp_restore_column_map(read_set, old[0]);
  tmp_restore_column_map(write_set, old[1]);
#endif
}


size_t max_row_length(TABLE *table, const uchar *data);


void init_mdl_requests(TABLE_LIST *table_list);

/**
   Unpack the definition of a virtual column. Parses the text obtained from
   TABLE_SHARE and produces an Item.

  @param thd                  Thread handler
  @param table                Table with the checked field
  @param field                Pointer to Field object
  @param is_create_table      Indicates that table is opened as part
                              of CREATE or ALTER and does not yet exist in SE
  @param error_reported       updated flag for the caller that no other error
                              messages are to be generated.

  @retval TRUE Failure.
  @retval FALSE Success.
*/

bool unpack_gcol_info(THD *thd,
                      TABLE *table,
                      Field *field,
                      bool is_create_table,
                      bool *error_reported);


/**
   Unpack the partition expression. Parse the partition expression
   to produce an Item.

  @param[in] thd                Thread handler
  @param[in] outparam           Table object
  @param[in] share              TABLE_SHARE object
  @param[in] engine_type        Engine type of the partitions.
  @param[in] is_create_table    Indicates that table is opened as part of
                                CREATE or ALTER and does not yet exist in SE

  @retval TRUE Failure.
  @retval FALSE Success.
*/

bool unpack_partition_info(THD *thd,
                           TABLE *outparam,
                           TABLE_SHARE *share,
                           handlerton *engine_type,
                           bool is_create_table);

int open_table_from_share(THD *thd, TABLE_SHARE *share, const char *alias,
                          uint db_stat, uint prgflag, uint ha_open_flags,
                          TABLE *outparam, bool is_create_table,
                          const dd::Table *table_def_param);
TABLE_SHARE *alloc_table_share(TABLE_LIST *table_list, const char *key,
                               size_t key_length);
void init_tmp_table_share(THD *thd, TABLE_SHARE *share, const char *key,
                          size_t key_length,
                          const char *table_name, const char *path,
                          MEM_ROOT *mem_root);
void free_table_share(TABLE_SHARE *share);
void update_create_info_from_table(HA_CREATE_INFO *info, TABLE *form);
Ident_name_check check_and_convert_db_name(LEX_STRING *db,
                                           bool preserve_lettercase);
bool check_column_name(const char *name);
Ident_name_check check_table_name(const char *name, size_t length);
int rename_file_ext(const char * from,const char * to,const char * ext);
char *get_field(MEM_ROOT *mem, Field *field);
bool get_field(MEM_ROOT *mem, Field *field, class String *res);

int closefrm(TABLE *table, bool free_share);
void free_blobs(TABLE *table);
void free_blob_buffers_and_reset(TABLE *table, uint32 size);
int set_zone(int nr,int min_zone,int max_zone);
void append_unescaped(String *res, const char *pos, size_t length);
char *fn_rext(char *name);
TABLE_CATEGORY get_table_category(const LEX_STRING &db,
                                  const LEX_STRING &name);

/* performance schema */
extern LEX_STRING PERFORMANCE_SCHEMA_DB_NAME;

extern LEX_STRING GENERAL_LOG_NAME;
extern LEX_STRING SLOW_LOG_NAME;

/* information schema */
extern LEX_STRING INFORMATION_SCHEMA_NAME;

/* mysql schema */
extern LEX_STRING MYSQL_SCHEMA_NAME;

/* mysql tablespace */
extern LEX_STRING MYSQL_TABLESPACE_NAME;

/* replication's tables */
extern LEX_STRING RLI_INFO_NAME;
extern LEX_STRING MI_INFO_NAME;
extern LEX_STRING WORKER_INFO_NAME;
extern "C" MYSQL_PLUGIN_IMPORT CHARSET_INFO *system_charset_info;

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

/**
  return true if the table was created explicitly.
*/
inline bool is_user_table(TABLE * table)
{
  const char *name= table->s->table_name.str;
  return strncmp(name, tmp_file_prefix, tmp_file_prefix_length);
}

bool is_simple_order(ORDER *order);

uint add_pk_parts_to_sk(KEY *sk, uint sk_n, KEY *pk, uint pk_n,
                        TABLE_SHARE *share, handler *handler_file,
                        uint *usable_parts);
void setup_key_part_field(TABLE_SHARE *share, handler *handler_file,
                          uint primary_key_n, KEY *keyinfo, uint key_n,
                          uint key_part_n, uint *usable_parts,
                          bool part_of_key_not_extended);

const uchar *get_field_name(const uchar *arg, size_t *length);

void repoint_field_to_record(TABLE *table, uchar *old_rec, uchar *new_rec);
bool update_generated_write_fields(const MY_BITMAP *bitmap, TABLE *table);
bool update_generated_read_fields(uchar *buf, TABLE *table,
                                  uint active_index= MAX_KEY);

/**
  Check if a TABLE_LIST instance represents a pre-opened temporary table.
*/

inline bool is_temporary_table(TABLE_LIST *tl)
{
  if (tl->is_view() || tl->schema_table)
    return FALSE;

  if (!tl->table)
    return FALSE;

  /*
    NOTE: 'table->s' might be NULL for specially constructed TABLE
    instances. See SHOW TRIGGERS for example.
  */

  if (!tl->table->s)
    return FALSE;

  return tl->table->s->tmp_table != NO_TMP_TABLE;
}


/**
  After parsing, a Common Table Expression is accessed through a
  TABLE_LIST. This class contains all information about the CTE which the
  TABLE_LIST needs.

  @note that before and during parsing, the CTE is described by a
  PT_common_table_expr.
*/
class Common_table_expr
{
public:
  Common_table_expr(MEM_ROOT *mem_root) : references(mem_root),
    recursive(false), tmp_tables(mem_root)
    {}
  TABLE *clone_tmp_table(THD *thd, TABLE_LIST *tl);
  bool substitute_recursive_reference(THD *thd, SELECT_LEX *sl);
  /**
     All references to this CTE in the statement, except those inside the
     query expression defining this CTE.
     In other words, all non-recursive references.
  */
  Mem_root_array<TABLE_LIST *> references;
  /// True if it's a recursive CTE
  bool recursive;
  /**
    List of all TABLE_LISTSs reading/writing to the tmp table created to
    materialize this CTE. Due to shared materialization, only the first one
    has a TABLE generated by create_tmp_table(); other ones have a TABLE
    generated by open_table_from_share().
  */
  Mem_root_array<TABLE_LIST *> tmp_tables;
};


/**
   This iterates on those references to a derived table / view / CTE which are
   materialized. If a recursive CTE, this includes recursive references.
   Upon construction it is passed a non-recursive materialized reference
   to the derived table (TABLE_LIST*).
   For a CTE it may return more than one reference; for a derived table or a
   view, there is only one (as references to a same view are treated as
   independent objects).
   References are returned as TABLE*.
*/
class Derived_refs_iterator
{
  TABLE_LIST *const start; ///< The reference provided in construction.
  int ref_idx; ///< Current index in cte->tmp_tables
public:
  explicit Derived_refs_iterator(TABLE_LIST *start_arg) :
  start(start_arg), ref_idx(-1)
  {}
  TABLE *get_next()
  {
    ref_idx++;
    const Common_table_expr *cte= start->common_table_expr();
    if (!cte)
      return (ref_idx < 1) ? start->table : nullptr;
    return ((uint)ref_idx < cte->tmp_tables.size()) ?
      cte->tmp_tables[ref_idx]->table : nullptr;
  }
  void rewind() { ref_idx= -1; }
  /// @returns true if the last get_next() returned the first element.
  bool is_first() const { return ref_idx == 0; }
};

//////////////////////////////////////////////////////////////////////////

/*
  NOTE:
  These structures are added to read .frm file in upgrade scenario.

  They should not be used any where else in the code.
  They will be removed in future release.
  Any new code should not be added in this section.
*/


/**
  These members were removed from TABLE_SHARE as they are not used in
  in the code. open_binary_frm() uses these members while reading
  .frm files.
*/
class FRM_context
{
public:
  FRM_context()
    : default_part_db_type(NULL), null_field_first(false), stored_fields(0),
      view_def(NULL), frm_version(0), fieldnames()
  {}

  handlerton *default_part_db_type;
  bool null_field_first;
  uint stored_fields;                   /* Number of stored fields
                                           (i.e. without generated-only ones) */

  enum utype  { NONE,DATE,SHIELD,NOEMPTY,CASEUP,PNR,BGNR,PGNR,YES,NO,REL,
                CHECK,EMPTY,UNKNOWN_FIELD,CASEDN,NEXT_NUMBER,INTERVAL_FIELD,
                BIT_FIELD, TIMESTAMP_OLD_FIELD, CAPITALIZE, BLOB_FIELD,
                TIMESTAMP_DN_FIELD, TIMESTAMP_UN_FIELD, TIMESTAMP_DNUN_FIELD,
                GENERATED_FIELD= 128 };

  /**
    For shares representing views File_parser object with view
    definition read from .FRM file.
  */
  const File_parser *view_def;
  uchar frm_version;
  TYPELIB fieldnames;                   /* Pointer to fieldnames */
};


/**
  Create TABLE_SHARE from .frm file.

  FRM_context object is used to store the value removed from
  TABLE_SHARE. These values are used only for .frm file parsing.

  @param[in]  thd                       Thread handle.
  @param[in]  path                      Path of the frm file.
  @param[out] share                     TABLE_SHARE to be populated.
  @param[out] frm_context               FRM_context object.
  @param[in]  db                        Database name.
  @param[in]  table                     Table name.
  @param[in]  is_fix_view_cols_and_deps Fix view column data, table
                                        and routine dependency.

  @retval TABLE_SHARE  ON SUCCESS
  @retval NULL         ON FAILURE
*/
bool create_table_share_for_upgrade(THD *thd,
                                    const char *path,
                                    TABLE_SHARE *share,
                                    FRM_context *frm_context,
                                    const char *db,
                                    const char *table,
                                    bool is_fix_view_cols_and_deps);
//////////////////////////////////////////////////////////////////////////

#endif /* TABLE_INCLUDED */
