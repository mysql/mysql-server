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


/* Handler-calling-functions */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "ha_heap.h"
#include "ha_myisam.h"
#include "ha_myisammrg.h"
#ifdef HAVE_ISAM
#include "ha_isam.h"
#include "ha_isammrg.h"
#endif
#ifdef HAVE_BERKELEY_DB
#include "ha_berkeley.h"
#endif
#ifdef HAVE_EXAMPLE_DB
#include "examples/ha_example.h"
#endif
#ifdef HAVE_ARCHIVE_DB
#include "examples/ha_archive.h"
#endif
#ifdef HAVE_CSV_DB
#include "examples/ha_tina.h"
#endif
#ifdef HAVE_INNOBASE_DB
#include "ha_innodb.h"
#endif
#ifdef HAVE_NDBCLUSTER_DB
#include "ha_ndbcluster.h"
#endif
#include <myisampack.h>
#include <errno.h>

	/* static functions defined in this file */

static int NEAR_F delete_file(const char *name,const char *ext,int extflag);

ulong ha_read_count, ha_write_count, ha_delete_count, ha_update_count,
      ha_read_key_count, ha_read_next_count, ha_read_prev_count,
      ha_read_first_count, ha_read_last_count,
      ha_commit_count, ha_rollback_count,
      ha_read_rnd_count, ha_read_rnd_next_count, ha_discover_count;

static SHOW_COMP_OPTION have_yes= SHOW_OPTION_YES;

struct show_table_type_st sys_table_types[]=
{
  {"MyISAM",	&have_yes,
   "Default engine as of MySQL 3.23 with great performance", DB_TYPE_MYISAM},
  {"HEAP",	&have_yes,
   "Alias for MEMORY", DB_TYPE_HEAP},
  {"MEMORY",	&have_yes,
   "Hash based, stored in memory, useful for temporary tables", DB_TYPE_HEAP},
  {"MERGE",	&have_yes,
   "Collection of identical MyISAM tables", DB_TYPE_MRG_MYISAM},
  {"MRG_MYISAM",&have_yes,
   "Alias for MERGE", DB_TYPE_MRG_MYISAM},
  {"ISAM",	&have_isam,
   "Obsolete storage engine, now replaced by MyISAM", DB_TYPE_ISAM},
  {"MRG_ISAM",  &have_isam,
   "Obsolete storage engine, now replaced by MERGE", DB_TYPE_MRG_ISAM},
  {"InnoDB",	&have_innodb,
   "Supports transactions, row-level locking, and foreign keys", DB_TYPE_INNODB},
  {"INNOBASE",	&have_innodb,
   "Alias for INNODB", DB_TYPE_INNODB},
  {"BDB",	&have_berkeley_db,
   "Supports transactions and page-level locking", DB_TYPE_BERKELEY_DB},
  {"BERKELEYDB",&have_berkeley_db,
   "Alias for BDB", DB_TYPE_BERKELEY_DB},
  {"NDBCLUSTER", &have_ndbcluster,
   "Clustered, fault-tolerant, memory-based tables", DB_TYPE_NDBCLUSTER},
  {"NDB", &have_ndbcluster,
   "Alias for NDBCLUSTER", DB_TYPE_NDBCLUSTER},
  {"EXAMPLE",&have_example_db,
   "Example storage engine", DB_TYPE_EXAMPLE_DB},
  {"ARCHIVE",&have_archive_db,
   "Archive storage engine", DB_TYPE_ARCHIVE_DB},
  {"CSV",&have_csv_db,
   "CSV storage engine", DB_TYPE_CSV_DB},
  {NullS, NULL, NullS, DB_TYPE_UNKNOWN}
};

const char *ha_row_type[] = {
  "", "FIXED", "DYNAMIC", "COMPRESSED","?","?","?"
};

const char *tx_isolation_names[] =
{ "READ-UNCOMMITTED", "READ-COMMITTED", "REPEATABLE-READ", "SERIALIZABLE",
  NullS};
TYPELIB tx_isolation_typelib= {array_elements(tx_isolation_names)-1,"",
			       tx_isolation_names};

enum db_type ha_resolve_by_name(const char *name, uint namelen)
{
  THD *thd=current_thd;
  if (thd && !my_strcasecmp(&my_charset_latin1, name, "DEFAULT")) {
    return (enum db_type) thd->variables.table_type;
  }
  
  show_table_type_st *types;
  for (types= sys_table_types; types->type; types++)
  {
    if (!my_strcasecmp(&my_charset_latin1, name, types->type))
      return (enum db_type) types->db_type;
  }
  return DB_TYPE_UNKNOWN;
}

const char *ha_get_storage_engine(enum db_type db_type)
{
  show_table_type_st *types;
  for (types= sys_table_types; types->type; types++)
  {
    if (db_type == types->db_type)
      return types->type;
  }
  
  return "none";
}

	/* Use other database handler if databasehandler is not incompiled */

enum db_type ha_checktype(enum db_type database_type)
{
  show_table_type_st *types;
  for (types= sys_table_types; types->type; types++)
  {
    if ((database_type == types->db_type) && 
	(*types->value == SHOW_OPTION_YES))
      return database_type;
  }

  switch (database_type) {
#ifndef NO_HASH
  case DB_TYPE_HASH:
    return (database_type);
#endif
  case DB_TYPE_MRG_ISAM:
    return (DB_TYPE_MRG_MYISAM);
  default:
    break;
  }
  
  return 
    DB_TYPE_UNKNOWN != (enum db_type) current_thd->variables.table_type ?
    (enum db_type) current_thd->variables.table_type :
    DB_TYPE_UNKNOWN != (enum db_type) global_system_variables.table_type ?
    (enum db_type) global_system_variables.table_type :
    DB_TYPE_MYISAM;
} /* ha_checktype */


handler *get_new_handler(TABLE *table, enum db_type db_type)
{
  switch (db_type) {
#ifndef NO_HASH
  case DB_TYPE_HASH:
    return new ha_hash(table);
#endif
#ifdef HAVE_ISAM
  case DB_TYPE_MRG_ISAM:
    return new ha_isammrg(table);
  case DB_TYPE_ISAM:
    return new ha_isam(table);
#else
  case DB_TYPE_MRG_ISAM:
    return new ha_myisammrg(table);
#endif
#ifdef HAVE_BERKELEY_DB
  case DB_TYPE_BERKELEY_DB:
    return new ha_berkeley(table);
#endif
#ifdef HAVE_INNOBASE_DB
  case DB_TYPE_INNODB:
    return new ha_innobase(table);
#endif
#ifdef HAVE_EXAMPLE_DB
  case DB_TYPE_EXAMPLE_DB:
    return new ha_example(table);
#endif
#ifdef HAVE_ARCHIVE_DB
  case DB_TYPE_ARCHIVE_DB:
    return new ha_archive(table);
#endif
#ifdef HAVE_CSV_DB
  case DB_TYPE_CSV_DB:
    return new ha_tina(table);
#endif
#ifdef HAVE_NDBCLUSTER_DB
  case DB_TYPE_NDBCLUSTER:
    return new ha_ndbcluster(table);
#endif
  case DB_TYPE_HEAP:
    return new ha_heap(table);
  default:					// should never happen
  {
    enum db_type def=(enum db_type) current_thd->variables.table_type;
    /* Try first with 'default table type' */
    if (db_type != def)
      return get_new_handler(table, def);
  }
  /* Fall back to MyISAM */
  case DB_TYPE_MYISAM:
    return new ha_myisam(table);
  case DB_TYPE_MRG_MYISAM:
    return new ha_myisammrg(table);
  }
}

bool ha_caching_allowed(THD* thd, char* table_key,
                        uint key_length, uint8 cache_type)
{
#ifdef HAVE_INNOBASE_DB
  if (cache_type == HA_CACHE_TBL_ASKTRANSACT)
    return innobase_query_caching_of_table_permitted(thd, table_key, key_length);
#endif
  return 1;
}

int ha_init()
{
  int error= 0;
#ifdef HAVE_BERKELEY_DB
  if (have_berkeley_db == SHOW_OPTION_YES)
  {
    if (berkeley_init())
    {
      have_berkeley_db= SHOW_OPTION_DISABLED;	// If we couldn't use handler
      error= 1;
    }
    else
      opt_using_transactions=1;
  }
#endif
#ifdef HAVE_INNOBASE_DB
  if (have_innodb == SHOW_OPTION_YES)
  {
    if (innobase_init())
    {
      have_innodb= SHOW_OPTION_DISABLED;	// If we couldn't use handler
      error= 1;
    }
    else
      opt_using_transactions=1;
  }
#endif
#ifdef HAVE_NDBCLUSTER_DB
  if (have_ndbcluster == SHOW_OPTION_YES)
  {
    if (ndbcluster_init())
    {
      have_ndbcluster= SHOW_OPTION_DISABLED;
      error= 1;
    }
    else
      opt_using_transactions=1;
  }
#endif
  return error;
}

	/* close, flush or restart databases */
	/* Ignore this for other databases than ours */

int ha_panic(enum ha_panic_function flag)
{
  int error=0;
#ifndef NO_HASH
  error|=h_panic(flag);			/* fix hash */
#endif
#ifdef HAVE_ISAM
  error|=mrg_panic(flag);
  error|=nisam_panic(flag);
#endif
  error|=heap_panic(flag);
  error|=mi_panic(flag);
  error|=myrg_panic(flag);
#ifdef HAVE_BERKELEY_DB
  if (have_berkeley_db == SHOW_OPTION_YES)
    error|=berkeley_end();
#endif
#ifdef HAVE_INNOBASE_DB
  if (have_innodb == SHOW_OPTION_YES)
    error|=innobase_end();
#endif
#ifdef HAVE_NDBCLUSTER_DB
  if (have_ndbcluster == SHOW_OPTION_YES)
    error|=ndbcluster_end();
#endif
  return error;
} /* ha_panic */

void ha_drop_database(char* path)
{
#ifdef HAVE_INNOBASE_DB
  if (have_innodb == SHOW_OPTION_YES)
    innobase_drop_database(path);
#endif
#ifdef HAVE_NDBCLUSTER_DB
  if (have_ndbcluster == SHOW_OPTION_YES)
    ndbcluster_drop_database(path);
#endif
}

void ha_close_connection(THD* thd)
{
#ifdef HAVE_INNOBASE_DB
  if (have_innodb == SHOW_OPTION_YES)
    innobase_close_connection(thd);
#endif
#ifdef HAVE_NDBCLUSTER_DB
  if (have_ndbcluster == SHOW_OPTION_YES)
    ndbcluster_close_connection(thd);
#endif
}

/*
  This is used to commit or rollback a single statement depending on the value
  of error. Note that if the autocommit is on, then the following call inside
  InnoDB will commit or rollback the whole transaction (= the statement). The
  autocommit mechanism built into InnoDB is based on counting locks, but if
  the user has used LOCK TABLES then that mechanism does not know to do the
  commit.
*/

int ha_autocommit_or_rollback(THD *thd, int error)
{
  DBUG_ENTER("ha_autocommit_or_rollback");
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
    if (!error)
    {
      if (ha_commit_stmt(thd))
	error=1;
    }
    else
      (void) ha_rollback_stmt(thd);

    thd->variables.tx_isolation=thd->session_tx_isolation;
  }
#endif
  DBUG_RETURN(error);
}

/*
  This function is called when MySQL writes the log segment of a
  transaction to the binlog. It is called when the LOCK_log mutex is
  reserved. Here we communicate to transactional table handlers what
  binlog position corresponds to the current transaction. The handler
  can store it and in recovery print to the user, so that the user
  knows from what position in the binlog to start possible
  roll-forward, for example, if the crashed server was a slave in
  replication. This function also calls the commit of the table
  handler, because the order of transactions in the log of the table
  handler must be the same as in the binlog.
  NOTE that to eliminate the bottleneck of the group commit, we do not
  flush the handler log files here, but only later in a call of
  ha_commit_complete().

  arguments:
  thd:           the thread handle of the current connection
  log_file_name: latest binlog file name
  end_offset:	 the offset in the binlog file up to which we wrote
  return value:  0 if success, 1 if error
*/

int ha_report_binlog_offset_and_commit(THD *thd,
				       char *log_file_name,
				       my_off_t end_offset)
{
  int  error= 0;
#ifdef HAVE_INNOBASE_DB
  THD_TRANS *trans;
  trans = &thd->transaction.all;
  if (trans->innodb_active_trans)
  {
    /*
      If we updated some InnoDB tables (innodb_active_trans is true), the
      binlog coords will be reported into InnoDB during the InnoDB commit
      (innobase_report_binlog_offset_and_commit). But if we updated only
      non-InnoDB tables, we need an explicit call to report it.
    */
    if ((error=innobase_report_binlog_offset_and_commit(thd,
                                                        trans->innobase_tid,
                                                        log_file_name,
                                                        end_offset)))
    {
      my_error(ER_ERROR_DURING_COMMIT, MYF(0), error);
      error=1;
    }
  }
  else if (opt_innodb_safe_binlog) // Don't report if not useful
    innobase_store_binlog_offset_and_flush_log(log_file_name, end_offset);
#endif
  return error;
}

/*
  Flushes the handler log files (if my.cnf settings do not free us from it)
  after we have called ha_report_binlog_offset_and_commit(). To eliminate
  the bottleneck from the group commit, this should be called when
  LOCK_log has been released in log.cc.

  arguments:
  thd:           the thread handle of the current connection
  return value:  always 0
*/

int ha_commit_complete(THD *thd)
{
#ifdef HAVE_INNOBASE_DB
  THD_TRANS *trans;
  trans = &thd->transaction.all;
  if (trans->innobase_tid)
  {
    innobase_commit_complete(trans->innobase_tid);

    trans->innodb_active_trans=0;
  }
#endif
  return 0;
}

/*
  This function should be called when MySQL sends rows of a SELECT result set
  or the EOF mark to the client. It releases a possible adaptive hash index
  S-latch held by thd in InnoDB and also releases a possible InnoDB query
  FIFO ticket to enter InnoDB. To save CPU time, InnoDB allows a thd to
  keep them over several calls of the InnoDB handler interface when a join
  is executed. But when we let the control to pass to the client they have
  to be released because if the application program uses mysql_use_result(),
  it may deadlock on the S-latch if the application on another connection
  performs another SQL query. In MySQL-4.1 this is even more important because
  there a connection can have several SELECT queries open at the same time.

  arguments:
  thd:           the thread handle of the current connection
  return value:  always 0
*/

int ha_release_temporary_latches(THD *thd)
{
#ifdef HAVE_INNOBASE_DB
  THD_TRANS *trans;
  trans = &thd->transaction.all;
  if (trans->innobase_tid)
    innobase_release_temporary_latches(trans->innobase_tid);
#endif
  return 0;
}

int ha_commit_trans(THD *thd, THD_TRANS* trans)
{
  int error=0;
  DBUG_ENTER("ha_commit_trans");
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
    bool transaction_commited= 0;
    bool operation_done= 0, need_start_waiters= 0;

    /* If transaction has done some updates to tables */
    if (trans == &thd->transaction.all && mysql_bin_log.is_open() &&
        my_b_tell(&thd->transaction.trans_log))
    {
      if (error= wait_if_global_read_lock(thd, 0, 0))
      {
        /*
          Note that ROLLBACK [TO SAVEPOINT] does not have this test; it's
          because ROLLBACK never updates data, so needn't wait on the lock.
        */
        my_error(ER_ERROR_DURING_COMMIT, MYF(0), error);
        error= 1;
      }
      else
        need_start_waiters= 1;
      if (mysql_bin_log.is_open())
      {
        mysql_bin_log.write(thd, &thd->transaction.trans_log, 1);
        statistic_increment(binlog_cache_use, &LOCK_status);
        if (thd->transaction.trans_log.disk_writes != 0)
        {
          /* 
            We have to do this after addition of trans_log to main binlog since
            this operation can cause flushing of end of trans_log to disk. 
          */
          statistic_increment(binlog_cache_disk_use, &LOCK_status);
          thd->transaction.trans_log.disk_writes= 0;
        }
        reinit_io_cache(&thd->transaction.trans_log,
                        WRITE_CACHE, (my_off_t) 0, 0, 1);
        thd->transaction.trans_log.end_of_file= max_binlog_cache_size;
      }
    }
#ifdef HAVE_NDBCLUSTER_DB
    if (trans->ndb_tid)
    {
      if ((error=ndbcluster_commit(thd,trans->ndb_tid)))
      {
	if (error == -1)
	  my_error(ER_ERROR_DURING_COMMIT, MYF(0));
        error=1;
      }
      if (trans == &thd->transaction.all)
        operation_done= transaction_commited= 1;
      trans->ndb_tid=0;
    }
#endif
#ifdef HAVE_BERKELEY_DB
    if (trans->bdb_tid)
    {
      if ((error=berkeley_commit(thd,trans->bdb_tid)))
      {
	my_error(ER_ERROR_DURING_COMMIT, MYF(0), error);
	error=1;
      }
      else
	if (!(thd->options & OPTION_BEGIN))
	  transaction_commited= 1; 
      trans->bdb_tid=0;
    }
#endif
#ifdef HAVE_INNOBASE_DB
    if (trans->innobase_tid)
    {
      if ((error=innobase_commit(thd,trans->innobase_tid)))
      {
	my_error(ER_ERROR_DURING_COMMIT, MYF(0), error);
	error=1;
      }
      trans->innodb_active_trans=0;
      if (trans == &thd->transaction.all)
	operation_done= transaction_commited= 1;
    }
#endif
#ifdef HAVE_QUERY_CACHE
    if (transaction_commited && thd->transaction.changed_tables)
      query_cache.invalidate(thd->transaction.changed_tables);
#endif /*HAVE_QUERY_CACHE*/
    if (error && trans == &thd->transaction.all && mysql_bin_log.is_open())
      sql_print_error("Got error during commit;  Binlog is not up to date!");
    thd->variables.tx_isolation=thd->session_tx_isolation;
    if (operation_done)
    {
      statistic_increment(ha_commit_count,&LOCK_status);
      thd->transaction.cleanup();
    }
    if (need_start_waiters)
      start_waiting_global_read_lock(thd);
  }
#endif // using transactions
  DBUG_RETURN(error);
}


int ha_rollback_trans(THD *thd, THD_TRANS *trans)
{
  int error=0;
  DBUG_ENTER("ha_rollback_trans");
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
    bool operation_done=0;
#ifdef HAVE_NDBCLUSTER_DB
    if (trans->ndb_tid)
    {
      if ((error=ndbcluster_rollback(thd, trans->ndb_tid)))
      {
	if (error == -1)
	  my_error(ER_ERROR_DURING_ROLLBACK, MYF(0));
        error=1;
      }
      trans->ndb_tid = 0;
      operation_done=1;
    }
#endif
#ifdef HAVE_BERKELEY_DB
    if (trans->bdb_tid)
    {
      if ((error=berkeley_rollback(thd, trans->bdb_tid)))
      {
	my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), error);
	error=1;
      }
      trans->bdb_tid=0;
      operation_done=1;
    }
#endif
#ifdef HAVE_INNOBASE_DB
    if (trans->innobase_tid)
    {
      if ((error=innobase_rollback(thd, trans->innobase_tid)))
      {
	my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), error);
	error=1;
      }
      trans->innodb_active_trans=0;
      operation_done=1;
    }
#endif
    if ((trans == &thd->transaction.all) && mysql_bin_log.is_open())
    {
      /* 
         Update the binary log with a BEGIN/ROLLBACK block if we have
         cached some queries and we updated some non-transactional
         table. Such cases should be rare (updating a
         non-transactional table inside a transaction...).  Count disk
         writes to trans_log in any case.
      */
      if (my_b_tell(&thd->transaction.trans_log))
      {
        if (unlikely(thd->options & OPTION_STATUS_NO_TRANS_UPDATE))
          mysql_bin_log.write(thd, &thd->transaction.trans_log, 0);
        statistic_increment(binlog_cache_use, &LOCK_status);
        if (thd->transaction.trans_log.disk_writes != 0)
        {
          /* 
            We have to do this after addition of trans_log to main binlog since
            this operation can cause flushing of end of trans_log to disk. 
          */
          statistic_increment(binlog_cache_disk_use, &LOCK_status);
          thd->transaction.trans_log.disk_writes= 0;
        }
      }
      /* Flushed or not, empty the binlog cache */
      reinit_io_cache(&thd->transaction.trans_log,
                      WRITE_CACHE, (my_off_t) 0, 0, 1);
      thd->transaction.trans_log.end_of_file= max_binlog_cache_size;
      if (operation_done)
        thd->transaction.cleanup();
    }
    thd->variables.tx_isolation=thd->session_tx_isolation;
    if (operation_done)
      statistic_increment(ha_rollback_count,&LOCK_status);
  }
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}


/*
  Rolls the current transaction back to a savepoint.
  Return value: 0 if success, 1 if there was not a savepoint of the given
  name.
  NOTE: how do we handle this (unlikely but legal) case:
  [transaction] + [update to non-trans table] + [rollback to savepoint] ?
  The problem occurs when a savepoint is before the update to the
  non-transactional table. Then when there's a rollback to the savepoint, if we
  simply truncate the binlog cache, we lose the part of the binlog cache where
  the update is. If we want to not lose it, we need to write the SAVEPOINT
  command and the ROLLBACK TO SAVEPOINT command to the binlog cache. The latter
  is easy: it's just write at the end of the binlog cache, but the former
  should be *inserted* to the place where the user called SAVEPOINT. The
  solution is that when the user calls SAVEPOINT, we write it to the binlog
  cache (so no need to later insert it). As transactions are never intermixed
  in the binary log (i.e. they are serialized), we won't have conflicts with
  savepoint names when using mysqlbinlog or in the slave SQL thread.
  Then when ROLLBACK TO SAVEPOINT is called, if we updated some
  non-transactional table, we don't truncate the binlog cache but instead write
  ROLLBACK TO SAVEPOINT to it; otherwise we truncate the binlog cache (which
  will chop the SAVEPOINT command from the binlog cache, which is good as in
  that case there is no need to have it in the binlog).
*/

int ha_rollback_to_savepoint(THD *thd, char *savepoint_name)
{
  my_off_t binlog_cache_pos=0;
  bool operation_done=0;
  int error=0;
  DBUG_ENTER("ha_rollback_to_savepoint");
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
#ifdef HAVE_INNOBASE_DB
    /*
    Retrieve the trans_log binlog cache position corresponding to the
    savepoint, and if the rollback is successful inside InnoDB reset the write
    position in the binlog cache to what it was at the savepoint.
    */
    if ((error=innobase_rollback_to_savepoint(thd, savepoint_name,
						  &binlog_cache_pos)))
    {
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), error);
      error=1;
    }
    else if (mysql_bin_log.is_open())
    {
      /* 
         Write ROLLBACK TO SAVEPOINT to the binlog cache if we have updated some
         non-transactional table. Otherwise, truncate the binlog cache starting
         from the SAVEPOINT command.
      */
      if (unlikely((thd->options & OPTION_STATUS_NO_TRANS_UPDATE) &&
                   my_b_tell(&thd->transaction.trans_log)))
      {
        Query_log_event qinfo(thd, thd->query, thd->query_length, TRUE);
        if (mysql_bin_log.write(&qinfo))
          error= 1;
      }
      else
        reinit_io_cache(&thd->transaction.trans_log, WRITE_CACHE,
                        binlog_cache_pos, 0, 0);
    }
    operation_done=1;
#endif
    if (operation_done)
      statistic_increment(ha_rollback_count,&LOCK_status);
  }
#endif /* USING_TRANSACTIONS */

  DBUG_RETURN(error);
}


/*
Sets a transaction savepoint.
Return value: always 0, that is, succeeds always
*/

int ha_savepoint(THD *thd, char *savepoint_name)
{
  int error=0;
  DBUG_ENTER("ha_savepoint");
#ifdef USING_TRANSACTIONS
  if (opt_using_transactions)
  {
    /* Write it to the binary log (see comments of ha_rollback_to_savepoint) */
    if (mysql_bin_log.is_open())
    {
#ifdef HAVE_INNOBASE_DB
      innobase_savepoint(thd,savepoint_name,
                         my_b_tell(&thd->transaction.trans_log));
#endif
      Query_log_event qinfo(thd, thd->query, thd->query_length, TRUE);
      if (mysql_bin_log.write(&qinfo))
	error= 1;
    }
#ifdef HAVE_INNOBASE_DB
    else
      innobase_savepoint(thd,savepoint_name,0);
#endif
  }
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}

bool ha_flush_logs()
{
  bool result=0;
#ifdef HAVE_BERKELEY_DB
  if ((have_berkeley_db == SHOW_OPTION_YES) &&
      berkeley_flush_logs())
    result=1;
#endif
#ifdef HAVE_INNOBASE_DB
  if ((have_innodb == SHOW_OPTION_YES) &&
      innobase_flush_logs())
    result=1;
#endif
  return result;
}

/*
  This should return ENOENT if the file doesn't exists.
  The .frm file will be deleted only if we return 0 or ENOENT
*/

int ha_delete_table(enum db_type table_type, const char *path)
{
  char tmp_path[FN_REFLEN];
  handler *file=get_new_handler((TABLE*) 0, table_type);
  if (!file)
    return ENOENT;
  if (lower_case_table_names == 2 && !(file->table_flags() & HA_FILE_BASED))
  {
    /* Ensure that table handler get path in lower case */
    strmov(tmp_path, path);
    my_casedn_str(files_charset_info, tmp_path);
    path= tmp_path;
  }
  int error=file->delete_table(path);
  delete file;
  return error;
}


void ha_store_ptr(byte *buff, uint pack_length, my_off_t pos)
{
  switch (pack_length) {
#if SIZEOF_OFF_T > 4
  case 8: mi_int8store(buff,pos); break;
  case 7: mi_int7store(buff,pos); break;
  case 6: mi_int6store(buff,pos); break;
  case 5: mi_int5store(buff,pos); break;
#endif
  case 4: mi_int4store(buff,pos); break;
  case 3: mi_int3store(buff,pos); break;
  case 2: mi_int2store(buff,(uint) pos); break;
  case 1: buff[0]= (uchar) pos; break;
  }
  return;
}

my_off_t ha_get_ptr(byte *ptr, uint pack_length)
{
  my_off_t pos;
  switch (pack_length) {
#if SIZEOF_OFF_T > 4
  case 8:
    pos= (my_off_t) mi_uint8korr(ptr);
    break;
  case 7:
    pos= (my_off_t) mi_uint7korr(ptr);
    break;
  case 6:
    pos= (my_off_t) mi_uint6korr(ptr);
    break;
  case 5:
    pos= (my_off_t) mi_uint5korr(ptr);
    break;
#endif
  case 4:
    pos= (my_off_t) mi_uint4korr(ptr);
    break;
  case 3:
    pos= (my_off_t) mi_uint3korr(ptr);
    break;
  case 2:
    pos= (my_off_t) mi_uint2korr(ptr);
    break;
  case 1:
    pos= (my_off_t) mi_uint2korr(ptr);
    break;
  default:
    pos=0;					// Impossible
    break;
  }
 return pos;
}

/****************************************************************************
** General handler functions
****************************************************************************/

	/* Open database-handler. Try O_RDONLY if can't open as O_RDWR */
	/* Don't wait for locks if not HA_OPEN_WAIT_IF_LOCKED is set */

int handler::ha_open(const char *name, int mode, int test_if_locked)
{
  int error;
  DBUG_ENTER("handler::ha_open");
  DBUG_PRINT("enter",("name: %s  db_type: %d  db_stat: %d  mode: %d  lock_test: %d",
		      name, table->db_type, table->db_stat, mode,
		      test_if_locked));

  if ((error=open(name,mode,test_if_locked)))
  {
    if ((error == EACCES || error == EROFS) && mode == O_RDWR &&
	(table->db_stat & HA_TRY_READ_ONLY))
    {
      table->db_stat|=HA_READ_ONLY;
      error=open(name,O_RDONLY,test_if_locked);
    }
  }
  if (error)
  {
    my_errno=error;			/* Safeguard */
    DBUG_PRINT("error",("error: %d  errno: %d",error,errno));
  }
  else
  {
    if (table->db_options_in_use & HA_OPTION_READ_ONLY_DATA)
      table->db_stat|=HA_READ_ONLY;
    (void) extra(HA_EXTRA_NO_READCHECK);	// Not needed in SQL

    if (!alloc_root_inited(&table->mem_root))	// If temporary table
      ref=(byte*) sql_alloc(ALIGN_SIZE(ref_length)*2);
    else
      ref=(byte*) alloc_root(&table->mem_root, ALIGN_SIZE(ref_length)*2);
    if (!ref)
    {
      close();
      error=HA_ERR_OUT_OF_MEM;
    }
    else
      dupp_ref=ref+ALIGN_SIZE(ref_length);
  }
  DBUG_RETURN(error);
}

/*
  Read first row (only) from a table
  This is never called for InnoDB or BDB tables, as these table types
  has the HA_NOT_EXACT_COUNT set.
*/

int handler::read_first_row(byte * buf, uint primary_key)
{
  register int error;
  DBUG_ENTER("handler::read_first_row");

  statistic_increment(ha_read_first_count,&LOCK_status);

  /*
    If there is very few deleted rows in the table, find the first row by
    scanning the table.
  */
  if (deleted < 10 || primary_key >= MAX_KEY)
  {
    (void) ha_rnd_init(1);
    while ((error= rnd_next(buf)) == HA_ERR_RECORD_DELETED) ;
    (void) ha_rnd_end();
  }
  else
  {
    /* Find the first row through the primary key */
    (void) ha_index_init(primary_key);
    error=index_first(buf);
    (void) ha_index_end();
  }
  DBUG_RETURN(error);
}


/* Set a timestamp in record */

void handler::update_timestamp(byte *record)
{
  long skr= (long) current_thd->query_start();
#ifdef WORDS_BIGENDIAN
  if (table->db_low_byte_first)
  {
    int4store(record,skr);
  }
  else
#endif
  longstore(record,skr);
  return;
}

/*
  Updates field with field_type NEXT_NUMBER according to following:
  if field = 0 change field to the next free key in database.
*/

void handler::update_auto_increment()
{
  longlong nr;
  THD *thd;
  DBUG_ENTER("handler::update_auto_increment");
  if (table->next_number_field->val_int() != 0 ||
      table->auto_increment_field_not_null &&
      current_thd->variables.sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO)
  {
    table->auto_increment_field_not_null= FALSE;
    auto_increment_column_changed=0;
    DBUG_VOID_RETURN;
  }
  table->auto_increment_field_not_null= FALSE;
  thd=current_thd;
  if ((nr=thd->next_insert_id))
    thd->next_insert_id=0;			// Clear after use
  else
    nr=get_auto_increment();
  if (!table->next_number_field->store(nr))
    thd->insert_id((ulonglong) nr);
  else
    thd->insert_id(table->next_number_field->val_int());
  auto_increment_column_changed=1;
  DBUG_VOID_RETURN;
}


longlong handler::get_auto_increment()
{
  longlong nr;
  int error;

  (void) extra(HA_EXTRA_KEYREAD);
  index_init(table->next_number_index);
  if (!table->next_number_key_offset)
  {						// Autoincrement at key-start
    error=index_last(table->record[1]);
  }
  else
  {
    byte key[MAX_KEY_LENGTH];
    key_copy(key,table,table->next_number_index,
             table->next_number_key_offset);
    error=index_read(table->record[1], key, table->next_number_key_offset,
                     HA_READ_PREFIX_LAST);
  }

  if (error)
    nr=1;
  else
    nr=(longlong) table->next_number_field->
      val_int_offset(table->rec_buff_length)+1;
  index_end();
  (void) extra(HA_EXTRA_NO_KEYREAD);
  return nr;
}

	/* Print error that we got from handler function */

void handler::print_error(int error, myf errflag)
{
  DBUG_ENTER("handler::print_error");
  DBUG_PRINT("enter",("error: %d",error));

  int textno=ER_GET_ERRNO;
  switch (error) {
  case EACCES:
    textno=ER_OPEN_AS_READONLY;
    break;
  case EAGAIN:
    textno=ER_FILE_USED;
    break;
  case ENOENT:
    textno=ER_FILE_NOT_FOUND;
    break;
  case HA_ERR_KEY_NOT_FOUND:
  case HA_ERR_NO_ACTIVE_RECORD:
  case HA_ERR_END_OF_FILE:
    textno=ER_KEY_NOT_FOUND;
    break;
  case HA_ERR_WRONG_MRG_TABLE_DEF:
    textno=ER_WRONG_MRG_TABLE;
    break;
  case HA_ERR_FOUND_DUPP_KEY:
  {
    uint key_nr=get_dup_key(error);
    if ((int) key_nr >= 0)
    {
      /* Write the dupplicated key in the error message */
      char key[MAX_KEY_LENGTH];
      String str(key,sizeof(key),system_charset_info);
      key_unpack(&str,table,(uint) key_nr);
      uint max_length=MYSQL_ERRMSG_SIZE-(uint) strlen(ER(ER_DUP_ENTRY));
      if (str.length() >= max_length)
      {
	str.length(max_length-4);
	str.append("...");
      }
      my_error(ER_DUP_ENTRY,MYF(0),str.c_ptr(),key_nr+1);
      DBUG_VOID_RETURN;
    }
    textno=ER_DUP_KEY;
    break;
  }
  case HA_ERR_FOUND_DUPP_UNIQUE:
    textno=ER_DUP_UNIQUE;
    break;
  case HA_ERR_RECORD_CHANGED:
    textno=ER_CHECKREAD;
    break;
  case HA_ERR_CRASHED:
    textno=ER_NOT_KEYFILE;
    break;
  case HA_ERR_CRASHED_ON_USAGE:
    textno=ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_CRASHED_ON_REPAIR:
    textno=ER_CRASHED_ON_REPAIR;
    break;
  case HA_ERR_OUT_OF_MEM:
    my_error(ER_OUT_OF_RESOURCES,errflag);
    DBUG_VOID_RETURN;
  case HA_ERR_WRONG_COMMAND:
    textno=ER_ILLEGAL_HA;
    break;
  case HA_ERR_OLD_FILE:
    textno=ER_OLD_KEYFILE;
    break;
  case HA_ERR_UNSUPPORTED:
    textno=ER_UNSUPPORTED_EXTENSION;
    break;
  case HA_ERR_RECORD_FILE_FULL:
    textno=ER_RECORD_FILE_FULL;
    break;
  case HA_ERR_LOCK_WAIT_TIMEOUT:
    textno=ER_LOCK_WAIT_TIMEOUT;
    break;
  case HA_ERR_LOCK_TABLE_FULL:
    textno=ER_LOCK_TABLE_FULL;
    break;
  case HA_ERR_LOCK_DEADLOCK:
    textno=ER_LOCK_DEADLOCK;
    break;
  case HA_ERR_READ_ONLY_TRANSACTION:
    textno=ER_READ_ONLY_TRANSACTION;
    break;
  case HA_ERR_CANNOT_ADD_FOREIGN:
    textno=ER_CANNOT_ADD_FOREIGN;
    break;
  case HA_ERR_ROW_IS_REFERENCED:
    textno=ER_ROW_IS_REFERENCED;
    break;
  case HA_ERR_NO_REFERENCED_ROW:
    textno=ER_NO_REFERENCED_ROW;
    break;
  default:
    {
      /* The error was "unknown" to this function.
	 Ask handler if it has got a message for this error */
      bool temporary= FALSE;
      String str;
      temporary= get_error_message(error, &str);
      if (!str.is_empty())
      {
	const char* engine= table_type();
	if (temporary)
	  my_error(ER_GET_TEMPORARY_ERRMSG,MYF(0),error,str.ptr(),engine);
	else
	  my_error(ER_GET_ERRMSG,MYF(0),error,str.ptr(),engine);
      }
      else       
	my_error(ER_GET_ERRNO,errflag,error);
      DBUG_VOID_RETURN;
    }
  }
  my_error(textno,errflag,table->table_name,error);
  DBUG_VOID_RETURN;
}


/* 
   Return an error message specific to this handler
   
   SYNOPSIS
   error        error code previously returned by handler
   buf          Pointer to String where to add error message
   
   Returns true if this is a temporary error
 */

bool handler::get_error_message(int error, String* buf)
{
  return FALSE;
}


/* Return key if error because of duplicated keys */

uint handler::get_dup_key(int error)
{
  DBUG_ENTER("handler::get_dup_key");
  table->file->errkey  = (uint) -1;
  if (error == HA_ERR_FOUND_DUPP_KEY || error == HA_ERR_FOUND_DUPP_UNIQUE)
    info(HA_STATUS_ERRKEY | HA_STATUS_NO_LOCK);
  DBUG_RETURN(table->file->errkey);
}


int handler::delete_table(const char *name)
{
  int error=0;
  for (const char **ext=bas_ext(); *ext ; ext++)
  {
    if (delete_file(name,*ext,2))
    {
      if ((error=errno) != ENOENT)
	break;
    }
  }
  return error;
}


int handler::rename_table(const char * from, const char * to)
{
  DBUG_ENTER("handler::rename_table");
  for (const char **ext=bas_ext(); *ext ; ext++)
  {
    if (rename_file_ext(from,to,*ext))
      DBUG_RETURN(my_errno);
  }
  DBUG_RETURN(0);
}

/*
  Tell the handler to turn on or off logging to the handler's recovery log
*/

int ha_recovery_logging(THD *thd, bool on)
{
  int error=0;

  DBUG_ENTER("ha_recovery_logging");
  DBUG_RETURN(error);
}

int handler::index_next_same(byte *buf, const byte *key, uint keylen)
{
  int error;
  if (!(error=index_next(buf)))
  {
    if (key_cmp_if_same(table, key, active_index, keylen))
    {
      table->status=STATUS_NOT_FOUND;
      error=HA_ERR_END_OF_FILE;
    }
  }
  return error;
}


/****************************************************************************
** Some general functions that isn't in the handler class
****************************************************************************/

/*
  Initiates table-file and calls apropriate database-creator
  Returns 1 if something got wrong
*/

int ha_create_table(const char *name, HA_CREATE_INFO *create_info,
		    bool update_create_info)
{
  int error;
  TABLE table;
  char name_buff[FN_REFLEN];
  DBUG_ENTER("ha_create_table");

  if (openfrm(name,"",0,(uint) READ_ALL, 0, &table))
    DBUG_RETURN(1);
  if (update_create_info)
  {
    update_create_info_from_table(create_info, &table);
  }
  if (lower_case_table_names == 2 &&
      !(table.file->table_flags() & HA_FILE_BASED))
  {
    /* Ensure that handler gets name in lower case */
    strmov(name_buff, name);
    my_casedn_str(files_charset_info, name_buff);
    name= name_buff;
  }

  error=table.file->create(name,&table,create_info);
  VOID(closefrm(&table));
  if (error)
    my_error(ER_CANT_CREATE_TABLE,MYF(ME_BELL+ME_WAITTANG),name,error);
  DBUG_RETURN(error != 0);
}

static int NEAR_F delete_file(const char *name,const char *ext,int extflag)
{
  char buff[FN_REFLEN];
  VOID(fn_format(buff,name,"",ext,extflag | 4));
  return(my_delete_with_symlink(buff,MYF(MY_WME)));
}

void st_ha_check_opt::init()
{
  flags= sql_flags= 0;
  sort_buffer_size = current_thd->variables.myisam_sort_buff_size;
}


/*****************************************************************************
  Key cache handling.

  This code is only relevant for ISAM/MyISAM tables

  key_cache->cache may be 0 only in the case where a key cache is not
  initialized or when we where not able to init the key cache in a previous
  call to ha_init_key_cache() (probably out of memory)
*****************************************************************************/

/* Init a key cache if it has not been initied before */


int ha_init_key_cache(const char *name, KEY_CACHE *key_cache)
{
  DBUG_ENTER("ha_init_key_cache");

  if (!key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    long tmp_buff_size= (long) key_cache->param_buff_size;
    long tmp_block_size= (long) key_cache->param_block_size;
    uint division_limit= key_cache->param_division_limit;
    uint age_threshold=  key_cache->param_age_threshold;
    pthread_mutex_unlock(&LOCK_global_system_variables);
    DBUG_RETURN(!init_key_cache(key_cache,
				tmp_block_size,
				tmp_buff_size,
				division_limit, age_threshold));
  }
  DBUG_RETURN(0);
}


/* Resize key cache */

int ha_resize_key_cache(KEY_CACHE *key_cache)
{
  DBUG_ENTER("ha_resize_key_cache");

  if (key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    long tmp_buff_size= (long) key_cache->param_buff_size;
    long tmp_block_size= (long) key_cache->param_block_size;
    uint division_limit= key_cache->param_division_limit;
    uint age_threshold=  key_cache->param_age_threshold;
    pthread_mutex_unlock(&LOCK_global_system_variables);
    DBUG_RETURN(!resize_key_cache(key_cache, tmp_block_size,
				  tmp_buff_size,
				  division_limit, age_threshold));
  }
  DBUG_RETURN(0);
}


/* Change parameters for key cache (like size) */

int ha_change_key_cache_param(KEY_CACHE *key_cache)
{
  if (key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    uint division_limit= key_cache->param_division_limit;
    uint age_threshold=  key_cache->param_age_threshold;
    pthread_mutex_unlock(&LOCK_global_system_variables);
    change_key_cache_param(key_cache, division_limit, age_threshold);
  }
  return 0;
}

/* Free memory allocated by a key cache */

int ha_end_key_cache(KEY_CACHE *key_cache)
{
  end_key_cache(key_cache, 1);		// Can never fail
  return 0;
}

/* Move all tables from one key cache to another one */

int ha_change_key_cache(KEY_CACHE *old_key_cache,
			KEY_CACHE *new_key_cache)
{
  mi_change_key_cache(old_key_cache, new_key_cache);
  return 0;
}


/*
  Try to discover one table from handler(s)
*/

int ha_discover(const char* dbname, const char* name,
               const void** frmblob, uint* frmlen)
{
  int error= 1; // Table does not exist in any handler
  DBUG_ENTER("ha_discover");
  DBUG_PRINT("enter", ("db: %s, name: %s", dbname, name));
#ifdef HAVE_NDBCLUSTER_DB
  if (have_ndbcluster == SHOW_OPTION_YES)
    error= ndbcluster_discover(dbname, name, frmblob, frmlen);
#endif
  if (!error)
    statistic_increment(ha_discover_count,&LOCK_status);
  DBUG_RETURN(error);
}


/*
  Read first row between two ranges.
  Store ranges for future calls to read_range_next

  SYNOPSIS
    read_range_first()
    start_key		Start key. Is 0 if no min range
    end_key		End key.  Is 0 if no max range
    eq_range_arg	Set to 1 if start_key == end_key		
    sorted		Set to 1 if result should be sorted per key

  NOTES
    Record is read into table->record[0]

  RETURN
    0			Found row
    HA_ERR_END_OF_FILE	No rows in range
    #			Error code
*/

int handler::read_range_first(const key_range *start_key,
			      const key_range *end_key,
			      bool eq_range_arg, bool sorted)
{
  int result;
  DBUG_ENTER("handler::read_range_first");

  eq_range= eq_range_arg;
  end_range= 0;
  if (end_key)
  {
    end_range= &save_end_range;
    save_end_range= *end_key;
    key_compare_result_on_equal= ((end_key->flag == HA_READ_BEFORE_KEY) ? 1 :
				  (end_key->flag == HA_READ_AFTER_KEY) ? -1 : 0);
  }
  range_key_part= table->key_info[active_index].key_part;

  if (!start_key)			// Read first record
    result= index_first(table->record[0]);
  else
    result= index_read(table->record[0],
		       start_key->key,
		       start_key->length,
		       start_key->flag);
  if (result)
    DBUG_RETURN((result == HA_ERR_KEY_NOT_FOUND) 
		? HA_ERR_END_OF_FILE
		: result);

  DBUG_RETURN (compare_key(end_range) <= 0 ? 0 : HA_ERR_END_OF_FILE);
}


/*
  Read next row between two ranges.

  SYNOPSIS
    read_range_next()

  NOTES
    Record is read into table->record[0]

  RETURN
    0			Found row
    HA_ERR_END_OF_FILE	No rows in range
    #			Error code
*/

int handler::read_range_next()
{
  int result;
  DBUG_ENTER("handler::read_range_next");

  if (eq_range)
  {
    /* We trust that index_next_same always gives a row in range */
    DBUG_RETURN(index_next_same(table->record[0],
                                end_range->key,
                                end_range->length));
  }
  result= index_next(table->record[0]);
  if (result)
    DBUG_RETURN(result);
  DBUG_RETURN(compare_key(end_range) <= 0 ? 0 : HA_ERR_END_OF_FILE);
}


/*
  Compare if found key (in row) is over max-value

  SYNOPSIS
    compare_key
    range		range to compare to row. May be 0 for no range
 
  NOTES
    See key.cc::key_cmp() for details

  RETURN
    The return value is SIGN(key_in_row - range_key):

    0			Key is equal to range or 'range' == 0 (no range)
   -1			Key is less than range
    1			Key is larger than range
*/

int handler::compare_key(key_range *range)
{
  int cmp;
  if (!range)
    return 0;					// No max range
  cmp= key_cmp(range_key_part, range->key, range->length);
  if (!cmp)
    cmp= key_compare_result_on_equal;
  return cmp;
}

int handler::index_read_idx(byte * buf, uint index, const byte * key,
			     uint key_len, enum ha_rkey_function find_flag)
{
  int error= ha_index_init(index);
  if (!error)
    error= index_read(buf, key, key_len, find_flag);
  if (!error)
    error= ha_index_end();
  return error;
}

