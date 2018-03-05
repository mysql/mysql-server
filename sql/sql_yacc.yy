/*
   Copyright (c) 2000, 2018 Oracle and/or its affiliates. All rights reserved.

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
#define YYLIP (& YYTHD->m_parser_state->m_lip)
#define YYPS (& YYTHD->m_parser_state->m_yacc)
#define YYCSCL (YYLIP->query_charset)

#define YYINITDEPTH 100
#define YYMAXDEPTH 3200                        /* Because of 64K stack */
#define Lex (YYTHD->lex)
#define Select Lex->current_select()
#include "sql_parse.h"                        /* comp_*_creator */
#include "sql_table.h"                        /* primary_key_name */
#include "partition_info.h"                   /* partition_info */
#include "sql_partition.h"                    /* mem_alloc_error */
#include "auth_common.h"                      /* *_ACL */
#include "password.h"       /* my_make_scrambled_password_323, my_make_scrambled_password */
#include "sql_class.h"      /* Key_part_spec, enum_filetype */
#include "rpl_slave.h"
#include "rpl_msr.h"       /* multisource replication */
#include "rpl_filter.h"
#include "log_event.h"
#include "lex_symbol.h"
#include "item_create.h"
#include "sp_head.h"
#include "sp_instr.h"
#include "sp_pcontext.h"
#include "sp_rcontext.h"
#include "sp.h"
#include "sql_alter.h"                         // Sql_cmd_alter_table*
#include "sql_truncate.h"                      // Sql_cmd_truncate_table
#include "sql_admin.h"                         // Sql_cmd_analyze/Check..._table
#include "sql_partition_admin.h"               // Sql_cmd_alter_table_*_part.
#include "sql_handler.h"                       // Sql_cmd_handler_*
#include "sql_signal.h"
#include "sql_get_diagnostics.h"               // Sql_cmd_get_diagnostics
#include "sql_servers.h"
#include "event_parse_data.h"
#include <myisam.h>
#include <myisammrg.h>
#include "keycaches.h"
#include "set_var.h"
#include "opt_explain_traditional.h"
#include "opt_explain_json.h"
#include "rpl_slave.h"                       // Sql_cmd_change_repl_filter
#include "sql_show_status.h"                 // build_show_session_status, ...
#include "parse_location.h"
#include "parse_tree_helpers.h"
#include "lex_token.h"
#include "item_cmpfunc.h"
#include "item_geofunc.h"
#include "item_json_func.h"
#include "sql_plugin.h"                      // plugin_is_ready
#include "parse_tree_hints.h"

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
    my_syntax_error(ER(ER_SYNTAX_ERROR));\
    MYSQL_YYABORT;                      \
  }

#define NEW_PTN new(YYTHD->mem_root)


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
    if (YYTHD->is_error())                              \
      MYSQL_YYABORT;                                    \
    Lex->m_sql_cmd= (x)->make_cmd(YYTHD);               \
  } while(0)


#ifndef DBUG_OFF
#define YYDEBUG 1
#else
#define YYDEBUG 0
#endif


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

void MYSQLerror(YYLTYPE *, THD *thd, const char *s)
{
  /*
    Restore the original LEX if it was replaced when parsing
    a stored procedure. We must ensure that a parsing error
    does not leave any side effects in the THD.
  */
  LEX::cleanup_lex_after_parse_error(thd);

  /* "parse error" changed into "syntax error" between bison 1.75 and 1.875 */
  if (strcmp(s,"parse error") == 0 || strcmp(s,"syntax error") == 0)
    s= ER(ER_SYNTAX_ERROR);
  my_syntax_error(s);
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

static bool is_native_function(THD *thd, const LEX_STRING *name)
{
  if (find_native_function_builder(thd, *name))
    return true;

  if (is_lex_native_function(name))
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

void case_stmt_action_case(THD *thd)
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

bool case_stmt_action_then(THD *thd, LEX *lex)
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

void case_stmt_action_end_case(LEX *lex, bool simple)
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


static bool add_create_index_prepare (LEX *lex, Table_ident *table)
{
  lex->sql_command= SQLCOM_CREATE_INDEX;
  if (!lex->current_select()->add_table_to_list(lex->thd, table, NULL,
                                              TL_OPTION_UPDATING,
                                              TL_READ_NO_INSERT,
                                              MDL_SHARED_UPGRADABLE))
    return TRUE;
  lex->alter_info.reset();
  lex->alter_info.flags= Alter_info::ALTER_ADD_INDEX;
  lex->col_list.empty();
  lex->change= NullS;
  return FALSE;
}

static bool add_create_index (LEX *lex, keytype type,
                              const LEX_STRING &name,
                              KEY_CREATE_INFO *info= NULL, bool generated= 0)
{
  Key *key;
  key= new Key(type, name, info ? info : &lex->key_create_info, generated,
               lex->col_list);
  if (key == NULL)
    return TRUE;

  lex->alter_info.key_list.push_back(key);
  lex->col_list.empty();
  return FALSE;
}

/**
  Compare a LEX_USER against the current user as defined by the exact user and
  host used during authentication.

  @param user A pointer to a user which needs to be matched against the
              current.

  @see SET PASSWORD rules

  @retval true The specified user is the authorized user
  @retval false The user doesn't match
*/

bool match_authorized_user(Security_context *ctx, LEX_USER *user)
{
  if(user->user.str && my_strcasecmp(system_charset_info,
                                     ctx->priv_user().str,
                                     user->user.str) == 0)
  {
    /*
      users match; let's compare hosts.
      1. first compare with the host we actually authorized,
      2. then see if we match the host mask of the priv_host
    */
    if (user->host.str && my_strcasecmp(system_charset_info,
                                        user->host.str,
                                        ctx->priv_host().str) == 0)
    {
      /* specified user exactly match the authorized user */
      return true;
    }
  }
  return false;
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

#include "parse_tree_nodes.h"
#include "parse_tree_items.h"

%}

%yacc

%parse-param { class THD *YYTHD }
%lex-param { class THD *YYTHD }
%pure-parser                                    /* We have threads */
/*
  Currently there are 159 shift/reduce conflicts.
  We should not introduce new conflicts any more.
*/
%expect 155

/*
   Comments for TOKENS.
   For each token, please include in the same line a comment that contains
   the following tags:
   SQL-2015-R : Reserved keyword as per SQL-2015 draft
   SQL-2003-R : Reserved keyword as per SQL-2003
   SQL-2003-N : Non Reserved keyword as per SQL-2003
   SQL-1999-R : Reserved keyword as per SQL-1999
   SQL-1999-N : Non Reserved keyword as per SQL-1999
   MYSQL      : MySQL extention (unspecified)
   MYSQL-FUNC : MySQL extention, function
   INTERNAL   : Not a real token, lex optimization
   OPERATOR   : SQL operator
   FUTURE-USE : Reserved for futur use

   This makes the code grep-able, and helps maintenance.
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
%token  ANALYSE_SYM
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
%token  BIGINT                        /* SQL-2003-R */
%token  BINARY                        /* SQL-2003-R */
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
%token  DATETIME
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
%token  DEFAULT                       /* SQL-2003-R */
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
%token  ENUM
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
%token  GEOMETRYCOLLECTION
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
%token  INSERT                        /* SQL-2003-R */
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
%token  JSON_UNQUOTED_SEPARATOR_SYM   /* MYSQL */
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
%token  LINESTRING
%token  LIST_SYM
%token  LOAD
%token  LOCAL_SYM                     /* SQL-2003-R */
%token  LOCATOR_SYM                   /* SQL-2003-N */
%token  LOCKS_SYM
%token  LOCK_SYM
%token  LOGFILE_SYM
%token  LOGS_SYM
%token  LONGBLOB
%token  LONGTEXT
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
%token  MEDIUMBLOB
%token  MEDIUMINT
%token  MEDIUMTEXT
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
%token  MULTILINESTRING
%token  MULTIPOINT
%token  MULTIPOLYGON
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
%token  ON                            /* SQL-2003-R */
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
%token  PARSE_GCOL_EXPR_SYM
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
%token  POLYGON
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
%token  REAL                          /* SQL-2003-R */
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
%token  REPLACE                       /* MYSQL-FUNC */
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
%token  SET                           /* SQL-2003-R */
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
%token  SMALLINT                      /* SQL-2003-R */
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
%token  TABLE_REF_PRIORITY
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
%token  TIMESTAMP                     /* SQL-2003-R */
%token  TIMESTAMP_ADD
%token  TIMESTAMP_DIFF
%token  TIME_SYM                      /* SQL-2003-R */
%token  TINYBLOB
%token  TINYINT
%token  TINYTEXT
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
%token  UNSIGNED
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
%token  VARBINARY
%token  VARCHAR                       /* SQL-2003-R */
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
%token  WITH_CUBE_SYM                 /* INTERNAL */
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
%token  ZEROFILL

/*
   Tokens from MySQL 8.0
*/
%token  JSON_OBJECTAGG                /* SQL-2015-R */
%token  JSON_ARRAYAGG                 /* SQL-2015-R */

/*
  Resolve column attribute ambiguity -- force precedence of "UNIQUE KEY" against
  simple "UNIQUE" and "KEY" attributes:
*/
%right UNIQUE_SYM KEY_SYM

%left   JOIN_SYM INNER_SYM STRAIGHT_JOIN CROSS LEFT RIGHT
/* A dummy token to force the priority of table_ref production in a join. */
%left   TABLE_REF_PRIORITY
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
%right  BINARY COLLATE_SYM
%left  INTERVAL_SYM

%type <lex_str>
        IDENT IDENT_QUOTED TEXT_STRING DECIMAL_NUM FLOAT_NUM NUM LONG_NUM HEX_NUM
        LEX_HOSTNAME ULONGLONG_NUM field_ident select_alias ident ident_or_text
        IDENT_sys TEXT_STRING_sys TEXT_STRING_literal
        NCHAR_STRING opt_component key_cache_name
        sp_opt_label BIN_NUM label_ident TEXT_STRING_filesystem ident_or_empty
        opt_constraint constraint opt_ident TEXT_STRING_sys_nonewline
        filter_wild_db_table_string

%type <lex_str_ptr>
        opt_table_alias

%type <table>
        table_ident table_ident_nodb references

%type <simple_string>
        opt_db password

%type <string>
        text_string opt_gconcat_separator

%type <num>
        type type_with_opt_collate int_type real_type order_dir lock_option
        udf_type if_exists opt_local opt_table_options table_options
        table_option opt_if_not_exists opt_no_write_to_binlog
        opt_temporary all_or_any opt_distinct
        opt_ignore_leaves fulltext_options spatial_type union_option
        transaction_access_mode_types
        opt_natural_language_mode opt_query_expansion
        opt_ev_status opt_ev_on_completion ev_on_completion opt_ev_comment
        ev_alter_on_schedule_completion opt_ev_rename_to opt_ev_sql_stmt
        trg_action_time trg_event field_def

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
        ws_nweights func_datetime_precision
        ws_level_flag_desc ws_level_flag_reverse ws_level_flags
        opt_ws_levels ws_level_list ws_level_list_item ws_level_number
        ws_level_range ws_level_list_or_range
        now

%type <ulonglong_number>
        ulonglong_num real_ulonglong_num size_number
        procedure_analyse_param

%type <lock_type>
        replace_lock_option opt_low_priority insert_lock_option load_data_lock

%type <item>
        literal insert_ident temporal_literal
        simple_ident expr opt_expr opt_else sum_expr in_sum_expr
        variable variable_aux bool_pri
        predicate bit_expr
        table_wild simple_expr udf_expr
        expr_or_default set_expr_or_default
        geometry_function
        signed_literal now_or_signed_literal opt_escape
        sp_opt_default
        simple_ident_nospvar simple_ident_q
        field_or_var limit_option
        part_func_expr
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
        NUM_literal
        select_item
        opt_where_clause
        opt_having_clause
        opt_simple_limit

%type <item_list>
        when_list
        opt_filter_db_list filter_db_list
        opt_filter_table_list filter_table_list
        opt_filter_string_list filter_string_list
        opt_filter_db_pair_list filter_db_pair_list

%type <item_list2>
        expr_list udf_expr_list opt_udf_expr_list opt_expr_list select_item_list
        ident_list ident_list_arg

%type <var_type>
        option_type opt_var_type opt_var_ident_type

%type <key_type>
        normal_key_type opt_unique constraint_key_type fulltext spatial

%type <key_alg>
        btree_or_rtree

%type <string_list>
        using_list opt_use_partition use_partition

%type <key_part>
        key_part

%type <date_time_type> date_time_type;
%type <interval> interval

%type <interval_time_st> interval_time_stamp

%type <db_type> storage_engines known_storage_engines

%type <row_type> row_types

%type <tx_isolation> isolation_types

%type <ha_rkey_mode> handler_rkey_mode

%type <ha_read_mode> handler_read_or_scan handler_scan_function
        handler_rkey_function

%type <cast_type> cast_type

%type <symbol> keyword keyword_sp

%type <lex_user> user grant_user user_func

%type <charset>
        opt_collate
        opt_collate_explicit
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
        create change drop
        truncate rename
        show describe load alter optimize keycache preload flush
        reset purge begin commit rollback savepoint release
        slave master_def master_defs master_file_def slave_until_opts
        repair analyze check start checksum filter_def filter_defs
        field_list field_list_item field_spec kill column_def key_def
        keycache_list keycache_list_or_parts assign_to_keycache
        assign_to_keycache_parts
        preload_list preload_list_or_parts preload_keys preload_keys_parts
        handler
        opt_column opt_restrict
        grant revoke lock unlock string_list field_options field_option
        field_opt_list table_lock_list table_lock
        ref_list opt_match_clause opt_on_update_delete use
        varchar nchar nvarchar
        opt_outer table_list table_name
        opt_place
        opt_attribute opt_attribute_list attribute column_list column_list_id
        opt_column_list grant_privileges grant_ident grant_list grant_option
        object_privilege object_privilege_list user_list rename_list
        clear_privileges flush_options flush_option
        opt_flush_lock flush_options_list
        equal optional_braces
        opt_mi_check_type opt_to mi_check_types normal_join
        table_to_table_list table_to_table opt_table_list opt_as
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
        view_suid view_tail view_list_opt view_list view_select
        view_check_option trigger_tail
        sp_tail sf_tail udf_tail event_tail
        install uninstall partition_entry binlog_base64_event
        init_key_options normal_key_options normal_key_opts all_key_opt
        spatial_key_options fulltext_key_options normal_key_opt
        fulltext_key_opt spatial_key_opt fulltext_key_opts spatial_key_opts
        key_using_alg
        part_column_list
        server_options_list server_option
        definer_opt no_definer definer get_diagnostics
        alter_user_command password_expire
        group_replication
END_OF_INPUT

%type <NONE> call sp_proc_stmts sp_proc_stmts1 sp_proc_stmt
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

%type <is_not_empty> opt_convert_xid opt_ignore

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

%type <subselect> subselect

%type <order_expr> order_expr

%type <order_list> order_list group_list gorder_list opt_gorder_clause

%type <c_str> field_length opt_field_length type_datetime_precision

%type <ulong_num> opt_bin_mod

%type <precision> precision opt_precision float_options

%type <charset_with_flags> opt_binary

%type <limit_options> limit_options

%type <limit_clause> limit_clause opt_limit_clause

%type <ulonglong_number> query_spec_option query_spec_option_list
        opt_query_spec_options

%type <select_options> select_option select_option_list select_options
        empty_select_options

%type <node> join_table order_or_limit opt_union_order_or_limit
        option_value union_opt

%type <table_reference_list> table_reference_list from_clause opt_from_clause

%type <select_part2_derived> select_part2_derived

%type <olap_type> olap_opt

%type <group> opt_group_clause

%type <order> order_clause opt_order_clause

%type <procedure_analyse_params> opt_procedure_analyse_params

%type <procedure_analyse> opt_procedure_analyse_clause

%type <select_lock_type> opt_select_lock_type

%type <union_order_or_limit> union_order_or_limit

%type <table_expression> table_expression

%type <table_list2> select_derived_union
        table_factor table_ref esc_table_ref derived_table_list select_derived

%type <join_table_list> join_table_list

%type <select_paren_derived> select_paren_derived

%type <select_lex2> query_specification query_expression_body

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

%type <union_list> union_list opt_union_clause

%type <line_separators> line_term line_term_list opt_line_term

%type <field_separators> field_term field_term_list opt_field_term

%type <into_destination> into_destination into opt_into

%type <select_var_ident> select_var_ident

%type <select_var_list> select_var_list

%type <select_options_and_item_list> select_options_and_item_list

%type <select_part2> select_part2

%type <select_paren> select_paren

%type <select_init> select_init

%type <select> select do_stmt

%type <param_marker> param_marker

%type <text_literal> text_literal

%type <item_list2> values opt_values row_value fields

%type <statement>
        delete_stmt
        update_stmt
        insert_stmt
        replace_stmt
        shutdown_stmt
	alter_instance_stmt

%type <table_ident> table_ident_opt_wild

%type <table_ident_list> table_alias_ref_list

%type <num> opt_delete_options

%type <opt_delete_option> opt_delete_option

%type <column_value_pair>
        update_elem

%type <column_value_list_pair>
        update_list
        opt_insert_update_list

%type <create_select> create_select

%type <values_list> values_list insert_values

%type <insert_from_subquery> insert_from_subquery

%type <insert_query_expression> insert_query_expression

%type <column_row_value_list_pair> insert_from_constructor

%type <optimizer_hints> SELECT_SYM INSERT REPLACE UPDATE_SYM DELETE_SYM

%type <alter_instance_action> alter_instance_action


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

query:
          END_OF_INPUT
          {
            THD *thd= YYTHD;
            if (!thd->bootstrap &&
                !thd->m_parser_state->has_comment())
            {
              my_message(ER_EMPTY_QUERY, ER(ER_EMPTY_QUERY), MYF(0));
              MYSQL_YYABORT;
            }
            thd->lex->sql_command= SQLCOM_EMPTY_QUERY;
            YYLIP->found_semicolon= NULL;
          }
        | verb_clause
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
        | verb_clause END_OF_INPUT
          {
            /* Single query, not terminated. */
            YYLIP->found_semicolon= NULL;
          }
        ;

opt_end_of_input:
          /* empty */
        | END_OF_INPUT
        ;

verb_clause:
          statement
        | begin
        ;

/* Verb clauses, except begin */
statement:
          alter
        | analyze
        | binlog_base64_event
        | call
        | change
        | check
        | checksum
        | commit
        | create
        | deallocate
        | delete_stmt           {  MAKE_CMD($1); }
        | describe
        | do_stmt               { CONTEXTUALIZE($1); }
        | drop
        | execute
        | flush
        | get_diagnostics
        | group_replication
        | grant
        | handler
        | help
        | insert_stmt           { MAKE_CMD($1); }
        | install
        | kill
        | load
        | lock
        | optimize
        | keycache
        | parse_gcol_expr
        | partition_entry
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
        | select                { CONTEXTUALIZE($1); }
        | set                   { CONTEXTUALIZE($1); }
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
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_change_repl_filter();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
          filter_defs
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
            $$= new (YYTHD->mem_root) List<Item>;
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
            $$= new (YYTHD->mem_root) List<Item>;
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
            Item *db_item= new (thd->mem_root) Item_string($1.str,
                                                           $1.length,
                                                           thd->charset());
            $$= db_item;
          }
        ;
opt_filter_db_pair_list:
          '(' ')'
          {
            $$= new (YYTHD->mem_root) List<Item>;
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
            $$= new (YYTHD->mem_root) List<Item>;
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
            $$= new (YYTHD->mem_root) List<Item>;
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
            $$= new (YYTHD->mem_root) List<Item>;
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
            Item_string *table_item= new (thd->mem_root) Item_string($1.str,
                                                              $1.length,
                                                              thd->charset());
            table_item->append(thd->strmake(".", 1), 1);
            table_item->append($3.str, $3.length);
            $$= table_item;
          }
        ;

opt_filter_string_list:
          '(' ')'
          {
            $$= new (YYTHD->mem_root) List<Item>;
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
            $$= new (YYTHD->mem_root) List<Item>;
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
            Item *string_item= new (thd->mem_root) Item_string($1.str,
                                                               $1.length,
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
            ITEMIZE($3, &$3);

            Lex->mi.heartbeat_period= (float) $3->val_real();
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
                           ER(ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MAX));
            }
            if (Lex->mi.heartbeat_period < 0.001)
            {
              if (Lex->mi.heartbeat_period != 0.0)
              {
                push_warning(YYTHD, Sql_condition::SL_WARNING,
                             ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MIN,
                             ER(ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE_MIN));
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

/* create a table */

create:
          CREATE opt_table_options TABLE_SYM opt_if_not_exists table_ident
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->sql_command= SQLCOM_CREATE_TABLE;
            if (!lex->select_lex->add_table_to_list(thd, $5, NULL,
                                                    TL_OPTION_UPDATING,
                                                    TL_WRITE, MDL_SHARED))
              MYSQL_YYABORT;
            /*
              Instruct open_table() to acquire SHARED lock to check the
              existance of table. If the table does not exist then
              it will be upgraded EXCLUSIVE MDL lock. If table exist
              then open_table() will return with an error or warning.
            */
            lex->query_tables->open_strategy= TABLE_LIST::OPEN_FOR_CREATE;
            lex->alter_info.reset();
            lex->col_list.empty();
            lex->change=NullS;
            memset(&lex->create_info, 0, sizeof(lex->create_info));
            lex->create_info.options=$2 | $4;
            lex->create_info.default_table_charset= NULL;
            lex->name.str= 0;
            lex->name.length= 0;
            lex->create_last_non_select_table= lex->last_table();
          }
          create2
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->set_current_select(lex->select_lex);
            if ((lex->create_info.used_fields & HA_CREATE_USED_ENGINE) &&
                !lex->create_info.db_type)
            {
              lex->create_info.db_type=
                lex->create_info.options & HA_LEX_CREATE_TMP_TABLE ?
                ha_default_temp_handlerton(thd) : ha_default_handlerton(thd);
              push_warning_printf(YYTHD, Sql_condition::SL_WARNING,
                                  ER_WARN_USING_OTHER_HANDLER,
                                  ER(ER_WARN_USING_OTHER_HANDLER),
                                  ha_resolve_storage_engine_name(lex->create_info.db_type),
                                  $5->table.str);
            }
            create_table_set_open_action_and_adjust_tables(lex);
          }
        | CREATE opt_unique INDEX_SYM ident key_alg ON table_ident
          {
            if (add_create_index_prepare(Lex, $7))
              MYSQL_YYABORT;
          }
          '(' key_list ')' normal_key_options
          {
            if (add_create_index(Lex, $2, $4))
              MYSQL_YYABORT;
          }
          opt_index_lock_algorithm { }
        | CREATE fulltext INDEX_SYM ident init_key_options ON
          table_ident
          {
            if (add_create_index_prepare(Lex, $7))
              MYSQL_YYABORT;
          }
          '(' key_list ')' fulltext_key_options
          {
            if (add_create_index(Lex, $2, $4))
              MYSQL_YYABORT;
          }
          opt_index_lock_algorithm { }
        | CREATE spatial INDEX_SYM ident init_key_options ON
          table_ident
          {
            if (add_create_index_prepare(Lex, $7))
              MYSQL_YYABORT;
          }
          '(' key_list ')' spatial_key_options
          {
            if (add_create_index(Lex, $2, $4))
              MYSQL_YYABORT;
          }
          opt_index_lock_algorithm { }
        | CREATE DATABASE opt_if_not_exists ident
          {
            Lex->create_info.default_table_charset= NULL;
            Lex->create_info.used_fields= 0;
          }
          opt_create_database_options
          {
            LEX *lex=Lex;
            lex->sql_command=SQLCOM_CREATE_DB;
            lex->name= $4;
            lex->create_info.options=$3;
          }
        | CREATE
          {
            Lex->create_view_mode= VIEW_CREATE_NEW;
            Lex->create_view_algorithm= VIEW_ALGORITHM_UNDEFINED;
            Lex->create_view_suid= TRUE;
          }
          view_or_trigger_or_sp_or_event
          {}
        | CREATE USER opt_if_not_exists clear_privileges grant_list require_clause
                      connect_options opt_account_lock_password_expire_options
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_CREATE_USER;
            lex->create_info.options=$3;
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
              new (YYTHD->mem_root) Sql_cmd_create_server(&Lex->server_options);
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
            lex->create_info.options= $2;
            if (!(lex->event_parse_data= Event_parse_data::new_instance(thd)))
              MYSQL_YYABORT;
            lex->event_parse_data->identifier= $3;
            lex->event_parse_data->on_completion=
                                  Event_parse_data::ON_COMPLETION_DROP;

            lex->sql_command= SQLCOM_CREATE_EVENT;
            /* We need that for disallowing subqueries */
          }
          ON SCHEDULE_SYM ev_schedule_time
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
        | DISABLE_SYM ON SLAVE
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
            Item *item= new (YYTHD->mem_root) Item_func_now_local(0);
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
          ON COMPLETION_SYM PRESERVE_SYM
          {
            Lex->event_parse_data->on_completion=
                                  Event_parse_data::ON_COMPLETION_PRESERVE;
            $$= 1;
          }
        | ON COMPLETION_SYM NOT_SYM PRESERVE_SYM
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
            Lex->comment= Lex->event_parse_data->comment= $2;
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
                                          SP_TYPE_EVENT,
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

clear_privileges:
          clear_password_expire_options
          {
           LEX *lex=Lex;
           lex->users_list.empty();
           lex->columns.empty();
           lex->grant= lex->grant_tot_col= 0;
           lex->all_privileges= 0;
           lex->select_lex->db= NULL;
           lex->ssl_type= SSL_TYPE_NOT_SPECIFIED;
           lex->ssl_cipher= lex->x509_subject= lex->x509_issuer= 0;
           lex->alter_password.update_account_locked_column= false;
           lex->alter_password.account_locked= false;
           memset(&(lex->mqh), 0, sizeof(lex->mqh));
         }
        ;

clear_password_expire_options:
         /* Nothing */
         {
           LEX *lex=Lex;
           lex->alter_password.update_password_expired_fields= false;
           lex->alter_password.update_password_expired_column= false;
           lex->alter_password.use_default_password_lifetime= true;
           lex->alter_password.expire_after_days= 0;
         }
        ;

sp_name:
          ident '.' ident
          {
            if (!$1.str ||
                (check_and_convert_db_name(&$1, FALSE) != IDENT_NAME_OK))
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
          { Lex->sp_chistics.comment= $2; }
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

call:
          CALL_SYM sp_name
          {
            LEX *lex = Lex;

            lex->sql_command= SQLCOM_CALL;
            lex->spname= $2;
            lex->call_value_list.empty();
            sp_add_used_routine(lex, YYTHD, $2, SP_TYPE_PROCEDURE);
          }
          opt_sp_cparam_list {}
        ;

/* CALL parameters */
opt_sp_cparam_list:
          /* Empty */
        | '(' opt_sp_cparams ')'
        ;

opt_sp_cparams:
          /* Empty */
        | sp_cparams
        ;

sp_cparams:
          sp_cparams ',' expr
          {
            ITEMIZE($3, &$3);

           Lex->call_value_list.push_back($3);
          }
        | expr
          {
            ITEMIZE($1, &$1);

            Lex->call_value_list.push_back($1);
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

sp_init_param:
          /* Empty */
          {
            LEX *lex= Lex;

            lex->length= 0;
            lex->dec= 0;
            lex->type= 0;

            lex->default_value= 0;
            lex->on_update_value= 0;

            lex->comment= null_lex_str;
            lex->charset= NULL;

            lex->interval_list.empty();
            lex->uint_geom_type= 0;
          }
        ;

sp_fdparam:
          ident sp_init_param type_with_opt_collate
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            if (pctx->find_variable($1, TRUE))
            {
              my_error(ER_SP_DUP_PARAM, MYF(0), $1.str);
              MYSQL_YYABORT;
            }

            sp_variable *spvar= pctx->add_variable(thd,
                                                   $1,
                                                   (enum enum_field_types) $3,
                                                   sp_variable::MODE_IN);

            if (fill_field_definition(thd, sp,
                                      (enum enum_field_types) $3,
                                      &spvar->field_def))
            {
              MYSQL_YYABORT;
            }
            spvar->field_def.field_name= spvar->name.str;
            spvar->field_def.pack_flag |= FIELDFLAG_MAYBE_NULL;
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
          sp_opt_inout sp_init_param ident type_with_opt_collate
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            if (pctx->find_variable($3, TRUE))
            {
              my_error(ER_SP_DUP_PARAM, MYF(0), $3.str);
              MYSQL_YYABORT;
            }
            sp_variable *spvar= pctx->add_variable(thd,
                                                   $3,
                                                   (enum enum_field_types) $4,
                                                   (sp_variable::enum_mode) $1);

            if (fill_field_definition(thd, sp,
                                      (enum enum_field_types) $4,
                                      &spvar->field_def))
            {
              MYSQL_YYABORT;
            }
            spvar->field_def.field_name= spvar->name.str;
            spvar->field_def.pack_flag |= FIELDFLAG_MAYBE_NULL;
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
              my_message(ER_SP_VARCOND_AFTER_CURSHNDLR,
                         ER(ER_SP_VARCOND_AFTER_CURSHNDLR), MYF(0));
              MYSQL_YYABORT;
            }
            if ($2.curs && $1.hndlrs)
            { /* Cursor following handler */
              my_message(ER_SP_CURSOR_AFTER_HANDLER,
                         ER(ER_SP_CURSOR_AFTER_HANDLER), MYF(0));
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
          {                     /*$3*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();

            sp->reset_lex(thd);
            pctx->declare_var_boundary($2);
          }
          type_with_opt_collate /*$4*/
          sp_opt_default        /*$5*/
          {                     /*$6*/
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;
            sp_pcontext *pctx= lex->get_sp_current_parsing_ctx();
            uint num_vars= pctx->context_var_count();
            enum enum_field_types var_type= (enum enum_field_types) $4;
            Item *dflt_value_item= $5;
            LEX_STRING dflt_value_query= EMPTY_STR;

            if (dflt_value_item)
            {
              // sp_opt_default only pushes start ptr for DEFAULT clause.
              const char *expr_start_ptr=
                sp->m_parser_data.pop_expr_start_ptr();
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
              dflt_value_item= new (thd->mem_root) Item_null();

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

              if (fill_field_definition(thd, sp, var_type, &spvar->field_def))
                MYSQL_YYABORT;

              spvar->field_def.field_name= spvar->name.str;
              spvar->field_def.pack_flag |= FIELDFLAG_MAYBE_NULL;

              /* The last instruction is responsible for freeing LEX. */

              sp_instr_set *is=
                new (thd->mem_root)
                  sp_instr_set(sp->instructions(),
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
              new (thd->mem_root)
                sp_instr_hpush_jump(sp->instructions(), handler_pctx, h);

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
                new (thd->mem_root) sp_instr_hreturn(sp->instructions(), pctx);

              if (!i || sp->add_instr(thd, i))
                MYSQL_YYABORT;
            }
            else
            {  /* EXIT or UNDO handler, just jump to the end of the block */
              sp_instr_hreturn *i=
                new (thd->mem_root) sp_instr_hreturn(sp->instructions(), pctx);

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
          select        /*$6*/
          {             /*$7*/
            CONTEXTUALIZE($6);

            THD *thd= YYTHD;
            LEX *cursor_lex= Lex;
            sp_head *sp= cursor_lex->sphead;

            DBUG_ASSERT(cursor_lex->sql_command == SQLCOM_SELECT);

            if (cursor_lex->result)
            {
              my_message(ER_SP_BAD_CURSOR_SELECT, ER(ER_SP_BAD_CURSOR_SELECT),
                         MYF(0));
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
              new (thd->mem_root)
                sp_instr_cpush(sp->instructions(), pctx,
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
              my_message(ER_SP_DUP_HANDLER, ER(ER_SP_DUP_HANDLER), MYF(0));
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
            $$= new (YYTHD->mem_root) sp_condition_value($1);
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
            $$= new (YYTHD->mem_root) sp_condition_value($3.str);
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
            $$= new (YYTHD->mem_root) sp_condition_value(sp_condition_value::WARNING);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | not FOUND_SYM /* SQLSTATEs 02??? */
          {
            $$= new (YYTHD->mem_root) sp_condition_value(sp_condition_value::NOT_FOUND);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | SQLEXCEPTION_SYM /* All other SQLSTATEs */
          {
            $$= new (YYTHD->mem_root) sp_condition_value(sp_condition_value::EXCEPTION);
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
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_signal($2, $3);
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
          { $$= new (YYTHD->mem_root) Set_signal_information(); }
        | SET signal_information_item_list
          { $$= $2; }
        ;

signal_information_item_list:
          signal_condition_information_item_name EQ signal_allowed_expr
          {
            $$= new (YYTHD->mem_root) Set_signal_information();
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
                my_syntax_error(ER(ER_SYNTAX_ERROR));
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
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_resignal($2, $3);
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
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_get_diagnostics(info);

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
            $$= new (YYTHD->mem_root) Statement_information($1);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | CONDITION_SYM condition_number condition_information
          {
            $$= new (YYTHD->mem_root) Condition_information($2, $3);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

statement_information:
          statement_information_item
          {
            $$= new (YYTHD->mem_root) List<Statement_information_item>;
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
            $$= new (YYTHD->mem_root) Statement_information_item($3, $1);
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
            $$= new (YYTHD->mem_root) List<Condition_information_item>;
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
            $$= new (YYTHD->mem_root) Condition_information_item($3, $1);
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
          { $$ = NULL; }
        | DEFAULT
          { Lex->sphead->m_parser_data.push_expr_start_ptr(@1.raw.end); }
          expr
          {
            ITEMIZE($3, &$3);

            $$ = $3;
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
          statement
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
            DBUG_ASSERT(lex->sql_command != SQLCOM_SET_OPTION ||
                        lex->var_list.is_empty());
            if (lex->sql_command != SQLCOM_SET_OPTION)
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
                new (thd->mem_root)
                  sp_instr_stmt(sp->instructions(), lex, query);

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

            sp->m_parser_data.push_expr_start_ptr(@1.raw.end);
          }
          expr          /*$3*/
          {             /*$4*/
            ITEMIZE($3, &$3);

            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            sp_head *sp= lex->sphead;

            /* Extract expression string. */

            LEX_STRING expr_query= EMPTY_STR;
            const char *expr_start_ptr= sp->m_parser_data.pop_expr_start_ptr();

            if (lex->is_metadata_used())
            {
              expr_query= make_string(thd, expr_start_ptr, @3.raw.end);
              if (!expr_query.str)
                MYSQL_YYABORT;
            }

            /* Check that this is a stored function. */

            if (sp->m_type != SP_TYPE_FUNCTION)
            {
              my_message(ER_SP_BADRETURN, ER(ER_SP_BADRETURN), MYF(0));
              MYSQL_YYABORT;
            }

            /* Indicate that we've reached RETURN statement. */

            sp->m_flags|= sp_head::HAS_RETURN;

            /* Add instruction. */

            sp_instr_freturn *i=
              new (thd->mem_root)
                sp_instr_freturn(sp->instructions(), lex, $3, expr_query,
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
              sp_instr_hpop *hpop=
                new (thd->mem_root) sp_instr_hpop(ip++, pctx);

              if (!hpop || sp->add_instr(thd, hpop))
                MYSQL_YYABORT;
            }

            n= pctx->diff_cursors(lab->ctx, exclusive);

            if (n)
            {
              sp_instr_cpop *cpop=
                new (thd->mem_root) sp_instr_cpop(ip++, pctx, n);

              if (!cpop || sp->add_instr(thd, cpop))
                MYSQL_YYABORT;
            }

            sp_instr_jump *i= new (thd->mem_root) sp_instr_jump(ip, pctx);

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
              sp_instr_hpop *hpop=
                new (thd->mem_root) sp_instr_hpop(ip++, pctx);

              if (!hpop || sp->add_instr(thd, hpop))
                MYSQL_YYABORT;
            }

            /* Inclusive the dest. */
            n= pctx->diff_cursors(lab->ctx, FALSE);

            if (n)
            {
              sp_instr_cpop *cpop=
                new (thd->mem_root) sp_instr_cpop(ip++, pctx, n);

              if (!cpop || sp->add_instr(thd, cpop))
                MYSQL_YYABORT;
            }

            /* Jump back */
            sp_instr_jump *i=
              new (thd->mem_root) sp_instr_jump(ip, pctx, lab->ip);

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

            sp_instr_copen *i=
              new (thd->mem_root)
                sp_instr_copen(sp->instructions(), pctx, offset);

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

            sp_instr_cfetch *i=
              new (thd->mem_root)
                sp_instr_cfetch(sp->instructions(), pctx, offset);

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
              new (thd->mem_root)
                sp_instr_cclose(sp->instructions(), pctx, offset);

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
            sp->m_parser_data.push_expr_start_ptr(@0.raw.end);
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
            const char *expr_start_ptr= sp->m_parser_data.pop_expr_start_ptr();

            if (lex->is_metadata_used())
            {
              expr_query= make_string(thd, expr_start_ptr, @2.raw.end);
              if (!expr_query.str)
                MYSQL_YYABORT;
            }

            sp_instr_jump_if_not *i =
              new (thd->mem_root)
                sp_instr_jump_if_not(sp->instructions(), lex,
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

            sp_instr_jump *i =
              new (thd->mem_root) sp_instr_jump(sp->instructions(), pctx);

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
            sp->m_parser_data.push_expr_start_ptr(@1.raw.end);
          }
          expr                          /*$3*/
          {                             /*$4*/
            ITEMIZE($3, &$3);

            THD *thd= YYTHD;
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;

            /* Extract CASE-expression string. */

            LEX_STRING case_expr_query= EMPTY_STR;
            const char *expr_start_ptr= sp->m_parser_data.pop_expr_start_ptr();

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
              new (thd->mem_root)
                sp_instr_set_case_expr(sp->instructions(), lex,
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
            sp->m_parser_data.push_expr_start_ptr(@1.raw.end);
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
            const char *expr_start_ptr= sp->m_parser_data.pop_expr_start_ptr();

            if (lex->is_metadata_used())
            {
              when_expr_query= make_string(thd, expr_start_ptr, @3.raw.end);
              if (!when_expr_query.str)
                MYSQL_YYABORT;
            }

            /* Add CASE-when-jump instruction. */

            sp_instr_jump_case_when *i =
              new (thd->mem_root)
                sp_instr_jump_case_when(sp->instructions(), lex,
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
            sp->m_parser_data.push_expr_start_ptr(@1.raw.end);
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
            const char *expr_start_ptr= sp->m_parser_data.pop_expr_start_ptr();

            if (lex->is_metadata_used())
            {
              when_query= make_string(thd, expr_start_ptr, @3.raw.end);
              if (!when_query.str)
                MYSQL_YYABORT;
            }

            /* Add jump instruction. */

            sp_instr_jump_if_not *i=
              new (thd->mem_root)
                sp_instr_jump_if_not(sp->instructions(), lex, $3, when_query);

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
              new (thd->mem_root)
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
              sp_instr *i=
                new (thd->mem_root) sp_instr_hpop(sp->instructions(), pctx);

              if (!i || sp->add_instr(thd, i))
                MYSQL_YYABORT;
            }

            if ($3.curs)
            {
              sp_instr *i=
                new (thd->mem_root)
                  sp_instr_cpop(sp->instructions(), pctx, $3.curs);

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

            sp_instr_jump *i=
                new (thd->mem_root)
                  sp_instr_jump(sp->instructions(), pctx,
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
            sp->m_parser_data.push_expr_start_ptr(@1.raw.end);
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
            const char *expr_start_ptr= sp->m_parser_data.pop_expr_start_ptr();

            if (lex->is_metadata_used())
            {
              expr_query= make_string(thd, expr_start_ptr, @3.raw.end);
              if (!expr_query.str)
                MYSQL_YYABORT;
            }

            /* Add jump instruction. */

            sp_instr_jump_if_not *i=
              new (thd->mem_root)
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

            sp_instr_jump *i=
              new (thd->mem_root)
                sp_instr_jump(sp->instructions(), pctx, pctx->last_label()->ip);

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
            sp->m_parser_data.push_expr_start_ptr(@3.raw.end);
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
            const char *expr_start_ptr= sp->m_parser_data.pop_expr_start_ptr();

            if (lex->is_metadata_used())
            {
              expr_query= make_string(thd, expr_start_ptr, @5.raw.end);
              if (!expr_query.str)
                MYSQL_YYABORT;
            }

            /* Add jump instruction. */

            sp_instr_jump_if_not *i=
              new (thd->mem_root)
                sp_instr_jump_if_not(ip, lex, $5, expr_query,
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
            INSERT
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
        | tablespace_options tablespace_option
        | tablespace_options ',' tablespace_option
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
        | alter_tablespace_options alter_tablespace_option
        | alter_tablespace_options ',' alter_tablespace_option
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
        | logfile_group_options logfile_group_option
        | logfile_group_options ',' logfile_group_option
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
        | alter_logfile_group_options alter_logfile_group_option
        | alter_logfile_group_options ',' alter_logfile_group_option
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
          opt_storage ENGINE_SYM opt_equal storage_engines
          {
            LEX *lex= Lex;
            if (lex->alter_tablespace_info->storage_engine != NULL)
            {
              my_error(ER_FILEGROUP_OPTION_ONLY_ONCE,MYF(0),
                       "STORAGE ENGINE");
              MYSQL_YYABORT;
            }
            lex->alter_tablespace_info->storage_engine= $4;
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

create2:
          '(' create2a {}
        | opt_create_table_options
          opt_create_partitioning
          create3 {}
        | LIKE table_ident
          {
            THD *thd= YYTHD;
            TABLE_LIST *src_table;
            LEX *lex= thd->lex;

            lex->create_info.options|= HA_LEX_CREATE_TABLE_LIKE;
            src_table= lex->select_lex->add_table_to_list(thd, $2, NULL, 0,
                                                          TL_READ,
                                                          MDL_SHARED_READ);
            if (! src_table)
              MYSQL_YYABORT;
            /* CREATE TABLE ... LIKE is not allowed for views. */
            src_table->required_type= FRMTYPE_TABLE;
          }
        | '(' LIKE table_ident ')'
          {
            THD *thd= YYTHD;
            TABLE_LIST *src_table;
            LEX *lex= thd->lex;

            lex->create_info.options|= HA_LEX_CREATE_TABLE_LIKE;
            src_table= lex->select_lex->add_table_to_list(thd, $3, NULL, 0,
                                                          TL_READ,
                                                          MDL_SHARED_READ);
            if (! src_table)
              MYSQL_YYABORT;
            /* CREATE TABLE ... LIKE is not allowed for views. */
            src_table->required_type= FRMTYPE_TABLE;
          }
        ;

create2a:
          create_field_list ')' opt_create_table_options
          opt_create_partitioning
          create3 {}
        |  opt_create_partitioning
           create_select ')'
           {
             CONTEXTUALIZE($2);
             Select->set_braces(1);
           }
           union_opt
           {
             if ($5 != NULL)
               CONTEXTUALIZE($5);
           }
        ;

create3:
          /* empty */ {}
        | opt_duplicate opt_as create_select
          {
            CONTEXTUALIZE($3);
            Select->set_braces(0);
          }
          opt_union_clause
          {
            if ($5 != NULL)
              CONTEXTUALIZE($5);
          }
        | opt_duplicate opt_as '(' create_select ')'
          {
            CONTEXTUALIZE($4);
            Select->set_braces(1);
          }
          union_opt
          {
             if ($7 != NULL)
               CONTEXTUALIZE($7);
          }
        ;

opt_create_partitioning:
          opt_partitioning
          {
            /*
              Remove all tables used in PARTITION clause from the global table
              list. Partitioning with subqueries is not allowed anyway.
            */
            TABLE_LIST *last_non_sel_table= Lex->create_last_non_select_table;
            last_non_sel_table->next_global= 0;
            Lex->query_tables_last= &last_non_sel_table->next_global;
          }
        ;

/*
 This part of the parser is about handling of the partition information.

 It's first version was written by Mikael Ronström with lots of answers to
 questions provided by Antony Curtis.

 The partition grammar can be called from three places.
 1) CREATE TABLE ... PARTITION ..
 2) ALTER TABLE table_name PARTITION ...
 3) PARTITION ...

 The first place is called when a new table is created from a MySQL client.
 The second place is called when a table is altered with the ALTER TABLE
 command from a MySQL client.
 The third place is called when opening an frm file and finding partition
 info in the .frm file. It is necessary to avoid allowing PARTITION to be
 an allowed entry point for SQL client queries. This is arranged by setting
 some state variables before arriving here.

 To be able to handle errors we will only set error code in this code
 and handle the error condition in the function calling the parser. This
 is necessary to ensure we can also handle errors when calling the parser
 from the openfrm function.
*/
opt_partitioning:
          /* empty */ {}
        | partitioning
        ;

partitioning:
          PARTITION_SYM
          {
            LEX *lex= Lex;
            lex->part_info= new partition_info();
            if (!lex->part_info)
            {
              mem_alloc_error(sizeof(partition_info));
              MYSQL_YYABORT;
            }
            if (lex->sql_command == SQLCOM_ALTER_TABLE)
            {
              lex->alter_info.flags|= Alter_info::ALTER_PARTITION;
            }
          }
          partition
        ;

partition_entry:
          PARTITION_SYM
          {
            LEX *lex= Lex;
            if (!lex->part_info)
            {
              my_syntax_error(ER(ER_PARTITION_ENTRY_ERROR));
              MYSQL_YYABORT;
            }
            /*
              We enter here when opening the frm file to translate
              partition info string into part_info data structure.
            */
          }
          partition {}
        ;

partition:
          BY part_type_def opt_num_parts opt_sub_part part_defs
        ;

part_type_def:
          opt_linear KEY_SYM opt_key_algo '(' part_field_list ')'
          {
            partition_info *part_info= Lex->part_info;
            part_info->list_of_part_fields= TRUE;
            part_info->column_list= FALSE;
            part_info->part_type= HASH_PARTITION;
          }
        | opt_linear HASH_SYM
          { Lex->part_info->part_type= HASH_PARTITION; }
          part_func {}
        | RANGE_SYM part_func
          { Lex->part_info->part_type= RANGE_PARTITION; }
        | RANGE_SYM part_column_list
          { Lex->part_info->part_type= RANGE_PARTITION; }
        | LIST_SYM part_func
          { Lex->part_info->part_type= LIST_PARTITION; }
        | LIST_SYM part_column_list
          { Lex->part_info->part_type= LIST_PARTITION; }
        ;

opt_linear:
          /* empty */ {}
        | LINEAR_SYM
          { Lex->part_info->linear_hash_ind= TRUE;}
        ;

opt_key_algo:
          /* empty */
          { Lex->part_info->key_algorithm= partition_info::KEY_ALGORITHM_NONE;}
        | ALGORITHM_SYM EQ real_ulong_num
          {
            switch ($3) {
            case 1:
              Lex->part_info->key_algorithm= partition_info::KEY_ALGORITHM_51;
              break;
            case 2:
              Lex->part_info->key_algorithm= partition_info::KEY_ALGORITHM_55;
              break;
            default:
              my_syntax_error(ER(ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
          }
        ;

part_field_list:
          /* empty */ {}
        | part_field_item_list {}
        ;

part_field_item_list:
          part_field_item {}
        | part_field_item_list ',' part_field_item {}
        ;

part_field_item:
          ident
          {
            partition_info *part_info= Lex->part_info;
            part_info->num_columns++;
            if (part_info->part_field_list.push_back($1.str))
            {
              mem_alloc_error(1);
              MYSQL_YYABORT;
            }
            if (part_info->num_columns > MAX_REF_PARTS)
            {
              my_error(ER_TOO_MANY_PARTITION_FUNC_FIELDS_ERROR, MYF(0),
                       "list of partition fields");
              MYSQL_YYABORT;
            }
          }
        ;

part_column_list:
          COLUMNS '(' part_field_list ')'
          {
            partition_info *part_info= Lex->part_info;
            part_info->column_list= TRUE;
            part_info->list_of_part_fields= TRUE;
          }
        ;


part_func:
          '(' part_func_expr ')'
          {
            partition_info *part_info= Lex->part_info;
            /*
              TODO: replace @1.cpp.end with @2.cpp.start: we don't need whitespaces at
              the beginning of the partition expression string:
            */
            if (part_info->set_part_expr(const_cast<char *>(@1.cpp.end), $2,
                                         const_cast<char *>(@2.cpp.end), FALSE))
            { MYSQL_YYABORT; }
            part_info->num_columns= 1;
            part_info->column_list= FALSE;
          }
        ;

sub_part_func:
          '(' part_func_expr ')'
          {
            /*
              TODO: replace @1.cpp.end with @2.cpp.start: we don't need whitespaces at
              the beginning of the partition expression string:
            */
            if (Lex->part_info->set_part_expr(const_cast<char *>(@1.cpp.end), $2,
                                              const_cast<char *>(@2.cpp.end), TRUE))
            { MYSQL_YYABORT; }
          }
        ;


opt_num_parts:
          /* empty */ {}
        | PARTITIONS_SYM real_ulong_num
          {
            uint num_parts= $2;
            partition_info *part_info= Lex->part_info;
            if (num_parts == 0)
            {
              my_error(ER_NO_PARTS_ERROR, MYF(0), "partitions");
              MYSQL_YYABORT;
            }

            part_info->num_parts= num_parts;
            part_info->use_default_num_partitions= FALSE;
          }
        ;

opt_sub_part:
          /* empty */ {}
        | SUBPARTITION_SYM BY opt_linear HASH_SYM sub_part_func
          { Lex->part_info->subpart_type= HASH_PARTITION; }
          opt_num_subparts {}
        | SUBPARTITION_SYM BY opt_linear KEY_SYM opt_key_algo
          '(' sub_part_field_list ')'
          {
            partition_info *part_info= Lex->part_info;
            part_info->subpart_type= HASH_PARTITION;
            part_info->list_of_subpart_fields= TRUE;
          }
          opt_num_subparts {}
        ;

sub_part_field_list:
          sub_part_field_item {}
        | sub_part_field_list ',' sub_part_field_item {}
        ;

sub_part_field_item:
          ident
          {
            partition_info *part_info= Lex->part_info;
            if (part_info->subpart_field_list.push_back($1.str))
            {
              mem_alloc_error(1);
              MYSQL_YYABORT;
            }
            if (part_info->subpart_field_list.elements > MAX_REF_PARTS)
            {
              my_error(ER_TOO_MANY_PARTITION_FUNC_FIELDS_ERROR, MYF(0),
                       "list of subpartition fields");
              MYSQL_YYABORT;
            }
          }
        ;

part_func_expr:
          bit_expr
          {
            ITEMIZE($1, &$1);

            LEX *lex= Lex;
            bool not_corr_func;
            not_corr_func= !lex->safe_to_cache_query;
            lex->safe_to_cache_query= 1;
            if (not_corr_func)
            {
              my_syntax_error(ER(ER_WRONG_EXPR_IN_PARTITION_FUNC_ERROR));
              MYSQL_YYABORT;
            }
            $$=$1;
          }
        ;

opt_num_subparts:
          /* empty */ {}
        | SUBPARTITIONS_SYM real_ulong_num
          {
            uint num_parts= $2;
            LEX *lex= Lex;
            if (num_parts == 0)
            {
              my_error(ER_NO_PARTS_ERROR, MYF(0), "subpartitions");
              MYSQL_YYABORT;
            }
            lex->part_info->num_subparts= num_parts;
            lex->part_info->use_default_num_subpartitions= FALSE;
          }
        ;

part_defs:
          /* empty */
          {
            partition_info *part_info= Lex->part_info;
            if (part_info->part_type == RANGE_PARTITION)
            {
              my_error(ER_PARTITIONS_MUST_BE_DEFINED_ERROR, MYF(0),
                       "RANGE");
              MYSQL_YYABORT;
            }
            else if (part_info->part_type == LIST_PARTITION)
            {
              my_error(ER_PARTITIONS_MUST_BE_DEFINED_ERROR, MYF(0),
                       "LIST");
              MYSQL_YYABORT;
            }
          }
        | '(' part_def_list ')'
          {
            partition_info *part_info= Lex->part_info;
            uint count_curr_parts= part_info->partitions.elements;
            if (part_info->num_parts != 0)
            {
              if (part_info->num_parts !=
                  count_curr_parts)
              {
                my_syntax_error(ER(ER_PARTITION_WRONG_NO_PART_ERROR));
                MYSQL_YYABORT;
              }
            }
            else if (count_curr_parts > 0)
            {
              part_info->num_parts= count_curr_parts;
            }
            part_info->count_curr_subparts= 0;
          }
        ;

part_def_list:
          part_definition {}
        | part_def_list ',' part_definition {}
        ;

part_definition:
          PARTITION_SYM
          {
            partition_info *part_info= Lex->part_info;
            partition_element *p_elem= new partition_element();

            if (!p_elem || part_info->partitions.push_back(p_elem))
            {
              mem_alloc_error(sizeof(partition_element));
              MYSQL_YYABORT;
            }
            p_elem->part_state= PART_NORMAL;
            part_info->curr_part_elem= p_elem;
            part_info->current_partition= p_elem;
            part_info->use_default_partitions= FALSE;
            part_info->use_default_num_partitions= FALSE;
          }
          part_name
          opt_part_values
          opt_part_options
          opt_sub_partition
          {}
        ;

part_name:
          ident
          {
            partition_info *part_info= Lex->part_info;
            partition_element *p_elem= part_info->curr_part_elem;
            if (check_string_char_length(to_lex_cstring($1), "", NAME_CHAR_LEN,
                                         system_charset_info, true))
            {
              my_error(ER_TOO_LONG_IDENT, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
            p_elem->partition_name= $1.str;
          }
        ;

opt_part_values:
          /* empty */
          {
            LEX *lex= Lex;
            partition_info *part_info= lex->part_info;
            if (! lex->is_partition_management())
            {
              if (part_info->part_type == RANGE_PARTITION)
              {
                my_error(ER_PARTITION_REQUIRES_VALUES_ERROR, MYF(0),
                         "RANGE", "LESS THAN");
                MYSQL_YYABORT;
              }
              if (part_info->part_type == LIST_PARTITION)
              {
                my_error(ER_PARTITION_REQUIRES_VALUES_ERROR, MYF(0),
                         "LIST", "IN");
                MYSQL_YYABORT;
              }
            }
            else
              part_info->part_type= HASH_PARTITION;
          }
        | VALUES LESS_SYM THAN_SYM
          {
            LEX *lex= Lex;
            partition_info *part_info= lex->part_info;
            if (! lex->is_partition_management())
            {
              if (part_info->part_type != RANGE_PARTITION)
              {
                my_error(ER_PARTITION_WRONG_VALUES_ERROR, MYF(0),
                         "RANGE", "LESS THAN");
                MYSQL_YYABORT;
              }
            }
            else
              part_info->part_type= RANGE_PARTITION;
          }
          part_func_max {}
        | VALUES IN_SYM
          {
            LEX *lex= Lex;
            partition_info *part_info= lex->part_info;
            if (! lex->is_partition_management())
            {
              if (part_info->part_type != LIST_PARTITION)
              {
                my_error(ER_PARTITION_WRONG_VALUES_ERROR, MYF(0),
                               "LIST", "IN");
                MYSQL_YYABORT;
              }
            }
            else
              part_info->part_type= LIST_PARTITION;
          }
          part_values_in {}
        ;

part_func_max:
          MAX_VALUE_SYM
          {
            partition_info *part_info= Lex->part_info;

            if (part_info->num_columns &&
                part_info->num_columns != 1U)
            {
              part_info->print_debug("Kilroy II", NULL);
              my_syntax_error(ER(ER_PARTITION_COLUMN_LIST_ERROR));
              MYSQL_YYABORT;
            }
            else
              part_info->num_columns= 1U;
            if (part_info->init_column_part())
            {
              MYSQL_YYABORT;
            }
            if (part_info->add_max_value())
            {
              MYSQL_YYABORT;
            }
          }
        | part_value_item {}
        ;

part_values_in:
          part_value_item
          {
            LEX *lex= Lex;
            partition_info *part_info= lex->part_info;
            part_info->print_debug("part_values_in: part_value_item", NULL);

            if (part_info->num_columns != 1U)
            {
              if (!lex->is_partition_management() ||
                  part_info->num_columns == 0 ||
                  part_info->num_columns > MAX_REF_PARTS)
              {
                part_info->print_debug("Kilroy III", NULL);
                my_syntax_error(ER(ER_PARTITION_COLUMN_LIST_ERROR));
                MYSQL_YYABORT;
              }
              /*
                Reorganize the current large array into a list of small
                arrays with one entry in each array. This can happen
                in the first partition of an ALTER TABLE statement where
                we ADD or REORGANIZE partitions. Also can only happen
                for LIST [COLUMNS] partitions.
              */
              if (part_info->reorganize_into_single_field_col_val())
              {
                MYSQL_YYABORT;
              }
            }
          }
        | '(' part_value_list ')'
          {
            partition_info *part_info= Lex->part_info;
            if (part_info->num_columns < 2U)
            {
              my_syntax_error(ER(ER_ROW_SINGLE_PARTITION_FIELD_ERROR));
              MYSQL_YYABORT;
            }
          }
        ;

part_value_list:
          part_value_item {}
        | part_value_list ',' part_value_item {}
        ;

part_value_item:
          '('
          {
            partition_info *part_info= Lex->part_info;
            part_info->print_debug("( part_value_item", NULL);
            /* Initialisation code needed for each list of value expressions */
            if (!(part_info->part_type == LIST_PARTITION &&
                  part_info->num_columns == 1U) &&
                 part_info->init_column_part())
            {
              MYSQL_YYABORT;
            }
          }
          part_value_item_list {}
          ')'
          {
            partition_info *part_info= Lex->part_info;
            part_info->print_debug(") part_value_item", NULL);
            if (part_info->num_columns == 0)
              part_info->num_columns= part_info->curr_list_object;
            if (part_info->num_columns != part_info->curr_list_object)
            {
              /*
                All value items lists must be of equal length, in some cases
                which is covered by the above if-statement we don't know yet
                how many columns is in the partition so the assignment above
                ensures that we only report errors when we know we have an
                error.
              */
              part_info->print_debug("Kilroy I", NULL);
              my_syntax_error(ER(ER_PARTITION_COLUMN_LIST_ERROR));
              MYSQL_YYABORT;
            }
            part_info->curr_list_object= 0;
          }
        ;

part_value_item_list:
          part_value_expr_item {}
        | part_value_item_list ',' part_value_expr_item {}
        ;

part_value_expr_item:
          MAX_VALUE_SYM
          {
            partition_info *part_info= Lex->part_info;
            if (part_info->part_type == LIST_PARTITION)
            {
              my_syntax_error(ER(ER_MAXVALUE_IN_VALUES_IN));
              MYSQL_YYABORT;
            }
            if (part_info->add_max_value())
            {
              MYSQL_YYABORT;
            }
          }
        | bit_expr
          {
            ITEMIZE($1, &$1);

            LEX *lex= Lex;
            partition_info *part_info= lex->part_info;
            Item *part_expr= $1;

            if (!lex->safe_to_cache_query)
            {
              my_syntax_error(ER(ER_WRONG_EXPR_IN_PARTITION_FUNC_ERROR));
              MYSQL_YYABORT;
            }
            if (part_info->add_column_list_value(YYTHD, part_expr))
            {
              MYSQL_YYABORT;
            }
          }
        ;


opt_sub_partition:
          /* empty */
          {
            partition_info *part_info= Lex->part_info;
            if (part_info->num_subparts != 0 &&
                !part_info->use_default_subpartitions)
            {
              /*
                We come here when we have defined subpartitions on the first
                partition but not on all the subsequent partitions.
              */
              my_syntax_error(ER(ER_PARTITION_WRONG_NO_SUBPART_ERROR));
              MYSQL_YYABORT;
            }
          }
        | '(' sub_part_list ')'
          {
            partition_info *part_info= Lex->part_info;
            if (part_info->num_subparts != 0)
            {
              if (part_info->num_subparts !=
                  part_info->count_curr_subparts)
              {
                my_syntax_error(ER(ER_PARTITION_WRONG_NO_SUBPART_ERROR));
                MYSQL_YYABORT;
              }
            }
            else if (part_info->count_curr_subparts > 0)
            {
              if (part_info->partitions.elements > 1)
              {
                my_syntax_error(ER(ER_PARTITION_WRONG_NO_SUBPART_ERROR));
                MYSQL_YYABORT;
              }
              part_info->num_subparts= part_info->count_curr_subparts;
            }
            part_info->count_curr_subparts= 0;
          }
        ;

sub_part_list:
          sub_part_definition {}
        | sub_part_list ',' sub_part_definition {}
        ;

sub_part_definition:
          SUBPARTITION_SYM
          {
            partition_info *part_info= Lex->part_info;
            partition_element *curr_part= part_info->current_partition;
            partition_element *sub_p_elem= new partition_element(curr_part);
            if (part_info->use_default_subpartitions &&
                part_info->partitions.elements >= 2)
            {
              /*
                create table t1 (a int)
                partition by list (a) subpartition by hash (a)
                (partition p0 values in (1),
                 partition p1 values in (2) subpartition sp11);
                causes use to arrive since we are on the second
                partition, but still use_default_subpartitions
                is set. When we come here we're processing at least
                the second partition (the current partition processed
                have already been put into the partitions list.
              */
              my_syntax_error(ER(ER_PARTITION_WRONG_NO_SUBPART_ERROR));
              MYSQL_YYABORT;
            }
            if (!sub_p_elem ||
             curr_part->subpartitions.push_back(sub_p_elem))
            {
              mem_alloc_error(sizeof(partition_element));
              MYSQL_YYABORT;
            }
            part_info->curr_part_elem= sub_p_elem;
            part_info->use_default_subpartitions= FALSE;
            part_info->use_default_num_subpartitions= FALSE;
            part_info->count_curr_subparts++;
          }
          sub_name opt_part_options {}
        ;

sub_name:
          ident_or_text
          {
            if (check_string_char_length(to_lex_cstring($1), "", NAME_CHAR_LEN,
                                         system_charset_info, true))
            {
              my_error(ER_TOO_LONG_IDENT, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
            Lex->part_info->curr_part_elem->partition_name= $1.str;
          }
        ;

opt_part_options:
         /* empty */ {}
       | opt_part_option_list {}
       ;

opt_part_option_list:
         opt_part_option_list opt_part_option {}
       | opt_part_option {}
       ;

opt_part_option:
          TABLESPACE_SYM opt_equal ident
          { Lex->part_info->curr_part_elem->tablespace_name= $3.str; }
        | opt_storage ENGINE_SYM opt_equal storage_engines
          {
            partition_info *part_info= Lex->part_info;
            part_info->curr_part_elem->engine_type= $4;
            part_info->default_engine_type= $4;
          }
        | NODEGROUP_SYM opt_equal real_ulong_num
          { Lex->part_info->curr_part_elem->nodegroup_id= (uint16) $3; }
        | MAX_ROWS opt_equal real_ulonglong_num
          { Lex->part_info->curr_part_elem->part_max_rows= (ha_rows) $3; }
        | MIN_ROWS opt_equal real_ulonglong_num
          { Lex->part_info->curr_part_elem->part_min_rows= (ha_rows) $3; }
        | DATA_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys
          { Lex->part_info->curr_part_elem->data_file_name= $4.str; }
        | INDEX_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys
          { Lex->part_info->curr_part_elem->index_file_name= $4.str; }
        | COMMENT_SYM opt_equal TEXT_STRING_sys
          { Lex->part_info->curr_part_elem->part_comment= $3.str; }
        ;

/*
 End of partition parser part
*/

create_select:
          SELECT_SYM select_options select_item_list table_expression
          {
            $$= NEW_PTN PT_create_select($1, $2, $3, $4);
          }
        ;

opt_as:
          /* empty */ {}
        | AS {}
        ;

opt_create_database_options:
          /* empty */ {}
        | create_database_options {}
        ;

create_database_options:
          create_database_option {}
        | create_database_options create_database_option {}
        ;

create_database_option:
          default_collation {}
        | default_charset {}
        ;

opt_table_options:
          /* empty */ { $$= 0; }
        | table_options  { $$= $1;}
        ;

table_options:
          table_option { $$=$1; }
        | table_option table_options { $$= $1 | $2; }
        ;

table_option:
          TEMPORARY { $$=HA_LEX_CREATE_TMP_TABLE; }
        ;

opt_if_not_exists:
          /* empty */ { $$= 0; }
        | IF not EXISTS { $$=HA_LEX_CREATE_IF_NOT_EXISTS; }
        ;

opt_create_table_options:
          /* empty */
        | create_table_options
        ;

create_table_options_space_separated:
          create_table_option
        | create_table_option create_table_options_space_separated
        ;

create_table_options:
          create_table_option
        | create_table_option     create_table_options
        | create_table_option ',' create_table_options
        ;

create_table_option:
          ENGINE_SYM opt_equal storage_engines
          {
            Lex->create_info.db_type= $3;
            Lex->create_info.used_fields|= HA_CREATE_USED_ENGINE;
          }
        | MAX_ROWS opt_equal ulonglong_num
          {
            Lex->create_info.max_rows= $3;
            Lex->create_info.used_fields|= HA_CREATE_USED_MAX_ROWS;
          }
        | MIN_ROWS opt_equal ulonglong_num
          {
            Lex->create_info.min_rows= $3;
            Lex->create_info.used_fields|= HA_CREATE_USED_MIN_ROWS;
          }
        | AVG_ROW_LENGTH opt_equal ulong_num
          {
            Lex->create_info.avg_row_length=$3;
            Lex->create_info.used_fields|= HA_CREATE_USED_AVG_ROW_LENGTH;
          }
        | PASSWORD opt_equal TEXT_STRING_sys
          {
            Lex->create_info.password=$3.str;
            Lex->create_info.used_fields|= HA_CREATE_USED_PASSWORD;
          }
        | COMMENT_SYM opt_equal TEXT_STRING_sys
          {
            Lex->create_info.comment=$3;
            Lex->create_info.used_fields|= HA_CREATE_USED_COMMENT;
          }
        | COMPRESSION_SYM opt_equal TEXT_STRING_sys
	  {
            Lex->create_info.used_fields|= HA_CREATE_USED_COMPRESS;
            Lex->create_info.compress= $3;
	  }
        | ENCRYPTION_SYM opt_equal TEXT_STRING_sys
	  {
            Lex->create_info.used_fields|= HA_CREATE_USED_ENCRYPT;
            Lex->create_info.encrypt_type= $3;
	  }
        | AUTO_INC opt_equal ulonglong_num
          {
            Lex->create_info.auto_increment_value=$3;
            Lex->create_info.used_fields|= HA_CREATE_USED_AUTO;
          }
        | PACK_KEYS_SYM opt_equal ulong_num
          {
            switch($3) {
            case 0:
                Lex->create_info.table_options|= HA_OPTION_NO_PACK_KEYS;
                break;
            case 1:
                Lex->create_info.table_options|= HA_OPTION_PACK_KEYS;
                break;
            default:
                my_syntax_error(ER(ER_SYNTAX_ERROR));
                MYSQL_YYABORT;
            }
            Lex->create_info.used_fields|= HA_CREATE_USED_PACK_KEYS;
          }
        | PACK_KEYS_SYM opt_equal DEFAULT
          {
            Lex->create_info.table_options&=
              ~(HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS);
            Lex->create_info.used_fields|= HA_CREATE_USED_PACK_KEYS;
          }
        | STATS_AUTO_RECALC_SYM opt_equal ulong_num
          {
            switch($3) {
            case 0:
                Lex->create_info.stats_auto_recalc= HA_STATS_AUTO_RECALC_OFF;
                break;
            case 1:
                Lex->create_info.stats_auto_recalc= HA_STATS_AUTO_RECALC_ON;
                break;
            default:
                my_syntax_error(ER(ER_SYNTAX_ERROR));
                MYSQL_YYABORT;
            }
            Lex->create_info.used_fields|= HA_CREATE_USED_STATS_AUTO_RECALC;
          }
        | STATS_AUTO_RECALC_SYM opt_equal DEFAULT
          {
            Lex->create_info.stats_auto_recalc= HA_STATS_AUTO_RECALC_DEFAULT;
            Lex->create_info.used_fields|= HA_CREATE_USED_STATS_AUTO_RECALC;
          }
        | STATS_PERSISTENT_SYM opt_equal ulong_num
          {
            switch($3) {
            case 0:
                Lex->create_info.table_options|= HA_OPTION_NO_STATS_PERSISTENT;
                break;
            case 1:
                Lex->create_info.table_options|= HA_OPTION_STATS_PERSISTENT;
                break;
            default:
                my_syntax_error(ER(ER_SYNTAX_ERROR));
                MYSQL_YYABORT;
            }
            Lex->create_info.used_fields|= HA_CREATE_USED_STATS_PERSISTENT;
          }
        | STATS_PERSISTENT_SYM opt_equal DEFAULT
          {
            Lex->create_info.table_options&=
              ~(HA_OPTION_STATS_PERSISTENT | HA_OPTION_NO_STATS_PERSISTENT);
            Lex->create_info.used_fields|= HA_CREATE_USED_STATS_PERSISTENT;
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
              my_syntax_error(ER(ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            Lex->create_info.stats_sample_pages=$3;
            Lex->create_info.used_fields|= HA_CREATE_USED_STATS_SAMPLE_PAGES;
          }
        | STATS_SAMPLE_PAGES_SYM opt_equal DEFAULT
          {
            Lex->create_info.stats_sample_pages=0;
            Lex->create_info.used_fields|= HA_CREATE_USED_STATS_SAMPLE_PAGES;
          }
        | CHECKSUM_SYM opt_equal ulong_num
          {
            Lex->create_info.table_options|= $3 ? HA_OPTION_CHECKSUM : HA_OPTION_NO_CHECKSUM;
            Lex->create_info.used_fields|= HA_CREATE_USED_CHECKSUM;
          }
        | TABLE_CHECKSUM_SYM opt_equal ulong_num
          {
             Lex->create_info.table_options|= $3 ? HA_OPTION_CHECKSUM : HA_OPTION_NO_CHECKSUM;
             Lex->create_info.used_fields|= HA_CREATE_USED_CHECKSUM;
          }
        | DELAY_KEY_WRITE_SYM opt_equal ulong_num
          {
            Lex->create_info.table_options|= $3 ? HA_OPTION_DELAY_KEY_WRITE : HA_OPTION_NO_DELAY_KEY_WRITE;
            Lex->create_info.used_fields|= HA_CREATE_USED_DELAY_KEY_WRITE;
          }
        | ROW_FORMAT_SYM opt_equal row_types
          {
            Lex->create_info.row_type= $3;
            Lex->create_info.used_fields|= HA_CREATE_USED_ROW_FORMAT;
          }
        | UNION_SYM opt_equal
          {
            Lex->select_lex->table_list.save_and_clear(&Lex->save_list);
          }
          '(' opt_table_list ')'
          {
            /*
              Move the union list to the merge_list and exclude its tables
              from the global list.
            */
            LEX *lex=Lex;
            lex->create_info.merge_list= lex->select_lex->table_list;
            lex->select_lex->table_list= lex->save_list;
            /*
              When excluding union list from the global list we assume that
              elements of the former immediately follow elements which represent
              table being created/altered and parent tables.
            */
            TABLE_LIST *last_non_sel_table= lex->create_last_non_select_table;
            DBUG_ASSERT(last_non_sel_table->next_global ==
                        lex->create_info.merge_list.first);
            last_non_sel_table->next_global= 0;
            Lex->query_tables_last= &last_non_sel_table->next_global;

            lex->create_info.used_fields|= HA_CREATE_USED_UNION;
          }
        | default_charset
        | default_collation
        | INSERT_METHOD opt_equal merge_insert_types
          {
            Lex->create_info.merge_insert_method= $3;
            Lex->create_info.used_fields|= HA_CREATE_USED_INSERT_METHOD;
          }
        | DATA_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys
          {
            Lex->create_info.data_file_name= $4.str;
            Lex->create_info.used_fields|= HA_CREATE_USED_DATADIR;
          }
        | INDEX_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys
          {
            Lex->create_info.index_file_name= $4.str;
            Lex->create_info.used_fields|= HA_CREATE_USED_INDEXDIR;
          }
        | TABLESPACE_SYM opt_equal ident
          {
            Lex->create_info.tablespace= $3.str;
            Lex->create_info.used_fields|= HA_CREATE_USED_TABLESPACE;
          }
        | STORAGE_SYM DISK_SYM
          {Lex->create_info.storage_media= HA_SM_DISK;}
        | STORAGE_SYM MEMORY_SYM
          {Lex->create_info.storage_media= HA_SM_MEMORY;}
        | CONNECTION_SYM opt_equal TEXT_STRING_sys
          {
            Lex->create_info.connect_string.str= $3.str;
            Lex->create_info.connect_string.length= $3.length;
            Lex->create_info.used_fields|= HA_CREATE_USED_CONNECTION;
          }
        | KEY_BLOCK_SIZE opt_equal ulong_num
          {
            Lex->create_info.used_fields|= HA_CREATE_USED_KEY_BLOCK_SIZE;
            Lex->create_info.key_block_size= $3;
          }
        ;

default_charset:
          opt_default charset opt_equal charset_name_or_default
          {
            HA_CREATE_INFO *cinfo= &Lex->create_info;
            if ((cinfo->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
                 cinfo->default_table_charset && $4 &&
                 !my_charset_same(cinfo->default_table_charset,$4))
            {
              my_error(ER_CONFLICTING_DECLARATIONS, MYF(0),
                       "CHARACTER SET ", cinfo->default_table_charset->csname,
                       "CHARACTER SET ", $4->csname);
              MYSQL_YYABORT;
            }
            Lex->create_info.default_table_charset= $4;
            Lex->create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
          }
        ;

default_collation:
          opt_default COLLATE_SYM opt_equal collation_name_or_default
          {
            HA_CREATE_INFO *cinfo= &Lex->create_info;
            if ((cinfo->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
                 cinfo->default_table_charset && $4 &&
                 !($4= merge_charset_and_collation(cinfo->default_table_charset,
                                                   $4)))
            {
              MYSQL_YYABORT;
            }

            Lex->create_info.default_table_charset= $4;
            Lex->create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
          }
        ;

storage_engines:
          ident_or_text
          {
            THD *thd= YYTHD;
            plugin_ref plugin=
              ha_resolve_by_name(thd, &$1,
                thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE);

            if (plugin)
              $$= plugin_data<handlerton*>(plugin);
            else
            {
              if (thd->variables.sql_mode & MODE_NO_ENGINE_SUBSTITUTION)
              {
                my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), $1.str);
                MYSQL_YYABORT;
              }
              $$= 0;
              push_warning_printf(thd, Sql_condition::SL_WARNING,
                                  ER_UNKNOWN_STORAGE_ENGINE,
                                  ER(ER_UNKNOWN_STORAGE_ENGINE),
                                  $1.str);
            }
          }
        ;

known_storage_engines:
          ident_or_text
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            plugin_ref plugin=
              ha_resolve_by_name(thd, &$1,
                lex->create_info.options & HA_LEX_CREATE_TMP_TABLE);
            if (plugin)
              $$= plugin_data<handlerton*>(plugin);
            else
            {
              my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
          }
        ;

row_types:
          DEFAULT        { $$= ROW_TYPE_DEFAULT; }
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
        | REAL {$$ = (int) REAL_RESULT; }
        | DECIMAL_SYM {$$ = (int) DECIMAL_RESULT; }
        | INT_SYM {$$ = (int) INT_RESULT; }
        ;


create_field_list:
        field_list
        {
          Lex->create_last_non_select_table= Lex->last_table();
        }
        ;

field_list:
          field_list_item
        | field_list ',' field_list_item
        ;

field_list_item:
          column_def
        | key_def
        ;

column_def:
          field_spec opt_check_constraint
        | field_spec references
          {
            Lex->col_list.empty(); /* Alloced by sql_alloc */
          }
        ;

key_def:
          normal_key_type opt_ident key_alg '(' key_list ')' normal_key_options
          {
            if (add_create_index (Lex, $1, $2))
              MYSQL_YYABORT;
          }
        | fulltext opt_key_or_index opt_ident init_key_options
            '(' key_list ')' fulltext_key_options
          {
            if (add_create_index (Lex, $1, $3))
              MYSQL_YYABORT;
          }
        | spatial opt_key_or_index opt_ident init_key_options
            '(' key_list ')' spatial_key_options
          {
            if (add_create_index (Lex, $1, $3))
              MYSQL_YYABORT;
          }
        | opt_constraint constraint_key_type opt_ident key_alg
          '(' key_list ')' normal_key_options
          {
            if (add_create_index (Lex, $2, $3.str ? $3 : $1))
              MYSQL_YYABORT;
          }
        | opt_constraint FOREIGN KEY_SYM opt_ident '(' key_list ')' references
          {
            LEX *lex=Lex;
            Key *key= new Foreign_key($4.str ? $4 : $1, lex->col_list,
                                      $8->db,
                                      $8->table,
                                      lex->ref_list,
                                      lex->fk_delete_opt,
                                      lex->fk_update_opt,
                                      lex->fk_match_option);
            if (key == NULL)
              MYSQL_YYABORT;
            lex->alter_info.key_list.push_back(key);
            if (add_create_index (lex, KEYTYPE_MULTIPLE, $1.str ? $1 : $4,
                                  &default_key_create_info, 1))
              MYSQL_YYABORT;
            /* Only used for ALTER TABLE. Ignored otherwise. */
            lex->alter_info.flags|= Alter_info::ADD_FOREIGN_KEY;
          }
        | opt_constraint check_constraint
          {
            Lex->col_list.empty(); /* Alloced by sql_alloc */
          }
        ;

opt_check_constraint:
          /* empty */
        | check_constraint
        ;

check_constraint:
          CHECK_SYM '(' expr ')'
          {
            ITEMIZE($3, &$3);
          }

        ;

opt_constraint:
          /* empty */ { $$= null_lex_str; }
        | constraint { $$= $1; }
        ;

constraint:
          CONSTRAINT opt_ident { $$=$2; }
        ;

field_spec:
          field_ident
          {
            LEX *lex=Lex;
            lex->length=lex->dec=0;
            lex->type=0;
            lex->default_value= lex->on_update_value= 0;
            lex->comment=null_lex_str;
            lex->charset=NULL;
            lex->gcol_info= 0;
          }
          field_def
          {
            LEX *lex=Lex;
            if (add_field_to_list(lex->thd, &$1, (enum enum_field_types) $3,
                                  lex->length,lex->dec,lex->type,
                                  lex->default_value, lex->on_update_value,
                                  &lex->comment,
                                  lex->change,&lex->interval_list,lex->charset,
                                  lex->uint_geom_type,
                                  lex->gcol_info))
              MYSQL_YYABORT;
          }
        ;

field_def:
          type opt_attribute {}
        | type opt_collate_explicit opt_generated_always
          AS '(' generated_column_func ')' opt_stored_attribute
          opt_gcol_attribute_list
          {
            $$= $1;
            if (Lex->charset)
            {
              Lex->charset= merge_charset_and_collation(Lex->charset, $2);
              if (Lex->charset == NULL)
                MYSQL_YYABORT;
            }
            else
              Lex->charset= $2;
            Lex->gcol_info->set_field_type((enum enum_field_types) $$);
          }
        ;

opt_generated_always:
          /* empty */
        | GENERATED ALWAYS_SYM
        ;

opt_gcol_attribute_list:
          /* empty */
        | gcol_attribute_list
        ;

gcol_attribute_list:
          gcol_attribute_list gcol_attribute
        | gcol_attribute
        ;

gcol_attribute:
          UNIQUE_SYM
          {
            LEX *lex=Lex;
            lex->type|= UNIQUE_FLAG;
            lex->alter_info.flags|= Alter_info::ALTER_ADD_INDEX;
          }
        | UNIQUE_SYM KEY_SYM
          {
            LEX *lex=Lex;
            lex->type|= UNIQUE_KEY_FLAG;
            lex->alter_info.flags|= Alter_info::ALTER_ADD_INDEX;
          }
        | COMMENT_SYM TEXT_STRING_sys { Lex->comment= $2; }
        | not NULL_SYM { Lex->type|= NOT_NULL_FLAG; }
        | NULL_SYM
        | opt_primary KEY_SYM
          {
            LEX *lex=Lex;
            lex->type|= PRI_KEY_FLAG | NOT_NULL_FLAG;
            lex->alter_info.flags|= Alter_info::ALTER_ADD_INDEX;
          }
        ;

opt_stored_attribute:
          /* empty */
        | VIRTUAL_SYM
        | STORED_SYM
          {
            Lex->gcol_info->set_field_stored(TRUE);
          }
        ;

parse_gcol_expr:
          PARSE_GCOL_EXPR_SYM '(' generated_column_func ')'
          {
            /* 
              "PARSE_GCOL_EXPR" can only be used by the SQL server
              when reading a '*.frm' file.
              Prevent the end user from invoking this command.
            */
            if (!Lex->parse_gcol_expr)
            {
              YYTHD->parse_error_at(@1, ER_THD(YYTHD, ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
          }
        ;

generated_column_func:
          expr
          {
            Lex->gcol_info= new Generated_column();
            if (!Lex->gcol_info)
            {
              mem_alloc_error(sizeof(Generated_column));
              MYSQL_YYABORT;
            }
            ITEMIZE($1, &$1);
            uint expr_len= (uint)@1.cpp.length();
            Lex->gcol_info->dup_expr_str(YYTHD->mem_root, @1.cpp.start, expr_len);
            Lex->gcol_info->expr_item= $1;
            /*
              @todo: problems:
              - here we have a call to the constructor of
              Generated_column, which takes no argument and builds a
              non-functional object
              - then we fill it member by member; either by assignment to
              public members (!) or by call to a public setter. Both these
              techniques allow changing, at any future point in time, vital
              properties of the object which should rather be constant.
              Class should rather have a constructor which takes arguments,
              sets members, and members should be constant after that.
              This would also get rid of some setters like set_field_stored();
            */
          }
        ;

type:
          int_type opt_field_length field_options
          {
            Lex->length= const_cast<char *>($2);
            $$=$1;
          }
        | real_type opt_precision field_options
          {
            $$=$1;
            Lex->length= const_cast<char *>($2.length);
            Lex->dec= const_cast<char *>($2.dec);
          }
        | FLOAT_SYM float_options field_options
          {
            Lex->length= const_cast<char *>($2.length);
            Lex->dec= const_cast<char *>($2.dec);
            $$=MYSQL_TYPE_FLOAT;
          }
        | BIT_SYM
          {
            Lex->length= (char*) "1";
            $$=MYSQL_TYPE_BIT;
          }
        | BIT_SYM field_length
          {
            Lex->length= const_cast<char *>($2);
            $$=MYSQL_TYPE_BIT;
          }
        | BOOL_SYM
          {
            Lex->length= (char*) "1";
            $$=MYSQL_TYPE_TINY;
          }
        | BOOLEAN_SYM
          {
            Lex->length= (char*) "1";
            $$=MYSQL_TYPE_TINY;
          }
        | char field_length opt_binary
          {
            Lex->length= const_cast<char *>($2);
            Lex->charset= $3.charset;
            Lex->type|= $3.type_flags;
            $$=MYSQL_TYPE_STRING;
          }
        | char opt_binary
          {
            Lex->length= (char*) "1";
            Lex->charset= $2.charset;
            Lex->type|= $2.type_flags;
            $$=MYSQL_TYPE_STRING;
          }
        | nchar field_length opt_bin_mod
          {
            Lex->length= const_cast<char *>($2);
            Lex->type|= $3;
            $$=MYSQL_TYPE_STRING;
            Lex->charset=national_charset_info;
          }
        | nchar opt_bin_mod
          {
            Lex->length= (char*) "1";
            Lex->type|= $2;
            $$=MYSQL_TYPE_STRING;
            Lex->charset=national_charset_info;
          }
        | BINARY field_length
          {
            Lex->length= const_cast<char *>($2);
            Lex->charset=&my_charset_bin;
            $$=MYSQL_TYPE_STRING;
          }
        | BINARY
          {
            Lex->length= (char*) "1";
            Lex->charset=&my_charset_bin;
            $$=MYSQL_TYPE_STRING;
          }
        | varchar field_length opt_binary
          {
            Lex->length= const_cast<char *>($2);
            Lex->charset= $3.charset;
            Lex->type|= $3.type_flags;
            $$= MYSQL_TYPE_VARCHAR;
          }
        | nvarchar field_length opt_bin_mod
          {
            Lex->length= const_cast<char *>($2);
            Lex->type|= $3;
            $$= MYSQL_TYPE_VARCHAR;
            Lex->charset=national_charset_info;
          }
        | VARBINARY field_length
          {
            Lex->length= const_cast<char *>($2);
            Lex->charset=&my_charset_bin;
            $$= MYSQL_TYPE_VARCHAR;
          }
        | YEAR_SYM opt_field_length field_options
          {
            Lex->length= const_cast<char *>($2);
            if (Lex->length)
            {
              errno= 0;
              ulong length= strtoul(Lex->length, NULL, 10);
              if (errno != 0 || length != 4)
              {
                /* Only support length is 4 */
                my_error(ER_INVALID_YEAR_COLUMN_LENGTH, MYF(0), "YEAR");
                MYSQL_YYABORT;
              }
            }
            $$=MYSQL_TYPE_YEAR;
          }
        | DATE_SYM
          { $$=MYSQL_TYPE_DATE; }
        | TIME_SYM type_datetime_precision
          {
            Lex->dec= const_cast<char *>($2);
            $$= MYSQL_TYPE_TIME2;
          }
        | TIMESTAMP type_datetime_precision
          {
            Lex->dec= const_cast<char *>($2);
            if (YYTHD->variables.sql_mode & MODE_MAXDB)
            {
              push_warning(current_thd, Sql_condition::SL_WARNING,
                  WARN_DEPRECATED_MAXDB_SQL_MODE_FOR_TIMESTAMP,
                  ER_THD(YYTHD, WARN_DEPRECATED_MAXDB_SQL_MODE_FOR_TIMESTAMP));
              $$=MYSQL_TYPE_DATETIME2;
            }
            else
            {
              /*
                Unlike other types TIMESTAMP fields are NOT NULL by default.
                This behavior is deprecated now.
              */
              if (!YYTHD->variables.explicit_defaults_for_timestamp)
                Lex->type|= NOT_NULL_FLAG;
              /*
                To flag the current statement as dependent for binary
                logging on the session var. Extra copying to Lex is
                done in case prepared stmt.
              */
              Lex->binlog_need_explicit_defaults_ts=
                YYTHD->binlog_need_explicit_defaults_ts= true;

              $$=MYSQL_TYPE_TIMESTAMP2;
            }
          }
        | DATETIME type_datetime_precision
          {
            Lex->dec= const_cast<char *>($2);
            $$= MYSQL_TYPE_DATETIME2;
          }
        | TINYBLOB
          {
            Lex->charset=&my_charset_bin;
            $$=MYSQL_TYPE_TINY_BLOB;
          }
        | BLOB_SYM opt_field_length
          {
            Lex->length= const_cast<char *>($2);
            Lex->charset=&my_charset_bin;
            $$=MYSQL_TYPE_BLOB;
          }
        | spatial_type
          {
            Lex->charset=&my_charset_bin;
            Lex->uint_geom_type= (uint)$1;
            $$=MYSQL_TYPE_GEOMETRY;
          }
        | MEDIUMBLOB
          {
            Lex->charset=&my_charset_bin;
            $$=MYSQL_TYPE_MEDIUM_BLOB;
          }
        | LONGBLOB
          {
            Lex->charset=&my_charset_bin;
            $$=MYSQL_TYPE_LONG_BLOB;
          }
        | LONG_SYM VARBINARY
          {
            Lex->charset=&my_charset_bin;
            $$=MYSQL_TYPE_MEDIUM_BLOB;
          }
        | LONG_SYM varchar opt_binary
          {
            Lex->charset= $3.charset;
            Lex->type|= $3.type_flags;
            $$=MYSQL_TYPE_MEDIUM_BLOB;
          }
        | TINYTEXT opt_binary
          {
            Lex->charset= $2.charset;
            Lex->type|= $2.type_flags;
            $$=MYSQL_TYPE_TINY_BLOB;
          }
        | TEXT_SYM opt_field_length opt_binary
          {
            Lex->length= const_cast<char *>($2);
            Lex->charset= $3.charset;
            Lex->type|= $3.type_flags;
            $$=MYSQL_TYPE_BLOB;
          }
        | MEDIUMTEXT opt_binary
          {
            Lex->charset= $2.charset;
            Lex->type|= $2.type_flags;
            $$=MYSQL_TYPE_MEDIUM_BLOB;
          }
        | LONGTEXT opt_binary
          {
            Lex->charset= $2.charset;
            Lex->type|= $2.type_flags;
            $$=MYSQL_TYPE_LONG_BLOB;
          }
        | DECIMAL_SYM float_options field_options
          {
            Lex->length= const_cast<char *>($2.length);
            Lex->dec= const_cast<char *>($2.dec);
            $$=MYSQL_TYPE_NEWDECIMAL;
          }
        | NUMERIC_SYM float_options field_options
          {
            Lex->length= const_cast<char *>($2.length);
            Lex->dec= const_cast<char *>($2.dec);
            $$=MYSQL_TYPE_NEWDECIMAL;
          }
        | FIXED_SYM float_options field_options
          {
            Lex->length= const_cast<char *>($2.length);
            Lex->dec= const_cast<char *>($2.dec);
            $$=MYSQL_TYPE_NEWDECIMAL;
          }
        | ENUM
          {Lex->interval_list.empty();}
          '(' string_list ')' opt_binary
          {
            Lex->charset= $6.charset;
            Lex->type|= $6.type_flags;
            $$=MYSQL_TYPE_ENUM;
          }
        | SET
          { Lex->interval_list.empty();}
          '(' string_list ')' opt_binary
          {
            Lex->charset= $6.charset;
            Lex->type|= $6.type_flags;
            $$=MYSQL_TYPE_SET;
          }
        | LONG_SYM opt_binary
          {
            Lex->charset= $2.charset;
            Lex->type|= $2.type_flags;
            $$=MYSQL_TYPE_MEDIUM_BLOB;
          }
        | SERIAL_SYM
          {
            $$=MYSQL_TYPE_LONGLONG;
            Lex->type|= (AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNSIGNED_FLAG |
              UNIQUE_FLAG);
          }
        | JSON_SYM
          {
            Lex->charset=&my_charset_bin;
            $$=MYSQL_TYPE_JSON;
          }
        ;

spatial_type:
          GEOMETRY_SYM        { $$= Field::GEOM_GEOMETRY; }
        | GEOMETRYCOLLECTION  { $$= Field::GEOM_GEOMETRYCOLLECTION; }
        | POINT_SYM
          {
            Lex->length= const_cast<char*>(STRINGIFY_ARG
                                           (MAX_LEN_GEOM_POINT_FIELD));
            $$= Field::GEOM_POINT;
          }
        | MULTIPOINT          { $$= Field::GEOM_MULTIPOINT; }
        | LINESTRING          { $$= Field::GEOM_LINESTRING; }
        | MULTILINESTRING     { $$= Field::GEOM_MULTILINESTRING; }
        | POLYGON             { $$= Field::GEOM_POLYGON; }
        | MULTIPOLYGON        { $$= Field::GEOM_MULTIPOLYGON; }
        ;

char:
          CHAR_SYM {}
        ;

nchar:
          NCHAR_SYM {}
        | NATIONAL_SYM CHAR_SYM {}
        ;

varchar:
          char VARYING {}
        | VARCHAR {}
        ;

nvarchar:
          NATIONAL_SYM VARCHAR {}
        | NVARCHAR_SYM {}
        | NCHAR_SYM VARCHAR {}
        | NATIONAL_SYM CHAR_SYM VARYING {}
        | NCHAR_SYM VARYING {}
        ;

int_type:
          INT_SYM   { $$=MYSQL_TYPE_LONG; }
        | TINYINT   { $$=MYSQL_TYPE_TINY; }
        | SMALLINT  { $$=MYSQL_TYPE_SHORT; }
        | MEDIUMINT { $$=MYSQL_TYPE_INT24; }
        | BIGINT    { $$=MYSQL_TYPE_LONGLONG; }
        ;

real_type:
          REAL
          {
            $$= YYTHD->variables.sql_mode & MODE_REAL_AS_FLOAT ?
              MYSQL_TYPE_FLOAT : MYSQL_TYPE_DOUBLE;
          }
        | DOUBLE_SYM
          { $$=MYSQL_TYPE_DOUBLE; }
        | DOUBLE_SYM PRECISION
          { $$=MYSQL_TYPE_DOUBLE; }
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
          /* empty */ {}
        | field_opt_list {}
        ;

field_opt_list:
          field_opt_list field_option {}
        | field_option {}
        ;

field_option:
          SIGNED_SYM {}
        | UNSIGNED { Lex->type|= UNSIGNED_FLAG;}
        | ZEROFILL { Lex->type|= UNSIGNED_FLAG | ZEROFILL_FLAG; }
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

opt_attribute:
          /* empty */ {}
        | opt_attribute_list {}
        ;

opt_attribute_list:
          opt_attribute_list attribute {}
        | attribute
        ;

attribute:
          NULL_SYM
          {
            Lex->type&= ~ NOT_NULL_FLAG;
            Lex->type|= EXPLICIT_NULL_FLAG;
          }
        | not NULL_SYM { Lex->type|= NOT_NULL_FLAG; }
        | DEFAULT now_or_signed_literal { Lex->default_value=$2; }
        | ON UPDATE_SYM now
          {
            Item *item= new (YYTHD->mem_root)
              Item_func_now_local(static_cast<uint8>($3));
            if (item == NULL)
              MYSQL_YYABORT;
            Lex->on_update_value= item;
          }
        | AUTO_INC { Lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG; }
        | SERIAL_SYM DEFAULT VALUE_SYM
          {
            LEX *lex=Lex;
            lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNIQUE_FLAG;
            lex->alter_info.flags|= Alter_info::ALTER_ADD_INDEX;
          }
        | opt_primary KEY_SYM
          {
            LEX *lex=Lex;
            lex->type|= PRI_KEY_FLAG | NOT_NULL_FLAG;
            lex->alter_info.flags|= Alter_info::ALTER_ADD_INDEX;
          }
        | UNIQUE_SYM
          {
            LEX *lex=Lex;
            lex->type|= UNIQUE_FLAG;
            lex->alter_info.flags|= Alter_info::ALTER_ADD_INDEX;
          }
        | UNIQUE_SYM KEY_SYM
          {
            LEX *lex=Lex;
            lex->type|= UNIQUE_KEY_FLAG;
            lex->alter_info.flags|= Alter_info::ALTER_ADD_INDEX;
          }
        | COMMENT_SYM TEXT_STRING_sys { Lex->comment= $2; }
        | COLLATE_SYM collation_name
          {
            if (Lex->charset && !my_charset_same(Lex->charset,$2))
            {
              my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
                       $2->name,Lex->charset->csname);
              MYSQL_YYABORT;
            }
            else
            {
              Lex->charset=$2;
            }
          }
        | COLUMN_FORMAT_SYM DEFAULT
          {
            Lex->type&= ~(FIELD_FLAGS_COLUMN_FORMAT_MASK);
            Lex->type|=
              (COLUMN_FORMAT_TYPE_DEFAULT << FIELD_FLAGS_COLUMN_FORMAT);
          }
        | COLUMN_FORMAT_SYM FIXED_SYM
          {
            Lex->type&= ~(FIELD_FLAGS_COLUMN_FORMAT_MASK);
            Lex->type|=
              (COLUMN_FORMAT_TYPE_FIXED << FIELD_FLAGS_COLUMN_FORMAT);
          }
        | COLUMN_FORMAT_SYM DYNAMIC_SYM
          {
            Lex->type&= ~(FIELD_FLAGS_COLUMN_FORMAT_MASK);
            Lex->type|=
              (COLUMN_FORMAT_TYPE_DYNAMIC << FIELD_FLAGS_COLUMN_FORMAT);
          }
        | STORAGE_SYM DEFAULT
          {
            Lex->type&= ~(FIELD_FLAGS_STORAGE_MEDIA_MASK);
            Lex->type|= (HA_SM_DEFAULT << FIELD_FLAGS_STORAGE_MEDIA);
          }
        | STORAGE_SYM DISK_SYM
          {
            Lex->type&= ~(FIELD_FLAGS_STORAGE_MEDIA_MASK);
            Lex->type|= (HA_SM_DISK << FIELD_FLAGS_STORAGE_MEDIA);
          }
        | STORAGE_SYM MEMORY_SYM
          {
            Lex->type&= ~(FIELD_FLAGS_STORAGE_MEDIA_MASK);
            Lex->type|= (HA_SM_MEMORY << FIELD_FLAGS_STORAGE_MEDIA);
          }
        ;


type_with_opt_collate:
        type opt_collate
        {
          $$= $1;

          if (Lex->charset) /* Lex->charset is scanned in "type" */
          {
            if (!(Lex->charset= merge_charset_and_collation(Lex->charset, $2)))
              MYSQL_YYABORT;
          }
          else if ($2)
          {
            my_error(ER_NOT_SUPPORTED_YET, MYF(0),
                     "COLLATE with no CHARACTER SET "
                     "in SP parameters, RETURNS, DECLARE");
            MYSQL_YYABORT;
          }
        }
        ;


now:
          NOW_SYM func_datetime_precision
          {
            $$= $2;
          };

now_or_signed_literal:
          now
          {
            $$= new (YYTHD->mem_root)
              Item_func_now_local(static_cast<uint8>($1));
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | signed_literal
          { $$=$1; }
        ;

charset:
          CHAR_SYM SET {}
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
        | BINARY { $$= &my_charset_bin; }
        ;

charset_name_or_default:
          charset_name { $$=$1;   }
        | DEFAULT    { $$=NULL; }
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
        | BINARY { $$= &my_charset_bin; }
        ;

old_or_new_charset_name_or_default:
          old_or_new_charset_name { $$=$1;   }
        | DEFAULT    { $$=NULL; }
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
        | COLLATE_SYM collation_name { $$= $2; }
        ;

collation_name_or_default:
          collation_name { $$=$1; }
        | DEFAULT    { $$=NULL; }
        ;

opt_default:
          /* empty */ {}
        | DEFAULT {}
        ;


ascii:
          ASCII_SYM        { $$= &my_charset_latin1; }
        | BINARY ASCII_SYM { $$= &my_charset_latin1_bin; }
        | ASCII_SYM BINARY { $$= &my_charset_latin1_bin; }
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
        | UNICODE_SYM BINARY
          {
            if (!($$= mysqld_collation_get_by_name("ucs2_bin")))
              MYSQL_YYABORT;
          }
        | BINARY UNICODE_SYM
          {
            if (!($$= mysqld_collation_get_by_name("ucs2_bin")))
              my_error(ER_UNKNOWN_COLLATION, MYF(0), "ucs2_bin");
          }
        ;

opt_binary:
          /* empty */
          {
            $$.charset= NULL;
            $$.type_flags= 0;
          }
        | ascii
          {
            $$.charset= $1;
            $$.type_flags= 0;
          }
        | unicode
          {
            $$.charset= $1;
            $$.type_flags= 0;
          }
        | BYTE_SYM
          {
            $$.charset= &my_charset_bin;
            $$.type_flags= 0;
          }
        | charset charset_name opt_bin_mod
          {
            $$.charset= $2;
            $$.type_flags= $3;
          }
        | BINARY
          {
            $$.charset= NULL;
            $$.type_flags= BINCMP_FLAG;
          }
        | BINARY charset charset_name
          {
            $$.charset= $3;
            $$.type_flags= BINCMP_FLAG;
          }
        ;

opt_bin_mod:
          /* empty */ { $$= 0; }
        | BINARY      { $$= BINCMP_FLAG; }
        ;

ws_nweights:
        '(' real_ulong_num
        {
          if ($2 == 0)
          {
            my_syntax_error(ER(ER_SYNTAX_ERROR));
            MYSQL_YYABORT;
          }
        }
        ')'
        { $$= $2; }
        ;

ws_level_flag_desc:
        ASC { $$= 0; }
        | DESC { $$= 1 << MY_STRXFRM_DESC_SHIFT; }
        ;

ws_level_flag_reverse:
        REVERSE_SYM { $$= 1 << MY_STRXFRM_REVERSE_SHIFT; } ;

ws_level_flags:
        /* empty */ { $$= 0; }
        | ws_level_flag_desc { $$= $1; }
        | ws_level_flag_desc ws_level_flag_reverse { $$= $1 | $2; }
        | ws_level_flag_reverse { $$= $1 ; }
        ;

ws_level_number:
        real_ulong_num
        {
          $$= $1 < 1 ? 1 : ($1 > MY_STRXFRM_NLEVELS ? MY_STRXFRM_NLEVELS : $1);
          $$--;
        }
        ;

ws_level_list_item:
        ws_level_number ws_level_flags
        {
          $$= (1 | $2) << $1;
        }
        ;

ws_level_list:
        ws_level_list_item { $$= $1; }
        | ws_level_list ',' ws_level_list_item { $$|= $3; }
        ;

ws_level_range:
        ws_level_number '-' ws_level_number
        {
          uint start= $1;
          uint end= $3;
          for ($$= 0; start <= end; start++)
            $$|= (1 << start);
        }
        ;

ws_level_list_or_range:
        ws_level_list { $$= $1; }
        | ws_level_range { $$= $1; }
        ;

opt_ws_levels:
        /* empty*/ { $$= 0; }
        | LEVEL_SYM ws_level_list_or_range { $$= $2; }
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
            $$=$2;
          }
        ;

opt_ref_list:
          /* empty */
          { Lex->ref_list.empty(); }
        | '(' ref_list ')'
        ;

ref_list:
          ref_list ',' ident
          {
            Key_part_spec *key= new Key_part_spec($3, 0);
            if (key == NULL)
              MYSQL_YYABORT;
            Lex->ref_list.push_back(key);
          }
        | ident
          {
            Key_part_spec *key= new Key_part_spec($1, 0);
            if (key == NULL)
              MYSQL_YYABORT;
            LEX *lex= Lex;
            lex->ref_list.empty();
            lex->ref_list.push_back(key);
          }
        ;

opt_match_clause:
          /* empty */
          { Lex->fk_match_option= FK_MATCH_UNDEF; }
        | MATCH FULL
          { Lex->fk_match_option= FK_MATCH_FULL; }
        | MATCH PARTIAL
          { Lex->fk_match_option= FK_MATCH_PARTIAL; }
        | MATCH SIMPLE_SYM
          { Lex->fk_match_option= FK_MATCH_SIMPLE; }
        ;

opt_on_update_delete:
          /* empty */
          {
            LEX *lex= Lex;
            lex->fk_update_opt= FK_OPTION_UNDEF;
            lex->fk_delete_opt= FK_OPTION_UNDEF;
          }
        | ON UPDATE_SYM delete_option
          {
            LEX *lex= Lex;
            lex->fk_update_opt= $3;
            lex->fk_delete_opt= FK_OPTION_UNDEF;
          }
        | ON DELETE_SYM delete_option
          {
            LEX *lex= Lex;
            lex->fk_update_opt= FK_OPTION_UNDEF;
            lex->fk_delete_opt= $3;
          }
        | ON UPDATE_SYM delete_option
          ON DELETE_SYM delete_option
          {
            LEX *lex= Lex;
            lex->fk_update_opt= $3;
            lex->fk_delete_opt= $6;
          }
        | ON DELETE_SYM delete_option
          ON UPDATE_SYM delete_option
          {
            LEX *lex= Lex;
            lex->fk_update_opt= $6;
            lex->fk_delete_opt= $3;
          }
        ;

delete_option:
          RESTRICT      { $$= FK_OPTION_RESTRICT; }
        | CASCADE       { $$= FK_OPTION_CASCADE; }
        | SET NULL_SYM  { $$= FK_OPTION_SET_NULL; }
        | NO_SYM ACTION { $$= FK_OPTION_NO_ACTION; }
        | SET DEFAULT   { $$= FK_OPTION_DEFAULT;  }
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

fulltext:
          FULLTEXT_SYM { $$= KEYTYPE_FULLTEXT;}
        ;

spatial:
          SPATIAL_SYM
          {
            $$= KEYTYPE_SPATIAL;
          }
        ;

init_key_options:
          {
            Lex->key_create_info= default_key_create_info;
          }
        ;

/*
  For now, key_alg initializies lex->key_create_info.
  In the future, when all key options are after key definition,
  we can remove key_alg and move init_key_options to key_options
*/

key_alg:
          init_key_options
        | init_key_options key_using_alg
        ;

normal_key_options:
          /* empty */ {}
        | normal_key_opts
        ;

fulltext_key_options:
          /* empty */ {}
        | fulltext_key_opts
        ;

spatial_key_options:
          /* empty */ {}
        | spatial_key_opts
        ;

normal_key_opts:
          normal_key_opt
        | normal_key_opts normal_key_opt
        ;

spatial_key_opts:
          spatial_key_opt
        | spatial_key_opts spatial_key_opt
        ;

fulltext_key_opts:
          fulltext_key_opt
        | fulltext_key_opts fulltext_key_opt
        ;

key_using_alg:
          USING btree_or_rtree     { Lex->key_create_info.algorithm= $2; }
        | TYPE_SYM btree_or_rtree  { Lex->key_create_info.algorithm= $2; }
        ;

all_key_opt:
          KEY_BLOCK_SIZE opt_equal ulong_num
          { Lex->key_create_info.block_size= $3; }
        | COMMENT_SYM TEXT_STRING_sys { Lex->key_create_info.comment= $2; }
        ;

normal_key_opt:
          all_key_opt
        | key_using_alg
        ;

spatial_key_opt:
          all_key_opt
        ;

fulltext_key_opt:
          all_key_opt
        | WITH PARSER_SYM IDENT_sys
          {
            LEX_CSTRING plugin_name= {$3.str, $3.length};
            if (plugin_is_ready(plugin_name, MYSQL_FTPARSER_PLUGIN))
              Lex->key_create_info.parser_name= $3;
            else
            {
              my_error(ER_FUNCTION_NOT_DEFINED, MYF(0), $3.str);
              MYSQL_YYABORT;
            }
          }
        ;

btree_or_rtree:
          BTREE_SYM { $$= HA_KEY_ALG_BTREE; }
        | RTREE_SYM { $$= HA_KEY_ALG_RTREE; }
        | HASH_SYM  { $$= HA_KEY_ALG_HASH; }
        ;

key_list:
          key_list ',' key_part order_dir { Lex->col_list.push_back($3); }
        | key_part order_dir { Lex->col_list.push_back($1); }
        ;

key_part:
          ident
          {
            $$= new Key_part_spec($1, 0);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | ident '(' NUM ')'
          {
            int key_part_len= atoi($3.str);
            if (!key_part_len)
            {
              my_error(ER_KEY_PART_0, MYF(0), $1.str);
            }
            $$= new Key_part_spec($1, (uint) key_part_len);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_ident:
          /* empty */ { $$= null_lex_str; }
        | field_ident { $$= $1; }
        ;

opt_component:
          /* empty */    { $$= null_lex_str; }
        | '.' ident      { $$= $2; }
        ;

string_list:
          text_string { Lex->interval_list.push_back($1); }
        | string_list ',' text_string { Lex->interval_list.push_back($3); };

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
            lex->col_list.empty();
            lex->select_lex->init_order();
            lex->select_lex->db=
                    const_cast<char*>((lex->select_lex->table_list.first)->db);
            memset(&lex->create_info, 0, sizeof(lex->create_info));
            lex->create_info.db_type= 0;
            lex->create_info.default_table_charset= NULL;
            lex->create_info.row_type= ROW_TYPE_NOT_USED;
            lex->alter_info.reset();
            lex->no_write_to_binlog= 0;
            lex->create_info.storage_media= HA_SM_DEFAULT;
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
              lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_alter_table();
              if (lex->m_sql_cmd == NULL)
                MYSQL_YYABORT;
            }
          }
        | ALTER DATABASE ident_or_empty
          {
            Lex->create_info.default_table_charset= NULL;
            Lex->create_info.used_fields= 0;
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
        | ALTER DATABASE ident UPGRADE_SYM DATA_SYM DIRECTORY_SYM NAME_SYM
          {
            LEX *lex= Lex;
            push_deprecated_warn_no_replacement(YYTHD,
              "UPGRADE DATA DIRECTORY NAME");
            if (lex->sphead)
            {
              my_error(ER_SP_NO_DROP_SP, MYF(0), "DATABASE");
              MYSQL_YYABORT;
            }
            lex->sql_command= SQLCOM_ALTER_DB_UPGRADE;
            lex->name= $3;
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
            lex->create_view_mode= VIEW_ALTER;
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
            lex->create_view_mode= VIEW_ALTER;
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

            if (!(Lex->event_parse_data= Event_parse_data::new_instance(YYTHD)))
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
              my_syntax_error(ER(ER_SYNTAX_ERROR));
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
              new (YYTHD->mem_root) Sql_cmd_alter_server(&Lex->server_options);
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
        ;

alter_user_command:
          ALTER USER if_exists clear_privileges
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
        | password_expire
          {
            LEX *lex=Lex;
            lex->alter_password.update_password_expired_fields= true;
            lex->alter_password.update_password_expired_column= true;
          }
        | password_expire INTERVAL_SYM real_ulong_num DAY_SYM
          {
            LEX *lex=Lex;
            if ($3 == 0 || $3 > UINT_MAX16)
            {
              char buf[MAX_BIGINT_WIDTH + 1];
              my_snprintf(buf, sizeof(buf), "%lu", $3);
              my_error(ER_WRONG_VALUE, MYF(0), "DAY", buf);
              MYSQL_YYABORT;
            }
            lex->alter_password.update_password_expired_fields= true;
            lex->alter_password.expire_after_days= $3;
            lex->alter_password.use_default_password_lifetime= false;
          }
        | password_expire NEVER_SYM
          {
            LEX *lex=Lex;
            lex->alter_password.update_password_expired_fields= true;
            lex->alter_password.use_default_password_lifetime= false;
          }
        | password_expire DEFAULT
          {
            Lex->alter_password.update_password_expired_fields= true;
          }
        ;

password_expire:
          PASSWORD EXPIRE_SYM clear_password_expire_options {}
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
        | ON SCHEDULE_SYM ev_schedule_time { $$= 1; }
        | ev_on_completion { $$= 1; }
        | ON SCHEDULE_SYM ev_schedule_time ev_on_completion { $$= 1; }
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
        | alter_command_list partitioning
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
            Lex->m_sql_cmd= new (YYTHD->mem_root)
              Sql_cmd_discard_import_tablespace(
                Sql_cmd_discard_import_tablespace::DISCARD_TABLESPACE);
            if (Lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | IMPORT TABLESPACE_SYM
          {
            Lex->m_sql_cmd= new (YYTHD->mem_root)
              Sql_cmd_discard_import_tablespace(
                Sql_cmd_discard_import_tablespace::IMPORT_TABLESPACE);
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
        | DROP PARTITION_SYM alt_part_name_list
          {
            Lex->alter_info.flags|= Alter_info::ALTER_DROP_PARTITION;
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
            lex->m_sql_cmd= new (thd->mem_root)
                              Sql_cmd_alter_table_optimize_partition();
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
            lex->m_sql_cmd= new (thd->mem_root)
                              Sql_cmd_alter_table_analyze_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | CHECK_SYM PARTITION_SYM all_or_alt_part_name_list
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->check_opt.init();
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= new (thd->mem_root)
                              Sql_cmd_alter_table_check_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
          opt_mi_check_type
        | REPAIR PARTITION_SYM opt_no_write_to_binlog
          all_or_alt_part_name_list
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            lex->no_write_to_binlog= $3;
            lex->check_opt.init();
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= new (thd->mem_root)
                              Sql_cmd_alter_table_repair_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
          opt_mi_repair_type
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
            lex->m_sql_cmd= new (thd->mem_root)
                              Sql_cmd_alter_table_truncate_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | reorg_partition_rule
        | EXCHANGE_SYM PARTITION_SYM alt_part_name_item
          WITH TABLE_SYM table_ident opt_validation
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
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
            lex->m_sql_cmd= new (thd->mem_root)
                               Sql_cmd_alter_table_exchange_partition();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | DISCARD PARTITION_SYM all_or_alt_part_name_list
          TABLESPACE_SYM
          {
            Lex->m_sql_cmd= new (YYTHD->mem_root)
              Sql_cmd_discard_import_tablespace(
                Sql_cmd_discard_import_tablespace::DISCARD_TABLESPACE);
            if (Lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        | IMPORT PARTITION_SYM all_or_alt_part_name_list
          TABLESPACE_SYM
          {
            Lex->m_sql_cmd= new (YYTHD->mem_root)
              Sql_cmd_discard_import_tablespace(
                Sql_cmd_discard_import_tablespace::IMPORT_TABLESPACE);
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
        | alt_part_name_list
        ;

add_partition_rule:
          ADD PARTITION_SYM opt_no_write_to_binlog
          {
            LEX *lex= Lex;
            lex->part_info= new partition_info();
            if (!lex->part_info)
            {
              mem_alloc_error(sizeof(partition_info));
              MYSQL_YYABORT;
            }
            lex->alter_info.flags|= Alter_info::ALTER_ADD_PARTITION;
            lex->no_write_to_binlog= $3;
          }
          add_part_extra
          {}
        ;

add_part_extra:
          /* empty */
        | '(' part_def_list ')'
          {
            LEX *lex= Lex;
            lex->part_info->num_parts= lex->part_info->partitions.elements;
          }
        | PARTITIONS_SYM real_ulong_num
          {
            Lex->part_info->num_parts= $2;
          }
        ;

reorg_partition_rule:
          REORGANIZE_SYM PARTITION_SYM opt_no_write_to_binlog
          {
            LEX *lex= Lex;
            lex->part_info= new partition_info();
            if (!lex->part_info)
            {
              mem_alloc_error(sizeof(partition_info));
              MYSQL_YYABORT;
            }
            lex->no_write_to_binlog= $3;
          }
          reorg_parts_rule
        ;

reorg_parts_rule:
          /* empty */
          {
            Lex->alter_info.flags|= Alter_info::ALTER_TABLE_REORG;
          }
        | alt_part_name_list
          {
            Lex->alter_info.flags|= Alter_info::ALTER_REORGANIZE_PARTITION;
          }
          INTO '(' part_def_list ')'
          {
            partition_info *part_info= Lex->part_info;
            part_info->num_parts= part_info->partitions.elements;
          }
        ;

alt_part_name_list:
          alt_part_name_item {}
        | alt_part_name_list ',' alt_part_name_item {}
        ;

alt_part_name_item:
          ident
          {
            String *s= new (YYTHD->mem_root) String((const char *) $1.str,
                                                    $1.length,
                                                    system_charset_info);
            if (s == NULL)
              MYSQL_YYABORT;
            if (Lex->alter_info.partition_names.push_back(s))
            {
              mem_alloc_error(1);
              MYSQL_YYABORT;
            }
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
            lex->change=0;
            lex->alter_info.flags|= Alter_info::ALTER_ADD_COLUMN;
          }
        ;

alter_list_item:
          add_column column_def opt_place
          {
            Lex->create_last_non_select_table= Lex->last_table();
          }
        | ADD key_def
          {
            Lex->create_last_non_select_table= Lex->last_table();
            Lex->alter_info.flags|= Alter_info::ALTER_ADD_INDEX;
          }
        | add_column '(' create_field_list ')'
        | CHANGE opt_column field_ident
          {
            LEX *lex=Lex;
            lex->change= $3.str;
            lex->alter_info.flags|= Alter_info::ALTER_CHANGE_COLUMN;
          }
          field_spec opt_place
          {
            Lex->create_last_non_select_table= Lex->last_table();
          }
        | MODIFY_SYM opt_column field_ident
          {
            LEX *lex=Lex;
            lex->length=lex->dec=0; lex->type=0;
            lex->default_value= lex->on_update_value= 0;
            lex->comment=null_lex_str;
            lex->charset= NULL;
            lex->alter_info.flags|= Alter_info::ALTER_CHANGE_COLUMN;
            lex->gcol_info= 0;
          }
          field_def
          {
            LEX *lex=Lex;
            if (add_field_to_list(lex->thd,&$3,
                                  (enum enum_field_types) $5,
                                  lex->length,lex->dec,lex->type,
                                  lex->default_value, lex->on_update_value,
                                  &lex->comment,
                                  $3.str, &lex->interval_list, lex->charset,
                                  lex->uint_geom_type,
                                  lex->gcol_info))
              MYSQL_YYABORT;
          }
          opt_place
          {
            Lex->create_last_non_select_table= Lex->last_table();
          }
        | DROP opt_column field_ident opt_restrict
          {
            LEX *lex=Lex;
            Alter_drop *ad= new Alter_drop(Alter_drop::COLUMN, $3.str);
            if (ad == NULL)
              MYSQL_YYABORT;
            lex->alter_info.drop_list.push_back(ad);
            lex->alter_info.flags|= Alter_info::ALTER_DROP_COLUMN;
          }
        | DROP FOREIGN KEY_SYM field_ident
          {
            LEX *lex=Lex;
            Alter_drop *ad= new Alter_drop(Alter_drop::FOREIGN_KEY, $4.str);
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
            Alter_drop *ad= new Alter_drop(Alter_drop::KEY, $3.str);
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
        | ALTER opt_column field_ident SET DEFAULT signed_literal
          {
            LEX *lex=Lex;
            Alter_column *ac= new Alter_column($3.str,$6);
            if (ac == NULL)
              MYSQL_YYABORT;
            lex->alter_info.alter_list.push_back(ac);
            lex->alter_info.flags|= Alter_info::ALTER_CHANGE_COLUMN_DEFAULT;
          }
        | ALTER opt_column field_ident DROP DEFAULT
          {
            LEX *lex=Lex;
            Alter_column *ac= new Alter_column($3.str, (Item*) 0);
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
            enum_ident_name_check ident_check_status=
              check_table_name($3->table.str,$3->table.length, FALSE);
            if (ident_check_status == IDENT_NAME_WRONG)
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $3->table.str);
              MYSQL_YYABORT;
            }
            else if (ident_check_status == IDENT_NAME_TOO_LONG)
            {
              my_error(ER_TOO_LONG_IDENT, MYF(0), $3->table.str);
              MYSQL_YYABORT;
            }
            LEX_STRING db_str= to_lex_string($3->db);
            if (db_str.str &&
                (check_and_convert_db_name(&db_str, FALSE) != IDENT_NAME_OK))
              MYSQL_YYABORT;
            lex->name.str= const_cast<char*>($3->table.str);
            lex->name.length= $3->table.length;
            lex->alter_info.flags|= Alter_info::ALTER_RENAME;
          }
        | RENAME key_or_index field_ident TO_SYM field_ident
          {
            LEX *lex=Lex;
            Alter_rename_key *ak= new (YYTHD->mem_root)
                                    Alter_rename_key($3.str, $5.str);
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
            lex->create_info.table_charset=
            lex->create_info.default_table_charset= $5;
            lex->create_info.used_fields|= (HA_CREATE_USED_CHARSET |
              HA_CREATE_USED_DEFAULT_CHARSET);
            lex->alter_info.flags|= Alter_info::ALTER_OPTIONS;
          }
        | create_table_options_space_separated
          {
            LEX *lex=Lex;
            lex->alter_info.flags|= Alter_info::ALTER_OPTIONS;
            if ((lex->create_info.used_fields & HA_CREATE_USED_ENGINE) &&
                !lex->create_info.db_type)
            {
              lex->create_info.used_fields&= ~HA_CREATE_USED_ENGINE;
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
        | UPGRADE_SYM PARTITIONING_SYM
          {
            Lex->alter_info.flags|= Alter_info::ALTER_UPGRADE_PARTITIONING;
          }
        ;

alter_commands_modifier:
          alter_algorithm_option
        | alter_lock_option
        | alter_opt_validation
        ;

opt_index_lock_algorithm:
          /* empty */
        | alter_lock_option
        | alter_algorithm_option
        | alter_lock_option alter_algorithm_option
        | alter_algorithm_option alter_lock_option

alter_algorithm_option:
          ALGORITHM_SYM opt_equal DEFAULT
          {
            Lex->alter_info.requested_algorithm=
              Alter_info::ALTER_TABLE_ALGORITHM_DEFAULT;
          }
        | ALGORITHM_SYM opt_equal ident
          {
            if (Lex->alter_info.set_requested_algorithm(&$3))
            {
              my_error(ER_UNKNOWN_ALTER_ALGORITHM, MYF(0), $3.str);
              MYSQL_YYABORT;
            }
          }
        ;

alter_lock_option:
          LOCK_SYM opt_equal DEFAULT
          {
            Lex->alter_info.requested_lock=
              Alter_info::ALTER_TABLE_LOCK_DEFAULT;
          }
        | LOCK_SYM opt_equal ident
          {
            if (Lex->alter_info.set_requested_lock(&$3))
            {
              my_error(ER_UNKNOWN_ALTER_LOCK, MYF(0), $3.str);
              MYSQL_YYABORT;
            }
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
          /* empty */ { Lex->drop_mode= DROP_DEFAULT; }
        | RESTRICT    { Lex->drop_mode= DROP_RESTRICT; }
        | CASCADE     { Lex->drop_mode= DROP_CASCADE; }
        ;

opt_place:
          /* empty */ {}
        | AFTER_SYM ident
          {
            store_position_for_column($2.str);
            Lex->alter_info.flags |= Alter_info::ALTER_COLUMN_ORDER;
          }
        | FIRST_SYM
          {
            store_position_for_column(first_keyword);
            Lex->alter_info.flags |= Alter_info::ALTER_COLUMN_ORDER;
          }
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
              my_syntax_error(ER(ER_SYNTAX_ERROR));
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
               my_message(ER_BAD_SLAVE_UNTIL_COND,
                          ER(ER_BAD_SLAVE_UNTIL_COND), MYF(0));
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
          CHECKSUM_SYM table_or_tables
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_CHECKSUM;
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
          }
          table_list opt_checksum_type
          {}
        ;

opt_checksum_type:
          /* nothing */ { Lex->check_opt.flags= 0; }
        | QUICK         { Lex->check_opt.flags= T_QUICK; }
        | EXTENDED_SYM  { Lex->check_opt.flags= T_EXTEND; }
        ;

repair:
          REPAIR opt_no_write_to_binlog table_or_tables
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_REPAIR;
            lex->no_write_to_binlog= $2;
            lex->check_opt.init();
            lex->alter_info.reset();
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
          }
          table_list opt_mi_repair_type
          {
            THD *thd= YYTHD;
            LEX* lex= thd->lex;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_repair_table();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_mi_repair_type:
          /* empty */ { Lex->check_opt.flags = T_MEDIUM; }
        | mi_repair_types {}
        ;

mi_repair_types:
          mi_repair_type {}
        | mi_repair_type mi_repair_types {}
        ;

mi_repair_type:
          QUICK        { Lex->check_opt.flags|= T_QUICK; }
        | EXTENDED_SYM { Lex->check_opt.flags|= T_EXTEND; }
        | USE_FRM      { Lex->check_opt.sql_flags|= TT_USEFRM; }
        ;

analyze:
          ANALYZE_SYM opt_no_write_to_binlog table_or_tables
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_ANALYZE;
            lex->no_write_to_binlog= $2;
            lex->check_opt.init();
            lex->alter_info.reset();
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
          }
          table_list
          {
            THD *thd= YYTHD;
            LEX* lex= thd->lex;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_analyze_table();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

binlog_base64_event:
          BINLOG_SYM TEXT_STRING_sys
          {
            Lex->sql_command = SQLCOM_BINLOG_BASE64_EVENT;
            Lex->comment= $2;
          }
        ;

check:
          CHECK_SYM table_or_tables
          {
            LEX *lex=Lex;

            if (lex->sphead)
            {
              my_error(ER_SP_BADSTATEMENT, MYF(0), "CHECK");
              MYSQL_YYABORT;
            }
            lex->sql_command = SQLCOM_CHECK;
            lex->check_opt.init();
            lex->alter_info.reset();
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
          }
          table_list opt_mi_check_type
          {
            THD *thd= YYTHD;
            LEX* lex= thd->lex;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_check_table();
            if (lex->m_sql_cmd == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_mi_check_type:
          /* empty */ { Lex->check_opt.flags = T_MEDIUM; }
        | mi_check_types {}
        ;

mi_check_types:
          mi_check_type {}
        | mi_check_type mi_check_types {}
        ;

mi_check_type:
          QUICK               { Lex->check_opt.flags|= T_QUICK; }
        | FAST_SYM            { Lex->check_opt.flags|= T_FAST; }
        | MEDIUM_SYM          { Lex->check_opt.flags|= T_MEDIUM; }
        | EXTENDED_SYM        { Lex->check_opt.flags|= T_EXTEND; }
        | CHANGED             { Lex->check_opt.flags|= T_CHECK_ONLY_CHANGED; }
        | FOR_SYM UPGRADE_SYM { Lex->check_opt.sql_flags|= TT_FOR_UPGRADE; }
        ;

optimize:
          OPTIMIZE opt_no_write_to_binlog table_or_tables
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_OPTIMIZE;
            lex->no_write_to_binlog= $2;
            lex->check_opt.init();
            lex->alter_info.reset();
            /* Will be overriden during execution. */
            YYPS->m_lock_type= TL_UNLOCK;
          }
          table_list
          {
            THD *thd= YYTHD;
            LEX* lex= thd->lex;
            DBUG_ASSERT(!lex->m_sql_cmd);
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_optimize_table();
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
        | RENAME USER clear_privileges rename_list
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
        | DEFAULT  { $$ = default_key_cache_base; }
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

/*
  Select : retrieve data from table
*/


select:
          select_init
          {
            $$= NEW_PTN PT_select($1, SQLCOM_SELECT);
          }
        ;

/* Need first branch for subselects. */
select_init:
          SELECT_SYM select_part2 opt_union_clause
          {
            $$= NEW_PTN PT_select_init2($1, $2, $3);
          }
        | '(' select_paren ')' union_opt
          {
            $$= NEW_PTN PT_select_init_parenthesis($2, $4);
          }
        ;

select_paren:
          SELECT_SYM select_part2
          {
            $$= NEW_PTN PT_select_paren($1, $2);
          }
        | '(' select_paren ')' { $$= $2; }
        ;

/* The equivalent of select_paren for nested queries. */
select_paren_derived:
          SELECT_SYM select_part2_derived table_expression
          {
            $$= NEW_PTN PT_select_paren_derived($1, $2, $3);
          }
        | '(' select_paren_derived ')' { $$= $2; }
        ;

/*
  Theoretically we can merge all 3 right hand sides of the select_part2
  rule into one, however such a transformation adds one shift/reduce
  conflict more.
*/
select_part2:
          select_options_and_item_list
          opt_order_clause
          opt_limit_clause
          opt_select_lock_type
          {
            $$= NEW_PTN PT_select_part2($1, NULL, NULL, NULL, NULL, NULL,
                                        $2, $3, NULL, NULL, $4);
          }
        | select_options_and_item_list into opt_select_lock_type
          {
            $$= NEW_PTN PT_select_part2($1, $2, NULL, NULL, NULL, NULL, NULL,
                                        NULL, NULL, NULL, $3);
          }
        | select_options_and_item_list  /* #1 */
          opt_into                      /* #2 */
          from_clause                   /* #3 */
          opt_where_clause              /* #4 */
          opt_group_clause              /* #5 */
          opt_having_clause             /* #6 */
          opt_order_clause              /* #7 */
          opt_limit_clause              /* #8 */
          opt_procedure_analyse_clause  /* #9 */
          opt_into                      /* #10 */
          opt_select_lock_type          /* #11 */
          {
            if ($2 && $10)
            {
              /* double "INTO" clause */
              YYTHD->parse_error_at(@10, ER(ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            if ($9 && ($2 || $10))
            {
              /* "INTO" with "PROCEDURE ANALYSE" */
              my_error(ER_WRONG_USAGE, MYF(0), "PROCEDURE", "INTO");
              MYSQL_YYABORT;
            }
            $$= NEW_PTN PT_select_part2($1, $2, $3, $4, $5, $6, $7, $8, $9, $10,
                                        $11);
          }
        ;

select_options_and_item_list:
          {
            /*
              TODO: remove this semantic action (currently this removal
              adds shift/reduce conflict)
            */
          }
          select_options select_item_list
          {
            $$= NEW_PTN PT_select_options_and_item_list($2, $3);
          }
        ;


table_expression:
          opt_from_clause               /* #1 */
          opt_where_clause              /* #2 */
          opt_group_clause              /* #3 */
          opt_having_clause             /* #4 */
          opt_order_clause              /* #5 */
          opt_limit_clause              /* #6 */
          opt_procedure_analyse_clause  /* #7 */
          opt_select_lock_type          /* #8 */
          {
            $$= NEW_PTN PT_table_expression($1, $2, $3, $4, $5, $6, $7, $8);
          }
        ;

from_clause:
          FROM table_reference_list { $$= $2; }
        ;

opt_from_clause:
          /* empty */ { $$= NULL; }
        | from_clause
        ;

table_reference_list:
          join_table_list
          {
            $$= NEW_PTN PT_table_reference_list($1);
          }
        | DUAL_SYM { $$= NULL; }
          /* oracle compatibility: oracle always requires FROM clause,
             and DUAL is system table without fields.
             Is "SELECT 1 FROM DUAL" any better than "SELECT 1" ?
          Hmmm :) */
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
            push_deprecated_warn_no_replacement(YYTHD, "SQL_NO_CACHE");
            /*
              Allow this flag only on the first top-level SELECT statement, if
              SQL_CACHE wasn't specified, and only once per query.
             */
            $$.query_spec_options= 0;
            $$.sql_cache= SELECT_LEX::SQL_NO_CACHE;
          }
        | SQL_CACHE_SYM
          {
            push_deprecated_warn_no_replacement(YYTHD, "SQL_CACHE");
            /*
              Allow this flag only on the first top-level SELECT statement, if
              SQL_NO_CACHE wasn't specified, and only once per query.
             */
            $$.query_spec_options= 0;
            $$.sql_cache= SELECT_LEX::SQL_CACHE;
          }
        ;

opt_select_lock_type:
          /* empty */ { $$.is_set= false; }
        | FOR_SYM UPDATE_SYM
          {
            $$.is_set= true;
            $$.lock_type= TL_WRITE;
            $$.is_safe_to_cache_query= false;
          }
        | LOCK_SYM IN_SYM SHARE_SYM MODE_SYM
          {
            $$.is_set= true;
            $$.lock_type= TL_READ_WITH_SHARED_LOCKS;
            $$.is_safe_to_cache_query= false;
          }
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
                                                 YYTHD->mem_root, @$, $1, $3);
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
                                                 YYTHD->mem_root, @$, $1, $3);
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
        | bool_pri comp_op predicate %prec EQ
          {
            $$= NEW_PTN PTI_comp_op(@$, $1, $2, $3);
          }
        | bool_pri comp_op all_or_any '(' subselect ')' %prec EQ
          {
            if ($2 == &comp_equal_creator)
              /*
                We throw this manual parse error rather than split the rule
                comp_op into a null-safe and a non null-safe rule, since doing
                so would add a shift/reduce conflict. It's actually this rule
                and the ones referencing it that cause all the conflicts, but
                we still don't want the count to go up.
              */
              YYTHD->parse_error_at(@2, ER_THD(YYTHD, ER_SYNTAX_ERROR));
            $$= NEW_PTN PTI_comp_op_all(@$, $1, $2, $3, $5);
          }
        | predicate
        ;

predicate:
          bit_expr IN_SYM '(' subselect ')'
          {
            $$= NEW_PTN Item_in_subselect(@$, $1, $4);
          }
        | bit_expr not IN_SYM '(' subselect ')'
          {
            Item *item= NEW_PTN Item_in_subselect(@$, $1, $5);
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
        | sum_expr
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
        | '(' subselect ')'
          {
            $$= NEW_PTN PTI_singlerow_subselect(@$, $2);
          }
        | '(' expr ')'
          { $$= $2; }
        | '(' expr ',' expr_list ')'
          {
            $$= NEW_PTN Item_row(@$, $2, $4->value);
          }
        | ROW_SYM '(' expr ',' expr_list ')'
          {
            $$= NEW_PTN Item_row(@$, $3, $5->value);
          }
        | EXISTS '(' subselect ')'
          {
            $$= NEW_PTN PTI_exists_subselect(@$, $3);
          }
        | '{' ident expr '}'
          {
            $$= NEW_PTN PTI_odbc_date(@$, $2, $3);
          }
        | MATCH ident_list_arg AGAINST '(' bit_expr fulltext_options ')'
          {
            $$= NEW_PTN Item_func_match(@$, $2, $5, $6);
          }
        | BINARY simple_expr %prec NEG
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
        | DEFAULT '(' simple_ident ')'
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
        | INSERT '(' expr ',' expr ',' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_insert(@$, $3, $5, $7, $9);
          }
        | INTERVAL_SYM '(' expr ',' expr ')' %prec INTERVAL_SYM
          {
            $$= NEW_PTN Item_func_interval(@$, YYTHD->mem_root, $3, $5);
          }
        | INTERVAL_SYM '(' expr ',' expr ',' expr_list ')' %prec INTERVAL_SYM
          {
            $$= NEW_PTN Item_func_interval(@$, YYTHD->mem_root, $3, $5, $7);
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
        | TIMESTAMP '(' expr ')'
          {
            $$= NEW_PTN Item_datetime_typecast(@$, $3);
          }
        | TIMESTAMP '(' expr ',' expr ')'
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
        | REPLACE '(' expr ',' expr ',' expr ')'
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
        | WEIGHT_STRING_SYM '(' expr opt_ws_levels ')'
          {
            $$= NEW_PTN Item_func_weight_string(@$, $3, 0, 0, $4);
          }
        | WEIGHT_STRING_SYM '(' expr AS CHAR_SYM ws_nweights opt_ws_levels ')'
          {
            $$= NEW_PTN Item_func_weight_string(@$, $3, 0, $6,
                        $7 | MY_STRXFRM_PAD_WITH_SPACE);
          }
        | WEIGHT_STRING_SYM '(' expr AS BINARY ws_nweights ')'
          {
            $$= NEW_PTN Item_func_weight_string(@$,
                        $3, 0, $6, MY_STRXFRM_PAD_WITH_SPACE, true);
          }
        | WEIGHT_STRING_SYM '(' expr ',' ulong_num ',' ulong_num ',' ulong_num ')'
          {
            $$= NEW_PTN Item_func_weight_string(@$, $3, $5, $7, $9);
          }
        | geometry_function
        ;

geometry_function:
          CONTAINS_SYM '(' expr ',' expr ')'
          {
            push_deprecated_warn(YYTHD, "CONTAINS", "MBRCONTAINS");
            $$= NEW_PTN Item_func_spatial_mbr_rel(@$, $3, $5,
                        Item_func::SP_CONTAINS_FUNC);
          }
        | GEOMETRYCOLLECTION '(' opt_expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_geometrycollection,
                        Geometry::wkb_point);
          }
        | LINESTRING '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_linestring,
                        Geometry::wkb_point);
          }
        | MULTILINESTRING '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_multilinestring,
                        Geometry::wkb_linestring);
          }
        | MULTIPOINT '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_multipoint,
                        Geometry::wkb_point);
          }
        | MULTIPOLYGON '(' expr_list ')'
          {
            $$= NEW_PTN Item_func_spatial_collection(@$, $3,
                        Geometry::wkb_multipolygon,
                        Geometry::wkb_polygon);
          }
        | POINT_SYM '(' expr ',' expr ')'
          {
            $$= NEW_PTN Item_func_point(@$, $3,$5);
          }
        | POLYGON '(' expr_list ')'
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
            $$= new (YYTHD->mem_root) String(",", 1, &my_charset_latin1);
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
          BINARY opt_field_length
          {
            $$.target= ITEM_CAST_CHAR;
            $$.charset= &my_charset_bin;
            $$.type_flags= 0;
            $$.length= $2;
            $$.dec= NULL;
          }
        | CHAR_SYM opt_field_length opt_binary
          {
            $$.target= ITEM_CAST_CHAR;
            $$.charset= $3.charset;
            $$.type_flags= $3.type_flags;
            $$.length= $2;
            $$.dec= NULL;
          }
        | NCHAR_SYM opt_field_length
          {
            $$.target= ITEM_CAST_CHAR;
            $$.charset= national_charset_info;
            $$.type_flags= 0;
            $$.length= $2;
            $$.dec= NULL;
          }
        | SIGNED_SYM
          {
            $$.target= ITEM_CAST_SIGNED_INT;
            $$.charset= NULL;
            $$.type_flags= 0;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | SIGNED_SYM INT_SYM
          {
            $$.target= ITEM_CAST_SIGNED_INT;
            $$.charset= NULL;
            $$.type_flags= 0;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | UNSIGNED
          {
            $$.target= ITEM_CAST_UNSIGNED_INT;
            $$.charset= NULL;
            $$.type_flags= 0;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | UNSIGNED INT_SYM
          {
            $$.target= ITEM_CAST_UNSIGNED_INT;
            $$.charset= NULL;
            $$.type_flags= 0;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | DATE_SYM
          {
            $$.target= ITEM_CAST_DATE;
            $$.charset= NULL;
            $$.type_flags= 0;
            $$.length= NULL;
            $$.dec= NULL;
          }
        | TIME_SYM type_datetime_precision
          {
            $$.target= ITEM_CAST_TIME;
            $$.charset= NULL;
            $$.type_flags= 0;
            $$.length= NULL;
            $$.dec= $2;
          }
        | DATETIME type_datetime_precision
          {
            $$.target= ITEM_CAST_DATETIME;
            $$.charset= NULL;
            $$.type_flags= 0;
            $$.length= NULL;
            $$.dec= $2;
          }
        | DECIMAL_SYM float_options
          {
            $$.target=ITEM_CAST_DECIMAL;
            $$.charset= NULL;
            $$.type_flags= 0;
            $$.length= $2.length;
            $$.dec= $2.dec;
          }
        | JSON_SYM
          {
            $$.target=ITEM_CAST_JSON;
            $$.charset= NULL;
            $$.type_flags= 0;
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

/* Equivalent to <table reference> in the SQL:2003 standard. */
/* Warning - may return NULL in case of incomplete SELECT */
table_ref:
          table_factor
        | join_table
          {
            $$= NEW_PTN PT_table_ref_join_table($1);
          }
        ;

join_table_list:
          derived_table_list
          {
            $$= NEW_PTN PT_join_table_list(@$, $1);
          }
        ;

/*
  The ODBC escape syntax for Outer Join is: '{' OJ join_table '}'
  The parser does not define OJ as a token, any ident is accepted
  instead in $2 (ident). Also, all productions from table_ref can
  be escaped, not only join_table. Both syntax extensions are safe
  and are ignored.
*/
esc_table_ref:
        table_ref
      | '{' ident table_ref '}' { $$= $3; }
      ;

/* Equivalent to <table reference list> in the SQL:2003 standard. */
/* Warning - may return NULL in case of incomplete SELECT */
derived_table_list:
          esc_table_ref
        | derived_table_list ',' esc_table_ref
          {
            $$= NEW_PTN PT_derived_table_list(@$, $1, $3);
          }
        ;

/*
  Notice that JOIN is a left-associative operation, and it must be parsed
  as such, that is, the parser must process first the left join operand
  then the right one. Such order of processing ensures that the parser
  produces correct join trees which is essential for semantic analysis
  and subsequent optimization phases.
*/
join_table:
          /* INNER JOIN variants */
          /*
            Use %prec to evaluate production 'table_ref' before 'normal_join'
            so that [INNER | CROSS] JOIN is properly nested as other
            left-associative joins.
          */
          table_ref normal_join table_ref %prec TABLE_REF_PRIORITY
          {
            $$= NEW_PTN PT_join_table<JTT_NORMAL>($1, @2, $3);
          }
        | table_ref STRAIGHT_JOIN table_factor
          {
            $$= NEW_PTN PT_join_table<JTT_STRAIGHT>($1, @2, $3);
          }
        | table_ref normal_join table_ref
          ON
          expr
          {
            $$= NEW_PTN PT_join_table_on<JTT_NORMAL>($1, @2, $3, $5);
          }
        | table_ref STRAIGHT_JOIN table_factor
          ON
          expr
          {
            $$= NEW_PTN PT_join_table_on<JTT_STRAIGHT>($1, @2, $3, $5);
          }
        | table_ref normal_join table_ref
          USING
          '(' using_list ')'
          {
            $$= NEW_PTN PT_join_table_using<JTT_NORMAL>($1, @2, $3, $6);
          }
        | table_ref NATURAL JOIN_SYM table_factor
          {
            $$= NEW_PTN PT_join_table<JTT_NATURAL>($1, @2, $4);
          }

          /* LEFT JOIN variants */
        | table_ref LEFT opt_outer JOIN_SYM table_ref
          ON
          expr
          {
            $$= NEW_PTN PT_join_table_on<JTT_LEFT>($1, @2, $5, $7);
          }
        | table_ref LEFT opt_outer JOIN_SYM table_factor
          USING '(' using_list ')'
          {
            $$= NEW_PTN PT_join_table_using<JTT_LEFT>($1, @2, $5, $8);
          }
        | table_ref NATURAL LEFT opt_outer JOIN_SYM table_factor
          {
            $$= NEW_PTN PT_join_table<JTT_NATURAL_LEFT>($1, @2, $6);
          }

          /* RIGHT JOIN variants */
        | table_ref RIGHT opt_outer JOIN_SYM table_ref
          ON
          expr
          {
            $$= NEW_PTN PT_join_table_on<JTT_RIGHT>($1, @2, $5, $7);
          }
        | table_ref RIGHT opt_outer JOIN_SYM table_factor
          USING '(' using_list ')'
          {
            $$= NEW_PTN PT_join_table_using<JTT_RIGHT>($1, @2, $5, $8);
          }
        | table_ref NATURAL RIGHT opt_outer JOIN_SYM table_factor
          {
            $$= NEW_PTN PT_join_table<JTT_NATURAL_RIGHT>($1, @2, $6);
          }
        ;

normal_join:
          JOIN_SYM {}
        | INNER_SYM JOIN_SYM {}
        | CROSS JOIN_SYM {}
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

/*
   This is a flattening of the rules <table factor> and <table primary>
   in the SQL:2003 standard, since we don't have <sample clause>

   I.e.
   <table factor> ::= <table primary> [ <sample clause> ]
*/
/* Warning - may return NULL in case of incomplete SELECT */
table_factor:
          table_ident opt_use_partition opt_table_alias opt_key_definition
          {
            $$= NEW_PTN PT_table_factor_table_ident($1, $2, $3, $4);
          }
        | SELECT_SYM select_options select_item_list table_expression
          {
            $$= NEW_PTN PT_table_factor_select_sym(@$, $1, $2, $3, $4);
          }
          /*
            Represents a flattening of the following rules from the SQL:2003
            standard. This sub-rule corresponds to the sub-rule
            <table primary> ::= ... | <derived table> [ AS ] <correlation name>

            The following rules have been flattened into query_expression_body
            (since we have no <with clause>).

            <derived table> ::= <table subquery>
            <table subquery> ::= <subquery>
            <subquery> ::= <left paren> <query expression> <right paren>
            <query expression> ::= [ <with clause> ] <query expression body>

            For the time being we use the non-standard rule
            select_derived_union which is a compromise between the standard
            and our parser. Possibly this rule could be replaced by our
            query_expression_body.
          */
        | '(' select_derived_union ')' opt_table_alias
          {
            $$= NEW_PTN PT_table_factor_parenthesis($2, $4, @4);
          }
        ;

/*
  This rule accepts just about anything. The reason is that we have
  empty-producing rules in the beginning of rules, in this case
  subselect_start. This forces bison to take a decision which rules to
  reduce by long before it has seen any tokens. This approach ties us
  to a very limited class of parseable languages, and unfortunately
  SQL is not one of them. The chosen 'solution' was this rule, which
  produces just about anything, even complete bogus statements, for
  instance ( table UNION SELECT 1 ).

  Fortunately, we know that the semantic value returned by
  select_derived->value is NULL if it contained a derived table, and a pointer to
  the base table's TABLE_LIST if it was a base table. So in the rule
  regarding union's, we throw a parse error manually and pretend it
  was bison that did it.

  Also worth noting is that this rule concerns query expressions in
  the from clause only. Top level select statements and other types of
  subqueries have their own union rules.
 */
select_derived_union:
          select_derived opt_union_order_or_limit
          {
            $$= NEW_PTN PT_select_derived_union_select($1, $2, @2);
          }
        | select_derived_union UNION_SYM union_option query_specification
          {
            $$= NEW_PTN PT_select_derived_union_union($1, @2, $3, $4);
          }
        ;

/* The equivalent of select_part2 for nested queries. */
select_part2_derived:
          {
            /*
              TODO: remove this semantic action (currently this removal
              adds shift/reduce conflict)
            */
          }
          opt_query_spec_options select_item_list
          {
            $$= NEW_PTN PT_select_part2_derived($2, $3);
          }
        ;

/* handle contents of parentheses in join expression */
select_derived:
          derived_table_list
          {
            $$= NEW_PTN PT_select_derived(@1, $1);
          }
        ;

opt_outer:
          /* empty */ {}
        | OUTER {}
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
            $$= new (YYTHD->mem_root) List<Index_hint>;
            Index_hint *hint= new (YYTHD->mem_root) Index_hint(NULL, 0);
            if ($$ == NULL || hint == NULL || $$->push_front(hint))
              MYSQL_YYABORT;
          }
        | key_usage_list
        ;

key_usage_element:
          ident
          {
            $$= new (YYTHD->mem_root) Index_hint($1.str, $1.length);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | PRIMARY_SYM
          {
            $$= new (YYTHD->mem_root) Index_hint(STRING_WITH_LEN("PRIMARY"));
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

key_usage_list:
          key_usage_element
          {
            $$= new (YYTHD->mem_root) List<Index_hint>;
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
          ident
          {
            if (!($$= new List<String>))
              MYSQL_YYABORT;
            String *s= new (YYTHD->mem_root) String((const char *) $1.str,
                                                    $1.length,
                                                    system_charset_info);
            if (s == NULL)
              MYSQL_YYABORT;
            $$->push_back(s);
          }
        | using_list ',' ident
          {
            String *s= new (YYTHD->mem_root) String((const char *) $3.str,
                                                    $3.length,
                                                    system_charset_info);
            if (s == NULL)
              MYSQL_YYABORT;
            $1->push_back(s);
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
        | TIMESTAMP {$$= MYSQL_TIMESTAMP_DATETIME; }
        | DATETIME  {$$= MYSQL_TIMESTAMP_DATETIME; }
        ;

table_alias:
          /* empty */
        | AS
        | EQ
        ;

opt_table_alias:
          /* empty */ { $$=0; }
        | table_alias ident
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
          /* empty */  { $$= NULL; }
        | WHERE expr
          {
            $$= new PTI_context<CTX_WHERE>(@$, $2);
          }
        ;

opt_having_clause:
          /* empty */ { $$= NULL; }
        | HAVING expr
          {
            $$= new PTI_context<CTX_HAVING>(@$, $2);
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
        | WITH_CUBE_SYM { $$= CUBE_TYPE; }
            /*
              'WITH CUBE' is reserved in the MySQL syntax, but not implemented,
              and cause LALR(2) conflicts.
              This syntax is not standard.
              MySQL syntax: GROUP BY col1, col2, col3 WITH CUBE
              SQL-2003: GROUP BY ... CUBE(col1, col2, col3)
            */
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
            order->direction= ($2 == 1) ? ORDER::ORDER_ASC : ORDER::ORDER_DESC;
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
          /* empty */ { $$ =  1; }
        | ASC  { $$ =1; }
        | DESC { $$ =0; }
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
          { my_syntax_error(ER(ER_ONLY_INTEGERS_ALLOWED)); }
        ;

dec_num:
          DECIMAL_NUM
        | FLOAT_NUM
        ;

opt_procedure_analyse_clause:
          /* empty */ { $$= NULL; }
        | PROCEDURE_SYM ANALYSE_SYM
          '(' opt_procedure_analyse_params ')'
          {
            push_deprecated_warn_no_replacement(YYTHD, "PROCEDURE ANALYSE");
            $$= NEW_PTN PT_procedure_analyse($4);
          }
        ;

opt_procedure_analyse_params:
          /* empty */
          {
            $$.max_tree_elements= Proc_analyse_params::default_max_tree_elements;
            $$.max_treemem= Proc_analyse_params::default_max_treemem;
          }
        | procedure_analyse_param
          {
            $$.max_tree_elements= static_cast<uint>($1);
            $$.max_treemem= Proc_analyse_params::default_max_treemem;
          }
        | procedure_analyse_param ',' procedure_analyse_param
          {
            $$.max_tree_elements= static_cast<uint>($1);
            $$.max_treemem= static_cast<uint>($3);
          }
        ;

procedure_analyse_param:
          NUM
          {
            int error;
            $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error);
            if (error != 0)
            {
              my_error(ER_WRONG_PARAMETERS_TO_PROCEDURE, MYF(0), "ANALYSE");
              MYSQL_YYABORT;
            }
          }
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
            $$= NEW_PTN PT_select_var_list;
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

opt_into:
          /* empty */ { $$= NULL; }
        | into
        ;

into:
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
            $$= NEW_PTN PT_into_destination_outfile($2, $3, $4, $5);
          }
        | DUMPFILE TEXT_STRING_filesystem
          {
            $$= NEW_PTN PT_into_destination_dumpfile($2);
          }
        | select_var_list { $$= $1; }
        ;

/*
  DO statement
*/

do_stmt:
          DO_SYM empty_select_options select_item_list
          {
            $$= NEW_PTN PT_select(
                  NEW_PTN PT_select_init2(NULL,
                    NEW_PTN PT_select_part2(
                      NEW_PTN PT_select_options_and_item_list($2, $3)), NULL),
                                                              SQLCOM_DO);
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
  Drop : delete tables or index or user
*/

drop:
          DROP opt_temporary table_or_tables if_exists
          {
            LEX *lex=Lex;
            lex->sql_command = SQLCOM_DROP_TABLE;
            lex->drop_temporary= $2;
            lex->drop_if_exists= $4;
            YYPS->m_lock_type= TL_UNLOCK;
            YYPS->m_mdl_type= MDL_EXCLUSIVE;
          }
          table_list opt_restrict
          {}
        | DROP INDEX_SYM ident ON table_ident {}
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
          opt_index_lock_algorithm {}
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
                (check_and_convert_db_name(&$4, FALSE) != IDENT_NAME_OK))
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
        | DROP USER if_exists clear_privileges user_list
          {
             LEX *lex=Lex;
             lex->sql_command= SQLCOM_DROP_USER;
             lex->drop_if_exists= $3;
          }
        | DROP VIEW_SYM if_exists
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_DROP_VIEW;
            lex->drop_if_exists= $3;
            YYPS->m_lock_type= TL_UNLOCK;
            YYPS->m_mdl_type= MDL_EXCLUSIVE;
          }
          table_list opt_restrict
          {}
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
            Lex->m_sql_cmd=
              new (YYTHD->mem_root) Sql_cmd_drop_server($4, $3);
          }
        ;

table_list:
          table_name
        | table_list ',' table_name
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
            $$.init(YYTHD->mem_root);
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
          /* empty */ { $$= 0; }
        | TEMPORARY   { $$= 1; }
        ;

drop_ts_options_list:
          /* empty */
        | drop_ts_options

drop_ts_options:
          drop_ts_option
        | drop_ts_options drop_ts_option
        | drop_ts_options_list ',' drop_ts_option
        ;

drop_ts_option:
          opt_ts_engine
        | ts_wait

/*
** Insert : add new data to table
*/

insert_stmt:
          INSERT                       /* #1 */
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
        | INSERT                       /* #1 */
          insert_lock_option           /* #2 */
          opt_ignore                   /* #3 */
          opt_INTO                     /* #4 */
          table_ident                  /* #5 */
          opt_use_partition            /* #6 */
          SET                          /* #7 */
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
        | INSERT                       /* #1 */
          insert_lock_option           /* #2 */
          opt_ignore                   /* #3 */
          opt_INTO                     /* #4 */
          table_ident                  /* #5 */
          opt_use_partition            /* #6 */
          insert_from_subquery         /* #7 */
          opt_insert_update_list       /* #8 */
          {
            $$= NEW_PTN PT_insert(false, $1, $2, $3, $5, $6,
                                  $7.column_list, NULL,
                                  $7.insert_query_expression,
                                  $8.column_list, $8.value_list);
          }
        ;

replace_stmt:
          REPLACE                       /* #1 */
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
        | REPLACE                       /* #1 */
          replace_lock_option           /* #2 */
          opt_INTO                      /* #3 */
          table_ident                   /* #4 */
          opt_use_partition             /* #5 */
          SET                           /* #6 */
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
        | REPLACE                       /* #1 */
          replace_lock_option           /* #2 */
          opt_INTO                      /* #3 */
          table_ident                   /* #4 */
          opt_use_partition             /* #5 */
          insert_from_subquery          /* #6 */
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
                              ER(ER_WARN_LEGACY_SYNTAX_CONVERTED),
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
                              ER(ER_WARN_LEGACY_SYNTAX_CONVERTED),
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

insert_from_subquery:
          insert_query_expression
          {
            $$.column_list= NEW_PTN PT_item_list;
            $$.insert_query_expression= $1;
          }
        | '(' ')' insert_query_expression
          {
            $$.column_list= NEW_PTN PT_item_list;
            $$.insert_query_expression= $3;
          }
        | '(' fields ')' insert_query_expression
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

insert_query_expression:
          create_select opt_union_clause
          {
            $$= NEW_PTN PT_insert_query_expression(false, $1, $2);
          }
        | '(' create_select ')' union_opt
          {
            $$= NEW_PTN PT_insert_query_expression(true, $2, $4);
          }
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
          EQ {}
        | SET_VAR {}
        ;

opt_equal:
          /* empty */ {}
        | equal {}
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
        | DEFAULT
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
        | ON DUPLICATE_SYM KEY_SYM UPDATE_SYM update_list
          {
            $$= $5;
          }
        ;

/* Update rows in a table */

update_stmt:
          UPDATE_SYM            /* #1 */
          opt_low_priority      /* #2 */
          opt_ignore            /* #3 */
          join_table_list       /* #4 */
          SET                   /* #5 */
          update_list           /* #6 */
          opt_where_clause      /* #7 */
          opt_order_clause      /* #8 */
          opt_simple_limit      /* #9 */
          {
            $$= NEW_PTN PT_update($1, $2, $3, $4, $6.column_list, $6.value_list,
                                  $7, $8, $9);
          }
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
          DELETE_SYM
          opt_delete_options
          FROM
          table_ident
          opt_use_partition
          opt_where_clause
          opt_order_clause
          opt_simple_limit
          {
            $$= NEW_PTN PT_delete(YYTHD->mem_root, $1, $2, $4, $5, $6, $7, $8);
          }
        | DELETE_SYM
          opt_delete_options
          table_alias_ref_list
          FROM
          join_table_list
          opt_where_clause
          {
            $$= NEW_PTN PT_delete($1, $2, $3, $5, $6);
          }
        | DELETE_SYM
          opt_delete_options
          FROM
          table_alias_ref_list
          USING
          join_table_list
          opt_where_clause
          {
            $$= NEW_PTN PT_delete($1, $2, $4, $6, $7);
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
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_truncate_table();
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
            memset(&lex->create_info, 0, sizeof(lex->create_info));
          }
          show_param
        ;

show_param:
           DATABASES opt_wild_or_where
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SHOW_DATABASES;
             if (prepare_schema_table(YYTHD, lex, 0, SCH_SCHEMATA))
               MYSQL_YYABORT;
           }
         | opt_full TABLES opt_db opt_wild_or_where
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SHOW_TABLES;
             lex->select_lex->db= $3;
             if (prepare_schema_table(YYTHD, lex, 0, SCH_TABLE_NAMES))
               MYSQL_YYABORT;
           }
         | opt_full TRIGGERS_SYM opt_db opt_wild_or_where
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SHOW_TRIGGERS;
             lex->select_lex->db= $3;
             if (prepare_schema_table(YYTHD, lex, 0, SCH_TRIGGERS))
               MYSQL_YYABORT;
           }
         | EVENTS_SYM opt_db opt_wild_or_where
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SHOW_EVENTS;
             lex->select_lex->db= $2;
             if (prepare_schema_table(YYTHD, lex, 0, SCH_EVENTS))
               MYSQL_YYABORT;
           }
         | TABLE_SYM STATUS_SYM opt_db opt_wild_or_where
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SHOW_TABLE_STATUS;
             lex->select_lex->db= $3;
             if (prepare_schema_table(YYTHD, lex, 0, SCH_TABLES))
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
        | ENGINE_SYM known_storage_engines show_engine_param
          { Lex->create_info.db_type= $2; }
        | ENGINE_SYM ALL show_engine_param
          { Lex->create_info.db_type= NULL; }
        | opt_full COLUMNS from_or_in table_ident opt_db opt_wild_or_where
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_FIELDS;
            if ($5)
              $4->change_db($5);
            if (prepare_schema_table(YYTHD, lex, $4, SCH_COLUMNS))
              MYSQL_YYABORT;
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
          opt_where_clause      /* #5 */
          {
            if ($5 != NULL)
              ITEMIZE($5, &$5);
            Select->set_where_cond($5);

            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_KEYS;
            if ($4)
              $3->change_db($4);
            if (prepare_schema_table(YYTHD, lex, $3, SCH_STATISTICS))
              MYSQL_YYABORT;
          }
        | opt_storage ENGINES_SYM
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_SHOW_STORAGE_ENGINES;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_ENGINES))
              MYSQL_YYABORT;
          }
        | PRIVILEGES
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_SHOW_PRIVILEGES;
          }
        | COUNT_SYM '(' '*' ')' WARNINGS
          {
            Lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // SHOW WARNINGS doesn't clear them.
            Parse_context pc(YYTHD, Select);
            create_select_for_variable(&pc, "warning_count");
          }
        | COUNT_SYM '(' '*' ')' ERRORS
          {
            Lex->keep_diagnostics= DA_KEEP_DIAGNOSTICS; // SHOW ERRORS doesn't clear them.
            Parse_context pc(YYTHD, Select);
            create_select_for_variable(&pc, "error_count");
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
                                ER(ER_WARN_DEPRECATED_SYNTAX),
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
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            if (show_compatibility_56)
            {
              /* 5.6, DEPRECATED */
              lex->sql_command= SQLCOM_SHOW_STATUS;
              lex->option_type= $1;
              if (prepare_schema_table(YYTHD, lex, 0, SCH_STATUS))
                MYSQL_YYABORT;
            }
            else
            {
              Item *where_cond= Select->where_cond();
              Select->set_where_cond(NULL);

              if ($1 == OPT_SESSION)
              {
                /* 5.7, SUPPORTED */
                if (build_show_session_status(@$, thd, lex->wild, where_cond) == NULL)
                  MYSQL_YYABORT;
              }
              else
              {
                /* 5.7, SUPPORTED */
                if (build_show_global_status(@$, thd, lex->wild, where_cond) == NULL)
                  MYSQL_YYABORT;
              }
            }
          }
        | opt_full PROCESSLIST_SYM
          { Lex->sql_command= SQLCOM_SHOW_PROCESSLIST;}
        | opt_var_type VARIABLES opt_wild_or_where_for_show
          {
            THD *thd= YYTHD;
            LEX *lex= thd->lex;
            if (show_compatibility_56)
            {
              /* 5.6, DEPRECATED */
              lex->sql_command= SQLCOM_SHOW_VARIABLES;
              lex->option_type= $1;
              if (prepare_schema_table(YYTHD, lex, 0, SCH_VARIABLES))
                MYSQL_YYABORT;
            }
            else
            {
              Item *where_cond= Select->where_cond();
              Select->set_where_cond(NULL);

              if ($1 == OPT_SESSION)
              {
                /* 5.7, SUPPORTED */
                if (build_show_session_variables(@$, thd, lex->wild, where_cond) == NULL)
                  MYSQL_YYABORT;
              }
              else
              {
                /* 5.7, SUPPORTED */
                if (build_show_global_variables(@$, thd, lex->wild, where_cond) == NULL)
                  MYSQL_YYABORT;
              }
            }
          }
        | charset opt_wild_or_where
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_CHARSETS;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_CHARSETS))
              MYSQL_YYABORT;
          }
        | COLLATION_SYM opt_wild_or_where
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_COLLATIONS;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_COLLATIONS))
              MYSQL_YYABORT;
          }
        | GRANTS
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_SHOW_GRANTS;
            LEX_USER *curr_user;
            if (!(curr_user= (LEX_USER*) lex->thd->alloc(sizeof(st_lex_user))))
              MYSQL_YYABORT;
            memset(curr_user, 0, sizeof(st_lex_user));
            lex->grant_user= curr_user;
          }
        | GRANTS FOR_SYM user
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_SHOW_GRANTS;
            lex->grant_user=$3;
            lex->grant_user->auth= NULL_CSTR;
          }
        | CREATE DATABASE opt_if_not_exists ident
          {
            Lex->sql_command=SQLCOM_SHOW_CREATE_DB;
            Lex->create_info.options=$3;
            Lex->name= $4;
          }
        | CREATE TABLE_SYM table_ident
          {
            LEX *lex= Lex;
            lex->sql_command = SQLCOM_SHOW_CREATE;
            if (!lex->select_lex->add_table_to_list(YYTHD, $3, NULL,0))
              MYSQL_YYABORT;
            lex->only_view= 0;
            lex->create_info.storage_media= HA_SM_DEFAULT;
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
        | PROCEDURE_SYM STATUS_SYM opt_wild_or_where
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_STATUS_PROC;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_PROCEDURES))
              MYSQL_YYABORT;
          }
        | FUNCTION_SYM STATUS_SYM opt_wild_or_where
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SHOW_STATUS_FUNC;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_PROCEDURES))
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
        | CREATE USER clear_privileges user
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_SHOW_CREATE_USER;
            lex->grant_user=$4;
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
        | BINARY
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
          /* empty */ { Lex->verbose=0; }
        | FULL        { Lex->verbose=1; }
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
            Lex->wild= new (YYTHD->mem_root) String($2.str, $2.length,
                                                    system_charset_info);
            if (Lex->wild == NULL)
              MYSQL_YYABORT;
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
          /* empty */
        | LIKE TEXT_STRING_sys
          {
            Lex->wild= new (YYTHD->mem_root) String($2.str, $2.length,
                                                    system_charset_info);
            if (Lex->wild == NULL)
              MYSQL_YYABORT;
          }
        | WHERE expr
          {
            if (show_compatibility_56)
            {
              /*
                This parsed tree fragment is added as part of a
                SQLCOM_SHOW_STATUS or SQLCOM_SHOW_VARIABLES command.
              */
              ITEMIZE($2, &$2);

              Select->set_where_cond($2);
              if ($2)
                $2->top_level_item();
            }
            else
            {
              /*
                This parsed tree fragment is used to build a
                SQLCOM_SELECT statement, see sql/sql_show_status.cc
              */
              Select->set_where_cond($2);
            }
          }
        ;

/* A Oracle compatible synonym for show */
describe:
          describe_command table_ident
          {
            LEX *lex= Lex;
            lex->current_select()->parsing_place= CTX_SELECT_LIST;
            lex->sql_command= SQLCOM_SHOW_FIELDS;
            lex->select_lex->db= NULL;
            lex->verbose= 0;
            if (prepare_schema_table(YYTHD, lex, $2, SCH_COLUMNS))
              MYSQL_YYABORT;
          }
          opt_describe_column
          {
            // Ensure we're resetting parsing context of the right select
            DBUG_ASSERT(Select->parsing_place == CTX_SELECT_LIST);
            Select->parsing_place= CTX_NONE;
          }
        | describe_command opt_extended_describe
          {
            Lex->describe|= DESCRIBE_NORMAL;
          }
          explainable_command
        ;

explainable_command:
          select  { CONTEXTUALIZE($1); }
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
            Lex->wild= new (YYTHD->mem_root) String((const char*) $1.str,
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
          table_or_tables
          {
            Lex->type|= REFRESH_TABLES;
            /*
              Set type of metadata and table locks for
              FLUSH TABLES table_list [WITH READ LOCK].
            */
            YYPS->m_lock_type= TL_READ_NO_INSERT;
            YYPS->m_mdl_type= MDL_SHARED_HIGH_PRIO;
          }
          opt_table_list {}
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
              tables->required_type= FRMTYPE_TABLE; /* Don't try to flush views. */
              tables->open_type= OT_BASE_ONLY;      /* Ignore temporary tables. */
            }
          }
        | FOR_SYM
          {
            if (Lex->query_tables == NULL) // Table list can't be empty
            {
              my_syntax_error(ER(ER_NO_TABLES_USED));
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
              tables->required_type= FRMTYPE_TABLE; /* Don't try to flush views. */
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
        | BINARY LOGS_SYM
          { Lex->type|= REFRESH_BINARY_LOG; }
        | RELAY LOGS_SYM opt_channel
          { Lex->type|= REFRESH_RELAY_LOG; }
        | QUERY_SYM CACHE_SYM
          {
            push_deprecated_warn_no_replacement(YYTHD, "FLUSH QUERY CACHE");
            Lex->type|= REFRESH_QUERY_CACHE_FREE;
          }
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
          /* empty */  {}
        | table_list {}
        ;

reset:
          RESET_SYM
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_RESET; lex->type=0;
          }
          reset_options
          {}
        ;

reset_options:
          reset_options ',' reset_option
        | reset_option
        ;

reset_option:
          SLAVE               { Lex->type|= REFRESH_SLAVE; }
          slave_reset_options opt_channel
        | MASTER_SYM          { Lex->type|= REFRESH_MASTER; }
        | QUERY_SYM CACHE_SYM
          {
            push_deprecated_warn_no_replacement(YYTHD, "RESET QUERY CACHE");
            Lex->type|= REFRESH_QUERY_CACHE;
          }
        ;

slave_reset_options:
          /* empty */ { Lex->reset_slave_info.all= false; }
        | ALL         { Lex->reset_slave_info.all= true; }
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
          {
            LEX *lex=Lex;
            lex->sql_command= SQLCOM_LOAD;
            lex->local_file=  $5;
            lex->duplicates= DUP_ERROR;
            lex->set_ignore(false);
            if (!(lex->exchange= new sql_exchange($7.str, 0, $2)))
              MYSQL_YYABORT;
          }
          opt_duplicate INTO TABLE_SYM table_ident opt_use_partition
          {
            LEX *lex=Lex;
            /* Fix lock for LOAD DATA CONCURRENT REPLACE */
            if (lex->duplicates == DUP_REPLACE && $4 == TL_WRITE_CONCURRENT_INSERT)
              $4= TL_WRITE_DEFAULT;
            if (!Select->add_table_to_list(YYTHD, $12, NULL, TL_OPTION_UPDATING,
                                           $4, $4 == TL_WRITE_LOW_PRIORITY ?
                                               MDL_SHARED_WRITE_LOW_PRIO :
                                               MDL_SHARED_WRITE, NULL, $13))
              MYSQL_YYABORT;
            lex->load_field_list.empty();
            lex->load_update_list.empty();
            lex->load_value_list.empty();
            /* We can't give an error in the middle when using LOCAL files */
            if (lex->local_file && lex->duplicates == DUP_ERROR)
              lex->set_ignore(true);
          }
          opt_load_data_charset
          { Lex->exchange->cs= $15; }
          opt_xml_rows_identified_by
          opt_field_term opt_line_term opt_ignore_lines opt_field_or_var_spec
          opt_load_data_set_spec
          {
            Lex->exchange->field.merge_field_separators($18);
            Lex->exchange->line.merge_line_separators($19);
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
          /* empty */ { Lex->duplicates=DUP_ERROR; }
        | REPLACE { Lex->duplicates=DUP_REPLACE; }
        | IGNORE_SYM { Lex->set_ignore(true); }
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
            $$= new (YYTHD->mem_root) Item_user_var_as_out_param($2);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

opt_load_data_set_spec:
          /* empty */ {}
        | SET load_data_set_list {}
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
            String *val= new (YYTHD->mem_root) String(@2.cpp.start,
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
            $$= new (YYTHD->mem_root) String($1.str,
                                             $1.length,
                                             YYTHD->variables.collation_connection);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | HEX_NUM
          {
            LEX_STRING s= Item_hex_string::make_hex_str($1.str, $1.length);
            $$= new (YYTHD->mem_root) String(s.str, s.length, &my_charset_bin);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        | BIN_NUM
          {
            LEX_STRING s= Item_bin_string::make_bin_str($1.str, $1.length);
            $$= new (YYTHD->mem_root) String(s.str, s.length, &my_charset_bin);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

param_marker:
          PARAM_MARKER
          {
            $$= NEW_PTN Item_param(@$,
                                   (uint) (@1.raw.start - YYLIP->get_buf()));
          }
        ;

signed_literal:
          literal { ITEMIZE($1, &$$); }
        | '+' NUM_literal { ITEMIZE($2, &$$); }
        | '-' NUM_literal
          {
            ITEMIZE($2, &$2);

            $2->max_length++;
            $$= ((Item_int *)$2)->neg();
          }
        ;


literal:
          text_literal { $$= $1; }
        | NUM_literal
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
            lip->next_state= MY_LEX_OPERATOR_OR_IDENT;
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
            $$= NEW_PTN PTI_num_literal_num(@$, $1);
          }
        | LONG_NUM
          {
            $$= NEW_PTN PTI_num_literal_num(@$, $1);
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
        | TIMESTAMP TEXT_STRING
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
            $$= NEW_PTN PT_order_expr($1, $2);
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
            push_deprecated_warn(YYTHD, ".<table>.<column>",
                                 "the table.column name without a dot prefix");
            $$= NEW_PTN PTI_simple_ident_q_3d(@$, NULL, $2.str, $4.str);
          }
        | ident '.' ident '.' ident
          {
            $$= NEW_PTN PTI_simple_ident_q_3d(@$, $1.str, $3.str, $5.str);
          }
        ;

field_ident:
          ident { $$=$1;}
        | ident '.' ident '.' ident
          {
            TABLE_LIST *table= Select->table_list.first;
            if (my_strcasecmp(table_alias_charset, $1.str, table->db))
            {
              my_error(ER_WRONG_DB_NAME, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
            if (my_strcasecmp(table_alias_charset, $3.str,
                              table->table_name))
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $3.str);
              MYSQL_YYABORT;
            }
            $$=$5;
          }
        | ident '.' ident
          {
            TABLE_LIST *table= Select->table_list.first;
            if (my_strcasecmp(table_alias_charset, $1.str, table->alias))
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $1.str);
              MYSQL_YYABORT;
            }
            $$=$3;
          }
        | '.' ident /* For Delphi */
          {
            push_deprecated_warn(YYTHD, ".<column>", "the column name without a dot prefix");
            $$=$2;
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
            push_deprecated_warn(YYTHD, ".<table>", "the table name without a dot prefix");
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
            $$= NEW_PTN Table_ident(YYTHD, to_lex_cstring($1),
                                    to_lex_cstring($3), 0);
            if ($$ == NULL)
              MYSQL_YYABORT;
          }
        ;

table_ident_nodb:
          ident
          {
            LEX_CSTRING db= { any_db, strlen(any_db) };
            $$= new Table_ident(YYTHD, db, to_lex_cstring($1), 0);
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
        | keyword
          {
            THD *thd= YYTHD;
            $$.str= thd->strmake($1.str, $1.length);
            if ($$.str == NULL)
              MYSQL_YYABORT;
            $$.length= $1.length;
          }
        ;

label_ident:
          IDENT_sys    { $$=$1; }
        | keyword_sp
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

user:
          ident_or_text
          {
            THD *thd= YYTHD;
            if (!($$=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
              MYSQL_YYABORT;

            /*
              Trim whitespace as the values will go to a CHAR field
              when stored.
            */
            trim_whitespace(system_charset_info, &$1);

            $$->user.str= $1.str;
            $$->user.length= $1.length;
            $$->host.str= "%";
            $$->host.length= 1;
            $$->plugin= EMPTY_CSTR;
            $$->auth= NULL_CSTR;
            $$->uses_identified_by_clause= false;
            $$->uses_identified_with_clause= false;
            $$->uses_identified_by_password_clause= false;
            $$->uses_authentication_string_clause= false;

            if (check_string_char_length($$->user, ER(ER_USERNAME),
                                         USERNAME_CHAR_LENGTH,
                                         system_charset_info, 0))
              MYSQL_YYABORT;
          }
        | ident_or_text '@' ident_or_text
          {
            THD *thd= YYTHD;
            if (!($$=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
              MYSQL_YYABORT;

            /*
              Trim whitespace as the values will go to a CHAR field
              when stored.
            */
            trim_whitespace(system_charset_info, &$1);
            trim_whitespace(system_charset_info, &$3);

            $$->user.str= $1.str;
            $$->user.length= $1.length;
            $$->host.str= $3.str;
            $$->host.length= $3.length;
            $$->plugin= EMPTY_CSTR;
            $$->auth= NULL_CSTR;
            $$->uses_identified_by_clause= false;
            $$->uses_identified_with_clause= false;
            $$->uses_identified_by_password_clause= false;
            $$->uses_authentication_string_clause= false;

            if (check_string_char_length($$->user, ER(ER_USERNAME),
                                         USERNAME_CHAR_LENGTH,
                                         system_charset_info, 0) ||
                check_host_name($$->host))
              MYSQL_YYABORT;
            /*
              Convert hostname part of username to lowercase.
              It's OK to use in-place lowercase as long as
              the character set is utf8.
            */
            my_casedn_str(system_charset_info, $3.str);
            $$->host.str= $3.str;
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

/* Keyword that we allow for identifiers (except SP labels) */
keyword:
          keyword_sp            {}
        | ACCOUNT_SYM           {}
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
        | EXECUTE_SYM           {}
        | FLUSH_SYM             {}
        | FOLLOWS_SYM           {}
        | FORMAT_SYM            {}
        | GROUP_REPLICATION     {}
        | HANDLER_SYM           {}
        | HELP_SYM              {}
        | HOST_SYM              {}
        | INSTALL_SYM           {}
        | LANGUAGE_SYM          {}
        | NO_SYM                {}
        | OPEN_SYM              {}
        | OPTIONS_SYM           {}
        | OWNER_SYM             {}
        | PARSER_SYM            {}
        | PARSE_GCOL_EXPR_SYM   {}
        | PORT_SYM              {}
        | PRECEDES_SYM          {}
        | PREPARE_SYM           {}
        | REMOVE_SYM            {}
        | REPAIR                {}
        | RESET_SYM             {}
        | RESTORE_SYM           {}
        | ROLLBACK_SYM          {}
        | SAVEPOINT_SYM         {}
        | SECURITY_SYM          {}
        | SERVER_SYM            {}
        | SHUTDOWN              {}
        | SIGNED_SYM            {}
        | SOCKET_SYM            {}
        | SLAVE                 {}
        | SONAME_SYM            {}
        | START_SYM             {}
        | STOP_SYM              {}
        | TRUNCATE_SYM          {}
        | UNICODE_SYM           {}
        | UNINSTALL_SYM         {}
        | WRAPPER_SYM           {}
        | XA_SYM                {}
        | UPGRADE_SYM           {}
        ;

/*
 * Keywords that we allow for labels in SPs.
 * Anything that's the beginning of a statement or characteristics
 * must be in keyword above, otherwise we get (harmful) shift/reduce
 * conflicts.
 */
keyword_sp:
          ACTION                   {}
        | ADDDATE_SYM              {}
        | AFTER_SYM                {}
        | AGAINST                  {}
        | AGGREGATE_SYM            {}
        | ALGORITHM_SYM            {}
        | ANALYSE_SYM              {}
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
        | CUBE_SYM                 {}
        /*
          Although a reserved keyword in SQL:2003 (and :2008),
          not reserved in MySQL per WL#2111 specification.
        */
        | CURRENT_SYM              {}
        | CURSOR_NAME_SYM          {}
        | DATA_SYM                 {}
        | DATAFILE_SYM             {}
        | DATETIME                 {}
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
        | ENUM                     {}
        | ENGINE_SYM               {}
        | ENGINES_SYM              {}
        | ERROR_SYM                {}
        | ERRORS                   {}
        | ESCAPE_SYM               {}
        | EVENT_SYM                {}
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
        | FILE_SYM                 {}
        | FILE_BLOCK_SIZE_SYM      {}
        | FILTER_SYM               {}
        | FIRST_SYM                {}
        | FIXED_SYM                {}
        | GENERAL                  {}
        | GEOMETRY_SYM             {}
        | GEOMETRYCOLLECTION       {}
        | GET_FORMAT               {}
        | GRANTS                   {}
        | GLOBAL_SYM               {}
        | HASH_SYM                 {}
        | HOSTS_SYM                {}
        | HOUR_SYM                 {}
        | IDENTIFIED_SYM           {}
        | IGNORE_SERVER_IDS_SYM    {}
        | INVOKER_SYM              {}
        | IMPORT                   {}
        | INDEXES                  {}
        | INITIAL_SIZE_SYM         {}
        | INSTANCE_SYM             {}
        | IO_SYM                   {}
        | IPC_SYM                  {}
        | ISOLATION                {}
        | ISSUER_SYM               {}
        | INSERT_METHOD            {}
        | JSON_SYM                 {}
        | KEY_BLOCK_SIZE           {}
        | LAST_SYM                 {}
        | LEAVES                   {}
        | LESS_SYM                 {}
        | LEVEL_SYM                {}
        | LINESTRING               {}
        | LIST_SYM                 {}
        | LOCAL_SYM                {}
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
        | MULTILINESTRING          {}
        | MULTIPOINT               {}
        | MULTIPOLYGON             {}
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
        | NONE_SYM                 {}
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
        | POLYGON                  {}
        | PRESERVE_SYM             {}
        | PREV_SYM                 {}
        | PRIVILEGES               {}
        | PROCESS                  {}
        | PROCESSLIST_SYM          {}
        | PROFILE_SYM              {}
        | PROFILES_SYM             {}
        | PROXY_SYM                {}
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
        | RELOAD                   {}
        | REORGANIZE_SYM           {}
        | REPEATABLE_SYM           {}
        | REPLICATION              {}
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
        | SIMPLE_SYM               {}
        | SHARE_SYM                {}
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
        | SUPER_SYM                {}
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
        | TIMESTAMP                {}
        | TIMESTAMP_ADD            {}
        | TIMESTAMP_DIFF           {}
        | TIME_SYM                 {}
        | TYPES_SYM                {}
        | TYPE_SYM                 {}
        | UDF_RETURNS_SYM          {}
        | FUNCTION_SYM             {}
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
  SQLCOM_SET_OPTION statement.

  Note that to avoid shift/reduce conflicts, we have separate rules for the
  first option listed in the statement.
*/

set:
          SET start_option_value_list
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
        | '@' '@' opt_var_ident_type internal_variable_name equal set_expr_or_default
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
        | DEFAULT '.' ident
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
        | DEFAULT { $$= NULL; }
        | ON
          {
            $$= NEW_PTN Item_string(@$, "ON",  2, system_charset_info);
          }
        | ALL
          {
            $$= NEW_PTN Item_string(@$, "ALL", 3, system_charset_info);
          }
        | BINARY
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
              YYTHD->parse_error_at(@2, ER(ER_SYNTAX_ERROR));
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
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_handler_open();
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
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_handler_close();
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
            Item *one= new (YYTHD->mem_root) Item_int((int32) 1);
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
            lex->m_sql_cmd= new (thd->mem_root) Sql_cmd_handler_read($5,
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
          FIRST_SYM { $$= RFIRST; }
        | NEXT_SYM  { $$= RNEXT;  }
        ;

handler_rkey_function:
          FIRST_SYM { $$= RFIRST; }
        | NEXT_SYM  { $$= RNEXT;  }
        | PREV_SYM  { $$= RPREV;  }
        | LAST_SYM  { $$= RLAST;  }
        | handler_rkey_mode
          {
            YYTHD->m_parser_state->m_yacc.m_ha_rkey_mode= $1;
          }
          '(' values ')'
          {
            CONTEXTUALIZE($4);
            Lex->handler_insert_list= &$4->value;
            $$= RKEY;
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
          REVOKE clear_privileges { Lex->sql_command= SQLCOM_REVOKE; } revoke_command
          {}
        ;

revoke_command:
          grant_privileges ON opt_table grant_ident FROM user_list
          {
            LEX *lex= Lex;
            lex->type= 0;
          }
        | grant_privileges ON FUNCTION_SYM grant_ident FROM user_list
          {
            LEX *lex= Lex;
            if (lex->columns.elements)
            {
              my_syntax_error(ER(ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            lex->type= TYPE_ENUM_FUNCTION;
          }
        | grant_privileges ON PROCEDURE_SYM grant_ident FROM user_list
          {
            LEX *lex= Lex;
            if (lex->columns.elements)
            {
              my_syntax_error(ER(ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            lex->type= TYPE_ENUM_PROCEDURE;
          }
        | ALL opt_privileges ',' GRANT OPTION FROM user_list
          {
            Lex->sql_command = SQLCOM_REVOKE_ALL;
          }
        | PROXY_SYM ON user FROM user_list
          {
            LEX *lex= Lex;
            lex->users_list.push_front ($3);
            lex->type= TYPE_ENUM_PROXY;
          }
        ;

grant:
          GRANT clear_privileges { Lex->sql_command= SQLCOM_GRANT; } grant_command
          {}
        ;

grant_command:
          grant_privileges ON opt_table grant_ident TO_SYM grant_list
          require_clause grant_options
          {
            LEX *lex= Lex;
            lex->type= 0;
          }
        | grant_privileges ON FUNCTION_SYM grant_ident TO_SYM grant_list
          require_clause grant_options
          {
            LEX *lex= Lex;
            if (lex->columns.elements)
            {
              my_syntax_error(ER(ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            lex->type= TYPE_ENUM_FUNCTION;
          }
        | grant_privileges ON PROCEDURE_SYM grant_ident TO_SYM grant_list
          require_clause grant_options
          {
            LEX *lex= Lex;
            if (lex->columns.elements)
            {
              my_syntax_error(ER(ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            lex->type= TYPE_ENUM_PROCEDURE;
          }
        | PROXY_SYM ON user TO_SYM grant_list opt_grant_option
          {
            LEX *lex= Lex;
            lex->users_list.push_front ($3);
            lex->type= TYPE_ENUM_PROXY;
          }
        ;

opt_table:
          /* Empty */
        | TABLE_SYM
        ;

grant_privileges:
          object_privilege_list
          {
            LEX *lex= Lex;
            if (lex->grant == GLOBAL_ACLS &&
                lex->sql_command == SQLCOM_REVOKE)
              lex->sql_command= SQLCOM_REVOKE_ALL;
          }
        | ALL opt_privileges
          {
            Lex->all_privileges= 1;
            Lex->grant= GLOBAL_ACLS;
          }
        ;

opt_privileges:
          /* empty */
        | PRIVILEGES
        ;

object_privilege_list:
          object_privilege
        | object_privilege_list ',' object_privilege
        ;

object_privilege:
          SELECT_SYM
          { Lex->which_columns = SELECT_ACL;}
          opt_column_list {}
        | INSERT
          { Lex->which_columns = INSERT_ACL;}
          opt_column_list {}
        | UPDATE_SYM
          { Lex->which_columns = UPDATE_ACL; }
          opt_column_list {}
        | REFERENCES
          { Lex->which_columns = REFERENCES_ACL;}
          opt_column_list {}
        | DELETE_SYM              { Lex->grant |= DELETE_ACL;}
        | USAGE                   {}
        | INDEX_SYM               { Lex->grant |= INDEX_ACL;}
        | ALTER                   { Lex->grant |= ALTER_ACL;}
        | CREATE                  { Lex->grant |= CREATE_ACL;}
        | DROP                    { Lex->grant |= DROP_ACL;}
        | EXECUTE_SYM             { Lex->grant |= EXECUTE_ACL;}
        | RELOAD                  { Lex->grant |= RELOAD_ACL;}
        | SHUTDOWN                { Lex->grant |= SHUTDOWN_ACL;}
        | PROCESS                 { Lex->grant |= PROCESS_ACL;}
        | FILE_SYM                { Lex->grant |= FILE_ACL;}
        | GRANT OPTION            { Lex->grant |= GRANT_ACL;}
        | SHOW DATABASES          { Lex->grant |= SHOW_DB_ACL;}
        | SUPER_SYM               { Lex->grant |= SUPER_ACL;}
        | CREATE TEMPORARY TABLES { Lex->grant |= CREATE_TMP_ACL;}
        | LOCK_SYM TABLES         { Lex->grant |= LOCK_TABLES_ACL; }
        | REPLICATION SLAVE       { Lex->grant |= REPL_SLAVE_ACL; }
        | REPLICATION CLIENT_SYM  { Lex->grant |= REPL_CLIENT_ACL; }
        | CREATE VIEW_SYM         { Lex->grant |= CREATE_VIEW_ACL; }
        | SHOW VIEW_SYM           { Lex->grant |= SHOW_VIEW_ACL; }
        | CREATE ROUTINE_SYM      { Lex->grant |= CREATE_PROC_ACL; }
        | ALTER ROUTINE_SYM       { Lex->grant |= ALTER_PROC_ACL; }
        | CREATE USER             { Lex->grant |= CREATE_USER_ACL; }
        | EVENT_SYM               { Lex->grant |= EVENT_ACL;}
        | TRIGGER_SYM             { Lex->grant |= TRIGGER_ACL; }
        | CREATE TABLESPACE_SYM   { Lex->grant |= CREATE_TABLESPACE_ACL; }
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
              my_message(ER_ILLEGAL_GRANT_FOR_TABLE,
                         ER(ER_ILLEGAL_GRANT_FOR_TABLE), MYF(0));
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
              my_message(ER_ILLEGAL_GRANT_FOR_TABLE,
                         ER(ER_ILLEGAL_GRANT_FOR_TABLE), MYF(0));
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
              my_message(ER_ILLEGAL_GRANT_FOR_TABLE,
                         ER(ER_ILLEGAL_GRANT_FOR_TABLE), MYF(0));
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
            if (Lex->users_list.push_back($1))
              MYSQL_YYABORT;
          }
        | user_list ',' user
          {
            if (Lex->users_list.push_back($3))
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
              my_syntax_error(ER_THD(YYTHD, ER_SYNTAX_ERROR));
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
          /* empty */
          {
            LEX *lex=Lex;
            lex->grant |= lex->which_columns;
          }
        | '(' column_list ')'
        ;

column_list:
          column_list ',' column_list_id
        | column_list_id
        ;

column_list_id:
          ident
          {
            String *new_str = new (YYTHD->mem_root) String((const char*) $1.str,$1.length,system_charset_info);
            if (new_str == NULL)
              MYSQL_YYABORT;
            List_iterator <LEX_COLUMN> iter(Lex->columns);
            class LEX_COLUMN *point;
            LEX *lex=Lex;
            while ((point=iter++))
            {
              if (!my_strcasecmp(system_charset_info,
                                 point->column.ptr(), new_str->ptr()))
                break;
            }
            lex->grant_tot_col|= lex->which_columns;
            if (point)
              point->rights |= lex->which_columns;
            else
            {
              LEX_COLUMN *col= new LEX_COLUMN (*new_str,lex->which_columns);
              if (col == NULL)
                MYSQL_YYABORT;
              lex->columns.push_back(col);
            }
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
          /* empty */ {}
        | WITH GRANT OPTION { Lex->grant |= GRANT_ACL;}
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

begin:
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


opt_union_clause:
          /* empty */ { $$= NULL; }
        | union_list
        ;

union_list:
          UNION_SYM union_option select_init
          {
            $$= NEW_PTN PT_union_list($2, $3);
          }
        ;

union_opt:
          /* Empty */          { $$= NULL; }
        | union_list           { $$= $1; }
        | union_order_or_limit { $$= $1; }
        ;

opt_union_order_or_limit:
          /* Empty */          { $$= NULL; }
        | union_order_or_limit { $$= $1; }
        ;

union_order_or_limit:
          order_or_limit
          {
            $$= NEW_PTN PT_union_order_or_limit($1);
          }
        ;

order_or_limit:
          order_clause opt_limit_clause
          {
            $$= NEW_PTN PT_order_or_limit_order($1, $2);
          }
        | limit_clause { $$= $1; }
        ;

union_option:
          /* empty */ { $$=1; }
        | DISTINCT  { $$=1; }
        | ALL       { $$=0; }
        ;

query_specification:
          SELECT_SYM select_part2_derived table_expression
          {
            $$= NEW_PTN PT_query_specification_select($1, $2, $3);
          }
        | '(' select_paren_derived ')'
          opt_union_order_or_limit
          {
            $$= NEW_PTN PT_query_specification_parenthesis($2, $4);
          }
        ;

query_expression_body:
          query_specification
        | query_expression_body UNION_SYM union_option query_specification
          {
            $$= NEW_PTN PT_query_expression_body_union(@$, $1, $3, $4);
          }
        ;

/* Corresponds to <query expression> in the SQL:2003 standard. */
subselect:
          {
            /*
              TODO: remove this semantic action (currently this removal
              adds reduce/reduce conflict)
            */
          }
          query_expression_body
          {
            $$= NEW_PTN PT_subselect(@$, $2);
          }
        ;

opt_query_spec_options:
          /* empty */ { $$= 0; }
        | query_spec_option_list
        ;

query_spec_option_list:
          query_spec_option_list query_spec_option
          {
            $$= $1 | $2;
          }
        | query_spec_option
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
          OR_SYM REPLACE
          { Lex->create_view_mode= VIEW_CREATE_OR_REPLACE; }
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
          view_suid VIEW_SYM table_ident
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
          }
          view_list_opt AS view_select
        ;

view_list_opt:
          /* empty */
          {}
        | '(' view_list ')'
        ;

view_list:
          ident
            {
              Lex->view_list.push_back((LEX_STRING*)
              sql_memdup(&$1, sizeof(LEX_STRING)));
            }
        | view_list ',' ident
            {
              Lex->view_list.push_back((LEX_STRING*)
              sql_memdup(&$3, sizeof(LEX_STRING)));
            }
        ;

view_select:
          {
            LEX *lex= Lex;
            lex->parsing_options.allows_variable= FALSE;
            lex->parsing_options.allows_select_into= FALSE;
            lex->parsing_options.allows_select_procedure= FALSE;
          }
          view_select_aux view_check_option
          {
            THD *thd= YYTHD;
            LEX *lex= Lex;

            lex->create_view_select.str= const_cast<char *>(@2.cpp.start);
            size_t len= @3.cpp.end - lex->create_view_select.str;
            void *create_view_select= thd->memdup(lex->create_view_select.str, len);
            lex->create_view_select.length= len;
            lex->create_view_select.str= (char *) create_view_select;
            trim_whitespace(thd->charset(), &lex->create_view_select);
            lex->parsing_options.allows_variable= TRUE;
            lex->parsing_options.allows_select_into= TRUE;
            lex->parsing_options.allows_select_procedure= TRUE;
          }
        ;

view_select_aux:
          create_view_select
          {
            if (Lex->current_select()->set_braces(0))
            {
              my_syntax_error(ER(ER_SYNTAX_ERROR));
              MYSQL_YYABORT;
            }
            /*
              For statment as "CREATE VIEW v1 AS SELECT1 UNION SELECT2",
              parsing of Select query (SELECT1) is completed and UNION_CLAUSE
              is not yet parsed. So check for
              Lex->current_select()->master_unit()->first_select()->braces
              (as its done in "PT_select_init2::contextualize()) is not
              done here.
            */
          }
          opt_union_clause
          {
            if ($3 != NULL)
              CONTEXTUALIZE($3);
          }
        | '(' create_view_select_paren ')' union_opt
          {
            if ($4 != NULL)
              CONTEXTUALIZE($4);
          }
        ;

create_view_select_paren:
          {
            Lex->current_select()->set_braces(true);
          }
          create_view_select
          {
            if (setup_select_in_parentheses(Select))
              MYSQL_YYABORT;
          }
        | '(' create_view_select_paren ')'
        ;

create_view_select:
          SELECT_SYM
          {
            Lex->current_select()->table_list.save_and_clear(&Lex->save_list);
          }
          select_part2
          {
            CONTEXTUALIZE($3);

            Lex->current_select()->table_list.push_front(&Lex->save_list);
          }
        ;

view_check_option:
          /* empty */
          { Lex->create_view_check= VIEW_CHECK_NONE; }
        | WITH CHECK_SYM OPTION
          { Lex->create_view_check= VIEW_CHECK_CASCADED; }
        | WITH CASCADED CHECK_SYM OPTION
          { Lex->create_view_check= VIEW_CHECK_CASCADED; }
        | WITH LOCAL_SYM CHECK_SYM OPTION
          { Lex->create_view_check= VIEW_CHECK_LOCAL; }
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
              $$.anchor_trigger_name.str= NULL;
              $$.anchor_trigger_name.length= 0;
            }
          |
            trigger_action_order ident_or_text
            {
              $$.ordering_clause= $1;
              $$.anchor_trigger_name= $2;
            }
          ;

trigger_tail:
          TRIGGER_SYM       /* $1 */
          sp_name           /* $2 */
          trg_action_time   /* $3 */
          trg_event         /* $4 */
          ON                /* $5 */
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

            lex->raw_trg_on_table_name_begin= @5.raw.start;
            lex->raw_trg_on_table_name_end= @7.raw.start;

            if (@10.is_empty())
            {
              /*
                @10.is_empty() is true when a clause PRECEDES/FOLLOWS is absent.
              */
              lex->trg_ordering_clause_begin= NULL;
              lex->trg_ordering_clause_end= NULL;
            }
            else
            {
              lex->trg_ordering_clause_begin= @10.cpp.start;
              lex->trg_ordering_clause_end= @10.cpp.end;
            }

            sp_head *sp= sp_start_parsing(thd, SP_TYPE_TRIGGER, $2);

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

            sp->set_body_start(thd, @9.cpp.end);
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
            if (is_native_function(thd, & $3))
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
            if (is_native_function(thd, & $2))
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

            sp_head *sp= sp_start_parsing(thd, SP_TYPE_FUNCTION, lex->spname);

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
          { /* $9 */
            LEX *lex= Lex;
            lex->charset= NULL;
            lex->length= lex->dec= NULL;
            lex->interval_list.empty();
            lex->type= 0;
            lex->gcol_info= 0;
          }
          type_with_opt_collate /* $10 */
          { /* $11 */
            LEX *lex= Lex;
            sp_head *sp= lex->sphead;
            /*
              This was disabled in 5.1.12. See bug #20701
              When collation support in SP is implemented, then this test
              should be removed.
            */
            if (($10 == MYSQL_TYPE_STRING || $10 == MYSQL_TYPE_VARCHAR)
                && (lex->type & BINCMP_FLAG))
            {
              my_error(ER_NOT_SUPPORTED_YET, MYF(0), "return value collation");
              MYSQL_YYABORT;
            }

            if (fill_field_definition(YYTHD, sp,
                                      (enum enum_field_types) $10,
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

            if (is_native_function(thd, & sp->m_name))
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
                                  ER(ER_NATIVE_FCT_NAME_COLLISION),
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

            sp_head *sp= sp_start_parsing(thd, SP_TYPE_PROCEDURE, $2);

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
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_xa_start($3, $4);
          }
        | XA_SYM END xid opt_suspend
          {
            Lex->sql_command = SQLCOM_XA_END;
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_xa_end($3, $4);
          }
        | XA_SYM PREPARE_SYM xid
          {
            Lex->sql_command = SQLCOM_XA_PREPARE;
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_xa_prepare($3);
          }
        | XA_SYM COMMIT_SYM xid opt_one_phase
          {
            Lex->sql_command = SQLCOM_XA_COMMIT;
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_xa_commit($3, $4);
          }
        | XA_SYM ROLLBACK_SYM xid
          {
            Lex->sql_command = SQLCOM_XA_ROLLBACK;
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_xa_rollback($3);
          }
        | XA_SYM RECOVER_SYM opt_convert_xid
          {
            Lex->sql_command = SQLCOM_XA_RECOVER;
            Lex->m_sql_cmd= new (YYTHD->mem_root) Sql_cmd_xa_recover($3);
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
        ;

uninstall:
          UNINSTALL_SYM PLUGIN_SYM ident
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_UNINSTALL_PLUGIN;
            lex->m_sql_cmd= new Sql_cmd_uninstall_plugin($3);
          }
        ;

/**
  @} (end of group Parser)
*/
