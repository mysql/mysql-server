/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <NdbSleep.h>
#include <getarg.h>
#include <NdbOut.hpp>
#include <OutputStream.hpp>

#include <NDBT.hpp>
#include <NdbRestarter.hpp>

int main(int argc, const char **argv) {
  ndb_init();

  const char *_hostName = NULL;
  int _initial = 0;
  int _help = 0;
  int _wait = 1;

  struct getargs args[] = {
      {"initial", 'i', arg_flag, &_initial, "Do initial restart", ""},
      {"wait", '\0', arg_negative_flag, &_wait,
       "Wait until restarted(default=true)", ""},
      {"usage", '?', arg_flag, &_help, "Print help", ""}

  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] =
      "hostname:port\n"
      "This program will connect to the management server of an NDB cluster\n"
      " and restart the cluster. \n";

  if (getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _hostName = argv[optind];

  NdbRestarter restarter(_hostName);
  setOutputLevel(1);  // Show only g_err
  int result = NDBT_OK;
  if (_initial) {
    ndbout << "Restarting cluster with initial restart" << endl;
    if (restarter.restartAll(true, false, false) != 0) result = NDBT_FAILED;
  } else {
    ndbout << "Restarting cluster " << endl;
    if (restarter.restartAll() != 0) result = NDBT_FAILED;
  }
  if (result == NDBT_FAILED) {
    g_err << "Failed to restart cluster" << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  if (_wait == 1) {
    ndbout << "Waiting for cluster to start" << endl;
    if (restarter.waitClusterStarted(120) != 0) {
      ndbout << "Failed waiting for restart of cluster" << endl;
      result = NDBT_FAILED;
    }
  }
  ndbout << "Cluster restarted" << endl;

  return NDBT_ProgramExit(result);
}
