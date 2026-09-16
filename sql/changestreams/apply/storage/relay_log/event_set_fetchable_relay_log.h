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

#ifndef MYSQL_CSA_EVENT_SET_FETCHABLE_RELAY_LOG_H
#define MYSQL_CSA_EVENT_SET_FETCHABLE_RELAY_LOG_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "mysql/utils/return_status.h"
#include "sql/binlog/decompressing_event_object_istream.h"  // Decompressing_event_object_istream
#include "sql/binlog_reader.h"  // Relaylog_file_reader
#include "sql/changestreams/apply/storage/common/event_set_fetchable.h"
#include "sql/changestreams/apply/storage/relay_log/relay_log_deleter.h"

namespace mysql::csa {

/// @brief Implementation of Event_set_fetchable that fetches events from the
/// relay log.
///
/// If the stream is compressed, the fetching function will perform
/// decompression. Objects of this class are created by the relay log reader and
/// used to fetch consecutive parts of a transaction (relay log events).
/// Typically, a transaction will contain as many event sets as the number of
/// files it spans.
/// @note Each object initializes its own reader and sets it to the position of
/// the first event metadata it contains.
class Event_set_fetchable_relay_log : public Event_set_fetchable {
 public:
  /// @brief Shared pointer to a Log_event.
  using Log_event_ptr = std::shared_ptr<Log_event>;
  /// @brief Type alias for return status.
  using Return_status = mysql::utils::Return_status;
  /// @brief Type alias for the decompressing event stream.
  using Stream_type = ::binlog::Decompressing_event_object_istream;
  /// @brief Unique pointer to the decompressing stream.
  using Stream_ptr = std::unique_ptr<Stream_type>;

  /// @brief Constructs an Event_set_fetchable_relay_log with relay log
  /// coordinates.
  ///
  /// The coordinates must remain valid until the transaction is applied or
  /// deemed non-appliable. Uses a relay log deleter handler to ensure the file
  /// is not deleted while in use.
  ///
  /// @param filename The name of the relay log file.
  /// @param start_file_pos The starting position in the file.
  /// @param end_file_pos The "end" position for this event set.
  /// @param deleter Handle to the relay log deleter.
  /// @param checksum_validation Flag to enable checksum validation.
  /// @param is_trx Flag indicating if this set represents a transaction.
  /// @param fde Shared pointer to FDE for this event set.
  /// @param streaming_open If true, this set is stream-open and accepts
  ///        incremental end position updates.
  Event_set_fetchable_relay_log(std::string filename,
                                std::size_t start_file_pos,
                                std::size_t end_file_pos,
                                Relay_log_deleter_handle deleter,
                                bool checksum_validation, bool is_trx,
                                Log_event_ptr fde, bool streaming_open = false);

  /// @brief Fetches the next event from the initialized internal stream.
  ///
  /// @return Managed_event or empty optional in case of stream end or error.
  ///         Error can be checked with 'is_error'.
  bool wait_next() override;
  std::optional<Managed_event> fetch_next() override;

  /// @brief Retrieves the error message if any error occurred.
  ///
  /// @return Const reference to the error message string.
  const std::string &get_error_str() const override;

  /// @brief Checks if the fetchable stream has finished without error.
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

  /// @brief Resets the state to allow fetching again, clearing error state.
  ///
  /// Leaves the reader closed to conserve file descriptors.
  /// @param reset_events When true events states need to be reset
  void reset(bool reset_events) override;

  /// @brief Callback notifying that task was executed successfully.
  void set_success() override;

  /// @brief Destructor.
  virtual ~Event_set_fetchable_relay_log() override;

  /// @brief Returns a string representation of this batch's information.
  ///
  /// @return String containing basic information about the batch.
  std::string to_string() const;

  /// @brief Obtains non-owning pointer to current transaction FDE
  /// @return Non-owning pointer to FDE
  Fde_ptr get_fde() override;

  /// @brief Appends one published event boundary for stream-open batch.
  /// @param end_file_pos End position (exclusive) for the next available event.
  /// @param seal_after When true, the batch is sealed together with publish.
  void append_event_end(std::size_t end_file_pos, bool seal_after = false);

  /// @brief Seals stream-open batch. No more events will be appended.
  void seal_stream();

  /// @brief Marks stream-open batch as truncated and wakes blocked readers.
  void set_stream_truncated();

 private:
  /// @brief Checks if currently decompressing the internal stream (TPLE).
  ///
  /// @return true if decompressing, false otherwise.
  bool decompressing() const;

  /// @brief Fetches the next event from the stream.
  ///
  /// @return Managed_event or empty optional in case of stream end or error.
  std::optional<Managed_event> fetch_from_stream();

  /// @brief Waits until stream has at least one more event available.
  ///
  /// @retval true At least one more event can be fetched.
  /// @retval false Stream is sealed/truncated/error and no more fetch is
  ///         possible.
  bool wait_for_event_availability();

  /// @brief Safely closes the reader if it is open.
  void safe_close_reader();
  /// @brief Safely opens the reader (closes if already open and reopens).
  void safe_open_reader();
  /// @brief Starts reading from the file by opening it and creating the input
  /// stream.
  void start_reading();

  /// @brief Flag indicating if the reader is open.
  bool m_is_initialized = false;
  /// @brief Flag indicating if processing is done (finished or error).
  bool m_is_done = false;
  /// @brief Name of the relay log file to read from.
  std::string m_file_name{""};
  /// @brief Starting file position for reading.
  std::size_t m_start_file_pos{0};
  /// @brief Detailed error message if any.
  std::string m_failure_msg{""};
  /// @brief Status of the object.
  Return_status m_status;
  /// @brief Handle to the relay log deleter.
  Relay_log_deleter_handle m_delete_file_handle;
  /// @brief Decompressing stream object for handling compressed events.
  Stream_ptr m_input_stream;
  /// @brief Relay log file reader used by the decompressing stream.
  Relaylog_file_reader m_reader;
  /// @brief Flag indicating if this is a transaction.
  bool m_is_trx{false};
  /// @brief Flag indicating if currently decompressing an internal event
  /// (TPLE).
  bool m_decompressing{false};
  /// @brief owning pointer to FDE.
  Log_event_ptr m_fde_base{};
  /// @brief non-owning pointer to FDE.
  Fde_ptr m_fde{};

  /// @brief Stream synchronization state.
  mutable std::mutex m_stream_mutex;
  std::condition_variable m_stream_cv;
  std::size_t m_published_end_file_pos{0};
  bool m_stream_open{false};
  bool m_stream_sealed{true};
  bool m_stream_truncated{false};
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_EVENT_SET_FETCHABLE_RELAY_LOG_H
