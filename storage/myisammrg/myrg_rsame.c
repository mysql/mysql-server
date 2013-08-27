/* Copyright (C) 2000-2002 MySQL AB

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

#include "myrg_def.h"

int myrg_rsame(MYRG_INFO *info,uchar *record,int inx)
{
  if (inx)					/* not yet used, should be 0 */
    return (my_errno=HA_ERR_WRONG_INDEX);

  if (!info->current_table)
    return (my_errno=HA_ERR_NO_ACTIVE_RECORD);

  return mi_rsame(info->current_table->table,record,inx);
}
