/* Copyright (c) 2002, 2004-2007 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

#include "myrg_def.h"


int myrg_rnext_same(MYRG_INFO *info, uchar *buf)
{
  int err;
  MI_INFO *mi;

  if (!info->current_table)
    return (HA_ERR_KEY_NOT_FOUND);

  /* at first, do rnext for the table found before */
  if ((err=mi_rnext_same(info->current_table->table,NULL)))
  {
    if (err == HA_ERR_END_OF_FILE)
    {
      queue_remove_top(&(info->by_key));
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
    queue_replace_top(&(info->by_key));
  }

  /* now, mymerge's read_next is as simple as one queue_top */
  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  return _myrg_mi_read_record(mi,buf);
}

