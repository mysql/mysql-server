/* Copyright (c) 2000, 2001, 2005-2007 MySQL AB
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

#include "myisamdef.h"

	/*  Read first row through  a specfic key */

int mi_rfirst(MI_INFO *info, uchar *buf, int inx)
{
  DBUG_ENTER("mi_rfirst");
  info->lastpos= HA_OFFSET_ERROR;
  info->update|= HA_STATE_PREV_FOUND;
  DBUG_RETURN(mi_rnext(info,buf,inx));
} /* mi_rfirst */
