/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

	/*
	   Read previous row with the same key as previous read
	*/

int myrg_rprev(MYRG_INFO *info, uchar *buf, int inx)
{
  int err;
  MI_INFO *mi;

  if (!info->current_table)
    return (HA_ERR_KEY_NOT_FOUND);

  /* at first, do rprev for the table found before */
  if ((err=mi_rprev(info->current_table->table,NULL,inx)))
  {
    if (err == HA_ERR_END_OF_FILE)
    {
      queue_remove(&(info->by_key),0);
      if (!info->by_key.elements)
        return HA_ERR_END_OF_FILE;
    }
    else
      return err;
  }
  else
  {
    /* Found here, adding to queue */
    queue_top(&(info->by_key))=(uchar *)(info->current_table);
    queue_replaced(&(info->by_key));
  }

  /* now, mymerge's read_prev is as simple as one queue_top */
  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  return _myrg_mi_read_record(mi,buf);
}
