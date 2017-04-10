/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/* sql_yacc.yy */

/**
  @defgroup Parser Parser
  @{
*/

%{
/*
Note: YYTHD is passed as an argument to yyparse(), and subsequently to yylex().
*/
#define YYP (YYTHD->m_parser_state)
#define YYLIP (& YYTHD->m_parser_state->m_lip)
#define YYPS (& YYTHD->m_parser_state->m_yacc)
#define YYCSCL (YYLIP->query_charset)
#define YYMEM_ROOT (YYTHD->mem_root)

#define YYINITDEPTH 100
#define YYMAXDEPTH 3200                        /* Because of 64K stack */
#define Lex (YYTHD->lex)
#define Select Lex->current_select()
#include "auth_common.h"                      /* *_ACL */
#include "binlog.h"                          // for MAX_LOG_UNIQUE_FN_EXT
#include "dd/info_schema/show.h"             // build_show_...
#include "dd/types/abstract_table.h"         // TT_BASE_TABLE
#include "derror.h"
#include "event_parse_data.h"
#include "item_cmpfunc.h"
#include "item_create.h"
#include "item_geofunc.h"
#include "item_json_func.h"
#include "key_spec.h"
#include "keycaches.h"
#include "lex_symbol.h"
#include "lex_token.h"
#include "log_event.h"
#include "my_dbug.h"
#include "myisam.h"
#include "myisammrg.h"
#include "mysqld.h"        // slave_net_timeout national_charset_info ...
#include "opt_explain_json.h"
#include "opt_explain_traditional.h"
#include "parse_location.h"
#include "parse_tree_helpers.h"
#include "parse_tree_hints.h"
#include "partition_info.h"                   /* partition_info */
#include "password.h"       /* my_make_scrambled_password_323, my_make_scrambled_password */
#include "rpl_filter.h"
#include "rpl_msr.h"       /* multisource replication */
#include "rpl_slave.h"
#include "rpl_slave.h"                       // Sql_cmd_change_repl_filter
#include "set_var.h"
#include "sp.h"
#include "sp_head.h"
#include "sp_instr.h"
#include "sp_pcontext.h"
#include "sp_rcontext.h"
#include "sql_admin.h"                         // Sql_cmd_analyze/Check..._table
#include "sql_alter.h"                         // Sql_cmd_alter_table*
#include "sql_base.h"                        // find_temporary_table
#include "sql_class.h"      /* Key_part_spec, enum_filetype */
#include "sql_component.h"
#include "sql_import.h"                        // Sql_cmd_import_table
#include "sql_get_diagnostics.h"               // Sql_cmd_get_diagnostics
#include "sql_handler.h"                       // Sql_cmd_handler_*
#include "sql_parse.h"                        /* comp_*_creator */
#include "sql_partition.h"                    /* mem_alloc_error */
#include "sql_partition_admin.h"               // Sql_cmd_alter_table_*_part.
#include "sql_plugin.h"                      // plugin_is_ready
#include "sql_select.h"                        // Sql_cmd_select...
#include "sql_servers.h"
#include "sql_show_status.h"                 // build_show_session_status, ...
#include "sql_signal.h"
#include "sql_table.h"                        /* primary_key_name */
#include "sql_trigger.h"                     // Sql_cmd_create_trigger,
                                             // Sql_cmd_create_trigger
#include "sql_truncate.h"                      // Sql_cmd_truncate_table
                                             // used in RESET_MASTER parsing check
/* this is to get the bison compilation windows warnings out */
#ifdef _MSC_VER
/* warning C4065: switch statement contains 'default' but no 'case' labels */
#pragma warning (disable : 4065)
#endif

using std::min;
using std::max;

int yylex(void *yylval, void *yythd);

#define yyoverflow(A,B,C,D,E,F,G,H)           \
  {                                           \
    ulong val= *(H);                          \
    if (my_yyoverflow((B), (D), (F), &val))   \
    {                                         \
      yyerror(NULL, YYTHD, (char*) (A));      \
      return 2;                               \
    }                                         \
    else                                      \
    {                                         \
      *(H)= (YYSIZE_T)val;                    \
    }                                         \
  }

#define MYSQL_YYABORT                         \
  do                                          \
  {                                           \
    LEX::cleanup_lex_after_parse_error(YYTHD);\
    YYABORT;                                  \
  } while (0)

#define MYSQL_YYABORT_UNLESS(A)         \
  if (!(A))                             \
  {                                     \
    my_syntax_error(YYTHD, ER_THD(YYTHD,ER_SYNTAX_ERROR));\
    MYSQL_YYABORT;                      \
  }

#define NEW_PTN new(YYMEM_ROOT)


/**
  Parse_tree_node::contextualize_() function call wrapper
*/
#define TMP_CONTEXTUALIZE(x)        \
  do                                \
  {                                 \
    Parse_context pc(YYTHD, Select);\
    if ((x)->contextualize_(&pc))   \
      MYSQL_YYABORT;                \
  } while(0)


/**
  Parse_tree_node::contextualize() function call wrapper
*/
#define CONTEXTUALIZE(x)                                \
  do                                                    \
  {                                                     \
    Parse_context pc(YYTHD, Select);                    \
    if (YYTHD->is_error() || (x)->contextualize(&pc))   \
      MYSQL_YYABORT;                                    \
  } while(0)


/**
  Item::itemize() function call wrapper
*/
#define ITEMIZE(x, y)                                  \
  do                                                   \
  {                                                    \
    Parse_context pc(YYTHD, Select);                   \
    if (YYTHD->is_error() || (x)->itemize(&pc, (y)))   \
      MYSQL_YYABORT;                                   \
  } while(0)

/**
  PT_statement::make_cmd() wrapper to raise postponed error message on OOM

  @note x may be NULL because of OOM error.
*/
#define MAKE_CMD(x)                                     \
  do                                                    \
  {                                                     \
    if (YYTHD->is_error() ||                            \
        (Lex->m_sql_cmd= (x)->make_cmd(YYTHD)) == NULL) \
      MYSQL_YYABORT;                                    \
  } while(0)


#ifndef DBUG_OFF
#define YYDEBUG 1
#else
#define YYDEBUG 0
#endif

/**
  The word DEFAULT is a reserved word, but it is treated as an identifier by
  both parser and the AST. In order to make the interfaces match up, there has
  to be a LEX_STRING for DEFAULT.
*/
static const LEX_STRING default_word =
{ STRING_WITH_LEN(const_cast<char*>("DEFAULT")) };


/**
  @brief Bison callback to report a syntax/OOM error

  This function is invoked by the bison-generated parser
  when a syntax error, a parse error or an out-of-memory
  condition occurs. This function is not invoked when the
  parser is requested to abort by semantic action code
  by means of YYABORT or YYACCEPT macros. This is why these
  macros should not be used (use MYSQL_YYABORT/MYSQL_YYACCEPT
  instead).

  The parser will abort immediately after invoking this callback.

  This function is not for use in semantic actions and is internal to
  the parser, as it performs some pre-return cleanup.
  In semantic actions, please use my_syntax_error or my_error to
  push an error into the error stack and MYSQL_YYABORT
  to abort from the parser.
*/

static void MYSQLerror(YYLTYPE *, THD *thd, const char *s)
{
  /*
    Restore the original LEX if it was replaced when parsing
    a stored procedure. We must ensure that a parsing error
    does not leave any side effects in the THD.
  */
  LEX::cleanup_lex_after_parse_error(thd);

  /* "parse error" changed into "syntax error" between bison 1.75 and 1.875 */
  if (strcmp(s,"parse error") == 0 || strcmp(s,"syntax error") == 0)
    s= ER_THD(thd, ER_SYNTAX_ERROR);
  my_syntax_error(thd, s);
}


#ifndef DBUG_OFF
void turn_parser_debug_on()
{
  /*
     MYSQLdebug is in sql/sql_yacc.cc, in bison generated code.
     Turning this option on is **VERY** verbose, and should be
     used when investigating a syntax error problem only.

     The syntax to run with bison traces is as follows :
     - Starting a server manually :
       mysqld --debug="d,parser_debug" ...
     - Running a test :
       mysql-test-run.pl --mysqld="--debug=d,parser_debug" ...

     The result will be in the process stderr (var/log/master.err)
   */

  extern int yydebug;
  yydebug= 1;
}
#endif

static bool is_native_function(const LEX_STRING &name)
{
  if (find_native_function_builder(name) != nullptr)
    return true;

  if (is_lex_native_function(&name))
    return true;

  return false;
}


/**
  Helper action for a case statement (entering the CASE).
  This helper is used for both 'simple' and 'searched' cases.
  This helper, with the other case_stmt_action_..., is executed when
  the following SQL code is parsed:
<pre>
CREATE PROCEDURE proc_19194_simple(i int)
BEGIN
  DECLARE str CHAR(10);

  CASE i
    WHEN 1 THEN SET str="1";
    WHEN 2 THEN SET str="2";
    WHEN 3 THEN SET str="3";
    ELSE SET str="unknown";
  END CASE;

  SELECT str;
END
</pre>
  The actions are used to generate the following code:
<pre>
SHOW PROCEDURE CODE proc_19194_simple;
Pos     Instruction
0       set str@1 NULL
1       set_case_expr (12) 0 i@0
2       jump_if_not 5(12) (case_expr@0 = 1)
3       set str@1 _latin1'1'
4       jump 12
5       jump_if_not 8(12) (case_expr@0 = 2)
6       set str@1 _latin1'2'
7       jump 12
8       jump_if_not 11(12) (case_expr@0 = 3)
9       set str@1 _latin1'3'
10      jump 12
11      set str@1 _latin1'unknown'
12      stmt 0 "SELECT str"
</pre>

  @param thd thread handler
*/

static void case_stmt_action_case(THD *thd)
{
  LEX *lex= thd->lex;
  sp_head *sp= lex->sphead;
  sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

  sp->m_parser_data.new_cont_backpatch();

  /*
    BACKPATCH: Creating target label for the jump to
    "case_stmt_action_end_case"
    (Instruction 12 in the example)
  */

  pctx->push_label(thd, EMPTY_STR, sp->instructions());
}

/**
  Helper action for a case then statements.
  This helper is used for both 'simple' and 'searched' cases.
  @param lex the parser lex context
*/

static bool case_stmt_action_then(THD *thd, LEX *lex)
{
  sp_head *sp= lex->sphead;
  sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

  sp_instr_jump *i =
    new (thd->mem_root) sp_instr_jump(sp->instructions(), pctx);

  if (!i || sp->add_instr(thd, i))
    return true;

  /*
    BACKPATCH: Resolving forward jump from
    "case_stmt_action_when" to "case_stmt_action_then"
    (jump_if_not from instruction 2 to 5, 5 to 8 ... in the example)
  */

  sp->m_parser_data.do_backpatch(pctx->pop_label(), sp->instructions());

  /*
    BACKPATCH: Registering forward jump from
    "case_stmt_action_then" to "case_stmt_action_end_case"
    (jump from instruction 4 to 12, 7 to 12 ... in the example)
  */

  return sp->m_parser_data.add_backpatch_entry(i, pctx->last_label());
}

/**
  Helper action for an end case.
  This helper is used for both 'simple' and 'searched' cases.
  @param lex the parser lex context
  @param simple true for simple cases, false for searched cases
*/

static void case_stmt_action_end_case(LEX *lex, bool simple)
{
  sp_head *sp= lex->sphead;
  sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

  /*
    BACKPATCH: Resolving forward jump from
    "case_stmt_action_then" to "case_stmt_action_end_case"
    (jump from instruction 4 to 12, 7 to 12 ... in the example)
  */
  sp->m_parser_data.do_backpatch(pctx->pop_label(), sp->instructions());

  if (simple)
    pctx->pop_case_expr_id();

  sp->m_parser_data.do_cont_backpatch(sp->instructions());
}


static void init_index_hints(List<Index_hint> *hints, index_hint_type type,
                             index_clause_map clause)
{
  List_iterator<Index_hint> it(*hints);
  Index_hint *hint;
  while ((hint= it++))
  {
    hint->type= type;
    hint->clause= clause;
  }
}

bool my_yyoverflow(short **a, YYSTYPE **b, YYLTYPE **c, ulong *yystacksize);

#include "parse_tree_column_attrs.h"
#include "parse_tree_items.h"
#include "parse_tree_nodes.h"
#include "parse_tree_partitions.h"

%}

%yacc

%start start_entry

%parse-param { class THD *YYTHD }
%lex-param { class THD *YYTHD }
%pure-parser                                    /* We have threads */
/*
  1. We do not accept any reduce/reduce conflicts
  2. We should not introduce new shift/reduce conflicts any more.
*/
%expect 110

/*
   MAINTAINER:

   1) Comments for TOKENS.

   For each token, please include in the same line a comment that contains
   one or more of the following tags:

   SQL-2015-R : Reserved keyword as per SQL-2015 draft
   SQL-2003-R : Reserved keyword as per SQL-2003
   SQL-2003-N : Non Reserved keyword as per SQL-2003
   SQL-1999-R : Reserved keyword as per SQL-1999
   SQL-1999-N : Non Reserved keyword as per SQL-1999
   MYSQL      : MySQL extension (unspecified)
   MYSQL-FUNC : MySQL extension, function
   INTERNAL   : Not a real token, lex optimization
   OPERATOR   : SQL operator
   FUTURE-USE : Reserved for futur use

   This makes the code grep-able, and helps maintenance.

   2) About token values

   Token values are assigned by bison, in order of declaration.

   Token values are used in query DIGESTS.
   To make DIGESTS stable, it is desirable to avoid changing token values.

   In practice, this means adding new tokens at the end of the list,
   in the current release section (8.0),
   instead of adding them in the middle of the list.

   Failing to comply with instructions below will trigger build failure,
   as this process is enforced by gen_lex_token.

   3) Instructions to add a new token:

   Add the new token at the end of the list,
   in the MySQL 8.0 section.

   4) Instructions to remove an old token:

   Do not remove the token, rename it as follows:
   %token OBSOLETE_TOKEN_<NNN> / * was: TOKEN_FOO * /
   where NNN is the token value (found in sql_yacc.h)

   For example, see OBSOLETE_TOKEN_820
*/

/*
   Tokens from MySQL 5.7, keep in alphabetical order.
*/

%token  ABORT_SYM                     /* INTERNAL (used in lex) */
%token  ACCESSIBLE_SYM
%token  ACCOUNT_SYM
%token  ACTION                        /* SQL-2003-N */
%token  ADD                           /* SQL-2003-R */
%token  ADDDATE_SYM                   /* MYSQL-FUNC */
%token  AFTER_SYM                     /* SQL-2003-N */
%token  AGAINST
%token  AGGREGATE_SYM
%token  ALGORITHM_SYM
%token  ALL                           /* SQL-2003-R */
%token  ALTER                         /* SQL-2003-R */
%token  ALWAYS_SYM
%token  OBSOLETE_TOKEN_271            /* was: ANALYSE_SYM */
%token  ANALYZE_SYM
%token  AND_AND_SYM                   /* OPERATOR */
%token  AND_SYM                       /* SQL-2003-R */
%token  ANY_SYM                       /* SQL-2003-R */
%token  AS                            /* SQL-2003-R */
%token  ASC                           /* SQL-2003-N */
%token  ASCII_SYM                     /* MYSQL-FUNC */
%token  ASENSITIVE_SYM                /* FUTURE-USE */
%token  AT_SYM                        /* SQL-2003-R */
%token  AUTOEXTEND_SIZE_SYM
%token  AUTO_INC
%token  AVG_ROW_LENGTH
%token  AVG_SYM                       /* SQL-2003-N */
%token  BACKUP_SYM
%token  BEFORE_SYM                    /* SQL-2003-N */
%token  BEGIN_SYM                     /* SQL-2003-R */
%token  BETWEEN_SYM                   /* SQL-2003-R */
%token  BIGINT_SYM                    /* SQL-2003-R */
%token  BINARY_SYM                    /* SQL-2003-R */
%token  BINLOG_SYM
%token  BIN_NUM
%token  BIT_AND                       /* MYSQL-FUNC */
%token  BIT_OR                        /* MYSQL-FUNC */
%token  BIT_SYM                       /* MYSQL-FUNC */
%token  BIT_XOR                       /* MYSQL-FUNC */
%token  BLOB_SYM                      /* SQL-2003-R */
%token  BLOCK_SYM
%token  BOOLEAN_SYM                   /* SQL-2003-R */
%token  BOOL_SYM
%token  BOTH                          /* SQL-2003-R */
%token  BTREE_SYM
%token  BY                            /* SQL-2003-R */
%token  BYTE_SYM
%token  CACHE_SYM
%token  CALL_SYM                      /* SQL-2003-R */
%token  CASCADE                       /* SQL-2003-N */
%token  CASCADED                      /* SQL-2003-R */
%token  CASE_SYM                      /* SQL-2003-R */
%token  CAST_SYM                      /* SQL-2003-R */
%token  CATALOG_NAME_SYM              /* SQL-2003-N */
%token  CHAIN_SYM                     /* SQL-2003-N */
%token  CHANGE
%token  CHANGED
%token  CHANNEL_SYM
%token  CHARSET
%token  CHAR_SYM                      /* SQL-2003-R */
%token  CHECKSUM_SYM
%token  CHECK_SYM                     /* SQL-2003-R */
%token  CIPHER_SYM
%token  CLASS_ORIGIN_SYM              /* SQL-2003-N */
%token  CLIENT_SYM
%token  CLOSE_SYM                     /* SQL-2003-R */
%token  COALESCE                      /* SQL-2003-N */
%token  CODE_SYM
%token  COLLATE_SYM                   /* SQL-2003-R */
%token  COLLATION_SYM                 /* SQL-2003-N */
%token  COLUMNS
%token  COLUMN_SYM                    /* SQL-2003-R */
%token  COLUMN_FORMAT_SYM
%token  COLUMN_NAME_SYM               /* SQL-2003-N */
%token  COMMENT_SYM
%token  COMMITTED_SYM                 /* SQL-2003-N */
%token  COMMIT_SYM                    /* SQL-2003-R */
%token  COMPACT_SYM
%token  COMPLETION_SYM
%token  COMPRESSED_SYM
%token  COMPRESSION_SYM
%token  ENCRYPTION_SYM
%token  CONCURRENT
%token  CONDITION_SYM                 /* SQL-2003-R, SQL-2008-R */
%token  CONNECTION_SYM
%token  CONSISTENT_SYM
%token  CONSTRAINT                    /* SQL-2003-R */
%token  CONSTRAINT_CATALOG_SYM        /* SQL-2003-N */
%token  CONSTRAINT_NAME_SYM           /* SQL-2003-N */
%token  CONSTRAINT_SCHEMA_SYM         /* SQL-2003-N */
%token  CONTAINS_SYM                  /* SQL-2003-N */
%token  CONTEXT_SYM
%token  CONTINUE_SYM                  /* SQL-2003-R */
%token  CONVERT_SYM                   /* SQL-2003-N */
%token  COUNT_SYM                     /* SQL-2003-N */
%token  CPU_SYM
%token  CREATE                        /* SQL-2003-R */
%token  CROSS                         /* SQL-2003-R */
%token  CUBE_SYM                      /* SQL-2003-R */
%token  CURDATE                       /* MYSQL-FUNC */
%token  CURRENT_SYM                   /* SQL-2003-R */
%token  CURRENT_USER                  /* SQL-2003-R */
%token  CURSOR_SYM                    /* SQL-2003-R */
%token  CURSOR_NAME_SYM               /* SQL-2003-N */
%token  CURTIME                       /* MYSQL-FUNC */
%token  DATABASE
%token  DATABASES
%token  DATAFILE_SYM
%token  DATA_SYM                      /* SQL-2003-N */
%token  DATETIME_SYM                  /* MYSQL */
%token  DATE_ADD_INTERVAL             /* MYSQL-FUNC */
%token  DATE_SUB_INTERVAL             /* MYSQL-FUNC */
%token  DATE_SYM                      /* SQL-2003-R */
%token  DAY_HOUR_SYM
%token  DAY_MICROSECOND_SYM
%token  DAY_MINUTE_SYM
%token  DAY_SECOND_SYM
%token  DAY_SYM                       /* SQL-2003-R */
%token  DEALLOCATE_SYM                /* SQL-2003-R */
%token  DECIMAL_NUM
%token  DECIMAL_SYM                   /* SQL-2003-R */
%token  DECLARE_SYM                   /* SQL-2003-R */
%token  DEFAULT_SYM                   /* SQL-2003-R */
%token  DEFAULT_AUTH_SYM              /* INTERNAL */
%token  DEFINER_SYM
%token  DELAYED_SYM
%token  DELAY_KEY_WRITE_SYM
%token  DELETE_SYM                    /* SQL-2003-R */
%token  DESC                          /* SQL-2003-N */
%token  DESCRIBE                      /* SQL-2003-R */
%token  DES_KEY_FILE
%token  DETERMINISTIC_SYM             /* SQL-2003-R */
%token  DIAGNOSTICS_SYM               /* SQL-2003-N */
%token  DIRECTORY_SYM
%token  DISABLE_SYM
%token  DISCARD
%token  DISK_SYM
%token  DISTINCT                      /* SQL-2003-R */
%token  DIV_SYM
%token  DOUBLE_SYM                    /* SQL-2003-R */
%token  DO_SYM
%token  DROP                          /* SQL-2003-R */
%token  DUAL_SYM
%token  DUMPFILE
%token  DUPLICATE_SYM
%token  DYNAMIC_SYM                   /* SQL-2003-R */
%token  EACH_SYM                      /* SQL-2003-R */
%token  ELSE                          /* SQL-2003-R */
%token  ELSEIF_SYM
%token  ENABLE_SYM
%token  ENCLOSED
%token  END                           /* SQL-2003-R */
%token  ENDS_SYM
%token  END_OF_INPUT                  /* INTERNAL */
%token  ENGINES_SYM
%token  ENGINE_SYM
%token  ENUM_SYM                      /* MYSQL */
%token  EQ                            /* OPERATOR */
%token  EQUAL_SYM                     /* OPERATOR */
%token  ERROR_SYM
%token  ERRORS
%token  ESCAPED
%token  ESCAPE_SYM                    /* SQL-2003-R */
%token  EVENTS_SYM
%token  EVENT_SYM
%token  EVERY_SYM                     /* SQL-2003-N */
%token  EXCHANGE_SYM
%token  EXECUTE_SYM                   /* SQL-2003-R */
%token  EXISTS                        /* SQL-2003-R */
%token  EXIT_SYM
%token  EXPANSION_SYM
%token  EXPIRE_SYM
%token  EXPORT_SYM
%token  EXTENDED_SYM
%token  EXTENT_SIZE_SYM
%token  EXTRACT_SYM                   /* SQL-2003-N */
%token  FALSE_SYM                     /* SQL-2003-R */
%token  FAST_SYM
%token  FAULTS_SYM
%token  FETCH_SYM                     /* SQL-2003-R */
%token  FILE_SYM
%token  FILE_BLOCK_SIZE_SYM
%token  FILTER_SYM
%token  FIRST_SYM                     /* SQL-2003-N */
%token  FIXED_SYM
%token  FLOAT_NUM
%token  FLOAT_SYM                     /* SQL-2003-R */
%token  FLUSH_SYM
%token  FOLLOWS_SYM                  /* MYSQL */
%token  FORCE_SYM
%token  FOREIGN                       /* SQL-2003-R */
%token  FOR_SYM                       /* SQL-2003-R */
%token  FORMAT_SYM
%token  FOUND_SYM                     /* SQL-2003-R */
%token  FROM
%token  FULL                          /* SQL-2003-R */
%token  FULLTEXT_SYM
%token  FUNCTION_SYM                  /* SQL-2003-R */
%token  GE
%token  GENERAL
%token  GENERATED
%token  GROUP_REPLICATION
%token  GEOMETRYCOLLECTION_SYM        /* MYSQL */
%token  GEOMETRY_SYM
%token  GET_FORMAT                    /* MYSQL-FUNC */
%token  GET_SYM                       /* SQL-2003-R */
%token  GLOBAL_SYM                    /* SQL-2003-R */
%token  GRANT                         /* SQL-2003-R */
%token  GRANTS
%token  GROUP_SYM                     /* SQL-2003-R */
%token  GROUP_CONCAT_SYM
%token  GT_SYM                        /* OPERATOR */
%token  HANDLER_SYM
%token  HASH_SYM
%token  HAVING                        /* SQL-2003-R */
%token  HELP_SYM
%token  HEX_NUM
%token  HIGH_PRIORITY
%token  HOST_SYM
%token  HOSTS_SYM
%token  HOUR_MICROSECOND_SYM
%token  HOUR_MINUTE_SYM
%token  HOUR_SECOND_SYM
%token  HOUR_SYM                      /* SQL-2003-R */
%token  IDENT
%token  IDENTIFIED_SYM
%token  IDENT_QUOTED
%token  IF
%token  IGNORE_SYM
%token  IGNORE_SERVER_IDS_SYM
%token  IMPORT
%token  INDEXES
%token  INDEX_SYM
%token  INFILE
%token  INITIAL_SIZE_SYM
%token  INNER_SYM                     /* SQL-2003-R */
%token  INOUT_SYM                     /* SQL-2003-R */
%token  INSENSITIVE_SYM               /* SQL-2003-R */
%token  INSERT_SYM                    /* SQL-2003-R */
%token  INSERT_METHOD
%token  INSTANCE_SYM
%token  INSTALL_SYM
%token  INTERVAL_SYM                  /* SQL-2003-R */
%token  INTO                          /* SQL-2003-R */
%token  INT_SYM                       /* SQL-2003-R */
%token  INVOKER_SYM
%token  IN_SYM                        /* SQL-2003-R */
%token  IO_AFTER_GTIDS                /* MYSQL, FUTURE-USE */
%token  IO_BEFORE_GTIDS               /* MYSQL, FUTURE-USE */
%token  IO_SYM
%token  IPC_SYM
%token  IS                            /* SQL-2003-R */
%token  ISOLATION                     /* SQL-2003-R */
%token  ISSUER_SYM
%token  ITERATE_SYM
%token  JOIN_SYM                      /* SQL-2003-R */
%token  JSON_SEPARATOR_SYM            /* MYSQL */
%token  JSON_SYM                      /* MYSQL */
%token  KEYS
%token  KEY_BLOCK_SIZE
%token  KEY_SYM                       /* SQL-2003-N */
%token  KILL_SYM
%token  LANGUAGE_SYM                  /* SQL-2003-R */
%token  LAST_SYM                      /* SQL-2003-N */
%token  LE                            /* OPERATOR */
%token  LEADING                       /* SQL-2003-R */
%token  LEAVES
%token  LEAVE_SYM
%token  LEFT                          /* SQL-2003-R */
%token  LESS_SYM
%token  LEVEL_SYM
%token  LEX_HOSTNAME
%token  LIKE                          /* SQL-2003-R */
%token  LIMIT
%token  LINEAR_SYM
%token  LINES
%token  LINESTRING_SYM                /* MYSQL */
%token  LIST_SYM
%token  LOAD
%token  LOCAL_SYM                     /* SQL-2003-R */
%token  LOCATOR_SYM                   /* SQL-2003-N */
%token  LOCKS_SYM
%token  LOCK_SYM
%token  LOGFILE_SYM
%token  LOGS_SYM
%token  LONGBLOB_SYM                  /* MYSQL */
%token  LONGTEXT_SYM                  /* MYSQL */
%token  LONG_NUM
%token  LONG_SYM
%token  LOOP_SYM
%token  LOW_PRIORITY
%token  LT                            /* OPERATOR */
%token  MASTER_AUTO_POSITION_SYM
%token  MASTER_BIND_SYM
%token  MASTER_CONNECT_RETRY_SYM
%token  MASTER_DELAY_SYM
%token  MASTER_HOST_SYM
%token  MASTER_LOG_FILE_SYM
%token  MASTER_LOG_POS_SYM
%token  MASTER_PASSWORD_SYM
%token  MASTER_PORT_SYM
%token  MASTER_RETRY_COUNT_SYM
%token  MASTER_SERVER_ID_SYM
%token  MASTER_SSL_CAPATH_SYM
%token  MASTER_TLS_VERSION_SYM
%token  MASTER_SSL_CA_SYM
%token  MASTER_SSL_CERT_SYM
%token  MASTER_SSL_CIPHER_SYM
%token  MASTER_SSL_CRL_SYM
%token  MASTER_SSL_CRLPATH_SYM
%token  MASTER_SSL_KEY_SYM
%token  MASTER_SSL_SYM
%token  MASTER_SSL_VERIFY_SERVER_CERT_SYM
%token  MASTER_SYM
%token  MASTER_USER_SYM
%token  MASTER_HEARTBEAT_PERIOD_SYM
%token  MATCH                         /* SQL-2003-R */
%token  MAX_CONNECTIONS_PER_HOUR
%token  MAX_QUERIES_PER_HOUR
%token  MAX_ROWS
%token  MAX_SIZE_SYM
%token  MAX_SYM                       /* SQL-2003-N */
%token  MAX_UPDATES_PER_HOUR
%token  MAX_USER_CONNECTIONS_SYM
%token  MAX_VALUE_SYM                 /* SQL-2003-N */
%token  MEDIUMBLOB_SYM                /* MYSQL */
%token  MEDIUMINT_SYM                 /* MYSQL */
%token  MEDIUMTEXT_SYM                /* MYSQL */
%token  MEDIUM_SYM
%token  MEMORY_SYM
%token  MERGE_SYM                     /* SQL-2003-R */
%token  MESSAGE_TEXT_SYM              /* SQL-2003-N */
%token  MICROSECOND_SYM               /* MYSQL-FUNC */
%token  MIGRATE_SYM
%token  MINUTE_MICROSECOND_SYM
%token  MINUTE_SECOND_SYM
%token  MINUTE_SYM                    /* SQL-2003-R */
%token  MIN_ROWS
%token  MIN_SYM                       /* SQL-2003-N */
%token  MODE_SYM
%token  MODIFIES_SYM                  /* SQL-2003-R */
%token  MODIFY_SYM
%token  MOD_SYM                       /* SQL-2003-N */
%token  MONTH_SYM                     /* SQL-2003-R */
%token  MULTILINESTRING_SYM           /* MYSQL */
%token  MULTIPOINT_SYM                /* MYSQL */
%token  MULTIPOLYGON_SYM              /* MYSQL */
%token  MUTEX_SYM
%token  MYSQL_ERRNO_SYM
%token  NAMES_SYM                     /* SQL-2003-N */
%token  NAME_SYM                      /* SQL-2003-N */
%token  NATIONAL_SYM                  /* SQL-2003-R */
%token  NATURAL                       /* SQL-2003-R */
%token  NCHAR_STRING
%token  NCHAR_SYM                     /* SQL-2003-R */
%token  NDBCLUSTER_SYM
%token  NE                            /* OPERATOR */
%token  NEG
%token  NEVER_SYM
%token  NEW_SYM                       /* SQL-2003-R */
%token  NEXT_SYM                      /* SQL-2003-N */
%token  NODEGROUP_SYM
%token  NONE_SYM                      /* SQL-2003-R */
%token  NOT2_SYM
%token  NOT_SYM                       /* SQL-2003-R */
%token  NOW_SYM
%token  NO_SYM                        /* SQL-2003-R */
%token  NO_WAIT_SYM
%token  NO_WRITE_TO_BINLOG
%token  NULL_SYM                      /* SQL-2003-R */
%token  NUM
%token  NUMBER_SYM                    /* SQL-2003-N */
%token  NUMERIC_SYM                   /* SQL-2003-R */
%token  NVARCHAR_SYM
%token  OFFSET_SYM
%token  ON_SYM                        /* SQL-2003-R */
%token  ONE_SYM
%token  ONLY_SYM                      /* SQL-2003-R */
%token  OPEN_SYM                      /* SQL-2003-R */
%token  OPTIMIZE
%token  OPTIMIZER_COSTS_SYM
%token  OPTIONS_SYM
%token  OPTION                        /* SQL-2003-N */
%token  OPTIONALLY
%token  OR2_SYM
%token  ORDER_SYM                     /* SQL-2003-R */
%token  OR_OR_SYM                     /* OPERATOR */
%token  OR_SYM                        /* SQL-2003-R */
%token  OUTER
%token  OUTFILE
%token  OUT_SYM                       /* SQL-2003-R */
%token  OWNER_SYM
%token  PACK_KEYS_SYM
%token  PAGE_SYM
%token  PARAM_MARKER
%token  PARSER_SYM
%token  OBSOLETE_TOKEN_654            /* was: PARSE_GCOL_EXPR_SYM */
%token  PARTIAL                       /* SQL-2003-N */
%token  PARTITION_SYM                 /* SQL-2003-R */
%token  PARTITIONS_SYM
%token  PARTITIONING_SYM
%token  PASSWORD
%token  PHASE_SYM
%token  PLUGIN_DIR_SYM                /* INTERNAL */
%token  PLUGIN_SYM
%token  PLUGINS_SYM
%token  POINT_SYM
%token  POLYGON_SYM                   /* MYSQL */
%token  PORT_SYM
%token  POSITION_SYM                  /* SQL-2003-N */
%token  PRECEDES_SYM                  /* MYSQL */
%token  PRECISION                     /* SQL-2003-R */
%token  PREPARE_SYM                   /* SQL-2003-R */
%token  PRESERVE_SYM
%token  PREV_SYM
%token  PRIMARY_SYM                   /* SQL-2003-R */
%token  PRIVILEGES                    /* SQL-2003-N */
%token  PROCEDURE_SYM                 /* SQL-2003-R */
%token  PROCESS
%token  PROCESSLIST_SYM
%token  PROFILE_SYM
%token  PROFILES_SYM
%token  PROXY_SYM
%token  PURGE
%token  QUARTER_SYM
%token  QUERY_SYM
%token  QUICK
%token  RANGE_SYM                     /* SQL-2003-R */
%token  READS_SYM                     /* SQL-2003-R */
%token  READ_ONLY_SYM
%token  READ_SYM                      /* SQL-2003-N */
%token  READ_WRITE_SYM
%token  REAL_SYM                      /* SQL-2003-R */
%token  REBUILD_SYM
%token  RECOVER_SYM
%token  REDOFILE_SYM
%token  REDO_BUFFER_SIZE_SYM
%token  REDUNDANT_SYM
%token  REFERENCES                    /* SQL-2003-R */
%token  REGEXP
%token  RELAY
%token  RELAYLOG_SYM
%token  RELAY_LOG_FILE_SYM
%token  RELAY_LOG_POS_SYM
%token  RELAY_THREAD
%token  RELEASE_SYM                   /* SQL-2003-R */
%token  RELOAD
%token  REMOVE_SYM
%token  RENAME
%token  REORGANIZE_SYM
%token  REPAIR
%token  REPEATABLE_SYM                /* SQL-2003-N */
%token  REPEAT_SYM                    /* MYSQL-FUNC */
%token  REPLACE_SYM                   /* MYSQL-FUNC */
%token  REPLICATION
%token  REPLICATE_DO_DB
%token  REPLICATE_IGNORE_DB
%token  REPLICATE_DO_TABLE
%token  REPLICATE_IGNORE_TABLE
%token  REPLICATE_WILD_DO_TABLE
%token  REPLICATE_WILD_IGNORE_TABLE
%token  REPLICATE_REWRITE_DB
%token  REQUIRE_SYM
%token  RESET_SYM
%token  RESIGNAL_SYM                  /* SQL-2003-R */
%token  RESOURCES
%token  RESTORE_SYM
%token  RESTRICT
%token  RESUME_SYM
%token  RETURNED_SQLSTATE_SYM         /* SQL-2003-N */
%token  RETURNS_SYM                   /* SQL-2003-R */
%token  RETURN_SYM                    /* SQL-2003-R */
%token  REVERSE_SYM
%token  REVOKE                        /* SQL-2003-R */
%token  RIGHT                         /* SQL-2003-R */
%token  ROLLBACK_SYM                  /* SQL-2003-R */
%token  ROLLUP_SYM                    /* SQL-2003-R */
%token  ROTATE_SYM
%token  ROUTINE_SYM                   /* SQL-2003-N */
%token  ROWS_SYM                      /* SQL-2003-R */
%token  ROW_FORMAT_SYM
%token  ROW_SYM                       /* SQL-2003-R */
%token  ROW_COUNT_SYM                 /* SQL-2003-N */
%token  RTREE_SYM
%token  SAVEPOINT_SYM                 /* SQL-2003-R */
%token  SCHEDULE_SYM
%token  SCHEMA_NAME_SYM               /* SQL-2003-N */
%token  SECOND_MICROSECOND_SYM
%token  SECOND_SYM                    /* SQL-2003-R */
%token  SECURITY_SYM                  /* SQL-2003-N */
%token  SELECT_SYM                    /* SQL-2003-R */
%token  SENSITIVE_SYM                 /* FUTURE-USE */
%token  SEPARATOR_SYM
%token  SERIALIZABLE_SYM              /* SQL-2003-N */
%token  SERIAL_SYM
%token  SESSION_SYM                   /* SQL-2003-N */
%token  SERVER_SYM
%token  SERVER_OPTIONS
%token  SET_SYM                       /* SQL-2003-R */
%token  SET_VAR
%token  SHARE_SYM
%token  SHIFT_LEFT                    /* OPERATOR */
%token  SHIFT_RIGHT                   /* OPERATOR */
%token  SHOW
%token  SHUTDOWN
%token  SIGNAL_SYM                    /* SQL-2003-R */
%token  SIGNED_SYM
%token  SIMPLE_SYM                    /* SQL-2003-N */
%token  SLAVE
%token  SLOW
%token  SMALLINT_SYM                  /* SQL-2003-R */
%token  SNAPSHOT_SYM
%token  SOCKET_SYM
%token  SONAME_SYM
%token  SOUNDS_SYM
%token  SOURCE_SYM
%token  SPATIAL_SYM
%token  SPECIFIC_SYM                  /* SQL-2003-R */
%token  SQLEXCEPTION_SYM              /* SQL-2003-R */
%token  SQLSTATE_SYM                  /* SQL-2003-R */
%token  SQLWARNING_SYM                /* SQL-2003-R */
%token  SQL_AFTER_GTIDS               /* MYSQL */
%token  SQL_AFTER_MTS_GAPS            /* MYSQL */
%token  SQL_BEFORE_GTIDS              /* MYSQL */
%token  SQL_BIG_RESULT
%token  SQL_BUFFER_RESULT
%token  SQL_CACHE_SYM
%token  SQL_CALC_FOUND_ROWS
%token  SQL_NO_CACHE_SYM
%token  SQL_SMALL_RESULT
%token  SQL_SYM                       /* SQL-2003-R */
%token  SQL_THREAD
%token  SSL_SYM
%token  STACKED_SYM                   /* SQL-2003-N */
%token  STARTING
%token  STARTS_SYM
%token  START_SYM                     /* SQL-2003-R */
%token  STATS_AUTO_RECALC_SYM
%token  STATS_PERSISTENT_SYM
%token  STATS_SAMPLE_PAGES_SYM
%token  STATUS_SYM
%token  STDDEV_SAMP_SYM               /* SQL-2003-N */
%token  STD_SYM
%token  STOP_SYM
%token  STORAGE_SYM
%token  STORED_SYM
%token  STRAIGHT_JOIN
%token  STRING_SYM
%token  SUBCLASS_ORIGIN_SYM           /* SQL-2003-N */
%token  SUBDATE_SYM
%token  SUBJECT_SYM
%token  SUBPARTITIONS_SYM
%token  SUBPARTITION_SYM
%token  SUBSTRING                     /* SQL-2003-N */
%token  SUM_SYM                       /* SQL-2003-N */
%token  SUPER_SYM
%token  SUSPEND_SYM
%token  SWAPS_SYM
%token  SWITCHES_SYM
%token  SYSDATE
%token  TABLES
%token  TABLESPACE_SYM
%token  OBSOLETE_TOKEN_820            /* was: TABLE_REF_PRIORITY */
%token  TABLE_SYM                     /* SQL-2003-R */
%token  TABLE_CHECKSUM_SYM
%token  TABLE_NAME_SYM                /* SQL-2003-N */
%token  TEMPORARY                     /* SQL-2003-N */
%token  TEMPTABLE_SYM
%token  TERMINATED
%token  TEXT_STRING
%token  TEXT_SYM
%token  THAN_SYM
%token  THEN_SYM                      /* SQL-2003-R */
%token  TIMESTAMP_SYM                 /* SQL-2003-R */
%token  TIMESTAMP_ADD
%token  TIMESTAMP_DIFF
%token  TIME_SYM                      /* SQL-2003-R */
%token  TINYBLOB_SYM                  /* MYSQL */
%token  TINYINT_SYM                   /* MYSQL */
%token  TINYTEXT_SYN                  /* MYSQL */
%token  TO_SYM                        /* SQL-2003-R */
%token  TRAILING                      /* SQL-2003-R */
%token  TRANSACTION_SYM
%token  TRIGGERS_SYM
%token  TRIGGER_SYM                   /* SQL-2003-R */
%token  TRIM                          /* SQL-2003-N */
%token  TRUE_SYM                      /* SQL-2003-R */
%token  TRUNCATE_SYM
%token  TYPES_SYM
%token  TYPE_SYM                      /* SQL-2003-N */
%token  UDF_RETURNS_SYM
%token  ULONGLONG_NUM
%token  UNCOMMITTED_SYM               /* SQL-2003-N */
%token  UNDEFINED_SYM
%token  UNDERSCORE_CHARSET
%token  UNDOFILE_SYM
%token  UNDO_BUFFER_SIZE_SYM
%token  UNDO_SYM                      /* FUTURE-USE */
%token  UNICODE_SYM
%token  UNINSTALL_SYM
%token  UNION_SYM                     /* SQL-2003-R */
%token  UNIQUE_SYM
%token  UNKNOWN_SYM                   /* SQL-2003-R */
%token  UNLOCK_SYM
%token  UNSIGNED_SYM                  /* MYSQL */
%token  UNTIL_SYM
%token  UPDATE_SYM                    /* SQL-2003-R */
%token  UPGRADE_SYM
%token  USAGE                         /* SQL-2003-N */
%token  USER                          /* SQL-2003-R */
%token  USE_FRM
%token  USE_SYM
%token  USING                         /* SQL-2003-R */
%token  UTC_DATE_SYM
%token  UTC_TIMESTAMP_SYM
%token  UTC_TIME_SYM
%token  VALIDATION_SYM                /* MYSQL */
%token  VALUES                        /* SQL-2003-R */
%token  VALUE_SYM                     /* SQL-2003-R */
%token  VARBINARY_SYM                 /* SQL-2008-R */
%token  VARCHAR_SYM                   /* SQL-2003-R */
%token  VARIABLES
%token  VARIANCE_SYM
%token  VARYING                       /* SQL-2003-R */
%token  VAR_SAMP_SYM
%token  VIEW_SYM                      /* SQL-2003-N */
%token  VIRTUAL_SYM
%token  WAIT_SYM
%token  WARNINGS
%token  WEEK_SYM
%token  WEIGHT_STRING_SYM
%token  WHEN_SYM                      /* SQL-2003-R */
%token  WHERE                         /* SQL-2003-R */
%token  WHILE_SYM
%token  WITH                          /* SQL-2003-R */
%token  OBSOLETE_TOKEN_893            /* was: WITH_CUBE_SYM */
%token  WITH_ROLLUP_SYM               /* INTERNAL */
%token  WITHOUT_SYM                   /* SQL-2003-R */
%token  WORK_SYM                      /* SQL-2003-N */
%token  WRAPPER_SYM
%token  WRITE_SYM                     /* SQL-2003-N */
%token  X509_SYM
%token  XA_SYM
%token  XID_SYM                       /* MYSQL */
%token  XML_SYM
%token  XOR
%token  YEAR_MONTH_SYM
%token  YEAR_SYM                      /* SQL-2003-R */
%token  ZEROFILL_SYM                  /* MYSQL */

/*
   Tokens from MySQL 8.0
*/
%token  JSON_UNQUOTED_SEPARATOR_SYM   /* MYSQL */
%token  PERSIST_SYM
%token  ROLE_SYM                      /* SQL-1999-R */
%token  ADMIN_SYM                     /* SQL-1999-R */
%token  INVISIBLE_SYM
%token  VISIBLE_SYM
%token  EXCEPT_SYM                    /* SQL-1999-R */
%token  COMPONENT_SYM                 /* MYSQL */
%token  RECURSIVE_SYM                 /* SQL-1999-R */
%token  GRAMMAR_SELECTOR_EXPR         /* synthetic token: starts single expr. */
%token  GRAMMAR_SELECTOR_GCOL       /* synthetic token: starts generated col. */
%token  GRAMMAR_SELECTOR_PART      /* synthetic token: starts partition expr. */
%token  GRAMMAR_SELECTOR_CTE             /* synthetic token: starts CTE expr. */
%token  JSON_OBJECTAGG                /* SQL-2015-R */
%token  JSON_ARRAYAGG                 /* SQL-2015-R */
%token  OF_SYM                        /* SQL-1999-R */
%token  SKIP_SYM                      /* MYSQL */
%token  LOCKED_SYM                    /* MYSQL */
%token  NOWAIT_SYM                    /* MYSQL */
%token  GROUPING_SYM                  /* SQL-2011-R */
/*
  Resolve column attribute ambiguity -- force precedence of "UNIQUE KEY" against
  simple "UNIQUE" and "KEY" attributes:
*/
%right UNIQUE_SYM KEY_SYM

%left CONDITIONLESS_JOIN
%left   JOIN_SYM INNER_SYM CROSS STRAIGHT_JOIN NATURAL LEFT RIGHT ON_SYM USING
%left   SET_VAR
%left   OR_OR_SYM OR_SYM OR2_SYM
%left   XOR
%left   AND_SYM AND_AND_SYM
%left   BETWEEN_SYM CASE_SYM WHEN_SYM THEN_SYM ELSE
%left   EQ EQUAL_SYM GE GT_SYM LE LT NE IS LIKE REGEXP IN_SYM
%left   '|'
%left   '&'
%left   SHIFT_LEFT SHIFT_RIGHT
%left   '-' '+'
%left   '*' '/' '%' DIV_SYM MOD_SYM
%left   '^'
%left   NEG '~'
%right  NOT_SYM NOT2_SYM
%right  BINARY_SYM COLLATE_SYM
%left  INTERVAL_SYM
%left SUBQUERY_AS_EXPR
%left '(' ')'

%left EMPTY_FROM_CLAUSE
%right INTO

%type <lex_str>
        IDENT IDENT_QUOTED TEXT_STRING DECIMAL_NUM FLOAT_NUM NUM LONG_NUM HEX_NUM
        LEX_HOSTNAME ULONGLONG_NUM select_alias ident ident_or_text
        role_ident role_ident_or_text
        IDENT_sys TEXT_STRING_sys TEXT_STRING_literal
        NCHAR_STRING opt_component key_cache_name
        sp_opt_label BIN_NUM label_ident TEXT_STRING_filesystem ident_or_empty
        TEXT_STRING_sys_nonewline
        filter_wild_db_table_string

%type <lex_str_list> TEXT_STRING_sys_list

%type <lex_str_ptr>
        opt_table_alias

%type <table>
        table_ident table_ident_nodb

%type <simple_string>
        opt_db password

%type <string>
        text_string opt_gconcat_separator

%type <num>
        order_dir lock_option
        udf_type if_exists opt_local
        opt_no_write_to_binlog
        all_or_any opt_distinct
        opt_ignore_leaves fulltext_options union_option
        transaction_access_mode_types
        opt_natural_language_mode opt_query_expansion
        opt_ev_status opt_ev_on_completion ev_on_completion opt_ev_comment
        ev_alter_on_schedule_completion opt_ev_rename_to opt_ev_sql_stmt
        trg_action_time trg_event
        view_check_option

/*
  Bit field of MYSQL_START_TRANS_OPT_* flags.
*/
%type <num> opt_start_transaction_option_list
%type <num> start_transaction_option_list
%type <num> start_transaction_option

%type <m_yes_no_unk>
        opt_chain opt_release

%type <m_fk_option>
        delete_option

%type <ulong_num>
        ulong_num real_ulong_num merge_insert_types
        ws_num_codepoints func_datetime_precision
        now
        opt_checksum_type

%type <ulonglong_number>
        ulonglong_num real_ulonglong_num size_number

%type <lock_type>
        replace_lock_option opt_low_priority insert_lock_option load_data_lock

%type <locked_row_action> locked_row_action opt_locked_row_action

%type <item>
        literal insert_ident temporal_literal
        simple_ident expr opt_expr opt_else
        set_function_specification sum_expr
        in_sum_expr grouping_operation
        variable variable_aux bool_pri
        predicate bit_expr
        table_wild simple_expr udf_expr
        expr_or_default set_expr_or_default
        geometry_function
        signed_literal now_or_signed_literal opt_escape
        simple_ident_nospvar simple_ident_q
        field_or_var limit_option
        function_call_keyword
        function_call_nonkeyword
        function_call_generic
        function_call_conflict
        signal_allowed_expr
        simple_target_specification
        condition_number
        filter_db_ident
        filter_table_ident
        filter_string
        select_item
        opt_where_clause
        opt_where_clause_expr
        opt_having_clause
        opt_simple_limit

%type <item_num> NUM_literal

%type <item_list>
        when_list
        opt_filter_db_list filter_db_list
        opt_filter_table_list filter_table_list
        opt_filter_string_list filter_string_list
        opt_filter_db_pair_list filter_db_pair_list

%type <item_list2>
        expr_list udf_expr_list opt_udf_expr_list opt_expr_list select_item_list
        opt_paren_expr_list ident_list_arg ident_list values opt_values row_value fields

%type <var_type>
        option_type opt_var_type opt_var_ident_type opt_set_var_ident_type

%type <key_type>
        normal_key_type opt_unique constraint_key_type spatial

%type <key_alg>
        index_type

%type <string_list>
        string_list using_list opt_use_partition use_partition ident_string_list

%type <key_part>
        key_part

%type <date_time_type> date_time_type;
%type <interval> interval

%type <interval_time_st> interval_time_stamp

%type <row_type> row_types

%type <tx_isolation> isolation_types

%type <ha_rkey_mode> handler_rkey_mode

%type <ha_read_mode> handler_read_or_scan handler_scan_function
        handler_rkey_function

%type <cast_type> cast_type

%type <symbol> ident_keyword label_keyword role_keyword
        role_or_label_keyword role_or_ident_keyword

%type <lex_user> user grant_user user_func role

%type <charset>
        opt_collate
        charset_name
        charset_name_or_default
        old_or_new_charset_name
        old_or_new_charset_name_or_default
        collation_name
        collation_name_or_default
        opt_load_data_charset
        UNDERSCORE_CHARSET
        ascii unicode

%type <boolfunc2creator> comp_op

%type <NONE>
        change
        truncate rename
        show describe load alter optimize keycache preload flush
        reset purge commit rollback savepoint release
        slave master_def master_defs master_file_def slave_until_opts
        repair analyze check start checksum filter_def filter_defs
        kill
        keycache_list keycache_list_or_parts assign_to_keycache
        assign_to_keycache_parts
        preload_list preload_list_or_parts preload_keys preload_keys_parts
        handler
        opt_column
        lock unlock
        table_lock_list table_lock
        use
        varchar nchar nvarchar
        table_name
        grant_ident grant_list grant_option
        rename_list
        flush_options flush_option
        opt_flush_lock flush_options_list
        optional_braces
        opt_to
        table_to_table_list table_to_table
        opt_and charset
        help
        opt_extended_describe
        prepare prepare_src execute deallocate
        sp_suid
        sp_c_chistics sp_a_chistics sp_chistic sp_c_chistic xa
        opt_field_or_var_spec fields_or_vars opt_load_data_set_spec
        view_replace_or_algorithm view_replace
        view_algorithm view_or_trigger_or_sp_or_event
        definer_tail no_definer_tail
        view_suid view_tail view_select
        trigger_tail
        sp_tail sf_tail udf_tail event_tail
        install uninstall binlog_base64_event
        server_options_list server_option
        definer_opt no_definer definer get_diagnostics
        alter_user_command
        group_replication
END_OF_INPUT

%type <NONE> sp_proc_stmts sp_proc_stmts1 sp_proc_stmt
%type <NONE> sp_proc_stmt_statement sp_proc_stmt_return
%type <NONE> sp_proc_stmt_if
%type <NONE> sp_labeled_control sp_proc_stmt_unlabeled
%type <NONE> sp_labeled_block sp_unlabeled_block
%type <NONE> sp_proc_stmt_leave
%type <NONE> sp_proc_stmt_iterate
%type <NONE> sp_proc_stmt_open sp_proc_stmt_fetch sp_proc_stmt_close
%type <NONE> case_stmt_specification simple_case_stmt searched_case_stmt

%type <num>  sp_decl_idents sp_opt_inout sp_handler_type sp_hcond_list
%type <spcondvalue> sp_cond sp_hcond sqlstate signal_value opt_signal_value
%type <spblock> sp_decls sp_decl
%type <spname> sp_name
%type <index_hint> index_hint_type
%type <num> index_hint_clause
%type <filetype> data_or_xml

%type <NONE> signal_stmt resignal_stmt
%type <da_condition_item_name> signal_condition_information_item_name

%type <diag_area> which_area;
%type <diag_info> diagnostics_information;
%type <stmt_info_item> statement_information_item;
%type <stmt_info_item_name> statement_information_item_name;
%type <stmt_info_list> statement_information;
%type <cond_info_item> condition_information_item;
%type <cond_info_item_name> condition_information_item_name;
%type <cond_info_list> condition_information;
%type <signal_item_list> signal_information_item_list;
%type <signal_item_list> opt_set_signal_information;

%type <trg_characteristics> trigger_follows_precedes_clause;
%type <trigger_action_order_type> trigger_action_order;

%type <xid> xid;
%type <xa_option_type> opt_join_or_resume;
%type <xa_option_type> opt_suspend;
%type <xa_option_type> opt_one_phase;

%type <is_not_empty> opt_convert_xid opt_ignore opt_linear opt_bin_mod
        opt_if_not_exists opt_temporary
        opt_grant_option opt_with_admin_option
        opt_full

%type <NONE>
        '-' '+' '*' '/' '%' '(' ')'
        ',' '!' '{' '}' '&' '|' AND_SYM OR_SYM OR_OR_SYM BETWEEN_SYM CASE_SYM
        THEN_SYM WHEN_SYM DIV_SYM MOD_SYM OR2_SYM AND_AND_SYM

%type<NONE> SHOW DESC DESCRIBE describe_command

/*
  A bit field of SLAVE_IO, SLAVE_SQL flags.
*/
%type <num> opt_slave_thread_option_list
%type <num> slave_thread_option_list
%type <num> slave_thread_option

%type <key_usage_element> key_usage_element

%type <key_usage_list> key_usage_list opt_key_usage_list index_hint_definition
        index_hints_list opt_index_hints_list opt_key_definition
        cache_key_list_or_empty cache_keys_spec

%type <order_expr> order_expr

%type <order_list> order_list group_list gorder_list opt_gorder_clause

%type <c_str> field_length opt_field_length type_datetime_precision
        opt_place

%type <precision> precision opt_precision float_options

%type <charset_with_opt_binary> opt_charset_with_opt_binary

%type <limit_options> limit_options

%type <limit_clause> limit_clause opt_limit_clause

%type <ulonglong_number> query_spec_option

%type <select_options> select_option select_option_list select_options
        empty_select_options

%type <node>
          option_value

%type <join_table> joined_table joined_table_parens

%type <table_reference_list> opt_from_clause from_clause from_tables
        table_reference_list table_reference_list_parens

%type <olap_type> olap_opt

%type <group> opt_group_clause

%type <order> order_clause opt_order_clause

%type <locking_clause> locking_clause

%type <locking_clause_list> opt_locking_clause_list locking_clause_list

%type <lock_strength> lock_strength

%type <table_reference> table_reference esc_table_reference
        table_factor single_table single_table_parens

%type <query_expression_body> query_expression_body

%type <internal_variable_name> internal_variable_name

%type <option_value_following_option_type> option_value_following_option_type

%type <option_value_no_option_type> option_value_no_option_type

%type <option_value_list> option_value_list option_value_list_continued

%type <start_option_value_list> start_option_value_list

%type <transaction_access_mode> transaction_access_mode
        opt_transaction_access_mode

%type <isolation_level> isolation_level opt_isolation_level

%type <transaction_characteristics> transaction_characteristics

%type <start_option_value_list_following_option_type>
        start_option_value_list_following_option_type

%type <set> set

%type <line_separators> line_term line_term_list opt_line_term

%type <field_separators> field_term field_term_list opt_field_term

%type <into_destination> into_destination into_clause

%type <select_var_ident> select_var_ident

%type <select_var_list> select_var_list

%type <query_primary> query_primary  query_specification

%type <query_expression> query_expression query_expression_parens
        query_expression_or_parens as_create_query_expression

%type <subquery> subquery row_subquery table_subquery

%type <derived_table> derived_table

%type <select_stmt> select_stmt do_stmt select_stmt_with_into

%type <param_marker> param_marker

%type <text_literal> text_literal

%type <statement>
        delete_stmt
        update_stmt
        insert_stmt
        call_stmt
        replace_stmt
        shutdown_stmt
	alter_instance_stmt
        create_table_stmt
        set_role_stmt

%type <table_ident> table_ident_opt_wild

%type <table_ident_list> table_alias_ref_list table_locking_list

%type <simple_ident_list> simple_ident_list opt_derived_column_list

%type <num> opt_delete_options

%type <opt_delete_option> opt_delete_option

%type <column_value_pair>
        update_elem

%type <column_value_list_pair>
        update_list
        opt_insert_update_list

%type <values_list> values_list insert_values

%type <insert_query_expression> insert_query_expression

%type <column_row_value_list_pair> insert_from_constructor

%type <optimizer_hints> SELECT_SYM INSERT_SYM REPLACE_SYM UPDATE_SYM DELETE_SYM

%type <join_type> outer_join_type natural_join_type inner_join_type

%type <user_list> user_list role_list opt_except_role_list

%type <alter_instance_action> alter_instance_action

%type <index_definition_stmt> index_definition_stmt

%type <index_column_list> key_list

%type <index_options> opt_index_options index_options  opt_fulltext_index_options
          fulltext_index_options opt_spatial_index_options spatial_index_options
          opt_index_lock_and_algorithm

%type <index_option> index_option common_index_option fulltext_index_option
          spatial_index_option alter_algorithm_option alter_lock_option

%type <index_type> index_type_clause

%type <table_constraint_def> table_constraint_def

%type <index_name_and_type> opt_index_name_and_type

%type <visibility> visibility

%type <with_clause> with_clause opt_with_clause
%type <with_list> with_list
%type <common_table_expr> common_table_expr

%type <partition_option> part_option

%type <partition_option_list> opt_part_options part_option_list

%type <sub_part_definition> sub_part_definition

%type <sub_part_list> sub_part_list opt_sub_partition

%type <part_value_item> part_value_item

%type <part_value_item_list> part_value_item_list

%type <part_value_item_list_paren> part_value_item_list_paren part_func_max

%type <part_value_list> part_value_list

%type <part_values> part_values_in

%type <opt_part_values> opt_part_values

%type <part_definition> part_definition

%type <part_def_list> part_def_list opt_part_defs

%type <ulong_num> opt_num_subparts opt_num_parts

%type <name_list> name_list opt_name_list

%type <opt_key_algo> opt_key_algo

%type <opt_sub_part> opt_sub_part

%type <part_type_def> part_type_def

%type <partition_clause> partition_clause

%type <add_partition_rule> add_partition_rule

%type <mi_type> mi_repair_type mi_repair_types opt_mi_repair_types
        mi_check_type mi_check_types opt_mi_check_types

%type <opt_restrict> opt_restrict;

%type <table_list> table_list opt_table_list

%type <ternary_option> ternary_option;

%type <create_table_option> create_table_option
        default_charset default_collation

%type <create_table_options> create_table_options
        create_table_options_space_separated

%type <on_duplicate> duplicate opt_duplicate

%type <col_attr> column_attribute opt_collate_explicit

%type <column_format> column_format

%type <storage_media> storage_media

%type <col_attr_list> column_attribute_list opt_column_attribute_list

%type <virtual_or_stored> opt_stored_attribute

%type <field_option> field_option field_opt_list field_options

%type <int_type> int_type

%type <type> spatial_type type

%type <numeric_type> real_type numeric_type

%type <sp_default> sp_opt_default

%type <field_def> field_def

%type <check_constraint> check_constraint opt_check_or_references

%type <fk_options> opt_on_update_delete

%type <opt_match_clause> opt_match_clause

%type <reference_list> reference_list opt_ref_list

%type <fk_references> references

%type <field_ident> field_ident opt_ident constraint opt_constraint

%type <column_def> column_def

%type <table_element> table_element

%type <table_element_list> table_element_list

%type <create_table_tail> opt_create_table_options_etc
        opt_create_partitioning_etc opt_duplicate_as_qe

%type <wild_or_where> opt_wild_or_where_for_show
%type <acl_type> opt_acl_type

%type <lex_cstring_list> column_list opt_column_list

%type <role_or_privilege> role_or_privilege

%type <role_or_privilege_list> role_or_privilege_list

%%

/*
  Indentation of grammar rules:

rule: <-- starts at col 1
          rule1a rule1b rule1c <-- starts at col 11
          { <-- starts at col 11
            code <-- starts at col 13, indentation is 2 spaces
          }
        | rule2a rule2b
          {
            code
          }
        ; <-- on a line by itself, starts at col 9

  Also, please do not use any <TAB>, but spaces.
  Having a uniform indentation in this file helps
  code reviews, patches, merges, and make maintenance easier.
  Tip: grep [[:cntrl:]] sql_yacc.yy
  Thanks.
*/

start_entry:
          sql_statement
        | GRAMMAR_SELECTOR_EXPR bit_expr END_OF_INPUT
          {
            ITEMIZE($2, &$2);
            static_cast<Expression_parser_state *>(YYP)->result= $2;
          }
        | GRAMMAR_SELECTOR_PART partition_clause END_OF_INPUT
          {
            /*
              We enter here when translating partition info string into
              partition_info data structure.
            */
            CONTEXTUALIZE($2);
            static_cast<Partition_expr_parser_state *>(YYP)->result=
              &$2->part_info;
          }
        | GRAMMAR_SELECTOR_GCOL IDENT_sys '(' expr ')' END_OF_INPUT
          {
            /*
              We enter here when translating generated column info string into
              partition_info data structure.
            */

            // Check gcol expression for the "PARSE_GCOL_EXPR" prefix:
            if (my_strcasecmp(system_charset_info, $2.str, "PARSE_GCOL_EXPR"))
              MYSQL_YYABORT;

            auto gcol_info= NEW_PTN Generated_column;
            if (gcol_info == NULL)
              MYSQL_YYABORT; // OOM
            ITEMIZE($4, &$4);
            gcol_info->expr_item= $4;
            static_cast<Gcol_expr_parser_state *>(YYP)->result= gcol_info;
          }
        | GRAMMAR_SELECTOR_CTE table_subquery END_OF_INPUT
          {
            static_cast<Common_table_expr_parser_state *>(YYP)->result= $2;
          }
        ;

sql_statement:
          END_OF_INPUT
          {
            THD *thd= YYTHD;
            if (!thd->is_bootstrap_system_thread() &&
                !thd->m_parser_state->has_comment())
            {
              my_error(ER_EMPTY_QUERY, MYF(0));
              MYSQL_YYABORT;
            }
            thd->lex->sql_command= SQLCOM_EMPTY_QUERY;
            YYLIP->found_semicolon= NULL;
          }
        | simple_statement_or_begin
          {
            Lex_input_stream *lip = YYLIP;

            if (YYTHD->get_protocol()->has_client_capability(CLIENT_MULTI_QUERIES) &&
                lip->multi_statements &&
                ! lip->eof())
            {
              /*
                We found a well formed query, and multi queries are allowed:
                - force the parser to stop after the ';'
                - mark the start of the next query for the next invocation
                  of the parser.
              */
              lip->next_state= MY_LEX_END;
              lip->found_semicolon= lip->get_ptr();
            }
            else
            {
              /* Single query, terminated. */
              lip->found_semicolon= NULL;
            }
          }
          ';'
          opt_end_of_input
        | simple_statement_or_begin END_OF_INPUT
          {
            /* Single query, not terminated. */
            YYLIP->found_semicolon= NULL;
          }
        ;

opt_end_of_input:
          /* empty */
        | END_OF_INPUT
        ;

simple_statement_or_begin:
          simple_statement
        | begin_stmt
        ;

/* Verb clauses, except begin_stmt */
simple_statement:
          alter
        | analyze
        | binlog_base64_event
        | call_stmt             { MAKE_CMD($1); }
        | change
        | check
        | checksum
        | commit
        | create
        | deallocate
        | delete_stmt           { MAKE_CMD($1); }
        | describe
        | do_stmt               { MAKE_CMD($1); }
        | drop
        | execute
        | flush
        | get_diagnostics
        | group_replication
        | grant
        | handler
        | help
        | import_stmt
        | insert_stmt           { MAKE_CMD($1); }
        | install
        | kill
        | load
        | lock
        | optimize
        | keycache
        | preload
        | prepare
        | purge
        | release
        | rename
        | repair
        | replace_stmt          { MAKE_CMD($1); }
        | reset
        | resignal_stmt
        | revoke
        | rollback
        | savepoint
        | select_stmt           { MAKE_CMD($1); }
        | set                   { CONTEXTUALIZE($1); }
        | set_role_stmt         { MAKE_CMD($1); } // TODO: merge with "set"
        | signal_stmt
        | show
        | shutdown_stmt         { MAKE_CMD($1); }
        | slave
        | start
        | truncate
        | uninstall
        | unlock
        | update_stmt           { MAKE_CMD($1); }
        | use
        | xa
        ;

deallocate:
          deallocate_or_drop PREPARE_SYM ident
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->sql_command= SQLCOM_DEALLOCATE_PREPARE;
            lex->prepared_stmt_name= to_lex_cstring($3);
          }
        ;

deallocate_or_drop:
          DEALLOCATE_SYM
        | DROP
        ;

prepare:
          PREPARE_SYM ident FROM prepare_src
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->sql_command= SQLCOM_PREPARE;
            lex->prepared_stmt_name= to_lex_cstring($2);
            /*
              We don't know know at this time whether there's a password
              in prepare_src, so we err on the side of caution.  Setting
              the flag will force a rewrite which will obscure all of
              prepare_src in the "Query" log line.  We'll see the actual
              query (with just the passwords obscured, if any) immediately
              afterwards in the "Prepare" log lines anyway, and then again
              in the "Execute" log line if and when prepare_src is executed.
            */
            lex->contains_plaintext_password= true;
          }
        ;

prepare_src:
          TEXT_STRING_sys
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->prepared_stmt_code= $1;
            lex->prepared_stmt_code_is_varref= FALSE;
          }
        | '@' ident_or_text
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->prepared_stmt_code= $2;
            lex->prepared_stmt_code_is_varref= TRUE;
          }
        ;

execute:
          EXECUTE_SYM ident
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->sql_command= SQLCOM_EXECUTE;
            lex->prepared_stmt_name= to_lex_cstring($2);
          }
          execute_using
          {}
        ;

execute_using:
          /* nothing */
        | USING execute_var_list
        ;

execute_var_list:
          execute_var_list ',' execute_var_ident
        | execute_var_ident
        ;

execute_var_ident:
          '@' ident_or_text
          {
            LEX *lex=Lex;
            LEX_STRING *lexstr= (LEX_STRING*)sql_memdup(&$2, sizeof(LEX_STRING));
            if (!lexstr || lex->prepared_stmt_params.push_back(lexstr))
              MYSQL_YYABORT;
          }
        ;

/* help */

help:
          HELP_SYM
          {
            if (Lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "HELP");
              MYSQL_YYABORT;
            }
          }
          ident_or_text
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_HELP;
            lex->help_arg= $3.str;
          }
        ;

/* change master */

change:
          CHANGE MASTER_SYM TO_SYM
          {
            LEX *lex = Lex;
            lex->sql_command = SQLCOM_CHANGE_MASTER;
            /*
              Clear LEX_MASTER_INFO struct. repl_ignore_server_ids is cleared
              in THD::cleanup_after_query. So it is guaranteed to be empty here.
            */
            DBUG_ASSERT(Lex->mi.repl_ignore_server_ids.empty());
            lex->mi.set_unspecified();
          }
          master_defs opt_channel
          {}
        | CHANGE REPLICATION FILTER_SYM
          {
            THD *thd= YYTHD;
            LEX* lex= thd->lex;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->sql_command = SQLCOM_CHANGE_REPLICATION_FILTER;
            lex->m_sql_cmd= NEW_PTN Sql_cmd_change_repl_filter();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
          filter_defs opt_channel
          {}
        ;

filter_defs:
          filter_def
        | filter_defs ',' filter_def
        ;
filter_def:
          REPLICATE_DO_DB EQ opt_filter_db_list
          {
            Sql_cmd_change_repl_filter * filter_sql_cmd=
              (Sql_cmd_change_repl_filter*) Lex->m_sql_cmd;
            DBUG_ASSERT(filter_sql_cmd);
            filter_sql_cmd->set_filter_value($3, OPT_REPLICATE_DO_DB);
          }
        | REPLICATE_IGNORE_DB EQ opt_filter_db_list
          {
            Sql_cmd_change_repl_filter * filter_sql_cmd=
              (Sql_cmd_change_repl_filter*) Lex->m_sql_cmd;
            DBUG_ASSERT(filter_sql_cmd);
            filter_sql_cmd->set_filter_value($3, OPT_REPLICATE_IGNORE_DB);
          }
        | REPLICATE_DO_TABLE EQ opt_filter_table_list
          {
            Sql_cmd_change_repl_filter * filter_sql_cmd=
              (Sql_cmd_change_repl_filter*) Lex->m_sql_cmd;
            DBUG_ASSERT(filter_sql_cmd);
           filter_sql_cmd->set_filter_value($3, OPT_REPLICATE_DO_TABLE);
          }
        | REPLICATE_IGNORE_TABLE EQ opt_filter_table_list
          {
            Sql_cmd_change_repl_filter * filter_sql_cmd=
              (Sql_cmd_change_repl_filter*) Lex->m_sql_cmd;
            DBUG_ASSERT(filter_sql_cmd);
            filter_sql_cmd->set_filter_value($3, OPT_REPLICATE_IGNORE_TABLE);
          }
        | REPLICATE_WILD_DO_TABLE EQ opt_filter_string_list
          {
            Sql_cmd_change_repl_filter * filter_sql_cmd=
              (Sql_cmd_change_repl_filter*) Lex->m_sql_cmd;
            DBUG_ASSERT(filter_sql_cmd);
            filter_sql_cmd->set_filter_value($3, OPT_REPLICATE_WILD_DO_TABLE);
          }
        | REPLICATE_WILD_IGNORE_TABLE EQ opt_filter_string_list
          {
            Sql_cmd_change_repl_filter * filter_sql_cmd=
              (Sql_cmd_change_repl_filter*) Lex->m_sql_cmd;
            DBUG_ASSERT(filter_sql_cmd);
            filter_sql_cmd->set_filter_value($3,
                                             OPT_REPLICATE_WILD_IGNORE_TABLE);
          }
        | REPLICATE_REWRITE_DB EQ opt_filter_db_pair_list
          {
            Sql_cmd_change_repl_filter * filter_sql_cmd=
              (Sql_cmd_change_repl_filter*) Lex->m_sql_cmd;
            DBUG_ASSERT(filter_sql_cmd);
            filter_sql_cmd->set_filter_value($3, OPT_REPLICATE_REWRITE_DB);
          }
        ;
opt_filter_db_list:
          '(' ')'
          {
            $$= NEW_PTN List<Item>;
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | '(' filter_db_list ')'
          {
            $$= $2;
          }
        ;

filter_db_list:
          filter_db_ident
          {
            $$= NEW_PTN List<Item>;
            if ($$ == NULL)
              MYSQL_YYABORT;
            $$->push_back($1);
          }
        | filter_db_list ',' filter_db_ident
          {
            $1->push_back($3);
            $$= $1;
          }
        ;

filter_db_ident:
          ident /* DB name */
          {
            THD *thd= YYTHD;
            Item *db_item= NEW_PTN Item_string($1.str, $1.length,
                                               thd->charset());
            $$= db_item;
          }
        ;
opt_filter_db_pair_list:
          '(' ')'
          {
            $$= NEW_PTN List<Item>;
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        |'(' filter_db_pair_list ')'
          {
            $$= $2;
          }
        ;
filter_db_pair_list:
          '(' filter_db_ident ',' filter_db_ident ')'
          {
            $$= NEW_PTN List<Item>;
            if ($$ == NULL)
              MYSQL_YYABORT;
            $$->push_back($2);
            $$->push_back($4);
          }
        | filter_db_pair_list ',' '(' filter_db_ident ',' filter_db_ident ')'
          {
            $1->push_back($4);
            $1->push_back($6);
            $$= $1;
          }
        ;
opt_filter_table_list:
          '(' ')'
          {
            $$= NEW_PTN List<Item>;
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        |'(' filter_table_list ')'
          {
            $$= $2;
          }
        ;

filter_table_list:
          filter_table_ident
          {
            $$= NEW_PTN List<Item>;
            if ($$ == NULL)
              MYSQL_YYABORT;
            $$->push_back($1);
          }
        | filter_table_list ',' filter_table_ident
          {
            $1->push_back($3);
            $$= $1;
          }
        ;

filter_table_ident:
          ident '.' ident /* qualified table name */
          {
            THD *thd= YYTHD;
            Item_string *table_item= NEW_PTN Item_string($1.str, $1.length,
                                                         thd->charset());
            table_item->append(thd->strmake(".", 1), 1);
            table_item->append($3.str, $3.length);
            $$= table_item;
          }
        ;

opt_filter_string_list:
          '(' ')'
          {
            $$= NEW_PTN List<Item>;
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        |'(' filter_string_list ')'
          {
            $$= $2;
          }
        ;

filter_string_list:
          filter_string
          {
            $$= NEW_PTN List<Item>;
            if ($$ == NULL)
              MYSQL_YYABORT;
            $$->push_back($1);
          }
        | filter_string_list ',' filter_string
          {
            $1->push_back($3);
            $$= $1;
          }
        ;

filter_string:
          filter_wild_db_table_string
          {
            THD *thd= YYTHD;
            Item *string_item= NEW_PTN Item_string($1.str, $1.length,
                                                   thd->charset());
            $$= string_item;
          }
        ;

master_defs:
          master_def
        | master_defs ',' master_def
        ;

master_def:
          MASTER_HOST_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.host = $3.str;
          }
        | MASTER_BIND_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.bind_addr = $3.str;
          }
        | MASTER_USER_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.user = $3.str;
          }
        | MASTER_PASSWORD_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.password = $3.str;
            if (strlen($3.str) > 32)
            {
              my_error(ER_CHANGE_MASTER_PASSWORD_LENGTH, MYF(0));
              MYSQL_YYABORT;
            }
            Lex->contains_plaintext_password= true;
          }
        | MASTER_PORT_SYM EQ ulong_num
          {
            Lex->mi.port = $3;
          }
        | MASTER_CONNECT_RETRY_SYM EQ ulong_num
          {
            Lex->mi.connect_retry = $3;
          }
        | MASTER_RETRY_COUNT_SYM EQ ulong_num
          {
            Lex->mi.retry_count= $3;
            Lex->mi.retry_count_opt= LEX_MASTER_INFO::LEX_MI_ENABLE;
          }
        | MASTER_DELAY_SYM EQ ulong_num
          {
            if ($3 > MASTER_DELAY_MAX)
            {
              const char *msg= YYTHD->strmake(@3.cpp.start, @3.cpp.end - @3.cpp.start);
              my_error(ER_MASTER_DELAY_VALUE_OUT_OF_RANGE, MYF(0),
                       msg, MASTER_DELAY_MAX);
            }
            else
              Lex->mi.sql_delay = $3;
          }
        | MASTER_SSL_SYM EQ ulong_num
          {
            Lex->mi.ssl= $3 ?
              LEX_MASTER_INFO::LEX_MI_ENABLE : LEX_MASTER_INFO::LEX_MI_DISABLE;
          }
        | MASTER_SSL_CA_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.ssl_ca= $3.str;
          }
        | MASTER_SSL_CAPATH_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.ssl_capath= $3.str;
          }
        | MASTER_TLS_VERSION_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.tls_version= $3.str;
          }
        | MASTER_SSL_CERT_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.ssl_cert= $3.str;
          }
        | MASTER_SSL_CIPHER_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.ssl_cipher= $3.str;
          }
        | MASTER_SSL_KEY_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.ssl_key= $3.str;
          }
        | MASTER_SSL_VERIFY_SERVER_CERT_SYM EQ ulong_num
          {
            Lex->mi.ssl_verify_server_cert= $3 ?
              LEX_MASTER_INFO::LEX_MI_ENABLE : LEX_MASTER_INFO::LEX_MI_DISABLE;
          }
        | MASTER_SSL_CRL_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.ssl_crl= $3.str;
          }
        | MASTER_SSL_CRLPATH_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.ssl_crlpath= $3.str;
          }

        | MASTER_HEARTBEAT_PERIOD_SYM EQ NUM_literal
          {
            Item *num= $3;
            ITEMIZE(num, &num);

            Lex->mi.heartbeat_period= (float) num->val_real();
            if (Lex->mi.heartbeat_period > SLAVE_MAX_HEARTBEAT_PERIOD ||
                Lex->mi.heartbeat_period < 0.0)
            {
               const char format[]= "%d";
               char buf[4*sizeof(SLAVE_MAX_HEARTBEAT_PERIOD) + sizeof(format)];
               sprintf(buf, format, SLAVE_MAX_HEARTBEAT_PERIOD);
               my_error(ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE, MYF(0), buf);
               MYSQL_YYABORT;
            }
            if (Lex->mi.heartbeat_period > slave_net_timeout)
            {
              push_warning(YYTHD, Sql_condition::SL_WARNING,
                           ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MAX,
                           ER_THD(YYTHD, ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MAX));
            }
            if (Lex->mi.heartbeat_period < 0.001)
            {
              if (Lex->mi.heartbeat_period != 0.0)
              {
                push_warning(YYTHD, Sql_condition::SL_WARNING,
                             ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MIN,
                             ER_THD(YYTHD, ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MIN));
                Lex->mi.heartbeat_period= 0.0;
              }
              Lex->mi.heartbeat_opt=  LEX_MASTER_INFO::LEX_MI_DISABLE;
            }
            Lex->mi.heartbeat_opt=  LEX_MASTER_INFO::LEX_MI_ENABLE;
          }
        | IGNORE_SERVER_IDS_SYM EQ '(' ignore_server_id_list ')'
          {
            Lex->mi.repl_ignore_server_ids_opt= LEX_MASTER_INFO::LEX_MI_ENABLE;
           }
        |
        MASTER_AUTO_POSITION_SYM EQ ulong_num
          {
            Lex->mi.auto_position= $3 ?
              LEX_MASTER_INFO::LEX_MI_ENABLE :
              LEX_MASTER_INFO::LEX_MI_DISABLE;
          }
        |
        master_file_def
        ;

ignore_server_id_list:
          /* Empty */
          | ignore_server_id
          | ignore_server_id_list ',' ignore_server_id
        ;

ignore_server_id:
          ulong_num
          {
            Lex->mi.repl_ignore_server_ids.push_back($1);
          }

master_file_def:
          MASTER_LOG_FILE_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.log_file_name = $3.str;
          }
        | MASTER_LOG_POS_SYM EQ ulonglong_num
          {
            Lex->mi.pos = $3;
            /*
               If the user specified a value < BIN_LOG_HEADER_SIZE, adjust it
               instead of causing subsequent errors.
               We need to do it in this file, because only there we know that
               MASTER_LOG_POS has been explicitely specified. On the contrary
               in change_master() (sql_repl.cc) we cannot distinguish between 0
               (MASTER_LOG_POS explicitely specified as 0) and 0 (unspecified),
               whereas we want to distinguish (specified 0 means "read the binlog
               from 0" (4 in fact), unspecified means "don't change the position
               (keep the preceding value)").
            */
            Lex->mi.pos = max<ulonglong>(BIN_LOG_HEADER_SIZE, Lex->mi.pos);
          }
        | RELAY_LOG_FILE_SYM EQ TEXT_STRING_sys_nonewline
          {
            Lex->mi.relay_log_name = $3.str;
          }
        | RELAY_LOG_POS_SYM EQ ulong_num
          {
            Lex->mi.relay_log_pos = $3;
            /* Adjust if < BIN_LOG_HEADER_SIZE (same comment as Lex->mi.pos) */
            Lex->mi.relay_log_pos = max<ulong>(BIN_LOG_HEADER_SIZE,
                                               Lex->mi.relay_log_pos);
          }
        ;

opt_channel:
         /*empty */
       {
         Lex->mi.channel= "";
         Lex->mi.for_channel= false;
       }
     | FOR_SYM CHANNEL_SYM TEXT_STRING_sys_nonewline
       {
         /*
           channel names are case insensitive. This means, even the results
           displayed to the user are converted to lower cases.
           system_charset_info is utf8_general_ci as required by channel name
           restrictions
         */
         my_casedn_str(system_charset_info, $3.str);
         Lex->mi.channel= $3.str;
         Lex->mi.for_channel= true;
       }
    ;

create_table_stmt:
          CREATE opt_temporary TABLE_SYM opt_if_not_exists table_ident
          '(' table_element_list ')' opt_create_table_options_etc
          {
            $$= NEW_PTN PT_create_table_stmt($2, $4, $5,
                                             $7,
                                             $9.opt_create_table_options,
                                             $9.opt_partitioning,
                                             $9.on_duplicate,
                                             $9.opt_query_expression);
          }
        | CREATE opt_temporary TABLE_SYM opt_if_not_exists table_ident
          opt_create_table_options_etc
          {
            $$= NEW_PTN PT_create_table_stmt($2, $4, $5,
                                             NULL,
                                             $6.opt_create_table_options,
                                             $6.opt_partitioning,
                                             $6.on_duplicate,
                                             $6.opt_query_expression);
          }
        | CREATE opt_temporary TABLE_SYM opt_if_not_exists table_ident
          LIKE table_ident
          {
            $$= NEW_PTN PT_create_table_stmt($2, $4, $5, $7);
          }
        | CREATE opt_temporary TABLE_SYM opt_if_not_exists table_ident
          '(' LIKE table_ident ')'
          {
            $$= NEW_PTN PT_create_table_stmt($2, $4, $5, $8);
          }
        ;

create:
          create_table_stmt     { MAKE_CMD($1); }
        | index_definition_stmt { CONTEXTUALIZE($1); }
        | CREATE DATABASE opt_if_not_exists ident
          {
            Lex->create_info= YYTHD->alloc_typed<HA_CREATE_INFO>();
            if (Lex->create_info == NULL)
              MYSQL_YYABORT; // OOM
            Lex->create_info->default_table_charset= NULL;
            Lex->create_info->used_fields= 0;
          }
          opt_create_database_options
          {
            LEX *lex=Lex;
            lex->sql_command=SQLCOM_CREATE_DB;
            lex->name= $4;
            lex->create_info->options= $3 ? HA_LEX_CREATE_IF_NOT_EXISTS : 0;
          }
        | CREATE
          {
            Lex->create_view_mode= enum_view_create_mode::VIEW_CREATE_NEW;
            Lex->create_view_algorithm= VIEW_ALGORITHM_UNDEFINED;
            Lex->create_view_suid= TRUE;
            Lex->create_info= YYTHD->alloc_typed<HA_CREATE_INFO>();
            if (Lex->create_info == NULL)
              MYSQL_YYABORT; // OOM
          }
          view_or_trigger_or_sp_or_event
          {}
        | CREATE USER opt_if_not_exists grant_list require_clause
                      connect_options opt_account_lock_password_expire_options
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_CREATE_USER;
            Lex->create_info= YYTHD->alloc_typed<HA_CREATE_INFO>();
            if (Lex->create_info == NULL)
              MYSQL_YYABORT; // OOM
            lex->create_info->options= $3 ? HA_LEX_CREATE_IF_NOT_EXISTS : 0;
          }
        | CREATE ROLE_SYM opt_if_not_exists role_list
          {
            Lex->sql_command= SQLCOM_CREATE_ROLE;
            PT_statement *tmp= NEW_PTN PT_create_role(!!$3, $4);
            MAKE_CMD(tmp);
          }
        | CREATE LOGFILE_SYM GROUP_SYM logfile_group_info
          {
            Lex->alter_tablespace_info->ts_cmd_type= CREATE_LOGFILE_GROUP;
          }
        | CREATE TABLESPACE_SYM tablespace_info
          {
            Lex->alter_tablespace_info->ts_cmd_type= CREATE_TABLESPACE;
          }
        | CREATE SERVER_SYM ident_or_text FOREIGN DATA_SYM WRAPPER_SYM
          ident_or_text OPTIONS_SYM '(' server_options_list ')'
          {
            Lex->sql_command= SQLCOM_CREATE_SERVER;
            if ($3.length == 0)
            {
              my_error(ER_WRONG_VALUE, MYF(0), "server name", "");
              MYSQL_YYABORT;
            }
            Lex->server_options.m_server_name= $3;
            Lex->server_options.set_scheme($7);
            Lex->m_sql_cmd=
              NEW_PTN Sql_cmd_create_server(&Lex->server_options);
          }
        ;

index_definition_stmt:
          CREATE opt_unique INDEX_SYM opt_index_name_and_type
          ON_SYM table_ident '(' key_list ')' opt_index_options
          opt_index_lock_and_algorithm
          {
            $$= NEW_PTN PT_index_definition_stmt($2, $4.name, $4.type, $6, $8,
                                                 $10, $11);
          }
        | CREATE FULLTEXT_SYM INDEX_SYM ident ON_SYM table_ident
          '(' key_list ')' opt_fulltext_index_options opt_index_lock_and_algorithm
          {
            $$= NEW_PTN PT_index_definition_stmt(KEYTYPE_FULLTEXT, $4, NULL,
                                                 $6, $8, $10, $11);
          }
        | CREATE SPATIAL_SYM INDEX_SYM ident ON_SYM table_ident
          '(' key_list ')' opt_spatial_index_options opt_index_lock_and_algorithm
          {
            $$= NEW_PTN PT_index_definition_stmt(KEYTYPE_SPATIAL, $4, NULL, $6,
                                                 $8, $10, $11);
          }
        ;

server_options_list:
          server_option
        | server_options_list ',' server_option
        ;

server_option:
          USER TEXT_STRING_sys
          {
            Lex->server_options.set_username($2);
          }
        | HOST_SYM TEXT_STRING_sys
          {
            Lex->server_options.set_host($2);
          }
        | DATABASE TEXT_STRING_sys
          {
            Lex->server_options.set_db($2);
          }
        | OWNER_SYM TEXT_STRING_sys
          {
            Lex->server_options.set_owner($2);
          }
        | PASSWORD TEXT_STRING_sys
          {
            Lex->server_options.set_password($2);
            Lex->contains_plaintext_password= true;
          }
        | SOCKET_SYM TEXT_STRING_sys
          {
            Lex->server_options.set_socket($2);
          }
        | PORT_SYM ulong_num
          {
            Lex->server_options.set_port($2);
          }
        ;

event_tail:
          EVENT_SYM opt_if_not_exists sp_name
          {
            THD *thd= YYTHD;
            LEX *lex=Lex;

            lex->stmt_definition_begin= @1.cpp.start;
            lex->create_info->options= $2 ? HA_LEX_CREATE_IF_NOT_EXISTS : 0;
            if (!(lex->event_parse_data= new (thd->mem_root) Event_parse_data()))
              MYSQL_YYABORT;
            lex->event_parse_data->identifier= $3;
            lex->event_parse_data->on_completion=
                                  Event_parse_data::ON_COMPLETION_DROP;

            lex->sql_command= SQLCOM_CREATE_EVENT;
            /* We need that for disallowing subqueries */
          }
          ON_SYM SCHEDULE_SYM ev_schedule_time
          opt_ev_on_completion
          opt_ev_status
          opt_ev_comment
          DO_SYM ev_sql_stmt
          {
            /*
              sql_command is set here because some rules in ev_sql_stmt
              can overwrite it
            */
            Lex->sql_command= SQLCOM_CREATE_EVENT;
          }
        ;

ev_schedule_time:
          EVERY_SYM expr interval
          {
            ITEMIZE($2, &$2);

            Lex->event_parse_data->item_expression= $2;
            Lex->event_parse_data->interval= $3;
          }
          ev_starts
          ev_ends
        | AT_SYM expr
          {
            ITEMIZE($2, &$2);

            Lex->event_parse_data->item_execute_at= $2;
          }
        ;

opt_ev_status:
          /* empty */ { $$= 0; }
        | ENABLE_SYM
          {
            Lex->event_parse_data->status= Event_parse_data::ENABLED;
            Lex->event_parse_data->status_changed= true;
            $$= 1;
          }
        | DISABLE_SYM ON_SYM SLAVE
          {
            Lex->event_parse_data->status= Event_parse_data::SLAVESIDE_DISABLED;
            Lex->event_parse_data->status_changed= true;
            $$= 1;
          }
        | DISABLE_SYM
          {
            Lex->event_parse_data->status= Event_parse_data::DISABLED;
            Lex->event_parse_data->status_changed= true;
            $$= 1;
          }
        ;

ev_starts:
          /* empty */
          {
            Item *item= NEW_PTN Item_func_now_local(0);
            if (item == NULL)
              MYSQL_YYABORT;
            Lex->event_parse_data->item_starts= item;
          }
        | STARTS_SYM expr
          {
            ITEMIZE($2, &$2);

            Lex->event_parse_data->item_starts= $2;
          }
        ;

ev_ends:
          /* empty */
        | ENDS_SYM expr
          {
            ITEMIZE($2, &$2);

            Lex->event_parse_data->item_ends= $2;
          }
        ;

opt_ev_on_completion:
          /* empty */ { $$= 0; }
        | ev_on_completion
        ;

ev_on_completion:
          ON_SYM COMPLETION_SYM PRESERVE_SYM
          {
            Lex->event_parse_data->on_completion=
                                  Event_parse_data::ON_COMPLETION_PRESERVE;
            $$= 1;
          }
        | ON_SYM COMPLETION_SYM NOT_SYM PRESERVE_SYM
          {
            Lex->event_parse_data->on_completion=
                                  Event_parse_data::ON_COMPLETION_DROP;
            $$= 1;
          }
        ;

opt_ev_comment:
          /* empty */ { $$= 0; }
        | COMMENT_SYM TEXT_STRING_sys
          {
            Lex->event_parse_data->comment= $2;
            $$= 1;
          }
        ;

ev_sql_stmt:
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            /*
              This stops the following :
              - CREATE EVENT ... DO CREATE EVENT ...;
              - ALTER  EVENT ... DO CREATE EVENT ...;
              - CREATE EVENT ... DO ALTER EVENT DO ....;
              - CREATE PROCEDURE ... BEGIN CREATE EVENT ... END|
              This allows:
              - CREATE EVENT ... DO DROP EVENT yyy;
              - CREATE EVENT ... DO ALTER EVENT yyy;
                (the nested ALTER EVENT can have anything but DO clause)
              - ALTER  EVENT ... DO ALTER EVENT yyy;
                (the nested ALTER EVENT can have anything but DO clause)
              - ALTER  EVENT ... DO DROP EVENT yyy;
              - CREATE PROCEDURE ... BEGIN ALTER EVENT ... END|
                (the nested ALTER EVENT can have anything but DO clause)
              - CREATE PROCEDURE ... BEGIN DROP EVENT ... END|
            */
            if (lex->sphead)
            {
              my_error(ER_EVENT_RECURSION_FORBIDDEN, MYF(0));
              MYSQL_YYABORT;
            }

            sp_head *sp= sp_start_parsing(thd,
                                          enum_sp_type::EVENT,
                                          lex->event_parse_data->identifier);

            if (!sp)
              MYSQL_YYABORT;

            lex->sphead= sp;

            memset(&lex->sp_chistics, 0, sizeof(st_sp_chistics));
            sp->m_chistics= &lex->sp_chistics;

            /*
              Set a body start to the end of the last preprocessed token
              before ev_sql_stmt:
            */
            sp->set_body_start(thd, @0.cpp.end);
          }
          ev_sql_stmt_inner
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            sp_finish_parsing(thd);

            lex->sp_chistics.suid= SP_IS_SUID;  //always the definer!
            lex->event_parse_data->body_changed= TRUE;
          }
        ;

ev_sql_stmt_inner:
          sp_proc_stmt_statement
        | sp_proc_stmt_return
        | sp_proc_stmt_if
        | case_stmt_specification
        | sp_labeled_block
        | sp_unlabeled_block
        | sp_labeled_control
        | sp_proc_stmt_unlabeled
        | sp_proc_stmt_leave
        | sp_proc_stmt_iterate
        | sp_proc_stmt_open
        | sp_proc_stmt_fetch
        | sp_proc_stmt_close
        ;

sp_name:
          ident '.' ident
          {
            if (!$1.str ||
                (check_and_convert_db_name(&$1, false) != Ident_name_check::OK))
              MYSQL_YYABORT;
            if (sp_check_name(&$3))
            {
              MYSQL_YYABORT;
            }
            $$= new sp_name(to_lex_cstring($1), $3, true);
            if ($$ == NULL)
              MYSQL_YYABORT;
            $$->init_qname(YYTHD);
          }
        | ident
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            LEX_STRING db;
            if (sp_check_name(&$1))
            {
              MYSQL_YYABORT;
            }
            if (lex->copy_db_to(&db.str, &db.length))
              MYSQL_YYABORT;
            $$= new sp_name(to_lex_cstring(db), $1, false);
            if ($$ == NULL)
              MYSQL_YYABORT;
            $$->init_qname(thd);
          }
        ;

sp_a_chistics:
          /* Empty */ {}
        | sp_a_chistics sp_chistic {}
        ;

sp_c_chistics:
          /* Empty */ {}
        | sp_c_chistics sp_c_chistic {}
        ;

/* Characteristics for both create and alter */
sp_chistic:
          COMMENT_SYM TEXT_STRING_sys
          { Lex->sp_chistics.comment= to_lex_cstring($2); }
        | LANGUAGE_SYM SQL_SYM
          { /* Just parse it, we only have one language for now. */ }
        | NO_SYM SQL_SYM
          { Lex->sp_chistics.daccess= SP_NO_SQL; }
        | CONTAINS_SYM SQL_SYM
          { Lex->sp_chistics.daccess= SP_CONTAINS_SQL; }
        | READS_SYM SQL_SYM DATA_SYM
          { Lex->sp_chistics.daccess= SP_READS_SQL_DATA; }
        | MODIFIES_SYM SQL_SYM DATA_SYM
          { Lex->sp_chistics.daccess= SP_MODIFIES_SQL_DATA; }
        | sp_suid
          {}
        ;

/* Create characteristics */
sp_c_chistic:
          sp_chistic            { }
        | DETERMINISTIC_SYM     { Lex->sp_chistics.detistic= TRUE; }
        | not DETERMINISTIC_SYM { Lex->sp_chistics.detistic= FALSE; }
        ;

sp_suid:
          SQL_SYM SECURITY_SYM DEFINER_SYM
          {
            Lex->sp_chistics.suid= SP_IS_SUID;
          }
        | SQL_SYM SECURITY_SYM INVOKER_SYM
          {
            Lex->sp_chistics.suid= SP_IS_NOT_SUID;
          }
        ;

call_stmt:
          CALL_SYM sp_name opt_paren_expr_list
          {
            $$= NEW_PTN PT_call($2, $3);
          }
        ;

opt_paren_expr_list:
            /* Empty */ { $$= NULL; }
          | '(' opt_expr_list ')'
            {
              $$= $2;
            }
          ;

/* Stored FUNCTION parameter declaration list */
sp_fdparam_list:
          /* Empty */
        | sp_fdparams
        ;

sp_fdparams:
          sp_fdparams ',' sp_fdparam
        | sp_fdparam
        ;

sp_fdparam:
          ident type opt_collate
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            CONTEXTUALIZE($2);
            enum_field_types field_type= $2->type;
            const CHARSET_INFO *cs= $2->get_charset();
            if (merge_sp_var_charset_and_collation(&cs, cs, $3))
              MYSQL_YYABORT;

            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            if (sp_check_name(&$1))
              MYSQL_YYABORT;

            if (pctx->find_variable($1, TRUE))
            {
              my_error(ER_SP_DUP_PARAM, MYF(0), $1.str);
              MYSQL_YYABORT;
            }

            sp_variable *spvar= pctx->add_variable(thd,
                                                   $1,
                                                   field_type,
                                                   sp_variable::MODE_IN);

            if (spvar->field_def.init(thd, "", field_type,
                                      $2->get_length(), $2->get_dec(),
                                      $2->get_type_flags(),
                                      NULL, NULL, &NULL_STR, 0,
                                      $2->get_interval_list(),
                                      cs ? cs : thd->variables.collation_database,
                                      $2->get_uint_geom_type(), NULL))
            {
              MYSQL_YYABORT;
            }

            if (prepare_sp_create_field(thd,
                                        &spvar->field_def))
            {
              MYSQL_YYABORT;
            }
            spvar->field_def.field_name= spvar->name.str;
            spvar->field_def.maybe_null= true;
          }
        ;

/* Stored PROCEDURE parameter declaration list */
sp_pdparam_list:
          /* Empty */
        | sp_pdparams
        ;

sp_pdparams:
          sp_pdparams ',' sp_pdparam
        | sp_pdparam
        ;

sp_pdparam:
          sp_opt_inout ident type opt_collate
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            if (sp_check_name(&$2))
              MYSQL_YYABORT;

            if (pctx->find_variable($2, TRUE))
            {
              my_error(ER_SP_DUP_PARAM, MYF(0), $2.str);
              MYSQL_YYABORT;
            }

            CONTEXTUALIZE($3);
            enum_field_types field_type= $3->type;
            const CHARSET_INFO *cs= $3->get_charset();
            if (merge_sp_var_charset_and_collation(&cs, cs, $4))
              MYSQL_YYABORT;

            sp_variable *spvar= pctx->add_variable(thd,
                                                   $2,
                                                   field_type,
                                                   (sp_variable::enum_mode) $1);

            if (spvar->field_def.init(thd, "", field_type,
                                      $3->get_length(), $3->get_dec(),
                                      $3->get_type_flags(),
                                      NULL, NULL, &NULL_STR, 0,
                                      $3->get_interval_list(),
                                      cs ? cs : thd->variables.collation_database,
                                      $3->get_uint_geom_type(), NULL))
            {
              MYSQL_YYABORT;
            }

            if (prepare_sp_create_field(thd,
                                        &spvar->field_def))
            {
              MYSQL_YYABORT;
            }
            spvar->field_def.field_name= spvar->name.str;
            spvar->field_def.maybe_null= true;
          }
        ;

sp_opt_inout:
          /* Empty */ { $$= sp_variable::MODE_IN; }
        | IN_SYM      { $$= sp_variable::MODE_IN; }
        | OUT_SYM     { $$= sp_variable::MODE_OUT; }
        | INOUT_SYM   { $$= sp_variable::MODE_INOUT; }
        ;

sp_proc_stmts:
          /* Empty */ {}
        | sp_proc_stmts  sp_proc_stmt ';'
        ;

sp_proc_stmts1:
          sp_proc_stmt ';' {}
        | sp_proc_stmts1  sp_proc_stmt ';'
        ;

sp_decls:
          /* Empty */
          {
            $$.vars= $$.conds= $$.hndlrs= $$.curs= 0;
          }
        | sp_decls sp_decl ';'
          {
            /* We check for declarations out of (standard) order this way
              because letting the grammar rules reflect it caused tricky
               shift/reduce conflicts with the wrong result. (And we get
               better error handling this way.) */
            if (($2.vars || $2.conds) && ($1.curs || $1.hndlrs))
            { /* Variable or condition following cursor or handler */
              my_error(ER_SP_VARCOND_AFTER_CURSHNDLR, MYF(0));
              MYSQL_YYABORT;
            }
            if ($2.curs && $1.hndlrs)
            { /* Cursor following handler */
              my_error(ER_SP_CURSOR_AFTER_HANDLER, MYF(0));
              MYSQL_YYABORT;
            }
            $$.vars= $1.vars + $2.vars;
            $$.conds= $1.conds + $2.conds;
            $$.hndlrs= $1.hndlrs + $2.hndlrs;
            $$.curs= $1.curs + $2.curs;
          }
        ;

sp_decl:
          DECLARE_SYM           /*$1*/
          sp_decl_idents        /*$2*/
          type                  /*$3*/
          opt_collate           /*$4*/
          sp_opt_default        /*$5*/
          {                     /*$6*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            sp->reset_lex(thd);
            lex= thd->lex;

            pctx->declare_var_boundary($2);

            CONTEXTUALIZE($3);
            enum enum_field_types var_type= $3->type;
            const CHARSET_INFO *cs= $3->get_charset();
            if (merge_sp_var_charset_and_collation(&cs, cs, $4))
              MYSQL_YYABORT;

            uint num_vars= pctx->context_var_count();
            Item *dflt_value_item= $5.expr;

            LEX_STRING dflt_value_query= EMPTY_STR;

            if (dflt_value_item)
            {
              ITEMIZE(dflt_value_item, &dflt_value_item);
              const char *expr_start_ptr= $5.expr_start;
              if (lex->is_metadata_used())
              {
                dflt_value_query= make_string(thd, expr_start_ptr,
                                              @5.raw.end);
                if (!dflt_value_query.str)
                  MYSQL_YYABORT;
              }
            }
            else
            {
              dflt_value_item= NEW_PTN Item_null();

              if (dflt_value_item == NULL)
                MYSQL_YYABORT;
            }

            // We can have several variables in DECLARE statement.
            // We need to create an sp_instr_set instruction for each variable.

            for (uint i = num_vars-$2 ; i < num_vars ; i++)
            {
              uint var_idx= pctx->var_context2runtime(i);
              sp_variable *spvar= pctx->find_variable(var_idx);

              if (!spvar)
                MYSQL_YYABORT;

              spvar->type= var_type;
              spvar->default_value= dflt_value_item;

              if (spvar->field_def.init(thd, "", var_type,
                                        $3->get_length(), $3->get_dec(),
                                        $3->get_type_flags(),
                                        NULL, NULL, &NULL_STR, 0,
                                        $3->get_interval_list(),
                                        cs ? cs : thd->variables.collation_database,
                                        $3->get_uint_geom_type(), NULL))
              {
                MYSQL_YYABORT;
              }

              if (prepare_sp_create_field(thd, &spvar->field_def))
                MYSQL_YYABORT;

              spvar->field_def.field_name= spvar->name.str;
              spvar->field_def.maybe_null= true;

              /* The last instruction is responsible for freeing LEX. */

              sp_instr_set *is= NEW_PTN sp_instr_set(sp->instructions(),
                                                     lex,
                                                     var_idx,
                                                     dflt_value_item,
                                                     dflt_value_query,
                                                     (i == num_vars - 1));

              if (!is || sp->add_instr(thd, is))
                MYSQL_YYABORT;
            }

            pctx->declare_var_boundary(0);
            if (sp->restore_lex(thd))
              MYSQL_YYABORT;
            $$.vars= $2;
            $$.conds= $$.hndlrs= $$.curs= 0;
          }
        | DECLARE_SYM ident CONDITION_SYM FOR_SYM sp_cond
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            if (pctx->find_condition($2, TRUE))
            {
              my_error(ER_SP_DUP_COND, MYF(0), $2.str);
              MYSQL_YYABORT;
            }
            if(pctx->add_condition(thd, $2, $5))
              MYSQL_YYABORT;
            lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // DECLARE COND FOR
            $$.vars= $$.hndlrs= $$.curs= 0;
            $$.conds= 1;
          }
        | DECLARE_SYM sp_handler_type HANDLER_SYM FOR_SYM
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            sp_pcontext *parent_pctx= lex->get_sp_current_parsing_ctx();

            sp_pcontext *handler_pctx=
              parent_pctx->push_context(thd, sp_pcontext::HANDLER_SCOPE);

            sp_handler *h=
              parent_pctx->add_handler(thd, (sp_handler::enum_type) $2);

            lex->set_sp_current_parsing_ctx(handler_pctx);

            sp_instr_hpush_jump *i=
              NEW_PTN sp_instr_hpush_jump(sp->instructions(), handler_pctx, h);

            if (!i || sp->add_instr(thd, i))
              MYSQL_YYABORT;

            if ($2 == sp_handler::CONTINUE)
            {
              // Mark the end of CONTINUE handler scope.

              if (sp->m_parser_data.add_backpatch_entry(
                    i, handler_pctx->last_label()))
              {
                MYSQL_YYABORT;
              }
            }

            if (sp->m_parser_data.add_backpatch_entry(
                  i, handler_pctx->push_label(thd, EMPTY_STR, 0)))
            {
              MYSQL_YYABORT;
            }

            lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // DECL HANDLER FOR
          }
          sp_hcond_list sp_proc_stmt
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_label *hlab= pctx->pop_label(); /* After this hdlr */

            if ($2 == sp_handler::CONTINUE)
            {
              sp_instr_hreturn *i=
                NEW_PTN sp_instr_hreturn(sp->instructions(), pctx);

              if (!i || sp->add_instr(thd, i))
                MYSQL_YYABORT;
            }
            else
            {  /* EXIT or UNDO handler, just jump to the end of the block */
              sp_instr_hreturn *i=
                NEW_PTN sp_instr_hreturn(sp->instructions(), pctx);

              if (i == NULL ||
                  sp->add_instr(thd, i) ||
                  sp->m_parser_data.add_backpatch_entry(i, pctx->last_label()))
                MYSQL_YYABORT;
            }

            sp->m_parser_data.do_backpatch(hlab, sp->instructions());

            lex->set_sp_current_parsing_ctx(pctx->pop_context());

            $$.vars= $$.conds= $$.curs= 0;
            $$.hndlrs= 1;
          }
        | DECLARE_SYM   /*$1*/
          ident         /*$2*/
          CURSOR_SYM    /*$3*/
          FOR_SYM       /*$4*/
          {             /*$5*/
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;

            sp->reset_lex(thd);
            sp->m_parser_data.set_current_stmt_start_ptr(@4.raw.end);
          }
          select_stmt   /*$6*/
          {             /*$7*/
            MAKE_CMD($6);

            THD *thd= YYTHD;
            LEX *cursor_lex= Lex;
            sp_head *sp= cursor_lex->sphead;

            DBUG_ASSERT(cursor_lex->sql_command == SQLCOM_SELECT);

            if (cursor_lex->result)
            {
              my_error(ER_SP_BAD_CURSOR_SELECT, MYF(0));
              MYSQL_YYABORT;
            }

            cursor_lex->sp_lex_in_use= true;

            if (sp->restore_lex(thd))
              MYSQL_YYABORT;

            LEX *lex= Lex;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            uint offp;

            if (pctx->find_cursor($2, &offp, TRUE))
            {
              my_error(ER_SP_DUP_CURS, MYF(0), $2.str);
              delete cursor_lex;
              MYSQL_YYABORT;
            }

            LEX_STRING cursor_query= EMPTY_STR;

            if (cursor_lex->is_metadata_used())
            {
              cursor_query=
                make_string(thd,
                            sp->m_parser_data.get_current_stmt_start_ptr(),
                            @6.raw.end);

              if (!cursor_query.str)
                MYSQL_YYABORT;
            }

            sp_instr_cpush *i=
              NEW_PTN sp_instr_cpush(sp->instructions(), pctx,
                                     cursor_lex, cursor_query,
                                     pctx->current_cursor_count());

            if (i == NULL ||
                sp->add_instr(thd, i) ||
                pctx->add_cursor($2))
            {
              MYSQL_YYABORT;
            }

            $$.vars= $$.conds= $$.hndlrs= 0;
            $$.curs= 1;
          }
        ;

sp_handler_type:
          EXIT_SYM      { $$= sp_handler::EXIT; }
        | CONTINUE_SYM  { $$= sp_handler::CONTINUE; }
        /*| UNDO_SYM      { QQ No yet } */
        ;

sp_hcond_list:
          sp_hcond_element
          { $$= 1; }
        | sp_hcond_list ',' sp_hcond_element
          { $$+= 1; }
        ;

sp_hcond_element:
          sp_hcond
          {
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_pcontext *parent_pctx= pctx->parent_context();

            if (parent_pctx->check_duplicate_handler($1))
            {
              my_error(ER_SP_DUP_HANDLER, MYF(0));
              MYSQL_YYABORT;
            }
            else
            {
              sp_instr_hpush_jump *i=
                (sp_instr_hpush_jump *)sp->last_instruction();

              i->add_condition($1);
            }
          }
        ;

sp_cond:
          ulong_num
          { /* mysql errno */
            if ($1 == 0)
            {
              my_error(ER_WRONG_VALUE, MYF(0), "CONDITION", "0");
              MYSQL_YYABORT;
            }
            $$= NEW_PTN sp_condition_value($1);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | sqlstate
        ;

sqlstate:
          SQLSTATE_SYM opt_value TEXT_STRING_literal
          { /* SQLSTATE */

            /*
              An error is triggered:
                - if the specified string is not a valid SQLSTATE,
                - or if it represents the completion condition -- it is not
                  allowed to SIGNAL, or declare a handler for the completion
                  condition.
            */
            if (!is_sqlstate_valid(&$3) || is_sqlstate_completion($3.str))
            {
              my_error(ER_SP_BAD_SQLSTATE, MYF(0), $3.str);
              MYSQL_YYABORT;
            }
            $$= NEW_PTN sp_condition_value($3.str);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_value:
          /* Empty */  {}
        | VALUE_SYM    {}
        ;

sp_hcond:
          sp_cond
          {
            $$= $1;
          }
        | ident /* CONDITION name */
          {
            LEX *lex= Lex;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            $$= pctx->find_condition($1, false);

            if ($$ == NULL)
            {
              my_error(ER_SP_COND_MISMATCH, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
          }
        | SQLWARNING_SYM /* SQLSTATEs 01??? */
          {
            $$= NEW_PTN sp_condition_value(sp_condition_value::WARNING);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | not FOUND_SYM /* SQLSTATEs 02??? */
          {
            $$= NEW_PTN sp_condition_value(sp_condition_value::NOT_FOUND);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | SQLEXCEPTION_SYM /* All other SQLSTATEs */
          {
            $$= NEW_PTN sp_condition_value(sp_condition_value::EXCEPTION);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

signal_stmt:
          SIGNAL_SYM signal_value opt_set_signal_information
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            lex->sql_command= SQLCOM_SIGNAL;
            lex->m_sql_cmd= NEW_PTN Sql_cmd_signal($2, $3);
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

signal_value:
          ident
          {
            LEX *lex= Lex;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            if (!pctx)
            {
              /* SIGNAL foo cannot be used outside of stored programs */
              my_error(ER_SP_COND_MISMATCH, MYF(0), $1.str);
              MYSQL_YYABORT;
            }

            sp_condition_value *cond= pctx->find_condition($1, false);

            if (!cond)
            {
              my_error(ER_SP_COND_MISMATCH, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
            if (cond->type != sp_condition_value::SQLSTATE)
            {
              my_error(ER_SIGNAL_BAD_CONDITION_TYPE, MYF(0));
              MYSQL_YYABORT;
            }
            $$= cond;
          }
        | sqlstate
          { $$= $1; }
        ;

opt_signal_value:
          /* empty */
          { $$= NULL; }
        | signal_value
          { $$= $1; }
        ;

opt_set_signal_information:
          /* empty */
          { $$= NEW_PTN Set_signal_information(); }
        | SET_SYM signal_information_item_list
          { $$= $2; }
        ;

signal_information_item_list:
          signal_condition_information_item_name EQ signal_allowed_expr
          {
            $$= NEW_PTN Set_signal_information();
            if ($$->set_item($1, $3))
              MYSQL_YYABORT;
          }
        | signal_information_item_list ','
          signal_condition_information_item_name EQ signal_allowed_expr
          {
            $$= $1;
            if ($$->set_item($3, $5))
              MYSQL_YYABORT;
          }
        ;

/*
  Only a limited subset of <expr> are allowed in SIGNAL/RESIGNAL.
*/
signal_allowed_expr:
          literal
          { ITEMIZE($1, &$$); }
        | variable
          {
            ITEMIZE($1, &$1);

            if ($1->type() == Item::FUNC_ITEM)
            {
              Item_func *item= (Item_func*) $1;
              if (item->functype() == Item_func::SUSERVAR_FUNC)
              {
                /*
                  Don't allow the following syntax:
                    SIGNAL/RESIGNAL ...
                    SET <signal condition item name> = @foo := expr
                */
                my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
                MYSQL_YYABORT;
              }
            }
            $$= $1;
          }
        | simple_ident
          { ITEMIZE($1, &$$); }
        ;

/* conditions that can be set in signal / resignal */
signal_condition_information_item_name:
          CLASS_ORIGIN_SYM
          { $$= CIN_CLASS_ORIGIN; }
        | SUBCLASS_ORIGIN_SYM
          { $$= CIN_SUBCLASS_ORIGIN; }
        | CONSTRAINT_CATALOG_SYM
          { $$= CIN_CONSTRAINT_CATALOG; }
        | CONSTRAINT_SCHEMA_SYM
          { $$= CIN_CONSTRAINT_SCHEMA; }
        | CONSTRAINT_NAME_SYM
          { $$= CIN_CONSTRAINT_NAME; }
        | CATALOG_NAME_SYM
          { $$= CIN_CATALOG_NAME; }
        | SCHEMA_NAME_SYM
          { $$= CIN_SCHEMA_NAME; }
        | TABLE_NAME_SYM
          { $$= CIN_TABLE_NAME; }
        | COLUMN_NAME_SYM
          { $$= CIN_COLUMN_NAME; }
        | CURSOR_NAME_SYM
          { $$= CIN_CURSOR_NAME; }
        | MESSAGE_TEXT_SYM
          { $$= CIN_MESSAGE_TEXT; }
        | MYSQL_ERRNO_SYM
          { $$= CIN_MYSQL_ERRNO; }
        ;

resignal_stmt:
          RESIGNAL_SYM opt_signal_value opt_set_signal_information
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            lex->sql_command= SQLCOM_RESIGNAL;
            lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // RESIGNAL doesn't clear diagnostics
            lex->m_sql_cmd= NEW_PTN Sql_cmd_resignal($2, $3);
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

get_diagnostics:
          GET_SYM which_area DIAGNOSTICS_SYM diagnostics_information
          {
            Diagnostics_information *info= $4;

            info->set_which_da($2);

            Lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // GET DIAGS doesn't clear them.
            Lex->sql_command= SQLCOM_GET_DIAGNOSTICS;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_get_diagnostics(info);

            if (Lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

which_area:
        /* If <which area> is not specified, then CURRENT is implicit. */
          { $$= Diagnostics_information::CURRENT_AREA; }
        | CURRENT_SYM
          { $$= Diagnostics_information::CURRENT_AREA; }
        | STACKED_SYM
          { $$= Diagnostics_information::STACKED_AREA; }
        ;

diagnostics_information:
          statement_information
          {
            $$= NEW_PTN Statement_information($1);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | CONDITION_SYM condition_number condition_information
          {
            $$= NEW_PTN Condition_information($2, $3);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

statement_information:
          statement_information_item
          {
            $$= NEW_PTN List<Statement_information_item>;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        | statement_information ',' statement_information_item
          {
            if ($1->push_back($3))
              MYSQL_YYABORT;
            $$= $1;
          }
        ;

statement_information_item:
          simple_target_specification EQ statement_information_item_name
          {
            $$= NEW_PTN Statement_information_item($3, $1);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }

simple_target_specification:
          ident
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            /*
              NOTE: lex->sphead is NULL if we're parsing something like
              'GET DIAGNOSTICS v' outside a stored program. We should throw
              ER_SP_UNDECLARED_VAR in such cases.
            */

            if (!sp)
            {
              my_error(ER_SP_UNDECLARED_VAR, MYF(0), $1.str);
              MYSQL_YYABORT;
            }

            $$=
              create_item_for_sp_var(
                thd, $1, NULL,
                sp->m_parser_data.get_current_stmt_start_ptr(),
                @1.raw.start,
                @1.raw.end);

            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | '@' ident_or_text
          {
            $$= NEW_PTN Item_func_get_user_var(@$, $2);
            ITEMIZE($$, &$$);
          }
        ;

statement_information_item_name:
          NUMBER_SYM
          { $$= Statement_information_item::NUMBER; }
        | ROW_COUNT_SYM
          { $$= Statement_information_item::ROW_COUNT; }
        ;

/*
   Only a limited subset of <expr> are allowed in GET DIAGNOSTICS
   <condition number>, same subset as for SIGNAL/RESIGNAL.
*/
condition_number:
          signal_allowed_expr
          { $$= $1; }
        ;

condition_information:
          condition_information_item
          {
            $$= NEW_PTN List<Condition_information_item>;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        | condition_information ',' condition_information_item
          {
            if ($1->push_back($3))
              MYSQL_YYABORT;
            $$= $1;
          }
        ;

condition_information_item:
          simple_target_specification EQ condition_information_item_name
          {
            $$= NEW_PTN Condition_information_item($3, $1);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }

condition_information_item_name:
          CLASS_ORIGIN_SYM
          { $$= Condition_information_item::CLASS_ORIGIN; }
        | SUBCLASS_ORIGIN_SYM
          { $$= Condition_information_item::SUBCLASS_ORIGIN; }
        | CONSTRAINT_CATALOG_SYM
          { $$= Condition_information_item::CONSTRAINT_CATALOG; }
        | CONSTRAINT_SCHEMA_SYM
          { $$= Condition_information_item::CONSTRAINT_SCHEMA; }
        | CONSTRAINT_NAME_SYM
          { $$= Condition_information_item::CONSTRAINT_NAME; }
        | CATALOG_NAME_SYM
          { $$= Condition_information_item::CATALOG_NAME; }
        | SCHEMA_NAME_SYM
          { $$= Condition_information_item::SCHEMA_NAME; }
        | TABLE_NAME_SYM
          { $$= Condition_information_item::TABLE_NAME; }
        | COLUMN_NAME_SYM
          { $$= Condition_information_item::COLUMN_NAME; }
        | CURSOR_NAME_SYM
          { $$= Condition_information_item::CURSOR_NAME; }
        | MESSAGE_TEXT_SYM
          { $$= Condition_information_item::MESSAGE_TEXT; }
        | MYSQL_ERRNO_SYM
          { $$= Condition_information_item::MYSQL_ERRNO; }
        | RETURNED_SQLSTATE_SYM
          { $$= Condition_information_item::RETURNED_SQLSTATE; }
        ;

sp_decl_idents:
          ident
          {
            /* NOTE: field definition is filled in sp_decl section. */

            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            if (pctx->find_variable($1, TRUE))
            {
              my_error(ER_SP_DUP_VAR, MYF(0), $1.str);
              MYSQL_YYABORT;
            }

            pctx->add_variable(thd,
                               $1,
                               MYSQL_TYPE_DECIMAL,
                               sp_variable::MODE_IN);
            $$= 1;
          }
        | sp_decl_idents ',' ident
          {
            /* NOTE: field definition is filled in sp_decl section. */

            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            if (pctx->find_variable($3, TRUE))
            {
              my_error(ER_SP_DUP_VAR, MYF(0), $3.str);
              MYSQL_YYABORT;
            }

            pctx->add_variable(thd,
                               $3,
                               MYSQL_TYPE_DECIMAL,
                               sp_variable::MODE_IN);
            $$= $1 + 1;
          }
        ;

sp_opt_default:
        /* Empty */
          {
            $$.expr_start= NULL;
            $$.expr = NULL;
          }
        | DEFAULT_SYM expr
          {
            $$.expr_start= @1.raw.end;
            $$.expr= $2;
          }
        ;

sp_proc_stmt:
          sp_proc_stmt_statement
        | sp_proc_stmt_return
        | sp_proc_stmt_if
        | case_stmt_specification
        | sp_labeled_block
        | sp_unlabeled_block
        | sp_labeled_control
        | sp_proc_stmt_unlabeled
        | sp_proc_stmt_leave
        | sp_proc_stmt_iterate
        | sp_proc_stmt_open
        | sp_proc_stmt_fetch
        | sp_proc_stmt_close
        ;

sp_proc_stmt_if:
          IF
          { Lex->sphead->m_parser_data.new_cont_backpatch(); }
          sp_if END IF
          {
            sp_head *sp= Lex->sphead;

            sp->m_parser_data.do_cont_backpatch(sp->instructions());
          }
        ;

sp_proc_stmt_statement:
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            sp->reset_lex(thd);
            sp->m_parser_data.set_current_stmt_start_ptr(yylloc.raw.start);
          }
          simple_statement
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            sp->m_flags|= sp_get_flags_for_command(lex);
            if (lex->sql_command == SQLCOM_CHANGE_DB)
            { /* "USE db" doesn't work in a procedure */
              my_error(ER_SP_BADSTATEMENT, MYF(0), "USE");
              MYSQL_YYABORT;
            }
            /*
              Don't add an instruction for SET statements, since all
              instructions for them were already added during processing
              of "set" rule.
            */
            DBUG_ASSERT((lex->sql_command != SQLCOM_SET_OPTION &&
                         lex->sql_command != SQLCOM_SET_PASSWORD) ||
                        lex->var_list.is_empty());
            if (lex->sql_command != SQLCOM_SET_OPTION &&
                lex->sql_command != SQLCOM_SET_PASSWORD)
            {
              /* Extract the query statement from the tokenizer. */

              LEX_STRING query=
                make_string(thd,
                            sp->m_parser_data.get_current_stmt_start_ptr(),
                            @2.raw.end);

              if (!query.str)
                MYSQL_YYABORT;

              /* Add instruction. */

              sp_instr_stmt *i=
                NEW_PTN sp_instr_stmt(sp->instructions(), lex, query);

              if (!i || sp->add_instr(thd, i))
                MYSQL_YYABORT;
            }

            if (sp->restore_lex(thd))
              MYSQL_YYABORT;
          }
        ;

sp_proc_stmt_return:
          RETURN_SYM    /*$1*/
          {             /*$2*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            sp->reset_lex(thd);
          }
          expr          /*$3*/
          {             /*$4*/
            ITEMIZE($3, &$3);

            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            /* Extract expression string. */

            LEX_STRING expr_query= EMPTY_STR;

            const char *expr_start_ptr= @1.raw.end;

            if (lex->is_metadata_used())
            {
              expr_query= make_string(thd, expr_start_ptr, @3.raw.end);
              if (!expr_query.str)
                MYSQL_YYABORT;
            }

            /* Check that this is a stored function. */

            if (sp->m_type != enum_sp_type::FUNCTION)
            {
              my_error(ER_SP_BADRETURN, MYF(0));
              MYSQL_YYABORT;
            }

            /* Indicate that we've reached RETURN statement. */

            sp->m_flags|= sp_head::HAS_RETURN;

            /* Add instruction. */

            sp_instr_freturn *i=
              NEW_PTN sp_instr_freturn(sp->instructions(), lex, $3, expr_query,
                                       sp->m_return_field_def.sql_type);

            if (i == NULL ||
                sp->add_instr(thd, i) ||
                sp->restore_lex(thd))
            {
              MYSQL_YYABORT;
            }
          }
        ;

sp_proc_stmt_unlabeled:
          { /* Unlabeled controls get a secret label. */
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            pctx->push_label(thd,
                             EMPTY_STR,
                             sp->instructions());
          }
          sp_unlabeled_control
          {
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            sp->m_parser_data.do_backpatch(pctx->pop_label(),
                                           sp->instructions());
          }
        ;

sp_proc_stmt_leave:
          LEAVE_SYM label_ident
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp = lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_label *lab= pctx->find_label($2);

            if (! lab)
            {
              my_error(ER_SP_LILABEL_MISMATCH, MYF(0), "LEAVE", $2.str);
              MYSQL_YYABORT;
            }

            uint ip= sp->instructions();

            /*
              When jumping to a BEGIN-END block end, the target jump
              points to the block hpop/cpop cleanup instructions,
              so we should exclude the block context here.
              When jumping to something else (i.e., sp_label::ITERATION),
              there are no hpop/cpop at the jump destination,
              so we should include the block context here for cleanup.
            */
            bool exclusive= (lab->type == sp_label::BEGIN);

            size_t n= pctx->diff_handlers(lab->ctx, exclusive);

            if (n)
            {
              sp_instr_hpop *hpop= NEW_PTN sp_instr_hpop(ip++, pctx);

              if (!hpop || sp->add_instr(thd, hpop))
                MYSQL_YYABORT;
            }

            n= pctx->diff_cursors(lab->ctx, exclusive);

            if (n)
            {
              sp_instr_cpop *cpop= NEW_PTN sp_instr_cpop(ip++, pctx, n);

              if (!cpop || sp->add_instr(thd, cpop))
                MYSQL_YYABORT;
            }

            sp_instr_jump *i= NEW_PTN sp_instr_jump(ip, pctx);

            if (!i ||
                /* Jumping forward */
                sp->m_parser_data.add_backpatch_entry(i, lab) ||
                sp->add_instr(thd, i))
              MYSQL_YYABORT;
          }
        ;

sp_proc_stmt_iterate:
          ITERATE_SYM label_ident
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_label *lab= pctx->find_label($2);

            if (! lab || lab->type != sp_label::ITERATION)
            {
              my_error(ER_SP_LILABEL_MISMATCH, MYF(0), "ITERATE", $2.str);
              MYSQL_YYABORT;
            }

            uint ip= sp->instructions();

            /* Inclusive the dest. */
            size_t n= pctx->diff_handlers(lab->ctx, FALSE);

            if (n)
            {
              sp_instr_hpop *hpop= NEW_PTN sp_instr_hpop(ip++, pctx);

              if (!hpop || sp->add_instr(thd, hpop))
                MYSQL_YYABORT;
            }

            /* Inclusive the dest. */
            n= pctx->diff_cursors(lab->ctx, FALSE);

            if (n)
            {
              sp_instr_cpop *cpop= NEW_PTN sp_instr_cpop(ip++, pctx, n);

              if (!cpop || sp->add_instr(thd, cpop))
                MYSQL_YYABORT;
            }

            /* Jump back */
            sp_instr_jump *i= NEW_PTN sp_instr_jump(ip, pctx, lab->ip);

            if (!i || sp->add_instr(thd, i))
              MYSQL_YYABORT;
          }
        ;

sp_proc_stmt_open:
          OPEN_SYM ident
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            uint offset;

            if (! pctx->find_cursor($2, &offset, false))
            {
              my_error(ER_SP_CURSOR_MISMATCH, MYF(0), $2.str);
              MYSQL_YYABORT;
            }

            sp_instr_copen *i= NEW_PTN sp_instr_copen(sp->instructions(), pctx,
                                                      offset);

            if (!i || sp->add_instr(thd, i))
              MYSQL_YYABORT;
          }
        ;

sp_proc_stmt_fetch:
          FETCH_SYM sp_opt_fetch_noise ident INTO
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            uint offset;

            if (! pctx->find_cursor($3, &offset, false))
            {
              my_error(ER_SP_CURSOR_MISMATCH, MYF(0), $3.str);
              MYSQL_YYABORT;
            }

            sp_instr_cfetch *i= NEW_PTN sp_instr_cfetch(sp->instructions(),
                                                        pctx, offset);

            if (!i || sp->add_instr(thd, i))
              MYSQL_YYABORT;
          }
          sp_fetch_list
          {}
        ;

sp_proc_stmt_close:
          CLOSE_SYM ident
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            uint offset;

            if (! pctx->find_cursor($2, &offset, false))
            {
              my_error(ER_SP_CURSOR_MISMATCH, MYF(0), $2.str);
              MYSQL_YYABORT;
            }

            sp_instr_cclose *i=
              NEW_PTN sp_instr_cclose(sp->instructions(), pctx, offset);

            if (!i || sp->add_instr(thd, i))
              MYSQL_YYABORT;
          }
        ;

sp_opt_fetch_noise:
          /* Empty */
        | NEXT_SYM FROM
        | FROM
        ;

sp_fetch_list:
          ident
          {
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_variable *spv;

            if (!pctx || !(spv= pctx->find_variable($1, false)))
            {
              my_error(ER_SP_UNDECLARED_VAR, MYF(0), $1.str);
              MYSQL_YYABORT;
            }

            /* An SP local variable */
            sp_instr_cfetch *i= (sp_instr_cfetch *)sp->last_instruction();

            i->add_to_varlist(spv);
          }
        | sp_fetch_list ',' ident
          {
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_variable *spv;

            if (!pctx || !(spv= pctx->find_variable($3, false)))
            {
              my_error(ER_SP_UNDECLARED_VAR, MYF(0), $3.str);
              MYSQL_YYABORT;
            }

            /* An SP local variable */
            sp_instr_cfetch *i= (sp_instr_cfetch *)sp->last_instruction();

            i->add_to_varlist(spv);
          }
        ;

sp_if:
          {                     /*$1*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            sp->reset_lex(thd);
          }
          expr                  /*$2*/
          {                     /*$3*/
            ITEMIZE($2, &$2);

            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            /* Extract expression string. */

            LEX_STRING expr_query= EMPTY_STR;
            const char *expr_start_ptr= @0.raw.end;

            if (lex->is_metadata_used())
            {
              expr_query= make_string(thd, expr_start_ptr, @2.raw.end);
              if (!expr_query.str)
                MYSQL_YYABORT;
            }

            sp_instr_jump_if_not *i =
              NEW_PTN sp_instr_jump_if_not(sp->instructions(), lex,
                                           $2, expr_query);

            /* Add jump instruction. */

            if (i == NULL ||
                sp->m_parser_data.add_backpatch_entry(
                  i, pctx->push_label(thd, EMPTY_STR, 0)) ||
                sp->m_parser_data.add_cont_backpatch_entry(i) ||
                sp->add_instr(thd, i) ||
                sp->restore_lex(thd))
            {
              MYSQL_YYABORT;
            }
          }
          THEN_SYM              /*$4*/
          sp_proc_stmts1        /*$5*/
          {                     /*$6*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            sp_instr_jump *i = NEW_PTN sp_instr_jump(sp->instructions(), pctx);

            if (!i || sp->add_instr(thd, i))
              MYSQL_YYABORT;

            sp->m_parser_data.do_backpatch(pctx->pop_label(),
                                           sp->instructions());

            sp->m_parser_data.add_backpatch_entry(
              i, pctx->push_label(thd, EMPTY_STR, 0));
          }
          sp_elseifs            /*$7*/
          {                     /*$8*/
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            sp->m_parser_data.do_backpatch(pctx->pop_label(),
                                           sp->instructions());
          }
        ;

sp_elseifs:
          /* Empty */
        | ELSEIF_SYM sp_if
        | ELSE sp_proc_stmts1
        ;

case_stmt_specification:
          simple_case_stmt
        | searched_case_stmt
        ;

simple_case_stmt:
          CASE_SYM                      /*$1*/
          {                             /*$2*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            case_stmt_action_case(thd);

            sp->reset_lex(thd); /* For CASE-expr $3 */
          }
          expr                          /*$3*/
          {                             /*$4*/
            ITEMIZE($3, &$3);

            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;

            /* Extract CASE-expression string. */

            LEX_STRING case_expr_query= EMPTY_STR;
            const char *expr_start_ptr= @1.raw.end;

            if (lex->is_metadata_used())
            {
              case_expr_query= make_string(thd, expr_start_ptr, @3.raw.end);
              if (!case_expr_query.str)
                MYSQL_YYABORT;
            }

            /* Register new CASE-expression and get its id. */

            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            int case_expr_id= pctx->push_case_expr_id();

            if (case_expr_id < 0)
              MYSQL_YYABORT;

            /* Add CASE-set instruction. */

            sp_instr_set_case_expr *i=
              NEW_PTN sp_instr_set_case_expr(sp->instructions(), lex,
                                             case_expr_id, $3, case_expr_query);

            if (i == NULL ||
                sp->m_parser_data.add_cont_backpatch_entry(i) ||
                sp->add_instr(thd, i) ||
                sp->restore_lex(thd))
            {
              MYSQL_YYABORT;
            }
          }
          simple_when_clause_list       /*$5*/
          else_clause_opt               /*$6*/
          END                           /*$7*/
          CASE_SYM                      /*$8*/
          {                             /*$9*/
            case_stmt_action_end_case(Lex, true);
          }
        ;

searched_case_stmt:
          CASE_SYM
          {
            case_stmt_action_case(YYTHD);
          }
          searched_when_clause_list
          else_clause_opt
          END
          CASE_SYM
          {
            case_stmt_action_end_case(Lex, false);
          }
        ;

simple_when_clause_list:
          simple_when_clause
        | simple_when_clause_list simple_when_clause
        ;

searched_when_clause_list:
          searched_when_clause
        | searched_when_clause_list searched_when_clause
        ;

simple_when_clause:
          WHEN_SYM                      /*$1*/
          {                             /*$2*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            sp->reset_lex(thd);
          }
          expr                          /*$3*/
          {                             /*$4*/
            /* Simple case: <caseval> = <whenval> */

            ITEMIZE($3, &$3);

            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            /* Extract expression string. */

            LEX_STRING when_expr_query= EMPTY_STR;
            const char *expr_start_ptr= @1.raw.end;

            if (lex->is_metadata_used())
            {
              when_expr_query= make_string(thd, expr_start_ptr, @3.raw.end);
              if (!when_expr_query.str)
                MYSQL_YYABORT;
            }

            /* Add CASE-when-jump instruction. */

            sp_instr_jump_case_when *i =
              NEW_PTN sp_instr_jump_case_when(sp->instructions(), lex,
                                              pctx->get_current_case_expr_id(),
                                              $3, when_expr_query);

            if (i == NULL ||
                i->on_after_expr_parsing(thd) ||
                sp->m_parser_data.add_backpatch_entry(
                  i, pctx->push_label(thd, EMPTY_STR, 0)) ||
                sp->m_parser_data.add_cont_backpatch_entry(i) ||
                sp->add_instr(thd, i) ||
                sp->restore_lex(thd))
            {
              MYSQL_YYABORT;
            }
          }
          THEN_SYM                      /*$5*/
          sp_proc_stmts1                /*$6*/
          {                             /*$7*/
            if (case_stmt_action_then(YYTHD, Lex))
              MYSQL_YYABORT;
          }
        ;

searched_when_clause:
          WHEN_SYM                      /*$1*/
          {                             /*$2*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            sp->reset_lex(thd);
          }
          expr                          /*$3*/
          {                             /*$4*/
            ITEMIZE($3, &$3);

            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            /* Extract expression string. */

            LEX_STRING when_query= EMPTY_STR;
            const char *expr_start_ptr= @1.raw.end;

            if (lex->is_metadata_used())
            {
              when_query= make_string(thd, expr_start_ptr, @3.raw.end);
              if (!when_query.str)
                MYSQL_YYABORT;
            }

            /* Add jump instruction. */

            sp_instr_jump_if_not *i=
              NEW_PTN sp_instr_jump_if_not(sp->instructions(), lex, $3,
                                           when_query);

            if (i == NULL ||
                sp->m_parser_data.add_backpatch_entry(
                  i, pctx->push_label(thd, EMPTY_STR, 0)) ||
                sp->m_parser_data.add_cont_backpatch_entry(i) ||
                sp->add_instr(thd, i) ||
                sp->restore_lex(thd))
            {
              MYSQL_YYABORT;
            }
          }
          THEN_SYM                      /*$6*/
          sp_proc_stmts1                /*$7*/
          {                             /*$8*/
            if (case_stmt_action_then(YYTHD, Lex))
              MYSQL_YYABORT;
          }
        ;

else_clause_opt:
          /* empty */
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            sp_instr_error *i=
              NEW_PTN
                sp_instr_error(sp->instructions(), pctx, ER_SP_CASE_NOT_FOUND);

            if (!i || sp->add_instr(thd, i))
              MYSQL_YYABORT;
          }
        | ELSE sp_proc_stmts1
        ;

sp_labeled_control:
          label_ident ':'
          {
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_label *lab= pctx->find_label($1);

            if (lab)
            {
              my_error(ER_SP_LABEL_REDEFINE, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
            else
            {
              lab= pctx->push_label(YYTHD, $1, sp->instructions());
              lab->type= sp_label::ITERATION;
            }
          }
          sp_unlabeled_control sp_opt_label
          {
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_label *lab= pctx->pop_label();

            if ($5.str)
            {
              if (my_strcasecmp(system_charset_info, $5.str, lab->name.str) != 0)
              {
                my_error(ER_SP_LABEL_MISMATCH, MYF(0), $5.str);
                MYSQL_YYABORT;
              }
            }
            sp->m_parser_data.do_backpatch(lab, sp->instructions());
          }
        ;

sp_opt_label:
          /* Empty  */  { $$= null_lex_str; }
        | label_ident   { $$= $1; }
        ;

sp_labeled_block:
          label_ident ':'
          {
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_label *lab= pctx->find_label($1);

            if (lab)
            {
              my_error(ER_SP_LABEL_REDEFINE, MYF(0), $1.str);
              MYSQL_YYABORT;
            }

            lab= pctx->push_label(YYTHD, $1, sp->instructions());
            lab->type= sp_label::BEGIN;
          }
          sp_block_content sp_opt_label
          {
            LEX *lex= Lex;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            sp_label *lab= pctx->pop_label();

            if ($5.str)
            {
              if (my_strcasecmp(system_charset_info, $5.str, lab->name.str) != 0)
              {
                my_error(ER_SP_LABEL_MISMATCH, MYF(0), $5.str);
                MYSQL_YYABORT;
              }
            }
          }
        ;

sp_unlabeled_block:
          { /* Unlabeled blocks get a secret label. */
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            sp_label *lab=
              pctx->push_label(YYTHD, EMPTY_STR, sp->instructions());

            lab->type= sp_label::BEGIN;
          }
          sp_block_content
          {
            LEX *lex= Lex;
            lex->get_sp_current_parsing_ctx()->pop_label();
          }
        ;

sp_block_content:
          BEGIN_SYM
          { /* QQ This is just a dummy for grouping declarations and statements
              together. No [[NOT] ATOMIC] yet, and we need to figure out how
              make it coexist with the existing BEGIN COMMIT/ROLLBACK. */
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_pcontext *parent_pctx= lex->get_sp_current_parsing_ctx();

            sp_pcontext *child_pctx=
              parent_pctx->push_context(thd, sp_pcontext::REGULAR_SCOPE);

            lex->set_sp_current_parsing_ctx(child_pctx);
          }
          sp_decls
          sp_proc_stmts
          END
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            // We always have a label.
            sp->m_parser_data.do_backpatch(pctx->last_label(),
                                           sp->instructions());

            if ($3.hndlrs)
            {
              sp_instr *i= NEW_PTN sp_instr_hpop(sp->instructions(), pctx);

              if (!i || sp->add_instr(thd, i))
                MYSQL_YYABORT;
            }

            if ($3.curs)
            {
              sp_instr *i= NEW_PTN sp_instr_cpop(sp->instructions(), pctx,
                                                 $3.curs);

              if (!i || sp->add_instr(thd, i))
                MYSQL_YYABORT;
            }

            lex->set_sp_current_parsing_ctx(pctx->pop_context());
          }
        ;

sp_unlabeled_control:
          LOOP_SYM
          sp_proc_stmts1 END LOOP_SYM
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            sp_instr_jump *i= NEW_PTN sp_instr_jump(sp->instructions(), pctx,
                                                    pctx->last_label()->ip);

            if (!i || sp->add_instr(thd, i))
              MYSQL_YYABORT;
          }
        | WHILE_SYM                     /*$1*/
          {                             /*$2*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            sp->reset_lex(thd);
          }
          expr                          /*$3*/
          {                             /*$4*/
            ITEMIZE($3, &$3);

            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            /* Extract expression string. */

            LEX_STRING expr_query= EMPTY_STR;
            const char *expr_start_ptr= @1.raw.end;

            if (lex->is_metadata_used())
            {
              expr_query= make_string(thd, expr_start_ptr, @3.raw.end);
              if (!expr_query.str)
                MYSQL_YYABORT;
            }

            /* Add jump instruction. */

            sp_instr_jump_if_not *i=
              NEW_PTN
                sp_instr_jump_if_not(sp->instructions(), lex, $3, expr_query);

            if (i == NULL ||
                /* Jumping forward */
                sp->m_parser_data.add_backpatch_entry(i, pctx->last_label()) ||
                sp->m_parser_data.new_cont_backpatch() ||
                sp->m_parser_data.add_cont_backpatch_entry(i) ||
                sp->add_instr(thd, i) ||
                sp->restore_lex(thd))
            {
              MYSQL_YYABORT;
            }
          }
          DO_SYM                        /*$10*/
          sp_proc_stmts1                /*$11*/
          END                           /*$12*/
          WHILE_SYM                     /*$13*/
          {                             /*$14*/
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            sp_instr_jump *i= NEW_PTN sp_instr_jump(sp->instructions(), pctx,
                                                    pctx->last_label()->ip);

            if (!i || sp->add_instr(thd, i))
              MYSQL_YYABORT;

            sp->m_parser_data.do_cont_backpatch(sp->instructions());
          }
        | REPEAT_SYM                    /*$1*/
          sp_proc_stmts1                /*$2*/
          UNTIL_SYM                     /*$3*/
          {                             /*$4*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            sp->reset_lex(thd);
          }
          expr                          /*$5*/
          {                             /*$6*/
            ITEMIZE($5, &$5);

            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            uint ip= sp->instructions();

            /* Extract expression string. */

            LEX_STRING expr_query= EMPTY_STR;
            const char *expr_start_ptr= @3.raw.end;

            if (lex->is_metadata_used())
            {
              expr_query= make_string(thd, expr_start_ptr, @5.raw.end);
              if (!expr_query.str)
                MYSQL_YYABORT;
            }

            /* Add jump instruction. */

            sp_instr_jump_if_not *i=
              NEW_PTN sp_instr_jump_if_not(ip, lex, $5, expr_query,
                                           pctx->last_label()->ip);

            if (i == NULL ||
                sp->add_instr(thd, i) ||
                sp->restore_lex(thd))
            {
              MYSQL_YYABORT;
            }

            /* We can shortcut the cont_backpatch here */
            i->set_cont_dest(ip + 1);
          }
          END                           /*$7*/
          REPEAT_SYM                    /*$8*/
        ;

trg_action_time:
            BEFORE_SYM
            { $$= TRG_ACTION_BEFORE; }
          | AFTER_SYM
            { $$= TRG_ACTION_AFTER; }
          ;

trg_event:
            INSERT_SYM
            { $$= TRG_EVENT_INSERT; }
          | UPDATE_SYM
            { $$= TRG_EVENT_UPDATE; }
          | DELETE_SYM
            { $$= TRG_EVENT_DELETE; }
          ;
/*
  This part of the parser contains common code for all TABLESPACE
  commands.
  CREATE TABLESPACE_SYM name ...
  ALTER TABLESPACE_SYM name CHANGE DATAFILE ...
  ALTER TABLESPACE_SYM name ADD DATAFILE ...
  ALTER TABLESPACE_SYM name access_mode
  CREATE LOGFILE GROUP_SYM name ...
  ALTER LOGFILE GROUP_SYM name ADD UNDOFILE ..
  ALTER LOGFILE GROUP_SYM name ADD REDOFILE ..
  DROP TABLESPACE_SYM name
  DROP LOGFILE GROUP_SYM name
*/
change_tablespace_access:
          tablespace_name
          ts_access_mode
        ;

change_tablespace_info:
          tablespace_name
          CHANGE ts_datafile
          change_ts_option_list
        ;

tablespace_info:
          tablespace_name
          ADD ts_datafile
          opt_logfile_group_name
          tablespace_option_list
        ;

opt_logfile_group_name:
          /* empty */ {}
        | USE_SYM LOGFILE_SYM GROUP_SYM ident
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->logfile_group_name= $4.str;
          }
        ;

alter_tablespace_info:
          tablespace_name
          ADD ts_datafile
          alter_tablespace_option_list
          {
            Lex->alter_tablespace_info->ts_alter_tablespace_type= ALTER_TABLESPACE_ADD_FILE;
          }
        | tablespace_name
          DROP ts_datafile
          alter_tablespace_option_list
          {
            Lex->alter_tablespace_info->ts_alter_tablespace_type= ALTER_TABLESPACE_DROP_FILE;
          }
        ;

logfile_group_info:
          logfile_group_name
          add_log_file
          logfile_group_option_list
        ;

alter_logfile_group_info:
          logfile_group_name
          add_log_file
          alter_logfile_group_option_list
        ;

add_log_file:
          ADD lg_undofile
        | ADD lg_redofile
        ;

change_ts_option_list:
          /* empty */ {}
          change_ts_options
        ;

change_ts_options:
          change_ts_option
        | change_ts_options change_ts_option
        | change_ts_options ',' change_ts_option
        ;

change_ts_option:
          opt_ts_initial_size
        | opt_ts_autoextend_size
        | opt_ts_max_size
        ;

tablespace_option_list:
          /* empty */
        | tablespace_options
        ;

tablespace_options:
          tablespace_option
        | tablespace_options opt_comma tablespace_option
        ;

tablespace_option:
          opt_ts_initial_size
        | opt_ts_autoextend_size
        | opt_ts_max_size
        | opt_ts_extent_size
        | opt_ts_nodegroup
        | opt_ts_engine
        | ts_wait
        | opt_ts_comment
        | opt_ts_file_block_size
        ;

alter_tablespace_option_list:
          /* empty */
        | alter_tablespace_options
        ;

alter_tablespace_options:
          alter_tablespace_option
        | alter_tablespace_options opt_comma alter_tablespace_option
        ;

alter_tablespace_option:
          opt_ts_initial_size
        | opt_ts_autoextend_size
        | opt_ts_max_size
        | opt_ts_engine
        | ts_wait
        ;

logfile_group_option_list:
          /* empty */
        | logfile_group_options
        ;

logfile_group_options:
          logfile_group_option
        | logfile_group_options opt_comma logfile_group_option
        ;

logfile_group_option:
          opt_ts_initial_size
        | opt_ts_undo_buffer_size
        | opt_ts_redo_buffer_size
        | opt_ts_nodegroup
        | opt_ts_engine
        | ts_wait
        | opt_ts_comment
        ;

alter_logfile_group_option_list:
          /* empty */
        | alter_logfile_group_options
        ;

alter_logfile_group_options:
          alter_logfile_group_option
        | alter_logfile_group_options opt_comma alter_logfile_group_option
        ;

alter_logfile_group_option:
          opt_ts_initial_size
        | opt_ts_engine
        | ts_wait
        ;


ts_datafile:
          DATAFILE_SYM TEXT_STRING_sys
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->data_file_name= $2.str;
          }
        ;

lg_undofile:
          UNDOFILE_SYM TEXT_STRING_sys
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->undo_file_name= $2.str;
          }
        ;

lg_redofile:
          REDOFILE_SYM TEXT_STRING_sys
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->redo_file_name= $2.str;
          }
        ;

tablespace_name:
          ident
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info= new st_alter_tablespace();
            if (lex->alter_tablespace_info == NULL)
              MYSQL_YYABORT;
            lex->alter_tablespace_info->tablespace_name= $1.str;
            lex->sql_command= SQLCOM_ALTER_TABLESPACE;
          }
        ;

logfile_group_name:
          ident
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info= new st_alter_tablespace();
            if (lex->alter_tablespace_info == NULL)
              MYSQL_YYABORT;
            lex->alter_tablespace_info->logfile_group_name= $1.str;
            lex->sql_command= SQLCOM_ALTER_TABLESPACE;
          }
        ;

ts_access_mode:
          READ_ONLY_SYM
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->ts_access_mode= TS_READ_ONLY;
          }
        | READ_WRITE_SYM
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->ts_access_mode= TS_READ_WRITE;
          }
        | NOT_SYM ACCESSIBLE_SYM
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->ts_access_mode= TS_NOT_ACCESSIBLE;
          }
        ;

opt_ts_initial_size:
          INITIAL_SIZE_SYM opt_equal size_number
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->initial_size= $3;
          }
        ;

opt_ts_autoextend_size:
          AUTOEXTEND_SIZE_SYM opt_equal size_number
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->autoextend_size= $3;
          }
        ;

opt_ts_max_size:
          MAX_SIZE_SYM opt_equal size_number
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->max_size= $3;
          }
        ;

opt_ts_extent_size:
          EXTENT_SIZE_SYM opt_equal size_number
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->extent_size= $3;
          }
        ;

opt_ts_undo_buffer_size:
          UNDO_BUFFER_SIZE_SYM opt_equal size_number
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->undo_buffer_size= $3;
          }
        ;

opt_ts_redo_buffer_size:
          REDO_BUFFER_SIZE_SYM opt_equal size_number
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->redo_buffer_size= $3;
          }
        ;

opt_ts_nodegroup:
          NODEGROUP_SYM opt_equal real_ulong_num
          {
            LEX *lex= Lex;
            if (lex->alter_tablespace_info->nodegroup_id != UNDEF_NODEGROUP)
            {
              my_error(ER_FILEGROUP_OPTION_ONLY_ONCE,MYF(0),"NODEGROUP");
              MYSQL_YYABORT;
            }
            lex->alter_tablespace_info->nodegroup_id= $3;
          }
        ;

opt_ts_comment:
          COMMENT_SYM opt_equal TEXT_STRING_sys
          {
            LEX *lex= Lex;
            if (lex->alter_tablespace_info->ts_comment != NULL)
            {
              my_error(ER_FILEGROUP_OPTION_ONLY_ONCE,MYF(0),"COMMENT");
              MYSQL_YYABORT;
            }
            lex->alter_tablespace_info->ts_comment= $3.str;
          }
        ;

opt_ts_engine:
          opt_storage ENGINE_SYM opt_equal ident_or_text
          {
            LEX *lex= Lex;
            if (lex->alter_tablespace_info->storage_engine != NULL)
            {
              my_error(ER_FILEGROUP_OPTION_ONLY_ONCE,MYF(0),
                       "STORAGE ENGINE");
              MYSQL_YYABORT;
            }
            if (resolve_engine(YYTHD, $4, false, false,
                  &lex->alter_tablespace_info->storage_engine))
              MYSQL_YYABORT;
          }
        ;

opt_ts_file_block_size:
          FILE_BLOCK_SIZE_SYM opt_equal size_number
          {
            LEX *lex= Lex;
            if (lex->alter_tablespace_info->file_block_size != 0)
            {
              my_error(ER_FILEGROUP_OPTION_ONLY_ONCE,MYF(0),
                       "FILE_BLOCK_SIZE");
              MYSQL_YYABORT;
            }
            lex->alter_tablespace_info->file_block_size= $3;
          }
        ;

ts_wait:
          WAIT_SYM
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->wait_until_completed= TRUE;
          }
        | NO_WAIT_SYM
          {
            LEX *lex= Lex;
            if (!(lex->alter_tablespace_info->wait_until_completed))
            {
              my_error(ER_FILEGROUP_OPTION_ONLY_ONCE,MYF(0),"NO_WAIT");
              MYSQL_YYABORT;
            }
            lex->alter_tablespace_info->wait_until_completed= FALSE;
          }
        ;

size_number:
          real_ulonglong_num { $$= $1;}
        | IDENT_sys
          {
            ulonglong number;
            uint text_shift_number= 0;
            longlong prefix_number;
            char *start_ptr= $1.str;
            size_t str_len= $1.length;
            char *end_ptr= start_ptr + str_len;
            int error;
            prefix_number= my_strtoll10(start_ptr, &end_ptr, &error);
            if ((start_ptr + str_len - 1) == end_ptr)
            {
              switch (end_ptr[0])
              {
                case 'g':
                case 'G':
                  text_shift_number+=10;
                  // Fall through.
                case 'm':
                case 'M':
                  text_shift_number+=10;
                  // Fall through.
                case 'k':
                case 'K':
                  text_shift_number+=10;
                  break;
                default:
                {
                  my_error(ER_WRONG_SIZE_NUMBER, MYF(0));
                  MYSQL_YYABORT;
                }
              }
              if (prefix_number >> 31)
              {
                my_error(ER_SIZE_OVERFLOW_ERROR, MYF(0));
                MYSQL_YYABORT;
              }
              number= prefix_number << text_shift_number;
            }
            else
            {
              my_error(ER_WRONG_SIZE_NUMBER, MYF(0));
              MYSQL_YYABORT;
            }
            $$= number;
          }
        ;

/*
  End tablespace part
*/

/*
  To avoid grammar conflicts, we introduce the next few rules in very details:
  we workaround empty rules for optional AS and DUPLICATE clauses by expanding
  them in place of the caller rule:

  opt_create_table_options_etc ::=
    create_table_options opt_create_partitioning_etc
  | opt_create_partitioning_etc

  opt_create_partitioning_etc ::=
    partitioin [opt_duplicate_as_qe] | [opt_duplicate_as_qe]

  opt_duplicate_as_qe ::=
    duplicate as_create_query_expression
  | as_create_query_expression

  as_create_query_expression ::=
    AS query_expression_or_parens
  | query_expression_or_parens

*/

opt_create_table_options_etc:
          create_table_options
          opt_create_partitioning_etc
          {
            $$= $2;
            $$.opt_create_table_options= $1;
          }
        | opt_create_partitioning_etc
        ;

opt_create_partitioning_etc:
          partition_clause opt_duplicate_as_qe
          {
            $$= $2;
            $$.opt_partitioning= $1;
          }
        | opt_duplicate_as_qe
        ;

opt_duplicate_as_qe:
          /* empty */
          {
            $$.opt_create_table_options= NULL;
            $$.opt_partitioning= NULL;
            $$.on_duplicate= On_duplicate::ERROR;
            $$.opt_query_expression= NULL;
          }
        | duplicate
          as_create_query_expression
          {
            $$.opt_create_table_options= NULL;
            $$.opt_partitioning= NULL;
            $$.on_duplicate= $1;
            $$.opt_query_expression= $2;
          }
        | as_create_query_expression
          {
            $$.opt_create_table_options= NULL;
            $$.opt_partitioning= NULL;
            $$.on_duplicate= On_duplicate::ERROR;
            $$.opt_query_expression= $1;
          }
        ;

as_create_query_expression:
          AS query_expression_or_parens { $$= $2; }
        | query_expression_or_parens
        ;

/*
 This part of the parser is about handling of the partition information.

 It's first version was written by Mikael Ronström with lots of answers to
 questions provided by Antony Curtis.

 The partition grammar can be called from two places.
 1) CREATE TABLE ... PARTITION ..
 2) ALTER TABLE table_name PARTITION ...
*/
partition_clause:
          PARTITION_SYM BY part_type_def opt_num_parts opt_sub_part
          opt_part_defs
          {
            $$= NEW_PTN PT_partition($3, $4, $5, @6, $6);
          }
        ;

part_type_def:
          opt_linear KEY_SYM opt_key_algo '(' opt_name_list ')'
          {
            $$= NEW_PTN PT_part_type_def_key($1, $3, $5);
          }
        | opt_linear HASH_SYM '(' bit_expr ')'
          {
            $$= NEW_PTN PT_part_type_def_hash($1, @4, $4);
          }
        | RANGE_SYM '(' bit_expr ')'
          {
            $$= NEW_PTN PT_part_type_def_range_expr(@3, $3);
          }
        | RANGE_SYM COLUMNS '(' name_list ')'
          {
            $$= NEW_PTN PT_part_type_def_range_columns($4);
          }
        | LIST_SYM '(' bit_expr ')'
          {
            $$= NEW_PTN PT_part_type_def_list_expr(@3, $3);
          }
        | LIST_SYM COLUMNS '(' name_list ')'
          {
            $$= NEW_PTN PT_part_type_def_list_columns($4);
          }
        ;

opt_linear:
          /* empty */ { $$= false; }
        | LINEAR_SYM  { $$= true; }
        ;

opt_key_algo:
          /* empty */
          { $$= enum_key_algorithm::KEY_ALGORITHM_NONE; }
        | ALGORITHM_SYM EQ real_ulong_num
          {
            switch ($3) {
            case 1:
              $$= enum_key_algorithm::KEY_ALGORITHM_51;
              break;
            case 2:
              $$= enum_key_algorithm::KEY_ALGORITHM_55;
              break;
            default:
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
          }
        ;

opt_num_parts:
          /* empty */
          { $$= 0; }
        | PARTITIONS_SYM real_ulong_num
          {
            if ($2 == 0)
            {
              my_error(ER_NO_PARTS_ERROR, MYF(0), "partitions");
              MYSQL_YYABORT;
            }
            $$= $2;
          }
        ;

opt_sub_part:
          /* empty */ { $$= NULL; }
        | SUBPARTITION_SYM BY opt_linear HASH_SYM '(' bit_expr ')'
          opt_num_subparts
          {
            $$= NEW_PTN PT_sub_partition_by_hash($3, @6, $6, $8);
          }
        | SUBPARTITION_SYM BY opt_linear KEY_SYM opt_key_algo
          '(' name_list ')' opt_num_subparts
          {
            $$= NEW_PTN PT_sub_partition_by_key($3, $5, $7, $9);
          }
        ;


opt_name_list:
          /* empty */ { $$= NULL; }
        | name_list
        ;


name_list:
          ident
          {
            $$= NEW_PTN List<char>;
            if ($$ == NULL || $$->push_back($1.str))
              MYSQL_YYABORT;
          }
        | name_list ',' ident
          {
            $$= $1;
            if ($$->push_back($3.str))
              MYSQL_YYABORT;
          }
        ;

opt_num_subparts:
          /* empty */
          { $$= 0; }
        | SUBPARTITIONS_SYM real_ulong_num
          {
            if ($2 == 0)
            {
              my_error(ER_NO_PARTS_ERROR, MYF(0), "subpartitions");
              MYSQL_YYABORT;
            }
            $$= $2;
          }
        ;

opt_part_defs:
          /* empty */           { $$= NULL; }
        | '(' part_def_list ')' { $$= $2; }
        ;

part_def_list:
          part_definition
          {
            $$= NEW_PTN Trivial_array<PT_part_definition*>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | part_def_list ',' part_definition
          {
            $$= $1;
            if ($$->push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

part_definition:
          PARTITION_SYM ident opt_part_values opt_part_options opt_sub_partition
          {
            $$= NEW_PTN PT_part_definition(@0, $2, $3.type, $3.values, @3,
                                           $4, $5, @5);
          }
        ;

opt_part_values:
          /* empty */
          {
            $$.type= partition_type::HASH;
          }
        | VALUES LESS_SYM THAN_SYM part_func_max
          {
            $$.type= partition_type::RANGE;
            $$.values= $4;
          }
        | VALUES IN_SYM part_values_in
          {
            $$.type= partition_type::LIST;
            $$.values= $3;
          }
        ;

part_func_max:
          MAX_VALUE_SYM   { $$= NULL; }
        | part_value_item_list_paren
        ;

part_values_in:
          part_value_item_list_paren
          {
            $$= NEW_PTN PT_part_values_in_item(@1, $1);
          }
        | '(' part_value_list ')'
          {
            $$= NEW_PTN PT_part_values_in_list(@3, $2);
          }
        ;

part_value_list:
          part_value_item_list_paren
          {
            $$= NEW_PTN
              Trivial_array<PT_part_value_item_list_paren *>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | part_value_list ',' part_value_item_list_paren
          {
            $$= $1;
            if ($$->push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

part_value_item_list_paren:
          '('
          {
            /*
              This empty action is required because it resolves 2 reduce/reduce
              conflicts with an anonymous row expression:

              simple_expr:
                        ...
                      | '(' expr ',' expr_list ')'
            */
          }
          part_value_item_list ')'
          {
            $$= NEW_PTN PT_part_value_item_list_paren($3, @4);
          }
        ;

part_value_item_list:
          part_value_item
          {
            $$= NEW_PTN Trivial_array<PT_part_value_item *>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | part_value_item_list ',' part_value_item
          {
            $$= $1;
            if ($$->push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

part_value_item:
          MAX_VALUE_SYM { $$= NEW_PTN PT_part_value_item_max(@1); }
        | bit_expr      { $$= NEW_PTN PT_part_value_item_expr(@1, $1); }
        ;


opt_sub_partition:
          /* empty */           { $$= NULL; }
        | '(' sub_part_list ')' { $$= $2; }
        ;

sub_part_list:
          sub_part_definition
          {
            $$= NEW_PTN Trivial_array<PT_subpartition *>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | sub_part_list ',' sub_part_definition
          {
            $$= $1;
            if ($$->push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

sub_part_definition:
          SUBPARTITION_SYM ident_or_text opt_part_options
          {
            $$= NEW_PTN PT_subpartition(@1, $2.str, $3);
          }
        ;

opt_part_options:
         /* empty */ { $$= NULL; }
       | part_option_list
       ;

part_option_list:
          part_option_list part_option
          {
            $$= $1;
            if ($$->push_back($2))
              MYSQL_YYABORT; // OOM
          }
        | part_option
          {
            $$= NEW_PTN Trivial_array<PT_partition_option *>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        ;

part_option:
          TABLESPACE_SYM opt_equal ident
          { $$= NEW_PTN PT_partition_tablespace($3.str); }
        | opt_storage ENGINE_SYM opt_equal ident_or_text
          { $$= NEW_PTN PT_partition_engine($4); }
        | NODEGROUP_SYM opt_equal real_ulong_num
          { $$= NEW_PTN PT_partition_nodegroup($3); }
        | MAX_ROWS opt_equal real_ulonglong_num
          { $$= NEW_PTN PT_partition_max_rows($3); }
        | MIN_ROWS opt_equal real_ulonglong_num
          { $$= NEW_PTN PT_partition_min_rows($3); }
        | DATA_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys
          { $$= NEW_PTN PT_partition_data_directory($4.str); }
        | INDEX_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys
          { $$= NEW_PTN PT_partition_index_directory($4.str); }
        | COMMENT_SYM opt_equal TEXT_STRING_sys
          { $$= NEW_PTN PT_partition_comment($3.str); }
        ;

/*
 End of partition parser part
*/

opt_create_database_options:
          /* empty */ {}
        | create_database_options {}
        ;

create_database_options:
          create_database_option {}
        | create_database_options create_database_option {}
        ;

create_database_option:
          default_collation { CONTEXTUALIZE($1); }
        | default_charset   { CONTEXTUALIZE($1); }
        ;

opt_if_not_exists:
          /* empty */   { $$= false; }
        | IF not EXISTS { $$= true; }
        ;

create_table_options_space_separated:
          create_table_option
          {
            $$= NEW_PTN Trivial_array<PT_create_table_option *>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | create_table_options_space_separated create_table_option
          {
            $$= $1;
            if ($$->push_back($2))
              MYSQL_YYABORT; // OOM
          }
        ;

create_table_options:
          create_table_option
          {
            $$= NEW_PTN Trivial_array<PT_create_table_option *>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | create_table_options opt_comma create_table_option
          {
            $$= $1;
            if ($$->push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

opt_comma:
          /* empty */
        | ','
        ;

create_table_option:
          ENGINE_SYM opt_equal ident_or_text
          {
            $$= NEW_PTN PT_create_table_engine_option($3);
          }
        | MAX_ROWS opt_equal ulonglong_num
          {
            $$= NEW_PTN PT_create_max_rows_option($3);
          }
        | MIN_ROWS opt_equal ulonglong_num
          {
            $$= NEW_PTN PT_create_min_rows_option($3);
          }
        | AVG_ROW_LENGTH opt_equal ulong_num
          {
            $$= NEW_PTN PT_create_avg_row_length_option($3);
          }
        | PASSWORD opt_equal TEXT_STRING_sys
          {
            $$= NEW_PTN PT_create_password_option($3.str);
          }
        | COMMENT_SYM opt_equal TEXT_STRING_sys
          {
            $$= NEW_PTN PT_create_commen_option($3);
          }
        | COMPRESSION_SYM opt_equal TEXT_STRING_sys
	  {
            $$= NEW_PTN PT_create_compress_option($3);
	  }
        | ENCRYPTION_SYM opt_equal TEXT_STRING_sys
	  {
            $$= NEW_PTN PT_create_encryption_option($3);
	  }
        | AUTO_INC opt_equal ulonglong_num
          {
            $$= NEW_PTN PT_create_auto_increment_option($3);
          }
        | PACK_KEYS_SYM opt_equal ternary_option
          {
            $$= NEW_PTN PT_create_pack_keys_option($3);
          }
        | STATS_AUTO_RECALC_SYM opt_equal ternary_option
          {
            $$= NEW_PTN PT_create_stats_auto_recalc_option($3);
          }
        | STATS_PERSISTENT_SYM opt_equal ternary_option
          {
            $$= NEW_PTN PT_create_stats_persistent_option($3);
          }
        | STATS_SAMPLE_PAGES_SYM opt_equal ulong_num
          {
            /* From user point of view STATS_SAMPLE_PAGES can be specified as
            STATS_SAMPLE_PAGES=N (where 0<N<=65535, it does not make sense to
            scan 0 pages) or STATS_SAMPLE_PAGES=default. Internally we record
            =default as 0. See create_frm() in sql/table.cc, we use only two
            bytes for stats_sample_pages and this is why we do not allow
            larger values. 65535 pages, 16kb each means to sample 1GB, which
            is impractical. If at some point this needs to be extended, then
            we can store the higher bits from stats_sample_pages in .frm too. */
            if ($3 == 0 || $3 > 0xffff)
            {
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            $$= NEW_PTN PT_create_stats_stable_pages($3);
          }
        | STATS_SAMPLE_PAGES_SYM opt_equal DEFAULT_SYM
          {
            $$= NEW_PTN PT_create_stats_stable_pages;
          }
        | CHECKSUM_SYM opt_equal ulong_num
          {
            $$= NEW_PTN PT_create_checksum_option($3);
          }
        | TABLE_CHECKSUM_SYM opt_equal ulong_num
          {
            $$= NEW_PTN PT_create_checksum_option($3);
          }
        | DELAY_KEY_WRITE_SYM opt_equal ulong_num
          {
            $$= NEW_PTN PT_create_delay_key_write_option($3);
          }
        | ROW_FORMAT_SYM opt_equal row_types
          {
            $$= NEW_PTN PT_create_row_format_option($3);
          }
        | UNION_SYM opt_equal '(' opt_table_list ')'
          {
            $$= NEW_PTN PT_create_union_option($4);
          }
        | default_charset
        | default_collation
        | INSERT_METHOD opt_equal merge_insert_types
          {
            $$= NEW_PTN PT_create_insert_method_option($3);
          }
        | DATA_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys
          {
            $$= NEW_PTN PT_create_data_directory_option($4.str);
          }
        | INDEX_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys
          {
            $$= NEW_PTN PT_create_index_directory_option($4.str);
          }
        | TABLESPACE_SYM opt_equal ident
          {
            $$= NEW_PTN PT_create_tablespace_option($3.str);
          }
        | STORAGE_SYM DISK_SYM
          {
            $$= NEW_PTN PT_create_storage_option(HA_SM_DISK);
          }
        | STORAGE_SYM MEMORY_SYM
          {
            $$= NEW_PTN PT_create_storage_option(HA_SM_MEMORY);
          }
        | CONNECTION_SYM opt_equal TEXT_STRING_sys
          {
            $$= NEW_PTN PT_create_connection_option($3);
          }
        | KEY_BLOCK_SIZE opt_equal ulong_num
          {
            $$= NEW_PTN PT_create_key_block_size_option($3);
          }
        ;

ternary_option:
          ulong_num
          {
            switch($1) {
            case 0:
                $$= Ternary_option::OFF;
                break;
            case 1:
                $$= Ternary_option::ON;
                break;
            default:
                my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
                MYSQL_YYABORT;
            }
          }
        | DEFAULT_SYM { $$= Ternary_option::DEFAULT; }
        ;

default_charset:
          opt_default charset opt_equal charset_name_or_default
          {
            $$= NEW_PTN PT_create_table_default_charset($4);
          }
        ;

default_collation:
          opt_default COLLATE_SYM opt_equal collation_name_or_default
          {
            $$= NEW_PTN PT_create_table_default_collation($4);
          }
        ;

row_types:
          DEFAULT_SYM    { $$= ROW_TYPE_DEFAULT; }
        | FIXED_SYM      { $$= ROW_TYPE_FIXED; }
        | DYNAMIC_SYM    { $$= ROW_TYPE_DYNAMIC; }
        | COMPRESSED_SYM { $$= ROW_TYPE_COMPRESSED; }
        | REDUNDANT_SYM  { $$= ROW_TYPE_REDUNDANT; }
        | COMPACT_SYM    { $$= ROW_TYPE_COMPACT; }
        ;

merge_insert_types:
         NO_SYM          { $$= MERGE_INSERT_DISABLED; }
       | FIRST_SYM       { $$= MERGE_INSERT_TO_FIRST; }
       | LAST_SYM        { $$= MERGE_INSERT_TO_LAST; }
       ;

udf_type:
          STRING_SYM {$$ = (int) STRING_RESULT; }
        | REAL_SYM {$$ = (int) REAL_RESULT; }
        | DECIMAL_SYM {$$ = (int) DECIMAL_RESULT; }
        | INT_SYM {$$ = (int) INT_RESULT; }
        ;

table_element_list:
          table_element
          {
            $$= NEW_PTN Trivial_array<PT_table_element *>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | table_element_list ',' table_element
          {
            $$= $1;
            if ($$->push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

table_element:
          column_def            { $$= $1; }
        | table_constraint_def  { $$= $1; }
        ;

column_def:
          field_ident field_def opt_check_or_references
          {
            $$= NEW_PTN PT_column_def($1, $2, $3);
          }
        ;

opt_check_or_references:
          /* empty */   { $$= NULL; }
        | check_constraint
          {
            /*
              Currently we ignore the CHECK clause.

              Return expression for syntax validation purposes only:
            */
            $$= $1;
          }
        |  references
          {
            /* Currently we ignore FK references here: */
            $$= NULL;
          }
        ;

table_constraint_def:
          normal_key_type opt_index_name_and_type '(' key_list ')'
          opt_index_options
          {
            $$= NEW_PTN PT_inline_index_definition($1, $2.name, $2.type, $4,
                                                   $6);
          }
        | FULLTEXT_SYM opt_key_or_index opt_ident '(' key_list ')'
          opt_fulltext_index_options
          {
            $$= NEW_PTN PT_inline_index_definition(KEYTYPE_FULLTEXT, $3, NULL,
                                                   $5, $7);
          }
        | spatial opt_key_or_index opt_ident '(' key_list ')'
          opt_spatial_index_options
          {
            $$= NEW_PTN PT_inline_index_definition($1, $3, NULL, $5, $7);
          }
        | opt_constraint constraint_key_type opt_index_name_and_type
          '(' key_list ')' opt_index_options
          {
            /*
              Constraint-implementing indexes are named by the constraint type
              by default.
            */
            PT_field_ident *name= $3.name != NULL ? $3.name : $1;
            $$= NEW_PTN PT_inline_index_definition($2, name, $3.type, $5, $7);
          }
        | opt_constraint FOREIGN KEY_SYM opt_ident '(' key_list ')' references
          {
            $$= NEW_PTN PT_foreign_key_definition($1, $4, $6, $8.table_name,
                                                  $8.reference_list,
                                                  $8.fk_match_option,
                                                  $8.fk_update_opt,
                                                  $8.fk_delete_opt);
          }
        | opt_constraint check_constraint
          {
            $$= $2;
          }
        ;

check_constraint:
          CHECK_SYM '(' expr ')'
          {
            /*
              Currently we ignore CHECK clauses in the query executor.

              Return expression for syntax validation purposes only:
            */
            $$= NEW_PTN PT_check_constraint($3);
          }
        ;

opt_constraint:
          /* empty */ { $$= NULL; }
        | constraint
        ;

constraint:
          CONSTRAINT opt_ident { $$=$2; }
        ;

field_def:
          type opt_column_attribute_list
          {
            $$= NEW_PTN PT_field_def($1, $2);
          }
        | type opt_collate_explicit opt_generated_always
          AS '(' expr ')'
          opt_stored_attribute opt_column_attribute_list
          {
            auto *opt_attrs= $9;
            if ($2 != NULL)
            {
              if (opt_attrs == NULL)
              {
                opt_attrs= NEW_PTN
                  Trivial_array<PT_column_attr_base *>(YYMEM_ROOT);
                if (opt_attrs == NULL)
                  MYSQL_YYABORT; // OOM
              }
              if (opt_attrs->push_back($2))
                MYSQL_YYABORT; // OOM
            }
            $$= NEW_PTN PT_generated_field_def($1, $6, $8, opt_attrs);
          }
        ;

opt_generated_always:
          /* empty */
        | GENERATED ALWAYS_SYM
        ;

opt_stored_attribute:
          /* empty */ { $$= Virtual_or_stored::VIRTUAL; }
        | VIRTUAL_SYM { $$= Virtual_or_stored::VIRTUAL; }
        | STORED_SYM  { $$= Virtual_or_stored::STORED; }
        ;

type:
          int_type opt_field_length field_options
          {
            $$= NEW_PTN PT_numeric_type($1, $2, $3);
          }
        | real_type opt_precision field_options
          {
            $$= NEW_PTN PT_numeric_type($1, $2.length, $2.dec, $3);
          }
        | numeric_type float_options field_options
          {
            $$= NEW_PTN PT_numeric_type($1, $2.length, $2.dec, $3);
          }
        | BIT_SYM
          {
            $$= NEW_PTN PT_bit_type;
          }
        | BIT_SYM field_length
          {
            $$= NEW_PTN PT_bit_type($2);
          }
        | BOOL_SYM
          {
            $$= NEW_PTN PT_boolean_type;
          }
        | BOOLEAN_SYM
          {
            $$= NEW_PTN PT_boolean_type;
          }
        | CHAR_SYM field_length opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_char_type(Char_type::CHAR, $2, $3.charset,
                                     $3.force_binary);
          }
        | CHAR_SYM opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_char_type(Char_type::CHAR, $2.charset,
                                     $2.force_binary);
          }
        | nchar field_length opt_bin_mod
          {
            const CHARSET_INFO *cs= $3 ?
              get_bin_collation(national_charset_info) : national_charset_info;
            if (cs == NULL)
              MYSQL_YYABORT;
            $$= NEW_PTN PT_char_type(Char_type::CHAR, $2, cs);
          }
        | nchar opt_bin_mod
          {
            const CHARSET_INFO *cs= $2 ?
              get_bin_collation(national_charset_info) : national_charset_info;
            if (cs == NULL)
              MYSQL_YYABORT;
            $$= NEW_PTN PT_char_type(Char_type::CHAR, cs);
          }
        | BINARY_SYM field_length
          {
            $$= NEW_PTN PT_char_type(Char_type::CHAR, $2, &my_charset_bin);
          }
        | BINARY_SYM
          {
            $$= NEW_PTN PT_char_type(Char_type::CHAR, &my_charset_bin);
          }
        | varchar field_length opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_char_type(Char_type::VARCHAR, $2, $3.charset,
                                     $3.force_binary);
          }
        | nvarchar field_length opt_bin_mod
          {
            const CHARSET_INFO *cs= $3 ?
              get_bin_collation(national_charset_info) : national_charset_info;
            if (cs == NULL)
              MYSQL_YYABORT;
            $$= NEW_PTN PT_char_type(Char_type::VARCHAR, $2, cs);
          }
        | VARBINARY_SYM field_length
          {
            $$= NEW_PTN PT_char_type(Char_type::VARCHAR, $2, &my_charset_bin);
          }
        | YEAR_SYM opt_field_length field_options
          {
            if ($2)
            {
              errno= 0;
              ulong length= strtoul($2, NULL, 10);
              if (errno != 0 || length != 4)
              {
                /* Only support length is 4 */
                my_error(ER_INVALID_YEAR_COLUMN_LENGTH, MYF(0), "YEAR");
                MYSQL_YYABORT;
              }
            }
            // We can ignore field length and UNSIGNED/ZEROFILL attributes here.
            $$= NEW_PTN PT_year_type;
          }
        | DATE_SYM
          {
            $$= NEW_PTN PT_date_type;
          }
        | TIME_SYM type_datetime_precision
          {
            $$= NEW_PTN PT_time_type(Time_type::TIME, $2);
          }
        | TIMESTAMP_SYM type_datetime_precision
          {
            if (YYTHD->variables.sql_mode & MODE_MAXDB)
              $$= NEW_PTN PT_time_type(Time_type::DATETIME, $2);
            else
              $$= NEW_PTN PT_timestamp_type($2);
          }
        | DATETIME_SYM type_datetime_precision
          {
            $$= NEW_PTN PT_time_type(Time_type::DATETIME, $2);
          }
        | TINYBLOB_SYM
          {
            $$= NEW_PTN PT_blob_type(Blob_type::TINY, &my_charset_bin);
          }
        | BLOB_SYM opt_field_length
          {
            $$= NEW_PTN PT_blob_type($2);
          }
        | spatial_type
        | MEDIUMBLOB_SYM
          {
            $$= NEW_PTN PT_blob_type(Blob_type::MEDIUM, &my_charset_bin);
          }
        | LONGBLOB_SYM
          {
            $$= NEW_PTN PT_blob_type(Blob_type::LONG, &my_charset_bin);
          }
        | LONG_SYM VARBINARY_SYM
          {
            $$= NEW_PTN PT_blob_type(Blob_type::MEDIUM, &my_charset_bin);
          }
        | LONG_SYM varchar opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_blob_type(Blob_type::MEDIUM, $3.charset,
                                     $3.force_binary);
          }
        | TINYTEXT_SYN opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_blob_type(Blob_type::TINY, $2.charset,
                                     $2.force_binary);
          }
        | TEXT_SYM opt_field_length opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_char_type(Char_type::TEXT, $2, $3.charset,
                                     $3.force_binary);
          }
        | MEDIUMTEXT_SYM opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_blob_type(Blob_type::MEDIUM, $2.charset,
                                     $2.force_binary);
          }
        | LONGTEXT_SYM opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_blob_type(Blob_type::LONG, $2.charset,
                                     $2.force_binary);
          }
        | ENUM_SYM '(' string_list ')' opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_enum_type($3, $5.charset, $5.force_binary);
          }
        | SET_SYM '(' string_list ')' opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_set_type($3, $5.charset, $5.force_binary);
          }
        | LONG_SYM opt_charset_with_opt_binary
          {
            $$= NEW_PTN PT_blob_type(Blob_type::MEDIUM, $2.charset,
                                     $2.force_binary);
          }
        | SERIAL_SYM
          {
            $$= NEW_PTN PT_serial_type;
          }
        | JSON_SYM
          {
            $$= NEW_PTN PT_json_type;
          }
        ;

spatial_type:
          GEOMETRY_SYM
          { $$= NEW_PTN PT_spacial_type(Field::GEOM_GEOMETRY); }
        | GEOMETRYCOLLECTION_SYM
          { $$= NEW_PTN PT_spacial_type(Field::GEOM_GEOMETRYCOLLECTION); }
        | POINT_SYM
          { $$= NEW_PTN PT_spacial_type(Field::GEOM_POINT); }
        | MULTIPOINT_SYM
          { $$= NEW_PTN PT_spacial_type(Field::GEOM_MULTIPOINT); }
        | LINESTRING_SYM
          { $$= NEW_PTN PT_spacial_type(Field::GEOM_LINESTRING); }
        | MULTILINESTRING_SYM
          { $$= NEW_PTN PT_spacial_type(Field::GEOM_MULTILINESTRING); }
        | POLYGON_SYM
          { $$= NEW_PTN PT_spacial_type(Field::GEOM_POLYGON); }
        | MULTIPOLYGON_SYM
          { $$= NEW_PTN PT_spacial_type(Field::GEOM_MULTIPOLYGON); }
        ;

nchar:
          NCHAR_SYM {}
        | NATIONAL_SYM CHAR_SYM {}
        ;

varchar:
          CHAR_SYM VARYING {}
        | VARCHAR_SYM {}
        ;

nvarchar:
          NATIONAL_SYM VARCHAR_SYM {}
        | NVARCHAR_SYM {}
        | NCHAR_SYM VARCHAR_SYM {}
        | NATIONAL_SYM CHAR_SYM VARYING {}
        | NCHAR_SYM VARYING {}
        ;

int_type:
          INT_SYM       { $$=Int_type::INT; }
        | TINYINT_SYM   { $$=Int_type::TINYINT; }
        | SMALLINT_SYM  { $$=Int_type::SMALLINT; }
        | MEDIUMINT_SYM { $$=Int_type::MEDIUMINT; }
        | BIGINT_SYM    { $$=Int_type::BIGINT; }
        ;

real_type:
          REAL_SYM
          {
            $$= YYTHD->variables.sql_mode & MODE_REAL_AS_FLOAT ?
              Numeric_type::FLOAT : Numeric_type::DOUBLE;
          }
        | DOUBLE_SYM opt_PRECISION
          { $$= Numeric_type::DOUBLE; }
        ;

opt_PRECISION:
          /* empty */
        | PRECISION
        ;

numeric_type:
          FLOAT_SYM   { $$= Numeric_type::FLOAT; }
        | DECIMAL_SYM { $$= Numeric_type::DECIMAL; }
        | NUMERIC_SYM { $$= Numeric_type::DECIMAL; }
        | FIXED_SYM   { $$= Numeric_type::DECIMAL; }
        ;

float_options:
          /* empty */
          {
            $$.length= NULL;
            $$.dec= NULL;
          }
        | field_length
          {
            $$.length= $1;
            $$.dec= NULL;
          }
        | precision
        ;

precision:
          '(' NUM ',' NUM ')'
          {
            $$.length= $2.str;
            $$.dec= $4.str;
          }
        ;


type_datetime_precision:
          /* empty */                { $$= NULL; }
        | '(' NUM ')'                { $$= $2.str; }
        ;

func_datetime_precision:
          /* empty */                { $$= 0; }
        | '(' ')'                    { $$= 0; }
        | '(' NUM ')'
           {
             int error;
             $$= (ulong) my_strtoll10($2.str, NULL, &error);
           }
        ;

field_options:
          /* empty */ { $$= Field_option::NONE; }
        | field_opt_list
        ;

field_opt_list:
          field_opt_list field_option
          {
            $$= static_cast<Field_option>(static_cast<ulong>($1) |
                                          static_cast<ulong>($2));
          }
        | field_option
        ;

field_option:
          SIGNED_SYM   { $$= Field_option::NONE; } // TODO: remove undocumented ignored syntax
        | UNSIGNED_SYM { $$= Field_option::UNSIGNED; }
        | ZEROFILL_SYM { $$= Field_option::ZEROFILL_UNSIGNED; }
        ;

field_length:
          '(' LONG_NUM ')'      { $$= $2.str; }
        | '(' ULONGLONG_NUM ')' { $$= $2.str; }
        | '(' DECIMAL_NUM ')'   { $$= $2.str; }
        | '(' NUM ')'           { $$= $2.str; };

opt_field_length:
          /* empty */  { $$= NULL; /* use default length */ }
        | field_length
        ;

opt_precision:
          /* empty */
          {
            $$.length= NULL;
            $$.dec = NULL;
          }
        | precision
        ;

opt_column_attribute_list:
          /* empty */ { $$= NULL; }
        | column_attribute_list
        ;

column_attribute_list:
          column_attribute_list column_attribute
          {
            $$= $1;
            if ($$->push_back($2))
              MYSQL_YYABORT; // OOM
          }
        | column_attribute
          {
            $$=
              NEW_PTN Trivial_array<PT_column_attr_base *>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        ;

column_attribute:
          NULL_SYM
          {
            $$= NEW_PTN PT_null_column_attr;
          }
        | not NULL_SYM
          {
            $$= NEW_PTN PT_not_null_column_attr;
          }
        | DEFAULT_SYM now_or_signed_literal
          {
            $$= NEW_PTN PT_default_column_attr($2);
          }
        | ON_SYM UPDATE_SYM now
          {
            $$= NEW_PTN PT_on_update_column_attr(static_cast<uint8>($3));
          }
        | AUTO_INC
          {
            $$= NEW_PTN PT_auto_increment_column_attr;
          }
        | SERIAL_SYM DEFAULT_SYM VALUE_SYM
          {
            $$= NEW_PTN PT_serial_default_value_column_attr;
          }
        | opt_primary KEY_SYM
          {
            $$= NEW_PTN PT_primary_key_column_attr;
          }
        | UNIQUE_SYM
          {
            $$= NEW_PTN PT_unique_key_column_attr;
          }
        | UNIQUE_SYM KEY_SYM
          {
            $$= NEW_PTN PT_unique_key_column_attr;
          }
        | COMMENT_SYM TEXT_STRING_sys
          {
            $$= NEW_PTN PT_comment_column_attr($2);
          }
        | COLLATE_SYM collation_name
          {
            $$= NEW_PTN PT_collate_column_attr($2);
          }
        | COLUMN_FORMAT_SYM column_format
          {
            $$= NEW_PTN PT_column_format_column_attr($2);
          }
        | STORAGE_SYM storage_media
          {
            $$= NEW_PTN PT_storage_media_column_attr($2);
          }
        ;


column_format:
          DEFAULT_SYM { $$= COLUMN_FORMAT_TYPE_DEFAULT; }
        | FIXED_SYM   { $$= COLUMN_FORMAT_TYPE_FIXED; }
        | DYNAMIC_SYM { $$= COLUMN_FORMAT_TYPE_DYNAMIC; }
        ;

storage_media:
          DEFAULT_SYM { $$= HA_SM_DEFAULT; }
        | DISK_SYM    { $$= HA_SM_DISK; }
        | MEMORY_SYM  { $$= HA_SM_MEMORY; }
        ;

now:
          NOW_SYM func_datetime_precision
          {
            $$= $2;
          };

now_or_signed_literal:
          now
          {
            $$= NEW_PTN Item_func_now_local(@$, static_cast<uint8>($1));
          }
        | signed_literal
        ;

charset:
          CHAR_SYM SET_SYM {}
        | CHARSET {}
        ;

charset_name:
          ident_or_text
          {
            if (!($$=get_charset_by_csname($1.str,MY_CS_PRIMARY,MYF(0))))
            {
              my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
          }
        | BINARY_SYM { $$= &my_charset_bin; }
        ;

charset_name_or_default:
          charset_name { $$=$1;   }
        | DEFAULT_SYM    { $$=NULL; }
        ;

opt_load_data_charset:
          /* Empty */ { $$= NULL; }
        | charset charset_name_or_default { $$= $2; }
        ;

old_or_new_charset_name:
          ident_or_text
          {
            if (!($$=get_charset_by_csname($1.str,MY_CS_PRIMARY,MYF(0))) &&
                !($$=get_old_charset_by_name($1.str)))
            {
              my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
          }
        | BINARY_SYM { $$= &my_charset_bin; }
        ;

old_or_new_charset_name_or_default:
          old_or_new_charset_name { $$=$1;   }
        | DEFAULT_SYM    { $$=NULL; }
        ;

collation_name:
          ident_or_text
          {
            if (!($$= mysqld_collation_get_by_name($1.str)))
              MYSQL_YYABORT;
          }
        ;

opt_collate:
          /* empty */ { $$=NULL; }
        | COLLATE_SYM collation_name_or_default { $$=$2; }
        ;

opt_collate_explicit:
          /* empty */ { $$= NULL; }
        | COLLATE_SYM collation_name
          { $$= NEW_PTN PT_collate_column_attr($2); }
        ;

collation_name_or_default:
          collation_name { $$=$1; }
        | DEFAULT_SYM    { $$=NULL; }
        ;

opt_default:
          /* empty */ {}
        | DEFAULT_SYM {}
        ;


ascii:
          ASCII_SYM        { $$= &my_charset_latin1; }
        | BINARY_SYM ASCII_SYM { $$= &my_charset_latin1_bin; }
        | ASCII_SYM BINARY_SYM { $$= &my_charset_latin1_bin; }
        ;

unicode:
          UNICODE_SYM
          {
            if (!($$= get_charset_by_csname("ucs2", MY_CS_PRIMARY,MYF(0))))
            {
              my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), "ucs2");
              MYSQL_YYABORT;
            }
          }
        | UNICODE_SYM BINARY_SYM
          {
            if (!($$= mysqld_collation_get_by_name("ucs2_bin")))
              MYSQL_YYABORT;
          }
        | BINARY_SYM UNICODE_SYM
          {
            if (!($$= mysqld_collation_get_by_name("ucs2_bin")))
              my_error(ER_UNKNOWN_COLLATION, MYF(0), "ucs2_bin");
          }
        ;

opt_charset_with_opt_binary:
          /* empty */
          {
            $$.charset= NULL;
            $$.force_binary= false;
          }
        | ascii
          {
            $$.charset= $1;
            $$.force_binary= false;
          }
        | unicode
          {
            $$.charset= $1;
            $$.force_binary= false;
          }
        | BYTE_SYM
          {
            $$.charset= &my_charset_bin;
            $$.force_binary= false;
          }
        | charset charset_name opt_bin_mod
          {
            $$.charset= $3 ? get_bin_collation($2) : $2;
            if ($$.charset == NULL)
              MYSQL_YYABORT;
            $$.force_binary= false;
          }
        | BINARY_SYM
          {
            $$.charset= NULL;
            $$.force_binary= true;
          }
        | BINARY_SYM charset charset_name
          {
            $$.charset= get_bin_collation($3);
            if ($$.charset == NULL)
              MYSQL_YYABORT;
            $$.force_binary= false;
          }
        ;

opt_bin_mod:
          /* empty */ { $$= false; }
        | BINARY_SYM  { $$= true; }
        ;

ws_num_codepoints:
        '(' real_ulong_num
        {
          if ($2 == 0)
          {
            my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
            MYSQL_YYABORT;
          }
        }
        ')'
        { $$= $2; }
        ;

opt_primary:
          /* empty */
        | PRIMARY_SYM
        ;

references:
          REFERENCES
          table_ident
          opt_ref_list
          opt_match_clause
          opt_on_update_delete
          {
            $$.table_name= $2;
            $$.reference_list= $3;
            $$.fk_match_option= $4;
            $$.fk_update_opt= $5.fk_update_opt;
            $$.fk_delete_opt= $5.fk_delete_opt;
          }
        ;

opt_ref_list:
          /* empty */      { $$= NULL; }
        | '(' reference_list ')' { $$= $2; }
        ;

reference_list:
          reference_list ',' ident
          {
            $$= $1;
            auto key= NEW_PTN Key_part_spec(to_lex_cstring($3), 0, ORDER_ASC);
            if (key == NULL || $$->push_back(key))
              MYSQL_YYABORT;
          }
        | ident
          {
            $$= NEW_PTN List<Key_part_spec>;
            auto key= NEW_PTN Key_part_spec(to_lex_cstring($1), 0, ORDER_ASC);
            if ($$ == NULL || key == NULL || $$->push_back(key))
              MYSQL_YYABORT;
          }
        ;

opt_match_clause:
          /* empty */      { $$= FK_MATCH_UNDEF; }
        | MATCH FULL       { $$= FK_MATCH_FULL; }
        | MATCH PARTIAL    { $$= FK_MATCH_PARTIAL; }
        | MATCH SIMPLE_SYM { $$= FK_MATCH_SIMPLE; }
        ;

opt_on_update_delete:
          /* empty */
          {
            $$.fk_update_opt= FK_OPTION_UNDEF;
            $$.fk_delete_opt= FK_OPTION_UNDEF;
          }
        | ON_SYM UPDATE_SYM delete_option
          {
            $$.fk_update_opt= $3;
            $$.fk_delete_opt= FK_OPTION_UNDEF;
          }
        | ON_SYM DELETE_SYM delete_option
          {
            $$.fk_update_opt= FK_OPTION_UNDEF;
            $$.fk_delete_opt= $3;
          }
        | ON_SYM UPDATE_SYM delete_option
          ON_SYM DELETE_SYM delete_option
          {
            $$.fk_update_opt= $3;
            $$.fk_delete_opt= $6;
          }
        | ON_SYM DELETE_SYM delete_option
          ON_SYM UPDATE_SYM delete_option
          {
            $$.fk_update_opt= $6;
            $$.fk_delete_opt= $3;
          }
        ;

delete_option:
          RESTRICT      { $$= FK_OPTION_RESTRICT; }
        | CASCADE       { $$= FK_OPTION_CASCADE; }
        | SET_SYM NULL_SYM  { $$= FK_OPTION_SET_NULL; }
        | NO_SYM ACTION { $$= FK_OPTION_NO_ACTION; }
        | SET_SYM DEFAULT_SYM { $$= FK_OPTION_DEFAULT;  }
        ;

normal_key_type:
          key_or_index { $$= KEYTYPE_MULTIPLE; }
        ;

constraint_key_type:
          PRIMARY_SYM KEY_SYM { $$= KEYTYPE_PRIMARY; }
        | UNIQUE_SYM opt_key_or_index { $$= KEYTYPE_UNIQUE; }
        ;

key_or_index:
          KEY_SYM {}
        | INDEX_SYM {}
        ;

opt_key_or_index:
          /* empty */ {}
        | key_or_index
        ;

keys_or_index:
          KEYS {}
        | INDEX_SYM {}
        | INDEXES {}
        ;

opt_unique:
          /* empty */  { $$= KEYTYPE_MULTIPLE; }
        | UNIQUE_SYM   { $$= KEYTYPE_UNIQUE; }
        ;

spatial:
          SPATIAL_SYM
          {
            $$= KEYTYPE_SPATIAL;
          }
        ;

opt_fulltext_index_options:
          /* Empty. */ { $$.init(YYMEM_ROOT); }
        | fulltext_index_options
        ;

fulltext_index_options:
          fulltext_index_option
          {
            $$.init(YYMEM_ROOT);
            if ($$.push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | fulltext_index_options fulltext_index_option
          {
            if ($1.push_back($2))
              MYSQL_YYABORT; // OOM
            $$= $1;
          }
        ;

fulltext_index_option:
          common_index_option
        | WITH PARSER_SYM IDENT_sys
          {
            LEX_CSTRING plugin_name= {$3.str, $3.length};
            if (!plugin_is_ready(plugin_name, MYSQL_FTPARSER_PLUGIN))
            {
              my_error(ER_FUNCTION_NOT_DEFINED, MYF(0), $3.str);
              MYSQL_YYABORT;
            }
            else
              $$= NEW_PTN PT_fulltext_index_parser_name(to_lex_cstring($3));
          }
        ;

opt_spatial_index_options:
          /* Empty. */ { $$.init(YYMEM_ROOT); }
        | spatial_index_options
        ;

spatial_index_options:
          spatial_index_option
          {
            $$.init(YYMEM_ROOT);
            if ($$.push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | spatial_index_options spatial_index_option
          {
            if ($1.push_back($2))
              MYSQL_YYABORT; // OOM
            $$= $1;
          }
        ;

spatial_index_option:
          common_index_option
        ;

opt_index_options:
          /* Empty. */ { $$.init(YYMEM_ROOT); }
        | index_options
        ;

index_options:
          index_option
          {
            $$.init(YYMEM_ROOT);
            if ($$.push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | index_options index_option
          {
            if ($1.push_back($2))
              MYSQL_YYABORT; // OOM
            $$= $1;
          }
        ;

index_option:
          common_index_option { $$= $1; }
        | index_type_clause { $$= $1; }
        ;

// These options are common for all index types.
common_index_option:
          KEY_BLOCK_SIZE opt_equal ulong_num { $$= NEW_PTN PT_block_size($3); }
        | COMMENT_SYM TEXT_STRING_sys
          {
            $$= NEW_PTN PT_index_comment(to_lex_cstring($2));
          }
        | visibility
          {
            $$= NEW_PTN PT_index_visibility($1);
          }
        ;

/*
  The syntax for defining an index is:

    ... INDEX [index_name] [USING|TYPE] <index_type> ...

  The problem is that whereas USING is a reserved word, TYPE is not. We can
  still handle it if an index name is supplied, i.e.:

    ... INDEX type TYPE <index_type> ...

  here the index's name is unmbiguously 'type', but for this:

    ... INDEX TYPE <index_type> ...

  it's impossible to know what this actually mean - is 'type' the name or the
  type? For this reason we accept the TYPE syntax only if a name is supplied.
*/
opt_index_name_and_type:
          opt_ident                  { $$= {$1, NULL}; }
        | opt_ident USING index_type { $$= {$1, NEW_PTN PT_index_type($3)}; }
        | ident TYPE_SYM index_type
          {
            $$= { NEW_PTN PT_field_ident($1), NEW_PTN PT_index_type($3) };
          }
        ;

index_type_clause:
          USING index_type    { $$= NEW_PTN PT_index_type($2); }
        | TYPE_SYM index_type { $$= NEW_PTN PT_index_type($2); }
        ;

visibility:
          VISIBLE_SYM { $$= true; }
        | INVISIBLE_SYM { $$= false; }
        ;

index_type:
          BTREE_SYM { $$= HA_KEY_ALG_BTREE; }
        | RTREE_SYM { $$= HA_KEY_ALG_RTREE; }
        | HASH_SYM  { $$= HA_KEY_ALG_HASH; }
        ;

key_list:
          key_list ',' key_part
          {
            if ($1->push_back($3))
              MYSQL_YYABORT; // OOM
            $$= $1;
          }
        | key_part
          {
            // The order is ignored.
            $$= new List<Key_part_spec>;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        ;

key_part:
          ident order_dir
          {
            $$= new Key_part_spec(to_lex_cstring($1), 0, (enum_order) $2);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | ident '(' NUM ')' order_dir
          {
            int key_part_len= atoi($3.str);
            if (!key_part_len)
            {
              my_error(ER_KEY_PART_0, MYF(0), $1.str);
            }
            $$= new Key_part_spec(to_lex_cstring($1), (uint) key_part_len,
                                  (enum_order) $5);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_ident:
          /* empty */ { $$= NULL; }
        | field_ident
        ;

opt_component:
          /* empty */    { $$= null_lex_str; }
        | '.' ident      { $$= $2; }
        ;

string_list:
          text_string
          {
            $$= NEW_PTN List<String>;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | string_list ',' text_string
          {
            if ($$->push_back($3))
              MYSQL_YYABORT;
          }
        ;

/*
** Alter table
*/

alter:
          ALTER TABLE_SYM table_ident
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->name.str= 0;
            lex->name.length= 0;
            lex->sql_command= SQLCOM_ALTER_TABLE;
            lex->duplicates= DUP_ERROR;
            if (!lex->select_lex->add_table_to_list(thd, $3, NULL,
                                                    TL_OPTION_UPDATING,
                                                    TL_READ_NO_INSERT,
                                                    MDL_SHARED_UPGRADABLE))
              MYSQL_YYABORT;
            lex->select_lex->init_order();
            lex->select_lex->db=
                    const_cast<char*>((lex->select_lex->table_list.first)->db);
            lex->create_info= YYTHD->alloc_typed<HA_CREATE_INFO>();
            if (lex->create_info == NULL)
              MYSQL_YYABORT; // OOM
            lex->create_info->db_type= 0;
            lex->create_info->default_table_charset= NULL;
            lex->create_info->row_type= ROW_TYPE_NOT_USED;
            lex->alter_info.reset();
            lex->no_write_to_binlog= 0;
            lex->create_info->storage_media= HA_SM_DEFAULT;
            lex->create_last_non_select_table= lex->last_table();
            DBUG_ASSERT(!lex->m_sql_cmd);
          }
          alter_commands
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            if (!lex->m_sql_cmd)
            {
              /* Create a generic ALTER TABLE statment. */
              lex->m_sql_cmd= NEW_PTN Sql_cmd_alter_table();
              if (lex->m_sql_cmd == NULL)
                MYSQL_YYABORT;
            }
          }
        | ALTER DATABASE ident_or_empty
          {
            Lex->create_info= YYTHD->alloc_typed<HA_CREATE_INFO>();
            if (Lex->create_info == NULL)
              MYSQL_YYABORT; // OOM
            Lex->create_info->default_table_charset= NULL;
            Lex->create_info->used_fields= 0;
          }
          create_database_options
          {
            LEX *lex=Lex;
            lex->sql_command=SQLCOM_ALTER_DB;
            lex->name= $3;
            if (lex->name.str == NULL &&
                lex->copy_db_to(&lex->name.str, &lex->name.length))
              MYSQL_YYABORT;
          }
        | ALTER PROCEDURE_SYM sp_name
          {
            LEX *lex= Lex;

            if (lex->sphead)
            {
              my_error(ER_SP_NO_DROP_SP, MYF(0), "PROCEDURE");
              MYSQL_YYABORT;
            }
            memset(&lex->sp_chistics, 0, sizeof(st_sp_chistics));
          }
          sp_a_chistics
          {
            LEX *lex=Lex;

            lex->sql_command= SQLCOM_ALTER_PROCEDURE;
            lex->spname= $3;
          }
        | ALTER FUNCTION_SYM sp_name
          {
            LEX *lex= Lex;

            if (lex->sphead)
            {
              my_error(ER_SP_NO_DROP_SP, MYF(0), "FUNCTION");
              MYSQL_YYABORT;
            }
            memset(&lex->sp_chistics, 0, sizeof(st_sp_chistics));
          }
          sp_a_chistics
          {
            LEX *lex=Lex;

            lex->sql_command= SQLCOM_ALTER_FUNCTION;
            lex->spname= $3;
          }
        | ALTER view_algorithm definer_opt
          {
            LEX *lex= Lex;

            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "ALTER VIEW");
              MYSQL_YYABORT;
            }
            lex->create_view_mode= enum_view_create_mode::VIEW_ALTER;
          }
          view_tail
          {}
        | ALTER definer_opt
          /*
            We have two separate rules for ALTER VIEW rather that
            optional view_algorithm above, to resolve the ambiguity
            with the ALTER EVENT below.
          */
          {
            LEX *lex= Lex;

            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "ALTER VIEW");
              MYSQL_YYABORT;
            }
            lex->create_view_algorithm= VIEW_ALGORITHM_UNDEFINED;
            lex->create_view_mode= enum_view_create_mode::VIEW_ALTER;
          }
          view_tail
          {}
        | ALTER definer_opt EVENT_SYM sp_name
          {
            /*
              It is safe to use Lex->spname because
              ALTER EVENT xxx RENATE TO yyy DO ALTER EVENT RENAME TO
              is not allowed. Lex->spname is used in the case of RENAME TO
              If it had to be supported spname had to be added to
              Event_parse_data.
            */

            if (!(Lex->event_parse_data= new (YYTHD->mem_root) Event_parse_data()))
              MYSQL_YYABORT;
            Lex->event_parse_data->identifier= $4;

            Lex->sql_command= SQLCOM_ALTER_EVENT;
          }
          ev_alter_on_schedule_completion
          opt_ev_rename_to
          opt_ev_status
          opt_ev_comment
          opt_ev_sql_stmt
          {
            if (!($6 || $7 || $8 || $9 || $10))
            {
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            /*
              sql_command is set here because some rules in ev_sql_stmt
              can overwrite it
            */
            Lex->sql_command= SQLCOM_ALTER_EVENT;
          }
        | ALTER TABLESPACE_SYM alter_tablespace_info
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->ts_cmd_type= ALTER_TABLESPACE;
          }
        | ALTER LOGFILE_SYM GROUP_SYM alter_logfile_group_info
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->ts_cmd_type= ALTER_LOGFILE_GROUP;
          }
        | ALTER TABLESPACE_SYM change_tablespace_info
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->ts_cmd_type= CHANGE_FILE_TABLESPACE;
          }
        | ALTER TABLESPACE_SYM change_tablespace_access
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->ts_cmd_type= ALTER_ACCESS_MODE_TABLESPACE;
          }
        | ALTER SERVER_SYM ident_or_text OPTIONS_SYM '(' server_options_list ')'
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_ALTER_SERVER;
            lex->server_options.m_server_name= $3;
            lex->m_sql_cmd=
              NEW_PTN Sql_cmd_alter_server(&Lex->server_options);
          }
        | alter_user_command grant_list require_clause
          connect_options opt_account_lock_password_expire_options
        | alter_user_command user_func IDENTIFIED_SYM BY TEXT_STRING
          {
            $2->auth.str= $5.str;
            $2->auth.length= $5.length;
            $2->uses_identified_by_clause= true;
            Lex->contains_plaintext_password= true;
          }
        | alter_instance_stmt { MAKE_CMD($1); }
        | alter_user_command user DEFAULT_SYM ROLE_SYM ALL
          {
            List<LEX_USER> *users= new List<LEX_USER>;
            if (users == NULL || users->push_back($2))
              MYSQL_YYABORT;
            List<LEX_USER> *role_list= new List<LEX_USER>;
            Lex->sql_command= SQLCOM_ALTER_USER_DEFAULT_ROLE;
              PT_statement *tmp=
                NEW_PTN PT_alter_user_default_role(Lex->drop_if_exists,
                                                   users, role_list, ROLE_ALL);
              MAKE_CMD(tmp);
          }
        | alter_user_command user DEFAULT_SYM ROLE_SYM NONE_SYM
          {
            List<LEX_USER> *users= new List<LEX_USER>;
            if (users == NULL || users->push_back($2))
              MYSQL_YYABORT;
            List<LEX_USER> *role_list= new List<LEX_USER>;
            Lex->sql_command= SQLCOM_ALTER_USER_DEFAULT_ROLE;
              PT_statement *tmp=
                NEW_PTN PT_alter_user_default_role(Lex->drop_if_exists,
                                                   users, role_list, ROLE_NONE);
              MAKE_CMD(tmp);
          }
        | alter_user_command user DEFAULT_SYM ROLE_SYM role_list
          {
            List<LEX_USER> *users= new List<LEX_USER>;
            if (users == NULL || users->push_back($2))
              MYSQL_YYABORT;
            Lex->sql_command= SQLCOM_ALTER_USER_DEFAULT_ROLE;
            PT_statement *tmp=
              NEW_PTN PT_alter_user_default_role(Lex->drop_if_exists,
                                                 users, $5, ROLE_NAME);
            MAKE_CMD(tmp);
          }
        ;

alter_user_command:
          ALTER USER if_exists
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_ALTER_USER;
            lex->drop_if_exists= $3;
          }
        ;

opt_account_lock_password_expire_options:
          /* empty */ {}
        | opt_account_lock_password_expire_option_list
        ;

opt_account_lock_password_expire_option_list:
          opt_account_lock_password_expire_option
        | opt_account_lock_password_expire_option_list opt_account_lock_password_expire_option
        ;

opt_account_lock_password_expire_option:
          ACCOUNT_SYM UNLOCK_SYM
          {
            LEX *lex=Lex;
            lex->alter_password.update_account_locked_column= true;
            lex->alter_password.account_locked= false;
          }
        | ACCOUNT_SYM LOCK_SYM
          {
            LEX *lex=Lex;
            lex->alter_password.update_account_locked_column= true;
            lex->alter_password.account_locked= true;
          }
        | PASSWORD EXPIRE_SYM
          {
            LEX *lex= Lex;
            lex->alter_password.expire_after_days= 0;
            lex->alter_password.update_password_expired_column= true;
            lex->alter_password.update_password_expired_fields= true;
            lex->alter_password.use_default_password_lifetime= true;
          }
        | PASSWORD EXPIRE_SYM INTERVAL_SYM real_ulong_num DAY_SYM
          {
            LEX *lex= Lex;
            if ($4 == 0 || $4 > UINT_MAX16)
            {
              char buf[MAX_BIGINT_WIDTH + 1];
              my_snprintf(buf, sizeof(buf), "%lu", $4);
              my_error(ER_WRONG_VALUE, MYF(0), "DAY", buf);
              MYSQL_YYABORT;
            }
            lex->alter_password.expire_after_days= $4;
            lex->alter_password.update_password_expired_column= false;
            lex->alter_password.update_password_expired_fields= true;
            lex->alter_password.use_default_password_lifetime= false;
          }
        | PASSWORD EXPIRE_SYM NEVER_SYM
          {
            LEX *lex= Lex;
            lex->alter_password.expire_after_days= 0;
            lex->alter_password.update_password_expired_column= false;
            lex->alter_password.update_password_expired_fields= true;
            lex->alter_password.use_default_password_lifetime= false;
          }
        | PASSWORD EXPIRE_SYM DEFAULT_SYM
          {
            LEX *lex= Lex;
            lex->alter_password.expire_after_days= 0;
            lex->alter_password.update_password_expired_column= false;
            Lex->alter_password.update_password_expired_fields= true;
            lex->alter_password.use_default_password_lifetime= true;
          }
        ;

connect_options:
          /* empty */ {}
        | WITH connect_option_list
        ;

connect_option_list:
          connect_option_list connect_option {}
        | connect_option {}
        ;

connect_option:
          MAX_QUERIES_PER_HOUR ulong_num
          {
            LEX *lex=Lex;
            lex->mqh.questions=$2;
            lex->mqh.specified_limits|= USER_RESOURCES::QUERIES_PER_HOUR;
          }
        | MAX_UPDATES_PER_HOUR ulong_num
          {
            LEX *lex=Lex;
            lex->mqh.updates=$2;
            lex->mqh.specified_limits|= USER_RESOURCES::UPDATES_PER_HOUR;
          }
        | MAX_CONNECTIONS_PER_HOUR ulong_num
          {
            LEX *lex=Lex;
            lex->mqh.conn_per_hour= $2;
            lex->mqh.specified_limits|= USER_RESOURCES::CONNECTIONS_PER_HOUR;
          }
        | MAX_USER_CONNECTIONS_SYM ulong_num
          {
            LEX *lex=Lex;
            lex->mqh.user_conn= $2;
            lex->mqh.specified_limits|= USER_RESOURCES::USER_CONNECTIONS;
          }
        ;

user_func:
          USER '(' ')'
          {
            /* empty LEX_USER means current_user */
            LEX_USER *curr_user;
            if (!(curr_user= (LEX_USER*) Lex->thd->alloc(sizeof(st_lex_user))))
              MYSQL_YYABORT;

            memset(curr_user, 0, sizeof(st_lex_user));
            Lex->users_list.push_back(curr_user);
            $$= curr_user;
          }
        ;

ev_alter_on_schedule_completion:
          /* empty */ { $$= 0;}
        | ON_SYM SCHEDULE_SYM ev_schedule_time { $$= 1; }
        | ev_on_completion { $$= 1; }
        | ON_SYM SCHEDULE_SYM ev_schedule_time ev_on_completion { $$= 1; }
        ;

opt_ev_rename_to:
          /* empty */ { $$= 0;}
        | RENAME TO_SYM sp_name
          {
            /*
              Use lex's spname to hold the new name.
              The original name is in the Event_parse_data object
            */
            Lex->spname= $3;
            $$= 1;
          }
        ;

opt_ev_sql_stmt:
          /* empty*/ { $$= 0;}
        | DO_SYM ev_sql_stmt { $$= 1; }
        ;

ident_or_empty:
          /* empty */ { $$.str= 0; $$.length= 0; }
        | ident { $$= $1; }
        ;

alter_commands:
          alter_command_list
        | alter_command_list partition_clause
          {
            Lex->alter_info.flags|= Alter_info::ALTER_PARTITION;
            CONTEXTUALIZE($2);
            Lex->part_info= &$2->part_info;
          }
        | alter_command_list remove_partitioning
        | standalone_alter_commands
        | alter_commands_modifier_list ',' standalone_alter_commands
        ;

alter_command_list:
	  /* empty */
        | alter_commands_modifier_list
        | alter_list
        | alter_commands_modifier_list ',' alter_list
        ;

standalone_alter_commands:
          DISCARD TABLESPACE_SYM
          {
            Lex->alter_info.flags|= Alter_info::ALTER_DISCARD_TABLESPACE;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_discard_import_tablespace();
            if (Lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | IMPORT TABLESPACE_SYM
          {
            Lex->alter_info.flags|= Alter_info::ALTER_IMPORT_TABLESPACE;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_discard_import_tablespace();
            if (Lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
/*
  This part was added for release 5.1 by Mikael Ronström.
  From here we insert a number of commands to manage the partitions of a
  partitioned table such as adding partitions, dropping partitions,
  reorganising partitions in various manners. In future releases the list
  will be longer.
*/
        | add_partition_rule
          { CONTEXTUALIZE($1); }
        | DROP PARTITION_SYM ident_string_list
          {
            Lex->alter_info.flags|= Alter_info::ALTER_DROP_PARTITION;
            DBUG_ASSERT(Lex->alter_info.partition_names.is_empty());
            Lex->alter_info.partition_names= *$3;
          }
        | REBUILD_SYM PARTITION_SYM opt_no_write_to_binlog
          all_or_alt_part_name_list
          {
            LEX *lex= Lex;
            lex->alter_info.flags|= Alter_info::ALTER_REBUILD_PARTITION;
            lex->no_write_to_binlog= $3;
          }
        | OPTIMIZE PARTITION_SYM opt_no_write_to_binlog
          all_or_alt_part_name_list
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->no_write_to_binlog= $3;
            lex->check_opt.init();
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_alter_table_optimize_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
          opt_no_write_to_binlog
        | ANALYZE_SYM PARTITION_SYM opt_no_write_to_binlog
          all_or_alt_part_name_list
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->no_write_to_binlog= $3;
            lex->check_opt.init();
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_alter_table_analyze_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | CHECK_SYM PARTITION_SYM all_or_alt_part_name_list opt_mi_check_types
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->check_opt.init();
            lex->check_opt.flags|= $4.flags;
            lex->check_opt.sql_flags|= $4.sql_flags;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_alter_table_check_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | REPAIR PARTITION_SYM opt_no_write_to_binlog
          all_or_alt_part_name_list
          opt_mi_repair_types
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->no_write_to_binlog= $3;
            lex->check_opt.init();
            lex->check_opt.flags|= $5.flags;
            lex->check_opt.sql_flags|= $5.sql_flags;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_alter_table_repair_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | COALESCE PARTITION_SYM opt_no_write_to_binlog real_ulong_num
          {
            LEX *lex= Lex;
            lex->alter_info.flags|= Alter_info::ALTER_COALESCE_PARTITION;
            lex->no_write_to_binlog= $3;
            lex->alter_info.num_parts= $4;
          }
        | TRUNCATE_SYM PARTITION_SYM all_or_alt_part_name_list
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->check_opt.init();
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_alter_table_truncate_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | reorg_partition_rule
        | EXCHANGE_SYM PARTITION_SYM ident
          WITH TABLE_SYM table_ident opt_validation
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            String *s= NEW_PTN String((const char *) $3.str,
                                      $3.length,
                                      system_charset_info);
            if (s == NULL || lex->alter_info.partition_names.push_back(s))
              MYSQL_YYABORT;

            size_t dummy;
            lex->select_lex->db= const_cast<char*>($6->db.str);
            if (lex->select_lex->db == NULL &&
                lex->copy_db_to(&lex->select_lex->db, &dummy))
            {
              MYSQL_YYABORT;
            }
            lex->name.str= const_cast<char*>($6->table.str);
            lex->name.length= $6->table.length;
            lex->alter_info.flags|= Alter_info::ALTER_EXCHANGE_PARTITION;
            if (!lex->select_lex->add_table_to_list(thd, $6, NULL,
                                                    TL_OPTION_UPDATING,
                                                    TL_READ_NO_INSERT,
                                                    MDL_SHARED_NO_WRITE))
              MYSQL_YYABORT;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_alter_table_exchange_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | DISCARD PARTITION_SYM all_or_alt_part_name_list
          TABLESPACE_SYM
          {
            Lex->alter_info.flags|= Alter_info::ALTER_DISCARD_TABLESPACE;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_discard_import_tablespace();
            if (Lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | IMPORT PARTITION_SYM all_or_alt_part_name_list
          TABLESPACE_SYM
          {
            Lex->alter_info.flags|= Alter_info::ALTER_IMPORT_TABLESPACE;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_discard_import_tablespace();
            if (Lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_validation:
          /* empty */
        | alter_opt_validation
        ;

alter_opt_validation:
        WITH VALIDATION_SYM
          {
            Lex->alter_info.with_validation= Alter_info::ALTER_WITH_VALIDATION;
          }
        | WITHOUT_SYM VALIDATION_SYM
          {
            Lex->alter_info.with_validation=
              Alter_info::ALTER_WITHOUT_VALIDATION;
          }
	    ;

remove_partitioning:
          REMOVE_SYM PARTITIONING_SYM
          {
            Lex->alter_info.flags|= Alter_info::ALTER_REMOVE_PARTITIONING;
          }
        ;

all_or_alt_part_name_list:
          ALL
          {
            Lex->alter_info.flags|= Alter_info::ALTER_ALL_PARTITION;
          }
        | ident_string_list
          {
            DBUG_ASSERT(Lex->alter_info.partition_names.is_empty());
            Lex->alter_info.partition_names= *$1;
          }
        ;

add_partition_rule:
          ADD PARTITION_SYM opt_no_write_to_binlog
          {
            $$= NEW_PTN PT_add_partition($3);
          }
        | ADD PARTITION_SYM opt_no_write_to_binlog '(' part_def_list ')'
          {
            $$= NEW_PTN PT_add_partition_def_list($3, $5);
          }
        | ADD PARTITION_SYM opt_no_write_to_binlog PARTITIONS_SYM real_ulong_num
          {
            $$= NEW_PTN PT_add_partition_num($3, $5);
          }
        ;

reorg_partition_rule:
          REORGANIZE_SYM PARTITION_SYM opt_no_write_to_binlog
          {
            LEX * const lex= Lex;
            lex->part_info= NEW_PTN partition_info();
            if (!lex->part_info)
              MYSQL_YYABORT;
            lex->no_write_to_binlog= $3;
            lex->alter_info.flags|= Alter_info::ALTER_TABLE_REORG;
          }
        | REORGANIZE_SYM PARTITION_SYM opt_no_write_to_binlog
          ident_string_list INTO '(' part_def_list ')'
          {
            LEX * const lex= Lex;
            lex->no_write_to_binlog= $3;
            lex->alter_info.flags|= Alter_info::ALTER_REORGANIZE_PARTITION;

            DBUG_ASSERT(lex->alter_info.partition_names.is_empty());
            lex->alter_info.partition_names= *$4;

            partition_info * const part_info= NEW_PTN partition_info();
            if (part_info == NULL)
              MYSQL_YYABORT;

            Partition_parse_context pc(YYTHD, part_info);
            if (YYTHD->is_error())
              MYSQL_YYABORT;

            for (auto part_def : *$7)
            {
              if (part_def->contextualize(&pc))
                MYSQL_YYABORT;
            }

            part_info->num_parts= part_info->partitions.elements;

            lex->part_info= part_info;
          }
        ;

/*
  End of management of partition commands
*/

alter_list:
          alter_list_item
        | alter_list ',' alter_list_item
        | alter_list ',' alter_commands_modifier
        ;

alter_commands_modifier_list:
          alter_commands_modifier
        | alter_commands_modifier_list ',' alter_commands_modifier
        ;

add_column:
          ADD opt_column
          {
            LEX *lex=Lex;
            lex->alter_info.flags|= Alter_info::ALTER_ADD_COLUMN;
          }
        ;

alter_list_item:
          add_column field_ident field_def opt_check_or_references opt_place
          {
            CONTEXTUALIZE($2);
            CONTEXTUALIZE($3);
            if ($4)
              CONTEXTUALIZE($4);

            Lex->alter_info.flags|= $3->alter_info_flags;
            if (Lex->alter_info.add_field(YYTHD,
                                          &$2->field_name,
                                          $3->type,
                                          $3->length,
                                          $3->dec,
                                          $3->type_flags,
                                          $3->default_value,
                                          $3->on_update_value,
                                          &$3->comment,
                                          NULL,
                                          $3->interval_list,
                                          $3->charset,
                                          $3->uint_geom_type,
                                          $3->gcol_info,
                                          $5))
              MYSQL_YYABORT;

            Lex->create_last_non_select_table= Lex->last_table();
          }
        | ADD table_constraint_def
          {
            CONTEXTUALIZE($2);
            Lex->create_last_non_select_table= Lex->last_table();
            Lex->alter_info.flags|= Alter_info::ALTER_ADD_INDEX;
          }
        | add_column '(' table_element_list ')'
          {
            for (auto element : *$3)
              CONTEXTUALIZE(element);
            Lex->create_last_non_select_table= Lex->last_table();
          }
        | CHANGE opt_column field_ident field_ident
          field_def
          opt_place
          {
            LEX *lex=Lex;
            CONTEXTUALIZE($3);
            CONTEXTUALIZE($4);
            CONTEXTUALIZE($5);
            lex->alter_info.flags|= $5->alter_info_flags;
            if (lex->alter_info.add_field(YYTHD, &$4->field_name, $5->type,
                                          $5->length, $5->dec, $5->type_flags,
                                          $5->default_value,
                                          $5->on_update_value,
                                          &$5->comment,
                                          $3->field_name.str,
                                          $5->interval_list,
                                          $5->charset,
                                          $5->uint_geom_type,
                                          $5->gcol_info,
                                          $6))
              MYSQL_YYABORT;
            lex->alter_info.flags|= Alter_info::ALTER_CHANGE_COLUMN;
            if ($5->default_value)
               lex->alter_info.flags|= Alter_info::ALTER_CHANGE_COLUMN_DEFAULT;
            Lex->create_last_non_select_table= Lex->last_table();
          }
        | MODIFY_SYM opt_column field_ident
          field_def
          opt_place
          {
            LEX *lex=Lex;
            CONTEXTUALIZE($3);
            CONTEXTUALIZE($4);
            lex->alter_info.flags|= $4->alter_info_flags;
            if (lex->alter_info.add_field(YYTHD, &$3->field_name,
                                          $4->type,
                                          $4->length, $4->dec, $4->type_flags,
                                          $4->default_value,
                                          $4->on_update_value,
                                          &$4->comment,
                                          $3->field_name.str,
                                          $4->interval_list,
                                          $4->charset,
                                          $4->uint_geom_type,
                                          $4->gcol_info,
                                          $5))
              MYSQL_YYABORT;
            lex->alter_info.flags|= Alter_info::ALTER_CHANGE_COLUMN;
            if ($4->default_value)
               lex->alter_info.flags|= Alter_info::ALTER_CHANGE_COLUMN_DEFAULT;
            Lex->create_last_non_select_table= Lex->last_table();
          }
        | DROP opt_column field_ident opt_restrict
          {
            LEX *lex=Lex;
            CONTEXTUALIZE($3);
            lex->drop_mode= $4;
            auto ad= new Alter_drop(Alter_drop::COLUMN, $3->field_name.str);
            if (ad == NULL)
              MYSQL_YYABORT;
            lex->alter_info.drop_list.push_back(ad);
            lex->alter_info.flags|= Alter_info::ALTER_DROP_COLUMN;
          }
        | DROP FOREIGN KEY_SYM field_ident
          {
            LEX *lex=Lex;
            CONTEXTUALIZE($4);
            auto ad= new Alter_drop(Alter_drop::FOREIGN_KEY, $4->field_name.str);
            if (ad == NULL)
              MYSQL_YYABORT;
            lex->alter_info.drop_list.push_back(ad);
            lex->alter_info.flags|= Alter_info::DROP_FOREIGN_KEY;
          }
        | DROP PRIMARY_SYM KEY_SYM
          {
            LEX *lex=Lex;
            Alter_drop *ad= new Alter_drop(Alter_drop::KEY, primary_key_name);
            if (ad == NULL)
              MYSQL_YYABORT;
            lex->alter_info.drop_list.push_back(ad);
            lex->alter_info.flags|= Alter_info::ALTER_DROP_INDEX;
          }
        | DROP key_or_index field_ident
          {
            LEX *lex=Lex;
            CONTEXTUALIZE($3);
            auto ad= new Alter_drop(Alter_drop::KEY, $3->field_name.str);
            if (ad == NULL)
              MYSQL_YYABORT;
            lex->alter_info.drop_list.push_back(ad);
            lex->alter_info.flags|= Alter_info::ALTER_DROP_INDEX;
          }
        | DISABLE_SYM KEYS
          {
            LEX *lex=Lex;
            lex->alter_info.keys_onoff= Alter_info::DISABLE;
            lex->alter_info.flags|= Alter_info::ALTER_KEYS_ONOFF;
          }
        | ENABLE_SYM KEYS
          {
            LEX *lex=Lex;
            lex->alter_info.keys_onoff= Alter_info::ENABLE;
            lex->alter_info.flags|= Alter_info::ALTER_KEYS_ONOFF;
          }
        | ALTER opt_column field_ident SET_SYM DEFAULT_SYM signed_literal
          {
            LEX *lex=Lex;
            CONTEXTUALIZE($3);
            ITEMIZE($6, &$6);
            Alter_column *ac= new Alter_column($3->field_name.str,$6);
            if (ac == NULL)
              MYSQL_YYABORT;
            lex->alter_info.alter_list.push_back(ac);
            lex->alter_info.flags|= Alter_info::ALTER_CHANGE_COLUMN_DEFAULT;
          }
        | ALTER INDEX_SYM ident visibility
          {
            LEX *lex= Lex;
            auto ac= new Alter_index_visibility($3.str, $4);
            if (ac == NULL)
              MYSQL_YYABORT;
            lex->alter_info.alter_index_visibility_list.push_back(ac);
            lex->alter_info.flags|= Alter_info::ALTER_INDEX_VISIBILITY;
          }
        | ALTER opt_column field_ident DROP DEFAULT_SYM
          {
            LEX *lex=Lex;
            CONTEXTUALIZE($3);
            Alter_column *ac= new Alter_column($3->field_name.str, (Item*) 0);
            if (ac == NULL)
              MYSQL_YYABORT;
            lex->alter_info.alter_list.push_back(ac);
            lex->alter_info.flags|= Alter_info::ALTER_CHANGE_COLUMN_DEFAULT;
          }
        | RENAME opt_to table_ident
          {
            LEX *lex=Lex;
            size_t dummy;
            lex->select_lex->db= const_cast<char*>($3->db.str);
            if (lex->select_lex->db == NULL &&
                lex->copy_db_to(&lex->select_lex->db, &dummy))
            {
              MYSQL_YYABORT;
            }
            Ident_name_check ident_check_status=
              check_table_name($3->table.str,$3->table.length);
            if (ident_check_status == Ident_name_check::WRONG)
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $3->table.str);
              MYSQL_YYABORT;
            }
            else if (ident_check_status == Ident_name_check::TOO_LONG)
            {
              my_error(ER_TOO_LONG_IDENT, MYF(0), $3->table.str);
              MYSQL_YYABORT;
            }
            LEX_STRING db_str= to_lex_string($3->db);
            if (db_str.str &&
                (check_and_convert_db_name(&db_str, false) !=
                 Ident_name_check::OK))
              MYSQL_YYABORT;
            lex->name.str= const_cast<char*>($3->table.str);
            lex->name.length= $3->table.length;
            lex->alter_info.flags|= Alter_info::ALTER_RENAME;
          }
        | RENAME key_or_index field_ident TO_SYM field_ident
          {
            LEX *lex=Lex;
            CONTEXTUALIZE($3);
            CONTEXTUALIZE($5);
            Alter_rename_key *ak=
              NEW_PTN Alter_rename_key($3->field_name.str, $5->field_name.str);
            if (ak == NULL)
              MYSQL_YYABORT;
            lex->alter_info.alter_rename_key_list.push_back(ak);
            lex->alter_info.flags|= Alter_info::ALTER_RENAME_INDEX;
          }
        | CONVERT_SYM TO_SYM charset charset_name_or_default opt_collate
          {
            if (!$4)
            {
              THD *thd= YYTHD;
              $4= thd->variables.collation_database;
            }
            $5= $5 ? $5 : $4;
            if (!my_charset_same($4,$5))
            {
              my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
                       $5->name, $4->csname);
              MYSQL_YYABORT;
            }

            LEX *lex= Lex;
            HA_CREATE_INFO *cinfo= lex->create_info;
            if ((cinfo->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
                 cinfo->default_table_charset && $5 &&
                 !my_charset_same(cinfo->default_table_charset,$5))
            {
              my_error(ER_CONFLICTING_DECLARATIONS, MYF(0),
                       "CHARACTER SET ", cinfo->default_table_charset->csname,
                       "CHARACTER SET ", $5->csname);
              MYSQL_YYABORT;
            }

            cinfo->table_charset= cinfo->default_table_charset= $5;
            cinfo->used_fields|= (HA_CREATE_USED_CHARSET |
                                  HA_CREATE_USED_DEFAULT_CHARSET);
            lex->alter_info.flags|= Alter_info::ALTER_OPTIONS;
          }
        | create_table_options_space_separated
          {
            for (auto *option : *$1)
              CONTEXTUALIZE(option);

            LEX *lex=Lex;
            lex->alter_info.flags|= Alter_info::ALTER_OPTIONS;
            if ((lex->create_info->used_fields & HA_CREATE_USED_ENGINE) &&
                !lex->create_info->db_type)
            {
              lex->create_info->used_fields&= ~HA_CREATE_USED_ENGINE;
            }
          }
        | FORCE_SYM
          {
            Lex->alter_info.flags|= Alter_info::ALTER_RECREATE;
          }
        | alter_order_clause
          {
            LEX *lex=Lex;
            lex->alter_info.flags|= Alter_info::ALTER_ORDER;
          }
        ;

alter_commands_modifier:
          alter_algorithm_option { CONTEXTUALIZE($1); }
        | alter_lock_option { CONTEXTUALIZE($1); }
        | alter_opt_validation
        ;

opt_index_lock_and_algorithm:
          /* Empty. */ { $$.init(YYMEM_ROOT); }
        | alter_lock_option
          {
            $$.init(YYMEM_ROOT);
            if ($$.push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | alter_algorithm_option
          {
            $$.init(YYMEM_ROOT);
            if ($$.push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | alter_lock_option alter_algorithm_option
          {
            $$.init(YYMEM_ROOT);
            if ($$.push_back($1) || $$.push_back($2))
              MYSQL_YYABORT; // OOM
          }
        | alter_algorithm_option alter_lock_option
          {
            $$.init(YYMEM_ROOT);
            if ($$.push_back($1) || $$.push_back($2))
              MYSQL_YYABORT; // OOM
          }
        ;

alter_algorithm_option:
          ALGORITHM_SYM opt_equal DEFAULT_SYM
          {
            $$= NEW_PTN PT_requested_algorithm(default_word);
          }
        | ALGORITHM_SYM opt_equal ident
          {
            $$= NEW_PTN PT_requested_algorithm($3);
          }
        ;

alter_lock_option:
          LOCK_SYM opt_equal DEFAULT_SYM
          {
            $$= NEW_PTN PT_requested_lock(default_word);
          }
        | LOCK_SYM opt_equal ident
          {
            $$= NEW_PTN PT_requested_lock($3);
          }
        ;

opt_column:
          /* empty */ {}
        | COLUMN_SYM {}
        ;

opt_ignore:
          /* empty */ { $$= false; }
        | IGNORE_SYM  { $$= true; }
        ;

opt_restrict:
          /* empty */ { $$= DROP_DEFAULT; }
        | RESTRICT    { $$= DROP_RESTRICT; }
        | CASCADE     { $$= DROP_CASCADE; }
        ;

opt_place:
          /* empty */           { $$= NULL; }
        | AFTER_SYM ident       { $$= $2.str; }
        | FIRST_SYM             { $$= first_keyword; }
        ;

opt_to:
          /* empty */ {}
        | TO_SYM {}
        | EQ {}
        | AS {}
        ;

group_replication:
                 START_SYM GROUP_REPLICATION
                 {
                   LEX *lex=Lex;
                   lex->sql_command = SQLCOM_START_GROUP_REPLICATION;
                 }
               | STOP_SYM GROUP_REPLICATION
                 {
                   LEX *lex=Lex;
                   lex->sql_command = SQLCOM_STOP_GROUP_REPLICATION;
                 }
               ;

slave:
        slave_start start_slave_opts{}
      | STOP_SYM SLAVE opt_slave_thread_option_list opt_channel
        {
          LEX *lex=Lex;
          lex->sql_command = SQLCOM_SLAVE_STOP;
          lex->type = 0;
          lex->slave_thd_opt= $3;
        }
      ;

slave_start:
          START_SYM SLAVE opt_slave_thread_option_list
          {
            LEX *lex=Lex;
            /* Clean previous slave connection values */
            lex->slave_connection.reset();
            lex->sql_command = SQLCOM_SLAVE_START;
            lex->type = 0;
            /* We'll use mi structure for UNTIL options */
            lex->mi.set_unspecified();
            lex->slave_thd_opt= $3;
          }
         ;

start_slave_opts:
          slave_until
          slave_connection_opts
          {
            /*
              It is not possible to set user's information when
              one is trying to start the SQL Thread.
            */
            if ((Lex->slave_thd_opt & SLAVE_SQL) == SLAVE_SQL &&
                (Lex->slave_thd_opt & SLAVE_IO) != SLAVE_IO &&
                (Lex->slave_connection.user ||
                 Lex->slave_connection.password ||
                 Lex->slave_connection.plugin_auth ||
                 Lex->slave_connection.plugin_dir))
            {
              my_error(ER_SQLTHREAD_WITH_SECURE_SLAVE, MYF(0));
              MYSQL_YYABORT;
            }
          }
          opt_channel
          ;

start:
          START_SYM TRANSACTION_SYM opt_start_transaction_option_list
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_BEGIN;
            /* READ ONLY and READ WRITE are mutually exclusive. */
            if (($3 & MYSQL_START_TRANS_OPT_READ_WRITE) &&
                ($3 & MYSQL_START_TRANS_OPT_READ_ONLY))
            {
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            lex->start_transaction_opt= $3;
          }
        ;

opt_start_transaction_option_list:
          /* empty */
          {
            $$= 0;
          }
        | start_transaction_option_list
          {
            $$= $1;
          }
        ;

start_transaction_option_list:
          start_transaction_option
          {
            $$= $1;
          }
        | start_transaction_option_list ',' start_transaction_option
          {
            $$= $1 | $3;
          }
        ;

start_transaction_option:
          WITH CONSISTENT_SYM SNAPSHOT_SYM
          {
            $$= MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT;
          }
        | READ_SYM ONLY_SYM
          {
            $$= MYSQL_START_TRANS_OPT_READ_ONLY;
          }
        | READ_SYM WRITE_SYM
          {
            $$= MYSQL_START_TRANS_OPT_READ_WRITE;
          }
        ;

slave_connection_opts:
          slave_user_name_opt slave_user_pass_opt
          slave_plugin_auth_opt slave_plugin_dir_opt
        ;

slave_user_name_opt:
          {
            /* empty */
          }
        | USER EQ TEXT_STRING_sys
          {
            Lex->slave_connection.user= $3.str;
          }
        ;

slave_user_pass_opt:
          {
            /* empty */
          }
        | PASSWORD EQ TEXT_STRING_sys
          {
            Lex->slave_connection.password= $3.str;
            Lex->contains_plaintext_password= true;
          }

slave_plugin_auth_opt:
          {
            /* empty */
          }
        | DEFAULT_AUTH_SYM EQ TEXT_STRING_sys
          {
            Lex->slave_connection.plugin_auth= $3.str;
          }
        ;

slave_plugin_dir_opt:
          {
            /* empty */
          }
        | PLUGIN_DIR_SYM EQ TEXT_STRING_sys
          {
            Lex->slave_connection.plugin_dir= $3.str;
          }
        ;

opt_slave_thread_option_list:
          /* empty */
          {
            $$= 0;
          }
        | slave_thread_option_list
          {
            $$= $1;
          }
        ;

slave_thread_option_list:
          slave_thread_option
          {
            $$= $1;
          }
        | slave_thread_option_list ',' slave_thread_option
          {
            $$= $1 | $3;
          }
        ;

slave_thread_option:
          SQL_THREAD
          {
            $$= SLAVE_SQL;
          }
        | RELAY_THREAD
          {
            $$= SLAVE_IO;
          }
        ;

slave_until:
          /*empty*/
          {
            LEX *lex= Lex;
            lex->mi.slave_until= false;
          }
        | UNTIL_SYM slave_until_opts
          {
            LEX *lex=Lex;
            if (((lex->mi.log_file_name || lex->mi.pos) &&
                lex->mi.gtid) ||
               ((lex->mi.relay_log_name || lex->mi.relay_log_pos) &&
                lex->mi.gtid) ||
                !((lex->mi.log_file_name && lex->mi.pos) ||
                  (lex->mi.relay_log_name && lex->mi.relay_log_pos) ||
                  lex->mi.gtid ||
                  lex->mi.until_after_gaps) ||
                /* SQL_AFTER_MTS_GAPS is meaningless in combination */
                /* with any other coordinates related options       */
                ((lex->mi.log_file_name || lex->mi.pos || lex->mi.relay_log_name
                  || lex->mi.relay_log_pos || lex->mi.gtid)
                 && lex->mi.until_after_gaps))
            {
               my_error(ER_BAD_SLAVE_UNTIL_COND, MYF(0));
               MYSQL_YYABORT;
            }
            lex->mi.slave_until= true;
          }
        ;

slave_until_opts:
          master_file_def
        | slave_until_opts ',' master_file_def
        | SQL_BEFORE_GTIDS EQ TEXT_STRING_sys
          {
            Lex->mi.gtid= $3.str;
            Lex->mi.gtid_until_condition= LEX_MASTER_INFO::UNTIL_SQL_BEFORE_GTIDS;
          }
        | SQL_AFTER_GTIDS EQ TEXT_STRING_sys
          {
            Lex->mi.gtid= $3.str;
            Lex->mi.gtid_until_condition= LEX_MASTER_INFO::UNTIL_SQL_AFTER_GTIDS;
          }
        | SQL_AFTER_MTS_GAPS
          {
            Lex->mi.until_after_gaps= true;
          }
        ;

checksum:
          CHECKSUM_SYM table_or_tables table_list opt_checksum_type
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_CHECKSUM;
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
            if (Select->add_tables(YYTHD, $3, TL_OPTION_UPDATING,
                                   YYPS->m_lock_type, YYPS->m_mdl_type))
              MYSQL_YYABORT;
            Lex->check_opt.flags= $4;
          }
        ;

opt_checksum_type:
          /* empty */   { $$= 0; }
        | QUICK         { $$= T_QUICK; }
        | EXTENDED_SYM  { $$= T_EXTEND; }
        ;

repair:
          REPAIR opt_no_write_to_binlog table_or_tables
          table_list opt_mi_repair_types
          {
            THD *thd= YYTHD;
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_REPAIR;
            lex->no_write_to_binlog= $2;
            lex->check_opt.init();
            lex->check_opt.flags|= $5.flags;
            lex->check_opt.sql_flags|= $5.sql_flags;
            lex->alter_info.reset();
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
            if (Select->add_tables(thd, $4, TL_OPTION_UPDATING,
                                   YYPS->m_lock_type, YYPS->m_mdl_type))
              MYSQL_YYABORT;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_repair_table();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_mi_repair_types:
          /* empty */ { $$.flags = T_MEDIUM; $$.sql_flags= 0; }
        | mi_repair_types
        ;

mi_repair_types:
          mi_repair_type
        | mi_repair_types mi_repair_type
          {
            $$.flags= $1.flags | $2.flags;
            $$.sql_flags= $1.sql_flags | $2.sql_flags;
          }
        ;

mi_repair_type:
          QUICK        { $$.flags= T_QUICK;  $$.sql_flags= 0; }
        | EXTENDED_SYM { $$.flags= T_EXTEND; $$.sql_flags= 0; }
        | USE_FRM      { $$.flags= 0;        $$.sql_flags= TT_USEFRM; }
        ;

analyze:
          ANALYZE_SYM opt_no_write_to_binlog table_or_tables table_list
          {
            THD *thd= YYTHD;
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_ANALYZE;
            lex->no_write_to_binlog= $2;
            lex->check_opt.init();
            lex->alter_info.reset();
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
            if (Select->add_tables(thd, $4, TL_OPTION_UPDATING,
                                   YYPS->m_lock_type, YYPS->m_mdl_type))
              MYSQL_YYABORT;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_analyze_table();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

binlog_base64_event:
          BINLOG_SYM TEXT_STRING_sys
          {
            Lex->sql_command = SQLCOM_BINLOG_BASE64_EVENT;
            Lex->binlog_stmt_arg= $2;
          }
        ;

check:
          CHECK_SYM table_or_tables table_list opt_mi_check_types
          {
            THD *thd= YYTHD;
            LEX *lex=Lex;

            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "CHECK");
              MYSQL_YYABORT;
            }
            lex->sql_command = SQLCOM_CHECK;
            lex->check_opt.init();
            lex->check_opt.flags|= $4.flags;
            lex->check_opt.sql_flags|= $4.sql_flags;
            lex->alter_info.reset();
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
            if (Select->add_tables(thd, $3, TL_OPTION_UPDATING,
                                   YYPS->m_lock_type, YYPS->m_mdl_type))
              MYSQL_YYABORT;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_check_table();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_mi_check_types:
          /* empty */ { $$.flags = T_MEDIUM; $$.sql_flags= 0; }
        | mi_check_types
        ;

mi_check_types:
          mi_check_type
        | mi_check_type mi_check_types
          {
            $$.flags= $1.flags | $2.flags;
            $$.sql_flags= $1.sql_flags | $2.sql_flags;
          }
        ;

mi_check_type:
          QUICK
          { $$.flags= T_QUICK;              $$.sql_flags= 0; }
        | FAST_SYM
          { $$.flags= T_FAST;               $$.sql_flags= 0; }
        | MEDIUM_SYM
          { $$.flags= T_MEDIUM;             $$.sql_flags= 0; }
        | EXTENDED_SYM
          { $$.flags= T_EXTEND;             $$.sql_flags= 0; }
        | CHANGED
          { $$.flags= T_CHECK_ONLY_CHANGED; $$.sql_flags= 0; }
        | FOR_SYM UPGRADE_SYM
          { $$.flags= 0;                    $$.sql_flags= TT_FOR_UPGRADE; }
        ;

optimize:
          OPTIMIZE opt_no_write_to_binlog table_or_tables table_list
          {
            THD *thd= YYTHD;
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_OPTIMIZE;
            lex->no_write_to_binlog= $2;
            lex->check_opt.init();
            lex->alter_info.reset();
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
            if (Select->add_tables(thd, $4, TL_OPTION_UPDATING,
                                   YYPS->m_lock_type, YYPS->m_mdl_type))
              MYSQL_YYABORT;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_optimize_table();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_no_write_to_binlog:
          /* empty */ { $$= 0; }
        | NO_WRITE_TO_BINLOG { $$= 1; }
        | LOCAL_SYM { $$= 1; }
        ;

rename:
          RENAME table_or_tables
          {
            Lex->sql_command= SQLCOM_RENAME_TABLE;
          }
          table_to_table_list
          {}
        | RENAME USER rename_list
          {
            Lex->sql_command = SQLCOM_RENAME_USER;
          }
        ;

rename_list:
          user TO_SYM user
          {
            if (Lex->users_list.push_back($1) || Lex->users_list.push_back($3))
              MYSQL_YYABORT;
          }
        | rename_list ',' user TO_SYM user
          {
            if (Lex->users_list.push_back($3) || Lex->users_list.push_back($5))
              MYSQL_YYABORT;
          }
        ;

table_to_table_list:
          table_to_table
        | table_to_table_list ',' table_to_table
        ;

table_to_table:
          table_ident TO_SYM table_ident
          {
            LEX *lex=Lex;
            SELECT_LEX *sl= Select;
            if (!sl->add_table_to_list(lex->thd, $1,NULL,TL_OPTION_UPDATING,
                                       TL_IGNORE, MDL_EXCLUSIVE) ||
                !sl->add_table_to_list(lex->thd, $3,NULL,TL_OPTION_UPDATING,
                                       TL_IGNORE, MDL_EXCLUSIVE))
              MYSQL_YYABORT;
          }
        ;

keycache:
          CACHE_SYM INDEX_SYM
          {
            Lex->alter_info.reset();
          }
          keycache_list_or_parts IN_SYM key_cache_name
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_ASSIGN_TO_KEYCACHE;
            lex->ident= $6;
          }
        ;

keycache_list_or_parts:
          keycache_list
        | assign_to_keycache_parts
        ;

keycache_list:
          assign_to_keycache
        | keycache_list ',' assign_to_keycache
        ;

assign_to_keycache:
          table_ident cache_keys_spec
          {
            if (!Select->add_table_to_list(YYTHD, $1, NULL, 0, TL_READ,
                                           MDL_SHARED_READ,
                                           $2))
              MYSQL_YYABORT;
          }
        ;

assign_to_keycache_parts:
          table_ident adm_partition cache_keys_spec
          {
            if (!Select->add_table_to_list(YYTHD, $1, NULL, 0, TL_READ,
                                           MDL_SHARED_READ,
                                           $3))
              MYSQL_YYABORT;
          }
        ;

key_cache_name:
          ident    { $$= $1; }
        | DEFAULT_SYM { $$ = default_key_cache_base; }
        ;

preload:
          LOAD INDEX_SYM INTO CACHE_SYM
          {
            LEX *lex=Lex;
            lex->sql_command=SQLCOM_PRELOAD_KEYS;
            lex->alter_info.reset();
          }
          preload_list_or_parts
          {}
        ;

preload_list_or_parts:
          preload_keys_parts
        | preload_list
        ;

preload_list:
          preload_keys
        | preload_list ',' preload_keys
        ;

preload_keys:
          table_ident cache_keys_spec opt_ignore_leaves
          {
            if (!Select->add_table_to_list(YYTHD, $1, NULL, $3, TL_READ,
                                           MDL_SHARED_READ,
                                           $2))
              MYSQL_YYABORT;
          }
        ;

preload_keys_parts:
          table_ident adm_partition cache_keys_spec opt_ignore_leaves
          {
            if (!Select->add_table_to_list(YYTHD, $1, NULL, $4, TL_READ,
                                           MDL_SHARED_READ,
                                           $3))
              MYSQL_YYABORT;
          }
        ;

adm_partition:
          PARTITION_SYM
          {
            Lex->alter_info.flags|= Alter_info::ALTER_ADMIN_PARTITION;
          }
          '(' all_or_alt_part_name_list ')'
        ;

cache_keys_spec:
          cache_key_list_or_empty
        ;

cache_key_list_or_empty:
          /* empty */ { $$= NULL; }
        | key_or_index '(' opt_key_usage_list ')'
          {
            init_index_hints($3, INDEX_HINT_USE,
                             old_mode ? INDEX_HINT_MASK_JOIN
                                      : INDEX_HINT_MASK_ALL);
            $$= $3;
          }
        ;

opt_ignore_leaves:
          /* empty */
          { $$= 0; }
        | IGNORE_SYM LEAVES { $$= TL_OPTION_IGNORE_LEAVES; }
        ;

select_stmt:
          query_expression
          {
            $$= NEW_PTN PT_select_stmt($1);
          }
        | query_expression_parens
          {
            if ($1 == NULL)
              MYSQL_YYABORT; // OOM
            $$= NEW_PTN PT_select_stmt($1);
          }
        | select_stmt_with_into
        ;

/*
  MySQL has a syntax extension that allows into clauses in any one of two
  places. They may appear either before the from clause or at the end. All in
  a top-level select statement. This extends the standard syntax in two
  ways. First, we don't have the restriction that the result can contain only
  one row: the into clause might be INTO OUTFILE/DUMPFILE in which case any
  number of rows is allowed. Hence MySQL does not have any special case for
  the standard's <select statement: single row>. Secondly, and this has more
  severe implications for the parser, it makes the grammar ambiguous, because
  in a from-clause-less select statement with an into clause, it is not clear
  whether the into clause is the leading or the trailing one.

  While it's possible to write an unambiguous grammar, it would force us to
  duplicate the entire <select statement> syntax all the way down to the <into
  clause>. So instead we solve it by writing an ambiguous grammar and use
  precedence rules to sort out the shift/reduce conflict.

  The problem is when the parser has seen SELECT <select list>, and sees an
  INTO token. It can now either shift it or reduce what it has to a table-less
  query expression. If it shifts the token, it will accept seeing a FROM token
  next and hence the INTO will be interpreted as the leading INTO. If it
  reduces what it has seen to a table-less select, however, it will interpret
  INTO as the trailing into. But what if the next token is FROM? Obviously,
  we want to always shift INTO. We do this by two precedence declarations: We
  make the INTO token right-associative, and we give it higher precedence than
  an empty from clause, using the artificial token EMPTY_FROM_CLAUSE.

  The remaining problem is that now we allow the leading INTO anywhere, when
  it should be allowed on the top level only. We solve this by manually
  throwing parse errors whenever we reduce a nested query expression if it
  contains an into clause.
*/
select_stmt_with_into:
          '(' select_stmt_with_into ')'
          {
            $$= $2;
          }
        | query_expression into_clause
          {
            if ($1 == NULL)
              MYSQL_YYABORT; // OOM

            if ($1->has_into_clause())
              YYTHD->syntax_error_at(@2, ER_THD(YYTHD, ER_SYNTAX_ERROR));

            $$= NEW_PTN PT_select_stmt($1, $2);
          }
        ;

/**
  A <query_expression> within parentheses can be used as an <expr>. Now,
  because both a <query_expression> and an <expr> can appear syntactically
  within any number of parentheses, we get an ambiguous grammar: Where do the
  parentheses belong? Techically, we have to tell Bison by which rule to
  reduce the extra pair of parentheses. We solve it in a somewhat tedious way
  by defining a query_expression so that it can't have enclosing
  parentheses. This forces us to be very explicit about exactly where we allow
  parentheses; while the standard defines only one rule for <query expression>
  parentheses, we have to do it in several places. But this is a blessing in
  disguise, as we are able to define our syntax in a more fine-grained manner,
  and this is necessary in order to support some MySQL extensions, for example
  as in the last two sub-rules here.

  Even if we define a query_expression not to have outer parentheses, we still
  get a shift/reduce conflict for the <subquery> rule, but we solve this by
  using an artifical token SUBQUERY_AS_EXPR that has less priority than
  parentheses. This ensures that the parser consumes as many parentheses as it
  can, and only when that fails will it try to reduce, and by then it will be
  clear from the lookahead token whether we have a subquery or just a
  query_expression within parentheses. For example, if the lookahead token is
  UNION it's just a query_expression within parentheses and the parentheses
  don't mean it's a subquery. If the next token is PLUS, we know it must be an
  <expr> and the parentheses really mean it's a subquery.

  A word about CTE's: The rules below are duplicated, one with a with_clause
  and one without, instead of using a single rule with an opt_with_clause. The
  reason we do this is because it would make Bison try to cram both rules into
  a single state, where it would have to decide whether to reduce a with_clause
  before seeing the rest of the input. This way we force Bison to parse the
  entire query expression before trying to reduce.
*/
query_expression:
          query_expression_body
          opt_order_clause
          opt_limit_clause
          opt_locking_clause_list
          {
            $$= NEW_PTN PT_query_expression($1, $2, $3, $4);
          }
        | with_clause
          query_expression_body
          opt_order_clause
          opt_limit_clause
          opt_locking_clause_list
          {
            $$= NEW_PTN PT_query_expression($1, $2, $3, $4, $5);
          }
        | query_expression_parens
          order_clause
          opt_limit_clause
          opt_locking_clause_list
          {
            auto nested= NEW_PTN PT_nested_query_expression($1);
            auto body= NEW_PTN PT_query_expression_body_primary(nested);
            $$= NEW_PTN PT_query_expression(body, $2, $3, $4);
          }
        | with_clause
          query_expression_parens
          order_clause
          opt_limit_clause
          opt_locking_clause_list
          {
            auto nested= NEW_PTN PT_nested_query_expression($2);
            auto body= NEW_PTN PT_query_expression_body_primary(nested);
            $$= NEW_PTN PT_query_expression($1, body, $3, $4, $5);
          }
        | query_expression_parens
          limit_clause
          opt_locking_clause_list
          {
            if ($1 == NULL)
              MYSQL_YYABORT; // OOM
            $$= NEW_PTN PT_query_expression($1->body(), NULL, $2, $3);
          }
        | with_clause
          query_expression_parens
          limit_clause
          opt_locking_clause_list
          {
            if ($2 == NULL)
              MYSQL_YYABORT; // OOM
            $$= NEW_PTN PT_query_expression($1, $2->body(), NULL, $3, $4);
          }
        | with_clause
          query_expression_parens
          opt_locking_clause_list
          {
            if ($2 == NULL)
              MYSQL_YYABORT; // OOM
            $$= NEW_PTN PT_query_expression($1, $2->body(), NULL, NULL, $3);
          }
        ;

query_expression_body:
          query_primary
          {
            $$= NEW_PTN PT_query_expression_body_primary($1);
          }
        | query_expression_body UNION_SYM union_option query_primary
          {
            $$= NEW_PTN PT_union(NEW_PTN PT_query_expression($1), @1, $3, $4);
          }
        | query_expression_parens UNION_SYM union_option query_primary
          {
            if ($1 == NULL)
              MYSQL_YYABORT; // OOM

            $1->set_parentheses();

            $$= NEW_PTN PT_union($1, @1, $3, $4);
          }
        | query_expression_body UNION_SYM union_option query_expression_parens
          {
            if ($4 == NULL)
              MYSQL_YYABORT; // OOM

            if ($4->is_union())
              YYTHD->syntax_error_at(@4, ER_THD(YYTHD, ER_SYNTAX_ERROR));

            auto lhs_qe= NEW_PTN PT_query_expression($1);
            PT_nested_query_expression *nested_qe=
              NEW_PTN PT_nested_query_expression($4);

            $$= NEW_PTN PT_union(lhs_qe, @1, $3, nested_qe);
          }
        | query_expression_parens UNION_SYM union_option query_expression_parens
          {
            if ($1 == NULL || $4 == NULL)
              MYSQL_YYABORT; // OOM

            if ($4->is_union())
              YYTHD->syntax_error_at(@4, ER_THD(YYTHD, ER_SYNTAX_ERROR));

            $1->set_parentheses();

            PT_nested_query_expression *nested_qe=
              NEW_PTN PT_nested_query_expression($4);
            $$= NEW_PTN PT_union($1, @1, $3, nested_qe);
          }
        ;


query_expression_parens:
          '(' query_expression_parens ')' { $$= $2; }
        | '(' query_expression ')'
          {
            /*
              We don't call set_parentheses() on a query expression here. It
              makes no difference to the contextualization phase whether a
              query expression was within parentheses unless it is used in
              conjunction with UNION. Therefore set_parentheses() is called
              only in the rules producing UNION syntax.

              The need for set_parentheses() is purely to support legacy parse
              rules, and we are gradually moving away from them and using the
              query_expression_body to define UNION syntax. When this move is
              complete, we will not need set_parentheses() any more, and the
              contextualize() phase can be greatly simplified.
            */
            $$= $2;
          }
        ;

query_primary:
          query_specification
          {
            // Bison doesn't get polymorphism.
            $$= $1;
          }
        ;

query_specification:
          SELECT_SYM
          select_options
          select_item_list
          into_clause
          opt_from_clause
          opt_where_clause
          opt_group_clause
          opt_having_clause
          {
            $$= NEW_PTN PT_query_specification(
                                      $1,  // SELECT_SYM
                                      $2,  // select_options
                                      $3,  // select_item_list
                                      $4,  // into_clause
                                      $5,  // from
                                      $6,  // where
                                      $7,  // group
                                      $8); // having
          }
        | SELECT_SYM
          select_options
          select_item_list
          opt_from_clause
          opt_where_clause
          opt_group_clause
          opt_having_clause
          {
            $$= NEW_PTN PT_query_specification(
                                      $1,  // SELECT_SYM
                                      $2,  // select_options
                                      $3,  // select_item_list
                                      NULL,// no INTO clause
                                      $4,  // from
                                      $5,  // where
                                      $6,  // group
                                      $7); // having
          }
        ;

opt_from_clause:
          /* Empty. */ %prec EMPTY_FROM_CLAUSE { $$.init(YYMEM_ROOT); }
        | from_clause
        ;

from_clause:
          FROM from_tables { $$= $2; }
        ;

from_tables:
          DUAL_SYM { $$.init(YYMEM_ROOT); }
        | table_reference_list
        ;

table_reference_list:
          table_reference
          {
            $$.init(YYMEM_ROOT);
            if ($$.push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | table_reference_list ',' table_reference
          {
            $$= $1;
            if ($$.push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

select_options:
          /* empty*/
          {
            $$.query_spec_options= 0;
            $$.sql_cache= SELECT_LEX::SQL_CACHE_UNSPECIFIED;
          }
        | select_option_list
        ;

select_option_list:
          select_option_list select_option
          {
            if ($$.merge($1, $2))
              MYSQL_YYABORT;
          }
        | select_option
        ;

select_option:
          query_spec_option
          {
            $$.query_spec_options= $1;
            $$.sql_cache= SELECT_LEX::SQL_CACHE_UNSPECIFIED;
          }
        | SQL_NO_CACHE_SYM
          {
            /*
              Allow this flag only on the first top-level SELECT statement, if
              SQL_CACHE wasn't specified, and only once per query.
             */
            $$.query_spec_options= 0;
            $$.sql_cache= SELECT_LEX::SQL_NO_CACHE;
          }
        | SQL_CACHE_SYM
          {
            /*
              Allow this flag only on the first top-level SELECT statement, if
              SQL_NO_CACHE wasn't specified, and only once per query.
             */
            $$.query_spec_options= 0;
            $$.sql_cache= SELECT_LEX::SQL_CACHE;
          }
        ;

opt_locking_clause_list:
          /* Empty. */ { $$= NULL; }
        | locking_clause_list
        ;

locking_clause_list:
          locking_clause_list locking_clause
          {
            $$= $1;
            if ($$->push_back($2))
              MYSQL_YYABORT; // OOM
          }
        | locking_clause
          {
            $$= NEW_PTN PT_locking_clause_list(YYTHD->mem_root);
            if ($$ == nullptr || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        ;

locking_clause:
          FOR_SYM lock_strength opt_locked_row_action
          {
            $$= NEW_PTN PT_query_block_locking_clause($2, $3);
          }
        | FOR_SYM lock_strength table_locking_list opt_locked_row_action
          {
            $$= NEW_PTN PT_table_locking_clause($2, $3, $4);
          }
        | LOCK_SYM IN_SYM SHARE_SYM MODE_SYM
          {
            $$= NEW_PTN PT_query_block_locking_clause(Lock_strength::SHARE);
          }
        ;

lock_strength:
          UPDATE_SYM { $$= Lock_strength::UPDATE; }
        | SHARE_SYM  { $$= Lock_strength::SHARE; }
        ;

table_locking_list:
          OF_SYM table_alias_ref_list { $$= $2; }
        ;

opt_locked_row_action:
          /* Empty */ { $$= Locked_row_action::WAIT; }
        | locked_row_action
        ;

locked_row_action:
          SKIP_SYM LOCKED_SYM { $$= Locked_row_action::SKIP; }
        | NOWAIT_SYM { $$= Locked_row_action::NOWAIT; }
        ;

select_item_list:
          select_item_list ',' select_item
          {
            if ($1 == NULL || $1->push_back($3))
              MYSQL_YYABORT;
            $$= $1;
          }
        | select_item
          {
            $$= NEW_PTN PT_select_item_list;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        | '*'
          {
            Item *item= NEW_PTN Item_field(@$, NULL, NULL, "*");
            $$= NEW_PTN PT_select_item_list;
            if ($$ == NULL || $$->push_back(item))
              MYSQL_YYABORT;
          }
        ;

select_item:
          table_wild { $$= $1; }
        | expr select_alias
          {
            $$= NEW_PTN PTI_expr_with_alias(@$, $1, @1.cpp, $2);
          }
        ;


select_alias:
          /* empty */ { $$=null_lex_str;}
        | AS ident { $$=$2; }
        | AS TEXT_STRING_sys { $$=$2; }
        | ident { $$=$1; }
        | TEXT_STRING_sys { $$=$1; }
        ;

optional_braces:
          /* empty */ {}
        | '(' ')' {}
        ;

/* all possible expressions */
expr:
          expr or expr %prec OR_SYM
          {
            $$= flatten_associative_operator<Item_cond_or,
                                             Item_func::COND_OR_FUNC>(
                                                 YYMEM_ROOT, @$, $1, $3);
          }
        | expr XOR expr %prec XOR
          {
            /* XOR is a proprietary extension */
            $$ = NEW_PTN Item_func_xor(@$, $1, $3);
          }
        | expr and expr %prec AND_SYM
          {
            $$= flatten_associative_operator<Item_cond_and,
                                             Item_func::COND_AND_FUNC>(
                                                 YYMEM_ROOT, @$, $1, $3);
          }
        | NOT_SYM expr %prec NOT_SYM
          {
            $$= NEW_PTN PTI_negate_expression(@$, $2);
          }
        | bool_pri IS TRUE_SYM %prec IS
          {
            $$= NEW_PTN Item_func_istrue(@$, $1);
          }
        | bool_pri IS not TRUE_SYM %prec IS
          {
            $$= NEW_PTN Item_func_isnottrue(@$, $1);
          }
        | bool_pri IS FALSE_SYM %prec IS
          {
            $$= NEW_PTN Item_func_isfalse(@$, $1);
          }
        | bool_pri IS not FALSE_SYM %prec IS
          {
            $$= NEW_PTN Item_func_isnotfalse(@$, $1);
          }
        | bool_pri IS UNKNOWN_SYM %prec IS
          {
            $$= NEW_PTN Item_func_isnull(@$, $1);
          }
        | bool_pri IS not UNKNOWN_SYM %prec IS
          {
            $$= NEW_PTN Item_func_isnotnull(@$, $1);
          }
        | bool_pri
        ;

bool_pri:
          bool_pri IS NULL_SYM %prec IS
          {
            $$= NEW_PTN Item_func_isnull(@$, $1);
          }
        | bool_pri IS not NULL_SYM %prec IS
          {
            $$= NEW_PTN Item_func_isnotnull(@$, $1);
          }
        | bool_pri comp_op predicate
          {
            $$= NEW_PTN PTI_comp_op(@$, $1, $2, $3);
          }
        | bool_pri comp_op all_or_any table_subquery %prec EQ
          {
            if ($2 == &comp_equal_creator)
              /*
                We throw this manual parse error rather than split the rule
                comp_op into a null-safe and a non null-safe rule, since doing
                so would add a shift/reduce conflict. It's actually this rule
                and the ones referencing it that cause all the conflicts, but
                we still don't want the count to go up.
              */
              YYTHD->syntax_error_at(@2, ER_THD(YYTHD, ER_SYNTAX_ERROR));
            $$= NEW_PTN PTI_comp_op_all(@$, $1, $2, $3, $4);
          }
        | predicate
        ;

predicate:
          bit_expr IN_SYM table_subquery
          {
            $$= NEW_PTN Item_in_subselect(@$, $1, $3);
          }
        | bit_expr not IN_SYM table_subquery
          {
            Item *item= NEW_PTN Item_in_subselect(@$, $1, $4);
            $$= NEW_PTN PTI_negate_expression(@$, item);
          }
        | bit_expr IN_SYM '(' expr ')'
          {
            $$= NEW_PTN PTI_handle_sql2003_note184_exception(@$, $1, true, $4);
          }
        | bit_expr IN_SYM '(' expr ',' expr_list ')'
          {
            if ($6 == NULL || $6->push_front($4) || $6->push_front($1))
              MYSQL_YYABORT;

            $$= NEW_PTN Item_func_in(@$, $6, false);
          }
        | bit_expr not IN_SYM '(' expr ')'
          {
            $$= NEW_PTN PTI_handle_sql2003_note184_exception(@$, $1, false, $5);
          }
        | bit_expr not IN_SYM '(' expr ',' expr_list ')'
          {
            if ($7 == NULL || $7->push_front($5) || $7->value.push_front($1))
              MYSQL_YYABORT;

            $$= NEW_PTN Item_func_in(@$, $7, true);
          }
        | bit_expr BETWEEN_SYM bit_expr AND_SYM predicate
          {
            $$= NEW_PTN Item_func_between(@$, $1, $3, $5, false);
          }
        | bit_expr not BETWEEN_SYM bit_expr AND_SYM predicate
          {
            $$= NEW_PTN Item_func_between(@$, $1, $4, $6, true);
          }
        | bit_expr SOUNDS_SYM LIKE bit_expr
          {
            Item *item1= NEW_PTN Item_func_soundex(@$, $1);
            Item *item4= NEW_PTN Item_func_soundex(@$, $4);
            if ((item1 == NULL) || (item4 == NULL))
              MYSQL_YYABORT;
            $$= NEW_PTN Item_func_eq(@$, item1, item4);
          }
        | bit_expr LIKE simple_expr opt_escape
          {
            $$= NEW_PTN Item_func_like(@$, $1, $3, $4);
          }
        | bit_expr not LIKE simple_expr opt_escape
          {
            Item *item= NEW_PTN Item_func_like(@$, $1, $4, $5);
            if (item == NULL)
              MYSQL_YYABORT;
            $$= NEW_PTN Item_func_not(@$, item);
          }
        | bit_expr REGEXP bit_expr
          {
            $$= NEW_PTN Item_func_regex(@$, $1, $3);
          }
        | bit_expr not REGEXP bit_expr
          {
            Item *item= NEW_PTN Item_func_regex(@$, $1, $4);
            $$= NEW_PTN PTI_negate_expression(@$, item);
          }
        | bit_expr
        ;

bit_expr:
          bit_expr '|' bit_expr %prec '|'
          {
            $$= NEW_PTN Item_func_bit_or(@$, $1, $3);
          }
        | bit_expr '&' bit_expr %prec '&'
          {
            $$= NEW_PTN Item_func_bit_and(@$, $1, $3);
          }
        | bit_expr SHIFT_LEFT bit_expr %prec SHIFT_LEFT
          {
            $$= NEW_PTN Item_func_shift_left(@$, $1, $3);
          }
        | bit_expr SHIFT_RIGHT bit_expr %prec SHIFT_RIGHT
          {
            $$= NEW_PTN Item_func_shift_right(@$, $1, $3);
          }
        | bit_expr '+' bit_expr %prec '+'
          {
            $$= NEW_PTN Item_func_plus(@$, $1, $3);
          }
        | bit_expr '-' bit_expr %prec '-'
          {
            $$= NEW_PTN Item_func_minus(@$, $1, $3);
          }
        | bit_expr '+' INTERVAL_SYM expr interval %prec '+'
          {
            $$= NEW_PTN Item_date_add_interval(@$, $1, $4, $5, 0);
          }
        | bit_expr '-' INTERVAL_SYM expr interval %prec '-'
          {
            $$= NEW_PTN Item_date_add_interval(@$, $1, $4, $5, 1);
          }
        | bit_expr '*' bit_expr %prec '*'
          {
            $$= NEW_PTN Item_func_mul(@$, $1, $3);
          }
        | bit_expr '/' bit_expr %prec '/'
          {
            $$= NEW_PTN Item_func_div(@$, $1,$3);
          }
        | bit_expr '%' bit_expr %prec '%'
          {
            $$= NEW_PTN Item_func_mod(@$, $1,$3);
          }
        | bit_expr DIV_SYM bit_expr %prec DIV_SYM
          {
            $$= NEW_PTN Item_func_int_div(@$, $1,$3);
          }
        | bit_expr MOD_SYM bit_expr %prec MOD_SYM
          {
            $$= NEW_PTN Item_func_mod(@$, $1, $3);
          }
        | bit_expr '^' bit_expr
          {
            $$= NEW_PTN Item_func_bit_xor(@$, $1, $3);
          }
        | simple_expr
        ;

or:
          OR_SYM
       | OR2_SYM
       ;

and:
          AND_SYM
       | AND_AND_SYM
       ;

not:
          NOT_SYM
        | NOT2_SYM
        ;

not2:
          '!'
        | NOT2_SYM
        ;

comp_op:
          EQ     { $$ = &comp_eq_creator; }
        | EQUAL_SYM { $$ = &comp_equal_creator; }
        | GE     { $$ = &comp_ge_creator; }
        | GT_SYM { $$ = &comp_gt_creator; }
        | LE     { $$ = &comp_le_creator; }
        | LT     { $$ = &comp_lt_creator; }
        | NE     { $$ = &comp_ne_creator; }
        ;

all_or_any:
          ALL     { $$ = 1; }
        | ANY_SYM { $$ = 0; }
        ;

simple_expr:
          simple_ident
        | function_call_keyword
        | function_call_nonkeyword
        | function_call_generic
        | function_call_conflict
        | simple_expr COLLATE_SYM ident_or_text %prec NEG
          {
            $$= NEW_PTN Item_func_set_collation(@$, $1, $3);
          }
        | literal
        | param_marker { $$= $1; }
        | variable
        | set_function_specification
        | simple_expr OR_OR_SYM simple_expr
          {
            $$= NEW_PTN Item_func_concat(@$, $1, $3);
          }
        | '+' simple_expr %prec NEG
          {
            $$= $2; // TODO: do we really want to ignore unary '+' before any kind of literals?
          }
        | '-' simple_expr %prec NEG
          {
            $$= NEW_PTN Item_func_neg(@$, $2);
          }
        | '~' simple_expr %prec NEG
          {
            $$= NEW_PTN Item_func_bit_neg(@$, $2);
          }
        | not2 simple_expr %prec NEG
          {
            $$= NEW_PTN PTI_negate_expression(@$, $2);
          }
        | row_subquery
          {
            $$= NEW_PTN PTI_singlerow_subselect(@$, $1);
          }
        | '(' expr ')' { $$= $2; }
        | '(' expr ',' expr_list ')'
          {
            $$= NEW_PTN Item_row(@$, $2, $4->value);
          }
        | ROW_SYM '(' expr ',' expr_list ')'
          {
            $$= NEW_PTN Item_row(@$, $3, $5->value);
          }
        | EXISTS table_subquery
          {
            $$= NEW_PTN PTI_exists_subselect(@$, $2);
          }
        | '{' ident expr '}'
          {
            $$= NEW_PTN PTI_odbc_date(@$, $2, $3);
          }
        | MATCH ident_list_arg AGAINST '(' bit_expr fulltext_options ')'
          {
            $$= NEW_PTN Item_func_match(@$, $2, $5, $6);
          }
        | BINARY_SYM simple_expr %prec NEG
          {
            $$= create_func_cast(YYTHD, @2, $2, ITEM_CAST_CHAR, &my_charset_bin);
          }
        | CAST_SYM '(' expr AS cast_type ')'
          {
            $$= create_func_cast(YYTHD, @3, $3, &$5);
          }
        | CASE_SYM opt_expr when_list opt_else END
          {
            $$= NEW_PTN Item_func_case(@$, *$3, $2, $4 );
          }
        | CONVERT_SYM '(' expr ',' cast_type ')'
          {
            $$= create_func_cast(YYTHD, @3, $3, &$5);
          }
        | CONVERT_SYM '(' expr USING charset_name ')'
          {
            $$= NEW_PTN Item_func_conv_charset(@$, $3,$5);
          }
        | DEFAULT_SYM '(' simple_ident ')'
          {
            $$= NEW_PTN Item_default_value(@$, $3);
          }
        | VALUES '(' simple_ident_nospvar ')'
          {
            $$= NEW_PTN Item_insert_value(@$, $3);
          }
        | INTERVAL_SYM expr interval '+' expr %prec INTERVAL_SYM
          /* we cannot put interval before - */
          {
            $$= NEW_PTN Item_date_add_interval(@$, $5, $2, $3, 0);
          }
        | simple_ident JSON_SEPARATOR_SYM TEXT_STRING_literal
          {
            Item_string *path=
              NEW_PTN Item_string(@$, $3.str, $3.length,
                                  YYTHD->variables.collation_connection);
            $$= NEW_PTN Item_func_json_extract(YYTHD, @$, $1, path);
          }
         | simple_ident JSON_UNQUOTED_SEPARATOR_SYM TEXT_STRING_literal
          {
            Item_string *path=
              NEW_PTN Item_string(@$, $3.str, $3.length,
                                  YYTHD->variables.collation_connection);
            Item *extr= NEW_PTN Item_func_json_extract(YYTHD, @$, $1, path);
            $$= NEW_PTN Item_func_json_unquote(@$, extr);
          }
        ;

/*
  Function call syntax using official SQL 2003 keywords.
  Because the function name is an official token,
  a dedicated grammar rule is needed in the parser.
  There is no potential for conflicts
*/
function_call_keyword:
          CHAR_SYM '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_char(@$, $3);
          }
        | CHAR_SYM '(' expr_list USING charset_name ')'
          {
            $$= NEW_PTN Item_func_char(@$, $3, $5);
          }
        | CURRENT_USER optional_braces
          {
            $$= NEW_PTN Item_func_current_user(@$);
          }
        | DATE_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_date_typecast(@$, $3);
          }
        | DAY_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_dayofmonth(@$, $3);
          }
        | HOUR_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_hour(@$, $3);
          }
        | INSERT_SYM '(' expr ',' expr ',' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_insert(@$, $3, $5, $7, $9);
          }
        | INTERVAL_SYM '(' expr ',' expr ')' %prec INTERVAL_SYM
          {
            $$= NEW_PTN Item_func_interval(@$, YYMEM_ROOT, $3, $5);
          }
        | INTERVAL_SYM '(' expr ',' expr ',' expr_list ')' %prec INTERVAL_SYM
          {
            $$= NEW_PTN Item_func_interval(@$, YYMEM_ROOT, $3, $5, $7);
          }
        | LEFT '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_left(@$, $3, $5);
          }
        | MINUTE_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_minute(@$, $3);
          }
        | MONTH_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_month(@$, $3);
          }
        | RIGHT '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_right(@$, $3, $5);
          }
        | SECOND_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_second(@$, $3);
          }
        | TIME_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_time_typecast(@$, $3);
          }
        | TIMESTAMP_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_datetime_typecast(@$, $3);
          }
        | TIMESTAMP_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_add_time(@$, $3, $5, 1, 0);
          }
        | TRIM '(' expr ')'
          {
            $$= NEW_PTN Item_func_trim(@$, $3,
                                       Item_func_trim::TRIM_BOTH_DEFAULT);
          }
        | TRIM '(' LEADING expr FROM expr ')'
          {
            $$= NEW_PTN Item_func_trim(@$, $6, $4,
                                       Item_func_trim::TRIM_LEADING);
          }
        | TRIM '(' TRAILING expr FROM expr ')'
          {
            $$= NEW_PTN Item_func_trim(@$, $6, $4,
                                       Item_func_trim::TRIM_TRAILING);
          }
        | TRIM '(' BOTH expr FROM expr ')'
          {
            $$= NEW_PTN Item_func_trim(@$, $6, $4, Item_func_trim::TRIM_BOTH);
          }
        | TRIM '(' LEADING FROM expr ')'
          {
            $$= NEW_PTN Item_func_trim(@$, $5, Item_func_trim::TRIM_LEADING);
          }
        | TRIM '(' TRAILING FROM expr ')'
          {
            $$= NEW_PTN Item_func_trim(@$, $5, Item_func_trim::TRIM_TRAILING);
          }
        | TRIM '(' BOTH FROM expr ')'
          {
            $$= NEW_PTN Item_func_trim(@$, $5, Item_func_trim::TRIM_BOTH);
          }
        | TRIM '(' expr FROM expr ')'
          {
            $$= NEW_PTN Item_func_trim(@$, $5, $3,
                                       Item_func_trim::TRIM_BOTH_DEFAULT);
          }
        | USER '(' ')'
          {
            $$= NEW_PTN Item_func_user(@$);
          }
        | YEAR_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_year(@$, $3);
          }
        ;

/*
  Function calls using non reserved keywords, with special syntaxic forms.
  Dedicated grammar rules are needed because of the syntax,
  but also have the potential to cause incompatibilities with other
  parts of the language.
  MAINTAINER:
  The only reasons a function should be added here are:
  - for compatibility reasons with another SQL syntax (CURDATE),
  - for typing reasons (GET_FORMAT)
  Any other 'Syntaxic sugar' enhancements should be *STRONGLY*
  discouraged.
*/
function_call_nonkeyword:
          ADDDATE_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_date_add_interval(@$, $3, $5, INTERVAL_DAY, 0);
          }
        | ADDDATE_SYM '(' expr ',' INTERVAL_SYM expr interval ')'
          {
            $$= NEW_PTN Item_date_add_interval(@$, $3, $6, $7, 0);
          }
        | CURDATE optional_braces
          {
            $$= NEW_PTN Item_func_curdate_local(@$);
          }
        | CURTIME func_datetime_precision
          {
            $$= NEW_PTN Item_func_curtime_local(@$, static_cast<uint8>($2));
          }
        | DATE_ADD_INTERVAL '(' expr ',' INTERVAL_SYM expr interval ')'
          %prec INTERVAL_SYM
          {
            $$= NEW_PTN Item_date_add_interval(@$, $3, $6, $7, 0);
          }
        | DATE_SUB_INTERVAL '(' expr ',' INTERVAL_SYM expr interval ')'
          %prec INTERVAL_SYM
          {
            $$= NEW_PTN Item_date_add_interval(@$, $3, $6, $7, 1);
          }
        | EXTRACT_SYM '(' interval FROM expr ')'
          {
            $$= NEW_PTN Item_extract(@$,  $3, $5);
          }
        | GET_FORMAT '(' date_time_type  ',' expr ')'
          {
            $$= NEW_PTN Item_func_get_format(@$, $3, $5);
          }
        | now
          {
            $$= NEW_PTN PTI_function_call_nonkeyword_now(@$,
              static_cast<uint8>($1));
          }
        | POSITION_SYM '(' bit_expr IN_SYM expr ')'
          {
            $$= NEW_PTN Item_func_locate(@$, $5,$3);
          }
        | SUBDATE_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_date_add_interval(@$, $3, $5, INTERVAL_DAY, 1);
          }
        | SUBDATE_SYM '(' expr ',' INTERVAL_SYM expr interval ')'
          {
            $$= NEW_PTN Item_date_add_interval(@$, $3, $6, $7, 1);
          }
        | SUBSTRING '(' expr ',' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_substr(@$, $3,$5,$7);
          }
        | SUBSTRING '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_substr(@$, $3,$5);
          }
        | SUBSTRING '(' expr FROM expr FOR_SYM expr ')'
          {
            $$= NEW_PTN Item_func_substr(@$, $3,$5,$7);
          }
        | SUBSTRING '(' expr FROM expr ')'
          {
            $$= NEW_PTN Item_func_substr(@$, $3,$5);
          }
        | SYSDATE func_datetime_precision
          {
            $$= NEW_PTN PTI_function_call_nonkeyword_sysdate(@$,
              static_cast<uint8>($2));
          }
        | TIMESTAMP_ADD '(' interval_time_stamp ',' expr ',' expr ')'
          {
            $$= NEW_PTN Item_date_add_interval(@$, $7, $5, $3, 0);
          }
        | TIMESTAMP_DIFF '(' interval_time_stamp ',' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_timestamp_diff(@$, $5,$7,$3);
          }
        | UTC_DATE_SYM optional_braces
          {
            $$= NEW_PTN Item_func_curdate_utc(@$);
          }
        | UTC_TIME_SYM func_datetime_precision
          {
            $$= NEW_PTN Item_func_curtime_utc(@$, static_cast<uint8>($2));
          }
        | UTC_TIMESTAMP_SYM func_datetime_precision
          {
            $$= NEW_PTN Item_func_now_utc(@$, static_cast<uint8>($2));
          }
        ;

/*
  Functions calls using a non reserved keyword, and using a regular syntax.
  Because the non reserved keyword is used in another part of the grammar,
  a dedicated rule is needed here.
*/
function_call_conflict:
          ASCII_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_ascii(@$, $3);
          }
        | CHARSET '(' expr ')'
          {
            $$= NEW_PTN Item_func_charset(@$, $3);
          }
        | COALESCE '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_coalesce(@$, $3);
          }
        | COLLATION_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_collation(@$, $3);
          }
        | DATABASE '(' ')'
          {
            $$= NEW_PTN Item_func_database(@$);
          }
        | IF '(' expr ',' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_if(@$, $3,$5,$7);
          }
        | FORMAT_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_format(@$, $3, $5);
          }
        | FORMAT_SYM '(' expr ',' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_format(@$, $3, $5, $7);
          }
        | MICROSECOND_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_microsecond(@$, $3);
          }
        | MOD_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_mod(@$, $3, $5);
          }
        | PASSWORD '(' expr ')'
          {
            $$= NEW_PTN PTI_password(@$, $3);
          }
        | QUARTER_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_quarter(@$, $3);
          }
        | REPEAT_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_repeat(@$, $3,$5);
          }
        | REPLACE_SYM '(' expr ',' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_replace(@$, $3,$5,$7);
          }
        | REVERSE_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_reverse(@$, $3);
          }
        | ROW_COUNT_SYM '(' ')'
          {
            $$= NEW_PTN Item_func_row_count(@$);
          }
        | TRUNCATE_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_round(@$, $3,$5,1);
          }
        | WEEK_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_week(@$, $3, NULL);
          }
        | WEEK_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_week(@$, $3, $5);
          }
        | WEIGHT_STRING_SYM '(' expr ')'
          {
            $$= NEW_PTN Item_func_weight_string(@$, $3, 0, 0, 0);
          }
        | WEIGHT_STRING_SYM '(' expr AS CHAR_SYM ws_num_codepoints ')'
          {
            $$= NEW_PTN Item_func_weight_string(@$, $3, 0, $6, 0);
          }
        | WEIGHT_STRING_SYM '(' expr AS BINARY_SYM ws_num_codepoints ')'
          {
            $$= NEW_PTN Item_func_weight_string(@$, $3, 0, $6, 0, true);
          }
        | WEIGHT_STRING_SYM '(' expr ',' ulong_num ',' ulong_num ',' ulong_num ')'
          {
            $$= NEW_PTN Item_func_weight_string(@$, $3, $5, $7, $9);
          }
        | geometry_function
        ;

geometry_function:
          GEOMETRYCOLLECTION_SYM '(' opt_expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_geometrycollection,
                        Geometry::wkb_point);
          }
        | LINESTRING_SYM '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_linestring,
                        Geometry::wkb_point);
          }
        | MULTILINESTRING_SYM '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_multilinestring,
                        Geometry::wkb_linestring);
          }
        | MULTIPOINT_SYM '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_multipoint,
                        Geometry::wkb_point);
          }
        | MULTIPOLYGON_SYM '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_multipolygon,
                        Geometry::wkb_polygon);
          }
        | POINT_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_point(@$, $3,$5);
          }
        | POLYGON_SYM '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_polygon,
                        Geometry::wkb_linestring);
          }
        ;

/*
  Regular function calls.
  The function name is *not* a token, and therefore is guaranteed to not
  introduce side effects to the language in general.
  MAINTAINER:
  All the new functions implemented for new features should fit into
  this category. The place to implement the function itself is
  in sql/item_create.cc
*/
function_call_generic:
          IDENT_sys '(' opt_udf_expr_list ')'
          {
            $$= NEW_PTN PTI_function_call_generic_ident_sys(@1, $1, $3);
          }
        | ident '.' ident '(' opt_expr_list ')'
          {
            $$= NEW_PTN PTI_function_call_generic_2d(@$, $1, $3, $5);
          }
        ;

fulltext_options:
          opt_natural_language_mode opt_query_expansion
          { $$= $1 | $2; }
        | IN_SYM BOOLEAN_SYM MODE_SYM
          {
            $$= FT_BOOL;
            DBUG_EXECUTE_IF("simulate_bug18831513",
                            {
                              THD *thd= YYTHD;
                              if (thd->sp_runtime_ctx)
                                MYSQLerror(NULL,thd,"syntax error");
                            });
          }
        ;

opt_natural_language_mode:
          /* nothing */                         { $$= FT_NL; }
        | IN_SYM NATURAL LANGUAGE_SYM MODE_SYM  { $$= FT_NL; }
        ;

opt_query_expansion:
          /* nothing */                         { $$= 0;         }
        | WITH QUERY_SYM EXPANSION_SYM          { $$= FT_EXPAND; }
        ;

opt_udf_expr_list:
        /* empty */     { $$= NULL; }
        | udf_expr_list { $$= $1; }
        ;

udf_expr_list:
          udf_expr
          {
            $$= NEW_PTN PT_item_list;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        | udf_expr_list ',' udf_expr
          {
            if ($1 == NULL || $1->push_back($3))
              MYSQL_YYABORT;
            $$= $1;
          }
        ;

udf_expr:
          expr select_alias
          {
            $$= NEW_PTN PTI_udf_expr(@$, $1, $2, @1.cpp);
          }
        ;

set_function_specification:
          sum_expr
        | grouping_operation
        ;

sum_expr:
          AVG_SYM '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_avg(@$, $3, FALSE);
          }
        | AVG_SYM '(' DISTINCT in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_avg(@$, $4, TRUE);
          }
        | BIT_AND  '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_and(@$, $3);
          }
        | BIT_OR  '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_or(@$, $3);
          }
        | JSON_ARRAYAGG '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_json_array(@$, $3);
          }
        | JSON_OBJECTAGG '(' in_sum_expr ',' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_json_object(@$, $3, $5);
          }
        | BIT_XOR  '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_xor(@$, $3);
          }
        | COUNT_SYM '(' opt_all '*' ')'
          {
            $$= NEW_PTN PTI_count_sym(@$);
          }
        | COUNT_SYM '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_count(@$, $3);
          }
        | COUNT_SYM '(' DISTINCT expr_list ')'
          {
            $$= new Item_sum_count(@$, $4);
          }
        | MIN_SYM '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_min(@$, $3);
          }
        /*
          According to ANSI SQL, DISTINCT is allowed and has
          no sense inside MIN and MAX grouping functions; so MIN|MAX(DISTINCT ...)
          is processed like an ordinary MIN | MAX()
        */
        | MIN_SYM '(' DISTINCT in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_min(@$, $4);
          }
        | MAX_SYM '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_max(@$, $3);
          }
        | MAX_SYM '(' DISTINCT in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_max(@$, $4);
          }
        | STD_SYM '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_std(@$, $3, 0);
          }
        | VARIANCE_SYM '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_variance(@$, $3, 0);
          }
        | STDDEV_SAMP_SYM '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_std(@$, $3, 1);
          }
        | VAR_SAMP_SYM '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_variance(@$, $3, 1);
          }
        | SUM_SYM '(' in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_sum(@$, $3, FALSE);
          }
        | SUM_SYM '(' DISTINCT in_sum_expr ')'
          {
            $$= NEW_PTN Item_sum_sum(@$, $4, TRUE);
          }
        | GROUP_CONCAT_SYM '(' opt_distinct
          expr_list opt_gorder_clause
          opt_gconcat_separator
          ')'
          {
            $$= NEW_PTN Item_func_group_concat(@$, $3, $4, $5, $6);
          }
        ;

grouping_operation:
          GROUPING_SYM '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_grouping(@$, $3);
          }
        ;

variable:
          '@' variable_aux { $$= $2; }
        ;

variable_aux:
          ident_or_text SET_VAR expr
          {
            $$= NEW_PTN PTI_variable_aux_set_var(@$, $1, $3);
          }
        | ident_or_text
          {
            $$= NEW_PTN PTI_variable_aux_ident_or_text(@$, $1);
          }
        | '@' opt_var_ident_type ident_or_text opt_component
          {
            $$= NEW_PTN PTI_variable_aux_3d(@$, $2, $3, @3, $4);
          }
        ;

opt_distinct:
          /* empty */ { $$ = 0; }
        | DISTINCT    { $$ = 1; }
        ;

opt_gconcat_separator:
          /* empty */
          {
            $$= NEW_PTN String(",", 1, &my_charset_latin1);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | SEPARATOR_SYM text_string { $$ = $2; }
        ;

opt_gorder_clause:
          /* empty */               { $$= NULL; }
        | ORDER_SYM BY gorder_list  { $$= $3; }
        ;

gorder_list:
          gorder_list ',' order_expr
          {
            $1->push_back($3);
            $$= $1;
          }
        | order_expr
          {
            $$= NEW_PTN PT_gorder_list();
            if ($1 == NULL)
              MYSQL_YYABORT;
            $$->push_back($1);
          }
        ;

in_sum_expr:
          opt_all expr
          {
            $$= NEW_PTN PTI_in_sum_expr(@1, $2);
          }
        ;

cast_type:
          BINARY_SYM opt_field_length
          {
            $$.target= ITEM_CAST_CHAR;
            $$.charset= &my_charset_bin;
            $$.length= $2;
            $$.dec= NULL;
          }
        | CHAR_SYM opt_field_length opt_charset_with_opt_binary
          {
            $$.target= ITEM_CAST_CHAR;
            $$.length= $2;
            $$.dec= NULL;
            if ($3.force_binary)
            {
              // Bugfix: before this patch we ignored [undocumented]
              // collation modifier in the CAST(expr, CHAR(...) BINARY) syntax.
              // To restore old behavior just remove this "if ($3...)" branch.

              $$.charset= get_bin_collation($3.charset ? $3.charset :
                  YYTHD->variables.collation_connection);
              if ($$.charset == NULL)
                MYSQL_YYABORT;
            }
            else
              $$.charset= $3.charset;
          }
        | nchar opt_field_length
          {
            $$.target= ITEM_CAST_CHAR;
            $$.charset= national_charset_info;
            $$.length= $2;
            $$.dec= NULL;
          }
        | SIGNED_SYM
          {
            $$.target= ITEM_CAST_SIGNED_INT;
            $$.charset= NULL;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | SIGNED_SYM INT_SYM
          {
            $$.target= ITEM_CAST_SIGNED_INT;
            $$.charset= NULL;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | UNSIGNED_SYM
          {
            $$.target= ITEM_CAST_UNSIGNED_INT;
            $$.charset= NULL;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | UNSIGNED_SYM INT_SYM
          {
            $$.target= ITEM_CAST_UNSIGNED_INT;
            $$.charset= NULL;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | DATE_SYM
          {
            $$.target= ITEM_CAST_DATE;
            $$.charset= NULL;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | TIME_SYM type_datetime_precision
          {
            $$.target= ITEM_CAST_TIME;
            $$.charset= NULL;
            $$.length= NULL;
            $$.dec= $2;
          }
        | DATETIME_SYM type_datetime_precision
          {
            $$.target= ITEM_CAST_DATETIME;
            $$.charset= NULL;
            $$.length= NULL;
            $$.dec= $2;
          }
        | DECIMAL_SYM float_options
          {
            $$.target=ITEM_CAST_DECIMAL;
            $$.charset= NULL;
            $$.length= $2.length;
            $$.dec= $2.dec;
          }
        | JSON_SYM
          {
            $$.target=ITEM_CAST_JSON;
            $$.charset= NULL;
            $$.length= NULL;
            $$.dec= NULL;
          }
        ;

opt_expr_list:
          /* empty */ { $$= NULL; }
        | expr_list
        ;

expr_list:
          expr
          {
            $$= NEW_PTN PT_item_list;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        | expr_list ',' expr
          {
            if ($1 == NULL || $1->push_back($3))
              MYSQL_YYABORT;
            $$= $1;
          }
        ;

ident_list_arg:
          ident_list          { $$= $1; }
        | '(' ident_list ')'  { $$= $2; }
        ;

ident_list:
          simple_ident
          {
            $$= NEW_PTN PT_item_list;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        | ident_list ',' simple_ident
          {
            if ($1 == NULL || $1->push_back($3))
              MYSQL_YYABORT;
            $$= $1;
          }
        ;

opt_expr:
          /* empty */    { $$= NULL; }
        | expr           { $$= $1; }
        ;

opt_else:
          /* empty */  { $$= NULL; }
        | ELSE expr    { $$= $2; }
        ;

when_list:
          WHEN_SYM expr THEN_SYM expr
          {
            $$= new List<Item>;
            if ($$ == NULL)
              MYSQL_YYABORT;
            $$->push_back($2);
            $$->push_back($4);
          }
        | when_list WHEN_SYM expr THEN_SYM expr
          {
            $1->push_back($3);
            $1->push_back($5);
            $$= $1;
          }
        ;

table_reference:
          table_factor { $$= $1; }
        | joined_table { $$= $1; }
        | '{' ident esc_table_reference '}' { $$= $3; }
        ;

/*
  The ODBC escape syntax for Outer Join is: '{' OJ joined_table '}'
  The parser does not define OJ as a token, any ident is accepted
  instead in $2 (ident). Also, all productions from table_ref can
  be escaped, not only joined_table. Both syntax extensions are safe
  and are ignored.
*/
esc_table_reference:
          table_factor { $$= $1; }
        | joined_table { $$= $1; }
        ;
/*
  Join operations are normally left-associative, as in

    t1 JOIN t2 ON t1.a = t2.a JOIN t3 ON t3.a = t2.a

  This is equivalent to

    (t1 JOIN t2 ON t1.a = t2.a) JOIN t3 ON t3.a = t2.a

  They can also be right-associative without parentheses, e.g.

    t1 JOIN t2 JOIN t3 ON t2.a = t3.a ON t1.a = t2.a

  Which is equivalent to

    t1 JOIN (t2 JOIN t3 ON t2.a = t3.a) ON t1.a = t2.a

  In MySQL, JOIN and CROSS JOIN mean the same thing, i.e.:

  - A join without a <join specification> is the same as a cross join.
  - A cross join with a <join specification> is the same as an inner join.

  For the join operation above, this means that the parser can't know until it
  has seen the last ON whether `t1 JOIN t2` was a cross join or not. The only
  way to solve the abiguity is to keep shifting the tokens on the stack, and
  not reduce until the last ON is seen. We tell Bison this by adding a fake
  token CONDITIONLESS_JOIN which has lower precedence than all tokens that
  would continue the join. These are JOIN_SYM, INNER_SYM, CROSS,
  STRAIGHT_JOIN, NATURAL, LEFT, RIGHT, ON and USING. This way the automaton
  only reduces to a cross join unless no other interpretation is
  possible. This gives a right-deep join tree for join *with* conditions,
  which is what is expected.

  The challenge here is that t1 JOIN t2 *could* have been a cross join, we
  just don't know it until afterwards. So if the query had been

    t1 JOIN t2 JOIN t3 ON t2.a = t3.a

  we will first reduce `t2 JOIN t3 ON t2.a = t3.a` to a <table_reference>,
  which is correct, but a problem arises when reducing t1 JOIN
  <table_reference>. If we were to do that, we'd get a right-deep tree. The
  solution is to build the tree downwards instead of upwards, as is normally
  done. This concept may seem outlandish at first, but it's really quite
  simple. When the semantic action for table_reference JOIN table_reference is
  executed, the parse tree is (please pardon the ASCII graphic):

                       JOIN ON t2.a = t3.a
                      /    \
                     t2    t3

  Now, normally we'd just add the cross join node on top of this tree, as:

                    JOIN
                   /    \
                 t1    JOIN ON t2.a = t3.a
                      /    \
                     t2    t3

  This is not the meaning of the query, however. The cross join should be
  addded at the bottom:


                       JOIN ON t2.a = t3.a
                      /    \
                    JOIN    t3
                   /    \
                  t1    t2

  There is only one rule to pay attention to: If the right-hand side of a
  cross join is a join tree, find its left-most leaf (which is a table
  name). Then replace this table name with a cross join of the left-hand side
  of the top cross join, and the right hand side with the original table.

  Natural joins are also syntactically conditionless, but we need to make sure
  that they are never right associative. We handle them in their own rule
  natural_join, which is left-associative only. In this case we know that
  there is no join condition to wait for, so we can reduce immediately.
*/
joined_table:
          table_reference inner_join_type table_reference ON_SYM expr
          {
            $$= NEW_PTN PT_joined_table_on($1, @2, $2, $3, $5);
          }
        | table_reference inner_join_type table_reference USING
          '(' using_list ')'
          {
            $$= NEW_PTN PT_joined_table_using($1, @2, $2, $3, $6);
          }
        | table_reference outer_join_type table_reference ON_SYM expr
          {
            $$= NEW_PTN PT_joined_table_on($1, @2, $2, $3, $5);
          }
        | table_reference outer_join_type table_reference USING '(' using_list ')'
          {
            $$= NEW_PTN PT_joined_table_using($1, @2, $2, $3, $6);
          }
        | table_reference inner_join_type table_reference
          %prec CONDITIONLESS_JOIN
          {
            auto this_cross_join= NEW_PTN PT_cross_join($1, @2, $2, NULL);

            if ($3 == NULL)
              MYSQL_YYABORT; // OOM

            $$= $3->add_cross_join(this_cross_join);
          }
        | table_reference natural_join_type table_factor
          {
            $$= NEW_PTN PT_joined_table_using($1, @2, $2, $3);
          }
        ;

natural_join_type:
          NATURAL opt_inner JOIN_SYM       { $$= JTT_NATURAL_INNER; }
        | NATURAL RIGHT opt_outer JOIN_SYM { $$= JTT_NATURAL_RIGHT; }
        | NATURAL LEFT opt_outer JOIN_SYM  { $$= JTT_NATURAL_LEFT; }
        ;

inner_join_type:
          JOIN_SYM                         { $$= JTT_INNER; }
        | INNER_SYM JOIN_SYM               { $$= JTT_INNER; }
        | CROSS JOIN_SYM                   { $$= JTT_INNER; }
        | STRAIGHT_JOIN                    { $$= JTT_STRAIGHT_INNER; }

outer_join_type:
          LEFT opt_outer JOIN_SYM          { $$= JTT_LEFT; }
        | RIGHT opt_outer JOIN_SYM         { $$= JTT_RIGHT; }
        ;

opt_inner:
          /* empty */
        | INNER_SYM
        ;

opt_outer:
          /* empty */
        | OUTER
        ;

/*
  table PARTITION (list of partitions), reusing using_list instead of creating
  a new rule for partition_list.
*/
opt_use_partition:
          /* empty */ { $$= NULL; }
        | use_partition
        ;

use_partition:
          PARTITION_SYM '(' using_list ')'
          {
            $$= $3;
          }
        ;

/**
  MySQL has a syntax extension where a comma-separated list of table
  references is allowed as a table reference in itself, for instance

    SELECT * FROM (t1, t2) JOIN t3 ON 1

  which is not allowed in standard SQL. The syntax is equivalent to

    SELECT * FROM (t1 CROSS JOIN t2) JOIN t3 ON 1

  We call this rule table_reference_list_parens.

  A <table_factor> may be a <single_table>, a <subquery>, a <derived_table>, a
  <joined_table>, or the bespoke <table_reference_list_parens>, each of those
  enclosed in any number of parentheses. This makes for an ambiguous grammar
  since a <table_factor> may also be enclosed in parentheses. We get around
  this by designing the grammar so that a <table_factor> does not have
  parentheses, but all the sub-cases of it have their own parentheses-rules,
  i.e. <single_table_parens>, <joined_table_parens> and
  <table_reference_list_parens>. It's a bit tedious but the grammar is
  unambiguous and doesn't have shift/reduce conflicts.
*/
table_factor:
          single_table
        | single_table_parens
        | derived_table { $$ = $1; }
        | joined_table_parens
          { $$= NEW_PTN PT_table_factor_joined_table($1); }
        | table_reference_list_parens
          { $$= NEW_PTN PT_table_reference_list_parens($1); }
        ;

table_reference_list_parens:
          '(' table_reference_list_parens ')' { $$= $2; }
        | '(' table_reference_list ',' table_reference ')'
          {
            $$= $2;
            if ($$.push_back($4))
              MYSQL_YYABORT; // OOM
          }
        ;

single_table_parens:
          '(' single_table_parens ')' { $$= $2; }
        | '(' single_table ')' { $$= $2; }
        ;

single_table:
          table_ident opt_use_partition opt_table_alias opt_key_definition
          {
            $$= NEW_PTN PT_table_factor_table_ident($1, $2, $3, $4);
          }
        ;

joined_table_parens:
          '(' joined_table_parens ')' { $$= $2; }
        | '(' joined_table ')' { $$= $2; }
        ;

derived_table:
          table_subquery opt_table_alias opt_derived_column_list
          {
            /*
              The alias is actually not optional at all, but being MySQL we
              are friendly and give an informative error message instead of
              just 'syntax error'.
            */
            if ($2 == NULL)
              my_message(ER_DERIVED_MUST_HAVE_ALIAS,
                         ER_THD(YYTHD, ER_DERIVED_MUST_HAVE_ALIAS), MYF(0));

            $$= NEW_PTN PT_derived_table($1, $2, &$3);
          }
        ;

index_hint_clause:
          /* empty */
          {
            $$= old_mode ?  INDEX_HINT_MASK_JOIN : INDEX_HINT_MASK_ALL;
          }
        | FOR_SYM JOIN_SYM      { $$= INDEX_HINT_MASK_JOIN;  }
        | FOR_SYM ORDER_SYM BY  { $$= INDEX_HINT_MASK_ORDER; }
        | FOR_SYM GROUP_SYM BY  { $$= INDEX_HINT_MASK_GROUP; }
        ;

index_hint_type:
          FORCE_SYM  { $$= INDEX_HINT_FORCE; }
        | IGNORE_SYM { $$= INDEX_HINT_IGNORE; }
        ;

index_hint_definition:
          index_hint_type key_or_index index_hint_clause
          '(' key_usage_list ')'
          {
            init_index_hints($5, $1, $3);
            $$= $5;
          }
        | USE_SYM key_or_index index_hint_clause
          '(' opt_key_usage_list ')'
          {
            init_index_hints($5, INDEX_HINT_USE, $3);
            $$= $5;
          }
       ;

index_hints_list:
          index_hint_definition
        | index_hints_list index_hint_definition
          {
            $2->concat($1);
            $$= $2;
          }
        ;

opt_index_hints_list:
          /* empty */ { $$= NULL; }
        | index_hints_list
        ;

opt_key_definition:
          opt_index_hints_list
        ;

opt_key_usage_list:
          /* empty */
          {
            $$= NEW_PTN List<Index_hint>;
            Index_hint *hint= NEW_PTN Index_hint(NULL, 0);
            if ($$ == NULL || hint == NULL || $$->push_front(hint))
              MYSQL_YYABORT;
          }
        | key_usage_list
        ;

key_usage_element:
          ident
          {
            $$= NEW_PTN Index_hint($1.str, $1.length);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | PRIMARY_SYM
          {
            $$= NEW_PTN Index_hint(STRING_WITH_LEN("PRIMARY"));
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

key_usage_list:
          key_usage_element
          {
            $$= NEW_PTN List<Index_hint>;
            if ($$ == NULL || $$->push_front($1))
              MYSQL_YYABORT;
          }
        | key_usage_list ',' key_usage_element
          {
            if ($$->push_front($3))
              MYSQL_YYABORT;
          }
        ;

using_list:
          ident_string_list
        ;

ident_string_list:
          ident
          {
            $$= NEW_PTN List<String>;
            String *s= NEW_PTN String(const_cast<const char *>($1.str),
                                               $1.length,
                                               system_charset_info);
            if ($$ == NULL || s == NULL || $$->push_back(s))
              MYSQL_YYABORT;
          }
        | ident_string_list ',' ident
          {
            String *s= NEW_PTN String(const_cast<const char *>($3.str),
                                               $3.length,
                                               system_charset_info);
            if (s == NULL || $1->push_back(s))
              MYSQL_YYABORT;
            $$= $1;
          }
        ;

interval:
          interval_time_stamp    {}
        | DAY_HOUR_SYM           { $$=INTERVAL_DAY_HOUR; }
        | DAY_MICROSECOND_SYM    { $$=INTERVAL_DAY_MICROSECOND; }
        | DAY_MINUTE_SYM         { $$=INTERVAL_DAY_MINUTE; }
        | DAY_SECOND_SYM         { $$=INTERVAL_DAY_SECOND; }
        | HOUR_MICROSECOND_SYM   { $$=INTERVAL_HOUR_MICROSECOND; }
        | HOUR_MINUTE_SYM        { $$=INTERVAL_HOUR_MINUTE; }
        | HOUR_SECOND_SYM        { $$=INTERVAL_HOUR_SECOND; }
        | MINUTE_MICROSECOND_SYM { $$=INTERVAL_MINUTE_MICROSECOND; }
        | MINUTE_SECOND_SYM      { $$=INTERVAL_MINUTE_SECOND; }
        | SECOND_MICROSECOND_SYM { $$=INTERVAL_SECOND_MICROSECOND; }
        | YEAR_MONTH_SYM         { $$=INTERVAL_YEAR_MONTH; }
        ;

interval_time_stamp:
          DAY_SYM         { $$=INTERVAL_DAY; }
        | WEEK_SYM        { $$=INTERVAL_WEEK; }
        | HOUR_SYM        { $$=INTERVAL_HOUR; }
        | MINUTE_SYM      { $$=INTERVAL_MINUTE; }
        | MONTH_SYM       { $$=INTERVAL_MONTH; }
        | QUARTER_SYM     { $$=INTERVAL_QUARTER; }
        | SECOND_SYM      { $$=INTERVAL_SECOND; }
        | MICROSECOND_SYM { $$=INTERVAL_MICROSECOND; }
        | YEAR_SYM        { $$=INTERVAL_YEAR; }
        ;

date_time_type:
          DATE_SYM  {$$= MYSQL_TIMESTAMP_DATE; }
        | TIME_SYM  {$$= MYSQL_TIMESTAMP_TIME; }
        | TIMESTAMP_SYM {$$= MYSQL_TIMESTAMP_DATETIME; }
        | DATETIME_SYM  {$$= MYSQL_TIMESTAMP_DATETIME; }
        ;

opt_as_or_eq:
          /* empty */
        | AS
        | EQ
        ;

opt_table_alias:
          /* empty */ { $$=0; }
        | opt_as_or_eq ident
          {
            $$= (LEX_STRING*) sql_memdup(&$2,sizeof(LEX_STRING));
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_all:
          /* empty */
        | ALL
        ;

opt_where_clause:
        opt_where_clause_expr
          {
            if ($1 != NULL)
              $$= new PTI_context<CTX_WHERE>(@$, $1);
            else
              $$= NULL;
          }
        ;

opt_where_clause_expr: /* empty */  { $$= NULL; }
        | WHERE expr
          {
            $$= $2;
          }
        ;

opt_having_clause:
          /* empty */ { $$= NULL; }
        | HAVING expr
          {
            $$= new PTI_context<CTX_HAVING>(@$, $2);
          }
        ;

with_clause:
          WITH with_list
          {
            $$= NEW_PTN PT_with_clause($2, false);
          }
        | WITH RECURSIVE_SYM with_list
          {
            $$= NEW_PTN PT_with_clause($3, true);
          }
        ;

with_list:
          with_list ',' common_table_expr
          {
            if ($1->push_back($3))
              MYSQL_YYABORT;
          }
        | common_table_expr
          {
            $$= NEW_PTN PT_with_list(YYTHD->mem_root);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;    /* purecov: inspected */
          }
        ;

common_table_expr:
          ident opt_derived_column_list AS table_subquery
          {
            LEX_STRING subq_text;
            subq_text.length= @4.raw.length();
            subq_text.str= YYTHD->strmake(@4.raw.start, subq_text.length);
            if (subq_text.str == NULL)
              MYSQL_YYABORT;   /* purecov: inspected */
            uint subq_text_offset= @4.raw.start - YYLIP->get_buf();
            $$= NEW_PTN PT_common_table_expr($1, subq_text, subq_text_offset,
                                             $4, &$2, YYTHD->mem_root);
            if ($$ == NULL)
              MYSQL_YYABORT;   /* purecov: inspected */
          }
        ;

opt_derived_column_list:
          /* empty */
          {
            /*
              Because () isn't accepted by the rule of
              simple_ident_list, we can use an empty array to
              designates that the parenthesised list was omitted.
            */
            $$.init(YYTHD->mem_root);
          }
        | '(' simple_ident_list ')'
          {
            $$= $2;
          }
        ;

simple_ident_list:
          ident
          {
            $$.init(YYTHD->mem_root);
            if ($$.push_back(to_lex_cstring($1)))
              MYSQL_YYABORT; /* purecov: inspected */
          }
        | simple_ident_list ',' ident
          {
            $$= $1;
            if ($$.push_back(to_lex_cstring($3)))
              MYSQL_YYABORT; /* purecov: inspected */
          }
        ;

opt_escape:
          ESCAPE_SYM simple_expr { $$= $2; }
        | /* empty */            { $$= NULL; }
        ;

/*
   group by statement in select
*/

opt_group_clause:
          /* empty */ { $$= NULL; }
        | GROUP_SYM BY group_list olap_opt
          {
            $$= NEW_PTN PT_group($3, $4);
          }
        ;

group_list:
          group_list ',' order_expr
          {
            $1->push_back($3);
            $$= $1;
          }
        | order_expr
          {
            $$= NEW_PTN PT_order_list();
            if ($1 == NULL)
              MYSQL_YYABORT;
            $$->push_back($1);
          }
        ;

olap_opt:
          /* empty */   { $$= UNSPECIFIED_OLAP_TYPE; }
        | WITH_ROLLUP_SYM { $$= ROLLUP_TYPE; }
            /*
              'WITH ROLLUP' is needed for backward compatibility,
              and cause LALR(2) conflicts.
              This syntax is not standard.
              MySQL syntax: GROUP BY col1, col2, col3 WITH ROLLUP
              SQL-2003: GROUP BY ... ROLLUP(col1, col2, col3)
            */
        ;

/*
  Order by statement in ALTER TABLE
*/

alter_order_clause:
          ORDER_SYM BY alter_order_list
        ;

alter_order_list:
          alter_order_list ',' alter_order_item
        | alter_order_item
        ;

alter_order_item:
          simple_ident_nospvar order_dir
          {
            ITEMIZE($1, &$1);

            THD *thd= YYTHD;
            ORDER *order= (ORDER *) thd->alloc(sizeof(ORDER));
            if (order == NULL)
              MYSQL_YYABORT;
            order->item_ptr= $1;
            order->direction= ($2 == ORDER_DESC) ? ORDER_DESC
                                                 : ORDER_ASC;
            order->is_explicit= ($2 != ORDER_NOT_RELEVANT);
            order->is_position= false;
            add_order_to_list(thd, order);
          }
        ;

/*
   Order by statement in select
*/

opt_order_clause:
          /* empty */ { $$= NULL; }
        | order_clause
        ;

order_clause:
          ORDER_SYM BY order_list
          {
            $$= NEW_PTN PT_order($3);
          }
        ;

order_list:
          order_list ',' order_expr
          {
            $1->push_back($3);
            $$= $1;
          }
        | order_expr
          {
            $$= NEW_PTN PT_order_list();
            if ($1 == NULL)
              MYSQL_YYABORT;
            $$->push_back($1);
          }
        ;

order_dir:
          /* empty */ { $$= ORDER_NOT_RELEVANT; }
        | ASC         { $$= ORDER_ASC; }
        | DESC        { $$= ORDER_DESC; }
        ;

opt_limit_clause:
          /* empty */ { $$= NULL; }
        | limit_clause
        ;

limit_clause:
          LIMIT limit_options
          {
            $$= NEW_PTN PT_limit_clause($2);
          }
        ;

limit_options:
          limit_option
          {
            $$.limit= $1;
            $$.opt_offset= NULL;
            $$.is_offset_first= false;
          }
        | limit_option ',' limit_option
          {
            $$.limit= $3;
            $$.opt_offset= $1;
            $$.is_offset_first= true;
          }
        | limit_option OFFSET_SYM limit_option
          {
            $$.limit= $1;
            $$.opt_offset= $3;
            $$.is_offset_first= false;
          }
        ;

limit_option:
          ident
          {
            $$= NEW_PTN PTI_limit_option_ident(@$, $1, @1.raw);
          }
        | param_marker
          {
            $$= NEW_PTN PTI_limit_option_param_marker(@$, $1);
          }
        | ULONGLONG_NUM
          {
            $$= NEW_PTN Item_uint(@$, $1.str, $1.length);
          }
        | LONG_NUM
          {
            $$= NEW_PTN Item_uint(@$, $1.str, $1.length);
          }
        | NUM
          {
            $$= NEW_PTN Item_uint(@$, $1.str, $1.length);
          }
        ;

opt_simple_limit:
          /* empty */        { $$= NULL; }
        | LIMIT limit_option { $$= $2; }
        ;

ulong_num:
          NUM           { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
        | HEX_NUM       { $$= (ulong) my_strtoll($1.str, (char**) 0, 16); }
        | LONG_NUM      { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
        | ULONGLONG_NUM { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
        | DECIMAL_NUM   { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
        | FLOAT_NUM     { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
        ;

real_ulong_num:
          NUM           { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
        | HEX_NUM       { $$= (ulong) my_strtoll($1.str, (char**) 0, 16); }
        | LONG_NUM      { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
        | ULONGLONG_NUM { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
        | dec_num_error { MYSQL_YYABORT; }
        ;

ulonglong_num:
          NUM           { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
        | ULONGLONG_NUM { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
        | LONG_NUM      { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
        | DECIMAL_NUM   { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
        | FLOAT_NUM     { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
        ;

real_ulonglong_num:
          NUM           { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
        | ULONGLONG_NUM { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
        | LONG_NUM      { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
        | dec_num_error { MYSQL_YYABORT; }
        ;

dec_num_error:
          dec_num
          { my_syntax_error(YYTHD, ER_THD(YYTHD, ER_ONLY_INTEGERS_ALLOWED)); }
        ;

dec_num:
          DECIMAL_NUM
        | FLOAT_NUM
        ;

select_var_list:
          select_var_list ',' select_var_ident
          {
            $$= $1;
            if ($$ == NULL || $$->push_back($3))
              MYSQL_YYABORT;
          }
        | select_var_ident
          {
            $$= NEW_PTN PT_select_var_list(@$);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        ;

select_var_ident:
          '@' ident_or_text
          {
            $$= NEW_PTN PT_select_var($2);
          }
        | ident_or_text
          {
            $$= NEW_PTN PT_select_sp_var($1);
          }
        ;

into_clause:
          INTO into_destination
          {
            $$= $2;
          }
        ;

into_destination:
          OUTFILE TEXT_STRING_filesystem
          opt_load_data_charset
          opt_field_term opt_line_term
          {
            $$= NEW_PTN PT_into_destination_outfile(@$, $2, $3, $4, $5);
          }
        | DUMPFILE TEXT_STRING_filesystem
          {
            $$= NEW_PTN PT_into_destination_dumpfile(@$, $2);
          }
        | select_var_list { $$= $1; }
        ;

/*
  DO statement
*/

do_stmt:
          DO_SYM empty_select_options select_item_list
          {
            $$= NEW_PTN PT_select_stmt(SQLCOM_DO,
                  NEW_PTN PT_query_expression(
                    NEW_PTN PT_query_expression_body_primary(
                      NEW_PTN PT_query_specification($2, $3))));
          }
        ;

empty_select_options:
          /* empty */
          {
            $$.query_spec_options= 0;
            $$.sql_cache= SELECT_LEX::SQL_CACHE_UNSPECIFIED;
          }
        ;

/*
  Drop : delete tables or index or user or role
*/

drop:
          DROP opt_temporary table_or_tables if_exists table_list opt_restrict
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_DROP_TABLE;
            lex->drop_temporary= $2;
            lex->drop_if_exists= $4;
            YYPS->m_lock_type= TL_UNLOCK;
            YYPS->m_mdl_type= MDL_EXCLUSIVE;
            if (Select->add_tables(YYTHD, $5, TL_OPTION_UPDATING,
                                   YYPS->m_lock_type, YYPS->m_mdl_type))
              MYSQL_YYABORT;
            lex->drop_mode= $6;
          }
        | DROP INDEX_SYM ident ON_SYM table_ident {}
          {
            LEX *lex=Lex;
            Alter_drop *ad= new Alter_drop(Alter_drop::KEY, $3.str);
            if (ad == NULL)
              MYSQL_YYABORT;
            lex->sql_command= SQLCOM_DROP_INDEX;
            lex->alter_info.reset();
            lex->alter_info.flags= Alter_info::ALTER_DROP_INDEX;
            lex->alter_info.drop_list.push_back(ad);
            if (!lex->current_select()->add_table_to_list(lex->thd, $5, NULL,
                                                        TL_OPTION_UPDATING,
                                                        TL_READ_NO_INSERT,
                                                        MDL_SHARED_UPGRADABLE))
              MYSQL_YYABORT;
          }
          opt_index_lock_and_algorithm
          {
            Parse_context pc(YYTHD, Select);
            if (YYTHD->is_error() || contextualize_nodes($8, &pc))
              MYSQL_YYABORT;
          }
        | DROP DATABASE if_exists ident
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_DROP_DB;
            lex->drop_if_exists=$3;
            lex->name= $4;
          }
        | DROP FUNCTION_SYM if_exists ident '.' ident
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_name *spname;
            if ($4.str &&
                (check_and_convert_db_name(&$4, false) != Ident_name_check::OK))
               MYSQL_YYABORT;
            if (sp_check_name(&$6))
               MYSQL_YYABORT;
            if (lex->sphead)
            {
              my_error(ER_SP_NO_DROP_SP, MYF(0), "FUNCTION");
              MYSQL_YYABORT;
            }
            lex->sql_command = SQLCOM_DROP_FUNCTION;
            lex->drop_if_exists= $3;
            spname= new sp_name(to_lex_cstring($4), $6, true);
            if (spname == NULL)
              MYSQL_YYABORT;
            spname->init_qname(thd);
            lex->spname= spname;
          }
        | DROP FUNCTION_SYM if_exists ident
          {
            /*
              Unlike DROP PROCEDURE, "DROP FUNCTION ident" should work
              even if there is no current database. In this case it
              applies only to UDF.
              Hence we can't merge rules for "DROP FUNCTION ident.ident"
              and "DROP FUNCTION ident" into one "DROP FUNCTION sp_name"
              rule. sp_name assumes that database name should be always
              provided - either explicitly or implicitly.
            */
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            LEX_STRING db= NULL_STR;
            sp_name *spname;
            if (lex->sphead)
            {
              my_error(ER_SP_NO_DROP_SP, MYF(0), "FUNCTION");
              MYSQL_YYABORT;
            }
            if (thd->db().str && lex->copy_db_to(&db.str, &db.length))
              MYSQL_YYABORT;
            if (sp_check_name(&$4))
               MYSQL_YYABORT;
            lex->sql_command = SQLCOM_DROP_FUNCTION;
            lex->drop_if_exists= $3;
            spname= new sp_name(to_lex_cstring(db), $4, false);
            if (spname == NULL)
              MYSQL_YYABORT;
            spname->init_qname(thd);
            lex->spname= spname;
          }
        | DROP PROCEDURE_SYM if_exists sp_name
          {
            LEX *lex=Lex;
            if (lex->sphead)
            {
              my_error(ER_SP_NO_DROP_SP, MYF(0), "PROCEDURE");
              MYSQL_YYABORT;
            }
            lex->sql_command = SQLCOM_DROP_PROCEDURE;
            lex->drop_if_exists= $3;
            lex->spname= $4;
          }
        | DROP USER if_exists user_list
          {
             LEX *lex=Lex;
             lex->sql_command= SQLCOM_DROP_USER;
             lex->drop_if_exists= $3;
             lex->users_list= *$4;
          }
        | DROP VIEW_SYM if_exists table_list opt_restrict
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_DROP_VIEW;
            lex->drop_if_exists= $3;
            YYPS->m_lock_type= TL_UNLOCK;
            YYPS->m_mdl_type= MDL_EXCLUSIVE;
            if (Select->add_tables(YYTHD, $4, TL_OPTION_UPDATING,
                                   YYPS->m_lock_type, YYPS->m_mdl_type))
              MYSQL_YYABORT;
            lex->drop_mode= $5;
          }
        | DROP EVENT_SYM if_exists sp_name
          {
            Lex->drop_if_exists= $3;
            Lex->spname= $4;
            Lex->sql_command = SQLCOM_DROP_EVENT;
          }
        | DROP TRIGGER_SYM if_exists sp_name
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_DROP_TRIGGER;
            lex->drop_if_exists= $3;
            lex->spname= $4;
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_drop_trigger();
          }
        | DROP TABLESPACE_SYM tablespace_name drop_ts_options_list
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->ts_cmd_type= DROP_TABLESPACE;
          }
        | DROP LOGFILE_SYM GROUP_SYM logfile_group_name drop_ts_options_list
          {
            LEX *lex= Lex;
            lex->alter_tablespace_info->ts_cmd_type= DROP_LOGFILE_GROUP;
          }
        | DROP SERVER_SYM if_exists ident_or_text
          {
            Lex->sql_command = SQLCOM_DROP_SERVER;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_drop_server($4, $3);
          }
        | DROP ROLE_SYM if_exists role_list
          {
            Lex->sql_command= SQLCOM_DROP_ROLE;
            PT_statement *tmp= NEW_PTN PT_drop_role($3, $4);
            MAKE_CMD(tmp);
          }
        ;

table_list:
          table_ident
          {
            $$= NEW_PTN Trivial_array<Table_ident *>(YYMEM_ROOT);
            if ($$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | table_list ',' table_ident
          {
            $$= $1;
            if ($$ == NULL || $$->push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

table_name:
          table_ident
          {
            if (!Select->add_table_to_list(YYTHD, $1, NULL,
                                           TL_OPTION_UPDATING,
                                           YYPS->m_lock_type,
                                           YYPS->m_mdl_type))
              MYSQL_YYABORT;
          }
        ;

table_alias_ref_list:
          table_ident_opt_wild
          {
            $$.init(YYMEM_ROOT);
            if ($$.push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | table_alias_ref_list ',' table_ident_opt_wild
          {
            $$= $1;
            if ($$.push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

if_exists:
          /* empty */ { $$= 0; }
        | IF EXISTS { $$= 1; }
        ;

opt_temporary:
          /* empty */ { $$= false; }
        | TEMPORARY   { $$= true; }
        ;

drop_ts_options_list:
          /* empty */
        | drop_ts_options

drop_ts_options:
          drop_ts_option
        | drop_ts_options opt_comma drop_ts_option
        ;

drop_ts_option:
          opt_ts_engine
        | ts_wait

/*
** Insert : add new data to table
*/

insert_stmt:
          INSERT_SYM                   /* #1 */
          insert_lock_option           /* #2 */
          opt_ignore                   /* #3 */
          opt_INTO                     /* #4 */
          table_ident                  /* #5 */
          opt_use_partition            /* #6 */
          insert_from_constructor      /* #7 */
          opt_insert_update_list       /* #8 */
          {
            $$= NEW_PTN PT_insert(false, $1, $2, $3, $5, $6,
                                  $7.column_list, $7.row_value_list,
                                  NULL,
                                  $8.column_list, $8.value_list);
          }
        | INSERT_SYM                   /* #1 */
          insert_lock_option           /* #2 */
          opt_ignore                   /* #3 */
          opt_INTO                     /* #4 */
          table_ident                  /* #5 */
          opt_use_partition            /* #6 */
          SET_SYM                      /* #7 */
          update_list                  /* #8 */
          opt_insert_update_list       /* #9 */
          {
            PT_insert_values_list *one_row= NEW_PTN PT_insert_values_list;
            if (one_row == NULL || one_row->push_back(&$8.value_list->value))
              MYSQL_YYABORT; // OOM
            $$= NEW_PTN PT_insert(false, $1, $2, $3, $5, $6,
                                  $8.column_list, one_row,
                                  NULL,
                                  $9.column_list, $9.value_list);
          }
        | INSERT_SYM                   /* #1 */
          insert_lock_option           /* #2 */
          opt_ignore                   /* #3 */
          opt_INTO                     /* #4 */
          table_ident                  /* #5 */
          opt_use_partition            /* #6 */
          insert_query_expression      /* #7 */
          opt_insert_update_list       /* #8 */
          {
            $$= NEW_PTN PT_insert(false, $1, $2, $3, $5, $6,
                                  $7.column_list, NULL,
                                  $7.insert_query_expression,
                                  $8.column_list, $8.value_list);
          }
        ;

replace_stmt:
          REPLACE_SYM                   /* #1 */
          replace_lock_option           /* #2 */
          opt_INTO                      /* #3 */
          table_ident                   /* #4 */
          opt_use_partition             /* #5 */
          insert_from_constructor       /* #6 */
          {
            $$= NEW_PTN PT_insert(true, $1, $2, false, $4, $5,
                                  $6.column_list, $6.row_value_list,
                                  NULL,
                                  NULL, NULL);
          }
        | REPLACE_SYM                   /* #1 */
          replace_lock_option           /* #2 */
          opt_INTO                      /* #3 */
          table_ident                   /* #4 */
          opt_use_partition             /* #5 */
          SET_SYM                       /* #6 */
          update_list                   /* #7 */
          {
            PT_insert_values_list *one_row= NEW_PTN PT_insert_values_list;
            if (one_row == NULL || one_row->push_back(&$7.value_list->value))
              MYSQL_YYABORT; // OOM
            $$= NEW_PTN PT_insert(true, $1, $2, false, $4, $5,
                                  $7.column_list, one_row,
                                  NULL,
                                  NULL, NULL);
          }
        | REPLACE_SYM                   /* #1 */
          replace_lock_option           /* #2 */
          opt_INTO                      /* #3 */
          table_ident                   /* #4 */
          opt_use_partition             /* #5 */
          insert_query_expression       /* #6 */
          {
            $$= NEW_PTN PT_insert(true, $1, $2, false, $4, $5,
                                  $6.column_list, NULL,
                                  $6.insert_query_expression,
                                  NULL, NULL);
          }
        ;

insert_lock_option:
          /* empty */   { $$= TL_WRITE_CONCURRENT_DEFAULT; }
        | LOW_PRIORITY  { $$= TL_WRITE_LOW_PRIORITY; }
        | DELAYED_SYM
        {
          $$= TL_WRITE_CONCURRENT_DEFAULT;

          push_warning_printf(YYTHD, Sql_condition::SL_WARNING,
                              ER_WARN_LEGACY_SYNTAX_CONVERTED,
                              ER_THD(YYTHD, ER_WARN_LEGACY_SYNTAX_CONVERTED),
                              "INSERT DELAYED", "INSERT");
        }
        | HIGH_PRIORITY { $$= TL_WRITE; }
        ;

replace_lock_option:
          opt_low_priority { $$= $1; }
        | DELAYED_SYM
        {
          $$= TL_WRITE_DEFAULT;

          push_warning_printf(YYTHD, Sql_condition::SL_WARNING,
                              ER_WARN_LEGACY_SYNTAX_CONVERTED,
                              ER_THD(YYTHD, ER_WARN_LEGACY_SYNTAX_CONVERTED),
                              "REPLACE DELAYED", "REPLACE");
        }
        ;

opt_INTO:
          /* empty */
        | INTO
        ;

insert_from_constructor:
          insert_values
          {
            $$.column_list= NEW_PTN PT_item_list;
            $$.row_value_list= $1;
          }
        | '(' ')' insert_values
          {
            $$.column_list= NEW_PTN PT_item_list;
            $$.row_value_list= $3;
          }
        | '(' fields ')' insert_values
          {
            $$.column_list= $2;
            $$.row_value_list= $4;
          }
        ;

insert_query_expression:
          query_expression_or_parens
          {
            $$.column_list= NEW_PTN PT_item_list;
            $$.insert_query_expression= $1;
          }
        | '(' ')' query_expression_or_parens
          {
            $$.column_list= NEW_PTN PT_item_list;
            $$.insert_query_expression= $3;
          }
        | '(' fields ')' query_expression_or_parens
          {
            $$.column_list= $2;
            $$.insert_query_expression= $4;
          }
        ;

fields:
          fields ',' insert_ident
          {
            if ($$->push_back($3))
              MYSQL_YYABORT;
            $$= $1;
          }
        | insert_ident
          {
            $$= NEW_PTN PT_item_list;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        ;

insert_values:
          value_or_values values_list
          {
            $$= $2;
          }
        ;

query_expression_or_parens:
          query_expression
        | query_expression_parens
        ;

value_or_values:
          VALUE_SYM
        | VALUES
        ;

values_list:
          values_list ','  row_value
          {
            if ($$->push_back(&$3->value))
              MYSQL_YYABORT;
          }
        | row_value
          {
            $$= NEW_PTN PT_insert_values_list;
            if ($$ == NULL || $$->push_back(&$1->value))
              MYSQL_YYABORT;
          }
        ;


equal:
          EQ
        | SET_VAR
        ;

opt_equal:
          /* empty */
        | equal
        ;

row_value:
          '(' opt_values ')' { $$= $2; }
        ;

opt_values:
          /* empty */
          {
            $$= NEW_PTN PT_item_list;
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | values
        ;

values:
          values ','  expr_or_default
          {
            if ($1->push_back($3))
              MYSQL_YYABORT;
            $$= $1;
          }
        | expr_or_default
          {
            $$= NEW_PTN PT_item_list;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        ;

expr_or_default:
          expr
        | DEFAULT_SYM
          {
            $$= NEW_PTN Item_default_value(@$);
          }
        ;

opt_insert_update_list:
          /* empty */
          {
            $$.value_list= NULL;
            $$.column_list= NULL;
          }
        | ON_SYM DUPLICATE_SYM KEY_SYM UPDATE_SYM update_list
          {
            $$= $5;
          }
        ;

/* Update rows in a table */

update_stmt:
          opt_with_clause
          UPDATE_SYM            /* #1 */
          opt_low_priority      /* #2 */
          opt_ignore            /* #3 */
          table_reference_list  /* #4 */
          SET_SYM               /* #5 */
          update_list           /* #6 */
          opt_where_clause      /* #7 */
          opt_order_clause      /* #8 */
          opt_simple_limit      /* #9 */
          {
            $$= NEW_PTN PT_update($1, $2, $3, $4, $5, $7.column_list, $7.value_list,
                                  $8, $9, $10);
          }
        ;

opt_with_clause:
          /* empty */ { $$= NULL; }
        | with_clause { $$= $1; }
        ;

update_list:
          update_list ',' update_elem
          {
            $$= $1;
            if ($$.column_list->push_back($3.column) ||
                $$.value_list->push_back($3.value))
              MYSQL_YYABORT; // OOM
          }
        | update_elem
          {
            $$.column_list= NEW_PTN PT_item_list;
            $$.value_list= NEW_PTN PT_item_list;
            if ($$.column_list == NULL || $$.value_list == NULL ||
                $$.column_list->push_back($1.column) ||
                $$.value_list->push_back($1.value))
              MYSQL_YYABORT; // OOM
          }
        ;

update_elem:
          simple_ident_nospvar equal expr_or_default
          {
            $$.column= $1;
            $$.value= $3;
          }
        ;

opt_low_priority:
          /* empty */ { $$= TL_WRITE_DEFAULT; }
        | LOW_PRIORITY { $$= TL_WRITE_LOW_PRIORITY; }
        ;

/* Delete rows from a table */

delete_stmt:
          opt_with_clause
          DELETE_SYM
          opt_delete_options
          FROM
          table_ident
          opt_use_partition
          opt_where_clause
          opt_order_clause
          opt_simple_limit
          {
            $$= NEW_PTN PT_delete($1, $2, $3, $5, $6, $7, $8, $9);
          }
        | opt_with_clause
          DELETE_SYM
          opt_delete_options
          table_alias_ref_list
          FROM
          table_reference_list
          opt_where_clause
          {
            $$= NEW_PTN PT_delete($1, $2, $3, $4, $6, $7);
          }
        | opt_with_clause
          DELETE_SYM
          opt_delete_options
          FROM
          table_alias_ref_list
          USING
          table_reference_list
          opt_where_clause
          {
            $$= NEW_PTN PT_delete($1, $2, $3, $5, $7, $8);
          }
        ;

opt_wild:
          /* empty */
        | '.' '*'
        ;

opt_delete_options:
          /* empty */                          { $$= 0; }
        | opt_delete_option opt_delete_options { $$= $1 | $2; }
        ;

opt_delete_option:
          QUICK        { $$= DELETE_QUICK; }
        | LOW_PRIORITY { $$= DELETE_LOW_PRIORITY; }
        | IGNORE_SYM   { $$= DELETE_IGNORE; }
        ;

truncate:
          TRUNCATE_SYM opt_table_sym
          {
            LEX* lex= Lex;
            lex->sql_command= SQLCOM_TRUNCATE;
            lex->alter_info.reset();
            YYPS->m_lock_type= TL_WRITE;
            YYPS->m_mdl_type= MDL_EXCLUSIVE;
          }
          table_name
          {
            THD *thd= YYTHD;
            LEX* lex= thd->lex;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= NEW_PTN Sql_cmd_truncate_table();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_table_sym:
          /* empty */
        | TABLE_SYM
        ;

opt_profile_defs:
  /* empty */
  | profile_defs;

profile_defs:
  profile_def
  | profile_defs ',' profile_def;

profile_def:
  CPU_SYM
    {
      Lex->profile_options|= PROFILE_CPU;
    }
  | MEMORY_SYM
    {
      Lex->profile_options|= PROFILE_MEMORY;
    }
  | BLOCK_SYM IO_SYM
    {
      Lex->profile_options|= PROFILE_BLOCK_IO;
    }
  | CONTEXT_SYM SWITCHES_SYM
    {
      Lex->profile_options|= PROFILE_CONTEXT;
    }
  | PAGE_SYM FAULTS_SYM
    {
      Lex->profile_options|= PROFILE_PAGE_FAULTS;
    }
  | IPC_SYM
    {
      Lex->profile_options|= PROFILE_IPC;
    }
  | SWAPS_SYM
    {
      Lex->profile_options|= PROFILE_SWAPS;
    }
  | SOURCE_SYM
    {
      Lex->profile_options|= PROFILE_SOURCE;
    }
  | ALL
    {
      Lex->profile_options|= PROFILE_ALL;
    }
  ;

opt_profile_args:
  /* empty */
    {
      Lex->query_id= 0;
    }
  | FOR_SYM QUERY_SYM NUM
    {
      int error;
      Lex->query_id= static_cast<my_thread_id>(my_strtoll10($3.str, NULL, &error));
      if (error != 0)
        MYSQL_YYABORT;
    }
  ;

/* Show things */

show:
          SHOW
          {
            LEX *lex=Lex;
            lex->create_info= YYTHD->alloc_typed<HA_CREATE_INFO>();
            if (lex->create_info == NULL)
              MYSQL_YYABORT; // OOM
          }
          show_param
        ;

show_param:
           DATABASES opt_wild_or_where_for_show
           {
             Lex->sql_command= SQLCOM_SHOW_DATABASES;
             if (Lex->set_wild($2.wild))
               MYSQL_YYABORT; // OOM
             if (dd::info_schema::build_show_databases_query(
                       @$, YYTHD, Lex->wild, $2.where) == nullptr)
               MYSQL_YYABORT;
           }
         | opt_full TABLES opt_db opt_wild_or_where_for_show
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SHOW_TABLES;
             lex->verbose= $1;
             lex->select_lex->db= $3;
             if (Lex->set_wild($4.wild))
               MYSQL_YYABORT; // OOM
             if (dd::info_schema::build_show_tables_query(@$, YYTHD, lex->wild,
                                         $4.where, false) == nullptr)
               MYSQL_YYABORT;
           }
         | opt_full TRIGGERS_SYM opt_db opt_wild_or_where_for_show
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SHOW_TRIGGERS;
             lex->verbose= $1;
             lex->select_lex->db= $3;
             if (Lex->set_wild($4.wild))
               MYSQL_YYABORT; // OOM
             if (dd::info_schema::build_show_triggers_query(
                                    @$, YYTHD, lex->wild, $4.where) == nullptr)
               MYSQL_YYABORT;
           }
         | EVENTS_SYM opt_db opt_wild_or_where_for_show
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SHOW_EVENTS;
             lex->select_lex->db= $2;
             if (Lex->set_wild($3.wild))
               MYSQL_YYABORT; // OOM
             if (dd::info_schema::build_show_events_query(
                                    @$, YYTHD, lex->wild, $3.where) == nullptr)
               MYSQL_YYABORT;
           }
         | TABLE_SYM STATUS_SYM opt_db opt_wild_or_where_for_show
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SHOW_TABLE_STATUS;
             lex->select_lex->db= $3;
             if (Lex->set_wild($4.wild))
               MYSQL_YYABORT; // OOM
             if (dd::info_schema::build_show_tables_query(@$, YYTHD, lex->wild,
                                         $4.where, true) == nullptr)
               MYSQL_YYABORT;
           }
        | OPEN_SYM TABLES opt_db opt_wild_or_where
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_OPEN_TABLES;
            lex->select_lex->db= $3;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_OPEN_TABLES))
              MYSQL_YYABORT;
          }
        | PLUGINS_SYM
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_PLUGINS;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_PLUGINS))
              MYSQL_YYABORT;
          }
        | ENGINE_SYM ident_or_text show_engine_param
          {
            const bool is_temp_table=
              Lex->create_info->options & HA_LEX_CREATE_TMP_TABLE;
            if (resolve_engine(YYTHD, $2, is_temp_table, true,
                               &Lex->create_info->db_type))
              MYSQL_YYABORT;
          }
        | ENGINE_SYM ALL show_engine_param
          { Lex->create_info->db_type= NULL; }
        | opt_full COLUMNS from_or_in table_ident opt_db opt_wild_or_where_for_show
          {
            LEX *lex= Lex;

            // TODO: error if table_ident is <db>.<table> and opt_db is set.
            if ($5)
              $4->change_db($5);

            Item *where= $6.where;
            LEX_STRING wild= $6.wild;
            DBUG_ASSERT((wild.str == nullptr) || (where == nullptr));

            auto *p= where ? NEW_PTN PT_show_fields(@$, $1, $4, where)
                           : NEW_PTN PT_show_fields(@$, $1, $4, wild);

            lex->sql_command= SQLCOM_SHOW_FIELDS;
            MAKE_CMD(p);
          }
        | master_or_binary LOGS_SYM
          {
            Lex->sql_command = SQLCOM_SHOW_BINLOGS;
          }
        | SLAVE HOSTS_SYM
          {
            Lex->sql_command = SQLCOM_SHOW_SLAVE_HOSTS;
          }
        | BINLOG_SYM EVENTS_SYM binlog_in binlog_from
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_BINLOG_EVENTS;
          }
          opt_limit_clause
          {
            if ($6 != NULL)
              CONTEXTUALIZE($6);
          }
        | RELAYLOG_SYM EVENTS_SYM binlog_in binlog_from
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_RELAYLOG_EVENTS;
          }
          opt_limit_clause opt_channel
          {
            if ($6 != NULL)
              CONTEXTUALIZE($6);
          }
        | keys_or_index         /* #1 */
          from_or_in            /* #2 */
          table_ident           /* #3 */
          opt_db                /* #4 */
          opt_where_clause_expr /* #5 */
          {
            LEX *lex= Lex;

            // TODO: error if table_ident is <db>.<table> and opt_db is set.
            if ($4)
              $3->change_db($4);

            auto *p= NEW_PTN PT_show_keys(@$, $3, $5);

            lex->sql_command= SQLCOM_SHOW_KEYS;
            MAKE_CMD(p);
          }
        | opt_storage ENGINES_SYM
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_SHOW_STORAGE_ENGINES;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_ENGINES))
              MYSQL_YYABORT;
          }
        | COUNT_SYM '(' '*' ')' WARNINGS
          {
            Lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // SHOW WARNINGS doesn't clear them.
            Parse_context pc(YYTHD, Select);
            if (create_select_for_variable(&pc, "warning_count"))
              YYABORT;
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_select(NULL);
          }
        | COUNT_SYM '(' '*' ')' ERRORS
          {
            Lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // SHOW ERRORS doesn't clear them.
            Parse_context pc(YYTHD, Select);
            if (create_select_for_variable(&pc, "error_count"))
              YYABORT;
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_select(NULL);
          }
        | WARNINGS opt_limit_clause
          {
            if ($2 != NULL)
              CONTEXTUALIZE($2);

            Lex->sql_command = SQLCOM_SHOW_WARNS;
            Lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // SHOW WARNINGS doesn't clear them.
          }
        | ERRORS opt_limit_clause
          {
            if ($2 != NULL)
              CONTEXTUALIZE($2);

            Lex->sql_command = SQLCOM_SHOW_ERRORS;
            Lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // SHOW ERRORS doesn't clear them.
          }
        | PROFILES_SYM
          {
            push_warning_printf(YYTHD, Sql_condition::SL_WARNING,
                                ER_WARN_DEPRECATED_SYNTAX,
                                ER_THD(YYTHD, ER_WARN_DEPRECATED_SYNTAX),
                                "SHOW PROFILES", "Performance Schema");
            Lex->sql_command = SQLCOM_SHOW_PROFILES;
          }
        | PROFILE_SYM opt_profile_defs opt_profile_args opt_limit_clause
          {
            if ($4 != NULL)
              CONTEXTUALIZE($4);

            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_PROFILE;
            if (prepare_schema_table(YYTHD, lex, NULL, SCH_PROFILES) != 0)
              YYABORT;
          }
        | opt_var_type STATUS_SYM opt_wild_or_where_for_show
          {
            Lex->sql_command= SQLCOM_SHOW_STATUS;
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            if (lex->set_wild($3.wild))
              MYSQL_YYABORT; // OOM

            if ($1 == OPT_SESSION)
            {
              if (build_show_session_status(
                    @$, thd, lex->wild, $3.where) == nullptr)
                MYSQL_YYABORT;
            }
            else
            {
              if (build_show_global_status(
                    @$, thd, lex->wild, $3.where) == nullptr)
                MYSQL_YYABORT;
            }
          }
        | opt_full PROCESSLIST_SYM
          {
            Lex->sql_command= SQLCOM_SHOW_PROCESSLIST;
            Lex->verbose= $1;
          }
        | opt_var_type VARIABLES opt_wild_or_where_for_show
          {
            Lex->sql_command= SQLCOM_SHOW_VARIABLES;
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            if (lex->set_wild($3.wild))
              MYSQL_YYABORT; // OOM

            if ($1 == OPT_SESSION)
            {
              if (build_show_session_variables(
                    @$, thd, lex->wild, $3.where) == nullptr)
                MYSQL_YYABORT;
            }
            else
            {
              if (build_show_global_variables(
                    @$, thd, lex->wild, $3.where) == nullptr)
                MYSQL_YYABORT;
            }
          }
        | charset opt_wild_or_where_for_show
          {
            Lex->sql_command= SQLCOM_SHOW_CHARSETS;
            if (Lex->set_wild($2.wild))
              MYSQL_YYABORT; // OOM
            if (dd::info_schema::build_show_character_set_query(
                                  @$, YYTHD, Lex->wild, $2.where) == nullptr)
              MYSQL_YYABORT;
          }
        | COLLATION_SYM opt_wild_or_where_for_show
          {
            Lex->sql_command= SQLCOM_SHOW_COLLATIONS;
            if (Lex->set_wild($2.wild))
              MYSQL_YYABORT; // OOM
            if (dd::info_schema::build_show_collation_query(
                                  @$, YYTHD, Lex->wild, $2.where) == nullptr)
              MYSQL_YYABORT;
          }
        | PRIVILEGES
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_SHOW_PRIVILEGES;
            /* Show all available grants in the server */
          }
        | GRANTS
          {
            Lex->sql_command= SQLCOM_SHOW_GRANTS;			
            PT_statement *tmp= NEW_PTN PT_show_privileges(0, 0);
            MAKE_CMD(tmp);
          }
        | GRANTS FOR_SYM user
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_GRANTS;
            PT_statement *tmp= NEW_PTN PT_show_privileges($3, 0);
            MAKE_CMD(tmp);
          }
        | GRANTS FOR_SYM user USING user_list
          {
            Lex->sql_command= SQLCOM_SHOW_GRANTS;
            PT_statement *tmp= NEW_PTN PT_show_privileges($3, $5);
            MAKE_CMD(tmp);
          }
        | CREATE DATABASE opt_if_not_exists ident
          {
            Lex->sql_command=SQLCOM_SHOW_CREATE_DB;
            Lex->create_info->options= $3 ? HA_LEX_CREATE_IF_NOT_EXISTS : 0;
            Lex->name= $4;
          }
        | CREATE TABLE_SYM table_ident
          {
            LEX *lex= Lex;
            lex->sql_command = SQLCOM_SHOW_CREATE;
            if (!lex->select_lex->add_table_to_list(YYTHD, $3, NULL,0))
              MYSQL_YYABORT;
            lex->only_view= 0;
            lex->create_info->storage_media= HA_SM_DEFAULT;
          }
        | CREATE VIEW_SYM table_ident
          {
            LEX *lex= Lex;
            lex->sql_command = SQLCOM_SHOW_CREATE;
            if (!lex->select_lex->add_table_to_list(YYTHD, $3, NULL, 0))
              MYSQL_YYABORT;
            lex->only_view= 1;
          }
        | MASTER_SYM STATUS_SYM
          {
            Lex->sql_command = SQLCOM_SHOW_MASTER_STAT;
          }
        | SLAVE STATUS_SYM opt_channel
          {
            Lex->sql_command = SQLCOM_SHOW_SLAVE_STAT;
          }
        | CREATE PROCEDURE_SYM sp_name
          {
            LEX *lex= Lex;

            lex->sql_command = SQLCOM_SHOW_CREATE_PROC;
            lex->spname= $3;
          }
        | CREATE FUNCTION_SYM sp_name
          {
            LEX *lex= Lex;

            lex->sql_command = SQLCOM_SHOW_CREATE_FUNC;
            lex->spname= $3;
          }
        | CREATE TRIGGER_SYM sp_name
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_CREATE_TRIGGER;
            lex->spname= $3;
          }
        | PROCEDURE_SYM STATUS_SYM opt_wild_or_where_for_show
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_STATUS_PROC;
             if (Lex->set_wild($3.wild))
               MYSQL_YYABORT; // OOM
            if (dd::info_schema::build_show_procedures_query(
                                    @$, YYTHD, lex->wild, $3.where) == nullptr)
              MYSQL_YYABORT;
          }
        | FUNCTION_SYM STATUS_SYM opt_wild_or_where_for_show
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_STATUS_FUNC;
             if (Lex->set_wild($3.wild))
               MYSQL_YYABORT; // OOM
            if (dd::info_schema::build_show_procedures_query(
                                    @$, YYTHD, lex->wild, $3.where) == nullptr)
              MYSQL_YYABORT;
          }
        | PROCEDURE_SYM CODE_SYM sp_name
          {
            Lex->sql_command= SQLCOM_SHOW_PROC_CODE;
            Lex->spname= $3;
          }
        | FUNCTION_SYM CODE_SYM sp_name
          {
            Lex->sql_command= SQLCOM_SHOW_FUNC_CODE;
            Lex->spname= $3;
          }
        | CREATE EVENT_SYM sp_name
          {
            Lex->spname= $3;
            Lex->sql_command = SQLCOM_SHOW_CREATE_EVENT;
          }
        | CREATE USER user
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_SHOW_CREATE_USER;
            lex->grant_user=$3;
          }
        ;

show_engine_param:
          STATUS_SYM
          { Lex->sql_command= SQLCOM_SHOW_ENGINE_STATUS; }
        | MUTEX_SYM
          { Lex->sql_command= SQLCOM_SHOW_ENGINE_MUTEX; }
        | LOGS_SYM
          { Lex->sql_command= SQLCOM_SHOW_ENGINE_LOGS; }
        ;

master_or_binary:
          MASTER_SYM
        | BINARY_SYM
        ;

opt_storage:
          /* empty */
        | STORAGE_SYM
        ;

opt_db:
          /* empty */  { $$= 0; }
        | from_or_in ident { $$= $2.str; }
        ;

opt_full:
          /* empty */ { $$= 0; }
        | FULL        { $$= 1; }
        ;

from_or_in:
          FROM
        | IN_SYM
        ;

binlog_in:
          /* empty */            { Lex->mi.log_file_name = 0; }
        | IN_SYM TEXT_STRING_sys { Lex->mi.log_file_name = $2.str; }
        ;

binlog_from:
          /* empty */        { Lex->mi.pos = 4; /* skip magic number */ }
        | FROM ulonglong_num { Lex->mi.pos = $2; }
        ;

opt_wild_or_where:
          /* empty */
        | LIKE TEXT_STRING_sys
          {
            if (Lex->set_wild($2))
              MYSQL_YYABORT; // OOM
          }
        | WHERE expr
          {
            ITEMIZE($2, &$2);

            Select->set_where_cond($2);
            if ($2)
              $2->top_level_item();
          }
        ;

opt_wild_or_where_for_show:
          /* empty */                   { $$= { NULL_STR, nullptr }; }
        | LIKE TEXT_STRING_literal      { $$= { $2, nullptr}; }
        | WHERE expr                    { $$= { NULL_STR, $2}; }
        ;

/* A Oracle compatible synonym for show */
describe:
          describe_command table_ident opt_describe_column
          {
            LEX *lex= Lex;
            lex->current_select()->parsing_place= CTX_SELECT_LIST;
            lex->select_lex->db= NULL;

            auto *p= NEW_PTN PT_show_fields(@$, false, $2);

            lex->sql_command= SQLCOM_SHOW_FIELDS;
            MAKE_CMD(p);

            // WL#6599 opt_describe_column is handled during prepare stage in
            // prepare_schema_dd_view instead of execution stage
            Select->parsing_place= CTX_NONE;
          }
        | describe_command opt_extended_describe
          {
            Lex->describe|= DESCRIBE_NORMAL;
          }
          explainable_command
        ;

explainable_command:
          select_stmt                           { MAKE_CMD($1); }
        | insert_stmt                           { MAKE_CMD($1); }
        | replace_stmt                          { MAKE_CMD($1); }
        | update_stmt                           { MAKE_CMD($1); }
        | delete_stmt                           { MAKE_CMD($1); }
        | FOR_SYM CONNECTION_SYM real_ulong_num
          {
            Lex->sql_command= SQLCOM_EXPLAIN_OTHER;
            if (Lex->sphead)
            {
              my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                       "non-standalone EXPLAIN FOR CONNECTION");
              MYSQL_YYABORT;
            }
            Lex->query_id= (my_thread_id)($3);
          }
        ;

describe_command:
          DESC
        | DESCRIBE
        ;

opt_extended_describe:
          /* empty */
          {
            if ((Lex->explain_format= new Explain_format_traditional) == NULL)
              MYSQL_YYABORT;
          }
        | EXTENDED_SYM
          {
            if ((Lex->explain_format= new Explain_format_traditional) == NULL)
              MYSQL_YYABORT;
            push_deprecated_warn_no_replacement(YYTHD, "EXTENDED");
          }
        | PARTITIONS_SYM
          {
            if ((Lex->explain_format= new Explain_format_traditional) == NULL)
              MYSQL_YYABORT;
            push_deprecated_warn_no_replacement(YYTHD, "PARTITIONS");
          }
        | FORMAT_SYM EQ ident_or_text
          {
            if (!my_strcasecmp(system_charset_info, $3.str, "JSON"))
            {
              if ((Lex->explain_format= new Explain_format_JSON) == NULL)
                MYSQL_YYABORT;
            }
            else if (!my_strcasecmp(system_charset_info, $3.str, "TRADITIONAL"))
            {
              if ((Lex->explain_format= new Explain_format_traditional) == NULL)
                MYSQL_YYABORT;
            }
            else
            {
              my_error(ER_UNKNOWN_EXPLAIN_FORMAT, MYF(0), $3.str);
              MYSQL_YYABORT;
            }
          }
        ;

opt_describe_column:
          /* empty */ {}
        | text_string { Lex->wild= $1; }
        | ident
          {
            Lex->wild= NEW_PTN String((const char*) $1.str,
                                      $1.length,
                                      system_charset_info);
            if (Lex->wild == NULL)
              MYSQL_YYABORT;
          }
        ;


/* flush things */

flush:
          FLUSH_SYM opt_no_write_to_binlog
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_FLUSH;
            lex->type= 0;
            lex->no_write_to_binlog= $2;
          }
          flush_options
          {}
        ;

flush_options:
          table_or_tables opt_table_list
          {
            Lex->type|= REFRESH_TABLES;
            /*
              Set type of metadata and table locks for
              FLUSH TABLES table_list [WITH READ LOCK].
            */
            YYPS->m_lock_type= TL_READ_NO_INSERT;
            YYPS->m_mdl_type= MDL_SHARED_HIGH_PRIO;
            if (Select->add_tables(YYTHD, $2, TL_OPTION_UPDATING,
                                   YYPS->m_lock_type, YYPS->m_mdl_type))
              MYSQL_YYABORT;
          }
          opt_flush_lock {}
        | flush_options_list
        ;

opt_flush_lock:
          /* empty */ {}
        | WITH READ_SYM LOCK_SYM
          {
            TABLE_LIST *tables= Lex->query_tables;
            Lex->type|= REFRESH_READ_LOCK;
            for (; tables; tables= tables->next_global)
            {
              tables->mdl_request.set_type(MDL_SHARED_NO_WRITE);
              /* Don't try to flush views. */
              tables->required_type= dd::enum_table_type::BASE_TABLE;
              tables->open_type= OT_BASE_ONLY;      /* Ignore temporary tables. */
            }
          }
        | FOR_SYM
          {
            if (Lex->query_tables == NULL) // Table list can't be empty
            {
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_NO_TABLES_USED));
              MYSQL_YYABORT;
            }
          }
          EXPORT_SYM
          {
            TABLE_LIST *tables= Lex->query_tables;
            Lex->type|= REFRESH_FOR_EXPORT;
            for (; tables; tables= tables->next_global)
            {
              tables->mdl_request.set_type(MDL_SHARED_NO_WRITE);
              /* Don't try to flush views. */
              tables->required_type= dd::enum_table_type::BASE_TABLE;
              tables->open_type= OT_BASE_ONLY;      /* Ignore temporary tables. */
            }
          }
        ;

flush_options_list:
          flush_options_list ',' flush_option
        | flush_option
          {}
        ;

flush_option:
          ERROR_SYM LOGS_SYM
          { Lex->type|= REFRESH_ERROR_LOG; }
        | ENGINE_SYM LOGS_SYM
          { Lex->type|= REFRESH_ENGINE_LOG; }
        | GENERAL LOGS_SYM
          { Lex->type|= REFRESH_GENERAL_LOG; }
        | SLOW LOGS_SYM
          { Lex->type|= REFRESH_SLOW_LOG; }
        | BINARY_SYM LOGS_SYM
          { Lex->type|= REFRESH_BINARY_LOG; }
        | RELAY LOGS_SYM opt_channel
          { Lex->type|= REFRESH_RELAY_LOG; }
        | QUERY_SYM CACHE_SYM
          { Lex->type|= REFRESH_QUERY_CACHE_FREE; }
        | HOSTS_SYM
          { Lex->type|= REFRESH_HOSTS; }
        | PRIVILEGES
          { Lex->type|= REFRESH_GRANT; }
        | LOGS_SYM
          { Lex->type|= REFRESH_LOG; }
        | STATUS_SYM
          { Lex->type|= REFRESH_STATUS; }
        | DES_KEY_FILE
          { Lex->type|= REFRESH_DES_KEY_FILE; }
        | RESOURCES
          { Lex->type|= REFRESH_USER_RESOURCES; }
        | OPTIMIZER_COSTS_SYM
          { Lex->type|= REFRESH_OPTIMIZER_COSTS; }
        ;

opt_table_list:
          /* empty */  { $$= NULL; }
        | table_list
        ;

reset:
          RESET_SYM
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_RESET; lex->type=0;
          }
          reset_options
          {}
        | RESET_SYM PERSIST_SYM opt_if_exists_ident
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_RESET;
            lex->type|= REFRESH_PERSIST;
            lex->option_type= OPT_PERSIST;
          }
        ;

reset_options:
          reset_options ',' reset_option
        | reset_option
        ;

opt_if_exists_ident:
          /* empty */
          {
            LEX *lex=Lex;
            lex->drop_if_exists= false;
            lex->name= NULL_STR;
          }
        | if_exists ident
          {
            LEX *lex=Lex;
            lex->drop_if_exists= $1;
            lex->name= $2;
          }
        ;

reset_option:
          SLAVE               { Lex->type|= REFRESH_SLAVE; }
          slave_reset_options opt_channel
        | MASTER_SYM          { Lex->type|= REFRESH_MASTER; }
          master_reset_options
        | QUERY_SYM CACHE_SYM { Lex->type|= REFRESH_QUERY_CACHE;}
        ;

slave_reset_options:
          /* empty */ { Lex->reset_slave_info.all= false; }
        | ALL         { Lex->reset_slave_info.all= true; }
        ;

master_reset_options:
          /* empty */ {}
        | TO_SYM real_ulong_num
          {
            if ($2 == 0 || $2 > MAX_LOG_UNIQUE_FN_EXT)
            {
              my_error(ER_RESET_MASTER_TO_VALUE_OUT_OF_RANGE, MYF(0),
                       $2, MAX_LOG_UNIQUE_FN_EXT);
              MYSQL_YYABORT;
            }
            else
              Lex->next_binlog_file_nr = $2;
          }
        ;

purge:
          PURGE
          {
            LEX *lex=Lex;
            lex->type=0;
            lex->sql_command = SQLCOM_PURGE;
          }
          purge_options
          {}
        ;

purge_options:
          master_or_binary LOGS_SYM purge_option
        ;

purge_option:
          TO_SYM TEXT_STRING_sys
          {
            Lex->to_log = $2.str;
          }
        | BEFORE_SYM expr
          {
            ITEMIZE($2, &$2);

            LEX *lex= Lex;
            lex->purge_value_list.empty();
            lex->purge_value_list.push_front($2);
            lex->sql_command= SQLCOM_PURGE_BEFORE;
          }
        ;

/* kill threads */

kill:
          KILL_SYM kill_option expr
          {
            ITEMIZE($3, &$3);

            LEX *lex=Lex;
            lex->kill_value_list.empty();
            lex->kill_value_list.push_front($3);
            lex->sql_command= SQLCOM_KILL;
          }
        ;

kill_option:
          /* empty */ { Lex->type= 0; }
        | CONNECTION_SYM { Lex->type= 0; }
        | QUERY_SYM      { Lex->type= ONLY_KILL_QUERY; }
        ;

/* change database */

use:
          USE_SYM ident
          {
            LEX *lex=Lex;
            lex->sql_command=SQLCOM_CHANGE_DB;
            lex->select_lex->db= $2.str;
          }
        ;

/* import, export of files */

load:
          LOAD data_or_xml
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0),
                       $2 == FILETYPE_CSV ? "LOAD DATA" : "LOAD XML");
              MYSQL_YYABORT;
            }
          }
          load_data_lock opt_local INFILE TEXT_STRING_filesystem
          opt_duplicate INTO TABLE_SYM table_ident opt_use_partition
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_LOAD;
            lex->local_file=  $5;
            lex->duplicates= DUP_ERROR;
            lex->set_ignore(false);
            if (!(lex->exchange= new sql_exchange($7.str, 0, $2)))
              MYSQL_YYABORT;

            switch ($8) {
            case On_duplicate::ERROR:
              Lex->duplicates=DUP_ERROR;
              break;
            case On_duplicate::IGNORE_DUP:
              Lex->set_ignore(true);
              break;
            case On_duplicate::REPLACE_DUP:
              Lex->duplicates=DUP_REPLACE;
              break;
            }

            /* Fix lock for LOAD DATA CONCURRENT REPLACE */
            if (lex->duplicates == DUP_REPLACE && $4 == TL_WRITE_CONCURRENT_INSERT)
              $4= TL_WRITE_DEFAULT;
            if (!Select->add_table_to_list(YYTHD, $11, NULL, TL_OPTION_UPDATING,
                                           $4, $4 == TL_WRITE_LOW_PRIORITY ?
                                               MDL_SHARED_WRITE_LOW_PRIO :
                                               MDL_SHARED_WRITE, NULL, $12))
              MYSQL_YYABORT;
            lex->load_field_list.empty();
            lex->load_update_list.empty();
            lex->load_value_list.empty();
            /* We can't give an error in the middle when using LOCAL files */
            if (lex->local_file && lex->duplicates == DUP_ERROR)
              lex->set_ignore(true);
          }
          opt_load_data_charset
          { Lex->exchange->cs= $14; }
          opt_xml_rows_identified_by
          opt_field_term opt_line_term opt_ignore_lines opt_field_or_var_spec
          opt_load_data_set_spec
          {
            Lex->exchange->field.merge_field_separators($17);
            Lex->exchange->line.merge_line_separators($18);
          }
          ;

data_or_xml:
        DATA_SYM  { $$= FILETYPE_CSV; }
        | XML_SYM { $$= FILETYPE_XML; }
        ;

opt_local:
          /* empty */ { $$=0;}
        | LOCAL_SYM { $$=1;}
        ;

load_data_lock:
          /* empty */ { $$= TL_WRITE_DEFAULT; }
        | CONCURRENT  { $$= TL_WRITE_CONCURRENT_INSERT; }
        | LOW_PRIORITY { $$= TL_WRITE_LOW_PRIORITY; }
        ;

opt_duplicate:
          /* empty */ { $$= On_duplicate::ERROR; }
        | duplicate
        ;

duplicate:
          REPLACE_SYM { $$= On_duplicate::REPLACE_DUP; }
        | IGNORE_SYM  { $$= On_duplicate::IGNORE_DUP; }
        ;

opt_field_term:
          /* empty */             { $$.cleanup(); }
        | COLUMNS field_term_list { $$= $2; }
        ;

field_term_list:
          field_term_list field_term
          {
            $$= $1;
            $$.merge_field_separators($2);
          }
        | field_term
        ;

field_term:
          TERMINATED BY text_string
          {
            $$.cleanup();
            $$.field_term= $3;
          }
        | OPTIONALLY ENCLOSED BY text_string
          {
            $$.cleanup();
            $$.enclosed= $4;
            $$.opt_enclosed= 1;
          }
        | ENCLOSED BY text_string
          {
            $$.cleanup();
            $$.enclosed= $3;
          }
        | ESCAPED BY text_string
          {
            $$.cleanup();
            $$.escaped= $3;
          }
        ;

opt_line_term:
          /* empty */          { $$.cleanup(); }
        | LINES line_term_list { $$= $2; }
        ;

line_term_list:
          line_term_list line_term
          {
            $$= $1;
            $$.merge_line_separators($2);
          }
        | line_term
        ;

line_term:
          TERMINATED BY text_string
          {
            $$.cleanup();
            $$.line_term= $3;
          }
        | STARTING BY text_string
          {
            $$.cleanup();
            $$.line_start= $3;
          }
        ;

opt_xml_rows_identified_by:
        /* empty */ { }
        | ROWS_SYM IDENTIFIED_SYM BY text_string
          { Lex->exchange->line.line_term = $4; };

opt_ignore_lines:
          /* empty */
        | IGNORE_SYM NUM lines_or_rows
          {
            DBUG_ASSERT(Lex->exchange != 0);
            Lex->exchange->skip_lines= atol($2.str);
          }
        ;

lines_or_rows:
        LINES { }

        | ROWS_SYM { }
        ;

opt_field_or_var_spec:
          /* empty */ {}
        | '(' fields_or_vars ')' {}
        | '(' ')' {}
        ;

fields_or_vars:
          fields_or_vars ',' field_or_var
          { Lex->load_field_list.push_back($3); }
        | field_or_var
          { Lex->load_field_list.push_back($1); }
        ;

field_or_var:
          simple_ident_nospvar { ITEMIZE($1, &$$); }
        | '@' ident_or_text
          {
            $$= NEW_PTN Item_user_var_as_out_param($2);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_load_data_set_spec:
          /* empty */ {}
        | SET_SYM load_data_set_list {}
        ;

load_data_set_list:
          load_data_set_list ',' load_data_set_elem
        | load_data_set_elem
        ;

load_data_set_elem:
          simple_ident_nospvar equal expr_or_default
          {
            ITEMIZE($1, &$1);
            ITEMIZE($3, &$3);

            LEX *lex= Lex;
            uint length= (uint) (@3.cpp.end - @2.cpp.start);
            String *val= NEW_PTN String(@2.cpp.start,
                                        length,
                                        YYTHD->charset());
            if (val == NULL)
              MYSQL_YYABORT;
            if (lex->load_update_list.push_back($1) ||
                lex->load_value_list.push_back($3) ||
                lex->load_set_str_list.push_back(val))
                MYSQL_YYABORT;
            $3->item_name.copy(@2.cpp.start, length, YYTHD->charset());
          }
        ;

/* Common definitions */

text_literal:
          TEXT_STRING
          {
            $$= NEW_PTN PTI_text_literal_text_string(@$,
                YYTHD->m_parser_state->m_lip.text_string_is_7bit(), $1);
          }
        | NCHAR_STRING
          {
            $$= NEW_PTN PTI_text_literal_nchar_string(@$,
                YYTHD->m_parser_state->m_lip.text_string_is_7bit(), $1);
          }
        | UNDERSCORE_CHARSET TEXT_STRING
          {
            $$= NEW_PTN PTI_text_literal_underscore_charset(@$,
                YYTHD->m_parser_state->m_lip.text_string_is_7bit(), $1, $2);
          }
        | text_literal TEXT_STRING_literal
          {
            $$= NEW_PTN PTI_text_literal_concat(@$,
                YYTHD->m_parser_state->m_lip.text_string_is_7bit(), $1, $2);
          }
        ;

text_string:
          TEXT_STRING_literal
          {
            $$= NEW_PTN String($1.str, $1.length,
                               YYTHD->variables.collation_connection);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | HEX_NUM
          {
            LEX_STRING s= Item_hex_string::make_hex_str($1.str, $1.length);
            $$= NEW_PTN String(s.str, s.length, &my_charset_bin);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | BIN_NUM
          {
            LEX_STRING s= Item_bin_string::make_bin_str($1.str, $1.length);
            $$= NEW_PTN String(s.str, s.length, &my_charset_bin);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

param_marker:
          PARAM_MARKER
          {
            $$= NEW_PTN Item_param(@$, YYMEM_ROOT,
                                   (uint) (@1.raw.start - YYLIP->get_buf()));
          }
        ;

signed_literal:
          literal
        | '+' NUM_literal { $$= $2; }
        | '-' NUM_literal
          {
            if ($2 == NULL)
              MYSQL_YYABORT; // OOM
            $2->max_length++;
            $$= $2->neg();
          }
        ;


literal:
          text_literal { $$= $1; }
        | NUM_literal  { $$= $1; }
        | temporal_literal
        | NULL_SYM
          {
            Lex_input_stream *lip= YYLIP;
            /*
              For the digest computation, in this context only,
              NULL is considered a literal, hence reduced to '?'
              REDUCE:
                TOK_GENERIC_VALUE := NULL_SYM
            */
            lip->reduce_digest_token(TOK_GENERIC_VALUE, NULL_SYM);
            $$= NEW_PTN Item_null(@$);
          }
        | FALSE_SYM
          {
            $$= NEW_PTN Item_int(@$, NAME_STRING("FALSE"), 0, 1);
          }
        | TRUE_SYM
          {
            $$= NEW_PTN Item_int(@$, NAME_STRING("TRUE"), 1, 1);
          }
        | HEX_NUM
          {
            $$= NEW_PTN Item_hex_string(@$, $1);
          }
        | BIN_NUM
          {
            $$= NEW_PTN Item_bin_string(@$, $1);
          }
        | UNDERSCORE_CHARSET HEX_NUM
          {
            $$= NEW_PTN PTI_literal_underscore_charset_hex_num(@$, $1, $2);
          }
        | UNDERSCORE_CHARSET BIN_NUM
          {
            $$= NEW_PTN PTI_literal_underscore_charset_bin_num(@$, $1, $2);
          }
        ;

NUM_literal:
          NUM
          {
            $$= NEW_PTN Item_int(@$, $1);
          }
        | LONG_NUM
          {
            $$= NEW_PTN Item_int(@$, $1);
          }
        | ULONGLONG_NUM
          {
            $$= NEW_PTN Item_uint(@$, $1.str, $1.length);
          }
        | DECIMAL_NUM
          {
            $$= NEW_PTN Item_decimal(@$, $1.str, $1.length, YYCSCL);
          }
        | FLOAT_NUM
          {
            $$= NEW_PTN Item_float(@$, $1.str, $1.length);
          }
        ;


temporal_literal:
        DATE_SYM TEXT_STRING
          {
            $$= NEW_PTN PTI_temporal_literal(@$, $2, MYSQL_TYPE_DATE, YYCSCL);
          }
        | TIME_SYM TEXT_STRING
          {
            $$= NEW_PTN PTI_temporal_literal(@$, $2, MYSQL_TYPE_TIME, YYCSCL);
          }
        | TIMESTAMP_SYM TEXT_STRING
          {
            $$= NEW_PTN PTI_temporal_literal(@$, $2, MYSQL_TYPE_DATETIME, YYCSCL);
          }
        ;




/**********************************************************************
** Creating different items.
**********************************************************************/

insert_ident:
          simple_ident_nospvar
        | table_wild
        ;

table_wild:
          ident '.' '*'
          {
            $$= NEW_PTN PTI_table_wild(@$, NULL, $1.str);
          }
        | ident '.' ident '.' '*'
          {
            $$= NEW_PTN PTI_table_wild(@$, $1.str, $3.str);
          }
        ;

order_expr:
          expr order_dir
          {
            $$= NEW_PTN PT_order_expr($1, (enum_order) $2);
          }
        ;

simple_ident:
          ident
          {
            $$= NEW_PTN PTI_simple_ident_ident(@$, $1);
          }
        | simple_ident_q
        ;

simple_ident_nospvar:
          ident
          {
            $$= NEW_PTN PTI_simple_ident_nospvar_ident(@$, $1);
          }
        | simple_ident_q
        ;

simple_ident_q:
          ident '.' ident
          {
            $$= NEW_PTN PTI_simple_ident_q_2d(@$, $1.str, $3.str);
          }
        | '.' ident '.' ident
          {
            $$= NEW_PTN PTI_simple_ident_q_3d(@$, NULL, $2.str, $4.str);
          }
        | ident '.' ident '.' ident
          {
            $$= NEW_PTN PTI_simple_ident_q_3d(@$, $1.str, $3.str, $5.str);
          }
        ;

field_ident:
          ident
          {
            $$= NEW_PTN PT_field_ident($1);
          }
        | ident '.' ident '.' ident
          {
            $$= NEW_PTN PT_field_ident_3d($1, $3, $5);
          }
        | ident '.' ident
          {
            $$= NEW_PTN PT_field_ident_3d($1, $3);
          }
        | '.' ident /* For Delphi */
          {
            $$= NEW_PTN PT_field_ident($2);
          }
        ;

table_ident:
          ident
          {
            $$= NEW_PTN Table_ident(to_lex_cstring($1));
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | ident '.' ident
          {
            if (YYTHD->get_protocol()->has_client_capability(CLIENT_NO_SCHEMA))
              $$= NEW_PTN Table_ident(to_lex_cstring($3));
            else {
              $$= NEW_PTN Table_ident(to_lex_cstring($1), to_lex_cstring($3));
            }
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | '.' ident
          {
            /* For Delphi */
            $$= NEW_PTN Table_ident(to_lex_cstring($2));
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

table_ident_opt_wild:
          ident opt_wild
          {
            $$= NEW_PTN Table_ident(to_lex_cstring($1));
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | ident '.' ident opt_wild
          {
            $$= NEW_PTN Table_ident(YYTHD->get_protocol(),
                                    to_lex_cstring($1),
                                    to_lex_cstring($3), 0);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

table_ident_nodb:
          ident
          {
            LEX_CSTRING db= { any_db, strlen(any_db) };
            $$= new Table_ident(YYTHD->get_protocol(),
                                db, to_lex_cstring($1), 0);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

IDENT_sys:
          IDENT { $$= $1; }
        | IDENT_QUOTED
          {
            THD *thd= YYTHD;

            if (thd->charset_is_system_charset)
            {
              const CHARSET_INFO *cs= system_charset_info;
              int dummy_error;
              size_t wlen= cs->cset->well_formed_len(cs, $1.str,
                                                     $1.str+$1.length,
                                                     $1.length, &dummy_error);
              if (wlen < $1.length)
              {
                ErrConvString err($1.str, $1.length, &my_charset_bin);
                my_error(ER_INVALID_CHARACTER_STRING, MYF(0),
                         cs->csname, err.ptr());
                MYSQL_YYABORT;
              }
              $$= $1;
            }
            else
            {
              if (thd->convert_string(&$$, system_charset_info,
                                  $1.str, $1.length, thd->charset()))
                MYSQL_YYABORT;
            }
          }
        ;

TEXT_STRING_sys_nonewline:
          TEXT_STRING_sys
          {
            if (!strcont($1.str, "\n"))
              $$= $1;
            else
            {
              my_error(ER_WRONG_VALUE, MYF(0), "argument contains not-allowed LF", $1.str);
              MYSQL_YYABORT;
            }
          }
        ;

filter_wild_db_table_string:
          TEXT_STRING_sys_nonewline
          {
            if (strcont($1.str, "."))
              $$= $1;
            else
            {
              my_error(ER_INVALID_RPL_WILD_TABLE_FILTER_PATTERN, MYF(0));
              MYSQL_YYABORT;
            }
          }
        ;

TEXT_STRING_sys:
          TEXT_STRING
          {
            THD *thd= YYTHD;

            if (thd->charset_is_system_charset)
              $$= $1;
            else
            {
              if (thd->convert_string(&$$, system_charset_info,
                                  $1.str, $1.length, thd->charset()))
                MYSQL_YYABORT;
            }
          }
        ;

TEXT_STRING_literal:
          TEXT_STRING
          {
            THD *thd= YYTHD;

            if (thd->charset_is_collation_connection)
              $$= $1;
            else
            {
              if (thd->convert_string(&$$, thd->variables.collation_connection,
                                  $1.str, $1.length, thd->charset()))
                MYSQL_YYABORT;
            }
          }
        ;

TEXT_STRING_filesystem:
          TEXT_STRING
          {
            THD *thd= YYTHD;

            if (thd->charset_is_character_set_filesystem)
              $$= $1;
            else
            {
              if (thd->convert_string(&$$,
                                      thd->variables.character_set_filesystem,
                                      $1.str, $1.length, thd->charset()))
                MYSQL_YYABORT;
            }
          }
        ;

ident:
          IDENT_sys    { $$=$1; }
        | ident_keyword
          {
            THD *thd= YYTHD;
            $$.str= thd->strmake($1.str, $1.length);
            if ($$.str == NULL)
              MYSQL_YYABORT;
            $$.length= $1.length;
          }
        ;

role_ident:
          IDENT_sys
        | role_keyword
          {
            $$.str= YYTHD->strmake($1.str, $1.length);
            if ($$.str == NULL)
              MYSQL_YYABORT;
            $$.length= $1.length;
          }
        ;

label_ident:
          IDENT_sys    { $$=$1; }
        | label_keyword
          {
            THD *thd= YYTHD;
            $$.str= thd->strmake($1.str, $1.length);
            if ($$.str == NULL)
              MYSQL_YYABORT;
            $$.length= $1.length;
          }
        ;

ident_or_text:
          ident           { $$=$1;}
        | TEXT_STRING_sys { $$=$1;}
        | LEX_HOSTNAME { $$=$1;}
        ;

role_ident_or_text:
          role_ident
        | TEXT_STRING_sys
        | LEX_HOSTNAME
        ;

user:
          ident_or_text
          {
            if (!($$= st_lex_user::alloc(YYTHD, &$1, NULL)))
              MYSQL_YYABORT;
          }
        | ident_or_text '@' ident_or_text
          {
            if (!($$= st_lex_user::alloc(YYTHD, &$1, &$3)))
              MYSQL_YYABORT;
          }
        | CURRENT_USER optional_braces
          {
            if (!($$=(LEX_USER*) YYTHD->alloc(sizeof(st_lex_user))))
              MYSQL_YYABORT;
            /*
              empty LEX_USER means current_user and
              will be handled in the  get_current_user() function
              later
            */
            memset($$, 0, sizeof(LEX_USER));
          }
        ;

role:
          role_ident_or_text
          {
            if (!($$= st_lex_user::alloc(YYTHD, &$1, NULL)))
              MYSQL_YYABORT;
          }
        | role_ident_or_text '@' ident_or_text
          {
            if (!($$= st_lex_user::alloc(YYTHD, &$1, &$3)))
              MYSQL_YYABORT;
          }
        ;

/*
  Non-reserved keywords that we allow for identifiers (except SP labels).

  Also see statement-specific rules:
    * label_keyword,
    * role_keyword

  We allow the use of some non-reserved keywords as identifiers, SP labels and
  roles, but the three sets of keywords are different and yet
  overlapping. Hence we need a somewhat complicated set of rules for all
  possible intersections of these sets: role_or_ident_keyword,
  role_or_label_keyword.
*/
ident_keyword:
          label_keyword         {}
        | role_or_ident_keyword {}
        | EXECUTE_SYM           {}
        | SHUTDOWN              {}
        ;

// These are the non-reserved keywords which may be used for roles or idents.
role_or_ident_keyword:
          ACCOUNT_SYM           {}
        | ASCII_SYM             {}
        | ALWAYS_SYM            {}
        | BACKUP_SYM            {}
        | BEGIN_SYM             {}
        | BYTE_SYM              {}
        | CACHE_SYM             {}
        | CHARSET               {}
        | CHECKSUM_SYM          {}
        | CLOSE_SYM             {}
        | COMMENT_SYM           {}
        | COMMIT_SYM            {}
        | CONTAINS_SYM          {}
        | DEALLOCATE_SYM        {}
        | DO_SYM                {}
        | END                   {}
        | FLUSH_SYM             {}
        | FOLLOWS_SYM           {}
        | FORMAT_SYM            {}
        | GROUP_REPLICATION     {}
        | HANDLER_SYM           {}
        | HELP_SYM              {}
        | HOST_SYM              {}
        | IMPORT                {}
        | INSTALL_SYM           {}
        | INVISIBLE_SYM         {}
        | LANGUAGE_SYM          {}
        | NO_SYM                {}
        | OPEN_SYM              {}
        | OPTIONS_SYM           {}
        | OWNER_SYM             {}
        | PARSER_SYM            {}
        | PORT_SYM              {}
        | PRECEDES_SYM          {}
        | PREPARE_SYM           {}
        | REMOVE_SYM            {}
        | REPAIR                {}
        | RESET_SYM             {}
        | RESTORE_SYM           {}
        | ROLE_SYM              {} 
        | ROLLBACK_SYM          {}
        | SAVEPOINT_SYM         {}
        | SECURITY_SYM          {}
        | SERVER_SYM            {}
        | SIGNED_SYM            {}
        | SOCKET_SYM            {}
        | SLAVE                 {}
        | SONAME_SYM            {}
        | START_SYM             {}
        | STOP_SYM              {}
        | TRUNCATE_SYM          {}
        | VISIBLE_SYM           {}
        | UNICODE_SYM           {}
        | UNINSTALL_SYM         {}
        | WRAPPER_SYM           {}
        | XA_SYM                {}
        | UPGRADE_SYM           {}
        ;

/*
  Keywords that we allow for labels in SPs.
  Anything that's the beginning of a statement or characteristics
  must be in keyword above, otherwise we get (harmful) shift/reduce
  conflicts.
*/
label_keyword:
          role_or_label_keyword    {}
        | EVENT_SYM                {}
        | FILE_SYM                 {}
        | NONE_SYM                 {}
        | PROCESS                  {}
        | PROXY_SYM                {}
        | RELOAD                   {}
        | REPLICATION              {}
        | SUPER_SYM                {}
        ;

// These are the non-reserved keywords which may be used for roles or SP labels.
role_or_label_keyword:
          ACTION                   {}
        | ADDDATE_SYM              {}
        | AFTER_SYM                {}
        | AGAINST                  {}
        | AGGREGATE_SYM            {}
        | ALGORITHM_SYM            {}
        | ANY_SYM                  {}
        | AT_SYM                   {}
        | AUTO_INC                 {}
        | AUTOEXTEND_SIZE_SYM      {}
        | AVG_ROW_LENGTH           {}
        | AVG_SYM                  {}
        | BINLOG_SYM               {}
        | BIT_SYM                  {}
        | BLOCK_SYM                {}
        | BOOL_SYM                 {}
        | BOOLEAN_SYM              {}
        | BTREE_SYM                {}
        | CASCADED                 {}
        | CATALOG_NAME_SYM         {}
        | CHAIN_SYM                {}
        | CHANGED                  {}
        | CHANNEL_SYM              {}
        | CIPHER_SYM               {}
        | CLIENT_SYM               {}
        | CLASS_ORIGIN_SYM         {}
        | COALESCE                 {}
        | CODE_SYM                 {}
        | COLLATION_SYM            {}
        | COLUMN_NAME_SYM          {}
        | COLUMN_FORMAT_SYM        {}
        | COLUMNS                  {}
        | COMMITTED_SYM            {}
        | COMPACT_SYM              {}
        | COMPLETION_SYM           {}
        | COMPONENT_SYM            {}
        | COMPRESSED_SYM           {}
        | COMPRESSION_SYM          {}
        | ENCRYPTION_SYM           {}
        | CONCURRENT               {}
        | CONNECTION_SYM           {}
        | CONSISTENT_SYM           {}
        | CONSTRAINT_CATALOG_SYM   {}
        | CONSTRAINT_SCHEMA_SYM    {}
        | CONSTRAINT_NAME_SYM      {}
        | CONTEXT_SYM              {}
        | CPU_SYM                  {}
        /*
          Although a reserved keyword in SQL:2003 (and :2008),
          not reserved in MySQL per WL#2111 specification.
        */
        | CURRENT_SYM              {}
        | CURSOR_NAME_SYM          {}
        | DATA_SYM                 {}
        | DATAFILE_SYM             {}
        | DATETIME_SYM             {}
        | DATE_SYM                 {}
        | DAY_SYM                  {}
        | DEFAULT_AUTH_SYM         {}
        | DEFINER_SYM              {}
        | DELAY_KEY_WRITE_SYM      {}
        | DES_KEY_FILE             {}
        | DIAGNOSTICS_SYM          {}
        | DIRECTORY_SYM            {}
        | DISABLE_SYM              {}
        | DISCARD                  {}
        | DISK_SYM                 {}
        | DUMPFILE                 {}
        | DUPLICATE_SYM            {}
        | DYNAMIC_SYM              {}
        | ENDS_SYM                 {}
        | ENUM_SYM                 {}
        | ENGINE_SYM               {}
        | ENGINES_SYM              {}
        | ERROR_SYM                {}
        | ERRORS                   {}
        | ESCAPE_SYM               {}
        | EVENTS_SYM               {}
        | EVERY_SYM                {}
        | EXCHANGE_SYM             {}
        | EXPANSION_SYM            {}
        | EXPIRE_SYM               {}
        | EXPORT_SYM               {}
        | EXTENDED_SYM             {}
        | EXTENT_SIZE_SYM          {}
        | FAULTS_SYM               {}
        | FAST_SYM                 {}
        | FOUND_SYM                {}
        | ENABLE_SYM               {}
        | FULL                     {}
        | FILE_BLOCK_SIZE_SYM      {}
        | FILTER_SYM               {}
        | FIRST_SYM                {}
        | FIXED_SYM                {}
        | GENERAL                  {}
        | GEOMETRY_SYM             {}
        | GEOMETRYCOLLECTION_SYM   {}
        | GET_FORMAT               {}
        | GRANTS                   {}
        | GLOBAL_SYM               {}
        | HASH_SYM                 {}
        | HOSTS_SYM                {}
        | HOUR_SYM                 {}
        | IDENTIFIED_SYM           {}
        | IGNORE_SERVER_IDS_SYM    {}
        | INVOKER_SYM              {}
        | INDEXES                  {}
        | INITIAL_SIZE_SYM         {}
        | IO_SYM                   {}
        | IPC_SYM                  {}
        | ISOLATION                {}
        | ISSUER_SYM               {}
        | INSERT_METHOD            {}
        | INSTANCE_SYM             {}
        | JSON_SYM                 {}
        | KEY_BLOCK_SIZE           {}
        | LAST_SYM                 {}
        | LEAVES                   {}
        | LESS_SYM                 {}
        | LEVEL_SYM                {}
        | LINESTRING_SYM           {}
        | LIST_SYM                 {}
        | LOCAL_SYM                {}
        | LOCKED_SYM               {}
        | LOCKS_SYM                {}
        | LOGFILE_SYM              {}
        | LOGS_SYM                 {}
        | MAX_ROWS                 {}
        | MASTER_SYM               {}
        | MASTER_HEARTBEAT_PERIOD_SYM {}
        | MASTER_HOST_SYM          {}
        | MASTER_PORT_SYM          {}
        | MASTER_LOG_FILE_SYM      {}
        | MASTER_LOG_POS_SYM       {}
        | MASTER_USER_SYM          {}
        | MASTER_PASSWORD_SYM      {}
        | MASTER_SERVER_ID_SYM     {}
        | MASTER_CONNECT_RETRY_SYM {}
        | MASTER_RETRY_COUNT_SYM   {}
        | MASTER_DELAY_SYM         {}
        | MASTER_SSL_SYM           {}
        | MASTER_SSL_CA_SYM        {}
        | MASTER_SSL_CAPATH_SYM    {}
        | MASTER_TLS_VERSION_SYM   {}
        | MASTER_SSL_CERT_SYM      {}
        | MASTER_SSL_CIPHER_SYM    {}
        | MASTER_SSL_CRL_SYM       {}
        | MASTER_SSL_CRLPATH_SYM   {}
        | MASTER_SSL_KEY_SYM       {}
        | MASTER_AUTO_POSITION_SYM {}
        | MAX_CONNECTIONS_PER_HOUR {}
        | MAX_QUERIES_PER_HOUR     {}
        | MAX_SIZE_SYM             {}
        | MAX_UPDATES_PER_HOUR     {}
        | MAX_USER_CONNECTIONS_SYM {}
        | MEDIUM_SYM               {}
        | MEMORY_SYM               {}
        | MERGE_SYM                {}
        | MESSAGE_TEXT_SYM         {}
        | MICROSECOND_SYM          {}
        | MIGRATE_SYM              {}
        | MINUTE_SYM               {}
        | MIN_ROWS                 {}
        | MODIFY_SYM               {}
        | MODE_SYM                 {}
        | MONTH_SYM                {}
        | MULTILINESTRING_SYM      {}
        | MULTIPOINT_SYM           {}
        | MULTIPOLYGON_SYM         {}
        | MUTEX_SYM                {}
        | MYSQL_ERRNO_SYM          {}
        | NAME_SYM                 {}
        | NAMES_SYM                {}
        | NATIONAL_SYM             {}
        | NCHAR_SYM                {}
        | NDBCLUSTER_SYM           {}
        | NEVER_SYM                {}
        | NEXT_SYM                 {}
        | NEW_SYM                  {}
        | NO_WAIT_SYM              {}
        | NODEGROUP_SYM            {}
        | NOWAIT_SYM               {}
        | NUMBER_SYM               {}
        | NVARCHAR_SYM             {}
        | OFFSET_SYM               {}
        | ONE_SYM                  {}
        | ONLY_SYM                 {}
        | PACK_KEYS_SYM            {}
        | PAGE_SYM                 {}
        | PARTIAL                  {}
        | PARTITIONING_SYM         {}
        | PARTITIONS_SYM           {}
        | PASSWORD                 {}
        | PHASE_SYM                {}
        | PLUGIN_DIR_SYM           {}
        | PLUGIN_SYM               {}
        | PLUGINS_SYM              {}
        | POINT_SYM                {}
        | POLYGON_SYM              {}
        | PRESERVE_SYM             {}
        | PREV_SYM                 {}
        | PRIVILEGES               {}
        | PROCESSLIST_SYM          {}
        | PROFILE_SYM              {}
        | PROFILES_SYM             {}
        | QUARTER_SYM              {}
        | QUERY_SYM                {}
        | QUICK                    {}
        | READ_ONLY_SYM            {}
        | REBUILD_SYM              {}
        | RECOVER_SYM              {}
        | REDO_BUFFER_SIZE_SYM     {}
        | REDOFILE_SYM             {}
        | REDUNDANT_SYM            {}
        | RELAY                    {}
        | RELAYLOG_SYM             {}
        | RELAY_LOG_FILE_SYM       {}
        | RELAY_LOG_POS_SYM        {}
        | RELAY_THREAD             {}
        | REORGANIZE_SYM           {}
        | REPEATABLE_SYM           {}
        | REPLICATE_DO_DB          {}
        | REPLICATE_IGNORE_DB      {}
        | REPLICATE_DO_TABLE       {}
        | REPLICATE_IGNORE_TABLE   {}
        | REPLICATE_WILD_DO_TABLE  {}
        | REPLICATE_WILD_IGNORE_TABLE {}
        | REPLICATE_REWRITE_DB     {}
        | RESOURCES                {}
        | RESUME_SYM               {}
        | RETURNED_SQLSTATE_SYM    {}
        | RETURNS_SYM              {}
        | REVERSE_SYM              {}
        | ROLLUP_SYM               {}
        | ROTATE_SYM               {}
        | ROUTINE_SYM              {}
        | ROWS_SYM                 {}
        | ROW_COUNT_SYM            {}
        | ROW_FORMAT_SYM           {}
        | ROW_SYM                  {}
        | RTREE_SYM                {}
        | SCHEDULE_SYM             {}
        | SCHEMA_NAME_SYM          {}
        | SECOND_SYM               {}
        | SERIAL_SYM               {}
        | SERIALIZABLE_SYM         {}
        | SESSION_SYM              {}
        | SHARE_SYM                {}
        | SIMPLE_SYM               {}
        | SKIP_SYM                 {}
        | SLOW                     {}
        | SNAPSHOT_SYM             {}
        | SOUNDS_SYM               {}
        | SOURCE_SYM               {}
        | SQL_AFTER_GTIDS          {}
        | SQL_AFTER_MTS_GAPS       {}
        | SQL_BEFORE_GTIDS         {}
        | SQL_CACHE_SYM            {}
        | SQL_BUFFER_RESULT        {}
        | SQL_NO_CACHE_SYM         {}
        | SQL_THREAD               {}
        | STACKED_SYM              {}
        | STARTS_SYM               {}
        | STATS_AUTO_RECALC_SYM    {}
        | STATS_PERSISTENT_SYM     {}
        | STATS_SAMPLE_PAGES_SYM   {}
        | STATUS_SYM               {}
        | STORAGE_SYM              {}
        | STRING_SYM               {}
        | SUBCLASS_ORIGIN_SYM      {}
        | SUBDATE_SYM              {}
        | SUBJECT_SYM              {}
        | SUBPARTITION_SYM         {}
        | SUBPARTITIONS_SYM        {}
        | SUSPEND_SYM              {}
        | SWAPS_SYM                {}
        | SWITCHES_SYM             {}
        | TABLE_NAME_SYM           {}
        | TABLES                   {}
        | TABLE_CHECKSUM_SYM       {}
        | TABLESPACE_SYM           {}
        | TEMPORARY                {}
        | TEMPTABLE_SYM            {}
        | TEXT_SYM                 {}
        | THAN_SYM                 {}
        | TRANSACTION_SYM          {}
        | TRIGGERS_SYM             {}
        | TIMESTAMP_SYM            {}
        | TIMESTAMP_ADD            {}
        | TIMESTAMP_DIFF           {}
        | TIME_SYM                 {}
        | TYPES_SYM                {}
        | TYPE_SYM                 {}
        | UDF_RETURNS_SYM          {}
        | UNCOMMITTED_SYM          {}
        | UNDEFINED_SYM            {}
        | UNDO_BUFFER_SIZE_SYM     {}
        | UNDOFILE_SYM             {}
        | UNKNOWN_SYM              {}
        | UNTIL_SYM                {}
        | USER                     {}
        | USE_FRM                  {}
        | VALIDATION_SYM           {}
        | VARIABLES                {}
        | VIEW_SYM                 {}
        | VALUE_SYM                {}
        | WARNINGS                 {}
        | WAIT_SYM                 {}
        | WEEK_SYM                 {}
        | WITHOUT_SYM              {}
        | WORK_SYM                 {}
        | WEIGHT_STRING_SYM        {}
        | X509_SYM                 {}
        | XID_SYM                  {}
        | XML_SYM                  {}
        | YEAR_SYM                 {}
        ;

/*
  Non-reserved keywords that we allow for role names.

  In order not to introduce new grammar conflicts, the following keyword tokens are
  not welcome as role names:

    EVENT_SYM
    EXECUTE_SYM
    FILE_SYM
    PROCESS
    PROXY_SYM
    RELOAD
    REPLICATION
    SHUTDOWN
    SUPER_SYM
*/
role_keyword:
          role_or_label_keyword
        | role_or_ident_keyword
        ;

/*
  SQLCOM_SET_OPTION statement.

  Note that to avoid shift/reduce conflicts, we have separate rules for the
  first option listed in the statement.
*/

set:
          SET_SYM start_option_value_list
          {
            $$= NEW_PTN PT_set(@1, $2);
          }
        ;


// Start of option value list
start_option_value_list:
          option_value_no_option_type option_value_list_continued
          {
            $$= NEW_PTN PT_start_option_value_list_no_type($1, @1, $2);
          }
        | TRANSACTION_SYM transaction_characteristics
          {
            $$= NEW_PTN PT_start_option_value_list_transaction($2, @2);
          }
        | option_type start_option_value_list_following_option_type
          {
            $$= NEW_PTN PT_start_option_value_list_type($1, $2);
          }
        | PASSWORD equal password
          {
            $$= NEW_PTN PT_option_value_no_option_type_password($3, @3);
          }
        | PASSWORD equal PASSWORD '(' password ')'
          {
            push_deprecated_warn(YYTHD, "SET PASSWORD = "
                                 "PASSWORD('<plaintext_password>')",
                                 "SET PASSWORD = '<plaintext_password>'");
            $$= NEW_PTN PT_option_value_no_option_type_password($5, @5);
          }
        | PASSWORD FOR_SYM user equal password
          {
            $$= NEW_PTN PT_option_value_no_option_type_password_for($3, $5, @5);
          }
        | PASSWORD FOR_SYM user equal PASSWORD '(' password ')'
          {
            push_deprecated_warn(YYTHD, "SET PASSWORD FOR <user> = "
                                 "PASSWORD('<plaintext_password>')",
                                 "SET PASSWORD FOR <user> = "
                                 "'<plaintext_password>'");
            $$= NEW_PTN PT_option_value_no_option_type_password_for($3, $7, @7);
          }
        ;

set_role_stmt:
          SET_SYM ROLE_SYM role_list
          {
            $$= NEW_PTN PT_set_role($3);
            Lex->sql_command= SQLCOM_SET_ROLE;
          }
        | SET_SYM ROLE_SYM NONE_SYM
          {
            $$= NEW_PTN PT_set_role(ROLE_NONE);
            Lex->sql_command= SQLCOM_SET_ROLE;
          }
        | SET_SYM ROLE_SYM DEFAULT_SYM
          {
            $$= NEW_PTN PT_set_role(ROLE_DEFAULT);
            Lex->sql_command= SQLCOM_SET_ROLE;
          }
        | SET_SYM DEFAULT_SYM ROLE_SYM role_list TO_SYM role_list
          {
            $$= NEW_PTN PT_alter_user_default_role(false, $6, $4, ROLE_NAME);
            Lex->sql_command= SQLCOM_ALTER_USER_DEFAULT_ROLE;
          }
        | SET_SYM DEFAULT_SYM ROLE_SYM NONE_SYM TO_SYM role_list
          {
            $$= NEW_PTN PT_alter_user_default_role(false, $6, NULL, ROLE_NONE);
            Lex->sql_command= SQLCOM_ALTER_USER_DEFAULT_ROLE;
          }
        | SET_SYM DEFAULT_SYM ROLE_SYM ALL TO_SYM role_list
          {
            $$= NEW_PTN PT_alter_user_default_role(false, $6, NULL, ROLE_ALL);
            Lex->sql_command= SQLCOM_ALTER_USER_DEFAULT_ROLE;
          }
        | SET_SYM ROLE_SYM ALL opt_except_role_list
          {
            $$= NEW_PTN PT_set_role(ROLE_ALL, $4);
            Lex->sql_command= SQLCOM_SET_ROLE;
          }
        ;

opt_except_role_list:
          /* empty */          { $$= NULL; }
        | EXCEPT_SYM role_list { $$= $2; }
        ;

// Start of option value list, option_type was given
start_option_value_list_following_option_type:
          option_value_following_option_type option_value_list_continued
          {
            $$=
              NEW_PTN PT_start_option_value_list_following_option_type_eq($1,
                                                                          @1,
                                                                          $2);
          }
        | TRANSACTION_SYM transaction_characteristics
          {
            $$= NEW_PTN
              PT_start_option_value_list_following_option_type_transaction($2,
                                                                           @2);
          }
        ;

// Remainder of the option value list after first option value.
option_value_list_continued:
          /* empty */           { $$= NULL; }
        | ',' option_value_list { $$= $2; }
        ;

// Repeating list of option values after first option value.
option_value_list:
          option_value
          {
            $$= NEW_PTN PT_option_value_list_head(@0, $1, @1);
          }
        | option_value_list ',' option_value
          {
            $$= NEW_PTN PT_option_value_list($1, @2, $3, @3);
          }
        ;

// Wrapper around option values following the first option value in the stmt.
option_value:
          option_type option_value_following_option_type
          {
            $$= NEW_PTN PT_option_value_type($1, $2);
          }
        | option_value_no_option_type { $$= $1; }
        ;

option_type:
          GLOBAL_SYM  { $$=OPT_GLOBAL; }
        | PERSIST_SYM { $$=OPT_PERSIST; }
        | LOCAL_SYM   { $$=OPT_SESSION; }
        | SESSION_SYM { $$=OPT_SESSION; }
        ;

opt_var_type:
          /* empty */ { $$=OPT_SESSION; }
        | GLOBAL_SYM  { $$=OPT_GLOBAL; }
        | LOCAL_SYM   { $$=OPT_SESSION; }
        | SESSION_SYM { $$=OPT_SESSION; }
        ;

opt_var_ident_type:
          /* empty */     { $$=OPT_DEFAULT; }
        | GLOBAL_SYM '.'  { $$=OPT_GLOBAL; }
        | LOCAL_SYM '.'   { $$=OPT_SESSION; }
        | SESSION_SYM '.' { $$=OPT_SESSION; }
        ;

opt_set_var_ident_type:
          /* empty */     { $$=OPT_DEFAULT; }
        | PERSIST_SYM '.' { $$=OPT_PERSIST; }
        | GLOBAL_SYM '.'  { $$=OPT_GLOBAL; }
        | LOCAL_SYM '.'   { $$=OPT_SESSION; }
        | SESSION_SYM '.' { $$=OPT_SESSION; }
         ;

// Option values with preceding option_type.
option_value_following_option_type:
          internal_variable_name equal set_expr_or_default
          {
            $$= NEW_PTN PT_option_value_following_option_type(@$, $1, $3);
          }
        ;

// Option values without preceding option_type.
option_value_no_option_type:
          internal_variable_name        /*$1*/
          equal                         /*$2*/
          set_expr_or_default           /*$3*/
          {
            $$= NEW_PTN PT_option_value_no_option_type_internal($1, $3, @3);
          }
        | '@' ident_or_text equal expr
          {
            $$= NEW_PTN PT_option_value_no_option_type_user_var($2, $4);
          }
        | '@' '@' opt_set_var_ident_type internal_variable_name equal
          set_expr_or_default
          {
            $$= NEW_PTN PT_option_value_no_option_type_sys_var($3, $4, $6);
          }
        | charset old_or_new_charset_name_or_default
          {
            $$= NEW_PTN PT_option_value_no_option_type_charset($2);
          }
        | NAMES_SYM equal expr
          {
            /*
              Bad syntax, always fails with an error
            */
            $$= NEW_PTN PT_option_value_no_option_type_names(@2);
          }
        | NAMES_SYM charset_name_or_default opt_collate
          {
            $$= NEW_PTN PT_option_value_no_option_type_names_charset($2, $3);
          }
        ;

internal_variable_name:
          ident
          {
            $$= NEW_PTN PT_internal_variable_name_1d($1);
          }
        | ident '.' ident
          {
            $$= NEW_PTN PT_internal_variable_name_2d(@$, $1, $3);
          }
        | DEFAULT_SYM '.' ident
          {
            $$= NEW_PTN PT_internal_variable_name_default($3);
          }
        ;

transaction_characteristics:
          transaction_access_mode opt_isolation_level
          {
            $$= NEW_PTN PT_transaction_characteristics($1, $2);
          }
        | isolation_level opt_transaction_access_mode
          {
            $$= NEW_PTN PT_transaction_characteristics($1, $2);
          }
        ;

transaction_access_mode:
          transaction_access_mode_types
          {
            $$= NEW_PTN PT_transaction_access_mode($1);
          }
        ;

opt_transaction_access_mode:
          /* empty */                 { $$= NULL; }
        | ',' transaction_access_mode { $$= $2; }
        ;

isolation_level:
          ISOLATION LEVEL_SYM isolation_types
          {
            $$= NEW_PTN PT_isolation_level($3);
          }
        ;

opt_isolation_level:
          /* empty */         { $$= NULL; }
        | ',' isolation_level { $$= $2; }
        ;

transaction_access_mode_types:
          READ_SYM ONLY_SYM { $$= true; }
        | READ_SYM WRITE_SYM { $$= false; }
        ;

isolation_types:
          READ_SYM UNCOMMITTED_SYM { $$= ISO_READ_UNCOMMITTED; }
        | READ_SYM COMMITTED_SYM   { $$= ISO_READ_COMMITTED; }
        | REPEATABLE_SYM READ_SYM  { $$= ISO_REPEATABLE_READ; }
        | SERIALIZABLE_SYM         { $$= ISO_SERIALIZABLE; }
        ;

password:
          TEXT_STRING
          {
            $$=$1.str;
            Lex->contains_plaintext_password= true;
          }
        ;


set_expr_or_default:
          expr
        | DEFAULT_SYM { $$= NULL; }
        | ON_SYM
          {
            $$= NEW_PTN Item_string(@$, "ON",  2, system_charset_info);
          }
        | ALL
          {
            $$= NEW_PTN Item_string(@$, "ALL", 3, system_charset_info);
          }
        | BINARY_SYM
          {
            $$= NEW_PTN Item_string(@$, "binary", 6, system_charset_info);
          }
        ;

/* Lock function */

lock:
          LOCK_SYM table_or_tables
          {
            LEX *lex= Lex;

            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "LOCK");
              MYSQL_YYABORT;
            }
            lex->sql_command= SQLCOM_LOCK_TABLES;
          }
          table_lock_list
          {}
        ;

table_or_tables:
          TABLE_SYM
        | TABLES
        ;

table_lock_list:
          table_lock
        | table_lock_list ',' table_lock
        ;

table_lock:
          table_ident opt_table_alias lock_option
          {
            thr_lock_type lock_type= (thr_lock_type) $3;
            enum_mdl_type mdl_lock_type;

            if (lock_type >= TL_WRITE_ALLOW_WRITE)
            {
              /* LOCK TABLE ... WRITE/LOW_PRIORITY WRITE */
              mdl_lock_type= MDL_SHARED_NO_READ_WRITE;
            }
            else if (lock_type == TL_READ)
            {
              /* LOCK TABLE ... READ LOCAL */
              mdl_lock_type= MDL_SHARED_READ;
            }
            else
            {
              /* LOCK TABLE ... READ */
              mdl_lock_type= MDL_SHARED_READ_ONLY;
            }

            if (!Select->add_table_to_list(YYTHD, $1, $2, 0, lock_type,
                                           mdl_lock_type))
              MYSQL_YYABORT;
          }
        ;

lock_option:
          READ_SYM               { $$= TL_READ_NO_INSERT; }
        | WRITE_SYM              { $$= TL_WRITE_DEFAULT; }
        | LOW_PRIORITY WRITE_SYM
          {
            $$= TL_WRITE_LOW_PRIORITY;
            push_deprecated_warn(YYTHD, "LOW_PRIORITY WRITE", "WRITE");
          }
        | READ_SYM LOCAL_SYM     { $$= TL_READ; }
        ;

unlock:
          UNLOCK_SYM
          {
            LEX *lex= Lex;

            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "UNLOCK");
              MYSQL_YYABORT;
            }
            lex->sql_command= SQLCOM_UNLOCK_TABLES;
          }
          table_or_tables
          {}
        ;


shutdown_stmt:
          SHUTDOWN
          {
            Lex->sql_command= SQLCOM_SHUTDOWN;
            $$= NEW_PTN PT_shutdown();
          }
        ;

alter_instance_stmt:
          ALTER INSTANCE_SYM alter_instance_action
          {
            Lex->sql_command= SQLCOM_ALTER_INSTANCE;
            $$= NEW_PTN PT_alter_instance($3);
          }

alter_instance_action:
          ROTATE_SYM ident_or_text MASTER_SYM KEY_SYM
          {
            if (!my_strcasecmp(system_charset_info, $2.str, "INNODB"))
            {
              $$= ROTATE_INNODB_MASTER_KEY;
            }
            else
            {
              YYTHD->syntax_error_at(@2, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
          }
        ;

/*
** Handler: direct access to ISAM functions
*/

handler:
          HANDLER_SYM table_ident OPEN_SYM opt_table_alias
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "HANDLER");
              MYSQL_YYABORT;
            }
            lex->sql_command = SQLCOM_HA_OPEN;
            if (!lex->current_select()->add_table_to_list(thd, $2, $4, 0))
              MYSQL_YYABORT;
            lex->m_sql_cmd= NEW_PTN Sql_cmd_handler_open();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | HANDLER_SYM table_ident_nodb CLOSE_SYM
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "HANDLER");
              MYSQL_YYABORT;
            }
            lex->sql_command = SQLCOM_HA_CLOSE;
            if (!lex->current_select()->add_table_to_list(thd, $2, 0, 0))
              MYSQL_YYABORT;
            lex->m_sql_cmd= NEW_PTN Sql_cmd_handler_close();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | HANDLER_SYM           /* #1 */
          table_ident_nodb      /* #2 */
          READ_SYM              /* #3 */
          {                     /* #4 */
            LEX *lex=Lex;
            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "HANDLER");
              MYSQL_YYABORT;
            }
            lex->expr_allows_subselect= FALSE;
            lex->sql_command = SQLCOM_HA_READ;
            Item *one= NEW_PTN Item_int((int32) 1);
            if (one == NULL)
              MYSQL_YYABORT;
            lex->current_select()->select_limit= one;
            lex->current_select()->offset_limit= 0;
            if (!lex->current_select()->add_table_to_list(lex->thd, $2, 0, 0))
              MYSQL_YYABORT;
          }
          handler_read_or_scan  /* #5 */
          opt_where_clause      /* #6 */
          opt_limit_clause      /* #7 */
          {
            if ($6 != NULL)
              ITEMIZE($6, &$6);
            Select->set_where_cond($6);

            if ($7 != NULL)
              CONTEXTUALIZE($7);

            THD *thd= YYTHD;
            LEX *lex= Lex;
            Lex->expr_allows_subselect= TRUE;
            /* Stored functions are not supported for HANDLER READ. */
            if (lex->uses_stored_routines())
            {
              my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                       "stored functions in HANDLER ... READ");
              MYSQL_YYABORT;
            }
            lex->m_sql_cmd= NEW_PTN Sql_cmd_handler_read($5,
                                  lex->ident.str, lex->handler_insert_list,
                                  thd->m_parser_state->m_yacc.m_ha_rkey_mode);
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

handler_read_or_scan:
          handler_scan_function       { Lex->ident= null_lex_str; $$=$1; }
        | ident handler_rkey_function { Lex->ident= $1; $$=$2; }
        ;

handler_scan_function:
          FIRST_SYM { $$= enum_ha_read_modes::RFIRST; }
        | NEXT_SYM  { $$= enum_ha_read_modes::RNEXT;  }
        ;

handler_rkey_function:
          FIRST_SYM { $$= enum_ha_read_modes::RFIRST; }
        | NEXT_SYM  { $$= enum_ha_read_modes::RNEXT;  }
        | PREV_SYM  { $$= enum_ha_read_modes::RPREV;  }
        | LAST_SYM  { $$= enum_ha_read_modes::RLAST;  }
        | handler_rkey_mode
          {
            YYTHD->m_parser_state->m_yacc.m_ha_rkey_mode= $1;
          }
          '(' values ')'
          {
            CONTEXTUALIZE($4);
            Lex->handler_insert_list= &$4->value;
            $$= enum_ha_read_modes::RKEY;
          }
        ;

handler_rkey_mode:
          EQ     { $$=HA_READ_KEY_EXACT;   }
        | GE     { $$=HA_READ_KEY_OR_NEXT; }
        | LE     { $$=HA_READ_KEY_OR_PREV; }
        | GT_SYM { $$=HA_READ_AFTER_KEY;   }
        | LT     { $$=HA_READ_BEFORE_KEY;  }
        ;

/* GRANT / REVOKE */

revoke:
          REVOKE role_or_privilege_list FROM user_list
          {
            Lex->sql_command= SQLCOM_REVOKE_ROLE;
            PT_statement *tmp= NEW_PTN PT_revoke_roles($2, $4);
            MAKE_CMD(tmp);
          }
        | REVOKE role_or_privilege_list ON_SYM opt_acl_type grant_ident FROM user_list
          {
            LEX *lex= Lex;
            if (apply_privileges(YYTHD, *$2))
              MYSQL_YYABORT;
            lex->sql_command= (lex->grant == GLOBAL_ACLS) ? SQLCOM_REVOKE_ALL
                                                          : SQLCOM_REVOKE;
            if ($4 != Acl_type::TABLE && !lex->columns.is_empty())
            {
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            lex->type= static_cast<ulong>($4);
            lex->users_list= *$7;
          }
        | REVOKE ALL opt_privileges
          {
            Lex->all_privileges= 1;
            Lex->grant= GLOBAL_ACLS;
          }
          ON_SYM opt_acl_type grant_ident FROM user_list
          {
            LEX *lex= Lex;
            lex->sql_command= (lex->grant == (GLOBAL_ACLS & ~GRANT_ACL)) ?
                                                            SQLCOM_REVOKE_ALL
                                                          : SQLCOM_REVOKE;
            if ($6 != Acl_type::TABLE && !lex->columns.is_empty())
            {
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            lex->type= static_cast<ulong>($6);
            lex->users_list= *$9;
          }
        | REVOKE ALL opt_privileges ',' GRANT OPTION FROM user_list
          {
            Lex->sql_command = SQLCOM_REVOKE_ALL;
            Lex->users_list= *$8;
          }
        | REVOKE PROXY_SYM ON_SYM user FROM user_list
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_REVOKE;
            lex->users_list= *$6;
            lex->users_list.push_front ($4);
            lex->type= TYPE_ENUM_PROXY;
          }
        ;

grant:
          GRANT role_or_privilege_list TO_SYM user_list opt_with_admin_option
          {
            Lex->sql_command= SQLCOM_GRANT_ROLE;
            PT_statement *tmp= NEW_PTN PT_grant_roles($2, $4, $5);
            MAKE_CMD(tmp);
          }
        | GRANT role_or_privilege_list ON_SYM opt_acl_type grant_ident TO_SYM grant_list
          require_clause grant_options
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_GRANT;
            if (apply_privileges(YYTHD, *$2))
              MYSQL_YYABORT;

            if ($4 != Acl_type::TABLE && !lex->columns.is_empty())
            {
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            lex->type= static_cast<ulong>($4);
          }
        | GRANT ALL opt_privileges
          {
            Lex->all_privileges= 1;
            Lex->grant= GLOBAL_ACLS;
          }
          ON_SYM opt_acl_type grant_ident TO_SYM grant_list
          require_clause grant_options
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_GRANT;
            if ($6 != Acl_type::TABLE && !lex->columns.is_empty())
            {
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            lex->type= static_cast<ulong>($6);
          }
        | GRANT PROXY_SYM ON_SYM user TO_SYM grant_list opt_grant_option
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_GRANT;
            if ($7)
              lex->grant |= GRANT_ACL;
            lex->users_list.push_front ($4);
            lex->type= TYPE_ENUM_PROXY;
          }
        ;

opt_acl_type:
          /* Empty */   { $$= Acl_type::TABLE; }
        | TABLE_SYM     { $$= Acl_type::TABLE; }
        | FUNCTION_SYM  { $$= Acl_type::FUNCTION; }
        | PROCEDURE_SYM { $$= Acl_type::PROCEDURE; }
        ;

opt_privileges:
          /* empty */
        | PRIVILEGES
        ;

role_or_privilege_list:
          role_or_privilege
          {
            $$= NEW_PTN Trivial_array<PT_role_or_privilege *>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | role_or_privilege_list ',' role_or_privilege
          {
            $$= $1;
            if ($$->push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

role_or_privilege:
          role_ident_or_text opt_column_list
          {
            if ($2 == NULL)
              $$= NEW_PTN PT_role_or_dynamic_privilege(@1, $1);
            else
              $$= NEW_PTN PT_dynamic_privilege(@1, $1, $2);
          }
        | role_ident_or_text '@' ident_or_text
          { $$= NEW_PTN PT_role_at_host(@1, $1, $3); }
        | SELECT_SYM opt_column_list
          { $$= NEW_PTN PT_static_privilege(@1, SELECT_ACL, $2); }
        | INSERT_SYM opt_column_list
          { $$= NEW_PTN PT_static_privilege(@1, INSERT_ACL, $2); }
        | UPDATE_SYM opt_column_list
          { $$= NEW_PTN PT_static_privilege(@1, UPDATE_ACL, $2); }
        | REFERENCES opt_column_list
          { $$= NEW_PTN PT_static_privilege(@1, REFERENCES_ACL, $2); }
        | DELETE_SYM
          { $$= NEW_PTN PT_static_privilege(@1, DELETE_ACL); }
        | USAGE
          { $$= NEW_PTN PT_static_privilege(@1, 0); }
        | INDEX_SYM
          { $$= NEW_PTN PT_static_privilege(@1, INDEX_ACL); }
        | ALTER
          { $$= NEW_PTN PT_static_privilege(@1, ALTER_ACL); }
        | CREATE
          { $$= NEW_PTN PT_static_privilege(@1, CREATE_ACL); }
        | DROP
          { $$= NEW_PTN PT_static_privilege(@1, DROP_ACL); }
        | EXECUTE_SYM
          { $$= NEW_PTN PT_static_privilege(@1, EXECUTE_ACL); }
        | RELOAD
          { $$= NEW_PTN PT_static_privilege(@1, RELOAD_ACL); }
        | SHUTDOWN
          { $$= NEW_PTN PT_static_privilege(@1, SHUTDOWN_ACL); }
        | PROCESS
          { $$= NEW_PTN PT_static_privilege(@1, PROCESS_ACL); }
        | FILE_SYM
          { $$= NEW_PTN PT_static_privilege(@1, FILE_ACL); }
        | GRANT OPTION
          { $$= NEW_PTN PT_static_privilege(@1, GRANT_ACL); }
        | SHOW DATABASES
          { $$= NEW_PTN PT_static_privilege(@1, SHOW_DB_ACL); }
        | SUPER_SYM
          { 
            /* DEPRECATED */
            $$= NEW_PTN PT_static_privilege(@1, SUPER_ACL);
            if (Lex->grant != GLOBAL_ACLS)
            {
              /*
                 An explicit request was made for the SUPER priv id
              */
              push_warning(Lex->thd, Sql_condition::SL_WARNING,
                           ER_WARN_DEPRECATED_SYNTAX,
                           "The SUPER privilege identifier is deprecated");
            }
          }
        | CREATE TEMPORARY TABLES
          { $$= NEW_PTN PT_static_privilege(@1, CREATE_TMP_ACL); }
        | LOCK_SYM TABLES
          { $$= NEW_PTN PT_static_privilege(@1, LOCK_TABLES_ACL); }
        | REPLICATION SLAVE
          { $$= NEW_PTN PT_static_privilege(@1, REPL_SLAVE_ACL); }
        | REPLICATION CLIENT_SYM
          { $$= NEW_PTN PT_static_privilege(@1, REPL_CLIENT_ACL); }
        | CREATE VIEW_SYM
          { $$= NEW_PTN PT_static_privilege(@1, CREATE_VIEW_ACL); }
        | SHOW VIEW_SYM
          { $$= NEW_PTN PT_static_privilege(@1, SHOW_VIEW_ACL); }
        | CREATE ROUTINE_SYM
          { $$= NEW_PTN PT_static_privilege(@1, CREATE_PROC_ACL); }
        | ALTER ROUTINE_SYM
          { $$= NEW_PTN PT_static_privilege(@1, ALTER_PROC_ACL); }
        | CREATE USER
          { $$= NEW_PTN PT_static_privilege(@1, CREATE_USER_ACL); }
        | EVENT_SYM
          { $$= NEW_PTN PT_static_privilege(@1, EVENT_ACL); }
        | TRIGGER_SYM
          { $$= NEW_PTN PT_static_privilege(@1, TRIGGER_ACL); }
        | CREATE TABLESPACE_SYM
          { $$= NEW_PTN PT_static_privilege(@1, CREATE_TABLESPACE_ACL); }
        | CREATE ROLE_SYM
          { $$= NEW_PTN PT_static_privilege(@1, CREATE_ROLE_ACL); }
        | DROP ROLE_SYM
          { $$= NEW_PTN PT_static_privilege(@1, DROP_ROLE_ACL); }
        ;

opt_with_admin_option:
          /* empty */           { $$= false; }
        | WITH ADMIN_SYM OPTION { $$= true; }
        ;

opt_and:
          /* empty */ {}
        | AND_SYM {}
        ;

require_list:
          require_list_element opt_and require_list
        | require_list_element
        ;

require_list_element:
          SUBJECT_SYM TEXT_STRING
          {
            LEX *lex=Lex;
            if (lex->x509_subject)
            {
              my_error(ER_DUP_ARGUMENT, MYF(0), "SUBJECT");
              MYSQL_YYABORT;
            }
            lex->x509_subject=$2.str;
          }
        | ISSUER_SYM TEXT_STRING
          {
            LEX *lex=Lex;
            if (lex->x509_issuer)
            {
              my_error(ER_DUP_ARGUMENT, MYF(0), "ISSUER");
              MYSQL_YYABORT;
            }
            lex->x509_issuer=$2.str;
          }
        | CIPHER_SYM TEXT_STRING
          {
            LEX *lex=Lex;
            if (lex->ssl_cipher)
            {
              my_error(ER_DUP_ARGUMENT, MYF(0), "CIPHER");
              MYSQL_YYABORT;
            }
            lex->ssl_cipher=$2.str;
          }
        ;

grant_ident:
          '*'
          {
            LEX *lex= Lex;
            size_t dummy;
            if (lex->copy_db_to(&lex->current_select()->db, &dummy))
              MYSQL_YYABORT;
            if (lex->grant == GLOBAL_ACLS)
              lex->grant = DB_ACLS & ~GRANT_ACL;
            else if (lex->columns.elements)
            {
              my_error(ER_ILLEGAL_GRANT_FOR_TABLE, MYF(0));
              MYSQL_YYABORT;
            }
          }
        | ident '.' '*'
          {
            LEX *lex= Lex;
            lex->current_select()->db = $1.str;
            if (lex->grant == GLOBAL_ACLS)
              lex->grant = DB_ACLS & ~GRANT_ACL;
            else if (lex->columns.elements)
            {
              my_error(ER_ILLEGAL_GRANT_FOR_TABLE, MYF(0));
              MYSQL_YYABORT;
            }
          }
        | '*' '.' '*'
          {
            LEX *lex= Lex;
            lex->current_select()->db = NULL;
            if (lex->grant == GLOBAL_ACLS)
              lex->grant= GLOBAL_ACLS & ~GRANT_ACL;
            else if (lex->columns.elements)
            {
              my_error(ER_ILLEGAL_GRANT_FOR_TABLE, MYF(0));
              MYSQL_YYABORT;
            }
          }
        | table_ident
          {
            LEX *lex=Lex;
            if (!lex->current_select()->add_table_to_list(lex->thd, $1,NULL,
                                                        TL_OPTION_UPDATING))
              MYSQL_YYABORT;
            if (lex->grant == GLOBAL_ACLS)
              lex->grant =  TABLE_ACLS & ~GRANT_ACL;
          }
        ;

user_list:
          user
          {
            $$= new List<LEX_USER>;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        | user_list ',' user
          {
            $$= $1;
            if ($$->push_back($3))
              MYSQL_YYABORT;
          }
        ;

role_list:
          role
          {
            $$= new List<LEX_USER>;
            if ($$ == NULL || $$->push_back($1))
              MYSQL_YYABORT;
          }
        | role_list ',' role
          {
            $$= $1;
            if ($$->push_back($3))
              MYSQL_YYABORT;
          }
        ;

grant_list:
          grant_user
          {
            if (Lex->users_list.push_back($1))
              MYSQL_YYABORT;
          }
        | grant_list ',' grant_user
          {
            if (Lex->users_list.push_back($3))
              MYSQL_YYABORT;
          }
        ;

grant_user:
          user IDENTIFIED_SYM BY TEXT_STRING
          {
            $$=$1;
            $1->auth.str= $4.str;
            $1->auth.length= $4.length;
            $1->uses_identified_by_clause= true;
            Lex->contains_plaintext_password= true;
          }
        | user IDENTIFIED_SYM BY PASSWORD TEXT_STRING
          {
            $$= $1;
            $1->auth.str= $5.str;
            $1->auth.length= $5.length;
            $1->uses_identified_by_password_clause= true;
            if (Lex->sql_command == SQLCOM_ALTER_USER)
            {
              my_syntax_error(YYTHD, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            else
              push_deprecated_warn(YYTHD, "IDENTIFIED BY PASSWORD",
                                   "IDENTIFIED WITH <plugin> AS <hash>");
          }
        | user IDENTIFIED_SYM WITH ident_or_text
          {
            $$= $1;
            $1->plugin.str= $4.str;
            $1->plugin.length= $4.length;
            $1->auth= EMPTY_CSTR;
            $1->uses_identified_with_clause= true;
          }
        | user IDENTIFIED_SYM WITH ident_or_text AS TEXT_STRING_sys
          {
            $$= $1;
            $1->plugin.str= $4.str;
            $1->plugin.length= $4.length;
            $1->auth.str= $6.str;
            $1->auth.length= $6.length;
            $1->uses_identified_with_clause= true;
            $1->uses_authentication_string_clause= true;
          }
        | user IDENTIFIED_SYM WITH ident_or_text BY TEXT_STRING_sys
          {
            $$= $1;
            $1->plugin.str= $4.str;
            $1->plugin.length= $4.length;
            $1->auth.str= $6.str;
            $1->auth.length= $6.length;
            $1->uses_identified_with_clause= true;
            $1->uses_identified_by_clause= true;
            Lex->contains_plaintext_password= true;
          }
        | user
          {
            $$= $1;
            $1->auth= NULL_CSTR;
          }
        ;

opt_column_list:
          /* empty */        { $$= NULL; }
        | '(' column_list ')' { $$= $2; }
        ;

column_list:
          ident
          {
            $$= NEW_PTN Trivial_array<LEX_CSTRING>(YYMEM_ROOT);
            if ($$ == NULL || $$->push_back(to_lex_cstring($1)))
              MYSQL_YYABORT; // OOM
          }
        | column_list ',' ident
          {
            $$= $1;
            if ($$->push_back(to_lex_cstring($3)))
              MYSQL_YYABORT; // OOM
          }
        ;

require_clause:
          /* empty */
        | REQUIRE_SYM require_list
          {
            Lex->ssl_type=SSL_TYPE_SPECIFIED;
          }
        | REQUIRE_SYM SSL_SYM
          {
            Lex->ssl_type=SSL_TYPE_ANY;
          }
        | REQUIRE_SYM X509_SYM
          {
            Lex->ssl_type=SSL_TYPE_X509;
          }
        | REQUIRE_SYM NONE_SYM
          {
            Lex->ssl_type=SSL_TYPE_NONE;
          }
        ;

grant_options:
          /* empty */ {}
        | WITH grant_option_list
        ;

opt_grant_option:
          /* empty */       { $$= false; }
        | WITH GRANT OPTION { $$= true; }
        ;

grant_option_list:
          grant_option_list grant_option {}
        | grant_option {}
        ;

grant_option:
          GRANT OPTION { Lex->grant |= GRANT_ACL;}
        | MAX_QUERIES_PER_HOUR ulong_num
          {
            LEX *lex=Lex;
            lex->mqh.questions=$2;
            lex->mqh.specified_limits|= USER_RESOURCES::QUERIES_PER_HOUR;
          }
        | MAX_UPDATES_PER_HOUR ulong_num
          {
            LEX *lex=Lex;
            lex->mqh.updates=$2;
            lex->mqh.specified_limits|= USER_RESOURCES::UPDATES_PER_HOUR;
          }
        | MAX_CONNECTIONS_PER_HOUR ulong_num
          {
            LEX *lex=Lex;
            lex->mqh.conn_per_hour= $2;
            lex->mqh.specified_limits|= USER_RESOURCES::CONNECTIONS_PER_HOUR;
          }
        | MAX_USER_CONNECTIONS_SYM ulong_num
          {
            LEX *lex=Lex;
            lex->mqh.user_conn= $2;
            lex->mqh.specified_limits|= USER_RESOURCES::USER_CONNECTIONS;
          }
        ;

begin_stmt:
          BEGIN_SYM
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_BEGIN;
            lex->start_transaction_opt= 0;
          }
          opt_work {}
        ;

opt_work:
          /* empty */ {}
        | WORK_SYM  {}
        ;

opt_chain:
          /* empty */
          { $$= TVL_UNKNOWN; }
        | AND_SYM NO_SYM CHAIN_SYM { $$= TVL_NO; }
        | AND_SYM CHAIN_SYM        { $$= TVL_YES; }
        ;

opt_release:
          /* empty */
          { $$= TVL_UNKNOWN; }
        | RELEASE_SYM        { $$= TVL_YES; }
        | NO_SYM RELEASE_SYM { $$= TVL_NO; }
;

opt_savepoint:
          /* empty */ {}
        | SAVEPOINT_SYM {}
        ;

commit:
          COMMIT_SYM opt_work opt_chain opt_release
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_COMMIT;
            /* Don't allow AND CHAIN RELEASE. */
            MYSQL_YYABORT_UNLESS($3 != TVL_YES || $4 != TVL_YES);
            lex->tx_chain= $3;
            lex->tx_release= $4;
          }
        ;

rollback:
          ROLLBACK_SYM opt_work opt_chain opt_release
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_ROLLBACK;
            /* Don't allow AND CHAIN RELEASE. */
            MYSQL_YYABORT_UNLESS($3 != TVL_YES || $4 != TVL_YES);
            lex->tx_chain= $3;
            lex->tx_release= $4;
          }
        | ROLLBACK_SYM opt_work
          TO_SYM opt_savepoint ident
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_ROLLBACK_TO_SAVEPOINT;
            lex->ident= $5;
          }
        ;

savepoint:
          SAVEPOINT_SYM ident
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_SAVEPOINT;
            lex->ident= $2;
          }
        ;

release:
          RELEASE_SYM SAVEPOINT_SYM ident
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_RELEASE_SAVEPOINT;
            lex->ident= $3;
          }
        ;

/*
   UNIONS : glue selects together
*/


union_option:
          /* empty */ { $$=1; }
        | DISTINCT  { $$=1; }
        | ALL       { $$=0; }
        ;

row_subquery:
          subquery
        ;

table_subquery:
          subquery
        ;

subquery:
          query_expression_parens %prec SUBQUERY_AS_EXPR
          {
            if ($1 == NULL)
              MYSQL_YYABORT; // OOM

            if ($1->has_into_clause())
              YYTHD->syntax_error_at(@1, ER_THD(YYTHD, ER_SYNTAX_ERROR));

            $$= NEW_PTN PT_subquery(@$, $1);
          }
        ;

query_spec_option:
          STRAIGHT_JOIN       { $$= SELECT_STRAIGHT_JOIN; }
        | HIGH_PRIORITY       { $$= SELECT_HIGH_PRIORITY; }
        | DISTINCT            { $$= SELECT_DISTINCT; }
        | SQL_SMALL_RESULT    { $$= SELECT_SMALL_RESULT; }
        | SQL_BIG_RESULT      { $$= SELECT_BIG_RESULT; }
        | SQL_BUFFER_RESULT   { $$= OPTION_BUFFER_RESULT; }
        | SQL_CALC_FOUND_ROWS { $$= OPTION_FOUND_ROWS; }
        | ALL                 { $$= SELECT_ALL; }
        ;

/**************************************************************************

 CREATE VIEW | TRIGGER | PROCEDURE statements.

**************************************************************************/

view_or_trigger_or_sp_or_event:
          definer definer_tail
          {}
        | no_definer no_definer_tail
          {}
        | view_replace_or_algorithm definer_opt view_tail
          {}
        ;

definer_tail:
          view_tail
        | trigger_tail
        | sp_tail
        | sf_tail
        | event_tail
        ;

no_definer_tail:
          view_tail
        | trigger_tail
        | sp_tail
        | sf_tail
        | udf_tail
        | event_tail
        ;

/**************************************************************************

 DEFINER clause support.

**************************************************************************/

definer_opt:
          no_definer
        | definer
        ;

no_definer:
          /* empty */
          {
            /*
              We have to distinguish missing DEFINER-clause from case when
              CURRENT_USER specified as definer explicitly in order to properly
              handle CREATE TRIGGER statements which come to replication thread
              from older master servers (i.e. to create non-suid trigger in this
              case).
            */
            YYTHD->lex->definer= 0;
          }
        ;

definer:
          DEFINER_SYM EQ user
          {
            YYTHD->lex->definer= get_current_user(YYTHD, $3);
          }
        ;

/**************************************************************************

 CREATE VIEW statement parts.

**************************************************************************/

view_replace_or_algorithm:
          view_replace
          {}
        | view_replace view_algorithm
          {}
        | view_algorithm
          {}
        ;

view_replace:
          OR_SYM REPLACE_SYM
          { Lex->create_view_mode= enum_view_create_mode::VIEW_CREATE_OR_REPLACE; }
        ;

view_algorithm:
          ALGORITHM_SYM EQ UNDEFINED_SYM
          { Lex->create_view_algorithm= VIEW_ALGORITHM_UNDEFINED; }
        | ALGORITHM_SYM EQ MERGE_SYM
          { Lex->create_view_algorithm= VIEW_ALGORITHM_MERGE; }
        | ALGORITHM_SYM EQ TEMPTABLE_SYM
          { Lex->create_view_algorithm= VIEW_ALGORITHM_TEMPTABLE; }
        ;

view_suid:
          /* empty */
          { Lex->create_view_suid= VIEW_SUID_DEFAULT; }
        | SQL_SYM SECURITY_SYM DEFINER_SYM
          { Lex->create_view_suid= VIEW_SUID_DEFINER; }
        | SQL_SYM SECURITY_SYM INVOKER_SYM
          { Lex->create_view_suid= VIEW_SUID_INVOKER; }
        ;

view_tail:
          view_suid VIEW_SYM table_ident opt_derived_column_list
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->sql_command= SQLCOM_CREATE_VIEW;
            /* first table in list is target VIEW name */
            if (!lex->select_lex->add_table_to_list(thd, $3, NULL,
                                                    TL_OPTION_UPDATING,
                                                    TL_IGNORE,
                                                    MDL_EXCLUSIVE))
              MYSQL_YYABORT;
            lex->query_tables->open_strategy= TABLE_LIST::OPEN_STUB;
            thd->parsing_system_view= lex->query_tables->is_system_view;
            if ($4.size())
            {
              /*
                The $4 object is short-lived (its 'm_array' is not);
                so we have to duplicate it, and then we can store a
                pointer.
              */
              void *rawmem= thd->memdup(&($4), sizeof($4));
              if (!rawmem)
                MYSQL_YYABORT; /* purecov: inspected */
              lex->query_tables->
                set_derived_column_names(static_cast<Create_col_name_list* >(rawmem));
            }
          }
          AS view_select
        ;

view_select:
          query_expression_or_parens view_check_option
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;
            lex->parsing_options.allows_variable= FALSE;
            lex->parsing_options.allows_select_into= FALSE;

            /*
              In CREATE VIEW v ... the table_list initially contains
              here a table entry for the destination "table" `v'.
              Backup it and clean the table list for the processing of
              the query expression and push `v' back to the beginning of the
              table_list finally.

              @todo: Don't save the CREATE destination table in
                     SELECT_LEX::table_list and remove this backup & restore.

              The following work only with the local list, the global list
              is created correctly in this case
            */
            SQL_I_List<TABLE_LIST> save_list;
            SELECT_LEX * const save_select= Select;
            save_select->table_list.save_and_clear(&save_list);

            CONTEXTUALIZE($1);

            /*
              The following work only with the local list, the global list
              is created correctly in this case
            */
            save_select->table_list.push_front(&save_list);

            Lex->create_view_check= $2;

            /*
              It's simpler to use @$ to grab the whole rule text, OTOH  it's
              also simple to lose something that way when changing this rule,
              so let use explicit @1 and @2 to memdup this view definition:
            */
            const size_t len= @2.cpp.end - @1.cpp.start;
            lex->create_view_select.str=
              static_cast<char *>(thd->memdup(@1.cpp.start, len));
            lex->create_view_select.length= len;
            trim_whitespace(thd->charset(), &lex->create_view_select);

            lex->parsing_options.allows_variable= TRUE;
            lex->parsing_options.allows_select_into= TRUE;
          }
        ;

view_check_option:
          /* empty */                     { $$= VIEW_CHECK_NONE; }
        | WITH CHECK_SYM OPTION           { $$= VIEW_CHECK_CASCADED; }
        | WITH CASCADED CHECK_SYM OPTION  { $$= VIEW_CHECK_CASCADED; }
        | WITH LOCAL_SYM CHECK_SYM OPTION { $$= VIEW_CHECK_LOCAL; }
        ;

/**************************************************************************

 CREATE TRIGGER statement parts.

**************************************************************************/

trigger_action_order:
            FOLLOWS_SYM
            { $$= TRG_ORDER_FOLLOWS; }
          | PRECEDES_SYM
            { $$= TRG_ORDER_PRECEDES; }
          ;

trigger_follows_precedes_clause:
            /* empty */
            {
              $$.ordering_clause= TRG_ORDER_NONE;
              $$.anchor_trigger_name= NULL_CSTR;
            }
          |
            trigger_action_order ident_or_text
            {
              $$.ordering_clause= $1;
              $$.anchor_trigger_name= { $2.str, $2.length };
            }
          ;

trigger_tail:
          TRIGGER_SYM       /* $1 */
          sp_name           /* $2 */
          trg_action_time   /* $3 */
          trg_event         /* $4 */
          ON_SYM            /* $5 */
          table_ident       /* $6 */
          FOR_SYM           /* $7 */
          EACH_SYM          /* $8 */
          ROW_SYM           /* $9 */
          trigger_follows_precedes_clause /* $10 */
          {                 /* $11 */
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            if (lex->sphead)
            {
              my_error(ER_SP_NO_RECURSIVE_CREATE, MYF(0), "TRIGGER");
              MYSQL_YYABORT;
            }

            sp_head *sp= sp_start_parsing(thd, enum_sp_type::TRIGGER, $2);

            if (!sp)
              MYSQL_YYABORT;

            sp->m_trg_chistics.action_time= (enum enum_trigger_action_time_type) $3;
            sp->m_trg_chistics.event= (enum enum_trigger_event_type) $4;
            sp->m_trg_chistics.ordering_clause= $10.ordering_clause;
            sp->m_trg_chistics.anchor_trigger_name= $10.anchor_trigger_name;

            lex->stmt_definition_begin= @1.cpp.start;
            lex->ident.str= const_cast<char *>(@6.cpp.start);
            lex->ident.length= @8.cpp.start - @6.cpp.start;

            lex->sphead= sp;
            lex->spname= $2;

            memset(&lex->sp_chistics, 0, sizeof(st_sp_chistics));
            sp->m_chistics= &lex->sp_chistics;
            sp->set_body_start(thd, @10.cpp.end);
          }
          sp_proc_stmt /* $12 */
          { /* $13 */
            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;

            sp_finish_parsing(thd);

            lex->sql_command= SQLCOM_CREATE_TRIGGER;

            if (sp->is_not_allowed_in_function("trigger"))
              MYSQL_YYABORT;

            /*
              We have to do it after parsing trigger body, because some of
              sp_proc_stmt alternatives are not saving/restoring LEX, so
              lex->query_tables can be wiped out.
            */
            if (!lex->select_lex->add_table_to_list(thd, $6,
                                                    (LEX_STRING*) 0,
                                                    TL_OPTION_UPDATING,
                                                    TL_READ_NO_INSERT,
                                                    MDL_SHARED_NO_WRITE))
              MYSQL_YYABORT;

            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_create_trigger();
          }
        ;

/**************************************************************************

 CREATE FUNCTION | PROCEDURE statements parts.

**************************************************************************/

udf_tail:
          AGGREGATE_SYM FUNCTION_SYM ident
          RETURNS_SYM udf_type SONAME_SYM TEXT_STRING_sys
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            if (is_native_function($3))
            {
              my_error(ER_NATIVE_FCT_NAME_COLLISION, MYF(0),
                       $3.str);
              MYSQL_YYABORT;
            }
            lex->sql_command = SQLCOM_CREATE_FUNCTION;
            lex->udf.type= UDFTYPE_AGGREGATE;
            lex->stmt_definition_begin= @2.cpp.start;
            lex->udf.name = $3;
            lex->udf.returns=(Item_result) $5;
            lex->udf.dl=$7.str;
          }
        | FUNCTION_SYM ident
          RETURNS_SYM udf_type SONAME_SYM TEXT_STRING_sys
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            if (is_native_function($2))
            {
              my_error(ER_NATIVE_FCT_NAME_COLLISION, MYF(0),
                       $2.str);
              MYSQL_YYABORT;
            }
            lex->sql_command = SQLCOM_CREATE_FUNCTION;
            lex->udf.type= UDFTYPE_FUNCTION;
            lex->stmt_definition_begin= @1.cpp.start;
            lex->udf.name = $2;
            lex->udf.returns=(Item_result) $4;
            lex->udf.dl=$6.str;
          }
        ;

sf_tail:
          FUNCTION_SYM /* $1 */
          sp_name /* $2 */
          '(' /* $3 */
          { /* $4 */
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            lex->stmt_definition_begin= @1.cpp.start;
            lex->spname= $2;

            if (lex->sphead)
            {
              my_error(ER_SP_NO_RECURSIVE_CREATE, MYF(0), "FUNCTION");
              MYSQL_YYABORT;
            }

            sp_head *sp= sp_start_parsing(thd, enum_sp_type::FUNCTION, lex->spname);

            if (!sp)
              MYSQL_YYABORT;

            lex->sphead= sp;

            sp->m_parser_data.set_parameter_start_ptr(@3.cpp.end);
          }
          sp_fdparam_list /* $5 */
          ')' /* $6 */
          { /* $7 */
            Lex->sphead->m_parser_data.set_parameter_end_ptr(@6.cpp.start);
          }
          RETURNS_SYM /* $8 */
          type        /* $9 */
          opt_collate /* $10 */
          { /* $11 */
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;

            CONTEXTUALIZE($9);
            enum_field_types field_type= $9->type;
            const CHARSET_INFO *cs= $9->get_charset();
            if (merge_sp_var_charset_and_collation(&cs, cs, $10))
              MYSQL_YYABORT;

            /*
              This was disabled in 5.1.12. See bug #20701
              When collation support in SP is implemented, then this test
              should be removed.
            */
            if ((field_type == MYSQL_TYPE_STRING || field_type == MYSQL_TYPE_VARCHAR)
                && ($9->get_type_flags() & BINCMP_FLAG))
            {
              my_error(ER_NOT_SUPPORTED_YET, MYF(0), "return value collation");
              MYSQL_YYABORT;
            }

            if (sp->m_return_field_def.init(YYTHD, "", field_type,
                                            $9->get_length(), $9->get_dec(),
                                            $9->get_type_flags(), NULL, NULL, &NULL_STR, 0,
                                            $9->get_interval_list(),
                                            cs ? cs : YYTHD->variables.collation_database,
                                            $9->get_uint_geom_type(), NULL))
            {
              MYSQL_YYABORT;
            }

            if (prepare_sp_create_field(YYTHD,
                                        &sp->m_return_field_def))
              MYSQL_YYABORT;

            memset(&lex->sp_chistics, 0, sizeof(st_sp_chistics));
          }
          sp_c_chistics /* $12 */
          { /* $13 */
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            lex->sphead->m_chistics= &lex->sp_chistics;
            lex->sphead->set_body_start(thd, yylloc.cpp.start);
          }
          sp_proc_stmt /* $14 */
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            if (sp->is_not_allowed_in_function("function"))
              MYSQL_YYABORT;

            sp_finish_parsing(thd);

            lex->sql_command= SQLCOM_CREATE_SPFUNCTION;

            if (!(sp->m_flags & sp_head::HAS_RETURN))
            {
              my_error(ER_SP_NORETURN, MYF(0), sp->m_qname.str);
              MYSQL_YYABORT;
            }

            if (is_native_function(sp->m_name))
            {
              /*
                This warning will be printed when
                [1] A client query is parsed,
                [2] A stored function is loaded by db_load_routine.
                Printing the warning for [2] is intentional, to cover the
                following scenario:
                - A user define a SF 'foo' using MySQL 5.N
                - An application uses select foo(), and works.
                - MySQL 5.{N+1} defines a new native function 'foo', as
                part of a new feature.
                - MySQL 5.{N+1} documentation is updated, and should mention
                that there is a potential incompatible change in case of
                existing stored function named 'foo'.
                - The user deploys 5.{N+1}. At this point, 'select foo()'
                means something different, and the user code is most likely
                broken (it's only safe if the code is 'select db.foo()').
                With a warning printed when the SF is loaded (which has to occur
                before the call), the warning will provide a hint explaining
                the root cause of a later failure of 'select foo()'.
                With no warning printed, the user code will fail with no
                apparent reason.
                Printing a warning each time db_load_routine is executed for
                an ambiguous function is annoying, since that can happen a lot,
                but in practice should not happen unless there *are* name
                collisions.
                If a collision exists, it should not be silenced but fixed.
              */
              push_warning_printf(thd,
                                  Sql_condition::SL_NOTE,
                                  ER_NATIVE_FCT_NAME_COLLISION,
                                  ER_THD(thd, ER_NATIVE_FCT_NAME_COLLISION),
                                  sp->m_name.str);
            }
          }
        ;

sp_tail:
          PROCEDURE_SYM         /*$1*/
          sp_name               /*$2*/
          {                     /*$3*/
            THD *thd= YYTHD;
            LEX *lex= Lex;

            if (lex->sphead)
            {
              my_error(ER_SP_NO_RECURSIVE_CREATE, MYF(0), "PROCEDURE");
              MYSQL_YYABORT;
            }

            lex->stmt_definition_begin= @2.cpp.start;

            sp_head *sp= sp_start_parsing(thd, enum_sp_type::PROCEDURE, $2);

            if (!sp)
              MYSQL_YYABORT;

            lex->sphead= sp;
          }
          '('                   /*$4*/
          {                     /*$5*/
            Lex->sphead->m_parser_data.set_parameter_start_ptr(@4.cpp.end);
          }
          sp_pdparam_list       /*$6*/
          ')'                   /*$7*/
          {                     /*$8*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            lex->sphead->m_parser_data.set_parameter_end_ptr(@7.cpp.start);
            memset(&lex->sp_chistics, 0, sizeof(st_sp_chistics));
          }
          sp_c_chistics         /*$9*/
          {                     /*$10*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;

            lex->sphead->m_chistics= &lex->sp_chistics;
            lex->sphead->set_body_start(thd, yylloc.cpp.start);
          }
          sp_proc_stmt          /*$11*/
          {                     /*$12*/
            THD *thd= YYTHD;
            LEX *lex= Lex;

            sp_finish_parsing(thd);

            lex->sql_command= SQLCOM_CREATE_PROCEDURE;
          }
        ;

/*************************************************************************/

xa:
          XA_SYM begin_or_start xid opt_join_or_resume
          {
            Lex->sql_command = SQLCOM_XA_START;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_xa_start($3, $4);
          }
        | XA_SYM END xid opt_suspend
          {
            Lex->sql_command = SQLCOM_XA_END;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_xa_end($3, $4);
          }
        | XA_SYM PREPARE_SYM xid
          {
            Lex->sql_command = SQLCOM_XA_PREPARE;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_xa_prepare($3);
          }
        | XA_SYM COMMIT_SYM xid opt_one_phase
          {
            Lex->sql_command = SQLCOM_XA_COMMIT;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_xa_commit($3, $4);
          }
        | XA_SYM ROLLBACK_SYM xid
          {
            Lex->sql_command = SQLCOM_XA_ROLLBACK;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_xa_rollback($3);
          }
        | XA_SYM RECOVER_SYM opt_convert_xid
          {
            Lex->sql_command = SQLCOM_XA_RECOVER;
            Lex->m_sql_cmd= NEW_PTN Sql_cmd_xa_recover($3);
          }
        ;

opt_convert_xid:
          /* empty */ { $$= false; }
         | CONVERT_SYM XID_SYM { $$= true; }

xid:
          text_string
          {
            MYSQL_YYABORT_UNLESS($1->length() <= MAXGTRIDSIZE);
            XID *xid;
            if (!(xid= (XID *)YYTHD->alloc(sizeof(XID))))
              MYSQL_YYABORT;
            xid->set(1L, $1->ptr(), $1->length(), 0, 0);
            $$= xid;
          }
          | text_string ',' text_string
          {
            MYSQL_YYABORT_UNLESS($1->length() <= MAXGTRIDSIZE &&
                                 $3->length() <= MAXBQUALSIZE);
            XID *xid;
            if (!(xid= (XID *)YYTHD->alloc(sizeof(XID))))
              MYSQL_YYABORT;
            xid->set(1L, $1->ptr(), $1->length(), $3->ptr(), $3->length());
            $$= xid;
          }
          | text_string ',' text_string ',' ulong_num
          {
            // check for overwflow of xid format id 
            bool format_id_overflow_detected= ($5 > LONG_MAX);

            MYSQL_YYABORT_UNLESS($1->length() <= MAXGTRIDSIZE &&
                                 $3->length() <= MAXBQUALSIZE
                                 && !format_id_overflow_detected);

            XID *xid;
            if (!(xid= (XID *)YYTHD->alloc(sizeof(XID))))
              MYSQL_YYABORT;
            xid->set($5, $1->ptr(), $1->length(), $3->ptr(), $3->length());
            $$= xid;
          }
        ;

begin_or_start:
          BEGIN_SYM {}
        | START_SYM {}
        ;

opt_join_or_resume:
          /* nothing */ { $$= XA_NONE;        }
        | JOIN_SYM      { $$= XA_JOIN;        }
        | RESUME_SYM    { $$= XA_RESUME;      }
        ;

opt_one_phase:
          /* nothing */     { $$= XA_NONE;        }
        | ONE_SYM PHASE_SYM { $$= XA_ONE_PHASE;   }
        ;

opt_suspend:
          /* nothing */
          { $$= XA_NONE;        }
        | SUSPEND_SYM
          { $$= XA_SUSPEND;     }
        | SUSPEND_SYM FOR_SYM MIGRATE_SYM
          { $$= XA_FOR_MIGRATE; }
        ;

install:
          INSTALL_SYM PLUGIN_SYM ident SONAME_SYM TEXT_STRING_sys
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_INSTALL_PLUGIN;
            lex->m_sql_cmd= new Sql_cmd_install_plugin($3, $5);
          }
        | INSTALL_SYM COMPONENT_SYM TEXT_STRING_sys_list
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_INSTALL_COMPONENT;
            lex->m_sql_cmd= new Sql_cmd_install_component($3);
          }
        ;

uninstall:
          UNINSTALL_SYM PLUGIN_SYM ident
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_UNINSTALL_PLUGIN;
            lex->m_sql_cmd= new Sql_cmd_uninstall_plugin($3);
          }
       | UNINSTALL_SYM COMPONENT_SYM TEXT_STRING_sys_list
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_UNINSTALL_COMPONENT;
            lex->m_sql_cmd= new Sql_cmd_uninstall_component($3);
          }
        ;

TEXT_STRING_sys_list:
          TEXT_STRING_sys
          {
            $$.init(YYTHD->mem_root);
            if ($$.push_back($1))
              MYSQL_YYABORT; // OOM
          }
        | TEXT_STRING_sys_list ',' TEXT_STRING_sys
          {
            $$= $1;
            if ($$.push_back($3))
              MYSQL_YYABORT; // OOM
          }
        ;

import_stmt:
          IMPORT TABLE_SYM FROM TEXT_STRING_sys_list
          {
            LEX *lex= Lex;
            lex->m_sql_cmd=
              new (YYTHD->mem_root) Sql_cmd_import_table($4);
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
            lex->sql_command= SQLCOM_IMPORT;
          }
        ;

/**
  @} (end of group Parser)
*/
