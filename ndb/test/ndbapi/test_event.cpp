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

int theThreadIdCounter = 0;

int runEventOperation(NDBT_Context* ctx, NDBT_Step* step)
{
  int tId = theThreadIdCounter++;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());

  EventOperationStats stats;

  g_info << "***** Id " << tId << endl;

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
    ndbout << "n_inserts =           " << stats.n_inserts << endl;
    ndbout << "n_deletes =           " << stats.n_deletes << endl;
    ndbout << "n_updates =           " << stats.n_updates << endl;
    ndbout << "n_consecutive =       " << stats.n_consecutive << endl;
    ndbout << "n_duplicates =        " << stats.n_duplicates << endl;
    ndbout << "n_inconsistent_gcis = " << stats.n_inconsistent_gcis << endl;
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
  STEP(runEventOperation);
  STEP(runEventOperation);
  STEP(runEventOperation);
  STEP(runEventLoad);
  FINALIZER(runDropEvent);
}
NDBT_TESTSUITE_END(test_event);

#if 0
NDBT_TESTSUITE(test_event);
TESTCASE("ParallellEventOperation", 
	 "Verify that we can listen to Events in Parallell"
	 "NOTE! No errors are allowed!" ){
  INITIALIZER(runCreateAllEvent);
  STEP(runEventOperation);
  FINALIZER(runDropEvent);
}
NDBT_TESTSUITE_END(test_event);
#endif

int main(int argc, const char** argv){
  ndb_init();
  return test_event.execute(argc, argv);
}

