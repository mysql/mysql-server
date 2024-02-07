/*
  Copyright (c) 2006, 2024, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_binlog_index_rows.h"

#include <cassert>
#include <sstream>

extern bool opt_ndb_log_orig;

void Ndb_binlog_index_rows::init() {
  // Remove rows but keep allocation
  m_rows.clear();

  // Always insert one "empty" row
  m_rows.emplace_back(Row(0, 0));

  // Reset counter
  m_schemaops = 0;
}

Ndb_binlog_index_rows::Row &Ndb_binlog_index_rows::find_row(
    uint orig_server_id, ulonglong orig_epoch) {
  // At least one row should always exist
  assert(!m_rows.empty());

  constexpr ssize_t NOT_FOUND = -1;
  if (opt_ndb_log_orig) {
    ssize_t found_id = NOT_FOUND;
    for (auto row = m_rows.rbegin(); row != m_rows.rend(); row++) {
      if (row->orig_server_id == orig_server_id) {
        // Found matching server_id

        if (row->orig_epoch == 0) {
          // Found the half filled in "empty" row, update and use it
          row->orig_epoch = orig_epoch;
          return *row;
        }

        if (orig_epoch == 0) {
          // Not a new epoch, use this row
          return *row;
        }

        if (found_id == NOT_FOUND) {
          // Remember previous row for orig_server_id
          found_id = std::distance(row, m_rows.rend()) - 1;
        }
      }

      if (row->orig_server_id == 0) {
        // Found the "empty" row, update and use it
        row->orig_epoch = orig_epoch;
        row->orig_server_id = orig_server_id;
        return *row;
      }
    }

    // Add new row at end of vector and thus "most recent" is last, this is for
    // effiency reasons and thus the list has to be iterated in reverse.
    auto &new_row = m_rows.emplace_back(Row(orig_server_id, orig_epoch));
    if (found_id != NOT_FOUND) {
      /*
        If we found row with same server id already
        that row will contain the current stats.
        Copy stats over to new and reset old.
      */
      Row &found_row = m_rows[found_id];
      std::swap(new_row.n_inserts, found_row.n_inserts);
      std::swap(new_row.n_updates, found_row.n_updates);
      std::swap(new_row.n_deletes, found_row.n_deletes);
    }
  }
  return *(m_rows.end() - 1);
}

std::string Ndb_binlog_index_rows::dbug_dump(const char *where,
                                             const char *line_separator) const {
  std::stringstream ss;
  ss << "== Row's " << where << " == " << line_separator;
  ss << "  schemaops: " << m_schemaops << line_separator;
  ss << "  rows: " << m_rows.size() << " [" << line_separator;
  for (auto row = m_rows.rbegin(); row != m_rows.rend(); row++) {
    ss << "    server_id: " << row->orig_server_id
       << ", epoch: " << row->orig_epoch << ", inserts: " << row->n_inserts
       << ", updates: " << row->n_updates << ", deletes: " << row->n_deletes
       << line_separator;
  }
  ss << "  ]" << line_separator;
  return ss.str();
}

#ifdef TEST_NDB_BINLOG_INDEX_ROWS
#include <iostream>

#include "NdbTap.hpp"

bool opt_ndb_log_orig = true;  // Mock

TAPTEST(NdbBinlogIndexRows) {
  {
    Ndb_binlog_index_rows rows;

    rows.init();

    // There is always one row after init()
    OK(rows.get_rows().size() == 1);

    // After init() the row is "empty" and everything is zero
    const auto &row = rows.get_rows()[0];
    OK(row.orig_server_id == 0 && row.orig_epoch == 0 && row.n_inserts == 0 &&
       row.n_updates == 0 && row.n_deletes == 0);
    OK(rows.get_schemaops() == 0);

    // Schema ops counter increment and get
    rows.inc_schemaops();
    OK(rows.get_schemaops() == 1);
    rows.inc_schemaops();
    rows.inc_schemaops();
    rows.inc_schemaops();
    rows.inc_schemaops();
    OK(rows.get_schemaops() == 5);

    // Schema ops counter is reset by init()
    rows.init();
    OK(rows.get_schemaops() == 0);
  }

  // Default case, changes are received from NDB, the server_id is extracted
  // from any_value and an entry is found or created in the list of Row's.
  // There might be more than one MySQL Server (or API) updating rows in NDB and
  // each is tracked indivdually
  {
    constexpr uint server_id1 = 37;

    Ndb_binlog_index_rows rows;
    rows.init();

    {
      auto &row = rows.find_row(server_id1, 0);
      // Still only one row, but NOT empty
      OK(rows.get_rows().size() == 1);
      OK(row.orig_server_id == server_id1);
      OK(row.orig_epoch == 0);
      row.n_inserts = 1;
      row.n_updates = 2;
      row.n_deletes = 3;
    }

    // Another row change from same server, finds same row, update stats
    {
      auto &same_row = rows.find_row(server_id1, 0);
      // Still only one row
      OK(rows.get_rows().size() == 1);
      OK(same_row.orig_server_id == server_id1);
      OK(same_row.orig_epoch == 0);
      OK(same_row.n_inserts == 1);
      OK(same_row.n_updates == 2);
      OK(same_row.n_deletes == 3);
      same_row.n_inserts++;
      same_row.n_updates++;
      same_row.n_deletes++;
    }

    // Row change from another server_id, creates a second row, set some stats
    constexpr uint server_id2 = 38;
    {
      auto &new_row = rows.find_row(server_id2, 0);
      // Another row was created since new server_id
      OK(rows.get_rows().size() == 2);
      OK(new_row.orig_server_id == server_id2);
      OK(new_row.orig_epoch == 0);
      OK(new_row.n_inserts == 0);
      OK(new_row.n_updates == 0);
      OK(new_row.n_deletes == 0);
      new_row.n_inserts = 5;
      new_row.n_updates = 6;
      new_row.n_deletes = 7;
    }

    // Row change from first server_id, finds the first row, updates stats
    {
      auto &first_row = rows.find_row(server_id1, 0);
      OK(rows.get_rows().size() == 2);
      OK(first_row.orig_server_id == server_id1);
      OK(first_row.orig_epoch == 0);
      OK(first_row.n_inserts == 2);
      OK(first_row.n_updates == 3);
      OK(first_row.n_deletes == 4);
      first_row.n_inserts++;
      first_row.n_updates++;
      first_row.n_deletes++;
    }

    // Row change from second server_id, finds second row
    {
      auto &second_row = rows.find_row(server_id2, 0);
      OK(rows.get_rows().size() == 2);
      OK(second_row.orig_server_id == server_id2);
      OK(second_row.orig_epoch == 0);
      OK(second_row.n_inserts == 5);
      OK(second_row.n_updates == 6);
      OK(second_row.n_deletes == 7);
    }

    // Finally init() prepares for new epoch and there is one empty row again
    {
      rows.init();
      OK(rows.get_rows().size() == 1);
      const auto &row = rows.get_rows()[0];
      OK(row.orig_server_id == 0 && row.orig_epoch == 0 && row.n_inserts == 0 &&
         row.n_updates == 0 && row.n_deletes == 0);
      OK(rows.get_schemaops() == 0);
    }
  }

  // Orig_epoch case, this occurs when there are MySQL Server(s) applying
  // replicated changes to NDB, this causes row changes to the
  // mysql.ndb_apply_status table to be received. The server_id and epoch are
  // extracted from the changed data and identifies the upstream MySQL
  // Server by server_id and the epoch when data was binlogged in that cluster.
  // There might be more than one change to ndb_apply_status in each handled
  // epoch.
  {
    Ndb_binlog_index_rows rows;
    rows.init();

    constexpr uint server_id1 = 37;
    {
      auto &row = rows.find_row(server_id1, 0);
      // Still only one row, but NOT empty
      OK(rows.get_rows().size() == 1);
      OK(row.orig_server_id == server_id1);
      OK(row.orig_epoch == 0);
      row.n_inserts = 1;
      row.n_updates = 2;
      row.n_deletes = 3;
    }

    // Changes to ndb_apply_status occurs, both server_id and epoch is known.
    // Finds same row and updates it with the epoch
    constexpr ulonglong epoch1 = 370037;
    {
      auto &same_row = rows.find_row(server_id1, epoch1);
      // Still only one row
      OK(rows.get_rows().size() == 1);
      OK(same_row.orig_server_id == server_id1);
      OK(same_row.orig_epoch == epoch1);
      OK(same_row.n_inserts == 1);
      OK(same_row.n_updates == 2);
      OK(same_row.n_deletes == 3);
    }

    // Find same row again only by server_id
    {
      auto &same_row = rows.find_row(server_id1, 0);
      // Still only one row
      OK(rows.get_rows().size() == 1);
      OK(same_row.orig_server_id == server_id1);
      OK(same_row.orig_epoch == epoch1);
      OK(same_row.n_inserts == 1);
      OK(same_row.n_updates == 2);
      OK(same_row.n_deletes == 3);
    }

    // Another change to ndb_apply_status from same server_id occurs, create new
    // row and update it with the epoch, move stats from previous row.
    constexpr ulonglong epoch2 = 380038;
    {
      auto &same_row = rows.find_row(server_id1, epoch2);
      OK(rows.get_rows().size() == 2);
      OK(same_row.orig_server_id == server_id1);
      OK(same_row.orig_epoch == epoch2);
      OK(same_row.n_inserts == 1);
      OK(same_row.n_updates == 2);
      OK(same_row.n_deletes == 3);
      // Previous row stats should be zero
      const auto &prev_row = rows.get_rows()[0];
      OK(prev_row.orig_server_id == server_id1 &&
         prev_row.orig_epoch == epoch1 && prev_row.n_inserts == 0 &&
         prev_row.n_updates == 0 && prev_row.n_deletes == 0);
    }

    // Find same row again only by server_id, should return the most recent row
    // with epoch2
    {
      auto &same_row = rows.find_row(server_id1, 0);
      OK(rows.get_rows().size() == 2);
      OK(same_row.orig_server_id == server_id1);
      OK(same_row.orig_epoch == epoch2);
      OK(same_row.n_inserts == 1);
      OK(same_row.n_updates == 2);
      OK(same_row.n_deletes == 3);
    }

    // Row change from another server_id, creates a third row
    constexpr uint server_id2 = 38;
    {
      auto &new_row = rows.find_row(server_id2, 0);
      // Another row was created since new server_id
      OK(rows.get_rows().size() == 3);
      OK(new_row.orig_server_id == server_id2);
      OK(new_row.orig_epoch == 0);
      OK(new_row.n_inserts == 0);
      OK(new_row.n_updates == 0);
      OK(new_row.n_deletes == 0);
      new_row.n_inserts = 5;
      new_row.n_updates = 6;
      new_row.n_deletes = 7;
    }

    // Row change from first server_id, finds the first row
    {
      auto &first_row = rows.find_row(server_id1, 0);
      OK(rows.get_rows().size() == 3);
      OK(first_row.orig_server_id == server_id1);
      OK(first_row.orig_epoch == epoch2);
      OK(first_row.n_inserts == 1);
      OK(first_row.n_updates == 2);
      OK(first_row.n_deletes == 3);
    }

    // Row change from second server_id, finds third row
    {
      auto &second_row = rows.find_row(server_id2, 0);
      OK(rows.get_rows().size() == 3);
      OK(second_row.orig_server_id == server_id2);
      OK(second_row.orig_epoch == 0);
      OK(second_row.n_inserts == 5);
      OK(second_row.n_updates == 6);
      OK(second_row.n_deletes == 7);
    }

    // Third change to ndb_apply_status from the other server_id occurs, create
    // new row and update it with the epoch
    constexpr ulonglong epoch3 = 390039;
    {
      auto &new_row = rows.find_row(server_id2, epoch3);
      OK(rows.get_rows().size() == 3);
      OK(new_row.orig_server_id == server_id2);
      OK(new_row.orig_epoch == epoch3);
      OK(new_row.n_inserts == 5);
      OK(new_row.n_updates == 6);
      OK(new_row.n_deletes == 7);
    }

    // Fourth change to ndb_apply_status from the other server_id occurs, create
    // new row and update it with the epoch, move stats from previous row.
    constexpr ulonglong epoch4 = 400040;
    {
      auto &new_row = rows.find_row(server_id2, epoch4);
      OK(rows.get_rows().size() == 4);
      OK(new_row.orig_server_id == server_id2);
      OK(new_row.orig_epoch == epoch4);
      OK(new_row.n_inserts == 5);
      OK(new_row.n_updates == 6);
      OK(new_row.n_deletes == 7);

      // Previous row stats for server_id2, epoch3 should be zero
      for (const auto &row : rows.get_rows()) {
        if (row.orig_server_id == server_id2 && row.orig_epoch == epoch3) {
          OK(row.n_inserts == 0 && row.n_updates == 0 && row.n_deletes == 0);
        }
      }
    }

    // Fifth change to ndb_apply_status from the first server_id occurs, create
    // new row and update it with the epoch, move stats from previous row.
    constexpr ulonglong epoch5 = 410041;
    {
      auto &new_row = rows.find_row(server_id1, epoch5);
      OK(rows.get_rows().size() == 5);
      OK(new_row.orig_server_id == server_id1);
      OK(new_row.orig_epoch == epoch5);
      OK(new_row.n_inserts == 1);
      OK(new_row.n_updates == 2);
      OK(new_row.n_deletes == 3);

      // Previous row stats for server_id1, epoch2 should be zero and such row
      // should exist
      bool found = false;
      for (const auto &row : rows.get_rows()) {
        if (row.orig_server_id == server_id1 && row.orig_epoch == epoch2) {
          OK(row.n_inserts == 0 && row.n_updates == 0 && row.n_deletes == 0);
          found = true;
        }
      }
      OK(found);
    }

    // Print the rows
    std::cout << rows.dbug_dump("").c_str() << std::endl;
  }
  return 1;  // OK
}
#endif
