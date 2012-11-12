/*
   Copyright (C) 2003-2006, 2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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
  const NdbDictionary::Table * pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);

  if (strcmp(pTab->getName(), "T1") != 0) {
    g_err << "runTestBug19537: skip, table != T1" << endl;
    return NDBT_OK;
  }


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

  // Perform initial read of column start value
  NdbRecAttr* initialVal = pOp->getValue("KOL2");
  if( initialVal == NULL ) {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }
  
  // Update the column
  Uint32 valToIncWith = 1;
  check = pOp->incValue("KOL2", valToIncWith);
  if( check == -1 ) {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Perform final read of column after value
  NdbRecAttr* afterVal = pOp->getValue("KOL2");
  if( afterVal == NULL ) {
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

  Uint32 oldValue = initialVal->u_32_value();
  Uint32 newValue = afterVal->u_32_value();
  Uint32 expectedValue = oldValue + valToIncWith;
    
  if (newValue != expectedValue)
  {
    g_err << "Failed : Expected " << oldValue << "+" <<
      valToIncWith << "=" << expectedValue <<
      " but received " << newValue << endl;
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  pNdb->closeTransaction(pTrans);


  return NDBT_OK;
}

int runTestBug19537(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table * pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);

  if (strcmp(pTab->getName(), "T1") != 0) {
    g_err << "runTestBug19537: skip, table != T1" << endl;
    return NDBT_OK;
  }


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
  
  if (pOp->interpretedUpdateTuple() == -1) {
    ERR(pOp->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }
  
  
  // Primary keys
  const Uint32 pkVal = 1;
  if (pOp->equal("KOL1", pkVal) == -1) {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }
  
  // Load 64-bit constant into register 1 and
  // write from register 1 to 32-bit column KOL2
  const Uint64 reg_val = 0x0102030405060708ULL;
  Uint32 reg_ptr32[2];
  memcpy(&(reg_ptr32[0]), (Uint8*)&reg_val, sizeof(Uint32));
  memcpy(&(reg_ptr32[1]), ((Uint8*)&reg_val)+4, sizeof(Uint32));
  if (reg_ptr32[0] == 0x05060708 && reg_ptr32[1] == 0x01020304) {
    g_err << "runTestBug19537: platform is LITTLE endian" << endl;
  } else if (reg_ptr32[0] == 0x01020304 && reg_ptr32[1] == 0x05060708) {
    g_err << "runTestBug19537: platform is BIG endian" << endl;
  } else {
    g_err << "runTestBug19537: impossible platform"
          << hex << " [0]=" << reg_ptr32[0] << " [1]=" <<reg_ptr32[1] << endl;
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pOp->load_const_u64(1, reg_val) == -1 ||
      pOp->write_attr("KOL2", 1) == -1) {
    ERR(pOp->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pTrans->execute(Commit) == -1) {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Read value via a new transaction

  pTrans = pNdb->startTransaction();
  if (pTrans == NULL){
    ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }
  
  pOp = pTrans->getNdbOperation(pTab->getName());
  if (pOp == NULL) {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }
  
  Uint32 kol2 = 0x09090909;
  if (pOp->readTuple() == -1 ||
      pOp->equal("KOL1", pkVal) == -1 ||
      pOp->getValue("KOL2", (char*)&kol2) == 0) {
    ERR(pOp->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (pTrans->execute(Commit) == -1) {
    ERR(pTrans->getNdbError());
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  // Expected conversion as in C - truncate to lower (logical) word

  if (kol2 == 0x01020304) {
    g_err << "runTestBug19537: the bug manifests itself !" << endl;
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  if (kol2 != 0x05060708) {
    g_err << "runTestBug19537: impossible KOL2 " << hex << kol2 << endl;
    pNdb->closeTransaction(pTrans);
    return NDBT_FAILED;
  }

  pNdb->closeTransaction(pTrans);
  return NDBT_OK;
}


int runTestBug34107(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table * pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  const Uint32 okSize= 10000;
  const Uint32 tooBig= 30000;

  Uint32 codeBuff[tooBig];

  int i;
  for (i = 0; i <= 1; i++) {
    g_info << "bug34107:" << (i == 0 ? " small" : " too big") << endl;

    NdbConnection* pTrans = pNdb->startTransaction();
    if (pTrans == NULL){
      ERR(pNdb->getNdbError());
      return NDBT_FAILED;
    }
    
    NdbScanOperation* pOp = pTrans->getNdbScanOperation(pTab->getName());
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    if (pOp->readTuples() == -1) {
      ERR(pOp->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    /* Test kernel mechanism for dealing with too large program
     * We need to provide our own program buffer as default
     * NdbInterpretedCode buffer will not grow larger than 
     * NDB_MAX_SCANFILTER_SIZE
     */

    NdbInterpretedCode code(NULL, // Table is irrelevant
                            codeBuff,
                            tooBig); // Size of codeBuff
    
    int n = i == 0 ? okSize : tooBig;
    int k;

    for (k = 0; k < n; k++) {

      // inserts 1 word ATTRINFO

      if (code.interpret_exit_ok() == -1) {
        ERR(code.getNdbError());
        pNdb->closeTransaction(pTrans);
        return NDBT_FAILED;
      }
    }

    if (code.finalise() != 0)
    {
      ERR(code.getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (pOp->setInterpretedCode(&code) != 0)
    {
      ERR(pOp->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    if (pTrans->execute(NoCommit) == -1) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    int ret;
    while ((ret = pOp->nextResult()) == 0)
      ;
    g_info << "ret=" << ret << " err=" << pOp->getNdbError().code << endl;

    if (i == 0 && ret != 1) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (i == 1 && ret != -1) {
      g_err << "unexpected big filter success" << endl;
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    if (i == 1 && pOp->getNdbError().code != 874) {
      g_err << "unexpected big filter error code, wanted 874" << endl;
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    pNdb->closeTransaction(pTrans);
  }

  return NDBT_OK;
}

static char pkIdxName[256];

int
createPkIndex(NDBT_Context* ctx, NDBT_Step* step){
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);

  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);
  bool logged = ctx->getProperty("LoggedIndexes", (Uint32)0);
  bool noddl= ctx->getProperty("NoDDL");

  // Create index
  BaseString::snprintf(pkIdxName, 255, "IDC_PK_%s", pTab->getName());
  if (orderedIndex)
    ndbout << "Creating " << ((logged)?"logged ": "temporary ") << "ordered index "
	   << pkIdxName << " (";
  else
    ndbout << "Creating " << ((logged)?"logged ": "temporary ") << "unique index "
	   << pkIdxName << " (";

  NdbDictionary::Index pIdx(pkIdxName);
  pIdx.setTable(pTab->getName());
  if (orderedIndex)
    pIdx.setType(NdbDictionary::Index::OrderedIndex);
  else
    pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
  for (int c = 0; c< pTab->getNoOfColumns(); c++){
    const NdbDictionary::Column * col = pTab->getColumn(c);
    if(col->getPrimaryKey()){
      pIdx.addIndexColumn(col->getName());
      ndbout << col->getName() <<" ";
    }
  }

  pIdx.setStoredIndex(logged);
  ndbout << ") ";
  if (noddl)
  {
    const NdbDictionary::Index* idx= pNdb->
      getDictionary()->getIndex(pkIdxName, pTab->getName());

    if (!idx)
    {
      ndbout << "Failed - Index does not exist and DDL not allowed" << endl;
      ERR(pNdb->getDictionary()->getNdbError());
      return NDBT_FAILED;
    }
    else
    {
      // TODO : Check index definition is ok
    }
  }
  else
  {
    if (pNdb->getDictionary()->createIndex(pIdx) != 0){
      ndbout << "FAILED!" << endl;
      const NdbError err = pNdb->getDictionary()->getNdbError();
      ERR(err);
      return NDBT_FAILED;
    }
  }

  ndbout << "OK!" << endl;
  return NDBT_OK;
}

int
createPkIndex_Drop(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);

  bool noddl= ctx->getProperty("NoDDL");

  // Drop index
  if (!noddl)
  {
    ndbout << "Dropping index " << pkIdxName << " ";
    if (pNdb->getDictionary()->dropIndex(pkIdxName,
                                         pTab->getName()) != 0){
      ndbout << "FAILED!" << endl;
      ERR(pNdb->getDictionary()->getNdbError());
      return NDBT_FAILED;
    } else {
      ndbout << "OK!" << endl;
    }
  }

  return NDBT_OK;
}

#define CHK_RET_FAILED(x) if (!(x)) { ndbout_c("Failed on line: %u", __LINE__); return NDBT_FAILED; }

int
runInterpretedUKLookup(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table * pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary * dict = pNdb->getDictionary();

  const NdbDictionary::Index* pIdx= dict->getIndex(pkIdxName, pTab->getName());
  CHK_RET_FAILED(pIdx != 0);

  const NdbRecord * pRowRecord = pTab->getDefaultRecord();
  CHK_RET_FAILED(pRowRecord != 0);
  const NdbRecord * pIdxRecord = pIdx->getDefaultRecord();
  CHK_RET_FAILED(pIdxRecord != 0);

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 * pRow = new Uint8[len];
  bzero(pRow, len);

  HugoCalculator calc(* pTab);
  calc.equalForRow(pRow, pRowRecord, 0);

  NdbTransaction* pTrans = pNdb->startTransaction();
  CHK_RET_FAILED(pTrans != 0);

  NdbInterpretedCode code;
  code.interpret_exit_ok();
  code.finalise();

  NdbOperation::OperationOptions opts;
  bzero(&opts, sizeof(opts));
  opts.optionsPresent = NdbOperation::OperationOptions::OO_INTERPRETED;
  opts.interpretedCode = &code;

  const NdbOperation * pOp = pTrans->readTuple(pIdxRecord, (char*)pRow,
                                               pRowRecord, (char*)pRow,
                                               NdbOperation::LM_Read,
                                               0,
                                               &opts,
                                               sizeof(opts));
  CHK_RET_FAILED(pOp);
  int res = pTrans->execute(Commit, AbortOnError);

  CHK_RET_FAILED(res == 0);

  delete [] pRow;

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
TESTCASE("Bug19537",
         "Test big-endian write_attr of 32 bit integer\n"){
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestBug19537);
  FINALIZER(runClearTable);
}
TESTCASE("Bug34107",
         "Test too big scan filter (error 874)\n"){
  INITIALIZER(runLoadTable);
  INITIALIZER(runTestBug34107);
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
TESTCASE("InterpretedUKLookup", "")
{
  INITIALIZER(runLoadTable);
  INITIALIZER(createPkIndex);
  INITIALIZER(runInterpretedUKLookup);
  INITIALIZER(createPkIndex_Drop);
}
NDBT_TESTSUITE_END(testInterpreter);

int main(int argc, const char** argv){
  ndb_init();
  //  TABLE("T1");
  NDBT_TESTSUITE_INSTANCE(testInterpreter);
  return testInterpreter.execute(argc, argv);
}


