/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "connection_delay.h"

#include <mysql/components/services/mysql_cond.h>
#include <mysql/components/services/mysql_mutex.h>
#include <mysqld_error.h>
#include "connection_control_coordinator.h"
#include "failed_attempts_list_imp.h"
#include "my_systime.h"
#include "option_usage.h"
#include "security_context_wrapper.h"

namespace connection_control {

/** constants/variables declared in connection_delay_interfaces.h */

const int64 MIN_THRESHOLD = 0;
const int64 DISABLE_THRESHOLD = 0;
const int64 MAX_THRESHOLD = INT_MAX32;

const int64 MIN_DELAY = 1000;
const int64 MAX_DELAY = INT_MAX32;

/** variables used by connection_delay.cc */
static mysql_rwlock_t connection_event_delay_lock;

static opt_connection_control opt_enums[] = {
    OPT_FAILED_CONNECTIONS_THRESHOLD, OPT_MIN_CONNECTION_DELAY,
    OPT_MAX_CONNECTION_DELAY, OPT_EXEMPT_UNKNOWN_USERS};
static const size_t opt_enums_size = std::size(opt_enums);

static stats_connection_control status_vars_enums[] = {
    STAT_CONNECTION_DELAY_TRIGGERED, STAT_CONNECTION_EXEMPTED_USERS};
static const size_t status_vars_enums_size = std::size(status_vars_enums);

static Connection_delay_action *g_max_failed_connection_handler = nullptr;

/**
  Connection_delay_action Constructor.

  @param [in] threshold         Defines a threshold after which wait is
  triggered
  @param [in] min_delay         Lower cap on wait
  @param [in] max_delay         Upper cap on wait
  @param [in] exempt_unknown_users Exempt unauthenticated sessions
  @param [in] sys_vars          System variables
  @param [in] sys_vars_size     Size of sys_vars array
  @param [in] status_vars       Status variables
  @param [in] status_vars_size  Size of status_vars array
  @param [in] lock              RW lock handle
*/

Connection_delay_action::Connection_delay_action(
    int64 threshold, int64 min_delay, int64 max_delay,
    bool exempt_unknown_users, opt_connection_control *sys_vars,
    size_t sys_vars_size, stats_connection_control *status_vars,
    size_t status_vars_size, mysql_rwlock_t *lock)
    : m_threshold(threshold),
      m_min_delay(min_delay),
      m_max_delay(max_delay),
      m_exempt_unknown_users(exempt_unknown_users),
      m_lock(lock) {
  for (uint i = 0; i < sys_vars_size; ++i) {
    m_sys_vars.push_back(sys_vars[i]);
  }
  for (uint i = 0; i < status_vars_size; ++i) {
    m_stats_vars.push_back(status_vars[i]);
  }
}

/**
  Create hash key of the format 'user'@'host'.
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
  if (proxy_user != nullptr && *proxy_user != 0) {
    s.append(proxy_user);
  } /* else if priv_user and/or priv_host is set, then use them */
  else {
    const char *priv_user = sctx_wrapper.get_priv_user();
    const char *priv_host = sctx_wrapper.get_priv_host();
    if ((priv_user != nullptr && *priv_user != 0) ||
        (priv_host != nullptr && *priv_host != 0)) {
      s.append("'");

      if (priv_user != nullptr && *priv_user != 0) {
        s.append(priv_user);
      }

      s.append("'@'");

      if (priv_host != nullptr && *priv_host != 0) {
        s.append(priv_host);
      }

      s.append("'");
    } else {
      const char *user = sctx_wrapper.get_user();
      const char *host = sctx_wrapper.get_host();
      const char *ip = sctx_wrapper.get_ip();

      s.append("'");

      if (user != nullptr && *user != 0) {
        s.append(user);
      }

      s.append("'@'");

      if (host != nullptr && *host != 0) {
        s.append(host);
      } else if (ip != nullptr && *ip != 0) {
        s.append(ip);
      }

      s.append("'");
    }
  }
}

/**
  Wait till the wait_time expires or thread is killed

  @param [in] wait_time  Maximum time to wait in msec
*/

void Connection_delay_action::conditional_wait(ulonglong wait_time) {
  /** mysql_cond_timedwait requires wait time in timespec format */
  timespec abstime;

  Timeout_type const nsec = wait_time * 1000000ULL;
  assert(nsec != std::numeric_limits<Timeout_type>::max());
  if (nsec == TIMEOUT_INF) {
    abstime = TIMESPEC_POSINF;
    return;
  }
  const unsigned long long int now = my_getsystime() + (nsec / 100);
  unsigned long long int const tv_sec = now / 10000000ULL;
#if SIZEOF_TIME_T < SIZEOF_LONG_LONG
  /* Ensure that the number of seconds don't overflow. */
  tv_sec = std::min(tv_sec, static_cast<unsigned long long int>(
                                std::numeric_limits<time_t>::max()));
#endif
  abstime.tv_sec = static_cast<time_t>(tv_sec);
  abstime.tv_nsec = (now % 10000000ULL) * 100 + (nsec % 100);

  /** Initialize mutex required for mysql_cond_timedwait */
  mysql_mutex_t connection_delay_mutex;
  mysql_mutex_init(key_connection_delay_mutex, &connection_delay_mutex,
                   nullptr);

  /* Initialize condition to wait for */
  mysql_cond_t connection_delay_wait_condition;
  mysql_cond_init(key_connection_delay_wait, &connection_delay_wait_condition);

  /** Register wait condition with THD */
  mysql_mutex_lock(&connection_delay_mutex);

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

  /* Cleanup */
  mysql_mutex_destroy(&connection_delay_mutex);
  mysql_cond_destroy(&connection_delay_wait_condition);
}

static bool is_unauthenticated_connection(MYSQL_THD thd) {
  Security_context_wrapper sctx_wrapper(thd);

  const char *proxy_user = sctx_wrapper.get_proxy_user();
  if (nullptr != proxy_user && *proxy_user != '\0') {
    return false;
  }

  const char *priv_user = sctx_wrapper.get_priv_user();
  if (nullptr != priv_user && *priv_user != '\0') {
    return false;
  }

  const char *user = sctx_wrapper.get_user();
  if (nullptr != user && *user != '\0') {
    return false;
  }

  // no user supplied, unauthenticated
  return true;
}

/**
  @brief  Handle a connection event and, if required,
  wait for random amount of time before returning.

  We only care about CONNECT and CHANGE_USER sub events.

  @param [in] thd                THD pointer
  @param [in] coordinator        Connection_event_coordinator
  @param [in] connection_event   Connection event to be handled

  @returns status of connection event handling
    @retval false  Successfully handled an event.
    @retval true   Something went wrong.
                   error_buffer may contain details.
*/

bool Connection_delay_action::notify_event(
    MYSQL_THD thd, Connection_event_coordinator *coordinator,
    const mysql_event_tracking_connection_data *connection_event) {
  bool error = false;
  const unsigned int subclass = connection_event->event_subclass;
  Connection_event_observer *self = this;

  if (subclass != EVENT_TRACKING_CONNECTION_CONNECT &&
      subclass != EVENT_TRACKING_CONNECTION_CHANGE_USER) {
    return error;
  }

  RD_lock rd_lock(m_lock);

  const int64 threshold = this->get_threshold();

  /* If feature was disabled, return */
  if (threshold <= DISABLE_THRESHOLD) {
    return error;
  }

  /* Ignore failed connections from unauthenticated users if exemption was
   * enabled */
  if (connection_event->status != 0 && get_exempt_unknown_users() &&
      is_unauthenticated_connection(thd)) {
    // increment statistics
    error = coordinator->notify_status_var(
        &self, STAT_CONNECTION_EXEMPTED_USERS, ACTION_INC);
    if (error) {
      LogComponentErr(ERROR_LEVEL,
                      ER_CONN_CONTROL_STAT_CONN_EXEMPTED_USERS_UPDATE_FAILED);
    }
    return error;
  }

  int64 current_count = 0;
  bool user_present = false;
  Sql_string userhost;

  make_hash_key(thd, userhost);

  /* Cache current failure count */
  current_count =
      g_failed_attempts_list.get_failed_attempts_count(userhost.c_str());
  user_present = current_count != 0;

  if (current_count >= threshold || current_count < 0) {
    /*
      If threshold is crosed, regardless of connection success
      or failure, wait for (current_count + 1) - threshold seconds
      Note that current_count is not yet updated in hash. So we
      have to consider current connection as well - Hence the usage
      of current_count + 1.
    */
    const ulonglong wait_time =
        get_wait_time(((current_count + 1) - threshold) * 1000);
    error = coordinator->notify_status_var(
        &self, STAT_CONNECTION_DELAY_TRIGGERED, ACTION_INC);
    if (error) {
      LogComponentErr(ERROR_LEVEL,
                      ER_CONN_CONTROL_STAT_CONN_DELAY_TRIGGERED_UPDATE_FAILED);
    }
    /*
      Invoking sleep while holding read lock on Connection_delay_action
      would block access to cache data through IS table.
    */
    rd_lock.unlock();
    conditional_wait(wait_time);
    rd_lock.lock();

    ++opt_option_tracker_usage_connection_control_component;
  }

  if (connection_event->status != 0) {
    /*
      Connection failure.
      Add new entry to hash or increment failed connection count
      for an existing entry
    */
    g_failed_attempts_list.failed_attempts_define(userhost.c_str());
  } else {
    /*
      Successful connection.
      delete entry for given account from the hash
    */
    if (user_present) {
      g_failed_attempts_list.failed_attempts_undefine(userhost.c_str());
    }
  }
  return error;
}

/**
  Notification of a change in system variable value

  @param [in] coordinator        Handle to coordinator
  @param [in] variable           Enum of variable
  @param [in] new_value          New value for variable

  @returns processing status
    @retval false  Change in variable value processed successfully
    @retval true Error processing new value.
                  error_buffer may contain more details.
*/

bool Connection_delay_action::notify_sys_var(
    Connection_event_coordinator *coordinator, opt_connection_control variable,
    void *new_value) {
  bool error = true;
  Connection_event_observer *self = this;

  const WR_lock wr_lock(m_lock);

  switch (variable) {
    case OPT_FAILED_CONNECTIONS_THRESHOLD: {
      const int64 new_threshold = *(static_cast<int64 *>(new_value));
      assert(new_threshold >= DISABLE_THRESHOLD);
      set_threshold(new_threshold);
      error = coordinator->notify_status_var(
          &self, STAT_CONNECTION_DELAY_TRIGGERED, ACTION_RESET);
      if (error) {
        LogComponentErr(ERROR_LEVEL,
                        ER_CONN_CONTROL_STAT_CONN_DELAY_TRIGGERED_RESET_FAILED);
      }
      break;
    }
    case OPT_MIN_CONNECTION_DELAY:
    case OPT_MAX_CONNECTION_DELAY: {
      const int64 new_delay = *(static_cast<int64 *>(new_value));
      error = set_delay(new_delay, (variable == OPT_MIN_CONNECTION_DELAY));
      if (error) {
        LogComponentErr(ERROR_LEVEL, ER_CONN_CONTROL_FAILED_TO_SET_CONN_DELAY,
                        (variable == OPT_MIN_CONNECTION_DELAY) ? "min" : "max");
      }
      break;
    }
    case OPT_EXEMPT_UNKNOWN_USERS: {
      const bool new_flag = *(static_cast<bool *>(new_value));
      set_exempt_unknown_users(new_flag);
      break;
    }
    default:
      /* Should never reach here. */
      assert(false);
      LogComponentErr(ERROR_LEVEL, ER_CONN_CONTROL_INVALID_CONN_DELAY_TYPE);
  };
  return error;
}

/**
  Subscribe with coordinator for connection events

  @param [in] coordinator  Handle to Connection_event_coordinator
                           for registration
*/
void Connection_delay_action::init(Connection_event_coordinator *coordinator) {
  assert(coordinator);
  [[maybe_unused]] bool retval;
  Connection_event_observer *subscriber = this;
  const WR_lock wr_lock(m_lock);
  try {
    retval = coordinator->register_event_subscriber(&subscriber, &m_sys_vars,
                                                    &m_stats_vars);
  } catch (...) {
  }
  assert(!retval);
}

/**
  Clear data from Connection_delay_action
*/

void Connection_delay_action::deinit() {
  mysql_rwlock_wrlock(m_lock);
  m_sys_vars.clear();
  m_stats_vars.clear();
  m_threshold = DISABLE_THRESHOLD;
  mysql_rwlock_unlock(m_lock);
  m_lock = nullptr;
}

/**
  Initializes required objects for handling connection events.

  @param [in] coordinator    Connection_event_coordinator handle.
*/

void init_connection_delay_event(Connection_event_coordinator *coordinator) {
  /*
    1. Initialize lock(s)
  */
  mysql_rwlock_init(key_connection_event_delay_lock,
                    &connection_event_delay_lock);
  g_max_failed_connection_handler = new Connection_delay_action(
      g_variables.failed_connections_threshold,
      g_variables.min_connection_delay, g_variables.max_connection_delay,
      g_variables.exempt_unknown_users, opt_enums, opt_enums_size,
      status_vars_enums, status_vars_enums_size, &connection_event_delay_lock);
  g_max_failed_connection_handler->init(coordinator);
}
/**
  Deinitializes objects and frees associated memory.
*/
void deinit_connection_delay_event() {
  delete g_max_failed_connection_handler;
  mysql_rwlock_destroy(&connection_event_delay_lock);
}

}  // namespace connection_control
