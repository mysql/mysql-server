/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file

  @details
  Mostly this file is used in the server. But a little part of it is used in
  mysqlbinlog too (definition of SELECT_DISTINCT and others).
  The consequence is that 90% of the file is wrapped in \#ifndef MYSQL_CLIENT,
  except the part which must be in the server and in the client.
*/

#ifndef SQL_PRIV_INCLUDED
#define SQL_PRIV_INCLUDED

#ifndef MYSQL_CLIENT

/*
  Generates a warning that a feature is deprecated. 

  Using it as

  WARN_DEPRECATED(thd, "BAD", "'GOOD'");

  Will result in a warning
 
  "The syntax 'BAD' is deprecated and will be removed in a
   future release. Please use 'GOOD' instead"

   Note that in macro arguments BAD is not quoted, while 'GOOD' is.
*/
#define WARN_DEPRECATED(Thd,Old,New)                            \
  do {                                                                      \
    if (((THD *) Thd) != NULL)                                              \
      push_warning_printf(((THD *) Thd), Sql_condition::WARN_LEVEL_WARN,    \
                        ER_WARN_DEPRECATED_SYNTAX,                          \
                        ER(ER_WARN_DEPRECATED_SYNTAX),                      \
                        (Old), (New));                                      \
    else                                                                    \
      sql_print_warning("The syntax '%s' is deprecated and will be removed " \
                        "in a future release. Please use %s instead.",      \
                        (Old), (New));                                      \
  } while(0)


/*
  Generates a warning that a feature is deprecated and there is no replacement.

  Using it as

  WARN_DEPRECATED_NO_REPLACEMENT(thd, "BAD");

  Will result in a warning
 
  "'BAD' is deprecated and will be removed in a future release."

   Note that in macro arguments BAD is not quoted.
*/

#define WARN_DEPRECATED_NO_REPLACEMENT(Thd,Old)                             \
  do {                                                                      \
    if (((THD *) Thd) != NULL)                                              \
      push_warning_printf(((THD *) Thd), Sql_condition::WARN_LEVEL_WARN,    \
                        ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,           \
                        ER(ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),       \
                        (Old));                                             \
    else                                                                    \
      sql_print_warning("'%s' is deprecated and will be removed "           \
                        "in a future release.", (Old));                     \
  } while(0)

/*************************************************************************/

#endif

/*
   This is included in the server and in the client.
   Options for select set by the yacc parser (stored in lex->options).

   NOTE
   log_event.h defines OPTIONS_WRITTEN_TO_BIN_LOG to specify what THD
   options list are written into binlog. These options can NOT change their
   values, or it will break replication between version.

   context is encoded as following:
   SELECT - SELECT_LEX_NODE::options
   THD    - THD::options
   intern - neither. used only as
            func(..., select_node->options | thd->options | OPTION_XXX, ...)

   TODO: separate three contexts above, move them to separate bitfields.
*/

#define SELECT_DISTINCT         (1ULL << 0)     // SELECT, user
#define SELECT_STRAIGHT_JOIN    (1ULL << 1)     // SELECT, user
#define SELECT_DESCRIBE         (1ULL << 2)     // SELECT, user
#define SELECT_SMALL_RESULT     (1ULL << 3)     // SELECT, user
#define SELECT_BIG_RESULT       (1ULL << 4)     // SELECT, user
#define OPTION_FOUND_ROWS       (1ULL << 5)     // SELECT, user
#define OPTION_TO_QUERY_CACHE   (1ULL << 6)     // SELECT, user
#define SELECT_NO_JOIN_CACHE    (1ULL << 7)     // intern
/** always the opposite of OPTION_NOT_AUTOCOMMIT except when in fix_autocommit() */
#define OPTION_AUTOCOMMIT       (1ULL << 8)    // THD, user
#define OPTION_BIG_SELECTS      (1ULL << 9)     // THD, user
#define OPTION_LOG_OFF          (1ULL << 10)    // THD, user
#define OPTION_QUOTE_SHOW_CREATE (1ULL << 11)   // THD, user, unused
#define TMP_TABLE_ALL_COLUMNS   (1ULL << 12)    // SELECT, intern
#define OPTION_WARNINGS         (1ULL << 13)    // THD, user
#define OPTION_AUTO_IS_NULL     (1ULL << 14)    // THD, user, binlog
#define OPTION_FOUND_COMMENT    (1ULL << 15)    // SELECT, intern, parser
#define OPTION_SAFE_UPDATES     (1ULL << 16)    // THD, user
#define OPTION_BUFFER_RESULT    (1ULL << 17)    // SELECT, user
#define OPTION_BIN_LOG          (1ULL << 18)    // THD, user
#define OPTION_NOT_AUTOCOMMIT   (1ULL << 19)    // THD, user
#define OPTION_BEGIN            (1ULL << 20)    // THD, intern
#define OPTION_TABLE_LOCK       (1ULL << 21)    // THD, intern
#define OPTION_QUICK            (1ULL << 22)    // SELECT (for DELETE)
/* 23rd bit is unused. It was occupied by OPTION_KEEP_LOG. */

/* The following is used to detect a conflict with DISTINCT */
#define SELECT_ALL              (1ULL << 24)    // SELECT, user, parser
/** The following can be set when importing tables in a 'wrong order'
   to suppress foreign key checks */
#define OPTION_NO_FOREIGN_KEY_CHECKS    (1ULL << 26) // THD, user, binlog
/** The following speeds up inserts to InnoDB tables by suppressing unique
   key checks in some cases */
#define OPTION_RELAXED_UNIQUE_CHECKS    (1ULL << 27) // THD, user, binlog
#define SELECT_NO_UNLOCK                (1ULL << 28) // SELECT, intern
#define OPTION_SCHEMA_TABLE             (1ULL << 29) // SELECT, intern
/** Flag set if setup_tables already done */
#define OPTION_SETUP_TABLES_DONE        (1ULL << 30) // intern
/** If not set then the thread will ignore all warnings with level notes. */
#define OPTION_SQL_NOTES                (1ULL << 31) // THD, user
/**
  Force the used temporary table to be a MyISAM table (because we will use
  fulltext functions when reading from it.
*/
#define TMP_TABLE_FORCE_MYISAM          (1ULL << 32)
#define OPTION_PROFILING                (1ULL << 33)
/**
  Indicates that this is a HIGH_PRIORITY SELECT.
  Currently used only for printing of such selects.
  Type of locks to be acquired is specified directly.
*/
#define SELECT_HIGH_PRIORITY            (1ULL << 34)     // SELECT, user
/**
  Is set in slave SQL thread when there was an
  error on master, which, when is not reproducible
  on slave (i.e. the query succeeds on slave),
  is not terminal to the state of repliation,
  and should be ignored. The slave SQL thread,
  however, needs to rollback the effects of the
  succeeded statement to keep replication consistent.
*/
#define OPTION_MASTER_SQL_ERROR (1ULL << 35)

/*
  Dont report errors for individual rows,
  But just report error on commit (or read ofcourse)
  Note! Reserved for use in MySQL Cluster
*/
#define OPTION_ALLOW_BATCH              (ULL(1) << 36) // THD, intern (slave)

/*
  Check how many bytes are available on buffer.

  @param buf_start    Pointer to buffer start.
  @param buf_current  Pointer to the current position on buffer.
  @param buf_len      Buffer length.

  @return             Number of bytes available on event buffer.
*/
template <class T> T available_buffer(const char* buf_start,
                                      const char* buf_current,
                                      T buf_len)
{
  return buf_len - (buf_current - buf_start);
}

/*
  Check if jump value is within buffer limits.

  @param jump         Number of positions we want to advance.
  @param buf_start    Pointer to buffer start
  @param buf_current  Pointer to the current position on buffer.
  @param buf_len      Buffer length.

  @return      True   If jump value is within buffer limits.
               False  Otherwise.
*/
template <class T> bool valid_buffer_range(T jump,
                                           const char* buf_start,
                                           const char* buf_current,
                                           T buf_len)
{
  return (jump <= available_buffer(buf_start, buf_current, buf_len));
}

/* The rest of the file is included in the server only */
#ifndef MYSQL_CLIENT

/* @@optimizer_switch flags. These must be in sync with optimizer_switch_typelib */
#define OPTIMIZER_SWITCH_INDEX_MERGE               (1ULL << 0)
#define OPTIMIZER_SWITCH_INDEX_MERGE_UNION         (1ULL << 1)
#define OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION    (1ULL << 2)
#define OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT     (1ULL << 3)
#define OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN (1ULL << 4)
#define OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN  (1ULL << 5)
/** If this is off, MRR is never used. */
#define OPTIMIZER_SWITCH_MRR                       (1ULL << 6)
/**
   If OPTIMIZER_SWITCH_MRR is on and this is on, MRR is used depending on a
   cost-based choice ("automatic"). If OPTIMIZER_SWITCH_MRR is on and this is
   off, MRR is "forced" (i.e. used as long as the storage engine is capable of
   doing it).
*/
#define OPTIMIZER_SWITCH_MRR_COST_BASED            (1ULL << 7)
#define OPTIMIZER_SWITCH_BNL                       (1ULL << 8)
#define OPTIMIZER_SWITCH_BKA                       (1ULL << 9)
#define OPTIMIZER_SWITCH_MATERIALIZATION           (1ULL << 10)
#define OPTIMIZER_SWITCH_SEMIJOIN                  (1ULL << 11)
#define OPTIMIZER_SWITCH_LOOSE_SCAN                (1ULL << 12)
#define OPTIMIZER_SWITCH_FIRSTMATCH                (1ULL << 13)
#define OPTIMIZER_SWITCH_SUBQ_MAT_COST_BASED       (1ULL << 14)
#define OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS      (1ULL << 15)
#define OPTIMIZER_SWITCH_LAST                      (1ULL << 16)

/**
   If OPTIMIZER_SWITCH_ALL is defined, optimizer_switch flags for newer 
   optimizer features (semijoin) will be available.
 */
#define OPTIMIZER_SWITCH_ALL 1

/* 
  The following must be kept in sync with optimizer_switch string in 
  sys_vars.cc.
*/
#ifdef OPTIMIZER_SWITCH_ALL
#define OPTIMIZER_SWITCH_DEFAULT (OPTIMIZER_SWITCH_INDEX_MERGE | \
                                  OPTIMIZER_SWITCH_INDEX_MERGE_UNION | \
                                  OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION | \
                                  OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT | \
                                  OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN |\
                                  OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN | \
                                  OPTIMIZER_SWITCH_MRR | \
                                  OPTIMIZER_SWITCH_MRR_COST_BASED | \
                                  OPTIMIZER_SWITCH_BNL | \
                                  OPTIMIZER_SWITCH_MATERIALIZATION | \
                                  OPTIMIZER_SWITCH_SEMIJOIN | \
                                  OPTIMIZER_SWITCH_LOOSE_SCAN | \
                                  OPTIMIZER_SWITCH_FIRSTMATCH | \
                                  OPTIMIZER_SWITCH_SUBQ_MAT_COST_BASED | \
                                  OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS)
#else
#define OPTIMIZER_SWITCH_DEFAULT (OPTIMIZER_SWITCH_INDEX_MERGE | \
                                  OPTIMIZER_SWITCH_INDEX_MERGE_UNION | \
                                  OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION | \
                                  OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT | \
                                  OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN |\
                                  OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN | \
                                  OPTIMIZER_SWITCH_MRR | \
                                  OPTIMIZER_SWITCH_MRR_COST_BASED | \
                                  OPTIMIZER_SWITCH_BNL | \
                                  OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS)
#endif
/*
  Replication uses 8 bytes to store SQL_MODE in the binary log. The day you
  use strictly more than 64 bits by adding one more define above, you should
  contact the replication team because the replication code should then be
  updated (to store more bytes on disk).

  NOTE: When adding new SQL_MODE types, make sure to also add them to
  the scripts used for creating the MySQL system tables
  in scripts/mysql_system_tables.sql and scripts/mysql_system_tables_fix.sql

*/

/*
  Flags below are set when we perform
  context analysis of the statement and make
  subqueries non-const. It prevents subquery
  evaluation at context analysis stage.
*/

/*
  Don't evaluate this subquery during statement prepare even if
  it's a constant one. The flag is switched off in the end of
  mysqld_stmt_prepare.
*/ 
#define CONTEXT_ANALYSIS_ONLY_PREPARE 1
/*
  Special JOIN::prepare mode: changing of query is prohibited.
  When creating a view, we need to just check its syntax omitting
  any optimizations: afterwards definition of the view will be
  reconstructed by means of ::print() methods and written to
  to an .frm file. We need this definition to stay untouched.
*/ 
#define CONTEXT_ANALYSIS_ONLY_VIEW    2
/*
  Don't evaluate this subquery during derived table prepare even if
  it's a constant one.
*/
#define CONTEXT_ANALYSIS_ONLY_DERIVED 4

// uncachable cause
#define UNCACHEABLE_DEPENDENT   1
#define UNCACHEABLE_RAND        2
#define UNCACHEABLE_SIDEEFFECT	4
/// forcing to save JOIN for explain
#define UNCACHEABLE_EXPLAIN     8
/* For uncorrelated SELECT in an UNION with some correlated SELECTs */
#define UNCACHEABLE_UNITED     16
#define UNCACHEABLE_CHECKOPTION 32

/*
  Some defines for exit codes for ::is_equal class functions.
*/
#define IS_EQUAL_NO 0
#define IS_EQUAL_YES 1
#define IS_EQUAL_PACK_LENGTH 2

enum enum_parsing_place
{
  NO_MATTER,
  IN_HAVING,
  SELECT_LIST,
  IN_WHERE,
  IN_ON
};


enum enum_var_type
{
  OPT_DEFAULT= 0, OPT_SESSION, OPT_GLOBAL
};

class sys_var;

enum enum_yes_no_unknown
{
  TVL_YES, TVL_NO, TVL_UNKNOWN
};

#ifdef MYSQL_SERVER

#endif /* MYSQL_SERVER */

#ifdef MYSQL_SERVER
/*
  External variables
*/

/* sql_yacc.cc */
#ifndef DBUG_OFF
extern void turn_parser_debug_on();

#endif

/**
  convert a hex digit into number.
*/

inline int hexchar_to_int(char c)
{
  if (c <= '9' && c >= '0')
    return c-'0';
  c|=32;
  if (c <= 'f' && c >= 'a')
    return c-'a'+10;
  return -1;
}

/* This must match the path length limit in the ER_NOT_RW_DIR error msg. */
#define ER_NOT_RW_DIR_PATHSIZE 200

#define IS_TABLESPACES_TABLESPACE_NAME    0
#define IS_TABLESPACES_ENGINE             1
#define IS_TABLESPACES_TABLESPACE_TYPE    2
#define IS_TABLESPACES_LOGFILE_GROUP_NAME 3
#define IS_TABLESPACES_EXTENT_SIZE        4
#define IS_TABLESPACES_AUTOEXTEND_SIZE    5
#define IS_TABLESPACES_MAXIMUM_SIZE       6
#define IS_TABLESPACES_NODEGROUP_ID       7
#define IS_TABLESPACES_TABLESPACE_COMMENT 8

#endif /* MYSQL_SERVER */

#endif /* MYSQL_CLIENT */

#endif /* SQL_PRIV_INCLUDED */
