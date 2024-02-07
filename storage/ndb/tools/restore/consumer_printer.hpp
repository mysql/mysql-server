/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef CONSUMER_PRINTER_HPP
#define CONSUMER_PRINTER_HPP

#include "consumer.hpp"

class BackupPrinter : public BackupConsumer {
  NdbOut &m_ndbout;

 public:
  BackupPrinter(NdbOut &out = ndbout) : m_ndbout(out) {
    m_print = false;
    m_print_log = false;
    m_print_sql_log = false;
    m_print_data = false;
    m_print_meta = false;
    m_logCount = 0;
    m_dataCount = 0;
  }

  bool table(const TableS &) override;
#ifdef USE_MYSQL
  virtual bool table(const TableS &, MYSQL *mysqlp);
#endif
  bool tuple(const TupleS &, Uint32 fragId) override;
  bool logEntry(const LogEntry &) override;
  void endOfTuples() override {}
  void endOfLogEntrys() override;
  bool update_apply_status(const RestoreMetaData &metaData,
                           bool snapshotstart) override;
  bool delete_epoch_tuple() override;
  bool m_print;
  bool m_print_log;
  bool m_print_sql_log;
  bool m_print_data;
  bool m_print_meta;
  Uint32 m_logCount;
  Uint32 m_dataCount;
};

#endif
