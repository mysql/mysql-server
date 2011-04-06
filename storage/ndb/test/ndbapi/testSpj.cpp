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
  int until_stopped = ctx->getProperty("UntilStopped", (Uint32)0);
  Uint32 stepNo = step->getStepNo();

  int i = 0;
  HugoQueryBuilder qb(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_LOOKUP);
  qb.setJoinLevel(joinlevel);
  const NdbQueryDef * query = qb.createQuery(GETNDB(step));
  HugoQueries hugoTrans(*query);
  while ((i<loops || until_stopped) && !ctx->isTestStopped())
  {
    g_info << i << ": ";
    if (hugoTrans.runLookupQuery(GETNDB(step), records))
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
runScanJoin(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int joinlevel = ctx->getProperty("JoinLevel", 3);
  int until_stopped = ctx->getProperty("UntilStopped", (Uint32)0);
  Uint32 stepNo = step->getStepNo();

  int i = 0;
  HugoQueryBuilder qb(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_SCAN);
  qb.setJoinLevel(joinlevel);
  const NdbQueryDef * query = qb.createQuery(GETNDB(step));
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
runJoin(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int joinlevel = ctx->getProperty("JoinLevel", 3);
  int records = ctx->getNumRecords();
  int until_stopped = ctx->getProperty("UntilStopped", (Uint32)0);
  Uint32 stepNo = step->getStepNo();

  int i = 0;
  HugoQueryBuilder qb1(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_SCAN);
  HugoQueryBuilder qb2(GETNDB(step), ctx->getTab(), HugoQueryBuilder::O_LOOKUP);
  qb1.setJoinLevel(joinlevel);
  qb2.setJoinLevel(joinlevel);
  const NdbQueryDef * q1 = qb1.createQuery(GETNDB(step));
  const NdbQueryDef * q2 = qb2.createQuery(GETNDB(step));
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
    if (hugoTrans2.runLookupQuery(GETNDB(step), records))
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
      Uint32 maxwait = 30;
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
      Uint32 maxwait = 30;
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

NDBT_TESTSUITE(testSpj);
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
NDBT_TESTSUITE_END(testSpj);


int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testSpj);
  return testSpj.execute(argc, argv);
}

