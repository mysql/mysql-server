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


/* Basic functions needed by many modules */

#include "mysql_priv.h"
#include "sql_select.h"
#include "sp_head.h"
#include "sp.h"
#include "sql_trigger.h"
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#ifdef  __WIN__
#include <io.h>
#endif

TABLE *unused_tables;				/* Used by mysql_test */
HASH open_cache;				/* Used by mysql_test */
static HASH table_def_cache;
static TABLE_SHARE *oldest_unused_share, end_of_unused_share;
static pthread_mutex_t LOCK_table_share;
static bool table_def_inited= 0;

static int open_unireg_entry(THD *thd, TABLE *entry, TABLE_LIST *table_list,
			     const char *alias,
                             char *cache_key, uint cache_key_length,
			     MEM_ROOT *mem_root);
static void free_cache_entry(TABLE *entry);
static void mysql_rm_tmp_tables(void);
static bool open_new_frm(THD *thd, TABLE_SHARE *share, const char *alias,
                         uint db_stat, uint prgflag,
                         uint ha_open_flags, TABLE *outparam,
                         TABLE_LIST *table_desc, MEM_ROOT *mem_root);
static void close_old_data_files(THD *thd, TABLE *table, bool abort_locks,
                                 bool send_refresh);
static bool reopen_table(TABLE *table);
static bool
has_two_write_locked_tables_with_auto_increment(TABLE_LIST *tables);


extern "C" byte *table_cache_key(const byte *record,uint *length,
				 my_bool not_used __attribute__((unused)))
{
  TABLE *entry=(TABLE*) record;
  *length= entry->s->table_cache_key.length;
  return (byte*) entry->s->table_cache_key.str;
}


bool table_cache_init(void)
{
  mysql_rm_tmp_tables();
  return hash_init(&open_cache, &my_charset_bin, table_cache_size+16,
		   0, 0,table_cache_key,
		   (hash_free_key) free_cache_entry, 0) != 0;
}

void table_cache_free(void)
{
  DBUG_ENTER("table_cache_free");
  if (table_def_inited)
  {
    close_cached_tables((THD*) 0,0,(TABLE_LIST*) 0);
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
  uint key_length= (uint) (strmov(strmov(key, table_list->db)+1,
                                  table_list->table_name)-key)+1;
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

extern "C" byte *table_def_key(const byte *record, uint *length,
                               my_bool not_used __attribute__((unused)))
{
  TABLE_SHARE *entry=(TABLE_SHARE*) record;
  *length= entry->table_cache_key.length;
  return (byte*) entry->table_cache_key.str;
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
  thd			Table share
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
  if ((share= (TABLE_SHARE*) hash_search(&table_def_cache,(byte*) key,
                                         key_length)))
    goto found;

  if (!(share= alloc_table_share(table_list, key, key_length)))
  {
#ifdef WAITING_FOR_TABLE_DEF_CACHE_STAGE_3
    pthread_mutex_unlock(&LOCK_open);
#endif
    DBUG_RETURN(0);
  }

#ifdef WAITING_FOR_TABLE_DEF_CACHE_STAGE_3
  // We need a write lock to be able to add a new entry
  pthread_mutex_unlock(&LOCK_open);
  pthread_mutex_lock(&LOCK_open);
  /* Check that another thread didn't insert the same table in between */
  if ((old_share= hash_search(&table_def_cache, (byte*) key, key_length)))
  {
    (void) pthread_mutex_lock(&share->mutex);
    free_table_share(share);
    share= old_share;
    goto found;
  }
#endif

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

  if (my_hash_insert(&table_def_cache, (byte*) share))
  {
#ifdef WAITING_FOR_TABLE_DEF_CACHE_STAGE_3
    pthread_mutex_unlock(&LOCK_open);    
    (void) pthread_mutex_unlock(&share->mutex);
#endif
    free_table_share(share);
    DBUG_RETURN(0);				// return error
  }
#ifdef WAITING_FOR_TABLE_DEF_CACHE_STAGE_3
  pthread_mutex_unlock(&LOCK_open);
#endif
  if (open_table_def(thd, share, db_flags))
  {
#ifdef WAITING_FOR_TABLE_DEF_CACHE_STAGE_3
    /*
      No such table or wrong table definition file
      Lock first the table cache and then the mutex.
      This will ensure that no other thread is using the share
      structure.
    */
    (void) pthread_mutex_unlock(&share->mutex);
    (void) pthread_mutex_lock(&LOCK_open);
    (void) pthread_mutex_lock(&share->mutex);
#endif
    *error= share->error;
    (void) hash_delete(&table_def_cache, (byte*) share);
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
#ifdef WAITING_FOR_TABLE_DEF_CACHE_STAGE_3
  pthread_mutex_unlock(&LOCK_open);
#endif
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
    VOID(hash_delete(&table_def_cache, (byte*) oldest_unused_share));
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

  if ((share= get_table_share(thd, table_list, key, key_length, 
                              db_flags, error)) ||
      thd->net.last_errno != ER_NO_SUCH_TABLE)
    DBUG_RETURN(share);

  /* Table didn't exist. Check if some engine can provide it */
  if ((tmp= ha_create_table_from_engine(thd, table_list->db,
                                        table_list->table_name)) < 0)
  {
    /*
      No such table in any engine.
      Hide "Table doesn't exist" errors if table belong to view
    */
    if (table_list->belong_to_view)
    {
      TABLE_LIST *view= table_list->belong_to_view;
      thd->clear_error();
      my_error(ER_VIEW_INVALID, MYF(0),
               view->view_db.str, view->view_name.str);
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
    hash_delete(&table_def_cache, (byte*) share);
    DBUG_VOID_RETURN;
  }
  pthread_mutex_unlock(&share->mutex);
  DBUG_VOID_RETURN;


#ifdef WAITING_FOR_TABLE_DEF_CACHE_STAGE_3
  if (to_be_deleted)
  {
    /*
      We must try again with new locks as we must get LOCK_open
      before share->mutex
    */
    pthread_mutex_unlock(&share->mutex);
    pthread_mutex_lock(&LOCK_open);
    pthread_mutex_lock(&share->mutex);
    if (!share->ref_count)
    {						// No one is using this now
      TABLE_SHARE *name_lock;
      if (share->replace_with_name_lock && (name_lock=get_name_lock(share)))
      {
	/*
	  This code is execured when someone does FLUSH TABLES while on has
	  locked tables.
	 */
	(void) hash_search(&def_cache,(byte*) key,key_length);
	hash_replace(&def_cache, def_cache.current_record,(byte*) name_lock);
      }
      else
      {
	/* Remove table definition */
	hash_delete(&def_cache,(byte*) share);
      }
      pthread_mutex_unlock(&LOCK_open);
      free_table_share(share);
    }
    else
    {
      pthread_mutex_unlock(&LOCK_open);
      if (type == RELEASE_WAIT_FOR_DROP)
	wait_for_table(share, "Waiting for close");
      else
	pthread_mutex_unlock(&share->mutex);
    }
  }
  else if (type == RELEASE_WAIT_FOR_DROP)
    wait_for_table(share, "Waiting for close");
  else
    pthread_mutex_unlock(&share->mutex);
#endif
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
  TABLE_LIST table_list;
  uint key_length;
  safe_mutex_assert_owner(&LOCK_open);

  table_list.db= (char*) db;
  table_list.table_name= (char*) table_name;
  key_length= create_table_def_key((THD*) 0, key, &table_list, 0);
  return (TABLE_SHARE*) hash_search(&table_def_cache,(byte*) key, key_length);
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
*/


static void close_handle_and_leave_table_as_lock(TABLE *table)
{
  TABLE_SHARE *share, *old_share= table->s;
  MEM_ROOT *mem_root= &table->mem_root;
  DBUG_ENTER("close_handle_and_leave_table_as_lock");

  /*
    Make a local copy of the table share and free the current one.
    This has to be done to ensure that the table share is removed from
    the table defintion cache as soon as the last instance is removed
  */
  if ((share= (TABLE_SHARE*) alloc_root(mem_root, sizeof(*share))))
  {
    bzero((char*) share, sizeof(*share));
    share->db.str= memdup_root(mem_root, old_share->db.str,
                               old_share->db.length+1);
    share->db.length= old_share->db.length;
    share->table_name.str= memdup_root(mem_root,
                                       old_share->table_name.str,
                                       old_share->table_name.length+1);
    share->table_name.length= old_share->table_name.length;
    share->table_cache_key.str= memdup_root(mem_root,
                                            old_share->table_cache_key.str,
                                            old_share->table_cache_key.length);
    share->table_cache_key.length= old_share->table_cache_key.length;
    share->tmp_table= INTERNAL_TMP_TABLE;       // for intern_close_table()
  }

  table->file->close();
  table->db_stat= 0;                            // Mark file closed
  release_table_share(table->s, RELEASE_NORMAL);
  table->s= share;

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

    if (check_table_access(thd,SELECT_ACL | EXTRA_ACL,&table_list,1))
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
  my_free((gptr) table,MYF(0));
  DBUG_VOID_RETURN;
}

/* Free resources allocated by filesort() and read_record() */

void free_io_cache(TABLE *table)
{
  DBUG_ENTER("free_io_cache");
  if (table->sort.io_cache)
  {
    close_cached_file(table->sort.io_cache);
    my_free((gptr) table->sort.io_cache,MYF(0));
    table->sort.io_cache=0;
  }
  DBUG_VOID_RETURN;
}

/*
  Close all tables which aren't in use by any thread

  THD can be NULL, but then if_wait_for_refresh must be FALSE
  and tables must be NULL.
*/

bool close_cached_tables(THD *thd, bool if_wait_for_refresh,
			 TABLE_LIST *tables, bool have_lock)
{
  bool result=0;
  DBUG_ENTER("close_cached_tables");
  DBUG_ASSERT(thd || (!if_wait_for_refresh && !tables));

  if (!have_lock)
    VOID(pthread_mutex_lock(&LOCK_open));
  if (!tables)
  {
    refresh_version++;				// Force close of open tables
    while (unused_tables)
    {
#ifdef EXTRA_DEBUG
      if (hash_delete(&open_cache,(byte*) unused_tables))
	printf("Warning: Couldn't delete open table from hash\n");
#else
      VOID(hash_delete(&open_cache,(byte*) unused_tables));
#endif
    }
    /* Free table shares */
    while (oldest_unused_share->next)
    {
      pthread_mutex_lock(&oldest_unused_share->mutex);
      VOID(hash_delete(&table_def_cache, (byte*) oldest_unused_share));
    }
  }
  else
  {
    bool found=0;
    for (TABLE_LIST *table= tables; table; table= table->next_local)
    {
      if ((!table->table || !table->table->s->log_table) &&
          remove_table_from_cache(thd, table->db, table->table_name,
                                  RTFC_OWNED_BY_THD_FLAG))
	found=1;
    }
    if (!found)
      if_wait_for_refresh=0;			// Nothing to wait for
  }
#ifndef EMBEDDED_LIBRARY
  if (!tables)
    kill_delayed_threads();
#endif
  if (if_wait_for_refresh)
  {
    /*
      If there is any table that has a lower refresh_version, wait until
      this is closed (or this thread is killed) before returning
    */
    thd->mysys_var->current_mutex= &LOCK_open;
    thd->mysys_var->current_cond= &COND_refresh;
    thd->proc_info="Flushing tables";

    close_old_data_files(thd,thd->open_tables,1,1);
    mysql_ha_flush(thd, tables, MYSQL_HA_REOPEN_ON_USAGE | MYSQL_HA_FLUSH_ALL,
                   TRUE);
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
	if (!table->s->log_table &&
            ((table->s->version) < refresh_version && table->db_stat))
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
      table->s->version= refresh_version;
  }
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


/*
  Close all tables used by the current substatement, or all tables
  used by this thread if we are on the upper level.

  SYNOPSIS
    close_thread_tables()
    thd			Thread handler
    lock_in_use		Set to 1 (0 = default) if caller has a lock on
			LOCK_open
    skip_derived	Set to 1 (0 = default) if we should not free derived
			tables.
    stopper             When closing tables from thd->open_tables(->next)*, 
                        don't close/remove tables starting from stopper.

  IMPLEMENTATION
    Unlocks tables and frees derived tables.
    Put all normal tables used by thread in free list.

    When in prelocked mode it will only close/mark as free for reuse
    tables opened by this substatement, it will also check if we are
    closing tables after execution of complete query (i.e. we are on
    upper level) and will leave prelocked mode if needed.
*/

void close_thread_tables(THD *thd, bool lock_in_use, bool skip_derived)
{
  bool found_old_table;
  prelocked_mode_type prelocked_mode= thd->prelocked_mode;
  DBUG_ENTER("close_thread_tables");

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
  if (thd->derived_tables && !skip_derived)
  {
    TABLE *table, *next;
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

  if (prelocked_mode)
  {
    /*
      Mark all temporary tables used by this substatement as free for reuse.
    */
    mark_used_tables_as_free_for_reuse(thd, thd->temporary_tables);
  }

  if (thd->locked_tables || prelocked_mode)
  {
    /*
      Let us commit transaction for statement. Since in 5.0 we only have
      one statement transaction and don't allow several nested statement
      transactions this call will do nothing if we are inside of stored
      function or trigger (i.e. statement transaction is already active and
      does not belong to statement for which we do close_thread_tables()).
      TODO: This should be fixed in later releases.
    */
    ha_commit_stmt(thd);

    /* Ensure we are calling ha_reset() for all used tables */
    mark_used_tables_as_free_for_reuse(thd, thd->open_tables);

    /* We are under simple LOCK TABLES so should not do anything else. */
    if (!prelocked_mode || !thd->lex->requires_prelocking())
      DBUG_VOID_RETURN;

    /*
      We are in prelocked mode, so we have to leave it now with doing
      implicit UNLOCK TABLES if need.
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
#ifdef HAVE_ROW_BASED_REPLICATION
    thd->binlog_flush_pending_rows_event(TRUE);
#endif /*HAVE_ROW_BASED_REPLICATION*/
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  /*
    assume handlers auto-commit (if some doesn't - transaction handling
    in MySQL should be redesigned to support it; it's a big change,
    and it's not worth it - better to commit explicitly only writing
    transactions, read-only ones should better take care of themselves.
    saves some work in 2pc too)
    see also sql_parse.cc - dispatch_command()
  */
  if (!(thd->state_flags & Open_tables_state::BACKUPS_AVAIL))
    bzero(&thd->transaction.stmt, sizeof(thd->transaction.stmt));
  if (!thd->active_transaction())
    thd->transaction.xid_state.xid.null();

  /* VOID(pthread_sigmask(SIG_SETMASK,&thd->block_signals,NULL)); */
  if (!lock_in_use)
    VOID(pthread_mutex_lock(&LOCK_open));

  DBUG_PRINT("info", ("thd->open_tables: %p", thd->open_tables));

  found_old_table= 0;
  while (thd->open_tables)
    found_old_table|= close_thread_table(thd, &thd->open_tables);
  thd->some_tables_deleted=0;

  /* Free tables to hold down open files */
  while (open_cache.records > table_cache_size && unused_tables)
    VOID(hash_delete(&open_cache,(byte*) unused_tables)); /* purecov: tested */
  check_unused();
  if (found_old_table)
  {
    /* Tell threads waiting for refresh that something has happened */
    broadcast_refresh();
  }
  if (!lock_in_use)
    VOID(pthread_mutex_unlock(&LOCK_open));
  /*  VOID(pthread_sigmask(SIG_SETMASK,&thd->signals,NULL)); */

  if (prelocked_mode == PRELOCKED)
  {
    /*
      If we are here then we are leaving normal prelocked mode, so it is
      good idea to turn off OPTION_TABLE_LOCK flag.
    */
    DBUG_ASSERT(thd->lex->requires_prelocking());
    thd->options&= ~(ulong) (OPTION_TABLE_LOCK);
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
  DBUG_ASSERT(table->file->inited == handler::NONE);

  *table_ptr=table->next;
  if (table->s->version != refresh_version ||
      thd->version != refresh_version || !table->db_stat)
  {
    VOID(hash_delete(&open_cache,(byte*) table));
    found_old_table=1;
  }
  else
  {
    if (table->s->flush_version != flush_version)
    {
      table->s->flush_version= flush_version;
      table->file->extra(HA_EXTRA_FLUSH);
    }
    // Free memory and reset for next loop
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
  if (!thd->temporary_tables)
    return;

  if (!mysql_bin_log.is_open() || thd->current_stmt_binlog_row_based)
  {
    TABLE *next;
    for (table= thd->temporary_tables; table; table= next)
    {
      next=table->next;
      close_temporary(table, 1, 1);
    }
    thd->temporary_tables= 0;
    return;
  }

  TABLE *next,
    *prev_table /* TODO: 5.1 maintaines prev link in temporary_tables
                   double-linked list so we could fix it. But it is not necessary
                   at this time when the list is being destroyed */;
  bool was_quote_show= true; /* to assume thd->options has OPTION_QUOTE_SHOW_CREATE */
  // Better add "if exists", in case a RESET MASTER has been done
  const char stub[]= "DROP /*!40005 TEMPORARY */ TABLE IF EXISTS ";
  uint stub_len= sizeof(stub) - 1;
  char buf[256];
  memcpy(buf, stub, stub_len);
  String s_query= String(buf, sizeof(buf), system_charset_info);
  bool found_user_tables= false;
  LINT_INIT(next);

  /*
     insertion sort of temp tables by pseudo_thread_id to build ordered list
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
      !(was_quote_show= (thd->options & OPTION_QUOTE_SHOW_CREATE)))
  {
    thd->options |= OPTION_QUOTE_SHOW_CREATE;
  }

  /* scan sorted tmps to generate sequence of DROP */
  for (table= thd->temporary_tables; table; table= next)
  {
    if (is_user_table(table))
    {
      /* Set pseudo_thread_id to be that of the processed table */
      thd->variables.pseudo_thread_id= tmpkeyval(thd, table);
      /* Loop forward through all tables within the sublist of
         common pseudo_thread_id to create single DROP query */
      for (s_query.length(stub_len);
           table && is_user_table(table) &&
             tmpkeyval(thd, table) == thd->variables.pseudo_thread_id;
           table= next)
      {
        /*
          We are going to add 4 ` around the db/table names and possible more
          due to special characters in the names
        */
        append_identifier(thd, &s_query, table->s->db.str, strlen(table->s->db.str));
        s_query.q_append('.');
        append_identifier(thd, &s_query, table->s->table_name.str,
                          strlen(table->s->table_name.str));
        s_query.q_append(',');
        next= table->next;
        close_temporary(table, 1, 1);
      }
      thd->clear_error();
      CHARSET_INFO *cs_save= thd->variables.character_set_client;
      thd->variables.character_set_client= system_charset_info;
      Query_log_event qinfo(thd, s_query.ptr(),
                            s_query.length() - 1 /* to remove trailing ',' */,
                            0, FALSE);
      thd->variables.character_set_client= cs_save;
      /*
        Imagine the thread had created a temp table, then was doing a SELECT, and
        the SELECT was killed. Then it's not clever to mark the statement above as
        "killed", because it's not really a statement updating data, and there
        are 99.99% chances it will succeed on slave.
        If a real update (one updating a persistent table) was killed on the
        master, then this real update will be logged with error_code=killed,
        rightfully causing the slave to stop.
      */
      qinfo.error_code= 0;
      mysql_bin_log.write(&qinfo);
    }
    else
    {
      next= table->next;
      close_temporary(table, 1, 1);
    }
  }
  if (!was_quote_show)
    thd->options &= ~OPTION_QUOTE_SHOW_CREATE; /* restore option */
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
                               st_table_list *TABLE_LIST::*link,
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

    TODO: when we will have table/view change detection we can do this check
          only once for PS/SP

  RETURN
    found duplicate
    0 if table is unique
*/

TABLE_LIST* unique_table(THD *thd, TABLE_LIST *table, TABLE_LIST *table_list)
{
  TABLE_LIST *res;
  const char *d_name, *t_name;
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

  DBUG_PRINT("info", ("real table: %s.%s", d_name, t_name));
  for (;;)
  {
    if (((! (res= find_table_in_global_list(table_list, d_name, t_name))) &&
         (! (res= mysql_lock_have_duplicate(thd, table, table_list)))) ||
        ((!res->table || res->table != table->table) &&
         res->select_lex && !res->select_lex->exclude_from_table_unique_test))
      break;
    /*
      If we found entry of this table or or table of SELECT which already
      processed in derived table or top select of multi-update/multi-delete
      (exclude_from_table_unique_test).
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
      if (update->view == duplicate->view)
        my_error(ER_NON_UPDATABLE_TABLE, MYF(0), update->alias, operation);
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

  key_length= create_table_def_key(thd, key, table_list, 1);
  for (table=thd->temporary_tables ; table ; table= table->next)
  {
    if (table->s->table_cache_key.length == key_length &&
	!memcmp(table->s->table_cache_key.str, key, key_length))
      return table;
  }
  return 0;					// Not a temporary table
}


/*
  Close temporary table and unlink from thd->temporary tables
*/

bool close_temporary_table(THD *thd, TABLE_LIST *table_list)
{
  TABLE *table;

  if (!(table= find_temporary_table(thd, table_list)))
    return 1;
  close_temporary_table(thd, table, 1, 1);
  return 0;
}

/*
  unlink from thd->temporary tables and close temporary table
*/

void close_temporary_table(THD *thd, TABLE *table,
                           bool free_share, bool delete_table)
{
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
}


/*
  Close and delete a temporary table

  NOTE
    This dosn't unlink table from thd->temporary
    If this is needed, use close_temporary_table()
*/

void close_temporary(TABLE *table, bool free_share, bool delete_table)
{
  handlerton *table_type= table->s->db_type;
  DBUG_ENTER("close_temporary");

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
  TABLE_SHARE *share= table->s;
  TABLE_LIST table_list;
  uint db_length, table_length;
  DBUG_ENTER("rename_temporary_table");

  if (!(key=(char*) alloc_root(&share->mem_root,
			       (uint) (db_length= strlen(db))+
			       (uint) (table_length= strlen(table_name))+6+4)))
    DBUG_RETURN(1);				/* purecov: inspected */

  table_list.db= (char*) db;
  table_list.table_name= (char*) table_name;
  share->db.str= share->table_cache_key.str= key;
  share->db.length= db_length;
  share->table_cache_key.length= create_table_def_key(thd, key,
                                                      &table_list, 1);
  /*
    Here we use the fact that table_name is stored as the second component
    in the 'key' (after db_name), where components are separated with \0
  */
  share->table_name.str=    key+db_length+1;
  share->table_name.length= table_length;
  DBUG_RETURN(0);
}


	/* move table first in unused links */

static void relink_unused(TABLE *table)
{
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


/*
  Remove all instances of table from the current open list
  Free all locks on tables that are done with LOCK TABLES
 */

TABLE *unlink_open_table(THD *thd, TABLE *list, TABLE *find)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length= find->s->table_cache_key.length;
  TABLE *start=list,**prev,*next;
  prev= &start;

  memcpy(key, find->s->table_cache_key.str, key_length);
  for (; list ; list=next)
  {
    next=list->next;
    if (list->s->table_cache_key.length == key_length &&
	!memcmp(list->s->table_cache_key.str, key, key_length))
    {
      if (thd->locked_tables)
	mysql_lock_remove(thd, thd->locked_tables,list);
      VOID(hash_delete(&open_cache,(byte*) list)); // Close table
    }
    else
    {
      *prev=list;				// put in use list
      prev= &list->next;
    }
  }
  *prev=0;
  // Notify any 'refresh' threads
  broadcast_refresh();
  return start;
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
  thd->proc_info="Waiting for table";
  DBUG_ENTER("wait_for_condition");
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
  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  thd->proc_info= proc_info;
  pthread_mutex_unlock(&thd->mysys_var->mutex);
  DBUG_VOID_RETURN;
}


/*
  Open table which is already name-locked by this thread.

  SYNOPSIS
    reopen_name_locked_table()
      thd         Thread handle
      table_list  TABLE_LIST object for table to be open, TABLE_LIST::table
                  member should point to TABLE object which was used for
                  name-locking.

  NOTE
    This function assumes that its caller already acquired LOCK_open mutex.

  RETURN VALUE
    FALSE - Success
    TRUE  - Error
*/

bool reopen_name_locked_table(THD* thd, TABLE_LIST* table_list)
{
  TABLE *table= table_list->table;
  TABLE_SHARE *share;
  char *db= table_list->db;
  char *table_name= table_list->table_name;
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  TABLE orig_table;
  DBUG_ENTER("reopen_name_locked_table");

  safe_mutex_assert_owner(&LOCK_open);

  if (thd->killed || !table)
    DBUG_RETURN(TRUE);

  orig_table= *table;
  key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;

  if (open_unireg_entry(thd, table, table_list, table_name,
                        table->s->table_cache_key.str,
                        table->s->table_cache_key.length, thd->mem_root))
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
  share->version=0;
  share->flush_version=0;
  table->in_use = thd;
  check_unused();
  table->next = thd->open_tables;
  thd->open_tables = table;
  table->tablenr=thd->current_tablenr++;
  table->used_fields=0;
  table->const_table=0;
  table->null_row= table->maybe_null= table->force_index= 0;
  table->status=STATUS_NO_RECORD;
  table->keys_in_use_for_query= share->keys_in_use;
  table->used_keys= share->keys_for_keyread;
  DBUG_RETURN(FALSE);
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
                          someone has done a flush or namelock on it.
                          No version number checking is done.
                          MYSQL_OPEN_IGNORE_LOCKED_TABLES - Open table
                          ignoring set of locked tables and prelocked mode.

  IMPLEMENTATION
    Uses a cache of open tables to find a table not in use.

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

  /* find a unused table in the open table cache */
  if (refresh)
    *refresh=0;

  /* an open table operation needs a lot of the stack space */
  if (check_stack_overrun(thd, STACK_MIN_SIZE_FOR_OPEN, (char *)&alias))
    DBUG_RETURN(0);

  if (thd->killed)
    DBUG_RETURN(0);

  key_length= (create_table_def_key(thd, key, table_list, 1) -
               TMP_TABLE_KEY_EXTRA);

  if (!table_list->skip_temporary)
  {
    for (table= thd->temporary_tables; table ; table=table->next)
    {
      if (table->s->table_cache_key.length == key_length +
          TMP_TABLE_KEY_EXTRA &&
	  !memcmp(table->s->table_cache_key.str, key,
		  key_length + TMP_TABLE_KEY_EXTRA))
      {
	if (table->query_id == thd->query_id ||
            thd->prelocked_mode && table->query_id)
	{
	  my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->alias);
	  DBUG_RETURN(0);
	}
	table->query_id= thd->query_id;
	table->clear_query_id= 1;
	thd->tmp_table_used= 1;
        DBUG_PRINT("info",("Using temporary table"));
        goto reset;
      }
    }
  }

  if (!(flags & MYSQL_OPEN_IGNORE_LOCKED_TABLES) &&
      (thd->locked_tables || thd->prelocked_mode))
  {						// Using table locks
    TABLE *best_table= 0;
    int best_distance= INT_MIN;
    bool check_if_used= thd->prelocked_mode &&
                        ((int) table_list->lock_type >=
                         (int) TL_WRITE_ALLOW_WRITE);
    for (table=thd->open_tables; table ; table=table->next)
    {
      if (table->s->table_cache_key.length == key_length &&
	  !memcmp(table->s->table_cache_key.str, key, key_length))
      {
        if (check_if_used && table->query_id &&
            table->query_id != thd->query_id)
        {
          /*
            If we are in stored function or trigger we should ensure that
            we won't change table that is already used by calling statement.
            So if we are opening table for writing, we should check that it
            is not already open by some calling stamement.
          */
          my_error(ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG, MYF(0),
                   table->s->table_name.str);
          DBUG_RETURN(0);
        }
        if (!my_strcasecmp(system_charset_info, table->alias, alias) &&
            table->query_id != thd->query_id && /* skip tables already used */
            !(thd->prelocked_mode && table->query_id))
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
          if (best_distance < 0 && distance > best_distance ||
              distance >= 0 && distance < best_distance)
          {
            best_distance= distance;
            best_table= table;
            if (best_distance == 0 && !check_if_used)
            {
              /*
                If we have found perfect match and we don't need to check that
                table is not used by one of calling statements (assuming that
                we are inside of function or trigger) we can finish iterating
                through open tables list.
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
      is it view?
      (it is work around to allow to open view with locked tables,
      real fix will be made after definition cache will be made)
    */
    {
      char path[FN_REFLEN];
      enum legacy_db_type not_used;
      build_table_filename(path, sizeof(path) - 1,
                           table_list->db, table_list->table_name, reg_ext);
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
                              key, key_length, mem_root))
        {
          DBUG_ASSERT(table_list->view != 0);
          VOID(pthread_mutex_unlock(&LOCK_open));
          DBUG_RETURN(0); // VIEW
        }
        VOID(pthread_mutex_unlock(&LOCK_open));
      }
    }
    my_error(ER_TABLE_NOT_LOCKED, MYF(0), alias);
    DBUG_RETURN(0);
  }

  VOID(pthread_mutex_lock(&LOCK_open));

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

  /* close handler tables which are marked for flush */
  if (thd->handler_tables)
    mysql_ha_flush(thd, (TABLE_LIST*) NULL, MYSQL_HA_REOPEN_ON_USAGE, TRUE);

  for (table= (TABLE*) hash_first(&open_cache, (byte*) key, key_length,
                                  &state);
       table && table->in_use ;
       table= (TABLE*) hash_next(&open_cache, (byte*) key, key_length,
                                 &state))
  {
    /*
      Here we flush tables marked for flush. However we never flush log
      tables here. They are flushed only on FLUSH LOGS.
    */
    if (table->s->version != refresh_version && !table->s->log_table)
    {
      DBUG_PRINT("note",
                 ("Found table '%s.%s' with different refresh version",
                  table_list->db, table_list->table_name));

      if (flags & MYSQL_LOCK_IGNORE_FLUSH)
      {
        /* Force close at once after usage */
        thd->version= table->s->version;
        continue;
      }

      /*
        There is a refresh in progress for this table
        Wait until the table is freed or the thread is killed.
      */
      close_old_data_files(thd,thd->open_tables,0,0);
      if (table->in_use != thd)
        wait_for_condition(thd, &LOCK_open, &COND_refresh);
      else
      {
	VOID(pthread_mutex_unlock(&LOCK_open));
      }
      if (refresh)
	*refresh=1;
      DBUG_RETURN(0);
    }
  }
  if (table)
  {
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
    /* Free cache if too big */
    while (open_cache.records > table_cache_size && unused_tables)
      VOID(hash_delete(&open_cache,(byte*) unused_tables)); /* purecov: tested */

    /* make a new table */
    if (!(table=(TABLE*) my_malloc(sizeof(*table),MYF(MY_WME))))
    {
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }

    if (open_unireg_entry(thd, table, table_list, alias, key, key_length,
                          mem_root))
    {
      my_free((gptr)table, MYF(0));
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }
    if (table_list->view)
    {
      my_free((gptr)table, MYF(0));
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(0); // VIEW
    }
    DBUG_PRINT("info", ("inserting table %p into the cache", table));
    VOID(my_hash_insert(&open_cache,(byte*) table));
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
  table->null_row= table->maybe_null= table->force_index= 0;
  table->status=STATUS_NO_RECORD;
  table->keys_in_use_for_query= table->s->keys_in_use;
  table->insert_values= 0;
  table->used_keys= table->s->keys_for_keyread;
  table->fulltext_searched= 0;
  table->file->ft_handler= 0;
  if (table->timestamp_field)
    table->timestamp_field_type= table->timestamp_field->get_auto_set_type();
  table_list->updatable= 1; // It is not derived table nor non-updatable VIEW
  table->clear_column_bitmaps();
  DBUG_ASSERT(table->key_read == 0);
  DBUG_RETURN(table);
}


TABLE *find_locked_table(THD *thd, const char *db,const char *table_name)
{
  char	key[MAX_DBKEY_LENGTH];
  uint key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;

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

static bool reopen_table(TABLE *table)
{
  TABLE tmp;
  bool error= 1;
  Field **field;
  uint key,part;
  TABLE_LIST table_list;
  THD *thd= table->in_use;
  DBUG_ENTER("reopen_table");

  DBUG_ASSERT(table->s->ref_count == 0);
  DBUG_ASSERT(!table->sort.io_cache);

#ifdef EXTRA_DEBUG
  if (table->db_stat)
    sql_print_error("Table %s had a open data handler in reopen_table",
		    table->alias);
#endif

  table_list.db=         table->s->db.str;
  table_list.table_name= table->s->table_name.str;
  table_list.table=      table;
  table_list.belong_to_view= 0;
  table_list.next_local= 0;

  if (wait_for_locked_table_names(thd, &table_list))
    DBUG_RETURN(1);                             // Thread was killed

  if (open_unireg_entry(thd, &tmp, &table_list,
			table->alias,
                        table->s->table_cache_key.str,
                        table->s->table_cache_key.length,
                        thd->mem_root))
    goto end;

  /* This list copies variables set by open_table */
  tmp.tablenr=		table->tablenr;
  tmp.used_fields=	table->used_fields;
  tmp.const_table=	table->const_table;
  tmp.null_row=		table->null_row;
  tmp.maybe_null=	table->maybe_null;
  tmp.status=		table->status;
  tmp.keys_in_use_for_query= tmp.s->keys_in_use;
  tmp.used_keys= 	tmp.s->keys_for_keyread;

  tmp.s->table_map_id=  table->s->table_map_id;

  /* Get state */
  tmp.in_use=    	thd;
  tmp.reginfo.lock_type=table->reginfo.lock_type;
  tmp.grant=		table->grant;

  /* Replace table in open list */
  tmp.next=		table->next;
  tmp.prev=		table->prev;

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
      table->key_info[key].key_part[part].field->table= table;
  }
  if (table->triggers)
    table->triggers->set_table(table);

  broadcast_refresh();
  error=0;

 end:
  DBUG_RETURN(error);
}


/*
  Used with ALTER TABLE:
  Close all instanses of table when LOCK TABLES is in used;
  Close first all instances of table and then reopen them
*/

bool close_data_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table;
  DBUG_ENTER("close_data_tables");

  for (table=thd->open_tables; table ; table=table->next)
  {
    if (!strcmp(table->s->table_name.str, table_name) &&
	!strcmp(table->s->db.str, db))
    {
      mysql_lock_remove(thd, thd->locked_tables,table);
      close_handle_and_leave_table_as_lock(table);
    }
  }
  DBUG_RETURN(0);                               // For the future
}


/*
  Reopen all tables with closed data files
  One should have lock on LOCK_open when calling this
*/

bool reopen_tables(THD *thd,bool get_locks,bool in_refresh)
{
  TABLE *table,*next,**prev;
  TABLE **tables,**tables_ptr;			// For locks
  bool error=0, not_used;
  DBUG_ENTER("reopen_tables");

  if (!thd->open_tables)
    DBUG_RETURN(0);

  safe_mutex_assert_owner(&LOCK_open);
  if (get_locks)
  {
    /* The ptr is checked later */
    uint opens=0;
    for (table= thd->open_tables; table ; table=table->next)
      opens++;
    tables= (TABLE**) my_alloca(sizeof(TABLE*)*opens);
  }
  else
    tables= &thd->open_tables;
  tables_ptr =tables;

  prev= &thd->open_tables;
  for (table=thd->open_tables; table ; table=next)
  {
    uint db_stat=table->db_stat;
    next=table->next;
    if (!tables || (!db_stat && reopen_table(table)))
    {
      my_error(ER_CANT_REOPEN_TABLE, MYF(0), table->alias);
      VOID(hash_delete(&open_cache,(byte*) table));
      error=1;
    }
    else
    {
      *prev= table;
      prev= &table->next;
      if (get_locks && !db_stat)
	*tables_ptr++= table;			// need new lock on this
      if (in_refresh)
      {
	table->s->version=0;
	table->locked_by_flush=0;
      }
    }
  }
  if (tables != tables_ptr)			// Should we get back old locks
  {
    MYSQL_LOCK *lock;
    /* We should always get these locks */
    thd->some_tables_deleted=0;
    if ((lock= mysql_lock_tables(thd, tables, (uint) (tables_ptr - tables),
                                 0, &not_used)))
    {
      thd->locked_tables=mysql_lock_merge(thd->locked_tables,lock);
    }
    else
      error=1;
  }
  if (get_locks && tables)
  {
    my_afree((gptr) tables);
  }
  broadcast_refresh();
  *prev=0;
  DBUG_RETURN(error);
}


/*
  Close handlers for tables in list, but leave the TABLE structure
  intact so that we can re-open these quickly
  abort_locks is set if called from flush_tables.
*/

void close_old_data_files(THD *thd, TABLE *table, bool abort_locks,
			  bool send_refresh)
{
  bool found= send_refresh;
  DBUG_ENTER("close_old_data_files");

  for (; table ; table=table->next)
  {
    /*
      Reopen marked for flush. But close log tables. They are flushed only
      explicitly on FLUSH LOGS
    */
    if (table->s->version != refresh_version && !table->s->log_table)
    {
      found=1;
      if (table->db_stat)
      {
	if (abort_locks)
	{
	  mysql_lock_abort(thd,table, TRUE);	// Close waiting threads
	  mysql_lock_remove(thd, thd->locked_tables,table);
	  table->locked_by_flush=1;		// Will be reopened with locks
	}
        close_handle_and_leave_table_as_lock(table);
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
  NOTE: log tables are handled differently by the logging routines.
        E.g. general_log is always opened and locked by the logger
        and the table handler used by the logger, will be skipped by
        this check.
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
    for (TABLE *search= (TABLE*) hash_first(&open_cache, (byte*) key,
                                             key_length, &state);
	 search ;
         search= (TABLE*) hash_next(&open_cache, (byte*) key,
                                    key_length, &state))
    {
      DBUG_PRINT("info", ("share: 0x%lx  locked_by_logger: %d "
                          "locked_by_flush: %d  locked_by_name: %d "
                          "db_stat: %u  version: %u",
                          (ulong) search->s, search->locked_by_logger,
                          search->locked_by_flush, search->locked_by_name,
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
        However we fo not wait if we encountered a table, locked by the logger.
        Log tables are managed separately by logging routines.
      */
      if (!search->locked_by_logger &&
          (search->locked_by_name && wait_for_name_lock ||
           search->locked_by_flush ||
           (search->db_stat && search->s->version < refresh_version)))
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

  thd->proc_info="Waiting for tables";
  pthread_mutex_lock(&LOCK_open);
  while (!thd->killed)
  {
    thd->some_tables_deleted=0;
    close_old_data_files(thd,thd->open_tables,0,dropping_tables != 0);
    mysql_ha_flush(thd, (TABLE_LIST*) NULL, MYSQL_HA_REOPEN_ON_USAGE, TRUE);
    if (!table_is_used(thd->open_tables,1))
      break;
    (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
  }
  if (thd->killed)
    result= 1;					// aborted
  else
  {
    /* Now we can open all tables without any interference */
    thd->proc_info="Reopen tables";
    thd->version= refresh_version;
    result=reopen_tables(thd,0,0);
  }
  pthread_mutex_unlock(&LOCK_open);
  thd->proc_info=0;
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

  for (table= thd->open_tables; table ; table=next)
  {
    next=table->next;
    if (!strcmp(table->s->table_name.str, table_name) &&
	!strcmp(table->s->db.str, db))
    {
      mysql_lock_remove(thd, thd->locked_tables,table);
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
        VOID(hash_delete(&open_cache,(byte*) table));
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
    my_free((gptr) thd->locked_tables,MYF(0));
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
      mysql_lock_abort(thd,table, TRUE);
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
    shares open at the same time).

    share->table_map_id is not ~0UL.
 */
void assign_new_table_id(TABLE_SHARE *share)
{
  static ulong last_table_id= ~0UL;

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
                             MEM_ROOT *mem_root)
{
  int error;
  TABLE_SHARE *share;
  uint discover_retry_count= 0;
  DBUG_ENTER("open_unireg_entry");

  safe_mutex_assert_owner(&LOCK_open);

retry:
  if (!(share= get_table_share_with_create(thd, table_list, cache_key,
                                           cache_key_length, 
                                           OPEN_VIEW, &error)))
    DBUG_RETURN(1);

  if (share->is_view)
  {
    /* Open view */
    error= (int) open_new_frm(thd, share, alias,
                              (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE |
                                      HA_GET_INDEX | HA_TRY_READ_ONLY),
                              READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
                              thd->open_options, entry, table_list,
                              mem_root);
    if (error)
      goto err;
    /* TODO: Don't free this */
    release_table_share(share, RELEASE_NORMAL);
    DBUG_RETURN(0);
  }

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
 	(entry->file->is_crashed() && entry->file->check_and_repair(thd)))
     {
       /* Give right error message */
       thd->clear_error();
       my_error(ER_NOT_KEYFILE, MYF(0), share->table_name.str, my_errno);
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
      char *query, *end;
      uint query_buf_size= 20 + share->db.length + share->table_name.length +1;
      if ((query= (char*) my_malloc(query_buf_size,MYF(MY_WME))))
      {
        /* this DELETE FROM is needed even with row-based binlogging */
        end = strxmov(strmov(query, "DELETE FROM `"),
                      share->db.str,"`.`",share->table_name.str,"`", NullS);
        thd->binlog_query(THD::STMT_QUERY_TYPE,
                          query, (ulong)(end-query), FALSE, FALSE);
        my_free(query, MYF(0));
      }
      else
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


/*
  Open all tables in list

  SYNOPSIS
    open_tables()
    thd - thread handler
    start - list of tables in/out
    counter - number of opened tables will be return using this parameter
    flags   - bitmap of flags to modify how the tables will be open:
              MYSQL_LOCK_IGNORE_FLUSH - open table even if someone has
              done a flush or namelock on it.

  NOTE
    Unless we are already in prelocked mode, this function will also precache
    all SP/SFs explicitly or implicitly (via views and triggers) used by the
    query and add tables needed for their execution to table list. If resulting
    tables list will be non empty it will mark query as requiring precaching.
    Prelocked mode will be enabled for such query during lock_tables() call.

    If query for which we are opening tables is already marked as requiring
    prelocking it won't do such precaching and will simply reuse table list
    which is already built.

  RETURN
    0  - OK
    -1 - error
*/

int open_tables(THD *thd, TABLE_LIST **start, uint *counter, uint flags)
{
  TABLE_LIST *tables;
  bool refresh;
  int result=0;
  MEM_ROOT new_frm_mem;
  /* Also used for indicating that prelocking is need */
  TABLE_LIST **query_tables_last_own;
  DBUG_ENTER("open_tables");
  /*
    temporary mem_root for new .frm parsing.
    TODO: variables for size
  */
  init_alloc_root(&new_frm_mem, 8024, 8024);

  thd->current_tablenr= 0;
 restart:
  *counter= 0;
  query_tables_last_own= 0;
  thd->proc_info="Opening tables";

  /*
    If we are not already executing prelocked statement and don't have
    statement for which table list for prelocking is already built, let
    us cache routines and try to build such table list.

  */

  if (!thd->prelocked_mode && !thd->lex->requires_prelocking() &&
      thd->lex->sroutines_list.elements)
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

  for (tables= *start; tables ;tables= tables->next_global)
  {
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
    if (tables->schema_table)
    {
      if (!mysql_schema_table(thd, thd->lex, tables))
        continue;
      DBUG_RETURN(-1);
    }
    (*counter)++;
    
    if (!tables->table &&
	!(tables->table= open_table(thd, tables, &new_frm_mem, &refresh, flags)))
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
          tables->table->triggers &&
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
      tables->table->reginfo.lock_type=tables->lock_type;
    tables->table->grant= tables->grant;

process_view_routines:
    /*
      Again we may need cache all routines used by this view and add
      tables used by them to table list.
    */
    if (tables->view && !thd->prelocked_mode &&
        !thd->lex->requires_prelocking() &&
        tables->view->sroutines_list.elements)
    {
      /* We have at least one table in TL here. */
      if (!query_tables_last_own)
        query_tables_last_own= thd->lex->query_tables_last;
      if (sp_cache_routines_and_add_tables_for_view(thd, thd->lex, tables))
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
  }

 err:
  thd->proc_info=0;
  free_root(&new_frm_mem, MYF(0));              // Free pre-alloced block

  if (query_tables_last_own)
    thd->lex->mark_as_requiring_prelocking(query_tables_last_own);

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


/*
  Open and lock one table

  SYNOPSIS
    open_ltable()
    thd			Thread handler
    table_list		Table to open is first table in this list
    lock_type		Lock to use for open

  NOTE
    This function don't do anything like SP/SF/views/triggers analysis done
    in open_tables(). It is intended for opening of only one concrete table.
    And used only in special contexts.

  RETURN VALUES
    table		Opened table
    0			Error
  
    If ok, the following are also set:
      table_list->lock_type 	lock_type
      table_list->table		table
*/

TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type lock_type)
{
  TABLE *table;
  bool refresh;
  DBUG_ENTER("open_ltable");

  thd->proc_info="Opening table";
  thd->current_tablenr= 0;
  /* open_ltable can be used only for BASIC TABLEs */
  table_list->required_type= FRMTYPE_TABLE;
  while (!(table= open_table(thd, table_list, thd->mem_root, &refresh, 0)) &&
         refresh)
    ;

  if (table)
  {
#if defined( __WIN__)
    /* Win32 can't drop a file that is open */
    if (lock_type == TL_WRITE_ALLOW_READ)
    {
      lock_type= TL_WRITE;
    }
#endif /* __WIN__ */
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
	if (! (thd->lock= mysql_lock_tables(thd, &table_list->table, 1, 0,
                                            &refresh)))
	  table= 0;
    }
  }
  thd->proc_info=0;
  DBUG_RETURN(table);
}


/*
  Open all tables in list and locks them for read without derived
  tables processing.

  SYNOPSIS
    simple_open_n_lock_tables()
    thd		- thread handler
    tables	- list of tables for open&locking

  RETURN
    0  - ok
    -1 - error

  NOTE
    The lock will automaticaly be freed by close_thread_tables()
*/

int simple_open_n_lock_tables(THD *thd, TABLE_LIST *tables)
{
  uint counter;
  bool need_reopen;
  DBUG_ENTER("simple_open_n_lock_tables");

  for ( ; ; ) 
  {
    if (open_tables(thd, &tables, &counter, 0))
      DBUG_RETURN(-1);
    if (!lock_tables(thd, tables, counter, &need_reopen))
      break;
    if (!need_reopen)
      DBUG_RETURN(-1);
    close_tables_for_reopen(thd, &tables);
  }
  DBUG_RETURN(0);
}


/*
  Open all tables in list, locks them and process derived tables
  tables processing.

  SYNOPSIS
    open_and_lock_tables()
    thd		- thread handler
    tables	- list of tables for open&locking

  RETURN
    FALSE - ok
    TRUE  - error

  NOTE
    The lock will automaticaly be freed by close_thread_tables()
*/

bool open_and_lock_tables(THD *thd, TABLE_LIST *tables)
{
  uint counter;
  bool need_reopen;
  DBUG_ENTER("open_and_lock_tables");

  for ( ; ; ) 
  {
    if (open_tables(thd, &tables, &counter, 0))
      DBUG_RETURN(-1);
    if (!lock_tables(thd, tables, counter, &need_reopen))
      break;
    if (!need_reopen)
      DBUG_RETURN(-1);
    close_tables_for_reopen(thd, &tables);
  }
  if (mysql_handle_derived(thd->lex, &mysql_derived_prepare) ||
      (thd->fill_derived_tables() &&
       mysql_handle_derived(thd->lex, &mysql_derived_filling)))
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
                  done a flush or namelock on it.

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
    if (!table->placeholder() && !table->schema_table)
      table->table->query_id= 0;
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

#ifdef HAVE_ROW_BASED_REPLICATION
  /*
    CREATE ... SELECT UUID() locks no tables, we have to test here.
  */
  if (thd->lex->binlog_row_based_if_mixed)
    thd->set_current_stmt_binlog_row_based_if_mixed();
#endif /*HAVE_ROW_BASED_REPLICATION*/

  if (!tables && !thd->lex->requires_prelocking())
    DBUG_RETURN(0);

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

    if (!(ptr=start=(TABLE**) thd->alloc(sizeof(TABLE*)*count)))
      DBUG_RETURN(-1);
    for (table= tables; table; table= table->next_global)
    {
      if (!table->placeholder() && !table->schema_table)
	*(ptr++)= table->table;
    }

    /* We have to emulate LOCK TABLES if we are statement needs prelocking. */
    if (thd->lex->requires_prelocking())
    {
      thd->in_lock_tables=1;
      thd->options|= OPTION_TABLE_LOCK;
#ifdef HAVE_ROW_BASED_REPLICATION
      /*
        If we have >= 2 different tables to update with auto_inc columns,
        statement-based binlogging won't work. We can solve this problem in
        mixed mode by switching to row-based binlogging:
      */
      if (thd->variables.binlog_format == BINLOG_FORMAT_MIXED &&
          has_two_write_locked_tables_with_auto_increment(tables))
      {
        thd->lex->binlog_row_based_if_mixed= TRUE;
        thd->set_current_stmt_binlog_row_based_if_mixed();
      }
#endif
    }

    if (! (thd->lock= mysql_lock_tables(thd, start, (uint) (ptr - start),
                                        MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN,
                                        need_reopen)))
    {
      if (thd->lex->requires_prelocking())
      {
        thd->options&= ~(ulong) (OPTION_TABLE_LOCK);
        thd->in_lock_tables=0;
      }
      DBUG_RETURN(-1);
    }
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

      for (table= tables; table != first_not_own; table= table->next_global)
      {
        if (!table->placeholder() && !table->schema_table)
        {
          table->table->query_id= thd->query_id;
          if (check_lock_and_start_stmt(thd, table->table, table->lock_type))
          {
            ha_rollback_stmt(thd);
            mysql_unlock_tables(thd, thd->locked_tables);
            thd->locked_tables= 0;
            thd->options&= ~(ulong) (OPTION_TABLE_LOCK);
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
    for (table= tables; table != first_not_own; table= table->next_global)
    {
      if (!table->placeholder() && !table->schema_table &&
	  check_lock_and_start_stmt(thd, table->table, table->lock_type))
      {
	ha_rollback_stmt(thd);
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
  DBUG_RETURN(0);
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
  mark_used_tables_as_free_for_reuse(thd, thd->temporary_tables);
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

  init_tmp_table_share(share, saved_cache_key, key_length,
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
                     TRANSACTIONAL_TMP_TABLE : TMP_TABLE);

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
  DBUG_RETURN(tmp_table);
}


bool rm_temporary_table(handlerton *base, char *path)
{
  bool error=0;
  handler *file;
  char *ext;
  DBUG_ENTER("rm_temporary_table");

  strmov(ext= strend(path), reg_ext);
  if (my_delete(path,MYF(0)))
    error=1; /* purecov: inspected */
  *ext= 0;				// remove extension
  file= get_new_handler((TABLE_SHARE*) 0, current_thd->mem_root, base);
  if (file && file->delete_table(path))
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
    MY_BITMAP *current_bitmap, *other_bitmap;

    /*
      We always want to register the used keys, as the column bitmap may have
      been set for all fields (for example for view).
    */
      
    table->used_keys.intersect(field->part_of_key);
    table->merge_keys.merge(field->part_of_key);

    if (thd->mark_used_columns == MARK_COLUMNS_READ)
    {
      current_bitmap= table->read_set;
      other_bitmap=   table->write_set;
    }
    else
    {
      current_bitmap= table->write_set;
      other_bitmap=   table->read_set;
    }

    if (bitmap_fast_test_and_set(current_bitmap, field->field_index))
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
  Query_arena *arena, backup;  
  LINT_INIT(arena);
  
  DBUG_ASSERT(table_list->schema_table_reformed ||
              (ref != 0 && table_list->view != 0));
  for (; !field_it.end_of_fields(); field_it.next())
  {
    if (!my_strcasecmp(system_charset_info, field_it.name(), name))
    {
      // in PS use own arena or data will be freed after prepare
      if (register_tree_change)
        arena= thd->activate_stmt_arena_if_needed(&backup);
      /*
        create_item() may, or may not create a new Item, depending on
        the column reference. See create_view_field() for details.
      */
      Item *item= field_it.create_item(thd);
      if (register_tree_change && arena)
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
  Natural_join_column *nj_col;
  Field *found_field;
  Query_arena *arena, backup;
  DBUG_ENTER("find_field_in_natural_join");
  DBUG_PRINT("enter", ("field name: '%s', ref 0x%lx",
		       name, (ulong) ref));
  DBUG_ASSERT(table_ref->is_natural_join && table_ref->join_columns);
  DBUG_ASSERT(*actual_table == NULL);

  LINT_INIT(arena);
  LINT_INIT(found_field);

  for (;;)
  {
    if (!(nj_col= field_it++))
      DBUG_RETURN(NULL);

    if (!my_strcasecmp(system_charset_info, nj_col->name(), name))
      break;
  }

  if (nj_col->view_field)
  {
    Item *item;
    if (register_tree_change)
      arena= thd->activate_stmt_arena_if_needed(&backup);
    /*
      create_item() may, or may not create a new Item, depending on the
      column reference. See create_view_field() for details.
    */
    item= nj_col->create_item(thd);
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
    DBUG_ASSERT(nj_col->table_ref->table == nj_col->table_field->table);
    found_field= nj_col->table_field;
    update_field_dependencies(thd, found_field, nj_col->table_ref->table);
  }

  *actual_table= nj_col->table_ref;
  
  DBUG_RETURN(found_field);
}


/*
  Find field by name in a base table or a view with temp table algorithm.

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
    field_ptr= (Field**) hash_search(&table->s->name_hash, (byte*) name,
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
              it->walk(&Item::register_field_in_read_map, 1, (byte *) 0);
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
    field_ptr= (Field**)hash_search(&table->s->name_hash,(byte*) name,
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
      found= find_field_in_table(thd, table_ref->table, name, length,
                                 TRUE, &(item->cached_field_index));
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
                                              check_privileges, allow_rowid,
                                              &(item->cached_field_index),
                                              register_tree_change,
                                              &actual_table);
    if (cur_field)
    {
      if (cur_field == WRONG_GRANT)
	return (Field*) 0;

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
    char buff[NAME_LEN*2+1];
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
    unaliased                   Set to true if item is field which was found
                                by original field name and not by its alias
                                in item list. Set to false otherwise.

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
                  find_item_error_report_type report_error, bool *unaliased)
{
  List_iterator<Item> li(items);
  Item **found=0, **found_unaliased= 0, *item;
  const char *db_name=0;
  const char *field_name=0;
  const char *table_name=0;
  bool found_unaliased_non_uniq= 0;
  uint unaliased_counter;

  LINT_INIT(unaliased_counter);                 // Dependent on found_unaliased

  *unaliased= FALSE;

  if (find->type() == Item::FIELD_ITEM	|| find->type() == Item::REF_ITEM)
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
            !strcmp(item_field->table_name, table_name) &&
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
          if (db_name)
            break;                              // Perfect match
        }
      }
      else if (!my_strcasecmp(system_charset_info, item_field->name,
                              field_name))
      {
        /*
          If table name was not given we should scan through aliases
          (or non-aliased fields) first. We are also checking unaliased
          name of the field in then next else-if, to be able to find
          instantly field (hidden by alias) if no suitable alias (or
          non-aliased field) was found.
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
      }
      else if (!my_strcasecmp(system_charset_info, item_field->field_name,
                              field_name))
      {
        /*
          We will use un-aliased field or react on such ambiguities only if
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
        else
        {
          found_unaliased= li.ref();
          unaliased_counter= i;
        }
      }
    }
    else if (!table_name && (find->eq(item,0) ||
			     find->name && item->name &&
			     !my_strcasecmp(system_charset_info, 
					    item->name,find->name)))
    {
      found= li.ref();
      *counter= i;
      break;
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
      *unaliased= TRUE;
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
    if (!(nj_col_1= it_1.get_or_create_column_ref(leaf_1)))
      goto err;
    field_name_1= nj_col_1->name();

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
      if (!(cur_nj_col_2= it_2.get_or_create_column_ref(leaf_2)))
        goto err;
      cur_field_name_2= cur_nj_col_2->name();

      /*
        Compare the two columns and check for duplicate common fields.
        A common field is duplicate either if it was already found in
        table_ref_2 (then found == TRUE), or if a field in table_ref_2
        was already matched by some previous field in table_ref_1
        (then cur_nj_col_2->is_common == TRUE).
      */
      if (!my_strcasecmp(system_charset_info, field_name_1, cur_field_name_2))
      {
        if (found || cur_nj_col_2->is_common)
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
    if (nj_col_2 &&
        (!using_fields ||
          test_if_string_in_list(field_name_1, using_fields)))
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

      if (field_1)
      {
        TABLE *table_1= nj_col_1->table_ref->table;
        /* Mark field_1 used for table cache. */
        bitmap_set_bit(table_1->read_set, field_1->field_index);
        table_1->used_keys.intersect(field_1->part_of_key);
        table_1->merge_keys.merge(field_1->part_of_key);
      }
      if (field_2)
      {
        TABLE *table_2= nj_col_2->table_ref->table;
        /* Mark field_2 used for table cache. */
        bitmap_set_bit(table_2->read_set, field_2->field_index);
        table_2->used_keys.intersect(field_2->part_of_key);
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
    TABLE_LIST *cur_left_neighbor= nested_it++;
    TABLE_LIST *cur_right_neighbor= NULL;

    while (cur_left_neighbor)
    {
      TABLE_LIST *cur_table_ref= cur_left_neighbor;
      cur_left_neighbor= nested_it++;
      /*
        The order of RIGHT JOIN operands is reversed in 'join list' to
        transform it into a LEFT JOIN. However, in this procedure we need
        the join operands in their lexical order, so below we reverse the
        join operands. Notice that this happens only in the first loop, and
        not in the second one, as in the second loop cur_left_neighbor == NULL.
        This is the correct behavior, because the second loop
        sets cur_table_ref reference correctly after the join operands are
        swapped in the first loop.
      */
      if (cur_left_neighbor &&
          cur_table_ref->outer_join & JOIN_TYPE_RIGHT)
      {
        /* This can happen only for JOIN ... ON. */
        DBUG_ASSERT(table_ref->nested_join->join_list.elements == 2);
        swap_variables(TABLE_LIST*, cur_left_neighbor, cur_table_ref);
      }

      if (cur_table_ref->nested_join &&
          store_top_level_join_columns(thd, cur_table_ref,
                                       cur_left_neighbor, cur_right_neighbor))
        goto err;
      cur_right_neighbor= cur_table_ref;
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

  /* Note that tables in the list are in reversed order */
  for (left_neighbor= table_ref_it++; left_neighbor ; )
  {
    table_ref= left_neighbor;
    left_neighbor= table_ref_it++;
    /* For stored procedures do not redo work if already done. */
    if (context->select_lex->first_execution)
    {
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
        it.replace(new Item_int("Not_used", (longlong) 1, 21));
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
  }
  if (arena)
  {
    /* make * substituting permanent */
    SELECT_LEX *select_lex= thd->lex->current_select;
    select_lex->with_wild= 0;
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
  DBUG_ENTER("setup_fields");

  thd->mark_used_columns= mark_used_columns;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
  if (allow_sum_func)
    thd->lex->allow_sum_func|= 1 << thd->lex->current_select->nest_level;
  thd->where= THD::DEFAULT_WHERE;

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

  Item **ref= ref_pointer_array;
  while ((item= it++))
  {
    if (!item->fixed && item->fix_fields(thd, it.ref()) ||
	(item= *(it.ref()))->check_cols(1))
    {
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
    thd->used_tables|= item->used_tables();
  }
  thd->lex->allow_sum_func= save_allow_sum_func;
  thd->mark_used_columns= save_mark_used_columns;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
  DBUG_RETURN(test(thd->net.report_error));
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

  context->table_list= context->first_name_resolution_table= tables;

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
    table->used_keys= table->s->keys_for_keyread;
    table->merge_keys.clear_all();
    if (table_list->use_index)
    {
      key_map map;
      get_key_map_from_key_list(&map, table, table_list->use_index);
      if (map.is_set_all())
	DBUG_RETURN(1);
      table->keys_in_use_for_query=map;
    }
    if (table_list->ignore_index)
    {
      key_map map;
      get_key_map_from_key_list(&map, table, table_list->ignore_index);
      if (map.is_set_all())
	DBUG_RETURN(1);
      table->keys_in_use_for_query.subtract(map);
    }
    table->used_keys.intersect(table->keys_in_use_for_query);
  }
  if (tablenr > MAX_TABLES)
  {
    my_error(ER_TOO_MANY_TABLES,MYF(0),MAX_TABLES);
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
                                   ulong want_access)
{
  TABLE_LIST *leaves_tmp= NULL;

  if (setup_tables(thd, context, from_clause, tables,
                   &leaves_tmp, select_insert))
    return TRUE;

  *leaves= leaves_tmp;

  for (; leaves_tmp; leaves_tmp= leaves_tmp->next_leaf)
  {
    if (leaves_tmp->belong_to_view && 
        check_single_table_access(thd, want_access,  leaves_tmp))
    {
      tables->hide_view_error(thd);
      return TRUE;
    }
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

    if (table_name && my_strcasecmp(table_alias_charset, table_name,
                                    tables->alias) ||
        (db_name && strcmp(tables->db,db_name)))
      continue;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    /* Ensure that we have access rights to all fields to be inserted. */
    if (!((table && (table->grant.privilege & SELECT_ACL) ||
           tables->view && (tables->grant.privilege & SELECT_ACL))) &&
        !any_privileges)
    {
      field_iterator.set(tables);
      if (check_grant_all_columns(thd, SELECT_ACL, field_iterator.grant(),
                                  field_iterator.db_name(),
                                  field_iterator.table_name(),
                                  &field_iterator))
        DBUG_RETURN(TRUE);
    }
#endif

    /*
      Update the tables used in the query based on the referenced fields. For
      views and natural joins this update is performed inside the loop below.
    */
    if (table)
      thd->used_tables|= table->map;

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
        DBUG_ASSERT(tables->field_translation == NULL && table ||
                    tables->is_natural_join);
        DBUG_ASSERT(item->type() == Item::FIELD_ITEM);
        Item_field *fld= (Item_field*) item;
        const char *field_table_name= field_iterator.table_name();

        if (!tables->schema_table && 
            !(fld->have_privileges=
              (get_column_grant(thd, field_iterator.grant(),
                                field_iterator.db_name(),
                                field_table_name, fld->field_name) &
               VIEW_ANY_ACL)))
        {
          my_error(ER_COLUMNACCESS_DENIED_ERROR, MYF(0), "ANY",
                   thd->security_ctx->priv_user,
                   thd->security_ctx->host_or_ip,
                   fld->field_name, field_table_name);
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
          table->used_keys.intersect(field->part_of_key);
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
            thd->used_tables|= field_table->map;
            field_table->used_keys.intersect(field->part_of_key);
            field_table->merge_keys.merge(field->part_of_key);
            field_table->used_fields++;
          }
        }
      }
      else
        thd->used_tables|= item->used_tables();
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
  DBUG_ENTER("setup_conds");

  if (select_lex->conds_processed_with_permanent_arena ||
      arena->is_conventional())
    arena= 0;                                   // For easier test

  thd->mark_used_columns= MARK_COLUMNS_READ;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
  select_lex->cond_count= 0;

  for (table= tables; table; table= table->next_local)
  {
    if (table->prepare_where(thd, conds, FALSE))
      goto err_no_arena;
  }

  if (*conds)
  {
    thd->where="where clause";
    if (!(*conds)->fixed && (*conds)->fix_fields(thd, conds) ||
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
        if (!embedded->on_expr->fixed &&
            embedded->on_expr->fix_fields(thd, &embedded->on_expr) ||
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
  DBUG_RETURN(test(thd->net.report_error));

err_no_arena:
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

  RETURN
    FALSE   OK
    TRUE    error occured
*/

static bool
fill_record(THD * thd, List<Item> &fields, List<Item> &values,
            bool ignore_errors)
{
  List_iterator_fast<Item> f(fields),v(values);
  Item *value;
  Item_field *field;
  DBUG_ENTER("fill_record");

  while ((field=(Item_field*) f++))
  {
    value=v++;
    Field *rfield= field->field;
    TABLE *table= rfield->table;
    if (rfield == table->next_number_field)
      table->auto_increment_field_not_null= TRUE;
    if ((value->save_in_field(rfield, 0) < 0) && !ignore_errors)
    {
      my_message(ER_UNKNOWN_ERROR, ER(ER_UNKNOWN_ERROR), MYF(0));
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(thd->net.report_error);
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
          triggers && triggers->process_triggers(thd, event,
                                                 TRG_ACTION_BEFORE, TRUE));
}


/*
  Fill field buffer with values from Field list

  SYNOPSIS
    fill_record()
    thd           thread handler
    ptr           pointer on pointer to record
    values        list of fields
    ignore_errors TRUE if we should ignore errors

  RETURN
    FALSE   OK
    TRUE    error occured
*/

bool
fill_record(THD *thd, Field **ptr, List<Item> &values, bool ignore_errors)
{
  List_iterator_fast<Item> v(values);
  Item *value;
  DBUG_ENTER("fill_record");

  Field *field;
  while ((field = *ptr++))
  {
    value=v++;
    TABLE *table= field->table;
    if (field == table->next_number_field)
      table->auto_increment_field_not_null= TRUE;
    if (value->save_in_field(field, 0) == -1)
      DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(thd->net.report_error);
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
          triggers && triggers->process_triggers(thd, event,
                                                 TRG_ACTION_BEFORE, TRUE));
}


static void mysql_rm_tmp_tables(void)
{
  uint i, idx;
  char	filePath[FN_REFLEN], *tmpdir;
  MY_DIR *dirp;
  FILEINFO *file;
  DBUG_ENTER("mysql_rm_tmp_tables");

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

      if (!bcmp(file->name,tmp_file_prefix,tmp_file_prefix_length))
      {
        sprintf(filePath,"%s%c%s",tmpdir,FN_LIBCHAR,file->name);
        VOID(my_delete(filePath,MYF(MY_WME)));
      }
    }
    my_dirend(dirp);
  }
  DBUG_VOID_RETURN;
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
    VOID(hash_delete(&open_cache,(byte*) unused_tables));
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
    hash_delete(&open_cache,(byte*) unused_tables);
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
  DBUG_PRINT("enter", ("Table: '%s.%s'  flags: %u", db, table_name, flags));

  key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;
  for (;;)
  {
    HASH_SEARCH_STATE state;
    result= signalled= 0;

    for (table= (TABLE*) hash_first(&open_cache, (byte*) key, key_length,
                                    &state);
         table;
         table= (TABLE*) hash_next(&open_cache, (byte*) key, key_length,
                                   &state))
    {
      THD *in_use;

      table->s->version=0L;		/* Free when thread is ready */
      if (!(in_use=table->in_use))
      {
        DBUG_PRINT("info",("Table was not in use"));
        relink_unused(table);
      }
      else if (in_use != thd)
      {
        DBUG_PRINT("info", ("Table was in use by other thread"));
        in_use->some_tables_deleted=1;
        if (table->db_stat)
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
	  as the thread may be waiting to get a lock for another table
        */
        for (TABLE *thd_table= in_use->open_tables;
	     thd_table ;
	     thd_table= thd_table->next)
        {
	  if (thd_table->db_stat)		// If table is open
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
      VOID(hash_delete(&open_cache,(byte*) unused_tables));

    DBUG_PRINT("info", ("Removing table from table_def_cache"));
    /* Remove table from table definition cache if it's not in use */
    if ((share= (TABLE_SHARE*) hash_search(&table_def_cache,(byte*) key,
                                           key_length)))
    {
      DBUG_PRINT("info", ("share version: %lu  ref_count: %u",
                          share->version, share->ref_count));
      share->version= 0;                          // Mark for delete
      if (share->ref_count == 0)
      {
        pthread_mutex_lock(&share->mutex);
        VOID(hash_delete(&table_def_cache, (byte*) share));
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
    thd->proc_info="FULLTEXT initialization";

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
  char path[FN_REFLEN];
  DBUG_ENTER("open_new_frm");

  /* Create path with extension */
  pathstr.length= (uint) (strxmov(path, share->normalized_path.str, reg_ext,
                                  NullS)- path);
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
      if (mysql_make_view(thd, parser, table_desc))
        goto err;
    }
    else
    {
      /* only VIEWs are supported now */
      my_error(ER_FRM_UNKNOWN_TYPE, MYF(0), share->path,  parser->type()->str);
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
    abort_and_upgrade_lock()
    lpt                           Parameter passing struct
    All parameters passed through the ALTER_PARTITION_PARAM_TYPE object
  RETURN VALUE
    0
  DESCRIPTION
    Remember old lock level (for possible downgrade later on), abort all
    waiting threads and ensure that all keeping locks currently are
    completed such that we own the lock exclusively and no other interaction
    is ongoing.

    thd                           Thread object
    table                         Table object
    db                            Database name
    table_name                    Table name
    old_lock_level                Old lock level
*/

int abort_and_upgrade_lock(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  uint flags= RTFC_WAIT_OTHER_THREAD_FLAG | RTFC_CHECK_KILLED_FLAG;
  DBUG_ENTER("abort_and_upgrade_locks");

  lpt->old_lock_type= lpt->table->reginfo.lock_type;
  VOID(pthread_mutex_lock(&LOCK_open));
  mysql_lock_abort(lpt->thd, lpt->table, TRUE);
  VOID(remove_table_from_cache(lpt->thd, lpt->db, lpt->table_name, flags));
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

void close_open_tables_and_downgrade(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  VOID(pthread_mutex_lock(&LOCK_open));
  remove_table_from_cache(lpt->thd, lpt->db, lpt->table_name,
                          RTFC_WAIT_OTHER_THREAD_FLAG);
  VOID(pthread_mutex_unlock(&LOCK_open));
  mysql_lock_downgrade_write(lpt->thd, lpt->table, lpt->old_lock_type);
}


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

  key_length=(uint) (strmov(strmov(key,lpt->db)+1,lpt->table_name)-key)+1;
  VOID(pthread_mutex_lock(&LOCK_open));
  HASH_SEARCH_STATE state;
  for (table= (TABLE*) hash_first(&open_cache,(byte*) key,key_length,
                                  &state) ;
       table;
       table= (TABLE*) hash_next(&open_cache,(byte*) key,key_length,
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
        as the thread may be waiting to get a lock for another table
      */
      for (TABLE *thd_table= in_use->open_tables;
           thd_table ;
           thd_table= thd_table->next)
      {
        if (thd_table->db_stat)		// If table is open
          mysql_lock_abort_for_thread(lpt->thd, thd_table);
      }
    }
  }
  /*
    We start by removing all unused objects from the cache and marking
    those in use for removal after completion. Now we also need to abort
    all that are locked and are not progressing due to being locked
    by our lock. We don't upgrade our lock here.
  */
  mysql_lock_abort(lpt->thd, my_table, FALSE);
  VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_VOID_RETURN;
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

  RETURN
    0  No
    1  Yes
*/

static bool
has_two_write_locked_tables_with_auto_increment(TABLE_LIST *tables)
{
  char *first_table_name= NULL, *first_db;
  for (TABLE_LIST *table= tables; table; table= table->next_global)
  {
    /* we must do preliminary checks as table->table may be NULL */
    if (!table->placeholder() && !table->schema_table &&
        table->table->found_next_number_field &&
        (table->lock_type >= TL_WRITE_ALLOW_WRITE))
    {
      if (first_table_name == NULL)
      {
        first_table_name= table->table_name;
        first_db= table->db;
        DBUG_ASSERT(first_db);
      }
      else if (strcmp(first_db, table->db) ||
               strcmp(first_table_name, table->table_name))
        return 1;
    }
  }
  return 0;
}
