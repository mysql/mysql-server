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

#include "maria_def.h"

	/* Read last row with the same key as the previous read. */

int maria_rlast(MARIA_HA *info, uchar *buf, int inx)
{
  DBUG_ENTER("maria_rlast");
  info->cur_row.lastpos= HA_OFFSET_ERROR;
  info->update|= HA_STATE_NEXT_FOUND;
  DBUG_RETURN(maria_rprev(info,buf,inx));
} /* maria_rlast */
