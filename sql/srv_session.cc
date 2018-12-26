/*
   Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "srv_session.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "sql_class.h"
#include "sql_base.h"            // close_mysql_tables
#include "sql_connect.h"         // thd_init_client_charset
#include "mysqld_thd_manager.h"  // Global_THD_manager
#include "sql_audit.h"           // MYSQL_AUDIT_NOTIFY_CONNECTION_CONNECT
#include "log.h"                 // Query log
#include "my_thread_local.h"     // my_get_thread_local & my_set_thread_local
#include "mysqld.h"              // current_thd
#include "sql_parse.h"           // dispatch_command()
#include "sql_thd_internal_api.h" // thd_set_thread_stack
#include "mutex_lock.h"
#include "mysql/psi/psi.h"
#include "conn_handler/connection_handler_manager.h"
#include "sql_plugin.h"

#include <map>

/**
  @file
  class Srv_session implementation. See the method comments for more. Please,
  check also srv_session.h for more information.
*/


extern void thd_clear_errors(THD *thd);

static thread_local_key_t THR_stack_start_address;
static thread_local_key_t THR_srv_session_thread;
static bool srv_session_THRs_initialized= false;


/**
  A simple wrapper around a RW lock:
  Grabs the lock in the CTOR, releases it in the DTOR.
  The lock may be NULL, in which case this is a no-op.

  Based on Mutex_lock from include/mutex_lock.h
*/
class Auto_rw_lock_read
{
public:
  explicit Auto_rw_lock_read(mysql_rwlock_t *lock) : rw_lock(NULL)
  {
    if (lock && 0 == mysql_rwlock_rdlock(lock))
      rw_lock = lock;
  }

  ~Auto_rw_lock_read()
  {
    if (rw_lock)
      mysql_rwlock_unlock(rw_lock);
  }
private:
  mysql_rwlock_t *rw_lock;

  Auto_rw_lock_read(const Auto_rw_lock_read&);         /* Not copyable. */
  void operator=(const Auto_rw_lock_read&);            /* Not assignable. */
};


class Auto_rw_lock_write
{
public:
  explicit Auto_rw_lock_write(mysql_rwlock_t *lock) : rw_lock(NULL)
  {
    if (lock && 0 == mysql_rwlock_wrlock(lock))
      rw_lock = lock;
  }

  ~Auto_rw_lock_write()
  {
    if (rw_lock)
      mysql_rwlock_unlock(rw_lock);
  }
private:
  mysql_rwlock_t *rw_lock;

  Auto_rw_lock_write(const Auto_rw_lock_write&);        /* Non-copyable */
  void operator=(const Auto_rw_lock_write&);            /* Non-assignable */
};


class Thread_to_plugin_map
{
private:
  bool initted;
  bool psi_initted;
  mysql_mutex_t LOCK_collection;

#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key key_LOCK_collection;
#endif
  std::map<my_thread_t, const void*> collection;

public:
  /**
    Initializes the map

    @param null_val null value to be returned when element not found in the map

    @return
      false  success
      true   failure
  */
  bool init()
  {
    const char* category= "session";
    PSI_mutex_info all_mutexes[]=
    {
      { &key_LOCK_collection, "LOCK_srv_session_threads", PSI_FLAG_GLOBAL}
    };

    initted= true;
#ifdef HAVE_PSI_INTERFACE
    psi_initted= true;

    mysql_mutex_register(category, all_mutexes, array_elements(all_mutexes));
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
  bool deinit()
  {
    initted= false;
    mysql_mutex_destroy(&LOCK_collection);

    return false;
  }

  /**
    Adds a pthread to the list

    @param key   plugin

    @return
      false  success
      true   failure
  */
  bool add(my_thread_t thread, const void *plugin)
  {
    Mutex_lock lock(&LOCK_collection);
    try
    {
      std::map<my_thread_t, const void*>::iterator it= collection.find(thread);
      if (it == collection.end())
        collection[thread]= plugin;
    }
    catch (const std::bad_alloc &e)
    {
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
  unsigned int remove(my_thread_t thread)
  {
    Mutex_lock lock(&LOCK_collection);
    std::map<my_thread_t, const void*>::iterator it= collection.find(thread);
    if (it != collection.end())
    {
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
  bool clear()
  {
    Mutex_lock lock(&LOCK_collection);
    collection.clear();
    return false;
  }

  /**
    Returns the number of all threads
  */
  unsigned int size()
  {
    Mutex_lock lock(&LOCK_collection);
    return collection.size();    
  }

  /**
    Returns the number threads for a plugin

    @param plugin The plugin for which we need info.
  */
  unsigned int count(const void *plugin)
  {
    if (!plugin)
      return size();

    unsigned int ret= 0;
    Mutex_lock lock(&LOCK_collection);
    std::map<my_thread_t, const void*>::iterator it= collection.begin();
    for (; it != collection.end(); ++it)
    {
      if (it->second == plugin)
        ++ret;
    }
    return ret;
  }

  /**
    Kills all threads associated with a plugin

    @param plugin The plugin for which we need info.
  */
  unsigned int kill(const void *plugin)
  {
    std::list<my_thread_t> to_remove;

    Mutex_lock lock(&LOCK_collection);

    for (std::map<my_thread_t, const void*>::iterator it= collection.begin();
         it != collection.end();
         ++it)
    {
      if (!plugin || (it->second == plugin))
      {
        to_remove.push_back(it->first);

        my_thread_handle thread;
#ifndef _WIN32
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
        thread.thread= it->first;
        sql_print_error("Killing thread %lu", (unsigned long) it->first);
        if (!my_thread_cancel(&thread))
        {
          void *dummy_retval;
          my_thread_join(&thread, &dummy_retval);
        }
#endif
      }
    }
    if (to_remove.size())
    {
      for (std::list<my_thread_t>::iterator it= to_remove.begin();
           it != to_remove.end();
           ++it)
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
class Mutexed_map_thd_srv_session
{
public:
  class Do_Impl
  {
  public:
    virtual ~Do_Impl() {}
    /**
      Work on the session

      @return
        false  Leave the session in the map
        true   Remove the session from the map
    */
    virtual bool operator()(Srv_session*) = 0;
  };

private:
  /*
    The first type in the tuple should be the key type of
    Thread_to_plugin_map
  */
  typedef std::pair<const void*, Srv_session*> map_value_t;

  std::map<const THD*, map_value_t> collection;

  bool initted;
  bool psi_initted;

  mysql_rwlock_t LOCK_collection;

#ifdef HAVE_PSI_INTERFACE
  PSI_rwlock_key key_LOCK_collection;
#endif

public:
  /**
    Initializes the map

    @param null_val null value to be returned when element not found in the map

    @return
      false  success
      true   failure
  */
  bool init()
  {
    const char* category= "session";
    PSI_rwlock_info all_rwlocks[]=
    {
      { &key_LOCK_collection, "LOCK_srv_session_collection", PSI_FLAG_GLOBAL}
    };

    initted= true;
#ifdef HAVE_PSI_INTERFACE
    psi_initted= true;

    mysql_rwlock_register(category, all_rwlocks, array_elements(all_rwlocks));
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
  bool deinit()
  {
    initted= false;
    mysql_rwlock_destroy(&LOCK_collection);

    return false;
  }

  /**
    Searches for an element with in the map

    @param key Key of the element

    @return
      value of the element
      NULL  if not found
  */
  Srv_session* find(const THD* key)
  {
    Auto_rw_lock_read lock(&LOCK_collection);

    std::map<const THD*, map_value_t>::iterator it= collection.find(key);
    return (it != collection.end())? it->second.second : NULL;
  }

  /**
    Add an element to the map

    @param key     key
    @param plugin  secondary key
    @param value   value

    @return
      false  success
      true   failure
  */
  bool add(const THD* key, const void *plugin, Srv_session *session)
  {
    Auto_rw_lock_write lock(&LOCK_collection);
    try
    {
      collection[key]= std::make_pair(plugin, session);
    }
    catch (const std::bad_alloc &e)
    {
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
  bool remove(const THD* key)
  {
    Auto_rw_lock_write lock(&LOCK_collection);
    /*
      If we use erase with the key directly an exception could be thrown. The
      find method never throws. erase() with iterator as parameter also never
      throws.
    */
    std::map<const THD*, map_value_t>::iterator it= collection.find(key);
    if (it != collection.end())
      collection.erase(it);
    return false;
  }

  /**
    Removes all elements which have been added with plugin as plugin name.

    @param plugin key
    @param removed OUT Number of removed elements
  */
  void remove_all_of_plugin(const void *plugin, unsigned int &removed)
  {
    std::list<Srv_session *> to_close;
    removed= 0;

    {
      Auto_rw_lock_write lock(&LOCK_collection);

      for (std::map<const THD*, map_value_t>::iterator it= collection.begin();
           it != collection.end();
           ++it)
      {
        if (it->second.first == plugin)
          to_close.push_back(it->second.second);
      }
    }

    /* Outside of the lock as Srv_session::close() will try to
      remove itself from the list*/
    if ((removed= to_close.size()))
    {
      for (std::list<Srv_session *>::iterator it= to_close.begin();
           it != to_close.end();
           ++it)
      {
        Srv_session *session= *it;
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
  bool clear()
  {
    Auto_rw_lock_write lock(&LOCK_collection);
    collection.clear();
    return false;
  }

  /**
    Returns the number of elements in the maps
  */
  unsigned int size()
  {
    Auto_rw_lock_read lock(&LOCK_collection);
    return collection.size();
  }
};

static Mutexed_map_thd_srv_session server_session_list;
static Thread_to_plugin_map server_session_threads;

/**
  Constructs a session state object for Srv_session::execute_command.
  Saves state. Uses RAII.

  @param sess Session to backup
*/
Srv_session::
Session_backup_and_attach::Session_backup_and_attach(Srv_session *sess,
                                                     bool is_close_session)
  :session(sess),
   old_session(NULL),
   in_close_session(is_close_session)
{
  THD *c_thd= current_thd;
  const void *is_srv_session_thread= my_get_thread_local(THR_srv_session_thread);
  backup_thd= is_srv_session_thread? NULL:c_thd;

  if (is_srv_session_thread && c_thd && c_thd != &session->thd)
  {
    if ((old_session= server_session_list.find(c_thd)))
      old_session->detach();
  }

  attach_error= session->attach();
}


/**
  Destructs the session state object. In other words it restores to
  previous state.
*/
Srv_session::Session_backup_and_attach::~Session_backup_and_attach()
{
  if (backup_thd)
  {
    session->detach();
    backup_thd->store_globals();
#ifdef HAVE_PSI_THREAD_INTERFACE
    enum_vio_type vio_type= backup_thd->get_vio_type();
    if (vio_type != NO_VIO_TYPE)
      PSI_THREAD_CALL(set_connection_type)(vio_type);
#endif /* HAVE_PSI_THREAD_INTERFACE */
  }
  else if (in_close_session)
  {
    /*
      We should restore the old session only in case of close.
      In case of execute we should stay attached.
    */
    session->detach();
    if (old_session)
      old_session->attach();
  }
}


static int err_start_result_metadata(void*, uint, uint, const CHARSET_INFO *)
{
  return 1;
}

static int err_field_metadata(void*, struct st_send_field*, const CHARSET_INFO*)
{
  return 1;
}

static int err_end_result_metadata(void*, uint, uint)
{
  return 1;
}

static int err_start_row(void*)
{
  return 1;
}

static int err_end_row(void*)
{
  return 1;
}

static void err_abort_row(void*)
{
}

static ulong err_get_client_capabilities(void*)
{
  return 0;
}

static int err_get_null(void*)
{
  return 1;
}

static int err_get_integer(void*, longlong)
{
  return 1;
}

static int err_get_longlong(void*, longlong, uint)
{
  return 1;
}

static int err_get_decimal(void*, const decimal_t*)
{
  return 1;
}

static int err_get_double(void*, double, uint32)
{
  return 1;
}

static int err_get_date(void*, const MYSQL_TIME*)
{
  return 1;
}

static int err_get_time(void*, const MYSQL_TIME*, uint)
{
  return 1;
}

static int err_get_datetime(void*, const MYSQL_TIME*, uint)
{
  return 1;
}


static int err_get_string(void*, const char*, size_t,const CHARSET_INFO*)
{
  return 1;
}

static void err_handle_ok(void * ctx, uint server_status, uint warn_count,
                          ulonglong affected_rows, ulonglong last_insert_id,
                          const char * const message)
{
  Srv_session::st_err_protocol_ctx *pctx=
             static_cast<Srv_session::st_err_protocol_ctx*>(ctx);
  if (pctx && pctx->handler)
  {
    char buf[256];
    my_snprintf(buf, sizeof(buf),
                "OK status=%u warnings=%u affected=%llu last_id=%llu",
                server_status, warn_count, affected_rows, last_insert_id);
    pctx->handler(pctx->handler_context, 0, buf);
  }
}

static void err_handle_error(void * ctx, uint err_errno, const char * err_msg,
                             const char * sqlstate)
{
  Srv_session::st_err_protocol_ctx *pctx=
             static_cast<Srv_session::st_err_protocol_ctx*>(ctx);
  if (pctx && pctx->handler)
    pctx->handler(pctx->handler_context, err_errno, err_msg);
}

static void err_shutdown(void*, int server_shutdown){}


const struct st_command_service_cbs error_protocol_callbacks=
{
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
  err_shutdown
};


/**
  Modifies the PSI structures to (de)install a THD

  @param thd THD
*/
static void set_psi(THD *thd)
{
#ifdef HAVE_PSI_THREAD_INTERFACE
  struct PSI_thread *psi= PSI_THREAD_CALL(get_thread)();
  PSI_THREAD_CALL(set_thread_id)(psi, thd? thd->thread_id() : 0);
  PSI_THREAD_CALL(set_thread_THD)(psi, thd);
#endif
}


/**
  Initializes physical thread to use with session service.

  @param plugin Pointer to the plugin structure, passed to the plugin over
                the plugin init function.

  @return
    false  success
    true   failure
*/
bool Srv_session::init_thread(const void *plugin)
{
  int stack_start;
  if (my_thread_init() ||
      my_set_thread_local(THR_srv_session_thread, const_cast<void*>(plugin)) ||
      my_set_thread_local(THR_stack_start_address, &stack_start))
  {
    connection_errors_internal++;
    return true;
  }

  server_session_threads.add(my_thread_self(), plugin);

  return false;
}


/**
  Looks if there is currently attached session and detaches it.

  @param plugin  The plugin to be checked
*/
static void close_currently_attached_session_if_any(const st_plugin_int *plugin)
{
  THD *c_thd= current_thd;
  if (!c_thd)
    return;

  Srv_session* current_session= server_session_list.find(c_thd);

  if (current_session)
  {
    sql_print_error("Plugin %s is deinitializing a thread but "
                     "left a session attached. Detaching it forcefully.",
                     plugin->name.str);

    if (current_session->detach())
      sql_print_error("Failed to detach the session.");
  }
}


/**
  Looks if the plugin has any non-closed sessions and closes them forcefully

  @param plugin  The plugin to be checked
*/
static void close_all_sessions_of_plugin_if_any(const st_plugin_int *plugin)
{
  unsigned int removed_count;

  server_session_list.remove_all_of_plugin(plugin, removed_count);

  if (removed_count)
     sql_print_error("Closed forcefully %u session%s left opened by plugin %s",
                     removed_count, (removed_count > 1)? "s":"",
                     plugin? plugin->name.str : "SERVER_INTERNAL");
}


/**
  Deinitializes physical thread to use with session service
*/
void Srv_session::deinit_thread()
{
  const st_plugin_int *plugin= static_cast<const st_plugin_int *>(
                                my_get_thread_local(THR_srv_session_thread));
  if (plugin)
    close_currently_attached_session_if_any(plugin);

  if (server_session_threads.remove(my_thread_self()))
    sql_print_error("Failed to decrement the number of threads");

  if (!server_session_threads.count(plugin))
    close_all_sessions_of_plugin_if_any(plugin);

  my_set_thread_local(THR_srv_session_thread, NULL);

  DBUG_ASSERT(my_get_thread_local(THR_stack_start_address));
  my_set_thread_local(THR_stack_start_address, NULL);
  my_thread_end();
}


/**
  Checks if a plugin has left threads and sessions

  @param plugin  The plugin to be checked
*/
void Srv_session::check_for_stale_threads(const st_plugin_int *plugin)
{
  if (!plugin)
    return;

  unsigned int thread_count= server_session_threads.count(plugin);
  if (thread_count)
  {
    close_all_sessions_of_plugin_if_any(plugin);

    sql_print_error("Plugin %s did not deinitialize %u threads",
                    plugin->name.str, thread_count);

    unsigned int killed_count= server_session_threads.kill(plugin);
    sql_print_error("Killed %u threads of plugin %s",
                    killed_count, plugin->name.str);
  }
}


/**
  Inits the module

  @return
    false  success
    true   failure
*/
bool Srv_session::module_init()
{
  if (srv_session_THRs_initialized)
    return false;

  if (my_create_thread_local_key(&THR_stack_start_address, NULL) ||
      my_create_thread_local_key(&THR_srv_session_thread, NULL))
  {
    sql_print_error("Can't create thread key for SQL session service");
    return true;
  }
  srv_session_THRs_initialized= true;

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
bool Srv_session::module_deinit()
{
  if (srv_session_THRs_initialized)
  {
    srv_session_THRs_initialized= false;
    (void) my_delete_thread_local_key(THR_stack_start_address);
    (void) my_delete_thread_local_key(THR_srv_session_thread);

    server_session_list.clear();
    server_session_list.deinit();

    server_session_threads.clear();
    server_session_threads.deinit();

    srv_session_THRs_initialized= false;
  }
  return false;
}


/**
  Checks if the session is valid.

  Checked is if session is NULL, or in the list of opened sessions. If the
  session is not in this list it was either closed or the address is invalid.

  @return
    true  valid
    false not valid
*/
bool Srv_session::is_valid(const Srv_session *session)
{
  const THD *thd= session?
    reinterpret_cast<const THD*>(session + my_offsetof(Srv_session, thd)):
    NULL;
  return thd? (bool) server_session_list.find(thd) : false;
}


/**
  Constructs a server session

  @param error_cb       Default completion callback
  @param err_cb_ctx     Plugin's context, opaque pointer that would
                        be provided to callbacks. Might be NULL.
*/
Srv_session::Srv_session(srv_session_error_cb err_cb, void *err_cb_ctx) :
  da(false), err_protocol_ctx(err_cb, err_cb_ctx),
  protocol_error(&error_protocol_callbacks, CS_TEXT_REPRESENTATION,
                 (void*)&err_protocol_ctx),
  state(SRV_SESSION_CREATED), vio_type(NO_VIO_TYPE)
{
  thd.mark_as_srv_session();
}


/**
  Opens a server session

  @return
    false  on success
    true   on failure
*/
bool Srv_session::open()
{
  DBUG_ENTER("Srv_session::open");

  DBUG_PRINT("info",("Session=%p  THD=%p  DA=%p", this, &thd, &da));
  DBUG_ASSERT(state == SRV_SESSION_CREATED || state == SRV_SESSION_CLOSED);

  thd.set_protocol(&protocol_error);
  thd.push_diagnostics_area(&da);
  /*
    thd.stack_start will be set once we start attempt to attach.
    store_globals() will check for it, so we will set it beforehand.

    No store_globals() here as the session is always created in a detached
    state. Attachment with store_globals() will happen on demand.
  */
  if (thd_init_client_charset(&thd, my_charset_utf8_general_ci.number))
  {
    connection_errors_internal++;
    if (err_protocol_ctx.handler)
      err_protocol_ctx.handler(err_protocol_ctx.handler_context,
                               ER_OUT_OF_RESOURCES,
                               ER_DEFAULT(ER_OUT_OF_RESOURCES));
    Connection_handler_manager::dec_connection_count();
    DBUG_RETURN(true);
  }

  thd.update_charset();

  thd.set_new_thread_id();

  DBUG_PRINT("info", ("thread_id=%d", thd.thread_id()));

  thd.set_time();
  thd.thr_create_utime= thd.start_utime= my_micro_time();

  /*
    Disable QC - plugins will most probably install their own protocol
    and it won't be compatible with the QC. In addition, Protocol_error
    is not compatible with the QC.
  */
  thd.variables.query_cache_type = 0;

  thd.set_command(COM_SLEEP);
  thd.init_for_queries();

  Global_THD_manager::get_instance()->add_thd(&thd);

  const void *plugin= my_get_thread_local(THR_srv_session_thread);

  server_session_list.add(&thd, plugin, this);

  if (mysql_audit_notify(&thd,
                         AUDIT_EVENT(MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE)))
  {
    Connection_handler_manager::dec_connection_count();
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/**
  Attaches the session to the current physical thread

  @param session  Session handle

  @returns
    false   success
    true    failure
*/
bool Srv_session::attach()
{
  const bool first_attach= (state == SRV_SESSION_CREATED);
  DBUG_ENTER("Srv_session::attach");
  DBUG_PRINT("info",("current_thd=%p", current_thd));

  if (is_attached())
  {
    if (!my_thread_equal(thd.real_id, my_thread_self()))
    {
      DBUG_PRINT("error", ("Attached to different thread. Detach in it"));
      DBUG_RETURN(true);
    }
    /* As it is attached, no need to do anything */
    DBUG_RETURN(false);
  }

  if (&thd == current_thd)
    DBUG_RETURN(false);

  THD *old_thd= current_thd;
  DBUG_PRINT("info",("current_thd=%p", current_thd));

  if (old_thd)
    old_thd->restore_globals();

  DBUG_PRINT("info",("current_thd=%p", current_thd));

  const char *new_stack= my_get_thread_local(THR_srv_session_thread)?
        (const char*)my_get_thread_local(THR_stack_start_address):
        (old_thd? old_thd->thread_stack : NULL);

  /*
    Attach optimistically, as this will set thread_stack,
    which needed by store_globals()
  */
  set_attached(new_stack);

  // This will install our new THD object as current_thd
  if (thd.store_globals())
  {
    DBUG_PRINT("error", ("Error while storing globals"));

    if (old_thd)
      old_thd->store_globals();

    set_psi(old_thd);

    set_detached();
    DBUG_RETURN(true);
  }
  Srv_session* old_session= server_session_list.find(old_thd);

  /* Really detach only if we are sure everything went fine */
  if (old_session)
    old_session->set_detached();

  thd_clear_errors(&thd);

  set_psi(&thd);

#ifdef HAVE_PSI_THREAD_INTERFACE
   PSI_THREAD_CALL(set_connection_type)(vio_type != NO_VIO_TYPE?
                                        vio_type : thd.get_vio_type());
#endif /* HAVE_PSI_THREAD_INTERFACE */

  if (first_attach)
  {
    /*
      At first attach the security context should have been already set and
      and this will report corect information.
    */
    if (mysql_audit_notify(&thd, AUDIT_EVENT(MYSQL_AUDIT_CONNECTION_CONNECT)))
      DBUG_RETURN(true);
    query_logger.general_log_print(&thd, COM_CONNECT, NullS);
  }

  DBUG_RETURN(false);
}


/**
  Detaches the session from the current physical thread.

  @returns
    false success
    true  failure
*/
bool Srv_session::detach()
{
  DBUG_ENTER("Srv_session::detach");

  if (!is_attached())
    DBUG_RETURN(false);

  if (!my_thread_equal(thd.real_id, my_thread_self()))
  {
    DBUG_PRINT("error", ("Attached to a different thread. Detach in it"));
    DBUG_RETURN(true);
  }

  DBUG_PRINT("info",("Session=%p THD=%p current_thd=%p",
                     this, &thd, current_thd));

  DBUG_ASSERT(&thd == current_thd);
  thd.restore_globals();

  set_psi(NULL);
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
  DBUG_RETURN(false);
}


/**
  Closes the session

  @returns
    false Session successfully closed
    true  No such session exists / Session is attached to a different thread
*/
bool Srv_session::close()
{
  DBUG_ENTER("Srv_session::close");

  DBUG_PRINT("info",("Session=%p THD=%p current_thd=%p",
                     this, &thd, current_thd));

  DBUG_ASSERT(state < SRV_SESSION_CLOSED);

  /*
    RAII
    We store the state (the currently attached session, if different than
    our and then attach our.
    The destructor will attach the session we detached.
  */

  Srv_session::Session_backup_and_attach backup(this, true);

  if (backup.attach_error)
    DBUG_RETURN(true);

  state= SRV_SESSION_CLOSED;

  server_session_list.remove(&thd);

  /*
    Log to general log must happen before release_resources() as
    current_thd will be different then.
  */
  query_logger.general_log_print(&thd, COM_QUIT, NullS);
  mysql_audit_notify(&thd, AUDIT_EVENT(MYSQL_AUDIT_CONNECTION_DISCONNECT), 0);

  close_mysql_tables(&thd);

  thd.pop_diagnostics_area();

  thd.get_stmt_da()->reset_diagnostics_area();

  thd.disconnect();

  set_psi(NULL);

  thd.release_resources();

  Global_THD_manager::get_instance()->remove_thd(&thd);

  Connection_handler_manager::dec_connection_count();

  DBUG_RETURN(false);
}


/**
  Sets session's state to attached

  @param stack  New stack address
*/
void Srv_session::set_attached(const char *stack)
{
  state= SRV_SESSION_ATTACHED;
  thd_set_thread_stack(&thd, stack);
}


/**
  Changes the state of a session to detached
*/
void Srv_session::set_detached()
{
  state= SRV_SESSION_DETACHED;
  thd_set_thread_stack(&thd, NULL);
}

#include "auth_common.h"

int Srv_session::execute_command(enum enum_server_command command,
                                 const union COM_DATA * data,
                                 const CHARSET_INFO * client_cs,
                                 const struct st_command_service_cbs *callbacks,
                                 enum cs_text_or_binary text_or_binary,
                                 void * callbacks_context)
{
  DBUG_ENTER("Srv_session::execute_command");

  if (!srv_session_server_is_available())
  {
    if (err_protocol_ctx.handler)
      err_protocol_ctx.handler(err_protocol_ctx.handler_context,
                               ER_SESSION_WAS_KILLED,
                               ER_DEFAULT(ER_SESSION_WAS_KILLED));
    DBUG_RETURN(1);
  }

  if (thd.killed)
  {
    if (err_protocol_ctx.handler)
      err_protocol_ctx.handler(err_protocol_ctx.handler_context,
                               ER_SESSION_WAS_KILLED,
                               ER_DEFAULT(ER_SESSION_WAS_KILLED));
    DBUG_RETURN(1);
  }

  DBUG_ASSERT(thd.get_protocol() == &protocol_error);

  // RAII:the destructor restores the state
  Srv_session::Session_backup_and_attach backup(this, false);

  if (backup.attach_error)
    DBUG_RETURN(1);

  if (client_cs &&
      thd.variables.character_set_results != client_cs &&
      thd_init_client_charset(&thd, client_cs->number))
    DBUG_RETURN(1);

  /* Switch to different callbacks */
  Protocol_callback client_proto(callbacks, text_or_binary, callbacks_context);

  thd.set_protocol(&client_proto);

  mysql_audit_release(&thd);

  /*
    The server does it for COM_QUERY in mysql_parse() but not for
    COM_INIT_DB, for example
  */
  if (command != COM_QUERY)
    thd.reset_for_next_command();

  DBUG_ASSERT(thd.m_statement_psi == NULL);
  thd.m_statement_psi= MYSQL_START_STATEMENT(&thd.m_statement_state,
                                             stmt_info_new_packet.m_key,
                                             thd.db().str,
                                             thd.db().length,
                                             thd.charset(), NULL);
  int ret= dispatch_command(&thd, data, command);

  thd.set_protocol(&protocol_error);
  DBUG_RETURN(ret);
}


/**
  Sets the connection type.

  @see enum_vio_type

  @return
    false success
    true  failure
*/
bool Srv_session::set_connection_type(enum_vio_type v_type)
{
  if (v_type < FIRST_VIO_TYPE || v_type > LAST_VIO_TYPE)
    return true;

  vio_type= v_type;
#ifdef HAVE_PSI_THREAD_INTERFACE
  if (is_attached())
    PSI_THREAD_CALL(set_connection_type)(vio_type);
#endif /* HAVE_PSI_THREAD_INTERFACE */
  return false;
}


/**
  Template class for scanning the thd list in the Global_THD_manager and
  modifying and element from the list. This template is for functions that
  need to change data of a THD without getting into races.

  If the THD is found, a callback is executed under THD::Lock_thd_data.
  A pointer to a member variable is passed as a second parameter to the
  callback. The callback should store its result there.

  After the search has been completed the result value can be obtained by
  calling get_result().
*/
template <typename INPUT_TYPE>
class Find_thd_by_id_with_callback_set: public Find_THD_Impl
{
public:
  typedef void (*callback_t)(THD *thd, INPUT_TYPE *result);

  Find_thd_by_id_with_callback_set(my_thread_id t_id, callback_t cb,
                                   INPUT_TYPE in):
    thread_id(t_id), callback(cb), input(in) {}

  /**
    Callback called for every THD in the thd_list.

    When a thread is found the callback function passed to the constructor
    is invoked under THD::Lock_thd_data
  */
  virtual bool operator()(THD *thd)
  {
    if (thd->thread_id() == thread_id)
    {
      Mutex_lock lock(&thd->LOCK_thd_data);
      callback(thd, &input);
      return true;
    }
    return false;
  }
private:
  my_thread_id thread_id;
  callback_t callback;
  INPUT_TYPE input;
};

/**
  Callback for inspecting a THD object and modifying the peer_port member
*/
static void set_client_port_in_thd(THD *thd, uint16_t *input)
{
  thd->peer_port= *input;
}


/**
  Sets the client port.

  @note The client port in SHOW PROCESSLIST, INFORMATION_SCHEMA.PROCESSLIST.
  This port is NOT shown in PERFORMANCE_SCHEMA.THREADS.

  @param port  Port number
*/
void Srv_session::set_client_port(uint16_t port)
{
  Find_thd_by_id_with_callback_set<uint16_t>
     find_thd_with_id(thd.thread_id(), set_client_port_in_thd, port);
  Global_THD_manager::get_instance()->find_thd(&find_thd_with_id);
}


/**
  Returns the number opened sessions in thread initialized by this class.
*/
unsigned int Srv_session::session_count()
{
  return server_session_list.size();
}


/**
  Returns the number currently running threads initialized by this class.
*/
unsigned int Srv_session::thread_count(const void *plugin)
{
  return server_session_threads.count(plugin);
}
