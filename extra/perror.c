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

/* Return error-text for system error messages and nisam messages */

#define PERROR_VERSION "2.6"

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <errno.h>
#include <getopt.h>


static struct option long_options[] =
{
  {"help",       no_argument,        0, '?'},
  {"info",       no_argument,        0, 'I'},
  {"all",        no_argument,        0, 'a'},
  {"silent",	 no_argument,	     0, 's'},
  {"verbose",    no_argument,        0, 'v'},
  {"version",    no_argument,        0, 'V'},
  {0, 0, 0, 0}
};

typedef struct ha_errors {
  int errcode;
  const char *msg;
} HA_ERRORS;

static int verbose=1,print_all_codes=0;

static HA_ERRORS ha_errlist[]=
{
  { 120,"Didn't find key on read or update" },
  { 121,"Duplicate key on write or update" },
  { 123,"Someone has changed the row since it was read; Update with is recoverable" },
  { 124,"Wrong index given to function" },
  { 126,"Index file is crashed / Wrong file format" },
  { 127,"Record-file is crashed" },
  { 131,"Command not supported by database" },
  { 132,"Old database file" },
  { 133,"No record read before update" },
  { 134,"Record was already deleted (or record file crashed)" },
  { 135,"No more room in record file" },
  { 136,"No more room in index file" },
  { 137,"No more records (read after end of file)" },
  { 138,"Unsupported extension used for table" },
  { 139,"Too big row (>= 16 M)"},
  { 140,"Wrong create options"},
  { 141,"Duplicate unique key or constraint on write or update"},
  { 142,"Unknown character set used"},
  { 143,"Conflicting table definition between MERGE and mapped table"},
  { 144,"Table is crashed and last repair failed"},
  { 145,"Table was marked as crashed and should be repaired"},
  { 0,NullS },
};


static void print_version(void)
{
  printf("%s Ver %s, for %s (%s)\n",my_progname,PERROR_VERSION,
	 SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage(void)
{
  print_version();
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  printf("Print a description for a system error code or a error code from\na MyISAM/ISAM table handler\n");
  printf("Usage: %s [OPTIONS] [ERRORCODE [ERRORCODE...]]\n",my_progname);
  printf("\n\
   -?, --help     Displays this help and exits.\n\
   -I, --info     Synonym for the above.");
#ifdef HAVE_SYS_ERRLIST
  printf("\n\
   -a, --all      Print all the error messages and the number.");
#endif
  printf("\n\
   -s, --silent	  Only print the error message\n\
   -v, --verbose  Print error code and message (default).\n\
   -V, --version  Displays version information and exits.\n");
} 


static int get_options(int *argc,char ***argv)
{
  int c,option_index;

  while ((c=getopt_long(*argc,*argv,"asvVI?",long_options,
			&option_index)) != EOF)
  {
      switch (c) {
#ifdef HAVE_SYS_ERRLIST
      case 'a':
	print_all_codes=1;
	break;
#endif
      case 'v':
      	verbose=1;
      	break;
      case 's':
	verbose=0;
	break;
      case 'V':
	print_version();
	exit(0);
	break;
      case 'I':
      case '?':
	usage();
	exit(0);
	break;
      default:
	fprintf(stderr,"%s: Illegal option character '%c'\n",
		my_progname,opterr);
	return(1);
	break;
      }
  }
  (*argc)-=optind;
  (*argv)+=optind;
  if (!*argc && !print_all_codes)
  {
    usage();
    return 1;
  }
  return 0;
} /* get_options */


static const char *get_ha_error_msg(int code)
{
  HA_ERRORS *ha_err_ptr;

  for (ha_err_ptr=ha_errlist ; ha_err_ptr->errcode ;ha_err_ptr++)
    if (ha_err_ptr->errcode == code)
      return ha_err_ptr->msg;
  return NullS;
}


int main(int argc,char *argv[])
{
  int error,code,found;
  const char *msg;
  MY_INIT(argv[0]);

  if (get_options(&argc,&argv))
    exit(1);

  error=0;
#ifdef HAVE_SYS_ERRLIST
  if (print_all_codes)
  {
    HA_ERRORS *ha_err_ptr;
    for (code=1 ; code < sys_nerr ; code++)
    {
      if (sys_errlist[code][0])
      {						/* Skipp if no error-text */
	printf("%3d = %s\n",code,sys_errlist[code]);
      }
    }
    for (ha_err_ptr=ha_errlist ; ha_err_ptr->errcode ;ha_err_ptr++)
      printf("%3d = %s\n",ha_err_ptr->errcode,ha_err_ptr->msg);
  }
  else
#endif
  {
    for ( ; argc-- > 0 ; argv++)
    {
      found=0;
      code=atoi(*argv);
      msg = strerror(code);
      if (msg)
      {
	found=1;
	if (verbose)
	  printf("Error code %3d:  %s\n",code,msg);
	else
	  puts(msg);
      }
      if (!(msg=get_ha_error_msg(code)))
      {
	if (!found)
	{
	  fprintf(stderr,"Illegal error code: %d\n",code);
	  error=1;
	}
      }
      else
      {
	if (verbose)
	  printf("%3d = %s\n",code,msg);
	else
	  puts(msg);
      }
    }
  }
  exit(error);
  return error;
}

