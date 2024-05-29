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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_MONITORED_QUERY_RETRY_ON_RO_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_MONITORED_QUERY_RETRY_ON_RO_H_

#include "mrs/database/helper/query_retry_on_ro.h"

namespace mrs {
namespace monitored {

/**
 * This function is dedicated for monitoring of generated timeouts
 *
 * Function is extracted here, because it monitors usage
 * count in concrete case.
 */
void throw_rest_error_asof_timeout_if_not_gtid_executed(
    mysqlrouter::MySQLSession *session, const mysqlrouter::sqlstring &gtid);

/**
 * This function is dedicated for monitoring of generated timeouts
 *
 * Function is extracted here, because it monitors usage
 * count in concrete case.
 */
void throw_rest_error_asof_timeout();

/**
 * This function is dedicated for monitoring "asof/wait for gtid" executions on
 * RO connection
 *
 * Function is extracted here, because it monitors usage
 * count in concrete case.
 */
void count_using_wait_at_ro_connection();

/**
 * This function is dedicated for monitoring "asof/wait for gtid" executions on
 * RW connection
 *
 * Function is extracted here, because it monitors usage
 * count in concrete case.
 */
void count_using_wait_at_rw_connection();

/**
 * This function is dedicated for monitoring "asof/wait for gtid"  where MRS
 * switched from RO to RW
 *
 * Function is extracted here, because it monitors usage
 * count in concrete case.
 */
void count_after_wait_timeout_switch_ro_to_rw();

/**
 * This class is dedicated for monitoring of generating timeouts
 *
 * Class is derived here, because it monitors usage
 * count of `throw_asof_timeout`.
 */
class QueryRetryOnRO : public mrs::database::QueryRetryOnRO {
 public:
  using mrs::database::QueryRetryOnRO::QueryRetryOnRO;

 protected:
  void throw_timeout() const override;
  void using_ro_connection() const override;
  void using_rw_connection() const override;
  void switch_ro_to_rw() const override;
};

}  // namespace monitored
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_MONITORED_QUERY_RETRY_ON_RO_H_
