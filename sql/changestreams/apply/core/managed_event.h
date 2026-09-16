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

#ifndef MYSQL_CSA_MANAGED_EVENT_H
#define MYSQL_CSA_MANAGED_EVENT_H

#include <memory>
#include "mysql/utils/return_status.h"
#include "sql/log_event.h"  // Log_event

namespace mysql::csa {

/// @brief Managed event. For now, it contains functionality to pass memory
/// ownership between CSA threads.
class Managed_event {
 public:
  using Log_event_ptr = std::shared_ptr<Log_event>;
  /// @brief Construct
  Managed_event() = default;
  /// @brief Construct from ev
  /// @param ev Pointer to copy
  /// @param ev_owns_memory True if ev exclusively owns log event memory
  Managed_event(const Log_event_ptr &ev, bool ev_owns_memory);
  /// @brief Internal event accessor
  /// @return Reference to internal event
  Log_event_ptr &get_event();
  /// @brief Internal event accessor, const
  /// @return Reference to internal event
  const Log_event_ptr &get_event() const;
  /// @brief Marks whether this fetched event is the last event of transaction.
  void set_last_in_transaction(bool is_last);
  /// @brief Returns whether this fetched event is the last event of
  /// transaction.
  bool is_last_in_transaction() const;
  /// @brief Returns true if Managed_event manages an event, false otherwise
  explicit operator bool() const noexcept;

 private:
  /// @brief Internal event pointer
  Log_event_ptr m_event;
  /// @brief True if this fetched event is the transaction terminal event.
  bool m_is_last_in_transaction{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_MANAGED_EVENT_H
