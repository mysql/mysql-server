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

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <NDBT.hpp>

#include <getarg.h>

static int clear_table(Ndb* pNdb, const NdbDictionary::Table* pTab, int parallelism=240);

int main(int argc, const char** argv){
  ndb_init();

  const char* _tabname = NULL;
  const char* _dbname = "TEST_DB";
  int _help = 0;
  
  struct getargs args[] = {
    { "usage", '?', arg_flag, &_help, "Print help", "" },
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
    if(clear_table(&MyNdb, pTab) == NDBT_FAILED){
      res = NDBT_FAILED;
      ndbout << "FAILED" << endl;
    }
  }
  return NDBT_ProgramExit(res);
}


int clear_table(Ndb* pNdb, const NdbDictionary::Table* pTab, int parallelism)
{
  // Scan all records exclusive and delete 
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int deletedRows = 0;
  int check;
  NdbConnection *pTrans;
  NdbScanOperation *pOp;
  NdbError err;

  int par = parallelism;
  while (true){
  restart:
    if (retryAttempt++ >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }
    
    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      err = pNdb->getNdbError();
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	continue;
      }
      goto failed;
    }
    
    pOp = pTrans->getNdbScanOperation(pTab->getName());	
    if (pOp == NULL) {
      goto failed;
    }
    
    NdbResultSet * rs = pOp->readTuplesExclusive(par);
    if( rs == 0 ) {
      goto failed;
    }
    
    if(pTrans->execute(NoCommit) != 0){
      err = pTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	continue;
      }
      goto failed;
    }
    
    while((check = rs->nextResult(true)) == 0){
      do {
	if (rs->deleteTuple() != 0){
	  goto failed;
	}
	deletedRows++;
      } while((check = rs->nextResult(false)) == 0);
      
      if(check != -1){
	check = pTrans->execute(Commit);   
	pTrans->restart();
      }
      
      err = pTrans->getNdbError();    
      if(check == -1){
	if(err.status == NdbError::TemporaryError){
	  ERR(err);
	  pNdb->closeTransaction(pTrans);
	  NdbSleep_MilliSleep(50);
	  par = 1;
	  goto restart;
	}
	goto failed;
      }
    }
    if(check == -1){
      err = pTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	par = 1;
	goto restart;
      }
      goto failed;
    }
    pNdb->closeTransaction(pTrans);
    return NDBT_OK;
  }
  return NDBT_FAILED;
  
 failed:
  if(pTrans != 0) pNdb->closeTransaction(pTrans);
  ERR(err);
  return (err.code != 0 ? err.code : NDBT_FAILED);
}
