/* Copyright (C) 2000 MySQL AB

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

/*
**  print_default.c:
**  Print all parameters in a default file that will be given to some program.
**
**  Written by Monty
*/

#include <my_global.h>
#include <my_sys.h>
#include <my_getopt.h>

const char *config_file="my";			/* Default config file */
uint verbose= 0, opt_defaults_file_used= 0;

static struct my_option my_long_options[] =
{
  {"config-file", 'c', "The config file to be used.",
   (gptr*) &config_file, (gptr*) &config_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"defaults-file", 'c', "Synonym for --config-file.",
   (gptr*) &config_file, (gptr*) &config_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"defaults-extra-file", 'e',
   "Read this file after the global /etc config file and before the config file in the users home directory.",
   (gptr*) &defaults_extra_file, (gptr*) &defaults_extra_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"extra-file", 'e',
   "Synonym for --defaults-extra-file.",
   (gptr*) &defaults_extra_file, (gptr*) &defaults_extra_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"no-defaults", 'n', "Return an empty string (useful for scripts).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
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
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Prints all arguments that is give to some program using the default files");
  printf("Usage: %s [OPTIONS] groups\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
  printf("\nExample usage:\n%s --config-file=my client mysql\n", my_progname);
}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch (optid) {
    case 'c':
      opt_defaults_file_used= 1;
      break;
    case 'n':
    exit(0);
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
  int count, error;
  char **load_default_groups, *tmp_arguments[2],
       **argument, **arguments;
  MY_INIT(argv[0]);

  /*
  ** Check out the args
  */
  if (get_options(&argc,&argv))
    exit(1);
  if (!(load_default_groups=(char**) my_malloc((argc+2)*sizeof(char*),
					       MYF(MY_WME))))
    exit(1);

  for (count=0; *argv ; argv++,count++)
    load_default_groups[count]= *argv;
  load_default_groups[count]=0;

  count=1;
  arguments=tmp_arguments;
  arguments[0]=my_progname;
  arguments[1]=0;
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
  }

  for (argument= arguments+1 ; *argument ; argument++)
    puts(*argument);
  my_free((char*) load_default_groups,MYF(0));
  free_defaults(arguments);

  exit(error);
}
