#include "azlib.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <my_getopt.h>
#include <mysql_version.h>

#define BUFFER_LEN 1024

#define SHOW_VERSION "0.1"

static void get_options(int *argc,char * * *argv);
static void print_version(void);
static void usage(void);
static const char *opt_tmpdir;
static const char *new_auto_increment_value;
static const char *load_default_groups[]= { "archive_reader", 0 };
static char **default_argv;

int main(int argc, char *argv[])
{
  unsigned int ret;
  azio_stream reader_handle;

  MY_INIT(argv[0]);
  get_options(&argc, &argv);

  if (argc < 1)
  {
    printf("No file specified. \n");
    return 0;
  }

  if (!(ret= azopen(&reader_handle, argv[0], O_RDONLY|O_BINARY)))
  {
    printf("Could not open Archive file\n");
    return 0;
  }

  printf("Version %u\n", reader_handle.version);
  printf("Start position %llu\n", (unsigned long long)reader_handle.start);
  if (reader_handle.version > 2)
  {
    printf("Block size %u\n", reader_handle.block_size);
    printf("Rows %llu\n", reader_handle.rows);
    printf("Autoincrement %llu\n", reader_handle.auto_increment);
    printf("Check Point %llu\n", reader_handle.check_point);
    printf("Forced Flushes %llu\n", reader_handle.forced_flushes);
    printf("State %s\n", ( reader_handle.dirty ? "dirty" : "clean"));
  }

  azclose(&reader_handle);

  return 0;
}

static my_bool
get_one_option(int optid,
	       const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case 'c':
    printf("Not implemented yet\n");
    break;
  case 'f':
    printf("Not implemented yet\n");
    break;
  case 'q':
    printf("Not implemented yet\n");
    break;
  case 'V':
    print_version();
    exit(0);
  case 't':
    printf("Not implemented yet\n");
    break;
  case 'A':
    printf("Not implemented yet\n");
    break;
  case '?':
    usage();
    exit(0);
  case '#':
    if (argument == disabled_my_option)
    {
      DBUG_POP();
    }
    else
    {
      DBUG_PUSH(argument ? argument : "d:t:o,/tmp/archive_reader.trace");
    }
    break;
  }
  return 0;
}

static struct my_option my_long_options[] =
{
  {"check", 'c',
   "Check table for errors.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#',
   "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"force", 'f',
   "Restart with -r if there are any errors in the table.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?',
   "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q', "Faster repair by not modifying the data file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"repair", 'r', "Repair a damaged Archive version 3 or above file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"set-auto-increment", 'A',
   "Force auto_increment to start at this or higher value.",
   (gptr*) &new_auto_increment_value,
   (gptr*) &new_auto_increment_value,
   0, GET_ULL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's',
   "Only print errors. One can use two -s to make archive_reader very silent.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't',
   "Path for temporary files.",
   (gptr*) &opt_tmpdir,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V',
   "Print version and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void usage(void)
{
  print_version();
  puts("Copyright (C) 2007 MySQL AB");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\
       \nand you are welcome to modify and redistribute it under the GPL \
       license\n");
  puts("Read and modify Archive files directly\n");
  printf("Usage: %s [OPTIONS] file_to_be_looked_at\n", my_progname);
  print_defaults("my", load_default_groups);
  my_print_help(my_long_options);
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n", my_progname, SHOW_VERSION,
         MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
}

static void get_options(int *argc, char ***argv)
{
  load_defaults("my", load_default_groups, argc, argv);
  default_argv= *argv;

  handle_options(argc, argv, my_long_options, get_one_option);

  if (*argc == 0)
  {
    usage();
    exit(-1);
  }

  return;
} /* get options */
