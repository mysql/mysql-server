/*
   Copyright (c) 2000, 2011, Oracle and/or its affiliates.

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


/**
  @file

  Locking functions for mysql.

  Because of the new concurrent inserts, we must first get external locks
  before getting internal locks.  If we do it in the other order, the status
  information is not up to date when called from the lock handler.

  GENERAL DESCRIPTION OF LOCKING

  When not using LOCK TABLES:

  - For each SQL statement mysql_lock_tables() is called for all involved
    tables.
    - mysql_lock_tables() will call
      table_handler->external_lock(thd,locktype) for each table.
      This is followed by a call to thr_multi_lock() for all tables.

  - When statement is done, we call mysql_unlock_tables().
    table_handler->external_lock(thd, F_UNLCK) followed by
    thr_multi_unlock() for each table.

  - Note that mysql_unlock_tables() may be called several times as
    MySQL in some cases can free some tables earlier than others.

  - The above is true both for normal and temporary tables.

  - Temporary non transactional tables are never passed to thr_multi_lock()
    and we never call external_lock(thd, F_UNLOCK) on these.

  When using LOCK TABLES:

  - LOCK TABLE will call mysql_lock_tables() for all tables.
    mysql_lock_tables() will call
    table_handler->external_lock(thd,locktype) for each table.
    This is followed by a call to thr_multi_lock() for all tables.

  - For each statement, we will call table_handler->start_stmt(THD)
    to inform the table handler that we are using the table.

    The tables used can only be tables used in LOCK TABLES or a
    temporary table.

  - When statement is done, we will call ha_commit_stmt(thd);

  - When calling UNLOCK TABLES we call mysql_unlock_tables() for all
    tables used in LOCK TABLES

  If table_handler->external_lock(thd, locktype) fails, we call
  table_handler->external_lock(thd, F_UNLCK) for each table that was locked,
  excluding one that caused failure. That means handler must cleanup itself
  in case external_lock() fails.

  @todo
  Change to use my_malloc() ONLY when using LOCK TABLES command or when
  we are forced to use mysql_lock_merge.
*/

#include "sql_priv.h"
#include "debug_sync.h"
#include "unireg.h"                    // REQUIRED: for other includes
#include "lock.h"
#include "sql_base.h"                       // close_tables_for_reopen
#include "sql_parse.h"                     // is_log_table_write_query
#include "sql_acl.h"                       // SUPER_ACL
#include <hash.h>
#include <assert.h>

/**
  @defgroup Locking Locking
  @{
*/

extern HASH open_cache;

static int lock_external(THD *thd, TABLE **table,uint count);
static int unlock_external(THD *thd, TABLE **table,uint count);
static void print_lock_error(int error, const char *);

/* Map the return value of thr_lock to an error from errmsg.txt */
static int thr_lock_errno_to_mysql[]=
{ 0, ER_LOCK_ABORTED, ER_LOCK_WAIT_TIMEOUT, ER_LOCK_DEADLOCK };

/**
  Perform semantic checks for mysql_lock_tables.
  @param thd The current thread
  @param tables The tables to lock
  @param count The number of tables to lock
  @param flags Lock flags
  @return 0 if all the check passed, non zero if a check failed.
*/

static int
lock_tables_check(THD *thd, TABLE **tables, uint count, uint flags)
{
  uint system_count, i;
  bool is_superuser, log_table_write_query;

  DBUG_ENTER("lock_tables_check");

  system_count= 0;
  is_superuser= thd->security_ctx->master_access & SUPER_ACL;
  log_table_write_query= (is_log_table_write_query(thd->lex->sql_command)
                         || ((flags & MYSQL_LOCK_LOG_TABLE) != 0));

  for (i=0 ; i<count; i++)
  {
    TABLE *t= tables[i];

    /* Protect against 'fake' partially initialized TABLE_SHARE */
    DBUG_ASSERT(t->s->table_category != TABLE_UNKNOWN_CATEGORY);

    /*
      Table I/O to performance schema tables is performed
      only internally by the server implementation.
      When a user is requesting a lock, the following
      constraints are enforced:
    */
    if (t->s->require_write_privileges() &&
        ! log_table_write_query)
    {
      /*
        A user should not be able to prevent writes,
        or hold any type of lock in a session,
        since this would be a DOS attack.
      */
      if ((t->reginfo.lock_type >= TL_READ_NO_INSERT)
          || (thd->lex->sql_command == SQLCOM_LOCK_TABLES))
      {
        my_error(ER_CANT_LOCK_LOG_TABLE, MYF(0));
        DBUG_RETURN(1);
      }
    }

    if (t->reginfo.lock_type >= TL_WRITE_ALLOW_WRITE)
    {
      if (t->s->table_category == TABLE_CATEGORY_SYSTEM)
        system_count++;

      if (t->db_stat & HA_READ_ONLY)
      {
        my_error(ER_OPEN_AS_READONLY, MYF(0), t->alias.c_ptr_safe());
        DBUG_RETURN(1);
      }
    }

    /*
      If we are going to lock a non-temporary table we must own metadata
      lock of appropriate type on it (I.e. for table to be locked for
      write we must own metadata lock of MDL_SHARED_WRITE or stronger
      type. For table to be locked for read we must own metadata lock
      of MDL_SHARED_READ or stronger type).
      The only exception are HANDLER statements which are allowed to
      lock table for read while having only MDL_SHARED lock on it.
    */
    DBUG_ASSERT(t->s->tmp_table ||
                thd->mdl_context.is_lock_owner(MDL_key::TABLE,
                                 t->s->db.str, t->s->table_name.str,
                                 t->reginfo.lock_type >= TL_WRITE_ALLOW_WRITE ?
                                 MDL_SHARED_WRITE : MDL_SHARED_READ) ||
                (t->open_by_handler &&
                 thd->mdl_context.is_lock_owner(MDL_key::TABLE,
                                  t->s->db.str, t->s->table_name.str,
                                  MDL_SHARED)));

    /*
      Prevent modifications to base tables if READ_ONLY is activated.
      In any case, read only does not apply to temporary tables.
    */
    if (!(flags & MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY) && !t->s->tmp_table)
    {
      if (t->reginfo.lock_type >= TL_WRITE_ALLOW_WRITE &&
          !is_superuser && opt_readonly && !thd->slave_thread)
      {
        my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
        DBUG_RETURN(1);
      }
    }
  }

  /*
    Locking of system tables is restricted:
    locking a mix of system and non-system tables in the same lock
    is prohibited, to prevent contention.
  */
  if ((system_count > 0) && (system_count < count))
  {
    my_error(ER_WRONG_LOCK_OF_SYSTEM_TABLE, MYF(0));
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

/**
  Reset lock type in lock data

  @param mysql_lock             Lock structures to reset.
  @param unlock			If set, then set lock type to TL_UNLOCK,
  				otherwise set to original lock type from
				get_store_lock().

  @note After a locking error we want to quit the locking of the table(s).
        The test case in the bug report for Bug #18544 has the following
        cases: 1. Locking error in lock_external() due to InnoDB timeout.
        2. Locking error in get_lock_data() due to missing write permission.
        3. Locking error in wait_if_global_read_lock() due to lock conflict.

  @note In all these cases we have already set the lock type into the lock
        data of the open table(s). If the table(s) are in the open table
        cache, they could be reused with the non-zero lock type set. This
        could lead to ignoring a different lock type with the next lock.

  @note Clear the lock type of all lock data. This ensures that the next
        lock request will set its lock type properly.
*/


void reset_lock_data(MYSQL_LOCK *sql_lock, bool unlock)
{
  THR_LOCK_DATA **ldata, **ldata_end;
  DBUG_ENTER("reset_lock_data");

  /* Clear the lock type of all lock data to avoid reusage. */
  for (ldata= sql_lock->locks, ldata_end= ldata + sql_lock->lock_count;
       ldata < ldata_end;
       ldata++)
    (*ldata)->type= unlock ? TL_UNLOCK : (*ldata)->org_type;
  DBUG_VOID_RETURN;
}


/**
   Lock tables.

   @param thd          The current thread.
   @param tables       An array of pointers to the tables to lock.
   @param count        The number of tables to lock.
   @param flags        Options:
                 MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY Ignore SET GLOBAL READ_ONLY
                 MYSQL_LOCK_IGNORE_TIMEOUT          Use maximum timeout value.

   @retval  A lock structure pointer on success.
   @retval  NULL if an error or if wait on a lock was killed.
*/

MYSQL_LOCK *mysql_lock_tables(THD *thd, TABLE **tables, uint count, uint flags)
{
  MYSQL_LOCK *sql_lock;
  DBUG_ENTER("mysql_lock_tables(tables)");

  if (lock_tables_check(thd, tables, count, flags))
    DBUG_RETURN(NULL);

  if (! (sql_lock= get_lock_data(thd, tables, count, GET_LOCK_STORE_LOCKS)))
    DBUG_RETURN(NULL);

  if (mysql_lock_tables(thd, sql_lock, flags))
  {
    /* Clear the lock type of all lock data to avoid reusage. */
    reset_lock_data(sql_lock, 1);
    my_free(sql_lock);
    sql_lock= 0;
  }
  DBUG_RETURN(sql_lock);
}

/**
   Lock tables based on a MYSQL_LOCK structure.

   mysql_lock_tables()

   @param thd			The current thread.
   @param sql_lock		Tables that should be locked
   @param flags			See mysql_lock_tables() above

   @return 0   ok
   @return 1  error
*/

bool mysql_lock_tables(THD *thd, MYSQL_LOCK *sql_lock, uint flags)
{
  int rc= 1;
  ulong timeout= (flags & MYSQL_LOCK_IGNORE_TIMEOUT) ?
    LONG_TIMEOUT : thd->variables.lock_wait_timeout;

  DBUG_ENTER("mysql_lock_tables(sql_lock)");

  thd_proc_info(thd, "System lock");
  if (sql_lock->table_count && lock_external(thd, sql_lock->table,
                                             sql_lock->table_count))
    goto end;

  thd_proc_info(thd, "Table lock");

  /* Copy the lock data array. thr_multi_lock() reorders its contents. */
  memcpy(sql_lock->locks + sql_lock->lock_count, sql_lock->locks,
         sql_lock->lock_count * sizeof(*sql_lock->locks));
  /* Lock on the copied half of the lock data array. */
  rc= thr_lock_errno_to_mysql[(int) thr_multi_lock(sql_lock->locks +
                                                   sql_lock->lock_count,
                                                   sql_lock->lock_count,
                                                   &thd->lock_info, timeout)];
  if (rc && sql_lock->table_count)
    (void) unlock_external(thd, sql_lock->table, sql_lock->table_count);

end:
  thd_proc_info(thd, "After table lock");

  if (thd->killed)
  {
    thd->send_kill_message();
    if (!rc)
      mysql_unlock_tables(thd, sql_lock, 0);
    rc= 1;
  }
  else if (rc > 1)
    my_error(rc, MYF(0));

  thd->set_time_after_lock();
  DBUG_RETURN(rc);
}


static int lock_external(THD *thd, TABLE **tables, uint count)
{
  reg1 uint i;
  int lock_type,error;
  DBUG_ENTER("lock_external");

  DBUG_PRINT("info", ("count %d", count));
  for (i=1 ; i <= count ; i++, tables++)
  {
    DBUG_ASSERT((*tables)->reginfo.lock_type >= TL_READ);
    lock_type=F_WRLCK;				/* Lock exclusive */
    if ((*tables)->db_stat & HA_READ_ONLY ||
	((*tables)->reginfo.lock_type >= TL_READ &&
	 (*tables)->reginfo.lock_type <= TL_READ_NO_INSERT))
      lock_type=F_RDLCK;

    if ((error=(*tables)->file->ha_external_lock(thd,lock_type)))
    {
      print_lock_error(error, (*tables)->file->table_type());
      while (--i)
      {
        tables--;
	(*tables)->file->ha_external_lock(thd, F_UNLCK);
	(*tables)->current_lock=F_UNLCK;
      }
      DBUG_RETURN(error);
    }
    else
    {
      (*tables)->db_stat &= ~ HA_BLOCK_LOCK;
      (*tables)->current_lock= lock_type;
    }
  }
  DBUG_RETURN(0);
}


void mysql_unlock_tables(THD *thd, MYSQL_LOCK *sql_lock, bool free_lock)
{
  DBUG_ENTER("mysql_unlock_tables");
  if (sql_lock->table_count)
    unlock_external(thd, sql_lock->table, sql_lock->table_count);
  if (sql_lock->lock_count)
    thr_multi_unlock(sql_lock->locks, sql_lock->lock_count, 0);
  if (free_lock)
    my_free(sql_lock);
  DBUG_VOID_RETURN;
}

/**
  Unlock some of the tables locked by mysql_lock_tables.

  This will work even if get_lock_data fails (next unlock will free all)
*/

void mysql_unlock_some_tables(THD *thd, TABLE **table,uint count)
{
  MYSQL_LOCK *sql_lock;
  if ((sql_lock= get_lock_data(thd, table, count, GET_LOCK_UNLOCK)))
    mysql_unlock_tables(thd, sql_lock, 1);
}


/**
  unlock all tables locked for read.
*/

void mysql_unlock_read_tables(THD *thd, MYSQL_LOCK *sql_lock)
{
  uint i,found;
  DBUG_ENTER("mysql_unlock_read_tables");

  /* Call external lock for all tables to be unlocked */

  /* Move all write locked tables first */
  TABLE **table=sql_lock->table;
  for (i=found=0 ; i < sql_lock->table_count ; i++)
  {
    DBUG_ASSERT(sql_lock->table[i]->lock_position == i);
    if ((uint) sql_lock->table[i]->reginfo.lock_type > TL_WRITE_ALLOW_WRITE)
    {
      swap_variables(TABLE *, *table, sql_lock->table[i]);
      table++;
      found++;
    }
  }
  /* Unlock all read locked tables */
  if (i != found)
  {
    (void) unlock_external(thd,table,i-found);
    sql_lock->table_count=found;
  }

  /* Call thr_unlock() for all tables to be unlocked */

  /* Move all write locks first */
  THR_LOCK_DATA **lock=sql_lock->locks;
  for (i=found=0 ; i < sql_lock->lock_count ; i++)
  {
    if (sql_lock->locks[i]->type >= TL_WRITE_ALLOW_WRITE)
    {
      swap_variables(THR_LOCK_DATA *, *lock, sql_lock->locks[i]);
      lock++;
      found++;
    }
  }
  /* unlock the read locked tables */
  if (i != found)
  {
    thr_multi_unlock(lock, i-found, 0);
    sql_lock->lock_count= found;
  }

  /* Fix the lock positions in TABLE */
  table= sql_lock->table;
  found= 0;
  for (i= 0; i < sql_lock->table_count; i++)
  {
    TABLE *tbl= *table;
    tbl->lock_position= (uint) (table - sql_lock->table);
    tbl->lock_data_start= found;
    found+= tbl->lock_count;
    table++;
  }
  DBUG_VOID_RETURN;
}


/**
  Try to find the table in the list of locked tables.
  In case of success, unlock the table and remove it from this list.
  If a table has more than one lock instance, removes them all.

  @param  thd             thread context
  @param  locked          list of locked tables
  @param  table           the table to unlock
*/

void mysql_lock_remove(THD *thd, MYSQL_LOCK *locked,TABLE *table)
{
  if (locked)
  {
    reg1 uint i;
    for (i=0; i < locked->table_count; i++)
    {
      if (locked->table[i] == table)
      {
        uint  j, removed_locks, old_tables;
        TABLE *tbl;
        uint lock_data_end;

        DBUG_ASSERT(table->lock_position == i);

        /* Unlock the table. */
        mysql_unlock_some_tables(thd, &table, /* table count */ 1);

        /* Decrement table_count in advance, making below expressions easier */
        old_tables= --locked->table_count;

        /* The table has 'removed_locks' lock data elements in locked->locks */
        removed_locks= table->lock_count;

        /* Move down all table pointers above 'i'. */
	bmove((char*) (locked->table+i),
	      (char*) (locked->table+i+1),
	      (old_tables - i) * sizeof(TABLE*));

        lock_data_end= table->lock_data_start + table->lock_count;
        /* Move down all lock data pointers above 'table->lock_data_end-1' */
        bmove((char*) (locked->locks + table->lock_data_start),
              (char*) (locked->locks + lock_data_end),
              (locked->lock_count - lock_data_end) *
              sizeof(THR_LOCK_DATA*));

        /*
          Fix moved table elements.
          lock_position is the index in the 'locked->table' array,
          it must be fixed by one.
          table->lock_data_start is pointer to the lock data for this table
          in the 'locked->locks' array, they must be fixed by 'removed_locks',
          the lock data count of the removed table.
        */
        for (j= i ; j < old_tables; j++)
        {
          tbl= locked->table[j];
          tbl->lock_position--;
          DBUG_ASSERT(tbl->lock_position == j);
          tbl->lock_data_start-= removed_locks;
        }

        /* Finally adjust lock_count. */
        locked->lock_count-= removed_locks;
	break;
      }
    }
  }
}


/** Abort all other threads waiting to get lock in table. */

void mysql_lock_abort(THD *thd, TABLE *table, bool upgrade_lock)
{
  MYSQL_LOCK *locked;
  DBUG_ENTER("mysql_lock_abort");

  if ((locked= get_lock_data(thd, &table, 1, GET_LOCK_UNLOCK)))
  {
    for (uint i=0; i < locked->lock_count; i++)
      thr_abort_locks(locked->locks[i]->lock, upgrade_lock);
    my_free(locked);
  }
  DBUG_VOID_RETURN;
}


/**
  Abort one thread / table combination.

  @param thd	   Thread handler
  @param table	   Table that should be removed from lock queue

  @retval
    0  Table was not locked by another thread
  @retval
    1  Table was locked by at least one other thread
*/

bool mysql_lock_abort_for_thread(THD *thd, TABLE *table)
{
  MYSQL_LOCK *locked;
  bool result= FALSE;
  DBUG_ENTER("mysql_lock_abort_for_thread");

  if ((locked= get_lock_data(thd, &table, 1, GET_LOCK_UNLOCK)))
  {
    for (uint i=0; i < locked->lock_count; i++)
    {
      if (thr_abort_locks_for_thread(locked->locks[i]->lock,
                                     table->in_use->thread_id))
        result= TRUE;
    }
    my_free(locked);
  }
  DBUG_RETURN(result);
}


/**
  Merge two thr_lock:s
  mysql_lock_merge()

  @param a	Original locks
  @param b	New locks

  @retval	New lock structure that contains a and b

  @note
  a and b are freed with my_free()
*/

MYSQL_LOCK *mysql_lock_merge(MYSQL_LOCK *a,MYSQL_LOCK *b)
{
  MYSQL_LOCK *sql_lock;
  TABLE **table, **end_table;
  DBUG_ENTER("mysql_lock_merge");
  DBUG_PRINT("enter", ("a->lock_count: %u  b->lock_count: %u",
                       a->lock_count, b->lock_count));

  if (!(sql_lock= (MYSQL_LOCK*)
	my_malloc(sizeof(*sql_lock)+
		  sizeof(THR_LOCK_DATA*)*((a->lock_count+b->lock_count)*2) +
		  sizeof(TABLE*)*(a->table_count+b->table_count),MYF(MY_WME))))
    DBUG_RETURN(0);				// Fatal error
  sql_lock->lock_count=a->lock_count+b->lock_count;
  sql_lock->table_count=a->table_count+b->table_count;
  sql_lock->locks=(THR_LOCK_DATA**) (sql_lock+1);
  sql_lock->table=(TABLE**) (sql_lock->locks+sql_lock->lock_count*2);
  memcpy(sql_lock->locks,a->locks,a->lock_count*sizeof(*a->locks));
  memcpy(sql_lock->locks+a->lock_count,b->locks,
	 b->lock_count*sizeof(*b->locks));
  memcpy(sql_lock->table,a->table,a->table_count*sizeof(*a->table));
  memcpy(sql_lock->table+a->table_count,b->table,
	 b->table_count*sizeof(*b->table));

  /*
    Now adjust lock_position and lock_data_start for all objects that was
    moved in 'b' (as there is now all objects in 'a' before these).
  */
  for (table= sql_lock->table + a->table_count,
         end_table= table + b->table_count;
       table < end_table;
       table++)
  {
    (*table)->lock_position+=   a->table_count;
    (*table)->lock_data_start+= a->lock_count;
  }

  /*
    Ensure that locks of the same tables share same data structures if we
    reopen a table that is already open. This can happen for example with
    MERGE tables.
  */

  /* Copy the lock data array. thr_merge_lock() reorders its content */
  memcpy(sql_lock->locks + sql_lock->lock_count, sql_lock->locks,
         sql_lock->lock_count * sizeof(*sql_lock->locks));
  thr_merge_locks(sql_lock->locks + sql_lock->lock_count,
                  a->lock_count, b->lock_count);

  /* Delete old, not needed locks */
  my_free(a);
  my_free(b);
  DBUG_RETURN(sql_lock);
}


/** Unlock a set of external. */

static int unlock_external(THD *thd, TABLE **table,uint count)
{
  int error,error_code;
  DBUG_ENTER("unlock_external");

  error_code=0;
  do
  {
    if ((*table)->current_lock != F_UNLCK)
    {
      (*table)->current_lock = F_UNLCK;
      if ((error=(*table)->file->ha_external_lock(thd, F_UNLCK)))
      {
	error_code=error;
	print_lock_error(error_code, (*table)->file->table_type());
      }
    }
    table++;
  } while (--count);
  DBUG_RETURN(error_code);
}


/**
  Get lock structures from table structs and initialize locks.

  @param thd		    Thread handler
  @param table_ptr	    Pointer to tables that should be locks
  @param flags		    One of:
           - GET_LOCK_UNLOCK      : If we should send TL_IGNORE to store lock
           - GET_LOCK_STORE_LOCKS : Store lock info in TABLE
*/

MYSQL_LOCK *get_lock_data(THD *thd, TABLE **table_ptr, uint count, uint flags)
{
  uint i,tables,lock_count;
  MYSQL_LOCK *sql_lock;
  THR_LOCK_DATA **locks, **locks_buf;
  TABLE **to, **table_buf;
  DBUG_ENTER("get_lock_data");

  DBUG_ASSERT((flags == GET_LOCK_UNLOCK) || (flags == GET_LOCK_STORE_LOCKS));
  DBUG_PRINT("info", ("count %d", count));

  for (i=tables=lock_count=0 ; i < count ; i++)
  {
    TABLE *t= table_ptr[i];
    
    
    if (t->s->tmp_table != NON_TRANSACTIONAL_TMP_TABLE && 
        t->s->tmp_table != INTERNAL_TMP_TABLE)
    {
      tables+= t->file->lock_count();
      lock_count++;
    }
  }

  /*
    Allocating twice the number of pointers for lock data for use in
    thr_multi_lock(). This function reorders the lock data, but cannot
    update the table values. So the second part of the array is copied
    from the first part immediately before calling thr_multi_lock().
  */
  if (!(sql_lock= (MYSQL_LOCK*)
	my_malloc(sizeof(*sql_lock) +
		  sizeof(THR_LOCK_DATA*) * tables * 2 +
                  sizeof(table_ptr) * lock_count,
		  MYF(0))))
    DBUG_RETURN(0);
  locks= locks_buf= sql_lock->locks= (THR_LOCK_DATA**) (sql_lock + 1);
  to= table_buf= sql_lock->table= (TABLE**) (locks + tables * 2);
  sql_lock->table_count=lock_count;

  for (i=0 ; i < count ; i++)
  {
    TABLE *table;
    enum thr_lock_type lock_type;
    THR_LOCK_DATA **locks_start;
    table= table_ptr[i];
    if (table->s->tmp_table == NON_TRANSACTIONAL_TMP_TABLE ||
        table->s->tmp_table == INTERNAL_TMP_TABLE) 
      continue;
    lock_type= table->reginfo.lock_type;
    DBUG_ASSERT(lock_type != TL_WRITE_DEFAULT && lock_type != TL_READ_DEFAULT);
    locks_start= locks;
    locks= table->file->store_lock(thd, locks,
                                   (flags & GET_LOCK_UNLOCK) ? TL_IGNORE :
                                   lock_type);
    if (flags & GET_LOCK_STORE_LOCKS)
    {
      table->lock_position=   (uint) (to - table_buf);
      table->lock_data_start= (uint) (locks_start - locks_buf);
      table->lock_count=      (uint) (locks - locks_start);
    }
    *to++= table;
    if (locks)
    {
      for ( ; locks_start != locks ; locks_start++)
      {
	(*locks_start)->debug_print_param= (void *) table;
	(*locks_start)->lock->name=         table->alias.c_ptr();
	(*locks_start)->org_type=           (*locks_start)->type;
      }
    }
  }
  /*
    We do not use 'tables', because there are cases where store_lock()
    returns less locks than lock_count() claimed. This can happen when
    a FLUSH TABLES tries to abort locks from a MERGE table of another
    thread. When that thread has just opened the table, but not yet
    attached its children, it cannot return the locks. lock_count()
    always returns the number of locks that an attached table has.
    This is done to avoid the reverse situation: If lock_count() would
    return 0 for a non-attached MERGE table, and that table becomes
    attached between the calls to lock_count() and store_lock(), then
    we would have allocated too little memory for the lock data. Now
    we may allocate too much, but better safe than memory overrun.
    And in the FLUSH case, the memory is released quickly anyway.
  */
  sql_lock->lock_count= locks - locks_buf;
  DBUG_PRINT("info", ("sql_lock->table_count %d sql_lock->lock_count %d",
                      sql_lock->table_count, sql_lock->lock_count));
  DBUG_RETURN(sql_lock);
}


/**
  Obtain an exclusive metadata lock on a schema name.

  @param thd         Thread handle.
  @param db          The database name.

  This function cannot be called while holding LOCK_open mutex.
  To avoid deadlocks, we do not try to obtain exclusive metadata
  locks in LOCK TABLES mode, since in this mode there may be
  other metadata locks already taken by the current connection,
  and we must not wait for MDL locks while holding locks.

  @retval FALSE  Success.
  @retval TRUE   Failure: we're in LOCK TABLES mode, or out of memory,
                 or this connection was killed.
*/

bool lock_schema_name(THD *thd, const char *db)
{
  MDL_request_list mdl_requests;
  MDL_request global_request;
  MDL_request mdl_request;

  if (thd->locked_tables_mode)
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    return TRUE;
  }

  if (thd->global_read_lock.can_acquire_protection())
    return TRUE;
  global_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                      MDL_STATEMENT);
  mdl_request.init(MDL_key::SCHEMA, db, "", MDL_EXCLUSIVE, MDL_TRANSACTION);

  mdl_requests.push_front(&mdl_request);
  mdl_requests.push_front(&global_request);

  if (thd->mdl_context.acquire_locks(&mdl_requests,
                                     thd->variables.lock_wait_timeout))
    return TRUE;

  DEBUG_SYNC(thd, "after_wait_locked_schema_name");
  return FALSE;
}


/**
  Obtain an exclusive metadata lock on an object name.

  @param thd         Thread handle.
  @param mdl_type    Object type (currently functions, procedures
                     and events can be name-locked).
  @param db          The schema the object belongs to.
  @param name        Object name in the schema.

  This function assumes that no metadata locks were acquired
  before calling it. Additionally, it cannot be called while
  holding LOCK_open mutex. Both these invariants are enforced by
  asserts in MDL_context::acquire_locks().
  To avoid deadlocks, we do not try to obtain exclusive metadata
  locks in LOCK TABLES mode, since in this mode there may be
  other metadata locks already taken by the current connection,
  and we must not wait for MDL locks while holding locks.

  @retval FALSE  Success.
  @retval TRUE   Failure: we're in LOCK TABLES mode, or out of memory,
                 or this connection was killed.
*/

bool lock_object_name(THD *thd, MDL_key::enum_mdl_namespace mdl_type,
                       const char *db, const char *name)
{
  MDL_request_list mdl_requests;
  MDL_request global_request;
  MDL_request schema_request;
  MDL_request mdl_request;

  if (thd->locked_tables_mode)
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    return TRUE;
  }

  DBUG_ASSERT(name);
  DEBUG_SYNC(thd, "before_wait_locked_pname");

  if (thd->global_read_lock.can_acquire_protection())
    return TRUE;
  global_request.init(MDL_key::GLOBAL, "", "", MDL_INTENTION_EXCLUSIVE,
                      MDL_STATEMENT);
  schema_request.init(MDL_key::SCHEMA, db, "", MDL_INTENTION_EXCLUSIVE,
                      MDL_TRANSACTION);
  mdl_request.init(mdl_type, db, name, MDL_EXCLUSIVE, MDL_TRANSACTION);

  mdl_requests.push_front(&mdl_request);
  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&global_request);

  if (thd->mdl_context.acquire_locks(&mdl_requests,
                                     thd->variables.lock_wait_timeout))
    return TRUE;

  DEBUG_SYNC(thd, "after_wait_locked_pname");
  return FALSE;
}


static void print_lock_error(int error, const char *table)
{
  int textno;
  DBUG_ENTER("print_lock_error");

  switch (error) {
  case HA_ERR_LOCK_WAIT_TIMEOUT:
    textno=ER_LOCK_WAIT_TIMEOUT;
    break;
  case HA_ERR_READ_ONLY_TRANSACTION:
    textno=ER_READ_ONLY_TRANSACTION;
    break;
  case HA_ERR_LOCK_DEADLOCK:
    textno=ER_LOCK_DEADLOCK;
    break;
  case HA_ERR_WRONG_COMMAND:
    textno=ER_ILLEGAL_HA;
    break;
  default:
    textno=ER_CANT_LOCK;
    break;
  }

  if ( textno == ER_ILLEGAL_HA )
    my_error(textno, MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG), table);
  else
    my_error(textno, MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG), error);

  DBUG_VOID_RETURN;
}


/****************************************************************************
  Handling of global read locks

  Global read lock is implemented using metadata lock infrastructure.

  Taking the global read lock is TWO steps (2nd step is optional; without
  it, COMMIT of existing transactions will be allowed):
  lock_global_read_lock() THEN make_global_read_lock_block_commit().

  How blocking of threads by global read lock is achieved: that's
  semi-automatic. We assume that any statement which should be blocked
  by global read lock will either open and acquires write-lock on tables
  or acquires metadata locks on objects it is going to modify. For any
  such statement global IX metadata lock is automatically acquired for
  its duration (in case of LOCK TABLES until end of LOCK TABLES mode).
  And lock_global_read_lock() simply acquires global S metadata lock
  and thus prohibits execution of statements which modify data (unless
  they modify only temporary tables). If deadlock happens it is detected
  by MDL subsystem and resolved in the standard fashion (by backing-off
  metadata locks acquired so far and restarting open tables process
  if possible).

  Why does FLUSH TABLES WITH READ LOCK need to block COMMIT: because it's used
  to read a non-moving SHOW MASTER STATUS, and a COMMIT writes to the binary
  log.

  Why getting the global read lock is two steps and not one. Because FLUSH
  TABLES WITH READ LOCK needs to insert one other step between the two:
  flushing tables. So the order is
  1) lock_global_read_lock() (prevents any new table write locks, i.e. stalls
  all new updates)
  2) close_cached_tables() (the FLUSH TABLES), which will wait for tables
  currently opened and being updated to close (so it's possible that there is
  a moment where all new updates of server are stalled *and* FLUSH TABLES WITH
  READ LOCK is, too).
  3) make_global_read_lock_block_commit().
  If we have merged 1) and 3) into 1), we would have had this deadlock:
  imagine thread 1 and 2, in non-autocommit mode, thread 3, and an InnoDB
  table t.
  thd1: SELECT * FROM t FOR UPDATE;
  thd2: UPDATE t SET a=1; # blocked by row-level locks of thd1
  thd3: FLUSH TABLES WITH READ LOCK; # blocked in close_cached_tables() by the
  table instance of thd2
  thd1: COMMIT; # blocked by thd3.
  thd1 blocks thd2 which blocks thd3 which blocks thd1: deadlock.

  Note that we need to support that one thread does
  FLUSH TABLES WITH READ LOCK; and then COMMIT;
  (that's what innobackup does, for some good reason).
  So in this exceptional case the COMMIT should not be blocked by the FLUSH
  TABLES WITH READ LOCK.

****************************************************************************/

/**
  Take global read lock, wait if there is protection against lock.

  If the global read lock is already taken by this thread, then nothing is done.

  See also "Handling of global read locks" above.

  @param thd     Reference to thread.

  @retval False  Success, global read lock set, commits are NOT blocked.
  @retval True   Failure, thread was killed.
*/

bool Global_read_lock::lock_global_read_lock(THD *thd)
{
  DBUG_ENTER("lock_global_read_lock");

  if (!m_state)
  {
    MDL_request mdl_request;

    DBUG_ASSERT(! thd->mdl_context.is_lock_owner(MDL_key::GLOBAL, "", "",
                                                 MDL_SHARED));
    mdl_request.init(MDL_key::GLOBAL, "", "", MDL_SHARED, MDL_EXPLICIT);

    if (thd->mdl_context.acquire_lock(&mdl_request,
                                      thd->variables.lock_wait_timeout))
      DBUG_RETURN(1);

    m_mdl_global_shared_lock= mdl_request.ticket;
    m_state= GRL_ACQUIRED;
  }
  /*
    We DON'T set global_read_lock_blocks_commit now, it will be set after
    tables are flushed (as the present function serves for FLUSH TABLES WITH
    READ LOCK only). Doing things in this order is necessary to avoid
    deadlocks (we must allow COMMIT until all tables are closed; we should not
    forbid it before, or we can have a 3-thread deadlock if 2 do SELECT FOR
    UPDATE and one does FLUSH TABLES WITH READ LOCK).
  */
  DBUG_RETURN(0);
}


/**
  Unlock global read lock.

  Commits may or may not be blocked when this function is called.

  See also "Handling of global read locks" above.

  @param thd    Reference to thread.
*/

void Global_read_lock::unlock_global_read_lock(THD *thd)
{
  DBUG_ENTER("unlock_global_read_lock");

  DBUG_ASSERT(m_mdl_global_shared_lock && m_state);

  if (thd->global_disable_checkpoint)
  {
    thd->global_disable_checkpoint= 0;
    if (!--global_disable_checkpoint)
    {
      ha_checkpoint_state(0);                   // Enable checkpoints
    }
  }

  if (m_mdl_blocks_commits_lock)
  {
    thd->mdl_context.release_lock(m_mdl_blocks_commits_lock);
    m_mdl_blocks_commits_lock= NULL;
  }
  thd->mdl_context.release_lock(m_mdl_global_shared_lock);
  m_mdl_global_shared_lock= NULL;
  m_state= GRL_NONE;

  DBUG_VOID_RETURN;
}


/**
  Make global read lock also block commits.

  The scenario is:
   - This thread has the global read lock.
   - Global read lock blocking of commits is not set.

  See also "Handling of global read locks" above.

  @param thd     Reference to thread.

  @retval False  Success, global read lock set, commits are blocked.
  @retval True   Failure, thread was killed.
*/

bool Global_read_lock::make_global_read_lock_block_commit(THD *thd)
{
  MDL_request mdl_request;
  DBUG_ENTER("make_global_read_lock_block_commit");
  /*
    If we didn't succeed lock_global_read_lock(), or if we already suceeded
    make_global_read_lock_block_commit(), do nothing.
  */
  if (m_state != GRL_ACQUIRED)
    DBUG_RETURN(0);

  mdl_request.init(MDL_key::COMMIT, "", "", MDL_SHARED, MDL_EXPLICIT);

  if (thd->mdl_context.acquire_lock(&mdl_request,
                                    thd->variables.lock_wait_timeout))
    DBUG_RETURN(TRUE);

  m_mdl_blocks_commits_lock= mdl_request.ticket;
  m_state= GRL_ACQUIRED_AND_BLOCKS_COMMIT;

  DBUG_RETURN(FALSE);
}


/**
  Set explicit duration for metadata locks which are used to implement GRL.

  @param thd     Reference to thread.
*/

void Global_read_lock::set_explicit_lock_duration(THD *thd)
{
  if (m_mdl_global_shared_lock)
    thd->mdl_context.set_lock_duration(m_mdl_global_shared_lock, MDL_EXPLICIT);
  if (m_mdl_blocks_commits_lock)
    thd->mdl_context.set_lock_duration(m_mdl_blocks_commits_lock, MDL_EXPLICIT);
}

/**
  @} (end of group Locking)
*/
