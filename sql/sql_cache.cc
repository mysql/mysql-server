/* Copyright (C) 2000 MySQL AB

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
  Description of the query cache:

1. Query_cache object consists of
	- query cache memory pool (cache)
	- queries hash (queries)
	- tables hash (tables)
	- list of blocks ordered as they allocated in memory
(first_block)
	- list of queries block (queries_blocks)
	- list of used tables (tables_blocks)

2. Query cache memory pool (cache) consists of
	- table of steps of memory bins allocation
	- table of free memory bins
	- blocks of memory

3. Memory blocks

Every memory block has the following structure:

+----------------------------------------------------------+
|      Block header (Query_cache_block structure)	   |
+----------------------------------------------------------+
|Table of database table lists (used for queries & tables) |
+----------------------------------------------------------+
|		  Type depended header			   |
|(Query_cache_query, Query_cache_table, Query_cache_result)|
+----------------------------------------------------------+
|			Data ...			   |
+----------------------------------------------------------+

Block header consists of:
- type:
  FREE		Free memory block
  QUERY		Query block
  RESULT	Ready to send result
  RES_CONT	Result's continuation
  RES_BEG	First block of results, that is not yet complete,
		written to cache
  RES_INCOMPLETE  Allocated for results data block
  TABLE		Block with database table description
  INCOMPLETE	The destroyed block
- length of block (length)
- length of data & headers (used)
- physical list links (pnext/pprev) - used for the list of
  blocks ordered as they are allocated in physical memory
- logical list links (next/prev) - used for queries block list, tables block
  list, free memory block lists and list of results block in query
- number of elements in table of database table list (n_tables)

4. Query & results blocks

Query stored in cache consists of following blocks:

more		      more
recent+-------------+ old
<-----|Query block 1|------> double linked list of queries block
 prev |		    | next
      +-------------+
    <-|  table 0    |-> (see "Table of database table lists" description)
    <-|  table 1    |->
      |  ...	    |		+--------------------------+
      +-------------+	 +-------------------------+	   |
NET   |		    |	 |	V		   V	   |
struct|		    |	 +-+------------+   +------------+ |
<-----|query header |----->|Result block|-->|Result block|-+ doublelinked
writer|		    |result|		|<--|		 |   list of results
      +-------------+	   +------------+   +------------+
      |charset	    |	   +------------+   +------------+ no table of dbtables
      |encoding +   |	   |   result	|   |	result	 |
      |query text   |<-----|   header	|   |	header	 |------+
      +-------------+parent|		|   |		 |parent|
	    ^		   +------------+   +------------+	|
	    |		   |result data |   |result data |	|
	    |		   +------------+   +------------+	|
	    +---------------------------------------------------+

First query is registered. During the registration query block is
allocated. This query block is included in query hash and is linked
with appropriate database tables lists (if there is no appropriate
list exists it will be created).

Later when query has performed results is written into the result blocks.
A result block cannot be smaller then QUERY_CACHE_MIN_RESULT_DATA_SIZE.

When new result is written to cache it is appended to the last result
block, if no more  free space left in the last block, new block is
allocated.

5. Table of database table lists.

For quick invalidation of queries all query are linked in lists on used
database tables basis (when table will be changed (insert/delete/...)
this queries will be removed from cache).

Root of such list is table block:

     +------------+	  list of used tables (used while invalidation of
<----|	Table	  |-----> whole database)
 prev|	block	  |next			     +-----------+
     |		  |	  +-----------+      |Query block|
     |		  |	  |Query block|      +-----------+
     +------------+	  +-----------+      | ...	 |
  +->| table 0	  |------>|table 0    |----->| table N	 |---+
  |+-|		  |<------|	      |<-----|		 |<-+|
  || +------------+	  | ...       |      | ...	 |  ||
  || |table header|	  +-----------+      +-----------+  ||
  || +------------+	  | ...       |      | ...	 |  ||
  || |db name +   |	  +-----------+      +-----------+  ||
  || |table name  |					    ||
  || +------------+					    ||
  |+--------------------------------------------------------+|
  +----------------------------------------------------------+

Table block is included into the tables hash (tables).

6. Free blocks, free blocks bins & steps of freeblock bins.

When we just started only one free memory block  existed. All query
cache memory (that will be used for block allocation) were
containing in this block.
When a new block is allocated we find most suitable memory block
(minimal of >= required size). If such a block can not be found, we try
to find max block < required size (if we allocate block for results).
If there is no free memory, oldest query is removed from cache, and then
we try to allocate memory. Last step should be repeated until we find
suitable block or until there is no unlocked query found.

If the block is found and its length more then we need, it should be
split into 2 blocks.
New blocks cannot be smaller then min_allocation_unit_bytes.

When a block becomes free, its neighbor-blocks should be tested and if
there are free blocks among them, they should be joined into one block.

Free memory blocks are stored in bins according to their sizes.
The bins are stored in size-descending order.
These bins are distributed (by size) approximately logarithmically.

First bin (number 0) stores free blocks with
size <= query_cache_size>>QUERY_CACHE_MEM_BIN_FIRST_STEP_PWR2.
It is first (number 0) step.
On the next step distributed (1 + QUERY_CACHE_MEM_BIN_PARTS_INC) *
QUERY_CACHE_MEM_BIN_PARTS_MUL bins. This bins allocated in interval from
query_cache_size>>QUERY_CACHE_MEM_BIN_FIRST_STEP_PWR2 to
query_cache_size>>QUERY_CACHE_MEM_BIN_FIRST_STEP_PWR2 >>
QUERY_CACHE_MEM_BIN_STEP_PWR2
...
On each step interval decreases in 2 power of
QUERY_CACHE_MEM_BIN_STEP_PWR2
times, number of bins (that distributed on this step) increases. If on
the previous step there were N bins distributed , on the current there
would be distributed
(N + QUERY_CACHE_MEM_BIN_PARTS_INC) * QUERY_CACHE_MEM_BIN_PARTS_MUL
bins.
Last distributed bin stores blocks with size near min_allocation_unit
bytes.

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


Calculation of steps/bins distribution is performed only when query cache
is resized.

When we need to find appropriate bin, first we should find appropriate
step, then we should calculate number of bins that are using data
stored in Query_cache_memory_bin_step structure.

Free memory blocks are sorted in bins in lists with size-ascending order
(more small blocks needed frequently then bigger one).

7. Packing cache.

Query cache packing is divided into two operation:
	- pack_cache
	- join_results

pack_cache moved all blocks to "top" of cache and create one block of free
space at the "bottom":

 before pack_cache    after pack_cache
 +-------------+      +-------------+
 | query 1     |      | query 1     |
 +-------------+      +-------------+
 | table 1     |      | table 1     |
 +-------------+      +-------------+
 | results 1.1 |      | results 1.1 |
 +-------------+      +-------------+
 | free        |      | query 2     |
 +-------------+      +-------------+
 | query 2     |      | table 2     |
 +-------------+ ---> +-------------+
 | table 2     |      | results 1.2 |
 +-------------+      +-------------+
 | results 1.2 |      | results 2   |
 +-------------+      +-------------+
 | free        |      | free        |
 +-------------+      |             |
 | results 2   |      |             |
 +-------------+      |             |
 | free        |      |             |
 +-------------+      +-------------+

pack_cache scan blocks in physical address order and move every non-free
block "higher".

pack_cach remove every free block it finds. The length of the deleted block
is accumulated to the "gap". All non free blocks should be shifted with the
"gap" step.

join_results scans all complete queries. If the results of query are not
stored in the same block, join_results tries to move results so, that they
are stored in one block.

 before join_results  after join_results
 +-------------+      +-------------+
 | query 1     |      | query 1     |
 +-------------+      +-------------+
 | table 1     |      | table 1     |
 +-------------+      +-------------+
 | results 1.1 |      | free        |
 +-------------+      +-------------+
 | query 2     |      | query 2     |
 +-------------+      +-------------+
 | table 2     |      | table 2     |
 +-------------+ ---> +-------------+
 | results 1.2 |      | free        |
 +-------------+      +-------------+
 | results 2   |      | results 2   |
 +-------------+      +-------------+
 | free        |      | results 1   |
 |             |      |             |
 |             |      +-------------+
 |             |      | free        |
 |             |      |             |
 +-------------+      +-------------+

If join_results allocated new block(s) then we need call pack_cache again.

TODO list:

  - Delayed till after-parsing qache answer (for column rights processing)
  - Optimize cache resizing
      - if new_size < old_size then pack & shrink
      - if new_size > old_size copy cached query to new cache
  - Move MRG_MYISAM table type processing to handlers, something like:
        tables_used->table->file->register_used_filenames(callback,
                                                          first_argument);
  - QC improvement suggested by Monty:
    - Add a counter in open_table() for how many MERGE (ISAM or MyISAM)
      tables are cached in the table cache.
      (This will be trivial when we have the new table cache in place I
      have been working on)
    - After this we can add the following test around the for loop in
      is_cacheable::

      if (thd->temp_tables || global_merge_table_count)

    - Another option would be to set thd->lex->safe_to_cache_query to 0
      in 'get_lock_data' if any of the tables was a tmp table or a
      MRG_ISAM table.
      (This could be done with almost no speed penalty)
*/

#include "mysql_priv.h"
#ifdef HAVE_QUERY_CACHE
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include "ha_myisammrg.h"
#ifndef MASTER
#include "../srclib/myisammrg/myrg_def.h"
#else
#include "../storage/myisammrg/myrg_def.h"
#endif

#ifdef EMBEDDED_LIBRARY
#include "emb_qcache.h"
#endif

#if !defined(EXTRA_DBUG) && !defined(DBUG_OFF)
#define MUTEX_LOCK(M) { DBUG_PRINT("lock", ("mutex lock 0x%lx", (ulong)(M))); \
  pthread_mutex_lock(M);}
#define MUTEX_UNLOCK(M) {DBUG_PRINT("lock", ("mutex unlock 0x%lx",\
  (ulong)(M))); pthread_mutex_unlock(M);}
#define RW_WLOCK(M) {DBUG_PRINT("lock", ("rwlock wlock 0x%lx",(ulong)(M))); \
  if (!rw_wrlock(M)) DBUG_PRINT("lock", ("rwlock wlock ok")) \
  else DBUG_PRINT("lock", ("rwlock wlock FAILED %d", errno)); }
#define RW_RLOCK(M) {DBUG_PRINT("lock", ("rwlock rlock 0x%lx", (ulong)(M))); \
  if (!rw_rdlock(M)) DBUG_PRINT("lock", ("rwlock rlock ok")) \
  else DBUG_PRINT("lock", ("rwlock wlock FAILED %d", errno)); }
#define RW_UNLOCK(M) {DBUG_PRINT("lock", ("rwlock unlock 0x%lx",(ulong)(M))); \
  if (!rw_unlock(M)) DBUG_PRINT("lock", ("rwlock unlock ok")) \
  else DBUG_PRINT("lock", ("rwlock unlock FAILED %d", errno)); }
#define STRUCT_LOCK(M) {DBUG_PRINT("lock", ("%d struct lock...",__LINE__)); \
  pthread_mutex_lock(M);DBUG_PRINT("lock", ("struct lock OK"));}
#define STRUCT_UNLOCK(M) { \
  DBUG_PRINT("lock", ("%d struct unlock...",__LINE__)); \
  pthread_mutex_unlock(M);DBUG_PRINT("lock", ("struct unlock OK"));}
#define BLOCK_LOCK_WR(B) {DBUG_PRINT("lock", ("%d LOCK_WR 0x%lx",\
  __LINE__,(ulong)(B))); \
  B->query()->lock_writing();}
#define BLOCK_LOCK_RD(B) {DBUG_PRINT("lock", ("%d LOCK_RD 0x%lx",\
  __LINE__,(ulong)(B))); \
  B->query()->lock_reading();}
#define BLOCK_UNLOCK_WR(B) { \
  DBUG_PRINT("lock", ("%d UNLOCK_WR 0x%lx",\
  __LINE__,(ulong)(B)));B->query()->unlock_writing();}
#define BLOCK_UNLOCK_RD(B) { \
  DBUG_PRINT("lock", ("%d UNLOCK_RD 0x%lx",\
  __LINE__,(ulong)(B)));B->query()->unlock_reading();}
#define DUMP(C) DBUG_EXECUTE("qcache", {\
  (C)->cache_dump(); (C)->queries_dump();(C)->tables_dump();})
#else
#define MUTEX_LOCK(M) pthread_mutex_lock(M)
#define MUTEX_UNLOCK(M) pthread_mutex_unlock(M)
#define RW_WLOCK(M) rw_wrlock(M)
#define RW_RLOCK(M) rw_rdlock(M)
#define RW_UNLOCK(M) rw_unlock(M)
#define STRUCT_LOCK(M) pthread_mutex_lock(M)
#define STRUCT_UNLOCK(M) pthread_mutex_unlock(M)
#define BLOCK_LOCK_WR(B) B->query()->lock_writing()
#define BLOCK_LOCK_RD(B) B->query()->lock_reading()
#define BLOCK_UNLOCK_WR(B) B->query()->unlock_writing()
#define BLOCK_UNLOCK_RD(B) B->query()->unlock_reading()
#define DUMP(C)
#endif

const char *query_cache_type_names[]= { "OFF", "ON", "DEMAND",NullS };
TYPELIB query_cache_type_typelib=
{
  array_elements(query_cache_type_names)-1,"", query_cache_type_names, NULL
};

/*****************************************************************************
 Query_cache_block_table method(s)
*****************************************************************************/

inline Query_cache_block * Query_cache_block_table::block()
{
  return (Query_cache_block *)(((byte*)this) -
			       ALIGN_SIZE(sizeof(Query_cache_block_table)*n) -
			       ALIGN_SIZE(sizeof(Query_cache_block)));
}

/*****************************************************************************
   Query_cache_block method(s)
*****************************************************************************/

void Query_cache_block::init(ulong block_length)
{
  DBUG_ENTER("Query_cache_block::init");
  DBUG_PRINT("qcache", ("init block 0x%lx  length: %lu", (ulong) this,
			block_length));
  length = block_length;
  used = 0;
  type = Query_cache_block::FREE;
  n_tables = 0;
  DBUG_VOID_RETURN;
}

void Query_cache_block::destroy()
{
  DBUG_ENTER("Query_cache_block::destroy");
  DBUG_PRINT("qcache", ("destroy block 0x%lx, type %d",
			(ulong) this, type));
  type = INCOMPLETE;
  DBUG_VOID_RETURN;
}

inline uint Query_cache_block::headers_len()
{
  return (ALIGN_SIZE(sizeof(Query_cache_block_table)*n_tables) +
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
	   n*sizeof(Query_cache_block_table)));
}


/*****************************************************************************
 *   Query_cache_table method(s)
 *****************************************************************************/

extern "C"
{
byte *query_cache_table_get_key(const byte *record, uint *length,
				my_bool not_used __attribute__((unused)))
{
  Query_cache_block* table_block = (Query_cache_block*) record;
  *length = (table_block->used - table_block->headers_len() -
	     ALIGN_SIZE(sizeof(Query_cache_table)));
  return (((byte *) table_block->data()) +
	  ALIGN_SIZE(sizeof(Query_cache_table)));
}
}

/*****************************************************************************
    Query_cache_query methods
*****************************************************************************/

/*
   Following methods work for block read/write locking only in this
   particular case and in interaction with structure_guard_mutex.

   Lock for write prevents any other locking. (exclusive use)
   Lock for read prevents only locking for write.
*/

inline void Query_cache_query::lock_writing()
{
  RW_WLOCK(&lock);
}


/*
  Needed for finding queries, that we may delete from cache.
  We don't want to wait while block become unlocked. In addition,
  block locking means that query is now used and we don't need to
  remove it.
*/

my_bool Query_cache_query::try_lock_writing()
{
  DBUG_ENTER("Query_cache_block::try_lock_writing");
  if (rw_trywrlock(&lock)!=0)
  {
    DBUG_PRINT("info", ("can't lock rwlock"));
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("rwlock 0x%lx locked", (ulong) &lock));
  DBUG_RETURN(1);
}


inline void Query_cache_query::lock_reading()
{
  RW_RLOCK(&lock);
}


inline void Query_cache_query::unlock_writing()
{
  RW_UNLOCK(&lock);
}


inline void Query_cache_query::unlock_reading()
{
  RW_UNLOCK(&lock);
}


void Query_cache_query::init_n_lock()
{
  DBUG_ENTER("Query_cache_query::init_n_lock");
  res=0; wri = 0; len = 0;
  my_rwlock_init(&lock, NULL);
  lock_writing();
  DBUG_PRINT("qcache", ("inited & locked query for block 0x%lx",
			((byte*) this)-ALIGN_SIZE(sizeof(Query_cache_block))));
  DBUG_VOID_RETURN;
}


void Query_cache_query::unlock_n_destroy()
{
  DBUG_ENTER("Query_cache_query::unlock_n_destroy");
  DBUG_PRINT("qcache", ("destroyed & unlocked query for block 0x%lx",
			((byte*)this)-ALIGN_SIZE(sizeof(Query_cache_block))));
  /*
    The following call is not needed on system where one can destroy an
    active semaphore
  */
  this->unlock_writing();
  rwlock_destroy(&lock);
  DBUG_VOID_RETURN;
}


extern "C"
{
byte *query_cache_query_get_key(const byte *record, uint *length,
				my_bool not_used)
{
  Query_cache_block *query_block = (Query_cache_block*) record;
  *length = (query_block->used - query_block->headers_len() -
	     ALIGN_SIZE(sizeof(Query_cache_query)));
  return (((byte *) query_block->data()) +
	  ALIGN_SIZE(sizeof(Query_cache_query)));
}
}

/*****************************************************************************
  Functions to store things into the query cache
*****************************************************************************/

/*
  Insert the packet into the query cache.
  This should only be called if net->query_cache_query != 0
*/

void query_cache_insert(NET *net, const char *packet, ulong length)
{
  DBUG_ENTER("query_cache_insert");

  STRUCT_LOCK(&query_cache.structure_guard_mutex);
  /*
    It is very unlikely that following condition is TRUE (it is possible
    only if other thread is resizing cache), so we check it only after guard
    mutex lock
  */
  if (unlikely(query_cache.query_cache_size == 0))
  {
    STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
    DBUG_VOID_RETURN;
  }

  Query_cache_block *query_block = ((Query_cache_block*)
				    net->query_cache_query);
  if (query_block)
  {
    Query_cache_query *header = query_block->query();
    Query_cache_block *result = header->result();

    DUMP(&query_cache);
    BLOCK_LOCK_WR(query_block);
    DBUG_PRINT("qcache", ("insert packet %lu bytes long",length));

    /*
      On success STRUCT_UNLOCK(&query_cache.structure_guard_mutex) will be
      done by query_cache.append_result_data if success (if not we need
      query_cache.structure_guard_mutex locked to free query)
    */
    if (!query_cache.append_result_data(&result, length, (gptr) packet,
					query_block))
    {
      DBUG_PRINT("warning", ("Can't append data"));
      header->result(result);
      DBUG_PRINT("qcache", ("free query 0x%lx", (ulong) query_block));
      // The following call will remove the lock on query_block
      query_cache.free_query(query_block);
      // append_result_data no success => we need unlock
      STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
      DBUG_VOID_RETURN;
    }
    header->result(result);
    header->last_pkt_nr= net->pkt_nr;
    BLOCK_UNLOCK_WR(query_block);
  }
  else
    STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
  DBUG_EXECUTE("check_querycache",query_cache.check_integrity(0););
  DBUG_VOID_RETURN;
}


void query_cache_abort(NET *net)
{
  DBUG_ENTER("query_cache_abort");

  if (net->query_cache_query != 0)	// Quick check on unlocked structure
  {
    STRUCT_LOCK(&query_cache.structure_guard_mutex);
    /*
      It is very unlikely that following condition is TRUE (it is possible
      only if other thread is resizing cache), so we check it only after guard
      mutex lock
    */
    if (unlikely(query_cache.query_cache_size == 0))
    {
      STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
      DBUG_VOID_RETURN;
    }

    Query_cache_block *query_block = ((Query_cache_block*)
				       net->query_cache_query);
    if (query_block)			// Test if changed by other thread
    {
      DUMP(&query_cache);
      BLOCK_LOCK_WR(query_block);
      // The following call will remove the lock on query_block
      query_cache.free_query(query_block);
    }
    net->query_cache_query=0;
    DBUG_EXECUTE("check_querycache",query_cache.check_integrity(1););
    STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}


void query_cache_end_of_result(THD *thd)
{
  DBUG_ENTER("query_cache_end_of_result");

  if (thd->net.query_cache_query != 0)	// Quick check on unlocked structure
  {
#ifdef EMBEDDED_LIBRARY
    query_cache_insert(&thd->net, (char*)thd, 
		       emb_count_querycache_size(thd));
#endif
    STRUCT_LOCK(&query_cache.structure_guard_mutex);
    /*
      It is very unlikely that following condition is TRUE (it is possible
      only if other thread is resizing cache), so we check it only after guard
      mutex lock
    */
    if (unlikely(query_cache.query_cache_size == 0))
    {
      STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
      DBUG_VOID_RETURN;
    }

    Query_cache_block *query_block = ((Query_cache_block*)
				      thd->net.query_cache_query);
    if (query_block)
    {
      DUMP(&query_cache);
      BLOCK_LOCK_WR(query_block);
      Query_cache_query *header = query_block->query();
      Query_cache_block *last_result_block = header->result()->prev;
      ulong allign_size = ALIGN_SIZE(last_result_block->used);
      ulong len = max(query_cache.min_allocation_unit, allign_size);
      if (last_result_block->length >= query_cache.min_allocation_unit + len)
	query_cache.split_block(last_result_block,len);
      STRUCT_UNLOCK(&query_cache.structure_guard_mutex);

#ifndef DBUG_OFF
      if (header->result() == 0)
      {
	DBUG_PRINT("error", ("end of data whith no result. query '%s'",
			     header->query()));
	query_cache.wreck(__LINE__, "");
	DBUG_VOID_RETURN;
      }
#endif
      header->found_rows(current_thd->limit_found_rows);
      header->result()->type = Query_cache_block::RESULT;
      header->writer(0);
      BLOCK_UNLOCK_WR(query_block);
    }
    else
    {
      // Cache was flushed or resized and query was deleted => do nothing
      STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
    }
    thd->net.query_cache_query=0;
    DBUG_EXECUTE("check_querycache",query_cache.check_integrity(0););
  }
  DBUG_VOID_RETURN;
}

void query_cache_invalidate_by_MyISAM_filename(const char *filename)
{
  query_cache.invalidate_by_MyISAM_filename(filename);
  DBUG_EXECUTE("check_querycache",query_cache.check_integrity(0););
}


/*****************************************************************************
   Query_cache methods
*****************************************************************************/

Query_cache::Query_cache(ulong query_cache_limit_arg,
			 ulong min_allocation_unit_arg,
			 ulong min_result_data_size_arg,
			 uint def_query_hash_size_arg,
			 uint def_table_hash_size_arg)
  :query_cache_size(0),
   query_cache_limit(query_cache_limit_arg),
   queries_in_cache(0), hits(0), inserts(0), refused(0),
   total_blocks(0), lowmem_prunes(0),
   min_allocation_unit(ALIGN_SIZE(min_allocation_unit_arg)),
   min_result_data_size(ALIGN_SIZE(min_result_data_size_arg)),
   def_query_hash_size(ALIGN_SIZE(def_query_hash_size_arg)),
   def_table_hash_size(ALIGN_SIZE(def_table_hash_size_arg)),
   initialized(0)
{
  ulong min_needed= (ALIGN_SIZE(sizeof(Query_cache_block)) +
		     ALIGN_SIZE(sizeof(Query_cache_block_table)) +
		     ALIGN_SIZE(sizeof(Query_cache_query)) + 3);
  set_if_bigger(min_allocation_unit,min_needed);
  this->min_allocation_unit= ALIGN_SIZE(min_allocation_unit);
  set_if_bigger(this->min_result_data_size,min_allocation_unit);
}


ulong Query_cache::resize(ulong query_cache_size_arg)
{
  DBUG_ENTER("Query_cache::resize");
  DBUG_PRINT("qcache", ("from %lu to %lu",query_cache_size,
			query_cache_size_arg));
  DBUG_ASSERT(initialized);
  STRUCT_LOCK(&structure_guard_mutex);
  if (query_cache_size > 0)
    free_cache();
  query_cache_size= query_cache_size_arg;
  ::query_cache_size= init_cache();
  STRUCT_UNLOCK(&structure_guard_mutex);
  DBUG_RETURN(::query_cache_size);
}


ulong Query_cache::set_min_res_unit(ulong size)
{
  if (size < min_allocation_unit)
    size= min_allocation_unit;
  return (min_result_data_size= ALIGN_SIZE(size));
}


void Query_cache::store_query(THD *thd, TABLE_LIST *tables_used)
{
  TABLE_COUNTER_TYPE local_tables;
  ulong tot_length;
  DBUG_ENTER("Query_cache::store_query");
  if (query_cache_size == 0 || thd->locked_tables)
    DBUG_VOID_RETURN;
  uint8 tables_type= 0;

  if ((local_tables= is_cacheable(thd, thd->query_length,
				  thd->query, thd->lex, tables_used,
				  &tables_type)))
  {
    NET *net= &thd->net;
    Query_cache_query_flags flags;
    // fill all gaps between fields with 0 to get repeatable key
    bzero(&flags, QUERY_CACHE_FLAGS_SIZE);
    flags.client_long_flag= test(thd->client_capabilities & CLIENT_LONG_FLAG);
    flags.client_protocol_41= test(thd->client_capabilities &
                                   CLIENT_PROTOCOL_41);
    flags.more_results_exists= test(thd->server_status &
                                    SERVER_MORE_RESULTS_EXISTS);
    flags.pkt_nr= net->pkt_nr;
    flags.character_set_client_num=
      thd->variables.character_set_client->number;
    flags.character_set_results_num=
      (thd->variables.character_set_results ?
       thd->variables.character_set_results->number :
       UINT_MAX);
    flags.collation_connection_num=
      thd->variables.collation_connection->number;
    flags.limit= thd->variables.select_limit;
    flags.time_zone= thd->variables.time_zone;
    flags.sql_mode= thd->variables.sql_mode;
    flags.max_sort_length= thd->variables.max_sort_length;
    flags.group_concat_max_len= thd->variables.group_concat_max_len;
    DBUG_PRINT("qcache", ("long %d, 4.1: %d, more results %d, pkt_nr: %d, \
CS client: %u, CS result: %u, CS conn: %u, limit: %lu, TZ: 0x%lx, \
sql mode: 0x%lx, sort len: %lu, conncat len: %lu",
                          (int)flags.client_long_flag,
                          (int)flags.client_protocol_41,
                          (int)flags.more_results_exists,
                          flags.pkt_nr,
                          flags.character_set_client_num,
                          flags.character_set_results_num,
                          flags.collation_connection_num,
                          flags.limit,
                          (ulong)flags.time_zone,
                          flags.sql_mode,
                          flags.max_sort_length,
                          flags.group_concat_max_len));
    STRUCT_LOCK(&structure_guard_mutex);

    if (query_cache_size == 0)
    {
      STRUCT_UNLOCK(&structure_guard_mutex);
      DBUG_VOID_RETURN;
    }
    DUMP(this);

    if (ask_handler_allowance(thd, tables_used))
    {
      refused++;
      STRUCT_UNLOCK(&structure_guard_mutex);
      DBUG_VOID_RETURN;
    }

    /* Key is query + database + flag */
    if (thd->db_length)
    {
      memcpy(thd->query+thd->query_length+1, thd->db, thd->db_length);
      DBUG_PRINT("qcache", ("database : %s length %u",
			    thd->db, thd->db_length)); 
    }
    else
    {
      DBUG_PRINT("qcache", ("No active database"));
    }
    tot_length= thd->query_length + thd->db_length + 1 +
      QUERY_CACHE_FLAGS_SIZE;
    /*
      We should only copy structure (don't use it location directly)
      because of alignment issue
    */
    memcpy((void *)(thd->query + (tot_length - QUERY_CACHE_FLAGS_SIZE)),
	   &flags, QUERY_CACHE_FLAGS_SIZE);

    /* Check if another thread is processing the same query? */
    Query_cache_block *competitor = (Query_cache_block *)
      hash_search(&queries, (byte*) thd->query, tot_length);
    DBUG_PRINT("qcache", ("competitor 0x%lx", (ulong) competitor));
    if (competitor == 0)
    {
      /* Query is not in cache and no one is working with it; Store it */
      Query_cache_block *query_block;
      query_block= write_block_data(tot_length, (gptr) thd->query,
				    ALIGN_SIZE(sizeof(Query_cache_query)),
				    Query_cache_block::QUERY, local_tables, 1);
      if (query_block != 0)
      {
	DBUG_PRINT("qcache", ("query block 0x%lx allocated, %lu",
			    (ulong) query_block, query_block->used));

	Query_cache_query *header = query_block->query();
	header->init_n_lock();
	if (my_hash_insert(&queries, (byte*) query_block))
	{
	  refused++;
	  DBUG_PRINT("qcache", ("insertion in query hash"));
	  header->unlock_n_destroy();
	  free_memory_block(query_block);
	  STRUCT_UNLOCK(&structure_guard_mutex);
	  goto end;
	}
	if (!register_all_tables(query_block, tables_used, local_tables))
	{
	  refused++;
	  DBUG_PRINT("warning", ("tables list including failed"));
	  hash_delete(&queries, (byte *) query_block);
	  header->unlock_n_destroy();
	  free_memory_block(query_block);
	  STRUCT_UNLOCK(&structure_guard_mutex);
	  goto end;
	}
	double_linked_list_simple_include(query_block, &queries_blocks);
	inserts++;
	queries_in_cache++;
	STRUCT_UNLOCK(&structure_guard_mutex);

	net->query_cache_query= (gptr) query_block;
	header->writer(net);
	header->tables_type(tables_type);
	// init_n_lock make query block locked
	BLOCK_UNLOCK_WR(query_block);
      }
      else
      {
	// We have not enough memory to store query => do nothing
	refused++;
	STRUCT_UNLOCK(&structure_guard_mutex);
	DBUG_PRINT("warning", ("Can't allocate query"));
      }
    }
    else
    {
      // Another thread is processing the same query => do nothing
      refused++;
      STRUCT_UNLOCK(&structure_guard_mutex);
      DBUG_PRINT("qcache", ("Another thread process same query"));
    }
  }
  else if (thd->lex->sql_command == SQLCOM_SELECT)
    statistic_increment(refused, &structure_guard_mutex);

end:
  DBUG_VOID_RETURN;
}


/*
  Check if the query is in the cache. If it was cached, send it
  to the user.

  RESULTS
        1	Query was not cached.
	0	The query was cached and user was sent the result.
	-1	The query was cached but we didn't have rights to use it.
		No error is sent to the client yet.

  NOTE
  This method requires that sql points to allocated memory of size:
  tot_length= query_length + thd->db_length + 1 + QUERY_CACHE_FLAGS_SIZE;
*/

int
Query_cache::send_result_to_client(THD *thd, char *sql, uint query_length)
{
  ulonglong engine_data;
  Query_cache_query *query;
  Query_cache_block *first_result_block, *result_block;
  Query_cache_block_table *block_table, *block_table_end;
  ulong tot_length;
  Query_cache_query_flags flags;
  DBUG_ENTER("Query_cache::send_result_to_client");

  if (query_cache_size == 0 || thd->locked_tables ||
      thd->variables.query_cache_type == 0)
    goto err;

  /* Check that we haven't forgot to reset the query cache variables */
  DBUG_ASSERT(thd->net.query_cache_query == 0);

  if (!thd->lex->safe_to_cache_query)
  {
    DBUG_PRINT("qcache", ("SELECT is non-cacheable"));
    goto err;
  }

  /*
    Test if the query is a SELECT
    (pre-space is removed in dispatch_command).

    First '/' looks like comment before command it is not
    frequently appeared in real lihe, consequently we can
    check all such queries, too.
  */
  if ((my_toupper(system_charset_info, sql[0]) != 'S' || 
       my_toupper(system_charset_info, sql[1]) != 'E' ||
       my_toupper(system_charset_info,sql[2]) !='L') &&
      sql[0] != '/')
  {
    DBUG_PRINT("qcache", ("The statement is not a SELECT; Not cached"));
    goto err;
  }

  STRUCT_LOCK(&structure_guard_mutex);
  if (query_cache_size == 0)
  {
    DBUG_PRINT("qcache", ("query cache disabled"));
    goto err_unlock;
  }
  Query_cache_block *query_block;

  tot_length= query_length + thd->db_length + 1 + QUERY_CACHE_FLAGS_SIZE;
  if (thd->db_length)
  {
    memcpy(sql+query_length+1, thd->db, thd->db_length);
    DBUG_PRINT("qcache", ("database: '%s' length %u",
			  thd->db, thd->db_length));
  }
  else
  {
    DBUG_PRINT("qcache", ("No active database"));
  }

  // fill all gaps between fields with 0 to get repeatable key
  bzero(&flags, QUERY_CACHE_FLAGS_SIZE);
  flags.client_long_flag= test(thd->client_capabilities & CLIENT_LONG_FLAG);
  flags.client_protocol_41= test(thd->client_capabilities &
                                 CLIENT_PROTOCOL_41);
  flags.more_results_exists= test(thd->server_status &
                                  SERVER_MORE_RESULTS_EXISTS);
  flags.pkt_nr= thd->net.pkt_nr;
  flags.character_set_client_num= thd->variables.character_set_client->number;
  flags.character_set_results_num=
    (thd->variables.character_set_results ?
     thd->variables.character_set_results->number :
     UINT_MAX);
  flags.collation_connection_num= thd->variables.collation_connection->number;
  flags.limit= thd->variables.select_limit;
  flags.time_zone= thd->variables.time_zone;
  flags.sql_mode= thd->variables.sql_mode;
  flags.max_sort_length= thd->variables.max_sort_length;
  flags.group_concat_max_len= thd->variables.group_concat_max_len;
  DBUG_PRINT("qcache", ("long %d, 4.1: %d, more results %d, pkt_nr: %d, \
CS client: %u, CS result: %u, CS conn: %u, limit: %lu, TZ: 0x%lx, \
sql mode: 0x%lx, sort len: %lu, conncat len: %lu",
                          (int)flags.client_long_flag,
                          (int)flags.client_protocol_41,
                          (int)flags.more_results_exists,
                          flags.pkt_nr,
                          flags.character_set_client_num,
                          flags.character_set_results_num,
                          flags.collation_connection_num,
                          flags.limit,
                          (ulong)flags.time_zone,
                          flags.sql_mode,
                          flags.max_sort_length,
                          flags.group_concat_max_len));
  memcpy((void *)(sql + (tot_length - QUERY_CACHE_FLAGS_SIZE)),
	 &flags, QUERY_CACHE_FLAGS_SIZE);
  query_block = (Query_cache_block *)  hash_search(&queries, (byte*) sql,
						   tot_length);
  /* Quick abort on unlocked data */
  if (query_block == 0 ||
      query_block->query()->result() == 0 ||
      query_block->query()->result()->type != Query_cache_block::RESULT)
  {
    DBUG_PRINT("qcache", ("No query in query hash or no results"));
    goto err_unlock;
  }
  DBUG_PRINT("qcache", ("Query in query hash 0x%lx", (ulong)query_block));

  /* Now lock and test that nothing changed while blocks was unlocked */
  BLOCK_LOCK_RD(query_block);

  query = query_block->query();
  result_block= first_result_block= query->result();

  if (result_block == 0 || result_block->type != Query_cache_block::RESULT)
  {
    /* The query is probably yet processed */
    DBUG_PRINT("qcache", ("query found, but no data or data incomplete"));
    BLOCK_UNLOCK_RD(query_block);
    goto err_unlock;
  }
  DBUG_PRINT("qcache", ("Query have result 0x%lx", (ulong) query));

  if ((thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) &&
      (query->tables_type() & HA_CACHE_TBL_TRANSACT))
  {
    DBUG_PRINT("qcache",
	       ("we are in transaction and have transaction tables in query"));
    BLOCK_UNLOCK_RD(query_block);
    goto err_unlock;
  }
      
  // Check access;
  block_table= query_block->table(0);
  block_table_end= block_table+query_block->n_tables;
  for (; block_table != block_table_end; block_table++)
  {
    TABLE_LIST table_list;
    TABLE *tmptable;
    Query_cache_table *table = block_table->parent;

    /*
      Check that we have not temporary tables with same names of tables
      of this query. If we have such tables, we will not send data from
      query cache, because temporary tables hide real tables by which
      query in query cache was made.
    */
    for (tmptable= thd->temporary_tables; tmptable ; tmptable= tmptable->next)
    {
      if (tmptable->s->key_length - TMP_TABLE_KEY_EXTRA == 
          table->key_length() &&
          !memcmp(tmptable->s->table_cache_key, table->data(),
                  table->key_length()))
      {
        DBUG_PRINT("qcache",
                   ("Temporary table detected: '%s.%s'",
                    table_list.db, table_list.alias));
        STRUCT_UNLOCK(&structure_guard_mutex);
        /*
          We should not store result of this query because it contain
          temporary tables => assign following variable to make check
          faster.
        */
        thd->lex->safe_to_cache_query=0;
        BLOCK_UNLOCK_RD(query_block);
        DBUG_RETURN(-1);
      }
    }

    bzero((char*) &table_list,sizeof(table_list));
    table_list.db = table->db();
    table_list.alias= table_list.table_name= table->table();
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    if (check_table_access(thd,SELECT_ACL,&table_list,1))
    {
      DBUG_PRINT("qcache",
		 ("probably no SELECT access to %s.%s =>  return to normal processing",
		  table_list.db, table_list.alias));
      STRUCT_UNLOCK(&structure_guard_mutex);
      thd->lex->safe_to_cache_query=0;		// Don't try to cache this
      BLOCK_UNLOCK_RD(query_block);
      DBUG_RETURN(-1);				// Privilege error
    }
    if (table_list.grant.want_privilege)
    {
      DBUG_PRINT("qcache", ("Need to check column privileges for %s.%s",
			    table_list.db, table_list.alias));
      BLOCK_UNLOCK_RD(query_block);
      thd->lex->safe_to_cache_query= 0;		// Don't try to cache this
      goto err_unlock;				// Parse query
    }
#endif /*!NO_EMBEDDED_ACCESS_CHECKS*/
    engine_data= table->engine_data();
    if (table->callback() &&
        !(*table->callback())(thd, table->db(),
                              table->key_length(),
                              &engine_data))
    {
      DBUG_PRINT("qcache", ("Handler does not allow caching for %s.%s",
			    table_list.db, table_list.alias));
      BLOCK_UNLOCK_RD(query_block);
      if (engine_data != table->engine_data())
      {
        DBUG_PRINT("qcache",
                   ("Handler require invalidation queries of %s.%s %lld-%lld",
                              table_list.db, table_list.alias,
                              engine_data, table->engine_data()));
        invalidate_table((byte *) table->db(), table->key_length());
      }
      else
        thd->lex->safe_to_cache_query= 0;       // Don't try to cache this
      goto err_unlock;				// Parse query
    }
    else
      DBUG_PRINT("qcache", ("handler allow caching %s,%s",
			    table_list.db, table_list.alias));
  }
  move_to_query_list_end(query_block);
  hits++;
  STRUCT_UNLOCK(&structure_guard_mutex);

  /*
    Send cached result to client
  */
#ifndef EMBEDDED_LIBRARY
  do
  {
    DBUG_PRINT("qcache", ("Results  (len %lu, used %lu, headers %lu)",
			  result_block->length, result_block->used,
			  result_block->headers_len()+
			  ALIGN_SIZE(sizeof(Query_cache_result))));
    
    Query_cache_result *result = result_block->result();
    if (net_real_write(&thd->net, result->data(),
		       result_block->used -
		       result_block->headers_len() -
		       ALIGN_SIZE(sizeof(Query_cache_result))))
      break;					// Client aborted
    result_block = result_block->next;
    thd->net.pkt_nr= query->last_pkt_nr; // Keep packet number updated
  } while (result_block != first_result_block);
#else
  {
    Querycache_stream qs(result_block, result_block->headers_len() +
			 ALIGN_SIZE(sizeof(Query_cache_result)));
    emb_load_querycache_result(thd, &qs);
  }
#endif /*!EMBEDDED_LIBRARY*/

  thd->limit_found_rows = query->found_rows();

  BLOCK_UNLOCK_RD(query_block);
  DBUG_RETURN(1);				// Result sent to client

err_unlock:
  STRUCT_UNLOCK(&structure_guard_mutex);
err:
  DBUG_RETURN(0);				// Query was not cached
}


/*
  Remove all cached queries that uses any of the tables in the list
*/

void Query_cache::invalidate(THD *thd, TABLE_LIST *tables_used,
			     my_bool using_transactions)
{
  DBUG_ENTER("Query_cache::invalidate (table list)");
  if (query_cache_size > 0)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)
    {
      DUMP(this);

      using_transactions = using_transactions &&
	(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
      for (; tables_used; tables_used= tables_used->next_local)
      {
	DBUG_ASSERT(!using_transactions || tables_used->table!=0);
	if (tables_used->derived)
	  continue;
	if (using_transactions &&
	   (tables_used->table->file->table_cache_type() ==
	    HA_CACHE_TBL_TRANSACT))
	  /*
	     Tables_used->table can't be 0 in transaction.
	     Only 'drop' invalidate not opened table, but 'drop'
	     force transaction finish.
	  */
	  thd->add_changed_table(tables_used->table);
	else
	  invalidate_table(tables_used);
      }
    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

void Query_cache::invalidate(CHANGED_TABLE_LIST *tables_used)
{
  DBUG_ENTER("Query_cache::invalidate (changed table list)");
  if (query_cache_size > 0 && tables_used)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)
    {
      DUMP(this);
      for (; tables_used; tables_used= tables_used->next)
      {
	invalidate_table((byte*) tables_used->key, tables_used->key_length);
	DBUG_PRINT("qcache", (" db %s, table %s", tables_used->key,
			      tables_used->key+
			      strlen(tables_used->key)+1));
      }
    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}


/*
  Invalidate locked for write

  SYNOPSIS
    Query_cache::invalidate_locked_for_write()
    tables_used - table list

  NOTE
    can be used only for opened tables
*/
void Query_cache::invalidate_locked_for_write(TABLE_LIST *tables_used)
{
  DBUG_ENTER("Query_cache::invalidate_locked_for_write");
  if (query_cache_size > 0 && tables_used)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)
    {
      DUMP(this);
      for (; tables_used; tables_used= tables_used->next_local)
      {
        if (tables_used->lock_type & (TL_WRITE_LOW_PRIORITY | TL_WRITE) &&
            tables_used->table)
	  invalidate_table(tables_used->table);
      }
    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

/*
  Remove all cached queries that uses the given table
*/

void Query_cache::invalidate(THD *thd, TABLE *table, 
			     my_bool using_transactions)
{
  DBUG_ENTER("Query_cache::invalidate (table)");
  
  if (query_cache_size > 0)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)
    {
      using_transactions = using_transactions &&
	(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
      if (using_transactions && 
	  (table->file->table_cache_type() == HA_CACHE_TBL_TRANSACT))
	thd->add_changed_table(table);
      else
	invalidate_table(table);
    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

void Query_cache::invalidate(THD *thd, const char *key, uint32  key_length,
			     my_bool using_transactions)
{
  DBUG_ENTER("Query_cache::invalidate (key)");
  
  if (query_cache_size > 0)
  {
    using_transactions = using_transactions &&
      (thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
    if (using_transactions) // used for innodb => has_transactions() is TRUE
      thd->add_changed_table(key, key_length);
    else
    {
      STRUCT_LOCK(&structure_guard_mutex);
      if (query_cache_size > 0)
	invalidate_table((byte*)key, key_length);
      STRUCT_UNLOCK(&structure_guard_mutex);  
    }
  }
  DBUG_VOID_RETURN;
}

/*
  Remove all cached queries that uses the given database
*/

void Query_cache::invalidate(char *db)
{
  DBUG_ENTER("Query_cache::invalidate (db)");
  if (query_cache_size > 0)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)
    {
      DUMP(this);
  restart_search:
      if (tables_blocks)
      {
	Query_cache_block *curr= tables_blocks;
	Query_cache_block *next;
	do
	{
	  next= curr->next;
	  if (strcmp(db, (char*)(curr->table()->db())) == 0)
	    invalidate_table(curr);
	  /*
	    invalidate_table can freed block on which point 'next' (if
	    table of this block used only in queries which was deleted
	    by invalidate_table). As far as we do not allocate new blocks
	    and mark all headers of freed blocks as 'FREE' (even if they are
	    merged with other blocks) we can just test type of block
	    to be sure that block is not deleted
	  */
	  if (next->type == Query_cache_block::FREE)
	    goto restart_search;
	  curr= next;
	} while (curr != tables_blocks);
      }
    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}


void Query_cache::invalidate_by_MyISAM_filename(const char *filename)
{
  DBUG_ENTER("Query_cache::invalidate_by_MyISAM_filename");
  if (query_cache_size > 0)
  {
    /* Calculate the key outside the lock to make the lock shorter */
    char key[MAX_DBKEY_LENGTH];
    uint32 db_length;
    uint key_length= filename_2_table_key(key, filename, &db_length);
    STRUCT_LOCK(&structure_guard_mutex);
    if (query_cache_size > 0)			// Safety if cache removed
    {
      Query_cache_block *table_block;
      if ((table_block = (Query_cache_block*) hash_search(&tables,
							  (byte*) key,
							  key_length)))
	invalidate_table(table_block);
    }
    STRUCT_UNLOCK(&structure_guard_mutex);
  }
  DBUG_VOID_RETURN;
}

  /* Remove all queries from cache */

void Query_cache::flush()
{
  DBUG_ENTER("Query_cache::flush");
  STRUCT_LOCK(&structure_guard_mutex);
  if (query_cache_size > 0)
  {
    DUMP(this);
    flush_cache();
    DUMP(this);
  }

  DBUG_EXECUTE("check_querycache",query_cache.check_integrity(1););
  STRUCT_UNLOCK(&structure_guard_mutex);
  DBUG_VOID_RETURN;
}

  /* Join result in cache in 1 block (if result length > join_limit) */

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
  if (!initialized)
  {
    DBUG_PRINT("qcache", ("Query Cache not initialized"));
  }
  else
  {
    free_cache();
    pthread_mutex_destroy(&structure_guard_mutex);
    initialized = 0;
  }
  DBUG_VOID_RETURN;
}


/*****************************************************************************
  init/destroy
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
  uint mem_bin_count, num, step;
  ulong mem_bin_size, prev_size, inc;
  ulong additional_data_size, max_mem_bin_size, approx_additional_data_size;
  int align;

  DBUG_ENTER("Query_cache::init_cache");
  approx_additional_data_size = (sizeof(Query_cache) +
				 sizeof(gptr)*(def_query_hash_size+
					       def_table_hash_size));
  if (query_cache_size < approx_additional_data_size)
    goto err;

  query_cache_size-= approx_additional_data_size;
  align= query_cache_size % ALIGN_SIZE(1);
  if (align)
  {
    query_cache_size-= align;
    approx_additional_data_size+= align;
  }

  /*
    Count memory bins number.
    Check section 6. in start comment for the used algorithm.
  */

  max_mem_bin_size = query_cache_size >> QUERY_CACHE_MEM_BIN_FIRST_STEP_PWR2;
  mem_bin_count = (uint)  ((1 + QUERY_CACHE_MEM_BIN_PARTS_INC) *
			   QUERY_CACHE_MEM_BIN_PARTS_MUL);
  mem_bin_num = 1;
  mem_bin_steps = 1;
  mem_bin_size = max_mem_bin_size >> QUERY_CACHE_MEM_BIN_STEP_PWR2;
  prev_size = 0;
  if (mem_bin_size <= min_allocation_unit)
  {
    DBUG_PRINT("qcache", ("too small query cache => query cache disabled"));
    // TODO here (and above) should be warning in 4.1
    goto err;
  }
  while (mem_bin_size > min_allocation_unit)
  {
    mem_bin_num += mem_bin_count;
    prev_size = mem_bin_size;
    mem_bin_size >>= QUERY_CACHE_MEM_BIN_STEP_PWR2;
    mem_bin_steps++;
    mem_bin_count += QUERY_CACHE_MEM_BIN_PARTS_INC;
    mem_bin_count = (uint) (mem_bin_count * QUERY_CACHE_MEM_BIN_PARTS_MUL);

    // Prevent too small bins spacing
    if (mem_bin_count > (mem_bin_size >> QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2))
      mem_bin_count= (mem_bin_size >> QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2);
  }
  inc = (prev_size - mem_bin_size) / mem_bin_count;
  mem_bin_num += (mem_bin_count - (min_allocation_unit - mem_bin_size)/inc);
  mem_bin_steps++;
  additional_data_size = ((mem_bin_num+1) *
			  ALIGN_SIZE(sizeof(Query_cache_memory_bin))+
			  (mem_bin_steps *
			   ALIGN_SIZE(sizeof(Query_cache_memory_bin_step))));

  if (query_cache_size < additional_data_size)
    goto err;
  query_cache_size -= additional_data_size;

  if (!(cache= (byte *)
        my_malloc_lock(query_cache_size+additional_data_size, MYF(0))))
    goto err;

  DBUG_PRINT("qcache", ("cache length %lu, min unit %lu, %u bins",
		      query_cache_size, min_allocation_unit, mem_bin_num));

  steps = (Query_cache_memory_bin_step *) cache;
  bins = ((Query_cache_memory_bin *)
	  (cache + mem_bin_steps *
	   ALIGN_SIZE(sizeof(Query_cache_memory_bin_step))));

  first_block = (Query_cache_block *) (cache + additional_data_size);
  first_block->init(query_cache_size);
  total_blocks++;
  first_block->pnext=first_block->pprev=first_block;
  first_block->next=first_block->prev=first_block;

  /* Prepare bins */

  bins[0].init(max_mem_bin_size);
  steps[0].init(max_mem_bin_size,0,0);
  mem_bin_count = (uint) ((1 + QUERY_CACHE_MEM_BIN_PARTS_INC) *
			  QUERY_CACHE_MEM_BIN_PARTS_MUL);
  num= step= 1;
  mem_bin_size = max_mem_bin_size >> QUERY_CACHE_MEM_BIN_STEP_PWR2;
  while (mem_bin_size > min_allocation_unit)
  {
    ulong incr = (steps[step-1].size - mem_bin_size) / mem_bin_count;
    unsigned long size = mem_bin_size;
    for (uint i= mem_bin_count; i > 0; i--)
    {
      bins[num+i-1].init(size);
      size += incr;
    }
    num += mem_bin_count;
    steps[step].init(mem_bin_size, num-1, incr);
    mem_bin_size >>= QUERY_CACHE_MEM_BIN_STEP_PWR2;
    step++;
    mem_bin_count += QUERY_CACHE_MEM_BIN_PARTS_INC;
    mem_bin_count = (uint) (mem_bin_count * QUERY_CACHE_MEM_BIN_PARTS_MUL);
    if (mem_bin_count > (mem_bin_size >> QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2))
      mem_bin_count=(mem_bin_size >> QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2);
  }
  inc = (steps[step-1].size - mem_bin_size) / mem_bin_count;

  /*
    num + mem_bin_count > mem_bin_num, but index never be > mem_bin_num
    because block with size < min_allocated_unit never will be requested
  */

  steps[step].init(mem_bin_size, num + mem_bin_count - 1, inc);
  {
    uint skiped = (min_allocation_unit - mem_bin_size)/inc;
    ulong size = mem_bin_size + inc*skiped;
    uint i = mem_bin_count - skiped;
    while (i-- > 0)
    {
      bins[num+i].init(size);
      size += inc;
    }
  }
  bins[mem_bin_num].number = 1;	// For easy end test in get_free_block
  free_memory = free_memory_blocks = 0;
  insert_into_free_memory_list(first_block);

  DUMP(this);

  VOID(hash_init(&queries, &my_charset_bin, def_query_hash_size, 0, 0,
		 query_cache_query_get_key, 0, 0));
#ifndef FN_NO_CASE_SENCE
  /*
    If lower_case_table_names!=0 then db and table names are already 
    converted to lower case and we can use binary collation for their 
    comparison (no matter if file system case sensitive or not).
    If we have case-sensitive file system (like on most Unixes) and
    lower_case_table_names == 0 then we should distinguish my_table
    and MY_TABLE cases and so again can use binary collation.
  */
  VOID(hash_init(&tables, &my_charset_bin, def_table_hash_size, 0, 0,
		 query_cache_table_get_key, 0, 0));
#else
  /*
    On windows, OS/2, MacOS X with HFS+ or any other case insensitive
    file system if lower_case_table_names!=0 we have same situation as
    in previous case, but if lower_case_table_names==0 then we should
    not distinguish cases (to be compatible in behavior with underlying
    file system) and so should use case insensitive collation for
    comparison.
  */
  VOID(hash_init(&tables,
		 lower_case_table_names ? &my_charset_bin :
		 files_charset_info,
		 def_table_hash_size, 0, 0,query_cache_table_get_key, 0, 0));
#endif

  queries_in_cache = 0;
  queries_blocks = 0;
  DBUG_RETURN(query_cache_size +
	      additional_data_size + approx_additional_data_size);

err:
  make_disabled();
  DBUG_RETURN(0);
}


/* Disable the use of the query cache */

void Query_cache::make_disabled()
{
  DBUG_ENTER("Query_cache::make_disabled");
  query_cache_size= 0;
  queries_blocks= 0;
  free_memory= 0;
  bins= 0;
  steps= 0;
  cache= 0;
  mem_bin_num= mem_bin_steps= 0;
  queries_in_cache= 0;
  first_block= 0;
  DBUG_VOID_RETURN;
}


void Query_cache::free_cache()
{
  DBUG_ENTER("Query_cache::free_cache");
  if (query_cache_size > 0)
  {
    flush_cache();
#ifndef DBUG_OFF
    if (bins[0].free_blocks == 0)
    {
      wreck(__LINE__,"no free memory found in (bins[0].free_blocks");
      DBUG_VOID_RETURN;
    }
#endif

    /* Becasue we did a flush, all cache memory must be in one this block */
    bins[0].free_blocks->destroy();
    total_blocks--;
#ifndef DBUG_OFF
    if (free_memory != query_cache_size)
      DBUG_PRINT("qcache", ("free memory %lu (should be %lu)",
			    free_memory , query_cache_size));
#endif
    my_free((gptr) cache, MYF(MY_ALLOW_ZERO_PTR));
    make_disabled();
    hash_free(&queries);
    hash_free(&tables);
  }
  DBUG_VOID_RETURN;
}

/*****************************************************************************
  Free block data
*****************************************************************************/

/*
  The following assumes we have a lock on the cache
*/

void Query_cache::flush_cache()
{
  while (queries_blocks != 0)
  {
    BLOCK_LOCK_WR(queries_blocks);
    free_query(queries_blocks);
  }
}

/*
  Free oldest query that is not in use by another thread.
  Returns 1 if we couldn't remove anything
*/

my_bool Query_cache::free_old_query()
{
  DBUG_ENTER("Query_cache::free_old_query");
  if (queries_blocks)
  {
    /*
      try_lock_writing used to prevent client because here lock
      sequence is breached.
      Also we don't need remove locked queries at this point.
    */
    Query_cache_block *query_block = 0;
    if (queries_blocks != 0)
    {
      Query_cache_block *block = queries_blocks;
      /* Search until we find first query that we can remove */
      do
      {
	Query_cache_query *header = block->query();
	if (header->result() != 0 &&
	    header->result()->type == Query_cache_block::RESULT &&
	    block->query()->try_lock_writing())
	{
	  query_block = block;
	  break;
	}
      } while ((block=block->next) != queries_blocks );
    }

    if (query_block != 0)
    {
      free_query(query_block);
      lowmem_prunes++;
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(1);				// Nothing to remove
}

/*
  Free query from query cache.
  query_block must be locked for writing.
  This function will remove (and destroy) the lock for the query.
*/

void Query_cache::free_query(Query_cache_block *query_block)
{
  DBUG_ENTER("Query_cache::free_query");
  DBUG_PRINT("qcache", ("free query 0x%lx %lu bytes result",
		      (ulong) query_block,
		      query_block->query()->length() ));

  queries_in_cache--;
  hash_delete(&queries,(byte *) query_block);

  Query_cache_query *query = query_block->query();

  if (query->writer() != 0)
  {
    /* Tell MySQL that this query should not be cached anymore */
    query->writer()->query_cache_query = 0;
    query->writer(0);
  }
  double_linked_list_exclude(query_block, &queries_blocks);
  Query_cache_block_table *table=query_block->table(0);

  for (TABLE_COUNTER_TYPE i=0; i < query_block->n_tables; i++)
    unlink_table(table++);
  Query_cache_block *result_block = query->result();

  /*
    The following is true when query destruction was called and no results
    in query . (query just registered and then abort/pack/flush called)
  */
  if (result_block != 0)
  {
    if (result_block->type != Query_cache_block::RESULT)
    {
      // removing unfinished query
      refused++;
      inserts--;
    }
    Query_cache_block *block = result_block;
    do
    {
      Query_cache_block *current = block;
      block = block->next;
      free_memory_block(current);
    } while (block != result_block);
  }
  else
  {
    // removing unfinished query
    refused++;
    inserts--;
  }

  query->unlock_n_destroy();
  free_memory_block(query_block);

  DBUG_VOID_RETURN;
}

/*****************************************************************************
 Query data creation
*****************************************************************************/

Query_cache_block *
Query_cache::write_block_data(ulong data_len, gptr data,
			      ulong header_len,
			      Query_cache_block::block_type type,
			      TABLE_COUNTER_TYPE ntab,
			      my_bool under_guard)
{
  ulong all_headers_len = (ALIGN_SIZE(sizeof(Query_cache_block)) +
			   ALIGN_SIZE(ntab*sizeof(Query_cache_block_table)) +
			   header_len);
  ulong len = data_len + all_headers_len;
  ulong align_len= ALIGN_SIZE(len);
  DBUG_ENTER("Query_cache::write_block_data");
  DBUG_PRINT("qcache", ("data: %ld, header: %ld, all header: %ld",
		      data_len, header_len, all_headers_len));
  Query_cache_block *block = allocate_block(max(align_len, 
						min_allocation_unit),
					    1, 0, under_guard);
  if (block != 0)
  {
    block->type = type;
    block->n_tables = ntab;
    block->used = len;

    memcpy((void*) (((byte *) block)+ all_headers_len),
	   (void*) data, data_len);
  }
  DBUG_RETURN(block);
}


/*
  On success STRUCT_UNLOCK(&query_cache.structure_guard_mutex) will be done.
*/

my_bool
Query_cache::append_result_data(Query_cache_block **current_block,
				ulong data_len, gptr data,
				Query_cache_block *query_block)
{
  DBUG_ENTER("Query_cache::append_result_data");
  DBUG_PRINT("qcache", ("append %lu bytes to 0x%lx query",
		      data_len, query_block));

  if (query_block->query()->add(data_len) > query_cache_limit)
  {
    DBUG_PRINT("qcache", ("size limit reached %lu > %lu",
			query_block->query()->length(),
			query_cache_limit));
    DBUG_RETURN(0);
  }
  if (*current_block == 0)
  {
    DBUG_PRINT("qcache", ("allocated first result data block %lu", data_len));
    /*
      STRUCT_UNLOCK(&structure_guard_mutex) Will be done by
      write_result_data if success;
    */
    DBUG_RETURN(write_result_data(current_block, data_len, data, query_block,
				  Query_cache_block::RES_BEG));
  }
  Query_cache_block *last_block = (*current_block)->prev;

  DBUG_PRINT("qcache", ("lastblock 0x%lx len %lu used %lu",
		      (ulong) last_block, last_block->length,
		      last_block->used));
  my_bool success = 1;
  ulong last_block_free_space= last_block->length - last_block->used;

  /*
    We will first allocate and write the 'tail' of data, that doesn't fit
    in the 'last_block'.  Only if this succeeds, we will fill the last_block.
    This saves us a memcpy if the query doesn't fit in the query cache.
  */

  // Try join blocks if physically next block is free...
  ulong tail = data_len - last_block_free_space;
  ulong append_min = get_min_append_result_data_size();
  if (last_block_free_space < data_len &&
      append_next_free_block(last_block,
			     max(tail, append_min)))
    last_block_free_space = last_block->length - last_block->used;
  // If no space in last block (even after join) allocate new block
  if (last_block_free_space < data_len)
  {
    DBUG_PRINT("qcache", ("allocate new block for %lu bytes",
			data_len-last_block_free_space));
    Query_cache_block *new_block = 0;
    /*
      On success STRUCT_UNLOCK(&structure_guard_mutex) will be done
      by the next call
    */
    success = write_result_data(&new_block, data_len-last_block_free_space,
				(gptr)(((byte*)data)+last_block_free_space),
				query_block,
				Query_cache_block::RES_CONT);
    /*
       new_block may be != 0 even !success (if write_result_data
       allocate a small block but failed to allocate continue)
    */
    if (new_block != 0)
      double_linked_list_join(last_block, new_block);
  }
  else
  {
    // It is success (nobody can prevent us write data)
    STRUCT_UNLOCK(&structure_guard_mutex);
  }

  // Now finally write data to the last block
  if (success && last_block_free_space > 0)
  {
    ulong to_copy = min(data_len,last_block_free_space);
    DBUG_PRINT("qcache", ("use free space %lub at block 0x%lx to copy %lub",
			last_block_free_space, (ulong)last_block, to_copy));
    memcpy((void*) (((byte*) last_block) + last_block->used), (void*) data,
	   to_copy);
    last_block->used+=to_copy;
  }
  DBUG_RETURN(success);
}


my_bool Query_cache::write_result_data(Query_cache_block **result_block,
				       ulong data_len, gptr data,
				       Query_cache_block *query_block,
				       Query_cache_block::block_type type)
{
  DBUG_ENTER("Query_cache::write_result_data");
  DBUG_PRINT("qcache", ("data_len %lu",data_len));

  /*
    Reserve block(s) for filling
    During data allocation we must have structure_guard_mutex locked.
    As data copy is not a fast operation, it's better if we don't have
    structure_guard_mutex locked during data coping.
    Thus we first allocate space and lock query, then unlock
    structure_guard_mutex and copy data.
  */

  my_bool success = allocate_data_chain(result_block, data_len, query_block,
					type == Query_cache_block::RES_BEG);
  if (success)
  {
    // It is success (nobody can prevent us write data)
    STRUCT_UNLOCK(&structure_guard_mutex);
    uint headers_len = (ALIGN_SIZE(sizeof(Query_cache_block)) +
			ALIGN_SIZE(sizeof(Query_cache_result)));
#ifndef EMBEDDED_LIBRARY
    Query_cache_block *block= *result_block;
    byte *rest= (byte*) data;
    // Now fill list of blocks that created by allocate_data_chain
    do
    {
      block->type = type;
      ulong length = block->used - headers_len;
      DBUG_PRINT("qcache", ("write %lu byte in block 0x%lx",length,
			    (ulong)block));
      memcpy((void*)(((byte*) block)+headers_len), (void*) rest, length);
      rest += length;
      block = block->next;
      type = Query_cache_block::RES_CONT;
    } while (block != *result_block);
#else
    /*
      Set type of first block, emb_store_querycache_result() will handle
      the others.
    */
    (*result_block)->type= type;
    Querycache_stream qs(*result_block, headers_len);
    emb_store_querycache_result(&qs, (THD*)data);
#endif /*!EMBEDDED_LIBRARY*/
  }
  else
  {
    if (*result_block != 0)
    {
      // Destroy list of blocks that was created & locked by lock_result_data
      Query_cache_block *block = *result_block;
      do
      {
	Query_cache_block *current = block;
	block = block->next;
	free_memory_block(current);
      } while (block != *result_block);
      *result_block = 0;
      /*
	It is not success => not unlock structure_guard_mutex (we need it to
	free query)
      */
    }
  }
  DBUG_PRINT("qcache", ("success %d", (int) success));
  DBUG_RETURN(success);
}

inline ulong Query_cache::get_min_first_result_data_size()
{
  if (queries_in_cache < QUERY_CACHE_MIN_ESTIMATED_QUERIES_NUMBER)
    return min_result_data_size;
  ulong avg_result = (query_cache_size - free_memory) / queries_in_cache;
  avg_result = min(avg_result, query_cache_limit);
  return max(min_result_data_size, avg_result);
}

inline ulong Query_cache::get_min_append_result_data_size()
{
  return min_result_data_size;
}

/*
  Allocate one or more blocks to hold data
*/
my_bool Query_cache::allocate_data_chain(Query_cache_block **result_block,
					 ulong data_len,
					 Query_cache_block *query_block,
					 my_bool first_block_arg)
{
  ulong all_headers_len = (ALIGN_SIZE(sizeof(Query_cache_block)) +
			   ALIGN_SIZE(sizeof(Query_cache_result)));
  ulong min_size = (first_block_arg ?
		    get_min_first_result_data_size():
		    get_min_append_result_data_size());
  Query_cache_block *prev_block= NULL;
  Query_cache_block *new_block;
  DBUG_ENTER("Query_cache::allocate_data_chain");
  DBUG_PRINT("qcache", ("data_len %lu, all_headers_len %lu",
			data_len, all_headers_len));

  do
  {
    ulong len= data_len + all_headers_len;
    ulong align_len= ALIGN_SIZE(len);

    if (!(new_block= allocate_block(max(min_size, align_len),
				    min_result_data_size == 0,
				    all_headers_len + min_result_data_size,
				    1)))
    {
      DBUG_PRINT("warning", ("Can't allocate block for results"));
      DBUG_RETURN(FALSE);
    }

    new_block->n_tables = 0;
    new_block->used = min(len, new_block->length);
    new_block->type = Query_cache_block::RES_INCOMPLETE;
    new_block->next = new_block->prev = new_block;
    Query_cache_result *header = new_block->result();
    header->parent(query_block);

    DBUG_PRINT("qcache", ("Block len %lu used %lu",
			  new_block->length, new_block->used));

    if (prev_block)
      double_linked_list_join(prev_block, new_block);
    else
      *result_block= new_block;
    if (new_block->length >= len)
      break;

    /*
      We got less memory then we need (no big memory blocks) =>
      Continue to allocated more blocks until we got everything we need.
    */
    data_len= len - new_block->length;
    prev_block= new_block;
  } while (1);

  DBUG_RETURN(TRUE);
}

/*****************************************************************************
  Tables management
*****************************************************************************/

/*
  Invalidate the first table in the table_list
*/

void Query_cache::invalidate_table(TABLE_LIST *table_list)
{
  if (table_list->table != 0)
    invalidate_table(table_list->table);	// Table is open
  else
  {
    char key[MAX_DBKEY_LENGTH];
    uint key_length;
    Query_cache_block *table_block;
    key_length=(uint) (strmov(strmov(key,table_list->db)+1,
			      table_list->table_name) -key)+ 1;

    // We don't store temporary tables => no key_length+=4 ...
    if ((table_block = (Query_cache_block*)
	 hash_search(&tables,(byte*) key,key_length)))
      invalidate_table(table_block);
  }
}

void Query_cache::invalidate_table(TABLE *table)
{
  invalidate_table((byte*) table->s->table_cache_key, table->s->key_length);
}

void Query_cache::invalidate_table(byte * key, uint32  key_length)
{
  Query_cache_block *table_block;
  if ((table_block = ((Query_cache_block*)
		      hash_search(&tables, key, key_length))))
    invalidate_table(table_block);
}

void Query_cache::invalidate_table(Query_cache_block *table_block)
{
  Query_cache_block_table *list_root =	table_block->table(0);
  while (list_root->next != list_root)
  {
    Query_cache_block *query_block = list_root->next->block();
    BLOCK_LOCK_WR(query_block);
    free_query(query_block);
  }
}


/*
  Register given table list begining with given position in tables table of
  block

  SYNOPSIS
    Query_cache::register_tables_from_list
    tables_used     given table list
    counter         number current position in table of tables of block
    block_table     pointer to current position in tables table of block

  RETURN
    0   error
    number of next position of table entry in table of tables of block
*/

TABLE_COUNTER_TYPE
Query_cache::register_tables_from_list(TABLE_LIST *tables_used,
                                       TABLE_COUNTER_TYPE counter,
                                       Query_cache_block_table *block_table)
{
  TABLE_COUNTER_TYPE n;
  DBUG_ENTER("Query_cache::register_tables_from_list");
  for (n= counter;
       tables_used;
       tables_used= tables_used->next_global, n++, block_table++)
  {
    if (tables_used->derived)
    {
      DBUG_PRINT("qcache", ("derived table skipped"));
      n--;
      block_table--;
      continue;
    }
    block_table->n= n;
    if (tables_used->view)
    {
      char key[MAX_DBKEY_LENGTH];
      uint key_length;
      DBUG_PRINT("qcache", ("view %s, db %s",
                            tables_used->view_name.str,
                            tables_used->view_db.str));
      key_length= (uint) (strmov(strmov(key, tables_used->view_db.str) + 1,
                                 tables_used->view_name.str) - key) + 1;
      /*
        There are not callback function for for VIEWs
      */
      if (!insert_table(key_length, key, block_table,
                        tables_used->view_db.length + 1,
                        HA_CACHE_TBL_NONTRANSACT, 0, 0))
        DBUG_RETURN(0);
      {
        TABLE_COUNTER_TYPE inc= register_tables_from_list(tables_used->ancestor,
                                                          n + 1,
                                                          block_table + 1);
        if (!inc)
          DBUG_RETURN(0);
        n+= inc;
        block_table+= inc;
      }
    }
    else
    {
      DBUG_PRINT("qcache",
                 ("table %s, db %s, openinfo at 0x%lx, keylen %u, key at 0x%lx",
                  tables_used->table->s->table_name,
                  tables_used->table->s->table_cache_key,
                  (ulong) tables_used->table,
                  tables_used->table->s->key_length,
                  (ulong) tables_used->table->s->table_cache_key));
      if (!insert_table(tables_used->table->s->key_length,
                        tables_used->table->s->table_cache_key, block_table,
                        tables_used->db_length,
                        tables_used->table->file->table_cache_type(),
                        tables_used->callback_func,
                        tables_used->engine_data))
        DBUG_RETURN(0);

      if (tables_used->table->s->db_type == DB_TYPE_MRG_MYISAM)
      {
        ha_myisammrg *handler = (ha_myisammrg *) tables_used->table->file;
        MYRG_INFO *file = handler->myrg_info();
        for (MYRG_TABLE *table = file->open_tables;
             table != file->end_table ;
             table++)
        {
          char key[MAX_DBKEY_LENGTH];
          uint32 db_length;
          uint key_length= filename_2_table_key(key, table->table->filename,
                                                &db_length);
          (++block_table)->n= ++n;
          /*
            There are not callback function for for MyISAM, and engine data
          */
          if (!insert_table(key_length, key, block_table,
                            db_length,
                            tables_used->table->file->table_cache_type(),
                            0, 0))
            DBUG_RETURN(0);
        }
      }
    }
  }
  DBUG_RETURN(n - counter);
}

/*
  Store all used tables

  SYNOPSIS
    register_all_tables()
    block		Store tables in this block
    tables_used		List if used tables
    tables_arg		Not used ?
*/

my_bool Query_cache::register_all_tables(Query_cache_block *block,
					 TABLE_LIST *tables_used,
					 TABLE_COUNTER_TYPE tables_arg)
{
  TABLE_COUNTER_TYPE n;
  DBUG_PRINT("qcache", ("register tables block 0x%lx, n %d, header %x",
		      (ulong) block, (int) tables_arg,
		      (int) ALIGN_SIZE(sizeof(Query_cache_block))));

  Query_cache_block_table *block_table = block->table(0);

  n= register_tables_from_list(tables_used, 0, block_table);

  if (n)
  {
    DBUG_PRINT("qcache", ("failed at table %d", (int) n));
    /* Unlink the tables we allocated above */
    for (Query_cache_block_table *tmp = block->table(0) ;
	 tmp != block_table;
	 tmp++)
      unlink_table(tmp);
  }
  return (n);
}

/*
  Insert used tablename in cache
  Returns 0 on error
*/

my_bool
Query_cache::insert_table(uint key_len, char *key,
			  Query_cache_block_table *node,
			  uint32 db_length, uint8 cache_type,
                          qc_engine_callback callback,
                          ulonglong engine_data)
{
  DBUG_ENTER("Query_cache::insert_table");
  DBUG_PRINT("qcache", ("insert table node 0x%lx, len %d",
		      (ulong)node, key_len));

  Query_cache_block *table_block = ((Query_cache_block *)
				    hash_search(&tables, (byte*) key,
						key_len));

  if (table_block &&
      table_block->table()->engine_data() != engine_data)
  {
    DBUG_PRINT("qcache",
               ("Handler require invalidation queries of %s.%s %lld-%lld",
                table_block->table()->db(),
                table_block->table()->table(),
                engine_data,
                table_block->table()->engine_data()));
    /*
      as far as we delete all queries with this table, table block will be
      deleted, too
    */
    invalidate_table(table_block);
    table_block= 0;
  }

  if (table_block == 0)
  {
    DBUG_PRINT("qcache", ("new table block from 0x%lx (%u)",
			(ulong) key, (int) key_len));
    table_block = write_block_data(key_len, (gptr) key,
				   ALIGN_SIZE(sizeof(Query_cache_table)),
				   Query_cache_block::TABLE,
				   1, 1);
    if (table_block == 0)
    {
      DBUG_PRINT("qcache", ("Can't write table name to cache"));
      DBUG_RETURN(0);
    }
    Query_cache_table *header = table_block->table();
    double_linked_list_simple_include(table_block,
				      &tables_blocks);
    Query_cache_block_table *list_root = table_block->table(0);
    list_root->n = 0;
    list_root->next = list_root->prev = list_root;
    if (my_hash_insert(&tables, (const byte *) table_block))
    {
      DBUG_PRINT("qcache", ("Can't insert table to hash"));
      // write_block_data return locked block
      free_memory_block(table_block);
      DBUG_RETURN(0);
    }
    char *db = header->db();
    header->table(db + db_length + 1);
    header->key_length(key_len);
    header->type(cache_type);
    header->callback(callback);
    header->engine_data(engine_data);
  }

  Query_cache_block_table *list_root = table_block->table(0);
  node->next = list_root->next;
  list_root->next = node;
  node->next->prev = node;
  node->prev = list_root;
  node->parent = table_block->table();
  DBUG_RETURN(1);
}


void Query_cache::unlink_table(Query_cache_block_table *node)
{
  DBUG_ENTER("Query_cache::unlink_table");
  node->prev->next = node->next;
  node->next->prev = node->prev;
  Query_cache_block_table *neighbour = node->next;
  if (neighbour->next == neighbour)
  {
    // list is empty (neighbor is root of list)
    Query_cache_block *table_block = neighbour->block();
    double_linked_list_exclude(table_block,
			       &tables_blocks);
    hash_delete(&tables,(byte *) table_block);
    free_memory_block(table_block);
  }
  DBUG_VOID_RETURN;
}

/*****************************************************************************
  Free memory management
*****************************************************************************/

Query_cache_block *
Query_cache::allocate_block(ulong len, my_bool not_less, ulong min,
			    my_bool under_guard)
{
  DBUG_ENTER("Query_cache::allocate_block");
  DBUG_PRINT("qcache", ("len %lu, not less %d, min %lu, uder_guard %d",
		      len, not_less,min,under_guard));

  if (len >= min(query_cache_size, query_cache_limit))
  {
    DBUG_PRINT("qcache", ("Query cache hase only %lu memory and limit %lu",
			query_cache_size, query_cache_limit));
    DBUG_RETURN(0); // in any case we don't have such piece of memory
  }

  if (!under_guard)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    /*
      It is very unlikely that following condition is TRUE (it is possible
      only if other thread is resizing cache), so we check it only after
      guard mutex lock
    */
    if (unlikely(query_cache.query_cache_size == 0))
    {
      STRUCT_UNLOCK(&structure_guard_mutex);
      DBUG_RETURN(0);
    }
  }

  /* Free old queries until we have enough memory to store this block */
  Query_cache_block *block;
  do
  {
    block= get_free_block(len, not_less, min);
  }
  while (block == 0 && !free_old_query());

  if (block != 0)				// If we found a suitable block
  {
    if (block->length >= ALIGN_SIZE(len) + min_allocation_unit)
      split_block(block,ALIGN_SIZE(len));
  }

  if (!under_guard)
    STRUCT_UNLOCK(&structure_guard_mutex);
  DBUG_RETURN(block);
}


Query_cache_block *
Query_cache::get_free_block(ulong len, my_bool not_less, ulong min)
{
  Query_cache_block *block = 0, *first = 0;
  DBUG_ENTER("Query_cache::get_free_block");
  DBUG_PRINT("qcache",("length %lu, not_less %d, min %lu", len,
		     (int)not_less, min));

  /* Find block with minimal size > len  */
  uint start = find_bin(len);
  // try matching bin
  if (bins[start].number != 0)
  {
    Query_cache_block *list = bins[start].free_blocks;
    if (list->prev->length >= len) // check block with max size 
    { 
      first = list;
      uint n = 0;
      while ( n < QUERY_CACHE_MEM_BIN_TRY &&
	      first->length < len) //we don't need irst->next != list
      {
	first=first->next;
	n++;
      }
      if (first->length >= len)
	block=first;
      else // we don't need if (first->next != list)
      {
	n = 0;
	block = list->prev;
	while (n < QUERY_CACHE_MEM_BIN_TRY &&
	       block->length > len)
	{
	  block=block->prev;
	  n++;
	}
	if (block->length < len)
	  block=block->next;
      }
    }
    else
      first = list->prev;
  }
  if (block == 0 && start > 0)
  {
    DBUG_PRINT("qcache",("Try bins with bigger block size"));
    // Try more big bins
    int i = start - 1;
    while (i > 0 && bins[i].number == 0)
      i--;
    if (bins[i].number > 0)
      block = bins[i].free_blocks;
  }

  // If no big blocks => try less size (if it is possible)
  if (block == 0 && ! not_less)
  {
    DBUG_PRINT("qcache",("Try to allocate a smaller block"));
    if (first != 0 && first->length > min)
      block = first;
    else
    {
      uint i = start + 1;
      /* bins[mem_bin_num].number contains 1 for easy end test */
      for (i= start+1 ; bins[i].number == 0 ; i++) ;
      if (i < mem_bin_num && bins[i].free_blocks->prev->length >= min)
	block = bins[i].free_blocks->prev;
    }
  }
  if (block != 0)
    exclude_from_free_memory_list(block);

  DBUG_PRINT("qcache",("getting block 0x%lx", (ulong) block));
  DBUG_RETURN(block);
}


void Query_cache::free_memory_block(Query_cache_block *block)
{
  DBUG_ENTER("Query_cache::free_memory_block");
  block->used=0;
  block->type= Query_cache_block::FREE; // mark block as free in any case
  DBUG_PRINT("qcache",
	     ("first_block 0x%lx, block 0x%lx, pnext 0x%lx pprev 0x%lx",
	      (ulong) first_block, (ulong) block, (ulong) block->pnext,
	      (ulong) block->pprev));

  if (block->pnext != first_block && block->pnext->is_free())
    block = join_free_blocks(block, block->pnext);
  if (block != first_block && block->pprev->is_free())
    block = join_free_blocks(block->pprev, block->pprev);
  insert_into_free_memory_list(block);
  DBUG_VOID_RETURN;
}


void Query_cache::split_block(Query_cache_block *block, ulong len)
{
  DBUG_ENTER("Query_cache::split_block");
  Query_cache_block *new_block = (Query_cache_block*)(((byte*) block)+len);

  new_block->init(block->length - len);
  total_blocks++;
  block->length=len;
  new_block->pnext = block->pnext;
  block->pnext = new_block;
  new_block->pprev = block;
  new_block->pnext->pprev = new_block;

  if (block->type == Query_cache_block::FREE)
  {
    // if block was free then it already joined with all free neighbours
    insert_into_free_memory_list(new_block);
  }
  else
    free_memory_block(new_block);

  DBUG_PRINT("qcache", ("split 0x%lx (%lu) new 0x%lx",
		      (ulong) block, len, (ulong) new_block));
  DBUG_VOID_RETURN;
}


Query_cache_block *
Query_cache::join_free_blocks(Query_cache_block *first_block_arg,
			      Query_cache_block *block_in_list)
{
  Query_cache_block *second_block;
  DBUG_ENTER("Query_cache::join_free_blocks");
  DBUG_PRINT("qcache",
	     ("join first 0x%lx, pnext 0x%lx, in list 0x%lx",
	      (ulong) first_block_arg, (ulong) first_block_arg->pnext,
	      (ulong) block_in_list));

  exclude_from_free_memory_list(block_in_list);
  second_block = first_block_arg->pnext;
  // May be was not free block
  second_block->used=0;
  second_block->destroy();
  total_blocks--;

  first_block_arg->length += second_block->length;
  first_block_arg->pnext = second_block->pnext;
  second_block->pnext->pprev = first_block_arg;

  DBUG_RETURN(first_block_arg);
}


my_bool Query_cache::append_next_free_block(Query_cache_block *block,
					    ulong add_size)
{
  Query_cache_block *next_block = block->pnext;
  DBUG_ENTER("Query_cache::append_next_free_block");
  DBUG_PRINT("enter", ("block 0x%lx, add_size %lu", (ulong) block,
		       add_size));

  if (next_block != first_block && next_block->is_free())
  {
    ulong old_len = block->length;
    exclude_from_free_memory_list(next_block);
    next_block->destroy();
    total_blocks--;

    block->length += next_block->length;
    block->pnext = next_block->pnext;
    next_block->pnext->pprev = block;

    if (block->length > ALIGN_SIZE(old_len + add_size) + min_allocation_unit)
      split_block(block,ALIGN_SIZE(old_len + add_size));
    DBUG_PRINT("exit", ("block was appended"));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


void Query_cache::exclude_from_free_memory_list(Query_cache_block *free_block)
{
  DBUG_ENTER("Query_cache::exclude_from_free_memory_list");
  Query_cache_memory_bin *bin = *((Query_cache_memory_bin **)
				  free_block->data());
  double_linked_list_exclude(free_block, &bin->free_blocks);
  bin->number--;
  free_memory-=free_block->length;
  free_memory_blocks--;
  DBUG_PRINT("qcache",("exclude block 0x%lx, bin 0x%lx", (ulong) free_block,
		     (ulong) bin));
  DBUG_VOID_RETURN;
}

void Query_cache::insert_into_free_memory_list(Query_cache_block *free_block)
{
  DBUG_ENTER("Query_cache::insert_into_free_memory_list");
  uint idx = find_bin(free_block->length);
  insert_into_free_memory_sorted_list(free_block, &bins[idx].free_blocks);
  /*
    We have enough memory in block for storing bin reference due to
    min_allocation_unit choice
  */
  Query_cache_memory_bin **bin_ptr = ((Query_cache_memory_bin**)
				      free_block->data());
  *bin_ptr = bins+idx;
  (*bin_ptr)->number++;
  DBUG_PRINT("qcache",("insert block 0x%lx, bin[%d] 0x%lx",
		     (ulong) free_block, idx, (ulong) *bin_ptr));
  DBUG_VOID_RETURN;
}

uint Query_cache::find_bin(ulong size)
{
  DBUG_ENTER("Query_cache::find_bin");
  // Binary search
  int left = 0, right = mem_bin_steps;
  do
  {
    int middle = (left + right) / 2;
    if (steps[middle].size > size)
      left = middle+1;
    else
      right = middle;
  } while (left < right);
  if (left == 0)
  {
    // first bin not subordinate of common rules
    DBUG_PRINT("qcache", ("first bin (# 0), size %lu",size));
    DBUG_RETURN(0);
  }
  uint bin =  steps[left].idx - 
    (uint)((size - steps[left].size)/steps[left].increment);
#ifndef DBUG_OFF
  bins_dump();
#endif
  DBUG_PRINT("qcache", ("bin %u step %u, size %lu step size %lu",
			bin, left, size, steps[left].size));
  DBUG_RETURN(bin);
}


/*****************************************************************************
 Lists management
*****************************************************************************/

void Query_cache::move_to_query_list_end(Query_cache_block *query_block)
{
  DBUG_ENTER("Query_cache::move_to_query_list_end");
  double_linked_list_exclude(query_block, &queries_blocks);
  double_linked_list_simple_include(query_block, &queries_blocks);
  DBUG_VOID_RETURN;
}


void Query_cache::insert_into_free_memory_sorted_list(Query_cache_block *
						      new_block,
						      Query_cache_block **
						      list)
{
  DBUG_ENTER("Query_cache::insert_into_free_memory_sorted_list");
  /*
     list sorted by size in ascendant order, because we need small blocks
     more frequently than bigger ones
  */

  new_block->used = 0;
  new_block->n_tables = 0;
  new_block->type = Query_cache_block::FREE;

  if (*list == 0)
  {
    *list = new_block->next=new_block->prev=new_block;
    DBUG_PRINT("qcache", ("inserted into empty list"));
  }
  else
  {
    Query_cache_block *point = *list;
    if (point->length >= new_block->length)
    {
      point = point->prev;
      *list = new_block;
    }
    else
    {
      /* Find right position in sorted list to put block */
      while (point->next != *list &&
	     point->next->length < new_block->length)
	point=point->next;
    }
    new_block->prev = point;
    new_block->next = point->next;
    new_block->next->prev = new_block;
    point->next = new_block;
  }
  free_memory+=new_block->length;
  free_memory_blocks++;
  DBUG_VOID_RETURN;
}


void
Query_cache::double_linked_list_simple_include(Query_cache_block *point,
						Query_cache_block **
						list_pointer)
{
  DBUG_ENTER("Query_cache::double_linked_list_simple_include");
  DBUG_PRINT("qcache", ("including block 0x%lx", (ulong) point));
  if (*list_pointer == 0)
    *list_pointer=point->next=point->prev=point;
  else
  {
    // insert to the end of list
    point->next = (*list_pointer);
    point->prev = (*list_pointer)->prev;
    point->prev->next = point;
    (*list_pointer)->prev = point;
  }
  DBUG_VOID_RETURN;
}

void
Query_cache::double_linked_list_exclude(Query_cache_block *point,
					Query_cache_block **list_pointer)
{
  DBUG_ENTER("Query_cache::double_linked_list_exclude");
  DBUG_PRINT("qcache", ("excluding block 0x%lx, list 0x%lx",
		      (ulong) point, (ulong) list_pointer));
  if (point->next == point)
    *list_pointer = 0;				// empty list
  else
  {
    point->next->prev = point->prev;
    point->prev->next = point->next;
    if (point == *list_pointer)
      *list_pointer = point->next;
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
 Query
*****************************************************************************/

/*
  Collect information about table types, check that tables are cachable and
  count them

  SYNOPSIS
    process_and_count_tables()
    tables_used     table list for processing
    tables_type     pointer to variable for table types collection

  RETURN
    0   error
    >0  number of tables
*/

static TABLE_COUNTER_TYPE process_and_count_tables(TABLE_LIST *tables_used,
                                                   uint8 *tables_type)
{
  DBUG_ENTER("process_and_count_tables");
  TABLE_COUNTER_TYPE table_count = 0;
  for (; tables_used; tables_used= tables_used->next_global)
  {
    table_count++;
    if (tables_used->view)
    {
      DBUG_PRINT("qcache", ("view %s, db %s",
                            tables_used->view_name.str,
                            tables_used->view_db.str));
      *tables_type|= HA_CACHE_TBL_NONTRANSACT;
      {
        TABLE_COUNTER_TYPE subcount;
        if (!(subcount= process_and_count_tables(tables_used->ancestor,
                                                 tables_type)))
          DBUG_RETURN(0);
        table_count+= subcount;
      }
    }
    else
    {
      DBUG_PRINT("qcache", ("table %s, db %s, type %u",
                            tables_used->table->s->table_name,
                            tables_used->table->s->table_cache_key,
                            tables_used->table->s->db_type));
      if (tables_used->derived)
      {
        table_count--;
        DBUG_PRINT("qcache", ("derived table skipped"));
        continue;
      }
      *tables_type|= tables_used->table->file->table_cache_type();

      /*
        table_alias_charset used here because it depends of
        lower_case_table_names variable
      */
      if (tables_used->table->s->tmp_table != NO_TMP_TABLE ||
          (*tables_type & HA_CACHE_TBL_NOCACHE) ||
          (tables_used->db_length == 5 &&
           my_strnncoll(table_alias_charset,
                        (uchar*)tables_used->table->s->table_cache_key, 6,
                        (uchar*)"mysql",6) == 0))
      {
        DBUG_PRINT("qcache",
                   ("select not cacheable: temporary, system or \
                    other non-cacheable table(s)"));
        DBUG_RETURN(0);
      }
      if (tables_used->table->s->db_type == DB_TYPE_MRG_MYISAM)
      {
        ha_myisammrg *handler = (ha_myisammrg *)tables_used->table->file;
        MYRG_INFO *file = handler->myrg_info();
        table_count+= (file->end_table - file->open_tables);
      }
    }
  }
  DBUG_RETURN(table_count);
}


/*
  If query is cacheable return number tables in query
  (query without tables are not cached)
*/

TABLE_COUNTER_TYPE Query_cache::is_cacheable(THD *thd, uint32 query_len,
					     char *query,
					     LEX *lex,
					     TABLE_LIST *tables_used,
					     uint8 *tables_type)
{
  TABLE_COUNTER_TYPE table_count;
  DBUG_ENTER("Query_cache::is_cacheable");

  if (lex->sql_command == SQLCOM_SELECT &&
      (thd->variables.query_cache_type == 1 ||
       (thd->variables.query_cache_type == 2 && (lex->select_lex.options &
						 OPTION_TO_QUERY_CACHE))) &&
      lex->safe_to_cache_query)
  {
    DBUG_PRINT("qcache", ("options %lx %lx, type %u",
			OPTION_TO_QUERY_CACHE,
			lex->select_lex.options,
			(int) thd->variables.query_cache_type));

    if (!(table_count= process_and_count_tables(tables_used, tables_type)))
      DBUG_RETURN(0);

    if ((thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) &&
	((*tables_type)&HA_CACHE_TBL_TRANSACT))
    {
      DBUG_PRINT("qcache", ("not in autocommin mode"));
      DBUG_RETURN(0);
    }
    DBUG_PRINT("qcache", ("select is using %d tables", table_count));
    DBUG_RETURN(table_count);
  }

  DBUG_PRINT("qcache",
	     ("not interesting query: %d or not cacheable, options %lx %lx, type %u",
	      (int) lex->sql_command,
	      OPTION_TO_QUERY_CACHE,
	      lex->select_lex.options,
	      (int) thd->variables.query_cache_type));
  DBUG_RETURN(0);
}

/*
  Check handler allowance to cache query with these tables

  SYNOPSYS
    Query_cache::ask_handler_allowance()
    thd - thread handlers
    tables_used - tables list used in query

  RETURN
    0 - caching allowed
    1 - caching disallowed
*/
my_bool Query_cache::ask_handler_allowance(THD *thd,
					   TABLE_LIST *tables_used)
{
  DBUG_ENTER("Query_cache::ask_handler_allowance");

  for (; tables_used; tables_used= tables_used->next_global)
  {
    TABLE *table;
    if (!(table= tables_used->table))
      continue;
    handler *handler= table->file;
    if (!handler->register_query_cache_table(thd, table->s->table_cache_key,
					     table->s->key_length,
					     &tables_used->callback_func,
					     &tables_used->engine_data))
    {
      DBUG_PRINT("qcache", ("Handler does not allow caching for %s.%s",
			    tables_used->db, tables_used->alias));
      thd->lex->safe_to_cache_query= 0;          // Don't try to cache this
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


/*****************************************************************************
  Packing
*****************************************************************************/

void Query_cache::pack_cache()
{
  DBUG_ENTER("Query_cache::pack_cache");
  STRUCT_LOCK(&structure_guard_mutex);
  /*
    It is very unlikely that following condition is TRUE (it is possible
    only if other thread is resizing cache), so we check it only after
    guard mutex lock
  */
  if (unlikely(query_cache_size == 0))
  {
    STRUCT_UNLOCK(&structure_guard_mutex);
    DBUG_VOID_RETURN;
  }

  DBUG_EXECUTE("check_querycache",query_cache.check_integrity(1););

  byte *border = 0;
  Query_cache_block *before = 0;
  ulong gap = 0;
  my_bool ok = 1;
  Query_cache_block *block = first_block;
  DUMP(this);

  if (first_block)
  {
    do
    {
      Query_cache_block *next=block->pnext;
      ok = move_by_type(&border, &before, &gap, block);
      block = next;
    } while (ok && block != first_block);

    if (border != 0)
    {
      Query_cache_block *new_block = (Query_cache_block *) border;
      new_block->init(gap);
      total_blocks++;
      new_block->pnext = before->pnext;
      before->pnext = new_block;
      new_block->pprev = before;
      new_block->pnext->pprev = new_block;
      insert_into_free_memory_list(new_block);
    }
    DUMP(this);
  }

  DBUG_EXECUTE("check_querycache",query_cache.check_integrity(1););
  STRUCT_UNLOCK(&structure_guard_mutex);
  DBUG_VOID_RETURN;
}


my_bool Query_cache::move_by_type(byte **border,
				  Query_cache_block **before, ulong *gap,
				  Query_cache_block *block)
{
  DBUG_ENTER("Query_cache::move_by_type");

  my_bool ok = 1;
  switch (block->type) {
  case Query_cache_block::FREE:
  {
    DBUG_PRINT("qcache", ("block 0x%lx FREE", (ulong) block));
    if (*border == 0)
    {
      *border = (byte *) block;
      *before = block->pprev;
      DBUG_PRINT("qcache", ("gap beginning here"));
    }
    exclude_from_free_memory_list(block);
    *gap +=block->length;
    block->pprev->pnext=block->pnext;
    block->pnext->pprev=block->pprev;
    block->destroy();
    total_blocks--;
    DBUG_PRINT("qcache", ("added to gap (%lu)", *gap));
    break;
  }
  case Query_cache_block::TABLE:
  {
    DBUG_PRINT("qcache", ("block 0x%lx TABLE", (ulong) block));
    if (*border == 0)
      break;
    ulong len = block->length, used = block->used;
    Query_cache_block_table *list_root = block->table(0);
    Query_cache_block_table *tprev = list_root->prev,
			    *tnext = list_root->next;
    Query_cache_block *prev = block->prev,
		      *next = block->next,
		      *pprev = block->pprev,
		      *pnext = block->pnext,
		      *new_block =(Query_cache_block *) *border;
    uint tablename_offset = block->table()->table() - block->table()->db();
    char *data = (char*) block->data();
    byte *key;
    uint key_length;
    key=query_cache_table_get_key((byte*) block, &key_length, 0);
    hash_search(&tables, (byte*) key, key_length);

    block->destroy();
    new_block->init(len);
    new_block->type=Query_cache_block::TABLE;
    new_block->used=used;
    new_block->n_tables=1;
    memmove((char*) new_block->data(), data, len-new_block->headers_len());
    relink(block, new_block, next, prev, pnext, pprev);
    if (tables_blocks == block)
      tables_blocks = new_block;

    Query_cache_block_table *nlist_root = new_block->table(0);
    nlist_root->n = 0;
    nlist_root->next = tnext;
    tnext->prev = nlist_root;
    nlist_root->prev = tprev;
    tprev->next = nlist_root;
    DBUG_PRINT("qcache",
	       ("list_root: 0x%lx tnext 0x%lx tprev 0x%lx tprev->next 0x%lx tnext->prev 0x%lx",
		(ulong) list_root, (ulong) tnext, (ulong) tprev,
		(ulong)tprev->next, (ulong)tnext->prev));
    /*
      Go through all queries that uses this table and change them to
      point to the new table object
    */
    Query_cache_table *new_block_table=new_block->table();
    for (;tnext != nlist_root; tnext=tnext->next)
      tnext->parent= new_block_table;
    *border += len;
    *before = new_block;
    /* Fix pointer to table name */
    new_block->table()->table(new_block->table()->db() + tablename_offset);
    /* Fix hash to point at moved block */
    hash_replace(&tables, tables.current_record, (byte*) new_block);

    DBUG_PRINT("qcache", ("moved %lu bytes to 0x%lx, new gap at 0x%lx",
			len, (ulong) new_block, (ulong) *border));
    break;
  }
  case Query_cache_block::QUERY:
  {
    DBUG_PRINT("qcache", ("block 0x%lx QUERY", (ulong) block));
    if (*border == 0)
      break;
    BLOCK_LOCK_WR(block);
    ulong len = block->length, used = block->used;
    TABLE_COUNTER_TYPE n_tables = block->n_tables;
    Query_cache_block	*prev = block->prev,
			*next = block->next,
			*pprev = block->pprev,
			*pnext = block->pnext,
			*new_block =(Query_cache_block*) *border;
    char *data = (char*) block->data();
    Query_cache_block *first_result_block = ((Query_cache_query *)
					     block->data())->result();
    byte *key;
    uint key_length;
    key=query_cache_query_get_key((byte*) block, &key_length, 0);
    hash_search(&queries, (byte*) key, key_length);
    // Move table of used tables 
    memmove((char*) new_block->table(0), (char*) block->table(0),
	   ALIGN_SIZE(n_tables*sizeof(Query_cache_block_table)));
    block->query()->unlock_n_destroy();
    block->destroy();
    new_block->init(len);
    new_block->type=Query_cache_block::QUERY;
    new_block->used=used;
    new_block->n_tables=n_tables;
    memmove((char*) new_block->data(), data, len - new_block->headers_len());
    relink(block, new_block, next, prev, pnext, pprev);
    if (queries_blocks == block)
      queries_blocks = new_block;
    Query_cache_block_table *beg_of_table_table= block->table(0),
      *end_of_table_table= block->table(n_tables);
    byte *beg_of_new_table_table= (byte*) new_block->table(0);
      
    for (TABLE_COUNTER_TYPE j=0; j < n_tables; j++)
    {
      Query_cache_block_table *block_table = new_block->table(j);

      // use aligment from begining of table if 'next' is in same block
      if ((beg_of_table_table <= block_table->next) &&
	  (block_table->next < end_of_table_table))
	((Query_cache_block_table *)(beg_of_new_table_table + 
				     (((byte*)block_table->next) -
				      ((byte*)beg_of_table_table))))->prev=
	 block_table;
      else
	block_table->next->prev= block_table;

      // use aligment from begining of table if 'prev' is in same block
      if ((beg_of_table_table <= block_table->prev) &&
	  (block_table->prev < end_of_table_table))
	((Query_cache_block_table *)(beg_of_new_table_table + 
				     (((byte*)block_table->prev) -
				      ((byte*)beg_of_table_table))))->next=
	  block_table;
      else
	block_table->prev->next = block_table;
    }
    DBUG_PRINT("qcache", ("after circle tt"));
    *border += len;
    *before = new_block;
    new_block->query()->result(first_result_block);
    if (first_result_block != 0)
    {
      Query_cache_block *result_block = first_result_block;
      do
      {
	result_block->result()->parent(new_block);
	result_block = result_block->next;
      } while ( result_block != first_result_block );
    }
    Query_cache_query *new_query= ((Query_cache_query *) new_block->data());
    my_rwlock_init(&new_query->lock, NULL);

    /* 
      If someone is writing to this block, inform the writer that the block
      has been moved.
    */
    NET *net = new_block->query()->writer();
    if (net != 0)
    {
      net->query_cache_query= (gptr) new_block;
    }
    /* Fix hash to point at moved block */
    hash_replace(&queries, queries.current_record, (byte*) new_block);
    DBUG_PRINT("qcache", ("moved %lu bytes to 0x%lx, new gap at 0x%lx",
			len, (ulong) new_block, (ulong) *border));
    break;
  }
  case Query_cache_block::RES_INCOMPLETE:
  case Query_cache_block::RES_BEG:
  case Query_cache_block::RES_CONT:
  case Query_cache_block::RESULT:
  {
    DBUG_PRINT("qcache", ("block 0x%lx RES* (%d)", (ulong) block,
			(int) block->type));
    if (*border == 0)
      break;
    Query_cache_block *query_block = block->result()->parent(),
		      *next = block->next,
		      *prev = block->prev;
    Query_cache_block::block_type type = block->type;
    BLOCK_LOCK_WR(query_block);
    ulong len = block->length, used = block->used;
    Query_cache_block *pprev = block->pprev,
		      *pnext = block->pnext,
		      *new_block =(Query_cache_block*) *border;
    char *data = (char*) block->data();
    block->destroy();
    new_block->init(len);
    new_block->type=type;
    new_block->used=used;
    memmove((char*) new_block->data(), data, len - new_block->headers_len());
    relink(block, new_block, next, prev, pnext, pprev);
    new_block->result()->parent(query_block);
    Query_cache_query *query = query_block->query();
    if (query->result() == block)
      query->result(new_block);
    *border += len;
    *before = new_block;
    /* If result writing complete && we have free space in block */
    ulong free_space= new_block->length - new_block->used;
    free_space-= free_space % ALIGN_SIZE(1);
    if (query->result()->type == Query_cache_block::RESULT &&
	new_block->length > new_block->used &&
	*gap + free_space > min_allocation_unit &&
	new_block->length - free_space > min_allocation_unit)
    {
      *border-= free_space;
      *gap+= free_space;
      DBUG_PRINT("qcache",
		 ("rest of result free space added to gap (%lu)", *gap));
      new_block->length -= free_space;
    }
    BLOCK_UNLOCK_WR(query_block);
    DBUG_PRINT("qcache", ("moved %lu bytes to 0x%lx, new gap at 0x%lx",
			len, (ulong) new_block, (ulong) *border));
    break;
  }
  default:
    DBUG_PRINT("error", ("unexpected block type %d, block 0x%lx",
			 (int)block->type, (ulong) block));
    ok = 0;
  }
  DBUG_RETURN(ok);
}


void Query_cache::relink(Query_cache_block *oblock,
			 Query_cache_block *nblock,
			 Query_cache_block *next, Query_cache_block *prev,
			 Query_cache_block *pnext, Query_cache_block *pprev)
{
  if (prev == oblock) //check pointer to himself
  {
    nblock->prev = nblock;
    nblock->next = nblock;
  }
  else
  {
    nblock->prev = prev;
    prev->next=nblock;
  }
  if (next != oblock)
  {
    nblock->next = next;
    next->prev=nblock;
  }
  nblock->pprev = pprev; // Physical pointer to himself have only 1 free block
  nblock->pnext = pnext;
  pprev->pnext=nblock;
  pnext->pprev=nblock;
}


my_bool Query_cache::join_results(ulong join_limit)
{
  my_bool has_moving = 0;
  DBUG_ENTER("Query_cache::join_results");

  STRUCT_LOCK(&structure_guard_mutex);
  if (queries_blocks != 0)
  {
    DBUG_ASSERT(query_cache_size > 0);
    Query_cache_block *block = queries_blocks;
    do
    {
      Query_cache_query *header = block->query();
      if (header->result() != 0 &&
	  header->result()->type == Query_cache_block::RESULT &&
	  header->length() > join_limit)
      {
	Query_cache_block *new_result_block =
	  get_free_block(ALIGN_SIZE(header->length()) +
			 ALIGN_SIZE(sizeof(Query_cache_block)) +
			 ALIGN_SIZE(sizeof(Query_cache_result)), 1, 0);
	if (new_result_block != 0)
	{
	  has_moving = 1;
	  Query_cache_block *first_result = header->result();
	  ulong new_len = (header->length() +
			   ALIGN_SIZE(sizeof(Query_cache_block)) +
			   ALIGN_SIZE(sizeof(Query_cache_result)));
	  if (new_result_block->length >
	      ALIGN_SIZE(new_len) + min_allocation_unit)
	    split_block(new_result_block, ALIGN_SIZE(new_len));
	  BLOCK_LOCK_WR(block);
	  header->result(new_result_block);
	  new_result_block->type = Query_cache_block::RESULT;
	  new_result_block->n_tables = 0;
	  new_result_block->used = new_len;

	  new_result_block->next = new_result_block->prev = new_result_block;
	  DBUG_PRINT("qcache", ("new block %lu/%lu (%lu)",
			      new_result_block->length,
			      new_result_block->used,
			      header->length()));

	  Query_cache_result *new_result = new_result_block->result();
	  new_result->parent(block);
	  byte *write_to = (byte*) new_result->data();
	  Query_cache_block *result_block = first_result;
	  do
	  {
	    ulong len = (result_block->used - result_block->headers_len() -
			 ALIGN_SIZE(sizeof(Query_cache_result)));
	    DBUG_PRINT("loop", ("add block %lu/%lu (%lu)",
				result_block->length,
				result_block->used,
				len));
	    memcpy((char *) write_to,
		   (char*) result_block->result()->data(),
		   len);
	    write_to += len;
	    Query_cache_block *old_result_block = result_block;
	    result_block = result_block->next;
	    free_memory_block(old_result_block);
	  } while (result_block != first_result);
	  BLOCK_UNLOCK_WR(block);
	}
      }
      block = block->next;
    } while ( block != queries_blocks );
  }
  STRUCT_UNLOCK(&structure_guard_mutex);
  DBUG_RETURN(has_moving);
}


uint Query_cache::filename_2_table_key (char *key, const char *path,
					uint32 *db_length)
{
  char tablename[FN_REFLEN+2], *filename, *dbname;
  DBUG_ENTER("Query_cache::filename_2_table_key");

  /* Safety if filename didn't have a directory name */
  tablename[0]= FN_LIBCHAR;
  tablename[1]= FN_LIBCHAR;
  /* Convert filename to this OS's format in tablename */
  fn_format(tablename + 2, path, "", "", MY_REPLACE_EXT);
  filename=  tablename + dirname_length(tablename + 2) + 2;
  /* Find start of databasename */
  for (dbname= filename - 2 ; dbname[-1] != FN_LIBCHAR ; dbname--) ;
  *db_length= (filename - dbname) - 1;
  DBUG_PRINT("qcache", ("table '%-.*s.%s'", *db_length, dbname, filename));

  DBUG_RETURN((uint) (strmov(strmake(key, dbname, *db_length) + 1,
			     filename) -key) + 1);
}

/****************************************************************************
  Functions to be used when debugging
****************************************************************************/

#if defined(DBUG_OFF) && !defined(USE_QUERY_CACHE_INTEGRITY_CHECK)

void wreck(uint line, const char *message) {}
void bins_dump() {}
void cache_dump() {}
void queries_dump() {}
void tables_dump() {}
my_bool check_integrity(bool not_locked) { return 0; }
my_bool in_list(Query_cache_block * root, Query_cache_block * point,
		const char *name) { return 0;}
my_bool in_blocks(Query_cache_block * point) { return 0; }

#else

void Query_cache::wreck(uint line, const char *message)
{
  THD *thd=current_thd;
  DBUG_ENTER("Query_cache::wreck");
  query_cache_size = 0;
  if (*message)
    DBUG_PRINT("error", (" %s", message));
  DBUG_PRINT("warning", ("=================================="));
  DBUG_PRINT("warning", ("%5d QUERY CACHE WRECK => DISABLED",line));
  DBUG_PRINT("warning", ("=================================="));
  if (thd)
    thd->killed= THD::KILL_CONNECTION;
  cache_dump();
  /* check_integrity(0); */ /* Can't call it here because of locks */
  bins_dump();
  DBUG_VOID_RETURN;
}


void Query_cache::bins_dump()
{
  uint i;
  
  if (!initialized || query_cache_size == 0)
  {
    DBUG_PRINT("qcache", ("Query Cache not initialized"));
    return;
  }

  DBUG_PRINT("qcache", ("mem_bin_num=%u, mem_bin_steps=%u",
		      mem_bin_num, mem_bin_steps));
  DBUG_PRINT("qcache", ("-------------------------"));
  DBUG_PRINT("qcache", ("      size idx       step"));
  DBUG_PRINT("qcache", ("-------------------------"));
  for (i=0; i < mem_bin_steps; i++)
  {
    DBUG_PRINT("qcache", ("%10lu %3d %10lu", steps[i].size, steps[i].idx,
			steps[i].increment));
  }
  DBUG_PRINT("qcache", ("-------------------------"));
  DBUG_PRINT("qcache", ("      size num"));
  DBUG_PRINT("qcache", ("-------------------------"));
  for (i=0; i < mem_bin_num; i++)
  {
    DBUG_PRINT("qcache", ("%10lu %3d 0x%lx", bins[i].size, bins[i].number,
			(ulong)&(bins[i])));
    if (bins[i].free_blocks)
    {
      Query_cache_block *block = bins[i].free_blocks;
      do{
	DBUG_PRINT("qcache", ("\\-- %lu 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
			    block->length, (ulong)block,
			    (ulong)block->next, (ulong)block->prev,
			    (ulong)block->pnext, (ulong)block->pprev));
	block = block->next;
      } while ( block != bins[i].free_blocks );
    }
  }
  DBUG_PRINT("qcache", ("-------------------------"));
}


void Query_cache::cache_dump()
{
  if (!initialized || query_cache_size == 0)
  {
    DBUG_PRINT("qcache", ("Query Cache not initialized"));
    return;
  }

  DBUG_PRINT("qcache", ("-------------------------------------"));
  DBUG_PRINT("qcache", ("    length       used t nt"));
  DBUG_PRINT("qcache", ("-------------------------------------"));
  Query_cache_block *i = first_block;
  do
  {
    DBUG_PRINT("qcache",
	       ("%10lu %10lu %1d %2d 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
		i->length, i->used, (int)i->type,
		i->n_tables, (ulong)i,
		(ulong)i->next, (ulong)i->prev, (ulong)i->pnext,
		(ulong)i->pprev));
    i = i->pnext;
  } while ( i != first_block );
  DBUG_PRINT("qcache", ("-------------------------------------"));
}


void Query_cache::queries_dump()
{

  if (!initialized)
  {
    DBUG_PRINT("qcache", ("Query Cache not initialized"));
    return;
  }

  DBUG_PRINT("qcache", ("------------------"));
  DBUG_PRINT("qcache", (" QUERIES"));
  DBUG_PRINT("qcache", ("------------------"));
  if (queries_blocks != 0)
  {
    Query_cache_block *block = queries_blocks;
    do
    {
      uint len;
      char *str = (char*) query_cache_query_get_key((byte*) block, &len, 0);
      len-= QUERY_CACHE_FLAGS_SIZE;		  // Point at flags
      Query_cache_query_flags flags;
      memcpy(&flags, str+len, QUERY_CACHE_FLAGS_SIZE);
      str[len]= 0; // make zero ending DB name
      DBUG_PRINT("qcache", ("F:%u C:%u L:%lu T:'%s' (%u) '%s' '%s'",
			    flags.client_long_flag,
			    flags.character_set_client_num, 
                            (ulong)flags.limit, flags.time_zone->get_name(),
			    len, str, strend(str)+1));
      DBUG_PRINT("qcache", ("-b- 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx", (ulong) block,
			    (ulong) block->next, (ulong) block->prev,
			    (ulong)block->pnext, (ulong)block->pprev));
      memcpy(str + len, &flags, QUERY_CACHE_FLAGS_SIZE); // restore flags
      for (TABLE_COUNTER_TYPE t= 0; t < block->n_tables; t++)
      {
	Query_cache_table *table= block->table(t)->parent;
	DBUG_PRINT("qcache", ("-t- '%s' '%s'", table->db(), table->table()));
      }
      Query_cache_query *header = block->query();
      if (header->result())
      {
	Query_cache_block *result_block = header->result();
	Query_cache_block *result_beg = result_block;
	do
	{
	  DBUG_PRINT("qcache", ("-r- %u %lu/%lu 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
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
    } while ((block=block->next) != queries_blocks);
  }
  else
  {
    DBUG_PRINT("qcache", ("no queries in list"));
  }
  DBUG_PRINT("qcache", ("------------------"));
}


void Query_cache::tables_dump()
{
  if (!initialized || query_cache_size == 0)
  {
    DBUG_PRINT("qcache", ("Query Cache not initialized"));
    return;
  }

  DBUG_PRINT("qcache", ("--------------------"));
  DBUG_PRINT("qcache", ("TABLES"));
  DBUG_PRINT("qcache", ("--------------------"));
  if (tables_blocks != 0)
  {
    Query_cache_block *table_block = tables_blocks;
    do
    {
      Query_cache_table *table = table_block->table();
      DBUG_PRINT("qcache", ("'%s' '%s'", table->db(), table->table()));
      table_block = table_block->next;
    } while ( table_block != tables_blocks);
  }
  else
    DBUG_PRINT("qcache", ("no tables in list"));
  DBUG_PRINT("qcache", ("--------------------"));
}


my_bool Query_cache::check_integrity(bool not_locked)
{
  my_bool result = 0;
  uint i;
  DBUG_ENTER("check_integrity");

  if (query_cache_size == 0)
  {
    DBUG_PRINT("qcache", ("Query Cache not initialized"));
    DBUG_RETURN(0);
  }
  if (!not_locked)
  {
    STRUCT_LOCK(&structure_guard_mutex);
    /*
      It is very unlikely that following condition is TRUE (it is possible
      only if other thread is resizing cache), so we check it only after
      guard mutex lock
    */
    if (unlikely(query_cache_size == 0))
    {
      STRUCT_UNLOCK(&query_cache.structure_guard_mutex);
      DBUG_RETURN(0);
    }
  }

  if (hash_check(&queries))
  {
    DBUG_PRINT("error", ("queries hash is damaged"));
    result = 1;
  }

  if (hash_check(&tables))
  {
    DBUG_PRINT("error", ("tables hash is damaged"));
    result = 1;
  }

  DBUG_PRINT("qcache", ("physical address check ..."));
  ulong free=0, used=0;
  Query_cache_block * block = first_block;
  do
  {
    DBUG_PRINT("qcache", ("block 0x%lx, type %u...", 
			  (ulong) block, (uint) block->type));  
    // Check allignment
    if ((((long)block) % (long) ALIGN_SIZE(1)) !=
	(((long)first_block) % (long)ALIGN_SIZE(1)))
    {
      DBUG_PRINT("error",
		 ("block 0x%lx do not aligned by %d", (ulong) block,
		  ALIGN_SIZE(1)));
      result = 1;
    }
    // Check memory allocation
    if (block->pnext == first_block) // Is it last block?
    {
      if (((byte*)block) + block->length != 
	  ((byte*)first_block) + query_cache_size)
      {
	DBUG_PRINT("error", 
		   ("block 0x%lx, type %u, ended at 0x%lx, but cache ended at 0x%lx",
		    (ulong) block, (uint) block->type, 
		    (ulong) (((byte*)block) + block->length),
		    (ulong) (((byte*)first_block) + query_cache_size)));
	result = 1;
      }
    }
    else
      if (((byte*)block) + block->length != ((byte*)block->pnext))
      {
	DBUG_PRINT("error", 
		   ("block 0x%lx, type %u, ended at 0x%lx, but next block begining at 0x%lx",
		    (ulong) block, (uint) block->type, 
		    (ulong) (((byte*)block) + block->length),
		    (ulong) ((byte*)block->pnext)));
      }
    if (block->type == Query_cache_block::FREE)
      free+= block->length;
    else
      used+= block->length;
    switch(block->type) {
    case Query_cache_block::FREE:
    {
      Query_cache_memory_bin *bin = *((Query_cache_memory_bin **)
				      block->data());
      //is it correct pointer?
      if (((byte*)bin) < ((byte*)bins) ||
	  ((byte*)bin) >= ((byte*)first_block))
      {
	DBUG_PRINT("error", 
		   ("free block 0x%lx have bin pointer 0x%lx beyaond of bins array bounds [0x%lx,0x%lx]",
		    (ulong) block, 
		    (ulong) bin,
		    (ulong) bins,
		    (ulong) first_block));
	result = 1;
      }
      else
      {
	int idx = (((byte*)bin) - ((byte*)bins)) /
	  sizeof(Query_cache_memory_bin);
	if (in_list(bins[idx].free_blocks, block, "free memory"))
	  result = 1;
      }
      break;
    }
    case Query_cache_block::TABLE:
      if (in_list(tables_blocks, block, "tables"))
	result = 1;
      if (in_table_list(block->table(0),  block->table(0), "table list root"))
	result = 1;
      break;
    case Query_cache_block::QUERY:
    {
      if (in_list(queries_blocks, block, "query"))
	result = 1;
      for (TABLE_COUNTER_TYPE j=0; j < block->n_tables; j++)
      {
	Query_cache_block_table *block_table = block->table(j);
	Query_cache_block_table *block_table_root = 
	  (Query_cache_block_table *) 
	  (((byte*)block_table->parent) -
	   ALIGN_SIZE(sizeof(Query_cache_block_table)));
	
    	if (in_table_list(block_table, block_table_root, "table list"))
    	  result = 1;
      }
      break;
    }
    case Query_cache_block::RES_INCOMPLETE:
      // This type of block can be not lincked yet (in multithread environment)
      break;
    case Query_cache_block::RES_BEG:
    case Query_cache_block::RES_CONT:
    case Query_cache_block::RESULT:
    {
      Query_cache_block * query_block = block->result()->parent();
      if (((byte*)query_block) < ((byte*)first_block) ||
	  ((byte*)query_block) >= (((byte*)first_block) + query_cache_size))
      {
	DBUG_PRINT("error", 
		   ("result block 0x%lx have query block pointer 0x%lx beyaond of block pool bounds [0x%lx,0x%lx]",
		    (ulong) block,
		    (ulong) query_block,
		    (ulong) first_block,
		    (ulong) (((byte*)first_block) + query_cache_size)));
	result = 1;
      }
      else
      {
	BLOCK_LOCK_RD(query_block);
	if (in_list(queries_blocks, query_block, "query from results"))
	  result = 1;
	if (in_list(query_block->query()->result(), block,
		    "results"))
	  result = 1;
	BLOCK_UNLOCK_RD(query_block);
      }
      break;
    }
    default:
      DBUG_PRINT("error",
		 ("block 0x%lx have incorrect type %u",
		  block, block->type));
      result = 1;
    }
    
    block = block->pnext;
  } while (block != first_block);
  
  if (used + free != query_cache_size)
  {
    DBUG_PRINT("error",
	       ("used memory (%lu) + free memory (%lu) !=  query_cache_size (%lu)",
		used, free, query_cache_size));
    result = 1;
  }
  
  if (free != free_memory)
  {
    DBUG_PRINT("error",
	       ("free memory (%lu) != free_memory (%lu)",
		free, free_memory));
    result = 1;
  }

  DBUG_PRINT("qcache", ("check queries ..."));
  if ((block = queries_blocks))
  {
    do
    {
      DBUG_PRINT("qcache", ("block 0x%lx, type %u...", 
			    (ulong) block, (uint) block->type));
      uint length;
      byte *key = query_cache_query_get_key((byte*) block, &length, 0);
      gptr val = hash_search(&queries, key, length);
      if (((gptr)block) != val)
      {
	DBUG_PRINT("error", ("block 0x%lx found in queries hash like 0x%lx",
			     (ulong) block, (ulong) val));
      }
      if (in_blocks(block))
	result = 1;
      Query_cache_block * results = block->query()->result();
      if (results)
      {
	Query_cache_block * result_block = results;
	do
	{
	  DBUG_PRINT("qcache", ("block 0x%lx, type %u...", 
				(ulong) block, (uint) block->type));
	  if (in_blocks(result_block))
	    result = 1;

	  result_block = result_block->next;
	} while (result_block != results);
      }
      block = block->next;
    } while (block != queries_blocks);
  }

  DBUG_PRINT("qcache", ("check tables ..."));
  if ((block = tables_blocks))
  {
    do
    {
      DBUG_PRINT("qcache", ("block 0x%lx, type %u...", 
			    (ulong) block, (uint) block->type));
      uint length;
      byte *key = query_cache_table_get_key((byte*) block, &length, 0);
      gptr val = hash_search(&tables, key, length);
      if (((gptr)block) != val)
      {
	DBUG_PRINT("error", ("block 0x%lx found in tables hash like 0x%lx",
			     (ulong) block, (ulong) val));
      }
      
      if (in_blocks(block))
	result = 1;
      block=block->next;
    } while (block != tables_blocks);
  }

  DBUG_PRINT("qcache", ("check free blocks"));
  for (i = 0; i < mem_bin_num; i++)
  {
    if ((block = bins[i].free_blocks))
    {
      uint count = 0;
      do
      {
	DBUG_PRINT("qcache", ("block 0x%lx, type %u...", 
			      (ulong) block, (uint) block->type));
	if (in_blocks(block))
	  result = 1;
	
	count++;
	block=block->next;
      } while (block != bins[i].free_blocks);
      if (count != bins[i].number)
      {
	DBUG_PRINT("error", ("bin[%d].number is %d, but bin have %d blocks",
			     bins[i].number,  count));
	result = 1;
      }
    }
  }
  DBUG_ASSERT(result == 0);
  if (!not_locked)
    STRUCT_UNLOCK(&structure_guard_mutex);
  DBUG_RETURN(result);
}


my_bool Query_cache::in_blocks(Query_cache_block * point)
{
  my_bool result = 0;
  Query_cache_block *block = point;
  //back
  do
  {
    if (block->pprev->pnext != block)
    {
      DBUG_PRINT("error",
		 ("block 0x%lx in physical list is incorrect linked, prev block 0x%lx refered as next to 0x%lx (check from 0x%lx)",
		  (ulong) block, (ulong) block->pprev,
		  (ulong) block->pprev->pnext,
		  (ulong) point));
      //back trace
      for (; block != point; block = block->pnext)
	    DBUG_PRINT("error", ("back trace 0x%lx", (ulong) block));
      result = 1;
      goto err1;
    }
    block = block->pprev;
  } while (block != first_block && block != point);
  if (block != first_block)
  {
    DBUG_PRINT("error",
	       ("block 0x%lx (0x%lx<-->0x%lx) not owned by pysical list",
		(ulong) block, (ulong) block->pprev, (ulong )block->pnext));
    return 1;
  }

err1:
  //forward
  block = point;
  do
  {
    if (block->pnext->pprev != block)
    {
      DBUG_PRINT("error",
		 ("block 0x%lx in physicel list is incorrect linked, next block 0x%lx refered as prev to 0x%lx (check from 0x%lx)",
		  (ulong) block, (ulong) block->pnext,
		  (ulong) block->pnext->pprev,
		  (ulong) point));
      //back trace
      for (; block != point; block = block->pprev)
	    DBUG_PRINT("error", ("back trace 0x%lx", (ulong) block));
      result = 1;
      goto err2;
    }
    block = block->pnext;
  } while (block != first_block);
err2:
  return result;
}


my_bool Query_cache::in_list(Query_cache_block * root,
			     Query_cache_block * point,
			     const char *name)
{
  my_bool result = 0;
  Query_cache_block *block = point;
  //back
  do
  {
    if (block->prev->next != block)
    {
      DBUG_PRINT("error",
		 ("block 0x%lx in list '%s' 0x%lx is incorrect linked, prev block 0x%lx refered as next to 0x%lx (check from 0x%lx)",
		  (ulong) block, name, (ulong) root, (ulong) block->prev,
		  (ulong) block->prev->next,
		  (ulong) point));
      //back trace
      for (; block != point; block = block->next)
	    DBUG_PRINT("error", ("back trace 0x%lx", (ulong) block));
      result = 1;
      goto err1;
    }
    block = block->prev;
  } while (block != root && block != point);
  if (block != root)
  {
    DBUG_PRINT("error",
	       ("block 0x%lx (0x%lx<-->0x%lx) not owned by list '%s' 0x%lx",
		(ulong) block, 
		(ulong) block->prev, (ulong) block->next,
		name, (ulong) root));
    return 1;
  }
err1:
  // forward
  block = point;
  do
  {
    if (block->next->prev != block)
    {
      DBUG_PRINT("error",
		 ("block 0x%lx in list '%s' 0x%lx is incorrect linked, next block 0x%lx refered as prev to 0x%lx (check from 0x%lx)",
		  (ulong) block, name, (ulong) root, (ulong) block->next,
		  (ulong) block->next->prev,
		  (ulong) point));
      //back trace
      for (; block != point; block = block->prev)
	    DBUG_PRINT("error", ("back trace 0x%lx", (ulong) block));
      result = 1;
      goto err2;
    }
    block = block->next;
  } while (block != root);
err2:
  return result;
}

void dump_node(Query_cache_block_table * node, 
	       const char * call, const char * descr)
{
  DBUG_PRINT("qcache", ("%s: %s: node: 0x%lx", call, descr, (ulong) node));
  DBUG_PRINT("qcache", ("%s: %s: node block: 0x%lx",
			call, descr, (ulong) node->block()));
  DBUG_PRINT("qcache", ("%s: %s: next: 0x%lx", call, descr,
			(ulong) node->next));
  DBUG_PRINT("qcache", ("%s: %s: prev: 0x%lx", call, descr,
			(ulong) node->prev));
}

my_bool Query_cache::in_table_list(Query_cache_block_table * root,
				   Query_cache_block_table * point,
				   const char *name)
{
  my_bool result = 0;
  Query_cache_block_table *table = point;
  dump_node(root, name, "parameter root");
  //back
  do
  {
    dump_node(table, name, "list element << ");
    if (table->prev->next != table)
    {
      DBUG_PRINT("error",
		 ("table 0x%lx(0x%lx) in list '%s' 0x%lx(0x%lx) is incorrect linked, prev table 0x%lx(0x%lx) refered as next to 0x%lx(0x%lx) (check from 0x%lx(0x%lx))",
		  (ulong) table, (ulong) table->block(), name, 
		  (ulong) root, (ulong) root->block(),
		  (ulong) table->prev, (ulong) table->prev->block(),
		  (ulong) table->prev->next, 
		  (ulong) table->prev->next->block(),
		  (ulong) point, (ulong) point->block()));
      //back trace
      for (; table != point; table = table->next)
	    DBUG_PRINT("error", ("back trace 0x%lx(0x%lx)", 
				 (ulong) table, (ulong) table->block()));
      result = 1;
      goto err1;
    }
    table = table->prev;
  } while (table != root && table != point);
  if (table != root)
  {
    DBUG_PRINT("error",
	       ("table 0x%lx(0x%lx) (0x%lx(0x%lx)<-->0x%lx(0x%lx)) not owned by list '%s' 0x%lx(0x%lx)",
		(ulong) table, (ulong) table->block(),
		(ulong) table->prev, (ulong) table->prev->block(),
		(ulong) table->next, (ulong) table->next->block(),
		name, (ulong) root, (ulong) root->block()));
    return 1;
  }
err1:
  // forward
  table = point;
  do
  {
    dump_node(table, name, "list element >> ");
    if (table->next->prev != table)
    {
      DBUG_PRINT("error",
		 ("table 0x%lx(0x%lx) in list '%s' 0x%lx(0x%lx) is incorrect linked, next table 0x%lx(0x%lx) refered as prev to 0x%lx(0x%lx) (check from 0x%lx(0x%lx))",
		  (ulong) table, (ulong) table->block(),
		  name, (ulong) root, (ulong) root->block(),
		  (ulong) table->next, (ulong) table->next->block(),
		  (ulong) table->next->prev,
		  (ulong) table->next->prev->block(),
		  (ulong) point, (ulong) point->block()));
      //back trace
      for (; table != point; table = table->prev)
	    DBUG_PRINT("error", ("back trace 0x%lx(0x%lx)",
				 (ulong) table, (ulong) table->block()));
      result = 1;
      goto err2;
    }
    table = table->next;
  } while (table != root);
err2:
  return result;
}

#endif /* DBUG_OFF */

#endif /*HAVE_QUERY_CACHE*/
