/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <ndb_opts.h>

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbSleep.h>
#include <NDBT.hpp>

#include "my_alloc.h"

static int clear_table(Ndb* pNdb, const NdbDictionary::Table* pTab,
                       bool fetch_across_commit, int parallelism=240);

static const char* _dbname = "TEST_DB";
static bool _transactional = false;
static bool _tupscan = 0;
static bool _diskscan = 0;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "database", 'd', "Name of database table is in",
    (uchar**) &_dbname, (uchar**) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "transactional", 't', "Single transaction (may run out of operations)",
    (uchar**) &_transactional, (uchar**) &_transactional, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "tupscan", NDB_OPT_NOSHORT, "Run tupscan",
    (uchar**) &_tupscan, (uchar**) &_tupscan, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "diskscan", NDB_OPT_NOSHORT, "Run diskcan",
    (uchar**) &_diskscan, (uchar**) &_diskscan, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

int main(int argc, char** argv){
  Ndb_opts opts(argc, argv, my_long_options);
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_delete_all.trace";
#endif
  if (opts.handle_options())
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
  con.set_name("ndb_delete_all");
  if(con.connect(opt_connect_retries - 1, opt_connect_retry_delay, 1) != 0)
  {
    ndbout << "Unable to connect to management server." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  if (con.wait_until_ready(30,0) < 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _dbname );
  if(MyNdb.init() != 0){
    NDB_ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  
  // Check if table exists in db
  int res = NDBT_OK;
  for(int i = 0; i<argc; i++){
    const NdbDictionary::Table * pTab = NDBT_Table::discoverTableFromDb(&MyNdb, argv[i]);
    if(pTab == NULL){
      ndbout << " Table " << argv[i] << " does not exist!" << endl;
      return NDBT_ProgramExit(NDBT_WRONGARGS);
    }
    ndbout << "Deleting all from " << argv[i];
    if (! _transactional)
      ndbout << " (non-transactional)";
    ndbout << " ...";
    if(clear_table(&MyNdb, pTab, ! _transactional) == NDBT_FAILED){
      res = NDBT_FAILED;
      ndbout << "FAILED" << endl;
    }
  }
  return NDBT_ProgramExit(res);
}


int clear_table(Ndb* pNdb, const NdbDictionary::Table* pTab,
                bool fetch_across_commit, int parallelism)
{
  // Scan all records exclusive and delete 
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int deletedRows = 0;
  int check;
  NdbTransaction *pTrans;
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
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	continue;
      }
      goto failed;
    }

    pOp = pTrans->getNdbScanOperation(pTab->getName());	
    if (pOp == NULL) {
      goto failed;
    }
    
    int flags = 0;
    flags |= _tupscan ? NdbScanOperation::SF_TupScan : 0;
    flags |= _diskscan ? NdbScanOperation::SF_DiskScan : 0;
    if( pOp->readTuples(NdbOperation::LM_Exclusive, 
			flags, par) ) {
      goto failed;
    }
    
    if(pTrans->execute(NdbTransaction::NoCommit) != 0){
      err = pTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	continue;
      }
      goto failed;
    }
    
    while((check = pOp->nextResult(true)) == 0){
      do {
	if (pOp->deleteCurrentTuple() != 0){
	  goto failed;
	}
	deletedRows++;
      } while((check = pOp->nextResult(false)) == 0);
      
      if(check != -1){
        if (fetch_across_commit) {
          check = pTrans->execute(NdbTransaction::Commit);   
          pTrans->restart(); // new tx id
        } else {
          check = pTrans->execute(NdbTransaction::NoCommit);
        }
      }
      
      err = pTrans->getNdbError();    
      if(check == -1){
	if(err.status == NdbError::TemporaryError){
	  NDB_ERR(err);
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
	NDB_ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	par = 1;
	goto restart;
      }
      goto failed;
    }
    if (! fetch_across_commit &&
        pTrans->execute(NdbTransaction::Commit) != 0) {
      err = pTrans->getNdbError();
      goto failed;
    }
    pNdb->closeTransaction(pTrans);
    return NDBT_OK;
  }
  return NDBT_FAILED;
  
 failed:
  if(pTrans != 0) pNdb->closeTransaction(pTrans);
  NDB_ERR(err);
  return (err.code != 0 ? err.code : NDBT_FAILED);
}
