/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file sql/handler.cc

    @brief
    Implements functions in the handler interface that are shared between all
    storage engines.
*/

#include "sql/handler.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <list>
#include <random>  // std::uniform_real_distribution
#include <string>
#include <string_view>
#include <vector>

#include "keycache.h"
#include "libbinlogevents/include/binlog_event.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_bit.h"     // my_count_bits
#include "my_bitmap.h"  // MY_BITMAP
#include "my_check_opt.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_psi_config.h"
#include "my_sqlcommand.h"
#include "my_sys.h"  // MEM_DEFINED_IF_ADDRESSABLE()
#include "myisam.h"  // TT_FOR_UPGRADE
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_table.h"
#include "mysql/psi/mysql_transaction.h"
#include "mysql/psi/psi_table.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql_com.h"
#include "mysql_version.h"  // MYSQL_VERSION_ID
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "sql/auth/auth_common.h"  // check_readonly() and SUPER_ACL
#include "sql/binlog.h"            // mysql_bin_log
#include "sql/check_stack.h"
#include "sql/clone_handler.h"
#include "sql/current_thd.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd.h"                       // dd::get_dictionary
#include "sql/dd/dictionary.h"               // dd:acquire_shared_table_mdl
#include "sql/dd/types/table.h"              // dd::Table
#include "sql/dd_table_share.h"              // open_table_def
#include "sql/debug_sync.h"                  // DEBUG_SYNC
#include "sql/derror.h"                      // ER_DEFAULT
#include "sql/error_handler.h"               // Internal_error_handler
#include "sql/field.h"
#include "sql/item.h"
#include "sql/lock.h"  // MYSQL_LOCK
#include "sql/log.h"
#include "sql/log_event.h"  // Write_rows_log_event
#include "sql/mdl.h"
#include "sql/mysqld.h"                 // global_system_variables heap_hton ..
#include "sql/opt_costconstantcache.h"  // reload_optimizer_cost_constants
#include "sql/opt_costmodel.h"
#include "sql/opt_hints.h"
#include "sql/protocol.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/record_buffer.h"  // Record_buffer
#include "sql/rpl_filter.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_handler.h"                       // RUN_HOOK
#include "sql/rpl_replica_commit_order_manager.h"  // Commit_order_manager
#include "sql/rpl_rli.h"                // is_atomic_ddl_commit_on_slave
#include "sql/rpl_write_set_handler.h"  // add_pke
#include "sql/sdi_utils.h"              // import_serialized_meta_data
#include "sql/session_tracker.h"
#include "sql/sql_base.h"  // free_io_cache
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"   // check_stack_overrun
#include "sql/sql_plugin.h"  // plugin_foreach
#include "sql/sql_select.h"  // actual_key_parts
#include "sql/sql_table.h"   // build_table_filename
#include "sql/strfunc.h"     // strnncmp_nopads
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/tc_log.h"
#include "sql/thr_malloc.h"
#include "sql/transaction.h"  // trans_commit_implicit
#include "sql/transaction_info.h"
#include "sql/xa.h"
#include "sql/xa/sql_cmd_xa.h"  // Sql_cmd_xa_*
#include "sql_string.h"
#include "sql_tmp_table.h"  // free_tmp_table
#include "template_utils.h"
#include "uniques.h"  // Unique_on_insert
#include "varlen_sort.h"

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
  @param RESULT the result of the table operation performed
  @param PAYLOAD instrumented code to execute
  @sa handler::end_psi_batch_mode.
*/
#ifdef HAVE_PSI_TABLE_INTERFACE
#define MYSQL_TABLE_IO_WAIT(OP, INDEX, RESULT, PAYLOAD)                     \
  {                                                                         \
    if (m_psi != NULL) {                                                    \
      switch (m_psi_batch_mode) {                                           \
        case PSI_BATCH_MODE_NONE: {                                         \
          PSI_table_locker *sub_locker = NULL;                              \
          PSI_table_locker_state reentrant_safe_state;                      \
          reentrant_safe_state.m_thread = nullptr;                          \
          reentrant_safe_state.m_wait = nullptr;                            \
          sub_locker = PSI_TABLE_CALL(start_table_io_wait)(                 \
              &reentrant_safe_state, m_psi, OP, INDEX, __FILE__, __LINE__); \
          PAYLOAD                                                           \
          if (sub_locker != NULL) PSI_TABLE_CALL(end_table_io_wait)         \
          (sub_locker, 1);                                                  \
          break;                                                            \
        }                                                                   \
        case PSI_BATCH_MODE_STARTING: {                                     \
          m_psi_locker = PSI_TABLE_CALL(start_table_io_wait)(               \
              &m_psi_locker_state, m_psi, OP, INDEX, __FILE__, __LINE__);   \
          PAYLOAD                                                           \
          if (RESULT != HA_ERR_END_OF_FILE) m_psi_numrows++;                \
          m_psi_batch_mode = PSI_BATCH_MODE_STARTED;                        \
          break;                                                            \
        }                                                                   \
        case PSI_BATCH_MODE_STARTED:                                        \
        default: {                                                          \
          assert(m_psi_batch_mode == PSI_BATCH_MODE_STARTED);               \
          PAYLOAD                                                           \
          if (RESULT != HA_ERR_END_OF_FILE) m_psi_numrows++;                \
          break;                                                            \
        }                                                                   \
      }                                                                     \
    } else {                                                                \
      PAYLOAD                                                               \
    }                                                                       \
  }
#else
#define MYSQL_TABLE_IO_WAIT(OP, INDEX, RESULT, PAYLOAD) PAYLOAD
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
#define MYSQL_TABLE_LOCK_WAIT(OP, FLAGS, PAYLOAD)                              \
  {                                                                            \
    if (m_psi != NULL) {                                                       \
      PSI_table_locker *locker;                                                \
      PSI_table_locker_state state;                                            \
      locker = PSI_TABLE_CALL(start_table_lock_wait)(&state, m_psi, OP, FLAGS, \
                                                     __FILE__, __LINE__);      \
      PAYLOAD                                                                  \
      if (locker != NULL) PSI_TABLE_CALL(end_table_lock_wait)(locker);         \
    } else {                                                                   \
      PAYLOAD                                                                  \
    }                                                                          \
  }
#else
#define MYSQL_TABLE_LOCK_WAIT(OP, FLAGS, PAYLOAD) PAYLOAD
#endif

using std::list;
using std::log2;
using std::max;
using std::min;

/**
  While we have legacy_db_type, we have this array to
  check for dups and to find handlerton from legacy_db_type.
  Remove when legacy_db_type is finally gone
*/
static Prealloced_array<st_plugin_int *, PREALLOC_NUM_HA> se_plugin_array(
    PSI_NOT_INSTRUMENTED);

/**
  Array allowing to check if handlerton is builtin without
  acquiring LOCK_plugin.
*/
static Prealloced_array<bool, PREALLOC_NUM_HA> builtin_htons(
    PSI_NOT_INSTRUMENTED);

st_plugin_int *hton2plugin(uint slot) { return se_plugin_array[slot]; }

size_t num_hton2plugins() { return se_plugin_array.size(); }

st_plugin_int *insert_hton2plugin(uint slot, st_plugin_int *plugin) {
  if (se_plugin_array.assign_at(slot, plugin)) return nullptr;
  builtin_htons.assign_at(slot, true);
  return se_plugin_array[slot];
}

st_plugin_int *remove_hton2plugin(uint slot) {
  st_plugin_int *retval = se_plugin_array[slot];
  se_plugin_array[slot] = NULL;
  builtin_htons.assign_at(slot, false);
  return retval;
}

const char *ha_resolve_storage_engine_name(const handlerton *db_type) {
  return db_type == nullptr ? "UNKNOWN" : hton2plugin(db_type->slot)->name.str;
}

static handlerton *installed_htons[128];

/* number of storage engines (from installed_htons[]) that support 2pc */
ulong total_ha_2pc = 0;
/* size of savepoint storage area (see ha_init) */
ulong savepoint_alloc_size = 0;

namespace {
struct Storage_engine_identifier {
  const LEX_CSTRING canonical;
  const LEX_CSTRING legacy;
};
const Storage_engine_identifier se_names[] = {
    {{STRING_WITH_LEN("INNODB")}, {STRING_WITH_LEN("INNOBASE")}},
    {{STRING_WITH_LEN("NDBCLUSTER")}, {STRING_WITH_LEN("NDB")}},
    {{STRING_WITH_LEN("MEMORY")}, {STRING_WITH_LEN("HEAP")}},
    {{STRING_WITH_LEN("MRG_MYISAM")}, {STRING_WITH_LEN("MERGE")}}};
const auto se_names_end = std::end(se_names);
std::vector<std::string> disabled_se_names;
}  // namespace

const char *ha_row_type[] = {"",
                             "FIXED",
                             "DYNAMIC",
                             "COMPRESSED",
                             "REDUNDANT",
                             "COMPACT",
                             /* Reserved to be "PAGE" in future versions */ "?",
                             "?",
                             "?",
                             "?"};

const char *tx_isolation_names[] = {"READ-UNCOMMITTED", "READ-COMMITTED",
                                    "REPEATABLE-READ", "SERIALIZABLE", NullS};
TYPELIB tx_isolation_typelib = {array_elements(tx_isolation_names) - 1, "",
                                tx_isolation_names, nullptr};

// Called for each SE to check if given db.table_name is a system table.
static bool check_engine_system_table_handlerton(THD *unused, plugin_ref plugin,
                                                 void *arg);

static int ha_discover(THD *thd, const char *db, const char *name,
                       uchar **frmblob, size_t *frmlen);

/**
  Structure used by SE during check for system table.
  This structure is passed to each SE handlerton and the status (OUT param)
  is collected.
*/
struct st_sys_tbl_chk_params {
  const char *db;                  // IN param
  const char *table_name;          // IN param
  bool is_sql_layer_system_table;  // IN param
  legacy_db_type db_type;          // IN param

  enum enum_sys_tbl_chk_status {
    // db.table_name is not a supported system table.
    NOT_KNOWN_SYSTEM_TABLE,
    /*
      db.table_name is a system table,
      but may not be supported by SE.
    */
    KNOWN_SYSTEM_TABLE,
    /*
      db.table_name is a system table,
      and is supported by SE.
    */
    SUPPORTED_SYSTEM_TABLE
  } status;  // OUT param
};

static plugin_ref ha_default_plugin(THD *thd) {
  if (thd->variables.table_plugin) return thd->variables.table_plugin;
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
handlerton *ha_default_handlerton(THD *thd) {
  plugin_ref plugin = ha_default_plugin(thd);
  assert(plugin);
  handlerton *hton = plugin_data<handlerton *>(plugin);
  assert(hton);
  return hton;
}

static plugin_ref ha_default_temp_plugin(THD *thd) {
  if (thd->variables.temp_table_plugin) return thd->variables.temp_table_plugin;
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
handlerton *ha_default_temp_handlerton(THD *thd) {
  plugin_ref plugin = ha_default_temp_plugin(thd);
  assert(plugin);
  handlerton *hton = plugin_data<handlerton *>(plugin);
  assert(hton);
  return hton;
}

/**
  Resolve handlerton plugin by name, without checking for "DEFAULT" or
  HTON_NOT_USER_SELECTABLE.

  @param thd  Thread context.
  @param name Plugin name.

  @return plugin or NULL if not found.
*/
plugin_ref ha_resolve_by_name_raw(THD *thd, const LEX_CSTRING &name) {
  return plugin_lock_by_name(thd, name, MYSQL_STORAGE_ENGINE_PLUGIN);
}

static const CHARSET_INFO &hton_charset() { return *system_charset_info; }

/**
  Return the storage engine handlerton for the supplied name.

  @param thd           Current thread. May be nullptr, (e.g. during initialize).
  @param name          Name of storage engine.
  @param is_temp_table true if table is a temporary table.

  @return Pointer to storage engine plugin handle.
*/
plugin_ref ha_resolve_by_name(THD *thd, const LEX_CSTRING *name,
                              bool is_temp_table) {
  if (thd && 0 == strnncmp_nopads(hton_charset(), *name,
                                  {STRING_WITH_LEN("DEFAULT")})) {
    return is_temp_table ? ha_default_plugin(thd) : ha_default_temp_plugin(thd);
  }

  // Note that thd CAN be nullptr here - it is not actually needed by
  // ha_resolve_by_name_raw().
  plugin_ref plugin = ha_resolve_by_name_raw(thd, *name);
  if (plugin == nullptr) {
    // If we fail to resolve the name passed in, we try to see if it is a
    // historical alias.
    auto match = std::find_if(
        std::begin(se_names), se_names_end,
        [&](const Storage_engine_identifier &sei) {
          return (0 == strnncmp_nopads(hton_charset(), *name, sei.legacy));
        });
    if (match != se_names_end) {
      // if it is, we resolve using the new name
      plugin = ha_resolve_by_name_raw(thd, match->canonical);
    }
  }
  if (plugin != nullptr) {
    handlerton *hton = plugin_data<handlerton *>(plugin);
    if (hton && !(hton->flags & HTON_NOT_USER_SELECTABLE)) return plugin;

    /*
      unlocking plugin immediately after locking is relatively low cost.
    */
    plugin_unlock(thd, plugin);
  }
  return nullptr;
}

bool ha_secondary_engine_supports_ddl(
    THD *thd, const LEX_CSTRING &secondary_engine) noexcept {
  bool ret = false;
  auto *plugin = ha_resolve_by_name_raw(thd, secondary_engine);

  if (plugin != nullptr) {
    const auto *se_hton = plugin_data<const handlerton *>(plugin);
    if (se_hton != nullptr) {
      ret = secondary_engine_supports_ddl(se_hton);
    }

    plugin_unlock(thd, plugin);
  }
  return ret;
}

/**
  Read a comma-separated list of storage engine names. Look up each in the
  known list of canonical and legacy names. In case of a match; add both the
  canonical and the legacy name to disabled_se_names, which is a static vector
  of disabled storage engine names.
  If there is no match, the unmodified name is added to the vector.
*/
void set_externally_disabled_storage_engine_names(const char *disabled_list) {
  assert(disabled_list != nullptr);

  myu::Split(
      disabled_list, disabled_list + strlen(disabled_list), myu::IsComma,
      [](const char *f, const char *l) {
        auto tr = myu::FindTrimmedRange(f, l, myu::IsSpace);
        if (tr.first == tr.second) return;

        const LEX_CSTRING dse{tr.first,
                              static_cast<size_t>(tr.second - tr.first)};
        auto match = std::find_if(
            std::begin(se_names), se_names_end,
            [&](const Storage_engine_identifier &seid) {
              return (
                  (0 == strnncmp_nopads(hton_charset(), dse, seid.canonical)) ||
                  (0 == strnncmp_nopads(hton_charset(), dse, seid.legacy)));
            });
        if (match == se_names_end) {
          disabled_se_names.emplace_back(dse.str, dse.length);
          return;
        }
        disabled_se_names.emplace_back(match->canonical.str,
                                       match->canonical.length);
        disabled_se_names.emplace_back(match->legacy.str, match->legacy.length);
      });
}

static bool is_storage_engine_name_externally_disabled(const char *name) {
  const LEX_CSTRING n{name, strlen(name)};
  return std::any_of(
      disabled_se_names.begin(), disabled_se_names.end(),
      [&](const std::string &dse) {
        return (0 == strnncmp_nopads(hton_charset(), n,
                                     {dse.c_str(), dse.length()}));
      });
}

/**
  Returns true if the storage engine of the handlerton argument has
  been listed in the disabled_storage_engines system variable. @note
  that the SE may still be internally enabled, that is
  HaIsInternallyEnabled may return true.
 */
bool ha_is_externally_disabled(const handlerton &htnr) {
  const char *se_name = ha_resolve_storage_engine_name(&htnr);
  assert(se_name != nullptr);
  return is_storage_engine_name_externally_disabled(se_name);
}

// Check if storage engine is disabled for table/tablespace creation.
bool ha_is_storage_engine_disabled(handlerton *se_handle) {
  assert(se_handle != nullptr);
  return ha_is_externally_disabled(*se_handle);
}

plugin_ref ha_lock_engine(THD *thd, const handlerton *hton) {
  if (hton) {
    st_plugin_int **plugin = &se_plugin_array[hton->slot];

#ifdef NDEBUG
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
    if (builtin_htons[hton->slot]) return *plugin;

    return my_plugin_lock(thd, plugin);
#else
    /*
      We can't take shortcut in debug builds.
      At least assert that builtin_htons[slot] is set correctly.
    */
    assert(builtin_htons[hton->slot] == (plugin[0]->plugin_dl == nullptr));
    return my_plugin_lock(thd, &plugin);
#endif
  }
  return nullptr;
}

handlerton *ha_resolve_by_legacy_type(THD *thd, enum legacy_db_type db_type) {
  plugin_ref plugin;
  switch (db_type) {
    case DB_TYPE_DEFAULT:
      return ha_default_handlerton(thd);
    default:
      if (db_type > DB_TYPE_UNKNOWN && db_type < DB_TYPE_DEFAULT &&
          (plugin = ha_lock_engine(thd, installed_htons[db_type])))
        return plugin_data<handlerton *>(plugin);
      [[fallthrough]];
    case DB_TYPE_UNKNOWN:
      return nullptr;
  }
}

/**
  Use other database handler if databasehandler is not compiled in.
*/
handlerton *ha_checktype(THD *thd, enum legacy_db_type database_type,
                         bool no_substitute, bool report_error) {
  DBUG_TRACE;
  handlerton *hton = ha_resolve_by_legacy_type(thd, database_type);
  if (ha_storage_engine_is_enabled(hton)) return hton;

  if (no_substitute) {
    if (report_error) {
      const char *engine_name = ha_resolve_storage_engine_name(hton);
      my_error(ER_FEATURE_DISABLED, MYF(0), engine_name, engine_name);
    }
    return nullptr;
  }

  (void)RUN_HOOK(transaction, after_rollback, (thd, false));

  switch (database_type) {
    case DB_TYPE_MRG_ISAM:
      return ha_resolve_by_legacy_type(thd, DB_TYPE_MRG_MYISAM);
    default:
      break;
  }

  return ha_default_handlerton(thd);
} /* ha_checktype */

/**
  Create handler object for the table in the storage engine.

  @param share        TABLE_SHARE for the table, can be NULL if caller
                      didn't perform full-blown open of table definition.
  @param partitioned  Indicates whether table is partitioned.
  @param alloc        Memory root to be used for allocating handler object.
  @param db_type      Table's storage engine.

  @note This function will try to use default storage engine if one which
        was specified through db_type parameter is not available.
*/
handler *get_new_handler(TABLE_SHARE *share, bool partitioned, MEM_ROOT *alloc,
                         handlerton *db_type) {
  handler *file;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("alloc: %p", alloc));

  if (db_type && db_type->state == SHOW_OPTION_YES && db_type->create) {
    if ((file = db_type->create(db_type, share, partitioned, alloc)))
      file->init();
    return file;
  }
  /*
    Try the default table type
    Here the call to current_thd() is ok as we call this function a lot of
    times but we enter this branch very seldom.
  */
  return get_new_handler(share, partitioned, alloc,
                         ha_default_handlerton(current_thd));
}

static const char **handler_errmsgs;

static const char *get_handler_errmsg(int nr) {
  return handler_errmsgs[nr - HA_ERR_FIRST];
}

/**
  Register handler error messages for use with my_error().

  @retval
    0           OK
  @retval
    !=0         Error
*/

int ha_init_errors(void) {
#define SETMSG(nr, msg) handler_errmsgs[(nr)-HA_ERR_FIRST] = (msg)

  /* Allocate a pointer array for the error message strings. */
  /* Zerofill it to avoid uninitialized gaps. */
  if (!(handler_errmsgs = static_cast<const char **>(my_malloc(
            key_memory_errmsgs_handler, HA_ERR_ERRORS * sizeof(char *),
            MYF(MY_WME | MY_ZEROFILL)))))
    return 1;

  /* Set the dedicated error messages. */
  SETMSG(HA_ERR_KEY_NOT_FOUND, ER_DEFAULT(ER_KEY_NOT_FOUND));
  SETMSG(HA_ERR_FOUND_DUPP_KEY, ER_DEFAULT(ER_DUP_KEY));
  SETMSG(HA_ERR_RECORD_CHANGED, "Update wich is recoverable");
  SETMSG(HA_ERR_WRONG_INDEX, "Wrong index given to function");
  SETMSG(HA_ERR_CRASHED, ER_DEFAULT(ER_NOT_KEYFILE));
  SETMSG(HA_ERR_WRONG_IN_RECORD, ER_DEFAULT(ER_CRASHED_ON_USAGE));
  SETMSG(HA_ERR_OUT_OF_MEM, "Table handler out of memory");
  SETMSG(HA_ERR_NOT_A_TABLE, "Incorrect file format '%.64s'");
  SETMSG(HA_ERR_WRONG_COMMAND, "Command not supported");
  SETMSG(HA_ERR_OLD_FILE, ER_DEFAULT(ER_OLD_KEYFILE));
  SETMSG(HA_ERR_NO_ACTIVE_RECORD, "No record read in update");
  SETMSG(HA_ERR_RECORD_DELETED, "Intern record deleted");
  SETMSG(HA_ERR_RECORD_FILE_FULL, ER_DEFAULT(ER_RECORD_FILE_FULL));
  SETMSG(HA_ERR_INDEX_FILE_FULL, "No more room in index file '%.64s'");
  SETMSG(HA_ERR_END_OF_FILE, "End in next/prev/first/last");
  SETMSG(HA_ERR_UNSUPPORTED, ER_DEFAULT(ER_ILLEGAL_HA));
  SETMSG(HA_ERR_TOO_BIG_ROW, "Too big row");
  SETMSG(HA_WRONG_CREATE_OPTION, "Wrong create option");
  SETMSG(HA_ERR_FOUND_DUPP_UNIQUE, ER_DEFAULT(ER_DUP_UNIQUE));
  SETMSG(HA_ERR_UNKNOWN_CHARSET, "Can't open charset");
  SETMSG(HA_ERR_WRONG_MRG_TABLE_DEF, ER_DEFAULT(ER_WRONG_MRG_TABLE));
  SETMSG(HA_ERR_CRASHED_ON_REPAIR, ER_DEFAULT(ER_CRASHED_ON_REPAIR));
  SETMSG(HA_ERR_CRASHED_ON_USAGE, ER_DEFAULT(ER_CRASHED_ON_USAGE));
  SETMSG(HA_ERR_LOCK_WAIT_TIMEOUT, ER_DEFAULT(ER_LOCK_WAIT_TIMEOUT));
  SETMSG(HA_ERR_LOCK_TABLE_FULL, ER_DEFAULT(ER_LOCK_TABLE_FULL));
  SETMSG(HA_ERR_READ_ONLY_TRANSACTION, ER_DEFAULT(ER_READ_ONLY_TRANSACTION));
  SETMSG(HA_ERR_LOCK_DEADLOCK, ER_DEFAULT(ER_LOCK_DEADLOCK));
  SETMSG(HA_ERR_CANNOT_ADD_FOREIGN, ER_DEFAULT(ER_CANNOT_ADD_FOREIGN));
  SETMSG(HA_ERR_NO_REFERENCED_ROW, ER_DEFAULT(ER_NO_REFERENCED_ROW_2));
  SETMSG(HA_ERR_ROW_IS_REFERENCED, ER_DEFAULT(ER_ROW_IS_REFERENCED_2));
  SETMSG(HA_ERR_NO_SAVEPOINT, "No savepoint with that name");
  SETMSG(HA_ERR_NON_UNIQUE_BLOCK_SIZE, "Non unique key block size");
  SETMSG(HA_ERR_NO_SUCH_TABLE, "No such table: '%.64s'");
  SETMSG(HA_ERR_TABLE_EXIST, ER_DEFAULT(ER_TABLE_EXISTS_ERROR));
  SETMSG(HA_ERR_NO_CONNECTION, "Could not connect to storage engine");
  SETMSG(HA_ERR_TABLE_DEF_CHANGED, ER_DEFAULT(ER_TABLE_DEF_CHANGED));
  SETMSG(HA_ERR_FOREIGN_DUPLICATE_KEY,
         "FK constraint would lead to duplicate key");
  SETMSG(HA_ERR_TABLE_NEEDS_UPGRADE, ER_DEFAULT(ER_TABLE_NEEDS_UPGRADE));
  SETMSG(HA_ERR_TABLE_READONLY, ER_DEFAULT(ER_OPEN_AS_READONLY));
  SETMSG(HA_ERR_AUTOINC_READ_FAILED, ER_DEFAULT(ER_AUTOINC_READ_FAILED));
  SETMSG(HA_ERR_AUTOINC_ERANGE, ER_DEFAULT(ER_WARN_DATA_OUT_OF_RANGE));
  SETMSG(HA_ERR_TOO_MANY_CONCURRENT_TRXS,
         ER_DEFAULT(ER_TOO_MANY_CONCURRENT_TRXS));
  SETMSG(HA_ERR_INDEX_COL_TOO_LONG, ER_DEFAULT(ER_INDEX_COLUMN_TOO_LONG));
  SETMSG(HA_ERR_INDEX_CORRUPT, ER_DEFAULT(ER_INDEX_CORRUPT));
  SETMSG(HA_FTS_INVALID_DOCID, "Invalid InnoDB FTS Doc ID");
  SETMSG(HA_ERR_TABLE_IN_FK_CHECK, ER_DEFAULT(ER_TABLE_IN_FK_CHECK));
  SETMSG(HA_ERR_TABLESPACE_EXISTS, "Tablespace already exists");
  SETMSG(HA_ERR_TABLESPACE_MISSING, ER_DEFAULT(ER_TABLESPACE_MISSING));
  SETMSG(HA_ERR_FTS_EXCEED_RESULT_CACHE_LIMIT,
         "FTS query exceeds result cache limit");
  SETMSG(HA_ERR_TEMP_FILE_WRITE_FAILURE,
         ER_DEFAULT(ER_TEMP_FILE_WRITE_FAILURE));
  SETMSG(HA_ERR_INNODB_FORCED_RECOVERY, ER_DEFAULT(ER_INNODB_FORCED_RECOVERY));
  SETMSG(HA_ERR_FTS_TOO_MANY_WORDS_IN_PHRASE,
         "Too many words in a FTS phrase or proximity search");
  SETMSG(HA_ERR_TABLE_CORRUPT, ER_DEFAULT(ER_TABLE_CORRUPT));
  SETMSG(HA_ERR_TABLESPACE_MISSING, ER_DEFAULT(ER_TABLESPACE_MISSING));
  SETMSG(HA_ERR_TABLESPACE_IS_NOT_EMPTY,
         ER_DEFAULT(ER_TABLESPACE_IS_NOT_EMPTY));
  SETMSG(HA_ERR_WRONG_FILE_NAME, ER_DEFAULT(ER_WRONG_FILE_NAME));
  SETMSG(HA_ERR_NOT_ALLOWED_COMMAND, ER_DEFAULT(ER_NOT_ALLOWED_COMMAND));
  SETMSG(HA_ERR_COMPUTE_FAILED, "Compute virtual column value failed");
  SETMSG(HA_ERR_DISK_FULL_NOWAIT, ER_DEFAULT(ER_DISK_FULL_NOWAIT));
  SETMSG(HA_ERR_NO_SESSION_TEMP, ER_DEFAULT(ER_NO_SESSION_TEMP));
  SETMSG(HA_ERR_WRONG_TABLE_NAME, ER_DEFAULT(ER_WRONG_TABLE_NAME));
  SETMSG(HA_ERR_TOO_LONG_PATH, ER_DEFAULT(ER_TABLE_NAME_CAUSES_TOO_LONG_PATH));
  SETMSG(HA_ERR_FTS_TOO_MANY_NESTED_EXP,
         "Too many nested sub-expressions in a full-text search");
  /* Register the error messages for use with my_error(). */
  return my_error_register(get_handler_errmsg, HA_ERR_FIRST, HA_ERR_LAST);
}

int ha_finalize_handlerton(st_plugin_int *plugin) {
  handlerton *hton = (handlerton *)plugin->data;
  DBUG_TRACE;

  /* hton can be NULL here, if ha_initialize_handlerton() failed. */
  if (!hton) goto end;

  switch (hton->state) {
    case SHOW_OPTION_NO:
    case SHOW_OPTION_DISABLED:
      break;
    case SHOW_OPTION_YES:
      if (installed_htons[hton->db_type] == hton)
        installed_htons[hton->db_type] = nullptr;
      break;
  };

  if (hton->panic) hton->panic(hton, HA_PANIC_CLOSE);

  if (plugin->plugin->deinit) {
    /*
      Today we have no defined/special behavior for uninstalling
      engine plugins.
    */
    DBUG_PRINT("info", ("Deinitializing plugin: '%s'", plugin->name.str));
    if (plugin->plugin->deinit(nullptr)) {
      DBUG_PRINT("warning", ("Plugin '%s' deinit function returned error.",
                             plugin->name.str));
    }
  }

  /*
    In case a plugin is uninstalled and re-installed later, it should
    reuse an array slot. Otherwise the number of uninstall/install
    cycles would be limited.
  */
  if (hton->slot != HA_SLOT_UNDEF) {
    /* Make sure we are not unpluging another plugin */
    assert(se_plugin_array[hton->slot] == plugin);
    assert(hton->slot < se_plugin_array.size());
    se_plugin_array[hton->slot] = NULL;
    builtin_htons[hton->slot] = false; /* Extra correctness. */
  }

  my_free(hton);
  plugin->data = nullptr;
end:
  return 0;
}

int ha_initialize_handlerton(st_plugin_int *plugin) {
  handlerton *hton;
  DBUG_TRACE;
  DBUG_PRINT("plugin", ("initialize plugin: '%s'", plugin->name.str));

  hton = static_cast<handlerton *>(my_malloc(key_memory_handlerton_objects,
                                             sizeof(handlerton),
                                             MYF(MY_WME | MY_ZEROFILL)));

  if (hton == nullptr) {
    LogErr(ERROR_LEVEL, ER_HANDLERTON_OOM, plugin->name.str);
    goto err_no_hton_memory;
  }

  hton->slot = HA_SLOT_UNDEF;
  /* Historical Requirement */
  plugin->data = hton;  // shortcut for the future
  if (plugin->plugin->init && plugin->plugin->init(hton)) {
    LogErr(ERROR_LEVEL, ER_PLUGIN_INIT_FAILED, plugin->name.str);
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
    case SHOW_OPTION_YES: {
      uint tmp;
      ulong fslot;
      /* now check the db_type for conflict */
      if (hton->db_type <= DB_TYPE_UNKNOWN ||
          hton->db_type >= DB_TYPE_DEFAULT || installed_htons[hton->db_type]) {
        int idx = (int)DB_TYPE_FIRST_DYNAMIC;

        while (idx < (int)DB_TYPE_DEFAULT && installed_htons[idx]) idx++;

        if (idx == (int)DB_TYPE_DEFAULT) {
          LogErr(WARNING_LEVEL, ER_TOO_MANY_STORAGE_ENGINES);
          goto err_deinit;
        }
        if (hton->db_type != DB_TYPE_UNKNOWN)
          LogErr(WARNING_LEVEL, ER_SE_TYPECODE_CONFLICT, plugin->plugin->name,
                 idx);
        hton->db_type = (enum legacy_db_type)idx;
      }

      /*
        In case a plugin is uninstalled and re-installed later, it should
        reuse an array slot. Otherwise the number of uninstall/install
        cycles would be limited. So look for a free slot.
      */
      DBUG_PRINT("plugin",
                 ("total_ha: %lu", static_cast<ulong>(se_plugin_array.size())));
      for (fslot = 0; fslot < se_plugin_array.size(); fslot++) {
        if (!se_plugin_array[fslot]) break;
      }
      if (fslot < se_plugin_array.size())
        hton->slot = fslot;
      else {
        hton->slot = se_plugin_array.size();
      }
      if (se_plugin_array.assign_at(hton->slot, plugin) ||
          builtin_htons.assign_at(hton->slot, (plugin->plugin_dl == nullptr)))
        goto err_deinit;

      installed_htons[hton->db_type] = hton;
      tmp = hton->savepoint_offset;
      hton->savepoint_offset = savepoint_alloc_size;
      savepoint_alloc_size += tmp;
      if (hton->prepare) total_ha_2pc++;
      break;
    }
      [[fallthrough]];
    default:
      hton->state = SHOW_OPTION_DISABLED;
      break;
  }

  /*
    This is entirely for legacy. We will create a new "disk based" hton and a
    "memory" hton which will be configurable longterm. We should be able to
    remove partition and myisammrg.
  */
  switch (hton->db_type) {
    case DB_TYPE_HEAP:
      heap_hton = hton;
      break;
    case DB_TYPE_TEMPTABLE:
      temptable_hton = hton;
      break;
    case DB_TYPE_MYISAM:
      myisam_hton = hton;
      break;
    case DB_TYPE_INNODB:
      innodb_hton = hton;
      break;
    default:
      break;
  };

  /*
    Re-load the optimizer cost constants since this storage engine can
    have non-default cost constants.
  */
  reload_optimizer_cost_constants();

  return 0;

err_deinit:
  /*
    Let plugin do its inner deinitialization as plugin->init()
    was successfully called before.
  */
  if (plugin->plugin->deinit) (void)plugin->plugin->deinit(nullptr);

err:
  my_free(hton);
err_no_hton_memory:
  plugin->data = nullptr;
  return 1;
}

int ha_init() {
  int error = 0;
  DBUG_TRACE;

  /*
    Check if there is a transaction-capable storage engine besides the
    binary log.
  */
  opt_using_transactions =
      se_plugin_array.size() > static_cast<ulong>(opt_bin_log);
  savepoint_alloc_size += sizeof(SAVEPOINT);

  return error;
}

void ha_end() {
  // Unregister handler error messages.
  my_error_unregister(HA_ERR_FIRST, HA_ERR_LAST);
  my_free(handler_errmsgs);
}

static bool dropdb_handlerton(THD *, plugin_ref plugin, void *path) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->drop_database)
    hton->drop_database(hton, (char *)path);
  return false;
}

void ha_drop_database(char *path) {
  plugin_foreach(nullptr, dropdb_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, path);
}

static bool closecon_handlerton(THD *thd, plugin_ref plugin, void *) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  /*
    there's no need to rollback here as all transactions must
    be rolled back already
  */
  if (hton->state == SHOW_OPTION_YES && thd_get_ha_data(thd, hton)) {
    if (hton->close_connection) hton->close_connection(hton, thd);
    /* make sure ha_data is reset and ha_data_lock is released */
    thd_set_ha_data(thd, hton, nullptr);
  }
  return false;
}

/**
  @note
    don't bother to rollback here, it's done already
*/
void ha_close_connection(THD *thd) {
  plugin_foreach(thd, closecon_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN,
                 nullptr);
}

static bool kill_handlerton(THD *thd, plugin_ref plugin, void *) {
  handlerton *hton = plugin_data<handlerton *>(plugin);

  if (hton->state == SHOW_OPTION_YES && hton->kill_connection) {
    if (thd_get_ha_data(thd, hton)) hton->kill_connection(hton, thd);
  }

  return false;
}

void ha_kill_connection(THD *thd) {
  plugin_foreach(thd, kill_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, nullptr);
}

/** Invoke handlerton::pre_dd_shutdown() on a plugin.
@param plugin	storage engine plugin
@retval false (always) */
static bool pre_dd_shutdown_handlerton(THD *, plugin_ref plugin, void *) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->pre_dd_shutdown)
    hton->pre_dd_shutdown(hton);
  return false;
}

/** Invoke handlerton::pre_dd_shutdown() on every storage engine plugin. */
void ha_pre_dd_shutdown(void) {
  plugin_foreach(nullptr, pre_dd_shutdown_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, nullptr);
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
                       const ulonglong *trxid [[maybe_unused]]) {
  Ha_trx_info *ha_info;
  Transaction_ctx *trn_ctx = thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope =
      all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;

  DBUG_TRACE;
  DBUG_PRINT("enter", ("%s", all ? "all" : "stmt"));

  if (all) {
    /*
      Ensure no active backup engine data exists, unless the current
      transaction is from replication and in active xa state.
    */
    assert(
        thd->get_ha_data(ht_arg->slot)->ha_ptr_backup == nullptr ||
        (thd->get_transaction()->xid_state()->has_state(XID_STATE::XA_ACTIVE)));
    assert(thd->get_ha_data(ht_arg->slot)->ha_ptr_backup == nullptr ||
           (thd->is_binlog_applier() || thd->slave_thread));

    thd->server_status |= SERVER_STATUS_IN_TRANS;
    if (thd->tx_read_only)
      thd->server_status |= SERVER_STATUS_IN_TRANS_READONLY;
    DBUG_PRINT("info", ("setting SERVER_STATUS_IN_TRANS"));
  }

  ha_info = thd->get_ha_data(ht_arg->slot)->ha_info + (all ? 1 : 0);

  if (ha_info->is_started()) {
    assert(trn_ctx->ha_trx_info(trx_scope));
    return; /* already registered, return */
  }

  trn_ctx->register_ha(trx_scope, ha_info, ht_arg);
  trn_ctx->set_ha_trx_info(trx_scope, ha_info);

  if (ht_arg->prepare == nullptr) trn_ctx->set_no_2pc(trx_scope, true);

  trn_ctx->xid_state()->set_query_id(thd->query_id);
/*
        Register transaction start in performance schema if not done already.
        By doing this, we handle cases when the transaction is started
   implicitly in autocommit=0 mode, and cases when we are in normal autocommit=1
   mode and the executed statement is a single-statement transaction.

        Explicitly started transactions are handled in trans_begin().

        Do not register transactions in which binary log is the only
   participating transactional storage engine.
*/
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (thd->m_transaction_psi == nullptr && ht_arg->db_type != DB_TYPE_BINLOG &&
      !thd->is_attachable_transaction_active()) {
    const XID *xid = trn_ctx->xid_state()->get_xid();
    bool autocommit = !thd->in_multi_stmt_transaction_mode();
    thd->m_transaction_psi = MYSQL_START_TRANSACTION(
        &thd->m_transaction_state, xid, trxid, thd->tx_isolation,
        thd->tx_read_only, autocommit);
    DEBUG_SYNC(thd, "after_set_transaction_psi_before_set_transaction_gtid");
    gtid_set_performance_schema_values(thd);
  }
#endif
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

static uint ha_check_and_coalesce_trx_read_only(THD *thd,
                                                Ha_trx_info_list &ha_list,
                                                bool all) {
  /* The number of storage engines that have actual changes. */
  unsigned rw_ha_count = 0;

  for (auto const &ha_info : ha_list) {
    if (ha_info.is_trx_read_write()) ++rw_ha_count;

    if (!all) {
      Ha_trx_info *ha_info_all =
          &thd->get_ha_data(ha_info.ht()->slot)->ha_info[1];
      assert(&ha_info != ha_info_all);
      /*
        Merge read-only/read-write information about statement
        transaction to its enclosing normal transaction. Do this
        only if in a real transaction -- that is, if we know
        that ha_info_all is registered in thd->transaction.all.
        Since otherwise we only clutter the normal transaction flags.
      */
      if (ha_info_all->is_started()) /* false if autocommit. */
        ha_info_all->coalesce_trx_with(ha_info);
    } else if (rw_ha_count > 1) {
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
  Determines whether ha_commit_low may invoke commit ordering

  @param[in]  thd  Thread handle.
  @param[in]  all  Is set in case of explicit commit
                   (COMMIT statement), or implicit commit
                   issued by DDL. Is not set when called
                   at the end of statement, even if
                   autocommit=1.
  @retval true ha_commit_low invokes commit order
  @retval false ha_commit_low does not invoke commit order
  @note   Result of has_commit_order_manager() is not taken
          into account here. the calls to
          Commit_order_manager::wait/wait_and_finish() will be
          no-op for threads other than replication applier threads.
  @details Preserve externalization and persistence order for applier
           threads. The conditions should be understood as follows:

    - When the binlog is enabled and binlog local caches contain transaction
      information, ordering is done in MYSQL_BIN_LOG::ordered_commit
      and should be disabled here. Therefore, we have the condition
      thd->is_current_stmt_binlog_log_replica_updates_disabled(). We also
      enable commit ordering in case binlogging is enabled in the current
      call to ha_commit_low (OPT_BIN_LOG bit), but caches are disabled
      or empty (NDB). Please note that it is important to check
      opt_bin_log in is_current_stmt_binlog_log_replica_updates_disabled,
      because of statements such as ALTER TABLE OPTIMIZE PARTITION,
      where the last call to trans_commit_stmt in the
      mysql_inplace_alter_table (Implicit_substatement_guard disabled)
      is not the last call.
      Moreover, there are also cases in which binlog caches were
      emptied after thread entered the ordered_commit function in the
      MYSQL_BIN_LOG. Therefore, condition is checked in commit() function
      and the result is assigned to the is_low_level_commit_ordering_enabled
      flag introduced in the THD.

    - This function is usually called once per statement, with
      all=false.  We should not preserve the commit order when this
      function is called in that context.  Therefore, we have the
      condition ending_trans(thd, all).

    - Statements such as ANALYZE/OPTIMIZE/REPAIR TABLE will call
      ha_commit_low multiple times with all=true from within
      mysql_admin_table, mysql_recreate_table, and
      handle_histogram_command. After returning to
      mysql_execute_command, it will call ha_commit_low one last
      time.  It is only in this final call that we should preserve
      the commit order. Therefore, we set the flag
      thd->is_operating_substatement_implicitly while executing
      mysql_admin_table, mysql_recreate_table, and
      handle_histogram_command, clear it when returning from those
      functions, and check the flag here in ha_commit_low().

    - In all the above cases, we should make the current transaction
      fail early in case a previous transaction has rolled back.
      Therefore, we also invoke the commit order manager in case
      get_rollback_status returns true.

    Note: the calls to Commit_order_manager::wait/wait_and_finish() will be
          no-op for threads other than replication applier threads.
*/
bool is_ha_commit_low_invoking_commit_order(THD *thd, bool all) {
  return (!thd->is_operating_substatement_implicitly &&
          !thd->is_operating_gtid_table_implicitly &&
          (thd->is_current_stmt_binlog_log_replica_updates_disabled() ||
           thd->is_low_level_commit_ordering_enabled()) &&
          ending_trans(thd, all));
}

/**
  The function computes condition to call gtid persistor wrapper,
  and executes it.
  It is invoked at committing a statement or transaction, including XA,
  and also at XA prepare handling.

  @param thd  Thread context.
  @param all  The execution scope, true for the transaction one, false
              for the statement one.

  @return   std::pair containing: Error and Owned GTID release status
   Error
            @retval  0    Ok
            @retval !0    Error

   Owned GTID release status
            @retval  true   remove the GTID owned by thread from owned GTIDs
            @retval  false  removal of the GTID owned by thread from owned GTIDs
                            is not required
*/

std::pair<int, bool> commit_owned_gtids(THD *thd, bool all) {
  DBUG_TRACE;
  int error = 0;
  bool need_clear_owned_gtid = false;

  /*
    If the binary log is disabled for this thread (either by
    log_bin=0 or sql_log_bin=0 or by log_replica_updates=0 for a
    slave thread), then the statement will not be written to
    the binary log. In this case, we should save its GTID into
    mysql.gtid_executed table and @@GLOBAL.GTID_EXECUTED as it
    did when binlog is enabled.

    We also skip saving GTID into mysql.gtid_executed table and
    @@GLOBAL.GTID_EXECUTED when replica-preserve-commit-order is enabled. We
    skip as GTID will be saved in
    Commit_order_manager::flush_engine_and_signal_threads (invoked from
    Commit_order_manager::wait_and_finish). In particular, there is the
    following call stack under ha_commit_low which save GTID in case its skipped
    here:

      ha_commit_low ->
      Commit_order_manager::wait_and_finish ->
      Commit_order_manager::finish ->
      Commit_order_manager::flush_engine_and_signal_threads ->
      Gtid_state::update_commit_group

    We also skip saving GTID for intermediate commits i.e. when
    thd->is_operating_substatement_implicitly is enabled.
  */
  if (is_ha_commit_low_invoking_commit_order(thd, all)) {
    if (!has_commit_order_manager(thd) &&
        (thd->owned_gtid.sidno > 0 ||
         thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS)) {
      need_clear_owned_gtid = true;
    }

    /*
      If GTID is not persisted by SE, write it to
      mysql.gtid_executed table.
    */
    if (thd->owned_gtid.sidno > 0 && !thd->se_persists_gtid()) {
      error = gtid_state->save(thd);
    }
  }

  return std::make_pair(error, need_clear_owned_gtid);
}

/**
  @param[in] thd                       Thread handle.
  @param[in] all                       Session transaction if true, statement
                                       otherwise.
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

int ha_commit_trans(THD *thd, bool all, bool ignore_global_read_lock) {
  int error = 0;
  THD_STAGE_INFO(thd, stage_waiting_for_handler_commit);
  bool run_slave_post_commit = false;
  bool need_clear_owned_gtid = false;
  /*
    Save transaction owned gtid into table before transaction prepare
    if binlog is disabled, or binlog is enabled and log_replica_updates
    is disabled with slave SQL thread or slave worker thread.
  */
  std::tie(error, need_clear_owned_gtid) = commit_owned_gtids(thd, all);

  /*
    'all' means that this is either an explicit commit issued by
    user, or an implicit commit issued by a DDL.
  */
  Transaction_ctx *trn_ctx = thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope =
      all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;

  /*
    "real" is a nick name for a transaction for which a commit will
    make persistent changes. E.g. a 'stmt' transaction inside a 'all'
    transaction is not 'real': even though it's possible to commit it,
    the changes are not durable as they might be rolled back if the
    enclosing 'all' transaction is rolled back.
  */
  bool is_real_trans = all || !trn_ctx->is_active(Transaction_ctx::SESSION);
#ifndef NDEBUG
  bool transaction_to_skip = false;
  DBUG_EXECUTE_IF("replica_crash_after_commit", {
    transaction_to_skip = is_already_logged_transaction(thd);
  });
#endif  // NDEBUG
  auto ha_info = trn_ctx->ha_trx_info(trx_scope);
  XID_STATE *xid_state = trn_ctx->xid_state();

  DBUG_TRACE;

  DBUG_PRINT("info", ("all=%d thd->in_sub_stmt=%d ha_info=%p is_real_trans=%d",
                      all, thd->in_sub_stmt, ha_info.head(), is_real_trans));
  /*
    We must not commit the normal transaction if a statement
    transaction is pending. Otherwise statement transaction
    flags will not get propagated to its normal transaction's
    counterpart.
  */
  assert(!trn_ctx->is_active(Transaction_ctx::STMT) || !all);

  DBUG_EXECUTE_IF("pre_commit_error", {
    error = true;
    my_error(ER_UNKNOWN_ERROR, MYF(0));
  });

  /*
    When atomic DDL is executed on the slave, we would like to
    to update slave applier state as part of DDL's transaction.
    Call Relay_log_info::pre_commit() hook to do this before DDL
    gets committed in the following block.
    Failed atomic DDL statements should've been marked as executed/committed
    during statement rollback, though some like GRANT may continue until
    this point.
    When applying a DDL statement on a slave and the statement is filtered
    out by a table filter, we report an error "ER_SLAVE_IGNORED_TABLE" to
    warn slave applier thread. We need to save the DDL statement's gtid
    into mysql.gtid_executed system table if the binary log is disabled
    on the slave and gtids are enabled.
  */
  if (is_real_trans && is_atomic_ddl_commit_on_slave(thd) &&
      (!thd->is_error() ||
       (thd->is_operating_gtid_table_implicitly &&
        thd->get_stmt_da()->mysql_errno() == ER_SLAVE_IGNORED_TABLE))) {
    run_slave_post_commit = true;
    error = error || thd->rli_slave->pre_commit();

    DBUG_EXECUTE_IF("rli_pre_commit_error", {
      error = true;
      my_error(ER_UNKNOWN_ERROR, MYF(0));
    });
    DBUG_EXECUTE_IF("replica_crash_before_commit", {
      /* This pre-commit crash aims solely at atomic DDL */
      DBUG_SUICIDE();
    });
  }

  if (thd->in_sub_stmt) {
    assert(0);
    /*
      Since we don't support nested statement transactions in 5.0,
      we can't commit or rollback stmt transactions while we are inside
      stored functions or triggers. So we simply do nothing now.
      TODO: This should be fixed in later ( >= 5.1) releases.
    */
    if (!all) return 0;
    /*
      We assume that all statements which commit or rollback main transaction
      are prohibited inside of stored functions or triggers. So they should
      bail out with error even before ha_commit_trans() call. To be 100% safe
      let us throw error in non-debug builds.
    */
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    return 2;
  }

  MDL_request mdl_request;
  bool release_mdl = false;
  if (ha_info && !error) {
    uint rw_ha_count = 0;
    bool rw_trans;

    DBUG_EXECUTE_IF("crash_commit_before", DBUG_SUICIDE(););

    /*
     skip 2PC if the transaction is empty and it is not marked as started (which
     can happen when the slave's binlog is disabled)
    */
    if (ha_info->is_started())
      rw_ha_count = ha_check_and_coalesce_trx_read_only(thd, ha_info, all);
    trn_ctx->set_rw_ha_count(trx_scope, rw_ha_count);
    /* rw_trans is true when we in a transaction changing data */
    rw_trans = is_real_trans && (rw_ha_count > 0);

    DBUG_EXECUTE_IF("dbug.enabled_commit", {
      const char act[] = "now signal Reached wait_for signal.commit_continue";
      assert(!debug_sync_set_action(thd, STRING_WITH_LEN(act)));
    };);
    DEBUG_SYNC(thd, "ha_commit_trans_before_acquire_commit_lock");
    if (rw_trans && !ignore_global_read_lock) {
      /*
        Acquire a metadata lock which will ensure that COMMIT is blocked
        by an active FLUSH TABLES WITH READ LOCK (and vice versa:
        COMMIT in progress blocks FTWRL).

        We allow the owner of FTWRL to COMMIT; we assume that it knows
        what it does.
      */
      MDL_REQUEST_INIT(&mdl_request, MDL_key::COMMIT, "", "",
                       MDL_INTENTION_EXCLUSIVE, MDL_EXPLICIT);

      DBUG_PRINT("debug", ("Acquire MDL commit lock"));
      if (thd->mdl_context.acquire_lock(&mdl_request,
                                        thd->variables.lock_wait_timeout)) {
        ha_rollback_trans(thd, all);
        return 1;
      }
      release_mdl = true;

      DEBUG_SYNC(thd, "ha_commit_trans_after_acquire_commit_lock");
    }

    if (rw_trans && stmt_has_updated_trans_table(ha_info) &&
        check_readonly(thd, true)) {
      ha_rollback_trans(thd, all);
      error = 1;
      goto end;
    }

    if (!trn_ctx->no_2pc(trx_scope) && (trn_ctx->rw_ha_count(trx_scope) > 1))
      error = tc_log->prepare(thd, all);
  }
  /*
    The state of XA transaction is changed to Prepared, intermediately.
    It's going to change to the regular NOTR at the end.
    The fact of the Prepared state is of interest to binary logger.
  */
  if (!error && all && xid_state->has_state(XID_STATE::XA_IDLE)) {
    assert(
        thd->lex->sql_command == SQLCOM_XA_COMMIT &&
        static_cast<Sql_cmd_xa_commit *>(thd->lex->m_sql_cmd)->get_xa_opt() ==
            XA_ONE_PHASE);

    xid_state->set_state(XID_STATE::XA_PREPARED);
  }
  if (error || (error = tc_log->commit(thd, all))) {
    ha_rollback_trans(thd, all);
    error = 1;
    goto end;
  }
/*
        Mark multi-statement (any autocommit mode) or single-statement
        (autocommit=1) transaction as rolled back
*/
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (is_real_trans && thd->m_transaction_psi != nullptr) {
    MYSQL_COMMIT_TRANSACTION(thd->m_transaction_psi);
    thd->m_transaction_psi = nullptr;
  }
#endif
  DBUG_EXECUTE_IF("crash_commit_after",
                  if (!thd->is_operating_gtid_table_implicitly)
                      DBUG_SUICIDE(););
end:
  if (release_mdl && mdl_request.ticket) {
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
  if (is_real_trans) {
    trn_ctx->cleanup();
    thd->tx_priority = 0;
  }

  if (need_clear_owned_gtid) {
    thd->server_status &= ~SERVER_STATUS_IN_TRANS;
    /*
      Release the owned GTID when binlog is disabled, or binlog is
      enabled and log_replica_updates is disabled with slave SQL thread
      or slave worker thread.
    */
    if (error)
      gtid_state->update_on_rollback(thd);
    else
      gtid_state->update_on_commit(thd);
  } else {
    if (has_commit_order_manager(thd) && error) {
      gtid_state->update_on_rollback(thd);
    }
  }
  if (run_slave_post_commit) {
    DBUG_EXECUTE_IF("replica_crash_after_commit", DBUG_SUICIDE(););

    thd->rli_slave->post_commit(error != 0);
    /*
      SERVER_STATUS_IN_TRANS may've been gained by pre_commit alone
      when the main DDL transaction is filtered out of execution.
      In such case the status has to be reset now.

      TODO: move/refactor this handling onto trans_commit/commit_implicit()
            the caller level.
    */
    thd->server_status &= ~SERVER_STATUS_IN_TRANS;
  } else {
    DBUG_EXECUTE_IF("replica_crash_after_commit", {
      if (thd->slave_thread && thd->rli_slave &&
          thd->rli_slave->current_event &&
          thd->rli_slave->current_event->get_type_code() ==
              binary_log::XID_EVENT &&
          !thd->is_operating_substatement_implicitly &&
          !thd->is_operating_gtid_table_implicitly && !transaction_to_skip)
        DBUG_SUICIDE();
    });
  }

  return error;
}

/**
  Commit the sessions outstanding transaction.

  @pre thd->transaction.flags.commit_low == true
  @post thd->transaction.flags.commit_low == false

  @note This function does not care about global read lock; the caller
  should.

  @param[in]  thd  Thread handle.
  @param[in]  all  Is set in case of explicit commit
                   (COMMIT statement), or implicit commit
                   issued by DDL. Is not set when called
                   at the end of statement, even if
                   autocommit=1.
  @param[in]  run_after_commit
                   True by default, otherwise, does not execute
                   the after_commit hook in the function.
*/

int ha_commit_low(THD *thd, bool all, bool run_after_commit) {
  int error = 0;
  Transaction_ctx *trn_ctx = thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope =
      all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;
  auto ha_list = trn_ctx->ha_trx_info(trx_scope);

  DBUG_TRACE;

  if (ha_list) {
    bool restore_backup_ha_data = false;
    /*
      At execution of XA COMMIT ONE PHASE binlog or slave applier
      reattaches the engine ha_data to THD, previously saved at XA START.
    */
    if (all && thd->is_engine_ha_data_detached()) {
      DBUG_PRINT("info", ("query='%s'", thd->query().str));
      assert(thd->lex->sql_command == SQLCOM_XA_COMMIT);
      assert(
          static_cast<Sql_cmd_xa_commit *>(thd->lex->m_sql_cmd)->get_xa_opt() ==
          XA_ONE_PHASE);
      restore_backup_ha_data = true;
    }

    bool is_applier_wait_enabled = false;

    if (is_ha_commit_low_invoking_commit_order(thd, all) ||
        Commit_order_manager::get_rollback_status(thd)) {
      if (Commit_order_manager::wait(thd)) {
        error = 1;
        /*
          Remove applier thread from waiting in Commit Order Queue and
          allow next applier thread to be ordered.
        */
        Commit_order_manager::wait_and_finish(thd, error);
        goto err;
      }
      is_applier_wait_enabled = true;
    }

    for (auto &ha_info : ha_list) {
      int err;
      auto ht = ha_info.ht();
      if ((err = ht->commit(ht, thd, all))) {
        char errbuf[MYSQL_ERRMSG_SIZE];
        my_error(ER_ERROR_DURING_COMMIT, MYF(0), err,
                 my_strerror(errbuf, MYSQL_ERRMSG_SIZE, err));
        error = 1;
      }
      assert(!thd->status_var_aggregated);
      thd->status_var.ha_commit_count++;
      ha_info.reset(); /* keep it conveniently zero-filled */
    }
    if (restore_backup_ha_data) thd->rpl_reattach_engine_ha_data();
    trn_ctx->reset_scope(trx_scope);

    /*
      After ensuring externalization order for applier thread, remove it
      from waiting (Commit Order Queue) and allow next applier thread to
      be ordered.

      Note: the calls to Commit_order_manager::wait_and_finish() will be
            no-op for threads other than replication applier threads.
    */
    if (is_applier_wait_enabled) {
      Commit_order_manager::wait_and_finish(thd, error);
    }
  }

err:
  /* Free resources and perform other cleanup even for 'empty' transactions. */
  if (all) trn_ctx->cleanup();
  /*
    When the transaction has been committed, we clear the commit_low
    flag. This allow other parts of the system to check if commit_low
    was called.
  */
  trn_ctx->m_flags.commit_low = false;
  if (run_after_commit && thd->get_transaction()->m_flags.run_hooks) {
    /*
       If commit succeeded, we call the after_commit hook.

       TODO: Investigate if this can be refactored so that there is
             only one invocation of this hook in the code (in
             MYSQL_LOG_BIN::finish_commit).
    */
    if (!error) (void)RUN_HOOK(transaction, after_commit, (thd, all));
    trn_ctx->m_flags.run_hooks = false;
  }
  return error;
}

int ha_rollback_low(THD *thd, bool all) {
  Transaction_ctx *trn_ctx = thd->get_transaction();
  int error = 0;
  Transaction_ctx::enum_trx_scope trx_scope =
      all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;
  auto ha_list = trn_ctx->ha_trx_info(trx_scope);

  (void)RUN_HOOK(transaction, before_rollback, (thd, all));

  if (ha_list) {
    bool restore_backup_ha_data = false;
    /*
      Similarly to the commit case, the binlog or slave applier
      reattaches the engine ha_data to THD.
    */
    if (all && thd->is_engine_ha_data_detached()) {
      assert(trn_ctx->xid_state()->get_state() != XID_STATE::XA_NOTR ||
             thd->killed == THD::KILL_CONNECTION);

      restore_backup_ha_data = true;
    }

    for (auto &ha_info : ha_list) {
      int err;
      auto ht = ha_info.ht();
      if ((err = ht->rollback(ht, thd, all))) {  // cannot happen
        char errbuf[MYSQL_ERRMSG_SIZE];
        my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err,
                 my_strerror(errbuf, MYSQL_ERRMSG_SIZE, err));
        error = 1;
      }
      assert(!thd->status_var_aggregated);
      thd->status_var.ha_rollback_count++;
      ha_info.reset(); /* keep it conveniently zero-filled */
    }
    if (restore_backup_ha_data) thd->rpl_reattach_engine_ha_data();
    trn_ctx->reset_scope(trx_scope);
  }

  /*
    Thanks to possibility of MDL deadlock rollback request can come even if
    transaction hasn't been started in any transactional storage engine.

    It is possible to have a call of ha_rollback_low() while handling
    failure from Sql_cmd_xa_prepare::process_xa_prepare() and an error in
    Daignostics_area still wasn't set. Therefore it is required to check
    that an error in Diagnostics_area is set before calling the method
    XID_STATE::set_error().

    If it wasn't done it would lead to failure of the assertion
      assert(m_status == DA_ERROR)
    in the method Diagnostics_area::mysql_errno().

    In case Sql_cmd_xa_prepare::process_xa_prepare() has failed and an error
    wasn't set in Diagnostics_area the error ER_XA_RBROLLBACK is set in the
    Diagnostics_area from the method Sql_cmd_xa_prepare::trans_xa_prepare()
    when non-zero result code returned by
    Sql_cmd_xa_prepare::process_xa_prepare() is handled.
  */
  if (all && thd->transaction_rollback_request && thd->is_error())
    trn_ctx->xid_state()->set_error(thd);

  (void)RUN_HOOK(transaction, after_rollback, (thd, all));
  return error;
}

int ha_rollback_trans(THD *thd, bool all) {
  int error = 0;
  Transaction_ctx *trn_ctx = thd->get_transaction();
  bool is_xa_rollback = trn_ctx->xid_state()->has_state(XID_STATE::XA_PREPARED);

  /*
    "real" is a nick name for a transaction for which a commit will
    make persistent changes. E.g. a 'stmt' transaction inside a 'all'
    transaction is not 'real': even though it's possible to commit it,
    the changes are not durable as they might be rolled back if the
    enclosing 'all' transaction is rolled back.
    We establish the value of 'is_real_trans' by checking
    if it's an explicit COMMIT or BEGIN statement, or implicit
    commit issued by DDL (in these cases all == true),
    or if we're running in autocommit mode (it's only in the autocommit mode
    ha_commit_one_phase() is called with an empty
    transaction.all.ha_list, see why in trans_register_ha()).
  */
  bool is_real_trans = all || !trn_ctx->is_active(Transaction_ctx::SESSION);

  DBUG_TRACE;

  /*
    We must not rollback the normal transaction if a statement
    transaction is pending.
  */
  assert(!trn_ctx->is_active(Transaction_ctx::STMT) || !all);

  if (thd->in_sub_stmt) {
    assert(0);
    /*
      If we are inside stored function or trigger we should not commit or
      rollback current statement transaction. See comment in ha_commit_trans()
      call for more information.
    */
    if (!all) return 0;
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    return 1;
  }

  if (tc_log) error = tc_log->rollback(thd, all);
    /*
      Mark multi-statement (any autocommit mode) or single-statement
      (autocommit=1) transaction as rolled back
    */
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (all || !thd->in_active_multi_stmt_transaction()) {
    MYSQL_ROLLBACK_TRANSACTION(thd->m_transaction_psi);
    thd->m_transaction_psi = nullptr;
  }
#endif

  /* Always cleanup. Even if nht==0. There may be savepoints. */
  if (is_real_trans) {
    trn_ctx->cleanup();
    thd->tx_priority = 0;
  }

  if (all) thd->transaction_rollback_request = false;

  /*
    Only call gtid_rollback(THD*), which will purge thd->owned_gtid, if
    complete transaction is being rollback or autocommit=1.
    Notice, XA rollback has just invoked update_on_commit() through
    tc_log->*rollback* stack.
  */
  if (is_real_trans && !is_xa_rollback) gtid_state->update_on_rollback(thd);

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
      trn_ctx->cannot_safely_rollback(Transaction_ctx::SESSION) &&
      !thd->slave_thread && thd->killed != THD::KILL_CONNECTION)
    trn_ctx->push_unsafe_rollback_warnings(thd);

  return error;
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
int ha_commit_attachable(THD *thd) {
  int error = 0;
  Transaction_ctx *trn_ctx = thd->get_transaction();
  auto ha_list = trn_ctx->ha_trx_info(Transaction_ctx::STMT);

  /* This function only handles attachable transactions. */
  assert(thd->is_attachable_ro_transaction_active());
  /*
    Since the attachable transaction is AUTOCOMMIT we only need
    to care about statement transaction.
  */
  assert(!trn_ctx->is_active(Transaction_ctx::SESSION));

  if (ha_list) {
    for (auto &ha_info : ha_list) {
      /* Attachable transaction is not supposed to modify anything. */
      assert(!ha_info.is_trx_read_write());

      auto ht = ha_info.ht();
      if (ht->commit(ht, thd, false)) {
        /*
          In theory this should not happen since attachable transactions
          are read only and therefore commit is supposed to only release
          resources/cleanup state. Even if this happens we will simply
          continue committing attachable transaction in other SEs.
        */
        assert(false);
        error = 1;
      }
      assert(!thd->status_var_aggregated);
      thd->status_var.ha_commit_count++;
      ha_info.reset(); /* keep it conveniently zero-filled */
    }
    trn_ctx->reset_scope(Transaction_ctx::STMT);
  }

  /*
    Mark transaction as committed in PSI.
  */
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (thd->m_transaction_psi != nullptr) {
    MYSQL_COMMIT_TRANSACTION(thd->m_transaction_psi);
    thd->m_transaction_psi = nullptr;
  }
#endif

  /* Free resources and perform other cleanup even for 'empty' transactions. */
  trn_ctx->cleanup();

  return (error);
}

/**
  Check if all storage engines used in transaction agree that after
  rollback to savepoint it is safe to release MDL locks acquired after
  savepoint creation.

  @param thd   The client thread that executes the transaction.

  @return true  - It is safe to release MDL locks.
          false - If it is not.
*/
bool ha_rollback_to_savepoint_can_release_mdl(THD *thd) {
  Transaction_ctx *trn_ctx = thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope =
      thd->in_sub_stmt ? Transaction_ctx::STMT : Transaction_ctx::SESSION;

  DBUG_TRACE;

  /**
    Checking whether it is safe to release metadata locks after rollback to
    savepoint in all the storage engines that are part of the transaction.
  */
  for (auto const &ha_info : trn_ctx->ha_trx_info(trx_scope)) {
    auto ht = ha_info.ht();
    assert(ht);

    if (ht->savepoint_rollback_can_release_mdl == nullptr ||
        ht->savepoint_rollback_can_release_mdl(ht, thd) == false)
      return false;
  }

  return true;
}

int ha_rollback_to_savepoint(THD *thd, SAVEPOINT *sv) {
  int error = 0;
  Transaction_ctx *trn_ctx = thd->get_transaction();
  Transaction_ctx::enum_trx_scope trx_scope =
      !thd->in_sub_stmt ? Transaction_ctx::SESSION : Transaction_ctx::STMT;

  DBUG_TRACE;

  trn_ctx->set_rw_ha_count(trx_scope, 0);
  trn_ctx->set_no_2pc(trx_scope, false);
  /*
    rolling back to savepoint in all storage engines that were part of the
    transaction when the savepoint was set
  */
  Ha_trx_info_list ha_list{sv->ha_list};
  for (auto const &ha_info : ha_list) {
    int err;
    auto ht = ha_info.ht();
    assert(ht);
    assert(ht->savepoint_set != nullptr);
    if ((err = ht->savepoint_rollback(
             ht, thd,
             (uchar *)(sv + 1) + ht->savepoint_offset))) {  // cannot happen
      char errbuf[MYSQL_ERRMSG_SIZE];
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err,
               my_strerror(errbuf, MYSQL_ERRMSG_SIZE, err));
      error = 1;
    }
    assert(!thd->status_var_aggregated);
    thd->status_var.ha_savepoint_rollback_count++;
    if (ht->prepare == nullptr) trn_ctx->set_no_2pc(trx_scope, true);
  }

  /*
    rolling back the transaction in all storage engines that were not part of
    the transaction when the savepoint was set
  */
  ha_list = trn_ctx->ha_trx_info(trx_scope);
  for (auto ha_info = ha_list.begin(); ha_info != sv->ha_list; ++ha_info) {
    int err;
    auto ht = ha_info->ht();
    if ((err = ht->rollback(ht, thd, !thd->in_sub_stmt))) {  // cannot happen
      char errbuf[MYSQL_ERRMSG_SIZE];
      my_error(ER_ERROR_DURING_ROLLBACK, MYF(0), err,
               my_strerror(errbuf, MYSQL_ERRMSG_SIZE, err));
      error = 1;
    }
    assert(!thd->status_var_aggregated);
    thd->status_var.ha_rollback_count++;
    ha_info->reset(); /* keep it conveniently zero-filled */
  }
  trn_ctx->set_ha_trx_info(trx_scope, sv->ha_list);

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (thd->m_transaction_psi != nullptr)
    MYSQL_INC_TRANSACTION_ROLLBACK_TO_SAVEPOINT(thd->m_transaction_psi, 1);
#endif

  return error;
}

int ha_prepare_low(THD *thd, bool all) {
  DBUG_TRACE;
  int error = 0;
  Transaction_ctx::enum_trx_scope trx_scope =
      all ? Transaction_ctx::SESSION : Transaction_ctx::STMT;
  auto ha_list = thd->get_transaction()->ha_trx_info(trx_scope);

  if (ha_list) {
    for (auto const &ha_info : ha_list) {
      if (!ha_info.is_trx_read_write() &&  // Do not call two-phase commit if
                                           // transaction is read-only
          !thd_holds_xa_transaction(thd))  // but only if is not an XA
                                           // transaction
        continue;

      auto ht = ha_info.ht();
      int err = ht->prepare(ht, thd, all);
      if (err) {
        if (!thd_holds_xa_transaction(
                thd)) {  // If XA PREPARE, let error be handled by caller
          char errbuf[MYSQL_ERRMSG_SIZE];
          my_error(ER_ERROR_DURING_COMMIT, MYF(0), err,
                   my_strerror(errbuf, MYSQL_ERRMSG_SIZE, err));
        }
        error = 1;
      }
      assert(!thd->status_var_aggregated);
      thd->status_var.ha_prepare_count++;

      if (error) break;
    }
    DBUG_EXECUTE_IF("crash_commit_after_prepare", DBUG_SUICIDE(););
  }

  return error;
}

/**
  @note
  according to the sql standard (ISO/IEC 9075-2:2003)
  section "4.33.4 SQL-statements and transaction states",
  SAVEPOINT is *not* transaction-initiating SQL-statement
*/
int ha_savepoint(THD *thd, SAVEPOINT *sv) {
  int error = 0;
  Transaction_ctx::enum_trx_scope trx_scope =
      !thd->in_sub_stmt ? Transaction_ctx::SESSION : Transaction_ctx::STMT;

  DBUG_TRACE;

  auto ha_list = thd->get_transaction()->ha_trx_info(trx_scope);
  for (auto const &ha_info : ha_list) {
    int err;
    auto ht = ha_info.ht();
    assert(ht);
    if (!ht->savepoint_set) {
      my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "SAVEPOINT");
      error = 1;
      break;
    }
    if ((err = ht->savepoint_set(
             ht, thd,
             (uchar *)(sv + 1) + ht->savepoint_offset))) {  // cannot happen
      char errbuf[MYSQL_ERRMSG_SIZE];
      my_error(ER_GET_ERRNO, MYF(0), err,
               my_strerror(errbuf, MYSQL_ERRMSG_SIZE, err));
      error = 1;
    }
    assert(!thd->status_var_aggregated);
    thd->status_var.ha_savepoint_count++;
  }
  /*
    Remember the list of registered storage engines. All new
    engines are prepended to the beginning of the list.
  */
  sv->ha_list = ha_list.head();

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (!error && thd->m_transaction_psi != nullptr)
    MYSQL_INC_TRANSACTION_SAVEPOINTS(thd->m_transaction_psi, 1);
#endif

  return error;
}

int ha_release_savepoint(THD *thd, SAVEPOINT *sv) {
  int error = 0;
  DBUG_TRACE;

  Ha_trx_info_list ha_list{sv->ha_list};
  for (auto const &ha_info : ha_list) {
    int err;
    auto ht = ha_info.ht();
    /* Savepoint life time is enclosed into transaction life time. */
    assert(ht);
    if (!ht->savepoint_release) continue;
    if ((err = ht->savepoint_release(
             ht, thd,
             (uchar *)(sv + 1) + ht->savepoint_offset))) {  // cannot happen
      char errbuf[MYSQL_ERRMSG_SIZE];
      my_error(ER_GET_ERRNO, MYF(0), err,
               my_strerror(errbuf, MYSQL_ERRMSG_SIZE, err));
      error = 1;
    }
  }
  DBUG_EXECUTE_IF("fail_ha_release_savepoint", {
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    error = 1;
  });

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  if (thd->m_transaction_psi != nullptr)
    MYSQL_INC_TRANSACTION_RELEASE_SAVEPOINT(thd->m_transaction_psi, 1);
#endif
  return error;
}

static bool snapshot_handlerton(THD *thd, plugin_ref plugin, void *arg) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->start_consistent_snapshot) {
    hton->start_consistent_snapshot(hton, thd);
    *((bool *)arg) = false;
  }
  return false;
}

int ha_start_consistent_snapshot(THD *thd) {
  bool warn = true;

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

static bool flush_handlerton(THD *, plugin_ref plugin, void *arg) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->flush_logs &&
      hton->flush_logs(hton, *(static_cast<bool *>(arg))))
    return true;
  return false;
}

bool ha_flush_logs(bool binlog_group_flush) {
  if (plugin_foreach(nullptr, flush_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN,
                     static_cast<void *>(&binlog_group_flush))) {
    return true;
  }
  return false;
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
                                   char *tmp_path) {
  uint i;
  if (lower_case_table_names != 2 || (file->ha_table_flags() & HA_FILE_BASED))
    return path;

  for (i = 0; i <= mysql_tmpdir_list.max; i++) {
    if (is_prefix(path, mysql_tmpdir_list.list[i])) return path;
  }

  /* Ensure that table handler get path in lower case */
  if (tmp_path != path) my_stpcpy(tmp_path, path);

  /*
    we only should turn into lowercase database/table part
    so start the process after homedirectory
  */
  my_casedn_str(files_charset_info, tmp_path + mysql_data_home_len);
  return tmp_path;
}

class Ha_delete_table_error_handler : public Internal_error_handler {
 public:
  bool handle_condition(THD *, uint, const char *,
                        Sql_condition::enum_severity_level *level,
                        const char *) override {
    /* Downgrade errors to warnings. */
    if (*level == Sql_condition::SL_ERROR) *level = Sql_condition::SL_WARNING;
    return false;
  }
};

/**
  Delete table from the storage engine.

  @param thd                Thread context.
  @param table_type         Handlerton for table's SE.
  @param path               Path to table (without extension).
  @param db                 Table database.
  @param alias              Table name.
  @param table_def          dd::Table object describing the table.
  @param generate_warning   Indicates whether errors during deletion
                            should be reported as warnings.

  @return  0 - in case of success, non-0 in case of failure, ENOENT
           if the file doesn't exists.
*/
int ha_delete_table(THD *thd, handlerton *table_type, const char *path,
                    const char *db, const char *alias,
                    const dd::Table *table_def, bool generate_warning) {
  handler *file;
  char tmp_path[FN_REFLEN];
  int error;
  TABLE dummy_table;
  TABLE_SHARE dummy_share;
  DBUG_TRACE;

  dummy_table.s = &dummy_share;

  /* DB_TYPE_UNKNOWN is used in ALTER TABLE when renaming only .frm files */
  if (table_type == nullptr ||
      !(file =
            get_new_handler((TABLE_SHARE *)nullptr,
                            table_def->partition_type() != dd::Table::PT_NONE,
                            thd->mem_root, table_type))) {
    return ENOENT;
  }

  path = get_canonical_filename(file, path, tmp_path);

  if ((error = file->ha_delete_table(path, table_def)) && generate_warning) {
    /*
      Because file->print_error() use my_error() to generate the error message
      we use an internal error handler to intercept it and store the text
      in a temporary buffer. Later the message will be presented to user
      as a warning.
    */
    Ha_delete_table_error_handler ha_delete_table_error_handler;

    /* Fill up strucutures that print_error may need */
    dummy_share.path.str = const_cast<char *>(path);
    dummy_share.path.length = strlen(path);
    dummy_share.db.str = db;
    dummy_share.db.length = strlen(db);
    dummy_share.table_name.str = alias;
    dummy_share.table_name.length = strlen(alias);
    dummy_table.alias = alias;

    file->change_table_ptr(&dummy_table, &dummy_share);

    /*
      XXX: should we convert *all* errors to warnings here?
      What if the error is fatal?
    */
    thd->push_internal_handler(&ha_delete_table_error_handler);
    file->print_error(error, 0);

    thd->pop_internal_handler();
  }

  destroy(file);

#ifdef HAVE_PSI_TABLE_INTERFACE
  if (likely(error == 0)) {
    /* Table share not available, so check path for temp_table prefix. */
    bool temp_table = (strstr(path, tmp_file_prefix) != nullptr);
    PSI_TABLE_CALL(drop_table_share)
    (temp_table, db, strlen(db), alias, strlen(alias));
  }
#endif

  return error;
}

// Prepare HA_CREATE_INFO to be used by ALTER as well as upgrade code.
void HA_CREATE_INFO::init_create_options_from_share(const TABLE_SHARE *share,
                                                    uint64_t used_fields) {
  if (!(used_fields & HA_CREATE_USED_MIN_ROWS)) min_rows = share->min_rows;

  if (!(used_fields & HA_CREATE_USED_MAX_ROWS)) max_rows = share->max_rows;

  if (!(used_fields & HA_CREATE_USED_AVG_ROW_LENGTH))
    avg_row_length = share->avg_row_length;

  if (!(used_fields & HA_CREATE_USED_DEFAULT_CHARSET))
    default_table_charset = share->table_charset;

  if (!(used_fields & HA_CREATE_USED_KEY_BLOCK_SIZE))
    key_block_size = share->key_block_size;

  if (!(used_fields & HA_CREATE_USED_STATS_SAMPLE_PAGES))
    stats_sample_pages = share->stats_sample_pages;

  if (!(used_fields & HA_CREATE_USED_STATS_AUTO_RECALC))
    stats_auto_recalc = share->stats_auto_recalc;

  if (!(used_fields & HA_CREATE_USED_TABLESPACE))
    tablespace = share->tablespace;

  if (storage_media == HA_SM_DEFAULT)
    storage_media = share->default_storage_media;

  /* Creation of federated table with LIKE clause needs connection string */
  if (!(used_fields & HA_CREATE_USED_CONNECTION))
    connect_string = share->connect_string;

  if (!(used_fields & HA_CREATE_USED_COMMENT)) {
    // Assert to check that used_fields flag and comment are in sync.
    assert(!comment.str);
    comment = share->comment;
  }

  if (!(used_fields & HA_CREATE_USED_COMPRESS)) {
    // Assert to check that used_fields flag and compress are in sync
    assert(!compress.str);
    compress = share->compress;
  }

  if (!(used_fields & (HA_CREATE_USED_ENCRYPT))) {
    // Assert to check that used_fields flag and encrypt_type are in sync
    assert(!encrypt_type.str);
    encrypt_type = share->encrypt_type;
  }

  if (!(used_fields & HA_CREATE_USED_SECONDARY_ENGINE)) {
    assert(secondary_engine.str == nullptr);
    secondary_engine = share->secondary_engine;
  }

  if (!(used_fields & HA_CREATE_USED_AUTOEXTEND_SIZE)) {
    /* m_implicit_tablespace_autoextend_size = 0 is a valid value. Hence,
    we need a mechanism to indicate the value change. */
    m_implicit_tablespace_autoextend_size = share->autoextend_size;
    m_implicit_tablespace_autoextend_size_change = false;
  }

  if (engine_attribute.str == nullptr)
    engine_attribute = share->engine_attribute;

  if (secondary_engine_attribute.str == nullptr)
    secondary_engine_attribute = share->secondary_engine_attribute;
}

/****************************************************************************
** General handler functions
****************************************************************************/
handler *handler::clone(const char *name, MEM_ROOT *mem_root) {
  DBUG_TRACE;

  handler *new_handler = get_new_handler(
      table->s, (table->s->m_part_info != nullptr), mem_root, ht);

  if (!new_handler) return nullptr;
  if (new_handler->set_ha_share_ref(ha_share)) goto err;

  /*
    Allocate handler->ref here because otherwise ha_open will allocate it
    on this->table->mem_root and we will not be able to reclaim that memory
    when the clone handler object is destroyed.
  */
  if (!(new_handler->ref =
            (uchar *)mem_root->Alloc(ALIGN_SIZE(ref_length) * 2)))
    goto err;
  /*
    TODO: Implement a more efficient way to have more than one index open for
    the same table instance. The ha_open call is not cacheable for clone.
  */
  if (new_handler->ha_open(table, name, table->db_stat,
                           HA_OPEN_IGNORE_IF_LOCKED, nullptr))
    goto err;

  return new_handler;

err:
  destroy(new_handler);
  return nullptr;
}

void handler::ha_statistic_increment(
    ulonglong System_status_var::*offset) const {
  if (table && table->in_use) (table->in_use->status_var.*offset)++;
}

THD *handler::ha_thd() const {
  assert(table == nullptr || table->in_use == nullptr ||
         table->in_use == current_thd);
  return table != nullptr && table->in_use != nullptr ? table->in_use
                                                      : current_thd;
}

void handler::unbind_psi() {
#ifdef HAVE_PSI_TABLE_INTERFACE
  assert(m_lock_type == F_UNLCK);
  assert(inited == NONE);
  /*
    Notify the instrumentation that this table is not owned
    by this thread any more.
  */
  PSI_TABLE_CALL(unbind_table)(m_psi);
#endif
}

void handler::rebind_psi() {
#ifdef HAVE_PSI_TABLE_INTERFACE
  assert(m_lock_type == F_UNLCK);
  assert(inited == NONE);
  /*
    Notify the instrumentation that this table is now owned
    by this thread.
  */
  PSI_table_share *share_psi = ha_table_share_psi(table_share);
  m_psi = PSI_TABLE_CALL(rebind_table)(share_psi, this, m_psi);
#endif
}

void handler::start_psi_batch_mode() {
#ifdef HAVE_PSI_TABLE_INTERFACE
  assert(m_psi_batch_mode == PSI_BATCH_MODE_NONE);
  assert(m_psi_locker == nullptr);
  m_psi_batch_mode = PSI_BATCH_MODE_STARTING;
  m_psi_numrows = 0;
#endif
}

void handler::end_psi_batch_mode() {
#ifdef HAVE_PSI_TABLE_INTERFACE
  assert(m_psi_batch_mode != PSI_BATCH_MODE_NONE);
  if (m_psi_locker != nullptr) {
    assert(m_psi_batch_mode == PSI_BATCH_MODE_STARTED);
    PSI_TABLE_CALL(end_table_io_wait)(m_psi_locker, m_psi_numrows);
    m_psi_locker = nullptr;
  }
  m_psi_batch_mode = PSI_BATCH_MODE_NONE;
#endif
}

PSI_table_share *handler::ha_table_share_psi(const TABLE_SHARE *share) const {
  return share->m_psi;
}

/*
  Open database handler object.

  Used for opening tables. The name will be the name of the file.
  A table is opened when it needs to be opened. For instance
  when a request comes in for a select on the table (tables are not
  open and closed for each request, they are cached).

  The server opens all tables by calling ha_open() which then calls
  the handler specific open().

  Try O_RDONLY if cannot open as O_RDWR. Don't wait for locks if not
  HA_OPEN_WAIT_IF_LOCKED is set

  @param  [out] table_arg             Table structure.
  @param        name                  Full path of table name.
  @param        mode                  Open mode flags.
  @param        test_if_locked        ?
  @param        table_def             dd::Table object describing table
                                      being open. Can be NULL for temporary
                                      tables created by optimizer.

  @retval >0    Error.
  @retval  0    Success.
*/

int handler::ha_open(TABLE *table_arg, const char *name, int mode,
                     int test_if_locked, const dd::Table *table_def) {
  int error;
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("name: %s  db_type: %d  db_stat: %d  mode: %d  lock_test: %d",
              name, ht->db_type, table_arg->db_stat, mode, test_if_locked));

  table = table_arg;
  assert(table->s == table_share);
  assert(m_lock_type == F_UNLCK);
  DBUG_PRINT("info", ("old m_lock_type: %d F_UNLCK %d", m_lock_type, F_UNLCK));
  MEM_ROOT *mem_root = (test_if_locked & HA_OPEN_TMP_TABLE)
                           ? &table->s->mem_root
                           : &table->mem_root;
  assert(alloc_root_inited(mem_root));

  if ((error = open(name, mode, test_if_locked, table_def))) {
    if ((error == EACCES || error == EROFS) && mode == O_RDWR &&
        (table->db_stat & HA_TRY_READ_ONLY)) {
      table->db_stat |= HA_READ_ONLY;
      error = open(name, O_RDONLY, test_if_locked, table_def);
    }
  }
  if (error) {
    set_my_errno(error); /* Safeguard */
    DBUG_PRINT("error", ("error: %d  errno: %d", error, errno));
  } else {
    assert(m_psi == nullptr);
    assert(table_share != nullptr);
#ifdef HAVE_PSI_TABLE_INTERFACE
    PSI_table_share *share_psi = ha_table_share_psi(table_share);
    m_psi = PSI_TABLE_CALL(open_table)(share_psi, this);
#endif

    if (table->s->db_options_in_use & HA_OPTION_READ_ONLY_DATA)
      table->db_stat |= HA_READ_ONLY;
    (void)extra(HA_EXTRA_NO_READCHECK);  // Not needed in SQL

    /* ref is already allocated for us if we're called from handler::clone() */
    if (!ref && !(ref = (uchar *)mem_root->Alloc(ALIGN_SIZE(ref_length) * 2))) {
      ha_close();
      error = HA_ERR_OUT_OF_MEM;
    } else
      dup_ref = ref + ALIGN_SIZE(ref_length);

    // Give the table a defined starting cursor, even if it never actually seeks
    // or writes. This is important for things like weedout on const tables
    // (which is a nonsensical combination, but can happen).
    memset(ref, 0, ref_length);
    cached_table_flags = table_flags();
  }

  return error;
}

/**
  Close handler.

  Called from sql_base.cc, sql_select.cc, and table.cc.
  In sql_select.cc it is only used to close up temporary tables or during
  the process where a temporary table is converted over to being a
  myisam table.
  For sql_base.cc look at close_data_tables().

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_close(void) {
  DBUG_TRACE;
#ifdef HAVE_PSI_TABLE_INTERFACE
  PSI_TABLE_CALL(close_table)(table_share, m_psi);
  m_psi = nullptr; /* instrumentation handle, invalid after close_table() */
  assert(m_psi_batch_mode == PSI_BATCH_MODE_NONE);
  assert(m_psi_locker == nullptr);
#endif
  // TODO: set table= NULL to mark the handler as closed?
  assert(m_psi == nullptr);
  assert(m_lock_type == F_UNLCK);
  assert(inited == NONE);
  if (m_unique) {
    // It's allocated on memroot and will be freed along with it
    m_unique->cleanup();
    m_unique = nullptr;
  }
  return close();
}

/**
  Initialize use of index.

  @param idx     Index to use
  @param sorted  Use sorted order

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_index_init(uint idx, bool sorted) {
  DBUG_EXECUTE_IF("ha_index_init_fail", return HA_ERR_TABLE_DEF_CHANGED;);
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == NONE);
  if (!(result = index_init(idx, sorted))) inited = INDEX;
  mrr_have_range = false;
  end_range = nullptr;
  return result;
}

/**
  End use of index.

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_index_end() {
  DBUG_TRACE;
  /* SQL HANDLER function can call this without having it locked. */
  assert(table->open_by_handler || table_share->tmp_table != NO_TMP_TABLE ||
         m_lock_type != F_UNLCK);
  assert(inited == INDEX);
  inited = NONE;
  end_range = nullptr;
  m_record_buffer = nullptr;
  if (m_unique) m_unique->reset(false);
  return index_end();
}

/**
  Initialize table for random read or scan.

  @param scan  if true: Initialize for random scans through rnd_next()
               if false: Initialize for random reads through rnd_pos()

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_rnd_init(bool scan) {
  DBUG_EXECUTE_IF("ha_rnd_init_fail", return HA_ERR_TABLE_DEF_CHANGED;);
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == NONE || (inited == RND && scan));
  inited = (result = rnd_init(scan)) ? NONE : RND;
  end_range = nullptr;
  return result;
}

/**
  End use of random access.

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_rnd_end() {
  DBUG_TRACE;
  /* SQL HANDLER function can call this without having it locked. */
  assert(table->open_by_handler || table_share->tmp_table != NO_TMP_TABLE ||
         m_lock_type != F_UNLCK);
  assert(inited == RND);
  inited = NONE;
  end_range = nullptr;
  m_record_buffer = nullptr;
  return rnd_end();
}

/**
  Read next row via random scan.

  @param buf  Buffer to read the row into

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_rnd_next(uchar *buf) {
  int result;
  DBUG_EXECUTE_IF("ha_rnd_next_deadlock", return HA_ERR_LOCK_DEADLOCK;);
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == RND);

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, MAX_KEY, result,
                      { result = rnd_next(buf); })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
}

/**
  Read row via random scan from position.

  @param[out] buf  Buffer to read the row into
  @param      pos  Position from position() call

  @return Operation status
    @retval 0     Success
    @retval != 0  Error (error code returned)
*/

int handler::ha_rnd_pos(uchar *buf, uchar *pos) {
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  /* TODO: Find out how to solve ha_rnd_pos when finding duplicate update. */
  /* assert(inited == RND); */

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, MAX_KEY, result,
                      { result = rnd_pos(buf, pos); })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
}

FT_INFO *handler::ft_init_ext(uint flags [[maybe_unused]],
                              uint inx [[maybe_unused]],
                              String *key [[maybe_unused]]) {
  my_error(ER_TABLE_CANT_HANDLE_FT, MYF(0));
  return nullptr;
}

int handler::ha_ft_read(uchar *buf) {
  int result;
  DBUG_TRACE;

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  result = ft_read(buf);
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
}

int handler::ha_sample_init(void *&scan_ctx, double sampling_percentage,
                            int sampling_seed,
                            enum_sampling_method sampling_method,
                            const bool tablesample) {
  DBUG_TRACE;
  assert(sampling_percentage >= 0.0);
  assert(sampling_percentage <= 100.0);
  assert(inited == NONE);

  // Initialise the random number generator.
  m_random_number_engine.seed(sampling_seed);
  m_sampling_percentage = sampling_percentage;

  int result = sample_init(scan_ctx, sampling_percentage, sampling_seed,
                           sampling_method, tablesample);
  inited = (result != 0) ? NONE : SAMPLING;
  return result;
}

int handler::ha_sample_end(void *scan_ctx) {
  DBUG_TRACE;
  assert(inited == SAMPLING);
  inited = NONE;
  int result = sample_end(scan_ctx);
  return result;
}

int handler::ha_sample_next(void *scan_ctx, uchar *buf) {
  DBUG_TRACE;
  assert(inited == SAMPLING);

  if (m_sampling_percentage == 0.0) return HA_ERR_END_OF_FILE;

  m_update_generated_read_fields = table->has_gcol();

  int result;
  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, MAX_KEY, result,
                      { result = sample_next(scan_ctx, buf); })

  if (result == 0 && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);

  return result;
}

int handler::sample_init(void *&scan_ctx [[maybe_unused]], double, int,
                         enum_sampling_method, const bool) {
  return rnd_init(true);
}

int handler::sample_end(void *scan_ctx [[maybe_unused]]) { return rnd_end(); }

int handler::sample_next(void *scan_ctx [[maybe_unused]], uchar *buf) {
  // Temporary set inited to RND, since we are calling rnd_next().
  int res = rnd_next(buf);

  std::uniform_real_distribution<double> rnd(0.0, 1.0);
  while (!res && rnd(m_random_number_engine) > (m_sampling_percentage / 100.0))
    res = rnd_next(buf);

  return res;
}

int handler::records(ha_rows *num_rows) {
  if (ha_table_flags() & HA_COUNT_ROWS_INSTANT) {
    *num_rows = stats.records;
    return 0;
  }

  int error = 0;
  ha_rows rows = 0;
  start_psi_batch_mode();

  if (!(error = ha_rnd_init(true))) {
    while (!table->in_use->killed) {
      DBUG_EXECUTE_IF("bug28079850", table->in_use->killed = THD::KILL_QUERY;);
      if ((error = ha_rnd_next(table->record[0]))) {
        if (error == HA_ERR_RECORD_DELETED)
          continue;
        else
          break;
      }
      ++rows;
    }
  }

  *num_rows = rows;
  end_psi_batch_mode();
  int ha_rnd_end_error = 0;
  if (error != HA_ERR_END_OF_FILE) *num_rows = HA_POS_ERROR;

  // Call ha_rnd_end() only if only if handler has been initialized.
  if (inited && (ha_rnd_end_error = ha_rnd_end())) *num_rows = HA_POS_ERROR;

  return (error != HA_ERR_END_OF_FILE) ? error : ha_rnd_end_error;
}

int handler::records_from_index(ha_rows *num_rows, uint index) {
  if (ha_table_flags() & HA_COUNT_ROWS_INSTANT) {
    *num_rows = stats.records;
    return 0;
  }

  int error = 0;
  ha_rows rows = 0;
  uchar *buf = table->record[0];
  start_psi_batch_mode();

  if (!(error = ha_index_init(index, false))) {
    if (!(error = ha_index_first(buf))) {
      rows = 1;

      while (!table->in_use->killed) {
        DBUG_EXECUTE_IF("bug28079850",
                        table->in_use->killed = THD::KILL_QUERY;);
        if ((error = ha_index_next(buf))) {
          if (error == HA_ERR_RECORD_DELETED)
            continue;
          else
            break;
        }
        ++rows;
      }
    }
  }

  *num_rows = rows;
  end_psi_batch_mode();
  int ha_index_end_error = 0;
  if (error != HA_ERR_END_OF_FILE) *num_rows = HA_POS_ERROR;

  // Call ha_index_end() only if handler has been initialized.
  if (inited && (ha_index_end_error = ha_index_end())) *num_rows = HA_POS_ERROR;

  return (error != HA_ERR_END_OF_FILE) ? error : ha_index_end_error;
}

int handler::handle_records_error(int error, ha_rows *num_rows) {
  // If query was killed set the error since not all storage engines do it.
  if (table->in_use->killed) {
    *num_rows = HA_POS_ERROR;
    if (error == 0) error = HA_ERR_QUERY_INTERRUPTED;
  }

  if (error != 0) assert(*num_rows == HA_POS_ERROR);
  if (*num_rows == HA_POS_ERROR) assert(error != 0);
  if (error != 0) {
    /*
      ha_innobase::records may have rolled back internally.
      In this case, thd_mark_transaction_to_rollback() will have been called.
      For the errors below, we need to abort right away.
    */
    switch (error) {
      case HA_ERR_LOCK_DEADLOCK:
      case HA_ERR_LOCK_TABLE_FULL:
      case HA_ERR_LOCK_WAIT_TIMEOUT:
      case HA_ERR_QUERY_INTERRUPTED:
        print_error(error, MYF(0));
        return error;
      default:
        return error;
    }
  }
  return 0;
}

/**
  Read [part of] row via [part of] index.
  @param[out] buf          buffer where store the data
  @param      key          Key to search for
  @param      keypart_map  Which part of key to use
  @param      find_flag    Direction/condition on key usage

  @returns Operation status
    @retval  0                   Success (found a record, and function has
                                 set table status to "has row")
    @retval  HA_ERR_END_OF_FILE  Row not found (function has set table status
                                 to "no row"). End of index passed.
    @retval  HA_ERR_KEY_NOT_FOUND Row not found (function has set table status
                                 to "no row"). Index cursor positioned.
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
                               enum ha_rkey_function find_flag) {
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == INDEX);
  assert(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result, {
    result = index_read_map(buf, key, keypart_map, find_flag);
  })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields = false;
  }
  // Filter duplicate records from multi-value index read.
  // (m_unique != nullptr in case of multi-value index read)
  // In case of range scan, duplicate records are filtered in
  // multi_range_read_next()
  if (!result && !mrr_have_range && m_unique != nullptr && filter_dup_records())
    result = HA_ERR_KEY_NOT_FOUND;

  table->set_row_status_from_handler(result);
  return result;
}

int handler::ha_index_read_last_map(uchar *buf, const uchar *key,
                                    key_part_map keypart_map) {
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == INDEX);
  assert(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
                      { result = index_read_last_map(buf, key, keypart_map); })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
}

/**
  Initializes an index and read it.

  @see handler::ha_index_read_map.
*/

int handler::ha_index_read_idx_map(uchar *buf, uint index, const uchar *key,
                                   key_part_map keypart_map,
                                   enum ha_rkey_function find_flag) {
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == NONE);
  assert(end_range == nullptr);
  assert(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, index, result, {
    result = index_read_idx_map(buf, index, key, keypart_map, find_flag);
  })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, index);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  assert(inited == NONE);
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

int handler::ha_index_next(uchar *buf) {
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == INDEX);
  assert(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
                      { result = index_next(buf); })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields = false;
  }
  // Filter duplicate records from multi-value index read.
  // (m_unique != nullptr in case of multi-value index read)
  // In case of range scan, duplicate records are filtered in
  // multi_range_read_next()
  if (!result && !mrr_have_range && m_unique != nullptr && filter_dup_records())
    result = HA_ERR_KEY_NOT_FOUND;

  table->set_row_status_from_handler(result);
  return result;
}

/**
  Reads the previous row via index.

  @param[out] buf  Row data

  @return Operation status.
    @retval  0                   Success
    @retval  HA_ERR_END_OF_FILE  Row not found
    @retval  != 0                Error
*/

int handler::ha_index_prev(uchar *buf) {
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == INDEX);
  assert(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
                      { result = index_prev(buf); })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
}

/**
  Reads the first row via index.

  @param[out] buf  Row data

  @return Operation status.
    @retval  0                   Success
    @retval  HA_ERR_END_OF_FILE  Row not found
    @retval  != 0                Error
*/

int handler::ha_index_first(uchar *buf) {
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == INDEX);
  assert(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
                      { result = index_first(buf); })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields = false;
  }
  // Filter duplicate records from multi-value index read.
  // (m_unique != nullptr in case of multi-value index read)
  // In case of range scan, duplicate records are filtered in
  // multi_range_read_next()
  if (!result && !mrr_have_range && m_unique != nullptr && filter_dup_records())
    result = HA_ERR_KEY_NOT_FOUND;

  table->set_row_status_from_handler(result);
  return result;
}

/**
  Reads the last row via index.

  @param[out] buf  Row data

  @return Operation status.
    @retval  0                   Success
    @retval  HA_ERR_END_OF_FILE  Row not found
    @retval  != 0                Error
*/

int handler::ha_index_last(uchar *buf) {
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == INDEX);
  assert(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
                      { result = index_last(buf); })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
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

int handler::ha_index_next_same(uchar *buf, const uchar *key, uint keylen) {
  int result;
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  assert(inited == INDEX);
  assert(!pushed_idx_cond || buf == table->record[0]);

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();
  MYSQL_TABLE_IO_WAIT(PSI_TABLE_FETCH_ROW, active_index, result,
                      { result = index_next_same(buf, key, keylen); })
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields = false;
  }
  // Filter duplicate records from multi-value index read.
  // (m_unique != nullptr in case of multi-value index read)
  // In case of range scan, duplicate records are filtered in
  // multi_range_read_next()

  if (!result && !mrr_have_range && m_unique != nullptr &&
      filter_dup_records()) {
    result = HA_ERR_KEY_NOT_FOUND;
  }

  table->set_row_status_from_handler(result);
  return result;
}

/**
  Read first row (only) from a table.

  This is never called for tables whose storage engine do not contain exact
  statistics on number of records, e.g. InnoDB.

  @note Since there is only one implementation for this function, it is
        non-virtual and does not call a protected inner function, like
        most other handler functions.

  @note Implementation only calls other handler functions, so there is no need
        to update generated columns nor set table status.
*/
int handler::ha_read_first_row(uchar *buf, uint primary_key) {
  int error;
  DBUG_TRACE;

  ha_statistic_increment(&System_status_var::ha_read_first_count);

  /*
    If there is very few deleted rows in the table, find the first row by
    scanning the table.
    TODO remove the test for HA_READ_ORDER
  */
  if (stats.deleted < 10 || primary_key >= MAX_KEY ||
      !(index_flags(primary_key, 0, false) & HA_READ_ORDER)) {
    if (!(error = ha_rnd_init(true))) {
      while ((error = ha_rnd_next(buf)) == HA_ERR_RECORD_DELETED)
        /* skip deleted row */;
      const int end_error = ha_rnd_end();
      if (!error) error = end_error;
    }
  } else {
    /* Find the first row through the primary key */
    if (!(error = ha_index_init(primary_key, false))) {
      error = ha_index_first(buf);
      const int end_error = ha_index_end();
      if (!error) error = end_error;
    }
  }
  return error;
}

int handler::ha_index_read_pushed(uchar *buf, const uchar *key,
                                  key_part_map keypart_map) {
  DBUG_TRACE;

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  int result = index_read_pushed(buf, key, keypart_map);
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
}

int handler::ha_index_next_pushed(uchar *buf) {
  DBUG_TRACE;

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  int result = index_next_pushed(buf);
  if (!result && m_update_generated_read_fields) {
    result = update_generated_read_fields(buf, table, active_index);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
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
inline ulonglong compute_next_insert_id(ulonglong nr,
                                        struct System_variables *variables) {
  const ulonglong save_nr = nr;

  if (variables->auto_increment_increment == 1)
    nr = nr + 1;  // optimization of the formula below
  else {
    nr = (((nr + variables->auto_increment_increment -
            variables->auto_increment_offset)) /
          (ulonglong)variables->auto_increment_increment);
    nr = (nr * (ulonglong)variables->auto_increment_increment +
          variables->auto_increment_offset);
  }

  if (unlikely(nr <= save_nr)) return ULLONG_MAX;

  return nr;
}

void handler::adjust_next_insert_id_after_explicit_value(ulonglong nr) {
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
inline ulonglong prev_insert_id(ulonglong nr,
                                struct System_variables *variables) {
  if (unlikely(nr < variables->auto_increment_offset)) {
    /*
      There's nothing good we can do here. That is a pathological case, where
      the offset is larger than the column's max possible value, i.e. not even
      the first sequence value may be inserted. User will receive warning.
    */
    DBUG_PRINT("info", ("auto_increment: nr: %lu cannot honour "
                        "auto_increment_offset: %lu",
                        (ulong)nr, variables->auto_increment_offset));
    return nr;
  }
  if (variables->auto_increment_increment == 1)
    return nr;  // optimization of the formula below
  nr = (((nr - variables->auto_increment_offset)) /
        (ulonglong)variables->auto_increment_increment);
  return (nr * (ulonglong)variables->auto_increment_increment +
          variables->auto_increment_offset);
}

/**
  Update the auto_increment field if necessary.

  Updates columns with type NEXT_NUMBER if:

  - If column value is set to NULL (in which case
    autoinc_field_has_explicit_non_null_value is 0)
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
    table->autoinc_field_has_explicit_non_null_value, and there also exists
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

#define AUTO_INC_DEFAULT_NB_ROWS 1  // Some prefer 1024 here
#define AUTO_INC_DEFAULT_NB_MAX_BITS 16
#define AUTO_INC_DEFAULT_NB_MAX ((1 << AUTO_INC_DEFAULT_NB_MAX_BITS) - 1)

int handler::update_auto_increment() {
  ulonglong nr, nb_reserved_values = 0;
  bool append = false;
  THD *thd = table->in_use;
  struct System_variables *variables = &thd->variables;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  DBUG_TRACE;

  /*
    next_insert_id is a "cursor" into the reserved interval, it may go greater
    than the interval, but not smaller.
  */
  assert(next_insert_id >= auto_inc_interval_for_cur_row.minimum());

  if ((nr = table->next_number_field->val_int()) != 0 ||
      (table->autoinc_field_has_explicit_non_null_value &&
       thd->variables.sql_mode & MODE_NO_AUTO_VALUE_ON_ZERO)) {
    /*
      First test if the query was aborted due to strict mode constraints.
    */
    if (thd->is_error() &&
        thd->get_stmt_da()->mysql_errno() == ER_TRUNCATED_WRONG_VALUE)
      return HA_ERR_AUTOINC_ERANGE;

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
    if (table->next_number_field->is_unsigned() || ((longlong)nr) > 0)
      adjust_next_insert_id_after_explicit_value(nr);

    insert_id_for_cur_row = 0;  // didn't generate anything
    return 0;
  }

  if (next_insert_id > table->next_number_field->get_max_int_value())
    return HA_ERR_AUTOINC_READ_FAILED;

  if ((nr = next_insert_id) >= auto_inc_interval_for_cur_row.maximum()) {
    /* next_insert_id is beyond what is reserved, so we reserve more. */
    const Discrete_interval *forced = thd->auto_inc_intervals_forced.get_next();
    if (forced != nullptr) {
      nr = forced->minimum();
      /*
        In a multi insert statement when the number of affected rows is known
        then reserve those many number of auto increment values. So that
        interval will be starting value to starting value + number of affected
        rows * increment of auto increment.
       */
      nb_reserved_values = (estimation_rows_to_insert > 0)
                               ? estimation_rows_to_insert
                               : forced->values();
    } else {
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
        nb_desired_values = estimation_rows_to_insert;
      else if ((auto_inc_intervals_count == 0) &&
               (thd->lex->bulk_insert_row_cnt > 0)) {
        /*
          For multi-row inserts, if the bulk inserts cannot be started, the
          handler::estimation_rows_to_insert will not be set. But we still
          want to reserve the autoinc values.
        */
        nb_desired_values = thd->lex->bulk_insert_row_cnt;
      } else /* go with the increasing defaults */
      {
        /* avoid overflow in formula, with this if() */
        if (auto_inc_intervals_count <= AUTO_INC_DEFAULT_NB_MAX_BITS) {
          nb_desired_values =
              AUTO_INC_DEFAULT_NB_ROWS * (1 << auto_inc_intervals_count);
          nb_desired_values =
              std::min(nb_desired_values, ulonglong(AUTO_INC_DEFAULT_NB_MAX));
        } else
          nb_desired_values = AUTO_INC_DEFAULT_NB_MAX;
      }
      /* This call ignores all its parameters but nr, currently */
      get_auto_increment(variables->auto_increment_offset,
                         variables->auto_increment_increment, nb_desired_values,
                         &nr, &nb_reserved_values);
      if (nr == ULLONG_MAX) return HA_ERR_AUTOINC_READ_FAILED;  // Mark failure

      /*
        That rounding below should not be needed when all engines actually
        respect offset and increment in get_auto_increment(). But they don't
        so we still do it. Wonder if for the not-first-in-index we should do
        it. Hope that this rounding didn't push us out of the interval; even
        if it did we cannot do anything about it (calling the engine again
        will not help as we inserted no row).
      */
      nr = compute_next_insert_id(nr - 1, variables);
    }

    if (table->s->next_number_keypart == 0) {
      /* We must defer the appending until "nr" has been possibly truncated */
      append = true;
    } else {
      /*
        For such auto_increment there is no notion of interval, just a
        singleton. The interval is not even stored in
        thd->auto_inc_interval_for_cur_row, so we are sure to call the engine
        for next row.
      */
      DBUG_PRINT("info", ("auto_increment: special not-first-in-index"));
    }
  }

  if (unlikely(nr == ULLONG_MAX)) return HA_ERR_AUTOINC_ERANGE;

  DBUG_PRINT("info", ("auto_increment: %lu", (ulong)nr));

  if (unlikely(table->next_number_field->store((longlong)nr, true))) {
    /*
      first test if the query was aborted due to strict mode constraints
    */
    if (thd->is_error() &&
        thd->get_stmt_da()->mysql_errno() == ER_WARN_DATA_OUT_OF_RANGE)
      return HA_ERR_AUTOINC_ERANGE;

    /*
      field refused this value (overflow) and truncated it, use the result of
      the truncation (which is going to be inserted); however we try to
      decrease it to honour auto_increment_* variables.
      That will shift the left bound of the reserved interval, we don't
      bother shifting the right bound (anyway any other value from this
      interval will cause a duplicate key).
    */
    nr = prev_insert_id(table->next_number_field->val_int(), variables);
    if (unlikely(table->next_number_field->store((longlong)nr, true)))
      nr = table->next_number_field->val_int();
  }
  if (append) {
    auto_inc_interval_for_cur_row.replace(nr, nb_reserved_values,
                                          variables->auto_increment_increment);
    auto_inc_intervals_count++;
    /* Row-based replication does not need to store intervals in binlog */
    if (mysql_bin_log.is_open() && !thd->is_current_stmt_binlog_format_row())
      thd->auto_inc_intervals_in_cur_stmt_for_binlog.append(
          auto_inc_interval_for_cur_row.minimum(),
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
  insert_id_for_cur_row = nr;
  /*
    Set next insert id to point to next auto-increment value to be able to
    handle multi-row statements.
  */
  set_next_insert_id(compute_next_insert_id(nr, variables));

  return 0;
}

/** @brief
  MySQL signal that it changed the column bitmap

  USAGE
    This is for handlers that needs to setup their own column bitmaps.
    Normally the handler should set up their own column bitmaps in
    index_init() or rnd_init() and in any column_bitmaps_signal() call after
    this.

    The handler is allowed to do changes to the bitmap after an index_init or
    rnd_init() call is made as after this, MySQL will not use the bitmap
    for any program logic checking.
*/
void handler::column_bitmaps_signal() {
  DBUG_TRACE;
  DBUG_PRINT("info", ("read_set: %p  write_set: %p", table->read_set,
                      table->write_set));
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

void handler::get_auto_increment(ulonglong offset [[maybe_unused]],
                                 ulonglong increment [[maybe_unused]],
                                 ulonglong nb_desired_values [[maybe_unused]],
                                 ulonglong *first_value,
                                 ulonglong *nb_reserved_values) {
  ulonglong nr;
  int error;
  DBUG_TRACE;

  (void)extra(HA_EXTRA_KEYREAD);
  table->mark_columns_used_by_index_no_reset(table->s->next_number_index,
                                             table->read_set);
  column_bitmaps_signal();

  if (ha_index_init(table->s->next_number_index, true)) {
    /* This should never happen, assert in debug, and fail in release build */
    assert(0);
    *first_value = ULLONG_MAX;
    return;
  }

  if (table->s->next_number_keypart == 0) {  // Autoincrement at key-start
    error = ha_index_last(table->record[1]);
    /*
      MySQL implicitly assumes such method does locking (as MySQL decides to
      use nr+increment without checking again with the handler, in
      handler::update_auto_increment()), so reserves to infinite.
    */
    *nb_reserved_values = ULLONG_MAX;
  } else {
    uchar key[MAX_KEY_LENGTH];
    key_copy(key, table->record[0],
             table->key_info + table->s->next_number_index,
             table->s->next_number_key_offset);
    error =
        ha_index_read_map(table->record[1], key,
                          make_prev_keypart_map(table->s->next_number_keypart),
                          HA_READ_PREFIX_LAST);
    /*
      MySQL needs to call us for next row: assume we are inserting ("a",null)
      here, we return 3, and next this statement will want to insert
      ("b",null): there is no reason why ("b",3+1) would be the good row to
      insert: maybe it already exists, maybe 3+1 is too large...
    */
    *nb_reserved_values = 1;
  }

  if (error) {
    if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND) {
      /* No entry found, start with 1. */
      nr = 1;
    } else {
      assert(0);
      nr = ULLONG_MAX;
    }
  } else
    nr = ((ulonglong)table->next_number_field->val_int_offset(
              table->s->rec_buff_length) +
          1);
  ha_index_end();
  (void)extra(HA_EXTRA_NO_KEYREAD);
  *first_value = nr;
}

void handler::ha_release_auto_increment() {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK ||
         (!next_insert_id && !insert_id_for_cur_row));
  DEBUG_SYNC(ha_thd(), "release_auto_increment");
  release_auto_increment();
  insert_id_for_cur_row = 0;
  auto_inc_interval_for_cur_row.replace(0, 0, 0);
  auto_inc_intervals_count = 0;
  if (next_insert_id > 0) {
    next_insert_id = 0;
    /*
      this statement used forced auto_increment values if there were some,
      wipe them away for other statements.
    */
    table->in_use->auto_inc_intervals_forced.clear();
  }
}

const char *table_case_name(const HA_CREATE_INFO *info, const char *name) {
  return ((lower_case_table_names == 2 && info->alias) ? info->alias : name);
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
  @param org_table_name  The original table name (if any)
*/

void print_keydup_error(TABLE *table, KEY *key, const char *msg, myf errflag,
                        const char *org_table_name) {
  /* Write the duplicated key in the error message */
  char key_buff[MAX_KEY_LENGTH];
  String str(key_buff, sizeof(key_buff), system_charset_info);
  std::string key_name;

  if (key == nullptr) {
    /* Key is unknown */
    key_name = "*UNKNOWN*";
    str.copy("", 0, system_charset_info);

  } else {
    /* Table is opened and defined at this point */
    key_unpack(&str, table, key);
    size_t max_length = MYSQL_ERRMSG_SIZE - strlen(msg);
    if (str.length() >= max_length) {
      str.length(max_length - 4);
      str.append(STRING_WITH_LEN("..."));
    }
    str[str.length()] = 0;
    if (org_table_name != nullptr)
      key_name = org_table_name;
    else
      key_name = table->s->table_name.str;
    key_name += ".";

    key_name += key->name;
  }

  my_printf_error(ER_DUP_ENTRY, msg, errflag, str.c_ptr(), key_name.c_str());
}

/**
  Construct and emit duplicate key error message using information
  from table's record buffer.

  @sa print_keydup_error(table, key, msg, errflag).
*/

void print_keydup_error(TABLE *table, KEY *key, myf errflag,
                        const char *org_table_name) {
  print_keydup_error(table, key,
                     ER_THD(current_thd, ER_DUP_ENTRY_WITH_KEY_NAME), errflag,
                     org_table_name);
}

/**
  This method is used to analyse the error to see whether the error
  is ignorable or not. Further comments in header file.
*/

bool handler::is_ignorable_error(int error) {
  DBUG_TRACE;

  // Catch errors that are ignorable
  switch (error) {
    // Error code 0 is not an error.
    case 0:
    // Dup key errors may be explicitly ignored.
    case HA_ERR_FOUND_DUPP_KEY:
    case HA_ERR_FOUND_DUPP_UNIQUE:
    // Foreign key constraint violations are ignorable.
    case HA_ERR_ROW_IS_REFERENCED:
    case HA_ERR_NO_REFERENCED_ROW:
      return true;
  }

  // Default is that an error is not ignorable.
  return false;
}

/**
  This method is used to analyse the error to see whether the error
  is fatal or not. Further comments in header file.
*/

bool handler::is_fatal_error(int error) {
  DBUG_TRACE;

  // No ignorable errors are fatal
  if (is_ignorable_error(error)) return false;

  // Catch errors that are not fatal
  switch (error) {
    /*
            Deadlock and lock timeout cause transaction/statement rollback so
       that THD::is_fatal_sub_stmt_error will be set. This means that they will
       not be possible to handle by stored program handlers inside stored
       functions and triggers even if non-fatal.
    */
    case HA_ERR_LOCK_WAIT_TIMEOUT:
    case HA_ERR_LOCK_DEADLOCK:
      return false;

    case HA_ERR_NULL_IN_SPATIAL:
      return false;
  }

  // Default is that an error is fatal
  return true;
}

/**
  Print error that we got from handler function.

  @note
    In case of delete table it's only safe to use the following parts of
    the 'table' structure:
    - table->s->path
    - table->alias
*/
void handler::print_error(int error, myf errflag) {
  THD *thd = current_thd;
  Foreign_key_error_handler foreign_key_error_handler(thd, this);

  DBUG_TRACE;
  DBUG_PRINT("enter", ("error: %d", error));

  int textno = ER_GET_ERRNO;
  switch (error) {
    case EACCES:
      textno = ER_OPEN_AS_READONLY;
      break;
    case EAGAIN:
      textno = ER_FILE_USED;
      break;
    case ENOENT: {
      char errbuf[MYSYS_STRERROR_SIZE];
      textno = ER_FILE_NOT_FOUND;
      my_error(textno, errflag, table_share->table_name.str, error,
               my_strerror(errbuf, sizeof(errbuf), error));
    } break;
    case HA_ERR_KEY_NOT_FOUND:
    case HA_ERR_NO_ACTIVE_RECORD:
    case HA_ERR_RECORD_DELETED:
    case HA_ERR_END_OF_FILE:
      textno = ER_KEY_NOT_FOUND;
      break;
    case HA_ERR_WRONG_MRG_TABLE_DEF:
      textno = ER_WRONG_MRG_TABLE;
      break;
    case HA_ERR_FOUND_DUPP_KEY: {
      uint key_nr = table ? get_dup_key(error) : -1;
      if ((int)key_nr >= 0) {
        print_keydup_error(
            table, key_nr == MAX_KEY ? nullptr : &table->key_info[key_nr],
            errflag);
        return;
      }
      textno = ER_DUP_KEY;
      break;
    }
    case HA_ERR_FOREIGN_DUPLICATE_KEY: {
      assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);

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
                              child_key_name, sizeof(child_key_name))) {
        my_error(ER_FOREIGN_DUPLICATE_KEY_WITH_CHILD_INFO, errflag,
                 table_share->table_name.str, rec.c_ptr_safe(),
                 child_table_name, child_key_name);
      } else {
        my_error(ER_FOREIGN_DUPLICATE_KEY_WITHOUT_CHILD_INFO, errflag,
                 table_share->table_name.str, rec.c_ptr_safe());
      }
      return;
    }
    case HA_ERR_NULL_IN_SPATIAL:
      my_error(ER_CANT_CREATE_GEOMETRY_OBJECT, errflag);
      return;
    case HA_ERR_FOUND_DUPP_UNIQUE:
      textno = ER_DUP_UNIQUE;
      break;
    case HA_ERR_RECORD_CHANGED:
      textno = ER_CHECKREAD;
      break;
    case HA_ERR_CRASHED:
      textno = ER_NOT_KEYFILE;
      break;
    case HA_ERR_WRONG_IN_RECORD:
      textno = ER_CRASHED_ON_USAGE;
      break;
    case HA_ERR_CRASHED_ON_USAGE:
      textno = ER_CRASHED_ON_USAGE;
      break;
    case HA_ERR_NOT_A_TABLE:
      textno = error;
      break;
    case HA_ERR_CRASHED_ON_REPAIR:
      textno = ER_CRASHED_ON_REPAIR;
      break;
    case HA_ERR_OUT_OF_MEM:
      textno = ER_OUT_OF_RESOURCES;
      break;
    case HA_ERR_SE_OUT_OF_MEMORY:
      my_error(ER_ENGINE_OUT_OF_MEMORY, errflag, table->file->table_type());
      return;
    case HA_ERR_WRONG_COMMAND:
      textno = ER_ILLEGAL_HA;
      break;
    case HA_ERR_OLD_FILE:
      textno = ER_OLD_KEYFILE;
      break;
    case HA_ERR_UNSUPPORTED:
      textno = ER_UNSUPPORTED_EXTENSION;
      break;
    case HA_ERR_RECORD_FILE_FULL:
    case HA_ERR_INDEX_FILE_FULL: {
      textno = ER_RECORD_FILE_FULL;
      /* Write the error message to error log */
      LogErr(ERROR_LEVEL, ER_SERVER_RECORD_FILE_FULL,
             table_share->table_name.str);
      break;
    }
    case HA_ERR_DISK_FULL_NOWAIT: {
      textno = ER_DISK_FULL_NOWAIT;
      /* Write the error message to error log */
      LogErr(ERROR_LEVEL, ER_SERVER_DISK_FULL_NOWAIT,
             table_share->table_name.str);
      break;
    }
    case HA_ERR_LOCK_WAIT_TIMEOUT:
      textno = ER_LOCK_WAIT_TIMEOUT;
      break;
    case HA_ERR_LOCK_TABLE_FULL:
      textno = ER_LOCK_TABLE_FULL;
      break;
    case HA_ERR_LOCK_DEADLOCK:
      textno = ER_LOCK_DEADLOCK;
      break;
    case HA_ERR_READ_ONLY_TRANSACTION:
      textno = ER_READ_ONLY_TRANSACTION;
      break;
    case HA_ERR_CANNOT_ADD_FOREIGN:
      textno = ER_CANNOT_ADD_FOREIGN;
      break;
    case HA_ERR_ROW_IS_REFERENCED: {
      String str;
      /*
        Manipulate the error message while handling the error
        condition based on the access check.
      */
      thd->push_internal_handler(&foreign_key_error_handler);
      get_error_message(error, &str);
      my_error(ER_ROW_IS_REFERENCED_2, errflag, str.c_ptr_safe());
      thd->pop_internal_handler();
      return;
    }
    case HA_ERR_NO_REFERENCED_ROW: {
      String str;
      /*
        Manipulate the error message while handling the error
        condition based on the access check.
      */
      thd->push_internal_handler(&foreign_key_error_handler);
      get_error_message(error, &str);
      my_error(ER_NO_REFERENCED_ROW_2, errflag, str.c_ptr_safe());
      thd->pop_internal_handler();
      return;
    }
    case HA_ERR_TABLE_DEF_CHANGED:
      textno = ER_TABLE_DEF_CHANGED;
      break;
    case HA_ERR_NO_SUCH_TABLE:
      my_error(ER_NO_SUCH_TABLE, errflag, table_share->db.str,
               table_share->table_name.str);
      return;
    case HA_ERR_RBR_LOGGING_FAILED:
      textno = ER_BINLOG_ROW_LOGGING_FAILED;
      break;
    case HA_ERR_DROP_INDEX_FK: {
      const char *ptr = "???";
      uint key_nr = table ? get_dup_key(error) : -1;
      if ((int)key_nr >= 0 && key_nr != MAX_KEY)
        ptr = table->key_info[key_nr].name;
      my_error(ER_DROP_INDEX_FK, errflag, ptr);
      return;
    }
    case HA_ERR_TABLE_NEEDS_UPGRADE:
      textno = ER_TABLE_NEEDS_UPGRADE;
      break;
    case HA_ERR_NO_PARTITION_FOUND:
      textno = ER_WRONG_PARTITION_NAME;
      break;
    case HA_ERR_TABLE_READONLY:
      textno = ER_OPEN_AS_READONLY;
      break;
    case HA_ERR_AUTOINC_READ_FAILED:
      textno = ER_AUTOINC_READ_FAILED;
      break;
    case HA_ERR_AUTOINC_ERANGE:
      textno = ER_WARN_DATA_OUT_OF_RANGE;
      break;
    case HA_ERR_TOO_MANY_CONCURRENT_TRXS:
      textno = ER_TOO_MANY_CONCURRENT_TRXS;
      break;
    case HA_ERR_INDEX_COL_TOO_LONG:
      textno = ER_INDEX_COLUMN_TOO_LONG;
      break;
    case HA_ERR_NOT_IN_LOCK_PARTITIONS:
      textno = ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET;
      break;
    case HA_ERR_INDEX_CORRUPT:
      textno = ER_INDEX_CORRUPT;
      break;
    case HA_ERR_UNDO_REC_TOO_BIG:
      textno = ER_UNDO_RECORD_TOO_BIG;
      break;
    case HA_ERR_TABLE_IN_FK_CHECK:
      textno = ER_TABLE_IN_FK_CHECK;
      break;
    case HA_WRONG_CREATE_OPTION:
      textno = ER_ILLEGAL_HA;
      break;
    case HA_MISSING_CREATE_OPTION: {
      const char *engine = table_type();
      my_error(ER_MISSING_HA_CREATE_OPTION, errflag, engine);
      return;
    }
    case HA_ERR_TOO_MANY_FIELDS:
      textno = ER_TOO_MANY_FIELDS;
      break;
    case HA_ERR_INNODB_READ_ONLY:
      textno = ER_INNODB_READ_ONLY;
      break;
    case HA_ERR_TEMP_FILE_WRITE_FAILURE:
      textno = ER_TEMP_FILE_WRITE_FAILURE;
      break;
    case HA_ERR_INNODB_FORCED_RECOVERY:
      textno = ER_INNODB_FORCED_RECOVERY;
      break;
    case HA_ERR_TABLE_CORRUPT:
      my_error(ER_TABLE_CORRUPT, errflag, table_share->db.str,
               table_share->table_name.str);
      return;
    case HA_ERR_QUERY_INTERRUPTED:
      textno = ER_QUERY_INTERRUPTED;
      break;
    case HA_ERR_TABLESPACE_MISSING: {
      char errbuf[MYSYS_STRERROR_SIZE];
      snprintf(errbuf, MYSYS_STRERROR_SIZE, "`%s`.`%s`", table_share->db.str,
               table_share->table_name.str);
      my_error(ER_TABLESPACE_MISSING, errflag, errbuf, error);
      return;
    }
    case HA_ERR_TABLESPACE_IS_NOT_EMPTY:
      my_error(ER_TABLESPACE_IS_NOT_EMPTY, errflag, table_share->db.str,
               table_share->table_name.str);
      return;
    case HA_ERR_WRONG_FILE_NAME:
      my_error(ER_WRONG_FILE_NAME, errflag, table_share->table_name.str);
      return;
    case HA_ERR_NOT_ALLOWED_COMMAND:
      textno = ER_NOT_ALLOWED_COMMAND;
      break;
    case HA_ERR_NO_SESSION_TEMP:
      textno = ER_NO_SESSION_TEMP;
      break;
    case HA_ERR_WRONG_TABLE_NAME:
      textno = ER_WRONG_TABLE_NAME;
      break;
    case HA_ERR_TOO_LONG_PATH:
      textno = ER_TABLE_NAME_CAUSES_TOO_LONG_PATH;
      break;
    case HA_ERR_TOO_BIG_ROW: {
      char errbuf[MYSQL_ERRMSG_SIZE];
      my_error(ER_GET_ERRNO, MYF(0), HA_ERR_TOO_BIG_ROW,
               my_strerror(errbuf, MYSQL_ERRMSG_SIZE, HA_ERR_TOO_BIG_ROW));
    }
      return;
    default: {
      /* The error was "unknown" to this function.
         Ask handler if it has got a message for this error */
      String str;
      bool temporary = get_error_message(error, &str);
      if (!str.is_empty()) {
        const char *engine = table_type();
        if (temporary)
          my_error(ER_GET_TEMPORARY_ERRMSG, errflag, error, str.ptr(), engine);
        else
          my_error(ER_GET_ERRMSG, errflag, error, str.ptr(), engine);
      } else {
        char errbuf[MYSQL_ERRMSG_SIZE];
        my_error(ER_GET_ERRNO, errflag, error,
                 my_strerror(errbuf, MYSQL_ERRMSG_SIZE, error));
      }
      return;
    }
  }
  if (textno != ER_FILE_NOT_FOUND)
    my_error(textno, errflag, table_share->table_name.str, error);
}

/**
  Return an error message specific to this handler.

  @param error  error code previously returned by handler
  @param buf    pointer to String where to add error message

  @return
    Returns true if this is a temporary error
*/
bool handler::get_error_message(int error [[maybe_unused]],
                                String *buf [[maybe_unused]]) {
  return false;
}

/**
  Check for incompatible collation changes.

  @retval
    HA_ADMIN_NEEDS_UPGRADE   Table may have data requiring upgrade.
  @retval
    0                        No upgrade required.
*/

int handler::check_collation_compatibility() {
  ulong mysql_version = table->s->mysql_version;

  if (mysql_version < 50124) {
    KEY *key = table->key_info;
    KEY *key_end = key + table->s->keys;
    for (; key < key_end; key++) {
      KEY_PART_INFO *key_part = key->key_part;
      KEY_PART_INFO *key_part_end = key_part + key->user_defined_key_parts;
      for (; key_part < key_part_end; key_part++) {
        if (!key_part->fieldnr) continue;
        Field *field = table->field[key_part->fieldnr - 1];
        uint cs_number = field->charset()->number;
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
             (cs_number == 33 || /* utf8mb3_general_ci - bug #27877 */
              cs_number == 35))) /* ucs2_general_ci - bug #27877 */
          return HA_ADMIN_NEEDS_UPGRADE;
      }
    }
  }
  return 0;
}

int handler::ha_check_for_upgrade(HA_CHECK_OPT *check_opt) {
  int error;
  KEY *keyinfo, *keyend;
  KEY_PART_INFO *keypart, *keypartend;

  if (!table->s->mysql_version) {
    /* check for blob-in-key error */
    keyinfo = table->key_info;
    keyend = table->key_info + table->s->keys;
    for (; keyinfo < keyend; keyinfo++) {
      keypart = keyinfo->key_part;
      keypartend = keypart + keyinfo->user_defined_key_parts;
      for (; keypart < keypartend; keypart++) {
        if (!keypart->fieldnr) continue;
        Field *field = table->field[keypart->fieldnr - 1];
        if (field->type() == MYSQL_TYPE_BLOB) {
          if (check_opt->sql_flags & TT_FOR_UPGRADE)
            check_opt->flags = T_MEDIUM;
          return HA_ADMIN_NEEDS_CHECK;
        }
      }
    }
  }

  if ((error = check_collation_compatibility())) return error;

  return check_for_upgrade(check_opt);
}

// Function identifies any old data type present in table.
int check_table_for_old_types(const TABLE *table, bool check_temporal_upgrade) {
  Field **field;

  for (field = table->field; (*field); field++) {
    if (table->s->mysql_version == 0)  // prior to MySQL 5.0
    {
      /* check for bad DECIMAL field */
      if ((*field)->type() == MYSQL_TYPE_NEWDECIMAL) {
        return HA_ADMIN_NEEDS_ALTER;
      }
      if ((*field)->type() == MYSQL_TYPE_VAR_STRING) {
        return HA_ADMIN_NEEDS_ALTER;
      }
    }

    /*
      Check for old DECIMAL field.

      Above check does not take into account for pre 5.0 decimal types which can
      be present in the data directory if user did in-place upgrade from
      mysql-4.1 to mysql-5.0.
    */
    if ((*field)->type() == MYSQL_TYPE_DECIMAL) {
      return HA_ADMIN_NEEDS_DUMP_UPGRADE;
    }

    if ((*field)->type() == MYSQL_TYPE_YEAR && (*field)->field_length == 2)
      return HA_ADMIN_NEEDS_ALTER;  // obsolete YEAR(2) type

    if (check_temporal_upgrade) {
      if (((*field)->real_type() == MYSQL_TYPE_TIME) ||
          ((*field)->real_type() == MYSQL_TYPE_DATETIME) ||
          ((*field)->real_type() == MYSQL_TYPE_TIMESTAMP))
        return HA_ADMIN_NEEDS_ALTER;
    }
  }
  return 0;
}

/**
  @return
    key if error because of duplicated keys
*/
uint handler::get_dup_key(int error) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  DBUG_TRACE;
  table->file->errkey = (uint)-1;
  if (error == HA_ERR_FOUND_DUPP_KEY || error == HA_ERR_FOUND_DUPP_UNIQUE ||
      error == HA_ERR_NULL_IN_SPATIAL || error == HA_ERR_DROP_INDEX_FK)
    table->file->info(HA_STATUS_ERRKEY | HA_STATUS_NO_LOCK);
  return table->file->errkey;
}

bool handler::get_foreign_dup_key(char *, uint, char *, uint) {
  assert(false);
  return (false);
}

int handler::delete_table(const char *name, const dd::Table *) {
  int saved_error = 0;
  int error = 0;
  int enoent_or_zero = ENOENT;  // Error if no file was deleted
  char buff[FN_REFLEN];
  const char **start_ext;

  assert(m_lock_type == F_UNLCK);

  if (!(start_ext = ht->file_extensions)) return 0;
  for (const char **ext = start_ext; *ext; ext++) {
    fn_format(buff, name, "", *ext, MY_UNPACK_FILENAME | MY_APPEND_EXT);
    if (mysql_file_delete_with_symlink(key_file_misc, buff, MYF(0))) {
      if (my_errno() != ENOENT) {
        /*
          If error on the first existing file, return the error.
          Otherwise delete as much as possible.
        */
        if (enoent_or_zero) return my_errno();
        saved_error = my_errno();
      }
    } else
      enoent_or_zero = 0;  // No error for ENOENT
    error = enoent_or_zero;
  }
  return saved_error ? saved_error : error;
}

int handler::rename_table(const char *from, const char *to,
                          const dd::Table *from_table_def [[maybe_unused]],
                          dd::Table *to_table_def [[maybe_unused]]) {
  int error = 0;
  const char **ext, **start_ext;

  if (!(start_ext = ht->file_extensions)) return 0;
  for (ext = start_ext; *ext; ext++) {
    if (rename_file_ext(from, to, *ext)) {
      error = my_errno();
      if (error != ENOENT) break;
      error = 0;
    }
  }
  if (error) {
    /* Try to revert the rename. Ignore errors. */
    for (; ext >= start_ext; ext--) rename_file_ext(to, from, *ext);
  }
  return error;
}

void handler::drop_table(const char *name) {
  close();
  delete_table(name, nullptr);
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
int handler::ha_check(THD *thd, HA_CHECK_OPT *check_opt) {
  int error;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);

  if ((table->s->mysql_version >= MYSQL_VERSION_ID) &&
      (check_opt->sql_flags & TT_FOR_UPGRADE))
    return 0;

  if (table->s->mysql_version < MYSQL_VERSION_ID) {
    // Check for old temporal format if avoid_temporal_upgrade is disabled.
    mysql_mutex_lock(&LOCK_global_system_variables);
    const bool check_temporal_upgrade = !avoid_temporal_upgrade;
    mysql_mutex_unlock(&LOCK_global_system_variables);

    if ((error = check_table_for_old_types(table, check_temporal_upgrade)))
      return error;
    error = ha_check_for_upgrade(check_opt);
    if (error && (error != HA_ADMIN_NEEDS_CHECK)) return error;
    if (!error && (check_opt->sql_flags & TT_FOR_UPGRADE)) return 0;
  }
  return check(thd, check_opt);
}

/**
  A helper function to mark a transaction read-write,
  if it is started.
*/

void handler::mark_trx_read_write() {
  Ha_trx_info *ha_info = &ha_thd()->get_ha_data(ht->slot)->ha_info[0];
  /*
    When a storage engine method is called, the transaction must
    have been started, unless it's a DDL call, for which the
    storage engine starts the transaction internally, and commits
    it internally, without registering in the ha_list.
    Unfortunately here we can't know for sure if the engine
    has registered the transaction or not, so we must check.
  */
  if (ha_info->is_started()) {
    assert(has_transactions());
    /*
      table_share can be NULL in ha_delete_table(). See implementation
      of standalone function ha_delete_table() in sql_base.cc.
    */
    if (table_share == nullptr || table_share->tmp_table == NO_TMP_TABLE) {
      /* TempTable and Heap tables don't use/support transactions. */
      ha_info->set_trx_read_write();
    }
  }
}

/**
  Repair table: public interface.

  @sa handler::repair()
*/

int handler::ha_repair(THD *thd, HA_CHECK_OPT *check_opt) {
  int result;
  mark_trx_read_write();

  result = repair(thd, check_opt);
  assert(result == HA_ADMIN_NOT_IMPLEMENTED ||
         ha_table_flags() & HA_CAN_REPAIR);

  // TODO: Check if table version in DD needs to be updated.
  // Previously we checked/updated FRM version here.
  return result;
}

/**
  Start bulk insert.

  Allow the handler to optimize for multiple row insert.

  @note rows == 0 means we will probably insert many rows.

  @param rows  Estimated rows to insert
*/

void handler::ha_start_bulk_insert(ha_rows rows) {
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);
  estimation_rows_to_insert = rows;
  start_bulk_insert(rows);
}

/**
  End bulk insert.

  @return Operation status
    @retval 0     Success
    @retval != 0  Failure (error code returned)
*/

int handler::ha_end_bulk_insert() {
  DBUG_TRACE;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);
  estimation_rows_to_insert = 0;
  return end_bulk_insert();
}

/**
  Bulk update row: public interface.

  @sa handler::bulk_update_row()
*/

int handler::ha_bulk_update_row(const uchar *old_data, uchar *new_data,
                                uint *dup_key_found) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);
  mark_trx_read_write();

  return bulk_update_row(old_data, new_data, dup_key_found);
}

/**
  Delete all rows: public interface.

  @sa handler::delete_all_rows()
*/

int handler::ha_delete_all_rows() {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);
  mark_trx_read_write();

  return delete_all_rows();
}

/**
  Truncate table: public interface.

  @sa handler::truncate()
*/

int handler::ha_truncate(dd::Table *table_def) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);
  mark_trx_read_write();

  return truncate(table_def);
}

/**
  Optimize table: public interface.

  @sa handler::optimize()
*/

int handler::ha_optimize(THD *thd, HA_CHECK_OPT *check_opt) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);
  mark_trx_read_write();

  return optimize(thd, check_opt);
}

/**
  Analyze table: public interface.

  @sa handler::analyze()
*/

int handler::ha_analyze(THD *thd, HA_CHECK_OPT *check_opt) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  mark_trx_read_write();

  return analyze(thd, check_opt);
}

/**
  Check and repair table: public interface.

  @sa handler::check_and_repair()
*/

bool handler::ha_check_and_repair(THD *thd) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_UNLCK);
  mark_trx_read_write();

  return check_and_repair(thd);
}

/**
  Disable indexes: public interface.

  @sa handler::disable_indexes()
*/

int handler::ha_disable_indexes(uint mode) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  mark_trx_read_write();

  return disable_indexes(mode);
}

/**
  Enable indexes: public interface.

  @sa handler::enable_indexes()
*/

int handler::ha_enable_indexes(uint mode) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  mark_trx_read_write();

  return enable_indexes(mode);
}

/**
  Discard or import tablespace: public interface.

  @sa handler::discard_or_import_tablespace()
*/

int handler::ha_discard_or_import_tablespace(bool discard,
                                             dd::Table *table_def) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);
  mark_trx_read_write();

  return discard_or_import_tablespace(discard, table_def);
}

bool handler::ha_prepare_inplace_alter_table(TABLE *altered_table,
                                             Alter_inplace_info *ha_alter_info,
                                             const dd::Table *old_table_def,
                                             dd::Table *new_table_def) {
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type != F_UNLCK);
  mark_trx_read_write();

  return prepare_inplace_alter_table(altered_table, ha_alter_info,
                                     old_table_def, new_table_def);
}

bool handler::ha_commit_inplace_alter_table(TABLE *altered_table,
                                            Alter_inplace_info *ha_alter_info,
                                            bool commit,
                                            const dd::Table *old_table_def,
                                            dd::Table *new_table_def) {
  /*
    At this point we should have an exclusive metadata lock on the table.
    The exception is if we're about to roll back changes (commit= false).
    In this case, we might be rolling back after a failed lock upgrade,
    so we could be holding the same lock level as for inplace_alter_table().
  */
  assert(ha_thd()->mdl_context.owns_equal_or_stronger_lock(
             MDL_key::TABLE, table->s->db.str, table->s->table_name.str,
             MDL_EXCLUSIVE) ||
         !commit);

  return commit_inplace_alter_table(altered_table, ha_alter_info, commit,
                                    old_table_def, new_table_def);
}

/*
   Default implementation to support in-place/instant alter table
   for operations which do not affect table data.
*/

enum_alter_inplace_result handler::check_if_supported_inplace_alter(
    TABLE *altered_table [[maybe_unused]], Alter_inplace_info *ha_alter_info) {
  DBUG_TRACE;

  HA_CREATE_INFO *create_info = ha_alter_info->create_info;

  Alter_inplace_info::HA_ALTER_FLAGS inplace_offline_operations =
      Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH |
      Alter_inplace_info::ALTER_COLUMN_NAME |
      Alter_inplace_info::ALTER_COLUMN_DEFAULT |
      Alter_inplace_info::CHANGE_CREATE_OPTION |
      Alter_inplace_info::ALTER_RENAME | Alter_inplace_info::RENAME_INDEX |
      Alter_inplace_info::ALTER_INDEX_COMMENT |
      Alter_inplace_info::CHANGE_INDEX_OPTION |
      Alter_inplace_info::ALTER_COLUMN_INDEX_LENGTH;

  /* Is there at least one operation that requires copy algorithm? */
  if (ha_alter_info->handler_flags & ~inplace_offline_operations)
    return HA_ALTER_INPLACE_NOT_SUPPORTED;

  /*
    ALTER TABLE tbl_name CONVERT TO CHARACTER SET .. and
    ALTER TABLE table_name DEFAULT CHARSET = .. most likely
    change column charsets and so not supported in-place through
    old API.

    Changing of PACK_KEYS, MAX_ROWS and ROW_FORMAT options were
    not supported as in-place operations in old API either.
  */
  if (create_info->used_fields &
          (HA_CREATE_USED_CHARSET | HA_CREATE_USED_DEFAULT_CHARSET |
           HA_CREATE_USED_PACK_KEYS | HA_CREATE_USED_MAX_ROWS) ||
      (table->s->row_type != create_info->row_type))
    return HA_ALTER_INPLACE_NOT_SUPPORTED;

  // The presence of engine attributes does not prevent inplace so
  // that we get the same behavior as COMMENT. If SEs support engine
  // attribute values which are incompatible with INPLACE the need to
  // check for that when overriding (as they must do for parsed
  // comments).

  uint table_changes = (ha_alter_info->handler_flags &
                        Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH)
                           ? IS_EQUAL_PACK_LENGTH
                           : IS_EQUAL_YES;
  if (table->file->check_if_incompatible_data(create_info, table_changes) ==
      COMPATIBLE_DATA_YES)
    return HA_ALTER_INPLACE_INSTANT;

  return HA_ALTER_INPLACE_NOT_SUPPORTED;
}

void Alter_inplace_info::report_unsupported_error(const char *not_supported,
                                                  const char *try_instead) {
  if (unsupported_reason == nullptr)
    my_error(ER_ALTER_OPERATION_NOT_SUPPORTED, MYF(0), not_supported,
             try_instead);
  else
    my_error(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON, MYF(0), not_supported,
             unsupported_reason, try_instead);
}

/**
  Rename table: public interface.

  @sa handler::rename_table()
*/

int handler::ha_rename_table(const char *from, const char *to,
                             const dd::Table *from_table_def,
                             dd::Table *to_table_def) {
  assert(m_lock_type == F_UNLCK);
  mark_trx_read_write();

  return rename_table(from, to, from_table_def, to_table_def);
}

/**
  Delete table: public interface.

  @sa handler::delete_table()
*/

int handler::ha_delete_table(const char *name, const dd::Table *table_def) {
  assert(m_lock_type == F_UNLCK);
  mark_trx_read_write();

  return delete_table(name, table_def);
}

/**
  Drop table in the engine: public interface.

  @sa handler::drop_table()
*/

void handler::ha_drop_table(const char *name) {
  assert(m_lock_type == F_UNLCK);
  mark_trx_read_write();

  return drop_table(name);
}

/**
  Create a table in the engine: public interface.

  @sa handler::create()
*/

int handler::ha_create(const char *name, TABLE *form, HA_CREATE_INFO *info,
                       dd::Table *table_def) {
  assert(m_lock_type == F_UNLCK);
  mark_trx_read_write();

  return create(name, form, info, table_def);
}

/**
 * Loads a table into its defined secondary storage engine: public interface.
 *
 * @param table The table to load into the secondary engine. Its read_set tells
 * which columns to load.
 *
 * @sa handler::load_table()
 */
int handler::ha_load_table(const TABLE &table) { return load_table(table); }

/**
 * Unloads a table from its defined secondary storage engine: public interface.
 *
 * @sa handler::unload_table()
 */
int handler::ha_unload_table(const char *db_name, const char *table_name,
                             bool error_if_not_loaded) {
  return unload_table(db_name, table_name, error_if_not_loaded);
}

/**
  Get the hard coded SE private data from the handler for a DD table.

  @sa handler::get_se_private_data()
*/
bool handler::ha_get_se_private_data(dd::Table *dd_table, bool reset) {
  return get_se_private_data(dd_table, reset);
}

/**
  Tell the storage engine that it is allowed to "disable transaction" in the
  handler. It is a hint that ACID is not required - it is used in NDB for
  ALTER TABLE, for example, when data are copied to temporary table.
  A storage engine may treat this hint any way it likes. NDB for example
  starts to commit every now and then automatically.
  This hint can be safely ignored.
*/
int ha_enable_transaction(THD *thd, bool on) {
  int error = 0;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("on: %d", (int)on));

  if ((thd->get_transaction()->m_flags.enabled = on)) {
    /*
      Now all storage engines should have transaction handling enabled.
      But some may have it enabled all the time - "disabling" transactions
      is an optimization hint that storage engine is free to ignore.
      So, let's commit an open transaction (if any) now.
    */
    if (!(error = ha_commit_trans(thd, false)))
      error = trans_commit_implicit(thd);
  }
  return error;
}

int handler::index_next_same(uchar *buf, const uchar *key, uint keylen) {
  int error;
  DBUG_TRACE;
  if (!(error = index_next(buf))) {
    ptrdiff_t ptrdiff = buf - table->record[0];
    uchar *save_record_0 = nullptr;
    KEY *key_info = nullptr;
    KEY_PART_INFO *key_part = nullptr;
    KEY_PART_INFO *key_part_end = nullptr;

    /*
      key_cmp_if_same() compares table->record[0] against 'key'.
      In parts it uses table->record[0] directly, in parts it uses
      field objects with their local pointers into table->record[0].
      If 'buf' is distinct from table->record[0], we need to move
      all record references. This is table->record[0] itself and
      the field pointers of the fields used in this key.
    */
    if (ptrdiff) {
      save_record_0 = table->record[0];
      table->record[0] = buf;
      key_info = table->key_info + active_index;
      key_part = key_info->key_part;
      key_part_end = key_part + key_info->user_defined_key_parts;
      for (; key_part < key_part_end; key_part++) {
        assert(key_part->field);
        key_part->field->move_field_offset(ptrdiff);
      }
    }

    if (key_cmp_if_same(table, key, active_index, keylen))
      error = HA_ERR_END_OF_FILE;

    /* Move back if necessary. */
    if (ptrdiff) {
      table->record[0] = save_record_0;
      for (key_part = key_info->key_part; key_part < key_part_end; key_part++)
        key_part->field->move_field_offset(-ptrdiff);
    }
  }
  return error;
}

/****************************************************************************
** Some general functions that isn't in the handler class
****************************************************************************/

/**
  Initiates table-file and calls appropriate database-creator.

  @param thd                 Thread context.
  @param path                Path to table file (without extension).
  @param db                  Database name.
  @param table_name          Table name.
  @param create_info         HA_CREATE_INFO describing table.
  @param update_create_info  Indicates that create_info needs to be
                             updated from table share.
  @param is_temp_table       Indicates that this is temporary table (for
                             cases when this info is not available from
                             HA_CREATE_INFO).
  @param table_def           Data-dictionary object describing table to
                             be used for table creation. Can be adjusted
                             by storage engine if it supports atomic DDL.
                             For non-temporary tables these changes will
                             be saved to the data-dictionary by this call.

  @retval
   0  ok
  @retval
   1  error
*/
int ha_create_table(THD *thd, const char *path, const char *db,
                    const char *table_name, HA_CREATE_INFO *create_info,
                    bool update_create_info, bool is_temp_table,
                    dd::Table *table_def) {
  int error = 1;
  TABLE table;
  char name_buff[FN_REFLEN];
  const char *name;
  TABLE_SHARE share;
#ifdef HAVE_PSI_TABLE_INTERFACE
  bool temp_table = is_temp_table ||
                    (create_info->options & HA_LEX_CREATE_TMP_TABLE) ||
                    (strstr(path, tmp_file_prefix) != nullptr);
#endif
  DBUG_TRACE;

  init_tmp_table_share(thd, &share, db, 0, table_name, path, nullptr);

  if (open_table_def(thd, &share, *table_def)) goto err;

#ifdef HAVE_PSI_TABLE_INTERFACE
  share.m_psi = PSI_TABLE_CALL(get_table_share)(temp_table, &share);
#endif

  // When db_stat is 0, we can pass nullptr as dd::Table since it won't be used.
  destroy(&table);
  if (open_table_from_share(thd, &share, "", 0, (uint)READ_ALL, 0, &table, true,
                            nullptr)) {
#ifdef HAVE_PSI_TABLE_INTERFACE
    PSI_TABLE_CALL(drop_table_share)
    (temp_table, db, strlen(db), table_name, strlen(table_name));
#endif
    goto err;
  }

  if (update_create_info) update_create_info_from_table(create_info, &table);

  name = get_canonical_filename(table.file, share.path.str, name_buff);

  error = table.file->ha_create(name, &table, create_info, table_def);

  if (error) {
    table.file->print_error(error, MYF(0));
#ifdef HAVE_PSI_TABLE_INTERFACE
    PSI_TABLE_CALL(drop_table_share)
    (temp_table, db, strlen(db), table_name, strlen(table_name));
#endif
  } else {
    /*
      We do post-create update only for engines supporting atomic DDL
      as only such engines are allowed to update dd::Table objects in
      handler::ha_create().
      The dd::Table objects for temporary tables are not stored in DD
      so do not need DD update.
      The dd::Table objects representing the DD tables themselves cannot
      be stored until the DD tables have been created in the SE.
    */
    if (!((create_info->options & HA_LEX_CREATE_TMP_TABLE) || is_temp_table ||
          dd::get_dictionary()->is_dd_table_name(db, table_name)) &&
        (table.file->ht->flags & HTON_SUPPORTS_ATOMIC_DDL)) {
      if (thd->dd_client()->update<dd::Table>(table_def)) error = 1;
    }
  }
  (void)closefrm(&table, false);
err:
  free_table_share(&share);
  return error != 0;
}

/**
  Try to discover table from engine.

  @note
    If found, import the serialized dictionary information.

  @retval
  -1    Table did not exists
  @retval
   0    Table created ok
  @retval
   > 0  Error, table existed but could not be created
*/
int ha_create_table_from_engine(THD *thd, const char *db, const char *name) {
  int error;
  uchar *sdi_blob;
  size_t sdi_len;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("name '%s'.'%s'", db, name));

  if ((error = ha_discover(thd, db, name, &sdi_blob, &sdi_len))) {
    /* Table could not be discovered and thus not created */
    return error;
  }

  /*
    Table was successfully discovered from SE, check if SDI need
    to be installed or if that has already been done by SE.
    No SDI blob returned from SE indicates it has installed
    the table definition for this table into DD itself.
    Otherwise, import the SDI based on the sdi_blob and sdi_len,
    which are set.
  */
  if (sdi_blob) {
    error = import_serialized_meta_data(sdi_blob, sdi_len, true);
    my_free(sdi_blob);
    if (error) return 2;
  }

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Table *table_def = nullptr;
  if (thd->dd_client()->acquire(db, name, &table_def)) return 3;

  if (table_def == nullptr) {
    my_error(ER_NO_SUCH_TABLE, MYF(0), db, name);
    return 3;
  }

  char path[FN_REFLEN + 1];
  build_table_filename(path, sizeof(path) - 1, db, name, "", 0);

  TABLE_SHARE share;
  init_tmp_table_share(thd, &share, db, 0, name, path, nullptr);

  if (open_table_def(thd, &share, *table_def)) return 3;

  TABLE table;
  // When db_stat is 0, we can pass nullptr as dd::Table since it won't be used.
  if (open_table_from_share(thd, &share, "", 0, 0, 0, &table, false, nullptr)) {
    free_table_share(&share);
    return 3;
  }

  HA_CREATE_INFO create_info;
  update_create_info_from_table(&create_info, &table);
  create_info.table_options |= HA_OPTION_CREATE_FROM_ENGINE;

  get_canonical_filename(table.file, path, path);
  std::unique_ptr<dd::Table> table_def_clone(table_def->clone());
  error =
      table.file->ha_create(path, &table, &create_info, table_def_clone.get());
  /*
    Note that the table_def_clone is not stored into the DD,
    necessary changes to the table_def should already have
    been done in ha_discover/import_serialized_meta_data.
  */
  (void)closefrm(&table, true);

  return error != 0;
}

/**
  Try to find a table in a storage engine.

  @param thd  Thread handle
  @param db   Normalized table schema name
  @param name Normalized table name.
  @param[out] exists Only valid if the function succeeded.

  @retval true   An error is found
  @retval false  Success, check *exists
*/

bool ha_check_if_table_exists(THD *thd, const char *db, const char *name,
                              bool *exists) {
  uchar *frmblob = nullptr;
  size_t frmlen;
  DBUG_TRACE;

  *exists = !ha_discover(thd, db, name, &frmblob, &frmlen);
  if (*exists) my_free(frmblob);

  return false;
}

/**
  Check if a table specified by name is a system table.

  @param       db                         Database name for the table.
  @param       table_name                 Table name to be checked.
  @param[out]  is_sql_layer_system_table  True if a system table belongs to
                                          sql_layer.

  @return Operation status
    @retval    true              If the table name is a system table.
    @retval    false             If the table name is a user-level table.
*/

static bool check_if_system_table(const char *db, const char *table_name,
                                  bool *is_sql_layer_system_table) {
  // Check if we have the system database name in the command.
  if (!dd::get_dictionary()->is_dd_schema_name(db)) return false;

  // Check if this is SQL layer system tables.
  if (dd::get_dictionary()->is_system_table_name(db, table_name))
    *is_sql_layer_system_table = true;

  return true;
}

/**
  @brief Check if a given table is a system table.

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

bool ha_check_if_supported_system_table(handlerton *hton, const char *db,
                                        const char *table_name) {
  DBUG_TRACE;
  st_sys_tbl_chk_params check_params;

  check_params.is_sql_layer_system_table = false;
  if (!check_if_system_table(db, table_name,
                             &check_params.is_sql_layer_system_table))
    return true;  // It's a user table name

  // Check if this is a system table and if some engine supports it.
  check_params.status = check_params.is_sql_layer_system_table
                            ? st_sys_tbl_chk_params::KNOWN_SYSTEM_TABLE
                            : st_sys_tbl_chk_params::NOT_KNOWN_SYSTEM_TABLE;
  check_params.db_type = hton->db_type;
  check_params.table_name = table_name;
  check_params.db = db;
  plugin_foreach(nullptr, check_engine_system_table_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &check_params);

  // SE does not support this system table.
  if (check_params.status == st_sys_tbl_chk_params::KNOWN_SYSTEM_TABLE)
    return false;

  // It's a system table or a valid user table.
  return true;
}

/**
  @brief Called for each SE to check if given db, tablename is a system table.

  @details The primary purpose of introducing this function is to stop system
  tables to be created or being moved to undesired storage engines.

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
static bool check_engine_system_table_handlerton(THD *, plugin_ref plugin,
                                                 void *arg) {
  st_sys_tbl_chk_params *check_params = (st_sys_tbl_chk_params *)arg;
  handlerton *hton = plugin_data<handlerton *>(plugin);

  // Do we already know that the table is a system table?
  if (check_params->status == st_sys_tbl_chk_params::KNOWN_SYSTEM_TABLE) {
    /*
      If this is the same SE specified in the command, we can
      simply ask the SE if it supports it stop the search regardless.
    */
    if (hton->db_type == check_params->db_type) {
      if (hton->is_supported_system_table &&
          hton->is_supported_system_table(
              check_params->db, check_params->table_name,
              check_params->is_sql_layer_system_table))
        check_params->status = st_sys_tbl_chk_params::SUPPORTED_SYSTEM_TABLE;
      return true;
    }
    /*
      If this is a different SE, there is no point in asking the SE
      since we already know it's a system table and we don't care
      if it is supported or not.
    */
    return false;
  }

  /*
    We don't yet know if the table is a system table or not.
    We therefore must always ask the SE.
  */
  if (hton->is_supported_system_table &&
      hton->is_supported_system_table(
          check_params->db, check_params->table_name,
          check_params->is_sql_layer_system_table)) {
    /*
      If this is the same SE specified in the command, we know it's a
      supported system table and can stop the search.
    */
    if (hton->db_type == check_params->db_type) {
      check_params->status = st_sys_tbl_chk_params::SUPPORTED_SYSTEM_TABLE;
      return true;
    } else
      check_params->status = st_sys_tbl_chk_params::KNOWN_SYSTEM_TABLE;
  }

  return false;
}

static bool rm_tmp_tables_handlerton(THD *thd, plugin_ref plugin, void *files) {
  handlerton *hton = plugin_data<handlerton *>(plugin);

  if (hton->state == SHOW_OPTION_YES && hton->rm_tmp_tables &&
      hton->rm_tmp_tables(hton, thd, (List<LEX_STRING> *)files))
    return true;

  return false;
}

/**
  Ask all SEs to drop all temporary tables which have been left from
  previous server run. Used on server start-up.

  @param[in]      thd    Thread context.
  @param[in,out]  files  List of files in directories for temporary files
                         which match tmp_file_prefix and thus can belong to
                         temporary tables. If any SE recognizes some file as
                         belonging to temporary table in this SE and deletes
                         the file it is also supposed to remove file from
                         this list.
*/

bool ha_rm_tmp_tables(THD *thd, List<LEX_STRING> *files) {
  return plugin_foreach(thd, rm_tmp_tables_handlerton,
                        MYSQL_STORAGE_ENGINE_PLUGIN, files);
}

/**
  Default implementation for handlerton::rm_tmp_tables() method which
  simply removes all files from "files" list which have one of SE's
  extensions. This implementation corresponds to default implementation
  of handler::delete_table() method.
*/

bool default_rm_tmp_tables(handlerton *hton, THD *, List<LEX_STRING> *files) {
  List_iterator<LEX_STRING> files_it(*files);
  LEX_STRING *file_path;

  if (!hton->file_extensions) return false;

  while ((file_path = files_it++)) {
    const char *file_ext = fn_ext(file_path->str);

    for (const char **ext = hton->file_extensions; *ext; ext++) {
      if (strcmp(file_ext, *ext) == 0) {
        if (my_is_symlink(file_path->str, nullptr) &&
            test_if_data_home_dir(file_path->str)) {
          /*
            For safety reasons, if temporary table file is a symlink pointing
            to a file in the data directory, don't delete the file, delete
            symlink file only. It would be nicer to not delete symlinked files
            at all but MyISAM supports temporary tables with DATA
            DIRECTORY/INDEX DIRECTORY options.
          */
          (void)mysql_file_delete(key_file_misc, file_path->str, MYF(0));
        } else
          (void)mysql_file_delete_with_symlink(key_file_misc, file_path->str,
                                               MYF(0));
        files_it.remove();
        break;
      }
    }
  }
  return false;
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
int ha_init_key_cache(std::string_view, KEY_CACHE *key_cache) {
  DBUG_TRACE;

  if (!key_cache->key_cache_inited) {
    mysql_mutex_lock(&LOCK_global_system_variables);
    size_t tmp_buff_size = (size_t)key_cache->param_buff_size;
    ulonglong tmp_block_size = key_cache->param_block_size;
    ulonglong division_limit = key_cache->param_division_limit;
    ulonglong age_threshold = key_cache->param_age_threshold;
    mysql_mutex_unlock(&LOCK_global_system_variables);
    return !init_key_cache(key_cache, tmp_block_size, tmp_buff_size,
                           division_limit, age_threshold);
  }
  return 0;
}

/**
  Resize key cache.
*/
int ha_resize_key_cache(KEY_CACHE *key_cache) {
  DBUG_TRACE;

  if (key_cache->key_cache_inited) {
    mysql_mutex_lock(&LOCK_global_system_variables);
    size_t tmp_buff_size = (size_t)key_cache->param_buff_size;
    ulonglong tmp_block_size = key_cache->param_block_size;
    ulonglong division_limit = key_cache->param_division_limit;
    ulonglong age_threshold = key_cache->param_age_threshold;
    mysql_mutex_unlock(&LOCK_global_system_variables);
    const int retval =
        resize_key_cache(key_cache, keycache_thread_var(), tmp_block_size,
                         tmp_buff_size, division_limit, age_threshold);
    return !retval;
  }
  return 0;
}

/**
  Move all tables from one key cache to another one.
*/
int ha_change_key_cache(KEY_CACHE *old_key_cache, KEY_CACHE *new_key_cache) {
  mi_change_key_cache(old_key_cache, new_key_cache);
  return 0;
}

struct st_discover_args {
  const char *db;
  const char *name;
  uchar **frmblob;
  size_t *frmlen;
};

static bool discover_handlerton(THD *thd, plugin_ref plugin, void *arg) {
  st_discover_args *vargs = (st_discover_args *)arg;
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->discover &&
      (!(hton->discover(hton, thd, vargs->db, vargs->name, vargs->frmblob,
                        vargs->frmlen))))
    return true;

  return false;
}

/**
  Try to discover one table from handler(s).

  @param[in]      thd     Thread context.
  @param[in]      db      Schema of table
  @param[in]      name    Name of table
  @param[out]     frmblob Pointer to blob with table definition.
  @param[out]     frmlen  Length of the returned table definition blob

  @retval
    -1   Table did not exists
  @retval
    0   OK. Table could be discovered from SE.
        The *frmblob and *frmlen may be set if returning a blob
        which should be installed into data dictionary
        by the caller.

  @retval
    >0   error.  frmblob and frmlen may not be set

*/
static int ha_discover(THD *thd, const char *db, const char *name,
                       uchar **frmblob, size_t *frmlen) {
  int error = -1;  // Table does not exist in any handler
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name));
  st_discover_args args = {db, name, frmblob, frmlen};

  if (is_prefix(name, tmp_file_prefix)) /* skip temporary tables */
    return error;

  if (plugin_foreach(thd, discover_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN,
                     &args))
    error = 0;

  if (!error) {
    assert(!thd->status_var_aggregated);
    thd->status_var.ha_discover_count++;
  }
  return error;
}

/**
  Call this function in order to give the handler the possibility
  to ask engine if there are any new tables that should be written to disk
  or any dropped tables that need to be removed from disk
*/
struct st_find_files_args {
  const char *db;
  const char *path;
  const char *wild;
  bool dir;
  List<LEX_STRING> *files;
};

static bool find_files_handlerton(THD *thd, plugin_ref plugin, void *arg) {
  st_find_files_args *vargs = (st_find_files_args *)arg;
  handlerton *hton = plugin_data<handlerton *>(plugin);

  if (hton->state == SHOW_OPTION_YES && hton->find_files)
    if (hton->find_files(hton, thd, vargs->db, vargs->path, vargs->wild,
                         vargs->dir, vargs->files))
      return true;

  return false;
}

int ha_find_files(THD *thd, const char *db, const char *path, const char *wild,
                  bool dir, List<LEX_STRING> *files) {
  int error = 0;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: '%s'  path: '%s'  wild: '%s'  dir: %d", db, path,
                       wild ? wild : "NULL", dir));
  st_find_files_args args = {db, path, wild, dir, files};

  plugin_foreach(thd, find_files_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN,
                 &args);
  /* The return value is not currently used */
  return error;
}

/**
  Ask handler if the table exists in engine.
  @retval
    HA_ERR_NO_SUCH_TABLE     Table does not exist
  @retval
    HA_ERR_TABLE_EXIST       Table exists
*/
struct st_table_exists_in_engine_args {
  const char *db;
  const char *name;
  int err;
};

static bool table_exists_in_engine_handlerton(THD *thd, plugin_ref plugin,
                                              void *arg) {
  st_table_exists_in_engine_args *vargs = (st_table_exists_in_engine_args *)arg;
  handlerton *hton = plugin_data<handlerton *>(plugin);

  int err = HA_ERR_NO_SUCH_TABLE;

  if (hton->state == SHOW_OPTION_YES && hton->table_exists_in_engine)
    err = hton->table_exists_in_engine(hton, thd, vargs->db, vargs->name);

  vargs->err = err;
  if (vargs->err == HA_ERR_TABLE_EXIST) return true;

  return false;
}

int ha_table_exists_in_engine(THD *thd, const char *db, const char *name) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: %s, name: %s", db, name));
  st_table_exists_in_engine_args args = {db, name, HA_ERR_NO_SUCH_TABLE};
  plugin_foreach(thd, table_exists_in_engine_handlerton,
                 MYSQL_STORAGE_ENGINE_PLUGIN, &args);
  DBUG_PRINT("exit", ("error: %d", args.err));
  return args.err;
}

/*
  TODO: change this into a dynamic struct
  List<handlerton> does not work as
  1. binlog_end is called when MEM_ROOT is gone
  2. cannot work with thd MEM_ROOT as memory should be freed
*/
#define MAX_HTON_LIST_ST 63
struct hton_list_st {
  handlerton *hton[MAX_HTON_LIST_ST];
  uint sz;
};

struct binlog_func_st {
  enum_binlog_func fn;
  void *arg;
};

/** @brief
  Listing handlertons first to avoid recursive calls and deadlock
*/
static bool binlog_func_list(THD *, plugin_ref plugin, void *arg) {
  hton_list_st *hton_list = (hton_list_st *)arg;
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->binlog_func) {
    uint sz = hton_list->sz;
    if (sz == MAX_HTON_LIST_ST - 1) {
      /* list full */
      return false;
    }
    hton_list->hton[sz] = hton;
    hton_list->sz = sz + 1;
  }
  return false;
}

static bool binlog_func_foreach(THD *thd, binlog_func_st *bfn) {
  hton_list_st hton_list;
  uint i, sz;

  hton_list.sz = 0;
  plugin_foreach(thd, binlog_func_list, MYSQL_STORAGE_ENGINE_PLUGIN,
                 &hton_list);

  for (i = 0, sz = hton_list.sz; i < sz; i++)
    hton_list.hton[i]->binlog_func(hton_list.hton[i], thd, bfn->fn, bfn->arg);
  return false;
}

int ha_reset_logs(THD *thd) {
  binlog_func_st bfn = {BFN_RESET_LOGS, nullptr};
  binlog_func_foreach(thd, &bfn);
  return 0;
}

void ha_reset_slave(THD *thd) {
  binlog_func_st bfn = {BFN_RESET_SLAVE, nullptr};
  binlog_func_foreach(thd, &bfn);
}

void ha_binlog_wait(THD *thd) {
  binlog_func_st bfn = {BFN_BINLOG_WAIT, nullptr};
  binlog_func_foreach(thd, &bfn);
}

int ha_binlog_index_purge_file(THD *thd, const char *file) {
  binlog_func_st bfn = {BFN_BINLOG_PURGE_FILE, const_cast<char *>(file)};
  binlog_func_foreach(thd, &bfn);
  return 0;
}

struct binlog_log_query_st {
  enum_binlog_command binlog_command;
  const char *query;
  size_t query_length;
  const char *db;
  const char *table_name;
};

static bool binlog_log_query_handlerton2(THD *thd, handlerton *hton,
                                         void *args) {
  struct binlog_log_query_st *b = (struct binlog_log_query_st *)args;
  if (hton->state == SHOW_OPTION_YES && hton->binlog_log_query)
    hton->binlog_log_query(hton, thd, b->binlog_command, b->query,
                           b->query_length, b->db, b->table_name);
  return false;
}

static bool binlog_log_query_handlerton(THD *thd, plugin_ref plugin,
                                        void *args) {
  return binlog_log_query_handlerton2(thd, plugin_data<handlerton *>(plugin),
                                      args);
}

void ha_binlog_log_query(THD *thd, handlerton *hton,
                         enum_binlog_command binlog_command, const char *query,
                         size_t query_length, const char *db,
                         const char *table_name) {
  struct binlog_log_query_st b;
  b.binlog_command = binlog_command;
  b.query = query;
  b.query_length = query_length;
  b.db = db;
  b.table_name = table_name;
  if (hton == nullptr)
    plugin_foreach(thd, binlog_log_query_handlerton,
                   MYSQL_STORAGE_ENGINE_PLUGIN, &b);
  else
    binlog_log_query_handlerton2(thd, hton, &b);
}

int ha_binlog_end(THD *thd) {
  binlog_func_st bfn = {BFN_BINLOG_END, nullptr};
  binlog_func_foreach(thd, &bfn);
  return 0;
}

static bool acl_notify_handlerton(THD *thd, plugin_ref plugin, void *data) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->acl_notify)
    hton->acl_notify(thd,
                     static_cast<const class Acl_change_notification *>(data));
  return false;
}

void ha_acl_notify(THD *thd, class Acl_change_notification *data) {
  plugin_foreach(thd, acl_notify_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN, data);
}

/**
  Calculate cost of 'index only' scan for given index and number of records

  @param keynr    Index number
  @param records  Estimated number of records to be retrieved

  @note
    It is assumed that we will read through the whole key range and that all
    key blocks are half full (normally things are much better). It is also
    assumed that each time we read the next key from the index, the handler
    performs a random seek, thus the cost is proportional to the number of
    blocks read.

  @return
    Estimated cost of 'index only' scan
*/

double handler::index_only_read_time(uint keynr, double records) {
  double read_time;
  uint keys_per_block =
      (stats.block_size / 2 /
           (table_share->key_info[keynr].key_length + ref_length) +
       1);
  read_time = ((double)(records + keys_per_block - 1) / (double)keys_per_block);
  return read_time;
}

double handler::table_in_memory_estimate() const {
  assert(stats.table_in_mem_estimate == IN_MEMORY_ESTIMATE_UNKNOWN ||
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

double handler::index_in_memory_estimate(uint keyno) const {
  const KEY *key = &table->key_info[keyno];

  /*
    If the storage engine has supplied information about how much of the
    index that is currently in a memory buffer, then use this estimate.
  */
  const double est = key->in_memory_estimate();
  if (est != IN_MEMORY_ESTIMATE_UNKNOWN) return est;

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
    file_length = stats.data_file_length;
  else
    file_length = stats.index_file_length;

  return estimate_in_memory_buffer(file_length);
}

double handler::estimate_in_memory_buffer(ulonglong table_index_size) const {
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
    of data can be cached in memory.
  */
  longlong memory_buf_size = get_memory_buffer_size();
  if (memory_buf_size <= 0) memory_buf_size = 100 * 1024 * 1024;  // 100 MB

  /*
    Upper limit for the relative size of a table to be considered
    entirely available in a memory buffer. If the actual table size is
    less than this we assume it is complete cached in a memory buffer.
  */
  const double table_index_in_memory_limit = 0.2;

  /*
    Estimate for how much of the total memory buffer this table/index
    can occupy.
  */
  const double percent_of_mem =
      static_cast<double>(table_index_size) / memory_buf_size;

  double in_mem_est;

  if (percent_of_mem < table_index_in_memory_limit)  // Less than 20 percent
    in_mem_est = 1.0;
  else if (percent_of_mem > 1.0)  // Larger than buffer
    in_mem_est = 0.0;
  else {
    /*
      The size of the table/index is larger than
      "table_index_in_memory_limit" * "memory_buf_size" but less than
      the total size of the memory buffer.
    */
    in_mem_est = 1.0 - (percent_of_mem - table_index_in_memory_limit) /
                           (1.0 - table_index_in_memory_limit);
  }
  assert(in_mem_est >= 0.0 && in_mem_est <= 1.0);

  return in_mem_est;
}

Cost_estimate handler::table_scan_cost() {
  /*
    This function returns a Cost_estimate object. The function should be
    implemented in a way that allows the compiler to use "return value
    optimization" to avoid creating the temporary object for the return value
    and use of the copy constructor.
  */

  const double io_cost = scan_time() * table->cost_model()->page_read_cost(1.0);
  Cost_estimate cost;
  cost.add_io(io_cost);
  return cost;
}

Cost_estimate handler::index_scan_cost(uint index,
                                       double ranges [[maybe_unused]],
                                       double rows) {
  /*
    This function returns a Cost_estimate object. The function should be
    implemented in a way that allows the compiler to use "return value
    optimization" to avoid creating the temporary object for the return value
    and use of the copy constructor.
  */

  assert(ranges >= 0.0);
  assert(rows >= 0.0);

  const double io_cost = index_only_read_time(index, rows) *
                         table->cost_model()->page_read_cost_index(index, 1.0);
  Cost_estimate cost;
  cost.add_io(io_cost);
  return cost;
}

Cost_estimate handler::read_cost(uint index, double ranges, double rows) {
  /*
    This function returns a Cost_estimate object. The function should be
    implemented in a way that allows the compiler to use "return value
    optimization" to avoid creating the temporary object for the return value
    and use of the copy constructor.
  */

  assert(ranges >= 0.0);
  assert(rows >= 0.0);

  const double io_cost =
      read_time(index, static_cast<uint>(ranges), static_cast<ha_rows>(rows)) *
      table->cost_model()->page_read_cost(1.0);
  Cost_estimate cost;
  cost.add_io(io_cost);
  return cost;
}

double handler::page_read_cost(uint index [[maybe_unused]], double reads) {
  return table->cost_model()->page_read_cost(reads);

  /////////////////
  // Other, non-page-based storage engine, may prefer to
  // override to;
  // return read_cost(index, 1, reads).total_cost();

  // Longer term: We should avoid mixed usage of read_cost()
  // and page_read_cost() from the optimizer. Use only
  // one of these to get cost estimates comparable between different
  // access methods and call paths.
}

double handler::worst_seek_times(double reads) {
  return table->cost_model()->page_read_cost(reads);
}

/**
  Check if key has partially-covered columns

  We can't use DS-MRR to perform range scans when the ranges are over
  partially-covered keys, because we'll not have full key part values
  (we'll have their prefixes from the index) and will not be able to check
  if we've reached the end the range.

  @param table  Table to check keys for
  @param keyno  Key to check

  @todo
    Allow use of DS-MRR in cases where the index has partially-covered
    components but they are not used for scanning.

  @retval true   Yes
  @retval false  No
*/

static bool key_uses_partial_cols(TABLE *table, uint keyno) {
  KEY_PART_INFO *kp = table->key_info[keyno].key_part;
  KEY_PART_INFO *kp_end = kp + table->key_info[keyno].user_defined_key_parts;
  for (; kp != kp_end; kp++) {
    if (!kp->field->part_of_key.is_set(keyno)) return true;
  }
  return false;
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
  @param [in,out] bufsz  IN:  Size of the buffer available for use
                         OUT: Size of the buffer that is expected to be actually
                              used, or 0 if buffer is not needed.
  @param [in,out] flags  A combination of HA_MRR_* flags
  @param [out] cost      Estimated cost of MRR access

  @note
    This method (or an overriding one in a derived class) must check for
    \c thd->killed and return HA_POS_ERROR if it is not zero. This is required
    for a user to be able to interrupt the calculation by killing the
    connection/query.

  @retval
    HA_POS_ERROR  Error or the engine is unable to perform the requested
                  scan. Values of OUT parameters are undefined.
  @retval
    other         OK, *cost contains cost of the scan, *bufsz and *flags
                  contain scan parameters.
*/

ha_rows handler::multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                             void *seq_init_param,
                                             uint n_ranges_arg [[maybe_unused]],
                                             uint *bufsz, uint *flags,
                                             Cost_estimate *cost) {
  KEY_MULTI_RANGE range;
  range_seq_t seq_it;
  ha_rows rows, total_rows = 0;
  uint n_ranges = 0;
  THD *thd = current_thd;

  /* Default MRR implementation doesn't need buffer */
  *bufsz = 0;

  DBUG_EXECUTE_IF("bug13822652_2", thd->killed = THD::KILL_QUERY;);

  seq_it = seq->init(seq_init_param, n_ranges, *flags);
  while (!seq->next(seq_it, &range)) {
    if (unlikely(thd->killed != 0)) return HA_POS_ERROR;

    n_ranges++;
    key_range *min_endp, *max_endp;
    if (range.range_flag & GEOM_FLAG) {
      min_endp = &range.start_key;
      max_endp = nullptr;
    } else {
      min_endp = range.start_key.length ? &range.start_key : nullptr;
      max_endp = range.end_key.length ? &range.end_key : nullptr;
    }

    /*
      Return HA_POS_ERROR if the specified keyno is not capable of
      serving the specified range request. The cases checked for are:

        1) The range contain NULL values and the specified index will
           fallback to do a full table scan if it find NULLs in the keys.
        2) The range does not specify all key parts and the key
           cannot provide partial key searches.
    */
    if (range.range_flag & NULL_RANGE &&  // 1)
        table->file->index_flags(keyno, 0, false) & HA_TABLE_SCAN_ON_NULL) {
      // The NULL_RANGE will result in a full TABLE_SCAN, reject it.
      return HA_POS_ERROR;
    }
    if (!(range.range_flag & EQ_RANGE) ||  // 2)
        range.start_key.length < table->key_info[keyno].key_length) {
      // A full EQ-range was not specified, reject if not OK by index.
      if (index_flags(keyno, 0, false) & HA_ONLY_WHOLE_INDEX) {
        return HA_POS_ERROR;
      }
    }

    /*
      Get the number of rows in the range. This is done by calling
      records_in_range() unless:

        1) The index is unique.
           There cannot be more than one matching row, so 1 is
           assumed. Note that it is possible that the correct number
           is actually 0, so the row estimate may be too high in this
           case. Also note: ranges of the form "x IS NULL" may have more
           than 1 matching row so records_in_range() is called for these.
        2) SKIP_RECORDS_IN_RANGE will be set when skip_records_in_range or
           use_index_statistics are true.
           Ranges of the form "x IS NULL" will not use index statistics
           because the number of rows with this value are likely to be
           very different than the values in the index statistics.

      Note: With SKIP_RECORDS_IN_RANGE, use Index statistics if:
            a) Index statistics is available.
            b) The range is an equality range but the index is either not
               unique or all of the keyparts are not used.
    */
    int keyparts_used = 0;
    if ((range.range_flag & UNIQUE_RANGE) &&  // 1)
        !(range.range_flag & NULL_RANGE))
      rows = 1; /* there can be at most one row */
    else if (range.range_flag & SKIP_RECORDS_IN_RANGE &&  // 2)
             !(range.range_flag & NULL_RANGE)) {
      if ((range.range_flag & EQ_RANGE) &&
          (keyparts_used = my_count_bits(range.start_key.keypart_map)) &&
          table->key_info[keyno].has_records_per_key(keyparts_used - 1)) {
        rows = static_cast<ha_rows>(
            table->key_info[keyno].records_per_key(keyparts_used - 1));
      } else {
        /*
          Since records_in_range has not been called, set the rows to 1.
          FORCE INDEX has been used, cost model values will be ignored anyway.
        */
        rows = 1;
      }
    } else {
      DBUG_EXECUTE_IF("crash_records_in_range", DBUG_SUICIDE(););
      assert(min_endp || max_endp);
      rows = table->pos_in_table_list->is_derived_unfinished_materialization()
                 ? HA_POS_ERROR
                 : this->records_in_range(keyno, min_endp, max_endp);
      if (rows == HA_POS_ERROR) {
        /* Can't scan one range => can't do MRR scan at all */
        return HA_POS_ERROR;
      }
    }
    total_rows += rows;
  }

  assert(total_rows != HA_POS_ERROR);
  {
    const Cost_model_table *const cost_model = table->cost_model();

    /* The following calculation is the same as in multi_range_read_info(): */
    *flags |= (HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SUPPORT_SORTED);

    assert(cost->is_zero());
    if (*flags & HA_MRR_INDEX_ONLY)
      *cost = index_scan_cost(keyno, static_cast<double>(n_ranges),
                              static_cast<double>(total_rows));
    else
      *cost = read_cost(keyno, static_cast<double>(n_ranges),
                        static_cast<double>(total_rows));
    cost->add_cpu(
        cost_model->row_evaluate_cost(static_cast<double>(total_rows)) + 0.01);
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
  @param [in,out] bufsz  IN:  Size of the buffer available for use
                         OUT: Size of the buffer that will be actually used, or
                              0 if buffer is not needed.
  @param [in,out] flags  A combination of HA_MRR_* flags
  @param [out] cost      Estimated cost of MRR access

  @retval
    0     OK, *cost contains cost of the scan, *bufsz and *flags contain scan
          parameters.
  @retval
    other Error or can't perform the requested scan
*/

ha_rows handler::multi_range_read_info(uint keyno, uint n_ranges, uint n_rows,
                                       uint *bufsz, uint *flags,
                                       Cost_estimate *cost) {
  *bufsz = 0; /* Default implementation doesn't need a buffer */

  *flags |= HA_MRR_USE_DEFAULT_IMPL;
  *flags |= HA_MRR_SUPPORT_SORTED;

  assert(cost->is_zero());

  /* Produce the same cost as non-MRR code does */
  if (*flags & HA_MRR_INDEX_ONLY)
    *cost = index_scan_cost(keyno, n_ranges, n_rows);
  else
    *cost = read_cost(keyno, n_ranges, n_rows);
  return 0;
}

/**
  Initialize the MRR scan.

  This function may do heavyweight scan
  initialization like row prefetching/sorting/etc (NOTE: but better not do
  it here as we may not need it, e.g. if we never satisfy WHERE clause on
  previous tables. For many implementations it would be natural to do such
  initializations in the first multi_read_range_next() call)

  mode is a combination of the following flags: HA_MRR_SORTED,
  HA_MRR_INDEX_ONLY, HA_MRR_NO_ASSOCIATION

  @param seq_funcs       Range sequence to be traversed
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

int handler::multi_range_read_init(RANGE_SEQ_IF *seq_funcs,
                                   void *seq_init_param, uint n_ranges,
                                   uint mode,
                                   HANDLER_BUFFER *buf [[maybe_unused]]) {
  DBUG_TRACE;
  mrr_iter = seq_funcs->init(seq_init_param, n_ranges, mode);
  mrr_funcs = *seq_funcs;
  mrr_is_output_sorted = mode & HA_MRR_SORTED;
  mrr_have_range = false;
  return 0;
}

int handler::ha_multi_range_read_next(char **range_info) {
  int result;
  DBUG_TRACE;

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  result = multi_range_read_next(range_info);
  if (!result && m_update_generated_read_fields) {
    result =
        update_generated_read_fields(table->record[0], table, active_index);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
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

int handler::multi_range_read_next(char **range_info) {
  int result = HA_ERR_END_OF_FILE;
  int range_res = 0;
  bool dup_found = false;
  DBUG_TRACE;
  // For a multi-valued index the unique filter have to be used for correct
  // result
  assert(!(table->key_info[active_index].flags & HA_MULTI_VALUED_KEY) ||
         m_unique);

  if (!mrr_have_range) {
    mrr_have_range = true;
    goto start;
  }

  do {
    /*
      Do not call read_range_next() if its equality on a unique
      index.
    */
    if (!((mrr_cur_range.range_flag & UNIQUE_RANGE) &&
          (mrr_cur_range.range_flag & EQ_RANGE))) {
      assert(!result || result == HA_ERR_END_OF_FILE);
      result = read_range_next();
      DBUG_EXECUTE_IF("bug20162055_DEADLOCK", result = HA_ERR_LOCK_DEADLOCK;);
      /*
        On success check loop condition to filter duplicates, if needed.
        Exit on non-EOF error. Use next range on EOF error.
      */
      if (!result) continue;
      if (result != HA_ERR_END_OF_FILE) break;
    } else {
      if (was_semi_consistent_read()) goto scan_it_again;
    }

  start:
    /* Try the next range(s) until one matches a record. */
    while (!(range_res = mrr_funcs.next(mrr_iter, &mrr_cur_range))) {
    scan_it_again:
      result = read_range_first(
          mrr_cur_range.start_key.keypart_map ? &mrr_cur_range.start_key
                                              : nullptr,
          mrr_cur_range.end_key.keypart_map ? &mrr_cur_range.end_key : nullptr,
          mrr_cur_range.range_flag & EQ_RANGE, mrr_is_output_sorted);
      if (result != HA_ERR_END_OF_FILE) break;
    }
  } while (((result == HA_ERR_END_OF_FILE) ||
            (m_unique && (dup_found = filter_dup_records()))) &&
           !range_res);

  *range_info = mrr_cur_range.ptr;
  /*
    Last found record was a duplicate and we retrieved records from all
    ranges, so no more records can be returned.
  */
  if (dup_found && range_res) result = HA_ERR_END_OF_FILE;

  DBUG_PRINT("exit", ("handler::multi_range_read_next result %d", result));
  return result;
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

  @param         seq_funcs       Interval sequence enumeration functions
  @param         seq_init_param  Interval sequence enumeration parameter
  @param         n_ranges        Number of ranges in the sequence.
  @param         mode            HA_MRR_* modes to use
  @param[in,out] buf             Buffer to use

  @retval 0     Ok, Scan started.
  @retval other Error
*/

int DsMrr_impl::dsmrr_init(RANGE_SEQ_IF *seq_funcs, void *seq_init_param,
                           uint n_ranges, uint mode, HANDLER_BUFFER *buf) {
  assert(table != nullptr);  // Verify init() called

  uint elem_size;
  int retval = 0;
  DBUG_TRACE;
  THD *const thd = table->in_use;  // current THD

  if (!hint_key_state(thd, table->pos_in_table_list, h->active_index,
                      MRR_HINT_ENUM, OPTIMIZER_SWITCH_MRR) ||
      mode & (HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED))  // DS-MRR doesn't sort
  {
    use_default_impl = true;
    retval = h->handler::multi_range_read_init(seq_funcs, seq_init_param,
                                               n_ranges, mode, buf);
    return retval;
  }

  /*
    This assert will hit if we have pushed an index condition to the
    primary key index and then "change our mind" and use a different
    index for retrieving data with MRR. One of the following criteria
    must be true:
      1. We have not pushed an index condition on this handler.
      2. We have pushed an index condition and this is on the currently used
         index.
      3. We have pushed an index condition but this is not for the primary key.
      4. We have pushed an index condition and this has been transferred to
         the clone (h2) of the handler object.
  */
  assert(!h->pushed_idx_cond || h->pushed_idx_cond_keyno == h->active_index ||
         h->pushed_idx_cond_keyno != table->s->primary_key ||
         (h2 && h->pushed_idx_cond_keyno == h2->active_index));

  rowids_buf = buf->buffer;

  is_mrr_assoc = !(mode & HA_MRR_NO_ASSOCIATION);

  if (is_mrr_assoc) {
    assert(!thd->status_var_aggregated);
    table->in_use->status_var.ha_multi_range_read_init_count++;
  }

  rowids_buf_end = buf->buffer_end;
  elem_size = h->ref_length + (int)is_mrr_assoc * sizeof(void *);
  rowids_buf_last =
      rowids_buf + ((rowids_buf_end - rowids_buf) / elem_size) * elem_size;
  rowids_buf_end = rowids_buf_last;

  /*
    The DS-MRR scan uses a second handler object (h2) for doing the
    index scan. Create this by cloning the primary handler
    object. The h2 handler object is deleted when DsMrr_impl::reset()
    is called.
  */
  if (!h2) {
    handler *new_h2;
    /*
      ::clone() takes up a lot of stack, especially on 64 bit platforms.
      The constant 5 is an empiric result.
      @todo Is this still the case? Leave it as it is for now but could
            likely be removed?
    */
    if (check_stack_overrun(thd, 5 * STACK_MIN_SIZE, (uchar *)&new_h2))
      return 1;

    if (!(new_h2 = h->clone(table->s->normalized_path.str, thd->mem_root)))
      return 1;
    h2 = new_h2; /* Ok, now can put it into h2 */
    table->prepare_for_position();
  }

  /*
    Open the index scan on h2 using the key from the primary handler.
  */
  if (h2->active_index == MAX_KEY) {
    assert(h->active_index != MAX_KEY);
    const uint mrr_keyno = h->active_index;

    if ((retval = h2->ha_external_lock(thd, h->get_lock_type()))) goto error;

    if ((retval = h2->extra(HA_EXTRA_KEYREAD))) goto error;

    if ((retval = h2->ha_index_init(mrr_keyno, false))) goto error;

    if ((table->key_info[mrr_keyno].flags & HA_MULTI_VALUED_KEY) &&
        (retval = h2->ha_extra(HA_EXTRA_ENABLE_UNIQUE_RECORD_FILTER)))
      goto error; /* purecov: inspected */

    // Transfer ICP from h to h2
    if (mrr_keyno == h->pushed_idx_cond_keyno) {
      if (h2->idx_cond_push(mrr_keyno, h->pushed_idx_cond)) {
        retval = 1;
        goto error;
      }
    } else {
      // Cancel any potentially previously pushed index conditions
      h2->cancel_pushed_idx_cond();
    }
  } else {
    /*
      h2 has already an open index. This happens when the DS-MRR scan
      is re-started without closing it first. In this case the primary
      handler must be used for reading records from the table, ie. it
      must not be opened for doing a new range scan. In this case
      the active_index must either not be set or be the primary key.
    */
    assert(h->inited == handler::RND);
    assert(h->active_index == MAX_KEY ||
           h->active_index == table->s->primary_key);
  }

  /*
    The index scan is now transferred to h2 and we can close the open
    index scan on the primary handler.
  */
  if (h->inited == handler::INDEX) {
    /*
      Calling h->ha_index_end() will invoke dsmrr_close() for this object,
      which will close the index scan on h2. We need to keep it open, so
      temporarily move h2 out of the DsMrr object.
    */
    handler *save_h2 = h2;
    h2 = nullptr;
    retval = h->ha_index_end();
    h2 = save_h2;
    if (retval) goto error;
  }

  /*
    Verify consistency between h and h2.
  */
  assert(h->inited != handler::INDEX);
  assert(h->active_index == MAX_KEY ||
         h->active_index == table->s->primary_key);
  assert(h2->inited == handler::INDEX);
  assert(h2->active_index != MAX_KEY);
  assert(h->get_lock_type() == h2->get_lock_type());

  if ((retval = h2->handler::multi_range_read_init(seq_funcs, seq_init_param,
                                                   n_ranges, mode, buf)))
    goto error;

  if ((retval = dsmrr_fill_buffer())) goto error;

  /*
    If the above call has scanned through all intervals in *seq, then
    adjust *buf to indicate that the remaining buffer space will not be used.
  */
  if (dsmrr_eof) buf->end_of_used_area = rowids_buf_last;

  /*
     h->inited == INDEX may occur when 'range checked for each record' is
     used.
  */
  if ((h->inited != handler::RND) &&
      ((h->inited == handler::INDEX ? h->ha_index_end() : false) ||
       (h->ha_rnd_init(false)))) {
    retval = 1;
    goto error;
  }

  use_default_impl = false;
  h->mrr_funcs = *seq_funcs;

  return 0;
error:
  h2->ha_index_or_rnd_end();
  h2->ha_external_lock(thd, F_UNLCK);
  h2->ha_close();
  destroy(h2);
  h2 = nullptr;
  assert(retval != 0);
  return retval;
}

void DsMrr_impl::dsmrr_close() {
  DBUG_TRACE;

  // If there is an open index on h2, then close it
  if (h2 && h2->active_index != MAX_KEY) {
    h2->ha_index_or_rnd_end();
    h2->ha_external_lock(current_thd, F_UNLCK);
  }
  use_default_impl = true;
}

void DsMrr_impl::reset() {
  DBUG_TRACE;

  if (h2) {
    // Close any ongoing DS-MRR scan
    dsmrr_close();

    // Close and delete the h2 handler
    h2->ha_close();
    destroy(h2);
    h2 = nullptr;
  }
}

/**
  DS-MRR: Fill the buffer with rowids and sort it by rowid

  {This is an internal function of DiskSweep MRR implementation}
  Scan the MRR ranges and collect ROWIDs (or {ROWID, range_id} pairs) into
  buffer. When the buffer is full or scan is completed, sort the buffer by
  rowid and return.

  The function assumes that rowids buffer is empty when it is invoked.

  @retval 0      OK, the next portion of rowids is in the buffer,
                 properly ordered
  @retval other  Error
*/

int DsMrr_impl::dsmrr_fill_buffer() {
  char *range_info;
  int res = 0;
  DBUG_TRACE;
  assert(rowids_buf < rowids_buf_end);

  /*
    Set key_read to true since we only read fields from the index.
    This ensures that any virtual columns are read from index and are not
    attempted to be evaluated from base columns.
    (Do not use TABLE::set_keyread() since the MRR implementation operates
    with two handler objects, and set_keyread() would manipulate the keyread
    property of the wrong handler. MRR sets the handlers' keyread properties
    when initializing the MRR operation, independent of this call).
  */
  bool table_keyread_save = table->key_read;
  table->key_read = true;

  rowids_buf_cur = rowids_buf;
  /*
    Do not use ha_multi_range_read_next() as it would call the engine's
    overridden multi_range_read_next() but the default implementation is wanted.
  */
  while ((rowids_buf_cur < rowids_buf_end) &&
         !(res = h2->handler::multi_range_read_next(&range_info))) {
    /* Put rowid, or {rowid, range_id} pair into the buffer */
    h2->position(table->record[0]);
    memcpy(rowids_buf_cur, h2->ref, h2->ref_length);
    rowids_buf_cur += h2->ref_length;

    if (is_mrr_assoc) {
      memcpy(rowids_buf_cur, &range_info, sizeof(void *));
      rowids_buf_cur += sizeof(void *);
    }
  }

  // Restore key_read since the next read operation might read complete rows
  table->key_read = table_keyread_save;

  if (res && res != HA_ERR_END_OF_FILE) return res;
  dsmrr_eof = (res == HA_ERR_END_OF_FILE);

  /* Sort the buffer contents by rowid */
  uint elem_size = h->ref_length + (int)is_mrr_assoc * sizeof(void *);
  assert((rowids_buf_cur - rowids_buf) % elem_size == 0);

  varlen_sort(
      rowids_buf, rowids_buf_cur, elem_size,
      [this](const uchar *a, const uchar *b) { return h->cmp_ref(a, b) < 0; });
  rowids_buf_last = rowids_buf_cur;
  rowids_buf_cur = rowids_buf;
  return 0;
}

/*
  DS-MRR implementation: multi_range_read_next() function
*/

int DsMrr_impl::dsmrr_next(char **range_info) {
  int res;
  uchar *cur_range_info = nullptr;
  uchar *rowid;

  if (use_default_impl) return h->handler::multi_range_read_next(range_info);

  do {
    if (rowids_buf_cur == rowids_buf_last) {
      if (dsmrr_eof) {
        res = HA_ERR_END_OF_FILE;
        goto end;
      }

      res = dsmrr_fill_buffer();
      if (res) goto end;
    }

    /* return eof if there are no rowids in the buffer after re-fill attempt */
    if (rowids_buf_cur == rowids_buf_last) {
      res = HA_ERR_END_OF_FILE;
      goto end;
    }
    rowid = rowids_buf_cur;

    if (is_mrr_assoc)
      memcpy(&cur_range_info, rowids_buf_cur + h->ref_length, sizeof(uchar *));

    rowids_buf_cur += h->ref_length + sizeof(void *) * is_mrr_assoc;
    if (h2->mrr_funcs.skip_record &&
        h2->mrr_funcs.skip_record(h2->mrr_iter, (char *)cur_range_info, rowid))
      continue;
    res = h->ha_rnd_pos(table->record[0], rowid);
    break;
  } while (true);

  if (is_mrr_assoc) {
    memcpy(range_info, rowid + h->ref_length, sizeof(void *));
  }
end:
  return res;
}

/*
  DS-MRR implementation: multi_range_read_info() function
*/
ha_rows DsMrr_impl::dsmrr_info(uint keyno, uint n_ranges, uint rows,
                               uint *bufsz, uint *flags, Cost_estimate *cost) {
  ha_rows res [[maybe_unused]];
  uint def_flags = *flags;
  uint def_bufsz = *bufsz;

  /* Get cost/flags/mem_usage of default MRR implementation */
  res = h->handler::multi_range_read_info(keyno, n_ranges, rows, &def_bufsz,
                                          &def_flags, cost);
  assert(!res);

  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) ||
      choose_mrr_impl(keyno, rows, flags, bufsz, cost)) {
    /* Default implementation is chosen */
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags = def_flags;
    *bufsz = def_bufsz;
    assert(*flags & HA_MRR_USE_DEFAULT_IMPL);
  } else {
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
                                     uint *bufsz, uint *flags,
                                     Cost_estimate *cost) {
  ha_rows rows;
  uint def_flags = *flags;
  uint def_bufsz = *bufsz;
  /* Get cost/flags/mem_usage of default MRR implementation */
  rows = h->handler::multi_range_read_info_const(
      keyno, seq, seq_init_param, n_ranges, &def_bufsz, &def_flags, cost);
  if (rows == HA_POS_ERROR) {
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
      choose_mrr_impl(keyno, rows, flags, bufsz, cost)) {
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags = def_flags;
    *bufsz = def_bufsz;
    assert(*flags & HA_MRR_USE_DEFAULT_IMPL);
  } else {
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
                OUT  If DS-MRR is chosen, flags of DS-MRR implementation
                     else the value is not modified
  @param bufsz  IN   If DS-MRR is chosen, buffer use of DS-MRR implementation
                     else the value is not modified
  @param cost   IN   Cost of default MRR implementation
                OUT  If DS-MRR is chosen, cost of DS-MRR scan
                     else the value is not modified

  @retval true   Default MRR implementation should be used
  @retval false  DS-MRR implementation should be used
*/

bool DsMrr_impl::choose_mrr_impl(uint keyno, ha_rows rows, uint *flags,
                                 uint *bufsz, Cost_estimate *cost) {
  bool res;
  THD *thd = current_thd;
  Table_ref *tl = table->pos_in_table_list;
  const bool mrr_on =
      hint_key_state(thd, tl, keyno, MRR_HINT_ENUM, OPTIMIZER_SWITCH_MRR);
  const bool force_dsmrr_by_hints =
      hint_key_state(thd, tl, keyno, MRR_HINT_ENUM, 0) ||
      hint_table_state(thd, tl, BKA_HINT_ENUM, 0);

  if (!(mrr_on || force_dsmrr_by_hints) ||
      *flags & (HA_MRR_INDEX_ONLY | HA_MRR_SORTED) ||  // Unsupported by DS-MRR
      (keyno == table->s->primary_key && h->primary_key_is_clustered()) ||
      key_uses_partial_cols(table, keyno) ||
      table->s->tmp_table != NO_TMP_TABLE) {
    /* Use the default implementation, don't modify args: See comments  */
    return true;
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
      !force_dsmrr_by_hints) {
    /*
      If the storage engine has a database buffer we use this as the
      minimum size the table should have before considering DS-MRR.
    */
    longlong min_file_size = table->file->get_memory_buffer_size();
    if (min_file_size == -1) {
      // No estimate for database buffer
      min_file_size = 100 * 1024 * 1024;  // 100 MB
    }

    if (table->file->stats.data_file_length <
            static_cast<ulonglong>(min_file_size) ||
        rows <= 50)
      return true;  // Use the default implementation
  }

  Cost_estimate dsmrr_cost;
  if (get_disk_sweep_mrr_cost(keyno, rows, *flags, bufsz, &dsmrr_cost))
    return true;

  /*
    If @@optimizer_switch has "mrr" on and "mrr_cost_based" off, then set cost
    of DS-MRR to be minimum of DS-MRR and Default implementations cost. This
    allows one to force use of DS-MRR whenever it is applicable without
    affecting other cost-based choices. Note that if MRR or BKA hint is
    specified, DS-MRR will be used regardless of cost.
  */
  const bool force_dsmrr =
      (force_dsmrr_by_hints ||
       !thd->optimizer_switch_flag(OPTIMIZER_SWITCH_MRR_COST_BASED));

  if (force_dsmrr && dsmrr_cost.total_cost() > cost->total_cost())
    dsmrr_cost = *cost;

  if (force_dsmrr || (dsmrr_cost.total_cost() <= cost->total_cost())) {
    *flags &= ~HA_MRR_USE_DEFAULT_IMPL; /* Use the DS-MRR implementation */
    *flags &= ~HA_MRR_SUPPORT_SORTED;   /* We can't provide ordered output */
    *cost = dsmrr_cost;
    res = false;
  } else {
    /* Use the default MRR implementation */
    res = true;
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

  @retval false  OK
  @retval true   Error, DS-MRR cannot be used (the buffer is too small
                 for even 1 rowid)
*/

bool DsMrr_impl::get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags,
                                         uint *buffer_size,
                                         Cost_estimate *cost) {
  ha_rows rows_in_last_step;
  uint n_full_steps;

  const uint elem_size =
      h->ref_length + sizeof(void *) * !(flags & HA_MRR_NO_ASSOCIATION);
  const ha_rows max_buff_entries = *buffer_size / elem_size;

  if (!max_buff_entries)
    return true; /* Buffer has not enough space for even 1 rowid */

  /* Number of iterations we'll make with full buffer */
  n_full_steps = (uint)floor(rows2double(rows) / max_buff_entries);

  /*
    Get numbers of rows we'll be processing in last iteration, with
    non-full buffer
  */
  rows_in_last_step = rows % max_buff_entries;

  assert(cost->is_zero());

  if (n_full_steps) {
    get_sort_and_sweep_cost(table, max_buff_entries, cost);
    cost->multiply(n_full_steps);
  } else {
    /*
      Adjust buffer size since only parts of the buffer will be used:
      1. Adjust record estimate for the last scan to reduce likelihood
         of needing more than one scan by adding 20 percent to the
         record estimate and by ensuring this is at least 100 records.
      2. If the estimated needed buffer size is lower than suggested by
         the caller then set it to the estimated buffer size.
    */
    const ha_rows keys_in_buffer =
        max<ha_rows>(static_cast<ha_rows>(1.2 * rows_in_last_step), 100);
    *buffer_size = min<ulong>(*buffer_size,
                              static_cast<ulong>(keys_in_buffer) * elem_size);
  }

  Cost_estimate last_step_cost;
  get_sort_and_sweep_cost(table, rows_in_last_step, &last_step_cost);
  (*cost) += last_step_cost;

  /*
    Cost of memory is not included in the total_cost() function and
    thus will not be considered when comparing costs. Still, we
    record it in the cost estimate object for future use.
  */
  cost->add_mem(*buffer_size);

  /* Total cost of all index accesses */
  (*cost) += h->index_scan_cost(keynr, 1, static_cast<double>(rows));

  /*
    Add CPU cost for processing records (see
    @handler::multi_range_read_info_const()).
  */
  cost->add_cpu(
      table->cost_model()->row_evaluate_cost(static_cast<double>(rows)));
  return false;
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

static void get_sort_and_sweep_cost(TABLE *table, ha_rows nrows,
                                    Cost_estimate *cost) {
  assert(cost->is_zero());
  if (nrows) {
    get_sweep_read_cost(table, nrows, false, cost);

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
    const double ROWID_COMPARE_SORT_COST =
        table->cost_model()->key_compare_cost(1.0) / 10;

    /* Add cost of qsort call: n * log2(n) * cost(rowid_comparison) */

    // For the old version of the cost model this cost calculations should
    // be used....
    const double cpu_sort = nrows * log2(nrows) * ROWID_COMPARE_SORT_COST;
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
                         Cost_estimate *cost) {
  DBUG_TRACE;

  assert(cost->is_zero());
  if (nrows > 0) {
    const Cost_model_table *const cost_model = table->cost_model();

    // The total number of blocks used by this table
    double n_blocks =
        ceil(ulonglong2double(table->file->stats.data_file_length) / IO_SIZE);
    if (n_blocks < 1.0)  // When data_file_length is 0
      n_blocks = 1.0;

    /*
      The number of blocks that in average need to be read given that
      the records are uniformly distribution over the table.
    */
    double busy_blocks =
        n_blocks * (1.0 - pow(1.0 - 1.0 / n_blocks, rows2double(nrows)));
    if (busy_blocks < 1.0) busy_blocks = 1.0;

    DBUG_PRINT("info",
               ("sweep: nblocks=%g, busy_blocks=%g", n_blocks, busy_blocks));
    /*
      The random access cost for reading the data pages will be the upper
      limit for the sweep_cost.
    */
    cost->add_io(cost_model->page_read_cost(busy_blocks));
    if (!interrupted) {
      Cost_estimate sweep_cost;
      /*
        Assume reading pages from disk is done in one 'sweep'.

        The cost model and cost estimate for pages already in a memory
        buffer will be different from pages that needed to be read from
        disk. Calculate the number of blocks that likely already are
        in memory and the number of blocks that need to be read from
        disk.
      */
      const double busy_blocks_mem =
          busy_blocks * table->file->table_in_memory_estimate();
      const double busy_blocks_disk = busy_blocks - busy_blocks_mem;
      assert(busy_blocks_disk >= 0.0);

      // Cost of accessing blocks in main memory buffer
      sweep_cost.add_io(cost_model->buffer_block_read_cost(busy_blocks_mem));

      // Cost of reading blocks from disk in a 'sweep'
      const double seek_distance =
          (busy_blocks_disk > 1.0) ? n_blocks / busy_blocks_disk : n_blocks;

      const double disk_cost =
          busy_blocks_disk * cost_model->disk_seek_cost(seek_distance);
      sweep_cost.add_io(disk_cost);

      /*
        For some cases, ex: when only few blocks need to be read and the
        seek distance becomes very large, the sweep cost model can produce
        a cost estimate that is larger than the cost of random access.
        To handle this case, we use the sweep cost only when it is less
        than the random access cost.
      */
      if (sweep_cost < *cost) *cost = sweep_cost;
    }
  }
  DBUG_PRINT("info", ("returning cost=%g", cost->total_cost()));
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
*/
int handler::read_range_first(const key_range *start_key,
                              const key_range *end_key, bool eq_range_arg,
                              bool sorted [[maybe_unused]]) {
  int result;
  DBUG_TRACE;

  eq_range = eq_range_arg;
  set_end_range(end_key, RANGE_SCAN_ASC);

  range_key_part = table->key_info[active_index].key_part;

  if (!start_key)  // Read first record
    result = ha_index_first(table->record[0]);
  else
    result = ha_index_read_map(table->record[0], start_key->key,
                               start_key->keypart_map, start_key->flag);
  if (result)
    return (result == HA_ERR_KEY_NOT_FOUND) ? HA_ERR_END_OF_FILE : result;

  if (compare_key(end_range) > 0) {
    /*
      The last read row does not fall in the range. So request
      storage engine to release row lock if possible.
    */
    unlock_row();
    result = HA_ERR_END_OF_FILE;
  }
  return result;
}

int handler::ha_read_range_first(const key_range *start_key,
                                 const key_range *end_key, bool eq_range,
                                 bool sorted) {
  int result;
  DBUG_TRACE;

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  result = read_range_first(start_key, end_key, eq_range, sorted);
  if (!result && m_update_generated_read_fields) {
    result =
        update_generated_read_fields(table->record[0], table, active_index);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
}

int handler::ha_read_range_next() {
  int result;
  DBUG_TRACE;

  // Set status for the need to update generated fields
  m_update_generated_read_fields = table->has_gcol();

  result = read_range_next();
  if (!result && m_update_generated_read_fields) {
    result =
        update_generated_read_fields(table->record[0], table, active_index);
    m_update_generated_read_fields = false;
  }
  table->set_row_status_from_handler(result);
  return result;
}

/** @brief
  Read next row between two endpoints.

  @note
    Record is read into table->record[0]

  @retval
    0			Found row
  @retval
    HA_ERR_END_OF_FILE	No rows in range
*/
int handler::read_range_next() {
  DBUG_TRACE;

  int result;
  if (eq_range) {
    /* We trust that index_next_same always gives a row in range */
    result =
        ha_index_next_same(table->record[0], end_range->key, end_range->length);
  } else {
    result = ha_index_next(table->record[0]);
    if (result) return result;

    if (compare_key(end_range) > 0) {
      /*
        The last read row does not fall in the range. So request
        storage engine to release row lock if possible.
      */
      unlock_row();
      result = HA_ERR_END_OF_FILE;
    }
  }
  return result;
}

/**
  Check if one of the columns in a key is a virtual generated column.

  @param part    the first part of the key to check
  @param length  the length of the key
  @retval true   if the key contains a virtual generated column
  @retval false  if the key does not contain a virtual generated column
*/
static bool key_has_vcol(const KEY_PART_INFO *part, uint length) {
  for (uint len = 0; len < length; len += part->store_length, ++part)
    if (part->field->is_virtual_gcol()) return true;
  return false;
}

void handler::set_end_range(const key_range *range,
                            enum_range_scan_direction direction) {
  if (range) {
    save_end_range = *range;
    end_range = &save_end_range;
    range_key_part = table->key_info[active_index].key_part;
    key_compare_result_on_equal =
        (range->flag == HA_READ_BEFORE_KEY)
            ? 1
            : (range->flag == HA_READ_AFTER_KEY ? -1 : 0);
    m_virt_gcol_in_end_range = key_has_vcol(range_key_part, range->length);
  } else
    end_range = nullptr;

  /*
    Clear the out-of-range flag in the record buffer when a new range is
    started. Also set the in_range_check_pushed_down flag, since the
    storage engine needs to do the evaluation of the end-range to avoid
    filling the record buffer with out-of-range records.
  */
  if (m_record_buffer != nullptr) {
    m_record_buffer->set_out_of_range(false);
    in_range_check_pushed_down = true;
  }

  range_scan_direction = direction;
}

/**
  Compare if found key (in row) is over max-value.

  @param range		range to compare to row. May be 0 for no range

  @sa
    key.cc::key_cmp()

  @return
    The return value is SIGN(key_in_row - range_key):

    - 0   : Key is equal to range or 'range' == 0 (no range)
    - -1  : Key is less than range
    - 1   : Key is larger than range
*/
int handler::compare_key(key_range *range) {
  int cmp;
  if (!range || in_range_check_pushed_down) return 0;  // No max range
  cmp = key_cmp(range_key_part, range->key, range->length);
  if (!cmp) cmp = key_compare_result_on_equal;
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

int handler::compare_key_icp(const key_range *range) const {
  int cmp;
  if (!range) return 0;  // no max range
  cmp = key_cmp(range_key_part, range->key, range->length);
  if (!cmp) cmp = key_compare_result_on_equal;
  if (range_scan_direction == RANGE_SCAN_DESC) cmp = -cmp;
  return cmp;
}

/**
  Change the offsets of all the fields in a key range.

  @param range     the key range
  @param key_part  the first key part
  @param diff      how much to change the offsets with
*/
static inline void move_key_field_offsets(const key_range *range,
                                          const KEY_PART_INFO *key_part,
                                          ptrdiff_t diff) {
  for (size_t len = 0; len < range->length;
       len += key_part->store_length, ++key_part)
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
int handler::compare_key_in_buffer(const uchar *buf) const {
  assert(end_range != nullptr &&
         (m_record_buffer == nullptr || !m_record_buffer->is_out_of_range()));

  /*
    End range on descending scans is only checked with ICP for now, and then we
    check it with compare_key_icp() instead of this function.
  */
  assert(range_scan_direction == RANGE_SCAN_ASC);

  // Make the fields in the key point into the buffer instead of record[0].
  const ptrdiff_t diff = buf - table->record[0];
  if (diff != 0) move_key_field_offsets(end_range, range_key_part, diff);

  // Compare the key in buf against end_range.
  int cmp = key_cmp(range_key_part, end_range->key, end_range->length);
  if (cmp == 0) cmp = key_compare_result_on_equal;

  // Reset the field offsets.
  if (diff != 0) move_key_field_offsets(end_range, range_key_part, -diff);

  return cmp;
}

int handler::index_read_idx_map(uchar *buf, uint index, const uchar *key,
                                key_part_map keypart_map,
                                enum ha_rkey_function find_flag) {
  int error, error1 = 0;
  error = index_init(index, false);
  if (!error) {
    error = index_read_map(buf, key, keypart_map, find_flag);
    error1 = index_end();
  }
  return error ? error : error1;
}

uint calculate_key_len(TABLE *table, uint key, key_part_map keypart_map) {
  /* works only with key prefixes */
  assert(((keypart_map + 1) & keypart_map) == 0);

  KEY *key_info = table->key_info + key;
  KEY_PART_INFO *key_part = key_info->key_part;
  KEY_PART_INFO *end_key_part = key_part + actual_key_parts(key_info);
  uint length = 0;

  while (key_part < end_key_part && keypart_map) {
    length += key_part->store_length;
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
static bool exts_handlerton(THD *, plugin_ref plugin, void *arg) {
  List<const char> *found_exts = static_cast<List<const char> *>(arg);
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->file_extensions) {
    List_iterator_fast<const char> it(*found_exts);
    const char **ext, *old_ext;

    for (ext = hton->file_extensions; *ext; ext++) {
      while ((old_ext = it++)) {
        if (!strcmp(old_ext, *ext)) break;
      }
      if (!old_ext) found_exts->push_back(*ext);

      it.rewind();
    }
  }
  return false;
}

TYPELIB *ha_known_exts() {
  TYPELIB *known_extensions = (TYPELIB *)(*THR_MALLOC)->Alloc(sizeof(TYPELIB));
  known_extensions->name = "known_exts";
  known_extensions->type_lengths = nullptr;

  List<const char> found_exts;
  const char **ext, *old_ext;

  plugin_foreach(nullptr, exts_handlerton, MYSQL_STORAGE_ENGINE_PLUGIN,
                 &found_exts);

  size_t arr_length = sizeof(char *) * (found_exts.elements + 1);
  ext = (const char **)(*THR_MALLOC)->Alloc(arr_length);

  assert(nullptr != ext);
  known_extensions->count = found_exts.elements;
  known_extensions->type_names = ext;

  List_iterator_fast<const char> it(found_exts);
  while ((old_ext = it++)) *ext++ = old_ext;
  *ext = nullptr;
  return known_extensions;
}

static bool stat_print(THD *thd, const char *type, size_t type_len,
                       const char *file, size_t file_len, const char *status,
                       size_t status_len) {
  Protocol *protocol = thd->get_protocol();
  protocol->start_row();
  protocol->store_string(type, type_len, system_charset_info);
  protocol->store_string(file, file_len, system_charset_info);
  protocol->store_string(status, status_len, system_charset_info);
  if (protocol->end_row()) return true;
  return false;
}

static bool showstat_handlerton(THD *thd, plugin_ref plugin, void *arg) {
  enum ha_stat_type stat = *(enum ha_stat_type *)arg;
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->show_status &&
      hton->show_status(hton, thd, stat_print, stat))
    return true;
  return false;
}

bool ha_show_status(THD *thd, handlerton *db_type, enum ha_stat_type stat) {
  mem_root_deque<Item *> field_list(thd->mem_root);
  field_list.push_back(new Item_empty_string("Type", 10));
  field_list.push_back(new Item_empty_string("Name", FN_REFLEN));
  field_list.push_back(new Item_empty_string("Status", 10));

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  bool result;
  if (db_type == nullptr) {
    result = plugin_foreach(thd, showstat_handlerton,
                            MYSQL_STORAGE_ENGINE_PLUGIN, &stat);
  } else {
    if (db_type->state != SHOW_OPTION_YES) {
      const LEX_CSTRING *name = &se_plugin_array[db_type->slot]->name;
      result = stat_print(thd, name->str, name->length, "", 0, "DISABLED", 8)
                   ? true
                   : false;
    } else {
      DBUG_EXECUTE_IF("simulate_show_status_failure",
                      DBUG_SET("+d,simulate_net_write_failure"););
      result = db_type->show_status &&
                       db_type->show_status(db_type, thd, stat_print, stat)
                   ? true
                   : false;
      DBUG_EXECUTE_IF("simulate_show_status_failure",
                      DBUG_SET("-d,simulate_net_write_failure"););
    }
  }

  if (!result) my_eof(thd);
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

static bool check_table_binlog_row_based(THD *thd, TABLE *table) {
  if (table->s->cached_row_logging_check == -1) {
    int const check(table->s->tmp_table == NO_TMP_TABLE &&
                    !table->no_replicate &&
                    binlog_filter->db_ok(table->s->db.str));
    table->s->cached_row_logging_check = check;
  }

  assert(table->s->cached_row_logging_check == 0 ||
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

static int write_locked_table_maps(THD *thd) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("thd: %p  thd->lock: %p "
                       "thd->extra_lock: %p",
                       thd, thd->lock, thd->extra_lock));

  DBUG_PRINT("debug",
             ("get_binlog_table_maps(): %d", thd->get_binlog_table_maps()));

  if (thd->get_binlog_table_maps() == 0) {
    for (MYSQL_LOCK *lock : {thd->extra_lock, thd->lock}) {
      if (lock == nullptr) continue;

      bool need_binlog_rows_query = thd->variables.binlog_rows_query_log_events;
      TABLE **const end_ptr = lock->table + lock->table_count;
      for (TABLE **table_ptr = lock->table; table_ptr != end_ptr; ++table_ptr) {
        TABLE *const table = *table_ptr;
        DBUG_PRINT("info", ("Checking table %s", table->s->table_name.str));
        if (table->current_lock == F_WRLCK &&
            check_table_binlog_row_based(thd, table)) {
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
          bool const has_trans = thd->lex->sql_command == SQLCOM_CREATE_TABLE ||
                                 table->file->has_transactions();
          int const error = thd->binlog_write_table_map(table, has_trans,
                                                        need_binlog_rows_query);
          /* Binlog Rows_query log event once for one statement which updates
             two or more tables.*/
          if (need_binlog_rows_query) need_binlog_rows_query = false;
          /*
            If an error occurs, it is the responsibility of the caller to
            roll back the transaction.
          */
          if (unlikely(error)) return 1;
        }
      }
    }
  }
  return 0;
}

/**
  The purpose of an instance of this class is to :

  1) Given a TABLE instance, backup the given TABLE::read_set, TABLE::write_set
     and restore those members upon this instance disposal.

  2) Store a reference to a dynamically allocated buffer and dispose of it upon
     this instance disposal.
 */

class Binlog_log_row_cleanup {
 public:
  /**
    This constructor aims to create temporary copies of readset and writeset.

    @param table                 A pointer to TABLE object
    @param temp_read_bitmap      Temporary BITMAP to store read_set.
    @param temp_write_bitmap     Temporary BITMAP to store write_set.
    */
  Binlog_log_row_cleanup(TABLE &table, MY_BITMAP &temp_read_bitmap,

                         MY_BITMAP &temp_write_bitmap)

      : m_cleanup_table(table),
        m_cleanup_read_bitmap(temp_read_bitmap),
        m_cleanup_write_bitmap(temp_write_bitmap) {
    bitmap_copy(&this->m_cleanup_read_bitmap, this->m_cleanup_table.read_set);
    bitmap_copy(&this->m_cleanup_write_bitmap, this->m_cleanup_table.write_set);
  }

  /**
    This destructor aims to restore the original readset and writeset and
    delete the temporary copies.
   */
  virtual ~Binlog_log_row_cleanup() {
    bitmap_copy(this->m_cleanup_table.read_set, &this->m_cleanup_read_bitmap);
    bitmap_copy(this->m_cleanup_table.write_set, &this->m_cleanup_write_bitmap);
    bitmap_free(&this->m_cleanup_read_bitmap);
    bitmap_free(&this->m_cleanup_write_bitmap);
  }

 private:
  TABLE &m_cleanup_table;  // Creating a TABLE to get access to its members.
  MY_BITMAP &m_cleanup_read_bitmap;   // Temporary bitmap to store read_set.
  MY_BITMAP &m_cleanup_write_bitmap;  // Temporary bitmap to store write_set.
};

int binlog_log_row(TABLE *table, const uchar *before_record,
                   const uchar *after_record, Log_func *log_func) {
  bool error = false;
  THD *const thd = table->in_use;

  if (check_table_binlog_row_based(thd, table)) {
    if (thd->variables.transaction_write_set_extraction != HASH_ALGORITHM_OFF) {
      try {
        MY_BITMAP save_read_set;
        MY_BITMAP save_write_set;
        if (bitmap_init(&save_read_set, nullptr, table->s->fields) ||
            bitmap_init(&save_write_set, nullptr, table->s->fields)) {
          my_error(ER_OUT_OF_RESOURCES, MYF(0));
          return HA_ERR_RBR_LOGGING_FAILED;
        }

        Binlog_log_row_cleanup cleanup_sentry(*table, save_read_set,
                                              save_write_set);

        if (thd->variables.binlog_row_image == 0) {
          for (uint key_number = 0; key_number < table->s->keys; ++key_number) {
            if (((table->key_info[key_number].flags & (HA_NOSAME)) ==
                 HA_NOSAME)) {
              table->mark_columns_used_by_index_no_reset(key_number,
                                                         table->read_set);
              table->mark_columns_used_by_index_no_reset(key_number,
                                                         table->write_set);
            }
          }
        }
        std::array<const uchar *, 2> records{after_record, before_record};
        for (auto rec : records) {
          if (rec != nullptr) {
            assert(rec == table->record[0] || rec == table->record[1]);
            bool res = add_pke(table, thd, rec);
            if (res) return HA_ERR_RBR_LOGGING_FAILED;
          }
        }
      } catch (const std::bad_alloc &) {
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
        return HA_ERR_RBR_LOGGING_FAILED;
      }
    }
    if (table->in_use->is_error()) return error ? HA_ERR_RBR_LOGGING_FAILED : 0;

    DBUG_DUMP("read_set 10", (uchar *)table->read_set->bitmap,
              (table->s->fields + 7) / 8);

    /*
      If there are no table maps written to the binary log, this is
      the first row handled in this statement. In that case, we need
      to write table maps for all locked tables to the binary log.
    */
    if (likely(!(error = write_locked_table_maps(thd)))) {
      /*
        We need to have a transactional behavior for SQLCOM_CREATE_TABLE
        (i.e. CREATE TABLE... SELECT * FROM TABLE) in order to keep a
        compatible behavior with the STMT based replication even when
        the table is not transactional. In other words, if the operation
        fails while executing the insert phase nothing is written to the
        binlog.
      */
      bool const has_trans = thd->lex->sql_command == SQLCOM_CREATE_TABLE ||
                             table->file->has_transactions();
      error = (*log_func)(thd, table, has_trans, before_record, after_record);
    }
  }

  return error ? HA_ERR_RBR_LOGGING_FAILED : 0;
}

int handler::ha_external_lock(THD *thd, int lock_type) {
  int error;
  DBUG_TRACE;
  /*
    Whether this is lock or unlock, this should be true, and is to verify that
    if get_auto_increment() was called (thus may have reserved intervals or
    taken a table lock), ha_release_auto_increment() was too.
  */
  assert(next_insert_id == 0);
  /* Consecutive calls for lock without unlocking in between is not allowed */
  assert(table_share->tmp_table != NO_TMP_TABLE ||
         ((lock_type != F_UNLCK && m_lock_type == F_UNLCK) ||
          lock_type == F_UNLCK));
  /* SQL HANDLER call locks/unlock while scanning (RND/INDEX). */
  assert(inited == NONE || table->open_by_handler);

  ha_statistic_increment(&System_status_var::ha_external_lock_count);

  MYSQL_TABLE_LOCK_WAIT(PSI_TABLE_EXTERNAL_LOCK, lock_type,
                        { error = external_lock(thd, lock_type); })

  /*
    We cache the table flags if the locking succeeded. Otherwise, we
    keep them as they were when they were fetched in ha_open().
  */

  if (error == 0) {
    /*
      The lock type is needed by MRR when creating a clone of this handler
      object.
    */
    m_lock_type = lock_type;
    cached_table_flags = table_flags();
  }

  return error;
}

/** @brief
  Check handler usage and reset state of file to after 'open'

  @note can be called regardless of it is locked or not.
*/
int handler::ha_reset() {
  DBUG_TRACE;
  /* Check that we have called all proper deallocation functions */
  assert((uchar *)table->def_read_set.bitmap + table->s->column_bitmap_size ==
         (uchar *)table->def_write_set.bitmap);
  assert(bitmap_is_set_all(&table->s->all_set));
  assert(table->key_read == 0);
  /* ensure that ha_index_end / ha_rnd_end has been called */
  assert(inited == NONE);
  /* Free cache used by filesort */
  free_io_cache(table);
  /* reset the bitmaps to point to defaults */
  table->default_column_bitmaps();
  /* Reset the handler flags used for dupilcate record handling */
  table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
  /* Reset information about pushed engine conditions */
  pushed_cond = nullptr;
  /* Reset information about pushed index conditions */
  cancel_pushed_idx_cond();
  // Forget the record buffer.
  m_record_buffer = nullptr;
  m_unique = nullptr;

  const int retval = reset();
  return retval;
}

int handler::ha_write_row(uchar *buf) {
  int error;
  Log_func *log_func = Write_rows_log_event::binlog_row_logging_function;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);

  DBUG_TRACE;
  DBUG_EXECUTE_IF("inject_error_ha_write_row", return HA_ERR_INTERNAL_ERROR;);
  DBUG_EXECUTE_IF("simulate_storage_engine_out_of_memory",
                  return HA_ERR_SE_OUT_OF_MEMORY;);
  mark_trx_read_write();

  DBUG_EXECUTE_IF(
      "handler_crashed_table_on_usage",
      my_error(HA_ERR_CRASHED, MYF(ME_ERRORLOG), table_share->table_name.str);
      set_my_errno(HA_ERR_CRASHED); return HA_ERR_CRASHED;);

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_WRITE_ROW, MAX_KEY, error,
                      { error = write_row(buf); })

  if (unlikely(error)) return error;

  if (unlikely((error = binlog_log_row(table, nullptr, buf, log_func))))
    return error; /* purecov: inspected */

  DEBUG_SYNC_C("ha_write_row_end");
  return 0;
}

int handler::ha_update_row(const uchar *old_data, uchar *new_data) {
  int error;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);
  Log_func *log_func = Update_rows_log_event::binlog_row_logging_function;

  /*
    Some storage engines require that the new record is in record[0]
    (and the old record is in record[1]).
   */
  assert(new_data == table->record[0]);
  assert(old_data == table->record[1]);

  mark_trx_read_write();

  DBUG_EXECUTE_IF(
      "handler_crashed_table_on_usage",
      my_error(HA_ERR_CRASHED, MYF(ME_ERRORLOG), table_share->table_name.str);
      set_my_errno(HA_ERR_CRASHED); return (HA_ERR_CRASHED););

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_UPDATE_ROW, active_index, error,
                      { error = update_row(old_data, new_data); })

  if (unlikely(error)) return error;
  if (unlikely((error = binlog_log_row(table, old_data, new_data, log_func))))
    return error;
  return 0;
}

int handler::ha_delete_row(const uchar *buf) {
  int error;
  assert(table_share->tmp_table != NO_TMP_TABLE || m_lock_type == F_WRLCK);
  Log_func *log_func = Delete_rows_log_event::binlog_row_logging_function;
  /*
    Normally table->record[0] is used, but sometimes table->record[1] is used.
  */
  assert(buf == table->record[0] || buf == table->record[1]);
  DBUG_EXECUTE_IF("inject_error_ha_delete_row", return HA_ERR_INTERNAL_ERROR;);

  DBUG_EXECUTE_IF(
      "handler_crashed_table_on_usage",
      my_error(HA_ERR_CRASHED, MYF(ME_ERRORLOG), table_share->table_name.str);
      set_my_errno(HA_ERR_CRASHED); return (HA_ERR_CRASHED););

  mark_trx_read_write();

  MYSQL_TABLE_IO_WAIT(PSI_TABLE_DELETE_ROW, active_index, error,
                      { error = delete_row(buf); })

  if (unlikely(error)) return error;
  if (unlikely((error = binlog_log_row(table, buf, nullptr, log_func))))
    return error;
  return 0;
}

/** @brief
  use_hidden_primary_key() is called in case of an update/delete when
  (table_flags() and HA_PRIMARY_KEY_REQUIRED_FOR_DELETE) is defined
  but we don't have a primary key
*/
void handler::use_hidden_primary_key() {
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

Handler_share *handler::get_ha_share_ptr() {
  DBUG_TRACE;
  assert(ha_share && table_share);

#ifndef NDEBUG
  if (table_share->tmp_table == NO_TMP_TABLE)
    mysql_mutex_assert_owner(&table_share->LOCK_ha_data);
#endif

  return *ha_share;
}

/**
  Set ha_share to be used by all instances of the same table/partition.

  @param arg_ha_share    Handler_share to be shared.

  @note
  If not a temp table, then LOCK_ha_data must be held.
*/

void handler::set_ha_share_ptr(Handler_share *arg_ha_share) {
  DBUG_TRACE;
  assert(ha_share);
#ifndef NDEBUG
  if (table_share->tmp_table == NO_TMP_TABLE)
    mysql_mutex_assert_owner(&table_share->LOCK_ha_data);
#endif

  *ha_share = arg_ha_share;
}

/**
  Take a lock for protecting shared handler data.
*/

void handler::lock_shared_ha_data() {
  assert(table_share);
  if (table_share->tmp_table == NO_TMP_TABLE)
    mysql_mutex_lock(&table_share->LOCK_ha_data);
}

/**
  Release lock for protecting ha_share.
*/

void handler::unlock_shared_ha_data() {
  assert(table_share);
  if (table_share->tmp_table == NO_TMP_TABLE)
    mysql_mutex_unlock(&table_share->LOCK_ha_data);
}

/**
  This structure is a helper structure for passing the length and pointer of
  blob space allocated by storage engine.
*/
struct blob_len_ptr {
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

static void extract_blob_space_and_length_from_record_buff(
    const TABLE *table, const MY_BITMAP *const fields,
    blob_len_ptr *blob_len_ptr_array) {
  int num = 0;
  for (Field **vfield = table->vfield; *vfield; vfield++) {
    // Check if this field should be included
    if (bitmap_is_set(fields, (*vfield)->field_index()) &&
        (*vfield)->is_virtual_gcol() && (*vfield)->type() == MYSQL_TYPE_BLOB) {
      auto field = down_cast<Field_blob *>(*vfield);
      blob_len_ptr_array[num].length = field->data_length();
      // TODO: The following check is only for Innodb.
      assert(blob_len_ptr_array[num].length == 255 ||
             blob_len_ptr_array[num].length == 768 ||
             blob_len_ptr_array[num].length == 3073);

      blob_len_ptr_array[num].ptr = field->get_blob_data();

      // Let server allocate the space for BLOB virtual generated columns
      field->reset();

      num++;
      assert(num <= MAX_FIELDS);
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

static void copy_blob_data(const TABLE *table, const MY_BITMAP *const fields,
                           blob_len_ptr *blob_len_ptr_array) {
  uint num = 0;
  for (Field **vfield = table->vfield; *vfield; vfield++) {
    // Check if this field should be included
    if (bitmap_is_set(fields, (*vfield)->field_index()) &&
        (*vfield)->is_virtual_gcol() && (*vfield)->type() == MYSQL_TYPE_BLOB) {
      assert(blob_len_ptr_array[num].length > 0);
      assert(blob_len_ptr_array[num].ptr != nullptr);

      /*
        Only copy as much of the blob as the storage engine has
        allocated space for. This is sufficient since the only use of the
        blob in the storage engine is for using a prefix of it in a
        secondary index.
      */
      uint length = (*vfield)->data_length();
      const uint alloc_len = blob_len_ptr_array[num].length;
      length = length > alloc_len ? alloc_len : length;

      Field_blob *blob_field = down_cast<Field_blob *>(*vfield);
      memcpy(blob_len_ptr_array[num].ptr, blob_field->get_blob_data(), length);
      blob_field->store_in_allocated_space(
          pointer_cast<char *>(blob_len_ptr_array[num].ptr), length);
      num++;
      assert(num <= MAX_FIELDS);
    }
  }
}

/*
  Evaluate generated column's value. This is an internal helper reserved for
  handler::my_eval_gcolumn_expr().

  @param thd        pointer of THD
  @param table      The pointer of table where evaluated generated
                    columns are in
  @param fields     bitmap of field index of evaluated generated column
  @param[in,out] record record buff of base columns generated column depends.
                        After calling this function, it will be used to return
                        the value of generated column.
  @param in_purge   whether the function is called by purge thread

  @return true in case of error, false otherwise.
*/

static bool my_eval_gcolumn_expr_helper(THD *thd, TABLE *table,
                                        const MY_BITMAP *const fields,
                                        uchar *record, bool in_purge,
                                        const char **mv_data_ptr,
                                        ulong *mv_length) {
  DBUG_TRACE;
  assert(table && table->vfield);
  assert(!thd->is_error());

  uchar *old_buf = table->record[0];
  repoint_field_to_record(table, old_buf, record);

  blob_len_ptr blob_len_ptr_array[MAX_FIELDS];

  /*
    If it's purge thread, we need get the space allocated by storage engine
    for blob.
  */
  if (in_purge)
    extract_blob_space_and_length_from_record_buff(table, fields,
                                                   blob_len_ptr_array);

  bool res = false;
  Field *mv_field = nullptr;
  MY_BITMAP fields_to_evaluate;
  my_bitmap_map bitbuf[bitmap_buffer_size(MAX_FIELDS) / sizeof(my_bitmap_map)];
  bitmap_init(&fields_to_evaluate, bitbuf, table->s->fields);
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
  for (Field **vfield_ptr = table->vfield; *vfield_ptr; vfield_ptr++) {
    Field *field = *vfield_ptr;
    // Validate that the field number is less than the bit map size
    assert(field->field_index() < fields->n_bits);

    if (bitmap_is_set(fields, field->field_index())) {
      bitmap_union(&fields_to_evaluate, &field->gcol_info->base_columns_map);
      if (field->is_array()) {
        mv_field = field;
        // Backup current value and use dedicated temporary buffer
        if ((down_cast<Field_blob *>(field))->backup_blob_field()) return true;
      }
    }
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
  dbug_tmp_use_all_columns(table, old_maps, table->read_set, table->write_set);

  for (Field **vfield_ptr = table->vfield; *vfield_ptr; vfield_ptr++) {
    Field *field = *vfield_ptr;

    // Check if we should evaluate this field
    if (bitmap_is_set(&fields_to_evaluate, field->field_index()) &&
        field->is_virtual_gcol()) {
      assert(field->gcol_info && field->gcol_info->expr_item->fixed);

      const type_conversion_status save_in_field_status =
          field->gcol_info->expr_item->save_in_field(field, false);
      assert(!thd->is_error() || save_in_field_status != TYPE_OK);

      /*
        save_in_field() may return non-zero even if there was no
        error. This happens if a warning is raised, such as an
        out-of-range warning when converting the result to the target
        type of the virtual column. We should stop only if the
        non-zero return value was caused by an actual error.
      */
      if (save_in_field_status != TYPE_OK && thd->is_error()) {
        res = true;
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
  if (in_purge) copy_blob_data(table, fields, blob_len_ptr_array);

  if (mv_field) {
    assert(mv_data_ptr);
    Field_json *fld = down_cast<Field_json *>(mv_field);
    // Save calculated value
    *mv_data_ptr = fld->get_binary();
    *mv_length = fld->data_length();
    // Restore original value
    (fld)->restore_blob_backup();
  }

  repoint_field_to_record(table, record, old_buf);
  return res;
}

// Set se_private_id and se_private_data during upgrade
bool handler::ha_upgrade_table(THD *thd, const char *dbname,
                               const char *table_name, dd::Table *dd_table,
                               TABLE *table_arg) {
  table = table_arg;
  return upgrade_table(thd, dbname, table_name, dd_table);
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

bool handler::my_prepare_gcolumn_template(THD *thd, const char *db_name,
                                          const char *table_name,
                                          my_gcolumn_template_callback_t myc,
                                          void *ib_table) {
  bool rc = true;
  Temp_table_handle tblhdl;
  TABLE *table = tblhdl.open(thd, db_name, table_name);

  if (table) {
    myc(table, ib_table);
    rc = false;
  }
  return rc;
}

/**
  Callback for generated columns processing. Will open the table, in the
  server *only*, and call my_eval_gcolumn_expr_helper() to do the actual
  processing. This function is a variant of the other
  handler::my_eval_gcolumn_expr() but is intended for use when no TABLE
  object already exists - e.g. from purge threads.

  Note! The call to open_table_uncached() must be made with the second-to-last
  argument (open_in_engine) set to false. Failing to do so will cause
  deadlocks and incorrect behavior.

  @param thd         thread handle
  @param db_name     database containing the table to open
  @param table_name  name of table to open
  @param fields      bitmap of field index of evaluated generated column
  @param record      record buffer
  @param[out] mv_data_ptr     For a typed array field in this arg the pointer
                              to its value is returned
  @param[out] mv_length  Length of the value above

  @return true in case of error, false otherwise.
*/

bool handler::my_eval_gcolumn_expr_with_open(THD *thd, const char *db_name,
                                             const char *table_name,
                                             const MY_BITMAP *const fields,
                                             uchar *record,
                                             const char **mv_data_ptr,
                                             ulong *mv_length) {
  bool retval = true;
  Temp_table_handle tblhdl;
  TABLE *table = tblhdl.open(thd, db_name, table_name);

  if (table) {
    retval = my_eval_gcolumn_expr_helper(thd, table, fields, record, true,
                                         mv_data_ptr, mv_length);
  }

  return retval;
}

bool handler::my_eval_gcolumn_expr(THD *thd, TABLE *table,
                                   const MY_BITMAP *const fields, uchar *record,
                                   const char **mv_data_ptr, ulong *mv_length) {
  DBUG_TRACE;

  const bool res = my_eval_gcolumn_expr_helper(thd, table, fields, record,
                                               false, mv_data_ptr, mv_length);
  return res;
}

bool handler::filter_dup_records() {
  assert(inited == INDEX && m_unique);
  position(table->record[0]);
  return m_unique->unique_add(ref);
}

int handler::ha_extra(enum ha_extra_function operation) {
  if (operation == HA_EXTRA_ENABLE_UNIQUE_RECORD_FILTER) {
    // This operation should be called only for active multi-valued index
    assert(inited == INDEX &&
           (table->key_info[active_index].flags & HA_MULTI_VALUED_KEY));
    // This unique filter uses only row id to weed out duplicates. Due to that
    // it will work with any active index.
    if (!m_unique &&
        (!(m_unique = new (*THR_MALLOC) Unique_on_insert(ref_length)) ||
         m_unique->init())) {
      /* purecov: begin inspected */
      destroy(m_unique);
      return HA_ERR_OUT_OF_MEM;
      /* purecov: end */
    }
    m_unique->reset(true);
    return 0;
  } else if (operation == HA_EXTRA_DISABLE_UNIQUE_RECORD_FILTER) {
    if (m_unique) {
      m_unique->cleanup();
      destroy(m_unique);
      m_unique = nullptr;
    }
  }
  return extra(operation);
}

TABLE *Temp_table_handle::open(THD *thd, const char *db_name,
                               const char *table_name) {
  char path[FN_REFLEN + 1];
  bool was_truncated;
  build_table_filename(path, sizeof(path) - 1 - reg_ext_length, db_name,
                       table_name, "", 0, &was_truncated);
  assert(!was_truncated);

  MDL_request table_request;
  MDL_REQUEST_INIT(&table_request, MDL_key::TABLE, db_name, table_name,
                   MDL_SHARED, MDL_TRANSACTION);

  if (thd->mdl_context.acquire_lock(&table_request,
                                    thd->variables.lock_wait_timeout)) {
    return nullptr;
  }

  {
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Table *tab_obj = nullptr;
    if (thd->dd_client()->acquire(db_name, table_name, &tab_obj))
      return nullptr;
    assert(tab_obj);
    table = open_table_uncached(thd, path, db_name, table_name, false, false,
                                *tab_obj);
  }
  return table;
}

Temp_table_handle::~Temp_table_handle() {
  if (table != nullptr) {
    intern_close_table(table);
  }
}

/**
  Auxiliary structure for passing information to notify_*_helper()
  functions.
*/

struct HTON_NOTIFY_PARAMS {
  HTON_NOTIFY_PARAMS(const MDL_key *mdl_key, ha_notification_type mdl_type,
                     ha_ddl_type ddl_type = HA_INVALID_DDL,
                     const char *old_db_name = nullptr,
                     const char *old_table_name = nullptr,
                     const char *new_db_name = nullptr,
                     const char *new_table_name = nullptr)
      : key(mdl_key),
        notification_type(mdl_type),
        ddl_type{ddl_type},
        some_htons_were_notified(false),
        victimized(false),
        m_old_db_name(old_db_name),
        m_old_table_name(old_table_name),
        m_new_db_name(new_db_name),
        m_new_table_name(new_table_name) {}

  const MDL_key *key;
  const ha_notification_type notification_type;
  const ha_ddl_type ddl_type;
  bool some_htons_were_notified;
  bool victimized;
  /* Only used in RENAME TABLE */
  const char *m_old_db_name;
  const char *m_old_table_name;
  const char *m_new_db_name;
  const char *m_new_table_name;
};

static bool notify_exclusive_mdl_helper(THD *thd, plugin_ref plugin,
                                        void *arg) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->notify_exclusive_mdl) {
    HTON_NOTIFY_PARAMS *params = reinterpret_cast<HTON_NOTIFY_PARAMS *>(arg);

    if (hton->notify_exclusive_mdl(thd, params->key, params->notification_type,
                                   &params->victimized)) {
      // Ignore failures from post event notification.
      if (params->notification_type == HA_NOTIFY_PRE_EVENT) return true;
    } else
      params->some_htons_were_notified = true;
  }
  return false;
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

  See @sa handlerton::notify_exclusive_mdl for details about
  calling convention and error reporting.

  @return False - if notification was successful/lock can be acquired,
          True - if it has failed/lock should not be acquired.
*/

bool ha_notify_exclusive_mdl(THD *thd, const MDL_key *mdl_key,
                             ha_notification_type notification_type,
                             bool *victimized) {
  HTON_NOTIFY_PARAMS params(mdl_key, notification_type);
  *victimized = false;
  if (plugin_foreach(thd, notify_exclusive_mdl_helper,
                     MYSQL_STORAGE_ENGINE_PLUGIN, &params)) {
    *victimized = params.victimized;
    /*
      If some SE hasn't given its permission to acquire lock and some SEs
      has given their permissions, we need to notify the latter group about
      failed lock acquisition. We do this by calling post-release notification
      for all interested SEs unconditionally.
    */
    if (notification_type == HA_NOTIFY_PRE_EVENT &&
        params.some_htons_were_notified) {
      HTON_NOTIFY_PARAMS rollback_params(mdl_key, HA_NOTIFY_POST_EVENT);
      (void)plugin_foreach(thd, notify_exclusive_mdl_helper,
                           MYSQL_STORAGE_ENGINE_PLUGIN, &rollback_params);
    }
    return true;
  }
  return false;
}

static bool notify_table_ddl_helper(THD *thd, plugin_ref plugin, void *arg) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES &&
      (hton->notify_alter_table || hton->notify_rename_table ||
       hton->notify_truncate_table)) {
    HTON_NOTIFY_PARAMS *params = reinterpret_cast<HTON_NOTIFY_PARAMS *>(arg);

    bool notify_ret{false};

    /* If the DDL is ALTER or TRUNCATE, it shouldn't have the names set. */
    assert(((params->ddl_type == HA_ALTER_DDL ||
             params->ddl_type == HA_TRUNCATE_DDL) &&
            (params->m_old_db_name == nullptr &&
             params->m_old_table_name == nullptr &&
             params->m_new_db_name == nullptr &&
             params->m_new_table_name == nullptr)) ||
           (params->ddl_type == HA_RENAME_DDL));

    switch (params->ddl_type) {
      case HA_ALTER_DDL:
        if (hton->notify_alter_table) {
          notify_ret = hton->notify_alter_table(thd, params->key,
                                                params->notification_type);
        }
        break;
      case HA_TRUNCATE_DDL:
        if (hton->notify_truncate_table) {
          notify_ret = hton->notify_truncate_table(thd, params->key,
                                                   params->notification_type);
        }
        break;
      case HA_RENAME_DDL:
        if (hton->notify_rename_table) {
          notify_ret = hton->notify_rename_table(
              thd, params->key, params->notification_type,
              params->m_old_db_name, params->m_old_table_name,
              params->m_new_db_name, params->m_new_table_name);
        }
        break;
      default:
        assert(0);
        return true;
    }

    if (notify_ret) {
      // Ignore failures from post event notification.
      if (params->notification_type == HA_NOTIFY_PRE_EVENT) return true;
    } else
      params->some_htons_were_notified = true;
  }
  return false;
}

/**
  Notify/get permission from all interested storage engines before or after
  executed DDL (ALTER TABLE, RENAME TABLE, TRUNCATE TABLE) on the table
  identified by key.

  @param thd                Thread context.
  @param mdl_key            MDL key identifying table.
  @param notification_type  Indicates whether this is pre-DDL or post-DDL
  notification.
  @param old_db_name        Old db name, used in RENAME DDL
  @param old_table_name     Old table name, used in RENAME DDL
  @param new_db_name        New db name, used in RENAME DDL
  @param new_table_name     New table name, used in RENAME DDL

  See @sa handlerton::notify_alter_table for rationale, details about calling
  convention and error reporting.

  @return False - if notification was successful/DDL can proceed.
          True -  if it has failed/DDL should fail.
*/
bool ha_notify_table_ddl(THD *thd, const MDL_key *mdl_key,
                         ha_notification_type notification_type,
                         ha_ddl_type ddl_type, const char *old_db_name,
                         const char *old_table_name, const char *new_db_name,
                         const char *new_table_name) {
  HTON_NOTIFY_PARAMS params(mdl_key, notification_type, ddl_type, old_db_name,
                            old_table_name, new_db_name, new_table_name);

  if (plugin_foreach(thd, notify_table_ddl_helper, MYSQL_STORAGE_ENGINE_PLUGIN,
                     &params)) {
    if (notification_type == HA_NOTIFY_PRE_EVENT &&
        params.some_htons_were_notified) {
      HTON_NOTIFY_PARAMS rollback_params(mdl_key, HA_NOTIFY_POST_EVENT,
                                         ddl_type, old_db_name, old_table_name,
                                         new_db_name, new_table_name);
      (void)plugin_foreach(thd, notify_table_ddl_helper,
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
bool set_tx_isolation(THD *thd, enum_tx_isolation tx_isolation, bool one_shot) {
  TX_TRACKER_GET(tst);

  if (thd->variables.session_track_transaction_info <= TX_TRACK_NONE)
    tst = nullptr;

  thd->tx_isolation = tx_isolation;

  if (one_shot) {
    assert(!thd->in_active_multi_stmt_transaction());
    assert(!thd->in_sub_stmt);
    enum enum_tx_isol_level l;
    switch (thd->tx_isolation) {
      case ISO_READ_UNCOMMITTED:
        l = TX_ISOL_UNCOMMITTED;
        break;
      case ISO_READ_COMMITTED:
        l = TX_ISOL_COMMITTED;
        break;
      case ISO_REPEATABLE_READ:
        l = TX_ISOL_REPEATABLE;
        break;
      case ISO_SERIALIZABLE:
        l = TX_ISOL_SERIALIZABLE;
        break;
      default:
        assert(0);
        return true;
    }
    if (tst) tst->set_isol_level(thd, l);
  } else if (tst) {
    tst->set_isol_level(thd, TX_ISOL_INHERIT);
  }
  return false;
}

static bool post_recover_handlerton(THD *, plugin_ref plugin, void *) {
  handlerton *hton = plugin_data<handlerton *>(plugin);

  if (hton->state == SHOW_OPTION_YES && hton->post_recover)
    hton->post_recover();

  return false;
}

void ha_post_recover(void) {
  (void)plugin_foreach(nullptr, post_recover_handlerton,
                       MYSQL_STORAGE_ENGINE_PLUGIN, nullptr);
}

void handler::ha_set_primary_handler(handler *primary_handler) {
  assert((ht->flags & HTON_IS_SECONDARY_ENGINE) != 0);
  assert(primary_handler->table->s->has_secondary_engine());
  m_primary_handler = primary_handler;
}

const handlerton *SecondaryEngineHandlerton(const THD *thd) {
  if (thd->lex->m_sql_cmd == nullptr) {
    return nullptr;
  }
  return thd->lex->m_sql_cmd->secondary_engine();
}

/**
  Checks if the database name is reserved word used by SE by invoking
  the handlerton method.

  @param  plugin        SE plugin.
  @param  name          Database name.

  @retval true          If the name is reserved word.
  @retval false         If the name is not reserved word.
*/
static bool is_reserved_db_name_handlerton(THD *, plugin_ref plugin,
                                           void *name) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->state == SHOW_OPTION_YES && hton->is_reserved_db_name)
    return (hton->is_reserved_db_name(hton, (const char *)name));
  return false;
}

/**
   Check if the database name is reserved word used by SE.

   @param  name    Database name.

   @retval true    If the name is a reserved word.
   @retval false   If the name is not a reserved word.
*/
bool ha_check_reserved_db_name(const char *name) {
  return (plugin_foreach(nullptr, is_reserved_db_name_handlerton,
                         MYSQL_STORAGE_ENGINE_PLUGIN,
                         const_cast<char *>(name)));
}

/**
   Check whether an error is index access error or not
   after an index read. Error other than HA_ERR_END_OF_FILE
   or HA_ERR_KEY_NOT_FOUND will stop next index read.

   @param  error    Handler error code.

   @retval true     if error is different from HA_ERR_END_OF_FILE or
                    HA_ERR_KEY_NOT_FOUND.
   @retval false    if error is HA_ERR_END_OF_FILE or HA_ERR_KEY_NOT_FOUND.
*/
bool is_index_access_error(int error) {
  return (error != HA_ERR_END_OF_FILE && error != HA_ERR_KEY_NOT_FOUND);
}

Xa_state_list::Xa_state_list(Xa_state_list::list &populated_by_tc)
    : m_underlying{populated_by_tc} {}

enum_ha_recover_xa_state Xa_state_list::find(XID const &to_find) {
  auto found = this->m_underlying.find(to_find);
  if (found != this->m_underlying.end()) return found->second;
  return enum_ha_recover_xa_state::NOT_FOUND;
}

enum_ha_recover_xa_state Xa_state_list::add(XID const &xid,
                                            enum_ha_recover_xa_state state) {
  auto previous_state = enum_ha_recover_xa_state::NOT_FOUND;

  auto it = this->m_underlying.find(xid);
  if (it != this->m_underlying.end()) previous_state = it->second;

  switch (state) {
    case enum_ha_recover_xa_state::PREPARED_IN_SE: {
      if (previous_state == enum_ha_recover_xa_state::NOT_FOUND ||
          previous_state == enum_ha_recover_xa_state::COMMITTED ||
          previous_state == enum_ha_recover_xa_state::ROLLEDBACK)
        this->m_underlying[xid] = state;
      break;
    }
    case enum_ha_recover_xa_state::PREPARED_IN_TC: {
      if (previous_state == enum_ha_recover_xa_state::NOT_FOUND ||
          previous_state == enum_ha_recover_xa_state::PREPARED_IN_SE)
        this->m_underlying[xid] = state;
      break;
    }
    case enum_ha_recover_xa_state::NOT_FOUND:
    case enum_ha_recover_xa_state::COMMITTED:
    case enum_ha_recover_xa_state::COMMITTED_WITH_ONEPHASE:
    case enum_ha_recover_xa_state::ROLLEDBACK: {
      assert(false);
      break;
    }
  }
  return previous_state;
}

Xa_state_list::instantiation_tuple Xa_state_list::new_instance() {
  auto mem_root =
      std::make_unique<MEM_ROOT>(PSI_INSTRUMENT_ME, tc_log_page_size / 3);
  auto map_alloc = std::make_unique<Xa_state_list::allocator>(mem_root.get());
  auto xid_map = std::make_unique<Xa_state_list::list>(*map_alloc.get());
  auto xa_list = std::make_unique<Xa_state_list>(*xid_map.get());
  return std::make_tuple<
      std::unique_ptr<MEM_ROOT>, std::unique_ptr<Xa_state_list::allocator>,
      std::unique_ptr<Xa_state_list::list>, std::unique_ptr<Xa_state_list>>(
      std::move(mem_root), std::move(map_alloc), std::move(xid_map),
      std::move(xa_list));
}
