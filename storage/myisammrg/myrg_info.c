/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "myrg_def.h"

ulonglong myrg_position(MYRG_INFO *info)
{
  MYRG_TABLE *current_table;

  if (!(current_table = info->current_table) &&
      info->open_tables != info->end_table)
    current_table = info->open_tables;
  return  current_table ?
    current_table->table->lastpos + current_table->file_offset :
    ~(ulonglong) 0;
}

int myrg_status(MYRG_INFO *info, MYMERGE_INFO *x,int flag)
{
  MYRG_TABLE *current_table;
  DBUG_ENTER("myrg_status");

  if (!(current_table = info->current_table) &&
      info->open_tables != info->end_table)
    current_table = info->open_tables;
  x->recpos  = info->current_table ?
    info->current_table->table->lastpos + info->current_table->file_offset :
      (ulong) -1L;
  if (flag != HA_STATUS_POS)
  {
    MYRG_TABLE *file;

    info->records=info->del=info->data_file_length=0;
    for (file=info->open_tables ; file != info->end_table ; file++)
    {
      file->file_offset=info->data_file_length;
      info->data_file_length+=file->table->s->state.state.data_file_length;
      info->records+=file->table->s->state.state.records;
      info->del+=file->table->s->state.state.del;
      DBUG_PRINT("info2",("table: %s, offset: %lu",
                  file->table->filename,(ulong) file->file_offset));
    }
    x->records= info->records;
    x->deleted= info->del;
    x->data_file_length= info->data_file_length;
    x->reclength= info->reclength;
    x->options= info->options;
    if (current_table)
    {
      /*
        errkey is set to the index number of the myisam tables. But
        since the MERGE table can have less keys than the MyISAM
        tables, errkey cannot be be used as an index into the key_info
        on the server. This value will be overwritten with MAX_KEY by
        the MERGE engine.
      */
      x->errkey= current_table->table->errkey;
      /*
        Calculate the position of the duplicate key to be the sum of the
        offset of the myisam file and the offset into the file at which
        the duplicate key is located.
      */
      x->dupp_key_pos= current_table->file_offset + current_table->table->dupp_key_pos;
    }
    else
    {
      x->errkey= 0;
      x->dupp_key_pos= 0;
    }
    x->rec_per_key = info->rec_per_key_part;
  }
  DBUG_RETURN(0);
}
