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

#include <NdbApiSignal.hpp>
#include <getarg.h>

#include <rep/RepComponents.hpp>

#include "rep_version.hpp"
#include <rep/RepCommandInterpreter.hpp>
#include <rep/RepApiInterpreter.hpp>


int
main(int argc, const char **argv)
{
  RepComponents comps;
  RepCommandInterpreter cmd(&comps);


  int helpFlag = false;
  int noConnectFlag = false;
  int onlyPrimaryFlag = false;
  int onlyStandbyFlag = false;
  int port = 18000;
  replogEnabled = false;

  struct getargs args[] = {
    { "psc", '1', arg_string, &comps.m_connectStringPS, 
      "Connect string", "connectstring" },
    { "ssc", '2', arg_string, &comps.m_connectStringSS, 
      "Connect string", "connectstring" },
    { "port", 'p', arg_integer, &port, 
      "port for rep api. Default 18000", "" },
    { "usage", '?', arg_flag, &helpFlag, 
      "Print help", "" },
/* @todo
    { "noConnect", 'n', arg_flag, &noConnectFlag, 
      "Do not connect adapters", "" },
*/
    { "debug", 'd', arg_flag, &replogEnabled, 
      "Enable debug printouts on console", "" },
    { "onlyStandby", 's', arg_flag, &onlyStandbyFlag, 
      "Let Replication Server view DBMS as standby (destination) system only", 
      "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "\nWhen working as a primary system node, this program receives\n"\
    "records from the primary NDB Cluster and forwards them to\n"\
    "the standby system.\n\n"\
    "When working as a standby system node, this program receives\n"\
    "records from another replication node and inserts them into\n"\
    "the standby NDB Cluster.\n\n"\
    "Example:  ndb_rep --psc=\"nodeid=3;host=localhost:10000\"\n";
  
  if(getarg(args, num_args, argc, argv, &optind) || 
     //argv[optind] == NULL || 
     helpFlag)
  {
    arg_printusage(args, num_args, argv[0], desc);
    return -1; //NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  RepApiInterpreter api(&comps,port);
  api.startInterpreter();
  
  /**************************
   * Command-line interface *
   **************************/
  if (!noConnectFlag && !onlyPrimaryFlag) comps.connectSS();
  if (!noConnectFlag && !onlyStandbyFlag) comps.connectPS();


  while (true) {
    if(!cmd.readAndExecute()) {
      api.stopInterpreter();
      exit(1);
    }
  }
}
