/*****************************************************************************

Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**
 @file log/log0consumer.cc

 *******************************************************/

#include "log0consumer.h" /* Log_consumer */
#include "arch0arch.h"
#include "arch0log.h"
#include "log0chkp.h"
#include "log0files_governor.h" /* log_files_mutex_own() */
#include "log0log.h"            /* log_get_lsn */
#include "srv0shutdown.h"       /* srv_shutdown_state, ... */
#include "srv0start.h"          /* srv_is_being_started */

Log_user_consumer::Log_user_consumer(const std::string &name) : m_name{name} {}

const std::string &Log_user_consumer::get_name() const { return m_name; }

void Log_user_consumer::set_consumed_lsn(lsn_t consumed_lsn) {
  if (consumed_lsn % OS_FILE_LOG_BLOCK_SIZE == 0) {
    consumed_lsn += LOG_BLOCK_HDR_SIZE;
  }
  ut_a(m_consumed_lsn <= consumed_lsn);
  m_consumed_lsn = consumed_lsn;
}

lsn_t Log_user_consumer::get_consumed_lsn() const { return m_consumed_lsn; }

void Log_user_consumer::consumption_requested() {}

Log_checkpoint_consumer::Log_checkpoint_consumer(log_t &log) : m_log{log} {}

const std::string &Log_checkpoint_consumer::get_name() const {
  static std::string name{"log_checkpointer"};
  return name;
}

lsn_t Log_checkpoint_consumer::get_consumed_lsn() const {
  return log_get_checkpoint_lsn(m_log);
}

void Log_checkpoint_consumer::consumption_requested() {
  log_request_checkpoint_in_next_file(m_log);
}

void log_consumer_register(log_t &log, Log_consumer *log_consumer) {
  ut_ad(log_files_mutex_own(log) || srv_is_being_started);

  log.m_consumers.insert(log_consumer);
}

void log_consumer_unregister(log_t &log, Log_consumer *log_consumer) {
  ut_ad(log_files_mutex_own(log) || srv_is_being_started ||
        srv_shutdown_state.load() != SRV_SHUTDOWN_NONE);

  log.m_consumers.erase(log_consumer);
}

Log_consumer *log_consumer_get_oldest(const log_t &log,
                                      lsn_t &oldest_needed_lsn) {
  ut_ad(log_files_mutex_own(log) || srv_is_being_started ||
        srv_shutdown_state.load() != SRV_SHUTDOWN_NONE);

  Log_consumer *oldest_consumer{nullptr};
  oldest_needed_lsn = LSN_MAX;

  for (auto consumer : log.m_consumers) {
    const lsn_t oldest_lsn = consumer->get_consumed_lsn();

    if (oldest_lsn < oldest_needed_lsn) {
      oldest_consumer = consumer;
      oldest_needed_lsn = oldest_lsn;
    }
  }

  const lsn_t current_lsn = log_get_lsn(log);
  ut_a(oldest_needed_lsn <= current_lsn);

  return oldest_consumer;
}
