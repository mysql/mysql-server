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
#include "trnman_public.h"

/**
   @brief deletes all rows from a table

   @param  info             Maria handler

   @return Operation status
     @retval 0      ok
     @retval 1      error
*/

int maria_delete_all_rows(MARIA_HA *info)
{
  uint i;
  MARIA_SHARE *share=info->s;
  MARIA_STATE_INFO *state=&share->state;
  my_bool log_record;
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
  log_record= share->base.transactional && !share->temporary;
  if (_ma_mark_file_changed(info))
    goto err;

  info->state->records=info->state->del=state->split=0;
  state->changed= 0;                            /* File is optimized */
  state->dellink = HA_OFFSET_ERROR;
  state->sortkey=  (ushort) ~0;
  info->state->key_file_length=share->base.keystart;
  info->state->data_file_length=0;
  info->state->empty=info->state->key_empty=0;
  info->state->checksum=0;

  state->key_del= HA_OFFSET_ERROR;
  for (i=0 ; i < share->base.keys ; i++)
    state->key_root[i]= HA_OFFSET_ERROR;

  /*
    If we are using delayed keys or if the user has done changes to the tables
    since it was locked then there may be key blocks in the key cache
  */
  flush_pagecache_blocks(share->pagecache, &share->kfile,
                         FLUSH_IGNORE_CHANGED);
  if (my_chsize(info->dfile.file, 0, 0, MYF(MY_WME)) ||
      my_chsize(share->kfile.file, share->base.keystart, 0, MYF(MY_WME))  )
    goto err;

  if (_ma_initialize_data_file(info->dfile.file, share))
    goto err;

  VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
#ifdef HAVE_MMAP
  /* Resize mmaped area */
  rw_wrlock(&info->s->mmap_lock);
  _ma_remap_file(info, (my_off_t)0);
  rw_unlock(&info->s->mmap_lock);
#endif
  if (log_record)
  {
    /* For now this record is only informative */
    LEX_STRING log_array[TRANSLOG_INTERNAL_PARTS + 1];
    uchar log_data[FILEID_STORE_SIZE];
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    (char*) log_data;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data);
    if (unlikely(translog_write_record(&share->state.create_rename_lsn,
                                       LOGREC_REDO_DELETE_ALL,
                                       info->trn, share, 0,
                                       sizeof(log_array)/sizeof(log_array[0]),
                                       log_array, log_data)))
      goto err;
    /*
      store LSN into file. It is an optimization so that all old REDOs for
      this table are ignored (scenario: checkpoint, INSERT1s, DELETE ALL;
      INSERT2s, crash: then Recovery can skip INSERT1s). It also allows us to
      ignore the present record at Recovery.
      Note that storing the LSN could not be done by _ma_writeinfo() above as
      the table is locked at this moment. So we need to do it by ourselves.
    */
    if (_ma_update_create_rename_lsn_on_disk(share, FALSE) ||
        _ma_sync_table_files(info))
      goto err;
    /**
       @todo RECOVERY Until we take into account the log record above
       for log-low-water-mark calculation and use it in Recovery, we need
       to sync above.
    */
  }
  allow_break();			/* Allow SIGHUP & SIGINT */
  DBUG_RETURN(0);

err:
  {
    int save_errno=my_errno;
    VOID(_ma_writeinfo(info,WRITEINFO_UPDATE_KEYFILE));
    info->update|=HA_STATE_WRITTEN;	/* Buffer changed */
    /** @todo RECOVERY until we use the log record above we have to sync */
    if (log_record &&_ma_sync_table_files(info) && !save_errno)
      save_errno= my_errno;
    allow_break();			/* Allow SIGHUP & SIGINT */
    DBUG_RETURN(my_errno=save_errno);
  }
} /* maria_delete */
