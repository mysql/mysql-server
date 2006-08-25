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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "rpl_filter.h"


#include <myisampack.h>
#include <errno.h>

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
#define NDB_MAX_ATTRIBUTES_IN_TABLE 128
#include "ha_ndbcluster.h"
#endif

#ifdef WITH_PARTITION_STORAGE_ENGINE
#include "ha_partition.h"
#endif

/*
  While we have legacy_db_type, we have this array to
  check for dups and to find handlerton from legacy_db_type.
  Remove when legacy_db_type is finally gone
*/
st_plugin_int *hton2plugin[MAX_HA];

static handlerton *installed_htons[128];

#define BITMAP_STACKBUF_SIZE (128/8)

KEY_CREATE_INFO default_key_create_info= { HA_KEY_ALG_UNDEF, 0, {NullS,0} };

/* static functions defined in this file */

static handler *create_default(TABLE_SHARE *table, MEM_ROOT *mem_root);

static SHOW_COMP_OPTION have_yes= SHOW_OPTION_YES;

/* number of entries in handlertons[] */
ulong total_ha= 0;
/* number of storage engines (from handlertons[]) that support 2pc */
ulong total_ha_2pc= 0;
/* size of savepoint storage area (see ha_init) */
ulong savepoint_alloc_size= 0;

static const LEX_STRING sys_table_aliases[]=
{
  {(char*)STRING_WITH_LEN("INNOBASE")},  {(char*)STRING_WITH_LEN("INNODB")},
  {(char*)STRING_WITH_LEN("NDB")},       {(char*)STRING_WITH_LEN("NDBCLUSTER")},
  {(char*)STRING_WITH_LEN("HEAP")},      {(char*)STRING_WITH_LEN("MEMORY")},
  {(char*)STRING_WITH_LEN("MERGE")},     {(char*)STRING_WITH_LEN("MRG_MYISAM")},
  {NullS, 0}
};

const char *ha_row_type[] = {
  "", "FIXED", "DYNAMIC", "COMPRESSED", "REDUNDANT", "COMPACT", "?","?","?"
};

const char *tx_isolation_names[] =
{ "READ-UNCOMMITTED", "READ-COMMITTED", "REPEATABLE-READ", "SERIALIZABLE",
  NullS};
TYPELIB tx_isolation_typelib= {array_elements(tx_isolation_names)-1,"",
			       tx_isolation_names, NULL};

static TYPELIB known_extensions= {0,"known_exts", NULL, NULL};
uint known_extensions_id= 0;


/*
  Return the default storage engine handlerton for thread
  
  SYNOPSIS
    ha_default_handlerton(thd)
    thd         current thread
  
  RETURN
    pointer to handlerton
*/

handlerton *ha_default_handlerton(THD *thd)
{
  return (thd->variables.table_type != NULL) ?
          thd->variables.table_type :
          (global_system_variables.table_type != NULL ?
           global_system_variables.table_type : &myisam_hton);
}


/*
  Return the storage engine handlerton for the supplied name
  
  SYNOPSIS
    ha_resolve_by_name(thd, name)
    thd         current thread
    name        name of storage engine
  
  RETURN
    pointer to handlerton
*/

handlerton *ha_resolve_by_name(THD *thd, const LEX_STRING *name)
{
  const LEX_STRING *table_alias;
  st_plugin_int *plugin;

redo:
  /* my_strnncoll is a macro and gcc doesn't do early expansion of macro */
  if (thd && !my_charset_latin1.coll->strnncoll(&my_charset_latin1,
                           (const uchar *)name->str, name->length,
                           (const uchar *)STRING_WITH_LEN("DEFAULT"), 0))
    return ha_default_handlerton(thd);

  if ((plugin= plugin_lock(name, MYSQL_STORAGE_ENGINE_PLUGIN)))
  {
    handlerton *hton= (handlerton *)plugin->data;
    if (!(hton->flags & HTON_NOT_USER_SELECTABLE))
      return hton;
    plugin_unlock(plugin);
  }

  /*
    We check for the historical aliases.
  */
  for (table_alias= sys_table_aliases; table_alias->str; table_alias+= 2)
  {
    if (!my_strnncoll(&my_charset_latin1,
                      (const uchar *)name->str, name->length,
                      (const uchar *)table_alias->str, table_alias->length))
    {
      name= table_alias + 1;
      goto redo;
    }
  }

  return NULL;
}


const char *ha_get_storage_engine(enum legacy_db_type db_type)
{
  switch (db_type) {
  case DB_TYPE_DEFAULT:
    return "DEFAULT";
  default:
    if (db_type > DB_TYPE_UNKNOWN && db_type < DB_TYPE_DEFAULT &&
        installed_htons[db_type])
      return hton2plugin[installed_htons[db_type]->slot]->name.str;
    /* fall through */
  case DB_TYPE_UNKNOWN:
    return "UNKNOWN";
  }
}


static handler *create_default(TABLE_SHARE *table, MEM_ROOT *mem_root)
{
  handlerton *hton= ha_default_handlerton(current_thd);
  return (hton && hton->create) ? hton->create(table, mem_root) : NULL;
}


handlerton *ha_resolve_by_legacy_type(THD *thd, enum legacy_db_type db_type)
{
  switch (db_type) {
  case DB_TYPE_DEFAULT:
    return ha_default_handlerton(thd);
  case DB_TYPE_UNKNOWN:
    return NULL;
  default:
    if (db_type > DB_TYPE_UNKNOWN && db_type < DB_TYPE_DEFAULT)
      return installed_htons[db_type];
    return NULL;
  }
}


/* Use other database handler if databasehandler is not compiled in */

handlerton *ha_checktype(THD *thd, enum legacy_db_type database_type,
                          bool no_substitute, bool report_error)
{
  handlerton *hton= ha_resolve_by_legacy_type(thd, database_type);
  if (ha_storage_engine_is_enabled(hton))
    return hton;

  if (no_substitute)
  {
    if (report_error)
    {
      const char *engine_name= ha_get_storage_engine(database_type);
      my_error(ER_FEATURE_DISABLED,MYF(0),engine_name,engine_name);
    }
    return NULL;
  }

  switch (database_type) {
#ifndef NO_HASH
  case DB_TYPE_HASH:
    return ha_resolve_by_legacy_type(thd, DB_TYPE_HASH);
#endif
  case DB_TYPE_MRG_ISAM:
    return ha_resolve_by_legacy_type(thd, DB_TYPE_MRG_MYISAM);
  default:
    break;
  }

  return ha_default_handlerton(thd);
} /* ha_checktype */


handler *get_new_handler(TABLE_SHARE *share, MEM_ROOT *alloc,
                         handlerton *db_type)
{
  handler *file;
  DBUG_ENTER("get_new_handler");
  DBUG_PRINT("enter", ("alloc: 0x%lx", (long) alloc));

  if (db_type && db_type->state == SHOW_OPTION_YES && db_type->create)
  {
    if ((file= db_type->create(share, alloc)))
      file->init();
    DBUG_RETURN(file);
  }
  /*
    Try the default table type
    Here the call to current_thd() is ok as we call this function a lot of
    times but we enter this branch very seldom.
  */
  DBUG_RETURN(get_new_handler(share, alloc,
                              current_thd->variables.table_type));
}


#ifdef WITH_PARTITION_STORAGE_ENGINE
handler *get_ha_partition(partition_info *part_info)
{
  ha_partition *partition;
  DBUG_ENTER("get_ha_partition");
  if ((partition= new ha_partition(part_info)))
  {
    if (partition->initialise_partition(current_thd->mem_root))
    {
      delete partition;
      partition= 0;
    }
    else
      partition->init();
  }
  else
  {
    my_error(ER_OUTOFMEMORY, MYF(0), sizeof(ha_partition));
  }
  DBUG_RETURN(((handler*) partition));
}
#endif


/*
  Register handler error messages for use with my_error().

  SYNOPSIS
    ha_init_errors()

  RETURN
    0           OK
    != 0        Error
*/

static int ha_init_errors(void)
{
#define SETMSG(nr, msg) errmsgs[(nr) - HA_ERR_FIRST]= (msg)
  const char    **errmsgs;

  /* Allocate a pointer array for the error message strings. */
  /* Zerofill it to avoid uninitialized gaps. */
  if (! (errmsgs= (const char**) my_malloc(HA_ERR_ERRORS * sizeof(char*),
                                           MYF(MY_WME | MY_ZEROFILL))))
    return 1;

  /* Set the dedicated error messages. */
  SETMSG(HA_ERR_KEY_NOT_FOUND,          ER(ER_KEY_NOT_FOUND));
  SETMSG(HA_ERR_FOUND_DUPP_KEY,         ER(ER_DUP_KEY));
  SETMSG(HA_ERR_RECORD_CHANGED,         "Update wich is recoverable");
  SETMSG(HA_ERR_WRONG_INDEX,            "Wrong index given to function");
  SETMSG(HA_ERR_CRASHED,                ER(ER_NOT_KEYFILE));
  SETMSG(HA_ERR_WRONG_IN_RECORD,        ER(ER_CRASHED_ON_USAGE));
  SETMSG(HA_ERR_OUT_OF_MEM,             "Table handler out of memory");
  SETMSG(HA_ERR_NOT_A_TABLE,            "Incorrect file format '%.64s'");
  SETMSG(HA_ERR_WRONG_COMMAND,          "Command not supported");
  SETMSG(HA_ERR_OLD_FILE,               ER(ER_OLD_KEYFILE));
  SETMSG(HA_ERR_NO_ACTIVE_RECORD,       "No record read in update");
  SETMSG(HA_ERR_RECORD_DELETED,         "Intern record deleted");
  SETMSG(HA_ERR_RECORD_FILE_FULL,       ER(ER_RECORD_FILE_FULL));
  SETMSG(HA_ERR_INDEX_FILE_FULL,        "No more room in index file '%.64s'");
  SETMSG(HA_ERR_END_OF_FILE,            "End in next/prev/first/last");
  SETMSG(HA_ERR_UNSUPPORTED,            ER(ER_ILLEGAL_HA));
  SETMSG(HA_ERR_TO_BIG_ROW,             "Too big row");
  SETMSG(HA_WRONG_CREATE_OPTION,        "Wrong create option");
  SETMSG(HA_ERR_FOUND_DUPP_UNIQUE,      ER(ER_DUP_UNIQUE));
  SETMSG(HA_ERR_UNKNOWN_CHARSET,        "Can't open charset");
  SETMSG(HA_ERR_WRONG_MRG_TABLE_DEF,    ER(ER_WRONG_MRG_TABLE));
  SETMSG(HA_ERR_CRASHED_ON_REPAIR,      ER(ER_CRASHED_ON_REPAIR));
  SETMSG(HA_ERR_CRASHED_ON_USAGE,       ER(ER_CRASHED_ON_USAGE));
  SETMSG(HA_ERR_LOCK_WAIT_TIMEOUT,      ER(ER_LOCK_WAIT_TIMEOUT));
  SETMSG(HA_ERR_LOCK_TABLE_FULL,        ER(ER_LOCK_TABLE_FULL));
  SETMSG(HA_ERR_READ_ONLY_TRANSACTION,  ER(ER_READ_ONLY_TRANSACTION));
  SETMSG(HA_ERR_LOCK_DEADLOCK,          ER(ER_LOCK_DEADLOCK));
  SETMSG(HA_ERR_CANNOT_ADD_FOREIGN,     ER(ER_CANNOT_ADD_FOREIGN));
  SETMSG(HA_ERR_NO_REFERENCED_ROW,      ER(ER_NO_REFERENCED_ROW_2));
  SETMSG(HA_ERR_ROW_IS_REFERENCED,      ER(ER_ROW_IS_REFERENCED_2));
  SETMSG(HA_ERR_NO_SAVEPOINT,           "No savepoint with that name");
  SETMSG(HA_ERR_NON_UNIQUE_BLOCK_SIZE,  "Non unique key block size");
  SETMSG(HA_ERR_NO_SUCH_TABLE,          "No such table: '%.64s'");
  SETMSG(HA_ERR_TABLE_EXIST,            ER(ER_TABLE_EXISTS_ERROR));
  SETMSG(HA_ERR_NO_CONNECTION,          "Could not connect to storage engine");
  SETMSG(HA_ERR_TABLE_DEF_CHANGED,      ER(ER_TABLE_DEF_CHANGED));
  SETMSG(HA_ERR_FOREIGN_DUPLICATE_KEY,  "FK constraint would lead to duplicate key");
  SETMSG(HA_ERR_TABLE_NEEDS_UPGRADE,    ER(ER_TABLE_NEEDS_UPGRADE));
  SETMSG(HA_ERR_TABLE_READONLY,         ER(ER_OPEN_AS_READONLY));

  /* Register the error messages for use with my_error(). */
  return my_error_register(errmsgs, HA_ERR_FIRST, HA_ERR_LAST);
}


/*
  Unregister handler error messages.

  SYNOPSIS
    ha_finish_errors()

  RETURN
    0           OK
    != 0        Error
*/

static int ha_finish_errors(void)
{
  const char    **errmsgs;

  /* Allocate a pointer array for the error message strings. */
  if (! (errmsgs= my_error_unregister(HA_ERR_FIRST, HA_ERR_LAST)))
    return 1;
  my_free((gptr) errmsgs, MYF(0));
  return 0;
}


int ha_finalize_handlerton(st_plugin_int *plugin)
{
  handlerton *hton= (handlerton *)plugin->data;
  DBUG_ENTER("ha_finalize_handlerton");

  switch (hton->state)
  {
  case SHOW_OPTION_NO:
  case SHOW_OPTION_DISABLED:
    break;
  case SHOW_OPTION_YES:
    if (installed_htons[hton->db_type] == hton)
      installed_htons[hton->db_type]= NULL;
    if (hton->panic && hton->panic(HA_PANIC_CLOSE))
      DBUG_RETURN(1);
    break;
  };
  DBUG_RETURN(0);
}


int ha_initialize_handlerton(st_plugin_int *plugin)
{
  handlerton *hton= ((st_mysql_storage_engine *)plugin->plugin->info)->handlerton;
  DBUG_ENTER("ha_initialize_handlerton");

  plugin->data= hton; // shortcut for the future

  /*
    the switch below and hton->state should be removed when
    command-line options for plugins will be implemented
  */
  switch (hton->state) {
  case SHOW_OPTION_NO:
    break;
  case SHOW_OPTION_YES:
    {
      uint tmp;
      /* now check the db_type for conflict */
      if (hton->db_type <= DB_TYPE_UNKNOWN ||
          hton->db_type >= DB_TYPE_DEFAULT ||
          installed_htons[hton->db_type])
      {
        int idx= (int) DB_TYPE_FIRST_DYNAMIC;

        while (idx < (int) DB_TYPE_DEFAULT && installed_htons[idx])
          idx++;

        if (idx == (int) DB_TYPE_DEFAULT)
        {
          sql_print_warning("Too many storage engines!");
          DBUG_RETURN(1);
        }
        if (hton->db_type != DB_TYPE_UNKNOWN)
          sql_print_warning("Storage engine '%s' has conflicting typecode. "
                            "Assigning value %d.", plugin->plugin->name, idx);
        hton->db_type= (enum legacy_db_type) idx;
      }
      installed_htons[hton->db_type]= hton;
      tmp= hton->savepoint_offset;
      hton->savepoint_offset= savepoint_alloc_size;
      savepoint_alloc_size+= tmp;
      hton->slot= total_ha++;
      hton2plugin[hton->slot]=plugin;
      /* This is just a temp need until plugin/engine startup is fixed */
      if (plugin->plugin->status_vars)
      {
        add_status_vars(plugin->plugin->status_vars);
      }

      if (hton->prepare)
        total_ha_2pc++;
      break;
    }
    /* fall through */
  default:
    hton->state= SHOW_OPTION_DISABLED;
    break;
  }
  DBUG_RETURN(0);
}

int ha_init()
{
  int error= 0;
  DBUG_ENTER("ha_init");

  if (ha_init_errors())
    DBUG_RETURN(1);

  DBUG_ASSERT(total_ha < MAX_HA);
  /*
    Check if there is a transaction-capable storage engine besides the
    binary log (which is considered a transaction-capable storage engine in
    counting total_ha)
  */
  opt_using_transactions= total_ha>(ulong)opt_bin_log;
  savepoint_alloc_size+= sizeof(SAVEPOINT);
  DBUG_RETURN(error);
}

/*
  close, flush or restart databases
  Ignore this for other databases than ours
*/

static my_bool panic_handlerton(THD *unused1, st_plugin_int *plugin, void *arg)
{
  handlerton *hton= (handlerton *)plugin->data;
  if (hton->state == SHOW_OPTION_YES && hton->panic)
    ((int*)arg)[0]|= hton->panic((enum ha_panic_function)((int*)arg)[1]);
  return FALSE;
}


int ha_panic(enum ha_panic_function flag)
{
  int error[2];

  error[0]= 0; error[1]= (int)flag;
  plugin_foreach(NULL, panic_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, error);

  if (flag == HA_PANIC_CLOSE && ha_finish_errors())
    error[0]= 1;
  return error[0];
} /* ha_panic */

static my_bool dropdb_handlerton(THD *unused1, st_plugin_int *plugin,
                                 void *path)
{
  handlerton *hton= (handlerton *)plugin->data;
  if (hton->state == SHOW_OPTION_YES && hton->drop_database)
    hton->drop_database((char *)path);
  return FALSE;
}


void ha_drop_database(char* path)
{
  plugin_foreach(NULL, dropdb_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, path);
}


static my_bool closecon_handlerton(THD *thd, st_plugin_int *plugin,
                                   void *unused)
{
  handlerton *hton= (handlerton *)plugin->data;
  /*
    there's no need to rollback here as all transactions must
    be rolled back already
  */
  if (hton->state == SHOW_OPTION_YES && hton->close_connection &&
      thd->ha_data[hton->slot])
    hton->close_connection(thd);
  return FALSE;
}


/* don't bother to rollback here, it's done already */
void ha_close_connection(THD* thd)
{
  plugin_foreach(thd, closecon_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, 0);
}

/* ========================================================================
 ======================= TRANSACTIONS ===================================*/

/*
  Register a storage engine for a transaction

  DESCRIPTION
    Every storage engine MUST call this function when it starts
    a transaction or a statement (that is it must be called both for the
    "beginning of transaction" and "beginning of statement").
    Only storage engines registered for the transaction/statement
    will know when to commit/rollback it.

  NOTE
    trans_register_ha is idempotent - storage engine may register many
    times per transaction.

*/
void trans_register_ha(THD *thd, bool all, handlerton *ht_arg)
{
  THD_TRANS *trans;
  handlerton **ht;
  DBUG_ENTER("trans_register_ha");
  DBUG_PRINT("enter",("%s", all ? "all" : "stmt"));

  if (all)
  {
    trans= &thd->transaction.all;
    thd->server_status|= SERVER_STATUS_IN_TRANS;
  }
  else
    trans= &thd->transaction.stmt;

  for (ht=trans->ht; *ht; ht++)
    if (*ht == ht_arg)
      DBUG_VOID_RETURN;  /* already registered, return */

  trans->ht[trans->nht++]=ht_arg;
  DBUG_ASSERT(*ht == ht_arg);
  trans->no_2pc|=(ht_arg->prepare==0);
  if (thd->transaction.xid_state.xid.is_null())
    thd->transaction.xid_state.xid.set(thd->query_id);
  DBUG_VOID_RETURN;
}

/*
  RETURN
      0  - ok
      1  - error, transaction was rolled back
*/
int ha_prepare(THD *thd)
{
  int error=0, all=1;
  THD_TRANS *trans=all ? &thd->transaction.all : &thd->transaction.stmt;
  handlerton **ht=trans->ht;
  DBUG_ENTER("ha_prepare");
#ifdef USING_TRANSACTIONS
  if (trans->nht)
  {
    for (; *ht; ht++)
    {
      int err;
      statistic_increment(thd->status_var.ha_prepare_count,&LOCK_status);
      if ((*ht)->prepare)
      {
        if ((err= (*(*ht)->prepare)(thd, all)))
        {
          my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
          ha_rollback_trans(thd, all);
          error=1;
          break;
        }
      }
      else
      {
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                            hton2plugin[(*ht)->slot]->name.str);
      }
    }
  }
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}

/*
  RETURN
      0  - ok
      1  - transaction was rolled back
      2  - error during commit, data may be inconsistent
*/
int ha_commit_trans(THD *thd, bool all)
{
  int error= 0, cookie= 0;
  THD_TRANS *trans= all ? &thd->transaction.all : &thd->transaction.stmt;
  bool is_real_trans= all || thd->transaction.all.nht == 0;
  handlerton **ht= trans->ht;
  my_xid xid= thd->transaction.xid_state.xid.get_my_xid();
  DBUG_ENTER("ha_commit_trans");

  if (thd->in_sub_stmt)
  {
    /*
      Since we don't support nested statement transactions in 5.0,
      we can't commit or rollback stmt transactions while we are inside
      stored functions or triggers. So we simply do nothing now.
      TODO: This should be fixed in later ( >= 5.1) releases.
    */
    if (!all)
      DBUG_RETURN(0);
    /*
      We assume that all statements which commit or rollback main transaction
      are prohibited inside of stored functions or triggers. So they should
      bail out with error even before ha_commit_trans() call. To be 100% safe
      let us throw error in non-debug builds.
    */
    DBUG_ASSERT(0);
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    DBUG_RETURN(2);
  }
#ifdef USING_TRANSACTIONS
  if (trans->nht)
  {
    if (is_real_trans && wait_if_global_read_lock(thd, 0, 0))
    {
      ha_rollback_trans(thd, all);
      DBUG_RETURN(1);
    }
    DBUG_EXECUTE_IF("crash_commit_before", abort(););

    /* Close all cursors that can not survive COMMIT */
    if (is_real_trans)                          /* not a statement commit */
      thd->stmt_map.close_transient_cursors();

    if (!trans->no_2pc && trans->nht > 1)
    {
      for (; *ht && !error; ht++)
      {
        int err;
        if ((err= (*(*ht)->prepare)(thd, all)))
        {
          my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
          error= 1;
        }
        statistic_increment(thd->status_var.ha_prepare_count,&LOCK_status);
      }
      DBUG_EXECUTE_IF("crash_commit_after_prepare", abort(););
      if (error || (is_real_trans && xid &&
                    (error= !(cookie= tc_log->log(thd, xid)))))
      {
        ha_rollback_trans(thd, all);
        error= 1;
        goto end;
      }
      DBUG_EXECUTE_IF("crash_commit_after_log", abort(););
    }
    error=ha_commit_one_phase(thd, all) ? cookie ? 2 : 1 : 0;
    DBUG_EXECUTE_IF("crash_commit_before_unlog", abort(););
    if (cookie)
      tc_log->unlog(cookie, xid);
    DBUG_EXECUTE_IF("crash_commit_after", abort(););
end:
    if (is_real_trans)
      start_waiting_global_read_lock(thd);
  }
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}

/*
  NOTE - this function does not care about global read lock.
  A caller should.
*/
int ha_commit_one_phase(THD *thd, bool all)
{
  int error=0;
  THD_TRANS *trans=all ? &thd->transaction.all : &thd->transaction.stmt;
  bool is_real_trans=all || thd->transaction.all.nht == 0;
  handlerton **ht=trans->ht;
  DBUG_ENTER("ha_commit_one_phase");
#ifdef USING_TRANSACTIONS
  if (trans->nht)
  {
    for (ht=trans->ht; *ht; ht++)
    {
      int err;
      if ((err= (*(*ht)->commit)(thd, all)))
      {
        my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
        error=1;
      }
      statistic_increment(thd->status_var.ha_commit_count,&LOCK_status);
      *ht= 0;
    }
    trans->nht=0;
    trans->no_2pc=0;
    if (is_real_trans)
      thd->transaction.xid_state.xid.null();
    if (all)
    {
#ifdef HAVE_QUERY_CACHE
      if (thd->transaction.changed_tables)
        query_cache.invalidate(thd->transaction.changed_tables);
#endif
      thd->variables.tx_isolation=thd->session_tx_isolation;
      thd->transaction.cleanup();
    }
  }
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}


int ha_rollback_trans(THD *thd, bool all)
{
  int error=0;
  THD_TRANS *trans=all ? &thd->transaction.all : &thd->transaction.stmt;
  bool is_real_trans=all || thd->transaction.all.nht == 0;
  DBUG_ENTER("ha_rollback_trans");
  if (thd->in_sub_stmt)
  {
    /*
      If we are inside stored function or trigger we should not commit or
      rollback current statement transaction. See comment in ha_commit_trans()
      call for more information.
    */
    if (!all)
      DBUG_RETURN(0);
    DBUG_ASSERT(0);
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    DBUG_RETURN(1);
  }
#ifdef USING_TRANSACTIONS
  if (trans->nht)
  {
    /* Close all cursors that can not survive ROLLBACK */
    if (is_real_trans)                          /* not a statement commit */
      thd->stmt_map.close_transient_cursors();

    for (handlerton **ht=trans->ht; *ht; ht++)
    {
      int err;
      if ((err= (*(*ht)->rollback)(thd, all)))
      { // cannot happen
        my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
        error=1;
      }
      statistic_increment(thd->status_var.ha_rollback_count,&LOCK_status);
      *ht= 0;
    }
    trans->nht=0;
    trans->no_2pc=0;
    if (is_real_trans)
      thd->transaction.xid_state.xid.null();
    if (all)
    {
      thd->variables.tx_isolation=thd->session_tx_isolation;
      thd->transaction.cleanup();
    }
  }
#endif /* USING_TRANSACTIONS */
  /*
    If a non-transactional table was updated, warn; don't warn if this is a
    slave thread (because when a slave thread executes a ROLLBACK, it has
    been read from the binary log, so it's 100% sure and normal to produce
    error ER_WARNING_NOT_COMPLETE_ROLLBACK. If we sent the warning to the
    slave SQL thread, it would not stop the thread but just be printed in
    the error log; but we don't want users to wonder why they have this
    message in the error log, so we don't send it.
  */
  if (is_real_trans && (thd->options & OPTION_STATUS_NO_TRANS_UPDATE) &&
      !thd->slave_thread)
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
  DBUG_RETURN(error);
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
  if (thd->transaction.stmt.nht)
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


struct xahton_st {
  XID *xid;
  int result;
};

static my_bool xacommit_handlerton(THD *unused1, st_plugin_int *plugin,
                                   void *arg)
{
  handlerton *hton= (handlerton *)plugin->data;
  if (hton->state == SHOW_OPTION_YES && hton->recover)
  {
    hton->commit_by_xid(((struct xahton_st *)arg)->xid);
    ((struct xahton_st *)arg)->result= 0;
  }
  return FALSE;
}

static my_bool xarollback_handlerton(THD *unused1, st_plugin_int *plugin,
                                     void *arg)
{
  handlerton *hton= (handlerton *)plugin->data;
  if (hton->state == SHOW_OPTION_YES && hton->recover)
  {
    hton->rollback_by_xid(((struct xahton_st *)arg)->xid);
    ((struct xahton_st *)arg)->result= 0;
  }
  return FALSE;
}


int ha_commit_or_rollback_by_xid(XID *xid, bool commit)
{
  struct xahton_st xaop;
  xaop.xid= xid;
  xaop.result= 1;

  plugin_foreach(NULL, commit ? xacommit_handlerton : xarollback_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &xaop);

  return xaop.result;
}


#ifndef DBUG_OFF
/* this does not need to be multi-byte safe or anything */
static char* xid_to_str(char *buf, XID *xid)
{
  int i;
  char *s=buf;
  *s++='\'';
  for (i=0; i < xid->gtrid_length+xid->bqual_length; i++)
  {
    uchar c=(uchar)xid->data[i];
    /* is_next_dig is set if next character is a number */
    bool is_next_dig= FALSE;
    if (i < XIDDATASIZE)
    {
      char ch= xid->data[i+1];
      is_next_dig= (ch >= '0' && ch <='9');
    }
    if (i == xid->gtrid_length)
    {
      *s++='\'';
      if (xid->bqual_length)
      {
        *s++='.';
        *s++='\'';
      }
    }
    if (c < 32 || c > 126)
    {
      *s++='\\';
      /*
        If next character is a number, write current character with
        3 octal numbers to ensure that the next number is not seen
        as part of the octal number
      */
      if (c > 077 || is_next_dig)
        *s++=_dig_vec_lower[c >> 6];
      if (c > 007 || is_next_dig)
        *s++=_dig_vec_lower[(c >> 3) & 7];
      *s++=_dig_vec_lower[c & 7];
    }
    else
    {
      if (c == '\'' || c == '\\')
        *s++='\\';
      *s++=c;
    }
  }
  *s++='\'';
  *s=0;
  return buf;
}
#endif

/*
  recover() step of xa

  NOTE
   there are three modes of operation:

   - automatic recover after a crash
     in this case commit_list != 0, tc_heuristic_recover==0
     all xids from commit_list are committed, others are rolled back

   - manual (heuristic) recover
     in this case commit_list==0, tc_heuristic_recover != 0
     DBA has explicitly specified that all prepared transactions should
     be committed (or rolled back).

   - no recovery (MySQL did not detect a crash)
     in this case commit_list==0, tc_heuristic_recover == 0
     there should be no prepared transactions in this case.
*/

struct xarecover_st
{
  int len, found_foreign_xids, found_my_xids;
  XID *list;
  HASH *commit_list;
  bool dry_run;
};

static my_bool xarecover_handlerton(THD *unused, st_plugin_int *plugin,
                                    void *arg)
{
  handlerton *hton= (handlerton *)plugin->data;
  struct xarecover_st *info= (struct xarecover_st *) arg;
  int got;

  if (hton->state == SHOW_OPTION_YES && hton->recover)
  {
    while ((got= hton->recover(info->list, info->len)) > 0 )
    {
      sql_print_information("Found %d prepared transaction(s) in %s",
                            got, hton2plugin[hton->slot]->name.str);
      for (int i=0; i < got; i ++)
      {
        my_xid x=info->list[i].get_my_xid();
        if (!x) // not "mine" - that is generated by external TM
        {
#ifndef DBUG_OFF
          char buf[XIDDATASIZE*4+6]; // see xid_to_str
          sql_print_information("ignore xid %s", xid_to_str(buf, info->list+i));
#endif
          xid_cache_insert(info->list+i, XA_PREPARED);
          info->found_foreign_xids++;
          continue;
        }
        if (info->dry_run)
        {
          info->found_my_xids++;
          continue;
        }
        // recovery mode
        if (info->commit_list ?
            hash_search(info->commit_list, (byte *)&x, sizeof(x)) != 0 :
            tc_heuristic_recover == TC_HEURISTIC_RECOVER_COMMIT)
        {
#ifndef DBUG_OFF
          char buf[XIDDATASIZE*4+6]; // see xid_to_str
          sql_print_information("commit xid %s", xid_to_str(buf, info->list+i));
#endif
          hton->commit_by_xid(info->list+i);
        }
        else
        {
#ifndef DBUG_OFF
          char buf[XIDDATASIZE*4+6]; // see xid_to_str
          sql_print_information("rollback xid %s",
                                xid_to_str(buf, info->list+i));
#endif
          hton->rollback_by_xid(info->list+i);
        }
      }
      if (got < info->len)
        break;
    }
  }
  return FALSE;
}

int ha_recover(HASH *commit_list)
{
  struct xarecover_st info;
  DBUG_ENTER("ha_recover");
  info.found_foreign_xids= info.found_my_xids= 0;
  info.commit_list= commit_list;
  info.dry_run= (info.commit_list==0 && tc_heuristic_recover==0);
  info.list= NULL;

  /* commit_list and tc_heuristic_recover cannot be set both */
  DBUG_ASSERT(info.commit_list==0 || tc_heuristic_recover==0);
  /* if either is set, total_ha_2pc must be set too */
  DBUG_ASSERT(info.dry_run || total_ha_2pc>(ulong)opt_bin_log);

  if (total_ha_2pc <= (ulong)opt_bin_log)
    DBUG_RETURN(0);

  if (info.commit_list)
    sql_print_information("Starting crash recovery...");

#ifndef WILL_BE_DELETED_LATER
  /*
    for now, only InnoDB supports 2pc. It means we can always safely
    rollback all pending transactions, without risking inconsistent data
  */
  DBUG_ASSERT(total_ha_2pc == (ulong) opt_bin_log+1); // only InnoDB and binlog
  tc_heuristic_recover= TC_HEURISTIC_RECOVER_ROLLBACK; // forcing ROLLBACK
  info.dry_run=FALSE;
#endif

  for (info.len= MAX_XID_LIST_SIZE ; 
       info.list==0 && info.len > MIN_XID_LIST_SIZE; info.len/=2)
  {
    info.list=(XID *)my_malloc(info.len*sizeof(XID), MYF(0));
  }
  if (!info.list)
  {
    sql_print_error(ER(ER_OUTOFMEMORY), info.len*sizeof(XID));
    DBUG_RETURN(1);
  }

  plugin_foreach(NULL, xarecover_handlerton, 
                 MYSQL_STORAGE_ENGINE_PLUGIN, &info);

  my_free((gptr)info.list, MYF(0));
  if (info.found_foreign_xids)
    sql_print_warning("Found %d prepared XA transactions", 
                      info.found_foreign_xids);
  if (info.dry_run && info.found_my_xids)
  {
    sql_print_error("Found %d prepared transactions! It means that mysqld was "
                    "not shut down properly last time and critical recovery "
                    "information (last binlog or %s file) was manually deleted "
                    "after a crash. You have to start mysqld with "
                    "--tc-heuristic-recover switch to commit or rollback "
                    "pending transactions.",
                    info.found_my_xids, opt_tc_log_file);
    DBUG_RETURN(1);
  }
  if (info.commit_list)
    sql_print_information("Crash recovery finished.");
  DBUG_RETURN(0);
}

/*
  return the list of XID's to a client, the same way SHOW commands do

  NOTE
    I didn't find in XA specs that an RM cannot return the same XID twice,
    so mysql_xa_recover does not filter XID's to ensure uniqueness.
    It can be easily fixed later, if necessary.
*/
bool mysql_xa_recover(THD *thd)
{
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  int i=0;
  XID_STATE *xs;
  DBUG_ENTER("mysql_xa_recover");

  field_list.push_back(new Item_int("formatID",0,11));
  field_list.push_back(new Item_int("gtrid_length",0,11));
  field_list.push_back(new Item_int("bqual_length",0,11));
  field_list.push_back(new Item_empty_string("data",XIDDATASIZE));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(1);

  pthread_mutex_lock(&LOCK_xid_cache);
  while ((xs= (XID_STATE*)hash_element(&xid_cache, i++)))
  {
    if (xs->xa_state==XA_PREPARED)
    {
      protocol->prepare_for_resend();
      protocol->store_longlong((longlong)xs->xid.formatID, FALSE);
      protocol->store_longlong((longlong)xs->xid.gtrid_length, FALSE);
      protocol->store_longlong((longlong)xs->xid.bqual_length, FALSE);
      protocol->store(xs->xid.data, xs->xid.gtrid_length+xs->xid.bqual_length,
                      &my_charset_bin);
      if (protocol->write())
      {
        pthread_mutex_unlock(&LOCK_xid_cache);
        DBUG_RETURN(1);
      }
    }
  }

  pthread_mutex_unlock(&LOCK_xid_cache);
  send_eof(thd);
  DBUG_RETURN(0);
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

static my_bool release_temporary_latches(THD *thd, st_plugin_int *plugin,
                                 void *unused)
{
  handlerton *hton= (handlerton *)plugin->data;

  if (hton->state == SHOW_OPTION_YES && hton->release_temporary_latches)
    hton->release_temporary_latches(thd);

  return FALSE;
}


int ha_release_temporary_latches(THD *thd)
{
  plugin_foreach(thd, release_temporary_latches, MYSQL_STORAGE_ENGINE_PLUGIN, 
                 NULL);

  return 0;
}

int ha_rollback_to_savepoint(THD *thd, SAVEPOINT *sv)
{
  int error=0;
  THD_TRANS *trans= (thd->in_sub_stmt ? &thd->transaction.stmt :
                                        &thd->transaction.all);
  handlerton **ht=trans->ht, **end_ht;
  DBUG_ENTER("ha_rollback_to_savepoint");

  trans->nht=sv->nht;
  trans->no_2pc=0;
  end_ht=ht+sv->nht;
  /*
    rolling back to savepoint in all storage engines that were part of the
    transaction when the savepoint was set
  */
  for (; ht < end_ht; ht++)
  {
    int err;
    DBUG_ASSERT((*ht)->savepoint_set != 0);
    if ((err= (*(*ht)->savepoint_rollback)(thd, (byte *)(sv+1)+(*ht)->savepoint_offset)))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error=1;
    }
    statistic_increment(thd->status_var.ha_savepoint_rollback_count,
                        &LOCK_status);
    trans->no_2pc|=(*ht)->prepare == 0;
  }
  /*
    rolling back the transaction in all storage engines that were not part of
    the transaction when the savepoint was set
  */
  for (; *ht ; ht++)
  {
    int err;
    if ((err= (*(*ht)->rollback)(thd, !thd->in_sub_stmt)))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error=1;
    }
    statistic_increment(thd->status_var.ha_rollback_count,&LOCK_status);
    *ht=0; // keep it conveniently zero-filled
  }
  DBUG_RETURN(error);
}

/*
  note, that according to the sql standard (ISO/IEC 9075-2:2003)
  section "4.33.4 SQL-statements and transaction states",
  SAVEPOINT is *not* transaction-initiating SQL-statement
*/

int ha_savepoint(THD *thd, SAVEPOINT *sv)
{
  int error=0;
  THD_TRANS *trans= (thd->in_sub_stmt ? &thd->transaction.stmt :
                                        &thd->transaction.all);
  handlerton **ht=trans->ht;
  DBUG_ENTER("ha_savepoint");
#ifdef USING_TRANSACTIONS
  for (; *ht; ht++)
  {
    int err;
    if (! (*ht)->savepoint_set)
    {
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "SAVEPOINT");
      error=1;
      break;
    }
    if ((err= (*(*ht)->savepoint_set)(thd, (byte *)(sv+1)+(*ht)->savepoint_offset)))
    { // cannot happen
      my_error(ER_GET_ERRNO, MYF(0), err);
      error=1;
    }
    statistic_increment(thd->status_var.ha_savepoint_count,&LOCK_status);
  }
  sv->nht=trans->nht;
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}

int ha_release_savepoint(THD *thd, SAVEPOINT *sv)
{
  int error=0;
  THD_TRANS *trans= (thd->in_sub_stmt ? &thd->transaction.stmt :
                                        &thd->transaction.all);
  handlerton **ht=trans->ht, **end_ht;
  DBUG_ENTER("ha_release_savepoint");

  end_ht=ht+sv->nht;
  for (; ht < end_ht; ht++)
  {
    int err;
    if (!(*ht)->savepoint_release)
      continue;
    if ((err= (*(*ht)->savepoint_release)(thd, (byte *)(sv+1)+(*ht)->savepoint_offset)))
    { // cannot happen
      my_error(ER_GET_ERRNO, MYF(0), err);
      error=1;
    }
  }
  DBUG_RETURN(error);
}


static my_bool snapshot_handlerton(THD *thd, st_plugin_int *plugin,
                                   void *arg)
{
  handlerton *hton= (handlerton *)plugin->data;
  if (hton->state == SHOW_OPTION_YES &&
      hton->start_consistent_snapshot)
  {
    hton->start_consistent_snapshot(thd);
    *((bool *)arg)= false;
  }
  return FALSE;
}

int ha_start_consistent_snapshot(THD *thd)
{
  bool warn= true;

  plugin_foreach(thd, snapshot_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, &warn);

  /*
    Same idea as when one wants to CREATE TABLE in one engine which does not
    exist:
  */
  if (warn)
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                 "This MySQL server does not support any "
                 "consistent-read capable storage engine");
  return 0;
}


static my_bool flush_handlerton(THD *thd, st_plugin_int *plugin,
                                void *arg)
{
  handlerton *hton= (handlerton *)plugin->data;
  if (hton->state == SHOW_OPTION_YES && hton->flush_logs && hton->flush_logs())
    return TRUE;
  return FALSE;
}


bool ha_flush_logs(handlerton *db_type)
{
  if (db_type == NULL)
  {
    if (plugin_foreach(NULL, flush_handlerton,
                          MYSQL_STORAGE_ENGINE_PLUGIN, 0))
      return TRUE;
  }
  else
  {
    if (db_type->state != SHOW_OPTION_YES ||
        (db_type->flush_logs && db_type->flush_logs()))
      return TRUE;
  }
  return FALSE;
}

/*
  This should return ENOENT if the file doesn't exists.
  The .frm file will be deleted only if we return 0 or ENOENT
*/

int ha_delete_table(THD *thd, handlerton *table_type, const char *path,
                    const char *db, const char *alias, bool generate_warning)
{
  handler *file;
  char tmp_path[FN_REFLEN];
  int error;
  TABLE dummy_table;
  TABLE_SHARE dummy_share;
  DBUG_ENTER("ha_delete_table");

  bzero((char*) &dummy_table, sizeof(dummy_table));
  bzero((char*) &dummy_share, sizeof(dummy_share));
  dummy_table.s= &dummy_share;

  /* DB_TYPE_UNKNOWN is used in ALTER TABLE when renaming only .frm files */
  if (table_type == NULL ||
      ! (file=get_new_handler(&dummy_share, thd->mem_root, table_type)))
    DBUG_RETURN(ENOENT);

  if (lower_case_table_names == 2 && !(file->ha_table_flags() & HA_FILE_BASED))
  {
    /* Ensure that table handler get path in lower case */
    strmov(tmp_path, path);
    my_casedn_str(files_charset_info, tmp_path);
    path= tmp_path;
  }
  if ((error= file->delete_table(path)) && generate_warning)
  {
    /*
      Because file->print_error() use my_error() to generate the error message
      we must store the error state in thd, reset it and restore it to
      be able to get hold of the error message.
      (We should in the future either rewrite handler::print_error() or make
      a nice method of this.
    */
    bool query_error= thd->query_error;
    sp_rcontext *spcont= thd->spcont;
    SELECT_LEX *current_select= thd->lex->current_select;
    char buff[sizeof(thd->net.last_error)];
    char new_error[sizeof(thd->net.last_error)];
    int last_errno= thd->net.last_errno;

    strmake(buff, thd->net.last_error, sizeof(buff)-1);
    thd->query_error= 0;
    thd->spcont= 0;
    thd->lex->current_select= 0;
    thd->net.last_error[0]= 0;

    /* Fill up strucutures that print_error may need */
    dummy_share.path.str= (char*) path;
    dummy_share.path.length= strlen(path);
    dummy_share.db.str= (char*) db;
    dummy_share.db.length= strlen(db);
    dummy_share.table_name.str= (char*) alias;
    dummy_share.table_name.length= strlen(alias);
    dummy_table.alias= alias;

    file->print_error(error, 0);
    strmake(new_error, thd->net.last_error, sizeof(buff)-1);

    /* restore thd */
    thd->query_error= query_error;
    thd->spcont= spcont;
    thd->lex->current_select= current_select;
    thd->net.last_errno= last_errno;
    strmake(thd->net.last_error, buff, sizeof(buff)-1);
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR, error, new_error);
  }
  delete file;
  DBUG_RETURN(error);
}

/****************************************************************************
** General handler functions
****************************************************************************/


void handler::ha_statistic_increment(ulong SSV::*offset) const
{
  statistic_increment(table->in_use->status_var.*offset, &LOCK_status);
}


bool handler::check_if_log_table_locking_is_allowed(uint sql_command,
                                                    ulong type, TABLE *table)
{
  /*
    Deny locking of the log tables, which is incompatible with
    concurrent insert. Unless called from a logger THD:
    general_log_thd or slow_log_thd.
  */
  if (table->s->log_table &&
      sql_command != SQLCOM_TRUNCATE &&
      sql_command != SQLCOM_ALTER_TABLE &&
      !(sql_command == SQLCOM_FLUSH &&
        type & REFRESH_LOG) &&
      (table->reginfo.lock_type >= TL_READ_NO_INSERT))
  {
    /*
      The check  >= TL_READ_NO_INSERT denies all write locks
      plus the only read lock (TL_READ_NO_INSERT itself)
    */
    table->reginfo.lock_type == TL_READ_NO_INSERT ?
      my_error(ER_CANT_READ_LOCK_LOG_TABLE, MYF(0)) :
        my_error(ER_CANT_WRITE_LOCK_LOG_TABLE, MYF(0));
    return FALSE;
  }
  return TRUE;
}

/*
  Open database-handler.

  IMPLEMENTATION
    Try O_RDONLY if cannot open as O_RDWR
    Don't wait for locks if not HA_OPEN_WAIT_IF_LOCKED is set
*/

int handler::ha_open(TABLE *table_arg, const char *name, int mode,
                     int test_if_locked)
{
  int error;
  DBUG_ENTER("handler::ha_open");
  DBUG_PRINT("enter",
             ("name: %s  db_type: %d  db_stat: %d  mode: %d  lock_test: %d",
              name, table_share->db_type, table_arg->db_stat, mode,
              test_if_locked));

  table= table_arg;
  DBUG_ASSERT(table->s == table_share);
  DBUG_ASSERT(alloc_root_inited(&table->mem_root));

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
    my_errno= error;                            /* Safeguard */
    DBUG_PRINT("error",("error: %d  errno: %d",error,errno));
  }
  else
  {
    if (table->s->db_options_in_use & HA_OPTION_READ_ONLY_DATA)
      table->db_stat|=HA_READ_ONLY;
    (void) extra(HA_EXTRA_NO_READCHECK);	// Not needed in SQL

    if (!(ref= (byte*) alloc_root(&table->mem_root, ALIGN_SIZE(ref_length)*2)))
    {
      close();
      error=HA_ERR_OUT_OF_MEM;
    }
    else
      dup_ref=ref+ALIGN_SIZE(ref_length);
    cached_table_flags= table_flags();
  }
  DBUG_RETURN(error);
}


/*
  Read first row (only) from a table
  This is never called for InnoDB tables, as these table types
  has the HA_STATS_RECORDS_IS_EXACT set.
*/

int handler::read_first_row(byte * buf, uint primary_key)
{
  register int error;
  DBUG_ENTER("handler::read_first_row");

  statistic_increment(table->in_use->status_var.ha_read_first_count,
                      &LOCK_status);

  /*
    If there is very few deleted rows in the table, find the first row by
    scanning the table.
    TODO remove the test for HA_READ_ORDER
  */
  if (stats.deleted < 10 || primary_key >= MAX_KEY ||
      !(index_flags(primary_key, 0, 0) & HA_READ_ORDER))
  {
    (void) ha_rnd_init(1);
    while ((error= rnd_next(buf)) == HA_ERR_RECORD_DELETED) ;
    (void) ha_rnd_end();
  }
  else
  {
    /* Find the first row through the primary key */
    (void) ha_index_init(primary_key, 0);
    error=index_first(buf);
    (void) ha_index_end();
  }
  DBUG_RETURN(error);
}

/*
  Generate the next auto-increment number based on increment and offset:
  computes the lowest number
  - strictly greater than "nr"
  - of the form: auto_increment_offset + N * auto_increment_increment

  In most cases increment= offset= 1, in which case we get:
  1,2,3,4,5,...
  If increment=10 and offset=5 and previous number is 1, we get:
  1,5,15,25,35,...
*/

inline ulonglong
compute_next_insert_id(ulonglong nr,struct system_variables *variables)
{
  if (variables->auto_increment_increment == 1)
    return (nr+1); // optimization of the formula below
  nr= (((nr+ variables->auto_increment_increment -
         variables->auto_increment_offset)) /
       (ulonglong) variables->auto_increment_increment);
  return (nr* (ulonglong) variables->auto_increment_increment +
          variables->auto_increment_offset);
}


void handler::adjust_next_insert_id_after_explicit_value(ulonglong nr)
{
  /*
    If we have set THD::next_insert_id previously and plan to insert an
    explicitely-specified value larger than this, we need to increase
    THD::next_insert_id to be greater than the explicit value.
  */
  if ((next_insert_id > 0) && (nr >= next_insert_id))
    set_next_insert_id(compute_next_insert_id(nr, &table->in_use->variables));
}


/*
  Computes the largest number X:
  - smaller than or equal to "nr"
  - of the form: auto_increment_offset + N * auto_increment_increment
  where N>=0.

  SYNOPSIS
    prev_insert_id
      nr            Number to "round down"
      variables     variables struct containing auto_increment_increment and
                    auto_increment_offset

  RETURN
    The number X if it exists, "nr" otherwise.
*/

inline ulonglong
prev_insert_id(ulonglong nr, struct system_variables *variables)
{
  if (unlikely(nr < variables->auto_increment_offset))
  {
    /*
      There's nothing good we can do here. That is a pathological case, where
      the offset is larger than the column's max possible value, i.e. not even
      the first sequence value may be inserted. User will receive warning.
    */
    DBUG_PRINT("info",("auto_increment: nr: %lu cannot honour "
                       "auto_increment_offset: %lu",
                       nr, variables->auto_increment_offset));
    return nr;
  }
  if (variables->auto_increment_increment == 1)
    return nr; // optimization of the formula below
  nr= (((nr - variables->auto_increment_offset)) /
       (ulonglong) variables->auto_increment_increment);
  return (nr * (ulonglong) variables->auto_increment_increment +
          variables->auto_increment_offset);
}


/*
  Update the auto_increment field if necessary

  SYNOPSIS
    update_auto_increment()

  RETURN
    0	ok
    1 	get_auto_increment() was called and returned ~(ulonglong) 0
    

  IMPLEMENTATION

    Updates the record's Field of type NEXT_NUMBER if:

  - If column value is set to NULL (in which case
    auto_increment_field_not_null is 0)
  - If column is set to 0 and (sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO) is not
    set. In the future we will only set NEXT_NUMBER fields if one sets them
    to NULL (or they are not included in the insert list).

    In those cases, we check if the currently reserved interval still has
    values we have not used. If yes, we pick the smallest one and use it.
    Otherwise:

  - If a list of intervals has been provided to the statement via SET
    INSERT_ID or via an Intvar_log_event (in a replication slave), we pick the
    first unused interval from this list, consider it as reserved.

  - Otherwise we set the column for the first row to the value
    next_insert_id(get_auto_increment(column))) which is usually
    max-used-column-value+1.
    We call get_auto_increment() for the first row in a multi-row
    statement. get_auto_increment() will tell us the interval of values it
    reserved for us.

  - In both cases, for the following rows we use those reserved values without
    calling the handler again (we just progress in the interval, computing
    each new value from the previous one). Until we have exhausted them, then
    we either take the next provided interval or call get_auto_increment()
    again to reserve a new interval.

  - In both cases, the reserved intervals are remembered in
    thd->auto_inc_intervals_in_cur_stmt_for_binlog if statement-based
    binlogging; the last reserved interval is remembered in
    auto_inc_interval_for_cur_row.

    The idea is that generated auto_increment values are predictable and
    independent of the column values in the table.  This is needed to be
    able to replicate into a table that already has rows with a higher
    auto-increment value than the one that is inserted.

    After we have already generated an auto-increment number and the user
    inserts a column with a higher value than the last used one, we will
    start counting from the inserted value.

    This function's "outputs" are: the table's auto_increment field is filled
    with a value, thd->next_insert_id is filled with the value to use for the
    next row, if a value was autogenerated for the current row it is stored in
    thd->insert_id_for_cur_row, if get_auto_increment() was called
    thd->auto_inc_interval_for_cur_row is modified, if that interval is not
    present in thd->auto_inc_intervals_in_cur_stmt_for_binlog it is added to
    this list.

   TODO

    Replace all references to "next number" or NEXT_NUMBER to
    "auto_increment", everywhere (see below: there is
    table->auto_increment_field_not_null, and there also exists
    table->next_number_field, it's not consistent).

*/

#define AUTO_INC_DEFAULT_NB_ROWS 1 // Some prefer 1024 here
#define AUTO_INC_DEFAULT_NB_MAX_BITS 16
#define AUTO_INC_DEFAULT_NB_MAX ((1 << AUTO_INC_DEFAULT_NB_MAX_BITS) - 1)

bool handler::update_auto_increment()
{
  ulonglong nr, nb_reserved_values;
  bool append= FALSE;
  THD *thd= table->in_use;
  struct system_variables *variables= &thd->variables;
  bool auto_increment_field_not_null;
  bool result= 0;
  DBUG_ENTER("handler::update_auto_increment");

  /*
    next_insert_id is a "cursor" into the reserved interval, it may go greater
    than the interval, but not smaller.
  */
  DBUG_ASSERT(next_insert_id >= auto_inc_interval_for_cur_row.minimum());
  auto_increment_field_not_null= table->auto_increment_field_not_null;
  table->auto_increment_field_not_null= FALSE; // to reset for next row

  if ((nr= table->next_number_field->val_int()) != 0 ||
      auto_increment_field_not_null &&
      thd->variables.sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO)
  {
    /*
      Update next_insert_id if we had already generated a value in this
      statement (case of INSERT VALUES(null),(3763),(null):
      the last NULL needs to insert 3764, not the value of the first NULL plus
      1).
    */
    adjust_next_insert_id_after_explicit_value(nr);
    insert_id_for_cur_row= 0; // didn't generate anything
    DBUG_RETURN(0);
  }

  if ((nr= next_insert_id) >= auto_inc_interval_for_cur_row.maximum())
  {
    /* next_insert_id is beyond what is reserved, so we reserve more. */
    const Discrete_interval *forced=
      thd->auto_inc_intervals_forced.get_next();
    if (forced != NULL)
    {
      nr= forced->minimum();
      nb_reserved_values= forced->values();
    }
    else
    {
      /*
        handler::estimation_rows_to_insert was set by
        handler::ha_start_bulk_insert(); if 0 it means "unknown".
      */
      uint nb_already_reserved_intervals=
        thd->auto_inc_intervals_in_cur_stmt_for_binlog.nb_elements();
      ulonglong nb_desired_values;
      /*
        If an estimation was given to the engine:
        - use it.
        - if we already reserved numbers, it means the estimation was
        not accurate, then we'll reserve 2*AUTO_INC_DEFAULT_NB_ROWS the 2nd
        time, twice that the 3rd time etc.
        If no estimation was given, use those increasing defaults from the
        start, starting from AUTO_INC_DEFAULT_NB_ROWS.
        Don't go beyond a max to not reserve "way too much" (because
        reservation means potentially losing unused values).
      */
      if (nb_already_reserved_intervals == 0 &&
          (estimation_rows_to_insert > 0))
        nb_desired_values= estimation_rows_to_insert;
      else /* go with the increasing defaults */
      {
        /* avoid overflow in formula, with this if() */
        if (nb_already_reserved_intervals <= AUTO_INC_DEFAULT_NB_MAX_BITS)
        {
          nb_desired_values= AUTO_INC_DEFAULT_NB_ROWS * 
            (1 << nb_already_reserved_intervals);
          set_if_smaller(nb_desired_values, AUTO_INC_DEFAULT_NB_MAX);
        }
        else
          nb_desired_values= AUTO_INC_DEFAULT_NB_MAX;
      }
      /* This call ignores all its parameters but nr, currently */
      get_auto_increment(variables->auto_increment_offset,
                         variables->auto_increment_increment,
                         nb_desired_values, &nr,
                         &nb_reserved_values);
      if (nr == ~(ulonglong) 0)
        result= 1;                                // Mark failure
      
      /*
        That rounding below should not be needed when all engines actually
        respect offset and increment in get_auto_increment(). But they don't
        so we still do it. Wonder if for the not-first-in-index we should do
        it. Hope that this rounding didn't push us out of the interval; even
        if it did we cannot do anything about it (calling the engine again
        will not help as we inserted no row).
      */
      nr= compute_next_insert_id(nr-1, variables);
    }
    
    if (table->s->next_number_key_offset == 0)
    {
      /* We must defer the appending until "nr" has been possibly truncated */
      append= TRUE;
    }
    else
    {
      /*
        For such auto_increment there is no notion of interval, just a
        singleton. The interval is not even stored in
        thd->auto_inc_interval_for_cur_row, so we are sure to call the engine
        for next row.
      */
      DBUG_PRINT("info",("auto_increment: special not-first-in-index"));
    }
  }

  DBUG_PRINT("info",("auto_increment: %lu", (ulong) nr));

  if (unlikely(table->next_number_field->store((longlong) nr, TRUE)))
  {
    /*
      field refused this value (overflow) and truncated it, use the result of
      the truncation (which is going to be inserted); however we try to
      decrease it to honour auto_increment_* variables.
      That will shift the left bound of the reserved interval, we don't
      bother shifting the right bound (anyway any other value from this
      interval will cause a duplicate key).
    */
    nr= prev_insert_id(table->next_number_field->val_int(), variables);
    if (unlikely(table->next_number_field->store((longlong) nr, TRUE)))
      nr= table->next_number_field->val_int();
  }
  if (append)
  {
    auto_inc_interval_for_cur_row.replace(nr, nb_reserved_values,
                                          variables->auto_increment_increment);
    /* Row-based replication does not need to store intervals in binlog */
    if (!thd->current_stmt_binlog_row_based)
      result= result ||
        thd->auto_inc_intervals_in_cur_stmt_for_binlog.append(auto_inc_interval_for_cur_row.minimum(),
                                                              auto_inc_interval_for_cur_row.values(),
                                                              variables->auto_increment_increment);
  }

  /*
    Record this autogenerated value. If the caller then
    succeeds to insert this value, it will call
    record_first_successful_insert_id_in_cur_stmt()
    which will set first_successful_insert_id_in_cur_stmt if it's not
    already set.
  */
  insert_id_for_cur_row= nr;
  /*
    Set next insert id to point to next auto-increment value to be able to
    handle multi-row statements.
  */
  set_next_insert_id(compute_next_insert_id(nr, variables));

  DBUG_RETURN(result);
}


/*
  MySQL signal that it changed the column bitmap

  USAGE
    This is for handlers that needs to setup their own column bitmaps.
    Normally the handler should set up their own column bitmaps in
    index_init() or rnd_init() and in any column_bitmaps_signal() call after
    this.

    The handler is allowd to do changes to the bitmap after a index_init or
    rnd_init() call is made as after this, MySQL will not use the bitmap
    for any program logic checking.
*/

void handler::column_bitmaps_signal()
{
  DBUG_ENTER("column_bitmaps_signal");
  DBUG_PRINT("info", ("read_set: 0x%lx  write_set: 0x%lx", table->read_set,
                      table->write_set));
  DBUG_VOID_RETURN;
}


/*
  Reserves an interval of auto_increment values from the handler.

  SYNOPSIS
    get_auto_increment()
    offset              
    increment
    nb_desired_values   how many values we want
    first_value         (OUT) the first value reserved by the handler
    nb_reserved_values  (OUT) how many values the handler reserved

  offset and increment means that we want values to be of the form
  offset + N * increment, where N>=0 is integer.
  If the function sets *first_value to ~(ulonglong)0 it means an error.
  If the function sets *nb_reserved_values to ULONGLONG_MAX it means it has
  reserved to "positive infinite".
*/

void handler::get_auto_increment(ulonglong offset, ulonglong increment,
                                 ulonglong nb_desired_values,
                                 ulonglong *first_value,
                                 ulonglong *nb_reserved_values)
{
  ulonglong nr;
  int error;

  (void) extra(HA_EXTRA_KEYREAD);
  table->mark_columns_used_by_index_no_reset(table->s->next_number_index,
                                        table->read_set);
  column_bitmaps_signal();
  index_init(table->s->next_number_index, 1);
  if (!table->s->next_number_key_offset)
  {						// Autoincrement at key-start
    error=index_last(table->record[1]);
    /*
      MySQL implicitely assumes such method does locking (as MySQL decides to
      use nr+increment without checking again with the handler, in
      handler::update_auto_increment()), so reserves to infinite.
    */
    *nb_reserved_values= ULONGLONG_MAX;
  }
  else
  {
    byte key[MAX_KEY_LENGTH];
    key_copy(key, table->record[0],
             table->key_info + table->s->next_number_index,
             table->s->next_number_key_offset);
    error= index_read(table->record[1], key, table->s->next_number_key_offset,
                      HA_READ_PREFIX_LAST);
    /*
      MySQL needs to call us for next row: assume we are inserting ("a",null)
      here, we return 3, and next this statement will want to insert
      ("b",null): there is no reason why ("b",3+1) would be the good row to
      insert: maybe it already exists, maybe 3+1 is too large...
    */
    *nb_reserved_values= 1;
  }

  if (error)
    nr=1;
  else
    nr= ((ulonglong) table->next_number_field->
         val_int_offset(table->s->rec_buff_length)+1);
  index_end();
  (void) extra(HA_EXTRA_NO_KEYREAD);
  *first_value= nr;
}


void handler::ha_release_auto_increment()
{
  release_auto_increment();
  insert_id_for_cur_row= 0;
  auto_inc_interval_for_cur_row.replace(0, 0, 0);
  if (next_insert_id > 0)
  {
    next_insert_id= 0;
    /*
      this statement used forced auto_increment values if there were some,
      wipe them away for other statements.
    */
    table->in_use->auto_inc_intervals_forced.empty();
  }
}


void handler::print_keydup_error(uint key_nr, const char *msg)
{
  /* Write the duplicated key in the error message */
  char key[MAX_KEY_LENGTH];
  String str(key,sizeof(key),system_charset_info);
  /* Table is opened and defined at this point */
  key_unpack(&str,table,(uint) key_nr);
  uint max_length=MYSQL_ERRMSG_SIZE-(uint) strlen(msg);
  if (str.length() >= max_length)
  {
    str.length(max_length-4);
    str.append(STRING_WITH_LEN("..."));
  }
  my_printf_error(ER_DUP_ENTRY, msg,
                  MYF(0), str.c_ptr(), table->key_info[key_nr].name);
}


/*
  Print error that we got from handler function

  NOTE
   In case of delete table it's only safe to use the following parts of
   the 'table' structure:
     table->s->path
     table->alias
*/

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
      print_keydup_error(key_nr, ER(ER_DUP_ENTRY));
      DBUG_VOID_RETURN;
    }
    textno=ER_DUP_KEY;
    break;
  }
  case HA_ERR_FOREIGN_DUPLICATE_KEY:
  {
    uint key_nr= get_dup_key(error);
    if ((int) key_nr >= 0)
    {
      uint max_length;
      /* Write the key in the error message */
      char key[MAX_KEY_LENGTH];
      String str(key,sizeof(key),system_charset_info);
      /* Table is opened and defined at this point */
      key_unpack(&str,table,(uint) key_nr);
      max_length= (MYSQL_ERRMSG_SIZE-
                   (uint) strlen(ER(ER_FOREIGN_DUPLICATE_KEY)));
      if (str.length() >= max_length)
      {
        str.length(max_length-4);
        str.append(STRING_WITH_LEN("..."));
      }
      my_error(ER_FOREIGN_DUPLICATE_KEY, MYF(0), table_share->table_name.str,
        str.c_ptr(), key_nr+1);
      DBUG_VOID_RETURN;
    }
    textno= ER_DUP_KEY;
    break;
  }
  case HA_ERR_NULL_IN_SPATIAL:
    textno= ER_UNKNOWN_ERROR;
    break;
  case HA_ERR_FOUND_DUPP_UNIQUE:
    textno=ER_DUP_UNIQUE;
    break;
  case HA_ERR_RECORD_CHANGED:
    textno=ER_CHECKREAD;
    break;
  case HA_ERR_CRASHED:
    textno=ER_NOT_KEYFILE;
    break;
  case HA_ERR_WRONG_IN_RECORD:
    textno= ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_CRASHED_ON_USAGE:
    textno=ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_NOT_A_TABLE:
    textno= error;
    break;
  case HA_ERR_CRASHED_ON_REPAIR:
    textno=ER_CRASHED_ON_REPAIR;
    break;
  case HA_ERR_OUT_OF_MEM:
    textno=ER_OUT_OF_RESOURCES;
    break;
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
  case HA_ERR_INDEX_FILE_FULL:
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
  {
    String str;
    get_error_message(error, &str);
    my_error(ER_ROW_IS_REFERENCED_2, MYF(0), str.c_ptr_safe());
    DBUG_VOID_RETURN;
  }
  case HA_ERR_NO_REFERENCED_ROW:
  {
    String str;
    get_error_message(error, &str);
    my_error(ER_NO_REFERENCED_ROW_2, MYF(0), str.c_ptr_safe());
    DBUG_VOID_RETURN;
  }
  case HA_ERR_TABLE_DEF_CHANGED:
    textno=ER_TABLE_DEF_CHANGED;
    break;
  case HA_ERR_NO_SUCH_TABLE:
    my_error(ER_NO_SUCH_TABLE, MYF(0), table_share->db.str,
             table_share->table_name.str);
    break;
  case HA_ERR_RBR_LOGGING_FAILED:
    textno= ER_BINLOG_ROW_LOGGING_FAILED;
    break;
  case HA_ERR_DROP_INDEX_FK:
  {
    const char *ptr= "???";
    uint key_nr= get_dup_key(error);
    if ((int) key_nr >= 0)
      ptr= table->key_info[key_nr].name;
    my_error(ER_DROP_INDEX_FK, MYF(0), ptr);
    DBUG_VOID_RETURN;
  }
  case HA_ERR_TABLE_NEEDS_UPGRADE:
    textno=ER_TABLE_NEEDS_UPGRADE;
    break;
  case HA_ERR_TABLE_READONLY:
    textno= ER_OPEN_AS_READONLY;
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
	  my_error(ER_GET_TEMPORARY_ERRMSG, MYF(0), error, str.ptr(), engine);
	else
	  my_error(ER_GET_ERRMSG, MYF(0), error, str.ptr(), engine);
      }
      else
	my_error(ER_GET_ERRNO,errflag,error);
      DBUG_VOID_RETURN;
    }
  }
  my_error(textno, errflag, table_share->table_name.str, error);
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


int handler::ha_check_for_upgrade(HA_CHECK_OPT *check_opt)
{
  KEY *keyinfo, *keyend;
  KEY_PART_INFO *keypart, *keypartend;

  if (!table->s->mysql_version)
  {
    /* check for blob-in-key error */
    keyinfo= table->key_info;
    keyend= table->key_info + table->s->keys;
    for (; keyinfo < keyend; keyinfo++)
    {
      keypart= keyinfo->key_part;
      keypartend= keypart + keyinfo->key_parts;
      for (; keypart < keypartend; keypart++)
      {
        if (!keypart->fieldnr)
          continue;
        Field *field= table->field[keypart->fieldnr-1];
        if (field->type() == FIELD_TYPE_BLOB)
        {
          if (check_opt->sql_flags & TT_FOR_UPGRADE)
            check_opt->flags= T_MEDIUM;
          return HA_ADMIN_NEEDS_CHECK;
        }
      }
    }
  }
  return check_for_upgrade(check_opt);
}


int handler::check_old_types()
{
  Field** field;

  if (!table->s->mysql_version)
  {
    /* check for bad DECIMAL field */
    for (field= table->field; (*field); field++)
    {
      if ((*field)->type() == FIELD_TYPE_NEWDECIMAL)
      {
        return HA_ADMIN_NEEDS_ALTER;
      }
    }
  }
  return 0;
}


static bool update_frm_version(TABLE *table, bool needs_lock)
{
  char path[FN_REFLEN];
  File file;
  int result= 1;
  DBUG_ENTER("update_frm_version");

  if (table->s->mysql_version != MYSQL_VERSION_ID)
    DBUG_RETURN(0);

  strxmov(path, table->s->normalized_path.str, reg_ext, NullS);

  if (needs_lock)
    pthread_mutex_lock(&LOCK_open);

  if ((file= my_open(path, O_RDWR|O_BINARY, MYF(MY_WME))) >= 0)
  {
    uchar version[4];
    char *key= table->s->table_cache_key.str;
    uint key_length= table->s->table_cache_key.length;
    TABLE *entry;
    HASH_SEARCH_STATE state;

    int4store(version, MYSQL_VERSION_ID);

    if ((result= my_pwrite(file,(byte*) version,4,51L,MYF_RW)))
      goto err;

    for (entry=(TABLE*) hash_first(&open_cache,(byte*) key,key_length, &state);
         entry;
         entry= (TABLE*) hash_next(&open_cache,(byte*) key,key_length, &state))
      entry->s->mysql_version= MYSQL_VERSION_ID;
  }
err:
  if (file >= 0)
    VOID(my_close(file,MYF(MY_WME)));
  if (needs_lock)
    pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(result);
}



/* Return key if error because of duplicated keys */

uint handler::get_dup_key(int error)
{
  DBUG_ENTER("handler::get_dup_key");
  table->file->errkey  = (uint) -1;
  if (error == HA_ERR_FOUND_DUPP_KEY || error == HA_ERR_FOREIGN_DUPLICATE_KEY ||
      error == HA_ERR_FOUND_DUPP_UNIQUE || error == HA_ERR_NULL_IN_SPATIAL ||
      error == HA_ERR_DROP_INDEX_FK)
    info(HA_STATUS_ERRKEY | HA_STATUS_NO_LOCK);
  DBUG_RETURN(table->file->errkey);
}


/*
  Delete all files with extension from bas_ext()

  SYNOPSIS
    delete_table()
    name		Base name of table

  NOTES
    We assume that the handler may return more extensions than
    was actually used for the file.

  RETURN
    0   If we successfully deleted at least one file from base_ext and
	didn't get any other errors than ENOENT
    #   Error
*/

int handler::delete_table(const char *name)
{
  int error= 0;
  int enoent_or_zero= ENOENT;                   // Error if no file was deleted
  char buff[FN_REFLEN];

  for (const char **ext=bas_ext(); *ext ; ext++)
  {
    fn_format(buff, name, "", *ext, MY_UNPACK_FILENAME|MY_APPEND_EXT);
    if (my_delete_with_symlink(buff, MYF(0)))
    {
      if ((error= my_errno) != ENOENT)
	break;
    }
    else
      enoent_or_zero= 0;                        // No error for ENOENT
    error= enoent_or_zero;
  }
  return error;
}


int handler::rename_table(const char * from, const char * to)
{
  int error= 0;
  for (const char **ext= bas_ext(); *ext ; ext++)
  {
    if (rename_file_ext(from, to, *ext))
    {
      if ((error=my_errno) != ENOENT)
	break;
      error= 0;
    }
  }
  return error;
}


void handler::drop_table(const char *name)
{
  close();
  delete_table(name);
}


/*
   Performs checks upon the table.

   SYNOPSIS
   check()
   thd                thread doing CHECK TABLE operation
   check_opt          options from the parser

   NOTES

   RETURN
   HA_ADMIN_OK                 Successful upgrade
   HA_ADMIN_NEEDS_UPGRADE      Table has structures requiring upgrade
   HA_ADMIN_NEEDS_ALTER        Table has structures requiring ALTER TABLE
   HA_ADMIN_NOT_IMPLEMENTED
*/

int handler::ha_check(THD *thd, HA_CHECK_OPT *check_opt)
{
  int error;

  if ((table->s->mysql_version >= MYSQL_VERSION_ID) &&
      (check_opt->sql_flags & TT_FOR_UPGRADE))
    return 0;

  if (table->s->mysql_version < MYSQL_VERSION_ID)
  {
    if ((error= check_old_types()))
      return error;
    error= ha_check_for_upgrade(check_opt);
    if (error && (error != HA_ADMIN_NEEDS_CHECK))
      return error;
    if (!error && (check_opt->sql_flags & TT_FOR_UPGRADE))
      return 0;
  }
  if ((error= check(thd, check_opt)))
    return error;
  return update_frm_version(table, 0);
}


int handler::ha_repair(THD* thd, HA_CHECK_OPT* check_opt)
{
  int result;
  if ((result= repair(thd, check_opt)))
    return result;
  return update_frm_version(table, 0);
}


/*
  Tell the storage engine that it is allowed to "disable transaction" in the
  handler. It is a hint that ACID is not required - it is used in NDB for
  ALTER TABLE, for example, when data are copied to temporary table.
  A storage engine may treat this hint any way it likes. NDB for example
  starts to commit every now and then automatically.
  This hint can be safely ignored.
*/

int ha_enable_transaction(THD *thd, bool on)
{
  int error=0;

  DBUG_ENTER("ha_enable_transaction");
  thd->transaction.on= on;
  if (on)
  {
    /*
      Now all storage engines should have transaction handling enabled.
      But some may have it enabled all the time - "disabling" transactions
      is an optimization hint that storage engine is free to ignore.
      So, let's commit an open transaction (if any) now.
    */
    if (!(error= ha_commit_stmt(thd)))
      error= end_trans(thd, COMMIT);
  }
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


void handler::get_dynamic_partition_info(PARTITION_INFO *stat_info,
                                         uint part_id)
{
  info(HA_STATUS_CONST | HA_STATUS_TIME | HA_STATUS_VARIABLE |
       HA_STATUS_NO_LOCK);
  stat_info->records=              stats.records;
  stat_info->mean_rec_length=      stats.mean_rec_length;
  stat_info->data_file_length=     stats.data_file_length;
  stat_info->max_data_file_length= stats.max_data_file_length;
  stat_info->index_file_length=    stats.index_file_length;
  stat_info->delete_length=        stats.delete_length;
  stat_info->create_time=          stats.create_time;
  stat_info->update_time=          stats.update_time;
  stat_info->check_time=           stats.check_time;
  stat_info->check_sum=            0;
  if (table_flags() & (ulong) HA_HAS_CHECKSUM)
    stat_info->check_sum= checksum();
  return;
}


/****************************************************************************
** Some general functions that isn't in the handler class
****************************************************************************/

/*
  Initiates table-file and calls appropriate database-creator

  NOTES
    We must have a write lock on LOCK_open to be sure no other thread
    interferes with table
    
  RETURN
   0  ok
   1  error
*/

int ha_create_table(THD *thd, const char *path,
                    const char *db, const char *table_name,
                    HA_CREATE_INFO *create_info,
		    bool update_create_info)
{
  int error= 1;
  TABLE table;
  char name_buff[FN_REFLEN];
  const char *name;
  TABLE_SHARE share;
  DBUG_ENTER("ha_create_table");
  
  init_tmp_table_share(&share, db, 0, table_name, path);
  if (open_table_def(thd, &share, 0) ||
      open_table_from_share(thd, &share, "", 0, (uint) READ_ALL, 0, &table,
                            TRUE))
    goto err;

  if (update_create_info)
    update_create_info_from_table(create_info, &table);

  name= share.path.str;
  if (lower_case_table_names == 2 &&
      !(table.file->ha_table_flags() & HA_FILE_BASED))
  {
    /* Ensure that handler gets name in lower case */
    strmov(name_buff, name);
    my_casedn_str(files_charset_info, name_buff);
    name= name_buff;
  }

  error= table.file->create(name, &table, create_info);
  VOID(closefrm(&table, 0));
  if (error)
  {
    strxmov(name_buff, db, ".", table_name, NullS);
    my_error(ER_CANT_CREATE_TABLE, MYF(ME_BELL+ME_WAITTANG), name_buff, error);
  }
err:
  free_table_share(&share);
  DBUG_RETURN(error != 0);
}

/*
  Try to discover table from engine

  NOTES
    If found, write the frm file to disk.

  RETURN VALUES:
  -1    Table did not exists
   0    Table created ok
   > 0  Error, table existed but could not be created

*/

int ha_create_table_from_engine(THD* thd, const char *db, const char *name)
{
  int error;
  const void *frmblob;
  uint frmlen;
  char path[FN_REFLEN];
  HA_CREATE_INFO create_info;
  TABLE table;
  TABLE_SHARE share;
  DBUG_ENTER("ha_create_table_from_engine");
  DBUG_PRINT("enter", ("name '%s'.'%s'", db, name));

  bzero((char*) &create_info,sizeof(create_info));
  if ((error= ha_discover(thd, db, name, &frmblob, &frmlen)))
  {
    /* Table could not be discovered and thus not created */
    DBUG_RETURN(error);
  }

  /*
    Table exists in handler and could be discovered
    frmblob and frmlen are set, write the frm to disk
  */

  (void)strxnmov(path,FN_REFLEN-1,mysql_data_home,"/",db,"/",name,NullS);
  // Save the frm file
  error= writefrm(path, frmblob, frmlen);
  my_free((char*) frmblob, MYF(0));
  if (error)
    DBUG_RETURN(2);

  init_tmp_table_share(&share, db, 0, name, path);
  if (open_table_def(thd, &share, 0))
  {
    DBUG_RETURN(3);
  }
  if (open_table_from_share(thd, &share, "" ,0, 0, 0, &table, FALSE))
  {
    free_table_share(&share);
    DBUG_RETURN(3);
  }

  update_create_info_from_table(&create_info, &table);
  create_info.table_options|= HA_OPTION_CREATE_FROM_ENGINE;

  if (lower_case_table_names == 2 &&
      !(table.file->ha_table_flags() & HA_FILE_BASED))
  {
    /* Ensure that handler gets name in lower case */
    my_casedn_str(files_charset_info, path);
  }
  error=table.file->create(path,&table,&create_info);
  VOID(closefrm(&table, 1));

  DBUG_RETURN(error != 0);
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

  RETURN
   -1  : Table did not exists
    0  : OK. In this case *frmblob and *frmlen are set
    >0 : error.  frmblob and frmlen may not be set
*/

typedef struct st_discover_args
{
  const char *db;
  const char *name;
  const void** frmblob; 
  uint* frmlen;
};

static my_bool discover_handlerton(THD *thd, st_plugin_int *plugin,
                                   void *arg)
{
  st_discover_args *vargs= (st_discover_args *)arg;
  handlerton *hton= (handlerton *)plugin->data;
  if (hton->state == SHOW_OPTION_YES && hton->discover &&
      (!(hton->discover(thd, vargs->db, vargs->name, vargs->frmblob, vargs->frmlen))))
    return TRUE;

  return FALSE;
}

int ha_discover(THD *thd, const char *db, const char *name,
		const void **frmblob, uint *frmlen)
{
  int error= -1; // Table does not exist in any handler
  DBUG_ENTER("ha_discover");
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name));
  st_discover_args args= {db, name, frmblob, frmlen};

  if (is_prefix(name,tmp_file_prefix)) /* skip temporary tables */
    DBUG_RETURN(error);

  if (plugin_foreach(thd, discover_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &args))
    error= 0;

  if (!error)
    statistic_increment(thd->status_var.ha_discover_count,&LOCK_status);
  DBUG_RETURN(error);
}


/*
  Call this function in order to give the handler the possibility 
  to ask engine if there are any new tables that should be written to disk 
  or any dropped tables that need to be removed from disk
*/

int
ha_find_files(THD *thd,const char *db,const char *path,
	      const char *wild, bool dir, List<char> *files)
{
  int error= 0;
  DBUG_ENTER("ha_find_files");
  DBUG_PRINT("enter", ("db: %s, path: %s, wild: %s, dir: %d", 
		       db, path, wild, dir));
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
  if (have_ndbcluster == SHOW_OPTION_YES)
    error= ndbcluster_find_files(thd, db, path, wild, dir, files);
#endif
  DBUG_RETURN(error);
}


/*
  Ask handler if the table exists in engine

  RETURN
    0                   Table does not exist
    1                   Table exists
    #                   Error code

 */
int ha_table_exists_in_engine(THD* thd, const char* db, const char* name)
{
  int error= 0;
  DBUG_ENTER("ha_table_exists_in_engine");
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name));
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
  if (have_ndbcluster == SHOW_OPTION_YES)
    error= ndbcluster_table_exists_in_engine(thd, db, name);
#endif
  DBUG_PRINT("exit", ("error: %d", error));
  DBUG_RETURN(error);
}

#ifdef HAVE_NDB_BINLOG
/*
  TODO: change this into a dynamic struct
  List<handlerton> does not work as
  1. binlog_end is called when MEM_ROOT is gone
  2. cannot work with thd MEM_ROOT as memory should be freed
*/
#define MAX_HTON_LIST_ST 63
struct hton_list_st
{
  handlerton *hton[MAX_HTON_LIST_ST];
  uint sz;
};

struct binlog_func_st
{
  enum_binlog_func fn;
  void *arg;
};

/*
  Listing handlertons first to avoid recursive calls and deadlock
*/
static my_bool binlog_func_list(THD *thd, st_plugin_int *plugin, void *arg)
{
  hton_list_st *hton_list= (hton_list_st *)arg;
  handlerton *hton= (handlerton *)plugin->data;
  if (hton->state == SHOW_OPTION_YES && hton->binlog_func)
  {
    uint sz= hton_list->sz;
    if (sz == MAX_HTON_LIST_ST-1)
    {
      /* list full */
      return FALSE;
    }
    hton_list->hton[sz]= hton;
    hton_list->sz= sz+1;
  }
  return FALSE;
}

static my_bool binlog_func_foreach(THD *thd, binlog_func_st *bfn)
{
  handlerton *hton;
  hton_list_st hton_list;
  hton_list.sz= 0;
  plugin_foreach(thd, binlog_func_list,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &hton_list);

  uint i= 0, sz= hton_list.sz;
  while(i < sz)
    hton_list.hton[i++]->binlog_func(thd, bfn->fn, bfn->arg);
  return FALSE;
}

int ha_reset_logs(THD *thd)
{
  binlog_func_st bfn= {BFN_RESET_LOGS, 0};
  binlog_func_foreach(thd, &bfn);
  return 0;
}

void ha_reset_slave(THD* thd)
{
  binlog_func_st bfn= {BFN_RESET_SLAVE, 0};
  binlog_func_foreach(thd, &bfn);
}

void ha_binlog_wait(THD* thd)
{
  binlog_func_st bfn= {BFN_BINLOG_WAIT, 0};
  binlog_func_foreach(thd, &bfn);
}

int ha_binlog_end(THD* thd)
{
  binlog_func_st bfn= {BFN_BINLOG_END, 0};
  binlog_func_foreach(thd, &bfn);
  return 0;
}

int ha_binlog_index_purge_file(THD *thd, const char *file)
{
  binlog_func_st bfn= {BFN_BINLOG_PURGE_FILE, (void *)file};
  binlog_func_foreach(thd, &bfn);
  return 0;
}

struct binlog_log_query_st
{
  enum_binlog_command binlog_command;
  const char *query;
  uint query_length;
  const char *db;
  const char *table_name;
};

static my_bool binlog_log_query_handlerton2(THD *thd,
                                            const handlerton *hton,
                                            void *args)
{
  struct binlog_log_query_st *b= (struct binlog_log_query_st*)args;
  if (hton->state == SHOW_OPTION_YES && hton->binlog_log_query)
    hton->binlog_log_query(thd,
                           b->binlog_command,
                           b->query,
                           b->query_length,
                           b->db,
                           b->table_name);
  return FALSE;
}

static my_bool binlog_log_query_handlerton(THD *thd,
                                           st_plugin_int *plugin,
                                           void *args)
{
  return binlog_log_query_handlerton2(thd, (const handlerton *)plugin->data, args);
}

void ha_binlog_log_query(THD *thd, const handlerton *hton,
                         enum_binlog_command binlog_command,
                         const char *query, uint query_length,
                         const char *db, const char *table_name)
{
  struct binlog_log_query_st b;
  b.binlog_command= binlog_command;
  b.query= query;
  b.query_length= query_length;
  b.db= db;
  b.table_name= table_name;
  if (hton == 0)
    plugin_foreach(thd, binlog_log_query_handlerton,
                   MYSQL_STORAGE_ENGINE_PLUGIN, &b);
  else
    binlog_log_query_handlerton2(thd, hton, &b);
}
#endif

/*
  Read the first row of a multi-range set.

  SYNOPSIS
    read_multi_range_first()
    found_range_p       Returns a pointer to the element in 'ranges' that
                        corresponds to the returned row.
    ranges              An array of KEY_MULTI_RANGE range descriptions.
    range_count         Number of ranges in 'ranges'.
    sorted		If result should be sorted per key.
    buffer              A HANDLER_BUFFER for internal handler usage.

  NOTES
    Record is read into table->record[0].
    *found_range_p returns a valid value only if read_multi_range_first()
    returns 0.
    Sorting is done within each range. If you want an overall sort, enter
    'ranges' with sorted ranges.

  RETURN
    0			OK, found a row
    HA_ERR_END_OF_FILE	No rows in range
    #			Error code
*/

int handler::read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
                                    KEY_MULTI_RANGE *ranges, uint range_count,
                                    bool sorted, HANDLER_BUFFER *buffer)
{
  int result= HA_ERR_END_OF_FILE;
  DBUG_ENTER("handler::read_multi_range_first");
  multi_range_sorted= sorted;
  multi_range_buffer= buffer;

  table->mark_columns_used_by_index_no_reset(active_index, table->read_set);
  table->column_bitmaps_set(table->read_set, table->write_set);

  for (multi_range_curr= ranges, multi_range_end= ranges + range_count;
       multi_range_curr < multi_range_end;
       multi_range_curr++)
  {
    result= read_range_first(multi_range_curr->start_key.length ?
                             &multi_range_curr->start_key : 0,
                             multi_range_curr->end_key.length ?
                             &multi_range_curr->end_key : 0,
                             test(multi_range_curr->range_flag & EQ_RANGE),
                             multi_range_sorted);
    if (result != HA_ERR_END_OF_FILE)
      break;
  }

  *found_range_p= multi_range_curr;
  DBUG_PRINT("exit",("result %d", result));
  DBUG_RETURN(result);
}


/*
  Read the next row of a multi-range set.

  SYNOPSIS
    read_multi_range_next()
    found_range_p       Returns a pointer to the element in 'ranges' that
                        corresponds to the returned row.

  NOTES
    Record is read into table->record[0].
    *found_range_p returns a valid value only if read_multi_range_next()
    returns 0.

  RETURN
    0			OK, found a row
    HA_ERR_END_OF_FILE	No (more) rows in range
    #			Error code
*/

int handler::read_multi_range_next(KEY_MULTI_RANGE **found_range_p)
{
  int result;
  DBUG_ENTER("handler::read_multi_range_next");

  /* We should not be called after the last call returned EOF. */
  DBUG_ASSERT(multi_range_curr < multi_range_end);

  do
  {
    /* Save a call if there can be only one row in range. */
    if (multi_range_curr->range_flag != (UNIQUE_RANGE | EQ_RANGE))
    {
      result= read_range_next();

      /* On success or non-EOF errors jump to the end. */
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
    else
    {
      /*
        We need to set this for the last range only, but checking this
        condition is more expensive than just setting the result code.
      */
      result= HA_ERR_END_OF_FILE;
    }

    /* Try the next range(s) until one matches a record. */
    for (multi_range_curr++;
         multi_range_curr < multi_range_end;
         multi_range_curr++)
    {
      result= read_range_first(multi_range_curr->start_key.length ?
                               &multi_range_curr->start_key : 0,
                               multi_range_curr->end_key.length ?
                               &multi_range_curr->end_key : 0,
                               test(multi_range_curr->range_flag & EQ_RANGE),
                               multi_range_sorted);
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
  }
  while ((result == HA_ERR_END_OF_FILE) &&
         (multi_range_curr < multi_range_end));

  *found_range_p= multi_range_curr;
  DBUG_PRINT("exit",("handler::read_multi_range_next: result %d", result));
  DBUG_RETURN(result);
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
  int error= ha_index_init(index, 0);
  if (!error)
    error= index_read(buf, key, key_len, find_flag);
  if (!error)
    error= ha_index_end();
  return error;
}


/*
  Returns a list of all known extensions.

  SYNOPSIS
    ha_known_exts()

  NOTES
    No mutexes, worst case race is a minor surplus memory allocation
    We have to recreate the extension map if mysqld is restarted (for example
    within libmysqld)

  RETURN VALUE
    pointer		pointer to TYPELIB structure
*/

static my_bool exts_handlerton(THD *unused, st_plugin_int *plugin,
                               void *arg)
{
  List<char> *found_exts= (List<char> *) arg;
  handlerton *hton= (handlerton *)plugin->data;
  handler *file;
  if (hton->state == SHOW_OPTION_YES && hton->create &&
      (file= hton->create((TABLE_SHARE*) 0, current_thd->mem_root)))
  {
    List_iterator_fast<char> it(*found_exts);
    const char **ext, *old_ext;

    for (ext= file->bas_ext(); *ext; ext++)
    {
      while ((old_ext= it++))
      {
        if (!strcmp(old_ext, *ext))
	  break;
      }
      if (!old_ext)
        found_exts->push_back((char *) *ext);

      it.rewind();
    }
    delete file;
  }
  return FALSE;
}

TYPELIB *ha_known_exts(void)
{
  MEM_ROOT *mem_root= current_thd->mem_root;
  if (!known_extensions.type_names || mysys_usage_id != known_extensions_id)
  {
    List<char> found_exts;
    const char **ext, *old_ext;

    known_extensions_id= mysys_usage_id;
    found_exts.push_back((char*) triggers_file_ext);
    found_exts.push_back((char*) trigname_file_ext);

    plugin_foreach(NULL, exts_handlerton,
                   MYSQL_STORAGE_ENGINE_PLUGIN, &found_exts);

    ext= (const char **) my_once_alloc(sizeof(char *)*
                                       (found_exts.elements+1),
                                       MYF(MY_WME | MY_FAE));

    DBUG_ASSERT(ext != 0);
    known_extensions.count= found_exts.elements;
    known_extensions.type_names= ext;

    List_iterator_fast<char> it(found_exts);
    while ((old_ext= it++))
      *ext++= old_ext;
    *ext= 0;
  }
  return &known_extensions;
}


static bool stat_print(THD *thd, const char *type, uint type_len,
                       const char *file, uint file_len,
                       const char *status, uint status_len)
{
  Protocol *protocol= thd->protocol;
  protocol->prepare_for_resend();
  protocol->store(type, type_len, system_charset_info);
  protocol->store(file, file_len, system_charset_info);
  protocol->store(status, status_len, system_charset_info);
  if (protocol->write())
    return TRUE;
  return FALSE;
}


static my_bool showstat_handlerton(THD *thd, st_plugin_int *plugin,
                                   void *arg)
{
  enum ha_stat_type stat= *(enum ha_stat_type *) arg;
  handlerton *hton= (handlerton *)plugin->data;
  if (hton->state == SHOW_OPTION_YES && hton->show_status &&
      hton->show_status(thd, stat_print, stat))
    return TRUE;
  return FALSE;
}

bool ha_show_status(THD *thd, handlerton *db_type, enum ha_stat_type stat)
{
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  bool result;

  field_list.push_back(new Item_empty_string("Type",10));
  field_list.push_back(new Item_empty_string("Name",FN_REFLEN));
  field_list.push_back(new Item_empty_string("Status",10));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return TRUE;

  if (db_type == NULL)
  {
    result= plugin_foreach(thd, showstat_handlerton,
                           MYSQL_STORAGE_ENGINE_PLUGIN, &stat);
  }
  else
  {
    if (db_type->state != SHOW_OPTION_YES)
    {
      const LEX_STRING *name=&hton2plugin[db_type->slot]->name;
      result= stat_print(thd, name->str, name->length,
                         "", 0, "DISABLED", 8) ? 1 : 0;
    }
    else
      result= db_type->show_status &&
              db_type->show_status(thd, stat_print, stat) ? 1 : 0;
  }

  if (!result)
    send_eof(thd);
  return result;
}

/*
  Function to check if the conditions for row-based binlogging is
  correct for the table.

  A row in the given table should be replicated if:
  - Row-based replication is enabled in the current thread
  - The binlog is enabled
  - It is not a temporary table
  - The binary log is open
  - The database the table resides in shall be binlogged (binlog_*_db rules)
  - table is not mysql.event
*/

#ifdef HAVE_ROW_BASED_REPLICATION
/* The Sun compiler cannot instantiate the template below if this is
   declared static, but it works by putting it into an anonymous
   namespace. */
namespace {
  struct st_table_data {
    char const *db;
    char const *name;
  };

  static int table_name_compare(void const *a, void const *b)
  {
    st_table_data const *x = (st_table_data const*) a;
    st_table_data const *y = (st_table_data const*) b;

    /* Doing lexical compare in order (db,name) */
    int const res= strcmp(x->db, y->db);
    return res != 0 ? res : strcmp(x->name, y->name);
  }

  bool check_table_binlog_row_based(THD *thd, TABLE *table)
  {
    static st_table_data const ignore[] = {
      { "mysql", "event" },
      { "mysql", "general_log" },
      { "mysql", "slow_log" }
    };

    my_size_t const ignore_size = sizeof(ignore)/sizeof(*ignore);
    st_table_data const item = { table->s->db.str, table->s->table_name.str };

    if (table->s->cached_row_logging_check == -1)
      table->s->cached_row_logging_check=
        (table->s->tmp_table == NO_TMP_TABLE) &&
        binlog_filter->db_ok(table->s->db.str) &&
        bsearch(&item, ignore, ignore_size,
                sizeof(st_table_data), table_name_compare) == NULL;

    DBUG_ASSERT(table->s->cached_row_logging_check == 0 ||
                table->s->cached_row_logging_check == 1);

    return (thd->current_stmt_binlog_row_based &&
            (thd->options & OPTION_BIN_LOG) &&
            mysql_bin_log.is_open() &&
            table->s->cached_row_logging_check);
  }
}

/*
   Write table maps for all (manually or automatically) locked tables
   to the binary log.

   SYNOPSIS
     write_locked_table_maps()
       thd     Pointer to THD structure

   DESCRIPTION
       This function will generate and write table maps for all tables
       that are locked by the thread 'thd'.  Either manually locked
       (stored in THD::locked_tables) and automatically locked (stored
       in THD::lock) are considered.
   
   RETURN VALUE
       0   All OK
       1   Failed to write all table maps

   SEE ALSO
       THD::lock
       THD::locked_tables
 */
namespace
{
  int write_locked_table_maps(THD *thd)
  {
    DBUG_ENTER("write_locked_table_maps");
    DBUG_PRINT("enter", ("thd=%p, thd->lock=%p, thd->locked_tables=%p, thd->extra_lock",
                         thd, thd->lock, thd->locked_tables, thd->extra_lock));

    if (thd->get_binlog_table_maps() == 0)
    {
      MYSQL_LOCK *locks[3];
      locks[0]= thd->extra_lock;
      locks[1]= thd->lock;
      locks[2]= thd->locked_tables;
      for (uint i= 0 ; i < sizeof(locks)/sizeof(*locks) ; ++i )
      {
        MYSQL_LOCK const *const lock= locks[i];
        if (lock == NULL)
          continue;

        TABLE **const end_ptr= lock->table + lock->table_count;
        for (TABLE **table_ptr= lock->table ; 
             table_ptr != end_ptr ;
             ++table_ptr)
        {
          TABLE *const table= *table_ptr;
          DBUG_PRINT("info", ("Checking table %s", table->s->table_name));
          if (table->current_lock == F_WRLCK &&
              check_table_binlog_row_based(thd, table))
          {
            int const has_trans= table->file->has_transactions();
            int const error= thd->binlog_write_table_map(table, has_trans);
            /*
              If an error occurs, it is the responsibility of the caller to
              roll back the transaction.
            */
            if (unlikely(error))
              DBUG_RETURN(1);
          }
        }
      }
    }
    DBUG_RETURN(0);
  }

  template<class RowsEventT> int
  binlog_log_row(TABLE* table,
                 const byte *before_record,
                 const byte *after_record)
  {
    if (table->file->ha_table_flags() & HA_HAS_OWN_BINLOGGING)
      return 0;
    bool error= 0;
    THD *const thd= table->in_use;

    if (check_table_binlog_row_based(thd, table))
    {
      MY_BITMAP cols;
      /* Potential buffer on the stack for the bitmap */
      uint32 bitbuf[BITMAP_STACKBUF_SIZE/sizeof(uint32)];
      uint n_fields= table->s->fields;
      my_bool use_bitbuf= n_fields <= sizeof(bitbuf)*8;

      /*
        If there are no table maps written to the binary log, this is
        the first row handled in this statement. In that case, we need
        to write table maps for all locked tables to the binary log.
      */
      if (likely(!(error= bitmap_init(&cols,
                                      use_bitbuf ? bitbuf : NULL,
                                      (n_fields + 7) & ~7UL,
                                      FALSE))))
      {
        bitmap_set_all(&cols);
        if (likely(!(error= write_locked_table_maps(thd))))
        {
          error=
            RowsEventT::binlog_row_logging_function(thd, table,
                                                    table->file->
                                                    has_transactions(),
                                                    &cols, table->s->fields,
                                                    before_record,
                                                    after_record);
        }
        if (!use_bitbuf)
          bitmap_free(&cols);
      }
    }
    return error ? HA_ERR_RBR_LOGGING_FAILED : 0;
  }

  /*
    Instantiate the versions we need for the above template function,
    because we have -fno-implicit-template as compiling option.
  */

  template int
  binlog_log_row<Write_rows_log_event>(TABLE *, const byte *, const byte *);

  template int
  binlog_log_row<Delete_rows_log_event>(TABLE *, const byte *, const byte *);

  template int
  binlog_log_row<Update_rows_log_event>(TABLE *, const byte *, const byte *);
}

#endif /* HAVE_ROW_BASED_REPLICATION */

int handler::ha_external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("handler::ha_external_lock");
  /*
    Whether this is lock or unlock, this should be true, and is to verify that
    if get_auto_increment() was called (thus may have reserved intervals or
    taken a table lock), ha_release_auto_increment() was too.
  */
  DBUG_ASSERT(next_insert_id == 0);
  DBUG_RETURN(external_lock(thd, lock_type));
}


/*
  Check handler usage and reset state of file to after 'open'
*/

int handler::ha_reset()
{
  DBUG_ENTER("ha_reset");
  /* Check that we have called all proper delallocation functions */
  DBUG_ASSERT((byte*) table->def_read_set.bitmap +
              table->s->column_bitmap_size ==
              (char*) table->def_write_set.bitmap);
  DBUG_ASSERT(bitmap_is_set_all(&table->s->all_set));
  DBUG_ASSERT(table->key_read == 0);
  /* ensure that ha_index_end / ha_rnd_end has been called */
  DBUG_ASSERT(inited == NONE);
  /* Free cache used by filesort */
  free_io_cache(table);
  DBUG_RETURN(reset());
}


int handler::ha_write_row(byte *buf)
{
  int error;
  if (unlikely(error= write_row(buf)))
    return error;
#ifdef HAVE_ROW_BASED_REPLICATION
  if (unlikely(error= binlog_log_row<Write_rows_log_event>(table, 0, buf)))
    return error;
#endif
  return 0;
}

int handler::ha_update_row(const byte *old_data, byte *new_data)
{
  int error;

  /*
    Some storage engines require that the new record is in record[0]
    (and the old record is in record[1]).
   */
  DBUG_ASSERT(new_data == table->record[0]);

  if (unlikely(error= update_row(old_data, new_data)))
    return error;
#ifdef HAVE_ROW_BASED_REPLICATION
  if (unlikely(error= binlog_log_row<Update_rows_log_event>(table, old_data, new_data)))
    return error;
#endif
  return 0;
}

int handler::ha_delete_row(const byte *buf)
{
  int error;
  if (unlikely(error= delete_row(buf)))
    return error;
#ifdef HAVE_ROW_BASED_REPLICATION
  if (unlikely(error= binlog_log_row<Delete_rows_log_event>(table, buf, 0)))
    return error;
#endif
  return 0;
}



/*
  use_hidden_primary_key() is called in case of an update/delete when
  (table_flags() and HA_PRIMARY_KEY_REQUIRED_FOR_DELETE) is defined
  but we don't have a primary key
*/

void handler::use_hidden_primary_key()
{
  /* fallback to use all columns in the table to identify row */
  table->use_all_columns();
}


/*
  Dummy function which accept information about log files which is not need
  by handlers
*/

void signal_log_not_needed(struct handlerton, char *log_file)
{
  DBUG_ENTER("signal_log_not_needed");
  DBUG_PRINT("enter", ("logfile '%s'", log_file));
  DBUG_VOID_RETURN;
}


#ifdef TRANS_LOG_MGM_EXAMPLE_CODE
/*
  Example of transaction log management functions based on assumption that logs
  placed into a directory
*/
#include <my_dir.h>
#include <my_sys.h>
int example_of_iterator_using_for_logs_cleanup(handlerton *hton)
{
  void *buffer;
  int res= 1;
  struct handler_iterator iterator;
  struct handler_log_file_data data;

  if (!hton->create_iterator)
    return 1; /* iterator creator is not supported */

  if ((*hton->create_iterator)(HA_TRANSACTLOG_ITERATOR, &iterator) !=
      HA_ITERATOR_OK)
  {
    /* error during creation of log iterator or iterator is not supported */
    return 1;
  }
  while((*iterator.next)(&iterator, (void*)&data) == 0)
  {
    printf("%s\n", data.filename.str);
    if (data.status == HA_LOG_STATUS_FREE &&
        my_delete(data.filename.str, MYF(MY_WME)))
      goto err;
  }
  res= 0;
err:
  (*iterator.destroy)(&iterator);
  return res;
}


/*
  Here we should get info from handler where it save logs but here is
  just example, so we use constant.
  IMHO FN_ROOTDIR ("/") is safe enough for example, because nobody has
  rights on it except root and it consist of directories only at lest for
  *nix (sorry, can't find windows-safe solution here, but it is only example).
*/
#define fl_dir FN_ROOTDIR


/*
  Dummy function to return log status should be replaced by function which
  really detect the log status and check that the file is a log of this
  handler.
*/

enum log_status fl_get_log_status(char *log)
{
  MY_STAT stat_buff;
  if (my_stat(log, &stat_buff, MYF(0)))
    return HA_LOG_STATUS_INUSE;
  return HA_LOG_STATUS_NOSUCHLOG;
}


struct fl_buff
{
  LEX_STRING *names;
  enum log_status *statuses;
  uint32 entries;
  uint32 current;
};


int fl_log_iterator_next(struct handler_iterator *iterator,
                          void *iterator_object)
{
  struct fl_buff *buff= (struct fl_buff *)iterator->buffer;
  struct handler_log_file_data *data=
    (struct handler_log_file_data *) iterator_object;
  if (buff->current >= buff->entries)
    return 1;
  data->filename= buff->names[buff->current];
  data->status= buff->statuses[buff->current];
  buff->current++;
  return 0;
}


void fl_log_iterator_destroy(struct handler_iterator *iterator)
{
  my_free((gptr)iterator->buffer, MYF(MY_ALLOW_ZERO_PTR));
}


/*
  returns buffer, to be assigned in handler_iterator struct
*/
enum handler_create_iterator_result
fl_log_iterator_buffer_init(struct handler_iterator *iterator)
{
  MY_DIR *dirp;
  struct fl_buff *buff;
  char *name_ptr;
  byte *ptr;
  FILEINFO *file;
  uint32 i;

  /* to be able to make my_free without crash in case of error */
  iterator->buffer= 0;

  if (!(dirp = my_dir(fl_dir, MYF(0))))
  {
    return HA_ITERATOR_ERROR;
  }
  if ((ptr= (byte*)my_malloc(ALIGN_SIZE(sizeof(fl_buff)) +
                             ((ALIGN_SIZE(sizeof(LEX_STRING)) +
                               sizeof(enum log_status) +
                               + FN_REFLEN) *
                              (uint) dirp->number_off_files),
                             MYF(0))) == 0)
  {
    return HA_ITERATOR_ERROR;
  }
  buff= (struct fl_buff *)ptr;
  buff->entries= buff->current= 0;
  ptr= ptr + (ALIGN_SIZE(sizeof(fl_buff)));
  buff->names= (LEX_STRING*) (ptr);
  ptr= ptr + ((ALIGN_SIZE(sizeof(LEX_STRING)) *
               (uint) dirp->number_off_files));
  buff->statuses= (enum log_status *)(ptr);
  name_ptr= (char *)(ptr + (sizeof(enum log_status) *
                            (uint) dirp->number_off_files));
  for (i=0 ; i < (uint) dirp->number_off_files  ; i++)
  {
    enum log_status st;
    file= dirp->dir_entry + i;
    if ((file->name[0] == '.' &&
         ((file->name[1] == '.' && file->name[2] == '\0') ||
            file->name[1] == '\0')))
      continue;
    if ((st= fl_get_log_status(file->name)) == HA_LOG_STATUS_NOSUCHLOG)
      continue;
    name_ptr= strxnmov(buff->names[buff->entries].str= name_ptr,
                       FN_REFLEN, fl_dir, file->name, NullS);
    buff->names[buff->entries].length= (name_ptr -
                                        buff->names[buff->entries].str) - 1;
    buff->statuses[buff->entries]= st;
    buff->entries++;
  }

  iterator->buffer= buff;
  iterator->next= &fl_log_iterator_next;
  iterator->destroy= &fl_log_iterator_destroy;
  return HA_ITERATOR_OK;
}


/* An example of a iterator creator */
enum handler_create_iterator_result
fl_create_iterator(enum handler_iterator_type type,
                   struct handler_iterator *iterator)
{
  switch(type) {
  case HA_TRANSACTLOG_ITERATOR:
    return fl_log_iterator_buffer_init(iterator);
  default:
    return HA_ITERATOR_UNSUPPORTED;
  }
}
#endif /*TRANS_LOG_MGM_EXAMPLE_CODE*/
