/* Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "mrs/database/helper/query_retry_on_rw.h"
#include "mrs/database/helper/query_faults.h"
#include "mrs/database/helper/query_gtid_executed.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using MySQLSession = mysqlrouter::MySQLSession;
using sqlstring = mysqlrouter::sqlstring;

static bool is_rw(collector::MySQLConnection connection) {
  return connection == collector::kMySQLConnectionMetadataRW ||
         connection == collector::kMySQLConnectionUserdataRW;
}

QueryRetryOnRW::QueryRetryOnRW(collector::MysqlCacheManager *cache,
                               CachedSession &session,
                               FilterObjectGenerator &fog,
                               uint64_t wait_gtid_timeout,
                               bool query_has_gtid_check)
    : session_{session},
      cache_{cache},
      fog_{fog},
      wait_gtid_timeout_{wait_gtid_timeout},
      query_has_gtid_check_{query_has_gtid_check} {
  if (fog_.has_asof()) {
    gtid_ = fog_.get_asof();
  }
}

void QueryRetryOnRW::before_query() {
  if (query_has_gtid_check_) return;
  if (!fog_.has_asof()) return;

  if (!wait_gtid_executed(session_.get(), gtid_, wait_gtid_timeout_)) {
    if (is_rw(cache_->get_type(session_))) throw_rest_error_asof_timeout();
    session_ =
        cache_->get_instance(collector::kMySQLConnectionUserdataRW, false);

    is_retry_ = true;
    before_query();
  }
}

mysqlrouter::MySQLSession *QueryRetryOnRW::get_session() {
  return session_.get();
}

const FilterObjectGenerator &QueryRetryOnRW::get_fog() { return fog_; }

bool QueryRetryOnRW::should_retry(const uint64_t affected) const {
  if (!query_has_gtid_check_) return false;
  if (!is_retry_ && !fog_.has_asof()) return false;
  if (affected != 0) return false;

  // Check the timeout
  if (!is_gtid_executed(session_.get(), gtid_)) {
    // Timeout, change session to RW
    if (is_rw(cache_->get_type(session_))) {
      throw_rest_error_asof_timeout();
    }
    log_debug("Retry on RW session.");
    session_ =
        cache_->get_instance(collector::kMySQLConnectionUserdataRW, false);

    is_retry_ = true;

    return !session_.empty();
  }
  // There was not timeout
  return false;
}

}  // namespace database
}  // namespace mrs
