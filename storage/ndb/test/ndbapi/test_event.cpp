/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
#include <TestNdbEventOperation.hpp>
#include <NdbAutoPtr.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <signaldata/DumpStateOrd.hpp>

#define GETNDB(ps) ((NDBT_NdbApiStep*)ps)->getNdb()

static int createEvent(Ndb *pNdb, const NdbDictionary::Table &tab,
                       bool merge_events = false)
{
  char eventName[1024];
  sprintf(eventName,"%s_EVENT",tab.getName());

  NdbDictionary::Dictionary *myDict = pNdb->getDictionary();

  if (!myDict) {
    g_err << "Dictionary not found " 
	  << pNdb->getNdbError().code << " "
	  << pNdb->getNdbError().message << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Event myEvent(eventName);
  myEvent.setTable(tab.getName());
  myEvent.addTableEvent(NdbDictionary::Event::TE_ALL); 
  for(int a = 0; a < tab.getNoOfColumns(); a++){
    myEvent.addEventColumn(a);
  }
  myEvent.mergeEvents(merge_events);

  int res = myDict->createEvent(myEvent); // Add event to database

  if (res == 0)
    myEvent.print();
  else if (myDict->getNdbError().classification ==
	   NdbError::SchemaObjectExists) 
  {
    g_info << "Event creation failed event exists\n";
    res = myDict->dropEvent(eventName);
    if (res) {
      g_err << "Failed to drop event: " 
	    << myDict->getNdbError().code << " : "
	    << myDict->getNdbError().message << endl;
      return NDBT_FAILED;
    }
    // try again
    res = myDict->createEvent(myEvent); // Add event to database
    if (res) {
      g_err << "Failed to create event (1): " 
	    << myDict->getNdbError().code << " : "
	    << myDict->getNdbError().message << endl;
      return NDBT_FAILED;
    }
  }
  else 
  {
    g_err << "Failed to create event (2): " 
	  << myDict->getNdbError().code << " : "
	  << myDict->getNdbError().message << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

static int dropEvent(Ndb *pNdb, const NdbDictionary::Table &tab)
{
  char eventName[1024];
  sprintf(eventName,"%s_EVENT",tab.getName());
  NdbDictionary::Dictionary *myDict = pNdb->getDictionary();
  if (!myDict) {
    g_err << "Dictionary not found " 
	  << pNdb->getNdbError().code << " "
	  << pNdb->getNdbError().message << endl;
    return NDBT_FAILED;
  }
  if (myDict->dropEvent(eventName)) {
    g_err << "Failed to drop event: " 
	  << myDict->getNdbError().code << " : "
	  << myDict->getNdbError().message << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}
 
static
NdbEventOperation *createEventOperation(Ndb *ndb,
                                        const NdbDictionary::Table &tab,
                                        int do_report_error = 1)
{
  char buf[1024];
  sprintf(buf, "%s_EVENT", tab.getName());
  NdbEventOperation *pOp= ndb->createEventOperation(buf);
  if (pOp == 0)
  {
    if (do_report_error)
      g_err << "createEventOperation: "
            << ndb->getNdbError().code << " "
            << ndb->getNdbError().message << endl;
    return 0;
  }
  int n_columns= tab.getNoOfColumns();
  for (int j = 0; j < n_columns; j++)
  {
    pOp->getValue(tab.getColumn(j)->getName());
    pOp->getPreValue(tab.getColumn(j)->getName());
  }
  if ( pOp->execute() )
  {
    if (do_report_error)
      g_err << "pOp->execute(): "
            << pOp->getNdbError().code << " "
            << pOp->getNdbError().message << endl;
    ndb->dropEventOperation(pOp);
    return 0;
  }
  return pOp;
}

static int runCreateEvent(NDBT_Context* ctx, NDBT_Step* step)
{
  bool merge_events = ctx->getProperty("MergeEvents");
  if (createEvent(GETNDB(step),* ctx->getTab(), merge_events) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

struct receivedEvent {
  Uint32 pk;
  Uint32 count;
  Uint32 event;
};

static int 
eventOperation(Ndb* pNdb, const NdbDictionary::Table &tab, void* pstats, int records)
{
  int i;
  const char function[] = "HugoTransactions::eventOperation: ";
  struct receivedEvent* recInsertEvent;
  NdbAutoObjArrayPtr<struct receivedEvent>
    p00( recInsertEvent = new struct receivedEvent[3*records] );
  struct receivedEvent* recUpdateEvent = &recInsertEvent[records];
  struct receivedEvent* recDeleteEvent = &recInsertEvent[2*records];

  EventOperationStats &stats = *(EventOperationStats*)pstats;

  stats.n_inserts = 0;
  stats.n_deletes = 0;
  stats.n_updates = 0;
  stats.n_consecutive = 0;
  stats.n_duplicates = 0;
  stats.n_inconsistent_gcis = 0;

  for (i = 0; i < records; i++) {
    recInsertEvent[i].pk    = 0xFFFFFFFF;
    recInsertEvent[i].count = 0;
    recInsertEvent[i].event = 0xFFFFFFFF;

    recUpdateEvent[i].pk    = 0xFFFFFFFF;
    recUpdateEvent[i].count = 0;
    recUpdateEvent[i].event = 0xFFFFFFFF;

    recDeleteEvent[i].pk    = 0xFFFFFFFF;
    recDeleteEvent[i].count = 0;
    recDeleteEvent[i].event = 0xFFFFFFFF;
  }

  NdbDictionary::Dictionary *myDict = pNdb->getDictionary();

  if (!myDict) {
    g_err << function << "Event Creation failedDictionary not found\n";
    return NDBT_FAILED;
  }

  int                  r = 0;
  NdbEventOperation    *pOp;

  char eventName[1024];
  sprintf(eventName,"%s_EVENT",tab.getName());
  Uint32 noEventColumnName = tab.getNoOfColumns();

  g_info << function << "create EventOperation\n";
  pOp = pNdb->createEventOperation(eventName);
  if ( pOp == NULL ) {
    g_err << function << "Event operation creation failed\n";
    return NDBT_FAILED;
  }

  g_info << function << "get values\n";
  NdbRecAttr* recAttr[1024];
  NdbRecAttr* recAttrPre[1024];

  const NdbDictionary::Table *_table = myDict->getTable(tab.getName());

  for (int a = 0; a < noEventColumnName; a++) {
    recAttr[a]    = pOp->getValue(_table->getColumn(a)->getName());
    recAttrPre[a] = pOp->getPreValue(_table->getColumn(a)->getName());
  }
  
  // set up the callbacks
  g_info << function << "execute\n";
  if (pOp->execute()) { // This starts changes to "start flowing"
    g_err << function << "operation execution failed: \n";
    g_err << pOp->getNdbError().code << " "
	  << pOp->getNdbError().message << endl;
    return NDBT_FAILED;
  }

  g_info << function << "ok\n";

  int count = 0;
  Uint32 last_inconsitant_gci = 0xEFFFFFF0;

  while (r < records){
    //printf("now waiting for event...\n");
    int res = pNdb->pollEvents(1000); // wait for event or 1000 ms

    if (res > 0) {
      //printf("got data! %d\n", r);
      NdbEventOperation *tmp;
      while ((tmp= pNdb->nextEvent()))
      {
	assert(tmp == pOp);
	r++;
	count++;

	Uint32 gci = pOp->getGCI();
	Uint32 pk = recAttr[0]->u_32_value();

        if (!pOp->isConsistent()) {
	  if (last_inconsitant_gci != gci) {
	    last_inconsitant_gci = gci;
	    stats.n_inconsistent_gcis++;
	  }
	  g_warning << "A node failure has occured and events might be missing\n";	
	}
	g_info << function << "GCI " << gci << ": " << count;
	struct receivedEvent* recEvent;
	switch (pOp->getEventType()) {
	case NdbDictionary::Event::TE_INSERT:
	  stats.n_inserts++;
	  g_info << " INSERT: ";
	  recEvent = recInsertEvent;
	  break;
	case NdbDictionary::Event::TE_DELETE:
	  stats.n_deletes++;
	  g_info << " DELETE: ";
	  recEvent = recDeleteEvent;
	  break;
	case NdbDictionary::Event::TE_UPDATE:
	  stats.n_updates++;
	  g_info << " UPDATE: ";
	  recEvent = recUpdateEvent;
	  break;
	default:
	case NdbDictionary::Event::TE_ALL:
	  abort();
	}

	if ((int)pk < records) {
	  recEvent[pk].pk = pk;
	  recEvent[pk].count++;
	}

	for (i = 1; i < noEventColumnName; i++) {
	  if (recAttr[i]->isNULL() >= 0) { // we have a value
	    g_info << " post[" << i << "]=";
	    if (recAttr[i]->isNULL() == 0) // we have a non-null value
	      g_info << recAttr[i]->u_32_value();
	    else                           // we have a null value
	      g_info << "NULL";
	  }
	  if (recAttrPre[i]->isNULL() >= 0) { // we have a value
	    g_info << " pre[" << i << "]=";
	    if (recAttrPre[i]->isNULL() == 0) // we have a non-null value
	      g_info << recAttrPre[i]->u_32_value();
	    else                              // we have a null value
	      g_info << "NULL";
	  }
	}
	g_info << endl;
      }
    } else
      ;//printf("timed out\n");
  }

  g_info << "dropping event operation" << endl;

  int res = pNdb->dropEventOperation(pOp);
  if (res != 0) {
    g_err << "operation execution failed\n";
    return NDBT_FAILED;
  }

  g_info << " ok" << endl;

  if (stats.n_inserts > 0) {
    stats.n_consecutive++;
  }
  if (stats.n_deletes > 0) {
    stats.n_consecutive++;
  }
  if (stats.n_updates > 0) {
    stats.n_consecutive++;
  }
  for (i = 0; i < (Uint32)records/3; i++) {
    if (recInsertEvent[i].pk != i) {
      stats.n_consecutive ++;
      ndbout << "missing insert pk " << i << endl;
    } else if (recInsertEvent[i].count > 1) {
      ndbout << "duplicates insert pk " << i
	     << " count " << recInsertEvent[i].count << endl;
      stats.n_duplicates += recInsertEvent[i].count-1;
    }
    if (recUpdateEvent[i].pk != i) {
      stats.n_consecutive ++;
      ndbout << "missing update pk " << i << endl;
    } else if (recUpdateEvent[i].count > 1) {
      ndbout << "duplicates update pk " << i
	     << " count " << recUpdateEvent[i].count << endl;
      stats.n_duplicates += recUpdateEvent[i].count-1;
    }
    if (recDeleteEvent[i].pk != i) {
      stats.n_consecutive ++;
      ndbout << "missing delete pk " << i << endl;
    } else if (recDeleteEvent[i].count > 1) {
      ndbout << "duplicates delete pk " << i
	     << " count " << recDeleteEvent[i].count << endl;
      stats.n_duplicates += recDeleteEvent[i].count-1;
    }
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
  // TODO should be removed
  // This should work wo/ next line
  //table_shadow.setNodeGroupIds(0, 0);
  GETNDB(step)->getDictionary()->createTable(table_shadow);
  if (GETNDB(step)->getDictionary()->getTable(buf))
    return NDBT_OK;

  g_err << "unsucessful create of " << buf << endl;
  return NDBT_FAILED;
}

int runDropShadowTable(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table *table= ctx->getTab();
  char buf[1024];
  sprintf(buf, "%s_SHADOW", table->getName());
  
  GETNDB(step)->getDictionary()->dropTable(buf);
  return NDBT_OK;
}

int runCreateDropEventOperation(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  //int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  EventOperationStats stats;

  //Ndb *pNdb=GETNDB(step);
  const NdbDictionary::Table& tab= *ctx->getTab();
  //NdbEventOperation    *pOp;
  char eventName[1024];
  sprintf(eventName,"%s_EVENT",tab.getName());
  //int noEventColumnName = tab.getNoOfColumns();

  for (int i= 0; i < loops; i++)
  {
#if 1
    if (eventOperation(GETNDB(step), tab, (void*)&stats, 0) != 0){
      return NDBT_FAILED;
    }
#else
    g_info << "create EventOperation\n";
    pOp = pNdb->createEventOperation(eventName);
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
  //int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());

  EventOperationStats stats;

  g_info << "***** start Id " << tId << endl;

  //  sleep(tId);

  if (eventOperation(GETNDB(step), *ctx->getTab(), (void*)&stats, 3*records) != 0){
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

  sleep(1);
#if 0
  sleep(5);
  sleep(theThreadIdCounter);
#endif
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
  
  if(ctx->getPropertyWait("LastGCI_hi", ~(Uint32)0))
  {
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }

  while(loops -- && !ctx->isTestStopped())
  {
    hugoTrans.clearTable(GETNDB(step), 0);

    if (hugoTrans.loadTable(GETNDB(step), 3*records, 1, true, 1) != 0){
      g_err << "FAIL " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    if (hugoTrans.pkDelRecords(GETNDB(step), 3*records, 1, true, 1) != 0){
      g_err << "FAIL " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    if (hugoTrans.loadTable(GETNDB(step), records, 1, true, 1) != 0){
      g_err << "FAIL " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, 1) != 0){
      g_err << "FAIL " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, 1) != 0){
      g_err << "FAIL " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, 1) != 0){
      g_err << "FAIL " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    ndbout_c("set(LastGCI_hi): %u/%u",
             Uint32(hugoTrans.m_latest_gci >> 32),
             Uint32(hugoTrans.m_latest_gci));
    ctx->setProperty("LastGCI_lo", Uint32(hugoTrans.m_latest_gci));
    ctx->setProperty("LastGCI_hi", Uint32(hugoTrans.m_latest_gci >> 32));
    if(ctx->getPropertyWait("LastGCI_hi", ~(Uint32)0))
    {
      g_err << "FAIL " << __LINE__ << endl;
      return NDBT_FAILED;
    }
  }
  ctx->stopTest();  
  return NDBT_OK;
}

int runDropEvent(NDBT_Context* ctx, NDBT_Step* step)
{
  return NDBT_OK;
}

int runVerify(NDBT_Context* ctx, NDBT_Step* step)
{
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

  int result = NDBT_OK;
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);

  char shadow[1024], buf[1024];
  sprintf(shadow, "%s_SHADOW", table->getName());
  const NdbDictionary::Table * table_shadow;
  if ((table_shadow = GETNDB(step)->getDictionary()->getTable(shadow)) == 0)
  {
    g_err << "Unable to get table " << shadow << endl;
    DBUG_RETURN(NDBT_FAILED);
  }
  
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp, *pCreate = 0;
  pCreate = pOp = GETNDB(step)->createEventOperation(buf);
  if ( pOp == NULL ) {
    g_err << "Event operation creation failed on %s" << buf << endl;
    DBUG_RETURN(NDBT_FAILED);
  }
  bool merge_events = ctx->getProperty("MergeEvents");
  pOp->mergeEvents(merge_events);

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
    result = NDBT_FAILED;
    goto end;
  }

  ctx->setProperty("LastGCI_hi", ~(Uint32)0);
  ctx->broadcast();

  while(!ctx->isTestStopped())
  {
    int count= 0;
    Uint64 stop_gci= ~(Uint64)0;
    Uint64 curr_gci = 0;
    Ndb* ndb= GETNDB(step);

    while(!ctx->isTestStopped() && curr_gci <= stop_gci)
    {
      ndb->pollEvents(100, &curr_gci);
      while ((pOp= ndb->nextEvent()) != 0)
      {
	assert(pOp == pCreate);
      
        if (pOp->getEventType() >=
            NdbDictionary::Event::TE_FIRST_NON_DATA_EVENT)
          continue;

	int noRetries= 0;
	do
	{
	  NdbTransaction *trans= GETNDB(step)->startTransaction();
	  if (trans == 0)
	  {
	    g_err << "startTransaction failed "
		  << GETNDB(step)->getNdbError().code << " "
		  << GETNDB(step)->getNdbError().message << endl;
	    result = NDBT_FAILED;
	    goto end;

	  }
	
	  NdbOperation *op= trans->getNdbOperation(table_shadow);
	  if (op == 0)
	  {
	    g_err << "getNdbOperation failed "
		  << trans->getNdbError().code << " "
		  << trans->getNdbError().message << endl;
	    result = NDBT_FAILED;
	    goto end;

	  }

	  switch (pOp->getEventType()) {
	  case NdbDictionary::Event::TE_INSERT:
	    if (op->writeTuple())
	    {
	      g_err << "insertTuple "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      result = NDBT_FAILED;
	      goto end;

	    }
	    break;
	  case NdbDictionary::Event::TE_DELETE:
	    if (op->deleteTuple())
	    {
	      g_err << "deleteTuple "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      result = NDBT_FAILED;
	      goto end;

	    }
	    break;
	  case NdbDictionary::Event::TE_UPDATE:
	    if (op->writeTuple())
	    {
	      g_err << "updateTuple "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      result = NDBT_FAILED;
	      goto end;

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
		result = NDBT_FAILED;
		goto end;

	      }
	      switch (pOp->getEventType()) {
	      case NdbDictionary::Event::TE_INSERT:
		if (recAttr[i]->isNULL() < 0)
		{
		  g_err << "internal error: missing value for insert\n";
		  result = NDBT_FAILED;
		  goto end;

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
	      result = NDBT_FAILED;
	      goto end;

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
		result = NDBT_FAILED;
		goto end;

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
		result = NDBT_FAILED;
		goto end;

	      }
	    }
	    break;
	  default:
	  case NdbDictionary::Event::TE_ALL:
	    abort();
	  }
	  if (trans->execute(Commit) == 0)
	  {
	    trans->close();
	    count++;
	    // everything ok
	    break;
	  }

	  if (trans->getNdbError().status == NdbError::PermanentError)
	  {
	    g_err << "Ignoring execute failed "
		  << trans->getNdbError().code << " "
		  << trans->getNdbError().message << endl;
	  
	    trans->close();
	    count++;
	    break;
	  }
	  else if (noRetries++ == 10)
	  {
	    g_err << "execute failed "
		  << trans->getNdbError().code << " "
		  << trans->getNdbError().message << endl;
	    trans->close();
	    result = NDBT_FAILED;
	    goto end;

	  }
	  trans->close();
	  NdbSleep_MilliSleep(100); // sleep before retying
	} while(1);
      }
      Uint32 stop_gci_hi = ctx->getProperty("LastGCI_hi", ~(Uint32)0);
      Uint32 stop_gci_lo = ctx->getProperty("LastGCI_lo", ~(Uint32)0);
      stop_gci = Uint64(stop_gci_lo) | (Uint64(stop_gci_hi) << 32);
    } 
    
    ndbout_c("Applied gci: %u/%u, %d events",
             Uint32(stop_gci >> 32), Uint32(stop_gci), count);
    if (hugoTrans.compare(GETNDB(step), shadow, 0))
    {
      g_err << "compare failed" << endl;
      result = NDBT_FAILED;
      goto end;
    }
    ctx->setProperty("LastGCI_hi", ~(Uint32)0);
    ctx->broadcast();
  }
  
end:
  if(pCreate)
  {
    if (GETNDB(step)->dropEventOperation(pCreate)) {
      g_err << "dropEventOperation execution failed "
	    << GETNDB(step)->getNdbError().code << " "
	    << GETNDB(step)->getNdbError().message << endl;
      result = NDBT_FAILED;
    }
  }
  ctx->stopTest();
  DBUG_RETURN(result);
}

int runEventListenerUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  
  int result = NDBT_OK;
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);
  Ndb* ndb= GETNDB(step);

  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp, *pCreate = 0;
  pCreate = pOp = ndb->createEventOperation(buf);
  if ( pOp == NULL ) {
    g_err << "Event operation creation failed on %s" << buf << endl;
    return NDBT_FAILED;
  }
  
  int i;
  int n_columns= table->getNoOfColumns();
  NdbRecAttr* recAttr[1024];
  NdbRecAttr* recAttrPre[1024];
  for (i = 0; i < n_columns; i++) {
    recAttr[i]    = pOp->getValue(table->getColumn(i)->getName());
    recAttrPre[i] = pOp->getPreValue(table->getColumn(i)->getName());
  }

  if (pOp->execute()) 
  { // This starts changes to "start flowing"
    g_err << "execute operation execution failed: \n";
    g_err << pOp->getNdbError().code << " "
	  << pOp->getNdbError().message << endl;
    result = NDBT_FAILED;
    goto end;
  }
  
  while(!ctx->isTestStopped())
  {
    Uint64 curr_gci = 0;
    while(!ctx->isTestStopped())
    {
      ndb->pollEvents(100, &curr_gci);
      while ((pOp= ndb->nextEvent()) != 0)
      {
	assert(pOp == pCreate);
      } 
    }
  }
  
end:
  if(pCreate)
  {
    if (ndb->dropEventOperation(pCreate)) {
      g_err << "dropEventOperation execution failed "
	    << ndb->getNdbError().code << " "
	    << ndb->getNdbError().message << endl;
      result = NDBT_FAILED;
    }
  }
  return result;
}

int runRestarter(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted(60) != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }
  
  while(result != NDBT_FAILED && !ctx->isTestStopped()){

    int id = lastId % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl; 
    if(restarter.restartOneDbNode(nodeId, false, false, true) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }    

    if(restarter.waitClusterStarted(60) != 0){
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }
    
    lastId++;
    i++;
  }

  return result;
}

int runRestarterLoop(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted(60) != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }
  
  while(result != NDBT_FAILED 
	&& !ctx->isTestStopped() 
	&& i < loops)
  {
    int id = lastId % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl; 
    if(restarter.restartOneDbNode(nodeId, false, false, true) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }    
    
    if(restarter.waitClusterStarted(60) != 0){
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }
    
    lastId++;
    i++;
  }

  ctx->stopTest();
  return result;
}

Vector<const NdbDictionary::Table*> pTabs;
Vector<const NdbDictionary::Table*> pShadowTabs;

static int getAllTables(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("getAllTables");
  Ndb * ndb= GETNDB(step);
  NdbDictionary::Dictionary * dict = ndb->getDictionary();
  pTabs.clear();

  for (int i= 0; i < ctx->getNumTables(); i++)
  {
    const NdbDictionary::Table *pTab= dict->getTable(ctx->getTableName(i));
    if (pTab == 0)
    {
      ndbout << "Failed to get table" << endl;
      ndbout << dict->getNdbError() << endl;
      DBUG_RETURN(NDBT_FAILED);
    }
    pTabs.push_back(pTab);
    ndbout << " " << ctx->getTableName(i);
  }
  pTabs.push_back(NULL);
  ndbout << endl;

  DBUG_RETURN(NDBT_OK);
}

static int createAllEvents(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("createAllEvents");
  Ndb * ndb= GETNDB(step);
  for (int i= 0; pTabs[i]; i++)
  {
    if (createEvent(ndb,*pTabs[i]))
    {
      DBUG_RETURN(NDBT_FAILED);
    }
  }
  DBUG_RETURN(NDBT_OK);
}

static int dropAllEvents(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("dropAllEvents");
  Ndb * ndb= GETNDB(step);
  int i;

  for (i= 0; pTabs[i]; i++)
  {
    if (dropEvent(ndb,*pTabs[i]))
    {
      DBUG_RETURN(NDBT_FAILED);
    }
  }
  DBUG_RETURN(NDBT_OK);
}

static int createAllShadows(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("createAllShadows");
  Ndb * ndb= GETNDB(step);
  NdbDictionary::Dictionary * dict = ndb->getDictionary();
  // create a "shadow" table for each table
  for (int i= 0; pTabs[i]; i++)
  {
    char buf[1024];
    sprintf(buf, "%s_SHADOW", pTabs[i]->getName());

    dict->dropTable(buf);
    if (dict->getTable(buf))
    {
      DBUG_RETURN(NDBT_FAILED);
    }

    NdbDictionary::Table table_shadow(*pTabs[i]);
    table_shadow.setName(buf);
    if (dict->createTable(table_shadow))
    {
      g_err << "createTable(" << buf << ") "
	    << dict->getNdbError().code << " "
	    << dict->getNdbError().message << endl;
      DBUG_RETURN(NDBT_FAILED);
    }
    pShadowTabs.push_back(dict->getTable(buf));
    if (!pShadowTabs[i])
    {
      g_err << "getTable(" << buf << ") "
	    << dict->getNdbError().code << " "
	    << dict->getNdbError().message << endl;
      DBUG_RETURN(NDBT_FAILED);
    }
  }
  DBUG_RETURN(NDBT_OK);
}

static int dropAllShadows(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("dropAllShadows");
  Ndb * ndb= GETNDB(step);
  NdbDictionary::Dictionary * dict = ndb->getDictionary();

  for (int i= 0; pTabs[i]; i++)
  {
    char buf[1024];
    sprintf(buf, "%s_SHADOW", pTabs[i]->getName());
    if (dict->dropTable(buf))
    {
      DBUG_RETURN(NDBT_FAILED);
    }
  }  
  DBUG_RETURN(NDBT_OK);
}

static int start_transaction(Ndb *ndb, Vector<HugoOperations*> &ops)
{
  if (ops[0]->startTransaction(ndb) != NDBT_OK)
    return -1;
  NdbTransaction * t= ops[0]->getTransaction();
  for (int i= ops.size()-1; i > 0; i--)
  {
    ops[i]->setTransaction(t,true);
  }
  return 0;
}

static int close_transaction(Ndb *ndb, Vector<HugoOperations*> &ops)
{
  if (ops[0]->closeTransaction(ndb) != NDBT_OK)
    return -1;
  for (int i= ops.size()-1; i > 0; i--)
  {
    ops[i]->setTransaction(NULL,true);
  }
  return 0;
}

static int execute_commit(Ndb *ndb, Vector<HugoOperations*> &ops)
{
  if (ops[0]->execute_Commit(ndb) != NDBT_OK)
    return -1;
  return 0;
}

static int copy_events(Ndb *ndb)
{
  DBUG_ENTER("copy_events");
  int r= 0;
  NdbDictionary::Dictionary * dict = ndb->getDictionary();
  int n_inserts= 0;
  int n_updates= 0;
  int n_deletes= 0;
  while (1)
  {
    int res= ndb->pollEvents(1000); // wait for event or 1000 ms
    DBUG_PRINT("info", ("pollEvents res=%d", res));
    if (res <= 0)
    {
      break;
    }
    NdbEventOperation *pOp;
    while ((pOp= ndb->nextEvent()))
    {
      char buf[1024];
      sprintf(buf, "%s_SHADOW", pOp->getEvent()->getTable()->getName());
      const NdbDictionary::Table *table= dict->getTable(buf);
      
      if (table == 0)
      {
	g_err << "unable to find table " << buf << endl;
	DBUG_RETURN(-1);
      }

      if (pOp->isOverrun())
      {
	g_err << "buffer overrun\n";
	DBUG_RETURN(-1);
      }
      r++;
      
      if (!pOp->isConsistent()) {
	g_err << "A node failure has occured and events might be missing\n";
	DBUG_RETURN(-1);
      }
	
      int noRetries= 0;
      do
      {
	NdbTransaction *trans= ndb->startTransaction();
	if (trans == 0)
	{
	  g_err << "startTransaction failed "
		<< ndb->getNdbError().code << " "
		<< ndb->getNdbError().message << endl;
	  DBUG_RETURN(-1);
	}
	
	NdbOperation *op= trans->getNdbOperation(table);
	if (op == 0)
	{
	  g_err << "getNdbOperation failed "
		<< trans->getNdbError().code << " "
		<< trans->getNdbError().message << endl;
	  DBUG_RETURN(-1);
	}
	
	switch (pOp->getEventType()) {
	case NdbDictionary::Event::TE_INSERT:
	  if (op->insertTuple())
	  {
	    g_err << "insertTuple "
		  << op->getNdbError().code << " "
		  << op->getNdbError().message << endl;
	    DBUG_RETURN(-1);
	  }
	  if (noRetries == 0)
	  {
	    n_inserts++;
	  }
	  break;
	case NdbDictionary::Event::TE_DELETE:
	  if (op->deleteTuple())
	  {
	    g_err << "deleteTuple "
		  << op->getNdbError().code << " "
		  << op->getNdbError().message << endl;
	    DBUG_RETURN(-1);
	  }
	  if (noRetries == 0)
	  {
	    n_deletes++;
	  }
	  break;
	case NdbDictionary::Event::TE_UPDATE:
	  if (op->updateTuple())
	  {
	    g_err << "updateTuple "
		  << op->getNdbError().code << " "
		  << op->getNdbError().message << endl;
	    DBUG_RETURN(-1);
	  }
	  if (noRetries == 0)
	  {
	    n_updates++;
	  }
	  break;
	default:
	  abort();
	}
	
	{
	  for (const NdbRecAttr *pk= pOp->getFirstPkAttr();
	       pk;
	       pk= pk->next())
	  {
	    if (pk->isNULL())
	    {
	      g_err << "internal error: primary key isNull()="
		    << pk->isNULL() << endl;
	      DBUG_RETURN(NDBT_FAILED);
	    }
	    if (op->equal(pk->getColumn()->getColumnNo(),pk->aRef()))
	    {
	      g_err << "equal " << pk->getColumn()->getColumnNo() << " "
		    << op->getNdbError().code << " "
		    << op->getNdbError().message << endl;
	      DBUG_RETURN(NDBT_FAILED);
	    }
	  }
	}
	
	switch (pOp->getEventType()) {
	case NdbDictionary::Event::TE_INSERT:
	{
	  for (const NdbRecAttr *data= pOp->getFirstDataAttr();
	       data;
	       data= data->next())
	  {
	    if (data->isNULL() < 0 ||
		op->setValue(data->getColumn()->getColumnNo(),
			     data->isNULL() ? 0:data->aRef()))
	    {
	      g_err << "setValue(insert) " << data->getColumn()->getColumnNo()
		    << " " << op->getNdbError().code
		    << " " << op->getNdbError().message << endl;
	      DBUG_RETURN(-1);
	    }
	  }
	  break;
	}
	case NdbDictionary::Event::TE_DELETE:
	  break;
	case NdbDictionary::Event::TE_UPDATE:
	{
	  for (const NdbRecAttr *data= pOp->getFirstDataAttr();
	       data;
	       data= data->next())
	  {
	    if (data->isNULL() >= 0 &&
		op->setValue(data->getColumn()->getColumnNo(),
			     data->isNULL() ? 0:data->aRef()))
	    {
	      g_err << "setValue(update) " << data->getColumn()->getColumnNo()
		    << " " << op->getNdbError().code
		    << " " << op->getNdbError().message << endl;
	      DBUG_RETURN(NDBT_FAILED);
	    }
	  }
	  break;
	}
	default:
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
	  DBUG_RETURN(-1);
	}
	trans->close();
	NdbSleep_MilliSleep(100); // sleep before retying
      } while(1);
    } // for
  } // while(1)
  g_info << "n_updates: " << n_updates << " "
	 << "n_inserts: " << n_inserts << " "
	 << "n_deletes: " << n_deletes << endl;
  DBUG_RETURN(r);
}

static int verify_copy(Ndb *ndb,
		       Vector<const NdbDictionary::Table *> &tabs1,
		       Vector<const NdbDictionary::Table *> &tabs2)
{
  for (unsigned i= 0; i < tabs1.size(); i++)
    if (tabs1[i])
    {
      HugoTransactions hugoTrans(*tabs1[i]);
      if (hugoTrans.compare(ndb, tabs2[i]->getName(), 0))
	return -1;
    }
  return 0;
}

static int createEventOperations(Ndb * ndb)
{
  DBUG_ENTER("createEventOperations");
  int i;

  // creat all event ops
  for (i= 0; pTabs[i]; i++)
  {
    char buf[1024];
    sprintf(buf, "%s_EVENT", pTabs[i]->getName());
    NdbEventOperation *pOp= ndb->createEventOperation(buf);
    if ( pOp == NULL )
    {
      DBUG_RETURN(NDBT_FAILED);
    }

    int n_columns= pTabs[i]->getNoOfColumns();
    for (int j = 0; j < n_columns; j++)
    {
      pOp->getValue(pTabs[i]->getColumn(j)->getName());
      pOp->getPreValue(pTabs[i]->getColumn(j)->getName());
    }

    if ( pOp->execute() )
    {
      DBUG_RETURN(NDBT_FAILED);
    }
  }

  DBUG_RETURN(NDBT_OK);
}

static int createAllEventOperations(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("createAllEventOperations");
  Ndb * ndb= GETNDB(step);
  int r= createEventOperations(ndb);
  if (r != NDBT_OK)
  {
    DBUG_RETURN(NDBT_FAILED);
  }
  DBUG_RETURN(NDBT_OK);
}

static int dropEventOperations(Ndb * ndb)
{
  DBUG_ENTER("dropEventOperations");

  NdbEventOperation *pOp;
  while ( (pOp= ndb->getEventOperation()) )
  {
    if (ndb->dropEventOperation(pOp))
    {
      DBUG_RETURN(NDBT_FAILED);
    }
  }

  DBUG_RETURN(NDBT_OK);
}

static int dropAllEventOperations(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("dropAllEventOperations");
  Ndb * ndb= GETNDB(step);
  int r= dropEventOperations(ndb);
  if (r != NDBT_OK)
  {
    DBUG_RETURN(NDBT_FAILED);
  }
  DBUG_RETURN(NDBT_OK);
}

static int runMulti(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("runMulti");

  Ndb * ndb= GETNDB(step);

  int no_error= 1;
  int i;

  if (createEventOperations(ndb))
  {
    DBUG_RETURN(NDBT_FAILED);
  }

  // create a hugo operation per table
  Vector<HugoOperations *> hugo_ops;
  for (i= 0; no_error && pTabs[i]; i++)
  {
    hugo_ops.push_back(new HugoOperations(*pTabs[i]));
  }

  int n_records= 3;
  // insert n_records records per table
  do {
    if (start_transaction(ndb, hugo_ops))
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }
    for (i= 0; no_error && pTabs[i]; i++)
    {
      hugo_ops[i]->pkInsertRecord(ndb, 0, n_records);
    }
    if (execute_commit(ndb, hugo_ops))
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }
    if(close_transaction(ndb, hugo_ops))
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }
  } while(0);

  // copy events and verify
  do {
    if (copy_events(ndb) < 0)
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }
    if (verify_copy(ndb, pTabs, pShadowTabs))
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }
  } while (0);

  // update n_records-1 records in first table
  do {
    if (start_transaction(ndb, hugo_ops))
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }

    hugo_ops[0]->pkUpdateRecord(ndb, n_records-1);

    if (execute_commit(ndb, hugo_ops))
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }
    if(close_transaction(ndb, hugo_ops))
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }
  } while(0);

  // copy events and verify
  do {
    if (copy_events(ndb) < 0)
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }
    if (verify_copy(ndb, pTabs, pShadowTabs))
    {
      no_error= 0;
      DBUG_RETURN(NDBT_FAILED);
    }
  } while (0);

  if (dropEventOperations(ndb))
  {
    DBUG_RETURN(NDBT_FAILED);
  }

  if (no_error)
    DBUG_RETURN(NDBT_OK);
  DBUG_RETURN(NDBT_FAILED);
}

static int runMulti_NR(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("runMulti");

  int records = ctx->getNumRecords();
  int loops = ctx->getNumLoops();
  Ndb * ndb= GETNDB(step);

  int i;

  if (createEventOperations(ndb))
  {
    DBUG_RETURN(NDBT_FAILED);
  }

  for (i= 0; pTabs[i]; i++)
  {
    HugoTransactions hugo(*pTabs[i]);
    if (hugo.loadTable(ndb, records, 1, true, 1))
    {
      DBUG_RETURN(NDBT_FAILED);
    }
    // copy events and verify
    if (copy_events(ndb) < 0)
    {
      DBUG_RETURN(NDBT_FAILED);
    }
  }

  if (verify_copy(ndb, pTabs, pShadowTabs))
  {
    DBUG_RETURN(NDBT_FAILED);
  }

  {
    NdbRestarts restarts;
    for (int j= 0; j < loops; j++)
    {
      // restart a node
      int timeout = 240;
      if (restarts.executeRestart("RestartRandomNodeAbort", timeout))
      {
	DBUG_RETURN(NDBT_FAILED);
      }

      sleep(5);
      // update all tables
      for (i= 0; pTabs[i]; i++)
      {
	HugoTransactions hugo(*pTabs[i]);
	if (hugo.pkUpdateRecords(ndb, records, 1, 1))
	{
	  DBUG_RETURN(NDBT_FAILED);
	}
	if (copy_events(ndb) < 0)
	{
	  DBUG_RETURN(NDBT_FAILED);
	}
      }

      // copy events and verify
      if (verify_copy(ndb, pTabs, pShadowTabs))
      {
	DBUG_RETURN(NDBT_FAILED);
      }
    }
  }

  if (dropEventOperations(ndb))
  {
    DBUG_RETURN(NDBT_FAILED);
  }

  DBUG_RETURN(NDBT_OK);
}

static int restartAllNodes()
{
  NdbRestarter restarter;
  int id = 0;
  do {
    int nodeId = restarter.getDbNodeId(id++);
    ndbout << "Restart node " << nodeId << endl; 
    if(restarter.restartOneDbNode(nodeId, false, false, true) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      break;
    }    
    if(restarter.waitClusterStarted(60) != 0){
      g_err << "Cluster failed to start" << endl;
      break;
    }
    id = id % restarter.getNumDbNodes();
  } while (id);
  return id != 0;
}

static int runCreateDropNR(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("runCreateDropNR");
  Ndb * ndb= GETNDB(step);
  int result = NDBT_OK;
  NdbRestarter restarter;
  int loops = ctx->getNumLoops();

  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }
  do
  {
    result = NDBT_FAILED;
    const NdbDictionary::Table* pTab = ctx->getTab();
    if (createEvent(ndb, *pTab))
    {
      g_err << "createEvent failed" << endl;
      break;
    }
    NdbEventOperation *pOp= createEventOperation(ndb, *pTab);
    if (pOp == 0)
    {
      g_err << "Failed to createEventOperation" << endl;
      break;
    }
    if (dropEvent(ndb, *pTab))
    {
      g_err << "Failed to dropEvent()" << endl;
      break;
    }
    ndbout << "Restarting with dropped events with subscribers" << endl;
    if (restartAllNodes())
      break;
    if (ndb->getDictionary()->dropTable(pTab->getName()) != 0){
      g_err << "Failed to drop " << pTab->getName() <<" in db" << endl;
      break;
    }
    ndbout << "Restarting with dropped events and dropped "
           << "table with subscribers" << endl;
    if (restartAllNodes())
      break;
    if (ndb->dropEventOperation(pOp))
    {
      g_err << "Failed dropEventOperation" << endl;
      break;
    }
    NdbDictionary::Table tmp(*pTab);
    //tmp.setNodeGroupIds(0, 0);
    if (ndb->getDictionary()->createTable(tmp) != 0){
      g_err << "createTable failed: "
            << ndb->getDictionary()->getNdbError() << endl;
      break;
    }
    result = NDBT_OK;
  } while (--loops);

  DBUG_RETURN(result);
}

static
int
runSubscribeUnsubscribe(NDBT_Context* ctx, NDBT_Step* step)
{
  char buf[1024];
  const NdbDictionary::Table & tab = * ctx->getTab();
  sprintf(buf, "%s_EVENT", tab.getName());
  Ndb* ndb = GETNDB(step);
  int loops = 5 * ctx->getNumLoops();

  while (--loops)
  {
    NdbEventOperation *pOp= ndb->createEventOperation(buf);
    if (pOp == 0)
    {
      g_err << "createEventOperation: "
	    << ndb->getNdbError().code << " "
	    << ndb->getNdbError().message << endl;
      return NDBT_FAILED;
    }
    
    int n_columns= tab.getNoOfColumns();
    for (int j = 0; j < n_columns; j++)
    {
      pOp->getValue(tab.getColumn(j)->getName());
      pOp->getPreValue(tab.getColumn(j)->getName());
    }
    if ( pOp->execute() )
    {
      g_err << "pOp->execute(): "
	    << pOp->getNdbError().code << " "
	    << pOp->getNdbError().message << endl;
      
      ndb->dropEventOperation(pOp);
      
      return NDBT_FAILED;
    }
    
    if (ndb->dropEventOperation(pOp))
    {
      g_err << "pOp->execute(): "
	    << ndb->getNdbError().code << " "
	    << ndb->getNdbError().message << endl;
      return NDBT_FAILED;
    }
  }
  
  return NDBT_OK;
}

int 
runScanUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", (Uint32)0);
  int abort = ctx->getProperty("AbortProb", (Uint32)0);
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) 
  {
    if (hugoTrans.scanUpdateRecords(GETNDB(step), 0, abort, 
				    parallelism) == NDBT_FAILED){
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int 
runInsertDeleteUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) 
  {
    if (hugoTrans.loadTable(GETNDB(step), records, 1) != 0){
      return NDBT_FAILED;
    }
    if (utilTrans.clearTable(GETNDB(step),  records) != 0){
      return NDBT_FAILED;
    }
  }
  
  return NDBT_OK;
}

int 
runBug31701(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;

  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }
  // This should really wait for applier to start...10s is likely enough
  NdbSleep_SecSleep(10);

  int nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  if (restarter.dumpStateOneNode(nodeId, val2, 2))
    return NDBT_FAILED;
  
  restarter.insertErrorInNode(nodeId, 13033);
  if (restarter.waitNodesNoStart(&nodeId, 1))
    return NDBT_FAILED;

  if (restarter.startNodes(&nodeId, 1))
    return NDBT_FAILED;

  if (restarter.waitClusterStarted())
    return NDBT_FAILED;

  
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  
  if(ctx->getPropertyWait("LastGCI", ~(Uint32)0))
  if(ctx->getPropertyWait("LastGCI_hi", ~(Uint32)0))
  {
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }

  hugoTrans.clearTable(GETNDB(step), 0);
  
  if (hugoTrans.loadTable(GETNDB(step), 3*records, 1, true, 1) != 0){
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  
  if (hugoTrans.pkDelRecords(GETNDB(step), 3*records, 1, true, 1) != 0){
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  if (hugoTrans.loadTable(GETNDB(step), records, 1, true, 1) != 0){
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, 1) != 0){
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, 1) != 0){
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 1, 1) != 0){
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  
  ctx->setProperty("LastGCI_lo", Uint32(hugoTrans.m_latest_gci));
  ctx->setProperty("LastGCI_hi", Uint32(hugoTrans.m_latest_gci >> 32));
  if(ctx->getPropertyWait("LastGCI_hi", ~(Uint32)0))
  {
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }

  ctx->stopTest();  
  return NDBT_OK;
}

NDBT_TESTSUITE(test_event);
TESTCASE("BasicEventOperation", 
	 "Verify that we can listen to Events"
	 "NOTE! No errors are allowed!" )
{
#if 0
  TABLE("T1");
  TABLE("T3");
  TABLE("T5");
  TABLE("T6");
  TABLE("T8");
#endif
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
  FINALIZER(runDropShadowTable);
}
TESTCASE("EventOperationApplier_NR", 
	 "Verify that if we apply the data we get from event "
	 "operation is the same as the original table"
	 "NOTE! No errors are allowed!" ){
  INITIALIZER(runCreateEvent);
  INITIALIZER(runCreateShadowTable);
  STEP(runEventApplier);
  STEP(runEventMixedLoad);
  STEP(runRestarter);
  FINALIZER(runDropEvent);
  FINALIZER(runVerify);
  FINALIZER(runDropShadowTable);
}
TESTCASE("MergeEventOperationApplier", 
	 "Verify that if we apply the data we get from merged event "
	 "operation is the same as the original table"
	 "NOTE! No errors are allowed!" ){
  TC_PROPERTY("MergeEvents", 1);
  INITIALIZER(runCreateEvent);
  INITIALIZER(runCreateShadowTable);
  STEP(runEventApplier);
  STEP(runEventMixedLoad);
  FINALIZER(runDropEvent);
  FINALIZER(runVerify);
  FINALIZER(runDropShadowTable);
}
TESTCASE("MergeEventOperationApplier_NR", 
	 "Verify that if we apply the data we get from merged event "
	 "operation is the same as the original table"
	 "NOTE! No errors are allowed!" ){
  TC_PROPERTY("MergeEvents", 1);
  INITIALIZER(runCreateEvent);
  INITIALIZER(runCreateShadowTable);
  STEP(runEventApplier);
  STEP(runEventMixedLoad);
  STEP(runRestarter);
  FINALIZER(runDropEvent);
  FINALIZER(runVerify);
  FINALIZER(runDropShadowTable);
}
TESTCASE("Multi", 
	 "Verify that we can work with all tables in parallell"
	 "NOTE! HugoOperations::startTransaction, pTrans != NULL errors, "
	 "are allowed!" ){
  ALL_TABLES();
  INITIALIZER(getAllTables);
  INITIALIZER(createAllEvents);
  INITIALIZER(createAllShadows);
  STEP(runMulti);
  FINALIZER(dropAllShadows);
  FINALIZER(dropAllEvents);
}
TESTCASE("Multi_NR", 
	 "Verify that we can work with all tables in parallell"
	 "NOTE! HugoOperations::startTransaction, pTrans != NULL errors, "
	 "are allowed!" ){
  ALL_TABLES();
  INITIALIZER(getAllTables);
  INITIALIZER(createAllEvents);
  INITIALIZER(createAllShadows);
  STEP(runMulti_NR);
  FINALIZER(dropAllShadows);
  FINALIZER(dropAllEvents);
}
TESTCASE("CreateDropNR", 
	 "Verify that we can Create and Drop in any order"
	 "NOTE! No errors are allowed!" ){
  FINALIZER(runCreateDropNR);
}
TESTCASE("SubscribeUnsubscribe", 
	 "A bunch of threads doing subscribe/unsubscribe in loop"
	 "NOTE! No errors are allowed!" ){
  INITIALIZER(runCreateEvent);
  STEPS(runSubscribeUnsubscribe, 16);
  FINALIZER(runDropEvent);
}
TESTCASE("Bug27169", ""){
  INITIALIZER(runCreateEvent);
  STEP(runEventListenerUntilStopped);
  STEP(runInsertDeleteUntilStopped);
  STEP(runScanUpdateUntilStopped);
  STEP(runRestarterLoop);
  FINALIZER(runDropEvent);
}
TESTCASE("Bug31701", ""){
  INITIALIZER(runCreateEvent);
  INITIALIZER(runCreateShadowTable);
  STEP(runEventApplier);
  STEP(runBug31701);
  FINALIZER(runDropEvent);
  FINALIZER(runDropShadowTable);
}
NDBT_TESTSUITE_END(test_event);

int main(int argc, const char** argv){
  ndb_init();
  test_event.setCreateAllTables(true);
  return test_event.execute(argc, argv);
}

template class Vector<HugoOperations *>;
template class Vector<NdbEventOperation *>;
template class Vector<NdbRecAttr*>;
template class Vector<Vector<NdbRecAttr*> >;
