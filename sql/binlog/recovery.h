/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef BINLOG_RECOVERY_H_INCLUDED
#define BINLOG_RECOVERY_H_INCLUDED

#include "sql/binlog/global.h"
#include "sql/binlog/log_sanitizer.h"
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
class Binlog_recovery : public Log_sanitizer {
 public:
  /**
    Class constructor.

    @param binlog_file_reader The already instantiated and initialized file
                              reader for the last available binary log
                              file.
   */
  Binlog_recovery(Binlog_file_reader &binlog_file_reader);
  ~Binlog_recovery() override = default;

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

 protected:
  /// @brief Function used to obtain memory key for derived classes
  /// @returns Reference to a memory key
  PSI_memory_key &get_memory_key() const override {
    return key_memory_recovery;
  }

 private:
  /** File reader for the last available binary log file */
  Binlog_file_reader &m_reader;
  /** Whether or not the recovery in the storage engines failed */
  bool m_no_engine_recovery{false};
};
}  // namespace binlog

#endif  // BINLOG_RECOVERY_H_INCLUDED
