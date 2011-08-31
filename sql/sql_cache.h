/* Copyright (c) 2001, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _SQL_CACHE_H
#define _SQL_CACHE_H

#include "hash.h"
#include "my_base.h"                            /* ha_rows */

class MY_LOCALE;
struct TABLE_LIST;
class Time_zone;
struct LEX;
struct TABLE;
typedef struct st_changed_table_list CHANGED_TABLE_LIST;

/* Query cache */

/*
   Can't create new free memory block if unused memory in block less
   then QUERY_CACHE_MIN_ALLOCATION_UNIT.
   if QUERY_CACHE_MIN_ALLOCATION_UNIT == 0 then
   QUERY_CACHE_MIN_ALLOCATION_UNIT choosed automaticaly
*/
#define QUERY_CACHE_MIN_ALLOCATION_UNIT		512

/* inittial size of hashes */
#define QUERY_CACHE_DEF_QUERY_HASH_SIZE		1024
#define QUERY_CACHE_DEF_TABLE_HASH_SIZE		1024

/* minimal result data size when data allocated */
#define QUERY_CACHE_MIN_RESULT_DATA_SIZE	1024*4

/* 
   start estimation of first result block size only when number of queries
   bigger then: 
*/
#define QUERY_CACHE_MIN_ESTIMATED_QUERIES_NUMBER 3



/* memory bins size spacing (see at Query_cache::init_cache (sql_cache.cc)) */
#define QUERY_CACHE_MEM_BIN_FIRST_STEP_PWR2	4
#define QUERY_CACHE_MEM_BIN_STEP_PWR2		2
#define QUERY_CACHE_MEM_BIN_PARTS_INC		1
#define QUERY_CACHE_MEM_BIN_PARTS_MUL		1.2
#define QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2	3

/* how many free blocks check when finding most suitable before other 'end'
   of list of free blocks */
#define QUERY_CACHE_MEM_BIN_TRY                 5

/* packing parameters */
#define QUERY_CACHE_PACK_ITERATION		2
#define QUERY_CACHE_PACK_LIMIT			(512*1024L)

#define TABLE_COUNTER_TYPE uint

struct Query_cache_block;
struct Query_cache_block_table;
struct Query_cache_table;
struct Query_cache_query;
struct Query_cache_result;
class Query_cache;
struct Query_cache_tls;
struct LEX;
class THD;

typedef my_bool (*qc_engine_callback)(THD *thd, char *table_key,
                                      uint key_length,
                                      ulonglong *engine_data);

/**
  This class represents a node in the linked chain of queries
  belonging to one table.

  @note The root of this linked list is not a query-type block, but the table-
        type block which all queries has in common.
*/
struct Query_cache_block_table
{
  Query_cache_block_table() {}                /* Remove gcc warning */

  /**
    This node holds a position in a static table list belonging
    to the associated query (base 0).
  */
  TABLE_COUNTER_TYPE n;

  /**
    Pointers to the next and previous node, linking all queries with 
    a common table.
  */
  Query_cache_block_table *next, *prev;

  /**
    A pointer to the table-type block which all
    linked queries has in common.
  */
  Query_cache_table *parent;

  /**
    A method to calculate the address of the query cache block
    owning this node. The purpose of this calculation is to 
    make it easier to move the query cache block without having
    to modify all the pointer addresses.
  */
  inline Query_cache_block *block();
};

struct Query_cache_block
{
  Query_cache_block() {}                      /* Remove gcc warning */
  enum block_type {FREE, QUERY, RESULT, RES_CONT, RES_BEG,
		   RES_INCOMPLETE, TABLE, INCOMPLETE};

  ulong length;					// length of all block
  ulong used;					// length of data
  /*
    Not used **pprev, **prev because really needed access to pervious block:
    *pprev to join free blocks
    *prev to access to opposite side of list in cyclic sorted list
  */
  Query_cache_block *pnext,*pprev,		// physical next/previous block
		    *next,*prev;		// logical next/previous block
  block_type type;
  TABLE_COUNTER_TYPE n_tables;			// number of tables in query

  inline my_bool is_free(void) { return type == FREE; }
  void init(ulong length);
  void destroy();
  inline uint headers_len();
  inline uchar* data(void);
  inline Query_cache_query *query();
  inline Query_cache_table *table();
  inline Query_cache_result *result();
  inline Query_cache_block_table *table(TABLE_COUNTER_TYPE n);
};

struct Query_cache_query
{
  ulonglong limit_found_rows;
  mysql_rwlock_t lock;
  Query_cache_block *res;
  Query_cache_tls *wri;
  ulong len;
  uint8 tbls_type;
  unsigned int last_pkt_nr;

  Query_cache_query() {}                      /* Remove gcc warning */
  inline void init_n_lock();
  void unlock_n_destroy();
  inline ulonglong found_rows()		   { return limit_found_rows; }
  inline void found_rows(ulonglong rows)   { limit_found_rows= rows; }
  inline Query_cache_block *result()	   { return res; }
  inline void result(Query_cache_block *p) { res= p; }
  inline Query_cache_tls *writer()	   { return wri; }
  inline void writer(Query_cache_tls *p)   { wri= p; }
  inline uint8 tables_type()               { return tbls_type; }
  inline void tables_type(uint8 type)      { tbls_type= type; }
  inline ulong length()			   { return len; }
  inline ulong add(ulong packet_len)	   { return(len+= packet_len); }
  inline void length(ulong length_arg)	   { len= length_arg; }
  inline uchar* query()
  {
    return (((uchar*)this) + ALIGN_SIZE(sizeof(Query_cache_query)));
  }
  void lock_writing();
  void lock_reading();
  my_bool try_lock_writing();
  void unlock_writing();
  void unlock_reading();
};


struct Query_cache_table
{
  Query_cache_table() {}                      /* Remove gcc warning */
  char *tbl;
  uint32 key_len;
  uint8 table_type;
  /* unique for every engine reference */
  qc_engine_callback callback_func;
  /* data need by some engines */
  ulonglong engine_data_buff;

  /**
    The number of queries depending of this table.
  */
  int32 m_cached_query_count;

  inline char *db()			     { return (char *) data(); }
  inline char *table()			     { return tbl; }
  inline void table(char *table_arg)	     { tbl= table_arg; }
  inline uint32 key_length()                 { return key_len; }
  inline void key_length(uint32 len)         { key_len= len; }
  inline uint8 type()                        { return table_type; }
  inline void type(uint8 t)                  { table_type= t; }
  inline qc_engine_callback callback()       { return callback_func; }
  inline void callback(qc_engine_callback fn){ callback_func= fn; }
  inline ulonglong engine_data()             { return engine_data_buff; }
  inline void engine_data(ulonglong data_arg){ engine_data_buff= data_arg; }
  inline uchar* data()
  {
    return (uchar*)(((uchar*)this)+
		  ALIGN_SIZE(sizeof(Query_cache_table)));
  }
};

struct Query_cache_result
{
  Query_cache_result() {}                     /* Remove gcc warning */
  Query_cache_block *query;

  inline uchar* data()
  {
    return (uchar*)(((uchar*) this)+
		  ALIGN_SIZE(sizeof(Query_cache_result)));
  }
  /* data_continue (if not whole packet contained by this block) */
  inline Query_cache_block *parent()		  { return query; }
  inline void parent (Query_cache_block *p)	  { query=p; }
};


extern "C"
{
  uchar *query_cache_query_get_key(const uchar *record, size_t *length,
                                   my_bool not_used);
  uchar *query_cache_table_get_key(const uchar *record, size_t *length,
                                   my_bool not_used);
}
extern "C" void query_cache_invalidate_by_MyISAM_filename(const char* filename);


struct Query_cache_memory_bin
{
  Query_cache_memory_bin() {}                 /* Remove gcc warning */
#ifndef DBUG_OFF
  ulong size;
#endif
  uint number;
  Query_cache_block *free_blocks;

  inline void init(ulong size_arg)
  {
#ifndef DBUG_OFF
    size = size_arg;
#endif
    number = 0;
    free_blocks = 0;
  }
};

struct Query_cache_memory_bin_step
{
  Query_cache_memory_bin_step() {}            /* Remove gcc warning */
  ulong size;
  ulong increment;
  uint idx;
  inline void init(ulong size_arg, uint idx_arg, ulong increment_arg)
  {
    size = size_arg;
    idx = idx_arg;
    increment = increment_arg;
  }
};

class Query_cache
{
public:
  /* Info */
  ulong query_cache_size, query_cache_limit;
  /* statistics */
  ulong free_memory, queries_in_cache, hits, inserts, refused,
    free_memory_blocks, total_blocks, lowmem_prunes;


private:
#ifndef DBUG_OFF
  my_thread_id m_cache_lock_thread_id;
#endif
  mysql_cond_t COND_cache_status_changed;
  enum Cache_lock_status { UNLOCKED, LOCKED_NO_WAIT, LOCKED };
  Cache_lock_status m_cache_lock_status;

  bool m_query_cache_is_disabled;

  void free_query_internal(Query_cache_block *point);
  void invalidate_table_internal(THD *thd, uchar *key, uint32 key_length);
  void disable_query_cache(void) { m_query_cache_is_disabled= TRUE; }

protected:
  /*
    The following mutex is locked when searching or changing global
    query, tables lists or hashes. When we are operating inside the
    query structure we locked an internal query block mutex.
    LOCK SEQUENCE (to prevent deadlocks):
      1. structure_guard_mutex
      2. query block (for operation inside query (query block/results))

    Thread doing cache flush releases the mutex once it sets
    m_cache_status flag, so other threads may bypass the cache as
    if it is disabled, not waiting for reset to finish.  The exception
    is other threads that were going to do cache flush---they'll wait
    till the end of a flush operation.
  */
  mysql_mutex_t structure_guard_mutex;
  uchar *cache;					// cache memory
  Query_cache_block *first_block;		// physical location block list
  Query_cache_block *queries_blocks;		// query list (LIFO)
  Query_cache_block *tables_blocks;

  Query_cache_memory_bin *bins;			// free block lists
  Query_cache_memory_bin_step *steps;		// bins spacing info
  HASH queries, tables;
  /* options */
  ulong min_allocation_unit, min_result_data_size;
  uint def_query_hash_size, def_table_hash_size;
  
  uint mem_bin_num, mem_bin_steps;		// See at init_cache & find_bin

  my_bool initialized;

  /* Exclude/include from cyclic double linked list */
  static void double_linked_list_exclude(Query_cache_block *point,
					 Query_cache_block **list_pointer);
  static void double_linked_list_simple_include(Query_cache_block *point,
						Query_cache_block **
						list_pointer);
  static void double_linked_list_join(Query_cache_block *head_tail,
				      Query_cache_block *tail_head);

  /* Table key generation */
  static uint filename_2_table_key (char *key, const char *filename,
				    uint32 *db_langth);

  /* The following functions require that structure_guard_mutex is locked */
  void flush_cache();
  my_bool free_old_query();
  void free_query(Query_cache_block *point);
  my_bool allocate_data_chain(Query_cache_block **result_block,
			      ulong data_len,
			      Query_cache_block *query_block,
			      my_bool first_block);
  void invalidate_table(THD *thd, TABLE_LIST *table);
  void invalidate_table(THD *thd, TABLE *table);
  void invalidate_table(THD *thd, uchar *key, uint32  key_length);
  void invalidate_table(THD *thd, Query_cache_block *table_block);
  void invalidate_query_block_list(THD *thd, 
                                   Query_cache_block_table *list_root);

  TABLE_COUNTER_TYPE
    register_tables_from_list(TABLE_LIST *tables_used,
                              TABLE_COUNTER_TYPE counter,
                              Query_cache_block_table *block_table);
  my_bool register_all_tables(Query_cache_block *block,
			      TABLE_LIST *tables_used,
			      TABLE_COUNTER_TYPE tables);
  my_bool insert_table(uint key_len, char *key,
		       Query_cache_block_table *node,
		       uint32 db_length, uint8 cache_type,
		       qc_engine_callback callback,
		       ulonglong engine_data);
  void unlink_table(Query_cache_block_table *node);
  Query_cache_block *get_free_block (ulong len, my_bool not_less,
				      ulong min);
  void free_memory_block(Query_cache_block *point);
  void split_block(Query_cache_block *block, ulong len);
  Query_cache_block *join_free_blocks(Query_cache_block *first_block,
				       Query_cache_block *block_in_list);
  my_bool append_next_free_block(Query_cache_block *block,
				 ulong add_size);
  void exclude_from_free_memory_list(Query_cache_block *free_block);
  void insert_into_free_memory_list(Query_cache_block *new_block);
  my_bool move_by_type(uchar **border, Query_cache_block **before,
		       ulong *gap, Query_cache_block *i);
  uint find_bin(ulong size);
  void move_to_query_list_end(Query_cache_block *block);
  void insert_into_free_memory_sorted_list(Query_cache_block *new_block,
					   Query_cache_block **list);
  void pack_cache();
  void relink(Query_cache_block *oblock,
	      Query_cache_block *nblock,
	      Query_cache_block *next,
	      Query_cache_block *prev,
	      Query_cache_block *pnext,
	      Query_cache_block *pprev);
  my_bool join_results(ulong join_limit);

  /*
    Following function control structure_guard_mutex
    by themself or don't need structure_guard_mutex
  */
  ulong init_cache();
  void make_disabled();
  void free_cache();
  Query_cache_block *write_block_data(ulong data_len, uchar* data,
				       ulong header_len,
				       Query_cache_block::block_type type,
				       TABLE_COUNTER_TYPE ntab = 0);
  my_bool append_result_data(Query_cache_block **result,
			     ulong data_len, uchar* data,
			     Query_cache_block *parent);
  my_bool write_result_data(Query_cache_block **result,
			    ulong data_len, uchar* data,
			    Query_cache_block *parent,
			    Query_cache_block::block_type
			    type=Query_cache_block::RESULT);
  inline ulong get_min_first_result_data_size();
  inline ulong get_min_append_result_data_size();
  Query_cache_block *allocate_block(ulong len, my_bool not_less,
				     ulong min);
  /*
    If query is cacheable return number tables in query
    (query without tables not cached)
  */
  TABLE_COUNTER_TYPE is_cacheable(THD *thd, size_t query_len,
                                  const char *query,
                                  LEX *lex, TABLE_LIST *tables_used,
                                  uint8 *tables_type);
  TABLE_COUNTER_TYPE process_and_count_tables(THD *thd,
                                              TABLE_LIST *tables_used,
                                              uint8 *tables_type);

  static my_bool ask_handler_allowance(THD *thd, TABLE_LIST *tables_used);
 public:

  Query_cache(ulong query_cache_limit = ULONG_MAX,
	      ulong min_allocation_unit = QUERY_CACHE_MIN_ALLOCATION_UNIT,
	      ulong min_result_data_size = QUERY_CACHE_MIN_RESULT_DATA_SIZE,
	      uint def_query_hash_size = QUERY_CACHE_DEF_QUERY_HASH_SIZE,
	      uint def_table_hash_size = QUERY_CACHE_DEF_TABLE_HASH_SIZE);

  bool is_disabled(void) { return m_query_cache_is_disabled; }

  /* initialize cache (mutex) */
  void init();
  /* resize query cache (return real query size, 0 if disabled) */
  ulong resize(ulong query_cache_size);
  /* set limit on result size */
  inline void result_size_limit(ulong limit){query_cache_limit=limit;}
  /* set minimal result data allocation unit size */
  ulong set_min_res_unit(ulong size);

  /* register query in cache */
  void store_query(THD *thd, TABLE_LIST *used_tables);

  /*
    Check if the query is in the cache and if this is true send the
    data to client.
  */
  int send_result_to_client(THD *thd, char *query, uint query_length);

  /* Remove all queries that uses any of the listed following tables */
  void invalidate(THD* thd, TABLE_LIST *tables_used,
		  my_bool using_transactions);
  void invalidate(CHANGED_TABLE_LIST *tables_used);
  void invalidate_locked_for_write(TABLE_LIST *tables_used);
  void invalidate(THD* thd, TABLE *table, my_bool using_transactions);
  void invalidate(THD *thd, const char *key, uint32  key_length,
		  my_bool using_transactions);

  /* Remove all queries that uses any of the tables in following database */
  void invalidate(char *db);

  /* Remove all queries that uses any of the listed following table */
  void invalidate_by_MyISAM_filename(const char *filename);

  void flush();
  void pack(ulong join_limit = QUERY_CACHE_PACK_LIMIT,
	    uint iteration_limit = QUERY_CACHE_PACK_ITERATION);

  void destroy();

  void insert(Query_cache_tls *query_cache_tls,
              const char *packet,
              ulong length,
              unsigned pkt_nr);

  void end_of_result(THD *thd);
  void abort(Query_cache_tls *query_cache_tls);

  /*
    The following functions are only used when debugging
    We don't protect these with ifndef DBUG_OFF to not have to recompile
    everything if we want to add checks of the cache at some places.
  */
  void wreck(uint line, const char *message);
  void bins_dump();
  void cache_dump();
  void queries_dump();
  void tables_dump();
  my_bool check_integrity(bool not_locked);
  my_bool in_list(Query_cache_block * root, Query_cache_block * point,
		  const char *name);
  my_bool in_table_list(Query_cache_block_table * root,
			Query_cache_block_table * point,
			const char *name);
  my_bool in_blocks(Query_cache_block * point);

  bool try_lock(bool use_timeout= FALSE);
  void lock(void);
  void lock_and_suspend(void);
  void unlock(void);
};

#ifdef HAVE_QUERY_CACHE
struct Query_cache_query_flags
{
  unsigned int client_long_flag:1;
  unsigned int client_protocol_41:1;
  unsigned int protocol_type:2;
  unsigned int more_results_exists:1;
  unsigned int in_trans:1;
  unsigned int autocommit:1;
  unsigned int pkt_nr;
  uint character_set_client_num;
  uint character_set_results_num;
  uint collation_connection_num;
  ha_rows limit;
  Time_zone *time_zone;
  ulong sql_mode;
  ulong max_sort_length;
  ulong group_concat_max_len;
  ulong default_week_format;
  ulong div_precision_increment;
  MY_LOCALE *lc_time_names;
};
#define QUERY_CACHE_FLAGS_SIZE sizeof(Query_cache_query_flags)
#include "sql_cache.h"
#define query_cache_abort(A) query_cache.abort(A)
#define query_cache_end_of_result(A) query_cache.end_of_result(A)
#define query_cache_store_query(A, B) query_cache.store_query(A, B)
#define query_cache_destroy() query_cache.destroy()
#define query_cache_result_size_limit(A) query_cache.result_size_limit(A)
#define query_cache_init() query_cache.init()
#define query_cache_resize(A) query_cache.resize(A)
#define query_cache_set_min_res_unit(A) query_cache.set_min_res_unit(A)
#define query_cache_invalidate3(A, B, C) query_cache.invalidate(A, B, C)
#define query_cache_invalidate1(A) query_cache.invalidate(A)
#define query_cache_send_result_to_client(A, B, C) \
  query_cache.send_result_to_client(A, B, C)
#define query_cache_invalidate_by_MyISAM_filename_ref \
  &query_cache_invalidate_by_MyISAM_filename
/* note the "maybe": it's a read without mutex */
#define query_cache_maybe_disabled(T)                                 \
  (T->variables.query_cache_type == 0 || query_cache.query_cache_size == 0)
#define query_cache_is_cacheable_query(L) \
  (((L)->sql_command == SQLCOM_SELECT) && (L)->safe_to_cache_query)
#else
#define QUERY_CACHE_FLAGS_SIZE 0
#define query_cache_store_query(A, B)
#define query_cache_destroy()
#define query_cache_result_size_limit(A)
#define query_cache_init()
#define query_cache_resize(A)
#define query_cache_set_min_res_unit(A)
#define query_cache_invalidate3(A, B, C)
#define query_cache_invalidate1(A)
#define query_cache_send_result_to_client(A, B, C) 0
#define query_cache_invalidate_by_MyISAM_filename_ref NULL

#define query_cache_abort(A)
#define query_cache_end_of_result(A)
#define query_cache_invalidate_by_MyISAM_filename_ref NULL
#define query_cache_maybe_disabled(T) 1
#define query_cache_is_cacheable_query(L) 0
#endif /*HAVE_QUERY_CACHE*/

extern Query_cache query_cache;
#endif
