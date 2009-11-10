/* Copyright 2000-2008 MySQL AB, 2008 Sun Microsystems, Inc.

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

#include "mysql_priv.h"
#include <hash.h>
#include <assert.h>

/**
  @defgroup Locking Locking
  @{
*/

extern HASH open_cache;

/* flags for get_lock_data */
#define GET_LOCK_UNLOCK         1
#define GET_LOCK_STORE_LOCKS    2

static MYSQL_LOCK *get_lock_data(THD *thd, TABLE **table,uint count,
				 uint flags, TABLE **write_locked);
static void reset_lock_data(MYSQL_LOCK *sql_lock);
static int lock_external(THD *thd, TABLE **table,uint count);
static int unlock_external(THD *thd, TABLE **table,uint count);
static void print_lock_error(int error, const char *);

/*
  Lock tables.

  SYNOPSIS
    mysql_lock_tables()
    thd                         The current thread.
    tables                      An array of pointers to the tables to lock.
    count                       The number of tables to lock.
    flags                       Options:
      MYSQL_LOCK_IGNORE_GLOBAL_READ_LOCK      Ignore a global read lock
      MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY      Ignore SET GLOBAL READ_ONLY
      MYSQL_LOCK_IGNORE_FLUSH                 Ignore a flush tables.
      MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN        Instead of reopening altered
                                              or dropped tables by itself,
                                              mysql_lock_tables() should
                                              notify upper level and rely
                                              on caller doing this.
    need_reopen                 Out parameter, TRUE if some tables were altered
                                or deleted and should be reopened by caller.

  RETURN
    A lock structure pointer on success.
    NULL on error or if some tables should be reopen.
*/

/* Map the return value of thr_lock to an error from errmsg.txt */
static int thr_lock_errno_to_mysql[]=
{ 0, 1, ER_LOCK_WAIT_TIMEOUT, ER_LOCK_DEADLOCK };

/**
  Perform semantic checks for mysql_lock_tables.
  @param thd The current thread
  @param tables The tables to lock
  @param count The number of tables to lock
  @param flags Lock flags
  @return 0 if all the check passed, non zero if a check failed.
*/
int mysql_lock_tables_check(THD *thd, TABLE **tables, uint count, uint flags)
{
  bool log_table_write_query;
  uint system_count;
  uint i;

  DBUG_ENTER("mysql_lock_tables_check");

  system_count= 0;
  log_table_write_query= (is_log_table_write_query(thd->lex->sql_command)
                         || ((flags & MYSQL_LOCK_PERF_SCHEMA) != 0));

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

    if ((t->s->table_category == TABLE_CATEGORY_SYSTEM) &&
        (t->reginfo.lock_type >= TL_WRITE_ALLOW_WRITE))
    {
      system_count++;
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

MYSQL_LOCK *mysql_lock_tables(THD *thd, TABLE **tables, uint count,
                              uint flags, bool *need_reopen)
{
  MYSQL_LOCK *sql_lock;
  TABLE *write_lock_used;
  int rc;

  DBUG_ENTER("mysql_lock_tables");

  *need_reopen= FALSE;

  if (mysql_lock_tables_check(thd, tables, count, flags))
    DBUG_RETURN (NULL);

  for (;;)
  {
    if (! (sql_lock= get_lock_data(thd, tables, count, GET_LOCK_STORE_LOCKS,
                                   &write_lock_used)))
      break;

    if (global_read_lock && write_lock_used &&
        ! (flags & MYSQL_LOCK_IGNORE_GLOBAL_READ_LOCK))
    {
      /*
	Someone has issued LOCK ALL TABLES FOR READ and we want a write lock
	Wait until the lock is gone
      */
      if (wait_if_global_read_lock(thd, 1, 1))
      {
        /* Clear the lock type of all lock data to avoid reusage. */
        reset_lock_data(sql_lock);
	my_free((uchar*) sql_lock,MYF(0));
	sql_lock=0;
	break;
      }
      if (thd->version != refresh_version)
      {
        /* Clear the lock type of all lock data to avoid reusage. */
        reset_lock_data(sql_lock);
	my_free((uchar*) sql_lock,MYF(0));
	goto retry;
      }
    }

    if (!(flags & MYSQL_LOCK_IGNORE_GLOBAL_READ_ONLY) &&
        write_lock_used &&
        opt_readonly &&
        !(thd->security_ctx->master_access & SUPER_ACL) &&
        !thd->slave_thread)
    {
      /*
	Someone has issued SET GLOBAL READ_ONLY=1 and we want a write lock.
        We do not wait for READ_ONLY=0, and fail.
      */
      reset_lock_data(sql_lock);
      my_free((uchar*) sql_lock, MYF(0));
      sql_lock=0;
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
      break;
    }

    thd_proc_info(thd, "System lock");
    DBUG_PRINT("info", ("thd->proc_info %s", thd->proc_info));
    if (sql_lock->table_count && lock_external(thd, sql_lock->table,
                                               sql_lock->table_count))
    {
      /* Clear the lock type of all lock data to avoid reusage. */
      reset_lock_data(sql_lock);
      my_free((uchar*) sql_lock,MYF(0));
      sql_lock=0;
      break;
    }
    thd_proc_info(thd, "Table lock");
    DBUG_PRINT("info", ("thd->proc_info %s", thd->proc_info));
    thd->locked=1;
    /* Copy the lock data array. thr_multi_lock() reorders its contens. */
    memcpy(sql_lock->locks + sql_lock->lock_count, sql_lock->locks,
           sql_lock->lock_count * sizeof(*sql_lock->locks));
    /* Lock on the copied half of the lock data array. */
    rc= thr_lock_errno_to_mysql[(int) thr_multi_lock(sql_lock->locks +
                                                     sql_lock->lock_count,
                                                     sql_lock->lock_count,
                                                     thd->lock_id)];
    if (rc > 1)                                 /* a timeout or a deadlock */
    {
      if (sql_lock->table_count)
        VOID(unlock_external(thd, sql_lock->table, sql_lock->table_count));
      my_error(rc, MYF(0));
      my_free((uchar*) sql_lock,MYF(0));
      sql_lock= 0;
      break;
    }
    else if (rc == 1)                           /* aborted */
    {
      /*
        reset_lock_data is required here. If thr_multi_lock fails it
        resets lock type for tables, which were locked before (and
        including) one that caused error. Lock type for other tables
        preserved.
      */
      reset_lock_data(sql_lock);
      thd->some_tables_deleted=1;		// Try again
      sql_lock->lock_count= 0;                  // Locks are already freed
    }
    else if (!thd->some_tables_deleted || (flags & MYSQL_LOCK_IGNORE_FLUSH))
    {
      /*
        Thread was killed or lock aborted. Let upper level close all
        used tables and retry or give error.
      */
      thd->locked=0;
      break;
    }
    else if (!thd->open_tables)
    {
      // Only using temporary tables, no need to unlock
      thd->some_tables_deleted=0;
      thd->locked=0;
      break;
    }
    thd_proc_info(thd, 0);

    /* some table was altered or deleted. reopen tables marked deleted */
    mysql_unlock_tables(thd,sql_lock);
    thd->locked=0;
retry:
    sql_lock=0;
    if (flags & MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN)
    {
      *need_reopen= TRUE;
      break;
    }
    if (wait_for_tables(thd))
      break;					// Couldn't open tables
  }
  thd_proc_info(thd, 0);
  if (thd->killed)
  {
    thd->send_kill_message();
    if (sql_lock)
    {
      mysql_unlock_tables(thd,sql_lock);
      sql_lock=0;
    }
  }

  thd->set_time_after_lock();
  DBUG_RETURN (sql_lock);
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


void mysql_unlock_tables(THD *thd, MYSQL_LOCK *sql_lock)
{
  DBUG_ENTER("mysql_unlock_tables");
  if (sql_lock->table_count)
    VOID(unlock_external(thd,sql_lock->table,sql_lock->table_count));
  if (sql_lock->lock_count)
    thr_multi_unlock(sql_lock->locks,sql_lock->lock_count);
  my_free((uchar*) sql_lock,MYF(0));
  DBUG_VOID_RETURN;
}

/**
  Unlock some of the tables locked by mysql_lock_tables.

  This will work even if get_lock_data fails (next unlock will free all)
*/

void mysql_unlock_some_tables(THD *thd, TABLE **table,uint count)
{
  MYSQL_LOCK *sql_lock;
  TABLE *write_lock_used;
  if ((sql_lock= get_lock_data(thd, table, count, GET_LOCK_UNLOCK,
                               &write_lock_used)))
    mysql_unlock_tables(thd, sql_lock);
}


/**
  unlock all tables locked for read.
*/

void mysql_unlock_read_tables(THD *thd, MYSQL_LOCK *sql_lock)
{
  uint i,found;
  DBUG_ENTER("mysql_unlock_read_tables");

  /* Move all write locks first */
  THR_LOCK_DATA **lock=sql_lock->locks;
  for (i=found=0 ; i < sql_lock->lock_count ; i++)
  {
    if (sql_lock->locks[i]->type >= TL_WRITE_ALLOW_READ)
    {
      swap_variables(THR_LOCK_DATA *, *lock, sql_lock->locks[i]);
      lock++;
      found++;
    }
  }
  /* unlock the read locked tables */
  if (i != found)
  {
    thr_multi_unlock(lock,i-found);
    sql_lock->lock_count= found;
  }

  /* Then do the same for the external locks */
  /* Move all write locked tables first */
  TABLE **table=sql_lock->table;
  for (i=found=0 ; i < sql_lock->table_count ; i++)
  {
    DBUG_ASSERT(sql_lock->table[i]->lock_position == i);
    if ((uint) sql_lock->table[i]->reginfo.lock_type >= TL_WRITE_ALLOW_READ)
    {
      swap_variables(TABLE *, *table, sql_lock->table[i]);
      table++;
      found++;
    }
  }
  /* Unlock all read locked tables */
  if (i != found)
  {
    VOID(unlock_external(thd,table,i-found));
    sql_lock->table_count=found;
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

  @note This function has a legacy side effect: the table is
  unlocked even if it is not found in the locked list.
  It's not clear if this side effect is intentional or still
  desirable. It might lead to unmatched calls to
  unlock_external(). Moreover, a discrepancy can be left
  unnoticed by the storage engine, because in
  unlock_external() we call handler::external_lock(F_UNLCK) only
  if table->current_lock is not F_UNLCK.

  @param  thd             thread context
  @param  locked          list of locked tables
  @param  table           the table to unlock
  @param  always_unlock   specify explicitly if the legacy side
                          effect is desired.
*/

void mysql_lock_remove(THD *thd, MYSQL_LOCK *locked,TABLE *table,
                       bool always_unlock)
{
  if (always_unlock == TRUE)
    mysql_unlock_some_tables(thd, &table, /* table count */ 1);
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

        /* Unlock if not yet unlocked */
        if (always_unlock == FALSE)
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

/* Downgrade all locks on a table to new WRITE level from WRITE_ONLY */

void mysql_lock_downgrade_write(THD *thd, TABLE *table,
                                thr_lock_type new_lock_type)
{
  MYSQL_LOCK *locked;
  TABLE *write_lock_used;
  if ((locked = get_lock_data(thd, &table, 1, GET_LOCK_UNLOCK,
                              &write_lock_used)))
  {
    for (uint i=0; i < locked->lock_count; i++)
      thr_downgrade_write_lock(locked->locks[i], new_lock_type);
    my_free((uchar*) locked,MYF(0));
  }
}


/** Abort all other threads waiting to get lock in table. */

void mysql_lock_abort(THD *thd, TABLE *table, bool upgrade_lock)
{
  MYSQL_LOCK *locked;
  TABLE *write_lock_used;
  DBUG_ENTER("mysql_lock_abort");

  if ((locked= get_lock_data(thd, &table, 1, GET_LOCK_UNLOCK,
                             &write_lock_used)))
  {
    for (uint i=0; i < locked->lock_count; i++)
      thr_abort_locks(locked->locks[i]->lock, upgrade_lock);
    my_free((uchar*) locked,MYF(0));
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
  TABLE *write_lock_used;
  bool result= FALSE;
  DBUG_ENTER("mysql_lock_abort_for_thread");

  if ((locked= get_lock_data(thd, &table, 1, GET_LOCK_UNLOCK,
                             &write_lock_used)))
  {
    for (uint i=0; i < locked->lock_count; i++)
    {
      if (thr_abort_locks_for_thread(locked->locks[i]->lock,
                                     table->in_use->thread_id))
        result= TRUE;
    }
    my_free((uchar*) locked,MYF(0));
  }
  DBUG_RETURN(result);
}


MYSQL_LOCK *mysql_lock_merge(MYSQL_LOCK *a,MYSQL_LOCK *b)
{
  MYSQL_LOCK *sql_lock;
  TABLE **table, **end_table;
  DBUG_ENTER("mysql_lock_merge");

  if (!(sql_lock= (MYSQL_LOCK*)
	my_malloc(sizeof(*sql_lock)+
		  sizeof(THR_LOCK_DATA*)*(a->lock_count+b->lock_count)+
		  sizeof(TABLE*)*(a->table_count+b->table_count),MYF(MY_WME))))
    DBUG_RETURN(0);				// Fatal error
  sql_lock->lock_count=a->lock_count+b->lock_count;
  sql_lock->table_count=a->table_count+b->table_count;
  sql_lock->locks=(THR_LOCK_DATA**) (sql_lock+1);
  sql_lock->table=(TABLE**) (sql_lock->locks+sql_lock->lock_count);
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

  /* Delete old, not needed locks */
  my_free((uchar*) a,MYF(0));
  my_free((uchar*) b,MYF(0));
  DBUG_RETURN(sql_lock);
}


/**
  Find duplicate lock in tables.

  Temporary tables are ignored here like they are ignored in
  get_lock_data(). If we allow two opens on temporary tables later,
  both functions should be checked.

  @param thd                 The current thread.
  @param needle              The table to check for duplicate lock.
  @param haystack            The list of tables to search for the dup lock.

  @note
    This is mainly meant for MERGE tables in INSERT ... SELECT
    situations. The 'real', underlying tables can be found only after
    the MERGE tables are opened. This function assumes that the tables are
    already locked.

  @retval
    NULL    No duplicate lock found.
  @retval
    !NULL   First table from 'haystack' that matches a lock on 'needle'.
*/

TABLE_LIST *mysql_lock_have_duplicate(THD *thd, TABLE_LIST *needle,
                                      TABLE_LIST *haystack)
{
  MYSQL_LOCK            *mylock;
  TABLE                 **lock_tables;
  TABLE                 *table;
  TABLE                 *table2;
  THR_LOCK_DATA         **lock_locks;
  THR_LOCK_DATA         **table_lock_data;
  THR_LOCK_DATA         **end_data;
  THR_LOCK_DATA         **lock_data2;
  THR_LOCK_DATA         **end_data2;
  DBUG_ENTER("mysql_lock_have_duplicate");

  /*
    Table may not be defined for derived or view tables.
    Table may not be part of a lock for delayed operations.
  */
  if (! (table= needle->table) || ! table->lock_count)
    goto end;

  /* A temporary table does not have locks. */
  if (table->s->tmp_table == NON_TRANSACTIONAL_TMP_TABLE)
    goto end;

  /* Get command lock or LOCK TABLES lock. Maybe empty for INSERT DELAYED. */
  if (! (mylock= thd->lock ? thd->lock : thd->locked_tables))
    goto end;

  /* If we have less than two tables, we cannot have duplicates. */
  if (mylock->table_count < 2)
    goto end;

  lock_locks=  mylock->locks;
  lock_tables= mylock->table;

  /* Prepare table related variables that don't change in loop. */
  DBUG_ASSERT((table->lock_position < mylock->table_count) &&
              (table == lock_tables[table->lock_position]));
  table_lock_data= lock_locks + table->lock_data_start;
  end_data= table_lock_data + table->lock_count;

  for (; haystack; haystack= haystack->next_global)
  {
    if (haystack->placeholder())
      continue;
    table2= haystack->table;
    if (table2->s->tmp_table == NON_TRANSACTIONAL_TMP_TABLE)
      continue;

    /* All tables in list must be in lock. */
    DBUG_ASSERT((table2->lock_position < mylock->table_count) &&
                (table2 == lock_tables[table2->lock_position]));

    for (lock_data2=  lock_locks + table2->lock_data_start,
           end_data2= lock_data2 + table2->lock_count;
         lock_data2 < end_data2;
         lock_data2++)
    {
      THR_LOCK_DATA **lock_data;
      THR_LOCK *lock2= (*lock_data2)->lock;

      for (lock_data= table_lock_data;
           lock_data < end_data;
           lock_data++)
      {
        if ((*lock_data)->lock == lock2)
        {
          DBUG_PRINT("info", ("haystack match: '%s'", haystack->table_name));
          DBUG_RETURN(haystack);
        }
      }
    }
  }

 end:
  DBUG_PRINT("info", ("no duplicate found"));
  DBUG_RETURN(NULL);
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
  @param write_lock_used   Store pointer to last table with WRITE_ALLOW_WRITE
*/

static MYSQL_LOCK *get_lock_data(THD *thd, TABLE **table_ptr, uint count,
				 uint flags, TABLE **write_lock_used)
{
  uint i,tables,lock_count;
  MYSQL_LOCK *sql_lock;
  THR_LOCK_DATA **locks, **locks_buf, **locks_start;
  TABLE **to, **table_buf;
  DBUG_ENTER("get_lock_data");

  DBUG_ASSERT((flags == GET_LOCK_UNLOCK) || (flags == GET_LOCK_STORE_LOCKS));

  DBUG_PRINT("info", ("count %d", count));
  *write_lock_used=0;
  for (i=tables=lock_count=0 ; i < count ; i++)
  {
    TABLE *t= table_ptr[i];

    if (t->s->tmp_table != NON_TRANSACTIONAL_TMP_TABLE)
    {
      tables+= t->file->lock_count();
      lock_count++;
    }
  }

  /*
    Allocating twice the number of pointers for lock data for use in
    thr_mulit_lock(). This function reorders the lock data, but cannot
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

    if ((table=table_ptr[i])->s->tmp_table == NON_TRANSACTIONAL_TMP_TABLE)
      continue;
    lock_type= table->reginfo.lock_type;
    DBUG_ASSERT(lock_type != TL_WRITE_DEFAULT && lock_type != TL_READ_DEFAULT);
    if (lock_type >= TL_WRITE_ALLOW_WRITE)
    {
      *write_lock_used=table;
      if (table->db_stat & HA_READ_ONLY)
      {
	my_error(ER_OPEN_AS_READONLY,MYF(0),table->alias);
        /* Clear the lock type of the lock data that are stored already. */
        sql_lock->lock_count= (uint) (locks - sql_lock->locks);
        reset_lock_data(sql_lock);
	my_free((uchar*) sql_lock,MYF(0));
	DBUG_RETURN(0);
      }
    }
    THR_LOCK_DATA **org_locks = locks;
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
      for ( ; org_locks != locks ; org_locks++)
	(*org_locks)->debug_print_param= (void *) table;
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
  Reset lock type in lock data.

  After a locking error we want to quit the locking of the table(s).
  The test case in the bug report for Bug #18544 has the following
  cases:
  -# Locking error in lock_external() due to InnoDB timeout.
  -# Locking error in get_lock_data() due to missing write permission.
  -# Locking error in wait_if_global_read_lock() due to lock conflict.

  In all these cases we have already set the lock type into the lock
  data of the open table(s). If the table(s) are in the open table
  cache, they could be reused with the non-zero lock type set. This
  could lead to ignoring a different lock type with the next lock.

  Clear the lock type of all lock data. This ensures that the next
  lock request will set its lock type properly.

  @param sql_lock                  The MySQL lock.
*/

static void reset_lock_data(MYSQL_LOCK *sql_lock)
{
  THR_LOCK_DATA **ldata;
  THR_LOCK_DATA **ldata_end;

  for (ldata= sql_lock->locks, ldata_end= ldata + sql_lock->lock_count;
       ldata < ldata_end;
       ldata++)
  {
    /* Reset lock type. */
    (*ldata)->type= TL_UNLOCK;
  }
}


/*****************************************************************************
  Lock table based on the name.
  This is used when we need total access to a closed, not open table
*****************************************************************************/

/**
  Lock and wait for the named lock.

  @param thd			Thread handler
  @param table_list		Lock first table in this list


  @note
    Works together with global read lock.

  @retval
    0	ok
  @retval
    1	error
*/

int lock_and_wait_for_table_name(THD *thd, TABLE_LIST *table_list)
{
  int lock_retcode;
  int error= -1;
  DBUG_ENTER("lock_and_wait_for_table_name");

  if (wait_if_global_read_lock(thd, 0, 1))
    DBUG_RETURN(1);
  VOID(pthread_mutex_lock(&LOCK_open));
  if ((lock_retcode = lock_table_name(thd, table_list, TRUE)) < 0)
    goto end;
  if (lock_retcode && wait_for_locked_table_names(thd, table_list))
  {
    unlock_table_name(thd, table_list);
    goto end;
  }
  error=0;

end:
  pthread_mutex_unlock(&LOCK_open);
  start_waiting_global_read_lock(thd);
  DBUG_RETURN(error);
}


/**
  Put a not open table with an old refresh version in the table cache.

  @param thd			Thread handler
  @param table_list		Lock first table in this list
  @param check_in_use           Do we need to check if table already in use by us

  @note
    One must have a lock on LOCK_open!

  @warning
    If you are going to update the table, you should use
    lock_and_wait_for_table_name instead of this function as this works
    together with 'FLUSH TABLES WITH READ LOCK'

  @note
    This will force any other threads that uses the table to release it
    as soon as possible.

  @return
    < 0 error
  @return
    == 0 table locked
  @return
    > 0  table locked, but someone is using it
*/

int lock_table_name(THD *thd, TABLE_LIST *table_list, bool check_in_use)
{
  TABLE *table;
  char  key[MAX_DBKEY_LENGTH];
  char *db= table_list->db;
  uint  key_length;
  HASH_SEARCH_STATE state;
  DBUG_ENTER("lock_table_name");
  DBUG_PRINT("enter",("db: %s  name: %s", db, table_list->table_name));

  key_length= create_table_def_key(thd, key, table_list, 0);

  if (check_in_use)
  {
    /* Only insert the table if we haven't insert it already */
    for (table=(TABLE*) hash_first(&open_cache, (uchar*)key,
                                   key_length, &state);
         table ;
         table = (TABLE*) hash_next(&open_cache,(uchar*) key,
                                    key_length, &state))
    {
      if (table->in_use == thd)
      {
        DBUG_PRINT("info", ("Table is in use"));
        table->s->version= 0;                  // Ensure no one can use this
        table->locked_by_name= 1;
        DBUG_RETURN(0);
      }
    }
  }

  if (!(table= table_cache_insert_placeholder(thd, key, key_length)))
    DBUG_RETURN(-1);

  table_list->table=table;

  /* Return 1 if table is in use */
  DBUG_RETURN(test(remove_table_from_cache(thd, db, table_list->table_name,
             check_in_use ? RTFC_NO_FLAG : RTFC_WAIT_OTHER_THREAD_FLAG)));
}


void unlock_table_name(THD *thd, TABLE_LIST *table_list)
{
  if (table_list->table)
  {
    hash_delete(&open_cache, (uchar*) table_list->table);
    broadcast_refresh();
  }
}


static bool locked_named_table(THD *thd, TABLE_LIST *table_list)
{
  for (; table_list ; table_list=table_list->next_local)
  {
    TABLE *table= table_list->table;
    if (table)
    {
      TABLE *save_next= table->next;
      bool result;
      table->next= 0;
      result= table_is_used(table_list->table, 0);
      table->next= save_next;
      if (result)
        return 1;
    }
  }
  return 0;					// All tables are locked
}


bool wait_for_locked_table_names(THD *thd, TABLE_LIST *table_list)
{
  bool result=0;
  DBUG_ENTER("wait_for_locked_table_names");

  safe_mutex_assert_owner(&LOCK_open);

  while (locked_named_table(thd,table_list))
  {
    if (thd->killed)
    {
      result=1;
      break;
    }
    wait_for_condition(thd, &LOCK_open, &COND_refresh);
    pthread_mutex_lock(&LOCK_open);
  }
  DBUG_RETURN(result);
}


/**
  Lock all tables in list with a name lock.

  REQUIREMENTS
  - One must have a lock on LOCK_open when calling this

  @param thd			Thread handle
  @param table_list		Names of tables to lock

  @note
    If you are just locking one table, you should use
    lock_and_wait_for_table_name().

  @retval
    0	ok
  @retval
    1	Fatal error (end of memory ?)
*/

bool lock_table_names(THD *thd, TABLE_LIST *table_list)
{
  bool got_all_locks=1;
  TABLE_LIST *lock_table;

  for (lock_table= table_list; lock_table; lock_table= lock_table->next_local)
  {
    int got_lock;
    if ((got_lock=lock_table_name(thd,lock_table, TRUE)) < 0)
      goto end;					// Fatal error
    if (got_lock)
      got_all_locks=0;				// Someone is using table
  }

  /* If some table was in use, wait until we got the lock */
  if (!got_all_locks && wait_for_locked_table_names(thd, table_list))
    goto end;
  return 0;

end:
  unlock_table_names(thd, table_list, lock_table);
  return 1;
}


/**
  Unlock all tables in list with a name lock.

  @param thd        Thread handle.
  @param table_list Names of tables to lock.

  @note 
    This function needs to be protected by LOCK_open. If we're 
    under LOCK TABLES, this function does not work as advertised. Namely,
    it does not exclude other threads from using this table and does not
    put an exclusive name lock on this table into the table cache.

  @see lock_table_names
  @see unlock_table_names

  @retval TRUE An error occured.
  @retval FALSE Name lock successfully acquired.
*/

bool lock_table_names_exclusively(THD *thd, TABLE_LIST *table_list)
{
  if (lock_table_names(thd, table_list))
    return TRUE;

  /*
    Upgrade the table name locks from semi-exclusive to exclusive locks.
  */
  for (TABLE_LIST *table= table_list; table; table= table->next_global)
  {
    if (table->table)
      table->table->open_placeholder= 1;
  }
  return FALSE;
}


/**
  Test is 'table' is protected by an exclusive name lock.

  @param[in] thd        The current thread handler
  @param[in] table_list Table container containing the single table to be
                        tested

  @note Needs to be protected by LOCK_open mutex.

  @return Error status code
    @retval TRUE Table is protected
    @retval FALSE Table is not protected
*/

bool
is_table_name_exclusively_locked_by_this_thread(THD *thd,
                                                TABLE_LIST *table_list)
{
  char  key[MAX_DBKEY_LENGTH];
  uint  key_length;

  key_length= create_table_def_key(thd, key, table_list, 0);

  return is_table_name_exclusively_locked_by_this_thread(thd, (uchar *)key,
                                                         key_length);
}


/**
  Test is 'table key' is protected by an exclusive name lock.

  @param[in] thd        The current thread handler.
  @param[in] key
  @param[in] key_length

  @note Needs to be protected by LOCK_open mutex

  @retval TRUE Table is protected
  @retval FALSE Table is not protected
 */

bool
is_table_name_exclusively_locked_by_this_thread(THD *thd, uchar *key,
                                                int key_length)
{
  HASH_SEARCH_STATE state;
  TABLE *table;

  for (table= (TABLE*) hash_first(&open_cache, key,
                                  key_length, &state);
       table ;
       table= (TABLE*) hash_next(&open_cache, key,
                                 key_length, &state))
  {
    if (table->in_use == thd &&
        table->open_placeholder == 1 &&
        table->s->version == 0)
      return TRUE;
  }

  return FALSE;
}

/**
  Unlock all tables in list with a name lock.

  @param
    thd			Thread handle
  @param
    table_list		Names of tables to unlock
  @param
    last_table		Don't unlock any tables after this one.
			        (default 0, which will unlock all tables)

  @note
    One must have a lock on LOCK_open when calling this.

  @note
    This function will broadcast refresh signals to inform other threads
    that the name locks are removed.

  @retval
    0	ok
  @retval
    1	Fatal error (end of memory ?)
*/

void unlock_table_names(THD *thd, TABLE_LIST *table_list,
			TABLE_LIST *last_table)
{
  DBUG_ENTER("unlock_table_names");
  for (TABLE_LIST *table= table_list;
       table != last_table;
       table= table->next_local)
    unlock_table_name(thd,table);
  broadcast_refresh();
  DBUG_VOID_RETURN;
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

  Taking the global read lock is TWO steps (2nd step is optional; without
  it, COMMIT of existing transactions will be allowed):
  lock_global_read_lock() THEN make_global_read_lock_block_commit().

  The global locks are handled through the global variables:
  global_read_lock
    count of threads which have the global read lock (i.e. have completed at
    least the first step above)
  global_read_lock_blocks_commit
    count of threads which have the global read lock and block
    commits (i.e. are in or have completed the second step above)
  waiting_for_read_lock
    count of threads which want to take a global read lock but cannot
  protect_against_global_read_lock
    count of threads which have set protection against global read lock.

  access to them is protected with a mutex LOCK_global_read_lock

  (XXX: one should never take LOCK_open if LOCK_global_read_lock is
  taken, otherwise a deadlock may occur. Other mutexes could be a
  problem too - grep the code for global_read_lock if you want to use
  any other mutex here) Also one must not hold LOCK_open when calling
  wait_if_global_read_lock(). When the thread with the global read lock
  tries to close its tables, it needs to take LOCK_open in
  close_thread_table().

  How blocking of threads by global read lock is achieved: that's
  advisory. Any piece of code which should be blocked by global read lock must
  be designed like this:
  - call to wait_if_global_read_lock(). When this returns 0, no global read
  lock is owned; if argument abort_on_refresh was 0, none can be obtained.
  - job
  - if abort_on_refresh was 0, call to start_waiting_global_read_lock() to
  allow other threads to get the global read lock. I.e. removal of the
  protection.
  (Note: it's a bit like an implementation of rwlock).

  [ I am sorry to mention some SQL syntaxes below I know I shouldn't but found
  no better descriptive way ]

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

volatile uint global_read_lock=0;
volatile uint global_read_lock_blocks_commit=0;
static volatile uint protect_against_global_read_lock=0;
static volatile uint waiting_for_read_lock=0;

#define GOT_GLOBAL_READ_LOCK               1
#define MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT 2

bool lock_global_read_lock(THD *thd)
{
  DBUG_ENTER("lock_global_read_lock");

  if (!thd->global_read_lock)
  {
    const char *old_message;
    (void) pthread_mutex_lock(&LOCK_global_read_lock);
    old_message=thd->enter_cond(&COND_global_read_lock, &LOCK_global_read_lock,
                                "Waiting to get readlock");
    DBUG_PRINT("info",
	       ("waiting_for: %d  protect_against: %d",
		waiting_for_read_lock, protect_against_global_read_lock));

    waiting_for_read_lock++;
    while (protect_against_global_read_lock && !thd->killed)
      pthread_cond_wait(&COND_global_read_lock, &LOCK_global_read_lock);
    waiting_for_read_lock--;
    if (thd->killed)
    {
      thd->exit_cond(old_message);
      DBUG_RETURN(1);
    }
    thd->global_read_lock= GOT_GLOBAL_READ_LOCK;
    global_read_lock++;
    thd->exit_cond(old_message); // this unlocks LOCK_global_read_lock
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


void unlock_global_read_lock(THD *thd)
{
  uint tmp;
  DBUG_ENTER("unlock_global_read_lock");
  DBUG_PRINT("info",
             ("global_read_lock: %u  global_read_lock_blocks_commit: %u",
              global_read_lock, global_read_lock_blocks_commit));

  pthread_mutex_lock(&LOCK_global_read_lock);
  tmp= --global_read_lock;
  if (thd->global_read_lock == MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT)
    --global_read_lock_blocks_commit;
  pthread_mutex_unlock(&LOCK_global_read_lock);
  /* Send the signal outside the mutex to avoid a context switch */
  if (!tmp)
  {
    DBUG_PRINT("signal", ("Broadcasting COND_global_read_lock"));
    pthread_cond_broadcast(&COND_global_read_lock);
  }
  thd->global_read_lock= 0;

  DBUG_VOID_RETURN;
}

#define must_wait (global_read_lock &&                             \
                   (is_not_commit ||                               \
                    global_read_lock_blocks_commit))

bool wait_if_global_read_lock(THD *thd, bool abort_on_refresh,
                              bool is_not_commit)
{
  const char *UNINIT_VAR(old_message);
  bool result= 0, need_exit_cond;
  DBUG_ENTER("wait_if_global_read_lock");

  /*
    Assert that we do not own LOCK_open. If we would own it, other
    threads could not close their tables. This would make a pretty
    deadlock.
  */
  safe_mutex_assert_not_owner(&LOCK_open);

  (void) pthread_mutex_lock(&LOCK_global_read_lock);
  if ((need_exit_cond= must_wait))
  {
    if (thd->global_read_lock)		// This thread had the read locks
    {
      if (is_not_commit)
        my_message(ER_CANT_UPDATE_WITH_READLOCK,
                   ER(ER_CANT_UPDATE_WITH_READLOCK), MYF(0));
      (void) pthread_mutex_unlock(&LOCK_global_read_lock);
      /*
        We allow FLUSHer to COMMIT; we assume FLUSHer knows what it does.
        This allowance is needed to not break existing versions of innobackup
        which do a BEGIN; INSERT; FLUSH TABLES WITH READ LOCK; COMMIT.
      */
      DBUG_RETURN(is_not_commit);
    }
    old_message=thd->enter_cond(&COND_global_read_lock, &LOCK_global_read_lock,
				"Waiting for release of readlock");
    while (must_wait && ! thd->killed &&
	   (!abort_on_refresh || thd->version == refresh_version))
    {
      DBUG_PRINT("signal", ("Waiting for COND_global_read_lock"));
      (void) pthread_cond_wait(&COND_global_read_lock, &LOCK_global_read_lock);
      DBUG_PRINT("signal", ("Got COND_global_read_lock"));
    }
    if (thd->killed)
      result=1;
  }
  if (!abort_on_refresh && !result)
    protect_against_global_read_lock++;
  /*
    The following is only true in case of a global read locks (which is rare)
    and if old_message is set
  */
  if (unlikely(need_exit_cond))
    thd->exit_cond(old_message); // this unlocks LOCK_global_read_lock
  else
    pthread_mutex_unlock(&LOCK_global_read_lock);
  DBUG_RETURN(result);
}


void start_waiting_global_read_lock(THD *thd)
{
  bool tmp;
  DBUG_ENTER("start_waiting_global_read_lock");
  if (unlikely(thd->global_read_lock))
    DBUG_VOID_RETURN;
  (void) pthread_mutex_lock(&LOCK_global_read_lock);
  DBUG_ASSERT(protect_against_global_read_lock);
  tmp= (!--protect_against_global_read_lock &&
        (waiting_for_read_lock || global_read_lock_blocks_commit));
  (void) pthread_mutex_unlock(&LOCK_global_read_lock);
  if (tmp)
    pthread_cond_broadcast(&COND_global_read_lock);
  DBUG_VOID_RETURN;
}


bool make_global_read_lock_block_commit(THD *thd)
{
  bool error;
  const char *old_message;
  DBUG_ENTER("make_global_read_lock_block_commit");
  /*
    If we didn't succeed lock_global_read_lock(), or if we already suceeded
    make_global_read_lock_block_commit(), do nothing.
  */
  if (thd->global_read_lock != GOT_GLOBAL_READ_LOCK)
    DBUG_RETURN(0);
  pthread_mutex_lock(&LOCK_global_read_lock);
  /* increment this BEFORE waiting on cond (otherwise race cond) */
  global_read_lock_blocks_commit++;
  /* For testing we set up some blocking, to see if we can be killed */
  DBUG_EXECUTE_IF("make_global_read_lock_block_commit_loop",
                  protect_against_global_read_lock++;);
  old_message= thd->enter_cond(&COND_global_read_lock, &LOCK_global_read_lock,
                               "Waiting for all running commits to finish");
  while (protect_against_global_read_lock && !thd->killed)
    pthread_cond_wait(&COND_global_read_lock, &LOCK_global_read_lock);
  DBUG_EXECUTE_IF("make_global_read_lock_block_commit_loop",
                  protect_against_global_read_lock--;);
  if ((error= test(thd->killed)))
    global_read_lock_blocks_commit--; // undo what we did
  else
    thd->global_read_lock= MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT;
  thd->exit_cond(old_message); // this unlocks LOCK_global_read_lock
  DBUG_RETURN(error);
}


/**
  Broadcast COND_refresh and COND_global_read_lock.

    Due to a bug in a threading library it could happen that a signal
    did not reach its target. A condition for this was that the same
    condition variable was used with different mutexes in
    pthread_cond_wait(). Some time ago we changed LOCK_open to
    LOCK_global_read_lock in global read lock handling. So COND_refresh
    was used with LOCK_open and LOCK_global_read_lock.

    We did now also change from COND_refresh to COND_global_read_lock
    in global read lock handling. But now it is necessary to signal
    both conditions at the same time.

  @note
    When signalling COND_global_read_lock within the global read lock
    handling, it is not necessary to also signal COND_refresh.
*/

void broadcast_refresh(void)
{
  VOID(pthread_cond_broadcast(&COND_refresh));
  VOID(pthread_cond_broadcast(&COND_global_read_lock));
}

/**
  @} (end of group Locking)
*/
