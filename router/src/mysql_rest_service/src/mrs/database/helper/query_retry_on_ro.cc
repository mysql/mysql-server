/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mrs/database/helper/query_retry_on_ro.h"

#include "mrs/database/helper/query_gtid_executed.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using MySQLSession = mysqlrouter::MySQLSession;
using sqlstring = mysqlrouter::sqlstring;
using TCPAddress = mysql_harness::TCPAddress;
using ConnectionParameters =
    collector::CountedMySQLSession::ConnectionParameters;

static TCPAddress get_tcpaddr(const ConnectionParameters &c) {
  return {c.conn_opts.host, static_cast<uint16_t>(c.conn_opts.port)};
}

static bool is_rw(collector::MySQLConnection connection) {
  return connection == collector::kMySQLConnectionMetadataRW ||
         connection == collector::kMySQLConnectionUserdataRW;
}

QueryRetryOnRO::QueryRetryOnRO(collector::MysqlCacheManager *cache,
                               CachedSession &session,
                               GtidManager *gtid_manager,
                               FilterObjectGenerator &fog,
                               uint64_t wait_gtid_timeout,
                               bool query_has_gtid_check)
    : session_{session},
      gtid_manager_{gtid_manager},
      cache_{cache},
      fog_{fog},
      filter_object_has_asof_{fog_.has_asof()},
      wait_gtid_timeout_{wait_gtid_timeout},
      query_has_gtid_check_{query_has_gtid_check} {
  if (filter_object_has_asof_) {
    gtid_ = fog_.get_asof();
  }
}

void QueryRetryOnRO::before_query() {
  const bool is_session_rw = is_rw(cache_->get_type(session_));

  if (filter_object_has_asof_) {
    if (!is_session_rw)
      using_ro_connection();
    else
      using_rw_connection();
  }

  if (!fog_.has_asof()) return;

  if (check_gtid(gtid_)) {
    fog_.reset(FilterObjectGenerator::Clear::kAsof);

    // Just to block retry
    query_has_gtid_check_ = false;
    return;
  }

  if (query_has_gtid_check_) {
    return;
  }

  if (!wait_gtid_executed(session_.get(), gtid_, wait_gtid_timeout_)) {
    if (is_session_rw) throw_timeout();
    session_ =
        cache_->get_instance(collector::kMySQLConnectionUserdataRW, false);

    is_retry_ = true;
    switch_ro_to_rw();
    before_query();
    return;
  }
  auto addr = get_tcpaddr(session_->get_connection_parameters());
  gtid_manager_->remember(addr, {gtid_.str()});
}

mysqlrouter::MySQLSession *QueryRetryOnRO::get_session() {
  return session_.get();
}

const FilterObjectGenerator &QueryRetryOnRO::get_fog() { return fog_; }

bool QueryRetryOnRO::should_retry(const uint64_t affected) const {
  if (!query_has_gtid_check_) return false;
  if (!is_retry_ && !fog_.has_asof()) return false;
  if (affected != 0) return false;

  // Check the timeout
  if (!is_gtid_executed(session_.get(), gtid_)) {
    // Timeout, change session to RW
    if (is_rw(cache_->get_type(session_))) {
      throw_timeout();
    }
    log_debug("Retry on RW session.");
    session_ =
        cache_->get_instance(collector::kMySQLConnectionUserdataRW, false);

    switch_ro_to_rw();
    is_retry_ = true;

    return !session_.empty();
  }
  // There was not timeout
  return false;
}

bool QueryRetryOnRO::check_gtid(const std::string &gtid_str) {
  mrs::database::Gtid gtid{gtid_str};
  auto addr = get_tcpaddr(session_->get_connection_parameters());

  for (int retry = 0; retry < 2; ++retry) {
    auto result = gtid_manager_->is_executed_on_server(addr, gtid);

    if (result == mrs::GtidAction::k_needs_update) {
      auto gtidsets = get_gtid_executed(session_.get());
      gtid_manager_->reinitialize(addr, gtidsets);
      continue;
    }
    return result == mrs::GtidAction::k_is_on_server ? true : false;
  }

  return false;
}

}  // namespace database
}  // namespace mrs
