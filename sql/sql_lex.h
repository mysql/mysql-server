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


/* YACC and LEX Definitions */

/* These may not be declared yet */
class Table_ident;
class sql_exchange;
class LEX_COLUMN;
class sp_head;
class sp_name;
class sp_instr;
class sp_pcontext;

/*
  The following hack is needed because mysql_yacc.cc does not define
  YYSTYPE before including this file
*/

#include "set_var.h"

#ifdef MYSQL_YACC
#define LEX_YYSTYPE void *
#else
#include "lex_symbol.h"
#include "sql_yacc.h"
#define LEX_YYSTYPE YYSTYPE *
#endif

/*
  When a command is added here, be sure it's also added in mysqld.cc
  in "struct show_var_st status_vars[]= {" ...
*/

enum enum_sql_command {
  SQLCOM_SELECT, SQLCOM_CREATE_TABLE, SQLCOM_CREATE_INDEX, SQLCOM_ALTER_TABLE,
  SQLCOM_UPDATE, SQLCOM_INSERT, SQLCOM_INSERT_SELECT,
  SQLCOM_DELETE, SQLCOM_TRUNCATE, SQLCOM_DROP_TABLE, SQLCOM_DROP_INDEX,

  SQLCOM_SHOW_DATABASES, SQLCOM_SHOW_TABLES, SQLCOM_SHOW_FIELDS,
  SQLCOM_SHOW_KEYS, SQLCOM_SHOW_VARIABLES, SQLCOM_SHOW_LOGS, SQLCOM_SHOW_STATUS,
  SQLCOM_SHOW_INNODB_STATUS,
  SQLCOM_SHOW_PROCESSLIST, SQLCOM_SHOW_MASTER_STAT, SQLCOM_SHOW_SLAVE_STAT,
  SQLCOM_SHOW_GRANTS, SQLCOM_SHOW_CREATE, SQLCOM_SHOW_CHARSETS,
  SQLCOM_SHOW_COLLATIONS, SQLCOM_SHOW_CREATE_DB,

  SQLCOM_LOAD,SQLCOM_SET_OPTION,SQLCOM_LOCK_TABLES,SQLCOM_UNLOCK_TABLES,
  SQLCOM_GRANT,
  SQLCOM_CHANGE_DB, SQLCOM_CREATE_DB, SQLCOM_DROP_DB, SQLCOM_ALTER_DB,
  SQLCOM_REPAIR, SQLCOM_REPLACE, SQLCOM_REPLACE_SELECT,
  SQLCOM_CREATE_FUNCTION, SQLCOM_DROP_FUNCTION,
  SQLCOM_REVOKE,SQLCOM_OPTIMIZE, SQLCOM_CHECK, 
  SQLCOM_ASSIGN_TO_KEYCACHE, SQLCOM_PRELOAD_KEYS,
  SQLCOM_FLUSH, SQLCOM_KILL, SQLCOM_ANALYZE,
  SQLCOM_ROLLBACK, SQLCOM_ROLLBACK_TO_SAVEPOINT,
  SQLCOM_COMMIT, SQLCOM_SAVEPOINT,
  SQLCOM_SLAVE_START, SQLCOM_SLAVE_STOP,
  SQLCOM_BEGIN, SQLCOM_LOAD_MASTER_TABLE, SQLCOM_CHANGE_MASTER,
  SQLCOM_RENAME_TABLE, SQLCOM_BACKUP_TABLE, SQLCOM_RESTORE_TABLE,
  SQLCOM_RESET, SQLCOM_PURGE, SQLCOM_PURGE_BEFORE, SQLCOM_SHOW_BINLOGS,
  SQLCOM_SHOW_OPEN_TABLES, SQLCOM_LOAD_MASTER_DATA,
  SQLCOM_HA_OPEN, SQLCOM_HA_CLOSE, SQLCOM_HA_READ,
  SQLCOM_SHOW_SLAVE_HOSTS, SQLCOM_DELETE_MULTI, SQLCOM_UPDATE_MULTI,
  SQLCOM_SHOW_BINLOG_EVENTS, SQLCOM_SHOW_NEW_MASTER, SQLCOM_DO,
  SQLCOM_SHOW_WARNS, SQLCOM_EMPTY_QUERY, SQLCOM_SHOW_ERRORS,
  SQLCOM_SHOW_COLUMN_TYPES, SQLCOM_SHOW_STORAGE_ENGINES, SQLCOM_SHOW_PRIVILEGES,
  SQLCOM_HELP, SQLCOM_DROP_USER, SQLCOM_REVOKE_ALL, SQLCOM_CHECKSUM,
  SQLCOM_CREATE_PROCEDURE, SQLCOM_CREATE_SPFUNCTION, SQLCOM_CALL,
  SQLCOM_DROP_PROCEDURE, SQLCOM_ALTER_PROCEDURE,SQLCOM_ALTER_FUNCTION,
  SQLCOM_SHOW_CREATE_PROC, SQLCOM_SHOW_CREATE_FUNC,
  SQLCOM_SHOW_STATUS_PROC, SQLCOM_SHOW_STATUS_FUNC,
  SQLCOM_PREPARE, SQLCOM_EXECUTE, SQLCOM_DEALLOCATE_PREPARE,
  SQLCOM_CREATE_VIEW, SQLCOM_DROP_VIEW,
  /* This should be the last !!! */
  SQLCOM_END
};

// describe/explain types
#define DESCRIBE_NORMAL		1
#define DESCRIBE_EXTENDED	2

enum suid_behaviour
{
  IS_DEFAULT_SUID= 0, IS_NOT_SUID, IS_SUID
};

#define DERIVED_SUBQUERY	1
#define DERIVED_VIEW		2

enum enum_view_create_mode
{
  VIEW_CREATE_NEW,		// check that there are not such VIEW/table
  VIEW_ALTER,			// check that VIEW .frm with such name exists
  VIEW_CREATE_OR_REPLACE	// check only that there are not such table
};

enum enum_drop_mode
{
  DROP_DEFAULT, // mode is not specified
  DROP_CASCADE, // CASCADE option
  DROP_RESTRICT // RESTRICT option
};

typedef List<Item> List_item;

typedef struct st_lex_master_info
{
  char *host, *user, *password, *log_file_name;
  uint port, connect_retry;
  ulonglong pos;
  ulong server_id;
  /* 
     Variable for MASTER_SSL option.
     MASTER_SSL=0 in CHANGE MASTER TO corresponds to SSL_DISABLE
     MASTER_SSL=1 corresponds to SSL_ENABLE
  */
  enum {SSL_UNCHANGED=0, SSL_DISABLE, SSL_ENABLE} ssl; 
  char *ssl_key, *ssl_cert, *ssl_ca, *ssl_capath, *ssl_cipher;
  char *relay_log_name;
  ulong relay_log_pos;
} LEX_MASTER_INFO;


enum sub_select_type
{
  UNSPECIFIED_TYPE,UNION_TYPE, INTERSECT_TYPE,
  EXCEPT_TYPE, GLOBAL_OPTIONS_TYPE, DERIVED_TABLE_TYPE, OLAP_TYPE
};

enum olap_type 
{
  UNSPECIFIED_OLAP_TYPE, CUBE_TYPE, ROLLUP_TYPE
};

enum tablespace_op_type
{
  NO_TABLESPACE_OP, DISCARD_TABLESPACE, IMPORT_TABLESPACE
};

/* 
  The state of the lex parsing for selects 
   
   All select describing structures linked with following pointers:
   - list of neighbors (next/prev) (prev of first element point to slave 
     pointer of upper structure)
     - one level units for unit (union) structure
     - member of one union(unit) for ordinary select_lex
   - pointer to master
     - outer select_lex for unit (union)
     - unit structure for ordinary select_lex
   - pointer to slave
     - first list element of select_lex belonged to this unit for unit
     - first unit in list of units that belong to this select_lex (as
       subselects or derived tables) for ordinary select_lex
   - list of all select_lex (for group operation like correcting list of opened
     tables)
   - if unit contain several selects (union) then it have special 
     select_lex called fake_select_lex. It used for storing global parameters
     and executing union. subqueries of global ORDER BY clause will be
     attached to this fake_select_lex, which will allow them correctly
     resolve fields of 'upper' union and other more outer selects. 

   for example for following query:

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

/* 
    Base class for st_select_lex (SELECT_LEX) & 
    st_select_lex_unit (SELECT_LEX_UNIT)
*/
struct st_lex;
class st_select_lex;
class st_select_lex_unit;
class st_select_lex_node {
protected:
  st_select_lex_node *next, **prev,   /* neighbor list */
    *master, *slave,                  /* vertical links */
    *link_next, **link_prev;          /* list of whole SELECT_LEX */
public:
  enum enum_parsing_place
  {
    NO_MATTER,
    IN_HAVING,
    SELECT_LIST
  };

  ulong options;
  /*
    result of this query can't be cached, bit field, can be :
      UNCACHEABLE_DEPENDENT
      UNCACHEABLE_RAND
      UNCACHEABLE_SIDEEFFECT
      UNCACHEABLE_EXPLAIN
  */
  uint8 uncacheable;
  enum sub_select_type linkage;
  bool no_table_names_allowed; /* used for global order by */
  bool no_error; /* suppress error message (convert it to warnings) */

  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint) size);
  }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint) size); }
  static void operator delete(void *ptr,size_t size) {}
  st_select_lex_node(): linkage(UNSPECIFIED_TYPE) {}
  virtual ~st_select_lex_node() {}
  inline st_select_lex_node* get_master() { return master; }
  virtual void init_query();
  virtual void init_select();
  void include_down(st_select_lex_node *upper);
  void include_neighbour(st_select_lex_node *before);
  void include_standalone(st_select_lex_node *sel, st_select_lex_node **ref);
  void include_global(st_select_lex_node **plink);
  void exclude();

  virtual st_select_lex_unit* master_unit()= 0;
  virtual st_select_lex* outer_select()= 0;
  virtual st_select_lex* return_after_parsing()= 0;

  virtual bool set_braces(bool value);
  virtual bool inc_in_sum_expr();
  virtual uint get_in_sum_expr();
  virtual TABLE_LIST* get_table_list();
  virtual List<Item>* get_item_list();
  virtual List<String>* get_use_index();
  virtual List<String>* get_ignore_index();
  virtual ulong get_table_join_options();
  virtual TABLE_LIST *add_table_to_list(THD *thd, Table_ident *table,
					LEX_STRING *alias,
					ulong table_options,
					thr_lock_type flags= TL_UNLOCK,
					List<String> *use_index= 0,
					List<String> *ignore_index= 0,
                                        LEX_STRING *option= 0);
  virtual void set_lock_for_tables(thr_lock_type lock_type) {}

  friend class st_select_lex_unit;
  friend bool mysql_new_select(struct st_lex *lex, bool move_down);
  friend my_bool mysql_make_view (File_parser *parser,
				  TABLE_LIST *table);
private:
  void fast_exclude();
};
typedef class st_select_lex_node SELECT_LEX_NODE;

/* 
   SELECT_LEX_UNIT - unit of selects (UNION, INTERSECT, ...) group 
   SELECT_LEXs
*/
struct st_lex;
class THD;
class select_result;
class JOIN;
class select_union;
class st_select_lex_unit: public st_select_lex_node {
protected:
  TABLE_LIST result_table_list;
  select_union *union_result;
  TABLE *table; /* temporary table using for appending UNION results */

  select_result *result;
  int res;
  ulong found_rows_for_union;
  bool  prepared, // prepare phase already performed for UNION (unit)
    optimized, // optimize phase already performed for UNION (unit)
    executed, // already executed
    cleaned;

public:
  // list of fields which points to temporary table for union
  List<Item> item_list;
  /*
    list of types of items inside union (used for union & derived tables)
    
    Item_type_holders from which this list consist may have pointers to Field,
    pointers is valid only after preparing SELECTS of this unit and before
    any SELECT of this unit execution
  */
  List<Item> types;
  /*
    Pointer to 'last' select or pointer to unit where stored
    global parameters for union
  */
  st_select_lex *global_parameters;
  //node on wich we should return current_select pointer after parsing subquery
  st_select_lex *return_to;
  /* LIMIT clause runtime counters */
  ha_rows select_limit_cnt, offset_limit_cnt;
  /* not NULL if unit used in subselect, point to subselect item */
  Item_subselect *item;
  /* thread handler */
  THD *thd;
  /*
    SELECT_LEX for hidden SELECT in onion which process global
    ORDER BY and LIMIT
  */
  st_select_lex *fake_select_lex;

  st_select_lex *union_distinct; /* pointer to the last UNION DISTINCT */
  bool describe; /* union exec() called for EXPLAIN */

  void init_query();
  st_select_lex_unit* master_unit();
  st_select_lex* outer_select();
  st_select_lex* first_select()
  {
    return my_reinterpret_cast(st_select_lex*)(slave);
  }
  st_select_lex* first_select_in_union() 
  { 
    return my_reinterpret_cast(st_select_lex*)(slave);
  }
  st_select_lex_unit* next_unit()
  {
    return my_reinterpret_cast(st_select_lex_unit*)(next);
  }
  st_select_lex* return_after_parsing() { return return_to; }
  void exclude_level();
  void exclude_tree();

  /* UNION methods */
  int prepare(THD *thd, select_result *result, ulong additional_options);
  int exec();
  int cleanup();
  inline void unclean() { cleaned= 0; }
  void reinit_exec_mechanism();

  bool check_updateable(char *db, char *table);
  void print(String *str);

  ulong init_prepare_fake_select_lex(THD *thd);
  int change_result(select_subselect *result, select_subselect *old_result);
  void set_limit(st_select_lex *values, st_select_lex *sl);

  friend void mysql_init_query(THD *thd, uchar *buf, uint length, bool lexonly);
  friend int subselect_union_engine::exec();
};
typedef class st_select_lex_unit SELECT_LEX_UNIT;

/*
  SELECT_LEX - store information of parsed SELECT_LEX statment
*/
class st_select_lex: public st_select_lex_node
{
public:
  char *db, *db1, *table1, *db2, *table2;      	/* For outer join using .. */
  Item *where, *having;                         /* WHERE & HAVING clauses */
  Item *prep_where; /* saved WHERE clause for prepared statement processing */
  /* point on lex in which it was created, used in view subquery detection */
  st_lex *parent_lex;
  enum olap_type olap;
  SQL_LIST	      table_list, group_list;   /* FROM & GROUP BY clauses */
  List<Item>          item_list; /* list of fields & expressions */
  List<String>        interval_list, use_index, *use_index_ptr,
		      ignore_index, *ignore_index_ptr;
  /* 
    Usualy it is pointer to ftfunc_list_alloc, but in union used to create fake
    select_lex for calling mysql_select under results of union
  */
  List<Item_func_match> *ftfunc_list;
  List<Item_func_match> ftfunc_list_alloc;
  JOIN *join; /* after JOIN::prepare it is pointer to corresponding JOIN */
  List<TABLE_LIST> top_join_list; /* join list of the top level          */
  List<TABLE_LIST> *join_list;    /* list for the currently parsed join  */
  TABLE_LIST *embedding;          /* table embedding to the above list   */
  const char *type;               /* type of select for EXPLAIN          */

  SQL_LIST order_list;                /* ORDER clause */
  List<List_item>     expr_list;
  List<List_item>     when_list;      /* WHEN clause (expression) */
  SQL_LIST *gorder_list;
  ha_rows select_limit, offset_limit; /* LIMIT clause parameters */
  // Arrays of pointers to top elements of all_fields list
  Item **ref_pointer_array;

  /*
    number of items in select_list and HAVING clause used to get number
    bigger then can be number of entries that will be added to all item
    list during split_sum_func
  */
  uint select_n_having_items;
  uint cond_count;      /* number of arguments of and/or/xor in where/having */
  enum_parsing_place parsing_place; /* where we are parsing expression */
  bool with_sum_func;   /* sum function indicator */
  /* 
    PS or SP cond natural joins was alredy processed with permanent
    arena and all additional items which we need alredy stored in it
  */
  bool conds_processed_with_permanent_arena;

  ulong table_join_options;
  uint in_sum_expr;
  uint select_number; /* number of select (used for EXPLAIN) */
  uint with_wild; /* item list contain '*' */
  bool  braces;   	/* SELECT ... UNION (SELECT ... ) <- this braces */
  /* TRUE when having fix field called in processing of this SELECT */
  bool having_fix_field;
  /* explicit LIMIT clause was used */
  bool explicit_limit;
  /*
    there are subquery in HAVING clause => we can't close tables before
    query processing end even if we use temporary table
  */
  bool subquery_in_having;
  bool first_execution; /* first execution in SP or PS */
  bool first_cond_optimization;
  /* do not wrap view fields with Item_ref */
  bool no_wrap_view_item;

  /* 
     SELECT for SELECT command st_select_lex. Used to privent scaning
     item_list of non-SELECT st_select_lex (no sense find to finding
     reference in it (all should be in tables, it is dangerouse due
     to order of fix_fields calling for non-SELECTs commands (item list
     can be not fix_fieldsd)). This value will be assigned for
     primary select (sql_yac.yy) and for any subquery and
     UNION SELECT (sql_parse.cc mysql_new_select())


     INSERT for primary st_select_lex structure of simple INSERT/REPLACE
     (used for name resolution, see Item_fiels & Item_ref fix_fields,
     FALSE for INSERT/REPLACE ... SELECT, because it's
     st_select_lex->table_list will be preprocessed (first table removed)
     before passing to handle_select)

     NOMATTER for other
  */
  enum {NOMATTER_MODE, SELECT_MODE, INSERT_MODE} resolve_mode;


  void init_query();
  void init_select();
  st_select_lex_unit* master_unit();
  st_select_lex_unit* first_inner_unit() 
  { 
    return (st_select_lex_unit*) slave; 
  }
  st_select_lex* outer_select();
  st_select_lex* next_select() { return (st_select_lex*) next; }
  st_select_lex* next_select_in_list() 
  {
    return (st_select_lex*) link_next;
  }
  st_select_lex_node** next_select_in_list_addr()
  {
    return &link_next;
  }
  st_select_lex* return_after_parsing()
  {
    return master_unit()->return_after_parsing();
  }

  void mark_as_dependent(st_select_lex *last);

  bool set_braces(bool value);
  bool inc_in_sum_expr();
  uint get_in_sum_expr();

  bool add_item_to_list(THD *thd, Item *item);
  bool add_group_to_list(THD *thd, Item *item, bool asc);
  bool add_ftfunc_to_list(Item_func_match *func);
  bool add_order_to_list(THD *thd, Item *item, bool asc);
  TABLE_LIST* add_table_to_list(THD *thd, Table_ident *table,
				LEX_STRING *alias,
				ulong table_options,
				thr_lock_type flags= TL_UNLOCK,
				List<String> *use_index= 0,
				List<String> *ignore_index= 0,
                                LEX_STRING *option= 0);
  TABLE_LIST* get_table_list();
  bool init_nested_join(THD *thd);
  TABLE_LIST *end_nested_join(THD *thd);
  TABLE_LIST *nest_last_join(THD *thd);
  void save_names_for_using_list(TABLE_LIST *tab1, TABLE_LIST *tab2);
  void add_joined_table(TABLE_LIST *table);
  TABLE_LIST *convert_right_join();
  List<Item>* get_item_list();
  List<String>* get_use_index();
  List<String>* get_ignore_index();
  ulong get_table_join_options();
  void set_lock_for_tables(thr_lock_type lock_type);
  inline void init_order()
  {
    order_list.elements= 0;
    order_list.first= 0;
    order_list.next= (byte**) &order_list.first;
  }
  
  bool test_limit();

  friend void mysql_init_query(THD *thd, uchar *buf, uint length, bool lexonly);
  st_select_lex() {}
  void make_empty_select()
  {
    init_query();
    init_select();
  }
  bool setup_ref_array(THD *thd, uint order_group_num);
  bool check_updateable(char *db, char *table);
  void print(THD *thd, String *str);
  static void print_order(String *str, ORDER *order);
  void print_limit(THD *thd, String *str);
  void fix_prepare_information(THD *thd, Item **conds);
};
typedef class st_select_lex SELECT_LEX;

#define ALTER_ADD_COLUMN	1
#define ALTER_DROP_COLUMN	2
#define ALTER_CHANGE_COLUMN	4
#define ALTER_ADD_INDEX		8
#define ALTER_DROP_INDEX	16
#define ALTER_RENAME		32
#define ALTER_ORDER		64
#define ALTER_OPTIONS		128

typedef struct st_alter_info
{
  List<Alter_drop>            drop_list;
  List<Alter_column>          alter_list;
  uint                        flags;
  enum enum_enable_or_disable keys_onoff;
  enum tablespace_op_type     tablespace_op;
  bool                        is_simple;

  st_alter_info(){clear();}
  void clear(){keys_onoff= LEAVE_AS_IS;tablespace_op= NO_TABLESPACE_OP;}
  void reset(){drop_list.empty();alter_list.empty();clear();}
} ALTER_INFO;

struct st_sp_chistics
{
  LEX_STRING comment;
  enum suid_behaviour suid;
  bool detistic;
};

/* The state of the lex parsing. This is saved in the THD struct */

typedef struct st_lex
{
  uint	 yylineno,yytoklen;			/* Simulate lex */
  LEX_YYSTYPE yylval;
  SELECT_LEX_UNIT unit;                         /* most upper unit */
  SELECT_LEX select_lex;                        /* first SELECT_LEX */
  /* current SELECT_LEX in parsing */
  SELECT_LEX *current_select;
  /* list of all SELECT_LEX */
  SELECT_LEX *all_selects_list;
  uchar *buf;			/* The beginning of string, used by SPs */
  uchar *ptr,*tok_start,*tok_end,*end_of_query;
  char *length,*dec,*change,*name;
  char *help_arg;
  char *backup_dir;				/* For RESTORE/BACKUP */
  char* to_log;                                 /* For PURGE MASTER LOGS TO */
  time_t purge_time;                            /* For PURGE MASTER LOGS BEFORE */
  char* x509_subject,*x509_issuer,*ssl_cipher;
  char* found_colon;                            /* For multi queries - next query */
  String *wild;
  sql_exchange *exchange;
  select_result *result;
  Item *default_value, *on_update_value;
  LEX_STRING *comment, name_and_length;
  LEX_USER *grant_user;
  gptr yacc_yyss,yacc_yyvs;
  THD *thd;
  CHARSET_INFO *charset;
  TABLE_LIST *query_tables;	/* global list of all tables in this query */
  /* last element next_global of previous list */
  TABLE_LIST **query_tables_last;
  TABLE_LIST *proc_table; /* refer to mysql.proc if it was opened by VIEW */

  List<key_part_spec> col_list;
  List<key_part_spec> ref_list;
  List<String>	      interval_list;
  List<LEX_USER>      users_list;
  List<LEX_COLUMN>    columns;
  List<Key>	      key_list;
  List<create_field>  create_list;
  List<Item>	      *insert_list,field_list,value_list;
  List<List_item>     many_values;
  List<set_var_base>  var_list;
  List<Item_param>    param_list;
  List<LEX_STRING>    view_list; // view list (list of field names in view)
  SQL_LIST	      proc_list, auxilliary_table_list, save_list;
  TYPELIB	      *interval;
  create_field	      *last_field;
  char		      *savepoint_name;		// Transaction savepoint id
  udf_func udf;
  HA_CHECK_OPT   check_opt;			// check/repair options
  HA_CREATE_INFO create_info;
  LEX_MASTER_INFO mi;				// used by CHANGE MASTER
  USER_RESOURCES mqh;
  ulong thread_id,type;
  enum_sql_command sql_command;
  thr_lock_type lock_option;
  enum SSL_type ssl_type;			/* defined in violite.h */
  enum my_lex_states next_state;
  enum enum_duplicates duplicates;
  enum enum_tx_isolation tx_isolation;
  enum enum_ha_read_modes ha_read_mode;
  enum ha_rkey_function ha_rkey_mode;
  enum enum_var_type option_type;
  enum enum_view_create_mode create_view_mode;
  enum enum_drop_mode drop_mode;
  uint uint_geom_type;
  uint grant, grant_tot_col, which_columns;
  uint fk_delete_opt, fk_update_opt, fk_match_option;
  uint slave_thd_opt;
  uint8 describe;
  uint8 derived_tables;
  uint8 create_view_algorithm;
  uint8 create_view_check;
  bool drop_if_exists, drop_temporary, local_file, one_shot_set;
  bool in_comment, ignore_space, verbose, no_write_to_binlog;
  /* special JOIN::prepare mode: changing of query is prohibited */
  bool view_prepare_mode;
  bool safe_to_cache_query;
  bool variables_used;
  ALTER_INFO alter_info;
  /* Prepared statements SQL syntax:*/
  LEX_STRING prepared_stmt_name; /* Statement name (in all queries) */
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
  bool sp_lex_in_use;	/* Keep track on lex usage in SPs for error handling */
  sp_pcontext *spcont;
  HASH spfuns;		/* Called functions */
  st_sp_chistics sp_chistics;
  bool only_view;       /* used for SHOW CREATE TABLE/VIEW */
  /*
    field_list was created for view and should be removed before PS/SP
    rexecuton
  */
  bool empty_field_list_on_rset;

  st_lex()
  {
    bzero((char *)&spfuns, sizeof(spfuns));
  }
  
  ~st_lex()
  {
    if (spfuns.array.buffer)
      hash_free(&spfuns);
  }

  inline void uncacheable(uint8 cause)
  {
    safe_to_cache_query= 0;

    /*
      There are no sense to mark select_lex and union fields of LEX,
      but we should merk all subselects as uncacheable from current till
      most upper
    */
    SELECT_LEX *sl;
    SELECT_LEX_UNIT *un;
    for (sl= current_select, un= sl->master_unit();
	 un != &unit;
	 sl= sl->outer_select(), un= sl->master_unit())
    {
      sl->uncacheable|= cause;
      un->uncacheable|= cause;
    }
  }
  TABLE_LIST *unlink_first_table(bool *link_to_local);
  void link_first_table_back(TABLE_LIST *first, bool link_to_local);
  void first_lists_tables_same();

  bool can_be_merged();
  bool can_use_merged();
  bool can_not_use_merged();
  bool only_view_structure();
} LEX;

struct st_lex_local: public st_lex
{
  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint) size);
  }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  {
    return (void*) alloc_root(mem_root, (uint) size);
  }
  static void operator delete(void *ptr,size_t size) {}
};

void lex_init(void);
void lex_free(void);
void lex_start(THD *thd, uchar *buf,uint length);
void lex_end(LEX *lex);

extern pthread_key(LEX*,THR_LEX);

#define current_lex (current_thd->lex)
