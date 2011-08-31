/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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
  Generates a warning that a feature is deprecated. After a specified
  version asserts that the feature is removed.

  Using it as

  WARN_DEPRECATED(thd, 6,2, "BAD", "'GOOD'");

  Will result in a warning
 
  "The syntax 'BAD' is deprecated and will be removed in MySQL 6.2. Please
   use 'GOOD' instead"

   Note that in macro arguments BAD is not quoted, while 'GOOD' is.
   Note that the version is TWO numbers, separated with a comma
   (two macro arguments, that is)
*/
#define WARN_DEPRECATED(Thd,VerHi,VerLo,Old,New)                            \
  do {                                                                      \
    compile_time_assert(MYSQL_VERSION_ID < VerHi * 10000 + VerLo * 100);    \
    if (((THD *) Thd) != NULL)                                              \
      push_warning_printf(((THD *) Thd), MYSQL_ERROR::WARN_LEVEL_WARN,      \
                        ER_WARN_DEPRECATED_SYNTAX,                          \
                        ER(ER_WARN_DEPRECATED_SYNTAX),                      \
                        (Old), (New));                                      \
    else                                                                    \
      sql_print_warning("The syntax '%s' is deprecated and will be removed " \
                        "in a future release. Please use %s instead.",      \
                        (Old), (New));                                      \
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
#define OPTION_KEEP_LOG         (1ULL << 23)    // THD, user

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

/* The rest of the file is included in the server only */
#ifndef MYSQL_CLIENT

/* @@optimizer_switch flags. These must be in sync with optimizer_switch_typelib */
#define OPTIMIZER_SWITCH_INDEX_MERGE               (1ULL << 0)
#define OPTIMIZER_SWITCH_INDEX_MERGE_UNION         (1ULL << 1)
#define OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION    (1ULL << 2)
#define OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT     (1ULL << 3)
#define OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN (1ULL << 4)
#define OPTIMIZER_SWITCH_LAST                      (1ULL << 5)

/* The following must be kept in sync with optimizer_switch_str in mysqld.cc */
#define OPTIMIZER_SWITCH_DEFAULT (OPTIMIZER_SWITCH_INDEX_MERGE | \
                                  OPTIMIZER_SWITCH_INDEX_MERGE_UNION | \
                                  OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION | \
                                  OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT | \
                                  OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN)


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

/* Used to check GROUP BY list in the MODE_ONLY_FULL_GROUP_BY mode */
#define UNDEF_POS (-1)

/* BINLOG_DUMP options */

#define BINLOG_DUMP_NON_BLOCK   1

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
