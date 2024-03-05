/*  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/srv_session.h"

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <atomic>
#include <list>
#include <map>
#include <new>
#include <utility>

#include "lex_string.h"
#include "m_string.h"
#include "mutex_lock.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_thread.h"
#include "my_thread_local.h"  // my_get_thread_local & my_set_thread_local
#include "mysql.h"
#include "mysql/components/my_service.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "mysql/components/services/bits/mysql_rwlock_bits.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "mysql/components/services/bits/psi_rwlock_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysql/plugin_audit.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/psi/mysql_statement.h"
#include "mysql/strings/m_ctype.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "nulls.h"
#include "pfs_thread_provider.h"
#include "rwlock_scoped_lock.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/conn_handler/connection_handler_manager.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"          // DEBUG_SYNC
#include "sql/derror.h"              // ER_DEFAULT
#include "sql/log.h"                 // Query log
#include "sql/mysqld.h"              // current_thd
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/sql_audit.h"           // MYSQL_AUDIT_NOTIFY_CONNECTION_CONNECT
#include "sql/sql_base.h"            // close_mysql_tables
#include "sql/sql_class.h"
#include "sql/sql_connect.h"  // thd_init_client_charset
#include "sql/sql_list.h"
#include "sql/sql_parse.h"  // dispatch_command()
#include "sql/sql_plugin_ref.h"
#include "sql/sql_thd_internal_api.h"  // thd_set_thread_stack
#include "sql/system_variables.h"
#include "thr_mutex.h"

struct decimal_t;

/**
  @file
  class Srv_session implementation. See the method comments for more. Please,
  check also srv_session.h for more information.
*/

extern void thd_clear_errors(THD *thd);

static thread_local const char *THR_stack_start_address = nullptr;
static thread_local const st_plugin_int *THR_srv_session_thread = nullptr;
static bool srv_session_THRs_initialized = false;

class Thread_to_plugin_map {
 private:
  bool initted;
  bool psi_initted;
  mysql_mutex_t LOCK_collection;

#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key key_LOCK_collection;
#endif
  std::map<my_thread_t, const void *> collection;

 public:
  /**
    Initializes the map

    @return
      false  success
      true   failure
  */
  bool init() {
    const char *category = "session";
    PSI_mutex_info all_mutexes[] = {{&key_LOCK_collection,
                                     "LOCK_srv_session_threads",
                                     PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}};

    initted = true;
#ifdef HAVE_PSI_INTERFACE
    psi_initted = true;

    mysql_mutex_register(category, all_mutexes,
                         static_cast<int>(array_elements(all_mutexes)));
#endif
    mysql_mutex_init(key_LOCK_collection, &LOCK_collection, MY_MUTEX_INIT_FAST);

    return false;
  }

  /**
    Deinitializes the map

    @return
      false  success
      true   failure
  */
  bool deinit() {
    initted = false;
    mysql_mutex_destroy(&LOCK_collection);

    return false;
  }

  /**
    Adds a pthread to the list

    @return
      false  success
      true   failure
  */
  bool add(my_thread_t thread, const void *plugin) {
    MUTEX_LOCK(lock, &LOCK_collection);
    try {
      std::map<my_thread_t, const void *>::iterator it =
          collection.find(thread);
      if (it == collection.end()) collection[thread] = plugin;
    } catch (const std::bad_alloc &) {
      return true;
    }
    return false;
  }

  /**
    Removes a pthread from the list

    @param thread  Thread to remove from the list

    @return
      false  success
      true   failure
  */
  unsigned int remove(my_thread_t thread) {
    MUTEX_LOCK(lock, &LOCK_collection);
    std::map<my_thread_t, const void *>::iterator it = collection.find(thread);
    if (it != collection.end()) {
      collection.erase(it);
      return false;
    }
    return true;
  }

  /**
    Empties the map

    @return
      false  success
      true   failure
  */
  bool clear() {
    MUTEX_LOCK(lock, &LOCK_collection);
    collection.clear();
    return false;
  }

  /**
    Returns the number of all threads
  */
  unsigned int size() {
    MUTEX_LOCK(lock, &LOCK_collection);
    return collection.size();
  }

  /**
    Returns the number threads for a plugin

    @param plugin The plugin for which we need info.
  */
  unsigned int count(const void *plugin) {
    if (!plugin) return size();

    unsigned int ret = 0;
    MUTEX_LOCK(lock, &LOCK_collection);
    std::map<my_thread_t, const void *>::iterator it = collection.begin();
    for (; it != collection.end(); ++it) {
      if (it->second == plugin) ++ret;
    }
    return ret;
  }

  /**
    Kills all threads associated with a plugin

    @param plugin The plugin for which we need info.
  */
  unsigned int kill(const void *plugin) {
    std::list<my_thread_t> to_remove;

    MUTEX_LOCK(lock, &LOCK_collection);

    for (std::map<my_thread_t, const void *>::iterator it = collection.begin();
         it != collection.end(); ++it) {
      if (!plugin || (it->second == plugin)) {
        to_remove.push_back(it->first);

#ifndef _WIN32
        my_thread_handle thread;
        /*
           On Windows we need HANDLE to cancel a thread.
           Win32 API's GetCurrentThread() returns something which seems the
           same in every thread, thus unusable as a key.
           GetCurrentThreadId returns an ID (DWORD), but it can't be used with
           my_thread_cancel() which calls TerminateThread() on Windows.
           TerminateThread() needs a Handle.
           Therefore this killing functionality is now only for Posix Threads
           until there is a solution for Windows.
        */
        thread.thread = it->first;
        LogErr(ERROR_LEVEL, ER_KILLING_THREAD, (unsigned long)it->first);
        if (!my_thread_cancel(&thread)) {
          void *dummy_retval;
          my_thread_join(&thread, &dummy_retval);
        }
#endif
      }
    }
    if (to_remove.size()) {
      for (std::list<my_thread_t>::iterator it = to_remove.begin();
           it != to_remove.end(); ++it)
        collection.erase(*it);
    }
    return to_remove.size();
  }
};

/**
 std::map of THD* as key and Srv_session* as value guarded by a read-write lock.
 RW lock is used instead of a mutex, as find() is a hot spot due to the sanity
 checks it is used for - when a pointer to a closed session is passed.
*/
class Mutexed_map_thd_srv_session {
 public:
  class Do_Impl {
   public:
    virtual ~Do_Impl() = default;
    /**
      Work on the session

      @return
        false  Leave the session in the map
        true   Remove the session from the map
    */
    virtual bool operator()(Srv_session *) = 0;
  };

 private:
  /*
    The first type in the tuple should be the key type of
    Thread_to_plugin_map
  */
  typedef std::pair<const void *, Srv_session *> map_value_t;

  std::map<const THD *, map_value_t> collection;

  bool initted;
  bool psi_initted;

  mysql_rwlock_t LOCK_collection;

#ifdef HAVE_PSI_INTERFACE
  PSI_rwlock_key key_LOCK_collection;
#endif

 public:
  /**
    Initializes the map

    @return
      false  success
      true   failure
  */
  bool init() {
    const char *category = "session";
    PSI_rwlock_info all_rwlocks[] = {{&key_LOCK_collection,
                                      "LOCK_srv_session_collection",
                                      PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME}};

    initted = true;
#ifdef HAVE_PSI_INTERFACE
    psi_initted = true;

    mysql_rwlock_register(category, all_rwlocks,
                          static_cast<int>(array_elements(all_rwlocks)));
#endif
    mysql_rwlock_init(key_LOCK_collection, &LOCK_collection);

    return false;
  }

  /**
    Deinitializes the map

    @return
      false  success
      true   failure
  */
  bool deinit() {
    initted = false;
    mysql_rwlock_destroy(&LOCK_collection);

    return false;
  }

  /**
    Searches for an element with in the map

    @param key Key of the element

    @return value of the element
    @retval NULL  if not found
  */
  Srv_session *find(const THD *key) {
    rwlock_scoped_lock lock(&LOCK_collection, false, __FILE__, __LINE__);

    std::map<const THD *, map_value_t>::iterator it = collection.find(key);
    return (it != collection.end()) ? it->second.second : nullptr;
  }

  /**
    Add an element to the map

    @param key     key
    @param plugin  secondary key
    @param session object to be added to the collection

    @retval false  success
    @retval true   failure
  */
  bool add(const THD *key, const void *plugin, Srv_session *session) {
    rwlock_scoped_lock lock(&LOCK_collection, true, __FILE__, __LINE__);
    try {
      collection[key] = std::make_pair(plugin, session);
    } catch (const std::bad_alloc &) {
      return true;
    }
    return false;
  }

  /**
    Removes an element from the map.

    @param key  key

    @return
      false  success
      true   failure
  */
  bool remove(const THD *key) {
    rwlock_scoped_lock lock(&LOCK_collection, true, __FILE__, __LINE__);
    /*
      If we use erase with the key directly an exception could be thrown. The
      find method never throws. erase() with iterator as parameter also never
      throws.
    */
    std::map<const THD *, map_value_t>::iterator it = collection.find(key);
    if (it != collection.end()) collection.erase(it);
    return false;
  }

  /**
    Removes all elements which have been added with plugin as plugin name.

    @param plugin key
    @param removed OUT Number of removed elements
  */
  void remove_all_of_plugin(const void *plugin, unsigned int &removed) {
    std::list<Srv_session *> to_close;
    removed = 0;

    {
      rwlock_scoped_lock lock(&LOCK_collection, true, __FILE__, __LINE__);

      for (std::map<const THD *, map_value_t>::iterator it = collection.begin();
           it != collection.end(); ++it) {
        if (it->second.first == plugin) to_close.push_back(it->second.second);
      }
    }

    /* Outside of the lock as Srv_session::close() will try to
      remove itself from the list*/
    if ((removed = to_close.size())) {
      for (std::list<Srv_session *>::iterator it = to_close.begin();
           it != to_close.end(); ++it) {
        Srv_session *session = *it;
        session->detach();
        session->close();
        delete session;
      }
    }
  }

  /**
    Empties the map

    @return
      false  success
      true   failure
  */
  bool clear() {
    rwlock_scoped_lock lock(&LOCK_collection, true, __FILE__, __LINE__);
    collection.clear();
    return false;
  }

  /**
    Returns the number of elements in the maps
  */
  unsigned int size() {
    rwlock_scoped_lock lock(&LOCK_collection, false, __FILE__, __LINE__);
    return collection.size();
  }
};

static Mutexed_map_thd_srv_session server_session_list;
static Thread_to_plugin_map server_session_threads;

Srv_session::Session_backup_and_attach::Session_backup_and_attach(
    Srv_session *sess, bool is_in_close_session)
    : session(sess),
      old_session(nullptr),
      in_close_session(is_in_close_session) {
  THD *c_thd = current_thd;
  backup_thd = c_thd;
  bool is_plugin = false;

  if (session->is_associate()) {
    attach_error = session->associate();
    return;
  }

  /*
    Check whether the current thread and the one we're going to switch to,
    belong to a plugin and if the plugin is the same (so we can reuse the thd).
  */
  if (THR_srv_session_thread && c_thd &&
      c_thd->get_plugin() == session->m_thd->get_plugin()) {
    is_plugin = true;
    backup_thd = nullptr;
  }

  if (is_plugin && c_thd != session->m_thd &&
      (old_session = server_session_list.find(c_thd))) {
    old_session->detach();
  } else if (is_plugin) {
    assert(session->is_attached());
  }

  attach_error = session->attach();
}

Srv_session::Session_backup_and_attach::~Session_backup_and_attach() {
  if (session->is_associated()) {
    session->disassociate();
    if (backup_thd && backup_thd != session->m_thd) backup_thd->store_globals();
  } else if (backup_thd) {
    session->detach();
    backup_thd->store_globals();
#ifdef HAVE_PSI_THREAD_INTERFACE
    enum_vio_type vio_type = backup_thd->get_vio_type();
    if (vio_type != NO_VIO_TYPE) PSI_THREAD_CALL(set_connection_type)(vio_type);
#endif /* HAVE_PSI_THREAD_INTERFACE */
  } else if (in_close_session) {
    /*
      We should restore the old session only in case of close.
      In case of execute we should stay attached.
    */
    session->detach();
    if (old_session) {
      old_session->attach();
    }
  }
}

static int err_start_result_metadata(void *, uint, uint, const CHARSET_INFO *) {
  return 1;
}

static int err_field_metadata(void *, struct st_send_field *,
                              const CHARSET_INFO *) {
  return 1;
}

static int err_end_result_metadata(void *, uint, uint) { return 1; }

static int err_start_row(void *) { return 1; }

static int err_end_row(void *) { return 1; }

static void err_abort_row(void *) {}

static ulong err_get_client_capabilities(void *) { return 0; }

static int err_get_null(void *) { return 1; }

static int err_get_integer(void *, longlong) { return 1; }

static int err_get_longlong(void *, longlong, uint) { return 1; }

static int err_get_decimal(void *, const decimal_t *) { return 1; }

static int err_get_double(void *, double, uint32) { return 1; }

static int err_get_date(void *, const MYSQL_TIME *) { return 1; }

static int err_get_time(void *, const MYSQL_TIME *, uint) { return 1; }

static int err_get_datetime(void *, const MYSQL_TIME *, uint) { return 1; }

static int err_get_string(void *, const char *, size_t, const CHARSET_INFO *) {
  return 1;
}

static void err_handle_ok(void *ctx, uint server_status, uint warn_count,
                          ulonglong affected_rows, ulonglong last_insert_id,
                          const char *const) {
  Srv_session::st_err_protocol_ctx *pctx =
      static_cast<Srv_session::st_err_protocol_ctx *>(ctx);
  if (pctx && pctx->handler) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "OK status=%u warnings=%u affected=%llu last_id=%llu",
             server_status, warn_count, affected_rows, last_insert_id);
    pctx->handler(pctx->handler_context, 0, buf);
  }
}

static void err_handle_error(void *ctx, uint err_errno, const char *err_msg,
                             const char *) {
  Srv_session::st_err_protocol_ctx *pctx =
      static_cast<Srv_session::st_err_protocol_ctx *>(ctx);
  if (pctx && pctx->handler)
    pctx->handler(pctx->handler_context, err_errno, err_msg);
}

static void err_shutdown(void *, int) {}

static bool err_alive(void *) { return true; }

const struct st_command_service_cbs error_protocol_callbacks = {
    err_start_result_metadata,
    err_field_metadata,
    err_end_result_metadata,
    err_start_row,
    err_end_row,
    err_abort_row,
    err_get_client_capabilities,
    err_get_null,
    err_get_integer,
    err_get_longlong,
    err_get_decimal,
    err_get_double,
    err_get_date,
    err_get_time,
    err_get_datetime,
    err_get_string,
    err_handle_ok,
    err_handle_error,
    err_shutdown,
    err_alive};

/**
  Modifies the PSI structures to (de)install a THD

  @param thd THD
*/
#ifdef HAVE_PSI_THREAD_INTERFACE
static void set_psi(THD *thd) {
  struct PSI_thread *psi = PSI_THREAD_CALL(get_thread)();
  PSI_THREAD_CALL(set_thread_id)(psi, thd ? thd->thread_id() : 0);
  PSI_THREAD_CALL(set_thread_THD)(psi, thd);
}
#endif

/**
  Initializes physical thread to use with session service.

  @param plugin Pointer to the plugin structure, passed to the plugin over
                the plugin init function.

  @return
    false  success
    true   failure
*/
bool Srv_session::init_thread(const void *plugin) {
  char stack_start;

  /* If not null, means its already initialized, so return success*/
  if (THR_srv_session_thread != nullptr) return false;

  if (my_thread_init()) {
    connection_errors_internal++;
    return true;
  }

  THR_srv_session_thread =
      reinterpret_cast<st_plugin_int *>(const_cast<void *>(plugin));
  THR_stack_start_address = &stack_start;

  server_session_threads.add(my_thread_self(), plugin);

  return false;
}

/**
  Looks if there is currently attached session and detaches it.

  @param plugin  The plugin to be checked
*/
static void close_currently_attached_session_if_any(
    const st_plugin_int *plugin) {
  THD *c_thd = current_thd;
  if (!c_thd) return;

  Srv_session *current_session = server_session_list.find(c_thd);

  if (current_session) {
    LogErr(ERROR_LEVEL, ER_DETACHING_SESSION_LEFT_BY_PLUGIN, plugin->name.str);

    if (current_session->detach())
      LogErr(ERROR_LEVEL, ER_CANT_DETACH_SESSION_LEFT_BY_PLUGIN);
  }
}

/**
  Looks if the plugin has any non-closed sessions and closes them forcefully

  @param plugin  The plugin to be checked
*/
static void close_all_sessions_of_plugin_if_any(const st_plugin_int *plugin) {
  unsigned int removed_count;

  server_session_list.remove_all_of_plugin(plugin, removed_count);

  if (removed_count)
    LogErr(ERROR_LEVEL, ER_DETACHED_SESSIONS_LEFT_BY_PLUGIN, removed_count,
           (removed_count > 1) ? "s" : "",
           plugin ? plugin->name.str : "SERVER_INTERNAL");
}

/**
  Deinitializes physical thread to use with session service
*/
void Srv_session::deinit_thread() {
  const st_plugin_int *plugin = THR_srv_session_thread;
  if (plugin)
    close_currently_attached_session_if_any(plugin);
  else
    return;

  if (server_session_threads.remove(my_thread_self()))
    LogErr(ERROR_LEVEL, ER_FAILED_TO_DECREMENT_NUMBER_OF_THREADS);

  if (!server_session_threads.count(plugin))
    close_all_sessions_of_plugin_if_any(plugin);

  THR_srv_session_thread = nullptr;

  assert(THR_stack_start_address);
  THR_stack_start_address = nullptr;
  my_thread_end();
}

/**
  Checks if a plugin has left threads and sessions

  @param plugin  The plugin to be checked
*/
void Srv_session::check_for_stale_threads(const st_plugin_int *plugin) {
  if (!plugin) return;

  unsigned int thread_count = server_session_threads.count(plugin);
  if (thread_count) {
    close_all_sessions_of_plugin_if_any(plugin);

    LogErr(ERROR_LEVEL, ER_PLUGIN_DID_NOT_DEINITIALIZE_THREADS,
           plugin->name.str, thread_count);

    unsigned int killed_count = server_session_threads.kill(plugin);
    LogErr(ERROR_LEVEL, ER_KILLED_THREADS_OF_PLUGIN, killed_count,
           plugin->name.str);
  }
}

/**
  Inits the module

  @return
    false  success
    true   failure
*/
bool Srv_session::module_init() {
  if (srv_session_THRs_initialized) return false;
  srv_session_THRs_initialized = true;
  THR_stack_start_address = nullptr;
  THR_srv_session_thread = nullptr;

  server_session_list.init();
  server_session_threads.init();

  return false;
}

/**
  Deinits the module.

  Never fails

  @return
    false  success
*/
bool Srv_session::module_deinit() {
  if (srv_session_THRs_initialized) {
    THR_stack_start_address = nullptr;
    THR_srv_session_thread = nullptr;

    server_session_list.clear();
    server_session_list.deinit();

    server_session_threads.clear();
    server_session_threads.deinit();

    srv_session_THRs_initialized = false;
  }
  return false;
}

/**
  Checks if the session is valid.

  Checked is if session is NULL, or the state of the session is
  SRV_SESSION_OPENED, SRV_SESSION_ATTACHED or SRV_SESSION_DETACHED.

  @return
    true  valid
    false not valid
*/
bool Srv_session::is_valid(const Srv_session *session) {
  assert(session != nullptr);
  const bool is_valid_session = ((session->m_state > SRV_SESSION_CREATED) &&
                                 (session->m_state < SRV_SESSION_CLOSED));

  if (session->is_associate() || session->is_associated()) return true;

  /*
    Make sure valid session exists and invalid sessions doesn't exists in the
    list of opened sessions.
  */
  assert((is_valid_session && server_session_list.find(session->m_thd)) ||
         (!is_valid_session && !server_session_list.find(session->m_thd)));

  return is_valid_session;
}

/**
  Constructs a server session
*/
Srv_session::Srv_session(srv_session_error_cb err_cb, void *err_cb_ctx)
    : Srv_session(err_cb, err_cb_ctx, SRV_SESSION_CREATED, true, nullptr) {
  m_thd->mark_as_srv_session();
  m_thd->m_audited = false;
}

Srv_session::Srv_session(srv_session_error_cb err_cb, void *err_cb_ctx,
                         THD *thd)
    : Srv_session(err_cb, err_cb_ctx, SRV_SESSION_ASSOCIATE, false, thd) {
  assert(thd);  // A valid thd must be passed.
  m_thd->push_protocol(&m_protocol_error);
  m_thd->push_diagnostics_area(&m_da);
}

/** Delegated constructor to intialize the members. */
Srv_session::Srv_session(srv_session_error_cb err_cb, void *err_cb_ctx,
                         srv_session_state state, bool free_resources, THD *thd)
    : m_da(false),
      m_err_protocol_ctx(err_cb, err_cb_ctx),
      m_protocol_error(&error_protocol_callbacks, CS_TEXT_REPRESENTATION,
                       (void *)&m_err_protocol_ctx),
      m_state(state),
      m_vio_type(NO_VIO_TYPE),
      m_thd(thd),
      m_free_resources(free_resources) {
  try {
    if (m_thd == nullptr) m_thd = new THD();
  } catch (...) {
    DBUG_PRINT("error", ("Can't allocate the THD object"));
    throw;
  }
}

Srv_session::~Srv_session() {
  if (m_free_resources) {
    delete m_thd;
  } else {
    mysql_mutex_lock(&m_thd->LOCK_thd_protocol);
    m_thd->pop_protocol();
    mysql_mutex_unlock(&m_thd->LOCK_thd_protocol);
    m_thd->pop_diagnostics_area();
  }
}

/**
Opens a server session

@return
false  on success
true   on failure
*/
bool Srv_session::open() {
  char stack_start;
  DBUG_TRACE;

  DBUG_PRINT("info", ("Session=%p  THD=%p  DA=%p", this, m_thd, &m_da));
  assert(m_state == SRV_SESSION_CREATED || m_state == SRV_SESSION_CLOSED);

  m_thd->push_protocol(&m_protocol_error);
  m_thd->push_diagnostics_area(&m_da);
  /*
    m_thd.stack_start will be set once we start attempt to attach.
    store_globals() will check for it, so we will set it beforehand.

    No store_globals() here as the session is always created in a detached
    state. Attachment with store_globals() will happen on demand.
  */
  if (thd_init_client_charset(m_thd, my_charset_utf8mb3_general_ci.number)) {
    connection_errors_internal++;
    if (m_err_protocol_ctx.handler)
      m_err_protocol_ctx.handler(m_err_protocol_ctx.handler_context,
                                 ER_OUT_OF_RESOURCES,
                                 ER_DEFAULT(ER_OUT_OF_RESOURCES));
    Connection_handler_manager::dec_connection_count();
    return true;
  }

  m_thd->update_charset();

  m_thd->set_new_thread_id();

  DBUG_PRINT("info", ("thread_id=%d", m_thd->thread_id()));

  m_thd->set_command(COM_SLEEP);
  m_thd->init_query_mem_roots();

  /*
    Set current_thd so that it can be used during authentication,
    before attach() is called. Note that this kind of breaks the
    separation between open() and attach() so it is likely that
    a conceptually better solution is required long-term.
  */
  thd_set_thread_stack(m_thd, &stack_start);
  m_thd->store_globals();

  Global_THD_manager::get_instance()->add_thd(m_thd);

  const st_plugin_int *plugin = THR_srv_session_thread;
  m_thd->set_plugin(plugin);

  server_session_list.add(m_thd, plugin, this);

  m_state = SRV_SESSION_OPENED;

  return false;
}

/**
  Attaches the session to the current physical thread

  @returns
    false   success
    true    failure
*/
bool Srv_session::attach() {
  const bool first_attach = (m_state == SRV_SESSION_OPENED);
  DBUG_TRACE;
  DBUG_PRINT("info", ("current_thd=%p", current_thd));
  assert(m_state > SRV_SESSION_CREATED && m_state < SRV_SESSION_CLOSED);

  if (is_attached()) {
    if (!my_thread_equal(m_thd->real_id, my_thread_self())) {
      DBUG_PRINT("error", ("Attached to different thread. Detach in it"));
      return true;
    }
    /* As it is attached, no need to do anything */
    return false;
  }

  // Since we now set current_thd during open(), we need to do complete
  // attach the first time in any case.
  if (!first_attach && m_thd == current_thd) return false;

  THD *old_thd = current_thd;
  DBUG_PRINT("info", ("current_thd=%p", current_thd));

  if (old_thd) old_thd->restore_globals();

  DBUG_PRINT("info", ("current_thd=%p", current_thd));

  const char *new_stack = THR_srv_session_thread
                              ? THR_stack_start_address
                              : (old_thd ? old_thd->thread_stack : nullptr);
  /*
    Attach optimistically, as this will set thread_stack,
    which needed by store_globals()
  */
  set_attached(new_stack);

  // This will install our new THD object as current_thd
  m_thd->store_globals();

  Srv_session *old_session = server_session_list.find(old_thd);

  /* Really detach only if we are sure everything went fine */
  if (old_session) old_session->set_detached();

  thd_clear_errors(m_thd);

#ifdef HAVE_PSI_THREAD_INTERFACE
  set_psi(m_thd);

  PSI_THREAD_CALL(set_connection_type)
  (m_vio_type != NO_VIO_TYPE ? m_vio_type : m_thd->get_vio_type());
#endif /* HAVE_PSI_THREAD_INTERFACE */

  if (first_attach) {
    /*
      At first attach the security context should have been already set and
      and this will report correct information.
    */
#ifdef HAVE_PSI_THREAD_INTERFACE
    PSI_THREAD_CALL(notify_session_connect)(m_thd->get_psi());
#endif /* HAVE_PSI_THREAD_INTERFACE */

    query_logger.general_log_print(m_thd, COM_CONNECT, NullS);
  }

  return false;
}

/**
  Detaches the session from the current physical thread.

  @returns
    false success
    true  failure
*/
bool Srv_session::detach() {
  DBUG_TRACE;

  if (!is_attached()) return false;

  if (!my_thread_equal(m_thd->real_id, my_thread_self())) {
    DBUG_PRINT("error", ("Attached to a different thread. Detach in it"));
    return true;
  }

  DBUG_PRINT("info",
             ("Session=%p THD=%p current_thd=%p", this, m_thd, current_thd));

  assert(m_thd == current_thd);

  m_thd->restore_globals();

#ifdef HAVE_PSI_THREAD_INTERFACE
  set_psi(nullptr);
#endif
  /*
    We can't call PSI_THREAD_CALL(set_connection_type)(NO_VIO_TYPE) here because
    it will assert. Thus, it will be possible to have a physical thread, which
    has no session attached to it but have cached vio type.
    This only will happen in a spawned thread initialized by this service.
    If a server initialized thread is used, just after detach the previous
    current_thd will be re-attached again (not created by our service) and
    the vio type will be set correctly.
    Please see: Session_backup_and_attach::~Session_backup_and_attach()
  */

  /*
    Call after restore_globals() as it will check the stack_addr, which is
    nulled by set_detached()
  */
  set_detached();
  return false;
}

/**
  Closes the session

  @returns
    false Session successfully closed
    true  No such session exists / Session is attached to a different thread
*/
bool Srv_session::close() {
  DBUG_TRACE;

  DBUG_PRINT("info",
             ("Session=%p THD=%p current_thd=%p", this, m_thd, current_thd));

  assert(m_state < SRV_SESSION_CLOSED);

  /*
    RAII
    We store the state (the currently attached session, if different than
    our and then attach our.
    The destructor will attach the session we detached.
  */

  Srv_session::Session_backup_and_attach backup(this, true);

  if (backup.attach_error) return true;

  m_state = SRV_SESSION_CLOSED;

  server_session_list.remove(m_thd);

  /*
    Log to general log must happen before release_resources() as
    current_thd will be different then.
  */
  query_logger.general_log_print(m_thd, COM_QUIT, NullS);

#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(notify_session_disconnect)(m_thd->get_psi());
#endif /* HAVE_PSI_THREAD_INTERFACE */

  m_thd->security_context()->logout();
  m_thd->m_view_ctx_list.clear();
  close_mysql_tables(m_thd);

  m_thd->set_plugin(nullptr);
  m_thd->pop_diagnostics_area();

  m_thd->get_stmt_da()->reset_diagnostics_area();

  // DEBUG_SYNC control block is released under call to
  // `m_thd->release_resource`, thus we can't put this sync
  // point directly before `pop_protocol`.
  // Second constrain is that `THD::disconnect` marks
  // this connection as killed, which disables DEBUG_SYNC.
  DEBUG_SYNC(m_thd, "srv_session_close");

  m_thd->disconnect();

#ifdef HAVE_PSI_THREAD_INTERFACE
  set_psi(nullptr);
#endif

  m_thd->release_resources();

  Global_THD_manager::get_instance()->remove_thd(m_thd);

  mysql_mutex_lock(&m_thd->LOCK_thd_protocol);
  m_thd->pop_protocol();
  mysql_mutex_unlock(&m_thd->LOCK_thd_protocol);

  Connection_handler_manager::dec_connection_count();

  return false;
}

/**
  Sets session's state to attached

  @param stack  New stack address
*/
void Srv_session::set_attached(const char *stack) {
  m_state = SRV_SESSION_ATTACHED;
  thd_set_thread_stack(m_thd, stack);
}

/**
  Changes the state of a session to detached
*/
void Srv_session::set_detached() {
  m_state = SRV_SESSION_DETACHED;
  thd_set_thread_stack(m_thd, nullptr);
}

int Srv_session::execute_command(enum enum_server_command command,
                                 const union COM_DATA *data,
                                 const CHARSET_INFO *client_cs,
                                 const struct st_command_service_cbs *callbacks,
                                 enum cs_text_or_binary text_or_binary,
                                 void *callbacks_context) {
  DBUG_TRACE;

  if (!srv_session_server_is_available()) {
    if (m_err_protocol_ctx.handler)
      m_err_protocol_ctx.handler(m_err_protocol_ctx.handler_context,
                                 ER_SESSION_WAS_KILLED,
                                 ER_DEFAULT(ER_SESSION_WAS_KILLED));
    return 1;
  }

  if (m_thd->killed) {
    if (m_err_protocol_ctx.handler)
      m_err_protocol_ctx.handler(m_err_protocol_ctx.handler_context,
                                 ER_SESSION_WAS_KILLED,
                                 ER_DEFAULT(ER_SESSION_WAS_KILLED));
    return 1;
  }

  assert(m_thd->get_protocol() == &m_protocol_error);

  // RAII:the destructor restores the state
  Srv_session::Session_backup_and_attach backup(this, false);

  if (backup.attach_error) return 1;

  if (client_cs && m_thd->variables.character_set_results != client_cs &&
      thd_init_client_charset(m_thd, client_cs->number))
    return 1;

  /* Switch to different callbacks */
  Protocol_callback client_proto(callbacks, text_or_binary, callbacks_context);

  m_thd->push_protocol(&client_proto);

  mysql_audit_release(m_thd);

  /*
    The server does it for COM_QUERY in dispatch_sql_command() but not for
    COM_INIT_DB, for example
  */
  if (command != COM_QUERY) m_thd->reset_for_next_command();

  /* For per-query performance counters with log_slow_statement */
  struct System_status_var query_start_status;
  m_thd->clear_copy_status_var();
  if (opt_log_slow_extra) {
    m_thd->copy_status_var(&query_start_status);
  }

  mysql_thread_set_secondary_engine(false);

  if (m_state != SRV_SESSION_ASSOCIATED) {
    assert(m_thd->m_statement_psi == nullptr);
    m_thd->m_statement_psi = MYSQL_START_STATEMENT(
        &m_thd->m_statement_state, stmt_info_new_packet.m_key, m_thd->db().str,
        m_thd->db().length, m_thd->charset(), nullptr);
  }
  int ret = dispatch_command(m_thd, data, command);

  DEBUG_SYNC(m_thd, "wait_before_popping_protocol");
  m_thd->pop_protocol();
  assert(m_thd->get_protocol() == &m_protocol_error);
  return ret;
}

/**
  Sets the connection type.

  @see enum_vio_type

  @return
    false success
    true  failure
*/
bool Srv_session::set_connection_type(enum_vio_type v_type) {
  if (v_type < FIRST_VIO_TYPE || v_type > LAST_VIO_TYPE) return true;

  m_vio_type = v_type;
#ifdef HAVE_PSI_THREAD_INTERFACE
  if (is_attached()) PSI_THREAD_CALL(set_connection_type)(m_vio_type);
#endif /* HAVE_PSI_THREAD_INTERFACE */
  return false;
}

void Srv_session::set_associate() { m_state = SRV_SESSION_ASSOCIATED; }

void Srv_session::set_disassociate() { m_state = SRV_SESSION_DISASSOCIATED; }

bool Srv_session::is_associate() const {
  return m_state == SRV_SESSION_ASSOCIATE;
}

bool Srv_session::is_associated() const {
  return m_state == SRV_SESSION_ASSOCIATED;
}

bool Srv_session::associate() {
  if (!is_associate()) return true;
  const char *stack_start = m_thd->thread_stack ? m_thd->thread_stack : nullptr;
  thd_set_thread_stack(m_thd, stack_start);
  // This will install our new THD object as current_thd
  m_thd->store_globals();
  thd_clear_errors(m_thd);
  set_associate();
  return false;
}

bool Srv_session::disassociate() {
  if (!is_associated()) return true;

  if (!my_thread_equal(m_thd->real_id, my_thread_self())) {
    DBUG_PRINT("error", ("Attached to a different thread. Detach in it"));
    return true;
  }

  DBUG_PRINT("info",
             ("Session=%p THD=%p current_thd=%p", this, m_thd, current_thd));

  m_thd->restore_globals();
  set_disassociate();
  return false;
}

/**
  Callback for inspecting a THD object and modifying the peer_port member
*/
static void set_client_port_in_thd(THD *thd, uint16_t *input) {
  thd->peer_port = *input;
}

/**
  Sets the client port.

  @note The client port in SHOW PROCESSLIST, INFORMATION_SCHEMA.PROCESSLIST.
  This port is NOT shown in PERFORMANCE_SCHEMA.THREADS.

  @param port  Port number
*/
void Srv_session::set_client_port(uint16_t port) {
  Find_thd_with_id find_thd_with_id(m_thd->thread_id());
  THD_ptr thd_ptr =
      Global_THD_manager::get_instance()->find_thd(&find_thd_with_id);
  if (thd_ptr) set_client_port_in_thd(thd_ptr.get(), &port);
}

/**
  Returns the number opened sessions in thread initialized by this class.
*/
unsigned int Srv_session::session_count() { return server_session_list.size(); }

/**
  Returns the number currently running threads initialized by this class.
*/
unsigned int Srv_session::thread_count(const void *plugin) {
  return server_session_threads.count(plugin);
}

/**
  Check if current physical thread was created to be used with this class.
*/
bool Srv_session::is_srv_session_thread() {
  return nullptr != THR_srv_session_thread;
}
