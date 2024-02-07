/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef NDB_PFS_TABLE_H
#define NDB_PFS_TABLE_H

#include "mysql/components/services/pfs_plugin_table_service.h"  // PFS_engine_table_share_proxy

/*
  Wrapper around PFS_engine_table_share_proxy. Used to construct a table share
  for ndbcluster PFS tables. It contains table information and is used to
  register callbacks for various functions. The share is used to create the
  table in PFS
*/
class Ndb_pfs_table_share : public PFS_engine_table_share_proxy {
 public:
  Ndb_pfs_table_share();
  Ndb_pfs_table_share(const Ndb_pfs_table_share &) = delete;

  virtual ~Ndb_pfs_table_share() = default;
};

/*
  Encapsulates all table operation related functionality
*/
class Ndb_pfs_table {
  unsigned int m_rows{0};      // Number of rows in the table
  unsigned int m_position{0};  // Current position of the cursor

 protected:
  /*
    @brief Return the current cursor position

    @return Current cursor position
  */
  unsigned int get_position() const { return m_position; }

  /*
    @brief Check if there's no data in the table

    @return true if the table is empty, false if not
  */
  bool is_empty() const { return (m_rows == 0); }

  /*
    @brief Set the number of rows in the table

    @return void
  */
  void set_num_rows(unsigned int rows) { m_rows = rows; }

  /*
    @brief Check if there are more rows remaining to be read

    @return true if there are more rows to be read, false if not
  */
  bool rows_pending_read() const { return (m_position <= m_rows); }

  /*
    @brief Check if all rows have been read

    @return true if all rows have been read, false if not
  */
  bool all_rows_read() const { return (m_position == m_rows + 1); }

 public:
  Ndb_pfs_table() = default;
  Ndb_pfs_table(const Ndb_pfs_table &) = delete;

  virtual ~Ndb_pfs_table() = default;

  /*
    @brief Read column at index of current row. Implementation
           is specific to table

    @param field [out]  Field	column value
    @param index [in]	  Index	column position within row

    @return 0 on success, plugin table error code on failure
  */
  virtual int read_column_value(PSI_field *field, unsigned int index) = 0;

  /*
    @brief Initialize the table

    @return 0 on success, plugin table error code on failure
  */
  virtual int rnd_init() = 0;

  /*
    @brief Set cursor to the next record

    @return 0 on success, plugin table error code on failure
  */
  int rnd_next();

  /*
    @brief Set cursor to current position: currently a no-op

    @return 0 on success, plugin table error code on failure
  */
  int rnd_pos() {
    if (m_position > 0 && m_position <= m_rows) {
      return (0);
    }
    return (PFS_HA_ERR_END_OF_FILE);
  }

  /*
    @brief Reset cursor position to the beginning

    @return void
  */
  void reset_pos() { m_position = 0; }

  /*
    @brief Close the table

    @return void
  */
  virtual void close() { m_position = 0; }

  /*
    @brief Get the address of the current position

    @return reference to current position
  */
  unsigned int *get_position_address() { return (&m_position); }
};

#endif
