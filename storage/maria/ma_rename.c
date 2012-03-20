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
#include "trnman_public.h"

/**
   @brief renames a table

   @param  old_name        current name of table
   @param  new_name        table should be renamed to this name

   @return Operation status
     @retval 0      OK
     @retval !=0    Error
*/

int maria_rename(const char *old_name, const char *new_name)
{
  char from[FN_REFLEN],to[FN_REFLEN];
  int data_file_rename_error;
#ifdef USE_RAID
  uint raid_type=0,raid_chunks=0;
#endif
  MARIA_HA *info;
  MARIA_SHARE *share;
  myf sync_dir;
  DBUG_ENTER("maria_rename");

#ifdef EXTRA_DEBUG
  _ma_check_table_is_closed(old_name,"rename old_table");
  _ma_check_table_is_closed(new_name,"rename new table2");
#endif
  /** @todo LOCK take X-lock on table */
  if (!(info= maria_open(old_name, O_RDWR, HA_OPEN_FOR_REPAIR)))
    DBUG_RETURN(my_errno);
  share= info->s;
#ifdef USE_RAID
  raid_type =      share->base.raid_type;
  raid_chunks =    share->base.raid_chunks;
#endif

  /*
    the renaming of an internal table to the final table (like in ALTER TABLE)
    is the moment when this table receives its correct create_rename_lsn and
    this is important; make sure transactionality has been re-enabled.
  */
  DBUG_ASSERT(share->now_transactional == share->base.born_transactional);
  sync_dir= (share->now_transactional && !share->temporary &&
             !maria_in_recovery) ? MY_SYNC_DIR : 0;
  if (sync_dir)
  {
    LSN lsn;
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 2];
    uint old_name_len= strlen(old_name)+1, new_name_len= strlen(new_name)+1;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str= (uchar*)old_name;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= old_name_len;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].str= (uchar*)new_name;
    log_array[TRANSLOG_INTERNAL_PARTS + 1].length= new_name_len;
    /*
      For this record to be of any use for Recovery, we need the upper
      MySQL layer to be crash-safe, which it is not now (that would require
      work using the ddl_log of sql/sql_table.cc); when it is, we should
      reconsider the moment of writing this log record (before or after op,
      under THR_LOCK_maria or not...), how to use it in Recovery.
      For now it can serve to apply logs to a backup so we sync it.
    */
    if (unlikely(translog_write_record(&lsn, LOGREC_REDO_RENAME_TABLE,
                                       &dummy_transaction_object, NULL,
                                       old_name_len + new_name_len,
                                       sizeof(log_array)/sizeof(log_array[0]),
                                       log_array, NULL, NULL) ||
                 translog_flush(lsn)))
    {
      maria_close(info);
      DBUG_RETURN(1);
    }
    /*
      store LSN into file, needed for Recovery to not be confused if a
      RENAME happened (applying REDOs to the wrong table).
    */
    if (_ma_update_state_lsns(share, lsn, share->state.create_trid, TRUE,
                              TRUE))
    {
      maria_close(info);
      DBUG_RETURN(1);
    }
  }

  _ma_reset_state(info);
  maria_close(info);

  fn_format(from,old_name,"",MARIA_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  fn_format(to,new_name,"",MARIA_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  if (mysql_file_rename_with_symlink(key_file_kfile, from, to,
                                     MYF(MY_WME | sync_dir)))
    DBUG_RETURN(my_errno);
  fn_format(from,old_name,"",MARIA_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  fn_format(to,new_name,"",MARIA_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  data_file_rename_error=
      mysql_file_rename_with_symlink(key_file_dfile, from, to,
                                     MYF(MY_WME | sync_dir));
  if (data_file_rename_error)
  {
    /*
      now we have a renamed index file and a non-renamed data file, try to
      undo the rename of the index file.
    */
    data_file_rename_error= my_errno;
    fn_format(from, old_name, "", MARIA_NAME_IEXT, MYF(MY_UNPACK_FILENAME|MY_APPEND_EXT));
    fn_format(to, new_name, "", MARIA_NAME_IEXT, MYF(MY_UNPACK_FILENAME|MY_APPEND_EXT));
    mysql_file_rename_with_symlink(key_file_kfile, to, from,
                                   MYF(MY_WME | sync_dir));
  }
  DBUG_RETURN(data_file_rename_error);

}
