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
#include <NdbMain.h>
#include <NDBT.hpp> 
#include <NdbSleep.h>
#include <getarg.h>
#include <UtilTransactions.hpp>
 
static int 
select_count(Ndb* pNdb, const NdbDictionary::Table* pTab,
	     int parallelism,
	     int* count_rows,
	     UtilTransactions::ScanLock lock);

int main(int argc, const char** argv){
  ndb_init();
  const char* _dbname = "TEST_DB";
  int _parallelism = 240;
  int _help = 0;
  int _lock = 0;

  struct getargs args[] = {
    { "database", 'd', arg_string, &_dbname, "dbname", 
      "Name of database table is in"},
    { "parallelism", 's', arg_integer, &_parallelism, "parallelism", "parallelism" },
    { "usage", '?', arg_flag, &_help, "Print help", "" },
    { "lock", 'l', arg_integer, &_lock, 
      "Read(0), Read-hold(1), Exclusive(2)", "lock"}

  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "tabname1 ... tabnameN\n"\
    "This program will count the number of records in tables\n";
  
  if(getarg(args, num_args, argc, argv, &optind) || 
     argv[optind] == NULL || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }

  // Connect to Ndb
  Ndb MyNdb(_dbname);

  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  // Connect to Ndb and wait for it to become ready
  while(MyNdb.waitUntilReady() != 0)
    ndbout << "Waiting for ndb to become ready..." << endl;
   
  for(int i = optind; i<argc; i++){
    // Check if table exists in db
    const NdbDictionary::Table * pTab = NDBT_Table::discoverTableFromDb(&MyNdb, argv[i]);
    if(pTab == NULL){
      ndbout << " Table " << argv[i] << " does not exist!" << endl;
      continue;
    }

    int rows = 0;
    if (select_count(&MyNdb, pTab, _parallelism, &rows, 
		     (UtilTransactions::ScanLock)_lock) != 0){
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    
    ndbout << rows << " records in table " << argv[i] << endl;
  }
  return NDBT_ProgramExit(NDBT_OK);
}

int 
select_count(Ndb* pNdb, const NdbDictionary::Table* pTab,
	     int parallelism,
	     int* count_rows,
	     UtilTransactions::ScanLock lock){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbConnection	       *pTrans;
  NdbScanOperation	       *pOp;

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
      ERR(err);
      return NDBT_FAILED;
    }
    pOp = pTrans->getNdbScanOperation(pTab->getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    NdbResultSet * rs = pOp->readTuples(NdbScanOperation::LM_Dirty); 
    if( rs == 0 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }


    check = pOp->interpret_exit_last_row();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
  
    Uint64 tmp;
    pOp->getValue(NdbDictionary::Column::ROW_COUNT, (char*)&tmp);
    
    check = pTrans->execute(NoCommit);
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    Uint64 row_count = 0;
    int eof;
    while((eof = rs->nextResult(true)) == 0){
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
      ERR(err);
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


