/* Copyright (c) 2002, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "storage/myisammrg/myrg_def.h"

ha_rows myrg_records_in_range(MYRG_INFO *info, int inx, key_range *min_key,
                              key_range *max_key) {
  ha_rows records = 0, res;
  MYRG_TABLE *table;

  for (table = info->open_tables; table != info->end_table; table++) {
    res = mi_records_in_range(table->table, inx, min_key, max_key);
    if (res == HA_POS_ERROR) return HA_POS_ERROR;
    if (records > HA_POS_ERROR - res) return HA_POS_ERROR - 1;
    records += res;
  }
  return records;
}
