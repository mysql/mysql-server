/* Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */
#ifndef RPL_INFO_TABLE_ACCESS_H
#define RPL_INFO_TABLE_ACCESS_H

#include "my_global.h"
#include "rpl_table_access.h"    // System_table_access
/*
#include <table.h>
#include <key.h>
#include "rpl_info_handler.h"
#include "rpl_info_values.h"
*/
class Field;
class Rpl_info_values;


enum enum_return_id { FOUND_ID= 1, NOT_FOUND_ID, ERROR_ID };

class Rpl_info_table_access : public System_table_access
{
public:
  Rpl_info_table_access(): thd_created(false) { };
  virtual ~Rpl_info_table_access() { };

  /**
    Prepares before opening table.
    - set flags
    - start lex and reset the part of THD responsible
      for the state of command processing if needed.

    @param[in]  thd  Thread requesting to open the table
  */
  void before_open(THD* thd);
  bool close_table(THD* thd, TABLE* table, Open_tables_backup* backup,
                   bool error);
  enum enum_return_id find_info(Rpl_info_values *field_values, TABLE *table);
  enum enum_return_id scan_info(TABLE *table, uint instance);
  bool count_info(TABLE *table, uint* counter);
  bool load_info_values(uint max_num_field, Field **fields,
                        Rpl_info_values *field_values);
  bool store_info_values(uint max_num_field, Field **fields,
                         Rpl_info_values *field_values);
  THD *create_thd();
  void drop_thd(THD* thd);

private:
  bool thd_created;

  Rpl_info_table_access& operator=(const Rpl_info_table_access& info);
  Rpl_info_table_access(const Rpl_info_table_access& info);
};
#endif /* RPL_INFO_TABLE_ACCESS_H */
