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
#include <OutputStream.hpp>
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <getarg.h>

#include <NdbRestarter.hpp>
#include <NDBT.hpp>

int main(int argc, const char** argv){
  ndb_init();

  const char* _hostName = NULL;
  int _initial = 0;
  int _help = 0;
  int _wait = 1;


  struct getargs args[] = {
    { "initial", 'i', arg_flag, &_initial, "Do initial restart"},
    { "wait", '\0', arg_negative_flag, &_wait, "Wait until restarted(default=true)"},
    { "usage", '?', arg_flag, &_help, "Print help", "" }
    
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "hostname:port\n"\
    "This program will connect to the mgmsrv of a NDB cluster\n"\
    " and restart the cluster. \n";

  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _hostName = argv[optind];
  
  NdbRestarter restarter(_hostName);
  setOutputLevel(1); // Show only g_err
  int result = NDBT_OK;
  if (_initial){
    ndbout << "Restarting cluster with initial restart" << endl;
    if (restarter.restartAll(true, false, false) != 0)
      result = NDBT_FAILED;
  } else {
    ndbout << "Restarting cluster " << endl;
    if (restarter.restartAll() != 0)
      result = NDBT_FAILED;
  }
  if (result == NDBT_FAILED){
    g_err << "Failed to restart cluster" << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  if (_wait == 1){
    ndbout << "Waiting for cluster to start" << endl;
    if ( restarter.waitClusterStarted(120) != 0){
      ndbout << "Failed waiting for restart of cluster" << endl;
      result = NDBT_FAILED;
    }
  }
  ndbout << "Cluster restarted" << endl;

  return NDBT_ProgramExit(result);
}
