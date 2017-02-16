/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include "maria_def.h"

int maria_scan_init(register MARIA_HA *info)
{
  DBUG_ENTER("maria_scan_init");

  info->cur_row.nextpos= info->s->pack.header_length;	/* Read first record */
  info->lastinx= -1;				/* Can't forward or backward */
  if (info->opt_flag & WRITE_CACHE_USED && flush_io_cache(&info->rec_cache))
    DBUG_RETURN(my_errno);

  if ((*info->s->scan_init)(info))
    DBUG_RETURN(my_errno);
  DBUG_RETURN(0);
}

/*
  Read a row based on position.

  SYNOPSIS
    maria_scan()
    info		Maria handler
    record		Read data here

  RETURN
    0  			   ok
    HA_ERR_END_OF_FILE     End of file
    HA_ERR_RECORD_DELETED  Record was deleted (can only happen for static rec)
    #			   Error code
*/

int maria_scan(MARIA_HA *info, uchar *record)
{
  DBUG_ENTER("maria_scan");
  /* Init all but update-flag */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  DBUG_RETURN((*info->s->scan)(info, record, info->cur_row.nextpos, 1));
}


void maria_scan_end(MARIA_HA *info)
{
  (*info->s->scan_end)(info);
}


int _ma_def_scan_remember_pos(MARIA_HA *info, MARIA_RECORD_POS *lastpos)
{
  *lastpos= info->cur_row.lastpos;
  return 0;
}


int _ma_def_scan_restore_pos(MARIA_HA *info, MARIA_RECORD_POS lastpos)
{
  info->cur_row.nextpos= lastpos;
  return 0;
}
