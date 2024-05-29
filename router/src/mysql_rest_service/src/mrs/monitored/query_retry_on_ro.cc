/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "query_retry_on_ro.h"

#include "mrs/database/helper/query_gtid_executed.h"
#include "mrs/interface/rest_error.h"
#include "mrs/router_observation_entities.h"

namespace mrs {
namespace monitored {

void count_using_wait_at_ro_connection() {
  Counter<kEntityCounterRestAsofUsesRo>::increment();
}

void count_using_wait_at_rw_connection() {
  Counter<kEntityCounterRestAsofUsesRw>::increment();
}

void count_after_wait_timeout_switch_ro_to_rw() {
  Counter<kEntityCounterRestAsofSwitchesFromRo2Rw>::increment();
}

void throw_rest_error_asof_timeout_if_not_gtid_executed(
    mysqlrouter::MySQLSession *session, const mysqlrouter::sqlstring &gtid) {
  if (!mrs::database::is_gtid_executed(session, gtid)) {
    throw_rest_error_asof_timeout();
  }
}

void throw_rest_error_asof_timeout() {
  Counter<kEntityCounterRestAsofNumberOfTimeouts>::increment();
  throw mrs::interface::RestError(
      "'Asof' requirement was not fulfilled, timeout occurred.");
}

void QueryRetryOnRO::throw_timeout() const {
  mrs::monitored::throw_rest_error_asof_timeout();
}

void QueryRetryOnRO::using_ro_connection() const {
  count_using_wait_at_ro_connection();
}

void QueryRetryOnRO::using_rw_connection() const {
  count_using_wait_at_rw_connection();
}

void QueryRetryOnRO::switch_ro_to_rw() const {
  count_after_wait_timeout_switch_ro_to_rw();
}

}  // namespace monitored
}  // namespace mrs
