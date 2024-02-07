/* Copyright (c) 2006, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file
  File containing constants that can be used throughout the server.

  @note This file shall not contain any includes of sql/xxx.h files.
*/

#ifndef SQL_CONST_INCLUDED
#define SQL_CONST_INCLUDED

#include <float.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "my_config.h"     // MAX_INDEXES
#include "my_table_map.h"  // table_map

constexpr const int MAX_ALIAS_NAME{256};
constexpr const int MAX_FIELD_NAME{34}; /* Max column name length +2 */

constexpr const unsigned int MAX_KEY{MAX_INDEXES}; /* Max used keys */
constexpr const unsigned int MAX_REF_PARTS{16};    /* Max parts used as ref */
constexpr const unsigned int MAX_KEY_LENGTH{3072}; /* max possible key */

constexpr const int MAX_FIELD_CHARLENGTH{255};
constexpr const int MAX_FIELD_VARCHARLENGTH{65535};

/* cf Field_blob::get_length() */
constexpr const unsigned int MAX_FIELD_BLOBLENGTH{
    std::numeric_limits<uint32_t>::max()};

/**
  CHAR and VARCHAR fields longer than this number of characters are converted
  to BLOB.
  Non-character fields longer than this number of bytes are converted to BLOB.
  Comparisons should be '>' or '<='.
*/
constexpr const int CONVERT_IF_BIGGER_TO_BLOB{512};

/** Max column width + 1. 3 is mbmaxlen for utf8mb3 */
constexpr const int MAX_FIELD_WIDTH{MAX_FIELD_CHARLENGTH * 3 + 1};

/** YYYY-MM-DD */
constexpr const int MAX_DATE_WIDTH{10};
/** -838:59:59 */
constexpr const int MAX_TIME_WIDTH{10};
/** -DDDDDD HH:MM:SS.###### */
constexpr const int MAX_TIME_FULL_WIDTH{23};
/** YYYY-MM-DD HH:MM:SS.###### AM */
constexpr const int MAX_DATETIME_FULL_WIDTH{29};
/** YYYY-MM-DD HH:MM:SS */
constexpr const int MAX_DATETIME_WIDTH{19};

/**
  MAX_TABLES and xxx_TABLE_BIT are used in optimization of table factors and
  expressions, and in join plan generation.
  MAX_TABLES counts the maximum number of tables that can be handled in a
  join operation. It is the number of bits in the table_map, minus the
  number of pseudo table bits (bits that do not represent actual tables, but
  still need to be handled by our algorithms). The pseudo table bits are:
  INNER_TABLE_BIT is set for all expressions that contain a parameter,
  a subquery that accesses tables, or a function that accesses tables.
  An expression that has only INNER_TABLE_BIT is constant for the duration
  of a query expression, but must be evaluated at least once during execution.
  OUTER_REF_TABLE_BIT is set for expressions that contain a column that
  is resolved as an outer reference. Also notice that all subquery items
  between the column reference and the query block where the column is
  resolved, have this bit set. Expressions that are represented by this bit
  are constant for the duration of the subquery they are defined in.
  RAND_TABLE_BIT is set for expressions containing a non-deterministic
  element, such as a random function or a non-deterministic function.
  Expressions containing this bit cannot be evaluated once and then cached,
  they must be evaluated at latest possible point.
  RAND_TABLE_BIT is also piggy-backed to avoid moving Item_func_reject_if
  from its join condition. This usage is similar to its use by
  Item_is_not_null_test.
  MAX_TABLES_FOR_SIZE adds the pseudo bits and is used for sizing purposes only.
*/
/** Use for sizing ONLY */
constexpr const size_t MAX_TABLES_FOR_SIZE{sizeof(table_map) * 8};

/** Max tables in join */
constexpr const size_t MAX_TABLES{MAX_TABLES_FOR_SIZE - 3};

constexpr const table_map INNER_TABLE_BIT{1ULL << (MAX_TABLES + 0)};
constexpr const table_map OUTER_REF_TABLE_BIT{1ULL << (MAX_TABLES + 1)};
constexpr const table_map RAND_TABLE_BIT{1ULL << (MAX_TABLES + 2)};
constexpr const table_map PSEUDO_TABLE_BITS{
    INNER_TABLE_BIT | OUTER_REF_TABLE_BIT | RAND_TABLE_BIT};

/** Maximum number of columns */
constexpr const int MAX_FIELDS{4096};
constexpr const int MAX_PARTITIONS{8192};

/** Max length of enum/set values */
constexpr const int MAX_INTERVAL_VALUE_LENGTH{255};

constexpr const size_t MIN_SORT_MEMORY{32 * 1024};

constexpr const size_t STRING_BUFFER_USUAL_SIZE{80};

/** Memory allocated when parsing a statement */
constexpr const size_t MEM_ROOT_BLOCK_SIZE{8192};

/** Default mode on new files */
constexpr const int CREATE_MODE{0};

constexpr const size_t MAX_PASSWORD_LENGTH{32};

/**
  Stack reservation.
  Feel free to raise this by the smallest amount you can to get the
  "execution_constants" test to pass.
*/
#if defined HAVE_UBSAN && SIZEOF_CHARP == 4
constexpr const long STACK_MIN_SIZE{30000};  // Abort if less stack during eval.
#else
constexpr const long STACK_MIN_SIZE{20000};  // Abort if less stack during eval.
#endif

constexpr const int STACK_BUFF_ALLOC{352};  ///< For stack overrun checks

constexpr const size_t ACL_ALLOC_BLOCK_SIZE{1024};
constexpr const size_t TABLE_ALLOC_BLOCK_SIZE{1024};

constexpr const int PRECISION_FOR_DOUBLE{53};
constexpr const int PRECISION_FOR_FLOAT{24};

/** -[digits].E+## */
constexpr const int MAX_FLOAT_STR_LENGTH{FLT_DIG + 6};
/** -[digits].E+### */
constexpr const int MAX_DOUBLE_STR_LENGTH{DBL_DIG + 7};

constexpr const unsigned long LONG_TIMEOUT{3600 * 24 * 365};

/*
  Flags below are set when we perform
  context analysis of the statement and make
  subqueries non-const. It prevents subquery
  evaluation at context analysis stage.
*/

/**
  Don't evaluate this subquery during statement prepare even if
  it's a constant one. The flag is switched off in the end of
  mysqld_stmt_prepare.
*/
constexpr const uint8_t CONTEXT_ANALYSIS_ONLY_PREPARE{1};
/**
  Special Query_block::prepare mode: changing of query is prohibited.
  When creating a view, we need to just check its syntax omitting
  any optimizations: afterwards definition of the view will be
  reconstructed by means of ::print() methods and written to
  to an .frm file. We need this definition to stay untouched.
*/
constexpr const uint8_t CONTEXT_ANALYSIS_ONLY_VIEW{2};
/**
  Don't evaluate this subquery during derived table prepare even if
  it's a constant one.
*/
constexpr const uint8_t CONTEXT_ANALYSIS_ONLY_DERIVED{4};

/**
   @@optimizer_switch flags.
   These must be in sync with optimizer_switch_typelib
 */
constexpr const uint64_t OPTIMIZER_SWITCH_INDEX_MERGE{1ULL << 0};
constexpr const uint64_t OPTIMIZER_SWITCH_INDEX_MERGE_UNION{1ULL << 1};
constexpr const uint64_t OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION{1ULL << 2};
constexpr const uint64_t OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT{1ULL << 3};
constexpr const uint64_t OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN{1ULL << 4};
constexpr const uint64_t OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN{1ULL << 5};
/** If this is off, MRR is never used. */
constexpr const uint64_t OPTIMIZER_SWITCH_MRR{1ULL << 6};
/**
   If OPTIMIZER_SWITCH_MRR is on and this is on, MRR is used depending on a
   cost-based choice ("automatic"). If OPTIMIZER_SWITCH_MRR is on and this is
   off, MRR is "forced" (i.e. used as long as the storage engine is capable of
   doing it).
*/
constexpr const uint64_t OPTIMIZER_SWITCH_MRR_COST_BASED{1ULL << 7};
constexpr const uint64_t OPTIMIZER_SWITCH_BNL{1ULL << 8};
constexpr const uint64_t OPTIMIZER_SWITCH_BKA{1ULL << 9};
constexpr const uint64_t OPTIMIZER_SWITCH_MATERIALIZATION{1ULL << 10};
constexpr const uint64_t OPTIMIZER_SWITCH_SEMIJOIN{1ULL << 11};
constexpr const uint64_t OPTIMIZER_SWITCH_LOOSE_SCAN{1ULL << 12};
constexpr const uint64_t OPTIMIZER_SWITCH_FIRSTMATCH{1ULL << 13};
constexpr const uint64_t OPTIMIZER_SWITCH_DUPSWEEDOUT{1ULL << 14};
constexpr const uint64_t OPTIMIZER_SWITCH_SUBQ_MAT_COST_BASED{1ULL << 15};
constexpr const uint64_t OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS{1ULL << 16};
constexpr const uint64_t OPTIMIZER_SWITCH_COND_FANOUT_FILTER{1ULL << 17};
constexpr const uint64_t OPTIMIZER_SWITCH_DERIVED_MERGE{1ULL << 18};
constexpr const uint64_t OPTIMIZER_SWITCH_USE_INVISIBLE_INDEXES{1ULL << 19};
constexpr const uint64_t OPTIMIZER_SKIP_SCAN{1ULL << 20};
constexpr const uint64_t OPTIMIZER_SWITCH_HASH_JOIN{1ULL << 21};
constexpr const uint64_t OPTIMIZER_SWITCH_SUBQUERY_TO_DERIVED{1ULL << 22};
constexpr const uint64_t OPTIMIZER_SWITCH_PREFER_ORDERING_INDEX{1ULL << 23};
constexpr const uint64_t OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER{1ULL << 24};
constexpr const uint64_t OPTIMIZER_SWITCH_DERIVED_CONDITION_PUSHDOWN{1ULL
                                                                     << 25};
constexpr const uint64_t OPTIMIZER_SWITCH_HASH_SET_OPERATIONS{1ULL << 26};
constexpr const uint64_t OPTIMIZER_SWITCH_LAST{1ULL << 27};

enum SHOW_COMP_OPTION { SHOW_OPTION_YES, SHOW_OPTION_NO, SHOW_OPTION_DISABLED };

enum enum_mark_columns {
  MARK_COLUMNS_NONE,
  MARK_COLUMNS_READ,
  MARK_COLUMNS_WRITE,
  MARK_COLUMNS_TEMP
};

/**
  Exit code used by mysqld_exit, exit and _exit function
  to indicate successful termination of mysqld.
*/
constexpr const int MYSQLD_SUCCESS_EXIT{0};
/**
  Exit code used by mysqld_exit, exit and _exit function to
  signify unsuccessful termination of mysqld. The exit
  code signifies the server should NOT BE RESTARTED AUTOMATICALLY
  by init systems like systemd.
*/
constexpr const int MYSQLD_ABORT_EXIT{1};
/**
  Exit code used by mysqld_exit, exit and _exit function to
  signify unsuccessful termination of mysqld. The exit code
  signifies the server should be RESTARTED AUTOMATICALLY by
  init systems like systemd.
*/
constexpr const int MYSQLD_FAILURE_EXIT{2};
/**
  Exit code used by mysqld_exit, my_thread_exit function which allows
  for external programs like systemd, mysqld_safe to restart mysqld
  server. The exit code 16 is chosen so it is safe as InnoDB code
  exit directly with values like 3.
*/
constexpr const int MYSQLD_RESTART_EXIT{16};

constexpr const size_t UUID_LENGTH{8 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 12};

/**
  This enumeration type is used only by the function find_item_in_list
  to return the info on how an item has been resolved against a list
  of possibly aliased items.
  The item can be resolved:
   - against an alias name of the list's element (RESOLVED_AGAINST_ALIAS)
   - against non-aliased field name of the list  (RESOLVED_WITH_NO_ALIAS)
   - against an aliased field name of the list   (RESOLVED_BEHIND_ALIAS)
   - ignoring the alias name in cases when SQL requires to ignore aliases
     (e.g. when the resolved field reference contains a table name or
     when the resolved item is an expression)   (RESOLVED_IGNORING_ALIAS)
*/
enum enum_resolution_type {
  NOT_RESOLVED = 0,
  RESOLVED_BEHIND_ALIAS,
  RESOLVED_AGAINST_ALIAS,
  RESOLVED_WITH_NO_ALIAS,
  RESOLVED_IGNORING_ALIAS
};

/// Enumeration for {Item,Query_block[_UNIT],Table_function}::walk
enum class enum_walk {
  PREFIX = 0x01,
  POSTFIX = 0x02,
  SUBQUERY = 0x04,
  SUBQUERY_PREFIX = 0x05,  // Combine prefix and subquery traversal
  SUBQUERY_POSTFIX = 0x06  // Combine postfix and subquery traversal
};

inline enum_walk operator|(enum_walk lhs, enum_walk rhs) {
  return enum_walk(int(lhs) | int(rhs));
}

inline bool operator&(enum_walk lhs, enum_walk rhs) {
  return (int(lhs) & int(rhs)) != 0;
}

class Item;
/// Processor type for {Item,Query_block[_UNIT],Table_function}::walk
using Item_processor = bool (Item::*)(unsigned char *);

/// Enumeration for Query_block::condition_context.
/// If the expression being resolved belongs to a condition clause (WHERE, etc),
/// it is connected to the clause's root through a chain of Items; tells if this
/// chain matches ^(AND)*$ ("is top-level"), ^(AND|OR)*$, or neither.
enum class enum_condition_context {
  NEITHER,
  ANDS,
  ANDS_ORS,
};

/// Used to uniquely name expressions in derived tables
#define SYNTHETIC_FIELD_NAME "Name_exp_"
#endif /* SQL_CONST_INCLUDED */
