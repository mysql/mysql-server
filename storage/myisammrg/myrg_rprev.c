/* Copyright (C) 2000-2003 MySQL AB

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

#include "myrg_def.h"

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
