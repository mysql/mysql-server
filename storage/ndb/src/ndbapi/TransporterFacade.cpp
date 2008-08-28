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

#include <ndb_global.h>
#include <my_pthread.h>
#include <ndb_limits.h>
#include "TransporterFacade.hpp"
#include "ClusterMgr.hpp"
#include <IPCConfig.hpp>
#include <TransporterCallback.hpp>
#include <TransporterRegistry.hpp>
#include "NdbApiSignal.hpp"
#include <NdbOut.hpp>
#include <NdbEnv.h>
#include <NdbSleep.h>

#include "API.hpp"
#include <mgmapi_config_parameters.h>
#include <mgmapi_configuration.hpp>
#include <NdbConfig.h>
#include <ndb_version.h>
#include <SignalLoggerManager.hpp>
#include <kernel/ndb_limits.h>
#include <signaldata/AlterTable.hpp>
#include <signaldata/SumaImpl.hpp>

//#define REPORT_TRANSPORTER
//#define API_TRACE

static int numberToIndex(int number)
{
  return number - MIN_API_BLOCK_NO;
}

static int indexToNumber(int index)
{
  return index + MIN_API_BLOCK_NO;
}

#if defined DEBUG_TRANSPORTER
#define TRP_DEBUG(t) ndbout << __FILE__ << ":" << __LINE__ << ":" << t << endl;
#else
#define TRP_DEBUG(t)
#endif

/*****************************************************************************
 * Call back functions
 *****************************************************************************/

void* ndb_thread_add_thread_id(void *param)
{
  return NULL;
}

void *ndb_thread_remove_thread_id(void *param)
{
  return NULL;
}

void ndb_thread_fill_thread_object(void *param, uint *len, my_bool server)
{
  *len = 0;
}

void
TransporterFacade::reportError(NodeId nodeId,
                               TransporterError errorCode, const char *info)
{
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: reportError (nodeId=%d, errorCode=%d) %s", 
	   (int)nodeId, (int)errorCode, info ? info : "");
#endif
  if(errorCode & TE_DO_DISCONNECT) {
    ndbout_c("reportError (%d, %d) %s", (int)nodeId, (int)errorCode,
	     info ? info : "");
    doDisconnect(nodeId);
  }
}

/**
 * Report average send length in bytes (4096 last sends)
 */
void
TransporterFacade::reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes)
{
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: reportSendLen (nodeId=%d, bytes/count=%d)", 
	   (int)nodeId, (Uint32)(bytes/count));
#endif
  (void)nodeId;
  (void)count;
  (void)bytes;
}

/** 
 * Report average receive length in bytes (4096 last receives)
 */
void
TransporterFacade::reportReceiveLen(NodeId nodeId, Uint32 count, Uint64 bytes)
{
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: reportReceiveLen (nodeId=%d, bytes/count=%d)", 
	   (int)nodeId, (Uint32)(bytes/count));
#endif
  (void)nodeId;
  (void)count;
  (void)bytes;
}

/**
 * Report connection established
 */
void
TransporterFacade::reportConnect(NodeId nodeId)
{
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: API reportConnect (nodeId=%d)", (int)nodeId);
#endif
  reportConnected(nodeId);
}

/**
 * Report connection broken
 */
void
TransporterFacade::reportDisconnect(NodeId nodeId, Uint32 error){
#ifdef REPORT_TRANSPORTER
  ndbout_c("REPORT_TRANSP: API reportDisconnect (nodeId=%d)", (int)nodeId);
#endif
  reportDisconnected(nodeId);
}

void
TransporterFacade::transporter_recv_from(NodeId nodeId)
{
  hb_received(nodeId);
}

/****************************************************************************
 * 
 *****************************************************************************/

/**
 * Report connection broken
 */
int
TransporterFacade::checkJobBuffer()
{
  return 0;
}

#ifdef API_TRACE
static const char * API_SIGNAL_LOG = "API_SIGNAL_LOG";
static const char * apiSignalLog   = 0;
static SignalLoggerManager signalLogger;
static
inline
bool
setSignalLog(){
  signalLogger.flushSignalLog();

  const char * tmp = NdbEnv_GetEnv(API_SIGNAL_LOG, (char *)0, 0);
  if(tmp != 0 && apiSignalLog != 0 && strcmp(tmp,apiSignalLog) == 0){
    return true;
  } else if(tmp == 0 && apiSignalLog == 0){
    return false;
  } else if(tmp == 0 && apiSignalLog != 0){
    signalLogger.setOutputStream(0);
    apiSignalLog = tmp;
    return false;
  } else if(tmp !=0){
    if (strcmp(tmp, "-") == 0)
        signalLogger.setOutputStream(stdout);
#ifndef DBUG_OFF
    else if (strcmp(tmp, "+") == 0)
        signalLogger.setOutputStream(DBUG_FILE);
#endif
    else
        signalLogger.setOutputStream(fopen(tmp, "w"));
    apiSignalLog = tmp;
    return true;
  }
  return false;
}
inline
bool
TRACE_GSN(Uint32 gsn)
{
  switch(gsn){
#ifndef TRACE_APIREGREQ
  case GSN_API_REGREQ:
  case GSN_API_REGCONF:
    return false;
#endif
#if 1
  case GSN_SUB_GCP_COMPLETE_REP:
  case GSN_SUB_GCP_COMPLETE_ACK:
    return false;
#endif
  default:
    return true;
  }
}
#endif

/**
 * The execute function : Handle received signal
 */
void
TransporterFacade::deliver_signal(SignalHeader * const header,
                                  Uint8 prio, Uint32 * const theData,
                                  LinearSectionPtr ptr[3])
{

  TransporterFacade::ThreadData::Object_Execute oe; 
  Uint32 tRecBlockNo = header->theReceiversBlockNumber;
  
#ifdef API_TRACE
  if(setSignalLog() && TRACE_GSN(header->theVerId_signalNumber)){
    signalLogger.executeSignal(* header, 
			       prio,
                               theData,
			       ownId(),
                               ptr, header->m_noOfSections);
    signalLogger.flushSignalLog();
  }
#endif  

  if (tRecBlockNo >= MIN_API_BLOCK_NO) {
    oe = m_threads.get(tRecBlockNo);
    if (oe.m_object != 0 && oe.m_executeFunction != 0) {
      /**
       * Handle received signal immediately to avoid any unnecessary
       * copying of data, allocation of memory and other things. Copying
       * of data could be interesting to support several priority levels
       * and to support a special memory structure when executing the
       * signals. Neither of those are interesting when receiving data
       * in the NDBAPI. The NDBAPI will thus read signal data directly as
       * it was written by the sender (SCI sender is other node, Shared
       * memory sender is other process and TCP/IP sender is the OS that
       * writes the TCP/IP message into a message buffer).
       */
      NdbApiSignal tmpSignal(*header);
      NdbApiSignal * tSignal = &tmpSignal;
      tSignal->setDataPtr(theData);
      (* oe.m_executeFunction) (oe.m_object, tSignal, ptr);
    }//if
  } else if (tRecBlockNo == API_PACKED) {
    /**
     * Block number == 2047 is used to signal a signal that consists of
     * multiple instances of the same signal. This is an effort to
     * package the signals so as to avoid unnecessary communication
     * overhead since TCP/IP has a great performance impact.
     */
    Uint32 Tlength = header->theLength;
    Uint32 Tsent = 0;
    /**
     * Since it contains at least two data packets we will first
     * copy the signal data to safe place.
     */
    while (Tsent < Tlength) {
      Uint32 Theader = theData[Tsent];
      Tsent++;
      Uint32 TpacketLen = (Theader & 0x1F) + 3;
      tRecBlockNo = Theader >> 16;
      if (TpacketLen <= 25) {
	if ((TpacketLen + Tsent) <= Tlength) {
	  /**
	   * Set the data length of the signal and the receivers block
	   * reference and then call the API.
	   */
	  header->theLength = TpacketLen;
	  header->theReceiversBlockNumber = tRecBlockNo;
	  Uint32* tDataPtr = &theData[Tsent];
	  Tsent += TpacketLen;
	  if (tRecBlockNo >= MIN_API_BLOCK_NO) {
	    oe = m_threads.get(tRecBlockNo);
	    if(oe.m_object != 0 && oe.m_executeFunction != 0){
	      NdbApiSignal tmpSignal(*header);
	      NdbApiSignal * tSignal = &tmpSignal;
	      tSignal->setDataPtr(tDataPtr);
	      (*oe.m_executeFunction)(oe.m_object, tSignal, 0);
	    }
	  }
	}
      }
    }
    return;
  } else if (tRecBlockNo == API_CLUSTERMGR) {
     /**
      * The signal was aimed for the Cluster Manager. 
      * We handle it immediately here.
      */     
     ClusterMgr * clusterMgr = theClusterMgr;
     const Uint32 gsn = header->theVerId_signalNumber;

     switch (gsn){
     case GSN_API_REGREQ:
       clusterMgr->execAPI_REGREQ(theData);
       break;

     case GSN_API_REGCONF:
       clusterMgr->execAPI_REGCONF(theData);
       break;
     
     case GSN_API_REGREF:
       clusterMgr->execAPI_REGREF(theData);
       break;

     case GSN_NODE_FAILREP:
       clusterMgr->execNODE_FAILREP(theData);
       break;
       
     case GSN_NF_COMPLETEREP:
       clusterMgr->execNF_COMPLETEREP(theData);
       break;

     case GSN_ARBIT_STARTREQ:
       if (theArbitMgr != NULL)
	 theArbitMgr->doStart(theData);
       break;
       
     case GSN_ARBIT_CHOOSEREQ:
       if (theArbitMgr != NULL)
	 theArbitMgr->doChoose(theData);
       break;
       
     case GSN_ARBIT_STOPORD:
       if(theArbitMgr != NULL)
	 theArbitMgr->doStop(theData);
       break;

     case GSN_ALTER_TABLE_REP:
     {
       if (m_globalDictCache == NULL)
         break;
       const AlterTableRep* rep = (const AlterTableRep*)theData;
       m_globalDictCache->lock();
       m_globalDictCache->
	 alter_table_rep((const char*)ptr[0].p, 
			 rep->tableId,
			 rep->tableVersion,
			 rep->changeType == AlterTableRep::CT_ALTERED);
       m_globalDictCache->unlock();
       break;
     }
     case GSN_SUB_GCP_COMPLETE_REP:
     {
       /**
	* Report
	*/
       NdbApiSignal tSignal(* header);
       tSignal.setDataPtr(theData);
       for_each(&tSignal, ptr);

       /**
	* Reply
	*/
       {
	 Uint32* send= tSignal.getDataPtrSend();
	 memcpy(send, theData, tSignal.getLength() << 2);
	 ((SubGcpCompleteAck*)send)->rep.senderRef = 
	   numberToRef(API_CLUSTERMGR, theOwnId);
	 Uint32 ref= header->theSendersBlockRef;
	 Uint32 aNodeId= refToNode(ref);
	 tSignal.theReceiversBlockNumber= refToBlock(ref);
	 tSignal.theVerId_signalNumber= GSN_SUB_GCP_COMPLETE_ACK;
	 sendSignalUnCond(&tSignal, aNodeId);
       }
       break;
     }
     default:
       break;
       
     }
     return;
  } else {
    ; // Ignore all other block numbers.
    if(header->theVerId_signalNumber != GSN_API_REGREQ) {
      TRP_DEBUG( "TransporterFacade received signal to unknown block no." );
      ndbout << "BLOCK NO: "  << tRecBlockNo << " sig " 
	     << header->theVerId_signalNumber  << endl;
      abort();
    }
  }
}

// These symbols are needed, but not used in the API
void 
SignalLoggerManager::printSegmentedSection(FILE *, const SignalHeader &,
					   const SegmentedSectionPtr ptr[3],
					   unsigned i){
  abort();
}

void 
copy(Uint32 * & insertPtr, 
     class SectionSegmentPool & thePool, const SegmentedSectionPtr & _ptr){
  abort();
}

/**
 * Note that this function needs no locking since it is
 * only called from the constructor of Ndb (the NdbObject)
 * 
 * Which is protected by a mutex
 */

int
TransporterFacade::start_instance(int nodeId, 
				  const ndb_mgm_configuration* props)
{
  if (! init(nodeId, props)) {
    return -1;
  }
  
  /**
   * Install signal handler for SIGPIPE
   *
   * This due to the fact that a socket connection might have
   * been closed in between a select and a corresponding send
   */
#if !defined NDB_WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  return 0;
}

/**
 * Note that this function need no locking since its
 * only called from the destructor of Ndb (the NdbObject)
 * 
 * Which is protected by a mutex
 */
void
TransporterFacade::stop_instance(){
  DBUG_ENTER("TransporterFacade::stop_instance");
  doStop();
  DBUG_VOID_RETURN;
}

void
TransporterFacade::doStop(){
  DBUG_ENTER("TransporterFacade::doStop");
  /**
   * First stop the ClusterMgr because it needs to send one more signal
   * and also uses theFacadeInstance to lock/unlock theMutexPtr
   */
  if (theClusterMgr != NULL) theClusterMgr->doStop();
  if (theArbitMgr != NULL) theArbitMgr->doStop(NULL);
  
  /**
   * Now stop the send and receive threads
   */
  void *status;
  theStopReceive = 1;
  if (theReceiveThread) {
    NdbThread_WaitFor(theReceiveThread, &status);
    NdbThread_Destroy(&theReceiveThread);
  }
  if (theSendThread) {
    NdbThread_WaitFor(theSendThread, &status);
    NdbThread_Destroy(&theSendThread);
  }
  DBUG_VOID_RETURN;
}

extern "C" 
void* 
runSendRequest_C(void * me)
{
  ((TransporterFacade*) me)->threadMainSend();
  return 0;
}

void TransporterFacade::threadMainSend(void)
{
  theTransporterRegistry->startSending();
  if (!theTransporterRegistry->start_clients()){
    ndbout_c("Unable to start theTransporterRegistry->start_clients");
    exit(0);
  }

  m_socket_server.startServer();

  while(!theStopReceive) {
    NdbSleep_MilliSleep(10);
    NdbMutex_Lock(theMutexPtr);
    if (sendPerformedLastInterval == 0) {
      theTransporterRegistry->performSend();
    }
    sendPerformedLastInterval = 0;
    NdbMutex_Unlock(theMutexPtr);
  }
  theTransporterRegistry->stopSending();

  m_socket_server.stopServer();
  m_socket_server.stopSessions(true);

  theTransporterRegistry->stop_clients();
}

extern "C" 
void* 
runReceiveResponse_C(void * me)
{
  ((TransporterFacade*) me)->threadMainReceive();
  return 0;
}

/*
  The receiver thread is changed to only wake up once every 10 milliseconds
  to poll. It will first check that nobody owns the poll "right" before
  polling. This means that methods using the receiveResponse and
  sendRecSignal will have a slightly longer response time if they are
  executed without any parallel key lookups. Currently also scans are
  affected but this is to be fixed.
*/
void TransporterFacade::threadMainReceive(void)
{
  theTransporterRegistry->startReceiving();
#ifdef NDB_SHM_TRANSPORTER
  NdbThread_set_shm_sigmask(TRUE);
#endif
  NdbMutex_Lock(theMutexPtr);
  theTransporterRegistry->update_connections();
  NdbMutex_Unlock(theMutexPtr);
  while(!theStopReceive) {
    for(int i = 0; i<10; i++){
      NdbSleep_MilliSleep(10);
      NdbMutex_Lock(theMutexPtr);
      if (poll_owner == NULL) {
        const int res = theTransporterRegistry->pollReceive(0);
        if(res > 0)
          theTransporterRegistry->performReceive();
      }
      NdbMutex_Unlock(theMutexPtr);
    }
    NdbMutex_Lock(theMutexPtr);
    theTransporterRegistry->update_connections();
    NdbMutex_Unlock(theMutexPtr);
  }//while
  theTransporterRegistry->stopReceiving();
}
/*
  This method is called by worker thread that owns the poll "rights".
  It waits for events and if something arrives it takes care of it
  and returns to caller. It will quickly come back here if not all
  data was received for the worker thread.
*/
void TransporterFacade::external_poll(Uint32 wait_time)
{
  NdbMutex_Unlock(theMutexPtr);
  const int res = theTransporterRegistry->pollReceive(wait_time);
  NdbMutex_Lock(theMutexPtr);
  if (res > 0) {
    theTransporterRegistry->performReceive();
  }
}

/*
  This Ndb object didn't get hold of the poll "right" and will wait on a
  conditional mutex wait instead. It is put into the conditional wait
  queue so that it is accessible to take over the poll "right" if needed.
  The method gets a free entry in the free list and puts it first in the
  doubly linked list. Finally it assigns the ndb object reference to the
  entry.
*/
Uint32 TransporterFacade::put_in_cond_wait_queue(NdbWaiter *aWaiter)
{
  /*
   Get first free entry
  */
  Uint32 index = first_free_cond_wait;
  assert(index < MAX_NO_THREADS);
  first_free_cond_wait = cond_wait_array[index].next_cond_wait;

  /*
   Put in doubly linked list
  */
  cond_wait_array[index].next_cond_wait = MAX_NO_THREADS;
  cond_wait_array[index].prev_cond_wait = last_in_cond_wait;
  if (last_in_cond_wait == MAX_NO_THREADS) {
    first_in_cond_wait = index;
  } else
    cond_wait_array[last_in_cond_wait].next_cond_wait = index;
  last_in_cond_wait = index;

  cond_wait_array[index].cond_wait_object = aWaiter;
  aWaiter->set_cond_wait_index(index);
  return index;
}

/*
  Somebody is about to signal the thread to wake it up, it could also
  be that it woke up on a timeout and found himself still in the list.
  Removes the entry from the doubly linked list.
  Inserts the entry into the free list.
  NULLifies the ndb object reference entry and sets the index in the
  Ndb object to NIL (=MAX_NO_THREADS)
*/
void TransporterFacade::remove_from_cond_wait_queue(NdbWaiter *aWaiter)
{
  Uint32 index = aWaiter->get_cond_wait_index();
  assert(index < MAX_NO_THREADS &&
         cond_wait_array[index].cond_wait_object == aWaiter);
  /*
   Remove from doubly linked list
  */
  Uint32 prev_elem, next_elem;
  prev_elem = cond_wait_array[index].prev_cond_wait;
  next_elem = cond_wait_array[index].next_cond_wait;
  if (prev_elem != MAX_NO_THREADS)
    cond_wait_array[prev_elem].next_cond_wait = next_elem;
  else
    first_in_cond_wait = next_elem;
  if (next_elem != MAX_NO_THREADS)
    cond_wait_array[next_elem].prev_cond_wait = prev_elem;
  else
    last_in_cond_wait = prev_elem;
  /*
   Insert into free list
  */
  cond_wait_array[index].next_cond_wait = first_free_cond_wait;
  cond_wait_array[index].prev_cond_wait = MAX_NO_THREADS;
  first_free_cond_wait = index;

  cond_wait_array[index].cond_wait_object = NULL;
  aWaiter->set_cond_wait_index(MAX_NO_THREADS);
}

/*
  Get the latest Ndb object from the conditional wait queue
  and also remove it from the list.
*/
NdbWaiter* TransporterFacade::rem_last_from_cond_wait_queue()
{
  NdbWaiter *tWaiter;
  Uint32 index = last_in_cond_wait;
  if (last_in_cond_wait == MAX_NO_THREADS)
    return NULL;
  tWaiter = cond_wait_array[index].cond_wait_object;
  remove_from_cond_wait_queue(tWaiter);
  return tWaiter;
}

void TransporterFacade::init_cond_wait_queue()
{
  Uint32 i;
  /*
   Initialise the doubly linked list as empty
  */
  first_in_cond_wait = MAX_NO_THREADS;
  last_in_cond_wait = MAX_NO_THREADS;
  /*
   Initialise free list
  */
  first_free_cond_wait = 0;
  for (i = 0; i < MAX_NO_THREADS; i++) {
    cond_wait_array[i].cond_wait_object = NULL;
    cond_wait_array[i].next_cond_wait = i+1;
    cond_wait_array[i].prev_cond_wait = MAX_NO_THREADS;
  }
}

TransporterFacade::TransporterFacade(GlobalDictCache *cache) :
  theTransporterRegistry(0),
  theStopReceive(0),
  theSendThread(NULL),
  theReceiveThread(NULL),
  m_fragmented_signal_id(0),
  m_globalDictCache(cache)
{
  DBUG_ENTER("TransporterFacade::TransporterFacade");
  init_cond_wait_queue();
  poll_owner = NULL;
  theOwnId = 0;
  theMutexPtr = NdbMutex_Create();
  sendPerformedLastInterval = 0;

  checkCounter = 4;
  currentSendLimit = 1;
  theClusterMgr = NULL;
  theArbitMgr = NULL;
  theStartNodeId = 1;
  m_scan_batch_size= MAX_SCAN_BATCH_SIZE;
  m_batch_byte_size= SCAN_BATCH_SIZE;
  m_batch_size= DEF_BATCH_SIZE;
  m_max_trans_id = 0;

  theClusterMgr = new ClusterMgr(* this);

#ifdef API_TRACE
  apiSignalLog = 0;
#endif
  DBUG_VOID_RETURN;
}

bool
TransporterFacade::init(Uint32 nodeId, const ndb_mgm_configuration* props)
{
  DBUG_ENTER("TransporterFacade::init");

  theOwnId = nodeId;
  theTransporterRegistry = new TransporterRegistry(this);

  const int res = IPCConfig::configureTransporters(nodeId, 
						   * props, 
						   * theTransporterRegistry);
  if(res <= 0){
    TRP_DEBUG( "configureTransporters returned 0 or less" );
    DBUG_RETURN(false);
  }
  
  ndb_mgm_configuration_iterator iter(* props, CFG_SECTION_NODE);
  iter.first();
  theClusterMgr->init(iter);
  
  iter.first();
  if(iter.find(CFG_NODE_ID, nodeId)){
    TRP_DEBUG( "Node info missing from config." );
    DBUG_RETURN(false);
  }
  
  Uint32 total_send_buffer = 0;
  if(iter.get(CFG_TOTAL_SEND_BUFFER_MEMORY, &total_send_buffer) ||
     total_send_buffer == 0)
  {
    total_send_buffer = theTransporterRegistry->get_total_max_send_buffer();
  }
  theTransporterRegistry->allocate_send_buffers(total_send_buffer);

  Uint32 rank = 0;
  if(!iter.get(CFG_NODE_ARBIT_RANK, &rank) && rank>0){
    theArbitMgr = new ArbitMgr(* this);
    theArbitMgr->setRank(rank);
    Uint32 delay = 0;
    iter.get(CFG_NODE_ARBIT_DELAY, &delay);
    theArbitMgr->setDelay(delay);
  }
  Uint32 scan_batch_size= 0;
  if (!iter.get(CFG_MAX_SCAN_BATCH_SIZE, &scan_batch_size)) {
    m_scan_batch_size= scan_batch_size;
  }
  Uint32 batch_byte_size= 0;
  if (!iter.get(CFG_BATCH_BYTE_SIZE, &batch_byte_size)) {
    m_batch_byte_size= batch_byte_size;
  }
  Uint32 batch_size= 0;
  if (!iter.get(CFG_BATCH_SIZE, &batch_size)) {
    m_batch_size= batch_size;
  }
  
  Uint32 timeout = 120000;
  iter.first();
  for (iter.first(); iter.valid(); iter.next())
  {
    Uint32 tmp1 = 0, tmp2 = 0;
    iter.get(CFG_DB_TRANSACTION_CHECK_INTERVAL, &tmp1);
    iter.get(CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT, &tmp2);
    tmp1 += tmp2;
    if (tmp1 > timeout)
      timeout = tmp1;
  }
  m_waitfor_timeout = timeout;
  
  if (!theTransporterRegistry->start_service(m_socket_server)){
    ndbout_c("Unable to start theTransporterRegistry->start_service");
    DBUG_RETURN(false);
  }

  theReceiveThread = NdbThread_Create(runReceiveResponse_C,
                                      (void**)this,
                                      32768,
                                      "ndb_receive",
                                      NDB_THREAD_PRIO_LOW);

  theSendThread = NdbThread_Create(runSendRequest_C,
                                   (void**)this,
                                   32768,
                                   "ndb_send",
                                   NDB_THREAD_PRIO_LOW);
  theClusterMgr->startThread();
  
#ifdef API_TRACE
  signalLogger.logOn(true, 0, SignalLoggerManager::LogInOut);
#endif
  
  DBUG_RETURN(true);
}

void
TransporterFacade::for_each(NdbApiSignal* aSignal, LinearSectionPtr ptr[3])
{
  DBUG_ENTER("TransporterFacade::for_each");
  Uint32 sz = m_threads.m_statusNext.size();
  TransporterFacade::ThreadData::Object_Execute oe; 
  for (Uint32 i = 0; i < sz ; i ++) 
  {
    oe = m_threads.m_objectExecute[i];
    if (m_threads.getInUse(i))
    {
      (* oe.m_executeFunction) (oe.m_object, aSignal, ptr);
    }
  }
  DBUG_VOID_RETURN;
}

void
TransporterFacade::connected()
{
  DBUG_ENTER("TransporterFacade::connected");
  Uint32 sz = m_threads.m_statusNext.size();
  for (Uint32 i = 0; i < sz ; i ++) {
    if (m_threads.getInUse(i)){
      void * obj = m_threads.m_objectExecute[i].m_object;
      NodeStatusFunction RegPC = m_threads.m_statusFunction[i];
      (*RegPC) (obj, numberToRef(indexToNumber(i), theOwnId), true, true);
    }
  }
  DBUG_VOID_RETURN;
}

void
TransporterFacade::ReportNodeDead(NodeId tNodeId)
{
  DBUG_ENTER("TransporterFacade::ReportNodeDead");
  DBUG_PRINT("enter",("nodeid= %d", tNodeId));
  /**
   * When a node fails we must report this to each Ndb object. 
   * The function that is used for communicating node failures is called.
   * This is to ensure that the Ndb objects do not think their connections 
   * are correct after a failure followed by a restart. 
   * After the restart the node is up again and the Ndb object 
   * might not have noticed the failure.
   */
  Uint32 sz = m_threads.m_statusNext.size();
  for (Uint32 i = 0; i < sz ; i ++) {
    if (m_threads.getInUse(i)){
      void * obj = m_threads.m_objectExecute[i].m_object;
      NodeStatusFunction RegPC = m_threads.m_statusFunction[i];
      (*RegPC) (obj, tNodeId, false, false);
    }
  }
  DBUG_VOID_RETURN;
}

void
TransporterFacade::ReportNodeFailureComplete(NodeId tNodeId)
{
  /**
   * When a node fails we must report this to each Ndb object. 
   * The function that is used for communicating node failures is called.
   * This is to ensure that the Ndb objects do not think their connections 
   * are correct after a failure followed by a restart. 
   * After the restart the node is up again and the Ndb object 
   * might not have noticed the failure.
   */

  DBUG_ENTER("TransporterFacade::ReportNodeFailureComplete");
  DBUG_PRINT("enter",("nodeid= %d", tNodeId));
  Uint32 sz = m_threads.m_statusNext.size();
  for (Uint32 i = 0; i < sz ; i ++) {
    if (m_threads.getInUse(i)){
      void * obj = m_threads.m_objectExecute[i].m_object;
      NodeStatusFunction RegPC = m_threads.m_statusFunction[i];
      (*RegPC) (obj, tNodeId, false, true);
    }
  }
  DBUG_VOID_RETURN;
}

void
TransporterFacade::ReportNodeAlive(NodeId tNodeId)
{
  /**
   * When a node fails we must report this to each Ndb object. 
   * The function that is used for communicating node failures is called.
   * This is to ensure that the Ndb objects do not think there connections 
   * are correct after a failure
   * followed by a restart. 
   * After the restart the node is up again and the Ndb object 
   * might not have noticed the failure.
   */
  Uint32 sz = m_threads.m_statusNext.size();
  for (Uint32 i = 0; i < sz ; i ++) {
    if (m_threads.getInUse(i)){
      void * obj = m_threads.m_objectExecute[i].m_object;
      NodeStatusFunction RegPC = m_threads.m_statusFunction[i];
      (*RegPC) (obj, tNodeId, true, false);
    }
  }
}

int 
TransporterFacade::close(BlockNumber blockNumber, Uint64 trans_id)
{
  NdbMutex_Lock(theMutexPtr);
  Uint32 low_bits = (Uint32)trans_id;
  m_max_trans_id = m_max_trans_id > low_bits ? m_max_trans_id : low_bits;
  close_local(blockNumber);
  NdbMutex_Unlock(theMutexPtr);
  return 0;
}

int 
TransporterFacade::close_local(BlockNumber blockNumber){
  m_threads.close(blockNumber);
  return 0;
}

int
TransporterFacade::open(void* objRef, 
                        ExecuteFunction fun, 
                        NodeStatusFunction statusFun,
                        int blockNo)
{
  DBUG_ENTER("TransporterFacade::open");
  int r= m_threads.open(objRef, fun, statusFun, blockNo);
  if (r < 0)
    DBUG_RETURN(r);
#if 1
  if (theOwnId > 0) {
    (*statusFun)(objRef, numberToRef(r, theOwnId), true, true);
  }
#endif
  DBUG_RETURN(r);
}

TransporterFacade::~TransporterFacade()
{  
  DBUG_ENTER("TransporterFacade::~TransporterFacade");

  NdbMutex_Lock(theMutexPtr);
  delete theClusterMgr;  
  delete theArbitMgr;
  delete theTransporterRegistry;
  NdbMutex_Unlock(theMutexPtr);
  NdbMutex_Destroy(theMutexPtr);
#ifdef API_TRACE
  signalLogger.setOutputStream(0);
#endif
  DBUG_VOID_RETURN;
}

void 
TransporterFacade::calculateSendLimit()
{
  Uint32 Ti;
  Uint32 TthreadCount = 0;
  
  Uint32 sz = m_threads.m_statusNext.size();
  for (Ti = 0; Ti < sz; Ti++) {
    if (m_threads.m_statusNext[Ti] == (ThreadData::ACTIVE)){
      TthreadCount++;
      m_threads.m_statusNext[Ti] = ThreadData::INACTIVE;
    }
  }
  currentSendLimit = TthreadCount;
  if (currentSendLimit == 0) {
    currentSendLimit = 1;
  }
  checkCounter = currentSendLimit << 2;
}


//-------------------------------------------------
// Force sending but still report the sending to the
// adaptive algorithm.
//-------------------------------------------------
void TransporterFacade::forceSend(Uint32 block_number) {
  checkCounter--;
  m_threads.m_statusNext[numberToIndex(block_number)] = ThreadData::ACTIVE;
  sendPerformedLastInterval = 1;
  if (checkCounter < 0) {
    calculateSendLimit();
  }
  theTransporterRegistry->forceSendCheck(0);
}

//-------------------------------------------------
// Improving API performance
//-------------------------------------------------
void
TransporterFacade::checkForceSend(Uint32 block_number) {  
  m_threads.m_statusNext[numberToIndex(block_number)] = ThreadData::ACTIVE;
  //-------------------------------------------------
  // This code is an adaptive algorithm to discover when
  // the API should actually send its buffers. The reason
  // is that the performance is highly dependent on the
  // size of the writes over the communication network.
  // Thus we try to ensure that the send size is as big
  // as possible. At the same time we don't want response
  // time to increase so therefore we have to keep track of
  // how the users are performing adaptively.
  //-------------------------------------------------
  
  if (theTransporterRegistry->forceSendCheck(currentSendLimit) == 1) {
    sendPerformedLastInterval = 1;
  }
  checkCounter--;
  if (checkCounter < 0) {
    calculateSendLimit();
  }
}


/******************************************************************************
 * SEND SIGNAL METHODS
 *****************************************************************************/
int
TransporterFacade::sendSignal(NdbApiSignal * aSignal, NodeId aNode){
  Uint32* tDataPtr = aSignal->getDataPtrSend();
  Uint32 Tlen = aSignal->theLength;
  Uint32 TBno = aSignal->theReceiversBlockNumber;
  if(getIsNodeSendable(aNode) == true){
#ifdef API_TRACE
    if(setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)){
      Uint32 tmp = aSignal->theSendersBlockRef;
      aSignal->theSendersBlockRef = numberToRef(tmp, theOwnId);
      LinearSectionPtr ptr[3];
      signalLogger.sendSignal(* aSignal,
			      1,
			      tDataPtr,
			      aNode, ptr, 0);
      signalLogger.flushSignalLog();
      aSignal->theSendersBlockRef = tmp;
    }
#endif
    if ((Tlen != 0) && (Tlen <= 25) && (TBno != 0)) {
      SendStatus ss = theTransporterRegistry->prepareSend(aSignal, 
							  1, // JBB
							  tDataPtr, 
							  aNode, 
							  (LinearSectionPtr*)0);
      //if (ss != SEND_OK) ndbout << ss << endl;
      return (ss == SEND_OK ? 0 : -1);
    } else {
      ndbout << "ERR: SigLen = " << Tlen << " BlockRec = " << TBno;
      ndbout << " SignalNo = " << aSignal->theVerId_signalNumber << endl;
      assert(0);
    }//if
  }
  //const ClusterMgr::Node & node = theClusterMgr->getNodeInfo(aNode);
  //const Uint32 startLevel = node.m_state.startLevel;
  return -1; // Node Dead
}

int
TransporterFacade::sendSignalUnCond(NdbApiSignal * aSignal, 
                                    NodeId aNode,
                                    Uint32 prio){
  Uint32* tDataPtr = aSignal->getDataPtrSend();
  assert(prio <= 1);
#ifdef API_TRACE
  if(setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)){
    Uint32 tmp = aSignal->theSendersBlockRef;
    aSignal->theSendersBlockRef = numberToRef(tmp, theOwnId);
    LinearSectionPtr ptr[3];
    signalLogger.sendSignal(* aSignal,
			    prio,
			    tDataPtr,
			    aNode, ptr, 0);
    signalLogger.flushSignalLog();
    aSignal->theSendersBlockRef = tmp;
  }
#endif
  assert((aSignal->theLength != 0) &&
         (aSignal->theLength <= 25) &&
         (aSignal->theReceiversBlockNumber != 0));
  SendStatus ss = theTransporterRegistry->prepareSend(aSignal, 
						      prio, 
						      tDataPtr,
						      aNode, 
						      (LinearSectionPtr*)0);
  
  return (ss == SEND_OK ? 0 : -1);
}

/**
 * FragmentedSectionIterator
 * -------------------------
 * This class acts as an adapter to a GenericSectionIterator
 * instance, providing a sub-range iterator interface.
 * It is used when long sections of a signal are fragmented
 * across multiple actual signals - the user-supplied
 * GenericSectionIterator is then adapted into a
 * GenericSectionIterator that only returns a subset of
 * the contained words for each signal fragment.
 */
class FragmentedSectionIterator: public GenericSectionIterator
{
private :
  GenericSectionIterator* realIterator; /* Real underlying iterator */
  Uint32 realIterWords;                 /* Total size of underlying */
  Uint32 realCurrPos;                   /* Current pos in underlying */
  Uint32 rangeStart;                    /* Sub range start in underlying */
  Uint32 rangeLen;                      /* Sub range len in underlying */
  Uint32 rangeRemain;                   /* Remaining words in underlying */
  Uint32* lastReadPtr;                  /* Ptr to last chunk obtained from
                                         * underlying */
  Uint32 lastReadPtrLen;                /* Remaining words in last chunk
                                         * obtained from underlying */
public:
  /* Constructor
   * The instance is constructed with the sub-range set to be the
   * full range of the underlying iterator
   */
  FragmentedSectionIterator(GenericSectionPtr ptr)
  {
    realIterator= ptr.sectionIter;
    realIterWords= ptr.sz;
    realCurrPos= 0;
    rangeStart= 0;
    rangeLen= rangeRemain= realIterWords;
    lastReadPtr= NULL;
    lastReadPtrLen= 0;
    moveToPos(0);

    assert(checkInvariants());
  }

private:
  /** 
   * checkInvariants
   * These class invariants must hold true at all stable states
   * of the iterator
   */
  bool checkInvariants()
  {
    assert( (realIterator != NULL) || (realIterWords == 0) );
    assert( realCurrPos <= realIterWords );
    assert( rangeStart <= realIterWords );
    assert( (rangeStart+rangeLen) <= realIterWords);
    assert( rangeRemain <= rangeLen );
    
    /* Can only have a null readptr if nothing is left */
    assert( (lastReadPtr != NULL) || (rangeRemain == 0));

    /* If we have a non-null readptr and some remaining 
     * words the readptr must have some words
     */
    assert( (lastReadPtr == NULL) || 
            ((rangeRemain == 0) || (lastReadPtrLen != 0)));
    return true;
  }

  /**
   * moveToPos
   * This method is used when the iterator is reset(), to move
   * to the start of the current sub-range.
   * If the iterator is already in-position then this is efficient
   * Otherwise, it has to reset() the underling iterator and
   * advance it until the start position is reached.
   */
  void moveToPos(Uint32 pos)
  {
    assert(pos <= realIterWords);

    if (pos < realCurrPos)
    {
      /* Need to reset, and advance from the start */
      realIterator->reset();
      realCurrPos= 0;
      lastReadPtr= NULL;
      lastReadPtrLen= 0;
    }

    if ((lastReadPtr == NULL) && 
        (realIterWords != 0) &&
        (pos != realIterWords))
      lastReadPtr= realIterator->getNextWords(lastReadPtrLen);
    
    if (pos == realCurrPos)
      return;

    /* Advance until we get a chunk which contains the pos */
    while (pos >= realCurrPos + lastReadPtrLen)
    {
      realCurrPos+= lastReadPtrLen;
      lastReadPtr= realIterator->getNextWords(lastReadPtrLen);
      assert(lastReadPtr != NULL);
    }

    const Uint32 chunkOffset= pos - realCurrPos;
    lastReadPtr+= chunkOffset;
    lastReadPtrLen-= chunkOffset;
    realCurrPos= pos;
  }

public:
  /**
   * setRange
   * Set the sub-range of the iterator.  Must be within the
   * bounds of the underlying iterator
   * After the range is set, the iterator is reset() to the
   * start of the supplied subrange
   */
  bool setRange(Uint32 start, Uint32 len)
  {
    assert(checkInvariants());
    if (start+len > realIterWords)
      return false;
    moveToPos(start);
    
    rangeStart= start;
    rangeLen= rangeRemain= len;

    assert(checkInvariants());
    return true;
  }

  /**
   * reset
   * (GenericSectionIterator)
   * Reset the iterator to the start of the current sub-range
   * Avoid calling as it could be expensive.
   */
  void reset()
  {
    /* Reset iterator to last specified range */
    assert(checkInvariants());
    moveToPos(rangeStart);
    rangeRemain= rangeLen;
    assert(checkInvariants());
  }

  /**
   * getNextWords
   * (GenericSectionIterator)
   * Get ptr and size of next contiguous words in subrange
   */
  Uint32* getNextWords(Uint32& sz)
  {
    assert(checkInvariants());
    Uint32* currPtr= NULL;

    if (rangeRemain)
    {
      assert(lastReadPtr != NULL);
      assert(lastReadPtrLen != 0);
      currPtr= lastReadPtr;
      
      sz= MIN(rangeRemain, lastReadPtrLen);
      
      if (sz == lastReadPtrLen)
        /* Will return everything in this chunk, move iterator to 
         * next
         */
        lastReadPtr= realIterator->getNextWords(lastReadPtrLen);
      else
      {
        /* Not returning all of this chunk, just advance within it */
        lastReadPtr+= sz;
        lastReadPtrLen-= sz;
      }
      realCurrPos+= sz;
      rangeRemain-= sz;
    }
    else
    {
      sz= 0;
    }
    
    assert(checkInvariants());
    return currPtr;
  }
};

/* Max fragmented signal chunk size (words) is max round number 
 * of NDB_SECTION_SEGMENT_SZ words with some slack left for 'main'
 * part of signal etc.
 */
#define CHUNK_SZ ((((MAX_SEND_MESSAGE_BYTESIZE >> 2) / NDB_SECTION_SEGMENT_SZ) - 2 ) \
                  * NDB_SECTION_SEGMENT_SZ)

/**
 * sendFragmentedSignal (GenericSectionPtr variant)
 * ------------------------------------------------
 * This method will send a signal with attached long sections.  If 
 * the signal is longer than CHUNK_SZ, the signal will be split into
 * multiple CHUNK_SZ fragments.
 * 
 * This is done by sending two or more long signals(fragments), with the
 * original GSN, but different signal data and with as much of the long 
 * sections as will fit in each.
 *
 * Non-final fragment signals contain a fraginfo value in the header
 * (1= first fragment, 2= intermediate fragment, 3= final fragment)
 * 
 * Fragment signals contain additional words in their signals :
 *   1..n words Mapping section numbers in fragment signal to original 
 *              signal section numbers
 *   1 word     Fragmented signal unique id.
 * 
 * Non final fragments (fraginfo=1/2) only have this data in them.  Final
 * fragments have this data in addition to the normal signal data.
 * 
 * Each fragment signal can transport one or more long sections, starting 
 * with section 0.  Sections are always split on NDB_SECTION_SEGMENT_SZ word
 * boundaries to simplify reassembly in the kernel.
 */
int
TransporterFacade::sendFragmentedSignal(NdbApiSignal* aSignal, NodeId aNode, 
					GenericSectionPtr ptr[3], Uint32 secs)
{
  unsigned i;
  Uint32 totalSectionLength= 0;
  for (i= 0; i < secs; i++)
    totalSectionLength+= ptr[i].sz;
  
  /* If there's no need to fragment, send normally */
  if (totalSectionLength <= CHUNK_SZ)
    return sendSignal(aSignal, aNode, ptr, secs);
  
  /* We will fragment */
  if(getIsNodeSendable(aNode) != true)
    return -1;

  // TODO : Consider tracing fragment signals?
#ifdef API_TRACE
  if(setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)){
    Uint32 tmp = aSignal->theSendersBlockRef;
    aSignal->theSendersBlockRef = numberToRef(tmp, theOwnId);
    signalLogger.sendSignal(* aSignal,
			    1,
			    aSignal->getDataPtrSend(),
			    aNode,
			    ptr, secs);
    aSignal->theSendersBlockRef = tmp;
    /* Reset section iterators */
    for(Uint32 s=0; s < secs; s++)
      ptr[s].sectionIter->reset();
  }
#endif

  NdbApiSignal tmp_signal(*(SignalHeader*)aSignal);
  GenericSectionPtr tmp_ptr[3];
  GenericSectionPtr empty= {0, NULL};
  Uint32 unique_id= m_fragmented_signal_id++; // next unique id
  
  /* Init tmp_ptr array from ptr[] array, make sure we have
   * 0 length for missing sections
   */
  for (i= 0; i < 3; i++)
    tmp_ptr[i]= (i < secs)? ptr[i] : empty;

  /* Create our section iterator adapters */
  FragmentedSectionIterator sec0(tmp_ptr[0]);
  FragmentedSectionIterator sec1(tmp_ptr[1]);
  FragmentedSectionIterator sec2(tmp_ptr[2]);

  /* Replace caller's iterators with ours */
  tmp_ptr[0].sectionIter= &sec0;
  tmp_ptr[1].sectionIter= &sec1;
  tmp_ptr[2].sectionIter= &sec2;

  unsigned start_i= 0;
  unsigned this_chunk_sz= 0;
  unsigned fragment_info= 0;
  Uint32 *tmp_signal_data= tmp_signal.getDataPtrSend();
  for (i= 0; i < secs;) {
    unsigned remaining_sec_sz= tmp_ptr[i].sz;
    tmp_signal_data[i-start_i]= i;
    if (this_chunk_sz + remaining_sec_sz <= CHUNK_SZ)
    {
      /* This section fits whole, move onto next */
      this_chunk_sz+= remaining_sec_sz;
      i++;
    }
    else
    {
      /* This section doesn't fit, truncate it */
      unsigned send_sz= CHUNK_SZ - this_chunk_sz;
      if (i != start_i)
      {
        /* We ensure that the first piece of a new section which is
         * being truncated is a multiple of NDB_SECTION_SEGMENT_SZ
         * (to simplify reassembly).  Subsequent non-truncated pieces
         * will be CHUNK_SZ which is a multiple of NDB_SECTION_SEGMENT_SZ
         * The final piece does not need to be a multiple of
         * NDB_SECTION_SEGMENT_SZ
         * 
         * Note that this can push this_chunk_sz above CHUNK_SZ
         * Should probably round-down, but need to be careful of
         * 'can't fit any' cases.  Instead, CHUNK_SZ is defined
         * with some slack below MAX_SENT_MESSAGE_BYTESIZE
         */
	send_sz=
	  NDB_SECTION_SEGMENT_SZ
	  *((send_sz+NDB_SECTION_SEGMENT_SZ-1)
            /NDB_SECTION_SEGMENT_SZ);
        if (send_sz > remaining_sec_sz)
	  send_sz= remaining_sec_sz;
      }

      /* Modify tmp generic section ptr to describe truncated
       * section
       */
      tmp_ptr[i].sz= send_sz;
      FragmentedSectionIterator* fragIter= 
        (FragmentedSectionIterator*) tmp_ptr[i].sectionIter;
      const Uint32 total_sec_sz= ptr[i].sz;
      const Uint32 start= (total_sec_sz - remaining_sec_sz);
      bool ok= fragIter->setRange(start, send_sz);
      assert(ok);
      if (!ok)
        return -1;
      
      if (fragment_info < 2) // 1 = first fragment signal
                             // 2 = middle fragments
	fragment_info++;

      // send tmp_signal
      tmp_signal_data[i-start_i+1]= unique_id;
      tmp_signal.setLength(i-start_i+2);
      tmp_signal.m_fragmentInfo= fragment_info;
      tmp_signal.m_noOfSections= i-start_i+1;
      // do prepare send
      {
	SendStatus ss = theTransporterRegistry->prepareSend
	  (&tmp_signal, 
	   1, /*JBB*/
	   tmp_signal_data,
	   aNode, 
	   &tmp_ptr[start_i]);
	assert(ss != SEND_MESSAGE_TOO_BIG);
	if (ss != SEND_OK) return -1;
      }
      // setup variables for next signal
      start_i= i;
      this_chunk_sz= 0;
      assert(remaining_sec_sz >= send_sz);
      Uint32 remaining= remaining_sec_sz - send_sz;
      tmp_ptr[i].sz= remaining;
      /* Set sub-range iterator to cover remaining words */
      ok= fragIter->setRange(start+send_sz, remaining);
      assert(ok);
      if (!ok)
        return -1;
      
      if (remaining == 0)
        /* This section's done, move onto the next */
	i++;
    }
  }

  unsigned a_sz= aSignal->getLength();

  if (fragment_info > 0) {
    // update the original signal to include section info
    Uint32 *a_data= aSignal->getDataPtrSend();
    unsigned tmp_sz= i-start_i;
    memcpy(a_data+a_sz,
	   tmp_signal_data,
	   tmp_sz*sizeof(Uint32));
    a_data[a_sz+tmp_sz]= unique_id;
    aSignal->setLength(a_sz+tmp_sz+1);

    // send last fragment
    aSignal->m_fragmentInfo= 3; // 3 = last fragment
    aSignal->m_noOfSections= i-start_i;
  } else {
    aSignal->m_noOfSections= secs;
  }

  // send aSignal
  int ret;
  {
    SendStatus ss = theTransporterRegistry->prepareSend
      (aSignal,
       1/*JBB*/,
       aSignal->getDataPtrSend(),
       aNode,
       &tmp_ptr[start_i]);
    assert(ss != SEND_MESSAGE_TOO_BIG);
    ret = (ss == SEND_OK ? 0 : -1);
  }
  aSignal->m_noOfSections = 0;
  aSignal->m_fragmentInfo = 0;
  aSignal->setLength(a_sz);
  return ret;
}

int
TransporterFacade::sendFragmentedSignal(NdbApiSignal* aSignal, NodeId aNode, 
					LinearSectionPtr ptr[3], Uint32 secs)
{
  /* Use the GenericSection variant of sendFragmentedSignal */
  GenericSectionPtr tmpPtr[3];
  LinearSectionPtr linCopy[3];
  const LinearSectionPtr empty= {0, NULL};
  
  /* Make sure all of linCopy is initialised */
  for (Uint32 j=0; j<3; j++)
    linCopy[j]= (j < secs)? ptr[j] : empty;
  
  LinearSectionIterator zero (linCopy[0].p, linCopy[0].sz);
  LinearSectionIterator one  (linCopy[1].p, linCopy[1].sz);
  LinearSectionIterator two  (linCopy[2].p, linCopy[2].sz);

  /* Build GenericSectionPtr array using iterators */
  tmpPtr[0].sz= linCopy[0].sz;
  tmpPtr[0].sectionIter= &zero;
  tmpPtr[1].sz= linCopy[1].sz;
  tmpPtr[1].sectionIter= &one;
  tmpPtr[2].sz= linCopy[2].sz;
  tmpPtr[2].sectionIter= &two;

  return sendFragmentedSignal(aSignal, aNode, tmpPtr, secs);
}
  

int
TransporterFacade::sendSignal(NdbApiSignal* aSignal, NodeId aNode, 
			      LinearSectionPtr ptr[3], Uint32 secs){
  aSignal->m_noOfSections = secs;
  if(getIsNodeSendable(aNode) == true){
#ifdef API_TRACE
    if(setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)){
      Uint32 tmp = aSignal->theSendersBlockRef;
      aSignal->theSendersBlockRef = numberToRef(tmp, theOwnId);
      signalLogger.sendSignal(* aSignal,
			      1,
			      aSignal->getDataPtrSend(),
			      aNode,
                              ptr, secs);
      signalLogger.flushSignalLog();
      aSignal->theSendersBlockRef = tmp;
    }
#endif
    SendStatus ss = theTransporterRegistry->prepareSend
      (aSignal, 
       1, // JBB
       aSignal->getDataPtrSend(),
       aNode, 
       ptr);
    assert(ss != SEND_MESSAGE_TOO_BIG);
    aSignal->m_noOfSections = 0;
    return (ss == SEND_OK ? 0 : -1);
  }
  aSignal->m_noOfSections = 0;
  return -1;
}

int
TransporterFacade::sendSignal(NdbApiSignal* aSignal, NodeId aNode,
                              GenericSectionPtr ptr[3], Uint32 secs){
  aSignal->m_noOfSections = secs;
  if(getIsNodeSendable(aNode) == true){
#ifdef API_TRACE
    if(setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)){
      Uint32 tmp = aSignal->theSendersBlockRef;
      aSignal->theSendersBlockRef = numberToRef(tmp, theOwnId);
      signalLogger.sendSignal(* aSignal,
			      1,
			      aSignal->getDataPtrSend(),
			      aNode,
                              ptr, secs);
      signalLogger.flushSignalLog();
      aSignal->theSendersBlockRef = tmp;
    }
    /* Reset section iterators */
    for(Uint32 s=0; s < secs; s++)
      ptr[s].sectionIter->reset();
#endif
    SendStatus ss = theTransporterRegistry->prepareSend
      (aSignal, 
       1, // JBB
       aSignal->getDataPtrSend(),
       aNode, 
       ptr);
    assert(ss != SEND_MESSAGE_TOO_BIG);
    aSignal->m_noOfSections = 0;
    return (ss == SEND_OK ? 0 : -1);
  }
  aSignal->m_noOfSections = 0;
  return -1;
}

/******************************************************************************
 * CONNECTION METHODS  Etc
 ******************************************************************************/

void
TransporterFacade::doConnect(int aNodeId){
  theTransporterRegistry->setIOState(aNodeId, NoHalt);
  theTransporterRegistry->do_connect(aNodeId);
}

void
TransporterFacade::doDisconnect(int aNodeId)
{
  theTransporterRegistry->do_disconnect(aNodeId);
}

void
TransporterFacade::reportConnected(int aNodeId)
{
  theClusterMgr->reportConnected(aNodeId);
  return;
}

void
TransporterFacade::reportDisconnected(int aNodeId)
{
  theClusterMgr->reportDisconnected(aNodeId);
  return;
}

NodeId
TransporterFacade::ownId() const
{
  return theOwnId;
}

bool
TransporterFacade::isConnected(NodeId aNodeId){
  return theTransporterRegistry->is_connected(aNodeId);
}

NodeId
TransporterFacade::get_an_alive_node()
{
  DBUG_ENTER("TransporterFacade::get_an_alive_node");
  DBUG_PRINT("enter", ("theStartNodeId: %d", theStartNodeId));
#ifdef VM_TRACE
  const char* p = NdbEnv_GetEnv("NDB_ALIVE_NODE_ID", (char*)0, 0);
  if (p != 0 && *p != 0)
    return atoi(p);
#endif
  NodeId i;
  for (i = theStartNodeId; i < MAX_NDB_NODES; i++) {
    if (get_node_alive(i)){
      DBUG_PRINT("info", ("Node %d is alive", i));
      theStartNodeId = ((i + 1) % MAX_NDB_NODES);
      DBUG_RETURN(i);
    }
  }
  for (i = 1; i < theStartNodeId; i++) {
    if (get_node_alive(i)){
      DBUG_PRINT("info", ("Node %d is alive", i));
      theStartNodeId = ((i + 1) % MAX_NDB_NODES);
      DBUG_RETURN(i);
    }
  }
  DBUG_RETURN((NodeId)0);
}

TransporterFacade::ThreadData::ThreadData(Uint32 size){
  m_use_cnt = 0;
  m_firstFree = END_OF_LIST;
  expand(size);
}

void
TransporterFacade::ThreadData::expand(Uint32 size){
  Object_Execute oe = { 0 ,0 };
  NodeStatusFunction fun = 0;

  const Uint32 sz = m_statusNext.size();
  m_objectExecute.fill(sz + size, oe);
  m_statusFunction.fill(sz + size, fun);
  for(Uint32 i = 0; i<size; i++){
    m_statusNext.push_back(sz + i + 1);
  }

  m_statusNext.back() = m_firstFree;
  m_firstFree = m_statusNext.size() - size;
}


int
TransporterFacade::ThreadData::open(void* objRef, 
				    ExecuteFunction fun, 
				    NodeStatusFunction fun2,
                                    int blockNo)
{
  Uint32 nextFree = m_firstFree;

  if(m_statusNext.size() >= MAX_NO_THREADS && nextFree == END_OF_LIST){
    return -1;
  }

  Object_Execute oe = { objRef , fun };

  if (unlikely(blockNo >= 0)) {
    // Open block with fixed number
    Uint32 index= numberToIndex(blockNo);

    if(index > m_statusNext.size()){
      expand(index - m_statusNext.size());
    }

    m_use_cnt++;

    // Single linked free list, relink the previous one that points to this
    for(Uint32 i = 0; i < m_statusNext.size(); i++){
      if (m_statusNext[i] == index){
        m_statusNext[i]= m_statusNext[index];
        break;
      }
    }

    m_statusNext[index] = INACTIVE;
    m_objectExecute[index] = oe;
    m_statusFunction[index] = fun2;

    return indexToNumber(index);
  }

  if(nextFree == END_OF_LIST){
    expand(10);
    nextFree = m_firstFree;
  }
  
  m_use_cnt++;
  m_firstFree = m_statusNext[nextFree];

  m_statusNext[nextFree] = INACTIVE;
  m_objectExecute[nextFree] = oe;
  m_statusFunction[nextFree] = fun2;

  return indexToNumber(nextFree);
}

int
TransporterFacade::ThreadData::close(int number){
  number= numberToIndex(number);
  assert(getInUse(number));
  m_statusNext[number] = m_firstFree;
  assert(m_use_cnt);
  m_use_cnt--;
  m_firstFree = number;
  Object_Execute oe = { 0, 0 };
  m_objectExecute[number] = oe;
  m_statusFunction[number] = 0;
  return 0;
}

Uint32
TransporterFacade::get_active_ndb_objects() const
{
  return m_threads.m_use_cnt;
}

PollGuard::PollGuard(TransporterFacade *tp, NdbWaiter *aWaiter,
                     Uint32 block_no)
{
  m_tp= tp;
  m_waiter= aWaiter;
  m_locked= true;
  m_block_no= block_no;
  tp->lock_mutex();
}

/*
  This is a common routine for possibly forcing the send of buffered signals
  and receiving response the thread is waiting for. It is designed to be
  useful from:
  1) PK, UK lookups using the asynchronous interface
     This routine uses the wait_for_input routine instead since it has
     special end conditions due to the asynchronous nature of its usage.
  2) Scans
  3) dictSignal
  It uses a NdbWaiter object to wait on the events and this object is
  linked into the conditional wait queue. Thus this object contains
  a reference to its place in the queue.

  It replaces the method receiveResponse previously used on the Ndb object
*/
int PollGuard::wait_n_unlock(int wait_time, NodeId nodeId, Uint32 state,
                             bool forceSend)
{
  int ret_val;
  m_waiter->set_node(nodeId);
  m_waiter->set_state(state);
  ret_val= wait_for_input_in_loop(wait_time, forceSend);
  unlock_and_signal();
  return ret_val;
}

int PollGuard::wait_scan(int wait_time, NodeId nodeId, bool forceSend)
{
  m_waiter->set_node(nodeId);
  m_waiter->set_state(WAIT_SCAN);
  return wait_for_input_in_loop(wait_time, forceSend);
}

int PollGuard::wait_for_input_in_loop(int wait_time, bool forceSend)
{
  int ret_val;
  if (forceSend)
    m_tp->forceSend(m_block_no);
  else
    m_tp->checkForceSend(m_block_no);

  NDB_TICKS curr_time = NdbTick_CurrentMillisecond();
  NDB_TICKS max_time = curr_time + (NDB_TICKS)wait_time;
  const int maxsleep = (wait_time == -1 || wait_time > 10) ? 10 : wait_time;
  do
  {
    wait_for_input(maxsleep);
    Uint32 state= m_waiter->get_state();
    if (state == NO_WAIT)
    {
      return 0;
    }
    else if (state == WAIT_NODE_FAILURE)
    {
      ret_val= -2;
      break;
    }
    if (wait_time == -1)
    {
#ifdef NOT_USED
      ndbout << "Waited WAITFOR_RESPONSE_TIMEOUT, continuing wait" << endl;
#endif
      continue;
    }
    wait_time= max_time - NdbTick_CurrentMillisecond();
    if (wait_time <= 0)
    {
#ifdef VM_TRACE
      ndbout << "Time-out state is " << m_waiter->get_state() << endl;
#endif
      m_waiter->set_state(WST_WAIT_TIMEOUT);
      ret_val= -1;
      break;
    }
  } while (1);
#ifdef VM_TRACE
  ndbout << "ERR: receiveResponse - theImpl->theWaiter.m_state = ";
  ndbout << m_waiter->get_state() << endl;
#endif
  m_waiter->set_state(NO_WAIT);
  return ret_val;
}

void PollGuard::wait_for_input(int wait_time)
{
  NdbWaiter *t_poll_owner= m_tp->get_poll_owner();
  if (t_poll_owner != NULL && t_poll_owner != m_waiter)
  {
    /*
      We didn't get hold of the poll "right". We will sleep on a
      conditional mutex until the thread owning the poll "right"
      will wake us up after all data is received. If no data arrives
      we will wake up eventually due to the timeout.
      After receiving all data we take the object out of the cond wait
      queue if it hasn't happened already. It is usually already out of the
      queue but at time-out it could be that the object is still there.
    */
    (void) m_tp->put_in_cond_wait_queue(m_waiter);
    m_waiter->wait(wait_time);
    if (m_waiter->get_cond_wait_index() != TransporterFacade::MAX_NO_THREADS)
    {
      m_tp->remove_from_cond_wait_queue(m_waiter);
    }
  }
  else
  {
    /*
      We got the poll "right" and we poll until data is received. After
      receiving data we will check if all data is received, if not we
      poll again.
    */
#ifdef NDB_SHM_TRANSPORTER
    /*
      If shared memory transporters are used we need to set our sigmask
      such that we wake up also on interrupts on the shared memory
      interrupt signal.
    */
    NdbThread_set_shm_sigmask(FALSE);
#endif
    m_tp->set_poll_owner(m_waiter);
    m_waiter->set_poll_owner(true);
    m_tp->external_poll((Uint32)wait_time);
  }
}

void PollGuard::unlock_and_signal()
{
  NdbWaiter *t_signal_cond_waiter= 0;
  if (!m_locked)
    return;
  /*
   When completing the poll for this thread we must return the poll
   ownership if we own it. We will give it to the last thread that
   came here (the most recent) which is likely to be the one also
   last to complete. We will remove that thread from the conditional
   wait queue and set him as the new owner of the poll "right".
   We will wait however with the signal until we have unlocked the
   mutex for performance reasons.
   See Stevens book on Unix NetworkProgramming: The Sockets Networking
   API Volume 1 Third Edition on page 703-704 for a discussion on this
   subject.
  */
  if (m_tp->get_poll_owner() == m_waiter)
  {
#ifdef NDB_SHM_TRANSPORTER
    /*
      If shared memory transporters are used we need to reset our sigmask
      since we are no longer the thread to receive interrupts.
    */
    NdbThread_set_shm_sigmask(TRUE);
#endif
    m_waiter->set_poll_owner(false);
    t_signal_cond_waiter= m_tp->rem_last_from_cond_wait_queue();
    m_tp->set_poll_owner(t_signal_cond_waiter);
    if (t_signal_cond_waiter)
      t_signal_cond_waiter->set_poll_owner(true);
  }
  if (t_signal_cond_waiter)
    t_signal_cond_waiter->cond_signal();
  m_tp->unlock_mutex();
  m_locked=false;
}

template class Vector<NodeStatusFunction>;
template class Vector<TransporterFacade::ThreadData::Object_Execute>;

#include "SignalSender.hpp"

SendStatus
SignalSender::sendSignal(Uint16 nodeId, const SimpleSignal * s){
#ifdef API_TRACE
  if(setSignalLog() && TRACE_GSN(s->header.theVerId_signalNumber)){
    SignalHeader tmp = s->header;
    tmp.theSendersBlockRef = getOwnRef();

    LinearSectionPtr ptr[3];
    signalLogger.sendSignal(tmp,
			    1,
			    s->theData,
			    nodeId, ptr, 0);
    signalLogger.flushSignalLog();
  }
#endif
  assert(getNodeInfo(nodeId).m_api_reg_conf == true ||
         s->readSignalNumber() == GSN_API_REGREQ);
  
  SendStatus ss = 
    theFacade->theTransporterRegistry->prepareSend(&s->header,
                                                   1, // JBB
                                                   &s->theData[0],
                                                   nodeId, 
                                                   &s->ptr[0]);

  if (ss == SEND_OK)
  {
    theFacade->forceSend(m_blockNo);
  }

  return ss;
}


Uint32*
SignalSectionIterator::getNextWords(Uint32& sz)
{
  if (likely(currentSignal != NULL))
  {
    NdbApiSignal* signal= currentSignal;
    currentSignal= currentSignal->next();
    sz= signal->getLength();
    return signal->getDataPtrSend();
  }
  sz= 0;
  return NULL;
}


// Unit test code starts
#include <random.h>

#define VERIFY(x) if ((x) == 0) { printf("VERIFY failed at Line %u : %s\n",__LINE__, #x);  return -1; }

/* Verify that word[n] == bias + n */
int
verifyIteratorContents(GenericSectionIterator& gsi, int dataWords, int bias)
{
  int pos= 0;

  while (pos < dataWords)
  {
    Uint32* readPtr=NULL;
    Uint32 len= 0;

    readPtr= gsi.getNextWords(len);
    
    VERIFY(readPtr != NULL);
    VERIFY(len != 0);
    VERIFY(len <= (Uint32) (dataWords - pos));
    
    for (int j=0; j < (int) len; j++)
      VERIFY(readPtr[j] == (Uint32) (bias ++));

    pos += len;
  }

  return 0;
}

int
checkGenericSectionIterator(GenericSectionIterator& iter, int size, int bias)
{
  /* Verify contents */
  VERIFY(verifyIteratorContents(iter, size, bias) == 0);
  
  Uint32 sz;
  
  /* Check that iterator is empty now */
  VERIFY(iter.getNextWords(sz) == NULL);
  VERIFY(sz == 0);
    
  VERIFY(iter.getNextWords(sz) == NULL);
  VERIFY(sz == 0);
  
  iter.reset();
  
  /* Verify reset put us back to the start */
  VERIFY(verifyIteratorContents(iter, size, bias) == 0);
  
  /* Verify no more words available */
  VERIFY(iter.getNextWords(sz) == NULL);
  VERIFY(sz == 0);  
  
  return 0;
}

int
checkIterator(GenericSectionIterator& iter, int size, int bias)
{
  /* Test iterator itself, and then FragmentedSectionIterator
   * adaptation
   */
  VERIFY(checkGenericSectionIterator(iter, size, bias) == 0);
  
  /* Now we'll test the FragmentedSectionIterator on the iterator
   * we were passed
   */
  const int subranges= 20;
  
  iter.reset();
  GenericSectionPtr ptr;
  ptr.sz= size;
  ptr.sectionIter= &iter;
  FragmentedSectionIterator fsi(ptr);

  for (int s=0; s< subranges; s++)
  {
    Uint32 start= 0;
    Uint32 len= 0;
    if (size > 0)
    {
      start= (Uint32) myRandom48(size);
      if (0 != (size-start)) 
        len= (Uint32) myRandom48(size-start);
    }
    
    /*
      printf("Range (0-%u) = (%u + %u)\n",
              size, start, len);
    */
    fsi.setRange(start, len);
    VERIFY(checkGenericSectionIterator(fsi, len, bias + start) == 0);
  }
  
  return 0;
}



int
testLinearSectionIterator()
{
  /* Test Linear section iterator of various
   * lengths with section[n] == bias + n
   */
  const int totalSize= 200000;
  const int bias= 13;

  Uint32 data[totalSize];
  for (int i=0; i<totalSize; i++)
    data[i]= bias + i;

  for (int len= 0; len < 50000; len++)
  {
    LinearSectionIterator something(data, len);

    VERIFY(checkIterator(something, len, bias) == 0);
  }

  return 0;
}

NdbApiSignal*
createSignalChain(NdbApiSignal*& poolHead, int length, int bias)
{
  /* Create signal chain, with word[n] == bias+n */
  NdbApiSignal* chainHead= NULL;
  NdbApiSignal* chainTail= NULL;
  int pos= 0;
  int signals= 0;

  while (pos < length)
  {
    int offset= pos % NdbApiSignal::MaxSignalWords;
    
    if (offset == 0)
    {
      if (poolHead == NULL)
        return 0;

      NdbApiSignal* newSig= poolHead;
      poolHead= poolHead->next();
      signals++;

      newSig->next(NULL);

      if (chainHead == NULL)
      {
        chainHead= chainTail= newSig;
      }
      else
      {
        chainTail->next(newSig);
        chainTail= newSig;
      }
    }
    
    chainTail->getDataPtrSend()[offset]= (bias + pos);
    chainTail->setLength(offset + 1);
    pos ++;
  }

  return chainHead;
}
    
int
testSignalSectionIterator()
{
  /* Create a pool of signals, build
   * signal chains from it, test
   * the iterator against the signal chains
   */
  const int totalNumSignals= 1000;
  NdbApiSignal* poolHead= NULL;

  /* Allocate some signals */
  for (int i=0; i < totalNumSignals; i++)
  {
    NdbApiSignal* sig= new NdbApiSignal((BlockReference) 0);

    if (poolHead == NULL)
    {
      poolHead= sig;
      sig->next(NULL);
    }
    else
    {
      sig->next(poolHead);
      poolHead= sig;
    }
  }

  const int bias= 7;
  for (int dataWords= 1; 
       dataWords <= (int)(totalNumSignals * 
                          NdbApiSignal::MaxSignalWords); 
       dataWords ++)
  {
    NdbApiSignal* signalChain= NULL;
    
    VERIFY((signalChain= createSignalChain(poolHead, dataWords, bias)) != NULL );
    
    SignalSectionIterator ssi(signalChain);
    
    VERIFY(checkIterator(ssi, dataWords, bias) == 0);
    
    /* Now return the signals to the pool */
    while (signalChain != NULL)
    {
      NdbApiSignal* sig= signalChain;
      signalChain= signalChain->next();
      
      sig->next(poolHead);
      poolHead= sig;
    }
  }
  
  /* Free signals from pool */
  while (poolHead != NULL)
  {
    NdbApiSignal* sig= poolHead;
    poolHead= sig->next();
    delete(sig);
  }
  
  return 0;
}

//#define WANT_TESTSECTIONITERATORS 1

#ifdef WANT_TESTSECTIONITERATORS
int main(int arg, char** argv)
{
  /* Test Section Iterators
   * ----------------------
   * To run this code : 
   *   cd storage/ndb/src/ndbapi
   *   make testSectionIterators
   *   ./testSectionIterators
   *
   * Will print "OK" in success case
   */
  

  VERIFY(testLinearSectionIterator() == 0);
  VERIFY(testSignalSectionIterator() == 0);
  
  printf("OK\n");

  return 0;
}
#endif
