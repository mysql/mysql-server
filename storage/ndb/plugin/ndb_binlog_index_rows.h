/*
   Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef NDB_BINLOG_INDEX_ROWS_H
#define NDB_BINLOG_INDEX_ROWS_H

#include <string>
#include <vector>

#include "my_inttypes.h"

// Encapsulates a list of server_id's, epoch's and their corresponding stats
// which are handled while binlogging of an epoch transaction.
class Ndb_binlog_index_rows {
 public:
  struct Row {
    Row(ulong server_id, ulonglong epoch)
        : orig_server_id(server_id), orig_epoch(epoch) {}
    ulong orig_server_id;
    ulonglong orig_epoch;

    ulong n_inserts{0};
    ulong n_updates{0};
    ulong n_deletes{0};
  };
  using Rows = std::vector<Row>;

  /**
     @brief Reset the list of rows for handling of new epoch
   */
  void init();

  /**
     @brief Find or create a Row for the given server_id/epoch in the list of
     rows describing what has happened during processing of current epoch.

     @param orig_server_id The server_id where change originated
     @param orig_epoch     The epoch when change originated

     @return row for the given server_id/epoch combination
   */
  Row &find_row(uint orig_server_id, ulonglong orig_epoch);

  // Return reference to the row list
  const Rows &get_rows() const { return m_rows; }

  // The number of schemaops (DDL) are counted for the whole epoch
  void inc_schemaops() { m_schemaops++; }
  ulong get_schemaops() const { return m_schemaops; }

  std::string dbug_dump(const char *where,
                        const char *line_separator = "\n") const;

 private:
  ulong m_schemaops{0};

  // List of rows describing what has been processing of current epoch.
  Rows m_rows;
};

#endif
