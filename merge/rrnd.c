/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Read a record with random-access. The position to the record must
  get by mrg_info(). The next record can be read with pos= -1 */


#include "mrgdef.h"

static MRG_TABLE *find_table(MRG_TABLE *start,MRG_TABLE *end,mrg_off_t pos);

/*
  	If filepos == -1, read next
	Returns same as nisam_rrnd:
	   0 = Ok.
	   1 = Record deleted.
	  -1 = EOF (or something, errno should be HA_ERR_END_OF_FILE)
*/

int mrg_rrnd(MRG_INFO *info,byte *buf,mrg_off_t filepos)
{
  int error;
  N_INFO *isam_info;

  if (filepos == ~(mrg_off_t) 0)		/* Can't use HA_POS_ERROR */
  {
    if (!info->current_table)
    {
      if (info->open_tables == info->end_table)
      {						/* No tables */
	my_errno=HA_ERR_END_OF_FILE;
	return -1;
      }
      isam_info=(info->current_table=info->open_tables)->table;
      if (info->cache_in_use)
	nisam_extra(isam_info,HA_EXTRA_CACHE);
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
      if ((error=(*isam_info->s->read_rnd)(isam_info,(byte*) buf,
					filepos,1)) >= 0 ||
	  my_errno != HA_ERR_END_OF_FILE)
	return (error);
      if (info->cache_in_use)
	nisam_extra(info->current_table->table,HA_EXTRA_NO_CACHE);
      if (info->current_table+1 == info->end_table)
	return(-1);
      info->current_table++;
      info->last_used_table=info->current_table;
      if (info->cache_in_use)
	nisam_extra(info->current_table->table,HA_EXTRA_CACHE);
      info->current_table->file_offset=
	info->current_table[-1].file_offset+
	info->current_table[-1].table->s->state.data_file_length;

      isam_info=info->current_table->table;
      filepos=isam_info->s->pack.header_length;
      isam_info->lastinx= (uint) -1;
    }
  }
  info->current_table=find_table(info->open_tables,
				 info->last_used_table,filepos);
  isam_info=info->current_table->table;
  isam_info->update&= HA_STATE_CHANGED;
  return ((*isam_info->s->read_rnd)(isam_info,(byte*) buf,
				    (ulong) (filepos -
					     info->current_table->file_offset),
				    0));
}


	/* Find which table to use according to file-pos */

static MRG_TABLE *find_table(MRG_TABLE *start,MRG_TABLE *end,mrg_off_t pos)
{
  MRG_TABLE *mid;

  while (start != end)
  {
    mid=start+((uint) (end-start)+1)/2;
    if (mid->file_offset > pos)
      end=mid-1;
    else
      start=mid;
  }
  return start;
}
