/* Copyright (C) 2006-2003 MySQL AB

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

/* Describe, check and repair of MARIA tables */

#include "ma_fulltext.h"
#include <myisamchk.h>
#include <my_bit.h>
#include <m_ctype.h>
#include <stdarg.h>
#include <my_getopt.h>
#ifdef HAVE_SYS_VADVICE_H
#include <sys/vadvise.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
SET_STACK_SIZE(9000)			/* Minimum stack size for program */

#ifndef USE_RAID
#define my_raid_create(A,B,C,D,E,F,G) my_create(A,B,C,G)
#define my_raid_delete(A,B,C) my_delete(A,B)
#endif

static uint decode_bits;
static char **default_argv;
static const char *load_default_groups[]= { "aria_chk", 0 };
static const char *set_collation_name, *opt_tmpdir, *opt_log_dir;
static CHARSET_INFO *set_collation;
static int stopwords_inited= 0;
static MY_TMPDIR maria_chk_tmpdir;
static my_bool opt_transaction_logging, opt_debug, opt_require_control_file;
static my_bool opt_warning_for_wrong_transid;

static const char *type_names[]=
{
  "impossible","char","binary", "short", "long", "float",
  "double","number","unsigned short",
  "unsigned long","longlong","ulonglong","int24",
  "uint24","int8","varchar", "varbin", "varchar2", "varbin2", "bit",
  "?","?"
};

static const char *prefix_packed_txt="packed ",
		  *bin_packed_txt="prefix ",
		  *diff_txt="stripped ",
		  *null_txt="NULL",
		  *blob_txt="BLOB ";

static const char *field_pack[]=
{
  "","no endspace", "no prespace",
 "no zeros", "blob", "constant", "table-lockup",
 "always zero","varchar","unique-hash","?","?"
};

static const char *record_formats[]=
{
  "Fixed length", "Packed", "Compressed", "Block", "?"
};

static const char *bitmap_description[]=
{
  "Empty page", "Part filled head page","Part filled head page",
  "Part filled head page", "Full head page",
  "Part filled tail page","Part filled tail page",
  "Full tail or blob page"
};

static const char *maria_stats_method_str="nulls_unequal";
static char default_open_errmsg[]=  "%d when opening Aria table '%s'";
static char default_close_errmsg[]= "%d when closing Aria table '%s'";

static void get_options(int *argc,char * * *argv);
static void print_version(void);
static void usage(void);
static int maria_chk(HA_CHECK *param, char *filename);
static void descript(HA_CHECK *param, register MARIA_HA *info, char *name);
static int maria_sort_records(HA_CHECK *param, register MARIA_HA *info,
                              char *name, uint sort_key,
                              my_bool write_info, my_bool update_index);
static int sort_record_index(MARIA_SORT_PARAM *sort_param, MARIA_PAGE *page,
			     uint sortkey, File new_file,
                             my_bool update_index);
static my_bool write_log_record(HA_CHECK *param);

HA_CHECK check_param;

	/* Main program */

int main(int argc, char **argv)
{
  int error;
  MY_INIT(argv[0]);

  opt_log_dir= maria_data_root= (char *)".";
  maria_chk_init(&check_param);
  check_param.opt_lock_memory= 1;		/* Lock memory if possible */
  check_param.using_global_keycache = 0;
  get_options(&argc,(char***) &argv);
  maria_quick_table_bits=decode_bits;
  error=0;
  maria_init();

  maria_block_size= 0;                 /* Use block size from control file */
  if (ma_control_file_open(FALSE, opt_require_control_file ||
                           !(check_param.testflag & T_SILENT)) &&
      (opt_require_control_file ||
       (opt_transaction_logging && (check_param.testflag & T_REP_ANY))))
  {
    error= 1;
    goto end;
  }

  /*
    If we are doing a repair, user may want to store this repair into the log
    so that the log has a complete history and can be used to replay.
  */
  if (opt_transaction_logging && (check_param.testflag & T_REP_ANY))
  {
    if (init_pagecache(maria_log_pagecache,
                       TRANSLOG_PAGECACHE_SIZE, 0, 0,
                       TRANSLOG_PAGE_SIZE, MY_WME) == 0 ||
        translog_init(opt_log_dir, TRANSLOG_FILE_SIZE,
                      0, 0, maria_log_pagecache,
                      TRANSLOG_DEFAULT_FLAGS, 0))
    {
      _ma_check_print_error(&check_param,
                            "Can't initialize transaction logging. Run "
                            "recovery with switch --skip-transaction-log");
      error= 1;
      goto end;
    }
  }

  while (--argc >= 0)
  {
    int new_error=maria_chk(&check_param, *(argv++));
    if ((check_param.testflag & T_REP_ANY) != T_REP)
      check_param.testflag&= ~T_REP;
    VOID(fflush(stdout));
    VOID(fflush(stderr));
    if ((check_param.error_printed | check_param.warning_printed) &&
	(check_param.testflag & T_FORCE_CREATE) &&
	(!(check_param.testflag & (T_REP | T_REP_BY_SORT | T_SORT_RECORDS |
				   T_SORT_INDEX))))
    {
      ulonglong old_testflag=check_param.testflag;
      if (!(check_param.testflag & T_REP))
	check_param.testflag|= T_REP_BY_SORT;
      check_param.testflag&= ~T_EXTEND;			/* Not needed  */
      error|=maria_chk(&check_param, argv[-1]);
      check_param.testflag= old_testflag;
      VOID(fflush(stdout));
      VOID(fflush(stderr));
    }
    else
      error|=new_error;
    if (argc && (!(check_param.testflag & T_SILENT) ||
                 check_param.testflag & T_INFO))
    {
      puts("\n---------\n");
      VOID(fflush(stdout));
    }
  }
end:
  if (check_param.total_files > 1)
  {					/* Only if descript */
    char buff[22],buff2[22];
    if (!(check_param.testflag & T_SILENT) || check_param.testflag & T_INFO)
      puts("\n---------");
    printf("\nTotal of all %d Aria-files:\nData records: %9s   Deleted blocks: %9s\n",check_param.total_files,llstr(check_param.total_records,buff),
	   llstr(check_param.total_deleted,buff2));
  }
  free_defaults(default_argv);
  free_tmpdir(&maria_chk_tmpdir);
  maria_end();
  my_end(check_param.testflag & T_INFO ?
         MY_CHECK_ERROR | MY_GIVE_INFO : MY_CHECK_ERROR);
  exit(error);
#ifndef _lint
  return 0;				/* No compiler warning */
#endif
} /* main */

enum options_mc {
  OPT_CHARSETS_DIR=256, OPT_SET_COLLATION,OPT_START_CHECK_POS,
  OPT_CORRECT_CHECKSUM, OPT_PAGE_BUFFER_SIZE,
  OPT_KEY_CACHE_BLOCK_SIZE, OPT_MARIA_BLOCK_SIZE,
  OPT_READ_BUFFER_SIZE, OPT_WRITE_BUFFER_SIZE, OPT_SORT_BUFFER_SIZE,
  OPT_SORT_KEY_BLOCKS, OPT_DECODE_BITS, OPT_FT_MIN_WORD_LEN,
  OPT_FT_MAX_WORD_LEN, OPT_FT_STOPWORD_FILE,
  OPT_MAX_RECORD_LENGTH, OPT_AUTO_CLOSE, OPT_STATS_METHOD, OPT_TRANSACTION_LOG,
  OPT_SKIP_SAFEMALLOC, OPT_ZEROFILL_KEEP_LSN, OPT_REQUIRE_CONTROL_FILE,
  OPT_LOG_DIR, OPT_DATADIR, OPT_WARNING_FOR_WRONG_TRANSID
};

static struct my_option my_long_options[] =
{
  {"analyze", 'a',
   "Analyze distribution of keys. Will make some joins in MySQL faster. You can check the calculated distribution.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __NETWARE__
  {"autoclose", OPT_AUTO_CLOSE, "Auto close the screen on exit for Netware.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"block-search", 'b',
   "No help available.",
   0, 0, 0, GET_ULONG, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"backup", 'B',
   "Make a backup of the .MAD file as 'filename-time.BAK'.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.",
   (char**) &charsets_dir, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"check", 'c',
   "Check table for errors.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"check-only-changed", 'C',
   "Check only tables that have changed since last check. It also applies to other requested actions (e.g. --analyze will be ignored if the table is already analyzed).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"correct-checksum", OPT_CORRECT_CHECKSUM,
   "Correct checksum information for table.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#',
   "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"description", 'd',
   "Prints some information about table.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"data-file-length", 'D',
   "Max length of data file (when recreating data-file when it's full).",
   &check_param.max_data_file_length,
   &check_param.max_data_file_length,
   0, GET_LL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"extend-check", 'e',
   "If used when checking a table, ensure that the table is 100 percent consistent, which will take a long time. If used when repairing a table, try to recover every possible row from the data file. Normally this will also find a lot of garbage rows; Don't use this option with repair if you are not totally desperate.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"fast", 'F',
   "Check only tables that haven't been closed properly. It also applies to other requested actions (e.g. --analyze will be ignored if the table is already analyzed).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f',
   "Restart with -r if there are any errors in the table. States will be updated as with --update-state.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"HELP", 'H',
   "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?',
   "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"information", 'i',
   "Print statistics information about table that is checked.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"keys-used", 'k',
   "Tell Aria to update only some specific keys. # is a bit mask of which keys to use. This can be used to get faster inserts.",
   &check_param.keys_in_use,
   &check_param.keys_in_use,
   0, GET_ULL, REQUIRED_ARG, -1, 0, 0, 0, 0, 0},
  {"datadir", OPT_DATADIR,
   "Path for control file (and logs if --logdir not used).",
   &maria_data_root, 0, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"logdir", OPT_LOG_DIR,
   "Path for log files.",
   (char**) &opt_log_dir, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"max-record-length", OPT_MAX_RECORD_LENGTH,
   "Skip rows bigger than this if aria_chk can't allocate memory to hold it",
   &check_param.max_record_length,
   &check_param.max_record_length,
   0, GET_ULL, REQUIRED_ARG, LONGLONG_MAX, 0, LONGLONG_MAX, 0, 0, 0},
  {"medium-check", 'm',
   "Faster than extend-check, but only finds 99.99% of all errors. Should be good enough for most cases.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q', "Faster repair by not modifying the data file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"read-only", 'T',
   "Don't mark table as checked.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"recover", 'r',
   "Can fix almost anything except unique keys that aren't unique.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"parallel-recover", 'p',
   "Same as '-r' but creates all the keys in parallel.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"safe-recover", 'o',
   "Uses old recovery method; Slower than '-r' but can handle a couple of cases where '-r' reports that it can't fix the data file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sort-recover", 'n',
   "Force recovering with sorting even if the temporary file was very big.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "require-control-file", OPT_REQUIRE_CONTROL_FILE,
    "Abort if cannot find control file",
    (uchar**)&opt_require_control_file, 0, 0, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0},
#ifdef DEBUG
  {"start-check-pos", OPT_START_CHECK_POS,
   "No help available.",
   0, 0, 0, GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"set-auto-increment", 'A',
   "Force auto_increment to start at this or higher value. If no value is given, then sets the next auto_increment value to the highest used value for the auto key + 1.",
   &check_param.auto_increment_value,
   &check_param.auto_increment_value,
   0, GET_ULL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"set-collation", OPT_SET_COLLATION,
   "Change the collation used by the index",
   (char**) &set_collation_name, 0, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"silent", 's',
   "Only print errors. One can use two -s to make aria_chk very silent.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
#ifdef SAFEMALLOC
  {"skip-safemalloc", OPT_SKIP_SAFEMALLOC,
   "Don't use the memory allocation checking.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
#endif
#endif
  {"sort-index", 'S',
   "Sort index blocks. This speeds up 'read-next' in applications.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sort-records", 'R',
   "Sort records according to an index. This makes your data much more localized and may speed up things. (It may be VERY slow to do a sort the first time!)",
   &check_param.opt_sort_key,
   &check_param.opt_sort_key,
   0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't', "Path for temporary files.", (char**) &opt_tmpdir,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"transaction-log", OPT_TRANSACTION_LOG,
   "Log repair command to transaction log",
   &opt_transaction_logging, &opt_transaction_logging,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"update-state", 'U',
   "Mark tables as crashed if any errors were found and clean if check didn't "
   "find any errors. This allows one to get rid of warnings like 'table not "
   "properly closed'",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"unpack", 'u',
   "Unpack file packed with aria_pack.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v',
   "Print more information. This can be used with --description and --check. Use many -v for more verbosity!",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Print version and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"wait", 'w', "Wait if table is locked.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"warning-for-wrong-transaction-id", OPT_WARNING_FOR_WRONG_TRANSID,
   "Give a warning if we find a transaction id in the table that is bigger"
   "than what exists in the control file. Use --skip-... to disable warning",
   &opt_warning_for_wrong_transid, &opt_warning_for_wrong_transid,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  { "page_buffer_size", OPT_PAGE_BUFFER_SIZE,
    "Size of page buffer. Used by --safe-repair",
    &check_param.use_buffers, &check_param.use_buffers, 0,
    GET_ULONG, REQUIRED_ARG, (long) USE_BUFFER_INIT, 1024L*1024L,
    (long) ~0L, (long) MALLOC_OVERHEAD, (long) IO_SIZE, 0},
  { "read_buffer_size", OPT_READ_BUFFER_SIZE,
    "Read buffer size for sequential reads during scanning",
    &check_param.read_buffer_length,
    &check_param.read_buffer_length, 0, GET_ULONG, REQUIRED_ARG,
    (long) READ_BUFFER_INIT, (long) MALLOC_OVERHEAD,
    (long) ~0L, (long) MALLOC_OVERHEAD, (long) 1L, 0},
  { "write_buffer_size", OPT_WRITE_BUFFER_SIZE,
    "Write buffer size for sequential writes during repair of fixed size or dynamic size rows",
    &check_param.write_buffer_length,
    &check_param.write_buffer_length, 0, GET_ULONG, REQUIRED_ARG,
    (long) READ_BUFFER_INIT, (long) MALLOC_OVERHEAD,
    (long) ~0L, (long) MALLOC_OVERHEAD, (long) 1L, 0},
  { "sort_buffer_size", OPT_SORT_BUFFER_SIZE,
    "Size of sort buffer. Used by --recover",
    &check_param.sort_buffer_length,
    &check_param.sort_buffer_length, 0, GET_ULONG, REQUIRED_ARG,
    (long) SORT_BUFFER_INIT, (long) (MIN_SORT_BUFFER + MALLOC_OVERHEAD),
    (long) ~0L, (long) MALLOC_OVERHEAD, (long) 1L, 0},
  { "sort_key_blocks", OPT_SORT_KEY_BLOCKS,
    "Internal buffer for sorting keys; Don't touch :)",
    &check_param.sort_key_blocks,
    &check_param.sort_key_blocks, 0, GET_ULONG, REQUIRED_ARG,
    BUFFERS_WHEN_SORTING, 4L, 100L, 0L, 1L, 0},
  { "decode_bits", OPT_DECODE_BITS, "", &decode_bits,
    &decode_bits, 0, GET_UINT, REQUIRED_ARG, 9L, 4L, 17L, 0L, 1L, 0},
  { "ft_min_word_len", OPT_FT_MIN_WORD_LEN, "", &ft_min_word_len,
    &ft_min_word_len, 0, GET_ULONG, REQUIRED_ARG, 4, 1, HA_FT_MAXCHARLEN,
    0, 1, 0},
  { "ft_max_word_len", OPT_FT_MAX_WORD_LEN, "", &ft_max_word_len,
    &ft_max_word_len, 0, GET_ULONG, REQUIRED_ARG, HA_FT_MAXCHARLEN, 10,
    HA_FT_MAXCHARLEN, 0, 1, 0},
  { "aria_ft_stopword_file", OPT_FT_STOPWORD_FILE,
    "Use stopwords from this file instead of built-in list.",
    (char**) &ft_stopword_file, (char**) &ft_stopword_file, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "stats_method", OPT_STATS_METHOD,
    "Specifies how index statistics collection code should treat NULLs. "
    "Possible values of name are \"nulls_unequal\" (default behavior for 4.1/5.0), "
    "\"nulls_equal\" (emulate 4.0 behavior), and \"nulls_ignored\".",
    (char**) &maria_stats_method_str, (char**) &maria_stats_method_str, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "zerofill", 'z',
    "Fill empty space in data and index files with zeroes,",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "zerofill-keep-lsn", OPT_ZEROFILL_KEEP_LSN,
    "Like --zerofill but does not zero out LSN of data/index pages;"
    " used only for testing and debugging",
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


#include <help_start.h>

static void print_version(void)
{
  printf("%s  Ver 1.0 for %s at %s\n", my_progname, SYSTEM_TYPE,
	 MACHINE_TYPE);
  NETWARE_SET_SCREEN_MODE(1);
}


static void usage(void)
{
  print_version();
  puts("By Monty, for your professional use");
  puts("This software comes with NO WARRANTY: see the PUBLIC for details.\n");
  puts("Description, check and repair of Aria tables.");
  puts("Used without options all tables on the command will be checked for errors");
  printf("Usage: %s [OPTIONS] tables[.MAI]\n", my_progname_short);
  printf("\nGlobal options:\n");
#ifndef DBUG_OFF
  printf("\
  -#, --debug=...     Output debug log. Often this is 'd:t:o,filename'.\n");
#endif
  printf("\
  -H, --HELP          Display this help and exit.\n\
  -?, --help          Display this help and exit.\n\
  --datadir=path      Path for control file (and logs if --logdir not used)\n\
  --logdir=path       Path for log files\n\
  --require-control-file  Abort if we can't find/read the maria_log_control\n\
                          file\n\
  -s, --silent	      Only print errors.  One can use two -s to make\n\
		      maria_chk very silent.\n\
  -t, --tmpdir=path   Path for temporary files. Multiple paths can be\n\
                      specified, separated by ");
#if defined( __WIN__) || defined(__NETWARE__)
   printf("semicolon (;)");
#else
   printf("colon (:)");
#endif
   printf(", they will be used\n\
                      in a round-robin fashion.\n\
  -v, --verbose       Print more information. This can be used with\n\
                      --description and --check. Use many -v for more verbosity.\n\
  -V, --version       Print version and exit.\n\
  -w, --wait          Wait if table is locked.\n\n");
#ifdef DEBUG
  puts("  --start-check-pos=# Start reading file at given offset.\n");
#endif

  puts("Check options (check is the default action for aria_chk):\n\
  -c, --check	      Check table for errors.\n\
  -e, --extend-check  Check the table VERY throughly.  Only use this in\n\
                      extreme cases as aria_chk should normally be able to\n\
                      find out if the table is ok even without this switch.\n\
  -F, --fast	      Check only tables that haven't been closed properly.\n\
  -C, --check-only-changed\n\
		      Check only tables that have changed since last check.\n\
  -f, --force         Restart with '-r' if there are any errors in the table.\n\
		      States will be updated as with '--update-state'.\n\
  -i, --information   Print statistics information about table that is checked.\n\
  -m, --medium-check  Faster than extend-check, but only finds 99.99% of\n\
		      all errors.  Should be good enough for most cases.\n\
  -U, --update-state  Mark tables as crashed if you find any errors.\n\
  -T, --read-only     Don't mark table as checked.\n");

  puts("\
Recover (repair)/ options (When using '--recover' or '--safe-recover'):\n\
  -B, --backup	      Make a backup of the .MAD file as 'filename-time.BAK'.\n\
  --correct-checksum  Correct checksum information for table.\n\
  -D, --data-file-length=#  Max length of data file (when recreating data\n\
                      file when it's full).\n\
  -e, --extend-check  Try to recover every possible row from the data file\n\
		      Normally this will also find a lot of garbage rows;\n\
		      Don't use this option if you are not totally desperate.\n\
  -f, --force         Overwrite old temporary files.\n\
  -k, --keys-used=#   Tell Aria to update only some specific keys. # is a\n\
	              bit mask of which keys to use. This can be used to\n\
		      get faster inserts.\n\
  --max-record-length=#\n\
                      Skip rows bigger than this if aria_chk can't allocate\n\
		      memory to hold it.\n\
  -r, --recover       Can fix almost anything except unique keys that aren't\n\
                      unique.\n\
  -n, --sort-recover  Forces recovering with sorting even if the temporary\n\
		      file would be very big.\n\
  -p, --parallel-recover\n\
                      Uses the same technique as '-r' and '-n', but creates\n\
                      all the keys in parallel, in different threads.");
  puts("\
  -o, --safe-recover  Uses old recovery method; Slower than '-r' but can\n \
		      handle a couple of cases where '-r' reports that it\n\
		      can't fix the data file.\n\
  --transaction-log   Log repair command to transaction log. This is needed\n\
                      if one wants to use the aria_read_log to repeat the \n\
                      repair\n\
  --character-sets-dir=...\n\
                      Directory where character sets are.\n\
  --set-collation=name\n\
 		      Change the collation used by the index.\n\
  -q, --quick         Faster repair by not modifying the data file.\n\
                      One can give a second '-q' to force aria_chk to\n\
		      modify the original datafile in case of duplicate keys.\n\
		      NOTE: Tables where the data file is currupted can't be\n\
		      fixed with this option.\n\
  -u, --unpack        Unpack file packed with ariapack.\n\
");

  puts("Other actions:\n\
  -a, --analyze	      Analyze distribution of keys. Will make some joins in\n\
		      MariaDB faster.  You can check the calculated distribution\n\
		      by using '--description --verbose table_name'.\n\
  --stats_method=name Specifies how index statistics collection code should\n\
                      treat NULLs. Possible values of name are \"nulls_unequal\"\n\
                      (default for 4.1/5.0), \"nulls_equal\" (emulate 4.0), and \n\
                      \"nulls_ignored\".\n\
  -d, --description   Prints some information about table.\n\
  -A, --set-auto-increment[=value]\n\
		      Force auto_increment to start at this or higher value\n\
		      If no value is given, then sets the next auto_increment\n\
		      value to the highest used value for the auto key + 1.\n\
  -S, --sort-index    Sort index blocks.  This speeds up 'read-next' in\n\
		      applications.\n\
  -R, --sort-records=#\n\
		      Sort records according to an index.  This makes your\n\
		      data much more localized and may speed up things\n\
		      (It may be VERY slow to do a sort the first time!).\n\
  -b,  --block-search=#\n\
                      Find a record, a block at given offset belongs to.\n\
  -z,  --zerofill     Fill empty space in data and index files with zeroes\n\
  --zerofill-keep-lsn Like --zerofill but does not zero out LSN of\n\
                      data/index pages.");

  puts("Variables:\n\
--page_buffer_size=#   Size of page buffer. Used by --safe-repair\n\
--read_buffer_size=#   Read buffer size for sequential reads during scanning\n\
--sort_buffer_size=#   Size of sort buffer. Used by --recover\n\
--sort_key_blocks=#    Internal buffer for sorting keys; Don't touch :)\n\
--write_buffer_size=#  Write buffer size for sequential writes during repair");

  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}

#include <help_end.h>

const char *maria_stats_method_names[] = {"nulls_unequal", "nulls_equal",
                                           "nulls_ignored", NullS};
TYPELIB maria_stats_method_typelib= {
  array_elements(maria_stats_method_names) - 1, "",
  maria_stats_method_names, NULL};

	 /* Read options */

static my_bool
get_one_option(int optid,
	       const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
#ifdef __NETWARE__
  case OPT_AUTO_CLOSE:
    setscreenmode(SCR_AUTOCLOSE_ON_EXIT);
    break;
#endif
  case 'a':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_STATISTICS;
    else
      check_param.testflag|= T_STATISTICS;
    break;
  case 'A':
    if (argument)
      check_param.auto_increment_value= strtoull(argument, NULL, 0);
    else
      check_param.auto_increment_value= 0;	/* Set to max used value */
    check_param.testflag|= T_AUTO_INC;
    break;
  case 'b':
    check_param.search_after_block= strtoul(argument, NULL, 10);
    break;
  case 'B':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_BACKUP_DATA;
    else
      check_param.testflag|= T_BACKUP_DATA;
    break;
  case 'c':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_CHECK;
    else
      check_param.testflag|= T_CHECK;
    break;
  case 'C':
    if (argument == disabled_my_option)
      check_param.testflag&= ~(T_CHECK | T_CHECK_ONLY_CHANGED);
    else
      check_param.testflag|= T_CHECK | T_CHECK_ONLY_CHANGED;
    break;
  case 'D':
    check_param.max_data_file_length=strtoll(argument, NULL, 10);
    break;
  case 's':				/* silent */
    if (argument == disabled_my_option)
      check_param.testflag&= ~(T_SILENT | T_VERY_SILENT);
    else
    {
      if (check_param.testflag & T_SILENT)
	check_param.testflag|= T_VERY_SILENT;
      check_param.testflag|= T_SILENT;
      check_param.testflag&= ~T_WRITE_LOOP;
    }
    break;
  case 'w':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_WAIT_FOREVER;
    else
      check_param.testflag|= T_WAIT_FOREVER;
    break;
  case 'd':				/* description if isam-file */
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_DESCRIPT;
    else
      check_param.testflag|= T_DESCRIPT;
    break;
  case 'e':				/* extend check */
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_EXTEND;
    else
      check_param.testflag|= T_EXTEND;
    break;
  case 'i':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_INFO;
    else
      check_param.testflag|= T_INFO;
    break;
  case 'f':
    if (argument == disabled_my_option)
    {
      check_param.tmpfile_createflag= O_RDWR | O_TRUNC | O_EXCL;
      check_param.testflag&= ~(T_FORCE_CREATE | T_UPDATE_STATE);
    }
    else
    {
      check_param.tmpfile_createflag= O_RDWR | O_TRUNC;
      check_param.testflag|= T_FORCE_CREATE | T_UPDATE_STATE;
    }
    break;
  case 'F':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_FAST;
    else
      check_param.testflag|= T_FAST;
    break;
  case 'k':
    check_param.keys_in_use= (ulonglong) strtoll(argument, NULL, 10);
    break;
  case 'm':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_MEDIUM;
    else
      check_param.testflag|= T_MEDIUM;		/* Medium check */
    break;
  case 'r':				/* Repair table */
    check_param.testflag&= ~T_REP_ANY;
    if (argument != disabled_my_option)
      check_param.testflag|= T_REP_BY_SORT;
    break;
  case 'p':
    check_param.testflag&= ~T_REP_ANY;
    if (argument != disabled_my_option)
      check_param.testflag|= T_REP_PARALLEL;
    break;
  case 'o':
    check_param.testflag&= ~T_REP_ANY;
    check_param.force_sort= 0;
    if (argument != disabled_my_option)
    {
      check_param.testflag|= T_REP;
      my_disable_async_io= 1;		/* More safety */
    }
    break;
  case 'n':
    check_param.testflag&= ~T_REP_ANY;
    if (argument == disabled_my_option)
      check_param.force_sort= 0;
    else
    {
      check_param.testflag|= T_REP_BY_SORT;
      check_param.force_sort= 1;
    }
    break;
  case 'q':
    if (argument == disabled_my_option)
      check_param.testflag&= ~(T_QUICK | T_FORCE_UNIQUENESS);
    else
      check_param.testflag|=
        (check_param.testflag & T_QUICK) ? T_FORCE_UNIQUENESS : T_QUICK;
    break;
  case 'u':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_UNPACK;
    else
    {
      check_param.testflag|= T_UNPACK;
      if (!(check_param.testflag & T_REP_ANY))
        check_param.testflag|= T_REP_BY_SORT;
    }
    break;
  case 'v':				/* Verbose */
    if (argument == disabled_my_option)
    {
      check_param.testflag&= ~T_VERBOSE;
      check_param.verbose=0;
    }
    else
    {
      check_param.testflag|= T_VERBOSE;
      check_param.verbose++;
    }
    break;
  case 'R':				/* Sort records */
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_SORT_RECORDS;
    else
    {
      check_param.testflag|= T_SORT_RECORDS;
      check_param.opt_sort_key= (uint) atoi(argument) - 1;
      if (check_param.opt_sort_key >= MARIA_MAX_KEY)
      {
	fprintf(stderr,
		"The value of the sort key is bigger than max key: %d.\n",
		MARIA_MAX_KEY);
	exit(1);
      }
    }
    break;
  case 'S':			      /* Sort index */
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_SORT_INDEX;
    else
      check_param.testflag|= T_SORT_INDEX;
    break;
  case 'T':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_READONLY;
    else
      check_param.testflag|= T_READONLY;
    break;
  case 'U':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_UPDATE_STATE;
    else
      check_param.testflag|= T_UPDATE_STATE;
    break;
  case '#':
    DBUG_SET_INITIAL(argument ? argument : "d:t:o,/tmp/aria_chk.trace");
    opt_debug= 1;
    break;
  case OPT_SKIP_SAFEMALLOC:
#ifdef SAFEMALLOC
    sf_malloc_quick=1;
#endif
    break;
  case 'V':
    print_version();
    exit(0);
  case OPT_CORRECT_CHECKSUM:
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_CALC_CHECKSUM;
    else
      check_param.testflag|= T_CALC_CHECKSUM;
    break;
  case OPT_STATS_METHOD:
  {
    int method;
    enum_handler_stats_method method_conv;
    LINT_INIT(method_conv);
    maria_stats_method_str= argument;
    if ((method=find_type(argument, &maria_stats_method_typelib, 2)) <= 0)
    {
      fprintf(stderr, "Invalid value of stats_method: %s.\n", argument);
      exit(1);
    }
    switch (method-1) {
    case 0:
      method_conv= MI_STATS_METHOD_NULLS_EQUAL;
      break;
    case 1:
      method_conv= MI_STATS_METHOD_NULLS_NOT_EQUAL;
      break;
    case 2:
      method_conv= MI_STATS_METHOD_IGNORE_NULLS;
      break;
    default: assert(0);                         /* Impossible */
    }
    check_param.stats_method= method_conv;
    break;
  }
#ifdef DEBUG					/* Only useful if debugging */
  case OPT_START_CHECK_POS:
    check_param.start_check_pos= strtoull(argument, NULL, 0);
    break;
#endif
  case 'z':
    if (argument == disabled_my_option)
      check_param.testflag&= ~T_ZEROFILL;
    else
      check_param.testflag|= T_ZEROFILL;
    break;
  case OPT_ZEROFILL_KEEP_LSN:
    if (argument == disabled_my_option)
      check_param.testflag&= ~(T_ZEROFILL_KEEP_LSN | T_ZEROFILL);
    else
      check_param.testflag|= (T_ZEROFILL_KEEP_LSN | T_ZEROFILL);
    break;
  case 'H':
    my_print_help(my_long_options);
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}


static void get_options(register int *argc,register char ***argv)
{
  int ho_error;

  load_defaults("my", load_default_groups, argc, argv);
  default_argv= *argv;
  if (isatty(fileno(stdout)))
    check_param.testflag|=T_WRITE_LOOP;

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  /* If using repair, then update checksum if one uses --update-state */
  if ((check_param.testflag & T_UPDATE_STATE) &&
      (check_param.testflag & T_REP_ANY))
    check_param.testflag|= T_CALC_CHECKSUM;

  if (*argc == 0)
  {
    usage();
    exit(-1);
  }

  if ((check_param.testflag & T_UNPACK) &&
      (check_param.testflag & (T_QUICK | T_SORT_RECORDS)))
  {
    VOID(fprintf(stderr,
		 "%s: --unpack can't be used with --quick or --sort-records\n",
		 my_progname_short));
    exit(1);
  }
  if ((check_param.testflag & T_READONLY) &&
      (check_param.testflag &
       (T_REP_ANY | T_STATISTICS | T_AUTO_INC |
	T_SORT_RECORDS | T_SORT_INDEX | T_FORCE_CREATE)))
  {
    VOID(fprintf(stderr,
		 "%s: Can't use --readonly when repairing or sorting\n",
		 my_progname_short));
    exit(1);
  }

  if (!opt_debug)
  {
    DEBUGGER_OFF;                               /* Speed up things a bit */
  }
  if (init_tmpdir(&maria_chk_tmpdir, opt_tmpdir))
    exit(1);

  check_param.tmpdir=&maria_chk_tmpdir;

  if (set_collation_name)
    if (!(set_collation= get_charset_by_name(set_collation_name,
                                             MYF(MY_WME))))
      exit(1);

  return;
} /* get options */


	/* Check table */

static int maria_chk(HA_CHECK *param, char *filename)
{
  int error,lock_type,recreate;
  my_bool rep_quick= test(param->testflag & (T_QUICK | T_FORCE_UNIQUENESS));
  MARIA_HA *info;
  File datafile;
  char llbuff[22],llbuff2[22];
  my_bool state_updated=0;
  MARIA_SHARE *share;
  DBUG_ENTER("maria_chk");

  param->out_flag=error=param->warning_printed=param->error_printed=
    recreate=0;
  datafile=0;
  param->isam_file_name=filename;		/* For error messages */
  if (!(info=maria_open(filename,
                        (param->testflag & (T_DESCRIPT | T_READONLY)) ?
                        O_RDONLY : O_RDWR,
                        HA_OPEN_FOR_REPAIR |
                        ((param->testflag & T_WAIT_FOREVER) ?
                         HA_OPEN_WAIT_IF_LOCKED :
                         (param->testflag & T_DESCRIPT) ?
                         HA_OPEN_IGNORE_IF_LOCKED : HA_OPEN_ABORT_IF_LOCKED))))
  {
    /* Avoid twice printing of isam file name */
    param->error_printed=1;
    switch (my_errno) {
    case HA_ERR_CRASHED:
      _ma_check_print_error(param,"'%s' doesn't have a correct index definition. You need to recreate it before you can do a repair",filename);
      break;
    case HA_ERR_NOT_A_TABLE:
      _ma_check_print_error(param,"'%s' is not a Aria table",filename);
      break;
    case HA_ERR_CRASHED_ON_USAGE:
      _ma_check_print_error(param,"'%s' is marked as crashed",filename);
      break;
    case HA_ERR_CRASHED_ON_REPAIR:
      _ma_check_print_error(param,"'%s' is marked as crashed after last repair",filename);
      break;
    case HA_ERR_OLD_FILE:
      _ma_check_print_error(param,"'%s' is a old type of Aria table", filename);
      break;
    case HA_ERR_NEW_FILE:
      _ma_check_print_error(param,"'%s' uses new features not supported by this version of the Aria library", filename);
      break;
    case HA_ERR_END_OF_FILE:
      _ma_check_print_error(param,"Couldn't read complete header from '%s'", filename);
      break;
    case EAGAIN:
      _ma_check_print_error(param,"'%s' is locked. Use -w to wait until unlocked",filename);
      break;
    case ENOENT:
      _ma_check_print_error(param,"File '%s' doesn't exist",filename);
      break;
    case EACCES:
      _ma_check_print_error(param,"You don't have permission to use '%s'",
                            filename);
      break;
    default:
      _ma_check_print_error(param,"%d when opening Aria table '%s'",
		  my_errno,filename);
      break;
    }
    DBUG_RETURN(1);
  }
  share= info->s;
  share->tot_locks-= share->r_locks;
  share->r_locks=0;
  maria_block_size= share->base.block_size;

  if (share->data_file_type == BLOCK_RECORD ||
      ((param->testflag & T_UNPACK) &&
       share->state.header.org_data_file_type == BLOCK_RECORD))
  {
    if (param->testflag & T_SORT_RECORDS)
    {
      _ma_check_print_error(param,
                            "Record format used by '%s' is is not yet supported with sort-records",
                            filename);
      param->error_printed= 0;
      error= 1;
      goto end2;
    }
    /* We can't do parallell repair with BLOCK_RECORD yet */
    if (param->testflag & T_REP_PARALLEL)
    {
      param->testflag&= ~T_REP_PARALLEL;
      param->testflag|= T_REP_BY_SORT;
    }
  }

  /*
    Skip the checking of the file if:
    We are using --fast and the table is closed properly
    We are using --check-only-changed-tables and the table hasn't changed
  */
  if (param->testflag & (T_FAST | T_CHECK_ONLY_CHANGED))
  {
    my_bool need_to_check= (maria_is_crashed(info) ||
                            share->state.open_count != 0);

    if ((param->testflag & (T_REP_ANY | T_SORT_RECORDS)) &&
	((share->state.changed & (STATE_CHANGED | STATE_CRASHED |
				  STATE_CRASHED_ON_REPAIR | STATE_IN_REPAIR) ||
	  !(param->testflag & T_CHECK_ONLY_CHANGED))))
      need_to_check=1;

    if (info->s->base.keys && info->state->records)
    {
      if ((param->testflag & T_STATISTICS) &&
          (share->state.changed & STATE_NOT_ANALYZED))
        need_to_check=1;
      if ((param->testflag & T_SORT_INDEX) &&
          (share->state.changed & STATE_NOT_SORTED_PAGES))
        need_to_check=1;
      if ((param->testflag & T_REP_BY_SORT) &&
          (share->state.changed & STATE_NOT_OPTIMIZED_KEYS))
        need_to_check=1;
    }
    if ((param->testflag & T_CHECK_ONLY_CHANGED) &&
	(share->state.changed & (STATE_CHANGED | STATE_CRASHED |
				 STATE_CRASHED_ON_REPAIR | STATE_IN_REPAIR)))
      need_to_check=1;
    if (!need_to_check)
    {
      if (!(param->testflag & T_SILENT) || param->testflag & T_INFO)
	printf("Aria file: %s is already checked\n",filename);
      if (maria_close(info))
      {
	_ma_check_print_error(param,"%d when closing Aria table '%s'",
			     my_errno,filename);
	DBUG_RETURN(1);
      }
      DBUG_RETURN(0);
    }
  }
  if ((param->testflag & (T_REP_ANY | T_STATISTICS |
			  T_SORT_RECORDS | T_SORT_INDEX)) &&
      (((param->testflag & T_UNPACK) &&
	share->data_file_type == COMPRESSED_RECORD) ||
       mi_uint2korr(share->state.header.state_info_length) !=
       MARIA_STATE_INFO_SIZE ||
       mi_uint2korr(share->state.header.base_info_length) !=
       MARIA_BASE_INFO_SIZE ||
       maria_is_any_intersect_keys_active(param->keys_in_use, share->base.keys,
                                       ~share->state.key_map) ||
       maria_test_if_almost_full(info) ||
       info->s->state.header.file_version[3] != maria_file_magic[3] ||
       (set_collation &&
        set_collation->number != share->state.header.language)))
  {
    if (set_collation)
      param->language= set_collation->number;
    if (maria_recreate_table(param, &info,filename))
    {
      VOID(fprintf(stderr,
		   "Aria table '%s' is not fixed because of errors\n",
	      filename));
      return(-1);
    }
    recreate=1;
    if (!(param->testflag & T_REP_ANY))
    {
      param->testflag|=T_REP_BY_SORT;		/* if only STATISTICS */
      if (!(param->testflag & T_SILENT))
	printf("- '%s' has old table-format. Recreating index\n",filename);
      rep_quick= 1;
    }
    share= info->s;
    share->tot_locks-= share->r_locks;
    share->r_locks=0;
  }

  if (param->testflag & T_DESCRIPT)
  {
    param->total_files++;
    param->total_records+=info->state->records;
    param->total_deleted+=info->state->del;
    descript(param, info, filename);
    maria_close(info);                          /* Should always succeed */
    return(0);
  }

  if (!stopwords_inited++)
    ft_init_stopwords();

  if (!(param->testflag & T_READONLY))
    lock_type = F_WRLCK;			/* table is changed */
  else
    lock_type= F_RDLCK;
  if (info->lock_type == F_RDLCK)
    info->lock_type=F_UNLCK;			/* Read only table */
  if (_ma_readinfo(info,lock_type,0))
  {
    _ma_check_print_error(param,"Can't lock indexfile of '%s', error: %d",
                          filename,my_errno);
    param->error_printed=0;
    error= 1;
    goto end2;
  }
  /*
    _ma_readinfo() has locked the table.
    We mark the table as locked (without doing file locks) to be able to
    use functions that only works on locked tables (like row caching).
  */
  maria_lock_database(info, F_EXTRA_LCK);
  datafile= info->dfile.file;
  if (init_pagecache(maria_pagecache, (size_t) param->use_buffers, 0, 0,
                     maria_block_size, MY_WME) == 0)
  {
    _ma_check_print_error(param, "Can't initialize page cache with %lu memory",
                          (ulong) param->use_buffers);
    error= 1;
    goto end2;
  }

  if (param->testflag & (T_REP_ANY | T_SORT_RECORDS | T_SORT_INDEX |
                         T_ZEROFILL))
  {
    /*
      Mark table as not transactional to avoid logging. Should not be needed,
      maria_repair and maria_zerofill do it already.
    */
    _ma_tmp_disable_logging_for_table(info, FALSE);

    if (param->testflag & T_REP_ANY)
    {
      ulonglong tmp=share->state.key_map;
      maria_copy_keys_active(share->state.key_map, share->base.keys,
                             param->keys_in_use);
      if (tmp != share->state.key_map)
        info->update|=HA_STATE_CHANGED;

      if (rep_quick &&
          maria_chk_del(param, info, param->testflag & ~T_VERBOSE))
      {
        if (param->testflag & T_FORCE_CREATE)
        {
          rep_quick=0;
          _ma_check_print_info(param,"Creating new data file\n");
        }
        else
        {
          error=1;
          _ma_check_print_error(param,
                                "Quick-recover aborted; Run recovery without switch 'q'");
        }
      }
    }
    if (!error)
    {
      /*
        Unless this was only --zerofill-keep-lsn, old REDOs are not
        applicable, tell the server's Recovery to ignore them; we don't
        know what the log's end LSN is now, so we just let the server know
        that it will have to find and store it.
        This is the only case where create_rename_lsn can be a horizon and not
        a LSN.
        If this was only --zerofill-keep-lsn, the table can be used in
        Recovery and especially in this scenario: do a dirty-copy-based backup
        (snapshot-like), --zerofill-keep-lsn on the copies to achieve better
        compression, compress the copies with an external tool, and after a
        restore, Recovery still works (because pages and state still have
        their correct LSNs).
      */
      if (share->base.born_transactional &&
          ((param->testflag & (T_REP_ANY | T_SORT_RECORDS | T_SORT_INDEX |
                               T_ZEROFILL | T_ZEROFILL_KEEP_LSN)) !=
           (T_ZEROFILL | T_ZEROFILL_KEEP_LSN)))
        share->state.create_rename_lsn= share->state.is_of_horizon=
          share->state.skip_redo_lsn= LSN_NEEDS_NEW_STATE_LSNS;
    }
    if (!error && (param->testflag & T_REP_ANY))
    {
      if ((param->testflag & (T_REP_BY_SORT | T_REP_PARALLEL)) &&
          (maria_is_any_key_active(share->state.key_map) ||
           (rep_quick && !param->keys_in_use && !recreate)) &&
          maria_test_if_sort_rep(info, info->state->records,
                                 info->s->state.key_map,
                                 param->force_sort))
      {
        if (param->testflag & T_REP_BY_SORT)
          error=maria_repair_by_sort(param,info,filename,rep_quick);
        else
          error=maria_repair_parallel(param,info,filename,rep_quick);
        state_updated=1;
      }
      else
        error=maria_repair(param, info,filename,rep_quick);
    }
    if (!error && (param->testflag & T_SORT_RECORDS))
    {
      /*
        The data file is nowadays reopened in the repair code so we should
        soon remove the following reopen-code
      */
#ifndef TO_BE_REMOVED
      if (param->out_flag & O_NEW_DATA)
      {			/* Change temp file to org file */
        VOID(my_close(info->dfile.file, MYF(MY_WME))); /* Close new file */
        error|=maria_change_to_newfile(filename,MARIA_NAME_DEXT,DATA_TMP_EXT,
                                       0, MYF(0));
        if (_ma_open_datafile(info,info->s, NullS, -1))
          error=1;
        param->out_flag&= ~O_NEW_DATA; /* We are using new datafile */
        param->read_cache.file= info->dfile.file;
      }
#endif
      if (! error)
      {
        uint key;
        /*
          We can't update the index in maria_sort_records if we have a
          prefix compressed or fulltext index
        */
        my_bool update_index=1;
        for (key=0 ; key < share->base.keys; key++)
          if (share->keyinfo[key].flag & (HA_BINARY_PACK_KEY|HA_FULLTEXT))
            update_index=0;

        error=maria_sort_records(param,info,filename,param->opt_sort_key,
                                 /* what is the following parameter for ? */
                                 (my_bool) !(param->testflag & T_REP),
                                 update_index);
        datafile= info->dfile.file;	/* This is now locked */
        if (!error && !update_index)
        {
          if (param->verbose)
            puts("Table had a compressed index;  We must now recreate the index");
          error=maria_repair_by_sort(param,info,filename,1);
        }
      }
    }
    if (!error && (param->testflag & T_SORT_INDEX))
      error= maria_sort_index(param,info,filename);
    if (!error && (param->testflag & T_ZEROFILL))
      error= maria_zerofill(param, info, filename);
    if (!error)
    {
      DBUG_PRINT("info", ("Reseting crashed state"));
      share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
                               STATE_CRASHED_ON_REPAIR | STATE_IN_REPAIR);
    }
    else
      maria_mark_crashed(info);
  }
  else if ((param->testflag & T_CHECK) || !(param->testflag & T_AUTO_INC))
  {
    if (!(param->testflag & T_VERY_SILENT) || param->testflag & T_INFO)
      printf("Checking Aria file: %s\n",filename);
    if (!(param->testflag & T_SILENT))
      printf("Data records: %7s   Deleted blocks: %7s\n",
             llstr(info->state->records,llbuff),
             llstr(info->state->del,llbuff2));
    maria_chk_init_for_check(param, info);
    if (opt_warning_for_wrong_transid == 0)
      param->max_trid= ~ (ulonglong) 0;
    error= maria_chk_status(param,info);
    maria_intersect_keys_active(share->state.key_map, param->keys_in_use);
    error|= maria_chk_size(param,info);
    if (!error || !(param->testflag & (T_FAST | T_FORCE_CREATE)))
      error|=maria_chk_del(param, info,param->testflag);
    if ((!error || (!(param->testflag & (T_FAST | T_FORCE_CREATE)) &&
                    !param->start_check_pos)))
    {
      error|=maria_chk_key(param, info);
      if (!error && (param->testflag & (T_STATISTICS | T_AUTO_INC)))
        error=maria_update_state_info(param, info,
                                      ((param->testflag & T_STATISTICS) ?
                                       UPDATE_STAT : 0) |
                                      ((param->testflag & T_AUTO_INC) ?
                                       UPDATE_AUTO_INC : 0));
    }
    if ((!rep_quick && !error) ||
        !(param->testflag & (T_FAST | T_FORCE_CREATE)))
    {
      VOID(init_io_cache(&param->read_cache,datafile,
                         (uint) param->read_buffer_length,
                         READ_CACHE,
                         (param->start_check_pos ?
                          param->start_check_pos :
                          share->pack.header_length),
                         1,
                         MYF(MY_WME)));
      maria_lock_memory(param);
      if ((info->s->data_file_type != STATIC_RECORD) ||
          (param->testflag & (T_EXTEND | T_MEDIUM)))
        error|=maria_chk_data_link(param, info,
                                   test(param->testflag & T_EXTEND));
      VOID(end_io_cache(&param->read_cache));
    }
    if (!error)
    {
      if (((share->state.changed &
            (STATE_CHANGED | STATE_CRASHED | STATE_CRASHED_ON_REPAIR |
             STATE_IN_REPAIR)) ||
           share->state.open_count != 0)
          && (param->testflag & T_UPDATE_STATE))
        info->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
      DBUG_PRINT("info", ("Reseting crashed state"));
      share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
                               STATE_CRASHED_ON_REPAIR | STATE_IN_REPAIR);
    }
    else if (!maria_is_crashed(info) &&
             (param->testflag & T_UPDATE_STATE))
    {						/* Mark crashed */
      maria_mark_crashed(info);
      info->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
    }
  }

  if ((param->testflag & T_AUTO_INC) ||
      ((param->testflag & T_REP_ANY) && info->s->base.auto_key))
    _ma_update_auto_increment_key(param, info,
                                  (my_bool) !test(param->testflag & T_AUTO_INC));

  if (info->update & HA_STATE_CHANGED && ! (param->testflag & T_READONLY))
    error|=maria_update_state_info(param, info,
                                   UPDATE_OPEN_COUNT |
                                   (((param->testflag & T_REP_ANY) ?
                                     UPDATE_TIME : 0) |
                                    (state_updated ? UPDATE_STAT : 0) |
                                    ((param->testflag & T_SORT_RECORDS) ?
                                     UPDATE_SORT : 0)));
  info->update&= ~HA_STATE_CHANGED;
  _ma_reenable_logging_for_table(info, FALSE);
  maria_lock_database(info, F_UNLCK);

end2:
  if (maria_close(info))
  {
    _ma_check_print_error(param, default_close_errmsg, my_errno, filename);
    DBUG_RETURN(1);
  }
  end_pagecache(maria_pagecache, 1);
  if (error == 0)
  {
    if (param->out_flag & O_NEW_DATA)
      error|=maria_change_to_newfile(filename,MARIA_NAME_DEXT,DATA_TMP_EXT,
                                     param->backup_time,
                                     ((param->testflag & T_BACKUP_DATA) ?
                                      MYF(MY_REDEL_MAKE_BACKUP) : MYF(0)));
  }
  if (opt_transaction_logging &&
      share->base.born_transactional && !error &&
      (param->testflag & (T_REP_ANY | T_SORT_RECORDS | T_SORT_INDEX |
                          T_ZEROFILL)))
    error= write_log_record(param);

  if (param->not_visible_rows_found && (param->testflag & T_VERBOSE))
  {
    char buff[22];
    printf("Max transaction id found: %s\n",
           llstr(param->max_found_trid, buff));
  }

  VOID(fflush(stdout)); VOID(fflush(stderr));

  if (param->error_printed)
  {
    if (param->testflag & (T_REP_ANY | T_SORT_RECORDS | T_SORT_INDEX))
    {
      VOID(fprintf(stderr,
		   "Aria table '%s' is not fixed because of errors\n",
		   filename));
      if (param->testflag & T_REP_ANY)
	VOID(fprintf(stderr,
		     "Try fixing it by using the --safe-recover (-o), the --force (-f) option or by not using the --quick (-q) flag\n"));
    }
    else if (!(param->error_printed & 2) &&
	     !(param->testflag & T_FORCE_CREATE))
      VOID(fprintf(stderr,
      "Aria table '%s' is corrupted\nFix it using switch \"-r\" or \"-o\"\n",
	      filename));
  }
  else if (param->warning_printed &&
	   ! (param->testflag & (T_REP_ANY | T_SORT_RECORDS | T_SORT_INDEX |
			  T_FORCE_CREATE)))
    VOID(fprintf(stderr, "Aria table '%s' is usable but should be fixed\n",
		 filename));
  VOID(fflush(stderr));
  DBUG_RETURN(error);
} /* maria_chk */


/* Write info about table */

static void descript(HA_CHECK *param, register MARIA_HA *info, char *name)
{
  uint key,keyseg_nr,field;
  reg3 MARIA_KEYDEF *keyinfo;
  reg2 HA_KEYSEG *keyseg;
  reg4 const char *text;
  char buff[200],length[10],*pos,*end;
  enum en_fieldtype type;
  MARIA_SHARE *share= info->s;
  char llbuff[22],llbuff2[22];
  DBUG_ENTER("descript");

  if (param->testflag & T_VERY_SILENT)
  {
    longlong checksum= info->state->checksum;
    if (!(share->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD)))
      checksum= 0;
    printf("%s %s %s\n", name, llstr(info->state->records,llbuff),
           llstr(checksum, llbuff2));
    DBUG_VOID_RETURN;
  }

  printf("Aria file:          %s\n",name);
  printf("Record format:       %s\n", record_formats[share->data_file_type]);
  printf("Crashsafe:           %s\n",
         share->base.born_transactional ? "yes" : "no");
  printf("Character set:       %s (%d)\n",
	 get_charset_name(share->state.header.language),
	 share->state.header.language);

  if (param->testflag & T_VERBOSE)
  {
    printf("File-version:        %d\n",
	   (int) share->state.header.file_version[3]);
    if (share->state.create_time)
    {
      get_date(buff,1,share->state.create_time);
      printf("Creation time:       %s\n",buff);
    }
    if (share->state.check_time)
    {
      get_date(buff,1,share->state.check_time);
      printf("Recover time:        %s\n",buff);
    }
    if (share->base.born_transactional)
    {
      printf("LSNs:                create_rename (%lu,0x%lx),"
             " state_horizon (%lu,0x%lx), skip_redo (%lu,0x%lx)\n",
             LSN_IN_PARTS(share->state.create_rename_lsn),
             LSN_IN_PARTS(share->state.is_of_horizon),
             LSN_IN_PARTS(share->state.skip_redo_lsn));
    }
    compile_time_assert((MY_UUID_STRING_LENGTH + 1) <= sizeof(buff));
    buff[MY_UUID_STRING_LENGTH]= 0;
    my_uuid2str(share->base.uuid, buff);
    printf("UUID:                %s\n", buff);
    pos=buff;
    if (share->state.changed & STATE_CRASHED)
      strmov(buff, share->state.changed & STATE_CRASHED_ON_REPAIR ?
             "crashed on repair" : "crashed");
    else
    {
      if (share->state.open_count)
	pos=strmov(pos,"open,");
      if (share->state.changed & STATE_CHANGED)
	pos=strmov(pos,"changed,");
      else
	pos=strmov(pos,"checked,");
      if (!(share->state.changed & STATE_NOT_ANALYZED))
	pos=strmov(pos,"analyzed,");
      if (!(share->state.changed & STATE_NOT_OPTIMIZED_KEYS))
	pos=strmov(pos,"optimized keys,");
      if (!(share->state.changed & STATE_NOT_SORTED_PAGES))
	pos=strmov(pos,"sorted index pages,");
      if (!(share->state.changed & STATE_NOT_ZEROFILLED))
	pos=strmov(pos,"zerofilled,");
      if (!(share->state.changed & STATE_NOT_MOVABLE))
	pos=strmov(pos,"movable,");
      pos[-1]=0;				/* Remove extra ',' */
    }
    printf("Status:              %s\n",buff);
    if (share->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
      printf("Checksum:  %26s\n",llstr(info->state->checksum,llbuff));
;
    if (share->options & HA_OPTION_DELAY_KEY_WRITE)
      printf("Keys are only flushed at close\n");

    if (share->options & HA_OPTION_PAGE_CHECKSUM)
      printf("Page checksums are used\n");
    if (share->base.auto_key)
    {
      printf("Auto increment key:  %16d  Last value:         %18s\n",
	     share->base.auto_key,
	     llstr(share->state.auto_increment,llbuff));
    }
  }
  printf("Data records:        %16s  Deleted blocks:     %18s\n",
	 llstr(info->state->records,llbuff),llstr(info->state->del,llbuff2));
  if (param->testflag & T_SILENT)
    DBUG_VOID_RETURN;				/* This is enough */

  if (param->testflag & T_VERBOSE)
  {
#ifdef USE_RELOC
    printf("Init-relocation:     %16s\n",llstr(share->base.reloc,llbuff));
#endif
    printf("Datafile parts:      %16s  Deleted data:       %18s\n",
	   llstr(share->state.split,llbuff),
	   llstr(info->state->empty,llbuff2));
    printf("Datafile pointer (bytes): %11d  Keyfile pointer (bytes): %13d\n",
	   share->rec_reflength,share->base.key_reflength);
    printf("Datafile length:     %16s  Keyfile length:     %18s\n",
	   llstr(info->state->data_file_length,llbuff),
	   llstr(info->state->key_file_length,llbuff2));

    if (info->s->base.reloc == 1L && info->s->base.records == 1L)
      puts("This is a one-record table");
    else
    {
      if (share->base.max_data_file_length != HA_OFFSET_ERROR ||
	  share->base.max_key_file_length != HA_OFFSET_ERROR)
	printf("Max datafile length: %16s  Max keyfile length: %18s\n",
	       llstr(share->base.max_data_file_length-1,llbuff),
	       llstr(share->base.max_key_file_length-1,llbuff2));
    }
  }
  printf("Block_size:          %16d\n",(int) share->block_size);
  printf("Recordlength:        %16d\n",(int) share->base.pack_reclength);
  if (! maria_is_all_keys_active(share->state.key_map, share->base.keys))
  {
    longlong2str(share->state.key_map,buff,2,1);
    printf("Using only keys '%s' of %d possibly keys\n",
	   buff, share->base.keys);
  }
  puts("\nTable description:");
  printf("Key Start Len Index   Type");
  if (param->testflag & T_VERBOSE)
    printf("                     Rec/key         Root  Blocksize");
  VOID(putchar('\n'));

  for (key=keyseg_nr=0, keyinfo= &share->keyinfo[0] ;
       key < share->base.keys;
       key++,keyinfo++)
  {
    keyseg=keyinfo->seg;
    if (keyinfo->flag & HA_NOSAME) text="unique ";
    else if (keyinfo->flag & HA_FULLTEXT) text="fulltext ";
    else text="multip.";

    pos=buff;
    if (keyseg->flag & HA_REVERSE_SORT)
      *pos++ = '-';
    pos=strmov(pos,type_names[keyseg->type]);
    *pos++ = ' ';
    *pos=0;
    if (keyinfo->flag & HA_PACK_KEY)
      pos=strmov(pos,prefix_packed_txt);
    if (keyinfo->flag & HA_BINARY_PACK_KEY)
      pos=strmov(pos,bin_packed_txt);
    if (keyseg->flag & HA_SPACE_PACK)
      pos=strmov(pos,diff_txt);
    if (keyseg->flag & HA_BLOB_PART)
      pos=strmov(pos,blob_txt);
    if (keyseg->flag & HA_NULL_PART)
      pos=strmov(pos,null_txt);
    *pos=0;

    printf("%-4d%-6ld%-3d %-8s%-23s",
	   key+1,(long) keyseg->start+1,keyseg->length,text,buff);
    if (share->state.key_root[key] != HA_OFFSET_ERROR)
      llstr(share->state.key_root[key],buff);
    else
      buff[0]=0;
    if (param->testflag & T_VERBOSE)
      printf("%9.0f %12s %10d",
	     share->state.rec_per_key_part[keyseg_nr++],
	     buff,keyinfo->block_length);
    VOID(putchar('\n'));
    while ((++keyseg)->type != HA_KEYTYPE_END)
    {
      pos=buff;
      if (keyseg->flag & HA_REVERSE_SORT)
	*pos++ = '-';
      pos=strmov(pos,type_names[keyseg->type]);
      *pos++= ' ';
      if (keyseg->flag & HA_SPACE_PACK)
	pos=strmov(pos,diff_txt);
      if (keyseg->flag & HA_BLOB_PART)
	pos=strmov(pos,blob_txt);
      if (keyseg->flag & HA_NULL_PART)
	pos=strmov(pos,null_txt);
      *pos=0;
      printf("    %-6ld%-3d         %-21s",
	     (long) keyseg->start+1,keyseg->length,buff);
      if (param->testflag & T_VERBOSE)
	printf("%11.0f", share->state.rec_per_key_part[keyseg_nr++]);
      VOID(putchar('\n'));
    }
    keyseg++;
  }
  if (share->state.header.uniques)
  {
    MARIA_UNIQUEDEF *uniqueinfo;
    puts("\nUnique  Key  Start  Len  Nullpos  Nullbit  Type");
    for (key=0,uniqueinfo= &share->uniqueinfo[0] ;
	 key < share->state.header.uniques; key++, uniqueinfo++)
    {
      my_bool new_row=0;
      char null_bit[8],null_pos[8];
      printf("%-8d%-5d",key+1,uniqueinfo->key+1);
      for (keyseg=uniqueinfo->seg ; keyseg->type != HA_KEYTYPE_END ; keyseg++)
      {
	if (new_row)
	  fputs("             ",stdout);
	null_bit[0]=null_pos[0]=0;
	if (keyseg->null_bit)
	{
	  sprintf(null_bit,"%d",keyseg->null_bit);
	  sprintf(null_pos,"%ld",(long) keyseg->null_pos+1);
	}
	printf("%-7ld%-5d%-9s%-10s%-30s\n",
	       (long) keyseg->start+1,keyseg->length,
	       null_pos,null_bit,
	       type_names[keyseg->type]);
	new_row=1;
      }
    }
  }
  if (param->verbose > 1)
  {
    char null_bit[8],null_pos[8];
    printf("\nField Start Length Nullpos Nullbit Type");
    if (share->options & HA_OPTION_COMPRESS_RECORD)
      printf("                         Huff tree  Bits");
    VOID(putchar('\n'));

    for (field=0 ; field < share->base.fields ; field++)
    {
      if (share->options & HA_OPTION_COMPRESS_RECORD)
	type=share->columndef[field].base_type;
      else
	type=(enum en_fieldtype) share->columndef[field].type;
      end=strmov(buff,field_pack[type]);
      if (share->options & HA_OPTION_COMPRESS_RECORD)
      {
	if (share->columndef[field].pack_type & PACK_TYPE_SELECTED)
	  end=strmov(end,", not_always");
	if (share->columndef[field].pack_type & PACK_TYPE_SPACE_FIELDS)
	  end=strmov(end,", no empty");
	if (share->columndef[field].pack_type & PACK_TYPE_ZERO_FILL)
	{
	  sprintf(end,", zerofill(%d)",share->columndef[field].space_length_bits);
	  end=strend(end);
	}
      }
      if (buff[0] == ',')
	strmov(buff,buff+2);
      int10_to_str((long) share->columndef[field].length,length,10);
      null_bit[0]=null_pos[0]=0;
      if (share->columndef[field].null_bit)
      {
	sprintf(null_bit,"%d",share->columndef[field].null_bit);
	sprintf(null_pos,"%d",share->columndef[field].null_pos+1);
      }
      printf("%-6d%-6u%-7s%-8s%-8s%-35s",field+1,
             (uint) share->columndef[field].offset+1,
             length, null_pos, null_bit, buff);
      if (share->options & HA_OPTION_COMPRESS_RECORD)
      {
	if (share->columndef[field].huff_tree)
	  printf("%3d    %2d",
		 (uint) (share->columndef[field].huff_tree-share->decode_trees)+1,
		 share->columndef[field].huff_tree->quick_table_bits);
      }
      VOID(putchar('\n'));
    }
    if (share->data_file_type == BLOCK_RECORD)
    {
      uint i;
      puts("\nBitmap  Data size  Description");
      for (i=0 ; i <= 7 ; i++)
        printf("%u           %5u  %s\n", i, share->bitmap.sizes[i],
               bitmap_description[i]);
    }
  }
  DBUG_VOID_RETURN;
} /* describe */


	/* Sort records according to one key */

static int maria_sort_records(HA_CHECK *param,
			   register MARIA_HA *info, char *name,
			   uint sort_key,
			   my_bool write_info,
			   my_bool update_index)
{
  int got_error;
  uint key;
  MARIA_KEYDEF *keyinfo;
  File new_file;
  uchar *temp_buff;
  ha_rows old_record_count;
  MARIA_SHARE *share= info->s;
  char llbuff[22],llbuff2[22];
  MARIA_SORT_INFO sort_info;
  MARIA_SORT_PARAM sort_param;
  MARIA_PAGE page;
  DBUG_ENTER("sort_records");

  bzero((char*)&sort_info,sizeof(sort_info));
  bzero((char*)&sort_param,sizeof(sort_param));
  sort_param.sort_info=&sort_info;
  sort_info.param=param;
  keyinfo= &share->keyinfo[sort_key];
  got_error=1;
  temp_buff=0;
  new_file= -1;

  if (! maria_is_key_active(share->state.key_map, sort_key))
  {
    _ma_check_print_warning(param,
			   "Can't sort table '%s' on key %d;  No such key",
		name,sort_key+1);
    param->error_printed=0;
    DBUG_RETURN(0);				/* Nothing to do */
  }
  if (keyinfo->flag & HA_FULLTEXT)
  {
    _ma_check_print_warning(param,"Can't sort table '%s' on FULLTEXT key %d",
			   name,sort_key+1);
    param->error_printed=0;
    DBUG_RETURN(0);				/* Nothing to do */
  }
  if (keyinfo->flag & HA_BINARY_PACK_KEY)
  {
    _ma_check_print_warning(param,
                            "Can't sort table '%s' on a key with prefix "
                            "packing %d",
                            name,sort_key+1);
    param->error_printed=0;
    DBUG_RETURN(0);
  }


  if (share->data_file_type == COMPRESSED_RECORD)
  {
    _ma_check_print_warning(param,"Can't sort read-only table '%s'", name);
    param->error_printed=0;
    DBUG_RETURN(0);				/* Nothing to do */
  }
  if (!(param->testflag & T_SILENT))
  {
    printf("- Sorting records for Aria table '%s'\n",name);
    if (write_info)
      printf("Data records: %9s   Deleted: %9s\n",
	     llstr(info->state->records,llbuff),
	     llstr(info->state->del,llbuff2));
  }
  if (share->state.key_root[sort_key] == HA_OFFSET_ERROR)
    DBUG_RETURN(0);				/* Nothing to do */

  if (init_io_cache(&info->rec_cache,-1,(uint) param->write_buffer_length,
		   WRITE_CACHE,share->pack.header_length,1,
		   MYF(MY_WME | MY_WAIT_IF_FULL)))
    goto err;
  info->opt_flag|=WRITE_CACHE_USED;

  if (!(temp_buff=(uchar*) my_alloca((uint) keyinfo->block_length)))
  {
    _ma_check_print_error(param,"Not enough memory for key block");
    goto err;
  }

  if (!(sort_param.record=
        (uchar*) my_malloc((uint) share->base.default_rec_buff_size, MYF(0))))
  {
    _ma_check_print_error(param,"Not enough memory for record");
    goto err;
  }

  fn_format(param->temp_filename,name,"", MARIA_NAME_DEXT,2+4+32);
  new_file= my_create(fn_format(param->temp_filename,
                                param->temp_filename,"",
                                DATA_TMP_EXT,
                                MY_REPLACE_EXT | MY_UNPACK_FILENAME),
                      0, param->tmpfile_createflag,
                      MYF(0));
  if (new_file < 0)
  {
    _ma_check_print_error(param,"Can't create new tempfile: '%s'",
			 param->temp_filename);
    goto err;
  }
  if (share->pack.header_length)
    if (maria_filecopy(param, new_file, info->dfile.file, 0L,
                       share->pack.header_length,
                       "datafile-header"))
      goto err;
  info->rec_cache.file=new_file;		/* Use this file for cacheing*/

  maria_lock_memory(param);
  for (key=0 ; key < share->base.keys ; key++)
    share->keyinfo[key].flag|= HA_SORT_ALLOWS_SAME;

  if (my_pread(share->kfile.file, temp_buff,
	       (uint) keyinfo->block_length,
	       share->state.key_root[sort_key],
	       MYF(MY_NABP+MY_WME)))
  {
    _ma_check_print_error(param, "Can't read indexpage from filepos: %s",
                          llstr(share->state.key_root[sort_key], llbuff));
    goto err;
  }

  /* Setup param for _ma_sort_write_record */
  sort_info.info=info;
  sort_info.new_data_file_type=share->data_file_type;
  sort_param.fix_datafile=1;
  sort_param.master=1;
  sort_param.filepos=share->pack.header_length;
  old_record_count=info->state->records;
  info->state->records=0;
  if (sort_info.new_data_file_type != COMPRESSED_RECORD)
    info->state->checksum=0;

  _ma_page_setup(&page, info, keyinfo, share->state.key_root[sort_key],
                 temp_buff);
  if (sort_record_index(&sort_param, &page, sort_key,new_file,update_index) ||
      maria_write_data_suffix(&sort_info,1) ||
      flush_io_cache(&info->rec_cache))
    goto err;

  if (info->state->records != old_record_count)
  {
    _ma_check_print_error(param,"found %s of %s records",
		llstr(info->state->records,llbuff),
		llstr(old_record_count,llbuff2));
    goto err;
  }

  VOID(my_close(info->dfile.file, MYF(MY_WME)));
  param->out_flag|=O_NEW_DATA;			/* Data in new file */
  info->dfile.file= new_file;                   /* Use new datafile */
  _ma_set_data_pagecache_callbacks(&info->dfile, info->s);

  info->state->del=0;
  info->state->empty=0;
  share->state.dellink= HA_OFFSET_ERROR;
  info->state->data_file_length=sort_param.filepos;
  share->state.split=info->state->records;	/* Only hole records */
  share->state.version=(ulong) time((time_t*) 0);

  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  if (param->testflag & T_WRITE_LOOP)
  {
    VOID(fputs("          \r",stdout)); VOID(fflush(stdout));
  }
  got_error=0;

err:
  if (got_error && new_file >= 0)
  {
    VOID(end_io_cache(&info->rec_cache));
    (void) my_close(new_file,MYF(MY_WME));
    (void) my_delete(param->temp_filename, MYF(MY_WME));
  }
  if (temp_buff)
  {
    my_afree(temp_buff);
  }
  my_free(sort_param.record,MYF(MY_ALLOW_ZERO_PTR));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  VOID(end_io_cache(&info->rec_cache));
  my_free(sort_info.buff,MYF(MY_ALLOW_ZERO_PTR));
  sort_info.buff=0;
  share->state.sortkey=sort_key;
  DBUG_RETURN(got_error);
} /* sort_records */


/* Sort records recursive using one index */

static int sort_record_index(MARIA_SORT_PARAM *sort_param,
                             MARIA_PAGE *ma_page, uint sort_key,
			     File new_file,my_bool update_index)
{
  MARIA_HA *info= ma_page->info;
  MARIA_SHARE *share= info->s;
  uint	page_flag, nod_flag,used_length;
  uchar *temp_buff,*keypos,*endpos;
  my_off_t next_page,rec_pos;
  uchar lastkey[MARIA_MAX_KEY_BUFF];
  char llbuff[22];
  MARIA_SORT_INFO *sort_info= sort_param->sort_info;
  HA_CHECK *param=sort_info->param;
  MARIA_KEY tmp_key;
  MARIA_PAGE new_page;
  const MARIA_KEYDEF *keyinfo= ma_page->keyinfo;
  DBUG_ENTER("sort_record_index");

  page_flag= ma_page->flag;
  nod_flag=  ma_page->node;
  temp_buff=0;
  tmp_key.keyinfo= (MARIA_KEYDEF*) keyinfo;
  tmp_key.data=    lastkey;

  if (nod_flag)
  {
    if (!(temp_buff= (uchar*) my_alloca(tmp_key.keyinfo->block_length)))
    {
      _ma_check_print_error(param,"Not Enough memory");
      DBUG_RETURN(-1);
    }
  }
  used_length= ma_page->size;
  keypos= ma_page->buff + share->keypage_header + nod_flag;
  endpos= ma_page->buff + used_length;
  for ( ;; )
  {
    _sanity(__FILE__,__LINE__);
    if (nod_flag)
    {
      next_page= _ma_kpos(nod_flag, keypos);
      if (my_pread(share->kfile.file, temp_buff,
		  (uint) tmp_key.keyinfo->block_length, next_page,
		   MYF(MY_NABP+MY_WME)))
      {
	_ma_check_print_error(param,"Can't read keys from filepos: %s",
		    llstr(next_page,llbuff));
	goto err;
      }
      _ma_page_setup(&new_page, info, ma_page->keyinfo, next_page, temp_buff);

      if (sort_record_index(sort_param, &new_page, sort_key,
			    new_file, update_index))
	goto err;
    }
    _sanity(__FILE__,__LINE__);
    if (keypos >= endpos ||
	!(*keyinfo->get_key)(&tmp_key, page_flag, nod_flag, &keypos))
      break;
    rec_pos= _ma_row_pos_from_key(&tmp_key);

    if ((*share->read_record)(info,sort_param->record,rec_pos))
    {
      _ma_check_print_error(param,"%d when reading datafile",my_errno);
      goto err;
    }
    if (rec_pos != sort_param->filepos && update_index)
    {
      _ma_dpointer(share, keypos - nod_flag - tmp_key.ref_length,
		   sort_param->filepos);
      if (maria_movepoint(info,sort_param->record,rec_pos,sort_param->filepos,
                          sort_key))
      {
	_ma_check_print_error(param,"%d when updating key-pointers",my_errno);
	goto err;
      }
    }
    if (_ma_sort_write_record(sort_param))
      goto err;
  }
  /* Clear end of block to get better compression if the table is backuped */
  bzero(ma_page->buff + used_length, keyinfo->block_length - used_length);
  if (my_pwrite(share->kfile.file, ma_page->buff, (uint)keyinfo->block_length,
		ma_page->pos, param->myf_rw))
  {
    _ma_check_print_error(param,"%d when updating keyblock",my_errno);
    goto err;
  }
  if (temp_buff)
    my_afree(temp_buff);
  DBUG_RETURN(0);
err:
  if (temp_buff)
    my_afree(temp_buff);
  DBUG_RETURN(1);
} /* sort_record_index */


static my_bool write_log_record(HA_CHECK *param)
{
  /*
    Now that all operations including O_NEW_DATA|INDEX are successfully
    done, we can write a log record.
  */
  MARIA_HA *info= maria_open(param->isam_file_name, O_RDWR, 0);
  if (info == NULL)
    _ma_check_print_error(param, default_open_errmsg, my_errno,
                          param->isam_file_name);
  else
  {
    if (write_log_record_for_repair(param, info))
      _ma_check_print_error(param, "%d when writing log record for"
                            " Aria table '%s'", my_errno,
                            param->isam_file_name);
    else if (maria_close(info))
      _ma_check_print_error(param, default_close_errmsg, my_errno,
                            param->isam_file_name);
    else
      return FALSE;
  }
  return TRUE;
}

#include "ma_check_standalone.h"
