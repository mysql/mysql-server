/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/** @file handler.cc

    @brief
  Handler-calling-functions
*/

#include "handler.h"

#include "my_bit.h"                   // my_count_bits
#include "myisam.h"                   // TT_FOR_UPGRADE
#include "mysql_version.h"            // MYSQL_VERSION_ID

#include "binlog.h"                   // mysql_bin_log
#include "debug_sync.h"               // DEBUG_SYNC
#include "discover.h"                 // writefrm
#include "log.h"                      // sql_print_error
#include "log_event.h"                // Write_rows_log_event
#include "my_bitmap.h"                // MY_BITMAP
#include "probes_mysql.h"             // MYSQL_HANDLER_WRLOCK_START
#include "opt_costconstantcache.h"    // reload_optimizer_cost_constants
#include "rpl_handler.h"              // RUN_HOOK
#include "sql_base.h"                 // free_io_cache
#include "sql_parse.h"                // check_stack_overrun
#include "sql_plugin.h"               // plugin_foreach
#include "sql_table.h"                // build_table_filename
#include "transaction.h"              // trans_commit_implicit
#include "trigger_def.h"              // TRG_EXT
#include "sql_select.h"               // actual_key_parts
#include "rpl_write_set_handler.h"    // add_pke
#include "auth_common.h"              // check_readonly() and SUPER_ACL


#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#include <pfs_table_provider.h>
#include <mysql/psi/mysql_table.h>

#include <pfs_transaction_provider.h>
#include <mysql/psi/mysql_transaction.h>
#include "opt_hints.h"

#include <list>
#include <cstring>
#include <string>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

/**
  @def MYSQL_TABLE_IO_WAIT
  Instrumentation helper for table io_waits.
  Note that this helper is intended to be used from
  within the handler class only, as it uses members
  from @c handler
  Performance schema events are instrumented as follows:
  - in non batch mode, one event is generated per call
  - in batch mode, the number of rows affected is saved
  in @c m_psi_numrows, so that @c end_psi_batch_mode()
  generates a single event for the batch.
  @param OP the table operation to be performed
  @param INDEX the table index used if any, or MAX_KEY.
  @param PAYLOAD instrumented code to execute
  @sa handler::end_psi_batch_mode.
*/
#ifdef HAVE_PSI_TABLE_INTERFACE
  #define MYSQL_TABLE_IO_WAIT(OP, INDEX, RESULT, PAYLOAD)     \
    {                                                         \
      if (m_psi != NULL)                                      \
      {                                                       \
        switch (m_psi_batch_mode)                             \
        {                                                     \
          case PSI_BATCH_MODE_NONE:                           \
          {                                                   \
            PSI_table_locker *sub_locker= NULL;               \
            PSI_table_locker_state reentrant_safe_state;      \
            sub_locker= PSI_TABLE_CALL(start_table_io_wait)   \
              (& reentrant_safe_state, m_psi, OP, INDEX,      \
               __FILE__, __LINE__);                           \
            PAYLOAD                                           \
            if (sub_locker != NULL)                           \
              PSI_TABLE_CALL(end_table_io_wait)               \
                (sub_locker, 1);                              \
            break;                                            \
          }                                                   \
          case PSI_BATCH_MODE_STARTING:                       \
          {                                                   \
            m_psi_locker= PSI_TABLE_CALL(start_table_io_wait) \
              (& m_psi_locker_state, m_psi, OP, INDEX,        \
               __FILE__, __LINE__);                           \
            PAYLOAD                                           \
            if (!RESULT)                                      \
              m_psi_numrows++;                                \
            m_psi_batch_mode= PSI_BATCH_MODE_STARTED;         \
            break;                                            \
          }                                                   \
          case PSI_BATCH_MODE_STARTED:                        \
          default:                                            \
          {                                                   \
            DBUG_ASSERT(m_psi_batch_mode                      \
                        == PSI_BATCH_MODE_STARTED);           \
            PAYLOAD                                           \
            if (!RESULT)                                      \
              m_psi_numrows++;                                \
            break;                                            \
          }                                                   \
        }                                                     \
      }                                                       \
      else                                                    \
      {                                                       \
        PAYLOAD                                               \
      }                                                       \
    }
#else
  #define MYSQL_TABLE_IO_WAIT(OP, INDEX, RESULT, PAYLOAD) \
    PAYLOAD
#endif

/**
  @def MYSQL_TABLE_LOCK_WAIT
  Instrumentation helper for table io_waits.
  @param OP the table operation to be performed
  @param FLAGS per table operation flags.
  @param PAYLOAD the code to instrument.
  @sa MYSQL_END_TABLE_WAIT.
*/
#ifdef HAVE_PSI_TABLE_INTERFACE
  #define MYSQL_TABLE_LOCK_WAIT(OP, FLAGS, PAYLOAD)    \
    {                                                  \
      if (m_psi != NULL)                               \
      {                                                \
        PSI_table_locker *locker;                      \
        PSI_table_locker_state state;                  \
        locker= PSI_TABLE_CALL(start_table_lock_wait)  \
          (& state, m_psi, OP, FLAGS,                  \
          __FILE__, __LINE__);                         \
        PAYLOAD                                        \
        if (locker != NULL)                            \
          PSI_TABLE_CALL(end_table_lock_wait)(locker); \
      }                                                \
      else                                             \
      {                                                \
        PAYLOAD                                        \
      }                                                \
    }
#else
  #define MYSQL_TABLE_LOCK_WAIT(OP, FLAGS, PAYLOAD) \
    PAYLOAD
#endif

using std::min;
using std::max;
using std::list;

// This is a temporary backporting fix.
#ifndef HAVE_LOG2
/*
  This will be slightly slower and perhaps a tiny bit less accurate than
  doing it the IEEE754 way but log2() should be available on C99 systems.
*/
inline double log2(double x)
{
  return (log(x) / M_LN2);
}
#endif

/*
  While we have legacy_db_type, we have this array to
  check for dups and to find handlerton from legacy_db_type.
  Remove when legacy_db_type is finally gone
*/
st_plugin_int *hton2plugin[MAX_HA];

/**
  Array allowing to check if handlerton is builtin without
  acquiring LOCK_plugin.
*/
static bool builtin_htons[MAX_HA];

const char *ha_resolve_storage_engine_name(const handlerton *db_type)
{
  return db_type == NULL ? "UNKNOWN" : hton2plugin[db_type->slot]->name.str;
}

static handlerton *installed_htons[128];

#define BITMAP_STACKBUF_SIZE (128/8)

KEY_CREATE_INFO default_key_create_info=
  { HA_KEY_ALG_UNDEF, 0, {NullS, 0}, {NullS, 0}, true };

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
  {NullS, 0}
};

const char *ha_row_type[] = {
  "", "FIXED", "DYNAMIC", "COMPRESSED", "REDUNDANT", "COMPACT",
  /* Reserved to be "PAGE" in future versions */ "?",
  "?","?","?"
};

const char *tx_isolation_names[] =
{ "READ-UNCOMMITTED", "READ-COMMITTED", "REPEATABLE-READ", "SERIALIZABLE",
  NullS};
TYPELIB tx_isolation_typelib= {array_elements(tx_isolation_names)-1,"",
			       tx_isolation_names, NULL};

#ifndef DBUG_OFF

const char *ha_legacy_type_name(legacy_db_type legacy_type)
{
  switch (legacy_type)
  {
  case DB_TYPE_UNKNOWN:
    return "DB_TYPE_UNKNOWN";
  case DB_TYPE_DIAB_ISAM:
    return "DB_TYPE_DIAB_ISAM";
  case DB_TYPE_HASH:
    return "DB_TYPE_HASH";
  case DB_TYPE_MISAM:
    return "DB_TYPE_MISAM";
  case DB_TYPE_PISAM:
    return "DB_TYPE_PISAM";
  case DB_TYPE_RMS_ISAM:
    return "DB_TYPE_RMS_ISAM";
  case DB_TYPE_HEAP:
    return "DB_TYPE_HEAP";
  case DB_TYPE_ISAM:
    return "DB_TYPE_ISAM";
  case DB_TYPE_MRG_ISAM:
    return "DB_TYPE_MRG_ISAM";
  case DB_TYPE_MYISAM:
    return "DB_TYPE_MYISAM";
  case DB_TYPE_MRG_MYISAM:
    return "DB_TYPE_MRG_MYISAM";
  case DB_TYPE_BERKELEY_DB:
    return "DB_TYPE_BERKELEY_DB";
  case DB_TYPE_INNODB:
    return "DB_TYPE_INNODB";
  case DB_TYPE_GEMINI:
    return "DB_TYPE_GEMINI";
  case DB_TYPE_NDBCLUSTER:
    return "DB_TYPE_NDBCLUSTER";
  case DB_TYPE_EXAMPLE_DB:
    return "DB_TYPE_EXAMPLE_DB";
  case DB_TYPE_ARCHIVE_DB:
    return "DB_TYPE_ARCHIVE_DB";
  case DB_TYPE_CSV_DB:
    return "DB_TYPE_CSV_DB";
  case DB_TYPE_FEDERATED_DB:
    return "DB_TYPE_FEDERATED_DB";
  case DB_TYPE_BLACKHOLE_DB:
    return "DB_TYPE_BLACKHOLE_DB";
  case DB_TYPE_PARTITION_DB:
    return "DB_TYPE_PARTITION_DB";
  case DB_TYPE_BINLOG:
    return "DB_TYPE_BINLOG";
  case DB_TYPE_SOLID:
    return "DB_TYPE_SOLID";
  case DB_TYPE_PBXT:
    return "DB_TYPE_PBXT";
  case DB_TYPE_TABLE_FUNCTION:
    return "DB_TYPE_TABLE_FUNCTION";
  case DB_TYPE_MEMCACHE:
    return "DB_TYPE_MEMCACHE";
  case DB_TYPE_FALCON:
    return "DB_TYPE_FALCON";
  case DB_TYPE_MARIA:
    return "DB_TYPE_MARIA";
  case DB_TYPE_PERFORMANCE_SCHEMA:
    return "DB_TYPE_PERFORMANCE_SCHEMA";
  default:
    return "DB_TYPE_DYNAMIC";
  }
}
#endif

/**
  Database name that hold most of mysqld system tables.
  Current code assumes that, there exists only some
  specific "database name" designated as system database.
*/
const char* mysqld_system_database= "mysql";

// System tables that belong to mysqld_system_database.
st_handler_tablename mysqld_system_tables[]= {
  {mysqld_system_database, "db"},
  {mysqld_system_database, "user"},
  {mysqld_system_database, "host"},
  {mysqld_system_database, "func"},
  {mysqld_system_database, "proc"},
  {mysqld_system_database, "event"},
  {mysqld_system_database, "plugin"},
  {mysqld_system_database, "servers"},
  {mysqld_system_database, "procs_priv"},
  {mysqld_system_database, "tables_priv"},
  {mysqld_system_database, "proxies_priv"},
  {mysqld_system_database, "columns_priv"},
  {mysqld_system_database, "time_zone"},
  {mysqld_system_database, "time_zone_name"},
  {mysqld_system_database, "time_zone_leap_second"},
  {mysqld_system_database, "time_zone_transition"},
  {mysqld_system_database, "time_zone_transition_type"},
  {mysqld_system_database, "help_category"},
  {mysqld_system_database, "help_keyword"},
  {mysqld_system_database, "help_relation"},
  {mysqld_system_database, "help_topic"},
  {mysqld_system_database, "innodb_table_stats"},
  {mysqld_system_database, "innodb_index_stats"},
  {(const char *)NULL, (const char *)NULL} /* This must be at the end */
};

/**
  This static pointer holds list of system databases from SQL layer and
  various SE's. The required memory is allocated once, and never freed.
*/
static const char **known_system_databases= NULL;
static const char **ha_known_system_databases();

// Called for each SE to get SE specific system database.
static my_bool system_databases_handlerton(THD *unused, plugin_ref plugin,
                                           void *arg);

// Called for each SE to check if given db.table_name is a system table.
static my_bool check_engine_system_table_handlerton(THD *unused,
                                                    plugin_ref plugin,
                                                    void *arg);
/**
  Structure used by SE during check for system table.
  This structure is passed to each SE handlerton and the status (OUT param)
  is collected.
*/
struct st_sys_tbl_chk_params
{
  const char *db;                             // IN param
  const char *table_name;                     // IN param
  bool is_sql_layer_system_table;             // IN param
  legacy_db_type db_type;                     // IN param

  enum enum_status
  {
    // db.table_name is user table.
    USER_TABLE,
    /*
      db.table_name is a system table,
      but may not be supported by SE.
    */
    SYSTEM_TABLE,
    /*
      db.table_name is a system table,
      and is supported by SE.
    */
    SE_SUPPORTED_SYSTEM_TABLE
  } status;                                    // OUT param
};


static plugin_ref ha_default_plugin(THD *thd)
{
  if (thd->variables.table_plugin)
    return thd->variables.table_plugin;
  return my_plugin_lock(thd, &global_system_variables.table_plugin);
}


/** @brief
  Return the default storage engine handlerton used for non-temp tables 
  for thread

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
  handlerton *hton= plugin_data<handlerton*>(plugin);
  DBUG_ASSERT(hton);
  return hton;
}


static plugin_ref ha_default_temp_plugin(THD *thd)
{
  if (thd->variables.temp_table_plugin)
    return thd->variables.temp_table_plugin;
  return my_plugin_lock(thd, &global_system_variables.temp_table_plugin);
}


/** @brief
  Return the default storage engine handlerton used for explicitly 
  created temp tables for a thread

  SYNOPSIS
    ha_default_temp_handlerton(thd)
    thd         current thread

  RETURN
    pointer to handlerton
*/
handlerton *ha_default_temp_handlerton(THD *thd)
{
  plugin_ref plugin= ha_default_temp_plugin(thd);
  DBUG_ASSERT(plugin);
  handlerton *hton= plugin_data<handlerton*>(plugin);
  DBUG_ASSERT(hton);
  return hton;
}


/**
  Resolve handlerton plugin by name, without checking for "DEFAULT" or
  HTON_NOT_USER_SELECTABLE.

  @param thd  Thread context.
  @param name Plugin name.

  @return plugin or NULL if not found.
*/
plugin_ref ha_resolve_by_name_raw(THD *thd, const LEX_CSTRING &name)
{
  return plugin_lock_by_name(thd, name, MYSQL_STORAGE_ENGINE_PLUGIN);
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
plugin_ref ha_resolve_by_name(THD *thd, const LEX_STRING *name, 
                              bool is_temp_table)
{
  const LEX_STRING *table_alias;
  plugin_ref plugin;

redo:
  /* my_strnncoll is a macro and gcc doesn't do early expansion of macro */
  if (thd && !my_charset_latin1.coll->strnncoll(&my_charset_latin1,
                           (const uchar *)name->str, name->length,
                           (const uchar *)STRING_WITH_LEN("DEFAULT"), 0))
    return is_temp_table ? 
      ha_default_plugin(thd) : ha_default_temp_plugin(thd);

  LEX_CSTRING cstring_name= {name->str, name->length};
  if ((plugin= ha_resolve_by_name_raw(thd, cstring_name)))
  {
    handlerton *hton= plugin_data<handlerton*>(plugin);
    if (hton && !(hton->flags & HTON_NOT_USER_SELECTABLE))
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

std::string normalized_se_str= "";

/*
  Parse comma separated list of disabled storage engine names
  and create a normalized string by appending storage names that
  have aliases. This normalized string is used to disallow
  table/tablespace creation under the storage engines specified.
*/
void ha_set_normalized_disabled_se_str(const std::string &disabled_se)
{
  boost::char_separator<char> sep(",");
  boost::tokenizer< boost::char_separator<char> > tokens(disabled_se, sep);
  normalized_se_str.append(",");
  BOOST_FOREACH (std::string se_name, tokens)
  {
    const LEX_STRING *table_alias;
    boost::algorithm::to_upper(se_name);
    for (table_alias= sys_table_aliases; table_alias->str; table_alias+= 2)
    {
      if (!strcasecmp(se_name.c_str(), table_alias->str) ||
          !strcasecmp(se_name.c_str(), (table_alias+1)->str))
      {
        normalized_se_str.append(std::string(table_alias->str) + "," +
                                 std::string((table_alias+1)->str) + ",");
        break;
      }
    }

    if (table_alias->str == NULL)
      normalized_se_str.append(se_name+",");
  }
}

// Check if storage engine is disabled for table/tablespace creation.
bool ha_is_storage_engine_disabled(handlerton *se_handle)
{
  if (normalized_se_str.size())
  {
    std::string se_name(",");
    se_name.append(ha_resolve_storage_engine_name(se_handle));
    se_name.append(",");
    boost::algorithm::to_upper(se_name);
    if(strstr(normalized_se_str.c_str(), se_name.c_str()))
      return true;
  }
  return false;
}


plugin_ref ha_lock_engine(THD *thd, const handlerton *hton)
{
  if (hton)
  {
    st_plugin_int **plugin= hton2plugin + hton->slot;

#ifdef DBUG_OFF
    /*
      Take a shortcut for builtin engines -- return pointer to plugin
      without acquiring LOCK_plugin mutex. This is safe safe since such
      plugins are not deleted until shutdown and we don't do reference
      counting in non-debug builds for them.

      Since we have reference to handlerton on our hands, this method
      can't be called concurrently to non-builtin handlerton initialization/
      deinitialization. So it is safe to access builtin_htons[] without
      additional locking.
     */
    if (builtin_htons[hton->slot])
      return *plugin;

    return my_plugin_lock(thd, plugin);
#else
    /*
      We can't take shortcut in debug builds.
      At least assert that builtin_htons[slot] is set correctly.
    */
    DBUG_ASSERT(builtin_htons[hton->slot] == (plugin[0]->plugin_dl == NULL));
    return my_plugin_lock(thd, &plugin);
#endif
  }
  return NULL;
}


handlerton *ha_resolve_by_legacy_type(THD *thd, enum legacy_db_type db_type)
{
  plugin_ref plugin;
  switch (db_type) {
  case DB_TYPE_DEFAULT:
    return ha_default_handlerton(thd);
  default:
    if (db_type > DB_TYPE_UNKNOWN && db_type < DB_TYPE_DEFAULT &&
        (plugin= ha_lock_engine(thd, installed_htons[db_type])))
      return plugin_data<handlerton*>(plugin);
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

  (void) RUN_HOOK(transaction, after_rollback, (thd, FALSE));

  switch (database_type) {
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


static const char **handler_errmsgs;

C_MODE_START
static const char *get_handler_errmsg(int nr)
{
  return handler_errmsgs[nr - HA_ERR_FIRST];
}
C_MODE_END


/**
  Register handler error messages for use with my_error().

  @retval
    0           OK
  @retval
    !=0         Error
*/

int ha_init_errors(void)
{
#define SETMSG(nr, msg) handler_errmsgs[(nr) - HA_ERR_FIRST]= (msg)

  /* Allocate a pointer array for the error message strings. */
  /* Zerofill it to avoid uninitialized gaps. */
  if (! (handler_errmsgs= (const char**) my_malloc(key_memory_handler_errmsgs,
                                                   HA_ERR_ERRORS * sizeof(char*),
                                                   MYF(MY_WME | MY_ZEROFILL))))
    return 1;

  /* Set the dedicated error messages. */
  SETMSG(HA_ERR_KEY_NOT_FOUND,          ER_DEFAULT(ER_KEY_NOT_FOUND));
  SETMSG(HA_ERR_FOUND_DUPP_KEY,         ER_DEFAULT(ER_DUP_KEY));
  SETMSG(HA_ERR_RECORD_CHANGED,         "Update wich is recoverable");
  SETMSG(HA_ERR_WRONG_INDEX,            "Wrong index given to function");
  SETMSG(HA_ERR_CRASHED,                ER_DEFAULT(ER_NOT_KEYFILE));
  SETMSG(HA_ERR_WRONG_IN_RECORD,        ER_DEFAULT(ER_CRASHED_ON_USAGE));
  SETMSG(HA_ERR_OUT_OF_MEM,             "Table handler out of memory");
  SETMSG(HA_ERR_NOT_A_TABLE,            "Incorrect file format '%.64s'");
  SETMSG(HA_ERR_WRONG_COMMAND,          "Command not supported");
  SETMSG(HA_ERR_OLD_FILE,               ER_DEFAULT(ER_OLD_KEYFILE));
  SETMSG(HA_ERR_NO_ACTIVE_RECORD,       "No record read in update");
  SETMSG(HA_ERR_RECORD_DELETED,         "Intern record deleted");
  SETMSG(HA_ERR_RECORD_FILE_FULL,       ER_DEFAULT(ER_RECORD_FILE_FULL));
  SETMSG(HA_ERR_INDEX_FILE_FULL,        "No more room in index file '%.64s'");
  SETMSG(HA_ERR_END_OF_FILE,            "End in next/prev/first/last");
  SETMSG(HA_ERR_UNSUPPORTED,            ER_DEFAULT(ER_ILLEGAL_HA));
  SETMSG(HA_ERR_TOO_BIG_ROW,            "Too big row");
  SETMSG(HA_WRONG_CREATE_OPTION,        "Wrong create option");
  SETMSG(HA_ERR_FOUND_DUPP_UNIQUE,      ER_DEFAULT(ER_DUP_UNIQUE));
  SETMSG(HA_ERR_UNKNOWN_CHARSET,        "Can't open charset");
  SETMSG(HA_ERR_WRONG_MRG_TABLE_DEF,    ER_DEFAULT(ER_WRONG_MRG_TABLE));
  SETMSG(HA_ERR_CRASHED_ON_REPAIR,      ER_DEFAULT(ER_CRASHED_ON_REPAIR));
  SETMSG(HA_ERR_CRASHED_ON_USAGE,       ER_DEFAULT(ER_CRASHED_ON_USAGE));
  SETMSG(HA_ERR_LOCK_WAIT_TIMEOUT,      ER_DEFAULT(ER_LOCK_WAIT_TIMEOUT));
  SETMSG(HA_ERR_LOCK_TABLE_FULL,        ER_DEFAULT(ER_LOCK_TABLE_FULL));
  SETMSG(HA_ERR_READ_ONLY_TRANSACTION,  ER_DEFAULT(ER_READ_ONLY_TRANSACTION));
  SETMSG(HA_ERR_LOCK_DEADLOCK,          ER_DEFAULT(ER_LOCK_DEADLOCK));
  SETMSG(HA_ERR_CANNOT_ADD_FOREIGN,     ER_DEFAULT(ER_CANNOT_ADD_FOREIGN));
  SETMSG(HA_ERR_NO_REFERENCED_ROW,      ER_DEFAULT(ER_NO_REFERENCED_ROW_2));
  SETMSG(HA_ERR_ROW_IS_REFERENCED,      ER_DEFAULT(ER_ROW_IS_REFERENCED_2));
  SETMSG(HA_ERR_NO_SAVEPOINT,           "No savepoint with that name");
  SETMSG(HA_ERR_NON_UNIQUE_BLOCK_SIZE,  "Non unique key block size");
  SETMSG(HA_ERR_NO_SUCH_TABLE,          "No such table: '%.64s'");
  SETMSG(HA_ERR_TABLE_EXIST,            ER_DEFAULT(ER_TABLE_EXISTS_ERROR));
  SETMSG(HA_ERR_NO_CONNECTION,          "Could not connect to storage engine");
  SETMSG(HA_ERR_TABLE_DEF_CHANGED,      ER_DEFAULT(ER_TABLE_DEF_CHANGED));
  SETMSG(HA_ERR_FOREIGN_DUPLICATE_KEY,  "FK constraint would lead to duplicate key");
  SETMSG(HA_ERR_TABLE_NEEDS_UPGRADE,    ER_DEFAULT(ER_TABLE_NEEDS_UPGRADE));
  SETMSG(HA_ERR_TABLE_READONLY,         ER_DEFAULT(ER_OPEN_AS_READONLY));
  SETMSG(HA_ERR_AUTOINC_READ_FAILED,    ER_DEFAULT(ER_AUTOINC_READ_FAILED));
  SETMSG(HA_ERR_AUTOINC_ERANGE,         ER_DEFAULT(ER_WARN_DATA_OUT_OF_RANGE));
  SETMSG(HA_ERR_TOO_MANY_CONCURRENT_TRXS, ER_DEFAULT(ER_TOO_MANY_CONCURRENT_TRXS));
  SETMSG(HA_ERR_INDEX_COL_TOO_LONG,     ER_DEFAULT(ER_INDEX_COLUMN_TOO_LONG));
  SETMSG(HA_ERR_INDEX_CORRUPT,          ER_DEFAULT(ER_INDEX_CORRUPT));
  SETMSG(HA_FTS_INVALID_DOCID,          "Invalid InnoDB FTS Doc ID");
  SETMSG(HA_ERR_TABLE_IN_FK_CHECK,	ER_DEFAULT(ER_TABLE_IN_FK_CHECK));
  SETMSG(HA_ERR_TABLESPACE_EXISTS,      "Tablespace already exists");
  SETMSG(HA_ERR_TABLESPACE_MISSING,     ER_DEFAULT(ER_TABLESPACE_MISSING));
  SETMSG(HA_ERR_FTS_EXCEED_RESULT_CACHE_LIMIT,  "FTS query exceeds result cache limit");
  SETMSG(HA_ERR_TEMP_FILE_WRITE_FAILURE,	ER_DEFAULT(ER_TEMP_FILE_WRITE_FAILURE));
  SETMSG(HA_ERR_INNODB_FORCED_RECOVERY,	ER_DEFAULT(ER_INNODB_FORCED_RECOVERY));
  SETMSG(HA_ERR_FTS_TOO_MANY_WORDS_IN_PHRASE,  "Too many words in a FTS phrase or proximity search");
  SETMSG(HA_ERR_TABLE_CORRUPT,		ER_DEFAULT(ER_TABLE_CORRUPT));
  SETMSG(HA_ERR_TABLESPACE_MISSING,	ER_DEFAULT(ER_TABLESPACE_MISSING));
  SETMSG(HA_ERR_TABLESPACE_IS_NOT_EMPTY,	ER_DEFAULT(ER_TABLESPACE_IS_NOT_EMPTY));
  SETMSG(HA_ERR_WRONG_FILE_NAME,		ER_DEFAULT(ER_WRONG_FILE_NAME));
  SETMSG(HA_ERR_NOT_ALLOWED_COMMAND,		ER_DEFAULT(ER_NOT_ALLOWED_COMMAND));
  SETMSG(HA_ERR_COMPUTE_FAILED,		"Compute virtual column value failed");
  /* Register the error messages for use with my_error(). */
  return my_error_register(get_handler_errmsg, HA_ERR_FIRST, HA_ERR_LAST);
}


int ha_finalize_handlerton(st_plugin_int *plugin)
{
  handlerton *hton= (handlerton *)plugin->data;
  DBUG_ENTER("ha_finalize_handlerton");

  /* hton can be NULL here, if ha_initialize_handlerton() failed. */
  if (!hton)
    goto end;

  switch (hton->state)
  {
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
  if (hton->slot != HA_SLOT_UNDEF)
  {
    /* Make sure we are not unpluging another plugin */
    DBUG_ASSERT(hton2plugin[hton->slot] == plugin);
    DBUG_ASSERT(hton->slot < MAX_HA);
    hton2plugin[hton->slot]= NULL;
    builtin_htons[hton->slot]= false; /* Extra correctness. */
  }

  my_free(hton);

 end:
  DBUG_RETURN(0);
}


int ha_initialize_handlerton(st_plugin_int *plugin)
{
  handlerton *hton;
  DBUG_ENTER("ha_initialize_handlerton");
  DBUG_PRINT("plugin", ("initialize plugin: '%s'", plugin->name.str));

  hton= (handlerton *)my_malloc(key_memory_handlerton,
                                sizeof(handlerton),
                                MYF(MY_WME | MY_ZEROFILL));

  if (hton == NULL)
  {
    sql_print_error("Unable to allocate memory for plugin '%s' handlerton.",
                    plugin->name.str);
    goto err_no_hton_memory;
  }

  hton->slot= HA_SLOT_UNDEF;
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
  DBUG_PRINT("info", ("hton->state=%d", hton->state));
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
          goto err_deinit;
        }
        if (hton->db_type != DB_TYPE_UNKNOWN)
          sql_print_warning("Storage engine '%s' has conflicting typecode. "
                            "Assigning value %d.", plugin->plugin->name, idx);
        hton->db_type= (enum legacy_db_type) idx;
      }

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
      builtin_htons[hton->slot]= (plugin->plugin_dl == NULL);
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
  case DB_TYPE_INNODB:
    innodb_hton= hton;
    break;
  default:
    break;
  };

  /*
    Re-load the optimizer cost constants since this storage engine can
    have non-default cost constants.
  */
  reload_optimizer_cost_constants();

  DBUG_RETURN(0);

err_deinit:
  /* 
    Let plugin do its inner deinitialization as plugin->init() 
    was successfully called before.
  */
  if (plugin->plugin->deinit)
    (void) plugin->plugin->deinit(NULL);
          
err:
  my_free(hton);
err_no_hton_memory:
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

  /*
    Initialize system database name cache.
    This cache is used to do a quick check if a given
    db.tablename is a system table.
  */
  known_system_databases= ha_known_system_databases();

  DBUG_RETURN(error);
}

void ha_end()
{
  // Unregister handler error messages.
  my_error_unregister(HA_ERR_FIRST, HA_ERR_LAST);
  my_free(handler_errmsgs);
}

static my_bool dropdb_handlerton(THD *unused1, plugin_ref plugin,
                                 void *path)
{
  handlerton *hton= plugin_data<handlerton*>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->drop_database)
    hton->drop_database(hton, (char *)path);
  return FALSE;
}


void ha_drop_database(char* path)
{
  plugin_foreach(NULL, dropdb_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, path);
}


static my_bool closecon_handlerton(THD *thd, plugin_ref plugin,
                                   void *unused)
{
  handlerton *hton= plugin_data<handlerton*>(plugin);
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


static my_bool kill_handlerton(THD *thd, plugin_ref plugin, void *)
{
  handlerton *hton= plugin_data<handlerton*>(plugin);

  if (hton->state == SHOW_OPTION_YES && hton->kill_connection)
  {
    if (thd_get_ha_data(thd, hton))
      hton->kill_connection(hton, thd);
  }

  return FALSE;
}

void ha_kill_connection(THD *thd)
{
  plugin_foreach(thd, kill_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, 0);
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

  (Historical note ends)

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

  At the end of a statement, server call trans_commit_stmt is
  invoked. This call in turn invokes handlerton::prepare()
  for every involved engine. Prepare is followed by a call
  to handlerton::commit_one_phase() If a one-phase commit
  will suffice, handlerton::prepare() is not invoked and
  the server only calls handlerton::commit_one_phase().
  At statement commit, the statement-related read-write
  engine flag is propagated to the corresponding flag in the
  normal transaction.  When the commit is complete, the list
  of registered engines is cleared.

  Rollback is handled in a similar fashion.

  Additional notes on DDL and the normal transaction.
  ---------------------------------------------------

  DDLs and operations with non-transactional engines
  do not "register" in thd->transaction lists, and thus do not
  modify the transaction state. Besides, each DDL in
  MySQL is prefixed with an implicit normal transaction commit
  (a call to trans_commit_implicit()), and thus leaves nothing
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
void trans_register_ha(THD *thd, bool all, handlerton *ht_arg,
                       const ulonglong *trxid)
{
  Ha_trx_info *ha_info;
  Transaction_ctx *trn_ctx= thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope=
    all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;

  DBUG_ENTER("trans_register_ha");
  DBUG_PRINT("enter",("%s", all ? "all" : "stmt"));

  Ha_trx_info *knownn_trans= trn_ctx->ha_trx_info(trx_scope);
  if (all)
  {
    /*
      Ensure no active backup engine data exists, unless the current transaction
      is from replication and in active xa state.
    */
    DBUG_ASSERT(thd->ha_data[ht_arg->slot].ha_ptr_backup == NULL ||
                (thd->get_transaction()->xid_state()->
                 has_state(XID_STATE::XA_ACTIVE)));
    DBUG_ASSERT(thd->ha_data[ht_arg->slot].ha_ptr_backup == NULL ||
                (thd->is_binlog_applier() || thd->slave_thread));

    thd->server_status|= SERVER_STATUS_IN_TRANS;
    if (thd->tx_read_only)
      thd->server_status|= SERVER_STATUS_IN_TRANS_READONLY;
    DBUG_PRINT("info", ("setting SERVER_STATUS_IN_TRANS"));
  }

  ha_info= thd->ha_data[ht_arg->slot].ha_info + (all ? 1 : 0);

  if (ha_info->is_started())
    DBUG_VOID_RETURN; /* already registered, return */

  ha_info->register_ha(knownn_trans, ht_arg);
  trn_ctx->set_ha_trx_info(trx_scope, ha_info);

  if (ht_arg->prepare == 0)
    trn_ctx->set_no_2pc(trx_scope, true);

  trn_ctx->xid_state()->set_query_id(thd->query_id);
/*
  Register transaction start in performance schema if not done already.
  By doing this, we handle cases when the transaction is started implicitly in
  autocommit=0 mode, and cases when we are in normal autocommit=1 mode and the
  executed statement is a single-statement transaction.

  Explicitly started transactions are handled in trans_begin().

  Do not register transactions in which binary log is the only participating
  transactional storage engine.
*/
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (thd->m_transaction_psi == NULL &&
      ht_arg->db_type != DB_TYPE_BINLOG)
  {
    const XID *xid= trn_ctx->xid_state()->get_xid();
    my_bool autocommit= !thd->in_multi_stmt_transaction_mode();
    thd->m_transaction_psi= MYSQL_START_TRANSACTION(&thd->m_transaction_state,
                                         xid, trxid, thd->tx_isolation,
                                         thd->tx_read_only, autocommit);
    DEBUG_SYNC(thd, "after_set_transaction_psi_before_set_transaction_gtid");
    gtid_set_performance_schema_values(thd);
  }
#endif
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
  int error=0;
  Transaction_ctx *trn_ctx= thd->get_transaction();
  DBUG_ENTER("ha_prepare");

  if (trn_ctx->is_active(Transaction_ctx::SESSION))
  {
    const Ha_trx_info *ha_info= trn_ctx->ha_trx_info(
      Transaction_ctx::SESSION);
    bool gtid_error= false, need_clear_owned_gtid= false;

    if ((gtid_error=
         MY_TEST(commit_owned_gtids(thd, true, &need_clear_owned_gtid))))
    {
      DBUG_ASSERT(need_clear_owned_gtid);

      ha_rollback_trans(thd, true);
      error= 1;
      goto err;
    }

    while (ha_info)
    {
      handlerton *ht= ha_info->ht();
      thd->status_var.ha_prepare_count++;
      if (ht->prepare)
      {
        DBUG_EXECUTE_IF("simulate_xa_failure_prepare", {
          ha_rollback_trans(thd, true);
          DBUG_RETURN(1);
        });
        if (ht->prepare(ht, thd, true))
        {
          ha_rollback_trans(thd, true);
          error=1;
          break;
        }
      }
      else
      {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_ILLEGAL_HA, ER(ER_ILLEGAL_HA),
                            ha_resolve_storage_engine_name(ht));
      }
      ha_info= ha_info->next();
    }

    DBUG_ASSERT(thd->get_transaction()->xid_state()->
                has_state(XID_STATE::XA_IDLE));

err:
    gtid_state_commit_or_rollback(thd, need_clear_owned_gtid, !gtid_error);
  }

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
  The function computes condition to call gtid persistor wrapper,
  and executes it.
  It is invoked at committing a statement or transaction, including XA,
  and also at XA prepare handling.

  @param thd  Thread context.
  @param all  The execution scope, true for the transaction one, false
              for the statement one.
  @param[out] need_clear_owned_gtid_ptr
              A pointer to bool variable to return the computed decision
              value.
  @return zero as no error indication, non-zero otherwise
*/

int commit_owned_gtids(THD *thd, bool all, bool *need_clear_owned_gtid_ptr)
{
  DBUG_ENTER("commit_owned_gtids(...)");
  int error= 0;

  if ((!opt_bin_log || (thd->slave_thread && !opt_log_slave_updates)) &&
      (all || !thd->in_multi_stmt_transaction_mode()) &&
      !thd->is_operating_gtid_table_implicitly &&
      !thd->is_operating_substatement_implicitly)
  {
    /*
      If the binary log is disabled for this thread (either by
      log_bin=0 or sql_log_bin=0 or by log_slave_updates=0 for a
      slave thread), then the statement will not be written to
      the binary log. In this case, we should save its GTID into
      mysql.gtid_executed table and @@GLOBAL.GTID_EXECUTED as it
      did when binlog is enabled.
    */
    if (thd->owned_gtid.sidno > 0)
    {
      error= gtid_state->save(thd);
      *need_clear_owned_gtid_ptr= true;
    }
    else if (thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS)
      *need_clear_owned_gtid_ptr= true;
  }
  else
  {
    *need_clear_owned_gtid_ptr= false;
  }

  DBUG_RETURN(error);
}


/**
  The function is a wrapper of commit_owned_gtids(...). It is invoked
  at committing a partially failed statement or transaction.

  @param thd  Thread context.

  @retval -1 if error when persisting owned gtid.
  @retval 0 if succeed to commit owned gtid.
  @retval 1 if do not meet conditions to commit owned gtid.
*/
int commit_owned_gtid_by_partial_command(THD *thd)
{
  DBUG_ENTER("commit_owned_gtid_by_partial_command(THD *thd)");
  bool need_clear_owned_gtid_ptr= false;
  int ret= 0;

  if (commit_owned_gtids(thd, true, &need_clear_owned_gtid_ptr))
  {
    /* Error when saving gtid into mysql.gtid_executed table. */
    gtid_state->update_on_rollback(thd);
    ret= -1;
  }
  else if (need_clear_owned_gtid_ptr)
  {
    gtid_state->update_on_commit(thd);
    ret= 0;
  }
  else
  {
    ret= 1;
  }

  DBUG_RETURN(ret);
}


/**
  @param[in] ignore_global_read_lock   Allow commit to complete even if a
                                       global read lock is active. This can be
                                       used to allow changes to internal tables
                                       (e.g. slave status tables).

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

int ha_commit_trans(THD *thd, bool all, bool ignore_global_read_lock)
{
  int error= 0;
  bool need_clear_owned_gtid= false;
  /*
    Save transaction owned gtid into table before transaction prepare
    if binlog is disabled, or binlog is enabled and log_slave_updates
    is disabled with slave SQL thread or slave worker thread.
  */
  error= commit_owned_gtids(thd, all, &need_clear_owned_gtid);

  /*
    'all' means that this is either an explicit commit issued by
    user, or an implicit commit issued by a DDL.
  */
  Transaction_ctx *trn_ctx= thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope=
    all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;

  /*
    "real" is a nick name for a transaction for which a commit will
    make persistent changes. E.g. a 'stmt' transaction inside a 'all'
    transation is not 'real': even though it's possible to commit it,
    the changes are not durable as they might be rolled back if the
    enclosing 'all' transaction is rolled back.
  */
  bool is_real_trans=
    all || !trn_ctx->is_active(Transaction_ctx::SESSION);

  Ha_trx_info *ha_info= trn_ctx->ha_trx_info(trx_scope);
  XID_STATE *xid_state= trn_ctx->xid_state();

  DBUG_ENTER("ha_commit_trans");

  DBUG_PRINT("info", ("all=%d thd->in_sub_stmt=%d ha_info=%p is_real_trans=%d",
                      all, thd->in_sub_stmt, ha_info, is_real_trans));
  /*
    We must not commit the normal transaction if a statement
    transaction is pending. Otherwise statement transaction
    flags will not get propagated to its normal transaction's
    counterpart.
  */
  DBUG_ASSERT(!trn_ctx->is_active(Transaction_ctx::STMT) ||
              !all);

  if (thd->in_sub_stmt)
  {
    DBUG_ASSERT(0);
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
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    DBUG_RETURN(2);
  }

  MDL_request mdl_request;
  bool release_mdl= false;
  if (ha_info)
  {
    uint rw_ha_count;
    bool rw_trans;

    DBUG_EXECUTE_IF("crash_commit_before", DBUG_SUICIDE(););

    rw_ha_count= ha_check_and_coalesce_trx_read_only(thd, ha_info, all);
    trn_ctx->set_rw_ha_count(trx_scope, rw_ha_count);
    /* rw_trans is TRUE when we in a transaction changing data */
    rw_trans= is_real_trans && (rw_ha_count > 0);

    DBUG_EXECUTE_IF("dbug.enabled_commit",
                    {
                      const char act[]= "now signal Reached wait_for signal.commit_continue";
                      DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                         STRING_WITH_LEN(act)));
                    };);
    if (rw_trans && !ignore_global_read_lock)
    {
      /*
        Acquire a metadata lock which will ensure that COMMIT is blocked
        by an active FLUSH TABLES WITH READ LOCK (and vice versa:
        COMMIT in progress blocks FTWRL).

        We allow the owner of FTWRL to COMMIT; we assume that it knows
        what it does.
      */
      MDL_REQUEST_INIT(&mdl_request,
                       MDL_key::COMMIT, "", "", MDL_INTENTION_EXCLUSIVE,
                       MDL_EXPLICIT);

      DBUG_PRINT("debug", ("Acquire MDL commit lock"));
      if (thd->mdl_context.acquire_lock(&mdl_request,
                                        thd->variables.lock_wait_timeout))
      {
        ha_rollback_trans(thd, all);
        DBUG_RETURN(1);
      }
      release_mdl= true;

      DEBUG_SYNC(thd, "ha_commit_trans_after_acquire_commit_lock");
    }

    if (rw_trans && stmt_has_updated_trans_table(ha_info)
        && check_readonly(thd, true))
    {
      ha_rollback_trans(thd, all);
      error= 1;
      goto end;
    }

    if (!trn_ctx->no_2pc(trx_scope) && (trn_ctx->rw_ha_count(trx_scope) > 1))
      error= tc_log->prepare(thd, all);
  }
  /*
    The state of XA transaction is changed to Prepared, intermediately.
    It's going to change to the regular NOTR at the end.
    The fact of the Prepared state is of interest to binary logger.
  */
  if (!error && all && xid_state->has_state(XID_STATE::XA_IDLE))
  {
    DBUG_ASSERT(thd->lex->sql_command == SQLCOM_XA_COMMIT &&
                static_cast<Sql_cmd_xa_commit*>(thd->lex->m_sql_cmd)->
                get_xa_opt() == XA_ONE_PHASE);

    xid_state->set_state(XID_STATE::XA_PREPARED);
  }
  if (error || (error= tc_log->commit(thd, all)))
  {
    ha_rollback_trans(thd, all);
    error= 1;
    goto end;
  }
/*
  Mark multi-statement (any autocommit mode) or single-statement
  (autocommit=1) transaction as rolled back
*/
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (is_real_trans && thd->m_transaction_psi != NULL)
  {
    MYSQL_COMMIT_TRANSACTION(thd->m_transaction_psi);
    thd->m_transaction_psi= NULL;
  }
#endif
  DBUG_EXECUTE_IF("crash_commit_after",
                  if (!thd->is_operating_gtid_table_implicitly)
                    DBUG_SUICIDE(););
end:
  if (release_mdl && mdl_request.ticket)
  {
    /*
      We do not always immediately release transactional locks
      after ha_commit_trans() (see uses of ha_enable_transaction()),
      thus we release the commit blocker lock as soon as it's
      not needed.
    */
    DBUG_PRINT("debug", ("Releasing MDL commit lock"));
    thd->mdl_context.release_lock(mdl_request.ticket);
  }
  /* Free resources and perform other cleanup even for 'empty' transactions. */
  if (is_real_trans)
  {
    trn_ctx->cleanup();
    thd->tx_priority= 0;
  }

  if (need_clear_owned_gtid)
  {
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    /*
      Release the owned GTID when binlog is disabled, or binlog is
      enabled and log_slave_updates is disabled with slave SQL thread
      or slave worker thread.
    */
    if (error)
      gtid_state->update_on_rollback(thd);
    else
      gtid_state->update_on_commit(thd);
  }

  DBUG_RETURN(error);
}

/**
  Commit the sessions outstanding transaction.

  @pre thd->transaction.flags.commit_low == true
  @post thd->transaction.flags.commit_low == false

  @note This function does not care about global read lock; the caller
  should.

  @param[in]  all  Is set in case of explicit commit
                   (COMMIT statement), or implicit commit
                   issued by DDL. Is not set when called
                   at the end of statement, even if
                   autocommit=1.
  @param[in]  run_after_commit
                   True by default, otherwise, does not execute
                   the after_commit hook in the function.
*/

int ha_commit_low(THD *thd, bool all, bool run_after_commit)
{
  int error=0;
  Transaction_ctx *trn_ctx= thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope=
    all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;
  Ha_trx_info *ha_info= trn_ctx->ha_trx_info(trx_scope), *ha_info_next;

  DBUG_ENTER("ha_commit_low");

  if (ha_info)
  {
    bool restore_backup_ha_data= false;
    /*
      At execution of XA COMMIT ONE PHASE binlog or slave applier
      reattaches the engine ha_data to THD, previously saved at XA START.
    */
    if (all && thd->rpl_unflag_detached_engine_ha_data())
    {
      DBUG_ASSERT(thd->lex->sql_command == SQLCOM_XA_COMMIT);
      DBUG_ASSERT(static_cast<Sql_cmd_xa_commit*>(thd->lex->m_sql_cmd)->
                  get_xa_opt() == XA_ONE_PHASE);
      restore_backup_ha_data= true;
    }

    for (; ha_info; ha_info= ha_info_next)
    {
      int err;
      handlerton *ht= ha_info->ht();
      if ((err= ht->commit(ht, thd, all)))
      {
        my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
        error=1;
      }
      thd->status_var.ha_commit_count++;
      ha_info_next= ha_info->next();
      if (restore_backup_ha_data)
        reattach_engine_ha_data_to_thd(thd, ht);
      ha_info->reset(); /* keep it conveniently zero-filled */
    }
    trn_ctx->reset_scope(trx_scope);
    if (all)
    {
      trn_ctx->invalidate_changed_tables_in_cache();
    }
  }
  /* Free resources and perform other cleanup even for 'empty' transactions. */
  if (all)
    trn_ctx->cleanup();
  /*
    When the transaction has been committed, we clear the commit_low
    flag. This allow other parts of the system to check if commit_low
    was called.
  */
  trn_ctx->m_flags.commit_low= false;
  if (run_after_commit && thd->get_transaction()->m_flags.run_hooks)
  {
    /*
       If commit succeeded, we call the after_commit hook.

       TODO: Investigate if this can be refactored so that there is
             only one invocation of this hook in the code (in
             MYSQL_LOG_BIN::finish_commit).
    */
    if (!error)
      (void) RUN_HOOK(transaction, after_commit, (thd, all));
    trn_ctx->m_flags.run_hooks= false;
  }
  DBUG_RETURN(error);
}


int ha_rollback_low(THD *thd, bool all)
{
  Transaction_ctx *trn_ctx= thd->get_transaction();
  int error= 0;
  Transaction_ctx::enum_trx_scope trx_scope=
    all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;
  Ha_trx_info *ha_info= trn_ctx->ha_trx_info(trx_scope), *ha_info_next;

  (void) RUN_HOOK(transaction, before_rollback, (thd, all));

  if (ha_info)
  {
    bool restore_backup_ha_data= false;
    /*
      Similarly to the commit case, the binlog or slave applier
      reattaches the engine ha_data to THD.
    */
    if (all && thd->rpl_unflag_detached_engine_ha_data())
    {
      DBUG_ASSERT(trn_ctx->xid_state()->get_state() != XID_STATE::XA_NOTR ||
                  thd->killed == THD::KILL_CONNECTION);

      restore_backup_ha_data= true;
    }

    for (; ha_info; ha_info= ha_info_next)
    {
      int err;
      handlerton *ht= ha_info->ht();
      if ((err= ht->rollback(ht, thd, all)))
      { // cannot happen
        my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
        error= 1;
      }
      thd->status_var.ha_rollback_count++;
      ha_info_next= ha_info->next();
      if (restore_backup_ha_data)
        reattach_engine_ha_data_to_thd(thd, ht);
      ha_info->reset(); /* keep it conveniently zero-filled */
    }
    trn_ctx->reset_scope(trx_scope);
  }

  /*
    Thanks to possibility of MDL deadlock rollback request can come even if
    transaction hasn't been started in any transactional storage engine.

    It is possible to have a call of ha_rollback_low() while handling
    failure from ha_prepare() and an error in Daignostics_area still
    wasn't set. Therefore it is required to check that an error in
    Diagnostics_area is set before calling the method XID_STATE::set_error().

    If it wasn't done it would lead to failure of the assertion
      DBUG_ASSERT(m_status == DA_ERROR)
    in the method Diagnostics_area::mysql_errno().

    In case ha_prepare is failed and an error wasn't set in Diagnostics_area
    the error ER_XA_RBROLLBACK is set in the Diagnostics_area from
    the method Sql_cmd_xa_prepare::trans_xa_prepare() when non-zero result code
    returned by ha_prepare() is handled.
  */
  if (all && thd->transaction_rollback_request && thd->is_error())
    trn_ctx->xid_state()->set_error(thd);

  (void) RUN_HOOK(transaction, after_rollback, (thd, all));
  return error;
}


int ha_rollback_trans(THD *thd, bool all)
{
  int error=0;
  Transaction_ctx *trn_ctx= thd->get_transaction();
  bool is_xa_rollback= trn_ctx->xid_state()->has_state(XID_STATE::XA_PREPARED);

  /*
    "real" is a nick name for a transaction for which a commit will
    make persistent changes. E.g. a 'stmt' transaction inside a 'all'
    transaction is not 'real': even though it's possible to commit it,
    the changes are not durable as they might be rolled back if the
    enclosing 'all' transaction is rolled back.
    We establish the value of 'is_real_trans' by checking
    if it's an explicit COMMIT or BEGIN statement, or implicit
    commit issued by DDL (in these cases all == TRUE),
    or if we're running in autocommit mode (it's only in the autocommit mode
    ha_commit_one_phase() is called with an empty
    transaction.all.ha_list, see why in trans_register_ha()).
  */
  bool is_real_trans=
    all || !trn_ctx->is_active(Transaction_ctx::SESSION);

  DBUG_ENTER("ha_rollback_trans");

  /*
    We must not rollback the normal transaction if a statement
    transaction is pending.
  */
  DBUG_ASSERT(!trn_ctx->is_active(Transaction_ctx::STMT) ||
              !all);

  if (thd->in_sub_stmt)
  {
    DBUG_ASSERT(0);
    /*
      If we are inside stored function or trigger we should not commit or
      rollback current statement transaction. See comment in ha_commit_trans()
      call for more information.
    */
    if (!all)
      DBUG_RETURN(0);
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    DBUG_RETURN(1);
  }

  if (tc_log)
    error= tc_log->rollback(thd, all);
  /*
    Mark multi-statement (any autocommit mode) or single-statement
    (autocommit=1) transaction as rolled back
  */
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (all || !thd->in_active_multi_stmt_transaction())
  {
    MYSQL_ROLLBACK_TRANSACTION(thd->m_transaction_psi);
    thd->m_transaction_psi= NULL;
  }
#endif

  /* Always cleanup. Even if nht==0. There may be savepoints. */
  if (is_real_trans)
  {
    trn_ctx->cleanup();
    thd->tx_priority= 0;
  }

  if (all)
    thd->transaction_rollback_request= FALSE;

  /*
    Only call gtid_rollback(THD*), which will purge thd->owned_gtid, if
    complete transaction is being rollback or autocommit=1.
    Notice, XA rollback has just invoked update_on_commit() through
    tc_log->*rollback* stack.
  */
  if (is_real_trans && !is_xa_rollback)
    gtid_state->update_on_rollback(thd);

  /*
    If the transaction cannot be rolled back safely, warn; don't warn if this
    is a slave thread (because when a slave thread executes a ROLLBACK, it has
    been read from the binary log, so it's 100% sure and normal to produce
    error ER_WARNING_NOT_COMPLETE_ROLLBACK. If we sent the warning to the
    slave SQL thread, it would not stop the thread but just be printed in
    the error log; but we don't want users to wonder why they have this
    message in the error log, so we don't send it.
  */
  if (is_real_trans &&
      trn_ctx->cannot_safely_rollback(
        Transaction_ctx::SESSION) &&
      !thd->slave_thread && thd->killed != THD::KILL_CONNECTION)
    trn_ctx->push_unsafe_rollback_warnings(thd);

  DBUG_RETURN(error);
}


/**
  Commit the attachable transaction in storage engines.

  @note This is slimmed down version of ha_commit_trans()/ha_commit_low()
        which commits attachable transaction but skips code which is
        unnecessary and unsafe for them (like dealing with GTIDs).
        Since attachable transactions are read-only their commit only
        needs to release resources and cleanup state in SE.

  @param thd     Current thread

  @retval 0      - Success
  @retval non-0  - Failure
*/
int ha_commit_attachable(THD *thd)
{
  int error= 0;
  Transaction_ctx *trn_ctx= thd->get_transaction();
  Ha_trx_info *ha_info= trn_ctx->ha_trx_info(Transaction_ctx::STMT);
  Ha_trx_info *ha_info_next;

  /* This function only handles attachable transactions. */
  DBUG_ASSERT(thd->is_attachable_ro_transaction_active());
  /*
    Since the attachable transaction is AUTOCOMMIT we only need
    to care about statement transaction.
  */
  DBUG_ASSERT(! trn_ctx->is_active(Transaction_ctx::SESSION));

  if (ha_info)
  {
    for (; ha_info; ha_info= ha_info_next)
    {
      /* Attachable transaction is not supposed to modify anything. */
      DBUG_ASSERT(! ha_info->is_trx_read_write());

      handlerton *ht= ha_info->ht();
      if (ht->commit(ht, thd, false))
      {
        /*
          In theory this should not happen since attachable transactions
          are read only and therefore commit is supposed to only release
          resources/cleanup state. Even if this happens we will simply
          continue committing attachable transaction in other SEs.
        */
        DBUG_ASSERT(false);
        error= 1;
      }
      thd->status_var.ha_commit_count++;
      ha_info_next= ha_info->next();

      ha_info->reset(); /* keep it conveniently zero-filled */
    }
    trn_ctx->reset_scope(Transaction_ctx::STMT);
  }

  /*
    Mark transaction as commited in PSI.
  */
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (thd->m_transaction_psi != NULL)
  {
    MYSQL_COMMIT_TRANSACTION(thd->m_transaction_psi);
    thd->m_transaction_psi= NULL;
  }
#endif

  /* Free resources and perform other cleanup even for 'empty' transactions. */
  trn_ctx->cleanup();

  return (error);
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
  const Ha_trx_info *info;
  Transaction_ctx *trn_ctx= thd->get_transaction();

  /*
    Note that below we assume that only transactional storage engines
    may need release_temporary_latches(). If this will ever become false,
    we could iterate on thd->open_tables instead (and remove duplicates
    as if (!seen[hton->slot]) { seen[hton->slot]=1; ... }).
  */
  for (info= trn_ctx->ha_trx_info(Transaction_ctx::STMT);
       info; info= info->next())
  {
    handlerton *hton= info->ht();
    if (hton && hton->release_temporary_latches)
        hton->release_temporary_latches(hton, thd);
  }
  return 0;
}

/**
  Check if all storage engines used in transaction agree that after
  rollback to savepoint it is safe to release MDL locks acquired after
  savepoint creation.

  @param thd   The client thread that executes the transaction.

  @return true  - It is safe to release MDL locks.
          false - If it is not.
*/
bool ha_rollback_to_savepoint_can_release_mdl(THD *thd)
{
  Ha_trx_info *ha_info;
  Transaction_ctx *trn_ctx= thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope=
    thd->in_sub_stmt ? Transaction_ctx::STMT : Transaction_ctx::SESSION;

  DBUG_ENTER("ha_rollback_to_savepoint_can_release_mdl");

  /**
    Checking whether it is safe to release metadata locks after rollback to
    savepoint in all the storage engines that are part of the transaction.
  */
  for (ha_info= trn_ctx->ha_trx_info(trx_scope);
       ha_info; ha_info= ha_info->next())
  {
    handlerton *ht= ha_info->ht();
    DBUG_ASSERT(ht);

    if (ht->savepoint_rollback_can_release_mdl == 0 ||
        ht->savepoint_rollback_can_release_mdl(ht, thd) == false)
      DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}

int ha_rollback_to_savepoint(THD *thd, SAVEPOINT *sv)
{
  int error=0;
  Transaction_ctx *trn_ctx= thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope=
    !thd->in_sub_stmt ? Transaction_ctx::SESSION : Transaction_ctx::STMT;

  Ha_trx_info *ha_info, *ha_info_next;

  DBUG_ENTER("ha_rollback_to_savepoint");

  trn_ctx->set_rw_ha_count(trx_scope, 0);
  trn_ctx->set_no_2pc(trx_scope, 0);
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
    thd->status_var.ha_savepoint_rollback_count++;
    if (ht->prepare == 0)
      trn_ctx->set_no_2pc(trx_scope, true);
  }

  /*
    rolling back the transaction in all storage engines that were not part of
    the transaction when the savepoint was set
  */
  for (ha_info= trn_ctx->ha_trx_info(trx_scope); ha_info != sv->ha_list;
       ha_info= ha_info_next)
  {
    int err;
    handlerton *ht= ha_info->ht();
    if ((err= ht->rollback(ht, thd, !thd->in_sub_stmt)))
    { // cannot happen
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err);
      error=1;
    }
    thd->status_var.ha_rollback_count++;
    ha_info_next= ha_info->next();
    ha_info->reset(); /* keep it conveniently zero-filled */
  }
  trn_ctx->set_ha_trx_info(trx_scope, sv->ha_list);

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (thd->m_transaction_psi != NULL)
    MYSQL_INC_TRANSACTION_ROLLBACK_TO_SAVEPOINT(thd->m_transaction_psi, 1);
#endif

  DBUG_RETURN(error);
}

int ha_prepare_low(THD *thd, bool all)
{
  int error= 0;
  Transaction_ctx::enum_trx_scope trx_scope=
    all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;
  Ha_trx_info *ha_info= thd->get_transaction()->ha_trx_info(trx_scope);

  DBUG_ENTER("ha_prepare_low");

  if (ha_info)
  {
    for (; ha_info && !error; ha_info= ha_info->next())
    {
      int err= 0;
      handlerton *ht= ha_info->ht();
      /*
        Do not call two-phase commit if this particular
        transaction is read-only. This allows for simpler
        implementation in engines that are always read-only.
      */
      if (!ha_info->is_trx_read_write())
        continue;
      if ((err= ht->prepare(ht, thd, all)))
      {
        my_error(ER_ERROR_DURING_COMMIT, MYF(0), err);
        error= 1;
      }
      thd->status_var.ha_prepare_count++;
    }
    DBUG_EXECUTE_IF("crash_commit_after_prepare", DBUG_SUICIDE(););
  }

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
  Transaction_ctx::enum_trx_scope trx_scope=
    !thd->in_sub_stmt ? Transaction_ctx::SESSION : Transaction_ctx::STMT;
  Ha_trx_info *ha_info= thd->get_transaction()->ha_trx_info(trx_scope);
  Ha_trx_info *begin_ha_info= ha_info;

  DBUG_ENTER("ha_savepoint");

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
    thd->status_var.ha_savepoint_count++;
  }
  /*
    Remember the list of registered storage engines. All new
    engines are prepended to the beginning of the list.
  */
  sv->ha_list= begin_ha_info;

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (!error && thd->m_transaction_psi != NULL)
    MYSQL_INC_TRANSACTION_SAVEPOINTS(thd->m_transaction_psi, 1);
#endif

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

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (thd->m_transaction_psi != NULL)
    MYSQL_INC_TRANSACTION_RELEASE_SAVEPOINT(thd->m_transaction_psi, 1);
#endif
  DBUG_RETURN(error);
}


static my_bool snapshot_handlerton(THD *thd, plugin_ref plugin,
                                   void *arg)
{
  handlerton *hton= plugin_data<handlerton*>(plugin);
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

  plugin_foreach(thd, snapshot_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, &warn);

  /*
    Same idea as when one wants to CREATE TABLE in one engine which does not
    exist:
  */
  if (warn)
    push_warning(thd, Sql_condition::SL_WARNING, ER_UNKNOWN_ERROR,
                 "This MySQL server does not support any "
                 "consistent-read capable storage engine");
  return 0;
}


static my_bool flush_handlerton(THD *thd, plugin_ref plugin,
                                void *arg)
{
  handlerton *hton= plugin_data<handlerton*>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->flush_logs &&
      hton->flush_logs(hton, *(static_cast<bool *>(arg))))
    return TRUE;
  return FALSE;
}


bool ha_flush_logs(handlerton *db_type, bool binlog_group_flush)
{
  if (db_type == NULL)
  {
    if (plugin_foreach(NULL, flush_handlerton,
                       MYSQL_STORAGE_ENGINE_PLUGIN,
                       static_cast<void *>(&binlog_group_flush)))
      return TRUE;
  }
  else
  {
    if (db_type->state != SHOW_OPTION_YES ||
        (db_type->flush_logs &&
         db_type->flush_logs(db_type, binlog_group_flush)))
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
    my_stpcpy(tmp_path, path);

  /*
    we only should turn into lowercase database/table part
    so start the process after homedirectory
  */
  my_casedn_str(files_charset_info, tmp_path + mysql_data_home_len);
  return tmp_path;
}


class Ha_delete_table_error_handler: public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    /* Downgrade errors to warnings. */
    if (*level == Sql_condition::SL_ERROR)
      *level= Sql_condition::SL_WARNING;
    return false;
  }
};


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
    dummy_table.alias= alias;

    file->change_table_ptr(&dummy_table, &dummy_share);

    /*
      XXX: should we convert *all* errors to warnings here?
      What if the error is fatal?
    */
    thd->push_internal_handler(&ha_delete_table_error_handler);
    file->print_error(error, 0);

    thd->pop_internal_handler();
  }
  delete file;

#ifdef HAVE_PSI_TABLE_INTERFACE
  if (likely(error == 0))
  {
    /* Table share not available, so check path for temp_table prefix. */
    bool temp_table= (strstr(path, tmp_file_prefix) != NULL);
    PSI_TABLE_CALL(drop_table_share)
      (temp_table, db, strlen(db), alias, strlen(alias));
  }
#endif

  DBUG_RETURN(error);
}

/****************************************************************************
** General handler functions
****************************************************************************/
handler *handler::clone(const char *name, MEM_ROOT *mem_root)
{
  DBUG_ENTER("handler::clone");
  handler *new_handler= get_new_handler(table->s, mem_root, ht);

  if (!new_handler)
    DBUG_RETURN(NULL);
  if (new_handler->set_ha_share_ref(ha_share))
    goto err;

  /*
    Allocate handler->ref here because otherwise ha_open will allocate it
    on this->table->mem_root and we will not be able to reclaim that memory 
    when the clone handler object is destroyed.
  */
  if (!(new_handler->ref= (uchar*) alloc_root(mem_root,
                                              ALIGN_SIZE(ref_length)*2)))
    goto err;
  /*
    TODO: Implement a more efficient way to have more than one index open for
    the same table instance. The ha_open call is not cachable for clone.
  */
  if (new_handler->ha_open(table, name, table->db_stat,
                           HA_OPEN_IGNORE_IF_LOCKED))
    goto err;

  DBUG_RETURN(new_handler);

err:
  delete new_handler;
  DBUG_RETURN(NULL);
}


void handler::ha_statistic_increment(ulonglong SSV::*offset) const
{
  if (table && table->in_use) (table->in_use->status_var.*offset)++;
}


THD *handler::ha_thd(void) const
{
  DBUG_ASSERT(!table || !table->in_use || table->in_use == current_thd);
  return (table && table->in_use) ? table->in_use : current_thd;
}

void handler::unbind_psi()
{
#ifdef HAVE_PSI_TABLE_INTERFACE
  DBUG_ASSERT(m_lock_type == F_UNLCK);
  DBUG_ASSERT(inited == NONE);
  /*
    Notify the instrumentation that this table is not owned
    by this thread any more.
  */
  PSI_TABLE_CALL(unbind_table)(m_psi);
#endif
}

void handler::rebind_psi()
{
#ifdef HAVE_PSI_TABLE_INTERFACE
  DBUG_ASSERT(m_lock_type == F_UNLCK);
  DBUG_ASSERT(inited == NONE);
  /*
    Notify the instrumentation that this table is now owned
    by this thread.
  */
  PSI_table_share *share_psi= ha_table_share_psi(table_share);
  m_psi= PSI_TABLE_CALL(rebind_table)(share_psi, this, m_psi);
#endif
}

void handler::start_psi_batch_mode()
{
#ifdef HAVE_PSI_TABLE_INTERFACE
  DBUG_ASSERT(m_psi_batch_mode == PSI_BATCH_MODE_NONE);
  DBUG_ASSERT(m_psi_locker == NULL);
  m_psi_batch_mode= PSI_BATCH_MODE_STARTING;
  m_psi_numrows= 0;
#endif
}

void handler::end_psi_batch_mode()
{
#ifdef HAVE_PSI_TABLE_INTERFACE
  DBUG_ASSERT(m_psi_batch_mode != PSI_BATCH_MODE_NONE);
  if (m_psi_locker != NULL)
  {
    DBUG_ASSERT(m_psi_batch_mode == PSI_BATCH_MODE_STARTED);
    PSI_TABLE_CALL(end_table_io_wait)(m_psi_locker, m_psi_numrows);
    m_psi_locker= NULL;
  }
  m_psi_batch_mode= PSI_BATCH_MODE_NONE;
#endif
}

PSI_table_share *handler::ha_table_share_psi(const TABLE_SHARE *share) const
{
  return share->m_psi;
}

/** @brief
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
              name, ht->db_type, table_arg->db_stat, mode,
              test_if_locked));

  table= table_arg;
  DBUG_ASSERT(table->s == table_share);
  DBUG_ASSERT(m_lock_type == F_UNLCK);
  DBUG_PRINT("info", ("old m_lock_type: %d F_UNLCK %d", m_lock_type, F_UNLCK));
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
    set_my_errno(error);                            /* Safeguard */
    DBUG_PRINT("error",("error: %d  errno: %d",error,errno));
  }
  else
  {
    DBUG_ASSERT(m_psi == NULL);
    DBUG_ASSERT(table_share != NULL);
#ifdef HAVE_PSI_TABLE_INTERFACE
    /*
      Do not call this for partitions handlers, since it may take too much
      resources.
      So only use the m_psi on table level, not for individual partitions.
    */
    if (!(test_if_locked & HA_OPEN_NO_PSI_CALL))
    {
      PSI_table_share *share_psi= ha_table_share_psi(table_share);
      m_psi= PSI_TABLE_CALL(open_table)(share_psi, this);
    }
#endif

    if (table->s->db_options_in_use & HA_OPTION_READ_ONLY_DATA)
      table->db_stat|=HA_READ_ONLY;
    (void) extra(HA_EXTRA_NO_READCHECK);	// Not needed in SQL

    /* ref is already allocated for us if we're called from handler::clone() */
    if (!ref && !(ref= (uchar*) alloc_root(&table->mem_root, 
                                          ALIGN_SIZE(ref_length)*2)))
    {
      ha_close();
      error=HA_ERR_OUT_OF_MEM;
    }
    else
      dup_ref=ref+ALIGN_SIZE(ref_length);
    cached_table_flags= table_flags();
  }
  DBUG_RETURN(error);
}


/**
  Close handler.
*/

int handler::ha_close(void)
{
  DBUG_ENTER("handler::ha_close");
#ifdef HAVE_PSI_TABLE_INTERFACE
  PSI_TABLE_CALL(close_table)(table_share, m_psi);
  m_psi= NULL; /* instrumentation handle, invalid after close_table() */
  DBUG_ASSERT(m_psi_batch_mode == PSI_BATCH_MODE_NONE);
  DBUG_ASSERT(m_psi_locker == NULL);
#endif
  // TODO: set table= NULL to mark the handler as closed?
  DBUG_ASSERT(m_psi == NULL);
  DBUG_ASSERT(m_lock_type == F_UNLCK);
  DBUG_ASSERT(inited == NONE);
  DBUG_RETURN(close());
}


/**
  Initialize use of index.

  @param idx     Index to use
  @param sorted  Use sorted order

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_index_init(uint idx, bool sorted)
{
  DBUG_EXECUTE_IF("ha_index_init_fail", return HA_ERR_TABLE_DEF_CHANGED;);
  int result;
  DBUG_ENTER("ha_index_init");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == NONE);
  if (!(result= index_init(idx, sorted)))
    inited= INDEX;
  end_range= NULL;
  DBUG_RETURN(result);
}


/**
  End use of index.

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_index_end()
{
  DBUG_ENTER("ha_index_end");
  /* SQL HANDLER function can call this without having it locked. */
  DBUG_ASSERT(table->open_by_handler ||
              table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == INDEX);
  inited= NONE;
  end_range= NULL;
  DBUG_RETURN(index_end());
}


/**
  Initialize table for random read or scan.

  @param scan  if true: Initialize for random scans through rnd_next()
               if false: Initialize for random reads through rnd_pos()

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_rnd_init(bool scan)
{
  DBUG_EXECUTE_IF("ha_rnd_init_fail", return HA_ERR_TABLE_DEF_CHANGED;);
  int result;
  DBUG_ENTER("ha_rnd_init");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == NONE || (inited == RND && scan));
  inited= (result= rnd_init(scan)) ? NONE : RND;
  end_range= NULL;
  DBUG_RETURN(result);
}


/**
  End use of random access.

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_rnd_end()
{
  DBUG_ENTER("ha_rnd_end");
  /* SQL HANDLER function can call this without having it locked. */
  DBUG_ASSERT(table->open_by_handler ||
              table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == RND);
  inited= NONE;
  end_range= NULL;
  DBUG_RETURN(rnd_end());
}


/**
  Read next row via random scan.

  @param buf  Buffer to read the row into

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_rnd_next(uchar *buf)
{
  int result;
  DBUG_EXECUTE_IF("ha_rnd_next_deadlock", return HA_ERR_LOCK_DEADLOCK;);
  DBUG_ENTER("handler::ha_rnd_next");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == RND);

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, MAX_KEY, result,
    { result= rnd_next(buf); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table);
    m_update_generated_read_fields= false;
  }
  DBUG_RETURN(result);
}


/**
  Read row via random scan from position.

  @param[out] buf  Buffer to read the row into
  @param      pos  Position from position() call

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_rnd_pos(uchar *buf, uchar *pos)
{
  int result;
  DBUG_ENTER("handler::ha_rnd_pos");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  /* TODO: Find out how to solve ha_rnd_pos when finding duplicate update. */
  /* DBUG_ASSERT(inited == RND); */

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, MAX_KEY, result,
    { result= rnd_pos(buf, pos); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table);
    m_update_generated_read_fields= false;
  }
  DBUG_RETURN(result);
}


/**
  Read [part of] row via [part of] index.
  @param[out] buf          buffer where store the data
  @param      key          Key to search for
  @param      keypart_map  Which part of key to use
  @param      find_flag    Direction/condition on key usage

  @returns Operation status
    @retval  0                   Success (found a record, and function has
                                 set table->status to 0)
    @retval  HA_ERR_END_OF_FILE  Row not found (function has set table->status
                                 to STATUS_NOT_FOUND). End of index passed.
    @retval  HA_ERR_KEY_NOT_FOUND Row not found (function has set table->status
                                 to STATUS_NOT_FOUND). Index cursor positioned.
    @retval  != 0                Error

  @note Positions an index cursor to the index specified in the handle.
  Fetches the row if available. If the key value is null,
  begin at the first key of the index.
  ha_index_read_map can be restarted without calling index_end on the previous
  index scan and without calling ha_index_init. In this case the
  ha_index_read_map is on the same index as the previous ha_index_scan.
  This is particularly used in conjunction with multi read ranges.
*/

int handler::ha_index_read_map(uchar *buf, const uchar *key,
                               key_part_map keypart_map,
                               enum ha_rkey_function find_flag)
{
  int result;
  DBUG_ENTER("handler::ha_index_read_map");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == INDEX);
  DBUG_ASSERT(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
    { result= index_read_map(buf, key, keypart_map, find_flag); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields= false;
  }
  DBUG_RETURN(result);
}

int handler::ha_index_read_last_map(uchar *buf, const uchar *key,
                                    key_part_map keypart_map)
{
  int result;
  DBUG_ENTER("handler::ha_index_read_last_map");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == INDEX);
  DBUG_ASSERT(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
    { result= index_read_last_map(buf, key, keypart_map); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields= false;
  }
  DBUG_RETURN(result);
}

/**
  Initializes an index and read it.

  @see handler::ha_index_read_map.
*/

int handler::ha_index_read_idx_map(uchar *buf, uint index, const uchar *key,
                                   key_part_map keypart_map,
                                   enum ha_rkey_function find_flag)
{
  int result;
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(end_range == NULL);
  DBUG_ASSERT(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, index, result,
    { result= index_read_idx_map(buf, index, key, keypart_map, find_flag); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table, index);
    m_update_generated_read_fields= false;
  }
  return result;
}


/**
  Reads the next row via index.

  @param[out] buf  Row data

  @return Operation status.
    @retval  0                   Success
    @retval  HA_ERR_END_OF_FILE  Row not found
    @retval  != 0                Error
*/

int handler::ha_index_next(uchar * buf)
{
  int result;
  DBUG_ENTER("handler::ha_index_next");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == INDEX);
  DBUG_ASSERT(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
    { result= index_next(buf); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields= false;
  }
  DBUG_RETURN(result);
}


/**
  Reads the previous row via index.

  @param[out] buf  Row data

  @return Operation status.
    @retval  0                   Success
    @retval  HA_ERR_END_OF_FILE  Row not found
    @retval  != 0                Error
*/

int handler::ha_index_prev(uchar * buf)
{
  int result;
  DBUG_ENTER("handler::ha_index_prev");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == INDEX);
  DBUG_ASSERT(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
    { result= index_prev(buf); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields= false;
  }
  DBUG_RETURN(result);
}


/**
  Reads the first row via index.

  @param[out] buf  Row data

  @return Operation status.
    @retval  0                   Success
    @retval  HA_ERR_END_OF_FILE  Row not found
    @retval  != 0                Error
*/

int handler::ha_index_first(uchar * buf)
{
  int result;
  DBUG_ENTER("handler::ha_index_first");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == INDEX);
  DBUG_ASSERT(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
    { result= index_first(buf); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields= false;
  }
  DBUG_RETURN(result);
}


/**
  Reads the last row via index.

  @param[out] buf  Row data

  @return Operation status.
    @retval  0                   Success
    @retval  HA_ERR_END_OF_FILE  Row not found
    @retval  != 0                Error
*/

int handler::ha_index_last(uchar * buf)
{
  int result;
  DBUG_ENTER("handler::ha_index_last");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == INDEX);
  DBUG_ASSERT(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
    { result= index_last(buf); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields= false;
  }
  DBUG_RETURN(result);
}


/**
  Reads the next same row via index.

  @param[out] buf     Row data
  @param      key     Key to search for
  @param      keylen  Length of key

  @return Operation status.
    @retval  0                   Success
    @retval  HA_ERR_END_OF_FILE  Row not found
    @retval  != 0                Error
*/

int handler::ha_index_next_same(uchar *buf, const uchar *key, uint keylen)
{
  int result;
  DBUG_ENTER("handler::ha_index_next_same");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ASSERT(inited == INDEX);
  DBUG_ASSERT(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
    { result= index_next_same(buf, key, keylen); })
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields= false;
  }
  DBUG_RETURN(result);
}


/**
  Read first row (only) from a table.

  This is never called for InnoDB tables, as these table types
  has the HA_STATS_RECORDS_IS_EXACT set.
*/
int handler::read_first_row(uchar * buf, uint primary_key)
{
  int error;
  DBUG_ENTER("handler::read_first_row");

  ha_statistic_increment(&SSV::ha_read_first_count);

  /*
    If there is very few deleted rows in the table, find the first row by
    scanning the table.
    TODO remove the test for HA_READ_ORDER
  */
  if (stats.deleted < 10 || primary_key >= MAX_KEY ||
      !(index_flags(primary_key, 0, 0) & HA_READ_ORDER))
  {
    if (!(error= ha_rnd_init(1)))
    {
      while ((error= ha_rnd_next(buf)) == HA_ERR_RECORD_DELETED)
        /* skip deleted row */;
      const int end_error= ha_rnd_end();
      if (!error)
        error= end_error;
    }
  }
  else
  {
    /* Find the first row through the primary key */
    if (!(error= ha_index_init(primary_key, 0)))
    {
      error= ha_index_first(buf);
      const int end_error= ha_index_end();
      if (!error)
        error= end_error;
    }
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
    return ULLONG_MAX;

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
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
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
      Also we should take into account the the sign of the value.
      Since auto_increment value can't have negative value we should update
      next_insert_id only in case when we INSERTing explicit positive value.
      It means that for a table that has SIGNED INTEGER column when we execute
      the following statement
      INSERT INTO t1 VALUES( NULL), (-1), (NULL)
      we shouldn't call adjust_next_insert_id_after_explicit_value()
      and the result row will be (1, -1, 2) (for new opened connection
      to the server). On the other hand, for the statement
      INSERT INTO t1 VALUES( NULL), (333), (NULL)
      we should call adjust_next_insert_id_after_explicit_value()
      and result row will be (1, 333, 334).
    */
    if (((Field_num*)table->next_number_field)->unsigned_flag ||
        ((longlong)nr) > 0)
      adjust_next_insert_id_after_explicit_value(nr);

    insert_id_for_cur_row= 0; // didn't generate anything
    DBUG_RETURN(0);
  }

  if (next_insert_id > table->next_number_field->get_max_int_value())
    DBUG_RETURN(HA_ERR_AUTOINC_READ_FAILED);

  if ((nr= next_insert_id) >= auto_inc_interval_for_cur_row.maximum())
  {
    /* next_insert_id is beyond what is reserved, so we reserve more. */
    const Discrete_interval *forced=
      thd->auto_inc_intervals_forced.get_next();
    if (forced != NULL)
    {
      nr= forced->minimum();
      /*
        In a multi insert statement when the number of affected rows is known
        then reserve those many number of auto increment values. So that
        interval will be starting value to starting value + number of affected
        rows * increment of auto increment.
       */
      nb_reserved_values= (estimation_rows_to_insert > 0) ?
        estimation_rows_to_insert : forced->values();
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
               (thd->lex->bulk_insert_row_cnt > 0))
      {
        /*
          For multi-row inserts, if the bulk inserts cannot be started, the
          handler::estimation_rows_to_insert will not be set. But we still
          want to reserve the autoinc values.
        */
        nb_desired_values= thd->lex->bulk_insert_row_cnt;
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
      if (nr == ULLONG_MAX)
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

  if (unlikely(nr == ULLONG_MAX))
      DBUG_RETURN(HA_ERR_AUTOINC_ERANGE); 

  DBUG_PRINT("info",("auto_increment: %lu", (ulong) nr));

  if (unlikely(table->next_number_field->store((longlong) nr, TRUE)))
  {
    /*
      first test if the query was aborted due to strict mode constraints
    */
    if (thd->killed == THD::KILL_BAD_DATA)
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
    if (mysql_bin_log.is_open() && !thd->is_current_stmt_binlog_format_row())
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
  DBUG_PRINT("info", ("read_set: 0x%lx  write_set: 0x%lx", (long) table->read_set,
                      (long)table->write_set));
  DBUG_VOID_RETURN;
}


/**
  Reserves an interval of auto_increment values from the handler.

  @param       offset              offset (modulus increment)
  @param       increment           increment between calls
  @param       nb_desired_values   how many values we want
  @param[out]  first_value         the first value reserved by the handler
  @param[out]  nb_reserved_values  how many values the handler reserved

  offset and increment means that we want values to be of the form
  offset + N * increment, where N>=0 is integer.
  If the function sets *first_value to ULLONG_MAX it means an error.
  If the function sets *nb_reserved_values to ULLONG_MAX it means it has
  reserved to "positive infinite".
*/

void handler::get_auto_increment(ulonglong offset, ulonglong increment,
                                 ulonglong nb_desired_values,
                                 ulonglong *first_value,
                                 ulonglong *nb_reserved_values)
{
  ulonglong nr;
  int error;
  DBUG_ENTER("handler::get_auto_increment");

  (void) extra(HA_EXTRA_KEYREAD);
  table->mark_columns_used_by_index_no_reset(table->s->next_number_index,
                                        table->read_set);
  column_bitmaps_signal();

  if (ha_index_init(table->s->next_number_index, 1))
  {
    /* This should never happen, assert in debug, and fail in release build */
    DBUG_ASSERT(0);
    *first_value= ULLONG_MAX;
    DBUG_VOID_RETURN;
  }

  if (table->s->next_number_keypart == 0)
  {						// Autoincrement at key-start
    error= ha_index_last(table->record[1]);
    /*
      MySQL implicitely assumes such method does locking (as MySQL decides to
      use nr+increment without checking again with the handler, in
      handler::update_auto_increment()), so reserves to infinite.
    */
    *nb_reserved_values= ULLONG_MAX;
  }
  else
  {
    uchar key[MAX_KEY_LENGTH];
    key_copy(key, table->record[0],
             table->key_info + table->s->next_number_index,
             table->s->next_number_key_offset);
    error= ha_index_read_map(table->record[1], key,
                             make_prev_keypart_map(table->s->next_number_keypart),
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
  {
    if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND)
    {
      /* No entry found, start with 1. */
      nr= 1;
    }
    else
    {
      DBUG_ASSERT(0);
      nr= ULLONG_MAX;
    }
  }
  else
    nr= ((ulonglong) table->next_number_field->
         val_int_offset(table->s->rec_buff_length)+1);
  ha_index_end();
  (void) extra(HA_EXTRA_NO_KEYREAD);
  *first_value= nr;
  DBUG_VOID_RETURN;
}


void handler::ha_release_auto_increment()
{
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK ||
              (!next_insert_id && !insert_id_for_cur_row));
  DEBUG_SYNC(ha_thd(), "release_auto_increment");
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
}


/**
  Construct and emit duplicate key error message using information
  from table's record buffer.

  @param table    TABLE object which record buffer should be used as
                  source for column values.
  @param key      Key description.
  @param msg      Error message template to which key value should be
                  added.
  @param errflag  Flags for my_error() call.
*/

void print_keydup_error(TABLE *table, KEY *key, const char *msg, myf errflag)
{
  /* Write the duplicated key in the error message */
  char key_buff[MAX_KEY_LENGTH];
  String str(key_buff,sizeof(key_buff),system_charset_info);

  if (key == NULL)
  {
    /* Key is unknown */
    str.copy("", 0, system_charset_info);
    my_printf_error(ER_DUP_ENTRY, msg, errflag, str.c_ptr(), "*UNKNOWN*");
  }
  else
  {
    /* Table is opened and defined at this point */
    key_unpack(&str,table, key);
    size_t max_length= MYSQL_ERRMSG_SIZE - strlen(msg);
    if (str.length() >= max_length)
    {
      str.length(max_length-4);
      str.append(STRING_WITH_LEN("..."));
    }
    my_printf_error(ER_DUP_ENTRY, msg, errflag, str.c_ptr_safe(), key->name);
  }
}


/**
  Construct and emit duplicate key error message using information
  from table's record buffer.

  @sa print_keydup_error(table, key, msg, errflag).
*/

void print_keydup_error(TABLE *table, KEY *key, myf errflag)
{
  print_keydup_error(table, key, ER(ER_DUP_ENTRY_WITH_KEY_NAME), errflag);
}


/**
  This method is used to analyse the error to see whether the error
  is ignorable or not. Further comments in header file.
*/

bool handler::is_ignorable_error(int error)
{
  DBUG_ENTER("is_ignorable_error");

  // Catch errors that are ignorable
  switch (error)
  {
    // Error code 0 is not an error.
    case 0:
    // Dup key errors may be explicitly ignored.
    case HA_ERR_FOUND_DUPP_KEY:
    case HA_ERR_FOUND_DUPP_UNIQUE:
    // Foreign key constraint violations are ignorable.
    case HA_ERR_ROW_IS_REFERENCED:
    case HA_ERR_NO_REFERENCED_ROW:
      DBUG_RETURN(true);
  }

  // Default is that an error is not ignorable.
  DBUG_RETURN(false);
}


/**
  This method is used to analyse the error to see whether the error
  is fatal or not. Further comments in header file.
*/

bool handler::is_fatal_error(int error)
{
  DBUG_ENTER("is_fatal_error");

  // No ignorable errors are fatal
  if (is_ignorable_error(error))
    DBUG_RETURN(false);

  // Catch errors that are not fatal
  switch (error)
  {
    /*
      Deadlock and lock timeout cause transaction/statement rollback so that
      THD::is_fatal_sub_stmt_error will be set. This means that they will not
      be possible to handle by stored program handlers inside stored functions
      and triggers even if non-fatal.
    */
    case HA_ERR_LOCK_WAIT_TIMEOUT:
    case HA_ERR_LOCK_DEADLOCK:
      DBUG_RETURN(false);

    case HA_ERR_NULL_IN_SPATIAL:
      DBUG_RETURN(false);
  }

  // Default is that an error is fatal
  DBUG_RETURN(true);
}


/**
  Print error that we got from handler function.

  @note
    In case of delete table it's only safe to use the following parts of
    the 'table' structure:
    - table->s->path
    - table->alias
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
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      textno=ER_FILE_NOT_FOUND;
      my_error(textno, errflag, table_share->table_name.str,
               error, my_strerror(errbuf, sizeof(errbuf), error));
    }
    break;
  case HA_ERR_KEY_NOT_FOUND:
  case HA_ERR_NO_ACTIVE_RECORD:
  case HA_ERR_RECORD_DELETED:
  case HA_ERR_END_OF_FILE:
    textno=ER_KEY_NOT_FOUND;
    break;
  case HA_ERR_WRONG_MRG_TABLE_DEF:
    textno=ER_WRONG_MRG_TABLE;
    break;
  case HA_ERR_FOUND_DUPP_KEY:
  {
    uint key_nr= table ? get_dup_key(error) : -1;
    if ((int) key_nr >= 0)
    {
      print_keydup_error(table,
                         key_nr == MAX_KEY ? NULL : &table->key_info[key_nr],
                         errflag);
      DBUG_VOID_RETURN;
    }
    textno=ER_DUP_KEY;
    break;
  }
  case HA_ERR_FOREIGN_DUPLICATE_KEY:
  {
    DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
                m_lock_type != F_UNLCK);

    char rec_buf[MAX_KEY_LENGTH];
    String rec(rec_buf, sizeof(rec_buf), system_charset_info);
    /* Table is opened and defined at this point */

    /*
      Just print the subset of fields that are part of the first index,
      printing the whole row from there is not easy.
    */
    key_unpack(&rec, table, &table->key_info[0]);

    char child_table_name[NAME_LEN + 1];
    char child_key_name[NAME_LEN + 1];
    if (get_foreign_dup_key(child_table_name, sizeof(child_table_name),
                            child_key_name, sizeof(child_key_name)))
    {
      my_error(ER_FOREIGN_DUPLICATE_KEY_WITH_CHILD_INFO, errflag,
               table_share->table_name.str, rec.c_ptr_safe(),
               child_table_name, child_key_name);
    }
    else
    {
      my_error(ER_FOREIGN_DUPLICATE_KEY_WITHOUT_CHILD_INFO, errflag,
               table_share->table_name.str, rec.c_ptr_safe());
    }
    DBUG_VOID_RETURN;
  }
  case HA_ERR_NULL_IN_SPATIAL:
    my_error(ER_CANT_CREATE_GEOMETRY_OBJECT, errflag);
    DBUG_VOID_RETURN;
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
  case HA_ERR_SE_OUT_OF_MEMORY:
    my_error(ER_ENGINE_OUT_OF_MEMORY, errflag,
             table->file->table_type());
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
  case HA_ERR_INDEX_FILE_FULL:
  {
    textno=ER_RECORD_FILE_FULL;
    /* Write the error message to error log */
    errflag|= ME_ERRORLOG;
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
    uint key_nr= table ? get_dup_key(error) : -1;
    if ((int) key_nr >= 0 && key_nr != MAX_KEY)
      ptr= table->key_info[key_nr].name;
    my_error(ER_DROP_INDEX_FK, errflag, ptr);
    DBUG_VOID_RETURN;
  }
  case HA_ERR_TABLE_NEEDS_UPGRADE:
    textno=ER_TABLE_NEEDS_UPGRADE;
    break;
  case HA_ERR_NO_PARTITION_FOUND:
    textno=ER_WRONG_PARTITION_NAME;
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
  case HA_ERR_INDEX_COL_TOO_LONG:
    textno= ER_INDEX_COLUMN_TOO_LONG;
    break;
  case HA_ERR_NOT_IN_LOCK_PARTITIONS:
    textno=ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET;
    break;
  case HA_ERR_INDEX_CORRUPT:
    textno= ER_INDEX_CORRUPT;
    break;
  case HA_ERR_UNDO_REC_TOO_BIG:
    textno= ER_UNDO_RECORD_TOO_BIG;
    break;
  case HA_ERR_TABLE_IN_FK_CHECK:
    textno= ER_TABLE_IN_FK_CHECK;
    break;
  case HA_WRONG_CREATE_OPTION:
    textno= ER_ILLEGAL_HA;
    break;
  case HA_MISSING_CREATE_OPTION:
  {
    const char* engine= table_type();
    my_error(ER_MISSING_HA_CREATE_OPTION, errflag, engine);
    DBUG_VOID_RETURN;
  }
  case HA_ERR_TOO_MANY_FIELDS:
    textno= ER_TOO_MANY_FIELDS;
    break;
  case HA_ERR_INNODB_READ_ONLY:
    textno= ER_INNODB_READ_ONLY;
    break;
  case HA_ERR_TEMP_FILE_WRITE_FAILURE:
    textno= ER_TEMP_FILE_WRITE_FAILURE;
    break;
  case HA_ERR_INNODB_FORCED_RECOVERY:
    textno= ER_INNODB_FORCED_RECOVERY;
    break;
  case HA_ERR_TABLE_CORRUPT:
    my_error(ER_TABLE_CORRUPT, errflag, table_share->db.str,
             table_share->table_name.str);
    DBUG_VOID_RETURN;
  case HA_ERR_QUERY_INTERRUPTED:
    textno= ER_QUERY_INTERRUPTED;
    break;
  case HA_ERR_TABLESPACE_MISSING:
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_snprintf(errbuf, MYSYS_STRERROR_SIZE, "`%s`.`%s`", table_share->db.str,
    table_share->table_name.str);
    my_error(ER_TABLESPACE_MISSING, errflag, errbuf, error);
    DBUG_VOID_RETURN;
  }
  case HA_ERR_TABLESPACE_IS_NOT_EMPTY:
    my_error(ER_TABLESPACE_IS_NOT_EMPTY, errflag, table_share->db.str,
             table_share->table_name.str);
    DBUG_VOID_RETURN;
  case HA_ERR_WRONG_FILE_NAME:
    my_error(ER_WRONG_FILE_NAME, errflag, table_share->table_name.str);
    DBUG_VOID_RETURN;
  case HA_ERR_NOT_ALLOWED_COMMAND:
    textno=ER_NOT_ALLOWED_COMMAND;
    break;
  default:
    {
      /* The error was "unknown" to this function.
	 Ask handler if it has got a message for this error */
      String str;
      bool temporary= get_error_message(error, &str);
      if (!str.is_empty())
      {
	const char* engine= table_type();
	if (temporary)
	  my_error(ER_GET_TEMPORARY_ERRMSG, errflag, error, str.ptr(), engine);
	else
	  my_error(ER_GET_ERRMSG, errflag, error, str.ptr(), engine);
      }
      else
	my_error(ER_GET_ERRNO,errflag,error);
      DBUG_VOID_RETURN;
    }
  }
  if (textno != ER_FILE_NOT_FOUND)
    my_error(textno, errflag, table_share->table_name.str, error);
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
      KEY_PART_INFO *key_part_end= key_part + key->user_defined_key_parts;
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
      keypartend= keypart + keyinfo->user_defined_key_parts;
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

  for (field= table->field; (*field); field++)
  {
    if (table->s->mysql_version == 0) // prior to MySQL 5.0
    {
      /* check for bad DECIMAL field */
      if ((*field)->type() == MYSQL_TYPE_NEWDECIMAL)
      {
        return HA_ADMIN_NEEDS_ALTER;
      }
      if ((*field)->type() == MYSQL_TYPE_VAR_STRING)
      {
        return HA_ADMIN_NEEDS_ALTER;
      }
    }

    /*
      Check for old DECIMAL field.

      Above check does not take into account for pre 5.0 decimal types which can
      be present in the data directory if user did in-place upgrade from
      mysql-4.1 to mysql-5.0.
    */
    if ((*field)->type() == MYSQL_TYPE_DECIMAL)
    {
      return HA_ADMIN_NEEDS_DUMP_UPGRADE;
    }

    if ((*field)->type() == MYSQL_TYPE_YEAR && (*field)->field_length == 2)
      return HA_ADMIN_NEEDS_ALTER; // obsolete YEAR(2) type

    //Check for old temporal format if avoid_temporal_upgrade is disabled.
    mysql_mutex_lock(&LOCK_global_system_variables);
    bool check_temporal_upgrade= !avoid_temporal_upgrade;
    mysql_mutex_unlock(&LOCK_global_system_variables);

    if (check_temporal_upgrade)
    {
      if (((*field)->real_type() == MYSQL_TYPE_TIME) ||
          ((*field)->real_type() == MYSQL_TYPE_DATETIME) ||
          ((*field)->real_type() == MYSQL_TYPE_TIMESTAMP))
        return HA_ADMIN_NEEDS_ALTER;
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

  if ((file= mysql_file_open(key_file_frm,
                             path, O_RDWR|O_BINARY, MYF(MY_WME))) >= 0)
  {
    uchar version[4];

    int4store(version, MYSQL_VERSION_ID);

    if ((result= mysql_file_pwrite(file, (uchar*) version, 4, 51L, MYF_RW)))
      goto err;

    table->s->mysql_version= MYSQL_VERSION_ID;
  }
err:
  if (file >= 0)
    (void) mysql_file_close(file, MYF(MY_WME));
  DBUG_RETURN(result);
}



/**
  @return
    key if error because of duplicated keys
*/
uint handler::get_dup_key(int error)
{
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  DBUG_ENTER("handler::get_dup_key");
  table->file->errkey  = (uint) -1;
  if (error == HA_ERR_FOUND_DUPP_KEY ||
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
  DBUG_ASSERT(m_lock_type == F_UNLCK);

  for (const char **ext=bas_ext(); *ext ; ext++)
  {
    fn_format(buff, name, "", *ext, MY_UNPACK_FILENAME|MY_APPEND_EXT);
    if (mysql_file_delete_with_symlink(key_file_misc, buff, MYF(0)))
    {
      if (my_errno() != ENOENT)
      {
        /*
          If error on the first existing file, return the error.
          Otherwise delete as much as possible.
        */
        if (enoent_or_zero)
          return my_errno();
	saved_error= my_errno();
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
      error= my_errno();
      if (error != ENOENT)
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
  close();
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
  bool skip_version_update = false;
  bool is_upgrade = check_opt->sql_flags & TT_FOR_UPGRADE;

  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);

  if ((table->s->mysql_version >= MYSQL_VERSION_ID) &&
      (check_opt->sql_flags & TT_FOR_UPGRADE))
    return 0;

  if (table->s->mysql_version < MYSQL_VERSION_ID)
  {
    if ((error= check_old_types()))
      return error;

    error= ha_check_for_upgrade(check_opt);
    switch (error)
    {
      case HA_ADMIN_NEEDS_UPG_PART:
        /* Skip version update as the table needs upgrade. */
        skip_version_update= true;
        /* Fall through */
      case HA_ADMIN_OK:
        if (is_upgrade)
          return error;
        /* Fall through */
      case HA_ADMIN_NEEDS_CHECK:
        break;
      default:
        return error;
    }
  }

  if ((error= check(thd, check_opt)))
    return error;
  /* Skip updating frm version if not main handler. */
  if (table->file != this || skip_version_update)
    return error;
  return update_frm_version(table);
}

/**
  A helper function to mark a transaction read-write,
  if it is started.
*/

void
handler::mark_trx_read_write()
{
  Ha_trx_info *ha_info= &ha_thd()->ha_data[ht->slot].ha_info[0];
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

  result= repair(thd, check_opt);
  DBUG_ASSERT(result == HA_ADMIN_NOT_IMPLEMENTED ||
              ha_table_flags() & HA_CAN_REPAIR);

  int old_types_error= check_old_types();

  if (old_types_error != HA_ADMIN_NEEDS_DUMP_UPGRADE && result == HA_ADMIN_OK)
    result= update_frm_version(table);
  return result;
}


/**
  Start bulk insert.

  Allow the handler to optimize for multiple row insert.

  @param rows  Estimated rows to insert
*/

void handler::ha_start_bulk_insert(ha_rows rows)
{
  DBUG_ENTER("handler::ha_start_bulk_insert");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);
  estimation_rows_to_insert= rows;
  start_bulk_insert(rows);
  DBUG_VOID_RETURN;
}


/**
  End bulk insert.

  @return Operation status
    @retval 0     Success
    @retval != 0  Failure (error code returned)
*/

int handler::ha_end_bulk_insert()
{
  DBUG_ENTER("handler::ha_end_bulk_insert");
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);
  estimation_rows_to_insert= 0;
  DBUG_RETURN(end_bulk_insert());
}


/**
  Bulk update row: public interface.

  @sa handler::bulk_update_row()
*/

int
handler::ha_bulk_update_row(const uchar *old_data, uchar *new_data,
                            uint *dup_key_found)
{
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);
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
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);
  mark_trx_read_write();

  return delete_all_rows();
}


/**
  Truncate table: public interface.

  @sa handler::truncate()
*/

int
handler::ha_truncate()
{
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);
  mark_trx_read_write();

  return truncate();
}


/**
  Optimize table: public interface.

  @sa handler::optimize()
*/

int
handler::ha_optimize(THD* thd, HA_CHECK_OPT* check_opt)
{
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);
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
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
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
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_UNLCK);
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
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
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
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
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
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);
  mark_trx_read_write();

  return discard_or_import_tablespace(discard);
}


bool handler::ha_prepare_inplace_alter_table(TABLE *altered_table,
                                             Alter_inplace_info *ha_alter_info)
{
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type != F_UNLCK);
  mark_trx_read_write();

  return prepare_inplace_alter_table(altered_table, ha_alter_info);
}


bool handler::ha_commit_inplace_alter_table(TABLE *altered_table,
                                            Alter_inplace_info *ha_alter_info,
                                            bool commit)
{
   /*
     At this point we should have an exclusive metadata lock on the table.
     The exception is if we're about to roll back changes (commit= false).
     In this case, we might be rolling back after a failed lock upgrade,
     so we could be holding the same lock level as for inplace_alter_table().
   */
   DBUG_ASSERT(ha_thd()->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                       table->s->db.str,
                                       table->s->table_name.str,
                                       MDL_EXCLUSIVE) ||
               !commit);

   return commit_inplace_alter_table(altered_table, ha_alter_info, commit);
}


/*
   Default implementation to support in-place alter table
   and old online add/drop index API
*/

enum_alter_inplace_result
handler::check_if_supported_inplace_alter(TABLE *altered_table,
                                          Alter_inplace_info *ha_alter_info)
{
  DBUG_ENTER("check_if_supported_alter");

  HA_CREATE_INFO *create_info= ha_alter_info->create_info;

  Alter_inplace_info::HA_ALTER_FLAGS inplace_offline_operations=
    Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH |
    Alter_inplace_info::ALTER_COLUMN_NAME |
    Alter_inplace_info::ALTER_COLUMN_DEFAULT |
    Alter_inplace_info::CHANGE_CREATE_OPTION |
    Alter_inplace_info::ALTER_RENAME |
    Alter_inplace_info::RENAME_INDEX |
    Alter_inplace_info::ALTER_INDEX_COMMENT |
    Alter_inplace_info::ALTER_COLUMN_INDEX_LENGTH;

  /* Is there at least one operation that requires copy algorithm? */
  if (ha_alter_info->handler_flags & ~inplace_offline_operations)
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);

  /*
    ALTER TABLE tbl_name CONVERT TO CHARACTER SET .. and
    ALTER TABLE table_name DEFAULT CHARSET = .. most likely
    change column charsets and so not supported in-place through
    old API.

    Changing of PACK_KEYS, MAX_ROWS and ROW_FORMAT options were
    not supported as in-place operations in old API either.
  */
  if (create_info->used_fields & (HA_CREATE_USED_CHARSET |
                                  HA_CREATE_USED_DEFAULT_CHARSET |
                                  HA_CREATE_USED_PACK_KEYS |
                                  HA_CREATE_USED_MAX_ROWS) ||
      (table->s->row_type != create_info->row_type))
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);

  uint table_changes= (ha_alter_info->handler_flags &
                       Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH) ?
    IS_EQUAL_PACK_LENGTH : IS_EQUAL_YES;
  if (table->file->check_if_incompatible_data(create_info, table_changes)
      == COMPATIBLE_DATA_YES)
    DBUG_RETURN(HA_ALTER_INPLACE_EXCLUSIVE_LOCK);

  DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
}


/*
   Default implementation to support in-place alter table
   and old online add/drop index API
*/

void handler::notify_table_changed()
{
  ha_create_handler_files(table->s->path.str, NULL, CHF_INDEX_FLAG, NULL);
}


void Alter_inplace_info::report_unsupported_error(const char *not_supported,
                                                  const char *try_instead)
{
  if (unsupported_reason == NULL)
    my_error(ER_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
             not_supported, try_instead);
  else
    my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0),
             not_supported, unsupported_reason, try_instead);
}


/**
  Rename table: public interface.

  @sa handler::rename_table()
*/

int
handler::ha_rename_table(const char *from, const char *to)
{
  DBUG_ASSERT(m_lock_type == F_UNLCK);
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
  DBUG_ASSERT(m_lock_type == F_UNLCK);
  mark_trx_read_write();

  return delete_table(name);
}


/**
  Drop table in the engine: public interface.

  @sa handler::drop_table()
*/

void
handler::ha_drop_table(const char *name)
{
  DBUG_ASSERT(m_lock_type == F_UNLCK);
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
  DBUG_ASSERT(m_lock_type == F_UNLCK);
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
  /*
    Normally this is done when unlocked, but in fast_alter_partition_table,
    it is done on an already locked handler when preparing to alter/rename
    partitions.
  */
  DBUG_ASSERT(m_lock_type == F_UNLCK ||
              (!old_name && strcmp(name, table_share->path.str)));
  mark_trx_read_write();

  return create_handler_files(name, old_name, action_flag, info);
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

  if ((thd->get_transaction()->m_flags.enabled= on))
  {
    /*
      Now all storage engines should have transaction handling enabled.
      But some may have it enabled all the time - "disabling" transactions
      is an optimization hint that storage engine is free to ignore.
      So, let's commit an open transaction (if any) now.
    */
    if (!(error= ha_commit_trans(thd, 0)))
      error= trans_commit_implicit(thd);
  }
  DBUG_RETURN(error);
}

int handler::index_next_same(uchar *buf, const uchar *key, uint keylen)
{
  int error;
  DBUG_ENTER("index_next_same");
  if (!(error=index_next(buf)))
  {
    my_ptrdiff_t ptrdiff= buf - table->record[0];
    uchar *save_record_0= NULL;
    KEY *key_info= NULL;
    KEY_PART_INFO *key_part= NULL;
    KEY_PART_INFO *key_part_end= NULL;

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
      key_part_end= key_part + key_info->user_defined_key_parts;
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
  DBUG_RETURN(error);
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
                    bool update_create_info,
                    bool is_temp_table)
{
  int error= 1;
  TABLE table;
  char name_buff[FN_REFLEN];
  const char *name;
  TABLE_SHARE share;
#ifdef HAVE_PSI_TABLE_INTERFACE
  bool temp_table = is_temp_table ||
    (create_info->options & HA_LEX_CREATE_TMP_TABLE) ||
    (strstr(path, tmp_file_prefix) != NULL);
#endif
  DBUG_ENTER("ha_create_table");
  
  init_tmp_table_share(thd, &share, db, 0, table_name, path);
  if (open_table_def(thd, &share, 0))
    goto err;

#ifdef HAVE_PSI_TABLE_INTERFACE
  share.m_psi= PSI_TABLE_CALL(get_table_share)(temp_table, &share);
#endif

  if (open_table_from_share(thd, &share, "", 0, (uint) READ_ALL, 0, &table,
                            TRUE))
  {
#ifdef HAVE_PSI_TABLE_INTERFACE
    PSI_TABLE_CALL(drop_table_share)
      (temp_table, db, strlen(db), table_name, strlen(table_name));
#endif
    goto err;
  }

  if (update_create_info)
    update_create_info_from_table(create_info, &table);

  name= get_canonical_filename(table.file, share.path.str, name_buff);

  error= table.file->ha_create(name, &table, create_info);
  if (error)
  {
    table.file->print_error(error, MYF(0));
#ifdef HAVE_PSI_TABLE_INTERFACE
    PSI_TABLE_CALL(drop_table_share)
      (temp_table, db, strlen(db), table_name, strlen(table_name));
#endif
  }
  (void) closefrm(&table, 0);
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
  my_free(frmblob);
  if (error)
    DBUG_RETURN(2);

  init_tmp_table_share(thd, &share, db, 0, name, path);
  if (open_table_def(thd, &share, 0))
  {
    DBUG_RETURN(3);
  }

#ifdef HAVE_PSI_TABLE_INTERFACE
  /*
    Table discovery is not instrumented.
    Once discovered, the table will be opened normally,
    and instrumented normally.
  */
#endif

  if (open_table_from_share(thd, &share, "" ,0, 0, 0, &table, FALSE))
  {
    free_table_share(&share);
    DBUG_RETURN(3);
  }

  update_create_info_from_table(&create_info, &table);
  create_info.table_options|= HA_OPTION_CREATE_FROM_ENGINE;

  get_canonical_filename(table.file, path, path);
  error=table.file->ha_create(path, &table, &create_info);
  (void) closefrm(&table, 1);

  DBUG_RETURN(error != 0);
}


/**
  Try to find a table in a storage engine. 

  @param db   Normalized table schema name
  @param name Normalized table name.
  @param[out] exists Only valid if the function succeeded.

  @retval TRUE   An error is found
  @retval FALSE  Success, check *exists
*/

bool
ha_check_if_table_exists(THD* thd, const char *db, const char *name,
                         bool *exists)
{
  uchar *frmblob= NULL;
  size_t frmlen;
  DBUG_ENTER("ha_check_if_table_exists");

  *exists= ! ha_discover(thd, db, name, &frmblob, &frmlen);
  if (*exists)
    my_free(frmblob);

  DBUG_RETURN(FALSE);
}


/**
  @brief Check if a given table is a user table or a valid system table or
         a valid system table that a SE supports.

  @param   hton                  Handlerton of new engine.
  @param   db                    Database name.
  @param   table_name            Table name to be checked.

  @retval  st_sys_tbl_chk_params::enum_status
*/
static st_sys_tbl_chk_params::enum_status
ha_get_system_table_check_status(handlerton *hton, const char *db,
                                   const char *table_name)
{
  DBUG_ENTER("ha_get_system_table_check_status");
  st_sys_tbl_chk_params check_params;
  check_params.status= st_sys_tbl_chk_params::USER_TABLE;
  bool is_system_database= false;
  const char **names;
  st_handler_tablename *systab;

  // Check if we have a system database name in the command.
  DBUG_ASSERT(known_system_databases != NULL);
  names= known_system_databases;
  while (names && *names)
  {
    if (strcmp(*names, db) == 0)
    {
      /* Used to compare later, will be faster */
      check_params.db= *names;
      is_system_database= true;
      break;
    }
    names++;
  }
  if (!is_system_database)
    DBUG_RETURN(st_sys_tbl_chk_params::USER_TABLE);

  // Check if this is SQL layer system tables.
  systab= mysqld_system_tables;
  check_params.is_sql_layer_system_table= false;
  while (systab && systab->db)
  {
    if (systab->db == check_params.db &&
        strcmp(systab->tablename, table_name) == 0)
    {
      check_params.is_sql_layer_system_table= true;
      break;
    }
    systab++;
  }

  // Check if this is a system table and if some engine supports it.
  check_params.status= check_params.is_sql_layer_system_table ?
    st_sys_tbl_chk_params::SYSTEM_TABLE :
    st_sys_tbl_chk_params::USER_TABLE;
  check_params.db_type= hton->db_type;
  check_params.table_name= table_name;
  plugin_foreach(NULL, check_engine_system_table_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &check_params);

  DBUG_RETURN(check_params.status);
}


/**
  @brief Check if a given table is a system table supported by a SE.

  @todo There is another function called is_system_table_name() used by
        get_table_category(), which is used to set TABLE_SHARE table_category.
        It checks only a subset of table name like proc, event and time*.
        We cannot use below function in get_table_category(),
        as that affects locking mechanism. If we need to
        unify these functions, we need to fix locking issues generated.

  @param   hton                  Handlerton of new engine.
  @param   db                    Database name.
  @param   table_name            Table name to be checked.

  @return Operation status
    @retval  true                If the table name is a valid system table
                                 that is supported by a SE.

    @retval  false               Not a system table.
*/
bool ha_is_supported_system_table(handlerton *hton, const char *db,
                                  const char *table_name)
{
  DBUG_ENTER("ha_is_supported_system_table");
  st_sys_tbl_chk_params::enum_status status=
    ha_get_system_table_check_status(hton, db, table_name);

  // It's a valid SE supported system table.
  DBUG_RETURN(status == st_sys_tbl_chk_params::SE_SUPPORTED_SYSTEM_TABLE);
}


/**
  @brief Check if a given table is a system table that belongs
  to some SE or a user table.

  @details The primary purpose of introducing this function is to stop system
  tables to be created or being moved to undesired storage engines.

  @todo There is another function called is_system_table_name() used by
        get_table_category(), which is used to set TABLE_SHARE table_category.
        It checks only a subset of table name like proc, event and time*.
        We cannot use below function in get_table_category(),
        as that affects locking mechanism. If we need to
        unify these functions, we need to fix locking issues generated.

  @param   hton                  Handlerton of new engine.
  @param   db                    Database name.
  @param   table_name            Table name to be checked.

  @return Operation status
    @retval  true                If the table name is a valid system table
                                 or if its a valid user table.

    @retval  false               If the table name is a system table name
                                 and does not belong to engine specified
                                 in the command.
*/
bool ha_is_valid_system_or_user_table(handlerton *hton, const char *db,
                                      const char *table_name)
{
  DBUG_ENTER("ha_is_valid_system_or_user_table");

  st_sys_tbl_chk_params::enum_status status=
    ha_get_system_table_check_status(hton, db, table_name);

  // It's a user table or a valid SE supported system table.
  DBUG_RETURN(status == st_sys_tbl_chk_params::USER_TABLE ||
              status == st_sys_tbl_chk_params::SE_SUPPORTED_SYSTEM_TABLE);
}


/**
  @brief Called for each SE to check if given db, tablename is a system table.

  @details The primary purpose of introducing this function is to stop system
  tables to be created or being moved to undesired storage engines.

  @param   unused  unused THD*
  @param   plugin  Points to specific SE.
  @param   arg     Is of type struct st_sys_tbl_chk_params.

  @note
    args->status   Indicates OUT param,
                   see struct st_sys_tbl_chk_params definition for more info.

  @return Operation status
    @retval  true  There was a match found.
                   This will stop doing checks with other SE's.

    @retval  false There was no match found.
                   Other SE's will be checked to find a match.
*/
static my_bool check_engine_system_table_handlerton(THD *unused,
                                                    plugin_ref plugin,
                                                    void *arg)
{
  st_sys_tbl_chk_params *check_params= (st_sys_tbl_chk_params*) arg;
  handlerton *hton= plugin_data<handlerton*>(plugin);

  // Do we already know that the table is a system table?
  if (check_params->status == st_sys_tbl_chk_params::SYSTEM_TABLE)
  {
    /*
      If this is the same SE specified in the command, we can
      simply ask the SE if it supports it stop the search regardless.
    */
    if (hton->db_type == check_params->db_type)
    {
      if (hton->is_supported_system_table &&
          hton->is_supported_system_table(check_params->db,
                                       check_params->table_name,
                                       check_params->is_sql_layer_system_table))
        check_params->status=
          st_sys_tbl_chk_params::SE_SUPPORTED_SYSTEM_TABLE;
      return TRUE;
    }
    /*
      If this is a different SE, there is no point in asking the SE
      since we already know it's a system table and we don't care
      if it is supported or not.
    */
    return FALSE;
  }

  /*
    We don't yet know if the table is a system table or not.
    We therefore must always ask the SE.
  */
  if (hton->is_supported_system_table &&
      hton->is_supported_system_table(check_params->db,
                                      check_params->table_name,
                                      check_params->is_sql_layer_system_table))
  {
    /*
      If this is the same SE specified in the command, we know it's a
      supported system table and can stop the search.
    */
    if (hton->db_type == check_params->db_type)
    {
      check_params->status= st_sys_tbl_chk_params::SE_SUPPORTED_SYSTEM_TABLE;
      return TRUE;
    }
    else
      check_params->status= st_sys_tbl_chk_params::SYSTEM_TABLE;
  }

  return FALSE;
}

/*
  Prepare list of all known system database names
  current we just have 'mysql' as system database name.

  Later ndbcluster, innodb SE's can define some new database
  name which can store system tables specific to SE.
*/
const char** ha_known_system_databases(void)
{
  list<const char*> found_databases;
  const char **databases, **database;

  // Get mysqld system database name.
  found_databases.push_back((char*) mysqld_system_database);

  // Get system database names from every specific storage engine.
  plugin_foreach(NULL, system_databases_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &found_databases);

  databases= (const char **) my_once_alloc(sizeof(char *)*
                                     (found_databases.size()+1),
                                     MYF(MY_WME | MY_FAE));
  DBUG_ASSERT(databases != NULL);

  list<const char*>::iterator it;
  database= databases;
  for (it= found_databases.begin(); it != found_databases.end(); it++)
    *database++= *it;
  *database= 0; // Last element.

  return databases;
}

/**
  @brief Fetch system database name specific to SE.

  @details This function is invoked by plugin_foreach() from
           ha_known_system_databases(), for each storage engine.
*/
static my_bool system_databases_handlerton(THD *unused, plugin_ref plugin,
                                           void *arg)
{
  list<const char*> *found_databases= (list<const char*> *) arg;
  const char *db;

  handlerton *hton= plugin_data<handlerton*>(plugin);
  if (hton->system_database)
  {
    db= hton->system_database();
    if (db)
      found_databases->push_back(db);
  }

  return FALSE;
}

void st_ha_check_opt::init()
{
  flags= sql_flags= 0;
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
    mysql_mutex_lock(&LOCK_global_system_variables);
    size_t tmp_buff_size= (size_t) key_cache->param_buff_size;
    ulonglong tmp_block_size= key_cache->param_block_size;
    ulonglong division_limit= key_cache->param_division_limit;
    ulonglong age_threshold=  key_cache->param_age_threshold;
    mysql_mutex_unlock(&LOCK_global_system_variables);
    DBUG_RETURN(!init_key_cache(key_cache,
				tmp_block_size,
				tmp_buff_size,
				division_limit, age_threshold));
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
    mysql_mutex_lock(&LOCK_global_system_variables);
    size_t tmp_buff_size= (size_t) key_cache->param_buff_size;
    ulonglong tmp_block_size= key_cache->param_block_size;
    ulonglong division_limit= key_cache->param_division_limit;
    ulonglong age_threshold=  key_cache->param_age_threshold;
    mysql_mutex_unlock(&LOCK_global_system_variables);
    const int retval= resize_key_cache(key_cache,
                                       keycache_thread_var(),
                                       tmp_block_size,
                                       tmp_buff_size,
                                       division_limit, age_threshold);
    DBUG_RETURN(!retval);
  }
  DBUG_RETURN(0);
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
  handlerton *hton= plugin_data<handlerton*>(plugin);
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
    thd->status_var.ha_discover_count++;
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
  handlerton *hton= plugin_data<handlerton*>(plugin);


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
		       db, path, wild ? wild : "NULL", dir));
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
  handlerton *hton= plugin_data<handlerton*>(plugin);

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

/**
  Prepare (sub-) sequences of joins in this statement 
  which may be pushed to each storage engine for execution.
*/
struct st_make_pushed_join_args
{
  const AQP::Join_plan* plan; // Query plan provided by optimizer
  int err;                    // Error code to return.
};

static my_bool make_pushed_join_handlerton(THD *thd, plugin_ref plugin,
                                   void *arg)
{
  st_make_pushed_join_args *vargs= (st_make_pushed_join_args *)arg;
  handlerton *hton= plugin_data<handlerton*>(plugin);

  if (hton && hton->make_pushed_join)
  {
    const int error= hton->make_pushed_join(hton, thd, vargs->plan);
    if (unlikely(error))
    {
      vargs->err = error;
      return TRUE;
    }
  }
  return FALSE;
}

int ha_make_pushed_joins(THD *thd, const AQP::Join_plan* plan)
{
  DBUG_ENTER("ha_make_pushed_joins");
  st_make_pushed_join_args args= {plan, 0};
  plugin_foreach(thd, make_pushed_join_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &args);
  DBUG_PRINT("exit", ("error: %d", args.err));
  DBUG_RETURN(args.err);
}

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
  handlerton *hton= plugin_data<handlerton*>(plugin);
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
  size_t query_length;
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
  return binlog_log_query_handlerton2(thd,
                                      plugin_data<handlerton*>(plugin), args);
}

void ha_binlog_log_query(THD *thd, handlerton *hton,
                         enum_binlog_command binlog_command,
                         const char *query, size_t query_length,
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

int ha_binlog_end(THD* thd)
{
  binlog_func_st bfn= {BFN_BINLOG_END, 0};
  binlog_func_foreach(thd, &bfn);
  return 0;
}

/**
  Calculate cost of 'index only' scan for given index and number of records

  @param keynr    Index number
  @param records  Estimated number of records to be retrieved

  @note
    It is assumed that we will read trough the whole key range and that all
    key blocks are half full (normally things are much better). It is also
    assumed that each time we read the next key from the index, the handler
    performs a random seek, thus the cost is proportional to the number of
    blocks read.

  @return
    Estimated cost of 'index only' scan
*/

double handler::index_only_read_time(uint keynr, double records)
{
  double read_time;
  uint keys_per_block= (stats.block_size/2/
                        (table_share->key_info[keynr].key_length + ref_length) +
                        1);
  read_time=((double) (records + keys_per_block-1) /
             (double) keys_per_block);
  return read_time;
}


double handler::table_in_memory_estimate() const
{
  DBUG_ASSERT(stats.table_in_mem_estimate == IN_MEMORY_ESTIMATE_UNKNOWN ||
              (stats.table_in_mem_estimate >= 0.0 &&
               stats.table_in_mem_estimate <= 1.0));

  /*
    If the storage engine has supplied information about how much of the
    table that is currently in a memory buffer, then use this estimate.
  */
  if (stats.table_in_mem_estimate != IN_MEMORY_ESTIMATE_UNKNOWN)
    return stats.table_in_mem_estimate;

  /*
    The storage engine has not provided any information about how much of
    this index is in memory, use an heuristic to produce an estimate.
  */
  return estimate_in_memory_buffer(stats.data_file_length);
}


double handler::index_in_memory_estimate(uint keyno) const
{
  const KEY *key= &table->key_info[keyno];

  /*
    If the storage engine has supplied information about how much of the
    index that is currently in a memory buffer, then use this estimate.
  */
  const double est= key->in_memory_estimate();
  if (est != IN_MEMORY_ESTIMATE_UNKNOWN)
    return est;

  /*
    The storage engine has not provided any information about how much of
    this index is in memory, use an heuristic to produce an estimate.
  */
  ulonglong file_length;

  /*
    If the index is a clustered primary index, then use the data file
    size as estimate for how large the index is.
  */
  if (keyno == table->s->primary_key && primary_key_is_clustered())
    file_length= stats.data_file_length;
  else
    file_length= stats.index_file_length;

  return estimate_in_memory_buffer(file_length);
}


double handler::estimate_in_memory_buffer(ulonglong table_index_size) const
{
  /*
    The storage engine has not provided any information about how much of
    the table/index is in memory. In this case we use a heuristic:

    - if the size of the table/index is less than 20 percent (pick any
      number) of the memory buffer, then the entire table/index is likely in
      memory.
    - if the size of the table/index is larger than the memory buffer, then
      assume nothing of the table/index is in memory.
    - if the size of the table/index is larger than 20 percent but less than
      the memory buffer size, then use a linear function of the table/index
      size that goes from 1.0 to 0.0.
  */

  /*
    If the storage engine has information about the size of its
    memory buffer, then use this. Otherwise, assume that at least 100 MB
    of data can be chached in memory.
  */
  longlong memory_buf_size= get_memory_buffer_size();
  if (memory_buf_size <= 0)
    memory_buf_size= 100 * 1024 * 1024;    // 100 MB

  /*
    Upper limit for the relative size of a table to be considered
    entirely available in a memory buffer. If the actual table size is
    less than this we assume it is complete cached in a memory buffer.
  */
  const double table_index_in_memory_limit= 0.2;

  /*
    Estimate for how much of the total memory buffer this table/index
    can occupy.
  */
  const double percent_of_mem= static_cast<double>(table_index_size) /
    memory_buf_size;

  double in_mem_est;

  if (percent_of_mem < table_index_in_memory_limit) // Less than 20 percent
    in_mem_est= 1.0;
  else if (percent_of_mem > 1.0)                // Larger than buffer
    in_mem_est= 0.0;
  else
  {
    /*
      The size of the table/index is larger than
      "table_index_in_memory_limit" * "memory_buf_size" but less than
      the total size of the memory buffer.
    */
    in_mem_est= 1.0 - (percent_of_mem - table_index_in_memory_limit) /
      (1.0 - table_index_in_memory_limit);
  }
  DBUG_ASSERT(in_mem_est >= 0.0 && in_mem_est <= 1.0);

  return in_mem_est;
}


Cost_estimate handler::table_scan_cost()
{
  /*
    This function returns a Cost_estimate object. The function should be
    implemented in a way that allows the compiler to use "return value
    optimization" to avoid creating the temporary object for the return value
    and use of the copy constructor.
  */

  const double io_cost= scan_time() * table->cost_model()->page_read_cost(1.0);
  Cost_estimate cost;
  cost.add_io(io_cost);
  return cost;
}

  
Cost_estimate handler::index_scan_cost(uint index, double ranges, double rows)
{
  /*
    This function returns a Cost_estimate object. The function should be
    implemented in a way that allows the compiler to use "return value
    optimization" to avoid creating the temporary object for the return value
    and use of the copy constructor.
  */

  DBUG_ASSERT(ranges >= 0.0);
  DBUG_ASSERT(rows >= 0.0);

  const double io_cost= index_only_read_time(index, rows) *
    table->cost_model()->page_read_cost_index(index, 1.0);
  Cost_estimate cost;
  cost.add_io(io_cost);
  return cost;
}


Cost_estimate handler::read_cost(uint index, double ranges, double rows)
{
  /*
    This function returns a Cost_estimate object. The function should be
    implemented in a way that allows the compiler to use "return value
    optimization" to avoid creating the temporary object for the return value
    and use of the copy constructor.
  */

  DBUG_ASSERT(ranges >= 0.0);
  DBUG_ASSERT(rows >= 0.0);

  const double io_cost= read_time(index, static_cast<uint>(ranges),
                                  static_cast<ha_rows>(rows)) *
                        table->cost_model()->page_read_cost(1.0);
  Cost_estimate cost;
  cost.add_io(io_cost);
  return cost;
}
  

/**
  Check if key has partially-covered columns

  We can't use DS-MRR to perform range scans when the ranges are over
  partially-covered keys, because we'll not have full key part values
  (we'll have their prefixes from the index) and will not be able to check
  if we've reached the end the range.

  @param keyno  Key to check

  @todo
    Allow use of DS-MRR in cases where the index has partially-covered
    components but they are not used for scanning.

  @retval TRUE   Yes
  @retval FALSE  No
*/

bool key_uses_partial_cols(TABLE *table, uint keyno)
{
  KEY_PART_INFO *kp= table->key_info[keyno].key_part;
  KEY_PART_INFO *kp_end= kp + table->key_info[keyno].user_defined_key_parts;
  for (; kp != kp_end; kp++)
  {
    if (!kp->field->part_of_key.is_set(keyno))
      return TRUE;
  }
  return FALSE;
}

/****************************************************************************
 * Default MRR implementation (MRR to non-MRR converter)
 ***************************************************************************/

/**
  Get cost and other information about MRR scan over a known list of ranges

  Calculate estimated cost and other information about an MRR scan for given
  sequence of ranges.

  @param keyno           Index number
  @param seq             Range sequence to be traversed
  @param seq_init_param  First parameter for seq->init()
  @param n_ranges_arg    Number of ranges in the sequence, or 0 if the caller
                         can't efficiently determine it
  @param bufsz[in,out]   IN:  Size of the buffer available for use
                         OUT: Size of the buffer that is expected to be actually
                              used, or 0 if buffer is not needed.
  @param flags[in,out]   A combination of HA_MRR_* flags
  @param cost[out]       Estimated cost of MRR access

  @note
    This method (or an overriding one in a derived class) must check for
    thd->killed and return HA_POS_ERROR if it is not zero. This is required
    for a user to be able to interrupt the calculation by killing the
    connection/query.

  @retval
    HA_POS_ERROR  Error or the engine is unable to perform the requested
                  scan. Values of OUT parameters are undefined.
  @retval
    other         OK, *cost contains cost of the scan, *bufsz and *flags
                  contain scan parameters.
*/

ha_rows 
handler::multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                     void *seq_init_param, uint n_ranges_arg,
                                     uint *bufsz, uint *flags, 
                                     Cost_estimate *cost)
{
  KEY_MULTI_RANGE range;
  range_seq_t seq_it;
  ha_rows rows, total_rows= 0;
  uint n_ranges=0;
  THD *thd= current_thd;
  
  /* Default MRR implementation doesn't need buffer */
  *bufsz= 0;

  DBUG_EXECUTE_IF("bug13822652_2", thd->killed= THD::KILL_QUERY;);

  seq_it= seq->init(seq_init_param, n_ranges, *flags);
  while (!seq->next(seq_it, &range))
  {
    if (unlikely(thd->killed != 0))
      return HA_POS_ERROR;
    
    n_ranges++;
    key_range *min_endp, *max_endp;
    if (range.range_flag & GEOM_FLAG)
    {
      min_endp= &range.start_key;
      max_endp= NULL;
    }
    else
    {
      min_endp= range.start_key.length? &range.start_key : NULL;
      max_endp= range.end_key.length? &range.end_key : NULL;
    }
    /*
      Get the number of rows in the range. This is done by calling
      records_in_range() unless:

        1) The range is an equality range and the index is unique.
           There cannot be more than one matching row, so 1 is
           assumed. Note that it is possible that the correct number
           is actually 0, so the row estimate may be too high in this
           case. Also note: ranges of the form "x IS NULL" may have more
           than 1 mathing row so records_in_range() is called for these.
        2) a) The range is an equality range but the index is either 
              not unique or all of the keyparts are not used. 
           b) The user has requested that index statistics should be used
              for equality ranges to avoid the incurred overhead of 
              index dives in records_in_range().
           c) Index statistics is available.
           Ranges of the form "x IS NULL" will not use index statistics 
           because the number of rows with this value are likely to be 
           very different than the values in the index statistics.
    */
    int keyparts_used= 0;
    if ((range.range_flag & UNIQUE_RANGE) &&                        // 1)
        !(range.range_flag & NULL_RANGE))
      rows= 1; /* there can be at most one row */
    else if ((range.range_flag & EQ_RANGE) &&                       // 2a)
             (range.range_flag & USE_INDEX_STATISTICS) &&           // 2b)
             (keyparts_used= my_count_bits(range.start_key.keypart_map)) &&
             table->
               key_info[keyno].has_records_per_key(keyparts_used-1) && // 2c)
             !(range.range_flag & NULL_RANGE))
    {
      rows= static_cast<ha_rows>(
        table->key_info[keyno].records_per_key(keyparts_used - 1));
    }
    else
    {
      DBUG_EXECUTE_IF("crash_records_in_range", DBUG_SUICIDE(););
      DBUG_ASSERT(min_endp || max_endp);
      if (HA_POS_ERROR == (rows= this->records_in_range(keyno, min_endp, 
                                                        max_endp)))
      {
        /* Can't scan one range => can't do MRR scan at all */
        total_rows= HA_POS_ERROR;
        break;
      }
    }
    total_rows += rows;
  }
  
  if (total_rows != HA_POS_ERROR)
  {
    const Cost_model_table *const cost_model= table->cost_model();

    /* The following calculation is the same as in multi_range_read_info(): */
    *flags|= HA_MRR_USE_DEFAULT_IMPL;
    *flags|= HA_MRR_SUPPORT_SORTED;

    DBUG_ASSERT(cost->is_zero());
    if (*flags & HA_MRR_INDEX_ONLY)
      *cost= index_scan_cost(keyno, static_cast<double>(n_ranges),
                             static_cast<double>(total_rows));
    else
      *cost= read_cost(keyno, static_cast<double>(n_ranges),
                       static_cast<double>(total_rows));
    cost->add_cpu(cost_model->row_evaluate_cost(
      static_cast<double>(total_rows)) + 0.01);
  }
  return total_rows;
}


/**
  Get cost and other information about MRR scan over some sequence of ranges

  Calculate estimated cost and other information about an MRR scan for some
  sequence of ranges.

  The ranges themselves will be known only at execution phase. When this
  function is called we only know number of ranges and a (rough) E(#records)
  within those ranges.

  Currently this function is only called for "n-keypart singlepoint" ranges,
  i.e. each range is "keypart1=someconst1 AND ... AND keypartN=someconstN"

  The flags parameter is a combination of those flags: HA_MRR_SORTED,
  HA_MRR_INDEX_ONLY, HA_MRR_NO_ASSOCIATION, HA_MRR_LIMITS.

  @param keyno           Index number
  @param n_ranges        Estimated number of ranges (i.e. intervals) in the
                         range sequence.
  @param n_rows          Estimated total number of records contained within all
                         of the ranges
  @param bufsz[in,out]   IN:  Size of the buffer available for use
                         OUT: Size of the buffer that will be actually used, or
                              0 if buffer is not needed.
  @param flags[in,out]   A combination of HA_MRR_* flags
  @param cost[out]       Estimated cost of MRR access

  @retval
    0     OK, *cost contains cost of the scan, *bufsz and *flags contain scan
          parameters.
  @retval
    other Error or can't perform the requested scan
*/

ha_rows handler::multi_range_read_info(uint keyno, uint n_ranges, uint n_rows,
                                       uint *bufsz, uint *flags, 
                                       Cost_estimate *cost)
{
  *bufsz= 0; /* Default implementation doesn't need a buffer */

  *flags|= HA_MRR_USE_DEFAULT_IMPL;
  *flags|= HA_MRR_SUPPORT_SORTED;

  DBUG_ASSERT(cost->is_zero());

  /* Produce the same cost as non-MRR code does */
  if (*flags & HA_MRR_INDEX_ONLY)
    *cost= index_scan_cost(keyno, n_ranges, n_rows);
  else
    *cost= read_cost(keyno, n_ranges, n_rows);
  return 0;
}


/**
  Initialize the MRR scan

  Initialize the MRR scan. This function may do heavyweight scan 
  initialization like row prefetching/sorting/etc (NOTE: but better not do
  it here as we may not need it, e.g. if we never satisfy WHERE clause on
  previous tables. For many implementations it would be natural to do such
  initializations in the first multi_read_range_next() call)

  mode is a combination of the following flags: HA_MRR_SORTED,
  HA_MRR_INDEX_ONLY, HA_MRR_NO_ASSOCIATION 

  @param seq             Range sequence to be traversed
  @param seq_init_param  First parameter for seq->init()
  @param n_ranges        Number of ranges in the sequence
  @param mode            Flags, see the description section for the details
  @param buf             INOUT: memory buffer to be used

  @note
    One must have called index_init() before calling this function. Several
    multi_range_read_init() calls may be made in course of one query.

    Until WL#2623 is done (see its text, section 3.2), the following will 
    also hold:
    The caller will guarantee that if "seq->init == mrr_ranges_array_init"
    then seq_init_param is an array of n_ranges KEY_MULTI_RANGE structures.
    This property will only be used by NDB handler until WL#2623 is done.
     
    Buffer memory management is done according to the following scenario:
    The caller allocates the buffer and provides it to the callee by filling
    the members of HANDLER_BUFFER structure.
    The callee consumes all or some fraction of the provided buffer space, and
    sets the HANDLER_BUFFER members accordingly.
    The callee may use the buffer memory until the next multi_range_read_init()
    call is made, all records have been read, or until index_end() call is
    made, whichever comes first.

  @retval 0  OK
  @retval 1  Error
*/

int
handler::multi_range_read_init(RANGE_SEQ_IF *seq_funcs, void *seq_init_param,
                               uint n_ranges, uint mode, HANDLER_BUFFER *buf)
{
  DBUG_ENTER("handler::multi_range_read_init");
  mrr_iter= seq_funcs->init(seq_init_param, n_ranges, mode);
  mrr_funcs= *seq_funcs;
  mrr_is_output_sorted= MY_TEST(mode & HA_MRR_SORTED);
  mrr_have_range= FALSE;
  DBUG_RETURN(0);
}


/**
  Get next record in MRR scan

  Default MRR implementation: read the next record

  @param range_info  OUT  Undefined if HA_MRR_NO_ASSOCIATION flag is in effect
                          Otherwise, the opaque value associated with the range
                          that contains the returned record.

  @retval 0      OK
  @retval other  Error code
*/

int handler::multi_range_read_next(char **range_info)
{
  int result= HA_ERR_END_OF_FILE;
  int range_res;
  DBUG_ENTER("handler::multi_range_read_next");

  // Set status for the need to update generated fields
  m_update_generated_read_fields= table->has_gcol();

  if (!mrr_have_range)
  {
    mrr_have_range= TRUE;
    goto start;
  }

  do
  {
    /* Save a call if there can be only one row in range. */
    if (mrr_cur_range.range_flag != (UNIQUE_RANGE | EQ_RANGE))
    {
      result= read_range_next();
      /* On success or non-EOF errors jump to the end. */
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
    else
    {
      if (was_semi_consistent_read())
        goto scan_it_again;
    }

start:
    /* Try the next range(s) until one matches a record. */
    while (!(range_res= mrr_funcs.next(mrr_iter, &mrr_cur_range)))
    {
scan_it_again:
      result= read_range_first(mrr_cur_range.start_key.keypart_map ?
                                 &mrr_cur_range.start_key : 0,
                               mrr_cur_range.end_key.keypart_map ?
                                 &mrr_cur_range.end_key : 0,
                               MY_TEST(mrr_cur_range.range_flag & EQ_RANGE),
                               mrr_is_output_sorted);
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
  }
  while ((result == HA_ERR_END_OF_FILE) && !range_res);

  *range_info= mrr_cur_range.ptr;

  /* Update virtual generated fields */
  if (!result && m_update_generated_read_fields)
  {
    result= update_generated_read_fields(table->record[0], table, active_index);
    m_update_generated_read_fields= false;
  }

  DBUG_PRINT("exit",("handler::multi_range_read_next result %d", result));
  DBUG_RETURN(result);
}


/****************************************************************************
 * DS-MRR implementation 
 ***************************************************************************/

/**
  DS-MRR: Initialize and start MRR scan

  Initialize and start the MRR scan. Depending on the mode parameter, this
  may use default or DS-MRR implementation.

  The DS-MRR implementation will use a second handler object (h2) for
  doing scan on the index:
  - on the first call to this function the h2 handler will be created
    and h2 will be opened using the same index as the main handler
    is set to use. The index scan on the main index will be closed
    and it will be re-opened to read records from the table using either 
    no key or the primary key. The h2 handler will be deleted when
    reset() is called (which should happen on the end of the statement).
  - when dsmrr_close() is called the index scan on h2 is closed.
  - on following calls to this function one of the following must be valid:
    a. if dsmrr_close has been called:
       the main handler (h) must be open on an index, h2 will be opened
       using this index, and the index on h will be closed and 
       h will be re-opened to read reads from the table using either
       no key or the primary key.
    b. dsmrr_close has not been called:
       h2 will already be open, the main handler h must be set up
       to read records from the table (handler->inited is RND) either
       using the primary index or using no index at all.

  @param h_arg           Table handler to be used
  @param seq_funcs       Interval sequence enumeration functions
  @param seq_init_param  Interval sequence enumeration parameter
  @param n_ranges        Number of ranges in the sequence.
  @param mode            HA_MRR_* modes to use
  @param buf             INOUT Buffer to use

  @retval 0     Ok, Scan started.
  @retval other Error
*/

int DsMrr_impl::dsmrr_init(handler *h_arg, RANGE_SEQ_IF *seq_funcs, 
                           void *seq_init_param, uint n_ranges, uint mode,
                           HANDLER_BUFFER *buf)
{
  uint elem_size;
  int retval= 0;
  DBUG_ENTER("DsMrr_impl::dsmrr_init");
  THD *thd= h_arg->table->in_use;     // current THD

  /*
    index_merge may invoke a scan on an object for which dsmrr_info[_const]
    has not been called, so set the owner handler here as well.
  */
  h= h_arg;
  
  if (!hint_key_state(thd, h->table, h->active_index,
                      MRR_HINT_ENUM, OPTIMIZER_SWITCH_MRR) ||
      mode & (HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED)) // DS-MRR doesn't sort
  {
    use_default_impl= TRUE;
    retval= h->handler::multi_range_read_init(seq_funcs, seq_init_param,
                                              n_ranges, mode, buf);
    DBUG_RETURN(retval);
  }

  /* 
    This assert will hit if we have pushed an index condition to the
    primary key index and then "change our mind" and use a different
    index for retrieving data with MRR. One of the following criteria
    must be true:
      1. We have not pushed an index conditon on this handler.
      2. We have pushed an index condition and this is on the currently used
         index.
      3. We have pushed an index condition but this is not for the primary key.
      4. We have pushed an index condition and this has been transferred to 
         the clone (h2) of the handler object.
  */
  DBUG_ASSERT(!h->pushed_idx_cond ||
              h->pushed_idx_cond_keyno == h->active_index ||
              h->pushed_idx_cond_keyno != table->s->primary_key ||
              (h2 && h->pushed_idx_cond_keyno == h2->active_index));

  rowids_buf= buf->buffer;

  is_mrr_assoc= !MY_TEST(mode & HA_MRR_NO_ASSOCIATION);

  if (is_mrr_assoc)
    table->in_use->status_var.ha_multi_range_read_init_count++;
 
  rowids_buf_end= buf->buffer_end;
  elem_size= h->ref_length + (int)is_mrr_assoc * sizeof(void*);
  rowids_buf_last= rowids_buf + 
                      ((rowids_buf_end - rowids_buf)/ elem_size)*
                      elem_size;
  rowids_buf_end= rowids_buf_last;

  /*
    The DS-MRR scan uses a second handler object (h2) for doing the
    index scan. Create this by cloning the primary handler
    object. The h2 handler object is deleted when DsMrr_impl::reset()
    is called.
  */
  if (!h2)
  {
    handler *new_h2;
    /*
      ::clone() takes up a lot of stack, especially on 64 bit platforms.
      The constant 5 is an empiric result.
      @todo Is this still the case? Leave it as it is for now but could
            likely be removed?
    */
    if (check_stack_overrun(thd, 5*STACK_MIN_SIZE, (uchar*) &new_h2))
      DBUG_RETURN(1);

    if (!(new_h2= h->clone(h->table->s->normalized_path.str, thd->mem_root)))
      DBUG_RETURN(1);
    h2= new_h2; /* Ok, now can put it into h2 */
    table->prepare_for_position();
  }

  /*
    Open the index scan on h2 using the key from the primary handler.
  */
  if (h2->active_index == MAX_KEY)
  {
    DBUG_ASSERT(h->active_index != MAX_KEY);
    const uint mrr_keyno= h->active_index;

    if ((retval= h2->ha_external_lock(thd, h->m_lock_type)))
      goto error;

    if ((retval= h2->extra(HA_EXTRA_KEYREAD)))
      goto error;

    if ((retval= h2->ha_index_init(mrr_keyno, false)))
      goto error;

    // Transfer ICP from h to h2
    if (mrr_keyno == h->pushed_idx_cond_keyno)
    {
      if (h2->idx_cond_push(mrr_keyno, h->pushed_idx_cond))
      {
        retval= 1;
        goto error;
      }
    }
    else
    {
      // Cancel any potentially previously pushed index conditions
      h2->cancel_pushed_idx_cond();
    }
  }
  else
  {
    /*
      h2 has already an open index. This happens when the DS-MRR scan
      is re-started without closing it first. In this case the primary
      handler must be used for reading records from the table, ie. it
      must not be opened for doing a new range scan. In this case
      the active_index must either not be set or be the primary key.
    */
    DBUG_ASSERT(h->inited == handler::RND);
    DBUG_ASSERT(h->active_index == MAX_KEY || 
                h->active_index == table->s->primary_key);
  }

  /*
    The index scan is now transferred to h2 and we can close the open
    index scan on the primary handler.
  */
  if (h->inited == handler::INDEX)
  {
    /*
      Calling h->ha_index_end() will invoke dsmrr_close() for this object,
      which will close the index scan on h2. We need to keep it open, so 
      temporarily move h2 out of the DsMrr object.
    */
    handler *save_h2= h2;
    h2= NULL;
    retval= h->ha_index_end();
    h2= save_h2;
    if (retval)
      goto error;
  }

  /*
    Verify consistency between h and h2.
  */
  DBUG_ASSERT(h->inited != handler::INDEX);
  DBUG_ASSERT(h->active_index == MAX_KEY || 
              h->active_index == table->s->primary_key);
  DBUG_ASSERT(h2->inited == handler::INDEX);
  DBUG_ASSERT(h2->active_index != MAX_KEY);
  DBUG_ASSERT(h->m_lock_type == h2->m_lock_type);

  if ((retval= h2->handler::multi_range_read_init(seq_funcs, seq_init_param, 
                                                  n_ranges, mode, buf)))
    goto error;

  if ((retval= dsmrr_fill_buffer()))
    goto error;

  /*
    If the above call has scanned through all intervals in *seq, then
    adjust *buf to indicate that the remaining buffer space will not be used.
  */
  if (dsmrr_eof) 
    buf->end_of_used_area= rowids_buf_last;

  /*
     h->inited == INDEX may occur when 'range checked for each record' is
     used.
  */
  if ((h->inited != handler::RND) && 
      ((h->inited==handler::INDEX? h->ha_index_end(): FALSE) || 
       (h->ha_rnd_init(FALSE))))
  {
    retval= 1;
    goto error;
  }

  use_default_impl= FALSE;
  h->mrr_funcs= *seq_funcs;
  
  DBUG_RETURN(0);
error:
  h2->ha_index_or_rnd_end();
  h2->ha_external_lock(thd, F_UNLCK);
  h2->ha_close();
  delete h2;
  h2= NULL;
  DBUG_ASSERT(retval != 0);
  DBUG_RETURN(retval);
}


void DsMrr_impl::dsmrr_close()
{
  DBUG_ENTER("DsMrr_impl::dsmrr_close");

  // If there is an open index on h2, then close it
  if (h2 && h2->active_index != MAX_KEY)
  {
    h2->ha_index_or_rnd_end();
    h2->ha_external_lock(current_thd, F_UNLCK);
  }
  use_default_impl= true;
  DBUG_VOID_RETURN;
}


void DsMrr_impl::reset()
{
  DBUG_ENTER("DsMrr_impl::reset");

  if (h2)
  {
    // Close any ongoing DS-MRR scan 
    dsmrr_close();

    // Close and delete the h2 handler
    h2->ha_close();
    delete h2;
    h2= NULL;
  }
  DBUG_VOID_RETURN;
}


static int rowid_cmp(void *h, uchar *a, uchar *b)
{
  return ((handler*)h)->cmp_ref(a, b);
}


/**
  DS-MRR: Fill the buffer with rowids and sort it by rowid

  {This is an internal function of DiskSweep MRR implementation}
  Scan the MRR ranges and collect ROWIDs (or {ROWID, range_id} pairs) into 
  buffer. When the buffer is full or scan is completed, sort the buffer by 
  rowid and return.
  
  The function assumes that rowids buffer is empty when it is invoked. 
  
  @param h  Table handler

  @retval 0      OK, the next portion of rowids is in the buffer,
                 properly ordered
  @retval other  Error
*/

int DsMrr_impl::dsmrr_fill_buffer()
{
  char *range_info;
  int res= 0;
  DBUG_ENTER("DsMrr_impl::dsmrr_fill_buffer");
  DBUG_ASSERT(rowids_buf < rowids_buf_end);

  /*
    Set key_read to TRUE since we only read fields from the index.
    This ensures that any virtual columns are read from index and are not
    attempted to be evaluated from base columns.
    (Do not use TABLE::set_keyread() since the MRR implementation operates
    with two handler objects, and set_keyread() would manipulate the keyread
    property of the wrong handler. MRR sets the handlers' keyread properties
    when initializing the MRR operation, independent of this call).
  */
  DBUG_ASSERT(table->key_read == FALSE);
  table->key_read= TRUE;

  rowids_buf_cur= rowids_buf;
  while ((rowids_buf_cur < rowids_buf_end) && 
         !(res= h2->handler::multi_range_read_next(&range_info)))
  {
    KEY_MULTI_RANGE *curr_range= &h2->handler::mrr_cur_range;
    if (h2->mrr_funcs.skip_index_tuple &&
        h2->mrr_funcs.skip_index_tuple(h2->mrr_iter, curr_range->ptr))
      continue;
    
    /* Put rowid, or {rowid, range_id} pair into the buffer */
    h2->position(table->record[0]);
    memcpy(rowids_buf_cur, h2->ref, h2->ref_length);
    rowids_buf_cur += h2->ref_length;

    if (is_mrr_assoc)
    {
      memcpy(rowids_buf_cur, &range_info, sizeof(void*));
      rowids_buf_cur += sizeof(void*);
    }
  }

  // Restore key_read since the next read operation will read complete rows
  table->key_read= FALSE;

  if (res && res != HA_ERR_END_OF_FILE)
    DBUG_RETURN(res); 
  dsmrr_eof= MY_TEST(res == HA_ERR_END_OF_FILE);

  /* Sort the buffer contents by rowid */
  uint elem_size= h->ref_length + (int)is_mrr_assoc * sizeof(void*);
  size_t n_rowids= (rowids_buf_cur - rowids_buf) / elem_size;
  
  my_qsort2(rowids_buf, n_rowids, elem_size, (qsort2_cmp)rowid_cmp,
            (void*)h);
  rowids_buf_last= rowids_buf_cur;
  rowids_buf_cur=  rowids_buf;
  DBUG_RETURN(0);
}


/*
  DS-MRR implementation: multi_range_read_next() function
*/

int DsMrr_impl::dsmrr_next(char **range_info)
{
  int res;
  uchar *cur_range_info= 0;
  uchar *rowid;

  if (use_default_impl)
    return h->handler::multi_range_read_next(range_info);
  
  do
  {
    if (rowids_buf_cur == rowids_buf_last)
    {
      if (dsmrr_eof)
      {
        res= HA_ERR_END_OF_FILE;
        goto end;
      }

      res= dsmrr_fill_buffer();
      if (res)
        goto end;
    }
   
    /* return eof if there are no rowids in the buffer after re-fill attempt */
    if (rowids_buf_cur == rowids_buf_last)
    {
      res= HA_ERR_END_OF_FILE;
      goto end;
    }
    rowid= rowids_buf_cur;

    if (is_mrr_assoc)
      memcpy(&cur_range_info, rowids_buf_cur + h->ref_length, sizeof(uchar*));

    rowids_buf_cur += h->ref_length + sizeof(void*) * MY_TEST(is_mrr_assoc);
    if (h2->mrr_funcs.skip_record &&
	h2->mrr_funcs.skip_record(h2->mrr_iter, (char *) cur_range_info, rowid))
      continue;
    res= h->ha_rnd_pos(table->record[0], rowid);
    break;
  } while (true);
 
  if (is_mrr_assoc)
  {
    memcpy(range_info, rowid + h->ref_length, sizeof(void*));
  }
end:
  return res;
}


/*
  DS-MRR implementation: multi_range_read_info() function
*/
ha_rows DsMrr_impl::dsmrr_info(uint keyno, uint n_ranges, uint rows,
                               uint *bufsz, uint *flags, Cost_estimate *cost)
{
  ha_rows res MY_ATTRIBUTE((unused));
  uint def_flags= *flags;
  uint def_bufsz= *bufsz;

  /* Get cost/flags/mem_usage of default MRR implementation */
  res=
    h->handler::multi_range_read_info(keyno, n_ranges, rows, &def_bufsz,
                                      &def_flags, cost);
  DBUG_ASSERT(!res);

  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) || 
      choose_mrr_impl(keyno, rows, flags, bufsz, cost))
  {
    /* Default implementation is choosen */
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags= def_flags;
    *bufsz= def_bufsz;
    DBUG_ASSERT(*flags & HA_MRR_USE_DEFAULT_IMPL);
  }
  else
  {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("DS-MRR implementation choosen"));
  }
  return 0;
}


/*
  DS-MRR Implementation: multi_range_read_info_const() function
*/

ha_rows DsMrr_impl::dsmrr_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                 void *seq_init_param, uint n_ranges, 
                                 uint *bufsz, uint *flags, Cost_estimate *cost)
{
  ha_rows rows;
  uint def_flags= *flags;
  uint def_bufsz= *bufsz;
  /* Get cost/flags/mem_usage of default MRR implementation */
  rows= h->handler::multi_range_read_info_const(keyno, seq, seq_init_param,
                                                n_ranges, &def_bufsz, 
                                                &def_flags, cost);
  if (rows == HA_POS_ERROR)
  {
    /* Default implementation can't perform MRR scan => we can't either */
    return rows;
  }

  /*
    If HA_MRR_USE_DEFAULT_IMPL has been passed to us, that is an order to
    use the default MRR implementation (we need it for UPDATE/DELETE).
    Otherwise, make a choice based on cost and mrr* flags of
    @@optimizer_switch.
  */
  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) ||
      choose_mrr_impl(keyno, rows, flags, bufsz, cost))
  {
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags= def_flags;
    *bufsz= def_bufsz;
    DBUG_ASSERT(*flags & HA_MRR_USE_DEFAULT_IMPL);
  }
  else
  {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("DS-MRR implementation choosen"));
  }
  return rows;
}


/**
  DS-MRR Internals: Choose between Default MRR implementation and DS-MRR

  Make the choice between using Default MRR implementation and DS-MRR.
  This function contains common functionality factored out of dsmrr_info()
  and dsmrr_info_const(). The function assumes that the default MRR
  implementation's applicability requirements are satisfied.

  @param keyno       Index number
  @param rows        E(full rows to be retrieved)
  @param flags  IN   MRR flags provided by the MRR user
                OUT  If DS-MRR is choosen, flags of DS-MRR implementation
                     else the value is not modified
  @param bufsz  IN   If DS-MRR is choosen, buffer use of DS-MRR implementation
                     else the value is not modified
  @param cost   IN   Cost of default MRR implementation
                OUT  If DS-MRR is choosen, cost of DS-MRR scan
                     else the value is not modified

  @retval TRUE   Default MRR implementation should be used
  @retval FALSE  DS-MRR implementation should be used
*/

bool DsMrr_impl::choose_mrr_impl(uint keyno, ha_rows rows, uint *flags,
                                 uint *bufsz, Cost_estimate *cost)
{
  bool res;
  THD *thd= current_thd;

  const bool mrr_on= hint_key_state(thd, table, keyno, MRR_HINT_ENUM,
                                    OPTIMIZER_SWITCH_MRR);
  const bool force_dsmrr_by_hints=
    hint_key_state(thd, table, keyno, MRR_HINT_ENUM, 0) ||
    hint_table_state(thd, table, BKA_HINT_ENUM, 0);

  if (!(mrr_on || force_dsmrr_by_hints) ||
      *flags & (HA_MRR_INDEX_ONLY | HA_MRR_SORTED) || // Unsupported by DS-MRR
      (keyno == table->s->primary_key && h->primary_key_is_clustered()) ||
       key_uses_partial_cols(table, keyno) ||
       table->s->tmp_table != NO_TMP_TABLE)
  {
    /* Use the default implementation, don't modify args: See comments  */
    return TRUE;
  }

  /*
    If @@optimizer_switch has "mrr_cost_based" on, we should avoid
    using DS-MRR for queries where it is likely that the records are
    stored in memory. Since there is currently no way to determine
    this, we use a heuristic:
    a) if the storage engine has a memory buffer, DS-MRR is only
       considered if the table size is bigger than the buffer.
    b) if the storage engine does not have a memory buffer, DS-MRR is
       only considered if the table size is bigger than 100MB.
    c) Since there is an initial setup cost of DS-MRR, so it is only
       considered if at least 50 records will be read.
  */
  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_MRR_COST_BASED) &&
      !force_dsmrr_by_hints)
  {
    /*
      If the storage engine has a database buffer we use this as the
      minimum size the table should have before considering DS-MRR.
    */ 
    longlong min_file_size= table->file->get_memory_buffer_size();
    if (min_file_size == -1)
    {
      // No estimate for database buffer
      min_file_size= 100 * 1024 * 1024;    // 100 MB
    }

    if (table->file->stats.data_file_length < 
        static_cast<ulonglong>(min_file_size) ||
        rows <= 50)
      return true;                 // Use the default implementation 
  }

  Cost_estimate dsmrr_cost;
  if (get_disk_sweep_mrr_cost(keyno, rows, *flags, bufsz, &dsmrr_cost))
    return TRUE;
  
  /* 
    If @@optimizer_switch has "mrr" on and "mrr_cost_based" off, then set cost
    of DS-MRR to be minimum of DS-MRR and Default implementations cost. This
    allows one to force use of DS-MRR whenever it is applicable without
    affecting other cost-based choices. Note that if MRR or BKA hint is
    specified, DS-MRR will be used regardless of cost.
  */
  const bool force_dsmrr=
    (force_dsmrr_by_hints ||
     !thd->optimizer_switch_flag(OPTIMIZER_SWITCH_MRR_COST_BASED));

  if (force_dsmrr && dsmrr_cost.total_cost() > cost->total_cost())
    dsmrr_cost= *cost;

  if (force_dsmrr || (dsmrr_cost.total_cost() <= cost->total_cost()))
  {
    *flags &= ~HA_MRR_USE_DEFAULT_IMPL;  /* Use the DS-MRR implementation */
    *flags &= ~HA_MRR_SUPPORT_SORTED;    /* We can't provide ordered output */
    *cost= dsmrr_cost;
    res= FALSE;
  }
  else
  {
    /* Use the default MRR implementation */
    res= TRUE;
  }
  return res;
}


static void get_sort_and_sweep_cost(TABLE *table, ha_rows nrows, 
                                    Cost_estimate *cost);


/**
  Get cost of DS-MRR scan

  @param keynr              Index to be used
  @param rows               E(Number of rows to be scanned)
  @param flags              Scan parameters (HA_MRR_* flags)
  @param buffer_size INOUT  Buffer size
  @param cost        OUT    The cost

  @retval FALSE  OK
  @retval TRUE   Error, DS-MRR cannot be used (the buffer is too small
                 for even 1 rowid)
*/

bool DsMrr_impl::get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags,
                                         uint *buffer_size, 
                                         Cost_estimate *cost)
{
  ha_rows rows_in_last_step;
  uint n_full_steps;

  const uint elem_size= h->ref_length + 
                        sizeof(void*) * (!MY_TEST(flags & HA_MRR_NO_ASSOCIATION));
  const ha_rows max_buff_entries= *buffer_size / elem_size;

  if (!max_buff_entries)
    return TRUE; /* Buffer has not enough space for even 1 rowid */

  /* Number of iterations we'll make with full buffer */
  n_full_steps= (uint)floor(rows2double(rows) / max_buff_entries);

  /*
    Get numbers of rows we'll be processing in last iteration, with
    non-full buffer
  */
  rows_in_last_step= rows % max_buff_entries;
  
  DBUG_ASSERT(cost->is_zero());

  if (n_full_steps)
  {
    get_sort_and_sweep_cost(table, max_buff_entries, cost);
    cost->multiply(n_full_steps);
  }
  else
  {
    /* 
      Adjust buffer size since only parts of the buffer will be used:
      1. Adjust record estimate for the last scan to reduce likelyhood
         of needing more than one scan by adding 20 percent to the
         record estimate and by ensuring this is at least 100 records.
      2. If the estimated needed buffer size is lower than suggested by 
         the caller then set it to the estimated buffer size.
    */
    const ha_rows keys_in_buffer=
      max<ha_rows>(static_cast<ha_rows>(1.2 * rows_in_last_step), 100);
    *buffer_size= min<ulong>(*buffer_size,
                             static_cast<ulong>(keys_in_buffer) * elem_size);
  }

  Cost_estimate last_step_cost;
  get_sort_and_sweep_cost(table, rows_in_last_step, &last_step_cost);
  (*cost)+= last_step_cost;

  /*
    Cost of memory is not included in the total_cost() function and
    thus will not be considered when comparing costs. Still, we
    record it in the cost estimate object for future use.
  */
  cost->add_mem(*buffer_size);

  /* Total cost of all index accesses */
  (*cost)+= h->index_scan_cost(keynr, 1, static_cast<double>(rows));

  /*
    Add CPU cost for processing records (see
    @handler::multi_range_read_info_const()).
  */
  cost->add_cpu(table->cost_model()->row_evaluate_cost(
    static_cast<double>(rows)));
  return FALSE;
}


/* 
  Get cost of one sort-and-sweep step

  SYNOPSIS
    get_sort_and_sweep_cost()
      table       Table being accessed
      nrows       Number of rows to be sorted and retrieved
      cost   OUT  The cost

  DESCRIPTION
    Get cost of these operations:
     - sort an array of #nrows ROWIDs using qsort
     - read #nrows records from table in a sweep.
*/

static 
void get_sort_and_sweep_cost(TABLE *table, ha_rows nrows, Cost_estimate *cost)
{
  DBUG_ASSERT(cost->is_zero());
  if (nrows)
  {
    get_sweep_read_cost(table, nrows, FALSE, cost);

    /*
      @todo CostModel: For the old version of the cost model the
      following code should be used. For the new version of the cost
      model Cost_model::key_compare_cost() should be used.  When
      removing support for the old cost model this code should be
      removed. The reason for this is that we should get rid of the
      ROWID_COMPARE_SORT_COST and use key_compare_cost() instead. For
      the current value returned by key_compare_cost() this would
      overestimate the cost for sorting.
    */

    /* 
      Constant for the cost of doing one key compare operation in the
      sort operation. We should have used the value returned by
      key_compare_cost() here but this would make the cost
      estimate of sorting very high for queries accessing many
      records. Until this constant is adjusted we introduce a constant
      that is more realistic. @todo: Replace this with
      key_compare_cost() when this has been given a realistic value.
    */
    const double ROWID_COMPARE_SORT_COST=
      table->cost_model()->key_compare_cost(1.0) / 10;

    /* Add cost of qsort call: n * log2(n) * cost(rowid_comparison) */
    
    // For the old version of the cost model this cost calculations should
    // be used....
    const double cpu_sort= nrows * log2(nrows) * ROWID_COMPARE_SORT_COST;
    // .... For the new cost model something like this should be used...
    // cpu_sort= nrows * log2(nrows) *
    //           table->cost_model()->rowid_compare_cost();
    cost->add_cpu(cpu_sort);
  }
}


/**
  Get cost of reading nrows table records in a "disk sweep"

  A disk sweep read is a sequence of handler->rnd_pos(rowid) calls that made
  for an ordered sequence of rowids.

  We take into account that some of the records might be in a memory
  buffer while others need to be read from a secondary storage
  device. The model for this assumes hard disk IO. A disk read is
  performed as follows:

   1. The disk head is moved to the needed cylinder
   2. The controller waits for the plate to rotate
   3. The data is transferred

  Time to do #3 is insignificant compared to #2+#1.

  Time to move the disk head is proportional to head travel distance.

  Time to wait for the plate to rotate depends on whether the disk head
  was moved or not. 

  If disk head wasn't moved, the wait time is proportional to distance
  between the previous block and the block we're reading.

  If the head was moved, we don't know how much we'll need to wait for the
  plate to rotate. We assume the wait time to be a variate with a mean of
  0.5 of full rotation time.

  Our cost units are "random disk seeks". The cost of random disk seek is
  actually not a constant, it depends one range of cylinders we're going
  to access. We make it constant by introducing a fuzzy concept of "typical 
  datafile length" (it's fuzzy as it's hard to tell whether it should
  include index file, temp.tables etc). Then random seek cost is:

    1 = half_rotation_cost + move_cost * 1/3 * typical_data_file_length

  We define half_rotation_cost as disk_seek_base_cost() (see
  Cost_model_server::disk_seek_base_cost()).

  @param      table        Table to be accessed
  @param      nrows        Number of rows to retrieve
  @param      interrupted  true <=> Assume that the disk sweep will be
                           interrupted by other disk IO. false - otherwise.
  @param[out] cost         the cost
*/

void get_sweep_read_cost(TABLE *table, ha_rows nrows, bool interrupted, 
                         Cost_estimate *cost)
{
  DBUG_ENTER("get_sweep_read_cost");

  DBUG_ASSERT(cost->is_zero());
  if(nrows > 0)
  {
    const Cost_model_table *const cost_model= table->cost_model();

    // The total number of blocks used by this table
    double n_blocks=
      ceil(ulonglong2double(table->file->stats.data_file_length) / IO_SIZE);
    if (n_blocks < 1.0)                         // When data_file_length is 0
      n_blocks= 1.0;

    /*
      The number of blocks that in average need to be read given that
      the records are uniformly distribution over the table.
    */
    double busy_blocks=
      n_blocks * (1.0 - pow(1.0 - 1.0/n_blocks, rows2double(nrows)));
    if (busy_blocks < 1.0)
      busy_blocks= 1.0;

    DBUG_PRINT("info",("sweep: nblocks=%g, busy_blocks=%g", n_blocks,
                       busy_blocks));
    /*
      The random access cost for reading the data pages will be the upper
      limit for the sweep_cost.
    */
    cost->add_io(cost_model->page_read_cost(busy_blocks));
    if (!interrupted)
    {
      Cost_estimate sweep_cost;
      /*
        Assume reading pages from disk is done in one 'sweep'. 

        The cost model and cost estimate for pages already in a memory
        buffer will be different from pages that needed to be read from
        disk. Calculate the number of blocks that likely already are
        in memory and the number of blocks that need to be read from
        disk.
      */
      const double busy_blocks_mem=
        busy_blocks * table->file->table_in_memory_estimate();
      const double busy_blocks_disk= busy_blocks - busy_blocks_mem;
      DBUG_ASSERT(busy_blocks_disk >= 0.0);

      // Cost of accessing blocks in main memory buffer
      sweep_cost.add_io(cost_model->buffer_block_read_cost(busy_blocks_mem));

      // Cost of reading blocks from disk in a 'sweep'
      const double seek_distance= (busy_blocks_disk > 1.0) ?
        n_blocks / busy_blocks_disk : n_blocks;

      const double disk_cost=
        busy_blocks_disk * cost_model->disk_seek_cost(seek_distance);
      sweep_cost.add_io(disk_cost);

      /*
        For some cases, ex: when only few blocks need to be read and the
        seek distance becomes very large, the sweep cost model can produce
        a cost estimate that is larger than the cost of random access.
        To handle this case, we use the sweep cost only when it is less
        than the random access cost.
      */
      if (sweep_cost < *cost)
        *cost= sweep_cost;
    }
  }
  DBUG_PRINT("info",("returning cost=%g", cost->total_cost()));
  DBUG_VOID_RETURN;
}


/****************************************************************************
 * DS-MRR implementation ends
 ***************************************************************************/

/** @brief
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
			      bool eq_range_arg,
                              bool sorted /* ignored */)
{
  int result;
  DBUG_ENTER("handler::read_range_first");

  eq_range= eq_range_arg;
  set_end_range(end_key, RANGE_SCAN_ASC);

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


/** @brief
  Read next row between two endpoints.

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


void handler::set_end_range(const key_range* range,
                            enum_range_scan_direction direction)
{
  if (range)
  {
    save_end_range= *range;
    end_range= &save_end_range;
    range_key_part= table->key_info[active_index].key_part;
    key_compare_result_on_equal= ((range->flag == HA_READ_BEFORE_KEY) ? 1 :
                                  (range->flag == HA_READ_AFTER_KEY) ? -1 : 0);
  }
  else
    end_range= NULL;

  range_scan_direction= direction;
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
  Compare if a found key (in row) is within the range.

  This function is similar to compare_key() but checks the range scan
  direction to determine if this is a descending scan. This function
  is used by the index condition pushdown implementation to determine
  if the read record is within the range scan.

  @param range Range to compare to row. May be NULL for no range.

  @seealso
    handler::compare_key()

  @return Returns whether the key is within the range

    - 0   : Key is equal to range or 'range' == 0 (no range)
    - -1  : Key is within the current range
    - 1   : Key is outside the current range
*/

int handler::compare_key_icp(const key_range *range) const
{
  int cmp;
  if (!range)
    return 0;					// no max range
  cmp= key_cmp(range_key_part, range->key, range->length);
  if (!cmp)
    cmp= key_compare_result_on_equal;
  if (range_scan_direction == RANGE_SCAN_DESC)
    cmp= -cmp;
  return cmp;
}

/**
  Change the offsets of all the fields in a key range.

  @param range	  the key range
  @param key_part the first key part
  @param diff	  how much to change the offsets with
*/
static inline void
move_key_field_offsets(const key_range *range, const KEY_PART_INFO *key_part,
		       my_ptrdiff_t diff)
{
  for (size_t len= 0; len < range->length;
       len+= key_part->store_length, ++key_part)
    key_part->field->move_field_offset(diff);
}

/**
  Check if the key in the given buffer (which is not necessarily
  TABLE::record[0]) is within range. Called by the storage engine to
  avoid reading too many rows.

  @param buf  the buffer that holds the key
  @retval -1 if the key is within the range
  @retval  0 if the key is equal to the end_range key, and
             key_compare_result_on_equal is 0
  @retval  1 if the key is outside the range
*/
int handler::compare_key_in_buffer(const uchar *buf) const
{
  DBUG_ASSERT(end_range != NULL);

  /*
    End range on descending scans is only checked with ICP for now, and then we
    check it with compare_key_icp() instead of this function.
  */
  DBUG_ASSERT(range_scan_direction == RANGE_SCAN_ASC);

  // Make the fields in the key point into the buffer instead of record[0].
  const my_ptrdiff_t diff= buf - table->record[0];
  if (diff != 0)
    move_key_field_offsets(end_range, range_key_part, diff);

  // Compare the key in buf against end_range.
  int cmp= key_cmp(range_key_part, end_range->key, end_range->length);
  if (cmp == 0)
    cmp= key_compare_result_on_equal;

  // Reset the field offsets.
  if (diff != 0)
    move_key_field_offsets(end_range, range_key_part, -diff);

  return cmp;
}

int handler::index_read_idx_map(uchar * buf, uint index, const uchar * key,
                                key_part_map keypart_map,
                                enum ha_rkey_function find_flag)
{
  int error, error1;
  error= index_init(index, 0);
  if (!error)
  {
    error= index_read_map(buf, key, keypart_map, find_flag);
    error1= index_end();
  }
  return error ?  error : error1;
}


uint calculate_key_len(TABLE *table, uint key,
                       key_part_map keypart_map)
{
  /* works only with key prefixes */
  DBUG_ASSERT(((keypart_map + 1) & keypart_map) == 0);

  KEY *key_info= table->key_info + key;
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end_key_part= key_part + actual_key_parts(key_info);
  uint length= 0;

  while (key_part < end_key_part && keypart_map)
  {
    length+= key_part->store_length;
    keypart_map >>= 1;
    key_part++;
  }
  return length;
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
  handlerton *hton= plugin_data<handlerton*>(plugin);
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

TYPELIB* ha_known_exts()
{
  TYPELIB *known_extensions = (TYPELIB*) sql_alloc(sizeof(TYPELIB));
  known_extensions->name= "known_exts";
  known_extensions->type_lengths= NULL;
  
  List<char> found_exts;
  const char **ext, *old_ext;

  found_exts.push_back((char*) TRG_EXT);
  found_exts.push_back((char*) TRN_EXT);

  plugin_foreach(NULL, exts_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &found_exts);

  size_t arr_length= sizeof(char *)* (found_exts.elements+1);
  ext= (const char **) sql_alloc(arr_length);

  DBUG_ASSERT(NULL != ext);
  known_extensions->count= found_exts.elements;
  known_extensions->type_names= ext;

  List_iterator_fast<char> it(found_exts);
  while ((old_ext= it++))
    *ext++= old_ext;
  *ext= NULL;
  return known_extensions;
}


static bool stat_print(THD *thd, const char *type, size_t type_len,
                       const char *file, size_t file_len,
                       const char *status, size_t status_len)
{
  Protocol *protocol= thd->get_protocol();
  protocol->start_row();
  protocol->store(type, type_len, system_charset_info);
  protocol->store(file, file_len, system_charset_info);
  protocol->store(status, status_len, system_charset_info);
  if (protocol->end_row())
    return TRUE;
  return FALSE;
}


static my_bool showstat_handlerton(THD *thd, plugin_ref plugin,
                                   void *arg)
{
  enum ha_stat_type stat= *(enum ha_stat_type *) arg;
  handlerton *hton= plugin_data<handlerton*>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->show_status &&
      hton->show_status(hton, thd, stat_print, stat))
    return TRUE;
  return FALSE;
}

bool ha_show_status(THD *thd, handlerton *db_type, enum ha_stat_type stat)
{
  List<Item> field_list;
  bool result;

  field_list.push_back(new Item_empty_string("Type",10));
  field_list.push_back(new Item_empty_string("Name",FN_REFLEN));
  field_list.push_back(new Item_empty_string("Status",10));

  if (thd->send_result_metadata(&field_list,
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
    {
      DBUG_EXECUTE_IF("simulate_show_status_failure",
                      DBUG_SET("+d,simulate_net_write_failure"););
      result= db_type->show_status &&
              db_type->show_status(db_type, thd, stat_print, stat) ? 1 : 0;
      DBUG_EXECUTE_IF("simulate_show_status_failure",
                      DBUG_SET("-d,simulate_net_write_failure"););
    }
  }

  if (!result)
    my_eof(thd);
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
                    ! table->no_replicate &&
                    binlog_filter->db_ok(table->s->db.str));
    table->s->cached_row_logging_check= check;
  }

  DBUG_ASSERT(table->s->cached_row_logging_check == 0 ||
              table->s->cached_row_logging_check == 1);

  return (thd->is_current_stmt_binlog_format_row() &&
          table->s->cached_row_logging_check &&
          (thd->variables.option_bits & OPTION_BIN_LOG) &&
          mysql_bin_log.is_open());
}


/** @brief
   Write table maps for all (manually or automatically) locked tables
   to the binary log.

   SYNOPSIS
     write_locked_table_maps()
       thd     Pointer to THD structure

   DESCRIPTION
       This function will generate and write table maps for all tables
       that are locked by the thread 'thd'.

   RETURN VALUE
       0   All OK
       1   Failed to write all table maps

   SEE ALSO
       THD::lock
*/

static int write_locked_table_maps(THD *thd)
{
  DBUG_ENTER("write_locked_table_maps");
  DBUG_PRINT("enter", ("thd: 0x%lx  thd->lock: 0x%lx "
                       "thd->extra_lock: 0x%lx",
                       (long) thd, (long) thd->lock, (long) thd->extra_lock));

  DBUG_PRINT("debug", ("get_binlog_table_maps(): %d", thd->get_binlog_table_maps()));

  if (thd->get_binlog_table_maps() == 0)
  {
    MYSQL_LOCK *locks[2];
    locks[0]= thd->extra_lock;
    locks[1]= thd->lock;
    for (uint i= 0 ; i < sizeof(locks)/sizeof(*locks) ; ++i )
    {
      MYSQL_LOCK const *const lock= locks[i];
      if (lock == NULL)
        continue;

      bool need_binlog_rows_query= thd->variables.binlog_rows_query_log_events;
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
          /*
            We need to have a transactional behavior for SQLCOM_CREATE_TABLE
            (e.g. CREATE TABLE... SELECT * FROM TABLE) in order to keep a
            compatible behavior with the STMT based replication even when
            the table is not transactional. In other words, if the operation
            fails while executing the insert phase nothing is written to the
            binlog.

            Note that at this point, we check the type of a set of tables to
            create the table map events. In the function binlog_log_row(),
            which calls the current function, we check the type of the table
            of the current row.
          */
          bool const has_trans= thd->lex->sql_command == SQLCOM_CREATE_TABLE ||
                                table->file->has_transactions();
          int const error= thd->binlog_write_table_map(table, has_trans,
                                                       need_binlog_rows_query);
          /* Binlog Rows_query log event once for one statement which updates
             two or more tables.*/
          if (need_binlog_rows_query)
            need_binlog_rows_query= FALSE;
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


typedef bool Log_func(THD*, TABLE*, bool,
                      const uchar*, const uchar*);

int binlog_log_row(TABLE* table,
                          const uchar *before_record,
                          const uchar *after_record,
                          Log_func *log_func)
{
  bool error= 0;
  THD *const thd= table->in_use;

  if (check_table_binlog_row_based(thd, table))
  {
    if (thd->variables.transaction_write_set_extraction != HASH_ALGORITHM_OFF)
    {
      if (before_record && after_record)
      {
        size_t length= table->s->reclength;
        uchar* temp_image=(uchar*) my_malloc(PSI_NOT_INSTRUMENTED,
                                             length,
                                             MYF(MY_WME));
        if (!temp_image)
        {
          sql_print_error("Out of memory on transaction write set extraction");
          return 1;
        }
        add_pke(table, thd);

        memcpy(temp_image, table->record[0],(size_t) table->s->reclength);
        memcpy(table->record[0],table->record[1],(size_t) table->s->reclength);

        add_pke(table, thd);

        memcpy(table->record[0], temp_image, (size_t) table->s->reclength);

        my_free(temp_image);
      }
      else
      {
        add_pke(table, thd);
      }
    }
    DBUG_DUMP("read_set 10", (uchar*) table->read_set->bitmap,
              (table->s->fields + 7) / 8);

    /*
      If there are no table maps written to the binary log, this is
      the first row handled in this statement. In that case, we need
      to write table maps for all locked tables to the binary log.
    */
    if (likely(!(error= write_locked_table_maps(thd))))
    {
      /*
        We need to have a transactional behavior for SQLCOM_CREATE_TABLE
        (i.e. CREATE TABLE... SELECT * FROM TABLE) in order to keep a
        compatible behavior with the STMT based replication even when
        the table is not transactional. In other words, if the operation
        fails while executing the insert phase nothing is written to the
        binlog.
      */
      bool const has_trans= thd->lex->sql_command == SQLCOM_CREATE_TABLE ||
                           table->file->has_transactions();
      error=
        (*log_func)(thd, table, has_trans, before_record, after_record);
    }
  }
  return error ? HA_ERR_RBR_LOGGING_FAILED : 0;
}

int handler::ha_external_lock(THD *thd, int lock_type)
{
  int error;
  DBUG_ENTER("handler::ha_external_lock");
  /*
    Whether this is lock or unlock, this should be true, and is to verify that
    if get_auto_increment() was called (thus may have reserved intervals or
    taken a table lock), ha_release_auto_increment() was too.
  */
  DBUG_ASSERT(next_insert_id == 0);
  /* Consecutive calls for lock without unlocking in between is not allowed */
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              ((lock_type != F_UNLCK && m_lock_type == F_UNLCK) ||
               lock_type == F_UNLCK));
  /* SQL HANDLER call locks/unlock while scanning (RND/INDEX). */
  DBUG_ASSERT(inited == NONE || table->open_by_handler);

  if (MYSQL_HANDLER_RDLOCK_START_ENABLED() && lock_type == F_RDLCK)
  {
    MYSQL_HANDLER_RDLOCK_START(table_share->db.str,
                               table_share->table_name.str);
  }
  else if (MYSQL_HANDLER_WRLOCK_START_ENABLED() && lock_type == F_WRLCK)
  {
    MYSQL_HANDLER_WRLOCK_START(table_share->db.str,
                               table_share->table_name.str);
  }
  else if (MYSQL_HANDLER_UNLOCK_START_ENABLED() && lock_type == F_UNLCK)
  {
    MYSQL_HANDLER_UNLOCK_START(table_share->db.str,
                               table_share->table_name.str);
  }

  ha_statistic_increment(&SSV::ha_external_lock_count);

  MYSQL_TABLE_LOCK_WAIT(PSI_TABLE_EXTERNAL_LOCK, lock_type,
    { error= external_lock(thd, lock_type); })

  /*
    We cache the table flags if the locking succeeded. Otherwise, we
    keep them as they were when they were fetched in ha_open().
  */

  if (error == 0)
  {
    /*
      The lock type is needed by MRR when creating a clone of this handler
      object.
    */
    m_lock_type= lock_type;
    cached_table_flags= table_flags();
  }

  if (MYSQL_HANDLER_RDLOCK_DONE_ENABLED() && lock_type == F_RDLCK)
  {
    MYSQL_HANDLER_RDLOCK_DONE(error);
  }
  else if (MYSQL_HANDLER_WRLOCK_DONE_ENABLED() && lock_type == F_WRLCK)
  {
    MYSQL_HANDLER_WRLOCK_DONE(error);
  }
  else if (MYSQL_HANDLER_UNLOCK_DONE_ENABLED() && lock_type == F_UNLCK)
  {
    MYSQL_HANDLER_UNLOCK_DONE(error);
  }
  DBUG_RETURN(error);
}


/** @brief
  Check handler usage and reset state of file to after 'open'

  @note can be called regardless of it is locked or not.
*/
int handler::ha_reset()
{
  DBUG_ENTER("handler::ha_reset");
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
  /* Reset information about pushed engine conditions */
  pushed_cond= NULL;
  /* Reset information about pushed index conditions */
  cancel_pushed_idx_cond();

  const int retval= reset();
  DBUG_RETURN(retval);
}


int handler::ha_write_row(uchar *buf)
{
  int error;
  Log_func *log_func= Write_rows_log_event::binlog_row_logging_function;
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);

  DBUG_ENTER("handler::ha_write_row");
  DBUG_EXECUTE_IF("inject_error_ha_write_row",
                  DBUG_RETURN(HA_ERR_INTERNAL_ERROR); );
  DBUG_EXECUTE_IF("simulate_storage_engine_out_of_memory",
                  DBUG_RETURN(HA_ERR_SE_OUT_OF_MEMORY); );
  MYSQL_INSERT_ROW_START(table_share->db.str, table_share->table_name.str);
  mark_trx_read_write();

  DBUG_EXECUTE_IF("handler_crashed_table_on_usage",
                  my_error(HA_ERR_CRASHED, MYF(ME_ERRORLOG), table_share->table_name.str);
                  set_my_errno(HA_ERR_CRASHED);
                  DBUG_RETURN(HA_ERR_CRASHED););

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_WRITE_ROW, MAX_KEY, error,
    { error= write_row(buf); })

  MYSQL_INSERT_ROW_DONE(error);
  if (unlikely(error))
    DBUG_RETURN(error);

  if (unlikely((error= binlog_log_row(table, 0, buf, log_func))))
    DBUG_RETURN(error); /* purecov: inspected */

  DEBUG_SYNC_C("ha_write_row_end");
  DBUG_RETURN(0);
}


int handler::ha_update_row(const uchar *old_data, uchar *new_data)
{
  int error;
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);
  Log_func *log_func= Update_rows_log_event::binlog_row_logging_function;

  /*
    Some storage engines require that the new record is in record[0]
    (and the old record is in record[1]).
   */
  DBUG_ASSERT(new_data == table->record[0]);
  DBUG_ASSERT(old_data == table->record[1]);

  DBUG_ENTER("hanlder::ha_update_row");
  DBUG_EXECUTE_IF("inject_error_ha_update_row",
                  DBUG_RETURN(HA_ERR_INTERNAL_ERROR); );

  MYSQL_UPDATE_ROW_START(table_share->db.str, table_share->table_name.str);
  mark_trx_read_write();

  DBUG_EXECUTE_IF("handler_crashed_table_on_usage",
                  my_error(HA_ERR_CRASHED, MYF(ME_ERRORLOG), table_share->table_name.str);
                  set_my_errno(HA_ERR_CRASHED);
                  return(HA_ERR_CRASHED););

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_UPDATE_ROW, active_index, error,
    { error= update_row(old_data, new_data);})

  MYSQL_UPDATE_ROW_DONE(error);
  if (unlikely(error))
    DBUG_RETURN(error);
  if (unlikely((error= binlog_log_row(table, old_data, new_data, log_func))))
    DBUG_RETURN(error);
  DBUG_RETURN(0);
}

int handler::ha_delete_row(const uchar *buf)
{
  int error;
  DBUG_ASSERT(table_share->tmp_table != NO_TMP_TABLE ||
              m_lock_type == F_WRLCK);
  Log_func *log_func= Delete_rows_log_event::binlog_row_logging_function;
  /*
    Normally table->record[0] is used, but sometimes table->record[1] is used.
  */
  DBUG_ASSERT(buf == table->record[0] ||
              buf == table->record[1]);
  DBUG_EXECUTE_IF("inject_error_ha_delete_row",
                  return HA_ERR_INTERNAL_ERROR; );

  DBUG_EXECUTE_IF("handler_crashed_table_on_usage",
                  my_error(HA_ERR_CRASHED, MYF(ME_ERRORLOG), table_share->table_name.str);
                  set_my_errno(HA_ERR_CRASHED);
                  return(HA_ERR_CRASHED););

  MYSQL_DELETE_ROW_START(table_share->db.str, table_share->table_name.str);
  mark_trx_read_write();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_DELETE_ROW, active_index, error,
    { error= delete_row(buf);})

  MYSQL_DELETE_ROW_DONE(error);
  if (unlikely(error))
    return error;
  if (unlikely((error= binlog_log_row(table, buf, 0, log_func))))
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


/**
  Get an initialized ha_share.

  @return Initialized ha_share
    @retval NULL    ha_share is not yet initialized.
    @retval != NULL previous initialized ha_share.

  @note
  If not a temp table, then LOCK_ha_data must be held.
*/

Handler_share *handler::get_ha_share_ptr()
{
  DBUG_ENTER("handler::get_ha_share_ptr");
  DBUG_ASSERT(ha_share && table_share);

#ifndef DBUG_OFF
  if (table_share->tmp_table == NO_TMP_TABLE)
    mysql_mutex_assert_owner(&table_share->LOCK_ha_data);
#endif

  DBUG_RETURN(*ha_share);
}


/**
  Set ha_share to be used by all instances of the same table/partition.

  @param ha_share    Handler_share to be shared.

  @note
  If not a temp table, then LOCK_ha_data must be held.
*/

void handler::set_ha_share_ptr(Handler_share *arg_ha_share)
{
  DBUG_ENTER("handler::set_ha_share_ptr");
  DBUG_ASSERT(ha_share);
#ifndef DBUG_OFF
  if (table_share->tmp_table == NO_TMP_TABLE)
    mysql_mutex_assert_owner(&table_share->LOCK_ha_data);
#endif

  *ha_share= arg_ha_share;
  DBUG_VOID_RETURN;
}


/**
  Take a lock for protecting shared handler data.
*/

void handler::lock_shared_ha_data()
{
  DBUG_ASSERT(table_share);
  if (table_share->tmp_table == NO_TMP_TABLE)
    mysql_mutex_lock(&table_share->LOCK_ha_data);
}


/**
  Release lock for protecting ha_share.
*/

void handler::unlock_shared_ha_data()
{
  DBUG_ASSERT(table_share);
  if (table_share->tmp_table == NO_TMP_TABLE)
    mysql_mutex_unlock(&table_share->LOCK_ha_data);
}


/**
  This structure is a helper structure for passing the length and pointer of
  blob space allocated by storage engine.
*/
struct blob_len_ptr{
  uint length;  // length of the blob
  uchar *ptr;   // pointer of the value
};


/**
  Get the blob length and pointer of allocated space from the record buffer.

  During evaluating the blob virtual generated columns, the blob space will
  be allocated by server. In order to keep the blob data after the table is
  closed, we need write the data into a specified space allocated by storage
  engine. Here, we have to extract the space pointer and length from the
  record buffer.
  After we get the value of virtual generated columns, copy the data into
  the specified space and store it in the record buffer (@see copy_blob_data()).

  @param table                    the pointer of table
  @param fields                   bitmap of field index of evaluated
                                  generated column
  @param[out] blob_len_ptr_array  an array to record the length and pointer
                                  of allocated space by storage engine.
  @note The caller should provide the blob_len_ptr_array with a size of
        MAX_FIELDS.
*/

static void extract_blob_space_and_length_from_record_buff(const TABLE *table,
                                           const MY_BITMAP *const fields,
                                           blob_len_ptr *blob_len_ptr_array)
{
  int num= 0;
  for (Field **vfield= table->vfield; *vfield; vfield++)
  {
    // Check if this field should be included
    if (bitmap_is_set(fields, (*vfield)->field_index) &&
        (*vfield)->is_virtual_gcol() && (*vfield)->type() == MYSQL_TYPE_BLOB)
    {
      blob_len_ptr_array[num].length= (*vfield)->data_length();
      // TODO: The following check is only for Innodb.
      DBUG_ASSERT(blob_len_ptr_array[num].length == 255 ||
                  blob_len_ptr_array[num].length == 768 ||
                  blob_len_ptr_array[num].length == 3073);

      uchar *ptr;
      (*vfield)->get_ptr(&ptr);
      blob_len_ptr_array[num].ptr= ptr;

      // Let server allocate the space for BLOB virtual generated columns
      (*vfield)->reset();

      num++;
      DBUG_ASSERT(num <= MAX_FIELDS);
    }
  }
}


/**
  Copy the value of BLOB virtual generated columns into the space allocated
  by storage engine.

  This is because the table is closed after evaluating the value. In order to
  keep the BLOB value after the table is closed, we have to copy the value into
  the place where storage engine prepares for.

  @param table              pointer of the table to be operated on
  @param fields             bitmap of field index of evaluated generated column
  @param blob_len_ptr_array array of length and pointer of allocated space by
                            storage engine.
*/

static void copy_blob_data(const TABLE *table,
                           const MY_BITMAP *const fields,
                           blob_len_ptr *blob_len_ptr_array)
{
  uint  num= 0;
  for (Field **vfield= table->vfield; *vfield; vfield++)
  {
    // Check if this field should be included
    if (bitmap_is_set(fields, (*vfield)->field_index) &&
        (*vfield)->is_virtual_gcol() && (*vfield)->type() == MYSQL_TYPE_BLOB)
    {
      DBUG_ASSERT(blob_len_ptr_array[num].length > 0);
      DBUG_ASSERT(blob_len_ptr_array[num].ptr != NULL);

      /*
        Only copy as much of the blob as the storage engine has
        allocated space for. This is sufficient since the only use of the
        blob in the storage engine is for using a prefix of it in a
        secondary index.
      */
      uint length= (*vfield)->data_length();
      const uint alloc_len= blob_len_ptr_array[num].length;
      length= length > alloc_len ? alloc_len : length;

      uchar *ptr;
      (*vfield)->get_ptr(&ptr);
      memcpy(blob_len_ptr_array[num].ptr, ptr, length);
      (down_cast<Field_blob *>(*vfield))->store_in_allocated_space(
                            pointer_cast<char *>(blob_len_ptr_array[num].ptr),
                            length);
      num++;
      DBUG_ASSERT(num <= MAX_FIELDS);
    }
  }
}


/*
  Evaluate generated column's value. This is an internal helper reserved for
  handler::my_eval_gcolumn_expr().

  @param thd        pointer of THD
  @param table      The pointer of table where evaluted generated
                    columns are in
  @param fields     bitmap of field index of evaluated generated column
  @param[in,out] record record buff of base columns generated column depends.
                        After calling this function, it will be used to return
                        the value of generated column.
  @param in_purge   whehter the function is called by purge thread

  @return true in case of error, false otherwise.
*/

static bool my_eval_gcolumn_expr_helper(THD *thd, TABLE *table,
                                        const MY_BITMAP *const fields,
                                        uchar *record,
                                        bool in_purge)
{
  DBUG_ENTER("my_eval_gcolumn_expr_helper");
  DBUG_ASSERT(table && table->vfield);
  DBUG_ASSERT(!thd->is_error());

  uchar *old_buf= table->record[0];
  repoint_field_to_record(table, old_buf, record);

  blob_len_ptr blob_len_ptr_array[MAX_FIELDS];

  /*
    If it's purge thread, we need get the space allocated by storage engine
    for blob.
  */
  if (in_purge)
    extract_blob_space_and_length_from_record_buff(table, fields,
                                                   blob_len_ptr_array);

  bool res= false;
  MY_BITMAP fields_to_evaluate;
  my_bitmap_map bitbuf[bitmap_buffer_size(MAX_FIELDS) / sizeof(my_bitmap_map)];
  bitmap_init(&fields_to_evaluate, bitbuf, table->s->fields, 0);
  bitmap_set_all(&fields_to_evaluate);
  bitmap_intersect(&fields_to_evaluate, fields);
  /*
    In addition to evaluating the value for the columns requested by
    the caller we also need to evaluate any virtual columns that these
    depend on.
    This loop goes through the columns that should be evaluated and
    adds all the base columns. If the base column is virtual, it has
    to be evaluated.
  */
  for (Field **vfield_ptr= table->vfield; *vfield_ptr; vfield_ptr++)
  {
    Field *field= *vfield_ptr;
    // Validate that the field number is less than the bit map size
    DBUG_ASSERT(field->field_index < fields->n_bits);

    if (bitmap_is_set(fields, field->field_index))
      bitmap_union(&fields_to_evaluate, &field->gcol_info->base_columns_map);
  }

   /*
     Evaluate all requested columns and all base columns these depends
     on that are virtual.

     This function is called by the storage engine, which may request to
     evaluate more generated columns than read_set/write_set says.
     For example, InnoDB's row_sel_sec_rec_is_for_clust_rec() reads the full
     record from the clustered index and asks us to compute generated columns
     that match key fields in the used secondary index. So we trust that the
     engine has filled all base columns necessary to requested computations,
     and we ignore read_set/write_set.
  */

  my_bitmap_map *old_maps[2];
  dbug_tmp_use_all_columns(table, old_maps,
                           table->read_set, table->write_set);

  for (Field **vfield_ptr= table->vfield; *vfield_ptr; vfield_ptr++)
  {
    Field *field= *vfield_ptr;

    // Check if we should evaluate this field
    if (bitmap_is_set(&fields_to_evaluate, field->field_index) &&
        field->is_virtual_gcol())
    {
      DBUG_ASSERT(field->gcol_info && field->gcol_info->expr_item->fixed);

      const type_conversion_status save_in_field_status=
        field->gcol_info->expr_item->save_in_field(field, 0);
      DBUG_ASSERT(!thd->is_error() || save_in_field_status != TYPE_OK);

      /*
        save_in_field() may return non-zero even if there was no
        error. This happens if a warning is raised, such as an
        out-of-range warning when converting the result to the target
        type of the virtual column. We should stop only if the
        non-zero return value was caused by an actual error.
      */
      if (save_in_field_status != TYPE_OK && thd->is_error())
      {
        res= true;
        break;
      }
    }
  }

  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_maps);

  /*
    If it's a purge thread, we need copy the blob data into specified place
    allocated by storage engine so that the blob data still can be accessed
    after table is closed.
  */
  if (in_purge)
    copy_blob_data(table, fields, blob_len_ptr_array);

  repoint_field_to_record(table, record, old_buf);
  DBUG_RETURN(res);
}


/**
   Callback to allow InnoDB to prepare a template for generated
   column processing. This function will open the table without
   opening in the engine and call the provided function with
   the TABLE object made. The function will then close the TABLE.

   @param thd            Thread handle
   @param db_name        Name of database containing the table
   @param table_name     Name of table to open
   @param myc            InnoDB function to call for processing TABLE
   @param ib_table       Argument for InnoDB function

   @return true in case of error, false otherwise.
*/

bool handler::my_prepare_gcolumn_template(THD *thd,
                                          const char *db_name,
                                          const char *table_name,
                                          my_gcolumn_template_callback_t myc,
                                          void* ib_table)
{
  char path[FN_REFLEN + 1];
  bool was_truncated;
  build_table_filename(path, sizeof(path) - 1 - reg_ext_length,
                       db_name, table_name, "", 0, &was_truncated);
  DBUG_ASSERT(!was_truncated);
  lex_start(thd);
  bool rc= true;

  // Note! The last argument to open_table_uncached() must be false,
  // since the table already exists in the TDC. Allowing the table to
  // be opened in the SE in this case is dangerous as the two shares
  // could get conflicting SE private data.
  TABLE *table= open_table_uncached(thd, path, db_name, table_name,
                                    false, false);
  if (table)
  {
    myc(table, ib_table);
    intern_close_table(table);
    rc= false;
  }
  lex_end(thd->lex);
  return rc;
}


/**
   Callback for generated columns processing. Will open the table, in the
   server *only*, and call my_eval_gcolumn_expr_helper() to do the actual
   processing. This function is a variant of the other
   handler::my_eval_gcolumn_expr() but is intended for use when no TABLE
   object already exists - e.g. from purge threads.

   Note! The call to open_table_uncached() must be made with the last
   argument (open_in_engine) set to false. Failing to do so will cause
   deadlocks and incorrect behavior.

   @param thd             Thread handle
   @param db_name         Database containing the table to open
   @param table_name      Name of table to open
   @param fields          Bitmap of field index of evaluated generated column
   @param record          Record buffer

   @return true in case of error, false otherwise.
*/

bool handler::my_eval_gcolumn_expr_with_open(THD *thd,
                                             const char *db_name,
                                             const char *table_name,
                                             const MY_BITMAP *const fields,
                                             uchar *record)
{
  bool retval= true;
  lex_start(thd);

  char path[FN_REFLEN + 1];
  bool was_truncated;
  build_table_filename(path, sizeof(path) - 1 - reg_ext_length,
                       db_name, table_name, "", 0, &was_truncated);
  DBUG_ASSERT(!was_truncated);

  TABLE *table= open_table_uncached(thd, path, db_name, table_name,
                                    false, false);
  if (table)
  {
    retval= my_eval_gcolumn_expr_helper(thd, table, fields, record, true);
    intern_close_table(table);
  }

  lex_end(thd->lex);
  return retval;
}


/**
  Evaluate generated Column's value. If the engine has to write an index entry
  to its UNDO log (in a DELETE or UPDATE), and the index is on a virtual
  generated column, engine needs to calculate the column's value. This variant
  of handler::my_eval_gcolumn_expr() is used by client threads which have a
  TABLE.

  @param thd        Thread handle
  @param table      mysql table object
  @param fields     bitmap of field index of evaluated
	            generated column
  @param record     buff of base columns generated column depends.
                    After calling this function, it will be used to
                    return the value of generated column.

  @retval true in case of error
  @retval false on success.
*/

bool handler::my_eval_gcolumn_expr(THD *thd, TABLE *table,
				   const MY_BITMAP *const fields,
                                   uchar *record)
{
  DBUG_ENTER("my_eval_gcolumn_expr");

  const bool res=
     my_eval_gcolumn_expr_helper(thd, table, fields, record, false);
  DBUG_RETURN(res);
}


/**
  Auxiliary structure for passing information to notify_*_helper()
  functions.
*/

struct HTON_NOTIFY_PARAMS
{
  HTON_NOTIFY_PARAMS(const MDL_key *mdl_key,
                     ha_notification_type mdl_type)
    : key(mdl_key), notification_type(mdl_type),
      some_htons_were_notified(false),
      victimized(false)
  {}

  const MDL_key *key;
  const ha_notification_type notification_type;
  bool some_htons_were_notified;
  bool victimized;
};


static my_bool
notify_exclusive_mdl_helper(THD *thd, plugin_ref plugin, void *arg)
{
  handlerton *hton= plugin_data<handlerton*>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->notify_exclusive_mdl)
  {
    HTON_NOTIFY_PARAMS *params= reinterpret_cast<HTON_NOTIFY_PARAMS*>(arg);

    if (hton->notify_exclusive_mdl(thd, params->key,
                                   params->notification_type,
                                   &params->victimized))
    {
      // Ignore failures from post event notification.
      if (params->notification_type == HA_NOTIFY_PRE_EVENT)
        return TRUE;
    }
    else
      params->some_htons_were_notified= true;
  }
  return FALSE;
}


/**
  Notify/get permission from all interested storage engines before
  acquisition or after release of exclusive metadata lock on object
  represented by key.

  @param thd                Thread context.
  @param mdl_key            MDL key identifying object on which exclusive
                            lock is to be acquired/was released.
  @param notification_type  Indicates whether this is pre-acquire or
                            post-release notification.
  @param victimized        'true' if locking failed as we were selected
                            as a victim in order to avoid possible deadlocks.

  @note @see handlerton::notify_exclusive_mdl for details about
        calling convention and error reporting.

  @return False - if notification was successful/lock can be acquired,
          True - if it has failed/lock should not be acquired.
*/

bool ha_notify_exclusive_mdl(THD *thd, const MDL_key *mdl_key,
                             ha_notification_type notification_type,
                             bool *victimized)
{
  HTON_NOTIFY_PARAMS params(mdl_key, notification_type);
  *victimized = false;
  if (plugin_foreach(thd, notify_exclusive_mdl_helper,
                     MYSQL_STORAGE_ENGINE_PLUGIN, &params))
  {
    *victimized = params.victimized;
    /*
      If some SE hasn't given its permission to acquire lock and some SEs
      has given their permissions, we need to notify the latter group about
      failed lock acquisition. We do this by calling post-release notification
      for all interested SEs unconditionally.
    */
    if (notification_type == HA_NOTIFY_PRE_EVENT &&
        params.some_htons_were_notified)
    {
      HTON_NOTIFY_PARAMS rollback_params(mdl_key, HA_NOTIFY_POST_EVENT);
      (void) plugin_foreach(thd, notify_exclusive_mdl_helper,
                            MYSQL_STORAGE_ENGINE_PLUGIN, &rollback_params);
    }
    return true;
  }
  return false;
}


static my_bool
notify_alter_table_helper(THD *thd, plugin_ref plugin, void *arg)
{
  handlerton *hton= plugin_data<handlerton*>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->notify_alter_table)
  {
    HTON_NOTIFY_PARAMS *params= reinterpret_cast<HTON_NOTIFY_PARAMS*>(arg);

    if (hton->notify_alter_table(thd, params->key, params->notification_type))
    {
      // Ignore failures from post event notification.
      if (params->notification_type == HA_NOTIFY_PRE_EVENT)
        return TRUE;
    }
    else
      params->some_htons_were_notified= true;
  }
  return FALSE;
}


/**
  Notify/get permission from all interested storage engines before
  or after executed ALTER TABLE on the table identified by key.

  @param thd                Thread context.
  @param mdl_key            MDL key identifying table.
  @param notification_type  Indicates whether this is pre-ALTER or
                            post-ALTER notification.

  @note @see handlerton::notify_alter_table for rationale,
        details about calling convention and error reporting.

  @return False - if notification was successful/ALTER TABLE can
                  proceed.
          True -  if it has failed/ALTER TABLE should fail.
*/

bool ha_notify_alter_table(THD *thd, const MDL_key *mdl_key,
                           ha_notification_type notification_type)
{
  HTON_NOTIFY_PARAMS params(mdl_key, notification_type);

  if (plugin_foreach(thd, notify_alter_table_helper,
                     MYSQL_STORAGE_ENGINE_PLUGIN, &params))
  {
    /*
      If some SE hasn't given its permission to do ALTER TABLE and some SEs
      has given their permissions, we need to notify the latter group about
      failed attemopt. We do this by calling post-ALTER TABLE notification
      for all interested SEs unconditionally.
    */
    if (notification_type == HA_NOTIFY_PRE_EVENT &&
        params.some_htons_were_notified)
    {
      HTON_NOTIFY_PARAMS rollback_params(mdl_key, HA_NOTIFY_POST_EVENT);
      (void) plugin_foreach(thd, notify_alter_table_helper,
                            MYSQL_STORAGE_ENGINE_PLUGIN, &rollback_params);
    }
    return true;
  }
  return false;
}

/**
  Set the transaction isolation level for the next transaction and update
  session tracker information about the transaction isolation level.

  @param thd           THD session setting the tx_isolation.
  @param tx_isolation  The isolation level to be set.
  @param one_shot      True if the isolation level should be restored to
                       session default after finishing the transaction.
*/
bool set_tx_isolation(THD *thd,
                      enum_tx_isolation tx_isolation,
                      bool one_shot)
{
  Transaction_state_tracker *tst= NULL;

  if (thd->variables.session_track_transaction_info > TX_TRACK_NONE)
    tst= (Transaction_state_tracker *)
           thd->session_tracker.get_tracker(TRANSACTION_INFO_TRACKER);

  thd->tx_isolation= tx_isolation;

  if (one_shot)
  {
    DBUG_ASSERT(!thd->in_active_multi_stmt_transaction());
    DBUG_ASSERT(!thd->in_sub_stmt);
    enum enum_tx_isol_level l;
    switch (thd->tx_isolation) {
    case ISO_READ_UNCOMMITTED:
      l=  TX_ISOL_UNCOMMITTED;
      break;
    case ISO_READ_COMMITTED:
      l=  TX_ISOL_COMMITTED;
      break;
    case ISO_REPEATABLE_READ:
      l= TX_ISOL_REPEATABLE;
      break;
    case ISO_SERIALIZABLE:
      l= TX_ISOL_SERIALIZABLE;
      break;
    default:
      DBUG_ASSERT(0);
      return true;
    }
    if (tst)
      tst->set_isol_level(thd, l);
  }
  else if (tst)
  {
    tst->set_isol_level(thd, TX_ISOL_INHERIT);
  }
  return false;
}
