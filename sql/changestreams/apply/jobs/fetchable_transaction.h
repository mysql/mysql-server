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

#ifndef MYSQL_CSA_FETCHABLE_TRANSACTION_H
#define MYSQL_CSA_FETCHABLE_TRANSACTION_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "mysql/utils/return_status.h"
#include "sql/changestreams/apply/core/managed_event.h"
#include "sql/changestreams/apply/storage/common/event_set_fetchable.h"

namespace mysql::csa {

/// @brief Opaque for transaction, that is able to fetch itself in parts
/// from the storage on demand.
/// Function to fetch an event is supplied by the specific
/// storage implementation.
class Fetchable_transaction {
 public:
  using Return_status = mysql::utils::Return_status;

  /// @brief Fetch function is supplied by a specific storage implementation
  Fetchable_transaction();
  Fetchable_transaction(Event_set_fetchable_list &&fetch_object);
  Fetchable_transaction(const Fetchable_transaction &) = delete;
  Fetchable_transaction(Fetchable_transaction &&) = delete;

  /// @brief Destructor
  virtual ~Fetchable_transaction();

  /// Fetches consecutive events from the storage, until done or error occurs
  /// @return Managed event object in case it was successfully fetched from
  /// the storage, empty object in case stream is done or error occurred
  bool wait_next();
  std::optional<Managed_event> fetch_next();

  /// @brief Returs true when fetching failed
  /// @return True when fetching failed, false otherwise
  bool is_fetching_error() const;

  /// @brief Checks current status of the object
  /// @return True when fetching is completed, false otherwise
  bool is_fetching_done() const;

  /// @brief Checks whether fetching stopped because the metadata stream was
  /// truncated.
  /// @return True when fetching stopped on truncation, false otherwise
  bool is_truncated() const;
  /// Returns the maximum event length published for this transaction.
  std::size_t get_max_event_length() const;

  /// @brief Reset the status of the Featchable job allowing it to be
  /// re-read from the storage
  /// @param all When true, internal object states must be reset
  void reset_fetching(bool all);

  /// @brief Accesses fetch error message
  /// @return Fetch error information
  std::string get_fetch_error_msg() const;

  /// @brief Returns information on whether this is actual transaction
  /// (transaction starting with a GTID)
  bool is_trx() const;

  /// Marks that transaction committed succesfully, success callback
  void set_success();

  /// @brief Obtains non-owning pointer to current FDE
  /// @return Non-owning pointer to current FDE
  Format_description_log_event *get_fde();

  /// Mark that fetching is successfully done, even if transaction was
  /// not fully read
  void set_fetching_done();

  /// Appends a new fetchable batch for this transaction.
  void append_batch(Event_set_fetchable_ptr batch);

  /// Seals transaction metadata stream. No more batches will be appended.
  void set_fetching_complete();
  /// Updates the maximum event length published for this transaction.
  void update_max_event_length(std::size_t event_length);

  /// Marks transaction metadata stream as truncated.
  void set_fetching_truncated();

 private:
  /// Waits until a current batch becomes available or fetching reaches a
  /// terminal state.
  /// @return Current batch pointer or nullptr in terminal state.
  Event_set_fetchable *wait_for_current_batch();
  /// Advances to the next batch if the current one has finished.
  /// @param event_batch Current batch being processed.
  /// @retval true The whole transaction finished.
  /// @retval false There may still be more events or batches to fetch.
  bool advance_finished_batch_unsafe(Event_set_fetchable *event_batch);
  /// Iterates over event sets, picking a correct batch
  std::optional<Event_set_fetchable *> get_current_event_batch_unsafe();
  /// Detailed error message if error appears
  std::string m_error_message;
  /// Internal status of event
  Return_status m_status;
  /// True when the consumer-side fetch stream reached a terminal state.
  bool m_is_done{false};
  /// True when reader has finished appending batches.
  bool m_is_complete{false};
  /// Maximum event length published for this transaction.
  std::atomic<std::size_t> m_max_event_length{0};
  /// True when reader marked metadata stream as truncated.
  std::atomic<bool> m_is_truncated{false};
  /// Guard for concurrent producer (reader) and consumer (worker).
  mutable std::mutex m_mutex;
  /// Notification for appended batches / terminal state updates.
  std::condition_variable m_cv;
  /// Object used to fetch transaction events from the storage
  Event_set_fetchable_list m_event_set_batches;
  /// Currently processed event batch iterator
  Event_set_fetchable_list::iterator m_current_batch_it;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_FETCHABLE_TRANSACTION_H
