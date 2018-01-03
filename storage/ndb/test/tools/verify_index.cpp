/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <NdbOut.hpp>

#include <NdbApi.hpp>
#include <NDBT.hpp> 
#include <NdbSleep.h>
#include <getarg.h>
#include <UtilTransactions.hpp>
 

int main(int argc, const char** argv){
  ndb_init();
  int _parallelism = 240;
  const char* _tabname = NULL;
  const char* _indexname = NULL;
  int _help = 0;
  
  struct getargs args[] = {
    { "parallelism", 's', arg_integer, &_parallelism, "parallelism", "parallelism" },
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname indexname\n"\
    "This program will verify the index [indexname] and compare it to data\n"
    "in table [tablename]\n";
  
  if(getarg(args, num_args, argc, argv, &optind) || 
     argv[optind] == NULL || argv[optind+1] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];
  _indexname = argv[optind+1];

  // Connect to Ndb
  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  Ndb MyNdb(&con, "TEST_DB" );

  if(MyNdb.init() != 0){
    NDB_ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  // Connect to Ndb and wait for it to become ready
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
   
  // Check if table exists in db
  const NdbDictionary::Table * pTab = NDBT_Table::discoverTableFromDb(&MyNdb, _tabname);
  if(pTab == NULL){
    ndbout << " Table " << _tabname << " does not exist!" << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  UtilTransactions utilTrans(*pTab);
  if (utilTrans.verifyIndex(&MyNdb, 
			    _indexname, 
			    _parallelism) != 0){
    return NDBT_ProgramExit(NDBT_FAILED);
  }
    
  return NDBT_ProgramExit(NDBT_OK);
}



