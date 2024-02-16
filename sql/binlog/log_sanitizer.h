// Copyright (c) 2022, 2024, Oracle and/or its affiliates.
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

#ifndef BINLOG_LOG_SANITIZER_H
#define BINLOG_LOG_SANITIZER_H

#include <functional>
#include "mysql/binlog/event/binlog_event.h"
#include "sql/binlog.h"
#include "sql/binlog/decompressing_event_object_istream.h"  // binlog::Decompressing_event_object_istream
#include "sql/binlog_ostream.h"  // binlog::tools::Iterator
#include "sql/binlog_reader.h"   // Binlog_file_reader
#include "sql/log_event.h"       // Log_event
#include "sql/xa.h"              // XID

namespace binlog {

/// @brief Class used to recover binary / relay log file
/// @details This base class is responsible for finding the last valid
/// position of a relay log / binary log file, meaning, the position of the
/// last finished event which occurs outside of transaction boundary.
/// Validation starts when first reliable position has been found, i.e.:
/// - source rotation event
/// - source FDE
/// - source STOP event
/// - first finished transaction:
///    * Query log event with: COMMIT / ROLLBACK / XA COMMIT / XA ROLLBACK /
///      atomic DDL
///    * XID Log event
/// Validation ends at the end of the binlog file / relay log file or in case
/// further reading is not possible.
/// Binary log recovery:
///   Binary log file always start with an FDE which is the first and valid
///   position within a file. Binary log files are never removed by a log
///   sanitizer.
/// Relay log recovery:
///   If no valid position has been found in any of the relay log files,
///   Log sanitizer will keep all of the relay log files.
///   In case a valid position has been found in any of the first relay log
///   files, relay log files that do not contain a valid position outside of
///   a transaction boundary, will be removed.
class Log_sanitizer {
 public:
  /// @brief Ctor
  Log_sanitizer();

  /// @brief Dtor
  virtual ~Log_sanitizer() = default;

  /// @brief Retrieves the position of the last binlog/relay log event that
  /// ended a transaction or position after the RLE/FDE/SE that comes from
  /// the source
  /// @return The position of the last binlog event that ended a transaction
  my_off_t get_valid_pos() const;

  /// @brief Retrieves the last valid source position of an event in
  /// read from the binary log / relay log file, which may be:
  /// - source position of the event ending a transaction
  /// - source position written in the source RLE
  /// @return The position of the last binlog event that ended a transaction
  /// and indicator whether this position is valid
  std::pair<my_off_t, bool> get_valid_source_pos() const;

  /// @brief Retrieves the updated name of the binlog source file
  /// @return Updated source file or empty string; indicator equal to true in
  /// case filename is valid
  std::pair<std::string, bool> get_valid_source_file() const;

  /// @brief Retrieves whether or not the log was correctly processed in full.
  /// @return true if the log processing ended with errors, false otherwise.
  bool is_log_malformed() const;

  /// @brief Retrieves the textual representation of the encontered failure, if
  /// any.
  /// @return the string containing the textual representation of the failure,
  /// an empty string otherwise.
  std::string const &get_failure_message() const;

  std::string get_valid_file() const { return m_valid_file; }

  /// @brief Checks whether a valid sanitized log file needs truncation of
  /// the last, partially written transaction or events that cannot be
  /// safely read
  /// @return true in case log file needs to be truncated, false
  /// otherwise
  bool is_log_truncation_needed() const;

  /// @brief Checks whether the fatal error occurred during log sanitization
  /// (OOM / decompression error which we cannot handle)
  /// @return true in case fatal error occurred, false otherwise
  bool is_fatal_error() const;

 protected:
  /// @brief Function used to obtain memory key for derived classes
  /// @returns Reference to a memory key
  virtual PSI_memory_key &get_memory_key() const = 0;

  /// @brief This function goes through the opened file and searches for
  /// a valid position in a binary log file. It also gathers
  /// information about XA transactions which will be used during the
  /// binary log recovery
  /// @param reader Log reader, must be opened
  template <class Type_reader>
  void process_logs(Type_reader &reader);

  /// @brief This function goes iterates over the relay log files
  /// in the 'list_of_files' container, starting from the most recent one.
  /// It gathers information about XA transactions and performs
  /// a small validation of the log files. Validation starts
  /// in case a first reliable position has been found (FDE/RLE/SE from the
  /// source or the end of a transaction), and proceeds till the end of file
  /// or until a read error has occurred.
  /// In case a valid position has been found within a file,
  /// relay log files that were created after this file will be removed.
  /// In case no valid position has been found within a file, sanitizer will
  /// iterate over events in the previous (older) relay log file.
  /// In case no valid position has been found in any of the files listed in
  /// the 'list_of_files' container, relay log files won't be removed. It may
  /// happen e.g. in case we cannot decrypt events.
  /// @param reader Relay log file reader object
  /// @param list_of_files The list of relay logs we know, obtained
  /// from the relay log index
  /// @param log MYSQL_BIN_LOG object used to manipulate relay log files
  template <class Type_reader>
  void process_logs(Type_reader &reader,
                    const std::list<std::string> &list_of_files,
                    MYSQL_BIN_LOG &log);

  /// @brief This function will obtain the list of relay log files using the
  /// object of MYSQL_BIN_LOG class and iterate over them to find the last
  /// valid position within a relay log file. It will remove relay log files
  /// that contain only parts of the last, partially written transaction
  /// @param reader Relay log file reader object
  /// @param log MYSQL_BIN_LOG object used to manipulate relay log files
  template <class Type_reader>
  void process_logs(Type_reader &reader, MYSQL_BIN_LOG &log);

  /// @brief Reads and validates one log file
  /// @param[in] filename Name of the log file to process
  /// @param[in] reader Reference to reader able to read processed log
  /// file
  /// @returns true if processed log contains a valid log position outside
  /// of transaction boundaries
  template <class Type_reader>
  bool process_one_log(Type_reader &reader, const std::string &filename);

  /// @brief Indicates whether validation has started.
  /// In case of relay log sanitization, we start validation
  /// when we are sure that we are at transaction boundary and we are able
  /// to recover source position, meaning, when we detect:
  /// - first encountered Rotation Event, that comes from the source
  /// - end of a transaction (Xid event, QLE containing
  ///   COMMIT/ROLLBACK/XA COMMIT/XA ROLLBACK)
  /// - an atomic DDL transaction
  /// Since binary logs always start at transaction boundary, when doing
  /// a binary log recovery, we start validation right away.
  /// By default, we are assuming that we are in the binary log recovery
  /// procedure
  bool m_validation_started{true};

  /// Position of the last binlog/relay log event that ended a transaction
  my_off_t m_valid_pos{0};
  /// Position of the last binlog event that ended a transaction (source
  /// position which corresponds to m_valid_pos)
  my_off_t m_valid_source_pos{0};
  /// Currently processed binlog file set in case source rotation
  /// event is encountered
  std::string m_valid_source_file{""};
  ///  Last log file containing finished transaction
  std::string m_valid_file{""};
  ///  Whether or not the event being processed is within a transaction
  bool m_in_transaction{false};
  ///  Whether or not the binary log is malformed/corrupted or error occurred
  bool m_is_malformed{false};
  ///  Whether or not the binary log has a fatal error
  bool m_fatal_error{false};
  ///  Textual representation of the encountered failure
  std::string m_failure_message{""};
  ///  Memory pool to use for the XID lists
  MEM_ROOT m_mem_root;
  ///  Memory pool allocator to use with the normal transaction list
  Mem_root_allocator<my_xid> m_set_alloc;
  ///  Memory pool allocator to use with the XA transaction list
  Mem_root_allocator<std::pair<const XID, XID_STATE::xa_states>> m_map_alloc;
  ///  List of normal transactions fully written to the binary log
  Xid_commit_list m_internal_xids;
  ///  List of XA transactions and states that appear in the binary log
  Xa_state_list::list m_external_xids;

  /// Information on whether log needs to be truncated, i.e.
  /// log is not ending at transaction boundary or we cannot read it till the
  /// end
  bool m_is_log_truncation_needed{false};

  /// Indicator on whether a valid position has been found in the log file
  bool m_has_valid_pos{false};

  /// Indicator on whether a valid source position has been found in the log
  /// file
  bool m_has_valid_source_pos{false};

  /// Last opened file size
  my_off_t m_last_file_size{0};

  /// @brief Invoked when a `Query_log_event` is read from the binary log file
  /// reader.
  /// @details The underlying query string is inspected to determine if the
  /// SQL command starts or ends a transaction. The following commands are
  /// searched for:
  /// - BEGIN
  /// - COMMIT
  /// - ROLLBACK
  /// - DDL
  /// - XA START
  /// - XA COMMIT
  /// - XA ROLLBACK
  /// Check below for the description of the action that is taken for each.
  /// @param ev The `Query_log_event` to process
  void process_query_event(Query_log_event const &ev);

  /// @brief Invoked when a `Xid_log_event` is read from the binary log file
  /// reader.
  /// @details Actions taken to process the event:
  /// - If `m_in_transaction` flag is set to false, `m_is_malformed` is set
  ///   to true, indicating that the binary log is malformed.
  /// - The `m_in_transaction` flag is set to false, indicating that the
  ///   event ends a transaction.
  /// - The XID of the transaction is extracted and added to the list of
  ///   internally coordinated transactions `m_internal_xids`.
  /// - If the XID already exists in the list, `m_is_malformed` is set to
  ///   true, indicating that the binary log is malformed.
  /// @param ev The `Xid_log_event` to process
  void process_xid_event(Xid_log_event const &ev);

  /// @brief Invoked when a `XA_prepare_log_event` is read from the binary log
  /// file reader.
  /// @details Actions taken to process the event:
  /// - If `m_in_transaction` flag is set to false, `m_is_malformed` is set
  ///   to true, indicating that the binary log is malformed.
  /// - The `m_in_transaction` flag is set to false, indicating that the
  ///   event ends a transaction.
  /// - The XID of the transaction is extracted and added to the list of
  ///   externally coordinated transactions `m_external_xids`, along side the
  ///   state COMMITTED if the event represents an `XA COMMIT ONE_PHASE` or
  ///   PREPARED if not.
  /// - If the XID already exists in the list associated with a state other
  ///   than `COMMITTED` or `ROLLEDBACK`, `m_is_malformed` is set to true,
  ///   indicating that the binary log is malformed.
  /// @param ev The `XA_prepare_log_event` to process
  void process_xa_prepare_event(XA_prepare_log_event const &ev);

  /// @brief Invoked when a `BEGIN` or an `XA START' is found in a
  /// `Query_log_event`.
  /// @details Actions taken to process the statement:
  /// - If `m_in_transaction` flag is set to true, `m_is_malformed` is set
  ///   to true, indicating that the binary log is malformed.
  /// - The `m_in_transaction` flag is set to true, indicating that the
  ///   event starts a transaction.
  void process_start();

  /// @brief Invoked when a `COMMIT` is found in a `Query_log_event`.
  /// @details Actions taken to process the statement:
  /// - If `m_in_transaction` flag is set to false, `m_is_malformed` is set
  ///   to true, indicating that the binary log is malformed.
  /// - The `m_in_transaction` flag is set to false, indicating that the
  ///   event starts a transaction.
  void process_commit();

  /// @brief Invoked when a `ROLLBACK` is found in a `Query_log_event`.
  /// @details Actions taken to process the statement:
  /// - If `m_in_transaction` flag is set to false, `m_is_malformed` is set
  ///   to true, indicating that the binary log is malformed.
  /// - The `m_in_transaction` flag is set to false, indicating that the
  ///   event starts a transaction.
  void process_rollback();

  /// @brief Invoked when a DDL is found in a `Query_log_event`.
  /// @details Actions taken to process the statement:
  /// - If `m_in_transaction` flag is set to true, `m_is_malformed` is set
  ///   to true, indicating that the binary log is malformed.
  /// - The XID of the transaction is extracted and added to the list of
  ///   internally coordinated transactions `m_internal_xids`.
  /// - If the XID already exists in the list, `m_is_malformed` is set to
  ///   true, indicating that the binary log is malformed.
  /// @param ev The `Query_log_event` to process
  void process_atomic_ddl(Query_log_event const &ev);

  /// @brief Invoked when an `XA COMMIT` is found in a `Query_log_event`.
  /// @details Actions taken to process the statement:
  /// - If `m_in_transaction` flag is set to true, `m_is_malformed` is set
  ///   to true, indicating that the binary log is malformed.
  /// - The `m_in_transaction` flag is set to false, indicating that the
  ///   event ends a transaction.
  /// - The XID of the transaction is extracted and added to the list of
  ///   externally coordinated transactions `m_external_xids`, alongside the
  ///   state COMMITTED.
  /// - If the XID already exists in the list associated with a state other
  ///   than `PREPARED`, `m_is_malformed` is set to true, indicating that the
  ///   binary log is malformed.
  /// @param query The query string to process
  void process_xa_commit(std::string const &query);

  /// @brief Invoked when an `XA ROLLBACK` is found in a `Query_log_event`.
  /// @details Actions taken to process the statement:
  /// - If `m_in_transaction` flag is set to true, `m_is_malformed` is set
  ///   to true, indicating that the binary log is malformed.
  /// - The `m_in_transaction` flag is set to false, indicating that the
  ///   event ends a transaction.
  /// - The XID of the transaction is extracted and added to the list of
  ///   externally coordinated transactions `m_external_xids`, along side the
  ///   state ROLLEDBACK.
  /// - If the XID already exists in the list associated with a state other
  ///   than `PREPARED`, `m_is_malformed` is set to true, indicating that the
  ///   binary log is malformed.
  /// @param query The query string to process
  void process_xa_rollback(std::string const &query);

  /// @brief Parses the provided string for an XID and adds it to the externally
  /// coordinated transactions map, along side the provided state.
  /// @param query The query to search and retrieve the XID from
  /// @param state The state to add to the map, along side the XID
  void add_external_xid(std::string const &query,
                        enum_ha_recover_xa_state state);
};

}  // namespace binlog

#include "sql/binlog/log_sanitizer_impl.hpp"

#endif  // BINLOG_LOG_SANITIZER_H
