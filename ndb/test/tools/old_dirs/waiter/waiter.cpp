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
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <getarg.h>


#include <NdbRestarter.hpp>
#include <NDBT.hpp>

int main(int argc, const char** argv){

  const char* _hostName = NULL;
  int _help = 0;

  struct getargs args[] = {
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "hostname:port\n"\
    "This program will connect to the mgmsrv of a NDB cluster.\n"\
    "It will then wait for all nodes to be started\n";

  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _hostName = argv[optind];

  NdbRestarter restarter(_hostName);

  if (restarter.waitClusterStarted() != 0)
    return NDBT_ProgramExit(NDBT_FAILED);

  return NDBT_ProgramExit(NDBT_OK);
}
