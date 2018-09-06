/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTING_DEST_ROUND_ROBIN_INCLUDED
#define ROUTING_DEST_ROUND_ROBIN_INCLUDED

#include "destination.h"
#include "mysqlrouter/routing.h"

#include "mysql/harness/logging/logging.h"
#include "mysql_router_thread.h"

class DestRoundRobin : public RouteDestination {
 public:
  using RouteDestination::RouteDestination;

  /** @brief Default constructor
   *
   * @param protocol Protocol for the destination, defaults to value returned
   *        by Protocol::get_default()
   * @param routing_sock_ops Socket operations implementation to use, defaults
   *        to "real" (not mock) implementation
   * (mysql_harness::SocketOperations)
   * @param thread_stack_size memory in kilobytes allocated for thread's stack
   */
  DestRoundRobin(
      Protocol::Type protocol = Protocol::get_default(),
      routing::RoutingSockOpsInterface *routing_sock_ops =
          routing::RoutingSockOps::instance(
              mysql_harness::SocketOperations::instance()),
      size_t thread_stack_size = mysql_harness::kDefaultStackSizeInKiloBytes)
      : RouteDestination(protocol, routing_sock_ops),
        quarantine_thread_(thread_stack_size) {}

  /** @brief Destructor */
  ~DestRoundRobin() override;

  /** @brief run Quarantine Manager Thread */
  static void *run_thread(void *context);

  virtual void start(const mysql_harness::PluginFuncEnv * /*env*/) override;

  int get_server_socket(
      std::chrono::milliseconds connect_timeout, int *error,
      mysql_harness::TCPAddress *address = nullptr) noexcept override;

  /** @brief Returns number of quarantined servers
   *
   * @return size_t
   */
  size_t size_quarantine();

 protected:
  /** @brief Returns whether destination is quarantined
   *
   * Uses the given index to check whether the destination is
   * quarantined.
   *
   * @param index index of the destination to check
   * @return True if destination is quarantined
   */
  virtual bool is_quarantined(const size_t index) {
    return std::find(quarantined_.begin(), quarantined_.end(), index) !=
           quarantined_.end();
  }

  /** @brief Adds server to quarantine
   *
   * Adds the given server address to the quarantine list. The index argument
   * is the index of the server in the destination list.
   *
   * @param index Index of the destination
   */
  virtual void add_to_quarantine(size_t index) noexcept;

  /** @brief Worker checking and removing servers from quarantine
   *
   * This method is meant to run in a thread and calling the
   * `cleanup_quarantine()` method.
   *
   * The caller is responsible for locking and unlocking the
   * mutex `mutex_quarantine_`.
   *
   */
  virtual void quarantine_manager_thread() noexcept;

  /** @brief Checks and removes servers from quarantine
   *
   * This method removes servers from quarantine while trying to establish
   * a connection. It is used in a seperate thread and will update the
   * quarantine list, and will keep trying until the list is empty.
   * A conditional variable is used to notify the thread servers were
   * quarantined.
   *
   */
  virtual void cleanup_quarantine() noexcept;

  /** @brief List of destinations which are quarantined */
  std::vector<size_t> quarantined_;

  /** @brief Conditional variable blocking quarantine manager thread */
  std::condition_variable condvar_quarantine_;

  /** @brief Mutex for quarantine manager thread */
  std::mutex mutex_quarantine_manager_;

  /** @brief Mutex for updating quarantine */
  std::mutex mutex_quarantine_;

  /** @brief refresh thread facade */
  mysql_harness::MySQLRouterThread quarantine_thread_;

  /** @brief Whether we are stopping */
  std::atomic_bool stopping_{false};
};

#endif  // ROUTING_DEST_ROUND_ROBIN_INCLUDED
