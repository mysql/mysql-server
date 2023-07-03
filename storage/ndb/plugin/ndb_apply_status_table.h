/*
   Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_APPLY_STATUS_TABLE_H
#define NDB_APPLY_STATUS_TABLE_H

#include <string>
#include <string_view>
#include <vector>

#include "storage/ndb/include/ndb_types.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/plugin/ndb_util_table.h"

class NdbTransaction;
namespace dd {
class Table;
}
struct NdbError;

// RAII style class for working with the apply status table in NDB
class Ndb_apply_status_table : public Ndb_util_table {
  Ndb_apply_status_table() = delete;
  Ndb_apply_status_table(const Ndb_apply_status_table &) = delete;

  bool define_table_ndb(NdbDictionary::Table &table,
                        unsigned mysql_version) const override;

  bool drop_events_in_NDB() const override;

 public:
  static const std::string DB_NAME;
  static const std::string TABLE_NAME;

  Ndb_apply_status_table(class Thd_ndb *);
  virtual ~Ndb_apply_status_table();

  bool check_schema() const override;

  bool need_upgrade() const override;

  bool need_reinstall(const dd::Table *table_def) const override;

  std::string define_table_dd() const override;

  /**
   * @brief Check if given name is the apply status table, special
            handling for the table is required in a few places.
     @param db database name
     @param table_name table name
     @return true if table is the apply status table
   */
  static bool is_apply_status_table(const char *db, const char *table_name);

  /**
     @brief Scan the ndb_apply_status table and return:
       1) MAX(epoch) WHERE server_id == own_server_id OR
                           server_id IN (<ignore_server_id_list>)
       2) epoch WHERE server_id == source_server_id
       3) list with all server_id's in table

     @param own_server_id               The server_id of this server
     @param ignore_server_ids           List of additional server_ids to find
                                        the MAX(epoch) for
     @param source_server_id            The server_id of source server
     @param highest_applied_epoch [out] The highest epoch value as per 1)
     @param source_epoch [out]          The epoch value as per 2)
     @param server_ids [out]            The list of server_id's as per 3)

     @return true for success
   */
  bool load_state(Uint32 own_server_id,
                  const std::vector<Uint32> &ignore_server_ids,
                  Uint32 source_server_id, Uint64 &highest_applied_epoch,
                  Uint64 &source_epoch, std::vector<Uint32> &server_ids) const;

  /**
     @brief Append an update of ndb_apply_status to given transaction.

    This function defines an UPDATE of ndb_apply_status with new values for
    log_name, start_pos and end_pos where server_id=<server_id>

     @param trans             The NDB transaction
     @param server_id         The server_id of row to update
     @param log_name          The value to set for "log_name" column
     @param start_pos         The value to set for "start_pos" column
     @param end_pos           The value to set for "end_pos" column
     @param any_value         The value to use as "any value" for the operation

     @return nullptr for success and pointer to NdbError otherwise
   */
  const NdbError *define_update_row(NdbTransaction *trans, Uint32 server_id,
                                    std::string_view log_name, Uint64 start_pos,
                                    Uint64 end_pos, Uint32 any_value);

  /**
     @brief Append a write to ndb_apply_status to given transaction.

    This function defines a WRITE of ndb_apply_status with values for
    epoch, log_name, start_pos and end_pos where server_id=<server_id>

     @param trans             The NDB transaction
     @param server_id         The server_id of row to write
     @param epoch             The value to set for "epoch" column
     @param log_name          The value to set for "log_name" column
     @param start_pos         The value to set for "start_pos" column
     @param end_pos           The value to set for "end_pos" column
     @param any_value         The value to use as "any value" for the operation

     @return nullptr for success and pointer to NdbError otherwise
   */
  const NdbError *define_write_row(NdbTransaction *trans, Uint32 server_id,
                                   Uint64 epoch, std::string_view log_name,
                                   Uint64 start_pos, Uint64 end_pos,
                                   Uint32 any_value);
};

#endif
