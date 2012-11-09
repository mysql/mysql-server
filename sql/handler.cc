/* Copyright (c) 2000, 2011, Oracle and/or its affiliates.
   Copyright (c) 2009-2011 Monty Program Ab

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

/** @file handler.cc

    @brief
  Handler-calling-functions
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "create_options.h"
#include "rpl_filter.h"
#include <myisampack.h>
#include "myisam.h"

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

/* number of entries in handlertons[] */
ulong total_ha= 0;
/* number of storage engines (from handlertons[]) that support 2pc */
ulong total_ha_2pc= 0;
/* size of savepoint storage area (see ha_init) */
ulong savepoint_alloc_size= 0;

static const LEX_STRING sys_table_aliases[]=
{
  { C_STRING_WITH_LEN("INNOBASE") },  { C_STRING_WITH_LEN("INNODB") },
  { C_STRING_WITH_LEN("NDB") },       { C_STRING_WITH_LEN("NDBCLUSTER") },
  { C_STRING_WITH_LEN("HEAP") },      { C_STRING_WITH_LEN("MEMORY") },
  { C_STRING_WITH_LEN("MERGE") },     { C_STRING_WITH_LEN("MRG_MYISAM") },
  { C_STRING_WITH_LEN("Maria") },      { C_STRING_WITH_LEN("Aria") },
  {NullS, 0}
};

const char *ha_row_type[] = {
  "", "FIXED", "DYNAMIC", "COMPRESSED", "REDUNDANT", "COMPACT",
  "PAGE",
  "?","?","?"
};

const char *tx_isolation_names[] =
{ "READ-UNCOMMITTED", "READ-COMMITTED", "REPEATABLE-READ", "SERIALIZABLE",
  NullS};
TYPELIB tx_isolation_typelib= {array_elements(tx_isolation_names)-1,"",
			       tx_isolation_names, NULL};

static TYPELIB known_extensions= {0,"known_exts", NULL, NULL};
uint known_extensions_id= 0;

static int commit_one_phase_2(THD *thd, bool all, THD_TRANS *trans,
                              bool is_real_trans);


static plugin_ref ha_default_plugin(THD *thd)
{
  if (thd->variables.table_plugin)
    return thd->variables.table_plugin;
  return my_plugin_lock(thd, global_system_variables.table_plugin);
}


/** @brief
  Return the default storage engine handlerton for thread

  SYNOPSIS
    ha_default_handlerton(thd)
    thd         current thread

  RETURN
    pointer to handlerton
*/
handlerton *ha_default_handlerton(THD *thd)
{
  plugin_ref plugin= ha_default_plugin(thd);
  DBUG_ASSERT(plugin);
  handlerton *hton= plugin_data(plugin, handlerton*);
  DBUG_ASSERT(hton);
  return hton;
}


/** @brief
  Return the storage engine handlerton for the supplied name
  
  SYNOPSIS
    ha_resolve_by_name(thd, name)
    thd         current thread
    name        name of storage engine
  
  RETURN
    pointer to storage engine plugin handle
*/
plugin_ref ha_resolve_by_name(THD *thd, const LEX_STRING *name)
{
  const LEX_STRING *table_alias;
  plugin_ref plugin;

redo:
  /* my_strnncoll is a macro and gcc doesn't do early expansion of macro */
  if (thd && !my_charset_latin1.coll->strnncoll(&my_charset_latin1,
                           (const uchar *)name->str, name->length,
                           (const uchar *)STRING_WITH_LEN("DEFAULT"), 0))
    return ha_default_plugin(thd);

  if ((plugin= my_plugin_lock_by_name(thd, name, MYSQL_STORAGE_ENGINE_PLUGIN)))
  {
    handlerton *hton= plugin_data(plugin, handlerton *);
    if (!(hton->flags & HTON_NOT_USER_SELECTABLE))
      return plugin;
      
    /*
      unlocking plugin immediately after locking is relatively low cost.
    */
    plugin_unlock(thd, plugin);
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


plugin_ref ha_lock_engine(THD *thd, const handlerton *hton)
{
  if (hton)
  {
    st_plugin_int *plugin= hton2plugin[hton->slot];
    return my_plugin_lock(thd, plugin_int_to_ref(plugin));
  }
  return NULL;
}


#ifdef NOT_USED
static handler *create_default(TABLE_SHARE *table, MEM_ROOT *mem_root)
{
  handlerton *hton= ha_default_handlerton(current_thd);
  return (hton && hton->create) ? hton->create(hton, table, mem_root) : NULL;
}
#endif


handlerton *ha_resolve_by_legacy_type(THD *thd, enum legacy_db_type db_type)
{
  plugin_ref plugin;
  switch (db_type) {
  case DB_TYPE_DEFAULT:
    return ha_default_handlerton(thd);
  default:
    if (db_type > DB_TYPE_UNKNOWN && db_type < DB_TYPE_DEFAULT &&
        (plugin= ha_lock_engine(thd, installed_htons[db_type])))
      return plugin_data(plugin, handlerton*);
    /* fall through */
  case DB_TYPE_UNKNOWN:
    return NULL;
  }
}


/**
  Use other database handler if databasehandler is not compiled in.
*/
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
      const char *engine_name= ha_resolve_storage_engine_name(hton);
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
    if ((file= db_type->create(db_type, share, alloc)))
      file->init();
    DBUG_RETURN(file);
  }
  /*
    Try the default table type
    Here the call to current_thd() is ok as we call this function a lot of
    times but we enter this branch very seldom.
  */
  DBUG_RETURN(get_new_handler(share, alloc, ha_default_handlerton(current_thd)));
}


#ifdef WITH_PARTITION_STORAGE_ENGINE
handler *get_ha_partition(partition_info *part_info)
{
  ha_partition *partition;
  DBUG_ENTER("get_ha_partition");
  if ((partition= new ha_partition(partition_hton, part_info)))
  {
    if (partition->initialize_partition(current_thd->mem_root))
    {
      delete partition;
      partition= 0;
    }
    else
      partition->init();
  }
  else
  {
    my_error(ER_OUTOFMEMORY, MYF(0), static_cast<int>(sizeof(ha_partition)));
  }
  DBUG_RETURN(((handler*) partition));
}
#endif


/**
  Register handler error messages for use with my_error().

  @retval
    0           OK
  @retval
    !=0         Error
*/

int ha_init_errors(void)
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
  SETMSG(HA_ERR_AUTOINC_READ_FAILED,    ER(ER_AUTOINC_READ_FAILED));
  SETMSG(HA_ERR_AUTOINC_ERANGE,         ER(ER_WARN_DATA_OUT_OF_RANGE));
  SETMSG(HA_ERR_TOO_MANY_CONCURRENT_TRXS, ER(ER_TOO_MANY_CONCURRENT_TRXS));
  SETMSG(HA_ERR_DISK_FULL,              ER(ER_DISK_FULL));
  SETMSG(HA_ERR_TABLE_IN_FK_CHECK,      "Table being used in foreign key check");

  /* Register the error messages for use with my_error(). */
  return my_error_register(errmsgs, HA_ERR_FIRST, HA_ERR_LAST);
}


/**
  Unregister handler error messages.

  @retval
    0           OK
  @retval
    !=0         Error
*/
static int ha_finish_errors(void)
{
  const char    **errmsgs;

  /* Allocate a pointer array for the error message strings. */
  if (! (errmsgs= my_error_unregister(HA_ERR_FIRST, HA_ERR_LAST)))
    return 1;
  my_free((uchar*) errmsgs, MYF(0));
  return 0;
}


int ha_finalize_handlerton(st_plugin_int *plugin)
{
  handlerton *hton= (handlerton *)plugin->data;
  DBUG_ENTER("ha_finalize_handlerton");

  /* hton can be NULL here, if ha_initialize_handlerton() failed. */
  if (!hton)
    goto end;

  switch (hton->state) {
  case SHOW_OPTION_NO:
  case SHOW_OPTION_DISABLED:
    break;
  case SHOW_OPTION_YES:
    if (installed_htons[hton->db_type] == hton)
      installed_htons[hton->db_type]= NULL;
    break;
  };

  if (hton->panic)
    hton->panic(hton, HA_PANIC_CLOSE);

  if (plugin->plugin->deinit)
  {
    /*
      Today we have no defined/special behavior for uninstalling
      engine plugins.
    */
    DBUG_PRINT("info", ("Deinitializing plugin: '%s'", plugin->name.str));
    if (plugin->plugin->deinit(NULL))
    {
      DBUG_PRINT("warning", ("Plugin '%s' deinit function returned error.",
                             plugin->name.str));
    }
  }

  /*
    In case a plugin is uninstalled and re-installed later, it should
    reuse an array slot. Otherwise the number of uninstall/install
    cycles would be limited.
  */
  hton2plugin[hton->slot]= NULL;

  my_free((uchar*)hton, MYF(0));

 end:
  DBUG_RETURN(0);
}


int ha_initialize_handlerton(st_plugin_int *plugin)
{
  handlerton *hton;
  DBUG_ENTER("ha_initialize_handlerton");
  DBUG_PRINT("plugin", ("initialize plugin: '%s'", plugin->name.str));

  hton= (handlerton *)my_malloc(sizeof(handlerton),
                                MYF(MY_WME | MY_ZEROFILL));
  /* Historical Requirement */
  plugin->data= hton; // shortcut for the future
  if (plugin->plugin->init && plugin->plugin->init(hton))
  {
    sql_print_error("Plugin '%s' init function returned error.",
		    plugin->name.str);
    goto err;
  }

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
      ulong fslot;
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
          my_free(hton, MYF(0));
          plugin->data= 0;
	  goto err_deinit;
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

      /*
        In case a plugin is uninstalled and re-installed later, it should
        reuse an array slot. Otherwise the number of uninstall/install
        cycles would be limited. So look for a free slot.
      */
      DBUG_PRINT("plugin", ("total_ha: %lu", total_ha));
      for (fslot= 0; fslot < total_ha; fslot++)
      {
        if (!hton2plugin[fslot])
          break;
      }
      if (fslot < total_ha)
        hton->slot= fslot;
      else
      {
        if (total_ha >= MAX_HA)
        {
          sql_print_error("Too many plugins loaded. Limit is %lu. "
                          "Failed on '%s'", (ulong) MAX_HA, plugin->name.str);
          goto err_deinit;
        }
        hton->slot= total_ha++;
      }
      installed_htons[hton->db_type]= hton;
      tmp= hton->savepoint_offset;
      hton->savepoint_offset= savepoint_alloc_size;
      savepoint_alloc_size+= tmp;
      hton2plugin[hton->slot]=plugin;
      if (hton->prepare)
        total_ha_2pc++;
      break;
    }
    /* fall through */
  default:
    hton->state= SHOW_OPTION_DISABLED;
    break;
  }
  
  /* 
    This is entirely for legacy. We will create a new "disk based" hton and a 
    "memory" hton which will be configurable longterm. We should be able to 
    remove partition and myisammrg.
  */
  switch (hton->db_type) {
  case DB_TYPE_HEAP:
    heap_hton= hton;
    break;
  case DB_TYPE_MYISAM:
    myisam_hton= hton;
    break;
  case DB_TYPE_PARTITION_DB:
    partition_hton= hton;
    break;
  default:
    break;
  };

  DBUG_RETURN(0);

err_deinit:
  /* 
    Let plugin do its inner deinitialization as plugin->init() 
    was successfully called before.
  */
  if (plugin->plugin->deinit)
    (void) plugin->plugin->deinit(NULL);
          
err:
  my_free((uchar*) hton, MYF(0));
  plugin->data= NULL;
  DBUG_RETURN(1);
}

int ha_init()
{
  int error= 0;
  DBUG_ENTER("ha_init");

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

int ha_end()
{
  int error= 0;
  DBUG_ENTER("ha_end");


  /* 
    This should be eventualy based  on the graceful shutdown flag.
    So if flag is equal to HA_PANIC_CLOSE, the deallocate
    the errors.
  */
  if (ha_finish_errors())
    error= 1;

  DBUG_RETURN(error);
}

static my_bool dropdb_handlerton(THD *unused1, plugin_ref plugin,
                                 void *path)
{
  handlerton *hton= plugin_data(plugin, handlerton *);
  if (hton->state == SHOW_OPTION_YES && hton->drop_database)
    hton->drop_database(hton, (char *)path);
  return FALSE;
}


void ha_drop_database(char* path)
{
  plugin_foreach(NULL, dropdb_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, path);
}


static my_bool checkpoint_state_handlerton(THD *unused1, plugin_ref plugin,
                                           void *disable)
{
  handlerton *hton= plugin_data(plugin, handlerton *);
  if (hton->state == SHOW_OPTION_YES && hton->checkpoint_state)
    hton->checkpoint_state(hton, (int) *(bool*) disable);
  return FALSE;
}


void ha_checkpoint_state(bool disable)
{
  plugin_foreach(NULL, checkpoint_state_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, &disable);
}



static my_bool closecon_handlerton(THD *thd, plugin_ref plugin,
                                   void *unused)
{
  handlerton *hton= plugin_data(plugin, handlerton *);
  /*
    there's no need to rollback here as all transactions must
    be rolled back already
  */
  if (hton->state == SHOW_OPTION_YES && thd_get_ha_data(thd, hton))
  {
    if (hton->close_connection)
      hton->close_connection(hton, thd);
    /* make sure ha_data is reset and ha_data_lock is released */
    thd_set_ha_data(thd, hton, NULL);
  }
  return FALSE;
}


/**
  @note
    don't bother to rollback here, it's done already
*/
void ha_close_connection(THD* thd)
{
  plugin_foreach(thd, closecon_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, 0);
}

/* ========================================================================
 ======================= TRANSACTIONS ===================================*/

/**
  Transaction handling in the server
  ==================================

  In each client connection, MySQL maintains two transactional
  states:
  - a statement transaction,
  - a standard, also called normal transaction.

  Historical note
  ---------------
  "Statement transaction" is a non-standard term that comes
  from the times when MySQL supported BerkeleyDB storage engine.

  First of all, it should be said that in BerkeleyDB auto-commit
  mode auto-commits operations that are atomic to the storage
  engine itself, such as a write of a record, and are too
  high-granular to be atomic from the application perspective
  (MySQL). One SQL statement could involve many BerkeleyDB
  auto-committed operations and thus BerkeleyDB auto-commit was of
  little use to MySQL.

  Secondly, instead of SQL standard savepoints, BerkeleyDB
  provided the concept of "nested transactions". In a nutshell,
  transactions could be arbitrarily nested, but when the parent
  transaction was committed or aborted, all its child (nested)
  transactions were handled committed or aborted as well.
  Commit of a nested transaction, in turn, made its changes
  visible, but not durable: it destroyed the nested transaction,
  all its changes would become available to the parent and
  currently active nested transactions of this parent.

  So the mechanism of nested transactions was employed to
  provide "all or nothing" guarantee of SQL statements
  required by the standard.
  A nested transaction would be created at start of each SQL
  statement, and destroyed (committed or aborted) at statement
  end. Such nested transaction was internally referred to as
  a "statement transaction" and gave birth to the term.

  <Historical note ends>

  Since then a statement transaction is started for each statement
  that accesses transactional tables or uses the binary log.  If
  the statement succeeds, the statement transaction is committed.
  If the statement fails, the transaction is rolled back. Commits
  of statement transactions are not durable -- each such
  transaction is nested in the normal transaction, and if the
  normal transaction is rolled back, the effects of all enclosed
  statement transactions are undone as well.  Technically,
  a statement transaction can be viewed as a savepoint which is
  maintained automatically in order to make effects of one
  statement atomic.

  The normal transaction is started by the user and is ended
  usually upon a user request as well. The normal transaction
  encloses transactions of all statements issued between
  its beginning and its end.
  In autocommit mode, the normal transaction is equivalent
  to the statement transaction.

  Since MySQL supports PSEA (pluggable storage engine
  architecture), more than one transactional engine can be
  active at a time. Hence transactions, from the server
  point of view, are always distributed. In particular,
  transactional state is maintained independently for each
  engine. In order to commit a transaction the two phase
  commit protocol is employed.

  Not all statements are executed in context of a transaction.
  Administrative and status information statements do not modify
  engine data, and thus do not start a statement transaction and
  also have no effect on the normal transaction. Examples of such
  statements are SHOW STATUS and RESET SLAVE.

  Similarly DDL statements are not transactional,
  and therefore a transaction is [almost] never started for a DDL
  statement. The difference between a DDL statement and a purely
  administrative statement though is that a DDL statement always
  commits the current transaction before proceeding, if there is
  any.

  At last, SQL statements that work with non-transactional
  engines also have no effect on the transaction state of the
  connection. Even though they are written to the binary log,
  and the binary log is, overall, transactional, the writes
  are done in "write-through" mode, directly to the binlog
  file, followed with a OS cache sync, in other words,
  bypassing the binlog undo log (translog).
  They do not commit the current normal transaction.
  A failure of a statement that uses non-transactional tables
  would cause a rollback of the statement transaction, but
  in case there no non-transactional tables are used,
  no statement transaction is started.

  Data layout
  -----------

  The server stores its transaction-related data in
  thd->transaction. This structure has two members of type
  THD_TRANS. These members correspond to the statement and
  normal transactions respectively:

  - thd->transaction.stmt contains a list of engines
  that are participating in the given statement
  - thd->transaction.all contains a list of engines that
  have participated in any of the statement transactions started
  within the context of the normal transaction.
  Each element of the list contains a pointer to the storage
  engine, engine-specific transactional data, and engine-specific
  transaction flags.

  In autocommit mode thd->transaction.all is empty.
  Instead, data of thd->transaction.stmt is
  used to commit/rollback the normal transaction.

  The list of registered engines has a few important properties:
  - no engine is registered in the list twice
  - engines are present in the list a reverse temporal order --
  new participants are always added to the beginning of the list.

  Transaction life cycle
  ----------------------

  When a new connection is established, thd->transaction
  members are initialized to an empty state.
  If a statement uses any tables, all affected engines
  are registered in the statement engine list. In
  non-autocommit mode, the same engines are registered in
  the normal transaction list.
  At the end of the statement, the server issues a commit
  or a roll back for all engines in the statement list.
  At this point transaction flags of an engine, if any, are
  propagated from the statement list to the list of the normal
  transaction.
  When commit/rollback is finished, the statement list is
  cleared. It will be filled in again by the next statement,
  and emptied again at the next statement's end.

  The normal transaction is committed in a similar way
  (by going over all engines in thd->transaction.all list)
  but at different times:
  - upon COMMIT SQL statement is issued by the user
  - implicitly, by the server, at the beginning of a DDL statement
  or SET AUTOCOMMIT={0|1} statement.

  The normal transaction can be rolled back as well:
  - if the user has requested so, by issuing ROLLBACK SQL
  statement
  - if one of the storage engines requested a rollback
  by setting thd->transaction_rollback_request. This may
  happen in case, e.g., when the transaction in the engine was
  chosen a victim of the internal deadlock resolution algorithm
  and rolled back internally. When such a situation happens, there
  is little the server can do and the only option is to rollback
  transactions in all other participating engines.  In this case
  the rollback is accompanied by an error sent to the user.

  As follows from the use cases above, the normal transaction
  is never committed when there is an outstanding statement
  transaction. In most cases there is no conflict, since
  commits of the normal transaction are issued by a stand-alone
  administrative or DDL statement, thus no outstanding statement
  transaction of the previous statement exists. Besides,
  all statements that manipulate with the normal transaction
  are prohibited in stored functions and triggers, therefore
  no conflicting situation can occur in a sub-statement either.
  The remaining rare cases when the server explicitly has
  to commit the statement transaction prior to committing the normal
  one cover error-handling scenarios (see for example
  SQLCOM_LOCK_TABLES).

  When committing a statement or a normal transaction, the server
  either uses the two-phase commit protocol, or issues a commit
  in each engine independently. The two-phase commit protocol
  is used only if:
  - all participating engines support two-phase commit (provide
    handlerton::prepare PSEA API call) and
  - transactions in at least two engines modify data (i.e. are
  not read-only).

  Note that the two phase commit is used for
  statement transactions, even though they are not durable anyway.
  This is done to ensure logical consistency of data in a multiple-
  engine transaction.
  For example, imagine that some day MySQL supports unique
  constraint checks deferred till the end of statement. In such
  case a commit in one of the engines may yield ER_DUP_KEY,
  and MySQL should be able to gracefully abort statement
  transactions of other participants.

  After the normal transaction has been committed,
  thd->transaction.all list is cleared.

  When a connection is closed, the current normal transaction, if
  any, is rolled back.

  Roles and responsibilities
  --------------------------

  The server has no way to know that an engine participates in
  the statement and a transaction has been started
  in it unless the engine says so. Thus, in order to be
  a part of a transaction, the engine must "register" itself.
  This is done by invoking trans_register_ha() server call.
  Normally the engine registers itself whenever handler::external_lock()
  is called. trans_register_ha() can be invoked many times: if
  an engine is already registered, the call does nothing.
  In case autocommit is not set, the engine must register itself
  twice -- both in the statement list and in the normal transaction
  list.
  In which list to register is a parameter of trans_register_ha().

  Note, that although the registration interface in itself is
  fairly clear, the current usage practice often leads to undesired
  effects. E.g. since a call to trans_register_ha() in most engines
  is embedded into implementation of handler::external_lock(), some
  DDL statements start a transaction (at least from the server
  point of view) even though they are not expected to. E.g.
  CREATE TABLE does not start a transaction, since
  handler::external_lock() is never called during CREATE TABLE. But
  CREATE TABLE ... SELECT does, since handler::external_lock() is
  called for the table that is being selected from. This has no
  practical effects currently, but must be kept in mind
  nevertheless.

  Once an engine is registered, the server will do the rest
  of the work.

  During statement execution, whenever any of data-modifying
  PSEA API methods is used, e.g. handler::write_row() or
  handler::update_row(), the read-write flag is raised in the
  statement transaction for the involved engine.
  Currently All PSEA calls are "traced", and the data can not be
  changed in a way other than issuing a PSEA call. Important:
  unless this invariant is preserved the server will not know that
  a transaction in a given engine is read-write and will not
  involve the two-phase commit protocol!

  At the end of a statement, server call
  ha_autocommit_or_rollback() is invoked. This call in turn
  invokes handlerton::prepare() for every involved engine.
  Prepare is followed by a call to handlerton::commit_one_phase()
  If a one-phase commit will suffice, handlerton::prepare() is not
  invoked and the server only calls handlerton::commit_one_phase().
  At statement commit, the statement-related read-write engine
  flag is propagated to the corresponding flag in the normal
  transaction.  When the commit is complete, the list of registered
  engines is cleared.

  Rollback is handled in a similar fashion.

  Additional notes on DDL and the normal transaction.
  ---------------------------------------------------

  DDLs and operations with non-transactional engines
  do not "register" in thd->transaction lists, and thus do not
  modify the transaction state. Besides, each DDL in
  MySQL is prefixed with an implicit normal transaction commit
  (a call to end_active_trans()), and thus leaves nothing
  to modify.
  However, as it has been pointed out with CREATE TABLE .. SELECT,
  some DDL statements can start a *new* transaction.

  Behaviour of the server in this case is currently badly
  defined.
  DDL statements use a form of "semantic" logging
  to maintain atomicity: if CREATE TABLE .. SELECT failed,
  the newly created table is deleted.
  In addition, some DDL statements issue interim transaction
  commits: e.g. ALTER TABLE issues a commit after data is copied
  from the original table to the internal temporary table. Other
  statements, e.g. CREATE TABLE ... SELECT do not always commit
  after itself.
  And finally there is a group of DDL statements such as
  RENAME/DROP TABLE that doesn't start a new transaction
  and doesn't commit.

  This diversity makes it hard to say what will happen if
  by chance a stored function is invoked during a DDL --
  whether any modifications it makes will be committed or not
  is not clear. Fortunately, SQL grammar of few DDLs allows
  invocation of a stored function.

  A consistent behaviour is perhaps to always commit the normal
  transaction after all DDLs, just like the statement transaction
  is always committed at the end of all statements.
*/

/**
  Register a storage engine for a transaction.

  Every storage engine MUST call this function when it starts
  a transaction or a statement (that is it must be called both for the
  "beginning of transaction" and "beginning of statement").
  Only storage engines registered for the transaction/statement
  will know when to commit/rollback it.

  @note
    trans_register_ha is idempotent - storage engine may register many
    times per transaction.

*/
void trans_register_ha(THD *thd, bool all, handlerton *ht_arg)
{
  THD_TRANS *trans;
  Ha_trx_info *ha_info;
  DBUG_ENTER("trans_register_ha");
  DBUG_PRINT("enter",("%s", all ? "all" : "stmt"));

  if (all)
  {
    trans= &thd->transaction.all;
    thd->server_status|= SERVER_STATUS_IN_TRANS;
  }
  else
    trans= &thd->transaction.stmt;

  ha_info= thd->ha_data[ht_arg->slot].ha_info + static_cast<unsigned>(all);

  if (ha_info->is_started())
    DBUG_VOID_RETURN; /* already registered, return */

  ha_info->register_ha(trans, ht_arg);

  trans->no_2pc|=(ht_arg->prepare==0);
  if (thd->transaction.xid_state.xid.is_null())
    thd->transaction.xid_state.xid.set(thd->query_id);
  DBUG_VOID_RETURN;
}

/**
  @retval
    0   ok
  @retval
    1   error, transaction was rolled back
*/
int ha_prepare(THD *thd)
{
  int error=0, all=1;
  THD_TRANS *trans=all ? &thd->transaction.all : &thd->transaction.stmt;
  Ha_trx_info *ha_info= trans->ha_list;
  DBUG_ENTER("ha_prepare");
#ifdef USING_TRANSACTIONS
  if (ha_info)
  {
    for (; ha_info; ha_info= ha_info->next())
    {
      int err;
      handlerton *ht= ha_info->ht();
      status_var_increment(thd->status_var.ha_prepare_count);
      if (ht->prepare)
      {
        if ((err= ht->prepare(ht, thd, all)))
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
                            ha_resolve_storage_engine_name(ht));
      }
    }
  }
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}

/**
  Check if we can skip the two-phase commit.

  A helper function to evaluate if two-phase commit is mandatory.
  As a side effect, propagates the read-only/read-write flags
  of the statement transaction to its enclosing normal transaction.
  
  If we have at least two engines with read-write changes we must
  run a two-phase commit. Otherwise we can run several independent
  commits as the only transactional engine has read-write changes
  and others are read-only.

  @retval   0   All engines are read-only.
  @retval   1   We have the only engine with read-write changes.
  @retval   >1  More than one engine have read-write changes.
                Note: return value might NOT be the exact number of
                engines with read-write changes.
*/

static
uint
ha_check_and_coalesce_trx_read_only(THD *thd, Ha_trx_info *ha_list,
                                    bool all)
{
  /* The number of storage engines that have actual changes. */
  unsigned rw_ha_count= 0;
  Ha_trx_info *ha_info;

  for (ha_info= ha_list; ha_info; ha_info= ha_info->next())
  {
    if (ha_info->is_trx_read_write())
      ++rw_ha_count;

    if (! all)
    {
      Ha_trx_info *ha_info_all= &thd->ha_data[ha_info->ht()->slot].ha_info[1];
      DBUG_ASSERT(ha_info != ha_info_all);
      /*
        Merge read-only/read-write information about statement
        transaction to its enclosing normal transaction. Do this
        only if in a real transaction -- that is, if we know
        that ha_info_all is registered in thd->transaction.all.
        Since otherwise we only clutter the normal transaction flags.
      */
      if (ha_info_all->is_started()) /* FALSE if autocommit. */
        ha_info_all->coalesce_trx_with(ha_info);
    }
    else if (rw_ha_count > 1)
    {
      /*
        It is a normal transaction, so we don't need to merge read/write
        information up, and the need for two-phase commit has been
        already established. Break the loop prematurely.
      */
      break;
    }
  }
  return rw_ha_count;
}


/**
  @retval
    0   ok
  @retval
    1   transaction was rolled back
  @retval
    2   error during commit, data may be inconsistent

  @todo
    Since we don't support nested statement transactions in 5.0,
    we can't commit or rollback stmt transactions while we are inside
    stored functions or triggers. So we simply do nothing now.
    TODO: This should be fixed in later ( >= 5.1) releases.
*/
int ha_commit_trans(THD *thd, bool all)
{
  int error= 0, cookie;
  /*
    'all' means that this is either an explicit commit issued by
    user, or an implicit commit issued by a DDL.
  */
  THD_TRANS *trans= all ? &thd->transaction.all : &thd->transaction.stmt;
  /*
    "real" is a nick name for a transaction for which a commit will
    make persistent changes. E.g. a 'stmt' transaction inside a 'all'
    transation is not 'real': even though it's possible to commit it,
    the changes are not durable as they might be rolled back if the
    enclosing 'all' transaction is rolled back.
  */
  bool is_real_trans= all || thd->transaction.all.ha_list == 0;
  Ha_trx_info *ha_info= trans->ha_list;
  bool need_prepare_ordered, need_commit_ordered;
  my_xid xid;
  DBUG_ENTER("ha_commit_trans");

  /* Just a random warning to test warnings pushed during autocommit. */
  DBUG_EXECUTE_IF("warn_during_ha_commit_trans",
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK)););

  /*
    We must not commit the normal transaction if a statement
    transaction is pending. Otherwise statement transaction
    flags will not get propagated to its normal transaction's
    counterpart.
  */
  DBUG_ASSERT(thd->transaction.stmt.ha_list == NULL ||
              trans == &thd->transaction.stmt);

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
  if (!ha_info)
  {
    /* Free resources and perform other cleanup even for 'empty' transactions. */
    if (is_real_trans)
      thd->transaction.cleanup();
    DBUG_RETURN(0);
  }

  DBUG_EXECUTE_IF("crash_commit_before", DBUG_SUICIDE(););

  /* Close all cursors that can not survive COMMIT */
  if (is_real_trans)                          /* not a statement commit */
    thd->stmt_map.close_transient_cursors();

  uint rw_ha_count= ha_check_and_coalesce_trx_read_only(thd, ha_info, all);
  /* rw_trans is TRUE when we in a transaction changing data */
  bool rw_trans= is_real_trans && (rw_ha_count > 0);

  if (rw_trans &&
      wait_if_global_read_lock(thd, 0, 0))
  {
    ha_rollback_trans(thd, all);
    DBUG_RETURN(1);
  }

  if (rw_trans &&
      opt_readonly &&
      !(thd->security_ctx->master_access & SUPER_ACL) &&
      !thd->slave_thread)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
    goto err;
  }

  if (trans->no_2pc || (rw_ha_count <= 1))
  {
    error= ha_commit_one_phase(thd, all);
    DBUG_EXECUTE_IF("crash_commit_after", DBUG_SUICIDE(););
    goto end;
  }

  need_prepare_ordered= FALSE;
  need_commit_ordered= FALSE;
  xid= thd->transaction.xid_state.xid.get_my_xid();

  for (Ha_trx_info *hi= ha_info; hi; hi= hi->next())
  {
    int err;
    handlerton *ht= hi->ht();
    /*
      Do not call two-phase commit if this particular
      transaction is read-only. This allows for simpler
      implementation in engines that are always read-only.
    */
    if (! hi->is_trx_read_write())
      continue;
    /*
      Sic: we know that prepare() is not NULL since otherwise
      trans->no_2pc would have been set.
    */
    err= ht->prepare(ht, thd, all);
    status_var_increment(thd->status_var.ha_prepare_count);
    if (err)
      my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);

    if (err)
      goto err;

    need_prepare_ordered|= (ht->prepare_ordered != NULL);
    need_commit_ordered|= (ht->commit_ordered != NULL);
  }
  DBUG_EXECUTE_IF("crash_commit_after_prepare", DBUG_SUICIDE(););

  if (!is_real_trans)
  {
    error= commit_one_phase_2(thd, all, trans, is_real_trans);
    DBUG_EXECUTE_IF("crash_commit_after", DBUG_SUICIDE(););
    goto end;
  }

  cookie= tc_log->log_and_order(thd, xid, all, need_prepare_ordered,
                                need_commit_ordered);
  if (!cookie)
    goto err;

  DBUG_EXECUTE_IF("crash_commit_after_log", DBUG_SUICIDE(););

  error= commit_one_phase_2(thd, all, trans, is_real_trans) ? 2 : 0;
  DBUG_EXECUTE_IF("crash_commit_after", DBUG_SUICIDE(););

  DBUG_EXECUTE_IF("crash_commit_before_unlog", DBUG_SUICIDE(););
  if (tc_log->unlog(cookie, xid))
  {
    error= 2;                                /* Error during commit */
    goto end;
  }

  DBUG_EXECUTE_IF("crash_commit_after", DBUG_SUICIDE(););
  goto end;

  /* Come here if error and we need to rollback. */
err:
  error= 1;                                  /* Transaction was rolled back */
  ha_rollback_trans(thd, all);

end:
  if (rw_trans)
    start_waiting_global_read_lock(thd);
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}

/**
  @note
  This function does not care about global read lock. A caller should.
*/
int ha_commit_one_phase(THD *thd, bool all)
{
  THD_TRANS *trans=all ? &thd->transaction.all : &thd->transaction.stmt;
  /*
    "real" is a nick name for a transaction for which a commit will
    make persistent changes. E.g. a 'stmt' transaction inside a 'all'
    transation is not 'real': even though it's possible to commit it,
    the changes are not durable as they might be rolled back if the
    enclosing 'all' transaction is rolled back.
  */
  bool is_real_trans=all || thd->transaction.all.ha_list == 0;
  DBUG_ENTER("ha_commit_one_phase");
  DBUG_RETURN(commit_one_phase_2(thd, all, trans, is_real_trans));
}

static int
commit_one_phase_2(THD *thd, bool all, THD_TRANS *trans, bool is_real_trans)
{
  int error= 0;
  Ha_trx_info *ha_info= trans->ha_list, *ha_info_next;
  DBUG_ENTER("commit_one_phase_2");
#ifdef USING_TRANSACTIONS
  if (ha_info)
  {
    for (; ha_info; ha_info= ha_info_next)
    {
      int err;
      handlerton *ht= ha_info->ht();
      if ((err= ht->commit(ht, thd, all)))
      {
        my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
        error=1;
      }
      /* Should this be done only if is_real_trans is set ? */
      status_var_increment(thd->status_var.ha_commit_count);
      ha_info_next= ha_info->next();
      ha_info->reset(); /* keep it conveniently zero-filled */
    }
    trans->ha_list= 0;
    trans->no_2pc=0;
    if (all)
    {
#ifdef HAVE_QUERY_CACHE
      if (thd->transaction.changed_tables)
        query_cache.invalidate(thd, thd->transaction.changed_tables);
#endif
      thd->variables.tx_isolation=thd->session_tx_isolation;
    }
  }
  /* Free resources and perform other cleanup even for 'empty' transactions. */
  if (is_real_trans)
    thd->transaction.cleanup();
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}


int ha_rollback_trans(THD *thd, bool all)
{
  int error=0;
  THD_TRANS *trans=all ? &thd->transaction.all : &thd->transaction.stmt;
  Ha_trx_info *ha_info= trans->ha_list, *ha_info_next;
  /*
    "real" is a nick name for a transaction for which a commit will
    make persistent changes. E.g. a 'stmt' transaction inside a 'all'
    transation is not 'real': even though it's possible to commit it,
    the changes are not durable as they might be rolled back if the
    enclosing 'all' transaction is rolled back.
  */
  bool is_real_trans=all || thd->transaction.all.ha_list == 0;
  DBUG_ENTER("ha_rollback_trans");

  /*
    We must not rollback the normal transaction if a statement
    transaction is pending.
  */
  DBUG_ASSERT(thd->transaction.stmt.ha_list == NULL ||
              trans == &thd->transaction.stmt);

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
  if (ha_info)
  {
    /* Close all cursors that can not survive ROLLBACK */
    if (is_real_trans)                          /* not a statement commit */
      thd->stmt_map.close_transient_cursors();

    for (; ha_info; ha_info= ha_info_next)
    {
      int err;
      handlerton *ht= ha_info->ht();
      if ((err= ht->rollback(ht, thd, all)))
      { // cannot happen
        my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
        error=1;
      }
      status_var_increment(thd->status_var.ha_rollback_count);
      ha_info_next= ha_info->next();
      ha_info->reset(); /* keep it conveniently zero-filled */
    }
    trans->ha_list= 0;
    trans->no_2pc=0;
    if (is_real_trans && thd->transaction_rollback_request &&
        thd->transaction.xid_state.xa_state != XA_NOTR)
      thd->transaction.xid_state.rm_error= thd->main_da.sql_errno();
    if (all)
      thd->variables.tx_isolation=thd->session_tx_isolation;
  }
  /* Always cleanup. Even if there nht==0. There may be savepoints. */
  if (is_real_trans)
    thd->transaction.cleanup();
#endif /* USING_TRANSACTIONS */
  if (all)
    thd->transaction_rollback_request= FALSE;

  /*
    If a non-transactional table was updated, warn; don't warn if this is a
    slave thread (because when a slave thread executes a ROLLBACK, it has
    been read from the binary log, so it's 100% sure and normal to produce
    error ER_WARNING_NOT_COMPLETE_ROLLBACK. If we sent the warning to the
    slave SQL thread, it would not stop the thread but just be printed in
    the error log; but we don't want users to wonder why they have this
    message in the error log, so we don't send it.

    We don't have to test for thd->killed == KILL_SYSTEM_THREAD as
    it doesn't matter if a warning is pushed to a system thread or not:
    No one will see it...
  */
  if (is_real_trans && thd->transaction.all.modified_non_trans_table &&
      !thd->slave_thread && thd->killed < KILL_CONNECTION)
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
  DBUG_RETURN(error);
}

/**
  This is used to commit or rollback a single statement depending on
  the value of error.

  @note
    Note that if the autocommit is on, then the following call inside
    InnoDB will commit or rollback the whole transaction (= the statement). The
    autocommit mechanism built into InnoDB is based on counting locks, but if
    the user has used LOCK TABLES then that mechanism does not know to do the
    commit.
*/
int ha_autocommit_or_rollback(THD *thd, int error)
{
  DBUG_ENTER("ha_autocommit_or_rollback");
#ifdef USING_TRANSACTIONS
  if (thd->transaction.stmt.ha_list)
  {
    if (!error)
    {
      if (ha_commit_trans(thd, 0))
	error=1;
    }
    else 
    {
      (void) ha_rollback_trans(thd, 0);
      if (thd->transaction_rollback_request && !thd->in_sub_stmt)
        (void) ha_rollback(thd);
    }

    thd->variables.tx_isolation=thd->session_tx_isolation;
  }
#endif
  DBUG_RETURN(error);
}


struct xahton_st {
  XID *xid;
  int result;
};

static my_bool xacommit_handlerton(THD *unused1, plugin_ref plugin,
                                   void *arg)
{
  handlerton *hton= plugin_data(plugin, handlerton *);
  if (hton->state == SHOW_OPTION_YES && hton->recover)
  {
    hton->commit_by_xid(hton, ((struct xahton_st *)arg)->xid);
    ((struct xahton_st *)arg)->result= 0;
  }
  return FALSE;
}

static my_bool xarollback_handlerton(THD *unused1, plugin_ref plugin,
                                     void *arg)
{
  handlerton *hton= plugin_data(plugin, handlerton *);
  if (hton->state == SHOW_OPTION_YES && hton->recover)
  {
    hton->rollback_by_xid(hton, ((struct xahton_st *)arg)->xid);
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
/**
  @note
    This does not need to be multi-byte safe or anything
*/
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

/**
  recover() step of xa.

  @note
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

static my_bool xarecover_handlerton(THD *unused, plugin_ref plugin,
                                    void *arg)
{
  handlerton *hton= plugin_data(plugin, handlerton *);
  struct xarecover_st *info= (struct xarecover_st *) arg;
  int got;

  if (hton->state == SHOW_OPTION_YES && hton->recover)
  {
    while ((got= hton->recover(hton, info->list, info->len)) > 0 )
    {
      sql_print_information("Found %d prepared transaction(s) in %s",
                            got, hton_name(hton)->str);
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
            hash_search(info->commit_list, (uchar *)&x, sizeof(x)) != 0 :
            tc_heuristic_recover == TC_HEURISTIC_RECOVER_COMMIT)
        {
#ifndef DBUG_OFF
          char buf[XIDDATASIZE*4+6]; // see xid_to_str
          sql_print_information("commit xid %s", xid_to_str(buf, info->list+i));
#endif
          hton->commit_by_xid(hton, info->list+i);
        }
        else
        {
#ifndef DBUG_OFF
          char buf[XIDDATASIZE*4+6]; // see xid_to_str
          sql_print_information("rollback xid %s",
                                xid_to_str(buf, info->list+i));
#endif
          hton->rollback_by_xid(hton, info->list+i);
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

  for (info.len= MAX_XID_LIST_SIZE ; 
       info.list==0 && info.len > MIN_XID_LIST_SIZE; info.len/=2)
  {
    info.list=(XID *)my_malloc(info.len*sizeof(XID), MYF(0));
  }
  if (!info.list)
  {
    sql_print_error(ER(ER_OUTOFMEMORY),
                    static_cast<int>(info.len*sizeof(XID)));
    DBUG_RETURN(1);
  }

  plugin_foreach(NULL, xarecover_handlerton, 
                 MYSQL_STORAGE_ENGINE_PLUGIN, &info);

  my_free((uchar*)info.list, MYF(0));
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

/**
  return the list of XID's to a client, the same way SHOW commands do.

  @note
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

  field_list.push_back(new Item_int("formatID", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_int("gtrid_length", 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_int("bqual_length", 0, MY_INT32_NUM_DECIMAL_DIGITS));
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
  my_eof(thd);
  DBUG_RETURN(0);
}

/**
  @details
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

  @param thd           the thread handle of the current connection

  @return
    always 0
*/

int ha_release_temporary_latches(THD *thd)
{
  Ha_trx_info *info;

  /*
    Note that below we assume that only transactional storage engines
    may need release_temporary_latches(). If this will ever become false,
    we could iterate on thd->open_tables instead (and remove duplicates
    as if (!seen[hton->slot]) { seen[hton->slot]=1; ... }).
  */
  for (info= thd->transaction.stmt.ha_list; info; info= info->next())
  {
    handlerton *hton= info->ht();
    if (hton && hton->release_temporary_latches)
        hton->release_temporary_latches(hton, thd);
  }
  return 0;
}

int ha_rollback_to_savepoint(THD *thd, SAVEPOINT *sv)
{
  int error=0;
  THD_TRANS *trans= (thd->in_sub_stmt ? &thd->transaction.stmt :
                                        &thd->transaction.all);
  Ha_trx_info *ha_info, *ha_info_next;

  DBUG_ENTER("ha_rollback_to_savepoint");

  trans->no_2pc=0;
  /*
    rolling back to savepoint in all storage engines that were part of the
    transaction when the savepoint was set
  */
  for (ha_info= sv->ha_list; ha_info; ha_info= ha_info->next())
  {
    int err;
    handlerton *ht= ha_info->ht();
    DBUG_ASSERT(ht);
    DBUG_ASSERT(ht->savepoint_set != 0);
    if ((err= ht->savepoint_rollback(ht, thd,
                                     (uchar *)(sv+1)+ht->savepoint_offset)))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error=1;
    }
    status_var_increment(thd->status_var.ha_savepoint_rollback_count);
    trans->no_2pc|= ht->prepare == 0;
  }
  /*
    rolling back the transaction in all storage engines that were not part of
    the transaction when the savepoint was set
  */
  for (ha_info= trans->ha_list; ha_info != sv->ha_list;
       ha_info= ha_info_next)
  {
    int err;
    handlerton *ht= ha_info->ht();
    if ((err= ht->rollback(ht, thd, !thd->in_sub_stmt)))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error=1;
    }
    status_var_increment(thd->status_var.ha_rollback_count);
    ha_info_next= ha_info->next();
    ha_info->reset(); /* keep it conveniently zero-filled */
  }
  trans->ha_list= sv->ha_list;
  DBUG_RETURN(error);
}

/**
  @note
  according to the sql standard (ISO/IEC 9075-2:2003)
  section "4.33.4 SQL-statements and transaction states",
  SAVEPOINT is *not* transaction-initiating SQL-statement
*/
int ha_savepoint(THD *thd, SAVEPOINT *sv)
{
  int error=0;
  THD_TRANS *trans= (thd->in_sub_stmt ? &thd->transaction.stmt :
                                        &thd->transaction.all);
  Ha_trx_info *ha_info= trans->ha_list;
  DBUG_ENTER("ha_savepoint");
#ifdef USING_TRANSACTIONS
  for (; ha_info; ha_info= ha_info->next())
  {
    int err;
    handlerton *ht= ha_info->ht();
    DBUG_ASSERT(ht);
    if (! ht->savepoint_set)
    {
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "SAVEPOINT");
      error=1;
      break;
    }
    if ((err= ht->savepoint_set(ht, thd, (uchar *)(sv+1)+ht->savepoint_offset)))
    { // cannot happen
      my_error(ER_GET_ERRNO, MYF(0), err);
      error=1;
    }
    status_var_increment(thd->status_var.ha_savepoint_count);
  }
  /*
    Remember the list of registered storage engines. All new
    engines are prepended to the beginning of the list.
  */
  sv->ha_list= trans->ha_list;
#endif /* USING_TRANSACTIONS */
  DBUG_RETURN(error);
}

int ha_release_savepoint(THD *thd, SAVEPOINT *sv)
{
  int error=0;
  Ha_trx_info *ha_info= sv->ha_list;
  DBUG_ENTER("ha_release_savepoint");

  for (; ha_info; ha_info= ha_info->next())
  {
    int err;
    handlerton *ht= ha_info->ht();
    /* Savepoint life time is enclosed into transaction life time. */
    DBUG_ASSERT(ht);
    if (!ht->savepoint_release)
      continue;
    if ((err= ht->savepoint_release(ht, thd,
                                    (uchar *)(sv+1) + ht->savepoint_offset)))
    { // cannot happen
      my_error(ER_GET_ERRNO, MYF(0), err);
      error=1;
    }
  }
  DBUG_RETURN(error);
}


static my_bool snapshot_handlerton(THD *thd, plugin_ref plugin,
                                   void *arg)
{
  handlerton *hton= plugin_data(plugin, handlerton *);
  if (hton->state == SHOW_OPTION_YES &&
      hton->start_consistent_snapshot)
  {
    hton->start_consistent_snapshot(hton, thd);
    *((bool *)arg)= false;
  }
  return FALSE;
}

int ha_start_consistent_snapshot(THD *thd)
{
  bool warn= true;

  /*
    Holding the LOCK_commit_ordered mutex ensures that we get the same
    snapshot for all engines (including the binary log).  This allows us
    among other things to do backups with
    START TRANSACTION WITH CONSISTENT SNAPSHOT and
    have a consistent binlog position.
  */
  pthread_mutex_lock(&LOCK_commit_ordered);
  plugin_foreach(thd, snapshot_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, &warn);
  pthread_mutex_unlock(&LOCK_commit_ordered);

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


static my_bool flush_handlerton(THD *thd, plugin_ref plugin,
                                void *arg)
{
  handlerton *hton= plugin_data(plugin, handlerton *);
  if (hton->state == SHOW_OPTION_YES && hton->flush_logs && 
      hton->flush_logs(hton))
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
        (db_type->flush_logs && db_type->flush_logs(db_type)))
      return TRUE;
  }
  return FALSE;
}


/**
  @brief make canonical filename

  @param[in]  file     table handler
  @param[in]  path     original path
  @param[out] tmp_path buffer for canonized path

  @details Lower case db name and table name path parts for
           non file based tables when lower_case_table_names
           is 2 (store as is, compare in lower case).
           Filesystem path prefix (mysql_data_home or tmpdir)
           is left intact.

  @note tmp_path may be left intact if no conversion was
        performed.

  @retval canonized path

  @todo This may be done more efficiently when table path
        gets built. Convert this function to something like
        ASSERT_CANONICAL_FILENAME.
*/
const char *get_canonical_filename(handler *file, const char *path,
                                   char *tmp_path)
{
  uint i;
  if (lower_case_table_names != 2 || (file->ha_table_flags() & HA_FILE_BASED))
    return path;

  for (i= 0; i <= mysql_tmpdir_list.max; i++)
  {
    if (is_prefix(path, mysql_tmpdir_list.list[i]))
      return path;
  }

  /* Ensure that table handler get path in lower case */
  if (tmp_path != path)
    strmov(tmp_path, path);

  /*
    we only should turn into lowercase database/table part
    so start the process after homedirectory
  */
  my_casedn_str(files_charset_info, tmp_path + mysql_data_home_len);
  return tmp_path;
}


/**
  An interceptor to hijack the text of the error message without
  setting an error in the thread. We need the text to present it
  in the form of a warning to the user.
*/

struct Ha_delete_table_error_handler: public Internal_error_handler
{
public:
  virtual bool handle_error(uint sql_errno,
                            const char *message,
                            MYSQL_ERROR::enum_warning_level level,
                            THD *thd);
  char buff[MYSQL_ERRMSG_SIZE];
};


bool
Ha_delete_table_error_handler::
handle_error(uint sql_errno,
             const char *message,
             MYSQL_ERROR::enum_warning_level level,
             THD *thd)
{
  /* Grab the error message */
  strmake(buff, message, sizeof(buff)-1);
  return TRUE;
}


/** @brief
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
      ! (file=get_new_handler((TABLE_SHARE*)0, thd->mem_root, table_type)))
    DBUG_RETURN(ENOENT);

  path= get_canonical_filename(file, path, tmp_path);
  if ((error= file->ha_delete_table(path)) && generate_warning)
  {
    /*
      Because file->print_error() use my_error() to generate the error message
      we use an internal error handler to intercept it and store the text
      in a temporary buffer. Later the message will be presented to user
      as a warning.
    */
    Ha_delete_table_error_handler ha_delete_table_error_handler;

    /* Fill up strucutures that print_error may need */
    dummy_share.path.str= (char*) path;
    dummy_share.path.length= strlen(path);
    dummy_share.db.str= (char*) db;
    dummy_share.db.length= strlen(db);
    dummy_share.table_name.str= (char*) alias;
    dummy_share.table_name.length= strlen(alias);
    dummy_table.alias.set(alias, dummy_share.table_name.length,
                          table_alias_charset);

    file->change_table_ptr(&dummy_table, &dummy_share);

    thd->push_internal_handler(&ha_delete_table_error_handler);
    file->print_error(error, 0);

    thd->pop_internal_handler();

    /*
      XXX: should we convert *all* errors to warnings here?
      What if the error is fatal?
    */
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR, error,
                ha_delete_table_error_handler.buff);
  }
  delete file;
  DBUG_RETURN(error);
}

/****************************************************************************
** General handler functions
****************************************************************************/
handler *handler::clone(const char *name, MEM_ROOT *mem_root)
{
  handler *new_handler= get_new_handler(table->s, mem_root, ht);
  if (! new_handler)
    return NULL;

  /*
    Allocate handler->ref here because otherwise ha_open will allocate it
    on this->table->mem_root and we will not be able to reclaim that memory 
    when the clone handler object is destroyed.
  */

  if (!(new_handler->ref= (uchar*) alloc_root(mem_root,
                                              ALIGN_SIZE(ref_length)*2)))
    return NULL;

  /*
    TODO: Implement a more efficient way to have more than one index open for
    the same table instance. The ha_open call is not cachable for clone.

    This is not critical as the engines already have the table open
    and should be able to use the original instance of the table.
  */
  if (new_handler->ha_open(table, name, table->db_stat,
                           HA_OPEN_IGNORE_IF_LOCKED))
    return NULL;

  new_handler->cloned= 1;                      // Marker for debugging
  return new_handler;
}


double handler::keyread_time(uint index, uint ranges, ha_rows rows)
{
  /*
    It is assumed that we will read trough the whole key range and that all
    key blocks are half full (normally things are much better). It is also
    assumed that each time we read the next key from the index, the handler
    performs a random seek, thus the cost is proportional to the number of
    blocks read. This model does not take into account clustered indexes -
    engines that support that (e.g. InnoDB) may want to overwrite this method.
  */
  double keys_per_block= (stats.block_size/2.0/
                          (table->key_info[index].key_length +
                           ref_length) + 1);
  return (rows + keys_per_block - 1)/ keys_per_block;
}

void **handler::ha_data(THD *thd) const
{
  return thd_ha_data(thd, ht);
}

THD *handler::ha_thd(void) const
{
  DBUG_ASSERT(!table || !table->in_use || table->in_use == current_thd);
  return (table && table->in_use) ? table->in_use : current_thd;
}


/** @brief
  Open database-handler.

  IMPLEMENTATION
    Try O_RDONLY if cannot open as O_RDWR
    Don't wait for locks if not HA_OPEN_WAIT_IF_LOCKED is set
*/
int handler::ha_open(TABLE *table_arg, const char *name, int mode,
                     uint test_if_locked)
{
  int error;
  DBUG_ENTER("handler::ha_open");
  DBUG_PRINT("enter",
             ("name: %s  db_type: %d  db_stat: %d  mode: %d  lock_test: %d",
              name, ht->db_type, table_arg->db_stat, mode,
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

    /* ref is already allocated for us if we're called from handler::clone() */
    if (!ref && !(ref= (uchar*) alloc_root(&table->mem_root, 
                                          ALIGN_SIZE(ref_length)*2)))
    {
      close();
      error=HA_ERR_OUT_OF_MEM;
    }
    else
      dup_ref=ref+ALIGN_SIZE(ref_length);
    cached_table_flags= table_flags();
  }
  reset_statistics();
  internal_tmp_table= test(test_if_locked & HA_OPEN_INTERNAL_TABLE);
  DBUG_RETURN(error);
}

int handler::ha_close()
{
  DBUG_ENTER("ha_close");
  /*
    Increment global statistics for temporary tables.
    In_use is 0 for tables that was closed from the table cache.
  */
  if (table->in_use)
    status_var_add(table->in_use->status_var.rows_tmp_read, rows_tmp_read);
  DBUG_RETURN(close());
}

/* Initialize handler for random reading, with error handling */

int handler::ha_rnd_init_with_error(bool scan)
{
  int error;
  if (!(error= ha_rnd_init(scan)))
    return 0;
  table->file->print_error(error, MYF(0));
  return error;
}


/**
  Read first row (only) from a table.

  This is never called for InnoDB tables, as these table types
  has the HA_STATS_RECORDS_IS_EXACT set.
*/
int handler::read_first_row(uchar * buf, uint primary_key)
{
  register int error;
  DBUG_ENTER("handler::read_first_row");

  /*
    If there is very few deleted rows in the table, find the first row by
    scanning the table.
    TODO remove the test for HA_READ_ORDER
  */
  if (stats.deleted < 10 || primary_key >= MAX_KEY ||
      !(index_flags(primary_key, 0, 0) & HA_READ_ORDER))
  {
    if ((!(error= ha_rnd_init(1))))
    {
      while ((error= ha_rnd_next(buf)) == HA_ERR_RECORD_DELETED) ;
      (void) ha_rnd_end();
    }
  }
  else
  {
    /* Find the first row through the primary key */
    if (!(error = ha_index_init(primary_key, 0)))
      error= ha_index_first(buf);
    (void) ha_index_end();
  }
  DBUG_RETURN(error);
}

/**
  Generate the next auto-increment number based on increment and offset.
  computes the lowest number
  - strictly greater than "nr"
  - of the form: auto_increment_offset + N * auto_increment_increment
  If overflow happened then return MAX_ULONGLONG value as an
  indication of overflow.
  In most cases increment= offset= 1, in which case we get:
  @verbatim 1,2,3,4,5,... @endverbatim
    If increment=10 and offset=5 and previous number is 1, we get:
  @verbatim 1,5,15,25,35,... @endverbatim
*/
inline ulonglong
compute_next_insert_id(ulonglong nr,struct system_variables *variables)
{
  const ulonglong save_nr= nr;

  if (variables->auto_increment_increment == 1)
    nr= nr + 1; // optimization of the formula below
  else
  {
    nr= (((nr+ variables->auto_increment_increment -
           variables->auto_increment_offset)) /
         (ulonglong) variables->auto_increment_increment);
    nr= (nr* (ulonglong) variables->auto_increment_increment +
         variables->auto_increment_offset);
  }

  if (unlikely(nr <= save_nr))
    return ULONGLONG_MAX;

  return nr;
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


/** @brief
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
                       (ulong) nr, variables->auto_increment_offset));
    return nr;
  }
  if (variables->auto_increment_increment == 1)
    return nr; // optimization of the formula below
  nr= (((nr - variables->auto_increment_offset)) /
       (ulonglong) variables->auto_increment_increment);
  return (nr * (ulonglong) variables->auto_increment_increment +
          variables->auto_increment_offset);
}


/**
  Update the auto_increment field if necessary.

  Updates columns with type NEXT_NUMBER if:

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
    auto_inc_interval_for_cur_row. The number of reserved intervals is
    remembered in auto_inc_intervals_count. It differs from the number of
    elements in thd->auto_inc_intervals_in_cur_stmt_for_binlog() because the
    latter list is cumulative over all statements forming one binlog event
    (when stored functions and triggers are used), and collapses two
    contiguous intervals in one (see its append() method).

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

  @todo
    Replace all references to "next number" or NEXT_NUMBER to
    "auto_increment", everywhere (see below: there is
    table->auto_increment_field_not_null, and there also exists
    table->next_number_field, it's not consistent).

  @retval
    0	ok
  @retval
    HA_ERR_AUTOINC_READ_FAILED  get_auto_increment() was called and
    returned ~(ulonglong) 0
  @retval
    HA_ERR_AUTOINC_ERANGE storing value in field caused strict mode
    failure.
*/

#define AUTO_INC_DEFAULT_NB_ROWS 1 // Some prefer 1024 here
#define AUTO_INC_DEFAULT_NB_MAX_BITS 16
#define AUTO_INC_DEFAULT_NB_MAX ((1 << AUTO_INC_DEFAULT_NB_MAX_BITS) - 1)

int handler::update_auto_increment()
{
  ulonglong nr, nb_reserved_values;
  bool append= FALSE;
  THD *thd= table->in_use;
  struct system_variables *variables= &thd->variables;
  DBUG_ENTER("handler::update_auto_increment");

  /*
    next_insert_id is a "cursor" into the reserved interval, it may go greater
    than the interval, but not smaller.
  */
  DBUG_ASSERT(next_insert_id >= auto_inc_interval_for_cur_row.minimum());

  if ((nr= table->next_number_field->val_int()) != 0 ||
      (table->auto_increment_field_not_null &&
       thd->variables.sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO))
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
        Note that in prelocked mode no estimation is given.
      */

      if ((auto_inc_intervals_count == 0) && (estimation_rows_to_insert > 0))
        nb_desired_values= estimation_rows_to_insert;
      else if ((auto_inc_intervals_count == 0) &&
               (thd->lex->many_values.elements > 0))
      {
        /*
          For multi-row inserts, if the bulk inserts cannot be started, the
          handler::estimation_rows_to_insert will not be set. But we still
          want to reserve the autoinc values.
        */
        nb_desired_values= thd->lex->many_values.elements;
      }
      else /* go with the increasing defaults */
      {
        /* avoid overflow in formula, with this if() */
        if (auto_inc_intervals_count <= AUTO_INC_DEFAULT_NB_MAX_BITS)
        {
          nb_desired_values= AUTO_INC_DEFAULT_NB_ROWS *
            (1 << auto_inc_intervals_count);
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
      if (nr == ULONGLONG_MAX)
        DBUG_RETURN(HA_ERR_AUTOINC_READ_FAILED);  // Mark failure

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

    if (table->s->next_number_keypart == 0)
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

  if (unlikely(nr == ULONGLONG_MAX))
      DBUG_RETURN(HA_ERR_AUTOINC_ERANGE); 

  DBUG_PRINT("info",("auto_increment: %lu", (ulong) nr));

  if (unlikely(table->next_number_field->store((longlong) nr, TRUE)))
  {
    /*
      first test if the query was aborted due to strict mode constraints
    */
    if (killed_mask_hard(thd->killed) == KILL_BAD_DATA)
      DBUG_RETURN(HA_ERR_AUTOINC_ERANGE);

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
    auto_inc_intervals_count++;
    /* Row-based replication does not need to store intervals in binlog */
    if (mysql_bin_log.is_open() && !thd->current_stmt_binlog_row_based)
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

  DBUG_RETURN(0);
}


/** @brief
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
  if (table)
    DBUG_PRINT("info", ("read_set: 0x%lx  write_set: 0x%lx",
                        (long) table->read_set, (long) table->write_set));
  DBUG_VOID_RETURN;
}


/** @brief
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
  ha_index_init(table->s->next_number_index, 1);
  if (table->s->next_number_keypart == 0)
  {						// Autoincrement at key-start
    error=ha_index_last(table->record[1]);
    /*
      MySQL implicitely assumes such method does locking (as MySQL decides to
      use nr+increment without checking again with the handler, in
      handler::update_auto_increment()), so reserves to infinite.
    */
    *nb_reserved_values= ULONGLONG_MAX;
  }
  else
  {
    uchar key[MAX_KEY_LENGTH];
    key_copy(key, table->record[0],
             table->key_info + table->s->next_number_index,
             table->s->next_number_key_offset);
    error= ha_index_read_map(table->record[1], key,
                             make_prev_keypart_map(table->s->
                                                   next_number_keypart),
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
  ha_index_end();
  (void) extra(HA_EXTRA_NO_KEYREAD);
  *first_value= nr;
}


void handler::ha_release_auto_increment()
{
  DBUG_ENTER("ha_release_auto_increment");
  release_auto_increment();
  insert_id_for_cur_row= 0;
  auto_inc_interval_for_cur_row.replace(0, 0, 0);
  auto_inc_intervals_count= 0;
  if (next_insert_id > 0)
  {
    next_insert_id= 0;
    /*
      this statement used forced auto_increment values if there were some,
      wipe them away for other statements.
    */
    table->in_use->auto_inc_intervals_forced.empty();
  }
  DBUG_VOID_RETURN;
}


void handler::print_keydup_error(uint key_nr, const char *msg)
{
  /* Write the duplicated key in the error message */
  char key[MAX_KEY_LENGTH];
  String str(key,sizeof(key),system_charset_info);

  if (key_nr == MAX_KEY)
  {
    /* Key is unknown */
    str.copy("", 0, system_charset_info);
    my_printf_error(ER_DUP_ENTRY, msg, MYF(0), str.c_ptr(), "*UNKNOWN*");
  }
  else
  {
    /* Table is opened and defined at this point */
    key_unpack(&str,table,(uint) key_nr);
    uint max_length=MYSQL_ERRMSG_SIZE-(uint) strlen(msg);
    if (str.length() >= max_length)
    {
      str.length(max_length-4);
      str.append(STRING_WITH_LEN("..."));
    }
    my_printf_error(ER_DUP_ENTRY, msg,
		    MYF(0), str.c_ptr_safe(), table->key_info[key_nr].name);
  }
}


/**
  Print error that we got from handler function.

  @note
    In case of delete table it's only safe to use the following parts of
    the 'table' structure:
    - table->s->path
    - table->alias
*/

#define SET_FATAL_ERROR fatal_error=1

void handler::print_error(int error, myf errflag)
{
  bool fatal_error= 0;
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
  case ENOSPC:
  case HA_ERR_DISK_FULL:
    textno= ER_DISK_FULL;
    SET_FATAL_ERROR;                            // Ensure error is logged
    break;
  case HA_ERR_KEY_NOT_FOUND:
  case HA_ERR_NO_ACTIVE_RECORD:
  case HA_ERR_END_OF_FILE:
    /*
      This errors is not not normally fatal (for example for reads). However
      if you get it during an update or delete, then its fatal.
      As the user is calling print_error() (which is not done on read), we
      assume something when wrong with the update or delete.
    */
    SET_FATAL_ERROR;
    textno=ER_KEY_NOT_FOUND;
    break;
  case HA_ERR_ABORTED_BY_USER:
  {
    DBUG_ASSERT(table->in_use->killed);
    table->in_use->send_kill_message();
    DBUG_VOID_RETURN;
  }
  case HA_ERR_WRONG_MRG_TABLE_DEF:
    textno=ER_WRONG_MRG_TABLE;
    break;
  case HA_ERR_FOUND_DUPP_KEY:
  {
    if (table)
    {
      uint key_nr=get_dup_key(error);
      if ((int) key_nr >= 0)
      {
        print_keydup_error(key_nr, ER(ER_DUP_ENTRY_WITH_KEY_NAME));
        DBUG_VOID_RETURN;
      }
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

      /*
        Use primary_key instead of key_nr because key_nr is a key
        number in the child FK table, not in our 'table'. See
        Bug#12661768 UPDATE IGNORE CRASHES SERVER IF TABLE IS INNODB
        AND IT IS PARENT FOR OTHER ONE This bug gets a better fix in
        MySQL 5.6, but it is too risky to get that in 5.1 and 5.5
        (extending the handler interface and adding new error message
        codes)
      */
      if (table->s->primary_key < MAX_KEY)
        key_unpack(&str,table,table->s->primary_key);
      else
      {
        LEX_CUSTRING tmp= {USTRING_WITH_LEN("Unknown key value")};
        str.set((const char*) tmp.str, tmp.length, system_charset_info);
      }
      max_length= (MYSQL_ERRMSG_SIZE-
                   (uint) strlen(ER(ER_FOREIGN_DUPLICATE_KEY)));
      if (str.length() >= max_length)
      {
        str.length(max_length-4);
        str.append(STRING_WITH_LEN("..."));
      }
      my_error(ER_FOREIGN_DUPLICATE_KEY, errflag, table_share->table_name.str,
               str.c_ptr_safe(), key_nr+1);
      DBUG_VOID_RETURN;
    }
    textno= ER_DUP_KEY;
    break;
  }
  case HA_ERR_NULL_IN_SPATIAL:
    my_error(ER_CANT_CREATE_GEOMETRY_OBJECT, errflag);
    DBUG_VOID_RETURN;
  case HA_ERR_FOUND_DUPP_UNIQUE:
    textno=ER_DUP_UNIQUE;
    break;
  case HA_ERR_RECORD_CHANGED:
    /*
      This is not fatal error when using HANDLER interface
      SET_FATAL_ERROR;
    */
    textno=ER_CHECKREAD;
    break;
  case HA_ERR_CRASHED:
    SET_FATAL_ERROR;
    textno=ER_NOT_KEYFILE;
    break;
  case HA_ERR_WRONG_IN_RECORD:
    SET_FATAL_ERROR;
    textno= ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_CRASHED_ON_USAGE:
    SET_FATAL_ERROR;
    textno=ER_CRASHED_ON_USAGE;
    break;
  case HA_ERR_NOT_A_TABLE:
    textno= error;
    break;
  case HA_ERR_CRASHED_ON_REPAIR:
    SET_FATAL_ERROR;
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
  {
    textno=ER_RECORD_FILE_FULL;
    /* Write the error message to error log */
    errflag|= ME_NOREFRESH;
    break;
  }
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
    my_error(ER_ROW_IS_REFERENCED_2, errflag, str.c_ptr_safe());
    DBUG_VOID_RETURN;
  }
  case HA_ERR_NO_REFERENCED_ROW:
  {
    String str;
    get_error_message(error, &str);
    my_error(ER_NO_REFERENCED_ROW_2, errflag, str.c_ptr_safe());
    DBUG_VOID_RETURN;
  }
  case HA_ERR_TABLE_DEF_CHANGED:
    textno=ER_TABLE_DEF_CHANGED;
    break;
  case HA_ERR_NO_SUCH_TABLE:
    my_error(ER_NO_SUCH_TABLE, errflag, table_share->db.str,
             table_share->table_name.str);
    DBUG_VOID_RETURN;
  case HA_ERR_RBR_LOGGING_FAILED:
    textno= ER_BINLOG_ROW_LOGGING_FAILED;
    break;
  case HA_ERR_DROP_INDEX_FK:
  {
    const char *ptr= "???";
    uint key_nr= get_dup_key(error);
    if ((int) key_nr >= 0)
      ptr= table->key_info[key_nr].name;
    my_error(ER_DROP_INDEX_FK, errflag, ptr);
    DBUG_VOID_RETURN;
  }
  case HA_ERR_TABLE_NEEDS_UPGRADE:
    textno=ER_TABLE_NEEDS_UPGRADE;
    break;
  case HA_ERR_TABLE_READONLY:
    textno= ER_OPEN_AS_READONLY;
    break;
  case HA_ERR_AUTOINC_READ_FAILED:
    textno= ER_AUTOINC_READ_FAILED;
    break;
  case HA_ERR_AUTOINC_ERANGE:
    textno= ER_WARN_DATA_OUT_OF_RANGE;
    break;
  case HA_ERR_TOO_MANY_CONCURRENT_TRXS:
    textno= ER_TOO_MANY_CONCURRENT_TRXS;
    break;
  case HA_ERR_TABLE_IN_FK_CHECK:
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
	  my_error(ER_GET_TEMPORARY_ERRMSG, errflag, error, str.c_ptr(),
                   engine);
	else
        {
          SET_FATAL_ERROR;
	  my_error(ER_GET_ERRMSG, errflag, error, str.c_ptr(), engine);
        }
      }
      else
	my_error(ER_GET_ERRNO,errflag,error);
      DBUG_VOID_RETURN;
    }
  }
  if (fatal_error)
  {
    /* Ensure this becomes a true error */
    errflag&= ~(ME_JUST_WARNING | ME_JUST_INFO);
    if ((debug_assert_if_crashed_table ||
                      global_system_variables.log_warnings > 1))
    {
      /*
        Log error to log before we crash or if extended warnings are requested
      */
      errflag|= ME_NOREFRESH;
    }
  }    
  my_error(textno, errflag, table_share->table_name.str, error);
  DBUG_ASSERT(!fatal_error || !debug_assert_if_crashed_table);
  DBUG_VOID_RETURN;
}


/**
  Return an error message specific to this handler.

  @param error  error code previously returned by handler
  @param buf    pointer to String where to add error message

  @return
    Returns true if this is a temporary error
*/
bool handler::get_error_message(int error, String* buf)
{
  return FALSE;
}


/**
  Check for incompatible collation changes.
   
  @retval
    HA_ADMIN_NEEDS_UPGRADE   Table may have data requiring upgrade.
  @retval
    0                        No upgrade required.
*/

int handler::check_collation_compatibility()
{
  ulong mysql_version= table->s->mysql_version;

  if (mysql_version < 50124)
  {
    KEY *key= table->key_info;
    KEY *key_end= key + table->s->keys;
    for (; key < key_end; key++)
    {
      KEY_PART_INFO *key_part= key->key_part;
      KEY_PART_INFO *key_part_end= key_part + key->key_parts;
      for (; key_part < key_part_end; key_part++)
      {
        if (!key_part->fieldnr)
          continue;
        Field *field= table->field[key_part->fieldnr - 1];
        uint cs_number= field->charset()->number;
        if ((mysql_version < 50048 &&
             (cs_number == 11 || /* ascii_general_ci - bug #29499, bug #27562 */
              cs_number == 41 || /* latin7_general_ci - bug #29461 */
              cs_number == 42 || /* latin7_general_cs - bug #29461 */
              cs_number == 20 || /* latin7_estonian_cs - bug #29461 */
              cs_number == 21 || /* latin2_hungarian_ci - bug #29461 */
              cs_number == 22 || /* koi8u_general_ci - bug #29461 */
              cs_number == 23 || /* cp1251_ukrainian_ci - bug #29461 */
              cs_number == 26)) || /* cp1250_general_ci - bug #29461 */
             (mysql_version < 50124 &&
             (cs_number == 33 || /* utf8_general_ci - bug #27877 */
              cs_number == 35))) /* ucs2_general_ci - bug #27877 */
          return HA_ADMIN_NEEDS_UPGRADE;
      }  
    }  
  }  
  return 0;
}


int handler::ha_check_for_upgrade(HA_CHECK_OPT *check_opt)
{
  int error;
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
        if (field->type() == MYSQL_TYPE_BLOB)
        {
          if (check_opt->sql_flags & TT_FOR_UPGRADE)
            check_opt->flags= T_MEDIUM;
          return HA_ADMIN_NEEDS_CHECK;
        }
      }
    }
  }
  if (table->s->frm_version != FRM_VER_TRUE_VARCHAR)
    return HA_ADMIN_NEEDS_ALTER;

  if ((error= check_collation_compatibility()))
    return error;
    
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
      if ((*field)->type() == MYSQL_TYPE_NEWDECIMAL)
      {
        return HA_ADMIN_NEEDS_ALTER;
      }
      if ((*field)->type() == MYSQL_TYPE_VAR_STRING)
      {
        return HA_ADMIN_NEEDS_ALTER;
      }
    }
  }
  return 0;
}


static bool update_frm_version(TABLE *table)
{
  char path[FN_REFLEN];
  File file;
  int result= 1;
  DBUG_ENTER("update_frm_version");

  /*
    No need to update frm version in case table was created or checked
    by server with the same version. This also ensures that we do not
    update frm version for temporary tables as this code doesn't support
    temporary tables.
  */
  if (table->s->mysql_version == MYSQL_VERSION_ID)
    DBUG_RETURN(0);

  strxmov(path, table->s->normalized_path.str, reg_ext, NullS);

  if ((file= my_open(path, O_RDWR|O_BINARY, MYF(MY_WME))) >= 0)
  {
    uchar version[4];
    char *key= table->s->table_cache_key.str;
    uint key_length= table->s->table_cache_key.length;
    TABLE *entry;
    HASH_SEARCH_STATE state;

    int4store(version, MYSQL_VERSION_ID);

    if ((result= my_pwrite(file,(uchar*) version,4,51L,MYF_RW)))
      goto err;

    for (entry=(TABLE*) hash_first(&open_cache,(uchar*) key,key_length, &state);
         entry;
         entry= (TABLE*) hash_next(&open_cache,(uchar*) key,key_length, &state))
      entry->s->mysql_version= MYSQL_VERSION_ID;
  }
err:
  if (file >= 0)
    VOID(my_close(file,MYF(MY_WME)));
  DBUG_RETURN(result);
}



/**
  @return
    key if error because of duplicated keys
*/
uint handler::get_dup_key(int error)
{
  DBUG_ENTER("handler::get_dup_key");
  table->file->errkey  = (uint) -1;
  if (error == HA_ERR_FOUND_DUPP_KEY || error == HA_ERR_FOREIGN_DUPLICATE_KEY ||
      error == HA_ERR_FOUND_DUPP_UNIQUE || error == HA_ERR_NULL_IN_SPATIAL ||
      error == HA_ERR_DROP_INDEX_FK)
    table->file->info(HA_STATUS_ERRKEY | HA_STATUS_NO_LOCK);
  DBUG_RETURN(table->file->errkey);
}


/**
  Delete all files with extension from bas_ext().

  @param name		Base name of table

  @note
    We assume that the handler may return more extensions than
    was actually used for the file.

  @retval
    0   If we successfully deleted at least one file from base_ext and
    didn't get any other errors than ENOENT
  @retval
    !0  Error
*/
int handler::delete_table(const char *name)
{
  int saved_error= 0;
  int error= 0;
  int enoent_or_zero= ENOENT;                   // Error if no file was deleted
  char buff[FN_REFLEN];

  for (const char **ext=bas_ext(); *ext ; ext++)
  {
    fn_format(buff, name, "", *ext, MY_UNPACK_FILENAME|MY_APPEND_EXT);
    if (my_delete_with_symlink(buff, MYF(0)))
    {
      if (my_errno != ENOENT)
      {
        /*
          If error on the first existing file, return the error.
          Otherwise delete as much as possible.
        */
        if (enoent_or_zero)
          return my_errno;
	saved_error= my_errno;
      }
    }
    else
      enoent_or_zero= 0;                        // No error for ENOENT
    error= enoent_or_zero;
  }
  return saved_error ? saved_error : error;
}


int handler::rename_table(const char * from, const char * to)
{
  int error= 0;
  const char **ext, **start_ext;
  start_ext= bas_ext();
  for (ext= start_ext; *ext ; ext++)
  {
    if (rename_file_ext(from, to, *ext))
    {
      if ((error=my_errno) != ENOENT)
	break;
      error= 0;
    }
  }
  if (error)
  {
    /* Try to revert the rename. Ignore errors. */
    for (; ext >= start_ext; ext--)
      rename_file_ext(to, from, *ext);
  }
  return error;
}


void handler::drop_table(const char *name)
{
  ha_close();
  delete_table(name);
}


/**
  Performs checks upon the table.

  @param thd                thread doing CHECK TABLE operation
  @param check_opt          options from the parser

  @retval
    HA_ADMIN_OK               Successful upgrade
  @retval
    HA_ADMIN_NEEDS_UPGRADE    Table has structures requiring upgrade
  @retval
    HA_ADMIN_NEEDS_ALTER      Table has structures requiring ALTER TABLE
  @retval
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
  return update_frm_version(table);
}

/**
  A helper function to mark a transaction read-write,
  if it is started.
*/

void
handler::mark_trx_read_write_part2()
{
  Ha_trx_info *ha_info= &ha_thd()->ha_data[ht->slot].ha_info[0];

  /* Don't call this function again for this statement */
  mark_trx_done= TRUE;

  /*
    When a storage engine method is called, the transaction must
    have been started, unless it's a DDL call, for which the
    storage engine starts the transaction internally, and commits
    it internally, without registering in the ha_list.
    Unfortunately here we can't know know for sure if the engine
    has registered the transaction or not, so we must check.
  */
  if (ha_info->is_started())
  {
    DBUG_ASSERT(has_transactions());
    /*
      table_share can be NULL in ha_delete_table(). See implementation
      of standalone function ha_delete_table() in sql_base.cc.
    */
    if (table_share == NULL || table_share->tmp_table == NO_TMP_TABLE)
      ha_info->set_trx_read_write();
  }
}


/**
  Repair table: public interface.

  @sa handler::repair()
*/

int handler::ha_repair(THD* thd, HA_CHECK_OPT* check_opt)
{
  int result;

  mark_trx_read_write();

  if ((result= repair(thd, check_opt)))
    return result;
  return update_frm_version(table);
}


/**
  Bulk update row: public interface.

  @sa handler::bulk_update_row()
*/

int
handler::ha_bulk_update_row(const uchar *old_data, uchar *new_data,
                            uint *dup_key_found)
{
  mark_trx_read_write();

  return bulk_update_row(old_data, new_data, dup_key_found);
}


/**
  Delete all rows: public interface.

  @sa handler::delete_all_rows()
*/

int
handler::ha_delete_all_rows()
{
  mark_trx_read_write();

  return delete_all_rows();
}


/**
  Reset auto increment: public interface.

  @sa handler::reset_auto_increment()
*/

int
handler::ha_reset_auto_increment(ulonglong value)
{
  mark_trx_read_write();

  return reset_auto_increment(value);
}


/**
  Backup table: public interface.

  @sa handler::backup()
*/

int
handler::ha_backup(THD* thd, HA_CHECK_OPT* check_opt)
{
  mark_trx_read_write();

  return backup(thd, check_opt);
}


/**
  Restore table: public interface.

  @sa handler::restore()
*/

int
handler::ha_restore(THD* thd, HA_CHECK_OPT* check_opt)
{
  mark_trx_read_write();

  return restore(thd, check_opt);
}


/**
  Optimize table: public interface.

  @sa handler::optimize()
*/

int
handler::ha_optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  mark_trx_read_write();

  return optimize(thd, check_opt);
}


/**
  Analyze table: public interface.

  @sa handler::analyze()
*/

int
handler::ha_analyze(THD* thd, HA_CHECK_OPT* check_opt)
{
  mark_trx_read_write();

  return analyze(thd, check_opt);
}


/**
  Check and repair table: public interface.

  @sa handler::check_and_repair()
*/

bool
handler::ha_check_and_repair(THD *thd)
{
  mark_trx_read_write();

  return check_and_repair(thd);
}


/**
  Disable indexes: public interface.

  @sa handler::disable_indexes()
*/

int
handler::ha_disable_indexes(uint mode)
{
  mark_trx_read_write();

  return disable_indexes(mode);
}


/**
  Enable indexes: public interface.

  @sa handler::enable_indexes()
*/

int
handler::ha_enable_indexes(uint mode)
{
  mark_trx_read_write();

  return enable_indexes(mode);
}


/**
  Discard or import tablespace: public interface.

  @sa handler::discard_or_import_tablespace()
*/

int
handler::ha_discard_or_import_tablespace(my_bool discard)
{
  mark_trx_read_write();

  return discard_or_import_tablespace(discard);
}


/**
  Prepare for alter: public interface.

  Called to prepare an *online* ALTER.

  @sa handler::prepare_for_alter()
*/

void
handler::ha_prepare_for_alter()
{
  mark_trx_read_write();

  prepare_for_alter();
}


/**
  Rename table: public interface.

  @sa handler::rename_table()
*/

int
handler::ha_rename_table(const char *from, const char *to)
{
  mark_trx_read_write();

  return rename_table(from, to);
}


/**
  Delete table: public interface.

  @sa handler::delete_table()
*/

int
handler::ha_delete_table(const char *name)
{
  mark_trx_read_write();

  return delete_table(name);
}


/**
  Drop table in the engine: public interface.

  @sa handler::drop_table()

  The difference between this and delete_table() is that the table is open in
  drop_table().
*/

void
handler::ha_drop_table(const char *name)
{
  mark_trx_read_write();

  return drop_table(name);
}


/**
  Create a table in the engine: public interface.

  @sa handler::create()
*/

int
handler::ha_create(const char *name, TABLE *form, HA_CREATE_INFO *info)
{
  mark_trx_read_write();

  return create(name, form, info);
}


/**
  Create handler files for CREATE TABLE: public interface.

  @sa handler::create_handler_files()
*/

int
handler::ha_create_handler_files(const char *name, const char *old_name,
                        int action_flag, HA_CREATE_INFO *info)
{
  mark_trx_read_write();

  return create_handler_files(name, old_name, action_flag, info);
}


/**
  Change partitions: public interface.

  @sa handler::change_partitions()
*/

int
handler::ha_change_partitions(HA_CREATE_INFO *create_info,
                     const char *path,
                     ulonglong * const copied,
                     ulonglong * const deleted,
                     const uchar *pack_frm_data,
                     size_t pack_frm_len)
{
  mark_trx_read_write();

  return change_partitions(create_info, path, copied, deleted,
                           pack_frm_data, pack_frm_len);
}


/**
  Drop partitions: public interface.

  @sa handler::drop_partitions()
*/

int
handler::ha_drop_partitions(const char *path)
{
  mark_trx_read_write();

  return drop_partitions(path);
}


/**
  Rename partitions: public interface.

  @sa handler::rename_partitions()
*/

int
handler::ha_rename_partitions(const char *path)
{
  mark_trx_read_write();

  return rename_partitions(path);
}


/**
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
  DBUG_PRINT("enter", ("on: %d", (int) on));

  if ((thd->transaction.on= on))
  {
    /*
      Now all storage engines should have transaction handling enabled.
      But some may have it enabled all the time - "disabling" transactions
      is an optimization hint that storage engine is free to ignore.
      So, let's commit an open transaction (if any) now.
    */
    if (!(error= ha_commit_trans(thd, 0)))
      error= end_trans(thd, COMMIT);
  }
  DBUG_RETURN(error);
}

int handler::index_next_same(uchar *buf, const uchar *key, uint keylen)
{
  int error;
  DBUG_ENTER("handler::index_next_same");
  if (!(error=index_next(buf)))
  {
    my_ptrdiff_t ptrdiff= buf - table->record[0];
    uchar *UNINIT_VAR(save_record_0);
    KEY *UNINIT_VAR(key_info);
    KEY_PART_INFO *UNINIT_VAR(key_part);
    KEY_PART_INFO *UNINIT_VAR(key_part_end);

    /*
      key_cmp_if_same() compares table->record[0] against 'key'.
      In parts it uses table->record[0] directly, in parts it uses
      field objects with their local pointers into table->record[0].
      If 'buf' is distinct from table->record[0], we need to move
      all record references. This is table->record[0] itself and
      the field pointers of the fields used in this key.
    */
    if (ptrdiff)
    {
      save_record_0= table->record[0];
      table->record[0]= buf;
      key_info= table->key_info + active_index;
      key_part= key_info->key_part;
      key_part_end= key_part + key_info->key_parts;
      for (; key_part < key_part_end; key_part++)
      {
        DBUG_ASSERT(key_part->field);
        key_part->field->move_field_offset(ptrdiff);
      }
    }

    if (key_cmp_if_same(table, key, active_index, keylen))
    {
      table->status=STATUS_NOT_FOUND;
      error=HA_ERR_END_OF_FILE;
    }

    /* Move back if necessary. */
    if (ptrdiff)
    {
      table->record[0]= save_record_0;
      for (key_part= key_info->key_part; key_part < key_part_end; key_part++)
        key_part->field->move_field_offset(-ptrdiff);
    }
  }
  DBUG_PRINT("return",("%i", error));
  DBUG_RETURN(error);
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
  if (table_flags() & (HA_HAS_OLD_CHECKSUM | HA_HAS_OLD_CHECKSUM))
    stat_info->check_sum= checksum();
  return;
}


/*
  Updates the global table stats with the TABLE this handler represents
*/

void handler::update_global_table_stats()
{
  TABLE_STATS * table_stats;

  status_var_add(table->in_use->status_var.rows_read, rows_read);
  DBUG_ASSERT(rows_tmp_read == 0);

  if (!table->in_use->userstat_running)
  {
    rows_read= rows_changed= 0;
    return;
  }

  if (rows_read + rows_changed == 0)
    return;                                     // Nothing to update.

  DBUG_ASSERT(table->s && table->s->table_cache_key.str);

  pthread_mutex_lock(&LOCK_global_table_stats);
  /* Gets the global table stats, creating one if necessary. */
  if (!(table_stats= (TABLE_STATS*)
        hash_search(&global_table_stats,
                    (uchar*) table->s->table_cache_key.str,
                    table->s->table_cache_key.length)))
  {
    if (!(table_stats = ((TABLE_STATS*)
                         my_malloc(sizeof(TABLE_STATS),
                                   MYF(MY_WME | MY_ZEROFILL)))))
    {
      /* Out of memory error already given */
      goto end;
    }
    memcpy(table_stats->table, table->s->table_cache_key.str,
           table->s->table_cache_key.length);
    table_stats->table_name_length= table->s->table_cache_key.length;
    table_stats->engine_type= ht->db_type;
    /* No need to set variables to 0, as we use MY_ZEROFILL above */

    if (my_hash_insert(&global_table_stats, (uchar*) table_stats))
    {
      /* Out of memory error is already given */
      my_free(table_stats, 0);
      goto end;
    }
  }
  // Updates the global table stats.
  table_stats->rows_read+=    rows_read;
  table_stats->rows_changed+= rows_changed;
  table_stats->rows_changed_x_indexes+= (rows_changed *
                                         (table->s->keys ? table->s->keys :
                                          1));
  rows_read= rows_changed= 0;
end:
  pthread_mutex_unlock(&LOCK_global_table_stats);
}


/*
  Updates the global index stats with this handler's accumulated index reads.
*/

void handler::update_global_index_stats()
{
  DBUG_ASSERT(table->s);

  if (!table->in_use->userstat_running)
  {
    /* Reset all index read values */
    bzero(index_rows_read, sizeof(index_rows_read[0]) * table->s->keys);
    return;
  }

  for (uint index = 0; index < table->s->keys; index++)
  {
    if (index_rows_read[index])
    {
      INDEX_STATS* index_stats;
      uint key_length;
      KEY *key_info = &table->key_info[index];  // Rows were read using this

      DBUG_ASSERT(key_info->cache_name);
      if (!key_info->cache_name)
        continue;
      key_length= table->s->table_cache_key.length + key_info->name_length + 1;
      pthread_mutex_lock(&LOCK_global_index_stats);
      // Gets the global index stats, creating one if necessary.
      if (!(index_stats= (INDEX_STATS*) hash_search(&global_index_stats,
                                                    key_info->cache_name,
                                                    key_length)))
      {
        if (!(index_stats = ((INDEX_STATS*)
                             my_malloc(sizeof(INDEX_STATS),
                                       MYF(MY_WME | MY_ZEROFILL)))))
          goto end;                             // Error is already given

        memcpy(index_stats->index, key_info->cache_name, key_length);
        index_stats->index_name_length= key_length;
        if (my_hash_insert(&global_index_stats, (uchar*) index_stats))
        {
          my_free(index_stats, 0);
          goto end;
        }
      }
      /* Updates the global index stats. */
      index_stats->rows_read+= index_rows_read[index];
      index_rows_read[index]= 0;
end:
      pthread_mutex_unlock(&LOCK_global_index_stats);
    }
  }
}


/****************************************************************************
** Some general functions that isn't in the handler class
****************************************************************************/

/**
  Initiates table-file and calls appropriate database-creator.

  @retval
   0  ok
  @retval
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
  
  init_tmp_table_share(thd, &share, db, 0, table_name, path);
  if (open_table_def(thd, &share, 0) ||
      open_table_from_share(thd, &share, "", 0, (uint) READ_ALL, 0, &table,
                            TRUE))
    goto err;

  if (update_create_info)
    update_create_info_from_table(create_info, &table);

  name= get_canonical_filename(table.file, share.path.str, name_buff);

  error= table.file->ha_create(name, &table, create_info);

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

/**
  Try to discover table from engine.

  @note
    If found, write the frm file to disk.

  @retval
  -1    Table did not exists
  @retval
   0    Table created ok
  @retval
   > 0  Error, table existed but could not be created
*/
int ha_create_table_from_engine(THD* thd, const char *db, const char *name)
{
  int error;
  uchar *frmblob;
  size_t frmlen;
  char path[FN_REFLEN + 1];
  HA_CREATE_INFO create_info;
  TABLE table;
  TABLE_SHARE share;
  DBUG_ENTER("ha_create_table_from_engine");
  DBUG_PRINT("enter", ("name '%s'.'%s'", db, name));

  bzero((uchar*) &create_info,sizeof(create_info));
  if ((error= ha_discover(thd, db, name, &frmblob, &frmlen)))
  {
    /* Table could not be discovered and thus not created */
    DBUG_RETURN(error);
  }

  /*
    Table exists in handler and could be discovered
    frmblob and frmlen are set, write the frm to disk
  */

  build_table_filename(path, sizeof(path) - 1, db, name, "", 0);
  // Save the frm file
  error= writefrm(path, frmblob, frmlen);
  my_free(frmblob, MYF(0));
  if (error)
    DBUG_RETURN(2);

  init_tmp_table_share(thd, &share, db, 0, name, path);
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

  get_canonical_filename(table.file, path, path);
  error=table.file->ha_create(path, &table, &create_info);
  VOID(closefrm(&table, 1));

  DBUG_RETURN(error != 0);
}

void st_ha_check_opt::init()
{
  flags= sql_flags= 0;
  start_time= my_time(0);
}


/*****************************************************************************
  Key cache handling.

  This code is only relevant for ISAM/MyISAM tables

  key_cache->cache may be 0 only in the case where a key cache is not
  initialized or when we where not able to init the key cache in a previous
  call to ha_init_key_cache() (probably out of memory)
*****************************************************************************/

/**
  Init a key cache if it has not been initied before.
*/
int ha_init_key_cache(const char *name, KEY_CACHE *key_cache)
{
  DBUG_ENTER("ha_init_key_cache");

  if (!key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    size_t tmp_buff_size= (size_t) key_cache->param_buff_size;
    uint tmp_block_size= (uint) key_cache->param_block_size;
    uint division_limit= key_cache->param_division_limit;
    uint age_threshold=  key_cache->param_age_threshold;
    uint partitions= key_cache->param_partitions;
    pthread_mutex_unlock(&LOCK_global_system_variables);
    DBUG_RETURN(!init_key_cache(key_cache,
				tmp_block_size,
				tmp_buff_size,
				division_limit, age_threshold,
                                partitions));
  }
  DBUG_RETURN(0);
}


/**
  Resize key cache.
*/
int ha_resize_key_cache(KEY_CACHE *key_cache)
{
  DBUG_ENTER("ha_resize_key_cache");

  if (key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    size_t tmp_buff_size= (size_t) key_cache->param_buff_size;
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


/**
  Change parameters for key cache (like division_limit)
*/
int ha_change_key_cache_param(KEY_CACHE *key_cache)
{
  DBUG_ENTER("ha_change_key_cache_param");

  if (key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    uint division_limit= key_cache->param_division_limit;
    uint age_threshold=  key_cache->param_age_threshold;
    pthread_mutex_unlock(&LOCK_global_system_variables);
    change_key_cache_param(key_cache, division_limit, age_threshold);
  }
  DBUG_RETURN(0);
}


/**
  Repartition key cache 
*/
int ha_repartition_key_cache(KEY_CACHE *key_cache)
{
  DBUG_ENTER("ha_repartition_key_cache");

  if (key_cache->key_cache_inited)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    size_t tmp_buff_size= (size_t) key_cache->param_buff_size;
    long tmp_block_size= (long) key_cache->param_block_size;
    uint division_limit= key_cache->param_division_limit;
    uint age_threshold=  key_cache->param_age_threshold;
    uint partitions= key_cache->param_partitions;
    pthread_mutex_unlock(&LOCK_global_system_variables);
    DBUG_RETURN(!repartition_key_cache(key_cache, tmp_block_size,
				       tmp_buff_size,
				       division_limit, age_threshold,
                                       partitions));
  }
  DBUG_RETURN(0);
}


/**
  Free memory allocated by a key cache.
*/
int ha_end_key_cache(KEY_CACHE *key_cache)
{
  end_key_cache(key_cache, 1);		// Can never fail
  return 0;
}

/**
  Move all tables from one key cache to another one.
*/
int ha_change_key_cache(KEY_CACHE *old_key_cache,
			KEY_CACHE *new_key_cache)
{
  mi_change_key_cache(old_key_cache, new_key_cache);
  return 0;
}


/**
  Try to discover one table from handler(s).

  @retval
    -1   Table did not exists
  @retval
    0   OK. In this case *frmblob and *frmlen are set
  @retval
    >0   error.  frmblob and frmlen may not be set
*/
struct st_discover_args
{
  const char *db;
  const char *name;
  uchar **frmblob; 
  size_t *frmlen;
};

static my_bool discover_handlerton(THD *thd, plugin_ref plugin,
                                   void *arg)
{
  st_discover_args *vargs= (st_discover_args *)arg;
  handlerton *hton= plugin_data(plugin, handlerton *);
  if (hton->state == SHOW_OPTION_YES && hton->discover &&
      (!(hton->discover(hton, thd, vargs->db, vargs->name, 
                        vargs->frmblob, 
                        vargs->frmlen))))
    return TRUE;

  return FALSE;
}

int ha_discover(THD *thd, const char *db, const char *name,
		uchar **frmblob, size_t *frmlen)
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
    status_var_increment(thd->status_var.ha_discover_count);
  DBUG_RETURN(error);
}


/**
  Call this function in order to give the handler the possiblity
  to ask engine if there are any new tables that should be written to disk
  or any dropped tables that need to be removed from disk
*/
struct st_find_files_args
{
  const char *db;
  const char *path;
  const char *wild;
  bool dir;
  List<LEX_STRING> *files;
};

static my_bool find_files_handlerton(THD *thd, plugin_ref plugin,
                                   void *arg)
{
  st_find_files_args *vargs= (st_find_files_args *)arg;
  handlerton *hton= plugin_data(plugin, handlerton *);


  if (hton->state == SHOW_OPTION_YES && hton->find_files)
      if (hton->find_files(hton, thd, vargs->db, vargs->path, vargs->wild, 
                          vargs->dir, vargs->files))
        return TRUE;

  return FALSE;
}

int
ha_find_files(THD *thd,const char *db,const char *path,
	      const char *wild, bool dir, List<LEX_STRING> *files)
{
  int error= 0;
  DBUG_ENTER("ha_find_files");
  DBUG_PRINT("enter", ("db: '%s'  path: '%s'  wild: '%s'  dir: %d", 
		       val_or_null(db), val_or_null(path),
                       val_or_null(wild), dir));
  st_find_files_args args= {db, path, wild, dir, files};

  plugin_foreach(thd, find_files_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &args);
  /* The return value is not currently used */
  DBUG_RETURN(error);
}

/**
  Ask handler if the table exists in engine.
  @retval
    HA_ERR_NO_SUCH_TABLE     Table does not exist
  @retval
    HA_ERR_TABLE_EXIST       Table exists
  @retval
    \#                  Error code
*/
struct st_table_exists_in_engine_args
{
  const char *db;
  const char *name;
  int err;
};

static my_bool table_exists_in_engine_handlerton(THD *thd, plugin_ref plugin,
                                   void *arg)
{
  st_table_exists_in_engine_args *vargs= (st_table_exists_in_engine_args *)arg;
  handlerton *hton= plugin_data(plugin, handlerton *);

  int err= HA_ERR_NO_SUCH_TABLE;

  if (hton->state == SHOW_OPTION_YES && hton->table_exists_in_engine)
    err = hton->table_exists_in_engine(hton, thd, vargs->db, vargs->name);

  vargs->err = err;
  if (vargs->err == HA_ERR_TABLE_EXIST)
    return TRUE;

  return FALSE;
}

int ha_table_exists_in_engine(THD* thd, const char* db, const char* name)
{
  DBUG_ENTER("ha_table_exists_in_engine");
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name));
  st_table_exists_in_engine_args args= {db, name, HA_ERR_NO_SUCH_TABLE};
  plugin_foreach(thd, table_exists_in_engine_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &args);
  DBUG_PRINT("exit", ("error: %d", args.err));
  DBUG_RETURN(args.err);
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

/** @brief
  Listing handlertons first to avoid recursive calls and deadlock
*/
static my_bool binlog_func_list(THD *thd, plugin_ref plugin, void *arg)
{
  hton_list_st *hton_list= (hton_list_st *)arg;
  handlerton *hton= plugin_data(plugin, handlerton *);
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
  hton_list_st hton_list;
  uint i, sz;

  hton_list.sz= 0;
  plugin_foreach(thd, binlog_func_list,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &hton_list);

  for (i= 0, sz= hton_list.sz; i < sz ; i++)
    hton_list.hton[i]->binlog_func(hton_list.hton[i], thd, bfn->fn, bfn->arg);
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
                                            handlerton *hton,
                                            void *args)
{
  struct binlog_log_query_st *b= (struct binlog_log_query_st*)args;
  if (hton->state == SHOW_OPTION_YES && hton->binlog_log_query)
    hton->binlog_log_query(hton, thd,
                           b->binlog_command,
                           b->query,
                           b->query_length,
                           b->db,
                           b->table_name);
  return FALSE;
}

static my_bool binlog_log_query_handlerton(THD *thd,
                                           plugin_ref plugin,
                                           void *args)
{
  return binlog_log_query_handlerton2(thd, plugin_data(plugin, handlerton *), args);
}

void ha_binlog_log_query(THD *thd, handlerton *hton,
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


/**
  Read first row between two ranges.
  Store ranges for future calls to read_range_next.

  @param start_key		Start key. Is 0 if no min range
  @param end_key		End key.  Is 0 if no max range
  @param eq_range_arg	        Set to 1 if start_key == end_key
  @param sorted		Set to 1 if result should be sorted per key

  @note
    Record is read into table->record[0]

  @retval
    0			Found row
  @retval
    HA_ERR_END_OF_FILE	No rows in range
  @retval
    \#			Error code
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
    result= ha_index_first(table->record[0]);
  else
    result= ha_index_read_map(table->record[0],
                              start_key->key,
                              start_key->keypart_map,
                              start_key->flag);
  if (result)
    DBUG_RETURN((result == HA_ERR_KEY_NOT_FOUND) 
		? HA_ERR_END_OF_FILE
		: result);

  if (compare_key(end_range) <= 0)
  {
    DBUG_RETURN(0);
  }
  else
  {
    /*
      The last read row does not fall in the range. So request
      storage engine to release row lock if possible.
    */
    unlock_row();
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
}


/**
  Read next row between two ranges.

  @note
    Record is read into table->record[0]

  @retval
    0			Found row
  @retval
    HA_ERR_END_OF_FILE	No rows in range
  @retval
    \#			Error code
*/
int handler::read_range_next()
{
  int result;
  DBUG_ENTER("handler::read_range_next");

  if (eq_range)
  {
    /* We trust that index_next_same always gives a row in range */
    DBUG_RETURN(ha_index_next_same(table->record[0],
                                   end_range->key,
                                   end_range->length));
  }
  result= ha_index_next(table->record[0]);
  if (result)
    DBUG_RETURN(result);

  if (compare_key(end_range) <= 0)
  {
    DBUG_RETURN(0);
  }
  else
  {
    /*
      The last read row does not fall in the range. So request
      storage engine to release row lock if possible.
    */
    unlock_row();
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
}


/**
  Compare if found key (in row) is over max-value.

  @param range		range to compare to row. May be 0 for no range

  @seealso
    key.cc::key_cmp()

  @return
    The return value is SIGN(key_in_row - range_key):

    - 0   : Key is equal to range or 'range' == 0 (no range)
    - -1  : Key is less than range
    - 1   : Key is larger than range
*/
int handler::compare_key(key_range *range)
{
  int cmp;
  if (!range || in_range_check_pushed_down)
    return 0;					// No max range
  cmp= key_cmp(range_key_part, range->key, range->length);
  if (!cmp)
    cmp= key_compare_result_on_equal;
  return cmp;
}


/*
  Same as compare_key() but doesn't check have in_range_check_pushed_down.
  This is used by index condition pushdown implementation.
*/

int handler::compare_key2(key_range *range)
{
  int cmp;
  if (!range)
    return 0;					// no max range
  cmp= key_cmp(range_key_part, range->key, range->length);
  if (!cmp)
    cmp= key_compare_result_on_equal;
  return cmp;
}


/**
  ICP callback - to be called by an engine to check the pushed condition
*/
extern "C" enum icp_result handler_index_cond_check(void* h_arg)
{
  handler *h= (handler*)h_arg;
  THD *thd= h->table->in_use;
  enum icp_result res;

  if (thd_killed(thd))
    return ICP_ABORTED_BY_USER;

  if (h->end_range && h->compare_key2(h->end_range) > 0)
    return ICP_OUT_OF_RANGE;
  h->increment_statistics(&SSV::ha_icp_attempts);
  if ((res= h->pushed_idx_cond->val_int()? ICP_MATCH : ICP_NO_MATCH) ==
      ICP_MATCH)
    h->increment_statistics(&SSV::ha_icp_match);
  return res;
}

int handler::index_read_idx_map(uchar * buf, uint index, const uchar * key,
                                key_part_map keypart_map,
                                enum ha_rkey_function find_flag)
{
  int error, error1;
  LINT_INIT(error1);

  error= ha_index_init(index, 0);
  if (!error)
  {
    error= index_read_map(buf, key, keypart_map, find_flag);
    error1= ha_index_end();
  }
  return error ?  error : error1;
}


/**
  Returns a list of all known extensions.

    No mutexes, worst case race is a minor surplus memory allocation
    We have to recreate the extension map if mysqld is restarted (for example
    within libmysqld)

  @retval
    pointer		pointer to TYPELIB structure
*/
static my_bool exts_handlerton(THD *unused, plugin_ref plugin,
                               void *arg)
{
  List<char> *found_exts= (List<char> *) arg;
  handlerton *hton= plugin_data(plugin, handlerton *);
  handler *file;
  if (hton->state == SHOW_OPTION_YES && hton->create &&
      (file= hton->create(hton, (TABLE_SHARE*) 0, current_thd->mem_root)))
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
  if (!known_extensions.type_names || mysys_usage_id != known_extensions_id)
  {
    List<char> found_exts;
    const char **ext, *old_ext;

    known_extensions_id= mysys_usage_id;
    found_exts.push_back((char*) TRG_EXT);
    found_exts.push_back((char*) TRN_EXT);

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


static my_bool showstat_handlerton(THD *thd, plugin_ref plugin,
                                   void *arg)
{
  enum ha_stat_type stat= *(enum ha_stat_type *) arg;
  handlerton *hton= plugin_data(plugin, handlerton *);
  if (hton->state == SHOW_OPTION_YES && hton->show_status &&
      hton->show_status(hton, thd, stat_print, stat))
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
      const LEX_STRING *name= hton_name(db_type);
      result= stat_print(thd, name->str, name->length,
                         "", 0, "DISABLED", 8) ? 1 : 0;
    }
    else
      result= db_type->show_status &&
              db_type->show_status(db_type, thd, stat_print, stat) ? 1 : 0;
  }

  if (!result)
    my_eof(thd);
  else if (!thd->is_error())
    my_error(ER_GET_ERRNO, MYF(0), 0);
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

static bool check_table_binlog_row_based(THD *thd, TABLE *table)
{
  if (table->s->cached_row_logging_check == -1)
  {
    int const check(table->s->tmp_table == NO_TMP_TABLE &&
                    binlog_filter->db_ok(table->s->db.str));
    table->s->cached_row_logging_check= check;
  }

  DBUG_ASSERT(table->s->cached_row_logging_check == 0 ||
              table->s->cached_row_logging_check == 1);

  return (thd->current_stmt_binlog_row_based &&
          table->s->cached_row_logging_check &&
          (thd->options & OPTION_BIN_LOG) &&
          mysql_bin_log.is_open());
}


/** @brief
   Write table maps for all (manually or automatically) locked tables
   to the binary log. Also, if binlog_annotate_row_events is ON,
   write Annotate_rows event before the first table map.

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

static int write_locked_table_maps(THD *thd)
{
  DBUG_ENTER("write_locked_table_maps");
  DBUG_PRINT("enter", ("thd: 0x%lx  thd->lock: 0x%lx  thd->locked_tables: 0x%lx  "
                       "thd->extra_lock: 0x%lx",
                       (long) thd, (long) thd->lock,
                       (long) thd->locked_tables, (long) thd->extra_lock));

  DBUG_PRINT("debug", ("get_binlog_table_maps(): %d", thd->get_binlog_table_maps()));

  if (thd->get_binlog_table_maps() == 0)
  {
    MYSQL_LOCK *locks[3];
    locks[0]= thd->extra_lock;
    locks[1]= thd->lock;
    locks[2]= thd->locked_tables;
    my_bool with_annotate= thd->variables.binlog_annotate_row_events &&
                           thd->query() && thd->query_length();

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
        DBUG_PRINT("info", ("Checking table %s", table->s->table_name.str));
        if (table->current_lock == F_WRLCK &&
            check_table_binlog_row_based(thd, table))
        {
          int const has_trans= table->file->has_transactions();
          int const error= thd->binlog_write_table_map(table, has_trans,
                                                       &with_annotate);
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


typedef bool Log_func(THD*, TABLE*, bool, MY_BITMAP*,
                      uint, const uchar*, const uchar*);

static int binlog_log_row(TABLE* table,
                          const uchar *before_record,
                          const uchar *after_record,
                          Log_func *log_func)
{
  if (table->no_replicate)
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
        error= (*log_func)(thd, table, table->file->has_transactions(),
                           &cols, table->s->fields,
                           before_record, after_record);

      if (!use_bitbuf)
        bitmap_free(&cols);
    }
  }
  return error ? HA_ERR_RBR_LOGGING_FAILED : 0;
}

int handler::ha_external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("handler::ha_external_lock");
  /*
    Whether this is lock or unlock, this should be true, and is to verify that
    if get_auto_increment() was called (thus may have reserved intervals or
    taken a table lock), ha_release_auto_increment() was too.
  */
  DBUG_ASSERT(next_insert_id == 0);

  /*
    We cache the table flags if the locking succeeded. Otherwise, we
    keep them as they were when they were fetched in ha_open().
  */
  int error= external_lock(thd, lock_type);
  if (error == 0)
    cached_table_flags= table_flags();
  DBUG_RETURN(error);
}


/** @brief
  Check handler usage and reset state of file to after 'open'
*/
int handler::ha_reset()
{
  DBUG_ENTER("ha_reset");
  /* Check that we have called all proper deallocation functions */
  DBUG_ASSERT((uchar*) table->def_read_set.bitmap +
              table->s->column_bitmap_size ==
              (uchar*) table->def_write_set.bitmap);
  DBUG_ASSERT(bitmap_is_set_all(&table->s->all_set));
  DBUG_ASSERT(table->key_read == 0);
  /* ensure that ha_index_end / ha_rnd_end has been called */
  DBUG_ASSERT(inited == NONE);
  /* Free cache used by filesort */
  free_io_cache(table);
  /* reset the bitmaps to point to defaults */
  table->default_column_bitmaps();
  pushed_cond= NULL;
  /* Reset information about pushed engine conditions */
  cancel_pushed_idx_cond();
  /* Reset information about pushed index conditions */
  DBUG_RETURN(reset());
}


int handler::ha_write_row(uchar *buf)
{
  int error;
  Log_func *log_func= Write_rows_log_event::binlog_row_logging_function;
  DBUG_ENTER("handler::ha_write_row");

  mark_trx_read_write();
  increment_statistics(&SSV::ha_write_count);

  if (unlikely(error= write_row(buf)))
    DBUG_RETURN(error);
  rows_changed++;
  if (unlikely(error= binlog_log_row(table, 0, buf, log_func)))
    DBUG_RETURN(error); /* purecov: inspected */

  DEBUG_SYNC_C("ha_write_row_end");
  DBUG_RETURN(0);
}


int handler::ha_update_row(const uchar *old_data, uchar *new_data)
{
  int error;
  Log_func *log_func= Update_rows_log_event::binlog_row_logging_function;

  /*
    Some storage engines require that the new record is in record[0]
    (and the old record is in record[1]).
   */
  DBUG_ASSERT(new_data == table->record[0]);

  mark_trx_read_write();
  increment_statistics(&SSV::ha_update_count);

  if (unlikely(error= update_row(old_data, new_data)))
    return error;
  rows_changed++;
  if (unlikely(error= binlog_log_row(table, old_data, new_data, log_func)))
    return error;
  return 0;
}

int handler::ha_delete_row(const uchar *buf)
{
  int error;
  Log_func *log_func= Delete_rows_log_event::binlog_row_logging_function;

  mark_trx_read_write();
  increment_statistics(&SSV::ha_delete_count);

  if (unlikely(error= delete_row(buf)))
    return error;
  rows_changed++;
  if (unlikely(error= binlog_log_row(table, buf, 0, log_func)))
    return error;
  return 0;
}



/** @brief
  use_hidden_primary_key() is called in case of an update/delete when
  (table_flags() and HA_PRIMARY_KEY_REQUIRED_FOR_DELETE) is defined
  but we don't have a primary key
*/
void handler::use_hidden_primary_key()
{
  /* fallback to use all columns in the table to identify row */
  table->use_all_columns();
}


/** @brief
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

  if ((*hton->create_iterator)(hton, HA_TRANSACTLOG_ITERATOR, &iterator) !=
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


/** @brief
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
  my_free((uchar*)iterator->buffer, MYF(MY_ALLOW_ZERO_PTR));
}


/** @brief
  returns buffer, to be assigned in handler_iterator struct
*/
enum handler_create_iterator_result
fl_log_iterator_buffer_init(struct handler_iterator *iterator)
{
  MY_DIR *dirp;
  struct fl_buff *buff;
  char *name_ptr;
  uchar *ptr;
  FILEINFO *file;
  uint32 i;

  /* to be able to make my_free without crash in case of error */
  iterator->buffer= 0;

  if (!(dirp = my_dir(fl_dir, MYF(0))))
  {
    return HA_ITERATOR_ERROR;
  }
  if ((ptr= (uchar*)my_malloc(ALIGN_SIZE(sizeof(fl_buff)) +
                             ((ALIGN_SIZE(sizeof(LEX_STRING)) +
                               sizeof(enum log_status) +
                               + FN_REFLEN + 1) *
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
                                        buff->names[buff->entries].str);
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
