/* Copyright (C) 2003 MySQL AB

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

#include <ndb_global.h>
#include <ndb_opts.h>

// copied from mysql.cc to get readline
extern "C" {
#if defined( __WIN__)
#include <conio.h>
#elif !defined(__NETWARE__)
#include <readline/readline.h>
extern "C" int add_history(const char *command); /* From readline directory */
#define HAVE_READLINE
#endif
}

#include <NdbMain.h>
#include <NdbHost.h>
#include <BaseString.hpp>
#include <NdbOut.hpp>
#include <mgmapi.h>
#include <ndb_version.h>

#include "ndb_mgmclient.hpp"

const char *progname = "ndb_mgm";


static Ndb_mgmclient* com;

extern "C"
void 
handler(int sig)
{
  DBUG_ENTER("handler");
  switch(sig){
  case SIGPIPE:
    /**
     * Will happen when connection to mgmsrv is broken
     * Reset connected flag
     */
    com->disconnect();    
    break;
  }
  DBUG_VOID_RETURN;
}

NDB_STD_OPTS_VARS;

static const char default_prompt[]= "ndb_mgm> ";
static unsigned _try_reconnect;
static const char *prompt= default_prompt;
static char *opt_execute_str= 0;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_mgm"),
  { "execute", 'e',
    "execute command and exit", 
    (gptr*) &opt_execute_str, (gptr*) &opt_execute_str, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "try-reconnect", 't',
    "Specify number of tries for connecting to ndb_mgmd (0 = infinite)", 
    (gptr*) &_try_reconnect, (gptr*) &_try_reconnect, 0,
    GET_UINT, REQUIRED_ARG, 3, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void short_usage_sub(void)
{
  printf("Usage: %s [OPTIONS] [hostname [port]]\n", my_progname);
}
static void usage()
{
  short_usage_sub();
  ndb_std_print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

static int 
read_and_execute(int _try_reconnect) 
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
  line_read = readline (prompt);    
  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);
#else
  static char linebuffer[254];
  fputs(prompt, stdout);
  linebuffer[sizeof(linebuffer)-1]=0;
  line_read = fgets(linebuffer, sizeof(linebuffer)-1, stdin);
  if (line_read == linebuffer) {
    char *q=linebuffer;
    while (*q > 31) q++;
    *q=0;
    line_read= strdup(linebuffer);
  }
#endif
  return com->execute(line_read, _try_reconnect, 1);
}

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char *_host = 0;
  int _port = 0;
  const char *load_default_groups[]= { "mysql_cluster","ndb_mgm",0 };

  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_mgm.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options,
			       ndb_std_get_one_option)))
    exit(ho_error);

  char buf[MAXHOSTNAMELEN+10];
  if(argc == 1) {
    BaseString::snprintf(buf, sizeof(buf), "%s",  argv[0]);
    opt_connect_str= buf;
  } else if (argc >= 2) {
    BaseString::snprintf(buf, sizeof(buf), "%s:%s",  argv[0], argv[1]);
    opt_connect_str= buf;
  }

  if (!isatty(0) || opt_execute_str)
  {
    prompt= 0;
  }

  signal(SIGPIPE, handler);
  com = new Ndb_mgmclient(opt_connect_str,1);
  int ret= 0;
  if (!opt_execute_str)
  {
    ndbout << "-- NDB Cluster -- Management Client --" << endl;
    while(read_and_execute(_try_reconnect));
  }
  else
  {
    com->execute(opt_execute_str,_try_reconnect, 0, &ret);
  }
  delete com;

  ndb_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  return ret;
}

