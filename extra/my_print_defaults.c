
/*
   Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
**  print_default.c:
**  Print all parameters in a default file that will be given to some program.
**
**  Written by Monty
*/

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <my_getopt.h>
#include "my_default.h"
#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */


const char *config_file="my";			/* Default config file */
static char *my_login_path;
static my_bool *show_passwords;
uint verbose= 0, opt_defaults_file_used= 0;
const char *default_dbug_option="d:t:o,/tmp/my_print_defaults.trace";

static struct my_option my_long_options[] =
{
  /*
    NB: --config-file is troublesome, because get_defaults_options() doesn't
    know about it, but we pretend --config-file is like --defaults-file.  In
    fact they behave differently: see the comments at the top of
    mysys/default.c for how --defaults-file should behave.

    This --config-file option behaves as:
    - If it has a directory name part (absolute or relative), then only this
      file is read; no error is given if the file doesn't exist
    - If the file has no directory name part, the standard locations are
      searched for a file of this name (and standard filename extensions are
      added if the file has no extension)
  */
  {"config-file", 'c', "Deprecated, please use --defaults-file instead. "
   "Name of config file to read; if no extension is given, default "
   "extension (e.g., .ini or .cnf) will be added",
   &config_file, &config_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit",
   0,0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log", &default_dbug_option,
   &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"defaults-file", 'c', "Like --config-file, except: if first option, "
   "then read this file only, do not read global or per-user config "
   "files; should be the first option",
   &config_file, &config_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"defaults-extra-file", 'e',
   "Read this file after the global config file and before the config "
   "file in the users home directory; should be the first option",
   &my_defaults_extra_file, &my_defaults_extra_file, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"defaults-group-suffix", 'g',
   "In addition to the given groups, read also groups with this suffix",
   &my_defaults_group_suffix, &my_defaults_group_suffix,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"extra-file", 'e',
   "Deprecated. Synonym for --defaults-extra-file.",
   &my_defaults_extra_file,
   &my_defaults_extra_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"no-defaults", 'n', "Ignore reading of default option file(s), "
   "except for login file.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"login-path", 'l', "Path to be read from under the login file.",
   &my_login_path, &my_login_path, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"show", 's', "Show passwords in plain text.",
   &show_passwords, &show_passwords, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this help message and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Increase the output level",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void usage(my_bool version)
{
  printf("%s  Ver 1.6 for %s at %s\n",my_progname,SYSTEM_TYPE,
	 MACHINE_TYPE);
  if (version)
    return;
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  puts("Prints all arguments that is give to some program using the default files");
  printf("Usage: %s [OPTIONS] groups\n", my_progname);
  my_print_help(my_long_options);
  my_print_default_files(config_file);
  my_print_variables(my_long_options);
  printf("\nExample usage:\n%s --defaults-file=example.cnf client mysql\n", my_progname);
}


static my_bool
get_one_option(int optid, const struct my_option *opt MY_ATTRIBUTE((unused)),
	       char *argument MY_ATTRIBUTE((unused)))
{
  switch (optid) {
    case 'c':
      opt_defaults_file_used= 1;
      break;
    case 'n':
      break;
    case 'I':
    case '?':
    usage(0);
    exit(0);
    case 'v':
      verbose++;
      break;
    case 'V':
    usage(1);
    exit(0);
    case '#':
      DBUG_PUSH(argument ? argument : default_dbug_option);
      break;
  }
  return 0;
}


static int get_options(int *argc,char ***argv)
{
  int ho_error;

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (*argc < 1)
  {
    usage(0);
    return 1;
  }
  return 0;
}


int main(int argc, char **argv)
{
  int count, error, args_used;
  char **load_default_groups, *tmp_arguments[6];
  char **argument, **arguments, **org_argv;
  char *defaults, *extra_defaults, *group_suffix, *login_path;

  MY_INIT(argv[0]);

  org_argv= argv;
  args_used= get_defaults_options(argc, argv, &defaults, &extra_defaults,
                                  &group_suffix, &login_path, FALSE);

  /* Copy defaults-xxx arguments & program name */
  count=args_used+1;
  arguments= tmp_arguments;
  memcpy((char*) arguments, (char*) org_argv, count * sizeof(*org_argv));
  arguments[count]= 0;

  /* Check out the args */
  if (!(load_default_groups=(char**) my_malloc(PSI_NOT_INSTRUMENTED,
                                               (argc+1)*sizeof(char*),
					       MYF(MY_WME))))
    exit(1);
  if (get_options(&argc,&argv))
    exit(1);
  memcpy((char*) load_default_groups, (char*) argv, (argc + 1) * sizeof(*argv));

  if ((error= load_defaults(config_file, (const char **) load_default_groups,
			   &count, &arguments)))
  {
    if (verbose && opt_defaults_file_used)
    {
      if (error == 1)
	fprintf(stderr, "WARNING: Defaults file '%s' not found!\n",
		config_file);
      /* This error is not available now. For the future */
      if (error == 2)
	fprintf(stderr, "WARNING: Defaults file '%s' is not a regular file!\n",
		config_file);
    }
    error= 2;
    exit(error);
  }

  for (argument= arguments+1 ; *argument ; argument++)
    if (!my_getopt_is_args_separator(*argument))           /* skip arguments separator */
    {
      if (!(show_passwords) && strncmp(*argument, "--password", 10) == 0)
        puts("--password=*****");
      else
        puts(*argument);
    }
  my_free(load_default_groups);
  free_defaults(arguments);

  exit(0);
}
