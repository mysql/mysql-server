/* Copyright (c) 2000, 2001, 2005, 2006 MySQL AB, 2009 Sun Microsystems, Inc.
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

/* Check if somebody has changed table since last check. */

#include "myisamdef.h"

       /* Return 0 if table isn't changed */

int mi_is_changed(MI_INFO *info)
{
  int result;
  DBUG_ENTER("mi_is_changed");
  if (fast_mi_readinfo(info))
    DBUG_RETURN(-1);
  (void) _mi_writeinfo(info,0);
  result=(int) info->data_changed;
  info->data_changed=0;
  DBUG_PRINT("exit",("result: %d",result));
  DBUG_RETURN(result);
}
