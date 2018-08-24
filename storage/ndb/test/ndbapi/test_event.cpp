/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <TestNdbEventOperation.hpp>
#include <NdbAutoPtr.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <NdbEnv.h>
#include <Bitmask.hpp>
#include "../src/kernel/ndbd.hpp"

#define CHK(b,e) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ \
          << ": " << e << endl; \
    return NDBT_FAILED; \
  }

static void inline generateEventName(char* eventName,
                                     const char* tabName, uint eventId)
{
  if (eventId == 0)
  {
    sprintf(eventName, "%s_EVENT", tabName);
  }
  else
  {
    sprintf(eventName, "%s_EVENT_%d", tabName, eventId);
  }
}

static int createEvent(Ndb *pNdb,
                       const NdbDictionary::Table &tab,
                       bool merge_events,
                       bool report,
                       uint eventId = 0)
{
  char eventName[1024];
  generateEventName(eventName, tab.getName(), eventId);

  NdbDictionary::Dictionary *myDict = pNdb->getDictionary();

  if (!myDict) {
    g_err << "Dictionary not found " 
	  << pNdb->getNdbError().code << " "
	  << pNdb->getNdbError().message << endl;
    return NDBT_FAILED;
  }
  
  myDict->dropEvent(eventName);

  NdbDictionary::Event myEvent(eventName);
  myEvent.setTable(tab.getName());
  myEvent.addTableEvent(NdbDictionary::Event::TE_ALL); 
  for(int a = 0; a < tab.getNoOfColumns(); a++){
    myEvent.addEventColumn(a);
  }
  myEvent.mergeEvents(merge_events);

  if (report)
    myEvent.setReport(NdbDictionary::Event::ER_SUBSCRIBE);

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

static int createEvent(Ndb *pNdb, 
                       const NdbDictionary::Table &tab,
                       NDBT_Context* ctx)
{
  bool merge_events = ctx->getProperty("MergeEvents");
  bool report = ctx->getProperty("ReportSubscribe");

  return createEvent(pNdb, tab, merge_events, report);
}

static int dropEvent(Ndb *pNdb, const NdbDictionary::Table &tab,
                     uint eventId = 0)
{
  char eventName[1024];
  generateEventName(eventName, tab.getName(), eventId);
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
                                        int do_report_error = 1,
                                        int eventId = 0)
{
  char eventName[1024];
  generateEventName(eventName, tab.getName(), eventId);
  NdbEventOperation *pOp= ndb->createEventOperation(eventName);
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
  if (createEvent(GETNDB(step),* ctx->getTab(), ctx) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

Uint32 setAnyValue(Ndb* ndb, NdbTransaction* trans, int rowid, int updVal)
{
  /* XOR 2 32bit words of transid together */
  Uint64 transId = trans->getTransactionId();
  return (Uint32)(transId ^ (transId >> 32));
}

bool checkAnyValueTransId(Uint64 transId, Uint32 anyValue)
{
  return transId && (anyValue == Uint32(transId ^ (transId >> 32)));
}

struct receivedEvent {
  Uint32 pk;
  Uint32 count;
  Uint32 event;
};

static int 
eventOperation(Ndb* pNdb, const NdbDictionary::Table &tab, void* pstats, int records)
{
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

  for (int i = 0; i < records; i++) {
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
  int noEventColumnName = tab.getNoOfColumns();

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

  for (int a = 0; a < (int)noEventColumnName; a++) {
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
  Uint64 last_inconsitant_gci = (Uint64)-1;

  while (r < records){
    //printf("now waiting for event...\n");
    int res = pNdb->pollEvents(1000); // wait for event or 1000 ms

    if (res > 0) {
      //printf("got data! %d\n", r);
      NdbEventOperation *tmp;
      while ((tmp= pNdb->nextEvent()))
      {
	require(tmp == pOp);
	r++;
	count++;

	Uint64 gci = pOp->getGCI();
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

        /* Check event transaction id */
        Uint32 anyValue = pOp->getAnyValue();
        Uint64 transId = pOp->getTransId();
        if (anyValue)
        {
          if (!checkAnyValueTransId(transId, anyValue))
          {
            g_err << "ERROR : TransId and AnyValue mismatch.  "
                  << "Transid : " << transId
                  << ", AnyValue : " << anyValue
                  << ", Expected AnyValue : "
                  << (Uint32) ((transId >> 32) ^ transId)
                  << endl;
            abort();
            return NDBT_FAILED;
          }
        }

	if ((int)pk < records) {
	  recEvent[pk].pk = pk;
	  recEvent[pk].count++;
	}

	for (int i = 1; i < (int)noEventColumnName; i++) {
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
    }
    else
    {
      ;//printf("timed out\n");
    }
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
  for (int i = 0; i < (int)records/3; i++) {
    if (recInsertEvent[i].pk != Uint32(i)) {
      stats.n_consecutive ++;
      ndbout << "missing insert pk " << i << endl;
    } else if (recInsertEvent[i].count > 1) {
      ndbout << "duplicates insert pk " << i
	     << " count " << recInsertEvent[i].count << endl;
      stats.n_duplicates += recInsertEvent[i].count-1;
    }
    if (recUpdateEvent[i].pk != Uint32(i)) {
      stats.n_consecutive ++;
      ndbout << "missing update pk " << i << endl;
    } else if (recUpdateEvent[i].count > 1) {
      ndbout << "duplicates update pk " << i
	     << " count " << recUpdateEvent[i].count << endl;
      stats.n_duplicates += recUpdateEvent[i].count-1;
    }
    if (recDeleteEvent[i].pk != Uint32(i)) {
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


int
listenEmptyEpochs(Ndb* ndb, bool useV2)
{
  Uint32 numPollEmptyEpochs = 0;
  Uint32 numEventEmptyEpochs = 0;
  Uint64 lastEpoch = 0;
  Uint64 stopEpoch = 0;
  while (true)
  {
    Uint64 highestQueuedEpoch = 0;
    int res = 0;

    if (useV2)
    {
      res = ndb->pollEvents2(1000, &highestQueuedEpoch);
    }
    else
    {
      res = ndb->pollEvents(1000, &highestQueuedEpoch);
    }

    if (lastEpoch == 0)
    {
      g_err << "Start epoch is "
            << (highestQueuedEpoch >> 32)
            << "/"
            << (highestQueuedEpoch & 0xffffffff)
            << endl;
      lastEpoch = highestQueuedEpoch;
      stopEpoch = ((highestQueuedEpoch >> 32) + 10) << 32;
      numPollEmptyEpochs = 1;
    }
    else
    {
      if (highestQueuedEpoch != lastEpoch)
      {
        g_err << "- poll empty epoch : "
              << (highestQueuedEpoch >> 32)
              << "/"
              << (highestQueuedEpoch & 0xffffffff)
              << endl;
        numPollEmptyEpochs++;
        lastEpoch = highestQueuedEpoch;
      }
    }

    if (res > 0)
    {
      g_err << "- ndb pollEvents returned > 0" << endl;

      NdbEventOperation* next;
      while ((next =
              (useV2?
               ndb->nextEvent2():
               ndb->nextEvent())) != NULL)
      {
        g_err << "-   ndb had an event.  Type : "
              << next->getEventType2()
              << " Epoch : "
              << (next->getEpoch() >> 32)
              << "/"
              << (next->getEpoch() & 0xffffffff)
              << endl;
        if (next->getEventType2() == NdbDictionary::Event::TE_EMPTY)
        {
          g_err << "-  event empty epoch" << endl;
          numEventEmptyEpochs++;
        }
      }
    }
    else if (res == 0)
    {
      g_err << "- ndb pollEvents returned 0" << endl;
    }
    else
    {
      g_err << "- ndb pollEvents failed : " << res << endl;
      return NDBT_FAILED;
    }

    if (highestQueuedEpoch > stopEpoch)
      break;
  }

  g_err << "Num poll empty epochs : "
        << numPollEmptyEpochs
        << ", Num event empty epochs : "
        << numEventEmptyEpochs
        << endl;

  if (useV2)
  {
    if (numEventEmptyEpochs < numPollEmptyEpochs)
    {
      g_err << "FAILED : Too few event empty epochs"
            << endl;
      return NDBT_FAILED;
    }
    else if (numEventEmptyEpochs > numPollEmptyEpochs)
    {
      g_info << "Some empty epochs missed by poll method\n"
            << endl;
    }
  }
  else
  {
    if (numEventEmptyEpochs > 0)
    {
      g_err << "FAILED : Received event empty epochs"
            << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int
runListenEmptyEpochs(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Compare empty epoch behaviour between original
   * and new Apis
   * Original does not expose them as events
   * New Api does
   */
  /* First set up two Ndb objects and two
   * event operations
   */
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();

  NdbEventOperation* evOp1 = createEventOperation(pNdb,
                                                  *pTab);
  if ( evOp1 == NULL )
  {
    g_err << "Event operation creation failed\n";
    return NDBT_FAILED;
  }

  int result= listenEmptyEpochs(pNdb, false);

  if (pNdb->dropEventOperation(evOp1))
  {
    g_err << "Drop event operation failed\n";
    return NDBT_FAILED;
  }

  if (result == NDBT_OK)
  {
    NdbEventOperation* evOp2 = createEventOperation(pNdb,
                                                    *pTab);
    if ( evOp2 == NULL )
    {
      g_err << "Event operation creation2 failed\n";
      return NDBT_FAILED;
    }
    result= listenEmptyEpochs(pNdb, true);

    if (pNdb->dropEventOperation(evOp2))
    {
      g_err << "Drop event operation2 failed\n";
      return NDBT_FAILED;
    }
  }

  return result;
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

  hugoTrans.setAnyValueCallback(setAnyValue);

  if (ctx->getProperty("AllowEmptyUpdates"))
    hugoTrans.setAllowEmptyUpdates(true);

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
  hugoTrans.setAnyValueCallback(setAnyValue);
  
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
  return dropEvent(GETNDB(step), * ctx->getTab());
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
	require(pOp == pCreate);
      
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

          /* Check event transaction id */
          Uint32 anyValue = pOp->getAnyValue();
          Uint64 transId = pOp->getTransId();
          if (anyValue)
          {
            if (!checkAnyValueTransId(transId, anyValue))
            {
              g_err << "ERROR : TransId and AnyValue mismatch.  "
                    << "Transid : " << transId
                    << ", AnyValue : " << anyValue
                    << ", Expected AnyValue : "
                    << (Uint32) ((transId >> 32) ^ transId)
                    << endl;
              abort();
              return NDBT_FAILED;
            }
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

int runEventConsumer(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("runEventConsumer");
  int result = NDBT_OK;
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);

  char buf[1024];
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
    //int count= 0;
    Ndb* ndb= GETNDB(step);

    Uint64 last_gci = 0;
    while(!ctx->isTestStopped())
    {
      Uint32 count = 0;
      Uint64 curr_gci;
      ndb->pollEvents(100, &curr_gci);
      if (curr_gci != last_gci)
      {
        while ((pOp= ndb->nextEvent()) != 0)
        {
          count++;
        }
        last_gci = curr_gci;
      }
      ndbout_c("Consumed gci: %u/%u, %d events",
               Uint32(last_gci >> 32), Uint32(last_gci), count);
    }
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
	require(pOp == pCreate);
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

/**
 * This method checks that the nextEvent() removes inconsistent epoch
 * from the event queue (Bug#18716991 - INCONSISTENT EVENT DATA IS NOT
 * REMOVED FROM EVENT QUEUE CAUSING CONSUMPTION STOP) and continues
 * delivering the following epoch event data.
 *
 * Listener stops the test when it either 1) receives 10 more epochs
 * after it consumed an inconsistent epoch or 2) has polled 120 more
 * poll rounds after the first event data is polled.
 * Test succeeds for case 1 and fails for case 2.
 */
int runEventListenerCheckProgressUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  // Check progress after FI
  int result = NDBT_OK;
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);
  Ndb* ndb= GETNDB(step);
  Uint64 poll_gci = 0;
  // First inconsistent epoch found after pollEvents call
  Uint64 incons_gci_by_poll = 0;
  // First inconsistent epoch found after nextEvent call
  Uint64 incons_gci_by_next = 0;
  Uint64 op_gci = 0, curr_gci = 0, consumed_gci = 0;

  Uint32 consumed_epochs = 0; // Total epochs consumed
  int32 consumed_epochs_after = 0; // epochs consumed after inconsis epoch

  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp, *pCreate = 0;
  pCreate = pOp = ndb->createEventOperation(buf);

  CHK(pOp != NULL, "Event operation creation failed");
  CHK(pOp->execute() == 0, "execute operation execution failed");

  // Syncronise event listening and error injection
  ctx->setProperty("Inject_error", (Uint32)0);
  ctx->setProperty("Found_inconsistency", (Uint32)0);

  // Wait max 10 sec for event data to start flowing
  Uint32 retries = 10;
  while (retries-- > 0)
  {
    if (ndb->pollEvents(1000, &poll_gci) == 1)
        break;
  }
  CHK(retries > 0, "No epoch has received in 10 secs");

  // Event data have started flowing, inject error after one sec
  NdbSleep_SecSleep(1);
  ctx->setProperty("Inject_error", (Uint32)1);

  // if no inconsistency is found after 120 poll rounds, fail
  retries = 120;
  while (!ctx->isTestStopped() && retries-- > 0)
  {
    ndb->pollEvents(1000, &poll_gci);
    if (incons_gci_by_poll == 0 && !ndb->isConsistent(incons_gci_by_poll))
    {
      // found the first inconsistency
      ctx->setProperty("Found_inconsistency", (Uint32)1);
    }

    /**
     * Call next event even if pollEvents returns 0
     * in order to remove the inconsistent event data, if occurred
     */
    while ((pOp = ndb->nextEvent()) != 0)
    {
      assert(pOp == pCreate);
      op_gci = pOp->getGCI();
      if (op_gci > curr_gci)
      {
        // epoch boundary
        consumed_gci = curr_gci;
        curr_gci = op_gci;
        consumed_epochs ++;
        if (incons_gci_by_next > 0 &&
            consumed_gci > incons_gci_by_next &&
            consumed_epochs_after++ == 10)
        {
          ctx->stopTest();
          break;
        }
      }
    }

    // nextEvent returned NULL: either queue is empty or
    // an inconsistent epoch is found
    if (incons_gci_by_poll != 0 && incons_gci_by_next == 0 &&
        !ndb->isConsistent(incons_gci_by_next))
    {
      CHK(incons_gci_by_poll == incons_gci_by_next,
          "pollEvents and nextEvent found different inconsistent epochs");

      // Start counting epochs consumed after the first inconsistent epoch
      consumed_epochs_after = 0;
      g_info << "Epochs consumed at inconsistent epoch : "
             << consumed_epochs << endl;
    }

    // Note epoch boundary when event queue becomes empty
    consumed_gci = curr_gci;
  }

  if (incons_gci_by_poll == 0)
  {
    g_err << "Inconsistent event data has not been seen. "
          << "Either fault injection did not work or test stopped earlier."
          << endl;
    result =  NDBT_FAILED;
  }
  else if (consumed_epochs_after == 0)
  {
    g_err << "Listener : consumption stalled after inconsistent gci : "
          << incons_gci_by_poll
          << ". Last consumed gci : " << consumed_gci
          << ". Last polled gci " << poll_gci << endl;
    result =  NDBT_FAILED;
  }
  else {
    g_info << "Epochs consumed totally: " << consumed_epochs
           << ". Epochs consumed after inconsistent epoch : "
           << consumed_epochs_after
           << ". Poll rounds " << (120 - retries) << endl;

    g_info << "Listener : progressed from inconsis_gci : " << incons_gci_by_poll
          << " to last consumed gci " << consumed_gci
          << ". Last polled gci : " << poll_gci << endl;
  }

  CHK(retries > 0, "Test failed despite 120 poll rounds");
  CHK((ndb->dropEventOperation(pCreate) == 0), "dropEventOperation failed");
  return result;
}

int runRestarter(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;
  bool abort = ctx->getProperty("Graceful", Uint32(0)) == 0;

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
    if (abort == false && ((i % 3) == 0))
    {
      restarter.insertErrorInNode(nodeId, 13043);
    }

    if(restarter.restartOneDbNode(nodeId, false, false, abort) != 0){
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
    if (createEvent(ndb,*pTabs[i], ctx))
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
  pShadowTabs.clear();

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
  if (t == NULL)
    return -1;

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
  int n_poll_retries = 300;

  while (1)
  {
    int res= ndb->pollEvents(1000); // wait for event or 1000 ms
    DBUG_PRINT("info", ("pollEvents res=%d", res));

    n_poll_retries--;
    if (res <= 0 && r == 0)
    {
      if (n_poll_retries > 0)
      {
	NdbSleep_SecSleep(1);
        continue;
      }

      g_err << "Copy_events: pollEvents could not find any epochs "
            << "despite 300 poll retries" << endl;
      DBUG_RETURN(-1);
    }
    
    NdbEventOperation *pOp = ndb->nextEvent();
    // (res==1 && pOp==NULL) means empty epochs
    if (pOp == NULL)
    {
      if (r == 0)
      {
        // Empty epoch preceeding regular epochs. Continue consuming.
        continue;
      }
      // Empty epoch after regular epochs. We are done.
      DBUG_RETURN(r);
    } 

    while (pOp)
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
      
      if (!pOp->isConsistent()) {
	g_err << "A node failure has occured and events might be missing\n";
	DBUG_RETURN(-1);
      }
	
      if (pOp->getEventType() == NdbDictionary::Event::TE_NODE_FAILURE)
      {
        pOp = ndb->nextEvent();
        continue;
      }
      r++;

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
        CHK((r == (n_inserts + n_deletes + n_updates)),
            "Number of record event operations consumed is not equal to the sum of insert,delete and update records.");
	
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
	NdbSleep_MilliSleep(100); // sleep before retrying
      } while(1);
      pOp = ndb->nextEvent();
    } // for
    // No more event data on the event queue.
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

static int createEventOperations(Ndb * ndb, NDBT_Context* ctx)
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

    if (ctx->getProperty("AllowEmptyUpdates"))
      pOp->setAllowEmptyUpdate(true);

    if ( pOp->execute() )
    {
      DBUG_RETURN(NDBT_FAILED);
    }
  }

  DBUG_RETURN(NDBT_OK);
}

#if 0
static int createAllEventOperations(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("createAllEventOperations");
  Ndb * ndb= GETNDB(step);
  int r= createEventOperations(ndb, ctx);
  if (r != NDBT_OK)
  {
    DBUG_RETURN(NDBT_FAILED);
  }
  DBUG_RETURN(NDBT_OK);
}
#endif

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

#if 0
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
#endif

static int runMulti(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("runMulti");

  Ndb * ndb= GETNDB(step);

  int i;

  if (createEventOperations(ndb, ctx))
  {
    DBUG_RETURN(NDBT_FAILED);
  }

  // create a hugo operation per table
  Vector<HugoOperations *> hugo_ops;
  for (i= 0; pTabs[i]; i++)
  {
    hugo_ops.push_back(new HugoOperations(*pTabs[i]));
  }

  int n_records= 3;
  // insert n_records records per table
  do {
    if (start_transaction(ndb, hugo_ops))
    {
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
    for (i= 0; pTabs[i]; i++)
    {
      hugo_ops[i]->pkInsertRecord(ndb, 0, n_records);
    }
    if (execute_commit(ndb, hugo_ops))
    {
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
    if(close_transaction(ndb, hugo_ops))
    {
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
  } while(0);

  // copy events and verify
  do {
    int ops_consumed = 0;
    if ((ops_consumed=copy_events(ndb)) != i * n_records)
    {
      g_err << "Not all records are consumed. Consumed " << ops_consumed
            << ", inserted " << i * n_records << endl;
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
    if (verify_copy(ndb, pTabs, pShadowTabs))
    {
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
  } while (0);

  // update n_records-1 records in first table
  do {
    if (start_transaction(ndb, hugo_ops))
    {
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }

    hugo_ops[0]->pkUpdateRecord(ndb, n_records-1);

    if (execute_commit(ndb, hugo_ops))
    {
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
    if(close_transaction(ndb, hugo_ops))
    {
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
  } while(0);

  // copy events and verify
  do {
    if (copy_events(ndb) <= 0)
    {
      g_err << "No update is consumed. " << endl;
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
    if (verify_copy(ndb, pTabs, pShadowTabs))
    {
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
  } while (0);

  if (dropEventOperations(ndb))
  {
    DBUG_RETURN(NDBT_FAILED);
  }

  DBUG_RETURN(NDBT_OK);
}

static int runMulti_NR(NDBT_Context* ctx, NDBT_Step* step)
{
  DBUG_ENTER("runMulti");

  int records = ctx->getNumRecords();
  int loops = ctx->getNumLoops();
  Ndb * ndb= GETNDB(step);

  int i;

  if (createEventOperations(ndb, ctx))
  {
    DBUG_RETURN(NDBT_FAILED);
  }

  for (i= 0; pTabs[i]; i++)
  {
    HugoTransactions hugo(*pTabs[i]);
    if (hugo.loadTable(ndb, records, 1, true, 1))
    {
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
    // copy events and verify
    int ops_consumed = 0;
    if ((ops_consumed=copy_events(ndb)) != records)
    {
      g_err << "Not all records are consumed. Consumed " << ops_consumed
            << ", inserted " << records << endl;
      dropEventOperations(ndb);
      DBUG_RETURN(NDBT_FAILED);
    }
  }

  if (verify_copy(ndb, pTabs, pShadowTabs))
  {
    dropEventOperations(ndb);
    DBUG_RETURN(NDBT_FAILED);
  }

  {
    NdbRestarts restarts;
    for (int j= 0; j < loops; j++)
    {
      // restart a node
      int timeout = 240;
      if (restarts.executeRestart(ctx, "RestartRandomNodeAbort", timeout))
      {
	dropEventOperations(ndb);
	DBUG_RETURN(NDBT_FAILED);
      }

      sleep(5);
      // update all tables
      for (i= 0; pTabs[i]; i++)
      {
	HugoTransactions hugo(*pTabs[i]);
	if (hugo.pkUpdateRecords(ndb, records, 1, 1))
	{
	  dropEventOperations(ndb);
	  DBUG_RETURN(NDBT_FAILED);
	}
        int ops_consumed = 0;
	if ((ops_consumed=copy_events(ndb)) != records)
	{
          g_err << "Not all updates are consumed. Consumed " << ops_consumed
                << ", updated " << records << endl;
	  dropEventOperations(ndb);
	  DBUG_RETURN(NDBT_FAILED);
	}
      }

      // copy events and verify
      if (verify_copy(ndb, pTabs, pShadowTabs))
      {
	dropEventOperations(ndb);
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

typedef Bitmask<(MAX_NDB_NODES + 31) / 32> NdbNodeBitmask;

static
int 
restartNodes(NdbNodeBitmask mask)
{
  int cnt = 0;
  int nodes[MAX_NDB_NODES];
  NdbRestarter res;
  for (Uint32 i = 0; i<MAX_NDB_NODES; i++)
  {
    if (mask.get(i))
    {
      nodes[cnt++] = i;
      res.restartOneDbNode(i,
                           /** initial */ false,
                           /** nostart */ true,
                           /** abort   */ true);
    }
  }

  if (res.waitNodesNoStart(nodes, cnt) != 0)
    return NDBT_FAILED;

  res.startNodes(nodes, cnt);

  return res.waitClusterStarted();
}

static int restartAllNodes()
{
  NdbRestarter restarter;
  NdbNodeBitmask ng;
  NdbNodeBitmask nodes0;
  NdbNodeBitmask nodes1;

  /**
   * Restart all nodes using two restarts
   *   instead of one by one...as this takes to long
   */
  for (Uint32 i = 0; i<(Uint32)restarter.getNumDbNodes(); i++)
  {
    int nodeId = restarter.getDbNodeId(i);
    if (ng.get(restarter.getNodeGroup(nodeId)) == false)
    {
      nodes0.set(nodeId);
      ng.set(restarter.getNodeGroup(nodeId));
    }
    else
    {
      nodes1.set(nodeId);
    }
  }

  int res;
  if ((res = restartNodes(nodes0)) != NDBT_OK)
  {
    return res;
  }
  

  res = restartNodes(nodes1);
  return res;
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
    DBUG_RETURN(NDBT_OK);
  }
  NdbDictionary::Table copy(* ctx->getTab());
  do
  {
    const NdbDictionary::Table* pTab = 
      ndb->getDictionary()->getTable(copy.getName());
    result = NDBT_FAILED;
    if (createEvent(ndb, *pTab, ctx))
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
    CHK_NDB_READY(ndb);
    if (ndb->getDictionary()->dropTable(pTab->getName()) != 0){
      g_err << "Failed to drop " << pTab->getName() <<" in db" << endl;
      break;
    }
    ndbout << "Restarting with dropped events and dropped "
           << "table with subscribers" << endl;
    if (restartAllNodes())
      break;
    CHK_NDB_READY(ndb);
    if (ndb->dropEventOperation(pOp))
    {
      g_err << "Failed dropEventOperation" << endl;
      break;
    }
    //tmp.setNodeGroupIds(0, 0);
    if (ndb->getDictionary()->createTable(copy) != 0){
      g_err << "createTable failed: "
            << ndb->getDictionary()->getNdbError() << endl;
      break;
    }
    result = NDBT_OK;
  } while (--loops > 0);
  
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
  int untilStopped = ctx->getProperty("SubscribeUntilStopped", (Uint32)0);

  while ((untilStopped || --loops) && !ctx->isTestStopped())
  {
    NdbEventOperation *pOp= ndb->createEventOperation(buf);
    if (pOp == 0)
    {
      g_err << "createEventOperation: "
	    << ndb->getNdbError().code << " "
	    << ndb->getNdbError().message << endl;
      ctx->stopTest();
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
      
      ctx->stopTest();
      return NDBT_FAILED;
    }
    
    // consume events to make sure dropped events are deleted
    if (ndb->pollEvents(0))
    {
      while (ndb->nextEvent())
        ;
    }

    if (ndb->dropEventOperation(pOp))
    {
      g_err << "pOp->execute(): "
	    << ndb->getNdbError().code << " "
	    << ndb->getNdbError().message << endl;
      ctx->stopTest();
      return NDBT_FAILED;
    }
  }
  
  ctx->stopTest();
  return NDBT_OK;
}

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
runScanUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  //int records = ctx->getNumRecords();
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
  //int result = NDBT_OK;
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
  //int result = NDBT_OK;

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

  CHK_NDB_READY(GETNDB(step));
  
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  
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

int
errorInjectBufferOverflow(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb * ndb= GETNDB(step);
  NdbRestarter restarter;
  const NdbDictionary::Table* pTab = ctx->getTab();
  int result= NDBT_OK;
  int res;
  bool found_gap = false;
  NdbEventOperation *pOp= createEventOperation(ndb, *pTab);
  Uint64 gci;

  if (pOp == 0)
  {
    g_err << "Failed to createEventOperation" << endl;
    return NDBT_FAILED;
  }

  if (restarter.insertErrorInAllNodes(13036) != 0)
  {
    result = NDBT_FAILED;
    goto cleanup;
  }

  res = ndb->pollEvents(5000);

  if (ndb->getNdbError().code != 0)
  {
    g_err << "pollEvents failed: \n";
    g_err << ndb->getNdbError().code << " "
          << ndb->getNdbError().message << endl;
    result = (ndb->getNdbError().code == 4720)?NDBT_OK:NDBT_FAILED;
    goto cleanup;
  }
  if (res >= 0) {
    NdbEventOperation *tmp;
    while (!found_gap && (tmp= ndb->nextEvent()))
    {
      if (!ndb->isConsistent(gci))
        found_gap = true;
    }
  }
  if (!ndb->isConsistent(gci))
    found_gap = true;
  if (!found_gap)
  {
    g_err << "buffer overflow not detected\n";
    result = NDBT_FAILED;
    goto cleanup;
  }

cleanup:

  if (ndb->dropEventOperation(pOp) != 0) {
    g_err << "dropping event operation failed\n";
    result = NDBT_FAILED;
  }

  return result;
}

int
errorInjectBufferOverflowOnly(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  assert(ctx->getProperty("Inject_error", (Uint32)0) == 0);
  // Wait for the signal from the listener
  while (ctx->getProperty("Inject_error", (Uint32)0) != 1)
  {
    NdbSleep_SecSleep(1);
  }
  ctx->setProperty("Inject_error", (Uint32)0);

  if (restarter.insertErrorInAllNodes(13036) != 0)
  {
    return NDBT_FAILED;
  }
  while (ctx->getProperty("Found_inconsistency", (Uint32)0) != 1)
  {
    NdbSleep_SecSleep(1);
  }
  ctx->setProperty("Inject_error", (Uint32)0);

  restarter.insertErrorInAllNodes(0); //Remove the injected error
  return NDBT_OK;
}

int
errorInjectStalling(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb * ndb= GETNDB(step);
  NdbRestarter restarter;
  const NdbDictionary::Table* pTab = ctx->getTab();
  NdbEventOperation *pOp= createEventOperation(ndb, *pTab);
  int result = NDBT_OK;
  int res;
  bool connected = true;
  uint retries = 100;

  if (pOp == 0)
  {
    g_err << "Failed to createEventOperation" << endl;
    return NDBT_FAILED;
  }

  if (restarter.insertErrorInAllNodes(13037) != 0)
  {
    result = NDBT_FAILED;
    goto cleanup;
  }

  Uint64 curr_gci;
  for (int i=0; (i<10) && (curr_gci != NDB_FAILURE_GCI); i++)
  {
    res = ndb->pollEvents(5000, &curr_gci) > 0;

    if (ndb->getNdbError().code != 0)
    {
      g_err << "pollEvents failed: \n";
      g_err << ndb->getNdbError().code << " "
            << ndb->getNdbError().message << endl;
      result = NDBT_FAILED;
      goto cleanup;
    }
  }

  if (curr_gci != NDB_FAILURE_GCI)
  {
    g_err << "pollEvents failed to detect cluster failure: \n";
    result = NDBT_FAILED;
    goto cleanup;
  } 

  if (res > 0) {
    NdbEventOperation *tmp;
    int count = 0;
    while (connected && (tmp= ndb->nextEvent()))
    {
      if (tmp != pOp)
      {
        printf("Found stray NdbEventOperation\n");
        result = NDBT_FAILED;
        goto cleanup;
      }
      switch (tmp->getEventType()) {
      case NdbDictionary::Event::TE_CLUSTER_FAILURE:
        g_err << "Found TE_CLUSTER_FAILURE" << endl;
        connected = false;
        break;
      default:
        count++;
        break;
      }
    }
    if (connected)
    {
      g_err << "failed to detect cluster disconnect\n";
      result = NDBT_FAILED;
      goto cleanup;
    }
  }

  if (ndb->dropEventOperation(pOp) != 0) {
    g_err << "dropping event operation failed\n";
    result = NDBT_FAILED;
  }

  // Reconnect by trying to start a transaction
  while (!connected && retries--)
  {
    HugoTransactions hugoTrans(* ctx->getTab());
    if (hugoTrans.loadTable(ndb, 100) == 0)
    {
      connected = true;
      result = NDBT_OK;
    }
    else
    {
      NdbSleep_MilliSleep(300);
      result = NDBT_FAILED;
    }
  }

  if (!connected)
    g_err << "Failed to reconnect\n";

  // Restart cluster with abort
  if (restarter.restartAll(false, false, true) != 0){
    ctx->stopTest();
    return NDBT_FAILED;
  }

  if (restarter.waitClusterStarted(300) != 0){
    return NDBT_FAILED;
  }

  CHK_NDB_READY(ndb);

  pOp= createEventOperation(ndb, *pTab);

  if (pOp == 0)
  {
    g_err << "Failed to createEventOperation" << endl;
    return NDBT_FAILED;
  }

  // Check that we receive events again
  for (int i=0; (i<10) && (curr_gci == NDB_FAILURE_GCI); i++)
  {
    res = ndb->pollEvents(5000, &curr_gci) > 0;

    if (ndb->getNdbError().code != 0)
    {
      g_err << "pollEvents failed: \n";
      g_err << ndb->getNdbError().code << " "
            << ndb->getNdbError().message << endl;
      result = NDBT_FAILED;
      goto cleanup;
    }
  }
  if (curr_gci == NDB_FAILURE_GCI)
  {
    g_err << "pollEvents after restart failed res " << res << " curr_gci " << curr_gci << endl;
    result =  NDBT_FAILED;
  }

cleanup:

  if (ndb->dropEventOperation(pOp) != 0) {
    g_err << "dropping event operation failed\n";
    result = NDBT_FAILED;
  }

  // Stop the other thread
  ctx->stopTest();

  return result;
}
int 
runBug33793(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;
  int loops = ctx->getNumLoops();

  NdbRestarter restarter;
  
  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }
  // This should really wait for applier to start...10s is likely enough
  NdbSleep_SecSleep(10);

  while (loops-- && ctx->isTestStopped() == false)
  {
    int nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
    int nodecount = 0;
    int nodes[255];
    printf("nodeid: %u : victims: ", nodeId);
    for (int i = 0; i<restarter.getNumDbNodes(); i++)
    {
      int id = restarter.getDbNodeId(i);
      if (id == nodeId)
        continue;
      
      if (restarter.getNodeGroup(id) == restarter.getNodeGroup(nodeId))
      {
        nodes[nodecount++] = id;
        printf("%u ", id);
        int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
        if (restarter.dumpStateOneNode(id, val2, 2))
          return NDBT_FAILED;
      }
    }
    printf("\n"); fflush(stdout);

    restarter.insertErrorInNode(nodeId, 13034);
    if (restarter.waitNodesNoStart(nodes, nodecount))
      return NDBT_FAILED;
    
    if (restarter.startNodes(nodes, nodecount))
      return NDBT_FAILED;
    
    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }

  ctx->stopTest();  
  return NDBT_OK;
}

static
int
cc(Ndb_cluster_connection** ctx, Ndb** ndb)
{
  Ndb_cluster_connection* xncc = new Ndb_cluster_connection;
  int ret;
  if ((ret = xncc->connect(30, 1, 0)) != 0)
  {
    delete xncc;
    return NDBT_FAILED;
  }

  if ((ret = xncc->wait_until_ready(30, 10)) != 0)
  {
    delete xncc;
    return NDBT_FAILED;
  }

  Ndb* xndb = new Ndb(xncc, "TEST_DB");
  if (xndb->init() != 0)
  {
    delete xndb;
    delete xncc;
    return NDBT_FAILED;
  }

  if (xndb->waitUntilReady(30) != 0)
  {
    delete xndb;
    delete xncc;
    return NDBT_FAILED;
  }

  * ctx = xncc;
  * ndb = xndb;
  return 0;
}

static
NdbEventOperation*
op(Ndb* xndb, const NdbDictionary::Table * table)
{
  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp;
  pOp = xndb->createEventOperation(buf);
  if ( pOp == NULL )
  {
    g_err << "Event operation creation failed on %s" << buf << endl;
    return 0;
  }

  int n_columns= table->getNoOfColumns();
  NdbRecAttr* recAttr[1024];
  NdbRecAttr* recAttrPre[1024];
  for (int i = 0; i < n_columns; i++) {
    recAttr[i]    = pOp->getValue(table->getColumn(i)->getName());
    recAttrPre[i] = pOp->getPreValue(table->getColumn(i)->getName());
  }

  return pOp;
}

int
runBug34853(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  //int records = ctx->getNumRecords();
  //Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  Ndb_cluster_connection* xncc;
  Ndb* xndb;

  if (cc(&xncc, &xndb))
  {
    return NDBT_FAILED;
  }

  NdbEventOperation* pOp = op(xndb, ctx->getTab());
  if (pOp == 0)
  {
    delete xndb;
    delete xncc;
    return NDBT_FAILED;
  }

  int api = xncc->node_id();
  int nodeId = res.getDbNodeId(rand() % res.getNumDbNodes());
  ndbout_c("stopping %u", nodeId);
  res.restartOneDbNode(nodeId,
                       /** initial */ false,
                       /** nostart */ true,
                       /** abort   */ true);

  ndbout_c("waiting for %u", nodeId);
  res.waitNodesNoStart(&nodeId, 1);

  int dump[2];
  dump[0] = 9004;
  dump[1] = api;
  res.dumpStateOneNode(nodeId, dump, 2);
  res.startNodes(&nodeId, 1);
  ndbout_c("waiting cluster");
  res.waitClusterStarted();

  CHK_NDB_READY(xndb);

  if (pOp->execute())
  { // This starts changes to "start flowing"
    g_err << "execute operation execution failed: \n";
    g_err << pOp->getNdbError().code << " "
	  << pOp->getNdbError().message << endl;
    delete xndb;
    delete xncc;
    return NDBT_FAILED;
  }

  xndb->dropEventOperation(pOp);

  ndbout_c("stopping %u", nodeId);
  res.restartOneDbNode(nodeId,
                       /** initial */ false,
                       /** nostart */ true,
                       /** abort   */ true);

  ndbout_c("waiting for %u", nodeId);
  res.waitNodesNoStart(&nodeId, 1);

  dump[0] = 71;
  dump[1] = 7;
  res.dumpStateOneNode(nodeId, dump, 2);
  res.startNodes(&nodeId, 1);
  ndbout_c("waiting node sp 7");
  res.waitNodesStartPhase(&nodeId, 1, 6);

  delete xndb;
  delete xncc;

  NdbSleep_SecSleep(5); // 3 seconds to open connections. i.e 5 > 3

  dump[0] = 71;
  res.dumpStateOneNode(nodeId, dump, 1);

  res.waitClusterStarted();

  if (cc(&xncc, &xndb))
  {
    return NDBT_FAILED;
  }

  pOp = op(xndb, ctx->getTab());
  if (pOp == 0)
  {
    delete xndb;
    delete xncc;
    return NDBT_FAILED;
  }

  if (pOp->execute())
  { // This starts changes to "start flowing"
    g_err << "execute operation execution failed: \n";
    g_err << pOp->getNdbError().code << " "
	  << pOp->getNdbError().message << endl;
    delete xndb;
    delete xncc;
    return NDBT_FAILED;
  }

  xndb->dropEventOperation(pOp);
  delete xndb;
  delete xncc;
  return NDBT_OK;
}

/** Telco 6.2 **/

int
runNFSubscribe(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  int codes[] = {
    6023,  (int)NdbRestarter::NS_NON_MASTER,
    13013, (int)NdbRestarter::NS_RANDOM,
    13019, (int)NdbRestarter::NS_RANDOM,
    13020, (int)NdbRestarter::NS_RANDOM,
    13041, (int)NdbRestarter::NS_RANDOM,
    0
  };
  
  int nr_codes[] = {
    13039,
    13040,
    13042,
    0
  };

  int loops = ctx->getNumLoops();
  while (loops-- && !ctx->isTestStopped())
  {
    int i = 0;
    while (codes[i] != 0)
    {
      int code = codes[i++];
      int nodeId = restarter.getNode((NdbRestarter::NodeSelector)codes[i++]);
      int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
      if (restarter.dumpStateOneNode(nodeId, val2, 2))
        return NDBT_FAILED;
      
      ndbout_c("Node %u error: %u", nodeId, code);
      if (restarter.insertErrorInNode(nodeId, code))
        return NDBT_FAILED;
      
      if (restarter.waitNodesNoStart(&nodeId, 1))
        return NDBT_FAILED;
      
      if (restarter.startNodes(&nodeId, 1))
        return NDBT_FAILED;
      
      if (restarter.waitClusterStarted())
        return NDBT_FAILED;
    }
    
    int nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
    if (restarter.restartOneDbNode(nodeId, false, true, true) != 0)
      return NDBT_FAILED;
    
    if (restarter.waitNodesNoStart(&nodeId, 1))
      return NDBT_FAILED;
    
    i = 0;
    while (nr_codes[i] != 0)
    {
      int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
      if (restarter.dumpStateOneNode(nodeId, val2, 2))
        return NDBT_FAILED;
      
      ndbout_c("Node %u error: %u", nodeId, nr_codes[i]);
      if (restarter.insertErrorInNode(nodeId, nr_codes[i]))
        return NDBT_FAILED;
      
      if (restarter.startNodes(&nodeId, 1))
        return NDBT_FAILED;

      NdbSleep_SecSleep(3);
      
      if (restarter.waitNodesNoStart(&nodeId, 1))
        return NDBT_FAILED;
      
      i++;
    }
    
    ndbout_c("Done..now starting %u", nodeId);
    if (restarter.startNodes(&nodeId, 1))
      return NDBT_FAILED;
    
    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }  

  ctx->stopTest();
  return NDBT_OK;
}

int
runBug35208_createTable(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbDictionary::Table tab = *ctx->getTab();

  while (tab.getNoOfColumns() < 100)
  {
    BaseString name;
    NdbDictionary::Column col;
    name.assfmt("COL_%d", tab.getNoOfColumns());
    col.setName(name.c_str());
    col.setType(NdbDictionary::Column::Unsigned);
    col.setLength(1);
    col.setNullable(false);
    col.setPrimaryKey(false);
    tab.addColumn(col);
  }

  NdbDictionary::Dictionary* dict = GETNDB(step)->getDictionary();
  dict->dropTable(tab.getName());
  dict->createTable(tab);

  const NdbDictionary::Table* pTab = dict->getTable(tab.getName());
  ctx->setTab(pTab);

  return NDBT_OK;
}

#define UPDATE_COL 66

int
runBug35208(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* ndb= GETNDB(step);
  const NdbDictionary::Table * table= ctx->getTab();

  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp = ndb->createEventOperation(buf);
  if ( pOp == NULL ) {
    g_err << "Event operation creation failed on %s" << buf << endl;
    return NDBT_FAILED;
  }

  int result = NDBT_OK;
  HugoTransactions hugoTrans(* table);

  char col[100];
  BaseString::snprintf(col, sizeof(col), "COL_%u", UPDATE_COL);

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
    goto err;
  }

  hugoTrans.loadTable(GETNDB(step), ctx->getNumRecords());

  for (int i = 0; i<ctx->getNumLoops(); i++)
  {
    ndbout_c("testing %u updates", (i + 1));
    NdbTransaction* pTrans = ndb->startTransaction();
    for (int m = 0; m<(i+1); m++)
    {
      for (int r = 0; r<ctx->getNumRecords(); r++)
      {
        NdbOperation* pOp = pTrans->getNdbOperation(table->getName());
        pOp->updateTuple();
        HugoOperations hop(* table);
        hop.equalForRow(pOp, r);
        pOp->setValue(col, rand());
      }
      if (pTrans->execute(NoCommit) != 0)
      {
        ndbout << pTrans->getNdbError() << endl;
        goto err;
      }
    }
    if (pTrans->execute(Commit) != 0)
    {
      ndbout << pTrans->getNdbError() << endl;
      goto err;
    }

    Uint64 gci;
    pTrans->getGCI(&gci);
    ndbout_c("set(LastGCI_hi): %u/%u",
             Uint32(gci >> 32),
             Uint32(gci));
    ctx->setProperty("LastGCI_lo", Uint32(gci));
    ctx->setProperty("LastGCI_hi", Uint32(gci >> 32));
    if(ctx->getPropertyWait("LastGCI_hi", ~(Uint32)0))
    {
      g_err << "FAIL " << __LINE__ << endl;
      goto err;
    }

    Uint32 bug = 0;
    Uint32 cnt = 0;
    Uint64 curr_gci = 0;
    while(curr_gci <= gci)
    {
      ndb->pollEvents(100, &curr_gci);
      NdbEventOperation* tmp = 0;
      while ((tmp= ndb->nextEvent()) != 0)
      {
        if (tmp->getEventType() == NdbDictionary::Event::TE_UPDATE)
        {
          cnt++;
          bool first = true;
          for (int c = 0; c<table->getNoOfColumns(); c++)
          {
            if (recAttr[c]->isNULL() >= 0)
            {
              /**
               * Column has value...it should be PK or column we updated
               */
              if (c != UPDATE_COL &&
                  table->getColumn(c)->getPrimaryKey() == false)
              {
                bug++;
                if (first)
                {
                  first = false;
                  printf("Detect (incorrect) update value for: ");
                }
                printf("%u ", c);
                result = NDBT_FAILED;
              }
            }
          }
          if (!first)
            printf("\n");
        }
      }
    }
    ndbout_c("found %u updates bugs: %u", cnt, bug);
  }

  ndb->dropEventOperation(pOp);
  ctx->stopTest();

  return result;

err:
  ndb->dropEventOperation(pOp);

  return NDBT_FAILED;
}



/** Telco 6.3 **/

int
runBug37279(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  if (res.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  if (runCreateEvent(ctx, step))
  {
    return NDBT_FAILED;
  }
  
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  
  const NdbDictionary::Table* tab = dict->getTable(ctx->getTab()->getName());
  //const NdbDictionary::Table* org = tab;
  NdbEventOperation* pOp0 = createEventOperation(pNdb, *tab);
  
  if (pOp0 == 0)
  {
    return NDBT_FAILED;
  }
  
  {
    Ndb* ndb = new Ndb(&ctx->m_cluster_connection, "TEST_DB");
    if (ndb->init() != 0)
    {
      delete ndb;
      ndbout_c("here: %u", __LINE__);
      return NDBT_FAILED;
    }
    
    if (ndb->waitUntilReady(30) != 0)
    {
      delete ndb;
      ndbout_c("here: %u", __LINE__);
      return NDBT_FAILED;
    }
    
    ndb->getDictionary()->dropTable(tab->getName());
    delete ndb;
  }
  
  int nodeId = res.getDbNodeId(rand() % res.getNumDbNodes());
  ndbout_c("stopping %u", nodeId);
  res.restartOneDbNode(nodeId,
                       /** initial */ false,
                       /** nostart */ false,
                       /** abort   */ true);
  if (res.waitClusterStarted())
  {
    return NDBT_FAILED;
  }

  CHK_NDB_READY(pNdb);
  
  pNdb->dropEventOperation(pOp0);
  runDropEvent(ctx, step);

  return NDBT_OK;
}

int
runBug37338(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  if (res.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  int nodeId = res.getDbNodeId(rand() % res.getNumDbNodes());

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  const NdbDictionary::Table* tab = dict->getTable(ctx->getTab()->getName());

  const char * name = "BugXXX";
  NdbDictionary::Table copy = * tab;
  copy.setName(name);
  dict->dropTable(name);

  for (int i = 0; i<ctx->getNumLoops(); i++)
  {
    Ndb* ndb0;
    Ndb_cluster_connection *con0;
    NdbEventOperation* pOp0;
    NdbDictionary::Dictionary * dict0;

    cc(&con0, &ndb0);
    dict0 = ndb0->getDictionary();
    if (dict0->createTable(copy) != 0)
    {
      ndbout << dict0->getNdbError() << endl;
      return NDBT_FAILED;
    }

    const NdbDictionary::Table * copyptr = dict0->getTable(name);
    if (copyptr == 0)
    {
      return NDBT_FAILED;
    }
    createEvent(ndb0, *copyptr, ctx);
    pOp0 = createEventOperation(ndb0, *copyptr);
    dict0 = ndb0->getDictionary();dict->dropTable(name);
    
    res.restartOneDbNode(nodeId,
                         /** initial */ false,
                         /** nostart */ true,
                         /** abort   */ true);
    
    res.waitNodesNoStart(&nodeId, 1);
    res.startNodes(&nodeId, 1);
    if (res.waitClusterStarted())
    {
      return NDBT_FAILED;
    }
    
    CHK_NDB_READY(ndb0);
  
    ndb0->dropEventOperation(pOp0);
    
    delete ndb0;
    delete con0;
  }
  
  return NDBT_OK;
}

int
runBug37442(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  if (res.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  int nodeId = res.getDbNodeId(rand() % res.getNumDbNodes());

  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();
  const NdbDictionary::Table* tab = dict->getTable(ctx->getTab()->getName());

  if (runCreateEvent(ctx, step))
  {
    return NDBT_FAILED;
  }
  
  for (int i = 0; i<ctx->getNumLoops(); i++)
  {
    NdbEventOperation * pOp = createEventOperation(GETNDB(step), *tab);
    
    res.restartOneDbNode(nodeId,
                         /** initial */ false,
                         /** nostart */ true,
                         /** abort   */ true);
    
    res.waitNodesNoStart(&nodeId, 1);

    GETNDB(step)->dropEventOperation(pOp);
    
    res.startNodes(&nodeId, 1);
    if (res.waitClusterStarted())
    {
      return NDBT_FAILED;
    }
    CHK_NDB_READY(GETNDB(step));
  }

  runDropEvent(ctx, step);
  
  return NDBT_OK;
}

const NdbDictionary::Table* createBoringTable(const char* name, Ndb* pNdb)
{
  NdbDictionary::Table tab;

  tab.setName(name);

  NdbDictionary::Column pk;
  pk.setName("Key");
  pk.setType(NdbDictionary::Column::Unsigned);
  pk.setLength(1); 
  pk.setNullable(false);
  pk.setPrimaryKey(true);
  tab.addColumn(pk);

  NdbDictionary::Column attr;
  attr.setName("Attr");
  attr.setType(NdbDictionary::Column::Unsigned);
  attr.setLength(1);
  attr.setNullable(true);
  attr.setPrimaryKey(false);
  tab.addColumn(attr);
  
  pNdb->getDictionary()->dropTable(tab.getName());
  if(pNdb->getDictionary()->createTable(tab) == 0)
  {
    ndbout << (NDBT_Table&)tab << endl;
    return pNdb->getDictionary()->getTable(tab.getName());
  }
  
  ndbout << "Table create failed, err : " << 
    pNdb->getDictionary()->getNdbError().code << endl;
  
  return NULL;
}

/* Types of operation which can be tagged via 'setAnyValue */
enum OpTypes {Insert, Update, Write, Delete, EndOfOpTypes};

/** 
 * executeOps
 * Generate a number of PK operations of the supplied type
 * using the passed operation options and setting the
 * anyValue tag
 */
int
executeOps(Ndb* pNdb,
           const NdbDictionary::Table* tab,
           OpTypes op, 
           Uint32 rowCount,
           Uint32 keyOffset,
           Uint32 anyValueOffset,
           NdbOperation::OperationOptions opts)
{
  NdbTransaction* trans= pNdb->startTransaction();
  const NdbRecord* record= tab->getDefaultRecord();

  char RowBuf[16];
  Uint32* keyPtr= (Uint32*) NdbDictionary::getValuePtr(record,
                                                       RowBuf,
                                                       0);
  Uint32* attrPtr= (Uint32*) NdbDictionary::getValuePtr(record,
                                                       RowBuf,
                                                       1);

  for (Uint32 i=keyOffset; i < (keyOffset + rowCount); i++)
  {
    memcpy(keyPtr, &i, sizeof(i));
    memcpy(attrPtr, &i, sizeof(i));
    opts.optionsPresent |= NdbOperation::OperationOptions::OO_ANYVALUE;
    opts.anyValue= anyValueOffset + i;
    bool allowInterpreted= 
      (op == Update) ||
      (op == Delete);

    if (!allowInterpreted)
      opts.optionsPresent &= 
        ~ (Uint64) NdbOperation::OperationOptions::OO_INTERPRETED;

    switch (op) {
    case Insert : 
      if (trans->insertTuple(record, 
                             RowBuf, 
                             NULL,
                             &opts, 
                             sizeof(opts)) == NULL)
      {
        g_err << "Can't create operation : " <<
          trans->getNdbError().code << endl;
        return NDBT_FAILED;
      }
      break;
    case Update :
      if (trans->updateTuple(record,
                             RowBuf,
                             record,
                             RowBuf,
                             NULL,
                             &opts,
                             sizeof(opts)) == NULL)
      {
        g_err << "Can't create operation : " <<
          trans->getNdbError().code << endl;
        return NDBT_FAILED;
      }
      break;
    case Write : 
      if (trans->writeTuple(record,
                            RowBuf,
                            record,
                            RowBuf,
                            NULL,
                            &opts,
                            sizeof(opts)) == NULL)
      {
        g_err << "Can't create operation : " <<
          trans->getNdbError().code << endl;
        return NDBT_FAILED;
      }
      break;
    case Delete : 
      if (trans->deleteTuple(record,
                             RowBuf,
                             record,
                             NULL,
                             NULL,
                             &opts,
                             sizeof(opts)) == NULL)
      {
        g_err << "Can't create operation : " <<
          trans->getNdbError().code << endl;
        return NDBT_FAILED;
      }
      break;
    default:
      g_err << "Bad operation type : " << op << endl;
      return NDBT_FAILED;
    }
  }

  trans->execute(Commit);

  if (trans->getNdbError().code != 0)
  {
    g_err << "Error executing operations :" << 
      trans->getNdbError().code << endl;
    return NDBT_FAILED;
  }
  
  trans->close();

  return NDBT_OK;
}

int
checkAnyValueInEvent(Ndb* pNdb,
                     NdbRecAttr* preKey,
                     NdbRecAttr* postKey,
                     NdbRecAttr* preAttr,
                     NdbRecAttr* postAttr,
                     Uint32 num,
                     Uint32 anyValueOffset,
                     bool checkPre)
{
  Uint32 received= 0;

  while (received < num)
  {
    int pollRc;

    if ((pollRc= pNdb->pollEvents(10000)) < 0)
    {
      g_err << "Error while polling for events : " <<
        pNdb->getNdbError().code;
      return NDBT_FAILED;
    }

    if (pollRc == 0)
    {
      printf("No event, waiting...\n");
      continue;
    }

    NdbEventOperation* event;
    while((event= pNdb->nextEvent()) != NULL)
    {
//       printf("Event is %p of type %u\n",
//              event, event->getEventType());
//       printf("Got event, prekey is %u predata is %u \n",
//              preKey->u_32_value(),
//              preAttr->u_32_value());
//       printf("           postkey is %u postdata is %u anyvalue is %u\n",
//              postKey->u_32_value(),
//              postAttr->u_32_value(),
//              event->getAnyValue());
      
      received ++;
      Uint32 keyVal= (checkPre? 
                      preKey->u_32_value() :
                      postKey->u_32_value());
      
      if (event->getAnyValue() != 
          (anyValueOffset + keyVal))
      {
        g_err << "Error : Got event, key is " <<
          keyVal << " anyValue is " <<
          event->getAnyValue() <<
          " expected " << (anyValueOffset + keyVal) 
              << endl;
        return NDBT_FAILED;
      }
    }
  }

  return NDBT_OK;
}
                      
                      

int
runBug37672(NDBT_Context* ctx, NDBT_Step* step)
{
  /* InterpretedDelete and setAnyValue failed */
  /* Let's create a boring, known table for this since 
   * we don't yet have Hugo tools for NdbRecord
   */
  BaseString name; 
  name.assfmt("TAB_TESTEVENT%d", rand() & 65535);
  Ndb* pNdb= GETNDB(step);
  
  const NdbDictionary::Table* tab= createBoringTable(name.c_str(), pNdb);
  
  if (tab == NULL)
    return NDBT_FAILED;
  
  /* Create an event to listen to events on the table */
  char eventName[1024];
  sprintf(eventName,"%s_EVENT", tab->getName());

  if (createEvent(pNdb, *tab, false, true) != 0)
    return NDBT_FAILED;

  /* Now create the event operation to retrieve the events */
  NdbEventOperation* eventOp;
  eventOp= pNdb->createEventOperation(eventName);

  if (eventOp == NULL)
  {
    g_err << "Failed to create event operation :" << 
      pNdb->getNdbError().code << endl;
    return NDBT_FAILED;
  }

  NdbRecAttr* eventKeyData= eventOp->getValue("Key");
  NdbRecAttr* eventOldKeyData= eventOp->getPreValue("Key");
  NdbRecAttr* eventAttrData= eventOp->getValue("Attr");
  NdbRecAttr* eventOldAttrData= eventOp->getPreValue("Attr");
  
  if ((eventKeyData == NULL) || (eventAttrData == NULL))
  {
    g_err << "Failed to get NdbRecAttrs for events" << endl;
    return NDBT_FAILED;
  };
  
  if (eventOp->execute() != 0)
  {
    g_err << "Failed to execute event operation :" <<
      eventOp->getNdbError().code << endl;
    return NDBT_FAILED;
  }

  /* Perform some operations on the table, and check
   * that we get the correct AnyValues propagated
   * through
   */
  NdbOperation::OperationOptions opts;
  opts.optionsPresent= 0;

  NdbInterpretedCode nonsenseProgram;

  nonsenseProgram.load_const_u32(0, 0);
  nonsenseProgram.interpret_exit_ok();

  nonsenseProgram.finalise();

  const Uint32 rowCount= 1500;
  Uint32 keyOffset= 0;
  Uint32 anyValueOffset= 100;

  printf ("Testing AnyValue with no interpreted program\n");
  for (int variants= 0; variants < 2; variants ++)
  {
    for (int op= Insert; op < EndOfOpTypes; op++)
    {
      printf("  Testing opType %d (ko=%d, ao=%d)...", 
             op, keyOffset, anyValueOffset);
      
      if (executeOps(pNdb, 
                     tab, 
                     (OpTypes)op, 
                     rowCount, 
                     keyOffset, 
                     anyValueOffset, 
                     opts))
        return NDBT_FAILED;
      
      if (checkAnyValueInEvent(pNdb, 
                               eventOldKeyData, eventKeyData,
                               eventOldAttrData, eventAttrData,
                               rowCount,
                               anyValueOffset,
                               false // always use postKey data
                               ) != NDBT_OK)
        return NDBT_FAILED;
      printf("ok\n");
    };
    
    printf("Testing AnyValue with interpreted program\n");
    opts.optionsPresent|= NdbOperation::OperationOptions::OO_INTERPRETED;
    opts.interpretedCode= &nonsenseProgram;
  }
    
  if (dropEventOperations(pNdb) != 0)
  {
    g_err << "Dropping event operations failed : " << 
      pNdb->getNdbError().code << endl;
    return NDBT_FAILED;
  }
  
  if (dropEvent(pNdb, tab->getName()) != 0)
  {
    g_err << "Dropping event failed : " << 
      pNdb->getDictionary()->getNdbError().code << endl;
    return NDBT_FAILED;
  }

  pNdb->getDictionary()->dropTable(tab->getName());
  
  return NDBT_OK;
}


int
runBug30780(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;

  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  const int cases = 4;
  int loops = ctx->getNumLoops();
  if (loops <= cases)
    loops = cases + 1;
  for (int i = 0; i<loops; i++)
  {
    int master = res.getMasterNodeId();
    int next = res.getNextMasterNodeId(master);

    res.insertErrorInNode(next, 8064);
    int val1[] = { 7213, 0 };
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (res.dumpStateOneNode(master, val2, 2))
      return NDBT_FAILED;

    int c = i % cases;
#ifdef NDB_USE_GET_ENV
    {
      char buf[100];
      const char * off = NdbEnv_GetEnv("NDB_ERR", buf, sizeof(buf));
      if (off)
      {
        c = atoi(off);
      }
    }
#endif
    switch(c){
    case 0:
      ndbout_c("stopping %u", master);
      res.restartOneDbNode(master,
                           /** initial */ false,
                           /** nostart */ true,
                           /** abort   */ true);
      break;
    case 1:
      ndbout_c("stopping %u, err 7213", master);
      val1[0] = 7213;
      val1[1] = master;
      res.dumpStateOneNode(next, val1, 2);
      break;
    case 2:
      ndbout_c("stopping %u, err 7214", master);
      val1[0] = 7214;
      val1[1] = master;
      res.dumpStateOneNode(next, val1, 2);
      break;
    case 3:
      ndbout_c("stopping %u, err 7007", master);
      res.insertErrorInNode(master, 7007);
      break;
    }
    ndbout_c("waiting for %u", master);
    res.waitNodesNoStart(&master, 1);
    ndbout_c("starting %u", master);
    res.startNodes(&master, 1);
    ndbout_c("waiting for cluster started");
    if (res.waitClusterStarted())
    {
      return NDBT_FAILED;
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int
runBug44915(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  
  NdbRestarter res;
  int error[] = { 13031, 13044, 13045, 0 };
  for (int i = 0; error[i] && result == NDBT_OK; i++)
  {
    ndbout_c("error: %d", error[i]);
    res.insertErrorInNode(res.getDbNodeId(rand() % res.getNumDbNodes()),
                          error[i]);
    
    result = runCreateEvent(ctx, step); // should fail due to error insert
    result = runCreateEvent(ctx, step); // should pass
    result = runDropEvent(ctx, step);
  }
  return result;
}

int
runBug56579(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;

  NdbRestarter res;
  Ndb* pNdb = GETNDB(step);

  int error_all[] = { 13046, 0 };
  for (int i = 0; error_all[i] && result == NDBT_OK; i++)
  {
    ndbout_c("error: %d", error_all[i]);
    res.insertErrorInAllNodes(error_all[i]);

    if (createEventOperation(pNdb, *ctx->getTab()) != 0)
    {
      return NDBT_FAILED;
    }
  }

  return result;
}

int
runBug18703871(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* ndb= GETNDB(step);
  const NdbDictionary::Table * table= ctx->getTab();

  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp = ndb->createEventOperation(buf);
  if ( pOp == NULL ) {
    g_err << "Event operation creation failed on %s" << buf << endl;
    return NDBT_FAILED;
  }

  Uint64 curr_gci;
  int res = ndb->pollEvents(0, &curr_gci);
  if (res == 1 && ndb->nextEvent() == 0)
  {
    g_err << "pollEvents returned 1, but nextEvent found none" << endl;
    ndb->dropEventOperation(pOp);
    return NDBT_FAILED;
  }
  ndb->dropEventOperation(pOp);
  return NDBT_OK;
}

int
runBug57886_create_drop(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  Ndb* pNdb = GETNDB(step);

  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
  NdbDictionary::Table tab = * ctx->getTab();

  sleep(5);

  while (loops --)
  {
    if (pDict->dropTable(tab.getName()) != 0)
    {
      return NDBT_FAILED;
    }

    if (pDict->createTable(tab) != 0)
    {
      return NDBT_FAILED;
    }

    sleep(1);
  }

  ctx->stopTest();
  return NDBT_OK;
}

int
runBug57886_subscribe_unsunscribe(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb;
  Ndb_cluster_connection *pCC;

  NdbDictionary::Table tab = * ctx->getTab();

  if (cc(&pCC, &pNdb))
  {
    // too few api slots...
    return NDBT_OK;
  }

  while (!ctx->isTestStopped())
  {
    createEvent(pNdb, tab, false, false);

    NdbEventOperation* op = createEventOperation(pNdb, tab, 0);
    if (op)
    {
      pNdb->dropEventOperation(op);
    }
    dropEvent(pNdb, tab);
  }

  delete pNdb;
  delete pCC;
  return NDBT_OK;
}

int
runBug12598496(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb *pNdb=GETNDB(step);
  NdbDictionary::Table tab = * ctx->getTab();
  createEvent(pNdb, tab, false, false);

  NdbRestarter restarter;
  int nodeId = restarter.getNode(NdbRestarter::NS_RANDOM);
  restarter.insertErrorInNode(nodeId, 13047);

  // should fail...
  if (createEventOperation(pNdb, tab, 0) != 0)
    return NDBT_FAILED;


  restarter.insertErrorInNode(nodeId, 0);
  if (restarter.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  NdbEventOperation * op = createEventOperation(pNdb, tab, 0);
  if (op == 0)
  {
    return NDBT_FAILED;
  }

  ndbout_c("restart %u", nodeId);
  restarter.restartOneDbNode(nodeId,
                             /** initial */ false,
                             /** nostart */ true,
                             /** abort   */ true);

  ndbout_c("wait not started %u", nodeId);
  if (restarter.waitNodesNoStart(&nodeId, 1) != 0)
    return NDBT_FAILED;

  ndbout_c("wait not started %u - OK", nodeId);

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  restarter.dumpStateOneNode(nodeId, val2, 2);
  restarter.insertErrorInNode(nodeId, 13047);
  restarter.insertErrorInNode(nodeId, 1003);
  ndbout_c("start %u", nodeId);
  restarter.startNodes(&nodeId, 1);

  NdbSleep_SecSleep(5);

  ndbout_c("wait not started %u", nodeId);
  if (restarter.waitNodesNoStart(&nodeId, 1) != 0)
    return NDBT_FAILED;

  ndbout_c("wait not started %u - OK", nodeId);

  ndbout_c("start %u", nodeId);
  restarter.startNodes(&nodeId, 1);
  ndbout_c("waitClusterStarted");
  if (restarter.waitClusterStarted() != 0)
    return NDBT_FAILED;

  CHK_NDB_READY(pNdb);

  pNdb->dropEventOperation(op);
  dropEvent(pNdb, tab);

  return NDBT_OK;
}

int runTryGetEvent(NDBT_Context* ctx, NDBT_Step* step)
{
  char eventName[1024];
  sprintf(eventName, "%s_EVENT", ctx->getTab()->getName());
  
  NdbDictionary::Dictionary *myDict = GETNDB(step)->getDictionary();
  NdbRestarter restarter;
  
  Uint32 iterations = 10;
  bool odd = true;

  while (iterations--)
  {
    g_err << "Attempting to get the event, expect "
          << ((odd?"success":"failure")) << endl;
    const NdbDictionary::Event* ev = myDict->getEvent(eventName);
    
    if (odd)
    {
      if (ev == NULL)
      {
        g_err << "Failed to get event on odd cycle with error "
              << myDict->getNdbError().code << " " 
              << myDict->getNdbError().message << endl;
        return NDBT_FAILED;
      }
      g_err << "Got event successfully" << endl;
      g_err << "Inserting errors 8107 + 4038" << endl;
      restarter.insertErrorInAllNodes(8107);
      restarter.insertErrorInAllNodes(4038);      
    }
    else
    {
      if (ev != NULL)
      {
        g_err << "Got event on even cycle!" << endl;
        restarter.insertErrorInAllNodes(0);
        return NDBT_FAILED;
      }
      if (myDict->getNdbError().code != 266)
      {
        g_err << "Did not get expected error.  Expected 266, got "
              << myDict->getNdbError().code << " " 
              << myDict->getNdbError().message << endl;
        return NDBT_FAILED;
      }

      g_err << "Failed to get event, clearing error insertion" << endl;
      restarter.insertErrorInAllNodes(0);
    }

    odd = !odd;
  }

  restarter.insertErrorInAllNodes(0);
  
  return NDBT_OK;
}

// Waits until the event buffer is filled upto fill_percent
// or #retries exhaust.
bool wait_to_fill_buffer(Ndb* ndb, Uint32 fill_percent)
{
  Ndb::EventBufferMemoryUsage mem_usage;
  Uint32 usage_before_wait = 0;
  Uint64 prev_gci = 0;
  Uint32 retries = 10;

  do
  {
    ndb->get_event_buffer_memory_usage(mem_usage);
    usage_before_wait = mem_usage.usage_percent;
    if (fill_percent < 100 && usage_before_wait >= fill_percent)
    {
      return true;
    }

    // Assume that latestGCI will increase in this sleep time
    // (with default TimeBetweenEpochs 100 mill).
    NdbSleep_MilliSleep(1000); 

    const Uint64 latest_gci = ndb->getLatestGCI();

    /* fill_percent == 100 :
     * It is not enough to test usage_after_wait >= 100 to decide
     * whether a gap has occurred, because a gap can occur
     * at a fill_percent < 100, eg at 99%, when there is no space
     * for a new epoch. Therefore, we have to wait until the
     * latest_gci (and usage) becomes stable, because epochs are
     * discarded during a gap.
     */
    if (prev_gci == latest_gci)
    {
      /* No new epoch is buffered despite waiting with continuous
       * load generation. A gap must have occurred. Enough waiting.
       * Check usage is unchanged as well.
       */
      ndb->get_event_buffer_memory_usage(mem_usage);
      const Uint32 usage_after_wait = mem_usage.usage_percent;
      if (usage_before_wait == usage_after_wait)
      {
        return true;
      }
      if (retries-- == 0)
      {
        g_err << "wait_to_fill_buffer failed : prev_gci "
              << prev_gci << "latest_gci " << latest_gci
              << " usage before wait " << usage_before_wait
              << " usage after wait " <<  usage_after_wait << endl;
        return false;
      }
    }
    prev_gci = latest_gci;
  } while (true);

  assert(false); // Should not reach here
}

/*********************************************************
 * Test the backward compatible pollEvents returns 1
 * when the event buffer overflows. However, the test cannot
 * guarantee that overflow epoch data is found at the
 * head of the event queue.
 * The following nextEvent call will crash with 'out of memory' error.
 * The test will fail if there is no crash.
 */
int runPollBCOverflowEB(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);
  Ndb* ndb= GETNDB(step);
  ndb->set_eventbuf_max_alloc(2621440); // max event buffer size

  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp = NULL;
  pOp = ndb->createEventOperation(buf);
  CHK(pOp != NULL, "Event operation creation failed");
  CHK(pOp->execute() == 0, "execute operation execution failed");

  // Wait until event buffer get filled 100%, to get a gap event
  if (!wait_to_fill_buffer(ndb, 100))
    return NDBT_FAILED;

  g_err << endl << "The test is expected to crash with"
        << " Event buffer out of memory." << endl << endl;

  Uint64 poll_gci = 0;
  while (ndb->pollEvents(100, &poll_gci))
  {
    while ((pOp = ndb->nextEvent()))
    {}
  }
  // The test should not come here. Expected to crash in nextEvent.
  return NDBT_FAILED;
}

/*************************************************************
 * Test: pollEvents(0) returns immediately :
 * The coumer waits max 10 secs to see an event.
 * Then it polls with 0 wait time. If it could not see
 * event data with the same epoch, it fails.
 */
int runPollBCNoWaitConsumer(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);
  Ndb* ndb= GETNDB(step);
  ndb->set_eventbuf_max_alloc(2621440); // max event buffer size

  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp = NULL, *pCreate = NULL;
  pCreate = pOp = ndb->createEventOperation(buf);
  CHK(pOp != NULL, "Event operation creation failed");
  CHK(pOp->execute() == 0, "execute operation execution failed");

  // Signal load generator
  ctx->setProperty("Listening", (Uint32)1);

  // Wait max 120 sec for event data to start flowing
  int retries = 120;
  Uint64 poll_gci = 0;
  while (retries-- > 0)
  {
    if (ndb->pollEvents(1000, &poll_gci) == 1)
      break;
    NdbSleep_SecSleep(1);
  }
  CHK(retries > 0, "No epoch has received in 10 secs");

  CHK_NDB_READY(ndb);

  g_err << "Node started" << endl;

  // pollEvents with aMilliSeconds = 0 will poll only once (no wait),
  // and it should see the event data seen above
  Uint64 poll_gci2 = 0;
  CHK((ndb->pollEvents(0, &poll_gci2) == 1),
      "pollEvents(0) hasn't seen the event data");

  if (poll_gci != poll_gci2)
    g_err << " gci-s differ: gci at first poll " << poll_gci
          << " gci at second poll " << poll_gci2 << endl;
  CHK(poll_gci == poll_gci2,
      "pollEvents(0) hasn't seen the same epoch");

  CHK((ndb->dropEventOperation(pCreate) == 0), "dropEventOperation failed");
  return NDBT_OK;
}

int runPollBCNoWait(NDBT_Context* ctx, NDBT_Step* step)
{
  // Insert one record, to test pollEvents(0).
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  while (ctx->getProperty("Listening", (Uint32)0) != 1)
  {
    NdbSleep_SecSleep(1);
  }
  CHK((hugoTrans.loadTable(GETNDB(step), 1, 1) == 0), "Insert failed");
  return NDBT_OK;
}

/*************************************************************
 * Test: pollEvents(-1) will wait long (2^32 mill secs) :
 *    To test it within a reasonable time, this wait will be
 *    ended after 10 secs by peroforming an insert by runPollBCLong()
 *    and the pollEvents sees it.
 *    If the wait will not end or it ends prematuerly (< 10 secs),
 *    the test will fail.
 */
int runPollBCLongWaitConsumer(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);
  Ndb* ndb= GETNDB(step);
  ndb->set_eventbuf_max_alloc(2621440); // max event buffer size

  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp = NULL, *pCreate = NULL;
  pCreate = pOp = ndb->createEventOperation(buf);
  CHK(pOp != NULL, "Event operation creation failed");
  CHK(pOp->execute() == 0, "execute operation execution failed");

  Uint64 poll_gci = 0;
  ndb->pollEvents(-1, &poll_gci);

  // poll has seen the insert event data now.
  CHK((ndb->dropEventOperation(pCreate) == 0), "dropEventOperation failed");
  ctx->stopTest();
  return NDBT_OK;
}

int runPollBCLongWait(NDBT_Context* ctx, NDBT_Step* step)
{
  /* runPollWaitLong() is blocked by pollEvent(-1).
   * We do not want to wait 2^32 millsec, so we end it
   * after 10 secs by sending an insert.
   */
  const NDB_TICKS startTime = NdbTick_getCurrentTicks();
  NdbSleep_SecSleep(10);

  // Insert one record, to end the consumer's long wait
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  CHK((hugoTrans.loadTable(GETNDB(step), 1, 1) == 0), "Insert failed");

  // Give max 10 sec for the consumer to see the insert
  Uint32 retries = 10;
  while (!ctx->isTestStopped() && retries-- > 0)
  {
    NdbSleep_SecSleep(1);
  }
  CHK((ctx->isTestStopped() || retries > 0),
      "Consumer hasn't seen the insert in 10 secs");

  const Uint32 duration =
    (Uint32)NdbTick_Elapsed(startTime, NdbTick_getCurrentTicks()).milliSec();

  if (duration < 10000)
  {
    g_err << "pollEvents(-1) returned earlier (" << duration
          << " secs) than expected minimum of 10 secs." << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


/*************************************************************
 * Test backward compatibility of the pollEvents related to
 * inconsistent epochs.
 * An inconsistent event data can be found at the head of the
 * event queue or after the head.
 * It it is at the head:
 *  - the backward compatible pollEvents will return 1 and
 *  - the following nextEvent call will return NULL.
 * The test writes out which case (head or after) is found.
 *
 * For each poll, nextEvent round will end the test when
 * it finds an inconsistent epoch, or process the whole queue.
 *
 * After each poll (before nextEvent call) event queue is checked
 * for inconsistency.
 * Test will fail :
 * a) if no inconsistent epoch is found after 120 poll rounds
 * b) If the  pollEvents and nextEvent found different inconsistent epochs
 * c) if the pollEvents and nextEvent found unequal #inconsistent epochs
 */
int runPollBCInconsistency(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);
  Ndb* ndb= GETNDB(step);

  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp, *pCreate = NULL;
  pCreate = pOp = ndb->createEventOperation(buf);
  CHK(pOp != NULL, "Event operation creation failed");
  CHK(pOp->execute() == 0, "execute operation execution failed");

  Uint32 n_ins = 0, n_dels = 0, n_unknown = 0;
  Uint32 n_inconsis_poll = 0, n_inconsis_next = 0;

  Uint64 inconsis_epoch_poll = 0; // inconsistent epoch seen by pollEvents
  Uint64 inconsis_epoch_next = 0;// inconsistent epoch seen by nextEvent

  Uint64 current_gci = 0, poll_gci = 0;

  // Syncronise event listening and error injection
  ctx->setProperty("Inject_error", (Uint32)0);
  ctx->setProperty("Found_inconsistency", (Uint32)0);

  // Wait max 10 sec for event data to start flowing
  Uint32 retries = 10;
  while (retries-- > 0)
  {
    if (ndb->pollEvents(1000, &poll_gci) == 1)
        break;
  }
  CHK(retries > 0, "No epoch has received in 10 secs");

  // Event data have started flowing, inject error after a sec
  NdbSleep_SecSleep(1);
  ctx->setProperty("Inject_error", (Uint32)1);

  // if no inconsistency is found within 120 poll rounds, fail
  retries = 120;
  do
  {
    // Check whether an inconsistent epoch is in the queue
    if (!ndb->isConsistent(inconsis_epoch_poll))
    {
      n_inconsis_poll++;
      ctx->setProperty("Found_inconsistency", (Uint32)1);
    }

    pOp = ndb->nextEvent();
    // Check whether an inconsistent epoch is at the head
    if (pOp == NULL)
    {
      // pollEvents returned 1, but nextEvent returned 0,
      // Should be an inconsistent epoch
      CHK(!ndb->isConsistent(inconsis_epoch_next),
          "Expected inconsistent epoch");
      g_info << "Next event found inconsistent epoch at the"
             << " head of the event queue" << endl;
      CHK((inconsis_epoch_poll != 0 &&
           inconsis_epoch_poll == inconsis_epoch_next),
          "pollEvents and nextEvent found different inconsistent epochs");
      n_inconsis_next++;
      goto end_test;

      g_err << "pollEvents returned 1, but nextEvent found no data"
            << endl;
      return NDBT_FAILED;
    }

    while (pOp)
    {
      current_gci = pOp->getGCI();

      switch (pOp->getEventType())
      {
      case NdbDictionary::Event::TE_INSERT:
        n_ins ++;
        break;
      case NdbDictionary::Event::TE_DELETE:
        n_dels ++;
        break;
      default:
        n_unknown ++;
        break;
      }

      pOp = ndb->nextEvent();
      if (pOp == NULL)
      {
        // pOp returned NULL, check it is an inconsistent epoch.
        if (!ndb->isConsistent(inconsis_epoch_next))
        {
          g_info << "Next event found inconsistent epoch"
                 << " in the event queue" << endl;
          CHK((inconsis_epoch_poll != 0 &&
               inconsis_epoch_poll == inconsis_epoch_next),
              "pollEvents and nextEvent found different inconsistent epochs");
          n_inconsis_next++;
          goto end_test;
        }
      }
    }
    
    if (inconsis_epoch_poll > 0 && inconsis_epoch_next == 0)
    {
      g_err << "Processed entire queue without finding the inconsistent epoch:"
            << endl;
      g_err << " current gci " << current_gci
            << " poll gci " << poll_gci << endl;
      goto end_test;
    }

  } while (retries-- > 0 && ndb->pollEvents(1000, &poll_gci));

end_test:

  if (retries == 0 ||
      n_inconsis_poll == 0 ||
      n_inconsis_poll != n_inconsis_next ||
      n_unknown != 0 )
  {
    g_err << "Test failed :" << endl;
    g_err << "Retries " << 120 - retries << endl;
    g_err << " #inconsistent epochs found by pollEvents "
          << n_inconsis_poll << endl;
    g_err << " #inconsistent epochs found by nextEvent "
          << n_inconsis_next << endl;
    g_err << " Inconsis epoch found by pollEvents "
          << inconsis_epoch_poll << endl;
    g_err << " inconsis epoch found by nextEvent "
          << inconsis_epoch_next << endl;
    g_err << " Inserts " << n_ins << ", deletes " << n_dels
          << ", unknowns " << n_unknown << endl;
    return NDBT_FAILED;
  }

  // Stop the transaction load to data nodes
  ctx->stopTest();

  CHK(ndb->dropEventOperation(pCreate) == 0, "dropEventOperation failed");

  return NDBT_OK;
}

/*************************************************************
 * Check highestQueuedEpoch stays stable over a poll period
 * while latestGCI increases due to newer completely-received
 * epochs get buffered
 */
int
runCheckHQElatestGCI(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();

  NdbEventOperation* evOp = createEventOperation(pNdb, *pTab);
  CHK(evOp != NULL, "Event operation creation failed");

  Uint64 highestQueuedEpoch = 0;
  int pollRetries = 120;
  int res = 0;
  while (res == 0 && pollRetries-- > 0)
  {
    res = pNdb->pollEvents2(1000, &highestQueuedEpoch);
    NdbSleep_SecSleep(1);
  }

  // 10 sec waiting should be enough to get an epoch with default
  // TimeBetweenEpochsTimeout (4 sec) and TimeBetweenEpochs (100 millsec).
  CHK(highestQueuedEpoch != 0, "No epochs received after 120 secs");


  // Wait for some more epochs to be buffered.
  int retries = 10;
  Uint64 latest = 0;
  do
  {
    NdbSleep_SecSleep(1);
    latest = pNdb->getLatestGCI();
  } while (latest <= highestQueuedEpoch && retries-- > 0);

  CHK(latest > highestQueuedEpoch, "No new epochs buffered");

  const Uint64 hqe = pNdb->getHighestQueuedEpoch();
  if (highestQueuedEpoch != hqe)
  {
    g_err << "Highest queued epoch " << highestQueuedEpoch
          << " has changed before the next poll to "
          << pNdb->getHighestQueuedEpoch() << endl;
    return NDBT_FAILED;
  }

  pNdb->pollEvents2(1000, &highestQueuedEpoch);
  if (highestQueuedEpoch <= hqe || highestQueuedEpoch < latest)
  {
    g_err << "No new epochs polled:"
          << " highestQueuedEpoch at the last poll" << hqe
          << " highestQueuedEpoch at the this poll " << highestQueuedEpoch
          << " latest epoch seen " << latest << endl;
    return NDBT_FAILED;
  }

  CHK(pNdb->dropEventOperation(evOp) == 0, "dropEventOperation failed");

  ctx->stopTest();
  return NDBT_OK;
}

/**
 * Wait until some epoch reaches the event queue and then
 * consume max nEpochs:
 * nEpochs = 0: Wait until some epoch reaches the event queue
 * and return true without consuming any event data.
 * nepochs > 0: Return true when the given number of regular epochs
 * are consumed or an empty epoch is found after some regular epochs.
 * Therefore, '#epochs consumed < nEpochs' will not be considered as an error.
 *
 * Returns false if no epoch reaches the event queue within the #pollRetries
 * or epochs are retrieved out of order.
 */

// Remember the highest queued epoch before the cluster restart
static Uint64 epoch_before_restart = 0;
bool
consumeEpochs(Ndb* ndb, Uint32 nEpochs)
{
  Uint64 op_gci = 0, curr_gci = 0, consumed_gci = 0;
  Uint32 consumed_epochs = 0, consumedRegEpochs = 0;
  Uint32 errorEpochs = 0, regularOps = 0, unknownOps = 0;
  Uint32 emptyEpochsBeforeRegular = 0, emptyEpochs = 0;

  int pollRetries = 60;
  int res = 0;
  Uint64 highestQueuedEpoch = 0;
  while (pollRetries-- > 0)
  {
    res = ndb->pollEvents2(1000, &highestQueuedEpoch);

    if (res == 0)
    {
      NdbSleep_SecSleep(1);
      continue;
    }

    if (nEpochs == 0)
    {
      g_info << "Some epochs reached the event queue. Leaving without consuming them as requested." << endl;
      epoch_before_restart = highestQueuedEpoch;
      g_info << "" << highestQueuedEpoch  << " ("
             << (Uint32)(highestQueuedEpoch >> 32) << "/"
             << (Uint32)highestQueuedEpoch << ")"
             << " pollRetries left " << pollRetries
             << " res " << res << endl;
      return true;
    }

    // Consume epochs
    Uint32 regOps = 0; // #regular ops received per epoch
    NdbEventOperation* pOp = NULL;
    while ((pOp = ndb->nextEvent2()))
    {
      NdbDictionary::Event::TableEvent err_type;
      if ((pOp->isErrorEpoch(&err_type)) ||
          (pOp->getEventType2() == NdbDictionary::Event::TE_CLUSTER_FAILURE))
        errorEpochs++;
      else if (pOp->isEmptyEpoch())
      {
        emptyEpochs++;
        if (consumedRegEpochs > 0)
        {
          g_info << "Empty epoch is found after regular epochs, returning."
                 << endl;
          consumed_epochs++;
          goto ok_exit;
        }
      }
      else if (pOp->getEventType2() == NdbDictionary::Event::TE_INSERT ||
               pOp->getEventType2() == NdbDictionary::Event::TE_DELETE ||
               pOp->getEventType2() == NdbDictionary::Event::TE_UPDATE)
      {
        regularOps++;
        regOps++;
      }
      else
      {
        g_err << "Received unexpected event type "
              << pOp->getEventType2() << endl;
        unknownOps++;
      }

      op_gci = pOp->getGCI();
      if (op_gci < curr_gci)
      {
        g_err << endl << "Out of order epochs: retrieved epoch " << op_gci
              << " (" << (Uint32)(op_gci >> 32) << "/"
              << (Uint32)op_gci << ")" << endl;
        g_err << " Curr gci " << curr_gci << " ("
              << (Uint32)(curr_gci >> 32) << "/"
              << (Uint32)curr_gci << ")" << endl;
        g_err << " Epoch before restart " << epoch_before_restart  << " ("
              << (Uint32)(epoch_before_restart >> 32) << "/"
              << (Uint32)epoch_before_restart << ")" << endl;
        g_err << " Consumed epoch " << consumed_gci << " ("
              << (Uint32)(consumed_gci >> 32) << "/"
              << (Uint32)consumed_gci << ")" << endl << endl;
        return false;
      }

      if (op_gci > curr_gci)
      {
        // epoch boundary
        consumed_gci = curr_gci;
        curr_gci = op_gci;
        consumed_epochs++;
        if (regOps > 0)
        {
          consumedRegEpochs++;
          regOps = 0;

          if (consumedRegEpochs == 1)
          {
            g_info << "Nulling pre-empty epochs " << emptyEpochs <<endl;
            emptyEpochsBeforeRegular = emptyEpochs;
            emptyEpochs = 0;
          }
        }

        if (consumedRegEpochs > 0 && consumedRegEpochs >= nEpochs)
        {
          g_info << "Requested regular epochs are consumed. "
                 << " Requested " << nEpochs
                 << "Consumed " << consumedRegEpochs << endl;
          goto ok_exit;
        }
      }
    }
    // Note epoch boundary when event queue becomes empty
    consumed_gci = curr_gci;
    consumed_epochs++;
    if (regOps > 0)
    {
      consumedRegEpochs++;
    }

    if (consumedRegEpochs > 0 && consumedRegEpochs >= nEpochs)
    {
      g_info << "Queue empty: Requested regular epochs are consumed : "
             << "Consumed " << consumedRegEpochs
             << " Requested " << nEpochs << endl;
      goto ok_exit;
    }
  }

  // Retries expired
  if ((nEpochs == 0 && highestQueuedEpoch == 0) ||
      (consumedRegEpochs == 0))
  {
    g_err << "No regular epoch reached the queue: " << endl;
    g_err << "Requested epochs to consume " << nEpochs
          << " HighestQueuedEpoch " << highestQueuedEpoch
          << " Consumed epochs " << consumed_epochs
          << " pollRetries left " << pollRetries << endl;
    return false;
  }

ok_exit:
  g_info << "ConsumeEpochs ok. Requested to consume " << nEpochs << endl;
  g_info << "Total epochs consumed " << consumed_epochs << endl;
  g_info << " Regular epochs " << consumedRegEpochs << endl;
  g_info << " Empty epochs received before regular epochs "
         << emptyEpochsBeforeRegular
         << " Empty epochs received after regular epochs " << emptyEpochs
         << endl;
  g_info << " Error epochs " << errorEpochs << endl;
  g_info << " Regualr ops " << regularOps
         << " Unknown ops " << unknownOps << endl;
  g_info << " pollRetries left " << pollRetries << endl << endl;
  return true;
}

/*************************************************************
 * The test generates some transaction load, waits til the
 * event queue gets filled, restarts cluster (initially if
 * requested), generates some transaction load and then
 * consumes all events from the queue.
 *
 * The test will fail if no epoch reaches the event queue within
 * the #pollRetries or epochs are retrieved out of order.
 */

/**
 * Table pointer to the table after cluster restart.
 * This pointer will also be used by the load generator
 * (runInsertDeleteAfterClusterFailure).
 */
static const NdbDictionary::Table *tab_ptr_after_CR;

int
runInjectClusterFailure(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  const NdbDictionary::Table tab(* ctx->getTab());

  NdbEventOperation* evOp1 = createEventOperation(pNdb, tab);
  CHK(evOp1 != NULL, "Event operation creation failed");

  // Generate some transaction load
  HugoTransactions hugoTrans(tab);
  Uint32 nOps = 1000;
  CHK(hugoTrans.loadTable(GETNDB(step), nOps, 100) == 0,
      "Failed to generate transaction load after cluster restart");

  // Poll until find some event data in the queue
  // but don't consume (nEpochs to consume is 0)
  NdbSleep_SecSleep(5);   //Wait for some events to arrive 
  CHK(consumeEpochs(pNdb, 0), "No event data found by pollEvents");

  /*
   * Drop the pre-created table before initial restart to avoid invalid
   * dict cache. Also use a copy of the pre-created table struct
   * to avoid accessing invalid memory.
   */
  const NdbDictionary::Table tab1(* ctx->getTab());

  bool initialRestart = ctx->getProperty("InitialRestart");
  bool consumeAfterDrop = ctx->getProperty("ConsumeAfterDrop");
  bool keepSomeEvOpOnClusterFailure = ctx->getProperty("KeepSomeEvOpOnClusterFailure");
  if (initialRestart)
  {
    CHK(dropEvent(pNdb, tab) == 0, pDict->getNdbError());
    CHK(pDict->dropTable(tab.getName()) == 0, pDict->getNdbError());
    g_err << "Restarting cluster initially" << endl;
  }
  else
    g_info << "Restarting cluster" << endl;

  // Restart cluster with abort
  NdbRestarter restarter;
  if (restarter.restartAll(initialRestart,
                           true, true) != 0)
    return NDBT_FAILED;

  g_err << "wait nostart" << endl;
  restarter.waitClusterNoStart();
  g_err << "startAll" << endl;
  restarter.startAll();
  g_err << "wait started" << endl;
  restarter.waitClusterStarted();
  CHK(pNdb->waitUntilReady(300) == 0, "Cluster failed to start");

  if (!keepSomeEvOpOnClusterFailure)
  {
    CHK(pNdb->dropEventOperation(evOp1) == 0, "dropEventOperation failed");
    NdbSleep_SecSleep(1);
    evOp1 = NULL;
  }

  if (initialRestart)
  {
    CHK(pDict->createTable(tab1) == 0, pDict->getNdbError());
    CHK(createEvent(pNdb, tab1, ctx)== 0, pDict->getNdbError());
  }
  tab_ptr_after_CR = pDict->getTable(tab1.getName());
  CHK(tab_ptr_after_CR != 0, pDict->getNdbError());
  
  g_info << "Signal to start the load" << endl;
  ctx->setProperty("ClusterRestarted", (Uint32)1);

  //Create event op
  NdbEventOperation* evOp2 = createEventOperation(pNdb, *tab_ptr_after_CR);
  CHK(evOp2 != NULL, "Event operation creation failed");

  // Consume 5 epochs to ensure that the event consumption
  // has started after recovery from cluster failure
  NdbSleep_SecSleep(5);   //Wait for events to arrive after restart 
  if (!consumeAfterDrop)
  {
    CHK(consumeEpochs(pNdb, 5), "Consumption after cluster restart failed");
  }

  g_info << "Signal to stop the load" << endl;
  ctx->setProperty("ClusterRestarted", (Uint32)0);
  NdbSleep_SecSleep(1);

  CHK(pNdb->dropEventOperation(evOp2) == 0, "dropEventOperation failed");
  evOp2 = NULL;

  if (consumeAfterDrop)
  {
    // First consume buffered events polled before restart.
    // If incorrectly handled, this will free the dropped evOp2.
    while (pNdb->nextEvent2() != NULL) {}

    // Poll and consume after evOp2 was dropped.
    // Events for dropped evOp2 will internally be seen by nextEvent(),
    // but should be ignored as not 'EXECUTING' - evOp2 must still exists though!
    Uint64 gci;
    CHK(pNdb->pollEvents2(1000, &gci), "Failed to pollEvents2 after restart + dropEvent");
    while (NdbEventOperation* op = pNdb->nextEvent2())
    {
      CHK((op!=evOp2), "Received events for 'evOp2' after it was dropped")
    }
  }

  if (keepSomeEvOpOnClusterFailure)
  {
    CHK(pNdb->dropEventOperation(evOp1) == 0, "dropEventOperation failed");
    NdbSleep_SecSleep(1);
    evOp1 = NULL;
  }
  return NDBT_OK;
}

int
runInsertDeleteAfterClusterFailure(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = ctx->getNumRecords();
  while (ctx->isTestStopped() == false &&
      ctx->getProperty("ClusterRestarted", (Uint32)0) == 0)
  {
    NdbSleep_SecSleep(1);
  }

  HugoTransactions hugoTrans(*tab_ptr_after_CR);
  UtilTransactions utilTrans(*tab_ptr_after_CR);

  while (ctx->getProperty("ClusterRestarted", (Uint32)0) == 1 &&
         ctx->isTestStopped() == false)
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

/******** Test event buffer overflow **********/

/**
 * Test the production and consumption of gap epochs
 * (having evnt type TE_OUT_OF_MEMORY) wth of a slow
 * listenenr causing event buffer to overflow (runTardyEventListener())
 */

// Collect statistics
class ConsumptionStatistics
{
public:
  ConsumptionStatistics();

  // Count the gaps consumed while the test is progressing.
  Uint32 consumedGaps;

  Uint32 emptyEpochs; // Count the empty epochs in the test

  // Number of event buffer overflows, the listener has intended to consume
  static const Uint32 totalGaps = 6;

  Uint64 gap_epoch[totalGaps]; // Store the gap epochs consumed

  /** consumed_epochs[0] : #epochs the event buffer can accomodate
   * before the first overflow.
   * consumed_epochs[1-5] : Consumed epochs between each gaps.
   */
  Uint32 consumed_epochs[totalGaps];

  void print();
};

ConsumptionStatistics::ConsumptionStatistics() : consumedGaps(0), emptyEpochs(0)
{
  for (Uint32 i = 0; i < totalGaps; i++)
  {
    gap_epoch[i] = 0;
    consumed_epochs[i] = 0;
  }
}

void ConsumptionStatistics::print()
{
  /* Buffer capacity : #epochs event buffer can accomodate.
   * The test fills the event buffer twice.
   * The buffer capacity of the first and the second rounds
   * should be comparable, with a small difference due to
   * timimg of transactions and epochs. However,
   * considering the different machine/platforms the test will
   * be run on, the difference is not intended to be used as
   * a test succees/failure criteria.
   * Instead both values are written out for manual inspection.
   */
  if (consumedGaps == 0)
    g_err << "Test failed. No epochs consumed." << endl;
  else if (consumedGaps < totalGaps)
    g_err << "Test failed. Less epochs consumed. Expected: "
          << totalGaps << " Consumed: " << consumedGaps << endl;

  // Calculate the event buffer capacity in the second round of filling
  Uint32 bufferCapacitySecondRound = 0;
  for (Uint32 i=1; i < totalGaps; ++i)
    bufferCapacitySecondRound += consumed_epochs[i];
  bufferCapacitySecondRound -= consumedGaps; // Exclude overflow epochs

  g_err << endl << "Empty epochs consumed : " << emptyEpochs << "." << endl;

  g_err << "Expected gap epochs : " << totalGaps
        << ", consumed gap epochs : " << consumedGaps << "." << endl;

  if (consumedGaps > 0)
  {
    g_err << "Gap epoch | Consumed epochs before this gap : " << endl;
    for (Uint32 i = 0; i< consumedGaps; ++i)
    {
      g_err << gap_epoch[i] << " | "
            << consumed_epochs[i] << endl;
    }

    g_err << endl
          << "Buffer capacity (Epochs consumed before first gap occurred) : "
          << consumed_epochs[0] << " epochs." << endl;
    g_err << "Epochs consumed after first gap until the buffer"
          << " got filled again (excluding overflow epochs): "
          << bufferCapacitySecondRound << "." << endl << endl;
  }
}

/* Consume event data until all gaps are consumed or
 * free_percent space in the event buffer becomes available
 */
bool consume_buffer(NDBT_Context* ctx, Ndb* ndb,
                   NdbEventOperation *pOp,
                   Uint32 buffer_percent,
                   ConsumptionStatistics &stats)
{
  Ndb::EventBufferMemoryUsage mem_usage;
  ndb->get_event_buffer_memory_usage(mem_usage);
  Uint32 prev_mem_usage = mem_usage.usage_percent;
  Uint64 op_gci = 0, curr_gci = 0;

  Uint64 poll_gci = 0;
  int poll_retries = 10;
  int res = 0;
  while (poll_retries-- > 0 && (res = ndb->pollEvents2(1000, &poll_gci)))
  {
    while ((pOp = ndb->nextEvent2()))
    {
      op_gci = pOp->getGCI();

      // -------- handle epoch boundary --------
      if (op_gci > curr_gci)
      {
        curr_gci = op_gci;
        stats.consumed_epochs[stats.consumedGaps] ++;

        if (pOp->getEventType2() == NdbDictionary::Event::TE_EMPTY)
          stats.emptyEpochs++;
        else
          if (pOp->getEventType2() == NdbDictionary::Event::TE_OUT_OF_MEMORY)
          {
            stats.gap_epoch[stats.consumedGaps++] = op_gci;
            if (stats.consumedGaps == stats.totalGaps)
              return true;
          }

        // Ensure that the event buffer memory usage doesn't grow during a gap
        ndb->get_event_buffer_memory_usage(mem_usage);
        const Uint32 current_mem_usage = mem_usage.usage_percent;
        if (current_mem_usage > prev_mem_usage)
        {
          g_err << "Test failed: The buffer usage grows during gap." << endl;
          g_err << " Prev mem usage " << prev_mem_usage
                << ", Current mem usage " << current_mem_usage << endl;
          return false;
        }

        /**
         * Consume until
         * a) the whole event buffer is consumed or
         * b) >= free_percent is consumed such that buffering can be resumed
         * (For case b) buffer_percent must be < (100-free_percent)
         * for resumption).
         */

        if (current_mem_usage == 0 ||
            current_mem_usage < buffer_percent)
        {
          return true;
        }
        prev_mem_usage = current_mem_usage;
      }
    }
  }

  // Event queue became empty or err before reaching the consumption target
  g_err << "Test failed: consumption target " << buffer_percent
        << " is not reached after "
        << poll_retries << " poll retries."
        << " poll results " << res << endl;

  return false;
}

/**
 * Test: Emulate a tardy consumer as follows :
 * Fill the event buffer to 100% initially, in order to accelerate
 * the gap occurence.
 * Then let the consumer to consume and free the buffer a little
 *   more than free_percent (20), such that buffering resumes again.
 *   Fill 100%. Repeat this consume/fill until 'n' gaps are
 *   produced and all are consumed, where n = ((100 / 20) + 1) = 6.
 *   When 6-th gap is produced, the event buffer is filled for the second time.
 * The load generator (insert/delete) is stopped after 6 gaps are produced.
 * Then the consumer consumes all produced gap epochs.
 * Test succeeds when : all gaps are consumed,
 * Test fails if
 *  a) producer cannot produce a gap
 *  b) event buffer usage grows during a gap (when consuming until free %)
 *  c) consumer cannot consume the given target buffer % within #retries
 *  d) Total gaps consumed < 6.
 */
int runTardyEventListener(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  const NdbDictionary::Table * table= ctx->getTab();
  HugoTransactions hugoTrans(* table);
  Ndb* ndb= GETNDB(step);

  ndb->set_eventbuf_max_alloc(5*1024*1024); // max event buffer size 1024*1024 
  const Uint32 free_percent = 20;
  ndb->set_eventbuffer_free_percent(free_percent);

  ConsumptionStatistics statistics;

  char buf[1024];
  sprintf(buf, "%s_EVENT", table->getName());
  NdbEventOperation *pOp, *pCreate = 0;
  pCreate = pOp = ndb->createEventOperation(buf);
  CHK(pOp != NULL, "Event operation creation failed");
  CHK(pOp->execute() == 0, "Execute operation execution failed");

  bool res = true;
  Uint32 producedGaps = 0;

  while (producedGaps++ < statistics.totalGaps)
  {
    /**
     * Fill the event buffer completely to 100% :
     *  - First time : to speed up the test,
     *  - then fill (~ free_percent) after resuming buffering
     */
    if (!wait_to_fill_buffer(ndb, 100))
    {
      goto end_test;
    }

    /**
     * The buffer has overflown, consume until buffer gets
     * free_percent space free, such that buffering can be resumed.
     */
    res = consume_buffer(ctx, ndb, pOp, (100 - free_percent), statistics);
    if (!res)
    {
      goto end_test;
    }
  }

  // Signal the load generator to stop the load
  ctx->stopTest();

  // Consume the whole event buffer, including last gap 
  // (buffer_percent to be consumed = 100 - 100 = 0)
  res = consume_buffer(ctx, ndb, pOp, 0, statistics);

end_test:

  if (!res)
    g_err << "consume_buffer failed." << endl;

  if (!res || statistics.consumedGaps != statistics.totalGaps)
    result = NDBT_FAILED;

  if (result == NDBT_FAILED)
    statistics.print();

  CHK(ndb->dropEventOperation(pOp) == 0, "dropEventOperation failed");
  return result;
}

/**
 * Inject error to crash the coordinator dbdict while performing dropEvent
 * after sumas have removed the subscriptions and returned execSUB_REMOVE_CONF
 * but beore the coordinator deletes the event from the systable.
 *
 * Test whether the dropped event is dangling in the sysTable.
 *
 * The test will fail if the following create/drop events fail
 * due to the dangling event.
 */
int runCreateDropEventOperation_NF(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb *pNdb=GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  const NdbDictionary::Table& tab= *ctx->getTab();

  char eventName[1024];
  sprintf(eventName,"%s_EVENT",tab.getName());

  NdbDictionary::Event myEvent(eventName);
  myEvent.setTable(tab.getName());
  myEvent.addTableEvent(NdbDictionary::Event::TE_ALL);

  NdbEventOperation *pOp = pNdb->createEventOperation(eventName);
  CHK(pOp != NULL, "Event operation creation failed");

  CHK(pOp->execute() == 0, "Execute operation execution failed");

  NdbRestarter restarter;
  int nodeid = restarter.getMasterNodeId();

  int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, NRT_NoStart_Restart};
  if (restarter.dumpStateOneNode(nodeid, val, 2))
  {
    return NDBT_FAILED;
  }

  restarter.insertErrorInNode(nodeid, 6125);

  const int res = pDict->dropEvent(eventName);
  if (res != 0)
  {
    g_err << "Failed to drop event: res "<< res <<" "
          << pDict->getNdbError().code << " : "
          << pDict->getNdbError().message << endl;
  }
  else
    g_info << "Dropped event1" << endl;

  if (restarter.waitNodesNoStart(&nodeid, 1) != 0)
  {
    g_err << "Master node " << nodeid << " never crashed." << endl;
    return NDBT_FAILED;
  }
  restarter.startNodes(&nodeid, 1);

  g_info << "Waiting for the node to start" << endl;
  if (restarter.waitClusterStarted(120) != 0)
  {
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  CHK_NDB_READY(pNdb);

  g_err << "Node started" << endl;

// g_err << "Dropping ev op:"
//       << " will remove the MARKED_DROPPED subscription at Suma" << endl;
//  CHK(pNdb->dropEventOperation(pOp) == 0, "dropEventOperation failed");

  int res1 = pDict->dropEvent(eventName);
  if (res1 != 0)
  {
    if (pDict->getNdbError().code == 4710)
    {
      // "4710 : Event not found" is expected since it is dropped above.
      res1 = 0;
      g_info << "Dropped event2" << endl;
    }
    else
    {
      g_err << "Failed to drop event: res1 "<< res1 <<" "
	    << pDict->getNdbError().code << " : "
	    << pDict->getNdbError().message << endl;
    }
  }

  const int res2 = pDict->createEvent(myEvent);
  if (res2 != 0)
    g_err << "Failed to cre event: res2 " << res2 << " "
          << pDict->getNdbError().code << " : "
          << pDict->getNdbError().message << endl;
  else
    g_info << "Event created1" << endl;

  const int res3 = pDict->dropEvent(eventName, -1);
  if (res3 != 0) {
    g_err << "Failed to drop event: res3 "<< res3 << " "
          << pDict->getNdbError().code << " : "
          << pDict->getNdbError().message << endl;
  }
  else
    g_info << "Dropped event3" << endl;

  const int res4 = pDict->createEvent(myEvent);
  if (res4)
    g_err << "Failed to cre event: res4 " << res4 << " "
          << pDict->getNdbError().code << " : "
          << pDict->getNdbError().message << endl;
  else
    g_info << "Event created2" << endl;

// clean up the newly created evnt and the eventops
  const int res5 = pDict->dropEvent(eventName, -1);
  if (res5 != 0) {
    g_err << "Failed to drop event: res5 "<< res5 << " "
          << pDict->getNdbError().code << " : "
          << pDict->getNdbError().message << endl;
  }
  else
    g_info << "Dropped event3" << endl;

  CHK(pNdb->dropEventOperation(pOp) == 0, "dropEventOperation failed");

  if (res || res1 || res2 || res3 || res4 || res5)
    return NDBT_FAILED;
  return NDBT_OK;
}


int
runBlockingRead(NDBT_Context* ctx, NDBT_Step* step)
{
  const NdbDictionary::Table* table = ctx->getTab();
  Ndb* ndb = GETNDB(step);

  /* Next do some NdbApi task that blocks */
  {
    HugoTransactions hugoTrans(*table);

    /* Load one record into the table */
    CHK(hugoTrans.loadTable(ndb, 1) == 0, "Failed to insert row");
  }

  /* Setup a read */
  HugoOperations read1(*table);
  CHK(read1.startTransaction(ndb) == 0, "Failed to start transaction");
  CHK(read1.pkReadRecord(ndb, 0, 1, NdbOperation::LM_Exclusive) == 0,
      "Failed to define locking row read");
  CHK(read1.execute_NoCommit(ndb) == 0, "Failed to obtain row lock");

  /* Setup a competing read */
  HugoOperations read2(*table);
  CHK(read2.startTransaction(ndb) == 0, "Failed to start transaction");
  CHK(read2.pkReadRecord(ndb, 0, 1, NdbOperation::LM_Exclusive) == 0,
      "Failed to define competing locking read");

  const Uint32 startCCCount =
    ndb->get_ndb_cluster_connection().get_connect_count();

  ndbout_c("Cluster connection count : %u",
           startCCCount);

  /* Executing this read will fail, and it will timeout
   * after at least the specified TDDT with error 266.
   * The interesting part of the TC is whether we are
   * still connected to the cluster at this time!
   */
  ndbout_c("Executing competing read, will block...");
  int rc = read2.execute_NoCommit(ndb);

  const Uint32 postCCCount =
    ndb->get_ndb_cluster_connection().get_connect_count();

  ndbout_c("Execute rc = %u", rc);
  ndbout_c("Cluster connection count : %u",
           postCCCount);

  CHK(rc == 266, "Got unexpected read return code");

  ndbout_c("Success");

  read1.execute_Rollback(ndb);
  read1.closeTransaction(ndb);
  read2.closeTransaction(ndb);

  return NDBT_OK;
}

int
runSlowGCPCompleteAck(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * Bug #18753341 NDB : SLOW NDBAPI OPERATIONS CAN CAUSE
   * MAXBUFFEREDEPOCHS TO BE EXCEEDED
   *
   * ClusterMgr was buffering SUB_GCP_COMMIT_ACK to be
   * sent by the receiver thread.
   * In some cases the receiver is the same thread for
   * a long time, and if it does not regularly flush
   * its send buffers then the ACKs don't get sent.
   */

  NdbRestarter restarter;

  /* We chose a value larger than the normal
   * MaxBufferedEpochs * TimeBetweenEpochs
   * to test for interaction between the two
   */
  const int TransactionDeadlockTimeout = 50000;

  /* First increase TDDT */
  int dumpCode[] = {DumpStateOrd::TcSetTransactionTimeout,
                    TransactionDeadlockTimeout};
  ndbout_c("Setting TDDT to %u millis",
           TransactionDeadlockTimeout);
  restarter.dumpStateAllNodes(dumpCode, 2);

  /* Next setup event operation so that we are a
   * subscriber
   */
  const NdbDictionary::Table* table = ctx->getTab();
  Ndb* ndb = GETNDB(step);
  NdbEventOperation* eventOp = createEventOperation(ndb, *table);
  CHK(eventOp != NULL, "Failed to create and execute EventOp");

  int result = runBlockingRead(ctx, step);

  ndb->dropEventOperation(eventOp);

  /* Restore TDDT from config setting */
  restarter.restartAll();

  return result;
}

int runCreateMultipleEvents(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Create multiple events */
  Ndb *pNdb = GETNDB(step);
  const NdbDictionary::Table *tab = ctx->getTab();
  int numOfEvents = ctx->getProperty("numOfEvents") *
                      ctx->getProperty("numOfThreads");

  for (int i = 0; i < numOfEvents; i++)
  {
    if (createEvent(pNdb, *tab, false, false, i+1) != 0){
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int runCreateDropMultipleEventOperations(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb;
  Ndb_cluster_connection *pCC;

  if (cc(&pCC, &pNdb))
  {
    // too few api slots...
    return NDBT_OK;
  }
  /* Create multiple event ops */
  NdbRestarter restarter;
  int res = NDBT_OK;

  const NdbDictionary::Table *tab= ctx->getTab();
  int numOfEvents = ctx->getProperty("numOfEvents");
  int eventID = (numOfEvents * (step->getStepNo() - 1)) + 1;

  Vector<NdbEventOperation*> pOpArr;
  for (int i = 0; i < numOfEvents; i++, eventID++)
  {
    NdbEventOperation *pOp = createEventOperation(pNdb, *tab, true, eventID);
    if (pOp == NULL) {
      res = NDBT_FAILED;
      goto dropEvents;
    }
    pOpArr.push_back(pOp);
  }

  restarter.insertErrorInAllNodes(13051);

  dropEvents:
  for(uint i =0; i < pOpArr.size(); i++)
  {
    if (pNdb->dropEventOperation(pOpArr[i]) != 0) {
      g_err << "operation drop failed\n";
      res = NDBT_FAILED;
    }
  }
  restarter.insertErrorInAllNodes(0);

  delete pNdb;
  delete pCC;
  return res;
}

int runDropMultipleEvents(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Drop multiple events from the table */
  Ndb *pNdb = GETNDB(step);
  const NdbDictionary::Table *tab = ctx->getTab();
  int numOfEvents = ctx->getProperty("numOfEvents");
  for (int i = 0; i < numOfEvents; i++)
  {
    if (dropEvent(pNdb, *tab, i+1) != 0){
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int runCheckAllNodesOnline(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  if(restarter.waitClusterStarted(1) != 0){
    g_err << "All nodes were not online " << endl;
    return NDBT_FAILED;
  }

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
TESTCASE("EventOperationApplier_NS",
	 "Verify that if we apply the data we get from event "
	 "operation is the same as the original table"
	 "NOTE! No errors are allowed!" ){
  TC_PROPERTY("Graceful", 1);
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
TESTCASE("SubscribeUnsubscribeWithLoad", 
	 "A bunch of threads doing subscribe/unsubscribe in loop"
         " while another thread does insert and deletes"
	 "NOTE! No errors from subscribe/unsubscribe are allowed!" ){
  INITIALIZER(runCreateEvent);
  STEPS(runSubscribeUnsubscribe, 16);
  STEP(runInsertDeleteUntilStopped);
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
TESTCASE("SubscribeNR", ""){
  TC_PROPERTY("ReportSubscribe", 1);
  TC_PROPERTY("SubscribeUntilStopped", 1);  
  INITIALIZER(runCreateEvent);
  STEPS(runSubscribeUnsubscribe, 5);
  STEP(runNFSubscribe);
  FINALIZER(runDropEvent);
}
TESTCASE("EventBufferOverflow",
         "Simulating EventBuffer overflow while node restart"
         "NOTE! No errors are allowed!" ){
  INITIALIZER(runCreateEvent);
  STEP(errorInjectBufferOverflow);
  FINALIZER(runDropEvent);
}
TESTCASE("StallingSubscriber",
         "Simulating slow subscriber that will become disconnected"
         "NOTE! No errors are allowed!" ){
  INITIALIZER(runCreateEvent);
  STEP(errorInjectStalling);
}
TESTCASE("Bug33793", ""){
  INITIALIZER(runCreateEvent);
  STEP(runEventListenerUntilStopped);
  STEP(runBug33793);
  FINALIZER(runDropEvent);
}
TESTCASE("Bug34853", ""){
  INITIALIZER(runCreateEvent);
  INITIALIZER(runBug34853);
  FINALIZER(runDropEvent);
}
TESTCASE("Bug35208", ""){
  INITIALIZER(runBug35208_createTable);
  INITIALIZER(runCreateEvent);
  INITIALIZER(runCreateShadowTable);
  STEP(runBug35208);
  STEP(runEventApplier);
  FINALIZER(runDropEvent);
  FINALIZER(runVerify);
  FINALIZER(runDropShadowTable);
}
TESTCASE("Bug37279", "")
{
  INITIALIZER(runBug37279);
}
TESTCASE("Bug37338", "")
{
  INITIALIZER(runBug37338);
}
TESTCASE("Bug37442", "")
{
  INITIALIZER(runBug37442);
}
TESTCASE("Bug37672", "NdbRecord option OO_ANYVALUE causes interpreted delete to abort.")
{
  INITIALIZER(runBug37672);
}
TESTCASE("Bug30780", "")
{
  INITIALIZER(runCreateEvent);
  INITIALIZER(runLoadTable);
  STEP(runEventConsumer);
  STEPS(runScanUpdateUntilStopped, 3);
  STEP(runBug30780);
  FINALIZER(runDropEvent);
}
TESTCASE("Bug44915", "")
{
  INITIALIZER(runBug44915);
}
TESTCASE("Bug56579", "")
{
  INITIALIZER(runCreateEvent);
  STEP(runBug56579);
  FINALIZER(runDropEvent);
}
TESTCASE("Bug57886", "")
{
  STEP(runBug57886_create_drop);
  STEPS(runBug57886_subscribe_unsunscribe, 5);
}
TESTCASE("Bug12598496", "")
{
  INITIALIZER(runBug12598496);
}
TESTCASE("DbUtilRace",
         "Test DbUtil handling of TC result race")
{
  INITIALIZER(runCreateEvent);
  STEP(runTryGetEvent);
  FINALIZER(runDropEvent);
}
TESTCASE("Bug18703871", "")
{
  INITIALIZER(runCreateEvent);
  STEP(runBug18703871);
  FINALIZER(runDropEvent);
}
TESTCASE("NextEventRemoveInconsisEvent", "")
{
  INITIALIZER(runCreateEvent);
  STEP(runEventListenerCheckProgressUntilStopped);
  STEP(runInsertDeleteUntilStopped);
  STEP(errorInjectBufferOverflowOnly);
  FINALIZER(runDropEvent);
}
TESTCASE("EmptyUpdates", 
	 "Verify that we can monitor empty updates"
	 "NOTE! No errors are allowed!" ){
  TC_PROPERTY("AllowEmptyUpdates", 1);
  INITIALIZER(runCreateEvent);
  STEP(runEventOperation);
  STEP(runEventLoad);
  FINALIZER(runDropEvent);
}
TESTCASE("Apiv2EmptyEpochs",
         "Verify the behaviour of the new API w.r.t."
         "empty epochs")
{
  INITIALIZER(runCreateEvent);
  STEP(runListenEmptyEpochs);
  FINALIZER(runDropEvent);
}
TESTCASE("BackwardCompatiblePollNoWait",
         "Check backward compatibility for pollEvents"
         "when poll does not wait")
{
  INITIALIZER(runCreateEvent);
  STEP(runPollBCNoWaitConsumer);
  STEP(runPollBCNoWait);
  FINALIZER(runDropEvent);
}
TESTCASE("BackwardCompatiblePollLongWait",
         "Check backward compatibility for pollEvents"
         "when poll waits long")
{
  INITIALIZER(runCreateEvent);
  STEP(runPollBCLongWaitConsumer);
  STEP(runPollBCLongWait);
  FINALIZER(runDropEvent);
}
TESTCASE("BackwardCompatiblePollInconsistency",
         "Check backward compatibility of pollEvents"
          "when handling data node buffer overflow")
{
  INITIALIZER(runCreateEvent);
  STEP(runInsertDeleteUntilStopped);
  STEP(runPollBCInconsistency);
  STEP(errorInjectBufferOverflowOnly);
  FINALIZER(runDropEvent);
}
TESTCASE("Apiv2HQE-latestGCI",
         "Verify the behaviour of the new API w.r.t."
         "highest queued and latest received epochs")
{
  INITIALIZER(runCreateEvent);
  STEP(runInsertDeleteUntilStopped);
  STEP(runCheckHQElatestGCI);
  FINALIZER(runDropEvent);
}
TESTCASE("Apiv2-check_event_queue_cleared",
         "Check whether subcriptions been dropped "
         "and recreated after a cluster restart "
         "cause any problem for event consumption.")
{
  INITIALIZER(runCreateEvent);
  STEP(runInjectClusterFailure);
  STEP(runInsertDeleteAfterClusterFailure);
}
TESTCASE("Apiv2-check_event_queue_cleared_initial",
         "test Bug 18411034 : Check whether the event queue is cleared "
         "after a cluster failure causing subcriptions to be "
         "dropped and recreated, and cluster is restarted initially.")
{
  TC_PROPERTY("InitialRestart", 1);
  INITIALIZER(runCreateEvent);
  STEP(runInjectClusterFailure);
  STEP(runInsertDeleteAfterClusterFailure);
}
TESTCASE("Apiv2-check_event_received_after_restart",
         "Check whether latestGCI is properly reset "
         "after a cluster failure. Even if subcriptions are "
         "dropped and recreated 'out of order', such that "
         "'active_op_count == 0' is never reached.")
{
  TC_PROPERTY("InitialRestart", 1);
  TC_PROPERTY("KeepSomeEvOpOnClusterFailure", 1);
  INITIALIZER(runCreateEvent);
  STEP(runInjectClusterFailure);
  STEP(runInsertDeleteAfterClusterFailure);
}
TESTCASE("Apiv2-check_drop_event_op_after_restart",
         "Check garbage collection of a dropped event operation "
         "after a cluster failure resetting the GCI sequence.")
{
  TC_PROPERTY("InitialRestart", 1);
  TC_PROPERTY("KeepSomeEvOpOnClusterFailure", 1);
  TC_PROPERTY("ConsumeAfterDrop", 1);
  INITIALIZER(runCreateEvent);
  STEP(runInjectClusterFailure);
  STEP(runInsertDeleteAfterClusterFailure);
}
TESTCASE("Apiv2EventBufferOverflow",
         "Check gap-resume works by: create a gap"
         "and consume to free free_percent of buffer; repeat")
{
  INITIALIZER(runCreateEvent);
  STEP(runInsertDeleteUntilStopped);
  STEP(runTardyEventListener);
  FINALIZER(runDropEvent);
}
TESTCASE("createDropEvent_NF",
         "Check cleanup works when Dbdict crashes before"
         " the event is deleted from the dictionary"
         " while performing dropEvent")
{
  INITIALIZER(runCreateEvent);
  STEP(runCreateDropEventOperation_NF);
}
TESTCASE("SlowGCP_COMPLETE_ACK",
         "Show problem where GCP_COMPLETE_ACK is not flushed")
{
  INITIALIZER(runCreateEvent);
  STEP(runSlowGCPCompleteAck);
  FINALIZER(runDropEvent);
}
TESTCASE("checkParallelTriggerDropReqHandling",
         "Flood the DBDICT with lots of SUB_STOP_REQs and "
         "check that the SUMA handles them properly without "
         "flooding the DBTUP with DROP_TRIG_IMPL_REQs")
{
  TC_PROPERTY("numOfEvents", 100);
  TC_PROPERTY("numOfThreads", 10);
  INITIALIZER(runCreateMultipleEvents);
  STEPS(runCreateDropMultipleEventOperations, 10);
  VERIFIER(runCheckAllNodesOnline);
  FINALIZER(runDropMultipleEvents);
}

#if 0
TESTCASE("BackwardCompatiblePollCOverflowEB",
         "Check whether backward compatibility of pollEvents  manually"
         "to see it causes process exit when nextEvent() processes an"
         "event buffer overflow event data fom the queue")
{
  INITIALIZER(runCreateEvent);
  STEP(runPollBCOverflowEB);
  STEP(runInsertDeleteUntilStopped);
  FINALIZER(runDropEvent);
}
#endif
NDBT_TESTSUITE_END(test_event);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(test_event);
  test_event.setCreateAllTables(true);
  return test_event.execute(argc, argv);
}

template class Vector<HugoOperations *>;
template class Vector<NdbEventOperation *>;
template class Vector<NdbRecAttr*>;
template class Vector<Vector<NdbRecAttr*> >;
