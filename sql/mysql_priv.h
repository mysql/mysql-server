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

/*
  Mostly this file is used in the server. But a little part of it is used in
  mysqlbinlog too (definition of SELECT_DISTINCT and others).
  The consequence is that 90% of the file is wrapped in #ifndef MYSQL_CLIENT,
  except the part which must be in the server and in the client.
*/

#ifndef MYSQL_CLIENT

#include <my_global.h>
#include <mysql_version.h>
#include <mysql_embed.h>
#include <my_sys.h>
#include <my_time.h>
#include <m_string.h>
#include <hash.h>
#include <signal.h>
#include <thr_lock.h>
#include <my_base.h>			/* Needed by field.h */
#include "sql_bitmap.h"

#ifdef __EMX__
#undef write  /* remove pthread.h macro definition for EMX */
#endif

/* TODO convert all these three maps to Bitmap classes */
typedef ulonglong table_map;          /* Used for table bits in join */
typedef Bitmap<64> key_map;           /* Used for finding keys */
typedef ulong key_part_map;           /* Used for finding key parts */

/* useful constants */
extern const key_map key_map_empty;
extern const key_map key_map_full;
extern const char *primary_key_name;

#include "mysql_com.h"
#include <violite.h>
#include "unireg.h"

void init_sql_alloc(MEM_ROOT *root, uint block_size, uint pre_alloc_size);
gptr sql_alloc(unsigned size);
gptr sql_calloc(unsigned size);
char *sql_strdup(const char *str);
char *sql_strmake(const char *str,uint len);
gptr sql_memdup(const void * ptr,unsigned size);
void sql_element_free(void *ptr);
char *sql_strmake_with_convert(const char *str, uint32 arg_length,
			       CHARSET_INFO *from_cs,
			       uint32 max_res_length,
			       CHARSET_INFO *to_cs, uint32 *result_length);
void kill_one_thread(THD *thd, ulong id, bool only_kill_query);
bool net_request_file(NET* net, const char* fname);
char* query_table_status(THD *thd,const char *db,const char *table_name);

#define x_free(A)	{ my_free((gptr) (A),MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR)); }
#define safeFree(x)	{ if(x) { my_free((gptr) x,MYF(0)); x = NULL; } }
#define PREV_BITS(type,A)	((type) (((type) 1 << (A)) -1))
#define all_bits_set(A,B) ((A) & (B) != (B))

extern CHARSET_INFO *system_charset_info, *files_charset_info ;
extern CHARSET_INFO *national_charset_info, *table_alias_charset;

/***************************************************************************
  Configuration parameters
****************************************************************************/

#define ACL_CACHE_SIZE		256
#define MAX_PASSWORD_LENGTH	32
#define HOST_CACHE_SIZE		128
#define MAX_ACCEPT_RETRY	10	// Test accept this many times
#define MAX_FIELDS_BEFORE_HASH	32
#define USER_VARS_HASH_SIZE     16
#define STACK_MIN_SIZE		8192	// Abort if less stack during eval.
#define STACK_BUFF_ALLOC	64	// For stack overrun checks
#ifndef MYSQLD_NET_RETRY_COUNT
#define MYSQLD_NET_RETRY_COUNT  10	// Abort read after this many int.
#endif
#define TEMP_POOL_SIZE          128

#define QUERY_ALLOC_BLOCK_SIZE		8192
#define QUERY_ALLOC_PREALLOC_SIZE   	8192
#define TRANS_ALLOC_BLOCK_SIZE		4096
#define TRANS_ALLOC_PREALLOC_SIZE	4096
#define RANGE_ALLOC_BLOCK_SIZE		2048
#define ACL_ALLOC_BLOCK_SIZE		1024
#define UDF_ALLOC_BLOCK_SIZE		1024
#define TABLE_ALLOC_BLOCK_SIZE		1024
#define BDB_LOG_ALLOC_BLOCK_SIZE	1024
#define WARN_ALLOC_BLOCK_SIZE		2048
#define WARN_ALLOC_PREALLOC_SIZE	1024

/*
  The following parameters is to decide when to use an extra cache to
  optimise seeks when reading a big table in sorted order
*/
#define MIN_FILE_LENGTH_TO_USE_ROW_CACHE (16L*1024*1024)
#define MIN_ROWS_TO_USE_TABLE_CACHE	 100
#define MIN_ROWS_TO_USE_BULK_INSERT	 100

/*
  The following is used to decide if MySQL should use table scanning
  instead of reading with keys.  The number says how many evaluation of the
  WHERE clause is comparable to reading one extra row from a table.
*/
#define TIME_FOR_COMPARE   5	// 5 compares == one read

/*
  Number of comparisons of table rowids equivalent to reading one row from a 
  table.
*/
#define TIME_FOR_COMPARE_ROWID  (TIME_FOR_COMPARE*2)

/*
  For sequential disk seeks the cost formula is:
    DISK_SEEK_BASE_COST + DISK_SEEK_PROP_COST * #blocks_to_skip  
  
  The cost of average seek 
    DISK_SEEK_BASE_COST + DISK_SEEK_PROP_COST*BLOCKS_IN_AVG_SEEK =1.0.
*/
#define DISK_SEEK_BASE_COST ((double)0.5)

#define BLOCKS_IN_AVG_SEEK  128

#define DISK_SEEK_PROP_COST ((double)0.5/BLOCKS_IN_AVG_SEEK)


/*
  Number of rows in a reference table when refereed through a not unique key.
  This value is only used when we don't know anything about the key
  distribution.
*/
#define MATCHING_ROWS_IN_OTHER_TABLE 10

/* Don't pack string keys shorter than this (if PACK_KEYS=1 isn't used) */
#define KEY_DEFAULT_PACK_LENGTH 8

/* Characters shown for the command in 'show processlist' */
#define PROCESS_LIST_WIDTH 100

/* Time handling defaults */
#define TIMESTAMP_MAX_YEAR 2038
#define YY_PART_YEAR	   70
#define TIMESTAMP_MIN_YEAR (1900 + YY_PART_YEAR - 1)
#define TIMESTAMP_MAX_VALUE 2145916799
#define TIMESTAMP_MIN_VALUE 1
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

#if defined(__WIN__) || defined(OS2)
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
#define TEST_SIGINT		1024	/* Allow sigint on threads */
#define TEST_SYNCHRONIZATION	2048	/* get server to do sleep in some 
                                                                       places */
#endif

/* 
   This is included in the server and in the client.
   Options for select set by the yacc parser (stored in lex->options).
   None of the 32 defines below should have its value changed, or this will
   break replication.
*/

#define SELECT_DISTINCT		(1L << 0)
#define SELECT_STRAIGHT_JOIN	(1L << 1)
#define SELECT_DESCRIBE		(1L << 2)
#define SELECT_SMALL_RESULT	(1L << 3)
#define SELECT_BIG_RESULT	(1L << 4)
#define OPTION_FOUND_ROWS	(1L << 5)
#define OPTION_TO_QUERY_CACHE   (1L << 6)
#define SELECT_NO_JOIN_CACHE	(1L << 7)       /* Intern */
#define OPTION_BIG_TABLES       (1L << 8)       /* for SQL OPTION */
#define OPTION_BIG_SELECTS      (1L << 9)       /* for SQL OPTION */
#define OPTION_LOG_OFF          (1L << 10)
#define OPTION_UPDATE_LOG       (1L << 11)      /* update log flag */
#define TMP_TABLE_ALL_COLUMNS   (1L << 12)
#define OPTION_WARNINGS         (1L << 13)
#define OPTION_AUTO_IS_NULL     (1L << 14)
#define OPTION_FOUND_COMMENT    (1L << 15)
#define OPTION_SAFE_UPDATES     (1L << 16)
#define OPTION_BUFFER_RESULT    (1L << 17)
#define OPTION_BIN_LOG          (1L << 18)
#define OPTION_NOT_AUTOCOMMIT   (1L << 19)
#define OPTION_BEGIN            (1L << 20)
#define OPTION_TABLE_LOCK       (1L << 21)
#define OPTION_QUICK            (1L << 22)
#define OPTION_QUOTE_SHOW_CREATE (1L << 23)
#define OPTION_INTERNAL_SUBTRANSACTIONS (1L << 24)

/* Set if we are updating a non-transaction safe table */
#define OPTION_STATUS_NO_TRANS_UPDATE   (1L << 25)

/* The following can be set when importing tables in a 'wrong order'
   to suppress foreign key checks */
#define OPTION_NO_FOREIGN_KEY_CHECKS    (1L << 26)
/* The following speeds up inserts to InnoDB tables by suppressing unique
   key checks in some cases */
#define OPTION_RELAXED_UNIQUE_CHECKS    (1L << 27)
#define SELECT_NO_UNLOCK                (1L << 28)
#define OPTION_SCHEMA_TABLE             (1L << 29)

/* The rest of the file is included in the server only */
#ifndef MYSQL_CLIENT

/* Bits for different SQL modes modes (including ANSI mode) */
#define MODE_REAL_AS_FLOAT      	1
#define MODE_PIPES_AS_CONCAT    	2
#define MODE_ANSI_QUOTES        	4
#define MODE_IGNORE_SPACE		8
#define MODE_NOT_USED			16
#define MODE_ONLY_FULL_GROUP_BY		32
#define MODE_NO_UNSIGNED_SUBTRACTION	64
#define MODE_NO_DIR_IN_CREATE		128
#define MODE_POSTGRESQL			256
#define MODE_ORACLE			512
#define MODE_MSSQL			1024
#define MODE_DB2			2048
#define MODE_MAXDB			4096
#define MODE_NO_KEY_OPTIONS             8192
#define MODE_NO_TABLE_OPTIONS           16384 
#define MODE_NO_FIELD_OPTIONS           32768
#define MODE_MYSQL323                   65536
#define MODE_MYSQL40                    (MODE_MYSQL323*2)
#define MODE_ANSI	                (MODE_MYSQL40*2)
#define MODE_NO_AUTO_VALUE_ON_ZERO      (MODE_ANSI*2)
#define MODE_NO_BACKSLASH_ESCAPES       (MODE_NO_AUTO_VALUE_ON_ZERO*2)
#define MODE_STRICT_TRANS_TABLES	(MODE_NO_BACKSLASH_ESCAPES*2)
#define MODE_STRICT_ALL_TABLES		(MODE_STRICT_TRANS_TABLES*2)
#define MODE_NO_ZERO_IN_DATE		(MODE_STRICT_ALL_TABLES*2)
#define MODE_NO_ZERO_DATE		(MODE_NO_ZERO_IN_DATE*2)
#define MODE_INVALID_DATES		(MODE_NO_ZERO_DATE*2)
#define MODE_ERROR_FOR_DIVISION_BY_ZERO (MODE_INVALID_DATES*2)
#define MODE_TRADITIONAL		(MODE_ERROR_FOR_DIVISION_BY_ZERO*2)
#define MODE_NO_AUTO_CREATE_USER	(MODE_TRADITIONAL*2)

#define RAID_BLOCK_SIZE 1024

#define MY_CHARSET_BIN_MB_MAXLEN 1

// uncachable cause
#define UNCACHEABLE_DEPENDENT   1
#define UNCACHEABLE_RAND        2
#define UNCACHEABLE_SIDEEFFECT	4
// forcing to save JOIN for explain
#define UNCACHEABLE_EXPLAIN     8

#ifdef EXTRA_DEBUG
/*
  Sync points allow us to force the server to reach a certain line of code
  and block there until the client tells the server it is ok to go on.
  The client tells the server to block with SELECT GET_LOCK()
  and unblocks it with SELECT RELEASE_LOCK(). Used for debugging difficult
  concurrency problems
*/
#define DBUG_SYNC_POINT(lock_name,lock_timeout) \
 debug_sync_point(lock_name,lock_timeout)
void debug_sync_point(const char* lock_name, uint lock_timeout);
#else
#define DBUG_SYNC_POINT(lock_name,lock_timeout)
#endif /* EXTRA_DEBUG */

/* BINLOG_DUMP options */

#define BINLOG_DUMP_NON_BLOCK   1

/* sql_show.cc:show_log_files() */
#define SHOW_LOG_STATUS_FREE "FREE"
#define SHOW_LOG_STATUS_INUSE "IN USE"

/* Options to add_table_to_list() */
#define TL_OPTION_UPDATING	1
#define TL_OPTION_FORCE_INDEX	2
#define TL_OPTION_IGNORE_LEAVES 4

/* Some portable defines */

#define portable_sizeof_char_ptr 8

#define tmp_file_prefix "#sql"			/* Prefix for tmp tables */
#define tmp_file_prefix_length 4

/* Flags for calc_week() function.  */
#define WEEK_MONDAY_FIRST    1
#define WEEK_YEAR            2
#define WEEK_FIRST_WEEKDAY   4

enum enum_parsing_place
{
  NO_MATTER,
  IN_HAVING,
  SELECT_LIST,
  IN_WHERE
};

struct st_table;
class THD;
class Item_arena;

/* Struct to handle simple linked lists */

typedef struct st_sql_list {
  uint elements;
  byte *first;
  byte **next;

  inline void empty()
  {
    elements=0;
    first=0;
    next= &first;
  }
  inline void link_in_list(byte *element,byte **next_ptr)
  {
    elements++;
    (*next)=element;
    next= next_ptr;
    *next=0;
  }
  inline void save_and_clear(struct st_sql_list *save)
  {
    *save= *this;
    empty();
  }
  inline void push_front(struct st_sql_list *save)
  {
    *save->next= first;				/* link current list last */
    first= save->first;
    elements+= save->elements;
  }
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
#include "parse_file.h"
#include "table.h"
#include "field.h"				/* Field definitions */
#include "protocol.h"
#include "sql_udf.h"
class user_var_entry;
#include "item.h"
#include "tztime.h"
typedef Comp_creator* (*chooser_compare_func_creator)(bool invert);
/* sql_parse.cc */
void free_items(Item *item);
void cleanup_items(Item *item);
class THD;
void close_thread_tables(THD *thd, bool locked=0, bool skip_derived=0,
                         TABLE *stopper= 0);
bool check_one_table_access(THD *thd, ulong privilege,
			   TABLE_LIST *tables);
bool check_some_access(THD *thd, ulong want_access, TABLE_LIST *table);
bool check_merge_table_access(THD *thd, char *db,
			      TABLE_LIST *table_list);
int multi_update_precheck(THD *thd, TABLE_LIST *tables);
int multi_delete_precheck(THD *thd, TABLE_LIST *tables, uint *table_count);
int mysql_multi_update_prepare(THD *thd);
int mysql_multi_delete_prepare(THD *thd);
int mysql_insert_select_prepare(THD *thd);
int insert_select_precheck(THD *thd, TABLE_LIST *tables);
int update_precheck(THD *thd, TABLE_LIST *tables);
int delete_precheck(THD *thd, TABLE_LIST *tables);
int insert_precheck(THD *thd, TABLE_LIST *tables);
int create_table_precheck(THD *thd, TABLE_LIST *tables,
			  TABLE_LIST *create_table);
Item *negate_expression(THD *thd, Item *expr);
#include "sql_class.h"
#include "opt_range.h"

#ifdef HAVE_QUERY_CACHE
struct Query_cache_query_flags
{
  unsigned int client_long_flag:1;
  uint character_set_client_num;
  uint character_set_results_num;
  uint collation_connection_num;
  ha_rows limit;
  Time_zone *time_zone;
  ulong sql_mode;
  ulong max_sort_length;
  ulong group_concat_max_len;
};
#define QUERY_CACHE_FLAGS_SIZE sizeof(Query_cache_query_flags)
#include "sql_cache.h"
#define query_cache_store_query(A, B) query_cache.store_query(A, B)
#define query_cache_destroy() query_cache.destroy()
#define query_cache_result_size_limit(A) query_cache.result_size_limit(A)
#define query_cache_resize(A) query_cache.resize(A)
#define query_cache_set_min_res_unit(A) query_cache.set_min_res_unit(A)
#define query_cache_invalidate3(A, B, C) query_cache.invalidate(A, B, C)
#define query_cache_invalidate1(A) query_cache.invalidate(A)
#define query_cache_send_result_to_client(A, B, C) \
  query_cache.send_result_to_client(A, B, C)
#define query_cache_invalidate_by_MyISAM_filename_ref \
  &query_cache_invalidate_by_MyISAM_filename
#else
#define QUERY_CACHE_FLAGS_SIZE 0
#define query_cache_store_query(A, B)
#define query_cache_destroy()
#define query_cache_result_size_limit(A)
#define query_cache_resize(A)
#define query_cache_set_min_res_unit(A)
#define query_cache_invalidate3(A, B, C)
#define query_cache_invalidate1(A)
#define query_cache_send_result_to_client(A, B, C) 0
#define query_cache_invalidate_by_MyISAM_filename_ref NULL

#define query_cache_abort(A)
#define query_cache_end_of_result(A)
#define query_cache_invalidate_by_MyISAM_filename_ref NULL
#endif /*HAVE_QUERY_CACHE*/

#define prepare_execute(A) ((A)->command == COM_EXECUTE)

int mysql_create_db(THD *thd, char *db, HA_CREATE_INFO *create, bool silent);
int mysql_alter_db(THD *thd, const char *db, HA_CREATE_INFO *create);
int mysql_rm_db(THD *thd,char *db,bool if_exists, bool silent);
void mysql_binlog_send(THD* thd, char* log_ident, my_off_t pos, ushort flags);
int mysql_rm_table(THD *thd,TABLE_LIST *tables, my_bool if_exists,
		   my_bool drop_temporary);
int mysql_rm_table_part2(THD *thd, TABLE_LIST *tables, bool if_exists,
			 bool drop_temporary, bool drop_view, bool log_query);
int mysql_rm_table_part2_with_lock(THD *thd, TABLE_LIST *tables,
				   bool if_exists, bool drop_temporary,
				   bool log_query);
int quick_rm_table(enum db_type base,const char *db,
		   const char *table_name);
void close_cached_table(THD *thd, TABLE *table);
bool mysql_rename_tables(THD *thd, TABLE_LIST *table_list);
bool mysql_change_db(THD *thd,const char *name);
void mysql_parse(THD *thd,char *inBuf,uint length);
bool mysql_test_parse_for_slave(THD *thd,char *inBuf,uint length);
bool is_update_query(enum enum_sql_command command);
bool alloc_query(THD *thd, char *packet, ulong packet_length);
void mysql_init_select(LEX *lex);
void mysql_reset_thd_for_next_command(THD *thd);
void mysql_init_query(THD *thd, uchar *buf, uint length);
bool mysql_new_select(LEX *lex, bool move_down);
void create_select_for_variable(const char *var_name);
void mysql_init_multi_delete(LEX *lex);
void init_max_user_conn(void);
void init_update_queries(void);
void free_max_user_conn(void);
extern "C" pthread_handler_decl(handle_one_connection,arg);
extern "C" pthread_handler_decl(handle_bootstrap,arg);
void end_thread(THD *thd,bool put_in_cache);
void flush_thread_cache();
int mysql_execute_command(THD *thd);
bool do_command(THD *thd);
bool dispatch_command(enum enum_server_command command, THD *thd,
		      char* packet, uint packet_length);
bool check_dup(const char *db, const char *name, TABLE_LIST *tables);

bool table_cache_init(void);
void table_cache_free(void);
uint cached_tables(void);
void kill_mysql(void);
void close_connection(THD *thd, uint errcode, bool lock);
bool reload_acl_and_cache(THD *thd, ulong options, TABLE_LIST *tables, 
                          bool *write_to_binlog);
bool check_access(THD *thd, ulong access, const char *db, ulong *save_priv,
		  bool no_grant, bool no_errors);
bool check_table_access(THD *thd, ulong want_access, TABLE_LIST *tables,
			bool no_errors);
bool check_global_access(THD *thd, ulong want_access);

int mysql_backup_table(THD* thd, TABLE_LIST* table_list);
int mysql_restore_table(THD* thd, TABLE_LIST* table_list);

int mysql_checksum_table(THD* thd, TABLE_LIST* table_list,
		      HA_CHECK_OPT* check_opt);
int mysql_check_table(THD* thd, TABLE_LIST* table_list,
		      HA_CHECK_OPT* check_opt);
int mysql_repair_table(THD* thd, TABLE_LIST* table_list,
		       HA_CHECK_OPT* check_opt);
int mysql_analyze_table(THD* thd, TABLE_LIST* table_list,
			HA_CHECK_OPT* check_opt);
int mysql_optimize_table(THD* thd, TABLE_LIST* table_list,
			 HA_CHECK_OPT* check_opt);
int mysql_assign_to_keycache(THD* thd, TABLE_LIST* table_list,
			     LEX_STRING *key_cache_name);
int mysql_preload_keys(THD* thd, TABLE_LIST* table_list);
int reassign_keycache_tables(THD* thd, KEY_CACHE *src_cache,
                             KEY_CACHE *dst_cache);

bool check_simple_select();

SORT_FIELD * make_unireg_sortorder(ORDER *order, uint *length);
int setup_order(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
		List<Item> &fields, List <Item> &all_fields, ORDER *order);
int setup_group(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
		List<Item> &fields, List<Item> &all_fields, ORDER *order,
		bool *hidden_group_fields);

int handle_select(THD *thd, LEX *lex, select_result *result);
int mysql_select(THD *thd, Item ***rref_pointer_array,
		 TABLE_LIST *tables, uint wild_num,  List<Item> &list,
		 COND *conds, uint og_num, ORDER *order, ORDER *group,
		 Item *having, ORDER *proc_param, ulong select_type, 
		 select_result *result, SELECT_LEX_UNIT *unit, 
		 SELECT_LEX *select_lex);
void free_underlaid_joins(THD *thd, SELECT_LEX *select);
int mysql_explain_union(THD *thd, SELECT_LEX_UNIT *unit,
			select_result *result);
int mysql_explain_select(THD *thd, SELECT_LEX *sl, char const *type,
			 select_result *result);
int mysql_union(THD *thd, LEX *lex, select_result *result,
		SELECT_LEX_UNIT *unit);
int mysql_handle_derived(LEX *lex, int (*processor)(THD *thd,
                                                    LEX *lex,
                                                    TABLE_LIST *table));
int mysql_derived_prepare(THD *thd, LEX *lex, TABLE_LIST *t);
int mysql_derived_filling(THD *thd, LEX *lex, TABLE_LIST *t);
Field *create_tmp_field(THD *thd, TABLE *table,Item *item, Item::Type type,
			Item ***copy_func, Field **from_field,
			bool group, bool modify_item, uint convert_blob_length);
int mysql_prepare_table(THD *thd, HA_CREATE_INFO *create_info,
		       List<create_field> &fields,
		       List<Key> &keys, uint &db_options, 
		       handler *file, KEY *&key_info_buffer,
		       uint &key_count, int select_field_count);
int mysql_create_table(THD *thd,const char *db, const char *table_name,
		       HA_CREATE_INFO *create_info,
		       List<create_field> &fields, List<Key> &keys,
		       bool tmp_table, uint select_field_count);

TABLE *create_table_from_items(THD *thd, HA_CREATE_INFO *create_info,
			       TABLE_LIST *create_table,
			       List<create_field> *extra_fields,
			       List<Key> *keys,
			       List<Item> *items,
			       MYSQL_LOCK **lock);
int mysql_alter_table(THD *thd, char *new_db, char *new_name,
		      HA_CREATE_INFO *create_info,
		      TABLE_LIST *table_list,
		      List<create_field> &fields,
		      List<Key> &keys,
		      uint order_num, ORDER *order,
		      enum enum_duplicates handle_duplicates,
		      ALTER_INFO *alter_info, bool do_send_ok=1);
int mysql_recreate_table(THD *thd, TABLE_LIST *table_list, bool do_send_ok);
int mysql_create_like_table(THD *thd, TABLE_LIST *table,
                            HA_CREATE_INFO *create_info,
                            Table_ident *src_table);
bool mysql_rename_table(enum db_type base,
			const char *old_db,
			const char * old_name,
			const char *new_db,
			const char * new_name);
int mysql_create_index(THD *thd, TABLE_LIST *table_list, List<Key> &keys);
int mysql_drop_index(THD *thd, TABLE_LIST *table_list,
		     ALTER_INFO *alter_info);
int mysql_prepare_update(THD *thd, TABLE_LIST *table_list,
			 Item **conds, uint order_num, ORDER *order);
int mysql_update(THD *thd,TABLE_LIST *tables,List<Item> &fields,
		 List<Item> &values,COND *conds,
                 uint order_num, ORDER *order, ha_rows limit,
		 enum enum_duplicates handle_duplicates);
int mysql_multi_update(THD *thd, TABLE_LIST *table_list,
		       List<Item> *fields, List<Item> *values,
		       COND *conds, ulong options,
		       enum enum_duplicates handle_duplicates,
		       SELECT_LEX_UNIT *unit, SELECT_LEX *select_lex);
int mysql_prepare_insert(THD *thd, TABLE_LIST *table_list, TABLE *table,
			 List<Item> &fields, List_item *values,
			 List<Item> &update_fields,
			 List<Item> &update_values, enum_duplicates duplic);
int mysql_insert(THD *thd,TABLE_LIST *table,List<Item> &fields,
		 List<List_item> &values, List<Item> &update_fields,
		 List<Item> &update_values, enum_duplicates flag);
int check_that_all_fields_are_given_values(THD *thd, TABLE *entry);
int mysql_prepare_delete(THD *thd, TABLE_LIST *table_list, Item **conds);
int mysql_delete(THD *thd, TABLE_LIST *table, COND *conds, SQL_LIST *order,
                 ha_rows rows, ulong options);
int mysql_truncate(THD *thd, TABLE_LIST *table_list, bool dont_send_ok);
int mysql_create_or_drop_trigger(THD *thd, TABLE_LIST *tables, bool create);
TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type update);
TABLE *open_table(THD *thd, TABLE_LIST *table_list, MEM_ROOT* mem,
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
void execute_init_command(THD *thd, sys_var_str *init_command_var,
			  rw_lock_t *var_mutex);
extern Field *not_found_field;
extern Field *view_ref_found;

enum find_item_error_report_type {REPORT_ALL_ERRORS, REPORT_EXCEPT_NOT_FOUND,
				  IGNORE_ERRORS, REPORT_EXCEPT_NON_UNIQUE,
                                  IGNORE_EXCEPT_NON_UNIQUE};
Field *find_field_in_tables(THD *thd, Item_ident *item, TABLE_LIST *tables,
			    Item **ref,
                            find_item_error_report_type report_error,
                            bool check_privileges);
Field *
find_field_in_table(THD *thd, TABLE_LIST *table_list,
                    const char *name, const char *item_name,
                    uint length, Item **ref,
                    bool check_grants_table, bool check_grants_view,
                    bool allow_rowid,
                    uint *cached_field_index_ptr);
Field *
find_field_in_real_table(THD *thd, TABLE *table, const char *name,
                         uint length, bool check_grants, bool allow_rowid,
                         uint *cached_field_index_ptr);
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
struct st_des_keyblock
{
  DES_cblock key1, key2, key3;
};
struct st_des_keyschedule
{
  DES_key_schedule ks1, ks2, ks3;
};
extern char *des_key_file;
extern struct st_des_keyschedule des_keyschedule[10];
extern uint des_default_key;
extern pthread_mutex_t LOCK_des_key_file;
bool load_des_key_file(const char *file_name);
void free_des_key_file();
#endif /* HAVE_OPENSSL */

/* sql_do.cc */
int mysql_do(THD *thd, List<Item> &values);

/* sql_show.cc */
int mysqld_show_open_tables(THD *thd,const char *wild);
int mysqld_show_logs(THD *thd);
void append_identifier(THD *thd, String *packet, const char *name,
		       uint length);
int get_quote_char_for_identifier(THD *thd, const char *name, uint length);
void mysqld_list_fields(THD *thd,TABLE_LIST *table, const char *wild);
int mysqld_dump_create_info(THD *thd, TABLE *table, int fd = -1);
int mysqld_show_create(THD *thd, TABLE_LIST *table_list);
int mysqld_show_create_db(THD *thd, char *dbname, HA_CREATE_INFO *create);

void mysqld_list_processes(THD *thd,const char *user,bool verbose);
int mysqld_show_status(THD *thd);
int mysqld_show_variables(THD *thd,const char *wild);
int mysqld_show(THD *thd, const char *wild, show_var_st *variables,
		enum enum_var_type value_type,
		pthread_mutex_t *mutex,
		struct system_status_var *status_var);
int mysql_find_files(THD *thd,List<char> *files, const char *db,
                const char *path, const char *wild, bool dir);
int mysqld_show_storage_engines(THD *thd);
int mysqld_show_privileges(THD *thd);
int mysqld_show_column_types(THD *thd);
int mysqld_help (THD *thd, const char *text);
void calc_sum_of_all_status(STATUS_VAR *to);



/* information schema */
extern LEX_STRING information_schema_name;
LEX_STRING *make_lex_string(THD *thd, LEX_STRING *lex_str,
                            const char* str, uint length,
                            bool allocate_lex_string= 0);
ST_SCHEMA_TABLE *find_schema_table(THD *thd, const char* table_name);
ST_SCHEMA_TABLE *get_schema_table(enum enum_schema_tables schema_table_idx);
int prepare_schema_table(THD *thd, LEX *lex, Table_ident *table_ident,
                         enum enum_schema_tables schema_table_idx);
int make_schema_select(THD *thd,  SELECT_LEX *sel,
                       enum enum_schema_tables schema_table_idx);
int mysql_schema_table(THD *thd, LEX *lex, TABLE_LIST *table_list);
int fill_schema_user_privileges(THD *thd, TABLE_LIST *tables, COND *cond);
int fill_schema_schema_privileges(THD *thd, TABLE_LIST *tables, COND *cond);
int fill_schema_table_privileges(THD *thd, TABLE_LIST *tables, COND *cond);
int fill_schema_column_privileges(THD *thd, TABLE_LIST *tables, COND *cond);
int get_schema_tables_result(JOIN *join);

/* sql_prepare.cc */
int mysql_stmt_prepare(THD *thd, char *packet, uint packet_length, 
                       LEX_STRING *name=NULL);
void mysql_stmt_execute(THD *thd, char *packet, uint packet_length);
void mysql_sql_stmt_execute(THD *thd, LEX_STRING *stmt_name);
void mysql_stmt_fetch(THD *thd, char *packet, uint packet_length);
void mysql_stmt_free(THD *thd, char *packet);
void mysql_stmt_reset(THD *thd, char *packet);
void mysql_stmt_get_longdata(THD *thd, char *pos, ulong packet_length);
void reset_stmt_for_execute(THD *thd, LEX *lex);

/* sql_error.cc */
MYSQL_ERROR *push_warning(THD *thd, MYSQL_ERROR::enum_warning_level level, uint code,
                          const char *msg);
void push_warning_printf(THD *thd, MYSQL_ERROR::enum_warning_level level,
			 uint code, const char *format, ...);
void mysql_reset_errors(THD *thd);
my_bool mysqld_show_warnings(THD *thd, ulong levels_to_show);

/* sql_handler.cc */
int mysql_ha_open(THD *thd, TABLE_LIST *tables, bool reopen= 0);
int mysql_ha_close(THD *thd, TABLE_LIST *tables);
int mysql_ha_read(THD *, TABLE_LIST *,enum enum_ha_read_modes,char *,
               List<Item> *,enum ha_rkey_function,Item *,ha_rows,ha_rows);
int mysql_ha_flush(THD *thd, TABLE_LIST *tables, uint mode_flags);
/* mysql_ha_flush mode_flags bits */
#define MYSQL_HA_CLOSE_FINAL        0x00
#define MYSQL_HA_REOPEN_ON_USAGE    0x01
#define MYSQL_HA_FLUSH_ALL          0x02

/* sql_base.cc */
#define TMP_TABLE_KEY_EXTRA 8
void set_item_name(Item *item,char *pos,uint length);
bool add_field_to_list(THD *thd, char *field_name, enum enum_field_types type,
		       char *length, char *decimal,
		       uint type_modifier,
		       Item *default_value, Item *on_update_value,
		       LEX_STRING *comment,
		       char *change, TYPELIB *interval,CHARSET_INFO *cs,
		       uint uint_geom_type);
void store_position_for_column(const char *name);
bool add_to_list(THD *thd, SQL_LIST &list,Item *group,bool asc=0);
void add_join_on(TABLE_LIST *b,Item *expr);
void add_join_natural(TABLE_LIST *a,TABLE_LIST *b);
bool add_proc_to_list(THD *thd, Item *item);
TABLE *unlink_open_table(THD *thd,TABLE *list,TABLE *find);

SQL_SELECT *make_select(TABLE *head, table_map const_tables,
			table_map read_tables, COND *conds, int *error,
                        bool allow_null_cond= false);
extern Item **not_found_item;
Item ** find_item_in_list(Item *item, List<Item> &items, uint *counter,
                          find_item_error_report_type report_error,
                          bool *unaliased);
bool get_key_map_from_key_list(key_map *map, TABLE *table,
                               List<String> *index_list);
bool insert_fields(THD *thd,TABLE_LIST *tables,
		   const char *db_name, const char *table_name,
		   List_iterator<Item> *it, bool any_privileges,
                   bool allocate_view_names);
bool setup_tables(THD *thd, TABLE_LIST *tables, Item **conds);
int setup_wild(THD *thd, TABLE_LIST *tables, List<Item> &fields,
	       List<Item> *sum_func_list, uint wild_num);
int setup_fields(THD *thd, Item** ref_pointer_array, TABLE_LIST *tables,
		 List<Item> &item, bool set_query_id,
		 List<Item> *sum_func_list, bool allow_sum_func);
int setup_conds(THD *thd,TABLE_LIST *tables,COND **conds);
int setup_ftfuncs(SELECT_LEX* select);
int init_ftfuncs(THD *thd, SELECT_LEX* select, bool no_order);
void wait_for_refresh(THD *thd);
int open_tables(THD *thd, TABLE_LIST *tables, uint *counter);
int simple_open_n_lock_tables(THD *thd,TABLE_LIST *tables);
int open_and_lock_tables(THD *thd,TABLE_LIST *tables);
int lock_tables(THD *thd, TABLE_LIST *tables, uint counter);
TABLE *open_temporary_table(THD *thd, const char *path, const char *db,
			    const char *table_name, bool link_in_list);
bool rm_temporary_table(enum db_type base, char *path);
void free_io_cache(TABLE *entry);
void intern_close_table(TABLE *entry);
bool close_thread_table(THD *thd, TABLE **table_ptr);
void close_temporary_tables(THD *thd);
TABLE_LIST *find_table_in_list(TABLE_LIST *table,
                               uint offset_to_list,
                               const char *db_name,
                               const char *table_name);
TABLE_LIST *unique_table(TABLE_LIST *table, TABLE_LIST *table_list);
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
int fill_record(List<Item> &fields,List<Item> &values, bool ignore_errors);
int fill_record(Field **field,List<Item> &values, bool ignore_errors);
OPEN_TABLE_LIST *list_open_tables(THD *thd, const char *wild);

inline TABLE_LIST *find_table_in_global_list(TABLE_LIST *table,
                                             const char *db_name,
                                             const char *table_name)
{
  return find_table_in_list(table, offsetof(TABLE_LIST, next_global),
                            db_name, table_name);
}

inline TABLE_LIST *find_table_in_local_list(TABLE_LIST *table,
                                            const char *db_name,
                                            const char *table_name)
{
  return find_table_in_list(table, offsetof(TABLE_LIST, next_local),
                            db_name, table_name);
}


/* sql_calc.cc */
bool eval_const_cond(COND *cond);

/* sql_load.cc */
int mysql_load(THD *thd, sql_exchange *ex, TABLE_LIST *table_list,
	       List<Item> &fields, enum enum_duplicates handle_duplicates,
	       bool local_file, thr_lock_type lock_type,
	       bool ignore_check_option_errors);
int write_record(THD *thd, TABLE *table, COPY_INFO *info);

/* sql_manager.cc */
/* bits set in manager_status */
#define MANAGER_BERKELEY_LOG_CLEANUP    (1L << 0)
extern ulong volatile manager_status;
extern bool volatile manager_thread_in_use, mqh_used;
extern pthread_t manager_thread;
extern "C" pthread_handler_decl(handle_manager, arg);

/* sql_test.cc */
#ifndef DBUG_OFF
void print_where(COND *cond,const char *info);
void print_cached_tables(void);
void TEST_filesort(SORT_FIELD *sortorder,uint s_length);
void print_plan(JOIN* join, double read_time, double record_count,
                uint idx, const char *info);
#endif
void mysql_print_status(THD *thd);
/* key.cc */
int find_ref_key(TABLE *form,Field *field, uint *offset);
void key_copy(byte *to_key, byte *from_record, KEY *key_info, uint key_length);
void key_restore(byte *to_record, byte *from_key, KEY *key_info,
                 uint key_length);
bool key_cmp_if_same(TABLE *form,const byte *key,uint index,uint key_length);
void key_unpack(String *to,TABLE *form,uint index);
bool check_if_key_used(TABLE *table, uint idx, List<Item> &fields);
int key_cmp(KEY_PART_INFO *key_part, const byte *key, uint key_length);

bool init_errmessage(void);
void sql_perror(const char *message);

void vprint_msg_to_log(enum loglevel level, const char *format, va_list args);
void sql_print_error(const char *format, ...);
void sql_print_warning(const char *format, ...);
void sql_print_information(const char *format, ...);



bool fn_format_relative_to_data_home(my_string to, const char *name,
				     const char *dir, const char *extension);
bool open_log(MYSQL_LOG *log, const char *hostname,
	      const char *opt_name, const char *extension,
	      const char *index_file_name,
	      enum_log_type type, bool read_append,
	      bool no_auto_events, ulong max_size);

/* mysqld.cc */
extern void yyerror(const char*);

/* item_func.cc */
extern bool check_reserved_words(LEX_STRING *name);

/* strfunc.cc */
ulonglong find_set(TYPELIB *lib, const char *x, uint length, CHARSET_INFO *cs,
		   char **err_pos, uint *err_len, bool *set_warning);
uint find_type(TYPELIB *lib, const char *find, uint length, bool part_match);
uint find_type2(TYPELIB *lib, const char *find, uint length, CHARSET_INFO *cs);
uint check_word(TYPELIB *lib, const char *val, const char *end,
		const char **end_of_word);

bool is_keyword(const char *name, uint len);


#define MY_DB_OPT_FILE "db.opt"
bool load_db_opt(THD *thd, const char *path, HA_CREATE_INFO *create);
bool my_dbopt_init(void);
void my_dbopt_cleanup(void);
void my_dbopt_free(void);

/*
  External variables
*/

extern time_t start_time;
extern char *mysql_data_home,server_version[SERVER_VERSION_LENGTH],
	    mysql_real_data_home[], *opt_mysql_tmpdir, mysql_charsets_dir[],
            def_ft_boolean_syntax[sizeof(ft_boolean_syntax)];
#define mysql_tmpdir (my_tmpdir(&mysql_tmpdir_list))
extern MY_TMPDIR mysql_tmpdir_list;
extern const char *command_name[];
extern const char *first_keyword, *my_localhost, *delayed_user, *binary_keyword;
extern const char **errmesg;			/* Error messages */
extern const char *myisam_recover_options_str;
extern const char *in_left_expr_name, *in_additional_cond;
extern Eq_creator eq_creator;
extern Ne_creator ne_creator;
extern Gt_creator gt_creator;
extern Lt_creator lt_creator;
extern Ge_creator ge_creator;
extern Le_creator le_creator;
extern char language[LIBLEN],reg_ext[FN_EXTLEN];
extern char glob_hostname[FN_REFLEN], mysql_home[FN_REFLEN];
extern char pidfile_name[FN_REFLEN], system_time_zone[30], *opt_init_file;
extern char log_error_file[FN_REFLEN];
extern double last_query_cost;
extern double log_10[32];
extern ulonglong log_10_int[20];
extern ulonglong keybuff_size;
extern ulong refresh_version,flush_version, thread_id,query_id;
extern ulong binlog_cache_use, binlog_cache_disk_use;
extern ulong aborted_threads,aborted_connects;
extern ulong delayed_insert_timeout;
extern ulong delayed_insert_limit, delayed_queue_size;
extern ulong delayed_insert_threads, delayed_insert_writes;
extern ulong delayed_rows_in_use,delayed_insert_errors;
extern ulong slave_open_temp_tables;
extern ulong query_cache_size, query_cache_min_res_unit;
extern ulong thd_startup_options, slow_launch_threads, slow_launch_time;
extern ulong server_id, concurrency;
extern ulong ha_read_count, ha_discover_count;
extern ulong table_cache_size;
extern ulong max_connections,max_connect_errors, connect_timeout;
extern ulong slave_net_timeout;
extern ulong max_user_connections;
extern ulong what_to_log,flush_time;
extern ulong query_buff_size, thread_stack,thread_stack_min;
extern ulong binlog_cache_size, max_binlog_cache_size, open_files_limit;
extern ulong max_binlog_size, max_relay_log_size;
extern ulong rpl_recovery_rank, thread_cache_size;
extern ulong back_log;
extern ulong specialflag, current_pid;
extern ulong expire_logs_days, sync_binlog_period, sync_binlog_counter;
extern my_bool relay_log_purge, opt_innodb_safe_binlog;
extern uint test_flags,select_errors,ha_open_options;
extern uint protocol_version, mysqld_port, dropping_tables;
extern uint delay_key_write_options, lower_case_table_names;
extern bool opt_endinfo, using_udf_functions, locked_in_memory;
extern bool opt_using_transactions, mysqld_embedded;
extern bool using_update_log, opt_large_files, server_id_supplied;
extern bool opt_log, opt_update_log, opt_bin_log, opt_slow_log, opt_error_log;
extern bool opt_disable_networking, opt_skip_show_db;
extern bool volatile abort_loop, shutdown_in_progress, grant_option;
extern bool mysql_proc_table_exists;
extern uint volatile thread_count, thread_running, global_read_lock;
extern my_bool opt_sql_bin_update, opt_safe_user_create, opt_no_mix_types;
extern my_bool opt_safe_show_db, opt_local_infile;
extern my_bool opt_slave_compressed_protocol, use_temp_pool;
extern my_bool opt_readonly, lower_case_file_system;
extern my_bool opt_enable_named_pipe, opt_sync_frm;
extern my_bool opt_secure_auth;
extern uint opt_crash_binlog_innodb;
extern char *shared_memory_base_name, *mysqld_unix_port;
extern bool opt_enable_shared_memory;
extern char *default_tz_name;

extern MYSQL_LOG mysql_log,mysql_slow_log,mysql_bin_log;
extern FILE *bootstrap_file;
extern pthread_key(MEM_ROOT**,THR_MALLOC);
extern pthread_mutex_t LOCK_mysql_create_db,LOCK_Acl,LOCK_open,
       LOCK_thread_count,LOCK_mapped_file,LOCK_user_locks, LOCK_status,
       LOCK_error_log, LOCK_delayed_insert, LOCK_uuid_generator,
       LOCK_delayed_status, LOCK_delayed_create, LOCK_crypt, LOCK_timezone,
       LOCK_slave_list, LOCK_active_mi, LOCK_manager,
       LOCK_global_system_variables, LOCK_user_conn;
extern rw_lock_t LOCK_grant, LOCK_sys_init_connect, LOCK_sys_init_slave;
extern pthread_cond_t COND_refresh, COND_thread_count, COND_manager;
extern pthread_attr_t connection_attrib;
extern I_List<THD> threads;
extern I_List<NAMED_LIST> key_caches;
extern MY_BITMAP temp_pool;
extern String my_empty_string;
extern const String my_null_string;
extern SHOW_VAR init_vars[],status_vars[], internal_vars[];
extern SHOW_COMP_OPTION have_isam;
extern SHOW_COMP_OPTION have_innodb;
extern SHOW_COMP_OPTION have_berkeley_db;
extern SHOW_COMP_OPTION have_ndbcluster;
extern struct system_variables global_system_variables;
extern struct system_variables max_system_variables;
extern struct system_status_var global_status_var;
extern struct rand_struct sql_rand;
extern KEY_CACHE *sql_key_cache;

extern const char *opt_date_time_formats[];
extern KNOWN_DATE_TIME_FORMAT known_date_time_formats[];

extern String null_string;
extern HASH open_cache;
extern TABLE *unused_tables;
extern I_List<i_string> binlog_do_db, binlog_ignore_db;
extern const char* any_db;
extern struct my_option my_long_options[];

/* optional things, have_* variables */

extern SHOW_COMP_OPTION have_isam, have_innodb, have_berkeley_db;
extern SHOW_COMP_OPTION have_example_db, have_archive_db, have_csv_db;
extern SHOW_COMP_OPTION have_raid, have_openssl, have_symlink;
extern SHOW_COMP_OPTION have_query_cache, have_berkeley_db, have_innodb;
extern SHOW_COMP_OPTION have_geometry, have_rtree_keys;
extern SHOW_COMP_OPTION have_crypt;
extern SHOW_COMP_OPTION have_compress;

#ifndef __WIN__
extern pthread_t signal_thread;
#endif

#ifdef HAVE_OPENSSL
extern struct st_VioSSLAcceptorFd * ssl_acceptor_fd;
#endif /* HAVE_OPENSSL */

MYSQL_LOCK *mysql_lock_tables(THD *thd,TABLE **table,uint count);
void mysql_unlock_tables(THD *thd, MYSQL_LOCK *sql_lock);
void mysql_unlock_read_tables(THD *thd, MYSQL_LOCK *sql_lock);
void mysql_unlock_some_tables(THD *thd, TABLE **table,uint count);
void mysql_lock_remove(THD *thd, MYSQL_LOCK *locked,TABLE *table);
void mysql_lock_abort(THD *thd, TABLE *table);
void mysql_lock_abort_for_thread(THD *thd, TABLE *table);
MYSQL_LOCK *mysql_lock_merge(MYSQL_LOCK *a,MYSQL_LOCK *b);
bool lock_global_read_lock(THD *thd);
void unlock_global_read_lock(THD *thd);
bool wait_if_global_read_lock(THD *thd, bool abort_on_refresh,
                              bool is_not_commit);
void start_waiting_global_read_lock(THD *thd);
void make_global_read_lock_block_commit(THD *thd);

/* Lock based on name */
int lock_and_wait_for_table_name(THD *thd, TABLE_LIST *table_list);
int lock_table_name(THD *thd, TABLE_LIST *table_list);
void unlock_table_name(THD *thd, TABLE_LIST *table_list);
bool wait_for_locked_table_names(THD *thd, TABLE_LIST *table_list);
bool lock_table_names(THD *thd, TABLE_LIST *table_list);
void unlock_table_names(THD *thd, TABLE_LIST *table_list,
			TABLE_LIST *last_table= 0);


/* old unireg functions */

void unireg_init(ulong options);
void unireg_end(void);
bool mysql_create_frm(THD *thd, my_string file_name,
		      HA_CREATE_INFO *create_info,
		      List<create_field> &create_field,
		      uint key_count,KEY *key_info,handler *db_type);
int rea_create_table(THD *thd, my_string file_name,HA_CREATE_INFO *create_info,
		     List<create_field> &create_field,
		     uint key_count,KEY *key_info);
int format_number(uint inputflag,uint max_length,my_string pos,uint length,
		  my_string *errpos);
int openfrm(THD *thd, const char *name,const char *alias,uint filestat,
            uint prgflag, uint ha_open_flags, TABLE *outparam);
int readfrm(const char *name, const void** data, uint* length);
int writefrm(const char* name, const void* data, uint len);
int closefrm(TABLE *table);
db_type get_table_type(const char *name);
int read_string(File file, gptr *to, uint length);
void free_blobs(TABLE *table);
int set_zone(int nr,int min_zone,int max_zone);
ulong convert_period_to_month(ulong period);
ulong convert_month_to_period(ulong month);
void get_date_from_daynr(long daynr,uint *year, uint *month,
			 uint *day);
my_time_t TIME_to_timestamp(THD *thd, const TIME *t, bool *not_exist);
bool str_to_time_with_warn(const char *str,uint length,TIME *l_time);
timestamp_type str_to_datetime_with_warn(const char *str, uint length,
                                         TIME *l_time, uint flags);
longlong number_to_TIME(longlong nr, TIME *time_res, bool fuzzy_date,
                        int *was_cut);
void localtime_to_TIME(TIME *to, struct tm *from);
void calc_time_from_sec(TIME *to, long seconds, long microseconds);

void make_truncated_value_warning(THD *thd, const char *str_val,
				  uint str_length, timestamp_type time_type,
                                  const char *field_name);
extern DATE_TIME_FORMAT *date_time_format_make(timestamp_type format_type,
					       const char *format_str,
					       uint format_length);
extern DATE_TIME_FORMAT *date_time_format_copy(THD *thd,
					       DATE_TIME_FORMAT *format);
const char *get_date_time_format_str(KNOWN_DATE_TIME_FORMAT *format,
				     timestamp_type type);
extern bool make_date_time(DATE_TIME_FORMAT *format, TIME *l_time,
			   timestamp_type type, String *str);
void make_datetime(const DATE_TIME_FORMAT *format, const TIME *l_time,
                   String *str);
void make_date(const DATE_TIME_FORMAT *format, const TIME *l_time,
               String *str);
void make_time(const DATE_TIME_FORMAT *format, const TIME *l_time,
               String *str);
ulonglong TIME_to_ulonglong_datetime(const TIME *time);
ulonglong TIME_to_ulonglong_date(const TIME *time);
ulonglong TIME_to_ulonglong_time(const TIME *time);
ulonglong TIME_to_ulonglong(const TIME *time);

int test_if_number(char *str,int *res,bool allow_wildcards);
void change_byte(byte *,uint,char,char);
void init_read_record(READ_RECORD *info, THD *thd, TABLE *reg_form,
		      SQL_SELECT *select,
		      int use_record_cache, bool print_errors);
void end_read_record(READ_RECORD *info);
ha_rows filesort(THD *thd, TABLE *form,struct st_sort_field *sortorder,
		 uint s_length, SQL_SELECT *select,
		 ha_rows max_rows, ha_rows *examined_rows);
void filesort_free_buffers(TABLE *table);
void change_double_for_sort(double nr,byte *to);
int get_quick_record(SQL_SELECT *select);
int calc_weekday(long daynr,bool sunday_first_day_of_week);
uint calc_week(TIME *l_time, uint week_behaviour, uint *year);
void find_date(char *pos,uint *vek,uint flag);
TYPELIB *convert_strings_to_array_type(my_string *typelibs, my_string *end);
TYPELIB *typelib(List<String> &strings);
ulong get_form_pos(File file, uchar *head, TYPELIB *save_names);
ulong make_new_entry(File file,uchar *fileinfo,TYPELIB *formnames,
		     const char *newname);
ulong next_io_size(ulong pos);
void append_unescaped(String *res, const char *pos, uint length);
int create_frm(char *name,uint reclength,uchar *fileinfo,
	       HA_CREATE_INFO *create_info, uint keys);
void update_create_info_from_table(HA_CREATE_INFO *info, TABLE *form);
int rename_file_ext(const char * from,const char * to,const char * ext);
bool check_db_name(char *db);
bool check_column_name(const char *name);
bool check_table_name(const char *name, uint length);
char *get_field(MEM_ROOT *mem, Field *field);
bool get_field(MEM_ROOT *mem, Field *field, class String *res);
int wild_case_compare(CHARSET_INFO *cs, const char *str,const char *wildstr);

/* from hostname.cc */
struct in_addr;
my_string ip_to_hostname(struct in_addr *in,uint *errors);
void inc_host_errors(struct in_addr *in);
void reset_host_errors(struct in_addr *in);
bool hostname_cache_init();
void hostname_cache_free();
void hostname_cache_refresh(void);

/* sql_cache.cc */
extern bool sql_cache_init();
extern void sql_cache_free();
extern int sql_cache_hit(THD *thd, char *inBuf, uint length);

/* item.cc */
Item *get_system_var(THD *thd, enum_var_type var_type, LEX_STRING name,
		     LEX_STRING component);
Item *get_system_var(THD *thd, enum_var_type var_type, const char *var_name,
		     uint length, const char *item_name);
/* item_func.cc */
int get_var_with_binlog(THD *thd, LEX_STRING &name,
                        user_var_entry **out_entry);
/* log.cc */
bool flush_error_log(void);

/* sql_list.cc */
void free_list(I_List <i_string_pair> *list);
void free_list(I_List <i_string> *list);

/* sql_yacc.cc */
extern int yyparse(void *thd);

/* frm_crypt.cc */
#ifdef HAVE_CRYPTED_FRM
SQL_CRYPT *get_crypt_for_frm(void);
#endif

#include "sql_view.h"

/* Some inline functions for more speed */

inline bool add_item_to_list(THD *thd, Item *item)
{
  return thd->lex->current_select->add_item_to_list(thd, item);
}

inline bool add_value_to_list(THD *thd, Item *value)
{
  return thd->lex->value_list.push_back(value);
}

inline bool add_order_to_list(THD *thd, Item *item, bool asc)
{
  return thd->lex->current_select->add_order_to_list(thd, item, asc);
}

inline bool add_group_to_list(THD *thd, Item *item, bool asc)
{
  return thd->lex->current_select->add_group_to_list(thd, item, asc);
}

inline void mark_as_null_row(TABLE *table)
{
  table->null_row=1;
  table->status|=STATUS_NULL_ROW;
  bfill(table->null_flags,table->null_bytes,255);
}

inline void table_case_convert(char * name, uint length)
{
  if (lower_case_table_names)
    my_casedn(files_charset_info, name, length);
}

inline const char *table_case_name(HA_CREATE_INFO *info, const char *name)
{
  return ((lower_case_table_names == 2 && info->alias) ? info->alias : name);
}

inline ulong sql_rnd_with_mutex()
{
  pthread_mutex_lock(&LOCK_thread_count);
  ulong tmp=(ulong) (my_rnd(&sql_rand) * 0xffffffff); /* make all bits random */
  pthread_mutex_unlock(&LOCK_thread_count);
  return tmp;
}

Comp_creator *comp_eq_creator(bool invert);
Comp_creator *comp_ge_creator(bool invert);
Comp_creator *comp_gt_creator(bool invert);
Comp_creator *comp_le_creator(bool invert);
Comp_creator *comp_lt_creator(bool invert);
Comp_creator *comp_ne_creator(bool invert);

Item * all_any_subquery_creator(Item *left_expr,
				chooser_compare_func_creator cmp,
				bool all,
				SELECT_LEX *select_lex);

/*
  clean/setup table fields and map

  SYNOPSYS
    setup_table_map()
    table - TABLE structure pointer (which should be setup)
    table_list TABLE_LIST structure pointer (owner of TABLE)
    tablenr - table number
*/

inline void setup_table_map(TABLE *table, TABLE_LIST *table_list, uint tablenr)
{
  table->used_fields= 0;
  table->const_table= 0;
  table->null_row= 0;
  table->status= STATUS_NO_RECORD;
  table->keys_in_use_for_query= table->keys_in_use;
  table->maybe_null= test(table->outer_join= table_list->outer_join);
  table->tablenr= tablenr;
  table->map= (table_map) 1 << tablenr;
  table->force_index= table_list->force_index;
}


/*
  Some functions that are different in the embedded library and the normal
  server
*/

#ifndef EMBEDDED_LIBRARY
extern "C" void unireg_abort(int exit_code);
void kill_delayed_threads(void);
bool check_stack_overrun(THD *thd,char *dummy);
#else
#define unireg_abort(exit_code) DBUG_RETURN(exit_code)
inline void kill_delayed_threads(void) {}
#define check_stack_overrun(A, B) 0
#endif

#endif /* MYSQL_CLIENT */
