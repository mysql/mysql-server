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

#ifndef MYSQL_CSA_RELAY_LOG_DELETER_H
#define MYSQL_CSA_RELAY_LOG_DELETER_H

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include "mysql/utils/return_status.h"
#include "sql/changestreams/apply/storage/relay_log/log_purge_controller.h"

namespace mysql::csa {

class Relay_log_deleter;

/// @brief Represents a shared reference to Relay_log_deleter. See explanation
/// below.
using Relay_log_deleter_handle = std::shared_ptr<Relay_log_deleter>;

/// @brief Class responsible for removing relay log file,
/// when the last handle to this deleter is released. To know if we are
/// allowed to delete this relay log file, we need to track its subscribers
/// and the number of successfull subscribers. If these two numbers are
/// equal and the last reference to the Relay_log_deleter is released, we
/// are allowed to remove the relay log. Otherwise, deleter won't remove the
/// file.
/// Object of this class is created by relay log metadata reader. Each event
/// batch in a transaction will contain shared reference to this object. When
/// event batch is successfully processed, i.e., job done notified in
/// @see Fetchable_transaction::set_success callback, it will release
/// reference to the Relay_log_deleter (in destructor).
/// Another subscriber of the relay log file
/// deleter is the relay log reader, which must hold reference until the file
/// changes or closes.
class Relay_log_deleter {
 public:
  /// @brief Constructor that initializes metadata needed for relay log
  /// deletion.
  /// @param rl_file_name Referenced relay log filename
  /// @param purger Relay log purger pointer
  Relay_log_deleter(const std::string &rl_file_name,
                    Log_purge_controller_sptr purger);
  /// @brief Destructor that removes the relay log file when the last reference
  /// is released.
  ~Relay_log_deleter();
  /// @brief Adds a subscriber to handled relay log file
  void add_subscriber();
  /// @brief Notifies that subscriber succeeded
  void set_subscriber_success();
  /// @brief Handled file name accessor
  /// @return Handled file name
  const std::string &get_file_name() const;

 private:
  /// @brief Physically removes relay log file from disk and index file
  void remove_handled_relay_log() const;
  /// @brief Relay log name handled by this deleter
  std::string m_rl_file_name{""};
  /// With below variables we track relay log file subscribers and the number of
  /// successful subscribers. If the number of subscribers is equal to
  /// the number of successful subscribers, we may remove the relay log.
  /// Inverse logic could be implemented with one atomic flag, however, this
  /// way we would not know if some transaction was killed without setting an
  /// error state (TBC).
  std::atomic<std::size_t> m_subscribers{0};
  /// The number of successful subscribers (see above)
  std::atomic<std::size_t> m_successfull_subscribers{0};
  /// Pointer to the relay log purger
  Log_purge_controller_sptr m_purger;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_RELAY_LOG_DELETER_H
