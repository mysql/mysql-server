/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
#define MYSQL_YACC
#define YYINITDEPTH 100
#define YYMAXDEPTH 3200				/* Because of 64K stack */
#define Lex current_lex
#include "mysql_priv.h"
#include "slave.h"  
#include "sql_acl.h"
#include "lex_symbol.h"
#include <myisam.h>

extern void yyerror(const char*);
int yylex(void *yylval);

#define yyoverflow(A,B,C,D,E,F) if (my_yyoverflow((B),(D),(F))) { yyerror((char*) (A)); return 2; }

inline Item *or_or_concat(Item* A, Item* B)
{
  return (current_thd->options & OPTION_ANSI_MODE ?
          (Item*) new Item_func_concat(A,B) : (Item*) new Item_cond_or(A,B));
}

%}
%union {
  int  num;
  ulong ulong_num;
  ulonglong ulonglong_num;
  LEX_STRING lex_str;
  LEX_STRING *lex_str_ptr;
  LEX_SYMBOL symbol;
  Table_ident *table;
  char *simple_string;
  Item *item;
  List<Item> *item_list;
  List<String> *string_list;
  Key::Keytype key_type;
  enum db_type db_type;
  enum row_type row_type;
  enum enum_tx_isolation tx_isolation;
  String *string;
  key_part_spec *key_part;
  TABLE_LIST *table_list;
  udf_func *udf;
  interval_type interval;
  LEX_USER *lex_user;
  enum Item_udftype udf_type;
}

%{
bool my_yyoverflow(short **a, YYSTYPE **b,int *yystacksize);
%}

%pure_parser					/* We have threads */

%token	END_OF_INPUT

%token	EQ
%token	EQUAL_SYM
%token	GE
%token	GT_SYM
%token	LE
%token	LT
%token	NE
%token	IS
%token	SHIFT_LEFT
%token	SHIFT_RIGHT
%token  SET_VAR

%token	AVG_SYM
%token	COUNT_SYM
%token	MAX_SYM
%token	MIN_SYM
%token	SUM_SYM
%token	STD_SYM

%token	ADD
%token	ALTER
%token	AFTER_SYM
%token  ANALYZE_SYM
%token  BEGIN_SYM
%token	CHANGE
%token  COMMENT_SYM
%token  COMMIT_SYM
%token	CREATE
%token	CROSS
%token	DELETE_SYM
%token	DROP
%token	INSERT
%token	FLUSH_SYM
%token	SELECT_SYM
%token  MASTER_SYM
%token	REPAIR
%token  RESET_SYM
%token  PURGE
%token  SLAVE
%token  START_SYM
%token  STOP_SYM
%token	TRUNCATE_SYM
%token  ROLLBACK_SYM
%token	OPTIMIZE
%token	SHOW
%token	UPDATE_SYM
%token	KILL_SYM
%token	LOAD
%token	LOCK_SYM
%token	UNLOCK_SYM

%token	ACTION
%token	AGGREGATE_SYM
%token	ALL
%token	AND
%token	AS
%token	ASC
%token	AUTO_INC
%token	AUTOCOMMIT
%token	AVG_ROW_LENGTH
%token  BACKUP_SYM
%token	BERKELEY_DB_SYM
%token	BINARY
%token	BIT_SYM
%token	BOOL_SYM
%token	BOTH
%token	BY
%token	CASCADE
%token	CHECKSUM_SYM
%token	CHECK_SYM
%token	COMMITTED_SYM
%token	COLUMNS
%token	COLUMN_SYM
%token	CONSTRAINT
%token	DATABASES
%token	DATA_SYM
%token	DEFAULT
%token	DELAYED_SYM
%token	DELAY_KEY_WRITE_SYM
%token	DESC
%token	DESCRIBE
%token	DISTINCT
%token	DYNAMIC_SYM
%token	ENCLOSED
%token	ESCAPED
%token	ESCAPE_SYM
%token	EXISTS
%token	EXTENDED_SYM
%token	FILE_SYM
%token	FIRST_SYM
%token	FIXED_SYM
%token	FLOAT_NUM
%token	FOREIGN
%token	FROM
%token	FULL
%token  FULLTEXT_SYM
%token  GEMINI_SYM
%token	GEMINI_SPIN_RETRIES
%token  GLOBAL_SYM
%token	GRANT
%token	GRANTS
%token	GREATEST_SYM
%token	GROUP
%token	HAVING
%token	HEAP_SYM
%token	HEX_NUM
%token	HIGH_PRIORITY
%token	HOSTS_SYM
%token	IDENT
%token	IGNORE_SYM
%token	INDEX
%token	INFILE
%token	INNER_SYM
%token	INNOBASE_SYM
%token	INTO
%token	IN_SYM
%token  ISOLATION
%token	ISAM_SYM
%token	JOIN_SYM
%token	KEYS
%token	KEY_SYM
%token	LEADING
%token	LEAST_SYM
%token  LEVEL_SYM
%token	LEX_HOSTNAME
%token	LIKE
%token	LINES
%token	LOCAL_SYM
%token	LOGS_SYM
%token	LONG_NUM
%token	LONG_SYM
%token	LOW_PRIORITY
%token  MASTER_HOST_SYM
%token  MASTER_USER_SYM
%token  MASTER_LOG_FILE_SYM
%token  MASTER_LOG_POS_SYM
%token  MASTER_PASSWORD_SYM
%token  MASTER_PORT_SYM
%token  MASTER_CONNECT_RETRY_SYM
%token	MATCH
%token	MAX_ROWS
%token	MEDIUM_SYM
%token	MERGE_SYM
%token	MIN_ROWS
%token	MYISAM_SYM
%token	NATIONAL_SYM
%token	NATURAL
%token	NCHAR_SYM
%token	NOT
%token	NO_SYM
%token	NULL_SYM
%token	NUM
%token	ON
%token	OPEN_SYM
%token	OPTION
%token	OPTIONALLY
%token	OR
%token	OR_OR_CONCAT
%token	ORDER_SYM
%token	OUTER
%token	OUTFILE
%token  DUMPFILE
%token	PACK_KEYS_SYM
%token	PARTIAL
%token	PRIMARY_SYM
%token	PRIVILEGES
%token	PROCESS
%token	PROCESSLIST_SYM
%token	RAID_0_SYM
%token	RAID_STRIPED_SYM
%token	RAID_TYPE
%token	RAID_CHUNKS
%token	RAID_CHUNKSIZE
%token	READ_SYM
%token	REAL_NUM
%token	REFERENCES
%token	REGEXP
%token	RELOAD
%token	RENAME
%token	REPEATABLE_SYM
%token  RESTORE_SYM
%token	RESTRICT
%token	REVOKE
%token	ROWS_SYM
%token	ROW_FORMAT_SYM
%token	ROW_SYM
%token	SET
%token	SERIALIZABLE_SYM
%token	SESSION_SYM
%token	SHUTDOWN
%token	STARTING
%token	STATUS_SYM
%token	STRAIGHT_JOIN
%token	TABLES
%token	TABLE_SYM
%token	TEMPORARY
%token	TERMINATED
%token	TEXT_STRING
%token	TO_SYM
%token	TRAILING
%token	TRANSACTION_SYM
%token	TYPE_SYM
%token	FUNC_ARG0
%token	FUNC_ARG1
%token	FUNC_ARG2
%token	FUNC_ARG3
%token	UDF_RETURNS_SYM
%token	UDF_SONAME_SYM
%token	UDF_SYM
%token  UNCOMMITTED_SYM
%token	UNION_SYM
%token	UNIQUE_SYM
%token	USAGE
%token	USE_SYM
%token	USING
%token	VALUES
%token	VARIABLES
%token	WHERE
%token	WITH
%token	WRITE_SYM
%token  COMPRESSED_SYM

%token	BIGINT
%token	BLOB_SYM
%token	CHAR_SYM
%token  CHANGED
%token	COALESCE
%token	DATETIME
%token	DATE_SYM
%token	DECIMAL_SYM
%token	DOUBLE_SYM
%token	ENUM
%token	FAST_SYM
%token	FLOAT_SYM
%token	INT_SYM
%token	LIMIT
%token	LONGBLOB
%token	LONGTEXT
%token	MEDIUMBLOB
%token	MEDIUMINT
%token	MEDIUMTEXT
%token	NUMERIC_SYM
%token	PRECISION
%token  QUICK
%token	REAL
%token	SMALLINT
%token	STRING_SYM
%token	TEXT_SYM
%token	TIMESTAMP
%token	TIME_SYM
%token	TINYBLOB
%token	TINYINT
%token	TINYTEXT
%token	UNSIGNED
%token	VARBINARY
%token	VARCHAR
%token	VARYING
%token	ZEROFILL

%token  AGAINST
%token	ATAN
%token	BETWEEN_SYM
%token	BIT_AND
%token	BIT_OR
%token	CASE_SYM
%token	CONCAT
%token  CONCAT_WS
%token	CURDATE
%token	CURTIME
%token	DATABASE
%token	DATE_ADD_INTERVAL
%token	DATE_SUB_INTERVAL
%token	DAY_HOUR_SYM
%token	DAY_MINUTE_SYM
%token	DAY_SECOND_SYM
%token	DAY_SYM
%token	DECODE_SYM
%token	ELSE
%token	ELT_FUNC
%token	ENCODE_SYM
%token	ENCRYPT
%token	EXPORT_SET
%token	EXTRACT_SYM
%token	FIELD_FUNC
%token	FORMAT_SYM
%token	FOR_SYM
%token	FROM_UNIXTIME
%token	GROUP_UNIQUE_USERS
%token	HOUR_MINUTE_SYM
%token	HOUR_SECOND_SYM
%token	HOUR_SYM
%token	IDENTIFIED_SYM
%token	IF
%token	INSERT_ID
%token	INTERVAL_SYM
%token	LAST_INSERT_ID
%token	LEFT
%token	LOCATE
%token	MAKE_SET_SYM
%token	MINUTE_SECOND_SYM
%token	MINUTE_SYM
%token  MODE_SYM
%token	MODIFY_SYM
%token	MONTH_SYM
%token	NOW_SYM
%token	PASSWORD
%token	POSITION_SYM
%token	PROCEDURE
%token	RAND
%token	REPLACE
%token	RIGHT
%token	ROUND
%token	SECOND_SYM
%token	SHARE_SYM
%token	SUBSTRING
%token	SUBSTRING_INDEX
%token	TRIM
%token	UDA_CHAR_SUM
%token	UDA_FLOAT_SUM
%token	UDA_INT_SUM
%token	UDF_CHAR_FUNC
%token	UDF_FLOAT_FUNC
%token	UDF_INT_FUNC
%token	UNIQUE_USERS
%token	UNIX_TIMESTAMP
%token	USER
%token	WEEK_SYM
%token	WHEN_SYM
%token  WORK_SYM
%token	YEAR_MONTH_SYM
%token	YEAR_SYM
%token	YEARWEEK
%token  BENCHMARK_SYM
%token  END
%token  THEN_SYM

%token	SQL_BIG_TABLES
%token	SQL_BIG_SELECTS
%token	SQL_SELECT_LIMIT
%token	SQL_MAX_JOIN_SIZE
%token	SQL_LOG_BIN
%token	SQL_LOG_OFF
%token	SQL_LOG_UPDATE
%token	SQL_LOW_PRIORITY_UPDATES
%token	SQL_SMALL_RESULT
%token	SQL_BIG_RESULT
%token  SQL_BUFFER_RESULT
%token	SQL_WARNINGS
%token	SQL_AUTO_IS_NULL
%token	SQL_SAFE_UPDATES
%token  SQL_QUOTE_SHOW_CREATE
%token  SQL_SLAVE_SKIP_COUNTER

%left   SET_VAR
%left	OR_OR_CONCAT OR
%left	AND
%left	BETWEEN_SYM CASE_SYM WHEN_SYM THEN_SYM ELSE
%left	EQ EQUAL_SYM GE GT_SYM LE LT NE IS LIKE REGEXP IN_SYM
%left	'|'
%left	'&'
%left	SHIFT_LEFT SHIFT_RIGHT
%left	'-' '+'
%left	'*' '/' '%'
%left	NEG '~'
%right	NOT
%right	BINARY

%type <lex_str>
	IDENT TEXT_STRING REAL_NUM FLOAT_NUM NUM LONG_NUM HEX_NUM LEX_HOSTNAME
	field_ident select_alias ident ident_or_text

%type <lex_str_ptr>
	opt_table_alias

%type <table>
	table_ident

%type <simple_string>
	remember_name remember_end opt_len opt_ident opt_db text_or_password
	opt_escape

%type <string>
	text_string

%type <num>
	type int_type real_type order_dir opt_field_spec set_option lock_option
	udf_type if_exists opt_local opt_table_options table_options
	table_option opt_if_not_exists

%type <ulong_num>
	ULONG_NUM raid_types

%type <ulonglong_num>
	ULONGLONG_NUM

%type <item>
	literal text_literal insert_ident group_ident order_ident
	simple_ident select_item2 expr opt_expr opt_else sum_expr in_sum_expr
	table_wild opt_pad no_in_expr expr_expr simple_expr no_and_expr
	using_list

%type <item_list>
	expr_list udf_expr_list when_list ident_list

%type <key_type>
	key_type opt_unique_or_fulltext

%type <string_list>
	key_usage_list

%type <key_part>
	key_part

%type <table_list>
	join_table_list join_table

%type <udf>
	UDF_CHAR_FUNC UDF_FLOAT_FUNC UDF_INT_FUNC
	UDA_CHAR_SUM UDA_FLOAT_SUM UDA_INT_SUM

%type <interval> interval

%type <db_type> table_types

%type <row_type> row_types

%type <tx_isolation> tx_isolation isolation_types

%type <udf_type> udf_func_type

%type <symbol> FUNC_ARG0 FUNC_ARG1 FUNC_ARG2 FUNC_ARG3 keyword

%type <lex_user> user grant_user

%type <NONE>
	query verb_clause create change select drop insert replace insert2
	insert_values update delete truncate rename
	show describe load alter optimize flush
	reset purge begin commit rollback slave master_def master_defs
	repair restore backup analyze check 
	field_list field_list_item field_spec kill
	select_item_list select_item values_list no_braces
	limit_clause delete_limit_clause fields opt_values values
	procedure_list procedure_list2 procedure_item
        when_list2 expr_list2
	opt_precision opt_ignore opt_column opt_restrict
	grant revoke set lock unlock string_list field_options field_option
	field_opt_list opt_binary table_lock_list table_lock varchar
	references opt_on_delete opt_on_delete_list opt_on_delete_item use
	opt_delete_options opt_delete_option
	opt_outer table_list table opt_option opt_place opt_low_priority
	opt_attribute opt_attribute_list attribute column_list column_list_id
	opt_column_list grant_privileges opt_table user_list grant_option
	grant_privilege grant_privilege_list
	flush_options flush_option insert_lock_option replace_lock_option
	equal optional_braces opt_key_definition key_usage_list2
	opt_mi_check_type opt_to mi_check_types normal_join
	table_to_table_list table_to_table opt_table_list opt_as
	END_OF_INPUT

%type <NONE>
	'-' '+' '*' '/' '%' '(' ')'
	',' '!' '{' '}' '&' '|' AND OR OR_OR_CONCAT BETWEEN_SYM CASE_SYM THEN_SYM WHEN_SYM
%%


query:
	END_OF_INPUT
	 {
	   if (!current_thd->bootstrap)
	     send_error(&current_thd->net,ER_EMPTY_QUERY);
	   YYABORT;
	}
	| verb_clause END_OF_INPUT {}

verb_clause:
	  alter
	| analyze
	| backup
	| begin
	| change
	| check
	| commit
	| create
	| delete
	| describe
	| drop
	| grant
	| insert
	| flush
	| load
	| lock
	| kill
	| optimize
	| purge  
	| rename
        | repair
	| replace
	| reset
	| restore
	| revoke
	| rollback
	| select
	| set
	| slave
	| show
	| truncate
	| unlock
	| update
	| use

/* change master */

change:
       CHANGE MASTER_SYM TO_SYM
        {
	  LEX *lex = Lex;
	  lex->sql_command = SQLCOM_CHANGE_MASTER;
	  memset(&lex->mi, 0, sizeof(lex->mi));
        } master_defs

master_defs:
       master_def
       |
       master_defs ',' master_def

master_def:
       MASTER_HOST_SYM EQ TEXT_STRING
       {
	 Lex->mi.host = $3.str;
       }
       |
       MASTER_USER_SYM EQ TEXT_STRING
       {
	 Lex->mi.user = $3.str;
       }
       |
       MASTER_PASSWORD_SYM EQ TEXT_STRING
       {
	 Lex->mi.password = $3.str;
       }
       |
       MASTER_LOG_FILE_SYM EQ TEXT_STRING
       {
	 Lex->mi.log_file_name = $3.str;
       }
       |
       MASTER_PORT_SYM EQ ULONG_NUM
       {
	 Lex->mi.port = $3;
       }
       |
       MASTER_LOG_POS_SYM EQ ULONGLONG_NUM
       {
	 Lex->mi.pos = $3;
       }
       |
       MASTER_CONNECT_RETRY_SYM EQ ULONG_NUM
       {
	 Lex->mi.connect_retry = $3;
       }



/* create a table */

create:
	CREATE opt_table_options TABLE_SYM opt_if_not_exists table_ident
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_CREATE_TABLE;
	  if (!add_table_to_list($5,
				 ($2 & HA_LEX_CREATE_TMP_TABLE ?
				   &tmp_table_alias : (LEX_STRING*) 0),1))
	    YYABORT;
	  lex->create_list.empty();
	  lex->key_list.empty();
	  lex->col_list.empty();
	  lex->change=NullS;
	  bzero((char*) &lex->create_info,sizeof(lex->create_info));
	  lex->create_info.options=$2 | $4;
	  lex->create_info.db_type= default_table_type;
	}
	create2

	| CREATE opt_unique_or_fulltext INDEX ident ON table_ident
	  {
	    Lex->sql_command= SQLCOM_CREATE_INDEX;
	    if (!add_table_to_list($6,NULL,1))
	      YYABORT;
	    Lex->create_list.empty();
	    Lex->key_list.empty();
	    Lex->col_list.empty();
	    Lex->change=NullS;
	  }
	  '(' key_list ')'
	  {
	    Lex->key_list.push_back(new Key($2,$4.str,Lex->col_list));
	    Lex->col_list.empty();
	  }
	| CREATE DATABASE opt_if_not_exists ident
	  {
	    Lex->sql_command=SQLCOM_CREATE_DB;
	    Lex->name=$4.str;
            Lex->create_info.options=$3;
	  }
	| CREATE udf_func_type UDF_SYM ident
	  {
	    Lex->sql_command = SQLCOM_CREATE_FUNCTION;
	    Lex->udf.name=$4.str;
	    Lex->udf.name_length=$4.length;
	    Lex->udf.type= $2;
	  }
	  UDF_RETURNS_SYM udf_type UDF_SONAME_SYM TEXT_STRING
	  {
	    Lex->udf.returns=(Item_result) $7;
	    Lex->udf.dl=$9.str;
	  }

create2:
	'(' field_list ')' opt_create_table_options create3 {}
	| opt_create_table_options create3 {}

create3:
	/* empty */ {}
	| opt_duplicate opt_as SELECT_SYM
          {
	    mysql_init_select(Lex);
          }
          select_options select_item_list opt_select_from {}

opt_as:
	/* empty */ {}
	| AS	    {}

opt_table_options:
	/* empty */	 { $$= 0; }
	| table_options  { $$= $1;}

table_options:
	table_option	{ $$=$1; }
	| table_option table_options { $$= $1 | $2 }

table_option:
	TEMPORARY	{ $$=HA_LEX_CREATE_TMP_TABLE; }

opt_if_not_exists:
	/* empty */	 { $$= 0; }
	| IF NOT EXISTS	 { $$=HA_LEX_CREATE_IF_NOT_EXISTS; }

opt_create_table_options:
	/* empty */
	| create_table_options

create_table_options:
	create_table_option
	| create_table_option create_table_options

create_table_option:
	TYPE_SYM EQ table_types		{ Lex->create_info.db_type= $3; }
	| MAX_ROWS EQ ULONGLONG_NUM	{ Lex->create_info.max_rows= $3; }
	| MIN_ROWS EQ ULONGLONG_NUM	{ Lex->create_info.min_rows= $3; }
	| AVG_ROW_LENGTH EQ ULONG_NUM	{ Lex->create_info.avg_row_length=$3; }
	| PASSWORD EQ TEXT_STRING	{ Lex->create_info.password=$3.str; }
	| COMMENT_SYM EQ TEXT_STRING	{ Lex->create_info.comment=$3.str; }
	| AUTO_INC EQ ULONGLONG_NUM	{ Lex->create_info.auto_increment_value=$3; Lex->create_info.used_fields|= HA_CREATE_USED_AUTO;}
	| PACK_KEYS_SYM EQ ULONG_NUM	{ Lex->create_info.table_options|= $3 ? HA_OPTION_PACK_KEYS : HA_OPTION_NO_PACK_KEYS; }
	| CHECKSUM_SYM EQ ULONG_NUM	{ Lex->create_info.table_options|= $3 ? HA_OPTION_CHECKSUM : HA_OPTION_NO_CHECKSUM; }
	| DELAY_KEY_WRITE_SYM EQ ULONG_NUM { Lex->create_info.table_options|= $3 ? HA_OPTION_DELAY_KEY_WRITE : HA_OPTION_NO_DELAY_KEY_WRITE; }
	| ROW_FORMAT_SYM EQ row_types	{ Lex->create_info.row_type= $3; }
	| RAID_TYPE EQ raid_types	{ Lex->create_info.raid_type= $3; Lex->create_info.used_fields|= HA_CREATE_USED_RAID;}
	| RAID_CHUNKS EQ ULONG_NUM	{ Lex->create_info.raid_chunks= $3; Lex->create_info.used_fields|= HA_CREATE_USED_RAID;}
	| RAID_CHUNKSIZE EQ ULONG_NUM	{ Lex->create_info.raid_chunksize= $3*RAID_BLOCK_SIZE; Lex->create_info.used_fields|= HA_CREATE_USED_RAID;}
	| UNION_SYM EQ '(' table_list ')'
	  {
	    /* Move the union list to the merge_list */
	    LEX *lex=Lex;
	    TABLE_LIST *table_list= (TABLE_LIST*) lex->table_list.first;
	    lex->create_info.merge_list= lex->table_list;
	    lex->create_info.merge_list.elements--;
	    lex->create_info.merge_list.first= (byte*) (table_list->next);
	    lex->table_list.elements=1;
	    lex->table_list.next= (byte**) &(table_list->next);
	    table_list->next=0;
	    lex->create_info.used_fields|= HA_CREATE_USED_UNION;
	  }

table_types:
	ISAM_SYM	{ $$= DB_TYPE_ISAM; }
	| MYISAM_SYM	{ $$= DB_TYPE_MYISAM; }
	| MERGE_SYM	{ $$= DB_TYPE_MRG_MYISAM; }
	| HEAP_SYM	{ $$= DB_TYPE_HEAP; }
	| BERKELEY_DB_SYM { $$= DB_TYPE_BERKELEY_DB; }
	| INNOBASE_SYM  { $$= DB_TYPE_INNOBASE; }
	| GEMINI_SYM    { $$= DB_TYPE_GEMINI; }

row_types:
	DEFAULT		{ $$= ROW_TYPE_DEFAULT; }
	| FIXED_SYM	{ $$= ROW_TYPE_FIXED; }
	| DYNAMIC_SYM	{ $$= ROW_TYPE_DYNAMIC; }
	| COMPRESSED_SYM { $$= ROW_TYPE_COMPRESSED; }

raid_types:
	RAID_STRIPED_SYM { $$= RAID_TYPE_0; }
	| RAID_0_SYM	 { $$= RAID_TYPE_0; }
	| ULONG_NUM	 { $$=$1;}

opt_select_from:
	/* empty */
	| select_from

udf_func_type:
	/* empty */ 	{ $$ = UDFTYPE_FUNCTION; }
	| AGGREGATE_SYM { $$ = UDFTYPE_AGGREGATE; }

udf_type:
	STRING_SYM {$$ = (int) STRING_RESULT; }
	| REAL {$$ = (int) REAL_RESULT; }
	| INT_SYM {$$ = (int) INT_RESULT; }

field_list:
	  field_list_item
	| field_list ',' field_list_item


field_list_item:
	  field_spec
	| field_spec references
	  {
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	| key_type opt_ident '(' key_list ')'
	  {
	    Lex->key_list.push_back(new Key($1,$2,Lex->col_list));
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	| opt_constraint FOREIGN KEY_SYM opt_ident '(' key_list ')' references
	  {
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }
	| opt_constraint CHECK_SYM '(' expr ')'
	  {
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }

opt_constraint:
	/* empty */
	| CONSTRAINT opt_ident

field_spec:
	field_ident
	 {
	   Lex->length=Lex->dec=0; Lex->type=0; Lex->interval=0;
	   Lex->default_value=0;
	 }
	type opt_attribute
	{
	  if (add_field_to_list($1.str,
				(enum enum_field_types) $3,
				Lex->length,Lex->dec,Lex->type,
				Lex->default_value,Lex->change,
				Lex->interval))
	    YYABORT;
	}

type:
	int_type opt_len field_options	{ Lex->length=$2; $$=$1; }
	| real_type opt_precision field_options { $$=$1; }
	| FLOAT_SYM float_options field_options { $$=FIELD_TYPE_FLOAT; }
	| BIT_SYM opt_len		{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_TINY; }
	| BOOL_SYM			{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_TINY; }
	| char '(' NUM ')' opt_binary { Lex->length=$3.str;
					  $$=FIELD_TYPE_STRING; }
	| char opt_binary		{ Lex->length=(char*) "1";
					  $$=FIELD_TYPE_STRING; }
	| BINARY '(' NUM ')' 		{ Lex->length=$3.str;
					  Lex->type|=BINARY_FLAG;
					  $$=FIELD_TYPE_STRING; }
	| varchar '(' NUM ')' opt_binary { Lex->length=$3.str;
					  $$=FIELD_TYPE_VAR_STRING; }
	| VARBINARY '(' NUM ')' 	{ Lex->length=$3.str;
					  Lex->type|=BINARY_FLAG;
					  $$=FIELD_TYPE_VAR_STRING; }
	| YEAR_SYM opt_len field_options { $$=FIELD_TYPE_YEAR; Lex->length=$2; }
	| DATE_SYM			{ $$=FIELD_TYPE_DATE; }
	| TIME_SYM			{ $$=FIELD_TYPE_TIME; }
	| TIMESTAMP			{ $$=FIELD_TYPE_TIMESTAMP; }
	| TIMESTAMP '(' NUM ')'		{ Lex->length=$3.str;
					  $$=FIELD_TYPE_TIMESTAMP; }
	| DATETIME			{ $$=FIELD_TYPE_DATETIME; }
	| TINYBLOB			{ Lex->type|=BINARY_FLAG;
					  $$=FIELD_TYPE_TINY_BLOB; }
	| BLOB_SYM			{ Lex->type|=BINARY_FLAG;
					  $$=FIELD_TYPE_BLOB; }
	| MEDIUMBLOB			{ Lex->type|=BINARY_FLAG;
					  $$=FIELD_TYPE_MEDIUM_BLOB; }
	| LONGBLOB			{ Lex->type|=BINARY_FLAG;
					  $$=FIELD_TYPE_LONG_BLOB; }
	| LONG_SYM VARBINARY		{ Lex->type|=BINARY_FLAG;
					  $$=FIELD_TYPE_MEDIUM_BLOB; }
	| LONG_SYM varchar		{ $$=FIELD_TYPE_MEDIUM_BLOB; }
	| TINYTEXT			{ $$=FIELD_TYPE_TINY_BLOB; }
	| TEXT_SYM			{ $$=FIELD_TYPE_BLOB; }
	| MEDIUMTEXT			{ $$=FIELD_TYPE_MEDIUM_BLOB; }
	| LONGTEXT			{ $$=FIELD_TYPE_LONG_BLOB; }
	| DECIMAL_SYM float_options field_options
					{ $$=FIELD_TYPE_DECIMAL;}
	| NUMERIC_SYM float_options field_options
					{ $$=FIELD_TYPE_DECIMAL;}
	| ENUM {Lex->interval_list.empty();} '(' string_list ')'
	  {
	    Lex->interval=typelib(Lex->interval_list);
	    $$=FIELD_TYPE_ENUM;
	  }
	| SET { Lex->interval_list.empty();} '(' string_list ')'
	  {
	    Lex->interval=typelib(Lex->interval_list);
	    $$=FIELD_TYPE_SET;
	  }

char:
	CHAR_SYM {}
	| NCHAR_SYM {}
	| NATIONAL_SYM CHAR_SYM {}

varchar:
	char VARYING {}
	| VARCHAR {}
	| NATIONAL_SYM VARCHAR {}
	| NCHAR_SYM VARCHAR {}

int_type:
	INT_SYM		{ $$=FIELD_TYPE_LONG; }
	| TINYINT	{ $$=FIELD_TYPE_TINY; }
	| SMALLINT	{ $$=FIELD_TYPE_SHORT; }
	| MEDIUMINT	{ $$=FIELD_TYPE_INT24; }
	| BIGINT	{ $$=FIELD_TYPE_LONGLONG; }

real_type:
	REAL		{ $$= current_thd->options & OPTION_ANSI_MODE ?
			      FIELD_TYPE_FLOAT : FIELD_TYPE_DOUBLE; }
	| DOUBLE_SYM	{ $$=FIELD_TYPE_DOUBLE; }
	| DOUBLE_SYM PRECISION { $$=FIELD_TYPE_DOUBLE; }


float_options:
	/* empty */		{}
	| '(' NUM ')'		{ Lex->length=$2.str; }
	| '(' NUM ',' NUM ')'	{ Lex->length=$2.str; Lex->dec=$4.str; }

field_options:
	/* empty */		{}
	| field_opt_list	{}

field_opt_list:
	field_opt_list field_option {}
	| field_option {}

field_option:
	UNSIGNED	{ Lex->type|= UNSIGNED_FLAG;}
	| ZEROFILL	{ Lex->type|= UNSIGNED_FLAG | ZEROFILL_FLAG; }

opt_len:
	/* empty */	{ $$=(char*) 0; }	/* use default length */
	| '(' NUM ')'	{ $$=$2.str; }

opt_precision:
	/* empty */	{}
	| '(' NUM ',' NUM ')'	{ Lex->length=$2.str; Lex->dec=$4.str; }

opt_attribute:
	/* empty */ {}
	| opt_attribute_list {}

opt_attribute_list:
	opt_attribute_list attribute {}
	| attribute

attribute:
	NULL_SYM	  { Lex->type&= ~ NOT_NULL_FLAG; }
	| NOT NULL_SYM	  { Lex->type|= NOT_NULL_FLAG; }
	| DEFAULT literal { Lex->default_value=$2; }
	| AUTO_INC	  { Lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG; }
	| PRIMARY_SYM KEY_SYM { Lex->type|= PRI_KEY_FLAG | NOT_NULL_FLAG; }
	| UNIQUE_SYM	  { Lex->type|= UNIQUE_FLAG; }
	| UNIQUE_SYM KEY_SYM { Lex->type|= UNIQUE_KEY_FLAG; }

opt_binary:
	/* empty */	{}
	| BINARY	{ Lex->type|=BINARY_FLAG; }

references:
	REFERENCES table_ident opt_on_delete {}
	| REFERENCES table_ident '(' key_list ')' opt_on_delete
	  {
	    Lex->col_list.empty();		/* Alloced by sql_alloc */
	  }

opt_on_delete:
	/* empty */ {}
	| opt_on_delete_list {}

opt_on_delete_list:
	opt_on_delete_list opt_on_delete_item {}
	| opt_on_delete_item {}


opt_on_delete_item:
	ON DELETE_SYM delete_option {}
	| ON UPDATE_SYM delete_option {}
	| MATCH FULL	{}
	| MATCH PARTIAL {}

delete_option:
	RESTRICT	 {}
	| CASCADE	 {}
	| SET NULL_SYM {}
	| NO_SYM ACTION {}
	| SET DEFAULT {}

key_type:
	opt_constraint PRIMARY_SYM KEY_SYM  { $$= Key::PRIMARY; }
	| key_or_index			    { $$= Key::MULTIPLE; }
	| FULLTEXT_SYM			    { $$= Key::FULLTEXT; }
	| FULLTEXT_SYM key_or_index	    { $$= Key::FULLTEXT; }
	| opt_constraint UNIQUE_SYM	    { $$= Key::UNIQUE; }
	| opt_constraint UNIQUE_SYM key_or_index { $$= Key::UNIQUE; }

key_or_index:
	KEY_SYM {}
	| INDEX {}

keys_or_index:
	KEYS {}
	| INDEX {}

opt_unique_or_fulltext:
	/* empty */	{ $$= Key::MULTIPLE; }
	| UNIQUE_SYM	{ $$= Key::UNIQUE; }
	| FULLTEXT_SYM	{ $$= Key::FULLTEXT; }

key_list:
	key_list ',' key_part order_dir { Lex->col_list.push_back($3); }
	| key_part order_dir		{ Lex->col_list.push_back($1); }

key_part:
	ident			{ $$=new key_part_spec($1.str); }
	| ident '(' NUM ')'	{ $$=new key_part_spec($1.str,(uint) atoi($3.str)); }

opt_ident:
	/* empty */	{ $$=(char*) 0; }	/* Defaultlength */
	| field_ident	{ $$=$1.str; }

string_list:
	text_string			{ Lex->interval_list.push_back($1); }
	| string_list ',' text_string	{ Lex->interval_list.push_back($3); }

/*
** Alter table
*/

alter:
	ALTER opt_ignore TABLE_SYM table_ident
	{
	  LEX *lex=Lex;
	  lex->sql_command = SQLCOM_ALTER_TABLE;
	  lex->name=0;
	  if (!add_table_to_list($4, NULL,1))
	    YYABORT;
	  lex->drop_primary=0;
	  lex->create_list.empty();
	  lex->key_list.empty();
	  lex->col_list.empty();
	  lex->drop_list.empty();
	  lex->alter_list.empty();
          lex->order_list.elements=0;
          lex->order_list.first=0;
          lex->order_list.next= (byte**) &lex->order_list.first;
	  lex->db=lex->name=0;
    	  bzero((char*) &lex->create_info,sizeof(lex->create_info));
	  lex->create_info.db_type= DB_TYPE_DEFAULT;
	}
	alter_list
 
alter_list:
        | alter_list_item
	| alter_list ',' alter_list_item

add_column:
	ADD opt_column { Lex->change=0;}

alter_list_item:
	add_column field_list_item opt_place
	| add_column '(' field_list ')'
	| CHANGE opt_column field_ident { Lex->change= $3.str; } field_spec
	| MODIFY_SYM opt_column field_ident
	  {
	    Lex->length=Lex->dec=0; Lex->type=0; Lex->interval=0;
	    Lex->default_value=0;
	  }
	  type opt_attribute
	  {
	    if (add_field_to_list($3.str,
				  (enum enum_field_types) $5,
				  Lex->length,Lex->dec,Lex->type,
				  Lex->default_value, $3.str,
				  Lex->interval))
	     YYABORT;
	  }
	| DROP opt_column field_ident opt_restrict
	  { Lex->drop_list.push_back(new Alter_drop(Alter_drop::COLUMN,
						    $3.str)); }
	| DROP PRIMARY_SYM KEY_SYM { Lex->drop_primary=1; }
	| DROP FOREIGN KEY_SYM opt_ident {}
	| DROP key_or_index field_ident
	  { Lex->drop_list.push_back(new Alter_drop(Alter_drop::KEY,
						    $3.str)); }
	| ALTER opt_column field_ident SET DEFAULT literal
	  { Lex->alter_list.push_back(new Alter_column($3.str,$6)); }
	| ALTER opt_column field_ident DROP DEFAULT
	  { Lex->alter_list.push_back(new Alter_column($3.str,(Item*) 0)); }
	| RENAME opt_to table_alias table_ident
	  { Lex->db=$4->db.str ; Lex->name= $4->table.str; }
        | create_table_options
	| order_clause

opt_column:
	/* empty */	{}
	| COLUMN_SYM	{}

opt_ignore:
	/* empty */	{ Lex->duplicates=DUP_ERROR; }
	| IGNORE_SYM	{ Lex->duplicates=DUP_IGNORE; }

opt_restrict:
	/* empty */	{}
	| RESTRICT	{}
	| CASCADE	{}

opt_place:
	/* empty */	{}
	| AFTER_SYM ident { store_position_for_column($2.str); }
	| FIRST_SYM	  { store_position_for_column(first_keyword); }

opt_to:
	/* empty */	{}
	| TO_SYM	{}
	| AS		{}

slave:
	SLAVE START_SYM
         {
           Lex->sql_command = SQLCOM_SLAVE_START;
	   Lex->type = 0;
         }
         |
	SLAVE STOP_SYM
         {
           Lex->sql_command = SQLCOM_SLAVE_STOP;
	   Lex->type = 0;
         };

restore:
	RESTORE_SYM table_or_tables
	{
	   Lex->sql_command = SQLCOM_RESTORE_TABLE;
	}
	table_list FROM TEXT_STRING
        {
	  Lex->backup_dir = $6.str;
        }
backup:
	BACKUP_SYM table_or_tables
	{
	   Lex->sql_command = SQLCOM_BACKUP_TABLE;
	}
	table_list TO_SYM TEXT_STRING
        {
	  Lex->backup_dir = $6.str;
        }


repair:
	REPAIR table_or_tables
	{
	   Lex->sql_command = SQLCOM_REPAIR;
	   Lex->check_opt.init();
	}
	table_list opt_mi_check_type


opt_mi_check_type:
	/* empty */ { Lex->check_opt.flags = T_MEDIUM; }
	| TYPE_SYM EQ mi_check_types {}
	| mi_check_types {}

mi_check_types:
	mi_check_type {}
	| mi_check_type mi_check_types {}

mi_check_type:
	QUICK      { Lex->check_opt.quick = 1; }
	| FAST_SYM { Lex->check_opt.flags|= T_FAST; }
	| MEDIUM_SYM { Lex->check_opt.flags|= T_MEDIUM; }
	| EXTENDED_SYM { Lex->check_opt.flags|= T_EXTEND; }
	| CHANGED  { Lex->check_opt.flags|= T_CHECK_ONLY_CHANGED; }

analyze:
	ANALYZE_SYM table_or_tables
	{
	   Lex->sql_command = SQLCOM_ANALYZE;
	   Lex->check_opt.init();
	}
	table_list opt_mi_check_type

check:
	CHECK_SYM table_or_tables
	{
	   Lex->sql_command = SQLCOM_CHECK;
	   Lex->check_opt.init();
	}
	table_list opt_mi_check_type

optimize:
	OPTIMIZE table_or_tables
	{
	   Lex->sql_command = SQLCOM_OPTIMIZE;
	   Lex->check_opt.init();
	}
	table_list opt_mi_check_type

rename:
	RENAME table_or_tables
	{
	   Lex->sql_command=SQLCOM_RENAME_TABLE;
	}
	table_to_table_list

table_to_table_list:
	table_to_table
	| table_to_table_list ',' table_to_table

table_to_table:
	table_ident TO_SYM table_ident
	{ if (!add_table_to_list($1,NULL,1,TL_IGNORE) ||
	      !add_table_to_list($3,NULL,1,TL_IGNORE))
	     YYABORT;
 	}

/*
** Select : retrieve data from table
*/


select:
	SELECT_SYM
	{
	  LEX *lex=Lex;
	  lex->sql_command= SQLCOM_SELECT;
	  lex->lock_option=TL_READ;
	  mysql_init_select(lex);
	}
	select_options select_item_list select_into select_lock_type

select_into:
	/* empty */
	| select_from
	| opt_into select_from
	| select_from opt_into

select_from:
	FROM join_table_list where_clause group_clause having_clause opt_order_clause limit_clause procedure_clause


select_options:
	/* empty*/
	| select_option_list

select_option_list:
	select_option_list select_option
	| select_option

select_option:
	STRAIGHT_JOIN { Lex->options|= SELECT_STRAIGHT_JOIN; }
	| HIGH_PRIORITY { Lex->lock_option= TL_READ_HIGH_PRIORITY; }
	| DISTINCT	{ Lex->options|= SELECT_DISTINCT; }
	| SQL_SMALL_RESULT { Lex->options|= SELECT_SMALL_RESULT; }
	| SQL_BIG_RESULT { Lex->options|= SELECT_BIG_RESULT; }
	| SQL_BUFFER_RESULT { Lex->options|= OPTION_BUFFER_RESULT; }
	| ALL		{}

select_lock_type:
	/* empty */
	| FOR_SYM UPDATE_SYM
	  { Lex->lock_option= TL_WRITE; }
	| IN_SYM SHARE_SYM MODE_SYM
	  { Lex->lock_option= TL_READ_WITH_SHARED_LOCKS; }

select_item_list:
	  select_item_list ',' select_item
	| select_item
	| '*'
	  {
	    if (add_item_to_list(new Item_field(NULL,NULL,"*")))
	      YYABORT;
	  }


select_item:
	  remember_name select_item2 remember_end select_alias
	  {
	    if (add_item_to_list($2))
	      YYABORT;
	    if ($4.str)
	      $2->set_name($4.str);
	    else if (!$2->name)
	      $2->set_name($1,(uint) ($3 - $1));
	  }

remember_name:
	{ $$=(char*) Lex->tok_start; }

remember_end:
	{ $$=(char*) Lex->tok_end; }

select_item2:
	table_wild	{ $$=$1; } /* table.* */
	| expr		{ $$=$1; }

select_alias:
	{ $$.str=0;}
	| AS ident { $$=$2; }
	| AS TEXT_STRING  { $$=$2; }
	| ident { $$=$1; }
	| TEXT_STRING  { $$=$1; }

optional_braces:
	/* empty */ {}
	| '(' ')' {}

/* all possible expressions */
expr:	expr_expr	{$$ = $1; }
	| simple_expr	{$$ = $1; }

/* expressions that begin with 'expr' */
expr_expr:
	expr IN_SYM '(' expr_list ')'
	  { $$= new Item_func_in($1,*$4); }
	| expr NOT IN_SYM '(' expr_list ')'
	  { $$= new Item_func_not(new Item_func_in($1,*$5)); }
	| expr BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_between($1,$3,$5); }
	| expr NOT BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_not(new Item_func_between($1,$4,$6)); }
	| expr OR_OR_CONCAT expr { $$= or_or_concat($1,$3); }
	| expr OR expr		{ $$= new Item_cond_or($1,$3); }
	| expr AND expr		{ $$= new Item_cond_and($1,$3); }
	| expr LIKE simple_expr opt_escape { $$= new Item_func_like($1,$3,$4); }
	| expr NOT LIKE simple_expr opt_escape	{ $$= new Item_func_not(new Item_func_like($1,$4,$5));}
	| expr REGEXP expr { $$= new Item_func_regex($1,$3); }
	| expr NOT REGEXP expr { $$= new Item_func_not(new Item_func_regex($1,$4)); }
	| expr IS NULL_SYM	{ $$= new Item_func_isnull($1); }
	| expr IS NOT NULL_SYM { $$= new Item_func_isnotnull($1); }
	| expr EQ expr		{ $$= new Item_func_eq($1,$3); }
	| expr EQUAL_SYM expr	{ $$= new Item_func_equal($1,$3); }
	| expr GE expr		{ $$= new Item_func_ge($1,$3); }
	| expr GT_SYM expr	{ $$= new Item_func_gt($1,$3); }
	| expr LE expr		{ $$= new Item_func_le($1,$3); }
	| expr LT expr		{ $$= new Item_func_lt($1,$3); }
	| expr NE expr		{ $$= new Item_func_ne($1,$3); }
	| expr SHIFT_LEFT expr	{ $$= new Item_func_shift_left($1,$3); }
	| expr SHIFT_RIGHT expr { $$= new Item_func_shift_right($1,$3); }
	| expr '+' expr		{ $$= new Item_func_plus($1,$3); }
	| expr '-' expr		{ $$= new Item_func_minus($1,$3); }
	| expr '*' expr		{ $$= new Item_func_mul($1,$3); }
	| expr '/' expr		{ $$= new Item_func_div($1,$3); }
	| expr '|' expr		{ $$= new Item_func_bit_or($1,$3); }
	| expr '&' expr		{ $$= new Item_func_bit_and($1,$3); }
	| expr '%' expr		{ $$= new Item_func_mod($1,$3); }
	| expr '+' INTERVAL_SYM expr interval
	  { $$= new Item_date_add_interval($1,$4,$5,0); }
	| expr '-' INTERVAL_SYM expr interval
	  { $$= new Item_date_add_interval($1,$4,$5,1); }

/* expressions that begin with 'expr' that do NOT follow IN_SYM */
no_in_expr:
	no_in_expr BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_between($1,$3,$5); }
	| no_in_expr NOT BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_not(new Item_func_between($1,$4,$6)); }
	| no_in_expr OR_OR_CONCAT expr	{ $$= or_or_concat($1,$3); }
	| no_in_expr OR expr		{ $$= new Item_cond_or($1,$3); }
	| no_in_expr AND expr		{ $$= new Item_cond_and($1,$3); }
	| no_in_expr LIKE simple_expr opt_escape { $$= new Item_func_like($1,$3,$4); }
	| no_in_expr NOT LIKE simple_expr opt_escape { $$= new Item_func_not(new Item_func_like($1,$4,$5)); }
	| no_in_expr REGEXP expr { $$= new Item_func_regex($1,$3); }
	| no_in_expr NOT REGEXP expr { $$= new Item_func_not(new Item_func_regex($1,$4)); }
	| no_in_expr IS NULL_SYM	{ $$= new Item_func_isnull($1); }
	| no_in_expr IS NOT NULL_SYM { $$= new Item_func_isnotnull($1); }
	| no_in_expr EQ expr		{ $$= new Item_func_eq($1,$3); }
	| no_in_expr EQUAL_SYM expr	{ $$= new Item_func_equal($1,$3); }
	| no_in_expr GE expr		{ $$= new Item_func_ge($1,$3); }
	| no_in_expr GT_SYM expr	{ $$= new Item_func_gt($1,$3); }
	| no_in_expr LE expr		{ $$= new Item_func_le($1,$3); }
	| no_in_expr LT expr		{ $$= new Item_func_lt($1,$3); }
	| no_in_expr NE expr		{ $$= new Item_func_ne($1,$3); }
	| no_in_expr SHIFT_LEFT expr  { $$= new Item_func_shift_left($1,$3); }
	| no_in_expr SHIFT_RIGHT expr { $$= new Item_func_shift_right($1,$3); }
	| no_in_expr '+' expr		{ $$= new Item_func_plus($1,$3); }
	| no_in_expr '-' expr		{ $$= new Item_func_minus($1,$3); }
	| no_in_expr '*' expr		{ $$= new Item_func_mul($1,$3); }
	| no_in_expr '/' expr		{ $$= new Item_func_div($1,$3); }
	| no_in_expr '|' expr		{ $$= new Item_func_bit_or($1,$3); }
	| no_in_expr '&' expr		{ $$= new Item_func_bit_and($1,$3); }
	| no_in_expr '%' expr		{ $$= new Item_func_mod($1,$3); }
	| no_in_expr '+' INTERVAL_SYM expr interval
	  { $$= new Item_date_add_interval($1,$4,$5,0); }
	| no_in_expr '-' INTERVAL_SYM expr interval
	  { $$= new Item_date_add_interval($1,$4,$5,1); }
	| simple_expr

/* expressions that begin with 'expr' that does NOT follow AND */
no_and_expr:
	no_and_expr IN_SYM '(' expr_list ')'
	{ $$= new Item_func_in($1,*$4); }
	| no_and_expr NOT IN_SYM '(' expr_list ')'
	  { $$= new Item_func_not(new Item_func_in($1,*$5)); }
	| no_and_expr BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_between($1,$3,$5); }
	| no_and_expr NOT BETWEEN_SYM no_and_expr AND expr
	  { $$= new Item_func_not(new Item_func_between($1,$4,$6)); }
	| no_and_expr OR_OR_CONCAT expr	{ $$= or_or_concat($1,$3); }
	| no_and_expr OR expr		{ $$= new Item_cond_or($1,$3); }
	| no_and_expr LIKE simple_expr opt_escape { $$= new Item_func_like($1,$3,$4); }
	| no_and_expr NOT LIKE simple_expr opt_escape	{ $$= new Item_func_not(new Item_func_like($1,$4,$5)); }
	| no_and_expr REGEXP expr { $$= new Item_func_regex($1,$3); }
	| no_and_expr NOT REGEXP expr { $$= new Item_func_not(new Item_func_regex($1,$4)); }
	| no_and_expr IS NULL_SYM	{ $$= new Item_func_isnull($1); }
	| no_and_expr IS NOT NULL_SYM { $$= new Item_func_isnotnull($1); }
	| no_and_expr EQ expr		{ $$= new Item_func_eq($1,$3); }
	| no_and_expr EQUAL_SYM expr	{ $$= new Item_func_equal($1,$3); }
	| no_and_expr GE expr		{ $$= new Item_func_ge($1,$3); }
	| no_and_expr GT_SYM expr	{ $$= new Item_func_gt($1,$3); }
	| no_and_expr LE expr		{ $$= new Item_func_le($1,$3); }
	| no_and_expr LT expr		{ $$= new Item_func_lt($1,$3); }
	| no_and_expr NE expr		{ $$= new Item_func_ne($1,$3); }
	| no_and_expr SHIFT_LEFT expr  { $$= new Item_func_shift_left($1,$3); }
	| no_and_expr SHIFT_RIGHT expr { $$= new Item_func_shift_right($1,$3); }
	| no_and_expr '+' expr		{ $$= new Item_func_plus($1,$3); }
	| no_and_expr '-' expr		{ $$= new Item_func_minus($1,$3); }
	| no_and_expr '*' expr		{ $$= new Item_func_mul($1,$3); }
	| no_and_expr '/' expr		{ $$= new Item_func_div($1,$3); }
	| no_and_expr '|' expr		{ $$= new Item_func_bit_or($1,$3); }
	| no_and_expr '&' expr		{ $$= new Item_func_bit_and($1,$3); }
	| no_and_expr '%' expr		{ $$= new Item_func_mod($1,$3); }
	| no_and_expr '+' INTERVAL_SYM expr interval
	  { $$= new Item_date_add_interval($1,$4,$5,0); }
	| no_and_expr '-' INTERVAL_SYM expr interval
	  { $$= new Item_date_add_interval($1,$4,$5,1); }
	| simple_expr

simple_expr:
	simple_ident
	| literal
	| '@' ident_or_text SET_VAR expr { $$= new Item_func_set_user_var($2,$4); }
	| '@' ident_or_text	 { $$= new Item_func_get_user_var($2); }
	| '@' '@' ident_or_text	 { if (!($$= get_system_var($3))) YYABORT; }
	| sum_expr
	| '-' expr %prec NEG	{ $$= new Item_func_neg($2); }
	| '~' expr %prec NEG	{ $$= new Item_func_bit_neg($2); }
	| NOT expr %prec NEG	{ $$= new Item_func_not($2); }
	| '!' expr %prec NEG	{ $$= new Item_func_not($2); }
	| '(' expr ')'		{ $$= $2; }
	| '{' ident expr '}'	{ $$= $3; }
        | MATCH '(' ident_list ')' AGAINST '(' expr ')'
          { Lex->ftfunc_list.push_back(
                   (Item_func_match *)($$=new Item_func_match(*$3,$7))); }
        | MATCH ident_list AGAINST '(' expr ')'
          { Lex->ftfunc_list.push_back(
                   (Item_func_match *)($$=new Item_func_match(*$2,$5))); }
	| BINARY expr %prec NEG	{ $$= new Item_func_binary($2); }
	| CASE_SYM opt_expr WHEN_SYM when_list opt_else END
	  { $$= new Item_func_case(* $4, $2, $5 ) }
	| FUNC_ARG0 '(' ')'
	  { $$= ((Item*(*)(void))($1.symbol->create_func))();}
	| FUNC_ARG1 '(' expr ')'
	  { $$= ((Item*(*)(Item*))($1.symbol->create_func))($3);}
	| FUNC_ARG2 '(' expr ',' expr ')'
	  { $$= ((Item*(*)(Item*,Item*))($1.symbol->create_func))($3,$5);}
	| FUNC_ARG3 '(' expr ',' expr ',' expr ')'
	  { $$= ((Item*(*)(Item*,Item*,Item*))($1.symbol->create_func))($3,$5,$7);}
	| ATAN	'(' expr ')'
	  { $$= new Item_func_atan($3); }
	| ATAN	'(' expr ',' expr ')'
	  { $$= new Item_func_atan($3,$5); }
	| CHAR_SYM '(' expr_list ')'
	  { $$= new Item_func_char(*$3); }
	| COALESCE '(' expr_list ')'
	  { $$= new Item_func_coalesce(* $3); }
	| CONCAT '(' expr_list ')'
	  { $$= new Item_func_concat(* $3); }
	| CONCAT_WS '(' expr ',' expr_list ')'
	  { $$= new Item_func_concat_ws($3, *$5); }
	| CURDATE optional_braces
	  { $$= new Item_func_curdate(); }
	| CURTIME optional_braces
	  { $$= new Item_func_curtime(); }
	| CURTIME '(' expr ')'
	  { $$= new Item_func_curtime($3); }
	| DATE_ADD_INTERVAL '(' expr ',' INTERVAL_SYM expr interval ')'
	  { $$= new Item_date_add_interval($3,$6,$7,0); }
	| DATE_SUB_INTERVAL '(' expr ',' INTERVAL_SYM expr interval ')'
	  { $$= new Item_date_add_interval($3,$6,$7,1); }
	| DATABASE '(' ')'
	  { $$= new Item_func_database(); }
	| ELT_FUNC '(' expr ',' expr_list ')'
	  { $$= new Item_func_elt($3, *$5); }
	| MAKE_SET_SYM '(' expr ',' expr_list ')'
	  { $$= new Item_func_make_set($3, *$5); }
	| ENCRYPT '(' expr ')' 		  { $$= new Item_func_encrypt($3); }
	| ENCRYPT '(' expr ',' expr ')'   { $$= new Item_func_encrypt($3,$5); }
	| DECODE_SYM '(' expr ',' TEXT_STRING ')'
	  { $$= new Item_func_decode($3,$5.str); }
	| ENCODE_SYM '(' expr ',' TEXT_STRING ')'
	 { $$= new Item_func_encode($3,$5.str); }
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
	    $$= new Item_func_date_format(new Item_func_from_unixtime($3),$5,0);
	  }
	| FIELD_FUNC '(' expr ',' expr_list ')'
	  { $$= new Item_func_field($3, *$5); }
	| HOUR_SYM '(' expr ')'
	  { $$= new Item_func_hour($3); }
	| IF '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_if($3,$5,$7); }
	| INSERT '(' expr ',' expr ',' expr ',' expr ')'
	  { $$= new Item_func_insert($3,$5,$7,$9); }
	| INTERVAL_SYM expr interval '+' expr
	  /* we cannot put interval before - */
	  { $$= new Item_date_add_interval($5,$2,$3,0); }
	| INTERVAL_SYM '(' expr ',' expr_list ')'
	  { $$= new Item_func_interval($3,* $5); }
	| LAST_INSERT_ID '(' ')'
	  {
	    $$= new Item_int((char*) "last_insert_id()",
			     current_thd->insert_id(),21);
	  }
	| LAST_INSERT_ID '(' expr ')'
	  {
	    $$= new Item_func_set_last_insert_id($3);
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
	| MINUTE_SYM '(' expr ')'
	  { $$= new Item_func_minute($3); }
	| MONTH_SYM '(' expr ')'
	  { $$= new Item_func_month($3); }
	| NOW_SYM optional_braces
	  { $$= new Item_func_now(); }
	| NOW_SYM '(' expr ')'
	  { $$= new Item_func_now($3); }
	| PASSWORD '(' expr ')' 	  { $$= new Item_func_password($3); }
	| POSITION_SYM '(' no_in_expr IN_SYM expr ')'
	  { $$ = new Item_func_locate($5,$3); }
	| RAND '(' expr ')'	{ $$= new Item_func_rand($3); }
	| RAND '(' ')'		{ $$= new Item_func_rand(); }
	| REPLACE '(' expr ',' expr ',' expr ')'
	  { $$= new Item_func_replace($3,$5,$7); }
	| RIGHT '(' expr ',' expr ')'
	  { $$= new Item_func_right($3,$5); }
	| ROUND '(' expr ')'
	  { $$= new Item_func_round($3, new Item_int((char*)"0",0,1),0); }
	| ROUND '(' expr ',' expr ')' { $$= new Item_func_round($3,$5,0); }
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
	| TRIM '(' expr ')'
	  { $$= new Item_func_trim($3,new Item_string(" ",1)); }
	| TRIM '(' LEADING opt_pad FROM expr ')'
	  { $$= new Item_func_ltrim($6,$4); }
	| TRIM '(' TRAILING opt_pad FROM expr ')'
	  { $$= new Item_func_rtrim($6,$4); }
	| TRIM '(' BOTH opt_pad FROM expr ')'
	  { $$= new Item_func_trim($6,$4); }
	| TRIM '(' expr FROM expr ')'
	  { $$= new Item_func_trim($5,$3); }
	| TRUNCATE_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_round($3,$5,1); }
	| UDA_CHAR_SUM '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_sum_udf_str($1, *$3);
	    else
	      $$ = new Item_sum_udf_str($1);
	  }
	| UDA_FLOAT_SUM '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_sum_udf_float($1, *$3);
	    else
	      $$ = new Item_sum_udf_float($1);
	  }
	| UDA_INT_SUM '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_sum_udf_int($1, *$3);
	    else
	      $$ = new Item_sum_udf_int($1);
	  }
	| UDF_CHAR_FUNC '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_func_udf_str($1, *$3);
	    else
	      $$ = new Item_func_udf_str($1);
	  }
	| UDF_FLOAT_FUNC '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_func_udf_float($1, *$3);
	    else
	      $$ = new Item_func_udf_float($1);
	  }
	| UDF_INT_FUNC '(' udf_expr_list ')'
	  {
	    if ($3 != NULL)
	      $$ = new Item_func_udf_int($1, *$3);
	    else
	      $$ = new Item_func_udf_int($1);
	  }
	| UNIQUE_USERS '(' text_literal ',' NUM ',' NUM ',' expr_list ')'
	  { $$= new Item_func_unique_users($3,atoi($5.str),atoi($7.str), * $9); }
	| UNIX_TIMESTAMP '(' ')'
	  { $$= new Item_func_unix_timestamp(); }
	| UNIX_TIMESTAMP '(' expr ')'
	  { $$= new Item_func_unix_timestamp($3); }
	| USER '(' ')'
	  { $$= new Item_func_user(); }
	| WEEK_SYM '(' expr ')'
	  { $$= new Item_func_week($3,new Item_int((char*) "0",0,1)); }
	| WEEK_SYM '(' expr ',' expr ')'
	  { $$= new Item_func_week($3,$5); }
	| YEAR_SYM '(' expr ')'
	  { $$= new Item_func_year($3); }
	| YEARWEEK '(' expr ')'
	  { $$= new Item_func_yearweek($3,new Item_int((char*) "0",0,1)); }
	| YEARWEEK '(' expr ',' expr ')'
	  { $$= new Item_func_yearweek($3, $5); }
	| BENCHMARK_SYM '(' ULONG_NUM ',' expr ')'
	  { $$=new Item_func_benchmark($3,$5); }
	| EXTRACT_SYM '(' interval FROM expr ')'
	{ $$=new Item_extract( $3, $5); }

udf_expr_list:
	/* empty */	{ $$= NULL; }
	| expr_list	{ $$= $1;}

sum_expr:
	AVG_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_avg($3); }
	| BIT_AND  '(' in_sum_expr ')'
	  { $$=new Item_sum_and($3); }
	| BIT_OR  '(' in_sum_expr ')'
	  { $$=new Item_sum_or($3); }
	| COUNT_SYM '(' '*' ')'
	  { $$=new Item_sum_count(new Item_int((int32) 0L,1)); }
	| COUNT_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_count($3); }
	| COUNT_SYM '(' DISTINCT expr_list ')'
	  { $$=new Item_sum_count_distinct(* $4); }
	| GROUP_UNIQUE_USERS '(' text_literal ',' NUM ',' NUM ',' in_sum_expr ')'
	  { $$= new Item_sum_unique_users($3,atoi($5.str),atoi($7.str),$9); }
	| MIN_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_min($3); }
	| MAX_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_max($3); }
	| STD_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_std($3); }
	| SUM_SYM '(' in_sum_expr ')'
	  { $$=new Item_sum_sum($3); }

in_sum_expr:
	{ Lex->in_sum_expr++ }
	expr
	{
	  Lex->in_sum_expr--;
	  $$=$2;
	}

expr_list:
	{ Lex->expr_list.push_front(new List<Item>); }
	expr_list2
	{ $$= Lex->expr_list.pop(); }

expr_list2:
	expr { Lex->expr_list.head()->push_back($1); }
	| expr_list2 ',' expr { Lex->expr_list.head()->push_back($3); }

ident_list:
        { Lex->expr_list.push_front(new List<Item>); }
        ident_list2
        { $$= Lex->expr_list.pop(); }

ident_list2:
        simple_ident { Lex->expr_list.head()->push_back($1); }
        | ident_list2 ',' simple_ident { Lex->expr_list.head()->push_back($3); }

opt_expr:
	/* empty */      { $$= NULL; }
	| expr         	 { $$= $1; }

opt_else:
	/* empty */    { $$= NULL; }
	| ELSE expr    { $$= $2; }

when_list:
        { Lex->when_list.push_front(new List<Item>) }
	when_list2
	{ $$= Lex->when_list.pop(); }

when_list2:
	expr THEN_SYM expr
	  {
	    Lex->when_list.head()->push_back($1);
	    Lex->when_list.head()->push_back($3);
	}
	| when_list2 WHEN_SYM expr THEN_SYM expr
	  {
	    Lex->when_list.head()->push_back($3);
	    Lex->when_list.head()->push_back($5);
	  }

opt_pad:
	/* empty */ { $$=new Item_string(" ",1); }
	| expr	    { $$=$1; }

join_table_list:
	'(' join_table_list ')'	{ $$=$2; }
	| join_table		{ $$=$1; }
	| join_table_list normal_join join_table { $$=$3 }
	| join_table_list STRAIGHT_JOIN join_table { $$=$3 ; $$->straight=1; }
	| join_table_list INNER_SYM JOIN_SYM join_table ON expr
	  { add_join_on($4,$6); $$=$4; }
	| join_table_list INNER_SYM JOIN_SYM join_table
	  { Lex->db1=$1->db; Lex->table1=$1->name;
	    Lex->db2=$4->db; Lex->table2=$4->name; }
	  USING '(' using_list ')'
	  { add_join_on($4,$8); $$=$4; }
	| join_table_list LEFT opt_outer JOIN_SYM join_table ON expr
	  { add_join_on($5,$7); $5->outer_join|=JOIN_TYPE_LEFT; $$=$5; }
	| join_table_list LEFT opt_outer JOIN_SYM join_table
	  { Lex->db1=$1->db; Lex->table1=$1->name;
	    Lex->db2=$5->db; Lex->table2=$5->name; }
	  USING '(' using_list ')'
	  { add_join_on($5,$9); $5->outer_join|=JOIN_TYPE_LEFT; $$=$5; }
	| join_table_list NATURAL LEFT opt_outer JOIN_SYM join_table
	  { add_join_natural($1,$6); $6->outer_join|=JOIN_TYPE_LEFT; $$=$6; }
	| join_table_list RIGHT opt_outer JOIN_SYM join_table ON expr
	  { add_join_on($1,$7); $1->outer_join|=JOIN_TYPE_RIGHT; $$=$1; }
	| join_table_list RIGHT opt_outer JOIN_SYM join_table
	  { Lex->db1=$1->db; Lex->table1=$1->name;
	    Lex->db2=$5->db; Lex->table2=$5->name; }
	  USING '(' using_list ')'
	  { add_join_on($1,$9); $1->outer_join|=JOIN_TYPE_RIGHT; $$=$1; }
	| join_table_list NATURAL RIGHT opt_outer JOIN_SYM join_table
	  { add_join_natural($6,$1); $1->outer_join|=JOIN_TYPE_RIGHT; $$=$1; }
	| join_table_list NATURAL JOIN_SYM join_table
	  { add_join_natural($1,$4); $$=$4; }

normal_join:
	',' {}
	| JOIN_SYM {}
	| CROSS JOIN_SYM {}

join_table:
	{ Lex->use_index_ptr=Lex->ignore_index_ptr=0; }
        table_ident opt_table_alias opt_key_definition
	{ if (!($$=add_table_to_list($2,$3,0,TL_UNLOCK, Lex->use_index_ptr,
	                             Lex->ignore_index_ptr))) YYABORT; }
	| '{' ident join_table LEFT OUTER JOIN_SYM join_table ON expr '}'
	  { add_join_on($7,$9); $7->outer_join|=JOIN_TYPE_LEFT; $$=$7; }

opt_outer:
	/* empty */	{}
	| OUTER		{}

opt_key_definition:
	/* empty */	{}
	| USE_SYM    key_usage_list
          { Lex->use_index= *$2; Lex->use_index_ptr= &Lex->use_index; }
	| IGNORE_SYM key_usage_list
	  { Lex->ignore_index= *$2; Lex->ignore_index_ptr= &Lex->ignore_index;}

key_usage_list:
	key_or_index { Lex->interval_list.empty() } '(' key_usage_list2 ')'
        { $$= &Lex->interval_list; }

key_usage_list2:
	key_usage_list2 ',' ident
        { Lex->interval_list.push_back(new String((const char*) $3.str,$3.length)); }
	| ident
        { Lex->interval_list.push_back(new String((const char*) $1.str,$1.length)); }
	| PRIMARY_SYM
        { Lex->interval_list.push_back(new String("PRIMARY",7)); }

using_list:
	ident
	  { if (!($$= new Item_func_eq(new Item_field(Lex->db1,Lex->table1, $1.str), new Item_field(Lex->db2,Lex->table2,$1.str))))
	      YYABORT;
	  }
	| using_list ',' ident
	  {
	    if (!($$= new Item_cond_and(new Item_func_eq(new Item_field(Lex->db1,Lex->table1,$3.str), new Item_field(Lex->db2,Lex->table2,$3.str)), $1)))
	      YYABORT;
	  }

interval:
	 DAY_HOUR_SYM		{ $$=INTERVAL_DAY_HOUR; }
	| DAY_MINUTE_SYM	{ $$=INTERVAL_DAY_MINUTE; }
	| DAY_SECOND_SYM	{ $$=INTERVAL_DAY_SECOND; }
	| DAY_SYM		{ $$=INTERVAL_DAY; }
	| HOUR_MINUTE_SYM	{ $$=INTERVAL_HOUR_MINUTE; }
	| HOUR_SECOND_SYM	{ $$=INTERVAL_HOUR_SECOND; }
	| HOUR_SYM		{ $$=INTERVAL_HOUR; }
	| MINUTE_SECOND_SYM	{ $$=INTERVAL_MINUTE_SECOND; }
	| MINUTE_SYM		{ $$=INTERVAL_MINUTE; }
	| MONTH_SYM		{ $$=INTERVAL_MONTH; }
	| SECOND_SYM		{ $$=INTERVAL_SECOND; }
	| YEAR_MONTH_SYM	{ $$=INTERVAL_YEAR_MONTH; }
	| YEAR_SYM		{ $$=INTERVAL_YEAR; }

table_alias:
	/* empty */
	| AS
	| EQ

opt_table_alias:
	/* empty */		{ $$=0; }
	| table_alias ident
	  { $$= (LEX_STRING*) sql_memdup(&$2,sizeof(LEX_STRING)); }


where_clause:
	/* empty */  { Lex->where= 0; }
	| WHERE expr { Lex->where= $2; }

having_clause:
	/* empty */
	| HAVING { Lex->create_refs=1; } expr
	{ Lex->having= $3; Lex->create_refs=0; }

opt_escape:
	ESCAPE_SYM TEXT_STRING	{ $$= $2.str; }
	| /* empty */		{ $$= (char*) "\\"; }


/*
** group by statement in select
*/

group_clause:
	/* empty */
	| GROUP BY group_list

group_list:
	group_list ',' group_ident
	  { if (add_group_to_list($3,(bool) 1)) YYABORT; }
	| group_ident
	  { if (add_group_to_list($1,(bool) 1)) YYABORT; }

/*
** Order by statement in select
*/

opt_order_clause:
	/* empty */
	| order_clause

order_clause:
	ORDER_SYM BY { Lex->sort_default=1; } order_list

order_list:
	order_list ',' order_ident order_dir
	  { if (add_order_to_list($3,(bool) $4)) YYABORT; }
	| order_ident order_dir
	  { if (add_order_to_list($1,(bool) $2)) YYABORT; }

order_dir:
	/* empty */ { $$ =  1; }
	| ASC  { $$ = Lex->sort_default=1; }
	| DESC { $$ = Lex->sort_default=0; }


limit_clause:
	/* empty */
	{
	  Lex->select_limit= current_thd->default_select_limit;
	  Lex->offset_limit= 0L;
	}
	| LIMIT ULONG_NUM
	  { Lex->select_limit= $2; Lex->offset_limit=0L; }
	| LIMIT ULONG_NUM ',' ULONG_NUM
	  { Lex->select_limit= $4; Lex->offset_limit=$2; }

delete_limit_clause:
	/* empty */
	{
	  Lex->select_limit= HA_POS_ERROR;
	}
	| LIMIT ULONGLONG_NUM
	{ Lex->select_limit= (ha_rows) $2; }

ULONG_NUM:
	NUM { $$= strtoul($1.str,NULL,10); }
	| REAL_NUM { $$= strtoul($1.str,NULL,10); }
	| FLOAT_NUM { $$= strtoul($1.str,NULL,10); }

ULONGLONG_NUM:
	NUM	   { $$= (ulonglong) strtoul($1.str,NULL,10); }
	| LONG_NUM { $$= strtoull($1.str,NULL,10); }
	| REAL_NUM { $$= strtoull($1.str,NULL,10); }
	| FLOAT_NUM { $$= strtoull($1.str,NULL,10); }

procedure_clause:
	/* empty */
	| PROCEDURE ident			/* Procedure name */
	  {
	    LEX *lex=Lex;
	    lex->proc_list.elements=0;
	    lex->proc_list.first=0;
	    lex->proc_list.next= (byte**) &lex->proc_list.first;
	    if (add_proc_to_list(new Item_field(NULL,NULL,$2.str)))
	      YYABORT;
	  }
	  '(' procedure_list ')'


procedure_list:
	/* empty */ {}
	| procedure_list2 {}

procedure_list2:
	procedure_list2 ',' procedure_item
	| procedure_item

procedure_item:
	  remember_name expr
	  {
	    if (add_proc_to_list($2))
	      YYABORT;
	    if (!$2->name)
	      $2->set_name($1,(uint) ((char*) Lex->tok_end - $1));
	  }

opt_into:
	INTO OUTFILE TEXT_STRING
	{
	  if (!(Lex->exchange= new sql_exchange($3.str,0)))
	    YYABORT;
	}
	opt_field_term opt_line_term
	| INTO DUMPFILE TEXT_STRING
	{
	  if (!(Lex->exchange= new sql_exchange($3.str,1)))
	    YYABORT;
	}


/*
** Drop : delete tables or index
*/

drop:
	DROP TABLE_SYM if_exists table_list opt_restrict
	{
	  Lex->sql_command = SQLCOM_DROP_TABLE;
	  Lex->drop_if_exists = $3;
	}
	| DROP INDEX ident ON table_ident {}
	  {
	     Lex->sql_command= SQLCOM_DROP_INDEX;
	     Lex->drop_list.empty();
	     Lex->drop_list.push_back(new Alter_drop(Alter_drop::KEY,
						     $3.str));
	     if (!add_table_to_list($5,NULL, 1))
	      YYABORT;
	  }
	| DROP DATABASE if_exists ident
	  {
	    Lex->sql_command= SQLCOM_DROP_DB;
	    Lex->drop_if_exists=$3;
	    Lex->name=$4.str;
	 }
	| DROP UDF_SYM ident
	  {
	    Lex->sql_command = SQLCOM_DROP_FUNCTION;
	    Lex->udf.name=$3.str;
	  }


table_list:
	table
	| table_list ',' table

table:
	table_ident
	{ if (!add_table_to_list($1,NULL,1)) YYABORT; }

if_exists:
	/* empty */ { $$=0; }
	| IF EXISTS { $$= 1; }

/*
** Insert : add new data to table
*/

insert:
	INSERT { Lex->sql_command = SQLCOM_INSERT; } insert_lock_option opt_ignore insert2 insert_field_spec

replace:
	REPLACE { Lex->sql_command = SQLCOM_REPLACE; } replace_lock_option insert2 insert_field_spec

insert_lock_option:
	/* empty */	{ Lex->lock_option= TL_WRITE_CONCURRENT_INSERT; }
	| LOW_PRIORITY	{ Lex->lock_option= TL_WRITE_LOW_PRIORITY; }
	| DELAYED_SYM	{ Lex->lock_option= TL_WRITE_DELAYED; }
	| HIGH_PRIORITY { Lex->lock_option= TL_WRITE; }

replace_lock_option:
	opt_low_priority {}
	| DELAYED_SYM	{ Lex->lock_option= TL_WRITE_DELAYED; }

insert2:
	INTO insert_table {}
	| insert_table {}

insert_table:
	table
	{
	  Lex->field_list.empty();
	  Lex->many_values.empty();
	  Lex->insert_list=0;
	}

insert_field_spec:
	opt_field_spec insert_values {}
	| SET
	  {
	    if (!(Lex->insert_list = new List_item) ||
		Lex->many_values.push_back(Lex->insert_list))
	      YYABORT;
	   }
	   ident_eq_list

opt_field_spec:
	/* empty */	  { }
	| '(' fields ')'  { }
	| '(' ')'	  { }

fields:
	fields ',' insert_ident { Lex->field_list.push_back($3); }
	| insert_ident		{ Lex->field_list.push_back($1); }

insert_values:
	VALUES	values_list  {}
	| SELECT_SYM
	  {
	    LEX *lex=Lex;
	    lex->sql_command = (lex->sql_command == SQLCOM_INSERT ?
				SQLCOM_INSERT_SELECT : SQLCOM_REPLACE_SELECT);
	    mysql_init_select(lex);
	  }
	  select_options select_item_list select_from {}

values_list:
	values_list ','  no_braces
	| no_braces

ident_eq_list:
	ident_eq_list ',' ident_eq_value
	|
	ident_eq_value

ident_eq_value:
	simple_ident equal expr
	 {
	  if (Lex->field_list.push_back($1) ||
	      Lex->insert_list->push_back($3))
	    YYABORT;
	 }

equal:	EQ		{}
	| SET_VAR	{}

no_braces:
	 '('
	 {
	    if (!(Lex->insert_list = new List_item))
	      YYABORT;
	 }
	 opt_values ')'
	 {
	  if (Lex->many_values.push_back(Lex->insert_list))
	    YYABORT;
	 }

opt_values:
	/* empty */ {}
	| values

values:
	values ','  expr
	{
	  if (Lex->insert_list->push_back($3))
	    YYABORT;
	}
	| expr
	{
	  if (Lex->insert_list->push_back($1))
	    YYABORT;
	}

/* Update rows in a table */

update:
	UPDATE_SYM opt_low_priority opt_ignore table SET update_list where_clause delete_limit_clause
	{ Lex->sql_command = SQLCOM_UPDATE; }

update_list:
	update_list ',' simple_ident equal expr
	{
	  if (add_item_to_list($3) || add_value_to_list($5))
	    YYABORT;
	}
	| simple_ident equal expr
	  {
	    if (add_item_to_list($1) || add_value_to_list($3))
	      YYABORT;
	  }

opt_low_priority:
	/* empty */	{ Lex->lock_option= current_thd->update_lock_default; }
	| LOW_PRIORITY	{ Lex->lock_option= TL_WRITE_LOW_PRIORITY; }

/* Delete rows from a table */

delete:
	DELETE_SYM
	{
	  Lex->sql_command= SQLCOM_DELETE; Lex->options=0;
	  Lex->lock_option= current_thd->update_lock_default;
	}
        opt_delete_options FROM table
	where_clause delete_limit_clause


opt_delete_options:
	/* empty */	    {}
	| opt_delete_option opt_delete_options {}

opt_delete_option:
	QUICK		{ Lex->options|= OPTION_QUICK; }
	| LOW_PRIORITY	{ Lex->lock_option= TL_WRITE_LOW_PRIORITY; }

truncate:
	TRUNCATE_SYM opt_table_sym table
	{ Lex->sql_command= SQLCOM_TRUNCATE; Lex->options=0;
	  Lex->lock_option= current_thd->update_lock_default; }

opt_table_sym:
	/* empty */
	| TABLE_SYM

/* Show things */

show:	SHOW { Lex->wild=0;} show_param

show_param:
	DATABASES wild
	  { Lex->sql_command= SQLCOM_SHOW_DATABASES; }
	| TABLES opt_db wild
	  { Lex->sql_command= SQLCOM_SHOW_TABLES; Lex->db= $2; Lex->options=0;}
	| TABLE_SYM STATUS_SYM opt_db wild
	  { Lex->sql_command= SQLCOM_SHOW_TABLES;
	    Lex->options|= SELECT_DESCRIBE;
	    Lex->db= $3;
	  }
	| OPEN_SYM TABLES opt_db wild
	  { Lex->sql_command= SQLCOM_SHOW_OPEN_TABLES;
	    Lex->db= $3;
	    Lex->options=0;
	  }
	| opt_full COLUMNS FROM table_ident opt_db wild
	  {
	    Lex->sql_command= SQLCOM_SHOW_FIELDS;
	    if ($5)
	      $4->change_db($5);
	    if (!add_table_to_list($4,NULL,0))
	      YYABORT;
	  }
        | MASTER_SYM LOGS_SYM
          {
	    Lex->sql_command = SQLCOM_SHOW_BINLOGS;
          }      
	| keys_or_index FROM table_ident opt_db
	  {
	    Lex->sql_command= SQLCOM_SHOW_KEYS;
	    if ($4)
	      $3->change_db($4);
	    if (!add_table_to_list($3,NULL,0))
	      YYABORT;
	  }
	| STATUS_SYM wild
	  { Lex->sql_command= SQLCOM_SHOW_STATUS; }
	| opt_full PROCESSLIST_SYM
	  { Lex->sql_command= SQLCOM_SHOW_PROCESSLIST;}
	| VARIABLES wild
	  { Lex->sql_command= SQLCOM_SHOW_VARIABLES; }
	| LOGS_SYM
	  { Lex->sql_command= SQLCOM_SHOW_LOGS; }
	| GRANTS FOR_SYM user
	  { Lex->sql_command= SQLCOM_SHOW_GRANTS;
	    Lex->grant_user=$3; Lex->grant_user->password.str=NullS; }
        | CREATE TABLE_SYM table_ident
          {
	    Lex->sql_command = SQLCOM_SHOW_CREATE;
	    if(!add_table_to_list($3, NULL,0))
	      YYABORT;
	  }
        | MASTER_SYM STATUS_SYM
          {
	    Lex->sql_command = SQLCOM_SHOW_MASTER_STAT;
          }
        | SLAVE STATUS_SYM
          {
	    Lex->sql_command = SQLCOM_SHOW_SLAVE_STAT;
          }

opt_db:
	/* empty */  { $$= 0; }
	| FROM ident { $$= $2.str; }

wild:
	/* empty */
	| LIKE text_string { Lex->wild= $2; }

opt_full:
	/* empty */ { Lex->verbose=0; }
	| FULL	    { Lex->verbose=1; }

/* A Oracle compatible synonym for show */
describe:
	describe_command table_ident
	{
	  Lex->wild=0;
	  Lex->verbose=0;
	  Lex->sql_command=SQLCOM_SHOW_FIELDS;
	  if (!add_table_to_list($2, NULL,0))
	    YYABORT;
	}
	opt_describe_column
	| describe_command select { Lex->options|= SELECT_DESCRIBE };


describe_command:
	DESC
	| DESCRIBE

opt_describe_column:
	/* empty */	{}
	| text_string	{ Lex->wild= $1; }
	| ident		{ Lex->wild= new String((const char*) $1.str,$1.length); }


/* flush things */

flush:
	FLUSH_SYM {Lex->sql_command= SQLCOM_FLUSH; Lex->type=0; } flush_options

flush_options:
	flush_options ',' flush_option
	| flush_option

flush_option:
	table_or_tables	{ Lex->type|= REFRESH_TABLES; } opt_table_list
	| TABLES WITH READ_SYM LOCK_SYM { Lex->type|= REFRESH_TABLES | REFRESH_READ_LOCK; }
	| HOSTS_SYM	{ Lex->type|= REFRESH_HOSTS; }
	| PRIVILEGES	{ Lex->type|= REFRESH_GRANT; }
	| LOGS_SYM	{ Lex->type|= REFRESH_LOG; }
	| STATUS_SYM	{ Lex->type|= REFRESH_STATUS; }
        | SLAVE         { Lex->type|= REFRESH_SLAVE; }
        | MASTER_SYM    { Lex->type|= REFRESH_MASTER; }

opt_table_list:
	/* empty */  {}
	| table_list {}

reset:
	RESET_SYM {Lex->sql_command= SQLCOM_RESET; Lex->type=0; } reset_options

reset_options:
	reset_options ',' reset_option
	| reset_option

reset_option:
        SLAVE           { Lex->type|= REFRESH_SLAVE; }
        | MASTER_SYM    { Lex->type|= REFRESH_MASTER; }

purge:
	PURGE { Lex->sql_command = SQLCOM_PURGE; Lex->type=0;}
        MASTER_SYM LOGS_SYM TO_SYM TEXT_STRING
         {
	   Lex->to_log = $6.str;
         } 

/* kill threads */

kill:
	KILL_SYM expr
	{
	  if ($2->fix_fields(current_thd,0))
	     { 
		send_error(&current_thd->net, ER_SET_CONSTANTS_ONLY);
	        YYABORT;
	     }
          Lex->sql_command=SQLCOM_KILL;
	  Lex->thread_id= (ulong) $2->val_int();
	}

/* change database */

use:	USE_SYM ident
	{ Lex->sql_command=SQLCOM_CHANGE_DB; Lex->db= $2.str; }

/* import, export of files */

load:	LOAD DATA_SYM opt_low_priority opt_local INFILE TEXT_STRING
	{
	  Lex->sql_command= SQLCOM_LOAD;
	  Lex->local_file= $4;
	  if (!(Lex->exchange= new sql_exchange($6.str,0)))
	    YYABORT;
	  Lex->field_list.empty();
	}
	opt_duplicate INTO TABLE_SYM table_ident opt_field_term opt_line_term
	opt_ignore_lines opt_field_spec
	{
	  if (!add_table_to_list($11,NULL,1))
	    YYABORT;
	}
        |
	LOAD TABLE_SYM table_ident FROM MASTER_SYM
        {
	  Lex->sql_command = SQLCOM_LOAD_MASTER_TABLE;
	  if (!add_table_to_list($3,NULL,1))
	    YYABORT;

        }

opt_local:
	/* empty */	{ $$=0;}
	| LOCAL_SYM	{ $$=1;}

opt_duplicate:
	/* empty */	{ Lex->duplicates=DUP_ERROR; }
	| REPLACE	{ Lex->duplicates=DUP_REPLACE; }
	| IGNORE_SYM	{ Lex->duplicates=DUP_IGNORE; }

opt_field_term:
	/* empty */
	| COLUMNS field_term_list

field_term_list:
	field_term_list field_term
	| field_term

field_term:
	TERMINATED BY text_string { Lex->exchange->field_term= $3;}
	| OPTIONALLY ENCLOSED BY text_string
	  { Lex->exchange->enclosed= $4; Lex->exchange->opt_enclosed=1;}
	| ENCLOSED BY text_string { Lex->exchange->enclosed= $3;}
	| ESCAPED BY text_string  { Lex->exchange->escaped= $3;}

opt_line_term:
	/* empty */
	| LINES line_term_list

line_term_list:
	line_term_list line_term
	| line_term

line_term:
	TERMINATED BY text_string { Lex->exchange->line_term= $3;}
	| STARTING BY text_string { Lex->exchange->line_start= $3;}

opt_ignore_lines:
	/* empty */
	| IGNORE_SYM NUM LINES
	  { Lex->exchange->skip_lines=atol($2.str); }

/* Common definitions */

text_literal:
	TEXT_STRING { $$ = new Item_string($1.str,$1.length); }
	| text_literal TEXT_STRING
	{ ((Item_string*) $1)->append($2.str,$2.length); }

text_string:
	TEXT_STRING	{ $$=  new String($1.str,$1.length); }
	| HEX_NUM
	  {
	    Item *tmp = new Item_varbinary($1.str,$1.length);
	    $$= tmp ? tmp->val_str((String*) 0) : (String*) 0;
	  }

literal:
	text_literal	{ $$ =	$1; }
	| NUM		{ $$ =	new Item_int($1.str, (longlong) atol($1.str),$1.length); }
	| LONG_NUM	{ $$ =	new Item_int($1.str); }
	| REAL_NUM	{ $$ =	new Item_real($1.str, $1.length); }
	| FLOAT_NUM	{ $$ =	new Item_float($1.str, $1.length); }
	| NULL_SYM	{ $$ =	new Item_null();
			  Lex->next_state=STATE_OPERATOR_OR_IDENT;}
	| HEX_NUM	{ $$ =	new Item_varbinary($1.str,$1.length)};
	| DATE_SYM text_literal { $$ = $2; }
	| TIME_SYM text_literal { $$ = $2; }
	| TIMESTAMP text_literal { $$ = $2; }

/**********************************************************************
** Createing different items.
**********************************************************************/

insert_ident:
	simple_ident	 { $$=$1; }
	| table_wild	 { $$=$1; }

table_wild:
	ident '.' '*' { $$ = new Item_field(NullS,$1.str,"*"); }
	| ident '.' ident '.' '*'
	{ $$ = new Item_field((current_thd->client_capabilities & CLIENT_NO_SCHEMA ? NullS : $1.str),$3.str,"*"); }

group_ident:
	order_ident order_dir

order_ident:
	expr { $$=$1; }

simple_ident:
	ident
	{ $$ = !Lex->create_refs || Lex->in_sum_expr > 0 ? (Item*) new Item_field(NullS,NullS,$1.str) : (Item*) new Item_ref(NullS,NullS,$1.str); }
	| ident '.' ident
	{ $$ = !Lex->create_refs || Lex->in_sum_expr > 0 ? (Item*) new Item_field(NullS,$1.str,$3.str) : (Item*) new Item_ref(NullS,$1.str,$3.str); }
	| '.' ident '.' ident
	{ $$ = !Lex->create_refs || Lex->in_sum_expr > 0 ? (Item*) new Item_field(NullS,$2.str,$4.str) : (Item*) new Item_ref(NullS,$2.str,$4.str); }
	| ident '.' ident '.' ident
	{ $$ = !Lex->create_refs || Lex->in_sum_expr > 0 ? (Item*) new Item_field((current_thd->client_capabilities & CLIENT_NO_SCHEMA ? NullS :$1.str),$3.str,$5.str) : (Item*) new Item_ref((current_thd->client_capabilities & CLIENT_NO_SCHEMA ? NullS :$1.str),$3.str,$5.str); }


field_ident:
	ident			{ $$=$1;}
	| ident '.' ident	{ $$=$3;}	/* Skipp schema name in create*/
	| '.' ident		{ $$=$2;}	/* For Delphi */

table_ident:
	ident			{ $$=new Table_ident($1); }
	| ident '.' ident	{ $$=new Table_ident($1,$3,0);}
	| '.' ident		{ $$=new Table_ident($2);}	/* For Delphi */

ident:
	IDENT	    { $$=$1; }
	| keyword
	{
	  $$.str=sql_strmake($1.str,$1.length);
	  $$.length=$1.length;
	  if (Lex->next_state != STATE_END)
	    Lex->next_state=STATE_OPERATOR_OR_IDENT;
	}

ident_or_text:
	ident 		{ $$=$1;}
	| TEXT_STRING	{ $$=$1;}
	| LEX_HOSTNAME	{ $$=$1;}

user:
	ident_or_text
	{
	  if (!($$=(LEX_USER*) sql_alloc(sizeof(st_lex_user))))
	    YYABORT;
	  $$->user = $1; $$->host.str=NullS;
	  }
	| ident_or_text '@' ident_or_text
	  {
	  if (!($$=(LEX_USER*) sql_alloc(sizeof(st_lex_user))))
	      YYABORT;
	    $$->user = $1; $$->host=$3;
	  }

/* Keyword that we allow for identifiers */

keyword:
	ACTION			{}
	| AFTER_SYM		{}
	| AGAINST		{}
	| AGGREGATE_SYM		{}
	| AUTOCOMMIT		{}
	| AUTO_INC		{}
	| AVG_ROW_LENGTH	{}
	| AVG_SYM		{}
	| BACKUP_SYM		{}
	| BEGIN_SYM		{}
	| BERKELEY_DB_SYM	{}
	| BIT_SYM		{}
	| BOOL_SYM		{}
	| CHANGED		{}
	| CHECKSUM_SYM		{}
	| CHECK_SYM		{}
	| COMMENT_SYM		{}
	| COMMIT_SYM		{}
	| COMMITTED_SYM		{}
	| COMPRESSED_SYM	{}
	| DATA_SYM		{}
	| DATETIME		{}
	| DATE_SYM		{}
	| DAY_SYM		{}
	| DELAY_KEY_WRITE_SYM	{}
	| DUMPFILE		{}
	| DYNAMIC_SYM		{}
	| END			{}
	| ENUM			{}
	| ESCAPE_SYM		{}
	| EXTENDED_SYM		{}
	| FAST_SYM		{}
	| FULL			{}
	| FILE_SYM		{}
	| FIRST_SYM		{}
	| FIXED_SYM		{}
	| FLUSH_SYM		{}
	| GRANTS                {}
	| GEMINI_SYM		{}
	| GLOBAL_SYM		{}
	| HEAP_SYM		{}
	| HOSTS_SYM		{}
	| HOUR_SYM		{}
	| IDENTIFIED_SYM	{}
	| ISOLATION		{}
	| ISAM_SYM		{}
	| INNOBASE_SYM		{}
	| LEVEL_SYM		{}
	| LOCAL_SYM		{}
	| LOGS_SYM		{}
	| MAX_ROWS		{}
	| MASTER_SYM		{}
	| MASTER_HOST_SYM	{}
	| MASTER_PORT_SYM	{}
	| MASTER_LOG_FILE_SYM	{}
	| MASTER_LOG_POS_SYM	{}
	| MASTER_USER_SYM	{}
	| MASTER_PASSWORD_SYM	{}
	| MASTER_CONNECT_RETRY_SYM	{}
	| MEDIUM_SYM		{}
	| MERGE_SYM		{}
	| MINUTE_SYM		{}
	| MIN_ROWS		{}
	| MODIFY_SYM		{}
	| MODE_SYM		{}
	| MONTH_SYM		{}
	| MYISAM_SYM		{}
	| NATIONAL_SYM		{}
	| NCHAR_SYM		{}
	| NO_SYM		{}
	| OPEN_SYM		{}
	| PACK_KEYS_SYM		{}
	| PASSWORD		{}
	| PROCESS		{}
	| PROCESSLIST_SYM	{}
	| QUICK			{}
	| RAID_0_SYM            {}
	| RAID_CHUNKS		{}
	| RAID_CHUNKSIZE	{}
	| RAID_STRIPED_SYM      {}
	| RAID_TYPE		{}
	| RELOAD		{}
	| REPAIR		{}
	| REPEATABLE_SYM	{}
	| RESET_SYM		{}
	| RESTORE_SYM		{}
	| ROLLBACK_SYM		{}
	| ROWS_SYM		{}
	| ROW_FORMAT_SYM	{}
	| ROW_SYM		{}
	| SECOND_SYM		{}
	| SERIALIZABLE_SYM	{}
	| SESSION_SYM		{}
	| SHARE_SYM		{}
	| SHUTDOWN		{}
	| START_SYM		{}
	| STATUS_SYM		{}
	| STOP_SYM		{}
	| STRING_SYM		{}
	| TEMPORARY		{}
	| TEXT_SYM		{}
	| TRANSACTION_SYM	{}
	| TRUNCATE_SYM		{}
	| TIMESTAMP		{}
	| TIME_SYM		{}
	| TYPE_SYM		{}
	| UDF_SYM		{}
	| UNCOMMITTED_SYM	{}
	| VARIABLES		{}
	| WORK_SYM		{}
	| YEAR_SYM		{}
        | SLAVE                 {}

/* Option functions */

set:
	SET opt_option
	{
	  THD *thd=current_thd;
	  Lex->sql_command= SQLCOM_SET_OPTION;
	  Lex->options=thd->options;
	  Lex->select_limit=thd->default_select_limit;
	  Lex->gemini_spin_retries=thd->gemini_spin_retries;
	  Lex->tx_isolation=thd->tx_isolation;
	}
	option_value_list

opt_option:
	/* empty */ {}
	| OPTION {}

option_value_list:
	option_value
	| option_value_list ',' option_value

option_value:
	set_option equal NUM
	{
	  if (atoi($3.str) == 0)
	    Lex->options&= ~$1;
	  else
	    Lex->options|= $1;
	}
	| set_isolation
	| AUTOCOMMIT equal NUM
	{
	  if (atoi($3.str) != 0)	/* Test NOT AUTOCOMMIT */
	    Lex->options&= ~(OPTION_NOT_AUTO_COMMIT);
	  else
	    Lex->options|= OPTION_NOT_AUTO_COMMIT;
	}
	| SQL_SELECT_LIMIT equal ULONG_NUM
	{
	  Lex->select_limit= $3;
	}
	| SQL_SELECT_LIMIT equal DEFAULT
	{
	  Lex->select_limit= HA_POS_ERROR;
	}
	| SQL_MAX_JOIN_SIZE equal ULONG_NUM
	{
	  current_thd->max_join_size= $3;
	  Lex->options&= ~OPTION_BIG_SELECTS;
	}
	| SQL_MAX_JOIN_SIZE equal DEFAULT
	{
	  current_thd->max_join_size= HA_POS_ERROR;
	}
	| TIMESTAMP equal ULONG_NUM
	{
	  current_thd->set_time((time_t) $3);
	}
	| TIMESTAMP equal DEFAULT
	{
	  current_thd->user_time=0;
	}
	| LAST_INSERT_ID equal ULONGLONG_NUM
	{
	  current_thd->insert_id($3);
	}
	| INSERT_ID equal ULONGLONG_NUM
	{
	  current_thd->next_insert_id=$3;
	}
	| GEMINI_SPIN_RETRIES equal ULONG_NUM
	{
	  Lex->gemini_spin_retries= $3;
	}
	| GEMINI_SPIN_RETRIES equal DEFAULT
	{
	  Lex->gemini_spin_retries= 1;
	}
	| CHAR_SYM SET IDENT
	{
	  CONVERT *tmp;
	  if (!(tmp=get_convert_set($3.str)))
	  {
	    net_printf(&current_thd->net,ER_UNKNOWN_CHARACTER_SET,$3);
	    YYABORT;
	  }
	  current_thd->convert_set=tmp;
	}
	| CHAR_SYM SET DEFAULT
	{
	  current_thd->convert_set=0;
	}
	| PASSWORD equal text_or_password
	 {
	   if (change_password(current_thd,current_thd->host,
			       current_thd->priv_user,$3))
	     YYABORT;
	 }
	| PASSWORD FOR_SYM user equal text_or_password
	 {
	   if (change_password(current_thd,
			       $3->host.str ? $3->host.str : current_thd->host,
			       $3->user.str,$5))
	     YYABORT;
	 }
	| '@' ident_or_text equal expr
	  {
	     Item_func_set_user_var *item = new Item_func_set_user_var($2,$4);
	     if (item->fix_fields(current_thd,0) || item->update())
	     { 
		send_error(&current_thd->net, ER_SET_CONSTANTS_ONLY);
	        YYABORT;
	     }
	   }
         | SQL_SLAVE_SKIP_COUNTER equal ULONG_NUM
          {
	    pthread_mutex_lock(&LOCK_slave);
	    if(slave_running)
	      send_error(&current_thd->net, ER_SLAVE_MUST_STOP);
	    else
	      slave_skip_counter = $3;
	    pthread_mutex_unlock(&LOCK_slave);
          }

text_or_password:
	TEXT_STRING { $$=$1.str;}
	| PASSWORD '(' TEXT_STRING ')'
	  {
	    if (!$3.length)
	      $$=$3.str;
	    else
	    {
	      char *buff=(char*) sql_alloc(HASH_PASSWORD_LENGTH+1);
	      make_scrambled_password(buff,$3.str);
	      $$=buff;
	    }
	  }

set_option:
	SQL_BIG_TABLES	        { $$= OPTION_BIG_TABLES; }
	| SQL_BIG_SELECTS	{ $$= OPTION_BIG_SELECTS; }
	| SQL_LOG_OFF		{ $$= OPTION_LOG_OFF; }
	| SQL_LOG_UPDATE
           {
	     $$= (opt_sql_bin_update)? 
                        OPTION_UPDATE_LOG|OPTION_BIN_LOG: 
                        OPTION_UPDATE_LOG ;
	   }
	| SQL_LOG_BIN
           {
	     $$= (opt_sql_bin_update)? 
                        OPTION_UPDATE_LOG|OPTION_BIN_LOG: 
                        OPTION_BIN_LOG ;
	   }
	| SQL_WARNINGS		{ $$= OPTION_WARNINGS; }
	| SQL_LOW_PRIORITY_UPDATES { $$= OPTION_LOW_PRIORITY_UPDATES; }
	| SQL_AUTO_IS_NULL	{ $$= OPTION_AUTO_IS_NULL; }
	| SQL_SAFE_UPDATES	{ $$= OPTION_SAFE_UPDATES; }
	| SQL_BUFFER_RESULT	{ $$= OPTION_BUFFER_RESULT; }
	| SQL_QUOTE_SHOW_CREATE { $$= OPTION_QUOTE_SHOW_CREATE; }


set_isolation:
	GLOBAL_SYM tx_isolation
	{
	  if (check_process_priv())
	    YYABORT;
	  default_tx_isolation= $2;
        }
	| SESSION_SYM tx_isolation
	{ current_thd->session_tx_isolation= $2; }
	| tx_isolation
	{ Lex->tx_isolation= $1; }

tx_isolation:
	TRANSACTION_SYM ISOLATION LEVEL_SYM isolation_types { $$=$4; }

isolation_types:
	READ_SYM UNCOMMITTED_SYM	{ $$= ISO_READ_UNCOMMITTED; }
	| READ_SYM COMMITTED_SYM	{ $$= ISO_READ_COMMITTED; }
	| REPEATABLE_SYM READ_SYM	{ $$= ISO_REPEATABLE_READ; }
	| SERIALIZABLE_SYM		{ $$= ISO_SERIALIZABLE; }

/* Lock function */

lock:
	LOCK_SYM table_or_tables
	{
	  Lex->sql_command=SQLCOM_LOCK_TABLES;
	}
	table_lock_list

table_or_tables:
	TABLE_SYM
	| TABLES

table_lock_list:
	table_lock
	| table_lock_list ',' table_lock

table_lock:
	table_ident opt_table_alias lock_option
	{ if (!add_table_to_list($1,$2,0,(thr_lock_type) $3)) YYABORT; }

lock_option:
	READ_SYM	{ $$=TL_READ_NO_INSERT; }
	| WRITE_SYM     { $$=current_thd->update_lock_default; }
	| LOW_PRIORITY WRITE_SYM { $$=TL_WRITE_LOW_PRIORITY; }
	| READ_SYM LOCAL_SYM { $$= TL_READ; }

unlock:
	UNLOCK_SYM table_or_tables { Lex->sql_command=SQLCOM_UNLOCK_TABLES; }


/* GRANT / REVOKE */

revoke:
	REVOKE
	{
	  Lex->sql_command = SQLCOM_REVOKE;
	  Lex->users_list.empty();
	  Lex->columns.empty();
	  Lex->grant= Lex->grant_tot_col=0;
	  Lex->db=0;
	}
	grant_privileges ON opt_table FROM user_list

grant:
	GRANT
	{
	  Lex->sql_command = SQLCOM_GRANT;
	  Lex->users_list.empty();
	  Lex->columns.empty();
	  Lex->grant= Lex->grant_tot_col=0;
	  Lex->db=0;
	}
	grant_privileges ON opt_table TO_SYM user_list
	grant_option

grant_privileges:
	grant_privilege_list {}
	| ALL PRIVILEGES	{ Lex->grant = UINT_MAX;}
	| ALL			{ Lex->grant = UINT_MAX;}

grant_privilege_list:
	grant_privilege
	| grant_privilege_list ',' grant_privilege

grant_privilege:
	SELECT_SYM
	  { Lex->which_columns = SELECT_ACL;}
	  opt_column_list
	| INSERT
	  { Lex->which_columns = INSERT_ACL; }
	  opt_column_list
	| UPDATE_SYM
	  { Lex->which_columns = UPDATE_ACL; }
	  opt_column_list
	| DELETE_SYM { Lex->grant |= DELETE_ACL;}
	| REFERENCES { Lex->which_columns = REFERENCES_ACL;} opt_column_list
	| USAGE {}
	| INDEX		{ Lex->grant |= INDEX_ACL;}
	| ALTER		{ Lex->grant |= ALTER_ACL;}
	| CREATE	{ Lex->grant |= CREATE_ACL;}
	| DROP		{ Lex->grant |= DROP_ACL;}
	| RELOAD	{ Lex->grant |= RELOAD_ACL;}
	| SHUTDOWN	{ Lex->grant |= SHUTDOWN_ACL;}
	| PROCESS	{ Lex->grant |= PROCESS_ACL;}
	| FILE_SYM	{ Lex->grant |= FILE_ACL;}
	| GRANT OPTION  { Lex->grant |= GRANT_ACL;}

opt_table:
	'*'
	  {
	    Lex->db=current_thd->db;
	    if (Lex->grant == UINT_MAX)
	      Lex->grant = DB_ACLS & ~GRANT_ACL;
	    else if (Lex->columns.elements)
	    {
	       net_printf(&current_thd->net,ER_ILLEGAL_GRANT_FOR_TABLE);
	       YYABORT;
	     }
	  }
	| ident '.' '*'
	  {
	    Lex->db = $1.str;
	    if (Lex->grant == UINT_MAX)
	      Lex->grant = DB_ACLS & ~GRANT_ACL;
	    else if (Lex->columns.elements)
	    {
	      net_printf(&current_thd->net,ER_ILLEGAL_GRANT_FOR_TABLE);
	      YYABORT;
	    }
	  }
	| '*' '.' '*'
	  {
	    Lex->db = NULL;
	    if (Lex->grant == UINT_MAX)
	      Lex->grant = GLOBAL_ACLS & ~GRANT_ACL;
	    else if (Lex->columns.elements)
	    {
	      net_printf(&current_thd->net,ER_ILLEGAL_GRANT_FOR_TABLE);
	      YYABORT;
	    }
	  }
	| table_ident
	  {
	    if (!add_table_to_list($1,NULL,0))
	      YYABORT;
	    if (Lex->grant == UINT_MAX)
	      Lex->grant =  TABLE_ACLS & ~GRANT_ACL;
	  }


user_list:
	grant_user	     { if (Lex->users_list.push_back($1)) YYABORT;}
	| user_list ',' grant_user { if (Lex->users_list.push_back($3)) YYABORT;}


grant_user:
	user IDENTIFIED_SYM BY TEXT_STRING
	{
	   $$=$1; $1->password=$4;
	   if ($4.length)
	   {
	     char *buff=(char*) sql_alloc(HASH_PASSWORD_LENGTH+1);
	     if (buff)
	     {
	       make_scrambled_password(buff,$4.str);
	       $1->password.str=buff;
	       $1->password.length=HASH_PASSWORD_LENGTH;
	     }
	  }
	}
	| user IDENTIFIED_SYM BY PASSWORD TEXT_STRING
	  { $$=$1; $1->password=$5 ; }
	| user
	  { $$=$1; $1->password.str=NullS; }


opt_column_list:
	/* empty */ { Lex->grant |= Lex->which_columns; }
	| '(' column_list ')'

column_list:
	column_list ',' column_list_id
	| column_list_id

column_list_id:
	ident
	{
	  String *new_str = new String((const char*) $1.str,$1.length);
	  List_iterator <LEX_COLUMN> iter(Lex->columns);
	  class LEX_COLUMN *point;
	  while ((point=iter++))
	  {
	    if (!my_strcasecmp(point->column.ptr(),new_str->ptr()))
		break;
	  }
	  Lex->grant_tot_col|= Lex->which_columns;
	  if (point)
	    point->rights |= Lex->which_columns;
	  else
	    Lex->columns.push_back(new LEX_COLUMN (*new_str,Lex->which_columns));
	}

grant_option:
	/* empty */ {}
	| WITH GRANT OPTION { Lex->grant |= GRANT_ACL;}

begin:
	BEGIN_SYM   { Lex->sql_command = SQLCOM_BEGIN;} opt_work

opt_work:
	/* empty */ {}
	| WORK_SYM {}

commit:
	COMMIT_SYM   { Lex->sql_command = SQLCOM_COMMIT;}

rollback:
	ROLLBACK_SYM { Lex->sql_command = SQLCOM_ROLLBACK;}
