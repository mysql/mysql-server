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

/* Return error-text for system error messages and nisam messages */

#define PERROR_VERSION "2.10"

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <errno.h>
#include <my_getopt.h>
#ifdef HAVE_NDBCLUSTER_DB
#include "../ndb/src/ndbapi/ndberror.c"
#include "../ndb/src/kernel/error/ndbd_exit_codes.c"
#endif

static my_bool verbose, print_all_codes;

#ifdef HAVE_NDBCLUSTER_DB
static my_bool ndb_code;
static char ndb_string[1024];
#endif

static struct my_option my_long_options[] =
{
  {"help", '?', "Displays this help and exits.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"info", 'I', "Synonym for --help.",  0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_NDBCLUSTER_DB
  {"ndb", 257, "Ndbcluster storage engine specific error codes.",  (gptr*) &ndb_code,
   (gptr*) &ndb_code, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
#ifdef HAVE_SYS_ERRLIST
  {"all", 'a', "Print all the error messages and the number.",
   (gptr*) &print_all_codes, (gptr*) &print_all_codes, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#endif
  {"silent", 's', "Only print the error message.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Print error code and message (default).", (gptr*) &verbose,
   (gptr*) &verbose, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"version", 'V', "Displays version information and exits.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


typedef struct ha_errors {
  int errcode;
  const char *msg;
} HA_ERRORS;


static HA_ERRORS ha_errlist[]=
{
  { 120,"Didn't find key on read or update" },
  { 121,"Duplicate key on write or update" },
  { 123,"Someone has changed the row since it was read (while the table was locked to prevent it)" },
  { 124,"Wrong index given to function" },
  { 126,"Index file is crashed" },
  { 127,"Record-file is crashed" },
  { 128,"Out of memory" },
  { 130,"Incorrect file format" },
  { 131,"Command not supported by database" },
  { 132,"Old database file" },
  { 133,"No record read before update" },
  { 134,"Record was already deleted (or record file crashed)" },
  { 135,"No more room in record file" },
  { 136,"No more room in index file" },
  { 137,"No more records (read after end of file)" },
  { 138,"Unsupported extension used for table" },
  { 139,"Too big row"},
  { 140,"Wrong create options"},
  { 141,"Duplicate unique key or constraint on write or update"},
  { 142,"Unknown character set used"},
  { 143,"Conflicting table definitions in sub-tables of MERGE table"},
  { 144,"Table is crashed and last repair failed"},
  { 145,"Table was marked as crashed and should be repaired"},
  { 146,"Lock timed out; Retry transaction"},
  { 147,"Lock table is full;  Restart program with a larger locktable"},
  { 148,"Updates are not allowed under a read only transactions"},
  { 149,"Lock deadlock; Retry transaction"},
  { 150,"Foreign key constraint is incorrectly formed"},
  { 151,"Cannot add a child row"},
  { 152,"Cannot delete a parent row"},
  { -30999, "DB_INCOMPLETE: Sync didn't finish"},
  { -30998, "DB_KEYEMPTY: Key/data deleted or never created"},
  { -30997, "DB_KEYEXIST: The key/data pair already exists"},
  { -30996, "DB_LOCK_DEADLOCK: Deadlock"},
  { -30995, "DB_LOCK_NOTGRANTED: Lock unavailable"},
  { -30994, "DB_NOSERVER: Server panic return"},
  { -30993, "DB_NOSERVER_HOME: Bad home sent to server"},
  { -30992, "DB_NOSERVER_ID: Bad ID sent to server"},
  { -30991, "DB_NOTFOUND: Key/data pair not found (EOF)"},
  { -30990, "DB_OLD_VERSION: Out-of-date version"},
  { -30989, "DB_RUNRECOVERY: Panic return"},
  { -30988, "DB_VERIFY_BAD: Verify failed; bad format"},
  { 0,NullS },
};


#include <help_start.h>

static void print_version(void)
{
  printf("%s Ver %s, for %s (%s)\n",my_progname,PERROR_VERSION,
	 SYSTEM_TYPE,MACHINE_TYPE);
}


static void usage(void)
{
  print_version();
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  printf("Print a description for a system error code or an error code from\na MyISAM/ISAM/BDB table handler.\n");
  printf("If you want to get the error for a negative error code, you should use\n-- before the first error code to tell perror that there was no more options.\n\n");
  printf("Usage: %s [OPTIONS] [ERRORCODE [ERRORCODE...]]\n",my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

#include <help_end.h>


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch (optid) {
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
  }
  return 0;
}


static int get_options(int *argc,char ***argv)
{
  int ho_error;

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

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
  char *unknown_error = 0;
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
      {						/* Skip if no error-text */
	printf("%3d = %s\n",code,sys_errlist[code]);
      }
    }
    for (ha_err_ptr=ha_errlist ; ha_err_ptr->errcode ;ha_err_ptr++)
      printf("%3d = %s\n",ha_err_ptr->errcode,ha_err_ptr->msg);
  }
  else
#endif
  {
    /*
      On some system, like NETWARE, strerror(unknown_error) returns a
      string 'Unknown Error'.  To avoid printing it we try to find the
      error string by asking for an impossible big error message.

      On Solaris 2.8 it might return NULL
    */
    if ((msg= strerror(10000)) == NULL)
      msg= "Unknown Error";

    /*
      Allocate a buffer for unknown_error since strerror always returns
      the same pointer on some platforms such as Windows
    */
    unknown_error= malloc(strlen(msg)+1);
    strmov(unknown_error, msg);

    for ( ; argc-- > 0 ; argv++)
    {

      found=0;
      code=atoi(*argv);
#ifdef HAVE_NDBCLUSTER_DB
      if (ndb_code)
      {
	if ((ndb_error_string(code, ndb_string, sizeof(ndb_string)) < 0) &&
	    (ndbd_exit_string(code, ndb_string, sizeof(ndb_string)) < 0))
	{
          msg= 0;
	}
	else
	  msg= ndb_string;
        if (msg)
        {
          if (verbose)
            printf("NDB error code %3d: %s\n",code,msg);
          else
            puts(msg);
        }
        else
        {
	  fprintf(stderr,"Illegal ndb error code: %d\n",code);
          error= 1;
        }
        found= 1;
        msg= 0;
      }
      else 
#endif
	msg = strerror(code);

      /*
        We don't print the OS error message if it is the same as the
        unknown_error message we retrieved above, or it starts with
        'Unknown Error' (without regard to case).
      */
      if (msg &&
          my_strnncoll(&my_charset_latin1, (const uchar*) msg, 13,
                       (const uchar*) "Unknown Error", 13) &&
          (!unknown_error || strcmp(msg, unknown_error)))
      {
	found=1;
	if (verbose)
	  printf("OS error code %3d:  %s\n",code,msg);
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
	  printf("MySQL error code %3d: %s\n",code,msg);
	else
	  puts(msg);
      }
    }
  }

  /* if we allocated a buffer for unknown_error, free it now */
  if (unknown_error)
    free(unknown_error);

  exit(error);
  return error;
}
