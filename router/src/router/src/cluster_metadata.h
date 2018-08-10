/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTER_CLUSTER_METADATA_INCLUDED
#define ROUTER_CLUSTER_METADATA_INCLUDED

#include "config_generator.h"
#include "mysqlrouter/mysql_session.h"
#include "socket_operations.h"

namespace mysqlrouter {

class MySQLInnoDBClusterMetadata {
 public:
  MySQLInnoDBClusterMetadata(MySQLSession *mysql,
                             mysql_harness::SocketOperationsBase *sockops =
                                 mysql_harness::SocketOperations::instance())
      : mysql_(mysql), socket_operations_(sockops) {}

  /** @brief Checks if Router with id is already registered in metadata database
   *
   * @param router_id Router id
   * @param hostname_override If non-empty, this hostname will be used instead
   *        of getting queried from OS
   *
   * @throws LocalHostnameResolutionError(std::runtime_error) on hostname query
   *         failure, std::runtime_error on other failure
   */
  void check_router_id(uint32_t router_id,
                       const std::string &hostname_override = "");

  /** @brief Registers Router in metadata database
   *
   * @param router_name Router name
   * @param overwrite if Router name is already registered, allow this
   * registration to be "hijacked" instead of throwing
   * @param hostname_override If non-empty, this hostname will be used instead
   *        of getting queried from OS
   *
   * @throws LocalHostnameResolutionError(std::runtime_error) on hostname query
   *         failure, std::runtime_error on other failure
   */
  uint32_t register_router(const std::string &router_name, bool overwrite,
                           const std::string &hostname_override = "");

  void update_router_info(uint32_t router_id, const std::string &rw_endpoint,
                          const std::string &ro_endpoint,
                          const std::string &rw_x_endpoint,
                          const std::string &ro_x_endpoint);

 private:
  MySQLSession *mysql_;
  mysql_harness::SocketOperationsBase *socket_operations_;
};

/** @brief Verify that host is a valid metadata server
 *
 * @param mysql session object
 *
 * @throws MySQLSession::Error
 * @throws std::runtime_error
 * @throws std::out_of_range
 * @throws std::logic_error
+ */
void require_innodb_metadata_is_ok(MySQLSession *mysql);

/** @brief Verify that host is a valid Group Replication member
 *
 * @param mysql session object
 *
 * @throws MySQLSession::Error
 * @throws std::runtime_error
 * @throws std::out_of_range
 * @throws std::logic_error
 */
void require_innodb_group_replication_is_ok(MySQLSession *mysql);

}  // namespace mysqlrouter

#endif  // ROUTER_CLUSTER_METADATA_INCLUDED
