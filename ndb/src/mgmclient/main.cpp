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

#include <NdbMain.h>
#include <NdbHost.h>
#include <util/getarg.h>
#include <mgmapi.h>
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

int main(int argc, const char** argv){
  int optind = 0;
  const char *_host = 0;
  int _port = 0;
  int _help = 0;
  int _try_reconnect = 0;
  
  struct getargs args[] = {
    { "try-reconnect", 't', arg_integer, &_try_reconnect, "Specify number of retries for connecting to ndb_mgmd, default infinite", "#" },
    { "usage", '?', arg_flag, &_help, "Print help", "" },
  };
  int num_args = sizeof(args) / sizeof(args[0]); /* Number of arguments */
  
  
  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, progname, "[host [port]]");
    exit(1);
  }

  argv += optind;
  argc -= optind;

  LocalConfig cfg;

  if(argc >= 1) {
    _host = argv[0];
    if(argc >= 2) {
      _port = atoi(argv[1]);
    }
  } else {
    if(cfg.init(0, 0) && cfg.ids.size() > 0 && cfg.ids[0].type == MgmId_TCP){
      _host = cfg.ids[0].name.c_str();
      _port = cfg.ids[0].port;
    } else {
      cfg.printError();
      cfg.printUsage();
      return 1;
    }
  }
  
  char buf[MAXHOSTNAMELEN+10];
  snprintf(buf, sizeof(buf), "%s:%d", _host, _port);

  ndbout << "-- NDB Cluster -- Management Client --" << endl;
  printf("Connecting to Management Server: %s\n", buf);

  signal(SIGPIPE, handler);

  com = new CommandInterpreter(buf);
  while(com->readAndExecute(_try_reconnect));
  delete com;
  
  return 0;
}

