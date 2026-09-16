// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_RELAY_LOG_LOG_PURGE_CONTROLLER_H
#define MYSQL_CSA_RELAY_LOG_LOG_PURGE_CONTROLLER_H

#include <functional>  // reference_wrapper
#include <memory>
#include <string>

namespace mysql::csa {

class Log_purge_controller;
using Log_purge_controller_ref = std::reference_wrapper<Log_purge_controller>;
using Log_purge_controller_sptr = std::shared_ptr<Log_purge_controller>;

/// @brief Interface for a relay log controller which is capable of purging
/// the relay logs
class Log_purge_controller {
 public:
  /// @brief Register log_filename as a log ready to be purged. This function
  /// will purge registered logs if they are in order according to
  /// the index file content.
  /// @param[in] log_filename Registeres this log as a log ready to be purged.
  /// If registered logs for purging are in order, purges up to log_filename,
  /// included.
  /// @retval false Success
  /// @retval true failure
  virtual bool concurrent_purge(const std::string &log_filename) = 0;
  virtual ~Log_purge_controller() {}
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_RELAY_LOG_LOG_PURGE_CONTROLLER_H
