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

#include <NdbMain.h>
#include <NdbHost.h>
#include <mgmapi.h>
#include <ndb_version.h>
#include <LocalConfig.hpp>

#include "CommandInterpreter.hpp"

const char *progname = "ndb_mgm";


static CommandInterpreter* com;

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

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char *_host = 0;
  int _port = 0;
  const char *load_default_groups[]= { "ndb_mgm",0 };

  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  LocalConfig cfg;

  if(argc >= 1) {
    _host = argv[0];
    if(argc >= 2) {
      _port = atoi(argv[1]);
    }
  } else {
    if(cfg.init(opt_connect_str, 0) && cfg.ids.size() > 0 && cfg.ids[0].type == MgmId_TCP){
      _host = cfg.ids[0].name.c_str();
      _port = cfg.ids[0].port;
    } else {
      cfg.printError();
      cfg.printUsage();
      return 1;
    }
  }
  
  char buf[MAXHOSTNAMELEN+10];
  BaseString::snprintf(buf, sizeof(buf), "%s:%d", _host, _port);

  ndbout << "-- NDB Cluster -- Management Client --" << endl;
  printf("Connecting to Management Server: %s\n", buf);

  signal(SIGPIPE, handler);

  com = new CommandInterpreter(buf);
  while(com->readAndExecute(_try_reconnect));
  delete com;
  
  return 0;
}

