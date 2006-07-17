/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* sql_yacc.yy */

%{
/* thd is passed as an arg to yyparse(), and subsequently to yylex().
** The type will be void*, so it must be  cast to (THD*) when used.
** Use the YYTHD macro for this.
*/
#define YYPARSE_PARAM yythd
#define YYLEX_PARAM yythd
#define YYTHD ((THD *)yythd)

#define MYSQL_YACC
#define YYINITDEPTH 100
#define YYMAXDEPTH 3200				/* Because of 64K stack */
#define Lex (YYTHD->lex)
#define Select Lex->current_select
#include "mysql_priv.h"
#include "slave.h"
#include "lex_symbol.h"
#include "item_create.h"
#include "sp_head.h"
#include "sp_pcontext.h"
#include "sp_rcontext.h"
#include "sp.h"
#include <myisam.h>
#include <myisammrg.h>

int yylex(void *yylval, void *yythd);

const LEX_STRING null_lex_str={0,0};

#define yyoverflow(A,B,C,D,E,F) {ulong val= *(F); if (my_yyoverflow((B), (D), &val)) { yyerror((char*) (A)); return 2; } else { *(F)= (YYSIZE_T)val; }}

#define WARN_DEPRECATED(A,B)                                        \
  push_warning_printf(((THD *)yythd), MYSQL_ERROR::WARN_LEVEL_WARN, \
		      ER_WARN_DEPRECATED_SYNTAX,                    \
		      ER(ER_WARN_DEPRECATED_SYNTAX), (A), (B));

#define YYERROR_UNLESS(A)                  \
  if (!(A))                             \
  {					\
    yyerror(ER(ER_SYNTAX_ERROR));	\
    YYABORT;				\
  }

/* Helper for parsing "IS [NOT] truth_value" */
inline Item *is_truth_value(Item *A, bool v1, bool v2)
{
  return new Item_func_if(create_func_ifnull(A,
	new Item_int((char *) (v2 ? "TRUE" : "FALSE"), v2, 1)),
	new Item_int((char *) (v1 ? "TRUE" : "FALSE"), v1, 1),
	new Item_int((char *) (v1 ? "FALSE" : "TRUE"),!v1, 1));
}

%}
%union {
  int  num;
  ulong ulong_num;
  ulonglong ulonglong_number;
  LEX_STRING lex_str;
  LEX_STRING *lex_str_ptr;
  LEX_SYMBOL symbol;
  Table_ident *table;
  char *simple_string;
  Item *item;
  Item_num *item_num;
  List<Item> *item_list;
  List<String> *string_list;
  String *string;
  key_part_spec *key_part;
  TABLE_LIST *table_list;
  udf_func *udf;
  LEX_USER *lex_user;
  struct sys_var_with_base variable;
  enum enum_var_type var_type;
  Key::Keytype key_type;
  enum ha_key_alg key_alg;
  enum db_type db_type;
  enum row_type row_type;
  enum ha_rkey_function ha_rkey_mode;
  enum enum_tx_isolation tx_isolation;
  enum Cast_target cast_type;
  enum Item_udftype udf_type;
  CHARSET_INFO *charset;
  thr_lock_type lock_type;
  interval_type interval, interval_time_st;
  timestamp_type date_time_type;
  st_select_lex *select_lex;
  chooser_compare_func_creator boolfunc2creator;
  struct sp_cond_type *spcondtype;
  struct { int vars, conds, hndlrs, curs; } spblock;
  sp_name *spname;
  struct st_lex *lex;
}

%{
bool my_yyoverflow(short **a, YYSTYPE **b, ulong *yystacksize);
%}

%pure_parser					/* We have threads */

%token  END_OF_INPUT

%token  ABORT_SYM
%token  ACTION
%token  ADD
%token  ADDDATE_SYM
%token  AFTER_SYM
%token  AGAINST
%token  AGGREGATE_SYM
%token  ALGORITHM_SYM
%token  ALL
%token  ALTER
%token  ANALYZE_SYM
%token  AND_AND_SYM
%token  AND_SYM
%token  ANY_SYM
%token  AS
%token  ASC
%token  ASCII_SYM
%token  ASENSITIVE_SYM
%token  ATAN
%token  AUTO_INC
%token  AVG_ROW_LENGTH
%token  AVG_SYM
%token  BACKUP_SYM
%token  BEFORE_SYM
%token  BEGIN_SYM
%token  BENCHMARK_SYM
%token  BERKELEY_DB_SYM
%token  BIGINT
%token  BINARY
%token  BINLOG_SYM
%token  BIN_NUM
%token  BIT_AND
%token  BIT_OR
%token  BIT_SYM
%token  BIT_XOR
%token  BLOB_SYM
%token  BOOLEAN_SYM
%token  BOOL_SYM
%token  BOTH
%token  BTREE_SYM
%token  BY
%token  BYTE_SYM
%token  CACHE_SYM
%token  CALL_SYM
%token  CASCADE
%token  CASCADED
%token  CAST_SYM
%token  CHAIN_SYM
%token  CHANGE
%token  CHANGED
%token  CHARSET
%token  CHAR_SYM
%token  CHECKSUM_SYM
%token  CHECK_SYM
%token  CIPHER_SYM
%token  CLIENT_SYM
%token  CLOSE_SYM
%token  COALESCE
%token  CODE_SYM
%token  COLLATE_SYM
%token  COLLATION_SYM
%token  COLUMNS
%token  COLUMN_SYM
%token  COMMENT_SYM
%token  COMMITTED_SYM
%token  COMMIT_SYM
%token  COMPACT_SYM
%token  COMPRESSED_SYM
%token  CONCAT
%token  CONCAT_WS
%token  CONCURRENT
%token  CONDITION_SYM
%token  CONNECTION_SYM
%token  CONSISTENT_SYM
%token  CONSTRAINT
%token  CONTAINS_SYM
%token  CONTINUE_SYM
%token  CONVERT_SYM
%token  CONVERT_TZ_SYM
%token  COUNT_SYM
%token  CREATE
%token  CROSS
%token  CUBE_SYM
%token  CURDATE
%token  CURRENT_USER
%token  CURSOR_SYM
%token  CURTIME
%token  DATABASE
%token  DATABASES
%token  DATA_SYM
%token  DATETIME
%token  DATE_ADD_INTERVAL
%token  DATE_SUB_INTERVAL
%token  DATE_SYM
%token  DAY_HOUR_SYM
%token  DAY_MICROSECOND_SYM
%token  DAY_MINUTE_SYM
%token  DAY_SECOND_SYM
%token  DAY_SYM
%token  DEALLOCATE_SYM
%token  DECIMAL_NUM
%token  DECIMAL_SYM
%token  DECLARE_SYM
%token  DECODE_SYM
%token  DEFAULT
%token  DEFINER_SYM
%token  DELAYED_SYM
%token  DELAY_KEY_WRITE_SYM
%token  DELETE_SYM
%token  DESC
%token  DESCRIBE
%token  DES_DECRYPT_SYM
%token  DES_ENCRYPT_SYM
%token  DES_KEY_FILE
%token  DETERMINISTIC_SYM
%token  DIRECTORY_SYM
%token  DISABLE_SYM
%token  DISCARD
%token  DISTINCT
%token  DIV_SYM
%token  DOUBLE_SYM
%token  DO_SYM
%token  DROP
%token  DUAL_SYM
%token  DUMPFILE
%token  DUPLICATE_SYM
%token  DYNAMIC_SYM
%token  EACH_SYM
%token  ELSEIF_SYM
%token  ELT_FUNC
%token  ENABLE_SYM
%token  ENCLOSED
%token  ENCODE_SYM
%token  ENCRYPT
%token  END
%token  ENGINES_SYM
%token  ENGINE_SYM
%token  ENUM
%token  EQ
%token  EQUAL_SYM
%token  ERRORS
%token  ESCAPED
%token  ESCAPE_SYM
%token  EVENTS_SYM
%token  EXECUTE_SYM
%token  EXISTS
%token  EXIT_SYM
%token  EXPANSION_SYM
%token  EXPORT_SET
%token  EXTENDED_SYM
%token  EXTRACT_SYM
%token  FALSE_SYM
%token  FAST_SYM
%token  FETCH_SYM
%token  FIELD_FUNC
%token  FILE_SYM
%token  FIRST_SYM
%token  FIXED_SYM
%token  FLOAT_NUM
%token  FLOAT_SYM
%token  FLUSH_SYM
%token  FORCE_SYM
%token  FOREIGN
%token  FORMAT_SYM
%token  FOR_SYM
%token  FOUND_SYM
%token  FRAC_SECOND_SYM
%token  FROM
%token  FROM_UNIXTIME
%token  FULL
%token  FULLTEXT_SYM
%token  FUNCTION_SYM
%token  FUNC_ARG0
%token  FUNC_ARG1
%token  FUNC_ARG2
%token  FUNC_ARG3
%token  GE
%token  GEOMCOLLFROMTEXT
%token  GEOMETRYCOLLECTION
%token  GEOMETRY_SYM
%token  GEOMFROMTEXT
%token  GEOMFROMWKB
%token  GET_FORMAT
%token  GLOBAL_SYM
%token  GRANT
%token  GRANTS
%token  GREATEST_SYM
%token  GROUP
%token  GROUP_CONCAT_SYM
%token  GROUP_UNIQUE_USERS
%token  GT_SYM
%token  HANDLER_SYM
%token  HASH_SYM
%token  HAVING
%token  HELP_SYM
%token  HEX_NUM
%token  HIGH_PRIORITY
%token  HOSTS_SYM
%token  HOUR_MICROSECOND_SYM
%token  HOUR_MINUTE_SYM
%token  HOUR_SECOND_SYM
%token  HOUR_SYM
%token  IDENT
%token  IDENTIFIED_SYM
%token  IDENT_QUOTED
%token  IF
%token  IGNORE_SYM
%token  IMPORT
%token  INDEXES
%token  INDEX_SYM
%token  INFILE
%token  INNER_SYM
%token  INNOBASE_SYM
%token  INOUT_SYM
%token  INSENSITIVE_SYM
%token  INSERT
%token  INSERT_METHOD
%token  INTERVAL_SYM
%token  INTO
%token  INT_SYM
%token  INVOKER_SYM
%token  IN_SYM
%token  IS
%token  ISOLATION
%token  ISSUER_SYM
%token  ITERATE_SYM
%token  JOIN_SYM
%token  KEYS
%token  KEY_SYM
%token  KILL_SYM
%token  LABEL_SYM
%token  LANGUAGE_SYM
%token  LAST_INSERT_ID
%token  LAST_SYM
%token  LE
%token  LEADING
%token  LEAST_SYM
%token  LEAVES
%token  LEAVE_SYM
%token  LEFT
%token  LEVEL_SYM
%token  LEX_HOSTNAME
%token  LIKE
%token  LIMIT
%token  LINEFROMTEXT
%token  LINES
%token  LINESTRING
%token  LOAD
%token  LOCAL_SYM
%token  LOCATE
%token  LOCATOR_SYM
%token  LOCKS_SYM
%token  LOCK_SYM
%token  LOGS_SYM
%token  LOG_SYM
%token  LONGBLOB
%token  LONGTEXT
%token  LONG_NUM
%token  LONG_SYM
%token  LOOP_SYM
%token  LOW_PRIORITY
%token  LT
%token  MAKE_SET_SYM
%token  MASTER_CONNECT_RETRY_SYM
%token  MASTER_HOST_SYM
%token  MASTER_LOG_FILE_SYM
%token  MASTER_LOG_POS_SYM
%token  MASTER_PASSWORD_SYM
%token  MASTER_PORT_SYM
%token  MASTER_POS_WAIT
%token  MASTER_SERVER_ID_SYM
%token  MASTER_SSL_CAPATH_SYM
%token  MASTER_SSL_CA_SYM
%token  MASTER_SSL_CERT_SYM
%token  MASTER_SSL_CIPHER_SYM
%token  MASTER_SSL_KEY_SYM
%token  MASTER_SSL_SYM
%token  MASTER_SYM
%token  MASTER_USER_SYM
%token  MATCH
%token  MAX_CONNECTIONS_PER_HOUR
%token  MAX_QUERIES_PER_HOUR
%token  MAX_ROWS
%token  MAX_SYM
%token  MAX_UPDATES_PER_HOUR
%token  MAX_USER_CONNECTIONS_SYM
%token  MEDIUMBLOB
%token  MEDIUMINT
%token  MEDIUMTEXT
%token  MEDIUM_SYM
%token  MERGE_SYM
%token  MICROSECOND_SYM
%token  MIGRATE_SYM
%token  MINUTE_MICROSECOND_SYM
%token  MINUTE_SECOND_SYM
%token  MINUTE_SYM
%token  MIN_ROWS
%token  MIN_SYM
%token  MLINEFROMTEXT
%token  MODE_SYM
%token  MODIFIES_SYM
%token  MODIFY_SYM
%token  MOD_SYM
%token  MONTH_SYM
%token  MPOINTFROMTEXT
%token  MPOLYFROMTEXT
%token  MULTILINESTRING
%token  MULTIPOINT
%token  MULTIPOLYGON
%token  MUTEX_SYM
%token  NAMES_SYM
%token  NAME_SYM
%token  NATIONAL_SYM
%token  NATURAL
%token  NCHAR_STRING
%token  NCHAR_SYM
%token  NDBCLUSTER_SYM
%token  NE
%token  NEW_SYM
%token  NEXT_SYM
%token  NONE_SYM
%token  NOT2_SYM
%token  NOT_SYM
%token  NOW_SYM
%token  NO_SYM
%token  NO_WRITE_TO_BINLOG
%token  NULL_SYM
%token  NUM
%token  NUMERIC_SYM
%token  NVARCHAR_SYM
%token  OFFSET_SYM
%token  OJ_SYM
%token  OLD_PASSWORD
%token  ON
%token  ONE_SHOT_SYM
%token  ONE_SYM
%token  OPEN_SYM
%token  OPTIMIZE
%token  OPTION
%token  OPTIONALLY
%token  OR2_SYM
%token  ORDER_SYM
%token  OR_OR_SYM
%token  OR_SYM
%token  OUTER
%token  OUTFILE
%token  OUT_SYM
%token  PACK_KEYS_SYM
%token  PARTIAL
%token  PASSWORD
%token  PARAM_MARKER
%token  PHASE_SYM
%token  POINTFROMTEXT
%token  POINT_SYM
%token  POLYFROMTEXT
%token  POLYGON
%token  POSITION_SYM
%token  PRECISION
%token  PREPARE_SYM
%token  PREV_SYM
%token  PRIMARY_SYM
%token  PRIVILEGES
%token  PROCEDURE
%token  PROCESS
%token  PROCESSLIST_SYM
%token  PURGE
%token  QUARTER_SYM
%token  QUERY_SYM
%token  QUICK
%token  RAID_0_SYM
%token  RAID_CHUNKS
%token  RAID_CHUNKSIZE
%token  RAID_STRIPED_SYM
%token  RAID_TYPE
%token  RAND
%token  READS_SYM
%token  READ_SYM
%token  REAL
%token  RECOVER_SYM
%token  REDUNDANT_SYM
%token  REFERENCES
%token  REGEXP
%token  RELAY_LOG_FILE_SYM
%token  RELAY_LOG_POS_SYM
%token  RELAY_THREAD
%token  RELEASE_SYM
%token  RELOAD
%token  RENAME
%token  REPAIR
%token  REPEATABLE_SYM
%token  REPEAT_SYM
%token  REPLACE
%token  REPLICATION
%token  REQUIRE_SYM
%token  RESET_SYM
%token  RESOURCES
%token  RESTORE_SYM
%token  RESTRICT
%token  RESUME_SYM
%token  RETURNS_SYM
%token  RETURN_SYM
%token  REVOKE
%token  RIGHT
%token  ROLLBACK_SYM
%token  ROLLUP_SYM
%token  ROUND
%token  ROUTINE_SYM
%token  ROWS_SYM
%token  ROW_COUNT_SYM
%token  ROW_FORMAT_SYM
%token  ROW_SYM
%token  RTREE_SYM
%token  SAVEPOINT_SYM
%token  SECOND_MICROSECOND_SYM
%token  SECOND_SYM
%token  SECURITY_SYM
%token  SELECT_SYM
%token  SENSITIVE_SYM
%token  SEPARATOR_SYM
%token  SERIALIZABLE_SYM
%token  SERIAL_SYM
%token  SESSION_SYM
%token  SET
%token  SET_VAR
%token  SHARE_SYM
%token  SHIFT_LEFT
%token  SHIFT_RIGHT
%token  SHOW
%token  SHUTDOWN
%token  SIGNED_SYM
%token  SIMPLE_SYM
%token  SLAVE
%token  SMALLINT
%token  SNAPSHOT_SYM
%token  SOUNDS_SYM
%token  SPATIAL_SYM
%token  SPECIFIC_SYM
%token  SQLEXCEPTION_SYM
%token  SQLSTATE_SYM
%token  SQLWARNING_SYM
%token  SQL_BIG_RESULT
%token  SQL_BUFFER_RESULT
%token  SQL_CACHE_SYM
%token  SQL_CALC_FOUND_ROWS
%token  SQL_NO_CACHE_SYM
%token  SQL_SMALL_RESULT
%token  SQL_SYM
%token  SQL_THREAD
%token  SSL_SYM
%token  STARTING
%token  START_SYM
%token  STATUS_SYM
%token  STD_SYM
%token  STDDEV_SAMP_SYM
%token  STOP_SYM
%token  STORAGE_SYM
%token  STRAIGHT_JOIN
%token  STRING_SYM
%token  SUBDATE_SYM
%token  SUBJECT_SYM
%token  SUBSTRING
%token  SUBSTRING_INDEX
%token  SUM_SYM
%token  SUPER_SYM
%token  SUSPEND_SYM
%token  SYSDATE
%token  TABLES
%token  TABLESPACE
%token  TABLE_SYM
%token  TEMPORARY
%token  TEMPTABLE_SYM
%token  TERMINATED
%token  TEXT_STRING
%token  TEXT_SYM
%token  TIMESTAMP
%token  TIMESTAMP_ADD
%token  TIMESTAMP_DIFF
%token  TIME_SYM
%token  TINYBLOB
%token  TINYINT
%token  TINYTEXT
%token  TO_SYM
%token  TRAILING
%token  TRANSACTION_SYM
%token  TRIGGER_SYM
%token  TRIGGERS_SYM
%token  TRIM
%token  TRUE_SYM
%token  TRUNCATE_SYM
%token  TYPES_SYM
%token  TYPE_SYM
%token  UDF_RETURNS_SYM
%token  UDF_SONAME_SYM
%token  ULONGLONG_NUM
%token  UNCOMMITTED_SYM
%token  UNDEFINED_SYM
%token  UNDERSCORE_CHARSET
%token  UNDO_SYM
%token  UNICODE_SYM
%token  UNION_SYM
%token  UNIQUE_SYM
%token  UNIQUE_USERS
%token  UNIX_TIMESTAMP
%token  UNKNOWN_SYM
%token  UNLOCK_SYM
%token  UNLOCK_SYM
%token  UNSIGNED
%token  UNTIL_SYM
%token  UNTIL_SYM
%token  UPDATE_SYM
%token  UPDATE_SYM
%token  UPGRADE_SYM
%token  USAGE
%token  USER
%token  USE_FRM
%token  USE_SYM
%token  USING
%token  UTC_DATE_SYM
%token  UTC_TIMESTAMP_SYM
%token  UTC_TIME_SYM
%token  VAR_SAMP_SYM
%token  VALUES
%token  VALUE_SYM
%token  VARBINARY
%token  VARCHAR
%token  VARIABLES
%token  VARIANCE_SYM
%token  VARYING
%token  VIEW_SYM
%token  WARNINGS
%token  WEEK_SYM
%token  WHEN_SYM
%token  WHERE
%token  WHILE_SYM
%token  WITH
%token  WORK_SYM
%token  WRITE_SYM
%token  X509_SYM
%token  XA_SYM
%token  XOR
%token  YEARWEEK
%token  YEAR_MONTH_SYM
%token  YEAR_SYM
%token  ZEROFILL

%left   JOIN_SYM INNER_SYM STRAIGHT_JOIN CROSS LEFT RIGHT
/* A dummy token to force the priority of table_ref production in a join. */
%left   TABLE_REF_PRIORITY
%left   SET_VAR
%left	OR_OR_SYM OR_SYM OR2_SYM XOR
%left	AND_SYM AND_AND_SYM
%left	BETWEEN_SYM CASE_SYM WHEN_SYM THEN_SYM ELSE
%left	EQ EQUAL_SYM GE GT_SYM LE LT NE IS LIKE REGEXP IN_SYM
%left	'|'
%left	'&'
%left	SHIFT_LEFT SHIFT_RIGHT
%left	'-' '+'
%left	'*' '/' '%' DIV_SYM MOD_SYM
%left   '^'
%left	NEG '~'
%right	NOT_SYM NOT2_SYM
%right	BINARY COLLATE_SYM

%type <lex_str>
        IDENT IDENT_QUOTED TEXT_STRING DECIMAL_NUM FLOAT_NUM NUM LONG_NUM HEX_NUM
	LEX_HOSTNAME ULONGLONG_NUM field_ident select_alias ident ident_or_text
        UNDERSCORE_CHARSET IDENT_sys TEXT_STRING_sys TEXT_STRING_literal
	NCHAR_STRING opt_component key_cache_name
        sp_opt_label BIN_NUM label_ident TEXT_STRING_filesystem

%type <lex_str_ptr>
	opt_table_alias

%type <table>
	table_ident table_ident_nodb references xid

%type <simple_string>
	remember_name remember_end opt_ident opt_db text_or_password
	opt_constraint constraint ident_or_empty

%type <string>
	text_string opt_gconcat_separator

%type <num>
	type int_type real_type order_dir lock_option
	udf_type if_exists opt_local opt_table_options table_options
        table_option opt_if_not_exists opt_no_write_to_binlog
        delete_option opt_temporary all_or_any opt_distinct
        opt_ignore_leaves fulltext_options spatial_type union_option
        start_transaction_opts opt_chain opt_release
        union_opt select_derived_init option_type2

%type <ulong_num>
	ulong_num raid_types merge_insert_types

%type <ulonglong_number>
	ulonglong_num

%type <lock_type>
	replace_lock_option opt_low_priority insert_lock_option load_data_lock

%type <item>
	literal text_literal insert_ident order_ident
	simple_ident select_item2 expr opt_expr opt_else sum_expr in_sum_expr
	bool_term bool_factor bool_test bool_pri 
	predicate bit_expr bit_term bit_factor value_expr term factor
	table_wild simple_expr udf_expr
	expr_or_default set_expr_or_default interval_expr
	param_marker singlerow_subselect singlerow_subselect_init
	exists_subselect exists_subselect_init geometry_function
	signed_literal now_or_signed_literal opt_escape
	sp_opt_default
	simple_ident_nospvar simple_ident_q
        field_or_var limit_option

%type <item_num>
	NUM_literal

%type <item_list>
	expr_list udf_expr_list udf_expr_list2 when_list
	ident_list ident_list_arg

%type <var_type>
        option_type opt_var_type opt_var_ident_type

%type <key_type>
	key_type opt_unique_or_fulltext constraint_key_type

%type <key_alg>
	key_alg opt_btree_or_rtree

%type <string_list>
	key_usage_list using_list

%type <key_part>
	key_part

%type <table_list>
	join_table_list  join_table
        table_factor table_ref
        select_derived derived_table_list

%type <date_time_type> date_time_type;
%type <interval> interval

%type <interval_time_st> interval_time_st

%type <db_type> storage_engines

%type <row_type> row_types

%type <tx_isolation> isolation_types

%type <ha_rkey_mode> handler_rkey_mode

%type <cast_type> cast_type

%type <udf_type> udf_func_type

%type <symbol> FUNC_ARG0 FUNC_ARG1 FUNC_ARG2 FUNC_ARG3 keyword keyword_sp

%type <lex_user> user grant_user

%type <charset>
	opt_collate
	charset_name
	charset_name_or_default
	old_or_new_charset_name
	old_or_new_charset_name_or_default
	collation_name
	collation_name_or_default

%type <variable> internal_variable_name

%type <select_lex> in_subselect in_subselect_init
	get_select_lex

%type <boolfunc2creator> comp_op

%type <NONE>
	query verb_clause create change select do drop insert replace insert2
	insert_values update delete truncate rename
	show describe load alter optimize keycache preload flush
	reset purge begin commit rollback savepoint release
	slave master_def master_defs master_file_def slave_until_opts
	repair restore backup analyze check start checksum
	field_list field_list_item field_spec kill column_def key_def
	keycache_list assign_to_keycache preload_list preload_keys
	select_item_list select_item values_list no_braces
	opt_limit_clause delete_limit_clause fields opt_values values
	procedure_list procedure_list2 procedure_item
	when_list2 expr_list2 udf_expr_list3 handler
	opt_precision opt_ignore opt_column opt_restrict
	grant revoke set lock unlock string_list field_options field_option
	field_opt_list opt_binary table_lock_list table_lock
	ref_list opt_on_delete opt_on_delete_list opt_on_delete_item use
	opt_delete_options opt_delete_option varchar nchar nvarchar
	opt_outer table_list table_name opt_option opt_place
	opt_attribute opt_attribute_list attribute column_list column_list_id
	opt_column_list grant_privileges grant_ident grant_list grant_option
	object_privilege object_privilege_list user_list rename_list
	clear_privileges flush_options flush_option
	equal optional_braces opt_key_definition key_usage_list2
	opt_mi_check_type opt_to mi_check_types normal_join
	table_to_table_list table_to_table opt_table_list opt_as
	handler_rkey_function handler_read_or_scan
	single_multi table_wild_list table_wild_one opt_wild
	union_clause union_list
	precision subselect_start opt_and charset
	subselect_end select_var_list select_var_list_init help opt_len
	opt_extended_describe
        prepare prepare_src execute deallocate
	statement sp_suid
	sp_c_chistics sp_a_chistics sp_chistic sp_c_chistic xa
        load_data opt_field_or_var_spec fields_or_vars opt_load_data_set_spec
        definer view_replace_or_algorithm view_replace view_algorithm_opt
        view_algorithm view_or_trigger_or_sp view_or_trigger_or_sp_tail
	view_suid view_tail view_list_opt view_list view_select
	view_check_option trigger_tail sp_tail
END_OF_INPUT

%type <NONE> call sp_proc_stmts sp_proc_stmts1 sp_proc_stmt
%type <num>  sp_decl_idents sp_opt_inout sp_handler_type sp_hcond_list
%type <spcondtype> sp_cond sp_hcond
%type <spblock> sp_decls sp_decl
%type <lex> sp_cursor_stmt
%type <spname> sp_name

%type <NONE>
	'-' '+' '*' '/' '%' '(' ')'
	',' '!' '{' '}' '&' '|' AND_SYM OR_SYM OR_OR_SYM BETWEEN_SYM CASE_SYM
	THEN_SYM WHEN_SYM DIV_SYM MOD_SYM OR2_SYM AND_AND_SYM
%%


query:
	END_OF_INPUT
	{
	   THD *thd= YYTHD;
	   if (!thd->bootstrap &&
	      (!(thd->lex->select_lex.options & OPTION_FOUND_COMMENT)))
	   {
	     my_message(ER_EMPTY_QUERY, ER(ER_EMPTY_QUERY), MYF(0));
	     YYABORT;
	   }
	   else
	   {
	     thd->lex->sql_command= SQLCOM_EMPTY_QUERY;
	   }
	}
	| verb_clause END_OF_INPUT {};

verb_clause:
	  statement
	| begin
	;

/* Verb clauses, except begin */
statement:
	  alter
	| analyze
	| backup
	| call
	| change
	| check
	| checksum
	| commit
	| create
        | deallocate
	| delete
	| describe
	| do
	| drop
        | execute
	| flush
	| grant
	| handler
	| help
	| insert
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
	| replace
	| reset
	| restore
	| revoke
	| rollback
	| savepoint
	| select
	| set
	| show
	| slave
	| start
	| truncate
	| unlock
	| update
	| use
	| xa
        ;

deallocate:
        deallocate_or_drop PREPARE_SYM ident
        {
          THD *thd=YYTHD;
          LEX *lex= thd->lex;
          if (lex->stmt_prepare_mode)
          {
            yyerror(ER(ER_SYNTAX_ERROR));
            YYABORT;
          }
          lex->sql_command= SQLCOM_DEALLOCATE_PREPARE;
          lex->prepared_stmt_name= $3;
        };

deallocate_or_drop:
	DEALLOCATE_SYM |
	DROP
	;


prepare:
        PREPARE_SYM ident FROM prepare_src
        {
          THD *thd=YYTHD;
          LEX *lex= thd->lex;
          if (lex->stmt_prepare_mode)
          {
            yyerror(ER(ER_SYNTAX_ERROR));
            YYABORT;
          }
          lex->sql_command= SQLCOM_PREPARE;
          lex->prepared_stmt_name= $2;
        };

prepare_src:
        TEXT_STRING_sys
        {
          THD *thd=YYTHD;
          LEX *lex= thd->lex;
          lex->prepared_stmt_code= $1;
          lex->prepared_stmt_code_is_varref= FALSE;
        }
        | '@' ident_or_text
        {
          THD *thd=YYTHD;
          LEX *lex= thd->lex;
          lex->prepared_stmt_code= $2;
          lex->prepared_stmt_code_is_varref= TRUE;
        };

execute:
        EXECUTE_SYM ident
        {
          THD *thd=YYTHD;
          LEX *lex= thd->lex;
          if (lex->stmt_prepare_mode)
          {
            yyerror(ER(ER_SYNTAX_ERROR));
            YYABORT;
          }
          lex->sql_command= SQLCOM_EXECUTE;
          lex->prepared_stmt_name= $2;
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

execute_var_ident: '@' ident_or_text
        {
          LEX *lex=Lex;
          LEX_STRING *lexstr= (LEX_STRING*)sql_memdup(&$2, sizeof(LEX_STRING));
          if (!lexstr || lex->prepared_stmt_params.push_back(lexstr))
              YYABORT;
        }
        ;

/* help */

help:
       HELP_SYM
       {
         if (Lex->sphead)
         {
           my_error(ER_SP_BADSTATEMENT, MYF(0), "HELP");
           YYABORT;
         }
       }
       ident_or_text
       {
	  LEX *lex= Lex;
	  lex->sql_command= SQLCOM_HELP;
	  lex->help_arg= $3.str;
       };

/* change master */

change:
       CHANGE MASTER_SYM TO_SYM
        {
	  LEX *lex = Lex;
	  lex->sql_command = SQLCOM_CHANGE_MASTER;
	  bzero((char*) &lex->mi, sizeof(lex->mi));
        }
       master_defs
	{}
       ;

master_defs:
       master_def
       | master_defs ',' master_def;

master_def:
       MASTER_HOST_SYM EQ TEXT_STRING_sys
       {
	 Lex->mi.host = $3.str;
       }
       |
       MASTER_USER_SYM EQ TEXT_STRING_sys
       {
	 Lex->mi.user = $3.str;
       }
       |
       MASTER_PASSWORD_SYM EQ TEXT_STRING_sys
       {
	 Lex->mi.password = $3.str;
       }
       |
       MASTER_PORT_SYM EQ ulong_num
       {
	 Lex->mi.port = $3;
       }
       |
       MASTER_CONNECT_RETRY_SYM EQ ulong_num
       {
	 Lex->mi.connect_retry = $3;
       }
       | MASTER_SSL_SYM EQ ulong_num
         {
           Lex->mi.ssl= $3 ? 
               LEX_MASTER_INFO::SSL_ENABLE : LEX_MASTER_INFO::SSL_DISABLE;
         }
       | MASTER_SSL_CA_SYM EQ TEXT_STRING_sys
         {
           Lex->mi.ssl_ca= $3.str;
         }
       | MASTER_SSL_CAPATH_SYM EQ TEXT_STRING_sys
         {
           Lex->mi.ssl_capath= $3.str;
         }
       | MASTER_SSL_CERT_SYM EQ TEXT_STRING_sys
         {
           Lex->mi.ssl_cert= $3.str;
         }
       | MASTER_SSL_CIPHER_SYM EQ TEXT_STRING_sys
         {
           Lex->mi.ssl_cipher= $3.str;
         }
       | MASTER_SSL_KEY_SYM EQ TEXT_STRING_sys
         {
           Lex->mi.ssl_key= $3.str;
	 }
       |
         master_file_def
       ;

master_file_def:
       MASTER_LOG_FILE_SYM EQ TEXT_STRING_sys
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
           Lex->mi.pos = max(BIN_LOG_HEADER_SIZE, Lex->mi.pos);
         }
       | RELAY_LOG_FILE_SYM EQ TEXT_STRING_sys
         {
           Lex->mi.relay_log_name = $3.str;
         }
       | RELAY_LOG_POS_SYM EQ ulong_num
         {
           Lex->mi.relay_log_pos = $3;
           /* Adjust if < BIN_LOG_HEADER_SIZE (same comment as Lex->mi.pos) */
           Lex->mi.relay_log_pos = max(BIN_LOG_HEADER_SIZE, Lex->mi.relay_log_pos);
         }
       ;

/* create a table */

create:
	CREATE opt_table_options TABLE_SYM opt_if_not_exists table_ident
	{
	  THD *thd= YYTHD;
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_CREATE_TABLE;
	  if (!lex->select_lex.add_table_to_list(thd, $5, NULL,
						 TL_OPTION_UPDATING,
						 (using_update_log ?
						  TL_READ_NO_INSERT:
						  TL_READ)))
	    YYABORT;
	  lex->create_list.empty();
	  lex->key_list.empty();
	  lex->col_list.empty();
	  lex->change=NullS;
	  bzero((char*) &lex->create_info,sizeof(lex->create_info));
	  lex->create_info.options=$2 | $4;
	  lex->create_info.db_type= (enum db_type) lex->thd->variables.table_type;
	  lex->create_info.default_table_charset= NULL;
	  lex->name=0;
	}
	create2
	  { Lex->current_select= &Lex->select_lex; }
	| CREATE opt_unique_or_fulltext INDEX_SYM ident key_alg ON table_ident
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_CREATE_INDEX;
	    if (!lex->current_select->add_table_to_list(lex->thd, $7, NULL,
							TL_OPTION_UPDATING))
	      YYABORT;
	    lex->create_list.empty();
	    lex->key_list.empty();
	    lex->col_list.empty();
	    lex->change=NullS;
	  }
	   '(' key_list ')'
	  {
	    LEX *lex=Lex;

	    lex->key_list.push_back(new Key($2,$4.str, $5, 0, lex->col_list));
	    lex->col_list.empty();
	  }
	| CREATE DATABASE opt_if_not_exists ident
	  {
             Lex->create_info.default_table_charset= NULL;
             Lex->create_info.used_fields= 0;
          }
	  opt_create_database_options
	  {
	    LEX *lex=Lex;
	    lex->sql_command=SQLCOM_CREATE_DB;
	    lex->name=$4.str;
            lex->create_info.options=$3;
	  }
	| CREATE
	  {
            Lex->create_view_mode= VIEW_CREATE_NEW;
            Lex->create_view_algorithm= VIEW_ALGORITHM_UNDEFINED;
            Lex->create_view_suid= TRUE;
	  }
	  view_or_trigger_or_sp
	  {}
	| CREATE USER clear_privileges grant_list
	  {
	    Lex->sql_command = SQLCOM_CREATE_USER;
          }
	;

clear_privileges:
        /* Nothing */
        {
          LEX *lex=Lex;
          lex->users_list.empty();
          lex->columns.empty();
          lex->grant= lex->grant_tot_col= 0;
	  lex->all_privileges= 0;
          lex->select_lex.db= 0;
          lex->ssl_type= SSL_TYPE_NOT_SPECIFIED;
          lex->ssl_cipher= lex->x509_subject= lex->x509_issuer= 0;
          bzero((char *)&(lex->mqh),sizeof(lex->mqh));
        }
        ;

sp_name:
	  ident '.' ident
	  {
            if (!$1.str || check_db_name($1.str))
            {
	      my_error(ER_WRONG_DB_NAME, MYF(0), $1.str);
	      YYABORT;
	    }
	    if (check_routine_name($3))
            {
	      my_error(ER_SP_WRONG_NAME, MYF(0), $3.str);
	      YYABORT;
	    }
	    $$= new sp_name($1, $3);
	    $$->init_qname(YYTHD);
	  }
	| ident
	  {
            THD *thd= YYTHD;
            LEX_STRING db;
	    if (check_routine_name($1))
            {
	      my_error(ER_SP_WRONG_NAME, MYF(0), $1.str);
	      YYABORT;
	    }
            if (thd->copy_db_to(&db.str, &db.length))
              YYABORT;
	    $$= new sp_name(db, $1);
            if ($$)
	      $$->init_qname(YYTHD);
	  }
	;

create_function_tail:
	  RETURNS_SYM udf_type UDF_SONAME_SYM TEXT_STRING_sys
	  {
	    LEX *lex=Lex;
	    lex->sql_command = SQLCOM_CREATE_FUNCTION;
	    lex->udf.name = lex->spname->m_name;
	    lex->udf.returns=(Item_result) $2;
	    lex->udf.dl=$4.str;
	  }
	| '('
	  {
	    LEX *lex= Lex;
	    sp_head *sp;

            /* 
              First check if AGGREGATE was used, in that case it's a
              syntax error.
            */
            if (lex->udf.type == UDFTYPE_AGGREGATE)
            {
              my_error(ER_SP_NO_AGGREGATE, MYF(0));
              YYABORT;
            }

	    if (lex->sphead)
	    {
	      my_error(ER_SP_NO_RECURSIVE_CREATE, MYF(0), "FUNCTION");
	      YYABORT;
	    }
	    /* Order is important here: new - reset - init */
	    sp= new sp_head();
	    sp->reset_thd_mem_root(YYTHD);
	    sp->init(lex);

	    sp->m_type= TYPE_ENUM_FUNCTION;
	    lex->sphead= sp;
	    /*
	     * We have to turn of CLIENT_MULTI_QUERIES while parsing a
	     * stored procedure, otherwise yylex will chop it into pieces
	     * at each ';'.
	     */
	    sp->m_old_cmq= YYTHD->client_capabilities & CLIENT_MULTI_QUERIES;
	    YYTHD->client_capabilities &= ~CLIENT_MULTI_QUERIES;
	    lex->sphead->m_param_begin= lex->tok_start+1;
	  }
          sp_fdparam_list ')'
	  {
	    LEX *lex= Lex;

	    lex->sphead->m_param_end= lex->tok_start;
	  }
	  RETURNS_SYM
	  {
	    LEX *lex= Lex;
	    lex->charset= NULL;
	    lex->length= lex->dec= NULL;
	    lex->interval_list.empty();
	    lex->type= 0;
	  }
	  type
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;

            if (sp->fill_field_definition(YYTHD, lex,
                                          (enum enum_field_types) $8,
                                          &sp->m_return_field_def))
              YYABORT;

	    bzero((char *)&lex->sp_chistics, sizeof(st_sp_chistics));
	  }
	  sp_c_chistics
	  {
	    LEX *lex= Lex;

	    lex->sphead->m_chistics= &lex->sp_chistics;
	    lex->sphead->m_body_begin= lex->tok_start;
	  }
	  sp_proc_stmt
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;

            if (sp->is_not_allowed_in_function("function"))
              YYABORT;

	    lex->sql_command= SQLCOM_CREATE_SPFUNCTION;
	    sp->init_strings(YYTHD, lex, lex->spname);
            if (!(sp->m_flags & sp_head::HAS_RETURN))
            {
              my_error(ER_SP_NORETURN, MYF(0), sp->m_qname.str);
              YYABORT;
            }
	    /* Restore flag if it was cleared above */
	    if (sp->m_old_cmq)
	      YYTHD->client_capabilities |= CLIENT_MULTI_QUERIES;
	    sp->restore_thd_mem_root(YYTHD);
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
	  { }
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
	    lex->value_list.empty();
	    sp_add_used_routine(lex, YYTHD, $2, TYPE_ENUM_PROCEDURE);
	  }
          '(' sp_cparam_list ')' {}
	;

/* CALL parameters */
sp_cparam_list:
	  /* Empty */
	| sp_cparams
	;

sp_cparams:
	  sp_cparams ',' expr
	  {
	    Lex->value_list.push_back($3);
	  }
	| expr
	  {
	    Lex->value_list.push_back($1);
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
	  ident sp_init_param type
	  {
	    LEX *lex= Lex;
	    sp_pcontext *spc= lex->spcont;

	    if (spc->find_variable(&$1, TRUE))
	    {
	      my_error(ER_SP_DUP_PARAM, MYF(0), $1.str);
	      YYABORT;
	    }
            sp_variable_t *spvar= spc->push_variable(&$1,
                                                     (enum enum_field_types)$3,
                                                     sp_param_in);

            if (lex->sphead->fill_field_definition(YYTHD, lex,
                                                   (enum enum_field_types) $3,
                                                   &spvar->field_def))
            {
              YYABORT;
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
	  sp_opt_inout sp_init_param ident type
	  {
	    LEX *lex= Lex;
	    sp_pcontext *spc= lex->spcont;

	    if (spc->find_variable(&$3, TRUE))
	    {
	      my_error(ER_SP_DUP_PARAM, MYF(0), $3.str);
	      YYABORT;
	    }
            sp_variable_t *spvar= spc->push_variable(&$3,
                                                     (enum enum_field_types)$4,
                                                     (sp_param_mode_t)$1);

            if (lex->sphead->fill_field_definition(YYTHD, lex,
                                                   (enum enum_field_types) $4,
                                                   &spvar->field_def))
            {
              YYABORT;
            }
            spvar->field_def.field_name= spvar->name.str;
            spvar->field_def.pack_flag |= FIELDFLAG_MAYBE_NULL;
	  }
	;

sp_opt_inout:
	  /* Empty */ { $$= sp_param_in; }
	| IN_SYM      { $$= sp_param_in; }
	| OUT_SYM     { $$= sp_param_out; }
	| INOUT_SYM   { $$= sp_param_inout; }
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
	      YYABORT;
	    }
	    if ($2.curs && $1.hndlrs)
	    { /* Cursor following handler */
	      my_message(ER_SP_CURSOR_AFTER_HANDLER,
                         ER(ER_SP_CURSOR_AFTER_HANDLER), MYF(0));
	      YYABORT;
	    }
	    $$.vars= $1.vars + $2.vars;
	    $$.conds= $1.conds + $2.conds;
	    $$.hndlrs= $1.hndlrs + $2.hndlrs;
	    $$.curs= $1.curs + $2.curs;
	  }
	;

sp_decl:
          DECLARE_SYM sp_decl_idents
          {
            LEX *lex= Lex;

            lex->sphead->reset_lex(YYTHD);
            lex->spcont->declare_var_boundary($2);
          }
          type
          sp_opt_default
          {
            LEX *lex= Lex;
            sp_pcontext *pctx= lex->spcont;
            uint num_vars= pctx->context_var_count();
            enum enum_field_types var_type= (enum enum_field_types) $4;
            Item *dflt_value_item= $5;
            create_field *create_field_op;
            
            if (!dflt_value_item)
            {
              dflt_value_item= new Item_null();
              /* QQ Set to the var_type with null_value? */
            }
            
            for (uint i = num_vars-$2 ; i < num_vars ; i++)
            {
              uint var_idx= pctx->var_context2runtime(i);
              sp_variable_t *spvar= pctx->find_variable(var_idx);
            
              if (!spvar)
                YYABORT;
            
              spvar->type= var_type;
              spvar->dflt= dflt_value_item;
            
              if (lex->sphead->fill_field_definition(YYTHD, lex, var_type,
                                                     &spvar->field_def))
              {
                YYABORT;
              }
            
              spvar->field_def.field_name= spvar->name.str;
              spvar->field_def.pack_flag |= FIELDFLAG_MAYBE_NULL;
            
              /* The last instruction is responsible for freeing LEX. */

              lex->sphead->add_instr(
                new sp_instr_set(lex->sphead->instructions(), pctx, var_idx,
                                 dflt_value_item, var_type, lex,
                                 (i == num_vars - 1)));
            }

            pctx->declare_var_boundary(0);
            lex->sphead->restore_lex(YYTHD);

            $$.vars= $2;
            $$.conds= $$.hndlrs= $$.curs= 0;
          }
	| DECLARE_SYM ident CONDITION_SYM FOR_SYM sp_cond
	  {
	    LEX *lex= Lex;
	    sp_pcontext *spc= lex->spcont;

	    if (spc->find_cond(&$2, TRUE))
	    {
	      my_error(ER_SP_DUP_COND, MYF(0), $2.str);
	      YYABORT;
	    }
	    YYTHD->lex->spcont->push_cond(&$2, $5);
	    $$.vars= $$.hndlrs= $$.curs= 0;
	    $$.conds= 1;
	  }
	| DECLARE_SYM sp_handler_type HANDLER_SYM FOR_SYM
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *ctx= lex->spcont;
	    sp_instr_hpush_jump *i=
              new sp_instr_hpush_jump(sp->instructions(), ctx, $2,
	                              ctx->current_var_count());

	    sp->add_instr(i);
	    sp->push_backpatch(i, ctx->push_label((char *)"", 0));
	    sp->m_flags|= sp_head::IN_HANDLER;
	  }
	  sp_hcond_list sp_proc_stmt
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *ctx= lex->spcont;
	    sp_label_t *hlab= lex->spcont->pop_label(); /* After this hdlr */
	    sp_instr_hreturn *i;

	    if ($2 == SP_HANDLER_CONTINUE)
	    {
	      i= new sp_instr_hreturn(sp->instructions(), ctx,
	                              ctx->current_var_count());
	      sp->add_instr(i);
	    }
	    else
	    {  /* EXIT or UNDO handler, just jump to the end of the block */
	      i= new sp_instr_hreturn(sp->instructions(), ctx, 0);

	      sp->add_instr(i);
	      sp->push_backpatch(i, lex->spcont->last_label()); /* Block end */
	    }
	    lex->sphead->backpatch(hlab);
	    sp->m_flags&= ~sp_head::IN_HANDLER;
	    $$.vars= $$.conds= $$.curs= 0;
	    $$.hndlrs= $6;
	    ctx->add_handlers($6);
	  }
	| DECLARE_SYM ident CURSOR_SYM FOR_SYM sp_cursor_stmt
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *ctx= lex->spcont;
	    uint offp;
	    sp_instr_cpush *i;

	    if (ctx->find_cursor(&$2, &offp, TRUE))
	    {
	      my_error(ER_SP_DUP_CURS, MYF(0), $2.str);
	      delete $5;
	      YYABORT;
	    }
            i= new sp_instr_cpush(sp->instructions(), ctx, $5,
                                  ctx->current_cursor_count());
	    sp->add_instr(i);
	    ctx->push_cursor(&$2);
	    $$.vars= $$.conds= $$.hndlrs= 0;
	    $$.curs= 1;
	  }
	;

sp_cursor_stmt:
	  {
	    Lex->sphead->reset_lex(YYTHD);

	    /* We use statement here just be able to get a better
	       error message. Using 'select' works too, but will then
	       result in a generic "syntax error" if a non-select
	       statement is given. */
	  }
	  statement
	  {
	    LEX *lex= Lex;

	    if (lex->sql_command != SQLCOM_SELECT)
	    {
	      my_message(ER_SP_BAD_CURSOR_QUERY, ER(ER_SP_BAD_CURSOR_QUERY),
                         MYF(0));
	      YYABORT;
	    }
	    if (lex->result)
	    {
	      my_message(ER_SP_BAD_CURSOR_SELECT, ER(ER_SP_BAD_CURSOR_SELECT),
                         MYF(0));
	      YYABORT;
	    }
	    lex->sp_lex_in_use= TRUE;
	    $$= lex;
	    lex->sphead->restore_lex(YYTHD);
	  }
	;

sp_handler_type:
	  EXIT_SYM      { $$= SP_HANDLER_EXIT; }
	| CONTINUE_SYM  { $$= SP_HANDLER_CONTINUE; }
/*	| UNDO_SYM      { QQ No yet } */
	;

sp_hcond_list:
	  sp_hcond
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *ctx= lex->spcont;

	    if (ctx->find_handler($1))
	    {
	      my_message(ER_SP_DUP_HANDLER, ER(ER_SP_DUP_HANDLER), MYF(0));
	      YYABORT;
	    }
	    else
	    {
	      sp_instr_hpush_jump *i=
                (sp_instr_hpush_jump *)sp->last_instruction();

	      i->add_condition($1);
	      ctx->push_handler($1);
	      $$= 1;
	    }
	  }
	| sp_hcond_list ',' sp_hcond
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *ctx= lex->spcont;

	    if (ctx->find_handler($3))
	    {
	      my_message(ER_SP_DUP_HANDLER, ER(ER_SP_DUP_HANDLER), MYF(0));
	      YYABORT;
	    }
	    else
	    {
	      sp_instr_hpush_jump *i=
	        (sp_instr_hpush_jump *)sp->last_instruction();

	      i->add_condition($3);
	      ctx->push_handler($3);
	      $$= $1 + 1;
	    }
	  }
	;

sp_cond:
	  ulong_num
	  {			/* mysql errno */
	    $$= (sp_cond_type_t *)YYTHD->alloc(sizeof(sp_cond_type_t));
	    $$->type= sp_cond_type_t::number;
	    $$->mysqlerr= $1;
	  }
	| SQLSTATE_SYM opt_value TEXT_STRING_literal
	  {		/* SQLSTATE */
	    if (!sp_cond_check(&$3))
	    {
	      my_error(ER_SP_BAD_SQLSTATE, MYF(0), $3.str);
	      YYABORT;
	    }
	    $$= (sp_cond_type_t *)YYTHD->alloc(sizeof(sp_cond_type_t));
	    $$->type= sp_cond_type_t::state;
	    memcpy($$->sqlstate, $3.str, 5);
	    $$->sqlstate[5]= '\0';
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
	| ident			/* CONDITION name */
	  {
	    $$= Lex->spcont->find_cond(&$1);
	    if ($$ == NULL)
	    {
	      my_error(ER_SP_COND_MISMATCH, MYF(0), $1.str);
	      YYABORT;
	    }
	  }
	| SQLWARNING_SYM	/* SQLSTATEs 01??? */
	  {
	    $$= (sp_cond_type_t *)YYTHD->alloc(sizeof(sp_cond_type_t));
	    $$->type= sp_cond_type_t::warning;
	  }
	| not FOUND_SYM		/* SQLSTATEs 02??? */
	  {
	    $$= (sp_cond_type_t *)YYTHD->alloc(sizeof(sp_cond_type_t));
	    $$->type= sp_cond_type_t::notfound;
	  }
	| SQLEXCEPTION_SYM	/* All other SQLSTATEs */
	  {
	    $$= (sp_cond_type_t *)YYTHD->alloc(sizeof(sp_cond_type_t));
	    $$->type= sp_cond_type_t::exception;
	  }
	;

sp_decl_idents:
	  ident
	  {
            /* NOTE: field definition is filled in sp_decl section. */

	    LEX *lex= Lex;
	    sp_pcontext *spc= lex->spcont;

	    if (spc->find_variable(&$1, TRUE))
	    {
	      my_error(ER_SP_DUP_VAR, MYF(0), $1.str);
	      YYABORT;
	    }
	    spc->push_variable(&$1, (enum_field_types)0, sp_param_in);
	    $$= 1;
	  }
	| sp_decl_idents ',' ident
	  {
            /* NOTE: field definition is filled in sp_decl section. */

	    LEX *lex= Lex;
	    sp_pcontext *spc= lex->spcont;

	    if (spc->find_variable(&$3, TRUE))
	    {
	      my_error(ER_SP_DUP_VAR, MYF(0), $3.str);
	      YYABORT;
	    }
	    spc->push_variable(&$3, (enum_field_types)0, sp_param_in);
	    $$= $1 + 1;
	  }
	;

sp_opt_default:
	  /* Empty */ { $$ = NULL; }
        | DEFAULT expr { $$ = $2; }
	;

sp_proc_stmt:
	  {
	    LEX *lex= Lex;

	    lex->sphead->reset_lex(YYTHD);
	    lex->sphead->m_tmp_query= lex->tok_start;
	  }
	  statement
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;

            sp->m_flags|= sp_get_flags_for_command(lex);
	    if (lex->sql_command == SQLCOM_CHANGE_DB)
	    { /* "USE db" doesn't work in a procedure */
	      my_error(ER_SP_BADSTATEMENT, MYF(0), "USE");
	      YYABORT;
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
              sp_instr_stmt *i=new sp_instr_stmt(sp->instructions(),
                                                 lex->spcont, lex);

              /* Extract the query statement from the tokenizer:
                 The end is either lex->tok_end or tok->ptr. */
              if (lex->ptr - lex->tok_end > 1)
                i->m_query.length= lex->ptr - sp->m_tmp_query;
              else
                i->m_query.length= lex->tok_end - sp->m_tmp_query;
              i->m_query.str= strmake_root(YYTHD->mem_root,
                                           (char *)sp->m_tmp_query,
                                           i->m_query.length);
              sp->add_instr(i);
            }
	    sp->restore_lex(YYTHD);
          }
          | RETURN_SYM 
          { Lex->sphead->reset_lex(YYTHD); }
          expr
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;

	    if (sp->m_type != TYPE_ENUM_FUNCTION)
	    {
	      my_message(ER_SP_BADRETURN, ER(ER_SP_BADRETURN), MYF(0));
	      YYABORT;
	    }
	    else
	    {
	      sp_instr_freturn *i;

	      i= new sp_instr_freturn(sp->instructions(), lex->spcont, $3,
                                      sp->m_return_field_def.sql_type, lex);
	      sp->add_instr(i);
	      sp->m_flags|= sp_head::HAS_RETURN;
	    }
	    sp->restore_lex(YYTHD);
	  }
        | IF
          { Lex->sphead->new_cont_backpatch(NULL); }
          sp_if END IF
          { Lex->sphead->do_cont_backpatch(); }
	| CASE_SYM WHEN_SYM
	  {
	    Lex->sphead->m_flags&= ~sp_head::IN_SIMPLE_CASE;
            Lex->sphead->new_cont_backpatch(NULL);
	  }
          sp_case END CASE_SYM { Lex->sphead->do_cont_backpatch(); }
        | CASE_SYM
          {
            Lex->sphead->reset_lex(YYTHD);
            Lex->sphead->new_cont_backpatch(NULL);
          }
          expr WHEN_SYM
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *parsing_ctx= lex->spcont;
	    int case_expr_id= parsing_ctx->register_case_expr();
            sp_instr_set_case_expr *i;
	    
	    if (parsing_ctx->push_case_expr_id(case_expr_id))
              YYABORT;

            i= new sp_instr_set_case_expr(sp->instructions(),
                                          parsing_ctx,
                                          case_expr_id,
                                          $3,
                                          lex);
            sp->add_cont_backpatch(i);
            sp->add_instr(i);
	    sp->m_flags|= sp_head::IN_SIMPLE_CASE;
	    sp->restore_lex(YYTHD);
	  }
	  sp_case END CASE_SYM
	  {
	    Lex->spcont->pop_case_expr_id();
            Lex->sphead->do_cont_backpatch();
	  }
	| sp_labeled_control
	  {}
	| { /* Unlabeled controls get a secret label. */
	    LEX *lex= Lex;

	    lex->spcont->push_label((char *)"", lex->sphead->instructions());
	  }
	  sp_unlabeled_control
	  {
	    LEX *lex= Lex;

	    lex->sphead->backpatch(lex->spcont->pop_label());
	  }
	| LEAVE_SYM label_ident
	  {
	    LEX *lex= Lex;
	    sp_head *sp = lex->sphead;
	    sp_pcontext *ctx= lex->spcont;
	    sp_label_t *lab= ctx->find_label($2.str);

	    if (! lab)
	    {
	      my_error(ER_SP_LILABEL_MISMATCH, MYF(0), "LEAVE", $2.str);
	      YYABORT;
	    }
	    else
	    {
	      sp_instr_jump *i;
	      uint ip= sp->instructions();
	      uint n;

	      n= ctx->diff_handlers(lab->ctx, TRUE);  /* Exclusive the dest. */
	      if (n)
	        sp->add_instr(new sp_instr_hpop(ip++, ctx, n));
	      n= ctx->diff_cursors(lab->ctx, TRUE);  /* Exclusive the dest. */
	      if (n)
	        sp->add_instr(new sp_instr_cpop(ip++, ctx, n));
	      i= new sp_instr_jump(ip, ctx);
	      sp->push_backpatch(i, lab);  /* Jumping forward */
              sp->add_instr(i);
	    }
	  }
	| ITERATE_SYM label_ident
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *ctx= lex->spcont;
	    sp_label_t *lab= ctx->find_label($2.str);

	    if (! lab || lab->type != SP_LAB_ITER)
	    {
	      my_error(ER_SP_LILABEL_MISMATCH, MYF(0), "ITERATE", $2.str);
	      YYABORT;
	    }
	    else
	    {
	      sp_instr_jump *i;
	      uint ip= sp->instructions();
	      uint n;

	      n= ctx->diff_handlers(lab->ctx, FALSE);  /* Inclusive the dest. */
	      if (n)
	        sp->add_instr(new sp_instr_hpop(ip++, ctx, n));
	      n= ctx->diff_cursors(lab->ctx, FALSE);  /* Inclusive the dest. */
	      if (n)
	        sp->add_instr(new sp_instr_cpop(ip++, ctx, n));
	      i= new sp_instr_jump(ip, ctx, lab->ip); /* Jump back */
              sp->add_instr(i);
	    }
	  }
	| OPEN_SYM ident
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    uint offset;
	    sp_instr_copen *i;

	    if (! lex->spcont->find_cursor(&$2, &offset))
	    {
	      my_error(ER_SP_CURSOR_MISMATCH, MYF(0), $2.str);
	      YYABORT;
	    }
	    i= new sp_instr_copen(sp->instructions(), lex->spcont, offset);
	    sp->add_instr(i);
	  }
	| FETCH_SYM sp_opt_fetch_noise ident INTO
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    uint offset;
	    sp_instr_cfetch *i;

	    if (! lex->spcont->find_cursor(&$3, &offset))
	    {
	      my_error(ER_SP_CURSOR_MISMATCH, MYF(0), $3.str);
	      YYABORT;
	    }
	    i= new sp_instr_cfetch(sp->instructions(), lex->spcont, offset);
	    sp->add_instr(i);
	  }
	  sp_fetch_list
	  { }
	| CLOSE_SYM ident
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    uint offset;
	    sp_instr_cclose *i;

	    if (! lex->spcont->find_cursor(&$2, &offset))
	    {
	      my_error(ER_SP_CURSOR_MISMATCH, MYF(0), $2.str);
	      YYABORT;
	    }
	    i= new sp_instr_cclose(sp->instructions(), lex->spcont,  offset);
	    sp->add_instr(i);
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
	    sp_pcontext *spc= lex->spcont;
	    sp_variable_t *spv;

	    if (!spc || !(spv = spc->find_variable(&$1)))
	    {
	      my_error(ER_SP_UNDECLARED_VAR, MYF(0), $1.str);
	      YYABORT;
	    }
	    else
	    {
	      /* An SP local variable */
	      sp_instr_cfetch *i= (sp_instr_cfetch *)sp->last_instruction();

	      i->add_to_varlist(spv);
	    }
	  }
	|
	  sp_fetch_list ',' ident
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *spc= lex->spcont;
	    sp_variable_t *spv;

	    if (!spc || !(spv = spc->find_variable(&$3)))
	    {
	      my_error(ER_SP_UNDECLARED_VAR, MYF(0), $3.str);
	      YYABORT;
	    }
	    else
	    {
	      /* An SP local variable */
	      sp_instr_cfetch *i= (sp_instr_cfetch *)sp->last_instruction();

	      i->add_to_varlist(spv);
	    }
	  }
	;

sp_if:
          { Lex->sphead->reset_lex(YYTHD); }
          expr THEN_SYM
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *ctx= lex->spcont;
	    uint ip= sp->instructions();
	    sp_instr_jump_if_not *i = new sp_instr_jump_if_not(ip, ctx,
                                                               $2, lex);

	    sp->push_backpatch(i, ctx->push_label((char *)"", 0));
            sp->add_cont_backpatch(i);
            sp->add_instr(i);
            sp->restore_lex(YYTHD);
	  }
	  sp_proc_stmts1
	  {
	    sp_head *sp= Lex->sphead;
	    sp_pcontext *ctx= Lex->spcont;
	    uint ip= sp->instructions();
	    sp_instr_jump *i = new sp_instr_jump(ip, ctx);

	    sp->add_instr(i);
	    sp->backpatch(ctx->pop_label());
	    sp->push_backpatch(i, ctx->push_label((char *)"", 0));
	  }
	  sp_elseifs
	  {
	    LEX *lex= Lex;

	    lex->sphead->backpatch(lex->spcont->pop_label());
	  }
	;

sp_elseifs:
	  /* Empty */
	| ELSEIF_SYM sp_if
	| ELSE sp_proc_stmts1
	;

sp_case:
	  { Lex->sphead->reset_lex(YYTHD); }
          expr THEN_SYM
	  {
            LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *ctx= Lex->spcont;
	    uint ip= sp->instructions();
	    sp_instr_jump_if_not *i;

	    if (! (sp->m_flags & sp_head::IN_SIMPLE_CASE))
	      i= new sp_instr_jump_if_not(ip, ctx, $2, lex);
	    else
	    { /* Simple case: <caseval> = <whenval> */

	      Item_case_expr *var;
              Item *expr;

              var= new Item_case_expr(ctx->get_current_case_expr_id());

#ifndef DBUG_OFF
              if (var)
                var->m_sp= sp;
#endif

	      expr= new Item_func_eq(var, $2);

	      i= new sp_instr_jump_if_not(ip, ctx, expr, lex);
	    }
	    sp->push_backpatch(i, ctx->push_label((char *)"", 0));
            sp->add_cont_backpatch(i);
            sp->add_instr(i);
            sp->restore_lex(YYTHD);
	  }
	  sp_proc_stmts1
	  {
	    sp_head *sp= Lex->sphead;
	    sp_pcontext *ctx= Lex->spcont;
	    uint ip= sp->instructions();
	    sp_instr_jump *i = new sp_instr_jump(ip, ctx);

	    sp->add_instr(i);
	    sp->backpatch(ctx->pop_label());
	    sp->push_backpatch(i, ctx->push_label((char *)"", 0));
	  }
	  sp_whens
	  {
	    LEX *lex= Lex;

	    lex->sphead->backpatch(lex->spcont->pop_label());
	  }
	;

sp_whens:
	  /* Empty */
	  {
	    sp_head *sp= Lex->sphead;
	    uint ip= sp->instructions();
	    sp_instr_error *i= new sp_instr_error(ip, Lex->spcont,
						  ER_SP_CASE_NOT_FOUND);

	    sp->add_instr(i);
	  }
	| ELSE sp_proc_stmts1 {}
	| WHEN_SYM sp_case {}
	;

sp_labeled_control:
	  label_ident ':'
	  {
	    LEX *lex= Lex;
	    sp_pcontext *ctx= lex->spcont;
	    sp_label_t *lab= ctx->find_label($1.str);

	    if (lab)
	    {
	      my_error(ER_SP_LABEL_REDEFINE, MYF(0), $1.str);
	      YYABORT;
	    }
	    else
	    {
	      lab= lex->spcont->push_label($1.str,
	                                   lex->sphead->instructions());
	      lab->type= SP_LAB_ITER;
	    }
	  }
	  sp_unlabeled_control sp_opt_label
	  {
	    LEX *lex= Lex;

	    if ($5.str)
	    {
	      sp_label_t *lab= lex->spcont->find_label($5.str);

	      if (!lab ||
	          my_strcasecmp(system_charset_info, $5.str, lab->name) != 0)
	      {
	        my_error(ER_SP_LABEL_MISMATCH, MYF(0), $5.str);
	        YYABORT;
	      }
	    }
	    lex->sphead->backpatch(lex->spcont->pop_label());
	  }
	;

sp_opt_label:
        /* Empty  */    { $$= null_lex_str; }
        | label_ident   { $$= $1; }
	;

sp_unlabeled_control:
	  BEGIN_SYM
	  { /* QQ This is just a dummy for grouping declarations and statements
	       together. No [[NOT] ATOMIC] yet, and we need to figure out how
	       make it coexist with the existing BEGIN COMMIT/ROLLBACK. */
	    LEX *lex= Lex;
	    sp_label_t *lab= lex->spcont->last_label();

	    lab->type= SP_LAB_BEGIN;
	    lex->spcont= lex->spcont->push_context();
	  }
	  sp_decls
	  sp_proc_stmts
	  END
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    sp_pcontext *ctx= lex->spcont;

  	    sp->backpatch(ctx->last_label());	/* We always have a label */
	    if ($3.hndlrs)
	      sp->add_instr(new sp_instr_hpop(sp->instructions(), ctx,
					      $3.hndlrs));
	    if ($3.curs)
	      sp->add_instr(new sp_instr_cpop(sp->instructions(), ctx,
					      $3.curs));
	    lex->spcont= ctx->pop_context();
	  }
	| LOOP_SYM
	  sp_proc_stmts1 END LOOP_SYM
	  {
	    LEX *lex= Lex;
	    uint ip= lex->sphead->instructions();
	    sp_label_t *lab= lex->spcont->last_label();  /* Jumping back */
	    sp_instr_jump *i = new sp_instr_jump(ip, lex->spcont, lab->ip);

	    lex->sphead->add_instr(i);
	  }
        | WHILE_SYM 
          { Lex->sphead->reset_lex(YYTHD); }
          expr DO_SYM
	  {
	    LEX *lex= Lex;
	    sp_head *sp= lex->sphead;
	    uint ip= sp->instructions();
	    sp_instr_jump_if_not *i = new sp_instr_jump_if_not(ip, lex->spcont,
							       $3, lex);

	    /* Jumping forward */
	    sp->push_backpatch(i, lex->spcont->last_label());
            sp->new_cont_backpatch(i);
            sp->add_instr(i);
            sp->restore_lex(YYTHD);
	  }
	  sp_proc_stmts1 END WHILE_SYM
	  {
	    LEX *lex= Lex;
	    uint ip= lex->sphead->instructions();
	    sp_label_t *lab= lex->spcont->last_label();  /* Jumping back */
	    sp_instr_jump *i = new sp_instr_jump(ip, lex->spcont, lab->ip);

	    lex->sphead->add_instr(i);
            lex->sphead->do_cont_backpatch();
	  }
        | REPEAT_SYM sp_proc_stmts1 UNTIL_SYM 
          { Lex->sphead->reset_lex(YYTHD); }
          expr END REPEAT_SYM
	  {
	    LEX *lex= Lex;
	    uint ip= lex->sphead->instructions();
	    sp_label_t *lab= lex->spcont->last_label();  /* Jumping back */
	    sp_instr_jump_if_not *i = new sp_instr_jump_if_not(ip, lex->spcont,
                                                               $5, lab->ip,
                                                               lex);
            lex->sphead->add_instr(i);
            lex->sphead->restore_lex(YYTHD);
            /* We can shortcut the cont_backpatch here */
            i->m_cont_dest= ip+1;
	  }
	;

trg_action_time:
            BEFORE_SYM 
            { Lex->trg_chistics.action_time= TRG_ACTION_BEFORE; }
          | AFTER_SYM 
            { Lex->trg_chistics.action_time= TRG_ACTION_AFTER; }
          ;

trg_event:
            INSERT 
            { Lex->trg_chistics.event= TRG_EVENT_INSERT; }
          | UPDATE_SYM
            { Lex->trg_chistics.event= TRG_EVENT_UPDATE; }
          | DELETE_SYM
            { Lex->trg_chistics.event= TRG_EVENT_DELETE; }
          ;

create2:
        '(' create2a {}
        | opt_create_table_options create3 {}
        | LIKE table_ident
          {
            LEX *lex=Lex;
            THD *thd= lex->thd;
            if (!(lex->name= (char *)$2))
              YYABORT;
            if ($2->db.str == NULL &&
                thd->copy_db_to(&($2->db.str), &($2->db.length)))
            {
              YYABORT;
            }
          }
        | '(' LIKE table_ident ')'
          {
            LEX *lex=Lex;
            THD *thd= lex->thd;
            if (!(lex->name= (char *)$3))
              YYABORT;
            if ($3->db.str == NULL &&
                thd->copy_db_to(&($3->db.str), &($3->db.length)))
            {
              YYABORT;
            }
          }
        ;

create2a:
        field_list ')' opt_create_table_options create3 {}
	|  create_select ')' { Select->set_braces(1);} union_opt {}
        ;

create3:
	/* empty */ {}
	| opt_duplicate opt_as     create_select
          { Select->set_braces(0);} union_clause {}
	| opt_duplicate opt_as '(' create_select ')'
          { Select->set_braces(1);} union_opt {}
        ;

create_select:
          SELECT_SYM
          {
	    LEX *lex=Lex;
	    lex->lock_option= using_update_log ? TL_READ_NO_INSERT : TL_READ;
	    if (lex->sql_command == SQLCOM_INSERT)
	      lex->sql_command= SQLCOM_INSERT_SELECT;
	    else if (lex->sql_command == SQLCOM_REPLACE)
	      lex->sql_command= SQLCOM_REPLACE_SELECT;
	    /*
              The following work only with the local list, the global list
              is created correctly in this case
	    */
	    lex->current_select->table_list.save_and_clear(&lex->save_list);
	    mysql_init_select(lex);
	    lex->current_select->parsing_place= SELECT_LIST;
          }
          select_options select_item_list
	  {
	    Select->parsing_place= NO_MATTER;
	  }
	  opt_select_from
	  {
	    /*
              The following work only with the local list, the global list
              is created correctly in this case
	    */
	    Lex->current_select->table_list.push_front(&Lex->save_list);
	  }
        ;

opt_as:
	/* empty */ {}
	| AS	    {};

opt_create_database_options:
	/* empty */			{}
	| create_database_options	{};

create_database_options:
	create_database_option					{}
	| create_database_options create_database_option	{};

create_database_option:
	default_collation   {}
	| default_charset   {};

opt_table_options:
	/* empty */	 { $$= 0; }
	| table_options  { $$= $1;};

table_options:
	table_option	{ $$=$1; }
	| table_option table_options { $$= $1 | $2; };

table_option:
	TEMPORARY	{ $$=HA_LEX_CREATE_TMP_TABLE; };

opt_if_not_exists:
	/* empty */	 { $$= 0; }
	| IF not EXISTS	 { $$=HA_LEX_CREATE_IF_NOT_EXISTS; };

opt_create_table_options:
	/* empty */
	| create_table_options;

create_table_options_space_separated:
	create_table_option
	| create_table_option create_table_options_space_separated;

create_table_options:
	create_table_option
	| create_table_option     create_table_options
	| create_table_option ',' create_table_options;

create_table_option:
	ENGINE_SYM opt_equal storage_engines    { Lex->create_info.db_type= $3; Lex->create_info.used_fields|= HA_CREATE_USED_ENGINE; }
	| TYPE_SYM opt_equal storage_engines    { Lex->create_info.db_type= $3; WARN_DEPRECATED("TYPE=storage_engine","ENGINE=storage_engine");   Lex->create_info.used_fields|= HA_CREATE_USED_ENGINE; }
	| MAX_ROWS opt_equal ulonglong_num	{ Lex->create_info.max_rows= $3; Lex->create_info.used_fields|= HA_CREATE_USED_MAX_ROWS;}
	| MIN_ROWS opt_equal ulonglong_num	{ Lex->create_info.min_rows= $3; Lex->create_info.used_fields|= HA_CREATE_USED_MIN_ROWS;}
	| AVG_ROW_LENGTH opt_equal ulong_num	{ Lex->create_info.avg_row_length=$3; Lex->create_info.used_fields|= HA_CREATE_USED_AVG_ROW_LENGTH;}
	| PASSWORD opt_equal TEXT_STRING_sys	{ Lex->create_info.password=$3.str; Lex->create_info.used_fields|= HA_CREATE_USED_PASSWORD; }
	| COMMENT_SYM opt_equal TEXT_STRING_sys	{ Lex->create_info.comment=$3; Lex->create_info.used_fields|= HA_CREATE_USED_COMMENT; }
	| AUTO_INC opt_equal ulonglong_num	{ Lex->create_info.auto_increment_value=$3; Lex->create_info.used_fields|= HA_CREATE_USED_AUTO;}
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
                yyerror(ER(ER_SYNTAX_ERROR));
                YYABORT;
            }
            Lex->create_info.used_fields|= HA_CREATE_USED_PACK_KEYS;
          }
        | PACK_KEYS_SYM opt_equal DEFAULT
          {
            Lex->create_info.table_options&=
              ~(HA_OPTION_PACK_KEYS | HA_OPTION_NO_PACK_KEYS);
            Lex->create_info.used_fields|= HA_CREATE_USED_PACK_KEYS;
          }
	| CHECKSUM_SYM opt_equal ulong_num	{ Lex->create_info.table_options|= $3 ? HA_OPTION_CHECKSUM : HA_OPTION_NO_CHECKSUM; Lex->create_info.used_fields|= HA_CREATE_USED_CHECKSUM; }
	| DELAY_KEY_WRITE_SYM opt_equal ulong_num { Lex->create_info.table_options|= $3 ? HA_OPTION_DELAY_KEY_WRITE : HA_OPTION_NO_DELAY_KEY_WRITE;  Lex->create_info.used_fields|= HA_CREATE_USED_DELAY_KEY_WRITE; }
	| ROW_FORMAT_SYM opt_equal row_types	{ Lex->create_info.row_type= $3;  Lex->create_info.used_fields|= HA_CREATE_USED_ROW_FORMAT; }
	| RAID_TYPE opt_equal raid_types
	  {
	    my_error(ER_WARN_DEPRECATED_SYNTAX, MYF(0), "RAID_TYPE", "PARTITION");
	    YYABORT;
	  }
	| RAID_CHUNKS opt_equal ulong_num
	  {
	    my_error(ER_WARN_DEPRECATED_SYNTAX, MYF(0), "RAID_CHUNKS", "PARTITION");
	    YYABORT;
	  }
	| RAID_CHUNKSIZE opt_equal ulong_num
	  {
	    my_error(ER_WARN_DEPRECATED_SYNTAX, MYF(0), "RAID_CHUNKSIZE", "PARTITION");
	    YYABORT;
	  }
	| UNION_SYM opt_equal '(' table_list ')'
	  {
	    /* Move the union list to the merge_list */
	    LEX *lex=Lex;
	    TABLE_LIST *table_list= lex->select_lex.get_table_list();
	    lex->create_info.merge_list= lex->select_lex.table_list;
	    lex->create_info.merge_list.elements--;
	    lex->create_info.merge_list.first=
	      (byte*) (table_list->next_local);
	    lex->select_lex.table_list.elements=1;
	    lex->select_lex.table_list.next=
	      (byte**) &(table_list->next_local);
	    table_list->next_local= 0;
	    lex->create_info.used_fields|= HA_CREATE_USED_UNION;
	  }
	| default_charset
	| default_collation
	| INSERT_METHOD opt_equal merge_insert_types   { Lex->create_info.merge_insert_method= $3; Lex->create_info.used_fields|= HA_CREATE_USED_INSERT_METHOD;}
	| DATA_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys { Lex->create_info.data_file_name= $4.str; Lex->create_info.used_fields|= HA_CREATE_USED_DATADIR; }
	| INDEX_SYM DIRECTORY_SYM opt_equal TEXT_STRING_sys { Lex->create_info.index_file_name= $4.str;  Lex->create_info.used_fields|= HA_CREATE_USED_INDEXDIR; }
	| CONNECTION_SYM opt_equal TEXT_STRING_sys { Lex->create_info.connect_string.str= $3.str; Lex->create_info.connect_string.length= $3.length;  Lex->create_info.used_fields|= HA_CREATE_USED_CONNECTION; }
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
            YYABORT;
          }
	  Lex->create_info.default_table_charset= $4;
          Lex->create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
        };

default_collation:
        opt_default COLLATE_SYM opt_equal collation_name_or_default
        {
          HA_CREATE_INFO *cinfo= &Lex->create_info;
          if ((cinfo->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
               cinfo->default_table_charset && $4 &&
               !my_charset_same(cinfo->default_table_charset,$4))
            {
              my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
                       $4->name, cinfo->default_table_charset->csname);
              YYABORT;
            }
            Lex->create_info.default_table_charset= $4;
            Lex->create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
        };

storage_engines:
	ident_or_text
	{
	  $$ = ha_resolve_by_name($1.str,$1.length);
	  if ($$ == DB_TYPE_UNKNOWN) {
	    my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), $1.str);
	    YYABORT;
	  }
	};

row_types:
	DEFAULT		{ $$= ROW_TYPE_DEFAULT; }
	| FIXED_SYM	{ $$= ROW_TYPE_FIXED; }
	| DYNAMIC_SYM	{ $$= ROW_TYPE_DYNAMIC; }
	| COMPRESSED_SYM { $$= ROW_TYPE_COMPRESSED; }
	| REDUNDANT_SYM	{ $$= ROW_TYPE_REDUNDANT; }
	| COMPACT_SYM	{ $$= ROW_TYPE_COMPACT; };

raid_types:
	RAID_STRIPED_SYM { $$= RAID_TYPE_0; }
	| RAID_0_SYM	 { $$= RAID_TYPE_0; }
	| ulong_num	 { $$=$1;};

merge_insert_types:
       NO_SYM            { $$= MERGE_INSERT_DISABLED; }
       | FIRST_SYM       { $$= MERGE_INSERT_TO_FIRST; }
       | LAST_SYM        { $$= MERGE_INSERT_TO_LAST; };

opt_select_from:
	opt_limit_clause {}
	| select_from select_lock_type;

udf_func_type:
	/* empty */	{ $$ = UDFTYPE_FUNCTION; }
	| AGGREGATE_SYM { $$ = UDFTYPE_AGGREGATE; };

udf_type:
	STRING_SYM {$$ = (int) STRING_RESULT; }
	| REAL {$$ = (int) REAL_RESULT; }
        | DECIMAL_SYM {$$ = (int) DECIMAL_RESULT; }
	| INT_SYM {$$ = (int) INT_RESULT; };

field_list:
	  field_list_item
	| field_list ',' field_list_item;


field_list_item:
	   column_def
         | key_def
         ;

column_def:
	  field_spec opt_check_constraint
	| field_spec references
	  {
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	;

key_def:
	key_type opt_ident key_alg '(' key_list ')'
	  {
	    LEX *lex=Lex;
	    lex->key_list.push_back(new Key($1,$2, $3, 0, lex->col_list));
	    lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	| opt_constraint constraint_key_type opt_ident key_alg '(' key_list ')'
	  {
	    LEX *lex=Lex;
	    const char *key_name= $3 ? $3:$1;
	    lex->key_list.push_back(new Key($2, key_name, $4, 0,
				    lex->col_list));
	    lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	| opt_constraint FOREIGN KEY_SYM opt_ident '(' key_list ')' references
	  {
	    LEX *lex=Lex;
	    lex->key_list.push_back(new foreign_key($4 ? $4:$1, lex->col_list,
				    $8,
				    lex->ref_list,
				    lex->fk_delete_opt,
				    lex->fk_update_opt,
				    lex->fk_match_option));
	    lex->key_list.push_back(new Key(Key::MULTIPLE, $4 ? $4 : $1,
					    HA_KEY_ALG_UNDEF, 1,
					    lex->col_list));
	    lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	| constraint opt_check_constraint
	  {
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	| opt_constraint check_constraint
	  {
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	;

opt_check_constraint:
	/* empty */
	| check_constraint
	;

check_constraint:
	CHECK_SYM expr
	;

opt_constraint:
	/* empty */		{ $$=(char*) 0; }
	| constraint		{ $$= $1; }
	;

constraint:
	CONSTRAINT opt_ident	{ $$=$2; }
	;

field_spec:
	field_ident
	 {
	   LEX *lex=Lex;
	   lex->length=lex->dec=0; lex->type=0;
	   lex->default_value= lex->on_update_value= 0;
           lex->comment=null_lex_str;
	   lex->charset=NULL;
	 }
	type opt_attribute
	{
	  LEX *lex=Lex;
	  if (add_field_to_list(lex->thd, $1.str,
				(enum enum_field_types) $3,
				lex->length,lex->dec,lex->type,
				lex->default_value, lex->on_update_value, 
                                &lex->comment,
				lex->change,&lex->interval_list,lex->charset,
				lex->uint_geom_type))
	    YYABORT;
	};

type:
	int_type opt_len field_options	{ $$=$1; }
	| real_type opt_precision field_options { $$=$1; }
	| FLOAT_SYM float_options field_options { $$=FIELD_TYPE_FLOAT; }
	| BIT_SYM			{ Lex->length= (char*) "1";
					  $$=FIELD_TYPE_BIT; }
	| BIT_SYM '(' NUM ')'		{ Lex->length= $3.str;
					  $$=FIELD_TYPE_BIT; }
	| BOOL_SYM			{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_TINY; }
	| BOOLEAN_SYM			{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_TINY; }
	| char '(' NUM ')' opt_binary	{ Lex->length=$3.str;
					  $$=FIELD_TYPE_STRING; }
	| char opt_binary		{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_STRING; }
	| nchar '(' NUM ')' opt_bin_mod	{ Lex->length=$3.str;
					  $$=FIELD_TYPE_STRING;
					  Lex->charset=national_charset_info; }
	| nchar opt_bin_mod		{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_STRING;
					  Lex->charset=national_charset_info; }
	| BINARY '(' NUM ')'		{ Lex->length=$3.str;
					  Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_STRING; }
	| BINARY			{ Lex->length= (char*) "1";
					  Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_STRING; }
	| varchar '(' NUM ')' opt_binary { Lex->length=$3.str;
					  $$= MYSQL_TYPE_VARCHAR; }
	| nvarchar '(' NUM ')' opt_bin_mod { Lex->length=$3.str;
					  $$= MYSQL_TYPE_VARCHAR;
					  Lex->charset=national_charset_info; }
	| VARBINARY '(' NUM ')' 	{ Lex->length=$3.str;
					  Lex->charset=&my_charset_bin;
					  $$= MYSQL_TYPE_VARCHAR; }
	| YEAR_SYM opt_len field_options { $$=FIELD_TYPE_YEAR; }
	| DATE_SYM			{ $$=FIELD_TYPE_DATE; }
	| TIME_SYM			{ $$=FIELD_TYPE_TIME; }
	| TIMESTAMP opt_len
	  {
	    if (YYTHD->variables.sql_mode & MODE_MAXDB)
	      $$=FIELD_TYPE_DATETIME;
	    else
            {
              /* 
                Unlike other types TIMESTAMP fields are NOT NULL by default.
              */
              Lex->type|= NOT_NULL_FLAG;
	      $$=FIELD_TYPE_TIMESTAMP;
            }
	   }
	| DATETIME			{ $$=FIELD_TYPE_DATETIME; }
	| TINYBLOB			{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_TINY_BLOB; }
	| BLOB_SYM opt_len		{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_BLOB; }
	| spatial_type
          {
#ifdef HAVE_SPATIAL
            Lex->charset=&my_charset_bin;
            Lex->uint_geom_type= (uint)$1;
            $$=FIELD_TYPE_GEOMETRY;
#else
            my_error(ER_FEATURE_DISABLED, MYF(0),
                     sym_group_geom.name, sym_group_geom.needed_define);
            YYABORT;
#endif
          }
	| MEDIUMBLOB			{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_MEDIUM_BLOB; }
	| LONGBLOB			{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_LONG_BLOB; }
	| LONG_SYM VARBINARY		{ Lex->charset=&my_charset_bin;
					  $$=FIELD_TYPE_MEDIUM_BLOB; }
	| LONG_SYM varchar opt_binary	{ $$=FIELD_TYPE_MEDIUM_BLOB; }
	| TINYTEXT opt_binary		{ $$=FIELD_TYPE_TINY_BLOB; }
	| TEXT_SYM opt_len opt_binary	{ $$=FIELD_TYPE_BLOB; }
	| MEDIUMTEXT opt_binary		{ $$=FIELD_TYPE_MEDIUM_BLOB; }
	| LONGTEXT opt_binary		{ $$=FIELD_TYPE_LONG_BLOB; }
	| DECIMAL_SYM float_options field_options
                                        { $$=FIELD_TYPE_NEWDECIMAL;}
	| NUMERIC_SYM float_options field_options
                                        { $$=FIELD_TYPE_NEWDECIMAL;}
	| FIXED_SYM float_options field_options
                                        { $$=FIELD_TYPE_NEWDECIMAL;}
	| ENUM {Lex->interval_list.empty();} '(' string_list ')' opt_binary
	  { $$=FIELD_TYPE_ENUM; }
	| SET { Lex->interval_list.empty();} '(' string_list ')' opt_binary
	  { $$=FIELD_TYPE_SET; }
	| LONG_SYM opt_binary		{ $$=FIELD_TYPE_MEDIUM_BLOB; }
	| SERIAL_SYM
	  {
	    $$=FIELD_TYPE_LONGLONG;
	    Lex->type|= (AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNSIGNED_FLAG |
		         UNIQUE_FLAG);
	  }
	;

spatial_type:
	GEOMETRY_SYM	      { $$= Field::GEOM_GEOMETRY; }
	| GEOMETRYCOLLECTION  { $$= Field::GEOM_GEOMETRYCOLLECTION; }
	| POINT_SYM           { Lex->length= (char*)"21";
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
	INT_SYM		{ $$=FIELD_TYPE_LONG; }
	| TINYINT	{ $$=FIELD_TYPE_TINY; }
	| SMALLINT	{ $$=FIELD_TYPE_SHORT; }
	| MEDIUMINT	{ $$=FIELD_TYPE_INT24; }
	| BIGINT	{ $$=FIELD_TYPE_LONGLONG; };

real_type:
	REAL		{ $$= YYTHD->variables.sql_mode & MODE_REAL_AS_FLOAT ?
			      FIELD_TYPE_FLOAT : FIELD_TYPE_DOUBLE; }
	| DOUBLE_SYM	{ $$=FIELD_TYPE_DOUBLE; }
	| DOUBLE_SYM PRECISION { $$=FIELD_TYPE_DOUBLE; };


float_options:
        /* empty */		{ Lex->dec=Lex->length= (char*)0; }
        | '(' NUM ')'		{ Lex->length=$2.str; Lex->dec= (char*)0; }
	| precision		{};

precision:
	'(' NUM ',' NUM ')'
	{
	  LEX *lex=Lex;
	  lex->length=$2.str; lex->dec=$4.str;
	};

field_options:
	/* empty */		{}
	| field_opt_list	{};

field_opt_list:
	field_opt_list field_option {}
	| field_option {};

field_option:
	SIGNED_SYM	{}
	| UNSIGNED	{ Lex->type|= UNSIGNED_FLAG;}
	| ZEROFILL	{ Lex->type|= UNSIGNED_FLAG | ZEROFILL_FLAG; };

opt_len:
	/* empty */	{ Lex->length=(char*) 0; } /* use default length */
	| '(' NUM ')'	{ Lex->length= $2.str; };

opt_precision:
	/* empty */	{}
	| precision	{};

opt_attribute:
	/* empty */ {}
	| opt_attribute_list {};

opt_attribute_list:
	opt_attribute_list attribute {}
	| attribute;

attribute:
	NULL_SYM	  { Lex->type&= ~ NOT_NULL_FLAG; }
	| not NULL_SYM	  { Lex->type|= NOT_NULL_FLAG; }
	| DEFAULT now_or_signed_literal { Lex->default_value=$2; }
	| ON UPDATE_SYM NOW_SYM optional_braces 
          { Lex->on_update_value= new Item_func_now_local(); }
	| AUTO_INC	  { Lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG; }
	| SERIAL_SYM DEFAULT VALUE_SYM
	  { 
	    LEX *lex=Lex;
	    lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNIQUE_FLAG; 
	    lex->alter_info.flags|= ALTER_ADD_INDEX; 
	  }
	| opt_primary KEY_SYM 
	  {
	    LEX *lex=Lex;
	    lex->type|= PRI_KEY_FLAG | NOT_NULL_FLAG; 
	    lex->alter_info.flags|= ALTER_ADD_INDEX; 
	  }
	| UNIQUE_SYM	  
	  {
	    LEX *lex=Lex;
	    lex->type|= UNIQUE_FLAG; 
	    lex->alter_info.flags|= ALTER_ADD_INDEX; 
	  }
	| UNIQUE_SYM KEY_SYM 
	  {
	    LEX *lex=Lex;
	    lex->type|= UNIQUE_KEY_FLAG; 
	    lex->alter_info.flags|= ALTER_ADD_INDEX; 
	  }
	| COMMENT_SYM TEXT_STRING_sys { Lex->comment= $2; }
	| COLLATE_SYM collation_name
	  {
	    if (Lex->charset && !my_charset_same(Lex->charset,$2))
	    {
	      my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
                       $2->name,Lex->charset->csname);
	      YYABORT;
	    }
	    else
	    {
	      Lex->charset=$2;
	    }
	  }
	;

now_or_signed_literal:
        NOW_SYM optional_braces { $$= new Item_func_now_local(); }
        | signed_literal { $$=$1; }
        ;

charset:
	CHAR_SYM SET	{}
	| CHARSET	{}
	;

charset_name:
	ident_or_text
	{
	  if (!($$=get_charset_by_csname($1.str,MY_CS_PRIMARY,MYF(0))))
	  {
	    my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), $1.str);
	    YYABORT;
	  }
	}
	| BINARY { $$= &my_charset_bin; }
	;

charset_name_or_default:
	charset_name { $$=$1;   }
	| DEFAULT    { $$=NULL; } ;


old_or_new_charset_name:
	ident_or_text
	{
	  if (!($$=get_charset_by_csname($1.str,MY_CS_PRIMARY,MYF(0))) &&
	      !($$=get_old_charset_by_name($1.str)))
	  {
	    my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), $1.str);
	    YYABORT;
	  }
	}
	| BINARY { $$= &my_charset_bin; }
	;

old_or_new_charset_name_or_default:
	old_or_new_charset_name { $$=$1;   }
	| DEFAULT    { $$=NULL; } ;

collation_name:
	ident_or_text
	{
	  if (!($$=get_charset_by_name($1.str,MYF(0))))
	  {
	    my_error(ER_UNKNOWN_COLLATION, MYF(0), $1.str);
	    YYABORT;
	  }
	};

opt_collate:
	/* empty */	{ $$=NULL; }
	| COLLATE_SYM collation_name_or_default { $$=$2; }
	;

collation_name_or_default:
	collation_name { $$=$1;   }
	| DEFAULT    { $$=NULL; } ;

opt_default:
	/* empty */	{}
	| DEFAULT	{};

opt_binary:
	/* empty */			{ Lex->charset=NULL; }
	| ASCII_SYM opt_bin_mod		{ Lex->charset=&my_charset_latin1; }
	| BYTE_SYM			{ Lex->charset=&my_charset_bin; }
	| UNICODE_SYM opt_bin_mod
	{
	  if (!(Lex->charset=get_charset_by_csname("ucs2",
                                                   MY_CS_PRIMARY,MYF(0))))
	  {
	    my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), "ucs2");
	    YYABORT;
	  }
	}
	| charset charset_name opt_bin_mod	{ Lex->charset=$2; }
        | BINARY opt_bin_charset { Lex->type|= BINCMP_FLAG; };

opt_bin_mod:
	/* empty */ { }
	| BINARY { Lex->type|= BINCMP_FLAG; };

opt_bin_charset:
	/* empty */ { }
	| ASCII_SYM	{ Lex->charset=&my_charset_latin1; }
	| UNICODE_SYM
	{
	  if (!(Lex->charset=get_charset_by_csname("ucs2",
                                                   MY_CS_PRIMARY,MYF(0))))
	  {
	    my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), "ucs2");
	    YYABORT;
	  }
	}
	| charset charset_name	{ Lex->charset=$2; } ;

opt_primary:
	/* empty */
	| PRIMARY_SYM
	;

references:
	REFERENCES table_ident
	{
	  LEX *lex=Lex;
	  lex->fk_delete_opt= lex->fk_update_opt= lex->fk_match_option= 0;
	  lex->ref_list.empty();
	}
	opt_ref_list
	{
	  $$=$2;
	};

opt_ref_list:
	/* empty */ opt_on_delete {}
	| '(' ref_list ')' opt_on_delete {};

ref_list:
	ref_list ',' ident	{ Lex->ref_list.push_back(new key_part_spec($3.str)); }
	| ident			{ Lex->ref_list.push_back(new key_part_spec($1.str)); };


opt_on_delete:
	/* empty */ {}
	| opt_on_delete_list {};

opt_on_delete_list:
	opt_on_delete_list opt_on_delete_item {}
	| opt_on_delete_item {};

opt_on_delete_item:
	ON DELETE_SYM delete_option   { Lex->fk_delete_opt= $3; }
	| ON UPDATE_SYM delete_option { Lex->fk_update_opt= $3; }
	| MATCH FULL	{ Lex->fk_match_option= foreign_key::FK_MATCH_FULL; }
	| MATCH PARTIAL { Lex->fk_match_option= foreign_key::FK_MATCH_PARTIAL; }
	| MATCH SIMPLE_SYM { Lex->fk_match_option= foreign_key::FK_MATCH_SIMPLE; };

delete_option:
	RESTRICT	 { $$= (int) foreign_key::FK_OPTION_RESTRICT; }
	| CASCADE	 { $$= (int) foreign_key::FK_OPTION_CASCADE; }
	| SET NULL_SYM   { $$= (int) foreign_key::FK_OPTION_SET_NULL; }
	| NO_SYM ACTION  { $$= (int) foreign_key::FK_OPTION_NO_ACTION; }
	| SET DEFAULT    { $$= (int) foreign_key::FK_OPTION_DEFAULT;  };

key_type:
	key_or_index			    { $$= Key::MULTIPLE; }
	| FULLTEXT_SYM opt_key_or_index	    { $$= Key::FULLTEXT; }
	| SPATIAL_SYM opt_key_or_index
	  {
#ifdef HAVE_SPATIAL
	    $$= Key::SPATIAL;
#else
	    my_error(ER_FEATURE_DISABLED, MYF(0),
                     sym_group_geom.name, sym_group_geom.needed_define);
	    YYABORT;
#endif
	  };

constraint_key_type:
	PRIMARY_SYM KEY_SYM  { $$= Key::PRIMARY; }
	| UNIQUE_SYM opt_key_or_index { $$= Key::UNIQUE; };

key_or_index:
	KEY_SYM {}
	| INDEX_SYM {};

opt_key_or_index:
	/* empty */ {}
	| key_or_index
	;

keys_or_index:
	KEYS {}
	| INDEX_SYM {}
	| INDEXES {};

opt_unique_or_fulltext:
	/* empty */	{ $$= Key::MULTIPLE; }
	| UNIQUE_SYM	{ $$= Key::UNIQUE; }
	| FULLTEXT_SYM	{ $$= Key::FULLTEXT;}
	| SPATIAL_SYM
	  {
#ifdef HAVE_SPATIAL
	    $$= Key::SPATIAL;
#else
            my_error(ER_FEATURE_DISABLED, MYF(0),
                     sym_group_geom.name, sym_group_geom.needed_define);
	    YYABORT;
#endif
	  }
        ;

key_alg:
	/* empty */		   { $$= HA_KEY_ALG_UNDEF; }
	| USING opt_btree_or_rtree { $$= $2; }
	| TYPE_SYM opt_btree_or_rtree  { $$= $2; };

opt_btree_or_rtree:
	BTREE_SYM	{ $$= HA_KEY_ALG_BTREE; }
	| RTREE_SYM
	  {
	    $$= HA_KEY_ALG_RTREE;
	  }
	| HASH_SYM	{ $$= HA_KEY_ALG_HASH; };

key_list:
	key_list ',' key_part order_dir { Lex->col_list.push_back($3); }
	| key_part order_dir		{ Lex->col_list.push_back($1); };

key_part:
	ident			{ $$=new key_part_spec($1.str); }
	| ident '(' NUM ')'	
        {
          int key_part_len= atoi($3.str);
          if (!key_part_len)
          {
            my_error(ER_KEY_PART_0, MYF(0), $1.str);
          }
          $$=new key_part_spec($1.str,(uint) key_part_len);
        };

opt_ident:
	/* empty */	{ $$=(char*) 0; }	/* Defaultlength */
	| field_ident	{ $$=$1.str; };

opt_component:
        /* empty */      { $$= null_lex_str; }
        | '.' ident      { $$= $2; };

string_list:
	text_string			{ Lex->interval_list.push_back($1); }
	| string_list ',' text_string	{ Lex->interval_list.push_back($3); };

/*
** Alter table
*/

alter:
	ALTER opt_ignore TABLE_SYM table_ident
	{
	  THD *thd= YYTHD;
	  LEX *lex= thd->lex;
	  lex->sql_command= SQLCOM_ALTER_TABLE;
	  lex->name= 0;
	  lex->duplicates= DUP_ERROR; 
	  if (!lex->select_lex.add_table_to_list(thd, $4, NULL,
						 TL_OPTION_UPDATING))
	    YYABORT;
	  lex->create_list.empty();
	  lex->key_list.empty();
	  lex->col_list.empty();
          lex->select_lex.init_order();
	  lex->select_lex.db=
            ((TABLE_LIST*) lex->select_lex.table_list.first)->db;
          lex->name=0;
	  bzero((char*) &lex->create_info,sizeof(lex->create_info));
	  lex->create_info.db_type= DB_TYPE_DEFAULT;
	  lex->create_info.default_table_charset= NULL;
	  lex->create_info.row_type= ROW_TYPE_NOT_USED;
	  lex->alter_info.reset();
	  lex->alter_info.flags= 0;
	}
	alter_list
	{}
	| ALTER DATABASE ident_or_empty
          {
            Lex->create_info.default_table_charset= NULL;
            Lex->create_info.used_fields= 0;
          }
          opt_create_database_options
	  {
	    LEX *lex=Lex;
            THD *thd= Lex->thd;
	    lex->sql_command=SQLCOM_ALTER_DB;
	    lex->name= $3;
            if (lex->name == NULL && thd->copy_db_to(&lex->name, NULL))
              YYABORT;
	  }
	| ALTER PROCEDURE sp_name
	  {
	    LEX *lex= Lex;

	    if (lex->sphead)
	    {
	      my_error(ER_SP_NO_DROP_SP, MYF(0), "PROCEDURE");
	      YYABORT;
	    }
	    bzero((char *)&lex->sp_chistics, sizeof(st_sp_chistics));
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
	      YYABORT;
	    }
	    bzero((char *)&lex->sp_chistics, sizeof(st_sp_chistics));
          }
	  sp_a_chistics
	  {
	    LEX *lex=Lex;

	    lex->sql_command= SQLCOM_ALTER_FUNCTION;
	    lex->spname= $3;
	  }
        | ALTER view_algorithm_opt definer view_suid
          VIEW_SYM table_ident
	  {
	    THD *thd= YYTHD;
	    LEX *lex= thd->lex;
	    lex->sql_command= SQLCOM_CREATE_VIEW;
	    lex->create_view_mode= VIEW_ALTER;
	    /* first table in list is target VIEW name */
	    lex->select_lex.add_table_to_list(thd, $6, NULL, TL_OPTION_UPDATING);
	  }
	  view_list_opt AS view_select view_check_option
	  {}
	;

ident_or_empty:
	/* empty */  { $$= 0; }
	| ident      { $$= $1.str; };

alter_list:
	| DISCARD TABLESPACE { Lex->alter_info.tablespace_op= DISCARD_TABLESPACE; }
	| IMPORT TABLESPACE { Lex->alter_info.tablespace_op= IMPORT_TABLESPACE; }
        | alter_list_item
	| alter_list ',' alter_list_item;

add_column:
	ADD opt_column
	{
	  LEX *lex=Lex;
	  lex->change=0;
	  lex->alter_info.flags|= ALTER_ADD_COLUMN;
	};

alter_list_item:
	add_column column_def opt_place { }
	| ADD key_def
	  {
	    Lex->alter_info.flags|= ALTER_ADD_INDEX;
	  }
	| add_column '(' field_list ')'
          {
	    Lex->alter_info.flags|= ALTER_ADD_COLUMN | ALTER_ADD_INDEX;
          }
	| CHANGE opt_column field_ident
	  {
	     LEX *lex=Lex;
	     lex->change= $3.str;
	     lex->alter_info.flags|= ALTER_CHANGE_COLUMN;
	  }
          field_spec opt_place
        | MODIFY_SYM opt_column field_ident
          {
            LEX *lex=Lex;
            lex->length=lex->dec=0; lex->type=0;
            lex->default_value= lex->on_update_value= 0;
            lex->comment=null_lex_str;
	    lex->charset= NULL;
	    lex->alter_info.flags|= ALTER_CHANGE_COLUMN;
          }
          type opt_attribute
          {
            LEX *lex=Lex;
            if (add_field_to_list(lex->thd,$3.str,
                                  (enum enum_field_types) $5,
                                  lex->length,lex->dec,lex->type,
                                  lex->default_value, lex->on_update_value,
                                  &lex->comment,
				  $3.str, &lex->interval_list, lex->charset,
				  lex->uint_geom_type))
	       YYABORT;
          }
          opt_place
	| DROP opt_column field_ident opt_restrict
	  {
	    LEX *lex=Lex;
	    lex->alter_info.drop_list.push_back(new Alter_drop(Alter_drop::COLUMN,
                                                               $3.str));
	    lex->alter_info.flags|= ALTER_DROP_COLUMN;
	  }
	| DROP FOREIGN KEY_SYM opt_ident
          {
	    Lex->alter_info.flags|= ALTER_DROP_INDEX;
          }
	| DROP PRIMARY_SYM KEY_SYM
	  {
	    LEX *lex=Lex;
	    lex->alter_info.drop_list.push_back(new Alter_drop(Alter_drop::KEY,
				               primary_key_name));
	    lex->alter_info.flags|= ALTER_DROP_INDEX;
	  }
	| DROP key_or_index field_ident
	  {
	    LEX *lex=Lex;
	    lex->alter_info.drop_list.push_back(new Alter_drop(Alter_drop::KEY,
					                       $3.str));
	    lex->alter_info.flags|= ALTER_DROP_INDEX;
	  }
	| DISABLE_SYM KEYS
          {
	    LEX *lex=Lex;
            lex->alter_info.keys_onoff= DISABLE;
	    lex->alter_info.flags|= ALTER_KEYS_ONOFF;
          }
	| ENABLE_SYM KEYS
          {
	    LEX *lex=Lex;
            lex->alter_info.keys_onoff= ENABLE;
	    lex->alter_info.flags|= ALTER_KEYS_ONOFF;
          }
	| ALTER opt_column field_ident SET DEFAULT signed_literal
	  {
	    LEX *lex=Lex;
	    lex->alter_info.alter_list.push_back(new Alter_column($3.str,$6));
	    lex->alter_info.flags|= ALTER_CHANGE_COLUMN_DEFAULT;
	  }
	| ALTER opt_column field_ident DROP DEFAULT
	  {
	    LEX *lex=Lex;
	    lex->alter_info.alter_list.push_back(new Alter_column($3.str,
                                                                  (Item*) 0));
	    lex->alter_info.flags|= ALTER_CHANGE_COLUMN_DEFAULT;
	  }
	| RENAME opt_to table_ident
	  {
	    LEX *lex=Lex;
            THD *thd= lex->thd;
	    lex->select_lex.db=$3->db.str;
            if (lex->select_lex.db == NULL &&
                thd->copy_db_to(&lex->select_lex.db, NULL))
            {
              YYABORT;
            }
            if (check_table_name($3->table.str,$3->table.length) ||
                $3->db.str && check_db_name($3->db.str))
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $3->table.str);
              YYABORT;
            }
	    lex->name= $3->table.str;
	    lex->alter_info.flags|= ALTER_RENAME;
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
	      YYABORT;
	    }
	    LEX *lex= Lex;
	    lex->create_info.table_charset=
	      lex->create_info.default_table_charset= $5;
	    lex->create_info.used_fields|= (HA_CREATE_USED_CHARSET |
					    HA_CREATE_USED_DEFAULT_CHARSET);
	    lex->alter_info.flags|= ALTER_CONVERT;
	  }
        | create_table_options_space_separated
	  {
	    LEX *lex=Lex;
	    lex->alter_info.flags|= ALTER_OPTIONS;
	  }
	| FORCE_SYM
	  {
	    Lex->alter_info.flags|= ALTER_FORCE;
	   }
	| order_clause
	  {
	    LEX *lex=Lex;
	    lex->alter_info.flags|= ALTER_ORDER;
	  };

opt_column:
	/* empty */	{}
	| COLUMN_SYM	{};

opt_ignore:
	/* empty */	{ Lex->ignore= 0;}
	| IGNORE_SYM	{ Lex->ignore= 1;}
	;

opt_restrict:
	/* empty */	{ Lex->drop_mode= DROP_DEFAULT; }
	| RESTRICT	{ Lex->drop_mode= DROP_RESTRICT; }
	| CASCADE	{ Lex->drop_mode= DROP_CASCADE; }
	;

opt_place:
	/* empty */	{}
	| AFTER_SYM ident { store_position_for_column($2.str); }
	| FIRST_SYM	  { store_position_for_column(first_keyword); };

opt_to:
	/* empty */	{}
	| TO_SYM	{}
	| EQ		{}
	| AS		{};

/*
  SLAVE START and SLAVE STOP are deprecated. We keep them for compatibility.
*/

slave:
	  START_SYM SLAVE slave_thread_opts
          {
	    LEX *lex=Lex;
            lex->sql_command = SQLCOM_SLAVE_START;
	    lex->type = 0;
	    /* We'll use mi structure for UNTIL options */
	    bzero((char*) &lex->mi, sizeof(lex->mi));
            /* If you change this code don't forget to update SLAVE START too */
          }
          slave_until
          {}
        | STOP_SYM SLAVE slave_thread_opts
          {
	    LEX *lex=Lex;
            lex->sql_command = SQLCOM_SLAVE_STOP;
	    lex->type = 0;
            /* If you change this code don't forget to update SLAVE STOP too */
          }
	| SLAVE START_SYM slave_thread_opts
         {
	   LEX *lex=Lex;
           lex->sql_command = SQLCOM_SLAVE_START;
	   lex->type = 0;
	    /* We'll use mi structure for UNTIL options */
	    bzero((char*) &lex->mi, sizeof(lex->mi));
          }
          slave_until
          {}
	| SLAVE STOP_SYM slave_thread_opts
         {
	   LEX *lex=Lex;
           lex->sql_command = SQLCOM_SLAVE_STOP;
	   lex->type = 0;
         }
        ;


start:
	START_SYM TRANSACTION_SYM start_transaction_opts
        {
          LEX *lex= Lex;
          lex->sql_command= SQLCOM_BEGIN;
          lex->start_transaction_opt= $3;
        }
	;

start_transaction_opts:
        /*empty*/ { $$ = 0; }
        | WITH CONSISTENT_SYM SNAPSHOT_SYM
        {
           $$= MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT;
        }
        ;

slave_thread_opts:
	{ Lex->slave_thd_opt= 0; }
	slave_thread_opt_list
        {}
	;

slave_thread_opt_list:
	slave_thread_opt
	| slave_thread_opt_list ',' slave_thread_opt
	;

slave_thread_opt:
	/*empty*/	{}
	| SQL_THREAD	{ Lex->slave_thd_opt|=SLAVE_SQL; }
	| RELAY_THREAD 	{ Lex->slave_thd_opt|=SLAVE_IO; }
	;

slave_until:
	/*empty*/	{}
	| UNTIL_SYM slave_until_opts
          {
            LEX *lex=Lex;
            if ((lex->mi.log_file_name || lex->mi.pos) &&
                (lex->mi.relay_log_name || lex->mi.relay_log_pos) ||
                !((lex->mi.log_file_name && lex->mi.pos) ||
                  (lex->mi.relay_log_name && lex->mi.relay_log_pos)))
            {
               my_message(ER_BAD_SLAVE_UNTIL_COND,
                          ER(ER_BAD_SLAVE_UNTIL_COND), MYF(0));
               YYABORT;
            }

          }
	;

slave_until_opts:
       master_file_def
       | slave_until_opts ',' master_file_def ;


restore:
	RESTORE_SYM table_or_tables
	{
	   Lex->sql_command = SQLCOM_RESTORE_TABLE;
	}
	table_list FROM TEXT_STRING_sys
        {
	  Lex->backup_dir = $6.str;
        };

backup:
	BACKUP_SYM table_or_tables
	{
	   Lex->sql_command = SQLCOM_BACKUP_TABLE;
	}
	table_list TO_SYM TEXT_STRING_sys
        {
	  Lex->backup_dir = $6.str;
        };

checksum:
        CHECKSUM_SYM table_or_tables
	{
	   LEX *lex=Lex;
	   lex->sql_command = SQLCOM_CHECKSUM;
	}
	table_list opt_checksum_type
        {}
	;

opt_checksum_type:
        /* nothing */  { Lex->check_opt.flags= 0; }
	| QUICK        { Lex->check_opt.flags= T_QUICK; }
	| EXTENDED_SYM { Lex->check_opt.flags= T_EXTEND; }
        ;

repair:
	REPAIR opt_no_write_to_binlog table_or_tables
	{
	   LEX *lex=Lex;
	   lex->sql_command = SQLCOM_REPAIR;
           lex->no_write_to_binlog= $2;
	   lex->check_opt.init();
	}
	table_list opt_mi_repair_type
	{}
	;

opt_mi_repair_type:
	/* empty */ { Lex->check_opt.flags = T_MEDIUM; }
	| mi_repair_types {};

mi_repair_types:
	mi_repair_type {}
	| mi_repair_type mi_repair_types {};

mi_repair_type:
	QUICK          { Lex->check_opt.flags|= T_QUICK; }
	| EXTENDED_SYM { Lex->check_opt.flags|= T_EXTEND; }
        | USE_FRM      { Lex->check_opt.sql_flags|= TT_USEFRM; };

analyze:
	ANALYZE_SYM opt_no_write_to_binlog table_or_tables
	{
	   LEX *lex=Lex;
	   lex->sql_command = SQLCOM_ANALYZE;
           lex->no_write_to_binlog= $2;
	   lex->check_opt.init();
	}
	table_list opt_mi_check_type
	{}
	;

check:
	CHECK_SYM table_or_tables
	{
	  LEX *lex=Lex;

	  if (lex->sphead)
	  {
	    my_error(ER_SP_BADSTATEMENT, MYF(0), "CHECK");
	    YYABORT;
	  }
	  lex->sql_command = SQLCOM_CHECK;
	  lex->check_opt.init();
	}
	table_list opt_mi_check_type
	{}
	;

opt_mi_check_type:
	/* empty */ { Lex->check_opt.flags = T_MEDIUM; }
	| mi_check_types {};

mi_check_types:
	mi_check_type {}
	| mi_check_type mi_check_types {};

mi_check_type:
	QUICK      { Lex->check_opt.flags|= T_QUICK; }
	| FAST_SYM { Lex->check_opt.flags|= T_FAST; }
	| MEDIUM_SYM { Lex->check_opt.flags|= T_MEDIUM; }
	| EXTENDED_SYM { Lex->check_opt.flags|= T_EXTEND; }
	| CHANGED  { Lex->check_opt.flags|= T_CHECK_ONLY_CHANGED; }
        | FOR_SYM UPGRADE_SYM { Lex->check_opt.sql_flags|= TT_FOR_UPGRADE; };

optimize:
	OPTIMIZE opt_no_write_to_binlog table_or_tables
	{
	   LEX *lex=Lex;
	   lex->sql_command = SQLCOM_OPTIMIZE;
           lex->no_write_to_binlog= $2;
	   lex->check_opt.init();
	}
	table_list opt_mi_check_type
	{}
	;

opt_no_write_to_binlog:
	/* empty */        { $$= 0; }
	| NO_WRITE_TO_BINLOG  { $$= 1; }
	| LOCAL_SYM  { $$= 1; }
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
            YYABORT;
        }
        | rename_list ',' user TO_SYM user
          {
            if (Lex->users_list.push_back($3) || Lex->users_list.push_back($5))
              YYABORT;
          }
        ;

table_to_table_list:
	table_to_table
	| table_to_table_list ',' table_to_table;

table_to_table:
	table_ident TO_SYM table_ident
	{
	  LEX *lex=Lex;
	  SELECT_LEX *sl= lex->current_select;
	  if (!sl->add_table_to_list(lex->thd, $1,NULL,TL_OPTION_UPDATING,
				     TL_IGNORE) ||
	      !sl->add_table_to_list(lex->thd, $3,NULL,TL_OPTION_UPDATING,
				     TL_IGNORE))
	    YYABORT;
	};

keycache:
        CACHE_SYM INDEX_SYM keycache_list IN_SYM key_cache_name
        {
          LEX *lex=Lex;
          lex->sql_command= SQLCOM_ASSIGN_TO_KEYCACHE;
	  lex->ident= $5;
        }
        ;

keycache_list:
        assign_to_keycache
        | keycache_list ',' assign_to_keycache;

assign_to_keycache:
        table_ident cache_keys_spec
        {
          LEX *lex=Lex;
          SELECT_LEX *sel= &lex->select_lex;
          if (!sel->add_table_to_list(lex->thd, $1, NULL, 0,
                                      TL_READ,
                                      sel->get_use_index(),
                                      (List<String> *)0))
            YYABORT;
        }
        ;

key_cache_name:
	ident	   { $$= $1; }
	| DEFAULT  { $$ = default_key_cache_base; }
	;

preload:
	LOAD INDEX_SYM INTO CACHE_SYM
	{
	  LEX *lex=Lex;
	  lex->sql_command=SQLCOM_PRELOAD_KEYS;
	}
	preload_list
	{}
	;

preload_list:
	preload_keys
	| preload_list ',' preload_keys;

preload_keys:
	table_ident cache_keys_spec opt_ignore_leaves
	{
	  LEX *lex=Lex;
	  SELECT_LEX *sel= &lex->select_lex;
	  if (!sel->add_table_to_list(lex->thd, $1, NULL, $3,
                                      TL_READ,
                                      sel->get_use_index(),
                                      (List<String> *)0))
            YYABORT;
	}
	;

cache_keys_spec:
        { Select->interval_list.empty(); }
        cache_key_list_or_empty
        {
          LEX *lex=Lex;
          SELECT_LEX *sel= &lex->select_lex;
          sel->use_index= sel->interval_list;
        }
        ;

cache_key_list_or_empty:
	/* empty */	{ Lex->select_lex.use_index_ptr= 0; }
	| opt_key_or_index '(' key_usage_list2 ')'
	  {
            SELECT_LEX *sel= &Lex->select_lex;
	    sel->use_index_ptr= &sel->use_index;
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
	  LEX *lex= Lex;
	  lex->sql_command= SQLCOM_SELECT;
	}
	;

/* Need select_init2 for subselects. */
select_init:
	SELECT_SYM select_init2
	|
	'(' select_paren ')' union_opt;

select_paren:
	SELECT_SYM select_part2
	  {
	    LEX *lex= Lex;
            SELECT_LEX * sel= lex->current_select;
	    if (sel->set_braces(1))
	    {
	      yyerror(ER(ER_SYNTAX_ERROR));
	      YYABORT;
	    }
	  if (sel->linkage == UNION_TYPE &&
	      !sel->master_unit()->first_select()->braces)
	  {
	    yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
	  }
            /* select in braces, can't contain global parameters */
	    if (sel->master_unit()->fake_select_lex)
              sel->master_unit()->global_parameters=
                 sel->master_unit()->fake_select_lex;
          }
	| '(' select_paren ')';

select_init2:
	select_part2
        {
	  LEX *lex= Lex;
          SELECT_LEX * sel= lex->current_select;
          if (lex->current_select->set_braces(0))
	  {
	    yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
	  }
	  if (sel->linkage == UNION_TYPE &&
	      sel->master_unit()->first_select()->braces)
	  {
	    yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
	  }
	}
	union_clause
	;

select_part2:
	{
	  LEX *lex= Lex;
	  SELECT_LEX *sel= lex->current_select;
	  if (sel->linkage != UNION_TYPE)
	    mysql_init_select(lex);
	  lex->current_select->parsing_place= SELECT_LIST;
	}
	select_options select_item_list
	{
	  Select->parsing_place= NO_MATTER;
	}
	select_into select_lock_type;

select_into:
	opt_order_clause opt_limit_clause {}
        | into
	| select_from
	| into select_from
	| select_from into;

select_from:
	  FROM join_table_list where_clause group_clause having_clause
	       opt_order_clause opt_limit_clause procedure_clause
        | FROM DUAL_SYM where_clause opt_limit_clause
          /* oracle compatibility: oracle always requires FROM clause,
             and DUAL is system table without fields.
             Is "SELECT 1 FROM DUAL" any better than "SELECT 1" ?
          Hmmm :) */
	;

select_options:
	/* empty*/
	| select_option_list
	  {
	    if (test_all_bits(Select->options, SELECT_ALL | SELECT_DISTINCT))
	    {
	      my_error(ER_WRONG_USAGE, MYF(0), "ALL", "DISTINCT");
              YYABORT;
	    }
          }
	  ;

select_option_list:
	select_option_list select_option
	| select_option;

select_option:
	STRAIGHT_JOIN { Select->options|= SELECT_STRAIGHT_JOIN; }
	| HIGH_PRIORITY
	  {
	    if (check_simple_select())
	      YYABORT;
	    Lex->lock_option= TL_READ_HIGH_PRIORITY;
	  }
	| DISTINCT         { Select->options|= SELECT_DISTINCT; }
	| SQL_SMALL_RESULT { Select->options|= SELECT_SMALL_RESULT; }
	| SQL_BIG_RESULT { Select->options|= SELECT_BIG_RESULT; }
	| SQL_BUFFER_RESULT
	  {
	    if (check_simple_select())
	      YYABORT;
	    Select->options|= OPTION_BUFFER_RESULT;
	  }
	| SQL_CALC_FOUND_ROWS
	  {
	    if (check_simple_select())
	      YYABORT;
	    Select->options|= OPTION_FOUND_ROWS;
	  }
	| SQL_NO_CACHE_SYM
          {
            Lex->safe_to_cache_query=0;
	    Lex->select_lex.options&= ~OPTION_TO_QUERY_CACHE;
            Lex->select_lex.sql_cache= SELECT_LEX::SQL_NO_CACHE;
          }
	| SQL_CACHE_SYM
	  {
            /* Honor this flag only if SQL_NO_CACHE wasn't specified. */
            if (Lex->select_lex.sql_cache != SELECT_LEX::SQL_NO_CACHE)
            {
              Lex->safe_to_cache_query=1;
	      Lex->select_lex.options|= OPTION_TO_QUERY_CACHE;
              Lex->select_lex.sql_cache= SELECT_LEX::SQL_CACHE;
            }
	  }
	| ALL		    { Select->options|= SELECT_ALL; }
	;

select_lock_type:
	/* empty */
	| FOR_SYM UPDATE_SYM
	  {
	    LEX *lex=Lex;
	    lex->current_select->set_lock_for_tables(TL_WRITE);
	    lex->safe_to_cache_query=0;
	  }
	| LOCK_SYM IN_SYM SHARE_SYM MODE_SYM
	  {
	    LEX *lex=Lex;
	    lex->current_select->
	      set_lock_for_tables(TL_READ_WITH_SHARED_LOCKS);
	    lex->safe_to_cache_query=0;
	  }
	;

select_item_list:
	  select_item_list ',' select_item
	| select_item
	| '*'
	  {
	    THD *thd= YYTHD;
	    if (add_item_to_list(thd,
                                 new Item_field(&thd->lex->current_select->
                                                context,
                                                NULL, NULL, "*")))
	      YYABORT;
	    (thd->lex->current_select->with_wild)++;
	  };


select_item:
	  remember_name select_item2 remember_end select_alias
	  {
	    if (add_item_to_list(YYTHD, $2))
	      YYABORT;
	    if ($4.str)
            {
              $2->is_autogenerated_name= FALSE;
	      $2->set_name($4.str, $4.length, system_charset_info);
            }
	    else if (!$2->name) {
	      char *str = $1;
	      if (str[-1] == '`')
	        str--;
	      $2->set_name(str,(uint) ($3 - str), YYTHD->charset());
	    }
	  };

remember_name:
	{ $$=(char*) Lex->tok_start; };

remember_end:
	{ $$=(char*) Lex->tok_end; };

select_item2:
	table_wild	{ $$=$1; } /* table.* */
	| expr		{ $$=$1; };

select_alias:
	/* empty */		{ $$=null_lex_str;}
	| AS ident		{ $$=$2; }
	| AS TEXT_STRING_sys	{ $$=$2; }
	| ident			{ $$=$1; }
	| TEXT_STRING_sys	{ $$=$1; }
	;

optional_braces:
	/* empty */ {}
	| '(' ')' {};

/* all possible expressions */
expr:	
	  bool_term { Select->expr_list.push_front(new List<Item>); }
          bool_or_expr
          {
            List<Item> *list= Select->expr_list.pop();
            if (list->elements)
            {
              list->push_front($1);
              $$= new Item_cond_or(*list);
              /* optimize construction of logical OR to reduce
                 amount of objects for complex expressions */
            }
            else
              $$= $1;
            delete list;
          }
	;

bool_or_expr:
	/* empty */
        | bool_or_expr or bool_term
          { Select->expr_list.head()->push_back($3); }
        ;

bool_term:
	bool_term XOR bool_term { $$= new Item_cond_xor($1,$3); }
	| bool_factor { Select->expr_list.push_front(new List<Item>); }
          bool_and_expr
          {
            List<Item> *list= Select->expr_list.pop();
            if (list->elements)
            {
              list->push_front($1);
              $$= new Item_cond_and(*list);
              /* optimize construction of logical AND to reduce
                 amount of objects for complex expressions */
            }
            else
              $$= $1;
            delete list;
          }
	;

bool_and_expr:
	/* empty */
        | bool_and_expr and bool_factor
          { Select->expr_list.head()->push_back($3); }
        ;

bool_factor:
	NOT_SYM bool_factor	{ $$= negate_expression(YYTHD, $2); }
	| bool_test ;

bool_test:
	bool_pri IS TRUE_SYM	{ $$= is_truth_value($1,1,0); }
	| bool_pri IS not TRUE_SYM { $$= is_truth_value($1,0,0); }
	| bool_pri IS FALSE_SYM	{ $$= is_truth_value($1,0,1); }
	| bool_pri IS not FALSE_SYM { $$= is_truth_value($1,1,1); }
	| bool_pri IS UNKNOWN_SYM { $$= new Item_func_isnull($1); }
	| bool_pri IS not UNKNOWN_SYM { $$= new Item_func_isnotnull($1); }
	| bool_pri ;

bool_pri:
	bool_pri IS NULL_SYM	{ $$= new Item_func_isnull($1); }
	| bool_pri IS not NULL_SYM { $$= new Item_func_isnotnull($1); }
	| bool_pri EQUAL_SYM predicate	{ $$= new Item_func_equal($1,$3); }
	| bool_pri comp_op predicate %prec EQ
	  { $$= (*$2)(0)->create($1,$3); }
	| bool_pri comp_op all_or_any in_subselect %prec EQ
	  { $$= all_any_subquery_creator($1, $2, $3, $4); }
	| predicate ;

predicate:
	 bit_expr IN_SYM '(' expr_list ')'
	  { 
            if ($4->elements == 1)
              $$= new Item_func_eq($1, $4->head());
            else
            {
              $4->push_front($1);
              $$= new Item_func_in(*$4);
            }
          }
	| bit_expr not IN_SYM '(' expr_list ')'
          {
            if ($5->elements == 1)
              $$= new Item_func_ne($1, $5->head());
            else
            {
              $5->push_front($1);
              Item_func_in *item = new Item_func_in(*$5);
              item->negate();
              $$= item;
            }            
          }
        | bit_expr IN_SYM in_subselect
	  { $$= new Item_in_subselect($1, $3); }
	| bit_expr not IN_SYM in_subselect
          { $$= negate_expression(YYTHD, new Item_in_subselect($1, $4)); }
	| bit_expr BETWEEN_SYM bit_expr AND_SYM predicate
	  { $$= new Item_func_between($1,$3,$5); }
	| bit_expr not BETWEEN_SYM bit_expr AND_SYM predicate
    {
      Item_func_between *item= new Item_func_between($1,$4,$6);
      item->negate();
      $$= item;
    }
	| bit_expr SOUNDS_SYM LIKE bit_expr
	  { $$= new Item_func_eq(new Item_func_soundex($1),
				 new Item_func_soundex($4)); }
	| bit_expr LIKE simple_expr opt_escape
          { $$= new Item_func_like($1,$3,$4,Lex->escape_used); }
	| bit_expr not LIKE simple_expr opt_escape
          { $$= new Item_func_not(new Item_func_like($1,$4,$5, Lex->escape_used)); }
	| bit_expr REGEXP bit_expr	{ $$= new Item_func_regex($1,$3); }
	| bit_expr not REGEXP bit_expr
          { $$= negate_expression(YYTHD, new Item_func_regex($1,$4)); }
	| bit_expr ;

bit_expr:
	bit_expr '|' bit_term	{ $$= new Item_func_bit_or($1,$3); }
	| bit_term ;

bit_term:
	bit_term '&' bit_factor	{ $$= new Item_func_bit_and($1,$3); }
	| bit_factor ;

bit_factor:
	bit_factor SHIFT_LEFT value_expr
	  { $$= new Item_func_shift_left($1,$3); }
	| bit_factor SHIFT_RIGHT value_expr 
	  { $$= new Item_func_shift_right($1,$3); }
	| value_expr ;

value_expr:
	value_expr '+' term	{ $$= new Item_func_plus($1,$3); }
	| value_expr '-' term	{ $$= new Item_func_minus($1,$3); }
	| value_expr '+' interval_expr interval
	  { $$= new Item_date_add_interval($1,$3,$4,0); }
	| value_expr '-' interval_expr interval
	  { $$= new Item_date_add_interval($1,$3,$4,1); }
	| term ;

term:
	term '*' factor		{ $$= new Item_func_mul($1,$3); }
	| term '/' factor	{ $$= new Item_func_div($1,$3); }
	| term '%' factor	{ $$= new Item_func_mod($1,$3); }
	| term DIV_SYM factor	{ $$= new Item_func_int_div($1,$3); }
	| term MOD_SYM factor	{ $$= new Item_func_mod($1,$3); }
	| factor ;

factor:
        factor '^' simple_expr	{ $$= new Item_func_bit_xor($1,$3); }
	| simple_expr ;

or:	OR_SYM | OR2_SYM;
and:	AND_SYM | AND_AND_SYM;
not:	NOT_SYM | NOT2_SYM;
not2:	'!' | NOT2_SYM;

comp_op:  EQ		{ $$ = &comp_eq_creator; }
	| GE		{ $$ = &comp_ge_creator; }
	| GT_SYM	{ $$ = &comp_gt_creator; }
	| LE		{ $$ = &comp_le_creator; }
	| LT		{ $$ = &comp_lt_creator; }
	| NE		{ $$ = &comp_ne_creator; }
	;

all_or_any: ALL     { $$ = 1; }
        |   ANY_SYM { $$ = 0; }
        ;

interval_expr:
         INTERVAL_SYM expr { $$=$2; }
        ;

simple_expr:
	simple_ident
 	| simple_expr COLLATE_SYM ident_or_text %prec NEG
	  {
	    $$= new Item_func_set_collation($1,
					    new Item_string($3.str,
							    $3.length,
                                                            YYTHD->charset()));
	  }
	| literal
	| param_marker
	| '@' ident_or_text SET_VAR expr
	  {
	    $$= new Item_func_set_user_var($2,$4);
	    LEX *lex= Lex;
	    lex->uncacheable(UNCACHEABLE_RAND);
	    lex->variables_used= 1;
	  }
	| '@' ident_or_text
	  {
	    $$= new Item_func_get_user_var($2);
	    LEX *lex= Lex;
	    lex->uncacheable(UNCACHEABLE_RAND);
	    lex->variables_used= 1;
	  }
	| '@' '@' opt_var_ident_type ident_or_text opt_component
	  {

            if ($4.str && $5.str && check_reserved_words(&$4))
            {
              yyerror(ER(ER_SYNTAX_ERROR));
              YYABORT;
            }
	    if (!($$= get_system_var(YYTHD, $3, $4, $5)))
	      YYABORT;
	    Lex->variables_used= 1;
	  }
	| sum_expr
	| simple_expr OR_OR_SYM simple_expr
	  { $$= new Item_func_concat($1, $3); }
	| '+' simple_expr %prec NEG	{ $$= $2; }
	| '-' simple_expr %prec NEG	{ $$= new Item_func_neg($2); }
	| '~' simple_expr %prec NEG	{ $$= new Item_func_bit_neg($2); }
	| not2 simple_expr %prec NEG	{ $$= negate_expression(YYTHD, $2); }
	| '(' expr ')'		{ $$= $2; }
	| '(' expr ',' expr_list ')'
	  {
	    $4->push_front($2);
	    $$= new Item_row(*$4);
	  }
	| ROW_SYM '(' expr ',' expr_list ')'
	  {
	    $5->push_front($3);
	    $$= new Item_row(*$5);
	  }
	| EXISTS exists_subselect { $$= $2; }
	| singlerow_subselect   { $$= $1; }
	| '{' ident expr '}'	{ $$= $3; }
        | MATCH ident_list_arg AGAINST '(' bit_expr fulltext_options ')'
          { $2->push_front($5);
            Select->add_ftfunc_to_list((Item_func_match*)
                                        ($$=new Item_func_match(*$2,$6))); }
	| ASCII_SYM '(' expr ')' { $$= new Item_func_ascii($3); }
	| BINARY simple_expr %prec NEG
	  {
            $$= create_func_cast($2, ITEM_CAST_CHAR, -1, 0, &my_charset_bin);
	  }
	| CAST_SYM '(' expr AS cast_type ')'
	  {
            LEX *lex= Lex;
	    $$= create_func_cast($3, $5,
                                 lex->length ? atoi(lex->length) : -1,
                                 lex->dec ? atoi(lex->dec) : 0,
                                 lex->charset);
	  }
	| CASE_SYM opt_expr WHEN_SYM when_list opt_else END
	  { $$= new Item_func_case(* $4, $2, $5 ); }
	| CONVERT_SYM '(' expr ',' cast_type ')'
	  {
	    $$= create_func_cast($3, $5,
				 Lex->length ? atoi(Lex->length) : -1,
                                 Lex->dec ? atoi(Lex->dec) : 0,
				 Lex->charset);
	  }
	| CONVERT_SYM '(' expr USING charset_name ')'
	  { $$= new Item_func_conv_charset($3,$5); }
	| DEFAULT '(' simple_ident ')'
	  {
	    if ($3->is_splocal())
	    {
	      Item_splocal *il= static_cast<Item_splocal *>($3);

	      my_error(ER_WRONG_COLUMN_NAME, MYF(0), il->my_name()->str);
	      YYABORT;
	    }
	    $$= new Item_default_value(Lex->current_context(), $3);
	  }
	| VALUES '(' simple_ident_nospvar ')'
	  { $$= new Item_insert_value(Lex->current_context(), $3); }
	| FUNC_ARG0 '(' ')'
	  {
	    if (!$1.symbol->create_func)
	    {
              my_error(ER_FEATURE_DISABLED, MYF(0),
                       $1.symbol->group->name,
                       $1.symbol->group->needed_define);
	      YYABORT;
	    }
	    $$= ((Item*(*)(void))($1.symbol->create_func))();
	  }
	| FUNC_ARG1 '(' expr ')'
	  {
	    if (!$1.symbol->create_func)
	    {
              my_error(ER_FEATURE_DISABLED, MYF(0),
                       $1.symbol->group->name,
                       $1.symbol->group->needed_define);
	      YYABORT;
	    }
	    $$= ((Item*(*)(Item*))($1.symbol->create_func))($3);
	  }
	| FUNC_ARG2 '(' expr ',' expr ')'
	  {
	    if (!$1.symbol->create_func)
	    {
	      my_error(ER_FEATURE_DISABLED, MYF(0),
                       $1.symbol->group->name,
                       $1.symbol->group->needed_define);
	      YYABORT;
	    }
	    $$= ((Item*(*)(Item*,Item*))($1.symbol->create_func))($3,$5);
	  }
	| FUNC_ARG3 '(' expr ',' expr ',' expr ')'
	  {
	    if (!$1.symbol->create_func)
	    {
              my_error(ER_FEATURE_DISABLED, MYF(0),
                       $1.symbol->group->name,
                       $1.symbol->group->needed_define);
	      YYABORT;
	    }
	    $$= ((Item*(*)(Item*,Item*,Item*))($1.symbol->create_func))($3,$5,$7);
	  }
	| ADDDATE_SYM '(' expr ',' expr ')'
	  { $$= new Item_date_add_interval($3, $5, INTERVAL_DAY, 0);}
	| ADDDATE_SYM '(' expr ',' INTERVAL_SYM expr interval ')'
	  { $$= new Item_date_add_interval($3, $6, $7, 0); }
	| REPEAT_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_repeat($3,$5); }
	| ATAN	'(' expr ')'
	  { $$= new Item_func_atan($3); }
	| ATAN	'(' expr ',' expr ')'
	  { $$= new Item_func_atan($3,$5); }
	| CHAR_SYM '(' expr_list ')'
	  { $$= new Item_func_char(*$3); }
	| CHAR_SYM '(' expr_list USING charset_name ')'
	  { $$= new Item_func_char(*$3, $5); }
	| CHARSET '(' expr ')'
	  { $$= new Item_func_charset($3); }
	| COALESCE '(' expr_list ')'
	  { $$= new Item_func_coalesce(* $3); }
	| COLLATION_SYM '(' expr ')'
	  { $$= new Item_func_collation($3); }
	| CONCAT '(' expr_list ')'
	  { $$= new Item_func_concat(* $3); }
	| CONCAT_WS '(' expr ',' expr_list ')'
	  { $5->push_front($3); $$= new Item_func_concat_ws(*$5); }
	| CONVERT_TZ_SYM '(' expr ',' expr ',' expr ')'
	  {
            if (Lex->add_time_zone_tables_to_query_tables(YYTHD))
              YYABORT;
	    $$= new Item_func_convert_tz($3, $5, $7);
	  }
	| CURDATE optional_braces
	  { $$= new Item_func_curdate_local(); Lex->safe_to_cache_query=0; }
	| CURTIME optional_braces
	  { $$= new Item_func_curtime_local(); Lex->safe_to_cache_query=0; }
	| CURTIME '(' expr ')'
	  {
	    $$= new Item_func_curtime_local($3);
	    Lex->safe_to_cache_query=0;
	  }
	| CURRENT_USER optional_braces
          {
            $$= new Item_func_current_user(Lex->current_context());
            Lex->safe_to_cache_query= 0;
          }
	| DATE_ADD_INTERVAL '(' expr ',' interval_expr interval ')'
	  { $$= new Item_date_add_interval($3,$5,$6,0); }
	| DATE_SUB_INTERVAL '(' expr ',' interval_expr interval ')'
	  { $$= new Item_date_add_interval($3,$5,$6,1); }
	| DATABASE '(' ')'
	  {
	    $$= new Item_func_database();
            Lex->safe_to_cache_query=0;
	  }
	| DATE_SYM '(' expr ')'
	  { $$= new Item_date_typecast($3); }
	| DAY_SYM '(' expr ')'
	  { $$= new Item_func_dayofmonth($3); }
	| ELT_FUNC '(' expr ',' expr_list ')'
	  { $5->push_front($3); $$= new Item_func_elt(*$5); }
	| MAKE_SET_SYM '(' expr ',' expr_list ')'
	  { $$= new Item_func_make_set($3, *$5); }
	| ENCRYPT '(' expr ')'
	  {
	    $$= new Item_func_encrypt($3);
	    Lex->uncacheable(UNCACHEABLE_RAND);
	  }
	| ENCRYPT '(' expr ',' expr ')'   { $$= new Item_func_encrypt($3,$5); }
	| DECODE_SYM '(' expr ',' TEXT_STRING_literal ')'
	  { $$= new Item_func_decode($3,$5.str); }
	| ENCODE_SYM '(' expr ',' TEXT_STRING_literal ')'
	 { $$= new Item_func_encode($3,$5.str); }
	| DES_DECRYPT_SYM '(' expr ')'
        { $$= new Item_func_des_decrypt($3); }
	| DES_DECRYPT_SYM '(' expr ',' expr ')'
        { $$= new Item_func_des_decrypt($3,$5); }
	| DES_ENCRYPT_SYM '(' expr ')'
        { $$= new Item_func_des_encrypt($3); }
	| DES_ENCRYPT_SYM '(' expr ',' expr ')'
        { $$= new Item_func_des_encrypt($3,$5); }
	| EXPORT_SET '(' expr ',' expr ',' expr ')'
		{ $$= new Item_func_export_set($3, $5, $7); }
	| EXPORT_SET '(' expr ',' expr ',' expr ',' expr ')'
		{ $$= new Item_func_export_set($3, $5, $7, $9); }
	| EXPORT_SET '(' expr ',' expr ',' expr ',' expr ',' expr ')'
		{ $$= new Item_func_export_set($3, $5, $7, $9, $11); }
	| FORMAT_SYM '(' expr ',' NUM ')'
	  { $$= new Item_func_format($3,atoi($5.str)); }
	| FROM_UNIXTIME '(' expr ')'
	  { $$= new Item_func_from_unixtime($3); }
	| FROM_UNIXTIME '(' expr ',' expr ')'
	  {
	    $$= new Item_func_date_format (new Item_func_from_unixtime($3),$5,0);
	  }
	| FIELD_FUNC '(' expr ',' expr_list ')'
	  { $5->push_front($3); $$= new Item_func_field(*$5); }
	| geometry_function
	  {
#ifdef HAVE_SPATIAL
	    $$= $1;
#else
	    my_error(ER_FEATURE_DISABLED, MYF(0),
                     sym_group_geom.name, sym_group_geom.needed_define);
	    YYABORT;
#endif
	  }
	| GET_FORMAT '(' date_time_type  ',' expr ')'
	  { $$= new Item_func_get_format($3, $5); }
	| HOUR_SYM '(' expr ')'
	  { $$= new Item_func_hour($3); }
	| IF '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_if($3,$5,$7); }
	| INSERT '(' expr ',' expr ',' expr ',' expr ')'
	  { $$= new Item_func_insert($3,$5,$7,$9); }
	| interval_expr interval '+' expr
	  /* we cannot put interval before - */
	  { $$= new Item_date_add_interval($4,$1,$2,0); }
	| interval_expr
	  {
            if ($1->type() != Item::ROW_ITEM)
            {
              yyerror(ER(ER_SYNTAX_ERROR));
              YYABORT;
            }
            $$= new Item_func_interval((Item_row *)$1);
          }
	| LAST_INSERT_ID '(' ')'
	  {
	    $$= new Item_func_last_insert_id();
	    Lex->safe_to_cache_query= 0;
	  }
	| LAST_INSERT_ID '(' expr ')'
	  {
	    $$= new Item_func_last_insert_id($3);
	    Lex->safe_to_cache_query= 0;
	  }
	| LEFT '(' expr ',' expr ')'
	  { $$= new Item_func_left($3,$5); }
	| LOCATE '(' expr ',' expr ')'
	  { $$= new Item_func_locate($5,$3); }
	| LOCATE '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_locate($5,$3,$7); }
	| GREATEST_SYM '(' expr ',' expr_list ')'
	  { $5->push_front($3); $$= new Item_func_max(*$5); }
	| LEAST_SYM '(' expr ',' expr_list ')'
	  { $5->push_front($3); $$= new Item_func_min(*$5); }
	| LOG_SYM '(' expr ')'
	  { $$= new Item_func_log($3); }
	| LOG_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_log($3, $5); }
	| MASTER_POS_WAIT '(' expr ',' expr ')'
	  {
	    $$= new Item_master_pos_wait($3, $5);
	    Lex->safe_to_cache_query=0;
		  }
	| MASTER_POS_WAIT '(' expr ',' expr ',' expr ')'
	  {
	    $$= new Item_master_pos_wait($3, $5, $7);
	    Lex->safe_to_cache_query=0;
	  }
	| MICROSECOND_SYM '(' expr ')'
	  { $$= new Item_func_microsecond($3); }
	| MINUTE_SYM '(' expr ')'
	  { $$= new Item_func_minute($3); }
	| MOD_SYM '(' expr ',' expr ')'
	  { $$ = new Item_func_mod( $3, $5); }
	| MONTH_SYM '(' expr ')'
	  { $$= new Item_func_month($3); }
	| NOW_SYM optional_braces
	  { $$= new Item_func_now_local(); Lex->safe_to_cache_query=0;}
	| NOW_SYM '(' expr ')'
	  { $$= new Item_func_now_local($3); Lex->safe_to_cache_query=0;}
	| PASSWORD '(' expr ')'
	  {
	    $$= YYTHD->variables.old_passwords ?
              (Item *) new Item_func_old_password($3) :
	      (Item *) new Item_func_password($3);
	  }
	| OLD_PASSWORD '(' expr ')'
	  { $$=  new Item_func_old_password($3); }
	| POSITION_SYM '(' bit_expr IN_SYM expr ')'
	  { $$ = new Item_func_locate($5,$3); }
	| QUARTER_SYM '(' expr ')'
	  { $$ = new Item_func_quarter($3); }
	| RAND '(' expr ')'
	  { $$= new Item_func_rand($3); Lex->uncacheable(UNCACHEABLE_RAND);}
	| RAND '(' ')'
	  { $$= new Item_func_rand(); Lex->uncacheable(UNCACHEABLE_RAND);}
	| REPLACE '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_replace($3,$5,$7); }
	| RIGHT '(' expr ',' expr ')'
	  { $$= new Item_func_right($3,$5); }
	| ROUND '(' expr ')'
	  { $$= new Item_func_round($3, new Item_int((char*)"0",0,1),0); }
	| ROUND '(' expr ',' expr ')' { $$= new Item_func_round($3,$5,0); }
	| ROW_COUNT_SYM '(' ')'
	  {
	    $$= new Item_func_row_count();
	    Lex->safe_to_cache_query= 0;
	  }
	| SUBDATE_SYM '(' expr ',' expr ')'
	  { $$= new Item_date_add_interval($3, $5, INTERVAL_DAY, 1);}
	| SUBDATE_SYM '(' expr ',' INTERVAL_SYM expr interval ')'
	  { $$= new Item_date_add_interval($3, $6, $7, 1); }
	| SECOND_SYM '(' expr ')'
	  { $$= new Item_func_second($3); }
	| SUBSTRING '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_substr($3,$5,$7); }
	| SUBSTRING '(' expr ',' expr ')'
	  { $$= new Item_func_substr($3,$5); }
	| SUBSTRING '(' expr FROM expr FOR_SYM expr ')'
	  { $$= new Item_func_substr($3,$5,$7); }
	| SUBSTRING '(' expr FROM expr ')'
	  { $$= new Item_func_substr($3,$5); }
	| SUBSTRING_INDEX '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_substr_index($3,$5,$7); }
	| SYSDATE optional_braces
          {
            if (global_system_variables.sysdate_is_now == 0)
              $$= new Item_func_sysdate_local();
            else $$= new Item_func_now_local();
            Lex->safe_to_cache_query=0;
          }
	| SYSDATE '(' expr ')'
          {
            if (global_system_variables.sysdate_is_now == 0)
              $$= new Item_func_sysdate_local($3);
            else $$= new Item_func_now_local($3);
            Lex->safe_to_cache_query=0;
          }
	| TIME_SYM '(' expr ')'
	  { $$= new Item_time_typecast($3); }
	| TIMESTAMP '(' expr ')'
	  { $$= new Item_datetime_typecast($3); }
	| TIMESTAMP '(' expr ',' expr ')'
	  { $$= new Item_func_add_time($3, $5, 1, 0); }
	| TIMESTAMP_ADD '(' interval_time_st ',' expr ',' expr ')'
	  { $$= new Item_date_add_interval($7,$5,$3,0); }
	| TIMESTAMP_DIFF '(' interval_time_st ',' expr ',' expr ')'
	  { $$= new Item_func_timestamp_diff($5,$7,$3); }
	| TRIM '(' expr ')'
	  { $$= new Item_func_trim($3); }
	| TRIM '(' LEADING expr FROM expr ')'
	  { $$= new Item_func_ltrim($6,$4); }
	| TRIM '(' TRAILING expr FROM expr ')'
	  { $$= new Item_func_rtrim($6,$4); }
	| TRIM '(' BOTH expr FROM expr ')'
	  { $$= new Item_func_trim($6,$4); }
	| TRIM '(' LEADING FROM expr ')'
	 { $$= new Item_func_ltrim($5); }
	| TRIM '(' TRAILING FROM expr ')'
	  { $$= new Item_func_rtrim($5); }
	| TRIM '(' BOTH FROM expr ')'
	  { $$= new Item_func_trim($5); }
	| TRIM '(' expr FROM expr ')'
	  { $$= new Item_func_trim($5,$3); }
	| TRUNCATE_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_round($3,$5,1); }
	| ident '.' ident '(' udf_expr_list ')'
	  {
	    LEX *lex= Lex;
	    sp_name *name= new sp_name($1, $3);

	    name->init_qname(YYTHD);
	    sp_add_used_routine(lex, YYTHD, name, TYPE_ENUM_FUNCTION);
	    if ($5)
	      $$= new Item_func_sp(Lex->current_context(), name, *$5);
	    else
	      $$= new Item_func_sp(Lex->current_context(), name);
	    lex->safe_to_cache_query=0;
	  }
	| IDENT_sys '(' 
          {
#ifdef HAVE_DLOPEN
            udf_func *udf= 0;
            if (using_udf_functions &&
                (udf= find_udf($1.str, $1.length)) &&
                udf->type == UDFTYPE_AGGREGATE)
            {
              LEX *lex= Lex;
              if (lex->current_select->inc_in_sum_expr())
              {
                yyerror(ER(ER_SYNTAX_ERROR));
                YYABORT;
              }
            }
            $<udf>$= udf;
#endif
          }
          udf_expr_list ')'
          {
#ifdef HAVE_DLOPEN
            udf_func *udf= $<udf>3;
            SELECT_LEX *sel= Select;

            if (udf)
            {
              if (udf->type == UDFTYPE_AGGREGATE)
                Select->in_sum_expr--;

              switch (udf->returns) {
              case STRING_RESULT:
                if (udf->type == UDFTYPE_FUNCTION)
                {
                  if ($4 != NULL)
                    $$ = new Item_func_udf_str(udf, *$4);
                  else
                    $$ = new Item_func_udf_str(udf);
                }
                else
                {
                  if ($4 != NULL)
                    $$ = new Item_sum_udf_str(udf, *$4);
                  else
                    $$ = new Item_sum_udf_str(udf);
                }
                break;
              case REAL_RESULT:
                if (udf->type == UDFTYPE_FUNCTION)
                {
                  if ($4 != NULL)
                    $$ = new Item_func_udf_float(udf, *$4);
                  else
                    $$ = new Item_func_udf_float(udf);
                }
                else
                {
                  if ($4 != NULL)
                    $$ = new Item_sum_udf_float(udf, *$4);
                  else
                    $$ = new Item_sum_udf_float(udf);
                }
                break;
              case INT_RESULT:
                if (udf->type == UDFTYPE_FUNCTION)
                {
                  if ($4 != NULL)
                    $$ = new Item_func_udf_int(udf, *$4);
                  else
                    $$ = new Item_func_udf_int(udf);
                }
                else
                {
                  if ($4 != NULL)
                    $$ = new Item_sum_udf_int(udf, *$4);
                  else
                    $$ = new Item_sum_udf_int(udf);
                }
                break;
              case DECIMAL_RESULT:
                if (udf->type == UDFTYPE_FUNCTION)
                {
                  if ($4 != NULL)
                    $$ = new Item_func_udf_decimal(udf, *$4);
                  else
                    $$ = new Item_func_udf_decimal(udf);
                }
                else
                {
                  if ($4 != NULL)
                    $$ = new Item_sum_udf_decimal(udf, *$4);
                  else
                    $$ = new Item_sum_udf_decimal(udf);
                }
                break;
              default:
                YYABORT;
              }
            }
            else
#endif /* HAVE_DLOPEN */
            {
	      LEX *lex= Lex;
              THD *thd= lex->thd;
              LEX_STRING db;
              if (thd->copy_db_to(&db.str, &db.length))
                YYABORT;
              sp_name *name= new sp_name(db, $1);
              if (name)
                name->init_qname(thd);

              sp_add_used_routine(lex, YYTHD, name, TYPE_ENUM_FUNCTION);
              if ($4)
                $$= new Item_func_sp(Lex->current_context(), name, *$4);
              else
                $$= new Item_func_sp(Lex->current_context(), name);
	      lex->safe_to_cache_query=0;
	    }
          }
	| UNIQUE_USERS '(' text_literal ',' NUM ',' NUM ',' expr_list ')'
	  {
            $$= new Item_func_unique_users($3,atoi($5.str),atoi($7.str), * $9);
	  }
	| UNIX_TIMESTAMP '(' ')'
	  {
	    $$= new Item_func_unix_timestamp();
	    Lex->safe_to_cache_query=0;
	  }
	| UNIX_TIMESTAMP '(' expr ')'
	  { $$= new Item_func_unix_timestamp($3); }
	| USER '(' ')'
	  { $$= new Item_func_user(); Lex->safe_to_cache_query=0; }
	| UTC_DATE_SYM optional_braces
	  { $$= new Item_func_curdate_utc(); Lex->safe_to_cache_query=0;}
	| UTC_TIME_SYM optional_braces
	  { $$= new Item_func_curtime_utc(); Lex->safe_to_cache_query=0;}
	| UTC_TIMESTAMP_SYM optional_braces
	  { $$= new Item_func_now_utc(); Lex->safe_to_cache_query=0;}
	| WEEK_SYM '(' expr ')'
	  {
            $$= new Item_func_week($3,new Item_int((char*) "0",
				   YYTHD->variables.default_week_format,1));
          }
	| WEEK_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_week($3,$5); }
	| YEAR_SYM '(' expr ')'
	  { $$= new Item_func_year($3); }
	| YEARWEEK '(' expr ')'
	  { $$= new Item_func_yearweek($3,new Item_int((char*) "0",0,1)); }
	| YEARWEEK '(' expr ',' expr ')'
	  { $$= new Item_func_yearweek($3, $5); }
	| BENCHMARK_SYM '(' ulong_num ',' expr ')'
	  {
	    $$=new Item_func_benchmark($3,$5);
	    Lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
	  }
	| EXTRACT_SYM '(' interval FROM expr ')'
	{ $$=new Item_extract( $3, $5); };

geometry_function:
	  CONTAINS_SYM '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_spatial_rel($3, $5, Item_func::SP_CONTAINS_FUNC)); }
	| GEOMFROMTEXT '(' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3)); }
	| GEOMFROMTEXT '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3, $5)); }
	| GEOMFROMWKB '(' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_wkb($3)); }
	| GEOMFROMWKB '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_wkb($3, $5)); }
	| GEOMETRYCOLLECTION '(' expr_list ')'
	  { $$= GEOM_NEW(Item_func_spatial_collection(* $3,
                           Geometry::wkb_geometrycollection,
                           Geometry::wkb_point)); }
	| LINESTRING '(' expr_list ')'
	  { $$= GEOM_NEW(Item_func_spatial_collection(* $3,
                  Geometry::wkb_linestring, Geometry::wkb_point)); }
 	| MULTILINESTRING '(' expr_list ')'
	  { $$= GEOM_NEW( Item_func_spatial_collection(* $3,
                   Geometry::wkb_multilinestring, Geometry::wkb_linestring)); }
 	| MLINEFROMTEXT '(' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3)); }
	| MLINEFROMTEXT '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3, $5)); }
	| MPOINTFROMTEXT '(' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3)); }
	| MPOINTFROMTEXT '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3, $5)); }
	| MPOLYFROMTEXT '(' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3)); }
	| MPOLYFROMTEXT '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3, $5)); }
	| MULTIPOINT '(' expr_list ')'
	  { $$= GEOM_NEW(Item_func_spatial_collection(* $3,
                  Geometry::wkb_multipoint, Geometry::wkb_point)); }
 	| MULTIPOLYGON '(' expr_list ')'
	  { $$= GEOM_NEW(Item_func_spatial_collection(* $3,
                  Geometry::wkb_multipolygon, Geometry::wkb_polygon)); }
	| POINT_SYM '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_point($3,$5)); }
 	| POINTFROMTEXT '(' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3)); }
	| POINTFROMTEXT '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3, $5)); }
	| POLYFROMTEXT '(' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3)); }
	| POLYFROMTEXT '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3, $5)); }
	| POLYGON '(' expr_list ')'
	  { $$= GEOM_NEW(Item_func_spatial_collection(* $3,
	          Geometry::wkb_polygon, Geometry::wkb_linestring)); }
 	| GEOMCOLLFROMTEXT '(' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3)); }
	| GEOMCOLLFROMTEXT '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3, $5)); }
 	| LINEFROMTEXT '(' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3)); }
	| LINEFROMTEXT '(' expr ',' expr ')'
	  { $$= GEOM_NEW(Item_func_geometry_from_text($3, $5)); }
	;

fulltext_options:
        /* nothing */                   { $$= FT_NL;  }
        | WITH QUERY_SYM EXPANSION_SYM  { $$= FT_NL | FT_EXPAND; }
        | IN_SYM BOOLEAN_SYM MODE_SYM   { $$= FT_BOOL; }
        ;

udf_expr_list:
	/* empty */	 { $$= NULL; }
	| udf_expr_list2 { $$= $1;}
	;

udf_expr_list2:
	{ Select->expr_list.push_front(new List<Item>); }
	udf_expr_list3
	{ $$= Select->expr_list.pop(); }
	;

udf_expr_list3:
	udf_expr 
	  {
	    Select->expr_list.head()->push_back($1);
	  }
	| udf_expr_list3 ',' udf_expr 
	  {
	    Select->expr_list.head()->push_back($3);
	  }
	;

udf_expr:
	remember_name expr remember_end select_alias
	{
	  if ($4.str)
          {
            $2->is_autogenerated_name= FALSE;
	    $2->set_name($4.str, $4.length, system_charset_info);
          }
	  else
	    $2->set_name($1, (uint) ($3 - $1), YYTHD->charset());
	  $$= $2;
	}
	;

sum_expr:
	AVG_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_avg($3); }
	| AVG_SYM '(' DISTINCT in_sum_expr ')'
	  { $$=new Item_sum_avg_distinct($4); }
	| BIT_AND  '(' in_sum_expr ')'
	  { $$=new Item_sum_and($3); }
	| BIT_OR  '(' in_sum_expr ')'
	  { $$=new Item_sum_or($3); }
	| BIT_XOR  '(' in_sum_expr ')'
	  { $$=new Item_sum_xor($3); }
	| COUNT_SYM '(' opt_all '*' ')'
	  { $$=new Item_sum_count(new Item_int((int32) 0L,1)); }
	| COUNT_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_count($3); }
	| COUNT_SYM '(' DISTINCT
	  { Select->in_sum_expr++; }
	   expr_list
	  { Select->in_sum_expr--; }
	  ')'
	  { $$=new Item_sum_count_distinct(* $5); }
	| GROUP_UNIQUE_USERS '(' text_literal ',' NUM ',' NUM ',' in_sum_expr ')'
	  { $$= new Item_sum_unique_users($3,atoi($5.str),atoi($7.str),$9); }
	| MIN_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_min($3); }
/*
   According to ANSI SQL, DISTINCT is allowed and has
   no sence inside MIN and MAX grouping functions; so MIN|MAX(DISTINCT ...)
   is processed like an ordinary MIN | MAX()
 */
	| MIN_SYM '(' DISTINCT in_sum_expr ')'
	  { $$=new Item_sum_min($4); }
	| MAX_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_max($3); }
	| MAX_SYM '(' DISTINCT in_sum_expr ')'
	  { $$=new Item_sum_max($4); }
	| STD_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_std($3, 0); }
	| VARIANCE_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_variance($3, 0); }
	| STDDEV_SAMP_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_std($3, 1); }
	| VAR_SAMP_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_variance($3, 1); }
	| SUM_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_sum($3); }
	| SUM_SYM '(' DISTINCT in_sum_expr ')'
	  { $$=new Item_sum_sum_distinct($4); }
	| GROUP_CONCAT_SYM '(' opt_distinct
	  { Select->in_sum_expr++; }
	  expr_list opt_gorder_clause
	  opt_gconcat_separator
	 ')'
	  {
            SELECT_LEX *sel= Select;
	    sel->in_sum_expr--;
	    $$=new Item_func_group_concat(Lex->current_context(), $3, $5,
                                          sel->gorder_list, $7);
	    $5->empty();
	  };

opt_distinct:
    /* empty */ { $$ = 0; }
    |DISTINCT   { $$ = 1; };

opt_gconcat_separator:
    /* empty */        { $$ = new (YYTHD->mem_root) String(",",1,default_charset_info); }
    |SEPARATOR_SYM text_string  { $$ = $2; };


opt_gorder_clause:
	  /* empty */
	  {
            Select->gorder_list = NULL;
	  }
	| order_clause
          {
            SELECT_LEX *select= Select;
            select->gorder_list=
	      (SQL_LIST*) sql_memdup((char*) &select->order_list,
				     sizeof(st_sql_list));
	    select->order_list.empty();
	  };


in_sum_expr:
	opt_all
	{
	  LEX *lex= Lex;
	  if (lex->current_select->inc_in_sum_expr())
	  {
	    yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
	  }
	}
	expr
	{
	  Select->in_sum_expr--;
	  $$= $3;
	};

cast_type:
        BINARY opt_len		{ $$=ITEM_CAST_CHAR; Lex->charset= &my_charset_bin; Lex->dec= 0; }
        | CHAR_SYM opt_len opt_binary	{ $$=ITEM_CAST_CHAR; Lex->dec= 0; }
	| NCHAR_SYM opt_len	{ $$=ITEM_CAST_CHAR; Lex->charset= national_charset_info; Lex->dec=0; }
        | SIGNED_SYM		{ $$=ITEM_CAST_SIGNED_INT; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | SIGNED_SYM INT_SYM	{ $$=ITEM_CAST_SIGNED_INT; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | UNSIGNED		{ $$=ITEM_CAST_UNSIGNED_INT; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | UNSIGNED INT_SYM	{ $$=ITEM_CAST_UNSIGNED_INT; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | DATE_SYM		{ $$=ITEM_CAST_DATE; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | TIME_SYM		{ $$=ITEM_CAST_TIME; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | DATETIME		{ $$=ITEM_CAST_DATETIME; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | DECIMAL_SYM float_options { $$=ITEM_CAST_DECIMAL; Lex->charset= NULL; }
	;

expr_list:
	{ Select->expr_list.push_front(new List<Item>); }
	expr_list2
	{ $$= Select->expr_list.pop(); };

expr_list2:
	expr { Select->expr_list.head()->push_back($1); }
	| expr_list2 ',' expr { Select->expr_list.head()->push_back($3); };

ident_list_arg:
          ident_list          { $$= $1; }
        | '(' ident_list ')'  { $$= $2; };

ident_list:
        { Select->expr_list.push_front(new List<Item>); }
        ident_list2
        { $$= Select->expr_list.pop(); };

ident_list2:
        simple_ident { Select->expr_list.head()->push_back($1); }
        | ident_list2 ',' simple_ident { Select->expr_list.head()->push_back($3); };

opt_expr:
	/* empty */      { $$= NULL; }
	| expr           { $$= $1; };

opt_else:
	/* empty */    { $$= NULL; }
	| ELSE expr    { $$= $2; };

when_list:
        { Select->when_list.push_front(new List<Item>); }
	when_list2
	{ $$= Select->when_list.pop(); };

when_list2:
	expr THEN_SYM expr
	  {
	    SELECT_LEX *sel=Select;
	    sel->when_list.head()->push_back($1);
	    sel->when_list.head()->push_back($3);
	}
	| when_list2 WHEN_SYM expr THEN_SYM expr
	  {
	    SELECT_LEX *sel=Select;
	    sel->when_list.head()->push_back($3);
	    sel->when_list.head()->push_back($5);
	  };

/* Warning - may return NULL in case of incomplete SELECT */
table_ref:
        table_factor            { $$=$1; }
        | join_table            { $$=$1; }
          {
	    LEX *lex= Lex;
            if (!($$= lex->current_select->nest_last_join(lex->thd)))
              YYABORT;
          }
        ;

join_table_list:
	derived_table_list		{ YYERROR_UNLESS($$=$1); }
	;

/* Warning - may return NULL in case of incomplete SELECT */
derived_table_list:
        table_ref { $$=$1; }
        | derived_table_list ',' table_ref
          {
            YYERROR_UNLESS($1 && ($$=$3));
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
        table_ref %prec TABLE_REF_PRIORITY normal_join table_ref
          { YYERROR_UNLESS($1 && ($$=$3)); }
	| table_ref STRAIGHT_JOIN table_factor
	  { YYERROR_UNLESS($1 && ($$=$3)); $3->straight=1; }
	| table_ref normal_join table_ref
          ON
          {
            YYERROR_UNLESS($1 && ($$=$3));
            /* Change the current name resolution context to a local context. */
            if (push_new_name_resolution_context(YYTHD, $1, $3))
              YYABORT;
          }
          expr
	  {
            add_join_on($3,$6);
            Lex->pop_context();
          }
        | table_ref STRAIGHT_JOIN table_factor
          ON
          {
            YYERROR_UNLESS($1 && ($$=$3));
            /* Change the current name resolution context to a local context. */
            if (push_new_name_resolution_context(YYTHD, $1, $3))
              YYABORT;
          }
          expr
          {
            $3->straight=1;
            add_join_on($3,$6);
            Lex->pop_context();
          }
	| table_ref normal_join table_ref
	  USING
	  {
	    SELECT_LEX *sel= Select;
            YYERROR_UNLESS($1 && $3);
	  }
	  '(' using_list ')'
          { add_join_natural($1,$3,$7); $$=$3; }
	| table_ref NATURAL JOIN_SYM table_factor
	  {
            YYERROR_UNLESS($1 && ($$=$4));
            add_join_natural($1,$4,NULL);
          }

/* LEFT JOIN variants */
	| table_ref LEFT opt_outer JOIN_SYM table_ref
          ON
          {
            YYERROR_UNLESS($1 && $5);
            /* Change the current name resolution context to a local context. */
            if (push_new_name_resolution_context(YYTHD, $1, $5))
              YYABORT;
          }
          expr
	  {
            add_join_on($5,$8);
            Lex->pop_context();
            $5->outer_join|=JOIN_TYPE_LEFT;
            $$=$5;
          }
	| table_ref LEFT opt_outer JOIN_SYM table_factor
	  {
	    SELECT_LEX *sel= Select;
            YYERROR_UNLESS($1 && $5);
	  }
	  USING '(' using_list ')'
          { add_join_natural($1,$5,$9); $5->outer_join|=JOIN_TYPE_LEFT; $$=$5; }
	| table_ref NATURAL LEFT opt_outer JOIN_SYM table_factor
	  {
            YYERROR_UNLESS($1 && $6);
 	    add_join_natural($1,$6,NULL);
	    $6->outer_join|=JOIN_TYPE_LEFT;
	    $$=$6;
	  }

/* RIGHT JOIN variants */
	| table_ref RIGHT opt_outer JOIN_SYM table_ref
          ON
          {
            YYERROR_UNLESS($1 && $5);
            /* Change the current name resolution context to a local context. */
            if (push_new_name_resolution_context(YYTHD, $1, $5))
              YYABORT;
          }
          expr
          {
	    LEX *lex= Lex;
            if (!($$= lex->current_select->convert_right_join()))
              YYABORT;
            add_join_on($$, $8);
            Lex->pop_context();
          }
	| table_ref RIGHT opt_outer JOIN_SYM table_factor
	  {
	    SELECT_LEX *sel= Select;
            YYERROR_UNLESS($1 && $5);
	  }
	  USING '(' using_list ')'
          {
	    LEX *lex= Lex;
            if (!($$= lex->current_select->convert_right_join()))
              YYABORT;
            add_join_natural($$,$5,$9);
          }
	| table_ref NATURAL RIGHT opt_outer JOIN_SYM table_factor
	  {
            YYERROR_UNLESS($1 && $6);
	    add_join_natural($6,$1,NULL);
	    LEX *lex= Lex;
            if (!($$= lex->current_select->convert_right_join()))
              YYABORT;
	  };

normal_join:
	JOIN_SYM		{}
	| INNER_SYM JOIN_SYM	{}
	| CROSS JOIN_SYM	{}
	;

/* Warning - may return NULL in case of incomplete SELECT */
table_factor:
	{
	  SELECT_LEX *sel= Select;
	  sel->use_index_ptr=sel->ignore_index_ptr=0;
	  sel->table_join_options= 0;
	}
        table_ident opt_table_alias opt_key_definition
	{
	  LEX *lex= Lex;
	  SELECT_LEX *sel= lex->current_select;
	  if (!($$= sel->add_table_to_list(lex->thd, $2, $3,
					   sel->get_table_join_options(),
					   lex->lock_option,
					   sel->get_use_index(),
					   sel->get_ignore_index())))
	    YYABORT;
          sel->add_joined_table($$);
	}
	| '{' ident table_ref LEFT OUTER JOIN_SYM table_ref
          ON
          {
            /* Change the current name resolution context to a local context. */
            if (push_new_name_resolution_context(YYTHD, $3, $7))
              YYABORT;

          }
          expr '}'
	  {
	    LEX *lex= Lex;
            YYERROR_UNLESS($3 && $7);
            add_join_on($7,$10);
            Lex->pop_context();
            $7->outer_join|=JOIN_TYPE_LEFT;
            $$=$7;
            if (!($$= lex->current_select->nest_last_join(lex->thd)))
              YYABORT;
          }
	| select_derived_init get_select_lex select_derived2
          {
            LEX *lex= Lex;
            SELECT_LEX *sel= lex->current_select;
            if ($1)
            {
	      if (sel->set_braces(1))
	      {
	        yyerror(ER(ER_SYNTAX_ERROR));
	        YYABORT;
	      }
              /* select in braces, can't contain global parameters */
	      if (sel->master_unit()->fake_select_lex)
                sel->master_unit()->global_parameters=
                   sel->master_unit()->fake_select_lex;
            }
            if ($2->init_nested_join(lex->thd))
              YYABORT;
            $$= 0;
            /* incomplete derived tables return NULL, we must be
               nested in select_derived rule to be here. */
          }
	| '(' get_select_lex select_derived union_opt ')' opt_table_alias
	{
          /* Use $2 instead of Lex->current_select as derived table will
             alter value of Lex->current_select. */

          if (!($3 || $6) && $2->embedding &&
              !$2->embedding->nested_join->join_list.elements)
          {
            /* we have a derived table ($3 == NULL) but no alias,
               Since we are nested in further parentheses so we
               can pass NULL to the outer level parentheses
               Permits parsing of "((((select ...))) as xyz)" */
            $$= 0;
          }
          else
          if (!$3)
          {
            /* Handle case of derived table, alias may be NULL if there
               are no outer parentheses, add_table_to_list() will throw
               error in this case */
	    LEX *lex=Lex;
            SELECT_LEX *sel= lex->current_select;
	    SELECT_LEX_UNIT *unit= sel->master_unit();
	    lex->current_select= sel= unit->outer_select();
	    if (!($$= sel->
                  add_table_to_list(lex->thd, new Table_ident(unit), $6, 0,
				    TL_READ,(List<String> *)0,
	                            (List<String> *)0)))

	      YYABORT;
            sel->add_joined_table($$);
            lex->pop_context();
          }
	  else
          if ($4 || $6)
	  {
            /* simple nested joins cannot have aliases or unions */
            yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
	  }
          else
            $$= $3;
	}
        ;

/* handle contents of parentheses in join expression */
select_derived:
	  get_select_lex
	  {
            LEX *lex= Lex;
            if ($1->init_nested_join(lex->thd))
              YYABORT;
          }
          derived_table_list
          {
            LEX *lex= Lex;
            /* for normal joins, $3 != NULL and end_nested_join() != NULL,
               for derived tables, both must equal NULL */

            if (!($$= $1->end_nested_join(lex->thd)) && $3)
              YYABORT;
            if (!$3 && $$)
            {
	      yyerror(ER(ER_SYNTAX_ERROR));
	      YYABORT;
            }
          }
 	;

select_derived2:
        {
	  LEX *lex= Lex;
	  lex->derived_tables|= DERIVED_SUBQUERY;
          if (lex->sql_command == (int)SQLCOM_HA_READ ||
              lex->sql_command == (int)SQLCOM_KILL)
	  {
	    yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
	  }
	  if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE ||
              mysql_new_select(lex, 1))
	    YYABORT;
	  mysql_init_select(lex);
	  lex->current_select->linkage= DERIVED_TABLE_TYPE;
	  lex->current_select->parsing_place= SELECT_LIST;
	}
        select_options select_item_list
	{
	  Select->parsing_place= NO_MATTER;
	}
	opt_select_from
        ;

get_select_lex:
	/* Empty */ { $$= Select; }
        ;

select_derived_init:
          SELECT_SYM
          {
            LEX *lex= Lex;
            SELECT_LEX *sel= lex->current_select;
            TABLE_LIST *embedding;
            if (!sel->embedding || sel->end_nested_join(lex->thd))
	    {
              /* we are not in parentheses */
              yyerror(ER(ER_SYNTAX_ERROR));
	      YYABORT;
	    }
            embedding= Select->embedding;
            $$= embedding &&
                !embedding->nested_join->join_list.elements;
            /* return true if we are deeply nested */
          }
        ;

opt_outer:
	/* empty */	{}
	| OUTER		{};

opt_key_definition:
	/* empty */	{}
	| USE_SYM    key_usage_list
          {
	    SELECT_LEX *sel= Select;
	    sel->use_index= *$2;
	    sel->use_index_ptr= &sel->use_index;
	  }
	| FORCE_SYM key_usage_list
          {
	    SELECT_LEX *sel= Select;
	    sel->use_index= *$2;
	    sel->use_index_ptr= &sel->use_index;
	    sel->table_join_options|= TL_OPTION_FORCE_INDEX;
	  }
	| IGNORE_SYM key_usage_list
	  {
	    SELECT_LEX *sel= Select;
	    sel->ignore_index= *$2;
	    sel->ignore_index_ptr= &sel->ignore_index;
	  };

key_usage_list:
	key_or_index { Select->interval_list.empty(); }
        '(' key_list_or_empty ')'
        { $$= &Select->interval_list; }
	;

key_list_or_empty:
	/* empty */ 		{}
	| key_usage_list2	{}
	;

key_usage_list2:
	key_usage_list2 ',' ident
        { Select->
	    interval_list.push_back(new (YYTHD->mem_root) String((const char*) $3.str, $3.length,
				    system_charset_info)); }
	| ident
        { Select->
	    interval_list.push_back(new (YYTHD->mem_root) String((const char*) $1.str, $1.length,
				    system_charset_info)); }
	| PRIMARY_SYM
        { Select->
	    interval_list.push_back(new (YYTHD->mem_root) String("PRIMARY", 7,
				    system_charset_info)); };

using_list:
	ident
	  {
            if (!($$= new List<String>))
	      YYABORT;
            $$->push_back(new (YYTHD->mem_root)
                              String((const char *) $1.str, $1.length,
                                      system_charset_info));
	  }
	| using_list ',' ident
	  {
            $1->push_back(new (YYTHD->mem_root)
                              String((const char *) $3.str, $3.length,
                                      system_charset_info));
            $$= $1;
	  };

interval:
	interval_time_st	{}
	| DAY_HOUR_SYM		{ $$=INTERVAL_DAY_HOUR; }
	| DAY_MICROSECOND_SYM	{ $$=INTERVAL_DAY_MICROSECOND; }
	| DAY_MINUTE_SYM	{ $$=INTERVAL_DAY_MINUTE; }
	| DAY_SECOND_SYM	{ $$=INTERVAL_DAY_SECOND; }
	| HOUR_MICROSECOND_SYM	{ $$=INTERVAL_HOUR_MICROSECOND; }
	| HOUR_MINUTE_SYM	{ $$=INTERVAL_HOUR_MINUTE; }
	| HOUR_SECOND_SYM	{ $$=INTERVAL_HOUR_SECOND; }
	| MICROSECOND_SYM	{ $$=INTERVAL_MICROSECOND; }
	| MINUTE_MICROSECOND_SYM	{ $$=INTERVAL_MINUTE_MICROSECOND; }
	| MINUTE_SECOND_SYM	{ $$=INTERVAL_MINUTE_SECOND; }
	| SECOND_MICROSECOND_SYM	{ $$=INTERVAL_SECOND_MICROSECOND; }
	| YEAR_MONTH_SYM	{ $$=INTERVAL_YEAR_MONTH; };

interval_time_st:
	DAY_SYM			{ $$=INTERVAL_DAY; }
	| WEEK_SYM		{ $$=INTERVAL_WEEK; }
	| HOUR_SYM		{ $$=INTERVAL_HOUR; }
	| FRAC_SECOND_SYM	{ $$=INTERVAL_MICROSECOND; }
	| MINUTE_SYM		{ $$=INTERVAL_MINUTE; }
	| MONTH_SYM		{ $$=INTERVAL_MONTH; }
	| QUARTER_SYM		{ $$=INTERVAL_QUARTER; }
	| SECOND_SYM		{ $$=INTERVAL_SECOND; }
	| YEAR_SYM		{ $$=INTERVAL_YEAR; }
        ;

date_time_type:
          DATE_SYM              {$$=MYSQL_TIMESTAMP_DATE;}
        | TIME_SYM              {$$=MYSQL_TIMESTAMP_TIME;}
        | DATETIME              {$$=MYSQL_TIMESTAMP_DATETIME;}
        | TIMESTAMP             {$$=MYSQL_TIMESTAMP_DATETIME;}
        ;

table_alias:
	/* empty */
	| AS
	| EQ;

opt_table_alias:
	/* empty */		{ $$=0; }
	| table_alias ident
	  { $$= (LEX_STRING*) sql_memdup(&$2,sizeof(LEX_STRING)); };

opt_all:
	/* empty */
	| ALL
	;

where_clause:
	/* empty */  { Select->where= 0; }
	| WHERE
          {
            Select->parsing_place= IN_WHERE;
          }
          expr
	  {
            SELECT_LEX *select= Select;
	    select->where= $3;
            select->parsing_place= NO_MATTER;
	    if ($3)
	      $3->top_level_item();
	  }
 	;

having_clause:
	/* empty */
	| HAVING
	  {
	    Select->parsing_place= IN_HAVING;
          }
	  expr
	  {
	    SELECT_LEX *sel= Select;
	    sel->having= $3;
	    sel->parsing_place= NO_MATTER;
	    if ($3)
	      $3->top_level_item();
	  }
	;

opt_escape:
	ESCAPE_SYM simple_expr 
          {
            Lex->escape_used= TRUE;
            $$= $2;
          }
	| /* empty */
          {
            Lex->escape_used= FALSE;
            $$= ((YYTHD->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES) ?
		 new Item_string("", 0, &my_charset_latin1) :
                 new Item_string("\\", 1, &my_charset_latin1));
          }
        ;


/*
   group by statement in select
*/

group_clause:
	/* empty */
	| GROUP BY group_list olap_opt;

group_list:
	group_list ',' order_ident order_dir
	  { if (add_group_to_list(YYTHD, $3,(bool) $4)) YYABORT; }
	| order_ident order_dir
	  { if (add_group_to_list(YYTHD, $1,(bool) $2)) YYABORT; };

olap_opt:
	/* empty */ {}
	| WITH CUBE_SYM
          {
	    LEX *lex=Lex;
	    if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE)
	    {
	      my_error(ER_WRONG_USAGE, MYF(0), "WITH CUBE",
		       "global union parameters");
	      YYABORT;
	    }
	    lex->current_select->olap= CUBE_TYPE;
	    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "CUBE");
	    YYABORT;	/* To be deleted in 5.1 */
	  }
	| WITH ROLLUP_SYM
          {
	    LEX *lex= Lex;
	    if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE)
	    {
	      my_error(ER_WRONG_USAGE, MYF(0), "WITH ROLLUP",
		       "global union parameters");
	      YYABORT;
	    }
	    lex->current_select->olap= ROLLUP_TYPE;
	  }
	;

/*
   Order by statement in select
*/

opt_order_clause:
	/* empty */
	| order_clause;

order_clause:
	ORDER_SYM BY
        {
	  LEX *lex=Lex;
          SELECT_LEX *sel= lex->current_select;
          SELECT_LEX_UNIT *unit= sel-> master_unit();
	  if (sel->linkage != GLOBAL_OPTIONS_TYPE &&
	      sel->olap != UNSPECIFIED_OLAP_TYPE)
	  {
	    my_error(ER_WRONG_USAGE, MYF(0),
                     "CUBE/ROLLUP", "ORDER BY");
	    YYABORT;
	  }
          if (lex->sql_command != SQLCOM_ALTER_TABLE && !unit->fake_select_lex)
          {
            /*
              A query of the of the form (SELECT ...) ORDER BY order_list is
              executed in the same way as the query
              SELECT ... ORDER BY order_list
              unless the SELECT construct contains ORDER BY or LIMIT clauses.
              Otherwise we create a fake SELECT_LEX if it has not been created
              yet.
            */
            SELECT_LEX *first_sl= unit->first_select();
            if (!first_sl->next_select() &&
                (first_sl->order_list.elements || 
                 first_sl->select_limit) &&            
                unit->add_fake_select_lex(lex->thd))
              YYABORT;
          }
	} order_list;

order_list:
	order_list ',' order_ident order_dir
	  { if (add_order_to_list(YYTHD, $3,(bool) $4)) YYABORT; }
	| order_ident order_dir
	  { if (add_order_to_list(YYTHD, $1,(bool) $2)) YYABORT; };

order_dir:
	/* empty */ { $$ =  1; }
	| ASC  { $$ =1; }
	| DESC { $$ =0; };


opt_limit_clause_init:
	/* empty */
	{
	  LEX *lex= Lex;
	  SELECT_LEX *sel= lex->current_select;
          sel->offset_limit= 0;
          sel->select_limit= 0;
	}
	| limit_clause {}
	;

opt_limit_clause:
	/* empty */	{}
	| limit_clause	{}
	;

limit_clause:
	LIMIT limit_options {}
	;

limit_options:
	limit_option
	  {
            SELECT_LEX *sel= Select;
            sel->select_limit= $1;
            sel->offset_limit= 0;
	    sel->explicit_limit= 1;
	  }
	| limit_option ',' limit_option
	  {
	    SELECT_LEX *sel= Select;
	    sel->select_limit= $3;
	    sel->offset_limit= $1;
	    sel->explicit_limit= 1;
	  }
	| limit_option OFFSET_SYM limit_option
	  {
	    SELECT_LEX *sel= Select;
	    sel->select_limit= $1;
	    sel->offset_limit= $3;
	    sel->explicit_limit= 1;
	  }
	;
limit_option:
        param_marker
        | ULONGLONG_NUM { $$= new Item_uint($1.str, $1.length); }
        | LONG_NUM     { $$= new Item_uint($1.str, $1.length); }
        | NUM           { $$= new Item_uint($1.str, $1.length); }
        ;

delete_limit_clause:
	/* empty */
	{
	  LEX *lex=Lex;
	  lex->current_select->select_limit= 0;
	}
	| LIMIT limit_option
	{
	  SELECT_LEX *sel= Select;
	  sel->select_limit= $2;
	  sel->explicit_limit= 1;
	};

ulong_num:
          NUM           { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
	| HEX_NUM       { $$= (ulong) strtol($1.str, (char**) 0, 16); }
	| LONG_NUM      { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
	| ULONGLONG_NUM { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
        | DECIMAL_NUM   { int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
	| FLOAT_NUM	{ int error; $$= (ulong) my_strtoll10($1.str, (char**) 0, &error); }
	;

ulonglong_num:
	NUM	    { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
	| ULONGLONG_NUM { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
	| LONG_NUM  { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
        | DECIMAL_NUM  { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
	| FLOAT_NUM { int error; $$= (ulonglong) my_strtoll10($1.str, (char**) 0, &error); }
	;

procedure_clause:
	/* empty */
	| PROCEDURE ident			/* Procedure name */
	  {
	    LEX *lex=Lex;
	    if (&lex->select_lex != lex->current_select)
	    {
	      my_error(ER_WRONG_USAGE, MYF(0), "PROCEDURE", "subquery");
	      YYABORT;
	    }
	    lex->proc_list.elements=0;
	    lex->proc_list.first=0;
	    lex->proc_list.next= (byte**) &lex->proc_list.first;
	    if (add_proc_to_list(lex->thd, new Item_field(&lex->
                                                          current_select->
                                                          context,
                                                          NULL,NULL,$2.str)))
	      YYABORT;
	    Lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
	  }
	  '(' procedure_list ')';


procedure_list:
	/* empty */ {}
	| procedure_list2 {};

procedure_list2:
	procedure_list2 ',' procedure_item
	| procedure_item;

procedure_item:
	  remember_name expr
	  {
	    LEX *lex= Lex;
	    if (add_proc_to_list(lex->thd, $2))
	      YYABORT;
	    if (!$2->name)
	      $2->set_name($1,(uint) ((char*) lex->tok_end - $1),
                           YYTHD->charset());
	  }
          ;


select_var_list_init:
	   {
             LEX *lex=Lex;
	     if (!lex->describe && (!(lex->result= new select_dumpvar())))
	        YYABORT;
	   }
	   select_var_list
	   {}
           ;

select_var_list:
	   select_var_list ',' select_var_ident
	   | select_var_ident {}
           ;

select_var_ident:  
	   '@' ident_or_text
           {
             LEX *lex=Lex;
	     if (lex->result) 
	       ((select_dumpvar *)lex->result)->var_list.push_back( new my_var($2,0,0,(enum_field_types)0));
	     else
               /*
                 The parser won't create select_result instance only
	         if it's an EXPLAIN.
               */
               DBUG_ASSERT(lex->describe);
	   }
           | ident_or_text
           {
             LEX *lex=Lex;
	     sp_variable_t *t;

	     if (!lex->spcont || !(t=lex->spcont->find_variable(&$1)))
	     {
	       my_error(ER_SP_UNDECLARED_VAR, MYF(0), $1.str);
	       YYABORT;
	     }
	     if (lex->result)
             {
               my_var *var;
	       ((select_dumpvar *)lex->result)->
                 var_list.push_back(var= new my_var($1,1,t->offset,t->type));
#ifndef DBUG_OFF
	       if (var)
		 var->sp= lex->sphead;
#endif
             }
	     else
	     {
               /*
                 The parser won't create select_result instance only
	         if it's an EXPLAIN.
               */
               DBUG_ASSERT(lex->describe);
	     }
	   }
           ;

into:
        INTO OUTFILE TEXT_STRING_filesystem
	{
          LEX *lex= Lex;
          lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
          if (!(lex->exchange= new sql_exchange($3.str, 0)) ||
              !(lex->result= new select_export(lex->exchange)))
            YYABORT;
	}
	opt_field_term opt_line_term
	| INTO DUMPFILE TEXT_STRING_filesystem
	{
	  LEX *lex=Lex;
	  if (!lex->describe)
	  {
	    lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
	    if (!(lex->exchange= new sql_exchange($3.str,1)))
	      YYABORT;
	    if (!(lex->result= new select_dump(lex->exchange)))
	      YYABORT;
	  }
	}
        | INTO select_var_list_init
	{
	  Lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
	}
        ;

/*
  DO statement
*/

do:	DO_SYM
	{
	  LEX *lex=Lex;
	  lex->sql_command = SQLCOM_DO;
	  mysql_init_select(lex);
	}
	expr_list
	{
	  Lex->insert_list= $3;
	}
	;

/*
  Drop : delete tables or index or user
*/

drop:
	DROP opt_temporary table_or_tables if_exists table_list opt_restrict
	{
	  LEX *lex=Lex;
	  lex->sql_command = SQLCOM_DROP_TABLE;
	  lex->drop_temporary= $2;
	  lex->drop_if_exists= $4;
	}
	| DROP INDEX_SYM ident ON table_ident {}
	  {
	     LEX *lex=Lex;
	     lex->sql_command= SQLCOM_DROP_INDEX;
	     lex->alter_info.drop_list.empty();
	     lex->alter_info.drop_list.push_back(new Alter_drop(Alter_drop::KEY,
                                                                $3.str));
	     if (!lex->current_select->add_table_to_list(lex->thd, $5, NULL,
							TL_OPTION_UPDATING))
	      YYABORT;
	  }
	| DROP DATABASE if_exists ident
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_DROP_DB;
	    lex->drop_if_exists=$3;
	    lex->name=$4.str;
	 }
	| DROP FUNCTION_SYM if_exists sp_name
	  {
	    LEX *lex=Lex;
	    if (lex->sphead)
	    {
	      my_error(ER_SP_NO_DROP_SP, MYF(0), "FUNCTION");
	      YYABORT;
	    }
	    lex->sql_command = SQLCOM_DROP_FUNCTION;
	    lex->drop_if_exists= $3;
	    lex->spname= $4;
	  }
	| DROP PROCEDURE if_exists sp_name
	  {
	    LEX *lex=Lex;
	    if (lex->sphead)
	    {
	      my_error(ER_SP_NO_DROP_SP, MYF(0), "PROCEDURE");
	      YYABORT;
	    }
	    lex->sql_command = SQLCOM_DROP_PROCEDURE;
	    lex->drop_if_exists= $3;
	    lex->spname= $4;
	  }
	| DROP USER clear_privileges user_list
	  {
	    Lex->sql_command = SQLCOM_DROP_USER;
          }
	| DROP VIEW_SYM if_exists table_list opt_restrict
	  {
	    LEX *lex= Lex;
	    lex->sql_command= SQLCOM_DROP_VIEW;
	    lex->drop_if_exists= $3;
	  }
        | DROP TRIGGER_SYM sp_name
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_DROP_TRIGGER;
            lex->spname= $3;
          }
	;

table_list:
	table_name
	| table_list ',' table_name;

table_name:
	table_ident
	{
	  if (!Select->add_table_to_list(YYTHD, $1, NULL, TL_OPTION_UPDATING))
	    YYABORT;
	}
	;

if_exists:
	/* empty */ { $$= 0; }
	| IF EXISTS { $$= 1; }
	;

opt_temporary:
	/* empty */ { $$= 0; }
	| TEMPORARY { $$= 1; }
	;
/*
** Insert : add new data to table
*/

insert:
	INSERT
	{
	  LEX *lex= Lex;
	  lex->sql_command= SQLCOM_INSERT;
	  lex->duplicates= DUP_ERROR; 
	  mysql_init_select(lex);
	  /* for subselects */
          lex->lock_option= (using_update_log) ? TL_READ_NO_INSERT : TL_READ;
	} insert_lock_option
	opt_ignore insert2
	{
	  Select->set_lock_for_tables($3);
	  Lex->current_select= &Lex->select_lex;
	}
	insert_field_spec opt_insert_update
	{}
	;

replace:
	REPLACE
	{
	  LEX *lex=Lex;
	  lex->sql_command = SQLCOM_REPLACE;
	  lex->duplicates= DUP_REPLACE;
	  mysql_init_select(lex);
	}
	replace_lock_option insert2
	{
	  Select->set_lock_for_tables($3);
	  Lex->current_select= &Lex->select_lex;
	}
	insert_field_spec
	{}
	;

insert_lock_option:
	/* empty */
          {
#ifdef HAVE_QUERY_CACHE
            /*
              If it is SP we do not allow insert optimisation whan result of
              insert visible only after the table unlocking but everyone can
              read table.
            */
            $$= (Lex->sphead ? TL_WRITE :TL_WRITE_CONCURRENT_INSERT);
#else
            $$= TL_WRITE_CONCURRENT_INSERT;
#endif
          }
	| LOW_PRIORITY	{ $$= TL_WRITE_LOW_PRIORITY; }
	| DELAYED_SYM	{ $$= TL_WRITE_DELAYED; }
	| HIGH_PRIORITY { $$= TL_WRITE; }
	;

replace_lock_option:
	opt_low_priority { $$= $1; }
	| DELAYED_SYM	 { $$= TL_WRITE_DELAYED; };

insert2:
	INTO insert_table {}
	| insert_table {};

insert_table:
	table_name
	{
	  LEX *lex=Lex;
	  lex->field_list.empty();
	  lex->many_values.empty();
	  lex->insert_list=0;
	};

insert_field_spec:
	insert_values {}
	| '(' ')' insert_values {}
	| '(' fields ')' insert_values {}
	| SET
	  {
	    LEX *lex=Lex;
	    if (!(lex->insert_list = new List_item) ||
		lex->many_values.push_back(lex->insert_list))
	      YYABORT;
	   }
	   ident_eq_list;

fields:
	fields ',' insert_ident { Lex->field_list.push_back($3); }
	| insert_ident		{ Lex->field_list.push_back($1); };

insert_values:
	VALUES	values_list  {}
	| VALUE_SYM values_list  {}
	|     create_select     { Select->set_braces(0);} union_clause {}
	| '(' create_select ')' { Select->set_braces(1);} union_opt {}
        ;

values_list:
	values_list ','  no_braces
	| no_braces;

ident_eq_list:
	ident_eq_list ',' ident_eq_value
	|
	ident_eq_value;

ident_eq_value:
	simple_ident_nospvar equal expr_or_default
	 {
	  LEX *lex=Lex;
	  if (lex->field_list.push_back($1) ||
	      lex->insert_list->push_back($3))
	    YYABORT;
	 };

equal:	EQ		{}
	| SET_VAR	{}
	;

opt_equal:
	/* empty */	{}
	| equal		{}
	;

no_braces:
	 '('
	 {
	    if (!(Lex->insert_list = new List_item))
	      YYABORT;
	 }
	 opt_values ')'
	 {
	  LEX *lex=Lex;
	  if (lex->many_values.push_back(lex->insert_list))
	    YYABORT;
	 };

opt_values:
	/* empty */ {}
	| values;

values:
	values ','  expr_or_default
	{
	  if (Lex->insert_list->push_back($3))
	    YYABORT;
	}
	| expr_or_default
	  {
	    if (Lex->insert_list->push_back($1))
	      YYABORT;
	  }
	;

expr_or_default:
	expr	  { $$= $1;}
	| DEFAULT {$$= new Item_default_value(Lex->current_context()); }
	;

opt_insert_update:
        /* empty */
        | ON DUPLICATE_SYM	{ Lex->duplicates= DUP_UPDATE; }
          KEY_SYM UPDATE_SYM insert_update_list
        ;

/* Update rows in a table */

update:
	UPDATE_SYM
	{
	  LEX *lex= Lex;
	  mysql_init_select(lex);
          lex->sql_command= SQLCOM_UPDATE;
	  lex->lock_option= TL_UNLOCK; 	/* Will be set later */
	  lex->duplicates= DUP_ERROR; 
        }
        opt_low_priority opt_ignore join_table_list
	SET update_list
	{
	  LEX *lex= Lex;
          if (lex->select_lex.table_list.elements > 1)
            lex->sql_command= SQLCOM_UPDATE_MULTI;
	  else if (lex->select_lex.get_table_list()->derived)
	  {
	    /* it is single table update and it is update of derived table */
	    my_error(ER_NON_UPDATABLE_TABLE, MYF(0),
                     lex->select_lex.get_table_list()->alias, "UPDATE");
	    YYABORT;
	  }
          /*
            In case of multi-update setting write lock for all tables may
            be too pessimistic. We will decrease lock level if possible in
            mysql_multi_update().
          */
          Select->set_lock_for_tables($3);
	}
	where_clause opt_order_clause delete_limit_clause {}
	;

update_list:
	update_list ',' update_elem
	| update_elem;

update_elem:
	simple_ident_nospvar equal expr_or_default
	{
	  if (add_item_to_list(YYTHD, $1) || add_value_to_list(YYTHD, $3))
	    YYABORT;
	};

insert_update_list:
	insert_update_list ',' insert_update_elem
	| insert_update_elem;

insert_update_elem:
	simple_ident_nospvar equal expr_or_default
	  {
	  LEX *lex= Lex;
	  if (lex->update_list.push_back($1) || 
	      lex->value_list.push_back($3))
	      YYABORT;
	  };

opt_low_priority:
	/* empty */	{ $$= YYTHD->update_lock_default; }
	| LOW_PRIORITY	{ $$= TL_WRITE_LOW_PRIORITY; };

/* Delete rows from a table */

delete:
	DELETE_SYM
	{
	  LEX *lex= Lex;
	  lex->sql_command= SQLCOM_DELETE;
	  mysql_init_select(lex);
	  lex->lock_option= lex->thd->update_lock_default;
	  lex->ignore= 0;
	  lex->select_lex.init_order();
	}
	opt_delete_options single_multi {}
	;

single_multi:
 	FROM table_ident
	{
	  if (!Select->add_table_to_list(YYTHD, $2, NULL, TL_OPTION_UPDATING,
					 Lex->lock_option))
	    YYABORT;
	}
	where_clause opt_order_clause
	delete_limit_clause {}
	| table_wild_list
	  { mysql_init_multi_delete(Lex); }
          FROM join_table_list where_clause
          { 
            if (multi_delete_set_locks_and_link_aux_tables(Lex))
              YYABORT;
          }
	| FROM table_wild_list
	  { mysql_init_multi_delete(Lex); }
	  USING join_table_list where_clause
          { 
            if (multi_delete_set_locks_and_link_aux_tables(Lex))
              YYABORT;
          }
	;

table_wild_list:
	  table_wild_one {}
	  | table_wild_list ',' table_wild_one {};

table_wild_one:
	ident opt_wild opt_table_alias
	{
	  if (!Select->add_table_to_list(YYTHD, new Table_ident($1), $3,
					 TL_OPTION_UPDATING, Lex->lock_option))
	    YYABORT;
        }
	| ident '.' ident opt_wild opt_table_alias
	  {
	    if (!Select->add_table_to_list(YYTHD,
					   new Table_ident(YYTHD, $1, $3, 0),
					   $5, TL_OPTION_UPDATING,
					   Lex->lock_option))
	      YYABORT;
	  }
	;

opt_wild:
	/* empty */	{}
	| '.' '*'	{};


opt_delete_options:
	/* empty */	{}
	| opt_delete_option opt_delete_options {};

opt_delete_option:
	QUICK		{ Select->options|= OPTION_QUICK; }
	| LOW_PRIORITY	{ Lex->lock_option= TL_WRITE_LOW_PRIORITY; }
	| IGNORE_SYM	{ Lex->ignore= 1; };

truncate:
	TRUNCATE_SYM opt_table_sym table_name
	{
	  LEX* lex= Lex;
	  lex->sql_command= SQLCOM_TRUNCATE;
	  lex->select_lex.options= 0;
          lex->select_lex.sql_cache= SELECT_LEX::SQL_CACHE_UNSPECIFIED;
	  lex->select_lex.init_order();
	}
	;

opt_table_sym:
	/* empty */
	| TABLE_SYM;

/* Show things */

show:	SHOW
	{
	  LEX *lex=Lex;
	  lex->wild=0;
          lex->lock_option= TL_READ;
          mysql_init_select(lex);
          lex->current_select->parsing_place= SELECT_LIST;
	  bzero((char*) &lex->create_info,sizeof(lex->create_info));
	}
	show_param
	{}
	;

show_param:
         DATABASES wild_and_where
         {
           LEX *lex= Lex;
           lex->sql_command= SQLCOM_SELECT;
           lex->orig_sql_command= SQLCOM_SHOW_DATABASES;
           if (prepare_schema_table(YYTHD, lex, 0, SCH_SCHEMATA))
             YYABORT;
         }
         | opt_full TABLES opt_db wild_and_where
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SELECT;
             lex->orig_sql_command= SQLCOM_SHOW_TABLES;
             lex->select_lex.db= $3;
             if (prepare_schema_table(YYTHD, lex, 0, SCH_TABLE_NAMES))
               YYABORT;
           }
         | opt_full TRIGGERS_SYM opt_db wild_and_where
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SELECT;
             lex->orig_sql_command= SQLCOM_SHOW_TRIGGERS;
             lex->select_lex.db= $3;
             if (prepare_schema_table(YYTHD, lex, 0, SCH_TRIGGERS))
               YYABORT;
           }
         | TABLE_SYM STATUS_SYM opt_db wild_and_where
           {
             LEX *lex= Lex;
             lex->sql_command= SQLCOM_SELECT;
             lex->orig_sql_command= SQLCOM_SHOW_TABLE_STATUS;
             lex->select_lex.db= $3;
             if (prepare_schema_table(YYTHD, lex, 0, SCH_TABLES))
               YYABORT;
           }
        | OPEN_SYM TABLES opt_db wild_and_where
	  {
	    LEX *lex= Lex;
            lex->sql_command= SQLCOM_SELECT;
            lex->orig_sql_command= SQLCOM_SHOW_OPEN_TABLES;
	    lex->select_lex.db= $3;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_OPEN_TABLES))
              YYABORT;
	  }
	| ENGINE_SYM storage_engines 
	  { Lex->create_info.db_type= $2; }
	  show_engine_param
	| opt_full COLUMNS from_or_in table_ident opt_db wild_and_where
	  {
 	    LEX *lex= Lex;
	    lex->sql_command= SQLCOM_SELECT;
	    lex->orig_sql_command= SQLCOM_SHOW_FIELDS;
	    if ($5)
	      $4->change_db($5);
	    if (prepare_schema_table(YYTHD, lex, $4, SCH_COLUMNS))
	      YYABORT;
	  }
        | NEW_SYM MASTER_SYM FOR_SYM SLAVE WITH MASTER_LOG_FILE_SYM EQ
	  TEXT_STRING_sys AND_SYM MASTER_LOG_POS_SYM EQ ulonglong_num
	  AND_SYM MASTER_SERVER_ID_SYM EQ
	ulong_num
          {
	    Lex->sql_command = SQLCOM_SHOW_NEW_MASTER;
	    Lex->mi.log_file_name = $8.str;
	    Lex->mi.pos = $12;
	    Lex->mi.server_id = $16;
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
          } opt_limit_clause_init
        | keys_or_index from_or_in table_ident opt_db where_clause
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SELECT;
            lex->orig_sql_command= SQLCOM_SHOW_KEYS;
	    if ($4)
	      $3->change_db($4);
            if (prepare_schema_table(YYTHD, lex, $3, SCH_STATISTICS))
              YYABORT;
	  }
	| COLUMN_SYM TYPES_SYM
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_COLUMN_TYPES;
	  }
	| TABLE_SYM TYPES_SYM
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_STORAGE_ENGINES;
	    WARN_DEPRECATED("SHOW TABLE TYPES", "SHOW [STORAGE] ENGINES");
	  }
	| opt_storage ENGINES_SYM
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_STORAGE_ENGINES;
	  }
	| PRIVILEGES
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_PRIVILEGES;
	  }
        | COUNT_SYM '(' '*' ')' WARNINGS
          { (void) create_select_for_variable("warning_count"); }
        | COUNT_SYM '(' '*' ')' ERRORS
	  { (void) create_select_for_variable("error_count"); }
        | WARNINGS opt_limit_clause_init
          { Lex->sql_command = SQLCOM_SHOW_WARNS;}
        | ERRORS opt_limit_clause_init
          { Lex->sql_command = SQLCOM_SHOW_ERRORS;}
        | opt_var_type STATUS_SYM wild_and_where
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SELECT;
            lex->orig_sql_command= SQLCOM_SHOW_STATUS;
            lex->option_type= $1;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_STATUS))
              YYABORT;
	  }	
        | INNOBASE_SYM STATUS_SYM
          { Lex->sql_command = SQLCOM_SHOW_INNODB_STATUS; WARN_DEPRECATED("SHOW INNODB STATUS", "SHOW ENGINE INNODB STATUS"); }
        | MUTEX_SYM STATUS_SYM
          { Lex->sql_command = SQLCOM_SHOW_MUTEX_STATUS; }
	| opt_full PROCESSLIST_SYM
	  { Lex->sql_command= SQLCOM_SHOW_PROCESSLIST;}
        | opt_var_type  VARIABLES wild_and_where
	  {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SELECT;
            lex->orig_sql_command= SQLCOM_SHOW_VARIABLES;
            lex->option_type= $1;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_VARIABLES))
              YYABORT;
	  }
        | charset wild_and_where
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SELECT;
            lex->orig_sql_command= SQLCOM_SHOW_CHARSETS;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_CHARSETS))
              YYABORT;
          }
        | COLLATION_SYM wild_and_where
          {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SELECT;
            lex->orig_sql_command= SQLCOM_SHOW_COLLATIONS;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_COLLATIONS))
              YYABORT;
          }
	| BERKELEY_DB_SYM LOGS_SYM
	  { Lex->sql_command= SQLCOM_SHOW_LOGS; WARN_DEPRECATED("SHOW BDB LOGS", "SHOW ENGINE BDB LOGS"); }
	| LOGS_SYM
	  { Lex->sql_command= SQLCOM_SHOW_LOGS; WARN_DEPRECATED("SHOW LOGS", "SHOW ENGINE BDB LOGS"); }
	| GRANTS
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_GRANTS;
	    LEX_USER *curr_user;
            if (!(curr_user= (LEX_USER*) lex->thd->alloc(sizeof(st_lex_user))))
              YYABORT;
            bzero(curr_user, sizeof(st_lex_user));
	    lex->grant_user= curr_user;
	  }
	| GRANTS FOR_SYM user
	  {
	    LEX *lex=Lex;
	    lex->sql_command= SQLCOM_SHOW_GRANTS;
	    lex->grant_user=$3;
	    lex->grant_user->password=null_lex_str;
	  }
	| CREATE DATABASE opt_if_not_exists ident
	  {
	    Lex->sql_command=SQLCOM_SHOW_CREATE_DB;
	    Lex->create_info.options=$3;
	    Lex->name=$4.str;
	  }
        | CREATE TABLE_SYM table_ident
          {
            LEX *lex= Lex;
	    lex->sql_command = SQLCOM_SHOW_CREATE;
	    if (!lex->select_lex.add_table_to_list(YYTHD, $3, NULL,0))
	      YYABORT;
            lex->only_view= 0;
	  }
        | CREATE VIEW_SYM table_ident
          {
            LEX *lex= Lex;
	    lex->sql_command = SQLCOM_SHOW_CREATE;
	    if (!lex->select_lex.add_table_to_list(YYTHD, $3, NULL, 0))
	      YYABORT;
            lex->only_view= 1;
	  }
        | MASTER_SYM STATUS_SYM
          {
	    Lex->sql_command = SQLCOM_SHOW_MASTER_STAT;
          }
        | SLAVE STATUS_SYM
          {
	    Lex->sql_command = SQLCOM_SHOW_SLAVE_STAT;
          }
	| CREATE PROCEDURE sp_name
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
	| PROCEDURE STATUS_SYM wild_and_where
	  {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SELECT;
            lex->orig_sql_command= SQLCOM_SHOW_STATUS_PROC;
	    if (!sp_add_to_query_tables(YYTHD, lex, "mysql", "proc", TL_READ))
	      YYABORT;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_PROCEDURES))
              YYABORT;
	  }
	| FUNCTION_SYM STATUS_SYM wild_and_where
	  {
            LEX *lex= Lex;
            lex->sql_command= SQLCOM_SELECT;
            lex->orig_sql_command= SQLCOM_SHOW_STATUS_FUNC;
	    if (!sp_add_to_query_tables(YYTHD, lex, "mysql", "proc", TL_READ))
	      YYABORT;
            if (prepare_schema_table(YYTHD, lex, 0, SCH_PROCEDURES))
              YYABORT;
	  }
        | PROCEDURE CODE_SYM sp_name
          {
#ifdef DBUG_OFF
            yyerror(ER(ER_SYNTAX_ERROR));
            YYABORT;
#else
            Lex->sql_command= SQLCOM_SHOW_PROC_CODE;
	    Lex->spname= $3;
#endif
          }
        | FUNCTION_SYM CODE_SYM sp_name
          {
#ifdef DBUG_OFF
            yyerror(ER(ER_SYNTAX_ERROR));
            YYABORT;
#else
            Lex->sql_command= SQLCOM_SHOW_FUNC_CODE;
	    Lex->spname= $3;
#endif
          }
        ;

show_engine_param:
	STATUS_SYM
	  {
	    switch (Lex->create_info.db_type) {
	    case DB_TYPE_NDBCLUSTER:
	      Lex->sql_command = SQLCOM_SHOW_NDBCLUSTER_STATUS;
	      break;
	    case DB_TYPE_INNODB:
	      Lex->sql_command = SQLCOM_SHOW_INNODB_STATUS;
	      break;
	    default:
	      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "STATUS");
	      YYABORT;
	    }
	  }
	| LOGS_SYM
	  {
	    switch (Lex->create_info.db_type) {
	    case DB_TYPE_BERKELEY_DB:
	      Lex->sql_command = SQLCOM_SHOW_LOGS;
	      break;
	    default:
	      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "LOGS");
	      YYABORT;
	    }
	  };

master_or_binary:
	MASTER_SYM
	| BINARY;

opt_storage:
	/* empty */
	| STORAGE_SYM;

opt_db:
	/* empty */  { $$= 0; }
	| from_or_in ident { $$= $2.str; };

opt_full:
	/* empty */ { Lex->verbose=0; }
	| FULL	    { Lex->verbose=1; };

from_or_in:
	FROM
	| IN_SYM;

binlog_in:
	/* empty */ { Lex->mi.log_file_name = 0; }
        | IN_SYM TEXT_STRING_sys { Lex->mi.log_file_name = $2.str; };

binlog_from:
	/* empty */ { Lex->mi.pos = 4; /* skip magic number */ }
        | FROM ulonglong_num { Lex->mi.pos = $2; };

wild_and_where:
      /* empty */
      | LIKE TEXT_STRING_sys
	{ Lex->wild=  new (YYTHD->mem_root) String($2.str, $2.length,
                                                   system_charset_info); }
      | WHERE expr
        {
          Select->where= $2;
          if ($2)
            $2->top_level_item();
        }
      ;


/* A Oracle compatible synonym for show */
describe:
	describe_command table_ident
	{
          LEX *lex= Lex;
          lex->lock_option= TL_READ;
          mysql_init_select(lex);
          lex->current_select->parsing_place= SELECT_LIST;
          lex->sql_command= SQLCOM_SELECT;
          lex->orig_sql_command= SQLCOM_SHOW_FIELDS;
          lex->select_lex.db= 0;
          lex->verbose= 0;
          if (prepare_schema_table(YYTHD, lex, $2, SCH_COLUMNS))
	    YYABORT;
	}
	opt_describe_column {}
	| describe_command opt_extended_describe
	  { Lex->describe|= DESCRIBE_NORMAL; }
	  select
          {
	    LEX *lex=Lex;
	    lex->select_lex.options|= SELECT_DESCRIBE;
	  }
	;

describe_command:
	DESC
	| DESCRIBE;

opt_extended_describe:
	/* empty */ {}
	| EXTENDED_SYM { Lex->describe|= DESCRIBE_EXTENDED; }
	;

opt_describe_column:
	/* empty */	{}
	| text_string	{ Lex->wild= $1; }
	| ident
	  { Lex->wild= new (YYTHD->mem_root) String((const char*) $1.str,$1.length,system_charset_info); };


/* flush things */

flush:
	FLUSH_SYM opt_no_write_to_binlog
	{
	  LEX *lex=Lex;
	  if (lex->sphead && lex->sphead->m_type != TYPE_ENUM_PROCEDURE)
	  {
            /*
              Note that both FLUSH TABLES and FLUSH PRIVILEGES will break
              execution in prelocked mode. So it is better to disable
              FLUSH in stored functions and triggers completely.
            */
            my_error(ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0), "FLUSH");
	    YYABORT;
	  }
	  lex->sql_command= SQLCOM_FLUSH; lex->type=0;
          lex->no_write_to_binlog= $2;
	}
	flush_options
	{}
	;

flush_options:
	flush_options ',' flush_option
	| flush_option;

flush_option:
	table_or_tables	{ Lex->type|= REFRESH_TABLES; } opt_table_list {}
	| TABLES WITH READ_SYM LOCK_SYM { Lex->type|= REFRESH_TABLES | REFRESH_READ_LOCK; }
	| QUERY_SYM CACHE_SYM { Lex->type|= REFRESH_QUERY_CACHE_FREE; }
	| HOSTS_SYM	{ Lex->type|= REFRESH_HOSTS; }
	| PRIVILEGES	{ Lex->type|= REFRESH_GRANT; }
	| LOGS_SYM	{ Lex->type|= REFRESH_LOG; }
	| STATUS_SYM	{ Lex->type|= REFRESH_STATUS; }
        | SLAVE         { Lex->type|= REFRESH_SLAVE; }
        | MASTER_SYM    { Lex->type|= REFRESH_MASTER; }
	| DES_KEY_FILE	{ Lex->type|= REFRESH_DES_KEY_FILE; }
 	| RESOURCES     { Lex->type|= REFRESH_USER_RESOURCES; };

opt_table_list:
	/* empty */  {;}
	| table_list {;};

reset:
	RESET_SYM
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_RESET; lex->type=0;
	} reset_options
	{}
	;

reset_options:
	reset_options ',' reset_option
	| reset_option;

reset_option:
        SLAVE                 { Lex->type|= REFRESH_SLAVE; }
        | MASTER_SYM          { Lex->type|= REFRESH_MASTER; }
	| QUERY_SYM CACHE_SYM { Lex->type|= REFRESH_QUERY_CACHE;};

purge:
	PURGE
	{
	  LEX *lex=Lex;
	  lex->type=0;
	} purge_options
	{}
	;

purge_options:
	master_or_binary LOGS_SYM purge_option
	;

purge_option:
        TO_SYM TEXT_STRING_sys
        {
	   Lex->sql_command = SQLCOM_PURGE;
	   Lex->to_log = $2.str;
        }
	| BEFORE_SYM expr
	{
	  LEX *lex= Lex;
	  lex->value_list.empty();
	  lex->value_list.push_front($2);
	  lex->sql_command= SQLCOM_PURGE_BEFORE;
	}
	;

/* kill threads */

kill:
	KILL_SYM { Lex->sql_command= SQLCOM_KILL; } kill_option expr
	{
	  LEX *lex=Lex;
	  lex->value_list.empty();
	  lex->value_list.push_front($4);
	};

kill_option:
	/* empty */	 { Lex->type= 0; }
	| CONNECTION_SYM { Lex->type= 0; }
	| QUERY_SYM      { Lex->type= ONLY_KILL_QUERY; }
        ;

/* change database */

use:	USE_SYM ident
	{
	  LEX *lex=Lex;
	  lex->sql_command=SQLCOM_CHANGE_DB;
	  lex->select_lex.db= $2.str;
	};

/* import, export of files */

load:   LOAD DATA_SYM
        {
          LEX *lex=Lex;
	  if (lex->sphead)
	  {
	    my_error(ER_SP_BADSTATEMENT, MYF(0), "LOAD DATA");
	    YYABORT;
	  }
          lex->fname_start= lex->ptr;
        }
        load_data
        {}
        |
        LOAD TABLE_SYM table_ident FROM MASTER_SYM
        {
	  LEX *lex=Lex;
	  if (lex->sphead)
	  {
	    my_error(ER_SP_BADSTATEMENT, MYF(0), "LOAD TABLE");
	    YYABORT;
	  }
          lex->sql_command = SQLCOM_LOAD_MASTER_TABLE;
          if (!Select->add_table_to_list(YYTHD, $3, NULL, TL_OPTION_UPDATING))
            YYABORT;
        };

load_data:
	load_data_lock opt_local INFILE TEXT_STRING_filesystem
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_LOAD;
	  lex->lock_option= $1;
	  lex->local_file=  $2;
	  lex->duplicates= DUP_ERROR;
	  lex->ignore= 0;
	  if (!(lex->exchange= new sql_exchange($4.str, 0)))
	    YYABORT;
        }
        opt_duplicate INTO
        {
	  LEX *lex=Lex;
	  lex->fname_end= lex->ptr;
	}
        TABLE_SYM table_ident
        {
          LEX *lex=Lex;
          if (!Select->add_table_to_list(YYTHD, $10, NULL, TL_OPTION_UPDATING,
                                         lex->lock_option))
            YYABORT;
          lex->field_list.empty();
          lex->update_list.empty();
          lex->value_list.empty();
        }
        opt_field_term opt_line_term opt_ignore_lines opt_field_or_var_spec
        opt_load_data_set_spec
        {}
        |
	FROM MASTER_SYM
        {
	  Lex->sql_command = SQLCOM_LOAD_MASTER_DATA;
        };

opt_local:
	/* empty */	{ $$=0;}
	| LOCAL_SYM	{ $$=1;};

load_data_lock:
	/* empty */	{ $$= YYTHD->update_lock_default; }
	| CONCURRENT
          {
#ifdef HAVE_QUERY_CACHE
            /*
              Ignore this option in SP to avoid problem with query cache
            */
            if (Lex->sphead != 0)
#endif
              $$= TL_WRITE_CONCURRENT_INSERT;
          }
	| LOW_PRIORITY	{ $$= TL_WRITE_LOW_PRIORITY; };


opt_duplicate:
	/* empty */	{ Lex->duplicates=DUP_ERROR; }
	| REPLACE	{ Lex->duplicates=DUP_REPLACE; }
	| IGNORE_SYM	{ Lex->ignore= 1; };

opt_field_term:
	/* empty */
	| COLUMNS field_term_list;

field_term_list:
	field_term_list field_term
	| field_term;

field_term:
	TERMINATED BY text_string 
          {
            DBUG_ASSERT(Lex->exchange != 0);
            Lex->exchange->field_term= $3;
          }
	| OPTIONALLY ENCLOSED BY text_string
	  {
            LEX *lex= Lex;
            DBUG_ASSERT(lex->exchange != 0);
            lex->exchange->enclosed= $4;
            lex->exchange->opt_enclosed= 1;
	  }
        | ENCLOSED BY text_string
          {
            DBUG_ASSERT(Lex->exchange != 0);
            Lex->exchange->enclosed= $3;
          }
        | ESCAPED BY text_string
          {
            DBUG_ASSERT(Lex->exchange != 0);
            Lex->exchange->escaped= $3;
          };

opt_line_term:
	/* empty */
	| LINES line_term_list;

line_term_list:
	line_term_list line_term
	| line_term;

line_term:
        TERMINATED BY text_string
          {
            DBUG_ASSERT(Lex->exchange != 0);
            Lex->exchange->line_term= $3;
          }
        | STARTING BY text_string
          {
            DBUG_ASSERT(Lex->exchange != 0);
            Lex->exchange->line_start= $3;
          };

opt_ignore_lines:
	/* empty */
        | IGNORE_SYM NUM LINES
          {
            DBUG_ASSERT(Lex->exchange != 0);
            Lex->exchange->skip_lines= atol($2.str);
          };

opt_field_or_var_spec:
	/* empty */	          { }
	| '(' fields_or_vars ')'  { }
	| '(' ')'	          { };

fields_or_vars:
        fields_or_vars ',' field_or_var
          { Lex->field_list.push_back($3); }
        | field_or_var
          { Lex->field_list.push_back($1); }
        ;

field_or_var:
        simple_ident_nospvar {$$= $1;}
        | '@' ident_or_text
          { $$= new Item_user_var_as_out_param($2); }
        ;

opt_load_data_set_spec:
        /* empty */           { }
        | SET insert_update_list  { };


/* Common definitions */

text_literal:
	TEXT_STRING_literal
	{
	  THD *thd= YYTHD;
	  $$ = new Item_string($1.str,$1.length,thd->variables.collation_connection);
	}
	| NCHAR_STRING
	{ $$=  new Item_string($1.str,$1.length,national_charset_info); }
	| UNDERSCORE_CHARSET TEXT_STRING
	  { $$ = new Item_string($2.str,$2.length,Lex->charset); }
	| text_literal TEXT_STRING_literal
	  { ((Item_string*) $1)->append($2.str,$2.length); }
	;

text_string:
	TEXT_STRING_literal
	{ $$=  new (YYTHD->mem_root) String($1.str,$1.length,YYTHD->variables.collation_connection); }
	| HEX_NUM
	  {
	    Item *tmp= new Item_hex_string($1.str, $1.length);
	    /*
	      it is OK only emulate fix_fields, because we need only
              value of constant
	    */
	    $$= tmp ?
	      tmp->quick_fix_field(), tmp->val_str((String*) 0) :
	      (String*) 0;
	  }
        | BIN_NUM
          {
	    Item *tmp= new Item_bin_string($1.str, $1.length);
	    /*
	      it is OK only emulate fix_fields, because we need only
              value of constant
	    */
	    $$= tmp ? tmp->quick_fix_field(), tmp->val_str((String*) 0) :
		      (String*) 0;
          }
	;

param_marker:
        PARAM_MARKER
        {
          THD *thd=YYTHD;
	  LEX *lex= thd->lex;
          Item_param *item= new Item_param((uint) (lex->tok_start -
                                                   (uchar *) thd->query));
          if (!($$= item) || lex->param_list.push_back(item))
          {
            my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
            YYABORT;
          }
        }
	;

signed_literal:
	literal		{ $$ = $1; }
	| '+' NUM_literal { $$ = $2; }
	| '-' NUM_literal
	  {
	    $2->max_length++;
	    $$= $2->neg();
	  }
	;


literal:
	text_literal	{ $$ =	$1; }
	| NUM_literal	{ $$ = $1; }
	| NULL_SYM	{ $$ =	new Item_null();
			  Lex->next_state=MY_LEX_OPERATOR_OR_IDENT;}
	| FALSE_SYM	{ $$= new Item_int((char*) "FALSE",0,1); }
	| TRUE_SYM	{ $$= new Item_int((char*) "TRUE",1,1); }
	| HEX_NUM	{ $$ =	new Item_hex_string($1.str, $1.length);}
	| BIN_NUM	{ $$= new Item_bin_string($1.str, $1.length); }
	| UNDERSCORE_CHARSET HEX_NUM
	  {
	    Item *tmp= new Item_hex_string($2.str, $2.length);
	    /*
	      it is OK only emulate fix_fieds, because we need only
              value of constant
	    */
	    String *str= tmp ?
	      tmp->quick_fix_field(), tmp->val_str((String*) 0) :
	      (String*) 0;
	    $$= new Item_string(str ? str->ptr() : "",
				str ? str->length() : 0,
				Lex->charset);
	  }
	| UNDERSCORE_CHARSET BIN_NUM
          {
	    Item *tmp= new Item_bin_string($2.str, $2.length);
	    /*
	      it is OK only emulate fix_fieds, because we need only
              value of constant
	    */
	    String *str= tmp ?
	      tmp->quick_fix_field(), tmp->val_str((String*) 0) :
	      (String*) 0;
	    $$= new Item_string(str ? str->ptr() : "",
				str ? str->length() : 0,
				Lex->charset);
          }
	| DATE_SYM text_literal { $$ = $2; }
	| TIME_SYM text_literal { $$ = $2; }
	| TIMESTAMP text_literal { $$ = $2; };

NUM_literal:
	NUM		{ int error; $$ = new Item_int($1.str, (longlong) my_strtoll10($1.str, NULL, &error), $1.length); }
	| LONG_NUM	{ int error; $$ = new Item_int($1.str, (longlong) my_strtoll10($1.str, NULL, &error), $1.length); }
	| ULONGLONG_NUM	{ $$ =	new Item_uint($1.str, $1.length); }
        | DECIMAL_NUM
	{
           $$= new Item_decimal($1.str, $1.length, YYTHD->charset());
	   if (YYTHD->net.report_error)
	   {
	     YYABORT;
	   }
	}
	| FLOAT_NUM
	{
	   $$ =	new Item_float($1.str, $1.length);
	   if (YYTHD->net.report_error)
	   {
	     YYABORT;
	   }
	}
	;

/**********************************************************************
** Creating different items.
**********************************************************************/

insert_ident:
	simple_ident_nospvar { $$=$1; }
	| table_wild	 { $$=$1; };

table_wild:
	ident '.' '*'
	{
          SELECT_LEX *sel= Select;
	  $$ = new Item_field(Lex->current_context(), NullS, $1.str, "*");
	  sel->with_wild++;
	}
	| ident '.' ident '.' '*'
	{
          SELECT_LEX *sel= Select;
	  $$ = new Item_field(Lex->current_context(), (YYTHD->client_capabilities &
                             CLIENT_NO_SCHEMA ? NullS : $1.str),
                             $3.str,"*");
	  sel->with_wild++;
	}
	;

order_ident:
	expr { $$=$1; };

simple_ident:
	ident
	{
	  sp_variable_t *spv;
	  LEX *lex = Lex;
          sp_pcontext *spc = lex->spcont;
	  if (spc && (spv = spc->find_variable(&$1)))
	  {
            /* We're compiling a stored procedure and found a variable */
            Item_splocal *splocal;
            splocal= new Item_splocal($1, spv->offset, spv->type,
                                      lex->tok_start_prev - 
                                      lex->sphead->m_tmp_query);
#ifndef DBUG_OFF
            if (splocal)
              splocal->m_sp= lex->sphead;
#endif
	    $$ = (Item*) splocal;
            lex->variables_used= 1;
	    lex->safe_to_cache_query=0;
	  }
	  else
	  {
	    SELECT_LEX *sel=Select;
	    $$= (sel->parsing_place != IN_HAVING ||
	         sel->get_in_sum_expr() > 0) ?
                 (Item*) new Item_field(Lex->current_context(), NullS, NullS, $1.str) :
	         (Item*) new Item_ref(Lex->current_context(), NullS, NullS, $1.str);
	  }
        }
        | simple_ident_q { $$= $1; }
	;

simple_ident_nospvar:
	ident
	{
	  SELECT_LEX *sel=Select;
	  $$= (sel->parsing_place != IN_HAVING ||
	       sel->get_in_sum_expr() > 0) ?
              (Item*) new Item_field(Lex->current_context(), NullS, NullS, $1.str) :
	      (Item*) new Item_ref(Lex->current_context(), NullS, NullS, $1.str);
	}
	| simple_ident_q { $$= $1; }
	;

simple_ident_q:
	ident '.' ident
	{
	  THD *thd= YYTHD;
	  LEX *lex= thd->lex;

          /*
            FIXME This will work ok in simple_ident_nospvar case because
            we can't meet simple_ident_nospvar in trigger now. But it
            should be changed in future.
          */
          if (lex->sphead && lex->sphead->m_type == TYPE_ENUM_TRIGGER &&
              (!my_strcasecmp(system_charset_info, $1.str, "NEW") ||
               !my_strcasecmp(system_charset_info, $1.str, "OLD")))
          {
            Item_trigger_field *trg_fld;
            bool new_row= ($1.str[0]=='N' || $1.str[0]=='n');

            if (lex->trg_chistics.event == TRG_EVENT_INSERT &&
                !new_row)
            {
              my_error(ER_TRG_NO_SUCH_ROW_IN_TRG, MYF(0), "OLD", "on INSERT");
              YYABORT;
            }

            if (lex->trg_chistics.event == TRG_EVENT_DELETE &&
                new_row)
            {
              my_error(ER_TRG_NO_SUCH_ROW_IN_TRG, MYF(0), "NEW", "on DELETE");
              YYABORT;
            }

            DBUG_ASSERT(!new_row ||
                        (lex->trg_chistics.event == TRG_EVENT_INSERT ||
                         lex->trg_chistics.event == TRG_EVENT_UPDATE));
            const bool read_only=
              !(new_row && lex->trg_chistics.action_time == TRG_ACTION_BEFORE);
            if (!(trg_fld= new Item_trigger_field(Lex->current_context(),
                                                  new_row ?
                                                  Item_trigger_field::NEW_ROW:
                                                  Item_trigger_field::OLD_ROW,
                                                  $3.str,
                                                  SELECT_ACL,
                                                  read_only)))
              YYABORT;

            /*
              Let us add this item to list of all Item_trigger_field objects
              in trigger.
            */
            lex->trg_table_fields.link_in_list((byte *)trg_fld,
              (byte**)&trg_fld->next_trg_field);

            $$= (Item *)trg_fld;
          }
          else
          {
	    SELECT_LEX *sel= lex->current_select;
	    if (sel->no_table_names_allowed)
	    {
	      my_error(ER_TABLENAME_NOT_ALLOWED_HERE,
                       MYF(0), $1.str, thd->where);
	    }
	    $$= (sel->parsing_place != IN_HAVING ||
	         sel->get_in_sum_expr() > 0) ?
	        (Item*) new Item_field(Lex->current_context(), NullS, $1.str, $3.str) :
	        (Item*) new Item_ref(Lex->current_context(), NullS, $1.str, $3.str);
          }
        }
	| '.' ident '.' ident
	{
	  THD *thd= YYTHD;
	  LEX *lex= thd->lex;
	  SELECT_LEX *sel= lex->current_select;
	  if (sel->no_table_names_allowed)
	  {
	    my_error(ER_TABLENAME_NOT_ALLOWED_HERE,
                     MYF(0), $2.str, thd->where);
	  }
	  $$= (sel->parsing_place != IN_HAVING ||
	       sel->get_in_sum_expr() > 0) ?
	      (Item*) new Item_field(Lex->current_context(), NullS, $2.str, $4.str) :
              (Item*) new Item_ref(Lex->current_context(), NullS, $2.str, $4.str);
	}
	| ident '.' ident '.' ident
	{
	  THD *thd= YYTHD;
	  LEX *lex= thd->lex;
	  SELECT_LEX *sel= lex->current_select;
	  if (sel->no_table_names_allowed)
	  {
	    my_error(ER_TABLENAME_NOT_ALLOWED_HERE,
                     MYF(0), $3.str, thd->where);
	  }
	  $$= (sel->parsing_place != IN_HAVING ||
	       sel->get_in_sum_expr() > 0) ?
	      (Item*) new Item_field(Lex->current_context(),
                                     (YYTHD->client_capabilities &
				      CLIENT_NO_SCHEMA ? NullS : $1.str),
				     $3.str, $5.str) :
	      (Item*) new Item_ref(Lex->current_context(),
                                   (YYTHD->client_capabilities &
				    CLIENT_NO_SCHEMA ? NullS : $1.str),
                                   $3.str, $5.str);
	};


field_ident:
	ident			{ $$=$1;}
	| ident '.' ident '.' ident
          {
            TABLE_LIST *table= (TABLE_LIST*) Select->table_list.first;
            if (my_strcasecmp(table_alias_charset, $1.str, table->db))
            {
              my_error(ER_WRONG_DB_NAME, MYF(0), $1.str);
              YYABORT;
            }
            if (my_strcasecmp(table_alias_charset, $3.str,
                              table->table_name))
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $3.str);
              YYABORT;
            }
            $$=$5;
          }
	| ident '.' ident
          {
            TABLE_LIST *table= (TABLE_LIST*) Select->table_list.first;
            if (my_strcasecmp(table_alias_charset, $1.str, table->alias))
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $1.str);
              YYABORT;
            }
            $$=$3;
          }
	| '.' ident		{ $$=$2;}	/* For Delphi */;

table_ident:
	ident			{ $$=new Table_ident($1); }
	| ident '.' ident	{ $$=new Table_ident(YYTHD, $1,$3,0);}
	| '.' ident		{ $$=new Table_ident($2);} /* For Delphi */
        ;

table_ident_nodb:
	ident			{ LEX_STRING db={(char*) any_db,3}; $$=new Table_ident(YYTHD, db,$1,0); }
        ;

IDENT_sys:
	IDENT { $$= $1; }
	| IDENT_QUOTED
	  {
	    THD *thd= YYTHD;
	    if (thd->charset_is_system_charset)
            {
              CHARSET_INFO *cs= system_charset_info;
              int dummy_error;
              uint wlen= cs->cset->well_formed_len(cs, $1.str,
                                                   $1.str+$1.length,
                                                   $1.length, &dummy_error);
              if (wlen < $1.length)
              {
                my_error(ER_INVALID_CHARACTER_STRING, MYF(0),
                         cs->csname, $1.str + wlen);
                YYABORT;
              }
	      $$= $1;
            }
	    else
	      thd->convert_string(&$$, system_charset_info,
				  $1.str, $1.length, thd->charset());
	  }
	;

TEXT_STRING_sys:
	TEXT_STRING
	{
	  THD *thd= YYTHD;
	  if (thd->charset_is_system_charset)
	    $$= $1;
	  else
	    thd->convert_string(&$$, system_charset_info,
				$1.str, $1.length, thd->charset());
	}
	;

TEXT_STRING_literal:
	TEXT_STRING
	{
	  THD *thd= YYTHD;
	  if (thd->charset_is_collation_connection)
	    $$= $1;
	  else
	    thd->convert_string(&$$, thd->variables.collation_connection,
				$1.str, $1.length, thd->charset());
	}
	;


TEXT_STRING_filesystem:
	TEXT_STRING
	{
	  THD *thd= YYTHD;
	  if (thd->charset_is_character_set_filesystem)
	    $$= $1;
	  else
	    thd->convert_string(&$$, thd->variables.character_set_filesystem,
				$1.str, $1.length, thd->charset());
	}
	;

ident:
	IDENT_sys	    { $$=$1; }
	| keyword
	{
	  THD *thd= YYTHD;
	  $$.str=    thd->strmake($1.str, $1.length);
	  $$.length= $1.length;
	}
	;

label_ident:
	IDENT_sys	    { $$=$1; }
	| keyword_sp
	{
	  THD *thd= YYTHD;
	  $$.str=    thd->strmake($1.str, $1.length);
	  $$.length= $1.length;
	}
	;

ident_or_text:
        ident                   { $$=$1;}
	| TEXT_STRING_sys	{ $$=$1;}
	| LEX_HOSTNAME		{ $$=$1;};

user:
	ident_or_text
	{
	  THD *thd= YYTHD;
	  if (!($$=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
	    YYABORT;
	  $$->user = $1;
	  $$->host.str= (char *) "%";
	  $$->host.length= 1;
	}
	| ident_or_text '@' ident_or_text
	  {
	    THD *thd= YYTHD;
	    if (!($$=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
	      YYABORT;
	    $$->user = $1; $$->host=$3;
	  }
	| CURRENT_USER optional_braces
	{
          if (!($$=(LEX_USER*) YYTHD->alloc(sizeof(st_lex_user))))
            YYABORT;
          /* 
            empty LEX_USER means current_user and 
            will be handled in the  get_current_user() function
            later
          */
          bzero($$, sizeof(LEX_USER));
	};

/* Keyword that we allow for identifiers (except SP labels) */
keyword:
	keyword_sp		{}
	| ASCII_SYM		{}
	| BACKUP_SYM		{}
	| BEGIN_SYM		{}
	| BYTE_SYM		{}
	| CACHE_SYM		{}
	| CHARSET		{}
	| CHECKSUM_SYM		{}
	| CLOSE_SYM		{}
	| COMMENT_SYM		{}
	| COMMIT_SYM		{}
	| CONTAINS_SYM          {}
        | DEALLOCATE_SYM        {}
	| DO_SYM		{}
	| END			{}
	| EXECUTE_SYM		{}
	| FLUSH_SYM		{}
	| HANDLER_SYM		{}
	| HELP_SYM		{}
	| LANGUAGE_SYM          {}
	| NO_SYM		{}
	| OPEN_SYM		{}
        | PREPARE_SYM           {}
	| REPAIR		{}
	| RESET_SYM		{}
	| RESTORE_SYM		{}
	| ROLLBACK_SYM		{}
	| SAVEPOINT_SYM		{}
	| SECURITY_SYM		{}
	| SIGNED_SYM		{}
	| SLAVE			{}
	| START_SYM		{}
	| STOP_SYM		{}
	| TRUNCATE_SYM		{}
	| UNICODE_SYM		{}
        | XA_SYM                {}
	;

/*
 * Keywords that we allow for labels in SPs.
 * Anything that's the beginning of a statement or characteristics
 * must be in keyword above, otherwise we get (harmful) shift/reduce
 * conflicts.
 */
keyword_sp:
	ACTION			{}
	| ADDDATE_SYM		{}
	| AFTER_SYM		{}
	| AGAINST		{}
	| AGGREGATE_SYM		{}
	| ALGORITHM_SYM		{}
	| ANY_SYM		{}
	| AUTO_INC		{}
	| AVG_ROW_LENGTH	{}
	| AVG_SYM		{}
	| BERKELEY_DB_SYM	{}
	| BINLOG_SYM		{}
	| BIT_SYM		{}
	| BOOL_SYM		{}
	| BOOLEAN_SYM		{}
	| BTREE_SYM		{}
	| CASCADED              {}
	| CHAIN_SYM		{}
	| CHANGED		{}
	| CIPHER_SYM		{}
	| CLIENT_SYM		{}
        | CODE_SYM              {}
	| COLLATION_SYM		{}
        | COLUMNS               {}
	| COMMITTED_SYM		{}
	| COMPACT_SYM		{}
	| COMPRESSED_SYM	{}
	| CONCURRENT		{}
	| CONSISTENT_SYM	{}
	| CUBE_SYM		{}
	| DATA_SYM		{}
	| DATETIME		{}
	| DATE_SYM		{}
	| DAY_SYM		{}
	| DEFINER_SYM		{}
	| DELAY_KEY_WRITE_SYM	{}
	| DES_KEY_FILE		{}
	| DIRECTORY_SYM		{}
	| DISCARD		{}
	| DUMPFILE		{}
	| DUPLICATE_SYM		{}
	| DYNAMIC_SYM		{}
	| ENUM			{}
	| ENGINE_SYM		{}
	| ENGINES_SYM		{}
	| ERRORS		{}
	| ESCAPE_SYM		{}
	| EVENTS_SYM		{}
        | EXPANSION_SYM         {}
	| EXTENDED_SYM		{}
	| FAST_SYM		{}
	| FOUND_SYM		{}
	| DISABLE_SYM		{}
	| ENABLE_SYM		{}
	| FULL			{}
	| FILE_SYM		{}
	| FIRST_SYM		{}
	| FIXED_SYM		{}
	| FRAC_SECOND_SYM	{}
	| GEOMETRY_SYM		{}
	| GEOMETRYCOLLECTION	{}
	| GET_FORMAT		{}
	| GRANTS		{}
	| GLOBAL_SYM		{}
	| HASH_SYM		{}
	| HOSTS_SYM		{}
	| HOUR_SYM		{}
	| IDENTIFIED_SYM	{}
	| INVOKER_SYM		{}
	| IMPORT		{}
	| INDEXES		{}
	| ISOLATION		{}
	| ISSUER_SYM		{}
	| INNOBASE_SYM		{}
	| INSERT_METHOD		{}
	| RELAY_THREAD		{}
	| LAST_SYM		{}
	| LEAVES                {}
	| LEVEL_SYM		{}
	| LINESTRING		{}
	| LOCAL_SYM		{}
	| LOCKS_SYM		{}
	| LOGS_SYM		{}
	| MAX_ROWS		{}
	| MASTER_SYM		{}
	| MASTER_HOST_SYM	{}
	| MASTER_PORT_SYM	{}
	| MASTER_LOG_FILE_SYM	{}
	| MASTER_LOG_POS_SYM	{}
	| MASTER_USER_SYM	{}
	| MASTER_PASSWORD_SYM	{}
	| MASTER_SERVER_ID_SYM  {}
	| MASTER_CONNECT_RETRY_SYM	{}
	| MASTER_SSL_SYM	{}
	| MASTER_SSL_CA_SYM	{}
	| MASTER_SSL_CAPATH_SYM	{}
	| MASTER_SSL_CERT_SYM	{}
	| MASTER_SSL_CIPHER_SYM	{}
	| MASTER_SSL_KEY_SYM	{}
	| MAX_CONNECTIONS_PER_HOUR	 {}
	| MAX_QUERIES_PER_HOUR	{}
	| MAX_UPDATES_PER_HOUR	{}
	| MAX_USER_CONNECTIONS_SYM {}
	| MEDIUM_SYM		{}
	| MERGE_SYM		{}
	| MICROSECOND_SYM	{}
        | MIGRATE_SYM           {}
	| MINUTE_SYM		{}
	| MIN_ROWS		{}
	| MODIFY_SYM		{}
	| MODE_SYM		{}
	| MONTH_SYM		{}
	| MULTILINESTRING	{}
	| MULTIPOINT		{}
	| MULTIPOLYGON		{}
        | MUTEX_SYM             {}
	| NAME_SYM              {}
	| NAMES_SYM		{}
	| NATIONAL_SYM		{}
	| NCHAR_SYM		{}
	| NDBCLUSTER_SYM	{}
	| NEXT_SYM		{}
	| NEW_SYM		{}
	| NONE_SYM		{}
	| NVARCHAR_SYM		{}
	| OFFSET_SYM		{}
	| OLD_PASSWORD		{}
	| ONE_SHOT_SYM		{}
        | ONE_SYM               {}
	| PACK_KEYS_SYM		{}
	| PARTIAL		{}
	| PASSWORD		{}
        | PHASE_SYM             {}
	| POINT_SYM		{}
	| POLYGON		{}
	| PREV_SYM		{}
        | PRIVILEGES            {}
	| PROCESS		{}
	| PROCESSLIST_SYM	{}
	| QUARTER_SYM		{}
	| QUERY_SYM		{}
	| QUICK			{}
	| RAID_0_SYM		{}
	| RAID_CHUNKS		{}
	| RAID_CHUNKSIZE	{}
	| RAID_STRIPED_SYM	{}
	| RAID_TYPE		{}
        | RECOVER_SYM           {}
        | REDUNDANT_SYM         {}
	| RELAY_LOG_FILE_SYM	{}
	| RELAY_LOG_POS_SYM	{}
	| RELOAD		{}
	| REPEATABLE_SYM	{}
	| REPLICATION		{}
	| RESOURCES		{}
        | RESUME_SYM            {}
	| RETURNS_SYM           {}
	| ROLLUP_SYM		{}
	| ROUTINE_SYM		{}
	| ROWS_SYM		{}
	| ROW_FORMAT_SYM	{}
	| ROW_SYM		{}
	| RTREE_SYM		{}
	| SECOND_SYM		{}
	| SERIAL_SYM		{}
	| SERIALIZABLE_SYM	{}
	| SESSION_SYM		{}
	| SIMPLE_SYM		{}
	| SHARE_SYM		{}
	| SHUTDOWN		{}
	| SNAPSHOT_SYM		{}
	| SOUNDS_SYM		{}
	| SQL_CACHE_SYM		{}
	| SQL_BUFFER_RESULT	{}
	| SQL_NO_CACHE_SYM	{}
	| SQL_THREAD		{}
	| STATUS_SYM		{}
	| STORAGE_SYM		{}
	| STRING_SYM		{}
	| SUBDATE_SYM		{}
	| SUBJECT_SYM		{}
	| SUPER_SYM		{}
        | SUSPEND_SYM           {}
        | TABLES                {}
	| TABLESPACE		{}
	| TEMPORARY		{}
	| TEMPTABLE_SYM		{}
	| TEXT_SYM		{}
	| TRANSACTION_SYM	{}
	| TRIGGERS_SYM		{}
	| TIMESTAMP		{}
	| TIMESTAMP_ADD		{}
	| TIMESTAMP_DIFF	{}
	| TIME_SYM		{}
	| TYPES_SYM		{}
        | TYPE_SYM              {}
        | UDF_RETURNS_SYM       {}
	| FUNCTION_SYM		{}
	| UNCOMMITTED_SYM	{}
	| UNDEFINED_SYM		{}
	| UNKNOWN_SYM		{}
	| UNTIL_SYM		{}
	| USER			{}
	| USE_FRM		{}
	| VARIABLES		{}
	| VIEW_SYM		{}
	| VALUE_SYM		{}
	| WARNINGS		{}
	| WEEK_SYM		{}
	| WORK_SYM		{}
	| X509_SYM		{}
	| YEAR_SYM		{}
	;

/* Option functions */

set:
	SET opt_option
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_SET_OPTION;
	  mysql_init_select(lex);
	  lex->option_type=OPT_SESSION;
	  lex->var_list.empty();
          lex->one_shot_set= 0;
	}
	option_value_list
	{}
	;

opt_option:
	/* empty */ {}
	| OPTION {};

option_value_list:
	option_type_value
	| option_value_list ',' option_type_value;

option_type_value:
        {
          if (Lex->sphead)
          {
            /*
              If we are in SP we want have own LEX for each assignment.
              This is mostly because it is hard for several sp_instr_set
              and sp_instr_set_trigger instructions share one LEX.
              (Well, it is theoretically possible but adds some extra
               overhead on preparation for execution stage and IMO less
               robust).

              QQ: May be we should simply prohibit group assignments in SP?
            */
            LEX *lex;
            Lex->sphead->reset_lex(YYTHD);
            lex= Lex;

            /* Set new LEX as if we at start of set rule. */
	    lex->sql_command= SQLCOM_SET_OPTION;
	    mysql_init_select(lex);
	    lex->option_type=OPT_SESSION;
	    lex->var_list.empty();
            lex->one_shot_set= 0;
	    lex->sphead->m_tmp_query= lex->tok_start;
          }
        }
	ext_option_value
        {
          LEX *lex= Lex;

          if (lex->sphead)
          {
            sp_head *sp= lex->sphead;

	    if (!lex->var_list.is_empty())
	    {
              /*
                We have assignment to user or system variable or
                option setting, so we should construct sp_instr_stmt
                for it.
              */
              LEX_STRING qbuff;
	      sp_instr_stmt *i;

              if (!(i= new sp_instr_stmt(sp->instructions(), lex->spcont,
                                         lex)))
                YYABORT;

              if (lex->ptr - lex->tok_end > 1)
                qbuff.length= lex->ptr - sp->m_tmp_query;
              else
                qbuff.length= lex->tok_end - sp->m_tmp_query;

              if (!(qbuff.str= alloc_root(YYTHD->mem_root, qbuff.length + 5)))
                YYABORT;

              strmake(strmake(qbuff.str, "SET ", 4), (char *)sp->m_tmp_query,
                      qbuff.length);
              qbuff.length+= 4;
              i->m_query= qbuff;
              sp->add_instr(i);
            }
            lex->sphead->restore_lex(YYTHD);
          }
        };

option_type:
        option_type2    {}
	| GLOBAL_SYM	{ $$=OPT_GLOBAL; }
	| LOCAL_SYM	{ $$=OPT_SESSION; }
	| SESSION_SYM	{ $$=OPT_SESSION; }
	;

option_type2:
	/* empty */	{ $$= OPT_DEFAULT; }
	| ONE_SHOT_SYM	{ Lex->one_shot_set= 1; $$= OPT_SESSION; }
	;

opt_var_type:
	/* empty */	{ $$=OPT_SESSION; }
	| GLOBAL_SYM	{ $$=OPT_GLOBAL; }
	| LOCAL_SYM	{ $$=OPT_SESSION; }
	| SESSION_SYM	{ $$=OPT_SESSION; }
	;

opt_var_ident_type:
	/* empty */		{ $$=OPT_DEFAULT; }
	| GLOBAL_SYM '.'	{ $$=OPT_GLOBAL; }
	| LOCAL_SYM '.'		{ $$=OPT_SESSION; }
	| SESSION_SYM '.'	{ $$=OPT_SESSION; }
	;

ext_option_value:
        sys_option_value
        | option_type2 option_value;

sys_option_value:
        option_type internal_variable_name equal set_expr_or_default
        {
          LEX *lex=Lex;

          if ($2.var == &trg_new_row_fake_var)
          {
            /* We are in trigger and assigning value to field of new row */
            Item *it;
            Item_trigger_field *trg_fld;
            sp_instr_set_trigger_field *sp_fld;
	    LINT_INIT(sp_fld);
            if ($1)
            {
              yyerror(ER(ER_SYNTAX_ERROR));
              YYABORT;
            }
            if ($4)
              it= $4;
            else
            {
              /* QQ: Shouldn't this be field's default value ? */
              it= new Item_null();
            }

            DBUG_ASSERT(lex->trg_chistics.action_time == TRG_ACTION_BEFORE &&
                        (lex->trg_chistics.event == TRG_EVENT_INSERT ||
                         lex->trg_chistics.event == TRG_EVENT_UPDATE));
            if (!(trg_fld= new Item_trigger_field(Lex->current_context(),
                                                  Item_trigger_field::NEW_ROW,
                                                  $2.base_name.str,
                                                  UPDATE_ACL, FALSE)) ||
                !(sp_fld= new sp_instr_set_trigger_field(lex->sphead->
                          	                         instructions(),
                                	                 lex->spcont,
							 trg_fld,
                                        	         it, lex)))
              YYABORT;

            /*
              Let us add this item to list of all Item_trigger_field
              objects in trigger.
            */
            lex->trg_table_fields.link_in_list((byte *)trg_fld,
                                    (byte **)&trg_fld->next_trg_field);

            lex->sphead->add_instr(sp_fld);
          }
          else if ($2.var)
          { /* System variable */
            if ($1)
              lex->option_type= $1;
            lex->var_list.push_back(new set_var(lex->option_type, $2.var,
                                    &$2.base_name, $4));
          }
          else
          {
            /* An SP local variable */
            sp_pcontext *ctx= lex->spcont;
            sp_variable_t *spv;
            sp_instr_set *sp_set;
            Item *it;
            if ($1)
            {
              yyerror(ER(ER_SYNTAX_ERROR));
              YYABORT;
            }

            spv= ctx->find_variable(&$2.base_name);

            if ($4)
              it= $4;
            else if (spv->dflt)
              it= spv->dflt;
            else
              it= new Item_null();
            sp_set= new sp_instr_set(lex->sphead->instructions(), ctx,
                                     spv->offset, it, spv->type, lex, TRUE);
            lex->sphead->add_instr(sp_set);
          }
        }
        | option_type TRANSACTION_SYM ISOLATION LEVEL_SYM isolation_types
	{
	  LEX *lex=Lex;
          if ($1)
            lex->option_type= $1;
	  lex->var_list.push_back(new set_var(lex->option_type,
                                              find_sys_var("tx_isolation"),
                                              &null_lex_str,
                                              new Item_int((int32) $5)));
	}
        ;

option_value:
	'@' ident_or_text equal expr
	{
          Lex->var_list.push_back(new set_var_user(new Item_func_set_user_var($2,$4)));
	}
	| '@' '@' opt_var_ident_type internal_variable_name equal set_expr_or_default
	  {
	    LEX *lex=Lex;
	    lex->var_list.push_back(new set_var($3, $4.var, &$4.base_name, $6));
	  }
	| charset old_or_new_charset_name_or_default
	{
	  THD *thd= YYTHD;
	  LEX *lex= Lex;
	  $2= $2 ? $2: global_system_variables.character_set_client;
	  lex->var_list.push_back(new set_var_collation_client($2,thd->variables.collation_database,$2));
	}
        | NAMES_SYM equal expr
	  {
	    LEX *lex= Lex;
            sp_pcontext *spc= lex->spcont;
	    LEX_STRING names;

	    names.str= (char *)"names";
	    names.length= 5;
	    if (spc && spc->find_variable(&names))
              my_error(ER_SP_BAD_VAR_SHADOW, MYF(0), names.str);
            else
              yyerror(ER(ER_SYNTAX_ERROR));

	    YYABORT;
	  }
	| NAMES_SYM charset_name_or_default opt_collate
	{
	  LEX *lex= Lex;
	  $2= $2 ? $2 : global_system_variables.character_set_client;
	  $3= $3 ? $3 : $2;
	  if (!my_charset_same($2,$3))
	  {
	    my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
                     $3->name, $2->csname);
	    YYABORT;
	  }
	  lex->var_list.push_back(new set_var_collation_client($3,$3,$3));
	}
	| PASSWORD equal text_or_password
	  {
	    THD *thd=YYTHD;
	    LEX_USER *user;
	    LEX *lex= Lex;	    
            sp_pcontext *spc= lex->spcont;
	    LEX_STRING pw;

	    pw.str= (char *)"password";
	    pw.length= 8;
	    if (spc && spc->find_variable(&pw))
	    {
              my_error(ER_SP_BAD_VAR_SHADOW, MYF(0), pw.str);
	      YYABORT;
	    }
	    if (!(user=(LEX_USER*) thd->alloc(sizeof(LEX_USER))))
	      YYABORT;
	    user->host=null_lex_str;
	    user->user.str=thd->security_ctx->priv_user;
	    thd->lex->var_list.push_back(new set_var_password(user, $3));
	  }
	| PASSWORD FOR_SYM user equal text_or_password
	  {
	    Lex->var_list.push_back(new set_var_password($3,$5));
	  }
	;

internal_variable_name:
	ident
	{
	  LEX *lex= Lex;
          sp_pcontext *spc= lex->spcont;
	  sp_variable_t *spv;

	  /* We have to lookup here since local vars can shadow sysvars */
	  if (!spc || !(spv = spc->find_variable(&$1)))
	  {
            /* Not an SP local variable */
	    sys_var *tmp=find_sys_var($1.str, $1.length);
	    if (!tmp)
	      YYABORT;
	    $$.var= tmp;
	    $$.base_name= null_lex_str;
            /*
              If this is time_zone variable we should open time zone
              describing tables 
            */
            if (tmp == &sys_time_zone &&
                lex->add_time_zone_tables_to_query_tables(YYTHD))
              YYABORT;
            else if (spc && tmp == &sys_autocommit)
            {
              /*
                We don't allow setting AUTOCOMMIT from a stored function
		or trigger.
              */
              lex->sphead->m_flags|= sp_head::HAS_SET_AUTOCOMMIT_STMT;
            }
	  }
	  else
	  {
            /* An SP local variable */
	    $$.var= NULL;
	    $$.base_name= $1;
	  }
	}
	| ident '.' ident
	  {
            LEX *lex= Lex;
            if (check_reserved_words(&$1))
            {
	      yyerror(ER(ER_SYNTAX_ERROR));
              YYABORT;
            }
            if (lex->sphead && lex->sphead->m_type == TYPE_ENUM_TRIGGER &&
                (!my_strcasecmp(system_charset_info, $1.str, "NEW") || 
                 !my_strcasecmp(system_charset_info, $1.str, "OLD")))
            {
              if ($1.str[0]=='O' || $1.str[0]=='o')
              {
                my_error(ER_TRG_CANT_CHANGE_ROW, MYF(0), "OLD", "");
                YYABORT;
              }
              if (lex->trg_chistics.event == TRG_EVENT_DELETE)
              {
                my_error(ER_TRG_NO_SUCH_ROW_IN_TRG, MYF(0),
                         "NEW", "on DELETE");
                YYABORT;
              }
              if (lex->trg_chistics.action_time == TRG_ACTION_AFTER)
              {
                my_error(ER_TRG_CANT_CHANGE_ROW, MYF(0), "NEW", "after ");
                YYABORT;
              }
              /* This special combination will denote field of NEW row */
              $$.var= &trg_new_row_fake_var;
              $$.base_name= $3;
            }
            else
            {
              sys_var *tmp=find_sys_var($3.str, $3.length);
              if (!tmp)
                YYABORT;
              if (!tmp->is_struct())
                my_error(ER_VARIABLE_IS_NOT_STRUCT, MYF(0), $3.str);
              $$.var= tmp;
              $$.base_name= $1;
            }
	  }
	| DEFAULT '.' ident
	  {
	    sys_var *tmp=find_sys_var($3.str, $3.length);
	    if (!tmp)
	      YYABORT;
	    if (!tmp->is_struct())
	      my_error(ER_VARIABLE_IS_NOT_STRUCT, MYF(0), $3.str);
	    $$.var= tmp;
	    $$.base_name.str=    (char*) "default";
	    $$.base_name.length= 7;
	  }
        ;

isolation_types:
	READ_SYM UNCOMMITTED_SYM	{ $$= ISO_READ_UNCOMMITTED; }
	| READ_SYM COMMITTED_SYM	{ $$= ISO_READ_COMMITTED; }
	| REPEATABLE_SYM READ_SYM	{ $$= ISO_REPEATABLE_READ; }
	| SERIALIZABLE_SYM		{ $$= ISO_SERIALIZABLE; }
	;

text_or_password:
	TEXT_STRING { $$=$1.str;}
	| PASSWORD '(' TEXT_STRING ')'
	  {
	    $$= $3.length ? YYTHD->variables.old_passwords ?
	        Item_func_old_password::alloc(YYTHD, $3.str) :
	        Item_func_password::alloc(YYTHD, $3.str) :
	      $3.str;
	  }
	| OLD_PASSWORD '(' TEXT_STRING ')'
	  {
	    $$= $3.length ? Item_func_old_password::alloc(YYTHD, $3.str) :
	      $3.str;
	  }
          ;


set_expr_or_default:
	expr      { $$=$1; }
	| DEFAULT { $$=0; }
	| ON	  { $$=new Item_string("ON",  2, system_charset_info); }
	| ALL	  { $$=new Item_string("ALL", 3, system_charset_info); }
	| BINARY  { $$=new Item_string("binary", 6, system_charset_info); }
	;


/* Lock function */

lock:
	LOCK_SYM table_or_tables
	{
	  LEX *lex= Lex;

	  if (lex->sphead)
	  {
	    my_error(ER_SP_BADSTATEMENT, MYF(0), "LOCK");
	    YYABORT;
	  }
	  lex->sql_command= SQLCOM_LOCK_TABLES;
	}
	table_lock_list
	{}
	;

table_or_tables:
	TABLE_SYM
	| TABLES;

table_lock_list:
	table_lock
	| table_lock_list ',' table_lock;

table_lock:
	table_ident opt_table_alias lock_option
	{
	  if (!Select->add_table_to_list(YYTHD, $1, $2, 0, (thr_lock_type) $3))
	   YYABORT;
	}
        ;

lock_option:
	READ_SYM	{ $$=TL_READ_NO_INSERT; }
	| WRITE_SYM     { $$=YYTHD->update_lock_default; }
	| LOW_PRIORITY WRITE_SYM { $$=TL_WRITE_LOW_PRIORITY; }
	| READ_SYM LOCAL_SYM { $$= TL_READ; }
        ;

unlock:
	UNLOCK_SYM
	{
	  LEX *lex= Lex;

	  if (lex->sphead)
	  {
	    my_error(ER_SP_BADSTATEMENT, MYF(0), "UNLOCK");
	    YYABORT;
	  }
	  lex->sql_command= SQLCOM_UNLOCK_TABLES;
	}
	table_or_tables
	{}
        ;


/*
** Handler: direct access to ISAM functions
*/

handler:
	HANDLER_SYM table_ident OPEN_SYM opt_table_alias
	{
	  LEX *lex= Lex;
	  if (lex->sphead)
	  {
	    my_error(ER_SP_BADSTATEMENT, MYF(0), "HANDLER");
	    YYABORT;
	  }
	  lex->sql_command = SQLCOM_HA_OPEN;
	  if (!lex->current_select->add_table_to_list(lex->thd, $2, $4, 0))
	    YYABORT;
	}
	| HANDLER_SYM table_ident_nodb CLOSE_SYM
	{
	  LEX *lex= Lex;
	  if (lex->sphead)
	  {
	    my_error(ER_SP_BADSTATEMENT, MYF(0), "HANDLER");
	    YYABORT;
	  }
	  lex->sql_command = SQLCOM_HA_CLOSE;
	  if (!lex->current_select->add_table_to_list(lex->thd, $2, 0, 0))
	    YYABORT;
	}
	| HANDLER_SYM table_ident_nodb READ_SYM
	{
	  LEX *lex=Lex;
	  if (lex->sphead)
	  {
	    my_error(ER_SP_BADSTATEMENT, MYF(0), "HANDLER");
	    YYABORT;
	  }
	  lex->sql_command = SQLCOM_HA_READ;
	  lex->ha_rkey_mode= HA_READ_KEY_EXACT;	/* Avoid purify warnings */
	  lex->current_select->select_limit= new Item_int((int32) 1);
	  lex->current_select->offset_limit= 0;
	  if (!lex->current_select->add_table_to_list(lex->thd, $2, 0, 0))
	    YYABORT;
        }
        handler_read_or_scan where_clause opt_limit_clause {}
        ;

handler_read_or_scan:
	handler_scan_function         { Lex->ident= null_lex_str; }
        | ident handler_rkey_function { Lex->ident= $1; }
        ;

handler_scan_function:
	FIRST_SYM  { Lex->ha_read_mode = RFIRST; }
	| NEXT_SYM { Lex->ha_read_mode = RNEXT;  }
        ;

handler_rkey_function:
	FIRST_SYM  { Lex->ha_read_mode = RFIRST; }
	| NEXT_SYM { Lex->ha_read_mode = RNEXT;  }
	| PREV_SYM { Lex->ha_read_mode = RPREV;  }
	| LAST_SYM { Lex->ha_read_mode = RLAST;  }
	| handler_rkey_mode
	{
	  LEX *lex=Lex;
	  lex->ha_read_mode = RKEY;
	  lex->ha_rkey_mode=$1;
	  if (!(lex->insert_list = new List_item))
	    YYABORT;
	} '(' values ')' { }
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
	REVOKE clear_privileges revoke_command
	{}
        ;

revoke_command:
	grant_privileges ON opt_table grant_ident FROM grant_list
	{
          LEX *lex= Lex;
	  lex->sql_command= SQLCOM_REVOKE;
          lex->type= 0;
        }
        |
        grant_privileges ON FUNCTION_SYM grant_ident FROM grant_list
        {
          LEX *lex= Lex;
          if (lex->columns.elements)
          {
            yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
          }
	  lex->sql_command= SQLCOM_REVOKE;
          lex->type= TYPE_ENUM_FUNCTION;
          
        }
	|
        grant_privileges ON PROCEDURE grant_ident FROM grant_list
        {
          LEX *lex= Lex;
          if (lex->columns.elements)
          {
            yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
          }
	  lex->sql_command= SQLCOM_REVOKE;
          lex->type= TYPE_ENUM_PROCEDURE;
        }
	|
	ALL opt_privileges ',' GRANT OPTION FROM grant_list
	{
	  Lex->sql_command = SQLCOM_REVOKE_ALL;
	}
	;

grant:
	GRANT clear_privileges grant_command
	{}
        ;

grant_command:
	grant_privileges ON opt_table grant_ident TO_SYM grant_list
	require_clause grant_options
	{
          LEX *lex= Lex;
          lex->sql_command= SQLCOM_GRANT;
          lex->type= 0;
        }
        |
	grant_privileges ON FUNCTION_SYM grant_ident TO_SYM grant_list
	require_clause grant_options
	{
          LEX *lex= Lex;
          if (lex->columns.elements)
          {
            yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
          }
          lex->sql_command= SQLCOM_GRANT;
          lex->type= TYPE_ENUM_FUNCTION;
        }
        |
	grant_privileges ON PROCEDURE grant_ident TO_SYM grant_list
	require_clause grant_options
	{
          LEX *lex= Lex;
          if (lex->columns.elements)
          {
            yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
          }
          lex->sql_command= SQLCOM_GRANT;
          lex->type= TYPE_ENUM_PROCEDURE;
        }
        ;

opt_table:
	/* Empty */
	| TABLE_SYM ;
        
grant_privileges:
	object_privilege_list { }
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
	| object_privilege_list ',' object_privilege;

object_privilege:
	SELECT_SYM	{ Lex->which_columns = SELECT_ACL;} opt_column_list {}
	| INSERT	{ Lex->which_columns = INSERT_ACL;} opt_column_list {}
	| UPDATE_SYM	{ Lex->which_columns = UPDATE_ACL; } opt_column_list {}
	| REFERENCES	{ Lex->which_columns = REFERENCES_ACL;} opt_column_list {}
	| DELETE_SYM	{ Lex->grant |= DELETE_ACL;}
	| USAGE		{}
	| INDEX_SYM	{ Lex->grant |= INDEX_ACL;}
	| ALTER		{ Lex->grant |= ALTER_ACL;}
	| CREATE	{ Lex->grant |= CREATE_ACL;}
	| DROP		{ Lex->grant |= DROP_ACL;}
	| EXECUTE_SYM	{ Lex->grant |= EXECUTE_ACL;}
	| RELOAD	{ Lex->grant |= RELOAD_ACL;}
	| SHUTDOWN	{ Lex->grant |= SHUTDOWN_ACL;}
	| PROCESS	{ Lex->grant |= PROCESS_ACL;}
	| FILE_SYM	{ Lex->grant |= FILE_ACL;}
	| GRANT OPTION  { Lex->grant |= GRANT_ACL;}
	| SHOW DATABASES { Lex->grant |= SHOW_DB_ACL;}
	| SUPER_SYM	{ Lex->grant |= SUPER_ACL;}
	| CREATE TEMPORARY TABLES { Lex->grant |= CREATE_TMP_ACL;}
	| LOCK_SYM TABLES   { Lex->grant |= LOCK_TABLES_ACL; }
	| REPLICATION SLAVE  { Lex->grant |= REPL_SLAVE_ACL; }
	| REPLICATION CLIENT_SYM { Lex->grant |= REPL_CLIENT_ACL; }
	| CREATE VIEW_SYM { Lex->grant |= CREATE_VIEW_ACL; }
	| SHOW VIEW_SYM { Lex->grant |= SHOW_VIEW_ACL; }
	| CREATE ROUTINE_SYM { Lex->grant |= CREATE_PROC_ACL; }
	| ALTER ROUTINE_SYM { Lex->grant |= ALTER_PROC_ACL; }
	| CREATE USER { Lex->grant |= CREATE_USER_ACL; }
	;


opt_and:
	/* empty */	{}
	| AND_SYM	{}
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
	    YYABORT;
	  }
	  lex->x509_subject=$2.str;
	}
	| ISSUER_SYM TEXT_STRING
	{
	  LEX *lex=Lex;
	  if (lex->x509_issuer)
	  {
	    my_error(ER_DUP_ARGUMENT, MYF(0), "ISSUER");
	    YYABORT;
	  }
	  lex->x509_issuer=$2.str;
	}
	| CIPHER_SYM TEXT_STRING
	{
	  LEX *lex=Lex;
	  if (lex->ssl_cipher)
	  {
	    my_error(ER_DUP_ARGUMENT, MYF(0), "CIPHER");
	    YYABORT;
	  }
	  lex->ssl_cipher=$2.str;
	}
	;

grant_ident:
	'*'
	  {
	    LEX *lex= Lex;
            THD *thd= lex->thd;
            if (thd->copy_db_to(&lex->current_select->db, NULL))
              YYABORT;
	    if (lex->grant == GLOBAL_ACLS)
	      lex->grant = DB_ACLS & ~GRANT_ACL;
	    else if (lex->columns.elements)
	    {
	      my_message(ER_ILLEGAL_GRANT_FOR_TABLE,
                         ER(ER_ILLEGAL_GRANT_FOR_TABLE), MYF(0));
	      YYABORT;
	    }
	  }
	| ident '.' '*'
	  {
	    LEX *lex= Lex;
	    lex->current_select->db = $1.str;
	    if (lex->grant == GLOBAL_ACLS)
	      lex->grant = DB_ACLS & ~GRANT_ACL;
	    else if (lex->columns.elements)
	    {
	      my_message(ER_ILLEGAL_GRANT_FOR_TABLE,
                         ER(ER_ILLEGAL_GRANT_FOR_TABLE), MYF(0));
	      YYABORT;
	    }
	  }
	| '*' '.' '*'
	  {
	    LEX *lex= Lex;
	    lex->current_select->db = NULL;
	    if (lex->grant == GLOBAL_ACLS)
	      lex->grant= GLOBAL_ACLS & ~GRANT_ACL;
	    else if (lex->columns.elements)
	    {
	      my_message(ER_ILLEGAL_GRANT_FOR_TABLE,
                         ER(ER_ILLEGAL_GRANT_FOR_TABLE), MYF(0));
	      YYABORT;
	    }
	  }
	| table_ident
	  {
	    LEX *lex=Lex;
	    if (!lex->current_select->add_table_to_list(lex->thd, $1,NULL,0))
	      YYABORT;
	    if (lex->grant == GLOBAL_ACLS)
	      lex->grant =  TABLE_ACLS & ~GRANT_ACL;
	  }
          ;


user_list:
	user  { if (Lex->users_list.push_back($1)) YYABORT;}
	| user_list ',' user
	  {
	    if (Lex->users_list.push_back($3))
	      YYABORT;
	  }
	;


grant_list:
	grant_user  { if (Lex->users_list.push_back($1)) YYABORT;}
	| grant_list ',' grant_user
	  {
	    if (Lex->users_list.push_back($3))
	      YYABORT;
	  }
	;


grant_user:
	user IDENTIFIED_SYM BY TEXT_STRING
	{
	   $$=$1; $1->password=$4;
	   if ($4.length)
	   {
             if (YYTHD->variables.old_passwords)
             {
               char *buff= 
                 (char *) YYTHD->alloc(SCRAMBLED_PASSWORD_CHAR_LENGTH_323+1);
               if (buff)
                 make_scrambled_password_323(buff, $4.str);
               $1->password.str= buff;
               $1->password.length= SCRAMBLED_PASSWORD_CHAR_LENGTH_323;
             }
             else
             {
               char *buff= 
                 (char *) YYTHD->alloc(SCRAMBLED_PASSWORD_CHAR_LENGTH+1);
               if (buff)
                 make_scrambled_password(buff, $4.str);
               $1->password.str= buff;
               $1->password.length= SCRAMBLED_PASSWORD_CHAR_LENGTH;
             }
	  }
	}
	| user IDENTIFIED_SYM BY PASSWORD TEXT_STRING
	  { $$= $1; $1->password= $5; }
	| user
	  { $$= $1; $1->password= null_lex_str; }
        ;


opt_column_list:
	/* empty */
	{
	  LEX *lex=Lex;
	  lex->grant |= lex->which_columns;
	}
	| '(' column_list ')';

column_list:
	column_list ',' column_list_id
	| column_list_id;

column_list_id:
	ident
	{
	  String *new_str = new (YYTHD->mem_root) String((const char*) $1.str,$1.length,system_charset_info);
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
	    lex->columns.push_back(new LEX_COLUMN (*new_str,lex->which_columns));
	}
        ;


require_clause: /* empty */
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
	| WITH grant_option_list;

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
	/* empty */ { $$= (YYTHD->variables.completion_type == 1); }
	| AND_SYM NO_SYM CHAIN_SYM	{ $$=0; }
	| AND_SYM CHAIN_SYM		{ $$=1; }
	;

opt_release:
	/* empty */ { $$= (YYTHD->variables.completion_type == 2); }
	| RELEASE_SYM 			{ $$=1; }
	| NO_SYM RELEASE_SYM 		{ $$=0; }
	;
	
opt_savepoint:
	/* empty */	{}
	| SAVEPOINT_SYM {}
	;

commit:
	COMMIT_SYM opt_work opt_chain opt_release
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_COMMIT;
	  lex->tx_chain= $3; 
	  lex->tx_release= $4;
	}
	;

rollback:
	ROLLBACK_SYM opt_work opt_chain opt_release
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_ROLLBACK;
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


union_clause:
	/* empty */ {}
	| union_list
	;

union_list:
	UNION_SYM union_option
	{
	  LEX *lex=Lex;
	  if (lex->exchange)
	  {
	    /* Only the last SELECT can have  INTO...... */
	    my_error(ER_WRONG_USAGE, MYF(0), "UNION", "INTO");
	    YYABORT;
	  }
	  if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE)
	  {
            yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
	  }
          /* This counter shouldn't be incremented for UNION parts */
          Lex->nest_level--;
	  if (mysql_new_select(lex, 0))
	    YYABORT;
          mysql_init_select(lex);
	  lex->current_select->linkage=UNION_TYPE;
          if ($2) /* UNION DISTINCT - remember position */
            lex->current_select->master_unit()->union_distinct=
                                                      lex->current_select;
	}
	select_init
        {
          /*
	    Remove from the name resolution context stack the context of the
            last select in the union.
	  */
          Lex->pop_context();
        }
	;

union_opt:
	/* Empty */ { $$= 0; }
	| union_list { $$= 1; }
	| union_order_or_limit { $$= 1; }
	;

union_order_or_limit:
	  {
	    THD *thd= YYTHD;
	    LEX *lex= thd->lex;
	    DBUG_ASSERT(lex->current_select->linkage != GLOBAL_OPTIONS_TYPE);
	    SELECT_LEX *sel= lex->current_select;
	    SELECT_LEX_UNIT *unit= sel->master_unit();
	    SELECT_LEX *fake= unit->fake_select_lex;
	    if (fake)
	    {
	      unit->global_parameters= fake;
	      fake->no_table_names_allowed= 1;
	      lex->current_select= fake;
	    }
	    thd->where= "global ORDER clause";
	  }
	order_or_limit
          {
	    THD *thd= YYTHD;
	    thd->lex->current_select->no_table_names_allowed= 0;
	    thd->where= "";
          }
	;

order_or_limit:
	order_clause opt_limit_clause_init
	| limit_clause
	;

union_option:
	/* empty */ { $$=1; }
	| DISTINCT  { $$=1; }
	| ALL       { $$=0; }
        ;

singlerow_subselect:
	subselect_start singlerow_subselect_init
	subselect_end
	{
	  $$= $2;
	};

singlerow_subselect_init:
	select_init2
	{
	  $$= new Item_singlerow_subselect(Lex->current_select->
					   master_unit()->first_select());
	};

exists_subselect:
	subselect_start exists_subselect_init
	subselect_end
	{
	  $$= $2;
	};

exists_subselect_init:
	select_init2
	{
	  $$= new Item_exists_subselect(Lex->current_select->master_unit()->
					first_select());
	};

in_subselect:
  subselect_start in_subselect_init
  subselect_end
  {
    $$= $2;
  };

in_subselect_init:
  select_init2
  {
    $$= Lex->current_select->master_unit()->first_select();
  };

subselect_start:
	'(' SELECT_SYM
	{
	  LEX *lex=Lex;
          if (lex->sql_command == (int)SQLCOM_HA_READ ||
              lex->sql_command == (int)SQLCOM_KILL)
	  {
            yyerror(ER(ER_SYNTAX_ERROR));
	    YYABORT;
	  }
	  if (mysql_new_select(Lex, 1))
	    YYABORT;
	};

subselect_end:
	')'
	{
	  LEX *lex=Lex;
          lex->pop_context();
	  lex->current_select = lex->current_select->return_after_parsing();
          lex->nest_level--;
	};

/**************************************************************************

 CREATE VIEW | TRIGGER | PROCEDURE statements.

**************************************************************************/

view_or_trigger_or_sp:
	definer view_or_trigger_or_sp_tail
	{}
	| view_replace_or_algorithm definer view_tail
	{}
	;

view_or_trigger_or_sp_tail:
	view_tail
	{}
	| trigger_tail
	{}
	| sp_tail
	{}
	;

/**************************************************************************

 DEFINER clause support.

**************************************************************************/

definer:
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
	| DEFINER_SYM EQ CURRENT_USER optional_braces
	{
          if (! (YYTHD->lex->definer= create_default_definer(YYTHD)))
            YYABORT;
	}
	| DEFINER_SYM EQ ident_or_text '@' ident_or_text
	{
          if (!(YYTHD->lex->definer= create_definer(YYTHD, &$3, &$5)))
            YYABORT;
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
	{ Lex->create_view_algorithm= VIEW_ALGORITHM_TMPTABLE; }
	;

view_algorithm_opt:
	/* empty */
	{ Lex->create_view_algorithm= VIEW_ALGORITHM_UNDEFINED; }
	| view_algorithm
	{}
	;

view_suid:
	/* empty */
	{ Lex->create_view_suid= TRUE; }
	| SQL_SYM SECURITY_SYM DEFINER_SYM
	{ Lex->create_view_suid= TRUE; }
	| SQL_SYM SECURITY_SYM INVOKER_SYM
	{ Lex->create_view_suid= FALSE; }
	;

view_tail:
	view_suid VIEW_SYM table_ident
	{
	  THD *thd= YYTHD;
	  LEX *lex= thd->lex;
	  lex->sql_command= SQLCOM_CREATE_VIEW;
	  /* first table in list is target VIEW name */
	  if (!lex->select_lex.add_table_to_list(thd, $3, NULL, TL_OPTION_UPDATING))
	    YYABORT;
	}
	view_list_opt AS view_select view_check_option
	{}
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
	SELECT_SYM remember_name select_init2
	{
          THD *thd=YYTHD;
          LEX *lex= thd->lex;
          char *stmt_beg= (lex->sphead ?
                           (char *)lex->sphead->m_tmp_query :
                           thd->query);
	  lex->create_view_select_start= $2 - stmt_beg;
	}
	| '(' remember_name select_paren ')' union_opt
	{
          THD *thd=YYTHD;
          LEX *lex= thd->lex;
          char *stmt_beg= (lex->sphead ?
                           (char *)lex->sphead->m_tmp_query :
                           thd->query);
	  lex->create_view_select_start= $2 - stmt_beg;
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

trigger_tail:
	TRIGGER_SYM remember_name sp_name trg_action_time trg_event
	ON remember_name table_ident FOR_SYM remember_name EACH_SYM ROW_SYM
	{
	  LEX *lex= Lex;
	  sp_head *sp;
	 
	  if (lex->sphead)
	  {
	    my_error(ER_SP_NO_RECURSIVE_CREATE, MYF(0), "TRIGGER");
	    YYABORT;
	  }
	
	  if (!(sp= new sp_head()))
	    YYABORT;
	  sp->reset_thd_mem_root(YYTHD);
	  sp->init(lex);
	
	  lex->stmt_definition_begin= $2;
          lex->ident.str= $7;
          lex->ident.length= $10 - $7;

	  sp->m_type= TYPE_ENUM_TRIGGER;
	  lex->sphead= sp;
	  lex->spname= $3;
	  /*
	    We have to turn of CLIENT_MULTI_QUERIES while parsing a
	    stored procedure, otherwise yylex will chop it into pieces
	    at each ';'.
	  */
	  sp->m_old_cmq= YYTHD->client_capabilities & CLIENT_MULTI_QUERIES;
	  YYTHD->client_capabilities &= ~CLIENT_MULTI_QUERIES;
	  
	  bzero((char *)&lex->sp_chistics, sizeof(st_sp_chistics));
	  lex->sphead->m_chistics= &lex->sp_chistics;
	  lex->sphead->m_body_begin= lex->ptr;
          while (my_isspace(system_charset_info, lex->sphead->m_body_begin[0]))
            ++lex->sphead->m_body_begin;
	}
	sp_proc_stmt
	{
	  LEX *lex= Lex;
	  sp_head *sp= lex->sphead;
	  
	  lex->sql_command= SQLCOM_CREATE_TRIGGER;
	  sp->init_strings(YYTHD, lex, $3);
	  /* Restore flag if it was cleared above */
	  if (sp->m_old_cmq)
	    YYTHD->client_capabilities |= CLIENT_MULTI_QUERIES;
	  sp->restore_thd_mem_root(YYTHD);
	
	  if (sp->is_not_allowed_in_function("trigger"))
	      YYABORT;
	
	  /*
	    We have to do it after parsing trigger body, because some of
	    sp_proc_stmt alternatives are not saving/restoring LEX, so
	    lex->query_tables can be wiped out.
	  */
	  if (!lex->select_lex.add_table_to_list(YYTHD, $8,
	                                         (LEX_STRING*) 0,
	                                         TL_OPTION_UPDATING,
                                                 TL_IGNORE))
	    YYABORT;
	}
	;

/**************************************************************************

 CREATE FUNCTION | PROCEDURE statements parts.

**************************************************************************/

sp_tail:
	udf_func_type remember_name FUNCTION_SYM sp_name
	{
	  LEX *lex=Lex;
	  lex->udf.type= $1;
	  lex->stmt_definition_begin= $2;
	  lex->spname= $4;
	}
	create_function_tail
	{}
	| PROCEDURE remember_name sp_name
	{
	  LEX *lex= Lex;
	  sp_head *sp;

	  if (lex->sphead)
	  {
	    my_error(ER_SP_NO_RECURSIVE_CREATE, MYF(0), "PROCEDURE");
	    YYABORT;
	  }
	  
	  lex->stmt_definition_begin= $2;
	  
	  /* Order is important here: new - reset - init */
	  sp= new sp_head();
	  sp->reset_thd_mem_root(YYTHD);
	  sp->init(lex);

	  sp->m_type= TYPE_ENUM_PROCEDURE;
	  lex->sphead= sp;
	  /*
	   * We have to turn of CLIENT_MULTI_QUERIES while parsing a
	   * stored procedure, otherwise yylex will chop it into pieces
	   * at each ';'.
	   */
	  sp->m_old_cmq= YYTHD->client_capabilities & CLIENT_MULTI_QUERIES;
	  YYTHD->client_capabilities &= (~CLIENT_MULTI_QUERIES);
	}
        '('
	{
	  LEX *lex= Lex;

	  lex->sphead->m_param_begin= lex->tok_start+1;
	}
	sp_pdparam_list
	')'
	{
	  LEX *lex= Lex;

	  lex->sphead->m_param_end= lex->tok_start;
	  bzero((char *)&lex->sp_chistics, sizeof(st_sp_chistics));
	}
	sp_c_chistics
	{
	  LEX *lex= Lex;

	  lex->sphead->m_chistics= &lex->sp_chistics;
	  lex->sphead->m_body_begin= lex->tok_start;
	}
	sp_proc_stmt
	{
	  LEX *lex= Lex;
	  sp_head *sp= lex->sphead;

	  sp->init_strings(YYTHD, lex, $3);
	  lex->sql_command= SQLCOM_CREATE_PROCEDURE;
	  /* Restore flag if it was cleared above */
	  if (sp->m_old_cmq)
	    YYTHD->client_capabilities |= CLIENT_MULTI_QUERIES;
	  sp->restore_thd_mem_root(YYTHD);
	}
	;

/*************************************************************************/

xa: XA_SYM begin_or_start xid opt_join_or_resume
      {
        Lex->sql_command = SQLCOM_XA_START;
      }
    | XA_SYM END xid opt_suspend
      {
        Lex->sql_command = SQLCOM_XA_END;
      }
    | XA_SYM PREPARE_SYM xid
      {
        Lex->sql_command = SQLCOM_XA_PREPARE;
      }
    | XA_SYM COMMIT_SYM xid opt_one_phase
      {
        Lex->sql_command = SQLCOM_XA_COMMIT;
      }
    | XA_SYM ROLLBACK_SYM xid
      {
        Lex->sql_command = SQLCOM_XA_ROLLBACK;
      }
    | XA_SYM RECOVER_SYM
      {
        Lex->sql_command = SQLCOM_XA_RECOVER;
      }
    ;

xid: text_string
     {
       YYERROR_UNLESS($1->length() <= MAXGTRIDSIZE);
       if (!(Lex->xid=(XID *)YYTHD->alloc(sizeof(XID))))
         YYABORT;
       Lex->xid->set(1L, $1->ptr(), $1->length(), 0, 0);
     }
     | text_string ',' text_string
     {
       YYERROR_UNLESS($1->length() <= MAXGTRIDSIZE && $3->length() <= MAXBQUALSIZE);
       if (!(Lex->xid=(XID *)YYTHD->alloc(sizeof(XID))))
         YYABORT;
       Lex->xid->set(1L, $1->ptr(), $1->length(), $3->ptr(), $3->length());
     }
     | text_string ',' text_string ',' ulong_num
     {
       YYERROR_UNLESS($1->length() <= MAXGTRIDSIZE && $3->length() <= MAXBQUALSIZE);
       if (!(Lex->xid=(XID *)YYTHD->alloc(sizeof(XID))))
         YYABORT;
       Lex->xid->set($5, $1->ptr(), $1->length(), $3->ptr(), $3->length());
     }
     ;

begin_or_start:   BEGIN_SYM {}
    |             START_SYM {}
    ;

opt_join_or_resume:
    /* nothing */           { Lex->xa_opt=XA_NONE;        }
    | JOIN_SYM              { Lex->xa_opt=XA_JOIN;        }
    | RESUME_SYM            { Lex->xa_opt=XA_RESUME;      }
    ;

opt_one_phase:
    /* nothing */           { Lex->xa_opt=XA_NONE;        }
    | ONE_SYM PHASE_SYM     { Lex->xa_opt=XA_ONE_PHASE;   }
    ;

opt_suspend:
    /* nothing */           { Lex->xa_opt=XA_NONE;        }
    | SUSPEND_SYM           { Lex->xa_opt=XA_SUSPEND;     }
      opt_migrate
    ;

opt_migrate:
    /* nothing */           { }
    | FOR_SYM MIGRATE_SYM   { Lex->xa_opt=XA_FOR_MIGRATE; }
    ;


