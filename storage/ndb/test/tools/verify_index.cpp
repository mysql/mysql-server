/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
  const char* _dbname = NULL;
  const char* _tabname = NULL;
  const char* _indexname = NULL;
  int _findnulls = 1;
  int _bidirectional = 1;
  int _checkviews = 1;
  int _checkdatareplicas = 1;
  int _allsources = 1;
  int _skipindexes = 0;
  int _help = 0;
  
  struct getargs args[] = {
    { "database", 'd', arg_string, &_dbname, "Name of database", "<database>" },
    { "findnulls", 0, arg_integer, &_findnulls, "Verify null values", "<0|(1)>" },
    { "bidirectional", 0, arg_integer, &_bidirectional, "Scan T->I AND I->T", "<0|(1)>" },
    { "checkviews", 0, arg_integer, &_checkviews, "Check index views from all nodes", "<0|(1)>" },
    { "checkdatareplicas", 0, arg_integer, &_checkdatareplicas, "Check table data replicas", "<0|(1)>" },
    { "allsources", 0, arg_integer, &_allsources, "Check table data replicas from all sources", "<0|(1)>" },
    { "skipindexes", 0, arg_integer, &_skipindexes, "Skip checking indexes", "<(0)|1>" },
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname [indexname]\n"\
    "This program will verify the index [indexname] and compare it to data\n"
    "in table [tablename]\n"
    "If no indexname is given, then all indexes are checked.\n"
    "Index checking can optionally not check for entries including nulls. (findnulls)\n"
    "Index checking can be performed unidirectional (table to index) or\n"
    "bidirectionally. (bidirectional)\n"
    "Different views of the index from different nodes can be checked for\n"
    "consistency.  (checkviews)\n"
    "The cross-replica consistency of the underlying data, as viewed from\n"
    "different nodes can be checked for consistency.  (checkdatareplicas)\n"
    "Cross-replica data consistency can be checked relative to a single table \n"
    "scan originating on one or a series of scans originating on all nodes.\n"
    "(allsources).\n"
    "The tool can be used to check cross-replica data consistency without\n"
    "checking index consistency.  (skipindexes)\n"
    "\n"
    "Default values are in (brackets).\n";
  
  if(getarg(args, num_args, argc, argv, &optind) || 
     argv[optind] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  _tabname = argv[optind];
  if (argv[optind+1] != NULL)
  {
    _indexname = argv[optind+1];
  }

  // Connect to Ndb
  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  Ndb MyNdb(&con, (_dbname == NULL?"TEST_DB":_dbname) );

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
  utilTrans.setVerbosity(1);

  if (_checkdatareplicas)
  {
    if (utilTrans.verifyTableReplicas(&MyNdb, (_allsources != 0)) != 0)
    {
      return NDBT_ProgramExit(NDBT_FAILED);
    }
  }

  if (_skipindexes == 0)
  {
    if (_indexname != NULL)
    {
      /* Single index */
      const NdbDictionary::Index* index =
        MyNdb.getDictionary()->getIndex(_indexname, *pTab);
      if (index == NULL)
      {
        ndbout << " Failed to find index " << _indexname << " for table "
               << _tabname << endl;
        return NDBT_ProgramExit(NDBT_FAILED);
      }

      if (utilTrans.verifyIndex(&MyNdb,
                                index,
                                false,
                                _findnulls) != 0){
        return NDBT_ProgramExit(NDBT_FAILED);
      }

      if (_bidirectional)
      {
        if (utilTrans.verifyIndex(&MyNdb,
                                  index,
                                  true,
                                  _findnulls) != 0){
          return NDBT_ProgramExit(NDBT_FAILED);
        }
      }

      if (_checkviews)
      {
        if (utilTrans.verifyIndexViews(&MyNdb, index) != 0){
          return NDBT_ProgramExit(NDBT_FAILED);
        }
      }
    }
    else
    {
      if (utilTrans.verifyAllIndexes(&MyNdb,
                                     _findnulls,
                                     _bidirectional,
                                     _checkviews) != 0){
        return NDBT_ProgramExit(NDBT_FAILED);
      }
    }
  }
    
  return NDBT_ProgramExit(NDBT_OK);
}



