/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @defgroup GROUP_PARSER Parser
  @{
*/

#ifndef SQL_LEX_INCLUDED
#define SQL_LEX_INCLUDED

#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "binary_log_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "map_helpers.h"
#include "memory_debugging.h"
#include "my_base.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "my_thread_local.h"
#include "my_time.h"
#include "mysql/components/services/psi_statement_bits.h"
#include "mysql/psi/psi_base.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "prealloced_array.h"                // Prealloced_array
#include "sql/dd/info_schema/table_stats.h"  // dd::info_schema::Table_stati...
#include "sql/dd/info_schema/tablespace_stats.h"  // dd::info_schema::Tablesp...
#include "sql/enum_query_type.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"            // Name_resolution_context
#include "sql/item_create.h"     // Cast_target
#include "sql/item_subselect.h"  // chooser_compare_func_creator
#include "sql/key_spec.h"        // KEY_CREATE_INFO
#include "sql/lex_symbol.h"      // LEX_SYMBOL
#include "sql/mdl.h"
#include "sql/mem_root_array.h"  // Mem_root_array
#include "sql/opt_hints.h"
#include "sql/parse_tree_hints.h"
#include "sql/parse_tree_node_base.h"  // enum_parsing_context
#include "sql/query_options.h"         // OPTION_NO_CONST_TABLES
#include "sql/resourcegroups/platform/thread_attrs_api.h"   // cpu_id_t
#include "sql/resourcegroups/resource_group_basic_types.h"  // Type, Range
#include "sql/set_var.h"
#include "sql/sql_admin.h"
#include "sql/sql_alter.h"  // Alter_info
#include "sql/sql_array.h"
#include "sql/sql_connect.h"  // USER_RESOURCES
#include "sql/sql_const.h"
#include "sql/sql_data_change.h"  // enum_duplicates
#include "sql/sql_exchange.h"
#include "sql/sql_get_diagnostics.h"  // Diagnostics_information
#include "sql/sql_list.h"
#include "sql/sql_plugin_ref.h"
#include "sql/sql_servers.h"  // Server_options
#include "sql/sql_signal.h"   // enum_condition_item_name
#include "sql/sql_udf.h"      // Item_udftype
#include "sql/table.h"        // TABLE_LIST
#include "sql/thr_malloc.h"
#include "sql/trigger_def.h"  // enum_trigger_action_time_type
#include "sql/window_lex.h"
#include "sql/xa.h"  // xa_option_words
#include "sql_chars.h"
#include "sql_string.h"
#include "thr_lock.h"  // thr_lock_type
#include "violite.h"   // SSL_type

class Item_func_set_user_var;
class Item_sum;
class Parse_tree_root;
class PT_alter_table_standalone_action;
class PT_assign_to_keycache;
class PT_base_index_option;
class PT_column_attr_base;
class PT_create_table_option;
class PT_ddl_table_option;
class PT_item_list;
class PT_json_table_column;
class PT_part_definition;
class PT_part_value_item;
class PT_part_value_item_list_paren;
class PT_part_values;
class PT_partition;
class PT_partition_option;
class PT_preload_keys;
class PT_query_expression;
class PT_role_or_privilege;
class PT_subpartition;
class PT_subquery;
class PT_table_element;
class PT_table_reference;
class Protocol;
class SELECT_LEX_UNIT;
class Select_lex_visitor;
class THD;
class Window;
struct MEM_ROOT;
struct Sql_cmd_srs_attributes;

enum class enum_jt_column;
enum class enum_jtc_on : uint16;
typedef Parse_tree_node_tmpl<struct Alter_tablespace_parse_context>
    PT_alter_tablespace_option_base;

/* YACC and LEX Definitions */

class Event_parse_data;
class Item_func;
class Item_func_match;
class Query_result_interceptor;
class SELECT_LEX;
class Sql_cmd;
class partition_info;
class sp_head;
class sp_name;
class sp_pcontext;
struct sql_digest_state;

const size_t INITIAL_LEX_PLUGIN_LIST_SIZE = 16;

enum class partition_type;      // from partition_element.h
enum class enum_key_algorithm;  // from partition_info.h

/*
  There are 8 different type of table access so there is no more than
  combinations 2^8 = 256:

  . STMT_READS_TRANS_TABLE

  . STMT_READS_NON_TRANS_TABLE

  . STMT_READS_TEMP_TRANS_TABLE

  . STMT_READS_TEMP_NON_TRANS_TABLE

  . STMT_WRITES_TRANS_TABLE

  . STMT_WRITES_NON_TRANS_TABLE

  . STMT_WRITES_TEMP_TRANS_TABLE

  . STMT_WRITES_TEMP_NON_TRANS_TABLE

  The unsafe conditions for each combination is represented within a byte
  and stores the status of the option --binlog-direct-non-trans-updates,
  whether the trx-cache is empty or not, and whether the isolation level
  is lower than ISO_REPEATABLE_READ:

  . option (OFF/ON)
  . trx-cache (empty/not empty)
  . isolation (>= ISO_REPEATABLE_READ / < ISO_REPEATABLE_READ)

  bits 0 : . OFF, . empty, . >= ISO_REPEATABLE_READ
  bits 1 : . OFF, . empty, . < ISO_REPEATABLE_READ
  bits 2 : . OFF, . not empty, . >= ISO_REPEATABLE_READ
  bits 3 : . OFF, . not empty, . < ISO_REPEATABLE_READ
  bits 4 : . ON, . empty, . >= ISO_REPEATABLE_READ
  bits 5 : . ON, . empty, . < ISO_REPEATABLE_READ
  bits 6 : . ON, . not empty, . >= ISO_REPEATABLE_READ
  bits 7 : . ON, . not empty, . < ISO_REPEATABLE_READ
*/
extern uint binlog_unsafe_map[256];
/*
  Initializes the array with unsafe combinations and its respective
  conditions.
*/
void binlog_unsafe_map_init();

enum enum_yes_no_unknown { TVL_YES, TVL_NO, TVL_UNKNOWN };

enum class enum_ha_read_modes;

/**
  used by the parser to store internal variable name
*/
struct sys_var_with_base {
  sys_var *var;
  LEX_STRING base_name;
};

#define YYSTYPE_IS_DECLARED 1
union YYSTYPE;

typedef YYSTYPE *LEX_YYSTYPE;

/*
  If we encounter a diagnostics statement (GET DIAGNOSTICS, or e.g.
  the old SHOW WARNINGS|ERRORS, or "diagnostics variables" such as
  @@warning_count | @@error_count, we'll set some hints so this
  information is not lost. DA_KEEP_UNSPECIFIED is used in LEX constructor to
  avoid leaving variables uninitialized.
 */
enum enum_keep_diagnostics {
  DA_KEEP_NOTHING = 0, /**< keep nothing */
  DA_KEEP_DIAGNOSTICS, /**< keep the diagnostics area */
  DA_KEEP_COUNTS,      /**< keep \@warning_count / \@error_count */
  DA_KEEP_PARSE_ERROR, /**< keep diagnostics area after parse error */
  DA_KEEP_UNSPECIFIED  /**< keep semantics is unspecified */
};

enum enum_sp_suid_behaviour {
  SP_IS_DEFAULT_SUID = 0,
  SP_IS_NOT_SUID,
  SP_IS_SUID
};

enum enum_sp_data_access {
  SP_DEFAULT_ACCESS = 0,
  SP_CONTAINS_SQL,
  SP_NO_SQL,
  SP_READS_SQL_DATA,
  SP_MODIFIES_SQL_DATA
};

/**
  enum_sp_type defines type codes of stored programs.

  @note these codes are used when dealing with the mysql.routines system table,
  so they must not be changed.

  @note the following macros were used previously for the same purpose. Now they
  are used for ACL only.
*/
enum class enum_sp_type { FUNCTION = 1, PROCEDURE, TRIGGER, EVENT };

inline enum_sp_type to_sp_type(longlong val) {
  DBUG_ASSERT(val >= 1 && val <= 4);
  return static_cast<enum_sp_type>(val);
}

inline longlong to_longlong(enum_sp_type val) {
  return static_cast<longlong>(val);
}

inline uint to_uint(enum_sp_type val) { return static_cast<uint>(val); }

/*
  Values for the type enum. This reflects the order of the enum declaration
  in the CREATE TABLE command. These values are used to enumerate object types
  for the ACL statements.

  These values were also used for enumerating stored program types. However, now
  enum_sp_type should be used for that instead of them.
*/
#define TYPE_ENUM_FUNCTION 1
#define TYPE_ENUM_PROCEDURE 2
#define TYPE_ENUM_TRIGGER 3
#define TYPE_ENUM_PROXY 4

enum class Acl_type {
  TABLE = 0,
  FUNCTION = TYPE_ENUM_FUNCTION,
  PROCEDURE = TYPE_ENUM_PROCEDURE,
};

const LEX_STRING sp_data_access_name[] = {
    {C_STRING_WITH_LEN("")},
    {C_STRING_WITH_LEN("CONTAINS SQL")},
    {C_STRING_WITH_LEN("NO SQL")},
    {C_STRING_WITH_LEN("READS SQL DATA")},
    {C_STRING_WITH_LEN("MODIFIES SQL DATA")}};

enum class enum_view_create_mode {
  VIEW_CREATE_NEW,        // check that there are not such VIEW/table
  VIEW_ALTER,             // check that VIEW with such name exists
  VIEW_CREATE_OR_REPLACE  // check only that there are not such table
};

enum enum_drop_mode {
  DROP_DEFAULT,  // mode is not specified
  DROP_CASCADE,  // CASCADE option
  DROP_RESTRICT  // RESTRICT option
};

/* Options to add_table_to_list() */
#define TL_OPTION_UPDATING 1
#define TL_OPTION_FORCE_INDEX 2
#define TL_OPTION_IGNORE_LEAVES 4
#define TL_OPTION_ALIAS 8

/* Structure for db & table in sql_yacc */
extern LEX_CSTRING EMPTY_CSTR;
extern LEX_CSTRING NULL_CSTR;

class Table_function;

class Table_ident {
 public:
  LEX_CSTRING db;
  LEX_CSTRING table;
  SELECT_LEX_UNIT *sel;
  Table_function *table_function;

  Table_ident(Protocol *protocol, const LEX_CSTRING &db_arg,
              const LEX_CSTRING &table_arg, bool force);
  Table_ident(const LEX_CSTRING &db_arg, const LEX_CSTRING &table_arg)
      : db(db_arg), table(table_arg), sel(NULL), table_function(NULL) {}
  Table_ident(const LEX_CSTRING &table_arg)
      : table(table_arg), sel(NULL), table_function(NULL) {
    db = NULL_CSTR;
  }
  /**
    This constructor is used only for the case when we create a derived
    table. A derived table has no name and doesn't belong to any database.
    Later, if there was an alias specified for the table, it will be set
    by add_table_to_list.
  */
  Table_ident(SELECT_LEX_UNIT *s) : sel(s), table_function(NULL) {
    db = EMPTY_CSTR; /* a subject to casedn_str */
    table = EMPTY_CSTR;
  }
  /*
    This constructor is used only for the case when we create a table function.
    It has no name and doesn't belong to any database as it exists only
    during query execution. Later, if there was an alias specified for the
    table, it will be set by add_table_to_list.
  */
  Table_ident(LEX_CSTRING &table_arg, Table_function *table_func_arg)
      : table(table_arg), sel(NULL), table_function(table_func_arg) {
    /* We must have a table name here as this is used with add_table_to_list */
    db = EMPTY_CSTR; /* a subject to casedn_str */
  }
  // True if we can tell from syntax that this is a table function.
  bool is_table_function() const { return (table_function != nullptr); }
  // True if we can tell from syntax that this is an unnamed derived table.
  bool is_derived_table() const { return sel; }
  void change_db(const char *db_name) {
    db.str = db_name;
    db.length = strlen(db_name);
  }
};

typedef List<Item> List_item;
typedef Mem_root_array<ORDER *> Group_list_ptrs;

/**
  Structure to hold parameters for CHANGE MASTER, START SLAVE, and STOP SLAVE.

  Remark: this should not be confused with Master_info (and perhaps
  would better be renamed to st_lex_replication_info).  Some fields,
  e.g., delay, are saved in Relay_log_info, not in Master_info.
*/
struct LEX_MASTER_INFO {
  /*
    The array of IGNORE_SERVER_IDS has a preallocation, and is not expected
    to grow to any significant size, so no instrumentation.
  */
  LEX_MASTER_INFO() : repl_ignore_server_ids(PSI_NOT_INSTRUMENTED) {
    initialize();
  }
  char *host, *user, *password, *log_file_name, *bind_addr;
  uint port, connect_retry;
  float heartbeat_period;
  int sql_delay;
  ulonglong pos;
  ulong server_id, retry_count;
  char *gtid;
  char *view_id;
  const char *channel;  // identifier similar to database name
  enum {
    UNTIL_SQL_BEFORE_GTIDS = 0,
    UNTIL_SQL_AFTER_GTIDS
  } gtid_until_condition;
  bool until_after_gaps;
  bool slave_until;
  bool for_channel;

  /*
    Enum is used for making it possible to detect if the user
    changed variable or if it should be left at old value
   */
  enum {
    LEX_MI_UNCHANGED = 0,
    LEX_MI_DISABLE,
    LEX_MI_ENABLE
  } ssl,
      ssl_verify_server_cert, heartbeat_opt, repl_ignore_server_ids_opt,
      retry_count_opt, auto_position, port_opt, get_public_key;
  char *ssl_key, *ssl_cert, *ssl_ca, *ssl_capath, *ssl_cipher;
  char *ssl_crl, *ssl_crlpath, *tls_version;
  char *public_key_path;
  char *relay_log_name;
  ulong relay_log_pos;
  Prealloced_array<ulong, 2> repl_ignore_server_ids;

  /// Initializes everything to zero/NULL/empty.
  void initialize();
  /// Sets all fields to their "unspecified" value.
  void set_unspecified();

 private:
  // Not copyable or assignable.
  LEX_MASTER_INFO(const LEX_MASTER_INFO &);
  LEX_MASTER_INFO &operator=(const LEX_MASTER_INFO &);
};

struct LEX_RESET_SLAVE {
  bool all;
};

enum sub_select_type {
  UNSPECIFIED_TYPE,
  UNION_TYPE,
  INTERSECT_TYPE,
  EXCEPT_TYPE,
  GLOBAL_OPTIONS_TYPE,
  DERIVED_TABLE_TYPE,
  OLAP_TYPE
};

enum olap_type { UNSPECIFIED_OLAP_TYPE, ROLLUP_TYPE };

/*
  String names used to print a statement with index hints.
  Keep in sync with index_hint_type.
*/
extern const char *index_hint_type_name[];
typedef uchar index_clause_map;

/*
  Bits in index_clause_map : one for each possible FOR clause in
  USE/FORCE/IGNORE INDEX index hint specification
*/
#define INDEX_HINT_MASK_JOIN (1)
#define INDEX_HINT_MASK_GROUP (1 << 1)
#define INDEX_HINT_MASK_ORDER (1 << 2)

#define INDEX_HINT_MASK_ALL \
  (INDEX_HINT_MASK_JOIN | INDEX_HINT_MASK_GROUP | INDEX_HINT_MASK_ORDER)

/* Single element of an USE/FORCE/IGNORE INDEX list specified as a SQL hint  */
class Index_hint {
 public:
  /* The type of the hint : USE/FORCE/IGNORE */
  enum index_hint_type type;
  /* Where the hit applies to. A bitmask of INDEX_HINT_MASK_<place> values */
  index_clause_map clause;
  /*
    The index name. Empty (str=NULL) name represents an empty list
    USE INDEX () clause
  */
  LEX_CSTRING key_name;

  Index_hint(const char *str, uint length) {
    key_name.str = str;
    key_name.length = length;
  }

  void print(THD *thd, String *str);
};

/*
  Class SELECT_LEX_UNIT represents a query expression.
  Class SELECT_LEX represents a query block.
  A query expression contains one or more query blocks (more than one means
  that we have a UNION query).
  These classes are connected as follows:
   Both classes have a master, a slave, a next and a prev field.
   For class SELECT_LEX, master and slave connect to objects of type
   SELECT_LEX_UNIT, whereas for class SELECT_LEX_UNIT, they connect
   to SELECT_LEX.
   master is pointer to outer node.
   slave is pointer to the first inner node

   neighbors are two SELECT_LEX or SELECT_LEX_UNIT objects on
   the same level.

   The structures are linked with the following pointers:
   - list of neighbors (next/prev) (prev of first element point to slave
     pointer of outer structure)
     - For SELECT_LEX, this is a list of query blocks.
     - For SELECT_LEX_UNIT, this is a list of subqueries.

   - pointer to outer node (master), which is
     If this is SELECT_LEX_UNIT
       - pointer to outer select_lex.
     If this is SELECT_LEX
       - pointer to outer SELECT_LEX_UNIT.

   - pointer to inner objects (slave), which is either:
     If this is an SELECT_LEX_UNIT:
       - first query block that belong to this query expression.
     If this is an SELECT_LEX
       - first query expression that belong to this query block (subqueries).

   - list of all SELECT_LEX objects (link_next/link_prev)
     This is to be used for things like derived tables creation, where we
     go through this list and create the derived tables.

   If query expression contain several query blocks (UNION now,
   INTERSECT etc later) then it has a special select_lex called
   fake_select_lex. It used for storing global parameters (like ORDER BY,
   LIMIT) and executing union.
   Subqueries used in global ORDER BY clause will be attached to this
   fake_select_lex, which will allow them to correctly resolve fields of
   the containing UNION and outer selects.

   For example for following query:

   select *
     from table1
     where table1.field IN (select * from table1_1_1 union
                            select * from table1_1_2)
     union
   select *
     from table2
     where table2.field=(select (select f1 from table2_1_1_1_1
                                   where table2_1_1_1_1.f2=table2_1_1.f3)
                           from table2_1_1
                           where table2_1_1.f1=table2.f2)
     union
   select * from table3;

   we will have following structure:

   select1: (select * from table1 ...)
   select2: (select * from table2 ...)
   select3: (select * from table3)
   select1.1.1: (select * from table1_1_1)
   ...

     main unit
     fake0
     select1 select2 select3
     |^^     |^
    s|||     ||master
    l|||     |+---------------------------------+
    a|||     +---------------------------------+|
    v|||master                         slave   ||
    e||+-------------------------+             ||
     V|            neighbor      |             V|
     unit1.1<+==================>unit1.2       unit2.1
     fake1.1
     select1.1.1 select 1.1.2    select1.2.1   select2.1.1
                                               |^
                                               ||
                                               V|
                                               unit2.1.1.1
                                               select2.1.1.1.1


   relation in main unit will be following:
   (bigger picture for:
      main unit
      fake0
      select1 select2 select3
   in the above picture)

         main unit
         |^^^^|fake_select_lex
         |||||+--------------------------------------------+
         ||||+--------------------------------------------+|
         |||+------------------------------+              ||
         ||+--------------+                |              ||
    slave||master         |                |              ||
         V|      neighbor |       neighbor |        master|V
         select1<========>select2<========>select3        fake0

    list of all select_lex will be following (as it will be constructed by
    parser):

    select1->select2->select3->select2.1.1->select 2.1.2->select2.1.1.1.1-+
                                                                          |
    +---------------------------------------------------------------------+
    |
    +->select1.1.1->select1.1.2

*/

class JOIN;
class PT_with_clause;
class Query_result;
class Query_result_union;
struct LEX;

/**
  This class represents a query expression (one query block or
  several query blocks combined with UNION).
*/
class SELECT_LEX_UNIT {
  /**
    Intrusive double-linked list of all query expressions
    immediately contained within the same query block.
  */
  SELECT_LEX_UNIT *next;
  SELECT_LEX_UNIT **prev;

  /**
    The query block wherein this query expression is contained,
    NULL if the query block is the outer-most one.
  */
  SELECT_LEX *master;
  /// The first query block in this query expression.
  SELECT_LEX *slave;

 private:
  /**
    Marker for subqueries in WHERE, HAVING, ORDER BY, GROUP BY and
    SELECT item lists.
    Must be read/written when holding LOCK_query_plan.

   See Item_subselect::explain_subquery_checker
  */
  enum_parsing_context explain_marker;

  bool prepared;   ///< All query blocks in query expression are prepared
  bool optimized;  ///< All query blocks in query expression are optimized
  bool executed;   ///< Query expression has been executed

  TABLE_LIST result_table_list;
  Query_result_union *union_result;
  TABLE *table; /* temporary table using for appending UNION results */
  /// Object to which the result for this query expression is sent
  Query_result *m_query_result;

 public:
  /**
    result of this query can't be cached, bit field, can be :
      UNCACHEABLE_DEPENDENT
      UNCACHEABLE_RAND
      UNCACHEABLE_SIDEEFFECT
  */
  uint8 uncacheable;

  explicit SELECT_LEX_UNIT(enum_parsing_context parsing_context);

  /// @return true for a query expression without UNION or multi-level ORDER
  bool is_simple() const { return !(is_union() || fake_select_lex); }

  /// Values for SELECT_LEX_UNIT::cleaned
  enum enum_clean_state {
    UC_DIRTY,       ///< Unit isn't cleaned
    UC_PART_CLEAN,  ///< Unit were cleaned, except JOIN and JOIN_TABs were
                    ///< kept for possible EXPLAIN
    UC_CLEAN        ///< Unit completely cleaned, all underlying JOINs were
                    ///< freed
  };
  enum_clean_state cleaned;  ///< cleanliness state

  // list of fields which points to temporary table for union
  List<Item> item_list;
  /*
    list of types of items inside union (used for union & derived tables)

    Item_type_holders from which this list consist may have pointers to Field,
    pointers is valid only after preparing SELECTS of this unit and before
    any SELECT of this unit execution

    TODO:
    Possibly this member should be protected, and its direct use replaced
    by get_unit_column_types(). Check the places where it is used.
  */
  List<Item> types;

  /**
    Pointer to query block containing global parameters for query.
    Global parameters may include ORDER BY, LIMIT and OFFSET.

    If this is a union of multiple query blocks, the global parameters are
    stored in fake_select_lex. If the union doesn't use a temporary table,
    SELECT_LEX_UNIT::prepare() nulls out fake_select_lex, but saves a copy
    in saved_fake_select_lex in order to preserve the global parameters.

    If this is not a union, and the query expression has no multi-level
    ORDER BY/LIMIT, global parameters are in the single query block.

    @return query block containing the global parameters
  */
  inline SELECT_LEX *global_parameters() const {
    if (fake_select_lex != NULL)
      return fake_select_lex;
    else if (saved_fake_select_lex != NULL)
      return saved_fake_select_lex;
    return first_select();
  };
  /* LIMIT clause runtime counters */
  ha_rows select_limit_cnt, offset_limit_cnt;
  /// Points to subquery if this query expression is used in one, otherwise NULL
  Item_subselect *item;
  THD *thd;  ///< Thread handler
  /**
    Helper query block for query expression with UNION or multi-level
    ORDER BY/LIMIT
  */
  SELECT_LEX *fake_select_lex;
  /**
    SELECT_LEX that stores LIMIT and OFFSET for UNION ALL when no
    fake_select_lex is used.
  */
  SELECT_LEX *saved_fake_select_lex;
  /**
     Points to last query block which has UNION DISTINCT on its left.
     In a list of UNIONed blocks, UNION is left-associative; so UNION DISTINCT
     eliminates duplicates in all blocks up to the first one on its right
     included. Which is why we only need to remember that query block.
  */
  SELECT_LEX *union_distinct;

  /**
    The WITH clause which is the first part of this query expression. NULL if
    none.
  */
  PT_with_clause *m_with_clause;
  /**
    If this query expression is underlying of a derived table, the derived
    table. NULL if none.
  */
  TABLE_LIST *derived_table;
  /**
     First query block (in this UNION) which references the CTE.
     NULL if not the query expression of a recursive CTE.
  */
  SELECT_LEX *first_recursive;

  /**
    True if the with-recursive algorithm has produced the complete result.
    In a recursive CTE, a JOIN is executed several times in a loop, and
    should not be cleaned up (e.g. by join_free()) before all iterations of
    the loop are done (i.e. before the CTE's result is complete).
  */
  bool got_all_recursive_rows;

  /// @return true if query expression can be merged into an outer query
  bool is_mergeable() const;

  /// @return true if query expression is recommended to be merged
  bool merge_heuristic() const;

  /// @return the query block this query expression belongs to as subquery
  SELECT_LEX *outer_select() const { return master; }

  /// @return the first query block inside this query expression
  SELECT_LEX *first_select() const { return slave; }

  /// @return the next query expression within same query block (next subquery)
  SELECT_LEX_UNIT *next_unit() const { return next; }

  /// @return the query result object in use for this query expression
  Query_result *query_result() const { return m_query_result; }

  /**
    If this unit is recursive, then this returns the Query_result which holds
    the rows of the recursive reference read by 'reader':
    - fake_select_lex reads rows from the union's result
    - other recursive query blocks read rows from the derived table's result.
    @param  reader  Recursive query block belonging to this unit
  */
  const Query_result *recursive_result(SELECT_LEX *reader) const;

  /// Set new query result object for this query expression
  void set_query_result(Query_result *res) { m_query_result = res; }

  bool prepare(THD *thd, Query_result *result, ulonglong added_options,
               ulonglong removed_options);
  bool optimize(THD *thd);
  bool execute(THD *thd);
  bool explain(THD *ethd);
  bool cleanup(bool full);
  inline void unclean() { cleaned = UC_DIRTY; }
  void reinit_exec_mechanism();

  void print(String *str, enum_query_type query_type);
  bool accept(Select_lex_visitor *visitor);

  bool add_fake_select_lex(THD *thd);
  bool prepare_fake_select_lex(THD *thd);
  void set_prepared() { prepared = true; }
  void set_optimized() { optimized = true; }
  void set_executed() { executed = true; }
  void reset_executed() { executed = false; }
  bool is_prepared() const { return prepared; }
  bool is_optimized() const { return optimized; }
  bool is_executed() const { return executed; }
  bool change_query_result(Query_result_interceptor *result,
                           Query_result_interceptor *old_result);
  bool prepare_limit(THD *thd, SELECT_LEX *provider);
  bool set_limit(THD *thd, SELECT_LEX *provider);
  void set_thd(THD *thd_arg) { thd = thd_arg; }

  inline bool is_union() const;
  bool union_needs_tmp_table();
  /// @returns true if mixes UNION DISTINCT and UNION ALL
  bool mixed_union_operators() const;

  /// Include a query expression below a query block.
  void include_down(LEX *lex, SELECT_LEX *outer);

  /// Exclude this unit and immediately contained select_lex objects
  void exclude_level();

  /// Exclude subtree of current unit from tree of SELECTs
  void exclude_tree();

  /// Renumber query blocks of a query expression according to supplied LEX
  void renumber_selects(LEX *lex);

  friend class SELECT_LEX;

  List<Item> *get_unit_column_types();
  List<Item> *get_field_list();

  enum_parsing_context get_explain_marker() const;
  void set_explain_marker(enum_parsing_context m);
  void set_explain_marker_from(const SELECT_LEX_UNIT *u);

#ifndef DBUG_OFF
  /**
     Asserts that none of {this unit and its children units} is fully cleaned
     up.
  */
  void assert_not_fully_clean();
#else
  void assert_not_fully_clean() {}
#endif
  void invalidate();

  bool is_recursive() const { return first_recursive != nullptr; }

  bool check_materialized_derived_query_blocks(THD *thd);

  /*
    An exception: this is the only function that needs to adjust
    explain_marker.
  */
  friend bool parse_view_definition(THD *thd, TABLE_LIST *view_ref);
};

typedef Bounds_checked_array<Item *> Ref_item_array;

/**
  SELECT_LEX type enum
*/
enum class enum_explain_type {
  EXPLAIN_NONE = 0,
  EXPLAIN_PRIMARY,
  EXPLAIN_SIMPLE,
  EXPLAIN_DERIVED,
  EXPLAIN_SUBQUERY,
  EXPLAIN_UNION,
  EXPLAIN_UNION_RESULT,
  EXPLAIN_MATERIALIZED,
  // Total:
  EXPLAIN_total  ///< fake type, total number of all valid types
  // Don't insert new types below this line!
};

/**
  This class represents a query block, aka a query specification, which is
  a query consisting of a SELECT keyword, followed by a table list,
  optionally followed by a WHERE clause, a GROUP BY, etc.
*/
class SELECT_LEX {
 public:
  Item *where_cond() const { return m_where_cond; }
  void set_where_cond(Item *cond) { m_where_cond = cond; }
  Item *having_cond() const { return m_having_cond; }
  void set_having_cond(Item *cond) { m_having_cond = cond; }
  void set_query_result(Query_result *result) { m_query_result = result; }
  Query_result *query_result() const { return m_query_result; }
  bool change_query_result(Query_result_interceptor *new_result,
                           Query_result_interceptor *old_result);

  /// Set base options for a query block (and active options too)
  void set_base_options(ulonglong options_arg) {
    DBUG_EXECUTE_IF("no_const_tables", options_arg |= OPTION_NO_CONST_TABLES;);

    // Make sure we do not overwrite options by accident
    DBUG_ASSERT(m_base_options == 0 && m_active_options == 0);
    m_base_options = options_arg;
    m_active_options = options_arg;
  }

  /// Add base options to a query block, also update active options
  void add_base_options(ulonglong options) {
    DBUG_ASSERT(first_execution);
    m_base_options |= options;
    m_active_options |= options;
  }

  /**
    Remove base options from a query block.
    Active options are also updated, and we assume here that "extra" options
    cannot override removed base options.
  */
  void remove_base_options(ulonglong options) {
    DBUG_ASSERT(first_execution);
    m_base_options &= ~options;
    m_active_options &= ~options;
  }

  /// Make active options from base options, supplied options and environment:
  void make_active_options(ulonglong added_options, ulonglong removed_options);

  /// Adjust the active option set
  void add_active_options(ulonglong options) { m_active_options |= options; }

  /// @return the active query options
  ulonglong active_options() const { return m_active_options; }

  /**
    Set associated tables as read_only, ie. they cannot be inserted into,
    updated or deleted from during this statement.
    Commonly used for query blocks that are part of derived tables or
    views that are materialized.
  */
  void set_tables_readonly() {
    // Set all referenced base tables as read only.
    for (TABLE_LIST *tr = leaf_tables; tr != nullptr; tr = tr->next_leaf)
      tr->set_readonly();
  }

  /// @returns a map of all tables references in the query block
  table_map all_tables_map() const { return (1ULL << leaf_table_count) - 1; }

 private:
  /**
    Intrusive double-linked list of all query blocks within the same
    query expression.
  */
  SELECT_LEX *next;
  SELECT_LEX **prev;

  /// The query expression containing this query block.
  SELECT_LEX_UNIT *master;
  /// The first query expression contained within this query block.
  SELECT_LEX_UNIT *slave;

  /// Intrusive double-linked global list of query blocks.
  SELECT_LEX *link_next;
  SELECT_LEX **link_prev;

  /// Result of this query block
  Query_result *m_query_result;

  /**
    Options assigned from parsing and throughout resolving,
    should not be modified after resolving is done.
  */
  ulonglong m_base_options;
  /**
    Active options. Derived from base options, modifiers added during
    resolving and values from session variable option_bits. Since the latter
    may change, active options are refreshed per execution of a statement.
  */
  ulonglong m_active_options;

 public:
  /**
    result of this query can't be cached, bit field, can be :
      UNCACHEABLE_DEPENDENT
      UNCACHEABLE_RAND
      UNCACHEABLE_SIDEEFFECT
  */
  uint8 uncacheable;

  /// True: skip local transformations during prepare() call (used by INSERT)
  bool skip_local_transforms;

  /// Describes context of this query block (e.g if it is a derived table).
  enum sub_select_type linkage;
  bool no_table_names_allowed;  ///< used for global order by
  /**
    Context for name resolution for all column references except columns
    from joined tables.
  */
  Name_resolution_context context;
  /**
    Pointer to first object in list of Name res context objects that have
    this query block as the base query block.
    Includes field "context" which is embedded in this query block.
  */
  Name_resolution_context *first_context;
  /**
    Three fields used by semi-join transformations to know when semi-join is
    possible, and in which condition tree the subquery predicate is located.
  */
  enum Resolve_place {
    RESOLVE_NONE,
    RESOLVE_JOIN_NEST,
    RESOLVE_CONDITION,
    RESOLVE_HAVING,
    RESOLVE_SELECT_LIST
  };
  Resolve_place resolve_place;  ///< Indicates part of query being resolved
  TABLE_LIST *resolve_nest;     ///< Used when resolving outer join condition
  /**
    Disables semi-join flattening when resolving a subtree in which flattening
    is not allowed. The flag should be true while resolving items that are not
    on the AND-top-level of a condition tree.
  */
  bool semijoin_disallowed;
  char *db;

 private:
  /**
    Condition to be evaluated after all tables in a query block are joined.
    After all permanent transformations have been conducted by
    SELECT_LEX::prepare(), this condition is "frozen", any subsequent changes
    to it must be done with change_item_tree(), unless they only modify AND/OR
    items and use a copy created by SELECT_LEX::get_optimizable_conditions().
    Same is true for 'having_cond'.
  */
  Item *m_where_cond;

  /// Condition to be evaluated on grouped rows after grouping.
  Item *m_having_cond;

 public:
  /**
    Saved values of the WHERE and HAVING clauses. Allowed values are:
     - COND_UNDEF if the condition was not specified in the query or if it
       has not been optimized yet
     - COND_TRUE if the condition is always true
     - COND_FALSE if the condition is impossible
     - COND_OK otherwise
  */
  Item::cond_result cond_value;
  Item::cond_result having_value;

  /// Reference to LEX that this query block belongs to
  LEX *parent_lex;
  /// Indicates whether this query block contains the WITH ROLLUP clause
  enum olap_type olap;
  /// List of tables in FROM clause - use TABLE_LIST::next_local to traverse
  SQL_I_List<TABLE_LIST> table_list;

  /**
    GROUP BY clause.
    This list may be mutated during optimization (by remove_const()),
    so for prepared statements, we keep a copy of the ORDER.next pointers in
    group_list_ptrs, and re-establish the original list before each execution.
  */
  SQL_I_List<ORDER> group_list;
  Group_list_ptrs *group_list_ptrs;

  /**
    All windows defined on the select, both named and inlined
  */
  List<Window> m_windows;

  /**
    List of columns and expressions:
    SELECT: Columns and expressions in the SELECT list.
    UPDATE: Columns in the SET clause.
  */
  List<Item> item_list;
  bool is_item_list_lookup;

  /// Number of GROUP BY expressions added to all_fields
  int hidden_group_field_count;

  List<Item> &fields_list;  ///< hold field list
  List<Item> all_fields;    ///< to store all expressions used in query
  /**
    Usually a pointer to ftfunc_list_alloc, but in UNION this is used to create
    fake select_lex that consolidates result fields of UNION
  */
  List<Item_func_match> *ftfunc_list;
  List<Item_func_match> ftfunc_list_alloc;
  /**
    After optimization it is pointer to corresponding JOIN. This member
    should be changed only when THD::LOCK_query_plan mutex is taken.
  */
  JOIN *join;
  /// join list of the top level
  List<TABLE_LIST> top_join_list;
  /// list for the currently parsed join
  List<TABLE_LIST> *join_list;
  /// table embedding the above list
  TABLE_LIST *embedding;
  /// List of semi-join nests generated for this query block
  List<TABLE_LIST> sj_nests;
  /**
    Points to first leaf table of query block. After setup_tables() is done,
    this is a list of base tables and derived tables. After derived tables
    processing is done, this is a list of base tables only.
    Use TABLE_LIST::next_leaf to traverse the list.
  */
  TABLE_LIST *leaf_tables;
  /// Number of leaf tables in this query block.
  uint leaf_table_count;
  /// Number of derived tables and views in this query block.
  uint derived_table_count;
  /// Number of table functions in this query block
  uint table_func_count;
  /// Number of materialized derived tables and views in this query block.
  uint materialized_derived_table_count;
  /**
    True if query block has semi-join nests merged into it. Notice that this
    is updated earlier than sj_nests, so check this if info is needed
    before the full resolver process is complete.
  */
  bool has_sj_nests;
  /// Number of partitioned tables
  uint partitioned_table_count;

  /**
    ORDER BY clause.
    This list may be mutated during optimization (by remove_const()),
    so for prepared statements, we keep a copy of the ORDER.next pointers in
    order_list_ptrs, and re-establish the original list before each execution.
  */
  SQL_I_List<ORDER> order_list;
  Group_list_ptrs *order_list_ptrs;

  /// LIMIT clause, NULL if no limit is given
  Item *select_limit;
  /// LIMIT ... OFFSET clause, NULL if no offset is given
  Item *offset_limit;

  /**
    Array of pointers to "base" items; one each for every selected expression
    and referenced item in the query block. All references to fields are to
    buffers associated with the primary input tables.
  */
  Ref_item_array base_ref_items;

  /**
    number of items in select_list and HAVING clause used to get number
    bigger then can be number of entries that will be added to all item
    list during split_sum_func
  */
  uint select_n_having_items;
  uint cond_count;     ///< number of arguments of and/or/xor in where/having/on
  uint between_count;  ///< number of between predicates in where/having/on
  uint max_equal_elems;  ///< maximal number of elements in multiple equalities
  /**
    Number of fields used in select list or where clause of current select
    and all inner subselects.
  */
  uint select_n_where_fields;

  /// Parse context: indicates where the current expression is being parsed
  enum_parsing_context parsing_place;
  /// Parse context: is inside a set function if this is positive
  uint in_sum_expr;

  /**
    True if contains or aggregates set functions.
    @note this is wrong when a locally found set function is aggregated
    in an outer query block.
  */
  bool with_sum_func;
  /// Number of Item_sum-derived objects in this SELECT
  uint n_sum_items;
  /// Number of Item_sum-derived objects in children and descendant SELECTs
  uint n_child_sum_items;

  uint select_number;  ///< Query block number (used for EXPLAIN)
  /**
    Nesting level of query block, outer-most query block has level 0,
    its subqueries have level 1, etc. @see also sql/item_sum.h.
  */
  int nest_level;
  /// Circular linked list of sum func in nested selects
  Item_sum *inner_sum_func_list;
  /**
    Number of wildcards used in the SELECT list. For example,
    SELECT *, t1.*, catalog.t2.* FROM t0, t1, t2;
    has 3 wildcards.
  */
  uint with_wild;
  bool braces;  ///< SELECT ... UNION (SELECT ... ) <- this braces
  /// true when having fix field called in processing of this query block
  bool having_fix_field;
  /// true when GROUP BY fix field called in processing of this query block
  bool group_fix_field;
  /// List of references to fields referenced from inner query blocks
  List<Item_outer_ref> inner_refs_list;

  /// explicit LIMIT clause is used
  bool explicit_limit;
  /**
    HAVING clause contains subquery => we can't close tables before
    query processing end even if we use temporary table
  */
  bool subquery_in_having;
  /**
    This variable is required to ensure proper work of subqueries and
    stored procedures. Generally, one should use the states of
    Query_arena to determine if it's a statement prepare or first
    execution of a stored procedure. However, in case when there was an
    error during the first execution of a stored procedure, the SP body
    is not expelled from the SP cache. Therefore, a deeply nested
    subquery might be left unoptimized. So we need this per-subquery
    variable to inidicate the optimization/execution state of every
    subquery. Prepared statements work OK in that regard, as in
    case of an error during prepare the PS is not created.
  */
  bool first_execution;
  /// True when semi-join pull-out processing is complete
  bool sj_pullout_done;
  /// exclude this query block from unique_table() check
  bool exclude_from_table_unique_test;
  /// Allow merge of immediate unnamed derived tables
  bool allow_merge_derived;
  /**
     If this query block is a recursive member of a recursive unit: the
     TABLE_LIST, in this recursive member, referencing the query
     name.
  */
  TABLE_LIST *recursive_reference;
  /**
     To pass the first steps of resolution, a recursive reference is made to
     be a dummy derived table; after the temporary table is created based on
     the non-recursive members' types, the recursive reference is made to be a
     reference to the tmp table. Its dummy-derived-table unit is saved in this
     member, so that when the statement's execution ends, the reference can be
     restored to be a dummy derived table for the next execution, which is
     necessary if we have a prepared statement.
     WL#6570 should allow to remove this.
  */
  SELECT_LEX_UNIT *recursive_dummy_unit;
  /**
    The set of those tables whose fields are referenced in the select list of
    this select level.
  */
  table_map select_list_tables;
  table_map outer_join;  ///< Bitmap of all inner tables from outer joins

  /// Query-block-level hints, for this query block
  Opt_hints_qb *opt_hints_qb;

  // Last table for LATERAL join, used by table functions
  TABLE_LIST *end_lateral_table;
  /**
    @note the group_by and order_by lists below will probably be added to the
          constructor when the parser is converted into a true bottom-up design.

          //SQL_I_LIST<ORDER> *group_by, SQL_I_LIST<ORDER> order_by
  */
  SELECT_LEX(Item *where, Item *having);

  SELECT_LEX_UNIT *master_unit() const { return master; }
  SELECT_LEX_UNIT *first_inner_unit() const { return slave; }
  SELECT_LEX *outer_select() const { return master->outer_select(); }
  SELECT_LEX *next_select() const { return next; }

  /**
    @return true  If STRAIGHT_JOIN applies to all tables.
    @return false Else.
  */
  bool is_straight_join() {
    bool straight_join = true;
    /// false for exmaple in t1 STRAIGHT_JOIN t2 JOIN t3.
    for (TABLE_LIST *tbl = leaf_tables->next_leaf; tbl; tbl = tbl->next_leaf)
      straight_join &= tbl->straight;
    return straight_join || (active_options() & SELECT_STRAIGHT_JOIN);
  }

  SELECT_LEX *last_select() {
    SELECT_LEX *mylast = this;
    for (; mylast->next_select(); mylast = mylast->next_select()) {
    }
    return mylast;
  }

  SELECT_LEX *next_select_in_list() const { return link_next; }

  void mark_as_dependent(SELECT_LEX *last, bool aggregate);

  /// @return true if query block is explicitly grouped (non-empty GROUP BY)
  bool is_explicitly_grouped() const { return group_list.elements > 0; }

  /**
    @return true if this query block is implicitly grouped, ie it is not
    explicitly grouped but contains references to set functions.
    The query will return max. 1 row (@see also is_single_grouped()).
  */
  bool is_implicitly_grouped() const {
    return m_agg_func_used && group_list.elements == 0;
  }

  /**
    True if this query block is implicitly grouped.

    @note Not reliable before name resolution.

    @return true if this query block is implicitly grouped and returns exactly
    one row, which happens when it does not have a HAVING clause.

    @remark This function is currently unused.
  */
  bool is_single_grouped() const {
    return m_agg_func_used && group_list.elements == 0 && m_having_cond == NULL;
  }

  /**
    @return true if this query block is explicitly or implicitly grouped.
    @note a query with DISTINCT is not considered to be aggregated.
    @note in standard SQL, a query with HAVING is defined as grouped, however
          MySQL allows HAVING without any aggregation to be the same as WHERE.
  */
  bool is_grouped() const { return group_list.elements > 0 || m_agg_func_used; }

  /// @return true if this query block contains DISTINCT at start of select list
  bool is_distinct() const { return active_options() & SELECT_DISTINCT; }

  /**
    @return true if this query block contains an ORDER BY clause.

    @note returns false if ORDER BY has been eliminated, e.g if the query
          can return max. 1 row.
  */
  bool is_ordered() const { return order_list.elements > 0; }

  /// @return true if this query block has a LIMIT clause
  bool has_limit() const { return select_limit != NULL; }

  /// @return true if query block references full-text functions
  bool has_ft_funcs() const { return ftfunc_list->elements > 0; }

  /// @returns true if query block is a recursive member of a recursive unit
  bool is_recursive() const { return recursive_reference != nullptr; }

  void invalidate();

  bool set_braces(bool value);
  uint get_in_sum_expr() const { return in_sum_expr; }

  bool add_item_to_list(Item *item);
  bool add_ftfunc_to_list(Item_func_match *func);
  void add_order_to_list(ORDER *order);
  TABLE_LIST *add_table_to_list(
      THD *thd, Table_ident *table, const char *alias, ulong table_options,
      thr_lock_type flags = TL_UNLOCK, enum_mdl_type mdl_type = MDL_SHARED_READ,
      List<Index_hint> *hints = 0, List<String> *partition_names = 0,
      LEX_STRING *option = 0, Parse_context *pc = NULL);
  TABLE_LIST *get_table_list() const { return table_list.first; }
  bool init_nested_join(THD *thd);
  TABLE_LIST *end_nested_join();
  TABLE_LIST *nest_last_join(THD *thd, size_t table_cnt = 2);
  bool add_joined_table(TABLE_LIST *table);
  TABLE_LIST *convert_right_join();
  List<Item> *get_item_list() { return &item_list; }

  // Check privileges for views that are merged into query block
  bool check_view_privileges(THD *thd, ulong want_privilege_first,
                             ulong want_privilege_next);

  // Resolve and prepare information about tables for one query block
  bool setup_tables(THD *thd, TABLE_LIST *tables, bool select_insert);

  // Resolve derived table, view, table function information for a query block
  bool resolve_placeholder_tables(THD *thd, bool apply_semijoin);

  // Propagate exclusion from table uniqueness test into subqueries
  void propagate_unique_test_exclusion();

  // Add full-text function elements from a list into this query block
  bool add_ftfunc_list(List<Item_func_match> *ftfuncs);

  void set_lock_for_table(const Lock_descriptor &descriptor, TABLE_LIST *table);

  void set_lock_for_tables(thr_lock_type lock_type);

  inline void init_order() {
    DBUG_ASSERT(order_list.elements == 0);
    order_list.elements = 0;
    order_list.first = 0;
    order_list.next = &order_list.first;
  }
  /*
    This method created for reiniting LEX in mysql_admin_table() and can be
    used only if you are going remove all SELECT_LEX & units except belonger
    to LEX (LEX::unit & LEX::select, for other purposes use
    SELECT_LEX_UNIT::exclude_level()
  */
  void cut_subtree() { slave = 0; }
  bool test_limit();
  /**
    Get offset for LIMIT.

    Evaluate offset item if necessary.

    @return Number of rows to skip.

    @todo Integrate better with SELECT_LEX_UNIT::set_limit()
  */
  ha_rows get_offset();
  /**
   Get limit.

   Evaluate limit item if necessary.

   @return Limit of rows in result.

   @todo Integrate better with SELECT_LEX_UNIT::set_limit()
  */
  ha_rows get_limit();

  /// Assign a default name resolution object for this query block.
  bool set_context(Name_resolution_context *outer_context);

  /// Setup the array containing references to base items
  bool setup_base_ref_items(THD *thd);
  void print(THD *thd, String *str, enum_query_type query_type);

  /**
    Print detail of the SELECT_LEX object.

    @param      thd          Thread handler
    @param      query_type   Options to print out string output
    @param[out] str          String of output.
  */
  void print_select(THD *thd, String *str, enum_query_type query_type);

  /**
    Print detail of the UPDATE statement.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_update(THD *thd, String *str, enum_query_type query_type);

  /**
    Print detail of the DELETE statement.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_delete(THD *thd, String *str, enum_query_type query_type);

  /**
    Print detail of the INSERT statement.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_insert(THD *thd, String *str, enum_query_type query_type);

  /**
    Print detail of Hints.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_hints(THD *thd, String *str, enum_query_type query_type);

  /**
    Print error.

    @param      thd          Thread handler
    @param[out] str          String of output

    @return
    @retval false   If there is no error
    @retval true    else
  */
  bool print_error(THD *thd, String *str);

  /**
    Print select options.

    @param[out] str          String of output
  */
  void print_select_options(String *str);

  /**
    Print UPDATE options.

    @param[out] str          String of output
  */
  void print_update_options(String *str);

  /**
    Print DELETE options.

    @param[out] str          String of output
  */
  void print_delete_options(String *str);

  /**
    Print INSERT options.

    @param[out] str          String of output
  */
  void print_insert_options(String *str);

  /**
    Print list of tables.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      table_list   TABLE_LIST object
    @param      query_type   Options to print out string output
  */
  void print_table_references(THD *thd, String *str, TABLE_LIST *table_list,
                              enum_query_type query_type);

  /**
    Print list of items in SELECT_LEX object.

    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_item_list(String *str, enum_query_type query_type);

  /**
    Print assignments list. Used in UPDATE and
    INSERT ... ON DUPLICATE KEY UPDATE ...

    @param[out] str          String of output
    @param      query_type   Options to print out string output
    @param      fields       List columns to be assigned.
    @param      values       List of values.
  */
  void print_update_list(String *str, enum_query_type query_type,
                         List<Item> fields, List<Item> values);

  /**
    Print column list to be inserted into. Used in INSERT.

    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_insert_fields(String *str, enum_query_type query_type);

  /**
    Print list of values to be inserted. Used in INSERT.

    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_insert_values(String *str, enum_query_type query_type);

  /**
    Print list of tables in FROM clause.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_from_clause(THD *thd, String *str, enum_query_type query_type);

  /**
    Print list of conditions in WHERE clause.

    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_where_cond(String *str, enum_query_type query_type);

  /**
    Print list of items in GROUP BY clause.

    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_group_by(String *str, enum_query_type query_type);

  /**
    Print list of items in HAVING clause.

    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_having(String *str, enum_query_type query_type);

  /**
    Print details of Windowing functions.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_windows(THD *thd, String *str, enum_query_type query_type);

  /**
    Print list of items in ORDER BY clause.

    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_order_by(String *str, enum_query_type query_type);

  static void print_order(String *str, ORDER *order,
                          enum_query_type query_type);
  void print_limit(String *str, enum_query_type query_type);
  void fix_prepare_information(THD *thd);

  bool accept(Select_lex_visitor *visitor);

  /**
    Cleanup this subtree (this SELECT_LEX and all nested SELECT_LEXes and
    SELECT_LEX_UNITs).
    @param full  if false only partial cleanup is done, JOINs and JOIN_TABs are
    kept to provide info for EXPLAIN CONNECTION; if true, complete cleanup is
    done, all JOINs are freed.
  */
  bool cleanup(bool full);
  /*
    Recursively cleanup the join of this select lex and of all nested
    select lexes. This is not a full cleanup.
  */
  void cleanup_all_joins();

  /// Return true if this query block is part of a UNION
  bool is_part_of_union() const { return master_unit()->is_union(); }

  /**
    @return true if query block is found during preparation to produce no data.
    Notice that if query is implicitly grouped, an aggregation row will
    still be returned.
  */
  bool is_empty_query() const { return m_empty_query; }

  /// Set query block as returning no data
  /// @todo This may also be set when we have an always false WHERE clause
  void set_empty_query() {
    DBUG_ASSERT(join == NULL);
    m_empty_query = true;
  }
  /*
    For MODE_ONLY_FULL_GROUP_BY we need to know if
    this query block is the aggregation query of at least one aggregate
    function.
  */
  bool agg_func_used() const { return m_agg_func_used; }
  bool json_agg_func_used() const { return m_json_agg_func_used; }

  void set_agg_func_used(bool val) { m_agg_func_used = val; }

  void set_json_agg_func_used(bool val) { m_json_agg_func_used = val; }

  /// Lookup for SELECT_LEX type
  enum_explain_type type();

  /// Lookup for a type string
  const char *get_type_str() { return type_str[static_cast<int>(type())]; }
  static const char *get_type_str(enum_explain_type type) {
    return type_str[static_cast<int>(type)];
  }

  bool is_dependent() const { return uncacheable & UNCACHEABLE_DEPENDENT; }
  bool is_cacheable() const { return !uncacheable; }

  /// Include query block inside a query expression.
  void include_down(LEX *lex, SELECT_LEX_UNIT *outer);

  /// Include a query block next to another query block.
  void include_neighbour(LEX *lex, SELECT_LEX *before);

  /// Include query block inside a query expression, but do not link.
  void include_standalone(SELECT_LEX_UNIT *sel, SELECT_LEX **ref);

  /// Include query block into global list.
  void include_in_global(SELECT_LEX **plink);

  /// Include chain of query blocks into global list.
  void include_chain_in_global(SELECT_LEX **start);

  /// Renumber query blocks of contained query expressions
  void renumber(LEX *lex);

  /**
     Set pointer to corresponding JOIN object.
     The function sets the pointer only after acquiring THD::LOCK_query_plan
     mutex. This is needed to avoid races when EXPLAIN FOR CONNECTION is used.
  */
  void set_join(JOIN *join_arg);
  /**
    Does permanent transformations which are local to a query block (which do
    not merge it to another block).
  */
  bool apply_local_transforms(THD *thd, bool prune);

  bool get_optimizable_conditions(THD *thd, Item **new_where,
                                  Item **new_having);

  bool validate_outermost_option(LEX *lex, const char *wrong_option) const;
  bool validate_base_options(LEX *lex, ulonglong options) const;

 private:
  // Delete unused columns from merged derived tables
  void delete_unused_merged_columns(List<TABLE_LIST> *tables);

  bool m_agg_func_used;
  bool m_json_agg_func_used;

  /**
    True if query block does not generate any rows before aggregation,
    determined during preparation (not optimization).
  */
  bool m_empty_query;

  /// Helper for fix_prepare_information()
  void fix_prepare_information_for_order(THD *thd, SQL_I_List<ORDER> *list,
                                         Group_list_ptrs **list_ptrs);
  static const char
      *type_str[static_cast<int>(enum_explain_type::EXPLAIN_total)];

  friend class SELECT_LEX_UNIT;
  bool record_join_nest_info(List<TABLE_LIST> *tables);
  bool simplify_joins(THD *thd, List<TABLE_LIST> *join_list, bool top,
                      bool in_sj, Item **new_conds, uint *changelog = NULL);
  /// Merge derived table into query block
 public:
  bool merge_derived(THD *thd, TABLE_LIST *derived_table);

 private:
  bool convert_subquery_to_semijoin(Item_exists_subselect *subq_pred);
  void remap_tables(THD *thd);
  bool resolve_subquery(THD *thd);
  bool resolve_rollup(THD *thd);
  bool change_func_or_wf_group_ref(THD *thd, Item *func, bool *changed);

 public:
  bool flatten_subqueries();
  void set_sj_candidates(Mem_root_array<Item_exists_subselect *> *sj_cand) {
    sj_candidates = sj_cand;
  }

  bool has_sj_candidates() const {
    return sj_candidates != NULL && !sj_candidates->empty();
  }

 private:
  bool setup_wild(THD *thd);
  bool setup_order_final(THD *thd);
  bool setup_group(THD *thd);
  void remove_redundant_subquery_clauses(THD *thd,
                                         int hidden_group_field_count);
  void repoint_contexts_of_join_nests(List<TABLE_LIST> join_list);
  void empty_order_list(SELECT_LEX *sl);
  bool setup_join_cond(THD *thd, List<TABLE_LIST> *tables, bool in_update);
  bool find_common_table_expr(THD *thd, Table_ident *table_id, TABLE_LIST *tl,
                              Parse_context *pc, bool *found);

  /**
    Pointer to collection of subqueries candidate for semijoin
    conversion.
    Template parameter is "true": no need to run DTORs on pointers.
  */
  Mem_root_array<Item_exists_subselect *> *sj_candidates;

 public:
  /// How many expressions are part of the order by but not select list.
  int hidden_order_field_count;

  bool fix_inner_refs(THD *thd);
  bool setup_conds(THD *thd);
  bool prepare(THD *thd);
  bool optimize(THD *thd);
  void reset_nj_counters(List<TABLE_LIST> *join_list = NULL);
  bool check_only_full_group_by(THD *thd);

  /// Merge name resolution context objects of a subquery into its parent
  void merge_contexts(SELECT_LEX *inner);

  /**
    Returns which subquery execution strategies can be used for this query
    block.

    @param thd  Pointer to THD object for session.
                Used to access optimizer_switch

    @retval EXEC_MATERIALIZATION  Subquery Materialization should be used
    @retval EXEC_EXISTS           In-to-exists execution should be used
    @retval EXEC_EXISTS_OR_MAT    A cost-based decision should be made
  */
  Item_exists_subselect::enum_exec_method subquery_strategy(THD *thd) const;

  /**
    Returns whether semi-join is enabled for this query block

    @see @c Opt_hints_qb::semijoin_enabled for details on how hints
    affect this decision.  If there are no hints for this query block,
    optimizer_switch setting determines whether semi-join is used.

    @param thd  Pointer to THD object for session.
                Used to access optimizer_switch

    @return true if semijoin is enabled,
            false otherwise
  */
  bool semijoin_enabled(THD *thd) const;
  /**
    Update available semijoin strategies for semijoin nests.

    Available semijoin strategies needs to be updated on every execution since
    optimizer_switch setting may have changed.

    @param thd  Pointer to THD object for session.
                Used to access optimizer_switch
  */
  void update_semijoin_strategies(THD *thd);

  /**
    Add item to the hidden part of select list

    @param item  item to add

    @return Pointer to reference of the added item
  */
  Item **add_hidden_item(Item *item);

  bool add_tables(THD *thd, const Mem_root_array<Table_ident *> *tables,
                  ulong table_options, thr_lock_type lock_type,
                  enum_mdl_type mdl_type);

  TABLE_LIST *find_table_by_name(const Table_ident *ident);
};

typedef class SELECT_LEX SELECT_LEX;

inline bool SELECT_LEX_UNIT::is_union() const {
  return first_select()->next_select() &&
         first_select()->next_select()->linkage == UNION_TYPE;
}

struct Cast_type {
  Cast_target target;
  const CHARSET_INFO *charset;
  const char *length;
  const char *dec;
};

struct Limit_options {
  Item *limit;
  Item *opt_offset;
  /*
    true for "LIMIT offset,limit" and false for "LIMIT limit OFFSET offset"
  */
  bool is_offset_first;
};

struct Query_options {
  ulonglong query_spec_options;

  bool merge(const Query_options &a, const Query_options &b);
  bool save_to(Parse_context *);
};

enum delete_option_enum {
  DELETE_QUICK = 1 << 0,
  DELETE_LOW_PRIORITY = 1 << 1,
  DELETE_IGNORE = 1 << 2
};

enum class Lock_strength { UPDATE, SHARE };

/// We will static_cast this one to thr_lock_type.
enum class Locked_row_action {
  DEFAULT = THR_DEFAULT,
  WAIT = THR_WAIT,
  NOWAIT = THR_NOWAIT,
  SKIP = THR_SKIP
};

/**
  Internally there is no CROSS JOIN join type, as cross joins are just a
  special case of inner joins with a join condition that is always true. The
  only difference is the nesting, and that is handled by the parser.
*/
enum PT_joined_table_type {
  JTT_INNER = 0x01,
  JTT_STRAIGHT = 0x02,
  JTT_NATURAL = 0x04,
  JTT_LEFT = 0x08,
  JTT_RIGHT = 0x10,

  JTT_STRAIGHT_INNER = JTT_STRAIGHT | JTT_INNER,
  JTT_NATURAL_INNER = JTT_NATURAL | JTT_INNER,
  JTT_NATURAL_LEFT = JTT_NATURAL | JTT_LEFT,
  JTT_NATURAL_RIGHT = JTT_NATURAL | JTT_RIGHT
};

typedef Mem_root_array_YY<LEX_CSTRING> Create_col_name_list;

enum class Ternary_option { DEFAULT, ON, OFF };

enum class On_duplicate { ERROR, IGNORE_DUP, REPLACE_DUP };

enum class Virtual_or_stored { VIRTUAL, STORED };

enum class Field_option : ulong {
  NONE = 0,
  UNSIGNED = UNSIGNED_FLAG,
  ZEROFILL_UNSIGNED = UNSIGNED_FLAG | ZEROFILL_FLAG
};

enum class Int_type : ulong {
  INT = MYSQL_TYPE_LONG,
  TINYINT = MYSQL_TYPE_TINY,
  SMALLINT = MYSQL_TYPE_SHORT,
  MEDIUMINT = MYSQL_TYPE_INT24,
  BIGINT = MYSQL_TYPE_LONGLONG,
};

enum class Numeric_type : ulong {
  DECIMAL = MYSQL_TYPE_NEWDECIMAL,
  FLOAT = MYSQL_TYPE_FLOAT,
  DOUBLE = MYSQL_TYPE_DOUBLE,
};

enum class Show_cmd_type {
  STANDARD,
  FULL_SHOW,
  EXTENDED_SHOW,
  EXTENDED_FULL_SHOW
};

/**
  std::optional-like wrapper for simple bitmaps (usually enums of binary flags)

  This template wraps trivial bitmap implementations to add two features:

  * std::optional-like behavior -- the "unset" flag, so we don't have
    to inject a special "invalid" value into existent enum types, this
    wrapper class does that for us.

  * the merge() function to merge two bitmap values in a type-safe way.

  @tparam Enum           Usually a enum type which simulates a bit set.
  @tparam Default_value  A default Enum value for "unset" variables.

*/
template <typename Enum, Enum Default_value>
class Enum_parser {
 public:
  /// Constructor-like function
  ///
  /// The Enum_parser<> class is designed for use as a field of restricted
  /// unions, so it can't have C++ constructors.
  void init() { m_is_set = false; }

  /// False if the wrapped Enum value is not assigned.
  bool is_set() const { return m_is_set; }

  /// Return the wrapped Enum value.
  ///
  /// @note The wrapped value must be assigned.
  Enum get() const {
    DBUG_ASSERT(is_set());
    return m_enum;
  }

  /// Return the wrapped Enum value (if any) or the Default_value.
  Enum get_or_default() const { return is_set() ? get() : Default_value; }

  /// Assign the wrapped Enum value.
  void set(Enum value) {
    m_is_set = true;
    m_enum = value;
  }

  /// Merge the x bit set into the wrapped Enum value (if any), or replace it
  void merge(const Enum_parser &x) {
    if (x.is_set()) set(x.get());
  }

 private:
  bool m_is_set;  ///< True if m_enum is assigned with some value
  Enum m_enum;    ///< The wrapped Enum value.
};

template <typename T>
struct Value_or_default {
  bool is_default;
  T value;  ///< undefined if is_default is true
};

enum class Explain_format_type { TRADITIONAL, JSON };

union YYSTYPE {
  /*
    Hint parser section (sql_hints.yy)
  */
  opt_hints_enum hint_type;
  LEX_CSTRING hint_string;
  class PT_hint *hint;
  class PT_hint_list *hint_list;
  Hint_param_index_list hint_param_index_list;
  Hint_param_table hint_param_table;
  Hint_param_table_list hint_param_table_list;

  /*
    Main parser section (sql_yacc.yy)
  */
  int num;
  ulong ulong_num;
  ulonglong ulonglong_number;
  LEX_CSTRING lex_cstr;
  LEX_STRING lex_str;
  LEX_STRING *lex_str_ptr;
  LEX_SYMBOL keyword;
  Table_ident *table;
  char *simple_string;
  Item *item;
  Item_num *item_num;
  List<Item> *item_list;
  List<String> *string_list;
  String *string;
  Key_part_spec *key_part;
  Mem_root_array<Table_ident *> *table_list;
  udf_func *udf;
  LEX_USER *lex_user;
  List<LEX_USER> *user_list;
  struct sys_var_with_base variable;
  enum enum_var_type var_type;
  keytype key_type;
  enum ha_key_alg key_alg;
  enum row_type row_type;
  enum ha_rkey_function ha_rkey_mode;
  enum_ha_read_modes ha_read_mode;
  enum enum_tx_isolation tx_isolation;
  const char *c_str;
  struct {
    const CHARSET_INFO *charset;
    bool force_binary;
  } charset_with_opt_binary;
  struct {
    const char *length;
    const char *dec;
  } precision;
  struct Cast_type cast_type;
  const CHARSET_INFO *charset;
  thr_lock_type lock_type;
  interval_type interval, interval_time_st;
  timestamp_type date_time_type;
  SELECT_LEX *select_lex;
  chooser_compare_func_creator boolfunc2creator;
  class sp_condition_value *spcondvalue;
  struct {
    int vars, conds, hndlrs, curs;
  } spblock;
  sp_name *spname;
  LEX *lex;
  sp_head *sphead;
  enum index_hint_type index_hint;
  enum enum_filetype filetype;
  enum fk_option m_fk_option;
  enum enum_yes_no_unknown m_yes_no_unk;
  enum_condition_item_name da_condition_item_name;
  Diagnostics_information::Which_area diag_area;
  Diagnostics_information *diag_info;
  Statement_information_item *stmt_info_item;
  Statement_information_item::Name stmt_info_item_name;
  List<Statement_information_item> *stmt_info_list;
  Condition_information_item *cond_info_item;
  Condition_information_item::Name cond_info_item_name;
  List<Condition_information_item> *cond_info_list;
  bool is_not_empty;
  Set_signal_information *signal_item_list;
  enum enum_trigger_order_type trigger_action_order_type;
  struct {
    enum enum_trigger_order_type ordering_clause;
    LEX_CSTRING anchor_trigger_name;
  } trg_characteristics;
  class Index_hint *key_usage_element;
  List<Index_hint> *key_usage_list;
  class PT_subselect *subselect;
  class PT_item_list *item_list2;
  class PT_order_expr *order_expr;
  class PT_order_list *order_list;
  struct Limit_options limit_options;
  Query_options select_options;
  class PT_limit_clause *limit_clause;
  Parse_tree_node *node;
  enum olap_type olap_type;
  class PT_group *group;
  class PT_window_list *windows;
  class PT_window *window;
  class PT_frame *window_frame;
  enum_window_frame_unit frame_units;
  class PT_borders *frame_extent;
  class PT_border *bound;
  class PT_exclusion *frame_exclusion;
  enum enum_null_treatment null_treatment;
  enum enum_from_first_last from_first_last;
  Item_string *item_string;
  class PT_order *order;
  class PT_table_reference *table_reference;
  class PT_joined_table *join_table;
  enum PT_joined_table_type join_type;
  class PT_internal_variable_name *internal_variable_name;
  class PT_option_value_following_option_type
      *option_value_following_option_type;
  class PT_option_value_no_option_type *option_value_no_option_type;
  class PT_option_value_list_head *option_value_list;
  class PT_start_option_value_list *start_option_value_list;
  class PT_transaction_access_mode *transaction_access_mode;
  class PT_isolation_level *isolation_level;
  class PT_transaction_characteristics *transaction_characteristics;
  class PT_start_option_value_list_following_option_type
      *start_option_value_list_following_option_type;
  class PT_set *set;
  Line_separators line_separators;
  Field_separators field_separators;
  class PT_into_destination *into_destination;
  class PT_select_var *select_var_ident;
  class PT_select_var_list *select_var_list;
  Mem_root_array_YY<PT_table_reference *> table_reference_list;
  class Item_param *param_marker;
  class PTI_text_literal *text_literal;
  class PT_query_expression *query_expression;
  class PT_derived_table *derived_table;
  class PT_query_expression_body *query_expression_body;
  class PT_query_primary *query_primary;
  class PT_subquery *subquery;

  XID *xid;
  enum xa_option_words xa_option_type;
  struct {
    Item *column;
    Item *value;
  } column_value_pair;
  struct {
    class PT_item_list *column_list;
    class PT_item_list *value_list;
  } column_value_list_pair;
  struct {
    class PT_item_list *column_list;
    class PT_insert_values_list *row_value_list;
  } column_row_value_list_pair;
  struct {
    class PT_item_list *column_list;
    class PT_query_expression *insert_query_expression;
  } insert_query_expression;
  struct {
    class Item *offset;
    class Item *default_value;
  } lead_lag_info;
  class PT_insert_values_list *values_list;
  Parse_tree_root *top_level_node;
  class Table_ident *table_ident;
  Mem_root_array_YY<Table_ident *> table_ident_list;
  delete_option_enum opt_delete_option;
  class PT_hint_list *optimizer_hints;
  enum alter_instance_action_enum alter_instance_action;
  class PT_create_index_stmt *create_index_stmt;
  class PT_table_constraint_def *table_constraint_def;
  List<Key_part_spec> *index_column_list;
  struct {
    LEX_STRING name;
    class PT_base_index_option *type;
  } index_name_and_type;
  class PT_base_index_option *index_option;
  Mem_root_array_YY<PT_base_index_option *> index_options;
  PT_base_index_option *index_type;
  Mem_root_array_YY<LEX_STRING> lex_str_list;
  bool visibility;
  class PT_with_clause *with_clause;
  class PT_with_list *with_list;
  class PT_common_table_expr *common_table_expr;
  Create_col_name_list simple_ident_list;
  class PT_partition_option *partition_option;
  Mem_root_array<PT_partition_option *> *partition_option_list;
  class PT_subpartition *sub_part_definition;
  Mem_root_array<PT_subpartition *> *sub_part_list;
  class PT_part_value_item *part_value_item;
  Mem_root_array<PT_part_value_item *> *part_value_item_list;
  class PT_part_value_item_list_paren *part_value_item_list_paren;
  Mem_root_array<PT_part_value_item_list_paren *> *part_value_list;
  class PT_part_values *part_values;
  struct {
    partition_type type;
    PT_part_values *values;
  } opt_part_values;
  class PT_part_definition *part_definition;
  Mem_root_array<PT_part_definition *> *part_def_list;
  List<char> *name_list;  // TODO: merge with string_list
  enum_key_algorithm opt_key_algo;
  class PT_sub_partition *opt_sub_part;
  class PT_part_type_def *part_type_def;
  class PT_partition *partition_clause;
  class PT_add_partition *add_partition_rule;
  struct {
    decltype(HA_CHECK_OPT::flags) flags;
    decltype(HA_CHECK_OPT::sql_flags) sql_flags;
  } mi_type;
  enum_drop_mode opt_restrict;
  Ternary_option ternary_option;
  class PT_create_table_option *create_table_option;
  Mem_root_array<PT_create_table_option *> *create_table_options;
  Mem_root_array<PT_ddl_table_option *> *space_separated_alter_table_opts;
  On_duplicate on_duplicate;
  class PT_column_attr_base *col_attr;
  column_format_type column_format;
  ha_storage_media storage_media;
  Mem_root_array<PT_column_attr_base *> *col_attr_list;
  Virtual_or_stored virtual_or_stored;
  Field_option field_option;
  Int_type int_type;
  class PT_type *type;
  Numeric_type numeric_type;
  struct {
    const char *expr_start;
    Item *expr;
  } sp_default;
  class PT_field_def_base *field_def;
  class PT_check_constraint *check_constraint;
  struct {
    fk_option fk_update_opt;
    fk_option fk_delete_opt;
  } fk_options;
  fk_match_opt opt_match_clause;
  List<Key_part_spec> *reference_list;
  struct {
    Table_ident *table_name;
    List<Key_part_spec> *reference_list;
    fk_match_opt fk_match_option;
    fk_option fk_update_opt;
    fk_option fk_delete_opt;
  } fk_references;
  class PT_column_def *column_def;
  class PT_table_element *table_element;
  Mem_root_array<PT_table_element *> *table_element_list;
  struct {
    Mem_root_array<PT_create_table_option *> *opt_create_table_options;
    PT_partition *opt_partitioning;
    On_duplicate on_duplicate;
    PT_query_expression *opt_query_expression;
  } create_table_tail;
  Lock_strength lock_strength;
  Locked_row_action locked_row_action;
  class PT_locking_clause *locking_clause;
  class PT_locking_clause_list *locking_clause_list;
  Mem_root_array<PT_json_table_column *> *jtc_list;
  struct jt_on_response {
    enum_jtc_on type;
    const LEX_STRING *default_str;
  } jt_on_response;
  struct {
    struct jt_on_response error;
    struct jt_on_response empty;
  } jt_on_error_or_empty;
  PT_json_table_column *jt_column;
  enum_jt_column jt_column_type;
  struct {
    LEX_STRING wild;
    Item *where;
  } wild_or_where;
  Show_cmd_type show_cmd_type;
  struct {
    Sql_cmd_analyze_table::Histogram_command command;
    List<String> *columns;
    int num_buckets;
  } histogram;
  Acl_type acl_type;
  Mem_root_array<LEX_CSTRING> *lex_cstring_list;
  class PT_role_or_privilege *role_or_privilege;
  Mem_root_array<PT_role_or_privilege *> *role_or_privilege_list;
  enum_order order_direction;
  Alter_info::enum_with_validation with_validation;
  class PT_alter_table_action *alter_table_action;
  class PT_alter_table_standalone_action *alter_table_standalone_action;
  Alter_info::enum_alter_table_algorithm alter_table_algorithm;
  Alter_info::enum_alter_table_lock alter_table_lock;
  struct Algo_and_lock {
    Enum_parser<Alter_info::enum_alter_table_algorithm,
                Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT>
        algo;
    Enum_parser<Alter_info::enum_alter_table_lock,
                Alter_info::ALTER_TABLE_LOCK_DEFAULT>
        lock;
    void init() {
      algo.init();
      lock.init();
    }
  } opt_index_lock_and_algorithm;
  struct Algo_and_lock_and_validation {
    Enum_parser<Alter_info::enum_alter_table_algorithm,
                Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT>
        algo;
    Enum_parser<Alter_info::enum_alter_table_lock,
                Alter_info::ALTER_TABLE_LOCK_DEFAULT>
        lock;
    Enum_parser<Alter_info::enum_with_validation,
                Alter_info::ALTER_VALIDATION_DEFAULT>
        validation;
    void init() {
      algo.init();
      lock.init();
      validation.init();
    }
    void merge(const Algo_and_lock_and_validation &x) {
      algo.merge(x.algo);
      lock.merge(x.lock);
      validation.merge(x.validation);
    }
  } algo_and_lock_and_validation;
  struct {
    Algo_and_lock_and_validation flags;
    Mem_root_array<PT_ddl_table_option *> *actions;
  } alter_list;
  struct {
    Algo_and_lock_and_validation flags;
    PT_alter_table_standalone_action *action;
  } standalone_alter_table_action;
  class PT_assign_to_keycache *assign_to_keycache;
  Mem_root_array<PT_assign_to_keycache *> *keycache_list;
  class PT_adm_partition *adm_partition;
  class PT_preload_keys *preload_keys;
  Mem_root_array<PT_preload_keys *> *preload_list;
  PT_alter_tablespace_option_base *ts_option;
  Mem_root_array<PT_alter_tablespace_option_base *> *ts_options;
  struct {
    resourcegroups::platform::cpu_id_t start;
    resourcegroups::platform::cpu_id_t end;
  } vcpu_range_type;
  Mem_root_array<resourcegroups::Range> *resource_group_vcpu_list_type;
  Value_or_default<int> resource_group_priority_type;
  Value_or_default<bool> resource_group_state_type;
  bool resource_group_flag_type;
  resourcegroups::Type resource_group_type;
  Mem_root_array<ulonglong> *thread_id_list_type;
  Explain_format_type explain_format_type;
  struct {
    Item *set_var;
    Item *set_expr;
    String *set_expr_str;
  } load_set_element;
  struct {
    PT_item_list *set_var_list;
    PT_item_list *set_expr_list;
    List<String> *set_expr_str_list;
  } load_set_list;
  Sql_cmd_srs_attributes *sql_cmd_srs_attributes;
};

static_assert(sizeof(YYSTYPE) <= 32, "YYSTYPE is too big");

/**
  Utility RAII class to save/modify/restore the
  semijoin_disallowed flag.
*/
class Disable_semijoin_flattening {
 public:
  Disable_semijoin_flattening(SELECT_LEX *select_ptr, bool apply)
      : select(NULL), saved_value() {
    if (select_ptr && apply) {
      select = select_ptr;
      saved_value = select->semijoin_disallowed;
      select->semijoin_disallowed = true;
    }
  }
  ~Disable_semijoin_flattening() {
    if (select) select->semijoin_disallowed = saved_value;
  }

 private:
  SELECT_LEX *select;
  bool saved_value;
};

typedef struct struct_slave_connection {
  char *user;
  char *password;
  char *plugin_auth;
  char *plugin_dir;

  void reset();
} LEX_SLAVE_CONNECTION;

struct st_sp_chistics {
  LEX_CSTRING comment;
  enum enum_sp_suid_behaviour suid;
  bool detistic;
  enum enum_sp_data_access daccess;
};

extern const LEX_STRING null_lex_str;
extern const LEX_STRING empty_lex_str;

struct st_trg_chistics {
  enum enum_trigger_action_time_type action_time;
  enum enum_trigger_event_type event;

  /**
    FOLLOWS or PRECEDES as specified in the CREATE TRIGGER statement.
  */
  enum enum_trigger_order_type ordering_clause;

  /**
    Trigger name referenced in the FOLLOWS/PRECEDES clause of the CREATE TRIGGER
    statement.
  */
  LEX_CSTRING anchor_trigger_name;
};

extern sys_var *trg_new_row_fake_var;

class Sroutine_hash_entry;

/*
  Class representing list of all tables used by statement and other
  information which is necessary for opening and locking its tables,
  like SQL command for this statement.

  Also contains information about stored functions used by statement
  since during its execution we may have to add all tables used by its
  stored functions/triggers to this list in order to pre-open and lock
  them.

  Also used by LEX::reset_n_backup/restore_backup_query_tables_list()
  methods to save and restore this information.
*/

class Query_tables_list {
 public:
  Query_tables_list &operator=(Query_tables_list &&) = default;

  /**
    SQL command for this statement. Part of this class since the
    process of opening and locking tables for the statement needs
    this information to determine correct type of lock for some of
    the tables.
  */
  enum_sql_command sql_command;
  /* Global list of all tables used by this statement */
  TABLE_LIST *query_tables;
  /* Pointer to next_global member of last element in the previous list. */
  TABLE_LIST **query_tables_last;
  /*
    If non-0 then indicates that query requires prelocking and points to
    next_global member of last own element in query table list (i.e. last
    table which was not added to it as part of preparation to prelocking).
    0 - indicates that this query does not need prelocking.
  */
  TABLE_LIST **query_tables_own_last;
  /*
    Set of stored routines called by statement.
    (Note that we use lazy-initialization for this hash).

    See Sroutine_hash_entry for explanation why this hash uses binary
    key comparison.
  */
  enum { START_SROUTINES_HASH_SIZE = 16 };
  std::unique_ptr<malloc_unordered_map<std::string, Sroutine_hash_entry *>>
      sroutines;
  /*
    List linking elements of 'sroutines' set. Allows you to add new elements
    to this set as you iterate through the list of existing elements.
    'sroutines_list_own_last' is pointer to ::next member of last element of
    this list which represents routine which is explicitly used by query.
    'sroutines_list_own_elements' number of explicitly used routines.
    We use these two members for restoring of 'sroutines_list' to the state
    in which it was right after query parsing.
  */
  SQL_I_List<Sroutine_hash_entry> sroutines_list;
  Sroutine_hash_entry **sroutines_list_own_last;
  uint sroutines_list_own_elements;

  /**
    Locking state of tables in this particular statement.

    If we under LOCK TABLES or in prelocked mode we consider tables
    for the statement to be "locked" if there was a call to lock_tables()
    (which called handler::start_stmt()) for tables of this statement
    and there was no matching close_thread_tables() call.

    As result this state may differ significantly from one represented
    by Open_tables_state::lock/locked_tables_mode more, which are always
    "on" under LOCK TABLES or in prelocked mode.
  */
  enum enum_lock_tables_state { LTS_NOT_LOCKED = 0, LTS_LOCKED };
  enum_lock_tables_state lock_tables_state;
  bool is_query_tables_locked() { return (lock_tables_state == LTS_LOCKED); }

  /**
    Number of tables which were open by open_tables() and to be locked
    by lock_tables().
    Note that we set this member only in some cases, when this value
    needs to be passed from open_tables() to lock_tables() which are
    separated by some amount of code.
  */
  uint table_count;

  /*
    These constructor and destructor serve for creation/destruction
    of Query_tables_list instances which are used as backup storage.
  */
  Query_tables_list() {}
  ~Query_tables_list() {}

  /* Initializes (or resets) Query_tables_list object for "real" use. */
  void reset_query_tables_list(bool init);
  void destroy_query_tables_list();
  void set_query_tables_list(Query_tables_list *state) {
    *this = std::move(*state);
  }

  /*
    Direct addition to the list of query tables.
    If you are using this function, you must ensure that the table
    object, in particular table->db member, is initialized.
  */
  void add_to_query_tables(TABLE_LIST *table) {
    *(table->prev_global = query_tables_last) = table;
    query_tables_last = &table->next_global;
  }
  bool requires_prelocking() { return query_tables_own_last; }
  void mark_as_requiring_prelocking(TABLE_LIST **tables_own_last) {
    query_tables_own_last = tables_own_last;
  }
  /* Return pointer to first not-own table in query-tables or 0 */
  TABLE_LIST *first_not_own_table() {
    return (query_tables_own_last ? *query_tables_own_last : 0);
  }
  void chop_off_not_own_tables() {
    if (query_tables_own_last) {
      *query_tables_own_last = 0;
      query_tables_last = query_tables_own_last;
      query_tables_own_last = 0;
    }
  }

  /**
    All types of unsafe statements.

    @note The int values of the enum elements are used to point to
    bits in two bitmaps in two different places:

    - Query_tables_list::binlog_stmt_flags
    - THD::binlog_unsafe_warning_flags

    Hence in practice this is not an enum at all, but a map from
    symbols to bit indexes.

    The ordering of elements in this enum must correspond to the order of
    elements in the array binlog_stmt_unsafe_errcode.
  */
  enum enum_binlog_stmt_unsafe {
    /**
      SELECT..LIMIT is unsafe because the set of rows returned cannot
      be predicted.
    */
    BINLOG_STMT_UNSAFE_LIMIT = 0,
    /**
      Access to log tables is unsafe because slave and master probably
      log different things.
    */
    BINLOG_STMT_UNSAFE_SYSTEM_TABLE,
    /**
      Inserting into an autoincrement column in a stored routine is unsafe.
      Even with just one autoincrement column, if the routine is invoked more
      than once slave is not guaranteed to execute the statement graph same way
      as the master. And since it's impossible to estimate how many times a
      routine can be invoked at the query pre-execution phase (see lock_tables),
      the statement is marked pessimistically unsafe.
    */
    BINLOG_STMT_UNSAFE_AUTOINC_COLUMNS,
    /**
      Using a UDF (user-defined function) is unsafe.
    */
    BINLOG_STMT_UNSAFE_UDF,
    /**
      Using most system variables is unsafe, because slave may run
      with different options than master.
    */
    BINLOG_STMT_UNSAFE_SYSTEM_VARIABLE,
    /**
      Using some functions is unsafe (e.g., UUID).
    */
    BINLOG_STMT_UNSAFE_SYSTEM_FUNCTION,

    /**
      Mixing transactional and non-transactional statements are unsafe if
      non-transactional reads or writes are occur after transactional
      reads or writes inside a transaction.
    */
    BINLOG_STMT_UNSAFE_NONTRANS_AFTER_TRANS,

    /**
      Mixing self-logging and non-self-logging engines in a statement
      is unsafe.
    */
    BINLOG_STMT_UNSAFE_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE,

    /**
      Statements that read from both transactional and non-transactional
      tables and write to any of them are unsafe.
    */
    BINLOG_STMT_UNSAFE_MIXED_STATEMENT,

    /**
      INSERT...IGNORE SELECT is unsafe because which rows are ignored depends
      on the order that rows are retrieved by SELECT. This order cannot be
      predicted and may differ on master and the slave.
    */
    BINLOG_STMT_UNSAFE_INSERT_IGNORE_SELECT,

    /**
      INSERT...SELECT...UPDATE is unsafe because which rows are updated depends
      on the order that rows are retrieved by SELECT. This order cannot be
      predicted and may differ on master and the slave.
    */
    BINLOG_STMT_UNSAFE_INSERT_SELECT_UPDATE,

    /**
     Query that writes to a table with auto_inc column after selecting from
     other tables are unsafe as the order in which the rows are retrieved by
     select may differ on master and slave.
    */
    BINLOG_STMT_UNSAFE_WRITE_AUTOINC_SELECT,

    /**
      INSERT...REPLACE SELECT is unsafe because which rows are replaced depends
      on the order that rows are retrieved by SELECT. This order cannot be
      predicted and may differ on master and the slave.
    */
    BINLOG_STMT_UNSAFE_REPLACE_SELECT,

    /**
      CREATE TABLE... IGNORE... SELECT is unsafe because which rows are ignored
      depends on the order that rows are retrieved by SELECT. This order cannot
      be predicted and may differ on master and the slave.
    */
    BINLOG_STMT_UNSAFE_CREATE_IGNORE_SELECT,

    /**
      CREATE TABLE...REPLACE... SELECT is unsafe because which rows are replaced
      depends on the order that rows are retrieved from SELECT. This order
      cannot be predicted and may differ on master and the slave
    */
    BINLOG_STMT_UNSAFE_CREATE_REPLACE_SELECT,

    /**
      CREATE TABLE...SELECT on a table with auto-increment column is unsafe
      because which rows are replaced depends on the order that rows are
      retrieved from SELECT. This order cannot be predicted and may differ on
      master and the slave
    */
    BINLOG_STMT_UNSAFE_CREATE_SELECT_AUTOINC,

    /**
      UPDATE...IGNORE is unsafe because which rows are ignored depends on the
      order that rows are updated. This order cannot be predicted and may differ
      on master and the slave.
    */
    BINLOG_STMT_UNSAFE_UPDATE_IGNORE,

    /**
      INSERT... ON DUPLICATE KEY UPDATE on a table with more than one
      UNIQUE KEYS  is unsafe.
    */
    BINLOG_STMT_UNSAFE_INSERT_TWO_KEYS,

    /**
       INSERT into auto-inc field which is not the first part in composed
       primary key.
    */
    BINLOG_STMT_UNSAFE_AUTOINC_NOT_FIRST,

    /**
       Using a plugin is unsafe.
    */
    BINLOG_STMT_UNSAFE_FULLTEXT_PLUGIN,
    BINLOG_STMT_UNSAFE_SKIP_LOCKED,
    BINLOG_STMT_UNSAFE_NOWAIT,

    /**
      XA transactions and statements.
    */
    BINLOG_STMT_UNSAFE_XA,

    /* the last element of this enumeration type. */
    BINLOG_STMT_UNSAFE_COUNT
  };
  /**
    This has all flags from 0 (inclusive) to BINLOG_STMT_FLAG_COUNT
    (exclusive) set.
  */
  static const int BINLOG_STMT_UNSAFE_ALL_FLAGS =
      ((1 << BINLOG_STMT_UNSAFE_COUNT) - 1);

  /**
    Maps elements of enum_binlog_stmt_unsafe to error codes.
  */
  static const int binlog_stmt_unsafe_errcode[BINLOG_STMT_UNSAFE_COUNT];

  /**
    Determine if this statement is marked as unsafe.

    @retval 0 if the statement is not marked as unsafe.
    @retval nonzero if the statement is marked as unsafe.
  */
  inline bool is_stmt_unsafe() const { return get_stmt_unsafe_flags() != 0; }

  inline bool is_stmt_unsafe(enum_binlog_stmt_unsafe unsafe) {
    return binlog_stmt_flags & (1 << unsafe);
  }

  /**
    Flag the current (top-level) statement as unsafe.
    The flag will be reset after the statement has finished.

    @param unsafe_type The type of unsafety: one of the @c
    BINLOG_STMT_FLAG_UNSAFE_* flags in @c enum_binlog_stmt_flag.
  */
  inline void set_stmt_unsafe(enum_binlog_stmt_unsafe unsafe_type) {
    DBUG_ENTER("set_stmt_unsafe");
    DBUG_ASSERT(unsafe_type >= 0 && unsafe_type < BINLOG_STMT_UNSAFE_COUNT);
    binlog_stmt_flags |= (1U << unsafe_type);
    DBUG_VOID_RETURN;
  }

  /**
    Set the bits of binlog_stmt_flags determining the type of
    unsafeness of the current statement.  No existing bits will be
    cleared, but new bits may be set.

    @param flags A binary combination of zero or more bits, (1<<flag)
    where flag is a member of enum_binlog_stmt_unsafe.
  */
  inline void set_stmt_unsafe_flags(uint32 flags) {
    DBUG_ENTER("set_stmt_unsafe_flags");
    DBUG_ASSERT((flags & ~BINLOG_STMT_UNSAFE_ALL_FLAGS) == 0);
    binlog_stmt_flags |= flags;
    DBUG_VOID_RETURN;
  }

  /**
    Return a binary combination of all unsafe warnings for the
    statement.  If the statement has been marked as unsafe by the
    'flag' member of enum_binlog_stmt_unsafe, then the return value
    from this function has bit (1<<flag) set to 1.
  */
  inline uint32 get_stmt_unsafe_flags() const {
    DBUG_ENTER("get_stmt_unsafe_flags");
    DBUG_RETURN(binlog_stmt_flags & BINLOG_STMT_UNSAFE_ALL_FLAGS);
  }

  /**
    Determine if this statement is a row injection.

    @retval 0 if the statement is not a row injection
    @retval nonzero if the statement is a row injection
  */
  inline bool is_stmt_row_injection() const {
    return binlog_stmt_flags &
           (1U << (BINLOG_STMT_UNSAFE_COUNT + BINLOG_STMT_TYPE_ROW_INJECTION));
  }

  /**
    Flag the statement as a row injection.  A row injection is either
    a BINLOG statement, or a row event in the relay log executed by
    the slave SQL thread.
  */
  inline void set_stmt_row_injection() {
    DBUG_ENTER("set_stmt_row_injection");
    binlog_stmt_flags |=
        (1U << (BINLOG_STMT_UNSAFE_COUNT + BINLOG_STMT_TYPE_ROW_INJECTION));
    DBUG_VOID_RETURN;
  }

  enum enum_stmt_accessed_table {
    /*
       If a transactional table is about to be read. Note that
       a write implies a read.
    */
    STMT_READS_TRANS_TABLE = 0,
    /*
       If a non-transactional table is about to be read. Note that
       a write implies a read.
    */
    STMT_READS_NON_TRANS_TABLE,
    /*
       If a temporary transactional table is about to be read. Note
       that a write implies a read.
    */
    STMT_READS_TEMP_TRANS_TABLE,
    /*
       If a temporary non-transactional table is about to be read. Note
      that a write implies a read.
    */
    STMT_READS_TEMP_NON_TRANS_TABLE,
    /*
       If a transactional table is about to be updated.
    */
    STMT_WRITES_TRANS_TABLE,
    /*
       If a non-transactional table is about to be updated.
    */
    STMT_WRITES_NON_TRANS_TABLE,
    /*
       If a temporary transactional table is about to be updated.
    */
    STMT_WRITES_TEMP_TRANS_TABLE,
    /*
       If a temporary non-transactional table is about to be updated.
    */
    STMT_WRITES_TEMP_NON_TRANS_TABLE,
    /*
      The last element of the enumeration. Please, if necessary add
      anything before this.
    */
    STMT_ACCESS_TABLE_COUNT
  };

#ifndef DBUG_OFF
  static inline const char *stmt_accessed_table_string(
      enum_stmt_accessed_table accessed_table) {
    switch (accessed_table) {
      case STMT_READS_TRANS_TABLE:
        return "STMT_READS_TRANS_TABLE";
        break;
      case STMT_READS_NON_TRANS_TABLE:
        return "STMT_READS_NON_TRANS_TABLE";
        break;
      case STMT_READS_TEMP_TRANS_TABLE:
        return "STMT_READS_TEMP_TRANS_TABLE";
        break;
      case STMT_READS_TEMP_NON_TRANS_TABLE:
        return "STMT_READS_TEMP_NON_TRANS_TABLE";
        break;
      case STMT_WRITES_TRANS_TABLE:
        return "STMT_WRITES_TRANS_TABLE";
        break;
      case STMT_WRITES_NON_TRANS_TABLE:
        return "STMT_WRITES_NON_TRANS_TABLE";
        break;
      case STMT_WRITES_TEMP_TRANS_TABLE:
        return "STMT_WRITES_TEMP_TRANS_TABLE";
        break;
      case STMT_WRITES_TEMP_NON_TRANS_TABLE:
        return "STMT_WRITES_TEMP_NON_TRANS_TABLE";
        break;
      case STMT_ACCESS_TABLE_COUNT:
      default:
        DBUG_ASSERT(0);
        break;
    }
    MY_ASSERT_UNREACHABLE();
    return "";
  }
#endif /* DBUG */

#define BINLOG_DIRECT_ON                    \
  0xF0 /* unsafe when                       \
          --binlog-direct-non-trans-updates \
          is ON */

#define BINLOG_DIRECT_OFF                  \
  0xF /* unsafe when                       \
         --binlog-direct-non-trans-updates \
         is OFF */

#define TRX_CACHE_EMPTY 0x33 /* unsafe when trx-cache is empty */

#define TRX_CACHE_NOT_EMPTY 0xCC /* unsafe when trx-cache is not empty */

#define IL_LT_REPEATABLE 0xAA /* unsafe when < ISO_REPEATABLE_READ */

#define IL_GTE_REPEATABLE 0x55 /* unsafe when >= ISO_REPEATABLE_READ */

  /**
    Sets the type of table that is about to be accessed while executing a
    statement.

    @param accessed_table Enumeration type that defines the type of table,
                           e.g. temporary, transactional, non-transactional.
  */
  inline void set_stmt_accessed_table(enum_stmt_accessed_table accessed_table) {
    DBUG_ENTER("LEX::set_stmt_accessed_table");

    DBUG_ASSERT(accessed_table >= 0 &&
                accessed_table < STMT_ACCESS_TABLE_COUNT);
    stmt_accessed_table_flag |= (1U << accessed_table);

    DBUG_VOID_RETURN;
  }

  /**
    Checks if a type of table is about to be accessed while executing a
    statement.

    @param accessed_table Enumeration type that defines the type of table,
           e.g. temporary, transactional, non-transactional.

    @return
      @retval true  if the type of the table is about to be accessed
      @retval false otherwise
  */
  inline bool stmt_accessed_table(enum_stmt_accessed_table accessed_table) {
    DBUG_ENTER("LEX::stmt_accessed_table");

    DBUG_ASSERT(accessed_table >= 0 &&
                accessed_table < STMT_ACCESS_TABLE_COUNT);

    DBUG_RETURN((stmt_accessed_table_flag & (1U << accessed_table)) != 0);
  }

  /*
    Checks if a mixed statement is unsafe.


    @param in_multi_stmt_transaction_mode defines if there is an on-going
           multi-transactional statement.
    @param binlog_direct defines if --binlog-direct-non-trans-updates is
           active.
    @param trx_cache_is_not_empty defines if the trx-cache is empty or not.
    @param trx_isolation defines the isolation level.

    @return
      @retval true if the mixed statement is unsafe
      @retval false otherwise
  */
  inline bool is_mixed_stmt_unsafe(bool in_multi_stmt_transaction_mode,
                                   bool binlog_direct,
                                   bool trx_cache_is_not_empty,
                                   uint tx_isolation) {
    bool unsafe = false;

    if (in_multi_stmt_transaction_mode) {
      uint condition =
          (binlog_direct ? BINLOG_DIRECT_ON : BINLOG_DIRECT_OFF) &
          (trx_cache_is_not_empty ? TRX_CACHE_NOT_EMPTY : TRX_CACHE_EMPTY) &
          (tx_isolation >= ISO_REPEATABLE_READ ? IL_GTE_REPEATABLE
                                               : IL_LT_REPEATABLE);

      unsafe = (binlog_unsafe_map[stmt_accessed_table_flag] & condition);

#if !defined(DBUG_OFF)
      DBUG_PRINT("LEX::is_mixed_stmt_unsafe",
                 ("RESULT %02X %02X %02X\n", condition,
                  binlog_unsafe_map[stmt_accessed_table_flag],
                  (binlog_unsafe_map[stmt_accessed_table_flag] & condition)));

      int type_in = 0;
      for (; type_in < STMT_ACCESS_TABLE_COUNT; type_in++) {
        if (stmt_accessed_table((enum_stmt_accessed_table)type_in))
          DBUG_PRINT("LEX::is_mixed_stmt_unsafe",
                     ("ACCESSED %s ", stmt_accessed_table_string(
                                          (enum_stmt_accessed_table)type_in)));
      }
#endif
    }

    if (stmt_accessed_table(STMT_WRITES_NON_TRANS_TABLE) &&
        stmt_accessed_table(STMT_READS_TRANS_TABLE) &&
        tx_isolation < ISO_REPEATABLE_READ)
      unsafe = true;
    else if (stmt_accessed_table(STMT_WRITES_TEMP_NON_TRANS_TABLE) &&
             stmt_accessed_table(STMT_READS_TRANS_TABLE) &&
             tx_isolation < ISO_REPEATABLE_READ)
      unsafe = true;

    return (unsafe);
  }

  /**
    true if the parsed tree contains references to stored procedures
    or functions, false otherwise
  */
  bool uses_stored_routines() const { return sroutines_list.elements != 0; }

  void set_using_match() { using_match = true; }
  bool get_using_match() { return using_match; }

 private:
  /**
    Enumeration listing special types of statements.

    Currently, the only possible type is ROW_INJECTION.
  */
  enum enum_binlog_stmt_type {
    /**
      The statement is a row injection (i.e., either a BINLOG
      statement or a row event executed by the slave SQL thread).
    */
    BINLOG_STMT_TYPE_ROW_INJECTION = 0,

    /** The last element of this enumeration type. */
    BINLOG_STMT_TYPE_COUNT
  };

  /**
    Bit field indicating the type of statement.

    There are two groups of bits:

    - The low BINLOG_STMT_UNSAFE_COUNT bits indicate the types of
      unsafeness that the current statement has.

    - The next BINLOG_STMT_TYPE_COUNT bits indicate if the statement
      is of some special type.

    This must be a member of LEX, not of THD: each stored procedure
    needs to remember its unsafeness state between calls and each
    stored procedure has its own LEX object (but no own THD object).
  */
  uint32 binlog_stmt_flags;

  /**
    Bit field that determines the type of tables that are about to be
    be accessed while executing a statement.
  */
  uint32 stmt_accessed_table_flag;

  /**
     It will be set true if 'MATCH () AGAINST' is used in the statement.
  */
  bool using_match;
};

/*
  st_parsing_options contains the flags for constructions that are
  allowed in the current statement.
*/

struct st_parsing_options {
  bool allows_variable;
  bool allows_select_into;

  st_parsing_options() { reset(); }
  void reset();
};

/**
  The state of the lexical parser, when parsing comments.
*/
enum enum_comment_state {
  /**
    Not parsing comments.
  */
  NO_COMMENT,

  /**
    Parsing comments that need to be preserved.
    (Copy '/' '*' and '*' '/' sequences to the preprocessed buffer.)
    Typically, these are user comments '/' '*' ... '*' '/'.
  */
  PRESERVE_COMMENT,

  /**
    Parsing comments that need to be discarded.
    (Don't copy '/' '*' '!' and '*' '/' sequences to the preprocessed buffer.)
    Typically, these are special comments '/' '*' '!' ... '*' '/',
    or '/' '*' '!' 'M' 'M' 'm' 'm' 'm' ... '*' '/', where the comment
    markers should not be expanded.
  */
  DISCARD_COMMENT
};

/**
  This class represents the character input stream consumed during lexical
  analysis.

  In addition to consuming the input stream, this class performs some comment
  pre processing, by filtering out out-of-bound special text from the query
  input stream.

  Two buffers, with pointers inside each, are maintained in parallel. The
  'raw' buffer is the original query text, which may contain out-of-bound
  comments. The 'cpp' (for comments pre processor) is the pre-processed buffer
  that contains only the query text that should be seen once out-of-bound data
  is removed.
*/

class Lex_input_stream {
 public:
  /**
    Constructor

    @param grammar_selector_token_arg   See grammar_selector_token.
  */

  explicit Lex_input_stream(uint grammar_selector_token_arg)
      : grammar_selector_token(grammar_selector_token_arg) {}

  /**
     Object initializer. Must be called before usage.

     @retval false OK
     @retval true  Error
  */
  bool init(THD *thd, const char *buff, size_t length);

  void reset(const char *buff, size_t length);

  /**
    Set the echo mode.

    When echo is true, characters parsed from the raw input stream are
    preserved. When false, characters parsed are silently ignored.
    @param echo the echo mode.
  */
  void set_echo(bool echo) { m_echo = echo; }

  void save_in_comment_state() {
    m_echo_saved = m_echo;
    in_comment_saved = in_comment;
  }

  void restore_in_comment_state() {
    m_echo = m_echo_saved;
    in_comment = in_comment_saved;
  }

  /**
    Skip binary from the input stream.
    @param n number of bytes to accept.
  */
  void skip_binary(int n) {
    DBUG_ASSERT(m_ptr + n <= m_end_of_query);
    if (m_echo) {
      memcpy(m_cpp_ptr, m_ptr, n);
      m_cpp_ptr += n;
    }
    m_ptr += n;
  }

  /**
    Get a character, and advance in the stream.
    @return the next character to parse.
  */
  unsigned char yyGet() {
    DBUG_ASSERT(m_ptr <= m_end_of_query);
    char c = *m_ptr++;
    if (m_echo) *m_cpp_ptr++ = c;
    return c;
  }

  /**
    Get the last character accepted.
    @return the last character accepted.
  */
  unsigned char yyGetLast() const { return m_ptr[-1]; }

  /**
    Look at the next character to parse, but do not accept it.
  */
  unsigned char yyPeek() const {
    DBUG_ASSERT(m_ptr <= m_end_of_query);
    return m_ptr[0];
  }

  /**
    Look ahead at some character to parse.
    @param n offset of the character to look up
  */
  unsigned char yyPeekn(int n) const {
    DBUG_ASSERT(m_ptr + n <= m_end_of_query);
    return m_ptr[n];
  }

  /**
    Cancel the effect of the last yyGet() or yySkip().
    Note that the echo mode should not change between calls to yyGet / yySkip
    and yyUnget. The caller is responsible for ensuring that.
  */
  void yyUnget() {
    m_ptr--;
    if (m_echo) m_cpp_ptr--;
  }

  /**
    Accept a character, by advancing the input stream.
  */
  void yySkip() {
    DBUG_ASSERT(m_ptr <= m_end_of_query);
    if (m_echo)
      *m_cpp_ptr++ = *m_ptr++;
    else
      m_ptr++;
  }

  /**
    Accept multiple characters at once.
    @param n the number of characters to accept.
  */
  void yySkipn(int n) {
    DBUG_ASSERT(m_ptr + n <= m_end_of_query);
    if (m_echo) {
      memcpy(m_cpp_ptr, m_ptr, n);
      m_cpp_ptr += n;
    }
    m_ptr += n;
  }

  /**
    Puts a character back into the stream, canceling
    the effect of the last yyGet() or yySkip().
    Note that the echo mode should not change between calls
    to unput, get, or skip from the stream.
  */
  char *yyUnput(char ch) {
    *--m_ptr = ch;
    if (m_echo) m_cpp_ptr--;
    return m_ptr;
  }

  /**
    Inject a character into the pre-processed stream.

    Note, this function is used to inject a space instead of multi-character
    C-comment. Thus there is no boundary checks here (basically, we replace
    N-chars by 1-char here).
  */
  char *cpp_inject(char ch) {
    *m_cpp_ptr = ch;
    return ++m_cpp_ptr;
  }

  /**
    End of file indicator for the query text to parse.
    @return true if there are no more characters to parse
  */
  bool eof() const { return (m_ptr >= m_end_of_query); }

  /**
    End of file indicator for the query text to parse.
    @param n number of characters expected
    @return true if there are less than n characters to parse
  */
  bool eof(int n) const { return ((m_ptr + n) >= m_end_of_query); }

  /** Get the raw query buffer. */
  const char *get_buf() const { return m_buf; }

  /** Get the pre-processed query buffer. */
  const char *get_cpp_buf() const { return m_cpp_buf; }

  /** Get the end of the raw query buffer. */
  const char *get_end_of_query() const { return m_end_of_query; }

  /** Mark the stream position as the start of a new token. */
  void start_token() {
    m_tok_start = m_ptr;
    m_tok_end = m_ptr;

    m_cpp_tok_start = m_cpp_ptr;
    m_cpp_tok_end = m_cpp_ptr;
  }

  /**
    Adjust the starting position of the current token.
    This is used to compensate for starting whitespace.
  */
  void restart_token() {
    m_tok_start = m_ptr;
    m_cpp_tok_start = m_cpp_ptr;
  }

  /** Get the token start position, in the raw buffer. */
  const char *get_tok_start() const { return m_tok_start; }

  /** Get the token start position, in the pre-processed buffer. */
  const char *get_cpp_tok_start() const { return m_cpp_tok_start; }

  /** Get the token end position, in the raw buffer. */
  const char *get_tok_end() const { return m_tok_end; }

  /** Get the token end position, in the pre-processed buffer. */
  const char *get_cpp_tok_end() const { return m_cpp_tok_end; }

  /** Get the current stream pointer, in the raw buffer. */
  const char *get_ptr() const { return m_ptr; }

  /** Get the current stream pointer, in the pre-processed buffer. */
  const char *get_cpp_ptr() const { return m_cpp_ptr; }

  /** Get the length of the current token, in the raw buffer. */
  uint yyLength() const {
    /*
      The assumption is that the lexical analyser is always 1 character ahead,
      which the -1 account for.
    */
    DBUG_ASSERT(m_ptr > m_tok_start);
    return (uint)((m_ptr - m_tok_start) - 1);
  }

  /** Get the utf8-body string. */
  const char *get_body_utf8_str() const { return m_body_utf8; }

  /** Get the utf8-body length. */
  uint get_body_utf8_length() const {
    return (uint)(m_body_utf8_ptr - m_body_utf8);
  }

  void body_utf8_start(THD *thd, const char *begin_ptr);
  void body_utf8_append(const char *ptr);
  void body_utf8_append(const char *ptr, const char *end_ptr);
  void body_utf8_append_literal(THD *thd, const LEX_STRING *txt,
                                const CHARSET_INFO *txt_cs,
                                const char *end_ptr);

  uint get_lineno(const char *raw_ptr) const;

  /** Current thread. */
  THD *m_thd;

  /** Current line number. */
  uint yylineno;

  /** Length of the last token parsed. */
  uint yytoklen;

  /** Interface with bison, value of the last token parsed. */
  LEX_YYSTYPE yylval;

  /**
    LALR(2) resolution, look ahead token.
    Value of the next token to return, if any,
    or -1, if no token was parsed in advance.
    Note: 0 is a legal token, and represents YYEOF.
  */
  int lookahead_token;

  /** LALR(2) resolution, value of the look ahead token.*/
  LEX_YYSTYPE lookahead_yylval;

  /// Skip adding of the current token's digest since it is already added
  ///
  /// Usually we calculate a digest token by token at the top-level function
  /// of the lexer: MYSQLlex(). However, some complex ("hintable") tokens break
  /// that data flow: for example, the `SELECT /*+ HINT(t) */` is the single
  /// token from the main parser's point of view, and we add the "SELECT"
  /// keyword to the digest buffer right after the lex_one_token() call,
  /// but the "/*+ HINT(t) */" is a sequence of separate tokens from the hint
  /// parser's point of view, and we add those tokens to the digest buffer
  /// *inside* the lex_one_token() call. Thus, the usual data flow adds
  /// tokens from the "/*+ HINT(t) */" string first, and only than it appends
  /// the "SELECT" keyword token to that stream: "/*+ HINT(t) */ SELECT".
  /// This is not acceptable, since we use the digest buffer to restore
  /// query strings in their normalized forms, so the order of added tokens is
  /// important. Thus, we add tokens of "hintable" keywords to a digest buffer
  /// right in the hint parser and skip adding of them at the caller with the
  /// help of skip_digest flag.
  bool skip_digest;

  void add_digest_token(uint token, LEX_YYSTYPE yylval);

  void reduce_digest_token(uint token_left, uint token_right);

  const CHARSET_INFO *query_charset;

 private:
  /** Pointer to the current position in the raw input stream. */
  char *m_ptr;

  /** Starting position of the last token parsed, in the raw buffer. */
  const char *m_tok_start;

  /** Ending position of the previous token parsed, in the raw buffer. */
  const char *m_tok_end;

  /** End of the query text in the input stream, in the raw buffer. */
  const char *m_end_of_query;

  /** Begining of the query text in the input stream, in the raw buffer. */
  const char *m_buf;

  /** Length of the raw buffer. */
  size_t m_buf_length;

  /** Echo the parsed stream to the pre-processed buffer. */
  bool m_echo;
  bool m_echo_saved;

  /** Pre-processed buffer. */
  char *m_cpp_buf;

  /** Pointer to the current position in the pre-processed input stream. */
  char *m_cpp_ptr;

  /**
    Starting position of the last token parsed,
    in the pre-processed buffer.
  */
  const char *m_cpp_tok_start;

  /**
    Ending position of the previous token parsed,
    in the pre-processed buffer.
  */
  const char *m_cpp_tok_end;

  /** UTF8-body buffer created during parsing. */
  char *m_body_utf8;

  /** Pointer to the current position in the UTF8-body buffer. */
  char *m_body_utf8_ptr;

  /**
    Position in the pre-processed buffer. The query from m_cpp_buf to
    m_cpp_utf_processed_ptr is converted to UTF8-body.
  */
  const char *m_cpp_utf8_processed_ptr;

 public:
  /** Current state of the lexical analyser. */
  enum my_lex_states next_state;

  /**
    Position of ';' in the stream, to delimit multiple queries.
    This delimiter is in the raw buffer.
  */
  const char *found_semicolon;

  /** Token character bitmaps, to detect 7bit strings. */
  uchar tok_bitmap;

  /** SQL_MODE = IGNORE_SPACE. */
  bool ignore_space;

  /**
    true if we're parsing a prepared statement: in this mode
    we should allow placeholders.
  */
  bool stmt_prepare_mode;
  /**
    true if we should allow multi-statements.
  */
  bool multi_statements;

  /** State of the lexical analyser for comments. */
  enum_comment_state in_comment;
  enum_comment_state in_comment_saved;

  /**
    Starting position of the TEXT_STRING or IDENT in the pre-processed
    buffer.

    NOTE: this member must be used within MYSQLlex() function only.
  */
  const char *m_cpp_text_start;

  /**
    Ending position of the TEXT_STRING or IDENT in the pre-processed
    buffer.

    NOTE: this member must be used within MYSQLlex() function only.
    */
  const char *m_cpp_text_end;

  /**
    Character set specified by the character-set-introducer.

    NOTE: this member must be used within MYSQLlex() function only.
  */
  const CHARSET_INFO *m_underscore_cs;

  /**
    Current statement digest instrumentation.
  */
  sql_digest_state *m_digest;

  /**
    The synthetic 1st token to prepend token stream with.

    This token value tricks parser to simulate multiple %start-ing points.
    Currently the grammar is aware of 4 such synthetic tokens:
    1. GRAMMAR_SELECTOR_PART for partitioning stuff from DD,
    2. GRAMMAR_SELECTOR_GCOL for generated column stuff from DD,
    3. GRAMMAR_SELECTOR_EXPR for generic single expressions from DD/.frm.
    4. GRAMMAR_SELECTOR_CTE for generic subquery expressions from CTEs.
  */
  const uint grammar_selector_token;

  bool text_string_is_7bit() const { return !(tok_bitmap & 0x80); }
};

class LEX_COLUMN {
 public:
  String column;
  uint rights;
  LEX_COLUMN(const String &x, const uint &y) : column(x), rights(y) {}
};

/* The state of the lex parsing. This is saved in the THD struct */

struct LEX : public Query_tables_list {
  friend bool lex_start(THD *thd);

  SELECT_LEX_UNIT *unit;  ///< Outer-most query expression
  /// @todo: select_lex can be replaced with unit->first-select()
  SELECT_LEX *select_lex;        ///< First query block
  SELECT_LEX *all_selects_list;  ///< List of all query blocks
 private:
  /* current SELECT_LEX in parsing */
  SELECT_LEX *m_current_select;

 public:
  inline SELECT_LEX *current_select() const { return m_current_select; }

  /*
    We want to keep current_thd out of header files, so the debug assert
    is moved to the .cc file.
  */
  void assert_ok_set_current_select();
  inline void set_current_select(SELECT_LEX *select) {
#ifndef DBUG_OFF
    assert_ok_set_current_select();
#endif
    m_current_select = select;
  }
  /// @return true if this is an EXPLAIN statement
  bool is_explain() const { return explain_format != nullptr; }
  LEX_STRING name;
  char *help_arg;
  char *to_log; /* For PURGE MASTER LOGS TO */
  char *x509_subject, *x509_issuer, *ssl_cipher;
  // Widcard from SHOW ... LIKE <wildcard> statements.
  String *wild;
  Query_result *result;
  LEX_STRING binlog_stmt_arg;  ///< Argument of the BINLOG event statement.
  LEX_STRING ident;
  LEX_USER *grant_user;
  LEX_ALTER alter_password;
  THD *thd;
  Generated_column *gcol_info;

  /* Optimizer hints */
  Opt_hints_global *opt_hints_global;

  /* maintain a list of used plugins for this LEX */
  typedef Prealloced_array<plugin_ref, INITIAL_LEX_PLUGIN_LIST_SIZE>
      Plugins_array;
  Plugins_array plugins;

  /// Table being inserted into (may be a view)
  TABLE_LIST *insert_table;
  /// Leaf table being inserted into (always a base table)
  TABLE_LIST *insert_table_leaf;

  /** SELECT of CREATE VIEW statement */
  LEX_STRING create_view_select;

  /* Partition info structure filled in by PARTITION BY parse part */
  partition_info *part_info;

  /*
    The definer of the object being created (view, trigger, stored routine).
    I.e. the value of DEFINER clause.
  */
  LEX_USER *definer;

  List<LEX_USER> users_list;
  List<LEX_COLUMN> columns;
  List<LEX_CSTRING> dynamic_privileges;
  List<LEX_USER> *default_roles;

  ulonglong bulk_insert_row_cnt;

  // PURGE statement-specific fields:
  List<Item> purge_value_list;

  // KILL statement-specific fields:
  List<Item> kill_value_list;

  // other stuff:
  List<set_var_base> var_list;
  List<Item_func_set_user_var> set_var_list;  // in-query assignment list
  /**
    List of placeholders ('?') for parameters of a prepared statement. Because
    we append to this list during parsing, it is naturally sorted by
    position of the '?' in the query string. The code which fills placeholders
    with user-supplied values, and the code which writes a query for
    statement-based logging, rely on this order.
    This list contains only real placeholders, not the clones which originate
    in a re-parsed CTE definition.
  */
  List<Item_param> param_list;

  bool locate_var_assignment(const Name_string &name);

  void insert_values_map(Field *f1, Field *f2) {
    if (!insert_update_values_map)
      insert_update_values_map = new std::map<Field *, Field *>;
    insert_update_values_map->insert(std::make_pair(f1, f2));
  }
  void clear_values_map() {
    if (insert_update_values_map) {
      insert_update_values_map->clear();
      delete insert_update_values_map;
      insert_update_values_map = NULL;
    }
  }
  bool has_values_map() const { return insert_update_values_map != NULL; }
  std::map<Field *, Field *>::iterator begin_values_map() {
    return insert_update_values_map->begin();
  }
  std::map<Field *, Field *>::iterator end_values_map() {
    return insert_update_values_map->end();
  }

 private:
  /*
    With Visual Studio, an std::map will always allocate two small objects
    on the heap. Sometimes we put LEX objects in a MEM_ROOT, and never run
    the LEX DTOR. To avoid memory leaks, put this std::map on the heap,
    and call clear_values_map() at the end of each statement.
   */
  std::map<Field *, Field *> *insert_update_values_map;

 public:
  /*
    A stack of name resolution contexts for the query. This stack is used
    at parse time to set local name resolution contexts for various parts
    of a query. For example, in a JOIN ... ON (some_condition) clause the
    Items in 'some_condition' must be resolved only against the operands
    of the the join, and not against the whole clause. Similarly, Items in
    subqueries should be resolved against the subqueries (and outer queries).
    The stack is used in the following way: when the parser detects that
    all Items in some clause need a local context, it creates a new context
    and pushes it on the stack. All newly created Items always store the
    top-most context in the stack. Once the parser leaves the clause that
    required a local context, the parser pops the top-most context.
  */
  List<Name_resolution_context> context_stack;

  Item_sum *in_sum_func;
  udf_func udf;
  HA_CHECK_OPT check_opt;  // check/repair options
  HA_CREATE_INFO *create_info;
  KEY_CREATE_INFO key_create_info;
  LEX_MASTER_INFO mi;  // used by CHANGE MASTER
  LEX_SLAVE_CONNECTION slave_connection;
  Server_options server_options;
  USER_RESOURCES mqh;
  LEX_RESET_SLAVE reset_slave_info;
  ulong type;
  /**
    This field is used as a work field during resolving to validate
    the use of aggregate functions. For example in a query
    SELECT ... FROM ...WHERE MIN(i) == 1 GROUP BY ... HAVING MIN(i) > 2
    MIN(i) in the WHERE clause is not allowed since only non-aggregated data
    is present, whereas MIN(i) in the HAVING clause is allowed because HAVING
    operates on the output of a grouping operation.
    Each query block is assigned a nesting level. This field is a bit field
    that contains the value one in the position of that nesting level if
    aggregate functions are allowed for that query block.
  */
  nesting_map allow_sum_func;
  /**
    Windowing functions are not allowed in HAVING - in contrast to group
    aggregates - then we need to be stricter than allow_sum_func.
    One bit per query block, as allow_sum_func.
  */
  nesting_map m_deny_window_func;

  Sql_cmd *m_sql_cmd;

  /*
    Usually `expr` rule of yacc is quite reused but some commands better
    not support subqueries which comes standard with this rule, like
    KILL, HA_READ, CREATE/ALTER EVENT etc. Set this to `false` to get
    syntax error back.
  */
  bool expr_allows_subselect;
  /**
    If currently re-parsing a CTE's definition, this is the offset in bytes
    of that definition in the original statement which had the WITH
    clause. Otherwise this is 0.
  */
  uint reparse_common_table_expr_at;

  enum SSL_type ssl_type; /* defined in violite.h */
  enum enum_duplicates duplicates;
  enum enum_tx_isolation tx_isolation;
  enum enum_var_type option_type;
  enum_view_create_mode create_view_mode;

  /// QUERY ID for SHOW PROFILE
  my_thread_id show_profile_query_id;
  uint profile_options;
  uint grant, grant_tot_col;
  bool grant_privilege;
  uint slave_thd_opt, start_transaction_opt;
  int select_number;  ///< Number of query block (by EXPLAIN)
  uint8 create_view_algorithm;
  uint8 create_view_check;
  /**
    @todo ensure that correct CONTEXT_ANALYSIS_ONLY is set for all preparation
          code, so we can fully rely on this field.
  */
  uint8 context_analysis_only;
  bool drop_if_exists;
  bool drop_temporary;
  bool autocommit;
  bool verbose, no_write_to_binlog;
  // For show commands to show hidden columns and indexes.
  bool m_extended_show;

  enum enum_yes_no_unknown tx_chain, tx_release;

  /**
    Whether this query will return the same answer every time, given unchanged
    data. Used to be for the query cache, but is now used to find out if an
    expression is usable for partitioning.
  */
  bool safe_to_cache_query;
  bool subqueries;

 private:
  bool ignore;

 public:
  bool is_ignore() const { return ignore; }
  void set_ignore(bool ignore_param) { ignore = ignore_param; }
  st_parsing_options parsing_options;
  Alter_info *alter_info;
  /* Prepared statements SQL syntax:*/
  LEX_CSTRING prepared_stmt_name; /* Statement name (in all queries) */
  /*
    Prepared statement query text or name of variable that holds the
    prepared statement (in PREPARE ... queries)
  */
  LEX_STRING prepared_stmt_code;
  /* If true, prepared_stmt_code is a name of variable that holds the query */
  bool prepared_stmt_code_is_varref;
  /* Names of user variables holding parameters (in EXECUTE) */
  List<LEX_STRING> prepared_stmt_params;
  sp_head *sphead;
  sp_name *spname;
  bool sp_lex_in_use; /* Keep track on lex usage in SPs for error handling */
  bool all_privileges;
  bool contains_plaintext_password;
  enum_keep_diagnostics keep_diagnostics;
  uint32 next_binlog_file_nr;

 private:
  bool m_broken;  ///< see mark_broken()
  /**
    Set to true when execution has started (after parsing, tables opened and
    query preparation is complete. Used to track arena state for SPs).
  */
  bool m_exec_started;
  /// Current SP parsing context.
  /// @see also sp_head::m_root_parsing_ctx.
  sp_pcontext *sp_current_parsing_ctx;

 public:
  bool is_broken() const { return m_broken; }
  /**
     Certain permanent transformations (like in2exists), if they fail, may
     leave the LEX in an inconsistent state. They should call the
     following function, so that this LEX is not reused by another execution.

     @todo If lex_start () were a member function of LEX, the "broken"
     argument could always be "true" and thus could be removed.
  */
  void mark_broken(bool broken = true) {
    if (broken) {
      /*
        "OPEN <cursor>" cannot be re-prepared if the cursor uses no tables
        ("SELECT FROM DUAL"). Indeed in that case cursor_query is left empty
        in constructions of sp_instr_cpush, and thus
        sp_lex_instr::parse_expr() cannot re-prepare. So we mark the statement
        as broken only if tables are used.
      */
      if (is_metadata_used()) m_broken = true;
    } else
      m_broken = false;
  }

  bool is_exec_started() const { return m_exec_started; }

  void set_exec_started() { m_exec_started = true; }
  void reset_exec_started() { m_exec_started = false; }
  sp_pcontext *get_sp_current_parsing_ctx() { return sp_current_parsing_ctx; }

  void set_sp_current_parsing_ctx(sp_pcontext *ctx) {
    sp_current_parsing_ctx = ctx;
  }

  /// Check if the current statement uses meta-data (uses a table or a stored
  /// routine).
  bool is_metadata_used() const {
    return query_tables != NULL ||
           (sroutines != nullptr && !sroutines->empty());
  }

 public:
  st_sp_chistics sp_chistics;

  Event_parse_data *event_parse_data;

  bool only_view; /* used for SHOW CREATE TABLE/VIEW */
  /*
    view created to be run from definer (standard behaviour)
  */
  uint8 create_view_suid;

  /**
    Intended to point to the next word after DEFINER-clause in the
    following statements:

      - CREATE TRIGGER (points to "TRIGGER");
      - CREATE PROCEDURE (points to "PROCEDURE");
      - CREATE FUNCTION (points to "FUNCTION" or "AGGREGATE");
      - CREATE EVENT (points to "EVENT")

    This pointer is required to add possibly omitted DEFINER-clause to the
    DDL-statement before dumping it to the binlog.
  */
  const char *stmt_definition_begin;
  const char *stmt_definition_end;

  /**
    During name resolution search only in the table list given by
    Name_resolution_context::first_name_resolution_table and
    Name_resolution_context::last_name_resolution_table
    (see Item_field::fix_fields()).
  */
  bool use_only_table_context;

  bool is_lex_started; /* If lex_start() did run. For debugging. */
  /// Set to true while resolving values in ON DUPLICATE KEY UPDATE clause
  bool in_update_value_clause;

  /*
    The set of those tables whose fields are referenced in all subqueries
    of the query.
    TODO: possibly this it is incorrect to have used tables in LEX because
    with subquery, it is not clear what does the field mean. To fix this
    we should aggregate used tables information for selected expressions
    into the select_lex. This map should contain "real" tables only.
  */
  table_map used_tables;

  class Explain_format *explain_format;

  // Maximum execution time for a statement.
  ulong max_execution_time;

  /*
    To flag the current statement as dependent for binary logging
    on explicit_defaults_for_timestamp
  */
  bool binlog_need_explicit_defaults_ts;

  /**
    Used to inform the parser whether it should contextualize the parse
    tree. When we get a pure parser this will not be needed.
  */
  bool will_contextualize;

  LEX();

  virtual ~LEX();

  /// Reset query context to initial state
  void reset();

  /// Create an empty query block within this LEX object.
  SELECT_LEX *new_empty_query_block();

  /// Create query expression object that contains one query block.
  SELECT_LEX *new_query(SELECT_LEX *curr_select);

  /// Create query block and attach it to the current query expression.
  SELECT_LEX *new_union_query(SELECT_LEX *curr_select, bool distinct,
                              bool check_syntax = true);

  /// Create top-level query expression and query block.
  bool new_top_level_query();

  /// Create query expression and query block in existing memory objects.
  void new_static_query(SELECT_LEX_UNIT *sel_unit, SELECT_LEX *select);

  inline bool is_ps_or_view_context_analysis() {
    return (context_analysis_only &
            (CONTEXT_ANALYSIS_ONLY_PREPARE | CONTEXT_ANALYSIS_ONLY_VIEW));
  }

  /**
    Set the current query as uncacheable.

    @param curr_select Current select query block
    @param cause       Why this query is uncacheable.

    @details
    All query blocks representing subqueries, from the current one up to
    the outer-most one, but excluding the main query block, are also set
    as uncacheable.
  */
  void set_uncacheable(SELECT_LEX *curr_select, uint8 cause) {
    safe_to_cache_query = false;

    if (m_current_select == NULL) return;
    SELECT_LEX *sl;
    SELECT_LEX_UNIT *un;
    for (sl = curr_select, un = sl->master_unit(); un != unit;
         sl = sl->outer_select(), un = sl->master_unit()) {
      sl->uncacheable |= cause;
      un->uncacheable |= cause;
    }
  }
  void set_trg_event_type_for_tables();

  TABLE_LIST *unlink_first_table(bool *link_to_local);
  void link_first_table_back(TABLE_LIST *first, bool link_to_local);
  void first_lists_tables_same();

  bool can_use_merged();
  bool can_not_use_merged();
  bool need_correct_ident();
  /*
    Is this update command where 'WHITH CHECK OPTION' clause is important

    SYNOPSIS
      LEX::which_check_option_applicable()

    RETURN
      true   have to take 'WHITH CHECK OPTION' clause into account
      false  'WHITH CHECK OPTION' clause do not need
  */
  inline bool which_check_option_applicable() {
    switch (sql_command) {
      case SQLCOM_UPDATE:
      case SQLCOM_UPDATE_MULTI:
      case SQLCOM_INSERT:
      case SQLCOM_INSERT_SELECT:
      case SQLCOM_REPLACE:
      case SQLCOM_REPLACE_SELECT:
      case SQLCOM_LOAD:
        return true;
      default:
        return false;
    }
  }

  void cleanup_after_one_table_open();

  bool push_context(Name_resolution_context *context) {
    return context_stack.push_front(context);
  }

  void pop_context() { context_stack.pop(); }

  bool copy_db_to(char const **p_db, size_t *p_db_length) const;

  bool copy_db_to(char **p_db, size_t *p_db_length) const {
    return copy_db_to(const_cast<const char **>(p_db), p_db_length);
  }

  Name_resolution_context *current_context() { return context_stack.head(); }
  /*
    Restore the LEX and THD in case of a parse error.
  */
  static void cleanup_lex_after_parse_error(THD *thd);

  void reset_n_backup_query_tables_list(Query_tables_list *backup);
  void restore_backup_query_tables_list(Query_tables_list *backup);

  bool table_or_sp_used();

  /**
    @brief check if the statement is a single-level join
    @return result of the check
      @retval true  The statement doesn't contain subqueries, unions and
                    stored procedure calls.
      @retval false There are subqueries, UNIONs or stored procedure calls.
  */
  bool is_single_level_stmt() {
    /*
      This check exploits the fact that the last added to all_select_list is
      on its top. So select_lex (as the first added) will be at the tail
      of the list.
    */
    if (select_lex == all_selects_list &&
        (sroutines == nullptr || sroutines->empty())) {
      DBUG_ASSERT(!all_selects_list->next_select_in_list());
      return true;
    }
    return false;
  }

  /**
    IS schema queries read some dynamic table statistics from SE.
    These statistics are cached, to avoid opening of table more
    than once while preparing a single output record buffer.
  */
  dd::info_schema::Table_statistics m_IS_table_stats;
  dd::info_schema::Tablespace_statistics m_IS_tablespace_stats;

  bool accept(Select_lex_visitor *visitor);

  bool set_wild(LEX_STRING);
  void clear_privileges();

  bool make_sql_cmd(Parse_tree_root *parse_tree);
};

/**
  The internal state of the syntax parser.
  This object is only available during parsing,
  and is private to the syntax parser implementation (sql_yacc.yy).
*/
class Yacc_state {
 public:
  Yacc_state() { reset(); }

  void reset() {
    yacc_yyss = NULL;
    yacc_yyvs = NULL;
    yacc_yyls = NULL;
    m_lock_type = TL_READ_DEFAULT;
    m_mdl_type = MDL_SHARED_READ;
  }

  ~Yacc_state();

  /**
    Reset part of the state which needs resetting before parsing
    substatement.
  */
  void reset_before_substatement() {
    m_lock_type = TL_READ_DEFAULT;
    m_mdl_type = MDL_SHARED_READ;
  }

  /**
    Bison internal state stack, yyss, when dynamically allocated using
    my_yyoverflow().
  */
  uchar *yacc_yyss;

  /**
    Bison internal semantic value stack, yyvs, when dynamically allocated using
    my_yyoverflow().
  */
  uchar *yacc_yyvs;

  /**
    Bison internal location value stack, yyls, when dynamically allocated using
    my_yyoverflow().
  */
  uchar *yacc_yyls;

  /**
    Type of lock to be used for tables being added to the statement's
    table list in table_factor, table_alias_ref, single_multi and
    table_wild_one rules.
    Statements which use these rules but require lock type different
    from one specified by this member have to override it by using
    SELECT_LEX::set_lock_for_tables() method.

    The default value of this member is TL_READ_DEFAULT. The only two
    cases in which we change it are:
    - When parsing SELECT HIGH_PRIORITY.
    - Rule for DELETE. In which we use this member to pass information
      about type of lock from delete to single_multi part of rule.

    We should try to avoid introducing new use cases as we would like
    to get rid of this member eventually.
  */
  thr_lock_type m_lock_type;

  /**
    The type of requested metadata lock for tables added to
    the statement table list.
  */
  enum_mdl_type m_mdl_type;

  /*
    TODO: move more attributes from the LEX structure here.
  */
};

/**
  Input parameters to the parser.
*/
struct Parser_input {
  bool m_compute_digest;

  Parser_input() : m_compute_digest(false) {}
};

/**
  Internal state of the parser.
  The complete state consist of:
  - input parameters that control the parser behavior
  - state data used during lexical parsing,
  - state data used during syntactic parsing.
*/
class Parser_state {
 protected:
  /**
    Constructor for special parsers of partial SQL clauses (DD)

    @param grammar_selector_token   See Lex_input_stream::grammar_selector_token
  */
  explicit Parser_state(uint grammar_selector_token)
      : m_input(), m_lip(grammar_selector_token), m_yacc(), m_comment(false) {}

 public:
  Parser_state() : m_input(), m_lip(-1), m_yacc(), m_comment(false) {}

  /**
     Object initializer. Must be called before usage.

     @retval false OK
     @retval true  Error
  */
  bool init(THD *thd, const char *buff, size_t length) {
    return m_lip.init(thd, buff, length);
  }

  void reset(const char *found_semicolon, size_t length) {
    m_lip.reset(found_semicolon, length);
    m_yacc.reset();
  }

  /// Signal that the current query has a comment
  void add_comment() { m_comment = true; }
  /// Check whether the current query has a comment
  bool has_comment() const { return m_comment; }

 public:
  Parser_input m_input;
  Lex_input_stream m_lip;
  Yacc_state m_yacc;
  /**
    Current performance digest instrumentation.
  */
  PSI_digest_locker *m_digest_psi;

 private:
  bool m_comment;  ///< True if current query contains comments
};

/**
  Parser state for partition expression parser (.frm/DD stuff)
*/
class Partition_expr_parser_state : public Parser_state {
 public:
  Partition_expr_parser_state();

  partition_info *result;
};

/**
  Parser state for generated column expression parser (.frm/DD stuff)
*/
class Gcol_expr_parser_state : public Parser_state {
 public:
  Gcol_expr_parser_state();

  Generated_column *result;
};

/**
  Parser state for single expression parser (.frm/DD stuff)
*/
class Expression_parser_state : public Parser_state {
 public:
  Expression_parser_state();

  Item *result;
};

/**
  Parser state for CTE subquery parser
*/
class Common_table_expr_parser_state : public Parser_state {
 public:
  Common_table_expr_parser_state();

  PT_subquery *result;
};

extern sql_digest_state *digest_add_token(sql_digest_state *state, uint token,
                                          LEX_YYSTYPE yylval);

extern sql_digest_state *digest_reduce_token(sql_digest_state *state,
                                             uint token_left, uint token_right);

struct st_lex_local : public LEX {
  static void *operator new(size_t size) throw() { return sql_alloc(size); }
  static void *operator new(
      size_t size, MEM_ROOT *mem_root,
      const std::nothrow_t &arg MY_ATTRIBUTE((unused)) = std::nothrow) throw() {
    return alloc_root(mem_root, size);
  }
  static void operator delete(void *ptr MY_ATTRIBUTE((unused)),
                              size_t size MY_ATTRIBUTE((unused))) {
    TRASH(ptr, size);
  }
  static void operator delete(
      void *, MEM_ROOT *, const std::nothrow_t &)throw() { /* Never called */
  }
};

extern bool lex_init(void);
extern void lex_free(void);
extern bool lex_start(THD *thd);
extern void lex_end(LEX *lex);
extern int MYSQLlex(union YYSTYPE *yylval, struct YYLTYPE *yylloc,
                    class THD *thd);

extern void trim_whitespace(const CHARSET_INFO *cs, LEX_STRING *str);

extern bool is_lex_native_function(const LEX_STRING *name);

bool is_keyword(const char *name, size_t len);
bool db_is_default_db(const char *db, size_t db_len, const THD *thd);

bool check_select_for_locking_clause(THD *);

void print_derived_column_names(THD *thd, String *str,
                                const Create_col_name_list *column_names);

/**
  @} (End of group GROUP_PARSER)
*/

/**
   Check if the given string is invalid using the system charset.

   @param string_val       Reference to the string.
   @param charset_info     Pointer to charset info.

   @return true if the string has an invalid encoding using
                the system charset else false.
*/

inline bool is_invalid_string(const LEX_CSTRING &string_val,
                              const CHARSET_INFO *charset_info) {
  size_t valid_len;
  bool len_error;

  if (validate_string(charset_info, string_val.str,
                      static_cast<uint32>(string_val.length), &valid_len,
                      &len_error)) {
    char hexbuf[7];
    octet2hex(
        hexbuf, string_val.str + valid_len,
        static_cast<uint>(std::min<size_t>(string_val.length - valid_len, 3)));
    my_error(ER_INVALID_CHARACTER_STRING, MYF(0), charset_info->csname, hexbuf);
    return true;
  }
  return false;
}

#endif /* SQL_LEX_INCLUDED */
