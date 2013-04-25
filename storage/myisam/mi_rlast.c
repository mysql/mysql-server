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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "myisamdef.h"

	/* Read last row with the same key as the previous read. */

int mi_rlast(MI_INFO *info, uchar *buf, int inx)
{
  DBUG_ENTER("mi_rlast");
  info->lastpos= HA_OFFSET_ERROR;
  info->update|= HA_STATE_NEXT_FOUND;
  DBUG_RETURN(mi_rprev(info,buf,inx));
} /* mi_rlast */
