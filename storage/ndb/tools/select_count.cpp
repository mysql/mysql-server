/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>
#include <ndb_opts.h>

#include <NdbOut.hpp>

#include <NdbApi.hpp>
#include <NdbMain.h>
#include <NDBT.hpp> 
#include <NdbSleep.h>
#include <UtilTransactions.hpp>
 
static int 
select_count(Ndb* pNdb, const NdbDictionary::Table* pTab,
	     int parallelism,
	     Uint64* count_rows,
	     NdbOperation::LockMode lock);

static const char* _dbname = "TEST_DB";
static int _parallelism = 240;
static int _lock = 0;

const char *load_default_groups[]= { "mysql_cluster",0 };

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_select_count"),
  { "database", 'd', "Name of database table is in",
    (uchar**) &_dbname, (uchar**) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "parallelism", 'p', "parallelism",
    (uchar**) &_parallelism, (uchar**) &_parallelism, 0,
    GET_INT, REQUIRED_ARG, 240, 0, 0, 0, 0, 0 }, 
  { "lock", 'l', "Read(0), Read-hold(1), Exclusive(2)",
    (uchar**) &_lock, (uchar**) &_lock, 0,
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 }, 
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub("<table name>[, <table name>[, ...]]");
}

static void usage()
{
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  ndb_load_defaults(NULL,load_default_groups,&argc,&argv);
  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_select_count.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options,
			       ndb_std_get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  if (argc < 1) {
    usage();
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  Ndb_cluster_connection con(opt_ndb_connectstring, opt_ndb_nodeid);
  con.set_name("ndb_select_count");
  if(con.connect(12, 5, 1) != 0)
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

  for(int i = 0; i<argc; i++){
    // Check if table exists in db
    const NdbDictionary::Table * pTab = NDBT_Table::discoverTableFromDb(&MyNdb, argv[i]);
    if(pTab == NULL){
      ndbout << " Table " << argv[i] << " does not exist!" << endl;
      continue;
    }

    Uint64 rows = 0;
    if (select_count(&MyNdb, pTab, _parallelism, &rows, 
		     (NdbOperation::LockMode)_lock) != 0){
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    
    ndbout << rows << " records in table " << argv[i] << endl;
  }
  return NDBT_ProgramExit(NDBT_OK);
}

int 
select_count(Ndb* pNdb, const NdbDictionary::Table* pTab,
	     int parallelism,
	     Uint64* count_rows,
	     NdbOperation::LockMode lock){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbTransaction       *pTrans;
  NdbScanOperation	       *pOp;
  const Uint32 codeWords= 1;
  Uint32 codeSpace[ codeWords ];
  NdbInterpretedCode code(NULL, // Table is irrelevant
                          &codeSpace[0],
                          codeWords);
  if ((code.interpret_exit_last_row() != 0) ||
      (code.finalise() != 0))
  {
    NDB_ERR(code.getNdbError());
    return NDBT_FAILED;
  }

  while (true){

    if (retryAttempt >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }
    pOp = pTrans->getNdbScanOperation(pTab->getName());	
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if( pOp->readTuples(NdbScanOperation::LM_Dirty) ) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }


    check = pOp->setInterpretedCode(&code);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
  
    Uint64 tmp;
    Uint32 row_size;
    pOp->getValue(NdbDictionary::Column::ROW_COUNT, (char*)&tmp);
    pOp->getValue(NdbDictionary::Column::ROW_SIZE, (char*)&row_size);
    check = pTrans->execute(NdbTransaction::NoCommit);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    Uint64 row_count = 0;
    int eof;
    while((eof = pOp->nextResult(true)) == 0){
      row_count += tmp;
    }
    
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    pNdb->closeTransaction(pTrans);
    
    if (count_rows != NULL){
      *count_rows = row_count;
    }
    
    return NDBT_OK;
  }
  return NDBT_FAILED;
}


