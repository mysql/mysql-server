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

int runCreateShadowTable(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table *table= ctx->getTab();
  char buf[1024];
  sprintf(buf, "%s_SHADOW", table->getName());

  GETNDB(step)->getDictionary()->dropTable(buf);
  if (GETNDB(step)->getDictionary()->getTable(buf))
  {
    g_err << "unsucessful drop of " << buf << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Table table_shadow(*table);
  table_shadow.setName(buf);
  GETNDB(step)->getDictionary()->createTable(table_shadow);
  if (GETNDB(step)->getDictionary()->getTable(buf))
    return NDBT_OK;

  g_err << "unsucessful create of " << buf << endl;
  return NDBT_FAILED;
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

int runEventMixedLoad(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());

  sleep(5);

  if (hugoTrans.loadTable(GETNDB(step), 3*records, 1, true, 1) != 0){
    return NDBT_FAILED;
  }
  if (hugoTrans.pkDelRecords(GETNDB(step), 3*records, 1, true, 1) != 0){
    return NDBT_FAILED;
  }
  if (hugoTrans.loadTable(GETNDB(step), records, 1, true, 1) != 0){
    return NDBT_FAILED;
  }
  if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, 1) != 0){
    return NDBT_FAILED;
  }
  if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, 1) != 0){
    return NDBT_FAILED;
  }
  if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, 1) != 0){
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

int runVerify(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = ctx->getNumRecords();
  const NdbDictionary::Table * table= ctx->getTab();
  char buf[1024];

  sprintf(buf, "%s_SHADOW", table->getName());

  HugoTransactions hugoTrans(*table);
  if (hugoTrans.compare(GETNDB(step), buf, 0))
  {
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runEventApplier(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("runEventApplier");

  int records = ctx->getNumRecords();
  int loops = ctx->getNumLoops();
  const NdbDictionary::Table * table= ctx->getTab();
  char buf[1024];

  sprintf(buf, "%s_SHADOW", table->getName());
  const NdbDictionary::Table * table_shadow;
  if ((table_shadow = GETNDB(step)->getDictionary()->getTable(buf)) == 0)
  {
    g_err << "Unable to get table " << buf << endl;
    DBUG_RETURN(NDBT_FAILED);
  }

  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation    *pOp;
  pOp = GETNDB(step)->createEventOperation(buf, 10*records);
  if ( pOp == NULL ) {
    g_err << "Event operation creation failed on %s" << buf << endl;
    DBUG_RETURN(NDBT_FAILED);
  }

  int i;
  int n_columns= table->getNoOfColumns();
  NdbRecAttr* recAttr[1024];
  NdbRecAttr* recAttrPre[1024];
  for (i = 0; i < n_columns; i++) {
    recAttr[i]    = pOp->getValue(table->getColumn(i)->getName());
    recAttrPre[i] = pOp->getPreValue(table->getColumn(i)->getName());
  }

  if (pOp->execute()) { // This starts changes to "start flowing"
    g_err << "execute operation execution failed: \n";
    g_err << pOp->getNdbError().code << " "
	  << pOp->getNdbError().message << endl;
    DBUG_RETURN(NDBT_FAILED);
  }

  int r= 0;
  int res;
  while (r < 10*records){
    //printf("now waiting for event...\n");
    res= GETNDB(step)->pollEvents(1000); // wait for event or 1000 ms
    if (res <= 0)
      continue;

    //printf("got data! %d\n", r);
    int overrun= 0;
    while (pOp->next(&overrun) > 0)
    {
      if (overrun)
      {
	g_err << "buffer overrun\n";
	DBUG_RETURN(NDBT_FAILED);
      }
      r++;

      Uint32 gci= pOp->getGCI();

      if (!pOp->isConsistent()) {
	g_err << "A node failure has occured and events might be missing\n";
	DBUG_RETURN(NDBT_FAILED);
      }

      int noRetries= 0;
      do
      {
	NdbTransaction *trans= GETNDB(step)->startTransaction();
	if (trans == 0)
	{
	  g_err << "startTransaction failed "
		<< GETNDB(step)->getNdbError().code << " "
		<< GETNDB(step)->getNdbError().message << endl;
	  DBUG_RETURN(NDBT_FAILED);
	}
	
	NdbOperation *op= trans->getNdbOperation(table_shadow);
	if (op == 0)
	{
	  g_err << "getNdbOperation failed "
		<< trans->getNdbError().code << " "
		<< trans->getNdbError().message << endl;
	  DBUG_RETURN(NDBT_FAILED);
	}
	
	switch (pOp->getEventType()) {
	case NdbDictionary::Event::TE_INSERT:
	  if (op->insertTuple())
	  {
	    g_err << "insertTuple "
		  << op->getNdbError().code << " "
		  << op->getNdbError().message << endl;
	    DBUG_RETURN(NDBT_FAILED);
	  }
	  break;
	case NdbDictionary::Event::TE_DELETE:
	  if (op->deleteTuple())
	  {
	    g_err << "deleteTuple "
		  << op->getNdbError().code << " "
		  << op->getNdbError().message << endl;
	    DBUG_RETURN(NDBT_FAILED);
	  }
	  break;
	case NdbDictionary::Event::TE_UPDATE:
	  if (op->updateTuple())
	  {
	    g_err << "updateTuple "
		  << op->getNdbError().code << " "
		  << op->getNdbError().message << endl;
	    DBUG_RETURN(NDBT_FAILED);
	  }
	  break;
	default:
	  abort();
	}

	for (i= 0; i < n_columns; i++)
	{
	  if (recAttr[i]->isNULL())
	  {
	    if (table->getColumn(i)->getPrimaryKey())
	    {
	      g_err << "internal error: primary key isNull()="
		    << recAttr[i]->isNULL() << endl;
	      DBUG_RETURN(NDBT_FAILED);
	    }
	    switch (pOp->getEventType()) {
	    case NdbDictionary::Event::TE_INSERT:
	      if (recAttr[i]->isNULL() < 0)
	      {
		g_err << "internal error: missing value for insert\n";
		DBUG_RETURN(NDBT_FAILED);
	      }
	      break;
	    case NdbDictionary::Event::TE_DELETE:
	      break;
	    case NdbDictionary::Event::TE_UPDATE:
	      break;
	    default:
	      abort();
	    }
	  }
	  if (table->getColumn(i)->getPrimaryKey() &&
	      op->equal(i,recAttr[i]->aRef()))
	  {
	    g_err << "equal " << i << " "
		  << op->getNdbError().code << " "
		  << op->getNdbError().message << endl;
	    DBUG_RETURN(NDBT_FAILED);
	  }
	}
	
	switch (pOp->getEventType()) {
	case NdbDictionary::Event::TE_INSERT:
	  for (i= 0; i < n_columns; i++)
	  {
	    if (!table->getColumn(i)->getPrimaryKey() &&
		op->setValue(i,recAttr[i]->isNULL() ? 0:recAttr[i]->aRef()))
	    {
	      g_err << "setValue(insert) " << i << " "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      DBUG_RETURN(NDBT_FAILED);
	    }
	  }
	  break;
	case NdbDictionary::Event::TE_DELETE:
	  break;
	case NdbDictionary::Event::TE_UPDATE:
	  for (i= 0; i < n_columns; i++)
	  {
	    if (!table->getColumn(i)->getPrimaryKey() &&
		recAttr[i]->isNULL() >= 0 &&
		op->setValue(i,recAttr[i]->isNULL() ? 0:recAttr[i]->aRef()))
	    {
	      g_err << "setValue(update) " << i << " "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      DBUG_RETURN(NDBT_FAILED);
	    }
	  }
	  break;
	case NdbDictionary::Event::TE_ALL:
	  abort();
	}
	if (trans->execute(Commit) == 0)
	{
	  trans->close();
	  // everything ok
	  break;
	}
	if (noRetries++ == 10 ||
	    trans->getNdbError().status != NdbError::TemporaryError)
	{
	  g_err << "execute " << r << " failed "
		<< trans->getNdbError().code << " "
		<< trans->getNdbError().message << endl;
	  trans->close();
	  DBUG_RETURN(NDBT_FAILED);
	}
	trans->close();
	NdbSleep_MilliSleep(100); // sleep before retying
      } while(1);
    }
  }

  if (GETNDB(step)->dropEventOperation(pOp)) {
    g_err << "dropEventOperation execution failed "
	  << GETNDB(step)->getNdbError().code << " "
	  << GETNDB(step)->getNdbError().message << endl;
    DBUG_RETURN(NDBT_FAILED);
  }
  
  DBUG_RETURN(NDBT_OK);
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
TESTCASE("EventOperationApplier", 
	 "Verify that if we apply the data we get from event "
	 "operation is the same as the original table"
	 "NOTE! No errors are allowed!" ){
  INITIALIZER(runCreateEvent);
  INITIALIZER(runCreateShadowTable);
  STEP(runEventApplier);
  STEP(runEventMixedLoad);
  FINALIZER(runDropEvent);
  FINALIZER(runVerify);
}
NDBT_TESTSUITE_END(test_event);

int main(int argc, const char** argv){
  ndb_init();
  return test_event.execute(argc, argv);
}

