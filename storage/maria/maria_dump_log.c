/* Copyright (C) 2007 MySQL AB & Sanja Belkin

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
extern void translog_example_table_init();
static const char *load_default_groups[]= { "maria_dump_log",0 };
static void get_options(int *argc,char * * *argv);
#ifndef DBUG_OFF
#if defined(__WIN__)
const char *default_dbug_option= "d:t:i:O,\\maria_dump_log.trace";
#else
const char *default_dbug_option= "d:t:i:o,/tmp/maria_dump_log.trace";
#endif
#endif
static ulonglong opt_offset;
static ulong opt_pages;
static const char *opt_file= NULL;
static File handler= -1;
static my_bool opt_unit= 0;
static struct my_option my_long_options[] =
{
#ifdef IMPLTMENTED
  {"body", 'b',
   "Print chunk body dump",
   (uchar **) &opt_body, (uchar **) &opt_body, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
#ifndef DBUG_OFF
  {"debug", '#', "Output debug log. Often the argument is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"file", 'f', "Path to file which will be read",
    (uchar**) &opt_file, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "offset", 'o', "Start reading log from this offset",
    (uchar**) &opt_offset, (uchar**) &opt_offset,
    0, GET_ULL, REQUIRED_ARG, 0, 0, ~(longlong) 0, 0, 0, 0 },
  { "pages", 'n', "Number of pages to read",
    (uchar**) &opt_pages, (uchar**) &opt_pages, 0,
    GET_ULONG, REQUIRED_ARG, (long) ~(ulong) 0,
    (long) 1, (long) ~(ulong) 0, (long) 0,
    (long) 1, 0},
  {"unit-test", 'U',
   "Use unit test record table (for logs created by unittests",
   (uchar **) &opt_unit, (uchar **) &opt_unit, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Print version and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void print_version(void)
{
  VOID(printf("%s Ver 1.0 for %s on %s\n",
              my_progname_short, SYSTEM_TYPE, MACHINE_TYPE));
  NETWARE_SET_SCREEN_MODE(1);
}


static void usage(void)
{
  print_version();
  puts("Copyright (C) 2008 MySQL AB");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,");
  puts("and you are welcome to modify and redistribute it under the GPL license\n");

  puts("Dump content of maria log pages.");
  VOID(printf("\nUsage: %s -f file OPTIONS\n", my_progname_short));
  my_print_help(my_long_options);
  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}


static my_bool
get_one_option(int optid __attribute__((unused)),
               const struct my_option *opt __attribute__((unused)),
               char *argument __attribute__((unused)))
{
  switch (optid) {
  case '?':
    usage();
    exit(0);
  case 'V':
    print_version();
    exit(0);
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

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (opt_file == NULL)
  {
    usage();
    exit(1);
  }
}


/**
  @brief maria_dump_log main function.
*/

int main(int argc, char **argv)
{
  char **default_argv;
  uchar buffer[TRANSLOG_PAGE_SIZE];
  MY_INIT(argv[0]);

  load_defaults("my", load_default_groups, &argc, &argv);
  default_argv= argv;
  get_options(&argc, &argv);

  if (opt_unit)
    translog_example_table_init();
  else
    translog_table_init();
  translog_fill_overhead_table();

  maria_data_root= (char *)".";

  if ((handler= my_open(opt_file, O_RDONLY, MYF(MY_WME))) < 0)
  {
    fprintf(stderr, "Can't open file: '%s'  errno: %d\n",
            opt_file, my_errno);
    goto err;
  }
  if (my_seek(handler, opt_offset, SEEK_SET, MYF(MY_WME)) !=
      opt_offset)
  {
     fprintf(stderr, "Can't set position %lld  file: '%s'  errno: %d\n",
             opt_offset, opt_file, my_errno);
     goto err;
  }
  for (;
       opt_pages;
       opt_offset+= TRANSLOG_PAGE_SIZE, opt_pages--)
  {
    if (my_pread(handler, buffer, TRANSLOG_PAGE_SIZE, opt_offset,
                 MYF(MY_NABP)))
    {
      if (my_errno == HA_ERR_FILE_TOO_SHORT)
        goto end;
      fprintf(stderr, "Can't read page at position %lld  file: '%s'  "
              "errno: %d\n", opt_offset, opt_file, my_errno);
      goto err;
    }
    printf("Page by offset %llu (0x%llx)\n", opt_offset, opt_offset);
    dump_page(buffer, handler);
  }

end:
  my_close(handler, MYF(0));
  free_defaults(default_argv);
  exit(0);
  return 0;				/* No compiler warning */

err:
  my_close(handler, MYF(0));
  fprintf(stderr, "%s: FAILED\n", my_progname_short);
  free_defaults(default_argv);
  exit(1);
}
