/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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
  Uint64 m_gci;
  Uint32 m_author;
  BaseString m_str;
  SavedRecord(Uint64 _gci, Uint32 _author, BaseString _str){
    m_gci = _gci; 
    m_author = _author;
    m_str.assign(_str); 
  }
  SavedRecord(){
    m_gci = 0;
    m_str = "";
  }
};
Vector<SavedRecord> savedRecords;
Uint64 highestExpectedGci;

#define CHECK(b) if (!(b)) { \
  ndbout << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  break; }

static
int
maybeExtraBits(Ndb* ndb, NdbDictionary::Table& tab, int when, void* arg)
{
  switch(when){
  case 0: // Before
    break;
  case 1: // After
    return 0;
  default:
    return 0;
  }

  bool useExtendedBits = ((ndb_rand() % 5) != 0);
  Uint32 numGciBits= ndb_rand() % 32;      /* 0 -> 31 */
  Uint32 numAuthorBits = ndb_rand() % 32;  /* 0 -> 31 */

  if (useExtendedBits && (numGciBits || numAuthorBits))
  {
    ndbout_c("Creating table %s with %u extra Gci and %u extra Author bits",
             tab.getName(), numGciBits, numAuthorBits);
    tab.setExtraRowGciBits(numGciBits);
    tab.setExtraRowAuthorBits(numAuthorBits);
  }
  else
  {
    ndbout_c("Table has no extra bits");
  }

  return 0;
}

int runDropTable(NDBT_Context* ctx, NDBT_Step* step)
{
  GETNDB(step)->getDictionary()->dropTable(ctx->getTab()->getName());
  return NDBT_OK;
}

int runCreateTable(NDBT_Context* ctx, NDBT_Step* step)
{

  runDropTable(ctx, step);

  /* Use extra proc to control whether we have extra bits */
  if (NDBT_Tables::createTable(GETNDB(step),
                               ctx->getTab()->getName(),
                               false, false,
                               maybeExtraBits) == NDBT_OK)
  {
    ctx->setTab(GETNDB(step)->
                getDictionary()->
                getTable(ctx->getTab()->getName()));
    return NDBT_OK;
  }
  return NDBT_FAILED;
}

int runInsertRememberGci(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  HugoOperations hugoOps(*ctx->getTab());
  HugoCalculator hugoCalc(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  int i = 0;

  ndbout_c("Inserting %u records", records);
  Uint64 minGci = ~Uint64(0);
  Uint64 maxGci = 0;
  Uint32 numAuthorBits = ctx->getTab()->getExtraRowAuthorBits();
  Uint32 authorMask = (1 << numAuthorBits) -1;
  ndbout_c("numAuthor bits is %u, mask is %x",
           numAuthorBits, authorMask);

  while(i < records){
    // Insert record and read it in same transaction
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    CHECK(hugoOps.pkInsertRecord(pNdb, i) == 0);
    if (hugoOps.execute_NoCommit(pNdb) != 0){
      ndbout << "Could not insert record " << i << endl;
      result = NDBT_FAILED;
      break;
    }
    /* Set the author column (if present) */
    Uint32 authorVal = 0;
    if (ctx->getTab()->getExtraRowAuthorBits() > 0)
    {
      authorVal = (ndb_rand() & authorMask);
      /* Pain here due to need to use NdbRecord */
      char rowBuff[NDB_MAX_TUPLE_SIZE];
      const NdbDictionary::Table* tab = ctx->getTab();
      CHECK(hugoCalc.setValues((Uint8*) rowBuff, tab->getDefaultRecord(),
                               i, 0) == 0);
      NdbOperation::SetValueSpec setValueSpec;
      setValueSpec.column = NdbDictionary::Column::ROW_AUTHOR;
      setValueSpec.value = &authorVal;
      NdbOperation::OperationOptions opts;
      opts.optionsPresent= NdbOperation::OperationOptions::OO_SETVALUE;
      opts.extraSetValues= &setValueSpec;
      opts.numExtraSetValues = 1;

      const NdbOperation* update = hugoOps.getTransaction()->
        updateTuple(tab->getDefaultRecord(), rowBuff,
                    tab->getDefaultRecord(), rowBuff,
                    NULL, /* mask */
                    &opts,
                    sizeof(opts));
      CHECK(update != NULL);
    }
    /* Read row back */
    CHECK(hugoOps.pkReadRecord(pNdb, i) == 0);
    if (hugoOps.execute_Commit(pNdb) != 0){
      ndbout << "Did not find record in DB " << i << endl;
      result = NDBT_FAILED;
      break;
    }
    Uint64 gci;
    CHECK(hugoOps.getTransaction()->getGCI(&gci) == 0);

    if (gci < minGci)
      minGci = gci;
    if (gci > maxGci)
      maxGci = gci;

    savedRecords.push_back(SavedRecord(gci,
                                       authorVal,
                                       hugoOps.getRecordStr(0)));

    CHECK(hugoOps.closeTransaction(pNdb) == 0);
    i++;
    /* Sleep so that records will have > 1 GCI between them */
    NdbSleep_MilliSleep(10);
  };

  ndbout_c("  Inserted records from gci %x/%x to gci %x/%x",
           (Uint32) (minGci >> 32), (Uint32) (minGci & 0xffffffff),
           (Uint32) (maxGci >> 32), (Uint32) (maxGci & 0xffffffff));

  highestExpectedGci = maxGci;

  return result;
}

int runRestartAll(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  NdbRestarter restarter;

  ndbout_c("Restart of all nodes");

  // Restart cluster with abort
  if (restarter.restartAll(false, false, true) != 0){
    ctx->stopTest();
    return NDBT_FAILED;
  }

  if (restarter.waitClusterStarted(300) != 0){
    return NDBT_FAILED;
  }
  
  if (pNdb->waitUntilReady() != 0){
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runRestartOneInitial(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2)
    return NDBT_OK;

  /* We don't restart the Master as we need to know a
   * non-restarted node to reliably get the restartGci
   * afterwards!
   * Should be no real reason not to restart the master.
   */
  int node = restarter.getRandomNotMasterNodeId(rand());
  ndbout_c("Restarting node %u initial", node);

  if (restarter.restartOneDbNode(node,
                                 true,  /* Initial */
                                 false, /* Nostart */
                                 true)  /* Abort */
      != 0)
  {
    ctx->stopTest();
    return NDBT_FAILED;
  }

  if (restarter.waitClusterStarted(300) != 0){
    return NDBT_FAILED;
  }

  if (pNdb->waitUntilReady() != 0){
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runRestartGciControl(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  UtilTransactions utilTrans(*ctx->getTab());
  
  // Wait until we have enough records in db
  int count = 0;
  while (count < records){
    if (utilTrans.selectCount(pNdb, 64, &count) != 0){
      ctx->stopTest();
      return NDBT_FAILED;
    }
    NdbSleep_MilliSleep(10);
  }

  return runRestartAll(ctx,step);
}

int runDetermineRestartGci(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  Uint32 restartGci;
  int res = pNdb->getDictionary()->getRestartGCI(&restartGci);
  if (res != 0)
  {
    ndbout << "Failed to retrieve restart gci" << endl;
    ndbout << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  ndbout_c("Restart GCI is %u (0x%x)",
           restartGci, restartGci);

  ndbout_c("Highest expected GCI was %x/%x",
           (Uint32) (highestExpectedGci >> 32),
           (Uint32) (highestExpectedGci & 0xffffffff));

  highestExpectedGci = ((Uint64) restartGci) << 32 | 0xffffffff;
  ndbout_c("Resetting Highest expected GCI to align with restart Gci (%x/%x)",
           (Uint32) (highestExpectedGci >> 32),
           (Uint32) (highestExpectedGci & 0xffffffff));
  return NDBT_OK;
}

int runRequireExact(NDBT_Context* ctx, NDBT_Step* step){
  ctx->incProperty("ExactGCI");
  return NDBT_OK;
}

int runVerifyInserts(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  Ndb* pNdb = GETNDB(step);
  UtilTransactions utilTrans(*ctx->getTab());
  HugoOperations hugoOps(*ctx->getTab());
  NdbRestarter restarter;
  Uint32 extraGciBits = ctx->getTab()->getExtraRowGciBits();
  Uint32 firstSaturatedValue = (1 << extraGciBits) -1;

  int count = 0;
  if (utilTrans.selectCount(pNdb, 64, &count) != 0){
    return NDBT_FAILED;
  }

  // RULE1: The vector with saved records should have exactly as many 
  // records with lower or same gci as there are in DB
  int recordsWithLowerOrSameGci = 0;
  unsigned i; 
  for (i = 0; i < savedRecords.size(); i++){
    if (savedRecords[i].m_gci <= highestExpectedGci)
      recordsWithLowerOrSameGci++;
  }
  if (recordsWithLowerOrSameGci != count){
    ndbout << "ERR: Wrong number of expected records" << endl;
    result = NDBT_FAILED;
  }

  bool exactGCIonly = ctx->getProperty("ExactGCI", (unsigned) 0);

  // RULE2: The records found in db should have same or lower 
  // gci as in the vector
  int recordsWithIncorrectGci = 0;
  int recordsWithRoundedGci = 0;
  int recordsWithIncorrectAuthor = 0;
  for (i = 0; i < savedRecords.size(); i++){
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    /* First read of row to check contents */
    CHECK(hugoOps.pkReadRecord(pNdb, i) == 0);
    /* Second read of row to get GCI */
    NdbTransaction* trans = hugoOps.getTransaction();
    NdbOperation* readOp = trans->getNdbOperation(ctx->getTab());
    CHECK(readOp != NULL);
    CHECK(readOp->readTuple() == 0);
    CHECK(hugoOps.equalForRow(readOp, i) == 0);
    NdbRecAttr* rowGci = readOp->getValue(NdbDictionary::Column::ROW_GCI64);
    NdbRecAttr* rowAuthor = readOp->getValue(NdbDictionary::Column::ROW_AUTHOR);
    CHECK(rowGci != NULL);
    CHECK(rowAuthor != NULL);
    if (hugoOps.execute_Commit(pNdb) != 0){
      // Record was not found in db'

      // Check record gci
      if (savedRecords[i].m_gci <= highestExpectedGci) {
	ndbout << "ERR: Record "<<i<<" should have existed" << endl;
	result = NDBT_FAILED;
      }
      else
      {
        /* It didn't exist, but that was expected.
         * Let's disappear it, so that it doesn't cause confusion
         * after further restarts.
         */
        savedRecords[i].m_gci = (Uint64(1) << 63) -1; // Big number
      }
    } else {
      // Record was found in db
      BaseString str = hugoOps.getRecordStr(0);
      // Check record string
      if (!(savedRecords[i].m_str == str)){
	ndbout << "ERR: Record "<<i<<" str did not match "<< endl;
	result = NDBT_FAILED;
      }
      // Check record gci in range
      Uint64 expectedRecordGci = savedRecords[i].m_gci;
      if (expectedRecordGci > highestExpectedGci){
	ndbout << "ERR: Record "<<i<<" should not have existed" << endl;
	result = NDBT_FAILED;
      }
      bool expectRounding = (expectedRecordGci & 0xffffffff) >= firstSaturatedValue;
      Uint64 expectedRoundedGci = (expectedRecordGci | 0xffffffff);
      Uint64 readGci = rowGci->u_64_value();
      Uint64 expectedRead = (expectRounding)?expectedRoundedGci :
        expectedRecordGci;
      // Check record gci is exactly correct
      if (expectedRead != readGci){
        if ((!exactGCIonly) &&
            (expectedRoundedGci == readGci))
        {
          /* Record rounded, though bits can be represented
           * presumably due to Redo gci truncation
           */
          recordsWithRoundedGci++;
        }
        else
        {
          ndbout_c("ERR: Record %u should have GCI %x/%x, but has "
                   "%x/%x.",
                   i,
                   (Uint32) (expectedRead >> 32),
                   (Uint32) (expectedRead & 0xffffffff),
                   (Uint32) (readGci >> 32),
                   (Uint32) (readGci & 0xffffffff));
          recordsWithIncorrectGci++;
          result = NDBT_FAILED;
        }
      }

      // Check author value is correct.
      Uint32 expectedAuthor = savedRecords[i].m_author;

      if (rowAuthor->u_32_value() != expectedAuthor)
      {
        ndbout_c("ERR: Record %u should have Author %d, but has %d.",
                 i,
                 expectedAuthor,
                 rowAuthor->u_32_value());
        recordsWithIncorrectAuthor++;
        result = NDBT_FAILED;
      }
    }

    CHECK(hugoOps.closeTransaction(pNdb) == 0);    
  }
  

  ndbout << "There are " << count << " records in db" << endl;
  ndbout << "There are " << savedRecords.size() 
	 << " records in vector" << endl;

  ndbout_c("There are %u records with lower or same gci than %x/%x",
           recordsWithLowerOrSameGci,
           (Uint32)(highestExpectedGci >> 32),
           (Uint32)(highestExpectedGci & 0xffffffff));
  
  ndbout_c("There are %u records with rounded Gcis.  Exact GCI flag is %u",
           recordsWithRoundedGci, exactGCIonly);

  ndbout << "There are " << recordsWithIncorrectGci
         << " records with incorrect Gci on recovery." << endl;

  ndbout << "There are " << recordsWithIncorrectAuthor
         << " records with incorrect Author on recovery." << endl;

  return result;
}

int runClearGlobals(NDBT_Context* ctx, NDBT_Step* step){
  savedRecords.clear();
  highestExpectedGci = 0;
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


int runLoadTable(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records, 512, false, 0, true) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runNodeInitialRestarts(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  const Uint32 numRestarts = 4;
  for (Uint32 nr = 0; nr < numRestarts; nr++)
  {
    if (ctx->isTestStopped())
    {
      return NDBT_OK;
    }
    int nodeId = restarter.getNode(NdbRestarter::NS_RANDOM);
    ndbout_c("Restarting node %u", nodeId);

    if (restarter.restartOneDbNode2(nodeId, NdbRestarter::NRRF_INITIAL) != 0)
    {
      ndbout_c("Error restarting node");
      ctx->stopTest();
      return NDBT_FAILED;
    }

    if (restarter.waitClusterStarted(300) != 0)
    {
      ctx->stopTest();
      return NDBT_FAILED;
    }

    if (GETNDB(step)->waitUntilReady() != 0)
    {
      ctx->stopTest();
      return NDBT_FAILED;
    }
  }

  ctx->stopTest();

  return NDBT_OK;
}

int runUpdateVerifyGCI(NDBT_Context* ctx, NDBT_Step* step)
{
  HugoOperations hugoOps(*ctx->getTab());
  HugoCalculator hugoCalc(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);

  /* Loop, updating the first record in the table, and checking
   * that it has the GCI it should
   */
  Uint64 loopCount = 0;
  Uint64 distinctCount = 0;
  Uint64 expectedGCI = 0;
  Uint64 lastGoodReadGCI = 0;
  Uint32 extraGciBits = ctx->getTab()->getExtraRowGciBits();
  Uint32 firstSaturatedValue = (1 << extraGciBits) -1;
  ndbout_c("Extra GCI bits : %u, firstSaturatedValue : %u",
           extraGciBits,
           firstSaturatedValue);
  int result = NDBT_OK;
  while (!ctx->isTestStopped())
  {
    CHECK(hugoOps.startTransaction(pNdb) == 0);
    /* Define a read op to get the 'existing' GCI */
    NdbTransaction* trans = hugoOps.getTransaction();
    CHECK(hugoOps.pkReadRecord(pNdb,
                               0,
                               1) == 0);
    NdbOperation* readOp = trans->getNdbOperation(ctx->getTab());
    CHECK(readOp != NULL);
    CHECK(readOp->readTuple() == 0);
    CHECK(hugoOps.equalForRow(readOp, 0) == 0);
    NdbRecAttr* rowGci = readOp->getValue(NdbDictionary::Column::ROW_GCI64);
    CHECK(rowGci != NULL);

    /* Define an update op to set the next GCI */
    CHECK(hugoOps.pkUpdateRecord(pNdb, 0, 1, (int)(loopCount+1)) == 0);

    if (hugoOps.execute_Commit(pNdb) != 0)
    {
      if (hugoOps.getNdbError().classification ==
          NdbError::NodeRecoveryError)
      {
        hugoOps.closeTransaction(pNdb);
        ndbout_c("Temporary error at loopCount %llu", loopCount);
        continue;
      }

      ndbout << "Error executing : " << hugoOps.getNdbError() << endl;
      return NDBT_FAILED;
    }

    /* First check the data is as expected */
    CHECK(hugoCalc.verifyRowValues(&hugoOps.get_row(0)) == 0);
    CHECK((Uint64)hugoCalc.getUpdatesValue(&hugoOps.get_row(0)) == loopCount);
    //ndbout_c("Updates value is %u", hugoCalc.getUpdatesValue(&hugoOps.get_row(0)));

    Uint64 committedGCI;
    CHECK(trans->getGCI(&committedGCI) == 0);
    Uint32 gci_lo = Uint32(committedGCI & 0xffffffff);

    Uint64 saturatedCommittedGCI = (gci_lo >= firstSaturatedValue) ?
      committedGCI | 0xffffffff : committedGCI;
    Uint64 rowGCI64 = rowGci->u_64_value();

//    ndbout_c("Read row GCI64 %x/%x.  Committed GCI64 : %x/%x.  Saturated GCI64 :%x/%x Last good read : %x/%x",
//             Uint32(rowGCI64 >> 32),
//             Uint32(rowGCI64 & 0xffffffff),
//             Uint32(committedGCI >> 32),
//             Uint32(committedGCI & 0xffffffff),
//             Uint32(saturatedCommittedGCI >> 32),
//             Uint32(saturatedCommittedGCI & 0xffffffff),
//             Uint32(lastGoodReadGCI >> 32),
//             Uint32(lastGoodReadGCI & 0xffffffff));


    if (rowGCI64 < lastGoodReadGCI)
    {
      ndbout_c("ERROR : Read row GCI value (%x/%x) lower than previous value (%x/%x)",
               (Uint32) (rowGCI64 >> 32),
               (Uint32) (rowGCI64 & 0xffffffff),
               Uint32(lastGoodReadGCI >> 32),
               Uint32(lastGoodReadGCI & 0xffffffff));
    }
    /* We certainly should not read a committed GCI value that's
     * bigger than the read's commit-point GCI
     */
    if (saturatedCommittedGCI < rowGCI64)
    {
      ndbout_c("ERROR : Saturated committed GCI (%x/%x) lower than actual read GCI (%x/%x)",
               Uint32(saturatedCommittedGCI >>32),
               Uint32(saturatedCommittedGCI & 0xffffffff),
               (Uint32) (rowGCI64 >> 32),
               (Uint32) (rowGCI64 & 0xffffffff));
    }
    /* If we've read a committed GCI then we should certainly not
     * be committing at lower values
     */
    if (saturatedCommittedGCI < lastGoodReadGCI)
    {
      ndbout_c("ERROR : Saturated committed GCI (%x/%x) lower than a previously"
               "read GCI (%x/%x)",
               Uint32(saturatedCommittedGCI >>32),
               Uint32(saturatedCommittedGCI & 0xffffffff),
               Uint32(lastGoodReadGCI >> 32),
               Uint32(lastGoodReadGCI & 0xffffffff));
    };
    /* If we've previously had a particular committed GCI then we
     * should certainly not now have a lower committed GCI
     */
    if (saturatedCommittedGCI < expectedGCI)
    {
      ndbout_c("ERROR : Saturated committed GCI (%x/%x) lower than expected GCI"
               " (%x/%x)",
               Uint32(saturatedCommittedGCI >>32),
               Uint32(saturatedCommittedGCI & 0xffffffff),
               Uint32(expectedGCI >> 32),
               Uint32(expectedGCI & 0xffffffff));
    }

    if (loopCount > 0)
    {
      if (rowGCI64 != expectedGCI)
      {
        ndbout_c("MISMATCH : Expected GCI of %x/%x, but found %x/%x",
                 (Uint32) (expectedGCI >> 32),
                 (Uint32) (expectedGCI & 0xffffffff),
                 (Uint32) (rowGCI64 >> 32),
                 (Uint32) (rowGCI64 & 0xffffffff));
        ndbout_c("At loopcount %llu", loopCount);
        ndbout_c("Last good read GCI %x/%x",
                 Uint32(lastGoodReadGCI >> 32),
                 Uint32(lastGoodReadGCI & 0xffffffff));
        ndbout_c("Read committed GCI : %x/%x",
                 Uint32(saturatedCommittedGCI >>32),
                 Uint32(saturatedCommittedGCI & 0xffffffff));
        ndbout_c("Transaction coordinator node : %u",
                 trans->getConnectedNodeId());
        return NDBT_FAILED;
      }

      if (saturatedCommittedGCI != expectedGCI)
      {
        distinctCount++;
      }
    }

    expectedGCI = saturatedCommittedGCI;
    lastGoodReadGCI = rowGCI64;

    hugoOps.closeTransaction(pNdb);
    loopCount++;

    /* Sleep to avoid excessive updating */
    NdbSleep_MilliSleep(10);
  }

  ndbout_c("%llu updates with %llu distinct GCI values",
           loopCount,
           distinctCount);

  return result;
}

NDBT_TESTSUITE(testRestartGci);
TESTCASE("InsertRestartGci", 
	 "Verify that only expected records are still in NDB\n"
	 "after a restart" ){
  INITIALIZER(runCreateTable);
  INITIALIZER(runClearGlobals);
  INITIALIZER(runInsertRememberGci);
  INITIALIZER(runRestartGciControl);
  INITIALIZER(runDetermineRestartGci);
  TC_PROPERTY("ExactGCI", Uint32(0)); /* Recovery from Redo == inexact low word */
  VERIFIER(runVerifyInserts);
  /* Restart again - LCP after first restart will mean that this
   * time we recover from LCP, not Redo
   */
  VERIFIER(runRestartAll);
  VERIFIER(runDetermineRestartGci);
  VERIFIER(runVerifyInserts);  // Check GCIs again
  /* Restart again - one node, initial.  This will check
   * COPYFRAG behaviour
   */
  VERIFIER(runRestartOneInitial);
  VERIFIER(runVerifyInserts);  // Check GCIs again
  VERIFIER(runClearTable);
  /* Re-fill table with records, will just be in Redo
   * Then restart, testing COPYFRAG behaviour with
   * non #ffff... low word
   */
  VERIFIER(runClearGlobals);
  VERIFIER(runInsertRememberGci);
  VERIFIER(runRestartOneInitial);
  /* Require exact GCI match from here - no Redo messing it up */
  VERIFIER(runRequireExact);
  VERIFIER(runVerifyInserts);
  /* Now-restart all nodes - all inserts should be
   * in LCP, and should be restored correctly
   */
  VERIFIER(runRestartAll);
  VERIFIER(runDetermineRestartGci);
  VERIFIER(runVerifyInserts);
  FINALIZER(runClearTable);
  FINALIZER(runDropTable);
}
TESTCASE("InitialNodeRestartUpdate",
         "Check that initial node restart (copyfrag) does "
         "not affect GCI recording")
{
  INITIALIZER(runCreateTable);
  INITIALIZER(runLoadTable);
  STEP(runNodeInitialRestarts);
  STEP(runUpdateVerifyGCI);
  FINALIZER(runClearTable);
  FINALIZER(runDropTable);
}
NDBT_TESTSUITE_END(testRestartGci)

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testRestartGci);
  testRestartGci.setCreateTable(false);
  return testRestartGci.execute(argc, argv);
}

template class Vector<SavedRecord>;
