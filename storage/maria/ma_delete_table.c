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

/*
  deletes a table
*/

#include "ma_fulltext.h"

int maria_delete_table(const char *name)
{
  char from[FN_REFLEN];
#ifdef USE_RAID
  uint raid_type=0,raid_chunks=0;
#endif
  DBUG_ENTER("maria_delete_table");

#ifdef EXTRA_DEBUG
  _ma_check_table_is_closed(name,"delete");
#endif
#ifdef USE_RAID
  {
    MARIA_HA *info;
    /*
      When built with RAID support, we need to determine if this table
      makes use of the raid feature. If yes, we need to remove all raid
      chunks. This is done with my_raid_delete(). Unfortunately it is
      necessary to open the table just to check this. We use
      'open_for_repair' to be able to open even a crashed table. If even
      this open fails, we assume no raid configuration for this table
      and try to remove the normal data file only. This may however
      leave the raid chunks behind.
    */
    if (!(info= maria_open(name, O_RDONLY, HA_OPEN_FOR_REPAIR)))
      raid_type= 0;
    else
    {
      raid_type=      info->s->base.raid_type;
      raid_chunks=    info->s->base.raid_chunks;
      maria_close(info);
    }
  }
#ifdef EXTRA_DEBUG
  _ma_check_table_is_closed(name,"delete");
#endif
#endif /* USE_RAID */

  fn_format(from,name,"",MARIA_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  if (my_delete_with_symlink(from, MYF(MY_WME)))
    DBUG_RETURN(my_errno);
  fn_format(from,name,"",MARIA_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
#ifdef USE_RAID
  if (raid_type)
    DBUG_RETURN(my_raid_delete(from, raid_chunks, MYF(MY_WME)) ? my_errno : 0);
#endif
  DBUG_RETURN(my_delete_with_symlink(from, MYF(MY_WME)) ? my_errno : 0);
}
