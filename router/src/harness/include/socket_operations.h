/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_SOCKETOPERATIONS_INCLUDED
#define MYSQL_HARNESS_SOCKETOPERATIONS_INCLUDED

#include <stdexcept>
#include <string>
#include <system_error>

#include "harness_export.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"

namespace mysql_harness {

using socket_t = net::impl::socket::native_handle_type;
constexpr socket_t kInvalidSocket = net::impl::socket::kInvalidSocket;

/** @class SocketOperationsBase
 * @brief Base class to allow multiple SocketOperations implementations
 *        (at least one "real" and one mock for testing purposes)
 */
class HARNESS_EXPORT SocketOperationsBase {
 public:
  explicit SocketOperationsBase() = default;
  explicit SocketOperationsBase(const SocketOperationsBase &) = default;
  SocketOperationsBase &operator=(const SocketOperationsBase &) = default;
  virtual ~SocketOperationsBase() = default;

  /** @brief Exception thrown by `get_local_hostname()` on error */
  class LocalHostnameResolutionError : public std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  /** @brief return hostname of local host */
  virtual std::string get_local_hostname() = 0;
};

/** @class SocketOperations
 * @brief This class provides a "real" (not mock) implementation
 */
class HARNESS_EXPORT SocketOperations : public SocketOperationsBase {
 public:
  static SocketOperations *instance();

  SocketOperations(const SocketOperations &) = delete;
  SocketOperations operator=(const SocketOperations &) = delete;
  /** @brief return hostname of local host
   *
   * @throws `LocalHostnameResolutionError` (std::runtime_error) on failure
   */
  std::string get_local_hostname() override;

 private:
  SocketOperations() = default;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_SOCKETOPERATIONS_INCLUDED
