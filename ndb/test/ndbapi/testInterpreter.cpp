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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <Vector.hpp>
#include <random.h>
#include <NdbTick.h>


#define CHECK(b) if (!(b)) { \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int batchSize = ctx->getProperty("BatchSize", 1);
  
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.pkDelRecords(GETNDB(step),  records, batchSize) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runTestIncValue64(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  //  NDBT_Table* pTab = ctx->getTab();
  //Ndb* pNdb = GETNDB(step);

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.pkInterpretedUpdateRecords(GETNDB(step), 
					   records) != 0){
    return NDBT_FAILED;
  }

  // Verify the update  
  if (hugoTrans.pkReadRecords(GETNDB(step), 
			      records) != 0){
    return NDBT_FAILED;
  }
  
  return NDBT_OK;

}

int runTestIncValue32(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  const NdbDictionary::Table * pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);


 NdbConnection* pTrans = pNdb->startTransaction();
 if (pTrans == NULL){
   ERR(pNdb->getNdbError());
   return NDBT_FAILED;
 }
 
 NdbOperation* pOp = pTrans->getNdbOperation(pTab->getName());
 if (pOp == NULL) {
   ERR(pTrans->getNdbError());
   pNdb->closeTransaction(pTrans);
   return NDBT_FAILED;
 }
 
 int check = pOp->interpretedUpdateTuple();
 if( check == -1 ) {
   ERR(pTrans->getNdbError());
   pNdb->closeTransaction(pTrans);
   return NDBT_FAILED;
 }


 // Primary keys
 Uint32 pkVal = 1;
 check = pOp->equal("KOL1", pkVal );
 if( check == -1 ) {
   ERR(pTrans->getNdbError());
   pNdb->closeTransaction(pTrans);
   return NDBT_FAILED;
 }

 // Attributes

 // Update column
 Uint32 valToIncWith = 1;
 check = pOp->incValue("KOL2", valToIncWith);
 if( check == -1 ) {
   ERR(pTrans->getNdbError());
   pNdb->closeTransaction(pTrans);
   return NDBT_FAILED;
 }

  NdbRecAttr* valueRec = pOp->getValue("KOL2");
  if( valueRec == NULL ) {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }
  
  check = pTrans->execute(Commit);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  Uint32 value = valueRec->u_32_value();
    
  pNdb->closeTransaction(pTrans);


  return NDBT_OK;
}


NDBT_TESTSUITE(testInterpreter);
TESTCASE("IncValue32", 
	 "Test incValue for 32 bit integer\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestIncValue32);
  FINALIZER(runClearTable);
}
TESTCASE("IncValue64", 
	 "Test incValue for 64 bit integer\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestIncValue64);
  FINALIZER(runClearTable);
}
#if 0
TESTCASE("MaxTransactions", 
	 "Start transactions until no more can be created\n"){ 
  INITIALIZER(runTestMaxTransaction);
}
TESTCASE("MaxOperations", 
	"Get operations until no more can be created\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestMaxOperations);
  FINALIZER(runClearTable);
}
TESTCASE("MaxGetValue", 
	"Call getValue loads of time\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestGetValue);
  FINALIZER(runClearTable);
}
TESTCASE("MaxEqual", 
	"Call equal loads of time\n"){ 
  INITIALIZER(runTestEqual);
}
TESTCASE("DeleteNdb", 
	"Make sure that a deleted Ndb object is properly deleted\n"
	"and removed from transporter\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestDeleteNdb);
  FINALIZER(runClearTable);
}
TESTCASE("WaitUntilReady", 
	"Make sure you get an error message when calling waitUntilReady\n"
	"without an init'ed Ndb\n"){ 
  INITIALIZER(runTestWaitUntilReady);
}
TESTCASE("GetOperationNoTab", 
	"Call getNdbOperation on a table that does not exist\n"){ 
  INITIALIZER(runGetNdbOperationNoTab);
}
TESTCASE("MissingOperation", 
	"Missing operation request(insertTuple) should give an error code\n"){ 
  INITIALIZER(runMissingOperation);
}
TESTCASE("GetValueInUpdate", 
	"Test that it's not possible to perform getValue in an update\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runGetValueInUpdate);
  FINALIZER(runClearTable);
}
TESTCASE("UpdateWithoutKeys", 
	"Test that it's not possible to perform update without setting\n"
	 "PKs"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runUpdateWithoutKeys);
  FINALIZER(runClearTable);
}
TESTCASE("UpdateWithoutValues", 
	"Test that it's not possible to perform update without setValues\n"){ 
  INITIALIZER(runLoadTable);
  INITIALIZER(runUpdateWithoutValues);
  FINALIZER(runClearTable);
}
TESTCASE("NdbErrorOperation", 
	 "Test that NdbErrorOperation is properly set"){
  INITIALIZER(runCheckGetNdbErrorOperation);
}
#endif
NDBT_TESTSUITE_END(testInterpreter);

int main(int argc, const char** argv){
  ndb_init();
  //  TABLE("T1");
  return testInterpreter.execute(argc, argv);
}


