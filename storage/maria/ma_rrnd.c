/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* Read a record with random-access. The position to the record must
   get by MARIA_HA. The next record can be read with pos= MARIA_POS_ERROR */


#include "maria_def.h"

/*
  Read a row based on position.

  RETURN
    0   Ok.
    HA_ERR_RECORD_DELETED  Record is deleted.
    HA_ERR_END_OF_FILE	   EOF.
*/

int maria_rrnd(MARIA_HA *info, byte *buf, MARIA_RECORD_POS filepos)
{
  DBUG_ENTER("maria_rrnd");

  DBUG_ASSERT(filepos != HA_OFFSET_ERROR);
#ifdef NOT_USED
  if (filepos == HA_OFFSET_ERROR)
  {
    skip_deleted_blocks=1;
    if (info->cur_row.lastpos == HA_OFFSET_ERROR)  /* First read ? */
      filepos= info->s->pack.header_length;	   /* Read first record */
    else
      filepos= info->cur_row.nextpos;
  }
#endif

  /* Init all but update-flag */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  if (info->opt_flag & WRITE_CACHE_USED && flush_io_cache(&info->rec_cache))
    DBUG_RETURN(my_errno);

  DBUG_RETURN((*info->s->read_record)(info, buf, filepos));
}
