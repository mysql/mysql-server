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

#ifndef _MYSQL_PRIV_H
#define _MYSQL_PRIV_H

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include "mysql_version.h"
#include <hash.h>
#include <signal.h>
#include <thr_lock.h>
#include <my_base.h>			/* Needed by field.h */
#include <my_bitmap.h>
#include <violite.h>

#undef write  // remove pthread.h macro definition for EMX

typedef ulong table_map;		/* Used for table bits in join */
typedef ulong key_map;			/* Used for finding keys */
typedef ulong key_part_map;		/* Used for finding key parts */

#include "mysql_com.h"
#include "unireg.h"

void init_sql_alloc(MEM_ROOT *root, uint block_size, uint pre_alloc_size);
gptr sql_alloc(unsigned size);
gptr sql_calloc(unsigned size);
char *sql_strdup(const char *str);
char *sql_strmake(const char *str,uint len);
gptr sql_memdup(const void * ptr,unsigned size);
void sql_element_free(void *ptr);
void kill_one_thread(THD *thd, ulong id);

#define x_free(A)	{ my_free((gptr) (A),MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR)); }
#define safeFree(x)	{ if(x) { my_free((gptr) x,MYF(0)); x = NULL; } }
#define PREV_BITS(type,A)	((type) (((type) 1 << (A)) -1))
#define all_bits_set(A,B) ((A) & (B) != (B))

#ifndef LL
#ifdef HAVE_LONG_LONG
#define LL(A) A ## LL
#else
#define LL(A) A ## L
#endif
#endif

/***************************************************************************
  Configuration parameters
****************************************************************************/

#define ACL_CACHE_SIZE		256
#define HASH_PASSWORD_LENGTH	16
#define HOST_CACHE_SIZE		128
#define MAX_ACCEPT_RETRY	10	// Test accept this many times
#define MAX_BLOB_WIDTH		8192	// Default width for blob
#define MAX_FIELDS_BEFORE_HASH	32
#define USER_VARS_HASH_SIZE     16
#define STACK_MIN_SIZE		8192	// Abort if less stack during eval.
#ifndef MYSQLD_NET_RETRY_COUNT
#define MYSQLD_NET_RETRY_COUNT  10	// Abort read after this many int.
#endif
#define TEMP_POOL_SIZE          128
/* The following parameters is to decide when to use an extra cache to
   optimise seeks when reading a big table in sorted order */
#define MIN_FILE_LENGTH_TO_USE_ROW_CACHE (16L*1024*1024)
#define MIN_ROWS_TO_USE_TABLE_CACHE	 100

// The following is used to decide if MySQL should use table scanning
// instead of reading with keys.  The number says how many evaluation of the
// WHERE clause is comparable to reading one extra row from a table.
#define TIME_FOR_COMPARE   5	// 5 compares == one read
// Number of rows in a reference table when refereed through a not unique key.
// This value is only used when we don't know anything about the key
// distribution.
#define MATCHING_ROWS_IN_OTHER_TABLE 10

/* Don't pack string keys shorter than this (if PACK_KEYS=1 isn't used) */
#define KEY_DEFAULT_PACK_LENGTH 8

/* Characters shown for the command in 'show processlist' */
#define PROCESS_LIST_WIDTH 100

/* Time handling defaults */
#define TIMESTAMP_MAX_YEAR 2038
#define YY_PART_YEAR	   70
#define PRECISION_FOR_DOUBLE 53
#define PRECISION_FOR_FLOAT  24

/* The following can also be changed from the command line */
#define CONNECT_TIMEOUT		5		// Do not wait long for connect
#define DEFAULT_CONCURRENCY	10
#define DELAYED_LIMIT		100		/* pause after xxx inserts */
#define DELAYED_QUEUE_SIZE	1000
#define DELAYED_WAIT_TIMEOUT	5*60		/* Wait for delayed insert */
#define FLUSH_TIME		0		/* Don't flush tables */
#define MAX_CONNECT_ERRORS	10		// errors before disabling host

#ifdef	__WIN__
#define IF_WIN(A,B) (A)
#undef	FLUSH_TIME
#define FLUSH_TIME	1800			/* Flush every half hour */

#define INTERRUPT_PRIOR -2
#define CONNECT_PRIOR	-1
#define WAIT_PRIOR	0
#define QUERY_PRIOR	2
#else
#define IF_WIN(A,B) (B)
#define INTERRUPT_PRIOR 10
#define CONNECT_PRIOR	9
#define WAIT_PRIOR	8
#define QUERY_PRIOR	6
#endif /* __WIN92__ */

	/* Bits from testflag */
#define TEST_PRINT_CACHED_TABLES 1
#define TEST_NO_KEY_GROUP	 2
#define TEST_MIT_THREAD		4
#define TEST_BLOCKING		8
#define TEST_KEEP_TMP_TABLES	16
#define TEST_NO_THREADS		32	/* For debugging under Linux */
#define TEST_READCHECK		64	/* Force use of readcheck */
#define TEST_NO_EXTRA		128
#define TEST_CORE_ON_SIGNAL	256	/* Give core if signal */
#define TEST_NO_STACKTRACE	512

/* options for select set by the yacc parser (stored in lex->options) */
#define SELECT_DISTINCT		1
#define SELECT_STRAIGHT_JOIN	2
#define SELECT_DESCRIBE		4
#define SELECT_SMALL_RESULT	8
#define SELECT_BIG_RESULT	16
#define SELECT_HIGH_PRIORITY	64		/* Intern */
#define SELECT_USE_CACHE	256		/* Intern */
#define SELECT_COUNT_DISTINCT	512		/* Intern */

#define OPTION_BIG_TABLES	512		/* for SQL OPTION */
#define OPTION_BIG_SELECTS	1024		/* for SQL OPTION */
#define OPTION_LOG_OFF		2048
#define OPTION_UPDATE_LOG	4096		/* update log flag */
#define OPTION_LOW_PRIORITY_UPDATES	8192
#define OPTION_WARNINGS		16384
#define OPTION_AUTO_IS_NULL	32768
#define OPTION_ANSI_MODE	65536L
#define OPTION_SAFE_UPDATES	OPTION_ANSI_MODE*2
#define OPTION_BUFFER_RESULT	OPTION_SAFE_UPDATES*2
#define OPTION_BIN_LOG          OPTION_BUFFER_RESULT*2
#define OPTION_NOT_AUTO_COMMIT	OPTION_BIN_LOG*2
#define OPTION_BEGIN		OPTION_NOT_AUTO_COMMIT*2
#define OPTION_QUICK		OPTION_BEGIN*2
#define OPTION_QUOTE_SHOW_CREATE OPTION_QUICK*2
#define OPTION_INTERNAL_SUBTRANSACTIONS OPTION_QUOTE_SHOW_CREATE*2

/* Set if we are updating a non-transaction safe table */
#define OPTION_STATUS_NO_TRANS_UPDATE OPTION_INTERNAL_SUBTRANSACTIONS*2

/* The following is set when parsing the query */
#define QUERY_NO_INDEX_USED		OPTION_STATUS_NO_TRANS_UPDATE*2
#define QUERY_NO_GOOD_INDEX_USED	QUERY_NO_INDEX_USED*2

#define RAID_BLOCK_SIZE 1024

/* BINLOG_DUMP options */

#define BINLOG_DUMP_NON_BLOCK   1

/* sql_show.cc:show_log_files() */
#define SHOW_LOG_STATUS_FREE "FREE"
#define SHOW_LOG_STATUS_INUSE "IN USE"

/* Some portable defines */

#define portable_sizeof_char_ptr 8

#define tmp_file_prefix "#sql"			/* Prefix for tmp tables */
#define tmp_file_prefix_length 4

struct st_table;
class THD;

/* Struct to handle simple linked lists */

typedef struct st_sql_list {
  uint elements;
  byte *first;
  byte **next;
} SQL_LIST;


uint nr_of_decimals(const char *str);		/* Neaded by sql_string.h */

extern pthread_key(THD*, THR_THD);
inline THD *_current_thd(void)
{
  return my_pthread_getspecific_ptr(THD*,THR_THD);
}
#define current_thd _current_thd()

#include "sql_string.h"
#include "sql_list.h"
#include "sql_map.h"
#include "handler.h"
#include "table.h"
#include "field.h"				/* Field definitions */
#include "sql_udf.h"
#include "item.h"
#include "sql_class.h"
#include "opt_range.h"


void mysql_create_db(THD *thd, char *db, uint create_info);
void mysql_binlog_send(THD* thd, char* log_ident, ulong pos, ushort flags);
int mysql_rm_table(THD *thd,TABLE_LIST *tables, my_bool if_exists);
int quick_rm_table(enum db_type base,const char *db,
		   const char *table_name);
bool mysql_rename_tables(THD *thd, TABLE_LIST *table_list);
bool mysql_change_db(THD *thd,const char *name);
void mysql_parse(THD *thd,char *inBuf,uint length);
void mysql_init_select(LEX *lex);
void init_max_user_conn(void);
void free_max_user_conn(void);
pthread_handler_decl(handle_one_connection,arg);
pthread_handler_decl(handle_bootstrap,arg);
sig_handler end_thread_signal(int sig);
void end_thread(THD *thd,bool put_in_cache);
void flush_thread_cache();
void mysql_execute_command(void);
bool do_command(THD *thd);
bool check_stack_overrun(THD *thd,char *dummy);
bool reload_acl_and_cache(THD *thd, uint options, TABLE_LIST *tables);
void mysql_rm_db(THD *thd,char *db,bool if_exists);
void table_cache_init(void);
void table_cache_free(void);
uint cached_tables(void);
void kill_mysql(void);
void close_connection(NET *net,uint errcode=0,bool lock=1);
bool check_access(THD *thd,uint access,const char *db=0,uint *save_priv=0,
		  bool no_grant=0);
bool check_process_priv(THD *thd=0);

int generate_table(THD *thd, TABLE_LIST *table_list,
		   TABLE *locked_table);


int mysql_backup_table(THD* thd, TABLE_LIST* table_list);
int mysql_restore_table(THD* thd, TABLE_LIST* table_list);

int mysql_check_table(THD* thd, TABLE_LIST* table_list,
		      HA_CHECK_OPT* check_opt);
int mysql_repair_table(THD* thd, TABLE_LIST* table_list,
		       HA_CHECK_OPT* check_opt);
int mysql_analyze_table(THD* thd, TABLE_LIST* table_list,
			HA_CHECK_OPT* check_opt);
int mysql_optimize_table(THD* thd, TABLE_LIST* table_list,
			 HA_CHECK_OPT* check_opt);

/* net_pkg.c */
void send_error(NET *net,uint sql_errno=0, const char *err=0);
void send_warning(NET *net, uint sql_errno, const char *err=0);
void net_printf(NET *net,uint sql_errno, ...);
void send_ok(NET *net,ha_rows affected_rows=0L,ulonglong id=0L,
	     const char *info=0);
void send_eof(NET *net,bool no_flush=0);
char *net_store_length(char *packet,ulonglong length);
char *net_store_length(char *packet,uint length);
char *net_store_data(char *to,const char *from);
char *net_store_data(char *to,int32 from);
char *net_store_data(char *to,longlong from);

bool net_store_null(String *packet);
bool net_store_data(String *packet,uint32 from);
bool net_store_data(String *packet,longlong from);
bool net_store_data(String *packet,const char *from);
bool net_store_data(String *packet,const char *from,uint length);
bool net_store_data(String *packet,struct tm *tmp);
bool net_store_data(String* packet, I_List<i_string>* str_list);
bool net_store_data(String *packet,CONVERT *convert, const char *from,
		    uint length);
bool net_store_data(String *packet, CONVERT *convert, const char *from);

SORT_FIELD * make_unireg_sortorder(ORDER *order, uint *length);
int setup_order(THD *thd,TABLE_LIST *tables, List<Item> &fields,
                List <Item> &all_fields, ORDER *order);

int mysql_select(THD *thd,TABLE_LIST *tables,List<Item> &list,COND *conds,
                 List<Item_func_match> &ftfuncs,
		 ORDER *order, ORDER *group,Item *having,ORDER *proc_param,
		 uint select_type,select_result *result);
Field *create_tmp_field(TABLE *table,Item *item, Item::Type type,
			Item_result_field ***copy_func, Field **from_field,
			bool group,bool modify_item);
int mysql_create_table(THD *thd,const char *db, const char *table_name,
		       HA_CREATE_INFO *create_info,
		       List<create_field> &fields, List<Key> &keys,
		       bool tmp_table, bool no_log);
// no_log is needed for the case of CREATE TABLE ... SELECT , as the logging
// will be done later in sql_insert.cc

TABLE *create_table_from_items(THD *thd, HA_CREATE_INFO *create_info,
			       const char *db, const char *name,
			       List<create_field> *extra_fields,
			       List<Key> *keys,
			       List<Item> *items,
			       MYSQL_LOCK **lock);
int mysql_alter_table(THD *thd, char *new_db, char *new_name,
		      HA_CREATE_INFO *create_info,
		      TABLE_LIST *table_list,
		      List<create_field> &fields,
		      List<Key> &keys,List<Alter_drop> &drop_list,
		      List<Alter_column> &alter_list,
                      ORDER *order,
		      bool drop_primary,
		      enum enum_duplicates handle_duplicates);
bool mysql_rename_table(enum db_type base,
			const char *old_db,
			const char * old_name,
			const char *new_db,
			const char * new_name);
bool close_cached_table(THD *thd,TABLE *table);
int mysql_create_index(THD *thd, TABLE_LIST *table_list, List<Key> &keys);
int mysql_drop_index(THD *thd, TABLE_LIST *table_list,
		     List<Alter_drop> &drop_list);
int mysql_update(THD *thd,TABLE_LIST *tables,List<Item> &fields,
		 List<Item> &values,COND *conds, ha_rows limit,
		 enum enum_duplicates handle_duplicates,
		 thr_lock_type lock_type);
int mysql_insert(THD *thd,TABLE_LIST *table,List<Item> &fields,
		 List<List_item> &values, enum_duplicates flag,
		 thr_lock_type lock_type);
void kill_delayed_threads(void);
int mysql_delete(THD *thd,TABLE_LIST *table,COND *conds,ha_rows rows,
		 thr_lock_type lock_type, ulong options);
TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type update);
TABLE *open_table(THD *thd,const char *db,const char *table,const char *alias,
		  bool *refresh);
TABLE *reopen_name_locked_table(THD* thd, TABLE_LIST* table);
TABLE *find_locked_table(THD *thd, const char *db,const char *table_name);
bool reopen_table(TABLE *table,bool locked=0);
bool reopen_tables(THD *thd,bool get_locks,bool in_refresh);
void close_old_data_files(THD *thd, TABLE *table, bool abort_locks,
			  bool send_refresh);
bool close_data_tables(THD *thd,const char *db, const char *table_name);
bool wait_for_tables(THD *thd);
bool table_is_used(TABLE *table, bool wait_for_name_lock);
bool drop_locked_tables(THD *thd,const char *db, const char *table_name);
void abort_locked_tables(THD *thd,const char *db, const char *table_name);
Field *find_field_in_tables(THD *thd,Item_field *item,TABLE_LIST *tables);
Field *find_field_in_table(THD *thd,TABLE *table,const char *name,uint length,
			   bool check_grant,bool allow_rowid);

/* sql_list.c */
int mysqld_show_dbs(THD *thd,const char *wild);
int mysqld_show_open_tables(THD *thd,const char *db,const char *wild);
int mysqld_show_tables(THD *thd,const char *db,const char *wild);
int mysqld_extend_show_tables(THD *thd,const char *db,const char *wild);
int mysqld_show_fields(THD *thd,TABLE_LIST *table, const char *wild,
		       bool verbose);
int mysqld_show_keys(THD *thd, TABLE_LIST *table);
int mysqld_show_logs(THD *thd);
void mysqld_list_fields(THD *thd,TABLE_LIST *table, const char *wild);
int mysqld_dump_create_info(THD *thd, TABLE *table, int fd = -1);
int mysqld_show_create(THD *thd, TABLE_LIST *table_list);

void mysqld_list_processes(THD *thd,const char *user,bool verbose);
int mysqld_show_status(THD *thd);
int mysqld_show_variables(THD *thd,const char *wild);
int mysqld_show(THD *thd, const char *wild, show_var_st *variables);

/* sql_base.cc */
void set_item_name(Item *item,char *pos,uint length);
bool add_field_to_list(char *field_name, enum enum_field_types type,
		       char *length, char *decimal,
		       uint type_modifier, Item *default_value,char *change,
		       TYPELIB *interval);
void store_position_for_column(const char *name);
bool add_to_list(SQL_LIST &list,Item *group,bool asc=0);
TABLE_LIST *add_table_to_list(Table_ident *table,LEX_STRING *alias,
			      bool updating,
			      thr_lock_type flags=TL_UNLOCK,
			      List<String> *use_index=0,
			      List<String> *ignore_index=0);
void add_join_on(TABLE_LIST *b,Item *expr);
void add_join_natural(TABLE_LIST *a,TABLE_LIST *b);
bool add_proc_to_list(Item *item);
TABLE *unlink_open_table(THD *thd,TABLE *list,TABLE *find);

SQL_SELECT *make_select(TABLE *head, table_map const_tables,
			table_map read_tables, COND *conds, int *error);
Item ** find_item_in_list(Item *item,List<Item> &items);
bool setup_tables(TABLE_LIST *tables);
int setup_fields(THD *thd,TABLE_LIST *tables,List<Item> &item,
		 bool set_query_id,List<Item> *sum_func_list);
int setup_conds(THD *thd,TABLE_LIST *tables,COND **conds);
int setup_ftfuncs(THD *thd,TABLE_LIST *tables, List<Item_func_match> &ftfuncs);
void wait_for_refresh(THD *thd);
int open_tables(THD *thd,TABLE_LIST *tables);
int open_and_lock_tables(THD *thd,TABLE_LIST *tables);
int lock_tables(THD *thd,TABLE_LIST *tables);
TABLE *open_temporary_table(THD *thd, const char *path, const char *db,
			    const char *table_name, bool link_in_list);
bool rm_temporary_table(enum db_type base, char *path);
bool send_fields(THD *thd,List<Item> &item,uint send_field_count);
void free_io_cache(TABLE *entry);
void intern_close_table(TABLE *entry);
void close_thread_tables(THD *thd,bool locked=0);
void close_temporary_tables(THD *thd);
TABLE **find_temporary_table(THD *thd, const char *db, const char *table_name);
bool close_temporary_table(THD *thd, const char *db, const char *table_name);
void close_temporary(TABLE *table, bool delete_table=1);
bool rename_temporary_table(THD* thd, TABLE *table, const char *new_db,
			    const char *table_name);
void remove_db_from_cache(const my_string db);
void flush_tables();
bool remove_table_from_cache(THD *thd, const char *db, const char *table,
			     bool return_if_owned_by_thd=0);
bool close_cached_tables(THD *thd, bool wait_for_refresh, TABLE_LIST *tables);
void copy_field_from_tmp_record(Field *field,int offset);
int fill_record(List<Item> &fields,List<Item> &values);
int fill_record(Field **field,List<Item> &values);
int list_open_tables(THD *thd,List<char> *files, const char *db,const char *wild);
char* query_table_status(THD *thd,const char *db,const char *table_name);

/* sql_calc.cc */
bool eval_const_cond(COND *cond);

/* sql_load.cc */
int mysql_load(THD *thd,sql_exchange *ex, TABLE_LIST *table_list,
	       List<Item> &fields, enum enum_duplicates handle_duplicates,
	       bool local_file,thr_lock_type lock_type);
int write_record(TABLE *table,COPY_INFO *info);

/* sql_manager.cc */
/* bits set in manager_status */
#define MANAGER_BERKELEY_LOG_CLEANUP    (1L << 0)
extern ulong volatile manager_status;
extern bool volatile manager_thread_in_use;
extern pthread_t manager_thread;
extern pthread_mutex_t LOCK_manager;
extern pthread_cond_t COND_manager;
pthread_handler_decl(handle_manager, arg);

/* sql_test.cc */
#ifndef DBUG_OFF
void print_where(COND *cond,const char *info);
void print_cached_tables(void);
void TEST_filesort(TABLE **form,SORT_FIELD *sortorder,uint s_length,
		   ha_rows special);
#endif
void mysql_print_status(THD *thd);
/* key.cc */
int find_ref_key(TABLE *form,Field *field, uint *offset);
void key_copy(byte *key,TABLE *form,uint index,uint key_length);
void key_restore(TABLE *form,byte *key,uint index,uint key_length);
int key_cmp(TABLE *form,const byte *key,uint index,uint key_length);
void key_unpack(String *to,TABLE *form,uint index);
bool check_if_key_used(TABLE *table, uint idx, List<Item> &fields);
void init_errmessage(void);

void sql_perror(const char *message);
void sql_print_error(const char *format,...)
	        __attribute__ ((format (printf, 1, 2)));

extern uint32 server_id;
extern char mysql_data_home[2],server_version[SERVER_VERSION_LENGTH],
	    max_sort_char, mysql_real_data_home[];
extern my_string mysql_unix_port,mysql_tmpdir;
extern const char *first_keyword, *localhost, *delayed_user;
extern ulong refresh_version,flush_version, thread_id,query_id,opened_tables,
	     created_tmp_tables, created_tmp_disk_tables,
	     aborted_threads,aborted_connects,
	     delayed_insert_timeout,
	     delayed_insert_limit, delayed_queue_size,
	     delayed_insert_threads, delayed_insert_writes,
	     delayed_rows_in_use,delayed_insert_errors;
extern ulong filesort_rows, filesort_range_count, filesort_scan_count;
extern ulong filesort_merge_passes;
extern ulong select_range_check_count, select_range_count, select_scan_count;
extern ulong select_full_range_join_count,select_full_join_count,
			 slave_open_temp_tables;
extern uint test_flags,select_errors,mysql_port,ha_open_options;
extern ulong thd_startup_options, slow_launch_threads, slow_launch_time;
extern time_t start_time;
extern const char *command_name[];
extern I_List<THD> threads;
extern MYSQL_LOG mysql_log,mysql_update_log,mysql_slow_log,mysql_bin_log;
extern FILE *bootstrap_file;
extern pthread_key(MEM_ROOT*,THR_MALLOC);
extern pthread_key(NET*, THR_NET);
extern pthread_mutex_t LOCK_mysql_create_db,LOCK_Acl,LOCK_open,
       LOCK_thread_count,LOCK_mapped_file,LOCK_user_locks, LOCK_status,
       LOCK_grant, LOCK_error_log, LOCK_delayed_insert,
       LOCK_delayed_status, LOCK_delayed_create, LOCK_crypt, LOCK_timezone,
       LOCK_binlog_update, LOCK_slave, LOCK_server_id;
extern pthread_cond_t COND_refresh,COND_thread_count, COND_binlog_update,
                      COND_slave_stopped, COND_slave_start;
extern pthread_attr_t connection_attrib;
extern bool opt_endinfo, using_udf_functions, locked_in_memory,
            opt_using_transactions, use_temp_pool;
extern char f_fyllchar;
extern ulong ha_read_count, ha_write_count, ha_delete_count, ha_update_count,
	     ha_read_key_count, ha_read_next_count, ha_read_prev_count,
	     ha_read_first_count, ha_read_last_count,
  	     ha_read_rnd_count, ha_read_rnd_next_count;
extern MY_BITMAP temp_pool;
extern uchar *days_in_month;
extern DATE_FORMAT dayord;
extern double log_10[32];
extern uint protocol_version,dropping_tables;
extern ulong keybuff_size,sortbuff_size,max_item_sort_length,table_cache_size,
	     max_join_size,join_buff_size,tmp_table_size,
	     max_connections,max_connect_errors,long_query_time,
	     max_insert_delayed_threads, max_user_connections,
	     long_query_count,net_wait_timeout,net_interactive_timeout,
	     net_read_timeout,net_write_timeout,
	     what_to_log,flush_time,
	     max_tmp_tables,max_heap_table_size,query_buff_size,
	     lower_case_table_names,thread_stack,thread_stack_min,
	     binlog_cache_size, max_binlog_cache_size;
extern ulong specialflag, current_pid;
extern bool low_priority_updates;
extern bool opt_sql_bin_update, opt_safe_show_db;
extern char language[LIBLEN],reg_ext[FN_EXTLEN],blob_newline;
extern const char **errmesg;			/* Error messages */
extern const char *default_tx_isolation_name;
extern String empty_string;
extern struct show_var_st init_vars[];
extern struct show_var_st status_vars[];
extern enum db_type default_table_type;
extern enum enum_tx_isolation default_tx_isolation;

#ifndef __WIN__
extern pthread_t signal_thread;
#endif

extern bool volatile abort_loop, shutdown_in_progress, grant_option;
extern uint volatile thread_count, thread_running, global_read_lock;

MYSQL_LOCK *mysql_lock_tables(THD *thd,TABLE **table,uint count);
void mysql_unlock_tables(THD *thd, MYSQL_LOCK *sql_lock);
void mysql_unlock_read_tables(THD *thd, MYSQL_LOCK *sql_lock);
void mysql_unlock_some_tables(THD *thd, TABLE **table,uint count);
void mysql_lock_remove(THD *thd, MYSQL_LOCK *locked,TABLE *table);
void mysql_lock_abort(THD *thd, TABLE *table);
MYSQL_LOCK *mysql_lock_merge(MYSQL_LOCK *a,MYSQL_LOCK *b);

/* Lock based on name */
int lock_table_name(THD *thd, TABLE_LIST *table_list);
void unlock_table_name(THD *thd, TABLE_LIST *table_list);
bool wait_for_locked_table_names(THD *thd, TABLE_LIST *table_list);


/* old unireg functions */

void unireg_init(ulong options);
void unireg_end(int signal);
int rea_create_table(my_string file_name,HA_CREATE_INFO *create_info,
		     List<create_field> &create_field,
		     uint key_count,KEY *key_info);
int format_number(uint inputflag,uint max_length,my_string pos,uint length,
		  my_string *errpos);
int openfrm(const char *name,const char *alias,uint filestat,uint prgflag,
	    uint ha_open_flags, TABLE *outparam);
int closefrm(TABLE *table);
db_type get_table_type(const char *name);
int read_string(File file, gptr *to, uint length);
void free_blobs(TABLE *table);
int set_zone(int nr,int min_zone,int max_zone);
ulong convert_period_to_month(ulong period);
ulong convert_month_to_period(ulong month);
long calc_daynr(uint year,uint month,uint day);
uint calc_days_in_year(uint year);
void get_date_from_daynr(long daynr,uint *year, uint *month,
			 uint *day);
void init_time(void);
long my_gmt_sec(TIME *);
time_t str_to_timestamp(const char *str,uint length);
bool str_to_time(const char *str,uint length,TIME *l_time);
longlong str_to_datetime(const char *str,uint length,bool fuzzy_date);
timestamp_type str_to_TIME(const char *str, uint length, TIME *l_time,
			   bool fuzzy_date);

int test_if_number(char *str,int *res,bool allow_wildcards);
void change_byte(byte *,uint,char,char);
void unireg_abort(int exit_code);
void init_read_record(READ_RECORD *info, THD *thd, TABLE *reg_form,
		      SQL_SELECT *select,
		      int use_record_cache, bool print_errors);
void end_read_record(READ_RECORD *info);
ha_rows filesort(TABLE **form,struct st_sort_field *sortorder, uint s_length,
		 SQL_SELECT *select, ha_rows special,ha_rows max_rows);
void change_double_for_sort(double nr,byte *to);
int get_quick_record(SQL_SELECT *select);
int calc_weekday(long daynr,bool sunday_first_day_of_week);
uint calc_week(TIME *ltime, bool with_year, bool sunday_first_day_of_week,
	       uint *year);
void find_date(char *pos,uint *vek,uint flag);
TYPELIB *convert_strings_to_array_type(my_string *typelibs, my_string *end);
TYPELIB *typelib(List<String> &strings);
void clean_up(void);
ulong get_form_pos(File file, uchar *head, TYPELIB *save_names);
ulong make_new_entry(File file,uchar *fileinfo,TYPELIB *formnames,
		     const char *newname);
ulong next_io_size(ulong pos);
void append_unescaped(String *res,const char *pos);
int create_frm(char *name,uint reclength,uchar *fileinfo,
	       HA_CREATE_INFO *create_info, uint keys);
void update_create_info_from_table(HA_CREATE_INFO *info, TABLE *form);
int rename_file_ext(const char * from,const char * to,const char * ext);
bool check_db_name(const char *db);
bool check_column_name(const char *name);
bool check_table_name(const char *name, uint length);
char *get_field(MEM_ROOT *mem,TABLE *table,uint fieldnr);
int wild_case_compare(const char *str,const char *wildstr);
int wild_compare(const char *str,const char *str_end,
		 const char *wildstr,const char *wildend,char escape);
int wild_case_compare(const char *str,const char *str_end,
		 const char *wildstr,const char *wildend,char escape);

/* from hostname.cc */
struct in_addr;
my_string ip_to_hostname(struct in_addr *in,uint *errors);
void inc_host_errors(struct in_addr *in);
void reset_host_errors(struct in_addr *in);
bool hostname_cache_init();
void hostname_cache_free();
void hostname_cache_refresh(void);
bool get_interval_info(const char *str,uint length,uint count,
		       long *values);
/* sql_cache */

extern bool sql_cache_init();
extern void sql_cache_free();
extern int sql_cache_hit(THD *thd, char *inBuf, uint length);

/* Some inline functions for more speed */

inline bool add_item_to_list(Item *item)
{
  return current_lex->item_list.push_back(item);
}
inline bool add_value_to_list(Item *value)
{
  return current_lex->value_list.push_back(value);
}
inline bool add_order_to_list(Item *item,bool asc)
{
  return add_to_list(current_lex->order_list,item,asc);
}
inline bool add_group_to_list(Item *item,bool asc)
{
  return add_to_list(current_lex->group_list,item,asc);
}
inline void mark_as_null_row(TABLE *table)
{
  table->null_row=1;
  table->status|=STATUS_NULL_ROW;
  bfill(table->null_flags,table->null_bytes,255);
}

#endif
