/* Copyright (C) 2000-2003 MySQL AB

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

/* Describe, check and repair of MyISAM tables */

#include "fulltext.h"

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

#ifdef OS2
#define _sanity(a,b)
#endif

static uint decode_bits;
static char **default_argv;
static const char *load_default_groups[]= { "myisamchk", 0 };
static const char *set_charset_name, *opt_tmpdir;
static CHARSET_INFO *set_charset;
static long opt_myisam_block_size;
static long opt_key_cache_block_size;
static const char *my_progname_short;
static int stopwords_inited= 0;
static MY_TMPDIR myisamchk_tmpdir;

static const char *type_names[]=
{ "?","char","binary", "short", "long", "float",
  "double","number","unsigned short",
  "unsigned long","longlong","ulonglong","int24",
  "uint24","int8","varchar", "varbin","?",
  "?"};

static const char *prefix_packed_txt="packed ",
		  *bin_packed_txt="prefix ",
		  *diff_txt="stripped ",
		  *null_txt="NULL",
		  *blob_txt="BLOB ";

static const char *field_pack[]=
{"","no endspace", "no prespace",
 "no zeros", "blob", "constant", "table-lockup",
 "always zero","varchar","unique-hash","?","?"};


static void get_options(int *argc,char * * *argv);
static void print_version(void);
static void usage(void);
static int myisamchk(MI_CHECK *param, char *filename);
static void descript(MI_CHECK *param, register MI_INFO *info, my_string name);
static int mi_sort_records(MI_CHECK *param, register MI_INFO *info,
                           my_string name, uint sort_key,
			   my_bool write_info, my_bool update_index);
static int sort_record_index(MI_SORT_PARAM *sort_param, MI_INFO *info,
                             MI_KEYDEF *keyinfo,
			     my_off_t page,uchar *buff,uint sortkey,
			     File new_file, my_bool update_index);

MI_CHECK check_param;

	/* Main program */

int main(int argc, char **argv)
{
  int error;
  MY_INIT(argv[0]);
  my_progname_short= my_progname+dirname_length(my_progname);

#ifdef __EMX__
  _wildcard (&argc, &argv);
#endif

  myisamchk_init(&check_param);
  check_param.opt_lock_memory=1;		/* Lock memory if possible */
  check_param.using_global_keycache = 0;
  get_options(&argc,(char***) &argv);
  myisam_quick_table_bits=decode_bits;
  error=0;
  while (--argc >= 0)
  {
    int new_error=myisamchk(&check_param, *(argv++));
    if ((check_param.testflag & T_REP_ANY) != T_REP)
      check_param.testflag&= ~T_REP;
    VOID(fflush(stdout));
    VOID(fflush(stderr));
    if ((check_param.error_printed | check_param.warning_printed) &&
	(check_param.testflag & T_FORCE_CREATE) &&
	(!(check_param.testflag & (T_REP | T_REP_BY_SORT | T_SORT_RECORDS |
				   T_SORT_INDEX))))
    {
      uint old_testflag=check_param.testflag;
      if (!(check_param.testflag & T_REP))
	check_param.testflag|= T_REP_BY_SORT;
      check_param.testflag&= ~T_EXTEND;			/* Don't needed  */
      error|=myisamchk(&check_param, argv[-1]);
      check_param.testflag= old_testflag;
      VOID(fflush(stdout));
      VOID(fflush(stderr));
    }
    else
      error|=new_error;
    if (argc && (!(check_param.testflag & T_SILENT) || check_param.testflag & T_INFO))
    {
      puts("\n---------\n");
      VOID(fflush(stdout));
    }
  }
  if (check_param.total_files > 1)
  {					/* Only if descript */
    char buff[22],buff2[22];
    if (!(check_param.testflag & T_SILENT) || check_param.testflag & T_INFO)
      puts("\n---------\n");
    printf("\nTotal of all %d MyISAM-files:\nData records: %9s   Deleted blocks: %9s\n",check_param.total_files,llstr(check_param.total_records,buff),
	   llstr(check_param.total_deleted,buff2));
  }
  free_defaults(default_argv);
  free_tmpdir(&myisamchk_tmpdir);
  ft_free_stopwords();
  my_end(check_param.testflag & T_INFO ? MY_CHECK_ERROR | MY_GIVE_INFO : MY_CHECK_ERROR);
  exit(error);
#ifndef _lint
  return 0;				/* No compiler warning */
#endif
} /* main */

enum options_mc {
  OPT_CHARSETS_DIR=256, OPT_SET_CHARSET,OPT_START_CHECK_POS,
  OPT_CORRECT_CHECKSUM, OPT_KEY_BUFFER_SIZE,
  OPT_KEY_CACHE_BLOCK_SIZE, OPT_MYISAM_BLOCK_SIZE,
  OPT_READ_BUFFER_SIZE, OPT_WRITE_BUFFER_SIZE, OPT_SORT_BUFFER_SIZE,
  OPT_SORT_KEY_BLOCKS, OPT_DECODE_BITS, OPT_FT_MIN_WORD_LEN,
  OPT_FT_MAX_WORD_LEN, OPT_FT_STOPWORD_FILE,
  OPT_MAX_RECORD_LENGTH
};

static struct my_option my_long_options[] =
{
  {"analyze", 'a',
   "Analyze distribution of keys. Will make some joins in MySQL faster. You can check the calculated distribution.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"block-search", 'b',
   "No help available.",
   0, 0, 0, GET_ULONG, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"backup", 'B',
   "Make a backup of the .MYD file as 'filename-time.BAK'.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.",
   (gptr*) &charsets_dir, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
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
   (gptr*) &check_param.max_data_file_length,
   (gptr*) &check_param.max_data_file_length,
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
   "Tell MyISAM to update only some specific keys. # is a bit mask of which keys to use. This can be used to get faster inserts.",
   (gptr*) &check_param.keys_in_use,
   (gptr*) &check_param.keys_in_use,
   0, GET_ULL, REQUIRED_ARG, -1, 0, 0, 0, 0, 0},
  {"max-record-length", OPT_MAX_RECORD_LENGTH,
   "Skip rows bigger than this if myisamchk can't allocate memory to hold it",
   (gptr*) &check_param.max_record_length,
   (gptr*) &check_param.max_record_length,
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
#ifdef DEBUG
  {"start-check-pos", OPT_START_CHECK_POS,
   "No help available.",
   0, 0, 0, GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"set-auto-increment", 'A',
   "Force auto_increment to start at this or higher value. If no value is given, then sets the next auto_increment value to the highest used value for the auto key + 1.",
   (gptr*) &check_param.auto_increment_value,
   (gptr*) &check_param.auto_increment_value,
   0, GET_ULL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"set-character-set", OPT_SET_CHARSET,
   "Change the character set used by the index",
   (gptr*) &set_charset_name, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"set-variable", 'O',
   "Change the value of a variable. Please note that this option is deprecated; you can set variables directly with --variable-name=value.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's',
   "Only print errors. One can use two -s to make myisamchk very silent.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sort-index", 'S',
   "Sort index blocks. This speeds up 'read-next' in applications.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sort-records", 'R',
   "Sort records according to an index. This makes your data much more localized and may speed up things. (It may be VERY slow to do a sort the first time!)",
   (gptr*) &check_param.opt_sort_key,
   (gptr*) &check_param.opt_sort_key,
   0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't',
   "Path for temporary files.",
   (gptr*) &opt_tmpdir,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"update-state", 'U',
   "Mark tables as crashed if any errors were found.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"unpack", 'u',
   "Unpack file packed with myisampack.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v',
   "Print more information. This can be used with --description and --check. Use many -v for more verbosity!",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V',
   "Print version and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"wait", 'w',
   "Wait if table is locked.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "key_buffer_size", OPT_KEY_BUFFER_SIZE, "",
    (gptr*) &check_param.use_buffers, (gptr*) &check_param.use_buffers, 0,
    GET_ULONG, REQUIRED_ARG, (long) USE_BUFFER_INIT, (long) MALLOC_OVERHEAD,
    (long) ~0L, (long) MALLOC_OVERHEAD, (long) IO_SIZE, 0},
  { "key_cache_block_size", OPT_KEY_CACHE_BLOCK_SIZE,  "",
    (gptr*) &opt_key_cache_block_size,
    (gptr*) &opt_key_cache_block_size, 0,
    GET_LONG, REQUIRED_ARG, MI_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH,
    MI_MAX_KEY_BLOCK_LENGTH, 0, MI_MIN_KEY_BLOCK_LENGTH, 0},
  { "myisam_block_size", OPT_MYISAM_BLOCK_SIZE,  "",
    (gptr*) &opt_myisam_block_size, (gptr*) &opt_myisam_block_size, 0,
    GET_LONG, REQUIRED_ARG, MI_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH,
    MI_MAX_KEY_BLOCK_LENGTH, 0, MI_MIN_KEY_BLOCK_LENGTH, 0},
  { "read_buffer_size", OPT_READ_BUFFER_SIZE, "",
    (gptr*) &check_param.read_buffer_length,
    (gptr*) &check_param.read_buffer_length, 0, GET_ULONG, REQUIRED_ARG,
    (long) READ_BUFFER_INIT, (long) MALLOC_OVERHEAD,
    (long) ~0L, (long) MALLOC_OVERHEAD, (long) 1L, 0},
  { "write_buffer_size", OPT_WRITE_BUFFER_SIZE, "",
    (gptr*) &check_param.write_buffer_length,
    (gptr*) &check_param.write_buffer_length, 0, GET_ULONG, REQUIRED_ARG,
    (long) READ_BUFFER_INIT, (long) MALLOC_OVERHEAD,
    (long) ~0L, (long) MALLOC_OVERHEAD, (long) 1L, 0},
  { "sort_buffer_size", OPT_SORT_BUFFER_SIZE, "",
    (gptr*) &check_param.sort_buffer_length,
    (gptr*) &check_param.sort_buffer_length, 0, GET_ULONG, REQUIRED_ARG,
    (long) SORT_BUFFER_INIT, (long) (MIN_SORT_BUFFER + MALLOC_OVERHEAD),
    (long) ~0L, (long) MALLOC_OVERHEAD, (long) 1L, 0},
  { "sort_key_blocks", OPT_SORT_KEY_BLOCKS, "",
    (gptr*) &check_param.sort_key_blocks,
    (gptr*) &check_param.sort_key_blocks, 0, GET_ULONG, REQUIRED_ARG,
    BUFFERS_WHEN_SORTING, 4L, 100L, 0L, 1L, 0},
  { "decode_bits", OPT_DECODE_BITS, "", (gptr*) &decode_bits,
    (gptr*) &decode_bits, 0, GET_UINT, REQUIRED_ARG, 9L, 4L, 17L, 0L, 1L, 0},
  { "ft_min_word_len", OPT_FT_MIN_WORD_LEN, "", (gptr*) &ft_min_word_len,
    (gptr*) &ft_min_word_len, 0, GET_ULONG, REQUIRED_ARG, 4, 1, HA_FT_MAXCHARLEN,
    0, 1, 0},
  { "ft_max_word_len", OPT_FT_MAX_WORD_LEN, "", (gptr*) &ft_max_word_len,
    (gptr*) &ft_max_word_len, 0, GET_ULONG, REQUIRED_ARG, HA_FT_MAXCHARLEN, 10,
    HA_FT_MAXCHARLEN, 0, 1, 0},
  { "ft_stopword_file", OPT_FT_STOPWORD_FILE,
    "Use stopwords from this file instead of built-in list.",
    (gptr*) &ft_stopword_file, (gptr*) &ft_stopword_file, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


#include <help_start.h>

static void print_version(void)
{
  printf("%s  Ver 2.7 for %s at %s\n", my_progname, SYSTEM_TYPE,
	 MACHINE_TYPE);
  NETWARE_SET_SCREEN_MODE(1);
}


static void usage(void)
{
  print_version();
  puts("By Monty, for your professional use");
  puts("This software comes with NO WARRANTY: see the PUBLIC for details.\n");
  puts("Description, check and repair of MyISAM tables.");
  puts("Used without options all tables on the command will be checked for errors");
  printf("Usage: %s [OPTIONS] tables[.MYI]\n", my_progname_short);
  puts("\nGlobal options:\n\
  -#, --debug=...     Output debug log. Often this is 'd:t:o,filename'.\n\
  -?, --help          Display this help and exit.\n\
  -O, --set-variable var=option.\n\
                      Change the value of a variable. Please note that\n\
                      this option is deprecated; you can set variables\n\
                      directly with '--variable-name=value'.\n\
  -t, --tmpdir=path   Path for temporary files. Multiple paths can be\n\
                      specified, separated by ");
#if defined( __WIN__) || defined(OS2) || defined(__NETWARE__)
   puts("semicolon (;)");
#else
   puts("colon (:)");
#endif
                      puts(", they will be used\n\
                      in a round-robin fashion.\n\
  -s, --silent	      Only print errors.  One can use two -s to make\n\
		      myisamchk very silent.\n\
  -v, --verbose       Print more information. This can be used with\n\
                      --description and --check. Use many -v for more verbosity.\n\
  -V, --version       Print version and exit.\n\
  -w, --wait          Wait if table is locked.\n");
#ifdef DEBUG
  puts("  --start-check-pos=# Start reading file at given offset.\n");
#endif

  puts("Check options (check is the default action for myisamchk):\n\
  -c, --check	      Check table for errors.\n\
  -e, --extend-check  Check the table VERY throughly.  Only use this in\n\
                      extreme cases as myisamchk should normally be able to\n\
                      find out if the table is ok even without this switch.\n\
  -F, --fast	      Check only tables that haven't been closed properly.\n\
  -C, --check-only-changed\n\
		      Check only tables that have changed since last check.\n\
  -f, --force         Restart with '-r' if there are any errors in the table.\n\
		      States will be updated as with '--update-state'.\n\
  -i, --information   Print statistics information about table that is checked.\n\
  -m, --medium-check  Faster than extend-check, but only finds 99.99% of\n\
		      all errors.  Should be good enough for most cases.\n\
  -U  --update-state  Mark tables as crashed if you find any errors.\n\
  -T, --read-only     Don't mark table as checked.\n");

  puts("Repair options (When using '-r' or '-o'):\n\
  -B, --backup	      Make a backup of the .MYD file as 'filename-time.BAK'.\n\
  --correct-checksum  Correct checksum information for table.\n\
  -D, --data-file-length=#  Max length of data file (when recreating data\n\
                      file when it's full).\n\
  -e, --extend-check  Try to recover every possible row from the data file\n\
		      Normally this will also find a lot of garbage rows;\n\
		      Don't use this option if you are not totally desperate.\n\
  -f, --force         Overwrite old temporary files.\n\
  -k, --keys-used=#   Tell MyISAM to update only some specific keys. # is a\n\
	              bit mask of which keys to use. This can be used to\n\
		      get faster inserts.\n\
  --max-record-length=#\n\
                      Skip rows bigger than this if myisamchk can't allocate\n\
		      memory to hold it.\n\
  -r, --recover       Can fix almost anything except unique keys that aren't\n\
                      unique.\n\
  -n, --sort-recover  Forces recovering with sorting even if the temporary\n\
		      file would be very big.\n\
  -p, --parallel-recover\n\
                      Uses the same technique as '-r' and '-n', but creates\n\
                      all the keys in parallel, in different threads.\n\
  -o, --safe-recover  Uses old recovery method; Slower than '-r' but can\n\
		      handle a couple of cases where '-r' reports that it\n\
		      can't fix the data file.\n\
  --character-sets-dir=...\n\
                      Directory where character sets are.\n\
  --set-character-set=name\n\
 		      Change the character set used by the index.\n\
  -q, --quick         Faster repair by not modifying the data file.\n\
                      One can give a second '-q' to force myisamchk to\n\
		      modify the original datafile in case of duplicate keys.\n\
		      NOTE: Tables where the data file is currupted can't be\n\
		      fixed with this option.\n\
  -u, --unpack        Unpack file packed with myisampack.\n\
");

  puts("Other actions:\n\
  -a, --analyze	      Analyze distribution of keys. Will make some joins in\n\
		      MySQL faster.  You can check the calculated distribution\n\
		      by using '--description --verbose table_name'.\n\
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
                       Find a record, a block at given offset belongs to.");

  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}

#include <help_end.h>

	 /* Read options */

static my_bool
get_one_option(int optid,
	       const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
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
      check_param.testflag&= ~(T_UNPACK | T_REP_BY_SORT);
    else
      check_param.testflag|= T_UNPACK | T_REP_BY_SORT;
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
      if (check_param.opt_sort_key >= MI_MAX_KEY)
      {
	fprintf(stderr,
		"The value of the sort key is bigger than max key: %d.\n",
		MI_MAX_KEY);
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
    if (argument == disabled_my_option)
    {
      DBUG_POP();
    }
    else
    {
      DBUG_PUSH(argument ? argument : "d:t:o,/tmp/myisamchk.trace");
    }
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
#ifdef DEBUG					/* Only useful if debugging */
  case OPT_START_CHECK_POS:
    check_param.start_check_pos= strtoull(argument, NULL, 0);
    break;
#endif
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

  if (init_tmpdir(&myisamchk_tmpdir, opt_tmpdir))
    exit(1);

  check_param.tmpdir=&myisamchk_tmpdir;
  check_param.key_cache_block_size= opt_key_cache_block_size;

  if (set_charset_name)
    if (!(set_charset=get_charset_by_name(set_charset_name, MYF(MY_WME))))
      exit(1);

  myisam_block_size=(uint) 1 << my_bit_log2(opt_myisam_block_size);
  return;
} /* get options */


	/* Check table */

static int myisamchk(MI_CHECK *param, my_string filename)
{
  int error,lock_type,recreate;
  int rep_quick= param->testflag & (T_QUICK | T_FORCE_UNIQUENESS);
  uint raid_chunks;
  MI_INFO *info;
  File datafile;
  char llbuff[22],llbuff2[22];
  my_bool state_updated=0;
  MYISAM_SHARE *share;
  DBUG_ENTER("myisamchk");

  param->out_flag=error=param->warning_printed=param->error_printed=
    recreate=0;
  datafile=0;
  param->isam_file_name=filename;		/* For error messages */
  if (!(info=mi_open(filename,
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
      mi_check_print_error(param,"'%s' doesn't have a correct index definition. You need to recreate it before you can do a repair",filename);
      break;
    case HA_ERR_NOT_A_TABLE:
      mi_check_print_error(param,"'%s' is not a MyISAM-table",filename);
      break;
    case HA_ERR_CRASHED_ON_USAGE:
      mi_check_print_error(param,"'%s' is marked as crashed",filename);
      break;
    case HA_ERR_CRASHED_ON_REPAIR:
      mi_check_print_error(param,"'%s' is marked as crashed after last repair",filename);
      break;
    case HA_ERR_OLD_FILE:
      mi_check_print_error(param,"'%s' is a old type of MyISAM-table", filename);
      break;
    case HA_ERR_END_OF_FILE:
      mi_check_print_error(param,"Couldn't read complete header from '%s'", filename);
      break;
    case EAGAIN:
      mi_check_print_error(param,"'%s' is locked. Use -w to wait until unlocked",filename);
      break;
    case ENOENT:
      mi_check_print_error(param,"File '%s' doesn't exist",filename);
      break;
    case EACCES:
      mi_check_print_error(param,"You don't have permission to use '%s'",filename);
      break;
    default:
      mi_check_print_error(param,"%d when opening MyISAM-table '%s'",
		  my_errno,filename);
      break;
    }
    DBUG_RETURN(1);
  }
  share=info->s;
  share->options&= ~HA_OPTION_READ_ONLY_DATA; /* We are modifing it */
  share->tot_locks-= share->r_locks;
  share->r_locks=0;
  raid_chunks=share->base.raid_chunks;

  /*
    Skip the checking of the file if:
    We are using --fast and the table is closed properly
    We are using --check-only-changed-tables and the table hasn't changed
  */
  if (param->testflag & (T_FAST | T_CHECK_ONLY_CHANGED))
  {
    my_bool need_to_check= mi_is_crashed(info) || share->state.open_count != 0;

    if ((param->testflag & (T_REP_ANY | T_SORT_RECORDS)) &&
	((share->state.changed & (STATE_CHANGED | STATE_CRASHED |
				  STATE_CRASHED_ON_REPAIR) ||
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
				 STATE_CRASHED_ON_REPAIR)))
      need_to_check=1;
    if (!need_to_check)
    {
      if (!(param->testflag & T_SILENT) || param->testflag & T_INFO)
	printf("MyISAM file: %s is already checked\n",filename);
      if (mi_close(info))
      {
	mi_check_print_error(param,"%d when closing MyISAM-table '%s'",
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
       MI_STATE_INFO_SIZE ||
       mi_uint2korr(share->state.header.base_info_length) !=
       MI_BASE_INFO_SIZE ||
       ((param->keys_in_use & ~share->state.key_map) &
	(((ulonglong) 1L << share->base.keys)-1)) ||
       test_if_almost_full(info) ||
       info->s->state.header.file_version[3] != myisam_file_magic[3] ||
       (set_charset && set_charset->number != share->state.header.language) ||
       myisam_block_size != MI_KEY_BLOCK_LENGTH))
  {
    if (set_charset)
      param->language=set_charset->number;
    if (recreate_table(param, &info,filename))
    {
      VOID(fprintf(stderr,
		   "MyISAM-table '%s' is not fixed because of errors\n",
	      filename));
      return(-1);
    }
    recreate=1;
    if (!(param->testflag & T_REP_ANY))
    {
      param->testflag|=T_REP_BY_SORT;		/* if only STATISTICS */
      if (!(param->testflag & T_SILENT))
	printf("- '%s' has old table-format. Recreating index\n",filename);
      rep_quick|=T_QUICK;
    }
    share=info->s;
    share->tot_locks-= share->r_locks;
    share->r_locks=0;
  }

  if (param->testflag & T_DESCRIPT)
  {
    param->total_files++;
    param->total_records+=info->state->records;
    param->total_deleted+=info->state->del;
    descript(param, info, filename);
  }
  else
  {
    if (!stopwords_inited++)
      ft_init_stopwords();

    if (!(param->testflag & T_READONLY))
      lock_type = F_WRLCK;			/* table is changed */
    else
      lock_type= F_RDLCK;
    if (info->lock_type == F_RDLCK)
      info->lock_type=F_UNLCK;			/* Read only table */
    if (_mi_readinfo(info,lock_type,0))
    {
      mi_check_print_error(param,"Can't lock indexfile of '%s', error: %d",
		  filename,my_errno);
      param->error_printed=0;
      goto end2;
    }
    /*
      _mi_readinfo() has locked the table.
      We mark the table as locked (without doing file locks) to be able to
      use functions that only works on locked tables (like row caching).
    */
    mi_lock_database(info, F_EXTRA_LCK);
    datafile=info->dfile;

    if (param->testflag & (T_REP_ANY | T_SORT_RECORDS | T_SORT_INDEX))
    {
      if (param->testflag & T_REP_ANY)
      {
	ulonglong tmp=share->state.key_map;
	share->state.key_map= (((ulonglong) 1 << share->base.keys)-1)
	  & param->keys_in_use;
	if (tmp != share->state.key_map)
	  info->update|=HA_STATE_CHANGED;
      }
      if (rep_quick && chk_del(param, info, param->testflag & ~T_VERBOSE))
      {
	if (param->testflag & T_FORCE_CREATE)
	{
	  rep_quick=0;
	  mi_check_print_info(param,"Creating new data file\n");
	}
	else
	{
	  error=1;
	  mi_check_print_error(param,
			       "Quick-recover aborted; Run recovery without switch 'q'");
	}
      }
      if (!error)
      {
	if ((param->testflag & (T_REP_BY_SORT | T_REP_PARALLEL)) &&
	    (share->state.key_map ||
	     (rep_quick && !param->keys_in_use && !recreate)) &&
	    mi_test_if_sort_rep(info, info->state->records,
				info->s->state.key_map,
				param->force_sort))
	{
          if (param->testflag & T_REP_BY_SORT)
            error=mi_repair_by_sort(param,info,filename,rep_quick);
          else
            error=mi_repair_parallel(param,info,filename,rep_quick);
	  state_updated=1;
	}
	else if (param->testflag & T_REP_ANY)
	  error=mi_repair(param, info,filename,rep_quick);
      }
      if (!error && param->testflag & T_SORT_RECORDS)
      {
	/*
	  The data file is nowadays reopened in the repair code so we should
	  soon remove the following reopen-code
	*/
#ifndef TO_BE_REMOVED
	if (param->out_flag & O_NEW_DATA)
	{			/* Change temp file to org file */
	  VOID(my_close(info->dfile,MYF(MY_WME))); /* Close new file */
	  error|=change_to_newfile(filename,MI_NAME_DEXT,DATA_TMP_EXT,
				   raid_chunks,
				   MYF(0));
	  if (mi_open_datafile(info,info->s, -1))
	    error=1;
	  param->out_flag&= ~O_NEW_DATA; /* We are using new datafile */
	  param->read_cache.file=info->dfile;
	}
#endif
	if (! error)
	{
	  uint key;
	  /*
	    We can't update the index in mi_sort_records if we have a
	    prefix compressed or fulltext index
	  */
	  my_bool update_index=1;
	  for (key=0 ; key < share->base.keys; key++)
	    if (share->keyinfo[key].flag & (HA_BINARY_PACK_KEY|HA_FULLTEXT))
	      update_index=0;

	  error=mi_sort_records(param,info,filename,param->opt_sort_key,
                             /* what is the following parameter for ? */
				(my_bool) !(param->testflag & T_REP),
				update_index);
	  datafile=info->dfile;	/* This is now locked */
	  if (!error && !update_index)
	  {
	    if (param->verbose)
	      puts("Table had a compressed index;  We must now recreate the index");
	    error=mi_repair_by_sort(param,info,filename,1);
	  }
	}
      }
      if (!error && param->testflag & T_SORT_INDEX)
	error=mi_sort_index(param,info,filename);
      if (!error)
	share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
				 STATE_CRASHED_ON_REPAIR);
      else
	mi_mark_crashed(info);
    }
    else if ((param->testflag & T_CHECK) || !(param->testflag & T_AUTO_INC))
    {
      if (!(param->testflag & T_SILENT) || param->testflag & T_INFO)
	printf("Checking MyISAM file: %s\n",filename);
      if (!(param->testflag & T_SILENT))
	printf("Data records: %7s   Deleted blocks: %7s\n",
	       llstr(info->state->records,llbuff),
	       llstr(info->state->del,llbuff2));
      error =chk_status(param,info);
      share->state.key_map &=param->keys_in_use;
      error =chk_size(param,info);
      if (!error || !(param->testflag & (T_FAST | T_FORCE_CREATE)))
	error|=chk_del(param, info,param->testflag);
      if ((!error || (!(param->testflag & (T_FAST | T_FORCE_CREATE)) &&
		      !param->start_check_pos)))
      {
	error|=chk_key(param, info);
	if (!error && (param->testflag & (T_STATISTICS | T_AUTO_INC)))
	  error=update_state_info(param, info,
				  ((param->testflag & T_STATISTICS) ?
				   UPDATE_STAT : 0) |
				  ((param->testflag & T_AUTO_INC) ?
				   UPDATE_AUTO_INC : 0));
      }
      if ((!rep_quick && !error) ||
	  !(param->testflag & (T_FAST | T_FORCE_CREATE)))
      {
	if (param->testflag & (T_EXTEND | T_MEDIUM))
	  VOID(init_key_cache(dflt_key_cache,opt_key_cache_block_size,
                              param->use_buffers, 0, 0));
	VOID(init_io_cache(&param->read_cache,datafile,
			   (uint) param->read_buffer_length,
			   READ_CACHE,
			   (param->start_check_pos ?
			    param->start_check_pos :
			    share->pack.header_length),
			   1,
			   MYF(MY_WME)));
	lock_memory(param);
	if ((info->s->options & (HA_OPTION_PACK_RECORD |
				 HA_OPTION_COMPRESS_RECORD)) ||
	    (param->testflag & (T_EXTEND | T_MEDIUM)))
	  error|=chk_data_link(param, info, param->testflag & T_EXTEND);
	error|=flush_blocks(param, share->key_cache, share->kfile);
	VOID(end_io_cache(&param->read_cache));
      }
      if (!error)
      {
	if ((share->state.changed & STATE_CHANGED) &&
	    (param->testflag & T_UPDATE_STATE))
	  info->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
	share->state.changed&= ~(STATE_CHANGED | STATE_CRASHED |
				 STATE_CRASHED_ON_REPAIR);
      }
      else if (!mi_is_crashed(info) &&
	       (param->testflag & T_UPDATE_STATE))
      {						/* Mark crashed */
	mi_mark_crashed(info);
	info->update|=HA_STATE_CHANGED | HA_STATE_ROW_CHANGED;
      }
    }
  }
  if ((param->testflag & T_AUTO_INC) ||
      ((param->testflag & T_REP_ANY) && info->s->base.auto_key))
    update_auto_increment_key(param, info,
			      (my_bool) !test(param->testflag & T_AUTO_INC));

  if (!(param->testflag & T_DESCRIPT))
  {
    if (info->update & HA_STATE_CHANGED && ! (param->testflag & T_READONLY))
      error|=update_state_info(param, info,
			       UPDATE_OPEN_COUNT |
			       (((param->testflag & T_REP_ANY) ?
				 UPDATE_TIME : 0) |
				(state_updated ? UPDATE_STAT : 0) |
				((param->testflag & T_SORT_RECORDS) ?
				 UPDATE_SORT : 0)));
    VOID(lock_file(param, share->kfile,0L,F_UNLCK,"indexfile",filename));
    info->update&= ~HA_STATE_CHANGED;
  }
  mi_lock_database(info, F_UNLCK);
end2:
  if (mi_close(info))
  {
    mi_check_print_error(param,"%d when closing MyISAM-table '%s'",my_errno,filename);
    DBUG_RETURN(1);
  }
  if (error == 0)
  {
    if (param->out_flag & O_NEW_DATA)
      error|=change_to_newfile(filename,MI_NAME_DEXT,DATA_TMP_EXT,
			       raid_chunks,
			       ((param->testflag & T_BACKUP_DATA) ?
				MYF(MY_REDEL_MAKE_BACKUP) : MYF(0)));
    if (param->out_flag & O_NEW_INDEX)
      error|=change_to_newfile(filename,MI_NAME_IEXT,INDEX_TMP_EXT,0,
			       MYF(0));
  }
  VOID(fflush(stdout)); VOID(fflush(stderr));
  if (param->error_printed)
  {
    if (param->testflag & (T_REP_ANY | T_SORT_RECORDS | T_SORT_INDEX))
    {
      VOID(fprintf(stderr,
		   "MyISAM-table '%s' is not fixed because of errors\n",
		   filename));
      if (param->testflag & T_REP_ANY)
	VOID(fprintf(stderr,
		     "Try fixing it by using the --safe-recover (-o), the --force (-f) option or by not using the --quick (-q) flag\n"));
    }
    else if (!(param->error_printed & 2) &&
	     !(param->testflag & T_FORCE_CREATE))
      VOID(fprintf(stderr,
      "MyISAM-table '%s' is corrupted\nFix it using switch \"-r\" or \"-o\"\n",
	      filename));
  }
  else if (param->warning_printed &&
	   ! (param->testflag & (T_REP_ANY | T_SORT_RECORDS | T_SORT_INDEX |
			  T_FORCE_CREATE)))
    VOID(fprintf(stderr, "MyISAM-table '%s' is usable but should be fixed\n",
		 filename));
  VOID(fflush(stderr));
  DBUG_RETURN(error);
} /* myisamchk */


	 /* Write info about table */

static void descript(MI_CHECK *param, register MI_INFO *info, my_string name)
{
  uint key,keyseg_nr,field,start;
  reg3 MI_KEYDEF *keyinfo;
  reg2 HA_KEYSEG *keyseg;
  reg4 const char *text;
  char buff[160],length[10],*pos,*end;
  enum en_fieldtype type;
  MYISAM_SHARE *share=info->s;
  char llbuff[22],llbuff2[22];
  DBUG_ENTER("describe");

  printf("\nMyISAM file:         %s\n",name);
  fputs("Record format:       ",stdout);
  if (share->options & HA_OPTION_COMPRESS_RECORD)
    puts("Compressed");
  else if (share->options & HA_OPTION_PACK_RECORD)
    puts("Packed");
  else
    puts("Fixed length");
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
    pos=buff;
    if (share->state.changed & STATE_CRASHED)
      strmov(buff,"crashed");
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
      pos[-1]=0;				/* Remove extra ',' */
    }      
    printf("Status:              %s\n",buff);
    if (share->base.auto_key)
    {
      printf("Auto increment key:  %13d  Last value:         %13s\n",
	     share->base.auto_key,
	     llstr(share->state.auto_increment,llbuff));
    }
    if (share->base.raid_type)
    {
      printf("RAID:                Type:  %u   Chunks: %u  Chunksize: %lu\n",
	     share->base.raid_type,
	     share->base.raid_chunks,
	     share->base.raid_chunksize);
    }
    if (share->options & (HA_OPTION_CHECKSUM | HA_OPTION_COMPRESS_RECORD))
      printf("Checksum:  %23s\n",llstr(info->s->state.checksum,llbuff));
;
    if (share->options & HA_OPTION_DELAY_KEY_WRITE)
      printf("Keys are only flushed at close\n");

  }
  printf("Data records:        %13s  Deleted blocks:     %13s\n",
	 llstr(info->state->records,llbuff),llstr(info->state->del,llbuff2));
  if (param->testflag & T_SILENT)
    DBUG_VOID_RETURN;				/* This is enough */

  if (param->testflag & T_VERBOSE)
  {
#ifdef USE_RELOC
    printf("Init-relocation:     %13s\n",llstr(share->base.reloc,llbuff));
#endif
    printf("Datafile parts:      %13s  Deleted data:       %13s\n",
	   llstr(share->state.split,llbuff),
	   llstr(info->state->empty,llbuff2));
    printf("Datafile pointer (bytes):%9d  Keyfile pointer (bytes):%9d\n",
	   share->rec_reflength,share->base.key_reflength);
    printf("Datafile length:     %13s  Keyfile length:     %13s\n",
	   llstr(info->state->data_file_length,llbuff),
	   llstr(info->state->key_file_length,llbuff2));

    if (info->s->base.reloc == 1L && info->s->base.records == 1L)
      puts("This is a one-record table");
    else
    {
      if (share->base.max_data_file_length != HA_OFFSET_ERROR ||
	  share->base.max_key_file_length != HA_OFFSET_ERROR)
	printf("Max datafile length: %13s  Max keyfile length: %13s\n",
	       llstr(share->base.max_data_file_length-1,llbuff),
	       llstr(share->base.max_key_file_length-1,llbuff2));
    }
  }

  printf("Recordlength:        %13d\n",(int) share->base.pack_reclength);
  if (share->state.key_map != (((ulonglong) 1 << share->base.keys) -1))
  {
    longlong2str(share->state.key_map,buff,2);
    printf("Using only keys '%s' of %d possibly keys\n",
	   buff, share->base.keys);
  }
  puts("\ntable description:");
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

    printf("%-4d%-6ld%-3d %-8s%-21s",
	   key+1,(long) keyseg->start+1,keyseg->length,text,buff);
    if (share->state.key_root[key] != HA_OFFSET_ERROR)
      llstr(share->state.key_root[key],buff);
    else
      buff[0]=0;
    if (param->testflag & T_VERBOSE)
      printf("%11lu %12s %10d",
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
	printf("%11lu", share->state.rec_per_key_part[keyseg_nr++]);
      VOID(putchar('\n'));
    }
    keyseg++;
  }
  if (share->state.header.uniques)
  {
    MI_UNIQUEDEF *uniqueinfo;
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
    start=1;
    for (field=0 ; field < share->base.fields ; field++)
    {
      if (share->options & HA_OPTION_COMPRESS_RECORD)
	type=share->rec[field].base_type;
      else
	type=(enum en_fieldtype) share->rec[field].type;
      end=strmov(buff,field_pack[type]);
      if (share->options & HA_OPTION_COMPRESS_RECORD)
      {
	if (share->rec[field].pack_type & PACK_TYPE_SELECTED)
	  end=strmov(end,", not_always");
	if (share->rec[field].pack_type & PACK_TYPE_SPACE_FIELDS)
	  end=strmov(end,", no empty");
	if (share->rec[field].pack_type & PACK_TYPE_ZERO_FILL)
	{
	  sprintf(end,", zerofill(%d)",share->rec[field].space_length_bits);
	  end=strend(end);
	}
      }
      if (buff[0] == ',')
	strmov(buff,buff+2);
      int10_to_str((long) share->rec[field].length,length,10);
      null_bit[0]=null_pos[0]=0;
      if (share->rec[field].null_bit)
      {
	sprintf(null_bit,"%d",share->rec[field].null_bit);
	sprintf(null_pos,"%d",share->rec[field].null_pos+1);
      }
      printf("%-6d%-6d%-7s%-8s%-8s%-35s",field+1,start,length,
	     null_pos, null_bit, buff);
      if (share->options & HA_OPTION_COMPRESS_RECORD)
      {
	if (share->rec[field].huff_tree)
	  printf("%3d    %2d",
		 (uint) (share->rec[field].huff_tree-share->decode_trees)+1,
		 share->rec[field].huff_tree->quick_table_bits);
      }
      VOID(putchar('\n'));
      start+=share->rec[field].length;
    }
  }
  DBUG_VOID_RETURN;
} /* describe */


	/* Sort records according to one key */

static int mi_sort_records(MI_CHECK *param,
			   register MI_INFO *info, my_string name,
			   uint sort_key,
			   my_bool write_info,
			   my_bool update_index)
{
  int got_error;
  uint key;
  MI_KEYDEF *keyinfo;
  File new_file;
  uchar *temp_buff;
  ha_rows old_record_count;
  MYISAM_SHARE *share=info->s;
  char llbuff[22],llbuff2[22];
  SORT_INFO sort_info;
  MI_SORT_PARAM sort_param;
  DBUG_ENTER("sort_records");

  bzero((char*)&sort_info,sizeof(sort_info));
  bzero((char*)&sort_param,sizeof(sort_param));
  sort_param.sort_info=&sort_info;
  sort_info.param=param;
  keyinfo= &share->keyinfo[sort_key];
  got_error=1;
  temp_buff=0;
  new_file= -1;

  if (!(((ulonglong) 1 << sort_key) & share->state.key_map))
  {
    mi_check_print_warning(param,
			   "Can't sort table '%s' on key %d;  No such key",
		name,sort_key+1);
    param->error_printed=0;
    DBUG_RETURN(0);				/* Nothing to do */
  }
  if (keyinfo->flag & HA_FULLTEXT)
  {
    mi_check_print_warning(param,"Can't sort table '%s' on FULLTEXT key %d",
			   name,sort_key+1);
    param->error_printed=0;
    DBUG_RETURN(0);				/* Nothing to do */
  }
  if (share->data_file_type == COMPRESSED_RECORD)
  {
    mi_check_print_warning(param,"Can't sort read-only table '%s'", name);
    param->error_printed=0;
    DBUG_RETURN(0);				/* Nothing to do */
  }
  if (!(param->testflag & T_SILENT))
  {
    printf("- Sorting records for MyISAM-table '%s'\n",name);
    if (write_info)
      printf("Data records: %9s   Deleted: %9s\n",
	     llstr(info->state->records,llbuff),
	     llstr(info->state->del,llbuff2));
  }
  if (share->state.key_root[sort_key] == HA_OFFSET_ERROR)
    DBUG_RETURN(0);				/* Nothing to do */

  init_key_cache(dflt_key_cache, opt_key_cache_block_size, param->use_buffers,
                 0, 0);
  if (init_io_cache(&info->rec_cache,-1,(uint) param->write_buffer_length,
		   WRITE_CACHE,share->pack.header_length,1,
		   MYF(MY_WME | MY_WAIT_IF_FULL)))
    goto err;
  info->opt_flag|=WRITE_CACHE_USED;

  if (!(temp_buff=(uchar*) my_alloca((uint) keyinfo->block_length)))
  {
    mi_check_print_error(param,"Not enough memory for key block");
    goto err;
  }
  if (!(sort_param.record=(byte*) my_malloc((uint) share->base.pack_reclength,
					   MYF(0))))
  {
    mi_check_print_error(param,"Not enough memory for record");
    goto err;
  }
  fn_format(param->temp_filename,name,"", MI_NAME_DEXT,2+4+32);
  new_file=my_raid_create(fn_format(param->temp_filename,
				    param->temp_filename,"",
				    DATA_TMP_EXT,2+4),
			  0,param->tmpfile_createflag,
			  share->base.raid_type,
			  share->base.raid_chunks,
			  share->base.raid_chunksize,
			  MYF(0));
  if (new_file < 0)
  {
    mi_check_print_error(param,"Can't create new tempfile: '%s'",
			 param->temp_filename);
    goto err;
  }
  if (share->pack.header_length)
    if (filecopy(param,new_file,info->dfile,0L,share->pack.header_length,
		 "datafile-header"))
      goto err;
  info->rec_cache.file=new_file;		/* Use this file for cacheing*/

  lock_memory(param);
  for (key=0 ; key < share->base.keys ; key++)
    share->keyinfo[key].flag|= HA_SORT_ALLOWS_SAME;

  if (my_pread(share->kfile,(byte*) temp_buff,
	       (uint) keyinfo->block_length,
	       share->state.key_root[sort_key],
	       MYF(MY_NABP+MY_WME)))
  {
    mi_check_print_error(param,"Can't read indexpage from filepos: %s",
		(ulong) share->state.key_root[sort_key]);
    goto err;
  }

  /* Setup param for sort_write_record */
  sort_info.info=info;
  sort_info.new_data_file_type=share->data_file_type;
  sort_param.fix_datafile=1;
  sort_param.master=1;
  sort_param.filepos=share->pack.header_length;
  old_record_count=info->state->records;
  info->state->records=0;
  if (sort_info.new_data_file_type != COMPRESSED_RECORD)
    share->state.checksum=0;

  if (sort_record_index(&sort_param,info,keyinfo,share->state.key_root[sort_key],
			temp_buff, sort_key,new_file,update_index) ||
      write_data_suffix(&sort_info,1) ||
      flush_io_cache(&info->rec_cache))
    goto err;

  if (info->state->records != old_record_count)
  {
    mi_check_print_error(param,"found %s of %s records",
		llstr(info->state->records,llbuff),
		llstr(old_record_count,llbuff2));
    goto err;
  }

  VOID(my_close(info->dfile,MYF(MY_WME)));
  param->out_flag|=O_NEW_DATA;			/* Data in new file */
  info->dfile=new_file;				/* Use new datafile */
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
    (void) my_raid_delete(param->temp_filename, share->base.raid_chunks,
			  MYF(MY_WME));
  }
  if (temp_buff)
  {
    my_afree((gptr) temp_buff);
  }
  my_free(sort_param.record,MYF(MY_ALLOW_ZERO_PTR));
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  VOID(end_io_cache(&info->rec_cache));
  my_free(sort_info.buff,MYF(MY_ALLOW_ZERO_PTR));
  sort_info.buff=0;
  share->state.sortkey=sort_key;
  DBUG_RETURN(flush_blocks(param, share->key_cache, share->kfile) |
	      got_error);
} /* sort_records */


	 /* Sort records recursive using one index */

static int sort_record_index(MI_SORT_PARAM *sort_param,MI_INFO *info,
                             MI_KEYDEF *keyinfo,
			     my_off_t page, uchar *buff, uint sort_key,
			     File new_file,my_bool update_index)
{
  uint	nod_flag,used_length,key_length;
  uchar *temp_buff,*keypos,*endpos;
  my_off_t next_page,rec_pos;
  uchar lastkey[MI_MAX_KEY_BUFF];
  char llbuff[22];
  SORT_INFO *sort_info= sort_param->sort_info;
  MI_CHECK *param=sort_info->param;
  DBUG_ENTER("sort_record_index");

  nod_flag=mi_test_if_nod(buff);
  temp_buff=0;

  if (nod_flag)
  {
    if (!(temp_buff=(uchar*) my_alloca((uint) keyinfo->block_length)))
    {
      mi_check_print_error(param,"Not Enough memory");
      DBUG_RETURN(-1);
    }
  }
  used_length=mi_getint(buff);
  keypos=buff+2+nod_flag;
  endpos=buff+used_length;
  for ( ;; )
  {
    _sanity(__FILE__,__LINE__);
    if (nod_flag)
    {
      next_page=_mi_kpos(nod_flag,keypos);
      if (my_pread(info->s->kfile,(byte*) temp_buff,
		  (uint) keyinfo->block_length, next_page,
		   MYF(MY_NABP+MY_WME)))
      {
	mi_check_print_error(param,"Can't read keys from filepos: %s",
		    llstr(next_page,llbuff));
	goto err;
      }
      if (sort_record_index(sort_param, info,keyinfo,next_page,temp_buff,sort_key,
			    new_file, update_index))
	goto err;
    }
    _sanity(__FILE__,__LINE__);
    if (keypos >= endpos ||
	(key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,lastkey))
	== 0)
      break;
    rec_pos= _mi_dpos(info,0,lastkey+key_length);

    if ((*info->s->read_rnd)(info,sort_param->record,rec_pos,0))
    {
      mi_check_print_error(param,"%d when reading datafile",my_errno);
      goto err;
    }
    if (rec_pos != sort_param->filepos && update_index)
    {
      _mi_dpointer(info,keypos-nod_flag-info->s->rec_reflength,
		   sort_param->filepos);
      if (movepoint(info,sort_param->record,rec_pos,sort_param->filepos,
		    sort_key))
      {
	mi_check_print_error(param,"%d when updating key-pointers",my_errno);
	goto err;
      }
    }
    if (sort_write_record(sort_param))
      goto err;
  }
  /* Clear end of block to get better compression if the table is backuped */
  bzero((byte*) buff+used_length,keyinfo->block_length-used_length);
  if (my_pwrite(info->s->kfile,(byte*) buff,(uint) keyinfo->block_length,
		page,param->myf_rw))
  {
    mi_check_print_error(param,"%d when updating keyblock",my_errno);
    goto err;
  }
  if (temp_buff)
    my_afree((gptr) temp_buff);
  DBUG_RETURN(0);
err:
  if (temp_buff)
    my_afree((gptr) temp_buff);
  DBUG_RETURN(1);
} /* sort_record_index */



/*
  Check if myisamchk was killed by a signal
  This is overloaded by other programs that want to be able to abort
  sorting
*/

static int not_killed= 0;

volatile int *killed_ptr(MI_CHECK *param)
{
  return &not_killed;			/* always NULL */
}

	/* print warnings and errors */
	/* VARARGS */

void mi_check_print_info(MI_CHECK *param __attribute__((unused)),
			 const char *fmt,...)
{
  va_list args;

  va_start(args,fmt);
  VOID(vfprintf(stdout, fmt, args));
  VOID(fputc('\n',stdout));
  va_end(args);
}

/* VARARGS */

void mi_check_print_warning(MI_CHECK *param, const char *fmt,...)
{
  va_list args;
  DBUG_ENTER("mi_check_print_warning");

  fflush(stdout);
  if (!param->warning_printed && !param->error_printed)
  {
    if (param->testflag & T_SILENT)
      fprintf(stderr,"%s: MyISAM file %s\n",my_progname_short,
	      param->isam_file_name);
    param->out_flag|= O_DATA_LOST;
  }
  param->warning_printed=1;
  va_start(args,fmt);
  fprintf(stderr,"%s: warning: ",my_progname_short);
  VOID(vfprintf(stderr, fmt, args));
  VOID(fputc('\n',stderr));
  fflush(stderr);
  va_end(args);
  DBUG_VOID_RETURN;
}

/* VARARGS */

void mi_check_print_error(MI_CHECK *param, const char *fmt,...)
{
  va_list args;
  DBUG_ENTER("mi_check_print_error");
  DBUG_PRINT("enter",("format: %s",fmt));

  fflush(stdout);
  if (!param->warning_printed && !param->error_printed)
  {
    if (param->testflag & T_SILENT)
      fprintf(stderr,"%s: MyISAM file %s\n",my_progname_short,param->isam_file_name);
    param->out_flag|= O_DATA_LOST;
  }
  param->error_printed|=1;
  va_start(args,fmt);
  fprintf(stderr,"%s: error: ",my_progname_short);
  VOID(vfprintf(stderr, fmt, args));
  VOID(fputc('\n',stderr));
  fflush(stderr);
  va_end(args);
  DBUG_VOID_RETURN;
}
