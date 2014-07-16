/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <string.h>
#include <NdbMain.h>
#include <OutputStream.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <getarg.h>

#include <NdbRestarter.hpp>
#include <NDBT.hpp>

int main(int argc, const char** argv){
  ndb_init();

  const char* _hostName = NULL;
  int _loops = 10;
  int _wait = 15;
  int _help = 0;
#if 0
  int _crash = 0;
  int _abort = 0;
#endif

  struct getargs args[] = {
    { "seconds", 's', arg_integer, &_wait, "Seconds to wait between each restart(0=random)", "secs" },
    { "loops", 'l', arg_integer, &_loops, "Number of loops", "loops 0=forever"},
#if 0 
    // Not yet!
    { "abort", 'a', arg_flag, &_abort, "Restart abort"},
    { "crash", 'c', arg_flag, &_crash, "Crash instead of restart"},
#endif
    { "usage", '?', arg_flag, &_help, "Print help", "" }
    
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "hostname:port\n"\
    "This program will connect to the mgmsrv of a NDB cluster.\n"\
    "It will wait for all nodes to be started, then restart all nodes\n"\
    "into nostart state. Then after a random delay it will tell all nodes\n"\
    "to start. It will do this loop number of times\n";

  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _hostName = argv[optind];
  
  NdbRestarter restarter(_hostName);
#if 0  
  if(_abort && _crash){
    g_err << "You can't specify both abort and crash" << endl;
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  if(_abort){
    restarter.setRestartType(NdbRestarter::AbortRestart);
  }
  if(_crash){
    restarter.setRestartType(NdbRestarter::Crash);
  }
#endif

  int l = 0;
  while (_loops == 0 || l<_loops){
    g_info << "Waiting for cluster to start" << endl;
    while(restarter.waitClusterStarted(120) != 0){
      g_warning << "Ndb failed to start in 2 minutes" << endl;
    }
    
    int seconds = _wait;
    if(seconds==0)
      seconds = (rand() % 120) + 1; // Create random value max 120 secs
    g_info << "Waiting for "<<seconds<<" secs" << endl;
    NdbSleep_SecSleep(seconds);

    g_info << l << ": restarting all nodes with nostart" << endl;
    const bool b = (restarter.restartAll(false, true, false) == 0);
    require(b);
    
    g_info << "Waiting for cluster to enter nostart" << endl;
    while(restarter.waitClusterNoStart(120) != 0){
      g_warning << "Ndb failed to enter no start in 2 minutes" << endl;
    }

    seconds = _wait;
    if(seconds==0)
      seconds = (rand() % 120) + 1; // Create random value max 120 secs
    g_info << "Waiting for " <<seconds<<" secs" << endl;
    NdbSleep_SecSleep(seconds);

    g_info << l << ": Telling all nodes to start" << endl;
    const bool b2 = (restarter.startAll() == 0);
    require(b2);

    l++;
  }

  return NDBT_ProgramExit(NDBT_OK);
}
