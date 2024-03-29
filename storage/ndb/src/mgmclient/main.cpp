/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
#include "my_getopt.h"

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

static ndb_password_state opt_backup_password_state("backup", nullptr);
static ndb_password_option opt_backup_password(opt_backup_password_state);
static ndb_password_from_stdin_option opt_backup_password_from_stdin(
                                          opt_backup_password_state);

static bool opt_encrypt_backup = false;

static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,
  NdbStdOpt::ndb_connectstring,
  NdbStdOpt::mgmd_host,
  NdbStdOpt::connectstring,
  NdbStdOpt::ndb_nodeid,
  NdbStdOpt::connect_retry_delay,
  NdbStdOpt::connect_retries,
  NDB_STD_OPT_DEBUG
  { "backup-password", NDB_OPT_NOSHORT, "Encryption password for backup file",
    nullptr, nullptr, nullptr, GET_PASSWORD, OPT_ARG,
    0, 0, 0, nullptr, 0, &opt_backup_password},
  { "backup-password-from-stdin", NDB_OPT_NOSHORT,
    "Encryption password for backup file",
    &opt_backup_password_from_stdin.opt_value, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, &opt_backup_password_from_stdin},
  { "encrypt-backup", NDB_OPT_NOSHORT,
    "Treat START BACKUP as START BACKUP ENCRYPT",
    &opt_encrypt_backup, nullptr, nullptr, GET_BOOL, NO_ARG,
    0, 0, 0, nullptr, 0, nullptr},
  { "execute", 'e', "execute command and exit",
    &opt_execute_str, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr},
  { "prompt", 'p', "Set prompt to string specified",
    &opt_prompt, nullptr, nullptr, GET_STR, REQUIRED_ARG,
    0, 0, 0, nullptr, 0, nullptr},
  { "verbose", 'v', "Control the amount of printout",
    &opt_verbose, nullptr, nullptr, GET_UINT, REQUIRED_ARG,
    1, 0, 0, nullptr, 0, nullptr},
  {"try-reconnect", 't', "Same as --connect-retries",
    &opt_connect_retries, nullptr, nullptr, GET_INT, REQUIRED_ARG,
    12, 0, INT_MAX, nullptr, 0, nullptr},
  NdbStdOpt::end_of_options
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
  static char linebuffer[512];
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
  NDB_INIT(argv[0]);

  Ndb_opts opts(argc, argv, my_long_options, load_default_groups);
  opts.set_usage_funcs(short_usage_sub);

  int ho_error;
#ifndef NDEBUG
  opt_debug= "d:t:O,/tmp/ndb_mgm.trace";
#endif
  if ((ho_error = opts.handle_options()))
    exit(ho_error);

  if (ndb_option::post_process_options())
  {
    BaseString err_msg = opt_backup_password_state.get_error_message();
    if (!err_msg.empty())
    {
      fprintf(stderr, "Error: backup password: %s\n", err_msg.c_str());
    }
    exit(2);
  }

  BaseString connect_str(opt_ndb_connectstring);
  if(argc == 1) {
    connect_str.assfmt("%s", argv[0]);
  } else if (argc >= 2) {
    connect_str.assfmt("%s %s", argv[0], argv[1]);
  }

  if (!isatty(0) || opt_execute_str)
  {
    opt_prompt= 0;
  }

  Ndb_mgmclient* com = new Ndb_mgmclient(connect_str.c_str(),
                                         "ndb_mgm> ",
                                         opt_verbose,
                                         opt_connect_retry_delay);
  com->set_always_encrypt_backup(opt_encrypt_backup);
  if (opt_backup_password_state.get_password())
  {
    if (com->set_default_backup_password(
                 opt_backup_password_state.get_password()) == 1)
    {
      fprintf(stderr, "Error: Failed set default backup password.\n");
      delete com;
      ndb_end(opt_ndb_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
      return 2;
    }
  }

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

