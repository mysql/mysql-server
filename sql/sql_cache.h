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

#ifndef _SQL_CACHE_H
#define _SQL_CACHE_H

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


struct Query_cache_block_table
{
  TABLE_COUNTER_TYPE n;		// numbr in table (from 0)
  Query_cache_block_table *next, *prev;
  Query_cache_table *parent;
  inline Query_cache_block *block();
};


struct Query_cache_block
{
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
  inline gptr data(void);
  inline Query_cache_query *query();
  inline Query_cache_table *table();
  inline Query_cache_result *result();
  inline Query_cache_block_table *table(TABLE_COUNTER_TYPE n);
};

struct Query_cache_query
{
  ulonglong limit_found_rows;
  rw_lock_t lock;
  Query_cache_block *res;
  NET *wri;
  ulong len;
  uint8 tbls_type;

  inline void init_n_lock();
  void unlock_n_destroy();
  inline ulonglong found_rows()		   { return limit_found_rows; }
  inline void found_rows(ulonglong rows)   { limit_found_rows= rows; }
  inline Query_cache_block *result()	   { return res; }
  inline void result(Query_cache_block *p) { res= p; }
  inline NET *writer()			   { return wri; }
  inline void writer(NET *p)		   { wri= p; }
  inline uint8 tables_type()               { return tbls_type; }
  inline void tables_type(uint8 type)      { tbls_type= type; }
  inline ulong length()			   { return len; }
  inline ulong add(ulong packet_len)	   { return(len+= packet_len); }
  inline void length(ulong length)	   { len= length; }
  inline gptr query()
  {
    return (gptr)(((byte*)this)+
		  ALIGN_SIZE(sizeof(Query_cache_query)));
  }
  void lock_writing();
  void lock_reading();
  my_bool try_lock_writing();
  void unlock_writing();
  void unlock_reading();
  static byte *cache_key(const byte *record, uint *length, my_bool not_used);
};


struct Query_cache_table
{
  char *tbl;
  uint32 key_len;
  uint8 table_type;
  /* unique for every engine reference */
  qc_engine_callback callback_func;
  /* data need by some engines */
  ulonglong engine_data_buff;

  inline char *db()			     { return (char *) data(); }
  inline char *table()			     { return tbl; }
  inline void table(char *table)	     { tbl= table; }
  inline uint32 key_length()                 { return key_len; }
  inline void key_length(uint32 len)         { key_len= len; }
  inline uint8 type()                        { return table_type; }
  inline void type(uint8 t)                  { table_type= t; }
  inline qc_engine_callback callback()       { return callback_func; }
  inline void callback(qc_engine_callback fn){ callback_func= fn; }
  inline ulonglong engine_data()             { return engine_data_buff; }
  inline void engine_data(ulonglong data)    { engine_data_buff= data; }
  inline gptr data()
  {
    return (gptr)(((byte*)this)+
		  ALIGN_SIZE(sizeof(Query_cache_table)));
  }
};

struct Query_cache_result
{
  Query_cache_block *query;

  inline gptr data()
  {
    return (gptr)(((byte*) this)+
		  ALIGN_SIZE(sizeof(Query_cache_result)));
  }
  /* data_continue (if not whole packet contained by this block) */
  inline Query_cache_block *parent()		  { return query; }
  inline void parent (Query_cache_block *p)	  { query=p; }
};


extern "C"
{
  byte *query_cache_query_get_key(const byte *record, uint *length,
				  my_bool not_used);
  byte *query_cache_table_get_key(const byte *record, uint *length,
				  my_bool not_used);
}
void query_cache_insert(NET *thd, const char *packet, ulong length);
extern "C" void query_cache_invalidate_by_MyISAM_filename(const char* filename);


struct Query_cache_memory_bin
{
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

protected:
  /*
    The following mutex is locked when searching or changing global
    query, tables lists or hashes. When we are operating inside the
    query structure we locked an internal query block mutex.
    LOCK SEQUENCE (to prevent deadlocks):
      1. structure_guard_mutex
      2. query block (for operation inside query (query block/results))
  */
  pthread_mutex_t structure_guard_mutex;
  byte *cache;					// cache memory
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
  void invalidate_table(TABLE_LIST *table);
  void invalidate_table(TABLE *table);
  void invalidate_table(byte *key, uint32  key_length);
  void invalidate_table(Query_cache_block *table_block);
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
  my_bool move_by_type(byte **border, Query_cache_block **before,
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
  void init();
  ulong init_cache();
  void make_disabled();
  void free_cache(my_bool destruction);
  Query_cache_block *write_block_data(ulong data_len, gptr data,
				       ulong header_len,
				       Query_cache_block::block_type type,
				       TABLE_COUNTER_TYPE ntab = 0,
				       my_bool under_guard=0);
  my_bool append_result_data(Query_cache_block **result,
			     ulong data_len, gptr data,
			     Query_cache_block *parent);
  my_bool write_result_data(Query_cache_block **result,
			    ulong data_len, gptr data,
			    Query_cache_block *parent,
			    Query_cache_block::block_type
			    type=Query_cache_block::RESULT);
  inline ulong get_min_first_result_data_size();
  inline ulong get_min_append_result_data_size();
  Query_cache_block *allocate_block(ulong len, my_bool not_less,
				     ulong min,
				     my_bool under_guard=0);
  /*
    If query is cacheable return number tables in query
    (query without tables not cached)
  */
  TABLE_COUNTER_TYPE is_cacheable(THD *thd, uint32 query_len, char *query,
				  LEX *lex, TABLE_LIST *tables_used,
				  uint8 *tables_type);

  static my_bool ask_handler_allowance(THD *thd, TABLE_LIST *tables_used);
 public:

  Query_cache(ulong query_cache_limit = ULONG_MAX,
	      ulong min_allocation_unit = QUERY_CACHE_MIN_ALLOCATION_UNIT,
	      ulong min_result_data_size = QUERY_CACHE_MIN_RESULT_DATA_SIZE,
	      uint def_query_hash_size = QUERY_CACHE_DEF_QUERY_HASH_SIZE,
	      uint def_table_hash_size = QUERY_CACHE_DEF_TABLE_HASH_SIZE);

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

  friend void query_cache_insert(NET *net, const char *packet, ulong length);
  friend void query_cache_end_of_result(THD *thd);
  friend void query_cache_abort(NET *net);

  /*
    The following functions are only used when debugging
    We don't protect these with ifndef DEBUG_OFF to not have to recompile
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
};

extern Query_cache query_cache;
extern TYPELIB query_cache_type_typelib;
void query_cache_end_of_result(THD *thd);
void query_cache_abort(NET *net);

#endif
