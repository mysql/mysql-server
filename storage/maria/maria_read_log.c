/* Copyright (C) 2007 MySQL AB
   Copyright (C) 2010 Monty Program Ab

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
#include "ma_recovery.h"
#include <my_getopt.h>

#define LOG_FLAGS 0

static const char *load_default_groups[]= { "aria_read_log",0 };
static void get_options(int *argc,char * * *argv);
#ifndef DBUG_OFF
#if defined(__WIN__)
const char *default_dbug_option= "d:t:O,\\aria_read_log.trace";
#else
const char *default_dbug_option= "d:t:o,/tmp/aria_read_log.trace";
#endif
#endif /* DBUG_OFF */
static my_bool opt_display_only, opt_apply, opt_apply_undo, opt_silent;
static my_bool opt_check;
static const char *opt_tmpdir;
static ulong opt_page_buffer_size;
static ulonglong opt_start_from_lsn, opt_end_lsn, opt_start_from_checkpoint;
static MY_TMPDIR maria_chk_tmpdir;


int main(int argc, char **argv)
{
  LSN lsn;
  char **default_argv;
  uint warnings_count;
  MY_INIT(argv[0]);

  load_defaults("my", load_default_groups, &argc, &argv);
  default_argv= argv;
  maria_data_root= (char *)".";
  get_options(&argc, &argv);

  maria_in_recovery= TRUE;

  if (maria_init())
  {
    fprintf(stderr, "Can't init Aria engine (%d)\n", errno);
    goto err;
  }
  maria_block_size= 0;                          /* Use block size from file */
  /* we don't want to create a control file, it MUST exist */
  if (ma_control_file_open(FALSE, TRUE))
  {
    fprintf(stderr, "Can't open control file (%d)\n", errno);
    goto err;
  }
  if (last_logno == FILENO_IMPOSSIBLE)
  {
    fprintf(stderr, "Can't find any log\n");
    goto err;
  }
  if (init_pagecache(maria_pagecache, opt_page_buffer_size, 0, 0,
                     maria_block_size, MY_WME) == 0)
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
  if (init_pagecache(maria_log_pagecache,
                     TRANSLOG_PAGECACHE_SIZE, 0, 0,
                     TRANSLOG_PAGE_SIZE, MY_WME) == 0 ||
      translog_init(maria_data_root, TRANSLOG_FILE_SIZE,
                    0, 0, maria_log_pagecache, TRANSLOG_DEFAULT_FLAGS,
                    opt_display_only))
  {
    fprintf(stderr, "Can't init loghandler (%d)\n", errno);
    goto err;
  }

  if (opt_display_only)
    printf("You are using --display-only, NOTHING will be written to disk\n");

  lsn= translog_first_lsn_in_log();
  if (lsn == LSN_ERROR)
  {
    fprintf(stderr, "Opening transaction log failed\n");
    goto end;
  }
  if (lsn == LSN_IMPOSSIBLE)
  {
     fprintf(stdout, "The transaction log is empty\n");
  }
  if (opt_start_from_checkpoint && !opt_start_from_lsn &&
      last_checkpoint_lsn != LSN_IMPOSSIBLE)
  {
    lsn= LSN_IMPOSSIBLE;             /* LSN set in maria_apply_log() */
    fprintf(stdout, "Starting from checkpoint (%lu,0x%lx)\n",
            LSN_IN_PARTS(last_checkpoint_lsn));
  }
  else
    fprintf(stdout, "The transaction log starts from lsn (%lu,0x%lx)\n",
            LSN_IN_PARTS(lsn));

  if (opt_start_from_lsn)
  {
    if (opt_start_from_lsn < (ulonglong) lsn)
    {
      fprintf(stderr, "start_from_lsn is too small. Aborting\n");
      maria_end();
      goto err;
    }
    lsn= (LSN) opt_start_from_lsn;
    fprintf(stdout, "Starting reading log from lsn (%lu,0x%lx)\n",
            LSN_IN_PARTS(lsn));
  }

  if (opt_end_lsn != LSN_IMPOSSIBLE)
  {
    /* We can't apply undo if we use end_lsn */
    opt_apply_undo= 0;
  }

  fprintf(stdout, "TRACE of the last aria_read_log\n");
  if (maria_apply_log(lsn, opt_end_lsn, opt_apply ?  MARIA_LOG_APPLY :
                      (opt_check ? MARIA_LOG_CHECK :
                       MARIA_LOG_DISPLAY_HEADER), opt_silent ? NULL : stdout,
                      opt_apply_undo, FALSE, FALSE, &warnings_count))
    goto err;
  if (warnings_count == 0)
    fprintf(stdout, "%s: SUCCESS\n", my_progname_short);
  else
    fprintf(stdout, "%s: DOUBTFUL (%u warnings, check previous output)\n",
            my_progname_short, warnings_count);

end:
  maria_end();
  free_tmpdir(&maria_chk_tmpdir);
  free_defaults(default_argv);
  my_end(0);
  exit(0);
  return 0;				/* No compiler warning */

err:
  /* don't touch anything more, in case we hit a bug */
  fprintf(stderr, "%s: FAILED\n", my_progname_short);
  free_tmpdir(&maria_chk_tmpdir);
  free_defaults(default_argv);
  exit(1);
}


#include "ma_check_standalone.h"

enum options_mc {
  OPT_CHARSETS_DIR=256
};

static struct my_option my_long_options[] =
{
  {"apply", 'a',
   "Apply log to tables: modifies tables! you should make a backup first! "
   " Displays a lot of information if not run with --silent",
   (uchar **) &opt_apply, (uchar **) &opt_apply, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.",
   (char**) &charsets_dir, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"check", 'c',
   "if --display-only, check if record is fully readable (for debugging)",
   (uchar **) &opt_check, (uchar **) &opt_check, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Output debug log. Often the argument is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"help", '?', "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"display-only", 'd', "display brief info read from records' header",
   &opt_display_only, &opt_display_only, 0, GET_BOOL,
   NO_ARG,0, 0, 0, 0, 0, 0},
  {"aria-log-dir-path", 'l',
    "Path to the directory where to store transactional log",
    (uchar **) &maria_data_root, (uchar **) &maria_data_root, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "page-buffer-size", 'P', "",
    &opt_page_buffer_size, &opt_page_buffer_size, 0,
    GET_ULONG, REQUIRED_ARG, (long) USE_BUFFER_INIT,
    (long) USE_BUFFER_INIT, (long) ~(ulong) 0, (long) MALLOC_OVERHEAD,
    (long) IO_SIZE, 0},
  { "start-from-lsn", 'o', "Start reading log from this lsn",
    &opt_start_from_lsn, &opt_start_from_lsn,
    0, GET_ULL, REQUIRED_ARG, 0, 0, ~(longlong) 0, 0, 0, 0 },
  {"start-from-checkpoint", 'C', "Start applying from last checkpoint",
   &opt_start_from_checkpoint, &opt_start_from_checkpoint, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "end-lsn", 'e', "Stop applying at this lsn. If end-lsn is used, UNDO:s "
    "will not be applied", &opt_end_lsn, &opt_end_lsn,
    0, GET_ULL, REQUIRED_ARG, 0, 0, ~(longlong) 0, 0, 0, 0 },
  {"silent", 's', "Print less information during apply/undo phase",
   &opt_silent, &opt_silent, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"tables-to-redo", 'T',
   "List of tables sepearated with , that we should apply REDO on. Use this if you only want to recover some tables",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Print more information during apply/undo phase",
   &maria_recovery_verbose, &maria_recovery_verbose, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't', "Path for temporary files. Multiple paths can be specified, "
   "separated by "
#if defined( __WIN__) || defined(__NETWARE__)
   "semicolon (;)"
#else
   "colon (:)"
#endif
   , (char**) &opt_tmpdir, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"undo", 'u', "Apply UNDO records to tables. (disable with --disable-undo)",
   (uchar **) &opt_apply_undo, (uchar **) &opt_apply_undo, 0,
   GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"version", 'V', "Print version and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

#include <help_start.h>

static void print_version(void)
{
  VOID(printf("%s Ver 1.3 for %s on %s\n",
              my_progname_short, SYSTEM_TYPE, MACHINE_TYPE));
  NETWARE_SET_SCREEN_MODE(1);
}


static void usage(void)
{
  print_version();
  puts("Copyright (C) 2007 MySQL AB, 2009-2011 Monty Program Ab");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,");
  puts("and you are welcome to modify and redistribute it under the GPL license\n");

  puts("Display and apply log records from a Aria transaction log");
  puts("found in the current directory (for now)");
#ifndef IDENTICAL_PAGES_AFTER_RECOVERY
  puts("\nNote: Aria is compiled without -DIDENTICAL_PAGES_AFTER_RECOVERY\n"
       "which means that the table files are not byte-to-byte identical to\n"
       "files created during normal execution. This should be ok, except for\n"
       "test scripts that tries to compare files before and after recovery.");
#endif
  VOID(printf("\nUsage: %s OPTIONS\n", my_progname_short));
  puts("You need to use one of -d or -a");
  my_print_help(my_long_options);
  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}

#include <help_end.h>

static uchar* my_hash_get_string(const uchar *record, size_t *length,
                                my_bool first __attribute__ ((unused)))
{
  *length= (size_t) (strcend((const char*) record,',')- (const char*) record);
  return (uchar*) record;
}


static my_bool
get_one_option(int optid __attribute__((unused)),
               const struct my_option *opt __attribute__((unused)),
               char *argument)
{
  switch (optid) {
  case '?':
    usage();
    exit(0);
  case 'V':
    print_version();
    exit(0);
  case 'T':
  {
    char *pos;
    if (!my_hash_inited(&tables_to_redo))
    {
      my_hash_init2(&tables_to_redo, 16, &my_charset_bin,
                    16, 0, 0, my_hash_get_string, 0, HASH_UNIQUE);
    }
    do
    {
      pos= strcend(argument, ',');
      if (pos != argument)                      /* Skip empty strings */
        my_hash_insert(&tables_to_redo, (uchar*) argument);
      argument= pos+1;
    } while (*(pos++));
    break;
  }
#ifndef DBUG_OFF
  case '#':
    DBUG_SET_INITIAL(argument ? argument : default_dbug_option);
    break;
#endif
  }
  return 0;
}

static void get_options(int *argc,char ***argv)
{
  int ho_error;
  my_bool need_help= 0;

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (!opt_apply)
    opt_apply_undo= FALSE;

  if (*argc > 0)
  {
    need_help= 1;
    fprintf(stderr, "Too many arguments given\n");
  }
  if ((opt_display_only + opt_apply) != 1)
  {
    need_help= 1;
    fprintf(stderr,
            "You must use one and only one of the options 'display-only' or "
            "'apply'\n");
  }

  if (need_help)
  {
    fflush(stderr);
    need_help =1;
    usage();
    exit(1);
  }
  if (init_tmpdir(&maria_chk_tmpdir, opt_tmpdir))
    exit(1);
  maria_tmpdir= &maria_chk_tmpdir;
}
