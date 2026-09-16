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

#ifndef MYSQL_CSA_EVENT_SET_FETCHABLE_CACHE_H
#define MYSQL_CSA_EVENT_SET_FETCHABLE_CACHE_H

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "mysql/utils/return_status.h"
#include "sql/binlog/decompressing_event_object_istream.h"  // Decompressing_event_object_istream
#include "sql/changestreams/apply/storage/common/event_set_fetchable.h"
#include "sql/changestreams/apply/storage/relay_log/cached_event.h"
#include "sql/changestreams/apply/storage/relay_log/cached_event_payload.h"
#include "sql/changestreams/apply/storage/relay_log/ireader_event.h"
#include "sql/changestreams/apply/storage/relay_log/relay_log_deleter.h"

namespace mysql::csa {

/// @brief Empty implementation of Event_set_fetchable that does not fetch
/// events from the storage as they were already fetched. It returns cached
/// event objects instead.
class Event_set_fetchable_cache : public Event_set_fetchable {
 public:
  /// @brief Shared pointer to a Log_event.
  using Log_event_ptr = std::shared_ptr<Log_event>;
  /// @brief Type alias for return status.
  using Return_status = mysql::utils::Return_status;
  /// @brief Vector of Log_event_ptr representing a set of events.
  using Event_set_type = std::vector<IReader_event_ptr>;
  /// @brief Type alias for the decompressing event stream.
  using Stream_type = ::binlog::Decompressing_event_object_istream;
  /// @brief Unique pointer to the decompressing stream.
  using Stream_ptr = std::unique_ptr<Stream_type>;

  /// @brief Constructs an Event_set_fetchable_cache with pre-fetched events.
  ///
  /// @param events The vector of events to cache (moved into the object).
  /// @param is_trx Flag indicating if this set represents a transaction.
  /// @param fde Shared pointer to the Format_description_event.
  /// @param delete_file_handle Handle to the relay log deleter.
  /// @param streaming_open If true, events may be appended concurrently.
  Event_set_fetchable_cache(Event_set_type &&events, bool is_trx,
                            Log_event_ptr fde,
                            Relay_log_deleter_handle delete_file_handle,
                            bool streaming_open = false);

  /// @brief Fetches the next event from the internal cache.
  ///
  /// @return Managed_event or empty optional in case of stream end or error.
  ///         Error can be checked with 'is_error'. Decompresses events if
  ///         necessary.
  bool wait_next() override;
  std::optional<Managed_event> fetch_next() override;

  /// @brief Retrieves the error message if any error occurred.
  ///
  /// @return Const reference to the error message string.
  const std::string &get_error_str() const override;

  /// @brief Checks if the fetchable stream has finished processing without
  /// error.
  ///
  /// @return true if finished without error, false otherwise.
  bool is_done() const override;

  /// @brief Checks if an error occurred in the fetchable stream.
  ///
  /// @return true if an error occurred, false otherwise.
  bool is_error() const override;

  /// @brief Checks if this event set represents a transaction.
  ///
  /// @return true if it contains a transaction, false otherwise.
  bool is_trx() const override;

  /// @brief Resets the state to allow fetching the event set again, clearing
  /// any error state.
  /// @param reset_events When true events states need to be reset
  void reset(bool reset_events) override;

  /// @brief Destructor.
  virtual ~Event_set_fetchable_cache() override;

  /// @brief Obtains non-owning pointer to current transaction FDE
  /// @return Non-owning pointer to FDE
  Fde_ptr get_fde() override;

  /// @brief Callback notifying that task was executed successfully
  void set_success() override;

  /// @brief Appends one event to a stream-open cache batch.
  /// @param event Event to append
  /// @param seal_after When true, the batch is sealed together with publish.
  void append_event(IReader_event_ptr event, bool seal_after = false);

  /// @brief Seals stream-open cache batch.
  void seal_stream();

  /// @brief Marks stream-open cache batch as truncated.
  void set_stream_truncated();

 private:
  /// @brief Cached vector of events.
  Event_set_type m_events;

  /// @brief Decompresses and returns the next event from the TPLE stream.
  ///
  /// @return Optional Log_event_ptr if successful, empty if failed or stream
  /// ended.
  std::optional<Log_event_ptr> decompress();
  /// @brief Helper to deinitialize the decompression stream and update status.
  void end_decompression();
  /// @brief Helper to initialize the decompression stream and update status.
  void start_decompression();

  /// @brief Flag indicating if processing is done (finished or error).
  bool m_is_done = false;
  /// @brief Index of the next event to fetch.
  std::size_t m_event_id{0};
  /// @brief Detailed error message if any.
  std::string m_failure_msg{""};
  /// @brief Status of the object.
  Return_status m_status;
  /// @brief Flag indicating if this is a transaction.
  bool m_is_trx{false};
  /// @brief Flag indicating if currently decompressing a TPLE.
  bool m_decompressing{false};
  /// @brief Decompressing stream created from TPLE if any.
  Stream_ptr m_decompressing_stream{};
  /// @brief Non-owning pointer to compressed event casted to
  /// Transaction_payload_log_event.
  Transaction_payload_log_event *m_compressed_event_ptr{nullptr};
  /// @brief Compressed event used during decompression.
  Log_event_ptr m_compressed_event{};
  /// @brief Owning pointer to Format_description_event.
  Log_event_ptr m_fde_base{};
  /// @brief Non-owning pointer to Format_description_event.
  Fde_ptr m_fde{};
  /// @brief Handle to relay log deleter, relay log will be removed when
  /// last living reference to this file is released.
  Relay_log_deleter_handle m_delete_file_handle;

  /// @brief Stream synchronization state.
  mutable std::mutex m_stream_mutex;
  std::condition_variable m_stream_cv;
  bool m_stream_open{false};
  bool m_stream_sealed{true};
  bool m_stream_truncated{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_EVENT_SET_FETCHABLE_CACHE_H
