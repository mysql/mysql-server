/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
#ifndef RPL_INFO_TABLE_ACCESS_H
#define RPL_INFO_TABLE_ACCESS_H

#include <sys/types.h>

#include "sql/rpl_table_access.h"  // System_table_access

class Field;
class Open_tables_backup;
class Rpl_info_values;
class THD;
struct TABLE;

enum enum_return_id { FOUND_ID = 1, NOT_FOUND_ID, ERROR_ID };

class Rpl_info_table_access : public System_table_access {
 public:
  Rpl_info_table_access() : thd_created(false) {}
  ~Rpl_info_table_access() override = default;

  /**
    Prepares before opening table.
    - set flags
    - start lex and reset the part of THD responsible
      for the state of command processing if needed.

    @param[in]  thd  Thread requesting to open the table
  */
  void before_open(THD *thd) override;
  bool close_table(THD *thd, TABLE *table, Open_tables_backup *backup,
                   bool error);
  enum enum_return_id find_info(Rpl_info_values *field_values, TABLE *table);
  enum enum_return_id scan_info(TABLE *table, uint instance);
  std::pair<bool, bool> is_table_in_use(TABLE *table);
  bool count_info(TABLE *table, ulonglong *counter);
  bool load_info_values(uint max_num_field, Field **fields,
                        Rpl_info_values *field_values);
  bool store_info_values(uint max_num_field, Field **fields,
                         Rpl_info_values *field_values);
  THD *create_thd();
  void drop_thd(THD *thd);

 private:
  bool thd_created;

  Rpl_info_table_access &operator=(const Rpl_info_table_access &info);
  Rpl_info_table_access(const Rpl_info_table_access &info);
};
#endif /* RPL_INFO_TABLE_ACCESS_H */
