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
#include "TestNdbEventOperation.hpp"

#define GETNDB(ps) ((NDBT_NdbApiStep*)ps)->getNdb()

int runCreateEvent(NDBT_Context* ctx, NDBT_Step* step)
{
  HugoTransactions hugoTrans(*ctx->getTab());

  if (hugoTrans.createEvent(GETNDB(step)) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runCreateDropEventOperation(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  EventOperationStats stats;

  Ndb *pNdb=GETNDB(step);
  const NdbDictionary::Table& tab= *ctx->getTab();
  NdbEventOperation    *pOp;
  char eventName[1024];
  sprintf(eventName,"%s_EVENT",tab.getName());
  int noEventColumnName = tab.getNoOfColumns();

  for (int i= 0; i < loops; i++)
  {
#if 1
    if (hugoTrans.eventOperation(GETNDB(step), (void*)&stats, 0) != 0){
      return NDBT_FAILED;
    }
#else
    g_info << "create EventOperation\n";
    pOp = pNdb->createEventOperation(eventName, 100);
    if ( pOp == NULL ) {
      g_err << "Event operation creation failed\n";
      return NDBT_FAILED;
    }

    g_info << "dropping event operation" << endl;
    int res = pNdb->dropEventOperation(pOp);
    if (res != 0) {
      g_err << "operation execution failed\n";
      return NDBT_FAILED;
    }
#endif
  }

  return NDBT_OK;
}

int theThreadIdCounter = 0;

int runEventOperation(NDBT_Context* ctx, NDBT_Step* step)
{
  int tId = theThreadIdCounter++;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());

  EventOperationStats stats;

  g_info << "***** start Id " << tId << endl;

  //  sleep(tId);

  if (hugoTrans.eventOperation(GETNDB(step), (void*)&stats, 3*records) != 0){
    return NDBT_FAILED;
  }

  int ret;
  if (stats.n_inserts     == records &&
      stats.n_deletes     == records &&
      stats.n_updates     == records &&
      stats.n_consecutive == 3 &&
      stats.n_duplicates  == 0)
    ret = NDBT_OK;
  else
    ret = NDBT_FAILED;

  if (ret == NDBT_FAILED) {
    g_info << "***** end Id " << tId << endl;
    ndbout_c("n_inserts =           %d (%d)", stats.n_inserts, records);
    ndbout_c("n_deletes =           %d (%d)", stats.n_deletes, records);
    ndbout_c("n_updates =           %d (%d)", stats.n_updates, records);
    ndbout_c("n_consecutive =       %d (%d)", stats.n_consecutive, 3);
    ndbout_c("n_duplicates =        %d (%d)", stats.n_duplicates, 0);
    ndbout_c("n_inconsistent_gcis = %d (%d)", stats.n_inconsistent_gcis, 0);
  }

  return ret;
}

int runEventLoad(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());

  sleep(5);
  sleep(theThreadIdCounter);

  if (hugoTrans.loadTable(GETNDB(step), records, 1, true, loops) != 0){
    return NDBT_FAILED;
  }
  if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, loops) != 0){
    return NDBT_FAILED;
  }
  if (hugoTrans.pkDelRecords(GETNDB(step),  records, 1, true, loops) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runDropEvent(NDBT_Context* ctx, NDBT_Step* step)
{
  HugoTransactions hugoTrans(*ctx->getTab());

  theThreadIdCounter = 0;
  //  if (hugoTrans.createEvent(GETNDB(step)) != 0){
  //    return NDBT_FAILED;
  //  }
  return NDBT_OK;
}

//  INITIALIZER(runInsert);
//  STEP(runPkRead);
//  VERIFIER(runVerifyInsert);
//  FINALIZER(runClearTable);

NDBT_TESTSUITE(test_event);
TESTCASE("BasicEventOperation", 
	 "Verify that we can listen to Events"
	 "NOTE! No errors are allowed!" ){
  INITIALIZER(runCreateEvent);
  STEP(runEventOperation);
  STEP(runEventLoad);
  FINALIZER(runDropEvent);
}
TESTCASE("CreateDropEventOperation", 
	 "Verify that we can Create and Drop many times"
	 "NOTE! No errors are allowed!" ){
  INITIALIZER(runCreateEvent);
  STEP(runCreateDropEventOperation);
  FINALIZER(runDropEvent);
}
TESTCASE("ParallellEventOperation", 
	 "Verify that we can listen to Events in parallell"
	 "NOTE! No errors are allowed!" ){
  INITIALIZER(runCreateEvent);
  STEP(runEventOperation);
  STEP(runEventOperation);
  STEP(runEventLoad);
  FINALIZER(runDropEvent);
}
NDBT_TESTSUITE_END(test_event);

int main(int argc, const char** argv){
  ndb_init();
  return test_event.execute(argc, argv);
}

