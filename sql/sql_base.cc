/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Basic functions needed by many modules */

#include "mysql_priv.h"
#include "debug_sync.h"
#include "sql_select.h"
#include "sp_head.h"
#include "sp.h"
#include "sql_trigger.h"
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include "rpl_filter.h"
#ifdef  __WIN__
#include <io.h>
#endif

#define FLAGSTR(S,F) ((S) & (F) ? #F " " : "")

/**
  This internal handler is used to trap internally
  errors that can occur when executing open table
  during the prelocking phase.
*/
class Prelock_error_handler : public Internal_error_handler
{
public:
  Prelock_error_handler()
    : m_handled_errors(0), m_unhandled_errors(0)
  {}

  virtual ~Prelock_error_handler() {}

  virtual bool handle_error(uint sql_errno, const char *message,
                            MYSQL_ERROR::enum_warning_level level,
                            THD *thd);

  bool safely_trapped_errors();

private:
  int m_handled_errors;
  int m_unhandled_errors;
};


bool
Prelock_error_handler::handle_error(uint sql_errno,
                                    const char * /* message */,
                                    MYSQL_ERROR::enum_warning_level /* level */,
                                    THD * /* thd */)
{
  if (sql_errno == ER_NO_SUCH_TABLE)
  {
    m_handled_errors++;
    return TRUE;
  }

  m_unhandled_errors++;
  return FALSE;
}


bool Prelock_error_handler::safely_trapped_errors()
{
  /*
    If m_unhandled_errors != 0, something else, unanticipated, happened,
    so the error is not trapped but returned to the caller.
    Multiple ER_NO_SUCH_TABLE can be raised in case of views.
  */
  return ((m_handled_errors > 0) && (m_unhandled_errors == 0));
}

/**
  @defgroup Data_Dictionary Data Dictionary
  @{
*/
TABLE *unused_tables;				/* Used by mysql_test */
HASH open_cache;				/* Used by mysql_test */
static HASH table_def_cache;
static TABLE_SHARE *oldest_unused_share, end_of_unused_share;
static pthread_mutex_t LOCK_table_share;
static bool table_def_inited= 0;

/**
  Dummy TABLE instance which is used in reopen_tables() and reattach_merge()
  functions to mark MERGE tables and their children with which there is some
  kind of problem and which therefore we need to close.
*/
static TABLE bad_merge_marker;

static int open_unireg_entry(THD *thd, TABLE *entry, TABLE_LIST *table_list,
			     const char *alias,
                             char *cache_key, uint cache_key_length,
			     MEM_ROOT *mem_root, uint flags);
static void free_cache_entry(TABLE *entry);
static bool open_new_frm(THD *thd, TABLE_SHARE *share, const char *alias,
                         uint db_stat, uint prgflag,
                         uint ha_open_flags, TABLE *outparam,
                         TABLE_LIST *table_desc, MEM_ROOT *mem_root);
static void close_old_data_files(THD *thd, TABLE *table, bool morph_locks,
                                 bool send_refresh);
static bool
has_write_table_with_auto_increment(TABLE_LIST *tables);
static bool
has_write_table_auto_increment_not_first_in_pk(TABLE_LIST *tables);


extern "C" uchar *table_cache_key(const uchar *record, size_t *length,
				 my_bool not_used __attribute__((unused)))
{
  TABLE *entry=(TABLE*) record;
  *length= entry->s->table_cache_key.length;
  return (uchar*) entry->s->table_cache_key.str;
}


bool table_cache_init(void)
{
  return hash_init(&open_cache, &my_charset_bin, table_cache_size+16,
		   0, 0, table_cache_key,
		   (hash_free_key) free_cache_entry, 0) != 0;
}

void table_cache_free(void)
{
  DBUG_ENTER("table_cache_free");
  if (table_def_inited)
  {
    close_cached_tables(NULL, NULL, FALSE, FALSE, FALSE);
    if (!open_cache.records)			// Safety first
      hash_free(&open_cache);
  }
  DBUG_VOID_RETURN;
}

uint cached_open_tables(void)
{
  return open_cache.records;
}


#ifdef EXTRA_DEBUG
static void check_unused(void)
{
  uint count= 0, open_files= 0, idx= 0;
  TABLE *cur_link,*start_link;

  if ((start_link=cur_link=unused_tables))
  {
    do
    {
      if (cur_link != cur_link->next->prev || cur_link != cur_link->prev->next)
      {
	DBUG_PRINT("error",("Unused_links aren't linked properly")); /* purecov: inspected */
	return; /* purecov: inspected */
      }
    } while (count++ < open_cache.records &&
	     (cur_link=cur_link->next) != start_link);
    if (cur_link != start_link)
    {
      DBUG_PRINT("error",("Unused_links aren't connected")); /* purecov: inspected */
    }
  }
  for (idx=0 ; idx < open_cache.records ; idx++)
  {
    TABLE *entry=(TABLE*) hash_element(&open_cache,idx);
    if (!entry->in_use)
      count--;
    if (entry->file)
      open_files++;
  }
  if (count != 0)
  {
    DBUG_PRINT("error",("Unused_links doesn't match open_cache: diff: %d", /* purecov: inspected */
			count)); /* purecov: inspected */
  }

#ifdef NOT_SAFE_FOR_REPAIR
  /*
    check that open cache and table definition cache has same number of
    aktive tables
  */
  count= 0;
  for (idx=0 ; idx < table_def_cache.records ; idx++)
  {
    TABLE_SHARE *entry= (TABLE_SHARE*) hash_element(&table_def_cache,idx);
    count+= entry->ref_count;
  }
  if (count != open_files)
  {
    DBUG_PRINT("error", ("table_def ref_count: %u  open_cache: %u",
                         count, open_files));
    DBUG_ASSERT(count == open_files);
  }
#endif
}
#else
#define check_unused()
#endif


/*
  Create a table cache key

  SYNOPSIS
    create_table_def_key()
    thd			Thread handler
    key			Create key here (must be of size MAX_DBKEY_LENGTH)
    table_list		Table definition
    tmp_table		Set if table is a tmp table

 IMPLEMENTATION
    The table cache_key is created from:
    db_name + \0
    table_name + \0

    if the table is a tmp table, we add the following to make each tmp table
    unique on the slave:

    4 bytes for master thread id
    4 bytes pseudo thread id

  RETURN
    Length of key
*/

uint create_table_def_key(THD *thd, char *key, TABLE_LIST *table_list,
                          bool tmp_table)
{
  uint key_length= create_table_def_key(key, table_list->db,
                                        table_list->table_name);

  if (tmp_table)
  {
    int4store(key + key_length, thd->server_id);
    int4store(key + key_length + 4, thd->variables.pseudo_thread_id);
    key_length+= TMP_TABLE_KEY_EXTRA;
  }
  return key_length;
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
  if (share->prev)
  {
    /* remove from old_unused_share list */
    pthread_mutex_lock(&LOCK_table_share);
    *share->prev= share->next;
    share->next->prev= share->prev;
    pthread_mutex_unlock(&LOCK_table_share);
  }
  free_table_share(share);
  DBUG_VOID_RETURN;
}


bool table_def_init(void)
{
  table_def_inited= 1;
  pthread_mutex_init(&LOCK_table_share, MY_MUTEX_INIT_FAST);
  oldest_unused_share= &end_of_unused_share;
  end_of_unused_share.prev= &oldest_unused_share;

  return hash_init(&table_def_cache, &my_charset_bin, table_def_size,
		   0, 0, table_def_key,
		   (hash_free_key) table_def_free_entry, 0) != 0;
}


void table_def_free(void)
{
  DBUG_ENTER("table_def_free");
  if (table_def_inited)
  {
    table_def_inited= 0;
    pthread_mutex_destroy(&LOCK_table_share);
    hash_free(&table_def_cache);
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

TABLE_SHARE *get_table_share(THD *thd, TABLE_LIST *table_list, char *key,
                             uint key_length, uint db_flags, int *error)
{
  TABLE_SHARE *share;
  DBUG_ENTER("get_table_share");

  *error= 0;

  /* Read table definition from cache */
  if ((share= (TABLE_SHARE*) hash_search(&table_def_cache,(uchar*) key,
                                         key_length)))
    goto found;

  if (!(share= alloc_table_share(table_list, key, key_length)))
  {
    DBUG_RETURN(0);
  }

  /*
    Lock mutex to be able to read table definition from file without
    conflicts
  */
  (void) pthread_mutex_lock(&share->mutex);

  /*
    We assign a new table id under the protection of the LOCK_open and
    the share's own mutex.  We do this insted of creating a new mutex
    and using it for the sole purpose of serializing accesses to a
    static variable, we assign the table id here.  We assign it to the
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
    (void) hash_delete(&table_def_cache, (uchar*) share);
    DBUG_RETURN(0);
  }
  share->ref_count++;				// Mark in use
  DBUG_PRINT("exit", ("share: 0x%lx  ref_count: %u",
                      (ulong) share, share->ref_count));
  (void) pthread_mutex_unlock(&share->mutex);
  DBUG_RETURN(share);

found:
  /* 
     We found an existing table definition. Return it if we didn't get
     an error when reading the table definition from file.
  */

  /* We must do a lock to ensure that the structure is initialized */
  (void) pthread_mutex_lock(&share->mutex);
  if (share->error)
  {
    /* Table definition contained an error */
    open_table_error(share, share->error, share->open_errno, share->errarg);
    (void) pthread_mutex_unlock(&share->mutex);
    DBUG_RETURN(0);
  }
  if (share->is_view && !(db_flags & OPEN_VIEW))
  {
    open_table_error(share, 1, ENOENT, 0);
    (void) pthread_mutex_unlock(&share->mutex);
    DBUG_RETURN(0);
  }

  if (!share->ref_count++ && share->prev)
  {
    /*
      Share was not used before and it was in the old_unused_share list
      Unlink share from this list
    */
    DBUG_PRINT("info", ("Unlinking from not used list"));
    pthread_mutex_lock(&LOCK_table_share);
    *share->prev= share->next;
    share->next->prev= share->prev;
    share->next= 0;
    share->prev= 0;
    pthread_mutex_unlock(&LOCK_table_share);
  }
  (void) pthread_mutex_unlock(&share->mutex);

   /* Free cache if too big */
  while (table_def_cache.records > table_def_size &&
         oldest_unused_share->next)
  {
    pthread_mutex_lock(&oldest_unused_share->mutex);
    VOID(hash_delete(&table_def_cache, (uchar*) oldest_unused_share));
  }

  DBUG_PRINT("exit", ("share: 0x%lx  ref_count: %u",
                      (ulong) share, share->ref_count));
  DBUG_RETURN(share);
}


/*
  Get a table share. If it didn't exist, try creating it from engine

  For arguments and return values, see get_table_from_share()
*/

static TABLE_SHARE
*get_table_share_with_create(THD *thd, TABLE_LIST *table_list,
                             char *key, uint key_length,
                             uint db_flags, int *error)
{
  TABLE_SHARE *share;
  int tmp;
  DBUG_ENTER("get_table_share_with_create");

  share= get_table_share(thd, table_list, key, key_length, db_flags, error);
  /*
    If share is not NULL, we found an existing share.

    If share is NULL, and there is no error, we're inside
    pre-locking, which silences 'ER_NO_SUCH_TABLE' errors
    with the intention to silently drop non-existing tables 
    from the pre-locking list. In this case we still need to try
    auto-discover before returning a NULL share.

    If share is NULL and the error is ER_NO_SUCH_TABLE, this is
    the same as above, only that the error was not silenced by 
    pre-locking. Once again, we need to try to auto-discover
    the share.

    Finally, if share is still NULL, it's a real error and we need
    to abort.

    @todo Rework alternative ways to deal with ER_NO_SUCH TABLE.
  */
  if (share || (thd->is_error() && thd->main_da.sql_errno() != ER_NO_SUCH_TABLE))

    DBUG_RETURN(share);

  /* Table didn't exist. Check if some engine can provide it */
  tmp= ha_create_table_from_engine(thd, table_list->db,
                                   table_list->table_name);
  if (tmp < 0)
  {
    /*
      No such table in any engine.
      Hide "Table doesn't exist" errors if the table belongs to a view.
      The check for thd->is_error() is necessary to not push an
      unwanted error in case of pre-locking, which silences
      "no such table" errors.
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
    DBUG_RETURN(0);
  }
  if (tmp)
  {
    /* Give right error message */
    thd->clear_error();
    DBUG_PRINT("error", ("Discovery of %s/%s failed", table_list->db,
                         table_list->table_name));
    my_printf_error(ER_UNKNOWN_ERROR,
                    "Failed to open '%-.64s', error while "
                    "unpacking from engine",
                    MYF(0), table_list->table_name);
    DBUG_RETURN(0);
  }
  /* Table existed in engine. Let's open it */
  mysql_reset_errors(thd, 1);                   // Clear warnings
  thd->clear_error();                           // Clear error message
  DBUG_RETURN(get_table_share(thd, table_list, key, key_length,
                              db_flags, error));
}


/* 
   Mark that we are not using table share anymore.

   SYNOPSIS
     release_table_share()
     share		Table share
     release_type	How the release should be done:
     			RELEASE_NORMAL
                         - Release without checking
                        RELEASE_WAIT_FOR_DROP
                         - Don't return until we get a signal that the
                           table is deleted or the thread is killed.

   IMPLEMENTATION
     If ref_count goes to zero and (we have done a refresh or if we have
     already too many open table shares) then delete the definition.

     If type == RELEASE_WAIT_FOR_DROP then don't return until we get a signal
     that the table is deleted or the thread is killed.
*/

void release_table_share(TABLE_SHARE *share, enum release_type type)
{
  bool to_be_deleted= 0;
  DBUG_ENTER("release_table_share");
  DBUG_PRINT("enter",
             ("share: 0x%lx  table: %s.%s  ref_count: %u  version: %lu",
              (ulong) share, share->db.str, share->table_name.str,
              share->ref_count, share->version));

  safe_mutex_assert_owner(&LOCK_open);

  pthread_mutex_lock(&share->mutex);
  if (!--share->ref_count)
  {
    if (share->version != refresh_version)
      to_be_deleted=1;
    else
    {
      /* Link share last in used_table_share list */
      DBUG_PRINT("info",("moving share to unused list"));

      DBUG_ASSERT(share->next == 0);
      pthread_mutex_lock(&LOCK_table_share);
      share->prev= end_of_unused_share.prev;
      *end_of_unused_share.prev= share;
      end_of_unused_share.prev= &share->next;
      share->next= &end_of_unused_share;
      pthread_mutex_unlock(&LOCK_table_share);

      to_be_deleted= (table_def_cache.records > table_def_size);
    }
  }

  if (to_be_deleted)
  {
    DBUG_PRINT("info", ("Deleting share"));
    hash_delete(&table_def_cache, (uchar*) share);
    DBUG_VOID_RETURN;
  }
  pthread_mutex_unlock(&share->mutex);
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
  char key[NAME_LEN*2+2];
  uint key_length;
  safe_mutex_assert_owner(&LOCK_open);

  key_length= create_table_def_key(key, db, table_name);
  return (TABLE_SHARE*) hash_search(&table_def_cache,(uchar*) key, key_length);
}  


/*
  Close file handle, but leave the table in the table cache

  SYNOPSIS
    close_handle_and_leave_table_as_lock()
    table		Table handler

  NOTES
    By leaving the table in the table cache, it disallows any other thread
    to open the table

    thd->killed will be set if we run out of memory

    If closing a MERGE child, the calling function has to take care for
    closing the parent too, if necessary.
*/


void close_handle_and_leave_table_as_lock(TABLE *table)
{
  TABLE_SHARE *share, *old_share= table->s;
  char *key_buff;
  MEM_ROOT *mem_root= &table->mem_root;
  DBUG_ENTER("close_handle_and_leave_table_as_lock");

  DBUG_ASSERT(table->db_stat);

  /*
    Make a local copy of the table share and free the current one.
    This has to be done to ensure that the table share is removed from
    the table defintion cache as soon as the last instance is removed
  */
  if (multi_alloc_root(mem_root,
                       &share, sizeof(*share),
                       &key_buff, old_share->table_cache_key.length,
                       NULL))
  {
    bzero((char*) share, sizeof(*share));
    share->set_table_cache_key(key_buff, old_share->table_cache_key.str,
                               old_share->table_cache_key.length);
    share->tmp_table= INTERNAL_TMP_TABLE;       // for intern_close_table()
  }

  /*
    When closing a MERGE parent or child table, detach the children first.
    Do not clear child table references to allow for reopen.
  */
  if (table->child_l || table->parent)
    detach_merge_children(table, FALSE);
  table->file->close();
  table->db_stat= 0;                            // Mark file closed
  release_table_share(table->s, RELEASE_NORMAL);
  table->s= share;
  table->file->change_table_ptr(table, table->s);

  DBUG_VOID_RETURN;
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

  VOID(pthread_mutex_lock(&LOCK_open));
  bzero((char*) &table_list,sizeof(table_list));
  start_list= &open_list;
  open_list=0;

  for (uint idx=0 ; result == 0 && idx < open_cache.records; idx++)
  {
    OPEN_TABLE_LIST *table;
    TABLE *entry=(TABLE*) hash_element(&open_cache,idx);
    TABLE_SHARE *share= entry->s;

    if (db && my_strcasecmp(system_charset_info, db, share->db.str))
      continue;
    if (wild && wild_compare(share->table_name.str, wild, 0))
      continue;

    /* Check if user has SELECT privilege for any column in the table */
    table_list.db=         share->db.str;
    table_list.table_name= share->table_name.str;
    table_list.grant.privilege=0;

    if (check_table_access(thd,SELECT_ACL | EXTRA_ACL,&table_list, 1, TRUE))
      continue;
    /* need to check if we haven't already listed it */
    for (table= open_list  ; table ; table=table->next)
    {
      if (!strcmp(table->table, share->table_name.str) &&
	  !strcmp(table->db,    share->db.str))
      {
	if (entry->in_use)
	  table->in_use++;
	if (entry->locked_by_name)
	  table->locked++;
	break;
      }
    }
    if (table)
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
    (*start_list)->in_use= entry->in_use ? 1 : 0;
    (*start_list)->locked= entry->locked_by_name ? 1 : 0;
    start_list= &(*start_list)->next;
    *start_list=0;
  }
  VOID(pthread_mutex_unlock(&LOCK_open));
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
  if (table->file)                              // Not true if name lock
    VOID(closefrm(table, 1));			// close file
  DBUG_VOID_RETURN;
}

/*
  Remove table from the open table cache

  SYNOPSIS
    free_cache_entry()
    table		Table to remove

  NOTE
    We need to have a lock on LOCK_open when calling this
*/

static void free_cache_entry(TABLE *table)
{
  DBUG_ENTER("free_cache_entry");

  /* Assert that MERGE children are not attached before final close. */
  DBUG_ASSERT(!table->is_children_attached());

  intern_close_table(table);
  if (!table->in_use)
  {
    table->next->prev=table->prev;		/* remove from used chain */
    table->prev->next=table->next;
    if (table == unused_tables)
    {
      unused_tables=unused_tables->next;
      if (table == unused_tables)
	unused_tables=0;
    }
    check_unused();				// consisty check
  }
  my_free((uchar*) table,MYF(0));
  DBUG_VOID_RETURN;
}

/* Free resources allocated by filesort() and read_record() */

void free_io_cache(TABLE *table)
{
  DBUG_ENTER("free_io_cache");
  if (table->sort.io_cache)
  {
    close_cached_file(table->sort.io_cache);
    my_free((uchar*) table->sort.io_cache,MYF(0));
    table->sort.io_cache=0;
  }
  DBUG_VOID_RETURN;
}


/*
  Close all tables which aren't in use by any thread

  @param thd Thread context
  @param tables List of tables to remove from the cache
  @param have_lock If LOCK_open is locked
  @param wait_for_refresh Wait for a impending flush
  @param wait_for_placeholders Wait for tables being reopened so that the GRL
         won't proceed while write-locked tables are being reopened by other
         threads.

  @remark THD can be NULL, but then wait_for_refresh must be FALSE
          and tables must be NULL.
*/

bool close_cached_tables(THD *thd, TABLE_LIST *tables, bool have_lock,
                         bool wait_for_refresh, bool wait_for_placeholders)
{
  bool result=0;
  DBUG_ENTER("close_cached_tables");
  DBUG_ASSERT(thd || (!wait_for_refresh && !tables));

  if (!have_lock)
    VOID(pthread_mutex_lock(&LOCK_open));
  if (!tables)
  {
    refresh_version++;				// Force close of open tables
    while (unused_tables)
    {
#ifdef EXTRA_DEBUG
      if (hash_delete(&open_cache,(uchar*) unused_tables))
	printf("Warning: Couldn't delete open table from hash\n");
#else
      VOID(hash_delete(&open_cache,(uchar*) unused_tables));
#endif
    }
    /* Free table shares */
    while (oldest_unused_share->next)
    {
      pthread_mutex_lock(&oldest_unused_share->mutex);
      VOID(hash_delete(&table_def_cache, (uchar*) oldest_unused_share));
    }
    DBUG_PRINT("tcache", ("incremented global refresh_version to: %lu",
                          refresh_version));
    if (wait_for_refresh)
    {
      /*
        Other threads could wait in a loop in open_and_lock_tables(),
        trying to lock one or more of our tables.

        If they wait for the locks in thr_multi_lock(), their lock
        request is aborted. They loop in open_and_lock_tables() and
        enter open_table(). Here they notice the table is refreshed and
        wait for COND_refresh. Then they loop again in
        open_and_lock_tables() and this time open_table() succeeds. At
        this moment, if we (the FLUSH TABLES thread) are scheduled and
        on another FLUSH TABLES enter close_cached_tables(), they could
        awake while we sleep below, waiting for others threads (us) to
        close their open tables. If this happens, the other threads
        would find the tables unlocked. They would get the locks, one
        after the other, and could do their destructive work. This is an
        issue if we have LOCK TABLES in effect.

        The problem is that the other threads passed all checks in
        open_table() before we refresh the table.

        The fix for this problem is to set some_tables_deleted for all
        threads with open tables. These threads can still get their
        locks, but will immediately release them again after checking
        this variable. They will then loop in open_and_lock_tables()
        again. There they will wait until we update all tables version
        below.

        Setting some_tables_deleted is done by remove_table_from_cache()
        in the other branch.

        In other words (reviewer suggestion): You need this setting of
        some_tables_deleted for the case when table was opened and all
        related checks were passed before incrementing refresh_version
        (which you already have) but attempt to lock the table happened
        after the call to close_old_data_files() i.e. after removal of
        current thread locks.
      */
      for (uint idx=0 ; idx < open_cache.records ; idx++)
      {
        TABLE *table=(TABLE*) hash_element(&open_cache,idx);
        if (table->in_use)
          table->in_use->some_tables_deleted= 1;
      }
    }
  }
  else
  {
    bool found=0;
    for (TABLE_LIST *table= tables; table; table= table->next_local)
    {
      if (remove_table_from_cache(thd, table->db, table->table_name,
                                  RTFC_OWNED_BY_THD_FLAG))
	found=1;
    }
    if (!found)
      wait_for_refresh=0;			// Nothing to wait for
  }
#ifndef EMBEDDED_LIBRARY
  if (!tables)
    kill_delayed_threads();
#endif
  if (wait_for_refresh)
  {
    /*
      If there is any table that has a lower refresh_version, wait until
      this is closed (or this thread is killed) before returning
    */
    thd->mysys_var->current_mutex= &LOCK_open;
    thd->mysys_var->current_cond= &COND_refresh;
    thd_proc_info(thd, "Flushing tables");

    close_old_data_files(thd,thd->open_tables,1,1);
    mysql_ha_flush(thd);
    DEBUG_SYNC(thd, "after_flush_unlock");

    bool found=1;
    /* Wait until all threads has closed all the tables we had locked */
    DBUG_PRINT("info",
	       ("Waiting for other threads to close their open tables"));
    while (found && ! thd->killed)
    {
      found=0;
      for (uint idx=0 ; idx < open_cache.records ; idx++)
      {
	TABLE *table=(TABLE*) hash_element(&open_cache,idx);
        /* Avoid a self-deadlock. */
        if (table->in_use == thd)
          continue;
        /*
          Note that we wait here only for tables which are actually open, and
          not for placeholders with TABLE::open_placeholder set. Waiting for
          latter will cause deadlock in the following scenario, for example:

          conn1: lock table t1 write;
          conn2: lock table t2 write;
          conn1: flush tables;
          conn2: flush tables;

          It also does not make sense to wait for those of placeholders that
          are employed by CREATE TABLE as in this case table simply does not
          exist yet.
        */
	if (table->needs_reopen_or_name_lock() && (table->db_stat ||
            (table->open_placeholder && wait_for_placeholders)))
	{
	  found=1;
          DBUG_PRINT("signal", ("Waiting for COND_refresh"));
	  pthread_cond_wait(&COND_refresh,&LOCK_open);
	  break;
	}
      }
    }
    /*
      No other thread has the locked tables open; reopen them and get the
      old locks. This should always succeed (unless some external process
      has removed the tables)
    */
    thd->in_lock_tables=1;
    result=reopen_tables(thd,1,1);
    thd->in_lock_tables=0;
    /* Set version for table */
    for (TABLE *table=thd->open_tables; table ; table= table->next)
    {
      /*
        Preserve the version (0) of write locked tables so that a impending
        global read lock won't sneak in.
      */
      if (table->reginfo.lock_type < TL_WRITE_ALLOW_WRITE)
        table->s->version= refresh_version;
    }
  }
  if (!have_lock)
    VOID(pthread_mutex_unlock(&LOCK_open));
  if (wait_for_refresh)
  {
    pthread_mutex_lock(&thd->mysys_var->mutex);
    thd->mysys_var->current_mutex= 0;
    thd->mysys_var->current_cond= 0;
    thd_proc_info(thd, 0);
    pthread_mutex_unlock(&thd->mysys_var->mutex);
  }
  DBUG_RETURN(result);
}


/*
  Close all tables which match specified connection string or
  if specified string is NULL, then any table with a connection string.
*/

bool close_cached_connection_tables(THD *thd, bool if_wait_for_refresh,
                                    LEX_STRING *connection, bool have_lock)
{
  uint idx;
  TABLE_LIST tmp, *tables= NULL;
  bool result= FALSE;
  DBUG_ENTER("close_cached_connections");
  DBUG_ASSERT(thd);

  bzero(&tmp, sizeof(TABLE_LIST));

  if (!have_lock)
    VOID(pthread_mutex_lock(&LOCK_open));

  for (idx= 0; idx < table_def_cache.records; idx++)
  {
    TABLE_SHARE *share= (TABLE_SHARE *) hash_element(&table_def_cache, idx);

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

  if (tables)
    result= close_cached_tables(thd, tables, TRUE, FALSE, FALSE);

  if (!have_lock)
    VOID(pthread_mutex_unlock(&LOCK_open));

  if (if_wait_for_refresh)
  {
    pthread_mutex_lock(&thd->mysys_var->mutex);
    thd->mysys_var->current_mutex= 0;
    thd->mysys_var->current_cond= 0;
    thd->proc_info=0;
    pthread_mutex_unlock(&thd->mysys_var->mutex);
  }

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
    {
      table->query_id= 0;
      table->file->ha_reset();
      /*
        Detach temporary MERGE children from temporary parent to allow new
        attach at next open. Do not do the detach, if close_thread_tables()
        is called from a sub-statement. The temporary table might still be
        used in the top-level statement.
      */
      if (table->child_l || table->parent)
        detach_merge_children(table, TRUE);
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
  }
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
  bool found_old_table= 0;

  safe_mutex_assert_not_owner(&LOCK_open);

  VOID(pthread_mutex_lock(&LOCK_open));

  DBUG_PRINT("info", ("thd->open_tables: 0x%lx", (long) thd->open_tables));

  while (thd->open_tables)
    found_old_table|= close_thread_table(thd, &thd->open_tables);
  thd->some_tables_deleted= 0;

  /* Free tables to hold down open files */
  while (open_cache.records > table_cache_size && unused_tables)
    VOID(hash_delete(&open_cache,(uchar*) unused_tables)); /* purecov: tested */
  check_unused();
  if (found_old_table)
  {
    /* Tell threads waiting for refresh that something has happened */
    broadcast_refresh();
  }

  VOID(pthread_mutex_unlock(&LOCK_open));
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
  prelocked_mode_type prelocked_mode= thd->prelocked_mode;
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
  /*
    Let us commit transaction for statement. Since in 5.0 we only have
    one statement transaction and don't allow several nested statement
    transactions this call will do nothing if we are inside of stored
    function or trigger (i.e. statement transaction is already active and
    does not belong to statement for which we do close_thread_tables()).
    TODO: This should be fixed in later releases.
   */
  if (!(thd->state_flags & Open_tables_state::BACKUPS_AVAIL))
  {
    thd->main_da.can_overwrite_status= TRUE;
    ha_autocommit_or_rollback(thd, thd->is_error());
    thd->main_da.can_overwrite_status= FALSE;

    /*
      Reset transaction state, but only if we're not inside a
      sub-statement of a prelocked statement.
    */
    if (! prelocked_mode || thd->lex->requires_prelocking())
      thd->transaction.stmt.reset();
  }

  if (thd->locked_tables || prelocked_mode)
  {

    /* Ensure we are calling ha_reset() for all used tables */
    mark_used_tables_as_free_for_reuse(thd, thd->open_tables);

    /*
      We are under simple LOCK TABLES or we're inside a sub-statement
      of a prelocked statement, so should not do anything else.
    */
    if (!prelocked_mode || !thd->lex->requires_prelocking())
      DBUG_VOID_RETURN;

    /*
      We are in the top-level statement of a prelocked statement,
      so we have to leave the prelocked mode now with doing implicit
      UNLOCK TABLES if needed.
    */
    DBUG_PRINT("info",("thd->prelocked_mode= NON_PRELOCKED"));
    thd->prelocked_mode= NON_PRELOCKED;

    if (prelocked_mode == PRELOCKED_UNDER_LOCK_TABLES)
      DBUG_VOID_RETURN;

    thd->lock= thd->locked_tables;
    thd->locked_tables= 0;
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
  /*
    Note that we need to hold LOCK_open while changing the
    open_tables list. Another thread may work on it.
    (See: remove_table_from_cache(), mysql_wait_completed_table())
    Closing a MERGE child before the parent would be fatal if the
    other thread tries to abort the MERGE lock in between.
  */
  if (thd->open_tables)
    close_open_tables(thd);

  if (prelocked_mode == PRELOCKED)
  {
    /*
      If we are here then we are leaving normal prelocked mode, so it is
      good idea to turn off OPTION_TABLE_LOCK flag.
    */
    DBUG_ASSERT(thd->lex->requires_prelocking());
    thd->options&= ~(OPTION_TABLE_LOCK);
  }

  DBUG_VOID_RETURN;
}


/* move one table to free list */

bool close_thread_table(THD *thd, TABLE **table_ptr)
{
  bool found_old_table= 0;
  TABLE *table= *table_ptr;
  DBUG_ENTER("close_thread_table");
  DBUG_ASSERT(table->key_read == 0);
  DBUG_ASSERT(!table->file || table->file->inited == handler::NONE);
  DBUG_PRINT("tcache", ("table: '%s'.'%s' 0x%lx", table->s->db.str,
                        table->s->table_name.str, (long) table));

  *table_ptr=table->next;
  /*
    When closing a MERGE parent or child table, detach the children first.
    Clear child table references to force new assignment at next open.
  */
  if (table->child_l || table->parent)
    detach_merge_children(table, TRUE);

  if (table->needs_reopen_or_name_lock() ||
      thd->version != refresh_version || !table->db_stat)
  {
    VOID(hash_delete(&open_cache,(uchar*) table));
    found_old_table=1;
  }
  else
  {
    /*
      Open placeholders have TABLE::db_stat set to 0, so they should be
      handled by the first alternative.
    */
    DBUG_ASSERT(!table->open_placeholder);

    /* Assert that MERGE children are not attached in unused_tables. */
    DBUG_ASSERT(!table->is_children_attached());

    /* Free memory and reset for next loop */
    free_field_buffers_larger_than(table,MAX_TDC_BLOB_SIZE);
    
    table->file->ha_reset();
    table->in_use=0;
    if (unused_tables)
    {
      table->next=unused_tables;		/* Link in last */
      table->prev=unused_tables->prev;
      unused_tables->prev=table;
      table->prev->next=table;
    }
    else
      unused_tables=table->next=table->prev=table;
  }
  DBUG_RETURN(found_old_table);
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

void close_temporary_tables(THD *thd)
{
  TABLE *table;
  TABLE *next= NULL;
  TABLE *prev_table;
  /* Assume thd->options has OPTION_QUOTE_SHOW_CREATE */
  bool was_quote_show= TRUE;

  if (!thd->temporary_tables)
    return;

  if (!mysql_bin_log.is_open() || 
      (thd->current_stmt_binlog_row_based && thd->variables.binlog_format == BINLOG_FORMAT_ROW))
  {
    TABLE *tmp_next;
    for (table= thd->temporary_tables; table; table= tmp_next)
    {
      tmp_next= table->next;
      close_temporary(table, 1, 1);
    }
    thd->temporary_tables= 0;
    return;
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
      !(was_quote_show= test(thd->options & OPTION_QUOTE_SHOW_CREATE)))
  {
    thd->options |= OPTION_QUOTE_SHOW_CREATE;
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
      CHARSET_INFO *cs_save= thd->variables.character_set_client;
      thd->variables.character_set_client= system_charset_info;
      thd->thread_specific_used= TRUE;
      Query_log_event qinfo(thd, s_query.ptr(),
                            s_query.length() - 1 /* to remove trailing ',' */,
                            0, FALSE, 0);
      qinfo.db= db.ptr();
      qinfo.db_len= db.length();
      thd->variables.character_set_client= cs_save;
      if (mysql_bin_log.write(&qinfo))
      {
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR, MYF(0),
                     "Failed to write the DROP statement for temporary tables to binary log");
      }
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
    thd->options&= ~OPTION_QUOTE_SHOW_CREATE; /* restore option */
  thd->temporary_tables=0;
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


/*
  Test that table is unique (It's only exists once in the table list)

  SYNOPSIS
    unique_table()
    thd                   thread handle
    table                 table which should be checked
    table_list            list of tables
    check_alias           whether to check tables' aliases

  NOTE: to exclude derived tables from check we use following mechanism:
    a) during derived table processing set THD::derived_tables_processing
    b) JOIN::prepare set SELECT::exclude_from_table_unique_test if
       THD::derived_tables_processing set. (we can't use JOIN::execute
       because for PS we perform only JOIN::prepare, but we can't set this
       flag in JOIN::prepare if we are not sure that we are in derived table
       processing loop, because multi-update call fix_fields() for some its
       items (which mean JOIN::prepare for subqueries) before unique_table
       call to detect which tables should be locked for write).
    c) unique_table skip all tables which belong to SELECT with
       SELECT::exclude_from_table_unique_test set.
    Also SELECT::exclude_from_table_unique_test used to exclude from check
    tables of main SELECT of multi-delete and multi-update

    We also skip tables with TABLE_LIST::prelocking_placeholder set,
    because we want to allow SELECTs from them, and their modification
    will rise the error anyway.

    TODO: when we will have table/view change detection we can do this check
          only once for PS/SP

  RETURN
    found duplicate
    0 if table is unique
*/

TABLE_LIST* unique_table(THD *thd, TABLE_LIST *table, TABLE_LIST *table_list,
                         bool check_alias)
{
  TABLE_LIST *res;
  const char *d_name, *t_name, *t_alias;
  DBUG_ENTER("unique_table");
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
    if (((! (res= find_table_in_global_list(table_list, d_name, t_name))) &&
         (! (res= mysql_lock_have_duplicate(thd, table, table_list)))) ||
        ((!res->table || res->table != table->table) &&
         (!check_alias || !(lower_case_table_names ?
          my_strcasecmp(files_charset_info, t_alias, res->alias) :
          strcmp(t_alias, res->alias))) &&
         res->select_lex && !res->select_lex->exclude_from_table_unique_test &&
         !res->prelocking_placeholder))
      break;
    /*
      If we found entry of this table or table of SELECT which already
      processed in derived table or top select of multi-update/multi-delete
      (exclude_from_table_unique_test) or prelocking placeholder.
    */
    table_list= res->next_global;
    DBUG_PRINT("info",
               ("found same copy of table or table which we should skip"));
  }
  DBUG_RETURN(res);
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


TABLE *find_temporary_table(THD *thd, const char *db, const char *table_name)
{
  TABLE_LIST table_list;

  table_list.db= (char*) db;
  table_list.table_name= (char*) table_name;
  return find_temporary_table(thd, &table_list);
}


TABLE *find_temporary_table(THD *thd, TABLE_LIST *table_list)
{
  char	key[MAX_DBKEY_LENGTH];
  uint	key_length;
  TABLE *table;
  DBUG_ENTER("find_temporary_table");
  DBUG_PRINT("enter", ("table: '%s'.'%s'",
                       table_list->db, table_list->table_name));

  key_length= create_table_def_key(thd, key, table_list, 1);
  for (table=thd->temporary_tables ; table ; table= table->next)
  {
    if (table->s->table_cache_key.length == key_length &&
	!memcmp(table->s->table_cache_key.str, key, key_length))
    {
      DBUG_PRINT("info",
                 ("Found table. server_id: %u  pseudo_thread_id: %lu",
                  (uint) thd->server_id,
                  (ulong) thd->variables.pseudo_thread_id));
      DBUG_RETURN(table);
    }
  }
  DBUG_RETURN(0);                               // Not a temporary table
}


/**
  Drop a temporary table.

  Try to locate the table in the list of thd->temporary_tables.
  If the table is found:
   - if the table is being used by some outer statement, fail.
   - if the table is in thd->locked_tables, unlock it and
     remove it from the list of locked tables. Currently only transactional
     temporary tables are present in the locked_tables list.
   - Close the temporary table, remove its .FRM
   - remove the table from the list of temporary tables

  This function is used to drop user temporary tables, as well as
  internal tables created in CREATE TEMPORARY TABLE ... SELECT
  or ALTER TABLE. Even though part of the work done by this function
  is redundant when the table is internal, as long as we
  link both internal and user temporary tables into the same
  thd->temporary_tables list, it's impossible to tell here whether
  we're dealing with an internal or a user temporary table.

  @retval  0  the table was found and dropped successfully.
  @retval  1  the table was not found in the list of temporary tables
              of this thread
  @retval -1  the table is in use by a outer query
*/

int drop_temporary_table(THD *thd, TABLE_LIST *table_list)
{
  TABLE *table;
  DBUG_ENTER("drop_temporary_table");
  DBUG_PRINT("tmptable", ("closing table: '%s'.'%s'",
                          table_list->db, table_list->table_name));

  if (!(table= find_temporary_table(thd, table_list)))
    DBUG_RETURN(1);

  /* Table might be in use by some outer statement. */
  if (table->query_id && table->query_id != thd->query_id)
  {
    my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->alias);
    DBUG_RETURN(-1);
  }

  /*
    If LOCK TABLES list is not empty and contains this table,
    unlock the table and remove the table from this list.
  */
  mysql_lock_remove(thd, thd->locked_tables, table, FALSE);
  close_temporary_table(thd, table, 1, 1);
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

  /*
    When closing a MERGE parent or child table, detach the children
    first. Clear child table references as MERGE table cannot be
    reopened after final close of one of its tables.

    This is necessary here because it is sometimes called with attached
    tables and without prior close_thread_tables(). E.g. in
    mysql_alter_table(), mysql_rm_table_part2(), mysql_truncate(),
    drop_open_table().
  */
  if (table->child_l || table->parent)
    detach_merge_children(table, TRUE);

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
    slave_open_temp_tables--;
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
    my_free((char*) table,MYF(0));
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
  TABLE_LIST table_list;
  DBUG_ENTER("rename_temporary_table");

  if (!(key=(char*) alloc_root(&share->mem_root, MAX_DBKEY_LENGTH)))
    DBUG_RETURN(1);				/* purecov: inspected */

  table_list.db= (char*) db;
  table_list.table_name= (char*) table_name;
  key_length= create_table_def_key(thd, key, &table_list, 1);
  share->set_table_cache_key(key, key_length);
  DBUG_RETURN(0);
}


	/* move table first in unused links */

static void relink_unused(TABLE *table)
{
  /* Assert that MERGE children are not attached in unused_tables. */
  DBUG_ASSERT(!table->is_children_attached());

  if (table != unused_tables)
  {
    table->prev->next=table->next;		/* Remove from unused list */
    table->next->prev=table->prev;
    table->next=unused_tables;			/* Link in unused tables */
    table->prev=unused_tables->prev;
    unused_tables->prev->next=table;
    unused_tables->prev=table;
    unused_tables=table;
    check_unused();
  }
}


/**
  Prepare an open merge table for close.

  @param[in]     thd             thread context
  @param[in]     table           table to prepare
  @param[in,out] prev_pp         pointer to pointer of previous table

  @detail
    If the table is a MERGE parent, just detach the children.
    If the table is a MERGE child, close the parent (incl. detach).
*/

static void unlink_open_merge(THD *thd, TABLE *table, TABLE ***prev_pp)
{
  DBUG_ENTER("unlink_open_merge");

  if (table->parent)
  {
    /*
      If MERGE child, close parent too. Closing includes detaching.

      This is used for example in ALTER TABLE t1 RENAME TO t5 under
      LOCK TABLES where t1 is a MERGE child:
      CREATE TABLE t1 (c1 INT);
      CREATE TABLE t2 (c1 INT) ENGINE=MRG_MYISAM UNION=(t1);
      LOCK TABLES t1 WRITE, t2 WRITE;
      ALTER TABLE t1 RENAME TO t5;
    */
    TABLE *parent= table->parent;
    TABLE **prv_p;

    /* Find parent in open_tables list. */
    for (prv_p= &thd->open_tables;
         *prv_p && (*prv_p != parent);
         prv_p= &(*prv_p)->next) {}
    if (*prv_p)
    {
      /* Special treatment required if child follows parent in list. */
      if (*prev_pp == &parent->next)
        *prev_pp= prv_p;
      /*
        Remove parent from open_tables list and close it.
        This includes detaching and hence clearing parent references.
      */
      close_thread_table(thd, prv_p);
    }
  }
  else if (table->child_l)
  {
    /*
      When closing a MERGE parent, detach the children first. It is
      not necessary to clear the child or parent table reference of
      this table because the TABLE is freed. But we need to clear
      the child or parent references of the other belonging tables
      so that they cannot be moved into the unused_tables chain with
      these pointers set.

      This is used for example in ALTER TABLE t2 RENAME TO t5 under
      LOCK TABLES where t2 is a MERGE parent:
      CREATE TABLE t1 (c1 INT);
      CREATE TABLE t2 (c1 INT) ENGINE=MRG_MYISAM UNION=(t1);
      LOCK TABLES t1 WRITE, t2 WRITE;
      ALTER TABLE t2 RENAME TO t5;
    */
    detach_merge_children(table, TRUE);
  }

  DBUG_VOID_RETURN;
}


/**
    Remove all instances of table from thread's open list and
    table cache.

    @param  thd     Thread context
    @param  find    Table to remove
    @param  unlock  TRUE  - free all locks on tables removed that are
                            done with LOCK TABLES
                    FALSE - otherwise

    @note When unlock parameter is FALSE or current thread doesn't have
          any tables locked with LOCK TABLES, tables are assumed to be
          not locked (for example already unlocked).
*/

void unlink_open_table(THD *thd, TABLE *find, bool unlock)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length= find->s->table_cache_key.length;
  TABLE *list, **prev;
  DBUG_ENTER("unlink_open_table");

  safe_mutex_assert_owner(&LOCK_open);

  memcpy(key, find->s->table_cache_key.str, key_length);
  /*
    Note that we need to hold LOCK_open while changing the
    open_tables list. Another thread may work on it.
    (See: remove_table_from_cache(), mysql_wait_completed_table())
    Closing a MERGE child before the parent would be fatal if the
    other thread tries to abort the MERGE lock in between.
  */
  for (prev= &thd->open_tables; *prev; )
  {
    list= *prev;

    if (list->s->table_cache_key.length == key_length &&
	!memcmp(list->s->table_cache_key.str, key, key_length))
    {
      if (unlock && thd->locked_tables)
        mysql_lock_remove(thd, thd->locked_tables,
                          list->parent ? list->parent : list, TRUE);

      /* Prepare MERGE table for close. Close parent if necessary. */
      unlink_open_merge(thd, list, &prev);

      /* Remove table from open_tables list. */
      *prev= list->next;
      /* Close table. */
      VOID(hash_delete(&open_cache,(uchar*) list)); // Close table
    }
    else
    {
      /* Step to next entry in open_tables list. */
      prev= &list->next;
    }
  }

  // Notify any 'refresh' threads
  broadcast_refresh();
  DBUG_VOID_RETURN;
}


/**
    Auxiliary routine which closes and drops open table.

    @param  thd         Thread handle
    @param  table       TABLE object for table to be dropped
    @param  db_name     Name of database for this table
    @param  table_name  Name of this table

    @note This routine assumes that table to be closed is open only
          by calling thread so we needn't wait until other threads
          will close the table. Also unless called under implicit or
          explicit LOCK TABLES mode it assumes that table to be
          dropped is already unlocked. In the former case it will
          also remove lock on the table. But one should not rely on
          this behaviour as it may change in future.
          Currently, however, this function is never called for a
          table that was locked with LOCK TABLES.
*/

void drop_open_table(THD *thd, TABLE *table, const char *db_name,
                     const char *table_name)
{
  if (table->s->tmp_table)
    close_temporary_table(thd, table, 1, 1);
  else
  {
    handlerton *table_type= table->s->db_type();
    VOID(pthread_mutex_lock(&LOCK_open));
    /*
      unlink_open_table() also tells threads waiting for refresh or close
      that something has happened.
    */
    unlink_open_table(thd, table, FALSE);
    quick_rm_table(table_type, db_name, table_name, 0);
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
}


/*
   Wait for condition but allow the user to send a kill to mysqld

   SYNOPSIS
     wait_for_condition()
     thd	Thread handler
     mutex	mutex that is currently hold that is associated with condition
	        Will be unlocked on return     
     cond	Condition to wait for
*/

void wait_for_condition(THD *thd, pthread_mutex_t *mutex, pthread_cond_t *cond)
{
  /* Wait until the current table is up to date */
  const char *proc_info;
  thd->mysys_var->current_mutex= mutex;
  thd->mysys_var->current_cond= cond;
  proc_info=thd->proc_info;
  thd_proc_info(thd, "Waiting for table");
  DBUG_ENTER("wait_for_condition");
  DEBUG_SYNC(thd, "waiting_for_table");
  if (!thd->killed)
    (void) pthread_cond_wait(cond, mutex);

  /*
    We must unlock mutex first to avoid deadlock becasue conditions are
    sent to this thread by doing locks in the following order:
    lock(mysys_var->mutex)
    lock(mysys_var->current_mutex)

    One by effect of this that one can only use wait_for_condition with
    condition variables that are guranteed to not disapper (freed) even if this
    mutex is unlocked
  */
    
  pthread_mutex_unlock(mutex);
  DEBUG_SYNC(thd, "waiting_for_table_unlock");
  DBUG_EXECUTE_IF("sleep_after_waiting_for_table", my_sleep(1000000););
  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  thd_proc_info(thd, proc_info);
  pthread_mutex_unlock(&thd->mysys_var->mutex);
  DBUG_VOID_RETURN;
}


/**
  Exclusively name-lock a table that is already write-locked by the
  current thread.

  @param thd current thread context
  @param tables table list containing one table to open.

  @return FALSE on success, TRUE otherwise.
*/

bool name_lock_locked_table(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("name_lock_locked_table");

  /* Under LOCK TABLES we must only accept write locked tables. */
  tables->table= find_locked_table(thd, tables->db, tables->table_name);

  if (!tables->table)
    my_error(ER_TABLE_NOT_LOCKED, MYF(0), tables->alias);
  else if (tables->table->reginfo.lock_type < TL_WRITE_LOW_PRIORITY)
    my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE, MYF(0), tables->alias);
  else
  {
    /*
      Ensures that table is opened only by this thread and that no
      other statement will open this table.
    */
    wait_while_table_is_used(thd, tables->table, HA_EXTRA_FORCE_REOPEN);
    DBUG_RETURN(FALSE);
  }

  DBUG_RETURN(TRUE);
}


/*
  Open table which is already name-locked by this thread.

  SYNOPSIS
    reopen_name_locked_table()
      thd         Thread handle
      table_list  TABLE_LIST object for table to be open, TABLE_LIST::table
                  member should point to TABLE object which was used for
                  name-locking.
      link_in     TRUE  - if TABLE object for table to be opened should be
                          linked into THD::open_tables list.
                  FALSE - placeholder used for name-locking is already in
                          this list so we only need to preserve TABLE::next
                          pointer.

  NOTE
    This function assumes that its caller already acquired LOCK_open mutex.

  RETURN VALUE
    FALSE - Success
    TRUE  - Error
*/

bool reopen_name_locked_table(THD* thd, TABLE_LIST* table_list, bool link_in)
{
  TABLE *table= table_list->table;
  TABLE_SHARE *share;
  char *table_name= table_list->table_name;
  TABLE orig_table;
  DBUG_ENTER("reopen_name_locked_table");

  safe_mutex_assert_owner(&LOCK_open);

  if (thd->killed || !table)
    DBUG_RETURN(TRUE);

  orig_table= *table;

  if (open_unireg_entry(thd, table, table_list, table_name,
                        table->s->table_cache_key.str,
                        table->s->table_cache_key.length, thd->mem_root, 0))
  {
    intern_close_table(table);
    /*
      If there was an error during opening of table (for example if it
      does not exist) '*table' object can be wiped out. To be able
      properly release name-lock in this case we should restore this
      object to its original state.
    */
    *table= orig_table;
    DBUG_RETURN(TRUE);
  }

  share= table->s;
  /*
    We want to prevent other connections from opening this table until end
    of statement as it is likely that modifications of table's metadata are
    not yet finished (for example CREATE TRIGGER have to change .TRG file,
    or we might want to drop table if CREATE TABLE ... SELECT fails).
    This also allows us to assume that no other connection will sneak in
    before we will get table-level lock on this table.
  */
  share->version=0;
  table->in_use = thd;
  check_unused();

  if (link_in)
  {
    table->next= thd->open_tables;
    thd->open_tables= table;
  }
  else
  {
    /*
      TABLE object should be already in THD::open_tables list so we just
      need to set TABLE::next correctly.
    */
    table->next= orig_table.next;
  }

  table->tablenr=thd->current_tablenr++;
  table->used_fields=0;
  table->const_table=0;
  table->null_row= table->maybe_null= 0;
  table->force_index= table->force_index_order= table->force_index_group= 0;
  table->status=STATUS_NO_RECORD;
  DBUG_RETURN(FALSE);
}


/**
    Create and insert into table cache placeholder for table
    which will prevent its opening (or creation) (a.k.a lock
    table name).

    @param thd         Thread context
    @param key         Table cache key for name to be locked
    @param key_length  Table cache key length

    @return Pointer to TABLE object used for name locking or 0 in
            case of failure.
*/

TABLE *table_cache_insert_placeholder(THD *thd, const char *key,
                                      uint key_length)
{
  TABLE *table;
  TABLE_SHARE *share;
  char *key_buff;
  DBUG_ENTER("table_cache_insert_placeholder");

  safe_mutex_assert_owner(&LOCK_open);

  /*
    Create a table entry with the right key and with an old refresh version
    Note that we must use my_multi_malloc() here as this is freed by the
    table cache
  */
  if (!my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                       &table, sizeof(*table),
                       &share, sizeof(*share),
                       &key_buff, key_length,
                       NULL))
    DBUG_RETURN(NULL);

  table->s= share;
  share->set_table_cache_key(key_buff, key, key_length);
  share->tmp_table= INTERNAL_TMP_TABLE;  // for intern_close_table
  table->in_use= thd;
  table->locked_by_name=1;

  if (my_hash_insert(&open_cache, (uchar*)table))
  {
    my_free((uchar*) table, MYF(0));
    DBUG_RETURN(NULL);
  }

  DBUG_RETURN(table);
}


/**
    Obtain an exclusive name lock on the table if it is not cached
    in the table cache.

    @param      thd         Thread context
    @param      db          Name of database
    @param      table_name  Name of table
    @param[out] table       Out parameter which is either:
                            - set to NULL if table cache contains record for
                              the table or
                            - set to point to the TABLE instance used for
                              name-locking.

    @note This function takes into account all records for table in table
          cache, even placeholders used for name-locking. This means that
          'table' parameter can be set to NULL for some situations when
          table does not really exist.

    @retval  TRUE   Error occured (OOM)
    @retval  FALSE  Success. 'table' parameter set according to above rules.
*/

bool lock_table_name_if_not_cached(THD *thd, const char *db,
                                   const char *table_name, TABLE **table)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  DBUG_ENTER("lock_table_name_if_not_cached");

  key_length= create_table_def_key(key, db, table_name);
  VOID(pthread_mutex_lock(&LOCK_open));

  if (hash_search(&open_cache, (uchar *)key, key_length))
  {
    VOID(pthread_mutex_unlock(&LOCK_open));
    DBUG_PRINT("info", ("Table is cached, name-lock is not obtained"));
    *table= 0;
    DBUG_RETURN(FALSE);
  }
  if (!(*table= table_cache_insert_placeholder(thd, key, key_length)))
  {
    VOID(pthread_mutex_unlock(&LOCK_open));
    DBUG_RETURN(TRUE);
  }
  (*table)->open_placeholder= 1;
  (*table)->next= thd->open_tables;
  thd->open_tables= *table;
  VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_RETURN(FALSE);
}


/**
    Check that table exists in table definition cache, on disk
    or in some storage engine.

    @param       thd     Thread context
    @param       table   Table list element
    @param[out]  exists  Out parameter which is set to TRUE if table
                         exists and to FALSE otherwise.

    @note This function assumes that caller owns LOCK_open mutex.
          It also assumes that the fact that there are no name-locks
          on the table was checked beforehand.

    @note If there is no .FRM file for the table but it exists in one
          of engines (e.g. it was created on another node of NDB cluster)
          this function will fetch and create proper .FRM file for it.

    @retval  TRUE   Some error occured
    @retval  FALSE  No error. 'exists' out parameter set accordingly.
*/

bool check_if_table_exists(THD *thd, TABLE_LIST *table, bool *exists)
{
  char path[FN_REFLEN + 1];
  int rc;
  DBUG_ENTER("check_if_table_exists");

  safe_mutex_assert_owner(&LOCK_open);

  *exists= TRUE;

  if (get_cached_table_share(table->db, table->table_name))
    DBUG_RETURN(FALSE);

  build_table_filename(path, sizeof(path) - 1, table->db, table->table_name,
                       reg_ext, 0);

  if (!access(path, F_OK))
    DBUG_RETURN(FALSE);

  /* .FRM file doesn't exist. Check if some engine can provide it. */

  rc= ha_create_table_from_engine(thd, table->db, table->table_name);

  if (rc < 0)
  {
    /* Table does not exists in engines as well. */
    *exists= FALSE;
    DBUG_RETURN(FALSE);
  }
  else if (!rc)
  {
    /* Table exists in some engine and .FRM for it was created. */
    DBUG_RETURN(FALSE);
  }
  else /* (rc > 0) */
  {
    my_printf_error(ER_UNKNOWN_ERROR, "Failed to open '%-.64s', error while "
                    "unpacking from engine", MYF(0), table->table_name);
    DBUG_RETURN(TRUE);
  }
}


/*
  Open a table.

  SYNOPSIS
    open_table()
    thd                 Thread context.
    table_list          Open first table in list.
    refresh      INOUT  Pointer to memory that will be set to 1 if
                        we need to close all tables and reopen them.
                        If this is a NULL pointer, then the table is not
                        put in the thread-open-list.
    flags               Bitmap of flags to modify how open works:
                          MYSQL_LOCK_IGNORE_FLUSH - Open table even if
                          someone has done a flush on it.
                          No version number checking is done.
                          MYSQL_OPEN_TEMPORARY_ONLY - Open only temporary
                          table not the base table or view.

  IMPLEMENTATION
    Uses a cache of open tables to find a table not in use.

    If table list element for the table to be opened has "create" flag
    set and table does not exist, this function will automatically insert
    a placeholder for exclusive name lock into the open tables cache and
    will return the TABLE instance that corresponds to this placeholder.

  RETURN
    NULL  Open failed.  If refresh is set then one should close
          all other tables and retry the open.
    #     Success. Pointer to TABLE object for open table.
*/


TABLE *open_table(THD *thd, TABLE_LIST *table_list, MEM_ROOT *mem_root,
		  bool *refresh, uint flags)
{
  reg1	TABLE *table;
  char	key[MAX_DBKEY_LENGTH];
  uint	key_length;
  char	*alias= table_list->alias;
  HASH_SEARCH_STATE state;
  DBUG_ENTER("open_table");

  /* Parsing of partitioning information from .frm needs thd->lex set up. */
  DBUG_ASSERT(thd->lex->is_lex_started);

  /* find a unused table in the open table cache */
  if (refresh)
    *refresh=0;

  /* an open table operation needs a lot of the stack space */
  if (check_stack_overrun(thd, STACK_MIN_SIZE_FOR_OPEN, (uchar *)&alias))
    DBUG_RETURN(0);

  if (thd->killed)
    DBUG_RETURN(0);

  key_length= (create_table_def_key(thd, key, table_list, 1) -
               TMP_TABLE_KEY_EXTRA);

  /*
    Unless requested otherwise, try to resolve this table in the list
    of temporary tables of this thread. In MySQL temporary tables
    are always thread-local and "shadow" possible base tables with the
    same name. This block implements the behaviour.
    TODO: move this block into a separate function.
  */
  if (!table_list->skip_temporary)
  {
    for (table= thd->temporary_tables; table ; table=table->next)
    {
      if (table->s->table_cache_key.length == key_length +
          TMP_TABLE_KEY_EXTRA &&
	  !memcmp(table->s->table_cache_key.str, key,
		  key_length + TMP_TABLE_KEY_EXTRA))
      {
        /*
          We're trying to use the same temporary table twice in a query.
          Right now we don't support this because a temporary table
          is always represented by only one TABLE object in THD, and
          it can not be cloned. Emit an error for an unsupported behaviour.
        */
	if (table->query_id)
	{
          DBUG_PRINT("error",
                     ("query_id: %lu  server_id: %u  pseudo_thread_id: %lu",
                      (ulong) table->query_id, (uint) thd->server_id,
                      (ulong) thd->variables.pseudo_thread_id));
	  my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->alias);
	  DBUG_RETURN(0);
	}
	table->query_id= thd->query_id;
	thd->thread_specific_used= TRUE;
        DBUG_PRINT("info",("Using temporary table"));
        goto reset;
      }
    }
  }

  if (flags & MYSQL_OPEN_TEMPORARY_ONLY)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), table_list->db, table_list->table_name);
    DBUG_RETURN(0);
  }

  /*
    The table is not temporary - if we're in pre-locked or LOCK TABLES
    mode, let's try to find the requested table in the list of pre-opened
    and locked tables. If the table is not there, return an error - we can't
    open not pre-opened tables in pre-locked/LOCK TABLES mode.
    TODO: move this block into a separate function.
  */
  if (thd->locked_tables || thd->prelocked_mode)
  {						// Using table locks
    TABLE *best_table= 0;
    int best_distance= INT_MIN;
    for (table=thd->open_tables; table ; table=table->next)
    {
      if (table->s->table_cache_key.length == key_length &&
	  !memcmp(table->s->table_cache_key.str, key, key_length))
      {
        /*
          When looking for a usable TABLE, ignore MERGE children, as they
          belong to their parent and cannot be used explicitly.
        */
        if (!my_strcasecmp(system_charset_info, table->alias, alias) &&
            table->query_id != thd->query_id && /* skip tables already used */
            !(thd->prelocked_mode && table->query_id) &&
            !table->parent)
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
    */
    {
      char path[FN_REFLEN + 1];
      enum legacy_db_type not_used;
      build_table_filename(path, sizeof(path) - 1,
                           table_list->db, table_list->table_name, reg_ext, 0);
      if (mysql_frm_type(thd, path, &not_used) == FRMTYPE_VIEW)
      {
        /*
          Will not be used (because it's VIEW) but has to be passed.
          Also we will not free it (because it is a stack variable).
        */
        TABLE tab;
        table= &tab;
        VOID(pthread_mutex_lock(&LOCK_open));
        if (!open_unireg_entry(thd, table, table_list, alias,
                              key, key_length, mem_root, 0))
        {
          DBUG_ASSERT(table_list->view != 0);
          VOID(pthread_mutex_unlock(&LOCK_open));
          DBUG_RETURN(0); // VIEW
        }
        VOID(pthread_mutex_unlock(&LOCK_open));
      }
    }
    /*
      No table in the locked tables list. In case of explicit LOCK TABLES
      this can happen if a user did not include the able into the list.
      In case of pre-locked mode locked tables list is generated automatically,
      so we may only end up here if the table did not exist when
      locked tables list was created.
    */
    if (thd->prelocked_mode == PRELOCKED)
      my_error(ER_NO_SUCH_TABLE, MYF(0), table_list->db, table_list->alias);
    else
      my_error(ER_TABLE_NOT_LOCKED, MYF(0), alias);
    DBUG_RETURN(0);
  }

  /*
    Non pre-locked/LOCK TABLES mode, and the table is not temporary:
    this is the normal use case.
    Now we should:
    - try to find the table in the table cache.
    - if one of the discovered TABLE instances is name-locked
      (table->s->version == 0) or some thread has started FLUSH TABLES
      (refresh_version > table->s->version), back off -- we have to wait
      until no one holds a name lock on the table.
    - if there is no such TABLE in the name cache, read the table definition
    and insert it into the cache.
    We perform all of the above under LOCK_open which currently protects
    the open cache (also known as table cache) and table definitions stored
    on disk.
  */

  VOID(pthread_mutex_lock(&LOCK_open));

  /*
    If it's the first table from a list of tables used in a query,
    remember refresh_version (the version of open_cache state).
    If the version changes while we're opening the remaining tables,
    we will have to back off, close all the tables opened-so-far,
    and try to reopen them.
    Note: refresh_version is currently changed only during FLUSH TABLES.
  */
  if (!thd->open_tables)
    thd->version=refresh_version;
  else if ((thd->version != refresh_version) &&
           ! (flags & MYSQL_LOCK_IGNORE_FLUSH))
  {
    /* Someone did a refresh while thread was opening tables */
    if (refresh)
      *refresh=1;
    VOID(pthread_mutex_unlock(&LOCK_open));
    DBUG_RETURN(0);
  }

  /*
    In order for the back off and re-start process to work properly,
    handler tables having old versions (due to FLUSH TABLES or pending
    name-lock) MUST be closed. This is specially important if a name-lock
    is pending for any table of the handler_tables list, otherwise a
    deadlock may occur.
  */
  if (thd->handler_tables)
    mysql_ha_flush(thd);

  /*
    Actually try to find the table in the open_cache.
    The cache may contain several "TABLE" instances for the same
    physical table. The instances that are currently "in use" by
    some thread have their "in_use" member != NULL.
    There is no good reason for having more than one entry in the
    hash for the same physical table, except that we use this as
    an implicit "pending locks queue" - see
    wait_for_locked_table_names for details.
  */
  for (table= (TABLE*) hash_first(&open_cache, (uchar*) key, key_length,
                                  &state);
       table && table->in_use ;
       table= (TABLE*) hash_next(&open_cache, (uchar*) key, key_length,
                                 &state))
  {
    DBUG_PRINT("tcache", ("in_use table: '%s'.'%s' 0x%lx", table->s->db.str,
                          table->s->table_name.str, (long) table));
    /*
      Here we flush tables marked for flush.
      Normally, table->s->version contains the value of
      refresh_version from the moment when this table was
      (re-)opened and added to the cache.
      If since then we did (or just started) FLUSH TABLES
      statement, refresh_version has been increased.
      For "name-locked" TABLE instances, table->s->version is set
      to 0 (see lock_table_name for details).
      In case there is a pending FLUSH TABLES or a name lock, we
      need to back off and re-start opening tables.
      If we do not back off now, we may dead lock in case of lock
      order mismatch with some other thread:
      c1: name lock t1; -- sort of exclusive lock 
      c2: open t2;      -- sort of shared lock
      c1: name lock t2; -- blocks
      c2: open t1; -- blocks
    */
    if (table->needs_reopen_or_name_lock())
    {
      DBUG_PRINT("note",
                 ("Found table '%s.%s' with different refresh version",
                  table_list->db, table_list->table_name));

      /* Ignore FLUSH and pending name locks, but not acquired name locks! */
      if (flags & MYSQL_LOCK_IGNORE_FLUSH && !table->open_placeholder)
      {
        /* Force close at once after usage */
        thd->version= table->s->version;
        continue;
      }

      /* Avoid self-deadlocks by detecting self-dependencies. */
      if (table->open_placeholder && table->in_use == thd)
      {
	VOID(pthread_mutex_unlock(&LOCK_open));
        my_error(ER_UPDATE_TABLE_USED, MYF(0), table->s->table_name.str);
        DBUG_RETURN(0);
      }

      /*
        Back off, part 1: mark the table as "unused" for the
        purpose of name-locking by setting table->db_stat to 0. Do
        that only for the tables in this thread that have an old
        table->s->version (this is an optimization (?)).
        table->db_stat == 0 signals wait_for_locked_table_names
        that the tables in question are not used any more. See
        table_is_used call for details.

        Notice that HANDLER tables were already taken care of by
        the earlier call to mysql_ha_flush() in this same critical
        section.
      */
      close_old_data_files(thd,thd->open_tables,0,0);
      /*
        Back-off part 2: try to avoid "busy waiting" on the table:
        if the table is in use by some other thread, we suspend
        and wait till the operation is complete: when any
        operation that juggles with table->s->version completes,
        it broadcasts COND_refresh condition variable.
        If 'old' table we met is in use by current thread we return
        without waiting since in this situation it's this thread
        which is responsible for broadcasting on COND_refresh
        (and this was done already in close_old_data_files()).
        Good example of such situation is when we have statement
        that needs two instances of table and FLUSH TABLES comes
        after we open first instance but before we open second
        instance.
      */
      if (table->in_use != thd)
      {
        /* wait_for_conditionwill unlock LOCK_open for us */
        wait_for_condition(thd, &LOCK_open, &COND_refresh);
      }
      else
      {
	VOID(pthread_mutex_unlock(&LOCK_open));
      }
      /*
        There is a refresh in progress for this table.
        Signal the caller that it has to try again.
      */
      if (refresh)
	*refresh=1;
      DBUG_RETURN(0);
    }
  }
  if (table)
  {
    DBUG_PRINT("tcache", ("unused table: '%s'.'%s' 0x%lx", table->s->db.str,
                          table->s->table_name.str, (long) table));
    /* Unlink the table from "unused_tables" list. */
    if (table == unused_tables)
    {						// First unused
      unused_tables=unused_tables->next;	// Remove from link
      if (table == unused_tables)
	unused_tables=0;
    }
    table->prev->next=table->next;		/* Remove from unused list */
    table->next->prev=table->prev;
    table->in_use= thd;
  }
  else
  {
    /* Insert a new TABLE instance into the open cache */
    int error;
    DBUG_PRINT("tcache", ("opening new table"));
    /* Free cache if too big */
    while (open_cache.records > table_cache_size && unused_tables)
      VOID(hash_delete(&open_cache,(uchar*) unused_tables)); /* purecov: tested */

    if (table_list->create)
    {
      bool exists;

      if (check_if_table_exists(thd, table_list, &exists))
      {
        VOID(pthread_mutex_unlock(&LOCK_open));
        DBUG_RETURN(NULL);
      }

      if (!exists)
      {
        /*
          Table to be created, so we need to create placeholder in table-cache.
        */
        if (!(table= table_cache_insert_placeholder(thd, key, key_length)))
        {
          VOID(pthread_mutex_unlock(&LOCK_open));
          DBUG_RETURN(NULL);
        }
        /*
          Link placeholder to the open tables list so it will be automatically
          removed once tables are closed. Also mark it so it won't be ignored
          by other trying to take name-lock.
        */
        table->open_placeholder= 1;
        table->next= thd->open_tables;
        thd->open_tables= table;
        VOID(pthread_mutex_unlock(&LOCK_open));
        DBUG_RETURN(table);
      }
      /* Table exists. Let us try to open it. */
    }

    /* make a new table */
    if (!(table=(TABLE*) my_malloc(sizeof(*table),MYF(MY_WME))))
    {
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }

    error= open_unireg_entry(thd, table, table_list, alias, key, key_length,
                             mem_root, (flags & OPEN_VIEW_NO_PARSE));
    if (error > 0)
    {
      my_free((uchar*)table, MYF(0));
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }
    if (table_list->view || error < 0)
    {
      /*
        VIEW not really opened, only frm were read.
        Set 1 as a flag here
      */
      if (error < 0)
        table_list->view= (st_lex*)1;

      my_free((uchar*)table, MYF(0));
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(0); // VIEW
    }
    DBUG_PRINT("info", ("inserting table '%s'.'%s' 0x%lx into the cache",
                        table->s->db.str, table->s->table_name.str,
                        (long) table));
    if (my_hash_insert(&open_cache,(uchar*) table))
    {
      my_free(table, MYF(0));
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }
  }

  check_unused();				// Debugging call

  VOID(pthread_mutex_unlock(&LOCK_open));
  if (refresh)
  {
    table->next=thd->open_tables;		/* Link into simple list */
    thd->open_tables=table;
  }
  table->reginfo.lock_type=TL_READ;		/* Assume read */

 reset:
  DBUG_ASSERT(table->s->ref_count > 0 || table->s->tmp_table != NO_TMP_TABLE);

  if (thd->lex->need_correct_ident())
    table->alias_name_used= my_strcasecmp(table_alias_charset,
                                          table->s->table_name.str, alias);
  /* Fix alias if table name changes */
  if (strcmp(table->alias, alias))
  {
    uint length=(uint) strlen(alias)+1;
    table->alias= (char*) my_realloc((char*) table->alias, length,
                                     MYF(MY_WME));
    memcpy((char*) table->alias, alias, length);
  }
  /* These variables are also set in reopen_table() */
  table->tablenr=thd->current_tablenr++;
  table->used_fields=0;
  table->const_table=0;
  table->null_row= table->maybe_null= 0;
  table->force_index= table->force_index_order= table->force_index_group= 0;
  table->status=STATUS_NO_RECORD;
  table->insert_values= 0;
  table->fulltext_searched= 0;
  table->file->ft_handler= 0;
  /*
    Check that there is no reference to a condtion from an earlier query
    (cf. Bug#58553). 
  */
  DBUG_ASSERT(table->file->pushed_cond == NULL);
  table->reginfo.impossible_range= 0;
  /* Catch wrong handling of the auto_increment_field_not_null. */
  DBUG_ASSERT(!table->auto_increment_field_not_null);
  table->auto_increment_field_not_null= FALSE;
  if (table->timestamp_field)
    table->timestamp_field_type= table->timestamp_field->get_auto_set_type();
  table->pos_in_table_list= table_list;
  table_list->updatable= 1; // It is not derived table nor non-updatable VIEW
  table->clear_column_bitmaps();
  DBUG_ASSERT(table->key_read == 0);
  DBUG_RETURN(table);
}


TABLE *find_locked_table(THD *thd, const char *db,const char *table_name)
{
  char	key[MAX_DBKEY_LENGTH];
  uint key_length= create_table_def_key(key, db, table_name);

  for (TABLE *table=thd->open_tables; table ; table=table->next)
  {
    if (table->s->table_cache_key.length == key_length &&
	!memcmp(table->s->table_cache_key.str, key, key_length))
      return table;
  }
  return(0);
}


/*
  Reopen an table because the definition has changed.

  SYNOPSIS
    reopen_table()
    table	Table object

  NOTES
   The data file for the table is already closed and the share is released
   The table has a 'dummy' share that mainly contains database and table name.

 RETURN
   0  ok
   1  error. The old table object is not changed.
*/

bool reopen_table(TABLE *table)
{
  TABLE tmp;
  bool error= 1;
  Field **field;
  uint key,part;
  TABLE_LIST table_list;
  THD *thd= table->in_use;
  DBUG_ENTER("reopen_table");
  DBUG_PRINT("tcache", ("table: '%s'.'%s' 0x%lx", table->s->db.str,
                        table->s->table_name.str, (long) table));

  DBUG_ASSERT(table->s->ref_count == 0);
  DBUG_ASSERT(!table->sort.io_cache);
  DBUG_ASSERT(!table->children_attached);

#ifdef EXTRA_DEBUG
  if (table->db_stat)
    sql_print_error("Table %s had a open data handler in reopen_table",
		    table->alias);
#endif
  bzero((char*) &table_list, sizeof(TABLE_LIST));
  table_list.db=         table->s->db.str;
  table_list.table_name= table->s->table_name.str;
  table_list.table=      table;

  if (wait_for_locked_table_names(thd, &table_list))
    DBUG_RETURN(1);                             // Thread was killed

  if (open_unireg_entry(thd, &tmp, &table_list,
			table->alias,
                        table->s->table_cache_key.str,
                        table->s->table_cache_key.length,
                        thd->mem_root, 0))
    goto end;

  /* This list copies variables set by open_table */
  tmp.tablenr=		table->tablenr;
  tmp.used_fields=	table->used_fields;
  tmp.const_table=	table->const_table;
  tmp.null_row=		table->null_row;
  tmp.maybe_null=	table->maybe_null;
  tmp.status=		table->status;

  /* Get state */
  tmp.in_use=    	thd;
  tmp.reginfo.lock_type=table->reginfo.lock_type;
  tmp.grant=		table->grant;

  /* Replace table in open list */
  tmp.next=		table->next;
  tmp.prev=		table->prev;

  /* Preserve MERGE parent. */
  tmp.parent=           table->parent;
  /* Fix MERGE child list and check for unchanged union. */
  if ((table->child_l || tmp.child_l) &&
      fix_merge_after_open(table->child_l, table->child_last_l,
                           tmp.child_l, tmp.child_last_l))
  {
    VOID(closefrm(&tmp, 1)); // close file, free everything
    goto end;
  }

  delete table->triggers;
  if (table->file)
    VOID(closefrm(table, 1));		// close file, free everything

  *table= tmp;
  table->default_column_bitmaps();
  table->file->change_table_ptr(table, table->s);

  DBUG_ASSERT(table->alias != 0);
  for (field=table->field ; *field ; field++)
  {
    (*field)->table= (*field)->orig_table= table;
    (*field)->table_name= &table->alias;
  }
  for (key=0 ; key < table->s->keys ; key++)
  {
    for (part=0 ; part < table->key_info[key].usable_key_parts ; part++)
    {
      table->key_info[key].key_part[part].field->table= table;
      table->key_info[key].key_part[part].field->orig_table= table;
    }
  }
  if (table->triggers)
    table->triggers->set_table(table);
  /*
    Do not attach MERGE children here. The children might be reopened
    after the parent. Attach children after reopening all tables that
    require reopen. See for example reopen_tables().
  */

  broadcast_refresh();
  error=0;

 end:
  DBUG_RETURN(error);
}


/**
    Close all instances of a table open by this thread and replace
    them with exclusive name-locks.

    @param thd        Thread context
    @param db         Database name for the table to be closed
    @param table_name Name of the table to be closed

    @note This function assumes that if we are not under LOCK TABLES,
          then there is only one table open and locked. This means that
          the function probably has to be adjusted before it can be used
          anywhere outside ALTER TABLE.

    @note Must not use TABLE_SHARE::table_name/db of the table being closed,
          the strings are used in a loop even after the share may be freed.
*/

void close_data_files_and_morph_locks(THD *thd, const char *db,
                                      const char *table_name)
{
  TABLE *table;
  DBUG_ENTER("close_data_files_and_morph_locks");

  safe_mutex_assert_owner(&LOCK_open);

  if (thd->lock)
  {
    /*
      If we are not under LOCK TABLES we should have only one table
      open and locked so it makes sense to remove the lock at once.
    */
    mysql_unlock_tables(thd, thd->lock);
    thd->lock= 0;
  }

  /*
    Note that open table list may contain a name-lock placeholder
    for target table name if we process ALTER TABLE ... RENAME.
    So loop below makes sense even if we are not under LOCK TABLES.
  */
  for (table=thd->open_tables; table ; table=table->next)
  {
    if (!strcmp(table->s->table_name.str, table_name) &&
	!strcmp(table->s->db.str, db))
    {
      if (thd->locked_tables)
      {
        if (table->parent)
        {
          /*
            If MERGE child, need to reopen parent too. This means that
            the first child to be closed will detach all children from
            the parent and close it. OTOH in most cases a MERGE table
            won't have multiple children with the same db.table_name.
          */
          mysql_lock_remove(thd, thd->locked_tables, table->parent, TRUE);
          table->parent->open_placeholder= 1;
          close_handle_and_leave_table_as_lock(table->parent);
        }
        else
          mysql_lock_remove(thd, thd->locked_tables, table, TRUE);
      }
      table->open_placeholder= 1;
      close_handle_and_leave_table_as_lock(table);
    }
  }
  DBUG_VOID_RETURN;
}


/**
  @brief Mark merge parent and children with bad_merge_marker

  @param[in,out]  parent       the TABLE object of the parent
*/

static void mark_merge_parent_and_children_as_bad(TABLE *parent)
{
  TABLE_LIST *child_l;
  DBUG_ENTER("mark_merge_parent_and_children_as_bad");
  parent->parent= &bad_merge_marker;
  for (child_l= parent->child_l; ; child_l= child_l->next_global)
  {
    child_l->table->parent= &bad_merge_marker;
    child_l->table= NULL;
    if (&child_l->next_global == parent->child_last_l)
      break;
  }
  DBUG_VOID_RETURN;
}


/**
  Reattach MERGE children after reopen.

  @param[in]     thd            thread context

  @note If reattach failed for certain MERGE table, the table (and all
        it's children) are marked with bad_merge_marker.

  @return       status
    @retval     FALSE           OK
    @retval     TRUE            Error
*/

static bool reattach_merge(THD *thd)
{
  TABLE *table;
  bool error= FALSE;
  DBUG_ENTER("reattach_merge");

  for (table= thd->open_tables; table; table= table->next)
  {
    DBUG_PRINT("tcache", ("check table: '%s'.'%s' 0x%lx",
                          table->s->db.str, table->s->table_name.str,
                          (long) table));
    /*
      Reattach children only for MERGE tables that had children or parent
      with "closed data files" and were reopen. For extra safety skip MERGE
      tables which we failed to reopen (should not happen with current code).
    */
    if (table->child_l && table->parent != &bad_merge_marker &&
        !table->children_attached)
    {
      DBUG_PRINT("tcache", ("MERGE parent, attach children"));
      if (table->file->extra(HA_EXTRA_ATTACH_CHILDREN))
      {
        my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->alias);
        error= TRUE;
        mark_merge_parent_and_children_as_bad(table);
      }
      else
      {
        table->children_attached= TRUE;
        DBUG_PRINT("myrg", ("attached parent: '%s'.'%s' 0x%lx",
                            table->s->db.str,
                            table->s->table_name.str, (long) table));
      }
    }
  }
  DBUG_RETURN(error);
}


/**
    Reopen all tables with closed data files.

    @param thd         Thread context
    @param get_locks   Should we get locks after reopening tables ?
    @param mark_share_as_old  Mark share as old to protect from a impending
                              global read lock.

    @note Since this function can't properly handle prelocking and
          create placeholders it should be used in very special
          situations like FLUSH TABLES or ALTER TABLE. In general
          case one should just repeat open_tables()/lock_tables()
          combination when one needs tables to be reopened (for
          example see open_and_lock_tables()).

    @note One should have lock on LOCK_open when calling this.

    @return FALSE in case of success, TRUE - otherwise.
*/

bool reopen_tables(THD *thd, bool get_locks, bool mark_share_as_old)
{
  TABLE *table,*next,**prev;
  TABLE **tables,**tables_ptr;			// For locks
  bool error=0, not_used;
  bool merge_table_found= FALSE;
  const uint flags= MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN |
                    MYSQL_LOCK_IGNORE_GLOBAL_READ_LOCK |
                    MYSQL_LOCK_IGNORE_FLUSH;

  DBUG_ENTER("reopen_tables");

  if (!thd->open_tables)
    DBUG_RETURN(0);

  safe_mutex_assert_owner(&LOCK_open);
  if (get_locks)
  {
    /*
      The ptr is checked later
      Do not handle locks of MERGE children.
    */
    uint opens=0;
    for (table= thd->open_tables; table ; table=table->next)
      if (!table->parent)
        opens++;
    DBUG_PRINT("tcache", ("open tables to lock: %u", opens));
    tables= (TABLE**) my_alloca(sizeof(TABLE*)*opens);
  }
  else
    tables= &thd->open_tables;
  tables_ptr =tables;

  prev= &thd->open_tables;
  for (table=thd->open_tables; table ; table=next)
  {
    uint db_stat=table->db_stat;
    TABLE *parent= table->child_l ? table : table->parent;
    next=table->next;
    DBUG_PRINT("tcache", ("open table: '%s'.'%s' 0x%lx  "
                          "parent: 0x%lx  db_stat: %u",
                          table->s->db.str, table->s->table_name.str,
                          (long) table, (long) table->parent, db_stat));
    /*
      If we need to reopen child or parent table in a MERGE table, then
      children in this MERGE table has to be already detached at this
      point.
    */
    DBUG_ASSERT(db_stat || !parent || !parent->children_attached);
    /*
      Thanks to the above assumption the below condition will guarantee that
      merge_table_found is TRUE when we need to reopen child or parent table.
      Note that it works even in situation when it is only a child and not a
      parent that needs reopen (this can happen when get_locks == FALSE).
    */
    if (table->child_l && !table->children_attached)
      merge_table_found= TRUE;

    if (!tables)
    {
      /*
        If we could not allocate 'tables' we close ALL open tables here.
        Before closing MERGE child or parent we need to detach children
        and/or clear references in/to them.
      */
      if (parent)
        detach_merge_children(table, TRUE);
    }
    else if (table->parent == &bad_merge_marker)
    {
      /*
        This is either a child or a parent of a MERGE table for which
        we already decided that we are unable to reopen it. Close it.

        Reset parent reference, it may be used while freeing the table.
      */
      table->parent= NULL;
    }
    else if (!db_stat && reopen_table(table))
    {
      /*
        If we fail to reopen a child or a parent in a MERGE table and the
        MERGE table is affected for the first time, mark all relevant tables
        invalid. Otherwise handle it as usual.

        All in all we must end up with:
        - child tables are detached from parent. This was done earlier,
          but child<->parent references were kept valid for reopen.
        - parent is not in the to-be-locked tables
        - all child tables and parent are not in the THD::open_tables.
        - all child tables and parent are not in the open_cache.

        Please note that below we do additional pass through THD::open_tables
        list to achieve the last three points.
      */
      if (parent)
      {
        mark_merge_parent_and_children_as_bad(parent);
        table->parent= NULL;
      }
    }
    else
    {
      DBUG_PRINT("tcache", ("opened. need lock: %d",
                            get_locks && !db_stat && !table->parent));
      *prev= table;
      prev= &table->next;
      /* Do not handle locks of MERGE children. */
      if (get_locks && !db_stat && !table->parent)
	*tables_ptr++= table;			// need new lock on this
      if (mark_share_as_old)
      {
	table->s->version=0;
	table->open_placeholder= 0;
      }
      continue;
    }
    my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->alias);
    VOID(hash_delete(&open_cache, (uchar *) table));
    error= 1;
  }
  *prev=0;
  /*
    When all tables are open again, we can re-attach MERGE children to
    their parents.

    If there was an error while reopening a child or a parent of a MERGE
    table, or while reattaching child tables to their parents, some tables
    may have been kept open but marked for close with bad_merge_marker.
    Close these tables now.
  */
  if (tables && merge_table_found && (error|= reattach_merge(thd)))
  {
    prev= &thd->open_tables;
    for (table= thd->open_tables; table; table= next)
    {
      next= table->next;
      if (table->parent == &bad_merge_marker)
      {
        /* Remove merge parent from to-be-locked tables array. */
        if (get_locks && table->child_l)
        {
          TABLE **t;
          for (t= tables; t < tables_ptr; t++)
          {
            if (*t == table)
            {
              tables_ptr--;
              memmove(t, t + 1, (tables_ptr - t) * sizeof(TABLE *));
              break;
            }
          }
        }
        /* Reset parent reference, it may be used while freeing the table. */
        table->parent= NULL;
        /* Free table. */
        VOID(hash_delete(&open_cache, (uchar *) table));
      }
      else
      {
        *prev= table;
        prev= &table->next;
      }
    }
    *prev= 0;
  }
  DBUG_PRINT("tcache", ("open tables to lock: %u",
                        (uint) (tables_ptr - tables)));
  if (tables != tables_ptr)			// Should we get back old locks
  {
    MYSQL_LOCK *lock;
    /*
      We should always get these locks. Anyway, we must not go into
      wait_for_tables() as it tries to acquire LOCK_open, which is
      already locked.
    */
    thd->some_tables_deleted=0;
    if ((lock= mysql_lock_tables(thd, tables, (uint) (tables_ptr - tables),
                                 flags, &not_used)))
    {
      thd->locked_tables=mysql_lock_merge(thd->locked_tables,lock);
    }
    else
    {
      /*
        This case should only happen if there is a bug in the reopen logic.
        Need to issue error message to have a reply for the application.
        Not exactly what happened though, but close enough.
      */
      my_error(ER_LOCK_DEADLOCK, MYF(0));
      error=1;
    }
  }
  if (get_locks && tables)
  {
    my_afree((uchar*) tables);
  }
  broadcast_refresh();
  DBUG_RETURN(error);
}


/**
    Close handlers for tables in list, but leave the TABLE structure
    intact so that we can re-open these quickly.

    @param thd           Thread context
    @param table         Head of the list of TABLE objects
    @param morph_locks   TRUE  - remove locks which we have on tables being closed
                                 but ensure that no DML or DDL will sneak in before
                                 we will re-open the table (i.e. temporarily morph
                                 our table-level locks into name-locks).
                         FALSE - otherwise
    @param send_refresh  Should we awake waiters even if we didn't close any tables?
*/

static void close_old_data_files(THD *thd, TABLE *table, bool morph_locks,
                                 bool send_refresh)
{
  bool found= send_refresh;
  DBUG_ENTER("close_old_data_files");

  for (; table ; table=table->next)
  {
    DBUG_PRINT("tcache", ("checking table: '%s'.'%s' 0x%lx",
                          table->s->db.str, table->s->table_name.str,
                          (long) table));
    DBUG_PRINT("tcache", ("needs refresh: %d  is open: %u",
                          table->needs_reopen_or_name_lock(), table->db_stat));
    /*
      Reopen marked for flush.
    */
    if (table->needs_reopen_or_name_lock())
    {
      found=1;
      if (table->db_stat)
      {
	if (morph_locks)
	{
          /*
            Forward lock handling to MERGE parent. But unlock parent
            once only.
          */
          TABLE *ulcktbl= table->parent ? table->parent : table;
          if (ulcktbl->lock_count)
          {
            /*
              Wake up threads waiting for table-level lock on this table
              so they won't sneak in when we will temporarily remove our
              lock on it. This will also give them a chance to close their
              instances of this table.
            */
            mysql_lock_abort(thd, ulcktbl, TRUE);
            mysql_lock_remove(thd, thd->locked_tables, ulcktbl, TRUE);
            ulcktbl->lock_count= 0;
          }
          if ((ulcktbl != table) && ulcktbl->db_stat)
          {
            /*
              Close the parent too. Note that parent can come later in
              the list of tables. It will then be noticed as closed and
              as a placeholder. When this happens, do not clear the
              placeholder flag. See the branch below ("***").
            */
            ulcktbl->open_placeholder= 1;
            close_handle_and_leave_table_as_lock(ulcktbl);
          }
          /*
            We want to protect the table from concurrent DDL operations
            (like RENAME TABLE) until we will re-open and re-lock it.
          */
	  table->open_placeholder= 1;
	}
        close_handle_and_leave_table_as_lock(table);
      }
      else if (table->open_placeholder && !morph_locks)
      {
        /*
          We come here only in close-for-back-off scenario. So we have to
          "close" create placeholder here to avoid deadlocks (for example,
          in case of concurrent execution of CREATE TABLE t1 SELECT * FROM t2
          and RENAME TABLE t2 TO t1). In close-for-re-open scenario we will
          probably want to let it stay.

          Note "***": We must not enter this branch if the placeholder
          flag has been set because of a former close through a child.
          See above the comment that refers to this note.
        */
        table->open_placeholder= 0;
      }
    }
  }
  if (found)
    broadcast_refresh();
  DBUG_VOID_RETURN;
}


/*
  Wait until all threads has closed the tables in the list
  We have also to wait if there is thread that has a lock on this table even
  if the table is closed
*/

bool table_is_used(TABLE *table, bool wait_for_name_lock)
{
  DBUG_ENTER("table_is_used");
  do
  {
    char *key= table->s->table_cache_key.str;
    uint key_length= table->s->table_cache_key.length;

    DBUG_PRINT("loop", ("table_name: %s", table->alias));
    HASH_SEARCH_STATE state;
    for (TABLE *search= (TABLE*) hash_first(&open_cache, (uchar*) key,
                                             key_length, &state);
	 search ;
         search= (TABLE*) hash_next(&open_cache, (uchar*) key,
                                    key_length, &state))
    {
      DBUG_PRINT("info", ("share: 0x%lx  "
                          "open_placeholder: %d  locked_by_name: %d "
                          "db_stat: %u  version: %lu",
                          (ulong) search->s,
                          search->open_placeholder, search->locked_by_name,
                          search->db_stat,
                          search->s->version));
      if (search->in_use == table->in_use)
        continue;                               // Name locked by this thread
      /*
        We can't use the table under any of the following conditions:
        - There is an name lock on it (Table is to be deleted or altered)
        - If we are in flush table and we didn't execute the flush
        - If the table engine is open and it's an old version
        (We must wait until all engines are shut down to use the table)
      */
      if ( (search->locked_by_name && wait_for_name_lock) ||
           (search->is_name_opened() && search->needs_reopen_or_name_lock()))
        DBUG_RETURN(1);
    }
  } while ((table=table->next));
  DBUG_RETURN(0);
}


/* Wait until all used tables are refreshed */

bool wait_for_tables(THD *thd)
{
  bool result;
  DBUG_ENTER("wait_for_tables");

  thd_proc_info(thd, "Waiting for tables");
  pthread_mutex_lock(&LOCK_open);
  while (!thd->killed)
  {
    thd->some_tables_deleted=0;
    close_old_data_files(thd,thd->open_tables,0,dropping_tables != 0);
    mysql_ha_flush(thd);
    if (!table_is_used(thd->open_tables,1))
      break;
    (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
  }
  if (thd->killed)
    result= 1;					// aborted
  else
  {
    /* Now we can open all tables without any interference */
    thd_proc_info(thd, "Reopen tables");
    thd->version= refresh_version;
    result=reopen_tables(thd,0,0);
  }
  pthread_mutex_unlock(&LOCK_open);
  thd_proc_info(thd, 0);
  DBUG_RETURN(result);
}


/*
  drop tables from locked list

  SYNOPSIS
    drop_locked_tables()
    thd			Thread thandler
    db			Database
    table_name		Table name

  INFORMATION
    This is only called on drop tables

    The TABLE object for the dropped table is unlocked but still kept around
    as a name lock, which means that the table will be available for other
    thread as soon as we call unlock_table_names().
    If there is multiple copies of the table locked, all copies except
    the first, which acts as a name lock, is removed.

  RETURN
    #    If table existed, return table
    0	 Table was not locked
*/


TABLE *drop_locked_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table,*next,**prev, *found= 0;
  prev= &thd->open_tables;
  DBUG_ENTER("drop_locked_tables");

  /*
    Note that we need to hold LOCK_open while changing the
    open_tables list. Another thread may work on it.
    (See: remove_table_from_cache(), mysql_wait_completed_table())
    Closing a MERGE child before the parent would be fatal if the
    other thread tries to abort the MERGE lock in between.
  */
  for (table= thd->open_tables; table ; table=next)
  {
    next=table->next;
    if (!strcmp(table->s->table_name.str, table_name) &&
	!strcmp(table->s->db.str, db))
    {
      /* If MERGE child, forward lock handling to parent. */
      mysql_lock_remove(thd, thd->locked_tables,
                        table->parent ? table->parent : table, TRUE);
      /*
        When closing a MERGE parent or child table, detach the children first.
        Clear child table references in case this object is opened again.
      */
      if (table->child_l || table->parent)
        detach_merge_children(table, TRUE);

      if (!found)
      {
        found= table;
        /* Close engine table, but keep object around as a name lock */
        if (table->db_stat)
        {
          table->db_stat= 0;
          table->file->close();
        }
      }
      else
      {
        /* We already have a name lock, remove copy */
        VOID(hash_delete(&open_cache,(uchar*) table));
      }
    }
    else
    {
      *prev=table;
      prev= &table->next;
    }
  }
  *prev=0;
  if (found)
    broadcast_refresh();
  if (thd->locked_tables && thd->locked_tables->table_count == 0)
  {
    my_free((uchar*) thd->locked_tables,MYF(0));
    thd->locked_tables=0;
  }
  DBUG_RETURN(found);
}


/*
  If we have the table open, which only happens when a LOCK TABLE has been
  done on the table, change the lock type to a lock that will abort all
  other threads trying to get the lock.
*/

void abort_locked_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table;
  for (table= thd->open_tables; table ; table= table->next)
  {
    if (!strcmp(table->s->table_name.str, table_name) &&
	!strcmp(table->s->db.str, db))
    {
      /* If MERGE child, forward lock handling to parent. */
      mysql_lock_abort(thd, table->parent ? table->parent : table, TRUE);
      break;
    }
  }
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
    The LOCK_open mutex is locked

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
  safe_mutex_assert_owner(&LOCK_open);

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
  if (thd->m_reprepare_observer && thd->stmt_arena->is_reprepared == FALSE)
  {
    thd->m_reprepare_observer->report_error(thd);
    return TRUE;
  }

  return FALSE;
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

bool
check_and_update_table_version(THD *thd,
                               TABLE_LIST *tables, TABLE_SHARE *table_share)
{
  if (! tables->is_table_ref_id_equal(table_share))
  {
    if (thd->m_reprepare_observer &&
        thd->m_reprepare_observer->report_error(thd))
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

/*
  Load a table definition from file and open unireg table

  SYNOPSIS
    open_unireg_entry()
    thd			Thread handle
    entry		Store open table definition here
    table_list		TABLE_LIST with db, table_name & belong_to_view
    alias		Alias name
    cache_key		Key for share_cache
    cache_key_length	length of cache_key
    mem_root		temporary mem_root for parsing
    flags               the OPEN_VIEW_NO_PARSE flag to be passed to
                        openfrm()/open_new_frm()

  NOTES
   Extra argument for open is taken from thd->open_options
   One must have a lock on LOCK_open when calling this function

  RETURN
    0	ok
    #	Error
*/

static int open_unireg_entry(THD *thd, TABLE *entry, TABLE_LIST *table_list,
                             const char *alias,
                             char *cache_key, uint cache_key_length,
                             MEM_ROOT *mem_root, uint flags)
{
  int error;
  TABLE_SHARE *share;
  uint discover_retry_count= 0;
  DBUG_ENTER("open_unireg_entry");

  safe_mutex_assert_owner(&LOCK_open);
retry:
  if (!(share= get_table_share_with_create(thd, table_list, cache_key,
                                           cache_key_length, 
                                           OPEN_VIEW |
                                           table_list->i_s_requested_object,
                                           &error)))
    DBUG_RETURN(1);

  if (share->is_view)
  {
    /*
      If parent_l of the table_list is non null then a merge table
      has this view as child table, which is not supported.
    */
    if (table_list->parent_l)
    {
      my_error(ER_WRONG_MRG_TABLE, MYF(0));
      goto err;
    }

    /*
      This table is a view. Validate its metadata version: in particular,
      that it was a view when the statement was prepared.
    */
    if (check_and_update_table_version(thd, table_list, share))
      goto err;
    if (table_list->i_s_requested_object &  OPEN_TABLE_ONLY)
      goto err;

    /* Open view */
    error= (int) open_new_frm(thd, share, alias,
                              (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE |
                                      HA_GET_INDEX | HA_TRY_READ_ONLY),
                              READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD |
                              (flags & OPEN_VIEW_NO_PARSE),
                              thd->open_options, entry, table_list,
                              mem_root);
    if (error)
      goto err;
    /* TODO: Don't free this */
    release_table_share(share, RELEASE_NORMAL);
    DBUG_RETURN((flags & OPEN_VIEW_NO_PARSE)? -1 : 0);
  }
  else if (table_list->view)
  {
    /*
      We're trying to open a table for what was a view.
      This can only happen during (re-)execution.
      At prepared statement prepare the view has been opened and
      merged into the statement parse tree. After that, someone
      performed a DDL and replaced the view with a base table.
      Don't try to open the table inside a prepared statement,
      invalidate it instead.

      Note, the assert below is known to fail inside stored
      procedures (Bug#27011).
    */
    DBUG_ASSERT(thd->m_reprepare_observer);
    check_and_update_table_version(thd, table_list, share);
    /* Always an error. */
    DBUG_ASSERT(thd->is_error());
    goto err;
  }

  if (table_list->i_s_requested_object &  OPEN_VIEW_ONLY)
    goto err;

  while ((error= open_table_from_share(thd, share, alias,
                                       (uint) (HA_OPEN_KEYFILE |
                                               HA_OPEN_RNDFILE |
                                               HA_GET_INDEX |
                                               HA_TRY_READ_ONLY),
                                       (READ_KEYINFO | COMPUTE_TYPES |
                                        EXTRA_RECORD),
                                       thd->open_options, entry, FALSE)))
  {
    if (error == 7)                             // Table def changed
    {
      share->version= 0;                        // Mark share as old
      if (discover_retry_count++)               // Retry once
        goto err;

      /*
        TODO:
        Here we should wait until all threads has released the table.
        For now we do one retry. This may cause a deadlock if there
        is other threads waiting for other tables used by this thread.
        
        Proper fix would be to if the second retry failed:
        - Mark that table def changed
        - Return from open table
        - Close all tables used by this thread
        - Start waiting that the share is released
        - Retry by opening all tables again
      */
      if (ha_create_table_from_engine(thd, table_list->db,
                                      table_list->table_name))
        goto err;
      /*
        TO BE FIXED
        To avoid deadlock, only wait for release if no one else is
        using the share.
      */
      if (share->ref_count != 1)
        goto err;
      /* Free share and wait until it's released by all threads */
      release_table_share(share, RELEASE_WAIT_FOR_DROP);
      if (!thd->killed)
      {
        mysql_reset_errors(thd, 1);         // Clear warnings
        thd->clear_error();                 // Clear error message
        goto retry;
      }
      DBUG_RETURN(1);
    }
    if (!entry->s || !entry->s->crashed)
      goto err;
     // Code below is for repairing a crashed file
     if ((error= lock_table_name(thd, table_list, TRUE)))
     {
       if (error < 0)
 	goto err;
       if (wait_for_locked_table_names(thd, table_list))
       {
 	unlock_table_name(thd, table_list);
 	goto err;
       }
     }
     pthread_mutex_unlock(&LOCK_open);
     thd->clear_error();				// Clear error message
     error= 0;
     if (open_table_from_share(thd, share, alias,
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
       error=1;
     }
     else
       thd->clear_error();			// Clear error message
     pthread_mutex_lock(&LOCK_open);
     unlock_table_name(thd, table_list);
 
     if (error)
       goto err;
     break;
   }

  if (Table_triggers_list::check_n_load(thd, share->db.str,
                                        share->table_name.str, entry, 0))
  {
    closefrm(entry, 0);
    goto err;
  }

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
                            temp_buf.ptr(), temp_buf.length(),
                            FALSE, FALSE, errcode))
        goto err;
      if(error)
      {
        /*
          As replication is maybe going to be corrupted, we need to warn the
          DBA on top of warning the client (which will automatically be done
          because of MYF(MY_WME) in my_malloc() above).
        */
        sql_print_error("When opening HEAP table, could not allocate memory "
                        "to write 'DELETE FROM `%s`.`%s`' to the binary log",
                        table_list->db, table_list->table_name);
        delete entry->triggers;
        closefrm(entry, 0);
        goto err;
      }
    }
  }
  DBUG_RETURN(0);

err:
  release_table_share(share, RELEASE_NORMAL);
  DBUG_RETURN(1);
}


/**
  @brief Add list of MERGE children to a TABLE_LIST list.

  @param[in]    tlist           the parent TABLE_LIST object just opened

  @return status
    @retval     0               OK
    @retval     != 0            Error

  @detail
    When a MERGE parent table has just been opened, insert the
    TABLE_LIST chain from the MERGE handle into the table list used for
    opening tables for this statement. This lets the children be opened
    too.
*/

static int add_merge_table_list(TABLE_LIST *tlist)
{
  TABLE       *parent= tlist->table;
  TABLE_LIST  *child_l;
  DBUG_ENTER("add_merge_table_list");
  DBUG_PRINT("myrg", ("table: '%s'.'%s' 0x%lx", parent->s->db.str,
                      parent->s->table_name.str, (long) parent));

  /* Must not call this with attached children. */
  DBUG_ASSERT(!parent->children_attached);
  /* Must not call this with children list in place. */
  DBUG_ASSERT(tlist->next_global != parent->child_l);
  /* Prevent inclusion of another MERGE table. Could make infinite recursion. */
  if (tlist->parent_l)
  {
    my_error(ER_ADMIN_WRONG_MRG_TABLE, MYF(0), tlist->alias);
    DBUG_RETURN(1);
  }

  /* Fix children.*/
  for (child_l= parent->child_l; ; child_l= child_l->next_global)
  {
    /*
      Note: child_l->table may still be set if this parent was taken
      from the unused_tables chain. Ignore this fact here. The
      reference will be replaced by the handler in
      ::extra(HA_EXTRA_ATTACH_CHILDREN).
    */

    /* Set lock type. */
    child_l->lock_type= tlist->lock_type;

    /* Set parent reference. */
    child_l->parent_l= tlist;

    /* Break when this was the last child. */
    if (&child_l->next_global == parent->child_last_l)
      break;
  }

  /* Insert children into the table list. */
  *parent->child_last_l= tlist->next_global;
  tlist->next_global= parent->child_l;

  /*
    Do not fix the prev_global pointers. We will remove the
    chain soon anyway.
  */

  DBUG_RETURN(0);
}


/**
  @brief Attach MERGE children to the parent.

  @param[in]    tlist           the child TABLE_LIST object just opened

  @return status
    @retval     0               OK
    @retval     != 0            Error

  @note
    This is called when the last MERGE child has just been opened, let
    the handler attach the MyISAM tables to the MERGE table. Remove
    MERGE TABLE_LIST chain from the statement list so that it cannot be
    changed or freed.
*/

static int attach_merge_children(TABLE_LIST *tlist)
{
  TABLE *parent= tlist->parent_l->table;
  int error;
  DBUG_ENTER("attach_merge_children");
  DBUG_PRINT("myrg", ("table: '%s'.'%s' 0x%lx", parent->s->db.str,
                      parent->s->table_name.str, (long) parent));

  /* Must not call this with attached children. */
  DBUG_ASSERT(!parent->children_attached);
  /* Must call this with children list in place. */
  DBUG_ASSERT(tlist->parent_l->next_global == parent->child_l);

  /* Attach MyISAM tables to MERGE table. */
  error= parent->file->extra(HA_EXTRA_ATTACH_CHILDREN);

  /*
    Remove children from the table list. Even in case of an error.
    This should prevent tampering with them.
  */
  tlist->parent_l->next_global= *parent->child_last_l;

  /*
    Do not fix the last childs next_global pointer. It is needed for
    stepping to the next table in the enclosing loop in open_tables().
    Do not fix prev_global pointers. We did not set them.
  */

  if (error)
  {
    DBUG_PRINT("error", ("attaching MERGE children failed: %d", my_errno));
    parent->file->print_error(error, MYF(0));
    DBUG_RETURN(1);
  }

  parent->children_attached= TRUE;
  DBUG_PRINT("myrg", ("attached parent: '%s'.'%s' 0x%lx", parent->s->db.str,
                      parent->s->table_name.str, (long) parent));

  /*
    Note that we have the cildren in the thd->open_tables list at this
    point.
  */

  DBUG_RETURN(0);
}


/**
  @brief Detach MERGE children from the parent.

  @note
    Call this before the first table of a MERGE table (parent or child)
    is closed.

    When closing thread tables at end of statement, both parent and
    children are in thd->open_tables and will be closed. In most cases
    the children will be closed before the parent. They are opened after
    the parent and thus stacked into thd->open_tables before it.

    To avoid that we touch a closed children in any way, we must detach
    the children from the parent when the first belonging table is
    closed (parent or child).

    All references to the children should be removed on handler level
    and optionally on table level.

  @note
    Assure that you call it for a MERGE parent or child only.
    Either table->child_l or table->parent must be set.

  @param[in]    table           the TABLE object of the parent
  @param[in]    clear_refs      if to clear TABLE references
                                this must be true when called from
                                close_thread_tables() to enable fresh
                                open in open_tables()
                                it must be false when called in preparation
                                for reopen_tables()
*/

void detach_merge_children(TABLE *table, bool clear_refs)
{
  TABLE_LIST *child_l;
  TABLE *parent= table->child_l ? table : table->parent;
  bool first_detach;
  DBUG_ENTER("detach_merge_children");
  /*
    Either table->child_l or table->parent must be set. Parent must have
    child_l set.
  */
  DBUG_ASSERT(parent && parent->child_l);
  DBUG_PRINT("myrg", ("table: '%s'.'%s' 0x%lx  clear_refs: %d",
                      table->s->db.str, table->s->table_name.str,
                      (long) table, clear_refs));
  DBUG_PRINT("myrg", ("parent: '%s'.'%s' 0x%lx", parent->s->db.str,
                      parent->s->table_name.str, (long) parent));

  /*
    In a open_tables() loop it can happen that not all tables have their
    children attached yet. Also this is called for every child and the
    parent from close_thread_tables().
  */
  if ((first_detach= parent->children_attached))
  {
    VOID(parent->file->extra(HA_EXTRA_DETACH_CHILDREN));
    parent->children_attached= FALSE;
    DBUG_PRINT("myrg", ("detached parent: '%s'.'%s' 0x%lx", parent->s->db.str,
                        parent->s->table_name.str, (long) parent));
  }
  else
    DBUG_PRINT("myrg", ("parent is already detached"));

  if (clear_refs)
  {
    /* In any case clear the own parent reference. (***) */
    table->parent= NULL;

    /*
      On the first detach, clear all references. If this table is the
      parent, we still may need to clear the child references. The first
      detach might not have done this.
    */
    if (first_detach || (table == parent))
    {
      /* Clear TABLE references to force new assignment at next open. */
      for (child_l= parent->child_l; ; child_l= child_l->next_global)
      {
        /*
          Do not DBUG_ASSERT(child_l->table); open_tables might be
          incomplete.

          Clear the parent reference of the children only on the first
          detach. The children might already be closed. They will clear
          it themseves when this function is called for them with
          'clear_refs' true. See above "(***)".
        */
        if (first_detach && child_l->table)
          child_l->table->parent= NULL;

        /* Clear the table reference to force new assignment at next open. */
        child_l->table= NULL;

        /* Break when this was the last child. */
        if (&child_l->next_global == parent->child_last_l)
          break;
      }
    }
  }

  DBUG_VOID_RETURN;
}


/**
  @brief Fix MERGE children after open.

  @param[in]    old_child_list  first list member from original table
  @param[in]    old_last        pointer to &next_global of last list member
  @param[in]    new_child_list  first list member from freshly opened table
  @param[in]    new_last        pointer to &next_global of last list member

  @return       mismatch
    @retval     FALSE           OK, no mismatch
    @retval     TRUE            Error, lists mismatch

  @detail
    Main action is to copy TABLE reference for each member of original
    child list to new child list. After a fresh open these references
    are NULL. Assign the old children to the new table. Some of them
    might also be reopened or will be reopened soon.

    Other action is to verify that the table definition with respect to
    the UNION list did not change.

  @note
    This function terminates the child list if the respective '*_last'
    pointer is non-NULL. Do not call it from a place where the list is
    embedded in another list and this would break it.

    Terminating the list is required for example in the first
    reopen_table() after open_tables(). open_tables() requires the end
    of the list not to be terminated because other tables could follow
    behind the child list.

    If a '*_last' pointer is NULL, the respective list is assumed to be
    NULL terminated.
*/

bool fix_merge_after_open(TABLE_LIST *old_child_list, TABLE_LIST **old_last,
                          TABLE_LIST *new_child_list, TABLE_LIST **new_last)
{
  bool mismatch= FALSE;
  DBUG_ENTER("fix_merge_after_open");
  DBUG_PRINT("myrg", ("old last addr: 0x%lx  new last addr: 0x%lx",
                      (long) old_last, (long) new_last));

  /* Terminate the lists for easier check of list end. */
  if (old_last)
    *old_last= NULL;
  if (new_last)
    *new_last= NULL;

  for (;;)
  {
    DBUG_PRINT("myrg", ("old list item: 0x%lx  new list item: 0x%lx",
                        (long) old_child_list, (long) new_child_list));
    /* Break if one of the list is at its end. */
    if (!old_child_list || !new_child_list)
      break;
    /* Old table has references to child TABLEs. */
    DBUG_ASSERT(old_child_list->table);
    /* New table does not yet have references to child TABLEs. */
    DBUG_ASSERT(!new_child_list->table);
    DBUG_PRINT("myrg", ("old table: '%s'.'%s'  new table: '%s'.'%s'",
                        old_child_list->db, old_child_list->table_name,
                        new_child_list->db, new_child_list->table_name));
    /* Child db.table names must match. */
    if (strcmp(old_child_list->table_name, new_child_list->table_name) ||
        strcmp(old_child_list->db,         new_child_list->db))
      break;
    /*
      Copy TABLE reference. Child TABLE objects are still in place
      though not necessarily open yet.
    */
    DBUG_PRINT("myrg", ("old table ref: 0x%lx  replaces new table ref: 0x%lx",
                        (long) old_child_list->table,
                        (long) new_child_list->table));
    new_child_list->table= old_child_list->table;
    /* Step both lists. */
    old_child_list= old_child_list->next_global;
    new_child_list= new_child_list->next_global;
  }
  DBUG_PRINT("myrg", ("end of list, mismatch: %d", mismatch));
  /*
    If the list pointers are not both NULL after the loop, then the
    lists differ. If the are both identical, but not NULL, then they
    have at least one table in common and hence the rest of the list
    would be identical too. But in this case the loop woul run until the
    list end, where both pointers would become NULL.
  */
  if (old_child_list != new_child_list)
    mismatch= TRUE;
  if (mismatch)
    my_error(ER_TABLE_DEF_CHANGED, MYF(0));

  DBUG_RETURN(mismatch);
}


/*
  Return a appropriate read lock type given a table object.

  @param thd Thread context
  @param lex        LEX for the current statement.
  @param table_list Table list element for table to be locked.

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
          to tables used by those functions. This can be avoided if functions
          only read data but doing so requires more complex analysis than it
          is done now (unfortunately, due to bug #53921 "Wrong locks for
          SELECTs used stored functions may lead to broken SBR" this rule
          is not followed in cases when stored function or trigger use
          simple SELECT and not a subselect in their body).
          Furthermore, this does not apply to I_S and log tables as it's
          always unsafe to replicate such tables under statement-based
          replication as the table on the slave might contain other data
          (ie: general_log is enabled on the slave). The statement will
          be marked as unsafe for SBR in decide_logging_format().
*/

thr_lock_type read_lock_type_for_table(THD *thd, LEX *lex,
                                       TABLE_LIST *table_list)
{
  bool log_on= mysql_bin_log.is_open() && (thd->options & OPTION_BIN_LOG);
  ulong binlog_format= thd->variables.binlog_format;
  if ((log_on == FALSE) || (binlog_format == BINLOG_FORMAT_ROW) ||
      (table_list->table->s->table_category == TABLE_CATEGORY_PERFORMANCE) ||
      (lex->sql_command == SQLCOM_SELECT &&
       ! table_list->prelocking_placeholder))
    return TL_READ;
  else
    return TL_READ_NO_INSERT;
}


/*
  Open all tables in list

  SYNOPSIS
    open_tables()
    thd - thread handler
    start - list of tables in/out
    counter - number of opened tables will be return using this parameter
    flags   - bitmap of flags to modify how the tables will be open:
              MYSQL_LOCK_IGNORE_FLUSH - open table even if someone has
              done a flush on it.

  NOTE
    Unless we are already in prelocked mode, this function will also precache
    all SP/SFs explicitly or implicitly (via views and triggers) used by the
    query and add tables needed for their execution to table list. If resulting
    tables list will be non empty it will mark query as requiring precaching.
    Prelocked mode will be enabled for such query during lock_tables() call.

    If query for which we are opening tables is already marked as requiring
    prelocking it won't do such precaching and will simply reuse table list
    which is already built.

    If any table has a trigger and start->trg_event_map is non-zero
    the final lock will end up in thd->locked_tables, otherwise, the
    lock will be placed in thd->lock. See also comments in
    st_lex::set_trg_event_type_for_tables().

  RETURN
    0  - OK
    -1 - error
*/

int open_tables(THD *thd, TABLE_LIST **start, uint *counter, uint flags)
{
  TABLE_LIST *tables= NULL;
  bool refresh;
  int result=0;
  MEM_ROOT new_frm_mem;
  /* Also used for indicating that prelocking is need */
  TABLE_LIST **query_tables_last_own;
  bool safe_to_ignore_table;

  DBUG_ENTER("open_tables");
  /*
    temporary mem_root for new .frm parsing.
    TODO: variables for size
  */
  init_sql_alloc(&new_frm_mem, 8024, 8024);

  thd->current_tablenr= 0;
 restart:
  *counter= 0;
  query_tables_last_own= 0;
  thd_proc_info(thd, "Opening tables");

  /*
    If we are not already executing prelocked statement and don't have
    statement for which table list for prelocking is already built, let
    us cache routines and try to build such table list.

  */

  if (!thd->prelocked_mode && !thd->lex->requires_prelocking() &&
      thd->lex->uses_stored_routines())
  {
    bool first_no_prelocking, need_prelocking;
    TABLE_LIST **save_query_tables_last= thd->lex->query_tables_last;

    DBUG_ASSERT(thd->lex->query_tables == *start);
    sp_get_prelocking_info(thd, &need_prelocking, &first_no_prelocking);

    if (sp_cache_routines_and_add_tables(thd, thd->lex, first_no_prelocking))
    {
      /*
        Serious error during reading stored routines from mysql.proc table.
        Something's wrong with the table or its contents, and an error has
        been emitted; we must abort.
      */
      result= -1;
      goto err;
    }
    else if (need_prelocking)
    {
      query_tables_last_own= save_query_tables_last;
      *start= thd->lex->query_tables;
    }
  }

  /*
    For every table in the list of tables to open, try to find or open
    a table.
  */
  for (tables= *start; tables ;tables= tables->next_global)
  {
    DBUG_PRINT("tcache", ("opening table: '%s'.'%s'  item: 0x%lx",
                          tables->db, tables->table_name, (long) tables));

    safe_to_ignore_table= FALSE;

    /*
      Ignore placeholders for derived tables. After derived tables
      processing, link to created temporary table will be put here.
      If this is derived table for view then we still want to process
      routines used by this view.
     */
    if (tables->derived)
    {
      if (tables->view)
        goto process_view_routines;
      continue;
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
        goto process_view_routines;
      if (!mysql_schema_table(thd, thd->lex, tables) &&
          !check_and_update_table_version(thd, tables, tables->table->s))
      {
        continue;
      }
      DBUG_RETURN(-1);
    }
    (*counter)++;

    /*
      Not a placeholder: must be a base table or a view, and the table is
      not opened yet. Try to open the table.
    */
    if (!tables->table)
    {
      if (tables->prelocking_placeholder)
      {
        /*
          For the tables added by the pre-locking code, attempt to open
          the table but fail silently if the table does not exist.
          The real failure will occur when/if a statement attempts to use
          that table.
        */
        Prelock_error_handler prelock_handler;
        thd->push_internal_handler(& prelock_handler);
        tables->table= open_table(thd, tables, &new_frm_mem, &refresh, flags);
        thd->pop_internal_handler();
        safe_to_ignore_table= prelock_handler.safely_trapped_errors();
      }
      else
      {
        tables->table= open_table(thd, tables, &new_frm_mem, &refresh, flags);

        /*
          Skip further processing if there has been a fatal error while
          trying to open a table. For example, this might happen due to
          stack shortage, unknown definer in views, etc.
        */
        if (!tables->table && thd->is_error())
        {
          result= -1;
          goto err;
        }
      }
    }
    else
      DBUG_PRINT("tcache", ("referenced table: '%s'.'%s' 0x%lx",
                            tables->db, tables->table_name,
                            (long) tables->table));

    if (!tables->table)
    {
      free_root(&new_frm_mem, MYF(MY_KEEP_PREALLOC));

      if (tables->view)
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
        if (query_tables_last_own == &(tables->next_global) &&
            tables->view->query_tables)
          query_tables_last_own= tables->view->query_tables_last;
        /*
          Let us free memory used by 'sroutines' hash here since we never
          call destructor for this LEX.
        */
        hash_free(&tables->view->sroutines);
	goto process_view_routines;
      }

      /*
        If in a MERGE table open, we need to remove the children list
        from statement table list before restarting. Otherwise the list
        will be inserted another time.
      */
      if (tables->parent_l)
      {
        TABLE_LIST *parent_l= tables->parent_l;
        /* The parent table should be correctly open at this point. */
        DBUG_ASSERT(parent_l->table);
        parent_l->next_global= *parent_l->table->child_last_l;
      }

      if (refresh)				// Refresh in progress
      {
        /*
          We have met name-locked or old version of table. Now we have
          to close all tables which are not up to date. We also have to
          throw away set of prelocked tables (and thus close tables from
          this set that were open by now) since it possible that one of
          tables which determined its content was changed.

          Instead of implementing complex/non-robust logic mentioned
          above we simply close and then reopen all tables.

          In order to prepare for recalculation of set of prelocked tables
          we pretend that we have finished calculation which we were doing
          currently.
        */
        if (query_tables_last_own)
          thd->lex->mark_as_requiring_prelocking(query_tables_last_own);
        close_tables_for_reopen(thd, start);
	goto restart;
      }

      if (safe_to_ignore_table)
      {
        DBUG_PRINT("info", ("open_table: ignoring table '%s'.'%s'",
                            tables->db, tables->alias));
        continue;
      }

      result= -1;				// Fatal error
      break;
    }
    else
    {
      /*
        If we are not already in prelocked mode and extended table list is not
        yet built and we have trigger for table being opened then we should
        cache all routines used by its triggers and add their tables to
        prelocking list.
        If we lock table for reading we won't update it so there is no need to
        process its triggers since they never will be activated.
      */
      if (!thd->prelocked_mode && !thd->lex->requires_prelocking() &&
          tables->trg_event_map && tables->table->triggers &&
          tables->lock_type >= TL_WRITE_ALLOW_WRITE)
      {
        if (!query_tables_last_own)
          query_tables_last_own= thd->lex->query_tables_last;
        if (sp_cache_routines_and_add_tables_for_triggers(thd, thd->lex,
                                                          tables))
        {
          /*
            Serious error during reading stored routines from mysql.proc table.
            Something's wrong with the table or its contents, and an error has
            been emitted; we must abort.
          */
          result= -1;
          goto err;
        }
      }
      free_root(&new_frm_mem, MYF(MY_KEEP_PREALLOC));
    }

    if (tables->lock_type != TL_UNLOCK && ! thd->locked_tables)
    {
      if (tables->lock_type == TL_WRITE_DEFAULT)
        tables->table->reginfo.lock_type= thd->update_lock_default;
      else if (tables->lock_type == TL_READ_DEFAULT)
        tables->table->reginfo.lock_type=
          read_lock_type_for_table(thd, thd->lex, tables);
      else
        tables->table->reginfo.lock_type= tables->lock_type;
    }
    tables->table->grant= tables->grant;

    /* Check and update metadata version of a base table. */
    if (check_and_update_table_version(thd, tables, tables->table->s))
    {
      result= -1;
      goto err;
    }

    /* Attach MERGE children if not locked already. */
    DBUG_PRINT("tcache", ("is parent: %d  is child: %d",
                          test(tables->table->child_l),
                          test(tables->parent_l)));
    DBUG_PRINT("tcache", ("in lock tables: %d  in prelock mode: %d",
                          test(thd->locked_tables), test(thd->prelocked_mode)));
    if (((!thd->locked_tables && !thd->prelocked_mode) ||
         tables->table->s->tmp_table) &&
        ((tables->table->child_l &&
          add_merge_table_list(tables)) ||
         (tables->parent_l &&
          (&tables->next_global == tables->parent_l->table->child_last_l) &&
          attach_merge_children(tables))))
    {
      result= -1;
      goto err;
    }

process_view_routines:
    /*
      Again we may need cache all routines used by this view and add
      tables used by them to table list.
    */
    if (tables->view && !thd->prelocked_mode &&
        !thd->lex->requires_prelocking() &&
        tables->view->uses_stored_routines())
    {
      /* We have at least one table in TL here. */
      if (!query_tables_last_own)
        query_tables_last_own= thd->lex->query_tables_last;
      if (sp_cache_routines_and_add_tables_for_view(thd, thd->lex, tables))
      {
        /*
          Serious error during reading stored routines from mysql.proc table.
          Something is wrong with the table or its contents, and an error has
          been emitted; we must abort.
        */
        result= -1;
        goto err;
      }
    }
  }

 err:
  thd_proc_info(thd, 0);
  free_root(&new_frm_mem, MYF(0));              // Free pre-alloced block

  if (query_tables_last_own)
    thd->lex->mark_as_requiring_prelocking(query_tables_last_own);

  if (result && tables)
  {
    /*
      Some functions determine success as (tables->table != NULL).
      tables->table is in thd->open_tables. It won't go lost. If the
      error happens on a MERGE child, clear the parents TABLE reference.
    */
    if (tables->parent_l)
    {
      if (tables->parent_l->next_global == tables->parent_l->table->child_l)
        tables->parent_l->next_global= *tables->parent_l->table->child_last_l;
      tables->parent_l->table= NULL;
    }
    tables->table= NULL;
  }
  DBUG_PRINT("tcache", ("returning: %d", result));
  DBUG_RETURN(result);
}


/*
  Check that lock is ok for tables; Call start stmt if ok

  SYNOPSIS
    check_lock_and_start_stmt()
    thd			Thread handle
    table_list		Table to check
    lock_type		Lock used for table

  RETURN VALUES
  0	ok
  1	error
*/

static bool check_lock_and_start_stmt(THD *thd, TABLE *table,
				      thr_lock_type lock_type)
{
  int error;
  DBUG_ENTER("check_lock_and_start_stmt");

  if ((int) lock_type >= (int) TL_WRITE_ALLOW_READ &&
      (int) table->reginfo.lock_type < (int) TL_WRITE_ALLOW_READ)
  {
    my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE, MYF(0),table->alias);
    DBUG_RETURN(1);
  }
  if ((error=table->file->start_stmt(thd, lock_type)))
  {
    table->file->print_error(error,MYF(0));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/**
  @brief Open and lock one table

  @param[in]    thd             thread handle
  @param[in]    table_l         table to open is first table in this list
  @param[in]    lock_type       lock to use for table

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
    neither call decide_logging_format() nor handle some other logging
    and locking issues because it does not call lock_tables().
*/

TABLE *open_n_lock_single_table(THD *thd, TABLE_LIST *table_l,
                                thr_lock_type lock_type)
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
  if (simple_open_n_lock_tables(thd, table_l))
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
  bool refresh;
  DBUG_ENTER("open_ltable");

  /* should not be used in a prelocked_mode context, see NOTE above */
  DBUG_ASSERT(!thd->prelocked_mode);

  thd_proc_info(thd, "Opening table");
  thd->current_tablenr= 0;
  /* open_ltable can be used only for BASIC TABLEs */
  table_list->required_type= FRMTYPE_TABLE;
  while (!(table= open_table(thd, table_list, thd->mem_root, &refresh, 0)) &&
         refresh)
    ;

  if (table)
  {
    if (table->child_l)
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
    table_list->table=	   table;
    table->grant= table_list->grant;
    if (thd->locked_tables)
    {
      if (check_lock_and_start_stmt(thd, table, lock_type))
	table= 0;
    }
    else
    {
      DBUG_ASSERT(thd->lock == 0);	// You must lock everything at once
      if ((table->reginfo.lock_type= lock_type) != TL_UNLOCK)
	if (! (thd->lock= mysql_lock_tables(thd, &table_list->table, 1,
                                            lock_flags, &refresh)))
	  table= 0;
    }
  }

 end:
  thd_proc_info(thd, 0);
  DBUG_RETURN(table);
}


/*
  Open all tables in list, locks them and optionally process derived tables.

  SYNOPSIS
    open_and_lock_tables_derived()
    thd		- thread handler
    tables	- list of tables for open&locking
    derived     - if to handle derived tables

  RETURN
    FALSE - ok
    TRUE  - error

  NOTE
    The lock will automaticaly be freed by close_thread_tables()

  NOTE
    There are two convenience functions:
    - simple_open_n_lock_tables(thd, tables)  without derived handling
    - open_and_lock_tables(thd, tables)       with derived handling
    Both inline functions call open_and_lock_tables_derived() with
    the third argument set appropriately.
*/

int open_and_lock_tables_derived(THD *thd, TABLE_LIST *tables, bool derived)
{
  uint counter;
  bool need_reopen;
  DBUG_ENTER("open_and_lock_tables_derived");
  DBUG_PRINT("enter", ("derived handling: %d", derived));

  for ( ; ; ) 
  {
    if (open_tables(thd, &tables, &counter, 0))
      DBUG_RETURN(-1);

    DBUG_EXECUTE_IF("sleep_open_and_lock_after_open", {
      const char *old_proc_info= thd->proc_info;
      thd->proc_info= "DBUG sleep";
      my_sleep(6000000);
      thd->proc_info= old_proc_info;});

    if (!lock_tables(thd, tables, counter, &need_reopen))
      break;
    if (!need_reopen)
      DBUG_RETURN(-1);
    close_tables_for_reopen(thd, &tables);
  }
  if (derived &&
      (mysql_handle_derived(thd->lex, &mysql_derived_prepare) ||
       (thd->fill_derived_tables() &&
        mysql_handle_derived(thd->lex, &mysql_derived_filling))))
    DBUG_RETURN(TRUE); /* purecov: inspected */
  DBUG_RETURN(0);
}


/*
  Open all tables in list and process derived tables

  SYNOPSIS
    open_normal_and_derived_tables
    thd		- thread handler
    tables	- list of tables for open
    flags       - bitmap of flags to modify how the tables will be open:
                  MYSQL_LOCK_IGNORE_FLUSH - open table even if someone has
                  done a flush on it.

  RETURN
    FALSE - ok
    TRUE  - error

  NOTE 
    This is to be used on prepare stage when you don't read any
    data from the tables.
*/

bool open_normal_and_derived_tables(THD *thd, TABLE_LIST *tables, uint flags)
{
  uint counter;
  DBUG_ENTER("open_normal_and_derived_tables");
  DBUG_ASSERT(!thd->fill_derived_tables());
  if (open_tables(thd, &tables, &counter, flags) ||
      mysql_handle_derived(thd->lex, &mysql_derived_prepare))
    DBUG_RETURN(TRUE); /* purecov: inspected */
  DBUG_RETURN(0);
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

static void mark_real_tables_as_free_for_reuse(TABLE_LIST *table)
{
  for (; table; table= table->next_global)
    if (!table->placeholder())
      table->table->query_id= 0;
}


/**
   Decide on logging format to use for the statement.

   Compute the capabilities vector for the involved storage engines
   and mask out the flags for the binary log. Right now, the binlog
   flags only include the capabilities of the storage engines, so this
   is safe.

   We now have three alternatives that prevent the statement from
   being loggable:

   1. If there are no capabilities left (all flags are clear) it is
      not possible to log the statement at all, so we roll back the
      statement and report an error.

   2. Statement mode is set, but the capabilities indicate that
      statement format is not possible.

   3. Row mode is set, but the capabilities indicate that row
      format is not possible.

   4. Statement is unsafe, but the capabilities indicate that row
      format is not possible.

   If we are in MIXED mode, we then decide what logging format to use:

   1. If the statement is unsafe, row-based logging is used.

   2. If statement-based logging is not possible, row-based logging is
      used.

   3. Otherwise, statement-based logging is used.

   @param thd    Client thread
   @param tables Tables involved in the query
 */

int decide_logging_format(THD *thd, TABLE_LIST *tables)
{
  /*
    In SBR mode, we are only proceeding if we are binlogging this
    statement, ie, the filtering rules won't later filter this out.

    This check here is needed to prevent some spurious error to be
    raised in some cases (See BUG#42829).
   */
  if (mysql_bin_log.is_open() && (thd->options & OPTION_BIN_LOG) &&
      (thd->variables.binlog_format != BINLOG_FORMAT_STMT ||
       binlog_filter->db_ok(thd->db)))
  {
    /*
      Compute the starting vectors for the computations by creating a
      set with all the capabilities bits set and one with no
      capabilities bits set.
     */
    handler::Table_flags flags_write_some_set= 0;
    handler::Table_flags flags_access_some_set= 0;
    handler::Table_flags flags_write_all_set=
      HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE;

    /*
       If different types of engines are about to be updated.
       For example: Innodb and Falcon; Innodb and MyIsam.
    */
    my_bool multi_write_engine= FALSE;
    void* prev_write_ht= NULL;

    /*
       If different types of engines are about to be accessed
       and any of them is about to be updated. For example:
       Innodb and Falcon; Innodb and MyIsam.
    */
    my_bool multi_access_engine= FALSE;
    void* prev_access_ht= NULL;
    for (TABLE_LIST *table= tables; table; table= table->next_global)
    {
      if (table->placeholder())
        continue;
      if (table->table->s->table_category == TABLE_CATEGORY_PERFORMANCE)
        thd->lex->set_stmt_unsafe();
      ulonglong const flags= table->table->file->ha_table_flags();
      if (table->lock_type >= TL_WRITE_ALLOW_WRITE)
      {
        DBUG_PRINT("info", ("table: %s; ha_table_flags: %s%s",
                            table->table_name,
                            FLAGSTR(flags, HA_BINLOG_STMT_CAPABLE),
                            FLAGSTR(flags, HA_BINLOG_ROW_CAPABLE)));
        if (prev_write_ht && prev_write_ht != table->table->file->ht)
          multi_write_engine= TRUE;
        prev_write_ht= table->table->file->ht;
        flags_write_all_set &= flags;
        flags_write_some_set |= flags;
      }
      if (prev_access_ht && prev_access_ht != table->table->file->ht)
        multi_access_engine= TRUE;
      prev_access_ht= table->table->file->ht;
      flags_access_some_set |= flags;
    }

    DBUG_PRINT("info", ("flags_write_all_set: %s%s",
                        FLAGSTR(flags_write_all_set, HA_BINLOG_STMT_CAPABLE),
                        FLAGSTR(flags_write_all_set, HA_BINLOG_ROW_CAPABLE)));
    DBUG_PRINT("info", ("flags_write_some_set: %s%s",
                        FLAGSTR(flags_write_some_set, HA_BINLOG_STMT_CAPABLE),
                        FLAGSTR(flags_write_some_set, HA_BINLOG_ROW_CAPABLE)));
    DBUG_PRINT("info", ("flags_access_some_set: %s%s",
                        FLAGSTR(flags_access_some_set, HA_BINLOG_STMT_CAPABLE),
                        FLAGSTR(flags_access_some_set, HA_BINLOG_ROW_CAPABLE)));
    DBUG_PRINT("info", ("multi_write_engine: %s",
                        multi_write_engine ? "TRUE" : "FALSE"));
    DBUG_PRINT("info", ("multi_access_engine: %s",
                        multi_access_engine ? "TRUE" : "FALSE"));
    DBUG_PRINT("info", ("thd->variables.binlog_format: %ld",
                        thd->variables.binlog_format));

    int error= 0;
    if (flags_write_all_set == 0)
    {
      my_error((error= ER_BINLOG_LOGGING_IMPOSSIBLE), MYF(0),
               "Statement cannot be logged to the binary log in"
               " row-based nor statement-based format");
    }
    else if (thd->variables.binlog_format == BINLOG_FORMAT_STMT &&
             (flags_write_all_set & HA_BINLOG_STMT_CAPABLE) == 0)
    {
      my_error((error= ER_BINLOG_LOGGING_IMPOSSIBLE), MYF(0),
                "Statement-based format required for this statement,"
                " but not allowed by this combination of engines");
    }
    else if ((thd->variables.binlog_format == BINLOG_FORMAT_ROW ||
              thd->lex->is_stmt_unsafe()) &&
             (flags_write_all_set & HA_BINLOG_ROW_CAPABLE) == 0)
    {
      my_error((error= ER_BINLOG_LOGGING_IMPOSSIBLE), MYF(0),
                "Row-based format required for this statement,"
                " but not allowed by this combination of engines");
    }

    /*
      If more than one engine is involved in the statement and at
      least one is doing it's own logging (is *self-logging*), the
      statement cannot be logged atomically, so we generate an error
      rather than allowing the binlog to become corrupt.
     */
    if (multi_write_engine &&
        (flags_write_some_set & HA_HAS_OWN_BINLOGGING))
    {
      error= ER_BINLOG_LOGGING_IMPOSSIBLE;
      my_error(error, MYF(0),
               "Statement cannot be written atomically since more"
               " than one engine involved and at least one engine"
               " is self-logging");
    }
    /*
      Reading from a self-logging engine and updating another engine
      generates changes that are written to the binary log in the
      statement format and may make slaves to diverge. In the mixed
      mode, such changes should be written to the binary log in the
      row format.
    */
    else if (multi_access_engine &&
             (flags_access_some_set & HA_HAS_OWN_BINLOGGING))
      thd->lex->set_stmt_unsafe();

    DBUG_PRINT("info", ("error: %d", error));

    if (error)
      return -1;

    /*
      We switch to row-based format if we are in mixed mode and one of
      the following are true:

      1. If the statement is unsafe
      2. If statement format cannot be used

      Observe that point to cannot be decided before the tables
      involved in a statement has been checked, i.e., we cannot put
      this code in reset_current_stmt_binlog_row_based(), it has to be
      here.
    */
    if (thd->lex->is_stmt_unsafe() ||
        (flags_write_all_set & HA_BINLOG_STMT_CAPABLE) == 0)
    {
      thd->set_current_stmt_binlog_row_based_if_mixed();
    }
  }

  return 0;
}

/*
  Lock all tables in list

  SYNOPSIS
    lock_tables()
    thd			Thread handler
    tables		Tables to lock
    count		Number of opened tables
    need_reopen         Out parameter which if TRUE indicates that some
                        tables were dropped or altered during this call
                        and therefore invoker should reopen tables and
                        try to lock them once again (in this case
                        lock_tables() will also return error).

  NOTES
    You can't call lock_tables twice, as this would break the dead-lock-free
    handling thr_lock gives us.  You most always get all needed locks at
    once.

    If query for which we are calling this function marked as requring
    prelocking, this function will do implicit LOCK TABLES and change
    thd::prelocked_mode accordingly.

  RETURN VALUES
   0	ok
   -1	Error
*/

int lock_tables(THD *thd, TABLE_LIST *tables, uint count, bool *need_reopen)
{
  TABLE_LIST *table;

  DBUG_ENTER("lock_tables");
  /*
    We can't meet statement requiring prelocking if we already
    in prelocked mode.
  */
  DBUG_ASSERT(!thd->prelocked_mode || !thd->lex->requires_prelocking());
  *need_reopen= FALSE;

  if (!tables && !thd->lex->requires_prelocking())
    DBUG_RETURN(decide_logging_format(thd, tables));

  /*
    We need this extra check for thd->prelocked_mode because we want to avoid
    attempts to lock tables in substatements. Checking for thd->locked_tables
    is not enough in some situations. For example for SP containing
    "drop table t3; create temporary t3 ..; insert into t3 ...;"
    thd->locked_tables may be 0 after drop tables, and without this extra
    check insert will try to lock temporary table t3, that will lead
    to memory leak...
  */
  if (!thd->locked_tables && !thd->prelocked_mode)
  {
    DBUG_ASSERT(thd->lock == 0);	// You must lock everything at once
    TABLE **start,**ptr;
    uint lock_flag= MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN;

    if (!(ptr=start=(TABLE**) thd->alloc(sizeof(TABLE*)*count)))
      DBUG_RETURN(-1);
    for (table= tables; table; table= table->next_global)
    {
      if (!table->placeholder())
	*(ptr++)= table->table;
    }

    if (thd->variables.binlog_format != BINLOG_FORMAT_ROW && tables)
    {
      if (has_write_table_auto_increment_not_first_in_pk(tables))
        thd->lex->set_stmt_unsafe();
    }

    /* We have to emulate LOCK TABLES if we are statement needs prelocking. */
    if (thd->lex->requires_prelocking())
    {
      thd->in_lock_tables=1;
      thd->options|= OPTION_TABLE_LOCK;
      /*
        A query that modifies autoinc column in sub-statement can make the 
        master and slave inconsistent.
        We can solve these problems in mixed mode by switching to binlogging 
        if at least one updated table is used by sub-statement
      */
      /* The BINLOG_FORMAT_MIXED judgement is saved for suppressing 
         warnings, but it will be removed by fixing bug#45827 */
      if (thd->variables.binlog_format == BINLOG_FORMAT_MIXED && tables && 
          has_write_table_with_auto_increment(thd->lex->first_not_own_table()))
      {
        thd->lex->set_stmt_unsafe();
      }
    }

    DEBUG_SYNC(thd, "before_lock_tables_takes_lock");

    if (! (thd->lock= mysql_lock_tables(thd, start, (uint) (ptr - start),
                                        lock_flag, need_reopen)))
    {
      if (thd->lex->requires_prelocking())
      {
        thd->options&= ~(OPTION_TABLE_LOCK);
        thd->in_lock_tables=0;
      }
      DBUG_RETURN(-1);
    }

    DEBUG_SYNC(thd, "after_lock_tables_takes_lock");

    if (thd->lex->requires_prelocking() &&
        thd->lex->sql_command != SQLCOM_LOCK_TABLES)
    {
      TABLE_LIST *first_not_own= thd->lex->first_not_own_table();
      /*
        We just have done implicit LOCK TABLES, and now we have
        to emulate first open_and_lock_tables() after it.

        Note that "LOCK TABLES" can also be marked as requiring prelocking
        (e.g. if one locks view which uses functions). We should not emulate
        such open_and_lock_tables() in this case. We also should not set
        THD::prelocked_mode or first close_thread_tables() call will do
        "UNLOCK TABLES".
      */
      thd->locked_tables= thd->lock;
      thd->lock= 0;
      thd->in_lock_tables=0;

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
        if (!table->placeholder())
        {
          table->table->query_id= thd->query_id;
          if (check_lock_and_start_stmt(thd, table->table, table->lock_type))
          {
            mysql_unlock_tables(thd, thd->locked_tables);
            thd->locked_tables= 0;
            thd->options&= ~(OPTION_TABLE_LOCK);
            DBUG_RETURN(-1);
          }
        }
      }
      /*
        Let us mark all tables which don't belong to the statement itself,
        and was marked as occupied during open_tables() as free for reuse.
      */
      mark_real_tables_as_free_for_reuse(first_not_own);
      DBUG_PRINT("info",("prelocked_mode= PRELOCKED"));
      thd->prelocked_mode= PRELOCKED;
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
      if (thd->prelocked_mode &&
          table->lock_type >= TL_WRITE_ALLOW_WRITE)
      {
        for (TABLE* opentab= thd->open_tables; opentab; opentab= opentab->next)
        {
          if (table->table->s == opentab->s && opentab->query_id &&
              table->table->query_id != opentab->query_id)
          {
            my_error(ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG, MYF(0),
                     table->table->s->table_name.str);
            DBUG_RETURN(-1);
          }
        }
      }

      if (check_lock_and_start_stmt(thd, table->table, table->lock_type))
      {
	DBUG_RETURN(-1);
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
      DBUG_PRINT("info", ("thd->prelocked_mode= PRELOCKED_UNDER_LOCK_TABLES"));
      thd->prelocked_mode= PRELOCKED_UNDER_LOCK_TABLES;
    }
  }

  DBUG_RETURN(decide_logging_format(thd, tables));
}


/*
  Prepare statement for reopening of tables and recalculation of set of
  prelocked tables.

  SYNOPSIS
    close_tables_for_reopen()
      thd    in     Thread context
      tables in/out List of tables which we were trying to open and lock

*/

void close_tables_for_reopen(THD *thd, TABLE_LIST **tables)
{
  /*
    If table list consists only from tables from prelocking set, table list
    for new attempt should be empty, so we have to update list's root pointer.
  */
  if (thd->lex->first_not_own_table() == *tables)
    *tables= 0;
  thd->lex->chop_off_not_own_tables();
  sp_remove_not_own_routines(thd->lex);
  for (TABLE_LIST *tmp= *tables; tmp; tmp= tmp->next_global)
    tmp->table= 0;
  close_thread_tables(thd);
}


/*
  Open a single table without table caching and don't set it in open_list

  SYNPOSIS
    open_temporary_table()
    thd		  Thread object
    path	  Path (without .frm)
    db		  database
    table_name	  Table name
    link_in_list  1 if table should be linked into thd->temporary_tables

 NOTES:
    Used by alter_table to open a temporary table and when creating
    a temporary table with CREATE TEMPORARY ...

 RETURN
   0  Error
   #  TABLE object
*/

TABLE *open_temporary_table(THD *thd, const char *path, const char *db,
			    const char *table_name, bool link_in_list)
{
  TABLE *tmp_table;
  TABLE_SHARE *share;
  char cache_key[MAX_DBKEY_LENGTH], *saved_cache_key, *tmp_path;
  uint key_length;
  TABLE_LIST table_list;
  DBUG_ENTER("open_temporary_table");
  DBUG_PRINT("enter",
             ("table: '%s'.'%s'  path: '%s'  server_id: %u  "
              "pseudo_thread_id: %lu",
              db, table_name, path,
              (uint) thd->server_id, (ulong) thd->variables.pseudo_thread_id));

  table_list.db=         (char*) db;
  table_list.table_name= (char*) table_name;
  /* Create the cache_key for temporary tables */
  key_length= create_table_def_key(thd, cache_key, &table_list, 1);

  if (!(tmp_table= (TABLE*) my_malloc(sizeof(*tmp_table) + sizeof(*share) +
                                      strlen(path)+1 + key_length,
                                      MYF(MY_WME))))
    DBUG_RETURN(0);				/* purecov: inspected */

  share= (TABLE_SHARE*) (tmp_table+1);
  tmp_path= (char*) (share+1);
  saved_cache_key= strmov(tmp_path, path)+1;
  memcpy(saved_cache_key, cache_key, key_length);

  init_tmp_table_share(thd, share, saved_cache_key, key_length,
                       strend(saved_cache_key)+1, tmp_path);

  if (open_table_def(thd, share, 0) ||
      open_table_from_share(thd, share, table_name,
                            (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE |
                                    HA_GET_INDEX),
                            READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
                            ha_open_options,
                            tmp_table, FALSE))
  {
    /* No need to lock share->mutex as this is not needed for tmp tables */
    free_table_share(share);
    my_free((char*) tmp_table,MYF(0));
    DBUG_RETURN(0);
  }

  tmp_table->reginfo.lock_type= TL_WRITE;	 // Simulate locked
  share->tmp_table= (tmp_table->file->has_transactions() ? 
                     TRANSACTIONAL_TMP_TABLE : NON_TRANSACTIONAL_TMP_TABLE);

  if (link_in_list)
  {
    /* growing temp list at the head */
    tmp_table->next= thd->temporary_tables;
    if (tmp_table->next)
      tmp_table->next->prev= tmp_table;
    thd->temporary_tables= tmp_table;
    thd->temporary_tables->prev= 0;
    if (thd->slave_thread)
      slave_open_temp_tables++;
  }
  tmp_table->pos_in_table_list= 0;
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
  if (my_delete(frm_path, MYF(0)))
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
      if (*ref && !(*ref)->is_autogenerated_name)
      {
        item->set_name((*ref)->name, (*ref)->name_length,
                       system_charset_info);
        item->real_item()->set_name((*ref)->name, (*ref)->name_length,
                       system_charset_info);
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
    if (*ref && !(*ref)->is_autogenerated_name)
    {
      item->set_name((*ref)->name, (*ref)->name_length,
                     system_charset_info);
      item->real_item()->set_name((*ref)->name, (*ref)->name_length,
                                  system_charset_info);
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
                          nj_col->table_field->name));
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
    field_ptr= (Field**) hash_search(&table->s->name_hash, (uchar*) name,
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
        strcmp(db_name, table_list->db))))
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
    field_ptr= (Field**)hash_search(&table->s->name_hash,(uchar*) name,
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
      found= find_field_in_table_ref(thd, table_ref, name, length, item->name,
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
                                              item->name, db, table_name, ref,
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
                                           item->name, db, table_name, ref,
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
      if (!item_field->name)
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
        if (!my_strcasecmp(system_charset_info,
                           item_field->name,field_name))
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
      if (is_ref_by_name && find->name && item->name &&
	  !my_strcasecmp(system_charset_info,item->name,find->name))
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
      if (item_ref->name && item_ref->table_name &&
          !my_strcasecmp(system_charset_info, item_ref->name, field_name) &&
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
        !table_ref_1->on_expr && !table_ref_2->on_expr)
      table_ref_2->on_expr= new Item_int((longlong) 1,1);   /* Always true. */

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
  thd->where= "from clause";
  if (from_clause->elements == 0)
    return FALSE; /* We come here in the case of UNIONs. */

  List_iterator_fast<TABLE_LIST> table_ref_it(*from_clause);
  TABLE_LIST *table_ref; /* Current table reference. */
  /* Table reference to the left of the current. */
  TABLE_LIST *left_neighbor;
  /* Table reference to the right of the current. */
  TABLE_LIST *right_neighbor= NULL;
  bool save_first_natural_join_processing=
    context->select_lex->first_natural_join_processing;

  context->select_lex->first_natural_join_processing= FALSE;

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
    if (save_first_natural_join_processing)
    {
      context->select_lex->first_natural_join_processing= FALSE;
      if (store_top_level_join_columns(thd, table_ref,
                                       left_neighbor, right_neighbor))
        return TRUE;
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

  return FALSE;
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

  thd->lex->current_select->cur_pos_in_select_list= 0;
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
        it.replace(new Item_int("Not_used", (longlong) 1,
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
      thd->lex->current_select->cur_pos_in_select_list++;
  }
  thd->lex->current_select->cur_pos_in_select_list= UNDEF_POS;
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

bool setup_fields(THD *thd, Item **ref_pointer_array,
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
  if (ref_pointer_array)
    bzero(ref_pointer_array, sizeof(Item *) * fields.elements);

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

  Item **ref= ref_pointer_array;
  thd->lex->current_select->cur_pos_in_select_list= 0;
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
    if (ref)
      *(ref++)= item;
    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM &&
	sum_func_list)
      item->split_sum_func(thd, ref_pointer_array, *sum_func_list);
    thd->lex->used_tables|= item->used_tables();
    thd->lex->current_select->cur_pos_in_select_list++;
  }
  thd->lex->current_select->is_item_list_lookup= save_is_item_list_lookup;
  thd->lex->current_select->cur_pos_in_select_list= UNDEF_POS;

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
    my_error(ER_TOO_MANY_TABLES, MYF(0), static_cast<int>(MAX_TABLES));
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


/*
  prepare tables and check access for the view tables

  SYNOPSIS
    setup_tables_and_check_view_access()
    thd		  Thread handler
    context       name resolution contest to setup table list there
    from_clause   Top-level list of table references in the FROM clause
    tables	  Table list (select_lex->table_list)
    conds	  Condition of current SELECT (can be changed by VIEW)
    leaves        List of join table leaves list (select_lex->leaf_tables)
    refresh       It is onle refresh for subquery
    select_insert It is SELECT ... INSERT command
    want_access   what access is needed

  NOTE
    a wrapper for check_tables that will also check the resulting
    table leaves list for access to all the tables that belong to a view

  RETURN
    FALSE ok;  In this case *map will include the chosen index
    TRUE  error
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
   Create a key_map from a list of index names

   SYNOPSIS
     get_key_map_from_key_list()
     map		key_map to fill in
     table		Table
     index_list		List of index names

   RETURN
     0	ok;  In this case *map will includes the choosed index
     1	error
*/

bool get_key_map_from_key_list(key_map *map, TABLE *table,
                               List<String> *index_list)
{
  List_iterator_fast<String> it(*index_list);
  String *name;
  uint pos;

  map->clear_all();
  while ((name=it++))
  {
    if (table->s->keynames.type_names == 0 ||
        (pos= find_type(&table->s->keynames, name->ptr(),
                        name->length(), 1)) <=
        0)
    {
      my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), name->c_ptr(),
	       table->pos_in_table_list->alias);
      map->set_all();
      return 1;
    }
    map->set_bit(pos-1);
  }
  return 0;
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
      thd->lex->used_tables|= table->map;

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
            field_table->covering_keys.intersect(field->part_of_key);
            field_table->merge_keys.merge(field->part_of_key);
            field_table->used_fields++;
          }
        }
      }
      else
        thd->lex->used_tables|= item->used_tables();
      thd->lex->current_select->cur_pos_in_select_list++;
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
  if (!table_name)
    my_message(ER_NO_TABLES_USED, ER(ER_NO_TABLES_USED), MYF(0));
  else
    my_error(ER_BAD_TABLE_ERROR, MYF(0), table_name);

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
                COND **conds)
{
  SELECT_LEX *select_lex= thd->lex->current_select;
  Query_arena *arena= thd->stmt_arena, backup;
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

  if (select_lex->conds_processed_with_permanent_arena ||
      arena->is_conventional())
    arena= 0;                                   // For easier test

  thd->mark_used_columns= MARK_COLUMNS_READ;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
  select_lex->cond_count= 0;
  select_lex->between_count= 0;
  select_lex->max_equal_elems= 0;

  for (table= tables; table; table= table->next_local)
  {
    if (table->prepare_where(thd, conds, FALSE))
      goto err_no_arena;
  }

  if (*conds)
  {
    thd->where="where clause";
    if ((!(*conds)->fixed && (*conds)->fix_fields(thd, conds)) ||
	(*conds)->check_cols(1))
      goto err_no_arena;
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
      if (embedded->on_expr)
      {
        /* Make a join an a expression */
        thd->where="on clause";
        if ((!embedded->on_expr->fixed &&
            embedded->on_expr->fix_fields(thd, &embedded->on_expr)) ||
	    embedded->on_expr->check_cols(1))
	  goto err_no_arena;
        select_lex->cond_count++;
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

  if (!thd->stmt_arena->is_conventional())
  {
    /*
      We are in prepared statement preparation code => we should store
      WHERE clause changing for next executions.

      We do this ON -> WHERE transformation only once per PS/SP statement.
    */
    select_lex->where= *conds;
    select_lex->conds_processed_with_permanent_arena= 1;
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

  SYNOPSIS
    fill_record()
    thd           thread handler
    fields        Item_fields list to be filled
    values        values to fill with
    ignore_errors TRUE if we should ignore errors

  NOTE
    fill_record() may set table->auto_increment_field_not_null and a
    caller should make sure that it is reset after their last call to this
    function.

  RETURN
    FALSE   OK
    TRUE    error occured
*/

static bool
fill_record(THD * thd, List<Item> &fields, List<Item> &values,
            bool ignore_errors)
{
  List_iterator_fast<Item> f(fields),v(values);
  Item *value, *fld;
  Item_field *field;
  TABLE *table= 0;
  DBUG_ENTER("fill_record");

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
    if (!(field= fld->filed_for_view_update()))
    {
      my_error(ER_NONUPDATEABLE_COLUMN, MYF(0), fld->name);
      goto err;
    }
    table= field->field->table;
    table->auto_increment_field_not_null= FALSE;
    f.rewind();
  }
  while ((fld= f++))
  {
    if (!(field= fld->filed_for_view_update()))
    {
      my_error(ER_NONUPDATEABLE_COLUMN, MYF(0), fld->name);
      goto err;
    }
    value=v++;
    Field *rfield= field->field;
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
  return (fill_record(thd, fields, values, ignore_errors) ||
          (triggers && triggers->process_triggers(thd, event,
                                                 TRG_ACTION_BEFORE, TRUE)));
}


/*
  Fill field buffer with values from Field list

  SYNOPSIS
    fill_record()
    thd           thread handler
    ptr           pointer on pointer to record
    values        list of fields
    ignore_errors TRUE if we should ignore errors

  NOTE
    fill_record() may set table->auto_increment_field_not_null and a
    caller should make sure that it is reset after their last call to this
    function.

  RETURN
    FALSE   OK
    TRUE    error occured
*/

bool
fill_record(THD *thd, Field **ptr, List<Item> &values, bool ignore_errors)
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
    if (field == table->next_number_field)
      table->auto_increment_field_not_null= TRUE;
    if (value->save_in_field(field, 0) < 0)
      goto err;
  }
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
  return (fill_record(thd, ptr, values, ignore_errors) ||
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
        VOID(my_delete(filePath, MYF(0))); 
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
  Invalidate any cache entries that are for some DB

  SYNOPSIS
    remove_db_from_cache()
    db		Database name. This will be in lower case if
		lower_case_table_name is set

  NOTE:
  We can't use hash_delete when looping hash_elements. We mark them first
  and afterwards delete those marked unused.
*/

void remove_db_from_cache(const char *db)
{
  for (uint idx=0 ; idx < open_cache.records ; idx++)
  {
    TABLE *table=(TABLE*) hash_element(&open_cache,idx);
    if (!strcmp(table->s->db.str, db))
    {
      table->s->version= 0L;			/* Free when thread is ready */
      if (!table->in_use)
	relink_unused(table);
    }
  }
  while (unused_tables && !unused_tables->s->version)
    VOID(hash_delete(&open_cache,(uchar*) unused_tables));
}


/*
  free all unused tables

  NOTE
    This is called by 'handle_manager' when one wants to periodicly flush
    all not used tables.
*/

void flush_tables()
{
  (void) pthread_mutex_lock(&LOCK_open);
  while (unused_tables)
    hash_delete(&open_cache,(uchar*) unused_tables);
  (void) pthread_mutex_unlock(&LOCK_open);
}


/*
  Mark all entries with the table as deleted to force an reopen of the table

  The table will be closed (not stored in cache) by the current thread when
  close_thread_tables() is called.

  PREREQUISITES
    Lock on LOCK_open()

  RETURN
    0  This thread now have exclusive access to this table and no other thread
       can access the table until close_thread_tables() is called.
    1  Table is in use by another thread
*/

bool remove_table_from_cache(THD *thd, const char *db, const char *table_name,
                             uint flags)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  TABLE *table;
  TABLE_SHARE *share;
  bool result= 0, signalled= 0;
  DBUG_ENTER("remove_table_from_cache");
  DBUG_PRINT("enter", ("table: '%s'.'%s'  flags: %u", db, table_name, flags));

  key_length= create_table_def_key(key, db, table_name);
  for (;;)
  {
    HASH_SEARCH_STATE state;
    result= signalled= 0;

    for (table= (TABLE*) hash_first(&open_cache, (uchar*) key, key_length,
                                    &state);
         table;
         table= (TABLE*) hash_next(&open_cache, (uchar*) key, key_length,
                                   &state))
    {
      THD *in_use;
      DBUG_PRINT("tcache", ("found table: '%s'.'%s' 0x%lx", table->s->db.str,
                            table->s->table_name.str, (long) table));

      table->s->version=0L;		/* Free when thread is ready */
      if (!(in_use=table->in_use))
      {
        DBUG_PRINT("info",("Table was not in use"));
        relink_unused(table);
      }
      else if (in_use != thd)
      {
        DBUG_PRINT("info", ("Table was in use by other thread"));
        /*
          Mark that table is going to be deleted from cache. This will
          force threads that are in mysql_lock_tables() (but not yet
          in thr_multi_lock()) to abort it's locks, close all tables and retry
        */
        in_use->some_tables_deleted= 1;
        if (table->is_name_opened())
        {
          DBUG_PRINT("info", ("Found another active instance of the table"));
  	  result=1;
        }
        /* Kill delayed insert threads */
        if ((in_use->system_thread & SYSTEM_THREAD_DELAYED_INSERT) &&
            ! in_use->killed)
        {
	  in_use->killed= THD::KILL_CONNECTION;
	  pthread_mutex_lock(&in_use->mysys_var->mutex);
	  if (in_use->mysys_var->current_cond)
	  {
	    pthread_mutex_lock(in_use->mysys_var->current_mutex);
            signalled= 1;
	    pthread_cond_broadcast(in_use->mysys_var->current_cond);
	    pthread_mutex_unlock(in_use->mysys_var->current_mutex);
	  }
	  pthread_mutex_unlock(&in_use->mysys_var->mutex);
        }
        /*
	  Now we must abort all tables locks used by this thread
	  as the thread may be waiting to get a lock for another table.
          Note that we need to hold LOCK_open while going through the
          list. So that the other thread cannot change it. The other
          thread must also hold LOCK_open whenever changing the
          open_tables list. Aborting the MERGE lock after a child was
          closed and before the parent is closed would be fatal.
        */
        for (TABLE *thd_table= in_use->open_tables;
	     thd_table ;
	     thd_table= thd_table->next)
        {
          /* Do not handle locks of MERGE children. */
	  if (thd_table->db_stat && !thd_table->parent)	// If table is open
	    signalled|= mysql_lock_abort_for_thread(thd, thd_table);
        }
      }
      else
      {
        DBUG_PRINT("info", ("Table was in use by current thread. db_stat: %u",
                            table->db_stat));
        result= result || (flags & RTFC_OWNED_BY_THD_FLAG);
      }
    }
    while (unused_tables && !unused_tables->s->version)
      VOID(hash_delete(&open_cache,(uchar*) unused_tables));

    DBUG_PRINT("info", ("Removing table from table_def_cache"));
    /* Remove table from table definition cache if it's not in use */
    if ((share= (TABLE_SHARE*) hash_search(&table_def_cache,(uchar*) key,
                                           key_length)))
    {
      DBUG_PRINT("info", ("share version: %lu  ref_count: %u",
                          share->version, share->ref_count));
      share->version= 0;                          // Mark for delete
      if (share->ref_count == 0)
      {
        pthread_mutex_lock(&share->mutex);
        VOID(hash_delete(&table_def_cache, (uchar*) share));
      }
    }

    if (result && (flags & RTFC_WAIT_OTHER_THREAD_FLAG))
    {
      /*
        Signal any thread waiting for tables to be freed to
        reopen their tables
      */
      broadcast_refresh();
      DBUG_PRINT("info", ("Waiting for refresh signal"));
      if (!(flags & RTFC_CHECK_KILLED_FLAG) || !thd->killed)
      {
        dropping_tables++;
        if (likely(signalled))
          (void) pthread_cond_wait(&COND_refresh, &LOCK_open);
        else
        {
          struct timespec abstime;
          /*
            It can happen that another thread has opened the
            table but has not yet locked any table at all. Since
            it can be locked waiting for a table that our thread
            has done LOCK TABLE x WRITE on previously, we need to
            ensure that the thread actually hears our signal
            before we go to sleep. Thus we wait for a short time
            and then we retry another loop in the
            remove_table_from_cache routine.
          */
          set_timespec(abstime, 10);
          pthread_cond_timedwait(&COND_refresh, &LOCK_open, &abstime);
        }
        dropping_tables--;
        continue;
      }
    }
    break;
  }
  DBUG_RETURN(result);
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
    thd_proc_info(thd, "FULLTEXT initialization");

    while ((ifm=li++))
      ifm->init_search(no_order);
  }
  return 0;
}


/*
  open new .frm format table

  SYNOPSIS
    open_new_frm()
    THD		  thread handler
    path	  path to .frm file (without extension)
    alias	  alias for table
    db            database
    table_name    name of table
    db_stat	  open flags (for example ->OPEN_KEYFILE|HA_OPEN_RNDFILE..)
		  can be 0 (example in ha_example_table)
    prgflag	  READ_ALL etc..
    ha_open_flags HA_OPEN_ABORT_IF_LOCKED etc..
    outparam	  result table
    table_desc	  TABLE_LIST descriptor
    mem_root	  temporary MEM_ROOT for parsing
*/

static bool
open_new_frm(THD *thd, TABLE_SHARE *share, const char *alias,
             uint db_stat, uint prgflag,
	     uint ha_open_flags, TABLE *outparam, TABLE_LIST *table_desc,
	     MEM_ROOT *mem_root)
{
  LEX_STRING pathstr;
  File_parser *parser;
  char path[FN_REFLEN+1];
  DBUG_ENTER("open_new_frm");

  /* Create path with extension */
  pathstr.length= (uint) (strxnmov(path, sizeof(path) - 1,
                                   share->normalized_path.str,
                                   reg_ext,
                                   NullS) - path);
  pathstr.str=    path;

  if ((parser= sql_parse_prepare(&pathstr, mem_root, 1)))
  {
    if (is_equal(&view_type, parser->type()))
    {
      if (table_desc == 0 || table_desc->required_type == FRMTYPE_TABLE)
      {
        my_error(ER_WRONG_OBJECT, MYF(0), share->db.str, share->table_name.str,
                 "BASE TABLE");
        goto err;
      }
      if (mysql_make_view(thd, parser, table_desc,
                          (prgflag & OPEN_VIEW_NO_PARSE)))
        goto err;
    }
    else
    {
      /* only VIEWs are supported now */
      my_error(ER_FRM_UNKNOWN_TYPE, MYF(0), share->path.str,  parser->type()->str);
      goto err;
    }
    DBUG_RETURN(0);
  }
 
err:
  bzero(outparam, sizeof(TABLE));	// do not run repair
  DBUG_RETURN(1);
}


bool is_equal(const LEX_STRING *a, const LEX_STRING *b)
{
  return a->length == b->length && !strncmp(a->str, b->str, a->length);
}


/*
  SYNOPSIS
    abort_and_upgrade_lock_and_close_table()
    lpt                           Parameter passing struct
    All parameters passed through the ALTER_PARTITION_PARAM_TYPE object
  RETURN VALUE
    0
  DESCRIPTION
    Remember old lock level (for possible downgrade later on), abort all
    waiting threads and ensure that all keeping locks currently are
    completed such that we own the lock exclusively and no other interaction
    is ongoing. Close the table and hold the name lock.

    thd                           Thread object
    table                         Table object
    db                            Database name
    table_name                    Table name
    old_lock_level                Old lock level
*/

int abort_and_upgrade_lock_and_close_table(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  uint flags= RTFC_WAIT_OTHER_THREAD_FLAG | RTFC_CHECK_KILLED_FLAG;
  const char *db=         lpt->db;
  const char *table_name= lpt->table_name;
  THD *thd=               lpt->thd;
  DBUG_ENTER("abort_and_upgrade_lock_and_close_table");

  lpt->old_lock_type= lpt->table->reginfo.lock_type;
  safe_mutex_assert_not_owner(&LOCK_open);
  VOID(pthread_mutex_lock(&LOCK_open));
  /* If MERGE child, forward lock handling to parent. */
  mysql_lock_abort(thd, lpt->table->parent ? lpt->table->parent : lpt->table,
                   TRUE);
  if (remove_table_from_cache(thd, db, table_name, flags))
  {
    VOID(pthread_mutex_unlock(&LOCK_open));
    DBUG_RETURN(1);
  }
  close_data_files_and_morph_locks(thd, db, table_name);
  VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_RETURN(0);
}


/*
  SYNOPSIS
    close_open_tables_and_downgrade()
  RESULT VALUES
    NONE
  DESCRIPTION
    We need to ensure that any thread that has managed to open the table
    but not yet encountered our lock on the table is also thrown out to
    ensure that no threads see our frm changes premature to the final
    version. The intermediate versions are only meant for use after a
    crash and later REPAIR TABLE.
    We also downgrade locks after the upgrade to WRITE_ONLY
*/

/* purecov: begin deadcode */
void close_open_tables_and_downgrade(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  VOID(pthread_mutex_lock(&LOCK_open));
  remove_table_from_cache(lpt->thd, lpt->db, lpt->table_name,
                          RTFC_WAIT_OTHER_THREAD_FLAG);
  VOID(pthread_mutex_unlock(&LOCK_open));
  /* If MERGE child, forward lock handling to parent. */
  mysql_lock_downgrade_write(lpt->thd, lpt->table->parent ? lpt->table->parent :
                             lpt->table, lpt->old_lock_type);
}
/* purecov: end */


/*
  SYNOPSIS
    mysql_wait_completed_table()
    lpt                            Parameter passing struct
    my_table                       My table object
    All parameters passed through the ALTER_PARTITION_PARAM object
  RETURN VALUES
    TRUE                          Failure
    FALSE                         Success
  DESCRIPTION
    We have changed the frm file and now we want to wait for all users of
    the old frm to complete before proceeding to ensure that no one
    remains that uses the old frm definition.
    Start by ensuring that all users of the table will be removed from cache
    once they are done. Then abort all that have stumbled on locks and
    haven't been started yet.

    thd                           Thread object
    table                         Table object
    db                            Database name
    table_name                    Table name
*/

void mysql_wait_completed_table(ALTER_PARTITION_PARAM_TYPE *lpt, TABLE *my_table)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  TABLE *table;
  DBUG_ENTER("mysql_wait_completed_table");

  key_length= create_table_def_key(key, lpt->db, lpt->table_name);
  VOID(pthread_mutex_lock(&LOCK_open));
  HASH_SEARCH_STATE state;
  for (table= (TABLE*) hash_first(&open_cache,(uchar*) key,key_length,
                                  &state) ;
       table;
       table= (TABLE*) hash_next(&open_cache,(uchar*) key,key_length,
                                 &state))
  {
    THD *in_use= table->in_use;
    table->s->version= 0L;
    if (!in_use)
    {
      relink_unused(table);
    }
    else
    {
      /* Kill delayed insert threads */
      if ((in_use->system_thread & SYSTEM_THREAD_DELAYED_INSERT) &&
          ! in_use->killed)
      {
        in_use->killed= THD::KILL_CONNECTION;
        pthread_mutex_lock(&in_use->mysys_var->mutex);
        if (in_use->mysys_var->current_cond)
        {
          pthread_mutex_lock(in_use->mysys_var->current_mutex);
          pthread_cond_broadcast(in_use->mysys_var->current_cond);
          pthread_mutex_unlock(in_use->mysys_var->current_mutex);
        }
        pthread_mutex_unlock(&in_use->mysys_var->mutex);
      }
      /*
        Now we must abort all tables locks used by this thread
        as the thread may be waiting to get a lock for another table.
        Note that we need to hold LOCK_open while going through the
        list. So that the other thread cannot change it. The other
        thread must also hold LOCK_open whenever changing the
        open_tables list. Aborting the MERGE lock after a child was
        closed and before the parent is closed would be fatal.
      */
      for (TABLE *thd_table= in_use->open_tables;
           thd_table ;
           thd_table= thd_table->next)
      {
        /* Do not handle locks of MERGE children. */
        if (thd_table->db_stat && !thd_table->parent) // If table is open
          mysql_lock_abort_for_thread(lpt->thd, thd_table);
      }
    }
  }
  /*
    We start by removing all unused objects from the cache and marking
    those in use for removal after completion. Now we also need to abort
    all that are locked and are not progressing due to being locked
    by our lock. We don't upgrade our lock here.
    If MERGE child, forward lock handling to parent.
  */
  mysql_lock_abort(lpt->thd, my_table->parent ? my_table->parent : my_table,
                   FALSE);
  VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_VOID_RETURN;
}


/*
  Check if one (or more) write tables have auto_increment columns.

  @param[in] tables Table list

  @retval 0 if at least one write tables has an auto_increment column
  @retval 1 otherwise

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
                            Open_tables_state *backup)
{
  DBUG_ENTER("open_system_tables_for_read");

  thd->reset_n_backup_open_tables_state(backup);

  uint count= 0;
  bool not_used;
  for (TABLE_LIST *tables= table_list; tables; tables= tables->next_global)
  {
    TABLE *table= open_table(thd, tables, thd->mem_root, &not_used,
                             MYSQL_LOCK_IGNORE_FLUSH);
    if (!table)
      goto error;

    DBUG_ASSERT(table->s->table_category == TABLE_CATEGORY_SYSTEM);

    table->use_all_columns();
    table->reginfo.lock_type= tables->lock_type;
    tables->table= table;
    count++;
  }

  {
    TABLE **list= (TABLE**) thd->alloc(sizeof(TABLE*) * count);
    TABLE **ptr= list;
    for (TABLE_LIST *tables= table_list; tables; tables= tables->next_global)
      *(ptr++)= tables->table;

    thd->lock= mysql_lock_tables(thd, list, count,
                                 MYSQL_LOCK_IGNORE_FLUSH, &not_used);
  }
  if (thd->lock)
    DBUG_RETURN(FALSE);

error:
  close_system_tables(thd, backup);

  DBUG_RETURN(TRUE);
}


/*
  Close system tables, opened with open_system_tables_for_read().

  SYNOPSIS
    close_system_tables()
      thd     Thread context
      backup  Pointer to Open_tables_state instance which holds
              information about tables which were open before we
              decided to access system tables.
*/

void
close_system_tables(THD *thd, Open_tables_state *backup)
{
  close_thread_tables(thd);
  thd->restore_backup_open_tables_state(backup);
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

  TABLE *table= open_ltable(thd, one_table, one_table->lock_type, 0);
  if (table)
  {
    DBUG_ASSERT(table->s->table_category == TABLE_CATEGORY_SYSTEM);
    table->use_all_columns();
  }

  DBUG_RETURN(table);
}

/**
  Open a performance schema table.
  Opening such tables is performed internally in the server
  implementation, and is a 'nested' open, since some tables
  might be already opened by the current thread.
  The thread context before this call is saved, and is restored
  when calling close_performance_schema_table().
  @param thd The current thread
  @param one_table Performance schema table to open
  @param backup [out] Temporary storage used to save the thread context
*/
TABLE *
open_performance_schema_table(THD *thd, TABLE_LIST *one_table,
                              Open_tables_state *backup)
{
  uint flags= ( MYSQL_LOCK_IGNORE_GLOBAL_READ_LOCK |
                MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY |
                MYSQL_LOCK_IGNORE_FLUSH |
                MYSQL_LOCK_PERF_SCHEMA);
  TABLE *table;
  /* Save value that is changed in mysql_lock_tables() */
  ulonglong save_utime_after_lock= thd->utime_after_lock;
  DBUG_ENTER("open_performance_schema_table");

  thd->reset_n_backup_open_tables_state(backup);

  if ((table= open_ltable(thd, one_table, one_table->lock_type, flags)))
  {
    DBUG_ASSERT(table->s->table_category == TABLE_CATEGORY_PERFORMANCE);
    /* Make sure all columns get assigned to a default value */
    table->use_all_columns();
    table->no_replicate= 1;
    /*
      Don't set automatic timestamps as we may want to use time of logging,
      not from query start
    */
    table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
  }
  else
  {
    /*
      If error in mysql_lock_tables(), open_ltable doesn't close the
      table. Thread kill during mysql_lock_tables() is such error. But
      open tables cannot be accepted when restoring the open tables
      state.
    */
    if (thd->killed)
      close_thread_tables(thd);
    thd->restore_backup_open_tables_state(backup);
  }

  thd->utime_after_lock= save_utime_after_lock;
  DBUG_RETURN(table);
}

/**
  Close a performance schema table.
  The last table opened by open_performance_schema_table()
  is closed, then the thread context is restored.
  @param thd The current thread
  @param backup [in] the context to restore.
*/
void close_performance_schema_table(THD *thd, Open_tables_state *backup)
{
  bool found_old_table;

  /*
    If open_performance_schema_table() fails,
    this function should not be called.
  */
  DBUG_ASSERT(thd->lock != NULL);

  /*
    Note:
    We do not create explicitly a separate transaction for the
    performance table I/O, but borrow the current transaction.
    lock + unlock will autocommit the change done in the
    performance schema table: this is the expected result.
    The current transaction should not be affected by this code.
    TODO: Note that if a transactional engine is used for log tables,
    this code will need to be revised, as a separate transaction
    might be needed.
  */
  mysql_unlock_tables(thd, thd->lock);
  thd->lock= 0;

  pthread_mutex_lock(&LOCK_open);

  found_old_table= false;
  /*
    Note that we need to hold LOCK_open while changing the
    open_tables list. Another thread may work on it.
    (See: remove_table_from_cache(), mysql_wait_completed_table())
    Closing a MERGE child before the parent would be fatal if the
    other thread tries to abort the MERGE lock in between.
  */
  while (thd->open_tables)
    found_old_table|= close_thread_table(thd, &thd->open_tables);

  if (found_old_table)
    broadcast_refresh();

  pthread_mutex_unlock(&LOCK_open);

  thd->restore_backup_open_tables_state(backup);
}

/**
  @} (end of group Data_Dictionary)
*/
