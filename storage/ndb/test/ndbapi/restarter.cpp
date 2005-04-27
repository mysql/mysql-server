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


#include "mgmapi.h"
#include <string.h>
#include <NdbMain.h>
#include <OutputStream.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <getarg.h>

#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <NDBT.hpp>

int main(int argc, const char** argv){
  ndb_init();

  const char* _hostName = NULL;
  int _loops = 10;
  int _wait = 15;
  int _help = 0;
  int _error_insert = 0;
  int _initial = 0;
  int _master = 0;
  int _maxwait = 120;
  int _multiple = 0;

  struct getargs args[] = {
    { "seconds", 's', arg_integer, &_wait, 
      "Seconds to wait between each restart(0=random)", "secs" },
    { "max seconds", 'm', arg_integer, &_maxwait, 
      "Max seconds to wait between each restart. Default is 120 seconds", 
      "msecs" },
    { "loops", 'l', arg_integer, &_loops, 
      "Number of loops(0=forever)", "loops"},
    { "initial", 'i', arg_flag, &_initial, "Initial node restart"},
    { "error-insert", 'e', arg_flag, &_error_insert, "Use error insert"},
    { "master", 'm', arg_flag, &_master, 
      "Restart the master"},
    { "multiple", 'x', arg_flag, &_multiple,
      "Multiple random node restarts. OBS! Even and odd node Ids must be separated into each node group"},
    { "usage", '?', arg_flag, &_help, "Print help", "" }
    
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "hostname:port\n"\
    "This program will connect to the mgmsrv of a NDB cluster.\n"\
    "It will then wait for all nodes to be started, then restart node(s)\n"\
    "and wait for all to restart inbetween. It will do this \n"\
    "loop number of times\n";

  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _hostName = argv[optind];

  
  NdbRestarts restarts(_hostName);
  NdbRestarter restarter(_hostName);
  
  const char* restartName = "";
  if (_multiple){
    if (_master){
      restartName = "TwoMasterNodeFailure";
    }
    else {
      // Restart 50 percent of nodes
      restartName = "FiftyPercentFail";
    }
  }
  else if (_master){
    restartName = "RestartMasterNodeError";
  }else { 
    if (_error_insert)
      restartName = "RestartRandomNodeError";
    else if (_initial)
      restartName = "RestartRandomNodeInitial";
    else      
      restartName = "RestartRandomNode";
  }

  ndbout << "Performing " << restartName << endl;

  int result = NDBT_OK;
  int l = 0;
  while (_loops == 0 || l<_loops){

    g_info << "Waiting for cluster to start" << endl;
    while (restarter.waitClusterStarted(1) != 0){
      //g_warning << "Ndb failed to start in 2 minutes" << endl;
    }
    
    int seconds = _wait;
    if(seconds==0) {
      // Create random value, default 120 secs
      seconds = (rand() % _maxwait) + 1; 
    }
    g_info << "Waiting for " << seconds << "(" << _maxwait 
	   << ") secs " << endl;
    NdbSleep_SecSleep(seconds);

    g_info << l << ": Restarting node(s) " << endl;

    if (restarts.executeRestart(restartName) != 0){
      result = NDBT_FAILED;
      break;
    }

    l++;
  }
  return NDBT_ProgramExit(NDBT_OK);
}
