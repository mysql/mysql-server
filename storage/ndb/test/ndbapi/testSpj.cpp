/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <NDBT_Test.hpp>
#include <NDBT_ReturnCodes.h>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <Bitmask.hpp>
#include <random.h>
#include <HugoQueryBuilder.hpp>
#include <HugoQueries.hpp>
#include <NdbSchemaCon.hpp>
#include <ndb_version.h>

static int faultToInject = 0;

enum faultsToInject {
  FI_START = 17001,
  FI_END = 17521
};

int
runLoadTable(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
runClearTable(NDBT_Context* ctx, NDBT_Step* step)
{
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable(GETNDB(step)) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

static
void
addMask(NDBT_Context* ctx, Uint32 val, const char * name)
{
  Uint32 oldValue = 0;
  do
  {
    oldValue = ctx->getProperty(name);
    Uint32 newValue = oldValue | val;
    if (ctx->casProperty(name, oldValue, newValue) == oldValue)
      return;
    NdbSleep_MilliSleep(5);
  } while (true);
}

int
runLookupJoin(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int joinlevel = ctx->getProperty("JoinLevel", 3);
  int records = ctx->getNumRecords();
  int queries = records/joinlevel;
  int until_stopped = ctx->getProperty("UntilStopped", (Uint32)0);
  Uint32 stepNo = step->getStepNo();

  int i = 0;
  HugoQueryBuilder qb(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_LOOKUP);
  qb.setJoinLevel(joinlevel);
  const NdbQueryDef * query = qb.createQuery();
  HugoQueries hugoTrans(*query);
  while ((i<loops || until_stopped) && !ctx->isTestStopped())
  {
    g_info << i << ": ";
    if (hugoTrans.runLookupQuery(GETNDB(step), queries))
    {
      g_info << endl;
      return NDBT_FAILED;
    }
    addMask(ctx, (1 << stepNo), "Running");
    i++;
  }
  g_info << endl;
  return NDBT_OK;
}

int
runLookupJoinError(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int joinlevel = ctx->getProperty("JoinLevel", 8);
  int records = ctx->getNumRecords();
  int queries = records/joinlevel;
  int until_stopped = ctx->getProperty("UntilStopped", (Uint32)0);
  Uint32 stepNo = step->getStepNo();

  int i = 0;
  HugoQueryBuilder qb(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_LOOKUP);
  qb.setJoinLevel(joinlevel);
  const NdbQueryDef * query = qb.createQuery();
  HugoQueries hugoTrans(*query);

  NdbRestarter restarter;
  int lookupFaults[] = {
      7240,        // DIGETNODESREQ returns error 
      17001, 17005, 17006, 17008,
      17012, // testing abort in :execDIH_SCAN_TAB_CONF
      17013, // Simulate DbspjErr::InvalidRequest
      17020, 17021, 17022, // lookup_send() encounter dead node -> NodeFailure
      17030, 17031, 17032, // LQHKEYREQ reply is LQHKEYREF('Invalid..')
      17040, 17041, 17042, // lookup_parent_row -> OutOfQueryMemory
      17050, 17051, 17052, 17053, // parseDA -> outOfSectionMem
      17060, 17061, 17062, 17063, // scanIndex_parent_row -> outOfSectionMem
      17070, 17071, 17072, // lookup_send.dupsec -> outOfSectionMem
      17080, 17081, 17082, // lookup_parent_row -> OutOfQueryMemory
      17120, 17121, // execTRANSID_AI -> OutOfRowMemory
      17130,        // sendSignal(DIH_SCAN_GET_NODES_REQ)  -> import() failed
      7234,         // sendSignal(DIH_SCAN_GET_NODES_CONF) -> import() failed (DIH)
      17510,        // random failure when allocating section memory
      17520, 17521  // failure (+random) from ::checkTableError()
  }; 
  loops =  faultToInject ? 1 : sizeof(lookupFaults)/sizeof(int);

  while ((i<loops || until_stopped) && !ctx->isTestStopped())
  {
    g_info << i << ": ";

    int inject_err = faultToInject ? faultToInject : lookupFaults[i];
    int randomId = rand() % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(randomId);

    ndbout << "LookupJoinError: Injecting error "<<  inject_err <<
      " in node " << nodeId << " loop "<< i << endl;

    if (restarter.insertErrorInNode(nodeId, inject_err) != 0)
    {
      ndbout << "Could not insert error in node "<< nodeId <<endl;
      g_info << endl;
      return NDBT_FAILED;
    }

    // It'd be better if test could differentiates failures from
    // fault injection and others.
    // We expect to fail, and it's a failure if we don't
    if (!hugoTrans.runLookupQuery(GETNDB(step), queries))
    {
      g_info << "LookUpJoinError didn't fail as expected."<< endl;
      // return NDBT_FAILED;
    }

    addMask(ctx, (1 << stepNo), "Running");
    i++;
  }
  g_info << endl;
  return NDBT_OK;
}

int
runScanJoin(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int joinlevel = ctx->getProperty("JoinLevel", 3);
  int until_stopped = ctx->getProperty("UntilStopped", (Uint32)0);
  Uint32 stepNo = step->getStepNo();

  int i = 0;
  HugoQueryBuilder qb(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_SCAN);
  qb.setJoinLevel(joinlevel);
  const NdbQueryDef * query = qb.createQuery();
  HugoQueries hugoTrans(* query);
  while ((i<loops || until_stopped) && !ctx->isTestStopped())
  {
    g_info << i << ": ";
    if (hugoTrans.runScanQuery(GETNDB(step)))
    {
      g_info << endl;
      return NDBT_FAILED;
    }
    addMask(ctx, (1 << stepNo), "Running");
    i++;
  }
  g_info << endl;
  return NDBT_OK;
}

int
runScanJoinError(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int joinlevel = ctx->getProperty("JoinLevel", 3);
  int until_stopped = ctx->getProperty("UntilStopped", (Uint32)0);
  Uint32 stepNo = step->getStepNo();

  int i = 0;
  HugoQueryBuilder qb(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_SCAN);
  qb.setJoinLevel(joinlevel);
  const NdbQueryDef * query = qb.createQuery();
  HugoQueries hugoTrans(* query);

  NdbRestarter restarter;
  int scanFaults[] = {
      7240,        // DIGETNODESREQ returns error 
      17002, 17004, 17005, 17006, 17008,
      17012, // testing abort in :execDIH_SCAN_TAB_CONF
      17013, // Simulate DbspjErr::InvalidRequest
      17020, 17021, 17022, // lookup_send() encounter dead node -> NodeFailure
      17030, 17031, 17032, // LQHKEYREQ reply is LQHKEYREF('Invalid..')
      17040, 17041, 17042, // lookup_parent_row -> OutOfQueryMemory
      17050, 17051, 17052, 17053, // parseDA -> outOfSectionMem
      17060, 17061, 17062, 17063, // scanIndex_parent_row -> outOfSectionMem
      17070, 17071, 17072, // lookup_send.dupsec -> outOfSectionMem
      17080, 17081, 17082, // lookup_parent_row -> OutOfQueryMemory
      17090, 17091, 17092, 17093, // scanIndex_send -> OutOfQueryMemory
      17100, // scanFrag_sends invalid schema version, to get a SCAN_FRAGREF
      17110, 17111, 17112, // scanIndex_sends invalid schema version, to get a SCAN_FRAGREF
      17120, 17121, // execTRANSID_AI -> OutOfRowMemory
      17510,        // random failure when allocating section memory
      17520, 17521  // failure (+random) from TableRecord::checkTableError()
  }; 
  loops =  faultToInject ? 1 : sizeof(scanFaults)/sizeof(int);

  while ((i<loops || until_stopped) && !ctx->isTestStopped())
  {
    g_info << i << ": ";

    int inject_err = faultToInject ? faultToInject : scanFaults[i];
    int randomId = rand() % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(randomId);

    ndbout << "ScanJoin: Injecting error "<<  inject_err <<
              " in node " << nodeId << " loop "<< i<< endl;

    if (restarter.insertErrorInNode(nodeId, inject_err) != 0)
    {
      ndbout << "Could not insert error in node "<< nodeId <<endl;
      return NDBT_FAILED;
    }

    // It'd be better if test could differentiates failures from
    // fault injection and others.
    // We expect to fail, and it's a failure if we don't
    if (!hugoTrans.runScanQuery(GETNDB(step)))
    {
      g_info << "ScanJoinError didn't fail as expected."<< endl;
      // return NDBT_FAILED;
    }

    addMask(ctx, (1 << stepNo), "Running");
    i++;
  }

  g_info << endl;
  return NDBT_OK;
}

int
runJoin(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int joinlevel = ctx->getProperty("JoinLevel", 3);
  int records = ctx->getNumRecords();
  int queries = records/joinlevel;
  int until_stopped = ctx->getProperty("UntilStopped", (Uint32)0);
  Uint32 stepNo = step->getStepNo();

  int i = 0;
  HugoQueryBuilder qb1(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_SCAN);
  HugoQueryBuilder qb2(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_LOOKUP);
  qb1.setJoinLevel(joinlevel);
  qb2.setJoinLevel(joinlevel);
  const NdbQueryDef * q1 = qb1.createQuery();
  const NdbQueryDef * q2 = qb2.createQuery();
  HugoQueries hugoTrans1(* q1);
  HugoQueries hugoTrans2(* q2);
  while ((i<loops || until_stopped) && !ctx->isTestStopped())
  {
    g_info << i << ": ";
    if (hugoTrans1.runScanQuery(GETNDB(step)))
    {
      g_info << endl;
      return NDBT_FAILED;
    }
    if (hugoTrans2.runLookupQuery(GETNDB(step), queries))
    {
      g_info << endl;
      return NDBT_FAILED;
    }
    i++;
    addMask(ctx, (1 << stepNo), "Running");
  }
  g_info << endl;
  return NDBT_OK;
}

int
runRestarter(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int waitprogress = ctx->getProperty("WaitProgress", (unsigned)0);
  int randnode = ctx->getProperty("RandNode", (unsigned)0);
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted() != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  loops *= (restarter.getNumDbNodes() > 2 ? 2 : restarter.getNumDbNodes());
  if (loops < restarter.getNumDbNodes())
    loops = restarter.getNumDbNodes();

  NdbSleep_MilliSleep(200);
  Uint32 running = ctx->getProperty("Running", (Uint32)0);
  while (running == 0 && !ctx->isTestStopped())
  {
    NdbSleep_MilliSleep(100);
    running = ctx->getProperty("Running", (Uint32)0);
  }

  if (ctx->isTestStopped())
    return NDBT_FAILED;

  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){

    int id = lastId % restarter.getNumDbNodes();
    if (randnode == 1)
    {
      id = rand() % restarter.getNumDbNodes();
    }
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl;

    if(restarter.restartOneDbNode(nodeId, false, true, true) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.waitNodesNoStart(&nodeId, 1))
    {
      g_err << "Failed to waitNodesNoStart" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (waitprogress)
    {
      Uint32 maxwait = 60;
      ndbout_c("running: 0x%.8x", running);
      for (Uint32 checks = 0; checks < 3 && !ctx->isTestStopped(); checks++)
      {
        ctx->setProperty("Running", (Uint32)0);
        for (; maxwait != 0 && !ctx->isTestStopped(); maxwait--)
        {
          if ((ctx->getProperty("Running", (Uint32)0) & running) == running)
            goto ok;
          NdbSleep_SecSleep(1);
        }

        if (ctx->isTestStopped())
        {
          g_err << "Test stopped while waiting for progress!" << endl;
          return NDBT_FAILED;
        }

        g_err << "No progress made!!" << endl;
        return NDBT_FAILED;
    ok:
        g_err << "Progress made!! " << endl;
      }
    }

    if (restarter.startNodes(&nodeId, 1))
    {
      g_err << "Failed to start node" << endl;
      result = NDBT_FAILED;
      break;
    }

    if(restarter.waitClusterStarted() != 0){
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (waitprogress)
    {
      Uint32 maxwait = 60;
      ndbout_c("running: 0x%.8x", running);
      for (Uint32 checks = 0; checks < 3 && !ctx->isTestStopped(); checks++)
      {
        ctx->setProperty("Running", (Uint32)0);
        for (; maxwait != 0 && !ctx->isTestStopped(); maxwait--)
        {
          if ((ctx->getProperty("Running", (Uint32)0) & running) == running)
            goto ok2;
          NdbSleep_SecSleep(1);
        }

        if (ctx->isTestStopped())
        {
          g_err << "Test stopped while waiting for progress!" << endl;
          return NDBT_FAILED;
        }

        g_err << "No progress made!!" << endl;
        return NDBT_FAILED;
    ok2:
        g_err << "Progress made!! " << endl;
        ctx->setProperty("Running", (Uint32)0);
      }
    }

    lastId++;
    i++;
  }

  ctx->stopTest();

  return result;
}

#ifdef NDEBUG
// Some asserts have side effects, and there is no other error handling anyway.
#define ASSERT_ALWAYS(cond) if(!(cond)){abort();}
#else
#define ASSERT_ALWAYS assert
#endif

static const int nt2StrLen = 20;

static int
createNegativeSchema(NDBT_Context* ctx, NDBT_Step* step)
{
  for (int i = 0; i<2; i++)
  {
    NdbDictionary::Column::Type type = NdbDictionary::Column::Undefined;
    Uint32 arraySize = 0;
    const char* tabName = NULL;
    const char* ordIdxName = NULL;
    const char* unqIdxName = NULL;
    switch (i)
    {
    case 0:
      type = NdbDictionary::Column::Int;
      arraySize = 1;
      tabName = "nt1";
      ordIdxName = "nt1_oix";
      unqIdxName = "nt1_uix";
      break;
    case 1:
      type = NdbDictionary::Column::Varchar;
      arraySize = nt2StrLen;
      tabName = "nt2";
      ordIdxName = "nt2_oix";
      unqIdxName = "nt2_uix";
      break;
    }

    /****************************************************************
     *	Create table nt1 and attributes.
     ***************************************************************/
    NDBT_Attribute pk1("pk1", type, arraySize, true);
    NDBT_Attribute pk2("pk2", type, arraySize, true);
    NDBT_Attribute oi1("oi1", type, arraySize);
    NDBT_Attribute oi2("oi2", type, arraySize);
    NDBT_Attribute ui1("ui1", type, arraySize);
    NDBT_Attribute ui2("ui2", type, arraySize);

    NdbDictionary::Column* columns[] = {&pk1, &pk2, &oi1, &oi2, &ui1, &ui2};

    const NDBT_Table tabDef(tabName, sizeof columns/sizeof columns[0], columns);

    Ndb* const ndb = step->getNdb();

    NdbDictionary::Dictionary* const dictionary = ndb->getDictionary();

    dictionary->dropTable(tabName);
    ASSERT_ALWAYS(dictionary->createTable(tabDef) == 0);

    // Create ordered index on oi1,oi2.
    NdbDictionary::Index ordIdx(ordIdxName);
    ASSERT_ALWAYS(ordIdx.setTable(tabName) == 0);
    ordIdx.setType(NdbDictionary::Index::OrderedIndex);
    ordIdx.setLogging(false);
    ASSERT_ALWAYS(ordIdx.addColumn(oi1) == 0);
    ASSERT_ALWAYS(ordIdx.addColumn(oi2) == 0);
    ASSERT_ALWAYS(dictionary->createIndex(ordIdx, tabDef) == 0);

    // Create unique index on ui1,ui2.
    NdbDictionary::Index unqIdx(unqIdxName);
    ASSERT_ALWAYS(unqIdx.setTable(tabName) == 0);
    unqIdx.setType(NdbDictionary::Index::UniqueHashIndex);
    unqIdx.setLogging(true);
    ASSERT_ALWAYS(unqIdx.addColumn(ui1) == 0);
    ASSERT_ALWAYS(unqIdx.addColumn(ui2) == 0);
    ASSERT_ALWAYS(dictionary->createIndex(unqIdx, tabDef) == 0);
  } // for (...
  return NDBT_OK;
}

/* Query-related error codes. Used for negative testing. */
#define QRY_TOO_FEW_KEY_VALUES 4801
#define QRY_TOO_MANY_KEY_VALUES 4802
#define QRY_OPERAND_HAS_WRONG_TYPE 4803
#define QRY_CHAR_OPERAND_TRUNCATED 4804
#define QRY_NUM_OPERAND_RANGE 4805
#define QRY_MULTIPLE_PARENTS 4806
#define QRY_UNKNOWN_PARENT 4807
#define QRY_UNRELATED_INDEX 4809
#define QRY_WRONG_INDEX_TYPE 4810
#define QRY_DEFINITION_TOO_LARGE 4812
#define QRY_RESULT_ROW_ALREADY_DEFINED 4814
#define QRY_HAS_ZERO_OPERATIONS 4815
#define QRY_ILLEGAL_STATE 4817
#define QRY_WRONG_OPERATION_TYPE 4820
#define QRY_MULTIPLE_SCAN_SORTED 4824
#define QRY_EMPTY_PROJECTION 4826

/* Various error codes that are not specific to NdbQuery. */
static const int Err_FunctionNotImplemented = 4003;
static const int Err_UnknownColumn = 4004;
static const int Err_WrongFieldLength = 4209;
static const int Err_InvalidRangeNo = 4286;
static const int Err_DifferentTabForKeyRecAndAttrRec = 4287;
static const int Err_KeyIsNULL = 4316;

/**
 * Context data for negative tests of api extensions.
 */
class NegativeTest
{
public:
  // Static wrapper for each test case.
  static int keyTest(NDBT_Context* ctx, NDBT_Step* step)
  { return NegativeTest(ctx, step).runKeyTest();}

  static int graphTest(NDBT_Context* ctx, NDBT_Step* step)
  { return NegativeTest(ctx, step).runGraphTest();}

  static int setBoundTest(NDBT_Context* ctx, NDBT_Step* step)
  { return NegativeTest(ctx, step).runSetBoundTest();}

  static int valueTest(NDBT_Context* ctx, NDBT_Step* step)
  { return NegativeTest(ctx, step).runValueTest();}

  static int featureDisabledTest(NDBT_Context* ctx, NDBT_Step* step)
  { return NegativeTest(ctx, step).runFeatureDisabledTest();}

private:
  Ndb* m_ndb;
  NdbDictionary::Dictionary* m_dictionary;
  const NdbDictionary::Table* m_nt1Tab;
  const NdbDictionary::Index* m_nt1OrdIdx;
  const NdbDictionary::Index* m_nt1UnqIdx;
  const NdbDictionary::Table* m_nt2Tab;
  const NdbDictionary::Index* m_nt2OrdIdx;
  const NdbDictionary::Index* m_nt2UnqIdx;

  NegativeTest(NDBT_Context* ctx, NDBT_Step* step);

  // Tests
  int runKeyTest() const;
  int runGraphTest() const;
  int runSetBoundTest() const;
  int runValueTest() const;
  int runFeatureDisabledTest() const;
  // No copy.
  NegativeTest(const NegativeTest&);
  NegativeTest& operator=(const NegativeTest&);
};

NegativeTest::NegativeTest(NDBT_Context* ctx, NDBT_Step* step)
{
  m_ndb = step->getNdb();
  m_dictionary = m_ndb->getDictionary();

  m_nt1Tab = m_dictionary->getTable("nt1");
  ASSERT_ALWAYS(m_nt1Tab != NULL);

  m_nt1OrdIdx = m_dictionary->getIndex("nt1_oix", "nt1");
  ASSERT_ALWAYS(m_nt1OrdIdx != NULL);

  m_nt1UnqIdx = m_dictionary->getIndex("nt1_uix", "nt1");
  ASSERT_ALWAYS(m_nt1UnqIdx != NULL);

  m_nt2Tab = m_dictionary->getTable("nt2");
  ASSERT_ALWAYS(m_nt2Tab != NULL);

  m_nt2OrdIdx = m_dictionary->getIndex("nt2_oix", "nt2");
  ASSERT_ALWAYS(m_nt2OrdIdx != NULL);

  m_nt2UnqIdx = m_dictionary->getIndex("nt2_uix", "nt2");
  ASSERT_ALWAYS(m_nt2UnqIdx != NULL);
}

int
NegativeTest::runKeyTest() const
{
  // Make key with too long strings
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const char* longTxt= "x012345678901234567890123456789";
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(longTxt), builder->constValue(longTxt), NULL};

    if (builder->readTuple(m_nt2Tab, keyOperands) != NULL ||
        builder->getNdbError().code != QRY_CHAR_OPERAND_TRUNCATED)
    {
      g_err << "Lookup with truncated char values gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Make key with integer value outside column range.
  if (false) // Temporarily disabled.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1ull), builder->constValue(~0ull), NULL};

    if (builder->readTuple(m_nt1Tab, keyOperands) != NULL ||
        builder->getNdbError().code != QRY_NUM_OPERAND_RANGE)
    {
      g_err << "Lookup with integer value outside column range gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Make key with too few fields
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1), NULL};

    if (builder->readTuple(m_nt1Tab, keyOperands) != NULL ||
        builder->getNdbError().code != QRY_TOO_FEW_KEY_VALUES)
    {
      g_err << "Read with too few key values gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Make key with too many fields
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1), builder->constValue(1), builder->constValue(1), NULL};

    if (builder->readTuple(m_nt1Tab, keyOperands) != NULL ||
        builder->getNdbError().code != QRY_TOO_MANY_KEY_VALUES)
    {
      g_err << "Read with too many key values gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Make key with fields of wrong type.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1), builder->constValue("xxx"), NULL};

    if (builder->readTuple(m_nt1Tab, keyOperands) != NULL ||
        builder->getNdbError().code != QRY_OPERAND_HAS_WRONG_TYPE)
    {
      g_err << "Read with key values of wrong type gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Make key with unknown column. Try preparing failed NdbQueryBuilder.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1), builder->constValue(1), NULL};

    const NdbQueryLookupOperationDef* parentOperation
      = builder->readTuple(m_nt1Tab, keyOperands);
    ASSERT_ALWAYS(parentOperation != NULL);

    if (builder->linkedValue(parentOperation, "unknown_col") != NULL ||
        builder->getNdbError().code != Err_UnknownColumn)
    {
      g_err << "Link to unknown column gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    if (builder->prepare() != NULL)
    {
      g_err << "prepare() on failed query gave non-NULL result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Give too few parameter values.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->paramValue(), builder->paramValue(), NULL};

    ASSERT_ALWAYS(builder->readTuple(m_nt1Tab, keyOperands) != NULL);
    const NdbQueryDef* const queryDef = builder->prepare();
    ASSERT_ALWAYS(queryDef != NULL);
    builder->destroy();

    const NdbQueryParamValue params[] = {
      Uint32(1),
      NdbQueryParamValue()
    };

    NdbTransaction* const trans = m_ndb->startTransaction();
    NdbQuery* const query = trans->createQuery(queryDef, params);

    if (query != NULL || trans->getNdbError().code != Err_KeyIsNULL)
    {
      g_err << "Read with too few parameter values gave unexpected result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }
    m_ndb->closeTransaction(trans);
    queryDef->destroy();
  }

  /**
   * Check for too many parameter values currently not possible. Must decide if
   * NdbQueryParamValue with m_type==Type_NULL should be mandatory end marker or
   * used for specifying actual null values.
   */
  return NDBT_OK;
} // NegativeTest::runKeyTest()


int
NegativeTest::runGraphTest() const
{
  // Try preparing empty NdbQueryBuilder
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    if (builder->prepare() != NULL ||
        builder->getNdbError().code != QRY_HAS_ZERO_OPERATIONS)
    {
      g_err << "prepare() on empty query gave non-NULL result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Make query with too many operations.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1), builder->constValue(1), NULL};

    const NdbQueryLookupOperationDef* const parentOperation
      = builder->readTuple(m_nt1Tab, keyOperands);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryOperand* const childOperands[] =
      {builder->linkedValue(parentOperation, "ui1"),
       builder->linkedValue(parentOperation, "oi1"),
      NULL};

    for (Uint32 i = 0; i<32; i++)
    {
      const NdbQueryLookupOperationDef* const childOperation
        = builder->readTuple(m_nt1Tab, childOperands);
      if (i < 31)
      {
        ASSERT_ALWAYS(childOperation != NULL);
      }
      else if (childOperation != NULL &&
               builder->getNdbError().code != QRY_DEFINITION_TOO_LARGE)
      {
        g_err << "Building query with too many operations gave unexpected "
          "result.";
        builder->destroy();
        return NDBT_FAILED;
      }
    }
    builder->destroy();
  }

  // Make query with two root operations.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1), builder->constValue(1), NULL};

    const NdbQueryLookupOperationDef* const root1
      = builder->readTuple(m_nt1Tab, keyOperands);
    ASSERT_ALWAYS(root1 != NULL);

    if (builder->readTuple(m_nt1Tab, keyOperands)!= NULL ||
        builder->getNdbError().code != QRY_UNKNOWN_PARENT)
    {
      g_err << "Query with two root operations gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    };
    builder->destroy();
  }

  // Try lookup on ordered index.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1), builder->constValue(1), NULL};

    if (builder->readTuple(m_nt1OrdIdx, m_nt1Tab, keyOperands) != NULL ||
        builder->getNdbError().code != QRY_WRONG_INDEX_TYPE)
    {
      g_err << "Lookup on ordered index gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Try lookup on index on wrong table.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1), builder->constValue(1), NULL};

    if (builder->readTuple(m_nt2OrdIdx, m_nt1Tab, keyOperands) != NULL ||
        builder->getNdbError().code != QRY_UNRELATED_INDEX)
    {
      g_err << "Lookup on unrelated index gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Try scanning unique index.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const boundOperands[] =
      {builder->constValue(1), NULL};
    const NdbQueryIndexBound bound(boundOperands);

    if (builder->scanIndex(m_nt1UnqIdx, m_nt1Tab, &bound) != NULL ||
        builder->getNdbError().code != QRY_WRONG_INDEX_TYPE)
    {
      g_err << "Scan of unique index gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Try scanning index on wrong table.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const boundOperands[] =
      {builder->constValue(1), NULL};
    const NdbQueryIndexBound bound(boundOperands);

    if (builder->scanIndex(m_nt2OrdIdx, m_nt1Tab, &bound) != NULL ||
        builder->getNdbError().code != QRY_UNRELATED_INDEX)
    {
      g_err << "Scan of unrelated index gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Try adding a scan child to a lookup root.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const keyOperands[] =
      {builder->constValue(1), builder->constValue(1), NULL};

    const NdbQueryLookupOperationDef* parentOperation
      = builder->readTuple(m_nt1Tab, keyOperands);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryOperand* const childOperands[] =
      {builder->linkedValue(parentOperation, "ui1"),
       builder->linkedValue(parentOperation, "oi1"),
      NULL};
    const NdbQueryIndexBound bound(childOperands);

    if (builder->scanIndex(m_nt1OrdIdx, m_nt1Tab, &bound) != NULL ||
        builder->getNdbError().code != QRY_WRONG_OPERATION_TYPE)
    {
      g_err << "Lookup with scan child gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  // Try adding a sorted child scan to a query.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();

    const NdbQueryTableScanOperationDef* parentOperation
      = builder->scanTable(m_nt1Tab);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryOperand* const childOperands[] =
      {builder->linkedValue(parentOperation, "ui1"),
      NULL};
    const NdbQueryIndexBound bound(childOperands);
    NdbQueryOptions childOptions;
    childOptions.setOrdering(NdbQueryOptions::ScanOrdering_ascending);

    if (builder->scanIndex(m_nt1OrdIdx, m_nt1Tab, &bound, &childOptions) != NULL ||
        builder->getNdbError().code != QRY_MULTIPLE_SCAN_SORTED)
    {
      g_err << "Query with sorted child scan gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  /**
   * Try adding a child operation with two parents that are not descendants of each
   * other (i.e. a diamond-shaped query graph).
   */
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();
    const NdbQueryOperand* const rootKey[] =
      {builder->constValue(1), builder->constValue(1), NULL};

    const NdbQueryLookupOperationDef* rootOperation
      = builder->readTuple(m_nt1Tab, rootKey);
    ASSERT_ALWAYS(rootOperation != NULL);

    const NdbQueryOperand* const leftKey[] =
      {builder->linkedValue(rootOperation, "ui1"), builder->constValue(1), NULL};

    const NdbQueryLookupOperationDef* leftOperation
      = builder->readTuple(m_nt1Tab, leftKey);
    ASSERT_ALWAYS(leftOperation != NULL);

    const NdbQueryOperand* const rightKey[] =
      {builder->linkedValue(rootOperation, "ui1"), builder->constValue(1), NULL};

    const NdbQueryLookupOperationDef* rightOperation
      = builder->readTuple(m_nt1Tab, rightKey);
    ASSERT_ALWAYS(rightOperation != NULL);

    const NdbQueryOperand* const bottomKey[] =
      {builder->linkedValue(leftOperation, "ui1"),
       builder->linkedValue(rightOperation, "oi1"),
       NULL};

    if (builder->readTuple(m_nt1Tab, bottomKey) != NULL ||
        builder->getNdbError().code != QRY_MULTIPLE_PARENTS)
    {
      g_err << "Diamond-shaped query graph gave unexpected result.";
      builder->destroy();
      return NDBT_FAILED;
    }
    builder->destroy();
  }

  return NDBT_OK;
} // NegativeTest::runGraphTest()


int
NegativeTest::runSetBoundTest() const
{
  // Test NdbQueryOperation::setBound() with too long string value.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();

    const NdbQueryIndexScanOperationDef* parentOperation
      = builder->scanIndex(m_nt2OrdIdx, m_nt2Tab);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryDef* const queryDef = builder->prepare();
    ASSERT_ALWAYS(queryDef != NULL);
    builder->destroy();

    NdbTransaction* const trans = m_ndb->startTransaction();
    NdbQuery* const query = trans->createQuery(queryDef);

    // Make bound with too long string.
    const NdbDictionary::RecordSpecification ordIdxRecSpec[] =
      {{m_nt2Tab->getColumn("oi1"), 0, 0, 0}};

    const NdbRecord* const ordIdxRecord =
      m_dictionary->createRecord(m_nt2OrdIdx, ordIdxRecSpec,
                                 sizeof ordIdxRecSpec/sizeof ordIdxRecSpec[0],
                                 sizeof(NdbDictionary::RecordSpecification));
    ASSERT_ALWAYS(ordIdxRecord != NULL);

    char boundRow[2+nt2StrLen+10];
    memset(boundRow, 'x', sizeof boundRow);
    // Set string lenght field.
    *reinterpret_cast<Uint16*>(boundRow) = nt2StrLen+10;

    NdbIndexScanOperation::IndexBound
      bound = {boundRow, 1, true, boundRow, 1, true, 0};

    if (query->setBound(ordIdxRecord, &bound) == 0 ||
        query->getNdbError().code != Err_WrongFieldLength)
    {
      g_err << "Scan bound with too long string value gave unexpected result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }

    // Set correct string lengh.
    *reinterpret_cast<Uint16*>(boundRow) = nt2StrLen;
    bound.range_no = 1;
    if (query->setBound(ordIdxRecord, &bound) == 0 ||
        query->getNdbError().code != QRY_ILLEGAL_STATE)
    {
      g_err << "setBound() in failed state gave unexpected result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }

    m_ndb->closeTransaction(trans);
    queryDef->destroy();
  }

  // Test NdbQueryOperation::setBound() with wrong bound no.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();

    const NdbQueryIndexScanOperationDef* parentOperation
      = builder->scanIndex(m_nt1OrdIdx, m_nt1Tab);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryDef* const queryDef = builder->prepare();
    ASSERT_ALWAYS(queryDef != NULL);
    builder->destroy();

    NdbTransaction* const trans = m_ndb->startTransaction();
    NdbQuery* const query = trans->createQuery(queryDef);

    const int boundRow[] = {1, 1};

    // Make bound with wrong bound no.
    NdbIndexScanOperation::IndexBound
      bound = {reinterpret_cast<const char*>(boundRow), 1, true,
               reinterpret_cast<const char*>(boundRow), 1, true, 1/*Should be 0.*/};

    if (query->setBound(m_nt1OrdIdx->getDefaultRecord(), &bound) == 0 ||
        query->getNdbError().code != Err_InvalidRangeNo)
    {
      g_err << "Scan bound with wrong range no gave unexpected result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }

    m_ndb->closeTransaction(trans);
    queryDef->destroy();
  }

  // Test NdbQueryOperation::setBound() on table scan.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();

    const NdbQueryTableScanOperationDef* parentOperation
      = builder->scanTable(m_nt1Tab);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryDef* const queryDef = builder->prepare();
    ASSERT_ALWAYS(queryDef != NULL);
    builder->destroy();

    NdbTransaction* const trans = m_ndb->startTransaction();
    NdbQuery* const query = trans->createQuery(queryDef);

    const int boundRow[] = {1, 1};

    NdbIndexScanOperation::IndexBound
      bound = {reinterpret_cast<const char*>(boundRow), 1, true,
               reinterpret_cast<const char*>(boundRow), 1, true, 0};

    if (query->setBound(m_nt1OrdIdx->getDefaultRecord(), &bound) == 0 ||
        query->getNdbError().code != QRY_WRONG_OPERATION_TYPE)
    {
      g_err << "Scan bound on table scan gave unexpected result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }

    m_ndb->closeTransaction(trans);
    queryDef->destroy();
  }

  // Test NdbQueryOperation::setBound() in executed query.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();

    const NdbQueryIndexScanOperationDef* parentOperation
      = builder->scanIndex(m_nt1OrdIdx, m_nt1Tab);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryDef* const queryDef = builder->prepare();
    ASSERT_ALWAYS(queryDef != NULL);
    builder->destroy();

    NdbTransaction* const trans = m_ndb->startTransaction();
    NdbQuery* const query = trans->createQuery(queryDef);

    const char* resultRow;

    ASSERT_ALWAYS(query->getQueryOperation(0u)
                  ->setResultRowRef(m_nt1Tab->getDefaultRecord(),
                                    resultRow, NULL) == 0);

    ASSERT_ALWAYS(trans->execute(NoCommit)==0);

    const int boundRow[] = {1, 1};

    // Add bound now.
    NdbIndexScanOperation::IndexBound
      bound = {reinterpret_cast<const char*>(boundRow), 1, true,
               reinterpret_cast<const char*>(boundRow), 1, true, 0};

    if (query->setBound(m_nt1OrdIdx->getDefaultRecord(), &bound) == 0 ||
        query->getNdbError().code != QRY_ILLEGAL_STATE)
    {
      g_err << "Adding scan bound to executed query gave unexpected result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }

    m_ndb->closeTransaction(trans);
    queryDef->destroy();
  }

  return NDBT_OK;
} // NegativeTest::runSetBoundTest()


int
NegativeTest::runValueTest() const
{
  // Test NdbQueryOperation::getValue() on an unknown column.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();

    const NdbQueryTableScanOperationDef* parentOperation
      = builder->scanTable(m_nt1Tab);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryDef* const queryDef = builder->prepare();
    ASSERT_ALWAYS(queryDef != NULL);
    builder->destroy();

    NdbTransaction* const trans = m_ndb->startTransaction();
    NdbQuery* const query = trans->createQuery(queryDef);

    if (query->getQueryOperation(0u)->getValue("unknownCol") != NULL ||
        query->getNdbError().code != Err_UnknownColumn)
    {
      g_err << "NdbQueryOperation::getValue() on unknown column gave unexpected result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }

    m_ndb->closeTransaction(trans);
    queryDef->destroy();
  }

  // Try fetching results with an NdbRecord for a different table.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();

    const NdbQueryTableScanOperationDef* parentOperation
      = builder->scanTable(m_nt1Tab);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryDef* const queryDef = builder->prepare();
    ASSERT_ALWAYS(queryDef != NULL);
    builder->destroy();

    NdbTransaction* const trans = m_ndb->startTransaction();
    NdbQuery* const query = trans->createQuery(queryDef);

    const char* resultRow;

    if (query->getQueryOperation(0u)->setResultRowRef(m_nt2Tab->getDefaultRecord(),
                                                      resultRow, NULL) == 0 ||
        query->getNdbError().code != Err_DifferentTabForKeyRecAndAttrRec)
    {
      g_err << "NdbQueryOperation::setResultRowRef() on wrong table gave unexpected "
        "result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }

    m_ndb->closeTransaction(trans);
    queryDef->destroy();
  }

  // Try defining result row twice.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();

    const NdbQueryTableScanOperationDef* parentOperation
      = builder->scanTable(m_nt1Tab);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryDef* const queryDef = builder->prepare();
    ASSERT_ALWAYS(queryDef != NULL);
    builder->destroy();

    NdbTransaction* const trans = m_ndb->startTransaction();
    NdbQuery* const query = trans->createQuery(queryDef);

    const char* resultRow;

    ASSERT_ALWAYS(query->getQueryOperation(0u)
                  ->setResultRowRef(m_nt1Tab->getDefaultRecord(),
                                    resultRow, NULL) == 0);

    if (query->getQueryOperation(0u)->setResultRowRef(m_nt1Tab->getDefaultRecord(),
                                                      resultRow, NULL) == 0 ||
        query->getNdbError().code != QRY_RESULT_ROW_ALREADY_DEFINED)
    {
      g_err << "Defining result row twice gave unexpected result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }

    m_ndb->closeTransaction(trans);
    queryDef->destroy();
  }

  // Test operation with empty projection.
  {
    NdbQueryBuilder* const builder = NdbQueryBuilder::create();

    const NdbQueryIndexScanOperationDef* parentOperation
      = builder->scanIndex(m_nt1OrdIdx, m_nt1Tab);
    ASSERT_ALWAYS(parentOperation != NULL);

    const NdbQueryDef* const queryDef = builder->prepare();
    ASSERT_ALWAYS(queryDef != NULL);
    builder->destroy();

    NdbTransaction* const trans = m_ndb->startTransaction();
    NdbQuery* const query = trans->createQuery(queryDef);

    // Execute without defining a projection.
    if (trans->execute(NoCommit) == 0 ||
        query->getNdbError().code != QRY_EMPTY_PROJECTION)
    {
      g_err << "Having operation with empty projection gave unexpected result.";
      m_ndb->closeTransaction(trans);
      queryDef->destroy();
      return NDBT_FAILED;
    }

    m_ndb->closeTransaction(trans);
    queryDef->destroy();
  }
  return NDBT_OK;
} // NegativeTest::runValueBoundTest()

/**
 * Check that query pushdown is disabled in older versions of the code
 * (even if the API extensions are present in the code).
 */
int
NegativeTest::runFeatureDisabledTest() const
{
  NdbQueryBuilder* const builder = NdbQueryBuilder::create();
  
  const NdbQueryTableScanOperationDef* const parentOperation
    = builder->scanTable(m_nt1Tab);
  
  int result = NDBT_OK;

  if (ndb_join_pushdown(ndbGetOwnVersion()))
  {
    if (parentOperation == NULL)
    {
      g_err << "scanTable() failed: " << builder->getNdbError()
            << endl;
      result = NDBT_FAILED;
    }
    else
    {
      g_info << "scanTable() succeeded in version "
             << ndbGetOwnVersionString() << " as expected." << endl;
    }
  }
  else
  {
    // Query pushdown should not be enabled in this version.
    if (parentOperation != NULL)
    {
      g_err << "Succeeded with creating scan operation, which should not be "
        "possible in version " << ndbGetOwnVersionString() << endl;
      result = NDBT_FAILED;      
    }
    else if (builder->getNdbError().code != Err_FunctionNotImplemented)
    {
      g_err << "scanTable() failed with unexpected error: " 
            << builder->getNdbError() << endl;
      result = NDBT_FAILED;
    }
    else
    {
      g_info << "scanTable() failed in version "
             << ndbGetOwnVersionString() << " as expected with error: " 
             << builder->getNdbError() << endl;
    }
  }

  builder->destroy();
  return result;
} // NegativeTest::runFeatureDisabledTest()

static int
dropNegativeSchema(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbDictionary::Dictionary* const dictionary
    = step->getNdb()->getDictionary();

  if (dictionary->dropTable("nt1") != 0)
  {
    g_err << "Failed to drop table nt1." << endl;
    return NDBT_FAILED;
  }
  if (dictionary->dropTable("nt2") != 0)
  {
    g_err << "Failed to drop table nt2." << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

NDBT_TESTSUITE(testSpj);
TESTCASE("NegativeJoin", ""){
  INITIALIZER(createNegativeSchema);
  INITIALIZER(NegativeTest::keyTest);
  INITIALIZER(NegativeTest::graphTest);
  INITIALIZER(NegativeTest::setBoundTest);
  INITIALIZER(NegativeTest::valueTest);
  FINALIZER(dropNegativeSchema);
}
TESTCASE("FeatureDisabled", ""){
  INITIALIZER(createNegativeSchema);
  INITIALIZER(NegativeTest::featureDisabledTest);
  FINALIZER(dropNegativeSchema);
}
TESTCASE("LookupJoin", ""){
  INITIALIZER(runLoadTable);
  STEP(runLookupJoin);
  VERIFIER(runClearTable);
}
TESTCASE("ScanJoin", ""){
  INITIALIZER(runLoadTable);
  STEP(runScanJoin);
  FINALIZER(runClearTable);
}
TESTCASE("MixedJoin", ""){
  INITIALIZER(runLoadTable);
  STEPS(runJoin, 6);
  FINALIZER(runClearTable);
}
TESTCASE("NF_Join", ""){
  TC_PROPERTY("UntilStopped", 1);
  TC_PROPERTY("WaitProgress", 20);
  INITIALIZER(runLoadTable);
  //STEPS(runScanJoin, 6);
  //STEPS(runLookupJoin, 6);
  STEPS(runJoin, 6);
  STEP(runRestarter);
  FINALIZER(runClearTable);
}

TESTCASE("LookupJoinError", ""){
  INITIALIZER(runLoadTable);
  STEP(runLookupJoinError);
  VERIFIER(runClearTable);
}
TESTCASE("ScanJoinError", ""){
  INITIALIZER(runLoadTable);
  TC_PROPERTY("NodeNumber", 2);
  STEP(runScanJoinError);
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testSpj);


int main(int argc, const char** argv){
  ndb_init();

  /* To inject a single fault, for testing fault injection.
     Add the required fault number at the end
     of the command line. */

  if (argc > 0) sscanf(argv[argc-1], "%d",  &faultToInject);
  if (faultToInject && (faultToInject < FI_START || faultToInject > FI_END))
  {
    ndbout_c("Illegal fault to inject: %d. Legal range is between %d and %d",
             faultToInject, FI_START, FI_END);
    exit(1);
  }

  NDBT_TESTSUITE_INSTANCE(testSpj);
  return testSpj.execute(argc, argv);
}
