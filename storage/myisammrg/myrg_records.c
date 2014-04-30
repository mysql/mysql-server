/* Copyright (C) 2008 MySQL AB
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "myrg_def.h"

ha_rows myrg_records(MYRG_INFO *info)
{
  ha_rows records=0;
  MYRG_TABLE *file;
  DBUG_ENTER("myrg_records");

  for (file=info->open_tables ; file != info->end_table ; file++)
    records+= file->table->s->state.state.records;
  DBUG_RETURN(records);
}
