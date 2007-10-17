/* Copyright (C) 2000-2002, 2004 MySQL AB

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

/* Read a record with random-access. The position to the record must
   get by MI_INFO. The next record can be read with pos= MI_POS_ERROR */


#include "myisamdef.h"

/*
	   Read a row based on position.
	   If filepos= HA_OFFSET_ERROR then read next row
	   Return values
	   Returns one of following values:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record is deleted.
	   HA_ERR_END_OF_FILE = EOF.
*/

int mi_rrnd(MI_INFO *info, uchar *buf, register my_off_t filepos)
{
  my_bool skip_deleted_blocks;
  DBUG_ENTER("mi_rrnd");

  skip_deleted_blocks=0;

  if (filepos == HA_OFFSET_ERROR)
  {
    skip_deleted_blocks=1;
    if (info->lastpos == HA_OFFSET_ERROR)	/* First read ? */
      filepos= info->s->pack.header_length;	/* Read first record */
    else
      filepos= info->nextpos;
  }

  if (info->once_flags & RRND_PRESERVE_LASTINX)
    info->once_flags&= ~RRND_PRESERVE_LASTINX;
  else
    info->lastinx= -1;                          /* Can't forward or backward */
  /* Init all but update-flag */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  if (info->opt_flag & WRITE_CACHE_USED && flush_io_cache(&info->rec_cache))
    DBUG_RETURN(my_errno);

  DBUG_RETURN ((*info->s->read_rnd)(info,buf,filepos,skip_deleted_blocks));
}
