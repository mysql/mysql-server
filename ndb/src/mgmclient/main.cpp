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
#if defined( __WIN__) || defined(OS2)
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
handler(int sig){
  switch(sig){
  case SIGPIPE:
    /**
     * Will happen when connection to mgmsrv is broken
     * Reset connected flag
     */
    com->disconnect();    
    break;
  }
}


static unsigned _try_reconnect;
static char *opt_connect_str= 0;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_mgm"),
  { "try-reconnect", 't',
    "Specify number of retries for connecting to ndb_mgmd, default infinite", 
    (gptr*) &_try_reconnect, (gptr*) &_try_reconnect, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void short_usage_sub(void)
{
  printf("Usage: %s [OPTIONS] [hostname [port]]\n", my_progname);
}
static void print_version()
{
  printf("MySQL distrib %s, for %s (%s)\n",MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}
static void usage()
{
  short_usage_sub();
  print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:O,/tmp/ndb_mgm.trace");
    break;
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
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
  line_read = readline ("ndb_mgm> ");    
  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);
#else
  static char linebuffer[254];
  fputs("ndb_mgm> ", stdout);
  linebuffer[sizeof(linebuffer)-1]=0;
  line_read = fgets(linebuffer, sizeof(linebuffer)-1, stdin);
  if (line_read == linebuffer) {
    char *q=linebuffer;
    while (*q > 31) q++;
    *q=0;
    line_read= strdup(linebuffer);
  }
#endif
  return com->execute(line_read,_try_reconnect);
}

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char *_host = 0;
  int _port = 0;
  const char *load_default_groups[]= { "mysql_cluster","ndb_mgm",0 };

  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  char buf[MAXHOSTNAMELEN+10];
  if(argc == 1) {
    BaseString::snprintf(buf, sizeof(buf), "%s",  argv[0]);
    opt_connect_str= buf;
  } else if (argc >= 2) {
    BaseString::snprintf(buf, sizeof(buf), "%s:%s",  argv[0], argv[1]);
    opt_connect_str= buf;
  }

  ndbout << "-- NDB Cluster -- Management Client --" << endl;
  printf("Connecting to Management Server: %s\n", opt_connect_str ? opt_connect_str : "default");

  signal(SIGPIPE, handler);

  com = new Ndb_mgmclient(opt_connect_str);
  while(read_and_execute(_try_reconnect));
  delete com;
  
  return 0;
}

