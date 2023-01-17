/*
   Copyright (c) 2017, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Return error-text for NDB error messages in same
  fashion as "perror --ndb <error>"
*/

#include "ndb_global.h"
#include "ndb_opts.h"

#include "ndbapi/ndberror.h"
#include "mgmapi/ndbd_exit_codes.h"
#include "mgmapi/mgmapi_error.h"
#include "my_alloc.h"

static bool opt_verbose;
static bool opt_silent; // Overrides verbose and sets it to 0

static struct my_option my_long_options[] =
{
  NdbStdOpt::help,
  {"ndb", NDB_OPT_NOSHORT,
   "For command line compatibility with 'perror --ndb', ignored.",
   nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG,
   0, 0, 0, nullptr, 0, nullptr },
  {"silent", 's', "Only print the error message.",
   &opt_silent, nullptr, nullptr, GET_BOOL, NO_ARG,
   0, 0, 0, nullptr, 0, nullptr },
  {"verbose", 'v', "Print error code and message (default).",
   &opt_verbose, nullptr, nullptr, GET_BOOL, NO_ARG,
   1, 0, 0, nullptr, 0, nullptr },
  NdbStdOpt::version,
  NdbStdOpt::end_of_options
};

const char *load_default_groups[] = { 0 };

static void short_usage_sub(void)
{
  ndb_short_usage_sub("[ERRORCODE [ERRORCODE...]]");
}


static
int mgmapi_error_string(int err_no, char *str, int size)
{
  for (int i = 0; i < ndb_mgm_noOfErrorMsgs; i++)
  {
    if ((int)ndb_mgm_error_msgs[i].code == err_no)
    {
      snprintf(str, size - 1, "%s", ndb_mgm_error_msgs[i].msg);
      str[size - 1] = '\0';
      return 1; // Found a message
    }
  }
  return -1;
}


// Forward declare function from ndbd_exit_codes.cc which is not
// declared in any header
int ndbd_exit_string(int err_no, char *str, unsigned int size);


int main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options, load_default_groups);
  opts.set_usage_funcs(short_usage_sub);

  if (opts.handle_options() != 0)
    exit(255);

  if (opt_silent)
  {
    // --silent overrides any verbose setting
    opt_verbose = 0;
  }

  if (!argc)
  {
    opts.usage();
    exit(1);
  }

  int error = 0;
  for ( ; argc-- > 0 ; argv++)
  {
    int code=atoi(*argv);

    char error_string[1024];
    if ((ndb_error_string(code, error_string, sizeof(error_string)) > 0) ||
        (ndbd_exit_string(code, error_string, sizeof(error_string)) > 0) ||
        (mgmapi_error_string(code, error_string, sizeof(error_string)) > 0))
    {
      if (opt_verbose)
        printf("NDB error code %3d: %s\n", code, error_string);
      else
        puts(error_string);
    }
    else
    {
      fprintf(stderr, "Illegal ndb error code: %d\n", code);
      error= 1;
    }
  }
      
  exit(error);
  return error;
}
