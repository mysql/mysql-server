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

#include <NDBT_Test.hpp>
#include <NDBT_ReturnCodes.h>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <signaldata/DumpStateOrd.hpp>

struct OperationTestCase {
  const char * name;
  bool preCond; // start transaction | insert | commit

  // start transaction 1
  const char * op1;
  const int val1;

  // no commit

  // start transaction 2
  const char * op2;
  const int res2;
  const int val2;
  // no commit

  // commit transaction 1
  // commit transaction 2
  
  // start transaction
  // op3 = READ
  const int res3;
  const int val3;
  // commit transaction
};

#define X -1

OperationTestCase matrix[] = {
  { "ReadRead",         true, "READ",   1, "READ",      0, 1,   0, 1 },
  { "ReadReadEx",       true, "READ",   1, "READ-EX", 266, X,   0, 1 },
  { "ReadSimpleRead",   true, "READ",   1, "S-READ",    0, 1,   0, 1 },
  { "ReadDirtyRead",    true, "READ",   1, "D-READ",    0, 1,   0, 1 },
  { "ReadInsert",       true, "READ",   1, "INSERT",  266, X,   0, 1 },
  { "ReadUpdate",       true, "READ",   1, "UPDATE",  266, X,   0, 1 },
  { "ReadDelete",       true, "READ",   1, "DELETE",  266, X,   0, 1 },
  { "ReadScan",         true, "READ",   1, "SCAN",      0, 1,   0, 1 },
  { "ReadScanHl",       true, "READ",   1, "SCAN-HL",   0, 1,   0, 1 },
  { "ReadScanEx",       true, "READ",   1, "SCAN-EX", 274, X,   0, 1 },
#if 0
  { "ReadScanUp",       true, "READ",   1, "SCAN-UP", 266, X,   0, 1 },
  { "ReadScanDe",       true, "READ",   1, "SCAN-DE", 266, X,   0, 1 },
#endif

  { "ScanRead",         true, "SCAN",   1, "READ",      0, 1,   0, 1 },
  { "ScanReadEx",       true, "SCAN",   1, "READ-EX",   0, 1,   0, 1 },
  { "ScanSimpleRead",   true, "SCAN",   1, "S-READ",    0, 1,   0, 1 },
  { "ScanDirtyRead",    true, "SCAN",   1, "D-READ",    0, 1,   0, 1 },
  { "ScanInsert",       true, "SCAN",   1, "INSERT",  630, X,   0, 1 },
  { "ScanUpdate",       true, "SCAN",   1, "UPDATE",    0, 2,   0, 2 },
  { "ScanDelete",       true, "SCAN",   1, "DELETE",    0, X, 626, X },
  { "ScanScan",         true, "SCAN",   1, "SCAN",      0, 1,   0, 1 },
  { "ScanScanHl",       true, "SCAN",   1, "SCAN-HL",   0, 1,   0, 1 },
  { "ScanScanEx",       true, "SCAN",   1, "SCAN-EX",   0, 1,   0, 1 },
#if 0
  { "ScanScanUp",       true, "SCAN",   1, "SCAN-UP", 266, X,   0, 1 },
  { "ScanScanDe",       true, "SCAN",   1, "SCAN-DE", 266, X,   0, 1 },
#endif

  { "ScanHlRead",       true, "SCAN-HL",1, "READ",      0, 1,   0, 1 },
  { "ScanHlReadEx",     true, "SCAN-HL",1, "READ-EX", 266, 1,   0, 1 },
  { "ScanHlSimpleRead", true, "SCAN-HL",1, "S-READ",    0, 1,   0, 1 },
  { "ScanHlDirtyRead",  true, "SCAN-HL",1, "D-READ",    0, 1,   0, 1 },
  { "ScanHlInsert",     true, "SCAN-HL",1, "INSERT",  266, X,   0, 1 },
  { "ScanHlUpdate",     true, "SCAN-HL",1, "UPDATE",  266, 2,   0, 1 },
  { "ScanHlDelete",     true, "SCAN-HL",1, "DELETE",  266, X,   0, 1 },
  { "ScanHlScan",       true, "SCAN-HL",1, "SCAN",      0, 1,   0, 1 },
  { "ScanHlScanHl",     true, "SCAN-HL",1, "SCAN-HL",   0, 1,   0, 1 },
  { "ScanHlScanEx",     true, "SCAN-HL",1, "SCAN-EX", 274, X,   0, 1 },
#if 0
  { "ScanHlScanUp",     true, "SCAN-HL",1, "SCAN-UP", 266, X,   0, 1 },
  { "ScanHlScanDe",     true, "SCAN-HL",1, "SCAN-DE", 266, X,   0, 1 },
#endif

  { "ScanExRead",       true, "SCAN-EX",1, "READ",    266, 1,   0, 1 },
  { "ScanExReadEx",     true, "SCAN-EX",1, "READ-EX", 266, 1,   0, 1 },
  { "ScanExSimpleRead", true, "SCAN-EX",1, "S-READ",  266, 1,   0, 1 },
  { "ScanExDirtyRead",  true, "SCAN-EX",1, "D-READ",    0, 1,   0, 1 },
  { "ScanExInsert",     true, "SCAN-EX",1, "INSERT",  266, X,   0, 1 },
  { "ScanExUpdate",     true, "SCAN-EX",1, "UPDATE",  266, 2,   0, 1 },
  { "ScanExDelete",     true, "SCAN-EX",1, "DELETE",  266, X,   0, 1 },
  { "ScanExScan",       true, "SCAN-EX",1, "SCAN",      0, 1,   0, 1 },
  { "ScanExScanHl",     true, "SCAN-EX",1, "SCAN-HL", 274, X,   0, 1 },
  { "ScanExScanEx",     true, "SCAN-EX",1, "SCAN-EX", 274, X,   0, 1 },
#if 0
  { "ScanExScanUp",     true, "SCAN-EX",1, "SCAN-UP", 266, X,   0, 1 },
  { "ScanExScanDe",     true, "SCAN-EX",1, "SCAN-DE", 266, X,   0, 1 },
#endif

  { "ReadExRead",       true, "READ-EX",1, "READ",    266, X,   0, 1 },
  { "ReadExReadEx",     true, "READ-EX",1, "READ-EX", 266, X,   0, 1 },
  { "ReadExSimpleRead", true, "READ-EX",1, "S-READ",  266, X,   0, 1 },
  { "ReadExDirtyRead",  true, "READ-EX",1, "D-READ",    0, 1,   0, 1 },
  { "ReadExInsert",     true, "READ-EX",1, "INSERT",  266, X,   0, 1 },
  { "ReadExUpdate",     true, "READ-EX",1, "UPDATE",  266, X,   0, 1 },
  { "ReadExDelete",     true, "READ-EX",1, "DELETE",  266, X,   0, 1 },
  { "ReadExScan",       true, "READ-EX",1, "SCAN",      0, 1,   0, 1 },
  { "ReadExScanHl",     true, "READ-EX",1, "SCAN-HL", 274, X,   0, 1 },
  { "ReadExScanEx",     true, "READ-EX",1, "SCAN-EX", 274, X,   0, 1 },
#if 0
  { "ReadExScanUp",     true, "READ-EX",1, "SCAN-UP", 266, X,   0, 1 },
  { "ReadExScanDe",     true, "READ-EX",1, "SCAN-DE", 266, X,   0, 1 },
#endif

  { "InsertRead",      false, "INSERT", 1, "READ",    266, X,   0, 1 },
  { "InsertReadEx",    false, "INSERT", 1, "READ-EX", 266, X,   0, 1 },
  { "InsertSimpleRead",false, "INSERT", 1, "S-READ",  266, X,   0, 1 },
  { "InsertDirtyRead", false, "INSERT", 1, "D-READ",  626, X,   0, 1 },
  { "InsertInsert",    false, "INSERT", 1, "INSERT",  266, X,   0, 1 },
  { "InsertUpdate",    false, "INSERT", 1, "UPDATE",  266, X,   0, 1 },
  { "InsertDelete",    false, "INSERT", 1, "DELETE",  266, X,   0, 1 },
  { "InsertScan",      false, "INSERT", 1, "SCAN",    626, X,   0, 1 },
  { "InsertScanHl",    false, "INSERT", 1, "SCAN-HL", 274, X,   0, 1 },
  { "InsertScanEx",    false, "INSERT", 1, "SCAN-EX", 274, X,   0, 1 },
#if 0
  { "InsertScanUp",    false, "INSERT",   1, "SCAN-UP", 266, X,   0, 1 },
  { "InsertScanDe",    false, "INSERT",   1, "SCAN-DE", 266, X,   0, 1 },
#endif

  { "UpdateRead",       true, "UPDATE", 2, "READ",    266, X,   0, 2 },
  { "UpdateReadEx",     true, "UPDATE", 2, "READ-EX", 266, X,   0, 2 },
  { "UpdateSimpleRead", true, "UPDATE", 2, "S-READ",  266, X,   0, 2 },
  { "UpdateDirtyRead",  true, "UPDATE", 2, "D-READ",    0, 1,   0, 2 },
  { "UpdateInsert",     true, "UPDATE", 2, "INSERT",  266, X,   0, 2 },
  { "UpdateUpdate",     true, "UPDATE", 2, "UPDATE",  266, X,   0, 2 },
  { "UpdateDelete",     true, "UPDATE", 2, "DELETE",  266, X,   0, 2 },
  { "UpdateScan",       true, "UPDATE", 2, "SCAN",      0, 1,   0, 2 },
  { "UpdateScanHl",     true, "UPDATE", 2, "SCAN-HL", 274, X,   0, 2 },
  { "UpdateScanEx",     true, "UPDATE", 2, "SCAN-EX", 274, X,   0, 2 },
#if 0
  { "UpdateScanUp",     true, "UPDATE", 2, "SCAN-UP", 266, X,   0, 2 },
  { "UpdateScanDe",     true, "UPDATE", 2, "SCAN-DE", 266, X,   0, 2 },
#endif

  { "DeleteRead",       true, "DELETE", X, "READ",    266, X, 626, X },
  { "DeleteReadEx",     true, "DELETE", X, "READ-EX", 266, X, 626, X },
  { "DeleteSimpleRead", true, "DELETE", X, "S-READ",  266, X, 626, X },
  { "DeleteDirtyRead",  true, "DELETE", X, "D-READ",    0, 1, 626, X },
  { "DeleteInsert",     true, "DELETE", X, "INSERT",  266, X, 626, X },
  { "DeleteUpdate",     true, "DELETE", X, "UPDATE",  266, X, 626, X },
  { "DeleteDelete",     true, "DELETE", X, "DELETE",  266, X, 626, X },
  { "DeleteScan",       true, "DELETE", X, "SCAN",      0, 1, 626, X },
  { "DeleteScanHl",     true, "DELETE", X, "SCAN-HL", 274, X, 626, X },
  { "DeleteScanEx",     true, "DELETE", X, "SCAN-EX", 274, X, 626, X },
#if 0
  { "DeleteScanUp",     true, "DELETE", X, "SCAN-UP", 266, X, 626, X },
  { "DeleteScanDe",     true, "DELETE", X, "SCAN-DE", 266, X, 626, X }
#endif

};

#define CHECK(a, b) { int x = a; int y = b; if (x != y) { \
  g_err  << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl << "  " \
         << x << " != " << y << endl;\
   result = NDBT_FAILED; \
  break; } }

int
runOp(HugoOperations & hugoOps,
      Ndb * pNdb,
      const char * op,
      int value){

#define C2(x) if(!(x)) {\
  g_err  << "ERR: failed on line " << __LINE__ << endl; \
  return NDBT_FAILED; }
  
  if(strcmp(op, "READ") == 0){
    C2(hugoOps.pkReadRecord(pNdb, 1, false, 1) == 0);
  } else if(strcmp(op, "READ-EX") == 0){
    C2(hugoOps.pkReadRecord(pNdb, 1, true, 1) == 0);      
  } else if(strcmp(op, "S-READ") == 0){
    C2(hugoOps.pkSimpleReadRecord(pNdb, 1, 1) == 0);      
  } else if(strcmp(op, "D-READ") == 0){
    C2(hugoOps.pkDirtyReadRecord(pNdb, 1, 1) == 0);      
  } else if(strcmp(op, "INSERT") == 0){
    C2(hugoOps.pkInsertRecord(pNdb, 1, 1, value) == 0);      
  } else if(strcmp(op, "UPDATE") == 0){
    C2(hugoOps.pkUpdateRecord(pNdb, 1, 1, value) == 0);      
  } else if(strcmp(op, "DELETE") == 0){
    C2(hugoOps.pkDeleteRecord(pNdb, 1, 1) == 0);      
  } else if(strcmp(op, "SCAN") == 0){
    C2(hugoOps.scanReadRecords(pNdb) == 0);
  } else if(strcmp(op, "SCAN-HL") == 0){
    C2(hugoOps.scanReadRecords(pNdb, NdbScanOperation::LM_Read)== 0);
  } else if(strcmp(op, "SCAN-EX") == 0){
    C2(hugoOps.scanReadRecords(pNdb, NdbScanOperation::LM_Exclusive)== 0);
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
  } else if(strcmp(op, "SCAN") == 0){
  } else if(strcmp(op, "SCAN-HL") == 0){
  } else if(strcmp(op, "SCAN-EX") == 0){
  } else {
    return NDBT_OK;
  }
  
  return hugoOps.verifyUpdatesValue(value);
}

#define TIMEOUT 100

int
setTransactionTimeout(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  int val[] = 
    { DumpStateOrd::TcSetTransactionTimeout, TIMEOUT };
  if(restarter.dumpStateAllNodes(val, 2) != 0){
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int 
runTwoTrans1(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations T1(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  const char * op1 = ctx->getProperty("op1", "NONE");
  int val1 = ctx->getProperty("val1", ~0);

  do {
    // Insert, read
    CHECK(T1.startTransaction(pNdb), 0);  
    CHECK(runOp(T1, pNdb, op1, val1), 0);
    CHECK(T1.execute_NoCommit(pNdb),  0);
    CHECK(checkVal(T1, op1, val1, 0), 0);
    
    ctx->setProperty("T1-1-Complete", 1);
    while(ctx->getProperty("T2-Complete", (Uint32)0) == 0){
      T1.refresh();
      NdbSleep_MilliSleep(10);
    }
    
    CHECK(T1.execute_Commit(pNdb), 0);
    
  } while(false);
  T1.closeTransaction(pNdb);
  
  if(result != NDBT_OK)
    return result;

  const int res3 = ctx->getProperty("res3", ~0);
  const int val3 = ctx->getProperty("val3", ~0);
  
  do {
    CHECK(T1.startTransaction(pNdb), 0);
    CHECK(runOp(T1, pNdb, "READ", 0), 0);
    CHECK(T1.execute_Commit(pNdb), res3);
    CHECK(checkVal(T1, "READ", val3, res3), 0);
  } while(false);
  T1.closeTransaction(pNdb);
  
  return result;
}

int 
runTwoTrans2(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations T2(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  const char * op2 = ctx->getProperty("op2", "NONE");
  const int res2 = ctx->getProperty("res2", ~0);
  const int val2 = ctx->getProperty("val2", ~0);

  while(ctx->getProperty("T1-1-Complete", (Uint32)0) == 0 && 
	!ctx->isTestStopped()){
    NdbSleep_MilliSleep(100);
  }

  if(!ctx->isTestStopped()){
    do {
      CHECK(T2.startTransaction(pNdb), 0);  
      CHECK(runOp(T2, pNdb, op2, val2), 0);
      CHECK(T2.execute_NoCommit(pNdb), res2);
      CHECK(checkVal(T2, op2, val2, res2), 0);
      if(res2 == 0){
	CHECK(T2.execute_Commit(pNdb), res2);
      }
    } while(false);
    T2.closeTransaction(pNdb);
  }
  
  ctx->setProperty("T2-Complete", 1);  
  
  return result;
}

int 
runInsertRecord(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  do{
    // Insert, insert 
    CHECK(hugoOps.startTransaction(pNdb), 0);  
    CHECK(hugoOps.pkInsertRecord(pNdb, 1, 1, 1), 0);
    CHECK(hugoOps.execute_Commit(pNdb), 0);    
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
    
    pt->addInitializer(new NDBT_Initializer(pt, 
					    "setTransactionTimeout", 
					    setTransactionTimeout));
    
    pt->setProperty("op1", matrix[i].op1);
    pt->setProperty("val1", matrix[i].val1);

    pt->setProperty("op2", matrix[i].op2);
    pt->setProperty("res2", matrix[i].res2);
    pt->setProperty("val2", matrix[i].val2);

    pt->setProperty("res3", matrix[i].res3);
    pt->setProperty("val3", matrix[i].val3);

    pt->addStep(new NDBT_ParallelStep(pt, 
				      matrix[i].name,
				      runTwoTrans1));
    pt->addStep(new NDBT_ParallelStep(pt, 
				      matrix[i].name,
				      runTwoTrans2));
    pt->addFinalizer(new NDBT_Finalizer(pt, 
					"runClearTable", 
					runClearTable));
    
    ts.addTest(pt);
  }

  return ts.execute(argc, argv);
}

