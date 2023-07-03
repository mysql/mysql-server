/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include "plugin/connection_control/connection_delay.h"

#include <mysql/psi/mysql_thread.h>
#include <time.h>

#include "m_ctype.h" /* my_charset_bin */
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_systime.h"
#include "mysqld_error.h"
#include "plugin/connection_control/connection_control.h"
#include "plugin/connection_control/security_context_wrapper.h"
#include "sql/current_thd.h" /* current_thd */
#include "sql/item_cmpfunc.h"
#include "sql/sql_class.h" /* THD, Security context */

/* Forward declaration */
bool schema_table_store_record(THD *thd, TABLE *table);

void thd_enter_cond(void *opaque_thd, mysql_cond_t *cond, mysql_mutex_t *mutex,
                    const PSI_stage_info *stage, PSI_stage_info *old_stage,
                    const char *src_function, const char *src_file,
                    int src_line);

void thd_exit_cond(void *opaque_thd, const PSI_stage_info *stage,
                   const char *src_function, const char *src_file,
                   int src_line);

struct st_mysql_information_schema connection_control_failed_attempts_view = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};

/** Fields of information_schema.connection_control_failed_attempts */
static ST_FIELD_INFO failed_attempts_view_fields[] = {
    {"USERHOST", USERNAME_LENGTH + HOSTNAME_LENGTH + 6, MYSQL_TYPE_STRING, 0,
     MY_I_S_UNSIGNED, nullptr, 0},
    {"FAILED_ATTEMPTS", 16, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {nullptr, 0, MYSQL_TYPE_NULL, 0, 0, nullptr, 0}};

namespace connection_control {

/** constants/variables declared in connection_delay_interfaces.h */

int64 DEFAULT_THRESHOLD = 3;
int64 MIN_THRESHOLD = 0;
int64 DISABLE_THRESHOLD = 0;
int64 MAX_THRESHOLD = INT_MAX32;

int64 DEFAULT_MAX_DELAY = INT_MAX32;
int64 DEFAULT_MIN_DELAY = 1000;
int64 MIN_DELAY = 1000;
int64 MAX_DELAY = INT_MAX32;

/** variables used by connection_delay.cc */
static mysql_rwlock_t connection_event_delay_lock;

static opt_connection_control opt_enums[] = {OPT_FAILED_CONNECTIONS_THRESHOLD,
                                             OPT_MIN_CONNECTION_DELAY,
                                             OPT_MAX_CONNECTION_DELAY};
size_t opt_enums_size = 3;

static stats_connection_control status_vars_enums[] = {
    STAT_CONNECTION_DELAY_TRIGGERED};
size_t status_vars_enums_size = 1;

static Connection_delay_action *g_max_failed_connection_handler = nullptr;

Sql_string I_S_CONNECTION_CONTROL_FAILED_ATTEMPTS_USERHOST(
    "information_schema.connection_control_failed_login_attempts.userhost");

/**
  Helper function for Connection_delay_event::reset_all

  @returns 1 to indicate that entry is a match
*/
int match_all_entries(const uchar *, void *) { return 1; }

/**
  Callback function for LF hash to get key information

  Returns a pointer to a buffer which represent a key in the hash.
  The function does NOT calculate a hash.
  The function is called during lf_hash_insert(). The buffer is
  fed to an internal calc_hash() which use the defined charset to
  calculate a hash from the key buffer (in most cases a murmur)

  @param [in] el        Pointer to an element in the hash
  @param [out] length   The length of the key belonging to the element

  @returns Pointer to key buffer
*/

const uchar *connection_delay_event_hash_key(const uchar *el, size_t *length) {
  const Connection_event_record *const *entry;
  const Connection_event_record *entry_info;
  entry = reinterpret_cast<const Connection_event_record *const *>(el);
  assert(entry != nullptr);
  entry_info = *entry;
  *length = entry_info->get_length();
  return (const_cast<uchar *>(entry_info->get_userhost()));
}

/**
  Constructor for Connection_delay_event

  Initialize LF hash.
*/

Connection_delay_event::Connection_delay_event() {
  lf_hash_init(&m_entries, sizeof(Connection_event_record **), LF_HASH_UNIQUE,
               0, /* key offset */
               0, /* key length not used */
               connection_delay_event_hash_key, &my_charset_bin);
}

/**
  Creates or updates an entry in hash

  @param [in] s    User information in '<user'@'<host>' format

  @returns status of insertion/update
    @retval false  Insertion/Update successful
    @retval true   Failed to insert/update an entry
*/

bool Connection_delay_event::create_or_update_entry(const Sql_string &s) {
  Connection_event_record **searched_entry = nullptr;
  Connection_event_record *searched_entry_info = nullptr;
  Connection_event_record *new_entry = nullptr;
  int insert_status;
  DBUG_TRACE;

  LF_PINS *pins = lf_hash_get_pins(&m_entries);
  if (unlikely(pins == nullptr)) return true;

  searched_entry = reinterpret_cast<Connection_event_record **>(
      lf_hash_search(&m_entries, pins, s.c_str(), s.length()));

  if (searched_entry && (searched_entry != MY_LF_ERRPTR)) {
    /* We found an entry, so increment the count */
    searched_entry_info = *searched_entry;
    assert(searched_entry_info != nullptr);
    searched_entry_info->inc_count();
    lf_hash_search_unpin(pins);
    lf_hash_put_pins(pins);
    return false;
  } else {
    /* No entry found, so try to add new entry */
    lf_hash_search_unpin(pins);
    new_entry = new Connection_event_record(s);

    insert_status = lf_hash_insert(&m_entries, pins, &new_entry);

    if (likely(insert_status == 0)) {
      lf_hash_put_pins(pins);
      return false;
    } else {
      /*
        OOM. We are likely in bigger trouble than just
        failing to insert an entry in hash.
      */
      lf_hash_put_pins(pins);
      delete new_entry;
      new_entry = nullptr;
      return true;
    }
  }
}

/**
  Resets count stored against given user entry

  @param [in] s    User information in '<user'@'<host>' format

  @returns status of reset operation
    @retval false Reset successful
    @retval true Failed to find given entry
*/

bool Connection_delay_event::remove_entry(const Sql_string &s) {
  Connection_event_record **searched_entry = nullptr;
  Connection_event_record *searched_entry_info = nullptr;
  DBUG_TRACE;

  LF_PINS *pins = lf_hash_get_pins(&m_entries);

  searched_entry = reinterpret_cast<Connection_event_record **>(
      lf_hash_search(&m_entries, pins, s.c_str(), s.length()));

  if (searched_entry && searched_entry != MY_LF_ERRPTR) {
    searched_entry_info = *searched_entry;
    assert(searched_entry_info != nullptr);
    int rc = lf_hash_delete(&m_entries, pins, s.c_str(), s.length());
    lf_hash_search_unpin(pins);
    lf_hash_put_pins(pins);
    if (rc == 0) {
      /* free memory upon successful deletion */
      delete searched_entry_info;
    }
    return rc != 0;
  } else {
    /* No entry found. */
    lf_hash_search_unpin(pins);
    lf_hash_put_pins(pins);
    return true;
  }
}

/**
  Retrieve stored value for given user entry

  @param [in] s        User information in '<user'@'<host>' format
  @param value [out]   Buffer to hold value stored against given user

  @returns whether given entry is present in hash or not
    @retval false  Entry found. Corresponding value is copied in value buffer.
    @retval true   No matching entry found. value buffer should not be used.
*/

bool Connection_delay_event::match_entry(const Sql_string &s, void *value) {
  Connection_event_record **searched_entry = nullptr;
  Connection_event_record *searched_entry_info = nullptr;
  int64 count = DISABLE_THRESHOLD;
  bool error = true;
  DBUG_TRACE;

  LF_PINS *pins = lf_hash_get_pins(&m_entries);

  searched_entry = reinterpret_cast<Connection_event_record **>(
      lf_hash_search(&m_entries, pins, s.c_str(), s.length()));

  if (searched_entry && searched_entry != MY_LF_ERRPTR) {
    searched_entry_info = *searched_entry;
    count = searched_entry_info->get_count();
    error = false;
  }

  lf_hash_search_unpin(pins);
  lf_hash_put_pins(pins);
  *(reinterpret_cast<int64 *>(value)) = count;

  return error;
}

/**
  Delete all entries from hash and free memory
*/

void Connection_delay_event::reset_all() {
  Connection_event_record **searched_entry = nullptr;
  DBUG_TRACE;
  LF_PINS *pins = lf_hash_get_pins(&m_entries);

  do {
    /* match anything */
    searched_entry = reinterpret_cast<Connection_event_record **>(
        lf_hash_random_match(&m_entries, pins, match_all_entries, 0, nullptr));

    if (searched_entry != nullptr && searched_entry != MY_LF_ERRPTR &&
        (*searched_entry) &&
        !lf_hash_delete(&m_entries, pins, (*searched_entry)->get_userhost(),
                        (*searched_entry)->get_length())) {
      delete (*searched_entry);
      *searched_entry = nullptr;
    } else {
      /* Failed to delete didn't remove any pins */
      lf_hash_search_unpin(pins);
    }
  } while (searched_entry != nullptr);

  lf_hash_put_pins(pins);
}

/** This works because connction_delay_IS_table is protected by wrlock */
static TABLE *connection_delay_IS_table;
void set_connection_delay_IS_table(TABLE *t) { connection_delay_IS_table = t; }

/**
  Function to populate information_schema view.

  @param [in] ptr  Entry from LF hash

  @returns status of row insertion
    @retval 0 Success
    @retval 1 Error
*/

int connection_delay_IS_table_writer(const uchar *ptr,
                                     void *arg [[maybe_unused]]) {
  /* Always return "no match" so that we go through all entries */
  THD *thd = current_thd;
  const Connection_event_record *const *entry;
  const Connection_event_record *entry_info;
  entry = reinterpret_cast<const Connection_event_record *const *>(ptr);
  entry_info = *entry;
  connection_delay_IS_table->field[0]->store((char *)entry_info->get_userhost(),
                                             entry_info->get_length(),
                                             system_charset_info);
  connection_delay_IS_table->field[1]->store(entry_info->get_count(), true);
  if (schema_table_store_record(thd, connection_delay_IS_table)) return 1;
  /* Always return "no match" so that we go over all entries in the hash */
  return 0;
}

/**
  Function to dump LF hash data to IS table.

  @param [in] tables Handle to
                     information_schema.connection_control_failed_attempts
*/

void Connection_delay_event::fill_IS_table(Table_ref *tables) {
  DBUG_TRACE;
  TABLE *table = tables->table;
  set_connection_delay_IS_table(table);
  LF_PINS *pins = lf_hash_get_pins(&m_entries);
  void *key = nullptr;

  do {
    key =
        lf_hash_random_match(&m_entries, pins,
                             /* Functor: match anything and store the fields */
                             connection_delay_IS_table_writer, 0, nullptr);
    /* Always unpin after lf_hash_random_match() */
    lf_hash_search_unpin(pins);
  } while (key != nullptr);

  lf_hash_put_pins(pins);
}

/**
  Connection_delay_action Constructor.

  @param [in] threshold         Defines a threshold after which wait is
  triggered
  @param [in] min_delay         Lower cap on wait
  @param [in] max_delay         Upper cap on wait
  @param [in] sys_vars          System variables
  @param [in] sys_vars_size     Size of sys_vars array
  @param [in] status_vars       Status variables
  @param [in] status_vars_size  Size of status_vars array
  @param [in] lock              RW lock handle
*/

Connection_delay_action::Connection_delay_action(
    int64 threshold, int64 min_delay, int64 max_delay,
    opt_connection_control *sys_vars, size_t sys_vars_size,
    stats_connection_control *status_vars, size_t status_vars_size,
    mysql_rwlock_t *lock)
    : m_threshold(threshold),
      m_min_delay(min_delay),
      m_max_delay(max_delay),
      m_lock(lock) {
  if (sys_vars_size) {
    for (uint i = 0; i < sys_vars_size; ++i) m_sys_vars.push_back(sys_vars[i]);
  }

  if (status_vars_size) {
    for (uint i = 0; i < status_vars_size; ++i)
      m_stats_vars.push_back(status_vars[i]);
  }
}

/**
  Create hash key of the format '<user>'@'<host>'.
  Policy:
  1. Use proxy_user information if available. Else if,
  2. Use priv_user/priv_host if either of them is not empty. Else,
  3. Use user/host

  @param [in] thd        THD pointer for getting security context
  @param [out] s         Hash key is stored here
*/

void Connection_delay_action::make_hash_key(MYSQL_THD thd, Sql_string &s) {
  /* Our key for hash will be of format : '<user>'@'<host>' */

  /* If proxy_user is set then use it directly for lookup */
  Security_context_wrapper sctx_wrapper(thd);
  const char *proxy_user = sctx_wrapper.get_proxy_user();
  if (proxy_user && *proxy_user) {
    s.append(proxy_user);
  } /* else if priv_user and/or priv_host is set, then use them */
  else {
    const char *priv_user = sctx_wrapper.get_priv_user();
    const char *priv_host = sctx_wrapper.get_priv_host();
    if (*priv_user || *priv_host) {
      s.append("'");

      if (*priv_user) s.append(priv_user);

      s.append("'@'");

      if (*priv_host) s.append(priv_host);

      s.append("'");
    } else {
      const char *user = sctx_wrapper.get_user();
      const char *host = sctx_wrapper.get_host();
      const char *ip = sctx_wrapper.get_ip();

      s.append("'");

      if (user && *user) s.append(user);

      s.append("'@'");

      if (host && *host)
        s.append(host);
      else if (ip && *ip)
        s.append(ip);

      s.append("'");
    }
  }
}

/**
  Wait till the wait_time expires or thread is killed

  @param [in] thd        Handle to MYSQL_THD object
  @param [in] wait_time  Maximum time to wait
*/

void Connection_delay_action::conditional_wait(MYSQL_THD thd,
                                               ulonglong wait_time) {
  DBUG_TRACE;

  /** mysql_cond_timedwait requires wait time in timespec format */
  struct timespec abstime;
  /** Since we get wait_time in milliseconds, convert it to nanoseconds */
  set_timespec_nsec(&abstime, wait_time * 1000000ULL);

  /** PSI_stage_info for thd_enter_cond/thd_exit_cond */
  PSI_stage_info old_stage;

  /** Initialize mutex required for mysql_cond_timedwait */
  mysql_mutex_t connection_delay_mutex;
  mysql_mutex_init(key_connection_delay_mutex, &connection_delay_mutex,
                   MY_MUTEX_INIT_FAST);

  /* Initialize condition to wait for */
  mysql_cond_t connection_delay_wait_condition;
  mysql_cond_init(key_connection_delay_wait, &connection_delay_wait_condition);

  /** Register wait condition with THD */
  mysql_mutex_lock(&connection_delay_mutex);
  thd_enter_cond(thd, &connection_delay_wait_condition, &connection_delay_mutex,
                 &stage_waiting_in_connection_control_plugin, &old_stage,
                 __func__, __FILE__, __LINE__);

  /*
    At this point, thread is essentially going to sleep till
    timeout. If admin issues KILL statement for this THD,
    there is no point keeping this thread in sleep mode only
    to wake up to be terminated. Hence, in case of KILL,
    we will return control to server without worring about
    wait_time.
  */
  mysql_cond_timedwait(&connection_delay_wait_condition,
                       &connection_delay_mutex, &abstime);

  /* Finish waiting and deregister wait condition */
  mysql_mutex_unlock(&connection_delay_mutex);
  thd_exit_cond(thd, &stage_waiting_in_connection_control_plugin, __func__,
                __FILE__, __LINE__);

  /* Cleanup */
  mysql_mutex_destroy(&connection_delay_mutex);
  mysql_cond_destroy(&connection_delay_wait_condition);
}

/**
  @brief  Handle a connection event and, if required,
  wait for random amount of time before returning.

  We only care about CONNECT and CHANGE_USER sub events.

  @param [in] thd                THD pointer
  @param [in] coordinator        Connection_event_coordinator
  @param [in] connection_event   Connection event to be handled
  @param [in] error_handler      Error handler object

  @returns status of connection event handling
    @retval false  Successfully handled an event.
    @retval true   Something went wrong.
                   error_buffer may contain details.
*/

bool Connection_delay_action::notify_event(
    MYSQL_THD thd, Connection_event_coordinator_services *coordinator,
    const mysql_event_connection *connection_event,
    Error_handler *error_handler) {
  DBUG_TRACE;
  bool error = false;
  unsigned int subclass = connection_event->event_subclass;
  Connection_event_observer *self = this;

  if (subclass != MYSQL_AUDIT_CONNECTION_CONNECT &&
      subclass != MYSQL_AUDIT_CONNECTION_CHANGE_USER)
    return error;

  RD_lock rd_lock(m_lock);

  int64 threshold = this->get_threshold();

  /* If feature was disabled, return */
  if (threshold <= DISABLE_THRESHOLD) return error;

  int64 current_count = 0;
  bool user_present = false;
  Sql_string userhost;

  make_hash_key(thd, userhost);

  DBUG_PRINT("info", ("Connection control : Connection event lookup for: %s",
                      userhost.c_str()));

  /* Cache current failure count */
  user_present = m_userhost_hash.match_entry(userhost, (void *)&current_count)
                     ? false
                     : true;

  if (current_count >= threshold || current_count < 0) {
    /*
      If threshold is crosed, regardless of connection success
      or failure, wait for (current_count + 1) - threshold seconds
      Note that current_count is not yet updated in hash. So we
      have to consider current connection as well - Hence the usage
      of current_count + 1.
    */
    ulonglong wait_time = get_wait_time((current_count + 1) - threshold);

    if ((error = coordinator->notify_status_var(
             &self, STAT_CONNECTION_DELAY_TRIGGERED, ACTION_INC))) {
      error_handler->handle_error(
          ER_CONN_CONTROL_STAT_CONN_DELAY_TRIGGERED_UPDATE_FAILED);
    }
    /*
      Invoking sleep while holding read lock on Connection_delay_action
      would block access to cache data through IS table.
    */
    rd_lock.unlock();
    conditional_wait(thd, wait_time);
    rd_lock.lock();
  }

  if (connection_event->status) {
    /*
      Connection failure.
      Add new entry to hash or increment failed connection count
      for an existing entry
    */
    if (m_userhost_hash.create_or_update_entry(userhost)) {
      error_handler->handle_error(
          ER_CONN_CONTROL_FAILED_TO_UPDATE_CONN_DELAY_HASH, userhost.c_str());
      error = true;
    }
  } else {
    /*
      Successful connection.
      delete entry for given account from the hash
    */
    if (user_present) {
      (void)m_userhost_hash.remove_entry(userhost);
    }
  }

  return error;
}

/**
  Notification of a change in system variable value

  @param [in] coordinator        Handle to coordinator
  @param [in] variable           Enum of variable
  @param [in] new_value          New value for variable
  @param [in] error_handler      Error handler object

  @returns processing status
    @retval false  Change in variable value processed successfully
    @retval true Error processing new value.
                  error_buffer may contain more details.
*/

bool Connection_delay_action::notify_sys_var(
    Connection_event_coordinator_services *coordinator,
    opt_connection_control variable, void *new_value,
    Error_handler *error_handler) {
  DBUG_TRACE;
  bool error = true;
  Connection_event_observer *self = this;

  WR_lock wr_lock(m_lock);

  switch (variable) {
    case OPT_FAILED_CONNECTIONS_THRESHOLD: {
      int64 new_threshold = *(static_cast<int64 *>(new_value));
      assert(new_threshold >= DISABLE_THRESHOLD);
      set_threshold(new_threshold);

      if ((error = coordinator->notify_status_var(
               &self, STAT_CONNECTION_DELAY_TRIGGERED, ACTION_RESET))) {
        error_handler->handle_error(
            ER_CONN_CONTROL_STAT_CONN_DELAY_TRIGGERED_RESET_FAILED);
      }
      break;
    }
    case OPT_MIN_CONNECTION_DELAY:
    case OPT_MAX_CONNECTION_DELAY: {
      int64 new_delay = *(static_cast<int64 *>(new_value));
      if ((error =
               set_delay(new_delay, (variable == OPT_MIN_CONNECTION_DELAY)))) {
        error_handler->handle_error(
            ER_CONN_CONTROL_FAILED_TO_SET_CONN_DELAY,
            (variable == OPT_MIN_CONNECTION_DELAY) ? "min" : "max");
      }
      break;
    }
    default:
      /* Should never reach here. */
      assert(false);
      error_handler->handle_error(ER_CONN_CONTROL_INVALID_CONN_DELAY_TYPE);
  };
  return error;
}

/**
  Subscribe with coordinator for connection events

  @param [in] coordinator  Handle to Connection_event_coordinator_services
                           for registration
*/
void Connection_delay_action::init(
    Connection_event_coordinator_services *coordinator) {
  DBUG_TRACE;
  assert(coordinator);
  bool retval;
  Connection_event_observer *subscriber = this;
  WR_lock wr_lock(m_lock);
  retval = coordinator->register_event_subscriber(&subscriber, &m_sys_vars,
                                                  &m_stats_vars);
  assert(!retval);
  if (retval) retval = false; /* Make compiler happy */
}

/**
  Clear data from Connection_delay_action
*/

void Connection_delay_action::deinit() {
  mysql_rwlock_wrlock(m_lock);
  m_userhost_hash.reset_all();
  m_sys_vars.clear();
  m_stats_vars.clear();
  m_threshold = DISABLE_THRESHOLD;
  mysql_rwlock_unlock(m_lock);
  m_lock = nullptr;
}

/**
  Get user information from "where userhost = <value>"

  @param [in] cond        Equality condition structure
  @param [out] eq_arg     Sql_string handle to store user information
  @param [in] field_name  userhost field

  @returns whether a value was found or not
    @retval false Found a value. Check eq_arg
    @retval true Error.
*/

static bool get_equal_condition_argument(Item *cond, Sql_string *eq_arg,
                                         const Sql_string &field_name) {
  if (cond != nullptr && cond->type() == Item::FUNC_ITEM) {
    Item_func *func = static_cast<Item_func *>(cond);
    if (func != nullptr && func->functype() == Item_func::EQ_FUNC) {
      Item_func_eq *eq_func = static_cast<Item_func_eq *>(func);
      if (eq_func->arguments()[0]->type() == Item::FIELD_ITEM &&
          my_strcasecmp(system_charset_info,
                        eq_func->arguments()[0]->full_name(),
                        field_name.c_str()) == 0) {
        char buff[1024];
        String *res;
        String filter(buff, sizeof(buff), system_charset_info);
        if (eq_func->arguments()[1] != nullptr &&
            (res = eq_func->arguments()[1]->val_str(&filter))) {
          eq_arg->append(res->c_ptr_safe(), res->length());
          return false;
        }
      }
    }
  }
  return true;
}

/**
  Function to fill information_schema.connection_control_failed_attempts.

  Handles query with equality condition.
  For full scan, calls Connection_delay_event::fill_IS_table()

  Permission : SUPER_ACL is required.

  @param [in] thd     THD handle.
  @param [in] tables  Handle to
                      information_schema.connection_control_failed_attempts.
  @param [in] cond    Condition if any.
*/

void Connection_delay_action::fill_IS_table(THD *thd, Table_ref *tables,
                                            Item *cond) {
  DBUG_TRACE;
  Security_context_wrapper sctx_wrapper(thd);
  if (!(sctx_wrapper.is_super_user() || sctx_wrapper.is_connection_admin()))
    return;
  WR_lock wr_lock(m_lock);
  Sql_string eq_arg;
  if (cond != nullptr &&
      !get_equal_condition_argument(
          cond, &eq_arg, I_S_CONNECTION_CONTROL_FAILED_ATTEMPTS_USERHOST)) {
    int64 current_count = 0;
    if (m_userhost_hash.match_entry(eq_arg, (void *)&current_count)) {
      /* There are no matches given the condition */
      return;
    } else {
      /* There is exactly one matching userhost entry */
      TABLE *table = tables->table;
      table->field[0]->store(eq_arg.c_str(), eq_arg.length(),
                             system_charset_info);
      table->field[1]->store(current_count, true);
      schema_table_store_record(thd, table);
    }
  } else
    m_userhost_hash.fill_IS_table(tables);
}

/**
  Initializes required objects for handling connection events.

  @param [in] coordinator    Connection_event_coordinator_services handle.
*/

bool init_connection_delay_event(
    Connection_event_coordinator_services *coordinator,
    Error_handler *error_handler) {
  /*
    1. Initialize lock(s)
  */
  mysql_rwlock_init(key_connection_event_delay_lock,
                    &connection_event_delay_lock);
  g_max_failed_connection_handler = new Connection_delay_action(
      g_variables.failed_connections_threshold,
      g_variables.min_connection_delay, g_variables.max_connection_delay,
      opt_enums, opt_enums_size, status_vars_enums, status_vars_enums_size,
      &connection_event_delay_lock);
  if (!g_max_failed_connection_handler) {
    error_handler->handle_error(ER_CONN_CONTROL_DELAY_ACTION_INIT_FAILED);
    return true;
  }
  g_max_failed_connection_handler->init(coordinator);

  return false;
}

/**
  Deinitializes objects and frees associated memory.
*/

void deinit_connection_delay_event() {
  if (g_max_failed_connection_handler) delete g_max_failed_connection_handler;
  g_max_failed_connection_handler = nullptr;
  mysql_rwlock_destroy(&connection_event_delay_lock);
  return;
}
}  // namespace connection_control

/**
  Function to fill information_schema.connection_control_failed_attempts.

  @param [in] thd     THD handle.
  @param [in] tables  Handle to
                      information_schema.connection_control_failed_attempts.
  @param [in] cond    Condition if any.

  @returns Always returns false.
*/

int fill_failed_attempts_view(THD *thd, Table_ref *tables, Item *cond) {
  if (connection_control::g_max_failed_connection_handler)
    connection_control::g_max_failed_connection_handler->fill_IS_table(
        thd, tables, cond);
  return false;
}

/**
  View init function

  @param [in] ptr    Handle to
                     information_schema.connection_control_failed_attempts.

  @returns Always returns 0.
*/

int connection_control_failed_attempts_view_init(void *ptr) {
  ST_SCHEMA_TABLE *schema_table = (ST_SCHEMA_TABLE *)ptr;

  schema_table->fields_info = failed_attempts_view_fields;
  schema_table->fill_table = fill_failed_attempts_view;
  return 0;
}
