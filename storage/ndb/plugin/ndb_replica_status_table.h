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

#ifndef NDB_REPLICA_STATUS_TABLE_H
#define NDB_REPLICA_STATUS_TABLE_H

#include <vector>

#include "storage/ndb/plugin/ndb_pfs_table.h"  // Ndb_pfs_table
#include "storage/ndb/plugin/ndb_replica.h"

class Ndb_replica_status_table_share : public Ndb_pfs_table_share {
 public:
  Ndb_replica_status_table_share();
  Ndb_replica_status_table_share(const Ndb_replica_status_table_share &) =
      delete;
};

class Ndb_replica_status_table : public Ndb_pfs_table {
  std::vector<Ndb_replica::ChannelPtr> m_channel_list;

 public:
  Ndb_replica_status_table() = default;
  Ndb_replica_status_table(const Ndb_replica_status_table &) = delete;

  /*
    @brief Read column at index of current row. Implementation
           is specific to each table

    @param field [out]  Field	column value
    @param index [in]	  Index	column position within row

    @return 0 on success, plugin table error code on failure
  */
  int read_column_value(PSI_field *field, unsigned int index) override;

  /*
    @brief Initialize the table for random read

    @return 0 on success, plugin table error code on failure
  */
  int rnd_init() override;

  /*
    @brief Close the table

    @return void
  */
  void close() override;
};

#endif
