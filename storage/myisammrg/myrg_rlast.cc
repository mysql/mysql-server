/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>

#include "my_inttypes.h"
#include "storage/myisammrg/myrg_def.h"

	/* Read last row with the same key as the previous read. */

int myrg_rlast(MYRG_INFO *info, uchar *buf, int inx)
{
  MYRG_TABLE *table;
  MI_INFO *mi;
  int err;

  if (_myrg_init_queue(info,inx, HA_READ_KEY_OR_PREV))
    return my_errno();

  for (table=info->open_tables ; table < info->end_table ; table++)
  {
    if ((err=mi_rlast(table->table,NULL,inx)))
    {
      if (err == HA_ERR_END_OF_FILE)
	continue;
      return err;
    }
    /* adding to queue */
    queue_insert(&(info->by_key),(uchar *)table);
  }
  /* We have done a read in all tables */
  info->last_used_table=table;

  if (!info->by_key.elements)
    return HA_ERR_END_OF_FILE;

  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  return _myrg_mi_read_record(mi,buf);
}

