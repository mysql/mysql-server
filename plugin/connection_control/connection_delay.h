/* Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CONNECTION_DELAY_H
#define CONNECTION_DELAY_H

#include <atomic>

#include "lf.h"          /* LF Hash */
#include "my_hostname.h" /* HOSTNAME_LENGTH */
#include "my_inttypes.h"
#include "mysql_com.h" /* USERNAME_LENGTH */
#include "plugin/connection_control/connection_control_data.h" /* variables and status */
#include "plugin/connection_control/connection_control_interfaces.h" /* Observer interface */
#include "plugin/connection_control/connection_control_memory.h" /* Connection_control_alloc */
#include "plugin/connection_control/connection_delay_api.h" /* Constants */
#include "sql/table.h"                                      /* TABLE_LIST */

namespace connection_control {
/**
  Class to store failed attempts information for a given user.
*/

class Connection_event_record : public Connection_control_alloc {
 public:
  /**
    Constructor for Connection_event_record. Always initializes failed login
    count to 1.
  */
  Connection_event_record(const Sql_string &s) : m_count(1) {
    memset((void *)m_userhost, 0, sizeof(m_userhost));
    memcpy((void *)m_userhost, s.c_str(), s.length());
    m_length = s.length();
  }

  /**
    Retrives failed login count for given user entry

    @returns Failed login count
  */
  int64 get_count() const { return m_count.load(); }

  /** Increment failed login count for given user entry by 1 */
  void inc_count() { ++m_count; }

  /** Reset failed login count for given user entry */
  void reset_count() { m_count.store(0); }

  /** Get user information */
  uchar *get_userhost() const { return const_cast<uchar *>(m_userhost); }

  /**  Get length information  */
  size_t get_length() const { return m_length; }

  /** Destructor */
  ~Connection_event_record() { m_count = DISABLE_THRESHOLD; }

 private:
  /* '<user>'@'<host>' */
  uchar m_userhost[1 + USERNAME_LENGTH + 3 + HOSTNAME_LENGTH + 1 + 1];
  /* Length of m_userhost */
  size_t m_length;
  /* connection event count */
  std::atomic<int64> m_count;
};

/**
  Hash for a connection event.
  Stores information in Connection_event_record object for each user.
*/

class Connection_delay_event : public Connection_event_records {
 public:
  /** Constructor. Also initializes the hash */
  Connection_delay_event();

  /** Destructor. Removes all entries from hash before destroying hash */
  ~Connection_delay_event() {
    reset_all();
    lf_hash_destroy(&m_entries);
  }

  void fill_IS_table(TABLE_LIST *tables);

  /* Overridden function */
  bool create_or_update_entry(const Sql_string &s);
  bool remove_entry(const Sql_string &s);
  bool match_entry(const Sql_string &s, void *value);
  void reset_all();

 private:
  /** Hash for storing Connection_event_record per user */
  LF_HASH m_entries;
};

/**
  Connection event action to enforce max failed login constraint
*/

class Connection_delay_action : public Connection_event_observer,
                                public Connection_control_alloc {
 public:
  Connection_delay_action(int64 threshold, int64 min_delay, int64 max_delay,
                          opt_connection_control *sys_vars,
                          size_t sys_vars_size,
                          stats_connection_control *status_vars,
                          size_t status_vars_size, mysql_rwlock_t *lock);

  /** Destructor */
  ~Connection_delay_action() {
    deinit();
    m_lock = 0;
  }

  void init(Connection_event_coordinator_services *coordinator);

  /**
    Set threshold value.

    @param threshold [in]        New threshold value

    @returns whether threshold value was changed successfully or not
      @retval true  Success
      @retval false Failure. Invalid threshold value specified.
  */

  void set_threshold(int64 threshold) {
    m_threshold.store(threshold);
    /* Clear the hash */
    m_userhost_hash.reset_all();
  }

  /** Get threshold value */
  int64 get_threshold() { return m_threshold.load(); }

  /**
    Set min/max delay

    @param new_value [in]        New m_min_delay/m_max_delay value
    @param min [in]              true for m_min_delay. false otherwise.

    @returns whether m_min_delay/m_max_delay value was changed successfully or
    not
      @retval false  Success
      @retval true Failure. Invalid value specified.
  */

  bool set_delay(int64 new_value, bool min) {
    int64 current_max = get_max_delay();
    int64 current_min = get_min_delay();

    if (new_value < MIN_DELAY) return true;

    if ((min && new_value > current_max) || (!min && new_value < current_min))
      return true;

    else
      min ? m_min_delay.store(new_value) : m_max_delay.store(new_value);
    return false;
  }

  /** Get max value */
  int64 get_max_delay() { return m_max_delay.load(); }

  /** Get min value */
  int64 get_min_delay() { return m_min_delay.load(); }

  void fill_IS_table(THD *thd, TABLE_LIST *tables, Item *cond);

  /** Overridden functions */
  bool notify_event(MYSQL_THD thd,
                    Connection_event_coordinator_services *coordinator,
                    const mysql_event_connection *connection_event,
                    Error_handler *error_handler);
  bool notify_sys_var(Connection_event_coordinator_services *coordinator,
                      opt_connection_control variable, void *new_value,
                      Error_handler *error_handler);

 private:
  void deinit();
  void make_hash_key(MYSQL_THD thd, Sql_string &s);
  /**
    Generates wait time

    @param count [in] Proposed delay

    @returns wait time
  */

  inline ulonglong get_wait_time(int64 count) {
    int64 max_delay = get_max_delay();
    int64 min_delay = get_min_delay();
    int64 count_mili = count * 1000;

    /*
      if count < 0 (can happen in edge cases
      we return max_delay.
      Otherwise, following equation will be used:
      wait_time = MIN(MIN(count, min_delay),
                      max_delay)
    */
    return (static_cast<ulonglong>(
        (count_mili >= MIN_DELAY && count_mili < max_delay)
            ? (count_mili < min_delay ? min_delay : count_mili)
            : max_delay));
  }
  void conditional_wait(THD *thd, ulonglong wait_time);

 private:
  /** Threshold value which triggers wait */
  std::atomic<int64> m_threshold;
  /** Lower cap on delay to be generated */
  std::atomic<int64> m_min_delay;
  /** Upper cap on delay to be generated */
  std::atomic<int64> m_max_delay;
  /** System variables */
  std::vector<opt_connection_control> m_sys_vars;
  /** Status variables */
  std::vector<stats_connection_control> m_stats_vars;
  /** Hash to store failed attempts for each user entry */
  Connection_delay_event m_userhost_hash;
  /** RW lock */
  mysql_rwlock_t *m_lock;
};
}  // namespace connection_control
#endif /* !CONNECTION_DELAY_H */
