/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

7. Interface
The query cache interfaces with the rest of the server code through 7
functions:
 1. Query_cache::send_result_to_client
       - Called before parsing and used to match a statement with the stored
         queries hash.
         If a match is found the cached result set is sent through repeated
         calls to net_write_packet. (note: calling thread doesn't have a regis-
         tered result set writer: thd->net.query_cache_query=0)
 2. Query_cache::store_query
       - Called just before handle_select() and is used to register a result
         set writer to the statement currently being processed
         (thd->net.query_cache_query).
 3. query_cache_insert
       - Called from net_write_packet to append a result set to a cached query
         if (and only if) this query has a registered result set writer
         (thd->net.query_cache_query).
 4. Query_cache::invalidate
    Query_cache::invalidate_locked_for_write
       - Called from various places to invalidate query cache based on data-
         base, table and myisam file name. During an on going invalidation
         the query cache is temporarily disabled.
 5. Query_cache::flush
       - Used when a RESET QUERY CACHE is issued. This clears the entire
         cache block by block.
 6. Query_cache::resize
       - Used to change the available memory used by the query cache. This
         will also invalidate the entrie query cache in one free operation.
 7. Query_cache::pack
       - Used when a FLUSH QUERY CACHE is issued. This changes the order of
         the used memory blocks in physical memory order and move all avail-
         able memory to the 'bottom' of the memory.


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

    - Another option would be to set thd->lex->safe_to_cache_query to false
      in 'get_lock_data' if any of the tables was a tmp table or a
      MRG_ISAM table.
      (This could be done with almost no speed penalty)
*/

#include "sql_cache.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <algorithm>

#include "../storage/myisam/myisamdef.h"       // st_myisam_info
#include "../storage/myisammrg/ha_myisammrg.h" // ha_myisammrg
#include "auth_acls.h"
#include "auth_common.h"      // check_table_access
#include "current_thd.h"      // current_thd
#include "debug_sync.h"       // DEBUG_SYNC
#include "handler.h"
#include "key.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_systime.h"
#include "myisammrg.h"        // MYRG_INFO
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/psi/psi_stage.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql_com.h"
#include "mysqld.h"           // key_structure_guard_mutex
#include "opt_trace.h"        // Opt_trace_stmt
#include "protocol.h"
#include "protocol_classic.h"
#include "psi_memory_key.h"   // key_memory_queue_item
#include "query_options.h"
#include "session_tracker.h"
#include "set_var.h"
#include "sql_base.h"         // get_table_def_key
#include "sql_class.h"        // THD
#include "sql_const.h"
#include "sql_error.h"
#include "sql_lex.h"
#include "sql_plugin.h"
#include "sql_plugin_ref.h"
#include "sql_table.h"        // build_table_filename
#include "system_variables.h"
#include "table.h"
#include "thr_lock.h"
#include "thr_mutex.h"
#include "transaction.h"      // trans_rollback_stmt
#include "transaction_info.h"
#include "xa.h"

class MY_LOCALE;
class Time_zone;

using std::min;
using std::max;

/*
   Can't create new free memory block if unused memory in block less
   then QUERY_CACHE_MIN_ALLOCATION_UNIT.
   if QUERY_CACHE_MIN_ALLOCATION_UNIT == 0 then
   QUERY_CACHE_MIN_ALLOCATION_UNIT choosed automaticaly
*/
static const ulong QUERY_CACHE_MIN_ALLOCATION_UNIT= 512;

/* inittial size of hashes */
static const uint QUERY_CACHE_DEF_QUERY_HASH_SIZE= 1024;
static const uint QUERY_CACHE_DEF_TABLE_HASH_SIZE= 1024;

/* packing parameters */
static const uint QUERY_CACHE_PACK_ITERATION= 2;
static const ulong QUERY_CACHE_PACK_LIMIT= 512*1024L;

/*
   start estimation of first result block size only when number of queries
   bigger then:
*/
static const ulong QUERY_CACHE_MIN_ESTIMATED_QUERIES_NUMBER= 3;

/* memory bins size spacing (see at Query_cache::init_cache (sql_cache.cc)) */
static const uint QUERY_CACHE_MEM_BIN_FIRST_STEP_PWR2= 4;
static const uint QUERY_CACHE_MEM_BIN_STEP_PWR2= 2;
static const uint QUERY_CACHE_MEM_BIN_PARTS_INC= 1;
static const float QUERY_CACHE_MEM_BIN_PARTS_MUL= 1.2f;
static const uint QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2= 3;

/* how many free blocks check when finding most suitable before other 'end'
   of list of free blocks */
static const uint QUERY_CACHE_MEM_BIN_TRY= 5;

// Max aligned size for ulong type query_cache_min_res_unit.
static const ulong max_aligned_min_res_unit_size= ((ULONG_MAX) &
                                                   (~(sizeof(double) - 1)));

/* Exclude/include from cyclic double linked list */
static void double_linked_list_exclude(Query_cache_block *point,
                                       Query_cache_block **list_pointer);
static void double_linked_list_simple_include(Query_cache_block *point,
                                              Query_cache_block **
                                              list_pointer);
static void double_linked_list_join(Query_cache_block *head_tail,
                                    Query_cache_block *tail_head);

/* Table key generation */
static size_t filename_2_table_key(char *key, const char *filename,
                                   size_t *db_length);


static void relink(Query_cache_block *oblock,
                   Query_cache_block *nblock,
                   Query_cache_block *next,
                   Query_cache_block *prev,
                   Query_cache_block *pnext,
                   Query_cache_block *pprev);

static bool ask_handler_allowance(THD *thd, TABLE_LIST *tables_used);


struct Query_cache_result
{
  Query_cache_result() {}                     /* Remove gcc warning */
  /* data_continue (if not whole packet contained by this block) */
  Query_cache_block *parent;

  uchar* data()
  {
    return reinterpret_cast<uchar*>(this) + ALIGN_SIZE(sizeof(Query_cache_result));
  }
};


struct Query_cache_memory_bin
{
  Query_cache_memory_bin() {}                 /* Remove gcc warning */
  Query_cache_block *free_blocks;
#ifndef DBUG_OFF
  ulong size;
#endif
  uint number;

  void init(ulong size_arg MY_ATTRIBUTE((unused)))
  {
#ifndef DBUG_OFF
    size = size_arg;
#endif
    number = 0;
    free_blocks= NULL;
  }
};


struct Query_cache_memory_bin_step
{
  Query_cache_memory_bin_step() {}            /* Remove gcc warning */
  ulong size;
  ulong increment;
  uint idx;
  void init(ulong size_arg, uint idx_arg, ulong increment_arg)
  {
    size = size_arg;
    idx = idx_arg;
    increment = increment_arg;
  }
};


/**
  Thread state to be used when the query cache lock needs to be acquired.
  Sets the thread state name in the constructor, resets on destructor.
*/

class Query_cache_wait_state
{
private:
  THD *m_thd;
  PSI_stage_info m_old_stage;
  const char *m_func;
  const char *m_file;
  uint m_line;

public:
  Query_cache_wait_state(THD *thd, const char *func,
                         const char *file, uint line)
  : m_thd(thd),
    m_old_stage(),
    m_func(func), m_file(file), m_line(line)
  {
    if (m_thd)
      m_thd->enter_stage(&stage_waiting_for_query_cache_lock,
                         &m_old_stage,
                         m_func, m_file, m_line);
  }

  ~Query_cache_wait_state()
  {
    if (m_thd)
      m_thd->enter_stage(&m_old_stage, NULL, m_func, m_file, m_line);
  }
};


struct Query_cache_query_state
{
  bool client_long_flag;
  bool client_protocol_41;
  uint8 protocol_type;
  bool more_results_exists;
  bool in_trans;
  bool autocommit;
  uint pkt_nr;
  uint character_set_client_num;
  uint character_set_results_num;
  uint collation_connection_num;
  ha_rows limit;
  Time_zone *time_zone;
  sql_mode_t sql_mode;
  ulong max_sort_length;
  ulong group_concat_max_len;
  ulong default_week_format;
  ulong div_precision_increment;
  MY_LOCALE *lc_time_names;
};


/**
   Construct the key used for cache lookup.
   The key is query + NULL + current DB (if any) + query state
   The key is allocated on THD's mem_root.

   @param thd               Thread context
   @param query             Query string
   @param query_state       Cache query state
   @param [out] tot_length  Length of the key

   @return The constructed cache key or NULL if OOM
*/

static const uchar *make_cache_key(THD *thd,
                                   const LEX_CSTRING &query,
                                   Query_cache_query_state *query_state,
                                   size_t *tot_length)
{
  *tot_length= query.length + 1 + thd->db().length +
    sizeof(Query_cache_query_state);
  uchar *cache_key= static_cast<uchar*>(thd->alloc(*tot_length));
  if (cache_key == NULL)
    return NULL;

  memcpy(cache_key, query.str, query.length);
  cache_key[query.length]= '\0';
  if (thd->db().length)
    memcpy(cache_key + query.length + 1, thd->db().str, thd->db().length);

  /*
    We should only copy structure (don't use it location directly)
    because of alignment issue
  */
  memcpy(static_cast<void*>(cache_key +
                            (*tot_length - sizeof(Query_cache_query_state))),
         query_state, sizeof(Query_cache_query_state));
  return cache_key;
}


/**
  Serialize access to the query cache.
  If the lock cannot be granted the thread hangs in a conditional wait which
  is signalled on each unlock.

  The lock attempt will also fail without wait if lock_and_suspend() is in
  effect by another thread. This enables a quick path in execution to skip waits
  when the outcome is known.

  @param thd         Thread handle
  @param use_timeout true if the lock can abort because of a timeout.

  @note use_timeout is optional and default value is false.

  @return
   @retval false An exclusive lock was taken
   @retval true The locking attempt failed
*/

bool Query_cache::try_lock(THD *thd, bool use_timeout)
{
  bool interrupt= false;
  Query_cache_wait_state wait_state(thd, __func__, __FILE__, __LINE__);
  DBUG_ENTER("Query_cache::try_lock");

  mysql_mutex_lock(&structure_guard_mutex);
  while (true)
  {
    if (m_cache_lock_status == Query_cache::UNLOCKED)
    {
      m_cache_lock_status= Query_cache::LOCKED;
#ifndef DBUG_OFF
      if (thd)
        m_cache_lock_thread_id= thd->thread_id();
#endif
      break;
    }
    else if (m_cache_lock_status == Query_cache::LOCKED_NO_WAIT)
    {
      /*
        If query cache is protected by a LOCKED_NO_WAIT lock this thread
        should avoid using the query cache as it is being evicted.
      */
      interrupt= true;
      break;
    }
    else
    {
      DBUG_ASSERT(m_cache_lock_status == Query_cache::LOCKED);
      /*
        To prevent send_result_to_client() and query_cache_insert() from
        blocking execution for too long a timeout is put on the lock.
      */
      if (use_timeout)
      {
        struct timespec waittime;
        set_timespec_nsec(&waittime, 50000000UL);  /* Wait for 50 msec */
        int res= mysql_cond_timedwait(&COND_cache_status_changed,
                                      &structure_guard_mutex, &waittime);
        if (res == ETIMEDOUT)
        {
          interrupt= true;
          break;
        }
      }
      else
      {
        mysql_cond_wait(&COND_cache_status_changed, &structure_guard_mutex);
      }
    }
  }
  mysql_mutex_unlock(&structure_guard_mutex);

  DBUG_RETURN(interrupt);
}


/**
  Serialize access to the query cache.
  If the lock cannot be granted the thread hangs in a conditional wait which
  is signalled on each unlock.

  This method also suspends the query cache so that other threads attempting to
  lock the cache with try_lock() will fail directly without waiting.

  It is used by all methods which flushes or destroys the whole cache.
 */

void Query_cache::lock_and_suspend(THD *thd)
{
  Query_cache_wait_state wait_state(thd, __func__, __FILE__, __LINE__);
  DBUG_ENTER("Query_cache::lock_and_suspend");

  mysql_mutex_lock(&structure_guard_mutex);
  while (m_cache_lock_status != Query_cache::UNLOCKED)
    mysql_cond_wait(&COND_cache_status_changed, &structure_guard_mutex);
  m_cache_lock_status= Query_cache::LOCKED_NO_WAIT;
#ifndef DBUG_OFF
  if (thd)
    m_cache_lock_thread_id= thd->thread_id();
#endif
  /* Wake up everybody, a whole cache flush is starting! */
  mysql_cond_broadcast(&COND_cache_status_changed);
  mysql_mutex_unlock(&structure_guard_mutex);

  DBUG_VOID_RETURN;
}


/**
  Serialize access to the query cache.
  If the lock cannot be granted the thread hangs in a conditional wait which
  is signalled on each unlock.

  It is used by all methods which invalidates one or more tables.
 */

void Query_cache::lock(THD *thd)
{
  Query_cache_wait_state wait_state(thd, __func__, __FILE__, __LINE__);
  DBUG_ENTER("Query_cache::lock");

  mysql_mutex_lock(&structure_guard_mutex);
  while (m_cache_lock_status != Query_cache::UNLOCKED)
    mysql_cond_wait(&COND_cache_status_changed, &structure_guard_mutex);
  m_cache_lock_status= Query_cache::LOCKED;
#ifndef DBUG_OFF
  if (thd)
    m_cache_lock_thread_id= thd->thread_id();
#endif
  mysql_mutex_unlock(&structure_guard_mutex);

  DBUG_VOID_RETURN;
}


/**
  Set the query cache to UNLOCKED and signal waiting threads.
*/

void Query_cache::unlock(THD *thd MY_ATTRIBUTE((unused)))
{
  DBUG_ENTER("Query_cache::unlock");
  mysql_mutex_lock(&structure_guard_mutex);
#ifndef DBUG_OFF
  if (thd)
    DBUG_ASSERT(m_cache_lock_thread_id == thd->thread_id());
#endif
  DBUG_ASSERT(m_cache_lock_status == Query_cache::LOCKED ||
              m_cache_lock_status == Query_cache::LOCKED_NO_WAIT);
  m_cache_lock_status= Query_cache::UNLOCKED;
  DBUG_PRINT("Query_cache",("Sending signal"));
  mysql_cond_signal(&COND_cache_status_changed);
  mysql_mutex_unlock(&structure_guard_mutex);
  DBUG_VOID_RETURN;
}


/**
  Helper function for determine if a SELECT statement has a SQL_NO_CACHE
  directive.

  @param sql           Query string
  @param offset        Offset of the first whitespace character after SELECT
  @param query_length  The total length of the query string

  @return
   @retval true The character string contains SQL_NO_CACHE
   @retval false No directive found.
*/

static bool has_no_cache_directive(const char *sql, uint offset,
                                   size_t query_length)
{
  uint i= offset;

  // Must have at least one whitespace char before SQL_NO_CACHE
  if (!my_isspace(system_charset_info, sql[i]))
    return false;

  // But can have several
  while (i < query_length &&
         my_isspace(system_charset_info, sql[i]))
    ++i;

  // Check that we have enough chars left for SQL_NO_CACHE
  if (i + 12 >= query_length)
    return false;

  if (my_toupper(system_charset_info, sql[i])    == 'S' &&
      my_toupper(system_charset_info, sql[i+1])  == 'Q' &&
      my_toupper(system_charset_info, sql[i+2])  == 'L' &&
      my_toupper(system_charset_info, sql[i+3])  == '_' &&
      my_toupper(system_charset_info, sql[i+4])  == 'N' &&
      my_toupper(system_charset_info, sql[i+5])  == 'O' &&
      my_toupper(system_charset_info, sql[i+6])  == '_' &&
      my_toupper(system_charset_info, sql[i+7])  == 'C' &&
      my_toupper(system_charset_info, sql[i+8])  == 'A' &&
      my_toupper(system_charset_info, sql[i+9])  == 'C' &&
      my_toupper(system_charset_info, sql[i+10]) == 'H' &&
      my_toupper(system_charset_info, sql[i+11]) == 'E' &&
      my_isspace(system_charset_info, sql[i+12]))
    return true;

  return false;
}


/*****************************************************************************
 Query_cache_block_table
*****************************************************************************/

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
  Query_cache_block *block()
  {
    return reinterpret_cast<Query_cache_block *>((reinterpret_cast<uchar*>(this)) -
                                                 ALIGN_SIZE(sizeof(Query_cache_block_table)*n) -
                                                 ALIGN_SIZE(sizeof(Query_cache_block)));
  }
};


/*****************************************************************************
   Query_cache_block method(s)
*****************************************************************************/

inline uint Query_cache_block::headers_len() const
{
  return static_cast<uint>(ALIGN_SIZE(sizeof(Query_cache_block_table)*n_tables) +
                           ALIGN_SIZE(sizeof(Query_cache_block)));
}


inline uchar* Query_cache_block::data()
{
  return reinterpret_cast<uchar*>(this) + headers_len();
}


inline Query_cache_query * Query_cache_block::query()
{
  return reinterpret_cast<Query_cache_query *>(data());
}


inline Query_cache_table * Query_cache_block::table()
{
  return reinterpret_cast<Query_cache_table *>(data());
}


inline Query_cache_result * Query_cache_block::result()
{
  return reinterpret_cast<Query_cache_result *>(data());
}


inline Query_cache_block_table * Query_cache_block::table(TABLE_COUNTER_TYPE n)
{
  return reinterpret_cast<Query_cache_block_table *>
    (reinterpret_cast<uchar*>(this)+ALIGN_SIZE(sizeof(Query_cache_block)) +
     n*sizeof(Query_cache_block_table));
}


/*****************************************************************************
 *   Query_cache_table
 *****************************************************************************/

struct Query_cache_table
{
  Query_cache_table() {}                      /* Remove gcc warning */
  const char *table;
  /* unique for every engine reference */
  qc_engine_callback callback;
  /* data need by some engines */
  ulonglong engine_data;
private:
  uint32 key_len;
public:
  /**
    The number of queries depending of this table.
  */
  int32 m_cached_query_count;
  uint8 table_type;

  const char *db() const              { return reinterpret_cast<const char *>(data()); }
  size_t key_length() const           { return key_len; }
  void key_length(size_t len)         { key_len= static_cast<uint32>(len); }
  const uchar* data() const
  {
    return reinterpret_cast<const uchar*>(this)+
      ALIGN_SIZE(sizeof(Query_cache_table));
  }
};


static const uchar *query_cache_table_get_key(const uchar *record, size_t *length)
{
  Query_cache_block* table_block=
    reinterpret_cast<Query_cache_block*>(const_cast<uchar*>(record));
  *length = (table_block->used - table_block->headers_len() -
	     ALIGN_SIZE(sizeof(Query_cache_table)));
  return (table_block->data() +
	  ALIGN_SIZE(sizeof(Query_cache_table)));
}


/*****************************************************************************
    Query_cache_query
*****************************************************************************/

struct Query_cache_query
{
  ulonglong current_found_rows;
  mysql_rwlock_t lock;
  Query_cache_block *result;
  THD *writer;
  ulong length;
  unsigned int last_pkt_nr;
  uint8 tables_type;

  Query_cache_query() {}                      /* Remove gcc warning */
  void init_n_lock()
  {
    result= NULL;
    writer= NULL;
    length= 0;
    mysql_rwlock_init(key_rwlock_query_cache_query_lock, &lock);
    mysql_rwlock_wrlock(&lock);
  }
  void unlock_n_destroy()
  {
    /*
      The following call is not needed on system where one can destroy an
      active semaphore
    */
    mysql_rwlock_unlock(&lock);
    mysql_rwlock_destroy(&lock);
  }
  ulong add(size_t packet_len)
  { return (length+= static_cast<ulong>(packet_len)); }
  const uchar* query()
  {
    return reinterpret_cast<const uchar*>(this) +
      ALIGN_SIZE(sizeof(Query_cache_query));
  }
  /*
    Needed for finding queries, that we may delete from cache.
    We don't want to wait while block become unlocked. In addition,
    block locking means that query is now used and we don't need to
    remove it.
  */
  bool try_lock_writing()
  {
    return mysql_rwlock_trywrlock(&lock) == 0;
  }
};


static const uchar *query_cache_query_get_key(const uchar *record, size_t *length)
{
  Query_cache_block *query_block=
    reinterpret_cast<Query_cache_block*>(const_cast<uchar*>(record));
  *length = (query_block->used - query_block->headers_len() -
	     ALIGN_SIZE(sizeof(Query_cache_query)));
  return (query_block->data() +
	  ALIGN_SIZE(sizeof(Query_cache_query)));
}


/*****************************************************************************
  Functions to store things into the query cache
*****************************************************************************/

/*
  Note on double-check locking (DCL) usage.

  Below, in query_cache_insert(), query_cache_abort() and
  Query_cache::end_of_result() we use what is called double-check
  locking (DCL) for THD::first_query_cache_block.
  I.e. we test it first without a lock, and, if positive, test again
  under the lock.

  This means that if we see 'first_query_block == 0' without a
  lock we will skip the operation.  But this is safe here: when we
  started to cache a query, we called Query_cache::store_query(), and
  'first_query_block' was set to non-zero in this thread (and the
  thread always sees results of its memory operations, mutex or not).
  If later we see 'first_query_block == 0' without locking a
  mutex, that may only mean that some other thread have reset it by
  invalidating the query.  Skipping the operation in this case is the
  right thing to do, as first_query_block won't get non-zero for
  this query again.

  See also comments in Query_cache::store_query() and
  Query_cache::send_result_to_client().

  NOTE, however, that double-check locking is not applicable in
  'invalidate' functions, as we may erroneously skip invalidation,
  because the thread doing invalidation may never see non-zero
  'first_query_block'.
*/


/**
  libmysql convenience wrapper to insert data into query cache.
*/

void query_cache_insert(const uchar *packet, size_t length, uint pkt_nr)
{
  THD *thd= current_thd;

  /*
    Current_thd can be NULL when a new connection is immediately ended
    due to "Too many connections". thd->store_globals() has not been
    called at this time and hence my_thread_setspecific_ptr(THR_THD,
    this) has not been called for this thread.
  */

  if (!thd)
    return;

  query_cache.insert(thd, packet, length, pkt_nr);
}


/**
  Insert the packet into the query cache.
*/

void Query_cache::insert(THD *thd, const uchar *packet, size_t length,
                         uint pkt_nr)
{
  DBUG_ENTER("Query_cache::insert");

  /* See the comment on double-check locking usage above. */
  if (is_disabled() || thd->first_query_cache_block == NULL)
    DBUG_VOID_RETURN;

  DEBUG_SYNC(thd, "wait_in_query_cache_insert");

  if (try_lock(thd, false))
    DBUG_VOID_RETURN;

  Query_cache_block *query_block = thd->first_query_cache_block;
  if (query_block == NULL)
  {
    /*
      We lost the writer and the currently processed query has been
      invalidated; there is nothing left to do.
    */
    unlock(thd);
    DBUG_VOID_RETURN;
  }
  mysql_rwlock_wrlock(&query_block->query()->lock);
  Query_cache_query *header= query_block->query();
  Query_cache_block *result= header->result;

  DBUG_PRINT("qcache", ("insert packet %zu bytes long", length));

  /*
    On success, STRUCT_UNLOCK is done by append_result_data. Otherwise, we
    still need structure_guard_mutex to free the query, and therefore unlock
    it later in this function.
  */
  if (!append_result_data(thd, &result, static_cast<ulong>(length), packet,
                          query_block))
  {
    DBUG_PRINT("warning", ("Can't append data"));
    header->result= result;
    DBUG_PRINT("qcache", ("free query %p", query_block));
    // The following call will remove the lock on query_block
    query_cache.free_query(query_block);
    query_cache.refused++;
    // append_result_data no success => we need unlock
    unlock(thd);
    DBUG_VOID_RETURN;
  }

  header->result= result;
  header->last_pkt_nr= pkt_nr;
  mysql_rwlock_unlock(&query_block->query()->lock);

  DBUG_VOID_RETURN;
}


void Query_cache::abort(THD *thd)
{
  DBUG_ENTER("query_cache_abort");

  /* See the comment on double-check locking usage above. */
  if (is_disabled() || thd->first_query_cache_block == NULL)
    DBUG_VOID_RETURN;

  if (try_lock(thd, false))
    DBUG_VOID_RETURN;

  /*
    While we were waiting another thread might have changed the status
    of the writer. Make sure the writer still exists before continue.
  */
  Query_cache_block *query_block= thd->first_query_cache_block;
  if (query_block)
  {
    THD_STAGE_INFO(thd, stage_storing_result_in_query_cache);
    mysql_rwlock_wrlock(&query_block->query()->lock);
    // The following call will remove the lock on query_block
    free_query(query_block);
    thd->first_query_cache_block= NULL;
  }

  unlock(thd);

  DBUG_VOID_RETURN;
}


void Query_cache::end_of_result(THD *thd)
{
  Query_cache_block *query_block;
  ulonglong current_found_rows= thd->current_found_rows;
  DBUG_ENTER("Query_cache::end_of_result");

  /* See the comment on double-check locking usage above. */
  if (thd->first_query_cache_block == NULL)
    DBUG_VOID_RETURN;

  if (thd->killed || thd->is_error())
  {
    abort(thd);
    DBUG_VOID_RETURN;
  }

  /* Ensure that only complete results are cached. */
  DBUG_ASSERT(thd->get_stmt_da()->is_eof());

  if (try_lock(thd, false))
    DBUG_VOID_RETURN;

  query_block= thd->first_query_cache_block;
  if (query_block)
  {
    /*
      The writer is still present; finish last result block by chopping it to 
      suitable size if needed and setting block type. Since this is the last
      block, the writer should be dropped.
    */
    THD_STAGE_INFO(thd, stage_storing_result_in_query_cache);
    mysql_rwlock_wrlock(&query_block->query()->lock);
    Query_cache_query *header= query_block->query();
    Query_cache_block *last_result_block;
    ulong allign_size;
    ulong len;

    if (header->result == NULL)
    {
      DBUG_PRINT("error", ("End of data with no result blocks; "
                           "Query '%s' removed from cache.", header->query()));
      /*
        Extra safety: empty result should not happen in the normal call
        to this function. In the release version that query should be ignored
        and removed from QC.
      */
      DBUG_ASSERT(false);
      free_query(query_block);
      unlock(thd);
      DBUG_VOID_RETURN;
    }
    last_result_block= header->result->prev;
    allign_size= ALIGN_SIZE(last_result_block->used);
    len= max(query_cache.min_allocation_unit, allign_size);
    if (last_result_block->length >= query_cache.min_allocation_unit + len)
      query_cache.split_block(last_result_block,len);

    header->current_found_rows= current_found_rows;
    header->result->type= Query_cache_block::RESULT;

    /* Drop the writer. */
    header->writer= NULL;
    thd->first_query_cache_block= NULL;
    mysql_rwlock_unlock(&query_block->query()->lock);
  }

  unlock(thd);
  DBUG_VOID_RETURN;
}


void query_cache_invalidate_by_MyISAM_filename(const char *filename)
{
  query_cache.invalidate_by_MyISAM_filename(current_thd, filename);
}


/*****************************************************************************
   Query_cache methods
*****************************************************************************/

Query_cache::Query_cache()
  :query_cache_size(0),
   query_cache_limit(ULONG_MAX),
   queries_in_cache(0), hits(0), inserts(0), refused(0),
   total_blocks(0), lowmem_prunes(0), m_query_cache_is_disabled(false),
   min_allocation_unit(ALIGN_SIZE(QUERY_CACHE_MIN_ALLOCATION_UNIT)),
   min_result_data_size(ALIGN_SIZE(QUERY_CACHE_MIN_RESULT_DATA_SIZE)),
   def_query_hash_size(ALIGN_SIZE(QUERY_CACHE_DEF_QUERY_HASH_SIZE)),
   def_table_hash_size(ALIGN_SIZE(QUERY_CACHE_DEF_TABLE_HASH_SIZE)),
   initialized(false)
{
  ulong min_needed= (ALIGN_SIZE(sizeof(Query_cache_block)) +
		     ALIGN_SIZE(sizeof(Query_cache_block_table)) +
		     ALIGN_SIZE(sizeof(Query_cache_query)) + 3);
  set_if_bigger(min_allocation_unit,min_needed);
  this->min_allocation_unit= ALIGN_SIZE(min_allocation_unit);
  set_if_bigger(this->min_result_data_size,min_allocation_unit);
}


ulong Query_cache::resize(THD *thd, ulong query_cache_size_arg)
{
  ulong new_query_cache_size;
  DBUG_ENTER("Query_cache::resize");
  DBUG_ASSERT(initialized);

  lock_and_suspend(thd);

  /*
    Wait for all readers and writers to exit. When the list of all queries
    is iterated over with a block level lock, we are done.
  */
  Query_cache_block *block= queries_blocks;
  if (block)
  {
    do
    {
      mysql_rwlock_wrlock(&block->query()->lock);
      Query_cache_query *query= block->query();
      if (query->writer)
      {
        /*
           Drop the writer; this will cancel any attempts to store
           the processed statement associated with this writer.
         */
        query->writer->first_query_cache_block= NULL;
        query->writer= NULL;
        refused++;
      }
      query->unlock_n_destroy();
      block= block->next;
    } while (block != queries_blocks);
  }
  free_cache();

  query_cache_size= query_cache_size_arg;
  new_query_cache_size= init_cache();

  unlock(thd);
  DBUG_RETURN(new_query_cache_size);
}


ulong Query_cache::set_min_res_unit(ulong size)
{
  if (size < min_allocation_unit)
    size= min_allocation_unit;
  else if (size > max_aligned_min_res_unit_size)
    size= max_aligned_min_res_unit_size;

  return (min_result_data_size= ALIGN_SIZE(size));
}


void Query_cache::store_query(THD *thd, TABLE_LIST *tables_used)
{
  TABLE_COUNTER_TYPE local_tables;
  DBUG_ENTER("Query_cache::store_query");
  /*
    Testing 'query_cache_size' without a lock here is safe: the thing
    we may loose is that the query won't be cached, but we save on
    mutex locking in the case when query cache is disabled or the
    query is uncachable.

    See also a note on double-check locking usage above.
  */
  if (thd->locked_tables_mode || query_cache_size == 0)
    DBUG_VOID_RETURN;

  /*
    Do not store queries while tracking transaction state.
    The tracker already flags queries that actually have
    transaction tracker items, but this will make behavior
    more straight forward.
  */
  if (thd->variables.session_track_transaction_info != TX_TRACK_NONE)
    DBUG_VOID_RETURN;

  /*
    The query cache is only supported for the classic protocols.
  */
  if (!thd->is_classic_protocol())
    DBUG_VOID_RETURN;
  /*
    Without active vio, net_write_packet() will not be called and
    therefore neither Query_cache::insert(). Since we will never get a
    complete query result in this case, it does not make sense to
    register the query in the first place.
  */
  if (!thd->get_protocol()->connection_alive())
    DBUG_VOID_RETURN;

  uint8 tables_type= 0;

  if ((local_tables= is_cacheable(thd, thd->lex, tables_used, &tables_type)))
  {
    Query_cache_query_state query_state;
    // fill all gaps between fields with 0 to get repeatable key
    memset(&query_state, 0, sizeof(Query_cache_query_state));
    query_state.client_long_flag=
      thd->get_protocol()->has_client_capability(CLIENT_LONG_FLAG);
    query_state.client_protocol_41=
      thd->get_protocol()->has_client_capability(CLIENT_PROTOCOL_41);
    /*
      Protocol influences result format, so statement results in the binary
      protocol (COM_EXECUTE) cannot be served to statements asking for results
      in the text protocol (COM_QUERY) and vice-versa.
    */
    query_state.protocol_type= static_cast<uint8>(thd->get_protocol()->type());
    /* PROTOCOL_LOCAL results are not cached. */
    DBUG_ASSERT(query_state.protocol_type !=
                static_cast<uint8>(Protocol::PROTOCOL_LOCAL));
    query_state.more_results_exists=
      thd->server_status & SERVER_MORE_RESULTS_EXISTS;
    query_state.in_trans= thd->in_active_multi_stmt_transaction();
    query_state.autocommit= thd->server_status & SERVER_STATUS_AUTOCOMMIT;
    query_state.pkt_nr= thd->get_protocol_classic()->get_output_pkt_nr();
    query_state.character_set_client_num=
      thd->variables.character_set_client->number;
    query_state.character_set_results_num=
      (thd->variables.character_set_results ?
       thd->variables.character_set_results->number :
       UINT_MAX);
    query_state.collation_connection_num=
      thd->variables.collation_connection->number;
    query_state.limit= thd->variables.select_limit;
    query_state.time_zone= thd->variables.time_zone;
    query_state.sql_mode= thd->variables.sql_mode;
    query_state.max_sort_length= thd->variables.max_sort_length;
    query_state.lc_time_names= thd->variables.lc_time_names;
    query_state.group_concat_max_len= thd->variables.group_concat_max_len;
    query_state.div_precision_increment= thd->variables.div_precincrement;
    query_state.default_week_format= thd->variables.default_week_format;
    DBUG_PRINT("qcache", ("\
long %d, 4.1: %d, bin_proto: %d, more results %d, pkt_nr: %d,  \
CS client: %u, CS result: %u, CS conn: %u, limit: %llu, TZ: %p, \
sql mode: 0x%llx, sort len: %lu, conncat len: %lu, div_precision: %lu, \
def_week_frmt: %lu, in_trans: %d, autocommit: %d",
                          query_state.client_long_flag,
                          query_state.client_protocol_41,
                          query_state.protocol_type,
                          query_state.more_results_exists,
                          query_state.pkt_nr,
                          query_state.character_set_client_num,
                          query_state.character_set_results_num,
                          query_state.collation_connection_num,
                          query_state.limit,
                          query_state.time_zone,
                          query_state.sql_mode,
                          query_state.max_sort_length,
                          query_state.group_concat_max_len,
                          query_state.div_precision_increment,
                          query_state.default_week_format,
                          query_state.in_trans,
                          query_state.autocommit));

    /*
      A table- or a full flush operation can potentially take a long time to
      finish. We choose not to wait for them and skip caching statements
      instead.

      In case the wait time can't be determined there is an upper limit which
      causes try_lock() to abort with a time out.

      The 'true' parameter indicate that the lock is allowed to timeout

    */
    if (try_lock(thd, true))
      DBUG_VOID_RETURN;
    if (query_cache_size == 0)
    {
      unlock(thd);
      DBUG_VOID_RETURN;
    }

    if (ask_handler_allowance(thd, tables_used))
    {
      refused++;
      unlock(thd);
      DBUG_VOID_RETURN;
    }

    /* Key is query + database + flag */
    size_t tot_length;
    const uchar *cache_key= make_cache_key(thd, thd->query(),
                                           &query_state, &tot_length);
    if (cache_key == NULL)
    {
      unlock(thd);
      DBUG_VOID_RETURN;
    }

    /* Check if another thread is processing the same query? */
    Query_cache_block *competitor=
      reinterpret_cast<Query_cache_block *>(my_hash_search(&queries,
                                                           cache_key, tot_length));
    DBUG_PRINT("qcache", ("competitor %p", competitor));
    if (competitor == NULL)
    {
      /* Query is not in cache and no one is working with it; Store it */
      Query_cache_block *query_block=
        write_block_data(tot_length, cache_key,
                         ALIGN_SIZE(sizeof(Query_cache_query)),
                         Query_cache_block::QUERY, local_tables);
      if (query_block != NULL)
      {
	DBUG_PRINT("qcache", ("query block %p allocated, %lu",
                              query_block, query_block->used));

	Query_cache_query *header = query_block->query();
	header->init_n_lock();
	if (my_hash_insert(&queries, reinterpret_cast<uchar*>(query_block)))
	{
	  refused++;
	  DBUG_PRINT("qcache", ("insertion in query hash"));
	  header->unlock_n_destroy();
	  free_memory_block(query_block);
          unlock(thd);
	  DBUG_VOID_RETURN;
	}
	if (!register_all_tables(query_block, tables_used))
	{
	  refused++;
	  DBUG_PRINT("warning", ("tables list including failed"));
	  my_hash_delete(&queries, reinterpret_cast<uchar *>(query_block));
	  header->unlock_n_destroy();
	  free_memory_block(query_block);
          unlock(thd);
	  DBUG_VOID_RETURN;
	}
	double_linked_list_simple_include(query_block, &queries_blocks);
	inserts++;
	queries_in_cache++;
	thd->first_query_cache_block= query_block;
	header->writer= thd;
	header->tables_type= tables_type;

        unlock(thd);

	// init_n_lock make query block locked
        mysql_rwlock_unlock(&query_block->query()->lock);
      }
      else
      {
	// We have not enough memory to store query => do nothing
	refused++;
        unlock(thd);
	DBUG_PRINT("warning", ("Can't allocate query"));
      }
    }
    else
    {
      // Another thread is processing the same query => do nothing
      refused++;
      unlock(thd);
      DBUG_PRINT("qcache", ("Another thread process same query"));
    }
  }
  else if (thd->lex->sql_command == SQLCOM_SELECT)
    refused++;

  DBUG_VOID_RETURN;
}


/**
  Send a single memory block from the query cache.

  Respects the client/server protocol limits for the
  size of the network packet, and splits a large block
  in pieces to ensure that individual piece doesn't exceed
  the maximal allowed size of the network packet (16M).

  @param[in] net NET handler
  @param[in] packet packet to send
  @param[in] len packet length

  @return Operation status
    @retval false On success
    @retval true On error
*/

static bool send_data_in_chunks(NET *net, const uchar *packet, ulong len)
{
  /*
    On the client we may require more memory than max_allowed_packet
    to keep, both, the truncated last logical packet, and the
    compressed next packet.  This never (or in practice never)
    happens without compression, since without compression it's very
    unlikely that a) a truncated logical packet would remain on the
    client when it's time to read the next packet b) a subsequent
    logical packet that is being read would be so large that
    size-of-new-packet + size-of-old-packet-tail >
    max_allowed_packet.  To remedy this issue, we send data in 1MB
    sized packets, that's below the current client default of 16MB
    for max_allowed_packet, but large enough to ensure there is no
    unnecessary overhead from too many syscalls per result set.
  */
  static const ulong MAX_CHUNK_LENGTH= 1024*1024;

  while (len > MAX_CHUNK_LENGTH)
  {
    if (net_write_packet(net, packet, MAX_CHUNK_LENGTH))
      return true;
    packet+= MAX_CHUNK_LENGTH;
    len-= MAX_CHUNK_LENGTH;
  }
  if (len && net_write_packet(net, packet, len))
    return true;

  return false;
}


/**
  Check if the query is in the cache. If it was cached, send it
  to the user.

  @param thd Pointer to the thread handler
  @param sql A reference to the sql statement *

  @return status code
  @retval 0  Query was not cached.
  @retval 1  The query was cached and user was sent the result.
  @retval -1 The query was cached but we didn't have rights to use it.

  In case of -1, no error is sent to the client.

  *) The buffer must be allocated memory of size:
  tot_length= query_length + thd->db_length + 1 + sizeof(Query_cache_query_state);
*/

int Query_cache::send_result_to_client(THD *thd, const LEX_CSTRING &sql)
{
  ulonglong engine_data;
  Query_cache_query *query;
  Query_cache_block *first_result_block;
  Query_cache_block *result_block;
  Query_cache_block_table *block_table, *block_table_end;
  const uchar *cache_key= NULL;
  size_t tot_length;
  Query_cache_query_state query_state;
  DBUG_ENTER("Query_cache::send_result_to_client");

  /*
    Testing 'query_cache_size' without a lock here is safe: the thing
    we may loose is that the query won't be served from cache, but we
    save on mutex locking in the case when query cache is disabled.

    See also a note on double-check locking usage above.
  */
  if (is_disabled() || thd->locked_tables_mode ||
      thd->variables.query_cache_type == 0 || query_cache_size == 0)
    goto err;

  /*
    Don't work with Query_cache if the state of XA transaction is
    either IDLE or PREPARED. If we didn't do so we would get an
    assert fired later in the function trx_start_if_not_started_low()
    that is called when we are checking that query cache is allowed at
    this moment to operate on an InnoDB table.
  */
  if (thd->get_transaction()->xid_state()->check_xa_idle_or_prepared(false))
    goto err;

  /*
    Don't allow serving from Query_cache while tracking transaction
    state. This is a safeguard in case an otherwise matching query
    was added to the cache before tracking was turned on.
  */
  if (thd->variables.session_track_transaction_info != TX_TRACK_NONE)
    goto err;

  if (!thd->lex->safe_to_cache_query)
  {
    DBUG_PRINT("qcache", ("SELECT is non-cacheable"));
    goto err;
  }

  {
    uint i= 0;
    /*
      Skip '(' characters in queries like following:
      (select a from t1) union (select a from t1);
    */
    while (sql.str[i]=='(')
      i++;

    /*
      Test if this is a SELECT statement.
      Leading spaces have been removed by dispatch_command().
      If query doesn't start with a comment, then if it is a SELECT statement
      it must start with SELECT or WITH.
    */
    char first_letter= my_toupper(system_charset_info, sql.str[i]);
    if ((first_letter                                    != 'S' ||
         my_toupper(system_charset_info, sql.str[i + 1]) != 'E' ||
         my_toupper(system_charset_info, sql.str[i + 2]) != 'L' ||
         my_toupper(system_charset_info, sql.str[i + 3]) != 'E' ||
         my_toupper(system_charset_info, sql.str[i + 4]) != 'C' ||
         my_toupper(system_charset_info, sql.str[i + 5]) != 'T') &&
        (first_letter                                    != 'W' ||
         my_toupper(system_charset_info, sql.str[i + 1]) != 'I' ||
         my_toupper(system_charset_info, sql.str[i + 2]) != 'T' ||
         my_toupper(system_charset_info, sql.str[i + 3]) != 'H') &&
        (sql.str[i] != '/' || sql.length < i+6))
    {
      DBUG_PRINT("qcache", ("The statement is not a SELECT; Not cached"));
      goto err;
    }

    DBUG_EXECUTE_IF("test_sql_no_cache",
                    DBUG_ASSERT(has_no_cache_directive(sql.str, i+6,
                                                       sql.length)););
    if (has_no_cache_directive(sql.str, i+6, sql.length))
    {
      /*
        We do not increase 'refused' statistics here since it will be done
        later when the query is parsed.
      */
      DBUG_PRINT("qcache", ("The statement has a SQL_NO_CACHE directive"));
      goto err;
    }
  }
  /*
    Try to obtain an exclusive lock on the query cache. If the cache is
    disabled or if a full cache flush is in progress, the attempt to
    get the lock is aborted.

    The 'true' parameter indicate that the lock is allowed to timeout
  */
  if (try_lock(thd, true))
    goto err;

  if (query_cache_size == 0)
    goto err_unlock;

  Query_cache_block *query_block;

  THD_STAGE_INFO(thd, stage_checking_query_cache_for_query);

  // fill all gaps between fields with 0 to get repeatable key
  memset(&query_state, 0, sizeof(Query_cache_query_state));
  query_state.client_long_flag=
    thd->get_protocol()->has_client_capability(CLIENT_LONG_FLAG);
  query_state.client_protocol_41=
    thd->get_protocol()->has_client_capability(CLIENT_PROTOCOL_41);
  query_state.protocol_type= static_cast<uint8>(thd->get_protocol()->type());
  query_state.more_results_exists=
    thd->server_status & SERVER_MORE_RESULTS_EXISTS;
  query_state.in_trans= thd->in_active_multi_stmt_transaction();
  query_state.autocommit= thd->server_status & SERVER_STATUS_AUTOCOMMIT;
  query_state.pkt_nr= thd->get_protocol_classic()->get_output_pkt_nr();
  query_state.character_set_client_num=
    thd->variables.character_set_client->number;
  query_state.character_set_results_num=
    (thd->variables.character_set_results ?
     thd->variables.character_set_results->number :
     UINT_MAX);
  query_state.collation_connection_num=
    thd->variables.collation_connection->number;
  query_state.limit= thd->variables.select_limit;
  query_state.time_zone= thd->variables.time_zone;
  query_state.sql_mode= thd->variables.sql_mode;
  query_state.max_sort_length= thd->variables.max_sort_length;
  query_state.group_concat_max_len= thd->variables.group_concat_max_len;
  query_state.div_precision_increment= thd->variables.div_precincrement;
  query_state.default_week_format= thd->variables.default_week_format;
  query_state.lc_time_names= thd->variables.lc_time_names;
  DBUG_PRINT("qcache", ("\
long %d, 4.1: %d, bin_proto: %d, more results %d, pkt_nr: %d, \
CS client: %u, CS result: %u, CS conn: %u, limit: %llu, TZ: %p, \
sql mode: 0x%llx, sort len: %lu, conncat len: %lu, div_precision: %lu, \
def_week_frmt: %lu, in_trans: %d, autocommit: %d",
                        query_state.client_long_flag,
                        query_state.client_protocol_41,
                        query_state.protocol_type,
                        query_state.more_results_exists,
                        query_state.pkt_nr,
                        query_state.character_set_client_num,
                        query_state.character_set_results_num,
                        query_state.collation_connection_num,
                        query_state.limit,
                        query_state.time_zone,
                        query_state.sql_mode,
                        query_state.max_sort_length,
                        query_state.group_concat_max_len,
                        query_state.div_precision_increment,
                        query_state.default_week_format,
                        query_state.in_trans,
                        query_state.autocommit));

  cache_key= make_cache_key(thd, thd->query(), &query_state, &tot_length);
  if (cache_key == NULL)
    goto err_unlock;

  query_block=
    reinterpret_cast<Query_cache_block *>(my_hash_search(&queries,
                                                         cache_key,
                                                         tot_length));
  /* Quick abort on unlocked data */
  if (query_block == NULL ||
      query_block->query()->result == NULL ||
      query_block->query()->result->type != Query_cache_block::RESULT)
  {
    DBUG_PRINT("qcache", ("No query in query hash or no results"));
    goto err_unlock;
  }
  DBUG_PRINT("qcache", ("Query in query hash %p", query_block));

  /*
    We only need to clear the diagnostics area when we actually
    find the query, as in all other cases, we'll go through
    regular parsing and execution, where the DA will be reset
    as needed, anyway.

    We're not pushing/popping a private DA here the way we do for
    parsing; if we got this far, we know we've got a SELECT on our
    hands and not a diagnotics statement that might need the
    previous statement's diagnostics area, so we just clear the DA.

    We're doing it here and not in the caller as there's three of
    them (PS, SP, interactive).  Doing it any earlier in this routine
    would reset the DA in "SELECT @@error_count"/"SELECT @@warning_count"
    before we can save the counts we'll need later (QC will see the
    SELECT go into this branch, but since we haven't parsed yet, we
    don't know yet that it's one of those legacy variables that require
    saving and basically turn SELECT into a sort of, sort of not
    diagnostics command.  Ugly stuff.
  */
  thd->get_stmt_da()->reset_diagnostics_area();
  thd->get_stmt_da()->reset_condition_info(thd);

  /* Now lock and test that nothing changed while blocks was unlocked */
  mysql_rwlock_rdlock(&query_block->query()->lock);

  query = query_block->query();
  result_block= query->result;
  first_result_block= result_block;

  if (result_block == NULL || result_block->type != Query_cache_block::RESULT)
  {
    /* The query is probably yet processed */
    DBUG_PRINT("qcache", ("query found, but no data or data incomplete"));
    mysql_rwlock_unlock(&query_block->query()->lock);
    goto err_unlock;
  }
  DBUG_PRINT("qcache", ("Query have result %p", query));

  if (thd->in_multi_stmt_transaction_mode() &&
      (query->tables_type & HA_CACHE_TBL_TRANSACT))
  {
    DBUG_PRINT("qcache",
	       ("we are in transaction and have transaction tables in query"));
    mysql_rwlock_unlock(&query_block->query()->lock);
    goto err_unlock;
  }

  // Check access;
  THD_STAGE_INFO(thd, stage_checking_privileges_on_cached_query);
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
      if (tmptable->s->table_cache_key.length - TMP_TABLE_KEY_EXTRA == 
          table->key_length() &&
          !memcmp(tmptable->s->table_cache_key.str, table->data(),
                  table->key_length()))
      {
        DBUG_PRINT("qcache",
                   ("Temporary table detected: '%s.%s'",
                    tmptable->s->db.str, tmptable->s->table_name.str));
        unlock(thd);
        /*
          We should not store result of this query because it contain
          temporary tables => assign following variable to make check
          faster.
        */
        thd->lex->safe_to_cache_query= false;
        mysql_rwlock_unlock(&query_block->query()->lock);
        DBUG_RETURN(-1);
      }
    }

    memset(&table_list, 0, sizeof(table_list));
    table_list.db = table->db();
    table_list.alias= table_list.table_name= table->table;

    if (check_table_access(thd,SELECT_ACL,&table_list, false, 1,true))
    {
      DBUG_PRINT("qcache",
		 ("probably no SELECT access to %s.%s =>  return to normal processing",
		  table_list.db, table_list.alias));
      unlock(thd);
      thd->lex->safe_to_cache_query= false;	// Don't try to cache this
      mysql_rwlock_unlock(&query_block->query()->lock);
      DBUG_RETURN(-1);				// Privilege error
    }
    DBUG_ASSERT((SELECT_ACL & ~table_list.grant.privilege) ==
                table_list.grant.want_privilege);
    if ((table_list.grant.privilege & SELECT_ACL) == 0)
    {
      DBUG_PRINT("qcache", ("Need to check column privileges for %s.%s",
			    table_list.db, table_list.alias));
      mysql_rwlock_unlock(&query_block->query()->lock);
      thd->lex->safe_to_cache_query= false;	// Don't try to cache this
      goto err_unlock;				// Parse query
    }

    engine_data= table->engine_data;
    if (table->callback)
    {
      char qcache_se_key_name[FN_REFLEN + 1];
      size_t qcache_se_key_len;
      engine_data= table->engine_data;

      qcache_se_key_len= build_table_filename(qcache_se_key_name,
                                              sizeof(qcache_se_key_name),
                                              table->db(), table->table,
                                              "", 0);

      if (!(*table->callback)(thd, qcache_se_key_name,
                              static_cast<uint>(qcache_se_key_len),
                              &engine_data))
      {
        DBUG_PRINT("qcache", ("Handler does not allow caching for %s.%s",
                               table_list.db, table_list.alias));
        mysql_rwlock_unlock(&query_block->query()->lock);
        if (engine_data != table->engine_data)
        {
          DBUG_PRINT("qcache",
                     ("Handler require invalidation queries of %s.%s %llu-%llu",
                      table_list.db, table_list.alias,
                      engine_data, table->engine_data));
          invalidate_table_internal(reinterpret_cast<const uchar *>(table->db()),
                                    table->key_length());
        }
        else
          thd->lex->safe_to_cache_query= false;      // Don't try to cache this
        /*
          End the statement transaction potentially started by engine.
          Currently our engines do not request rollback from callbacks.
          If this is going to change code needs to be reworked.
        */
        DBUG_ASSERT(! thd->transaction_rollback_request);
        trans_rollback_stmt(thd);
        goto err_unlock;				// Parse query
     }
   }
    else
      DBUG_PRINT("qcache", ("handler allow caching %s,%s",
			    table_list.db, table_list.alias));
  }
  move_to_query_list_end(query_block);
  hits++;
  unlock(thd);

  /*
    Send cached result to client
  */
  THD_STAGE_INFO(thd, stage_sending_cached_result_to_client);
  do
  {
    DBUG_PRINT("qcache", ("Results  (len: %lu  used: %lu  headers: %zu)",
			  result_block->length, result_block->used,
			  result_block->headers_len() +
                          ALIGN_SIZE(sizeof(Query_cache_result))));

    Query_cache_result *result = result_block->result();
    if (send_data_in_chunks(thd->get_protocol_classic()->get_net(),
                            result->data(),
                            result_block->used -
                            result_block->headers_len() -
                            ALIGN_SIZE(sizeof(Query_cache_result))))
      break;                                    // Client aborted
    result_block = result_block->next;
    // Keep packet number updated
    thd->get_protocol_classic()->set_output_pkt_nr(query->last_pkt_nr);
  } while (result_block != first_result_block);

  thd->current_found_rows= query->current_found_rows;
  thd->update_previous_found_rows();
  thd->clear_current_query_costs();
  thd->save_current_query_costs();

  {
    Opt_trace_start ots(thd, NULL, SQLCOM_SELECT, NULL,
                        thd->query().str, thd->query().length, NULL,
                        thd->variables.character_set_client);

    Opt_trace_object (&thd->opt_trace)
      .add("query_result_read_from_cache", true);
  }

  /*
    End the statement transaction potentially started by an
    engine callback. We ignore the return value for now,
    since as long as EOF packet is part of the query cache
    response, we can't handle it anyway.
  */
  (void) trans_commit_stmt(thd);
  if (!thd->get_stmt_da()->is_set())
    thd->get_stmt_da()->disable_status();

  mysql_rwlock_unlock(&query_block->query()->lock);
  DBUG_RETURN(1);				// Result sent to client

err_unlock:
  unlock(thd);
err:
  DBUG_RETURN(0);				// Query was not cached
}


/**
  Remove all cached queries that use the given table.

  @param thd                 Thread handle
  @param table_used          TABLE_LIST representing the table to be
                             invalidated.
  @param using_transactions  If we are inside a transaction only add
                             the table to a list of changed tables for now,
                             don't invalidate directly. The table will instead
                             be invalidated once the transaction commits.
*/

void Query_cache::invalidate_single(THD *thd, TABLE_LIST *table_used,
                                    bool using_transactions)
{
  DBUG_ENTER("Query_cache::invalidate_single (table list)");
  if (is_disabled())
    DBUG_VOID_RETURN;

  using_transactions&= thd->in_multi_stmt_transaction_mode();
  DBUG_ASSERT(!using_transactions || table_used->table!=0);
  DBUG_ASSERT(!table_used->is_view_or_derived());
  if (table_used->is_view_or_derived())
    DBUG_VOID_RETURN;
  if (using_transactions &&
      (table_used->table->file->table_cache_type() ==
       HA_CACHE_TBL_TRANSACT))
    /*
      table_used->table can't be 0 in transaction.
      Only 'drop' invalidate not opened table, but 'drop'
      force transaction finish.
    */
    thd->get_transaction()->
      add_changed_table(table_used->table->s->table_cache_key.str,
                        static_cast<uint32>(table_used->table->s->table_cache_key.length));
  else
    invalidate_table(thd, table_used);

  DBUG_VOID_RETURN;
}


/**
  Remove all cached queries that use any of the tables in the list.

  @see Query_cache::invalidate_single().
*/

void Query_cache::invalidate(THD *thd, TABLE_LIST *tables_used,
			     bool using_transactions)
{
  DBUG_ENTER("Query_cache::invalidate (table list)");
  if (is_disabled())
    DBUG_VOID_RETURN;

  using_transactions= using_transactions && thd->in_multi_stmt_transaction_mode();
  for (; tables_used; tables_used= tables_used->next_local)
    invalidate_single(thd, tables_used, using_transactions);

  DEBUG_SYNC(thd, "wait_after_query_cache_invalidate");

  DBUG_VOID_RETURN;
}


/**
  Invalidate locked for write

  @param thd         - thread handle
  @param tables_used - table list

  @note can be used only for opened tables
*/

void Query_cache::invalidate_locked_for_write(THD *thd, TABLE_LIST *tables_used)
{
  DBUG_ENTER("Query_cache::invalidate_locked_for_write");
  if (is_disabled())
    DBUG_VOID_RETURN;

  for (; tables_used; tables_used= tables_used->next_local)
  {
    THD_STAGE_INFO(thd, stage_invalidating_query_cache_entries_table);
    if (tables_used->lock_descriptor().type >= TL_WRITE_ALLOW_WRITE &&
        tables_used->table)
    {
      invalidate_table(thd, tables_used->table);
    }
  }
  DBUG_VOID_RETURN;
}


/**
  Remove all cached queries that uses the given table
*/

void Query_cache::invalidate(THD *thd, TABLE *table, bool using_transactions)
{
  DBUG_ENTER("Query_cache::invalidate (table)");
  if (is_disabled())
    DBUG_VOID_RETURN;

  using_transactions= using_transactions && thd->in_multi_stmt_transaction_mode();
  if (using_transactions &&
      (table->file->table_cache_type() == HA_CACHE_TBL_TRANSACT))
    thd->get_transaction()->
      add_changed_table(table->s->table_cache_key.str,
                        static_cast<uint32>(table->s->table_cache_key.length));
  else
    invalidate_table(thd, table);


  DBUG_VOID_RETURN;
}


void Query_cache::invalidate(THD *thd, const char *key, uint32  key_length,
			     bool using_transactions)
{
  DBUG_ENTER("Query_cache::invalidate (key)");
  if (is_disabled())
   DBUG_VOID_RETURN;

  using_transactions= using_transactions && thd->in_multi_stmt_transaction_mode();
  if (using_transactions) // used for innodb => has_transactions() is true
    thd->get_transaction()->add_changed_table(key, key_length);
  else
    invalidate_table(thd, reinterpret_cast<const uchar*>(key), key_length);

  DBUG_VOID_RETURN;
}


/**
  Remove all cached queries that uses the given database.
*/

void Query_cache::invalidate(THD *thd, const char *db)
{
  DBUG_ENTER("Query_cache::invalidate (db)");
  if (is_disabled())
    DBUG_VOID_RETURN;

  bool restart= false;
  /*
    Lock the query cache and queue all invalidation attempts to avoid
    the risk of a race between invalidation, cache inserts and flushes.
  */
  lock(thd);

  if (query_cache_size > 0)
  {
    if (tables_blocks)
    {
      Query_cache_block *table_block = tables_blocks;
      do {
        restart= false;
        do
        {
          Query_cache_block *next= table_block->next;
          Query_cache_table *table = table_block->table();
          if (strcmp(table->db(),db) == 0)
          {
            Query_cache_block_table *list_root= table_block->table(0);
            invalidate_query_block_list(list_root);
          }

          table_block= next;

          /*
            If our root node to used tables became null then the last element
            in the table list was removed when a query was invalidated;
            Terminate the search.
          */
          if (tables_blocks == NULL)
          {
            table_block= tables_blocks;
          }
          /*
            If the iterated list has changed underlying structure;
            we need to restart the search.
          */
          else if (table_block->type == Query_cache_block::FREE)
          {
            restart= true;
            table_block= tables_blocks;
          }
          /* 
            The used tables are linked in a circular list;
            loop until we return to the begining.
          */
        } while (table_block != tables_blocks);
        /*
           Invalidating a table will also mean that all cached queries using
           this table also will be invalidated. This will in turn change the
           list of tables associated with these queries and the linked list of
           used table will be changed. Because of this we might need to restart
           the search when a table has been invalidated.
        */
      } while (restart);
    } // end if( tables_blocks )
  }
  unlock(thd);

  DBUG_VOID_RETURN;
}


void Query_cache::invalidate_by_MyISAM_filename(THD *thd, const char *filename)
{
  DBUG_ENTER("Query_cache::invalidate_by_MyISAM_filename");

  /* Calculate the key outside the lock to make the lock shorter */
  char key[MAX_DBKEY_LENGTH];
  size_t db_length;
  size_t key_length= filename_2_table_key(key, filename, &db_length);
  invalidate_table(thd, reinterpret_cast<const uchar *>(key), key_length);
  DBUG_VOID_RETURN;
}

  /* Remove all queries from cache */

void Query_cache::flush(THD *thd)
{
  DBUG_ENTER("Query_cache::flush");
  if (is_disabled())
    DBUG_VOID_RETURN;

  DEBUG_SYNC(thd, "wait_in_query_cache_flush1");

  lock_and_suspend(thd);
  if (query_cache_size > 0)
  {
    flush_cache(thd);
  }

  unlock(thd);
  DBUG_VOID_RETURN;
}


/**
  Rearrange the memory blocks and join result in cache in 1 block (if
  result length > join_limit)
*/

void Query_cache::pack(THD *thd)
{
  DBUG_ENTER("Query_cache::pack");

  // If the minimum length of a result block to be joined.
  ulong join_limit= QUERY_CACHE_PACK_LIMIT;
  // The maximum number of packing and joining sequences.
  uint iteration_limit= QUERY_CACHE_PACK_ITERATION;

  if (is_disabled())
    DBUG_VOID_RETURN;

  /*
    If the entire qc is being invalidated we can bail out early
    instead of waiting for the lock.
  */
  if (try_lock(thd, false))
    DBUG_VOID_RETURN;

  if (query_cache_size == 0)
  {
    unlock(thd);
    DBUG_VOID_RETURN;
  }

  uint i = 0;
  do
  {
    pack_cache();
  } while ((++i < iteration_limit) && join_results(join_limit));

  unlock(thd);
  DBUG_VOID_RETURN;
}


void Query_cache::destroy(THD *thd)
{
  DBUG_ENTER("Query_cache::destroy");
  if (!initialized)
  {
    DBUG_PRINT("qcache", ("Query Cache not initialized"));
  }
  else
  {
    /* Underlying code expects the lock. */
    lock_and_suspend(thd);
    free_cache();
    unlock(thd);

    mysql_cond_destroy(&COND_cache_status_changed);
    mysql_mutex_destroy(&structure_guard_mutex);
    initialized= false;
  }
  DBUG_VOID_RETURN;
}


/*****************************************************************************
  init/destroy
*****************************************************************************/

void Query_cache::init()
{
  DBUG_ENTER("Query_cache::init");
  mysql_mutex_init(key_structure_guard_mutex,
                   &structure_guard_mutex, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_cache_status_changed,
                  &COND_cache_status_changed);
  m_cache_lock_status= Query_cache::UNLOCKED;
  initialized= true;
  /*
    If we explicitly turn off query cache from the command line query cache will
    be disabled for the reminder of the server life time. This is because we
    want to avoid locking the QC specific mutex if query cache isn't going to
    be used.
  */
  if (global_system_variables.query_cache_type == 0)
    query_cache.disable_query_cache();

  DBUG_VOID_RETURN;
}


ulong Query_cache::init_cache()
{
  uint mem_bin_count, num, step;
  ulong mem_bin_size, prev_size, inc;
  ulong additional_data_size, max_mem_bin_size, approx_additional_data_size;
  ulong align;

  DBUG_ENTER("Query_cache::init_cache");

  approx_additional_data_size = (/* FIXME: WHAT FOR ? sizeof(Query_cache) + */
				 sizeof(uchar*)*(def_query_hash_size+
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
  mem_bin_count= static_cast<uint>((1 + QUERY_CACHE_MEM_BIN_PARTS_INC) *
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
    mem_bin_count*= QUERY_CACHE_MEM_BIN_PARTS_MUL;

    // Prevent too small bins spacing
    if (mem_bin_count > (mem_bin_size >> QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2))
      mem_bin_count= static_cast<uint>(mem_bin_size >> QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2);
  }
  inc = (prev_size - mem_bin_size) / mem_bin_count;
  mem_bin_num += static_cast<uint>((mem_bin_count - (min_allocation_unit - mem_bin_size)/inc));
  mem_bin_steps++;
  additional_data_size = ((mem_bin_num+1) *
			  ALIGN_SIZE(sizeof(Query_cache_memory_bin))+
			  (mem_bin_steps *
			   ALIGN_SIZE(sizeof(Query_cache_memory_bin_step))));

  if (query_cache_size < additional_data_size)
    goto err;
  query_cache_size -= additional_data_size;

  if (!(cache= static_cast<uchar *>
        (my_malloc(key_memory_Query_cache,
                   query_cache_size+additional_data_size, MYF(0)))))
    goto err;

  DBUG_PRINT("qcache", ("cache length %lu, min unit %lu, %u bins",
		      query_cache_size, min_allocation_unit, mem_bin_num));

  steps= reinterpret_cast<Query_cache_memory_bin_step *>(cache);
  bins= reinterpret_cast<Query_cache_memory_bin *>(cache + mem_bin_steps *
                                                   ALIGN_SIZE(sizeof(Query_cache_memory_bin_step)));

  first_block= reinterpret_cast<Query_cache_block *>(cache + additional_data_size);
  first_block->init(query_cache_size);
  total_blocks++;
  first_block->pnext=first_block->pprev=first_block;
  first_block->next=first_block->prev=first_block;

  /* Prepare bins */

  bins[0].init(max_mem_bin_size);
  steps[0].init(max_mem_bin_size,0,0);
  mem_bin_count= static_cast<uint>((1 + QUERY_CACHE_MEM_BIN_PARTS_INC) *
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
    mem_bin_count*= QUERY_CACHE_MEM_BIN_PARTS_MUL;
    if (mem_bin_count > (mem_bin_size >> QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2))
      mem_bin_count= static_cast<uint>(mem_bin_size >> QUERY_CACHE_MEM_BIN_SPC_LIM_PWR2);
  }
  inc = (steps[step-1].size - mem_bin_size) / mem_bin_count;

  /*
    num + mem_bin_count > mem_bin_num, but index never be > mem_bin_num
    because block with size < min_allocated_unit never will be requested
  */

  steps[step].init(mem_bin_size, num + mem_bin_count - 1, inc);
  {
    ulong skiped = (min_allocation_unit - mem_bin_size)/inc;
    ulong size = mem_bin_size + inc*skiped;
    ulong i = mem_bin_count - skiped;
    while (i-- > 0)
    {
      bins[num+i].init(size);
      size += inc;
    }
  }
  bins[mem_bin_num].number = 1;	// For easy end test in get_free_block
  free_memory = free_memory_blocks = 0;
  insert_into_free_memory_list(first_block);

  (void) my_hash_init(&queries, &my_charset_bin, def_query_hash_size, 0,
                      query_cache_query_get_key, nullptr, 0,
                      key_memory_Query_cache);
#ifndef FN_NO_CASE_SENSE
  /*
    If lower_case_table_names!=0 then db and table names are already 
    converted to lower case and we can use binary collation for their 
    comparison (no matter if file system case sensitive or not).
    If we have case-sensitive file system (like on most Unixes) and
    lower_case_table_names == 0 then we should distinguish my_table
    and MY_TABLE cases and so again can use binary collation.
  */
  (void) my_hash_init(&tables, &my_charset_bin, def_table_hash_size, 0,
                      query_cache_table_get_key, nullptr, 0,
                      key_memory_Query_cache);
#else
  /*
    On windows, OS/2, MacOS X with HFS+ or any other case insensitive
    file system if lower_case_table_names!=0 we have same situation as
    in previous case, but if lower_case_table_names==0 then we should
    not distinguish cases (to be compatible in behavior with underlying
    file system) and so should use case insensitive collation for
    comparison.
  */
  (void) my_hash_init(&tables,
                      lower_case_table_names ? &my_charset_bin :
                      files_charset_info,
                      def_table_hash_size, 0, query_cache_table_get_key,
                      nullptr, 0,
                      key_memory_Query_cache);
#endif

  queries_in_cache = 0;
  queries_blocks = 0;
  DBUG_RETURN(query_cache_size +
	      additional_data_size + approx_additional_data_size);

err:
  make_disabled();
  DBUG_RETURN(0);
}


/**
  Disable the use of the query cache
*/

void Query_cache::make_disabled()
{
  DBUG_ENTER("Query_cache::make_disabled");
  query_cache_size= 0;
  queries_blocks= 0;
  free_memory= 0;
  free_memory_blocks= 0;
  bins= 0;
  steps= 0;
  cache= 0;
  mem_bin_num= 0;
  mem_bin_steps= 0;
  queries_in_cache= 0;
  first_block= 0;
  total_blocks= 0;
  tables_blocks= 0;
  DBUG_VOID_RETURN;
}


/**
  Free all resources allocated by the cache.

  This function frees all resources allocated by the cache.  You
  have to call init_cache() before using the cache again. This function
  requires the structure_guard_mutex to be locked.
*/

void Query_cache::free_cache()
{
  DBUG_ENTER("Query_cache::free_cache");

  my_free(cache);
  make_disabled();
  my_hash_free(&queries);
  my_hash_free(&tables);
  DBUG_VOID_RETURN;
}


/*****************************************************************************
  Free block data
*****************************************************************************/


/**
  Flush the cache.

  This function will flush cache contents.  It assumes we have
  'structure_guard_mutex' locked. The function sets the m_cache_status flag and
  releases the lock, so other threads may proceed skipping the cache as if it
  is disabled. Concurrent flushes are performed in turn.
  After flush_cache() call, the cache is flushed, all the freed memory is
  accumulated in bin[0], and the 'structure_guard_mutex' is locked. However,
  since we could release the mutex during execution, the rest of the cache
  state could have been changed, and should not be relied on.
*/

void Query_cache::flush_cache(THD *thd MY_ATTRIBUTE((unused)))
{
  DEBUG_SYNC(thd, "wait_in_query_cache_flush2");

  my_hash_reset(&queries);
  while (queries_blocks != NULL)
  {
    mysql_rwlock_wrlock(&queries_blocks->query()->lock);
    free_query_internal(queries_blocks);
  }
}


/**
  Free oldest query that is not in use by another thread.
  Returns true if we couldn't remove anything
*/

bool Query_cache::free_old_query()
{
  DBUG_ENTER("Query_cache::free_old_query");
  if (queries_blocks)
  {
    /*
      try_lock_writing used to prevent client because here lock
      sequence is breached.
      Also we don't need remove locked queries at this point.
    */
    Query_cache_block *query_block= NULL;
    if (queries_blocks != NULL)
    {
      Query_cache_block *block = queries_blocks;
      /* Search until we find first query that we can remove */
      do
      {
	Query_cache_query *header = block->query();
	if (header->result != NULL &&
	    header->result->type == Query_cache_block::RESULT &&
	    block->query()->try_lock_writing())
	{
	  query_block = block;
	  break;
	}
      } while ((block=block->next) != queries_blocks );
    }

    if (query_block != NULL)
    {
      free_query(query_block);
      lowmem_prunes++;
      DBUG_RETURN(false);
    }
  }
  DBUG_RETURN(true);				// Nothing to remove
}


/**
  free query from query cache.

  @param query_block           Query_cache_block representing the query

  This function will remove the query from a cache, and place its
  memory blocks to the list of free blocks.  'query_block' must be
  locked for writing, this function will release (and destroy) this
  lock.

  @note 'query_block' should be removed from 'queries' hash _before_
  calling this method, as the lock will be destroyed here.
*/

void Query_cache::free_query_internal(Query_cache_block *query_block)
{
  DBUG_ENTER("Query_cache::free_query_internal");

  queries_in_cache--;

  Query_cache_query *query= query_block->query();

  if (query->writer != NULL)
  {
    /* Tell MySQL that this query should not be cached anymore */
    query->writer->first_query_cache_block= NULL;
    query->writer= NULL;
  }
  double_linked_list_exclude(query_block, &queries_blocks);
  Query_cache_block_table *table= query_block->table(0);

  for (TABLE_COUNTER_TYPE i= 0; i < query_block->n_tables; i++)
    unlink_table(table++);
  Query_cache_block *result_block= query->result;

  /*
    The following is true when query destruction was called and no results
    in query . (query just registered and then abort/pack/flush called)
  */
  if (result_block != NULL)
  {
    if (result_block->type != Query_cache_block::RESULT)
    {
      // removing unfinished query
      refused++;
      inserts--;
    }
    Query_cache_block *block= result_block;
    do
    {
      Query_cache_block *current= block;
      block= block->next;
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


/**
  free query from query cache.

  @param query_block           Query_cache_block representing the query

  @note This function will remove 'query_block' from 'queries' hash, and
  then call free_query_internal(), which see.
*/

void Query_cache::free_query(Query_cache_block *query_block)
{
  DBUG_ENTER("Query_cache::free_query");

  my_hash_delete(&queries, reinterpret_cast<uchar *>(query_block));
  free_query_internal(query_block);

  DBUG_VOID_RETURN;
}


/*****************************************************************************
 Query data creation
*****************************************************************************/

Query_cache_block *
Query_cache::write_block_data(size_t data_len, const uchar* data,
                              size_t header_len,
                              Query_cache_block::block_type type,
                              TABLE_COUNTER_TYPE ntab)
{
  size_t all_headers_len= (ALIGN_SIZE(sizeof(Query_cache_block)) +
                          ALIGN_SIZE(ntab*sizeof(Query_cache_block_table)) +
                          header_len);
  size_t len = data_len + all_headers_len;
  size_t align_len= ALIGN_SIZE(len);
  DBUG_ENTER("Query_cache::write_block_data");

  Query_cache_block *block= allocate_block(max<size_t>(align_len,
                                           min_allocation_unit), true, 0);
  if (block != NULL)
  {
    block->type = type;
    block->n_tables = ntab;
    block->used = static_cast<ulong>(len);

    memcpy(reinterpret_cast<uchar *>(block) + all_headers_len, data, data_len);
  }
  DBUG_RETURN(block);
}


bool
Query_cache::append_result_data(THD *thd, Query_cache_block **current_block,
				size_t data_len, const uchar* data,
				Query_cache_block *query_block)
{
  DBUG_ENTER("Query_cache::append_result_data");

  if (query_block->query()->add(data_len) > query_cache_limit)
  {
    DBUG_PRINT("qcache", ("size limit reached %lu > %lu",
                          query_block->query()->length,
                          query_cache_limit));
    DBUG_RETURN(false);
  }
  if (*current_block == NULL)
  {
    DBUG_PRINT("qcache", ("allocated first result data block %zu", data_len));
    DBUG_RETURN(write_result_data(thd, current_block, data_len, data, query_block,
				  Query_cache_block::RES_BEG));
  }
  Query_cache_block *last_block = (*current_block)->prev;

  DBUG_PRINT("qcache", ("lastblock %p len %lu used %lu",
                        last_block, last_block->length,
                        last_block->used));
  bool success= true;
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
    Query_cache_block *new_block= NULL;
    success = write_result_data(thd, &new_block, data_len-last_block_free_space,
				data+last_block_free_space,
				query_block,
				Query_cache_block::RES_CONT);
    /*
       new_block may be != NULL even !success (if write_result_data
       allocate a small block but failed to allocate continue)
    */
    if (new_block != NULL)
      double_linked_list_join(last_block, new_block);
  }
  else
  {
    // It is success (nobody can prevent us write data)
    unlock(thd);
  }

  // Now finally write data to the last block
  if (success && last_block_free_space > 0)
  {
    size_t to_copy = min<size_t>(data_len,last_block_free_space);
    memcpy(reinterpret_cast<uchar*>(last_block) + last_block->used, data, to_copy);
    last_block->used+=to_copy;
  }
  DBUG_RETURN(success);
}


bool Query_cache::write_result_data(THD *thd,
                                    Query_cache_block **result_block,
                                    size_t data_len, const uchar* data,
                                    Query_cache_block *query_block,
                                    Query_cache_block::block_type type)
{
  DBUG_ENTER("Query_cache::write_result_data");

  /*
    Reserve block(s) for filling
    During data allocation we must have structure_guard_mutex locked.
    As data copy is not a fast operation, it's better if we don't have
    structure_guard_mutex locked during data coping.
    Thus we first allocate space and lock query, then unlock
    structure_guard_mutex and copy data.
  */

  bool success= allocate_data_chain(result_block, data_len, query_block,
                                    type == Query_cache_block::RES_BEG);
  if (success)
  {
    // It is success (nobody can prevent us write data)
    unlock(thd);
    uint headers_len = (ALIGN_SIZE(sizeof(Query_cache_block)) +
			ALIGN_SIZE(sizeof(Query_cache_result)));
    Query_cache_block *block= *result_block;
    const uchar *rest= data;
    // Now fill list of blocks that created by allocate_data_chain
    do
    {
      block->type = type;
      ulong length = block->used - headers_len;
      DBUG_PRINT("qcache", ("write %lu byte in block %p",length, block));
      memcpy(reinterpret_cast<uchar*>(block) + headers_len, rest, length);
      rest += length;
      block = block->next;
      type = Query_cache_block::RES_CONT;
    } while (block != *result_block);
  }
  else
  {
    if (*result_block != NULL)
    {
      // Destroy list of blocks that was created & locked by lock_result_data
      Query_cache_block *block = *result_block;
      do
      {
	Query_cache_block *current = block;
	block = block->next;
	free_memory_block(current);
      } while (block != *result_block);
      *result_block= NULL;
      /*
	It is not success => not unlock structure_guard_mutex (we need it to
	free query)
      */
    }
  }
  DBUG_PRINT("qcache", ("success %d", success));
  DBUG_RETURN(success);
}


inline ulong Query_cache::get_min_first_result_data_size() const
{
  if (queries_in_cache < QUERY_CACHE_MIN_ESTIMATED_QUERIES_NUMBER)
    return min_result_data_size;
  ulong avg_result = (query_cache_size - free_memory) / queries_in_cache;
  avg_result = min(avg_result, query_cache_limit);
  return max(min_result_data_size, avg_result);
}


/**
  Allocate one or more blocks to hold data
*/

bool Query_cache::allocate_data_chain(Query_cache_block **result_block,
                                      size_t data_len,
                                      Query_cache_block *query_block,
                                      bool first_block_arg)
{
  size_t all_headers_len = (ALIGN_SIZE(sizeof(Query_cache_block)) +
                            ALIGN_SIZE(sizeof(Query_cache_result)));
  size_t min_size = (first_block_arg ?
                     get_min_first_result_data_size():
                     get_min_append_result_data_size());
  Query_cache_block *prev_block= NULL;
  Query_cache_block *new_block;
  DBUG_ENTER("Query_cache::allocate_data_chain");

  do
  {
    size_t len= data_len + all_headers_len;
    size_t align_len= ALIGN_SIZE(len);

    if (!(new_block= allocate_block(max(min_size, align_len),
				    min_result_data_size == 0,
				    all_headers_len + min_result_data_size)))
    {
      DBUG_PRINT("warning", ("Can't allocate block for results"));
      DBUG_RETURN(false);
    }

    new_block->n_tables = 0;
    new_block->used = min<ulong>(len, new_block->length);
    new_block->type = Query_cache_block::RES_INCOMPLETE;
    new_block->next = new_block->prev = new_block;
    Query_cache_result *header = new_block->result();
    header->parent= query_block;

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
  } while (true);

  DBUG_RETURN(true);
}

/*****************************************************************************
  Tables management
*****************************************************************************/

/**
  Invalidate the first table in the table_list
*/

void Query_cache::invalidate_table(THD *thd, TABLE_LIST *table_list)
{
  if (table_list->table != NULL)
    invalidate_table(thd, table_list->table);	// Table is open
  else
  {
    const char *key;
    size_t key_length;
    key_length= get_table_def_key(table_list, &key);

    // We don't store temporary tables => no key_length+=4 ...
    invalidate_table(thd, reinterpret_cast<const uchar *>(key), key_length);
  }
}


void Query_cache::invalidate_table(THD *thd, TABLE *table)
{
  invalidate_table(thd,
                   reinterpret_cast<const uchar*>(table->s->table_cache_key.str),
                   table->s->table_cache_key.length);
}


void Query_cache::invalidate_table(THD *thd, const uchar * key, size_t key_length)
{
  DEBUG_SYNC(thd, "wait_in_query_cache_invalidate1");

  /*
    Lock the query cache and queue all invalidation attempts to avoid
    the risk of a race between invalidation, cache inserts and flushes.
  */
  lock(thd);

  DEBUG_SYNC(thd, "wait_in_query_cache_invalidate2");

  if (query_cache_size > 0)
    invalidate_table_internal(key, key_length);

  unlock(thd);
}


/**
  Try to locate and invalidate a table by name.
  The caller must ensure that no other thread is trying to work with
  the query cache when this function is executed.

  @pre structure_guard_mutex is acquired or LOCKED is set.
*/

void
Query_cache::invalidate_table_internal(const uchar *key, size_t key_length)
{
  Query_cache_block *table_block=
    reinterpret_cast<Query_cache_block*>(my_hash_search(&tables, key, key_length));
  if (table_block)
  {
    Query_cache_block_table *list_root= table_block->table(0);
    invalidate_query_block_list(list_root);
  }
}


/**
  Invalidate a linked list of query cache blocks.

  Each block tries to acquire a block level lock before
  free_query is a called. This function will in turn affect
  related table- and result-blocks.

  @param[in,out] list_root A pointer to a circular list of query blocks.

*/

void
Query_cache::invalidate_query_block_list(Query_cache_block_table *list_root)
{
  while (list_root->next != list_root)
  {
    Query_cache_block *query_block= list_root->next->block();
    mysql_rwlock_wrlock(&query_block->query()->lock);
    free_query(query_block);
  }
}


/**
  Register given table list begining with given position in tables table of
  block

  @param tables_used     given table list
  @param counter         number current position in table of tables of block
  @param block_table     pointer to current position in tables table of block

  @retval 0   error
  @retval >0  number of next position of table entry in table of tables of block
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
    if (tables_used->is_derived() || tables_used->is_recursive_reference())
    {
      DBUG_PRINT("qcache", ("derived table or recursive reference skipped"));
      n--;
      block_table--;
      continue;
    }
    block_table->n= n;
    if (tables_used->is_view())
    {
      const char *key;
      size_t key_length;
      DBUG_PRINT("qcache", ("view: %s  db: %s",
                            tables_used->view_name.str,
                            tables_used->view_db.str));
      key_length= get_table_def_key(tables_used, &key);
      /*
        There are not callback function for for VIEWs
      */
      if (!insert_table(key_length, reinterpret_cast<const uchar*>(key),
                        block_table, tables_used->view_db.length + 1,
                        HA_CACHE_TBL_NONTRANSACT, NULL, 0))
        DBUG_RETURN(0);
      /*
        We do not need to register view tables here because they are already
        present in the global list.
      */
    }
    else
    {
      DBUG_PRINT("qcache",
                 ("table: %s  db: %s  openinfo:  %p  keylen: %zu  key: %p",
                  tables_used->table->s->table_name.str,
                  tables_used->table->s->table_cache_key.str,
                  tables_used->table,
                  tables_used->table->s->table_cache_key.length,
                  tables_used->table->s->table_cache_key.str));

      if (!insert_table(tables_used->table->s->table_cache_key.length,
                        reinterpret_cast<const uchar*>(tables_used->table->s->table_cache_key.str),
                        block_table,
                        tables_used->db_length,
                        tables_used->table->file->table_cache_type(),
                        tables_used->callback_func,
                        tables_used->engine_data))
        DBUG_RETURN(0);

      /*
        XXX FIXME: Some generic mechanism is required here instead of this
        MYISAMMRG-specific implementation.
      */
      if (tables_used->table->s->db_type()->db_type == DB_TYPE_MRG_MYISAM)
      {
        ha_myisammrg *handler= static_cast<ha_myisammrg *>(tables_used->table->file);
        MYRG_INFO *file = handler->myrg_info();
        for (MYRG_TABLE *table = file->open_tables;
             table != file->end_table ;
             table++)
        {
          char key[MAX_DBKEY_LENGTH];
          size_t db_length;
          size_t key_length= filename_2_table_key(key, table->table->filename,
                                                  &db_length);
          (++block_table)->n= ++n;
          /*
            There are not callback function for for MyISAM, and engine data
          */
          if (!insert_table(key_length, reinterpret_cast<const uchar*>(key),
                            block_table, db_length,
                            tables_used->table->file->table_cache_type(),
                            NULL, 0))
            DBUG_RETURN(0);
        }
      }
    }
  }
  DBUG_RETURN(n - counter);
}


/**
  Store all used tables

  @param block		Store tables in this block
  @param tables_used	List if used tables
*/

bool Query_cache::register_all_tables(Query_cache_block *block,
                                      TABLE_LIST *tables_used)
{
  TABLE_COUNTER_TYPE n;

  Query_cache_block_table *block_table = block->table(0);

  n= register_tables_from_list(tables_used, 0, block_table);

  if (n==0)
  {
    /* Unlink the tables we allocated above */
    for (Query_cache_block_table *tmp = block->table(0) ;
	 tmp != block_table;
	 tmp++)
      unlink_table(tmp);
  }
  return n != 0;
}


/**
  Insert used table name into the cache.

  @return Error status
    @retval false On error
    @retval true On success
*/

bool
Query_cache::insert_table(size_t key_len, const uchar *key,
                          Query_cache_block_table *node,
                          size_t db_length, uint8 cache_type,
                          qc_engine_callback callback,
                          ulonglong engine_data)
{
  DBUG_ENTER("Query_cache::insert_table");

  Query_cache_block *table_block= reinterpret_cast<Query_cache_block *>
    (my_hash_search(&tables, key, key_len));

  if (table_block &&
      table_block->table()->engine_data != engine_data)
  {
    DBUG_PRINT("qcache",
               ("Handler require invalidation queries of %s.%s %llu-%llu",
                table_block->table()->db(),
                table_block->table()->table,
                engine_data,
                table_block->table()->engine_data));
    /*
      as far as we delete all queries with this table, table block will be
      deleted, too
    */
    {
      Query_cache_block_table *list_root= table_block->table(0);
      invalidate_query_block_list(list_root);
    }

    table_block= NULL;
  }

  if (table_block == NULL)
  {
    DBUG_PRINT("qcache", ("new table block from %p (%zu)", key, key_len));
    table_block= write_block_data(key_len, key,
                                  ALIGN_SIZE(sizeof(Query_cache_table)),
                                  Query_cache_block::TABLE, 1);
    if (table_block == NULL)
    {
      DBUG_PRINT("qcache", ("Can't write table name to cache"));
      DBUG_RETURN(false);
    }
    Query_cache_table *header= table_block->table();
    double_linked_list_simple_include(table_block,
                                      &tables_blocks);
    /*
      First node in the Query_cache_block_table-chain is the table-type
      block. This block will only have one Query_cache_block_table (n=0).
    */
    Query_cache_block_table *list_root= table_block->table(0);
    list_root->n= 0;

    /*
      The node list is circular in nature.
    */
    list_root->next= list_root->prev= list_root;

    if (my_hash_insert(&tables, reinterpret_cast<const uchar *>(table_block)))
    {
      DBUG_PRINT("qcache", ("Can't insert table to hash"));
      // write_block_data return locked block
      free_memory_block(table_block);
      DBUG_RETURN(false);
    }
    const char *db= header->db();
    header->table= db + db_length + 1;
    header->key_length(key_len);
    header->table_type= cache_type;
    header->callback= callback;
    header->engine_data= engine_data;

    /*
      We insert this table without the assumption that it isn't refrenenced by
      any queries.
    */
    header->m_cached_query_count= 0;
  }

  /*
    Table is now in the cache; link the table_block-node associated
    with the currently processed query into the chain of queries depending
    on the cached table.
  */
  Query_cache_block_table *list_root= table_block->table(0);
  node->next= list_root->next;
  list_root->next= node;
  node->next->prev= node;
  node->prev= list_root;
  node->parent= table_block->table();
  /*
    Increase the counter to keep track on how long this chain
    of queries is.
  */
  Query_cache_table *table_block_data= table_block->table();
  table_block_data->m_cached_query_count++;
  DBUG_RETURN(true);
}


void Query_cache::unlink_table(Query_cache_block_table *node)
{
  DBUG_ENTER("Query_cache::unlink_table");
  node->prev->next= node->next;
  node->next->prev= node->prev;
  Query_cache_block_table *neighbour= node->next;
  Query_cache_table *table_block_data= node->parent;
  table_block_data->m_cached_query_count--;

  DBUG_ASSERT(table_block_data->m_cached_query_count >= 0);

  if (neighbour->next == neighbour)
  {
    DBUG_ASSERT(table_block_data->m_cached_query_count == 0);
    /*
      If neighbor is root of list, the list is empty.
      The root of the list is always a table-type block
      which contain exactly one Query_cache_block_table
      node object, thus we can use the block() method
      to calculate the Query_cache_block address.
    */
    Query_cache_block *table_block= neighbour->block();
    double_linked_list_exclude(table_block,
                               &tables_blocks);
    my_hash_delete(&tables, reinterpret_cast<uchar *>(table_block));
    free_memory_block(table_block);
  }
  DBUG_VOID_RETURN;
}

/*****************************************************************************
  Free memory management
*****************************************************************************/

Query_cache_block *
Query_cache::allocate_block(size_t len, bool not_less, size_t minimum)
{
  DBUG_ENTER("Query_cache::allocate_block");

  if (len >= min(query_cache_size, query_cache_limit))
  {
    DBUG_PRINT("qcache", ("Query cache hase only %lu memory and limit %lu",
                          query_cache_size, query_cache_limit));
    DBUG_RETURN(NULL); // in any case we don't have such piece of memory
  }

  /* Free old queries until we have enough memory to store this block */
  Query_cache_block *block;
  do
  {
    block= get_free_block(len, not_less, minimum);
  }
  while (block == NULL && !free_old_query());

  if (block != NULL)				// If we found a suitable block
  {
    if (block->length >= ALIGN_SIZE(len) + min_allocation_unit)
      split_block(block,ALIGN_SIZE(len));
  }

  DBUG_RETURN(block);
}


Query_cache_block *
Query_cache::get_free_block(size_t len, bool not_less, size_t min)
{
  Query_cache_block *block= NULL, *first= NULL;
  DBUG_ENTER("Query_cache::get_free_block");

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
  if (block == NULL && start > 0)
  {
    DBUG_PRINT("qcache",("Try bins with bigger block size"));
    // Try more big bins
    int i= static_cast<int>(start - 1);
    while (i > 0 && bins[i].number == 0)
      i--;
    if (bins[i].number > 0)
      block = bins[i].free_blocks;
  }

  // If no big blocks => try less size (if it is possible)
  if (block == NULL && ! not_less)
  {
    DBUG_PRINT("qcache",("Try to allocate a smaller block"));
    if (first != NULL && first->length > min)
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
  if (block != NULL)
    exclude_from_free_memory_list(block);

  DBUG_PRINT("qcache",("getting block %p", block));
  DBUG_RETURN(block);
}


void Query_cache::free_memory_block(Query_cache_block *block)
{
  DBUG_ENTER("Query_cache::free_memory_block");
  block->used=0;
  block->type= Query_cache_block::FREE; // mark block as free in any case

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
  Query_cache_block *new_block=
    reinterpret_cast<Query_cache_block*>(reinterpret_cast<uchar*>(block)+len);

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

  DBUG_PRINT("qcache", ("split %p (%lu) new %p",
                        block, len, new_block));
  DBUG_VOID_RETURN;
}


Query_cache_block *
Query_cache::join_free_blocks(Query_cache_block *first_block_arg,
			      Query_cache_block *block_in_list)
{
  Query_cache_block *second_block;
  DBUG_ENTER("Query_cache::join_free_blocks");

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


bool Query_cache::append_next_free_block(Query_cache_block *block,
                                         ulong add_size)
{
  Query_cache_block *next_block = block->pnext;
  DBUG_ENTER("Query_cache::append_next_free_block");

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
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}


void Query_cache::exclude_from_free_memory_list(Query_cache_block *free_block)
{
  DBUG_ENTER("Query_cache::exclude_from_free_memory_list");
  Query_cache_memory_bin *bin = *(reinterpret_cast<Query_cache_memory_bin **>
				  (free_block->data()));
  double_linked_list_exclude(free_block, &bin->free_blocks);
  bin->number--;
  free_memory-=free_block->length;
  free_memory_blocks--;
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
  Query_cache_memory_bin **bin_ptr=
    reinterpret_cast<Query_cache_memory_bin**>(free_block->data());
  *bin_ptr = bins+idx;
  (*bin_ptr)->number++;

  DBUG_VOID_RETURN;
}


uint Query_cache::find_bin(size_t size)
{
  DBUG_ENTER("Query_cache::find_bin");
  // Binary search
  uint left = 0, right = mem_bin_steps;
  do
  {
    uint middle = (left + right) / 2;
    if (steps[middle].size > size)
      left = middle+1;
    else
      right = middle;
  } while (left < right);
  if (left == 0)
  {
    // first bin not subordinate of common rules
    DBUG_PRINT("qcache", ("first bin (# 0), size %zu",size));
    DBUG_RETURN(0);
  }
  uint bin =  steps[left].idx -
    static_cast<uint>((size - steps[left].size)/steps[left].increment);

  DBUG_PRINT("qcache", ("bin %u step %u, size %zu step size %lu",
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

  if (*list == NULL)
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


void double_linked_list_simple_include(Query_cache_block *point,
                                       Query_cache_block **
                                       list_pointer)
{
  if (*list_pointer == NULL)
    *list_pointer=point->next=point->prev=point;
  else
  {
    // insert to the end of list
    point->next = (*list_pointer);
    point->prev = (*list_pointer)->prev;
    point->prev->next = point;
    (*list_pointer)->prev = point;
  }
}


void double_linked_list_exclude(Query_cache_block *point,
                                Query_cache_block **list_pointer)
{
  if (point->next == point)
    *list_pointer= NULL;				// empty list
  else
  {
    point->next->prev = point->prev;
    point->prev->next = point->next;
    /*
       If the root is removed; select a new root
    */
    if (point == *list_pointer)
      *list_pointer= point->next;
  }
}


void double_linked_list_join(Query_cache_block *head_tail,
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

/**
  Collect information about table types, check that tables are cachable and
  count them

  @param thd             thread handle
  @param tables_used     table list for processing
  @param tables_type     pointer to variable for table types collection

  @retval 0   error
  @retval >0  number of tables
*/

TABLE_COUNTER_TYPE
Query_cache::process_and_count_tables(THD *thd, TABLE_LIST *tables_used,
                                      uint8 *tables_type) const
{
  DBUG_ENTER("process_and_count_tables");
  TABLE_COUNTER_TYPE table_count = 0;
  for (; tables_used; tables_used= tables_used->next_global)
  {
    table_count++;

    /*
      Disable any attempt to store this statement if there are
      column level grants on any referenced tables.
      The grant.want_privileges flag was set to 1 in the
      check_grant() function earlier if the TABLE_LIST object
      had any associated column privileges.

      We need to check that the TABLE_LIST object isn't part
      of a VIEW definition because we want to be able to cache
      views.

      Tables underlying a MERGE table does not have useful privilege
      information in their grant objects, so skip these tables from the test.
    */
    if (tables_used->belong_to_view == NULL &&
        (!tables_used->parent_l ||
         tables_used->parent_l->table->file->ht->db_type != DB_TYPE_MRG_MYISAM))
    {
      DBUG_ASSERT((SELECT_ACL & ~tables_used->grant.privilege) ==
                  tables_used->grant.want_privilege);
      if ((tables_used->grant.privilege & SELECT_ACL) == 0)
      {
        DBUG_PRINT("qcache", ("Don't cache statement as it refers to "
                              "tables with column privileges."));
        thd->lex->safe_to_cache_query= false;
        DBUG_RETURN(0);
      }
    }

    if (tables_used->is_view())
    {
      DBUG_PRINT("qcache", ("view: %s  db: %s",
                            tables_used->view_name.str,
                            tables_used->view_db.str));
      *tables_type|= HA_CACHE_TBL_NONTRANSACT;
    }
    else
    {
      if (tables_used->is_derived() || tables_used->is_recursive_reference())
      {
        DBUG_PRINT("qcache", ("table: %s", tables_used->alias));
        table_count--;
        DBUG_PRINT("qcache", ("derived table or recursive reference skipped"));
        continue;
      }
      DBUG_PRINT("qcache", ("table: %s  db:  %s  type: %u",
                            tables_used->table->s->table_name.str,
                            tables_used->table->s->db.str,
                            tables_used->table->s->db_type()->db_type));
      *tables_type|= tables_used->table->file->table_cache_type();

      /*
        table_alias_charset used here because it depends of
        lower_case_table_names variable
      */
      if (tables_used->table->s->tmp_table != NO_TMP_TABLE ||
          (*tables_type & HA_CACHE_TBL_NOCACHE) ||
          (tables_used->db_length == 5 &&
           my_strnncoll(table_alias_charset,
                        reinterpret_cast<const uchar*>(tables_used->table->s->table_cache_key.str),
                        6, reinterpret_cast<const uchar*>("mysql"), 6) == 0))
      {
        DBUG_PRINT("qcache",
                   ("select not cacheable: temporary, system or "
                    "other non-cacheable table(s)"));
        DBUG_RETURN(0);
      }
      /*
        XXX FIXME: Some generic mechanism is required here instead of this
        MYISAMMRG-specific implementation.
      */
      if (tables_used->table->s->db_type()->db_type == DB_TYPE_MRG_MYISAM)
      {
        ha_myisammrg *handler= static_cast<ha_myisammrg *>(tables_used->table->file);
        MYRG_INFO *file = handler->myrg_info();
        table_count+= static_cast<ulong>(file->end_table - file->open_tables);
      }
    }
  }
  DBUG_RETURN(table_count);
}


/**
  If query is cacheable return number tables in query
  (query without tables are not cached)
*/

TABLE_COUNTER_TYPE
Query_cache::is_cacheable(THD *thd, LEX *lex,
                          TABLE_LIST *tables_used, uint8 *tables_type) const
{
  TABLE_COUNTER_TYPE table_count;
  DBUG_ENTER("Query_cache::is_cacheable");

  if (lex->sql_command == SQLCOM_SELECT &&
      lex->safe_to_cache_query &&
      !lex->describe &&
      (thd->variables.query_cache_type == 1 ||
       (thd->variables.query_cache_type == 2 &&
        (lex->select_lex->active_options() & OPTION_TO_QUERY_CACHE))))
  {
    DBUG_PRINT("qcache", ("options: %llx  %llx  type: %lu",
                          OPTION_TO_QUERY_CACHE,
                          lex->select_lex->active_options(),
                          thd->variables.query_cache_type));

    if (!(table_count= process_and_count_tables(thd, tables_used,
                                                tables_type)))
      DBUG_RETURN(0);

    if (thd->in_multi_stmt_transaction_mode() &&
	((*tables_type)&HA_CACHE_TBL_TRANSACT))
    {
      DBUG_PRINT("qcache", ("not in autocommin mode"));
      DBUG_RETURN(0);
    }
    DBUG_PRINT("qcache", ("select is using %zu tables", table_count));
    DBUG_RETURN(table_count);
  }

  DBUG_PRINT("qcache",
	     ("not interesting query: %d or not cacheable, options %llx %llx  type: %lu",
	      lex->sql_command,
	      OPTION_TO_QUERY_CACHE,
	      lex->select_lex->active_options(),
	      thd->variables.query_cache_type));
  DBUG_RETURN(0);
}


/**
  Check handler allowance to cache query with these tables

  @param thd           thread handler
  @param tables_used   tables list used in query

  @retval false - caching allowed
  @retval true  - caching disallowed
*/

bool ask_handler_allowance(THD *thd, TABLE_LIST *tables_used)
{
  DBUG_ENTER("Query_cache::ask_handler_allowance");

  for (; tables_used; tables_used= tables_used->next_global)
  {
    TABLE *table;
    handler *handler;
    if (!(table= tables_used->table))
      continue;
    handler= table->file;
    // Allow caching of queries with materialized derived tables or views
    if (tables_used->uses_materialization())
    {
      /*
        Currently all result tables are MyISAM/Innodb or HEAP. MyISAM/Innodb
        allows caching unless table is under in a concurrent insert
        (which never could happen to a derived table). HEAP always allows caching.
      */
      DBUG_ASSERT(table->s->db_type() == heap_hton ||
                  table->s->db_type() == myisam_hton ||
                  table->s->db_type() == innodb_hton);
      DBUG_RETURN(false);
    }

    /*
      @todo: I think this code can be skipped, anyway it is dead now!
      We're skipping a special case here (MERGE VIEW on top of a TEMPTABLE
      view). This is MyISAMly safe because we know it's not a user-created
      TEMPTABLE as those are guarded against in
      Query_cache::process_and_count_tables(), and schema-tables clear
      safe_to_cache_query. This implies that nobody else will change our
      TEMPTABLE while we're using it, so calling register_query_cache_table()
      in MyISAM to check on it is pointless. Finally, we should see the
      TEMPTABLE view again in a subsequent iteration, anyway.
    */
    if (tables_used->is_view() && tables_used->is_merged() &&
        table->s->get_table_ref_type() == TABLE_REF_TMP_TABLE)
    {
      DBUG_ASSERT(false);
      continue;
    }
    if (!handler->register_query_cache_table(thd,
                                             table->s->normalized_path.str,
                                             table->s->normalized_path.length,
                                             &tables_used->callback_func,
                                             &tables_used->engine_data))
    {
      DBUG_PRINT("qcache", ("Handler does not allow caching for %s.%s",
                            tables_used->db, tables_used->alias));
      thd->lex->safe_to_cache_query= false;         // Don't try to cache this
      DBUG_RETURN(true);
    }
  }
  DBUG_RETURN(false);
}


/*****************************************************************************
  Packing
*****************************************************************************/


/**
  Rearrange all memory blocks so that free memory joins at the
  'bottom' of the allocated memory block containing all cache data.
  @see Query_cache::pack(ulong join_limit, uint iteration_limit)
*/

void Query_cache::pack_cache()
{
  DBUG_ENTER("Query_cache::pack_cache");

  uchar *border= NULL;
  Query_cache_block *before= NULL;
  ulong gap = 0;
  bool ok= true;
  Query_cache_block *block = first_block;

  if (first_block)
  {
    do
    {
      Query_cache_block *next=block->pnext;
      ok = move_by_type(&border, &before, &gap, block);
      block = next;
    } while (ok && block != first_block);

    if (border != NULL)
    {
      Query_cache_block *new_block= reinterpret_cast<Query_cache_block *>(border);
      new_block->init(gap);
      total_blocks++;
      new_block->pnext = before->pnext;
      before->pnext = new_block;
      new_block->pprev = before;
      new_block->pnext->pprev = new_block;
      insert_into_free_memory_list(new_block);
    }
  }

  DBUG_VOID_RETURN;
}


bool Query_cache::move_by_type(uchar **border,
                               Query_cache_block **before, ulong *gap,
                               Query_cache_block *block)
{
  DBUG_ENTER("Query_cache::move_by_type");

  bool ok= true;
  switch (block->type) {
  case Query_cache_block::FREE:
  {
    DBUG_PRINT("qcache", ("block %p FREE", block));
    if (*border == NULL)
    {
      *border= reinterpret_cast<uchar *>(block);
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
    HASH_SEARCH_STATE record_idx;
    DBUG_PRINT("qcache", ("block %p TABLE", block));
    if (*border == NULL)
      break;
    ulong len = block->length, used = block->used;
    Query_cache_block_table *list_root = block->table(0);
    Query_cache_block_table *tprev = list_root->prev,
			    *tnext = list_root->next;
    Query_cache_block *prev = block->prev,
      *next = block->next,
      *pprev = block->pprev,
      *pnext = block->pnext,
      *new_block= reinterpret_cast<Query_cache_block *>(*border);
    size_t tablename_offset= static_cast<size_t>(block->table()->table - block->table()->db());
    uchar *data= block->data();
    const uchar *key;
    size_t key_length;
    key= query_cache_table_get_key(reinterpret_cast<uchar*>(block), &key_length);
    my_hash_first(&tables, key, key_length, &record_idx);

    block->destroy();
    new_block->init(len);
    new_block->type=Query_cache_block::TABLE;
    new_block->used=used;
    new_block->n_tables=1;
    memmove(new_block->data(), data, len-new_block->headers_len());
    relink(block, new_block, next, prev, pnext, pprev);
    if (tables_blocks == block)
      tables_blocks = new_block;

    Query_cache_block_table *nlist_root = new_block->table(0);
    nlist_root->n = 0;
    nlist_root->next = tnext;
    tnext->prev = nlist_root;
    nlist_root->prev = tprev;
    tprev->next = nlist_root;

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
    new_block->table()->table= new_block->table()->db() + tablename_offset;
    /* Fix hash to point at moved block */
    my_hash_replace(&tables, &record_idx, reinterpret_cast<uchar*>(new_block));

    DBUG_PRINT("qcache", ("moved %lu bytes to %p, new gap at %p",
                          len, new_block, *border));
    break;
  }
  case Query_cache_block::QUERY:
  {
    HASH_SEARCH_STATE record_idx;
    DBUG_PRINT("qcache", ("block %p QUERY", block));
    if (*border == NULL)
      break;
    mysql_rwlock_wrlock(&block->query()->lock);
    ulong len = block->length, used = block->used;
    TABLE_COUNTER_TYPE n_tables = block->n_tables;
    Query_cache_block *prev= block->prev,
      *next= block->next,
      *pprev= block->pprev,
      *pnext= block->pnext,
      *new_block= reinterpret_cast<Query_cache_block*>(*border);
    uchar *data= block->data();
    Query_cache_block *first_result_block= block->query()->result;
    const uchar *key;
    size_t key_length;
    key=query_cache_query_get_key(reinterpret_cast<uchar*>(block), &key_length);
    my_hash_first(&queries, key, key_length, &record_idx);
    // Move table of used tables
    memmove(reinterpret_cast<char*>(new_block->table(0)),
            reinterpret_cast<char*>(block->table(0)),
            ALIGN_SIZE(n_tables*sizeof(Query_cache_block_table)));
    block->query()->unlock_n_destroy();
    block->destroy();
    new_block->init(len);
    new_block->type=Query_cache_block::QUERY;
    new_block->used=used;
    new_block->n_tables=n_tables;
    memmove(new_block->data(), data, len - new_block->headers_len());
    relink(block, new_block, next, prev, pnext, pprev);
    if (queries_blocks == block)
      queries_blocks = new_block;
    Query_cache_block_table *beg_of_table_table= block->table(0),
      *end_of_table_table= block->table(n_tables);
    uchar *beg_of_new_table_table= reinterpret_cast<uchar*>(new_block->table(0));

    for (TABLE_COUNTER_TYPE j=0; j < n_tables; j++)
    {
      Query_cache_block_table *block_table = new_block->table(j);

      // use aligment from begining of table if 'next' is in same block
      if ((beg_of_table_table <= block_table->next) &&
	  (block_table->next < end_of_table_table))
	(reinterpret_cast<Query_cache_block_table *>(beg_of_new_table_table +
				     ((reinterpret_cast<uchar*>(block_table->next)) -
				      (reinterpret_cast<uchar*>(beg_of_table_table)))))->prev=
	 block_table;
      else
	block_table->next->prev= block_table;

      // use aligment from begining of table if 'prev' is in same block
      if ((beg_of_table_table <= block_table->prev) &&
	  (block_table->prev < end_of_table_table))
	(reinterpret_cast<Query_cache_block_table *>(beg_of_new_table_table +
				     ((reinterpret_cast<uchar*>(block_table->prev)) -
				      (reinterpret_cast<uchar*>(beg_of_table_table)))))->next=
	  block_table;
      else
	block_table->prev->next = block_table;
    }
    DBUG_PRINT("qcache", ("after circle tt"));
    *border += len;
    *before = new_block;
    new_block->query()->result= first_result_block;
    if (first_result_block != NULL)
    {
      Query_cache_block *result_block = first_result_block;
      do
      {
	result_block->result()->parent= new_block;
	result_block = result_block->next;
      } while ( result_block != first_result_block );
    }
    Query_cache_query *new_query= new_block->query();
    mysql_rwlock_init(key_rwlock_query_cache_query_lock, &new_query->lock);

    /*
      If someone is writing to this block, inform the writer that the block
      has been moved.
    */
    THD *writer= new_block->query()->writer;
    if (writer != NULL)
    {
      writer->first_query_cache_block= new_block;
    }
    /* Fix hash to point at moved block */
    my_hash_replace(&queries, &record_idx, reinterpret_cast<uchar*>(new_block));
    DBUG_PRINT("qcache", ("moved %lu bytes to %p, new gap at %p",
                          len, new_block, *border));
    break;
  }
  case Query_cache_block::RES_INCOMPLETE:
  case Query_cache_block::RES_BEG:
  case Query_cache_block::RES_CONT:
  case Query_cache_block::RESULT:
  {
    DBUG_PRINT("qcache", ("block %p RES* (%d)", block, block->type));
    if (*border == NULL)
      break;
    Query_cache_block *query_block= block->result()->parent;
    mysql_rwlock_wrlock(&query_block->query()->lock);
    Query_cache_block *next= block->next, *prev= block->prev;
    Query_cache_block::block_type type= block->type;
    ulong len = block->length, used = block->used;
    Query_cache_block *pprev= block->pprev,
      *pnext= block->pnext,
      *new_block= reinterpret_cast<Query_cache_block*>(*border);
    uchar *data= block->data();
    block->destroy();
    new_block->init(len);
    new_block->type=type;
    new_block->used=used;
    memmove(new_block->data(), data, len - new_block->headers_len());
    relink(block, new_block, next, prev, pnext, pprev);
    new_block->result()->parent= query_block;
    Query_cache_query *query = query_block->query();
    if (query->result == block)
      query->result= new_block;
    *border += len;
    *before = new_block;
    /* If result writing complete && we have free space in block */
    ulong free_space= new_block->length - new_block->used;
    free_space-= free_space % ALIGN_SIZE(1);
    if (query->result->type == Query_cache_block::RESULT &&
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
    mysql_rwlock_unlock(&query_block->query()->lock);
    DBUG_PRINT("qcache", ("moved %lu bytes to %p, new gap at %p",
			len, new_block, *border));
    break;
  }
  default:
    DBUG_PRINT("error", ("unexpected block type %d, block %p",
			 block->type, block));
    ok= false;
  }
  DBUG_RETURN(ok);
}


void relink(Query_cache_block *oblock,
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


bool Query_cache::join_results(ulong join_limit)
{
  bool has_moving= false;
  DBUG_ENTER("Query_cache::join_results");

  if (queries_blocks != NULL)
  {
    DBUG_ASSERT(query_cache_size > 0);
    Query_cache_block *block = queries_blocks;
    do
    {
      Query_cache_query *header = block->query();
      if (header->result != NULL &&
	  header->result->type == Query_cache_block::RESULT &&
	  header->length > join_limit)
      {
	Query_cache_block *new_result_block =
	  get_free_block(ALIGN_SIZE(header->length) +
			 ALIGN_SIZE(sizeof(Query_cache_block)) +
			 ALIGN_SIZE(sizeof(Query_cache_result)), 1, 0);
	if (new_result_block != NULL)
	{
	  has_moving= true;
	  Query_cache_block *first_result = header->result;
	  ulong new_len = (header->length +
			   ALIGN_SIZE(sizeof(Query_cache_block)) +
			   ALIGN_SIZE(sizeof(Query_cache_result)));
	  if (new_result_block->length >
	      ALIGN_SIZE(new_len) + min_allocation_unit)
	    split_block(new_result_block, ALIGN_SIZE(new_len));
          mysql_rwlock_wrlock(&block->query()->lock);
	  header->result= new_result_block;
	  new_result_block->type = Query_cache_block::RESULT;
	  new_result_block->n_tables = 0;
	  new_result_block->used = new_len;

	  new_result_block->next = new_result_block->prev = new_result_block;
	  DBUG_PRINT("qcache", ("new block %lu/%lu (%lu)",
                                new_result_block->length,
                                new_result_block->used,
                                header->length));

	  Query_cache_result *new_result = new_result_block->result();
	  new_result->parent= block;
	  uchar *write_to = new_result->data();
	  Query_cache_block *result_block = first_result;
	  do
	  {
	    ulong len = (result_block->used - result_block->headers_len() -
			 ALIGN_SIZE(sizeof(Query_cache_result)));
	    DBUG_PRINT("loop", ("add block %lu/%lu (%lu)",
				result_block->length,
				result_block->used,
				len));
	    memcpy(write_to, result_block->result()->data(), len);
	    write_to += len;
	    Query_cache_block *old_result_block = result_block;
	    result_block = result_block->next;
	    free_memory_block(old_result_block);
	  } while (result_block != first_result);
          mysql_rwlock_unlock(&block->query()->lock);
	}
      }
      block = block->next;
    } while ( block != queries_blocks );
  }
  DBUG_RETURN(has_moving);
}


size_t filename_2_table_key(char *key, const char *path, size_t *db_length)
{
  char tablename[FN_REFLEN+2], *filename, *dbname;
  DBUG_ENTER("filename_2_table_key");

  /* Safety if filename didn't have a directory name */
  tablename[0]= FN_LIBCHAR;
  tablename[1]= FN_LIBCHAR;
  /* Convert filename to this OS's format in tablename */
  fn_format(tablename + 2, path, "", "", MY_REPLACE_EXT);
  filename=  tablename + dirname_length(tablename + 2) + 2;
  /* Find start of databasename */
  for (dbname= filename - 2 ; dbname[-1] != FN_LIBCHAR ; dbname--)
    ;
  *db_length= static_cast<size_t>((filename - dbname) - 1);

  DBUG_RETURN(static_cast<size_t>(strmake(strmake(key, dbname,
                                                  min<size_t>(*db_length,
                                                              NAME_LEN)) + 1,
                                  filename, NAME_LEN) - key) + 1);
}
