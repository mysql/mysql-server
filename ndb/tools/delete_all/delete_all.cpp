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

#include <stdio.h>

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <NDBT.hpp>

#include <getarg.h>

#include <UtilTransactions.hpp>

int main(int argc, const char** argv){

  const char* _tabname = NULL;
  const char* _dbname = "TEST_DB";
  int _help = 0;
  int _ver2 = 1;
  
  struct getargs args[] = {
    { "usage", '?', arg_flag, &_help, "Print help", "" },
    { "ver2", '2', arg_flag, &_ver2, "Use version 2 of clearTable (default)", "" },
    { "ver2", '1', arg_negative_flag, &_ver2, "Use version 1 of clearTable", "" },
    { "database", 'd', arg_string, &_dbname, "dbname", 
      "Name of database table is in"}
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname\n"\
    "This program will delete all records in the specified table using scan delete.\n";
  
  if(getarg(args, num_args, argc, argv, &optind) || 
     argv[optind] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];

  // Connect to Ndb
  Ndb MyNdb(_dbname);

  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  // Connect to Ndb and wait for it to become ready
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
   
  // Check if table exists in db
  int res = NDBT_OK;
  for(int i = optind; i<argc; i++){
    const NdbDictionary::Table * pTab = NDBT_Table::discoverTableFromDb(&MyNdb, argv[i]);
    if(pTab == NULL){
      ndbout << " Table " << _tabname << " does not exist!" << endl;
      return NDBT_ProgramExit(NDBT_WRONGARGS);
    }
    
    ndbout << "Deleting all from " << argv[i] << "...";
    UtilTransactions utilTrans(*pTab);
    int tmp = NDBT_OK;
    if (_ver2 == 0){
      if(utilTrans.clearTable(&MyNdb) == NDBT_FAILED)
	tmp = NDBT_FAILED;
    } else {
      if(utilTrans.clearTable3(&MyNdb) == NDBT_FAILED)
	tmp = NDBT_FAILED;
    }
    if(tmp == NDBT_FAILED){
      res = tmp;
      ndbout << "FAILED" << endl;
    }
  }
  return NDBT_ProgramExit(res);
}

