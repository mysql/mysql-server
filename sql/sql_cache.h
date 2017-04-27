/* Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef _SQL_CACHE_H
#define _SQL_CACHE_H

/* Query cache */

#include <stddef.h>
#include <sys/types.h>

#include "hash.h"         // HASH
#include "my_inttypes.h"
#include "my_thread_local.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"

class THD;
struct LEX;
struct Query_cache_block_table;
struct Query_cache_memory_bin;
struct Query_cache_memory_bin_step;
struct Query_cache_query;
struct Query_cache_result;
struct Query_cache_table;
struct TABLE;
struct TABLE_LIST;

typedef struct st_mysql_const_lex_string LEX_CSTRING;


/* minimal result data size when data allocated */
static const ulong QUERY_CACHE_MIN_RESULT_DATA_SIZE= 1024*4;

typedef size_t TABLE_COUNTER_TYPE;

typedef bool (*qc_engine_callback)(THD *thd, const char *table_key,
                                   uint key_length,
                                   ulonglong *engine_data);

/**
  libmysql convenience wrapper to insert data into query cache.
*/
void query_cache_insert(const uchar *packet, size_t length, uint pkt_nr);


extern "C" void query_cache_invalidate_by_MyISAM_filename(const char* filename);


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

  bool is_free() const { return type == FREE; }
  void init(ulong block_length)
  {
    length= block_length;
    used= 0;
    type= Query_cache_block::FREE;
    n_tables= 0;
  }
  void destroy() { type= INCOMPLETE; }
  inline uint headers_len() const;
  inline uchar* data();
  inline Query_cache_query *query();
  inline Query_cache_table *table();
  inline Query_cache_result *result();
  inline Query_cache_block_table *table(TABLE_COUNTER_TYPE n);
};


class Query_cache
{
public:
  /* Info */
  ulong query_cache_size;
  ulong query_cache_limit;

  /* statistics */
  ulong free_memory;
  ulong queries_in_cache;
  ulong hits;
  ulong inserts;
  ulong refused;
  ulong free_memory_blocks;
  ulong total_blocks;
  ulong lowmem_prunes;

private:
#ifndef DBUG_OFF
  my_thread_id m_cache_lock_thread_id;
#endif
  mysql_cond_t COND_cache_status_changed;
  enum Cache_lock_status { UNLOCKED, LOCKED_NO_WAIT, LOCKED };
  Cache_lock_status m_cache_lock_status;

  bool m_query_cache_is_disabled;

  void free_query_internal(Query_cache_block *point);

  void invalidate_table_internal(const uchar *key, size_t key_length);

  void disable_query_cache() { m_query_cache_is_disabled= true; }

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
  ulong min_allocation_unit;
  ulong min_result_data_size;
  uint def_query_hash_size;
  uint def_table_hash_size;

  uint mem_bin_num, mem_bin_steps;		// See at init_cache & find_bin

  bool initialized;

  /* The following functions require that structure_guard_mutex is locked */
  void flush_cache(THD *thd);
  bool free_old_query();
  void free_query(Query_cache_block *point);
  bool allocate_data_chain(Query_cache_block **result_block,
                           size_t data_len,
                           Query_cache_block *query_block,
                           bool first_block);
  void invalidate_table(THD *thd, TABLE_LIST *table);
  void invalidate_table(THD *thd, TABLE *table);
  void invalidate_table(THD *thd, const uchar *key, size_t key_length);
  void invalidate_query_block_list(Query_cache_block_table *list_root);

  TABLE_COUNTER_TYPE
    register_tables_from_list(TABLE_LIST *tables_used,
                              TABLE_COUNTER_TYPE counter,
                              Query_cache_block_table *block_table);
  bool register_all_tables(Query_cache_block *block,
                           TABLE_LIST *tables_used);
  bool insert_table(size_t key_len, const uchar *key,
                    Query_cache_block_table *node, size_t db_length,
                    uint8 cache_type, qc_engine_callback callback,
                    ulonglong engine_data);
  void unlink_table(Query_cache_block_table *node);
  Query_cache_block *get_free_block (size_t len, bool not_less, size_t min);
  void free_memory_block(Query_cache_block *point);
  void split_block(Query_cache_block *block, ulong len);
  Query_cache_block *join_free_blocks(Query_cache_block *first_block,
				       Query_cache_block *block_in_list);
  bool append_next_free_block(Query_cache_block *block,
                              ulong add_size);
  void exclude_from_free_memory_list(Query_cache_block *free_block);
  void insert_into_free_memory_list(Query_cache_block *new_block);
  bool move_by_type(uchar **border, Query_cache_block **before,
                    ulong *gap, Query_cache_block *i);
  uint find_bin(size_t size);
  void move_to_query_list_end(Query_cache_block *block);
  void insert_into_free_memory_sorted_list(Query_cache_block *new_block,
					   Query_cache_block **list);
  void pack_cache();
  bool join_results(ulong join_limit);

  /*
    Following function control structure_guard_mutex
    by themself or don't need structure_guard_mutex
  */
  ulong init_cache();
  void make_disabled();
  void free_cache();
  Query_cache_block *write_block_data(size_t data_len, const uchar* data,
                                      size_t header_len,
                                      Query_cache_block::block_type type,
                                      TABLE_COUNTER_TYPE ntab);
  bool append_result_data(THD *thd, Query_cache_block **result,
                          size_t data_len, const uchar* data,
                          Query_cache_block *parent);
  bool write_result_data(THD *thd, Query_cache_block **result,
                         size_t data_len, const uchar* data,
                         Query_cache_block *parent,
                         Query_cache_block::block_type type);
  inline ulong get_min_first_result_data_size() const;
  ulong get_min_append_result_data_size() const { return min_result_data_size; }
  Query_cache_block *allocate_block(size_t len, bool not_less, size_t min);
  /*
    If query is cacheable return number tables in query
    (query without tables not cached)
  */
  TABLE_COUNTER_TYPE is_cacheable(THD *thd, LEX *lex, TABLE_LIST *tables_used,
                                  uint8 *tables_type) const;
  TABLE_COUNTER_TYPE process_and_count_tables(THD *thd,
                                              TABLE_LIST *tables_used,
                                              uint8 *tables_type) const;

  bool try_lock(THD *thd, bool use_timeout);
  void lock(THD *thd);
  void lock_and_suspend(THD *thd);
  void unlock(THD *thd);

public:
  Query_cache();

  bool is_disabled() const { return m_query_cache_is_disabled; }

  /** initialize cache (mutex) */
  void init();

  /** resize query cache (return real query size, 0 if disabled) */
  ulong resize(THD *thd, ulong query_cache_size);

  /** set minimal result data allocation unit size */
  ulong set_min_res_unit(ulong size);

  /** register query in cache */
  void store_query(THD *thd, TABLE_LIST *used_tables);

  /**
    Check if the query is in the cache and if this is true send the
    data to client.
  */
  int send_result_to_client(THD *thd, const LEX_CSTRING &sql);

  /** Remove all queries that use the given table */
  void invalidate_single(THD* thd, TABLE_LIST *table_used,
                         bool using_transactions);

  /** Remove all queries that uses any of the listed following tables */
  void invalidate(THD* thd, TABLE_LIST *tables_used,
		  bool using_transactions);

  void invalidate_locked_for_write(THD *thd, TABLE_LIST *tables_used);

  void invalidate(THD* thd, TABLE *table, bool using_transactions);

  void invalidate(THD *thd, const char *key, uint32 key_length,
		  bool using_transactions);

  /** Remove all queries that uses any of the tables in following database */
  void invalidate(THD *thd, const char *db);

  /** Remove all queries that uses any of the listed following table */
  void invalidate_by_MyISAM_filename(THD *thd, const char *filename);

  void flush(THD *thd);

  void pack(THD *thd);

  void destroy(THD *thd);

  void insert(THD *thd, const uchar *packet, size_t length, uint pkt_nr);

  void end_of_result(THD *thd);

  void abort(THD *thd);
};


extern Query_cache query_cache;
#endif
