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

/*
  Rename a table
*/

#include "ma_fulltext.h"

int maria_rename(const char *old_name, const char *new_name)
{
  char from[FN_REFLEN],to[FN_REFLEN];
  int data_file_rename_error;
#ifdef USE_RAID
  uint raid_type=0,raid_chunks=0;
#endif
  DBUG_ENTER("maria_rename");

#ifdef EXTRA_DEBUG
  _ma_check_table_is_closed(old_name,"rename old_table");
  _ma_check_table_is_closed(new_name,"rename new table2");
#endif
  /* LOCK TODO take X-lock on table here */
#ifdef USE_RAID
  {
    MARIA_HA *info;
    if (!(info=maria_open(old_name, O_RDONLY, 0)))
      DBUG_RETURN(my_errno);
    raid_type =      info->s->base.raid_type;
    raid_chunks =    info->s->base.raid_chunks;
    maria_close(info);
  }
#ifdef EXTRA_DEBUG
  _ma_check_table_is_closed(old_name,"rename raidcheck");
#endif
#endif /* USE_RAID */

  fn_format(from,old_name,"",MARIA_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  fn_format(to,new_name,"",MARIA_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  /*
    RECOVERY TODO log the two renames below. Update
    ZeroDirtyPagesLSN of the table on disk (=> sync the files), this is
    needed so that Recovery does not pick a wrong table.
    Then do the file renames.
    For this log record to be of any use for Recovery, we need the upper MySQL
    layer to be crash-safe in DDLs; when it is we should reconsider the moment
    of writing this log record, how to use it in Recovery, and force the log.
    For now this record is only informative. But ZeroDirtyPagesLSN is
    critically needed!
  */
  if (my_rename_with_symlink(from, to, MYF(MY_WME | MY_SYNC_DIR)))
    DBUG_RETURN(my_errno);
  fn_format(from,old_name,"",MARIA_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  fn_format(to,new_name,"",MARIA_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
#ifdef USE_RAID
  if (raid_type)
    data_file_rename_error= my_raid_rename(from, to, raid_chunks,
                                           MYF(MY_WME | MY_SYNC_DIR));
  else
#endif
    data_file_rename_error=
      my_rename_with_symlink(from, to, MYF(MY_WME | MY_SYNC_DIR));
  if (data_file_rename_error)
  {
    /*
      now we have a renamed index file and a non-renamed data file, try to
      undo the rename of the index file.
    */
    data_file_rename_error= my_errno;
    fn_format(from, old_name, "", MARIA_NAME_IEXT, MYF(MY_UNPACK_FILENAME|MY_APPEND_EXT));
    fn_format(to, new_name, "", MARIA_NAME_IEXT, MYF(MY_UNPACK_FILENAME|MY_APPEND_EXT));
    my_rename_with_symlink(to, from, MYF(MY_WME | MY_SYNC_DIR));
  }
  DBUG_RETURN(data_file_rename_error);

}
