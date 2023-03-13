/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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
#include <sys/types.h>  // TODO: replace with cstdint

#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "map_helpers.h"
#include "mem_root_deque.h"
#include "memory_debugging.h"
#include "my_alloc.h"  // Destroy_only
#include "my_base.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"  // TODO: replace with cstdint
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "my_thread_local.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/service_mysql_alloc.h"  // my_free
#include "mysql_com.h"
#include "mysqld_error.h"
#include "prealloced_array.h"                // Prealloced_array
#include "sql/dd/info_schema/table_stats.h"  // dd::info_schema::Table_stati...
#include "sql/dd/info_schema/tablespace_stats.h"  // dd::info_schema::Tablesp...
#include "sql/enum_query_type.h"
#include "sql/handler.h"
#include "sql/item.h"            // Name_resolution_context
#include "sql/item_subselect.h"  // Subquery_strategy
#include "sql/iterators/row_iterator.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/key_spec.h"  // KEY_CREATE_INFO
#include "sql/mdl.h"
#include "sql/mem_root_array.h"        // Mem_root_array
#include "sql/parse_tree_node_base.h"  // enum_parsing_context
#include "sql/parser_yystype.h"
#include "sql/query_options.h"  // OPTION_NO_CONST_TABLES
#include "sql/query_term.h"
#include "sql/set_var.h"
#include "sql/sql_array.h"
#include "sql/sql_connect.h"  // USER_RESOURCES
#include "sql/sql_const.h"
#include "sql/sql_data_change.h"  // enum_duplicates
#include "sql/sql_error.h"        // warn_on_deprecated_charset
#include "sql/sql_list.h"
#include "sql/sql_plugin_ref.h"
#include "sql/sql_servers.h"  // Server_options
#include "sql/sql_udf.h"      // Item_udftype
#include "sql/table.h"        // Table_ref
#include "sql/thr_malloc.h"
#include "sql/trigger_def.h"  // enum_trigger_action_time_type
#include "sql/visible_fields.h"
#include "sql_chars.h"
#include "sql_string.h"
#include "thr_lock.h"  // thr_lock_type
#include "violite.h"   // SSL_type

class Alter_info;
class Event_parse_data;
class Field;
class Item_cond;
class Item_func_get_system_var;
class Item_func_match;
class Item_func_set_user_var;
class Item_rollup_group_item;
class Item_rollup_sum_switcher;
class Item_sum;
class JOIN;
class Opt_hints_global;
class Opt_hints_qb;
class PT_subquery;
class PT_with_clause;
class Parse_tree_root;
class Protocol;
class Query_result;
class Query_result_interceptor;
class Query_result_union;
class Query_block;
class Query_expression;
class Select_lex_visitor;
class Sql_cmd;
class THD;
class Value_generator;
class Window;
class partition_info;
class sp_head;
class sp_name;
class sp_pcontext;
struct LEX;
struct NESTED_JOIN;
struct PSI_digest_locker;
struct sql_digest_state;
union Lexer_yystype;
struct Lifted_fields_map;

const size_t INITIAL_LEX_PLUGIN_LIST_SIZE = 16;
constexpr const int MAX_SELECT_NESTING{sizeof(nesting_map) * 8 - 1};

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
enum class enum_sp_type {
  FUNCTION = 1,
  PROCEDURE,
  TRIGGER,
  EVENT,
  /*
    Must always be the last one.
    Denotes an error condition.
  */
  INVALID_SP_TYPE
};

inline enum_sp_type to_sp_type(longlong val) {
  if (val >= static_cast<longlong>(enum_sp_type::FUNCTION) &&
      val < static_cast<longlong>(enum_sp_type::INVALID_SP_TYPE))
    return static_cast<enum_sp_type>(val);
  else
    return enum_sp_type::INVALID_SP_TYPE;
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

const LEX_CSTRING sp_data_access_name[] = {
    {STRING_WITH_LEN("")},
    {STRING_WITH_LEN("CONTAINS SQL")},
    {STRING_WITH_LEN("NO SQL")},
    {STRING_WITH_LEN("READS SQL DATA")},
    {STRING_WITH_LEN("MODIFIES SQL DATA")}};

enum class enum_view_create_mode {
  VIEW_CREATE_NEW,        // check that there are not such VIEW/table
  VIEW_ALTER,             // check that VIEW with such name exists
  VIEW_CREATE_OR_REPLACE  // check only that there are not such table
};

enum class enum_alter_user_attribute {
  ALTER_USER_COMMENT_NOT_USED,  // No user metadata ALTER in the AST
  ALTER_USER_COMMENT,           // A text comment is expected
  ALTER_USER_ATTRIBUTE          // A JSON object is expected
};

/* Options to add_table_to_list() */
#define TL_OPTION_UPDATING 0x01
#define TL_OPTION_IGNORE_LEAVES 0x02
#define TL_OPTION_ALIAS 0x04

/* Structure for db & table in sql_yacc */
class Table_function;

class Table_ident {
 public:
  LEX_CSTRING db;
  LEX_CSTRING table;
  Query_expression *sel;
  Table_function *table_function;

  Table_ident(Protocol *protocol, const LEX_CSTRING &db_arg,
              const LEX_CSTRING &table_arg, bool force);
  Table_ident(const LEX_CSTRING &db_arg, const LEX_CSTRING &table_arg)
      : db(db_arg), table(table_arg), sel(nullptr), table_function(nullptr) {}
  Table_ident(const LEX_CSTRING &table_arg)
      : table(table_arg), sel(nullptr), table_function(nullptr) {
    db = NULL_CSTR;
  }
  /**
    This constructor is used only for the case when we create a derived
    table. A derived table has no name and doesn't belong to any database.
    Later, if there was an alias specified for the table, it will be set
    by add_table_to_list.
  */
  Table_ident(Query_expression *s) : sel(s), table_function(nullptr) {
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
      : table(table_arg), sel(nullptr), table_function(table_func_arg) {
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

using List_item = mem_root_deque<Item *>;
using Group_list_ptrs = Mem_root_array<ORDER *>;

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
  char *host, *user, *password, *log_file_name, *bind_addr, *network_namespace;
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
      retry_count_opt, auto_position, port_opt, get_public_key,
      m_source_connection_auto_failover, m_gtid_only;
  char *ssl_key, *ssl_cert, *ssl_ca, *ssl_capath, *ssl_cipher;
  char *ssl_crl, *ssl_crlpath, *tls_version;
  /*
    Ciphersuites used for TLS 1.3 communication with the master server.
  */
  enum enum_tls_ciphersuites {
    UNSPECIFIED = 0,
    SPECIFIED_NULL,
    SPECIFIED_STRING
  };
  enum enum_tls_ciphersuites tls_ciphersuites;
  char *tls_ciphersuites_string;
  char *public_key_path;
  char *relay_log_name;
  ulong relay_log_pos;
  char *compression_algorithm;
  uint zstd_compression_level;
  Prealloced_array<ulong, 2> repl_ignore_server_ids;
  /**
    Flag that is set to `true` whenever `PRIVILEGE_CHECKS_USER` is set to `NULL`
    as a part of a `CHANGE MASTER TO` statement.
   */
  bool privilege_checks_none;
  /**
    Username and hostname parts of the `PRIVILEGE_CHECKS_USER`, when it's set to
    a user.
   */
  const char *privilege_checks_username, *privilege_checks_hostname;
  /**
    Flag indicating if row format should be enforced for this channel event
    stream.
   */
  int require_row_format;

  /**
    Identifies what is the slave policy on primary keys in tables.
    If set to STREAM it just replicates the value of sql_require_primary_key.
    If set to ON it fails when the source tries to replicate a table creation
    or alter operation that does not have a primary key.
    If set to OFF it does not enforce any policies on the channel for primary
    keys.
  */
  enum {
    LEX_MI_PK_CHECK_UNCHANGED = 0,
    LEX_MI_PK_CHECK_STREAM = 1,
    LEX_MI_PK_CHECK_ON = 2,
    LEX_MI_PK_CHECK_OFF = 3,
    LEX_MI_PK_CHECK_GENERATE = 4
  } require_table_primary_key_check;

  enum {
    LEX_MI_ANONYMOUS_TO_GTID_UNCHANGED = 0,
    LEX_MI_ANONYMOUS_TO_GTID_OFF,
    LEX_MI_ANONYMOUS_TO_GTID_LOCAL,
    LEX_MI_ANONYMOUS_TO_GTID_UUID
  } assign_gtids_to_anonymous_transactions_type;

  const char *assign_gtids_to_anonymous_transactions_manual_uuid{nullptr};

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
  GLOBAL_OPTIONS_TYPE,
  DERIVED_TABLE_TYPE
};

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

  void print(const THD *thd, String *str);
};

/*
  Class Query_expression represents a query expression.
  Class Query_block represents a query block.

  In addition to what is explained below, the query block(s) of a query
  expression is contained in a tree expressing the nesting of set operations,
  cf.  query_term.h

   A query expression contains one or more query blocks (more than one means
   that the query expression contains one or more set operations - UNION,
   INTERSECT or EXCEPT - unless the query blocks are used to describe
   subqueries).  These classes are connected as follows: both classes have a
   master, a slave, a next and a prev field.  For class Query_block, master and
   slave connect to objects of type Query_expression, whereas for class
   Query_expression, they connect to Query_block.  master is pointer to outer
   node.  slave is pointer to the first inner node.

   neighbors are two Query_block or Query_expression objects on
   the same level.

   The structures are linked with the following pointers:
   - list of neighbors (next/prev) (prev of first element point to slave
     pointer of outer structure)
     - For Query_block, this is a list of query blocks.
     - For Query_expression, this is a list of subqueries.

   - pointer to outer node (master), which is
     If this is Query_expression
       - pointer to outer query_block.
     If this is Query_block
       - pointer to outer Query_expression.

   - pointer to inner objects (slave), which is either:
     If this is an Query_expression:
       - first query block that belong to this query expression.
     If this is an Query_block
       - first query expression that belong to this query block (subqueries).

   - list of all Query_block objects (link_next/link_prev)
     This is to be used for things like derived tables creation, where we
     go through this list and create the derived tables.

   In addition to the above mentioned link, the query's tree structure is
   represented by the member m_query_term, see query_term.h
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
     select1 select2 select3
     |^^     |^
    s|||     ||master
    l|||     |+---------------------------------+
    a|||     +---------------------------------+|
    v|||master                         slave   ||
    e||+-------------------------+             ||
     V|            neighbor      |             V|
     unit1.1<+==================>unit1.2       unit2.1
     select1.1.1 select 1.1.2    select1.2.1   select2.1.1
                                               |^
                                               ||
                                               V|
                                               unit2.1.1.1
                                               select2.1.1.1.1


   relation in main unit will be following:
   (bigger picture for:
      main unit
      select1 select2 select3
   in the above picture)

         main unit
         |^^^
         ||||
         ||||
         |||+------------------------------+
         ||+--------------+                |
    slave||master         |                |
         V|      neighbor |       neighbor |
         select1<========>select2<========>select3

    list of all query_block will be following (as it will be constructed by
    parser):

    select1->select2->select3->select2.1.1->select 2.1.2->select2.1.1.1.1-+
                                                                          |
    +---------------------------------------------------------------------+
    |
    +->select1.1.1->select1.1.2

*/

/**
  This class represents a query expression (one query block or
  several query blocks combined with UNION).
*/
class Query_expression {
  /**
    Intrusive double-linked list of all query expressions
    immediately contained within the same query block.
  */
  Query_expression *next;
  Query_expression **prev;

  /**
    The query block wherein this query expression is contained,
    NULL if the query block is the outer-most one.
  */
  Query_block *master;
  /// The first query block in this query expression.
  Query_block *slave;

  // The query set operation structure, see doc for Query_term.
  Query_term *m_query_term{nullptr};

 public:
  /// Getter for m_query_term, q.v.
  Query_term *query_term() const { return m_query_term; }
  /// Setter for m_query_term, q.v.
  void set_query_term(Query_term *qt) { m_query_term = qt; }
  /// Convenience method to avoid down casting, i.e. interpret m_query_term
  /// as a Query_term_set_op.
  /// @retval a non-null node iff !is_simple
  /// @retval nullptr if is_simple() holds.
  Query_term_set_op *set_operation() const {
    return is_simple() ? nullptr : down_cast<Query_term_set_op *>(m_query_term);
  }
  /// Return the query block iff !is_simple() holds
  Query_block *non_simple_result_query_block() const {
    if (is_simple())
      return nullptr;
    else
      return m_query_term->query_block();
  }
  bool is_leaf_block(Query_block *qb);
  Query_term *find_blocks_query_term(const Query_block *qb) const {
    for (auto qt : query_terms<>()) {
      if (qt->query_block() == qb) return qt;
    }
    return nullptr;
  }

  /**
    Return iterator object over query terms rooted in m_query_term,
    using either post order visiting (default) or pre order,
    optionally skipping leaf nodes (query blocks corresponding to SELECTs or
    table constructors). By default, we visit all nodes.
    Usage:  for (auto qt : query_terms<..>() { ... }
    E.g.
          for (auto qt : query_terms<>()) { } Visit all nodes, post order
          for (auto qt : query_terms<QTC_PRE_ORDER, false>()) { }
                                              Skip leaves, pre order
    @tparam order == QTC_POST_ORDER if post order traversal is desired;default
                  == QTC_PRE_ORDER  pre-order traversal
    @tparam visit_leaves == VL_VISIT_LEAVES: if we want the traversal to include
                            leaf nodes i.e. the SELECTs or table constructors
                         == VL_SKIP_LEAVES: leaves will be skipped
    @returns iterator object
  */
  template <Visit_order order = QTC_POST_ORDER,
            Visit_leaves visit_leaves = VL_VISIT_LEAVES>
  Query_terms<order, visit_leaves> query_terms() const {
    return Query_terms<order, visit_leaves>(m_query_term);
  }

  /**
    Return the Query_block of the last query term in a n-ary set
    operation that is the right side of the last DISTINCT set operation in that
    n_ary set operation:
    E.e. for
        A UNION B UNION ALL C,
    B's block will be returned. If no DISTINCT is present or not a set
    operation, return nullptr.

    @returns query block of last distinct right operand
  */
  Query_block *last_distinct() const {
    auto const setop = down_cast<Query_term_set_op *>(m_query_term);
    if (setop->m_last_distinct > 0)
      return setop->m_children[setop->m_last_distinct]->query_block();
    else
      return nullptr;
  }

  bool has_top_level_distinct() const {
    if (is_simple()) return false;
    return down_cast<Query_term_set_op *>(m_query_term)->m_last_distinct > 0;
  }

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

  /// Object to which the result for this query expression is sent.
  /// Not used if we materialize directly into a parent query expression's
  /// result table (see optimize()).
  Query_result *m_query_result;

  /**
    An iterator you can read from to get all records for this query.

    May be nullptr even after create_access_paths(), or in the case of an
    unfinished materialization (see optimize()).
   */
  unique_ptr_destroy_only<RowIterator> m_root_iterator;
  AccessPath *m_root_access_path = nullptr;

  /**
    If there is an unfinished materialization (see optimize()),
    contains one element for each query block in this query expression.
   */
  Mem_root_array<MaterializePathParameters::QueryBlock>
      m_query_blocks_to_materialize;

 private:
  /**
    Convert the executor structures to a set of access paths, storing the result
    in m_root_access_path.
   */
  void create_access_paths(THD *thd);

 public:
  /**
    result of this query can't be cached, bit field, can be :
      UNCACHEABLE_DEPENDENT
      UNCACHEABLE_RAND
      UNCACHEABLE_SIDEEFFECT
  */
  uint8 uncacheable;

  explicit Query_expression(enum_parsing_context parsing_context);

  /// @return true for a query expression without UNION/INTERSECT/EXCEPT or
  /// multi-level ORDER, i.e. we have a "simple table".
  bool is_simple() const { return m_query_term->term_type() == QT_QUERY_BLOCK; }

  /// Values for Query_expression::cleaned
  enum enum_clean_state {
    UC_DIRTY,       ///< Unit isn't cleaned
    UC_PART_CLEAN,  ///< Unit were cleaned, except JOIN and JOIN_TABs were
                    ///< kept for possible EXPLAIN
    UC_CLEAN        ///< Unit completely cleaned, all underlying JOINs were
                    ///< freed
  };
  enum_clean_state cleaned;  ///< cleanliness state

 private:
  /*
    list of types of items inside union (used for union & derived tables)

    Item_type_holders from which this list consist may have pointers to Field,
    pointers is valid only after preparing SELECTS of this unit and before
    any SELECT of this unit execution

    All hidden items are stripped away from this list.
  */
  mem_root_deque<Item *> types;

 public:
  /**
    Return the query block holding the top level  ORDER BY, LIMIT and OFFSET.

    If the query is not a set operation (UNION, INTERSECT or EXCEPT, and the
    query expression has no multi-level ORDER BY/LIMIT, this represents the
    single query block of the query itself, cf. documentation for class
    Query_term.

    @return query block containing the global parameters
  */
  inline Query_block *global_parameters() const {
    return query_term()->query_block();
  }

  /* LIMIT clause runtime counters */
  ha_rows select_limit_cnt, offset_limit_cnt;
  /// Points to subquery if this query expression is used in one, otherwise NULL
  Item_subselect *item;
  /**
    The WITH clause which is the first part of this query expression. NULL if
    none.
  */
  PT_with_clause *m_with_clause;
  /**
    If this query expression is underlying of a derived table, the derived
    table. NULL if none.
  */
  Table_ref *derived_table;
  /**
     First query block (in this UNION) which references the CTE.
     NULL if not the query expression of a recursive CTE.
  */
  Query_block *first_recursive;

  /**
    If 'this' is body of lateral derived table:
    map of tables in the same FROM clause as this derived table, and to which
    the derived table's body makes references.
    In pre-resolution stages, this is OUTER_REF_TABLE_BIT, just to indicate
    that this has LATERAL; after resolution, which has found references in the
    body, this is the proper map (with no PSEUDO_TABLE_BITS anymore).
  */
  table_map m_lateral_deps;

  /**
    This query expression represents a scalar subquery and we need a run-time
    check that the cardinality doesn't exceed 1.
  */
  bool m_reject_multiple_rows{false};

  /// @return true if query expression can be merged into an outer query
  bool is_mergeable() const;

  /// @return true if query expression is recommended to be merged
  bool merge_heuristic(const LEX *lex) const;

  /// @return the query block this query expression belongs to as subquery
  Query_block *outer_query_block() const { return master; }

  /// @return the first query block inside this query expression
  Query_block *first_query_block() const { return slave; }

  /// @return the next query expression within same query block (next subquery)
  Query_expression *next_query_expression() const { return next; }

  /// @return the query result object in use for this query expression
  Query_result *query_result() const { return m_query_result; }

  RowIterator *root_iterator() const { return m_root_iterator.get(); }
  unique_ptr_destroy_only<RowIterator> release_root_iterator() {
    return std::move(m_root_iterator);
  }
  AccessPath *root_access_path() const { return m_root_access_path; }

  // Asks each query block to switch to an access path with in2exists
  // conditions removed (if they were ever added).
  // See JOIN::change_to_access_path_without_in2exists().
  void change_to_access_path_without_in2exists(THD *thd);

  void clear_root_access_path() {
    m_root_access_path = nullptr;
    m_root_iterator.reset();
  }

  /**
    Ensures that there are iterators created for the access paths created
    by optimize(), even if it was called with create_access_paths = false.
    If there are already iterators, it is a no-op. optimize() must have
    been called earlier.

    The use case for this is if we have a query block that's not top-level,
    but we figure out after the fact that we wanted to run it anyway.
    The typical case would be that we notice that the query block can return
    at most one row (a so-called const table), and want to run it during
    optimization.
   */
  bool force_create_iterators(THD *thd);

  /// See optimize().
  bool unfinished_materialization() const {
    return !m_query_blocks_to_materialize.empty();
  }

  /// See optimize().
  Mem_root_array<MaterializePathParameters::QueryBlock>
  release_query_blocks_to_materialize() {
    return std::move(m_query_blocks_to_materialize);
  }

  /// Set new query result object for this query expression
  void set_query_result(Query_result *res) { m_query_result = res; }

  /**
    Whether there is a chance that optimize() is capable of materializing
    directly into a result table if given one. Note that even if this function
    returns true, optimize() can choose later not to do so, since it depends
    on information (in particular, whether the query blocks can run under
    the iterator executor or not) that is not available before optimize time.

    TODO(sgunders): Now that all query blocks can run under the iterator
    executor, the above may no longer be true. This needs investigation.
   */
  bool can_materialize_directly_into_result() const;

  bool prepare(THD *thd, Query_result *result,
               mem_root_deque<Item *> *insert_field_list,
               ulonglong added_options, ulonglong removed_options);

  /**
    If and only if materialize_destination is non-nullptr, it means that the
    caller intends to materialize our result into the given table. If it is
    advantageous (in particular, if this query expression is a UNION DISTINCT),
    optimize() will not create an iterator by itself, but rather do an
    unfinished materialize. This means that it will collect iterators for
    all the query blocks and prepare them for materializing into the given
    table, but not actually create a root iterator for this query expression;
    the caller is responsible for calling release_query_blocks_to_materialize()
    and creating the iterator itself.

    Even if materialize_destination is non-nullptr, this function may choose
    to make a regular iterator. The caller is responsible for checking
    unfinished_materialization() if it has given a non-nullptr table.

    @param thd Thread handle.

    @param materialize_destination What table to try to materialize into,
      or nullptr if the caller does not intend to materialize the result.

    @param create_iterators If false, only access paths are created,
      not iterators. Only top level query blocks (these that we are to call
      exec() on) should have iterators. See also force_create_iterators().

    @param finalize_access_paths Relevant for the hypergraph optimizer only.
      If false, the given access paths will _not_ be finalized, so you cannot
      create iterators from it before finalize() is called (see
      FinalizePlanForQueryBlock()), and create_iterators must also be false.
      This is relevant only if you are potentially optimizing multiple times
      (see change_to_access_path_without_in2exists()), since you are only
      allowed to finalize a query block once. "Fake" query blocks (see
      query_term.h) are always finalized.
   */
  bool optimize(THD *thd, TABLE *materialize_destination, bool create_iterators,
                bool finalize_access_paths);

  /**
    For any non-finalized query block, finalize it so that we are allowed to
    create iterators. Must be called after the final access path is chosen
    (ie., after any calls to change_to_access_path_without_in2exists()).
   */
  bool finalize(THD *thd);

  /**
    Do everything that would be needed before running Init() on the root
    iterator. In particular, clear out data from previous execution iterations,
    if needed.
   */
  bool ClearForExecution();

  bool ExecuteIteratorQuery(THD *thd);
  bool execute(THD *thd);
  bool explain(THD *explain_thd, const THD *query_thd);
  bool explain_query_term(THD *explain_thd, const THD *query_thd,
                          Query_term *qt);
  void cleanup(bool full);
  /**
    Destroy contained objects, in particular temporary tables which may
    have their own mem_roots.
  */
  void destroy();

  void print(const THD *thd, String *str, enum_query_type query_type);
  bool accept(Select_lex_visitor *visitor);

  /**
    Create a block to be used for ORDERING and LIMIT/OFFSET processing of a
    query term, which isn't itself a query specification or table value
    constructor. Such blocks are not included in the list starting in
    Query_Expression::first_query_block, and Query_block::next_query_block().
    They blocks are accessed via Query_term::query_block().

    @param term the term on behalf of which we are making a post processing
                block
    @returns a query block
   */
  Query_block *create_post_processing_block(Query_term_set_op *term);

  bool prepare_query_term(THD *thd, Query_term *qts,
                          Query_result *common_result, ulonglong added_options,
                          ulonglong create_options, int level,
                          Mem_root_array<bool> &nullable);
  void set_prepared() {
    assert(!is_prepared());
    prepared = true;
  }
  void set_optimized() {
    assert(is_prepared() && !is_optimized());
    optimized = true;
  }
  void set_executed() {
    // assert(is_prepared() && is_optimized() && !is_executed());
    assert(is_prepared() && is_optimized());
    executed = true;
  }
  /// Reset this query expression for repeated evaluation within same execution
  void reset_executed() {
    assert(is_prepared() && is_optimized());
    executed = false;
  }
  /// Clear execution state, needed before new execution of prepared statement
  void clear_execution() {
    // Cannot be enforced when called from Prepared_statement::execute():
    // assert(is_prepared());
    optimized = false;
    executed = false;
    cleaned = UC_DIRTY;
  }
  /// Check state of preparation of the contained query expression.
  bool is_prepared() const { return prepared; }
  /// Check state of optimization of the contained query expression.
  bool is_optimized() const { return optimized; }
  /**
    Check state of execution of the contained query expression.
    Should not be used to check the state of a complete statement, use
    LEX::is_exec_completed() instead.
  */
  bool is_executed() const { return executed; }
  bool change_query_result(THD *thd, Query_result_interceptor *result,
                           Query_result_interceptor *old_result);
  bool set_limit(THD *thd, Query_block *provider);
  bool has_any_limit() const;

  inline bool is_union() const;
  inline bool is_set_operation() const;

  /// Include a query expression below a query block.
  void include_down(LEX *lex, Query_block *outer);

  /// Exclude this unit and immediately contained query_block objects
  void exclude_level();

  /// Exclude subtree of current unit from tree of SELECTs
  void exclude_tree();

  /// Renumber query blocks of a query expression according to supplied LEX
  void renumber_selects(LEX *lex);

  void restore_cmd_properties();
  bool save_cmd_properties(THD *thd);

  friend class Query_block;

  mem_root_deque<Item *> *get_unit_column_types();
  mem_root_deque<Item *> *get_field_list();
  size_t num_visible_fields() const;

  // If we are doing a query with global LIMIT, we need somewhere to store the
  // record count for FOUND_ROWS().  It can't be in any of the JOINs, since
  // they may have their own LimitOffsetIterators, which will write to
  // join->send_records whenever there is an OFFSET. Thus, we'll keep it here
  // instead.
  ha_rows send_records;

  enum_parsing_context get_explain_marker(const THD *thd) const;
  void set_explain_marker(THD *thd, enum_parsing_context m);
  void set_explain_marker_from(THD *thd, const Query_expression *u);

#ifndef NDEBUG
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

  bool clear_correlated_query_blocks();

  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block);

  /**
    If unit is a subquery, which forms an object of the upper level (an
    Item_subselect, a derived Table_ref), adds to this object a map
    of tables of the upper level which the unit references.
  */
  void accumulate_used_tables(table_map map);

  /**
    If unit is a subquery, which forms an object of the upper level (an
    Item_subselect, a derived Table_ref), returns the place of this object
    in the upper level query block.
  */
  enum_parsing_context place() const;

  bool walk(Item_processor processor, enum_walk walk, uchar *arg);

  /*
    An exception: this is the only function that needs to adjust
    explain_marker.
  */
  friend bool parse_view_definition(THD *thd, Table_ref *view_ref);
};

typedef Bounds_checked_array<Item *> Ref_item_array;
class Semijoin_decorrelation;

/**
  Query_block type enum
*/
enum class enum_explain_type {
  EXPLAIN_NONE = 0,
  EXPLAIN_PRIMARY,
  EXPLAIN_SIMPLE,
  EXPLAIN_DERIVED,
  EXPLAIN_SUBQUERY,
  EXPLAIN_UNION,
  EXPLAIN_INTERSECT,
  EXPLAIN_EXCEPT,
  EXPLAIN_UNION_RESULT,
  EXPLAIN_INTERSECT_RESULT,
  EXPLAIN_EXCEPT_RESULT,
  EXPLAIN_UNARY_RESULT,
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
class Query_block : public Query_term {
 public:
  /**
    @note the group_by and order_by lists below will probably be added to the
          constructor when the parser is converted into a true bottom-up design.

          //SQL_I_LIST<ORDER> *group_by, SQL_I_LIST<ORDER> order_by
  */
  Query_block(MEM_ROOT *mem_root, Item *where, Item *having);

  /// Query_term methods overridden
  void debugPrint(int level, std::ostringstream &buf) const override;
  /// Minion of debugPrint
  void qbPrint(int level, std::ostringstream &buf) const;
  Query_term_type term_type() const override { return QT_QUERY_BLOCK; }
  const char *operator_string() const override { return "query_block"; }
  Query_block *query_block() const override {
    return const_cast<Query_block *>(this);
  }
  void destroy_tree() override { m_parent = nullptr; }

  bool open_result_tables(THD *, int) override;
  /// end of overridden methods from Query_term
  bool absorb_limit_of(Query_block *block);

  Item *where_cond() const { return m_where_cond; }
  Item **where_cond_ref() { return &m_where_cond; }
  void set_where_cond(Item *cond) { m_where_cond = cond; }
  Item *having_cond() const { return m_having_cond; }
  Item **having_cond_ref() { return &m_having_cond; }
  void set_having_cond(Item *cond) { m_having_cond = cond; }
  void set_query_result(Query_result *result) { m_query_result = result; }
  Query_result *query_result() const { return m_query_result; }
  bool change_query_result(THD *thd, Query_result_interceptor *new_result,
                           Query_result_interceptor *old_result);

  /// Set base options for a query block (and active options too)
  void set_base_options(ulonglong options_arg) {
    DBUG_EXECUTE_IF("no_const_tables", options_arg |= OPTION_NO_CONST_TABLES;);

    // Make sure we do not overwrite options by accident
    assert(m_base_options == 0 && m_active_options == 0);
    m_base_options = options_arg;
    m_active_options = options_arg;
  }

  /// Add base options to a query block, also update active options
  void add_base_options(ulonglong options) {
    assert(first_execution);
    m_base_options |= options;
    m_active_options |= options;
  }

  /**
    Remove base options from a query block.
    Active options are also updated, and we assume here that "extra" options
    cannot override removed base options.
  */
  void remove_base_options(ulonglong options) {
    assert(first_execution);
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
    for (Table_ref *tr = leaf_tables; tr != nullptr; tr = tr->next_leaf)
      tr->set_readonly();
  }

  /// @returns a map of all tables references in the query block
  table_map all_tables_map() const { return (1ULL << leaf_table_count) - 1; }

  bool remove_aggregates(THD *thd, Query_block *select);

  Query_expression *master_query_expression() const { return master; }
  Query_expression *first_inner_query_expression() const { return slave; }
  Query_block *outer_query_block() const { return master->outer_query_block(); }
  Query_block *next_query_block() const { return next; }

  Table_ref *find_table_by_name(const Table_ident *ident);

  Query_block *next_select_in_list() const { return link_next; }

  void mark_as_dependent(Query_block *last, bool aggregate);

  /// @returns true if query block references any tables
  bool has_tables() const { return m_table_list.elements != 0; }

  /// @return true if query block is explicitly grouped (non-empty GROUP BY)
  bool is_explicitly_grouped() const { return group_list.elements != 0; }

  /**
    @return true if this query block is implicitly grouped, ie it is not
    explicitly grouped but contains references to set functions.
    The query will return max. 1 row (@see also is_single_grouped()).
  */
  bool is_implicitly_grouped() const {
    return m_agg_func_used && group_list.elements == 0;
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

  /**
    Based on the structure of the query at resolution time, it is possible to
    conclude that DISTINCT is useless and remove it.
    This is the case if:
    - all GROUP BY expressions are in SELECT list, so resulting group rows are
    distinct,
    - and ROLLUP is not specified, so it adds no row for NULLs.

    @returns true if we can remove DISTINCT.

    @todo could refine this to if ROLLUP were specified and all GROUP
    expressions were non-nullable, because ROLLUP then adds only NULL values.
    Currently, ROLLUP+DISTINCT is rejected because executor cannot handle
    it in all cases.
  */
  bool can_skip_distinct() const {
    return is_grouped() && hidden_group_field_count == 0 &&
           olap == UNSPECIFIED_OLAP_TYPE;
  }

  /// @return true if this query block has a LIMIT clause
  bool has_limit() const { return select_limit != nullptr; }

  /// @return true if query block references full-text functions
  bool has_ft_funcs() const { return ftfunc_list->elements > 0; }

  /// @returns true if query block is a recursive member of a recursive unit
  bool is_recursive() const { return recursive_reference != nullptr; }

  /**
    Finds a group expression matching the given item, or nullptr if
    none. When there are multiple candidates, ones that match in name are
    given priority (such that “a AS c GROUP BY a,b,c” resolves to c, not a);
    if there is still a tie, the leftmost is given priority.

    @param item The item to search for.
    @param [out] rollup_level If not nullptr, will be set to the group
      expression's index (0-based).
   */
  ORDER *find_in_group_list(Item *item, int *rollup_level) const;
  int group_list_size() const;

  /// @returns true if query block contains window functions
  bool has_windows() const { return m_windows.elements > 0; }

  void invalidate();

  uint get_in_sum_expr() const { return in_sum_expr; }

  bool add_item_to_list(Item *item);
  bool add_ftfunc_to_list(Item_func_match *func);
  Table_ref *add_table_to_list(THD *thd, Table_ident *table, const char *alias,
                               ulong table_options,
                               thr_lock_type flags = TL_UNLOCK,
                               enum_mdl_type mdl_type = MDL_SHARED_READ,
                               List<Index_hint> *hints = nullptr,
                               List<String> *partition_names = nullptr,
                               LEX_STRING *option = nullptr,
                               Parse_context *pc = nullptr);

  /**
    Add item to the hidden part of select list

    @param item  item to add

    @return Pointer to reference of the added item
  */
  Item **add_hidden_item(Item *item);

  /// Remove hidden items from select list
  void remove_hidden_items();

  Table_ref *get_table_list() const { return m_table_list.first; }
  bool init_nested_join(THD *thd);
  Table_ref *end_nested_join();
  Table_ref *nest_last_join(THD *thd, size_t table_cnt = 2);
  bool add_joined_table(Table_ref *table);
  mem_root_deque<Item *> *get_fields_list() { return &fields; }

  /// Wrappers over fields / get_fields_list() that hide items where
  /// item->hidden, meant for range-based for loops. See sql/visible_fields.h.
  auto visible_fields() { return VisibleFields(fields); }
  auto visible_fields() const { return VisibleFields(fields); }

  /// Check privileges for views that are merged into query block
  bool check_view_privileges(THD *thd, ulong want_privilege_first,
                             ulong want_privilege_next);
  /// Check privileges for all columns referenced from query block
  bool check_column_privileges(THD *thd);

  /// Check privileges for column references in subqueries of a query block
  bool check_privileges_for_subqueries(THD *thd);

  /// Resolve and prepare information about tables for one query block
  bool setup_tables(THD *thd, Table_ref *tables, bool select_insert);

  /// Resolve OFFSET and LIMIT clauses
  bool resolve_limits(THD *thd);

  /// Resolve derived table, view, table function information for a query block
  bool resolve_placeholder_tables(THD *thd, bool apply_semijoin);

  /// Propagate exclusion from table uniqueness test into subqueries
  void propagate_unique_test_exclusion();

  /// Merge name resolution context objects of a subquery into its parent
  void merge_contexts(Query_block *inner);

  /// Merge derived table into query block
  bool merge_derived(THD *thd, Table_ref *derived_table);

  bool flatten_subqueries(THD *thd);

  /**
    Update available semijoin strategies for semijoin nests.

    Available semijoin strategies needs to be updated on every execution since
    optimizer_switch setting may have changed.

    @param thd  Pointer to THD object for session.
                Used to access optimizer_switch
  */
  void update_semijoin_strategies(THD *thd);

  /**
    Returns which subquery execution strategies can be used for this query
    block.

    @param thd  Pointer to THD object for session.
                Used to access optimizer_switch

    @retval SUBQ_MATERIALIZATION  Subquery Materialization should be used
    @retval SUBQ_EXISTS           In-to-exists execution should be used
    @retval CANDIDATE_FOR_IN2EXISTS_OR_MAT A cost-based decision should be made
  */
  Subquery_strategy subquery_strategy(const THD *thd) const;

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
  bool semijoin_enabled(const THD *thd) const;

  void set_sj_candidates(Mem_root_array<Item_exists_subselect *> *sj_cand) {
    sj_candidates = sj_cand;
  }

  bool has_sj_candidates() const {
    return sj_candidates != nullptr && !sj_candidates->empty();
  }

  /// Add full-text function elements from a list into this query block
  bool add_ftfunc_list(List<Item_func_match> *ftfuncs);

  void set_lock_for_table(const Lock_descriptor &descriptor, Table_ref *table);

  void set_lock_for_tables(thr_lock_type lock_type);

  inline void init_order() {
    assert(order_list.elements == 0);
    order_list.elements = 0;
    order_list.first = nullptr;
    order_list.next = &order_list.first;
  }
  /*
    This method created for reiniting LEX in mysql_admin_table() and can be
    used only if you are going remove all Query_block & units except belonger
    to LEX (LEX::unit & LEX::select, for other purposes use
    Query_expression::exclude_level()
  */
  void cut_subtree() { slave = nullptr; }
  bool test_limit();
  /**
    Get offset for LIMIT.

    Evaluate offset item if necessary.

    @return Number of rows to skip.

    @todo Integrate better with Query_expression::set_limit()
  */
  ha_rows get_offset(const THD *thd) const;
  /**
   Get limit.

   Evaluate limit item if necessary.

   @return Limit of rows in result.

   @todo Integrate better with Query_expression::set_limit()
  */
  ha_rows get_limit(const THD *thd) const;

  /// Assign a default name resolution object for this query block.
  bool set_context(Name_resolution_context *outer_context);

  /// Setup the array containing references to base items
  bool setup_base_ref_items(THD *thd);
  void print(const THD *thd, String *str, enum_query_type query_type);

  /**
    Print detail of the Query_block object.

    @param      thd          Thread handler
    @param      query_type   Options to print out string output
    @param[out] str          String of output.
  */
  void print_query_block(const THD *thd, String *str,
                         enum_query_type query_type);

  /**
    Print detail of the UPDATE statement.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_update(const THD *thd, String *str, enum_query_type query_type);

  /**
    Print detail of the DELETE statement.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_delete(const THD *thd, String *str, enum_query_type query_type);

  /**
    Print detail of the INSERT statement.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_insert(const THD *thd, String *str, enum_query_type query_type);

  /**
    Print detail of Hints.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_hints(const THD *thd, String *str, enum_query_type query_type);

  /**
    Print error.

    @param      thd          Thread handler
    @param[out] str          String of output

    @retval false   If there is no error
    @retval true    else
  */
  bool print_error(const THD *thd, String *str);

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
    @param      table_list   Table_ref object
    @param      query_type   Options to print out string output
  */
  void print_table_references(const THD *thd, String *str,
                              Table_ref *table_list,
                              enum_query_type query_type);

  /**
    Print list of items in Query_block object.

    @param      thd          Thread handle
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_item_list(const THD *thd, String *str, enum_query_type query_type);

  /**
    Print assignments list. Used in UPDATE and
    INSERT ... ON DUPLICATE KEY UPDATE ...

    @param      thd          Thread handle
    @param[out] str          String of output
    @param      query_type   Options to print out string output
    @param      fields       List columns to be assigned.
    @param      values       List of values.
  */
  void print_update_list(const THD *thd, String *str,
                         enum_query_type query_type,
                         const mem_root_deque<Item *> &fields,
                         const mem_root_deque<Item *> &values);

  /**
    Print column list to be inserted into. Used in INSERT.

    @param      thd          Thread handle
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_insert_fields(const THD *thd, String *str,
                           enum_query_type query_type);

  /**
    Print list of values, used in INSERT and for general VALUES clause.

    @param      thd          Thread handle
    @param[out] str          String of output
    @param      query_type   Options to print out string output
    @param      values       List of values
    @param      prefix       Prefix to print before each row in value list
                             = nullptr: No prefix wanted
  */
  void print_values(const THD *thd, String *str, enum_query_type query_type,
                    const mem_root_deque<mem_root_deque<Item *> *> &values,
                    const char *prefix);

  /**
    Print list of tables in FROM clause.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_from_clause(const THD *thd, String *str,
                         enum_query_type query_type);

  /**
    Print list of conditions in WHERE clause.

    @param      thd          Thread handle
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_where_cond(const THD *thd, String *str,
                        enum_query_type query_type);

  /**
    Print list of items in GROUP BY clause.

    @param      thd          Thread handle
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_group_by(const THD *thd, String *str, enum_query_type query_type);

  /**
    Print list of items in HAVING clause.

    @param      thd          Thread handle
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_having(const THD *thd, String *str, enum_query_type query_type);

  /**
    Print details of Windowing functions.

    @param      thd          Thread handler
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_windows(const THD *thd, String *str, enum_query_type query_type);

  /**
    Print list of items in ORDER BY clause.

    @param      thd          Thread handle
    @param[out] str          String of output
    @param      query_type   Options to print out string output
  */
  void print_order_by(const THD *thd, String *str,
                      enum_query_type query_type) const;

  void print_limit(const THD *thd, String *str,
                   enum_query_type query_type) const;
  bool save_properties(THD *thd);

  /**
    Accept function for SELECT and DELETE.

    @param    visitor  Select_lex_visitor Object
  */
  bool accept(Select_lex_visitor *visitor);

  /**
    Cleanup this subtree (this Query_block and all nested Query_blockes and
    Query_expressions).
    @param full  if false only partial cleanup is done, JOINs and JOIN_TABs are
    kept to provide info for EXPLAIN CONNECTION; if true, complete cleanup is
    done, all JOINs are freed.
  */
  void cleanup(bool full) override;
  /*
    Recursively cleanup the join of this select lex and of all nested
    select lexes. This is not a full cleanup.
  */
  void cleanup_all_joins();
  /**
    Destroy contained objects, in particular temporary tables which may
    have their own mem_roots.
  */
  void destroy();

  /// @return true when query block is not part of a set operation and is not a
  /// parenthesized query expression.
  bool is_simple_query_block() const {
    return master_query_expression()->is_simple();
  }

  /**
    @return true if query block is found during preparation to produce no data.
    Notice that if query is implicitly grouped, an aggregation row will
    still be returned.
  */
  bool is_empty_query() const { return m_empty_query; }

  /// Set query block as returning no data
  /// @todo This may also be set when we have an always false WHERE clause
  void set_empty_query() {
    assert(join == nullptr);
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

  bool right_joins() const { return m_right_joins; }
  void set_right_joins() { m_right_joins = true; }

  /// Lookup for Query_block type
  enum_explain_type type() const;

  /// Lookup for a type string
  const char *get_type_str() { return type_str[static_cast<int>(type())]; }
  static const char *get_type_str(enum_explain_type type) {
    return type_str[static_cast<int>(type)];
  }

  bool is_dependent() const { return uncacheable & UNCACHEABLE_DEPENDENT; }
  bool is_cacheable() const { return !uncacheable; }

  /// @returns true if this query block outputs at most one row.
  bool source_table_is_one_row() const {
    return (m_table_list.size() == 0 &&
            (!is_table_value_constructor || row_value_list->size() == 1));
  }

  /// Include query block inside a query expression.
  void include_down(LEX *lex, Query_expression *outer);

  /// Include a query block next to another query block.
  void include_neighbour(LEX *lex, Query_block *before);

  /// Include query block inside a query expression, but do not link.
  void include_standalone(Query_expression *sel);

  /// Include query block into global list.
  void include_in_global(Query_block **plink);

  /// Include chain of query blocks into global list.
  void include_chain_in_global(Query_block **start);

  /// Renumber query blocks of contained query expressions
  void renumber(LEX *lex);

  /**
    Does permanent transformations which are local to a query block (which do
    not merge it to another block).
  */
  bool apply_local_transforms(THD *thd, bool prune);

  /// Pushes parts of the WHERE condition of this query block to materialized
  /// derived tables.
  bool push_conditions_to_derived_tables(THD *thd);

  bool get_optimizable_conditions(THD *thd, Item **new_where,
                                  Item **new_having);

  bool validate_outermost_option(LEX *lex, const char *wrong_option) const;
  bool validate_base_options(LEX *lex, ulonglong options) const;

  bool walk(Item_processor processor, enum_walk walk, uchar *arg);

  bool add_tables(THD *thd, const Mem_root_array<Table_ident *> *tables,
                  ulong table_options, thr_lock_type lock_type,
                  enum_mdl_type mdl_type);

  bool resolve_rollup_wfs(THD *thd);

  bool setup_conds(THD *thd);
  bool prepare(THD *thd, mem_root_deque<Item *> *insert_field_list);
  bool optimize(THD *thd, bool finalize_access_paths);
  void reset_nj_counters(mem_root_deque<Table_ref *> *join_list = nullptr);

  // If the query block has exactly one single visible field, returns it.
  // If not, returns nullptr.
  Item *single_visible_field() const;
  size_t num_visible_fields() const;

  // Whether the SELECT list is empty (hidden fields are ignored).
  // Typically used to distinguish INSERT INTO ... SELECT queries
  // from INSERT INTO ... VALUES queries.
  bool field_list_is_empty() const;

  /// Creates a clone for the given expression by re-parsing the
  /// expression. Used in condition pushdown to derived tables.
  Item *clone_expression(THD *thd, Item *item);
  /// Returns an expression from the select list of the query block
  /// using the field's index in a derived table.
  Item *get_derived_expr(uint expr_index);

  MaterializePathParameters::QueryBlock setup_materialize_query_block(
      AccessPath *childPath, TABLE *dst_table);

  // ************************************************
  // * Members (most of these should not be public) *
  // ************************************************

  size_t m_added_non_hidden_fields{0};
  /**
    All expressions needed after join and filtering, ie., select list,
    group by list, having clause, window clause, order by clause,
    including hidden fields.
    Does not include join conditions nor where clause.

    This should ideally be changed into Mem_root_array<Item *>, but
    find_order_in_list() depends on pointer stability (it stores a pointer
    to an element in referenced_by[]). Similarly, there are some instances
    of thd->change_item_tree() that store pointers to elements in this list.

    Because of this, adding or removing elements in the middle is not allowed;
    std::deque guarantees pointer stability only in the face of adding
    or removing elements from either end, ie., {push,pop}_{front_back}.

    Currently, all hidden items must be before all visible items.
    This is primarily due to the requirement for pointer stability
    but also because change_to_use_tmp_fields() depends on it when mapping
    items to ref_item_array indexes. It would be good to get rid of this
    requirement in the future.
   */
  mem_root_deque<Item *> fields;

  /**
    All windows defined on the select, both named and inlined
  */
  List<Window> m_windows;

  /**
    A pointer to ftfunc_list_alloc, list of full text search functions.
  */
  List<Item_func_match> *ftfunc_list;
  List<Item_func_match> ftfunc_list_alloc{};

  /// The VALUES items of a table value constructor.
  mem_root_deque<mem_root_deque<Item *> *> *row_value_list{nullptr};

  /// List of semi-join nests generated for this query block
  mem_root_deque<Table_ref *> sj_nests;

  /// List of tables in FROM clause - use Table_ref::next_local to traverse
  SQL_I_List<Table_ref> m_table_list{};

  /**
    ORDER BY clause.
    This list may be mutated during optimization (by remove_const() in the old
    optimizer or by RemoveRedundantOrderElements() in the hypergraph optimizer),
    so for prepared statements, we keep a copy of the ORDER.next pointers in
    order_list_ptrs, and re-establish the original list before each execution.
  */
  SQL_I_List<ORDER> order_list{};
  Group_list_ptrs *order_list_ptrs{nullptr};

  /**
    GROUP BY clause.
    This list may be mutated during optimization (by remove_const() in the old
    optimizer or by RemoveRedundantOrderElements() in the hypergraph optimizer),
    so for prepared statements, we keep a copy of the ORDER.next pointers in
    group_list_ptrs, and re-establish the original list before each execution.
  */
  SQL_I_List<ORDER> group_list{};
  Group_list_ptrs *group_list_ptrs{nullptr};

  // Used so that AggregateIterator knows which items to signal when the rollup
  // level changes. Obviously only used in the presence of rollup.
  Prealloced_array<Item_rollup_group_item *, 4> rollup_group_items{
      PSI_NOT_INSTRUMENTED};
  Prealloced_array<Item_rollup_sum_switcher *, 4> rollup_sums{
      PSI_NOT_INSTRUMENTED};

  /// Query-block-level hints, for this query block
  Opt_hints_qb *opt_hints_qb{nullptr};

  char *db{nullptr};

  /**
     If this query block is a recursive member of a recursive unit: the
     Table_ref, in this recursive member, referencing the query
     name.
  */
  Table_ref *recursive_reference{nullptr};

  /// Reference to LEX that this query block belongs to
  LEX *parent_lex{nullptr};

  /**
    The set of those tables whose fields are referenced in the select list of
    this select level.
  */
  table_map select_list_tables{0};
  table_map outer_join{0};  ///< Bitmap of all inner tables from outer joins

  /**
    Context for name resolution for all column references except columns
    from joined tables.
  */
  Name_resolution_context context{};

  /**
    Pointer to first object in list of Name res context objects that have
    this query block as the base query block.
    Includes field "context" which is embedded in this query block.
  */
  Name_resolution_context *first_context;

  /**
    After optimization it is pointer to corresponding JOIN. This member
    should be changed only when THD::LOCK_query_plan mutex is taken.
  */
  JOIN *join{nullptr};
  /// Set of table references contained in outer-most join nest
  mem_root_deque<Table_ref *> m_table_nest;
  /// Pointer to the set of table references in the currently active join
  mem_root_deque<Table_ref *> *m_current_table_nest;
  /// table embedding the above list
  Table_ref *embedding{nullptr};
  /**
    Points to first leaf table of query block. After setup_tables() is done,
    this is a list of base tables and derived tables. After derived tables
    processing is done, this is a list of base tables only.
    Use Table_ref::next_leaf to traverse the list.
  */
  Table_ref *leaf_tables{nullptr};
  /// Last table for LATERAL join, used by table functions
  Table_ref *end_lateral_table{nullptr};

  /// LIMIT clause, NULL if no limit is given
  Item *select_limit{nullptr};
  /// LIMIT ... OFFSET clause, NULL if no offset is given
  Item *offset_limit{nullptr};

  /**
    Circular linked list of aggregate functions in nested query blocks.
    This is needed if said aggregate functions depend on outer values
    from this query block; if so, we want to add them as hidden items
    in our own field list, to be able to evaluate them.
    @see Item_sum::check_sum_func
   */
  Item_sum *inner_sum_func_list{nullptr};

  /**
    Array of pointers to "base" items; one each for every selected expression
    and referenced item in the query block. All references to fields are to
    buffers associated with the primary input tables.
  */
  Ref_item_array base_ref_items;

  uint select_number{0};  ///< Query block number (used for EXPLAIN)

  /**
    Saved values of the WHERE and HAVING clauses. Allowed values are:
     - COND_UNDEF if the condition was not specified in the query or if it
       has not been optimized yet
     - COND_TRUE if the condition is always true
     - COND_FALSE if the condition is impossible
     - COND_OK otherwise
  */
  Item::cond_result cond_value{Item::COND_UNDEF};
  Item::cond_result having_value{Item::COND_UNDEF};

  /// Parse context: indicates where the current expression is being parsed
  enum_parsing_context parsing_place{CTX_NONE};
  /// Parse context: is inside a set function if this is positive
  uint in_sum_expr{0};

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
  Resolve_place resolve_place{
      RESOLVE_NONE};  ///< Indicates part of query being resolved

  /**
    Number of fields used in select list or where clause of current select
    and all inner subselects.
  */
  uint select_n_where_fields{0};
  /**
    number of items in select_list and HAVING clause used to get number
    bigger then can be number of entries that will be added to all item
    list during split_sum_func
  */
  uint select_n_having_items{0};
  /// Number of arguments of and/or/xor in where/having/on
  uint saved_cond_count{0};
  /// Number of predicates after preparation
  uint cond_count{0};
  /// Number of between predicates in where/having/on
  uint between_count{0};
  /// Maximal number of elements in multiple equalities
  uint max_equal_elems{0};

  /**
    Number of Item_sum-derived objects in this SELECT. Keeps count of
    aggregate functions and window functions(to allocate items in ref array).
    See Query_block::setup_base_ref_items.
  */
  uint n_sum_items{0};
  /// Number of Item_sum-derived objects in children and descendant SELECTs
  uint n_child_sum_items{0};

  /// Keep track for allocation of base_ref_items: scalar subqueries may be
  /// replaced by a field during scalar_to_derived transformation
  uint n_scalar_subqueries{0};

  /// Number of materialized derived tables and views in this query block.
  uint materialized_derived_table_count{0};
  /// Number of partitioned tables
  uint partitioned_table_count{0};

  /**
    Number of wildcards used in the SELECT list. For example,
    SELECT *, t1.*, catalog.t2.* FROM t0, t1, t2;
    has 3 wildcards.
  */
  uint with_wild{0};

  /// Number of leaf tables in this query block.
  uint leaf_table_count{0};
  /// Number of derived tables and views in this query block.
  uint derived_table_count{0};
  /// Number of table functions in this query block
  uint table_func_count{0};

  /**
    Nesting level of query block, outer-most query block has level 0,
    its subqueries have level 1, etc. @see also sql/item_sum.h.
  */
  int nest_level{0};

  /// Indicates whether this query block contains the WITH ROLLUP clause
  olap_type olap{UNSPECIFIED_OLAP_TYPE};

  /// @see enum_condition_context
  enum_condition_context condition_context{enum_condition_context::ANDS};

  /// If set, the query block is of the form VALUES row_list.
  bool is_table_value_constructor{false};

  /// Describes context of this query block (e.g if it is a derived table).
  sub_select_type linkage{UNSPECIFIED_TYPE};

  /**
    result of this query can't be cached, bit field, can be :
      UNCACHEABLE_DEPENDENT
      UNCACHEABLE_RAND
      UNCACHEABLE_SIDEEFFECT
  */
  uint8 uncacheable{0};

  void update_used_tables();
  void restore_cmd_properties();
  bool save_cmd_properties(THD *thd);

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
  bool first_execution{true};

  /// True when semi-join pull-out processing is complete
  bool sj_pullout_done{false};

  /// Used by nested scalar_to_derived transformations
  bool m_was_implicitly_grouped{false};

  /// True: skip local transformations during prepare() call (used by INSERT)
  bool skip_local_transforms{false};

  bool is_item_list_lookup{false};

  /// true when having fix field called in processing of this query block
  bool having_fix_field{false};
  /// true when GROUP BY fix field called in processing of this query block
  bool group_fix_field{false};

  /**
    True if contains or aggregates set functions.
    @note this is wrong when a locally found set function is aggregated
    in an outer query block.
  */
  bool with_sum_func{false};

  /**
    HAVING clause contains subquery => we can't close tables before
    query processing end even if we use temporary table
  */
  bool subquery_in_having{false};

  /**
    If true, use select_limit to limit number of rows selected.
    Applicable when no explicit limit is supplied, and only for the
    outermost query block of a SELECT statement.
  */
  bool m_use_select_limit{false};

  /// If true, limit object is added internally
  bool m_internal_limit{false};

  /// exclude this query block from unique_table() check
  bool exclude_from_table_unique_test{false};

  bool no_table_names_allowed{false};  ///< used for global order by

  /// Hidden items added during optimization
  /// @note that using this means we modify resolved data during optimization
  uint hidden_items_from_optimization{0};

 private:
  friend class Query_expression;
  friend class Condition_context;

  /// Helper for save_properties()
  bool save_order_properties(THD *thd, SQL_I_List<ORDER> *list,
                             Group_list_ptrs **list_ptrs);

  bool record_join_nest_info(mem_root_deque<Table_ref *> *tables);
  bool simplify_joins(THD *thd, mem_root_deque<Table_ref *> *join_list,
                      bool top, bool in_sj, Item **new_conds,
                      uint *changelog = nullptr);
  /// Remove semijoin condition for this query block
  void clear_sj_expressions(NESTED_JOIN *nested_join);
  ///  Build semijoin condition for th query block
  bool build_sj_cond(THD *thd, NESTED_JOIN *nested_join,
                     Query_block *subq_query_block, table_map outer_tables_map,
                     Item **sj_cond);
  bool decorrelate_condition(Semijoin_decorrelation &sj_decor,
                             Table_ref *join_nest);

  bool convert_subquery_to_semijoin(THD *thd, Item_exists_subselect *subq_pred);
  Table_ref *synthesize_derived(THD *thd, Query_expression *unit,
                                Item *join_cond, bool left_outer,
                                bool use_inner_join);
  bool transform_subquery_to_derived(THD *thd, Table_ref **out_tl,
                                     Query_expression *subs_query_expression,
                                     Item_subselect *subq, bool use_inner_join,
                                     bool reject_multiple_rows,
                                     Item *join_condition,
                                     Item *lifted_where_cond);
  bool transform_table_subquery_to_join_with_derived(
      THD *thd, Item_exists_subselect *subq_pred);
  bool decorrelate_derived_scalar_subquery_pre(
      THD *thd, Table_ref *derived, Item *lifted_where,
      Lifted_fields_map *lifted_where_fields, bool *added_card_check);
  bool decorrelate_derived_scalar_subquery_post(
      THD *thd, Table_ref *derived, Lifted_fields_map *lifted_where_fields,
      bool added_card_check);
  void replace_referenced_item(Item *const old_item, Item *const new_item);
  void remap_tables(THD *thd);
  bool resolve_subquery(THD *thd);
  void mark_item_as_maybe_null_if_rollup_item(Item *item);
  Item *resolve_rollup_item(THD *thd, Item *item);
  bool resolve_rollup(THD *thd);

  bool setup_wild(THD *thd);
  bool setup_order_final(THD *thd);
  bool setup_group(THD *thd);
  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block);
  bool remove_redundant_subquery_clauses(THD *thd,
                                         int hidden_group_field_count);
  void repoint_contexts_of_join_nests(mem_root_deque<Table_ref *> join_list);
  bool empty_order_list(Query_block *sl);
  bool setup_join_cond(THD *thd, mem_root_deque<Table_ref *> *tables,
                       bool in_update);
  bool find_common_table_expr(THD *thd, Table_ident *table_id, Table_ref *tl,
                              Parse_context *pc, bool *found);
  /**
    Transform eligible scalar subqueries in the SELECT list, WHERE condition,
    HAVING condition or JOIN conditions of a query block[*] to an equivalent
    derived table of a LEFT OUTER join, e.g. as shown in this uncorrelated
    subquery:

    [*] a.k.a "transformed query block" throughout this method and its minions.

    <pre>
      SELECT * FROM t1
        WHERE t1.a > (SELECT COUNT(a) AS cnt FROM t2);  ->

      SELECT t1.* FROM t1 LEFT OUTER JOIN
                       (SELECT COUNT(a) AS cnt FROM t2) AS derived
        ON TRUE WHERE t1.a > derived.cnt;
    </pre>

    Grouping in the transformed query block may necessitate the grouping to be
    moved down to another derived table, cf.  transform_grouped_to_derived.

    Limitations:
    - only implicitly grouped subqueries (guaranteed to have cardinality one)
      are identified as scalar subqueries.
    _ Correlated subqueries are not handled

    @param[in,out] thd the session context
    @returns       true on error
  */
  bool transform_scalar_subqueries_to_join_with_derived(THD *thd);
  bool supported_correlated_scalar_subquery(THD *thd, Item::Css_info *subquery,
                                            Item **lifted_where);
  bool replace_item_in_expression(Item **expr, bool was_hidden,
                                  Item::Item_replacement *info,
                                  Item_transformer transformer);
  bool transform_grouped_to_derived(THD *thd, bool *break_off);
  bool replace_subquery_in_expr(THD *thd, Item::Css_info *subquery,
                                Table_ref *tr, Item **expr);
  bool nest_derived(THD *thd, Item *join_cond,
                    mem_root_deque<Table_ref *> *join_list,
                    Table_ref *new_derived_table);

  bool resolve_table_value_constructor_values(THD *thd);

  // Delete unused columns from merged derived tables
  void delete_unused_merged_columns(mem_root_deque<Table_ref *> *tables);

  bool prepare_values(THD *thd);
  bool check_only_full_group_by(THD *thd);
  bool is_row_count_valid_for_semi_join();

  /**
    Copies all non-aggregated calls to the full-text search MATCH function from
    the HAVING clause to the SELECT list (as hidden items), so that we can
    materialize their result and not only their input. This is needed when the
    result will be accessed after aggregation, as the result from MATCH cannot
    be recalculated from its input alone. It also needs the underlying scan to
    be positioned on the correct row. Storing the value before aggregation
    removes the need for evaluating MATCH again after materialization.
  */
  bool lift_fulltext_from_having_to_select_list(THD *thd);

  //
  // Members:
  //

  /**
    Pointer to collection of subqueries candidate for semi/antijoin
    conversion.
    Template parameter is "true": no need to run DTORs on pointers.
  */
  Mem_root_array<Item_exists_subselect *> *sj_candidates{nullptr};

  /// How many expressions are part of the order by but not select list.
  int hidden_order_field_count{0};

  /**
    Intrusive linked list of all query blocks within the same
    query expression.
  */
  Query_block *next{nullptr};

  /// The query expression containing this query block.
  Query_expression *master{nullptr};
  /// The first query expression contained within this query block.
  Query_expression *slave{nullptr};

  /// Intrusive double-linked global list of query blocks.
  Query_block *link_next{nullptr};
  Query_block **link_prev{nullptr};

  /// Result of this query block
  Query_result *m_query_result{nullptr};

  /**
    Options assigned from parsing and throughout resolving,
    should not be modified after resolving is done.
  */
  ulonglong m_base_options{0};
  /**
    Active options. Derived from base options, modifiers added during
    resolving and values from session variable option_bits. Since the latter
    may change, active options are refreshed per execution of a statement.
  */
  ulonglong m_active_options{0};

  Table_ref *resolve_nest{
      nullptr};  ///< Used when resolving outer join condition

  /**
    Condition to be evaluated after all tables in a query block are joined.
    After all permanent transformations have been conducted by
    Query_block::prepare(), this condition is "frozen", any subsequent changes
    to it must be done with change_item_tree(), unless they only modify AND/OR
    items and use a copy created by Query_block::get_optimizable_conditions().
    Same is true for 'having_cond'.
  */
  Item *m_where_cond;

  /// Condition to be evaluated on grouped rows after grouping.
  Item *m_having_cond;

  /// Number of GROUP BY expressions added to all_fields
  int hidden_group_field_count;

  /**
    True if query block has semi-join nests merged into it. Notice that this
    is updated earlier than sj_nests, so check this if info is needed
    before the full resolver process is complete.
  */
  bool has_sj_nests{false};
  bool has_aj_nests{false};   ///< @see has_sj_nests; counts antijoin nests.
  bool m_right_joins{false};  ///< True if query block has right joins

  /// Allow merge of immediate unnamed derived tables
  bool allow_merge_derived{true};

  bool m_agg_func_used{false};
  bool m_json_agg_func_used{false};

  /**
    True if query block does not generate any rows before aggregation,
    determined during preparation (not optimization).
  */
  bool m_empty_query{false};

  static const char
      *type_str[static_cast<int>(enum_explain_type::EXPLAIN_total)];
};

inline bool Query_expression::is_union() const {
  Query_term *qt = query_term();
  while (qt->term_type() == QT_UNARY)
    qt = down_cast<Query_term_unary *>(qt)->m_children[0];
  return qt->term_type() == QT_UNION;
}

inline bool Query_expression::is_set_operation() const {
  Query_term *qt = query_term();
  while (qt->term_type() == QT_UNARY)
    qt = down_cast<Query_term_unary *>(qt)->m_children[0];
  const Query_term_type type = qt->term_type();
  return type == QT_UNION || type == QT_INTERSECT || type == QT_EXCEPT;
}

/// Utility RAII class to save/modify/restore the condition_context information
/// of a query block. @see enum_condition_context.
class Condition_context {
 public:
  Condition_context(
      Query_block *select_ptr,
      enum_condition_context new_type = enum_condition_context::NEITHER)
      : select(nullptr), saved_value() {
    if (select_ptr) {
      select = select_ptr;
      saved_value = select->condition_context;
      // More restrictive wins over less restrictive:
      if (new_type == enum_condition_context::NEITHER ||
          (new_type == enum_condition_context::ANDS_ORS &&
           saved_value == enum_condition_context::ANDS))
        select->condition_context = new_type;
    }
  }
  ~Condition_context() {
    if (select) select->condition_context = saved_value;
  }

 private:
  Query_block *select;
  enum_condition_context saved_value;
};

bool walk_join_list(mem_root_deque<Table_ref *> &list,
                    std::function<bool(Table_ref *)> action);

/**
  Base class for secondary engine execution context objects. Secondary
  storage engines may create classes derived from this one which
  contain state they need to preserve between optimization and
  execution of statements. The context objects should be allocated on
  the execution MEM_ROOT.
*/
class Secondary_engine_execution_context {
 public:
  /**
    Destructs the secondary engine execution context object. It is
    called after the query execution has completed. Secondary engines
    may override the destructor in subclasses and add code that
    performs cleanup tasks that are needed after query execution.
  */
  virtual ~Secondary_engine_execution_context() = default;
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
  Table_ref *query_tables;
  /* Pointer to next_global member of last element in the previous list. */
  Table_ref **query_tables_last;
  /*
    If non-0 then indicates that query requires prelocking and points to
    next_global member of last own element in query table list (i.e. last
    table which was not added to it as part of preparation to prelocking).
    0 - indicates that this query does not need prelocking.
  */
  Table_ref **query_tables_own_last;
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
  bool is_query_tables_locked() const {
    return (lock_tables_state == LTS_LOCKED);
  }

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
  Query_tables_list() = default;
  ~Query_tables_list() = default;

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
  void add_to_query_tables(Table_ref *table) {
    *(table->prev_global = query_tables_last) = table;
    query_tables_last = &table->next_global;
  }
  bool requires_prelocking() { return query_tables_own_last; }
  void mark_as_requiring_prelocking(Table_ref **tables_own_last) {
    query_tables_own_last = tables_own_last;
  }
  /* Return pointer to first not-own table in query-tables or 0 */
  Table_ref *first_not_own_table() {
    return (query_tables_own_last ? *query_tables_own_last : nullptr);
  }
  void chop_off_not_own_tables() {
    if (query_tables_own_last) {
      *query_tables_own_last = nullptr;
      query_tables_last = query_tables_own_last;
      query_tables_own_last = nullptr;
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

    /**
      If a substatement inserts into or updates a table that has a column with
      an unsafe DEFAULT expression, it may not have the same effect on the
      slave.
    */
    BINLOG_STMT_UNSAFE_DEFAULT_EXPRESSION_IN_SUBSTATEMENT,

    /**
      DML or DDL statement that reads a ACL table is unsafe, because the row
      are read without acquiring SE row locks. This would allow ACL tables to
      be updated by concurrent thread. It would not have the same effect on the
      slave.
    */
    BINLOG_STMT_UNSAFE_ACL_TABLE_READ_IN_DML_DDL,

    /**
      Generating invisible primary key for a table created using CREATE TABLE...
      SELECT... is unsafe because order in which rows are retrieved by the
      SELECT determines which (if any) rows are inserted. This order cannot be
      predicted and values for generated invisible primary key column may
      differ on source and replica when @@session.binlog_format=STATEMENT.
    */
    BINLOG_STMT_UNSAFE_CREATE_SELECT_WITH_GIPK,

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
    DBUG_TRACE;
    assert(unsafe_type >= 0 && unsafe_type < BINLOG_STMT_UNSAFE_COUNT);
    binlog_stmt_flags |= (1U << unsafe_type);
    return;
  }

  /**
    Set the bits of binlog_stmt_flags determining the type of
    unsafeness of the current statement.  No existing bits will be
    cleared, but new bits may be set.

    @param flags A binary combination of zero or more bits, (1<<flag)
    where flag is a member of enum_binlog_stmt_unsafe.
  */
  inline void set_stmt_unsafe_flags(uint32 flags) {
    DBUG_TRACE;
    assert((flags & ~BINLOG_STMT_UNSAFE_ALL_FLAGS) == 0);
    binlog_stmt_flags |= flags;
    return;
  }

  /**
    Return a binary combination of all unsafe warnings for the
    statement.  If the statement has been marked as unsafe by the
    'flag' member of enum_binlog_stmt_unsafe, then the return value
    from this function has bit (1<<flag) set to 1.
  */
  inline uint32 get_stmt_unsafe_flags() const {
    DBUG_TRACE;
    return binlog_stmt_flags & BINLOG_STMT_UNSAFE_ALL_FLAGS;
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
    DBUG_TRACE;
    binlog_stmt_flags |=
        (1U << (BINLOG_STMT_UNSAFE_COUNT + BINLOG_STMT_TYPE_ROW_INJECTION));
    return;
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

#ifndef NDEBUG
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
        assert(0);
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
    DBUG_TRACE;

    assert(accessed_table >= 0 && accessed_table < STMT_ACCESS_TABLE_COUNT);
    stmt_accessed_table_flag |= (1U << accessed_table);

    return;
  }

  /**
    Checks if a type of table is about to be accessed while executing a
    statement.

    @param accessed_table Enumeration type that defines the type of table,
           e.g. temporary, transactional, non-transactional.

    @retval true  if the type of the table is about to be accessed
    @retval false otherwise
  */
  inline bool stmt_accessed_table(enum_stmt_accessed_table accessed_table) {
    DBUG_TRACE;

    assert(accessed_table >= 0 && accessed_table < STMT_ACCESS_TABLE_COUNT);

    return (stmt_accessed_table_flag & (1U << accessed_table)) != 0;
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

#if !defined(NDEBUG)
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

  void set_stmt_unsafe_with_mixed_mode() { stmt_unsafe_with_mixed_mode = true; }
  bool is_stmt_unsafe_with_mixed_mode() const {
    return stmt_unsafe_with_mixed_mode;
  }

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

  /**
    This flag is set to true if statement is unsafe to be binlogged in STATEMENT
    format, when in MIXED mode.
    Currently this flag is set to true if stored program used in statement has
    CREATE/DROP temporary table operation(s) as sub-statement(s).
  */
  bool stmt_unsafe_with_mixed_mode{false};
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
    assert(m_ptr + n <= m_end_of_query);
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
    assert(m_ptr <= m_end_of_query);
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
    assert(m_ptr <= m_end_of_query);
    return m_ptr[0];
  }

  /**
    Look ahead at some character to parse.
    @param n offset of the character to look up
  */
  unsigned char yyPeekn(int n) const {
    assert(m_ptr + n <= m_end_of_query);
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
    assert(m_ptr <= m_end_of_query);
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
    assert(m_ptr + n <= m_end_of_query);
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
    assert(m_ptr > m_tok_start);
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
  Lexer_yystype *yylval;

  /**
    LALR(2) resolution, look ahead token.
    Value of the next token to return, if any,
    or -1, if no token was parsed in advance.
    Note: 0 is a legal token, and represents YYEOF.
  */
  int lookahead_token;

  /** LALR(2) resolution, value of the look ahead token.*/
  Lexer_yystype *lookahead_yylval;

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

  void add_digest_token(uint token, Lexer_yystype *yylval);

  void reduce_digest_token(uint token_left, uint token_right);

  /**
    True if this scanner tokenizes a partial query (partition expression,
    generated column expression etc.)

    @return true if parsing a partial query, otherwise false.
  */
  bool is_partial_parser() const { return grammar_selector_token >= 0; }

  /**
    Outputs warnings on deprecated charsets in complete SQL statements

    @param [in] cs    The character set/collation to check for a deprecation.
    @param [in] alias The name/alias of @p cs.
  */
  void warn_on_deprecated_charset(const CHARSET_INFO *cs,
                                  const char *alias) const {
    if (!is_partial_parser()) {
      ::warn_on_deprecated_charset(m_thd, cs, alias);
    }
  }

  /**
    Outputs warnings on deprecated collations in complete SQL statements

    @param [in] collation     The collation to check for a deprecation.
  */
  void warn_on_deprecated_collation(const CHARSET_INFO *collation) const {
    if (!is_partial_parser()) {
      ::warn_on_deprecated_collation(m_thd, collation);
    }
  }

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

  /** Beginning of the query text in the input stream, in the raw buffer. */
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
    5. -1 when parsing with the main grammar (no grammar selector available).

    @note yylex() is expected to return the value of type int:
          0 is for EOF and everything else for real token numbers.
          Bison, in its turn, generates positive token numbers.
          So, the negative grammar_selector_token means "not a token".
          In other words, -1 is "empty value".
  */
  const int grammar_selector_token;

  bool text_string_is_7bit() const { return !(tok_bitmap & 0x80); }
};

class LEX_COLUMN {
 public:
  String column;
  uint rights;
  LEX_COLUMN(const String &x, const uint &y) : column(x), rights(y) {}
};

enum class role_enum;

/*
  This structure holds information about grantor's context
*/
class LEX_GRANT_AS {
 public:
  LEX_GRANT_AS();
  void cleanup();

 public:
  bool grant_as_used;
  role_enum role_type;
  LEX_USER *user;
  List<LEX_USER> *role_list;
};

/**
  The LEX object currently serves three different purposes:

  - It contains some universal properties of an SQL command, such as
    sql_command, presence of IGNORE in data change statement syntax, and list
    of tables (query_tables).

  - It contains some execution state variables, like m_exec_started
    (set to true when execution is started), plugins (list of plugins used
    by statement), insert_update_values_map (a map of objects used by certain
    INSERT statements), etc.

  - It contains a number of members that should be local to subclasses of
    Sql_cmd, like purge_value_list (for the PURGE command), kill_value_list
    (for the KILL command).

  The LEX object is strictly a part of class Sql_cmd, for those SQL commands
  that are represented by an Sql_cmd class. For the remaining SQL commands,
  it is a standalone object linked to the current THD.

  The lifecycle of a LEX object is as follows:

  - The LEX object is constructed either on the execution mem_root
    (for regular statements), on a Prepared_statement mem_root (for
    prepared statements), on an SP mem_root (for stored procedure instructions),
    or created on the current mem_root for short-lived uses.

  - Call lex_start() to initialize a LEX object before use.
    This initializes the execution state part of the object.
    It also calls LEX::reset() to ensure that all members are properly inited.

  - Parse and resolve the statement, using the LEX as a work area.

  - Execute an SQL command: call set_exec_started() when starting to execute
    (actually when starting to optimize).
    Typically call is_exec_started() to distinguish between preparation
    and optimization/execution stages of SQL command execution.

  - Call clear_execution() when execution is finished. This will clear all
    execution state associated with the SQL command, it also includes calling
    LEX::reset_exec_started().

  @todo - Create subclasses of Sql_cmd to contain data that are local
          to specific commands.

  @todo - Create a Statement context object that will hold the execution state
          part of struct LEX.

  @todo - Ensure that a LEX struct is never reused, thus making e.g
          LEX::reset() redundant.
*/

struct LEX : public Query_tables_list {
  friend bool lex_start(THD *thd);

  Query_expression *unit;  ///< Outer-most query expression
  /// @todo: query_block can be replaced with unit->first-select()
  Query_block *query_block;            ///< First query block
  Query_block *all_query_blocks_list;  ///< List of all query blocks
 private:
  /* current Query_block in parsing */
  Query_block *m_current_query_block;

 public:
  inline Query_block *current_query_block() const {
    return m_current_query_block;
  }

  /*
    We want to keep current_thd out of header files, so the debug assert
    is moved to the .cc file.
  */
  void assert_ok_set_current_query_block();
  inline void set_current_query_block(Query_block *select) {
#ifndef NDEBUG
    assert_ok_set_current_query_block();
#endif
    m_current_query_block = select;
  }
  /// @return true if this is an EXPLAIN statement
  bool is_explain() const { return explain_format != nullptr; }
  bool is_explain_analyze = false;
  /**
    Whether the currently-running query should be (attempted) executed in
    the hypergraph optimizer. This will not change after the query is
    done parsing, so you can use it in any query phase to e.g. figure out
    whether to inhibit some transformation that the hypergraph optimizer
    does not properly understand yet.
   */
  bool using_hypergraph_optimizer = false;
  LEX_STRING name;
  char *help_arg;
  char *to_log; /* For PURGE MASTER LOGS TO */
  const char *x509_subject, *x509_issuer, *ssl_cipher;
  // Widcard from SHOW ... LIKE <wildcard> statements.
  String *wild;
  Query_result *result;
  LEX_STRING binlog_stmt_arg = {
      nullptr, 0};  ///< Argument of the BINLOG event statement.
  LEX_STRING ident;
  LEX_USER *grant_user;
  LEX_ALTER alter_password;
  enum_alter_user_attribute alter_user_attribute;
  LEX_STRING alter_user_comment_text;
  LEX_GRANT_AS grant_as;
  THD *thd;

  /* Optimizer hints */
  Opt_hints_global *opt_hints_global;

  /* maintain a list of used plugins for this LEX */
  typedef Prealloced_array<plugin_ref, INITIAL_LEX_PLUGIN_LIST_SIZE>
      Plugins_array;
  Plugins_array plugins;

  /// Table being inserted into (may be a view)
  Table_ref *insert_table;
  /// Leaf table being inserted into (always a base table)
  Table_ref *insert_table_leaf;

  /** SELECT of CREATE VIEW statement */
  LEX_STRING create_view_query_block;

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

  void insert_values_map(Item_field *f1, Field *f2) {
    if (!insert_update_values_map)
      insert_update_values_map = new std::map<Item_field *, Field *>;
    insert_update_values_map->insert(std::make_pair(f1, f2));
  }
  void destroy_values_map() {
    if (insert_update_values_map) {
      insert_update_values_map->clear();
      delete insert_update_values_map;
      insert_update_values_map = nullptr;
    }
  }
  void clear_values_map() {
    if (insert_update_values_map) {
      insert_update_values_map->clear();
    }
  }
  bool has_values_map() const { return insert_update_values_map != nullptr; }
  std::map<Item_field *, Field *>::iterator begin_values_map() {
    return insert_update_values_map->begin();
  }
  std::map<Item_field *, Field *>::iterator end_values_map() {
    return insert_update_values_map->end();
  }

 private:
  /*
    With Visual Studio, an std::map will always allocate two small objects
    on the heap. Sometimes we put LEX objects in a MEM_ROOT, and never run
    the LEX DTOR. To avoid memory leaks, put this std::map on the heap,
    and call clear_values_map() at the end of each statement.
   */
  std::map<Item_field *, Field *> *insert_update_values_map;

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

  /// If true: during prepare, we did a subquery transformation (IN-to-EXISTS,
  /// SOME/ANY) that doesn't currently work for subquery to a derived table
  /// transformation.
  bool m_subquery_to_derived_is_impossible;

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
  /**
    If currently re-parsing a condition which is pushed down to a derived
    table, this will be set to true.
  */
  bool reparse_derived_table_condition{false};
  /**
    If currently re-parsing a condition that is being pushed down to a
    derived table, this has the positions of all the parameters that are
    part of that condition in the original statement. Otherwise it is empty.
  */
  std::vector<uint> reparse_derived_table_params_at;

  enum SSL_type ssl_type; /* defined in violite.h */
  enum enum_duplicates duplicates;
  enum enum_tx_isolation tx_isolation;
  enum enum_var_type option_type;
  enum_view_create_mode create_view_mode;

  /// QUERY ID for SHOW PROFILE
  my_thread_id show_profile_query_id;
  uint profile_options;
  uint grant, grant_tot_col;
  /**
   Set to true when GRANT ... GRANT OPTION ... TO ...
   is used (vs. GRANT ... WITH GRANT OPTION).
   The flag is used by @ref mysql_grant to grant GRANT OPTION (@ref GRANT_ACL)
   to all dynamic privileges.
  */
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
  /**
    refers to optional IF EXISTS clause in REVOKE sql. This flag when set to
    true will report warnings in case privilege being granted is not granted to
    given user/role. When set to false error is reported.
  */
  bool grant_if_exists;
  /**
    refers to optional IGNORE UNKNOWN USER clause in REVOKE sql. This flag when
    set to true will report warnings in case target user/role for which
    privilege being granted does not exists. When set to false error is
    reported.
  */
  bool ignore_unknown_user;
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

 private:
  /// True if statement references UDF functions
  bool m_has_udf{false};
  bool ignore;

 public:
  bool is_ignore() const { return ignore; }
  void set_ignore(bool ignore_param) { ignore = ignore_param; }
  void set_has_udf() { m_has_udf = true; }
  bool has_udf() const { return m_has_udf; }
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
  /**
    Set to true when execution is completed, ie optimization has been done
    and execution is successful or ended in error.
  */
  bool m_exec_completed;
  /**
    Current SP parsing context.
    @see also sp_head::m_root_parsing_ctx.
  */
  sp_pcontext *sp_current_parsing_ctx;

  /**
    Statement context for Query_block::make_active_options.
  */
  ulonglong m_statement_options{0};

 public:
  /**
    Gets the options that have been set for this statement. The options are
    propagated to the Query_block objects and should usually be read with
    #Query_block::active_options().

    @return a bit set of options set for this statement
  */
  ulonglong statement_options() { return m_statement_options; }
  /**
    Add options to values of m_statement_options. options is an ORed
    bit set of options defined in query_options.h

    @param options Add this set of options to the set already in
                   m_statement_options
  */
  void add_statement_options(ulonglong options) {
    m_statement_options |= options;
  }
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

  bool check_preparation_invalid(THD *thd);

  void cleanup(bool full) {
    unit->cleanup(full);
    if (full) {
      m_IS_table_stats.invalidate_cache();
      m_IS_tablespace_stats.invalidate_cache();
    }
  }

  bool is_exec_started() const { return m_exec_started; }
  void set_exec_started() { m_exec_started = true; }
  void reset_exec_started() {
    m_exec_started = false;
    m_exec_completed = false;
  }
  /**
    Check whether the statement has been executed (regardless of completion -
    successful or in error).
    Check this instead of Query_expression::is_executed() to determine
    the state of a complete statement.
  */
  bool is_exec_completed() const { return m_exec_completed; }
  void set_exec_completed() { m_exec_completed = true; }
  sp_pcontext *get_sp_current_parsing_ctx() { return sp_current_parsing_ctx; }

  void set_sp_current_parsing_ctx(sp_pcontext *ctx) {
    sp_current_parsing_ctx = ctx;
  }

  /// Check if the current statement uses meta-data (uses a table or a stored
  /// routine).
  bool is_metadata_used() const {
    return query_tables != nullptr || has_udf() ||
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

  /// Destroy contained objects, but not the LEX object itself.
  void destroy() {
    if (unit == nullptr) return;
    unit->destroy();
    unit = nullptr;
    query_block = nullptr;
    all_query_blocks_list = nullptr;
    m_current_query_block = nullptr;
    destroy_values_map();
  }

  /// Reset query context to initial state
  void reset();

  /// Create an empty query block within this LEX object.
  Query_block *new_empty_query_block();

  /// Create query expression object that contains one query block.
  Query_block *new_query(Query_block *curr_query_block);

  /// Create query block and attach it to the current query expression.
  Query_block *new_set_operation_query(Query_block *curr_query_block);

  /// Create top-level query expression and query block.
  bool new_top_level_query();

  /// Create query expression and query block in existing memory objects.
  void new_static_query(Query_expression *sel_query_expression,
                        Query_block *select);

  /// Create query expression under current_query_block and a query block under
  /// the new query expression. The new query expression is linked in under
  /// current_query_block. The new query block is linked in under the new
  /// query expression.
  ///
  /// @param thd            current session context
  /// @param current_query_block the root under which we create the new
  /// expression
  ///                       and block
  /// @param where_clause   any where clause for the block
  /// @param having_clause  any having clause for the block
  /// @param ctx            the parsing context
  ///
  /// @returns              the new query expression, or nullptr on error.
  Query_expression *create_query_expr_and_block(
      THD *thd, Query_block *current_query_block, Item *where_clause,
      Item *having_clause, enum_parsing_context ctx);

  inline bool is_ps_or_view_context_analysis() {
    return (context_analysis_only &
            (CONTEXT_ANALYSIS_ONLY_PREPARE | CONTEXT_ANALYSIS_ONLY_VIEW));
  }

  inline bool is_view_context_analysis() {
    return (context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW);
  }

  void clear_execution();

  /**
    Set the current query as uncacheable.

    @param curr_query_block Current select query block
    @param cause       Why this query is uncacheable.

    @details
    All query blocks representing subqueries, from the current one up to
    the outer-most one, but excluding the main query block, are also set
    as uncacheable.
  */
  void set_uncacheable(Query_block *curr_query_block, uint8 cause) {
    safe_to_cache_query = false;

    if (m_current_query_block == nullptr) return;
    Query_block *sl;
    Query_expression *un;
    for (sl = curr_query_block, un = sl->master_query_expression(); un != unit;
         sl = sl->outer_query_block(), un = sl->master_query_expression()) {
      sl->uncacheable |= cause;
      un->uncacheable |= cause;
    }
  }
  void set_trg_event_type_for_tables();

  Table_ref *unlink_first_table(bool *link_to_local);
  void link_first_table_back(Table_ref *first, bool link_to_local);
  void first_lists_tables_same();

  void restore_cmd_properties() { unit->restore_cmd_properties(); }

  void restore_properties_for_insert() {
    for (Table_ref *tr = insert_table->first_leaf_table(); tr != nullptr;
         tr = tr->next_leaf)
      tr->restore_properties();
  }

  bool save_cmd_properties(THD *thd) { return unit->save_cmd_properties(thd); }

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
      on its top. So query_block (as the first added) will be at the tail
      of the list.
    */
    if (query_block == all_query_blocks_list &&
        (sroutines == nullptr || sroutines->empty())) {
      assert(!all_query_blocks_list->next_select_in_list());
      return true;
    }
    return false;
  }

  void release_plugins();

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

 private:
  /**
    Context object used by secondary storage engines to store query
    state during optimization and execution.
  */
  Secondary_engine_execution_context *m_secondary_engine_context{nullptr};

 public:
  /**
    Gets the secondary engine execution context for this statement.
  */
  Secondary_engine_execution_context *secondary_engine_execution_context()
      const {
    return m_secondary_engine_context;
  }

  /**
    Sets the secondary engine execution context for this statement.
    The old context object is destroyed, if there is one. Can be set
    to nullptr to destroy the old context object and clear the
    pointer.

    The supplied context object should be allocated on the execution
    MEM_ROOT, so that its memory doesn't have to be manually freed
    after query execution.
  */
  void set_secondary_engine_execution_context(
      Secondary_engine_execution_context *context);

 private:
  bool m_is_replication_deprecated_syntax_used{false};

 public:
  bool is_replication_deprecated_syntax_used() {
    return m_is_replication_deprecated_syntax_used;
  }

  void set_replication_deprecated_syntax_used() {
    m_is_replication_deprecated_syntax_used = true;
  }

 private:
  bool m_was_replication_command_executed{false};

 public:
  bool was_replication_command_executed() const {
    return m_was_replication_command_executed;
  }

  void set_was_replication_command_executed() {
    m_was_replication_command_executed = true;
  }

  bool set_channel_name(LEX_CSTRING name = {});

 private:
  bool rewrite_required{false};

 public:
  void set_rewrite_required() { rewrite_required = true; }
  void reset_rewrite_required() { rewrite_required = false; }
  bool is_rewrite_required() { return rewrite_required; }
};

/**
  RAII class to ease the call of LEX::mark_broken() if error.
  Used during preparation and optimization of DML queries.
*/
class Prepare_error_tracker {
 public:
  Prepare_error_tracker(THD *thd_arg) : thd(thd_arg) {}
  ~Prepare_error_tracker();

 private:
  THD *const thd;
};

/**
  The internal state of the syntax parser.
  This object is only available during parsing,
  and is private to the syntax parser implementation (sql_yacc.yy).
*/
class Yacc_state {
 public:
  Yacc_state() : yacc_yyss(nullptr), yacc_yyvs(nullptr), yacc_yyls(nullptr) {
    reset();
  }

  void reset() {
    if (yacc_yyss != nullptr) {
      my_free(yacc_yyss);
      yacc_yyss = nullptr;
    }
    if (yacc_yyvs != nullptr) {
      my_free(yacc_yyvs);
      yacc_yyvs = nullptr;
    }
    if (yacc_yyls != nullptr) {
      my_free(yacc_yyls);
      yacc_yyls = nullptr;
    }
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
    Query_block::set_lock_for_tables() method.

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
  /**
    True if the text parsed corresponds to an actual query,
    and not another text artifact.
    This flag is used to disable digest parsing of nested:
    - view definitions
    - table trigger definitions
    - table partition definitions
    - event scheduler event definitions
  */
  bool m_has_digest;
  /**
    True if the caller needs to compute a digest.
    This flag is used to request explicitly a digest computation,
    independently of the performance schema configuration.
  */
  bool m_compute_digest;

  Parser_input() : m_has_digest(false), m_compute_digest(false) {}
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
  explicit Parser_state(int grammar_selector_token)
      : m_input(), m_lip(grammar_selector_token), m_yacc(), m_comment(false) {}

 public:
  Parser_state() : m_input(), m_lip(~0U), m_yacc(), m_comment(false) {}

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

  Value_generator *result;
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

/**
  Parser state for Derived table's condition parser.
  (Used in condition pushdown to derived tables)
*/
class Derived_expr_parser_state : public Parser_state {
 public:
  Derived_expr_parser_state();

  Item *result;
};

struct st_lex_local : public LEX {
  static void *operator new(size_t size) noexcept {
    return (*THR_MALLOC)->Alloc(size);
  }
  static void *operator new(size_t size, MEM_ROOT *mem_root,
                            const std::nothrow_t &arg
                            [[maybe_unused]] = std::nothrow) noexcept {
    return mem_root->Alloc(size);
  }
  static void operator delete(void *ptr [[maybe_unused]],
                              size_t size [[maybe_unused]]) {
    TRASH(ptr, size);
  }
  static void operator delete(
      void *, MEM_ROOT *, const std::nothrow_t &) noexcept { /* Never called */
  }
};

extern bool lex_init(void);
extern void lex_free(void);
extern bool lex_start(THD *thd);
extern void lex_end(LEX *lex);
extern int MYSQLlex(union YYSTYPE *, struct YYLTYPE *, class THD *);

extern void trim_whitespace(const CHARSET_INFO *cs, LEX_STRING *str);

extern bool is_lex_native_function(const LEX_STRING *name);

bool is_keyword(const char *name, size_t len);
bool db_is_default_db(const char *db, size_t db_len, const THD *thd);

bool check_select_for_locking_clause(THD *);

void print_derived_column_names(const THD *thd, String *str,
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

  if (validate_string(charset_info, string_val.str, string_val.length,
                      &valid_len, &len_error)) {
    char hexbuf[7];
    octet2hex(
        hexbuf, string_val.str + valid_len,
        static_cast<uint>(std::min<size_t>(string_val.length - valid_len, 3)));
    my_error(ER_INVALID_CHARACTER_STRING, MYF(0), charset_info->csname, hexbuf);
    return true;
  }
  return false;
}

/**
   Check if the given string is invalid using the system charset.

   @param       string_val       Reference to the string.
   @param       charset_info     Pointer to charset info.
   @param[out]  invalid_sub_str  If string has an invalid encoding then invalid
                                 string in printable ASCII format is stored.

   @return true if the string has an invalid encoding using
                the system charset else false.
*/

inline bool is_invalid_string(const LEX_CSTRING &string_val,
                              const CHARSET_INFO *charset_info,
                              std::string &invalid_sub_str) {
  size_t valid_len;
  bool len_error;

  if (validate_string(charset_info, string_val.str, string_val.length,
                      &valid_len, &len_error)) {
    char printable_buff[32];
    convert_to_printable(
        printable_buff, sizeof(printable_buff), string_val.str + valid_len,
        static_cast<uint>(std::min<size_t>(string_val.length - valid_len, 3)),
        charset_info, 3);
    invalid_sub_str = printable_buff;
    return true;
  }
  return false;
}

/**
  In debug mode, verify that we're not adding an item twice to the fields list
  with inconsistent hidden flags. Must be called before adding the item to
  fields.
 */
inline void assert_consistent_hidden_flags(const mem_root_deque<Item *> &fields
                                           [[maybe_unused]],
                                           Item *item [[maybe_unused]],
                                           bool hidden [[maybe_unused]]) {
#ifndef NDEBUG
  if (std::find(fields.begin(), fields.end(), item) != fields.end()) {
    // The item is already in the list, so we can't add it
    // with a different value for hidden.
    assert(item->hidden == hidden);
  }
#endif
}

bool walk_item(Item *item, Select_lex_visitor *visitor);
bool accept_for_order(SQL_I_List<ORDER> orders, Select_lex_visitor *visitor);
bool accept_table(Table_ref *t, Select_lex_visitor *visitor);
bool accept_for_join(mem_root_deque<Table_ref *> *tables,
                     Select_lex_visitor *visitor);
Table_ref *nest_join(THD *thd, Query_block *select, Table_ref *embedding,
                     mem_root_deque<Table_ref *> *jlist, size_t table_cnt,
                     const char *legend);
#endif /* SQL_LEX_INCLUDED */
