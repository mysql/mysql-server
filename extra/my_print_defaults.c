/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
**  print_default.c:
**  Print all parameters in a default file that will be given to some program.
**
**  Written by Monty
*/

#include <global.h>
#include <my_sys.h>
#include <getopt.h>

const char *config_file="my";			/* Default config file */

static struct option long_options[] =
{
  {"config-file",	required_argument, 0,	'c'},
  {"defaults-file",	required_argument, 0,	'c'},
  {"no-defaults",	no_argument,	   0,	'd'},
  {"help",		no_argument,	   0,	'?'},
  {"version",		no_argument,	   0,	'V'},
  {0, 0, 0, 0}
};

static void usage(my_bool version)
{
  printf("%s  Ver 1.1 for %s at %s\n",my_progname,SYSTEM_TYPE,
	 MACHINE_TYPE);
  if (version)
    return;
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Prints all arguments that is give to some program using the default files");
  printf("Usage: %s [OPTIONS] groups\n",my_progname);
  printf("\n\
  -c, --config-file=# --defaults-file=#\n\
	                The config file to use (default '%s')\n\
  --no-defaults		Return an empty string (useful for scripts)\n\
  -?, --help		Display this help message and exit.\n\
  -V, --version		Output version information and exit.\n",
	 config_file);
  printf("\nExample usage: %s --config-file=my client mysql\n",my_progname);
}

static int get_options(int *argc,char ***argv)
{
  int c,option_index;

  while ((c=getopt_long(*argc,*argv,"c:V?I",
			long_options, &option_index)) != EOF)
  {
    switch (c) {
    case 'c':
      config_file=optarg;
      break;
    case 'n':
      exit(0);
    case 'I':
    case '?':
      usage(0);
      exit(0);
    case 'V':
      usage(1);
      exit(0);
    }
  }
  (*argc)-=optind;
  (*argv)+=optind;
  if (*argc < 1)
  {
    usage(0);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  int count;
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
  load_defaults(config_file, (const char **) load_default_groups,
		&count, &arguments);

  for (argument= arguments+1 ; *argument ; argument++)
    puts(*argument);
  my_free((char*) load_default_groups,MYF(0));
  free_defaults(arguments);

  exit(0);
}
