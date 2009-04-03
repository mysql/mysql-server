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

/* Remove all rows from a MARIA table */
/* This clears the status information and truncates files */

#include "maria_def.h"
#include "trnman.h"

/**
   @brief deletes all rows from a table

   @param  info             Maria handler

   @note It is important that this function does not rely on the state
   information, as it may be called by ma_apply_undo_bulk_insert() on an
   inconsistent table left by a crash.

   @return Operation status
     @retval 0      ok
     @retval 1      error
*/

int maria_delete_all_rows(MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;
  my_bool log_record;
  LSN lsn;
  DBUG_ENTER("maria_delete_all_rows");

  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    DBUG_RETURN(my_errno=EACCES);
  }
  /**
     @todo LOCK take X-lock on table here.
     When we have versioning, if some other thread is looking at this table,
     we cannot shrink the file like this.
  */
  if (_ma_readinfo(info,F_WRLCK,1))
    DBUG_RETURN(my_errno);
  log_record= share->now_transactional && !share->temporary;
  if (_ma_mark_file_changed(info))
    goto err;

  if (log_record)
  {
    /*
      This record will be used by Recovery to finish the deletion if it
      crashed. We force it to have a complete history in the log.
    */
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
    uchar log_data[FILEID_STORE_SIZE];
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
    if (unlikely(translog_write_record(&lsn, LOGREC_REDO_DELETE_ALL,
                                       info->trn, info, 0,
                                       sizeof(log_array)/sizeof(log_array[0]),
                                       log_array, log_data, NULL) ||
                 translog_flush(lsn)))
      goto err;
    /*
      If we fail in this function after this point, log and table will be
      inconsistent.
    */
  }
  else
  {
    /* Other branch called function below when writing log record, in hook */
    _ma_reset_status(info);
  }
  /* Remove old history as the table is now empty for everyone */
  _ma_reset_state(info);

  /*
    If we are using delayed keys or if the user has done changes to the tables
    since it was locked then there may be key blocks in the page cache. Or
    there may be data blocks there. We need to throw them away or they may
    re-enter the emptied table or another table later.
  */

#ifdef HAVE_MMAP
  if (share->file_map)
    _ma_unmap_file(info);
#endif

  if (_ma_flush_table_files(info, MARIA_FLUSH_DATA|MARIA_FLUSH_INDEX,
                            FLUSH_IGNORE_CHANGED, FLUSH_IGNORE_CHANGED) ||
      my_chsize(info->dfile.file, 0, 0, MYF(MY_WME)) ||
      my_chsize(share->kfile.file, share->base.keystart, 0, MYF(MY_WME)))
    goto err;

  if (_ma_initialize_data_file(share, info->dfile.file))
    goto err;

  if (log_record)
  {
    /*
      Because LOGREC_REDO_DELETE_ALL does not operate on pages, it has the
      following problem:
      delete_all; inserts (redo_insert); all pages get flushed; checkpoint:
      the dirty pages list will be empty. In recovery, delete_all is executed,
      but redo_insert are skipped (dirty pages list is empty).
      To avoid this, we need to set skip_redo_lsn now, and thus need to sync
      files.
      Also fixes the problem of:
      bulk insert; insert; delete_all; crash:
      "bulk insert" is skipped (no REDOs), so if "insert" would not be skipped
      (if we didn't update skip_redo_lsn below) then "insert" would be tried
      and fail, saying that it sees that the first page has to be created
      though the inserted row has rownr>0.
    */
    my_bool error= _ma_state_info_write(share,
                                        MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET |
                                        MA_STATE_INFO_WRITE_LOCK) ||
      _ma_update_state_lsns(share, lsn, trnman_get_min_trid(), FALSE, FALSE) ||
      _ma_sync_table_files(info);
    info->trn->rec_lsn= LSN_IMPOSSIBLE;
    if (error)
      goto err;
  }

  VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
#ifdef HAVE_MMAP
  /* Map again */
  if (share->file_map)
    _ma_dynmap_file(info, (my_off_t) 0);
#endif
  allow_break();			/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(0);

err:
  {
    int save_errno=my_errno;
    VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
    info->update|=HA_STATE_WRITTEN;	/* Buffer changed */
    allow_break();			/* Allow SIGHUP & SIGINT */
    DBUG_RETURN(my_errno=save_errno);
  }
} /* maria_delete_all_rows */


/*
  Reset status information

  SYNOPSIS
    _ma_reset_status()
    maria	Maria handler

  DESCRIPTION
    Resets data and index file information as if the file would be empty
    Files are not touched.
*/

void _ma_reset_status(MARIA_HA *info)
{
  MARIA_SHARE *share= info->s;
  MARIA_STATE_INFO *state= &share->state;
  uint i;
  DBUG_ENTER("_ma_reset_status");

  state->split= 0;
  state->state.records= state->state.del= 0;
  state->changed=  0;                            /* File is optimized */
  state->dellink= HA_OFFSET_ERROR;
  state->sortkey=  (ushort) ~0;
  state->state.key_file_length= share->base.keystart;
  state->state.data_file_length= 0;
  state->state.empty= state->state.key_empty= 0;
  state->state.checksum= 0;

  *info->state= state->state;

  /* Drop the delete key chain. */
  state->key_del= HA_OFFSET_ERROR;
  /* Clear all keys */
  for (i=0 ; i < share->base.keys ; i++)
    state->key_root[i]= HA_OFFSET_ERROR;
  DBUG_VOID_RETURN;
}
