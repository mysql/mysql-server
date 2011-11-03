/* Copyright (c) 2000, 2002, 2005, 2006 MySQL AB, 2009 Sun Microsystems, Inc.
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

#include "heapdef.h"

	/* if flag == HA_PANIC_CLOSE then all files are removed for more
	   memory */

int hp_panic(enum ha_panic_function flag)
{
  LIST *element,*next_open;
  DBUG_ENTER("hp_panic");

  mysql_mutex_lock(&THR_LOCK_heap);
  for (element=heap_open_list ; element ; element=next_open)
  {
    HP_INFO *info=(HP_INFO*) element->data;
    next_open=element->next;	/* Save if close */
    switch (flag) {
    case HA_PANIC_CLOSE:
      hp_close(info);
      break;
    default:
      break;
    }
  }
  for (element=heap_share_list ; element ; element=next_open)
  {
    HP_SHARE *share=(HP_SHARE*) element->data;
    next_open=element->next;	/* Save if close */
    switch (flag) {
    case HA_PANIC_CLOSE:
    {
      if (!share->open_count)
	hp_free(share);
      break;
    }
    default:
      break;
    }
  }
  mysql_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(0);
} /* hp_panic */
