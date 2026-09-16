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

#ifndef MYSQL_CSA_EVENT_SET_FETCHABLE_H
#define MYSQL_CSA_EVENT_SET_FETCHABLE_H

#include <list>
#include <memory>
#include <optional>
#include "mysql/utils/return_status.h"
#include "sql/changestreams/apply/core/managed_event.h"

namespace mysql::csa {

class Event_set_fetchable;

/// @brief Unique pointer to a specific implementation of Event_set_fetchable
/// (no need to share).
using Event_set_fetchable_ptr = std::unique_ptr<Event_set_fetchable>;

/// @brief List of Event_set_fetchable pointers, each able to fetch a whole
/// transaction from the storage.
using Event_set_fetchable_list = std::list<Event_set_fetchable_ptr>;

/// @brief Represents metadata of a set of events, capable of being fetched
/// from a storage. Typically, one event set will contain
/// metadata of a part of, or a whole transaction. Event_set_fetchable supplies
/// a function to fetch consecutive Log_event objects. If needed, it
/// decompresses the underlying event stream. A transaction contains one or many
/// fetchable event sets; this is decided by a specific implementation of a
/// storage reader (e.g., relay log storage reader will keep separate event sets
/// for parts of transaction kept in separate files). The specific
/// implementation of a fetchable event set is driven by the implementation of
/// the storage type (see Event_set_fetchable_relay_log).
class Event_set_fetchable {
 public:
  using Fde_type = Format_description_log_event;
  using Fde_ptr = Fde_type *;
  using Log_event_ptr = std::shared_ptr<Log_event>;
  /// @brief Waits until the next event can be fetched.
  /// @return true if the next event can be fetched, false on end/error.
  virtual bool wait_next() = 0;
  /// @brief Fetches the next event if possible.
  ///
  /// Callers are expected to wait for availability with `wait_next()` first
  /// when the underlying implementation supports streaming updates.
  /// @return Managed_event object or an empty optional if the stream ended
  /// (with or without error).
  virtual std::optional<Managed_event> fetch_next() = 0;
  /// @brief Checks if the event set has been fully fetched successfully.
  /// @return true if the event set finished successfully, false otherwise.
  virtual bool is_done() const = 0;
  /// @brief Checks if an error occurred during fetching.
  /// @return true if an error occurred, false otherwise.
  virtual bool is_error() const = 0;
  /// @brief Retrieves the error message if an error occurred.
  /// @return A reference to the error message string (empty if no error).
  virtual const std::string &get_error_str() const = 0;
  /// @brief Resets the state to allow fetching the event set again, also
  /// clearing any error state.
  /// @param reset_events When true events states need to be reset
  virtual void reset(bool reset_events) = 0;
  /// @brief Checks if this event set represents a complete transaction.
  /// @return true if it represents a transaction, false otherwise.
  virtual bool is_trx() const = 0;
  /// @brief Obtains non-owning pointer to current transaction FDE.
  /// @return Non-owning pointer to FDE.
  virtual Fde_ptr get_fde() = 0;
  /// @brief Sets the end of the stream if needed. Empty by default.
  /// @brief Virtual destructor.
  virtual ~Event_set_fetchable() = default;
  /// @brief Callback on success, default - do nothing
  virtual void set_success() {}

 private:
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_EVENT_SET_FETCHABLE_H
