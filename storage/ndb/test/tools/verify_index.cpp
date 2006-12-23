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

#include <ndb_global.h>

#include <NdbOut.hpp>

#include <NdbApi.hpp>
#include <NdbMain.h>
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
    ERR(MyNdb.getNdbError());
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

  int rows = 0;
  UtilTransactions utilTrans(*pTab);
  if (utilTrans.verifyIndex(&MyNdb, 
			    _indexname, 
			    _parallelism) != 0){
    return NDBT_ProgramExit(NDBT_FAILED);
  }
    
  return NDBT_ProgramExit(NDBT_OK);
}



