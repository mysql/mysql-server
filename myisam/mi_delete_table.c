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
  deletes a table
*/

#include "fulltext.h"
#ifdef	__WIN__
#include <errno.h>
#endif

int mi_delete_table(const char *name)
{
  char from[FN_REFLEN];
#ifdef USE_RAID
  uint raid_type=0,raid_chunks=0;
#endif
  DBUG_ENTER("mi_delete_table");

#ifdef EXTRA_DEBUG
  check_table_is_closed(name,"delete");
#endif
#ifdef USE_RAID
  {
    MI_INFO *info;
    /* we use 'open_for_repair' to be able to delete a crashed table */
    if (!(info=mi_open(name, O_RDONLY, HA_OPEN_FOR_REPAIR)))
      DBUG_RETURN(my_errno);
    raid_type =      info->s->base.raid_type;
    raid_chunks =    info->s->base.raid_chunks;
    mi_close(info);
  }
#ifdef EXTRA_DEBUG
  check_table_is_closed(name,"delete");
#endif
#endif /* USE_RAID */

  fn_format(from,name,"",MI_NAME_IEXT,4);
  if (my_delete_with_symlink(from, MYF(MY_WME)))
    DBUG_RETURN(my_errno);
  fn_format(from,name,"",MI_NAME_DEXT,4);
#ifdef USE_RAID
  if (raid_type)
    DBUG_RETURN(my_raid_delete(from, raid_chunks, MYF(MY_WME)) ? my_errno : 0);
#endif
  DBUG_RETURN(my_delete_with_symlink(from, MYF(MY_WME)) ? my_errno : 0);
}
