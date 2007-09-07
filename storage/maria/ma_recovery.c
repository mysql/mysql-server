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
#include "trnman.h"

struct st_trn_for_recovery /* used only in the REDO phase */
{
  LSN group_start_lsn, undo_lsn, first_undo_lsn;
  TrID long_trid;
};
struct st_dirty_page /* used only in the REDO phase */
{
  uint64 file_and_page_id;
  LSN rec_lsn;
};
struct st_table_for_recovery /* used in the REDO and UNDO phase */
{
  MARIA_HA *info;
  File org_kfile, org_dfile; /**< OS descriptors when Checkpoint saw table */
};
/* Variables used by all functions of this module. Ok as single-threaded */
static struct st_trn_for_recovery *all_active_trans;
static struct st_table_for_recovery *all_tables;
static HASH all_dirty_pages;
static struct st_dirty_page *dirty_pages_pool;
static LSN current_group_end_lsn,
  checkpoint_start= LSN_IMPOSSIBLE;
static FILE *tracef; /**< trace file for debugging */

#define prototype_redo_exec_hook(R)                                          \
  static int exec_REDO_LOGREC_ ## R(const TRANSLOG_HEADER_BUFFER *rec)

#define prototype_redo_exec_hook_dummy(R)                                    \
  static int exec_REDO_LOGREC_ ## R(const TRANSLOG_HEADER_BUFFER *rec        \
                               __attribute ((unused)))

#define prototype_undo_exec_hook(R)                                          \
  static int exec_UNDO_LOGREC_ ## R(const TRANSLOG_HEADER_BUFFER *rec, TRN *trn)

prototype_redo_exec_hook(LONG_TRANSACTION_ID);
prototype_redo_exec_hook_dummy(CHECKPOINT);
prototype_redo_exec_hook(REDO_CREATE_TABLE);
prototype_redo_exec_hook(REDO_DROP_TABLE);
prototype_redo_exec_hook(FILE_ID);
prototype_redo_exec_hook(REDO_INSERT_ROW_HEAD);
prototype_redo_exec_hook(REDO_INSERT_ROW_TAIL);
prototype_redo_exec_hook(REDO_PURGE_ROW_HEAD);
prototype_redo_exec_hook(REDO_PURGE_ROW_TAIL);
prototype_redo_exec_hook(REDO_PURGE_BLOCKS);
prototype_redo_exec_hook(REDO_DELETE_ALL);
prototype_redo_exec_hook(UNDO_ROW_INSERT);
prototype_redo_exec_hook(UNDO_ROW_DELETE);
prototype_redo_exec_hook(UNDO_ROW_UPDATE);
prototype_redo_exec_hook(UNDO_ROW_PURGE);
prototype_redo_exec_hook(COMMIT);
prototype_redo_exec_hook(CLR_END);
prototype_undo_exec_hook(UNDO_ROW_INSERT);
prototype_undo_exec_hook(UNDO_ROW_DELETE);
prototype_undo_exec_hook(UNDO_ROW_UPDATE);

static int run_redo_phase(LSN lsn, my_bool apply);
static uint end_of_redo_phase(my_bool prepare_for_undo_phase);
static int run_undo_phase(uint unfinished);
static void display_record_position(const LOG_DESC *log_desc,
                                    const TRANSLOG_HEADER_BUFFER *rec,
                                    uint number);
static int display_and_apply_record(const LOG_DESC *log_desc,
                                    const TRANSLOG_HEADER_BUFFER *rec);
static MARIA_HA *get_MARIA_HA_from_REDO_record(const
                                               TRANSLOG_HEADER_BUFFER *rec);
static MARIA_HA *get_MARIA_HA_from_UNDO_record(const
                                               TRANSLOG_HEADER_BUFFER *rec);
static void prepare_table_for_close(MARIA_HA *info, LSN at_lsn);
static int parse_checkpoint_record(LSN lsn);
static void new_transaction(uint16 sid, TrID long_id, LSN undo_lsn,
                            LSN first_undo_lsn);
static int new_table(uint16 sid, const char *name,
                     File org_kfile, File org_dfile, LSN lsn);
static int new_page(File fileid, pgcache_page_no_t pageid, LSN rec_lsn,
                    struct st_dirty_page *dirty_page);
static int close_all_tables();

/** @brief global [out] buffer for translog_read_record(); never shrinks */
static LEX_STRING log_record_buffer;
#define enlarge_buffer(rec)                                             \
  if (log_record_buffer.length < (rec)->record_length)                  \
  {                                                                     \
    log_record_buffer.length= (rec)->record_length;                     \
    log_record_buffer.str= my_realloc(log_record_buffer.str,            \
                                      (rec)->record_length, MYF(MY_WME)); \
  }

#define ALERT_USER() DBUG_ASSERT(0)
#define LSN_IN_HEX(L) (ulong)LSN_FILE_NO(L),(ulong)LSN_OFFSET(L)


/**
   @brief Recovers from the last checkpoint.

   Runs the REDO phase using special structures, then sets up the playground
   of runtime: recreates transactions inside trnman, open tables with their
   two-byte-id mapping; takes a checkpoint and runs the UNDO phase. Closes all
   tables.
*/

int maria_recover()
{
  int res= 1;
  FILE *trace_file;
  DBUG_ENTER("maria_recover");

  DBUG_ASSERT(!maria_in_recovery);
  maria_in_recovery= TRUE;

  if ((trace_file= fopen("maria_recovery.trace", "w")))
  {
    fprintf(trace_file, "TRACE of the last MARIA recovery from mysqld\n");
    DBUG_ASSERT(maria_pagecache->inited);
    res= maria_apply_log(LSN_IMPOSSIBLE, TRUE, trace_file, TRUE);
    if (!res)
      fprintf(trace_file, "SUCCESS\n");
    fclose(trace_file);
  }
  // @todo set global_trid_generator from checkpoint or default value of 1/0,
  // and also update it when seeing LOGREC_LONG_TRANSACTION_ID
  // suggestion: add an arg to trnman_init
  maria_in_recovery= FALSE;
  DBUG_RETURN(res);
}


/**
   @brief Displays and/or applies the log

   @param  from_lsn        LSN from which log reading/applying should start;
                           LSN_IMPOSSIBLE means "use last checkpoint"
   @param  apply           if log records should be applied or not
   @param  trace_file      trace file where progress/debug messages will go

   @todo This trace_file thing is primitive; soon we will make it similar to
   ma_check_print_warning() etc, and a successful recovery does not need to
   create a trace file. But for debugging now it is useful.

   @return Operation status
     @retval 0      OK
     @retval !=0    Error
*/

int maria_apply_log(LSN from_lsn, my_bool apply, FILE *trace_file,
                    my_bool should_run_undo_phase)
{
  int error= 0;
  uint unfinished_trans;
  DBUG_ENTER("maria_apply_log");

  DBUG_ASSERT(apply || !should_run_undo_phase);
  DBUG_ASSERT(!maria_multi_threaded);
  all_active_trans= (struct st_trn_for_recovery *)
    my_malloc((SHORT_TRID_MAX + 1) * sizeof(struct st_trn_for_recovery),
              MYF(MY_ZEROFILL));
  all_tables= (struct st_table_for_recovery *)
    my_malloc((SHARE_ID_MAX + 1) * sizeof(struct st_table_for_recovery),
              MYF(MY_ZEROFILL));
  if (!all_active_trans || !all_tables)
    goto err;

  tracef= trace_file;

  if (from_lsn == LSN_IMPOSSIBLE)
  {
    if (last_checkpoint_lsn == LSN_IMPOSSIBLE)
    {
      from_lsn= translog_first_theoretical_lsn();
      /*
        as far as we have not yet any checkpoint then the very first
        log file should be present.
      */
      if (unlikely((from_lsn == LSN_IMPOSSIBLE) ||
                   (from_lsn == LSN_ERROR)))
        goto err;
    }
    else
    {
      DBUG_ASSERT(0); /* not yet implemented */
      from_lsn= parse_checkpoint_record(last_checkpoint_lsn);
      if (from_lsn == LSN_IMPOSSIBLE)
        goto err;
    }
  }

  if (run_redo_phase(from_lsn, apply))
    goto err;

  unfinished_trans= end_of_redo_phase(should_run_undo_phase);
  if (unfinished_trans == (uint)-1)
    goto err;
  if (should_run_undo_phase)
  {
    if (run_undo_phase(unfinished_trans))
      return 1;
  }
  else if (unfinished_trans > 0)
    fprintf(tracef, "WARNING: %u unfinished transactions; some tables may be"
            " left inconsistent!\n", unfinished_trans);

  /*
    we don't use maria_panic() because it would maria_end(), and Recovery does
    not want that (we want to keep some modules initialized for runtime).
  */
  if (close_all_tables())
    goto err;

  /*
    At this stage, end of recovery, trnman is left initialized. This is for
    the future, when we have an online UNDO phase or prepared transactions.
  */
  goto end;
err:
  error= 1;
  fprintf(tracef, "Recovery of tables with transaction logs FAILED\n");
end:
  hash_free(&all_dirty_pages);
  bzero(&all_dirty_pages, sizeof(all_dirty_pages));
  my_free(dirty_pages_pool, MYF(MY_ALLOW_ZERO_PTR));
  dirty_pages_pool= NULL;
  my_free(all_tables, MYF(MY_ALLOW_ZERO_PTR));
  all_tables= NULL;
  my_free(all_active_trans, MYF(MY_ALLOW_ZERO_PTR));
  all_active_trans= NULL;
  my_free(log_record_buffer.str, MYF(MY_ALLOW_ZERO_PTR));
  log_record_buffer.str= NULL;
  log_record_buffer.length= 0;
  /* we don't cleanly close tables if we hit some error (may corrupt them) */
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
          number ? "" : "   ", number, LSN_IN_HEX(rec->lsn),
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
    fprintf(tracef, "Got error when executing redo on record\n");
  return error;
}


prototype_redo_exec_hook(LONG_TRANSACTION_ID)
{
  uint16 sid= rec->short_trid;
  TrID long_trid= all_active_trans[sid].long_trid;
  /* abort group of this trn (must be of before a crash) */
  LSN gslsn= all_active_trans[sid].group_start_lsn;
  if (gslsn != LSN_IMPOSSIBLE)
  {
    fprintf(tracef, "Group at LSN (%lu,0x%lx) short_trid %u aborted\n",
            LSN_IN_HEX(gslsn), sid);
    all_active_trans[sid].group_start_lsn= LSN_IMPOSSIBLE;
  }
  if (long_trid != 0)
  {
    LSN ulsn= all_active_trans[sid].undo_lsn;
    if (ulsn != LSN_IMPOSSIBLE)
    {
      char llbuf[22];
      llstr(long_trid, llbuf);
      fprintf(tracef, "Found an old transaction long_trid %s short_trid %u"
              " with same short id as this new transaction, and has neither"
              " committed nor rollback (undo_lsn: (%lu,0x%lx))\n", llbuf,
              sid, LSN_IN_HEX(ulsn));
      goto err;
    }
  }
  long_trid= uint6korr(rec->header);
  new_transaction(sid, long_trid, LSN_IMPOSSIBLE, LSN_IMPOSSIBLE);
  goto end;
err:
  ALERT_USER();
  return 1;
end:
  return 0;
}


static void new_transaction(uint16 sid, TrID long_id, LSN undo_lsn,
                            LSN first_undo_lsn)
{
  char llbuf[22];
  all_active_trans[sid].long_trid= long_id;
  llstr(long_id, llbuf);
  fprintf(tracef, "Transaction long_trid %s short_trid %u starts\n",
          llbuf, sid);
  all_active_trans[sid].undo_lsn= undo_lsn;
  all_active_trans[sid].first_undo_lsn= first_undo_lsn;
  // @todo set_if_bigger(global_trid_generator, long_id)
  // indeed not only uncommitted transactions should bump generator,
  // committed ones too (those not seen by undo phase so not
  // into trnman_recreate)
}


prototype_redo_exec_hook_dummy(CHECKPOINT)
{
  /* the only checkpoint we care about was found via control file, ignore */
  return 0;
}


prototype_redo_exec_hook(REDO_CREATE_TABLE)
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
      fprintf(tracef, ", has create_rename_lsn (%lu,0x%lx) more recent than"
              " record, ignoring creation",
              LSN_IN_HEX(share->state.create_rename_lsn));
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
  /** @todo symlinks */
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


prototype_redo_exec_hook(REDO_DROP_TABLE)
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
      fprintf(tracef, ", has create_rename_lsn (%lu,0x%lx) more recent than"
              " record, ignoring removal",
              LSN_IN_HEX(share->state.create_rename_lsn));
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
    if (maria_extra(info, HA_EXTRA_PREPARE_FOR_DROP, NULL) ||
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


prototype_redo_exec_hook(FILE_ID)
{
  uint16 sid;
  int error= 1;
  const char *name;
  MARIA_HA *info;

  if (cmp_translog_addr(rec->lsn, checkpoint_start) < 0)
  {
    fprintf(tracef, "ignoring because before checkpoint\n");
    return 0;
  }

  enlarge_buffer(rec);
  if (log_record_buffer.str == NULL ||
      translog_read_record(rec->lsn, 0, rec->record_length,
                           log_record_buffer.str, NULL) !=
       rec->record_length)
  {
    fprintf(tracef, "Failed to read record\n");
    goto end;
  }
  sid= fileid_korr(log_record_buffer.str);
  info= all_tables[sid].info;
  if (info != NULL)
  {
    fprintf(tracef, "   Closing table '%s'\n", info->s->open_file_name);
    prepare_table_for_close(info, rec->lsn);
    if (maria_close(info))
    {
      fprintf(tracef, "Failed to close table\n");
      goto end;
    }
    all_tables[sid].info= NULL;
  }
  name= log_record_buffer.str + FILEID_STORE_SIZE;
  if (new_table(sid, name, -1, -1, rec->lsn))
    goto end;
  error= 0;
end:
  return error;
}


static int new_table(uint16 sid, const char *name,
                     File org_kfile, File org_dfile, LSN lsn)
{
  /*
    -1 (skip table): close table and return 0;
    1 (error): close table and return 1;
    0 (success): leave table open and return 0.
  */
  int error= 1;

  fprintf(tracef, "Table '%s', id %u", name, sid);
  MARIA_HA *info= maria_open(name, O_RDWR, HA_OPEN_FOR_REPAIR);
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
  MARIA_SHARE *share= info->s;
  /* check that we're not already using it */
  DBUG_ASSERT(share->reopen == 1);
  DBUG_ASSERT(share->now_transactional == share->base.born_transactional);
  if (!share->base.born_transactional)
  {
    fprintf(tracef, ", is not transactional\n");
    ALERT_USER();
    error= -1;
    goto end;
  }
  if (cmp_translog_addr(lsn, share->state.create_rename_lsn) <= 0)
  {
    fprintf(tracef, ", has create_rename_lsn (%lu,0x%lx) more recent than"
            " record, ignoring open request",
            LSN_IN_HEX(share->state.create_rename_lsn));
    error= -1;
    goto end;
  }
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
  if ((dfile_len % share->block_size) > 0)
  {
    fprintf(tracef, ", has too short last page\n");
    /* Recovery will fix this, no error */
    ALERT_USER();
  }
  all_tables[sid].info= info;
  all_tables[sid].org_kfile= org_kfile;
  all_tables[sid].org_dfile= org_dfile;
  fprintf(tracef, ", opened");
  error= 0;
end:
  fprintf(tracef, "\n");
  if (error)
  {
    if (info != NULL)
      maria_close(info);
    if (error == -1)
      error= 0;
  }
  return error;
}


prototype_redo_exec_hook(REDO_INSERT_ROW_HEAD)
{
  int error= 1;
  uchar *buff= NULL;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
  {
    /*
      Table was skipped at open time (because later dropped/renamed, not
      transactional, or create_rename_lsn newer than LOGREC_FILE_ID); it is
      not an error.
    */
    return 0;
  }
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


prototype_redo_exec_hook(REDO_INSERT_ROW_TAIL)
{
  int error= 1;
  uchar *buff;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    return 0;
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


prototype_redo_exec_hook(REDO_PURGE_ROW_HEAD)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    return 0;
  if (_ma_apply_redo_purge_row_head_or_tail(info, current_group_end_lsn,
                                            HEAD_PAGE,
                                            rec->header + FILEID_STORE_SIZE))
    goto end;
  error= 0;
end:
  return error;
}


prototype_redo_exec_hook(REDO_PURGE_ROW_TAIL)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    return 0;
  if (_ma_apply_redo_purge_row_head_or_tail(info, current_group_end_lsn,
                                            TAIL_PAGE,
                                            rec->header + FILEID_STORE_SIZE))
    goto end;
  error= 0;
end:
  return error;
}


prototype_redo_exec_hook(REDO_PURGE_BLOCKS)
{
  int error= 1;
  uchar *buff;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    return 0;
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
  if (_ma_apply_redo_purge_blocks(info, current_group_end_lsn,
                                  buff + FILEID_STORE_SIZE))
    goto end;
  error= 0;
end:
  return error;
}


prototype_redo_exec_hook(REDO_DELETE_ALL)
{
  int error= 1;
  MARIA_HA *info= get_MARIA_HA_from_REDO_record(rec);
  if (info == NULL)
    return 0;
  fprintf(tracef, "   deleting all %lu rows\n",
         (ulong)info->s->state.state.records);
  if (maria_delete_all_rows(info))
    goto end;
  error= 0;
end:
  return error;
}


#define set_undo_lsn_for_active_trans(TRID, LSN) do {  \
    all_active_trans[TRID].undo_lsn= LSN;                            \
    if (all_active_trans[TRID].first_undo_lsn == LSN_IMPOSSIBLE)   \
      all_active_trans[TRID].first_undo_lsn= LSN; } while (0)

prototype_redo_exec_hook(UNDO_ROW_INSERT)
{
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  if (info == NULL)
    return 0;
  set_undo_lsn_for_active_trans(rec->short_trid, rec->lsn);
  if (cmp_translog_addr(rec->lsn, info->s->state.is_of_lsn) > 0)
  {
    fprintf(tracef, "   state older than record, updating rows' count\n");
    info->s->state.state.records++;
    /** @todo RECOVERY BUG Also update the table's checksum */
    /**
       @todo some bits below will rather be set when executing UNDOs related
       to keys
    */
    info->s->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED |
      STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES;
  }
  fprintf(tracef, "   rows' count %lu\n", (ulong)info->s->state.state.records);
  return 0;
}


prototype_redo_exec_hook(UNDO_ROW_DELETE)
{
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  if (info == NULL)
    return 0;
  set_undo_lsn_for_active_trans(rec->short_trid, rec->lsn);
  if (cmp_translog_addr(rec->lsn, info->s->state.is_of_lsn) > 0)
  {
    fprintf(tracef, "   state older than record, updating rows' count\n");
    info->s->state.state.records--;
    info->s->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED |
      STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES;
  }
  fprintf(tracef, "   rows' count %lu\n", (ulong)info->s->state.state.records);
  return 0;
}


prototype_redo_exec_hook(UNDO_ROW_UPDATE)
{
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  if (info == NULL)
    return 0;
  set_undo_lsn_for_active_trans(rec->short_trid, rec->lsn);
  if (cmp_translog_addr(rec->lsn, info->s->state.is_of_lsn) > 0)
  {
    info->s->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED |
      STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES;
  }
  return 0;
}


prototype_redo_exec_hook(UNDO_ROW_PURGE)
{
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  if (info == NULL)
    return 0;
  /* this a bit broken, but this log record type will be deleted soon */
  set_undo_lsn_for_active_trans(rec->short_trid, rec->lsn);
  if (cmp_translog_addr(rec->lsn, info->s->state.is_of_lsn) > 0)
  {
    fprintf(tracef, "   state older than record, updating rows' count\n");
    info->s->state.state.records--;
    info->s->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED |
      STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES;
  }
  fprintf(tracef, "   rows' count %lu\n", (ulong)info->s->state.state.records);
  return 0;
}


prototype_redo_exec_hook(COMMIT)
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


prototype_redo_exec_hook(CLR_END)
{
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  if (info == NULL)
    return 0;
  LSN previous_undo_lsn= lsn_korr(rec->header);
  enum translog_record_type undone_record_type=
    (rec->header)[LSN_STORE_SIZE + FILEID_STORE_SIZE];
  const LOG_DESC *log_desc= &log_record_type_descriptor[undone_record_type];

  set_undo_lsn_for_active_trans(rec->short_trid, previous_undo_lsn);
  fprintf(tracef, "   CLR_END was about %s, undo_lsn now LSN (%lu,0x%lx)\n",
          log_desc->name, LSN_IN_HEX(previous_undo_lsn));
  if (cmp_translog_addr(rec->lsn, info->s->state.is_of_lsn) > 0)
  {
    fprintf(tracef, "   state older than record, updating rows' count\n");
    switch (undone_record_type) {
    case LOGREC_UNDO_ROW_DELETE:
      info->s->state.state.records++;
      break;
    case LOGREC_UNDO_ROW_INSERT:
      info->s->state.state.records--;
      break;
    default:
      DBUG_ASSERT(0);
    }
    info->s->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED |
      STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES;
  }
  fprintf(tracef, "   rows' count %lu\n", (ulong)info->s->state.state.records);
  return 0;
}


prototype_undo_exec_hook(UNDO_ROW_INSERT)
{
  my_bool error;
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  LSN previous_undo_lsn= lsn_korr(rec->header);

  if (info == NULL)
  {
    /*
      Unlike for REDOs, if the table was skipped it is abnormal; we have a
      transaction to rollback which used this table, as it is not rolled back
      it was supposed to hold this table and so the table should still be
      there.
    */
    return 1;
  }
  info->s->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED |
    STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES;

  info->trn= trn;
  error= _ma_apply_undo_row_insert(info, previous_undo_lsn,
                                   rec->header + LSN_STORE_SIZE +
                                   FILEID_STORE_SIZE);
  info->trn= 0;
  /* trn->undo_lsn is updated in an inwrite_hook when writing the CLR_END */
  fprintf(tracef, "   rows' count %lu\n", (ulong)info->s->state.state.records);
  fprintf(tracef, "   undo_lsn now LSN (%lu,0x%lx)\n",
          LSN_IN_HEX(previous_undo_lsn));
  return error;
}


prototype_undo_exec_hook(UNDO_ROW_DELETE)
{
  my_bool error;
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  LSN previous_undo_lsn= lsn_korr(rec->header);

  if (info == NULL)
    return 1;

  info->s->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED |
    STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES;

  enlarge_buffer(rec);
  if (log_record_buffer.str == NULL ||
      translog_read_record(rec->lsn, 0, rec->record_length,
                           log_record_buffer.str, NULL) !=
       rec->record_length)
  {
    fprintf(tracef, "Failed to read record\n");
    return 1;
  }

  info->trn= trn;
  /*
    For now we skip the page and directory entry. This is to be used
    later when we mark rows as deleted.
  */
  error= _ma_apply_undo_row_delete(info, previous_undo_lsn,
                                   log_record_buffer.str + LSN_STORE_SIZE +
                                   FILEID_STORE_SIZE + PAGE_STORE_SIZE +
                                   DIRPOS_STORE_SIZE,
                                   rec->record_length -
                                   (LSN_STORE_SIZE + FILEID_STORE_SIZE +
                                    PAGE_STORE_SIZE + DIRPOS_STORE_SIZE));
  info->trn= 0;
  fprintf(tracef, "   rows' count %lu\n", (ulong)info->s->state.state.records);
  fprintf(tracef, "   undo_lsn now LSN (%lu,0x%lx)\n",
          LSN_IN_HEX(previous_undo_lsn));
  return error;
}


prototype_undo_exec_hook(UNDO_ROW_UPDATE)
{
  my_bool error;
  MARIA_HA *info= get_MARIA_HA_from_UNDO_record(rec);
  LSN previous_undo_lsn= lsn_korr(rec->header);

  if (info == NULL)
    return 1;

  info->s->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED |
    STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES;

  enlarge_buffer(rec);
  if (log_record_buffer.str == NULL ||
      translog_read_record(rec->lsn, 0, rec->record_length,
                           log_record_buffer.str, NULL) !=
       rec->record_length)
  {
    fprintf(tracef, "Failed to read record\n");
    return 1;
  }

  info->trn= trn;
  /*
    For now we skip the page and directory entry. This is to be used
    later when we mark rows as deleted.
  */
  error= _ma_apply_undo_row_update(info, previous_undo_lsn,
                                   log_record_buffer.str + LSN_STORE_SIZE +
                                   FILEID_STORE_SIZE + PAGE_STORE_SIZE +
                                   DIRPOS_STORE_SIZE,
                                   rec->record_length -
                                   (LSN_STORE_SIZE + FILEID_STORE_SIZE +
                                    PAGE_STORE_SIZE + DIRPOS_STORE_SIZE));
  info->trn= 0;
  fprintf(tracef, "   undo_lsn now LSN (%lu,0x%lx)\n",
          LSN_IN_HEX(previous_undo_lsn));
  return error;
}


static int run_redo_phase(LSN lsn, my_bool apply)
{
  /* install hooks for execution */
#define install_redo_exec_hook(R)                                        \
  log_record_type_descriptor[LOGREC_ ## R].record_execute_in_redo_phase= \
    exec_REDO_LOGREC_ ## R;
#define install_undo_exec_hook(R)                                        \
  log_record_type_descriptor[LOGREC_ ## R].record_execute_in_undo_phase= \
    exec_UNDO_LOGREC_ ## R;
  install_redo_exec_hook(LONG_TRANSACTION_ID);
  install_redo_exec_hook(CHECKPOINT);
  install_redo_exec_hook(REDO_CREATE_TABLE);
  install_redo_exec_hook(REDO_DROP_TABLE);
  install_redo_exec_hook(FILE_ID);
  install_redo_exec_hook(REDO_INSERT_ROW_HEAD);
  install_redo_exec_hook(REDO_INSERT_ROW_TAIL);
  install_redo_exec_hook(REDO_PURGE_ROW_HEAD);
  install_redo_exec_hook(REDO_PURGE_ROW_TAIL);
  install_redo_exec_hook(REDO_PURGE_BLOCKS);
  install_redo_exec_hook(REDO_DELETE_ALL);
  install_redo_exec_hook(UNDO_ROW_INSERT);
  install_redo_exec_hook(UNDO_ROW_DELETE);
  install_redo_exec_hook(UNDO_ROW_UPDATE);
  install_redo_exec_hook(UNDO_ROW_PURGE);
  install_redo_exec_hook(COMMIT);
  install_redo_exec_hook(CLR_END);
  install_undo_exec_hook(UNDO_ROW_INSERT);
  install_undo_exec_hook(UNDO_ROW_DELETE);
  install_undo_exec_hook(UNDO_ROW_UPDATE);

  current_group_end_lsn= LSN_IMPOSSIBLE;

  TRANSLOG_HEADER_BUFFER rec;
  /*
    instead of this block below we will soon use
    translog_first_lsn_in_log()...
  */
  int len= translog_read_record_header(lsn, &rec);

  /** @todo EOF should be detected */
  if (len == RECHEADER_READ_ERROR)
  {
    fprintf(tracef, "Cannot find a first record\n");
    return 1;
  }
  struct st_translog_scanner_data scanner;
  if (translog_init_scanner(lsn, 1, &scanner))
  {
    fprintf(tracef, "Scanner init failed\n");
    return 1;
  }
  uint i;
  for (i= 1;;i++)
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
            return 1;
          }
          if (translog_init_scanner(rec2.lsn, 1, &scanner2))
          {
            fprintf(tracef, "Scanner2 init failed\n");
            return 1;
          }
          current_group_end_lsn= rec.lsn;
          do
          {
            if (rec2.short_trid == sid) /* it's in our group */
            {
              const LOG_DESC *log_desc2= &log_record_type_descriptor[rec2.type];
              display_record_position(log_desc2, &rec2, 0);
              if (apply && display_and_apply_record(log_desc2, &rec2))
                return 1;
            }
            len= translog_read_next_record_header(&scanner2, &rec2);
            if (len < 0) /* EOF or error */
            {
              fprintf(tracef, "Cannot find record where it should be\n");
              return 1;
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
        return 1;
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
        return 1;
      }
      break;
    }
  }
  translog_free_record_header(&rec);
  return 0;
}


/**
   @brief Informs about any aborted groups or unfinished transactions,
   prepares for the UNDO phase if needed.

   @param  prepare_for_undo_phase

   @note Observe that it may init trnman.
*/
static uint end_of_redo_phase(my_bool prepare_for_undo_phase)
{
  uint sid, unfinished= 0;

  hash_free(&all_dirty_pages);
  /*
    hash_free() can be called multiple times probably, but be safe it that
    changes
  */
  bzero(&all_dirty_pages, sizeof(all_dirty_pages));
  my_free(dirty_pages_pool, MYF(MY_ALLOW_ZERO_PTR));
  dirty_pages_pool= NULL;

  if (prepare_for_undo_phase && trnman_init())
    return -1;

  for (sid= 0; sid <= SHORT_TRID_MAX; sid++)
  {
    TrID long_trid= all_active_trans[sid].long_trid;
    LSN gslsn= all_active_trans[sid].group_start_lsn;
    TRN *trn;
    if (gslsn != LSN_IMPOSSIBLE)
    {
      fprintf(tracef, "Group at LSN (%lu,0x%lx) short_trid %u aborted\n",
             (ulong) LSN_FILE_NO(gslsn), (ulong) LSN_OFFSET(gslsn), sid);
      ALERT_USER();
    }
    if (all_active_trans[sid].undo_lsn != LSN_IMPOSSIBLE)
    {
      char llbuf[22];
      llstr(long_trid, llbuf);
      fprintf(tracef, "Transaction long_trid %s short_trid %u unfinished\n",
             llbuf, sid);
      /* dummy_transaction_object serves only for DDLs */
      DBUG_ASSERT(long_trid != 0);
      if (prepare_for_undo_phase)
      {
        if ((trn= trnman_recreate_trn_from_recovery(sid, long_trid)) == NULL)
          return -1;
        trn->undo_lsn= all_active_trans[sid].undo_lsn;
        trn->first_undo_lsn= all_active_trans[sid].first_undo_lsn |
          TRANSACTION_LOGGED_LONG_ID; /* because trn is known in log */
      }
      /* otherwise we will just warn about it */
      unfinished++;
    }
#ifdef MARIA_VERSIONING
    /*
      If real recovery: if transaction was committed, move it to some separate
      list for soon purging.
    */
#endif
  }

  my_free(all_active_trans, MYF(MY_ALLOW_ZERO_PTR));
  all_active_trans= NULL;

  /*
    The UNDO phase uses some normal run-time code of ROLLBACK: generates log
    records, etc; prepare tables for that
  */
  LSN addr= translog_get_horizon();
  for (sid= 0; sid <= SHARE_ID_MAX; sid++)
  {
    MARIA_HA *info= all_tables[sid].info;
    if (info != NULL)
    {
      prepare_table_for_close(info, addr);
      /*
        But we don't close it; we leave it available for the UNDO phase;
        it's likely that the UNDO phase will need it.
      */
      if (prepare_for_undo_phase)
        translog_assign_id_to_share_from_recovery(info->s, sid);
    }
  }

  /*
    We could take a checkpoint here, in case of a crash during the UNDO
    phase. The drawback is that a page which got a REDO (thus, flushed
    by this would-be checkpoint) is likely to have an UNDO executed on it
    soon. And so, the flush was probably lost time.
    So for now we prefer to do recovery with maximum speed and take a
    checkpoint only at the end of the UNDO phase.
  */

  return unfinished;
}


static int run_undo_phase(uint unfinished)
{
  if (unfinished > 0)
  {
    fprintf(tracef, "%u transactions will be rolled back\n", unfinished);
    for( ; unfinished-- ; )
    {
      char llbuf[22];
      TRN *trn= trnman_get_any_trn();
      DBUG_ASSERT(trn != NULL);
      llstr(trn->trid, llbuf);
      fprintf(tracef, "Rolling back transaction of long id %s\n", llbuf);

      /* Execute all undo entries */
      while (trn->undo_lsn)
      {
        TRANSLOG_HEADER_BUFFER rec;
        LOG_DESC *log_desc;
        if (translog_read_record_header(trn->undo_lsn, &rec) ==
            RECHEADER_READ_ERROR)
          return 1;
        log_desc= &log_record_type_descriptor[rec.type];
        display_record_position(log_desc, &rec, 0);
        if (log_desc->record_execute_in_undo_phase(&rec, trn))
        {
          fprintf(tracef, "Got error when executing undo\n");
          return 1;
        }
      }

      if (trnman_rollback_trn(trn))
        return 1;
      /* We could want to span a few threads (4?) instead of 1 */
      /* In the future, we want to have this phase *online* */
    }
  }
  return 0;
}


/**
   @brief re-enables transactionality, updates is_of_lsn

   @param  info                table
   @param  at_lsn              LSN to set is_of_lsn
*/

static void prepare_table_for_close(MARIA_HA *info, LSN at_lsn)
{
  MARIA_SHARE *share= info->s;
  /*
    State is now at least as new as the LSN of the current record. It may be
    newer, in case we are seeing a LOGREC_FILE_ID which tells us to close a
    table, but that table was later modified further in the log.
  */
  if (cmp_translog_addr(share->state.is_of_lsn, at_lsn) < 0)
    share->state.is_of_lsn= at_lsn;
  _ma_reenable_logging_for_table(share);
}


static MARIA_HA *get_MARIA_HA_from_REDO_record(const
                                               TRANSLOG_HEADER_BUFFER *rec)
{
  uint16 sid;
  pgcache_page_no_t page;
  MARIA_HA *info;
  char llbuf[22];

  sid= fileid_korr(rec->header);
  page= page_korr(rec->header + FILEID_STORE_SIZE);
  /**
     @todo RECOVERY BUG
     - for REDO_PURGE_BLOCKS, page is not at this pos
     - for DELETE_ALL, record ends here! buffer overrun!
     Solution: caller should pass a param enum { i_am_about_data_file,
     i_am_about_index_file, none }.
  */
  llstr(page, llbuf);
  fprintf(tracef, "   For page %s of table of short id %u", llbuf, sid);
  info= all_tables[sid].info;
  if (info == NULL)
  {
    fprintf(tracef, ", table skipped, so skipping record\n");
    return NULL;
  }
  fprintf(tracef, ", '%s'", info->s->open_file_name);
  /* detect if an open instance of a dropped table (internal bug) */
  DBUG_ASSERT(info->s->last_version != 0);
  if (cmp_translog_addr(rec->lsn, checkpoint_start) < 0)
  {
    /**
       @todo RECOVERY BUG always assuming this is REDO for data file, but it
       could soon be index file
    */
    uint64 file_and_page_id=
      (((uint64)all_tables[sid].org_dfile) << 32) | page;
    struct st_dirty_page *dirty_page= (struct st_dirty_page *)
      hash_search(&all_dirty_pages,
                  (uchar *)&file_and_page_id, sizeof(file_and_page_id));
    if ((dirty_page == NULL) ||
        cmp_translog_addr(rec->lsn, dirty_page->rec_lsn) < 0)
    {
      fprintf(tracef, ", ignoring because of dirty_pages list\n");
      return NULL;
    }
  }

  /*
    So we are going to read the page, and if its LSN is older than the
    record's we will modify the page
  */
  fprintf(tracef, ", applying record\n");
  _ma_writeinfo(info, WRITEINFO_UPDATE_KEYFILE); /* to flush state on close */
  return info;
}


static MARIA_HA *get_MARIA_HA_from_UNDO_record(const
                                               TRANSLOG_HEADER_BUFFER *rec)
{
  uint16 sid;
  MARIA_HA *info;

  sid= fileid_korr(rec->header + LSN_STORE_SIZE);
  fprintf(tracef, "   For table of short id %u", sid);
  info= all_tables[sid].info;
  if (info == NULL)
  {
    fprintf(tracef, ", table skipped, so skipping record\n");
    return NULL;
  }
  _ma_writeinfo(info, WRITEINFO_UPDATE_KEYFILE); /* to flush state on close */
  fprintf(tracef, ", '%s'", info->s->open_file_name);
  DBUG_ASSERT(info->s->last_version != 0);
  fprintf(tracef, ", applying record\n");
  return info;
}


static int parse_checkpoint_record(LSN lsn)
{
  uint i;
  TRANSLOG_HEADER_BUFFER rec;

  fprintf(tracef, "Loading data from checkpoint record\n");
  int len= translog_read_record_header(lsn, &rec);

  /** @todo EOF should be detected */
  if (len == RECHEADER_READ_ERROR)
  {
    fprintf(tracef, "Cannot find checkpoint record where it should be\n");
    return 1;
  }

  enlarge_buffer(&rec);
  if (log_record_buffer.str == NULL ||
      translog_read_record(rec.lsn, 0, rec.record_length,
                           log_record_buffer.str, NULL) !=
      rec.record_length)
  {
    fprintf(tracef, "Failed to read record\n");
    return 1;
  }

  char *ptr= log_record_buffer.str;
  checkpoint_start= lsn_korr(ptr);
  ptr+= LSN_STORE_SIZE;

  /* transactions */
  uint nb_active_transactions= uint2korr(ptr);
  ptr+= 2;
  fprintf(tracef, "%u active transactions\n", nb_active_transactions);
  LSN minimum_rec_lsn_of_active_transactions= lsn_korr(ptr);
  ptr+= LSN_STORE_SIZE;

  /*
    how much brain juice and discussions there was to come to writing this
    line
  */
  set_if_smaller(checkpoint_start, minimum_rec_lsn_of_active_transactions);

  for (i= 0; i < nb_active_transactions; i++)
  {
    uint16 sid= uint2korr(ptr);
    ptr+= 2;
    TrID long_id= uint6korr(ptr);
    ptr+= 6;
    DBUG_ASSERT(sid > 0 && long_id > 0);
    LSN undo_lsn= lsn_korr(ptr);
    ptr+= LSN_STORE_SIZE;
    LSN first_undo_lsn= lsn_korr(ptr);
    ptr+= LSN_STORE_SIZE;
    new_transaction(sid, long_id, undo_lsn, first_undo_lsn);
  }
  uint nb_committed_transactions= uint4korr(ptr);
  ptr+= 4;
  fprintf(tracef, "%lu committed transactions\n",
          (ulong)nb_committed_transactions);
  /* no purging => committed transactions are not important */
  ptr+= (6 + LSN_STORE_SIZE) * nb_committed_transactions;

  /* tables  */
  uint nb_tables= uint4korr(ptr);
  fprintf(tracef, "%u open tables\n", nb_tables);
  for (i= 0; i< nb_tables; i++)
  {
    char name[FN_REFLEN];
    uint16 sid= uint2korr(ptr);
    ptr+= 2;
    DBUG_ASSERT(sid > 0);
    File kfile= uint4korr(ptr);
    ptr+= 4;
    File dfile= uint4korr(ptr);
    ptr+= 4;
    LSN first_log_write_lsn= lsn_korr(ptr);
    ptr+= LSN_STORE_SIZE;
    uint name_len= strlen(ptr) + 1;
    ptr+= name_len;
    strnmov(name, ptr, sizeof(name));
    if (new_table(sid, name, kfile, dfile, first_log_write_lsn))
      return 1;
  }

  /* dirty pages */
  uint nb_dirty_pages= uint4korr(ptr);
  ptr+= 4;
  if (hash_init(&all_dirty_pages, &my_charset_bin, nb_dirty_pages,
                offsetof(struct st_dirty_page, file_and_page_id),
                sizeof(((struct st_dirty_page *)NULL)->file_and_page_id),
                NULL, NULL, 0))
    return 1;
  dirty_pages_pool=
    (struct st_dirty_page *)my_malloc(nb_dirty_pages *
                                      sizeof(struct st_dirty_page),
                                      MYF(MY_WME));
  if (unlikely(dirty_pages_pool == NULL))
    return 1;
  struct st_dirty_page *next_dirty_page_in_pool= dirty_pages_pool;
  LSN minimum_rec_lsn_of_dirty_pages= LSN_MAX;
  for (i= 0; i < nb_dirty_pages ; i++)
  {
    File fileid= uint4korr(ptr);
    ptr+= 4;
    pgcache_page_no_t pageid= uint4korr(ptr);
    ptr+= 4;
    LSN rec_lsn= lsn_korr(ptr);
    ptr+= LSN_STORE_SIZE;
    if (new_page(fileid, pageid, rec_lsn, next_dirty_page_in_pool++))
      return 1;
    set_if_smaller(minimum_rec_lsn_of_dirty_pages, rec_lsn);
  }
  /* after that, there will be no insert/delete into the hash */
  /*
    sanity check on record (did we screw up with all those "ptr+=", did the
    checkpoint write code and checkpoint read code go out of sync?).
  */
  /**
     @todo This probably presently and hopefully detects that
     first_log_write_lsn is not written by the checkpoint record; we need
     to add MARIA_SHARE::first_log_write_lsn, fill it with a inwrite-hook of
     LOGREC_FILE_ID (note that when we write this record we hold intern_lock,
     so Checkpoint will read the LSN correctly), and store it in the
     checkpoint record.
  */
  if (ptr != (log_record_buffer.str + log_record_buffer.length))
  {
    fprintf(tracef, "checkpoint record corrupted\n");
    return 1;
  }
  set_if_smaller(checkpoint_start, minimum_rec_lsn_of_dirty_pages);

  return 0;
}

static int new_page(File fileid, pgcache_page_no_t pageid, LSN rec_lsn,
                    struct st_dirty_page *dirty_page)
{
  /* serves as hash key */
  dirty_page->file_and_page_id= (((uint64)fileid) << 32) | pageid;
  dirty_page->rec_lsn= rec_lsn;
  return my_hash_insert(&all_dirty_pages, (uchar *)dirty_page);
}


static int close_all_tables()
{
  int error= 0;
  LIST *list_element, *next_open;
  MARIA_HA *info;
  pthread_mutex_lock(&THR_LOCK_maria);
  if (maria_open_list == NULL)
    goto end;
  fprintf(tracef, "Closing all tables\n");
  /*
    Since the end of end_of_redo_phase(), we may have written new records
    (if UNDO phase ran)  and thus the state is newer than at
    end_of_redo_phase(), we need to bump is_of_lsn again.
  */
  LSN addr= translog_get_horizon();
  for (list_element= maria_open_list ; list_element ; list_element= next_open)
  {
    next_open= list_element->next;
    info= (MARIA_HA*)list_element->data;
    pthread_mutex_unlock(&THR_LOCK_maria); /* ok, UNDO phase not online yet */
    prepare_table_for_close(info, addr);
    error|= maria_close(info);
    pthread_mutex_lock(&THR_LOCK_maria);
  }
end:
  pthread_mutex_unlock(&THR_LOCK_maria);
  return error;
}

#ifdef MARIA_EXTERNAL_LOCKING
#error Maria's Recovery is really not ready for it
#endif

/*
Recovery of the state :  how it works
=====================================

Ignoring Checkpoints for a start.

The state (MARIA_HA::MARIA_SHARE::MARIA_STATE_INFO) is updated in
memory frequently (at least at every row write/update/delete) but goes
to disk at few moments: maria_close() when closing the last open
instance, and a few rare places like CHECK/REPAIR/ALTER
(non-transactional tables also do it at maria_lock_database() but we
needn't cover them here).

In case of crash, state on disk is likely to be older than what it was
in memory, the REDO phase needs to recreate the state as it was in
memory at the time of crash. When we say Recovery here we will always
mean "REDO phase".

For example MARIA_STATUS_INFO::records (count of records). It is updated at
the end of every row write/update/delete/delete_all. When Recovery sees the
sign of such row operation (UNDO or REDO), it may need to update the records'
count if that count does not reflect that operation (is older). How to know
the age of the state compared to the log record: every time the state
goes to disk at runtime, its member "is_of_lsn" is updated to the
current end-of-log LSN. So Recovery just needs to compare is_of_lsn
and the record's LSN to know if it should modify "records".

Other operations like ALTER TABLE DISABLE KEYS update the state but
don't write log records, thus the REDO phase cannot repeat their
effect on the state in case of crash. But we make them sync the state
as soon as they have finished. This reduces the window for a problem.

It looks like only one thread at a time updates the state in memory or
on disk. However there is not 100% certainty when it comes to
HA_EXTRA_(FORCE_REOPEN|PREPARE_FOR_RENAME): can they read the state
from memory while some other thread is updating "records" in memory?
If yes, they may write a corrupted state to disk.
We assume that no for now: ASK_MONTY.

With checkpoints
================

Checkpoint module needs to read the state in memory and write it to
disk. This may happen while some other thread is modifying the state
in memory or on disk. Checkpoint thus may be reading changing data, it
needs a mutex to not have it corrupted, and concurrent modifiers of
the state need that mutex too for the same reason.
"records" is modified for every row write/update/delete, we don't want
to add a mutex lock/unlock there. So we re-use the mutex lock/unlock
which is already present in these moments, namely the log's mutex which is
taken when UNDO_ROW_INSERT|UPDATE|DELETE is written: we update "records" in
under-log-mutex hooks when writing these records (thus "records" is
not updated at the end of maria_write/update/delete() anymore).
Thus Checkpoint takes the log's lock and can read "records" from
memory an write it to disk and release log's lock.
We however want to avoid having the disk write under the log's
lock. So it has to be under another mutex, natural choice is
intern_lock (as Checkpoint needs it anyway to read MARIA_SHARE::kfile,
and as maria_close() takes it too). All state writes to disk are
changed to be protected with intern_lock.
So Checkpoint takes intern_lock, log's lock, reads "records" from
memory, releases log's lock, updates is_of_lsn and writes "records" to
disk, release intern_lock.
In practice, not only "records" needs to be written but the full
state. So, Checkpoint reads the full state from memory. Some other
thread may at this moment be modifying in memory some pieces of the
state which are not protected by the lock's log (see ma_extra.c
HA_EXTRA_NO_KEYS), and Checkpoint would be reading a corrupted state
from memory; to guard against that we extend the intern_lock-zone to
changes done to the state in memory by HA_EXTRA_NO_KEYS et al, and
also any change made in memory to create_rename_lsn/state_is_of_lsn.
Last, we don't want in Checkpoint to do
 log lock; read state from memory; release log lock;
for each table, it may hold the log's lock too much in total.
So, we instead do
 log lock; read N states from memory; release log lock;
Thus, the sequence above happens outside of any intern_lock.
But this re-introduces the problem that some other thread may be changing the
state in memory and on disk under intern_lock, without log's lock, like
HA_EXTRA_NO_KEYS, while we read the N states. However, when Checkpoint later
comes to handling the table under intern_lock, which is serialized with
HA_EXTRA_NO_KEYS, it can see that is_of_lsn is higher then when the state was
read from memory under log's lock, and thus can decide to not flush the
obsolete state it has, knowing that the other thread flushed a more recent
state already. If on the other hand is_of_lsn is not higher, the read state is
current and can be flushed. So we have a per-table sequence:
 lock intern_lock; test if is_of_lsn is higher than when we read the state
 under log's lock; if no then flush the read state to disk.
*/

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
