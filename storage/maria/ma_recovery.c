/* Copyright (C) 2006, 2007 MySQL AB

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
  WL#3072 Maria recovery
  First version written by Guilhem Bichot on 2006-04-27.
*/

/* Here is the implementation of this module */

#include "maria_def.h"
#include "ma_recovery.h"
#include "ma_blockrec.h"

struct TRN_FOR_RECOVERY
{
  LSN group_start_lsn, undo_lsn;
  TrID long_trid;
};

/* Variables used by all functions of this module. Ok as single-threaded */
static struct TRN_FOR_RECOVERY *all_active_trans;
static MARIA_HA **all_tables;
static LSN current_group_end_lsn;
FILE *tracef; /**< trace file for debugging */

#define prototype_exec_hook(R)                                  \
static int exec_LOGREC_ ## R(const TRANSLOG_HEADER_BUFFER *rec)
prototype_exec_hook(LONG_TRANSACTION_ID);
#ifdef MARIA_CHECKPOINT
prototype_exec_hook(CHECKPOINT);
#endif
prototype_exec_hook(REDO_CREATE_TABLE);
prototype_exec_hook(REDO_DROP_TABLE);
prototype_exec_hook(FILE_ID);
prototype_exec_hook(REDO_INSERT_ROW_HEAD);
prototype_exec_hook(REDO_INSERT_ROW_TAIL);
prototype_exec_hook(REDO_PURGE_ROW_HEAD);
prototype_exec_hook(REDO_PURGE_ROW_TAIL);
prototype_exec_hook(REDO_PURGE_BLOCKS);
prototype_exec_hook(REDO_DELETE_ALL);
prototype_exec_hook(UNDO_ROW_INSERT);
prototype_exec_hook(UNDO_ROW_DELETE);
prototype_exec_hook(UNDO_ROW_PURGE);
prototype_exec_hook(COMMIT);
static int  end_of_redo_phase();
static void display_record_position(const LOG_DESC *log_desc,
                                    const TRANSLOG_HEADER_BUFFER *rec,
                                    uint number);
static int display_and_apply_record(const LOG_DESC *log_desc,
                                    const TRANSLOG_HEADER_BUFFER *rec);
static MARIA_HA *get_MARIA_HA_from_REDO_record(const
                                               TRANSLOG_HEADER_BUFFER *rec);
static MARIA_HA *get_MARIA_HA_from_UNDO_record(const
                                               TRANSLOG_HEADER_BUFFER *rec);
static int close_recovered_table(MARIA_HA *info);


/** @brief global [out] buffer for translog_read_record(); never shrinks */
static LEX_STRING log_record_buffer;
#define enlarge_buffer(rec)                                             \
  if (log_record_buffer.length < rec->record_length)                    \
  {                                                                     \
    log_record_buffer.length= rec->record_length;                       \
    log_record_buffer.str= my_realloc(log_record_buffer.str,            \
                                      rec->record_length, MYF(MY_WME)); \
  }

#define ALERT_USER() DBUG_ASSERT(0)


/**
   @brief Recovers from the last checkpoint
*/

int maria_recover()
{
  my_bool res= TRUE;
  LSN from_lsn;
  FILE *trace_file;
  DBUG_ENTER("maria_recover");

  DBUG_ASSERT(!maria_in_recovery);
  maria_in_recovery= TRUE;

  if (last_checkpoint_lsn == LSN_IMPOSSIBLE)
  {
    from_lsn= translog_first_theoretical_lsn();
    /*
      as far as we have not yet any checkpoint then the very first
      log file should be present.
    */
    DBUG_ASSERT(from_lsn != LSN_IMPOSSIBLE);
    /*
      @todo process eroror of getting checkpoint
    if (from_lsn == ERROR_LSN)
      ...
    */
  }
  else
  {
    DBUG_ASSERT(0); /* not yet implemented */
    /**
       @todo read the checkpoint record, fill structures
       and use the minimum of checkpoint_start_lsn, rec_lsn of trns, rec_lsn
       of dirty pages.
    */
    //from_lsn= something;
  }

  /*
    mysqld has not yet initialized any page cache. Let's create a dedicated
    one for recovery.
  */
  if ((trace_file= fopen("maria_recovery.trace", "w")))
  {
    fprintf(trace_file, "TRACE of the last MARIA recovery from mysqld\n");
    res= (init_pagecache(maria_pagecache,
                         /** @todo what size? */
                         1024*1024,
                         0, 0,
                         maria_block_size) == 0) ||
      maria_apply_log(from_lsn, TRUE, trace_file);
    end_pagecache(maria_pagecache, TRUE);
    if (!res)
      fprintf(trace_file, "SUCCESS\n");
    fclose(trace_file);
  }
  /**
     @todo take checkpoint if log applying did some work.
     Be sure to not checkpoint if no work.
  */
  maria_in_recovery= FALSE;
  DBUG_RETURN(res);
}


/**
   @brief Displays and/or applies the log

   @param  lsn             LSN from which log reading/applying should start
   @param  apply           if log records should be applied or not
   @param  trace_file      trace file where progress/debug messages will go

   @todo This trace_file thing is primitive; soon we will make it similar to
   ma_check_print_warning() etc, and a successful recovery does not need to
   create a trace file. But for debugging now it is useful.

   @return Operation status
     @retval 0      OK
     @retval !=0    Error
*/

int maria_apply_log(LSN lsn, my_bool apply, FILE *trace_file)
{
  int error= 0;
  DBUG_ENTER("maria_apply_log");

  DBUG_ASSERT(!maria_multi_threaded);
  all_active_trans= (struct TRN_FOR_RECOVERY *)
    my_malloc((SHORT_TRID_MAX + 1) * sizeof(struct TRN_FOR_RECOVERY),
              MYF(MY_ZEROFILL));
  all_tables= (MARIA_HA **)my_malloc((SHARE_ID_MAX + 1) * sizeof(MARIA_HA *),
                                     MYF(MY_ZEROFILL));
  if (!all_active_trans || !all_tables)
    goto err;

  tracef= trace_file;
  /* install hooks for execution */
#define install_exec_hook(R)                                            \
  log_record_type_descriptor[LOGREC_ ## R].record_execute_in_redo_phase= \
    exec_LOGREC_ ## R;
  install_exec_hook(LONG_TRANSACTION_ID);
#ifdef MARIA_CHECKPOINT
  install_exec_hook(CHECKPOINT);
#endif
  install_exec_hook(REDO_CREATE_TABLE);
  install_exec_hook(REDO_DROP_TABLE);
  install_exec_hook(FILE_ID);
  install_exec_hook(REDO_INSERT_ROW_HEAD);
  install_exec_hook(REDO_INSERT_ROW_TAIL);
  install_exec_hook(REDO_PURGE_ROW_HEAD);
  install_exec_hook(REDO_PURGE_ROW_TAIL);
  install_exec_hook(REDO_PURGE_BLOCKS);
  install_exec_hook(REDO_DELETE_ALL);
  install_exec_hook(UNDO_ROW_INSERT);
  install_exec_hook(UNDO_ROW_DELETE);
  install_exec_hook(UNDO_ROW_PURGE);
  install_exec_hook(COMMIT);

  current_group_end_lsn= LSN_IMPOSSIBLE;

  TRANSLOG_HEADER_BUFFER rec;
  struct st_translog_scanner_data scanner;
  uint i= 1;

  int len= translog_read_record_header(lsn, &rec);

  /** @todo EOF should be detected */
  if (len == RECHEADER_READ_ERROR)
  {
    fprintf(tracef, "Cannot find a first record\n");
    goto err;
  }

  if (translog_init_scanner(lsn, 1, &scanner))
  {
    fprintf(tracef, "Scanner init failed\n");
    goto err;
  }
  for (;;i++)
  {
    uint16 sid= rec.short_trid;
    const LOG_DESC *log_desc= &log_record_type_descriptor[rec.type];
    display_record_position(log_desc, &rec, i);

    /*
      A complete group is a set of log records with an "end mark" record
      (e.g. a set of REDOs for an operation, terminated by an UNDO for this
      operation); if there is no "end mark" record the group is incomplete
      and won't be executed.
    */
    if ((log_desc->record_in_group == LOGREC_IS_GROUP_ITSELF) ||
        (log_desc->record_in_group == LOGREC_LAST_IN_GROUP))
    {
      if (all_active_trans[sid].group_start_lsn != LSN_IMPOSSIBLE)
      {
        if (log_desc->record_in_group == LOGREC_IS_GROUP_ITSELF)
        {
          /*
            can happen if the transaction got a table write error, then
            unlocked tables thus wrote a COMMIT record.
          */
          fprintf(tracef, "\nDiscarding unfinished group before this record\n");
          ALERT_USER();
          all_active_trans[sid].group_start_lsn= LSN_IMPOSSIBLE;
        }
        else
        {
          /*
            There is a complete group for this transaction, containing more
            than this event.
          */
          fprintf(tracef, "   ends a group:\n");
          struct st_translog_scanner_data scanner2;
          TRANSLOG_HEADER_BUFFER rec2;
          len=
            translog_read_record_header(all_active_trans[sid].group_start_lsn, &rec2);
          if (len < 0) /* EOF or error */
          {
            fprintf(tracef, "Cannot find record where it should be\n");
            goto err;
          }
          if (translog_init_scanner(rec2.lsn, 1, &scanner2))
          {
            fprintf(tracef, "Scanner2 init failed\n");
            goto err;
          }
          current_group_end_lsn= rec.lsn;
          do
          {
            if (rec2.short_trid == sid) /* it's in our group */
            {
              const LOG_DESC *log_desc2= &log_record_type_descriptor[rec2.type];
              display_record_position(log_desc2, &rec2, 0);
              if (apply && display_and_apply_record(log_desc2, &rec2))
                goto err;
            }
            len= translog_read_next_record_header(&scanner2, &rec2);
            if (len < 0) /* EOF or error */
            {
              fprintf(tracef, "Cannot find record where it should be\n");
              goto err;
            }
          }
          while (rec2.lsn < rec.lsn);
          translog_free_record_header(&rec2);
          /* group finished */
          all_active_trans[sid].group_start_lsn= LSN_IMPOSSIBLE;
          current_group_end_lsn= LSN_IMPOSSIBLE; /* for debugging */
          display_record_position(log_desc, &rec, 0);
        }
      }
      if (apply && display_and_apply_record(log_desc, &rec))
        goto err;
    }
    else /* record does not end group */
    {
      /* just record the fact, can't know if can execute yet */
      if (all_active_trans[sid].group_start_lsn == LSN_IMPOSSIBLE)
      {
        /* group not yet started */
        all_active_trans[sid].group_start_lsn= rec.lsn;
      }
    }
    len= translog_read_next_record_header(&scanner, &rec);
    if (len < 0)
    {
      switch (len)
      {
      case RECHEADER_READ_EOF:
        fprintf(tracef, "EOF on the log\n");
        break;
      case RECHEADER_READ_ERROR:
        fprintf(stderr, "Error reading log\n");
        goto err;
      }
      break;
    }
  }
  translog_free_record_header(&rec);

  /*
    So we have applied all REDOs.
    We may now have unfinished transactions.
    I don't think it's this program's job to roll them back:
    to roll back and at the same time stay idempotent, it needs to write log
    records (without CLRs, 2nd rollback would hit the effects of first
    rollback and fail). But this standalone tool is not allowed to write to
    the server's transaction log. So we do not roll back anything.
    In the real Recovery code, or the code to do "recover after online
    backup", yes we will roll back.
  */
  if (end_of_redo_phase())
    goto err;

  goto end;
err:
  error= 1;
  fprintf(tracef, "Recovery of tables with transaction logs FAILED\n");
end:
  my_free(all_tables, MYF(MY_ALLOW_ZERO_PTR));
  my_free(all_active_trans, MYF(MY_ALLOW_ZERO_PTR));
  my_free(log_record_buffer.str, MYF(MY_ALLOW_ZERO_PTR));
  log_record_buffer.str= NULL;
  log_record_buffer.length= 0;
  DBUG_RETURN(error);
}


/* very basic info about the record's header */
static void display_record_position(const LOG_DESC *log_desc,
                                    const TRANSLOG_HEADER_BUFFER *rec,
                                    uint number)
{
  /*
    if number==0, we're going over records which we had already seen and which
    form a group, so we indent below the group's end record
  */
  fprintf(tracef, "%sRec#%u LSN (%lu,0x%lx) short_trid %u %s(num_type:%u) len %lu\n",
         number ? "" : "   ", number,
         (ulong) LSN_FILE_NO(rec->lsn), (ulong) LSN_OFFSET(rec->lsn),
         rec->short_trid, log_desc->name, rec->type,
         (ulong)rec->record_length);
}


static int display_and_apply_record(const LOG_DESC *log_desc,
                                    const TRANSLOG_HEADER_BUFFER *rec)
{
  int error;
  if (log_desc->record_execute_in_redo_phase == NULL)
  {
    /* die on all not-yet-handled records :) */
    DBUG_ASSERT("one more hook" == "to write");
    return 1;
  }
  if ((error= (*log_desc->record_execute_in_redo_phase)(rec)))
    fprintf(tracef, "Got error when executing record\n");
  return error;
}


prototype_exec_hook(LONG_TRANSACTION_ID)
{
  uint16 sid= rec->short_trid;
  TrID long_trid= all_active_trans[sid].long_trid;
  /* abort group of this trn (must be of before a crash) */
  LSN gslsn= all_active_trans[sid].group_start_lsn;
  char llbuf[22];
  if (gslsn != LSN_IMPOSSIBLE)
  {
    fprintf(tracef, "Group at LSN (%lu,0x%lx) short_trid %u aborted\n",
           (ulong) LSN_FILE_NO(gslsn), (ulong) LSN_OFFSET(gslsn), sid);
    all_active_trans[sid].group_start_lsn= LSN_IMPOSSIBLE;
  }
  if (long_trid != 0)
  {
    LSN ulsn= all_active_trans[sid].undo_lsn;
    if (ulsn != LSN_IMPOSSIBLE)
    {
      llstr(long_trid, llbuf);
      fprintf(tracef, "Found an old transaction long_trid %s short_trid %u"
              " with same short id as this new transaction, and has neither"
              " committed nor rollback (undo_lsn: (%lu,0x%lx))\n", llbuf,
              sid, (ulong) LSN_FILE_NO(ulsn), (ulong) LSN_OFFSET(ulsn));
      goto err;
    }
  }
  long_trid= uint6korr(rec->header);
  all_active_trans[sid].long_trid= long_trid;
  llstr(long_trid, llbuf);
  fprintf(tracef, "Transaction long_trid %s short_trid %u starts\n", llbuf, sid);
  goto end;
err:
  ALERT_USER();
  return 1;
end:
  return 0;
}


#ifdef MARIA_CHECKPOINT
prototype_exec_hook(CHECKPOINT)
{
  /* the only checkpoint we care about was found via control file, ignore */
  return 0;
}
#endif


prototype_exec_hook(REDO_CREATE_TABLE)
{
  File dfile= -1, kfile= -1;
  char *linkname_ptr, filename[FN_REFLEN];
  char *name, *ptr;
  myf create_flag;
  uint flags;
  int error= 1, create_mode= O_RDWR | O_TRUNC;
  MARIA_HA *info= NULL;
  enlarge_buffer(rec);
  if (log_record_buffer.str == NULL ||
      translog_read_record(rec->lsn, 0, rec->record_length,
                           log_record_buffer.str, NULL) !=
      rec->record_length)
  {
    fprintf(tracef, "Failed to read record\n");
    goto end;
  }
  name= log_record_buffer.str;
  fprintf(tracef, "Table '%s'", name);
  /* we try hard to get create_rename_lsn, to avoid mistakes if possible */
  info= maria_open(name, O_RDONLY, HA_OPEN_FOR_REPAIR);
  if (info)
  {
    MARIA_SHARE *share= info->s;
    /* check that we're not already using it */
    DBUG_ASSERT(share->reopen == 1);
    DBUG_ASSERT(share->now_transactional == share->base.born_transactional);
    if (!share->base.born_transactional)
    {
      /*
        could be that transactional table was later dropped, and a non-trans
        one was renamed to its name, thus create_rename_lsn is 0 and should
        not be trusted.
      */
      fprintf(tracef, ", is not transactional\n");
      ALERT_USER();
      error= 0;
      goto end;
    }
    if (cmp_translog_addr(share->state.create_rename_lsn, rec->lsn) >= 0)
    {
      fprintf(tracef, ", has create_rename_lsn (%lu,0x%lx) more recent than record",
             (ulong) LSN_FILE_NO(rec->lsn),
             (ulong) LSN_OFFSET(rec->lsn));
      error= 0;
      goto end;
    }
    if (maria_is_crashed(info))
    {
      fprintf(tracef, ", is crashed, overwriting it");
      ALERT_USER();
    }
    maria_close(info);
    info= NULL;
  }
  /* if does not exist, is older, or its header is corrupted, overwrite it */
  // TODO symlinks
  ptr= name + strlen(name) + 1;
  if ((flags= ptr[0] ? HA_DONT_TOUCH_DATA : 0))
    fprintf(tracef, ", we will only touch index file");
  fn_format(filename, name, "", MARIA_NAME_IEXT,
            (MY_UNPACK_FILENAME |
             (flags & HA_DONT_TOUCH_DATA) ? MY_RETURN_REAL_PATH : 0) |
            MY_APPEND_EXT);
  linkname_ptr= NULL;
  create_flag= MY_DELETE_OLD;
  fprintf(tracef, ", creating as '%s'", filename);
  if ((kfile= my_create_with_symlink(linkname_ptr, filename, 0, create_mode,
                                     MYF(MY_WME|create_flag))) < 0)
  {
    fprintf(tracef, "Failed to create index file\n");
    goto end;
  }
  ptr++;
  uint kfile_size_before_extension= uint2korr(ptr);
  ptr+= 2;
  uint keystart= uint2korr(ptr);
  ptr+= 2;
  /* set create_rename_lsn (for maria_read_log to be idempotent) */
  lsn_store(ptr + sizeof(info->s->state.header) + 2, rec->lsn);
  /* we also set is_of_lsn, like maria_create() does */
  lsn_store(ptr + sizeof(info->s->state.header) + 2 + LSN_STORE_SIZE,
            rec->lsn);
  if (my_pwrite(kfile, ptr,
                kfile_size_before_extension, 0, MYF(MY_NABP|MY_WME)) ||
      my_chsize(kfile, keystart, 0, MYF(MY_WME)))
  {
    fprintf(tracef, "Failed to write to index file\n");
    goto end;
  }
  if (!(flags & HA_DONT_TOUCH_DATA))
  {
    fn_format(filename,name,"", MARIA_NAME_DEXT,
              MY_UNPACK_FILENAME | MY_APPEND_EXT);
    linkname_ptr= NULL;
    create_flag=MY_DELETE_OLD;
    if (((dfile=
          my_create_with_symlink(linkname_ptr, filename, 0, create_mode,
                                 MYF(MY_WME | create_flag))) < 0) ||
        my_close(dfile, MYF(MY_WME)))
    {
      fprintf(tracef, "Failed to create data file\n");
      goto end;
    }
    /*
      we now have an empty data file. To be able to
      _ma_initialize_data_file() we need some pieces of the share to be
      correctly filled. So we just open the table (fortunately, an empty
      data file does not preclude this).
    */
    if (((info= maria_open(name, O_RDONLY, 0)) == NULL) ||
        _ma_initialize_data_file(info->s, info->dfile.file))
    {
      fprintf(tracef, "Failed to open new table or write to data file\n");
      goto end;
    }
  }
  error= 0;
end:
  fprintf(tracef, "\n");
  if (kfile >= 0)
    error|= my_close(kfile, MYF(MY_WME));
  if (info != NULL)
    error|= maria_close(info);
  return error;
}


prototype_exec_hook(REDO_DROP_TABLE)
{
  char *name;
  int error= 1;
  MARIA_HA *info= NULL;
  enlarge_buffer(rec);
  if (log_record_buffer.str == NULL ||
      translog_read_record(rec->lsn, 0, rec->record_length,
                           log_record_buffer.str, NULL) !=
      rec->record_length)
  {
    fprintf(tracef, "Failed to read record\n");
    goto end;
  }
  name= log_record_buffer.str;
  fprintf(tracef, "Table '%s'", name);
  info= maria_open(name, O_RDONLY, HA_OPEN_FOR_REPAIR);
  if (info)
  {
    MARIA_SHARE *share= info->s;
    /*
      We may have open instances on this table. But it does not matter, the
      maria_extra() below will take care of them.
    */
    if (!share->base.born_transactional)
    {
      fprintf(tracef, ", is not transactional\n");
      ALERT_USER();
      error= 0;
      goto end;
    }
    if (cmp_translog_addr(share->state.create_rename_lsn, rec->lsn) >= 0)
    {
      fprintf(tracef, ", has create_rename_lsn (%lu,0x%lx) more recent than record",
             (ulong) LSN_FILE_NO(rec->lsn),
             (ulong) LSN_OFFSET(rec->lsn));
      error= 0;
      goto end;
    }
    if (maria_is_crashed(info))
    {
      fprintf(tracef, ", is crashed, dropping it");
      ALERT_USER();
    }
    /*
      This maria_extra() call serves to signal that old open instances of
      this table should not be used anymore, and (only on Windows) to close
      open files so they can be deleted
    */
    if (maria_extra(info, HA_EXTRA_PREPARE_FOR_DELETE, NULL) ||
        maria_close(info))
      goto end;
    info= NULL;
  }
  /* if does not exist, is older, or its header is corrupted, drop it */
  fprintf(tracef, ", dropping '%s'", name);
  if (maria_delete_table(name))
  {
    fprintf(tracef, "Failed to drop table\n");
    goto end;
  }
  error= 0;
end:
  fprintf(tracef, "\n");
  if (info != NULL)
    error|= maria_close(info);
  return error;
}


prototype_exec_hook(FILE_ID)
{
  uint16 sid;
  int error= 1;
  char *name, *buff;
  MARIA_HA *info= NULL;
  MARIA_SHARE *share;
  enlarge_buffer(rec);
  if (log_record_buffer.str == NULL ||
      translog_read_record(rec->lsn, 0, rec->record_length,
                           log_record_buffer.str, NULL) !=
       rec->record_length)
  {
    fprintf(tracef, "Failed to read record\n");
    goto end;
  }
  buff= log_record_buffer.str;
  sid= fileid_korr(buff);
  name= buff + FILEID_STORE_SIZE;
  info= all_tables[sid];
  if (info != NULL)
  {
    all_tables[sid]= NULL;
    if (close_recovered_table(info))
    {
      fprintf(tracef, "Failed to close table\n");
      goto end;
    }
  }
  fprintf(tracef, "Table '%s', id %u", name, sid);
  info= maria_open(name, O_RDWR, HA_OPEN_FOR_REPAIR);
  if (info == NULL)
  {
    fprintf(tracef, ", is absent (must have been dropped later?)"
           " or its header is so corrupted that we cannot open it;"
           " we skip it\n");
    error= 0;
    goto end;
  }
  if (maria_is_crashed(info))
  {
    fprintf(tracef, "Table is crashed, can't apply log records to it\n");
    goto end;
   /*
      we should make an exception for REDO_REPAIR_TABLE records: if we want to
      execute them, we should not reject the crashed table here.
    */
  }
  share= info->s;
  /* check that we're not already using it */
  DBUG_ASSERT(share->reopen == 1);
  DBUG_ASSERT(share->now_transactional == share->base.born_transactional);
  if (!share->base.born_transactional)
  {
    fprintf(tracef, ", is not transactional\n");
    ALERT_USER();
    error= 0;
    goto end;
  }
  all_tables[sid]= info;
  /* don't log any records for this work */
  _ma_tmp_disable_logging_for_table(share);
  /* execution of some REDO records relies on data_file_length */
  my_off_t dfile_len= my_seek(info->dfile.file, 0, SEEK_END, MYF(MY_WME));
  my_off_t kfile_len= my_seek(info->s->kfile.file, 0, SEEK_END, MYF(MY_WME));
  if ((dfile_len == MY_FILEPOS_ERROR) ||
      (kfile_len == MY_FILEPOS_ERROR))
  {
    fprintf(tracef, ", length unknown\n");
    goto end;
  }
  share->state.state.data_file_length= dfile_len;
  share->state.state.key_file_length=  kfile_len;
  if ((dfile_len == 0) || ((dfile_len % share->block_size) > 0))
  {
    fprintf(tracef, ", has too short last page\n");
    /* Recovery will fix this, no error */
    ALERT_USER();
  }
  fprintf(tracef, ", opened\n");
  error= 0;
end:
  if (error && info != NULL)
    error|= maria_close(info);
  return error;
}


prototype_exec_hook(REDO_INSERT_ROW_HEAD)
{
  int error= 1;
  uchar *buff= NULL;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    goto end;
  /*
    If REDO's LSN is > page's LSN (read from disk), we are going to modify the
    page and change its LSN. The normal runtime code stores the UNDO's LSN
    into the page. Here storing the REDO's LSN (rec->lsn) would work
    (we are not writing to the log here, so don't have to "flush up to UNDO's
    LSN"). But in a test scenario where we do updates at runtime, then remove
    tables, apply the log and check that this results in the same table as at
    runtime, putting the same LSN as runtime had done will decrease
    differences. So we use the UNDO's LSN which is current_group_end_lsn.
  */
  enlarge_buffer(rec);
  if (log_record_buffer.str == NULL ||
      translog_read_record(rec->lsn, 0, rec->record_length,
                           log_record_buffer.str, NULL) !=
       rec->record_length)
  {
    fprintf(tracef, "Failed to read record\n");
    goto end;
  }
  buff= log_record_buffer.str;
  if (_ma_apply_redo_insert_row_head_or_tail(info, current_group_end_lsn,
                                             HEAD_PAGE,
                                             buff + FILEID_STORE_SIZE,
                                             buff +
                                             FILEID_STORE_SIZE +
                                             PAGE_STORE_SIZE +
                                             DIRPOS_STORE_SIZE,
                                             rec->record_length -
                                             (FILEID_STORE_SIZE +
                                              PAGE_STORE_SIZE +
                                              DIRPOS_STORE_SIZE)))
    goto end;
  error= 0;
end:
  return error;
}


prototype_exec_hook(REDO_INSERT_ROW_TAIL)
{
  int error= 1;
  uchar *buff= NULL;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    goto end;
  enlarge_buffer(rec);
  if (log_record_buffer.str == NULL ||
      translog_read_record(rec->lsn, 0, rec->record_length,
                           log_record_buffer.str, NULL) !=
       rec->record_length)
  {
    fprintf(tracef, "Failed to read record\n");
    goto end;
  }
  buff= log_record_buffer.str;
  if (_ma_apply_redo_insert_row_head_or_tail(info, current_group_end_lsn,
                                             TAIL_PAGE,
                                             buff + FILEID_STORE_SIZE,
                                             buff +
                                             FILEID_STORE_SIZE +
                                             PAGE_STORE_SIZE +
                                             DIRPOS_STORE_SIZE,
                                             rec->record_length -
                                             (FILEID_STORE_SIZE +
                                              PAGE_STORE_SIZE +
                                              DIRPOS_STORE_SIZE)))
    goto end;
  error= 0;

end:
  return error;
}


prototype_exec_hook(REDO_PURGE_ROW_HEAD)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    goto end;
  if (_ma_apply_redo_purge_row_head_or_tail(info, current_group_end_lsn,
                                            HEAD_PAGE,
                                            rec->header + FILEID_STORE_SIZE))
    goto end;
  error= 0;
end:
  return error;
}


prototype_exec_hook(REDO_PURGE_ROW_TAIL)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    goto end;
  if (_ma_apply_redo_purge_row_head_or_tail(info, current_group_end_lsn,
                                            TAIL_PAGE,
                                            rec->header + FILEID_STORE_SIZE))
    goto end;
  error= 0;
end:
  return error;
}


prototype_exec_hook(REDO_PURGE_BLOCKS)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    goto end;
  if (_ma_apply_redo_purge_blocks(info, current_group_end_lsn,
                                  rec->header + FILEID_STORE_SIZE))
    goto end;
  error= 0;
end:
  return error;
}


prototype_exec_hook(REDO_DELETE_ALL)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    goto end;
  fprintf(tracef, "   deleting all %lu rows\n",
         (ulong)info->s->state.state.records);
  if (maria_delete_all_rows(info))
    goto end;
  error= 0;
end:
  return error;
}


prototype_exec_hook(UNDO_ROW_INSERT)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  if (info == NULL)
    goto end;
  all_active_trans[rec->short_trid].undo_lsn= rec->lsn;
  /*
    todo: instead of above, call write_hook_for_undo, it will also set
    first_undo_lsn
  */
  /*
    in an upcoming patch ("recovery of the state"), we introduce
    state.is_of_lsn. For now, we just assume the state is old (true when we
    recreate tables from scratch - but not idempotent).
  */
  {
    fprintf(tracef, "   state older than record, updating rows' count\n");
    info->s->state.state.records++;
  }
  fprintf(tracef, "   rows' count %lu\n", (ulong)info->s->state.state.records);
  error= 0;
end:
  return error;
}


prototype_exec_hook(UNDO_ROW_DELETE)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  if (info == NULL)
    goto end;
  all_active_trans[rec->short_trid].undo_lsn= rec->lsn;
  /*
    todo: instead of above, call write_hook_for_undo, it will also set
    first_undo_lsn
  */
  {
    fprintf(tracef, "   state older than record, updating rows' count\n");
    info->s->state.state.records--;
  }
  fprintf(tracef, "   rows' count %lu\n", (ulong)info->s->state.state.records);
  error= 0;
end:
  return error;
}


prototype_exec_hook(UNDO_ROW_PURGE)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  if (info == NULL)
    goto end;
  /* this a bit broken, but this log record type will be deleted soon */
  all_active_trans[rec->short_trid].undo_lsn= rec->lsn;
  /*
    todo: instead of above, call write_hook_for_undo, it will also set
    first_undo_lsn
  */
  {
    fprintf(tracef, "   state older than record, updating rows' count\n");
    info->s->state.state.records--;
  }
  fprintf(tracef, "   rows' count %lu\n", (ulong)info->s->state.state.records);
  error= 0;
end:
  return error;
}


prototype_exec_hook(COMMIT)
{
  uint16 sid= rec->short_trid;
  TrID long_trid= all_active_trans[sid].long_trid;
  LSN gslsn= all_active_trans[sid].group_start_lsn;
  char llbuf[22];
  if (long_trid == 0)
  {
    fprintf(tracef, "We don't know about transaction with short_trid %u;"
           "it probably committed long ago, forget it\n", sid);
    return 0;
  }
  llstr(long_trid, llbuf);
  fprintf(tracef, "Transaction long_trid %s short_trid %u committed", llbuf, sid);
  if (gslsn != LSN_IMPOSSIBLE)
  {
    /*
      It's not an error, it may be that trn got a disk error when writing to a
      table, so an unfinished group staid in the log.
    */
    fprintf(tracef, ", with group at LSN (%lu,0x%lx) short_trid %u aborted\n",
           (ulong) LSN_FILE_NO(gslsn), (ulong) LSN_OFFSET(gslsn), sid);
    all_active_trans[sid].group_start_lsn= LSN_IMPOSSIBLE;
  }
  else
    fprintf(tracef, "\n");
  bzero(&all_active_trans[sid], sizeof(all_active_trans[sid]));
#ifdef MARIA_VERSIONING
  /*
    if real recovery:
    transaction was committed, move it to some separate list for later
    purging (but don't purge now! purging may have been started before, we
    may find REDO_PURGE records soon).
  */
#endif
  return 0;
}


/* Just to inform about any aborted groups or unfinished transactions */
static int end_of_redo_phase()
{
  uint sid, unfinished= 0, error= 0;
  for (sid= 0; sid <= SHORT_TRID_MAX; sid++)
  {
    TrID long_trid= all_active_trans[sid].long_trid;
    LSN gslsn= all_active_trans[sid].group_start_lsn;
    if (all_active_trans[sid].undo_lsn != LSN_IMPOSSIBLE)
    {
      char llbuf[22];
      llstr(long_trid, llbuf);
      fprintf(tracef, "Transaction long_trid %s short_trid %u unfinished\n",
             llbuf, sid);
      unfinished++;
    }
    if (gslsn != LSN_IMPOSSIBLE)
    {
      fprintf(tracef, "Group at LSN (%lu,0x%lx) short_trid %u aborted\n",
             (ulong) LSN_FILE_NO(gslsn), (ulong) LSN_OFFSET(gslsn), sid);
      ALERT_USER();
    }
    /* If real recovery: roll back unfinished transaction */
#ifdef MARIA_VERSIONING
    /*
      If real recovery: transaction was committed, move it to some separate
      list for soon purging. Create TRNs.
    */
#endif
  }
 /*
    We don't close tables if there are some unfinished transactions, because
    closing tables normally requires that all unfinished transactions on them
    be rolled back. Unfinished transactions are symptom of a crash, we
    reproduce the crash.
    For example, closing will soon write the state to disk and when doing that
    it will think this is a committed state, but it may not be.
  */
  if (unfinished > 0)
    fprintf(tracef, "WARNING: %u unfinished transactions; some tables may be"
           " left inconsistent!\n", unfinished);
  for (sid= 0; sid <= SHARE_ID_MAX; sid++)
  {
    MARIA_HA *info= all_tables[sid];
    if (info != NULL)
    {
      /* if error, still close other tables */
      error|= close_recovered_table(info);
    }
  }
  return error;
}


static int close_recovered_table(MARIA_HA *info)
{
  int error;
  MARIA_SHARE *share= info->s;
  fprintf(tracef, "   Closing table '%s'\n", share->open_file_name);
  _ma_reenable_logging_for_table(share);
  /*
    Recovery normally corrected problems, don't scare user with "table was not
    closed properly" in CHECK TABLE and don't automatically check table at
    next open (when we have --maria-recover).
  */
  share->state.open_count= share->global_changed ? 1 : 0;
  /* this var is set only by non-recovery operations (mi_write() etc) */
  DBUG_ASSERT(!share->global_changed);
  if ((error= maria_close(info)))
    fprintf(tracef, "Failed to close table\n");
  return error;
}


static MARIA_HA *get_MARIA_HA_from_REDO_record(const
                                               TRANSLOG_HEADER_BUFFER *rec)
{
  uint16 sid;
  ulonglong page;
  MARIA_HA *info;
  char llbuf[22];

  sid= fileid_korr(rec->header);
  page= page_korr(rec->header + FILEID_STORE_SIZE);
  /* BUG not correct for REDO_PURGE_BLOCKS, page is not at this pos */
  llstr(page, llbuf);
  fprintf(tracef, "   For page %s of table of short id %u", llbuf, sid);
  info= all_tables[sid];
  if (info == NULL)
  {
    fprintf(tracef, ", table skipped, so skipping record\n");
    return NULL;
  }
  fprintf(tracef, ", '%s'", info->s->open_file_name);
  /* detect if an open instance of a dropped table (internal bug) */
  DBUG_ASSERT(info->s->last_version != 0);
  if (cmp_translog_addr(info->s->state.create_rename_lsn, rec->lsn) >= 0)
  {
    fprintf(tracef, ", has create_rename_lsn (%lu,0x%lx) more recent than log"
           " record\n",
           (ulong) LSN_FILE_NO(rec->lsn), (ulong) LSN_OFFSET(rec->lsn));
    return NULL;
  }
  fprintf(tracef, ", applying record\n");
  return info;
  /*
    Soon we will also skip the page depending on the rec_lsn for this page in
    the checkpoint record, but this is not absolutely needed for now (just
    assume we have made no checkpoint). Btw rec_lsn and bitmap's recovery is a
    an unsolved problem (rec_lsn is to ignore a REDO without reading the data
    page and to do so we need to be sure the corresponding bitmap page does
    not need a _ma_bitmap_set()).
  */
}


static MARIA_HA *get_MARIA_HA_from_UNDO_record(const
                                               TRANSLOG_HEADER_BUFFER *rec)
{
  uint16 sid;
  MARIA_HA *info;

  sid= fileid_korr(rec->header + LSN_STORE_SIZE);
  fprintf(tracef, "   For table of short id %u", sid);
  info= all_tables[sid];
  if (info == NULL)
  {
    fprintf(tracef, ", table skipped, so skipping record\n");
    return NULL;
  }
  fprintf(tracef, ", '%s'", info->s->open_file_name);
  DBUG_ASSERT(info->s->last_version != 0);
  if (cmp_translog_addr(info->s->state.create_rename_lsn, rec->lsn) >= 0)
  {
    fprintf(tracef, ", has create_rename_lsn (%lu,0x%lx) more recent than log"
           " record\n",
           (ulong) LSN_FILE_NO(rec->lsn), (ulong) LSN_OFFSET(rec->lsn));
    return NULL;
  }
  fprintf(tracef, ", applying record\n");
  return info;
  /*
    Soon we will also skip the page depending on the rec_lsn for this page in
    the checkpoint record, but this is not absolutely needed for now (just
    assume we have made no checkpoint).
  */
}




/* some comments and pseudo-code which we keep for later */
#if 0
  /*
    MikaelR suggests: support checkpoints during REDO phase too: do checkpoint
    after a certain amount of log records have been executed. This helps
    against repeated crashes. Those checkpoints could not be user-requested
    (as engine is not communicating during the REDO phase), so they would be
    automatic: this changes the original assumption that we don't write to the
    log while in the REDO phase, but why not. How often should we checkpoint?
  */

  /*
    We want to have two steps:
    engine->recover_with_max_memory();
    next_engine->recover_with_max_memory();
    engine->init_with_normal_memory();
    next_engine->init_with_normal_memory();
    So: in recover_with_max_memory() allocate a giant page cache, do REDO
    phase, then all page cache is flushed and emptied and freed (only retain
    small structures like TM): take full checkpoint, which is useful if
    next engine crashes in its recovery the next second.
    Destroy all shares (maria_close()), then at init_with_normal_memory() we
    do this:
  */

  /**** UNDO PHASE *****/

  print_information_to_error_log(nb of trans to roll back, nb of prepared trans
  /*
    Launch one or more threads to do the background rollback. Don't wait for
    them to complete their rollback (background rollback; for debugging, we
    can have an option which waits). Set a counter (total_of_rollback_threads)
    to the number of threads to lauch.

    Note that InnoDB's rollback-in-background works as long as InnoDB is the
    last engine to recover, otherwise MySQL will refuse new connections until
    the last engine has recovered so it's not "background" from the user's
    point of view. InnoDB is near top of sys_table_types so all others
    (e.g. BDB) recover after it... So it's really "online rollback" only if
    InnoDB is the only engine.
  */

  /* wake up delete/update handler */
  /* tell the TM that it can now accept new transactions */

  /*
    mark that checkpoint requests are now allowed.
  */
}

pthread_handler_decl rollback_background_thread()
{
  /*
    execute the normal runtime-rollback code for a bunch of transactions.
  */
  while (trans in list_of_trans_to_rollback_by_this_thread)
  {
    while (trans->undo_lsn != 0)
    {
      /* this is the normal runtime-rollback code: */
      record= log_read_record(trans->undo_lsn);
      execute_log_record_in_undo_phase(record);
      trans->undo_lsn= record.prev_undo_lsn;
    }
    /* remove trans from list */
  }
  lock_mutex(rollback_threads); /* or atomic counter */
  if (--total_of_rollback_threads == 0)
  {
    /*
      All rollback threads are done.  Print "rollback finished" to the error
      log and take a full checkpoint.
    */
  }
  unlock_mutex(rollback_threads);
  pthread_exit();
}
#endif
