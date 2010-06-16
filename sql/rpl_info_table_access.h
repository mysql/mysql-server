/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
#ifndef RPL_INFO_TABLE_ACCESS_H
#define RPL_INFO_TABLE_ACCESS_H

#include "my_global.h"
#include "mysql_priv.h"
#include "rpl_info_handler.h"
#include "rpl_info_fields.h"

#define NULL_TABLE_INFO "0"

enum enum_return_id { FOUND_ID= 1, NOT_FOUND_ID, ERROR_ID };

class Rpl_info_table_access
{
public:
  Rpl_info_table_access();
  virtual ~Rpl_info_table_access();

  bool open_table(THD* thd, const char *dbstr, const char *tbstr,
                  uint max_num_field, enum thr_lock_type lock_type,
                  TABLE** table, Open_tables_state* backup);
  bool close_table(THD* thd, TABLE* table, Open_tables_state* backup);
  enum enum_return_id find_info_id(uint idx, LEX_STRING, TABLE*);
  bool load_info_fields(uint max_num_field, Field **fields, ...);
  bool load_info_fields(uint max_num_field, Field **fields,
                        Rpl_info_fields *field_values);
  bool store_info_fields(uint max_num_field, Field **fields, ...);
  bool store_info_fields(uint max_num_field, Field **fields,
                         Rpl_info_fields *field_values);
  THD *create_fake_thd();
  bool drop_fake_thd(THD* thd, bool error);

private:
  THD *saved_current_thd;
  enum enum_thread_type saved_thd_type;

  MEM_ROOT mem_root;

  Rpl_info_table_access& operator=(const Rpl_info_table_access& info);
  Rpl_info_table_access(const Rpl_info_table_access& info);
};

#endif /* RPL_INFO_TABLE_ACCESS_H */
