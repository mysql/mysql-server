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

struct OperationTestCase {
  const char * name;
  bool preCond; // start transaction | insert | commit

  // start transaction
  const char * op1;
  const int res1;
  const int val1;

  // no commit

  const char * op2;
  const int res2;
  const int val2;
  // Commit

  // start transaction
  // op3 = READ
  const int res3;
  const int val3;
  // commit transaction
};

OperationTestCase matrix[] = {
  { "ReadRead",         true, "READ",  0, 0, "READ",      0, 0,   0, 0 },
  { "ReadReadEx",       true, "READ",  0, 0, "READ-EX",   0, 0,   0, 0 },
  { "ReadSimpleRead",   true, "READ",  0, 0, "S-READ",    0, 0,   0, 0 },
  { "ReadDirtyRead",    true, "READ",  0, 0, "D-READ",    0, 0,   0, 0 },
  { "ReadInsert",       true, "READ",  0, 0, "INSERT",  630, 1,   0, 0 },
  { "ReadUpdate",       true, "READ",  0, 0, "UPDATE",    0, 1,   0, 1 },
  { "ReadDelete",       true, "READ",  0, 0, "DELETE",    0, 0, 626, 0 },

  { "FReadRead",       false, "READ", 626, 0, "READ",    626, 0, 626, 0 },
  { "FReadReadEx",     false, "READ", 626, 0, "READ-EX", 626, 0, 626, 0 },
  { "FReadSimpleRead", false, "READ", 626, 0, "S-READ",  626, 0, 626, 0 },
  { "FReadDirtyRead",  false, "READ", 626, 0, "D-READ",  626, 0, 626, 0 },
  { "FReadInsert",     false, "READ", 626, 0, "INSERT",    0, 1,   0, 1 },
  { "FReadUpdate",     false, "READ", 626, 0, "UPDATE",  626, 0, 626, 0 },
  { "FReadDelete",     false, "READ", 626, 0, "DELETE",  626, 0, 626, 0 },

  { "ReadExRead",       true, "READ-EX", 0, 0, "READ",      0, 0,   0, 0 },
  { "ReadExReadEx",     true, "READ-EX", 0, 0, "READ-EX",   0, 0,   0, 0 },
  { "ReadExSimpleRead", true, "READ-EX", 0, 0, "S-READ",    0, 0,   0, 0 },
  { "ReadExDirtyRead",  true, "READ-EX", 0, 0, "D-READ",    0, 0,   0, 0 },
  { "ReadExInsert",     true, "READ-EX", 0, 0, "INSERT",  630, 1,   0, 0 },
  { "ReadExUpdate",     true, "READ-EX", 0, 0, "UPDATE",    0, 1,   0, 1 },
  { "ReadExDelete",     true, "READ-EX", 0, 0, "DELETE",    0, 0, 626, 0 },

  { "InsertRead",      false, "INSERT", 0, 0, "READ",      0, 0,   0, 0 },
  { "InsertReadEx",    false, "INSERT", 0, 0, "READ-EX",   0, 0,   0, 0 },
  { "InsertSimpleRead",false, "INSERT", 0, 0, "S-READ",    0, 0,   0, 0 },
  { "InsertDirtyRead", false, "INSERT", 0, 0, "D-READ",    0, 0,   0, 0 },
  { "InsertInsert",    false, "INSERT", 0, 0, "INSERT",  630, 0, 626, 0 },
  { "InsertUpdate",    false, "INSERT", 0, 0, "UPDATE",    0, 1,   0, 1 },
  { "InsertDelete",    false, "INSERT", 0, 0, "DELETE",    0, 0, 626, 0 },

  { "UpdateRead",       true, "UPDATE", 0, 1, "READ",      0, 1,   0, 1 },
  { "UpdateReadEx",     true, "UPDATE", 0, 1, "READ-EX",   0, 1,   0, 1 },
  { "UpdateSimpleRead", true, "UPDATE", 0, 1, "S-READ",    0, 1,   0, 1 },
  { "UpdateDirtyRead",  true, "UPDATE", 0, 1, "D-READ",    0, 1,   0, 1 },
  { "UpdateInsert",     true, "UPDATE", 0, 1, "INSERT",  630, 0,   0, 0 },
  { "UpdateUpdate",     true, "UPDATE", 0, 1, "UPDATE",    0, 2,   0, 2 },
  { "UpdateDelete",     true, "UPDATE", 0, 1, "DELETE",    0, 0, 626, 0 },

  { "DeleteRead",       true, "DELETE", 0, 0, "READ",    626, 0,   0, 0 },
  { "DeleteReadEx",     true, "DELETE", 0, 0, "READ-EX", 626, 0,   0, 0 },
  { "DeleteSimpleRead", true, "DELETE", 0, 0, "S-READ",  626, 0,   0, 0 },
  { "DeleteDirtyRead",  true, "DELETE", 0, 0, "D-READ",  626, 0, 626, 0 },
  { "DeleteInsert",     true, "DELETE", 0, 0, "INSERT",    0, 1,   0, 1 },
  { "DeleteUpdate",     true, "DELETE", 0, 0, "UPDATE",  626, 1,   0, 0 },
  { "DeleteDelete",     true, "DELETE", 0, 0, "DELETE",  626, 0,   0, 0 }
};

#define CHECK(b) if (!(b)) { \
  g_err  << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  break; }

int
runOp(HugoOperations & hugoOps,
      Ndb * pNdb,
      const char * op,
      int value){

#define C2(x, y) { int r = (x); int s = (y); if(r != s) {\
  g_err  << "ERR: failed on line " << __LINE__ << ": " \
     << r << " != " << s << endl; \
  return NDBT_FAILED; }}
  
  if(strcmp(op, "READ") == 0){
    C2(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Read), 0);
  } else if(strcmp(op, "READ-EX") == 0){
    C2(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Exclusive), 0);      
  } else if(strcmp(op, "S-READ") == 0){
    C2(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_Read), 0);
  } else if(strcmp(op, "D-READ") == 0){
    C2(hugoOps.pkReadRecord(pNdb, 1, 1, NdbOperation::LM_CommittedRead), 0);
  } else if(strcmp(op, "INSERT") == 0){
    C2(hugoOps.pkInsertRecord(pNdb, 1, 1, value), 0);
  } else if(strcmp(op, "UPDATE") == 0){
    C2(hugoOps.pkUpdateRecord(pNdb, 1, 1, value), 0);
  } else if(strcmp(op, "DELETE") == 0){
    C2(hugoOps.pkDeleteRecord(pNdb, 1, 1), 0);
  } else {
    g_err << __FILE__ << " - " << __LINE__ 
	  << ": Unknown operation" << op << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int
checkVal(HugoOperations & hugoOps,
	 const char * op,
	 int value,
	 int result){
  if(result != 0)
    return NDBT_OK;

  if(strcmp(op, "READ") == 0){
  } else if(strcmp(op, "READ-EX") == 0){
  } else if(strcmp(op, "S-READ") == 0){
  } else if(strcmp(op, "D-READ") == 0){
  } else {
    return NDBT_OK;
  }
  
  return hugoOps.verifyUpdatesValue(value);
}

int 
runTwoOperations(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  const char * op1 = ctx->getProperty("op1", "NONE");
  const int val1 = ctx->getProperty("val1", ~0);
  const int res1 = ctx->getProperty("res1", ~0);
  const char * op2 = ctx->getProperty("op2", "NONE");
  const int res2 = ctx->getProperty("res2", ~0);
  const int val2 = ctx->getProperty("val2", ~0);

  const int res3 = ctx->getProperty("res3", ~0);
  const int val3 = ctx->getProperty("val3", ~0);

  do {
    // Insert, read
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(runOp(hugoOps, pNdb, op1, val1) == 0);
    AbortOption oa = (res1 == 0) ? AbortOnError : IgnoreError;
    CHECK(hugoOps.execute_NoCommit(pNdb, oa) == res1);
    CHECK(checkVal(hugoOps, op1, val1, res1) == 0);

    ndbout_c("-- running op 2");

    CHECK(runOp(hugoOps, pNdb, op2, val2) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == res2);
    CHECK(checkVal(hugoOps, op2, val2, res2) == 0);

  } while(false);
  hugoOps.closeTransaction(pNdb);

  if(result != NDBT_OK)
    return result;

  do {
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(runOp(hugoOps, pNdb, "READ", 0) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == res3);
    CHECK(checkVal(hugoOps, "READ", val3, res3) == 0);
  } while(false);
  hugoOps.closeTransaction(pNdb);

  return result;
}

int 
runInsertRecord(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  do{
    // Insert, insert 
    CHECK(hugoOps.startTransaction(pNdb) == 0);  
    CHECK(hugoOps.pkInsertRecord(pNdb, 1) == 0);
    CHECK(hugoOps.execute_Commit(pNdb) == 0);
    
  } while(false);
  
  hugoOps.closeTransaction(pNdb);
  
  return result;
}

int
runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step), records, 240) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
main(int argc, const char** argv){
  ndb_init();

  NDBT_TestSuite ts("testOperations");
  for(Uint32 i = 0; i<sizeof(matrix)/sizeof(matrix[0]); i++){
    NDBT_TestCaseImpl1 *pt = new NDBT_TestCaseImpl1(&ts, matrix[i].name, "");
    
    pt->addInitializer(new NDBT_Initializer(pt, 
					    "runClearTable", 
					    runClearTable));
    
    if(matrix[i].preCond){
      pt->addInitializer(new NDBT_Initializer(pt, 
					      "runInsertRecord", 
					      runInsertRecord));
    }
    
    pt->setProperty("op1", matrix[i].op1);
    pt->setProperty("res1", matrix[i].res1);
    pt->setProperty("val1", matrix[i].val1);

    pt->setProperty("op2", matrix[i].op2);
    pt->setProperty("res2", matrix[i].res2);
    pt->setProperty("val2", matrix[i].val2);

    pt->setProperty("res3", matrix[i].res3);
    pt->setProperty("val3", matrix[i].val3);

    pt->addStep(new NDBT_ParallelStep(pt, 
				      matrix[i].name,
				      runTwoOperations));
    pt->addFinalizer(new NDBT_Finalizer(pt, 
					"runClearTable", 
					runClearTable));
    
    ts.addTest(pt);
  }

  return ts.execute(argc, argv);
}

