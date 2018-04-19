/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <ndb_opts.h>

#include "my_alloc.h"

// copied from mysql.cc to get readline
extern "C" {
#if defined(_WIN32)
#include <conio.h>
#elif !defined(__NETWARE__)
#include <readline.h>
extern "C" int add_history(const char *command); /* From readline directory */
extern "C" int read_history(const char *command);
extern "C" int write_history(const char *command);
#define HAVE_READLINE
#endif
}

#include <BaseString.hpp>
#include <NdbOut.hpp>
#include <mgmapi.h>
#include <ndb_version.h>

#include "ndb_mgmclient.hpp"

const char *load_default_groups[]= { "mysql_cluster","ndb_mgm",0 };

static char *opt_execute_str= 0;
static char *opt_prompt= 0;
static unsigned opt_verbose = 1;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_mgm"),
  { "execute", 'e',
    "execute command and exit", 
    (uchar**) &opt_execute_str, (uchar**) &opt_execute_str, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "prompt", 'p',
    "Set prompt to string specified",
    (uchar**) &opt_prompt, (uchar**) &opt_prompt, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "verbose", 'v',
    "Control the amount of printout",
    (uchar**) &opt_verbose, (uchar**) &opt_verbose, 0,
    GET_UINT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0}, \
  {"try-reconnect", 't', \
    "Same as --connect-retries", \
    (uchar**) &opt_connect_retries, (uchar**) &opt_connect_retries, 0, \
    GET_INT, REQUIRED_ARG, 12, 0, INT_MAX, 0, 0, 0 }, \
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub("[hostname [port]]");
}

static bool
read_and_execute(Ndb_mgmclient* com, int try_reconnect)
{
  static char *line_read = (char *)NULL;

  /* If the buffer has already been allocated, return the memory
     to the free pool. */
  if (line_read)
  {
    free (line_read);
    line_read = (char *)NULL;
  }
#ifdef HAVE_READLINE
  /* Get a line from the user. */
  line_read = readline (com->get_current_prompt());
  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);
#else
  static char linebuffer[254];
  fputs(com->get_current_prompt(), stdout);
  linebuffer[sizeof(linebuffer)-1]=0;
  line_read = fgets(linebuffer, sizeof(linebuffer)-1, stdin);
  if (line_read == linebuffer) {
    char *q=linebuffer;
    while (*q > 31) q++;
    *q=0;
    line_read= strdup(linebuffer);
  }
#endif
  return com->execute(line_read, try_reconnect, 1);
}

int main(int argc, char** argv){
  Ndb_opts opts(argc, argv, my_long_options, load_default_groups);
  opts.set_usage_funcs(short_usage_sub);

  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_mgm.trace";
#endif
  if ((ho_error=opts.handle_options()))
    exit(ho_error);

  BaseString connect_str(opt_ndb_connectstring);
  if(argc == 1) {
    connect_str.assfmt("%s", argv[0]);
  } else if (argc >= 2) {
    connect_str.assfmt("%s:%s", argv[0], argv[1]);
  }

  if (!isatty(0) || opt_execute_str)
  {
    opt_prompt= 0;
  }

  Ndb_mgmclient* com = new Ndb_mgmclient(connect_str.c_str(),
                                         "ndb_mgm> ",
                                         opt_verbose,
                                         opt_connect_retry_delay);
  if(opt_prompt)
  {
    /* Construct argument to be sent to execute function */
    BaseString prompt_args("prompt ");
    prompt_args.append(opt_prompt);
    com->execute(prompt_args.c_str(), opt_connect_retries, 0);
  }
  int ret= 0;
  BaseString histfile;
  if (!opt_execute_str)
  {
#ifdef HAVE_READLINE
    char *histfile_env= getenv("NDB_MGM_HISTFILE");
    if (histfile_env)
      histfile.assign(histfile_env,strlen(histfile_env));
    else if(getenv("HOME"))
    {
      histfile.assign(getenv("HOME"),strlen(getenv("HOME")));
      histfile.append("/.ndb_mgm_history");
    }
    if (histfile.length())
      read_history(histfile.c_str());
#endif

    ndbout << "-- NDB Cluster -- Management Client --" << endl;
    while(read_and_execute(com, opt_connect_retries))
      ;

#ifdef HAVE_READLINE
    if (histfile.length())
    {
      BaseString histfile_tmp;
      histfile_tmp.assign(histfile);
      histfile_tmp.append(".TMP");
      if(!write_history(histfile_tmp.c_str()))
        my_rename(histfile_tmp.c_str(), histfile.c_str(), MYF(MY_WME));
    }
#endif
  }
  else
  {
    com->execute(opt_execute_str, opt_connect_retries, 0, &ret);
  }
  delete com;
  ndb_end(opt_ndb_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);

  // Don't allow negative return code
  if (ret < 0)
    ret = 255;
  return ret;
}

