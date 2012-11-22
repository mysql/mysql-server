/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Return error-text for system error messages and handler messages */

#define PERROR_VERSION "2.11"

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <errno.h>
#include <my_getopt.h>
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
#include "../storage/ndb/src/ndbapi/ndberror.c"
#include "../storage/ndb/src/kernel/error/ndbd_exit_codes.c"
#include "../storage/ndb/include/mgmapi/mgmapi_error.h"
#endif
#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

static my_bool verbose, print_all_codes;

#include "../include/my_base.h"
#include "../mysys/my_handler_errors.h"
// #include "../include/my_compare.h"

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
static my_bool ndb_code;
static char ndb_string[1024];
int mgmapi_error_string(int err_no, char *str, int size)
{
  int i;
  for (i= 0; i < ndb_mgm_noOfErrorMsgs; i++)
  {
    if ((int)ndb_mgm_error_msgs[i].code == err_no)
    {
      my_snprintf(str, size-1, "%s", ndb_mgm_error_msgs[i].msg);
      str[size-1]= '\0';
      return 0;
    }
  }
  return -1;
}
#endif

static struct my_option my_long_options[] =
{
  {"help", '?', "Displays this help and exits.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"info", 'I', "Synonym for --help.",  0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
  {"ndb", 257, "Ndbcluster storage engine specific error codes.", &ndb_code,
   &ndb_code, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
#ifdef HAVE_SYS_ERRLIST
  {"all", 'a', "Print all the error messages and the number. Deprecated,"
   " will be removed in a future release.",
   &print_all_codes, &print_all_codes, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#endif
  {"silent", 's', "Only print the error message.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Print error code and message (default).", &verbose,
   &verbose, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
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


static void print_version(void)
{
  printf("%s Ver %s, for %s (%s)\n",my_progname,PERROR_VERSION,
	 SYSTEM_TYPE,MACHINE_TYPE);
}


static void usage(void)
{
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  printf("Print a description for a system error code or a MySQL error code.\n");
  printf("If you want to get the error for a negative error code, you should use\n-- before the first error code to tell perror that there was no more options.\n\n");
  printf("Usage: %s [OPTIONS] [ERRORCODE [ERRORCODE...]]\n",my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}


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

  /*
    If you got compilation error here about compile_time_assert array, check
    that every HA_ERR_xxx constant has a corresponding error message in
    handler_error_messages[] list (check mysys/ma_handler_errors.h and
    include/my_base.h).
  */
  compile_time_assert(HA_ERR_FIRST + array_elements(handler_error_messages) ==
                      HA_ERR_LAST + 1);
  if (code >= HA_ERR_FIRST && code <= HA_ERR_LAST)
    return handler_error_messages[code - HA_ERR_FIRST];

  for (ha_err_ptr=ha_errlist ; ha_err_ptr->errcode ;ha_err_ptr++)
    if (ha_err_ptr->errcode == code)
      return ha_err_ptr->msg;
  return NullS;
}

typedef struct
{
  const char *name;
  uint        code;
  const char *text;
} st_error;

static st_error global_error_names[] =
{
#include <mysqld_ername.h>
  { 0, 0, 0 }
};

/**
  Lookup an error by code in the global_error_names array.
  @param code the code to lookup
  @param [out] name_ptr the error name, when found
  @param [out] msg_ptr the error text, when found
  @return 1 when found, otherwise 0
*/
int get_ER_error_msg(uint code, const char **name_ptr, const char **msg_ptr)
{
  st_error *tmp_error;

  tmp_error= & global_error_names[0];

  while (tmp_error->name != NULL)
  {
    if (tmp_error->code == code)
    {
      *name_ptr= tmp_error->name;
      *msg_ptr= tmp_error->text;
      return 1;
    }
    tmp_error++;
  }

  return 0;
}

#if defined(__WIN__)
static my_bool print_win_error_msg(DWORD error, my_bool verbose)
{
  LPTSTR s;
  if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL, error, 0, (LPTSTR)&s, 0,
                    NULL))
  {
    if (verbose)
      printf("Win32 error code %d: %s", error, s);
    else
      puts(s);
    LocalFree(s);
    return 0;
  }
  return 1;
}
#endif

/*
  Register handler error messages for usage with my_error()

  NOTES
    This is safe to call multiple times as my_error_register()
    will ignore calls to register already registered error numbers.
*/

static const char **get_handler_error_messages()
{
  return handler_error_messages;
}

void my_handler_error_register(void)
{
  /*
    If you got compilation error here about compile_time_assert array, check
    that every HA_ERR_xxx constant has a corresponding error message in
    handler_error_messages[] list (check mysys/ma_handler_errors.h and
    include/my_base.h).
  */
  compile_time_assert(HA_ERR_FIRST + array_elements(handler_error_messages) ==
                      HA_ERR_LAST + 1);
  my_error_register(get_handler_error_messages, HA_ERR_FIRST,
                    HA_ERR_FIRST+ array_elements(handler_error_messages)-1);
}


void my_handler_error_unregister(void)
{
  my_error_unregister(HA_ERR_FIRST,
                      HA_ERR_FIRST+ array_elements(handler_error_messages)-1);
}

int main(int argc,char *argv[])
{
  int error,code,found;
  const char *msg;
  const char *name;
  char *unknown_error = 0;
#if defined(__WIN__)
  my_bool skip_win_message= 0;
#endif
  MY_INIT(argv[0]);

  if (get_options(&argc,&argv))
    exit(1);

  my_handler_error_register();

  error=0;
#ifdef HAVE_SYS_ERRLIST
  if (print_all_codes)
  {
    HA_ERRORS *ha_err_ptr;
    printf("WARNING: option '-a/--all' is deprecated and will be removed in a"
           " future release.\n");
    for (code=1 ; code < sys_nerr ; code++)
    {
      if (sys_errlist[code] && sys_errlist[code][0])
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
      On some system, like Linux, strerror(unknown_error) returns a
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
#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
      if (ndb_code)
      {
        if ((ndb_error_string(code, ndb_string, sizeof(ndb_string)) < 0) &&
            (ndbd_exit_string(code, ndb_string, sizeof(ndb_string)) < 0) &&
            (mgmapi_error_string(code, ndb_string, sizeof(ndb_string)) < 0))
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
	found= 1;
	if (verbose)
	  printf("OS error code %3d:  %s\n", code, msg);
	else
	  puts(msg);
      }
      if ((msg= get_ha_error_msg(code)))
      {
        found= 1;
        if (verbose)
          printf("MySQL error code %3d: %s\n", code, msg);
        else
          puts(msg);
      }
      if (get_ER_error_msg(code, & name, & msg))
      {
        found= 1;
        if (verbose)
          printf("MySQL error code %3d (%s): %s\n", code, name, msg);
        else
          puts(msg);
      }
      if (!found)
      {
#if defined(__WIN__)
        if (!(skip_win_message= !print_win_error_msg((DWORD)code, verbose)))
        {
#endif
          fprintf(stderr,"Illegal error code: %d\n",code);
          error=1;
#if defined(__WIN__)
        }
#endif
      }
#if defined(__WIN__)
      if (!skip_win_message)
        print_win_error_msg((DWORD)code, verbose);
#endif
    }
  }

  /* if we allocated a buffer for unknown_error, free it now */
  if (unknown_error)
    free(unknown_error);

  my_handler_error_unregister();
  my_end(0);
  exit(error);
  return error;
}
