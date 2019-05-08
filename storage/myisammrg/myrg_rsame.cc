/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "my_inttypes.h"
#include "my_thread_local.h"
#include "storage/myisammrg/myrg_def.h"

int myrg_rsame(MYRG_INFO *info, uchar *record, int inx) {
  if (inx) /* not yet used, should be 0 */
  {
    set_my_errno(HA_ERR_WRONG_INDEX);
    return HA_ERR_WRONG_INDEX;
  }

  if (!info->current_table) {
    set_my_errno(HA_ERR_NO_ACTIVE_RECORD);
    return HA_ERR_NO_ACTIVE_RECORD;
  }

  return mi_rsame(info->current_table->table, record, inx);
}
