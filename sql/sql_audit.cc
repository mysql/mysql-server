/* Copyright (c) 2007, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/sql_audit.h"

#include <sys/types.h>

#include "lex_string.h"
#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/my_loglevel.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/strings/m_ctype.h"
#include "mysqld_error.h"
#include "nulls.h"
#include "prealloced_array.h"
#include "sql/auto_thd.h"  // Auto_THD
#include "sql/command_mapping.h"
#include "sql/current_thd.h"
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/log.h"
#include "sql/mysqld.h"     // sql_statement_names
#include "sql/sql_class.h"  // THD
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_plugin.h"  // my_plugin_foreach
#include "sql/sql_plugin_ref.h"
#include "sql/sql_rewrite.h"  // mysql_rewrite_query
#include "sql/table.h"
#include "sql_string.h"
#include "strxnmov.h"
#include "thr_mutex.h"

namespace {
/**
  @class Event_tracking_error_handler

  Error handler that controls error reporting by plugin.
*/
class Event_tracking_error_handler : public Internal_error_handler {
 private:
  /**
    @brief Blocked copy constructor (private).
  */
  Event_tracking_error_handler(const Event_tracking_error_handler &obj
                               [[maybe_unused]])
      : m_thd(nullptr),
        m_warning_message(nullptr),
        m_error_reported(false),
        m_active(false) {}

 public:
  /**
    @brief Construction.

    @param thd            Current thread data.
    @param warning_message Warning message used when error has been
                               suppressed.
    @param active              Specifies whether the handler is active or not.
                               Optional parameter (default is true).
  */
  Event_tracking_error_handler(THD *thd, const char *warning_message,
                               bool active = true)
      : m_thd(thd),
        m_warning_message(warning_message),
        m_error_reported(false),
        m_active(active) {
    if (m_active) {
      /* Activate the error handler. */
      m_thd->push_internal_handler(this);
    }
  }

  /**
    @brief Destruction.
  */
  ~Event_tracking_error_handler() override {
    if (m_active) {
      /* Deactivate this handler. */
      m_thd->pop_internal_handler();
    }
  }

  /**
    @brief Simplified custom handler.

    @returns True on error rejection, otherwise false.
  */
  virtual bool handle() = 0;

  /**
    @brief Error handler.

    @see Internal_error_handler::handle_condition

    @returns True on error rejection, otherwise false.
  */
  bool handle_condition(THD *, uint sql_errno, const char *sqlstate,
                        Sql_condition::enum_severity_level *,
                        const char *msg) override {
    if (m_active && handle()) {
      /* Error has been rejected. Write warning message. */
      print_warning(m_warning_message, sql_errno, sqlstate, msg);

      m_error_reported = true;

      return true;
    }

    return false;
  }

  /**
    @brief Warning print routine.

    Also prints the underlying error attributes if supplied.

    @param warn_msg  Warning message to be printed.
    @param sql_errno The error number of the underlying error
    @param sqlstate  The SQL state of the underlying error. NULL if none
    @param msg       The text of the underlying error. NULL if none
  */
  virtual void print_warning(const char *warn_msg, uint sql_errno,
                             const char *sqlstate, const char *msg) {
    LogErr(WARNING_LEVEL, ER_AUDIT_WARNING, warn_msg, sql_errno,
           sqlstate ? sqlstate : "<NO_STATE>", msg ? msg : "<NO_MESSAGE>");
  }

  /**
    @brief Convert the result value returned from the audit api.

    @param result Result value received from the plugin function.

    @returns Converted result value.
  */
  int get_result(int result) { return m_error_reported ? 0 : result; }

 private:
  /** Current thread data. */
  THD *m_thd;

  /** Warning message used when the error is rejected. */
  const char *m_warning_message;

  /** Error has been reported. */
  bool m_error_reported;

  /** Handler has been activated. */
  const bool m_active;
};

/**
  @class Ignore_event_tracking_error_handler

  Ignore all errors notified from within plugin.
*/
class Ignore_event_tracking_error_handler
    : public Event_tracking_error_handler {
 public:
  /**
    @brief Construction.

    @param thd             Current thread data.
    @param event_name      Textual form of the audit event enum.
  */
  Ignore_event_tracking_error_handler(THD *thd, const char *event_name)
      : Event_tracking_error_handler(thd, ""), m_event_name(event_name) {}

  /**
    @brief Ignore all errors.

    @retval True on error rejection, otherwise false.
  */
  bool handle() override { return true; }

  /**
    @brief Custom warning print routine.

    Also prints the underlying error attributes if supplied.

    @param warn_msg  Warning message to be printed.
    @param sql_errno The error number of the underlying error
    @param sqlstate  The SQL state of the underlying error. NULL if none
    @param msg       The text of the underlying error. NULL if none
  */
  void print_warning(const char *warn_msg [[maybe_unused]], uint sql_errno,
                     const char *sqlstate, const char *msg) override {
    LogErr(WARNING_LEVEL, ER_AUDIT_CANT_ABORT_EVENT, m_event_name, sql_errno,
           sqlstate ? sqlstate : "<NO_STATE>", msg ? msg : "<NO_MESSAGE>");
  }

 private:
  /**
  @brief Event name used in the warning message.
  */
  const char *m_event_name;
};

/**
  @class Ignore_command_start_error_handler

  Ignore error for specified commands.
*/
class Ignore_command_start_error_handler : public Event_tracking_error_handler {
 public:
  /**
    @brief Construction.

    @param thd     Current thread data.
    @param command Current command that the handler will be active against.
    @param command_text SQL command.
  */
  Ignore_command_start_error_handler(THD *thd, enum_server_command command,
                                     const char *command_text)
      : Event_tracking_error_handler(thd, "", ignore_command(command)),
        m_command(command),
        m_command_text(command_text) {}

  /**
    @brief Error for specified command handling routine.

    @retval True on error rejection, otherwise false.
  */
  bool handle() override { return ignore_command(m_command); }

  /**
    @brief Custom warning print routine.

    Also prints the underlying error attributes if supplied.

    @param warn_msg  Warning message to be printed.
    @param sql_errno The error number of the underlying error
    @param sqlstate  The SQL state of the underlying error. NULL if none
    @param msg       The text of the underlying error. NULL if none
  */
  void print_warning(const char *warn_msg [[maybe_unused]], uint sql_errno,
                     const char *sqlstate, const char *msg) override {
    LogErr(WARNING_LEVEL, ER_AUDIT_CANT_ABORT_COMMAND, m_command_text,
           sql_errno, sqlstate ? sqlstate : "<NO_STATE>",
           msg ? msg : "<NO_MESSAGE>");
  }

  /**
    @brief Check whether the command is to be ignored.

    @retval True whether the command is to be ignored. Otherwise false.
  */
  static bool ignore_command(enum_server_command command) {
    /* Ignore these commands. The plugin cannot abort on these commands. */
    if (command == COM_QUIT || command == COM_PING ||
        command == COM_SLEEP || /* Deprecated commands from here. */
        command == COM_CONNECT || command == COM_TIME ||
        command == COM_DELAYED_INSERT || command == COM_END) {
      return true;
    }

    return false;
  }

 private:
  /** Command that the handler is active against. */
  enum_server_command m_command;

  /** Command string. */
  const char *m_command_text;
};

/**
  Check, whether masks specified by lhs parameter and rhs parameters overlap.

  @param lhs First mask to check.
  @param rhs Second mask to check.

  @return false, when masks overlap, otherwise true.
*/
static inline bool check_audit_mask(const unsigned long lhs,
                                    const unsigned long rhs) {
  return !(lhs & rhs);
}

/**
  Check, whether mask arrays specified by the lhs parameter and rhs parameter
  overlap.

  @param lhs First mask array to check.
  @param rhs Second mask array to check.

  @return false, when mask array overlap, otherwise true.
*/
static inline bool check_audit_mask(const unsigned long *lhs,
                                    const unsigned long *rhs) {
  int i;
  for (i = MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE; i++)
    if (!check_audit_mask(*lhs++, *rhs++)) return false;

  return true;
}

/**
  Distributes an audit event to components

  @param [in]     thd           THD that generated the event.
  @param [in,out] generic_event Event to be audited
*/

int event_tracking_dispatch(THD *thd, st_mysql_event_generic *generic_event) {
  int result = 0;
  result = thd->event_notify(generic_event);
  return result;
}

int event_tracking_dispatch_error(THD *thd, const char *event_name,
                                  st_mysql_event_generic *generic_event) {
  int result = 0;
  bool err = thd ? thd->get_stmt_da()->is_error() : true;

  if (err) /* Audit API cannot modify the already set DA's error state. */
    event_tracking_dispatch(thd, generic_event);
  else {
    /* We are not is the error state, we can modify the existing one. */
    thd->get_stmt_da()->set_overwrite_status(true);

    result = event_tracking_dispatch(thd, generic_event);

    if (result) {
      if (!thd->get_stmt_da()->is_error()) {
        my_error(ER_AUDIT_API_ABORT, MYF(0), event_name, result);
      }
    }

    thd->get_stmt_da()->set_overwrite_status(false);

    /* Because we rely on the error state, we have to notify our
    caller that the Audit API returned with error state. */
    if (thd->get_stmt_da()->is_error()) result = result != 0 ? result : 1;
  }

  return result;
}

}  // namespace

/**
  @struct st_mysql_subscribe_event

  Plugin event subscription structure. Used during acquisition of the plugins
  into user session.
*/
struct st_mysql_subscribe_event {
  /*
    Event class.
  */
  mysql_event_class_t event_class;
  /*
    Event subclass.
  */
  unsigned long event_subclass;
  /*
    The array that keeps sum (OR) mask of all plugins that subscribe
    to the event specified by the event_class and event_subclass.

    lookup_mask is acquired during build_lookup_mask call.
  */
  unsigned long lookup_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
  /*
    The array that keeps sum (OR) mask of all plugins that are acquired
    to the current session as a result of acquire_plugins call.

    subscribed_mask is acquired during acquisition of the plugins
    (acquire_plugins call).
  */
  unsigned long subscribed_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
  /*
    The array that keeps sum (OR) mask of all plugins that were not acquired
    to the current session as a result of acquire_plugins call.
  */
  unsigned long not_subscribed_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
};

unsigned long mysql_global_audit_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];

static mysql_mutex_t LOCK_audit_mask;

/**
  Add mask specified by the rhs parameter to the mask parameter.

  @param mask Mask, to which rhs mask is to be added.
  @param rhs  Mask to be added to mask parameter.
*/
static inline void add_audit_mask(unsigned long *mask, unsigned long rhs) {
  *mask |= rhs;
}

/**
  Add entire audit mask specified by the src to dst.

  @param dst Destination mask array pointer.
  @param src Source mask array pointer.
*/
static inline void add_audit_mask(unsigned long *dst,
                                  const unsigned long *src) {
  int i;
  for (i = MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE; i++)
    add_audit_mask(dst++, *src++);
}

/**
  Fill query info extracted from the thread object and return
  the thread object charset info.

  @param[in]  thd     Thread data.
  @param[out] query   SQL query text.

  @return SQL query charset.
*/
inline const char *thd_get_audit_query(THD *thd,
                                       mysql_cstring_with_length *query) {
  /*
    If we haven't tried to rewrite the query to obfuscate passwords
    etc. yet, do so now.
  */
  if (thd->rewritten_query().length() == 0) mysql_rewrite_query(thd);

  /*
    If there was something to rewrite, use the rewritten query;
    otherwise, just use the original as submitted by the client.
  */
  if (thd->rewritten_query().length() > 0) {
    query->str = thd->rewritten_query().ptr();
    query->length = thd->rewritten_query().length();
    return thd->rewritten_query().charset()->csname;
  } else {
    query->str = thd->query().str;
    query->length = thd->query().length;
    return thd->charset()->csname;
  }
}

/**
  Acquire plugin masks subscribing to the specified event of the specified
  class, passed by arg parameter. lookup_mask of the st_mysql_subscribe_event
  structure is filled, when the plugin is interested in receiving the event.

  @param         plugin Plugin reference.
  @param[in,out] arg    Opaque st_mysql_subscribe_event pointer.

  @return false is always returned.
*/
static bool acquire_lookup_mask(THD *, plugin_ref plugin, void *arg) {
  st_mysql_subscribe_event *evt = static_cast<st_mysql_subscribe_event *>(arg);
  st_mysql_audit *audit = plugin_data<st_mysql_audit *>(plugin);

  /* Check if this plugin is interested in the event */
  if (!check_audit_mask(audit->class_mask[evt->event_class],
                        evt->event_subclass))
    add_audit_mask(evt->lookup_mask, audit->class_mask);

  return false;
}

/**
  Acquire and lock any additional audit plugins, whose subscription
  mask overlaps with the lookup_mask.

  @param         thd    Current session THD.
  @param         plugin Plugin reference.
  @param[in,out] arg    Opaque st_mysql_subscribe_event pointer.

  @return This function always returns false.
*/
static bool acquire_plugins(THD *thd, plugin_ref plugin, void *arg) {
  st_mysql_subscribe_event *evt = static_cast<st_mysql_subscribe_event *>(arg);
  st_mysql_audit *data = plugin_data<st_mysql_audit *>(plugin);

  /* Check if this plugin is interested in the event */
  if (check_audit_mask(data->class_mask, evt->lookup_mask)) {
    add_audit_mask(evt->not_subscribed_mask, data->class_mask);
    return false;
  }

  /* Prevent from adding the same plugin more than one time. */
  if (!thd->audit_class_plugins.exists(plugin)) {
    /* lock the plugin and add it to the list */
    plugin = my_plugin_lock(nullptr, &plugin);

    /* The plugin could not be acquired. */
    if (plugin == nullptr) {
      /* Add this plugin mask to non subscribed mask. */
      add_audit_mask(evt->not_subscribed_mask, data->class_mask);
      return false;
    }

    thd->audit_class_plugins.push_back(plugin);
  }

  /* Copy subscription mask from the plugin into the array. */
  add_audit_mask(evt->subscribed_mask, data->class_mask);

  return false;
}

/**
  Acquire audit plugins. Ensure that audit plugins interested in given event
  class are locked by current thread.

  @param thd            MySQL thread handle.
  @param event_class    Audit event class.
  @param event_subclass Audit event subclass.
  @param check_audited  Take into account m_auditing_activated flag
                        of the THD.

  @return Zero, when there is a plugins interested in the event specified
          by event_class and event_subclass. Otherwise non zero value is
          returned.
*/
int mysql_audit_acquire_plugins(THD *thd, mysql_event_class_t event_class,
                                unsigned long event_subclass,
                                bool check_audited) {
  DBUG_TRACE;

  if (check_audited && thd && thd->m_audited == false) return 1;

  unsigned long global_mask = mysql_global_audit_mask[event_class];

  if (thd && !check_audit_mask(global_mask, event_subclass) &&
      check_audit_mask(thd->audit_class_mask[event_class], event_subclass)) {
    /*
      There is a plugin registered for the subclass, but THD has not
      registered yet for this event. Refresh THD class mask.
    */
    st_mysql_subscribe_event evt = {event_class,
                                    event_subclass,
                                    {
                                        0,
                                    },
                                    {
                                        0,
                                    },
                                    {
                                        0,
                                    }};
    plugin_foreach_func *funcs[] = {acquire_lookup_mask, acquire_plugins,
                                    nullptr};
    /*
      Acquire lookup_mask, which contains mask of all plugins that subscribe
      event specified by the event_class and event_subclass
      (acquire_lookup_mask).
      Load plugins that overlap with the lookup_mask (acquire_plugins).
    */
    plugin_foreach(thd, funcs, MYSQL_AUDIT_PLUGIN, &evt);
    /*
      Iterate through event masks of the acquired plugin, excluding masks
      of the the plugin not acquired. It's more likely that these plugins will
      be acquired during the next audit plugin acquisition.
    */
    int i;
    for (i = MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE; i++)
      add_audit_mask(&thd->audit_class_mask[i],
                     (evt.subscribed_mask[i] ^ evt.not_subscribed_mask[i]) &
                         evt.subscribed_mask[i]);

    global_mask = thd->audit_class_mask[event_class];
  }

  /* Check whether there is a plugin registered for this event. */
  return check_audit_mask(global_mask, event_subclass) ? 1 : 0;
}

/**
  Release any resources associated with the current thd.

  @param[in] thd Current thread

*/

void mysql_audit_release(THD *thd) {
  plugin_ref *plugins, *plugins_last;

  if (!thd || thd->audit_class_plugins.empty()) return;

  plugins = thd->audit_class_plugins.begin();
  plugins_last = thd->audit_class_plugins.end();
  for (; plugins != plugins_last; plugins++) {
    st_mysql_audit *data = plugin_data<st_mysql_audit *>(*plugins);

    /* Check to see if the plugin has a release method */
    if (!(data->release_thd)) continue;

    /* Tell the plugin to release its resources */
    data->release_thd(thd);
  }

  /* Move audit plugins array from THD to a tmp variable in order to avoid calls
     to them via THD after the plugin is unlocked (#bug34594035) */
  Plugin_array audit_class_plugins(std::move(thd->audit_class_plugins));

  /* Now we actually unlock the plugins */
  plugin_unlock_list(nullptr, audit_class_plugins.begin(),
                     audit_class_plugins.size());

  /* Reset the state of thread values */
  thd->audit_class_mask.clear();
  thd->audit_class_mask.resize(MYSQL_AUDIT_CLASS_MASK_SIZE);
}

void mysql_audit_enable_auditing(THD *thd) { thd->m_audited = true; }

/**
  Initialize thd variables used by Audit

  @param[in] thd Current thread

*/

void mysql_audit_init_thd(THD *thd) {
  thd->audit_class_mask.clear();
  thd->audit_class_mask.resize(MYSQL_AUDIT_CLASS_MASK_SIZE);
}

/**
  Free thd variables used by Audit

  @param thd Current thread
*/

void mysql_audit_free_thd(THD *thd) {
  mysql_audit_release(thd);
  assert(thd->audit_class_plugins.empty());
}

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_audit_mask;

static PSI_mutex_info all_audit_mutexes[] = {
    {&key_LOCK_audit_mask, "LOCK_audit_mask", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME}};

static void init_audit_psi_keys(void) {
  const char *category = "sql";
  int count;

  count = static_cast<int>(array_elements(all_audit_mutexes));
  mysql_mutex_register(category, all_audit_mutexes, count);
}
#endif /* HAVE_PSI_INTERFACE */

/**
  Initialize Audit global variables
*/

void mysql_audit_initialize() {
#ifdef HAVE_PSI_INTERFACE
  init_audit_psi_keys();
#endif

  mysql_mutex_init(key_LOCK_audit_mask, &LOCK_audit_mask, MY_MUTEX_INIT_FAST);
  memset(mysql_global_audit_mask, 0, sizeof(mysql_global_audit_mask));
}

/**
  Finalize Audit global variables
*/

void mysql_audit_finalize() { mysql_mutex_destroy(&LOCK_audit_mask); }

/**
  Initialize an Audit plug-in

  @param[in] plugin Plugin structure pointer to be initialized.

  @retval false  OK
  @retval true   There was an error.
*/

int initialize_audit_plugin(st_plugin_int *plugin) {
  st_mysql_audit *data = (st_mysql_audit *)plugin->plugin->info;
  int i;
  unsigned long masks = 0;

  for (i = MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE; i++) {
    masks |= data->class_mask[i];
  }

  if (data->class_mask[MYSQL_AUDIT_AUTHORIZATION_CLASS]) {
    LogErr(ERROR_LEVEL, ER_AUDIT_PLUGIN_DOES_NOT_SUPPORT_AUDIT_AUTH_EVENTS,
           plugin->name.str);
    return 1;
  }

  if (!data->event_notify || !masks) {
    LogErr(ERROR_LEVEL, ER_AUDIT_PLUGIN_HAS_INVALID_DATA, plugin->name.str);
    return 1;
  }

  if (plugin->plugin->init && plugin->plugin->init(plugin)) {
    LogErr(ERROR_LEVEL, ER_PLUGIN_INIT_FAILED, plugin->name.str);
    return 1;
  }

  /* Make the interface info more easily accessible */
  plugin->data = plugin->plugin->info;

  /* Add the bits the plugin is interested in to the global mask */
  mysql_mutex_lock(&LOCK_audit_mask);
  add_audit_mask(mysql_global_audit_mask, data->class_mask);
  mysql_mutex_unlock(&LOCK_audit_mask);

  return 0;
}

/**
  Performs a bitwise OR of the installed plugins event class masks

  @param[in] plugin Source of the audit mask.
  @param[in] arg    Destination, where the audit mask is copied.

  @retval false  always
*/
static bool calc_class_mask(THD *, plugin_ref plugin, void *arg) {
  st_mysql_audit *data = plugin_data<st_mysql_audit *>(plugin);
  if (data)
    add_audit_mask(reinterpret_cast<unsigned long *>(arg), data->class_mask);
  return false;
}

/**
  Finalize an Audit plug-in

  @param[in] plugin Plugin data pointer to be deinitialized.

  @retval false  OK
  @retval true   There was an error.
*/
int finalize_audit_plugin(st_plugin_int *plugin) {
  unsigned long event_class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];

  if (plugin->plugin->deinit && plugin->plugin->deinit(plugin)) {
    DBUG_PRINT("warning", ("Plugin '%s' deinit function returned error.",
                           plugin->name.str));
    DBUG_EXECUTE("finalize_audit_plugin", return 1;);
  }

  plugin->data = nullptr;
  memset(&event_class_mask, 0, sizeof(event_class_mask));

  /* Iterate through all the installed plugins to create new mask */

  /*
    LOCK_audit_mask/LOCK_plugin order is not fixed, but serialized with table
    lock on mysql.plugin.
  */
  mysql_mutex_lock(&LOCK_audit_mask);
  plugin_foreach(current_thd, calc_class_mask, MYSQL_AUDIT_PLUGIN,
                 &event_class_mask);

  /* Set the global audit mask */
  memmove(mysql_global_audit_mask, event_class_mask, sizeof(event_class_mask));
  mysql_mutex_unlock(&LOCK_audit_mask);

  return 0;
}

/**  There's at least one active audit plugin tracking a specified class */
bool is_audit_plugin_class_active(THD *thd [[maybe_unused]],
                                  unsigned long event_class) {
  return mysql_global_audit_mask[event_class] != 0;
}

/**
  @brief Checks presence of active audit plugin

  @retval      TRUE             At least one audit plugin is present
  @retval      FALSE            No audit plugin is present
*/
bool is_global_audit_mask_set() {
  for (int i = MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE;
       i++) {
    if (mysql_global_audit_mask[i] != 0) return true;
  }
  return false;
}

size_t make_user_name(Security_context *sctx, char *buf) {
  LEX_CSTRING sctx_user = sctx->user();
  LEX_CSTRING sctx_host = sctx->host();
  LEX_CSTRING sctx_ip = sctx->ip();
  LEX_CSTRING sctx_priv_user = sctx->priv_user();
  return static_cast<size_t>(
      strxnmov(buf, MAX_USER_HOST_SIZE,
               sctx_priv_user.str[0] ? sctx_priv_user.str : "", "[",
               sctx_user.length ? sctx_user.str : "", "] @ ",
               sctx_host.length ? sctx_host.str : "", " [",
               sctx_ip.length ? sctx_ip.str : "", "]", NullS) -
      buf);
}

inline void set_cstring_with_length(mysql_cstring_with_length &cstr,
                                    const char *str) {
  cstr.str = str;
  cstr.length = str ? strlen(str) : 0;
}

int mysql_event_tracking_authentication_notify(
    THD *thd, mysql_event_tracking_authentication_subclass_t subclass,
    const char *subclass_name, int status, const char *user, const char *host,
    const char *authentication_plugin, bool is_role, const char *new_user,
    const char *new_host) {
  mysql_event_tracking_authentication_data event;

  if (thd->check_event_subscribers(Event_tracking_class::AUTHENTICATION,
                                   subclass, true))
    return 0;

  event.event_subclass = subclass;
  event.status = status;
  event.connection_id = static_cast<mysql_connection_id>(thd->thread_id());
  set_cstring_with_length(event.user, user ? user : "");
  set_cstring_with_length(event.host, host ? host : "");

  std::vector<const char *> auth_methods;
  if (authentication_plugin) auth_methods.push_back(authentication_plugin);
  Event_tracking_authentication_information authentication_information(
      subclass, auth_methods, is_role, new_user ? new_user : nullptr,
      new_host ? new_host : nullptr);

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = &authentication_information;
  event_generic.event_class = Event_tracking_class::AUTHENTICATION;

  Ignore_event_tracking_error_handler handler(thd, subclass_name);
  return handler.get_result(
      event_tracking_dispatch_error(thd, subclass_name, &event_generic));
}

int mysql_event_tracking_command_notify(
    THD *thd, mysql_event_tracking_command_subclass_t subclass,
    const char *subclass_name, enum_server_command command,
    const char *command_text) {
  mysql_event_tracking_command_data event;

  if (thd->check_event_subscribers(Event_tracking_class::COMMAND, subclass,
                                   true))
    return 0;

  event.event_subclass = subclass;
  event.status =
      thd->get_stmt_da()->is_error() ? thd->get_stmt_da()->mysql_errno() : 0;
  event.connection_id =
      thd ? static_cast<mysql_connection_id>(thd->thread_id()) : 0;

  set_cstring_with_length(event.command, command_text ? command_text : "");

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::COMMAND;

  if (event.event_subclass & EVENT_TRACKING_COMMAND_START) {
    Ignore_command_start_error_handler handler(thd, command, event.command.str);

    return handler.get_result(
        event_tracking_dispatch_error(thd, subclass_name, &event_generic));
  }

  Ignore_event_tracking_error_handler handler(thd, subclass_name);

  return handler.get_result(
      event_tracking_dispatch_error(thd, subclass_name, &event_generic));
}

int mysql_event_tracking_connection_notify(
    THD *thd, mysql_event_tracking_connection_subclass_t subclass,
    const char *subclass_name, int errcode) {
  mysql_event_tracking_connection_data event;

  /*
    Do not take into account m_auditing_activated flag. Always generate
    events of the MYSQL_AUDIT_CONNECTION_CLASS class.
  */
  if (thd->check_event_subscribers(Event_tracking_class::CONNECTION, subclass,
                                   false))
    return 0;

  event.event_subclass = subclass;
  event.status = errcode;
  event.connection_id = static_cast<mysql_connection_id>(thd->thread_id());

  auto user = thd->security_context()->user();
  auto priv_user = thd->security_context()->priv_user();
  auto external_user = thd->security_context()->external_user();
  auto proxy_user = thd->security_context()->proxy_user();
  auto host = thd->security_context()->host();
  auto ip = thd->security_context()->ip();
  auto db = thd->db();

  event.user = {user.str, user.length};
  event.priv_user = {priv_user.str, priv_user.length};
  event.external_user = {external_user.str, external_user.length};
  event.proxy_user = {proxy_user.str, proxy_user.length};
  event.host = {host.str, host.length};
  event.ip = {ip.str, ip.length};
  event.database = {db.str, db.length};
  event.connection_type = thd->get_vio_type();

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::CONNECTION;

  if (event.event_subclass & EVENT_TRACKING_CONNECTION_DISCONNECT) {
    Ignore_event_tracking_error_handler handler(thd, subclass_name);
    return handler.get_result(
        event_tracking_dispatch_error(thd, subclass_name, &event_generic));
  }

  return event_tracking_dispatch_error(thd, subclass_name, &event_generic);
}

int mysql_event_tracking_connection_notify(
    THD *thd, mysql_event_tracking_connection_subclass_t subclass,
    const char *subclass_name) {
  return mysql_event_tracking_connection_notify(
      thd, subclass, subclass_name,
      thd->get_stmt_da()->is_error() ? thd->get_stmt_da()->mysql_errno() : 0);
}

int mysql_event_tracking_general_notify(
    THD *thd, mysql_event_tracking_general_subclass_t subclass,
    const char *subclass_name, int error_code, const char *msg,
    size_t msg_len) {
  mysql_event_tracking_general_data event;
  char user_buff[MAX_USER_HOST_SIZE];

  assert(thd);

  if (thd->check_event_subscribers(Event_tracking_class::GENERAL, subclass,
                                   true))
    return 0;

  event.event_subclass = subclass;
  event.error_code = error_code;
  event.connection_id = static_cast<mysql_connection_id>(thd->thread_id());

  Security_context *sctx = thd->security_context();

  auto ip = sctx->ip();
  auto host = sctx->host();

  event.user.str = user_buff;
  event.user.length = make_user_name(sctx, user_buff);
  event.ip = {ip.str, ip.length};
  event.host = {host.str, host.length};

  Event_tracking_general_information general_information(
      subclass,
      static_cast<uint64_t>(thd->get_stmt_da()->current_row_for_condition()),
      static_cast<uint64_t>(thd->query_start_in_secs()), sctx->external_user(),
      msg, msg_len);

  DBUG_EXECUTE_IF("audit_log_negative_general_error_code",
                  event.error_code *= -1;);

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = &general_information;
  event_generic.event_class = Event_tracking_class::GENERAL;

  if (event.event_subclass &
      (EVENT_TRACKING_GENERAL_ERROR | EVENT_TRACKING_GENERAL_STATUS |
       EVENT_TRACKING_GENERAL_RESULT)) {
    Ignore_event_tracking_error_handler handler(thd, subclass_name);
    return handler.get_result(event_tracking_dispatch(thd, &event_generic));
  }

  return event_tracking_dispatch_error(thd, subclass_name, &event_generic);
}

int mysql_event_tracking_global_variable_notify(
    THD *thd, mysql_event_tracking_global_variable_subclass_t subclass,
    const char *subclass_name, const char *name, const char *value,
    const unsigned int value_length) {
  if (thd->check_event_subscribers(Event_tracking_class::GLOBAL_VARIABLE,
                                   subclass, true))
    return 0;

  mysql_event_tracking_global_variable_data event;

  event.event_subclass = subclass;
  event.connection_id = static_cast<mysql_connection_id>(thd->thread_id());
  event.sql_command = get_sql_command_string(thd->lex->sql_command);

  set_cstring_with_length(event.variable_name, name ? name : "");
  event.variable_value = {value, value_length};

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::GLOBAL_VARIABLE;

  return event_tracking_dispatch_error(thd, subclass_name, &event_generic);
}

int mysql_event_tracking_message_notify(
    THD *thd, mysql_event_tracking_message_subclass_t subclass,
    const char *subclass_name, const char *component, size_t component_length,
    const char *producer, size_t producer_length, const char *message,
    size_t message_length,
    mysql_event_tracking_message_key_value_t *key_value_map,
    size_t key_value_map_length) {
  if (thd->check_event_subscribers(Event_tracking_class::MESSAGE, subclass,
                                   true))
    return 0;

  mysql_event_tracking_message_data event;

  event.connection_id = static_cast<mysql_connection_id>(thd->thread_id());
  event.event_subclass = subclass;
  event.component = {component, component_length};
  event.producer = {producer, producer_length};
  event.message = {message, message_length};
  event.key_value_map = key_value_map;
  event.key_value_map_length = key_value_map_length;

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::MESSAGE;

  return event_tracking_dispatch_error(thd, subclass_name, &event_generic);
}

int mysql_event_tracking_parse_notify(
    THD *thd, mysql_event_tracking_parse_subclass_t subclass,
    const char *subclass_name,
    mysql_event_tracking_parse_rewrite_plugin_flag *flags,
    mysql_cstring_with_length *rewritten_query) {
  if (thd->check_event_subscribers(Event_tracking_class::PARSE, subclass, true))
    return 0;

  mysql_event_tracking_parse_data event;

  auto query = thd->query();

  event.connection_id = static_cast<mysql_connection_id>(thd->thread_id());
  event.event_subclass = subclass;
  event.flags = flags;
  event.query = {query.str, query.length};
  event.rewritten_query = rewritten_query;

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::PARSE;

  return event_tracking_dispatch_error(thd, subclass_name, &event_generic);
}

int mysql_event_tracking_query_notify(
    THD *thd, mysql_event_tracking_query_subclass_t subclass,
    const char *subclass_name) {
  mysql_event_tracking_query_data event;

  if (thd->check_event_subscribers(Event_tracking_class::QUERY, subclass, true))
    return 0;

  event.event_subclass = subclass;
  event.status =
      thd->get_stmt_da()->is_error() ? thd->get_stmt_da()->mysql_errno() : 0;
  event.connection_id = static_cast<mysql_connection_id>(thd->thread_id());

  event.sql_command = get_sql_command_string(thd->lex->sql_command);

  event.query_charset = thd_get_audit_query(thd, &event.query);
  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::QUERY;

  return event_tracking_dispatch_error(thd, subclass_name, &event_generic);
}

/**
  Call audit plugins of SERVER SHUTDOWN audit class.

  @param[in] thd           Client thread info or NULL.
  @param[in] subclass      Type of the server abort audit event.
  @param[in] subclass_name Name of the subclass
  @param[in] reason        Reason code of the shutdown.
  @param[in] exit_code     Abort exit code.

  @result Value returned is not taken into consideration by the server.
*/
int mysql_event_tracking_shutdown_notify(
    THD *thd, mysql_event_tracking_shutdown_subclass_t subclass,
    const char *subclass_name [[maybe_unused]],
    mysql_event_tracking_shutdown_reason_t reason, int exit_code) {
  mysql_event_tracking_shutdown_data event;

  if (!thd || thd->check_event_subscribers(Event_tracking_class::SHUTDOWN,
                                           subclass, true))
    return 0;

  event.event_subclass = subclass;
  event.exit_code = exit_code;
  event.reason = reason;

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::SHUTDOWN;

  return event_tracking_dispatch(thd, &event_generic);
}

int mysql_event_tracking_shutdown_notify(
    mysql_event_tracking_shutdown_subclass_t subclass,
    const char *subclass_name, mysql_event_tracking_shutdown_reason_t reason,
    int exit_code) {
  if (error_handler_hook == my_message_sql) {
    Auto_THD thd;

    return mysql_event_tracking_shutdown_notify(
        thd.thd, subclass, subclass_name, reason, exit_code);
  }

  return mysql_event_tracking_shutdown_notify(nullptr, subclass, subclass_name,
                                              reason, exit_code);
}

int mysql_event_tracking_startup_notify(
    mysql_event_tracking_startup_subclass_t subclass, const char *subclass_name,
    const char **argv, unsigned int argc) {
  mysql_event_tracking_startup_data event;
  Auto_THD thd;

  if (thd.thd->check_event_subscribers(Event_tracking_class::STARTUP, subclass,
                                       true))
    return 0;

  event.event_subclass = subclass;
  event.argv = argv;
  event.argc = argc;

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::STARTUP;

  return event_tracking_dispatch_error(thd.thd, subclass_name, &event_generic);
}

int mysql_event_tracking_stored_program_notify(
    THD *thd, mysql_event_tracking_stored_program_subclass_t subclass,
    const char *subclass_name, const char *database, const char *name,
    void *parameters) {
  mysql_event_tracking_stored_program_data event;

  if (thd->check_event_subscribers(Event_tracking_class::STORED_PROGRAM,
                                   subclass, true))
    return 0;

  event.event_subclass = subclass;
  event.connection_id = static_cast<mysql_connection_id>(thd->thread_id());

  set_cstring_with_length(event.database, database ? database : "");
  set_cstring_with_length(event.name, name ? name : "");

  event.parameters = parameters;

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::STORED_PROGRAM;

  return event_tracking_dispatch_error(thd, subclass_name, &event_generic);
}

/**
  Check whether the table access event for a specified table will
  be generated.

  Events for Views, table catogories other than 'SYSTEM' or 'USER' and
  temporary tables are not generated.

  @param thd   Thread handler
  @param table Table that is to be check.

  @retval true - generate event, otherwise not.
*/
inline bool generate_table_access_event(THD *thd, Table_ref *table) {
  /* Discard views or derived tables. */
  if (table->is_view_or_derived()) return false;

  /* TRUNCATE query on Storage Engine supporting HTON_CAN_RECREATE flag. */
  if (!table->table) return true;

  /* Do not generate events, which come from PS preparation. */
  if (thd->lex->is_ps_or_view_context_analysis()) return false;

  /* Generate event for SYSTEM and USER tables, which are not temp tables. */
  if ((table->table->s->table_category == TABLE_CATEGORY_SYSTEM ||
       table->table->s->table_category == TABLE_CATEGORY_USER ||
       table->table->s->table_category == TABLE_CATEGORY_ACL_TABLE) &&
      table->table->s->tmp_table == NO_TMP_TABLE)
    return true;

  return false;
}

/**
  Function that allows to use AUDIT_EVENT macro for setting subclass
  and subclass name values.

  @param [out] out_subclass      Subclass value pointer to be set.
  @param [out] out_subclass_name Subclass name pointer to be set.
  @param subclass                Subclass that sets out_subclass value.
  @param subclass_name           Subclass name that sets out_subclass_name.
*/
inline static void set_table_access_subclass(
    mysql_event_tracking_table_access_subclass_t *out_subclass,
    const char **out_subclass_name,
    mysql_event_tracking_table_access_subclass_t subclass,
    const char *subclass_name) {
  *out_subclass = subclass;
  *out_subclass_name = subclass_name;
}

/**
  Generate table access event for a specified table. Table is being
  verified, whether the event for this table is to be generated.

  @see generate_event

  @param thd           Current thread data.
  @param subclass      Subclass value.
  @param subclass_name Subclass name.
  @param table         Table, for which table access event is to be generated.

  @return Abort execution on 'true', otherwise continue execution.
*/
static int mysql_event_tracking_table_access_notify(
    THD *thd, mysql_event_tracking_table_access_subclass_t subclass,
    const char *subclass_name, Table_ref *table) {
  if (!generate_table_access_event(thd, table) ||
      thd->check_event_subscribers(Event_tracking_class::TABLE_ACCESS, subclass,
                                   true))
    return 0;

  mysql_event_tracking_table_access_data event;

  event.event_subclass = subclass;
  event.connection_id = static_cast<mysql_connection_id>(thd->thread_id());

  event.table_database = {table->db, table->db_length};
  event.table_name = {table->table_name, table->table_name_length};

  struct st_mysql_event_generic event_generic;
  event_generic.event = &event;
  event_generic.event_information = nullptr;
  event_generic.event_class = Event_tracking_class::TABLE_ACCESS;

  return event_tracking_dispatch_error(thd, subclass_name, &event_generic);
}

int mysql_event_tracking_table_access_notify(THD *thd, Table_ref *table) {
  mysql_event_tracking_table_access_subclass_t subclass;
  const char *subclass_name;
  int ret;

  /* Do not generate events for non query table access. */
  if (!thd->lex->query_tables) return 0;

  switch (thd->lex->sql_command) {
    case SQLCOM_REPLACE_SELECT:
    case SQLCOM_INSERT_SELECT: {
      /*
        INSERT/REPLACE SELECT generates Insert event for the first table in the
        list and Read for remaining tables.
      */
      set_table_access_subclass(
          &subclass, &subclass_name,
          AUDIT_EVENT(EVENT_TRACKING_TABLE_ACCESS_INSERT));

      if ((ret = mysql_event_tracking_table_access_notify(
               thd, subclass, subclass_name, table)))
        return ret;

      /* Skip this table (event already generated). */
      table = table->next_global;

      set_table_access_subclass(&subclass, &subclass_name,
                                AUDIT_EVENT(EVENT_TRACKING_TABLE_ACCESS_READ));
      break;
    }
    case SQLCOM_INSERT:
    case SQLCOM_REPLACE:
    case SQLCOM_LOAD:
      set_table_access_subclass(
          &subclass, &subclass_name,
          AUDIT_EVENT(EVENT_TRACKING_TABLE_ACCESS_INSERT));
      break;
    case SQLCOM_DELETE:
    case SQLCOM_DELETE_MULTI:
    case SQLCOM_TRUNCATE:
      set_table_access_subclass(
          &subclass, &subclass_name,
          AUDIT_EVENT(EVENT_TRACKING_TABLE_ACCESS_DELETE));
      break;
    case SQLCOM_UPDATE:
    case SQLCOM_UPDATE_MULTI:
      /* Update state is taken from the table instance in the
         mysql_audit_notify function. */
      set_table_access_subclass(
          &subclass, &subclass_name,
          AUDIT_EVENT(EVENT_TRACKING_TABLE_ACCESS_UPDATE));
      break;
    case SQLCOM_SELECT:
    case SQLCOM_HA_READ:
    case SQLCOM_ANALYZE:
      set_table_access_subclass(&subclass, &subclass_name,
                                AUDIT_EVENT(EVENT_TRACKING_TABLE_ACCESS_READ));
      break;
    default:
      /* Do not generate event for not supported command. */
      return 0;
  }

  for (; table; table = table->next_global) {
    /*
      Do not generate audit logs for opening DD tables when processing I_S
      queries.
    */
    if (table->referencing_view && table->referencing_view->is_system_view)
      continue;

    /*
      Update-Multi query can have several updatable tables as well as readable
      tables. This is taken from table->updating field, which holds info,
      whether table is being updated or not. table->updating holds invalid
      info, when the updatable table is referenced by a view. View status is
      taken into account in that case.
    */
    if (subclass == EVENT_TRACKING_TABLE_ACCESS_UPDATE &&
        !table->referencing_view && !table->updating)
      set_table_access_subclass(&subclass, &subclass_name,
                                AUDIT_EVENT(EVENT_TRACKING_TABLE_ACCESS_READ));

    if ((ret = mysql_event_tracking_table_access_notify(thd, subclass,
                                                        subclass_name, table)))
      return ret;
  }

  return 0;
}

/*
Function commented out. No Audit API calls yet.

int mysql_audit_notify(THD *thd, mysql_event_authorization_subclass_t subclass,
                       const char *subclass_name,
                       const char *database, const char *table,
                       const char *object)
{
  mysql_event_authorization event;

  mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_AUTHORIZATION_CLASS,
                              static_cast<unsigned long>(subclass));

  event.event_subclass= subclass;
  event.connection_id= thd->thread_id();
  event.sql_command_id= thd->lex->sql_command;

  event.query_charset = thd_get_audit_query(thd, &event.query);

  LEX_CSTRING obj_str;

  lex_cstring_set(&obj_str, database ? database : "");
  event.database.str= database;
  event.database.length= obj_str.length;

  lex_cstring_set(&obj_str, table ? table : "");
  event.table.str= table;
  event.table.length = obj_str.length;

  lex_cstring_set(&obj_str, object ? object : "");
  event.object.str= object;
  event.object.length= obj_str.length;

  return event_class_dispatch_error(thd, MYSQL_AUDIT_AUTHORIZATION_CLASS,
                                    subclass_name, &event);
}
*/
