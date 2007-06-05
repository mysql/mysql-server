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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Read a record with random-access. The position to the record must
  get by myrg_info(). The next record can be read with pos= -1 */


#include "myrg_def.h"

static MYRG_TABLE *find_table(MYRG_TABLE *start,MYRG_TABLE *end,ulonglong pos);

/*
	If filepos == HA_OFFSET_ERROR, read next
	Returns same as mi_rrnd:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record is deleted.
	   HA_ERR_END_OF_FILE = EOF.
*/

int myrg_rrnd(MYRG_INFO *info,uchar *buf,ulonglong filepos)
{
  int error;
  MI_INFO *isam_info;
  DBUG_ENTER("myrg_rrnd");
  DBUG_PRINT("info",("offset: %lu", (ulong) filepos));

  if (filepos == HA_OFFSET_ERROR)
  {
    if (!info->current_table)
    {
      if (info->open_tables == info->end_table)
      {						/* No tables */
	DBUG_RETURN(my_errno=HA_ERR_END_OF_FILE);
      }
      isam_info=(info->current_table=info->open_tables)->table;
      if (info->cache_in_use)
	mi_extra(isam_info,HA_EXTRA_CACHE,(uchar*) &info->cache_size);
      filepos=isam_info->s->pack.header_length;
      isam_info->lastinx= (uint) -1;	/* Can't forward or backward */
    }
    else
    {
      isam_info=info->current_table->table;
      filepos= isam_info->nextpos;
    }

    for (;;)
    {
      isam_info->update&= HA_STATE_CHANGED;
      if ((error=(*isam_info->s->read_rnd)(isam_info,(uchar*) buf,
					   (my_off_t) filepos,1)) !=
	  HA_ERR_END_OF_FILE)
	DBUG_RETURN(error);
      if (info->cache_in_use)
	mi_extra(info->current_table->table, HA_EXTRA_NO_CACHE,
		 (uchar*) &info->cache_size);
      if (info->current_table+1 == info->end_table)
	DBUG_RETURN(HA_ERR_END_OF_FILE);
      info->current_table++;
      info->last_used_table=info->current_table;
      if (info->cache_in_use)
	mi_extra(info->current_table->table, HA_EXTRA_CACHE,
		 (uchar*) &info->cache_size);
      info->current_table->file_offset=
	info->current_table[-1].file_offset+
	info->current_table[-1].table->state->data_file_length;

      isam_info=info->current_table->table;
      filepos=isam_info->s->pack.header_length;
      isam_info->lastinx= (uint) -1;
    }
  }
  info->current_table=find_table(info->open_tables,
				 info->end_table-1,filepos);
  isam_info=info->current_table->table;
  isam_info->update&= HA_STATE_CHANGED;
  DBUG_RETURN((*isam_info->s->read_rnd)
              (isam_info, (uchar*) buf,
	      (my_off_t) (filepos - info->current_table->file_offset),
	      0));
}


	/* Find which table to use according to file-pos */

static MYRG_TABLE *find_table(MYRG_TABLE *start, MYRG_TABLE *end,
			      ulonglong pos)
{
  MYRG_TABLE *mid;
  DBUG_ENTER("find_table");

  while (start != end)
  {
    mid=start+((uint) (end-start)+1)/2;
    if (mid->file_offset > pos)
      end=mid-1;
    else
      start=mid;
  }
  DBUG_PRINT("info",("offset: %lu, table: %s",
		     (ulong) pos, start->table->filename));
  DBUG_RETURN(start);
}
