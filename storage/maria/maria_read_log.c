/* Copyright (C) 2007 MySQL AB

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

#include "maria_def.h"
#include <my_getopt.h>

#define PCACHE_SIZE (1024*1024*10)
#define LOG_FLAGS 0
#define LOG_FILE_SIZE (1024L*1024L)


static PAGECACHE pagecache;

static const char *load_default_groups[]= { "maria_read_log",0 };
static void get_options(int *argc,char * * *argv);
#ifndef DBUG_OFF
static const char *default_dbug_option;
#endif
static my_bool opt_only_display, opt_display_and_apply;

struct TRN_FOR_RECOVERY
{
  LSN group_start_lsn, undo_lsn;
  TrID long_trid;
};

struct TRN_FOR_RECOVERY all_active_trans[SHORT_TRID_MAX + 1];
MARIA_HA *all_tables[SHORT_TRID_MAX + 1];
LSN current_group_end_lsn= LSN_IMPOSSIBLE;

static void end_of_redo_phase();
static void display_record_position(const LOG_DESC *log_desc,
                                    const TRANSLOG_HEADER_BUFFER *rec,
                                    uint number);
static int display_and_apply_record(const LOG_DESC *log_desc,
                                    const TRANSLOG_HEADER_BUFFER *rec);
#define prototype_exec_hook(R)                                  \
static int exec_LOGREC_ ## R(const TRANSLOG_HEADER_BUFFER *rec)
prototype_exec_hook(LONG_TRANSACTION_ID);
#ifdef MARIA_CHECKPOINT
prototype_exec_hook(CHECKPOINT);
#endif
prototype_exec_hook(REDO_CREATE_TABLE);
prototype_exec_hook(FILE_ID);
prototype_exec_hook(REDO_INSERT_ROW_HEAD);
prototype_exec_hook(COMMIT);
/*
  To implement REDO_DROP_TABLE and REDO_RENAME_TABLE, we would need to go
  through the all_tables[] array, find all open instances of the
  table-to-drop-or-rename, and remove them from the array.
  We however know that in real Recovery, we don't have to handle those log
  records at all, same for REDO_CREATE_TABLE.
  So for now, we can use this program to replay/debug a sequence of CREATE +
  DMLs, but not DROP/RENAME; it is probably enough for a start.
*/

int main(int argc, char **argv)
{
  LSN lsn;
  char **default_argv;
  MY_INIT(argv[0]);

  load_defaults("my", load_default_groups, &argc, &argv);
  default_argv= argv;
  get_options(&argc, &argv);

  maria_data_root= ".";

#ifndef DBUG_OFF
#if defined(__WIN__)
  default_dbug_option= "d:t:i:O,\\maria_read_log.trace";
#else
  default_dbug_option= "d:t:i:o,/tmp/maria_read_log.trace";
#endif
  if (argc > 1)
  {
    DBUG_SET(default_dbug_option);
    DBUG_SET_INITIAL(default_dbug_option);
  }
#endif

  if (maria_init())
  {
    fprintf(stderr, "Can't init Maria engine (%d)\n", errno);
    goto err;
  }
  /* we don't want to create a control file, it MUST exist */
  if (ma_control_file_create_or_open(FALSE))
  {
    fprintf(stderr, "Can't open control file (%d)\n", errno);
    goto err;
  }
  if (last_logno == FILENO_IMPOSSIBLE)
  {
    fprintf(stderr, "Can't find any log\n");
    goto err;
  }
  if (init_pagecache(&pagecache, PCACHE_SIZE, 0, 0,
                     TRANSLOG_PAGE_SIZE) == 0)
  {
    fprintf(stderr, "Got error in init_pagecache() (errno: %d)\n", errno);
    goto err;
  }
  /*
    If log handler does not find the "last_logno" log it will return error,
    which is good.
    But if it finds a log and this log was crashed, it will create a new log,
    which is useless. TODO: start log handler in read-only mode.
  */
  if (translog_init(".", LOG_FILE_SIZE, 50112, 0, &pagecache,
                    TRANSLOG_DEFAULT_FLAGS))
  {
    fprintf(stderr, "Can't init loghandler (%d)\n", errno);
    goto err;
  }

  /* install hooks for execution */
#define install_exec_hook(R)                                            \
  log_record_type_descriptor[LOGREC_ ## R].record_execute_in_redo_phase= \
    exec_LOGREC_ ## R;
  install_exec_hook(LONG_TRANSACTION_ID);
#ifdef MARIA_CHECKPOINT
  install_exec_hook(CHECKPOINT);
#endif
  install_exec_hook(REDO_CREATE_TABLE);
  install_exec_hook(FILE_ID);
  install_exec_hook(REDO_INSERT_ROW_HEAD);
  install_exec_hook(COMMIT);

  if (opt_only_display)
    printf("You are using --only-display, NOTHING will be written to disk\n");

  lsn= first_lsn_in_log(); /*could also be last_checkpoint_lsn */

  TRANSLOG_HEADER_BUFFER rec;
  struct st_translog_scanner_data scanner;
  uint i= 1;

  translog_size_t len= translog_read_record_header(lsn, &rec);

  if (len == (TRANSLOG_RECORD_HEADER_MAX_SIZE + 1))
  {
    printf("EOF on the log\n");
    goto end;
  }

  if (translog_init_scanner(lsn, 1, &scanner))
  {
    fprintf(stderr, "Scanner init failed\n");
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
      There are pitfalls: if a table write failed, the transaction may have
      put an incomplete group in the log and then a COMMIT record, that will
      make a complete group which is wrong. We say that we should mark the
      table corrupted if such error happens (what if it cannot be marked?).
    */
    if (log_desc->record_ends_group)
    {
      if (all_active_trans[sid].group_start_lsn != LSN_IMPOSSIBLE)
      {
        /*
          There is a complete group for this transaction, containing more than
          this event.
        */
        printf("   ends a group:\n");
        struct st_translog_scanner_data scanner2;
        TRANSLOG_HEADER_BUFFER rec2;
        len=
          translog_read_record_header(all_active_trans[sid].group_start_lsn, &rec2);
        if (len == (TRANSLOG_RECORD_HEADER_MAX_SIZE + 1))
        {
          fprintf(stderr, "Cannot find record where it should be\n");
          goto err;
        }
        if (translog_init_scanner(rec2.lsn, 1, &scanner2))
        {
          fprintf(stderr, "Scanner2 init failed\n");
          goto err;
        }
        current_group_end_lsn= rec.lsn;
        do
        {
          if (rec2.short_trid == sid) /* it's in our group */
          {
            const LOG_DESC *log_desc2= &log_record_type_descriptor[rec2.type];
            display_record_position(log_desc2, &rec2, 0);
            if (display_and_apply_record(log_desc2, &rec2))
              goto err;
          }
          len= translog_read_next_record_header(&scanner2, &rec2);
          if (len == (TRANSLOG_RECORD_HEADER_MAX_SIZE + 1))
          {
            fprintf(stderr, "Cannot find record where it should be\n");
            goto err;
          }
        }
        while (rec2.lsn < rec.lsn);
        translog_free_record_header(&rec2);
        /* group finished */
        all_active_trans[sid].group_start_lsn= LSN_IMPOSSIBLE;
        current_group_end_lsn= LSN_IMPOSSIBLE; /* for debugging */
      }
      if (display_and_apply_record(log_desc, &rec))
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
    if (len == (TRANSLOG_RECORD_HEADER_MAX_SIZE + 1))
    {
      printf("EOF on the log\n");
      goto end;
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
  end_of_redo_phase();
  goto end;
err:
  /* don't touch anything more, in case we hit a bug */
  exit(1);
end:
  maria_end();
  free_defaults(default_argv);
  my_end(0);
  exit(0);
  return 0;				/* No compiler warning */
}


static struct my_option my_long_options[] =
{
  {"only-display", 'o', "display brief info about records's header",
   (gptr*) &opt_only_display, (gptr*) &opt_only_display, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"display-and-apply", 'a',
   "like --only-display but displays more info and modifies tables",
   (gptr*) &opt_display_and_apply, (gptr*) &opt_display_and_apply, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

#include <help_start.h>

static void print_version(void)
{
  VOID(printf("%s Ver 1.0 for %s on %s\n",
              my_progname, SYSTEM_TYPE, MACHINE_TYPE));
  NETWARE_SET_SCREEN_MODE(1);
}


static void usage(void)
{
  print_version();
  puts("Copyright (C) 2007 MySQL AB");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,");
  puts("and you are welcome to modify and redistribute it under the GPL license\n");

  puts("Display and apply log records from a MARIA transaction log");
  puts("found in the current directory (for now)");
  VOID(printf("\nUsage: %s OPTIONS\n", my_progname));
  puts("You need to use one of -o or -a");
  my_print_help(my_long_options);
  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}

#include <help_end.h>

static my_bool
get_one_option(int optid __attribute__((unused)),
               const struct my_option *opt __attribute__((unused)),
               char *argument __attribute__((unused)))
{
  /* for now there is nothing special with our options */
  return 0;
}

static void get_options(int *argc,char ***argv)
{
  int ho_error;

  my_progname= argv[0][0];

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if ((opt_only_display + opt_display_and_apply) != 1)
  {
    usage();
    exit(1);
  }
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
  printf("%sRec#%u LSN (%lu,0x%lx) short_trid %u %s(num_type:%u) len %lu\n",
         number ? "" : "   ", number,
         (ulong) LSN_FILE_NO(rec->lsn), (ulong) LSN_OFFSET(rec->lsn),
         rec->short_trid, log_desc->name, rec->type,
         (ulong)rec->record_length);
}


static int display_and_apply_record(const LOG_DESC *log_desc,
                                    const TRANSLOG_HEADER_BUFFER *rec)
{
  int error;
  if (opt_only_display)
    return 0;
  if (log_desc->record_execute_in_redo_phase == NULL)
  {
    /* die on all not-yet-handled records :) */
    DBUG_ASSERT("one more hook" == "to write");
  }
  if ((error= (*log_desc->record_execute_in_redo_phase)(rec)))
    fprintf(stderr, "Got error when executing record\n");
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
    printf("Group at LSN (%lu,0x%lx) short_trid %u aborted\n",
           (ulong) LSN_FILE_NO(gslsn), (ulong) LSN_OFFSET(gslsn), sid);
    all_active_trans[sid].group_start_lsn= LSN_IMPOSSIBLE;
  }
  if (long_trid != 0)
  {
    LSN ulsn= all_active_trans[sid].undo_lsn;
    if (ulsn != LSN_IMPOSSIBLE)
    {
      llstr(long_trid, llbuf);
      fprintf(stderr, "Found an old transaction long_trid %s short_trid %u"
              " with same short id as this new transaction, and has neither"
              " committed nor rollback (undo_lsn: (%lu,0x%lx))\n", llbuf,
              sid, (ulong) LSN_FILE_NO(ulsn), (ulong) LSN_OFFSET(ulsn));
      goto err;
    }
  }
  long_trid= uint6korr(rec->header);
  all_active_trans[sid].long_trid= long_trid;
  llstr(long_trid, llbuf);
  printf("Transaction long_trid %s short_trid %u starts\n", llbuf, sid);
  goto end;
err:
  DBUG_ASSERT(0);
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
  int error, create_mode= O_RDWR | O_TRUNC;
  MARIA_HA *info= NULL;
  if (((name= my_malloc(rec->record_length, MYF(MY_WME))) == NULL) ||
      (translog_read_record(rec->lsn, 0, rec->record_length, name, NULL) !=
       rec->record_length))
  {
    fprintf(stderr, "Failed to read record\n");
    goto err;
  }
  printf("Table '%s'", name);
  /* we try hard to get create_rename_lsn, to avoid mistakes if possible */
  info= maria_open(name, O_RDONLY, HA_OPEN_FOR_REPAIR);
  if (info)
  {
    DBUG_ASSERT(info->s->reopen == 1); /* check that we're not using it */
    if (!info->s->base.transactional)
    {
      /*
        could be that transactional table was later dropped, and a non-trans
        one was renamed to its name, thus create_rename_lsn is 0 and should
        not be trusted.
      */
      printf(", is not transactional\n");
      DBUG_ASSERT(0); /* I want to know this */
      goto end;
    }
    if (cmp_translog_addr(info->s->state.create_rename_lsn, rec->lsn) >= 0)
    {
      printf(", has create_rename_lsn (%lu,0x%lx) is more recent than record",
             (ulong) LSN_FILE_NO(rec->lsn),
             (ulong) LSN_OFFSET(rec->lsn));
      goto end;
    }
    if (maria_is_crashed(info))
    {
      printf(", is crashed, overwriting it");
      DBUG_ASSERT(0); /* I want to know this */
    }
    maria_close(info);
    info= NULL;
  }
  /* if does not exist, is older, or its header is corrupted, overwrite it */
  // TODO symlinks
  ptr= name + strlen(name) + 1;
  if ((flags= ptr[0] ? HA_DONT_TOUCH_DATA : 0))
    printf(", we will only touch index file");
  fn_format(filename, name, "", MARIA_NAME_IEXT,
            (MY_UNPACK_FILENAME |
             (flags & HA_DONT_TOUCH_DATA) ? MY_RETURN_REAL_PATH : 0) |
            MY_APPEND_EXT);
  linkname_ptr= NULL;
  create_flag= MY_DELETE_OLD;
  printf(", creating as '%s'", filename);
  if ((kfile= my_create_with_symlink(linkname_ptr, filename, 0, create_mode,
                                     MYF(MY_WME|create_flag))) < 0)
  {
    fprintf(stderr, "Failed to create index file\n");
    goto err;
  }
  ptr++;
  uint kfile_size_before_extension= uint2korr(ptr);
  ptr+= 2;
  uint keystart= uint2korr(ptr);
  ptr+= 2;
  /* set create_rename_lsn (for maria_read_log to be idempotent) */
  lsn_store(ptr + sizeof(info->s->state.header) + 2, rec->lsn);
  if (my_pwrite(kfile, ptr,
                kfile_size_before_extension, 0, MYF(MY_NABP|MY_WME)) ||
      my_chsize(kfile, keystart, 0, MYF(MY_WME)))
  {
    fprintf(stderr, "Failed to write to index file\n");
    goto err;
  }
  if (!(flags & HA_DONT_TOUCH_DATA))
  {
    fn_format(filename,name,"", MARIA_NAME_DEXT,
              MY_UNPACK_FILENAME | MY_APPEND_EXT);
    linkname_ptr= NULL;
    create_flag=MY_DELETE_OLD;
    if ((dfile=
         my_create_with_symlink(linkname_ptr, filename, 0, create_mode,
                                MYF(MY_WME | create_flag))) < 0)
    {
      fprintf(stderr, "Failed to create data file\n");
      goto err;
    }
    /*
      we now have an empty data file. To be able to
      _ma_initialize_data_file() we need some pieces of the share to be
      correctly filled. So we just open the table (fortunately, an empty
      data file does not preclude this).
    */
    if (((info= maria_open(name, O_RDONLY, 0)) == NULL) ||
        _ma_initialize_data_file(dfile, info->s))
    {
      fprintf(stderr, "Failed to open new table or write to data file\n");
      goto err;
    }
  }
  error= 0;
  goto end;
err:
  DBUG_ASSERT(0);
  error= 1;
end:
  printf("\n");
  if (kfile >= 0)
    error|= my_close(kfile, MYF(MY_WME));
  if (dfile >= 0)
    error|= my_close(dfile, MYF(MY_WME));
  if (info != NULL)
    error|= maria_close(info);
  my_free(name, MYF(MY_ALLOW_ZERO_PTR));
  return 0;
}


prototype_exec_hook(FILE_ID)
{
  uint16 sid;
  int error;
  char *name, *buff;
  MARIA_HA *info= NULL;
  if (((buff= my_malloc(rec->record_length, MYF(MY_WME))) == NULL) ||
      (translog_read_record(rec->lsn, 0, rec->record_length, buff, NULL) !=
       rec->record_length))
  {
    fprintf(stderr, "Failed to read record\n");
    goto err;
  }
  sid= fileid_korr(buff);
  name= buff + FILEID_STORE_SIZE;
  printf("Table '%s', id %u", name, sid);
  info= all_tables[sid];
  if (info != NULL)
  {
    printf(", closing table '%s'", info->s->open_file_name);
    all_tables[sid]= NULL;
    info->s->base.transactional= TRUE; /* put back the truth */
    if (maria_close(info))
    {
      fprintf(stderr, "Failed to close table\n");
      goto err;
    }
  }
  info= maria_open(name, O_RDWR, HA_OPEN_FOR_REPAIR);
  if (info == NULL)
  {
    printf(", is absent (must have been dropped later?)"
           " or its header is so corrupted that we cannot open it;"
           " we skip it\n");
    goto end;
  }
  if (maria_is_crashed(info))
  {
    fprintf(stderr, "Table is crashed, can't apply log records to it\n");
    goto err;
  }
  DBUG_ASSERT(info->s->reopen == 1); /* should always be only one instance */
  if (!info->s->base.transactional)
  {
    printf(", is not transactional\n");
    DBUG_ASSERT(0); /* I want to know this */
    goto end;
  }
  all_tables[sid]= info;
  /*
    don't log any records for this work. TODO make sure this variable does not
    go to disk before we restore it to its true value.
  */
  info->s->base.transactional= FALSE;
  printf(", opened\n");
  error= 0;
  goto end;
err:
  DBUG_ASSERT(0);
  error= 1;
  if (info != NULL)
    error|= maria_close(info);
end:
  my_free(buff, MYF(MY_ALLOW_ZERO_PTR));
  return 0;
}


prototype_exec_hook(REDO_INSERT_ROW_HEAD)
{
  uint16 sid;
  ulonglong page;
  MARIA_HA *info;
  char llbuf[22];
  sid= fileid_korr(rec->header);
  page= page_korr(rec->header + FILEID_STORE_SIZE);
  llstr(page, llbuf);
  printf("For page %s of table of short id %u", llbuf, sid);
  info= all_tables[sid];
  if (info == NULL)
  {
    printf(", table skipped, so skipping record\n");
    goto end;
  }
  printf(", '%s'", info->s->open_file_name);
  if (cmp_translog_addr(info->s->state.create_rename_lsn, rec->lsn) >= 0)
  {
    printf(", has create_rename_lsn (%lu,0x%lx) is more recent than log"
           " record\n",
           (ulong) LSN_FILE_NO(rec->lsn), (ulong) LSN_OFFSET(rec->lsn));
    goto end;
  }
  /*
    Soon we will also skip the page depending on the rec_lsn for this page in
    the checkpoint record, but this is not absolutely needed for now (just
    assume we have made no checkpoint).
  */
  printf(", applying record\n");
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
  DBUG_ASSERT("Monty" == "this is the place");
end:
  /* as we don't have apply working: */
  return 1;
}


prototype_exec_hook(COMMIT)
{
  uint16 sid= rec->short_trid;
  TrID long_trid= all_active_trans[sid].long_trid;
  LSN gslsn= all_active_trans[sid].group_start_lsn;
  char llbuf[22];
  if (long_trid == 0)
  {
    printf("We don't know about transaction short_trid %u;"
           "it probably committed long ago, forget it\n", sid);
    return 0;
  }
  llstr(long_trid, llbuf);
  printf("Transaction long_trid %s short_trid %u committed", llbuf, sid);
  if (gslsn != LSN_IMPOSSIBLE)
  {
    /*
      It's not an error, it may be that trn got a disk error when writing to a
      table, so an unfinished group staid in the log.
    */
    printf(", with group at LSN (%lu,0x%lx) short_trid %u aborted\n",
           (ulong) LSN_FILE_NO(gslsn), (ulong) LSN_OFFSET(gslsn), sid);
    all_active_trans[sid].group_start_lsn= LSN_IMPOSSIBLE;
  }
  else
    printf("\n");
  all_active_trans[sid].long_trid= 0;
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
static void end_of_redo_phase()
{
  uint sid, unfinished= 0;
  for (sid= 0; sid <= SHORT_TRID_MAX; sid++)
  {
    TrID long_trid= all_active_trans[sid].long_trid;
    LSN gslsn= all_active_trans[sid].group_start_lsn;
    if (long_trid == 0)
      continue;
    if (all_active_trans[sid].undo_lsn != LSN_IMPOSSIBLE)
    {
      char llbuf[22];
      llstr(long_trid, llbuf);
      printf("Transaction long_trid %s short_trid %u unfinished\n",
             llbuf, sid);
    }
    if (gslsn != LSN_IMPOSSIBLE)
    {
      printf("Group at LSN (%lu,0x%lx) short_trid %u aborted\n",
             (ulong) LSN_FILE_NO(gslsn), (ulong) LSN_OFFSET(gslsn), sid);
    }
    /* If real recovery: roll back unfinished transaction */
#ifdef MARIA_VERSIONING
    /*
      If real recovery: transaction was committed, move it to some separate
      list for soon purging.
    */
#endif
  }
 /*
    We don't close tables if there are some unfinished transactions, because
    closing tables normally requires that all unfinished transactions on them
    be rolled back.
    For example, closing will soon write the state to disk and when doing that
    it will think this is a committed state, but it may not be.
  */
  if (unfinished == 0)
  {
    for (sid= 0; sid <= SHORT_TRID_MAX; sid++)
    {
      MARIA_HA *info= all_tables[sid];
      if (info != NULL)
        maria_close(info);
    }
  }
}
