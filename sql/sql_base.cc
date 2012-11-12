/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

/* Basic functions needed by many modules */

#include "sql_base.h"                           // setup_table_map
#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_priv.h"
#include "unireg.h"
#include "debug_sync.h"
#include "lock.h"        // mysql_lock_remove,
                         // mysql_unlock_tables,
                         // mysql_lock_have_duplicate
#include "sql_show.h"    // append_identifier
#include "strfunc.h"     // find_type
#include "sql_view.h"    // mysql_make_view, VIEW_ANY_ACL
#include "sql_parse.h"   // check_table_access
#include "sql_acl.h"     // *_ACL, check_grant_all_columns,
                         // check_column_grant_in_table_ref,
                         // get_column_grant
#include "sql_partition.h"               // ALTER_PARTITION_PARAM_TYPE
#include "sql_derived.h" // mysql_derived_prepare,
                         // mysql_handle_derived,
                         // mysql_derived_filling
#include "sql_handler.h" // mysql_ha_flush
#include "sql_partition.h"                      // ALTER_PARTITION_PARAM_TYPE
#include "log_event.h"                          // Query_log_event
#include "sql_select.h"
#include "sp_head.h"
#include "sp.h"
#include "sp_cache.h"
#include "sql_trigger.h"
#include "transaction.h"
#include "sql_prepare.h"
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include "rpl_filter.h"
#include "sql_table.h"                          // build_table_filename
#include "datadict.h"   // dd_frm_type()
#include "sql_hset.h"   // Hash_set
#include "sql_tmp_table.h" // free_tmp_table
#ifdef  __WIN__
#include <io.h>
#endif
#include "table_cache.h" // Table_cache_manager, Table_cache


bool
No_such_table_error_handler::handle_condition(THD *,
                                              uint sql_errno,
                                              const char*,
                                              Sql_condition::enum_severity_level,
                                              const char*,
                                              Sql_condition ** cond_hdl)
{
  *cond_hdl= NULL;
  if (sql_errno == ER_NO_SUCH_TABLE)
  {
    m_handled_errors++;
    return TRUE;
  }

  m_unhandled_errors++;
  return FALSE;
}


bool No_such_table_error_handler::safely_trapped_errors()
{
  /*
    If m_unhandled_errors != 0, something else, unanticipated, happened,
    so the error is not trapped but returned to the caller.
    Multiple ER_NO_SUCH_TABLE can be raised in case of views.
  */
  return ((m_handled_errors > 0) && (m_unhandled_errors == 0));
}


/**
  This internal handler is used to trap ER_NO_SUCH_TABLE and
  ER_WRONG_MRG_TABLE errors during CHECK/REPAIR TABLE for MERGE
  tables.
*/

class Repair_mrg_table_error_handler : public Internal_error_handler
{
public:
  Repair_mrg_table_error_handler()
    : m_handled_errors(false), m_unhandled_errors(false)
  {}

  bool handle_condition(THD *thd,
                        uint sql_errno,
                        const char* sqlstate,
                        Sql_condition::enum_severity_level level,
                        const char* msg,
                        Sql_condition ** cond_hdl);

  /**
    Returns TRUE if there were ER_NO_SUCH_/WRONG_MRG_TABLE and there
    were no unhandled errors. FALSE otherwise.
  */
  bool safely_trapped_errors()
  {
    /*
      Check for m_handled_errors is here for extra safety.
      It can be useful in situation when call to open_table()
      fails because some error which was suppressed by another
      error handler (e.g. in case of MDL deadlock which we
      decided to solve by back-off and retry).
    */
    return (m_handled_errors && (! m_unhandled_errors));
  }

private:
  bool m_handled_errors;
  bool m_unhandled_errors;
};


bool
Repair_mrg_table_error_handler::handle_condition(THD *,
                                                 uint sql_errno,
                                                 const char*,
                                                 Sql_condition::enum_severity_level level,
                                                 const char*,
                                                 Sql_condition ** cond_hdl)
{
  *cond_hdl= NULL;
  if (sql_errno == ER_NO_SUCH_TABLE || sql_errno == ER_WRONG_MRG_TABLE)
  {
    m_handled_errors= true;
    return TRUE;
  }

  m_unhandled_errors= true;
  return FALSE;
}


/**
  @defgroup Data_Dictionary Data Dictionary
  @{
*/

/**
  LOCK_open protects the following variables/objects:

  1) The table_def_cache
     This is the hash table mapping table name to a table
     share object. The hash table can only be manipulated
     while holding LOCK_open.
  2) last_table_id
     Generation of a new unique table_map_id for a table
     share is done through incrementing last_table_id, a
     global variable used for this purpose.
  3) LOCK_open protects the initialisation of the table share
     object and all its members and also protects reading the
     .frm file from where the table share is initialised.
  4) In particular the share->ref_count is updated each time
     a new table object is created that refers to a table share.
     This update is protected by LOCK_open.
  5) oldest_unused_share, end_of_unused_share and share->next
     and share->prev are variables to handle the lists of table
     share objects, these can only be read and manipulated while
     holding the LOCK_open mutex.
  6) table_def_shutdown_in_progress can be updated only while
     holding LOCK_open and ALL table cache mutexes.
  7) refresh_version
     This variable can only be updated while holding LOCK_open AND
     all table cache mutexes.
  8) share->version
     This variable is initialised while holding LOCK_open. It can only
     be updated while holding LOCK_open AND all table cache mutexes.
     So if a table share is found through a reference its version won't
     change if any of those mutexes are held.
  9) share->m_flush_tickets
*/
mysql_mutex_t LOCK_open;

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_open;
static PSI_mutex_info all_tdc_mutexes[]= {
  { &key_LOCK_open, "LOCK_open", PSI_FLAG_GLOBAL }
};

/**
  Initialize performance schema instrumentation points
  used by the table cache.
*/

static void init_tdc_psi_keys(void)
{
  const char *category= "sql";
  int count;

  count= array_elements(all_tdc_mutexes);
  mysql_mutex_register(category, all_tdc_mutexes, count);
}
#endif /* HAVE_PSI_INTERFACE */

static void modify_slave_open_temp_tables(THD *thd, int inc)
{
  if (thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER)
  {
    my_atomic_rwlock_wrlock(&slave_open_temp_tables_lock);
    my_atomic_add32(&slave_open_temp_tables, inc);
    my_atomic_rwlock_wrunlock(&slave_open_temp_tables_lock);
  }
  else
  {
    slave_open_temp_tables += inc;
  }
}


HASH table_def_cache;
static TABLE_SHARE *oldest_unused_share, end_of_unused_share;
static bool table_def_inited= false;
static bool table_def_shutdown_in_progress= false;

static bool check_and_update_table_version(THD *thd, TABLE_LIST *tables,
                                           TABLE_SHARE *table_share);
static bool open_table_entry_fini(THD *thd, TABLE_SHARE *share, TABLE *entry);
static bool auto_repair_table(THD *thd, TABLE_LIST *table_list);
static bool
has_write_table_with_auto_increment(TABLE_LIST *tables);
static bool
has_write_table_with_auto_increment_and_select(TABLE_LIST *tables);
static bool has_write_table_auto_increment_not_first_in_pk(TABLE_LIST *tables);


/**
  Create a table cache/table definition cache key

  @param thd        Thread context
  @param key        Buffer for the key to be created (must be of
                    size MAX_DBKEY_LENGTH).
  @param db_name    Database name.
  @param table_name Table name.
  @param tmp_table  Set if table is a tmp table.

  @note
    The table cache_key is created from:
    db_name + \0
    table_name + \0

    if the table is a tmp table, we add the following to make each tmp table
    unique on the slave:

    4 bytes for master thread id
    4 bytes pseudo thread id

  @return Length of key.
*/

static uint create_table_def_key(THD *thd, char *key,
                                 const char *db_name, const char *table_name,
                                 bool tmp_table)
{
  uint key_length= (uint) (strmov(strmov(key, db_name) + 1, table_name) -
                           key) + 1;
  if (tmp_table)
  {
    int4store(key + key_length, thd->server_id);
    int4store(key + key_length + 4, thd->variables.pseudo_thread_id);
    key_length+= TMP_TABLE_KEY_EXTRA;
  }
  return key_length;
}


/**
  Get table cache key for a table list element.

  @param table_list[in]  Table list element.
  @param key[out]        On return points to table cache key for the table.

  @note Unlike create_table_def_key() call this function doesn't construct
        key in a buffer provider by caller. Instead it relies on the fact
        that table list element for which key is requested has properly
        initialized MDL_request object and the fact that table definition
        cache key is suffix of key used in MDL subsystem. So to get table
        definition key it simply needs to return pointer to appropriate
        part of MDL_key object nested in this table list element.
        Indeed, this means that lifetime of key produced by this call is
        limited by the lifetime of table list element which it got as
        parameter.

  @return Length of key.
*/

uint get_table_def_key(const TABLE_LIST *table_list, const char **key)
{
  /*
    This call relies on the fact that TABLE_LIST::mdl_request::key object
    is properly initialized, so table definition cache can be produced
    from key used by MDL subsystem.
  */
  DBUG_ASSERT(!strcmp(table_list->db, table_list->mdl_request.key.db_name()) &&
              !strcmp(table_list->table_name, table_list->mdl_request.key.name()));

  *key= (const char*)table_list->mdl_request.key.ptr() + 1;
  return table_list->mdl_request.key.length() - 1;
}



/*****************************************************************************
  Functions to handle table definition cach (TABLE_SHARE)
*****************************************************************************/

extern "C" uchar *table_def_key(const uchar *record, size_t *length,
                               my_bool not_used __attribute__((unused)))
{
  TABLE_SHARE *entry=(TABLE_SHARE*) record;
  *length= entry->table_cache_key.length;
  return (uchar*) entry->table_cache_key.str;
}


static void table_def_free_entry(TABLE_SHARE *share)
{
  DBUG_ENTER("table_def_free_entry");
  mysql_mutex_assert_owner(&LOCK_open);
  if (share->prev)
  {
    /* remove from old_unused_share list */
    *share->prev= share->next;
    share->next->prev= share->prev;
  }
  free_table_share(share);
  DBUG_VOID_RETURN;
}


bool table_def_init(void)
{
#ifdef HAVE_PSI_INTERFACE
  init_tdc_psi_keys();
#endif
  mysql_mutex_init(key_LOCK_open, &LOCK_open, MY_MUTEX_INIT_FAST);
  oldest_unused_share= &end_of_unused_share;
  end_of_unused_share.prev= &oldest_unused_share;

  if (table_cache_manager.init())
  {
    mysql_mutex_destroy(&LOCK_open);
    return true;
  }

  /*
    It is safe to destroy zero-initialized HASH even if its
    initialization has failed.
  */
  table_def_inited= true;

  return my_hash_init(&table_def_cache, &my_charset_bin, table_def_size,
                      0, 0, table_def_key,
                      (my_hash_free_key) table_def_free_entry, 0) != 0;
}


/**
  Notify table definition cache that process of shutting down server
  has started so it has to keep number of TABLE and TABLE_SHARE objects
  minimal in order to reduce number of references to pluggable engines.
*/

void table_def_start_shutdown(void)
{
  if (table_def_inited)
  {
    table_cache_manager.lock_all_and_tdc();
    /*
      Ensure that TABLE and TABLE_SHARE objects which are created for
      tables that are open during process of plugins' shutdown are
      immediately released. This keeps number of references to engine
      plugins minimal and allows shutdown to proceed smoothly.
    */
    table_def_shutdown_in_progress= true;
    table_cache_manager.unlock_all_and_tdc();
    /* Free all cached but unused TABLEs and TABLE_SHAREs. */
    close_cached_tables(NULL, NULL, FALSE, LONG_TIMEOUT);
  }
}


void table_def_free(void)
{
  DBUG_ENTER("table_def_free");
  if (table_def_inited)
  {
    table_def_inited= false;
    /* Free table definitions. */
    my_hash_free(&table_def_cache);
    table_cache_manager.destroy();
    mysql_mutex_destroy(&LOCK_open);
  }
  DBUG_VOID_RETURN;
}


uint cached_table_definitions(void)
{
  return table_def_cache.records;
}


/*
  Get TABLE_SHARE for a table.

  get_table_share()
  thd			Thread handle
  table_list		Table that should be opened
  key			Table cache key
  key_length		Length of key
  db_flags		Flags to open_table_def():
			OPEN_VIEW
  error			out: Error code from open_table_def()

  IMPLEMENTATION
    Get a table definition from the table definition cache.
    If it doesn't exist, create a new from the table definition file.

  NOTES
    We must have wrlock on LOCK_open when we come here
    (To be changed later)

  RETURN
   0  Error
   #  Share for table
*/

TABLE_SHARE *get_table_share(THD *thd, TABLE_LIST *table_list,
                             const char *key, uint key_length,
                             uint db_flags, int *error,
                             my_hash_value_type hash_value)
{
  TABLE_SHARE *share;
  DBUG_ENTER("get_table_share");

  *error= 0;

  /*
    To be able perform any operation on table we should own
    some kind of metadata lock on it.
  */
  DBUG_ASSERT(thd->mdl_context.is_lock_owner(MDL_key::TABLE,
                                             table_list->db,
                                             table_list->table_name,
                                             MDL_SHARED));

  /* Read table definition from cache */
  if ((share= (TABLE_SHARE*) my_hash_search_using_hash_value(&table_def_cache,
                                                             hash_value, (uchar*) key, key_length)))
    goto found;

  if (!(share= alloc_table_share(table_list, key, key_length)))
  {
    DBUG_RETURN(0);
  }

  /*
    We assign a new table id under the protection of LOCK_open.
    We do this instead of creating a new mutex
    and using it for the sole purpose of serializing accesses to a
    static variable, we assign the table id here. We assign it to the
    share before inserting it into the table_def_cache to be really
    sure that it cannot be read from the cache without having a table
    id assigned.

    CAVEAT. This means that the table cannot be used for
    binlogging/replication purposes, unless get_table_share() has been
    called directly or indirectly.
   */
  assign_new_table_id(share);

  if (my_hash_insert(&table_def_cache, (uchar*) share))
  {
    free_table_share(share);
    DBUG_RETURN(0);				// return error
  }
  if (open_table_def(thd, share, db_flags))
  {
    *error= share->error;
    (void) my_hash_delete(&table_def_cache, (uchar*) share);
    DBUG_RETURN(0);
  }
  share->ref_count++;				// Mark in use

#ifdef HAVE_PSI_TABLE_INTERFACE
  share->m_psi= PSI_TABLE_CALL(get_table_share)(false, share);
#else
  share->m_psi= NULL;
#endif

  DBUG_PRINT("exit", ("share: 0x%lx  ref_count: %u",
                      (ulong) share, share->ref_count));
  DBUG_RETURN(share);

found:
  /*
     We found an existing table definition. Return it if we didn't get
     an error when reading the table definition from file.
  */
  if (share->error)
  {
    /* Table definition contained an error */
    open_table_error(share, share->error, share->open_errno, share->errarg);
    DBUG_RETURN(0);
  }
  if (share->is_view && !(db_flags & OPEN_VIEW))
  {
    open_table_error(share, 1, ENOENT, 0);
    DBUG_RETURN(0);
  }

  ++share->ref_count;

  if (share->ref_count == 1 && share->prev)
  {
    /*
      Share was not used before and it was in the old_unused_share list
      Unlink share from this list
    */
    DBUG_PRINT("info", ("Unlinking from not used list"));
    *share->prev= share->next;
    share->next->prev= share->prev;
    share->next= 0;
    share->prev= 0;
  }

   /* Free cache if too big */
  while (table_def_cache.records > table_def_size &&
         oldest_unused_share->next)
    my_hash_delete(&table_def_cache, (uchar*) oldest_unused_share);

  DBUG_PRINT("exit", ("share: 0x%lx  ref_count: %u",
                      (ulong) share, share->ref_count));
  DBUG_RETURN(share);
}


/**
  Get a table share. If it didn't exist, try creating it from engine

  For arguments and return values, see get_table_share()
*/

static TABLE_SHARE *
get_table_share_with_discover(THD *thd, TABLE_LIST *table_list,
                              const char *key, uint key_length,
                              uint db_flags, int *error,
                              my_hash_value_type hash_value)

{
  TABLE_SHARE *share;
  bool exists;
  DBUG_ENTER("get_table_share_with_create");

  share= get_table_share(thd, table_list, key, key_length, db_flags, error,
                         hash_value);
  /*
    If share is not NULL, we found an existing share.

    If share is NULL, and there is no error, we're inside
    pre-locking, which silences 'ER_NO_SUCH_TABLE' errors
    with the intention to silently drop non-existing tables 
    from the pre-locking list. In this case we still need to try
    auto-discover before returning a NULL share.

    Or, we're inside SHOW CREATE VIEW, which
    also installs a silencer for ER_NO_SUCH_TABLE error.

    If share is NULL and the error is ER_NO_SUCH_TABLE, this is
    the same as above, only that the error was not silenced by
    pre-locking or SHOW CREATE VIEW.

    In both these cases it won't harm to try to discover the
    table.

    Finally, if share is still NULL, it's a real error and we need
    to abort.

    @todo Rework alternative ways to deal with ER_NO_SUCH TABLE.
  */
  if (share || (thd->is_error() &&
      thd->get_stmt_da()->mysql_errno() != ER_NO_SUCH_TABLE))
  {
    DBUG_RETURN(share);
  }

  *error= 0;

  /* Table didn't exist. Check if some engine can provide it */
  if (ha_check_if_table_exists(thd, table_list->db, table_list->table_name,
                               &exists))
  {
    thd->clear_error();
    /* Conventionally, the storage engine API does not report errors. */
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
  }
  else if (! exists)
  {
    /*
      No such table in any engine.
      Hide "Table doesn't exist" errors if the table belongs to a view.
      The check for thd->is_error() is necessary to not push an
      unwanted error in case the error was already silenced.
      @todo Rework the alternative ways to deal with ER_NO_SUCH TABLE.
    */
    if (thd->is_error())
    {
      if (table_list->parent_l)
      {
        thd->clear_error();
        my_error(ER_WRONG_MRG_TABLE, MYF(0));
      }
      else if (table_list->belong_to_view)
      {
        TABLE_LIST *view= table_list->belong_to_view;
        thd->clear_error();
        my_error(ER_VIEW_INVALID, MYF(0),
                 view->view_db.str, view->view_name.str);
      }
    }
  }
  else
  {
    thd->clear_error();
    *error= 7; /* Run auto-discover. */
  }
  DBUG_RETURN(NULL);
}


/**
  Mark that we are not using table share anymore.

  @param  share   Table share

  If the share has no open tables and (we have done a refresh or
  if we have already too many open table shares) then delete the
  definition.
*/

void release_table_share(TABLE_SHARE *share)
{
  DBUG_ENTER("release_table_share");
  DBUG_PRINT("enter",
             ("share: 0x%lx  table: %s.%s  ref_count: %u  version: %lu",
              (ulong) share, share->db.str, share->table_name.str,
              share->ref_count, share->version));

  mysql_mutex_assert_owner(&LOCK_open);

  DBUG_ASSERT(share->ref_count);
  if (!--share->ref_count)
  {
    if (share->has_old_version() || table_def_shutdown_in_progress)
      my_hash_delete(&table_def_cache, (uchar*) share);
    else
    {
      /* Link share last in used_table_share list */
      DBUG_PRINT("info",("moving share to unused list"));

      DBUG_ASSERT(share->next == 0);
      share->prev= end_of_unused_share.prev;
      *end_of_unused_share.prev= share;
      end_of_unused_share.prev= &share->next;
      share->next= &end_of_unused_share;

      if (table_def_cache.records > table_def_size)
      {
        /* Delete the least used share to preserve LRU order. */
        my_hash_delete(&table_def_cache, (uchar*) oldest_unused_share);
      }
    }
  }

  DBUG_VOID_RETURN;
}


/*
  Check if table definition exits in cache

  SYNOPSIS
    get_cached_table_share()
    db			Database name
    table_name		Table name

  RETURN
    0  Not cached
    #  TABLE_SHARE for table
*/

TABLE_SHARE *get_cached_table_share(const char *db, const char *table_name)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  mysql_mutex_assert_owner(&LOCK_open);

  key_length= create_table_def_key((THD*) 0, key, db, table_name, 0);
  return (TABLE_SHARE*) my_hash_search(&table_def_cache,
                                       (uchar*) key, key_length);
}  


/*
  Create a list for all open tables matching SQL expression

  SYNOPSIS
    list_open_tables()
    thd			Thread THD
    wild		SQL like expression

  NOTES
    One gets only a list of tables for which one has any kind of privilege.
    db and table names are allocated in result struct, so one doesn't need
    a lock on LOCK_open when traversing the return list.

  RETURN VALUES
    NULL	Error (Probably OOM)
    #		Pointer to list of names of open tables.
*/

OPEN_TABLE_LIST *list_open_tables(THD *thd, const char *db, const char *wild)
{
  int result = 0;
  OPEN_TABLE_LIST **start_list, *open_list;
  TABLE_LIST table_list;
  DBUG_ENTER("list_open_tables");

  memset(&table_list, 0, sizeof(table_list));
  start_list= &open_list;
  open_list=0;

  table_cache_manager.lock_all_and_tdc();

  for (uint idx=0 ; result == 0 && idx < table_def_cache.records; idx++)
  {
    TABLE_SHARE *share= (TABLE_SHARE *)my_hash_element(&table_def_cache, idx);

    if (db && my_strcasecmp(system_charset_info, db, share->db.str))
      continue;
    if (wild && wild_compare(share->table_name.str, wild, 0))
      continue;

    /* Check if user has SELECT privilege for any column in the table */
    table_list.db=         share->db.str;
    table_list.table_name= share->table_name.str;
    table_list.grant.privilege=0;

    if (check_table_access(thd,SELECT_ACL,&table_list, TRUE, 1, TRUE))
      continue;

    if (!(*start_list = (OPEN_TABLE_LIST *)
	  sql_alloc(sizeof(**start_list)+share->table_cache_key.length)))
    {
      open_list=0;				// Out of memory
      break;
    }
    strmov((*start_list)->table=
	   strmov(((*start_list)->db= (char*) ((*start_list)+1)),
		  share->db.str)+1,
	   share->table_name.str);
    (*start_list)->in_use= 0;
    Table_cache_iterator it(share);
    while (it++)
      ++(*start_list)->in_use;
    (*start_list)->locked= 0;                   /* Obsolete. */
    start_list= &(*start_list)->next;
    *start_list=0;
  }
  table_cache_manager.unlock_all_and_tdc();
  DBUG_RETURN(open_list);
}

/*****************************************************************************
 *	 Functions to free open table cache
 ****************************************************************************/


void intern_close_table(TABLE *table)
{						// Free all structures
  DBUG_ENTER("intern_close_table");
  DBUG_PRINT("tcache", ("table: '%s'.'%s' 0x%lx",
                        table->s ? table->s->db.str : "?",
                        table->s ? table->s->table_name.str : "?",
                        (long) table));

  free_io_cache(table);
  delete table->triggers;
  if (table->file)                              // Not true if placeholder
    (void) closefrm(table, 1);			// close file
  my_free(table);
  DBUG_VOID_RETURN;
}


/* Free resources allocated by filesort() and read_record() */

void free_io_cache(TABLE *table)
{
  DBUG_ENTER("free_io_cache");
  if (table->sort.io_cache)
  {
    close_cached_file(table->sort.io_cache);
    my_free(table->sort.io_cache);
    table->sort.io_cache=0;
  }
  DBUG_VOID_RETURN;
}


/*
  Close all tables which aren't in use by any thread

  @param thd Thread context
  @param tables List of tables to remove from the cache
  @param wait_for_refresh Wait for a impending flush
  @param timeout Timeout for waiting for flush to be completed.

  @note THD can be NULL, but then wait_for_refresh must be FALSE
        and tables must be NULL.

  @note When called as part of FLUSH TABLES WITH READ LOCK this function
        ignores metadata locks held by other threads. In order to avoid
        situation when FLUSH TABLES WITH READ LOCK sneaks in at the moment
        when some write-locked table is being reopened (by FLUSH TABLES or
        ALTER TABLE) we have to rely on additional global shared metadata
        lock taken by thread trying to obtain global read lock.
*/

bool close_cached_tables(THD *thd, TABLE_LIST *tables,
                         bool wait_for_refresh, ulong timeout)
{
  bool result= FALSE;
  bool found= TRUE;
  struct timespec abstime;
  DBUG_ENTER("close_cached_tables");
  DBUG_ASSERT(thd || (!wait_for_refresh && !tables));

  table_cache_manager.lock_all_and_tdc();
  if (!tables)
  {
    /*
      Force close of all open tables.

      Note that code in TABLE_SHARE::wait_for_old_version() assumes that
      incrementing of refresh_version and removal of unused tables and
      shares from TDC happens atomically under protection of LOCK_open,
      or putting it another way that TDC does not contain old shares
      which don't have any tables used.
    */
    refresh_version++;
    DBUG_PRINT("tcache", ("incremented global refresh_version to: %lu",
                          refresh_version));

    /*
      Get rid of all unused TABLE and TABLE_SHARE instances. By doing
      this we automatically close all tables which were marked as "old".
    */
    table_cache_manager.free_all_unused_tables();
    /* Free table shares which were not freed implicitly by loop above. */
    while (oldest_unused_share->next)
      (void) my_hash_delete(&table_def_cache, (uchar*) oldest_unused_share);
  }
  else
  {
    bool found=0;
    for (TABLE_LIST *table= tables; table; table= table->next_local)
    {
      TABLE_SHARE *share= get_cached_table_share(table->db, table->table_name);

      if (share)
      {
        /* tdc_remove_table() also sets TABLE_SHARE::version to 0. */
        tdc_remove_table(thd, TDC_RT_REMOVE_UNUSED, table->db,
                         table->table_name, TRUE);
        found=1;
      }
    }
    if (!found)
      wait_for_refresh=0;			// Nothing to wait for
  }

  table_cache_manager.unlock_all_and_tdc();

  if (!wait_for_refresh)
    DBUG_RETURN(result);

  set_timespec(abstime, timeout);

  if (thd->locked_tables_mode)
  {
    /*
      If we are under LOCK TABLES, we need to reopen the tables without
      opening a door for any concurrent threads to sneak in and get
      lock on our tables. To achieve this we use exclusive metadata
      locks.
    */
    TABLE_LIST *tables_to_reopen= (tables ? tables :
                                  thd->locked_tables_list.locked_tables());

    /* Close open HANLER instances to avoid self-deadlock. */
    mysql_ha_flush_tables(thd, tables_to_reopen);

    for (TABLE_LIST *table_list= tables_to_reopen; table_list;
         table_list= table_list->next_global)
    {
      /* A check that the table was locked for write is done by the caller. */
      TABLE *table= find_table_for_mdl_upgrade(thd, table_list->db,
                                               table_list->table_name, TRUE);

      /* May return NULL if this table has already been closed via an alias. */
      if (! table)
        continue;

      if (wait_while_table_is_used(thd, table, HA_EXTRA_FORCE_REOPEN))
      {
        result= TRUE;
        goto err_with_reopen;
      }
      close_all_tables_for_name(thd, table->s, false, NULL);
    }
  }

  /* Wait until all threads have closed all the tables we are flushing. */
  DBUG_PRINT("info", ("Waiting for other threads to close their open tables"));

  while (found && ! thd->killed)
  {
    TABLE_SHARE *share;
    found= FALSE;
    /*
      To a self-deadlock or deadlocks with other FLUSH threads
      waiting on our open HANDLERs, we have to flush them.
    */
    mysql_ha_flush(thd);
    DEBUG_SYNC(thd, "after_flush_unlock");

    mysql_mutex_lock(&LOCK_open);

    if (!tables)
    {
      for (uint idx=0 ; idx < table_def_cache.records ; idx++)
      {
        share= (TABLE_SHARE*) my_hash_element(&table_def_cache, idx);
        if (share->has_old_version())
        {
          found= TRUE;
          break;
        }
      }
    }
    else
    {
      for (TABLE_LIST *table= tables; table; table= table->next_local)
      {
        share= get_cached_table_share(table->db, table->table_name);
        if (share && share->has_old_version())
        {
	  found= TRUE;
          break;
        }
      }
    }

    if (found)
    {
      /*
        The method below temporarily unlocks LOCK_open and frees
        share's memory.
      */
      if (share->wait_for_old_version(thd, &abstime,
                                    MDL_wait_for_subgraph::DEADLOCK_WEIGHT_DDL))
      {
        mysql_mutex_unlock(&LOCK_open);
        result= TRUE;
        goto err_with_reopen;
      }
    }

    mysql_mutex_unlock(&LOCK_open);
  }

err_with_reopen:
  if (thd->locked_tables_mode)
  {
    /*
      No other thread has the locked tables open; reopen them and get the
      old locks. This should always succeed (unless some external process
      has removed the tables)
    */
    thd->locked_tables_list.reopen_tables(thd);
    /*
      Since downgrade_lock() won't do anything with shared
      metadata lock it is much simpler to go through all open tables rather
      than picking only those tables that were flushed.
    */
    for (TABLE *tab= thd->open_tables; tab; tab= tab->next)
      tab->mdl_ticket->downgrade_lock(MDL_SHARED_NO_READ_WRITE);
  }
  DBUG_RETURN(result);
}


/**
  Close all tables which match specified connection string or
  if specified string is NULL, then any table with a connection string.
*/

bool close_cached_connection_tables(THD *thd, LEX_STRING *connection)
{
  uint idx;
  TABLE_LIST tmp, *tables= NULL;
  bool result= FALSE;
  DBUG_ENTER("close_cached_connections");
  DBUG_ASSERT(thd);

  memset(&tmp, 0, sizeof(TABLE_LIST));

  mysql_mutex_lock(&LOCK_open);

  for (idx= 0; idx < table_def_cache.records; idx++)
  {
    TABLE_SHARE *share= (TABLE_SHARE *) my_hash_element(&table_def_cache, idx);

    /* Ignore if table is not open or does not have a connect_string */
    if (!share->connect_string.length || !share->ref_count)
      continue;

    /* Compare the connection string */
    if (connection &&
        (connection->length > share->connect_string.length ||
         (connection->length < share->connect_string.length &&
          (share->connect_string.str[connection->length] != '/' &&
           share->connect_string.str[connection->length] != '\\')) ||
         strncasecmp(connection->str, share->connect_string.str,
                     connection->length)))
      continue;

    /* close_cached_tables() only uses these elements */
    tmp.db= share->db.str;
    tmp.table_name= share->table_name.str;
    tmp.next_local= tables;

    tables= (TABLE_LIST *) memdup_root(thd->mem_root, (char*)&tmp, 
                                       sizeof(TABLE_LIST));
  }
  mysql_mutex_unlock(&LOCK_open);

  if (tables)
    result= close_cached_tables(thd, tables, FALSE, LONG_TIMEOUT);

  DBUG_RETURN(result);
}


/**
  Mark all temporary tables which were used by the current statement or
  substatement as free for reuse, but only if the query_id can be cleared.

  @param thd thread context

  @remark For temp tables associated with a open SQL HANDLER the query_id
          is not reset until the HANDLER is closed.
*/

static void mark_temp_tables_as_free_for_reuse(THD *thd)
{
  for (TABLE *table= thd->temporary_tables ; table ; table= table->next)
  {
    if ((table->query_id == thd->query_id) && ! table->open_by_handler)
      mark_tmp_table_for_reuse(table);
  }
}


/**
  Reset a single temporary table.
  Effectively this "closes" one temporary table,
  in a session.

  @param table     Temporary table.
*/

void mark_tmp_table_for_reuse(TABLE *table)
{
  DBUG_ASSERT(table->s->tmp_table);

  table->query_id= 0;
  table->file->ha_reset();

  /* Detach temporary MERGE children from temporary parent. */
  DBUG_ASSERT(table->file);
  table->file->extra(HA_EXTRA_DETACH_CHILDREN);

  /*
    Reset temporary table lock type to it's default value (TL_WRITE).

    Statements such as INSERT INTO .. SELECT FROM tmp, CREATE TABLE
    .. SELECT FROM tmp and UPDATE may under some circumstances modify
    the lock type of the tables participating in the statement. This
    isn't a problem for non-temporary tables since their lock type is
    reset at every open, but the same does not occur for temporary
    tables for historical reasons.

    Furthermore, the lock type of temporary tables is not really that
    important because they can only be used by one query at a time and
    not even twice in a query -- a temporary table is represented by
    only one TABLE object. Nonetheless, it's safer from a maintenance
    point of view to reset the lock type of this singleton TABLE object
    as to not cause problems when the table is reused.

    Even under LOCK TABLES mode its okay to reset the lock type as
    LOCK TABLES is allowed (but ignored) for a temporary table.
  */
  table->reginfo.lock_type= TL_WRITE;
}


/*
  Mark all tables in the list which were used by current substatement
  as free for reuse.

  SYNOPSIS
    mark_used_tables_as_free_for_reuse()
      thd   - thread context
      table - head of the list of tables

  DESCRIPTION
    Marks all tables in the list which were used by current substatement
    (they are marked by its query_id) as free for reuse.

  NOTE
    The reason we reset query_id is that it's not enough to just test
    if table->query_id != thd->query_id to know if a table is in use.

    For example
    SELECT f1_that_uses_t1() FROM t1;
    In f1_that_uses_t1() we will see one instance of t1 where query_id is
    set to query_id of original query.
*/

static void mark_used_tables_as_free_for_reuse(THD *thd, TABLE *table)
{
  for (; table ; table= table->next)
  {
    DBUG_ASSERT(table->pos_in_locked_tables == NULL ||
                table->pos_in_locked_tables->table == table);
    if (table->query_id == thd->query_id)
    {
      table->query_id= 0;
      table->file->ha_reset();
    }
  }
}


/**
  Auxiliary function to close all tables in the open_tables list.

  @param thd Thread context.

  @remark It should not ordinarily be called directly.
*/

static void close_open_tables(THD *thd)
{
  mysql_mutex_assert_not_owner(&LOCK_open);

  DBUG_PRINT("info", ("thd->open_tables: 0x%lx", (long) thd->open_tables));

  while (thd->open_tables)
    close_thread_table(thd, &thd->open_tables);
}


/**
  Close all open instances of the table but keep the MDL lock.

  Works both under LOCK TABLES and in the normal mode.
  Removes all closed instances of the table from the table cache.

  @param     thd     thread handle
  @param[in] share   table share, but is just a handy way to
                     access the table cache key

  @param[in] remove_from_locked_tables
                     TRUE if the table is being dropped or renamed.
                     In that case the documented behaviour is to
                     implicitly remove the table from LOCK TABLES
                     list.
  @param[in] skip_table
                     TABLE instance that should be kept open.

  @pre Must be called with an X MDL lock on the table.
*/

void
close_all_tables_for_name(THD *thd, TABLE_SHARE *share,
                          bool remove_from_locked_tables,
                          TABLE *skip_table)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length= share->table_cache_key.length;
  const char *db= key;
  const char *table_name= db + share->db.length + 1;

  memcpy(key, share->table_cache_key.str, key_length);

  mysql_mutex_assert_not_owner(&LOCK_open);
  for (TABLE **prev= &thd->open_tables; *prev; )
  {
    TABLE *table= *prev;

    if (table->s->table_cache_key.length == key_length &&
        !memcmp(table->s->table_cache_key.str, key, key_length) &&
        table != skip_table)
    {
      thd->locked_tables_list.unlink_from_list(thd,
                                               table->pos_in_locked_tables,
                                               remove_from_locked_tables);
      /*
        Does nothing if the table is not locked.
        This allows one to use this function after a table
        has been unlocked, e.g. in partition management.
      */
      mysql_lock_remove(thd, thd->lock, table);

      /* Inform handler that table will be dropped after close */
      if (table->db_stat && /* Not true for partitioned tables. */
          skip_table == NULL)
        table->file->extra(HA_EXTRA_PREPARE_FOR_DROP);
      close_thread_table(thd, prev);
    }
    else
    {
      /* Step to next entry in open_tables list. */
      prev= &table->next;
    }
  }
  if (skip_table == NULL)
  {
    /* Remove the table share from the cache. */
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, db, table_name,
                     FALSE);
  }
}


/*
  Close all tables used by the current substatement, or all tables
  used by this thread if we are on the upper level.

  SYNOPSIS
    close_thread_tables()
    thd			Thread handler

  IMPLEMENTATION
    Unlocks tables and frees derived tables.
    Put all normal tables used by thread in free list.

    It will only close/mark as free for reuse tables opened by this
    substatement, it will also check if we are closing tables after
    execution of complete query (i.e. we are on upper level) and will
    leave prelocked mode if needed.
*/

void close_thread_tables(THD *thd)
{
  TABLE *table;
  DBUG_ENTER("close_thread_tables");

#ifdef EXTRA_DEBUG
  DBUG_PRINT("tcache", ("open tables:"));
  for (table= thd->open_tables; table; table= table->next)
    DBUG_PRINT("tcache", ("table: '%s'.'%s' 0x%lx", table->s->db.str,
                          table->s->table_name.str, (long) table));
#endif

#if defined(ENABLED_DEBUG_SYNC)
  /* debug_sync may not be initialized for some slave threads */
  if (thd->debug_sync_control)
    DEBUG_SYNC(thd, "before_close_thread_tables");
#endif

  DBUG_ASSERT(thd->transaction.stmt.is_empty() || thd->in_sub_stmt ||
              (thd->state_flags & Open_tables_state::BACKUPS_AVAIL));

  /* Detach MERGE children after every statement. Even under LOCK TABLES. */
  for (table= thd->open_tables; table; table= table->next)
  {
    /* Table might be in use by some outer statement. */
    DBUG_PRINT("tcache", ("table: '%s'  query_id: %lu",
                          table->s->table_name.str, (ulong) table->query_id));
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES ||
        table->query_id == thd->query_id)
    {
      DBUG_ASSERT(table->file);
      table->file->extra(HA_EXTRA_DETACH_CHILDREN);
    }
  }

  /*
    We are assuming here that thd->derived_tables contains ONLY derived
    tables for this substatement. i.e. instead of approach which uses
    query_id matching for determining which of the derived tables belong
    to this substatement we rely on the ability of substatements to
    save/restore thd->derived_tables during their execution.

    TODO: Probably even better approach is to simply associate list of
          derived tables with (sub-)statement instead of thread and destroy
          them at the end of its execution.
  */
  if (thd->derived_tables)
  {
    TABLE *next;
    /*
      Close all derived tables generated in queries like
      SELECT * FROM (SELECT * FROM t1)
    */
    for (table= thd->derived_tables ; table ; table= next)
    {
      next= table->next;
      free_tmp_table(thd, table);
    }
    thd->derived_tables= 0;
  }

  /*
    Mark all temporary tables used by this statement as free for reuse.
  */
  mark_temp_tables_as_free_for_reuse(thd);

  if (thd->locked_tables_mode)
  {

    /* Ensure we are calling ha_reset() for all used tables */
    mark_used_tables_as_free_for_reuse(thd, thd->open_tables);

    /*
      Mark this statement as one that has "unlocked" its tables.
      For purposes of Query_tables_list::lock_tables_state we treat
      any statement which passed through close_thread_tables() as
      such.
    */
    thd->lex->lock_tables_state= Query_tables_list::LTS_NOT_LOCKED;

    /*
      We are under simple LOCK TABLES or we're inside a sub-statement
      of a prelocked statement, so should not do anything else.

      Note that even if we are in LTM_LOCK_TABLES mode and statement
      requires prelocking (e.g. when we are closing tables after
      failing ot "open" all tables required for statement execution)
      we will exit this function a few lines below.
    */
    if (! thd->lex->requires_prelocking())
      DBUG_VOID_RETURN;

    /*
      We are in the top-level statement of a prelocked statement,
      so we have to leave the prelocked mode now with doing implicit
      UNLOCK TABLES if needed.
    */
    if (thd->locked_tables_mode == LTM_PRELOCKED_UNDER_LOCK_TABLES)
      thd->locked_tables_mode= LTM_LOCK_TABLES;

    if (thd->locked_tables_mode == LTM_LOCK_TABLES)
      DBUG_VOID_RETURN;

    thd->leave_locked_tables_mode();

    /* Fallthrough */
  }

  if (thd->lock)
  {
    /*
      For RBR we flush the pending event just before we unlock all the
      tables.  This means that we are at the end of a topmost
      statement, so we ensure that the STMT_END_F flag is set on the
      pending event.  For statements that are *inside* stored
      functions, the pending event will not be flushed: that will be
      handled either before writing a query log event (inside
      binlog_query()) or when preparing a pending event.
     */
    (void)thd->binlog_flush_pending_rows_event(TRUE);
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }

  thd->lex->lock_tables_state= Query_tables_list::LTS_NOT_LOCKED;

  /*
    Closing a MERGE child before the parent would be fatal if the
    other thread tries to abort the MERGE lock in between.
  */
  if (thd->open_tables)
    close_open_tables(thd);

  DBUG_VOID_RETURN;
}


/* move one table to free list */

void close_thread_table(THD *thd, TABLE **table_ptr)
{
  TABLE *table= *table_ptr;
  DBUG_ENTER("close_thread_table");
  DBUG_ASSERT(table->key_read == 0);
  DBUG_ASSERT(!table->file || table->file->inited == handler::NONE);
  mysql_mutex_assert_not_owner(&LOCK_open);
  /*
    The metadata lock must be released after giving back
    the table to the table cache.
  */
  DBUG_ASSERT(thd->mdl_context.is_lock_owner(MDL_key::TABLE,
                                             table->s->db.str,
                                             table->s->table_name.str,
                                             MDL_SHARED));
  table->mdl_ticket= NULL;

  mysql_mutex_lock(&thd->LOCK_thd_data);
  *table_ptr=table->next;
  mysql_mutex_unlock(&thd->LOCK_thd_data);

  if (! table->needs_reopen())
  {
    /* Avoid having MERGE tables with attached children in unused_tables. */
    table->file->extra(HA_EXTRA_DETACH_CHILDREN);
    /* Free memory and reset for next loop. */
    free_field_buffers_larger_than(table, MAX_TDC_BLOB_SIZE);
    table->file->ha_reset();
  }

  /* Do this *before* entering the LOCK_open critical section. */
  if (table->file != NULL)
    table->file->unbind_psi();

  Table_cache *tc= table_cache_manager.get_cache(thd);

  tc->lock();

  if (table->s->has_old_version() || table->needs_reopen() ||
      table_def_shutdown_in_progress)
  {
    tc->remove_table(table);
    mysql_mutex_lock(&LOCK_open);
    intern_close_table(table);
    mysql_mutex_unlock(&LOCK_open);
  }
  else
    tc->release_table(thd, table);

  tc->unlock();
  DBUG_VOID_RETURN;
}


/* close_temporary_tables' internal, 4 is due to uint4korr definition */
static inline uint  tmpkeyval(THD *thd, TABLE *table)
{
  return uint4korr(table->s->table_cache_key.str + table->s->table_cache_key.length - 4);
}


/*
  Close all temporary tables created by 'CREATE TEMPORARY TABLE' for thread
  creates one DROP TEMPORARY TABLE binlog event for each pseudo-thread 
*/

bool close_temporary_tables(THD *thd)
{
  DBUG_ENTER("close_temporary_tables");
  TABLE *table;
  TABLE *next= NULL;
  TABLE *prev_table;
  /* Assume thd->variables.option_bits has OPTION_QUOTE_SHOW_CREATE */
  bool was_quote_show= TRUE;
  bool error= 0;

  if (!thd->temporary_tables)
    DBUG_RETURN(FALSE);

  if (!mysql_bin_log.is_open())
  {
    TABLE *tmp_next;
    for (table= thd->temporary_tables; table; table= tmp_next)
    {
      tmp_next= table->next;
      close_temporary(table, 1, 1);
    }
    thd->temporary_tables= 0;
    DBUG_RETURN(FALSE);
  }

  /* Better add "if exists", in case a RESET MASTER has been done */
  const char stub[]= "DROP /*!40005 TEMPORARY */ TABLE IF EXISTS ";
  uint stub_len= sizeof(stub) - 1;
  char buf[256];
  String s_query= String(buf, sizeof(buf), system_charset_info);
  bool found_user_tables= FALSE;

  memcpy(buf, stub, stub_len);

  /*
    Insertion sort of temp tables by pseudo_thread_id to build ordered list
    of sublists of equal pseudo_thread_id
  */

  for (prev_table= thd->temporary_tables, table= prev_table->next;
       table;
       prev_table= table, table= table->next)
  {
    TABLE *prev_sorted /* same as for prev_table */, *sorted;
    if (is_user_table(table))
    {
      if (!found_user_tables)
        found_user_tables= true;
      for (prev_sorted= NULL, sorted= thd->temporary_tables; sorted != table;
           prev_sorted= sorted, sorted= sorted->next)
      {
        if (!is_user_table(sorted) ||
            tmpkeyval(thd, sorted) > tmpkeyval(thd, table))
        {
          /* move into the sorted part of the list from the unsorted */
          prev_table->next= table->next;
          table->next= sorted;
          if (prev_sorted)
          {
            prev_sorted->next= table;
          }
          else
          {
            thd->temporary_tables= table;
          }
          table= prev_table;
          break;
        }
      }
    }
  }

  /* We always quote db,table names though it is slight overkill */
  if (found_user_tables &&
      !(was_quote_show= test(thd->variables.option_bits & OPTION_QUOTE_SHOW_CREATE)))
  {
    thd->variables.option_bits |= OPTION_QUOTE_SHOW_CREATE;
  }

  /* scan sorted tmps to generate sequence of DROP */
  for (table= thd->temporary_tables; table; table= next)
  {
    if (is_user_table(table))
    {
      bool save_thread_specific_used= thd->thread_specific_used;
      my_thread_id save_pseudo_thread_id= thd->variables.pseudo_thread_id;
      /* Set pseudo_thread_id to be that of the processed table */
      thd->variables.pseudo_thread_id= tmpkeyval(thd, table);
      String db;
      db.append(table->s->db.str);
      /* Loop forward through all tables that belong to a common database
         within the sublist of common pseudo_thread_id to create single
         DROP query 
      */
      for (s_query.length(stub_len);
           table && is_user_table(table) &&
             tmpkeyval(thd, table) == thd->variables.pseudo_thread_id &&
             table->s->db.length == db.length() &&
             strcmp(table->s->db.str, db.ptr()) == 0;
           table= next)
      {
        /*
          We are going to add ` around the table names and possible more
          due to special characters
        */
        append_identifier(thd, &s_query, table->s->table_name.str,
                          strlen(table->s->table_name.str));
        s_query.append(',');
        next= table->next;
        close_temporary(table, 1, 1);
      }
      thd->clear_error();
      const CHARSET_INFO *cs_save= thd->variables.character_set_client;
      thd->variables.character_set_client= system_charset_info;
      thd->thread_specific_used= TRUE;
      Query_log_event qinfo(thd, s_query.ptr(),
                            s_query.length() - 1 /* to remove trailing ',' */,
                            FALSE, TRUE, FALSE, 0);
      qinfo.db= db.ptr();
      qinfo.db_len= db.length();
      thd->variables.character_set_client= cs_save;

      thd->get_stmt_da()->set_overwrite_status(true);
      if ((error= (mysql_bin_log.write_event(&qinfo) || error)))
      {
        /*
          If we're here following THD::cleanup, thence the connection
          has been closed already. So lets print a message to the
          error log instead of pushing yet another error into the
          Diagnostics_area.

          Also, we keep the error flag so that we propagate the error
          up in the stack. This way, if we're the SQL thread we notice
          that close_temporary_tables failed. (Actually, the SQL
          thread only calls close_temporary_tables while applying old
          Start_log_event_v3 events.)
        */
        sql_print_error("Failed to write the DROP statement for "
                        "temporary tables to binary log");
      }
      thd->get_stmt_da()->set_overwrite_status(false);

      thd->variables.pseudo_thread_id= save_pseudo_thread_id;
      thd->thread_specific_used= save_thread_specific_used;
    }
    else
    {
      next= table->next;
      close_temporary(table, 1, 1);
    }
  }
  if (!was_quote_show)
    thd->variables.option_bits&= ~OPTION_QUOTE_SHOW_CREATE; /* restore option */
  thd->temporary_tables=0;

  DBUG_RETURN(error);
}

/*
  Find table in list.

  SYNOPSIS
    find_table_in_list()
    table		Pointer to table list
    offset		Offset to which list in table structure to use
    db_name		Data base name
    table_name		Table name

  NOTES:
    This is called by find_table_in_local_list() and
    find_table_in_global_list().

  RETURN VALUES
    NULL	Table not found
    #		Pointer to found table.
*/

TABLE_LIST *find_table_in_list(TABLE_LIST *table,
                               TABLE_LIST *TABLE_LIST::*link,
                               const char *db_name,
                               const char *table_name)
{
  for (; table; table= table->*link )
  {
    if ((table->table == 0 || table->table->s->tmp_table == NO_TMP_TABLE) &&
        strcmp(table->db, db_name) == 0 &&
        strcmp(table->table_name, table_name) == 0)
      break;
  }
  return table;
}


/**
  Test that table is unique (It's only exists once in the table list)

  @param  thd                   thread handle
  @param  table                 table which should be checked
  @param  table_list            list of tables
  @param  check_alias           whether to check tables' aliases

  NOTE: to exclude derived tables from check we use following mechanism:
    a) during derived table processing set THD::derived_tables_processing
    b) JOIN::prepare set SELECT::exclude_from_table_unique_test if
       THD::derived_tables_processing set. (we can't use JOIN::execute
       because for PS we perform only JOIN::prepare, but we can't set this
       flag in JOIN::prepare if we are not sure that we are in derived table
       processing loop, because multi-update call fix_fields() for some its
       items (which mean JOIN::prepare for subqueries) before unique_table
       call to detect which tables should be locked for write).
    c) find_dup_table skip all tables which belong to SELECT with
       SELECT::exclude_from_table_unique_test set.
    Also SELECT::exclude_from_table_unique_test used to exclude from check
    tables of main SELECT of multi-delete and multi-update

    We also skip tables with TABLE_LIST::prelocking_placeholder set,
    because we want to allow SELECTs from them, and their modification
    will rise the error anyway.

    TODO: when we will have table/view change detection we can do this check
          only once for PS/SP

  @retval !=0  found duplicate
  @retval 0 if table is unique
*/

static
TABLE_LIST* find_dup_table(THD *thd, TABLE_LIST *table, TABLE_LIST *table_list,
                           bool check_alias)
{
  TABLE_LIST *res;
  const char *d_name, *t_name, *t_alias;
  DBUG_ENTER("find_dup_table");
  DBUG_PRINT("enter", ("table alias: %s", table->alias));

  /*
    If this function called for query which update table (INSERT/UPDATE/...)
    then we have in table->table pointer to TABLE object which we are
    updating even if it is VIEW so we need TABLE_LIST of this TABLE object
    to get right names (even if lower_case_table_names used).

    If this function called for CREATE command that we have not opened table
    (table->table equal to 0) and right names is in current TABLE_LIST
    object.
  */
  if (table->table)
  {
    /* All MyISAMMRG children are plain MyISAM tables. */
    DBUG_ASSERT(table->table->file->ht->db_type != DB_TYPE_MRG_MYISAM);

    /* temporary table is always unique */
    if (table->table && table->table->s->tmp_table != NO_TMP_TABLE)
      DBUG_RETURN(0);
    table= table->find_underlying_table(table->table);
    /*
      as far as we have table->table we have to find real TABLE_LIST of
      it in underlying tables
    */
    DBUG_ASSERT(table);
  }
  d_name= table->db;
  t_name= table->table_name;
  t_alias= table->alias;

  DBUG_PRINT("info", ("real table: %s.%s", d_name, t_name));
  for (;;)
  {
    /*
      Table is unique if it is present only once in the global list
      of tables and once in the list of table locks.
    */
    if (! (res= find_table_in_global_list(table_list, d_name, t_name)))
      break;

    /* Skip if same underlying table. */
    if (res->table && (res->table == table->table))
      goto next;

    /* Skip if table alias does not match. */
    if (check_alias)
    {
      if (lower_case_table_names ?
          my_strcasecmp(files_charset_info, t_alias, res->alias) :
          strcmp(t_alias, res->alias))
        goto next;
    }

    /*
      Skip if marked to be excluded (could be a derived table) or if
      entry is a prelocking placeholder.
    */
    if (res->select_lex &&
        !res->select_lex->exclude_from_table_unique_test &&
        !res->prelocking_placeholder)
      break;

    /*
      If we found entry of this table or table of SELECT which already
      processed in derived table or top select of multi-update/multi-delete
      (exclude_from_table_unique_test) or prelocking placeholder.
    */
next:
    table_list= res->next_global;
    DBUG_PRINT("info",
               ("found same copy of table or table which we should skip"));
  }
  DBUG_RETURN(res);
}


/**
  Test that the subject table of INSERT/UPDATE/DELETE/CREATE
  or (in case of MyISAMMRG) one of its children are not used later
  in the query.

  For MyISAMMRG tables, it is assumed that all the underlying
  tables of @c table (if any) are listed right after it and that
  their @c parent_l field points at the main table.


  @retval non-NULL The table list element for the table that
                   represents the duplicate. 
  @retval NULL     No duplicates found.
*/

TABLE_LIST*
unique_table(THD *thd, TABLE_LIST *table, TABLE_LIST *table_list,
             bool check_alias)
{
  TABLE_LIST *dup;
  if (table->table && table->table->file->ht->db_type == DB_TYPE_MRG_MYISAM)
  {
    TABLE_LIST *child;
    dup= NULL;
    /* Check duplicates of all merge children. */
    for (child= table->next_global; child && child->parent_l == table;
         child= child->next_global)
    {
      if ((dup= find_dup_table(thd, child, child->next_global, check_alias)))
        break;
    }
  }
  else
    dup= find_dup_table(thd, table, table_list, check_alias);
  return dup;
}
/*
  Issue correct error message in case we found 2 duplicate tables which
  prevent some update operation

  SYNOPSIS
    update_non_unique_table_error()
    update      table which we try to update
    operation   name of update operation
    duplicate   duplicate table which we found

  NOTE:
    here we hide view underlying tables if we have them
*/

void update_non_unique_table_error(TABLE_LIST *update,
                                   const char *operation,
                                   TABLE_LIST *duplicate)
{
  update= update->top_table();
  duplicate= duplicate->top_table();
  if (!update->view || !duplicate->view ||
      update->view == duplicate->view ||
      update->view_name.length != duplicate->view_name.length ||
      update->view_db.length != duplicate->view_db.length ||
      my_strcasecmp(table_alias_charset,
                    update->view_name.str, duplicate->view_name.str) != 0 ||
      my_strcasecmp(table_alias_charset,
                    update->view_db.str, duplicate->view_db.str) != 0)
  {
    /*
      it is not the same view repeated (but it can be parts of the same copy
      of view), so we have to hide underlying tables.
    */
    if (update->view)
    {
      /* Issue the ER_NON_INSERTABLE_TABLE error for an INSERT */
      if (update->view == duplicate->view)
        my_error(!strncmp(operation, "INSERT", 6) ?
                 ER_NON_INSERTABLE_TABLE : ER_NON_UPDATABLE_TABLE, MYF(0),
                 update->alias, operation);
      else
        my_error(ER_VIEW_PREVENT_UPDATE, MYF(0),
                 (duplicate->view ? duplicate->alias : update->alias),
                 operation, update->alias);
      return;
    }
    if (duplicate->view)
    {
      my_error(ER_VIEW_PREVENT_UPDATE, MYF(0), duplicate->alias, operation,
               update->alias);
      return;
    }
  }
  my_error(ER_UPDATE_TABLE_USED, MYF(0), update->alias);
}


/**
  Find temporary table specified by database and table names in the
  THD::temporary_tables list.

  @return TABLE instance if a temporary table has been found; NULL otherwise.
*/

TABLE *find_temporary_table(THD *thd, const char *db, const char *table_name)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length= create_table_def_key(thd, key, db, table_name, 1);
  return find_temporary_table(thd, key, key_length);
}


/**
  Find a temporary table specified by TABLE_LIST instance in the
  THD::temporary_tables list.

  @return TABLE instance if a temporary table has been found; NULL otherwise.
*/

TABLE *find_temporary_table(THD *thd, const TABLE_LIST *tl)
{
  const char *key;
  uint key_length;
  char key_suffix[TMP_TABLE_KEY_EXTRA];
  TABLE *table;

  key_length= get_table_def_key(tl, &key);

  int4store(key_suffix, thd->server_id);
  int4store(key_suffix + 4, thd->variables.pseudo_thread_id);

  for (table= thd->temporary_tables; table; table= table->next)
  {
    if ((table->s->table_cache_key.length == key_length +
                                             TMP_TABLE_KEY_EXTRA) &&
        !memcmp(table->s->table_cache_key.str, key, key_length) &&
        !memcmp(table->s->table_cache_key.str + key_length, key_suffix,
                TMP_TABLE_KEY_EXTRA))
      return table;
  }
  return NULL;
}


/**
  Find a temporary table specified by a key in the THD::temporary_tables list.

  @return TABLE instance if a temporary table has been found; NULL otherwise.
*/

TABLE *find_temporary_table(THD *thd,
                            const char *table_key,
                            uint table_key_length)
{
  for (TABLE *table= thd->temporary_tables; table; table= table->next)
  {
    if (table->s->table_cache_key.length == table_key_length &&
        !memcmp(table->s->table_cache_key.str, table_key, table_key_length))
    {
      return table;
    }
  }

  return NULL;
}


/**
  Drop a temporary table.

  Try to locate the table in the list of thd->temporary_tables.
  If the table is found:
   - if the table is being used by some outer statement, fail.
   - if the table is locked with LOCK TABLES or by prelocking,
   unlock it and remove it from the list of locked tables
   (THD::lock). Currently only transactional temporary tables
   are locked.
   - Close the temporary table, remove its .FRM
   - remove the table from the list of temporary tables

  This function is used to drop user temporary tables, as well as
  internal tables created in CREATE TEMPORARY TABLE ... SELECT
  or ALTER TABLE. Even though part of the work done by this function
  is redundant when the table is internal, as long as we
  link both internal and user temporary tables into the same
  thd->temporary_tables list, it's impossible to tell here whether
  we're dealing with an internal or a user temporary table.

  In is_trans out-parameter, we return the type of the table:
  either transactional (e.g. innodb) as TRUE or non-transactional
  (e.g. myisam) as FALSE.

  This function assumes that table to be dropped was pre-opened
  using table list provided.

  @retval  0  the table was found and dropped successfully.
  @retval  1  the table was not found in the list of temporary tables
              of this thread
  @retval -1  the table is in use by a outer query
*/

int drop_temporary_table(THD *thd, TABLE_LIST *table_list, bool *is_trans)
{
  DBUG_ENTER("drop_temporary_table");
  DBUG_PRINT("tmptable", ("closing table: '%s'.'%s'",
                          table_list->db, table_list->table_name));

  if (!is_temporary_table(table_list))
    DBUG_RETURN(1);

  TABLE *table= table_list->table;

  /* Table might be in use by some outer statement. */
  if (table->query_id && table->query_id != thd->query_id)
  {
    my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->alias);
    DBUG_RETURN(-1);
  }

  *is_trans= table->file->has_transactions();

  /*
    If LOCK TABLES list is not empty and contains this table,
    unlock the table and remove the table from this list.
  */
  mysql_lock_remove(thd, thd->lock, table);
  close_temporary_table(thd, table, 1, 1);
  table_list->table= NULL;
  DBUG_RETURN(0);
}

/*
  unlink from thd->temporary tables and close temporary table
*/

void close_temporary_table(THD *thd, TABLE *table,
                           bool free_share, bool delete_table)
{
  DBUG_ENTER("close_temporary_table");
  DBUG_PRINT("tmptable", ("closing table: '%s'.'%s' 0x%lx  alias: '%s'",
                          table->s->db.str, table->s->table_name.str,
                          (long) table, table->alias));

  if (table->prev)
  {
    table->prev->next= table->next;
    if (table->prev->next)
      table->next->prev= table->prev;
  }
  else
  {
    /* removing the item from the list */
    DBUG_ASSERT(table == thd->temporary_tables);
    /*
      slave must reset its temporary list pointer to zero to exclude
      passing non-zero value to end_slave via rli->save_temporary_tables
      when no temp tables opened, see an invariant below.
    */
    thd->temporary_tables= table->next;
    if (thd->temporary_tables)
      table->next->prev= 0;
  }
  if (thd->slave_thread)
  {
    /* natural invariant of temporary_tables */
    DBUG_ASSERT(slave_open_temp_tables || !thd->temporary_tables);
    modify_slave_open_temp_tables(thd, -1);
  }
  close_temporary(table, free_share, delete_table);
  DBUG_VOID_RETURN;
}


/*
  Close and delete a temporary table

  NOTE
    This dosn't unlink table from thd->temporary
    If this is needed, use close_temporary_table()
*/

void close_temporary(TABLE *table, bool free_share, bool delete_table)
{
  handlerton *table_type= table->s->db_type();
  DBUG_ENTER("close_temporary");
  DBUG_PRINT("tmptable", ("closing table: '%s'.'%s'",
                          table->s->db.str, table->s->table_name.str));

  free_io_cache(table);
  closefrm(table, 0);
  if (delete_table)
    rm_temporary_table(table_type, table->s->path.str);
  if (free_share)
  {
    free_table_share(table->s);
    my_free(table);
  }
  DBUG_VOID_RETURN;
}


/*
  Used by ALTER TABLE when the table is a temporary one. It changes something
  only if the ALTER contained a RENAME clause (otherwise, table_name is the old
  name).
  Prepares a table cache key, which is the concatenation of db, table_name and
  thd->slave_proxy_id, separated by '\0'.
*/

bool rename_temporary_table(THD* thd, TABLE *table, const char *db,
			    const char *table_name)
{
  char *key;
  uint key_length;
  TABLE_SHARE *share= table->s;
  DBUG_ENTER("rename_temporary_table");

  if (!(key=(char*) alloc_root(&share->mem_root, MAX_DBKEY_LENGTH)))
    DBUG_RETURN(1);				/* purecov: inspected */

  key_length= create_table_def_key(thd, key, db, table_name, 1);
  share->set_table_cache_key(key, key_length);
  DBUG_RETURN(0);
}


/**
   Force all other threads to stop using the table by upgrading
   metadata lock on it and remove unused TABLE instances from cache.

   @param thd      Thread handler
   @param table    Table to remove from cache
   @param function HA_EXTRA_PREPARE_FOR_DROP if table is to be deleted
                   HA_EXTRA_FORCE_REOPEN if table is not be used
                   HA_EXTRA_PREPARE_FOR_RENAME if table is to be renamed

   @note When returning, the table will be unusable for other threads
         until metadata lock is downgraded.

   @retval FALSE Success.
   @retval TRUE  Failure (e.g. because thread was killed).
*/

bool wait_while_table_is_used(THD *thd, TABLE *table,
                              enum ha_extra_function function)
{
  DBUG_ENTER("wait_while_table_is_used");
  DBUG_PRINT("enter", ("table: '%s'  share: 0x%lx  db_stat: %u  version: %lu",
                       table->s->table_name.str, (ulong) table->s,
                       table->db_stat, table->s->version));

  if (thd->mdl_context.upgrade_shared_lock(
             table->mdl_ticket, MDL_EXCLUSIVE,
             thd->variables.lock_wait_timeout))
    DBUG_RETURN(TRUE);

  tdc_remove_table(thd, TDC_RT_REMOVE_NOT_OWN,
                   table->s->db.str, table->s->table_name.str,
                   FALSE);
  /* extra() call must come only after all instances above are closed */
  (void) table->file->extra(function);
  DBUG_RETURN(FALSE);
}


/**
  Close a and drop a just created table in CREATE TABLE ... SELECT.

  @param  thd         Thread handle
  @param  table       TABLE object for the table to be dropped
  @param  db_name     Name of database for this table
  @param  table_name  Name of this table

  This routine assumes that the table to be closed is open only
  by the calling thread, so we needn't wait until other threads
  close the table. It also assumes that the table is first
  in thd->open_ables and a data lock on it, if any, has been
  released. To sum up, it's tuned to work with
  CREATE TABLE ... SELECT and CREATE TABLE .. SELECT only.
  Note, that currently CREATE TABLE ... SELECT is not supported
  under LOCK TABLES. This function, still, can be called in
  prelocked mode, e.g. if we do CREATE TABLE .. SELECT f1();
*/

void drop_open_table(THD *thd, TABLE *table, const char *db_name,
                     const char *table_name)
{
  DBUG_ENTER("drop_open_table");
  if (table->s->tmp_table)
    close_temporary_table(thd, table, 1, 1);
  else
  {
    DBUG_ASSERT(table == thd->open_tables);

    handlerton *table_type= table->s->db_type();

    table->file->extra(HA_EXTRA_PREPARE_FOR_DROP);
    close_thread_table(thd, &thd->open_tables);
    /* Remove the table share from the table cache. */
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, db_name, table_name,
                     FALSE);
    /* Remove the table from the storage engine and rm the .frm. */
    quick_rm_table(thd, table_type, db_name, table_name, 0);
  }
  DBUG_VOID_RETURN;
}


/**
    Check that table exists in table definition cache, on disk
    or in some storage engine.

    @param       thd     Thread context
    @param       table   Table list element
    @param[out]  exists  Out parameter which is set to TRUE if table
                         exists and to FALSE otherwise.

    @note This function acquires LOCK_open internally.

    @note If there is no .FRM file for the table but it exists in one
          of engines (e.g. it was created on another node of NDB cluster)
          this function will fetch and create proper .FRM file for it.

    @retval  TRUE   Some error occurred
    @retval  FALSE  No error. 'exists' out parameter set accordingly.
*/

bool check_if_table_exists(THD *thd, TABLE_LIST *table, bool *exists)
{
  char path[FN_REFLEN + 1];
  TABLE_SHARE *share;
  DBUG_ENTER("check_if_table_exists");

  *exists= TRUE;

  DBUG_ASSERT(thd->mdl_context.
              is_lock_owner(MDL_key::TABLE, table->db,
                            table->table_name, MDL_SHARED));

  mysql_mutex_lock(&LOCK_open);
  share= get_cached_table_share(table->db, table->table_name);
  mysql_mutex_unlock(&LOCK_open);

  if (share)
    goto end;

  build_table_filename(path, sizeof(path) - 1, table->db, table->table_name,
                       reg_ext, 0);

  if (!access(path, F_OK))
    goto end;

  /* .FRM file doesn't exist. Check if some engine can provide it. */
  if (ha_check_if_table_exists(thd, table->db, table->table_name, exists))
  {
    my_printf_error(ER_OUT_OF_RESOURCES, "Failed to open '%-.64s', error while "
                    "unpacking from engine", MYF(0), table->table_name);
    DBUG_RETURN(TRUE);
  }
end:
  DBUG_RETURN(FALSE);
}


/**
  An error handler which converts, if possible, ER_LOCK_DEADLOCK error
  that can occur when we are trying to acquire a metadata lock to
  a request for back-off and re-start of open_tables() process.
*/

class MDL_deadlock_handler : public Internal_error_handler
{
public:
  MDL_deadlock_handler(Open_table_context *ot_ctx_arg)
    : m_ot_ctx(ot_ctx_arg), m_is_active(FALSE)
  {}

  virtual ~MDL_deadlock_handler() {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level level,
                                const char* msg,
                                Sql_condition ** cond_hdl);

private:
  /** Open table context to be used for back-off request. */
  Open_table_context *m_ot_ctx;
  /**
    Indicates that we are already in the process of handling
    ER_LOCK_DEADLOCK error. Allows to re-emit the error from
    the error handler without falling into infinite recursion.
  */
  bool m_is_active;
};


bool MDL_deadlock_handler::handle_condition(THD *,
                                            uint sql_errno,
                                            const char*,
                                            Sql_condition::enum_severity_level,
                                            const char*,
                                            Sql_condition ** cond_hdl)
{
  *cond_hdl= NULL;
  if (! m_is_active && sql_errno == ER_LOCK_DEADLOCK)
  {
    /* Disable the handler to avoid infinite recursion. */
    m_is_active= TRUE;
    (void) m_ot_ctx->request_backoff_action(
             Open_table_context::OT_BACKOFF_AND_RETRY,
             NULL);
    m_is_active= FALSE;
    /*
      If the above back-off request failed, a new instance of
      ER_LOCK_DEADLOCK error was emitted. Thus the current
      instance of error condition can be treated as handled.
    */
    return TRUE;
  }
  return FALSE;
}


/**
  Try to acquire an MDL lock for a table being opened.

  @param[in,out] thd      Session context, to report errors.
  @param[out]    ot_ctx   Open table context, to hold the back off
                          state. If we failed to acquire a lock
                          due to a lock conflict, we add the
                          failed request to the open table context.
  @param[in,out] mdl_request A request for an MDL lock.
                          If we managed to acquire a ticket
                          (no errors or lock conflicts occurred),
                          contains a reference to it on
                          return. However, is not modified if MDL
                          lock type- modifying flags were provided.
  @param[in]    flags flags MYSQL_OPEN_FORCE_SHARED_MDL,
                          MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL or
                          MYSQL_OPEN_FAIL_ON_MDL_CONFLICT
                          @sa open_table().
  @param[out]   mdl_ticket Only modified if there was no error.
                          If we managed to acquire an MDL
                          lock, contains a reference to the
                          ticket, otherwise is set to NULL.

  @retval TRUE  An error occurred.
  @retval FALSE No error, but perhaps a lock conflict, check mdl_ticket.
*/

static bool
open_table_get_mdl_lock(THD *thd, Open_table_context *ot_ctx,
                        MDL_request *mdl_request,
                        uint flags,
                        MDL_ticket **mdl_ticket)
{
  MDL_request mdl_request_shared;

  if (flags & (MYSQL_OPEN_FORCE_SHARED_MDL |
               MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL))
  {
    /*
      MYSQL_OPEN_FORCE_SHARED_MDL flag means that we are executing
      PREPARE for a prepared statement and want to override
      the type-of-operation aware metadata lock which was set
      in the parser/during view opening with a simple shared
      metadata lock.
      This is necessary to allow concurrent execution of PREPARE
      and LOCK TABLES WRITE statement against the same table.

      MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL flag means that we open
      the table in order to get information about it for one of I_S
      queries and also want to override the type-of-operation aware
      shared metadata lock which was set earlier (e.g. during view
      opening) with a high-priority shared metadata lock.
      This is necessary to avoid unnecessary waiting and extra
      ER_WARN_I_S_SKIPPED_TABLE warnings when accessing I_S tables.

      These two flags are mutually exclusive.
    */
    DBUG_ASSERT(!(flags & MYSQL_OPEN_FORCE_SHARED_MDL) ||
                !(flags & MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL));

    mdl_request_shared.init(&mdl_request->key,
                            (flags & MYSQL_OPEN_FORCE_SHARED_MDL) ?
                            MDL_SHARED : MDL_SHARED_HIGH_PRIO,
                            MDL_TRANSACTION);
    mdl_request= &mdl_request_shared;
  }

  if (flags & MYSQL_OPEN_FAIL_ON_MDL_CONFLICT)
  {
    /*
      When table is being open in order to get data for I_S table,
      we might have some tables not only open but also locked (e.g. when
      this happens under LOCK TABLES or in a stored function).
      As a result by waiting on a conflicting metadata lock to go away
      we may create a deadlock which won't entirely belong to the
      MDL subsystem and thus won't be detectable by this subsystem's
      deadlock detector.
      To avoid such situation we skip the trouble-making table if
      there is a conflicting lock.
    */
    if (thd->mdl_context.try_acquire_lock(mdl_request))
      return TRUE;
    if (mdl_request->ticket == NULL)
    {
      my_error(ER_WARN_I_S_SKIPPED_TABLE, MYF(0),
               mdl_request->key.db_name(), mdl_request->key.name());
      return TRUE;
    }
  }
  else
  {
    /*
      We are doing a normal table open. Let us try to acquire a metadata
      lock on the table. If there is a conflicting lock, acquire_lock()
      will wait for it to go away. Sometimes this waiting may lead to a
      deadlock, with the following results:
      1) If a deadlock is entirely within MDL subsystem, it is
         detected by the deadlock detector of this subsystem.
         ER_LOCK_DEADLOCK error is produced. Then, the error handler
         that is installed prior to the call to acquire_lock() attempts
         to request a back-off and retry. Upon success, ER_LOCK_DEADLOCK
         error is suppressed, otherwise propagated up the calling stack.
      2) Otherwise, a deadlock may occur when the wait-for graph
         includes edges not visible to the MDL deadlock detector.
         One such example is a wait on an InnoDB row lock, e.g. when:
         conn C1 gets SR MDL lock on t1 with SELECT * FROM t1
         conn C2 gets a row lock on t2 with  SELECT * FROM t2 FOR UPDATE
         conn C3 gets in and waits on C1 with DROP TABLE t0, t1
         conn C2 continues and blocks on C3 with SELECT * FROM t0
         conn C1 deadlocks by waiting on C2 by issuing SELECT * FROM
         t2 LOCK IN SHARE MODE.
         Such circular waits are currently only resolved by timeouts,
         e.g. @@innodb_lock_wait_timeout or @@lock_wait_timeout.
    */
    MDL_deadlock_handler mdl_deadlock_handler(ot_ctx);

    thd->push_internal_handler(&mdl_deadlock_handler);
    bool result= thd->mdl_context.acquire_lock(mdl_request,
                                               ot_ctx->get_timeout());
    thd->pop_internal_handler();

    if (result && !ot_ctx->can_recover_from_failed_open())
      return TRUE;
  }
  *mdl_ticket= mdl_request->ticket;
  return FALSE;
}


/**
  Check if table's share is being removed from the table definition
  cache and, if yes, wait until the flush is complete.

  @param thd             Thread context.
  @param table_list      Table which share should be checked.
  @param timeout         Timeout for waiting.
  @param deadlock_weight Weight of this wait for deadlock detector.

  @retval FALSE   Success. Share is up to date or has been flushed.
  @retval TRUE    Error (OOM, our was killed, the wait resulted
                  in a deadlock or timeout). Reported.
*/

static bool
tdc_wait_for_old_version(THD *thd, const char *db, const char *table_name,
                         ulong wait_timeout, uint deadlock_weight)
{
  TABLE_SHARE *share;
  bool res= FALSE;

  mysql_mutex_lock(&LOCK_open);
  if ((share= get_cached_table_share(db, table_name)) &&
      share->has_old_version())
  {
    struct timespec abstime;
    set_timespec(abstime, wait_timeout);
    res= share->wait_for_old_version(thd, &abstime, deadlock_weight);
  }
  mysql_mutex_unlock(&LOCK_open);
  return res;
}


/**
  Open a base table.

  @param thd            Thread context.
  @param table_list     Open first table in list.
  @param ot_ctx         Context with flags which modify how open works
                        and which is used to recover from a failed
                        open_table() attempt.
                        Some examples of flags:
                        MYSQL_OPEN_IGNORE_FLUSH - Open table even if
                        someone has done a flush. No version number
                        checking is done.
                        MYSQL_OPEN_HAS_MDL_LOCK - instead of acquiring
                        metadata locks rely on that caller already has
                        appropriate ones.

  Uses a cache of open tables to find a TABLE instance not in use.

  If TABLE_LIST::open_strategy is set to OPEN_IF_EXISTS, the table is
  opened only if it exists. If the open strategy is OPEN_STUB, the
  underlying table is never opened. In both cases, metadata locks are
  always taken according to the lock strategy.

  The function used to open temporary tables, but now it opens base tables
  only.

  @retval TRUE  Open failed. "action" parameter may contain type of action
                needed to remedy problem before retrying again.
  @retval FALSE Success. Members of TABLE_LIST structure are filled properly
                (e.g.  TABLE_LIST::table is set for real tables and
                TABLE_LIST::view is set for views).
*/

bool open_table(THD *thd, TABLE_LIST *table_list, Open_table_context *ot_ctx)
{
  reg1	TABLE *table;
  const char *key;
  uint key_length;
  char	*alias= table_list->alias;
  uint flags= ot_ctx->get_flags();
  MDL_ticket *mdl_ticket;
  int error;
  TABLE_SHARE *share;
  my_hash_value_type hash_value;

  DBUG_ENTER("open_table");

  /*
    The table must not be opened already. The table can be pre-opened for
    some statements if it is a temporary table.

    open_temporary_table() must be used to open temporary tables.
  */
  DBUG_ASSERT(!table_list->table);

  /* an open table operation needs a lot of the stack space */
  if (check_stack_overrun(thd, STACK_MIN_SIZE_FOR_OPEN, (uchar *)&alias))
    DBUG_RETURN(TRUE);

  if (thd->killed)
    DBUG_RETURN(TRUE);

  /*
    Check if we're trying to take a write lock in a read only transaction.

    Note that we allow write locks on log tables as otherwise logging
    to general/slow log would be disabled in read only transactions.
  */
  if (table_list->mdl_request.type >= MDL_SHARED_WRITE &&
      thd->tx_read_only &&
      !(flags & (MYSQL_LOCK_LOG_TABLE | MYSQL_OPEN_HAS_MDL_LOCK)))
  {
    my_error(ER_CANT_EXECUTE_IN_READ_ONLY_TRANSACTION, MYF(0));
    DBUG_RETURN(true);
  }

  key_length= get_table_def_key(table_list, &key);

  /*
    If we're in pre-locked or LOCK TABLES mode, let's try to find the
    requested table in the list of pre-opened and locked tables. If the
    table is not there, return an error - we can't open not pre-opened
    tables in pre-locked/LOCK TABLES mode.
    TODO: move this block into a separate function.
  */
  if (thd->locked_tables_mode &&
      ! (flags & MYSQL_OPEN_GET_NEW_TABLE))
  {						// Using table locks
    TABLE *best_table= 0;
    int best_distance= INT_MIN;
    for (table=thd->open_tables; table ; table=table->next)
    {
      if (table->s->table_cache_key.length == key_length &&
	  !memcmp(table->s->table_cache_key.str, key, key_length))
      {
        if (!my_strcasecmp(system_charset_info, table->alias, alias) &&
            table->query_id != thd->query_id && /* skip tables already used */
            (thd->locked_tables_mode == LTM_LOCK_TABLES ||
             table->query_id == 0))
        {
          int distance= ((int) table->reginfo.lock_type -
                         (int) table_list->lock_type);

          /*
            Find a table that either has the exact lock type requested,
            or has the best suitable lock. In case there is no locked
            table that has an equal or higher lock than requested,
            we us the closest matching lock to be able to produce an error
            message about wrong lock mode on the table. The best_table
            is changed if bd < 0 <= d or bd < d < 0 or 0 <= d < bd.

            distance <  0 - No suitable lock found
            distance >  0 - we have lock mode higher then we require
            distance == 0 - we have lock mode exactly which we need
          */
          if ((best_distance < 0 && distance > best_distance) ||
              (distance >= 0 && distance < best_distance))
          {
            best_distance= distance;
            best_table= table;
            if (best_distance == 0)
            {
              /*
                We have found a perfect match and can finish iterating
                through open tables list. Check for table use conflict
                between calling statement and SP/trigger is done in
                lock_tables().
              */
              break;
            }
          }
        }
      }
    }
    if (best_table)
    {
      table= best_table;
      table->query_id= thd->query_id;
      DBUG_PRINT("info",("Using locked table"));
      goto reset;
    }
    /*
      Is this table a view and not a base table?
      (it is work around to allow to open view with locked tables,
      real fix will be made after definition cache will be made)

      Since opening of view which was not explicitly locked by LOCK
      TABLES breaks metadata locking protocol (potentially can lead
      to deadlocks) it should be disallowed.
    */
    if (thd->mdl_context.is_lock_owner(MDL_key::TABLE,
                                       table_list->db,
                                       table_list->table_name,
                                       MDL_SHARED))
    {
      char path[FN_REFLEN + 1];
      enum legacy_db_type not_used;
      build_table_filename(path, sizeof(path) - 1,
                           table_list->db, table_list->table_name, reg_ext, 0);
      /*
        Note that we can't be 100% sure that it is a view since it's
        possible that we either simply have not found unused TABLE
        instance in THD::open_tables list or were unable to open table
        during prelocking process (in this case in theory we still
        should hold shared metadata lock on it).
      */
      if (dd_frm_type(thd, path, &not_used) == FRMTYPE_VIEW)
      {
        if (!tdc_open_view(thd, table_list, alias, key, key_length,
                           CHECK_METADATA_VERSION))
        {
          DBUG_ASSERT(table_list->view != 0);
          DBUG_RETURN(FALSE); // VIEW
        }
      }
    }
    /*
      No table in the locked tables list. In case of explicit LOCK TABLES
      this can happen if a user did not include the table into the list.
      In case of pre-locked mode locked tables list is generated automatically,
      so we may only end up here if the table did not exist when
      locked tables list was created.
    */
    if (thd->locked_tables_mode == LTM_PRELOCKED)
      my_error(ER_NO_SUCH_TABLE, MYF(0), table_list->db, table_list->alias);
    else
      my_error(ER_TABLE_NOT_LOCKED, MYF(0), alias);
    DBUG_RETURN(TRUE);
  }

  /* Non pre-locked/LOCK TABLES mode. This is the normal use case. */

  if (! (flags & MYSQL_OPEN_HAS_MDL_LOCK))
  {
    /*
      We are not under LOCK TABLES and going to acquire write-lock/
      modify the base table. We need to acquire protection against
      global read lock until end of this statement in order to have
      this statement blocked by active FLUSH TABLES WITH READ LOCK.

      We don't block acquire this protection under LOCK TABLES as
      such protection already acquired at LOCK TABLES time and
      not released until UNLOCK TABLES.

      We don't block statements which modify only temporary tables
      as these tables are not preserved by backup by any form of
      backup which uses FLUSH TABLES WITH READ LOCK.

      TODO: The fact that we sometimes acquire protection against
            GRL only when we encounter table to be write-locked
            slightly increases probability of deadlock.
            This problem will be solved once Alik pushes his
            temporary table refactoring patch and we can start
            pre-acquiring metadata locks at the beggining of
            open_tables() call.
    */
    if (table_list->mdl_request.type >= MDL_SHARED_WRITE &&
        ! (flags & (MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK |
                    MYSQL_OPEN_FORCE_SHARED_MDL |
                    MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL |
                    MYSQL_OPEN_SKIP_SCOPED_MDL_LOCK)) &&
        ! ot_ctx->has_protection_against_grl())
    {
      MDL_request protection_request;
      MDL_deadlock_handler mdl_deadlock_handler(ot_ctx);

      if (thd->global_read_lock.can_acquire_protection())
        DBUG_RETURN(TRUE);

      protection_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                              MDL_STATEMENT);

      /*
        Install error handler which if possible will convert deadlock error
        into request to back-off and restart process of opening tables.
      */
      thd->push_internal_handler(&mdl_deadlock_handler);
      bool result= thd->mdl_context.acquire_lock(&protection_request,
                                                 ot_ctx->get_timeout());
      thd->pop_internal_handler();

      if (result)
        DBUG_RETURN(TRUE);

      ot_ctx->set_has_protection_against_grl();
    }

    if (open_table_get_mdl_lock(thd, ot_ctx, &table_list->mdl_request,
                                flags, &mdl_ticket) ||
        mdl_ticket == NULL)
    {
      DEBUG_SYNC(thd, "before_open_table_wait_refresh");
      DBUG_RETURN(TRUE);
    }
    DEBUG_SYNC(thd, "after_open_table_mdl_shared");
  }
  else
  {
    /*
      Grab reference to the MDL lock ticket that was acquired
      by the caller.
    */
    mdl_ticket= table_list->mdl_request.ticket;
  }

  hash_value= my_calc_hash(&table_def_cache, (uchar*) key, key_length);


  if (table_list->open_strategy == TABLE_LIST::OPEN_IF_EXISTS)
  {
    bool exists;

    if (check_if_table_exists(thd, table_list, &exists))
      DBUG_RETURN(TRUE);

    if (!exists)
      DBUG_RETURN(FALSE);

    /* Table exists. Let us try to open it. */
  }
  else if (table_list->open_strategy == TABLE_LIST::OPEN_STUB)
    DBUG_RETURN(FALSE);

retry_share:
  {
    Table_cache *tc= table_cache_manager.get_cache(thd);

    tc->lock();

    /*
      Try to get unused TABLE object or at least pointer to
      TABLE_SHARE from the table cache.
    */
    table= tc->get_table(thd, hash_value, key, key_length, &share);

    if (table)
    {
      /* We have found an unused TABLE object. */

      if (!(flags & MYSQL_OPEN_IGNORE_FLUSH))
      {
        /*
          TABLE_SHARE::version can only be initialised while holding the
          LOCK_open and in this case no one has a reference to the share
          object, if a reference exists to the share object it is necessary
          to lock both LOCK_open AND all table caches in order to update
          TABLE_SHARE::version. The same locks are required to increment
          refresh_version global variable.

          As result it is safe to compare TABLE_SHARE::version and
          refresh_version values while having only lock on the table
          cache for this thread.

          Table cache should not contain any unused TABLE objects with
          old versions.
        */
        DBUG_ASSERT(!share->has_old_version());

        /*
          Still some of already opened might become outdated (e.g. due to
          concurrent table flush). So we need to compare version of opened
          tables with version of TABLE object we just have got.
        */
        if (thd->open_tables &&
            thd->open_tables->s->version != share->version)
        {
          tc->release_table(thd, table);
          tc->unlock();
          (void)ot_ctx->request_backoff_action(
                          Open_table_context::OT_REOPEN_TABLES,
                          NULL);
          DBUG_RETURN(TRUE);
        }
      }
      tc->unlock();

      /* Call rebind_psi outside of the critical section. */
      DBUG_ASSERT(table->file != NULL);
      table->file->rebind_psi();

      thd->status_var.table_open_cache_hits++;
      goto table_found;
    }
    else if (share)
    {
      /*
        We weren't able to get an unused TABLE object. Still we have
        found TABLE_SHARE for it. So let us try to create new TABLE
        for it. We start by incrementing share's reference count and
        checking its version.
      */
      mysql_mutex_lock(&LOCK_open);
      tc->unlock();
      share->ref_count++;
      goto share_found;
    }
    else
    {
      /*
        We have not found neither TABLE nor TABLE_SHARE object in
        table cache (this means that there are no TABLE objects for
        it in it).
        Let us try to get TABLE_SHARE from table definition cache or
        from disk and then to create TABLE object for it.
      */
      tc->unlock();
    }
  }

  mysql_mutex_lock(&LOCK_open);

  if (!(share= get_table_share_with_discover(thd, table_list, key,
                                             key_length, OPEN_VIEW,
                                             &error,
                                             hash_value)))
  {
    mysql_mutex_unlock(&LOCK_open);
    /*
      If thd->is_error() is not set, we either need discover
      (error == 7), or the error was silenced by the prelocking
      handler (error == 0), in which case we should skip this
      table.
    */
    if (error == 7 && !thd->is_error())
    {
      (void) ot_ctx->request_backoff_action(Open_table_context::OT_DISCOVER,
                                            table_list);
    }
    DBUG_RETURN(TRUE);
  }

  /*
    Check if this TABLE_SHARE-object corresponds to a view. Note, that there is
    no need to call TABLE_SHARE::has_old_version() as we do for regular tables,
    because view shares are always up to date.
  */
  if (share->is_view)
  {
    /*
      If parent_l of the table_list is non null then a merge table
      has this view as child table, which is not supported.
    */
    if (table_list->parent_l)
    {
      my_error(ER_WRONG_MRG_TABLE, MYF(0));
      goto err_unlock;
    }

    /*
      This table is a view. Validate its metadata version: in particular,
      that it was a view when the statement was prepared.
    */
    if (check_and_update_table_version(thd, table_list, share))
      goto err_unlock;
    if (table_list->i_s_requested_object & OPEN_TABLE_ONLY)
    {
      my_error(ER_NO_SUCH_TABLE, MYF(0), table_list->db,
               table_list->table_name);
      goto err_unlock;
    }

    /* Open view */
    if (mysql_make_view(thd, share, table_list, false))
      goto err_unlock;

    /* TODO: Don't free this */
    release_table_share(share);

    DBUG_ASSERT(table_list->view);

    mysql_mutex_unlock(&LOCK_open);
    DBUG_RETURN(FALSE);
  }

  /*
    Note that situation when we are trying to open a table for what
    was a view during previous execution of PS will be handled in by
    the caller. Here we should simply open our table even if
    TABLE_LIST::view is true.
  */

  if (table_list->i_s_requested_object &  OPEN_VIEW_ONLY)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), table_list->db,
             table_list->table_name);
    goto err_unlock;
  }

share_found:
  if (!(flags & MYSQL_OPEN_IGNORE_FLUSH))
  {
    if (share->has_old_version())
    {
      /*
        We already have an MDL lock. But we have encountered an old
        version of table in the table definition cache which is possible
        when someone changes the table version directly in the cache
        without acquiring a metadata lock (e.g. this can happen during
        "rolling" FLUSH TABLE(S)).
        Release our reference to share, wait until old version of
        share goes away and then try to get new version of table share.
      */
      release_table_share(share);
      mysql_mutex_unlock(&LOCK_open);

      MDL_deadlock_handler mdl_deadlock_handler(ot_ctx);
      bool wait_result;

      thd->push_internal_handler(&mdl_deadlock_handler);
      wait_result= tdc_wait_for_old_version(thd, table_list->db,
                                            table_list->table_name,
                                            ot_ctx->get_timeout(),
                                            mdl_ticket->get_deadlock_weight());
      thd->pop_internal_handler();

      if (wait_result)
        DBUG_RETURN(TRUE);

      goto retry_share;
    }

    if (thd->open_tables && thd->open_tables->s->version != share->version)
    {
      /*
        If the version changes while we're opening the tables,
        we have to back off, close all the tables opened-so-far,
        and try to reopen them. Note: refresh_version is currently
        changed only during FLUSH TABLES.
      */
      release_table_share(share);
      mysql_mutex_unlock(&LOCK_open);
      (void)ot_ctx->request_backoff_action(Open_table_context::OT_REOPEN_TABLES,
                                           NULL);
      DBUG_RETURN(TRUE);
    }
  }

  mysql_mutex_unlock(&LOCK_open);

  /* make a new table */
  if (!(table= (TABLE*) my_malloc(sizeof(*table), MYF(MY_WME))))
    goto err_lock;

  error= open_table_from_share(thd, share, alias,
                               (uint) (HA_OPEN_KEYFILE |
                                       HA_OPEN_RNDFILE |
                                       HA_GET_INDEX |
                                       HA_TRY_READ_ONLY),
                               (READ_KEYINFO | COMPUTE_TYPES |
                                EXTRA_RECORD),
                               thd->open_options, table, FALSE);

  if (error)
  {
    my_free(table);

    if (error == 7)
      (void) ot_ctx->request_backoff_action(Open_table_context::OT_DISCOVER,
                                            table_list);
    else if (share->crashed)
      (void) ot_ctx->request_backoff_action(Open_table_context::OT_REPAIR,
                                            table_list);
    goto err_lock;
  }
  if (open_table_entry_fini(thd, share, table))
  {
    closefrm(table, 0);
    my_free(table);
    goto err_lock;
  }
  {
    /* Add new TABLE object to table cache for this connection. */
    Table_cache *tc= table_cache_manager.get_cache(thd);

    tc->lock();

    if (tc->add_used_table(thd, table))
    {
      tc->unlock();
      goto err_lock;
    }
    tc->unlock();
  }
  thd->status_var.table_open_cache_misses++;

table_found:
  table->mdl_ticket= mdl_ticket;

  table->next= thd->open_tables;		/* Link into simple list */
  thd->set_open_tables(table);

  table->reginfo.lock_type=TL_READ;		/* Assume read */

 reset:
  table->created= TRUE;
  /*
    Check that there is no reference to a condition from an earlier query
    (cf. Bug#58553). 
  */
  DBUG_ASSERT(table->file->pushed_cond == NULL);
  table_list->updatable= 1; // It is not derived table nor non-updatable VIEW
  table_list->table= table;

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (table->part_info &&
      !(table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION))
  {

    /* Set all [named] partitions as used. */
    if (table->part_info->set_partition_bitmaps(table_list))
      DBUG_RETURN(true);
  }
  else if (table_list->partition_names)
  {
    /* Don't allow PARTITION () clause on a nonpartitioned table */
    my_error(ER_PARTITION_CLAUSE_ON_NONPARTITIONED, MYF(0));
    DBUG_RETURN(true);
  }
#endif

  table->init(thd, table_list);

  DBUG_RETURN(FALSE);

err_lock:
  mysql_mutex_lock(&LOCK_open);
err_unlock:
  release_table_share(share);
  mysql_mutex_unlock(&LOCK_open);

  DBUG_RETURN(TRUE);
}


/**
   Find table in the list of open tables.

   @param list       List of TABLE objects to be inspected.
   @param db         Database name
   @param table_name Table name

   @return Pointer to the TABLE object found, 0 if no table found.
*/

TABLE *find_locked_table(TABLE *list, const char *db, const char *table_name)
{
  char	key[MAX_DBKEY_LENGTH];
  uint key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;

  for (TABLE *table= list; table ; table=table->next)
  {
    if (table->s->table_cache_key.length == key_length &&
	!memcmp(table->s->table_cache_key.str, key, key_length))
      return table;
  }
  return(0);
}


/**
   Find instance of TABLE with upgradable or exclusive metadata
   lock from the list of open tables, emit error if no such table
   found.

   @param thd        Thread context
   @param db         Database name.
   @param table_name Name of table.
   @param no_error   Don't emit error if no suitable TABLE
                     instance were found.

   @note This function checks if the connection holds a global IX
         metadata lock. If no such lock is found, it is not safe to
         upgrade the lock and ER_TABLE_NOT_LOCKED_FOR_WRITE will be
         reported.

   @return Pointer to TABLE instance with MDL_SHARED_UPGRADABLE
           MDL_SHARED_NO_WRITE, MDL_SHARED_NO_READ_WRITE, or
           MDL_EXCLUSIVE metadata lock, NULL otherwise.
*/

TABLE *find_table_for_mdl_upgrade(THD *thd, const char *db,
                                  const char *table_name, bool no_error)
{
  TABLE *tab= find_locked_table(thd->open_tables, db, table_name);

  if (!tab)
  {
    if (!no_error)
      my_error(ER_TABLE_NOT_LOCKED, MYF(0), table_name);
    return NULL;
  }

  /*
    It is not safe to upgrade the metadata lock without a global IX lock.
    This can happen with FLUSH TABLES <list> WITH READ LOCK as we in these
    cases don't take a global IX lock in order to be compatible with
    global read lock.
  */
  if (!thd->mdl_context.is_lock_owner(MDL_key::GLOBAL, "", "",
                                      MDL_INTENTION_EXCLUSIVE))
  {
    if (!no_error)
      my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE, MYF(0), table_name);
    return NULL;
  }

  while (tab->mdl_ticket != NULL &&
         !tab->mdl_ticket->is_upgradable_or_exclusive() &&
         (tab= find_locked_table(tab->next, db, table_name)))
    continue;

  if (!tab && !no_error)
    my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE, MYF(0), table_name);

  return tab;
}


/***********************************************************************
  class Locked_tables_list implementation. Declared in sql_class.h
************************************************************************/

/**
  Enter LTM_LOCK_TABLES mode.

  Enter the LOCK TABLES mode using all the tables that are
  currently open and locked in this connection.
  Initializes a TABLE_LIST instance for every locked table.

  @param  thd  thread handle

  @return TRUE if out of memory.
*/

bool
Locked_tables_list::init_locked_tables(THD *thd)
{
  DBUG_ASSERT(thd->locked_tables_mode == LTM_NONE);
  DBUG_ASSERT(m_locked_tables == NULL);
  DBUG_ASSERT(m_reopen_array == NULL);
  DBUG_ASSERT(m_locked_tables_count == 0);

  for (TABLE *table= thd->open_tables; table;
       table= table->next, m_locked_tables_count++)
  {
    TABLE_LIST *src_table_list= table->pos_in_table_list;
    char *db, *table_name, *alias;
    size_t db_len= src_table_list->db_length;
    size_t table_name_len= src_table_list->table_name_length;
    size_t alias_len= strlen(src_table_list->alias);
    TABLE_LIST *dst_table_list;

    if (! multi_alloc_root(&m_locked_tables_root,
                           &dst_table_list, sizeof(*dst_table_list),
                           &db, db_len + 1,
                           &table_name, table_name_len + 1,
                           &alias, alias_len + 1,
                           NullS))
    {
      unlock_locked_tables(0);
      return TRUE;
    }

    memcpy(db, src_table_list->db, db_len + 1);
    memcpy(table_name, src_table_list->table_name, table_name_len + 1);
    memcpy(alias, src_table_list->alias, alias_len + 1);
    /**
      Sic: remember the *actual* table level lock type taken, to
      acquire the exact same type in reopen_tables().
      E.g. if the table was locked for write, src_table_list->lock_type is
      TL_WRITE_DEFAULT, whereas reginfo.lock_type has been updated from
      thd->update_lock_default.
    */
    dst_table_list->init_one_table(db, db_len, table_name, table_name_len,
                                   alias,
                                   src_table_list->table->reginfo.lock_type);
    dst_table_list->table= table;
    dst_table_list->mdl_request.ticket= src_table_list->mdl_request.ticket;

    /* Link last into the list of tables */
    *(dst_table_list->prev_global= m_locked_tables_last)= dst_table_list;
    m_locked_tables_last= &dst_table_list->next_global;
    table->pos_in_locked_tables= dst_table_list;
  }
  if (m_locked_tables_count)
  {
    /**
      Allocate an auxiliary array to pass to mysql_lock_tables()
      in reopen_tables(). reopen_tables() is a critical
      path and we don't want to complicate it with extra allocations.
    */
    m_reopen_array= (TABLE**)alloc_root(&m_locked_tables_root,
                                        sizeof(TABLE*) *
                                        (m_locked_tables_count+1));
    if (m_reopen_array == NULL)
    {
      unlock_locked_tables(0);
      return TRUE;
    }
  }
  thd->enter_locked_tables_mode(LTM_LOCK_TABLES);

  return FALSE;
}


/**
  Leave LTM_LOCK_TABLES mode if it's been entered.

  Close all locked tables, free memory, and leave the mode.

  @note This function is a no-op if we're not in LOCK TABLES.
*/

void
Locked_tables_list::unlock_locked_tables(THD *thd)

{
  if (thd)
  {
    DBUG_ASSERT(!thd->in_sub_stmt &&
                !(thd->state_flags & Open_tables_state::BACKUPS_AVAIL));
    /*
      Sic: we must be careful to not close open tables if
      we're not in LOCK TABLES mode: unlock_locked_tables() is
      sometimes called implicitly, expecting no effect on
      open tables, e.g. from begin_trans().
    */
    if (thd->locked_tables_mode != LTM_LOCK_TABLES)
      return;

    for (TABLE_LIST *table_list= m_locked_tables;
         table_list; table_list= table_list->next_global)
    {
      /*
        Clear the position in the list, the TABLE object will be
        returned to the table cache.
      */
      table_list->table->pos_in_locked_tables= NULL;
    }
    thd->leave_locked_tables_mode();

    DBUG_ASSERT(thd->transaction.stmt.is_empty());
    close_thread_tables(thd);
    /*
      We rely on the caller to implicitly commit the
      transaction and release transactional locks.
    */
  }
  /*
    After closing tables we can free memory used for storing lock
    request for metadata locks and TABLE_LIST elements.
  */
  free_root(&m_locked_tables_root, MYF(0));
  m_locked_tables= NULL;
  m_locked_tables_last= &m_locked_tables;
  m_reopen_array= NULL;
  m_locked_tables_count= 0;
}


/**
  Unlink a locked table from the locked tables list, either
  temporarily or permanently.

  @param  thd        thread handle
  @param  table_list the element of locked tables list.
                     The implementation assumes that this argument
                     points to a TABLE_LIST element linked into
                     the locked tables list. Passing a TABLE_LIST
                     instance that is not part of locked tables
                     list will lead to a crash.
  @param  remove_from_locked_tables
                      TRUE if the table is removed from the list
                      permanently.

  This function is a no-op if we're not under LOCK TABLES.

  @sa Locked_tables_list::reopen_tables()
*/


void Locked_tables_list::unlink_from_list(THD *thd,
                                          TABLE_LIST *table_list,
                                          bool remove_from_locked_tables)
{
  /*
    If mode is not LTM_LOCK_TABLES, we needn't do anything. Moreover,
    outside this mode pos_in_locked_tables value is not trustworthy.
  */
  if (thd->locked_tables_mode != LTM_LOCK_TABLES)
    return;

  /*
    table_list must be set and point to pos_in_locked_tables of some
    table.
  */
  DBUG_ASSERT(table_list->table->pos_in_locked_tables == table_list);

  /* Clear the pointer, the table will be returned to the table cache. */
  table_list->table->pos_in_locked_tables= NULL;

  /* Mark the table as closed in the locked tables list. */
  table_list->table= NULL;

  /*
    If the table is being dropped or renamed, remove it from
    the locked tables list (implicitly drop the LOCK TABLES lock
    on it).
  */
  if (remove_from_locked_tables)
  {
    *table_list->prev_global= table_list->next_global;
    if (table_list->next_global == NULL)
      m_locked_tables_last= table_list->prev_global;
    else
      table_list->next_global->prev_global= table_list->prev_global;
  }
}

/**
  This is an attempt to recover (somewhat) in case of an error.
  If we failed to reopen a closed table, let's unlink it from the
  list and forget about it. From a user perspective that would look
  as if the server "lost" the lock on one of the locked tables.

  @note This function is a no-op if we're not under LOCK TABLES.
*/

void Locked_tables_list::
unlink_all_closed_tables(THD *thd, MYSQL_LOCK *lock, size_t reopen_count)
{
  /* If we managed to take a lock, unlock tables and free the lock. */
  if (lock)
    mysql_unlock_tables(thd, lock);
  /*
    If a failure happened in reopen_tables(), we may have succeeded
    reopening some tables, but not all.
    This works when the connection was killed in mysql_lock_tables().
  */
  if (reopen_count)
  {
    while (reopen_count--)
    {
      /*
        When closing the table, we must remove it
        from thd->open_tables list.
        We rely on the fact that open_table() that was used
        in reopen_tables() always links the opened table
        to the beginning of the open_tables list.
      */
      DBUG_ASSERT(thd->open_tables == m_reopen_array[reopen_count]);

      thd->open_tables->pos_in_locked_tables->table= NULL;

      close_thread_table(thd, &thd->open_tables);
    }
  }
  /* Exclude all closed tables from the LOCK TABLES list. */
  for (TABLE_LIST *table_list= m_locked_tables; table_list; table_list=
       table_list->next_global)
  {
    if (table_list->table == NULL)
    {
      /* Unlink from list. */
      *table_list->prev_global= table_list->next_global;
      if (table_list->next_global == NULL)
        m_locked_tables_last= table_list->prev_global;
      else
        table_list->next_global->prev_global= table_list->prev_global;
    }
  }
}


/**
  Reopen the tables locked with LOCK TABLES and temporarily closed
  by a DDL statement or FLUSH TABLES.

  @note This function is a no-op if we're not under LOCK TABLES.

  @return TRUE if an error reopening the tables. May happen in
               case of some fatal system error only, e.g. a disk
               corruption, out of memory or a serious bug in the
               locking.
*/

bool
Locked_tables_list::reopen_tables(THD *thd)
{
  Open_table_context ot_ctx(thd, MYSQL_OPEN_REOPEN);
  size_t reopen_count= 0;
  MYSQL_LOCK *lock;
  MYSQL_LOCK *merged_lock;

  for (TABLE_LIST *table_list= m_locked_tables;
       table_list; table_list= table_list->next_global)
  {
    if (table_list->table)                      /* The table was not closed */
      continue;

    /* Links into thd->open_tables upon success */
    if (open_table(thd, table_list, &ot_ctx))
    {
      unlink_all_closed_tables(thd, 0, reopen_count);
      return TRUE;
    }
    table_list->table->pos_in_locked_tables= table_list;
    /* See also the comment on lock type in init_locked_tables(). */
    table_list->table->reginfo.lock_type= table_list->lock_type;

    DBUG_ASSERT(reopen_count < m_locked_tables_count);
    m_reopen_array[reopen_count++]= table_list->table;
  }
  if (reopen_count)
  {
    thd->in_lock_tables= 1;
    /*
      We re-lock all tables with mysql_lock_tables() at once rather
      than locking one table at a time because of the case
      reported in Bug#45035: when the same table is present
      in the list many times, thr_lock.c fails to grant READ lock
      on a table that is already locked by WRITE lock, even if
      WRITE lock is taken by the same thread. If READ and WRITE
      lock are passed to thr_lock.c in the same list, everything
      works fine. Patching legacy code of thr_lock.c is risking to
      break something else.
    */
    lock= mysql_lock_tables(thd, m_reopen_array, reopen_count,
                            MYSQL_OPEN_REOPEN);
    thd->in_lock_tables= 0;
    if (lock == NULL || (merged_lock=
                         mysql_lock_merge(thd->lock, lock)) == NULL)
    {
      unlink_all_closed_tables(thd, lock, reopen_count);
      if (! thd->killed)
        my_error(ER_LOCK_DEADLOCK, MYF(0));
      return TRUE;
    }
    thd->lock= merged_lock;
  }
  return FALSE;
}


/*
  Function to assign a new table map id to a table share.

  PARAMETERS

    share - Pointer to table share structure

  DESCRIPTION

    We are intentionally not checking that share->mutex is locked
    since this function should only be called when opening a table
    share and before it is entered into the table_def_cache (meaning
    that it cannot be fetched by another thread, even accidentally).

  PRE-CONDITION(S)

    share is non-NULL
    The LOCK_open mutex is locked.

  POST-CONDITION(S)

    share->table_map_id is given a value that with a high certainty is
    not used by any other table (the only case where a table id can be
    reused is on wrap-around, which means more than 4 billion table
    share opens have been executed while one table was open all the
    time).

    share->table_map_id is not ~0UL.
 */
static ulong last_table_id= ~0UL;

void assign_new_table_id(TABLE_SHARE *share)
{

  DBUG_ENTER("assign_new_table_id");

  /* Preconditions */
  DBUG_ASSERT(share != NULL);
  mysql_mutex_assert_owner(&LOCK_open);

  ulong tid= ++last_table_id;                   /* get next id */
  /*
    There is one reserved number that cannot be used.  Remember to
    change this when 6-byte global table id's are introduced.
  */
  if (unlikely(tid == ~0UL))
    tid= ++last_table_id;
  share->table_map_id= tid;
  DBUG_PRINT("info", ("table_id=%lu", tid));

  /* Post conditions */
  DBUG_ASSERT(share->table_map_id != ~0UL);

  DBUG_VOID_RETURN;
}

#ifndef DBUG_OFF
/* Cause a spurious statement reprepare for debug purposes. */
static bool inject_reprepare(THD *thd)
{
  Reprepare_observer *reprepare_observer= thd->get_reprepare_observer();

  if (reprepare_observer && !thd->stmt_arena->is_reprepared)
  {
    (void)reprepare_observer->report_error(thd);
    return true;
  }

  return false;
}
#endif

/**
  Compare metadata versions of an element obtained from the table
  definition cache and its corresponding node in the parse tree.

  @details If the new and the old values mismatch, invoke
  Metadata_version_observer.
  At prepared statement prepare, all TABLE_LIST version values are
  NULL and we always have a mismatch. But there is no observer set
  in THD, and therefore no error is reported. Instead, we update
  the value in the parse tree, effectively recording the original
  version.
  At prepared statement execute, an observer may be installed.  If
  there is a version mismatch, we push an error and return TRUE.

  For conventional execution (no prepared statements), the
  observer is never installed.

  @sa Execute_observer
  @sa check_prepared_statement() to see cases when an observer is installed
  @sa TABLE_LIST::is_table_ref_id_equal()
  @sa TABLE_SHARE::get_table_ref_id()

  @param[in]      thd         used to report errors
  @param[in,out]  tables      TABLE_LIST instance created by the parser
                              Metadata version information in this object
                              is updated upon success.
  @param[in]      table_share an element from the table definition cache

  @retval  TRUE  an error, which has been reported
  @retval  FALSE success, version in TABLE_LIST has been updated
*/

static bool
check_and_update_table_version(THD *thd,
                               TABLE_LIST *tables, TABLE_SHARE *table_share)
{
  if (! tables->is_table_ref_id_equal(table_share))
  {
    Reprepare_observer *reprepare_observer= thd->get_reprepare_observer();

    if (reprepare_observer &&
        reprepare_observer->report_error(thd))
    {
      /*
        Version of the table share is different from the
        previous execution of the prepared statement, and it is
        unacceptable for this SQLCOM. Error has been reported.
      */
      DBUG_ASSERT(thd->is_error());
      return TRUE;
    }
    /* Always maintain the latest version and type */
    tables->set_table_ref_id(table_share);
  }

  DBUG_EXECUTE_IF("reprepare_each_statement", return inject_reprepare(thd););
  return FALSE;
}


/**
  Compares versions of a stored routine obtained from the sp cache
  and the version used at prepare.

  @details If the new and the old values mismatch, invoke
  Metadata_version_observer.
  At prepared statement prepare, all Sroutine_hash_entry version values
  are NULL and we always have a mismatch. But there is no observer set
  in THD, and therefore no error is reported. Instead, we update
  the value in Sroutine_hash_entry, effectively recording the original
  version.
  At prepared statement execute, an observer may be installed.  If
  there is a version mismatch, we push an error and return TRUE.

  For conventional execution (no prepared statements), the
  observer is never installed.

  @param[in]      thd         used to report errors
  @param[in/out]  rt          pointer to stored routine entry in the
                              parse tree
  @param[in]      sp          pointer to stored routine cache entry.
                              Can be NULL if there is no such routine.
  @retval  TRUE  an error, which has been reported
  @retval  FALSE success, version in Sroutine_hash_entry has been updated
*/

static bool
check_and_update_routine_version(THD *thd, Sroutine_hash_entry *rt,
                                 sp_head *sp)
{
  ulong spc_version= sp_cache_version();
  /* sp is NULL if there is no such routine. */
  ulong version= sp ? sp->sp_cache_version() : spc_version;
  /*
    If the version in the parse tree is stale,
    or the version in the cache is stale and sp is not used,
    we need to reprepare.
    Sic: version != spc_version <--> sp is not NULL.
  */
  if (rt->m_sp_cache_version != version ||
      (version != spc_version && !sp->is_invoked()))
  {
    Reprepare_observer *reprepare_observer= thd->get_reprepare_observer();

    if (reprepare_observer &&
        reprepare_observer->report_error(thd))
    {
      /*
        Version of the sp cache is different from the
        previous execution of the prepared statement, and it is
        unacceptable for this SQLCOM. Error has been reported.
      */
      DBUG_ASSERT(thd->is_error());
      return TRUE;
    }
    /* Always maintain the latest cache version. */
    rt->m_sp_cache_version= version;
  }
  return FALSE;
}


/**
   Open view by getting its definition from disk (and table cache in future).

   @param thd               Thread handle
   @param table_list        TABLE_LIST with db, table_name & belong_to_view
   @param alias             Alias name
   @param cache_key         Key for table definition cache
   @param cache_key_length  Length of cache_key
   @param flags             Flags which modify how we open the view

   @todo This function is needed for special handling of views under
         LOCK TABLES. We probably should get rid of it in long term.

   @return FALSE if success, TRUE - otherwise.
*/

bool tdc_open_view(THD *thd, TABLE_LIST *table_list, const char *alias,
                   const char *cache_key, uint cache_key_length, uint flags)
{
  int error;
  my_hash_value_type hash_value;
  TABLE_SHARE *share;

  hash_value= my_calc_hash(&table_def_cache, (uchar*) cache_key,
                           cache_key_length);
  mysql_mutex_lock(&LOCK_open);

  if (!(share= get_table_share(thd, table_list, cache_key,
                               cache_key_length,
                               OPEN_VIEW, &error,
                               hash_value)))
    goto err;

  if ((flags & CHECK_METADATA_VERSION))
  {
    /*
      Check TABLE_SHARE-version of view only if we have been instructed to do
      so. We do not need to check the version if we're executing CREATE VIEW or
      ALTER VIEW statements.

      In the future, this functionality should be moved out from
      tdc_open_view(), and  tdc_open_view() should became a part of a clean
      table-definition-cache interface.
    */
    if (check_and_update_table_version(thd, table_list, share))
    {
      release_table_share(share);
      goto err;
    }
  }

  if (share->is_view &&
      !mysql_make_view(thd, share, table_list, (flags & OPEN_VIEW_NO_PARSE)))
  {
    release_table_share(share);
    mysql_mutex_unlock(&LOCK_open);
    return FALSE;
  }

  my_error(ER_WRONG_OBJECT, MYF(0), share->db.str, share->table_name.str, "VIEW");
  release_table_share(share);
err:
  mysql_mutex_unlock(&LOCK_open);
  return TRUE;
}


/**
   Finalize the process of TABLE creation by loading table triggers
   and taking action if a HEAP table content was emptied implicitly.
*/

static bool open_table_entry_fini(THD *thd, TABLE_SHARE *share, TABLE *entry)
{
  if (Table_triggers_list::check_n_load(thd, share->db.str,
                                        share->table_name.str, entry, 0))
    return TRUE;

  /*
    If we are here, there was no fatal error (but error may be still
    unitialized).
  */
  if (unlikely(entry->file->implicit_emptied))
  {
    entry->file->implicit_emptied= 0;
    if (mysql_bin_log.is_open())
    {
      bool error= false;
      String temp_buf;
      error= temp_buf.append("DELETE FROM ");
      append_identifier(thd, &temp_buf, share->db.str, strlen(share->db.str));
      error= temp_buf.append(".");
      append_identifier(thd, &temp_buf, share->table_name.str,
                        strlen(share->table_name.str));
      int errcode= query_error_code(thd, TRUE);
      if (thd->binlog_query(THD::STMT_QUERY_TYPE,
                            temp_buf.c_ptr_safe(), temp_buf.length(),
                            FALSE, FALSE, FALSE, errcode))
        return TRUE;
      if (error)
      {
        /*
          As replication is maybe going to be corrupted, we need to warn the
          DBA on top of warning the client (which will automatically be done
          because of MYF(MY_WME) in my_malloc() above).
        */
        sql_print_error("When opening HEAP table, could not allocate memory "
                        "to write 'DELETE FROM `%s`.`%s`' to the binary log",
                        share->db.str, share->table_name.str);
        delete entry->triggers;
        return TRUE;
      }
    }
  }
  return FALSE;
}


/**
   Auxiliary routine which is used for performing automatical table repair.
*/

static bool auto_repair_table(THD *thd, TABLE_LIST *table_list)
{
  const char *cache_key;
  uint cache_key_length;
  TABLE_SHARE *share;
  TABLE *entry;
  int not_used;
  bool result= TRUE;
  my_hash_value_type hash_value;

  cache_key_length= get_table_def_key(table_list, &cache_key);

  thd->clear_error();

  hash_value= my_calc_hash(&table_def_cache, (uchar*) cache_key,
                           cache_key_length);
  mysql_mutex_lock(&LOCK_open);

  if (!(share= get_table_share(thd, table_list, cache_key,
                               cache_key_length,
                               OPEN_VIEW, &not_used,
                               hash_value)))
    goto end_unlock;

  if (share->is_view)
  {
    release_table_share(share);
    goto end_unlock;
  }

  if (!(entry= (TABLE*)my_malloc(sizeof(TABLE), MYF(MY_WME))))
  {
    release_table_share(share);
    goto end_unlock;
  }
  mysql_mutex_unlock(&LOCK_open);

  if (open_table_from_share(thd, share, table_list->alias,
                            (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE |
                                    HA_GET_INDEX |
                                    HA_TRY_READ_ONLY),
                            READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
                            ha_open_options | HA_OPEN_FOR_REPAIR,
                            entry, FALSE) || ! entry->file ||
      (entry->file->is_crashed() && entry->file->ha_check_and_repair(thd)))
  {
    /* Give right error message */
    thd->clear_error();
    my_error(ER_NOT_KEYFILE, MYF(0), share->table_name.str);
    sql_print_error("Couldn't repair table: %s.%s", share->db.str,
                    share->table_name.str);
    if (entry->file)
      closefrm(entry, 0);
  }
  else
  {
    thd->clear_error();			// Clear error message
    closefrm(entry, 0);
    result= FALSE;
  }
  my_free(entry);

  table_cache_manager.lock_all_and_tdc();
  release_table_share(share);
  /* Remove the repaired share from the table cache. */
  tdc_remove_table(thd, TDC_RT_REMOVE_ALL,
                   table_list->db, table_list->table_name,
                   TRUE);
  table_cache_manager.unlock_all_and_tdc();
  return result;
end_unlock:
  mysql_mutex_unlock(&LOCK_open);
  return result;
}


/** Open_table_context */

Open_table_context::Open_table_context(THD *thd, uint flags)
  :m_failed_table(NULL),
   m_start_of_statement_svp(thd->mdl_context.mdl_savepoint()),
   m_timeout(flags & MYSQL_LOCK_IGNORE_TIMEOUT ?
             LONG_TIMEOUT : thd->variables.lock_wait_timeout),
   m_flags(flags),
   m_action(OT_NO_ACTION),
   m_has_locks(thd->mdl_context.has_locks()),
   m_has_protection_against_grl(FALSE)
{}


/**
  Check if we can back-off and set back off action if we can.
  Otherwise report and return error.

  @retval  TRUE if back-off is impossible.
  @retval  FALSE if we can back off. Back off action has been set.
*/

bool
Open_table_context::
request_backoff_action(enum_open_table_action action_arg,
                       TABLE_LIST *table)
{
  /*
    A back off action may be one of three kinds:

    * We met a broken table that needs repair, or a table that
      is not present on this MySQL server and needs re-discovery.
      To perform the action, we need an exclusive metadata lock on
      the table. Acquiring an X lock while holding other shared
      locks is very deadlock-prone. If this is a multi- statement
      transaction that holds metadata locks for completed
      statements, we don't do it, and report an error instead.
      The action type in this case is OT_DISCOVER or OT_REPAIR.
    * Our attempt to acquire an MDL lock lead to a deadlock,
      detected by the MDL deadlock detector. The current
      session was chosen a victim. If this is a multi-statement
      transaction that holds metadata locks taken by completed
      statements, restarting locking for the current statement
      may lead to a livelock. Releasing locks of completed
      statements can not be done as will lead to violation
      of ACID. Thus, again, if m_has_locks is set,
      we report an error. Otherwise, when there are no metadata
      locks other than which belong to this statement, we can
      try to recover from error by releasing all locks and
      restarting the pre-locking.
      Similarly, a deadlock error can occur when the
      pre-locking process met a TABLE_SHARE that is being
      flushed, and unsuccessfully waited for the flush to
      complete. A deadlock in this case can happen, e.g.,
      when our session is holding a metadata lock that
      is being waited on by a session which is using
      the table which is being flushed. The only way
      to recover from this error is, again, to close all
      open tables, release all locks, and retry pre-locking.
      Action type name is OT_REOPEN_TABLES. Re-trying
      while holding some locks may lead to a livelock,
      and thus we don't do it.
    * Finally, this session has open TABLEs from different
      "generations" of the table cache. This can happen, e.g.,
      when, after this session has successfully opened one
      table used for a statement, FLUSH TABLES interfered and
      expelled another table used in it. FLUSH TABLES then
      blocks and waits on the table already opened by this
      statement.
      We detect this situation by ensuring that table cache
      version of all tables used in a statement is the same.
      If it isn't, all tables needs to be reopened.
      Note, that we can always perform a reopen in this case,
      even if we already have metadata locks, since we don't
      keep tables open between statements and a livelock
      is not possible.
  */
  if (action_arg != OT_REOPEN_TABLES && m_has_locks)
  {
    my_error(ER_LOCK_DEADLOCK, MYF(0));
    return TRUE;
  }
  /*
    If auto-repair or discovery are requested, a pointer to table
    list element must be provided.
  */
  if (table)
  {
    DBUG_ASSERT(action_arg == OT_DISCOVER || action_arg == OT_REPAIR);
    m_failed_table= (TABLE_LIST*) current_thd->alloc(sizeof(TABLE_LIST));
    if (m_failed_table == NULL)
      return TRUE;
    m_failed_table->init_one_table(table->db, table->db_length,
                                   table->table_name,
                                   table->table_name_length,
                                   table->alias, TL_WRITE);
    m_failed_table->mdl_request.set_type(MDL_EXCLUSIVE);
  }
  m_action= action_arg;
  return FALSE;
}


/**
   Recover from failed attempt of open table by performing requested action.

   @param  thd     Thread context

   @pre This function should be called only with "action" != OT_NO_ACTION
        and after having called @sa close_tables_for_reopen().

   @retval FALSE - Success. One should try to open tables once again.
   @retval TRUE  - Error
*/

bool
Open_table_context::
recover_from_failed_open(THD *thd)
{
  bool result= FALSE;
  /* Execute the action. */
  switch (m_action)
  {
    case OT_BACKOFF_AND_RETRY:
      break;
    case OT_REOPEN_TABLES:
      break;
    case OT_DISCOVER:
      {
        if ((result= lock_table_names(thd, m_failed_table, NULL,
                                      get_timeout(), 0)))
          break;

        tdc_remove_table(thd, TDC_RT_REMOVE_ALL, m_failed_table->db,
                         m_failed_table->table_name, FALSE);
        ha_create_table_from_engine(thd, m_failed_table->db,
                                    m_failed_table->table_name);

        thd->get_stmt_da()->reset_condition_info(thd->query_id);
        thd->clear_error();                 // Clear error message
        thd->mdl_context.release_transactional_locks();
        break;
      }
    case OT_REPAIR:
      {
        if ((result= lock_table_names(thd, m_failed_table, NULL,
                                      get_timeout(), 0)))
          break;

        tdc_remove_table(thd, TDC_RT_REMOVE_ALL, m_failed_table->db,
                         m_failed_table->table_name, FALSE);

        result= auto_repair_table(thd, m_failed_table);
        thd->mdl_context.release_transactional_locks();
        break;
      }
    default:
      DBUG_ASSERT(0);
  }
  /*
    Reset the pointers to conflicting MDL request and the
    TABLE_LIST element, set when we need auto-discovery or repair,
    for safety.
  */
  m_failed_table= NULL;
  /*
    Reset flag indicating that we have already acquired protection
    against GRL. It is no longer valid as the corresponding lock was
    released by close_tables_for_reopen().
  */
  m_has_protection_against_grl= FALSE;
  /* Prepare for possible another back-off. */
  m_action= OT_NO_ACTION;
  return result;
}


/*
  Return a appropriate read lock type given a table object.

  @param thd              Thread context
  @param prelocking_ctx   Prelocking context.
  @param table_list       Table list element for table to be locked.
  @param routine_modifies_data 
                          Some routine that is invoked by statement 
                          modifies data.

  @remark Due to a statement-based replication limitation, statements such as
          INSERT INTO .. SELECT FROM .. and CREATE TABLE .. SELECT FROM need
          to grab a TL_READ_NO_INSERT lock on the source table in order to
          prevent the replication of a concurrent statement that modifies the
          source table. If such a statement gets applied on the slave before
          the INSERT .. SELECT statement finishes, data on the master could
          differ from data on the slave and end-up with a discrepancy between
          the binary log and table state.
          This also applies to SELECT/SET/DO statements which use stored
          functions. Calls to such functions are going to be logged as a
          whole and thus should be serialized against concurrent changes
          to tables used by those functions. This is avoided when functions
          do not modify data but only read it, since in this case nothing is
          written to the binary log. Argument routine_modifies_data
          denotes the same. So effectively, if the statement is not a
          update query and routine_modifies_data is false, then
          prelocking_placeholder does not take importance.

          Furthermore, this does not apply to I_S and log tables as it's
          always unsafe to replicate such tables under statement-based
          replication as the table on the slave might contain other data
          (ie: general_log is enabled on the slave). The statement will
          be marked as unsafe for SBR in decide_logging_format().
  @remark Note that even in prelocked mode it is important to correctly
          determine lock type value. In this mode lock type is passed to
          handler::start_stmt() method and can be used by storage engine,
          for example, to determine what kind of row locks it should acquire
          when reading data from the table.
*/

thr_lock_type read_lock_type_for_table(THD *thd,
                                       Query_tables_list *prelocking_ctx,
                                       TABLE_LIST *table_list,
                                       bool routine_modifies_data)
{
  /*
    In cases when this function is called for a sub-statement executed in
    prelocked mode we can't rely on OPTION_BIN_LOG flag in THD::options
    bitmap to determine that binary logging is turned on as this bit can
    be cleared before executing sub-statement. So instead we have to look
    at THD::variables::sql_log_bin member.
  */
  bool log_on= mysql_bin_log.is_open() && thd->variables.sql_log_bin;
  ulong binlog_format= thd->variables.binlog_format;
  if ((log_on == FALSE) || (binlog_format == BINLOG_FORMAT_ROW) ||
      (table_list->table->s->table_category == TABLE_CATEGORY_LOG) ||
      (table_list->table->s->table_category == TABLE_CATEGORY_RPL_INFO) ||
      (table_list->table->s->table_category == TABLE_CATEGORY_PERFORMANCE) ||
      !(is_update_query(prelocking_ctx->sql_command) ||
        (routine_modifies_data && table_list->prelocking_placeholder) ||
        (thd->locked_tables_mode > LTM_LOCK_TABLES)))
    return TL_READ;
  else
    return TL_READ_NO_INSERT;
}


/*
  Handle element of prelocking set other than table. E.g. cache routine
  and, if prelocking strategy prescribes so, extend the prelocking set
  with tables and routines used by it.

  @param[in]  thd                   Thread context.
  @param[in]  prelocking_ctx        Prelocking context.
  @param[in]  rt                    Element of prelocking set to be processed.
  @param[in]  prelocking_strategy   Strategy which specifies how the
                                    prelocking set should be extended when
                                    one of its elements is processed.
  @param[in]  has_prelocking_list   Indicates that prelocking set/list for
                                    this statement has already been built.
  @param[in]  ot_ctx                Context of open_table used to recover from
                                    locking failures.
  @param[out] need_prelocking       Set to TRUE if it was detected that this
                                    statement will require prelocked mode for
                                    its execution, not touched otherwise.
  @param[out] routine_modifies_data Set to TRUE if it was detected that this
                                    routine does modify table data.

  @retval FALSE  Success.
  @retval TRUE   Failure (Conflicting metadata lock, OOM, other errors).
*/

static bool
open_and_process_routine(THD *thd, Query_tables_list *prelocking_ctx,
                         Sroutine_hash_entry *rt,
                         Prelocking_strategy *prelocking_strategy,
                         bool has_prelocking_list,
                         Open_table_context *ot_ctx,
                         bool *need_prelocking, bool *routine_modifies_data)
{
  MDL_key::enum_mdl_namespace mdl_type= rt->mdl_request.key.mdl_namespace();
  *routine_modifies_data= false;
  DBUG_ENTER("open_and_process_routine");

  switch (mdl_type)
  {
  case MDL_key::FUNCTION:
  case MDL_key::PROCEDURE:
    {
      sp_head *sp;
      /*
        Try to get MDL lock on the routine.
        Note that we do not take locks on top-level CALLs as this can
        lead to a deadlock. Not locking top-level CALLs does not break
        the binlog as only the statements in the called procedure show
        up there, not the CALL itself.
      */
      if (rt != (Sroutine_hash_entry*)prelocking_ctx->sroutines_list.first ||
          mdl_type != MDL_key::PROCEDURE)
      {
        /*
          Since we acquire only shared lock on routines we don't
          need to care about global intention exclusive locks.
        */
        DBUG_ASSERT(rt->mdl_request.type == MDL_SHARED);

        /*
          Waiting for a conflicting metadata lock to go away may
          lead to a deadlock, detected by MDL subsystem.
          If possible, we try to resolve such deadlocks by releasing all
          metadata locks and restarting the pre-locking process.
          To prevent the error from polluting the Diagnostics Area
          in case of successful resolution, install a special error
          handler for ER_LOCK_DEADLOCK error.
        */
        MDL_deadlock_handler mdl_deadlock_handler(ot_ctx);

        thd->push_internal_handler(&mdl_deadlock_handler);
        bool result= thd->mdl_context.acquire_lock(&rt->mdl_request,
                                                   ot_ctx->get_timeout());
        thd->pop_internal_handler();

        if (result)
          DBUG_RETURN(TRUE);

        DEBUG_SYNC(thd, "after_shared_lock_pname");

        /* Ensures the routine is up-to-date and cached, if exists. */
        if (sp_cache_routine(thd, rt, has_prelocking_list, &sp))
          DBUG_RETURN(TRUE);

        /* Remember the version of the routine in the parse tree. */
        if (check_and_update_routine_version(thd, rt, sp))
          DBUG_RETURN(TRUE);

        /* 'sp' is NULL when there is no such routine. */
        if (sp)
        {
          *routine_modifies_data= sp->modifies_data();

          if (!has_prelocking_list)
            prelocking_strategy->handle_routine(thd, prelocking_ctx, rt, sp,
                                                need_prelocking);
        }
      }
      else
      {
        /*
          If it's a top level call, just make sure we have a recent
          version of the routine, if it exists.
          Validating routine version is unnecessary, since CALL
          does not affect the prepared statement prelocked list.
        */
        if (sp_cache_routine(thd, rt, FALSE, &sp))
          DBUG_RETURN(TRUE);
      }
    }
    break;
  case MDL_key::TRIGGER:
    /**
      We add trigger entries to lex->sroutines_list, but we don't
      load them here. The trigger entry is only used when building
      a transitive closure of objects used in a statement, to avoid
      adding to this closure objects that are used in the trigger more
      than once.
      E.g. if a trigger trg refers to table t2, and the trigger table t1
      is used multiple times in the statement (say, because it's used in
      function f1() twice), we will only add t2 once to the list of
      tables to prelock.

      We don't take metadata locks on triggers either: they are protected
      by a respective lock on the table, on which the trigger is defined.

      The only two cases which give "trouble" are SHOW CREATE TRIGGER
      and DROP TRIGGER statements. For these, statement syntax doesn't
      specify the table on which this trigger is defined, so we have
      to make a "dirty" read in the data dictionary to find out the
      table name. Once we discover the table name, we take a metadata
      lock on it, and this protects all trigger operations.
      Of course the table, in theory, may disappear between the dirty
      read and metadata lock acquisition, but in that case we just return
      a run-time error.

      Grammar of other trigger DDL statements (CREATE, DROP) requires
      the table to be specified explicitly, so we use the table metadata
      lock to protect trigger metadata in these statements. Similarly, in
      DML we always use triggers together with their tables, and thus don't
      need to take separate metadata locks on them.
    */
    break;
  default:
    /* Impossible type value. */
    DBUG_ASSERT(0);
  }
  DBUG_RETURN(FALSE);
}


/**
  Handle table list element by obtaining metadata lock, opening table or view
  and, if prelocking strategy prescribes so, extending the prelocking set with
  tables and routines used by it.

  @param[in]     thd                  Thread context.
  @param[in]     lex                  LEX structure for statement.
  @param[in]     tables               Table list element to be processed.
  @param[in,out] counter              Number of tables which are open.
  @param[in]     flags                Bitmap of flags to modify how the tables
                                      will be open, see open_table() description
                                      for details.
  @param[in]     prelocking_strategy  Strategy which specifies how the
                                      prelocking set should be extended
                                      when table or view is processed.
  @param[in]     has_prelocking_list  Indicates that prelocking set/list for
                                      this statement has already been built.
  @param[in]     ot_ctx               Context used to recover from a failed
                                      open_table() attempt.

  @retval  FALSE  Success.
  @retval  TRUE   Error, reported unless there is a chance to recover from it.
*/

static bool
open_and_process_table(THD *thd, LEX *lex, TABLE_LIST *tables,
                       uint *counter, uint flags,
                       Prelocking_strategy *prelocking_strategy,
                       bool has_prelocking_list,
                       Open_table_context *ot_ctx)
{
  bool error= FALSE;
  bool safe_to_ignore_table= FALSE;
  DBUG_ENTER("open_and_process_table");
  DEBUG_SYNC(thd, "open_and_process_table");

  /*
    Ignore placeholders for derived tables. After derived tables
    processing, link to created temporary table will be put here.
    If this is derived table for view then we still want to process
    routines used by this view.
  */
  if (tables->derived)
  {
    if (!tables->view)
      goto end;
    /*
      We restore view's name and database wiped out by derived tables
      processing and fall back to standard open process in order to
      obtain proper metadata locks and do other necessary steps like
      stored routine processing.
    */
    tables->db= tables->view_db.str;
    tables->db_length= tables->view_db.length;
    tables->table_name= tables->view_name.str;
    tables->table_name_length= tables->view_name.length;
  }
  /*
    If this TABLE_LIST object is a placeholder for an information_schema
    table, create a temporary table to represent the information_schema
    table in the query. Do not fill it yet - will be filled during
    execution.
  */
  if (tables->schema_table)
  {
    /*
      If this information_schema table is merged into a mergeable
      view, ignore it for now -- it will be filled when its respective
      TABLE_LIST is processed. This code works only during re-execution.
    */
    if (tables->view)
    {
      MDL_ticket *mdl_ticket;
      /*
        We still need to take a MDL lock on the merged view to protect
        it from concurrent changes.
      */
      if (!open_table_get_mdl_lock(thd, ot_ctx, &tables->mdl_request,
                                   flags, &mdl_ticket) &&
          mdl_ticket != NULL)
        goto process_view_routines;
      /* Fall-through to return error. */
    }
    else if (!mysql_schema_table(thd, lex, tables) &&
             !check_and_update_table_version(thd, tables, tables->table->s))
    {
      goto end;
    }
    error= TRUE;
    goto end;
  }
  DBUG_PRINT("tcache", ("opening table: '%s'.'%s'  item: %p",
                        tables->db, tables->table_name, tables)); //psergey: invalid read of size 1 here
  (*counter)++;

  /* Not a placeholder: must be a base/temporary table or a view. Let us open it. */

  if (tables->table)
  {
    /*
      If this TABLE_LIST object has an associated open TABLE object
      (TABLE_LIST::table is not NULL), that TABLE object must be a pre-opened
      temporary table.
    */
    DBUG_ASSERT(is_temporary_table(tables));
  }
  else if (tables->open_type == OT_TEMPORARY_ONLY)
  {
    /*
      OT_TEMPORARY_ONLY means that we are in CREATE TEMPORARY TABLE statement.
      Also such table list element can't correspond to prelocking placeholder
      or to underlying table of merge table.
      So existing temporary table should have been preopened by this moment
      and we can simply continue without trying to open temporary or base
      table.
    */
    DBUG_ASSERT(tables->open_strategy);
    DBUG_ASSERT(!tables->prelocking_placeholder);
    DBUG_ASSERT(!tables->parent_l);
  }
  else if (tables->prelocking_placeholder)
  {
    /*
      For the tables added by the pre-locking code, attempt to open
      the table but fail silently if the table does not exist.
      The real failure will occur when/if a statement attempts to use
      that table.
    */
    No_such_table_error_handler no_such_table_handler;
    thd->push_internal_handler(&no_such_table_handler);

    /*
      We're opening a table from the prelocking list.

      Since this table list element might have been added after pre-opening
      of temporary tables we have to try to open temporary table for it.

      We can't simply skip this table list element and postpone opening of
      temporary tabletill the execution of substatement for several reasons:
      - Temporary table can be a MERGE table with base underlying tables,
        so its underlying tables has to be properly open and locked at
        prelocking stage.
      - Temporary table can be a MERGE table and we might be in PREPARE
        phase for a prepared statement. In this case it is important to call
        HA_ATTACH_CHILDREN for all merge children.
        This is necessary because merge children remember "TABLE_SHARE ref type"
        and "TABLE_SHARE def version" in the HA_ATTACH_CHILDREN operation.
        If HA_ATTACH_CHILDREN is not called, these attributes are not set.
        Then, during the first EXECUTE, those attributes need to be updated.
        That would cause statement re-preparing (because changing those
        attributes during EXECUTE is caught by THD::m_reprepare_observers).
        The problem is that since those attributes are not set in merge
        children, another round of PREPARE will not help.
    */
    error= open_temporary_table(thd, tables);

    if (!error && !tables->table)
      error= open_table(thd, tables, ot_ctx);

    thd->pop_internal_handler();
    safe_to_ignore_table= no_such_table_handler.safely_trapped_errors();
  }
  else if (tables->parent_l && (thd->open_options & HA_OPEN_FOR_REPAIR))
  {
    /*
      Also fail silently for underlying tables of a MERGE table if this
      table is opened for CHECK/REPAIR TABLE statement. This is needed
      to provide complete list of problematic underlying tables in
      CHECK/REPAIR TABLE output.
    */
    Repair_mrg_table_error_handler repair_mrg_table_handler;
    thd->push_internal_handler(&repair_mrg_table_handler);

    error= open_temporary_table(thd, tables);
    if (!error && !tables->table)
      error= open_table(thd, tables, ot_ctx);

    thd->pop_internal_handler();
    safe_to_ignore_table= repair_mrg_table_handler.safely_trapped_errors();
  }
  else
  {
    if (tables->parent_l)
    {
      /*
        Even if we are opening table not from the prelocking list we
        still might need to look for a temporary table if this table
        list element corresponds to underlying table of a merge table.
      */
      error= open_temporary_table(thd, tables);
    }

    if (!error && !tables->table)
      error= open_table(thd, tables, ot_ctx);
  }

  if (error)
  {
    if (! ot_ctx->can_recover_from_failed_open() && safe_to_ignore_table)
    {
      DBUG_PRINT("info", ("open_table: ignoring table '%s'.'%s'",
                          tables->db, tables->alias));
      error= FALSE;
    }
    goto end;
  }

  /*
    We can't rely on simple check for TABLE_LIST::view to determine
    that this is a view since during re-execution we might reopen
    ordinary table in place of view and thus have TABLE_LIST::view
    set from repvious execution and TABLE_LIST::table set from
    current.
  */
  if (!tables->table && tables->view)
  {
    /* VIEW placeholder */
    (*counter)--;

    /*
      tables->next_global list consists of two parts:
      1) Query tables and underlying tables of views.
      2) Tables used by all stored routines that this statement invokes on
         execution.
      We need to know where the bound between these two parts is. If we've
      just opened a view, which was the last table in part #1, and it
      has added its base tables after itself, adjust the boundary pointer
      accordingly.
    */
    if (lex->query_tables_own_last == &(tables->next_global) &&
        tables->view->query_tables)
      lex->query_tables_own_last= tables->view->query_tables_last;
    /*
      Let us free memory used by 'sroutines' hash here since we never
      call destructor for this LEX.
    */
    my_hash_free(&tables->view->sroutines);
    goto process_view_routines;
  }

  /*
    Special types of open can succeed but still don't set
    TABLE_LIST::table to anything.
  */
  if (tables->open_strategy && !tables->table)
    goto end;

  /*
    If we are not already in prelocked mode and extended table list is not
    yet built we might have to build the prelocking set for this statement.

    Since currently no prelocking strategy prescribes doing anything for
    tables which are only read, we do below checks only if table is going
    to be changed.
  */
  if (thd->locked_tables_mode <= LTM_LOCK_TABLES &&
      ! has_prelocking_list &&
      tables->lock_type >= TL_WRITE_ALLOW_WRITE)
  {
    bool need_prelocking= FALSE;
    TABLE_LIST **save_query_tables_last= lex->query_tables_last;
    /*
      Extend statement's table list and the prelocking set with
      tables and routines according to the current prelocking
      strategy.

      For example, for DML statements we need to add tables and routines
      used by triggers which are going to be invoked for this element of
      table list and also add tables required for handling of foreign keys.
    */
    error= prelocking_strategy->handle_table(thd, lex, tables,
                                             &need_prelocking);

    if (need_prelocking && ! lex->requires_prelocking())
      lex->mark_as_requiring_prelocking(save_query_tables_last);

    if (error)
      goto end;
  }

  /* Copy grant information from TABLE_LIST instance to TABLE one. */
  tables->table->grant= tables->grant;

  /* Check and update metadata version of a base table. */
  error= check_and_update_table_version(thd, tables, tables->table->s);

  if (error)
    goto end;
  /*
    After opening a MERGE table add the children to the query list of
    tables, so that they are opened too.
    Note that placeholders don't have the handler open.
  */
  /* MERGE tables need to access parent and child TABLE_LISTs. */
  DBUG_ASSERT(tables->table->pos_in_table_list == tables);
  /* Non-MERGE tables ignore this call. */
  if (tables->table->file->extra(HA_EXTRA_ADD_CHILDREN_LIST))
  {
    error= TRUE;
    goto end;
  }

process_view_routines:
  /*
    Again we may need cache all routines used by this view and add
    tables used by them to table list.
  */
  if (tables->view &&
      thd->locked_tables_mode <= LTM_LOCK_TABLES &&
      ! has_prelocking_list)
  {
    bool need_prelocking= FALSE;
    TABLE_LIST **save_query_tables_last= lex->query_tables_last;

    error= prelocking_strategy->handle_view(thd, lex, tables,
                                            &need_prelocking);

    if (need_prelocking && ! lex->requires_prelocking())
      lex->mark_as_requiring_prelocking(save_query_tables_last);

    if (error)
      goto end;
  }

end:
  DBUG_RETURN(error);
}

extern "C" uchar *schema_set_get_key(const uchar *record, size_t *length,
                                     my_bool not_used __attribute__((unused)))
{
  TABLE_LIST *table=(TABLE_LIST*) record;
  *length= table->db_length;
  return (uchar*) table->db;
}

/**
  Acquire upgradable (SNW, SNRW) metadata locks on tables used by
  LOCK TABLES or by a DDL statement. Under LOCK TABLES, we can't take
  new locks, so use open_tables_check_upgradable_mdl() instead.

  @param thd               Thread context.
  @param tables_start      Start of list of tables on which upgradable locks
                           should be acquired.
  @param tables_end        End of list of tables.
  @param lock_wait_timeout Seconds to wait before timeout.
  @param flags             Bitmap of flags to modify how the tables will be
                           open, see open_table() description for details.

  @retval FALSE  Success.
  @retval TRUE   Failure (e.g. connection was killed)
*/

bool
lock_table_names(THD *thd,
                 TABLE_LIST *tables_start, TABLE_LIST *tables_end,
                 ulong lock_wait_timeout, uint flags)
{
  MDL_request_list mdl_requests;
  TABLE_LIST *table;
  MDL_request global_request;
  Hash_set<TABLE_LIST, schema_set_get_key> schema_set;

  DBUG_ASSERT(!thd->locked_tables_mode);

  for (table= tables_start; table && table != tables_end;
       table= table->next_global)
  {
    if (table->mdl_request.type < MDL_SHARED_UPGRADABLE ||
        table->open_type == OT_TEMPORARY_ONLY ||
        (table->open_type == OT_TEMPORARY_OR_BASE && is_temporary_table(table)))
    {
      continue;
    }

    /* Write lock on normal tables is not allowed in a read only transaction. */
    if (thd->tx_read_only)
    {
      my_error(ER_CANT_EXECUTE_IN_READ_ONLY_TRANSACTION, MYF(0));
      return true;
    }

    if (! (flags & MYSQL_OPEN_SKIP_SCOPED_MDL_LOCK) && schema_set.insert(table))
      return TRUE;

    mdl_requests.push_front(&table->mdl_request);
  }

  if (! (flags & MYSQL_OPEN_SKIP_SCOPED_MDL_LOCK) &&
      ! mdl_requests.is_empty())
  {
    /*
      Scoped locks: Take intention exclusive locks on all involved
      schemas.
    */
    Hash_set<TABLE_LIST, schema_set_get_key>::Iterator it(schema_set);
    while ((table= it++))
    {
      MDL_request *schema_request= new (thd->mem_root) MDL_request;
      if (schema_request == NULL)
        return TRUE;
      schema_request->init(MDL_key::SCHEMA, table->db, "",
                           MDL_INTENTION_EXCLUSIVE,
                           MDL_TRANSACTION);
      mdl_requests.push_front(schema_request);
    }

    /*
      Protect this statement against concurrent global read lock
      by acquiring global intention exclusive lock with statement
      duration.
    */
    if (thd->global_read_lock.can_acquire_protection())
      return TRUE;
    global_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                        MDL_STATEMENT);
    mdl_requests.push_front(&global_request);
  }

  if (thd->mdl_context.acquire_locks(&mdl_requests, lock_wait_timeout))
    return TRUE;

  return FALSE;
}


/**
  Check for upgradable (SNW, SNRW) metadata locks on tables to be opened
  for a DDL statement. Under LOCK TABLES, we can't take new locks, so we
  must check if appropriate locks were pre-acquired.

  @param thd           Thread context.
  @param tables_start  Start of list of tables on which upgradable locks
                       should be searched for.
  @param tables_end    End of list of tables.
  @param flags         Bitmap of flags to modify how the tables will be
                       open, see open_table() description for details.

  @retval FALSE  Success.
  @retval TRUE   Failure (e.g. connection was killed)
*/

static bool
open_tables_check_upgradable_mdl(THD *thd, TABLE_LIST *tables_start,
                                 TABLE_LIST *tables_end, uint flags)
{
  TABLE_LIST *table;

  DBUG_ASSERT(thd->locked_tables_mode);

  for (table= tables_start; table && table != tables_end;
       table= table->next_global)
  {
    if (table->mdl_request.type < MDL_SHARED_UPGRADABLE ||
        table->open_type == OT_TEMPORARY_ONLY ||
        (table->open_type == OT_TEMPORARY_OR_BASE && is_temporary_table(table)))
    {
      continue;
    }

    /*
      We don't need to do anything about the found TABLE instance as it
      will be handled later in open_tables(), we only need to check that
      an upgradable lock is already acquired. When we enter LOCK TABLES
      mode, SNRW locks are acquired before all other locks. So if under
      LOCK TABLES we find that there is TABLE instance with upgradeable
      lock, all other instances of TABLE for the same table will have the
      same ticket.

      Note that this works OK even for CREATE TABLE statements which
      request X type of metadata lock. This is because under LOCK TABLES
      such statements don't create the table but only check if it exists
      or, in most complex case, only insert into it.
      Thus SNRW lock should be enough.

      Note that find_table_for_mdl_upgrade() will report an error if
      no suitable ticket is found.
    */
    if (!find_table_for_mdl_upgrade(thd, table->db, table->table_name, false))
      return TRUE;
  }

  return FALSE;
}


/**
  Open all tables in list

  @param[in]     thd      Thread context.
  @param[in,out] start    List of tables to be open (it can be adjusted for
                          statement that uses tables only implicitly, e.g.
                          for "SELECT f1()").
  @param[out]    counter  Number of tables which were open.
  @param[in]     flags    Bitmap of flags to modify how the tables will be
                          open, see open_table() description for details.
  @param[in]     prelocking_strategy  Strategy which specifies how prelocking
                                      algorithm should work for this statement.

  @note
    Unless we are already in prelocked mode and prelocking strategy prescribes
    so this function will also precache all SP/SFs explicitly or implicitly
    (via views and triggers) used by the query and add tables needed for their
    execution to table list. Statement that uses SFs, invokes triggers or
    requires foreign key checks will be marked as requiring prelocking.
    Prelocked mode will be enabled for such query during lock_tables() call.

    If query for which we are opening tables is already marked as requiring
    prelocking it won't do such precaching and will simply reuse table list
    which is already built.

  @retval  FALSE  Success.
  @retval  TRUE   Error, reported.
*/

bool open_tables(THD *thd, TABLE_LIST **start, uint *counter, uint flags,
                Prelocking_strategy *prelocking_strategy)
{
  /*
    We use pointers to "next_global" member in the last processed TABLE_LIST
    element and to the "next" member in the last processed Sroutine_hash_entry
    element as iterators over, correspondingly, the table list and stored routines
    list which stay valid and allow to continue iteration when new elements are
    added to the tail of the lists.
  */
  TABLE_LIST **table_to_open;
  Sroutine_hash_entry **sroutine_to_open;
  TABLE_LIST *tables;
  Open_table_context ot_ctx(thd, flags);
  bool error= FALSE;
  bool some_routine_modifies_data= FALSE;
  bool has_prelocking_list;
  DBUG_ENTER("open_tables");

  /* Accessing data in XA_IDLE or XA_PREPARED is not allowed. */
  enum xa_states xa_state= thd->transaction.xid_state.xa_state;
  if (*start && (xa_state == XA_IDLE || xa_state == XA_PREPARED))
  {
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[xa_state]);
    DBUG_RETURN(true);
  }

  thd->current_tablenr= 0;
restart:
  /*
    Close HANDLER tables which are marked for flush or against which there
    are pending exclusive metadata locks. This is needed both in order to
    avoid deadlocks and to have a point during statement execution at
    which such HANDLERs are closed even if they don't create problems for
    the current session (i.e. to avoid having a DDL blocked by HANDLERs
    opened for a long time).
  */
  if (thd->handler_tables_hash.records)
    mysql_ha_flush(thd);

  has_prelocking_list= thd->lex->requires_prelocking();
  table_to_open= start;
  sroutine_to_open= (Sroutine_hash_entry**) &thd->lex->sroutines_list.first;
  *counter= 0;
  THD_STAGE_INFO(thd, stage_opening_tables);

  /*
    If we are executing LOCK TABLES statement or a DDL statement
    (in non-LOCK TABLES mode) we might have to acquire upgradable
    semi-exclusive metadata locks (SNW or SNRW) on some of the
    tables to be opened.
    When executing CREATE TABLE .. If NOT EXISTS .. SELECT, the
    table may not yet exist, in which case we acquire an exclusive
    lock.
    We acquire all such locks at once here as doing this in one
    by one fashion may lead to deadlocks or starvation. Later when
    we will be opening corresponding table pre-acquired metadata
    lock will be reused (thanks to the fact that in recursive case
    metadata locks are acquired without waiting).
  */
  if (! (flags & (MYSQL_OPEN_HAS_MDL_LOCK |
                  MYSQL_OPEN_FORCE_SHARED_MDL |
                  MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL)))
  {
    if (thd->locked_tables_mode)
    {
      /*
        Under LOCK TABLES, we can't acquire new locks, so we instead
        need to check if appropriate locks were pre-acquired.
      */
      if (open_tables_check_upgradable_mdl(thd, *start,
                                           thd->lex->first_not_own_table(),
                                           flags))
      {
        error= TRUE;
        goto err;
      }
    }
    else
    {
      TABLE_LIST *table;
      if (lock_table_names(thd, *start, thd->lex->first_not_own_table(),
                           ot_ctx.get_timeout(), flags))
      {
        error= TRUE;
        goto err;
      }
      for (table= *start; table && table != thd->lex->first_not_own_table();
           table= table->next_global)
      {
        if (table->mdl_request.type >= MDL_SHARED_UPGRADABLE)
          table->mdl_request.ticket= NULL;
      }
    }
  }

  /*
    Perform steps of prelocking algorithm until there are unprocessed
    elements in prelocking list/set.
  */
  while (*table_to_open  ||
         (thd->locked_tables_mode <= LTM_LOCK_TABLES &&
          *sroutine_to_open))
  {
    /*
      For every table in the list of tables to open, try to find or open
      a table.
    */
    for (tables= *table_to_open; tables;
         table_to_open= &tables->next_global, tables= tables->next_global)
    {
      error= open_and_process_table(thd, thd->lex, tables, counter,
                                    flags, prelocking_strategy,
                                    has_prelocking_list, &ot_ctx);

      if (error)
      {
        if (ot_ctx.can_recover_from_failed_open())
        {
          /*
            We have met exclusive metadata lock or old version of table.
            Now we have to close all tables and release metadata locks.
            We also have to throw away set of prelocked tables (and thus
            close tables from this set that were open by now) since it
            is possible that one of tables which determined its content
            was changed.

            Instead of implementing complex/non-robust logic mentioned
            above we simply close and then reopen all tables.

            We have to save pointer to table list element for table which we
            have failed to open since closing tables can trigger removal of
            elements from the table list (if MERGE tables are involved),
          */
          close_tables_for_reopen(thd, start, ot_ctx.start_of_statement_svp());

          /*
            Here we rely on the fact that 'tables' still points to the valid
            TABLE_LIST element. Altough currently this assumption is valid
            it may change in future.
          */
          if (ot_ctx.recover_from_failed_open(thd))
            goto err;

          /* Re-open temporary tables after close_tables_for_reopen(). */
          if (open_temporary_tables(thd, *start))
            goto err;

          error= FALSE;
          goto restart;
        }
        goto err;
      }

      DEBUG_SYNC(thd, "open_tables_after_open_and_process_table");
    }

    /*
      If we are not already in prelocked mode and extended table list is
      not yet built for our statement we need to cache routines it uses
      and build the prelocking list for it.
      If we are not in prelocked mode but have built the extended table
      list, we still need to call open_and_process_routine() to take
      MDL locks on the routines.
    */
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
    {
      bool routine_modifies_data;
      /*
        Process elements of the prelocking set which are present there
        since parsing stage or were added to it by invocations of
        Prelocking_strategy methods in the above loop over tables.

        For example, if element is a routine, cache it and then,
        if prelocking strategy prescribes so, add tables it uses to the
        table list and routines it might invoke to the prelocking set.
      */
      for (Sroutine_hash_entry *rt= *sroutine_to_open; rt;
           sroutine_to_open= &rt->next, rt= rt->next)
      {
        bool need_prelocking= false;
        TABLE_LIST **save_query_tables_last= thd->lex->query_tables_last;

        error= open_and_process_routine(thd, thd->lex, rt, prelocking_strategy,
                                        has_prelocking_list, &ot_ctx,
                                        &need_prelocking,
                                        &routine_modifies_data);


        if (need_prelocking && ! thd->lex->requires_prelocking())
          thd->lex->mark_as_requiring_prelocking(save_query_tables_last);

        if (need_prelocking && ! *start)
          *start= thd->lex->query_tables;

        if (error)
        {
          if (ot_ctx.can_recover_from_failed_open())
          {
            close_tables_for_reopen(thd, start,
                                    ot_ctx.start_of_statement_svp());
            if (ot_ctx.recover_from_failed_open(thd))
              goto err;

            /* Re-open temporary tables after close_tables_for_reopen(). */
            if (open_temporary_tables(thd, *start))
              goto err;

            error= FALSE;
            goto restart;
          }
          /*
            Serious error during reading stored routines from mysql.proc table.
            Something is wrong with the table or its contents, and an error has
            been emitted; we must abort.
          */
          goto err;
        }

        // Remember if any of SF modifies data.
        some_routine_modifies_data|= routine_modifies_data;
      }
    }
  }

  /*
    After successful open of all tables, including MERGE parents and
    children, attach the children to their parents. At end of statement,
    the children are detached. Attaching and detaching are always done,
    even under LOCK TABLES.

    We also convert all TL_WRITE_DEFAULT and TL_READ_DEFAULT locks to
    appropriate "real" lock types to be used for locking and to be passed
    to storage engine.
  */
  for (tables= *start; tables; tables= tables->next_global)
  {
    TABLE *tbl= tables->table;

    /*
      NOTE: temporary merge tables should be processed here too, because
      a temporary merge table can be based on non-temporary tables.
    */

    /* Schema tables may not have a TABLE object here. */
    if (tbl && tbl->file->ht->db_type == DB_TYPE_MRG_MYISAM)
    {
      /* MERGE tables need to access parent and child TABLE_LISTs. */
      DBUG_ASSERT(tbl->pos_in_table_list == tables);
      if (tbl->file->extra(HA_EXTRA_ATTACH_CHILDREN))
      {
        error= TRUE;
        goto err;
      }
    }

    /* Set appropriate TABLE::lock_type. */
    if (tbl && tables->lock_type != TL_UNLOCK && 
        !thd->locked_tables_mode)
    {
      if (tables->lock_type == TL_WRITE_DEFAULT)
        tbl->reginfo.lock_type= thd->update_lock_default;
      else if (tables->lock_type == TL_WRITE_CONCURRENT_DEFAULT)
        tables->table->reginfo.lock_type= thd->insert_lock_default;
      else if (tables->lock_type == TL_READ_DEFAULT)
          tbl->reginfo.lock_type=
            read_lock_type_for_table(thd, thd->lex, tables,
                                     some_routine_modifies_data);
      else
        tbl->reginfo.lock_type= tables->lock_type;
    }

  }

err:
  if (error && *table_to_open)
  {
    (*table_to_open)->table= NULL;
  }
  DBUG_PRINT("open_tables", ("returning: %d", (int) error));
  DBUG_RETURN(error);
}


/**
  Defines how prelocking algorithm for DML statements should handle routines:
  - For CALL statements we do unrolling (i.e. open and lock tables for each
    sub-statement individually). So for such statements prelocking is enabled
    only if stored functions are used in parameter list and only for period
    during which we calculate values of parameters. Thus in this strategy we
    ignore procedure which is directly called by such statement and extend
    the prelocking set only with tables/functions used by SF called from the
    parameter list.
  - For any other statement any routine which is directly or indirectly called
    by statement is going to be executed in prelocked mode. So in this case we
    simply add all tables and routines used by it to the prelocking set.

  @param[in]  thd              Thread context.
  @param[in]  prelocking_ctx   Prelocking context of the statement.
  @param[in]  rt               Prelocking set element describing routine.
  @param[in]  sp               Routine body.
  @param[out] need_prelocking  Set to TRUE if method detects that prelocking
                               required, not changed otherwise.

  @retval FALSE  Success.
  @retval TRUE   Failure (OOM).
*/

bool DML_prelocking_strategy::
handle_routine(THD *thd, Query_tables_list *prelocking_ctx,
               Sroutine_hash_entry *rt, sp_head *sp, bool *need_prelocking)
{
  /*
    We assume that for any "CALL proc(...)" statement sroutines_list will
    have 'proc' as first element (it may have several, consider e.g.
    "proc(sp_func(...)))". This property is currently guaranted by the
    parser.
  */

  if (rt != (Sroutine_hash_entry*)prelocking_ctx->sroutines_list.first ||
      rt->mdl_request.key.mdl_namespace() != MDL_key::PROCEDURE)
  {
    *need_prelocking= TRUE;
    sp_update_stmt_used_routines(thd, prelocking_ctx, &sp->m_sroutines,
                                 rt->belong_to_view);
    (void)sp->add_used_tables_to_table_list(thd,
                                            &prelocking_ctx->query_tables_last,
                                            rt->belong_to_view);
  }
  sp->propagate_attributes(prelocking_ctx);
  return FALSE;
}


/**
  Defines how prelocking algorithm for DML statements should handle table list
  elements:
  - If table has triggers we should add all tables and routines
    used by them to the prelocking set.

  We do not need to acquire metadata locks on trigger names
  in DML statements, since all DDL statements
  that change trigger metadata always lock their
  subject tables.

  @param[in]  thd              Thread context.
  @param[in]  prelocking_ctx   Prelocking context of the statement.
  @param[in]  table_list       Table list element for table.
  @param[in]  sp               Routine body.
  @param[out] need_prelocking  Set to TRUE if method detects that prelocking
                               required, not changed otherwise.

  @retval FALSE  Success.
  @retval TRUE   Failure (OOM).
*/

bool DML_prelocking_strategy::
handle_table(THD *thd, Query_tables_list *prelocking_ctx,
             TABLE_LIST *table_list, bool *need_prelocking)
{
  /* We rely on a caller to check that table is going to be changed. */
  DBUG_ASSERT(table_list->lock_type >= TL_WRITE_ALLOW_WRITE);

  if (table_list->trg_event_map)
  {
    if (table_list->table->triggers)
    {
      *need_prelocking= TRUE;

      if (table_list->table->triggers->
          add_tables_and_routines_for_triggers(thd, prelocking_ctx, table_list))
        return TRUE;
    }
  }

  return FALSE;
}


/**
  Defines how prelocking algorithm for DML statements should handle view -
  all view routines should be added to the prelocking set.

  @param[in]  thd              Thread context.
  @param[in]  prelocking_ctx   Prelocking context of the statement.
  @param[in]  table_list       Table list element for view.
  @param[in]  sp               Routine body.
  @param[out] need_prelocking  Set to TRUE if method detects that prelocking
                               required, not changed otherwise.

  @retval FALSE  Success.
  @retval TRUE   Failure (OOM).
*/

bool DML_prelocking_strategy::
handle_view(THD *thd, Query_tables_list *prelocking_ctx,
            TABLE_LIST *table_list, bool *need_prelocking)
{
  if (table_list->view->uses_stored_routines())
  {
    *need_prelocking= TRUE;

    sp_update_stmt_used_routines(thd, prelocking_ctx,
                                 &table_list->view->sroutines_list,
                                 table_list->top_table());
  }
  return FALSE;
}


/**
  Defines how prelocking algorithm for LOCK TABLES statement should handle
  table list elements.

  @param[in]  thd              Thread context.
  @param[in]  prelocking_ctx   Prelocking context of the statement.
  @param[in]  table_list       Table list element for table.
  @param[in]  sp               Routine body.
  @param[out] need_prelocking  Set to TRUE if method detects that prelocking
                               required, not changed otherwise.

  @retval FALSE  Success.
  @retval TRUE   Failure (OOM).
*/

bool Lock_tables_prelocking_strategy::
handle_table(THD *thd, Query_tables_list *prelocking_ctx,
             TABLE_LIST *table_list, bool *need_prelocking)
{
  if (DML_prelocking_strategy::handle_table(thd, prelocking_ctx, table_list,
                                            need_prelocking))
    return TRUE;

  /* We rely on a caller to check that table is going to be changed. */
  DBUG_ASSERT(table_list->lock_type >= TL_WRITE_ALLOW_WRITE);

  return FALSE;
}


/**
  Defines how prelocking algorithm for ALTER TABLE statement should handle
  routines - do nothing as this statement is not supposed to call routines.

  We still can end up in this method when someone tries
  to define a foreign key referencing a view, and not just
  a simple view, but one that uses stored routines.
*/

bool Alter_table_prelocking_strategy::
handle_routine(THD *thd, Query_tables_list *prelocking_ctx,
               Sroutine_hash_entry *rt, sp_head *sp, bool *need_prelocking)
{
  return FALSE;
}


/**
  Defines how prelocking algorithm for ALTER TABLE statement should handle
  table list elements.

  Unlike in DML, we do not process triggers here.

  @param[in]  thd              Thread context.
  @param[in]  prelocking_ctx   Prelocking context of the statement.
  @param[in]  table_list       Table list element for table.
  @param[in]  sp               Routine body.
  @param[out] need_prelocking  Set to TRUE if method detects that prelocking
                               required, not changed otherwise.


  @retval FALSE  Success.
  @retval TRUE   Failure (OOM).
*/

bool Alter_table_prelocking_strategy::
handle_table(THD *thd, Query_tables_list *prelocking_ctx,
             TABLE_LIST *table_list, bool *need_prelocking)
{
  return FALSE;
}


/**
  Defines how prelocking algorithm for ALTER TABLE statement
  should handle view - do nothing. We don't need to add view
  routines to the prelocking set in this case as view is not going
  to be materialized.
*/

bool Alter_table_prelocking_strategy::
handle_view(THD *thd, Query_tables_list *prelocking_ctx,
            TABLE_LIST *table_list, bool *need_prelocking)
{
  return FALSE;
}


/**
  Check that lock is ok for tables; Call start stmt if ok

  @param thd             Thread handle.
  @param prelocking_ctx  Prelocking context.
  @param table_list      Table list element for table to be checked.

  @retval FALSE - Ok.
  @retval TRUE  - Error.
*/

static bool check_lock_and_start_stmt(THD *thd,
                                      Query_tables_list *prelocking_ctx,
                                      TABLE_LIST *table_list)
{
  int error;
  thr_lock_type lock_type;
  DBUG_ENTER("check_lock_and_start_stmt");

  /*
    Prelocking placeholder is not set for TABLE_LIST that
    are directly used by TOP level statement.
  */
  DBUG_ASSERT(table_list->prelocking_placeholder == false);

  /*
    TL_WRITE_DEFAULT, TL_READ_DEFAULT and TL_WRITE_CONCURRENT_DEFAULT
    are supposed to be parser only types of locks so they should be
    converted to appropriate other types to be passed to storage engine.
    The exact lock type passed to the engine is important as, for example,
    InnoDB uses it to determine what kind of row locks should be acquired
    when executing statement in prelocked mode or under LOCK TABLES with
    @@innodb_table_locks = 0.

    Last argument routine_modifies_data for read_lock_type_for_table()
    is ignored, as prelocking placeholder will never be set here.
  */
  if (table_list->lock_type == TL_WRITE_DEFAULT)
    lock_type= thd->update_lock_default;
  else if (table_list->lock_type == TL_WRITE_CONCURRENT_DEFAULT)
    lock_type= thd->insert_lock_default;
  else if (table_list->lock_type == TL_READ_DEFAULT)
    lock_type= read_lock_type_for_table(thd, prelocking_ctx, table_list, true);
  else
    lock_type= table_list->lock_type;

  if ((int) lock_type > (int) TL_WRITE_ALLOW_WRITE &&
      (int) table_list->table->reginfo.lock_type <= (int) TL_WRITE_ALLOW_WRITE)
  {
    my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE, MYF(0), table_list->alias);
    DBUG_RETURN(1);
  }
  if ((error= table_list->table->file->start_stmt(thd, lock_type)))
  {
    table_list->table->file->print_error(error, MYF(0));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/**
  @brief Open and lock one table

  @param[in]    thd             thread handle
  @param[in]    table_l         table to open is first table in this list
  @param[in]    lock_type       lock to use for table
  @param[in]    flags           options to be used while opening and locking
                                table (see open_table(), mysql_lock_tables())
  @param[in]    prelocking_strategy  Strategy which specifies how prelocking
                                     algorithm should work for this statement.

  @return       table
    @retval     != NULL         OK, opened table returned
    @retval     NULL            Error

  @note
    If ok, the following are also set:
      table_list->lock_type 	lock_type
      table_list->table		table

  @note
    If table_l is a list, not a single table, the list is temporarily
    broken.

  @detail
    This function is meant as a replacement for open_ltable() when
    MERGE tables can be opened. open_ltable() cannot open MERGE tables.

    There may be more differences between open_n_lock_single_table() and
    open_ltable(). One known difference is that open_ltable() does
    neither call thd->decide_logging_format() nor handle some other logging
    and locking issues because it does not call lock_tables().
*/

TABLE *open_n_lock_single_table(THD *thd, TABLE_LIST *table_l,
                                thr_lock_type lock_type, uint flags,
                                Prelocking_strategy *prelocking_strategy)
{
  TABLE_LIST *save_next_global;
  DBUG_ENTER("open_n_lock_single_table");

  /* Remember old 'next' pointer. */
  save_next_global= table_l->next_global;
  /* Break list. */
  table_l->next_global= NULL;

  /* Set requested lock type. */
  table_l->lock_type= lock_type;
  /* Allow to open real tables only. */
  table_l->required_type= FRMTYPE_TABLE;

  /* Open the table. */
  if (open_and_lock_tables(thd, table_l, FALSE, flags,
                           prelocking_strategy))
    table_l->table= NULL; /* Just to be sure. */

  /* Restore list. */
  table_l->next_global= save_next_global;

  DBUG_RETURN(table_l->table);
}


/*
  Open and lock one table

  SYNOPSIS
    open_ltable()
    thd			Thread handler
    table_list		Table to open is first table in this list
    lock_type		Lock to use for open
    lock_flags          Flags passed to mysql_lock_table

  NOTE
    This function doesn't do anything like SP/SF/views/triggers analysis done 
    in open_table()/lock_tables(). It is intended for opening of only one
    concrete table. And used only in special contexts.

  RETURN VALUES
    table		Opened table
    0			Error
  
    If ok, the following are also set:
      table_list->lock_type 	lock_type
      table_list->table		table
*/

TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type lock_type,
                   uint lock_flags)
{
  TABLE *table;
  Open_table_context ot_ctx(thd, lock_flags);
  bool error;
  DBUG_ENTER("open_ltable");

  /* should not be used in a prelocked_mode context, see NOTE above */
  DBUG_ASSERT(thd->locked_tables_mode < LTM_PRELOCKED);

  THD_STAGE_INFO(thd, stage_opening_tables);
  thd->current_tablenr= 0;
  /* open_ltable can be used only for BASIC TABLEs */
  table_list->required_type= FRMTYPE_TABLE;

  /* This function can't properly handle requests for such metadata locks. */
  DBUG_ASSERT(table_list->mdl_request.type < MDL_SHARED_UPGRADABLE);

  while ((error= open_table(thd, table_list, &ot_ctx)) &&
         ot_ctx.can_recover_from_failed_open())
  {
    /*
      Even though we have failed to open table we still need to
      call release_transactional_locks() to release metadata locks which
      might have been acquired successfully.
    */
    thd->mdl_context.rollback_to_savepoint(ot_ctx.start_of_statement_svp());
    table_list->mdl_request.ticket= 0;
    if (ot_ctx.recover_from_failed_open(thd))
      break;
  }

  if (!error)
  {
    /*
      We can't have a view or some special "open_strategy" in this function
      so there should be a TABLE instance.
    */
    DBUG_ASSERT(table_list->table);
    table= table_list->table;
    if (table->file->ht->db_type == DB_TYPE_MRG_MYISAM)
    {
      /* A MERGE table must not come here. */
      /* purecov: begin tested */
      my_error(ER_WRONG_OBJECT, MYF(0), table->s->db.str,
               table->s->table_name.str, "BASE TABLE");
      table= 0;
      goto end;
      /* purecov: end */
    }

    table_list->lock_type= lock_type;
    table->grant= table_list->grant;
    if (thd->locked_tables_mode)
    {
      if (check_lock_and_start_stmt(thd, thd->lex, table_list))
	table= 0;
    }
    else
    {
      DBUG_ASSERT(thd->lock == 0);	// You must lock everything at once
      if ((table->reginfo.lock_type= lock_type) != TL_UNLOCK)
	if (! (thd->lock= mysql_lock_tables(thd, &table_list->table, 1,
                                            lock_flags)))
        {
          table= 0;
        }
    }
  }
  else
    table= 0;

end:
  if (table == NULL)
  {
    if (!thd->in_sub_stmt)
      trans_rollback_stmt(thd);
    close_thread_tables(thd);
  }
  DBUG_RETURN(table);
}


/**
  Open all tables in list, locks them and optionally process derived tables.

  @param thd		      Thread context.
  @param tables	              List of tables for open and locking.
  @param derived              If to handle derived tables.
  @param flags                Bitmap of options to be used to open and lock
                              tables (see open_tables() and mysql_lock_tables()
                              for details).
  @param prelocking_strategy  Strategy which specifies how prelocking algorithm
                              should work for this statement.

  @note
    The thr_lock locks will automatically be freed by
    close_thread_tables().

  @retval FALSE  OK.
  @retval TRUE   Error
*/

bool open_and_lock_tables(THD *thd, TABLE_LIST *tables,
                          bool derived, uint flags,
                          Prelocking_strategy *prelocking_strategy)
{
  uint counter;
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();
  DBUG_ENTER("open_and_lock_tables");
  DBUG_PRINT("enter", ("derived handling: %d", derived));

  if (open_tables(thd, &tables, &counter, flags, prelocking_strategy))
    goto err;

  DBUG_EXECUTE_IF("sleep_open_and_lock_after_open", {
                  const char *old_proc_info= thd->proc_info;
                  thd->proc_info= "DBUG sleep";
                  my_sleep(6000000);
                  thd->proc_info= old_proc_info;});

  if (lock_tables(thd, tables, counter, flags))
    goto err;

  if (derived &&
      (mysql_handle_derived(thd->lex, &mysql_derived_prepare)))
    goto err;

  DBUG_RETURN(FALSE);
err:
  if (! thd->in_sub_stmt)
    trans_rollback_stmt(thd);  /* Necessary if derived handling failed. */
  close_thread_tables(thd);
  /* Don't keep locks for a failed statement. */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
  DBUG_RETURN(TRUE);
}


/**
  Open all tables in list and process derived tables

  @param       thd      thread handler
  @param       tables   list of tables for open
  @param       flags    bitmap of flags to modify how the tables will be open:
                        MYSQL_LOCK_IGNORE_FLUSH - open table even if someone has
                        done a flush on it.

  @retval false - ok
  @retval true  - error

  @note
    This is to be used on prepare stage when you don't read any
    data from the tables.

  @note
    Updates Query_tables_list::table_count as side-effect.
*/

bool open_normal_and_derived_tables(THD *thd, TABLE_LIST *tables, uint flags)
{
  DML_prelocking_strategy prelocking_strategy;
  MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();
  DBUG_ENTER("open_normal_and_derived_tables");
  if (open_tables(thd, &tables, &thd->lex->table_count, flags,
                  &prelocking_strategy) ||
      mysql_handle_derived(thd->lex, &mysql_derived_prepare))
    goto end;

  DBUG_RETURN(0);
end:
  /*
    No need to commit/rollback the statement transaction: it's
    either not started or we're filling in an INFORMATION_SCHEMA
    table on the fly, and thus mustn't manipulate with the
    transaction of the enclosing statement.
  */
  DBUG_ASSERT(thd->transaction.stmt.is_empty() ||
              (thd->state_flags & Open_tables_state::BACKUPS_AVAIL) ||
              thd->in_sub_stmt);
  close_thread_tables(thd);
  /* Don't keep locks for a failed statement. */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

  DBUG_RETURN(TRUE); /* purecov: inspected */
}


/*
  Mark all real tables in the list as free for reuse.

  SYNOPSIS
    mark_real_tables_as_free_for_reuse()
      thd   - thread context
      table - head of the list of tables

  DESCRIPTION
    Marks all real tables in the list (i.e. not views, derived
    or schema tables) as free for reuse.
*/

static void mark_real_tables_as_free_for_reuse(TABLE_LIST *table_list)
{
  TABLE_LIST *table;
  for (table= table_list; table; table= table->next_global)
    if (!table->placeholder())
    {
      table->table->query_id= 0;
    }
  for (table= table_list; table; table= table->next_global)
    if (!table->placeholder())
    {
      /*
        Detach children of MyISAMMRG tables used in
        sub-statements, they will be reattached at open.
        This has to be done in a separate loop to make sure
        that children have had their query_id cleared.
      */
      table->table->file->extra(HA_EXTRA_DETACH_CHILDREN);
    }
}


/**
  Lock all tables in a list.

  @param  thd           Thread handler
  @param  tables        Tables to lock
  @param  count         Number of opened tables
  @param  flags         Options (see mysql_lock_tables() for details)

  You can't call lock_tables() while holding thr_lock locks, as
  this would break the dead-lock-free handling thr_lock gives us.
  You must always get all needed locks at once.

  If the query for which we are calling this function is marked as
  requiring prelocking, this function will change
  locked_tables_mode to LTM_PRELOCKED.

  @retval FALSE         Success. 
  @retval TRUE          A lock wait timeout, deadlock or out of memory.
*/

bool lock_tables(THD *thd, TABLE_LIST *tables, uint count,
                 uint flags)
{
  TABLE_LIST *table;

  DBUG_ENTER("lock_tables");
  /*
    We can't meet statement requiring prelocking if we already
    in prelocked mode.
  */
  DBUG_ASSERT(thd->locked_tables_mode <= LTM_LOCK_TABLES ||
              !thd->lex->requires_prelocking());

  /*
    lock_tables() should not be called if this statement has
    already locked its tables.
  */
  DBUG_ASSERT(thd->lex->lock_tables_state == Query_tables_list::LTS_NOT_LOCKED);

  if (!tables && !thd->lex->requires_prelocking())
  {
    /*
      Even though we are not really locking any tables mark this
      statement as one that has locked its tables, so we won't
      call this function second time for the same execution of
      the same statement.
    */
    thd->lex->lock_tables_state= Query_tables_list::LTS_LOCKED;
    DBUG_RETURN(thd->decide_logging_format(tables));
  }

  /*
    Check for thd->locked_tables_mode to avoid a redundant
    and harmful attempt to lock the already locked tables again.
    Checking for thd->lock is not enough in some situations. For example,
    if a stored function contains
    "drop table t3; create temporary t3 ..; insert into t3 ...;"
    thd->lock may be 0 after drop tables, whereas locked_tables_mode
    is still on. In this situation an attempt to lock temporary
    table t3 will lead to a memory leak.
  */
  if (! thd->locked_tables_mode)
  {
    DBUG_ASSERT(thd->lock == 0);	// You must lock everything at once
    TABLE **start,**ptr;

    if (!(ptr=start=(TABLE**) thd->alloc(sizeof(TABLE*)*count)))
      DBUG_RETURN(TRUE);
    for (table= tables; table; table= table->next_global)
    {
      if (!table->placeholder())
	*(ptr++)= table->table;
    }

    /*
    DML statements that modify a table with an auto_increment column based on
    rows selected from a table are unsafe as the order in which the rows are
    fetched fron the select tables cannot be determined and may differ on
    master and slave.
    */
    if (thd->variables.binlog_format != BINLOG_FORMAT_ROW && tables &&
        has_write_table_with_auto_increment_and_select(tables))
      thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_WRITE_AUTOINC_SELECT);
    /* Todo: merge all has_write_table_auto_inc with decide_logging_format */
    if (thd->variables.binlog_format != BINLOG_FORMAT_ROW && tables)
    {
      if (has_write_table_auto_increment_not_first_in_pk(tables))
        thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_AUTOINC_NOT_FIRST);
    }

    /* 
     INSERT...ON DUPLICATE KEY UPDATE on a table with more than one unique keys
     can be unsafe.
     */
    uint unique_keys= 0;
    for (TABLE_LIST *query_table= tables; query_table && unique_keys <= 1;
         query_table= query_table->next_global)
      if(query_table->table)
      {
        uint keys= query_table->table->s->keys, i= 0;
        unique_keys= 0;
        for (KEY* keyinfo= query_table->table->s->key_info;
             i < keys && unique_keys <= 1; i++, keyinfo++)
        {
          if (keyinfo->flags & HA_NOSAME)
            unique_keys++;
        }
        if (!query_table->placeholder() &&
            query_table->lock_type >= TL_WRITE_ALLOW_WRITE &&
            unique_keys > 1 && thd->lex->sql_command == SQLCOM_INSERT &&
            thd->lex->duplicates == DUP_UPDATE)
          thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_INSERT_TWO_KEYS);
      }
 
 
    /* We have to emulate LOCK TABLES if we are statement needs prelocking. */
    if (thd->lex->requires_prelocking())
    {

      /*
        A query that modifies autoinc column in sub-statement can make the 
        master and slave inconsistent.
        We can solve these problems in mixed mode by switching to binlogging 
        if at least one updated table is used by sub-statement
      */
      if (thd->variables.binlog_format != BINLOG_FORMAT_ROW && tables && 
          has_write_table_with_auto_increment(thd->lex->first_not_own_table()))
        thd->lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_AUTOINC_COLUMNS);
    }

    DEBUG_SYNC(thd, "before_lock_tables_takes_lock");

    if (! (thd->lock= mysql_lock_tables(thd, start, (uint) (ptr - start),
                                        flags)))
      DBUG_RETURN(TRUE);

    DEBUG_SYNC(thd, "after_lock_tables_takes_lock");

    if (thd->lex->requires_prelocking() &&
        thd->lex->sql_command != SQLCOM_LOCK_TABLES)
    {
      TABLE_LIST *first_not_own= thd->lex->first_not_own_table();
      /*
        We just have done implicit LOCK TABLES, and now we have
        to emulate first open_and_lock_tables() after it.

        When open_and_lock_tables() is called for a single table out of
        a table list, the 'next_global' chain is temporarily broken. We
        may not find 'first_not_own' before the end of the "list".
        Look for example at those places where open_n_lock_single_table()
        is called. That function implements the temporary breaking of
        a table list for opening a single table.
      */
      for (table= tables;
           table && table != first_not_own;
           table= table->next_global)
      {
        if (!table->placeholder())
        {
          table->table->query_id= thd->query_id;
          if (check_lock_and_start_stmt(thd, thd->lex, table))
          {
            mysql_unlock_tables(thd, thd->lock);
            thd->lock= 0;
            DBUG_RETURN(TRUE);
          }
        }
      }
      /*
        Let us mark all tables which don't belong to the statement itself,
        and was marked as occupied during open_tables() as free for reuse.
      */
      mark_real_tables_as_free_for_reuse(first_not_own);
      DBUG_PRINT("info",("locked_tables_mode= LTM_PRELOCKED"));
      thd->enter_locked_tables_mode(LTM_PRELOCKED);
    }
  }
  else
  {
    TABLE_LIST *first_not_own= thd->lex->first_not_own_table();
    /*
      When open_and_lock_tables() is called for a single table out of
      a table list, the 'next_global' chain is temporarily broken. We
      may not find 'first_not_own' before the end of the "list".
      Look for example at those places where open_n_lock_single_table()
      is called. That function implements the temporary breaking of
      a table list for opening a single table.
    */
    for (table= tables;
         table && table != first_not_own;
         table= table->next_global)
    {
      if (table->placeholder())
        continue;

      /*
        In a stored function or trigger we should ensure that we won't change
        a table that is already used by the calling statement.
      */
      if (thd->locked_tables_mode >= LTM_PRELOCKED &&
          table->lock_type >= TL_WRITE_ALLOW_WRITE)
      {
        for (TABLE* opentab= thd->open_tables; opentab; opentab= opentab->next)
        {
          if (table->table->s == opentab->s && opentab->query_id &&
              table->table->query_id != opentab->query_id)
          {
            my_error(ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG, MYF(0),
                     table->table->s->table_name.str);
            DBUG_RETURN(TRUE);
          }
        }
      }

      if (check_lock_and_start_stmt(thd, thd->lex, table))
      {
	DBUG_RETURN(TRUE);
      }
    }
    /*
      If we are under explicit LOCK TABLES and our statement requires
      prelocking, we should mark all "additional" tables as free for use
      and enter prelocked mode.
    */
    if (thd->lex->requires_prelocking())
    {
      mark_real_tables_as_free_for_reuse(first_not_own);
      DBUG_PRINT("info",
                 ("thd->locked_tables_mode= LTM_PRELOCKED_UNDER_LOCK_TABLES"));
      thd->locked_tables_mode= LTM_PRELOCKED_UNDER_LOCK_TABLES;
    }
  }

  /*
    Mark the statement as having tables locked. For purposes
    of Query_tables_list::lock_tables_state we treat any
    statement which passes through lock_tables() as such.
  */
  thd->lex->lock_tables_state= Query_tables_list::LTS_LOCKED;

  DBUG_RETURN(thd->decide_logging_format(tables));
}


/**
  Prepare statement for reopening of tables and recalculation of set of
  prelocked tables.

  @param[in] thd         Thread context.
  @param[in,out] tables  List of tables which we were trying to open
                         and lock.
  @param[in] start_of_statement_svp MDL savepoint which represents the set
                         of metadata locks which the current transaction
                         managed to acquire before execution of the current
                         statement and to which we should revert before
                         trying to reopen tables. NULL if no metadata locks
                         were held and thus all metadata locks should be
                         released.
*/

void close_tables_for_reopen(THD *thd, TABLE_LIST **tables,
                             const MDL_savepoint &start_of_statement_svp)
{
  TABLE_LIST *first_not_own_table= thd->lex->first_not_own_table();
  TABLE_LIST *tmp;

  /*
    If table list consists only from tables from prelocking set, table list
    for new attempt should be empty, so we have to update list's root pointer.
  */
  if (first_not_own_table == *tables)
    *tables= 0;
  thd->lex->chop_off_not_own_tables();
  /* Reset MDL tickets for procedures/functions */
  for (Sroutine_hash_entry *rt=
         (Sroutine_hash_entry*)thd->lex->sroutines_list.first;
       rt; rt= rt->next)
    rt->mdl_request.ticket= NULL;
  sp_remove_not_own_routines(thd->lex);
  for (tmp= *tables; tmp; tmp= tmp->next_global)
  {
    tmp->table= 0;
    tmp->mdl_request.ticket= NULL;
    /* We have to cleanup translation tables of views. */
    tmp->cleanup_items();
  }
  /*
    No need to commit/rollback the statement transaction: it's
    either not started or we're filling in an INFORMATION_SCHEMA
    table on the fly, and thus mustn't manipulate with the
    transaction of the enclosing statement.
  */
  DBUG_ASSERT(thd->transaction.stmt.is_empty() ||
              (thd->state_flags & Open_tables_state::BACKUPS_AVAIL));
  close_thread_tables(thd);
  thd->mdl_context.rollback_to_savepoint(start_of_statement_svp);
}


/**
  Open a single table without table caching and don't add it to
  THD::open_tables. Depending on the 'add_to_temporary_tables_list' value,
  the opened TABLE instance will be addded to THD::temporary_tables list.

  @param thd                          Thread context.
  @param path                         Path (without .frm)
  @param db                           Database name.
  @param table_name                   Table name.
  @param add_to_temporary_tables_list Specifies if the opened TABLE
                                      instance should be linked into
                                      THD::temporary_tables list.
  @param open_in_engine               Indicates that we need to open table
                                      in storage engine in addition to
                                      constructing TABLE object for it.

  @note This function is used:
    - by alter_table() to open a temporary table;
    - when creating a temporary table with CREATE TEMPORARY TABLE.

  @return TABLE instance for opened table.
  @retval NULL on error.
*/

TABLE *open_table_uncached(THD *thd, const char *path, const char *db,
                           const char *table_name,
                           bool add_to_temporary_tables_list,
                           bool open_in_engine)
{
  TABLE *tmp_table;
  TABLE_SHARE *share;
  char cache_key[MAX_DBKEY_LENGTH], *saved_cache_key, *tmp_path;
  uint key_length;
  DBUG_ENTER("open_table_uncached");
  DBUG_PRINT("enter",
             ("table: '%s'.'%s'  path: '%s'  server_id: %u  "
              "pseudo_thread_id: %lu",
              db, table_name, path,
              (uint) thd->server_id, (ulong) thd->variables.pseudo_thread_id));

  /* Create the cache_key for temporary tables */
  key_length= create_table_def_key(thd, cache_key, db, table_name, 1);

  if (!(tmp_table= (TABLE*) my_malloc(sizeof(*tmp_table) + sizeof(*share) +
                                      strlen(path)+1 + key_length,
                                      MYF(MY_WME))))
    DBUG_RETURN(0);				/* purecov: inspected */

#ifndef DBUG_OFF
  mysql_mutex_lock(&LOCK_open);
  DBUG_ASSERT(!my_hash_search(&table_def_cache, (uchar*) cache_key,
                              key_length));
  mysql_mutex_unlock(&LOCK_open);
#endif

  share= (TABLE_SHARE*) (tmp_table+1);
  tmp_path= (char*) (share+1);
  saved_cache_key= strmov(tmp_path, path)+1;
  memcpy(saved_cache_key, cache_key, key_length);

  init_tmp_table_share(thd, share, saved_cache_key, key_length,
                       strend(saved_cache_key)+1, tmp_path);

  if (open_table_def(thd, share, 0))
  {
    /* No need to lock share->mutex as this is not needed for tmp tables */
    free_table_share(share);
    my_free(tmp_table);
    DBUG_RETURN(0);
  }

#ifdef HAVE_PSI_TABLE_INTERFACE
  share->m_psi= PSI_TABLE_CALL(get_table_share)(true, share);
#else
  share->m_psi= NULL;
#endif

  if (open_table_from_share(thd, share, table_name,
                            open_in_engine ?
                            (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE |
                                    HA_GET_INDEX) : 0,
                            READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
                            ha_open_options,
                            tmp_table,
                            /*
                              Set "is_create_table" if the table does not
                              exist in SE
                            */
                            open_in_engine ? false : true))
  {
    /* No need to lock share->mutex as this is not needed for tmp tables */
    free_table_share(share);
    my_free(tmp_table);
    DBUG_RETURN(0);
  }

  tmp_table->reginfo.lock_type= TL_WRITE;	 // Simulate locked
  share->tmp_table= (tmp_table->file->has_transactions() ? 
                     TRANSACTIONAL_TMP_TABLE : NON_TRANSACTIONAL_TMP_TABLE);

  if (add_to_temporary_tables_list)
  {
    /* growing temp list at the head */
    tmp_table->next= thd->temporary_tables;
    if (tmp_table->next)
      tmp_table->next->prev= tmp_table;
    thd->temporary_tables= tmp_table;
    thd->temporary_tables->prev= 0;
    if (thd->slave_thread)
      modify_slave_open_temp_tables(thd, 1);
  }
  tmp_table->pos_in_table_list= 0;
  tmp_table->created= true;
  DBUG_PRINT("tmptable", ("opened table: '%s'.'%s' 0x%lx", tmp_table->s->db.str,
                          tmp_table->s->table_name.str, (long) tmp_table));
  DBUG_RETURN(tmp_table);
}


/**
  Delete a temporary table.

  @param base  Handlerton for table to be deleted.
  @param path  Path to the table to be deleted (i.e. path
               to its .frm without an extension).

  @retval false - success.
  @retval true  - failure.
*/

bool rm_temporary_table(handlerton *base, const char *path)
{
  bool error=0;
  handler *file;
  char frm_path[FN_REFLEN + 1];
  DBUG_ENTER("rm_temporary_table");

  strxnmov(frm_path, sizeof(frm_path) - 1, path, reg_ext, NullS);
  if (mysql_file_delete(key_file_frm, frm_path, MYF(0)))
    error=1; /* purecov: inspected */
  file= get_new_handler((TABLE_SHARE*) 0, current_thd->mem_root, base);
  if (file && file->ha_delete_table(path))
  {
    error=1;
    sql_print_warning("Could not remove temporary table: '%s', error: %d",
                      path, my_errno);
  }
  delete file;
  DBUG_RETURN(error);
}


/*****************************************************************************
* The following find_field_in_XXX procedures implement the core of the
* name resolution functionality. The entry point to resolve a column name in a
* list of tables is 'find_field_in_tables'. It calls 'find_field_in_table_ref'
* for each table reference. In turn, depending on the type of table reference,
* 'find_field_in_table_ref' calls one of the 'find_field_in_XXX' procedures
* below specific for the type of table reference.
******************************************************************************/

/* Special Field pointers as return values of find_field_in_XXX functions. */
Field *not_found_field= (Field*) 0x1;
Field *view_ref_found= (Field*) 0x2; 

#define WRONG_GRANT (Field*) -1

static void update_field_dependencies(THD *thd, Field *field, TABLE *table)
{
  DBUG_ENTER("update_field_dependencies");
  if (thd->mark_used_columns != MARK_COLUMNS_NONE)
  {
    MY_BITMAP *bitmap;

    /*
      We always want to register the used keys, as the column bitmap may have
      been set for all fields (for example for view).
    */
      
    table->covering_keys.intersect(field->part_of_key);
    table->merge_keys.merge(field->part_of_key);

    if (thd->mark_used_columns == MARK_COLUMNS_READ)
      bitmap= table->read_set;
    else
      bitmap= table->write_set;

    /* 
       The test-and-set mechanism in the bitmap is not reliable during
       multi-UPDATE statements under MARK_COLUMNS_READ mode
       (thd->mark_used_columns == MARK_COLUMNS_READ), as this bitmap contains
       only those columns that are used in the SET clause. I.e they are being
       set here. See multi_update::prepare()
    */
    if (bitmap_fast_test_and_set(bitmap, field->field_index))
    {
      if (thd->mark_used_columns == MARK_COLUMNS_WRITE)
      {
        DBUG_PRINT("warning", ("Found duplicated field"));
        thd->dup_field= field;
      }
      else
      {
        DBUG_PRINT("note", ("Field found before"));
      }
      DBUG_VOID_RETURN;
    }
    if (table->get_fields_in_item_tree)
      field->flags|= GET_FIXED_FIELDS_FLAG;
    table->used_fields++;
  }
  else if (table->get_fields_in_item_tree)
    field->flags|= GET_FIXED_FIELDS_FLAG;
  DBUG_VOID_RETURN;
}


/**
  Find a temporary table specified by TABLE_LIST instance in the cache and
  prepare its TABLE instance for use.

  This function tries to resolve this table in the list of temporary tables
  of this thread. Temporary tables are thread-local and "shadow" base
  tables with the same name.

  @note In most cases one should use open_temporary_tables() instead
        of this call.

  @note One should finalize process of opening temporary table for table
        list element by calling open_and_process_table(). This function
        is responsible for table version checking and handling of merge
        tables.

  @note We used to check global_read_lock before opening temporary tables.
        However, that limitation was artificial and is removed now.

  @return Error status.
    @retval FALSE On success. If a temporary table exists for the given
                  key, tl->table is set.
    @retval TRUE  On error. my_error() has been called.
*/

bool open_temporary_table(THD *thd, TABLE_LIST *tl)
{
  DBUG_ENTER("open_temporary_table");
  DBUG_PRINT("enter", ("table: '%s'.'%s'", tl->db, tl->table_name));

  /*
    Code in open_table() assumes that TABLE_LIST::table can
    be non-zero only for pre-opened temporary tables.
  */
  DBUG_ASSERT(tl->table == NULL);

  /*
    This function should not be called for cases when derived or I_S
    tables can be met since table list elements for such tables can
    have invalid db or table name.
    Instead open_temporary_tables() should be used.
  */
  DBUG_ASSERT(!tl->derived && !tl->schema_table);

  if (tl->open_type == OT_BASE_ONLY)
  {
    DBUG_PRINT("info", ("skip_temporary is set"));
    DBUG_RETURN(FALSE);
  }

  TABLE *table= find_temporary_table(thd, tl);

  if (!table)
  {
    if (tl->open_type == OT_TEMPORARY_ONLY &&
        tl->open_strategy == TABLE_LIST::OPEN_NORMAL)
    {
      my_error(ER_NO_SUCH_TABLE, MYF(0), tl->db, tl->table_name);
      DBUG_RETURN(TRUE);
    }
    DBUG_RETURN(FALSE);
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (tl->partition_names)
  {
    /* Partitioned temporary tables is not supported. */
    DBUG_ASSERT(!table->part_info);
    my_error(ER_PARTITION_CLAUSE_ON_NONPARTITIONED, MYF(0));
    DBUG_RETURN(true);
  }
#endif

  if (table->query_id)
  {
    /*
      We're trying to use the same temporary table twice in a query.
      Right now we don't support this because a temporary table is always
      represented by only one TABLE object in THD, and it can not be
      cloned. Emit an error for an unsupported behaviour.
    */

    DBUG_PRINT("error",
               ("query_id: %lu  server_id: %u  pseudo_thread_id: %lu",
                (ulong) table->query_id, (uint) thd->server_id,
                (ulong) thd->variables.pseudo_thread_id));
    my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->alias);
    DBUG_RETURN(TRUE);
  }

  table->query_id= thd->query_id;
  thd->thread_specific_used= TRUE;

  tl->updatable= 1; // It is not derived table nor non-updatable VIEW.
  tl->table= table;

  table->init(thd, tl);

  DBUG_PRINT("info", ("Using temporary table"));
  DBUG_RETURN(FALSE);
}


/**
  Pre-open temporary tables corresponding to table list elements.

  @note One should finalize process of opening temporary tables
        by calling open_tables(). This function is responsible
        for table version checking and handling of merge tables.

  @return Error status.
    @retval FALSE On success. If a temporary tables exists for the
                  given element, tl->table is set.
    @retval TRUE  On error. my_error() has been called.
*/

bool open_temporary_tables(THD *thd, TABLE_LIST *tl_list)
{
  TABLE_LIST *first_not_own= thd->lex->first_not_own_table();
  DBUG_ENTER("open_temporary_tables");

  for (TABLE_LIST *tl= tl_list; tl && tl != first_not_own; tl= tl->next_global)
  {
    if (tl->derived || tl->schema_table)
    {
      /*
        Derived and I_S tables will be handled by a later call to open_tables().
      */
      continue;
    }

    if (open_temporary_table(thd, tl))
      DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}


/*
  Find a field by name in a view that uses merge algorithm.

  SYNOPSIS
    find_field_in_view()
    thd				thread handler
    table_list			view to search for 'name'
    name			name of field
    length			length of name
    item_name                   name of item if it will be created (VIEW)
    ref				expression substituted in VIEW should be passed
                                using this reference (return view_ref_found)
    register_tree_change        TRUE if ref is not stack variable and we
                                need register changes in item tree

  RETURN
    0			field is not found
    view_ref_found	found value in VIEW (real result is in *ref)
    #			pointer to field - only for schema table fields
*/

static Field *
find_field_in_view(THD *thd, TABLE_LIST *table_list,
                   const char *name, uint length,
                   const char *item_name, Item **ref,
                   bool register_tree_change)
{
  DBUG_ENTER("find_field_in_view");
  DBUG_PRINT("enter",
             ("view: '%s', field name: '%s', item name: '%s', ref 0x%lx",
              table_list->alias, name, item_name, (ulong) ref));
  Field_iterator_view field_it;
  field_it.set(table_list);
  Query_arena *arena= 0, backup;  
  
  DBUG_ASSERT(table_list->schema_table_reformed ||
              (ref != 0 && table_list->view != 0));
  for (; !field_it.end_of_fields(); field_it.next())
  {
    if (!my_strcasecmp(system_charset_info, field_it.name(), name))
    {
      // in PS use own arena or data will be freed after prepare
      if (register_tree_change &&
          thd->stmt_arena->is_stmt_prepare_or_first_stmt_execute())
        arena= thd->activate_stmt_arena_if_needed(&backup);
      /*
        create_item() may, or may not create a new Item, depending on
        the column reference. See create_view_field() for details.
      */
      Item *item= field_it.create_item(thd);
      if (arena)
        thd->restore_active_arena(arena, &backup);
      
      if (!item)
        DBUG_RETURN(0);
      /*
       *ref != NULL means that *ref contains the item that we need to
       replace. If the item was aliased by the user, set the alias to
       the replacing item.
       We need to set alias on both ref itself and on ref real item.
      */
      if (*ref && !(*ref)->item_name.is_autogenerated())
      {
        item->item_name= (*ref)->item_name;
        item->real_item()->item_name= (*ref)->item_name;
      }
      if (register_tree_change)
        thd->change_item_tree(ref, item);
      else
        *ref= item;
      DBUG_RETURN((Field*) view_ref_found);
    }
  }
  DBUG_RETURN(0);
}


/*
  Find field by name in a NATURAL/USING join table reference.

  SYNOPSIS
    find_field_in_natural_join()
    thd			 [in]  thread handler
    table_ref            [in]  table reference to search
    name		 [in]  name of field
    length		 [in]  length of name
    ref                  [in/out] if 'name' is resolved to a view field, ref is
                               set to point to the found view field
    register_tree_change [in]  TRUE if ref is not stack variable and we
                               need register changes in item tree
    actual_table         [out] the original table reference where the field
                               belongs - differs from 'table_list' only for
                               NATURAL/USING joins

  DESCRIPTION
    Search for a field among the result fields of a NATURAL/USING join.
    Notice that this procedure is called only for non-qualified field
    names. In the case of qualified fields, we search directly the base
    tables of a natural join.

  RETURN
    NULL        if the field was not found
    WRONG_GRANT if no access rights to the found field
    #           Pointer to the found Field
*/

static Field *
find_field_in_natural_join(THD *thd, TABLE_LIST *table_ref, const char *name,
                           uint length, Item **ref, bool register_tree_change,
                           TABLE_LIST **actual_table)
{
  List_iterator_fast<Natural_join_column>
    field_it(*(table_ref->join_columns));
  Natural_join_column *nj_col, *curr_nj_col;
  Field *found_field;
  Query_arena *arena, backup;
  DBUG_ENTER("find_field_in_natural_join");
  DBUG_PRINT("enter", ("field name: '%s', ref 0x%lx",
		       name, (ulong) ref));
  DBUG_ASSERT(table_ref->is_natural_join && table_ref->join_columns);
  DBUG_ASSERT(*actual_table == NULL);

  LINT_INIT(arena);
  LINT_INIT(found_field);

  for (nj_col= NULL, curr_nj_col= field_it++; curr_nj_col; 
       curr_nj_col= field_it++)
  {
    if (!my_strcasecmp(system_charset_info, curr_nj_col->name(), name))
    {
      if (nj_col)
      {
        my_error(ER_NON_UNIQ_ERROR, MYF(0), name, thd->where);
        DBUG_RETURN(NULL);
      }
      nj_col= curr_nj_col;
    }
  }
  if (!nj_col)
    DBUG_RETURN(NULL);

  if (nj_col->view_field)
  {
    Item *item;
    LINT_INIT(arena);
    if (register_tree_change)
      arena= thd->activate_stmt_arena_if_needed(&backup);
    /*
      create_item() may, or may not create a new Item, depending on the
      column reference. See create_view_field() for details.
    */
    item= nj_col->create_item(thd);
    /*
     *ref != NULL means that *ref contains the item that we need to
     replace. If the item was aliased by the user, set the alias to
     the replacing item.
     We need to set alias on both ref itself and on ref real item.
     */
    if (*ref && !(*ref)->item_name.is_autogenerated())
    {
      item->item_name= (*ref)->item_name;
      item->real_item()->item_name= (*ref)->item_name;
    }
    if (register_tree_change && arena)
      thd->restore_active_arena(arena, &backup);

    if (!item)
      DBUG_RETURN(NULL);
    DBUG_ASSERT(nj_col->table_field == NULL);
    if (nj_col->table_ref->schema_table_reformed)
    {
      /*
        Translation table items are always Item_fields and fixed
        already('mysql_schema_table' function). So we can return
        ->field. It is used only for 'show & where' commands.
      */
      DBUG_RETURN(((Item_field*) (nj_col->view_field->item))->field);
    }
    if (register_tree_change)
      thd->change_item_tree(ref, item);
    else
      *ref= item;
    found_field= (Field*) view_ref_found;
  }
  else
  {
    /* This is a base table. */
    DBUG_ASSERT(nj_col->view_field == NULL);
    /*
      This fix_fields is not necessary (initially this item is fixed by
      the Item_field constructor; after reopen_tables the Item_func_eq
      calls fix_fields on that item), it's just a check during table
      reopening for columns that was dropped by the concurrent connection.
    */
    if (!nj_col->table_field->fixed &&
        nj_col->table_field->fix_fields(thd, (Item **)&nj_col->table_field))
    {
      DBUG_PRINT("info", ("column '%s' was dropped by the concurrent connection",
                          nj_col->table_field->item_name.ptr()));
      DBUG_RETURN(NULL);
    }
    DBUG_ASSERT(nj_col->table_ref->table == nj_col->table_field->field->table);
    found_field= nj_col->table_field->field;
    update_field_dependencies(thd, found_field, nj_col->table_ref->table);
  }

  *actual_table= nj_col->table_ref;
  
  DBUG_RETURN(found_field);
}


/*
  Find field by name in a base table or a view with temp table algorithm.

  The caller is expected to check column-level privileges.

  SYNOPSIS
    find_field_in_table()
    thd				thread handler
    table			table where to search for the field
    name			name of field
    length			length of name
    allow_rowid			do allow finding of "_rowid" field?
    cached_field_index_ptr	cached position in field list (used to speedup
                                lookup for fields in prepared tables)

  RETURN
    0	field is not found
    #	pointer to field
*/

Field *
find_field_in_table(THD *thd, TABLE *table, const char *name, uint length,
                    bool allow_rowid, uint *cached_field_index_ptr)
{
  Field **field_ptr, *field;
  uint cached_field_index= *cached_field_index_ptr;
  DBUG_ENTER("find_field_in_table");
  DBUG_PRINT("enter", ("table: '%s', field name: '%s'", table->alias, name));

  /* We assume here that table->field < NO_CACHED_FIELD_INDEX = UINT_MAX */
  if (cached_field_index < table->s->fields &&
      !my_strcasecmp(system_charset_info,
                     table->field[cached_field_index]->field_name, name))
    field_ptr= table->field + cached_field_index;
  else if (table->s->name_hash.records)
  {
    field_ptr= (Field**) my_hash_search(&table->s->name_hash, (uchar*) name,
                                        length);
    if (field_ptr)
    {
      /*
        field_ptr points to field in TABLE_SHARE. Convert it to the matching
        field in table
      */
      field_ptr= (table->field + (field_ptr - table->s->field));
    }
  }
  else
  {
    if (!(field_ptr= table->field))
      DBUG_RETURN((Field *)0);
    for (; *field_ptr; ++field_ptr)
      if (!my_strcasecmp(system_charset_info, (*field_ptr)->field_name, name))
        break;
  }

  if (field_ptr && *field_ptr)
  {
    *cached_field_index_ptr= field_ptr - table->field;
    field= *field_ptr;
  }
  else
  {
    if (!allow_rowid ||
        my_strcasecmp(system_charset_info, name, "_rowid") ||
        table->s->rowid_field_offset == 0)
      DBUG_RETURN((Field*) 0);
    field= table->field[table->s->rowid_field_offset-1];
  }

  update_field_dependencies(thd, field, table);

  DBUG_RETURN(field);
}


/*
  Find field in a table reference.

  SYNOPSIS
    find_field_in_table_ref()
    thd			   [in]  thread handler
    table_list		   [in]  table reference to search
    name		   [in]  name of field
    length		   [in]  field length of name
    item_name              [in]  name of item if it will be created (VIEW)
    db_name                [in]  optional database name that qualifies the
    table_name             [in]  optional table name that qualifies the field
    ref		       [in/out] if 'name' is resolved to a view field, ref
                                 is set to point to the found view field
    check_privileges       [in]  check privileges
    allow_rowid		   [in]  do allow finding of "_rowid" field?
    cached_field_index_ptr [in]  cached position in field list (used to
                                 speedup lookup for fields in prepared tables)
    register_tree_change   [in]  TRUE if ref is not stack variable and we
                                 need register changes in item tree
    actual_table           [out] the original table reference where the field
                                 belongs - differs from 'table_list' only for
                                 NATURAL_USING joins.

  DESCRIPTION
    Find a field in a table reference depending on the type of table
    reference. There are three types of table references with respect
    to the representation of their result columns:
    - an array of Field_translator objects for MERGE views and some
      information_schema tables,
    - an array of Field objects (and possibly a name hash) for stored
      tables,
    - a list of Natural_join_column objects for NATURAL/USING joins.
    This procedure detects the type of the table reference 'table_list'
    and calls the corresponding search routine.

    The routine checks column-level privieleges for the found field.

  RETURN
    0			field is not found
    view_ref_found	found value in VIEW (real result is in *ref)
    #			pointer to field
*/

Field *
find_field_in_table_ref(THD *thd, TABLE_LIST *table_list,
                        const char *name, uint length,
                        const char *item_name, const char *db_name,
                        const char *table_name, Item **ref,
                        bool check_privileges, bool allow_rowid,
                        uint *cached_field_index_ptr,
                        bool register_tree_change, TABLE_LIST **actual_table)
{
  Field *fld;
  DBUG_ENTER("find_field_in_table_ref");
  DBUG_ASSERT(table_list->alias);
  DBUG_ASSERT(name);
  DBUG_ASSERT(item_name);
  DBUG_PRINT("enter",
             ("table: '%s'  field name: '%s'  item name: '%s'  ref 0x%lx",
              table_list->alias, name, item_name, (ulong) ref));

  /*
    Check that the table and database that qualify the current field name
    are the same as the table reference we are going to search for the field.

    Exclude from the test below nested joins because the columns in a
    nested join generally originate from different tables. Nested joins
    also have no table name, except when a nested join is a merge view
    or an information schema table.

    We include explicitly table references with a 'field_translation' table,
    because if there are views over natural joins we don't want to search
    inside the view, but we want to search directly in the view columns
    which are represented as a 'field_translation'.

    TODO: Ensure that table_name, db_name and tables->db always points to
          something !
  */
  if (/* Exclude nested joins. */
      (!table_list->nested_join ||
       /* Include merge views and information schema tables. */
       table_list->field_translation) &&
      /*
        Test if the field qualifiers match the table reference we plan
        to search.
      */
      table_name && table_name[0] &&
      (my_strcasecmp(table_alias_charset, table_list->alias, table_name) ||
       (db_name && db_name[0] && table_list->db && table_list->db[0] &&
        (table_list->schema_table ?
         my_strcasecmp(system_charset_info, db_name, table_list->db) :
         strcmp(db_name, table_list->db)))))
    DBUG_RETURN(0);

  *actual_table= NULL;

  if (table_list->field_translation)
  {
    /* 'table_list' is a view or an information schema table. */
    if ((fld= find_field_in_view(thd, table_list, name, length, item_name, ref,
                                 register_tree_change)))
      *actual_table= table_list;
  }
  else if (!table_list->nested_join)
  {
    /* 'table_list' is a stored table. */
    DBUG_ASSERT(table_list->table);
    if ((fld= find_field_in_table(thd, table_list->table, name, length,
                                  allow_rowid,
                                  cached_field_index_ptr)))
      *actual_table= table_list;
  }
  else
  {
    /*
      'table_list' is a NATURAL/USING join, or an operand of such join that
      is a nested join itself.

      If the field name we search for is qualified, then search for the field
      in the table references used by NATURAL/USING the join.
    */
    if (table_name && table_name[0])
    {
      List_iterator<TABLE_LIST> it(table_list->nested_join->join_list);
      TABLE_LIST *table;
      while ((table= it++))
      {
        if ((fld= find_field_in_table_ref(thd, table, name, length, item_name,
                                          db_name, table_name, ref,
                                          check_privileges, allow_rowid,
                                          cached_field_index_ptr,
                                          register_tree_change, actual_table)))
          DBUG_RETURN(fld);
      }
      DBUG_RETURN(0);
    }
    /*
      Non-qualified field, search directly in the result columns of the
      natural join. The condition of the outer IF is true for the top-most
      natural join, thus if the field is not qualified, we will search
      directly the top-most NATURAL/USING join.
    */
    fld= find_field_in_natural_join(thd, table_list, name, length, ref,
                                    register_tree_change, actual_table);
  }

  if (fld)
  {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    /* Check if there are sufficient access rights to the found field. */
    if (check_privileges &&
        check_column_grant_in_table_ref(thd, *actual_table, name, length))
      fld= WRONG_GRANT;
    else
#endif
      if (thd->mark_used_columns != MARK_COLUMNS_NONE)
      {
        /*
          Get rw_set correct for this field so that the handler
          knows that this field is involved in the query and gets
          retrieved/updated
         */
        Field *field_to_set= NULL;
        if (fld == view_ref_found)
        {
          Item *it= (*ref)->real_item();
          if (it->type() == Item::FIELD_ITEM)
            field_to_set= ((Item_field*)it)->field;
          else
          {
            if (thd->mark_used_columns == MARK_COLUMNS_READ)
              it->walk(&Item::register_field_in_read_map, 1, (uchar *) 0);
          }
        }
        else
          field_to_set= fld;
        if (field_to_set)
        {
          TABLE *table= field_to_set->table;
          if (thd->mark_used_columns == MARK_COLUMNS_READ)
            bitmap_set_bit(table->read_set, field_to_set->field_index);
          else
            bitmap_set_bit(table->write_set, field_to_set->field_index);
        }
      }
  }
  DBUG_RETURN(fld);
}


/*
  Find field in table, no side effects, only purpose is to check for field
  in table object and get reference to the field if found.

  SYNOPSIS
  find_field_in_table_sef()

  table                         table where to find
  name                          Name of field searched for

  RETURN
    0                   field is not found
    #                   pointer to field
*/

Field *find_field_in_table_sef(TABLE *table, const char *name)
{
  Field **field_ptr;
  if (table->s->name_hash.records)
  {
    field_ptr= (Field**)my_hash_search(&table->s->name_hash,(uchar*) name,
                                       strlen(name));
    if (field_ptr)
    {
      /*
        field_ptr points to field in TABLE_SHARE. Convert it to the matching
        field in table
      */
      field_ptr= (table->field + (field_ptr - table->s->field));
    }
  }
  else
  {
    if (!(field_ptr= table->field))
      return (Field *)0;
    for (; *field_ptr; ++field_ptr)
      if (!my_strcasecmp(system_charset_info, (*field_ptr)->field_name, name))
        break;
  }
  if (field_ptr)
    return *field_ptr;
  else
    return (Field *)0;
}


/*
  Find field in table list.

  SYNOPSIS
    find_field_in_tables()
    thd			  pointer to current thread structure
    item		  field item that should be found
    first_table           list of tables to be searched for item
    last_table            end of the list of tables to search for item. If NULL
                          then search to the end of the list 'first_table'.
    ref			  if 'item' is resolved to a view field, ref is set to
                          point to the found view field
    report_error	  Degree of error reporting:
                          - IGNORE_ERRORS then do not report any error
                          - IGNORE_EXCEPT_NON_UNIQUE report only non-unique
                            fields, suppress all other errors
                          - REPORT_EXCEPT_NON_UNIQUE report all other errors
                            except when non-unique fields were found
                          - REPORT_ALL_ERRORS
    check_privileges      need to check privileges
    register_tree_change  TRUE if ref is not a stack variable and we
                          to need register changes in item tree

  RETURN VALUES
    0			If error: the found field is not unique, or there are
                        no sufficient access priviliges for the found field,
                        or the field is qualified with non-existing table.
    not_found_field	The function was called with report_error ==
                        (IGNORE_ERRORS || IGNORE_EXCEPT_NON_UNIQUE) and a
			field was not found.
    view_ref_found	View field is found, item passed through ref parameter
    found field         If a item was resolved to some field
*/

Field *
find_field_in_tables(THD *thd, Item_ident *item,
                     TABLE_LIST *first_table, TABLE_LIST *last_table,
		     Item **ref, find_item_error_report_type report_error,
                     bool check_privileges, bool register_tree_change)
{
  Field *found=0;
  const char *db= item->db_name;
  const char *table_name= item->table_name;
  const char *name= item->field_name;
  uint length=(uint) strlen(name);
  char name_buff[NAME_LEN+1];
  TABLE_LIST *cur_table= first_table;
  TABLE_LIST *actual_table;
  bool allow_rowid;

  if (!table_name || !table_name[0])
  {
    table_name= 0;                              // For easier test
    db= 0;
  }

  allow_rowid= table_name || (cur_table && !cur_table->next_local);

  if (item->cached_table)
  {
    /*
      This shortcut is used by prepared statements. We assume that
      TABLE_LIST *first_table is not changed during query execution (which
      is true for all queries except RENAME but luckily RENAME doesn't
      use fields...) so we can rely on reusing pointer to its member.
      With this optimization we also miss case when addition of one more
      field makes some prepared query ambiguous and so erroneous, but we
      accept this trade off.
    */
    TABLE_LIST *table_ref= item->cached_table;
    /*
      The condition (table_ref->view == NULL) ensures that we will call
      find_field_in_table even in the case of information schema tables
      when table_ref->field_translation != NULL.
      */
    if (table_ref->table && !table_ref->view)
    {
      found= find_field_in_table(thd, table_ref->table, name, length,
                                 TRUE, &(item->cached_field_index));
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      /* Check if there are sufficient access rights to the found field. */
      if (found && check_privileges &&
          check_column_grant_in_table_ref(thd, table_ref, name, length))
        found= WRONG_GRANT;
#endif
    }
    else
      found= find_field_in_table_ref(thd, table_ref, name, length,
                                     item->item_name.ptr(),
                                     NULL, NULL, ref, check_privileges,
                                     TRUE, &(item->cached_field_index),
                                     register_tree_change,
                                     &actual_table);
    if (found)
    {
      if (found == WRONG_GRANT)
	return (Field*) 0;

      /*
        Only views fields should be marked as dependent, not an underlying
        fields.
      */
      if (!table_ref->belong_to_view)
      {
        SELECT_LEX *current_sel= thd->lex->current_select;
        SELECT_LEX *last_select= table_ref->select_lex;
        /*
          If the field was an outer referencee, mark all selects using this
          sub query as dependent on the outer query
        */
        if (current_sel != last_select)
          mark_select_range_as_dependent(thd, last_select, current_sel,
                                         found, *ref, item);
      }
      return found;
    }
  }

  if (db && lower_case_table_names)
  {
    /*
      convert database to lower case for comparison.
      We can't do this in Item_field as this would change the
      'name' of the item which may be used in the select list
    */
    strmake(name_buff, db, sizeof(name_buff)-1);
    my_casedn_str(files_charset_info, name_buff);
    db= name_buff;
  }

  if (last_table)
    last_table= last_table->next_name_resolution_table;

  for (; cur_table != last_table ;
       cur_table= cur_table->next_name_resolution_table)
  {
    Field *cur_field= find_field_in_table_ref(thd, cur_table, name, length,
                                              item->item_name.ptr(), db, table_name, ref,
                                              (thd->lex->sql_command ==
                                               SQLCOM_SHOW_FIELDS)
                                              ? false : check_privileges,
                                              allow_rowid,
                                              &(item->cached_field_index),
                                              register_tree_change,
                                              &actual_table);
    if (cur_field)
    {
      if (cur_field == WRONG_GRANT)
      {
        if (thd->lex->sql_command != SQLCOM_SHOW_FIELDS)
          return (Field*) 0;

        thd->clear_error();
        cur_field= find_field_in_table_ref(thd, cur_table, name, length,
                                           item->item_name.ptr(), db, table_name, ref,
                                           false,
                                           allow_rowid,
                                           &(item->cached_field_index),
                                           register_tree_change,
                                           &actual_table);
        if (cur_field)
        {
          Field *nf=new Field_null(NULL,0,Field::NONE,
                                   cur_field->field_name,
                                   &my_charset_bin);
          nf->init(cur_table->table);
          cur_field= nf;
        }
      }

      /*
        Store the original table of the field, which may be different from
        cur_table in the case of NATURAL/USING join.
      */
      item->cached_table= (!actual_table->cacheable_table || found) ?
                          0 : actual_table;

      DBUG_ASSERT(thd->where);
      /*
        If we found a fully qualified field we return it directly as it can't
        have duplicates.
       */
      if (db)
        return cur_field;

      if (found)
      {
        if (report_error == REPORT_ALL_ERRORS ||
            report_error == IGNORE_EXCEPT_NON_UNIQUE)
          my_error(ER_NON_UNIQ_ERROR, MYF(0),
                   table_name ? item->full_name() : name, thd->where);
        return (Field*) 0;
      }
      found= cur_field;
    }
  }

  if (found)
    return found;

  /*
    If the field was qualified and there were no tables to search, issue
    an error that an unknown table was given. The situation is detected
    as follows: if there were no tables we wouldn't go through the loop
    and cur_table wouldn't be updated by the loop increment part, so it
    will be equal to the first table.
  */
  if (table_name && (cur_table == first_table) &&
      (report_error == REPORT_ALL_ERRORS ||
       report_error == REPORT_EXCEPT_NON_UNIQUE))
  {
    char buff[NAME_LEN*2 + 2];
    if (db && db[0])
    {
      strxnmov(buff,sizeof(buff)-1,db,".",table_name,NullS);
      table_name=buff;
    }
    my_error(ER_UNKNOWN_TABLE, MYF(0), table_name, thd->where);
  }
  else
  {
    if (report_error == REPORT_ALL_ERRORS ||
        report_error == REPORT_EXCEPT_NON_UNIQUE)
      my_error(ER_BAD_FIELD_ERROR, MYF(0), item->full_name(), thd->where);
    else
      found= not_found_field;
  }
  return found;
}


/*
  Find Item in list of items (find_field_in_tables analog)

  TODO
    is it better return only counter?

  SYNOPSIS
    find_item_in_list()
    find			Item to find
    items			List of items
    counter			To return number of found item
    report_error
      REPORT_ALL_ERRORS		report errors, return 0 if error
      REPORT_EXCEPT_NOT_FOUND	Do not report 'not found' error and
				return not_found_item, report other errors,
				return 0
      IGNORE_ERRORS		Do not report errors, return 0 if error
    resolution                  Set to the resolution type if the item is found 
                                (it says whether the item is resolved 
                                 against an alias name,
                                 or as a field name without alias,
                                 or as a field hidden by alias,
                                 or ignoring alias)
                                
  RETURN VALUES
    0			Item is not found or item is not unique,
			error message is reported
    not_found_item	Function was called with
			report_error == REPORT_EXCEPT_NOT_FOUND and
			item was not found. No error message was reported
                        found field
*/

/* Special Item pointer to serve as a return value from find_item_in_list(). */
Item **not_found_item= (Item**) 0x1;


Item **
find_item_in_list(Item *find, List<Item> &items, uint *counter,
                  find_item_error_report_type report_error,
                  enum_resolution_type *resolution)
{
  List_iterator<Item> li(items);
  Item **found=0, **found_unaliased= 0, *item;
  const char *db_name=0;
  const char *field_name=0;
  const char *table_name=0;
  bool found_unaliased_non_uniq= 0;
  /*
    true if the item that we search for is a valid name reference
    (and not an item that happens to have a name).
  */
  bool is_ref_by_name= 0;
  uint unaliased_counter= 0;

  *resolution= NOT_RESOLVED;

  is_ref_by_name= (find->type() == Item::FIELD_ITEM  || 
                   find->type() == Item::REF_ITEM);
  if (is_ref_by_name)
  {
    field_name= ((Item_ident*) find)->field_name;
    table_name= ((Item_ident*) find)->table_name;
    db_name=    ((Item_ident*) find)->db_name;
  }

  for (uint i= 0; (item=li++); i++)
  {
    if (field_name && item->real_item()->type() == Item::FIELD_ITEM)
    {
      Item_ident *item_field= (Item_ident*) item;

      /*
	In case of group_concat() with ORDER BY condition in the QUERY
	item_field can be field of temporary table without item name 
	(if this field created from expression argument of group_concat()),
	=> we have to check presence of name before compare
      */ 
      if (!item_field->item_name.is_set())
        continue;

      if (table_name)
      {
        /*
          If table name is specified we should find field 'field_name' in
          table 'table_name'. According to SQL-standard we should ignore
          aliases in this case.

          Since we should NOT prefer fields from the select list over
          other fields from the tables participating in this select in
          case of ambiguity we have to do extra check outside this function.

          We use strcmp for table names and database names as these may be
          case sensitive. In cases where they are not case sensitive, they
          are always in lower case.

	  item_field->field_name and item_field->table_name can be 0x0 if
	  item is not fix_field()'ed yet.
        */
        if (item_field->field_name && item_field->table_name &&
	    !my_strcasecmp(system_charset_info, item_field->field_name,
                           field_name) &&
            !my_strcasecmp(table_alias_charset, item_field->table_name, 
                           table_name) &&
            (!db_name || (item_field->db_name &&
                          !strcmp(item_field->db_name, db_name))))
        {
          if (found_unaliased)
          {
            if ((*found_unaliased)->eq(item, 0))
              continue;
            /*
              Two matching fields in select list.
              We already can bail out because we are searching through
              unaliased names only and will have duplicate error anyway.
            */
            if (report_error != IGNORE_ERRORS)
              my_error(ER_NON_UNIQ_ERROR, MYF(0),
                       find->full_name(), current_thd->where);
            return (Item**) 0;
          }
          found_unaliased= li.ref();
          unaliased_counter= i;
          *resolution= RESOLVED_IGNORING_ALIAS;
          if (db_name)
            break;                              // Perfect match
        }
      }
      else
      {
        int fname_cmp= my_strcasecmp(system_charset_info,
                                     item_field->field_name,
                                     field_name);
        if (item_field->item_name.eq_safe(field_name))
        {
          /*
            If table name was not given we should scan through aliases
            and non-aliased fields first. We are also checking unaliased
            name of the field in then next  else-if, to be able to find
            instantly field (hidden by alias) if no suitable alias or
            non-aliased field was found.
          */
          if (found)
          {
            if ((*found)->eq(item, 0))
              continue;                           // Same field twice
            if (report_error != IGNORE_ERRORS)
              my_error(ER_NON_UNIQ_ERROR, MYF(0),
                       find->full_name(), current_thd->where);
            return (Item**) 0;
          }
          found= li.ref();
          *counter= i;
          *resolution= fname_cmp ? RESOLVED_AGAINST_ALIAS:
	                           RESOLVED_WITH_NO_ALIAS;
        }
        else if (!fname_cmp)
        {
          /*
            We will use non-aliased field or react on such ambiguities only if
            we won't be able to find aliased field.
            Again if we have ambiguity with field outside of select list
            we should prefer fields from select list.
          */
          if (found_unaliased)
          {
            if ((*found_unaliased)->eq(item, 0))
              continue;                           // Same field twice
            found_unaliased_non_uniq= 1;
          }
          found_unaliased= li.ref();
          unaliased_counter= i;
        }
      }
    }
    else if (!table_name)
    { 
      if (is_ref_by_name && item->item_name.eq_safe(find->item_name))
      {
        found= li.ref();
        *counter= i;
        *resolution= RESOLVED_AGAINST_ALIAS;
        break;
      }
      else if (find->eq(item,0))
      {
        found= li.ref();
        *counter= i;
        *resolution= RESOLVED_IGNORING_ALIAS;
        break;
      }
    }
    else if (table_name && item->type() == Item::REF_ITEM &&
             ((Item_ref *)item)->ref_type() == Item_ref::VIEW_REF)
    {
      /*
        TODO:Here we process prefixed view references only. What we should 
        really do is process all types of Item_refs. But this will currently 
        lead to a clash with the way references to outer SELECTs (from the 
        HAVING clause) are handled in e.g. :
        SELECT 1 FROM t1 AS t1_o GROUP BY a
          HAVING (SELECT t1_o.a FROM t1 AS t1_i GROUP BY t1_i.a LIMIT 1).
        Processing all Item_refs here will cause t1_o.a to resolve to itself.
        We still need to process the special case of Item_direct_view_ref 
        because in the context of views they have the same meaning as 
        Item_field for tables.
      */
      Item_ident *item_ref= (Item_ident *) item;
      if (item_ref->item_name.eq_safe(field_name) &&
          item_ref->table_name &&
          !my_strcasecmp(table_alias_charset, item_ref->table_name,
                         table_name) &&
          (!db_name || (item_ref->db_name && 
                        !strcmp (item_ref->db_name, db_name))))
      {
        found= li.ref();
        *counter= i;
        *resolution= RESOLVED_IGNORING_ALIAS;
        break;
      }
    }
  }
  if (!found)
  {
    if (found_unaliased_non_uniq)
    {
      if (report_error != IGNORE_ERRORS)
        my_error(ER_NON_UNIQ_ERROR, MYF(0),
                 find->full_name(), current_thd->where);
      return (Item **) 0;
    }
    if (found_unaliased)
    {
      found= found_unaliased;
      *counter= unaliased_counter;
      *resolution= RESOLVED_BEHIND_ALIAS;
    }
  }
  if (found)
    return found;
  if (report_error != REPORT_EXCEPT_NOT_FOUND)
  {
    if (report_error == REPORT_ALL_ERRORS)
      my_error(ER_BAD_FIELD_ERROR, MYF(0),
               find->full_name(), current_thd->where);
    return (Item **) 0;
  }
  else
    return (Item **) not_found_item;
}


/*
  Test if a string is a member of a list of strings.

  SYNOPSIS
    test_if_string_in_list()
    find      the string to look for
    str_list  a list of strings to be searched

  DESCRIPTION
    Sequentially search a list of strings for a string, and test whether
    the list contains the same string.

  RETURN
    TRUE  if find is in str_list
    FALSE otherwise
*/

static bool
test_if_string_in_list(const char *find, List<String> *str_list)
{
  List_iterator<String> str_list_it(*str_list);
  String *curr_str;
  size_t find_length= strlen(find);
  while ((curr_str= str_list_it++))
  {
    if (find_length != curr_str->length())
      continue;
    if (!my_strcasecmp(system_charset_info, find, curr_str->ptr()))
      return TRUE;
  }
  return FALSE;
}


/*
  Create a new name resolution context for an item so that it is
  being resolved in a specific table reference.

  SYNOPSIS
    set_new_item_local_context()
    thd        pointer to current thread
    item       item for which new context is created and set
    table_ref  table ref where an item showld be resolved

  DESCRIPTION
    Create a new name resolution context for an item, so that the item
    is resolved only the supplied 'table_ref'.

  RETURN
    FALSE  if all OK
    TRUE   otherwise
*/

static bool
set_new_item_local_context(THD *thd, Item_ident *item, TABLE_LIST *table_ref)
{
  Name_resolution_context *context;
  if (!(context= new (thd->mem_root) Name_resolution_context))
    return TRUE;
  context->init();
  context->first_name_resolution_table=
    context->last_name_resolution_table= table_ref;
  item->context= context;
  return FALSE;
}


/*
  Find and mark the common columns of two table references.

  SYNOPSIS
    mark_common_columns()
    thd                [in] current thread
    table_ref_1        [in] the first (left) join operand
    table_ref_2        [in] the second (right) join operand
    using_fields       [in] if the join is JOIN...USING - the join columns,
                            if NATURAL join, then NULL
    found_using_fields [out] number of fields from the USING clause that were
                             found among the common fields

  DESCRIPTION
    The procedure finds the common columns of two relations (either
    tables or intermediate join results), and adds an equi-join condition
    to the ON clause of 'table_ref_2' for each pair of matching columns.
    If some of table_ref_XXX represents a base table or view, then we
    create new 'Natural_join_column' instances for each column
    reference and store them in the 'join_columns' of the table
    reference.

  IMPLEMENTATION
    The procedure assumes that store_natural_using_join_columns() was
    called for the previous level of NATURAL/USING joins.

  RETURN
    TRUE   error when some common column is non-unique, or out of memory
    FALSE  OK
*/

static bool
mark_common_columns(THD *thd, TABLE_LIST *table_ref_1, TABLE_LIST *table_ref_2,
                    List<String> *using_fields, uint *found_using_fields)
{
  Field_iterator_table_ref it_1, it_2;
  Natural_join_column *nj_col_1, *nj_col_2;
  Query_arena *arena, backup;
  bool result= TRUE;
  bool first_outer_loop= TRUE;
  /*
    Leaf table references to which new natural join columns are added
    if the leaves are != NULL.
  */
  TABLE_LIST *leaf_1= (table_ref_1->nested_join &&
                       !table_ref_1->is_natural_join) ?
                      NULL : table_ref_1;
  TABLE_LIST *leaf_2= (table_ref_2->nested_join &&
                       !table_ref_2->is_natural_join) ?
                      NULL : table_ref_2;

  DBUG_ENTER("mark_common_columns");
  DBUG_PRINT("info", ("operand_1: %s  operand_2: %s",
                      table_ref_1->alias, table_ref_2->alias));

  *found_using_fields= 0;
  arena= thd->activate_stmt_arena_if_needed(&backup);

  for (it_1.set(table_ref_1); !it_1.end_of_fields(); it_1.next())
  {
    bool found= FALSE;
    const char *field_name_1;
    /* true if field_name_1 is a member of using_fields */
    bool is_using_column_1;
    if (!(nj_col_1= it_1.get_or_create_column_ref(thd, leaf_1)))
      goto err;
    field_name_1= nj_col_1->name();
    is_using_column_1= using_fields && 
      test_if_string_in_list(field_name_1, using_fields);
    DBUG_PRINT ("info", ("field_name_1=%s.%s", 
                         nj_col_1->table_name() ? nj_col_1->table_name() : "", 
                         field_name_1));

    /*
      Find a field with the same name in table_ref_2.

      Note that for the second loop, it_2.set() will iterate over
      table_ref_2->join_columns and not generate any new elements or
      lists.
    */
    nj_col_2= NULL;
    for (it_2.set(table_ref_2); !it_2.end_of_fields(); it_2.next())
    {
      Natural_join_column *cur_nj_col_2;
      const char *cur_field_name_2;
      if (!(cur_nj_col_2= it_2.get_or_create_column_ref(thd, leaf_2)))
        goto err;
      cur_field_name_2= cur_nj_col_2->name();
      DBUG_PRINT ("info", ("cur_field_name_2=%s.%s", 
                           cur_nj_col_2->table_name() ? 
                             cur_nj_col_2->table_name() : "", 
                           cur_field_name_2));

      /*
        Compare the two columns and check for duplicate common fields.
        A common field is duplicate either if it was already found in
        table_ref_2 (then found == TRUE), or if a field in table_ref_2
        was already matched by some previous field in table_ref_1
        (then cur_nj_col_2->is_common == TRUE).
        Note that it is too early to check the columns outside of the
        USING list for ambiguity because they are not actually "referenced"
        here. These columns must be checked only on unqualified reference 
        by name (e.g. in SELECT list).
      */
      if (!my_strcasecmp(system_charset_info, field_name_1, cur_field_name_2))
      {
        DBUG_PRINT ("info", ("match c1.is_common=%d", nj_col_1->is_common));
        if (cur_nj_col_2->is_common ||
            (found && (!using_fields || is_using_column_1)))
        {
          my_error(ER_NON_UNIQ_ERROR, MYF(0), field_name_1, thd->where);
          goto err;
        }
        nj_col_2= cur_nj_col_2;
        found= TRUE;
      }
    }
    if (first_outer_loop && leaf_2)
    {
      /*
        Make sure that the next inner loop "knows" that all columns
        are materialized already.
      */
      leaf_2->is_join_columns_complete= TRUE;
      first_outer_loop= FALSE;
    }
    if (!found)
      continue;                                 // No matching field

    /*
      field_1 and field_2 have the same names. Check if they are in the USING
      clause (if present), mark them as common fields, and add a new
      equi-join condition to the ON clause.
    */
    if (nj_col_2 && (!using_fields ||is_using_column_1))
    {
      Item *item_1=   nj_col_1->create_item(thd);
      Item *item_2=   nj_col_2->create_item(thd);
      Field *field_1= nj_col_1->field();
      Field *field_2= nj_col_2->field();
      Item_ident *item_ident_1, *item_ident_2;
      Item_func_eq *eq_cond;

      if (!item_1 || !item_2)
        goto err;                               // out of memory

      /*
        The following assert checks that the two created items are of
        type Item_ident.
      */
      DBUG_ASSERT(!thd->lex->current_select->no_wrap_view_item);
      /*
        In the case of no_wrap_view_item == 0, the created items must be
        of sub-classes of Item_ident.
      */
      DBUG_ASSERT(item_1->type() == Item::FIELD_ITEM ||
                  item_1->type() == Item::REF_ITEM);
      DBUG_ASSERT(item_2->type() == Item::FIELD_ITEM ||
                  item_2->type() == Item::REF_ITEM);

      /*
        We need to cast item_1,2 to Item_ident, because we need to hook name
        resolution contexts specific to each item.
      */
      item_ident_1= (Item_ident*) item_1;
      item_ident_2= (Item_ident*) item_2;
      /*
        Create and hook special name resolution contexts to each item in the
        new join condition . We need this to both speed-up subsequent name
        resolution of these items, and to enable proper name resolution of
        the items during the execute phase of PS.
      */
      if (set_new_item_local_context(thd, item_ident_1, nj_col_1->table_ref) ||
          set_new_item_local_context(thd, item_ident_2, nj_col_2->table_ref))
        goto err;

      if (!(eq_cond= new Item_func_eq(item_ident_1, item_ident_2)))
        goto err;                               /* Out of memory. */

      /*
        Add the new equi-join condition to the ON clause. Notice that
        fix_fields() is applied to all ON conditions in setup_conds()
        so we don't do it here.
       */
      add_join_on((table_ref_1->outer_join & JOIN_TYPE_RIGHT ?
                   table_ref_1 : table_ref_2),
                  eq_cond);

      nj_col_1->is_common= nj_col_2->is_common= TRUE;
      DBUG_PRINT ("info", ("%s.%s and %s.%s are common", 
                           nj_col_1->table_name() ? 
                             nj_col_1->table_name() : "", 
                           nj_col_1->name(),
                           nj_col_2->table_name() ? 
                             nj_col_2->table_name() : "", 
                           nj_col_2->name()));

      if (field_1)
      {
        TABLE *table_1= nj_col_1->table_ref->table;
        /* Mark field_1 used for table cache. */
        bitmap_set_bit(table_1->read_set, field_1->field_index);
        table_1->covering_keys.intersect(field_1->part_of_key);
        table_1->merge_keys.merge(field_1->part_of_key);
      }
      if (field_2)
      {
        TABLE *table_2= nj_col_2->table_ref->table;
        /* Mark field_2 used for table cache. */
        bitmap_set_bit(table_2->read_set, field_2->field_index);
        table_2->covering_keys.intersect(field_2->part_of_key);
        table_2->merge_keys.merge(field_2->part_of_key);
      }

      if (using_fields != NULL)
        ++(*found_using_fields);
    }
  }
  if (leaf_1)
    leaf_1->is_join_columns_complete= TRUE;

  /*
    Everything is OK.
    Notice that at this point there may be some column names in the USING
    clause that are not among the common columns. This is an SQL error and
    we check for this error in store_natural_using_join_columns() when
    (found_using_fields < length(join_using_fields)).
  */
  result= FALSE;

err:
  if (arena)
    thd->restore_active_arena(arena, &backup);
  DBUG_RETURN(result);
}



/*
  Materialize and store the row type of NATURAL/USING join.

  SYNOPSIS
    store_natural_using_join_columns()
    thd                current thread
    natural_using_join the table reference of the NATURAL/USING join
    table_ref_1        the first (left) operand (of a NATURAL/USING join).
    table_ref_2        the second (right) operand (of a NATURAL/USING join).
    using_fields       if the join is JOIN...USING - the join columns,
                       if NATURAL join, then NULL
    found_using_fields number of fields from the USING clause that were
                       found among the common fields

  DESCRIPTION
    Iterate over the columns of both join operands and sort and store
    all columns into the 'join_columns' list of natural_using_join
    where the list is formed by three parts:
      part1: The coalesced columns of table_ref_1 and table_ref_2,
             sorted according to the column order of the first table.
      part2: The other columns of the first table, in the order in
             which they were defined in CREATE TABLE.
      part3: The other columns of the second table, in the order in
             which they were defined in CREATE TABLE.
    Time complexity - O(N1+N2), where Ni = length(table_ref_i).

  IMPLEMENTATION
    The procedure assumes that mark_common_columns() has been called
    for the join that is being processed.

  RETURN
    TRUE    error: Some common column is ambiguous
    FALSE   OK
*/

static bool
store_natural_using_join_columns(THD *thd, TABLE_LIST *natural_using_join,
                                 TABLE_LIST *table_ref_1,
                                 TABLE_LIST *table_ref_2,
                                 List<String> *using_fields,
                                 uint found_using_fields)
{
  Field_iterator_table_ref it_1, it_2;
  Natural_join_column *nj_col_1, *nj_col_2;
  Query_arena *arena, backup;
  bool result= TRUE;
  List<Natural_join_column> *non_join_columns;
  DBUG_ENTER("store_natural_using_join_columns");

  DBUG_ASSERT(!natural_using_join->join_columns);

  arena= thd->activate_stmt_arena_if_needed(&backup);

  if (!(non_join_columns= new List<Natural_join_column>) ||
      !(natural_using_join->join_columns= new List<Natural_join_column>))
    goto err;

  /* Append the columns of the first join operand. */
  for (it_1.set(table_ref_1); !it_1.end_of_fields(); it_1.next())
  {
    nj_col_1= it_1.get_natural_column_ref();
    if (nj_col_1->is_common)
    {
      natural_using_join->join_columns->push_back(nj_col_1);
      /* Reset the common columns for the next call to mark_common_columns. */
      nj_col_1->is_common= FALSE;
    }
    else
      non_join_columns->push_back(nj_col_1);
  }

  /*
    Check that all columns in the USING clause are among the common
    columns. If this is not the case, report the first one that was
    not found in an error.
  */
  if (using_fields && found_using_fields < using_fields->elements)
  {
    String *using_field_name;
    List_iterator_fast<String> using_fields_it(*using_fields);
    while ((using_field_name= using_fields_it++))
    {
      const char *using_field_name_ptr= using_field_name->c_ptr();
      List_iterator_fast<Natural_join_column>
        it(*(natural_using_join->join_columns));
      Natural_join_column *common_field;

      for (;;)
      {
        /* If reached the end of fields, and none was found, report error. */
        if (!(common_field= it++))
        {
          my_error(ER_BAD_FIELD_ERROR, MYF(0), using_field_name_ptr,
                   current_thd->where);
          goto err;
        }
        if (!my_strcasecmp(system_charset_info,
                           common_field->name(), using_field_name_ptr))
          break;                                // Found match
      }
    }
  }

  /* Append the non-equi-join columns of the second join operand. */
  for (it_2.set(table_ref_2); !it_2.end_of_fields(); it_2.next())
  {
    nj_col_2= it_2.get_natural_column_ref();
    if (!nj_col_2->is_common)
      non_join_columns->push_back(nj_col_2);
    else
    {
      /* Reset the common columns for the next call to mark_common_columns. */
      nj_col_2->is_common= FALSE;
    }
  }

  if (non_join_columns->elements > 0)
    natural_using_join->join_columns->concat(non_join_columns);
  natural_using_join->is_join_columns_complete= TRUE;

  result= FALSE;

err:
  if (arena)
    thd->restore_active_arena(arena, &backup);
  DBUG_RETURN(result);
}


/*
  Precompute and store the row types of the top-most NATURAL/USING joins.

  SYNOPSIS
    store_top_level_join_columns()
    thd            current thread
    table_ref      nested join or table in a FROM clause
    left_neighbor  neighbor table reference to the left of table_ref at the
                   same level in the join tree
    right_neighbor neighbor table reference to the right of table_ref at the
                   same level in the join tree

  DESCRIPTION
    The procedure performs a post-order traversal of a nested join tree
    and materializes the row types of NATURAL/USING joins in a
    bottom-up manner until it reaches the TABLE_LIST elements that
    represent the top-most NATURAL/USING joins. The procedure should be
    applied to each element of SELECT_LEX::top_join_list (i.e. to each
    top-level element of the FROM clause).

  IMPLEMENTATION
    Notice that the table references in the list nested_join->join_list
    are in reverse order, thus when we iterate over it, we are moving
    from the right to the left in the FROM clause.

  RETURN
    TRUE   Error
    FALSE  OK
*/

static bool
store_top_level_join_columns(THD *thd, TABLE_LIST *table_ref,
                             TABLE_LIST *left_neighbor,
                             TABLE_LIST *right_neighbor)
{
  Query_arena *arena, backup;
  bool result= TRUE;

  DBUG_ENTER("store_top_level_join_columns");

  arena= thd->activate_stmt_arena_if_needed(&backup);

  /* Call the procedure recursively for each nested table reference. */
  if (table_ref->nested_join)
  {
    List_iterator_fast<TABLE_LIST> nested_it(table_ref->nested_join->join_list);
    TABLE_LIST *same_level_left_neighbor= nested_it++;
    TABLE_LIST *same_level_right_neighbor= NULL;
    /* Left/right-most neighbors, possibly at higher levels in the join tree. */
    TABLE_LIST *real_left_neighbor, *real_right_neighbor;

    while (same_level_left_neighbor)
    {
      TABLE_LIST *cur_table_ref= same_level_left_neighbor;
      same_level_left_neighbor= nested_it++;
      /*
        The order of RIGHT JOIN operands is reversed in 'join list' to
        transform it into a LEFT JOIN. However, in this procedure we need
        the join operands in their lexical order, so below we reverse the
        join operands. Notice that this happens only in the first loop,
        and not in the second one, as in the second loop
        same_level_left_neighbor == NULL.
        This is the correct behavior, because the second loop sets
        cur_table_ref reference correctly after the join operands are
        swapped in the first loop.
      */
      if (same_level_left_neighbor &&
          cur_table_ref->outer_join & JOIN_TYPE_RIGHT)
      {
        /* This can happen only for JOIN ... ON. */
        DBUG_ASSERT(table_ref->nested_join->join_list.elements == 2);
        swap_variables(TABLE_LIST*, same_level_left_neighbor, cur_table_ref);
      }

      /*
        Pick the parent's left and right neighbors if there are no immediate
        neighbors at the same level.
      */
      real_left_neighbor=  (same_level_left_neighbor) ?
                           same_level_left_neighbor : left_neighbor;
      real_right_neighbor= (same_level_right_neighbor) ?
                           same_level_right_neighbor : right_neighbor;

      if (cur_table_ref->nested_join &&
          store_top_level_join_columns(thd, cur_table_ref,
                                       real_left_neighbor, real_right_neighbor))
        goto err;
      same_level_right_neighbor= cur_table_ref;
    }
  }

  /*
    If this is a NATURAL/USING join, materialize its result columns and
    convert to a JOIN ... ON.
  */
  if (table_ref->is_natural_join)
  {
    DBUG_ASSERT(table_ref->nested_join &&
                table_ref->nested_join->join_list.elements == 2);
    List_iterator_fast<TABLE_LIST> operand_it(table_ref->nested_join->join_list);
    /*
      Notice that the order of join operands depends on whether table_ref
      represents a LEFT or a RIGHT join. In a RIGHT join, the operands are
      in inverted order.
     */
    TABLE_LIST *table_ref_2= operand_it++; /* Second NATURAL join operand.*/
    TABLE_LIST *table_ref_1= operand_it++; /* First NATURAL join operand. */
    List<String> *using_fields= table_ref->join_using_fields;
    uint found_using_fields;

    /*
      The two join operands were interchanged in the parser, change the order
      back for 'mark_common_columns'.
    */
    if (table_ref_2->outer_join & JOIN_TYPE_RIGHT)
      swap_variables(TABLE_LIST*, table_ref_1, table_ref_2);
    if (mark_common_columns(thd, table_ref_1, table_ref_2,
                            using_fields, &found_using_fields))
      goto err;

    /*
      Swap the join operands back, so that we pick the columns of the second
      one as the coalesced columns. In this way the coalesced columns are the
      same as of an equivalent LEFT JOIN.
    */
    if (table_ref_1->outer_join & JOIN_TYPE_RIGHT)
      swap_variables(TABLE_LIST*, table_ref_1, table_ref_2);
    if (store_natural_using_join_columns(thd, table_ref, table_ref_1,
                                         table_ref_2, using_fields,
                                         found_using_fields))
      goto err;

    /*
      Change NATURAL JOIN to JOIN ... ON. We do this for both operands
      because either one of them or the other is the one with the
      natural join flag because RIGHT joins are transformed into LEFT,
      and the two tables may be reordered.
    */
    table_ref_1->natural_join= table_ref_2->natural_join= NULL;

    /* Add a TRUE condition to outer joins that have no common columns. */
    if (table_ref_2->outer_join &&
        !table_ref_1->join_cond() && !table_ref_2->join_cond())
      table_ref_2->set_join_cond(new Item_int((longlong) 1,1)); // Always true.

    /* Change this table reference to become a leaf for name resolution. */
    if (left_neighbor)
    {
      TABLE_LIST *last_leaf_on_the_left;
      last_leaf_on_the_left= left_neighbor->last_leaf_for_name_resolution();
      last_leaf_on_the_left->next_name_resolution_table= table_ref;
    }
    if (right_neighbor)
    {
      TABLE_LIST *first_leaf_on_the_right;
      first_leaf_on_the_right= right_neighbor->first_leaf_for_name_resolution();
      table_ref->next_name_resolution_table= first_leaf_on_the_right;
    }
    else
      table_ref->next_name_resolution_table= NULL;
  }
  result= FALSE; /* All is OK. */

err:
  if (arena)
    thd->restore_active_arena(arena, &backup);
  DBUG_RETURN(result);
}


/*
  Compute and store the row types of the top-most NATURAL/USING joins
  in a FROM clause.

  SYNOPSIS
    setup_natural_join_row_types()
    thd          current thread
    from_clause  list of top-level table references in a FROM clause

  DESCRIPTION
    Apply the procedure 'store_top_level_join_columns' to each of the
    top-level table referencs of the FROM clause. Adjust the list of tables
    for name resolution - context->first_name_resolution_table to the
    top-most, lef-most NATURAL/USING join.

  IMPLEMENTATION
    Notice that the table references in 'from_clause' are in reverse
    order, thus when we iterate over it, we are moving from the right
    to the left in the FROM clause.

  RETURN
    TRUE   Error
    FALSE  OK
*/
static bool setup_natural_join_row_types(THD *thd,
                                         List<TABLE_LIST> *from_clause,
                                         Name_resolution_context *context)
{
  DBUG_ENTER("setup_natural_join_row_types");
  thd->where= "from clause";
  if (from_clause->elements == 0)
    DBUG_RETURN(false); /* We come here in the case of UNIONs. */

  List_iterator_fast<TABLE_LIST> table_ref_it(*from_clause);
  TABLE_LIST *table_ref; /* Current table reference. */
  /* Table reference to the left of the current. */
  TABLE_LIST *left_neighbor;
  /* Table reference to the right of the current. */
  TABLE_LIST *right_neighbor= NULL;

  /* Note that tables in the list are in reversed order */
  for (left_neighbor= table_ref_it++; left_neighbor ; )
  {
    table_ref= left_neighbor;
    left_neighbor= table_ref_it++;
    /* 
      Do not redo work if already done:
      1) for stored procedures,
      2) for multitable update after lock failure and table reopening.
    */
    if (context->select_lex->first_natural_join_processing)
    {
      if (store_top_level_join_columns(thd, table_ref,
                                       left_neighbor, right_neighbor))
        DBUG_RETURN(true);
      if (left_neighbor)
      {
        TABLE_LIST *first_leaf_on_the_right;
        first_leaf_on_the_right= table_ref->first_leaf_for_name_resolution();
        left_neighbor->next_name_resolution_table= first_leaf_on_the_right;
      }
    }
    right_neighbor= table_ref;
  }

  /*
    Store the top-most, left-most NATURAL/USING join, so that we start
    the search from that one instead of context->table_list. At this point
    right_neighbor points to the left-most top-level table reference in the
    FROM clause.
  */
  DBUG_ASSERT(right_neighbor);
  context->first_name_resolution_table=
    right_neighbor->first_leaf_for_name_resolution();
  context->select_lex->first_natural_join_processing= false;

  DBUG_RETURN (false);
}


/****************************************************************************
** Expand all '*' in given fields
****************************************************************************/

int setup_wild(THD *thd, TABLE_LIST *tables, List<Item> &fields,
	       List<Item> *sum_func_list,
	       uint wild_num)
{
  if (!wild_num)
    return(0);

  Item *item;
  List_iterator<Item> it(fields);
  Query_arena *arena, backup;
  DBUG_ENTER("setup_wild");

  /*
    Don't use arena if we are not in prepared statements or stored procedures
    For PS/SP we have to use arena to remember the changes
  */
  arena= thd->activate_stmt_arena_if_needed(&backup);

  // When we enter, we're "nowhere":
  DBUG_ASSERT(thd->lex->current_select->cur_pos_in_all_fields ==
              SELECT_LEX::ALL_FIELDS_UNDEF_POS);
  // Now we're in the SELECT list:
  thd->lex->current_select->cur_pos_in_all_fields= 0;
  while (wild_num && (item= it++))
  {
    if (item->type() == Item::FIELD_ITEM &&
        ((Item_field*) item)->field_name &&
	((Item_field*) item)->field_name[0] == '*' &&
	!((Item_field*) item)->field)
    {
      uint elem= fields.elements;
      bool any_privileges= ((Item_field *) item)->any_privileges;
      Item_subselect *subsel= thd->lex->current_select->master_unit()->item;
      if (subsel &&
          subsel->substype() == Item_subselect::EXISTS_SUBS)
      {
        /*
          It is EXISTS(SELECT * ...) and we can replace * by any constant.

          Item_int do not need fix_fields() because it is basic constant.
        */
        it.replace(new Item_int(NAME_STRING("Not_used"), (longlong) 1,
                                MY_INT64_NUM_DECIMAL_DIGITS));
      }
      else if (insert_fields(thd, ((Item_field*) item)->context,
                             ((Item_field*) item)->db_name,
                             ((Item_field*) item)->table_name, &it,
                             any_privileges))
      {
	if (arena)
	  thd->restore_active_arena(arena, &backup);
	DBUG_RETURN(-1);
      }
      if (sum_func_list)
      {
	/*
	  sum_func_list is a list that has the fields list as a tail.
	  Because of this we have to update the element count also for this
	  list after expanding the '*' entry.
	*/
	sum_func_list->elements+= fields.elements - elem;
      }
      wild_num--;
    }
    else
      thd->lex->current_select->cur_pos_in_all_fields++;
  }
  // We're nowhere again:
  thd->lex->current_select->cur_pos_in_all_fields=
    SELECT_LEX::ALL_FIELDS_UNDEF_POS;

  if (arena)
  {
    /* make * substituting permanent */
    SELECT_LEX *select_lex= thd->lex->current_select;
    select_lex->with_wild= 0;
    /*   
      The assignment below is translated to memcpy() call (at least on some
      platforms). memcpy() expects that source and destination areas do not
      overlap. That problem was detected by valgrind. 
    */
    if (&select_lex->item_list != &fields)
      select_lex->item_list= fields;

    thd->restore_active_arena(arena, &backup);
  }
  DBUG_RETURN(0);
}

/****************************************************************************
** Check that all given fields exists and fill struct with current data
****************************************************************************/

bool setup_fields(THD *thd, Ref_ptr_array ref_pointer_array,
                  List<Item> &fields, enum_mark_columns mark_used_columns,
                  List<Item> *sum_func_list, bool allow_sum_func)
{
  reg2 Item *item;
  enum_mark_columns save_mark_used_columns= thd->mark_used_columns;
  nesting_map save_allow_sum_func= thd->lex->allow_sum_func;
  List_iterator<Item> it(fields);
  bool save_is_item_list_lookup;
  DBUG_ENTER("setup_fields");

  thd->mark_used_columns= mark_used_columns;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
  if (allow_sum_func)
    thd->lex->allow_sum_func|= 1 << thd->lex->current_select->nest_level;
  thd->where= THD::DEFAULT_WHERE;
  save_is_item_list_lookup= thd->lex->current_select->is_item_list_lookup;
  thd->lex->current_select->is_item_list_lookup= 0;

  /*
    To prevent fail on forward lookup we fill it with zerows,
    then if we got pointer on zero after find_item_in_list we will know
    that it is forward lookup.

    There is other way to solve problem: fill array with pointers to list,
    but it will be slower.

    TODO: remove it when (if) we made one list for allfields and
    ref_pointer_array
  */
  if (!ref_pointer_array.is_null())
  {
    DBUG_ASSERT(ref_pointer_array.size() >= fields.elements);
    memset(ref_pointer_array.array(), 0, sizeof(Item *) * fields.elements);
  }

  /*
    We call set_entry() there (before fix_fields() of the whole list of field
    items) because:
    1) the list of field items has same order as in the query, and the
       Item_func_get_user_var item may go before the Item_func_set_user_var:
          SELECT @a, @a := 10 FROM t;
    2) The entry->update_query_id value controls constantness of
       Item_func_get_user_var items, so in presence of Item_func_set_user_var
       items we have to refresh their entries before fixing of
       Item_func_get_user_var items.
  */
  List_iterator<Item_func_set_user_var> li(thd->lex->set_var_list);
  Item_func_set_user_var *var;
  while ((var= li++))
    var->set_entry(thd, FALSE);

  Ref_ptr_array ref= ref_pointer_array;
  DBUG_ASSERT(thd->lex->current_select->cur_pos_in_all_fields ==
              SELECT_LEX::ALL_FIELDS_UNDEF_POS);
  thd->lex->current_select->cur_pos_in_all_fields= 0;
  while ((item= it++))
  {
    if ((!item->fixed && item->fix_fields(thd, it.ref())) ||
	(item= *(it.ref()))->check_cols(1))
    {
      thd->lex->current_select->is_item_list_lookup= save_is_item_list_lookup;
      thd->lex->allow_sum_func= save_allow_sum_func;
      thd->mark_used_columns= save_mark_used_columns;
      DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
      DBUG_RETURN(TRUE); /* purecov: inspected */
    }
    if (!ref.is_null())
    {
      ref[0]= item;
      ref.pop_front();
    }
    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM &&
	sum_func_list)
      item->split_sum_func(thd, ref_pointer_array, *sum_func_list);
    thd->lex->current_select->select_list_tables|= item->used_tables();
    thd->lex->used_tables|= item->used_tables();
    thd->lex->current_select->cur_pos_in_all_fields++;
  }
  thd->lex->current_select->is_item_list_lookup= save_is_item_list_lookup;
  thd->lex->current_select->cur_pos_in_all_fields=
    SELECT_LEX::ALL_FIELDS_UNDEF_POS;

  thd->lex->allow_sum_func= save_allow_sum_func;
  thd->mark_used_columns= save_mark_used_columns;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
  DBUG_RETURN(test(thd->is_error()));
}


/*
  make list of leaves of join table tree

  SYNOPSIS
    make_leaves_list()
    list    pointer to pointer on list first element
    tables  table list

  RETURN pointer on pointer to next_leaf of last element
*/

TABLE_LIST **make_leaves_list(TABLE_LIST **list, TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table; table= table->next_local)
  {
    if (table->merge_underlying_list)
    {
      DBUG_ASSERT(table->view &&
                  table->effective_algorithm == VIEW_ALGORITHM_MERGE);
      list= make_leaves_list(list, table->merge_underlying_list);
    }
    else
    {
      *list= table;
      list= &table->next_leaf;
    }
  }
  return list;
}

/*
  prepare tables

  SYNOPSIS
    setup_tables()
    thd		  Thread handler
    context       name resolution contest to setup table list there
    from_clause   Top-level list of table references in the FROM clause
    tables	  Table list (select_lex->table_list)
    leaves        List of join table leaves list (select_lex->leaf_tables)
    refresh       It is onle refresh for subquery
    select_insert It is SELECT ... INSERT command

  NOTE
    Check also that the 'used keys' and 'ignored keys' exists and set up the
    table structure accordingly.
    Create a list of leaf tables. For queries with NATURAL/USING JOINs,
    compute the row types of the top most natural/using join table references
    and link these into a list of table references for name resolution.

    This has to be called for all tables that are used by items, as otherwise
    table->map is not set and all Item_field will be regarded as const items.

  RETURN
    FALSE ok;  In this case *map will includes the chosen index
    TRUE  error
*/

bool setup_tables(THD *thd, Name_resolution_context *context,
                  List<TABLE_LIST> *from_clause, TABLE_LIST *tables,
                  TABLE_LIST **leaves, bool select_insert)
{
  uint tablenr= 0;
  DBUG_ENTER("setup_tables");

  DBUG_ASSERT ((select_insert && !tables->next_name_resolution_table) || !tables || 
               (context->table_list && context->first_name_resolution_table));
  /*
    this is used for INSERT ... SELECT.
    For select we setup tables except first (and its underlying tables)
  */
  TABLE_LIST *first_select_table= (select_insert ?
                                   tables->next_local:
                                   0);
  if (!(*leaves))
    make_leaves_list(leaves, tables);

  TABLE_LIST *table_list;
  for (table_list= *leaves;
       table_list;
       table_list= table_list->next_leaf, tablenr++)
  {
    TABLE *table= table_list->table;
    table->pos_in_table_list= table_list;
    if (first_select_table &&
        table_list->top_table() == first_select_table)
    {
      /* new counting for SELECT of INSERT ... SELECT command */
      first_select_table= 0;
      tablenr= 0;
    }
    setup_table_map(table, table_list, tablenr);
    if (table_list->process_index_hints(table))
      DBUG_RETURN(1);
  }
  if (tablenr > MAX_TABLES)
  {
    my_error(ER_TOO_MANY_TABLES,MYF(0), static_cast<int>(MAX_TABLES));
    DBUG_RETURN(1);
  }
  for (table_list= tables;
       table_list;
       table_list= table_list->next_local)
  {
    if (table_list->merge_underlying_list)
    {
      DBUG_ASSERT(table_list->view &&
                  table_list->effective_algorithm == VIEW_ALGORITHM_MERGE);
      Query_arena *arena= thd->stmt_arena, backup;
      bool res;
      if (arena->is_conventional())
        arena= 0;                                   // For easier test
      else
        thd->set_n_backup_active_arena(arena, &backup);
      res= table_list->setup_underlying(thd);
      if (arena)
        thd->restore_active_arena(arena, &backup);
      if (res)
        DBUG_RETURN(1);
    }
  }

  /* Precompute and store the row types of NATURAL/USING joins. */
  if (setup_natural_join_row_types(thd, from_clause, context))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


/**
  Prepare tables and check access for the view tables.

  @param thd                Thread context.
  @param context            Name resolution contest to setup table list
                            there.
  @param from_clause        Top-level list of table references in the
                            FROM clause.
  @param tables             Table list (select_lex->table_list).
  @param leaves[in/out]     List of join table leaves list
                            (select_lex->leaf_tables).
  @param select_insert      It is SELECT ... INSERT command/
  @param want_access_first  What access is requested of the first leaf.
  @param want_access        What access is requested on the rest of leaves.

  @note A wrapper for check_tables that will also check the resulting
        table leaves list for access to all the tables that belong to
        a view.

  @note Beware that it can't properly check privileges in cases when
        table being changed is not the first table in the list of leaf
        tables (for example, for multi-UPDATE).

  @retval FALSE - Success.
  @retval TRUE  - Error.
*/

bool setup_tables_and_check_access(THD *thd, 
                                   Name_resolution_context *context,
                                   List<TABLE_LIST> *from_clause,
                                   TABLE_LIST *tables,
                                   TABLE_LIST **leaves,
                                   bool select_insert,
                                   ulong want_access_first,
                                   ulong want_access)
{
  TABLE_LIST *leaves_tmp= NULL;
  bool first_table= true;

  if (setup_tables(thd, context, from_clause, tables,
                   &leaves_tmp, select_insert))
    return TRUE;

  if (leaves)
    *leaves= leaves_tmp;

  for (; leaves_tmp; leaves_tmp= leaves_tmp->next_leaf)
  {
    if (leaves_tmp->belong_to_view && 
        check_single_table_access(thd, first_table ? want_access_first :
                                  want_access, leaves_tmp, FALSE))
    {
      tables->hide_view_error(thd);
      return TRUE;
    }
    first_table= 0;
  }
  return FALSE;
}


/*
  Drops in all fields instead of current '*' field

  SYNOPSIS
    insert_fields()
    thd			Thread handler
    context             Context for name resolution
    db_name		Database name in case of 'database_name.table_name.*'
    table_name		Table name in case of 'table_name.*'
    it			Pointer to '*'
    any_privileges	0 If we should ensure that we have SELECT privileges
		          for all columns
                        1 If any privilege is ok
  RETURN
    0	ok     'it' is updated to point at last inserted
    1	error.  Error message is generated but not sent to client
*/

bool
insert_fields(THD *thd, Name_resolution_context *context, const char *db_name,
	      const char *table_name, List_iterator<Item> *it,
              bool any_privileges)
{
  Field_iterator_table_ref field_iterator;
  bool found;
  char name_buff[NAME_LEN+1];
  DBUG_ENTER("insert_fields");
  DBUG_PRINT("arena", ("stmt arena: 0x%lx", (ulong)thd->stmt_arena));

  if (db_name && lower_case_table_names)
  {
    /*
      convert database to lower case for comparison
      We can't do this in Item_field as this would change the
      'name' of the item which may be used in the select list
    */
    strmake(name_buff, db_name, sizeof(name_buff)-1);
    my_casedn_str(files_charset_info, name_buff);
    db_name= name_buff;
  }

  found= FALSE;

  /*
    If table names are qualified, then loop over all tables used in the query,
    else treat natural joins as leaves and do not iterate over their underlying
    tables.
  */
  for (TABLE_LIST *tables= (table_name ? context->table_list :
                            context->first_name_resolution_table);
       tables;
       tables= (table_name ? tables->next_local :
                tables->next_name_resolution_table)
       )
  {
    Field *field;
    TABLE *table= tables->table;

    DBUG_ASSERT(tables->is_leaf_for_name_resolution());

    if ((table_name && my_strcasecmp(table_alias_charset, table_name,
                                    tables->alias)) ||
        (db_name && strcmp(tables->db,db_name)))
      continue;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    /* 
       Ensure that we have access rights to all fields to be inserted. Under
       some circumstances, this check may be skipped.

       - If any_privileges is true, skip the check.

       - If the SELECT privilege has been found as fulfilled already for both
         the TABLE and TABLE_LIST objects (and both of these exist, of
         course), the check is skipped.

       - If the SELECT privilege has been found fulfilled for the TABLE object
         and the TABLE_LIST represents a derived table other than a view (see
         below), the check is skipped.

       - If the TABLE_LIST object represents a view, we may skip checking if
         the SELECT privilege has been found fulfilled for it, regardless of
         the TABLE object.

       - If there is no TABLE object, the test is skipped if either 
         * the TABLE_LIST does not represent a view, or
         * the SELECT privilege has been found fulfilled.         

       A TABLE_LIST that is not a view may be a subquery, an
       information_schema table, or a nested table reference. See the comment
       for TABLE_LIST.
    */
    if (!((table && !tables->view && (table->grant.privilege & SELECT_ACL)) ||
          (tables->view && (tables->grant.privilege & SELECT_ACL))) &&
        !any_privileges)
    {
      field_iterator.set(tables);
      if (check_grant_all_columns(thd, SELECT_ACL, &field_iterator))
        DBUG_RETURN(TRUE);
    }
#endif

    /*
      Update the tables used in the query based on the referenced fields. For
      views and natural joins this update is performed inside the loop below.
    */
    if (table)
    {
      thd->lex->used_tables|= table->map;
      thd->lex->current_select->select_list_tables|= table->map;
    }

    /*
      Initialize a generic field iterator for the current table reference.
      Notice that it is guaranteed that this iterator will iterate over the
      fields of a single table reference, because 'tables' is a leaf (for
      name resolution purposes).
    */
    field_iterator.set(tables);

    for (; !field_iterator.end_of_fields(); field_iterator.next())
    {
      Item *item;

      if (!(item= field_iterator.create_item(thd)))
        DBUG_RETURN(TRUE);
      DBUG_ASSERT(item->fixed);
      /* cache the table for the Item_fields inserted by expanding stars */
      if (item->type() == Item::FIELD_ITEM && tables->cacheable_table)
        ((Item_field *)item)->cached_table= tables;

      if (!found)
      {
        found= TRUE;
        it->replace(item); /* Replace '*' with the first found item. */
      }
      else
        it->after(item);   /* Add 'item' to the SELECT list. */

#ifndef NO_EMBEDDED_ACCESS_CHECKS
      /*
        Set privilege information for the fields of newly created views.
        We have that (any_priviliges == TRUE) if and only if we are creating
        a view. In the time of view creation we can't use the MERGE algorithm,
        therefore if 'tables' is itself a view, it is represented by a
        temporary table. Thus in this case we can be sure that 'item' is an
        Item_field.
      */
      if (any_privileges)
      {
        DBUG_ASSERT((tables->field_translation == NULL && table) ||
                    tables->is_natural_join);
        DBUG_ASSERT(item->type() == Item::FIELD_ITEM);
        Item_field *fld= (Item_field*) item;
        const char *field_table_name= field_iterator.get_table_name();

        if (!tables->schema_table && 
            !(fld->have_privileges=
              (get_column_grant(thd, field_iterator.grant(),
                                field_iterator.get_db_name(),
                                field_table_name, fld->field_name) &
               VIEW_ANY_ACL)))
        {
          my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0), "ANY",
                   thd->security_ctx->priv_user,
                   thd->security_ctx->host_or_ip,
                   field_table_name);
          DBUG_RETURN(TRUE);
        }
      }
#endif

      if ((field= field_iterator.field()))
      {
        /* Mark fields as used to allow storage engine to optimze access */
        bitmap_set_bit(field->table->read_set, field->field_index);
        if (table)
        {
          table->covering_keys.intersect(field->part_of_key);
          table->merge_keys.merge(field->part_of_key);
        }
        if (tables->is_natural_join)
        {
          TABLE *field_table;
          /*
            In this case we are sure that the column ref will not be created
            because it was already created and stored with the natural join.
          */
          Natural_join_column *nj_col;
          if (!(nj_col= field_iterator.get_natural_column_ref()))
            DBUG_RETURN(TRUE);
          DBUG_ASSERT(nj_col->table_field);
          field_table= nj_col->table_ref->table;
          if (field_table)
          {
            thd->lex->used_tables|= field_table->map;
            thd->lex->current_select->select_list_tables|=
              field_table->map;
            field_table->covering_keys.intersect(field->part_of_key);
            field_table->merge_keys.merge(field->part_of_key);
            field_table->used_fields++;
          }
        }
      }
      else
      {
        thd->lex->used_tables|= item->used_tables();
        thd->lex->current_select->select_list_tables|=
          item->used_tables();
      }
      thd->lex->current_select->cur_pos_in_all_fields++;
    }
    /*
      In case of stored tables, all fields are considered as used,
      while in the case of views, the fields considered as used are the
      ones marked in setup_tables during fix_fields of view columns.
      For NATURAL joins, used_tables is updated in the IF above.
    */
    if (table)
      table->used_fields= table->s->fields;
  }
  if (found)
    DBUG_RETURN(FALSE);

  /*
    TODO: in the case when we skipped all columns because there was a
    qualified '*', and all columns were coalesced, we have to give a more
    meaningful message than ER_BAD_TABLE_ERROR.
  */
  if (!table_name || !*table_name)
    my_message(ER_NO_TABLES_USED, ER(ER_NO_TABLES_USED), MYF(0));
  else
  {
    String tbl_name;
    if (db_name)
    {
      tbl_name.append(String(db_name,system_charset_info));
      tbl_name.append('.');
    }
    tbl_name.append(String(table_name,system_charset_info));

    my_error(ER_BAD_TABLE_ERROR, MYF(0), tbl_name.c_ptr_safe());
  }

  DBUG_RETURN(TRUE);
}


/*
  Fix all conditions and outer join expressions.

  SYNOPSIS
    setup_conds()
    thd     thread handler
    tables  list of tables for name resolving (select_lex->table_list)
    leaves  list of leaves of join table tree (select_lex->leaf_tables)
    conds   WHERE clause

  DESCRIPTION
    TODO

  RETURN
    TRUE  if some error occured (e.g. out of memory)
    FALSE if all is OK
*/

int setup_conds(THD *thd, TABLE_LIST *tables, TABLE_LIST *leaves,
                Item **conds)
{
  SELECT_LEX *select_lex= thd->lex->current_select;
  TABLE_LIST *table= NULL;	// For HP compilers
  /*
    it_is_update set to TRUE when tables of primary SELECT_LEX (SELECT_LEX
    which belong to LEX, i.e. most up SELECT) will be updated by
    INSERT/UPDATE/LOAD
    NOTE: using this condition helps to prevent call of prepare_check_option()
    from subquery of VIEW, because tables of subquery belongs to VIEW
    (see condition before prepare_check_option() call)
  */
  bool it_is_update= (select_lex == &thd->lex->select_lex) &&
    thd->lex->which_check_option_applicable();
  bool save_is_item_list_lookup= select_lex->is_item_list_lookup;
  select_lex->is_item_list_lookup= 0;
  DBUG_ENTER("setup_conds");

  thd->mark_used_columns= MARK_COLUMNS_READ;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
  select_lex->cond_count= 0;
  select_lex->between_count= 0;
  select_lex->max_equal_elems= 0;

  for (table= tables; table; table= table->next_local)
  {
    select_lex->resolve_place= st_select_lex::RESOLVE_CONDITION;
    /*
      Walk up tree of join nests and try to find outer join nest.
      This is needed because simplify_joins() has not yet been called,
      and hence inner join nests have not yet been removed.
    */
    for (TABLE_LIST *embedding= table;
         embedding;
         embedding= embedding->embedding)
    {
      if (embedding->outer_join)
      {
        /*
          The join condition belongs to an outer join next.
          Record this fact and the outer join nest for possible transformation
          of subqueries into semi-joins.
        */  
        select_lex->resolve_place= st_select_lex::RESOLVE_JOIN_NEST;
        select_lex->resolve_nest= embedding;
        break;
      }
    }
    if (table->prepare_where(thd, conds, FALSE))
      goto err_no_arena;
    select_lex->resolve_place= st_select_lex::RESOLVE_NONE;
    select_lex->resolve_nest= NULL;
  }

  if (*conds)
  {
    select_lex->resolve_place= st_select_lex::RESOLVE_CONDITION;
    thd->where="where clause";
    if ((!(*conds)->fixed && (*conds)->fix_fields(thd, conds)) ||
	(*conds)->check_cols(1))
      goto err_no_arena;
    select_lex->where= *conds;
    select_lex->resolve_place= st_select_lex::RESOLVE_NONE;
  }

  /*
    Apply fix_fields() to all ON clauses at all levels of nesting,
    including the ones inside view definitions.
  */
  for (table= leaves; table; table= table->next_leaf)
  {
    TABLE_LIST *embedded; /* The table at the current level of nesting. */
    TABLE_LIST *embedding= table; /* The parent nested table reference. */
    do
    {
      embedded= embedding;
      if (embedded->join_cond())
      {
        /* Make a join an a expression */
        select_lex->resolve_place= st_select_lex::RESOLVE_JOIN_NEST;
        select_lex->resolve_nest= embedded;
        thd->where="on clause";
        if ((!embedded->join_cond()->fixed &&
           embedded->join_cond()->fix_fields(thd, embedded->join_cond_ref())) ||
	   embedded->join_cond()->check_cols(1))
	  goto err_no_arena;
        select_lex->cond_count++;
        select_lex->resolve_place= st_select_lex::RESOLVE_NONE;
        select_lex->resolve_nest= NULL;
      }
      embedding= embedded->embedding;
    }
    while (embedding &&
           embedding->nested_join->join_list.head() == embedded);

    /* process CHECK OPTION */
    if (it_is_update)
    {
      TABLE_LIST *view= table->top_table();
      if (view->effective_with_check)
      {
        if (view->prepare_check_option(thd))
          goto err_no_arena;
        thd->change_item_tree(&table->check_option, view->check_option);
      }
    }
  }

  thd->lex->current_select->is_item_list_lookup= save_is_item_list_lookup;
  DBUG_RETURN(test(thd->is_error()));

err_no_arena:
  select_lex->is_item_list_lookup= save_is_item_list_lookup;
  DBUG_RETURN(1);
}


/******************************************************************************
** Fill a record with data (for INSERT or UPDATE)
** Returns : 1 if some field has wrong type
******************************************************************************/


/*
  Fill fields with given items.

  @param thd           thread handler
  @param fields        Item_fields list to be filled
  @param values        values to fill with
  @param ignore_errors TRUE if we should ignore errors
  @param bitmap        Bitmap over fields to fill

  @note fill_record() may set table->auto_increment_field_not_null and a
  caller should make sure that it is reset after their last call to this
  function.

  @return Operation status
    @retval false   OK
    @retval true    Error occured
*/

bool
fill_record(THD * thd, List<Item> &fields, List<Item> &values,
            bool ignore_errors, MY_BITMAP *bitmap)
{
  List_iterator_fast<Item> f(fields),v(values);
  Item *value, *fld;
  Item_field *field;
  TABLE *table= 0;
  DBUG_ENTER("fill_record");
  DBUG_ASSERT(fields.elements == values.elements);
  /*
    Reset the table->auto_increment_field_not_null as it is valid for
    only one row.
  */
  if (fields.elements)
  {
    /*
      On INSERT or UPDATE fields are checked to be from the same table,
      thus we safely can take table from the first field.
    */
    fld= (Item_field*)f++;
    if (!(field= fld->field_for_view_update()))
    {
      my_error(ER_NONUPDATEABLE_COLUMN, MYF(0), fld->item_name.ptr());
      goto err;
    }
    table= field->field->table;
    table->auto_increment_field_not_null= FALSE;
    f.rewind();
  }
  while ((fld= f++))
  {
    if (!(field= fld->field_for_view_update()))
    {
      my_error(ER_NONUPDATEABLE_COLUMN, MYF(0), fld->item_name.ptr());
      goto err;
    }
    value=v++;
    Field *rfield= field->field;
    /* If bitmap over wanted fields are set, skip non marked fields. */
    if (bitmap && !bitmap_is_set(bitmap, rfield->field_index))
      continue;
    table= rfield->table;
    if (rfield == table->next_number_field)
      table->auto_increment_field_not_null= TRUE;
    if ((value->save_in_field(rfield, 0) < 0) && !ignore_errors)
    {
      my_message(ER_UNKNOWN_ERROR, ER(ER_UNKNOWN_ERROR), MYF(0));
      goto err;
    }
  }
  DBUG_RETURN(thd->is_error());
err:
  if (table)
    table->auto_increment_field_not_null= FALSE;
  DBUG_RETURN(TRUE);
}


/*
  Fill fields in list with values from the list of items and invoke
  before triggers.

  SYNOPSIS
    fill_record_n_invoke_before_triggers()
      thd           thread context
      fields        Item_fields list to be filled
      values        values to fill with
      ignore_errors TRUE if we should ignore errors
      triggers      object holding list of triggers to be invoked
      event         event type for triggers to be invoked

  NOTE
    This function assumes that fields which values will be set and triggers
    to be invoked belong to the same table, and that TABLE::record[0] and
    record[1] buffers correspond to new and old versions of row respectively.

  RETURN
    FALSE   OK
    TRUE    error occured
*/

bool
fill_record_n_invoke_before_triggers(THD *thd, List<Item> &fields,
                                     List<Item> &values, bool ignore_errors,
                                     Table_triggers_list *triggers,
                                     enum trg_event_type event)
{
  return (fill_record(thd, fields, values, ignore_errors, NULL) ||
          (triggers && triggers->process_triggers(thd, event,
                                                 TRG_ACTION_BEFORE, TRUE)));
}


/**
  Fill field buffer with values from Field list.

  @param thd           thread handler
  @param ptr           pointer on pointer to record
  @param values        list of fields
  @param ignore_errors True if we should ignore errors
  @param bitmap        Bitmap over fields to fill

  @note fill_record() may set table->auto_increment_field_not_null and a
  caller should make sure that it is reset after their last call to this
  function.

  @return Operation status
    @retval false   OK
    @retval true    Error occured
*/

bool
fill_record(THD *thd, Field **ptr, List<Item> &values, bool ignore_errors,
            MY_BITMAP *bitmap)
{
  List_iterator_fast<Item> v(values);
  Item *value;
  TABLE *table= 0;
  DBUG_ENTER("fill_record");

  Field *field;
  /*
    Reset the table->auto_increment_field_not_null as it is valid for
    only one row.
  */
  if (*ptr)
  {
    /*
      On INSERT or UPDATE fields are checked to be from the same table,
      thus we safely can take table from the first field.
    */
    table= (*ptr)->table;
    table->auto_increment_field_not_null= FALSE;
  }
  while ((field = *ptr++) && ! thd->is_error())
  {
    value=v++;
    table= field->table;
    /* If bitmap over wanted fields are set, skip non marked fields. */
    if (bitmap && !bitmap_is_set(bitmap, field->field_index))
      continue;
    if (field == table->next_number_field)
      table->auto_increment_field_not_null= TRUE;
    if (value->save_in_field(field, 0) == TYPE_ERR_NULL_CONSTRAINT_VIOLATION)
      goto err;
  }
  DBUG_ASSERT(thd->is_error() || !v++);      // No extra value!
  DBUG_RETURN(thd->is_error());

err:
  if (table)
    table->auto_increment_field_not_null= FALSE;
  DBUG_RETURN(TRUE);
}


/*
  Fill fields in array with values from the list of items and invoke
  before triggers.

  SYNOPSIS
    fill_record_n_invoke_before_triggers()
      thd           thread context
      ptr           NULL-ended array of fields to be filled
      values        values to fill with
      ignore_errors TRUE if we should ignore errors
      triggers      object holding list of triggers to be invoked
      event         event type for triggers to be invoked

  NOTE
    This function assumes that fields which values will be set and triggers
    to be invoked belong to the same table, and that TABLE::record[0] and
    record[1] buffers correspond to new and old versions of row respectively.

  RETURN
    FALSE   OK
    TRUE    error occured
*/

bool
fill_record_n_invoke_before_triggers(THD *thd, Field **ptr,
                                     List<Item> &values, bool ignore_errors,
                                     Table_triggers_list *triggers,
                                     enum trg_event_type event)
{
  return (fill_record(thd, ptr, values, ignore_errors, NULL) ||
          (triggers && triggers->process_triggers(thd, event,
                                                 TRG_ACTION_BEFORE, TRUE)));
}


my_bool mysql_rm_tmp_tables(void)
{
  uint i, idx;
  char	filePath[FN_REFLEN], *tmpdir, filePathCopy[FN_REFLEN];
  MY_DIR *dirp;
  FILEINFO *file;
  TABLE_SHARE share;
  THD *thd;
  DBUG_ENTER("mysql_rm_tmp_tables");

  if (!(thd= new THD))
    DBUG_RETURN(1);
  thd->thread_stack= (char*) &thd;
  thd->store_globals();

  for (i=0; i<=mysql_tmpdir_list.max; i++)
  {
    tmpdir=mysql_tmpdir_list.list[i];
    /* See if the directory exists */
    if (!(dirp = my_dir(tmpdir,MYF(MY_WME | MY_DONT_SORT))))
      continue;

    /* Remove all SQLxxx tables from directory */

    for (idx=0 ; idx < (uint) dirp->number_off_files ; idx++)
    {
      file=dirp->dir_entry+idx;

      /* skiping . and .. */
      if (file->name[0] == '.' && (!file->name[1] ||
                                   (file->name[1] == '.' &&  !file->name[2])))
        continue;

      if (!memcmp(file->name, tmp_file_prefix,
                  tmp_file_prefix_length))
      {
        char *ext= fn_ext(file->name);
        uint ext_len= strlen(ext);
        uint filePath_len= my_snprintf(filePath, sizeof(filePath),
                                       "%s%c%s", tmpdir, FN_LIBCHAR,
                                       file->name);
        if (!memcmp(reg_ext, ext, ext_len))
        {
          handler *handler_file= 0;
          /* We should cut file extention before deleting of table */
          memcpy(filePathCopy, filePath, filePath_len - ext_len);
          filePathCopy[filePath_len - ext_len]= 0;
          init_tmp_table_share(thd, &share, "", 0, "", filePathCopy);
          if (!open_table_def(thd, &share, 0) &&
              ((handler_file= get_new_handler(&share, thd->mem_root,
                                              share.db_type()))))
          {
            handler_file->ha_delete_table(filePathCopy);
            delete handler_file;
          }
          free_table_share(&share);
        }
        /*
          File can be already deleted by tmp_table.file->delete_table().
          So we hide error messages which happnes during deleting of these
          files(MYF(0)).
        */
        (void) mysql_file_delete(key_file_misc, filePath, MYF(0));
      }
    }
    my_dirend(dirp);
  }
  delete thd;
  my_pthread_setspecific_ptr(THR_THD,  0);
  DBUG_RETURN(0);
}



/*****************************************************************************
	unireg support functions
*****************************************************************************/

/*
  free all unused tables

  NOTE
    This is called by 'handle_manager' when one wants to periodicly flush
    all not used tables.
*/

void tdc_flush_unused_tables()
{
  table_cache_manager.lock_all_and_tdc();
  table_cache_manager.free_all_unused_tables();
  table_cache_manager.unlock_all_and_tdc();
}


/**
   Remove all or some (depending on parameter) instances of TABLE and
   TABLE_SHARE from the table definition cache.

   @param  thd          Thread context
   @param  remove_type  Type of removal:
                        TDC_RT_REMOVE_ALL     - remove all TABLE instances and
                                                TABLE_SHARE instance. There
                                                should be no used TABLE objects
                                                and caller should have exclusive
                                                metadata lock on the table.
                        TDC_RT_REMOVE_NOT_OWN - remove all TABLE instances
                                                except those that belong to
                                                this thread. There should be
                                                no TABLE objects used by other
                                                threads and caller should have
                                                exclusive metadata lock on the
                                                table.
                        TDC_RT_REMOVE_UNUSED  - remove all unused TABLE
                                                instances (if there are no
                                                used instances will also
                                                remove TABLE_SHARE).
                        TDC_RT_REMOVE_NOT_OWN_KEEP_SHARE -
                                                remove all TABLE instances
                                                except those that belong to
                                                this thread, but don't mark
                                                TABLE_SHARE as old. There
                                                should be no TABLE objects
                                                used by other threads and
                                                caller should have exclusive
                                                metadata lock on the table.
   @param  db           Name of database
   @param  table_name   Name of table
   @param  has_lock     If TRUE, LOCK_open is already acquired

   @note It assumes that table instances are already not used by any
   (other) thread (this should be achieved by using meta-data locks).
*/

void tdc_remove_table(THD *thd, enum_tdc_remove_table_type remove_type,
                      const char *db, const char *table_name,
                      bool has_lock)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  TABLE_SHARE *share;

  if (! has_lock)
    table_cache_manager.lock_all_and_tdc();
  else
    table_cache_manager.assert_owner_all_and_tdc();

  DBUG_ASSERT(remove_type == TDC_RT_REMOVE_UNUSED ||
              thd->mdl_context.is_lock_owner(MDL_key::TABLE, db, table_name,
                                             MDL_EXCLUSIVE));

  key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;

  if ((share= (TABLE_SHARE*) my_hash_search(&table_def_cache,(uchar*) key,
                                            key_length)))
  {
    if (share->ref_count)
    {
      /*
        Set share's version to zero in order to ensure that it gets
        automatically deleted once it is no longer referenced.

        Note that code in TABLE_SHARE::wait_for_old_version() assumes
        that marking share as old and removal of its unused tables
        and of the share itself from TDC happens atomically under
        protection of LOCK_open, or, putting it another way, that
        TDC does not contain old shares which don't have any tables
        used.
      */
      if (remove_type != TDC_RT_REMOVE_NOT_OWN_KEEP_SHARE)
        share->version= 0;
      table_cache_manager.free_table(thd, remove_type, share);
    }
    else
    {
      DBUG_ASSERT(remove_type != TDC_RT_REMOVE_NOT_OWN_KEEP_SHARE);
      (void) my_hash_delete(&table_def_cache, (uchar*) share);
    }
  }

  if (! has_lock)
    table_cache_manager.unlock_all_and_tdc();
}


int setup_ftfuncs(SELECT_LEX *select_lex)
{
  List_iterator<Item_func_match> li(*(select_lex->ftfunc_list)),
                                 lj(*(select_lex->ftfunc_list));
  Item_func_match *ftf, *ftf2;

  while ((ftf=li++))
  {
    if (ftf->fix_index())
      return 1;
    lj.rewind();
    while ((ftf2=lj++) != ftf)
    {
      if (ftf->eq(ftf2,1) && !ftf2->master)
        ftf2->master=ftf;
    }
  }

  return 0;
}


int init_ftfuncs(THD *thd, SELECT_LEX *select_lex, bool no_order)
{
  if (select_lex->ftfunc_list->elements)
  {
    List_iterator<Item_func_match> li(*(select_lex->ftfunc_list));
    Item_func_match *ifm;
    DBUG_PRINT("info",("Performing FULLTEXT search"));
    THD_STAGE_INFO(thd, stage_fulltext_initialization);

    while ((ifm=li++))
      ifm->init_search(no_order);
  }
  return 0;
}


bool is_equal(const LEX_STRING *a, const LEX_STRING *b)
{
  return a->length == b->length && !strncmp(a->str, b->str, a->length);
}


/*
  Tells if two (or more) tables have auto_increment columns and we want to
  lock those tables with a write lock.

  SYNOPSIS
    has_two_write_locked_tables_with_auto_increment
      tables        Table list

  NOTES:
    Call this function only when you have established the list of all tables
    which you'll want to update (including stored functions, triggers, views
    inside your statement).
*/

static bool
has_write_table_with_auto_increment(TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table; table= table->next_global)
  {
    /* we must do preliminary checks as table->table may be NULL */
    if (!table->placeholder() &&
        table->table->found_next_number_field &&
        (table->lock_type >= TL_WRITE_ALLOW_WRITE))
      return 1;
  }

  return 0;
}

/*
   checks if we have select tables in the table list and write tables
   with auto-increment column.

  SYNOPSIS
   has_two_write_locked_tables_with_auto_increment_and_select
      tables        Table list

  RETURN VALUES

   -true if the table list has atleast one table with auto-increment column


         and atleast one table to select from.
   -false otherwise
*/

static bool
has_write_table_with_auto_increment_and_select(TABLE_LIST *tables)
{
  bool has_select= false;
  bool has_auto_increment_tables = has_write_table_with_auto_increment(tables);
  for(TABLE_LIST *table= tables; table; table= table->next_global)
  {
     if (!table->placeholder() &&
        (table->lock_type <= TL_READ_NO_INSERT))
      {
        has_select= true;
        break;
      }
  }
  return(has_select && has_auto_increment_tables);
}

/*
  Tells if there is a table whose auto_increment column is a part
  of a compound primary key while is not the first column in
  the table definition.

  @param tables Table list

  @return true if the table exists, fais if does not.
*/

static bool
has_write_table_auto_increment_not_first_in_pk(TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table; table= table->next_global)
  {
    /* we must do preliminary checks as table->table may be NULL */
    if (!table->placeholder() &&
        table->table->found_next_number_field &&
        (table->lock_type >= TL_WRITE_ALLOW_WRITE)
        && table->table->s->next_number_keypart != 0)
      return 1;
  }

  return 0;
}



/*
  Open and lock system tables for read.

  SYNOPSIS
    open_system_tables_for_read()
      thd         Thread context.
      table_list  List of tables to open.
      backup      Pointer to Open_tables_state instance where
                  information about currently open tables will be
                  saved, and from which will be restored when we will
                  end work with system tables.

  NOTES
    Thanks to restrictions which we put on opening and locking of
    system tables for writing, we can open and lock them for reading
    even when we already have some other tables open and locked.  One
    must call close_system_tables() to close systems tables opened
    with this call.

  RETURN
    FALSE   Success
    TRUE    Error
*/

bool
open_system_tables_for_read(THD *thd, TABLE_LIST *table_list,
                            Open_tables_backup *backup)
{
  Query_tables_list query_tables_list_backup;
  LEX *lex= thd->lex;

  DBUG_ENTER("open_system_tables_for_read");

  /*
    Besides using new Open_tables_state for opening system tables,
    we also have to backup and reset/and then restore part of LEX
    which is accessed by open_tables() in order to determine if
    prelocking is needed and what tables should be added for it.
  */
  lex->reset_n_backup_query_tables_list(&query_tables_list_backup);
  thd->reset_n_backup_open_tables_state(backup);

  if (open_and_lock_tables(thd, table_list, FALSE,
                           MYSQL_OPEN_IGNORE_FLUSH |
                           MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    lex->restore_backup_query_tables_list(&query_tables_list_backup);
    thd->restore_backup_open_tables_state(backup);
    DBUG_RETURN(TRUE);
  }

  for (TABLE_LIST *tables= table_list; tables; tables= tables->next_global)
  {
    DBUG_ASSERT(tables->table->s->table_category == TABLE_CATEGORY_SYSTEM);
    tables->table->use_all_columns();
  }
  lex->restore_backup_query_tables_list(&query_tables_list_backup);

  DBUG_RETURN(FALSE);
}


/*
  Close system tables, opened with open_system_tables_for_read().

  SYNOPSIS
    close_system_tables()
      thd     Thread context
      backup  Pointer to Open_tables_backup instance which holds
              information about tables which were open before we
              decided to access system tables.
*/

void
close_system_tables(THD *thd, Open_tables_backup *backup)
{
  Query_tables_list query_tables_list_backup;

  /*
    In order not affect execution of current statement we have to
    backup/reset/restore Query_tables_list part of LEX, which is
    accessed and updated in the process of closing tables.
  */
  thd->lex->reset_n_backup_query_tables_list(&query_tables_list_backup);
  close_thread_tables(thd);
  thd->lex->restore_backup_query_tables_list(&query_tables_list_backup);
  thd->restore_backup_open_tables_state(backup);
}


/**
  A helper function to close a mysql.* table opened
  in an auxiliary THD during bootstrap or in the main
  connection, when we know that there are no locks
  held by the connection due to a preceding implicit
  commit.

  This function assumes that there is no
  statement transaction started for the operation
  itself, since mysql.* tables are not transactional
  and when they are used the binlog is off (DDL
  binlogging is always statement-based.

  We need this function since we'd like to not
  just close the system table, but also release
  the metadata lock on it.

  Note, that in LOCK TABLES mode this function
  does not release the metadata lock. But in this
  mode the table can be opened only if it is locked
  explicitly with LOCK TABLES.
*/

void
close_mysql_tables(THD *thd)
{
  /* No need to commit/rollback statement transaction, it's not started. */
  DBUG_ASSERT(thd->transaction.stmt.is_empty());
  close_thread_tables(thd);
  thd->mdl_context.release_transactional_locks();
}

/*
  Open and lock one system table for update.

  SYNOPSIS
    open_system_table_for_update()
      thd        Thread context.
      one_table  Table to open.

  NOTES
    Table opened with this call should closed using close_thread_tables().

  RETURN
    0	Error
    #	Pointer to TABLE object of system table
*/

TABLE *
open_system_table_for_update(THD *thd, TABLE_LIST *one_table)
{
  DBUG_ENTER("open_system_table_for_update");

  TABLE *table= open_ltable(thd, one_table, one_table->lock_type,
                            MYSQL_LOCK_IGNORE_TIMEOUT);
  if (table)
  {
    DBUG_ASSERT(table->s->table_category == TABLE_CATEGORY_SYSTEM);
    table->use_all_columns();
  }

  DBUG_RETURN(table);
}

/**
  Open a log table.
  Opening such tables is performed internally in the server
  implementation, and is a 'nested' open, since some tables
  might be already opened by the current thread.
  The thread context before this call is saved, and is restored
  when calling close_log_table().
  @param thd The current thread
  @param one_table Log table to open
  @param backup [out] Temporary storage used to save the thread context
*/
TABLE *
open_log_table(THD *thd, TABLE_LIST *one_table, Open_tables_backup *backup)
{
  uint flags= ( MYSQL_OPEN_IGNORE_GLOBAL_READ_LOCK |
                MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY |
                MYSQL_OPEN_IGNORE_FLUSH |
                MYSQL_LOCK_IGNORE_TIMEOUT |
                MYSQL_LOCK_LOG_TABLE);
  TABLE *table;
  /* Save value that is changed in mysql_lock_tables() */
  ulonglong save_utime_after_lock= thd->utime_after_lock;
  DBUG_ENTER("open_log_table");

  thd->reset_n_backup_open_tables_state(backup);

  if ((table= open_ltable(thd, one_table, one_table->lock_type, flags)))
  {
    DBUG_ASSERT(table->s->table_category == TABLE_CATEGORY_LOG);
    /* Make sure all columns get assigned to a default value */
    table->use_all_columns();
    DBUG_ASSERT(table->no_replicate);
  }
  else
    thd->restore_backup_open_tables_state(backup);

  thd->utime_after_lock= save_utime_after_lock;
  DBUG_RETURN(table);
}

/**
  Close a log table.
  The last table opened by open_log_table()
  is closed, then the thread context is restored.
  @param thd The current thread
  @param backup [in] the context to restore.
*/
void close_log_table(THD *thd, Open_tables_backup *backup)
{
  close_system_tables(thd, backup);
}

/**
  @} (end of group Data_Dictionary)
*/
