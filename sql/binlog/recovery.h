/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef BINLOG_RECOVERY_H_INCLUDED
#define BINLOG_RECOVERY_H_INCLUDED

#include "sql/binlog/global.h"
#include "sql/binlog_ostream.h"  // binlog::tools::Iterator
#include "sql/binlog_reader.h"   // Binlog_file_reader
#include "sql/log_event.h"       // Log_event
#include "sql/xa.h"              // XID

namespace binlog {
/**
  Recovers from last crashed binlog at server start.

  After a crash, storage engines may contain transactions that are prepared
  but not committed (in theory any engine, in practice InnoDB).  This
  classe's methods use the binary log as the source of truth to determine
  which of these transactions should be committed and which should be
  rolled back.

  The `Binlog::recovery()` method collects the following from the last
  available binary log:
  - the list of internally coordinated transactions (normal) that are
    completely written to the binary log.
  - the list of externally coordinated transactions (XA) that appear in the
    binary log, along the state those transactions are in.

  The list of XIDs of all internally coordinated transactions that are
  completely written to the binary log is passed to the storage engines
  through the ha_recover function in the handler interface. This tells the
  storage engines to commit all prepared transactions that are in the set,
  and to roll back all prepared transactions that are not in the set.

  The list of XIDs of all externally coordinated transactions that appear
  in the binary log, along with the state they are in, is passed to the
  storage engines through the ha_recover function in the handler
  interface. The storage engine will determine if the transaction is to be
  kept at PREPARE, is to be COMMITTED or ROLLED BACK, in accordance with:
  the state that is provided in the list; the internal storage engine state
  for the transaction.
*/
class Binlog_recovery {
 public:
  /**
    Class constructor.

    @param binlog_file_reader The already instantiated and initialized file
                              reader for the last available binary log
                              file.
   */
  Binlog_recovery(Binlog_file_reader &binlog_file_reader);
  virtual ~Binlog_recovery() = default;

  /**
    Retrieves the position of the last binlog event that ended a
    transaction.

    @return The position of the last binlog event that ended a
            transaction.
   */
  my_off_t get_valid_pos() const;
  /**
    Retrieves whether or not the recovery process ended successfully.

    @see Binlog_recovery::is_binlog_malformed()
    @see Binlog_recovery::has_engine_recovery_failed()

    @return true if the recovery process ended with errors, false
            otherwise.
   */
  bool has_failures() const;
  /**
    Retrieves whether or not the binary log was correctly processed in
    full.

    @return true if the binary log processing ended with errors, false
            otherwise.
   */
  bool is_binlog_malformed() const;
  /**
    Retrieves whether or not the storage engines XA recovery process
    completed successfully.

    @return false if the storge engines completed the XA recovery process
            successfully, true otherwise.
   */
  bool has_engine_recovery_failed() const;
  /**
    Retrieves the textual representation of the encontered failure, if any.

    @return the string containing the textual representation of the failure,
            an empty string otherwise.
   */
  std::string const &get_failure_message() const;
  /**
    Uses the provided binary log file reader to inspect the binary log and
    extract transaction information.

    The following is collected from the provided binlog file reader:
    - the list of internally coordinated transactions (normal) that are
      completely written to the binary log.
    - the list of externally coordinated transactions (XA) that appear in
      the binary log, along the state those transactions are in.

    The list of XIDs of all internally coordinated transactions that are
    completely written to the binary log is passed to the storage engines
    through the ha_recover function in the handler interface. This tells the
    storage engines to commit all prepared transactions that are in the set,
    and to roll back all prepared transactions that are not in the set.

    The list of XIDs of all externally coordinated transactions that appear
    in the binary log, along with the state they are in, is passed to the
    storage engines through the ha_recover function in the handler
    interface. The storage engine will determine if the transaction is to be
    kept at PREPARE, is to be COMMITTED or ROLLED BACK, in accordance with:
    the state that is provided in the list; the internal storage engine state
    for the transaction.

    After `recover()` returns, `has_failures()` should be invoked to
    determine if the recover process ended successfully. Additionally,
    `is_binlog_malformed()` and `has_engine_recovery_failed()` can be
    invoked to determine the type of error that occurred.

    @return This instance's reference, for chaining purposes.
   */
  Binlog_recovery &recover();

 private:
  /** File reader for the last available binary log file */
  Binlog_file_reader &m_reader;
  /** Position of the last binlog event that ended a transaction */
  my_off_t m_valid_pos{0};
  /** Whether or not the event being processed is within a transaction */
  bool m_in_transaction{false};
  /** Whether or not the binary log is malformed/corrupted */
  bool m_is_malformed{false};
  /** Whether or not the recovery in the storage engines failed */
  bool m_no_engine_recovery{false};
  /** Textual representation of the encountered failure */
  std::string m_failure_message{""};
  /** Memory pool to use for the XID lists */
  MEM_ROOT m_mem_root;
  /** Memory pool allocator to use with the normal transaction list */
  Mem_root_allocator<my_xid> m_set_alloc;
  /** Memory pool allocator to use with the XA transaction list */
  Mem_root_allocator<std::pair<const XID, XID_STATE::xa_states>> m_map_alloc;
  /** List of normal transactions fully written to the binary log */
  Xid_commit_list m_internal_xids;
  /** List of XA transactions and states that appear in the binary log */
  Xa_state_list::list m_external_xids;

  /**
    Invoked when a `Query_log_event` is read from the binary log file
    reader. The underlying query string is inspected to determine if the
    SQL command starts or ends a transaction. The following commands are
    searched for:
    - BEGIN
    - COMMIT
    - ROLLBACK
    - DDL
    - XA START
    - XA COMMIT
    - XA ROLLBACK

    Check below for the description of the action that is taken for each.

    @param ev The `Query_log_event` to process
   */
  void process_query_event(Query_log_event const &ev);
  /**
    Invoked when a `Xid_log_event` is read from the binary log file
    reader.

    Actions taken to process the event:
    - If `m_in_transaction` flag is set to false, `m_is_malformed` is set
      to true, indicating that the binary log is malformed.
    - The `m_in_transaction` flag is set to false, indicating that the
      event ends a transaction.
    - The XID of the transaction is extracted and added to the list of
      internally coordinated transactions `m_internal_xids`.
    - If the XID already exists in the list, `m_is_malformed` is set to
      true, indicating that the binary log is malformed.

    @param ev The `Xid_log_event` to process
   */
  void process_xid_event(Xid_log_event const &ev);
  /**
    Invoked when a `XA_prepare_log_event` is read from the binary log file
    reader.

    Actions taken to process the event:
    - If `m_in_transaction` flag is set to false, `m_is_malformed` is set
      to true, indicating that the binary log is malformed.
    - The `m_in_transaction` flag is set to false, indicating that the
      event ends a transaction.
    - The XID of the transaction is extracted and added to the list of
      externally coordinated transactions `m_external_xids`, along side the
      state COMMITTED if the event represents an `XA COMMIT ONE_PHASE` or
      PREPARED if not.
    - If the XID already exists in the list associated with a state other
      than `COMMITTED` or `ROLLEDBACK`, `m_is_malformed` is set to true,
      indicating that the binary log is malformed.

    @param ev The `XA_prepare_log_event` to process
   */
  void process_xa_prepare_event(XA_prepare_log_event const &ev);
  /**
    Invoked when a `BEGIN` or an `XA START' is found in a
    `Query_log_event`.

    Actions taken to process the statement:
    - If `m_in_transaction` flag is set to true, `m_is_malformed` is set
      to true, indicating that the binary log is malformed.
    - The `m_in_transaction` flag is set to true, indicating that the
      event starts a transaction.
   */
  void process_start();
  /**
    Invoked when a `COMMIT` is found in a `Query_log_event`.

    Actions taken to process the statement:
    - If `m_in_transaction` flag is set to false, `m_is_malformed` is set
      to true, indicating that the binary log is malformed.
    - The `m_in_transaction` flag is set to false, indicating that the
      event starts a transaction.
   */
  void process_commit();
  /**
    Invoked when a `ROLLBACK` is found in a `Query_log_event`.

    Actions taken to process the statement:
    - If `m_in_transaction` flag is set to false, `m_is_malformed` is set
      to true, indicating that the binary log is malformed.
    - The `m_in_transaction` flag is set to false, indicating that the
      event starts a transaction.
   */
  void process_rollback();
  /**
    Invoked when a DDL is found in a `Query_log_event`.

    Actions taken to process the statement:
    - If `m_in_transaction` flag is set to true, `m_is_malformed` is set
      to true, indicating that the binary log is malformed.
    - The XID of the transaction is extracted and added to the list of
      internally coordinated transactions `m_internal_xids`.
    - If the XID already exists in the list, `m_is_malformed` is set to
      true, indicating that the binary log is malformed.

    @param ev The `Query_log_event` to process
   */
  void process_atomic_ddl(Query_log_event const &ev);
  /**
    Invoked when an `XA COMMIT` is found in a `Query_log_event`.

    Actions taken to process the statement:
    - If `m_in_transaction` flag is set to true, `m_is_malformed` is set
      to true, indicating that the binary log is malformed.
    - The `m_in_transaction` flag is set to false, indicating that the
      event ends a transaction.
    - The XID of the transaction is extracted and added to the list of
      externally coordinated transactions `m_external_xids`, alongside the
      state COMMITTED.
    - If the XID already exists in the list associated with a state other
      than `PREPARED`, `m_is_malformed` is set to true, indicating that the
      binary log is malformed.

    @param query The query string to process
   */
  void process_xa_commit(std::string const &query);
  /**
    Invoked when an `XA ROLLBACK` is found in a `Query_log_event`.

    Actions taken to process the statement:
    - If `m_in_transaction` flag is set to true, `m_is_malformed` is set
      to true, indicating that the binary log is malformed.
    - The `m_in_transaction` flag is set to false, indicating that the
      event ends a transaction.
    - The XID of the transaction is extracted and added to the list of
      externally coordinated transactions `m_external_xids`, along side the
      state ROLLEDBACK.
    - If the XID already exists in the list associated with a state other
      than `PREPARED`, `m_is_malformed` is set to true, indicating that the
      binary log is malformed.

    @param query The query string to process
   */
  void process_xa_rollback(std::string const &query);
  /**
    Parses the provided string for an XID and adds it to the externally
    coordinated transactions map, along side the provided state.

    @param query The query to search and retrieve the XID from
    @param state The state to add to the map, along side the XID
   */
  void add_external_xid(std::string const &query,
                        enum_ha_recover_xa_state state);
};
}  // namespace binlog

#endif  // BINLOG_RECOVERY_H_INCLUDED
