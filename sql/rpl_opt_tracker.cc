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

#include "sql/rpl_opt_tracker.h"

#include <my_dbug.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/util/weak_service_reference.h>
#include "mysql/components/library_mysys/option_tracker_usage.h"
#include "sql/mysqld.h"
#include "sql/replication.h"
#include "sql/rpl_msr.h"

const std::string Rpl_opt_tracker::s_c_name_mysql_server{"mysql_server"};
const std::string Rpl_opt_tracker::s_f_name_binary_log{"Binary Log"};
const std::string Rpl_opt_tracker::s_f_name_replication_replica{
    "Replication Replica"};

static const std::string s_name("mysql_option_tracker_option");
static const std::string c_name_mysql_server_replication(
    "mysql_server_replication");
typedef weak_service_reference<SERVICE_TYPE(mysql_option_tracker_option),
                               c_name_mysql_server_replication, s_name>
    srv_weak_option_option;

static void *launch_thread(void *arg) {
  Rpl_opt_tracker *handler = (Rpl_opt_tracker *)arg;
  handler->worker();
  return nullptr;
}

Rpl_opt_tracker::Rpl_opt_tracker() {
  srv_weak_option_option::init(
      srv_registry, srv_registry_registration,
      [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        opt->define(s_f_name_binary_log.c_str(), s_c_name_mysql_server.c_str(),
                    opt_bin_log ? 1 : 0);
        opt->define(s_f_name_replication_replica.c_str(),
                    s_c_name_mysql_server.c_str(),
                    is_replication_replica_enabled() ? 1 : 0);
        option_usage_read_counter(s_f_name_binary_log.c_str(),
                                  &m_opt_option_tracker_usage_binary_log,
                                  srv_registry);
        cb_binlog_define_failed = option_usage_register_callback(
            s_f_name_binary_log.c_str(), cb_binlog, srv_registry);
        option_usage_read_counter(
            s_f_name_replication_replica.c_str(),
            &m_opt_option_tracker_usage_replication_replica, srv_registry);
        cb_replica_define_failed = option_usage_register_callback(
            s_f_name_replication_replica.c_str(), cb_replica, srv_registry);
        return false;
      },
      false);
}

Rpl_opt_tracker::~Rpl_opt_tracker() {
  srv_weak_option_option::deinit(
      srv_registry, srv_registry_registration,
      [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        opt->undefine(s_f_name_binary_log.c_str());
        if (!cb_binlog_define_failed) {
          option_usage_unregister_callback(s_f_name_binary_log.c_str(),
                                           cb_binlog, srv_registry);
        }
        opt->undefine(s_f_name_replication_replica.c_str());
        if (!cb_replica_define_failed) {
          option_usage_unregister_callback(s_f_name_replication_replica.c_str(),
                                           cb_replica, srv_registry);
        }
        return false;
      });
}

bool Rpl_opt_tracker::is_replication_replica_enabled() {
  bool replication_replica_enabled = false;

  if (server_id != 0) {
    channel_map.rdlock();
    if (is_slave_configured()) {
      replication_replica_enabled =
          (channel_map.get_number_of_configured_channels() > 0);
    }
    channel_map.unlock();
  }

  return replication_replica_enabled;
}

void Rpl_opt_tracker::track(const Tracker_service_guard &service_guard,
                            bool enabled, const std::string &fname,
                            unsigned long long &usage_counter) {
  if (service_guard.is_valid()) {
    service_guard->set_enabled(fname.c_str(), enabled ? 1 : 0);
    if (enabled) {
      ++usage_counter;
    }
  }
}

void Rpl_opt_tracker::track_binary_log(
    const Tracker_service_guard &service_guard, bool enabled) const {
  track(service_guard, enabled, s_f_name_binary_log,
        m_opt_option_tracker_usage_binary_log);
}

void Rpl_opt_tracker::track_replication_replica(
    const Tracker_service_guard &service_guard, bool enabled) const {
  track(service_guard, enabled, s_f_name_replication_replica,
        m_opt_option_tracker_usage_replication_replica);
}

void Rpl_opt_tracker::track_replication_replica(bool enabled) const {
  Tracker_service_guard service_guard(m_service_name, srv_registry);
  track_replication_replica(service_guard, enabled);
}

void Rpl_opt_tracker::worker() {
  THD *thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = reinterpret_cast<char *>(&thd);
  thd->set_command(COM_DAEMON);
  thd->security_context()->skip_grants();
  thd->system_thread = SYSTEM_THREAD_BACKGROUND;
  thd->store_globals();
  thd->set_time();

  for (;;) {
    /*
      Only track features if the option tracker service is
      installed.
    */
    {
      Tracker_service_guard service_guard(m_service_name, srv_registry);

      /*
        Binary Log
      */
      track_binary_log(service_guard, opt_bin_log);

      /*
        Replication Replica
      */
      track_replication_replica(service_guard,
                                is_replication_replica_enabled());
    }

    mysql_mutex_lock(&LOCK_rpl_opt_tracker);
    if (m_stop_worker || thd->killed) {
      mysql_mutex_unlock(&LOCK_rpl_opt_tracker);
      break;
    }

    THD_ENTER_COND(thd, &COND_rpl_opt_tracker, &LOCK_rpl_opt_tracker,
                   &stage_suspending, nullptr);
    struct timespec nowtime;
    set_timespec(&nowtime, 0);
    struct timespec abstime;
    set_timespec(&abstime, s_tracking_period);
    DBUG_EXECUTE_IF("rpl_opt_tracker_small_tracking_period",
                    set_timespec(&abstime, 1););

    while (cmp_timespec(&nowtime, &abstime) <= 0) {
      mysql_cond_timedwait(&COND_rpl_opt_tracker, &LOCK_rpl_opt_tracker,
                           &abstime);
      if (m_stop_worker || thd->killed) {
        break;
      }
      set_timespec(&nowtime, 0);
    }

    if (m_stop_worker || thd->killed) {
      mysql_mutex_unlock(&LOCK_rpl_opt_tracker);
      THD_EXIT_COND(thd, nullptr);
      break;
    }
    mysql_mutex_unlock(&LOCK_rpl_opt_tracker);
    THD_EXIT_COND(thd, nullptr);
  }

  thd->release_resources();
  thd->restore_globals();
  delete thd;
  my_thread_end();
  my_thread_exit(nullptr);
}

void Rpl_opt_tracker::start_worker() {
  my_thread_attr_t attr;
  if (my_thread_attr_init(&attr)) {
    LogErr(WARNING_LEVEL, ER_FAILED_TO_CREATE_RPL_OPT_TRACKER_THREAD);
    return;
  }

  if (
#ifndef _WIN32
      pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM) ||
#endif
      mysql_thread_create(key_thread_rpl_opt_tracker, &m_thread_id, &attr,
                          launch_thread, (void *)this)) {
    LogErr(WARNING_LEVEL, ER_FAILED_TO_CREATE_RPL_OPT_TRACKER_THREAD);
  }

  (void)my_thread_attr_destroy(&attr);
}

void Rpl_opt_tracker::stop_worker() {
  mysql_mutex_lock(&LOCK_rpl_opt_tracker);
  m_stop_worker = true;
  mysql_cond_signal(&COND_rpl_opt_tracker);
  mysql_mutex_unlock(&LOCK_rpl_opt_tracker);

  if (m_thread_id.thread != null_thread_initializer) {
    my_thread_join(&m_thread_id, nullptr);
    m_thread_id.thread = null_thread_initializer;
  }
}

unsigned long long Rpl_opt_tracker::m_opt_option_tracker_usage_binary_log = 0;
unsigned long long
    Rpl_opt_tracker::m_opt_option_tracker_usage_replication_replica = 0;
bool Rpl_opt_tracker::cb_binlog_define_failed = false,
     Rpl_opt_tracker::cb_replica_define_failed = false;
