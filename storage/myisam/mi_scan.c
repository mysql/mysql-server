/* Copyright (C) 2000-2001 MySQL AB

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

/* Read through all rows sequntially */

#include "myisamdef.h"

int mi_scan_init(register MI_INFO *info)
{
  DBUG_ENTER("mi_scan_init");
  info->nextpos=info->s->pack.header_length;	/* Read first record */
  info->lastinx= -1;				/* Can't forward or backward */
  if (info->opt_flag & WRITE_CACHE_USED && flush_io_cache(&info->rec_cache))
    DBUG_RETURN(my_errno);
  DBUG_RETURN(0);
}

/*
	   Read a row based on position.
	   If filepos= HA_OFFSET_ERROR then read next row
	   Return values
	   Returns one of following values:
	   0 = Ok.
	   HA_ERR_END_OF_FILE = EOF.
*/

int mi_scan(MI_INFO *info, uchar *buf)
{
  DBUG_ENTER("mi_scan");
  /* Init all but update-flag */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  DBUG_RETURN ((*info->s->read_rnd)(info,buf,info->nextpos,1));
}
