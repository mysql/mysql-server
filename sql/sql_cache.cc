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

#include "mysql_priv.h"
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include "sql_acl.h"
#include "ha_myisammrg.h"
#ifndef MASTER
#include "../srclib/myisammrg/myrg_def.h"
#else
#include "../myisammrg/myrg_def.h"
#endif

#ifdef EXTRA_DEBUG
#define MUTEX_LOCK(M) { DBUG_PRINT("info", ("mutex lock 0x%lx", (ulong)(M))); \
  pthread_mutex_lock(M);}
#define SEM_LOCK(M) { int val = 0; sem_getvalue (M, &val); \
  DBUG_PRINT("info", ("sem lock 0x%lx (%d)", (ulong)(M), val)); \
  sem_wait(M); DBUG_PRINT("info", ("sem lock ok")); }
#define MUTEX_UNLOCK(M) {DBUG_PRINT("info", ("mutex unlock 0x%lx",\
  (ulong)(M))); pthread_mutex_unlock(M);}
#define SEM_UNLOCK(M) {DBUG_PRINT("info", ("sem unlock 0x%lx", (ulong)(M))); \
  sem_post(M);	DBUG_PRINT("info", ("sem unlock ok")); }
#define STRUCT_LOCK(M) {DBUG_PRINT("info", ("%d struct lock...",__LINE__)); \
  pthread_mutex_lock(M);DBUG_PRINT("info", ("struct lock OK"));}
#define STRUCT_UNLOCK(M) { \
  DBUG_PRINT("info", ("%d struct unlock...",__LINE__)); \
  pthread_mutex_unlock(M);DBUG_PRINT("info", ("struct unlock OK"));}
#define BLOCK_LOCK_WR(B) {DBUG_PRINT("info", ("%d LOCK_WR 0x%lx",\
  __LINE__,(ulong)(B))); \
  B->query()->lock_writing();}
#define BLOCK_LOCK_RD(B) {DBUG_PRINT("info", ("%d LOCK_RD 0x%lx",\
  __LINE__,(ulong)(B))); \
  B->query()->lock_reading();}
#define BLOCK_UNLOCK_WR(B) { \
  DBUG_PRINT("info", ("%d UNLOCK_WR 0x%lx",\
  __LINE__,(ulong)(B)));B->query()->unlock_writing();}
#define BLOCK_UNLOCK_RD(B) { \
  DBUG_PRINT("info", ("%d UNLOCK_RD 0x%lx",\
  __LINE__,(ulong)(B)));B->query()->unlock_reading();}
#define DUMP(C) {C->bins_dump();C->cache_dump();\
  C->queries_dump();C->tables_dump();}
#else
#define MUTEX_LOCK(M) pthread_mutex_lock(M)
#define SEM_LOCK(M) sem_wait(M)
#define MUTEX_UNLOCK(M) pthread_mutex_unlock(M)
#define SEM_UNLOCK(M) sem_post(M)
#define STRUCT_LOCK(M) pthread_mutex_lock(M)
#define STRUCT_UNLOCK(M) pthread_mutex_unlock(M)
#define BLOCK_LOCK_WR(B) B->query()->lock_writing()
#define BLOCK_LOCK_RD(B) B->query()->lock_reading()
#define BLOCK_UNLOCK_WR(B) B->query()->unlock_writing()
#define BLOCK_UNLOCK_RD(B) B->query()->unlock_reading()
#define DUMP(C)
#endif

/*****************************************************************************
 *  Query_cache_block_table method(s)
 *****************************************************************************/

inline Query_cache_block * Query_cache_block_table::block()
{
  return (Query_cache_block *)( ((byte*)this) -
				ALIGN_SIZE(sizeof(Query_cache_block_table))*n -
				ALIGN_SIZE(sizeof(Query_cache_block)));
};

/*****************************************************************************
 *    Query_cache_block method(s)
 *****************************************************************************/

void Query_cache_block::init(ulong block_length)
{
  DBUG_ENTER("Query_cache_block::init");
  DBUG_PRINT("info", ("init block 0x%lx", (ulong) this));
  length = block_length;
  used = 0;
  type = Query_cache_block::FREE;
  n_tables = 0;
  DBUG_VOID_RETURN;
}

void Query_cache_block::destroy()
{
  DBUG_ENTER("Query_cache_block::destroy");
  DBUG_PRINT("info", ("destroy block 0x%lx, type %d",
		      (ulong)this, type));
  type = INCOMPLETE;
  DBUG_VOID_RETURN;
}

inline uint Query_cache_block::headers_len()
{
  return (ALIGN_SIZE(sizeof(Query_cache_block_table))*n_tables +
	  ALIGN_SIZE(sizeof(Query_cache_block)));
}

inline gptr Query_cache_block::data(void)
{
  return (gptr)( ((byte*)this) + headers_len() );
}

inline Query_cache_query * Query_cache_block::query()
{
#ifndef DBUG_OFF
  if (type != QUERY)
    query_cache.wreck(__LINE__, "incorrect block type");
#endif
  return (Query_cache_query *) data();
}

inline Query_cache_table * Query_cache_block::table()
{
#ifndef DBUG_OFF
  if (type != TABLE)
    query_cache.wreck(__LINE__, "incorrect block type");
#endif
  return (Query_cache_table *) data();
}

inline Query_cache_result * Query_cache_block::result()
{
#ifndef DBUG_OFF
  if (type != RESULT && type != RES_CONT && type != RES_BEG &&
      type != RES_INCOMPLETE)
    query_cache.wreck(__LINE__, "incorrect block type");
#endif
  return (Query_cache_result *) data();
}

inline Query_cache_block_table * Query_cache_block::table(TABLE_COUNTER_TYPE n)
{
  return ((Query_cache_block_table *)
	  (((byte*)this)+ALIGN_SIZE(sizeof(Query_cache_block)) +
	   n*ALIGN_SIZE(sizeof(Query_cache_block_table))));
}


/*****************************************************************************
 *   Query_cache_table method(s)
 *****************************************************************************/

byte * Query_cache_table::cache_key(const byte *record, uint *length,
				    my_bool not_used __attribute__((unused)))
{
  Query_cache_block* table_block = (Query_cache_block*) record;
  *length = (table_block->used - table_block->headers_len() -
	     ALIGN_SIZE(sizeof(Query_cache_table)));
  return (((byte *) table_block->data()) +
	  ALIGN_SIZE(sizeof(Query_cache_table)));
}

void Query_cache_table::free_cache(void *entry)
{
  //NOP
}

/*****************************************************************************
 *    Query_cache_query methods
 *****************************************************************************/

void Query_cache_query::init_n_lock()
{
  DBUG_ENTER("Query_cache_query::init_n_lock");
  res=0; wri = 0; len = 0;
  sem_init(&lock, 0, 1);
  pthread_mutex_init(&clients_guard,MY_MUTEX_INIT_FAST);
  clients = 0;
  lock_writing();
  DBUG_PRINT("info", ("inited & locked query for block 0x%lx",
		      ((byte*) this)-ALIGN_SIZE(sizeof(Query_cache_block))));
  DBUG_VOID_RETURN;
}

void Query_cache_query::unlock_n_destroy()
{
  DBUG_ENTER("Query_cache_query::unlock_n_destroy");
  this->unlock_writing();
  DBUG_PRINT("info", ("destroyed & unlocked query for block 0x%lx",
		      ((byte*)this)-ALIGN_SIZE(sizeof(Query_cache_block))));
  sem_destroy(&lock);
  pthread_mutex_destroy(&clients_guard);
  DBUG_VOID_RETURN;
}


/*
   Following methods work for block rwad/write locking only in this
   particular case and in interaction with structure_guard_mutex.

   Lock for write prevents any other locking.
   Lock for read prevents only locking for write.
*/

void Query_cache_query::lock_writing()
{
  SEM_LOCK(&lock);
}


/*
  Needed for finding queries, that we may delete from cache.
  We don't want wait while block become unlocked, in addition
  block locking mean that query now used and we not need to
  remove it
*/

my_bool Query_cache_query::try_lock_writing()
{
  DBUG_ENTER("Query_cache_block::try_lock_writing");
  if (sem_trywait(&lock)!=0 || clients != 0)
  {
    DBUG_PRINT("info", ("can't lock mutex"));
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("mutex 'lock' 0x%lx locked", (ulong) &lock));
  DBUG_RETURN(1);
}


void Query_cache_query::lock_reading()
{
  MUTEX_LOCK(&clients_guard);
  clients++;
  if (clients == 1) SEM_LOCK(&lock);
  MUTEX_UNLOCK(&clients_guard);
}


void Query_cache_query::unlock_writing()
{
  SEM_UNLOCK(&lock);
}


void Query_cache_query::unlock_reading()
{
  MUTEX_LOCK(&clients_guard);
  clients--;
  if (clients == 0) SEM_UNLOCK(&lock);
  MUTEX_UNLOCK(&clients_guard);

}


byte * Query_cache_query::cache_key( const byte *record, uint *length,
				     my_bool not_used)
{
  Query_cache_block * query_block = (Query_cache_block *) record;
  *length = (query_block->used - query_block->headers_len() -
	     ALIGN_SIZE(sizeof(Query_cache_query)));
  return (((byte *)query_block->data()) +
	  ALIGN_SIZE(sizeof(Query_cache_query)));
}


void Query_cache_query::free_cache(void *entry)
{
  //NOP
}

/*****************************************************************************
 * Query cache store functions
 *****************************************************************************/

void query_cache_insert(NET *net, const char *packet, ulong length)
{
  DBUG_ENTER("query_cache_insert");

#ifndef DBUG_OFF
  // Debugging method wreck may cause this
  if (query_cache.query_cache_size == 0)
    DBUG_VOID_RETURN;
#endif

  // Quick check on unlocked structure
  if (net->query_cache_query != 0)
  {
    STRUCT_LOCK(&query_cache.structure_guard_mutex);
    if (net->query_cache_query != 0)
    {
      Query_cache_block * query_block = (Query_cache_block*)
	net->query_cache_query;
      DUMP((&query_cache));
      BLOCK_LOCK_WR(query_block);
      DBUG_PRINT("info", ("insert packet %lu bytes long",length));
      Query_cache_query * header = query_block->query();

      Query_cache_block * result = header->result();
      /*
	STRUCT_UNLOCK(&query_cache.structure_guard_mutex); will be done by
	query_cache.append_result_data if success (if no success we need
	query_cache.structure_guard_mutex locked to free query)
      */
      if (!query_cache.append_result_data( result, length, (gptr) packet,
					   query_block, result))
      {
	DBUG_PRINT("warning", ("Can't append data"));
	header->result(result);
	DBUG_PRINT("info", ("free query 0x%lx", (ulong)query_block));
	query_cache.free_query(query_block);
	// append_result_data no success => we need unlock
	STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
	DBUG_VOID_RETURN;
      }
      header->result(result);
      BLOCK_UNLOCK_WR(query_block);
    }
    else
      STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

void query_cache_abort(NET *net)
{
  DBUG_ENTER("query_cache_abort");

#ifndef DBUG_OFF
  // debuging method wreck may cause this
  if (query_cache.query_cache_size == 0)
    DBUG_VOID_RETURN;
#endif

  // quick check on unlocked structure
  if (net->query_cache_query != 0)
  {
    STRUCT_LOCK(&query_cache.structure_guard_mutex);
    DUMP((&query_cache));
    Query_cache_block * query_block = ((Query_cache_block*)
				       net->query_cache_query);
    if (query_block)
    {
      BLOCK_LOCK_WR(query_block);
      query_cache.free_query(query_block);
      net->query_cache_query=0;
    }
    STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

void query_cache_end_of_result(NET * net)
{
  DBUG_ENTER("query_cache_end_of_result");

#ifndef DBUG_OFF
  // debuging method wreck may couse this
  if (query_cache.query_cache_size == 0) DBUG_VOID_RETURN;
#endif

  //quick check on unlocked structure
  if (net->query_cache_query != 0)
  {
    STRUCT_LOCK(&query_cache.structure_guard_mutex);
    if (net->query_cache_query != 0)
    {
      Query_cache_block * query_block = (Query_cache_block*)
	net->query_cache_query;
      DUMP((&query_cache));
      BLOCK_LOCK_WR(query_block);
      STRUCT_UNLOCK(&query_cache.structure_guard_mutex);

      Query_cache_query * header = query_block->query();
#ifndef DBUG_OFF
      if (header->result() != 0)
      {
#endif
	header->found_rows(current_thd->limit_found_rows);
	header->result()->type = Query_cache_block::RESULT;
#ifndef DBUG_OFF
      }
      else
      {
	DBUG_PRINT("error", ("end of data whith no result. query '%s'",
			     header->query()));
	query_cache.wreck(__LINE__, "");
	DBUG_VOID_RETURN;
      }
#endif
      net->query_cache_query=0;
      header->writer(0);
      BLOCK_UNLOCK_WR(query_block);
    }
    else
    {
      //cache was flushed or resized and query was deleted => do nothing
      STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
    }
    net->query_cache_query=0;
  }
  DBUG_VOID_RETURN;
}

void query_cache_invalidate_by_MyISAM_filename(char * filename)
{
  query_cache.invalidate_by_MyISAM_filename(filename);
}

/*****************************************************************************
 *    Query_cache methods
 *****************************************************************************/

/*****************************************************************************
 *    interface methods
 *****************************************************************************/

Query_cache::Query_cache(
			 ulong query_cache_limit,
			 ulong min_allocation_unit,
			 ulong min_result_data_size,
			 uint def_query_hash_size ,
			 uint def_table_hash_size):

  query_cache_size(0),
  query_cache_limit(query_cache_limit),
  min_allocation_unit(min_allocation_unit),
  min_result_data_size(min_result_data_size),
  def_query_hash_size(def_query_hash_size),
  def_table_hash_size(def_table_hash_size),
  queries_in_cache(0), hits(0), inserts(0), refused(0),
  initialized(0)
{
  if (min_allocation_unit < ALIGN_SIZE(sizeof(Query_cache_block)) +
      ALIGN_SIZE(sizeof(Query_cache_block_table)) +
      ALIGN_SIZE(sizeof(Query_cache_query)) + 3)
  {
    min_allocation_unit=ALIGN_SIZE(sizeof(Query_cache_block)) +
      ALIGN_SIZE(sizeof(Query_cache_block_table)) +
      ALIGN_SIZE(sizeof(Query_cache_query)) + 3;
  }
  this->min_allocation_unit = min_allocation_unit;
  if (min_result_data_size < min_allocation_unit)
    this->min_result_data_size = min_allocation_unit;
}

ulong Query_cache::resize(ulong query_cache_size)
{
  /*
     TODO: when will be realized pack() optimize case when
     query_cache_size < this->query_cache_size
  */
  /*
     TODO: try to copy old cache in new mamory
  */
  DBUG_ENTER("Query_cache::resize");
  DBUG_PRINT("info", ("from %lu to %lu",this->query_cache_size,\
    query_cache_size));
  free_cache(0);
  this->query_cache_size=query_cache_size;
  DBUG_RETURN(init_cache());
}

void Query_cache::store_query(THD *thd, TABLE_LIST *tables_used)
{
  /*
    TODO may be better convert keywords to upper case when query
    stored/compared
  */
  DBUG_ENTER("Query_cache::store_query");
  if (query_cache_size == 0)
    DBUG_VOID_RETURN;

  LEX * lex = &thd->lex;
  NET * net = &thd->net;

  TABLE_COUNTER_TYPE tables = 0;

  if ((tables = is_cachable(thd, thd->query_length,
			    thd->query, lex, tables_used))){
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size == 0)
      DBUG_VOID_RETURN;

    DUMP(this);

    /*
       prepare flags:
	 most significant bit - CLIENT_LONG_FLAG,
	 other - charset number (0 no charset convertion)
    */
    byte flags = (thd->client_capabilities & CLIENT_LONG_FLAG ? 0x80 : 0);
    if (thd->convert_set != 0)
    {
      flags |= (byte) thd->convert_set->number();
#ifndef DBUG_OFF
      if ( (thd->convert_set->number() & QUERY_CACHE_CHARSET_CONVERT_MASK) !=
	   thd->convert_set->number())
      {
	wreck(__LINE__,
	      "charset number bigger than QUERY_CACHE_CHARSET_CONVERT_MASK");
      }
#endif
    }

    /* check: Is it another thread who process same query? */
    thd->query[thd->query_length] = (char)flags;
    Query_cache_block *competitor = (Query_cache_block *)
      hash_search(&queries, thd->query, thd->query_length+1);
    thd->query[thd->query_length] = '\0';
    DBUG_PRINT("info", ("competitor 0x%lx, flags %x", (ulong)competitor,
			flags));

    if (competitor == 0)
    {
      thd->query[thd->query_length] = (char)flags;
      Query_cache_block * query_block =
	write_block_data(thd->query_length+1,
			 (gptr) thd->query,
			 ALIGN_SIZE(sizeof(Query_cache_query)),
			 Query_cache_block::QUERY, tables, 1);
      thd->query[thd->query_length] = '\0';
      if (query_block != 0)
      {
	DBUG_PRINT("info", ("query block 0x%lx allocated, %lu",
			    (ulong)query_block, query_block->used));

	Query_cache_query * header = query_block->query();
	header->init_n_lock();
	if (hash_insert(&queries, (byte*)query_block))
	{
	  refused++;
	  DBUG_PRINT("info", ("insertion in query hash"));
	  header->unlock_n_destroy();
	  free_memory_block(query_block);
	  STRUCT_UNLOCK(&structure_guard_mutex);
	  DBUG_VOID_RETURN;
	}
	if (!register_all_tables(query_block, tables_used, tables))
	{
	  refused++;
	  DBUG_PRINT("warning", ("tables list incliding filed"));
	  hash_delete(&queries, (char *) query_block);
	  header->unlock_n_destroy();
	  free_memory_block(query_block);
	  STRUCT_UNLOCK(&structure_guard_mutex);
	  DBUG_VOID_RETURN;
	}
	double_linked_list_simple_include(query_block, queries_blocks);
	inserts++;
	queries_in_cache++;
	STRUCT_UNLOCK(&structure_guard_mutex);

	net->query_cache_query = (gptr) query_block;
	header->writer(net);
	// init_n_lock make query block locked
	BLOCK_UNLOCK_WR(query_block);
      }
      else
      {
	refused++;
	// We have not enough memory to store query => do nothing
	STRUCT_UNLOCK(&structure_guard_mutex);
	DBUG_PRINT("warning", ("Can't allocate query"));
      }
    }
    else
    {
      refused++;
      // Another thread already procass same query => do nothing
      DBUG_PRINT("info", ("Another thread process same query"));
      STRUCT_UNLOCK(&structure_guard_mutex);
    }
  }
  else
    refused++;
  DBUG_VOID_RETURN;
}

my_bool Query_cache::send_result_to_client(
  THD *thd, char *sql, uint query_length)
{

  DBUG_ENTER("Query_cache::send_result_to_client");

  if (query_cache_size == 0 ||
      /*
	it is not possible to check has_transactions() function of handler
	because tables not opened yet
      */
      (thd->options & (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN)) ||
      thd->query_cache_type == 0)

  {
    DBUG_PRINT("info", ("query cache disabled on not in autocommit mode"));
    DBUG_RETURN(1);
  }

  char *begin = sql;
  while(*begin == ' ' || *begin == '\t') begin++;
  if (	toupper(begin[0])!='S' ||
	toupper(begin[1])!='E' ||
	toupper(begin[2])!='L')
  {
    DBUG_PRINT("info", ("Not look like SELECT"));
    DBUG_RETURN(1);
  }
  if (thd->temporary_tables != 0 || !thd->safe_to_cache_query )
  {
    DBUG_PRINT("info", ("SELECT is non-cachable"));
    DBUG_RETURN(1);
  }

  STRUCT_LOCK(&structure_guard_mutex);
  if (query_cache_size == 0)
  {
    DBUG_PRINT("info", ("query cache disabled on not in autocommit mode"));
    DBUG_RETURN(1);
  }
  DBUG_PRINT("info", (" sql %u '%s'", query_length, sql));
  Query_cache_block *query_block = 0;

  /*
     prepare flags:
       most significant bit - CLIENT_LONG_FLAG,
       other - charset number (0 no charset convertion)
  */
  byte flags = (thd->client_capabilities & CLIENT_LONG_FLAG ? 0x80 : 0);
  if (thd->convert_set != 0)
  {
    flags |= (byte) thd->convert_set->number();
#ifndef DBUG_OFF
    if ( (thd->convert_set->number() & QUERY_CACHE_CHARSET_CONVERT_MASK) !=
	 thd->convert_set->number())
    {
      wreck(__LINE__,
	    "charset number bigger than QUERY_CACHE_CHARSET_CONVERT_MASK");
    }
#endif
  }

  sql[query_length] = (char) flags;
  query_block = (Query_cache_block *)
    hash_search(&queries, sql, query_length+1);
  sql[query_length] = '\0';

  /*quick abort on unlocked data*/
  if (query_block == 0 ||
      query_block->query()->result() == 0 ||
      query_block->query()->result()->type != Query_cache_block::RESULT)
  {
    STRUCT_UNLOCK(&structure_guard_mutex);
    DBUG_PRINT("info", ("No query in query hash or no results"));
    DBUG_RETURN(1);
  }
  DBUG_PRINT("info", ("Query in query hash 0x%lx", (ulong)query_block));

  /* now lock and test that nothing changed while blocks was unlocked */
  BLOCK_LOCK_RD(query_block);
  STRUCT_UNLOCK(&structure_guard_mutex);

  Query_cache_query * query = query_block->query();
  Query_cache_block * first_result_block = query->result();
  Query_cache_block * result_block = first_result_block;
  if (result_block == 0 ||
      result_block->type != Query_cache_block::RESULT)
  {
    DBUG_PRINT("info", ("query found, but no data or data incomplete"));
    DBUG_RETURN(1); //no data in query
  }
  DBUG_PRINT("info", ("Query have result 0x%lx", (ulong)query));

  //check access;
  TABLE_COUNTER_TYPE t = 0;
  for(; t < query_block->n_tables; t++)
  {
      TABLE_LIST table_list;
      table_list.next = 0;
      table_list.use_index = table_list.ignore_index = 0;
      table_list.table = 0;
      table_list.grant.grant_table = 0;
      table_list.grant.version = table_list.grant.privilege =
	table_list.grant.want_privilege = 0;
      table_list.outer_join = 0;
      table_list.straight = 0;
      table_list.updating = 0;
      table_list.shared = 0;

      Query_cache_table * table = query_block->table(t)->parent;
      table_list.db = table->db();
      table_list.name = table_list.real_name = table->table();
      if (check_table_access(thd,SELECT_ACL,&table_list))
      {
	DBUG_PRINT("info",
		   ("probably no SELECT access to %s.%s =>\
 return to normal processing",
		    table_list.db, table_list.name));
	DBUG_RETURN(1); //no access
      }
  }

  move_to_query_list_end(query_block);

  hits++;
  do
  {
    DBUG_PRINT("info", ("Results  (len %lu, used %lu, headers %lu)",
			result_block->length, result_block->used,
			result_block->headers_len()+
			ALIGN_SIZE(sizeof(Query_cache_result))));

    Query_cache_result * result = result_block->result();
    net_real_write(&thd->net, result->data(),
		   result_block->used -
		   result_block->headers_len() -
		   ALIGN_SIZE(sizeof(Query_cache_result)));
    result_block = result_block->next;
  } while (result_block != first_result_block);

  thd->limit_found_rows = query->found_rows();

  BLOCK_UNLOCK_RD(query_block);

  DBUG_RETURN(0);
}

void Query_cache::invalidate(TABLE_LIST *tables_used)
{
  DBUG_ENTER("Query_cache::invalidate (table list)");
  if (query_cache_size > 0)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)
    {
      DUMP(this);
      for ( ; tables_used; tables_used=tables_used->next)
	invalidate_table(tables_used);
    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

void Query_cache::invalidate(TABLE *table)
{
  DBUG_ENTER("Query_cache::invalidate (table)");
  if (query_cache_size > 0)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)
      invalidate_table(table);
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

void Query_cache::invalidate(Query_cache_table::query_cache_table_type type)
{
  DBUG_ENTER("Query_cache::invalidate (type)");
  if (query_cache_size > 0)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    DUMP(this);
    if (query_cache_size > 0 && tables_blocks[type] != 0)
    {
      Query_cache_block *table_block = tables_blocks[type];
      do
      {
	/*
	  store next block address defore deletetion of current block
	*/
	Query_cache_block *next = table_block->next;

	invalidate_table(table_block);

	if (next == table_block)
	  break;

	table_block = next;
	} while (table_block != tables_blocks[type]);

    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

void Query_cache::invalidate(char *db)
{
  DBUG_ENTER("Query_cache::invalidate (db)");
  if (query_cache_size > 0)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)
    {
      DUMP(this);
      int i = 0;
      for(; i < (int) Query_cache_table::TYPES_NUMBER; i++)
      {
	if (tables_blocks[i] != 0) //cache not empty
	{
	  Query_cache_block *table_block = tables_blocks[i];
	  do
	  {
	    /*
	      store next block address defore deletetion of current block
	    */
	    Query_cache_block *next = table_block->next;

	    invalidate_table_in_db(table_block, db);

	    if (table_block == next)
	      break;

	    table_block = next;
	  } while (table_block != tables_blocks[i]);
	}
      }
    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

void Query_cache::invalidate_by_MyISAM_filename(char * filename)
{
  DBUG_ENTER("Query_cache::invalidate_by_MyISAM_filename");
  if (query_cache_size > 0)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)
    {
      char key[MAX_DBKEY_LENGTH];
      uint key_length =
	Query_cache::filename_2_table_key(key, filename);

	Query_cache_block *table_block;
	if ((table_block = (Query_cache_block*)
	     hash_search(&tables, key, key_length)))
	  invalidate_table(table_block);
    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}
void Query_cache::flush()
{
  DBUG_ENTER("Query_cache::flush");
  if (query_cache_size > 0)
  {
    DUMP(this);
    STRUCT_LOCK(&structure_guard_mutex);
    flush_cache();
    DUMP(this);
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

void Query_cache::pack(ulong join_limit, uint iteration_limit)
{
  DBUG_ENTER("Query_cache::pack");
  uint i = 0;
  do
  {
    pack_cache();
  } while ((++i < iteration_limit) && join_results(join_limit));
  DBUG_VOID_RETURN;
}

void Query_cache::destroy()
{
  DBUG_ENTER("Query_cache::destroy");
  free_cache(1);
  pthread_mutex_destroy(&structure_guard_mutex);
  initialized = 0;
  DBUG_VOID_RETURN;
}

#ifndef DBUG_OFF

void Query_cache::wreck(uint line, const char * message)
{
  DBUG_ENTER("Query_cache::wreck");
  query_cache_size = 0;
  DBUG_PRINT("error", (" %s", message));
  DBUG_PRINT("warning", ("=================================="));
  DBUG_PRINT("warning", ("%5d QUERY CACHE WRECK => DISABLED",line));
  DBUG_PRINT("warning", ("=================================="));
  current_thd->killed = 1;
  bins_dump();
  cache_dump();
  DBUG_VOID_RETURN;
}

void Query_cache::bins_dump()
{
  DBUG_PRINT("info", ("mem_bin_num=%u, mem_bin_steps=%u",
		      mem_bin_num, mem_bin_steps));
  DBUG_PRINT("info", ("-------------------------"));
  DBUG_PRINT("info", ("      size idx       step"));
  DBUG_PRINT("info", ("-------------------------"));
  uint i = 0;
  for(; i < mem_bin_steps; i++)
  {
    DBUG_PRINT("info", ("%10lu %3d %10lu", steps[i].size, steps[i].idx,
			steps[i].increment));
  }
  DBUG_PRINT("info", ("-------------------------"));
  DBUG_PRINT("info", ("      size num"));
  DBUG_PRINT("info", ("-------------------------"));
  i = 0;
  for(; i < mem_bin_num; i++)
  {
    DBUG_PRINT("info", ("%10lu %3d 0x%lx", bins[i].size, bins[i].number,
			(ulong)&(bins[i])));
    if (bins[i].free_blocks)
    {
      Query_cache_block * block = bins[i].free_blocks;
      do{
	DBUG_PRINT("info", ("\\-- %lu 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
			    block->length, (ulong)block,
			    (ulong)block->next, (ulong)block->prev,
			    (ulong)block->pnext, (ulong)block->pprev));
	block = block->next;
      } while ( block != bins[i].free_blocks );
    }
  }
  DBUG_PRINT("info", ("-------------------------"));
}

void Query_cache::cache_dump()
{
  DBUG_PRINT("info", ("-------------------------------------"));
  DBUG_PRINT("info", ("    length       used t nt"));
  DBUG_PRINT("info", ("-------------------------------------"));
  Query_cache_block * i = first_block;
  do
  {
    DBUG_PRINT("info",
	       ("%10lu %10lu %1d %2d 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
		i->length, i->used, (int)i->type,
		i->n_tables, (ulong)i,
		(ulong)i->next, (ulong)i->prev, (ulong)i->pnext,
		(ulong)i->pprev));
    i = i->pnext;
  } while ( i != first_block );
  DBUG_PRINT("info", ("-------------------------------------"));
}
void Query_cache::queries_dump()
{
  DBUG_PRINT("info", ("------------------"));
  DBUG_PRINT("info", (" QUERIES"));
  DBUG_PRINT("info", ("------------------"));
  if (queries_blocks != 0)
  {
    Query_cache_block * i = queries_blocks;
    do
    {
      uint len;
      char * str = (char*) Query_cache_query::cache_key((byte*) i, &len, 0);
      byte flags = (byte) str[len-1];
      str[len-1] = 0; //safe only under structure_guard_mutex locked
      DBUG_PRINT("info", ("%u (%u,%u) %s",len,
			  ((flags & QUERY_CACHE_CLIENT_LONG_FLAG_MASK)?1:0),
			  (flags & QUERY_CACHE_CHARSET_CONVERT_MASK), str));
      str[len-1] = (char)flags;
      DBUG_PRINT("info", ("-b- 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx", (ulong)i,
			  (ulong)i->next, (ulong)i->prev, (ulong)i->pnext,
			  (ulong)i->pprev));
      TABLE_COUNTER_TYPE t = 0;
      for (; t < i->n_tables; t++)
      {
	Query_cache_table * table = i->table(t)->parent;
	DBUG_PRINT("info", ("-t- '%s' '%s'", table->db(), table->table()));
      }
      Query_cache_query * header = i->query();
      if (header->result())
      {
	Query_cache_block * result_block = header->result();
	Query_cache_block * result_beg = result_block;
	do
	{
	  DBUG_PRINT("info", ("-r- %u %lu/%lu 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
			      (uint) result_block->type,
			      result_block->length, result_block->used,
			      (ulong) result_block,
			      (ulong) result_block->next,
			      (ulong) result_block->prev,
			      (ulong) result_block->pnext,
			      (ulong) result_block->pprev));
	  result_block = result_block->next;
	} while ( result_block != result_beg );
      }
      i = i->next;
    } while ( i != queries_blocks );
  }
  else
  {
    DBUG_PRINT("info", ("no queries in list"));
  }
  DBUG_PRINT("info", ("------------------"));
}

void Query_cache::tables_dump()
{
  DBUG_PRINT("info", ("--------------------"));
  DBUG_PRINT("info", ("TABLES"));
  DBUG_PRINT("info", ("--------------------"));
  int i = 0;
  for(; i < (int) Query_cache_table::TYPES_NUMBER; i++)
  {
    DBUG_PRINT("info", ("--- type %u", i));
    if (tables_blocks[i] != 0)
    {
      Query_cache_block * table_block = tables_blocks[i];
      do
      {
	Query_cache_table * table = table_block->table();
	DBUG_PRINT("info", ("'%s' '%s'", table->db(), table->table()));
	table_block = table_block->next;
      } while ( table_block != tables_blocks[i]);
    }
    else
      DBUG_PRINT("info", ("no tables in list"));
  }
  DBUG_PRINT("info", ("--------------------"));
}

#endif

/*****************************************************************************
 *   init/destroy
 *****************************************************************************/

void Query_cache::init()
{
  DBUG_ENTER("Query_cache::init");
  pthread_mutex_init(&structure_guard_mutex,MY_MUTEX_INIT_FAST);
  initialized = 1;
  DBUG_VOID_RETURN;
}

ulong Query_cache::init_cache()
{
  DBUG_ENTER("Query_cache::init_cache");
  if (!initialized)
  {
    DBUG_PRINT("info", ("first time init"));
    init();
  }
  ulong additional_data_size = 0,
    approx_additional_data_size = sizeof(Query_cache) +
    sizeof(gptr)*(def_query_hash_size+def_query_hash_size);

  ulong max_mem_bin_size = 0;
  if (query_cache_size < approx_additional_data_size)
  {
    additional_data_size = 0;
    approx_additional_data_size = 0;
    make_disabled();
  }
  else
  {
    query_cache_size -= approx_additional_data_size;

    //Count memory bins number.

    /*
      The idea is inherited from GNU malloc with some add-ons.
      Free memory blocks are stored in bins according to their sizes.
      The bins are stored in size-descending order.
      The bins are approximately logarithmically separated by size.

      Opposite to GNU malloc bin splitting is not fixed but calculated
      depending on cache size. Spliting calculating stored in cache and
      then used for bin finding.

      For example:
      query_cache_size>>QUERY_CACHE_MEM_BIN_FIRST_STEP_PWR2 = 100,
      min_allocation_unit = 17,
      QUERY_CACHE_MEM_BIN_STEP_PWR2 = 1,
      QUERY_CACHE_MEM_BIN_PARTS_INC = 1,
      QUERY_CACHE_MEM_BIN_PARTS_MUL = 1
      (in followed picture showed right (low) bound of bin):

      |       100>>1	 50>>1	      |25>>1|
      |		 |	   |	      |  |  |
      | 100  75 50  41 33 25  21 18 15| 12  | -  bins right (low) bounds
      |\---/\-----/\--------/\--------|---/ |
      |  0     1	2	   3  |     | - steps
       \-----------------------------/ \---/
	bins that we store in cache	this bin showed for example only
    */
    max_mem_bin_size =
      query_cache_size >> QUERY_CACHE_MEM_BIN_FIRST_STEP_PWR2;
    uint mem_bin_count = 1 + QUERY_CACHE_MEM_BIN_PARTS_INC;
    mem_bin_count = (uint) (mem_bin_count * QUERY_CACHE_MEM_BIN_PARTS_MUL);
    mem_bin_num = 1;
    mem_bin_steps = 1;
    ulong mem_bin_size = max_mem_bin_size >> QUERY_CACHE_MEM_BIN_STEP_PWR2;
    ulong prev_size = 0;
    while (mem_bin_size > min_allocation_unit)
    {
      mem_bin_num += mem_bin_count;
      prev_size = mem_bin_size;
      mem_bin_size >>= QUERY_CACHE_MEM_BIN_STEP_PWR2;
      mem_bin_steps++;
      mem_bin_count += QUERY_CACHE_MEM_BIN_PARTS_INC;
      mem_bin_count = (uint) (mem_bin_count * QUERY_CACHE_MEM_BIN_PARTS_MUL);

      //prevent too small bins spacing
      if (mem_bin_count > (mem_bin_size>>QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2))
	mem_bin_count=(mem_bin_size>>QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2);
    }
    ulong inc = (prev_size - mem_bin_size) / mem_bin_count;
    mem_bin_num += (mem_bin_count - (min_allocation_unit - mem_bin_size)/inc);
    mem_bin_steps++;
    additional_data_size = mem_bin_num *
      ALIGN_SIZE(sizeof(Query_cache_memory_bin))+
      mem_bin_steps * ALIGN_SIZE(sizeof(Query_cache_memory_bin_step));
  }
  if (query_cache_size < additional_data_size)
  {
    additional_data_size = 0;
    approx_additional_data_size = 0;
    make_disabled();
  }
  else
    query_cache_size -= additional_data_size;

  STRUCT_LOCK(&structure_guard_mutex);
  if (query_cache_size	<= min_allocation_unit)
  {
    DBUG_PRINT("info",
	       (" query_cache_size <= min_allocation_unit => cache disabled"));
    make_disabled();
  }
  else
  {
    if ( (cache = (byte *)
	  my_malloc_lock(query_cache_size+additional_data_size, MYF(0)))==0 )
    {
      DBUG_PRINT("warning",
		 ("can't allocate query cache memory => cache disabled"));
      make_disabled();
    }
    else
    {
      DBUG_PRINT("info",
		 ("cache length %lu, min unit %lu, %u bins",
		  query_cache_size, min_allocation_unit, mem_bin_num));

      steps = (Query_cache_memory_bin_step *) cache;
      bins = (Query_cache_memory_bin *)
	(cache +
	 mem_bin_steps * ALIGN_SIZE(sizeof(Query_cache_memory_bin_step)));

      first_block = (Query_cache_block *) (cache + additional_data_size);
      first_block->init(query_cache_size);
      first_block->pnext=first_block->pprev=first_block;
      first_block->next=first_block->prev=first_block;

      free_memory = query_cache_size;

      /* prepare bins */

      bins[0].init(max_mem_bin_size);
      steps[0].init(max_mem_bin_size,0,0);
      uint mem_bin_count = 1 + QUERY_CACHE_MEM_BIN_PARTS_INC;
      mem_bin_count = (uint) (mem_bin_count * QUERY_CACHE_MEM_BIN_PARTS_MUL);
      uint num = 1;
      ulong mem_bin_size = max_mem_bin_size >> QUERY_CACHE_MEM_BIN_STEP_PWR2;
      uint step = 1;
      while (mem_bin_size > min_allocation_unit)
      {
	ulong inc = (steps[step-1].size - mem_bin_size) / mem_bin_count;

	unsigned long size = mem_bin_size;
	uint i = mem_bin_count;
	for(; i > 0; i--)
	{
	  bins[num+i-1].init(size);
	  size += inc;
	}
	num += mem_bin_count;
	steps[step].init(mem_bin_size, num-1, inc);
	mem_bin_size >>= QUERY_CACHE_MEM_BIN_STEP_PWR2;
	step++;
	mem_bin_count += QUERY_CACHE_MEM_BIN_PARTS_INC;
	mem_bin_count = (uint) (mem_bin_count * QUERY_CACHE_MEM_BIN_PARTS_MUL);
	if (mem_bin_count > (mem_bin_size>>QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2))
	  mem_bin_count=(mem_bin_size>>QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2);
      }
      ulong inc = (steps[step-1].size - mem_bin_size) / mem_bin_count;
      /*
	num + mem_bin_count > mem_bin_num, but index never be > mem_bin_num
	because block with size < min_allocated_unit never will be requested
      */
      steps[step].init(mem_bin_size, num + mem_bin_count - 1, inc);
      uint skiped = (min_allocation_unit - mem_bin_size)/inc;
      ulong size = mem_bin_size + inc*skiped;
      uint i = mem_bin_count - skiped;
      for(; i > 0; i--)
      {
	bins[num+i-1].init(size);
	size += inc;
      }

      insert_into_free_memory_list(first_block);

      DUMP(this);

      VOID(hash_init(&queries,def_query_hash_size, 0, 0,
		     Query_cache_query::cache_key,
		     (void (*)(void*))Query_cache_query::free_cache,
		     0));
      VOID(hash_init(&tables,def_table_hash_size, 0, 0,
		     Query_cache_table::cache_key,
		     (void (*)(void*))Query_cache_table::free_cache,
		     0));
    }
  }
  queries_in_cache = 0;
  queries_blocks = 0;
  STRUCT_UNLOCK(&structure_guard_mutex);
  DBUG_RETURN(query_cache_size +
	      additional_data_size + approx_additional_data_size);
}

void Query_cache::make_disabled()
{
  DBUG_ENTER("Query_cache::make_disabled");
  query_cache_size = 0;
  free_memory = 0;
  bins = 0;
  steps = 0;
  cache = 0;
  mem_bin_num = mem_bin_steps = 0;
  DBUG_VOID_RETURN;
}

void Query_cache::free_cache(my_bool destruction)
{
  DBUG_ENTER("Query_cache::free_cache");
  if (query_cache_size > 0)
  {
    if (!destruction)
      STRUCT_LOCK(&structure_guard_mutex);

    flush_cache();
#ifndef DBUG_OFF
    if (bins[0].free_blocks == 0)
    {
      wreck(__LINE__,"no free memory found in (bins[0].free_blocks");
    }
#endif

    bins[0].free_blocks->destroy(); /* all cache memory must be
				       in one this block */
    DBUG_PRINT("info", ("free memory %lu (should be %lu)",
			free_memory , query_cache_size));
    free_memory = 0;

    my_free((gptr)cache, MYF(MY_ALLOW_ZERO_PTR));
    first_block = 0;
    bins = 0;
    cache = 0;
    query_cache_size = 0;
    hash_free(&queries);
    hash_free(&tables);
    queries_in_cache = 0;
    if (!destruction)
      STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

/*****************************************************************************
 *  free block data
 *****************************************************************************/

void Query_cache::flush_cache()
{
  while(queries_blocks != 0){
    BLOCK_LOCK_WR(queries_blocks);
    free_query(queries_blocks);
  }
}

my_bool Query_cache::free_old_query()
{
  DBUG_ENTER("Query_cache::free_old_query");
  if (queries_blocks == 0)
  {
    DBUG_RETURN(0);
  }
  /*
     try_lock_writing used to prevent clinch because
     here lock sequence is breached, also we don't need remove
     locked queries at this point
  */
  Query_cache_block *query_block = 0;
  if (queries_blocks != 0)
  {
    Query_cache_block *i = queries_blocks;
    do
    {
      Query_cache_query * header = i->query();
      if (header->result() != 0 &&
	  header->result()->type == Query_cache_block::RESULT &&
	  i->query()->try_lock_writing())
      {
	query_block = i;
	break;
      }
      i = i->next;
    } while ( i != queries_blocks );
  }

  if (query_block != 0)
  {
    free_query(query_block);
    DBUG_RETURN(1);
  }
  else
    DBUG_RETURN(0);
}

/* query_block must be lock_writing() */
void Query_cache::free_query(Query_cache_block * query_block)
{
  DBUG_ENTER("Query_cache::free_query");
  DBUG_PRINT("info", ("free query 0x%lx %lu bytes result",
		      (ulong)query_block,
		      query_block->query()->length() ));
  queries_in_cache--;
  hash_delete(&queries,(byte *) query_block);

  Query_cache_query * query = query_block->query();
  if (query->writer() != 0)
  {
    query->writer()->query_cache_query = 0;
    query->writer(0);
  }
  double_linked_list_exclude(query_block, queries_blocks);
  TABLE_COUNTER_TYPE i = 0;
  for(; i < query_block->n_tables; i++)
  {
    unlink_table(query_block->table(i));
  }
  Query_cache_block *result_block = query->result();
  if (result_block != 0)
  {
    Query_cache_block * block = result_block;
    do
    {
      Query_cache_block * current = block;
      block = block->next;
      free_memory_block(current);
    } while (block != result_block);
  }

  query->unlock_n_destroy();
  free_memory_block(query_block);

  DBUG_VOID_RETURN;
}

/*****************************************************************************
 *  query data creation
 *****************************************************************************/

Query_cache_block *
Query_cache::write_block_data(ulong data_len, gptr data,
			      ulong header_len,
			      Query_cache_block::block_type type,
			      TABLE_COUNTER_TYPE ntab,
			      my_bool under_guard)
{
  DBUG_ENTER("Query_cache::write_block_data");
  ulong all_headers_len = ALIGN_SIZE(sizeof(Query_cache_block)) +
    ntab*ALIGN_SIZE(sizeof(Query_cache_block_table)) +
    header_len;
  DBUG_PRINT("info", ("data: %ld, header: %ld, all header: %ld",
		      data_len, header_len, all_headers_len));
  ulong len = data_len + all_headers_len;
  Query_cache_block * block = allocate_block(max(len, min_allocation_unit),
					     1, 0, under_guard);
  if (block != 0)
  {
    block->type = type;
    block->n_tables = ntab;
    block->used = len;

    memcpy((void*)(((byte *) block)+
		   all_headers_len),
	   (void*)data,
	   data_len);
  }
  DBUG_RETURN(block);
}

my_bool
Query_cache::append_result_data(Query_cache_block * &result,
				ulong data_len, gptr data,
				Query_cache_block * query_block,
				Query_cache_block * first_data_block)
{
  DBUG_ENTER("Query_cache::uppend_result_data");
  DBUG_PRINT("info", ("append %lu bytes to 0x%lx query"));
  if (query_block->query()->add(data_len) > query_cache_limit)
  {
    DBUG_PRINT("info", ("size limit reached %lu > %lu",
			query_block->query()->length(),
			query_cache_limit));
    result=0;
    DBUG_RETURN(0);
  }
  if (first_data_block == 0)
  {
    DBUG_PRINT("info", ("allocated first result data block 0x%xl", data_len));
    /*
	STRUCT_UNLOCK(&structure_guard_mutex); will be done by
	query_cache.append_result_data if success;
    */
    DBUG_RETURN(write_result_data(result, data_len, data, query_block,
				  Query_cache_block::RES_BEG));
  }
  else
  {
    result = first_data_block;
  }
  Query_cache_block * last_block = first_data_block->prev;

  DBUG_PRINT("info", ("lastblock 0x%lx len %lu used %lu",
		      (ulong)last_block, last_block->length,
		      last_block->used));
  my_bool success = 1;

  ulong last_block_free_space = last_block->length - last_block->used;

  //write 'tail' of data, that can't be appended to last block

  //try join blocks if physicaly next block is free...
  if (last_block_free_space < data_len &&
      append_next_free_block(last_block,
			     max(data_len - last_block_free_space,
				 QUERY_CACHE_MIN_RESULT_DATA_SIZE)))
    last_block_free_space = last_block->length - last_block->used;
  //if no space in last block (even after join) allocate new block
  if (last_block_free_space < data_len)
  {
    //TODO try get memory from next free block (if exist) (is it needed?)
    DBUG_PRINT("info", ("allocate new block for %lu bytes",
			data_len-last_block_free_space));
    Query_cache_block *new_block = 0;
    /*
	STRUCT_UNLOCK(&structure_guard_mutex); will be done by
	query_cache.append_result_data
    */
    success = write_result_data(new_block, data_len-last_block_free_space,
				(gptr)(((byte*)data)+last_block_free_space),
				query_block,
				Query_cache_block::RES_CONT);
    /*
       new_block may be not 0 even !success (if write_result_data
       allocate small block but filed allocate continue
    */
    if (new_block != 0)
      double_linked_list_join(last_block, new_block);
  }
  else
    //it is success (nobody can prevent us write data)
    STRUCT_UNLOCK(&structure_guard_mutex);

  // append last block (if it is possible)
  if (last_block_free_space > 0)
  {
    ulong to_copy = min(data_len,last_block_free_space);
    DBUG_PRINT("info", ("use free space %lub at block 0x%lx to copy %lub",
			last_block_free_space, (ulong)last_block, to_copy));
    memcpy((void*)(((byte*)last_block)+last_block->used),
	   (void*)data,to_copy);
    last_block->used+=to_copy;
  }

  DBUG_RETURN(success);
}

my_bool Query_cache::write_result_data(Query_cache_block * &result_block,
				       ulong data_len, gptr data,
				       Query_cache_block * query_block,
				       Query_cache_block::block_type type)
{
  DBUG_ENTER("Query_cache::write_result_data");
  DBUG_PRINT("info", ("data_len %lu",data_len));

  //reserve block(s) for filling
  my_bool success = allocate_data_chain(result_block, data_len, query_block);
  if (success)
  {
    //it is success (nobody can prevent us write data)
    STRUCT_UNLOCK(&structure_guard_mutex);
    byte * rest = (byte*) data;
    Query_cache_block * block = result_block;
    uint headers_len = ALIGN_SIZE(sizeof(Query_cache_block)) +
      ALIGN_SIZE(sizeof(Query_cache_result));
    // now fill list of blocks that created by allocate_data_chain
    do
    {
      block->type = type;
      ulong length = block->used - headers_len;
      DBUG_PRINT("info", ("write %lu byte in block 0x%lx",length,
			  (ulong)block));
      memcpy((void*)(((byte*)block)+headers_len),
	     (void*)rest,length);

      rest += length;
      block = block->next;
      type = Query_cache_block::RES_CONT;
    } while (block != result_block);
  }
  else
  {
    if (result_block != 0)
    {
      // destroy list of blocks that created & locked by lock_result_data
      Query_cache_block * block = result_block;
      do
      {
	Query_cache_block * current = block;
	block = block->next;
	free_memory_block(current);
      } while (block != result_block);
      result_block = 0;
      /*
       it is not success => not unlock structure_guard_mutex (we need it to
       free query)
      */
    }
  }
  DBUG_PRINT("info", ("success %d", (int) success));
  DBUG_RETURN(success);
}

my_bool Query_cache::allocate_data_chain(Query_cache_block * &result_block,
					 ulong data_len,
					 Query_cache_block * query_block)
{
  DBUG_ENTER("Query_cache::allocate_data_chain");

  ulong all_headers_len = ALIGN_SIZE(sizeof(Query_cache_block)) +
    ALIGN_SIZE(sizeof(Query_cache_result));
  ulong len = data_len + all_headers_len;
  DBUG_PRINT("info", ("data_len %lu, all_headers_len %lu",
		      data_len, all_headers_len));

  result_block = allocate_block(max(min_result_data_size,len),
				min_result_data_size == 0,
				all_headers_len + min_result_data_size,
				1);
  my_bool success = (result_block != 0);
  if (success)
  {
    result_block->n_tables = 0;
    result_block->used = 0;
    result_block->type = Query_cache_block::RES_INCOMPLETE;
    result_block->next = result_block->prev = result_block;
    Query_cache_result * header = result_block->result();
    header->parent(query_block);

    Query_cache_block * next_block = 0;
    if (result_block->length < len)
    {
      /*
	allocated less memory then we need (no big memory blocks) =>
	to be continue
      */
      Query_cache_block * next_block;
      if ((success = allocate_data_chain(next_block,
					 len - result_block->length,
					 query_block)))
	double_linked_list_join(result_block, next_block);
    }
    if (success)
    {
      result_block->used = min(len, result_block->length);

      DBUG_PRINT("info", ("Block len %lu used %lu",
			  result_block->length, result_block->used));
    }
    else
      DBUG_PRINT("warning", ("Can't allocate block for continue"));
  }
  else
    DBUG_PRINT("warning", ("Can't allocate block for results"));
  DBUG_RETURN(success);
}

/*****************************************************************************
 *    tables management
 *****************************************************************************/

void Query_cache::invalidate_table(TABLE_LIST *table_list)
{
  if (table_list->table != 0)
    invalidate_table(table_list->table);
  else
  {
    char key[MAX_DBKEY_LENGTH], *key_ptr;
    uint key_length;
    Query_cache_block *table_block;
    key_length=(uint) (strmov(strmov(key,table_list->db)+1,
			      table_list->real_name) -key)+ 1;
    key_ptr = key;

    // we dont store temporary tables => no key_length+=4 ...
    if ((table_block = (Query_cache_block*)
	 hash_search(&tables,key_ptr,key_length)))
      invalidate_table(table_block);
  }
}

void Query_cache::invalidate_table(TABLE *table)
{
  Query_cache_block *table_block;
  if ((table_block = (Query_cache_block*)
       hash_search(&tables,table->table_cache_key,table->key_length)))
    invalidate_table(table_block);
}

void Query_cache::invalidate_table_in_db(Query_cache_block *table_block,
					 char * db)
{
  /*
    table key consist of data_base_name + '\0' + table_name +'\0'...
    => we may use strcmp.
  */
  if (strcmp(db, (char*)(table_block->table()->db()))==0)
  {
    invalidate_table(table_block);
  }
}

void Query_cache::invalidate_table(Query_cache_block *table_block)
{
  Query_cache_block_table * list_root =  table_block->table(0);
  while(list_root->next != list_root)
  {
    Query_cache_block * query_block = list_root->next->block();
    BLOCK_LOCK_WR(query_block);
    free_query(query_block);
  }
}

my_bool Query_cache::register_all_tables(Query_cache_block * block,
					 TABLE_LIST * tables_used,
					 TABLE_COUNTER_TYPE tables)
{
  DBUG_PRINT("info", ("register tables block 0x%lx, n %d, header %x",
		      (ulong)block, (int) tables,
		      (int)ALIGN_SIZE(sizeof(Query_cache_block)) ));

  TABLE_COUNTER_TYPE n = 0;
  TABLE_LIST * i = tables_used;
  for(; i != 0; i=i->next, n++)
  {
    DBUG_PRINT("info",
	       ("table %s, db %s, openinfo at 0x%lx, keylen %u, key at 0x%lx",
		i->real_name, i->db, (ulong) i->table,
		i->table->key_length,
		(ulong) i->table->table_cache_key));
    Query_cache_block_table * block_table = block->table(n);
    block_table->n=n;
    if (!insert_table(i->table->key_length,
		      i->table->table_cache_key, block_table,
		      Query_cache_table::type_convertion(i->table->db_type)))
      break;
    if (i->table->db_type == DB_TYPE_MRG_MYISAM)
    {
      ha_myisammrg * handler = (ha_myisammrg *)i->table->file;
      MYRG_INFO *file = handler->myrg_info();
      MYRG_TABLE *table = file->open_tables;
      for(;table != file->end_table ; table++)
      {
	char key[MAX_DBKEY_LENGTH];
	uint key_length =
	  Query_cache::filename_2_table_key(key, table->table->filename);
	n++;
	Query_cache_block_table * block_table = block->table(n);
	block_table->n=n;
	if (!insert_table(key_length, key, block_table,
			  Query_cache_table::type_convertion(DB_TYPE_MYISAM)))
	  goto err;
      }
    }
  }
err:
  if (i != 0)
  {
    n--;
    DBUG_PRINT("info", ("filed at table %d", (int)n));
    TABLE_COUNTER_TYPE idx = 0;
    for(i = tables_used;
	idx < n;
	idx++)
    {
      unlink_table(block->table(n));
    }
  }
  return(i == 0);
}

my_bool Query_cache::insert_table(uint key_len, char * key,
				  Query_cache_block_table * node,
				  Query_cache_table::query_cache_table_type
				  type)
{
  DBUG_ENTER("Query_cache::insert_table");
  DBUG_PRINT("info", ("insert table node 0x%lx, len %d",
		      (ulong)node, key_len));
  Query_cache_block *table_block = (Query_cache_block *)
    hash_search(&tables, key, key_len);

  if (table_block == 0)
  {
    DBUG_PRINT("info", ("new table block from 0x%lx (%u)",
			(ulong)key, (int) key_len));
    table_block = write_block_data(key_len, (gptr) key,
				   ALIGN_SIZE(sizeof(Query_cache_table)),
				   Query_cache_block::TABLE,
				   1, 1);
    if (table_block == 0)
    {
      DBUG_PRINT("info", ("Can't write table name to cache"));
      DBUG_RETURN(0);
    }
    Query_cache_table * header = table_block->table();
    header->type(type);
    double_linked_list_simple_include(table_block,
				      tables_blocks[type]);
    Query_cache_block_table * list_root = table_block->table(0);
    list_root->n = 0;
    list_root->next = list_root->prev = list_root;
    if (hash_insert(&tables, (const byte *)table_block))
    {
      DBUG_PRINT("info", ("Can't insert table to hash"));
      // write_block_data return locked block
      free_memory_block(table_block);
      DBUG_RETURN(0);
    }
    char * db = header->db();
    header->table(db + strlen(db) + 1);
  }

  Query_cache_block_table * list_root = table_block->table(0);
  node->next = list_root->next; list_root->next = node;
  node->next->prev = node; node->prev = list_root;
  node->parent = table_block->table();
  DBUG_RETURN(1);
}

void Query_cache::unlink_table(Query_cache_block_table * node)
{
  node->prev->next = node->next; node->next->prev = node->prev;
  Query_cache_block_table * neighbour = node->next;
  if (neighbour->next == neighbour)
  {
    // list is empty (neighbour is root of list)
    Query_cache_block * table_block = neighbour->block();
    double_linked_list_exclude(table_block,
			       tables_blocks[table_block->table()->type()]);
    hash_delete(&tables,(byte *) table_block);
    free_memory_block(table_block);
  }
}

/*****************************************************************************
 *    free memory management
 *****************************************************************************/

Query_cache_block * Query_cache::allocate_block(ulong len,
						my_bool not_less,
						ulong min,
						my_bool under_guard)
{
  DBUG_ENTER("Query_cache::allocate_n_lock_block");
  DBUG_PRINT("info", ("len %lu, not less %d, min %lu, uder_guard %d",
		      len, not_less,min,under_guard));

  if (len >= min(query_cache_size, query_cache_limit))
  {
    DBUG_PRINT("info", ("Query cache hase only %lu memory and limit %lu",
			query_cache_size, query_cache_limit));
    DBUG_RETURN(0); // in any case we don't have such piece of memory
  }

  if (!under_guard){STRUCT_LOCK(&structure_guard_mutex);};

  Query_cache_block *block = get_free_block(len, not_less, min);
  while( block == 0 && free_old_query())
  {
    block = get_free_block(len, not_less, min);
  }

  if (block!=0)
  {
    if (block->length > ALIGN_SIZE(len) + min_allocation_unit)
      split_block(block,ALIGN_SIZE(len));
  }

  if (!under_guard){STRUCT_UNLOCK(&structure_guard_mutex);};
  DBUG_RETURN(block);
}

Query_cache_block * Query_cache::get_free_block (ulong len,
							my_bool not_less,
							ulong min)
{
  DBUG_ENTER("Query_cache::get_free_block");
  Query_cache_block *block = 0, *first = 0;
  DBUG_PRINT("info",("length %lu, not_less %d, min %lu", len,
		     (int)not_less, min));

  /* find block with minimal size > len  */

  uint start = find_bin(len);
  // try matching bin
  if (bins[start].number != 0)
  {
    Query_cache_block * list = bins[start].free_blocks;
    first = list;
    while(first->next != list && first->length < len)
    {
      first=first->next;
    }
    if (first->length >= len)
      block=first;
  }
  if (block == 0 && start > 0)
  {
    DBUG_PRINT("info",("try bins whith more bigger blocks"));
    //try more big bins
    int i = start - 1;
    while(i > 0 && bins[i].number == 0)
      i--;
    if (bins[i].number > 0)
      block = bins[i].free_blocks;
  }
  // if no big blocks => try less size (if it is possible)
  if (block == 0 && ! not_less)
  {
    DBUG_PRINT("info",("try smaller blocks"));
    if (first != 0 && first->length > min)
      block = first;
    else
    {
      uint i = start + 1;
      while( i < mem_bin_num && bins[i].number == 0)
	i++;
      if (i < mem_bin_num && bins[i].number > 0 &&
	  bins[i].free_blocks->prev->length >= min)
	block = bins[i].free_blocks->prev;
    }
  }
  if (block != 0)
    exclude_from_free_memory_list(block);

  DBUG_PRINT("info",("getting block 0x%lx", (ulong) block));
  DBUG_RETURN(block);
}

void Query_cache::free_memory_block(Query_cache_block * block)
{
  DBUG_ENTER("Query_cache::free_n_unlock_memory_block");
  block->used=0;
  DBUG_PRINT("info",("first_block 0x%lx, block 0x%lx, pnext 0x%lx pprev 0x%lx",
		     (ulong) first_block, (ulong) block,block->pnext,
		     (ulong) block->pprev));
  if (block->pnext != first_block && block->pnext->is_free())
  {
    block = join_free_blocks(block, block->pnext);
  }
  if (block != first_block && block->pprev->is_free())
  {
    block = join_free_blocks(block->pprev, block->pprev);
  }
  insert_into_free_memory_list(block);
  DBUG_VOID_RETURN;
}

void Query_cache::split_block(Query_cache_block * block,ulong len)
{
  DBUG_ENTER("Query_cache::split_block");
  Query_cache_block * new_block = (Query_cache_block *)(((byte*)block)+len);

  new_block->init(block->length - len);
  block->length=len;
  new_block->pnext = block->pnext; block->pnext = new_block;
  new_block->pprev = block; new_block->pnext->pprev = new_block;

  insert_into_free_memory_list(new_block);

  DBUG_PRINT("info", ("split 0x%lx (%lu) new 0x%lx",
		      (ulong) block, len, (ulong) new_block));
  DBUG_VOID_RETURN;
}

Query_cache_block *
Query_cache::join_free_blocks(Query_cache_block *first_block,
			      Query_cache_block * block_in_list)
{
  DBUG_ENTER("Query_cache::join_free_blocks");
  DBUG_PRINT("info",
	     ("join first 0x%lx, pnext 0x%lx, in list 0x%lx",
	      (ulong) first_block, (ulong) first_block->pnext,
	      (ulong) block_in_list));
  exclude_from_free_memory_list(block_in_list);

  Query_cache_block * second_block = first_block->pnext;
  // may be was not free block
  second_block->type = Query_cache_block::FREE;second_block->used=0;
  second_block->destroy();

  first_block->length += second_block->length;
  first_block->pnext = second_block->pnext;
  second_block->pnext->pprev = first_block;

  DBUG_RETURN(first_block);
}

my_bool Query_cache::append_next_free_block(Query_cache_block * block,
					    ulong add_size)
{
  DBUG_ENTER("Query_cache::append_next_free_block");
  DBUG_PRINT("enter", ("block 0x%lx, add_size %lu", (ulong) block,
		       add_size));
  Query_cache_block * next_block = block->pnext;
  if (next_block->is_free())
  {
    exclude_from_free_memory_list(next_block);
    ulong old_len = block->length;

    next_block->destroy();

    block->length += next_block->length;
    block->pnext = next_block->pnext;
    next_block->pnext->pprev = block;

    if (block->length >
	ALIGN_SIZE(old_len + add_size) + min_allocation_unit)
      split_block(block,ALIGN_SIZE(old_len + add_size));
    DBUG_PRINT("exit", ("block was appended"));
    DBUG_RETURN(1);
  }
  else
  {
    DBUG_PRINT("exit", ("block was not appended"));
    DBUG_RETURN(0);
  }
}

void Query_cache::exclude_from_free_memory_list(Query_cache_block * free_block)
{
  DBUG_ENTER("Query_cache::exclude_from_free_memory_list");
  Query_cache_memory_bin * bin = *((Query_cache_memory_bin **)
    free_block->data());
  double_linked_list_exclude(free_block ,bin->free_blocks);
  bin->number--;
  free_memory-=free_block->length;
  DBUG_PRINT("info",("exclude block 0x%lx, bin 0x%lx", (ulong) free_block,
		     (ulong)bin));
  DBUG_VOID_RETURN;
}

void Query_cache::insert_into_free_memory_list(Query_cache_block * free_block)
{
  DBUG_ENTER("Query_cache::insert_into_free_memory_list");
  uint idx = find_bin(free_block->length);
  insert_into_free_memory_sorted_list(free_block, bins[idx].free_blocks);
  /*
    we have enough memory in block for storing bin reference due to
    min_allocation_unit choice
  */
  Query_cache_memory_bin * *bin_ptr = (Query_cache_memory_bin**)
    free_block->data();
  *bin_ptr = &(bins[idx]);
  bins[idx].number++;
  DBUG_PRINT("info",("insert block 0x%lx, bin[%d] 0x%lx",
		     (ulong) free_block, idx,
		     (ulong) &(bins[idx])));
  DBUG_VOID_RETURN;
}

uint Query_cache::find_bin(ulong size)
{
  DBUG_ENTER("Query_cache::find_bin");
  //begin small blocks to big (small blocks frequently asked)
  int i = mem_bin_steps - 1;
  for(; i > 0 && steps[i-1].size < size; i--);
  if (i == 0)
  {
    // first bin not subordinate of common rules
    DBUG_PRINT("info", ("first bin (# 0), size %lu",size));
    DBUG_RETURN(0);
  }
  uint bin =  steps[i].idx - (uint)((size - steps[i].size)/steps[i].increment);
  DBUG_PRINT("info", ("bin %u step %u, size %lu", bin, i, size));
  DBUG_RETURN(bin);
}

/*****************************************************************************
 *   lists management
 *****************************************************************************/

void Query_cache::move_to_query_list_end(Query_cache_block * query_block)
{
  DBUG_ENTER("Query_cache::move_to_query_list_end");
  double_linked_list_exclude(query_block, queries_blocks);
  double_linked_list_simple_include(query_block, queries_blocks);
  DBUG_VOID_RETURN;
}

void Query_cache::insert_into_free_memory_sorted_list(Query_cache_block *
						      new_block,
						      Query_cache_block *
						      &list)
{
  DBUG_ENTER("Query_cache::insert_into_free_memory_sorted_list");
  /*
     list sorted by size in ascendant order, because we need small blocks
     frequently than big one
  */

  new_block->used = 0;
  new_block->n_tables = 0;
  new_block->type = Query_cache_block::FREE;

  if (list == 0)
  {
    list = new_block->next=new_block->prev=new_block;
    DBUG_PRINT("info", ("inserted into empty list"));
  }
  else
  {
    Query_cache_block *point = list;
    if (point->length >= new_block->length)
    {
      point = point->prev;
      list = new_block;
    }
    else
    {
      while(point->next != list &&
	    point->next->length < new_block->length)
      {
	point=point->next;
      }
    }
    new_block->prev = point; new_block->next = point->next;
    new_block->next->prev = new_block; point->next = new_block;
  }
  free_memory+=new_block->length;
  DBUG_VOID_RETURN;
}


void
Query_cache::double_linked_list_simple_include(Query_cache_block * point,
						Query_cache_block *
						&list_pointer)
{
  DBUG_ENTER("Query_cache::double_linked_list_simple_include");
  DBUG_PRINT("info", ("including block 0x%lx", (ulong) point));
  if (list_pointer == 0)
    list_pointer=point->next=point->prev=point;
  else
  {
    point->next = list_pointer;
    point->prev = list_pointer->prev;
    point->prev->next = point;
    list_pointer->prev = point;
    list_pointer = point;
  }
  DBUG_VOID_RETURN;
}

void
Query_cache::double_linked_list_exclude(Query_cache_block *point,
					Query_cache_block * &list_pointer)
{
  DBUG_ENTER("Query_cache::double_linked_list_exclude");
  DBUG_PRINT("info", ("excluding block 0x%lx, list 0x%lx",
		      (ulong) point, (ulong) list_pointer));
  if (point->next == point)
    list_pointer = 0; // empty list
  else
  {
    point->next->prev = point->prev;
    point->prev->next = point->next;
    if (point == list_pointer) list_pointer = point->next;
  }
  DBUG_VOID_RETURN;
}

void Query_cache::double_linked_list_join(Query_cache_block *head_tail,
					  Query_cache_block *tail_head)
{
  Query_cache_block *head_head = head_tail->next,
    *tail_tail	= tail_head->prev;
  head_head->prev = tail_tail;
  head_tail->next = tail_head;
  tail_head->prev = head_tail;
  tail_tail->next = head_head;
}

/*****************************************************************************
 *    query
 *****************************************************************************/

/*
  if query is cachable return numder tables in query
  (query whithout tables tot chached)
*/
TABLE_COUNTER_TYPE Query_cache::is_cachable(THD *thd,
					    uint query_len, char *query,
					    LEX *lex, TABLE_LIST* tables_used)
{
  TABLE_COUNTER_TYPE tables = 0;
  DBUG_ENTER("Query_cache::is_cachable");
  if (lex->sql_command == SQLCOM_SELECT &&
      thd->temporary_tables == 0 &&
      (thd->query_cache_type == 1 || (thd->query_cache_type == 2 &&
				      (lex->select->options &
				       OPTION_TO_QUERY_CACHE))) &&
      thd->safe_to_cache_query)
  {
    DBUG_PRINT("info", ("options %lx %lx, type %u",
			OPTION_TO_QUERY_CACHE,
			lex->select->options,
			(int) thd->query_cache_type));
    my_bool has_transactions = 0;
    TABLE_LIST * i = tables_used;
    for(; i != 0; i=i->next)
    {
      tables++;
      DBUG_PRINT("info", ("table %s, db %s, type %u", i->real_name,
			  i->db, i->table->db_type));
      has_transactions = (has_transactions ||
			  i->table->file->has_transactions());
      if (i->table->db_type ==	DB_TYPE_MRG_ISAM)
      {
	DBUG_PRINT("info", ("select not cachable: used MRG_ISAM table(s)"));
	DBUG_RETURN(0);
      }
      if (i->table->db_type == DB_TYPE_MRG_MYISAM)
      {
	ha_myisammrg * handler = (ha_myisammrg *)i->table->file;
	MYRG_INFO *file = handler->myrg_info();
	MYRG_TABLE *table = file->open_tables;
	for(;table != file->end_table ; table++)
	{
	  tables++;
	  DBUG_PRINT("loop", (" + 1 table (mrg_MyISAM)"));
	}
      }
    }

    if ((thd->options & (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN)) &&
	has_transactions)
    {
      DBUG_PRINT("info", ("not in autocommin mode"));
      DBUG_RETURN(0);
    }
    else
    {
      DBUG_PRINT("info", ("select have %d tables", tables));
      DBUG_RETURN(tables);
    }
  }
  DBUG_PRINT("info",
	     ("not interest query: %d or not cachable, \
options %lx %lx, type %u",
	      (int) lex->sql_command,
	      OPTION_TO_QUERY_CACHE,
	      lex->select->options,
	      (int) thd->query_cache_type));
  DBUG_RETURN(0);
}

/*****************************************************************************
 *    packing
 *****************************************************************************/

void Query_cache::pack_cache()
{
  DBUG_ENTER("Query_cache::pack_cache");
  STRUCT_LOCK(&structure_guard_mutex);
  DUMP(this);
  byte * border = 0;
  Query_cache_block * before = 0;
  ulong gap = 0;
  my_bool ok = 1;
  Query_cache_block * i = first_block;
  do
  {
    ok = move_by_type(border, before, gap, i);
    i = i->pnext;
  } while (ok && i != first_block);
  if (border != 0)
  {
    Query_cache_block * new_block = (Query_cache_block *) border;
    new_block->init(gap);
    new_block->pnext = before->pnext; before->pnext = new_block;
    new_block->pprev = before; new_block->pnext->pprev = new_block;
    insert_into_free_memory_list(new_block);
  }
  DUMP(this);
  STRUCT_UNLOCK(&structure_guard_mutex);
  DBUG_VOID_RETURN;
}

my_bool Query_cache::move_by_type(byte * &border,
  Query_cache_block * &before, ulong &gap, Query_cache_block * i)
{
  DBUG_ENTER("Query_cache::move_by_type");
  my_bool ok = 1;
  switch(i->type)
  {
  case Query_cache_block::FREE:
    {
      DBUG_PRINT("info", ("block 0x%lx FREE", (ulong) i));
      if (border == 0)
      {
	border = (byte *) i;
	before = i->pprev;
	DBUG_PRINT("info", ("gap begining here"));
      }
      exclude_from_free_memory_list(i);
      gap +=i->length;
      i->pprev->pnext=i->pnext;
      i->pnext->pprev=i->pprev;
      i->destroy();
      DBUG_PRINT("info", ("added to gap (%lu)", gap));
      break;
    }
  case Query_cache_block::TABLE:
    {
      DBUG_PRINT("info", ("block 0x%lx TABLE", (ulong) i));
      if (border == 0) break;
      ulong len = i->length,
	used = i->used;
      Query_cache_block_table * list_root = i->table(0);
      Query_cache_block_table
	*tprev = list_root->prev,
	*tnext = list_root->next;
      Query_cache_block
	*prev = i->prev,
	*next = i->next,
	*pprev = i->pprev,
	*pnext = i->pnext,
	*new_block =(Query_cache_block *) border;
      char * data = (char*)i->data();
      hash_delete(&tables, (byte *)i);
      i->destroy();
      new_block->init(len);
      new_block->type=Query_cache_block::TABLE;
      new_block->used=used;
      new_block->n_tables=1;
      memcpy((char*)new_block->data(), data,
	     len-new_block->headers_len());
      relink(i, new_block, next, prev, pnext, pprev);
      if (tables_blocks[new_block->table()->type()] == i)
      {
	tables_blocks[new_block->table()->type()] = new_block;
      }
      Query_cache_block_table * nlist_root = new_block->table(0);
      nlist_root->n = 0;
      nlist_root->next = (tnext==list_root?nlist_root:tnext);
      nlist_root->prev = (tprev==list_root?nlist_root:tnext);
      tnext->prev = list_root;
      tprev->next = list_root;
      border += len;
      before = new_block;
      hash_insert(&tables, (const byte *)new_block);
      DBUG_PRINT("info", ("moved %lu bytes to 0x%lx, new gap at 0x%lx",
			  len, (ulong) new_block, (ulong) border));
      break;
    }
  case Query_cache_block::QUERY:
    {
      DBUG_PRINT("info", ("block 0x%lx QUERY", (ulong) i));
      if (border == 0) break;
      BLOCK_LOCK_WR(i);
      ulong len = i->length,
	used = i->used;
      TABLE_COUNTER_TYPE n_tables = i->n_tables;
      Query_cache_block
	*prev = i->prev,
	*next = i->next,
	*pprev = i->pprev,
	*pnext = i->pnext,
	*new_block =(Query_cache_block *) border;
      char * data = (char*)i->data();
      Query_cache_block * first_result_block = ((Query_cache_query *)
						i->data())->result();
      hash_delete(&queries, (byte *)i);
      memcpy((char*)new_block->table(0), (char*)i->table(0),
	     n_tables * ALIGN_SIZE(sizeof(Query_cache_block_table)));
      i->query()->unlock_n_destroy();
      i->destroy();
      new_block->init(len);
      new_block->type=Query_cache_block::QUERY;
      new_block->used=used;
      new_block->n_tables=n_tables;
      memcpy((char*)new_block->data(), data,
	     len - new_block->headers_len());
      relink(i, new_block, next, prev, pnext, pprev);
      if (queries_blocks == i)
	queries_blocks = new_block;
      TABLE_COUNTER_TYPE j=0;
      for(; j< n_tables; j++)
      {
	Query_cache_block_table * block_table = new_block->table(j);
	block_table->next->prev = block_table;
	block_table->prev->next = block_table;
      }
      DBUG_PRINT("info", ("after cicle tt"));
      border += len;
      before = new_block;
      new_block->query()->result(first_result_block);
      if (first_result_block != 0)
      {
	Query_cache_block * result_block = first_result_block;
	do
	{
	  result_block->result()->parent(new_block);
	  result_block = result_block->next;
	} while ( result_block != first_result_block );
      }
      NET * net = new_block->query()->writer();
      if (net != 0)
      {
	net->query_cache_query = (gptr) new_block;
      }
      hash_insert(&queries, (const byte *)new_block);
      DBUG_PRINT("info", ("moved %lu bytes to 0x%lx, new gap at 0x%lx",
			  len, (ulong) new_block, (ulong) border));
      break;
    }
  case Query_cache_block::RES_INCOMPLETE:
  case Query_cache_block::RES_BEG:
  case Query_cache_block::RES_CONT:
  case Query_cache_block::RESULT:
    {
      DBUG_PRINT("info", ("block 0x%lx RES* (%d)", (ulong) i, (int)i->type));
      if (border == 0) break;
      Query_cache_block
	*query_block = i->result()->parent(),
	*next = i->next,
	*prev = i->prev;
      Query_cache_block::block_type type = i->type;
      BLOCK_LOCK_WR(query_block);
      ulong len = i->length,
	used = i->used;
      Query_cache_block
	*pprev = i->pprev,
	*pnext = i->pnext,
	*new_block =(Query_cache_block *) border;
      char * data = (char*)i->data();
      i->destroy();
      new_block->init(len);
      new_block->type=type;
      new_block->used=used;
      memcpy((char*)new_block->data(), data,
	     len - new_block->headers_len());
      relink(i, new_block, next, prev, pnext, pprev);
      new_block->result()->parent(query_block);
      Query_cache_query * query = query_block->query();
      if (query->result() == i)
	query->result(new_block);
      border += len;
      before = new_block;
      /* if result writing complete && we have free space in block */
      ulong free_space = new_block->length - new_block->used;
      if (query->result()->type == Query_cache_block::RESULT &&
	  new_block->length > new_block->used &&
	  gap + free_space > min_allocation_unit &&
	  new_block->length - free_space > min_allocation_unit)
      {
	border -= free_space;
	gap += free_space;
	new_block->length -= free_space;
      }
      BLOCK_UNLOCK_WR(query_block);
      DBUG_PRINT("info", ("moved %lu bytes to 0x%lx, new gap at 0x%lx",
			  len, (ulong) new_block, (ulong) border));
      break;
    }
  default:
    DBUG_PRINT("error", ("unexpectet block type %d, block 0x%lx",
			 (int)i->type, (ulong) i));
    ok = 0;
  }
  DBUG_RETURN(ok);
}

void Query_cache::relink(Query_cache_block * oblock,
			 Query_cache_block * nblock,
			 Query_cache_block * next, Query_cache_block * prev,
			 Query_cache_block * pnext, Query_cache_block * pprev)
{
  nblock->prev = (prev==oblock?nblock:prev); //check pointer to himself
  nblock->next = (next==oblock?nblock:next);
  prev->next=nblock;
  next->prev=nblock;
  nblock->pprev = pprev; //physical pointer to himself have only 1 free block
  nblock->pnext = pnext;
  pprev->pnext=nblock;
  pnext->pprev=nblock;
}

my_bool Query_cache::join_results(ulong join_limit)
{
  //TODO
  DBUG_ENTER("Query_cache::join_results");
  my_bool has_moving = 0;
  Query_cache_block *query_block = 0;
  STRUCT_LOCK(&structure_guard_mutex);
  if (queries_blocks != 0)
  {
    Query_cache_block *i = queries_blocks;
    do
    {
      Query_cache_query * header = i->query();
      if (header->result() != 0 &&
	  header->result()->type == Query_cache_block::RESULT &&
	  header->length() > join_limit)
      {
	Query_cache_block *new_result_block =
	  get_free_block(header->length() +
			 ALIGN_SIZE(sizeof(Query_cache_block)) +
			 ALIGN_SIZE(sizeof(Query_cache_result)), 1, 0);
	if (new_result_block != 0)
	{
	  has_moving = 1;
	  Query_cache_block *first_result = header->result();
	  ulong new_len = header->length() +
	    ALIGN_SIZE(sizeof(Query_cache_block)) +
	    ALIGN_SIZE(sizeof(Query_cache_result));
	  if (new_result_block->length >
	      ALIGN_SIZE(new_len) + min_allocation_unit)
	    split_block(new_result_block,ALIGN_SIZE(new_len));
	  BLOCK_LOCK_WR(i);
	  header->result(new_result_block);
	  new_result_block->type = Query_cache_block::RESULT;
	  new_result_block->n_tables = 0;
	  new_result_block->used = new_len;

	  new_result_block->next = new_result_block->prev = new_result_block;
	  DBUG_PRINT("info", ("new block %lu/%lu (%lu)",
			      new_result_block->length,
			      new_result_block->used,
			      header->length()));

	  Query_cache_result *new_result = new_result_block->result();
	  new_result->parent(i);
	  byte *write_to = (byte*) new_result->data();
	  Query_cache_block *result_block = first_result;
	  do
	  {
	    ulong len = result_block->used - result_block->headers_len() -
	      ALIGN_SIZE(sizeof(Query_cache_result));
	    DBUG_PRINT("loop", ("add block %lu/%lu (%lu)",
				result_block->length,
				result_block->used,
				len));
	    memcpy((char *) write_to,
		   (char*) result_block->result()->data(),
		   len);
	    write_to += len;
	    Query_cache_block * old_result_block = result_block;
	    result_block = result_block->next;
	    free_memory_block(old_result_block);
	  } while (result_block != first_result);
	  BLOCK_UNLOCK_WR(i);
	}
      }
      i = i->next;
    } while ( i != queries_blocks );
  }
  STRUCT_UNLOCK(&structure_guard_mutex);
  DBUG_RETURN(has_moving);
}

uint Query_cache::filename_2_table_key (char * key, char *filename)
{
  DBUG_ENTER("Query_cache::filename_2_table_key");
  char tablename[FN_REFLEN];
  char dbbuff[FN_REFLEN];
  char *dbname;
  Query_cache_block *table_block;
  fn_format(tablename,filename,"","",3);

  int path_len = (strrchr(filename, '/') - filename);
  strncpy(dbbuff, filename, path_len);
  dbbuff[path_len] = '\0';
  dbname = strrchr(dbbuff, '/') + 1;

  DBUG_PRINT("info", ("table %s.%s", dbname, tablename));

  DBUG_RETURN( (uint) (strmov(strmov(key, dbname) + 1,
			tablename) -key) + 1);
}
