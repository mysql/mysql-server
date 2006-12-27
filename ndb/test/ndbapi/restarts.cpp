/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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

#include <NdbRestarts.hpp>
#include <NDBT.hpp>

int main(int argc, const char** argv){
  ndb_init();

  const char* _restartName = NULL;
  int _loops = 1;
  int _wait = -1;
  int _maxwait = 120;
  int _help = 0;
  int _list = 0;
  int _random = 0;
  int _timeout = 0;
  int _all = 0;

  struct getargs args[] = {
    { "seconds", 's', arg_integer, &_wait, 
      "Seconds to wait between each restart(0=random)", "secs" },
    { "max seconds", 'm', arg_integer, &_maxwait, 
      "Max seconds to wait between each restart. Default is 120 seconds", "msecs" },
    { "loops", 'l', arg_integer, &_loops, "Number of loops(0=forever)", "loops"},
    { "timeout", 't', arg_integer, &_timeout, "Timeout waiting for nodes to start", "seconds"},
    { "random", 'r', arg_flag, &_random, "Select restart case randomly", 
      ""},
    { "all", 'a', arg_flag, &_all, "Run all restarts", 
      ""},
    { "list-restarts", '\0', arg_flag, &_list, "List available restarts", ""},
    { "usage", '?', arg_flag, &_help, "Print help", "" }
    
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "testname\n" \
    "This program will perform node restart, \n"\
    "multiple node restart or system-restart.\n"\
    "Use --list-restarts to see whats available\n";

  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  if (_list){
    NdbRestarts restarts;
    restarts.listRestarts();
    return NDBT_ProgramExit(NDBT_OK);
  }

  if ((argv[optind] == NULL) && (_random == 0)){
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _restartName = argv[optind];
  
  NdbRestarts restarts;

  int res = NDBT_OK;
  int l = 0;
  while (_loops == 0 || l < _loops){

    if (_all) {
      // Execute all restarts, set loops to numRestarts
      // so that ecvery restart is executed once
      _loops = restarts.getNumRestarts();
      res = restarts.executeRestart(l, _timeout);
    } else if (_random) {
      int num = rand() % restarts.getNumRestarts();
      res = restarts.executeRestart(num, _timeout);
    } else {
      res = restarts.executeRestart(_restartName, _timeout);
    }
    if (res != NDBT_OK)
      break;

    if (_wait >= 0){
      int seconds = _wait;
      if(seconds==0) {
	// Create random value, default 120 secs
	seconds = (rand() % _maxwait) + 1; 
      }
      g_info << "Waiting for " << seconds << "(" << _maxwait 
	     << ") secs " << endl;
      NdbSleep_SecSleep(seconds);
    }

    l++;
  }
  return NDBT_ProgramExit(res);
}
