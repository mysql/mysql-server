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

#include "Cmvmi.hpp"

#include <Configuration.hpp>
#include <kernel_types.h>
#include <TransporterRegistry.hpp>
#include <NdbOut.hpp>
#include <NdbMem.h>

#include <SignalLoggerManager.hpp>
#include <FastScheduler.hpp>

#define DEBUG(x) { ndbout << "CMVMI::" << x << endl; }

#include <signaldata/TestOrd.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/TamperOrd.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/CloseComReqConf.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/EventSubscribeReq.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/DisconnectRep.hpp>

#include <EventLogger.hpp>
#include <TimeQueue.hpp>

#include <NdbSleep.h>
#include <SafeCounter.hpp>

// Used here only to print event reports on stdout/console.
EventLogger g_eventLogger;
extern int simulate_error_during_shutdown;

Cmvmi::Cmvmi(Block_context& ctx) :
  SimulatedBlock(CMVMI, ctx)
  ,subscribers(subscriberPool)
{
  BLOCK_CONSTRUCTOR(Cmvmi);

  Uint32 long_sig_buffer_size;
  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndb_mgm_get_int_parameter(p, CFG_DB_LONG_SIGNAL_BUFFER,  
			    &long_sig_buffer_size);

  long_sig_buffer_size= long_sig_buffer_size / 256;
  g_sectionSegmentPool.setSize(long_sig_buffer_size,
                               false,true,true,CFG_DB_LONG_SIGNAL_BUFFER);

  // Add received signals
  addRecSignal(GSN_CONNECT_REP, &Cmvmi::execCONNECT_REP);
  addRecSignal(GSN_DISCONNECT_REP, &Cmvmi::execDISCONNECT_REP);

  addRecSignal(GSN_NDB_TAMPER,  &Cmvmi::execNDB_TAMPER, true);
  addRecSignal(GSN_SET_LOGLEVELORD,  &Cmvmi::execSET_LOGLEVELORD);
  addRecSignal(GSN_EVENT_REP,  &Cmvmi::execEVENT_REP);
  addRecSignal(GSN_STTOR,  &Cmvmi::execSTTOR);
  addRecSignal(GSN_READ_CONFIG_REQ,  &Cmvmi::execREAD_CONFIG_REQ);
  addRecSignal(GSN_CLOSE_COMREQ,  &Cmvmi::execCLOSE_COMREQ);
  addRecSignal(GSN_ENABLE_COMORD,  &Cmvmi::execENABLE_COMORD);
  addRecSignal(GSN_OPEN_COMREQ,  &Cmvmi::execOPEN_COMREQ);
  addRecSignal(GSN_TEST_ORD,  &Cmvmi::execTEST_ORD);

  addRecSignal(GSN_TAMPER_ORD,  &Cmvmi::execTAMPER_ORD);
  addRecSignal(GSN_STOP_ORD,  &Cmvmi::execSTOP_ORD);
  addRecSignal(GSN_START_ORD,  &Cmvmi::execSTART_ORD);
  addRecSignal(GSN_EVENT_SUBSCRIBE_REQ, 
               &Cmvmi::execEVENT_SUBSCRIBE_REQ);

  addRecSignal(GSN_DUMP_STATE_ORD, &Cmvmi::execDUMP_STATE_ORD);

  addRecSignal(GSN_TESTSIG, &Cmvmi::execTESTSIG);
  addRecSignal(GSN_NODE_START_REP, &Cmvmi::execNODE_START_REP, true);
  
  subscriberPool.setSize(5);
  
  const ndb_mgm_configuration_iterator * db = m_ctx.m_config.getOwnConfigIterator();
  for(unsigned j = 0; j<LogLevel::LOGLEVEL_CATEGORIES; j++){
    Uint32 logLevel;
    if(!ndb_mgm_get_int_parameter(db, CFG_MIN_LOGLEVEL+j, &logLevel)){
      clogLevel.setLogLevel((LogLevel::EventCategory)j, 
			    logLevel);
    }
  }
  
  ndb_mgm_configuration_iterator * iter = m_ctx.m_config.getClusterConfigIterator();
  for(ndb_mgm_first(iter); ndb_mgm_valid(iter); ndb_mgm_next(iter)){
    jam();
    Uint32 nodeId;
    Uint32 nodeType;

    ndbrequire(!ndb_mgm_get_int_parameter(iter,CFG_NODE_ID, &nodeId));
    ndbrequire(!ndb_mgm_get_int_parameter(iter,CFG_TYPE_OF_SECTION,&nodeType));

    switch(nodeType){
    case NodeInfo::DB:
      c_dbNodes.set(nodeId);
      break;
    case NodeInfo::API:
    case NodeInfo::MGM:
      break;
    default:
      ndbrequire(false);
    }
    setNodeInfo(nodeId).m_type = nodeType;
  }

  setNodeInfo(getOwnNodeId()).m_connected = true;
  setNodeInfo(getOwnNodeId()).m_version = ndbGetOwnVersion();
  setNodeInfo(getOwnNodeId()).m_mysql_version = NDB_MYSQL_VERSION_D;
}

Cmvmi::~Cmvmi()
{
  m_shared_page_pool.clear();
}

#ifdef ERROR_INSERT
NodeBitmask c_error_9000_nodes_mask;
extern Uint32 MAX_RECEIVED_SIGNALS;
#endif

void Cmvmi::execNDB_TAMPER(Signal* signal) 
{
  jamEntry();
  SET_ERROR_INSERT_VALUE(signal->theData[0]);
  if(ERROR_INSERTED(9999)){
    CRASH_INSERTION(9999);
  }

  if(ERROR_INSERTED(9998)){
    while(true) NdbSleep_SecSleep(1);
  }

  if(ERROR_INSERTED(9997)){
    ndbrequire(false);
  }

#ifndef NDB_WIN32
  if(ERROR_INSERTED(9996)){
    simulate_error_during_shutdown= SIGSEGV;
    ndbrequire(false);
  }

  if(ERROR_INSERTED(9995)){
    simulate_error_during_shutdown= SIGSEGV;
    kill(getpid(), SIGABRT);
  }
#endif

#ifdef ERROR_INSERT
  if (signal->theData[0] == 9003)
  {
    if (MAX_RECEIVED_SIGNALS < 1024)
    {
      MAX_RECEIVED_SIGNALS = 1024;
    }
    else
    {
      MAX_RECEIVED_SIGNALS = 1 + (rand() % 128);
    }
    ndbout_c("MAX_RECEIVED_SIGNALS: %d", MAX_RECEIVED_SIGNALS);
    CLEAR_ERROR_INSERT_VALUE;
  }
#endif
}//execNDB_TAMPER()

void Cmvmi::execSET_LOGLEVELORD(Signal* signal) 
{
  SetLogLevelOrd * const llOrd = (SetLogLevelOrd *)&signal->theData[0];
  LogLevel::EventCategory category;
  Uint32 level;
  jamEntry();

  for(unsigned int i = 0; i<llOrd->noOfEntries; i++){
    category = (LogLevel::EventCategory)(llOrd->theData[i] >> 16);
    level = llOrd->theData[i] & 0xFFFF;
    
    clogLevel.setLogLevel(category, level);
  }
}//execSET_LOGLEVELORD()

void Cmvmi::execEVENT_REP(Signal* signal) 
{
  //-----------------------------------------------------------------------
  // This message is sent to report any types of events in NDB.
  // Based on the log level they will be either ignored or
  // reported. Currently they are printed, but they will be
  // transferred to the management server for further distribution
  // to the graphical management interface.
  //-----------------------------------------------------------------------
  EventReport * const eventReport = (EventReport *)&signal->theData[0]; 
  Ndb_logevent_type eventType = eventReport->getEventType();
  Uint32 nodeId= eventReport->getNodeId();
  if (nodeId == 0)
  {
    nodeId= refToNode(signal->getSendersBlockRef());
    eventReport->setNodeId(nodeId);
  }

  jamEntry();
  
  /**
   * If entry is not found
   */
  Uint32 threshold;
  LogLevel::EventCategory eventCategory;
  Logger::LoggerLevel severity;  
  EventLoggerBase::EventTextFunction textF;
  if (EventLoggerBase::event_lookup(eventType,eventCategory,threshold,severity,textF))
    return;
  
  SubscriberPtr ptr;
  for(subscribers.first(ptr); ptr.i != RNIL; subscribers.next(ptr)){
    if(ptr.p->logLevel.getLogLevel(eventCategory) < threshold){
      continue;
    }
    
    sendSignal(ptr.p->blockRef, GSN_EVENT_REP, signal, signal->length(), JBB);
  }
  
  if(clogLevel.getLogLevel(eventCategory) < threshold){
    return;
  }

  // Print the event info
  g_eventLogger.log(eventReport->getEventType(), 
		    signal->theData, signal->getLength(), 0, 0);
  
  return;
}//execEVENT_REP()

void
Cmvmi::execEVENT_SUBSCRIBE_REQ(Signal * signal){
  EventSubscribeReq * subReq = (EventSubscribeReq *)&signal->theData[0];
  Uint32 senderRef = signal->getSendersBlockRef();
  SubscriberPtr ptr;
  jamEntry();
  DBUG_ENTER("Cmvmi::execEVENT_SUBSCRIBE_REQ");

  /**
   * Search for subcription
   */
  for(subscribers.first(ptr); ptr.i != RNIL; subscribers.next(ptr)){
    if(ptr.p->blockRef == subReq->blockRef)
      break;
  }
  
  if(ptr.i == RNIL){
    /**
     * Create a new one
     */
    if(subscribers.seize(ptr) == false){
      sendSignal(senderRef, GSN_EVENT_SUBSCRIBE_REF, signal, 1, JBB);
      return;
    }
    ptr.p->logLevel.clear();
    ptr.p->blockRef = subReq->blockRef;    
  }
  
  if(subReq->noOfEntries == 0){
    /**
     * Cancel subscription
     */
    subscribers.release(ptr.i);
  } else {
    /**
     * Update subscription
     */
    LogLevel::EventCategory category;
    Uint32 level = 0;
    for(Uint32 i = 0; i<subReq->noOfEntries; i++){
      category = (LogLevel::EventCategory)(subReq->theData[i] >> 16);
      level = subReq->theData[i] & 0xFFFF;
      ptr.p->logLevel.setLogLevel(category, level);
      DBUG_PRINT("info",("entry %d: level=%d, category= %d", i, level, category));
    }
  }
  
  signal->theData[0] = ptr.i;
  sendSignal(senderRef, GSN_EVENT_SUBSCRIBE_CONF, signal, 1, JBB);
  DBUG_VOID_RETURN;
}

void
Cmvmi::cancelSubscription(NodeId nodeId){
  
  SubscriberPtr ptr;
  subscribers.first(ptr);
  
  while(ptr.i != RNIL){
    Uint32 i = ptr.i;
    BlockReference blockRef = ptr.p->blockRef;
    
    subscribers.next(ptr);
    
    if(refToNode(blockRef) == nodeId){
      subscribers.release(i);
    }
  }
}

void Cmvmi::sendSTTORRY(Signal* signal)
{
  jam();
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 8;
  signal->theData[6] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 7, JBB);
}//Cmvmi::sendSTTORRY


void 
Cmvmi::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Uint64 page_buffer = 64*1024*1024;
  ndb_mgm_get_int64_parameter(p, CFG_DB_DISK_PAGE_BUFFER_MEMORY, &page_buffer);
  
  Uint32 pages = 0;
  pages += page_buffer / GLOBAL_PAGE_SIZE; // in pages
  pages += LCP_RESTORE_BUFFER;
  m_global_page_pool.setSize(pages + 64, true);
  
  Uint64 shared_mem = 8*1024*1024;
  ndb_mgm_get_int64_parameter(p, CFG_DB_SGA, &shared_mem);
  shared_mem /= GLOBAL_PAGE_SIZE;

  Uint32 tupmem = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_PAGE, &tupmem));
  if (tupmem)
  {
    Resource_limit rl;
    rl.m_min = tupmem;
    rl.m_max = tupmem;
    rl.m_resource_id = 3;
    m_ctx.m_mm.set_resource_limit(rl);
  }

  if (shared_mem+tupmem)
  {
    Resource_limit rl;
    rl.m_min = 0;
    rl.m_max = shared_mem + tupmem;
    rl.m_resource_id = 0;
    m_ctx.m_mm.set_resource_limit(rl);
  }
  
  if (!m_ctx.m_mm.init())
  {
    char buf[255];

    struct ndb_mgm_param_info dm;
    struct ndb_mgm_param_info sga;
    size_t size = sizeof(ndb_mgm_param_info);
    
    ndb_mgm_get_db_parameter_info(CFG_DB_DATA_MEM, &dm, &size);
    size = sizeof(ndb_mgm_param_info);
    ndb_mgm_get_db_parameter_info(CFG_DB_SGA, &sga, &size);

    BaseString::snprintf(buf, sizeof(buf), 
			 "Malloc (%lld bytes) for %s and %s failed", 
			 Uint64(shared_mem + tupmem) * 32768,
			 dm.m_name, sga.m_name);
    
    ErrorReporter::handleAssert(buf,
				__FILE__, __LINE__, NDBD_EXIT_MEMALLOC);
    
    ndbrequire(false);
  }

  {
    void* ptr = m_ctx.m_mm.get_memroot();
    m_shared_page_pool.set((GlobalPage*)ptr, ~0);
  }
  
  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void Cmvmi::execSTTOR(Signal* signal)
{
  Uint32 theStartPhase  = signal->theData[1];

  jamEntry();
  if (theStartPhase == 1){
    jam();

    if(m_ctx.m_config.lockPagesInMainMemory() == 1)
    {
      int res = NdbMem_MemLockAll(0);
      if(res != 0){
	g_eventLogger.warning("Failed to memlock pages");
	warningEvent("Failed to memlock pages");
      }
    }
    
    sendSTTORRY(signal);
    return;
  } else if (theStartPhase == 3) {
    jam();
    globalData.activateSendPacked = 1;
    sendSTTORRY(signal);
  } else if (theStartPhase == 8){
    /*---------------------------------------------------*/
    /* Open com to API + REP nodes                       */
    /*---------------------------------------------------*/
    signal->theData[0] = 0; // no answer
    signal->theData[1] = 0; // no id
    signal->theData[2] = NodeInfo::API;
    execOPEN_COMREQ(signal);
    globalData.theStartLevel = NodeState::SL_STARTED;
    sendSTTORRY(signal);
  }
}

void Cmvmi::execCLOSE_COMREQ(Signal* signal)
{
  // Close communication with the node and halt input/output from 
  // other blocks than QMGR
  
  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];

  const BlockReference userRef = closeCom->xxxBlockRef;
  Uint32 failNo = closeCom->failNo;
//  Uint32 noOfNodes = closeCom->noOfNodes;
  
  jamEntry();
  for (unsigned i = 0; i < MAX_NODES; i++)
  {
    if(NodeBitmask::get(closeCom->theNodes, i))
    {
      jam();

      //-----------------------------------------------------
      // Report that the connection to the node is closed
      //-----------------------------------------------------
      signal->theData[0] = NDB_LE_CommunicationClosed;
      signal->theData[1] = i;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
      
      globalTransporterRegistry.setIOState(i, HaltIO);
      globalTransporterRegistry.do_disconnect(i);
    }
  }

  if (failNo != 0) 
  {
    jam();
    signal->theData[0] = userRef;
    signal->theData[1] = failNo;
    sendSignal(QMGR_REF, GSN_CLOSE_COMCONF, signal, 19, JBA);
  }
}

void Cmvmi::execOPEN_COMREQ(Signal* signal)
{
  // Connect to the specifed NDB node, only QMGR allowed communication 
  // so far with the node

  const BlockReference userRef = signal->theData[0];
  Uint32 tStartingNode = signal->theData[1];
  Uint32 tData2 = signal->theData[2];
  jamEntry();

  const Uint32 len = signal->getLength();
  if(len == 2)
  {
#ifdef ERROR_INSERT
    if (! ((ERROR_INSERTED(9000) || ERROR_INSERTED(9002)) 
	   && c_error_9000_nodes_mask.get(tStartingNode)))
#endif
    {
      if (globalData.theStartLevel != NodeState::SL_STARTED &&
          (getNodeInfo(tStartingNode).m_type != NodeInfo::DB &&
           getNodeInfo(tStartingNode).m_type != NodeInfo::MGM))
      {
        jam();
        goto done;
      }

      globalTransporterRegistry.do_connect(tStartingNode);
      globalTransporterRegistry.setIOState(tStartingNode, HaltIO);
      
      //-----------------------------------------------------
      // Report that the connection to the node is opened
      //-----------------------------------------------------
      signal->theData[0] = NDB_LE_CommunicationOpened;
      signal->theData[1] = tStartingNode;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
      //-----------------------------------------------------
    }
  } else {
    for(unsigned int i = 1; i < MAX_NODES; i++ ) 
    {
      jam();
      if (i != getOwnNodeId() && getNodeInfo(i).m_type == tData2)
      {
	jam();

#ifdef ERROR_INSERT
	if ((ERROR_INSERTED(9000) || ERROR_INSERTED(9002))
	    && c_error_9000_nodes_mask.get(i))
	  continue;
#endif
	
	globalTransporterRegistry.do_connect(i);
	globalTransporterRegistry.setIOState(i, HaltIO);
	
	signal->theData[0] = NDB_LE_CommunicationOpened;
	signal->theData[1] = i;
	sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
      }
    }
  }
  
done:  
  if (userRef != 0) {
    jam(); 
    signal->theData[0] = tStartingNode;
    signal->theData[1] = tData2;
    sendSignal(userRef, GSN_OPEN_COMCONF, signal, len - 1,JBA);
  }
}

void Cmvmi::execENABLE_COMORD(Signal* signal)
{
  // Enable communication with all our NDB blocks to this node
  
  Uint32 tStartingNode = signal->theData[0];
  globalTransporterRegistry.setIOState(tStartingNode, NoHalt);
  setNodeInfo(tStartingNode).m_connected = true;
    //-----------------------------------------------------
  // Report that the version of the node
  //-----------------------------------------------------
  signal->theData[0] = NDB_LE_ConnectedApiVersion;
  signal->theData[1] = tStartingNode;
  signal->theData[2] = getNodeInfo(tStartingNode).m_version;
  signal->theData[3] = getNodeInfo(tStartingNode).m_mysql_version;
  
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
  //-----------------------------------------------------
  
  jamEntry();
}

void Cmvmi::execDISCONNECT_REP(Signal *signal)
{
  const DisconnectRep * const rep = (DisconnectRep *)&signal->theData[0];
  const Uint32 hostId = rep->nodeId;
  const Uint32 errNo  = rep->err;
  
  jamEntry();

  setNodeInfo(hostId).m_connected = false;
  setNodeInfo(hostId).m_connectCount++;
  const NodeInfo::NodeType type = getNodeInfo(hostId).getType();
  ndbrequire(type != NodeInfo::INVALID);

  sendSignal(QMGR_REF, GSN_DISCONNECT_REP, signal, 
             DisconnectRep::SignalLength, JBA);
  
  cancelSubscription(hostId);

  signal->theData[0] = NDB_LE_Disconnected;
  signal->theData[1] = hostId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
}
 
void Cmvmi::execCONNECT_REP(Signal *signal){
  const Uint32 hostId = signal->theData[0];
  jamEntry();
  
  const NodeInfo::NodeType type = (NodeInfo::NodeType)getNodeInfo(hostId).m_type;
  ndbrequire(type != NodeInfo::INVALID);
  globalData.m_nodeInfo[hostId].m_version = 0;
  globalData.m_nodeInfo[hostId].m_mysql_version = 0;
  
  if(type == NodeInfo::DB || globalData.theStartLevel >= NodeState::SL_STARTED){
    jam();
    
    /**
     * Inform QMGR that client has connected
     */

    signal->theData[0] = hostId;
    sendSignal(QMGR_REF, GSN_CONNECT_REP, signal, 1, JBA);
  } else if(globalData.theStartLevel == NodeState::SL_CMVMI ||
            globalData.theStartLevel == NodeState::SL_STARTING) {
    jam();
    /**
     * Someone connected before start was finished
     */
    if(type == NodeInfo::MGM){
      jam();
      signal->theData[0] = hostId;
      sendSignal(QMGR_REF, GSN_CONNECT_REP, signal, 1, JBA);
    } else {
      /**
       * Dont allow api nodes to connect
       */
      ndbout_c("%d %d %d", hostId, type, globalData.theStartLevel);
      abort();
      globalTransporterRegistry.do_disconnect(hostId);
    }
  }
  
  /* Automatically subscribe events for MGM nodes.
   */
  if(type == NodeInfo::MGM){
    jam();
    globalTransporterRegistry.setIOState(hostId, NoHalt);
  }

  //------------------------------------------
  // Also report this event to the Event handler
  //------------------------------------------
  signal->theData[0] = NDB_LE_Connected;
  signal->theData[1] = hostId;
  signal->header.theLength = 2;
  
  execEVENT_REP(signal);
}

#ifdef VM_TRACE
void
modifySignalLogger(bool allBlocks, BlockNumber bno, 
                   TestOrd::Command cmd, 
                   TestOrd::SignalLoggerSpecification spec){
  SignalLoggerManager::LogMode logMode;

  /**
   * Mapping between SignalLoggerManager::LogMode and 
   *                 TestOrd::SignalLoggerSpecification
   */
  switch(spec){
  case TestOrd::InputSignals:
    logMode = SignalLoggerManager::LogIn;
    break;
  case TestOrd::OutputSignals:
    logMode = SignalLoggerManager::LogOut;
    break;
  case TestOrd::InputOutputSignals:
    logMode = SignalLoggerManager::LogInOut;
    break;
  default:
    return;
    break;
  }
  
  switch(cmd){
  case TestOrd::On:
    globalSignalLoggers.logOn(allBlocks, bno, logMode);
    break;
  case TestOrd::Off:
    globalSignalLoggers.logOff(allBlocks, bno, logMode);
    break;
  case TestOrd::Toggle:
    globalSignalLoggers.logToggle(allBlocks, bno, logMode);
    break;
  case TestOrd::KeepUnchanged:
    // Do nothing
    break;
  }
  globalSignalLoggers.flushSignalLog();
}
#endif

void
Cmvmi::execTEST_ORD(Signal * signal){
  jamEntry();
  
#ifdef VM_TRACE
  TestOrd * const testOrd = (TestOrd *)&signal->theData[0];

  TestOrd::Command cmd;

  {
    /**
     * Process Trace command
     */
    TestOrd::TraceSpecification traceSpec;

    testOrd->getTraceCommand(cmd, traceSpec);
    unsigned long traceVal = traceSpec;
    unsigned long currentTraceVal = globalSignalLoggers.getTrace();
    switch(cmd){
    case TestOrd::On:
      currentTraceVal |= traceVal;
      break;
    case TestOrd::Off:
      currentTraceVal &= (~traceVal);
      break;
    case TestOrd::Toggle:
      currentTraceVal ^= traceVal;
      break;
    case TestOrd::KeepUnchanged:
      // Do nothing
      break;
    }
    globalSignalLoggers.setTrace(currentTraceVal);
  }
  
  {
    /**
     * Process Log command
     */
    TestOrd::SignalLoggerSpecification logSpec;
    BlockNumber bno;
    unsigned int loggers = testOrd->getNoOfSignalLoggerCommands();
    
    if(loggers == (unsigned)~0){ // Apply command to all blocks
      testOrd->getSignalLoggerCommand(0, bno, cmd, logSpec);
      modifySignalLogger(true, bno, cmd, logSpec);
    } else {
      for(unsigned int i = 0; i<loggers; i++){
        testOrd->getSignalLoggerCommand(i, bno, cmd, logSpec);
        modifySignalLogger(false, bno, cmd, logSpec);
      }
    }
  }

  {
    /**
     * Process test command
     */
    testOrd->getTestCommand(cmd);
    switch(cmd){
    case TestOrd::On:{
      SET_GLOBAL_TEST_ON;
    }
    break;
    case TestOrd::Off:{
      SET_GLOBAL_TEST_OFF;
    }
    break;
    case TestOrd::Toggle:{
      TOGGLE_GLOBAL_TEST_FLAG;
    }
    break;
    case TestOrd::KeepUnchanged:
      // Do nothing
      break;
    }
    globalSignalLoggers.flushSignalLog();
  }

#endif
}

void Cmvmi::execSTOP_ORD(Signal* signal) 
{
  jamEntry();
  globalData.theRestartFlag = perform_stop;
}//execSTOP_ORD()

void
Cmvmi::execSTART_ORD(Signal* signal) {

  StartOrd * const startOrd = (StartOrd *)&signal->theData[0];
  jamEntry();
  
  Uint32 tmp = startOrd->restartInfo;
  if(StopReq::getPerformRestart(tmp)){
    jam();
    /**
     *
     */
    NdbRestartType type = NRT_Default;
    if(StopReq::getNoStart(tmp) && StopReq::getInitialStart(tmp))
      type = NRT_NoStart_InitialStart;
    if(StopReq::getNoStart(tmp) && !StopReq::getInitialStart(tmp))
      type = NRT_NoStart_Restart;
    if(!StopReq::getNoStart(tmp) && StopReq::getInitialStart(tmp))
      type = NRT_DoStart_InitialStart;
    if(!StopReq::getNoStart(tmp)&&!StopReq::getInitialStart(tmp))
      type = NRT_DoStart_Restart;
    NdbShutdown(NST_Restart, type);
  }

  if(globalData.theRestartFlag == system_started){
    jam()
    /**
     * START_ORD received when already started(ignored)
     */
    //ndbout << "START_ORD received when already started(ignored)" << endl;
    return;
  }
  
  if(globalData.theRestartFlag == perform_stop){
    jam()
    /**
     * START_ORD received when stopping(ignored)
     */
    //ndbout << "START_ORD received when stopping(ignored)" << endl;
    return;
  }
  
  if(globalData.theStartLevel == NodeState::SL_NOTHING){
    jam();
    globalData.theStartLevel = NodeState::SL_CMVMI;
    /**
     * Open connections to management servers
     */
    for(unsigned int i = 1; i < MAX_NODES; i++ ){
      if (getNodeInfo(i).m_type == NodeInfo::MGM){ 
        if(!globalTransporterRegistry.is_connected(i)){
          globalTransporterRegistry.do_connect(i);
          globalTransporterRegistry.setIOState(i, NoHalt);
        }
      }
    }

    EXECUTE_DIRECT(QMGR, GSN_START_ORD, signal, 1);
    return ;
  }
  
  if(globalData.theStartLevel == NodeState::SL_CMVMI){
    jam();

    if(m_ctx.m_config.lockPagesInMainMemory() == 2)
    {
      int res = NdbMem_MemLockAll(1);
      if(res != 0)
      {
	g_eventLogger.warning("Failed to memlock pages");
	warningEvent("Failed to memlock pages");
      }
      else
      {
	g_eventLogger.info("Locked future allocations");
      }
    }
    
    globalData.theStartLevel  = NodeState::SL_STARTING;
    globalData.theRestartFlag = system_started;
    /**
     * StartLevel 1
     *
     * Do Restart
     */
    
    // Disconnect all nodes as part of the system restart. 
    // We need to ensure that we are starting up
    // without any connected nodes.   
    for(unsigned int i = 1; i < MAX_NODES; i++ ){
      if (i != getOwnNodeId() && getNodeInfo(i).m_type != NodeInfo::MGM){
        globalTransporterRegistry.do_disconnect(i);
        globalTransporterRegistry.setIOState(i, HaltIO);
      }
    }
    
    /**
     * Start running startphases
     */
    sendSignal(NDBCNTR_REF, GSN_START_ORD, signal, 1, JBA);  
    return;
  }
}//execSTART_ORD()

void Cmvmi::execTAMPER_ORD(Signal* signal) 
{
  jamEntry();
  // TODO We should maybe introduce a CONF and REF signal
  // to be able to indicate if we really introduced an error.
#ifdef ERROR_INSERT
  TamperOrd* const tamperOrd = (TamperOrd*)&signal->theData[0];
  signal->theData[2] = 0;
  signal->theData[1] = tamperOrd->errorNo;
  signal->theData[0] = 5;
  sendSignal(DBDIH_REF, GSN_DIHNDBTAMPER, signal, 3,JBB);
#endif

}//execTAMPER_ORD()

#ifdef VM_TRACE
class RefSignalTest {
public:
  enum ErrorCode {
    OK = 0,
    NF_FakeErrorREF = 7
  };
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};
#endif


static int iii;

static
int
recurse(char * buf, int loops, int arg){
  char * tmp = (char*)alloca(arg);
  printf("tmp = %p\n", tmp);
  for(iii = 0; iii<arg; iii += 1024){
    tmp[iii] = (iii % 23 + (arg & iii));
  }
  
  if(loops == 0)
    return tmp[345];
  else
    return tmp[arg/loops] + recurse(tmp, loops - 1, arg);
}

#define check_block(block,val) \
(((val) >= DumpStateOrd::_ ## block ## Min) && ((val) <= DumpStateOrd::_ ## block ## Max))

void
Cmvmi::execDUMP_STATE_ORD(Signal* signal)
{
  Uint32 val = signal->theData[0];
  if (val >= DumpStateOrd::OneBlockOnly)
  {
    if (check_block(Backup, val))
    {
      sendSignal(BACKUP_REF, GSN_DUMP_STATE_ORD, signal, signal->length(), JBB);
    }
    else if (check_block(TC, val))
    {
    }
    return;
  }

  sendSignal(QMGR_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(NDBCNTR_REF, GSN_DUMP_STATE_ORD, signal, signal->length(), JBB);
  sendSignal(DBTC_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(DBDIH_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(DBDICT_REF, GSN_DUMP_STATE_ORD,  signal, signal->length(), JBB);
  sendSignal(DBLQH_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(DBTUP_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(DBACC_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(NDBFS_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(BACKUP_REF, GSN_DUMP_STATE_ORD,  signal, signal->length(), JBB);
  sendSignal(DBUTIL_REF, GSN_DUMP_STATE_ORD,  signal, signal->length(), JBB);
  sendSignal(SUMA_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(TRIX_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(DBTUX_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(LGMAN_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(TSMAN_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(PGMAN_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  
  /**
   *
   * Here I can dump CMVMI state if needed
   */
  if(signal->theData[0] == 13){
#if 0
    int loop = 100;
    int len = (10*1024*1024);
    if(signal->getLength() > 1)
      loop = signal->theData[1];
    if(signal->getLength() > 2)
      len = signal->theData[2];
    
    ndbout_c("recurse(%d loop, %dkb per recurse)", loop, len/1024);
    int a = recurse(0, loop, len);
    ndbout_c("after...%d", a);
#endif
  }

  DumpStateOrd * const & dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg = dumpState->args[0];
  if (arg == DumpStateOrd::CmvmiDumpConnections){
    for(unsigned int i = 1; i < MAX_NODES; i++ ){
      const char* nodeTypeStr = "";
      switch(getNodeInfo(i).m_type){
      case NodeInfo::DB:
	nodeTypeStr = "DB";
	break;
      case NodeInfo::API:
	nodeTypeStr = "API";
	break;
      case NodeInfo::MGM:
	nodeTypeStr = "MGM";
	break;
      case NodeInfo::INVALID:
	nodeTypeStr = 0;
	break;
      default:
	nodeTypeStr = "<UNKNOWN>";
      }

      if(nodeTypeStr == 0)
	continue;

      infoEvent("Connection to %d (%s) %s", 
                i, 
                nodeTypeStr,
                globalTransporterRegistry.getPerformStateString(i));
    }
  }
  
  if (arg == DumpStateOrd::CmvmiDumpSubscriptions)
  {
    SubscriberPtr ptr;
    subscribers.first(ptr);  
    g_eventLogger.info("List subscriptions:");
    while(ptr.i != RNIL)
    {
      g_eventLogger.info("Subscription: %u, nodeId: %u, ref: 0x%x",
                         ptr.i,  refToNode(ptr.p->blockRef), ptr.p->blockRef);
      for(Uint32 i = 0; i < LogLevel::LOGLEVEL_CATEGORIES; i++)
      {
        Uint32 level = ptr.p->logLevel.getLogLevel((LogLevel::EventCategory)i);
        g_eventLogger.info("Category %u Level %u", i, level);
      }
      subscribers.next(ptr);
    }
  }

  if (arg == DumpStateOrd::CmvmiDumpLongSignalMemory){
    infoEvent("Cmvmi: g_sectionSegmentPool size: %d free: %d",
	      g_sectionSegmentPool.getSize(),
	      g_sectionSegmentPool.getNoOfFree());
  }

  if (dumpState->args[0] == 1000)
  {
    Uint32 len = signal->getLength();
    if (signal->getLength() == 1)
    {
      signal->theData[1] = 0;
      signal->theData[2] = ~0;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 3, JBB);
      return;
    }
    Uint32 id = signal->theData[1];
    Resource_limit rl;
    if (!m_ctx.m_mm.get_resource_limit(id, rl))
      len = 2;
    else
    {
      if (rl.m_min || rl.m_curr || rl.m_max)
	infoEvent("Resource %d min: %d max: %d curr: %d",
		  id, rl.m_min, rl.m_max, rl.m_curr);
    }

    if (len == 3)
    {
      signal->theData[0] = 1000;
      signal->theData[1] = id+1;
      signal->theData[2] = ~0;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 3, JBB);
    }
    return;
  }
  
  if (arg == DumpStateOrd::CmvmiSetRestartOnErrorInsert)
  {
    if(signal->getLength() == 1)
    {
      Uint32 val = (Uint32)NRT_NoStart_Restart;
      const ndb_mgm_configuration_iterator * p = 
	m_ctx.m_config.getOwnConfigIterator();
      ndbrequire(p != 0);
      
      if(!ndb_mgm_get_int_parameter(p, CFG_DB_STOP_ON_ERROR_INSERT, &val))
      {
        m_ctx.m_config.setRestartOnErrorInsert(val);
      }
    }
    else
    {
      m_ctx.m_config.setRestartOnErrorInsert(signal->theData[1]);
    }
  }

  if (arg == DumpStateOrd::CmvmiTestLongSigWithDelay) {
    unsigned i;
    Uint32 loopCount = dumpState->args[1];
    const unsigned len0 = 11;
    const unsigned len1 = 123;
    Uint32 sec0[len0];
    Uint32 sec1[len1];
    for (i = 0; i < len0; i++)
      sec0[i] = i;
    for (i = 0; i < len1; i++)
      sec1[i] = 16 * i;
    Uint32* sig = signal->getDataPtrSend();
    sig[0] = reference();
    sig[1] = 20; // test type
    sig[2] = 0;
    sig[3] = 0;
    sig[4] = loopCount;
    sig[5] = len0;
    sig[6] = len1;
    sig[7] = 0;
    LinearSectionPtr ptr[3];
    ptr[0].p = sec0;
    ptr[0].sz = len0;
    ptr[1].p = sec1;
    ptr[1].sz = len1;
    sendSignal(reference(), GSN_TESTSIG, signal, 8, JBB, ptr, 2);
  }

#ifdef ERROR_INSERT
  if (arg == 9000 || arg == 9002)
  {
    SET_ERROR_INSERT_VALUE(arg);
    for (Uint32 i = 1; i<signal->getLength(); i++)
      c_error_9000_nodes_mask.set(signal->theData[i]);
  }
  
  if (arg == 9001)
  {
    CLEAR_ERROR_INSERT_VALUE;
    if (signal->getLength() == 1 || signal->theData[1])
    {
      for (Uint32 i = 0; i<MAX_NODES; i++)
      {
	if (c_error_9000_nodes_mask.get(i))
	{
	  signal->theData[0] = 0;
	  signal->theData[1] = i;
	  EXECUTE_DIRECT(CMVMI, GSN_OPEN_COMREQ, signal, 2);
	}
      }
    }
    c_error_9000_nodes_mask.clear();
  }
#endif

#ifdef VM_TRACE
#if 0
  {
    SafeCounterManager mgr(* this); mgr.setSize(1);
    SafeCounterHandle handle;

    {
      SafeCounter tmp(mgr, handle);
      tmp.init<RefSignalTest>(CMVMI, GSN_TESTSIG, /* senderData */ 13);
      tmp.setWaitingFor(3);
      ndbrequire(!tmp.done());
      ndbout_c("Allocted");
    }
    ndbrequire(!handle.done());
    {
      SafeCounter tmp(mgr, handle);
      tmp.clearWaitingFor(3);
      ndbrequire(tmp.done());
      ndbout_c("Deallocted");
    }
    ndbrequire(handle.done());
  }
#endif
#endif

  if (arg == 9999)
  {
    Uint32 delay = 1000;
    switch(signal->getLength()){
    case 1:
      break;
    case 2:
      delay = signal->theData[1];
      break;
    default:{
      Uint32 dmin = signal->theData[1];
      Uint32 dmax = signal->theData[2];
      delay = dmin + (rand() % (dmax - dmin));
      break;
    }
    }
    
    signal->theData[0] = 9999;
    if (delay == 0)
    {
      execNDB_TAMPER(signal);
    }
    else if (delay < 10)
    {
      sendSignal(reference(), GSN_NDB_TAMPER, signal, 1, JBB);
    }
    else
    {
      sendSignalWithDelay(reference(), GSN_NDB_TAMPER, signal, delay, 1);
    }
  }
}//Cmvmi::execDUMP_STATE_ORD()

void
Cmvmi::execNODE_START_REP(Signal* signal)
{
#ifdef ERROR_INSERT
  if (ERROR_INSERTED(9002) && signal->theData[0] == getOwnNodeId())
  {
    signal->theData[0] = 9001;
    execDUMP_STATE_ORD(signal);
  }
#endif
}

BLOCK_FUNCTIONS(Cmvmi)

static Uint32 g_print;
static LinearSectionPtr g_test[3];

void
Cmvmi::execTESTSIG(Signal* signal){
  Uint32 i;
  /**
   * Test of SafeCounter
   */
  jamEntry();

  if(!assembleFragments(signal)){
    jam();
    return;
  }

  Uint32 ref = signal->theData[0];
  Uint32 testType = signal->theData[1];
  Uint32 fragmentLength = signal->theData[2];
  g_print = signal->theData[3];
//  Uint32 returnCount = signal->theData[4];
  Uint32 * secSizes = &signal->theData[5];
  
  if(g_print){
    SignalLoggerManager::printSignalHeader(stdout, 
					   signal->header,
					   0,
					   getOwnNodeId(),
					   true);
    ndbout_c("-- Fixed section --");    
    for(i = 0; i<signal->length(); i++){
      fprintf(stdout, "H'0x%.8x ", signal->theData[i]);
      if(((i + 1) % 6) == 0)
	fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
    
    for(i = 0; i<signal->header.m_noOfSections; i++){
      SegmentedSectionPtr ptr(0,0,0);
      ndbout_c("-- Section %d --", i);
      signal->getSection(ptr, i);
      ndbrequire(ptr.p != 0);
      print(ptr, stdout);
      ndbrequire(ptr.sz == secSizes[i]);
    }
  }

  /**
   * Validate length:s
   */
  for(i = 0; i<signal->header.m_noOfSections; i++){
    SegmentedSectionPtr ptr;
    signal->getSection(ptr, i);
    ndbrequire(ptr.p != 0);
    ndbrequire(ptr.sz == secSizes[i]);
  }

  /**
   * Testing send with delay.
   */
  if (testType == 20) {
    if (signal->theData[4] == 0) {
      releaseSections(signal);
      return;
    }
    signal->theData[4]--;
    sendSignalWithDelay(reference(), GSN_TESTSIG, signal, 100, 8);
    return;
  }
  
  NodeReceiverGroup rg(CMVMI, c_dbNodes);

  if(signal->getSendersBlockRef() == ref){
    /**
     * Signal from API (not via NodeReceiverGroup)
     */
    if((testType % 2) == 1){
      signal->theData[4] = 1;
    } else {
      signal->theData[1] --;
      signal->theData[4] = rg.m_nodes.count();
    }
  } 
  
  switch(testType){
  case 1:
    sendSignal(ref, GSN_TESTSIG,  signal, signal->length(), JBB);      
    break;
  case 2:
    sendSignal(rg, GSN_TESTSIG,  signal, signal->length(), JBB);
    break;
  case 3:
  case 4:{
    LinearSectionPtr ptr[3];
    const Uint32 secs = signal->getNoOfSections();
    for(i = 0; i<secs; i++){
      SegmentedSectionPtr sptr(0,0,0);
      signal->getSection(sptr, i);
      ptr[i].sz = sptr.sz;
      ptr[i].p = new Uint32[sptr.sz];
      copy(ptr[i].p, sptr);
    }
    
    if(testType == 3){
      sendSignal(ref, GSN_TESTSIG,  signal, signal->length(), JBB, ptr, secs); 
    } else {
      sendSignal(rg, GSN_TESTSIG,  signal, signal->length(), JBB, ptr, secs); 
    }
    for(Uint32 i = 0; i<secs; i++){
      delete[] ptr[i].p;
    }
    break;
  }
  case 5:
  case 6:{
    
    NodeReceiverGroup tmp;
    if(testType == 5){
      tmp  = ref;
    } else {
      tmp = rg;
    }
    
    FragmentSendInfo fragSend;
    sendFirstFragment(fragSend,
		      tmp,
		      GSN_TESTSIG,
		      signal,
		      signal->length(),
		      JBB,
		      fragmentLength);
    int count = 1;
    while(fragSend.m_status != FragmentSendInfo::SendComplete){
      count++;
      if(g_print)
	ndbout_c("Sending fragment %d", count);
      sendNextSegmentedFragment(signal, fragSend);
    }
    break;
  }
  case 7:
  case 8:{
    LinearSectionPtr ptr[3];
    const Uint32 secs = signal->getNoOfSections();
    for(i = 0; i<secs; i++){
      SegmentedSectionPtr sptr(0,0,0);
      signal->getSection(sptr, i);
      ptr[i].sz = sptr.sz;
      ptr[i].p = new Uint32[sptr.sz];
      copy(ptr[i].p, sptr);
    }

    NodeReceiverGroup tmp;
    if(testType == 7){
      tmp  = ref;
    } else {
      tmp = rg;
    }

    FragmentSendInfo fragSend;
    sendFirstFragment(fragSend,
		      tmp,
		      GSN_TESTSIG,
		      signal,
		      signal->length(),
		      JBB,
		      ptr,
		      secs,
		      fragmentLength);
    
    int count = 1;
    while(fragSend.m_status != FragmentSendInfo::SendComplete){
      count++;
      if(g_print)
	ndbout_c("Sending fragment %d", count);
      sendNextLinearFragment(signal, fragSend);
    }
    
    for(i = 0; i<secs; i++){
      delete[] ptr[i].p;
    }
    break;
  }
  case 9:
  case 10:{

    Callback m_callBack;
    m_callBack.m_callbackFunction = 
      safe_cast(&Cmvmi::sendFragmentedComplete);
    
    if(testType == 9){
      m_callBack.m_callbackData = 9;
      sendFragmentedSignal(ref,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   m_callBack,
			   fragmentLength);
    } else {
      m_callBack.m_callbackData = 10;
      sendFragmentedSignal(rg,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   m_callBack,
			   fragmentLength);
    }
    break;
  }
  case 11:
  case 12:{

    const Uint32 secs = signal->getNoOfSections();
    memset(g_test, 0, sizeof(g_test));
    for(i = 0; i<secs; i++){
      SegmentedSectionPtr sptr(0,0,0);
      signal->getSection(sptr, i);
      g_test[i].sz = sptr.sz;
      g_test[i].p = new Uint32[sptr.sz];
      copy(g_test[i].p, sptr);
    }
    
    
    Callback m_callBack;
    m_callBack.m_callbackFunction = 
      safe_cast(&Cmvmi::sendFragmentedComplete);
    
    if(testType == 11){
      m_callBack.m_callbackData = 11;
      sendFragmentedSignal(ref,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   g_test, secs,
			   m_callBack,
			   fragmentLength);
    } else {
      m_callBack.m_callbackData = 12;
      sendFragmentedSignal(rg,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   g_test, secs,
			   m_callBack,
			   fragmentLength);
    }
    break;
  }
  case 13:{
    ndbrequire(signal->getNoOfSections() == 0);
    Uint32 loop = signal->theData[9];
    if(loop > 0){
      signal->theData[9] --;
      sendSignal(CMVMI_REF, GSN_TESTSIG, signal, signal->length(), JBB);
      return;
    }
    sendSignal(ref, GSN_TESTSIG, signal, signal->length(), JBB);
    return;
  }
  case 14:{
    Uint32 count = signal->theData[8];
    signal->theData[10] = count * rg.m_nodes.count();
    for(i = 0; i<count; i++){
      sendSignal(rg, GSN_TESTSIG, signal, signal->length(), JBB); 
    }
    return;
  }

  default:
    ndbrequire(false);
  }
  return;
}

void
Cmvmi::sendFragmentedComplete(Signal* signal, Uint32 data, Uint32 returnCode){
  if(g_print)
    ndbout_c("sendFragmentedComplete: %d", data);
  if(data == 11 || data == 12){
    for(Uint32 i = 0; i<3; i++){
      if(g_test[i].p != 0)
	delete[] g_test[i].p;
    }
  }
}
