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

#include "NDBT_Test.hpp"
#include "NDBT_ReturnCodes.h"
#include "HugoTransactions.hpp"
#include "UtilTransactions.hpp"
#include "NdbRestarter.hpp"


/**
 * Global vector to keep track of 
 * records stored in db
 */

struct SavedRecord {
  int m_gci;
  BaseString m_str;
  SavedRecord(int _gci, BaseString _str){ 
    m_gci = _gci; 
    m_str.assign(_str); 
  }
  SavedRecord(){
    m_gci = 0;
    m_str = "";
  };
};
Vector<SavedRecord> savedRecords;


#define CHECK(b) if (!(b)) { \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  break; }

int runInsertRememberGci(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  int i = 0;

  while(ctx->isTestStopped() == false && i < records){
    // Insert record and read it in same transaction
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, i) == 0);
    if (hugoOps.execute_NoCommit(pNdb) != 0){
      ndbout << "Could not insert record " << i << endl;
      result = NDBT_FAILED;
      break;
    }
    CHECK(hugoOps.pkReadRecord(pNdb, i, false) == 0);
    if (hugoOps.execute_Commit(pNdb) != 0){
      ndbout << "Did not find record in DB " << i << endl;
      result = NDBT_FAILED;
      break;
    }
    savedRecords.push_back(SavedRecord(hugoOps.getRecordGci(0),
				     hugoOps.getRecordStr(0)));

    CHECK(hugoOps.closeTransaction(pNdb) == 0);
    i++;
  };

  return result;
}

int runRestartGciControl(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  UtilTransactions utilTrans(*ctx->getTab());
  NdbRestarter restarter;
  
  // Wait until we have enough records in db
  int count = 0;
  while (count < records){
    if (utilTrans.selectCount(pNdb, 64, &count) != 0){
      ctx->stopTest();
      return NDBT_FAILED;
    }
  }

  // Restart cluster with abort
  if (restarter.restartAll(false, false, true) != 0){
    ctx->stopTest();
    return NDBT_FAILED;
  }

  // Stop the other thread
  ctx->stopTest();

  if (restarter.waitClusterStarted(300) != 0){
    return NDBT_FAILED;
  }
  
  if (pNdb->waitUntilReady() != 0){
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runVerifyInserts(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  Ndb* pNdb = GETNDB(step);
  UtilTransactions utilTrans(*ctx->getTab());
  HugoOperations hugoOps(*ctx->getTab());
  NdbRestarter restarter;

  int restartGCI = pNdb->NdbTamper(Ndb::ReadRestartGCI, 0);    

  ndbout << "restartGCI = " << restartGCI << endl;
  int count = 0;
  if (utilTrans.selectCount(pNdb, 64, &count) != 0){
    return NDBT_FAILED;
  }

  // RULE1: The vector with saved records should have exactly as many 
  // records with lower or same gci as there are in DB
  int recordsWithLowerOrSameGci = 0;
  for (unsigned i = 0; i < savedRecords.size(); i++){
    if (savedRecords[i].m_gci <= restartGCI)
      recordsWithLowerOrSameGci++;
  }
  if (recordsWithLowerOrSameGci != count){
    ndbout << "ERR: Wrong number of expected records" << endl;
    result = NDBT_FAILED;
  }


  // RULE2: The records found in db should have same or lower 
  // gci as in the vector
  for (unsigned i = 0; i < savedRecords.size(); i++){
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkReadRecord(pNdb, i, false) == 0);
    if (hugoOps.execute_Commit(pNdb) != 0){
      // Record was not found in db'

      // Check record gci
      if (savedRecords[i].m_gci <= restartGCI){
	ndbout << "ERR: Record "<<i<<" should have existed" << endl;
	result = NDBT_FAILED;
      }
    } else {
      // Record was found in db
      BaseString str = hugoOps.getRecordStr(0);
      // Check record string
      if (!(savedRecords[i].m_str == str)){
	ndbout << "ERR: Record "<<i<<" str did not match "<< endl;
	result = NDBT_FAILED;
      }
      // Check record gci
      if (savedRecords[i].m_gci > restartGCI){
	ndbout << "ERR: Record "<<i<<" should not have existed" << endl;
	result = NDBT_FAILED;
      }
    }

    CHECK(hugoOps.closeTransaction(pNdb) == 0);    
  }
  

  ndbout << "There are " << count << " records in db" << endl;
  ndbout << "There are " << savedRecords.size() 
	 << " records in vector" << endl;

  ndbout << "There are " << recordsWithLowerOrSameGci 
	 << " records with lower or same gci than " << restartGCI <<  endl;
  
  return result;
}

int runClearGlobals(NDBT_Context* ctx, NDBT_Step* step){
  savedRecords.clear();
  return NDBT_OK;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step), records, 240) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


NDBT_TESTSUITE(testRestartGci);
TESTCASE("InsertRestartGci", 
	 "Verify that only expected records are still in NDB\n"
	 "after a restart" ){
  INITIALIZER(runClearTable);
  INITIALIZER(runClearGlobals);
  STEP(runInsertRememberGci);
  STEP(runRestartGciControl);
  VERIFIER(runVerifyInserts);
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testRestartGci);

int main(int argc, const char** argv){
  ndb_init();
  return testRestartGci.execute(argc, argv);
}

template class Vector<SavedRecord>;
