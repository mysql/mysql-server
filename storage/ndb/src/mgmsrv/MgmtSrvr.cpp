/* Copyright (c) 2003-2007 MySQL AB


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


#include <ndb_global.h>
#include <my_pthread.h>

#include "MgmtSrvr.hpp"
#include "MgmtErrorReporter.hpp"
#include "ndb_mgmd_error.h"
#include <ConfigRetriever.hpp>

#include <NdbOut.hpp>
#include <NdbApiSignal.hpp>
#include <kernel_types.h>
#include <RefConvert.hpp>
#include <BlockNumbers.h>
#include <GlobalSignalNumbers.h>
#include <signaldata/TestOrd.hpp>
#include <signaldata/TamperOrd.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/ApiVersion.hpp>
#include <signaldata/ResumeReq.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/EventSubscribeReq.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/BackupSignalData.hpp>
#include <signaldata/ManagementServer.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/AllocNodeId.hpp>
#include <NdbSleep.h>
#include <EventLogger.hpp>
#include <DebuggerNames.hpp>
#include <ndb_version.h>

#include <SocketServer.hpp>
#include <NdbConfig.h>

#include <NdbAutoPtr.hpp>

#include <ndberror.h>

#include <mgmapi.h>
#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters.h>
#include <m_string.h>

#include <SignalSender.hpp>

//#define MGM_SRV_DEBUG
#ifdef MGM_SRV_DEBUG
#define DEBUG(x) do ndbout << x << endl; while(0)
#else
#define DEBUG(x)
#endif

int g_errorInsert;
#define ERROR_INSERTED(x) (g_errorInsert == x)

#define INIT_SIGNAL_SENDER(ss,nodeId) \
  SignalSender ss(theFacade); \
  ss.lock(); /* lock will be released on exit */ \
  {\
    int result = okToSendTo(nodeId, true);\
    if (result != 0) {\
      return result;\
    }\
  }

extern int g_no_nodeid_checks;
extern my_bool opt_core;

static void require(bool v)
{
  if(!v)
  {
    if (opt_core)
      abort();
    else
      exit(-1);
  }
}

void *
MgmtSrvr::logLevelThread_C(void* m)
{
  MgmtSrvr *mgm = (MgmtSrvr*)m;
  mgm->logLevelThreadRun();
  return 0;
}

extern EventLogger * g_eventLogger;

#ifdef NOT_USED
static NdbOut&
operator<<(NdbOut& out, const LogLevel & ll)
{
  out << "[LogLevel: ";
  for(size_t i = 0; i<LogLevel::LOGLEVEL_CATEGORIES; i++)
    out << ll.getLogLevel((LogLevel::EventCategory)i) << " ";
  out << "]";
  return out;
}
#endif

void
MgmtSrvr::logLevelThreadRun() 
{
  while (!_isStopThread) 
  {
    Vector<NodeId> failed_started_nodes;
    Vector<EventSubscribeReq> failed_log_level_requests;

    /**
     * Handle started nodes
     */
    m_started_nodes.lock();
    if (m_started_nodes.size() > 0)
    {
      // calculate max log level
      EventSubscribeReq req;
      {
        LogLevel tmp;
        m_event_listner.lock();
        for(int i = m_event_listner.m_clients.size() - 1; i >= 0; i--)
          tmp.set_max(m_event_listner[i].m_logLevel);
        m_event_listner.unlock();
        req.assign(tmp);
      }
      req.blockRef = _ownReference;
      while (m_started_nodes.size() > 0)
      {
        Uint32 node = m_started_nodes[0];
        m_started_nodes.erase(0, false);
        m_started_nodes.unlock();

        if (setEventReportingLevelImpl(node, req))
        {
          failed_started_nodes.push_back(node);
        }
        else
        {
          SetLogLevelOrd ord;
          ord.assign(m_nodeLogLevel[node]);
          setNodeLogLevelImpl(node, ord);
        }
        m_started_nodes.lock();
      }
    }
    m_started_nodes.unlock();
    
    m_log_level_requests.lock();
    while (m_log_level_requests.size() > 0)
    {
      EventSubscribeReq req = m_log_level_requests[0];
      m_log_level_requests.erase(0, false);
      m_log_level_requests.unlock();

      if(req.blockRef == 0)
      {
        req.blockRef = _ownReference;
        if (setEventReportingLevelImpl(0, req))
        {
          failed_log_level_requests.push_back(req);
        }
      } 
      else 
      {
        SetLogLevelOrd ord;
        ord.assign(req);
        if (setNodeLogLevelImpl(req.blockRef, ord))
        {
          failed_log_level_requests.push_back(req);
        }
      }
      m_log_level_requests.lock();
    }      
    m_log_level_requests.unlock();

    if(!ERROR_INSERTED(10000))
      m_event_listner.check_listeners();

    Uint32 sleeptime = _logLevelThreadSleep;
    if (failed_started_nodes.size())
    {
      m_started_nodes.lock();
      for (Uint32 i = 0; i<failed_started_nodes.size(); i++)
        m_started_nodes.push_back(failed_started_nodes[i], false);
      m_started_nodes.unlock();
      failed_started_nodes.clear();
      sleeptime = 100;
    }

    if (failed_log_level_requests.size())
    {
      m_log_level_requests.lock();
      for (Uint32 i = 0; i<failed_log_level_requests.size(); i++)
        m_log_level_requests.push_back(failed_log_level_requests[i], false);
      m_log_level_requests.unlock();
      failed_log_level_requests.clear();
      sleeptime = 100;
    }

    NdbSleep_MilliSleep(sleeptime);
  }
}

void
MgmtSrvr::startEventLog() 
{
  NdbMutex_Lock(m_configMutex);

  g_eventLogger->setCategory("MgmSrvr");

  ndb_mgm_configuration_iterator 
    iter(* _config->m_configValues, CFG_SECTION_NODE);

  if(iter.find(CFG_NODE_ID, _ownNodeId) != 0){
    NdbMutex_Unlock(m_configMutex);
    return;
  }
  
  const char * tmp;
  char errStr[100];
  int err= 0;
  BaseString logdest;
  char *clusterLog= NdbConfig_ClusterLogFileName(_ownNodeId);
  NdbAutoPtr<char> tmp_aptr(clusterLog);

  if(iter.get(CFG_LOG_DESTINATION, &tmp) == 0){
    logdest.assign(tmp);
  }
  NdbMutex_Unlock(m_configMutex);
  
  if(logdest.length() == 0 || logdest == "") {
    logdest.assfmt("FILE:filename=%s,maxsize=1000000,maxfiles=6", 
		   clusterLog);
  }
  errStr[0]='\0';
  if(!g_eventLogger->addHandler(logdest, &err, sizeof(errStr), errStr)) {
    ndbout << "Warning: could not add log destination \""
           << logdest.c_str() << "\". Reason: ";
    if(err)
      ndbout << strerror(err);
    if(err && errStr[0]!='\0')
      ndbout << ", ";
    if(errStr[0]!='\0')
      ndbout << errStr;
    ndbout << endl;
  }
}

void
MgmtSrvr::stopEventLog()
{
  g_eventLogger->close();
}

bool
MgmtSrvr::setEventLogFilter(int severity, int enable)
{
  Logger::LoggerLevel level = (Logger::LoggerLevel)severity;
  if (enable > 0) {
    g_eventLogger->enable(level);
  } else if (enable == 0) {
    g_eventLogger->disable(level);
  } else if (g_eventLogger->isEnable(level)) {
    g_eventLogger->disable(level);
  } else {
    g_eventLogger->enable(level);
  }
  return g_eventLogger->isEnable(level);
}

bool 
MgmtSrvr::isEventLogFilterEnabled(int severity) 
{
  return g_eventLogger->isEnable((Logger::LoggerLevel)severity);
}

int MgmtSrvr::translateStopRef(Uint32 errCode)
{
  switch(errCode){
  case StopRef::NodeShutdownInProgress:
    return NODE_SHUTDOWN_IN_PROGESS;
    break;
  case StopRef::SystemShutdownInProgress:
    return SYSTEM_SHUTDOWN_IN_PROGRESS;
    break;
  case StopRef::NodeShutdownWouldCauseSystemCrash:
    return NODE_SHUTDOWN_WOULD_CAUSE_SYSTEM_CRASH;
    break;
  case StopRef::UnsupportedNodeShutdown:
    return UNSUPPORTED_NODE_SHUTDOWN;
    break;
  }
  return 4999;
}

int 
MgmtSrvr::getNodeCount(enum ndb_mgm_node_type type) const 
{
  int count = 0;
  NodeId nodeId = 0;

  while (getNextNodeId(&nodeId, type)) {
    count++;
  }
  return count;
}

int 
MgmtSrvr::getPort() const
{
  if(NdbMutex_Lock(m_configMutex))
    return 0;

  ndb_mgm_configuration_iterator 
    iter(* _config->m_configValues, CFG_SECTION_NODE);

  if(iter.find(CFG_NODE_ID, getOwnNodeId()) != 0){
    ndbout << "Could not retrieve configuration for Node " 
	   << getOwnNodeId() << " in config file." << endl 
	   << "Have you set correct NodeId for this node?" << endl;
    NdbMutex_Unlock(m_configMutex);
    return 0;
  }

  unsigned type;
  if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0 ||
     type != NODE_TYPE_MGM){
    ndbout << "Local node id " << getOwnNodeId()
	   << " is not defined as management server" << endl
	   << "Have you set correct NodeId for this node?" << endl;
    NdbMutex_Unlock(m_configMutex);
    return 0;
  }
  
  Uint32 port = 0;
  if(iter.get(CFG_MGM_PORT, &port) != 0){
    ndbout << "Could not find PortNumber in the configuration file." << endl;
    NdbMutex_Unlock(m_configMutex);
    return 0;
  }

  NdbMutex_Unlock(m_configMutex);

  return port;
}

/* Constructor */
int MgmtSrvr::init()
{
  if ( _ownNodeId > 0)
    return 0;
  return -1;
}

MgmtSrvr::MgmtSrvr(SocketServer *socket_server,
		   const char *config_filename,
		   const char *connect_string) :
  _blockNumber(1), // Hard coded block number since it makes it easy to send
                   // signals to other management servers.
  m_socket_server(socket_server),
  _ownReference(0),
  theSignalIdleList(NULL),
  theWaitState(WAIT_SUBSCRIBE_CONF),
  m_local_mgm_handle(0),
  m_event_listner(this),
  m_master_node(0),
  m_version_string(ndbGetOwnVersionString())
{
    
  DBUG_ENTER("MgmtSrvr::MgmtSrvr");

  _ownNodeId= 0;

  _config     = NULL;

  _isStopThread        = false;
  _logLevelThread      = NULL;
  _logLevelThreadSleep = 500;

  theFacade = 0;

  m_newConfig = NULL;
  if (config_filename)
    m_configFilename.assign(config_filename);

  m_nextConfigGenerationNumber = 0;

  m_config_retriever= new ConfigRetriever(connect_string,
					  NDB_VERSION, NDB_MGM_NODE_TYPE_MGM);
  // if connect_string explicitly given or
  // no config filename is given then
  // first try to allocate nodeid from another management server
  if ((connect_string || config_filename == NULL) &&
      (m_config_retriever->do_connect(0,0,0) == 0))
  {
    int tmp_nodeid= 0;
    tmp_nodeid= m_config_retriever->allocNodeId(0 /*retry*/,0 /*delay*/);
    if (tmp_nodeid == 0)
    {
      ndbout_c(m_config_retriever->getErrorString());
      require(false);
    }
    // read config from other managent server
    _config= fetchConfig();
    if (_config == 0)
    {
      ndbout << m_config_retriever->getErrorString() << endl;
      require(false);
    }
    _ownNodeId= tmp_nodeid;
  }

  if (_ownNodeId == 0)
  {
    // read config locally
    _config= readConfig();
    if (_config == 0) {
      if (config_filename != NULL)
        ndbout << "Invalid configuration file: " << config_filename << endl;
      else
        ndbout << "Invalid configuration file" << endl;
      exit(-1);
    }
  }

  theMgmtWaitForResponseCondPtr = NdbCondition_Create();

  m_configMutex = NdbMutex_Create();

  /**
   * Fill the nodeTypes array
   */
  for(Uint32 i = 0; i<MAX_NODES; i++) {
    nodeTypes[i] = (enum ndb_mgm_node_type)-1;
    m_connect_address[i].s_addr= 0;
  }

  {
    ndb_mgm_configuration_iterator
      iter(* _config->m_configValues, CFG_SECTION_NODE);

    for(iter.first(); iter.valid(); iter.next()){
      unsigned type, id;
      if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0)
	continue;
      
      if(iter.get(CFG_NODE_ID, &id) != 0)
	continue;
      
      MGM_REQUIRE(id < MAX_NODES);
      
      switch(type){
      case NODE_TYPE_DB:
	nodeTypes[id] = NDB_MGM_NODE_TYPE_NDB;
	break;
      case NODE_TYPE_API:
	nodeTypes[id] = NDB_MGM_NODE_TYPE_API;
	break;
      case NODE_TYPE_MGM:
	nodeTypes[id] = NDB_MGM_NODE_TYPE_MGM;
	break;
      default:
	break;
      }
    }
  }

  _props = NULL;
  BaseString error_string;

  if ((m_node_id_mutex = NdbMutex_Create()) == 0)
  {
    ndbout << "mutex creation failed line = " << __LINE__ << endl;
    require(false);
  }

  if (_ownNodeId == 0) // we did not get node id from other server
  {
    NodeId tmp= m_config_retriever->get_configuration_nodeid();
    int error_code;

    if (!alloc_node_id(&tmp, NDB_MGM_NODE_TYPE_MGM,
		       0, 0, error_code, error_string, 20)){
      ndbout << "Unable to obtain requested nodeid: "
	     << error_string.c_str() << endl;
      require(false);
    }
    _ownNodeId = tmp;
  }

  {
    DBUG_PRINT("info", ("verifyConfig"));
    if (!m_config_retriever->verifyConfig(_config->m_configValues,
					  _ownNodeId))
    {
      ndbout << m_config_retriever->getErrorString() << endl;
      require(false);
    }
  }

  // Setup clusterlog as client[0] in m_event_listner
  {
    Ndb_mgmd_event_service::Event_listener se;
    se.m_socket = NDB_INVALID_SOCKET;
    for(size_t t = 0; t<LogLevel::LOGLEVEL_CATEGORIES; t++){
      se.m_logLevel.setLogLevel((LogLevel::EventCategory)t, 7);
    }
    se.m_logLevel.setLogLevel(LogLevel::llError, 15);
    se.m_logLevel.setLogLevel(LogLevel::llConnection, 8);
    se.m_logLevel.setLogLevel(LogLevel::llBackup, 15);
    m_event_listner.m_clients.push_back(se);
    m_event_listner.m_logLevel = se.m_logLevel;
  }
  
  DBUG_VOID_RETURN;
}


//****************************************************************************
//****************************************************************************
bool 
MgmtSrvr::check_start() 
{
  if (_config == 0) {
    DEBUG("MgmtSrvr.cpp: _config is NULL.");
    return false;
  }

  return true;
}

bool 
MgmtSrvr::start(BaseString &error_string, const char * bindaddress)
{
  int mgm_connect_result;

  DBUG_ENTER("MgmtSrvr::start");
  if (_props == NULL) {
    if (!check_start()) {
      error_string.append("MgmtSrvr.cpp: check_start() failed.");
      DBUG_RETURN(false);
    }
  }
  theFacade= new TransporterFacade(0);
  
  if(theFacade == 0) {
    DEBUG("MgmtSrvr.cpp: theFacade is NULL.");
    error_string.append("MgmtSrvr.cpp: theFacade is NULL.");
    DBUG_RETURN(false);
  }  
  if ( theFacade->start_instance
       (_ownNodeId, (ndb_mgm_configuration*)_config->m_configValues) < 0) {
    DEBUG("MgmtSrvr.cpp: TransporterFacade::start_instance < 0.");
    DBUG_RETURN(false);
  }

  MGM_REQUIRE(_blockNumber == 1);

  // Register ourself at TransporterFacade to be able to receive signals
  // and to be notified when a database process has died.
  _blockNumber = theFacade->open(this,
				 signalReceivedNotification,
				 nodeStatusNotification);
  
  if(_blockNumber == -1){
    DEBUG("MgmtSrvr.cpp: _blockNumber is -1.");
    error_string.append("MgmtSrvr.cpp: _blockNumber is -1.");
    theFacade->stop_instance();
    theFacade = 0;
    DBUG_RETURN(false);
  }

  if((mgm_connect_result= connect_to_self(bindaddress)) < 0)
  {
    ndbout_c("Unable to connect to our own ndb_mgmd (Error %d)",
             mgm_connect_result);
    ndbout_c("This is probably a bug.");
  }

  /*
    set api reg req frequency quite high:

    100 ms interval to make sure we have fairly up-to-date
    info from the nodes.  This to make sure that this info
    is not dependent on heart beat settings in the
    configuration
  */
  theFacade->theClusterMgr->set_max_api_reg_req_interval(100);

  TransporterRegistry *reg = theFacade->get_registry();
  for(unsigned int i=0;i<reg->m_transporter_interface.size();i++) {
    BaseString msg;
    DBUG_PRINT("info",("Setting dynamic port %d->%d : %d",
		       reg->get_localNodeId(),
		       reg->m_transporter_interface[i].m_remote_nodeId,
		       reg->m_transporter_interface[i].m_s_service_port
		       )
	       );
    int res = setConnectionDbParameter((int)reg->get_localNodeId(),
				       (int)reg->m_transporter_interface[i]
				            .m_remote_nodeId,
				       (int)CFG_CONNECTION_SERVER_PORT,
				       reg->m_transporter_interface[i]
				            .m_s_service_port,
					 msg);
    DBUG_PRINT("info",("Set result: %d: %s",res,msg.c_str()));
  }

  _ownReference = numberToRef(_blockNumber, _ownNodeId);
  
  startEventLog();
  // Set the initial confirmation count for subscribe requests confirm
  // from NDB nodes in the cluster.
  //
  // Loglevel thread
  _logLevelThread = NdbThread_Create(logLevelThread_C,
				     (void**)this,
                                     0, // default stack size
				     "MgmtSrvr_Loglevel",
				     NDB_THREAD_PRIO_LOW);

  DBUG_RETURN(true);
}


//****************************************************************************
//****************************************************************************
MgmtSrvr::~MgmtSrvr() 
{
  if(theFacade != 0){
    theFacade->stop_instance();
    delete theFacade;
    theFacade = 0;
  }

  stopEventLog();

  NdbMutex_Destroy(m_node_id_mutex);
  NdbCondition_Destroy(theMgmtWaitForResponseCondPtr);
  NdbMutex_Destroy(m_configMutex);

  if(m_newConfig != NULL)
    free(m_newConfig);

  if(_config != NULL)
    delete _config;

  // End set log level thread
  void* res = 0;
  _isStopThread = true;

  if (_logLevelThread != NULL) {
    NdbThread_WaitFor(_logLevelThread, &res);
    NdbThread_Destroy(&_logLevelThread);
  }

  if (m_config_retriever)
    delete m_config_retriever;
}

//****************************************************************************
//****************************************************************************

int MgmtSrvr::okToSendTo(NodeId nodeId, bool unCond) 
{
  if(nodeId == 0 || getNodeType(nodeId) != NDB_MGM_NODE_TYPE_NDB)
    return WRONG_PROCESS_TYPE;
  // Check if we have contact with it
  if(unCond){
    if(theFacade->theClusterMgr->getNodeInfo(nodeId).m_api_reg_conf)
      return 0;
  }
  else if (theFacade->get_node_alive(nodeId) == true)
    return 0;
  return NO_CONTACT_WITH_PROCESS;
}

void report_unknown_signal(SimpleSignal *signal)
{
  g_eventLogger->error("Unknown signal received. SignalNumber: "
                       "%i from (%d, %x)",
                       signal->readSignalNumber(),
                       refToNode(signal->header.theSendersBlockRef),
                       refToBlock(signal->header.theSendersBlockRef));
}

/*****************************************************************************
 * Starting and stopping database nodes
 ****************************************************************************/

int 
MgmtSrvr::start(int nodeId)
{
  INIT_SIGNAL_SENDER(ss,nodeId);
  
  SimpleSignal ssig;
  StartOrd* const startOrd = CAST_PTR(StartOrd, ssig.getDataPtrSend());
  ssig.set(ss,TestOrd::TraceAPI, CMVMI, GSN_START_ORD, StartOrd::SignalLength);
  startOrd->restartInfo = 0;
  
  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}

/*****************************************************************************
 * Version handling
 *****************************************************************************/

int 
MgmtSrvr::versionNode(int nodeId, Uint32 &version, Uint32& mysql_version,
		      const char **address)
{
  version= 0;
  mysql_version = 0;
  if (getOwnNodeId() == nodeId)
  {
    /**
     * If we're inquiring about our own node id,
     * We know what version we are (version implies connected for mgm)
     * but would like to find out from elsewhere what address they're using
     * to connect to us. This means that secondary mgm servers
     * can list ip addresses for mgm servers.
     *
     * If we don't get an address (i.e. no db nodes),
     * we get the address from the configuration.
     */
    sendVersionReq(nodeId, version, mysql_version, address);
    version= NDB_VERSION;
    mysql_version = NDB_MYSQL_VERSION_D;
    if(!*address)
    {
      ndb_mgm_configuration_iterator
	iter(*_config->m_configValues, CFG_SECTION_NODE);
      unsigned tmp= 0;
      for(iter.first();iter.valid();iter.next())
      {
	if(iter.get(CFG_NODE_ID, &tmp)) require(false);
	if((unsigned)nodeId!=tmp)
	  continue;
	if(iter.get(CFG_NODE_HOST, address)) require(false);
	break;
      }
    }
  }
  else if (getNodeType(nodeId) == NDB_MGM_NODE_TYPE_NDB)
  {
    ClusterMgr::Node node= theFacade->theClusterMgr->getNodeInfo(nodeId);
    if(node.connected)
    {
      version= node.m_info.m_version;
      mysql_version = node.m_info.m_mysql_version;
    }
    *address= get_connect_address(nodeId);
  }
  else if (getNodeType(nodeId) == NDB_MGM_NODE_TYPE_API ||
	   getNodeType(nodeId) == NDB_MGM_NODE_TYPE_MGM)
  {
    return sendVersionReq(nodeId, version, mysql_version, address);
  }

  return 0;
}

int 
MgmtSrvr::sendVersionReq(int v_nodeId, 
			 Uint32 &version, 
			 Uint32& mysql_version,
			 const char **address)
{
  SignalSender ss(theFacade);
  ss.lock();

  SimpleSignal ssig;
  ApiVersionReq* req = CAST_PTR(ApiVersionReq, ssig.getDataPtrSend());
  req->senderRef = ss.getOwnRef();
  req->nodeId = v_nodeId;
  ssig.set(ss, TestOrd::TraceAPI, QMGR, GSN_API_VERSION_REQ, 
	   ApiVersionReq::SignalLength);

  int do_send = 1;
  NodeId nodeId;

  while (1)
  {
    if (do_send)
    {
      bool next;
      nodeId = 0;

      while((next = getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) == true &&
	    okToSendTo(nodeId, true) != 0);

      const ClusterMgr::Node &node=
	theFacade->theClusterMgr->getNodeInfo(nodeId);
      if(next && node.m_state.startLevel != NodeState::SL_STARTED)
      {
	NodeId tmp=nodeId;
	while((next = getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) == true &&
	      okToSendTo(nodeId, true) != 0);
	if(!next)
	  nodeId= tmp;
      }

      if(!next) return NO_CONTACT_WITH_DB_NODES;

      if (ss.sendSignal(nodeId, &ssig) != SEND_OK) {
	return SEND_OR_RECEIVE_FAILED;
      }
      do_send = 0;
    }

    SimpleSignal *signal = ss.waitFor();

    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_API_VERSION_CONF: {
      const ApiVersionConf * const conf = 
	CAST_CONSTPTR(ApiVersionConf, signal->getDataPtr());
      assert((int) conf->nodeId == v_nodeId);
      version = conf->version;
      mysql_version = conf->mysql_version;
      if (version < NDBD_SPLIT_VERSION)
	mysql_version = 0;
      struct in_addr in;
      in.s_addr= conf->inet_addr;
      *address= inet_ntoa(in);
      return 0;
    }
    case GSN_NF_COMPLETEREP:{
      const NFCompleteRep * const rep =
	CAST_CONSTPTR(NFCompleteRep, signal->getDataPtr());
      if (rep->failedNodeId == nodeId)
	do_send = 1; // retry with other node
      continue;
    }
    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
	CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      if (NdbNodeBitmask::get(rep->theNodes,nodeId))
	do_send = 1; // retry with other node
      continue;
    }
    case GSN_TAKE_OVERTCCONF:
      continue;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
    break;
  } // while(1)

  return 0;
}

int MgmtSrvr::sendStopMgmd(NodeId nodeId,
			   bool abort,
			   bool stop,
			   bool restart,
			   bool nostart,
			   bool initialStart)
{
  const char* hostname;
  Uint32 port;
  BaseString connect_string;

  {
    Guard g(m_configMutex);
    {
      ndb_mgm_configuration_iterator
        iter(* _config->m_configValues, CFG_SECTION_NODE);

      if(iter.first())                       return SEND_OR_RECEIVE_FAILED;
      if(iter.find(CFG_NODE_ID, nodeId))     return SEND_OR_RECEIVE_FAILED;
      if(iter.get(CFG_NODE_HOST, &hostname)) return SEND_OR_RECEIVE_FAILED;
    }
    {
      ndb_mgm_configuration_iterator
        iter(* _config->m_configValues, CFG_SECTION_NODE);

      if(iter.first())                   return SEND_OR_RECEIVE_FAILED;
      if(iter.find(CFG_NODE_ID, nodeId)) return SEND_OR_RECEIVE_FAILED;
      if(iter.get(CFG_MGM_PORT, &port))  return SEND_OR_RECEIVE_FAILED;
    }
    if( strlen(hostname) == 0 )
      return SEND_OR_RECEIVE_FAILED;
  }
  connect_string.assfmt("%s:%u",hostname,port);

  DBUG_PRINT("info",("connect string: %s",connect_string.c_str()));

  NdbMgmHandle h= ndb_mgm_create_handle();
  if ( h && connect_string.length() > 0 )
  {
    ndb_mgm_set_connectstring(h,connect_string.c_str());
    if(ndb_mgm_connect(h,1,0,0))
    {
      DBUG_PRINT("info",("failed ndb_mgm_connect"));
      ndb_mgm_destroy_handle(&h);
      return SEND_OR_RECEIVE_FAILED;
    }
    if(!restart)
    {
      int nodes[1];
      nodes[0]= (int)nodeId;
      if(ndb_mgm_stop(h, 1, nodes) < 0)
      {
        ndb_mgm_destroy_handle(&h);
        return SEND_OR_RECEIVE_FAILED;
      }
    }
    else
    {
      int nodes[1];
      nodes[0]= (int)nodeId;
      if(ndb_mgm_restart2(h, 1, nodes, initialStart, nostart, abort) < 0)
      {
        ndb_mgm_destroy_handle(&h);
        return SEND_OR_RECEIVE_FAILED;
      }
    }
  }
  ndb_mgm_destroy_handle(&h);

  return 0;
}

/*
 * Common method for handeling all STOP_REQ signalling that
 * is used by Stopping, Restarting and Single user commands
 *
 * In the event that we need to stop a mgmd, we create a mgm
 * client connection to that mgmd and stop it that way.
 * This allows us to stop mgm servers when there isn't any real
 * distributed communication up.
 *
 * node_ids.size()==0 means to stop all DB nodes.
 *                    MGM nodes will *NOT* be stopped.
 *
 * If we work out we should be stopping or restarting ourselves,
 * we return <0 in stopSelf for restart, >0 for stop
 * and 0 for do nothing.
 */

int MgmtSrvr::sendSTOP_REQ(const Vector<NodeId> &node_ids,
			   NdbNodeBitmask &stoppedNodes,
			   Uint32 singleUserNodeId,
			   bool abort,
			   bool stop,
			   bool restart,
			   bool nostart,
			   bool initialStart,
                           int* stopSelf)
{
  int error = 0;
  DBUG_ENTER("MgmtSrvr::sendSTOP_REQ");
  DBUG_PRINT("enter", ("no of nodes: %d  singleUseNodeId: %d  "
                       "abort: %d  stop: %d  restart: %d  "
                       "nostart: %d  initialStart: %d",
                       node_ids.size(), singleUserNodeId,
                       abort, stop, restart, nostart, initialStart));

  stoppedNodes.clear();

  SignalSender ss(theFacade);
  ss.lock(); // lock will be released on exit

  SimpleSignal ssig;
  StopReq* const stopReq = CAST_PTR(StopReq, ssig.getDataPtrSend());
  ssig.set(ss, TestOrd::TraceAPI, NDBCNTR, GSN_STOP_REQ, StopReq::SignalLength);

  NdbNodeBitmask notstarted;
  for (Uint32 i = 0; i<node_ids.size(); i++)
  {
    Uint32 nodeId = node_ids[i];
    ClusterMgr::Node node = theFacade->theClusterMgr->getNodeInfo(nodeId);
    if (node.m_state.startLevel != NodeState::SL_STARTED)
      notstarted.set(nodeId);
  }
  
  stopReq->requestInfo = 0;
  stopReq->apiTimeout = 5000;
  stopReq->transactionTimeout = 1000;
  stopReq->readOperationTimeout = 1000;
  stopReq->operationTimeout = 1000;
  stopReq->senderData = 12;
  stopReq->senderRef = ss.getOwnRef();
  if (singleUserNodeId)
  {
    stopReq->singleuser = 1;
    stopReq->singleUserApi = singleUserNodeId;
    StopReq::setSystemStop(stopReq->requestInfo, false);
    StopReq::setPerformRestart(stopReq->requestInfo, false);
    StopReq::setStopAbort(stopReq->requestInfo, false);
  }
  else
  {
    stopReq->singleuser = 0;
    StopReq::setSystemStop(stopReq->requestInfo, stop);
    StopReq::setPerformRestart(stopReq->requestInfo, restart);
    StopReq::setStopAbort(stopReq->requestInfo, abort);
    StopReq::setNoStart(stopReq->requestInfo, nostart);
    StopReq::setInitialStart(stopReq->requestInfo, initialStart);
  }

  // send the signals
  NdbNodeBitmask nodes;
  NodeId nodeId= 0;
  int use_master_node= 0;
  int do_send= 0;
  *stopSelf= 0;
  NdbNodeBitmask nodes_to_stop;
  {
    for (unsigned i= 0; i < node_ids.size(); i++)
    {
      nodeId= node_ids[i];
      ndbout << "asked to stop " << nodeId << endl;

      if ((getNodeType(nodeId) != NDB_MGM_NODE_TYPE_MGM)
          &&(getNodeType(nodeId) != NDB_MGM_NODE_TYPE_NDB))
          return WRONG_PROCESS_TYPE;

      if (getNodeType(nodeId) != NDB_MGM_NODE_TYPE_MGM)
        nodes_to_stop.set(nodeId);
      else if (nodeId != getOwnNodeId())
      {
        error= sendStopMgmd(nodeId, abort, stop, restart,
                            nostart, initialStart);
        if (error == 0)
          stoppedNodes.set(nodeId);
      }
      else
      {
        ndbout << "which is me" << endl;
        *stopSelf= (restart)? -1 : 1;
        stoppedNodes.set(nodeId);
      }
    }
  }
  int no_of_nodes_to_stop= nodes_to_stop.count();
  if (node_ids.size())
  {
    if (no_of_nodes_to_stop)
    {
      do_send= 1;
      if (no_of_nodes_to_stop == 1)
      {
        nodeId= nodes_to_stop.find(0);
      }
      else // multi node stop, send to master
      {
        use_master_node= 1;
        nodes_to_stop.copyto(NdbNodeBitmask::Size, stopReq->nodes);
        StopReq::setStopNodes(stopReq->requestInfo, 1);
      }
    }
  }
  else
  {
    nodeId= 0;
    while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB))
    {
      if(okToSendTo(nodeId, true) == 0)
      {
	SendStatus result = ss.sendSignal(nodeId, &ssig);
	if (result == SEND_OK)
	  nodes.set(nodeId);
      }
    }
  }

  // now wait for the replies
  while (!nodes.isclear() || do_send)
  {
    if (do_send)
    {
      int r;
      assert(nodes.count() == 0);
      if (use_master_node)
        nodeId= m_master_node;
      if ((r= okToSendTo(nodeId, true)) != 0)
      {
        bool next;
        if (!use_master_node)
          DBUG_RETURN(r);
        m_master_node= nodeId= 0;
        while((next= getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) == true &&
              (r= okToSendTo(nodeId, true)) != 0);
        if (!next)
          DBUG_RETURN(NO_CONTACT_WITH_DB_NODES);
      }
      if (ss.sendSignal(nodeId, &ssig) != SEND_OK)
        DBUG_RETURN(SEND_OR_RECEIVE_FAILED);
      nodes.set(nodeId);
      do_send= 0;
    }
    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_STOP_REF:{
      const StopRef * const ref = CAST_CONSTPTR(StopRef, signal->getDataPtr());
      const NodeId nodeId = refToNode(signal->header.theSendersBlockRef);
#ifdef VM_TRACE
      ndbout_c("Node %d refused stop", nodeId);
#endif
      assert(nodes.get(nodeId));
      nodes.clear(nodeId);
      if (ref->errorCode == StopRef::MultiNodeShutdownNotMaster)
      {
        assert(use_master_node);
        m_master_node= ref->masterNodeId;
        do_send= 1;
        continue;
      }
      error = translateStopRef(ref->errorCode);
      break;
    }
    case GSN_STOP_CONF:{
#ifdef NOT_USED
      const StopConf * const ref = CAST_CONSTPTR(StopConf, signal->getDataPtr());
#endif
      const NodeId nodeId = refToNode(signal->header.theSendersBlockRef);
#ifdef VM_TRACE
      ndbout_c("Node %d single user mode", nodeId);
#endif
      assert(nodes.get(nodeId));
      if (singleUserNodeId != 0)
      {
        stoppedNodes.set(nodeId);
      }
      else
      {
        assert(no_of_nodes_to_stop > 1);
        stoppedNodes.bitOR(nodes_to_stop);
      }
      nodes.clear(nodeId);
      break;
    }
    case GSN_NF_COMPLETEREP:{
      const NFCompleteRep * const rep =
	CAST_CONSTPTR(NFCompleteRep, signal->getDataPtr());
#ifdef VM_TRACE
      ndbout_c("sendSTOP_REQ Node %d fail completed", rep->failedNodeId);
#endif
      nodes.clear(rep->failedNodeId); // clear the failed node
      if (singleUserNodeId == 0)
        stoppedNodes.set(rep->failedNodeId);
      break;
    }
    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
	CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      NdbNodeBitmask mask;
      mask.assign(NdbNodeBitmask::Size, rep->theNodes);
      mask.bitANDC(notstarted);
      nodes.bitANDC(mask);
      
      if (singleUserNodeId == 0)
	stoppedNodes.bitOR(mask);
      break;
    }
    case GSN_TAKE_OVERTCCONF:
      continue;
    default:
      report_unknown_signal(signal);
#ifdef VM_TRACE
      ndbout_c("Unknown signal %d", gsn);
#endif
      DBUG_RETURN(SEND_OR_RECEIVE_FAILED);
    }
  }
  if (error && *stopSelf)
  {
    *stopSelf= 0;
  }
  DBUG_RETURN(error);
}

/*
 * Stop one nodes
 */

int MgmtSrvr::stopNodes(const Vector<NodeId> &node_ids,
                        int *stopCount, bool abort, int* stopSelf)
{
  /*
    verify that no nodes are starting before stopping as this would cause
    the starting node to shutdown
  */
  if (!abort && check_nodes_starting())
    return OPERATION_NOT_ALLOWED_START_STOP;

  NdbNodeBitmask nodes;
  int ret= sendSTOP_REQ(node_ids,
                        nodes,
                        0,
                        abort,
                        false,
                        false,
                        false,
                        false,
                        stopSelf);
  if (stopCount)
    *stopCount= nodes.count();
  return ret;
}

int MgmtSrvr::shutdownMGM(int *stopCount, bool abort, int *stopSelf)
{
  NodeId nodeId = 0;
  int error;

  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_MGM))
  {
    if(nodeId==getOwnNodeId())
      continue;
    error= sendStopMgmd(nodeId, abort, true, false,
                        false, false);
    if (error == 0)
      *stopCount++;
  }

  *stopSelf= 1;
  *stopCount++;

  return 0;
}

/*
 * Perform DB nodes shutdown.
 * MGM servers are left in their current state
 */

int MgmtSrvr::shutdownDB(int * stopCount, bool abort)
{
  NdbNodeBitmask nodes;
  Vector<NodeId> node_ids;

  int tmp;

  int ret = sendSTOP_REQ(node_ids,
			 nodes,
			 0,
			 abort,
			 true,
			 false,
			 false,
			 false,
                         &tmp);
  if (stopCount)
    *stopCount = nodes.count();
  return ret;
}

/*
 * Enter single user mode on all live nodes
 */

int MgmtSrvr::enterSingleUser(int * stopCount, Uint32 singleUserNodeId)
{
  if (getNodeType(singleUserNodeId) != NDB_MGM_NODE_TYPE_API)
    return NODE_NOT_API_NODE;
  NdbNodeBitmask nodes;
  Vector<NodeId> node_ids;
  int stopSelf;
  int ret = sendSTOP_REQ(node_ids,
			 nodes,
			 singleUserNodeId,
			 false,
			 false,
			 false,
			 false,
			 false,
                         &stopSelf);
  if (stopCount)
    *stopCount = nodes.count();
  return ret;
}

/*
 * Perform node restart
 */

int MgmtSrvr::check_nodes_stopping()
{
  NodeId nodeId = 0;
  ClusterMgr::Node node;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB))
  {
    node = theFacade->theClusterMgr->getNodeInfo(nodeId);
    if((node.m_state.startLevel == NodeState::SL_STOPPING_1) || 
       (node.m_state.startLevel == NodeState::SL_STOPPING_2) || 
       (node.m_state.startLevel == NodeState::SL_STOPPING_3) || 
       (node.m_state.startLevel == NodeState::SL_STOPPING_4))
      return 1;
  }
  return 0;
}

int MgmtSrvr::check_nodes_starting()
{
  NodeId nodeId = 0;
  ClusterMgr::Node node;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB))
  {
    node = theFacade->theClusterMgr->getNodeInfo(nodeId);
    if((node.m_state.startLevel == NodeState::SL_STARTING))
      return 1;
  }
  return 0;
}

int MgmtSrvr::check_nodes_single_user()
{
  NodeId nodeId = 0;
  ClusterMgr::Node node;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB))
  {
    node = theFacade->theClusterMgr->getNodeInfo(nodeId);
    if((node.m_state.startLevel == NodeState::SL_SINGLEUSER))
      return 1;
  }
  return 0;
}

int MgmtSrvr::restartNodes(const Vector<NodeId> &node_ids,
                           int * stopCount, bool nostart,
                           bool initialStart, bool abort,
                           int *stopSelf)
{
  /*
    verify that no nodes are starting before stopping as this would cause
    the starting node to shutdown
    check single user mode
  */
  if ((!abort && check_nodes_starting()) || check_nodes_single_user())
    return OPERATION_NOT_ALLOWED_START_STOP;

  NdbNodeBitmask nodes;
  int ret= sendSTOP_REQ(node_ids,
                        nodes,
                        0,
                        abort,
                        false,
                        true,
                        true,
                        initialStart,
                        stopSelf);

  if (ret)
    return ret;

  if (stopCount)
    *stopCount = nodes.count();
  
  // start up the nodes again
  int waitTime = 12000;
  NDB_TICKS maxTime = NdbTick_CurrentMillisecond() + waitTime;
  for (unsigned i = 0; i < node_ids.size(); i++)
  {
    NodeId nodeId= node_ids[i];
    enum ndb_mgm_node_status s;
    s = NDB_MGM_NODE_STATUS_NO_CONTACT;
#ifdef VM_TRACE
    ndbout_c("Waiting for %d not started", nodeId);
#endif
    while (s != NDB_MGM_NODE_STATUS_NOT_STARTED && waitTime > 0)
    {
      Uint32 startPhase = 0, version = 0, dynamicId = 0, nodeGroup = 0;
      Uint32 mysql_version = 0;
      Uint32 connectCount = 0;
      bool system;
      const char *address;
      status(nodeId, &s, &version, &mysql_version, &startPhase, 
             &system, &dynamicId, &nodeGroup, &connectCount, &address);
      NdbSleep_MilliSleep(100);  
      waitTime = (maxTime - NdbTick_CurrentMillisecond());
    }
  }

  if (nostart)
    return 0;

  /*
    verify that no nodes are stopping before starting as this would cause
    the starting node to shutdown
  */
  int retry= 600*10;
  for (;check_nodes_stopping();)
  {
    if (--retry)
      break;
    NdbSleep_MilliSleep(100);
  }

  /*
    start the nodes
  */
  for (unsigned i = 0; i < node_ids.size(); i++)
  {
    (void) start(node_ids[i]);
  }
  return 0;
}

/*
 * Perform restart of all DB nodes
 */

int MgmtSrvr::restartDB(bool nostart, bool initialStart,
                        bool abort, int * stopCount)
{
  NdbNodeBitmask nodes;
  Vector<NodeId> node_ids;
  int tmp;

  int ret = sendSTOP_REQ(node_ids,
			 nodes,
			 0,
			 abort,
			 true,
			 true,
			 true,
			 initialStart,
                         &tmp);

  if (ret)
    return ret;

  if (stopCount)
    *stopCount = nodes.count();

#ifdef VM_TRACE
    ndbout_c("Stopped %d nodes", nodes.count());
#endif
  /**
   * Here all nodes were correctly stopped,
   * so we wait for all nodes to be contactable
   */
  int waitTime = 12000;
  NodeId nodeId = 0;
  NDB_TICKS maxTime = NdbTick_CurrentMillisecond() + waitTime;

  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) {
    if (!nodes.get(nodeId))
      continue;
    enum ndb_mgm_node_status s;
    s = NDB_MGM_NODE_STATUS_NO_CONTACT;
#ifdef VM_TRACE
    ndbout_c("Waiting for %d not started", nodeId);
#endif
    while (s != NDB_MGM_NODE_STATUS_NOT_STARTED && waitTime > 0) {
      Uint32 startPhase = 0, version = 0, dynamicId = 0, nodeGroup = 0;
      Uint32 mysql_version = 0;
      Uint32 connectCount = 0;
      bool system;
      const char *address;
      status(nodeId, &s, &version, &mysql_version, &startPhase, 
	     &system, &dynamicId, &nodeGroup, &connectCount, &address);
      NdbSleep_MilliSleep(100);  
      waitTime = (maxTime - NdbTick_CurrentMillisecond());
    }
  }
  
  if(nostart)
    return 0;
  
  /**
   * Now we start all database nodes (i.e. we make them non-idle)
   * We ignore the result we get from the start command.
   */
  nodeId = 0;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) {
    if (!nodes.get(nodeId))
      continue;
    int result;
    result = start(nodeId);
    DEBUG("Starting node " << nodeId << " with result " << result);
    /**
     * Errors from this call are deliberately ignored.
     * Maybe the user only wanted to restart a subset of the nodes.
     * It is also easy for the user to check which nodes have 
     * started and which nodes have not.
     */
  }
  
  return 0;
}

int
MgmtSrvr::exitSingleUser(int * stopCount, bool abort)
{
  NodeId nodeId = 0;
  int count = 0;

  SignalSender ss(theFacade);
  ss.lock(); // lock will be released on exit

  SimpleSignal ssig;
  ResumeReq* const resumeReq = 
    CAST_PTR(ResumeReq, ssig.getDataPtrSend());
  ssig.set(ss,TestOrd::TraceAPI, NDBCNTR, GSN_RESUME_REQ, 
	   ResumeReq::SignalLength);
  resumeReq->senderData = 12;
  resumeReq->senderRef = ss.getOwnRef();

  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)){
    if(okToSendTo(nodeId, true) == 0){
      SendStatus result = ss.sendSignal(nodeId, &ssig);
      if (result == SEND_OK)
	count++;
    }
  }

  if(stopCount != 0)
    * stopCount = count;

  return 0;
}

/*****************************************************************************
 * Status
 ****************************************************************************/

#include <ClusterMgr.hpp>

void
MgmtSrvr::updateStatus()
{
  theFacade->theClusterMgr->forceHB();
}

int 
MgmtSrvr::status(int nodeId, 
                 ndb_mgm_node_status * _status, 
		 Uint32 * version,
		 Uint32 * mysql_version,
		 Uint32 * _phase, 
		 bool * _system,
		 Uint32 * dynamic,
		 Uint32 * nodegroup,
		 Uint32 * connectCount,
		 const char **address)
{
  if (getNodeType(nodeId) == NDB_MGM_NODE_TYPE_API ||
      getNodeType(nodeId) == NDB_MGM_NODE_TYPE_MGM) {
    versionNode(nodeId, *version, *mysql_version, address);
  } else {
    *address= get_connect_address(nodeId);
  }

  const ClusterMgr::Node node = 
    theFacade->theClusterMgr->getNodeInfo(nodeId);

  if(!node.connected){
    * _status = NDB_MGM_NODE_STATUS_NO_CONTACT;
    return 0;
  }
  
  if (getNodeType(nodeId) == NDB_MGM_NODE_TYPE_NDB) {
    * version = node.m_info.m_version;
    * mysql_version = node.m_info.m_mysql_version;
  }

  * dynamic = node.m_state.dynamicId;
  * nodegroup = node.m_state.nodeGroup;
  * connectCount = node.m_info.m_connectCount;
  
  switch(node.m_state.startLevel){
  case NodeState::SL_CMVMI:
    * _status = NDB_MGM_NODE_STATUS_NOT_STARTED;
    * _phase = 0;
    return 0;
    break;
  case NodeState::SL_STARTING:
    * _status     = NDB_MGM_NODE_STATUS_STARTING;
    * _phase = node.m_state.starting.startPhase;
    return 0;
    break;
  case NodeState::SL_STARTED:
    * _status = NDB_MGM_NODE_STATUS_STARTED;
    * _phase = 0;
    return 0;
    break;
  case NodeState::SL_STOPPING_1:
    * _status = NDB_MGM_NODE_STATUS_SHUTTING_DOWN;
    * _phase = 1;
    * _system = node.m_state.stopping.systemShutdown != 0;
    return 0;
    break;
  case NodeState::SL_STOPPING_2:
    * _status = NDB_MGM_NODE_STATUS_SHUTTING_DOWN;
    * _phase = 2;
    * _system = node.m_state.stopping.systemShutdown != 0;
    return 0;
    break;
  case NodeState::SL_STOPPING_3:
    * _status = NDB_MGM_NODE_STATUS_SHUTTING_DOWN;
    * _phase = 3;
    * _system = node.m_state.stopping.systemShutdown != 0;
    return 0;
    break;
  case NodeState::SL_STOPPING_4:
    * _status = NDB_MGM_NODE_STATUS_SHUTTING_DOWN;
    * _phase = 4;
    * _system = node.m_state.stopping.systemShutdown != 0;
    return 0;
    break;
  case NodeState::SL_SINGLEUSER:
    * _status = NDB_MGM_NODE_STATUS_SINGLEUSER;
    * _phase  = 0;
    return 0;
    break;
  default:
    * _status = NDB_MGM_NODE_STATUS_UNKNOWN;
    * _phase = 0;
    return 0;
  }
  
  return -1;
}

int 
MgmtSrvr::setEventReportingLevelImpl(int nodeId_arg, 
				     const EventSubscribeReq& ll)
{
  SignalSender ss(theFacade);
  NdbNodeBitmask nodes;
  nodes.clear();
  while (1)
  {
    Uint32 nodeId, max;
    ss.lock();
    SimpleSignal ssig;
    EventSubscribeReq * dst = 
      CAST_PTR(EventSubscribeReq, ssig.getDataPtrSend());
    ssig.set(ss,TestOrd::TraceAPI, CMVMI, GSN_EVENT_SUBSCRIBE_REQ,
             EventSubscribeReq::SignalLength);
    *dst = ll;

    if (nodeId_arg == 0)
    {
      // all nodes
      nodeId = 1;
      max = MAX_NDB_NODES;
    }
    else
    {
      // only one node
      max = nodeId = nodeId_arg;
    }
    // first make sure nodes are sendable
    for(; nodeId <= max; nodeId++)
    {
      if (nodeTypes[nodeId] != NODE_TYPE_DB)
        continue;
      if (okToSendTo(nodeId, true))
      {
        if (theFacade->theClusterMgr->getNodeInfo(nodeId).connected  == false)
        {
          // node not connected we can safely skip this one
          continue;
        }
        // api_reg_conf not recevied yet, need to retry
        return SEND_OR_RECEIVE_FAILED;
      }
    }

    if (nodeId_arg == 0)
    {
      // all nodes
      nodeId = 1;
      max = MAX_NDB_NODES;
    }
    else
    {
      // only one node
      max = nodeId = nodeId_arg;
    }
    // now send to all sendable nodes nodes
    // note, lock is held, so states have not changed
    for(; (Uint32) nodeId <= max; nodeId++)
    {
      if (nodeTypes[nodeId] != NODE_TYPE_DB)
        continue;
      if (theFacade->theClusterMgr->getNodeInfo(nodeId).connected  == false)
        continue; // node is not connected, skip
      if (ss.sendSignal(nodeId, &ssig) == SEND_OK)
        nodes.set(nodeId);
      else if (max == nodeId)
      {
        return SEND_OR_RECEIVE_FAILED;
      }
    }
    break;
  }

  if (nodes.isclear())
  {
    return SEND_OR_RECEIVE_FAILED;
  }
  
  int error = 0;
  while (!nodes.isclear())
  {
    Uint32 nodeId;
    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();
    nodeId = refToNode(signal->header.theSendersBlockRef);
    switch (gsn) {
    case GSN_EVENT_SUBSCRIBE_CONF:{
      nodes.clear(nodeId);
      break;
    }
    case GSN_EVENT_SUBSCRIBE_REF:{
      nodes.clear(nodeId);
      error = 1;
      break;
    }
      // Since sending okToSend(true), 
      // there is no guarantee that NF_COMPLETEREP will come
      // i.e listen also to NODE_FAILREP
    case GSN_NODE_FAILREP: {
      const NodeFailRep * const rep =
	CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      NdbNodeBitmask mask;
      mask.assign(NdbNodeBitmask::Size, rep->theNodes);
      nodes.bitANDC(mask);
      break;
    }
      
    case GSN_NF_COMPLETEREP:{
      const NFCompleteRep * const rep =
	CAST_CONSTPTR(NFCompleteRep, signal->getDataPtr());
      nodes.clear(rep->failedNodeId);
      break;
    }
    case GSN_TAKE_OVERTCCONF:
      continue;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }
  if (error)
    return SEND_OR_RECEIVE_FAILED;
  return 0;
}

//****************************************************************************
//****************************************************************************
int 
MgmtSrvr::setNodeLogLevelImpl(int nodeId, const SetLogLevelOrd & ll)
{
  INIT_SIGNAL_SENDER(ss,nodeId);

  SimpleSignal ssig;
  ssig.set(ss,TestOrd::TraceAPI, CMVMI, GSN_SET_LOGLEVELORD,
	   SetLogLevelOrd::SignalLength);
  SetLogLevelOrd* const dst = CAST_PTR(SetLogLevelOrd, ssig.getDataPtrSend());
  *dst = ll;
  
  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}

//****************************************************************************
//****************************************************************************

int 
MgmtSrvr::insertError(int nodeId, int errorNo) 
{
  int block;

  if (errorNo < 0) {
    return INVALID_ERROR_NUMBER;
  }

  SignalSender ss(theFacade);
  ss.lock(); /* lock will be released on exit */

  if(getNodeType(nodeId) == NDB_MGM_NODE_TYPE_NDB)
  {
    block= CMVMI;
  }
  else if(nodeId == _ownNodeId)
  {
    g_errorInsert= errorNo;
    return 0;
  }
  else if(getNodeType(nodeId) == NDB_MGM_NODE_TYPE_MGM)
    block= _blockNumber;
  else
    return WRONG_PROCESS_TYPE;

  SimpleSignal ssig;
  ssig.set(ss,TestOrd::TraceAPI, block, GSN_TAMPER_ORD, 
	   TamperOrd::SignalLength);
  TamperOrd* const tamperOrd = CAST_PTR(TamperOrd, ssig.getDataPtrSend());
  tamperOrd->errorNo = errorNo;

  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}



//****************************************************************************
//****************************************************************************

int 
MgmtSrvr::setTraceNo(int nodeId, int traceNo)
{
  if (traceNo < 0) {
    return INVALID_TRACE_NUMBER;
  }

  INIT_SIGNAL_SENDER(ss,nodeId);

  SimpleSignal ssig;
  ssig.set(ss,TestOrd::TraceAPI, CMVMI, GSN_TEST_ORD, TestOrd::SignalLength);
  TestOrd* const testOrd = CAST_PTR(TestOrd, ssig.getDataPtrSend());
  testOrd->clear();
  // Assume TRACE command causes toggling. Not really defined... ? TODO
  testOrd->setTraceCommand(TestOrd::Toggle, 
			   (TestOrd::TraceSpecification)traceNo);

  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}

//****************************************************************************
//****************************************************************************

int 
MgmtSrvr::getBlockNumber(const BaseString &blockName) 
{
  short bno = getBlockNo(blockName.c_str());
  if(bno != 0)
    return bno;
  return -1;
}

//****************************************************************************
//****************************************************************************

int 
MgmtSrvr::setSignalLoggingMode(int nodeId, LogMode mode, 
			       const Vector<BaseString>& blocks)
{
  INIT_SIGNAL_SENDER(ss,nodeId);

  // Convert from MgmtSrvr format...

  TestOrd::Command command;
  if (mode == Off) {
    command = TestOrd::Off;
  }
  else {
    command = TestOrd::On;
  }

  TestOrd::SignalLoggerSpecification logSpec;
  switch (mode) {
  case In:
    logSpec = TestOrd::InputSignals;
    break;
  case Out:
    logSpec = TestOrd::OutputSignals;
    break;
  case InOut:
    logSpec = TestOrd::InputOutputSignals;
    break;
  case Off:
    // In MgmtSrvr interface it's just possible to switch off all logging, both
    // "in" and "out" (this should probably be changed).
    logSpec = TestOrd::InputOutputSignals;
    break;
  default:
    ndbout_c("Unexpected value %d, MgmtSrvr::setSignalLoggingMode, line %d",
	     (unsigned)mode, __LINE__);
    assert(false);
    return -1;
  }

  SimpleSignal ssig;
  ssig.set(ss,TestOrd::TraceAPI, CMVMI, GSN_TEST_ORD, TestOrd::SignalLength);

  TestOrd* const testOrd = CAST_PTR(TestOrd, ssig.getDataPtrSend());
  testOrd->clear();
  
  if (blocks.size() == 0 || blocks[0] == "ALL") {
    // Logg command for all blocks
    testOrd->addSignalLoggerCommand(command, logSpec);
  } else {
    for(unsigned i = 0; i < blocks.size(); i++){
      int blockNumber = getBlockNumber(blocks[i]);
      if (blockNumber == -1) {
        return INVALID_BLOCK_NAME;
      }
      testOrd->addSignalLoggerCommand(blockNumber, command, logSpec);
    } // for
  } // else

  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}

/*****************************************************************************
 * Signal tracing
 *****************************************************************************/
int MgmtSrvr::startSignalTracing(int nodeId)
{
  INIT_SIGNAL_SENDER(ss,nodeId);
  
  SimpleSignal ssig;
  ssig.set(ss,TestOrd::TraceAPI, CMVMI, GSN_TEST_ORD, TestOrd::SignalLength);

  TestOrd* const testOrd = CAST_PTR(TestOrd, ssig.getDataPtrSend());
  testOrd->clear();
  testOrd->setTestCommand(TestOrd::On);

  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}

int 
MgmtSrvr::stopSignalTracing(int nodeId) 
{
  INIT_SIGNAL_SENDER(ss,nodeId);

  SimpleSignal ssig;
  ssig.set(ss,TestOrd::TraceAPI, CMVMI, GSN_TEST_ORD, TestOrd::SignalLength);
  TestOrd* const testOrd = CAST_PTR(TestOrd, ssig.getDataPtrSend());
  testOrd->clear();
  testOrd->setTestCommand(TestOrd::Off);

  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}


/*****************************************************************************
 * Dump state
 *****************************************************************************/

int
MgmtSrvr::dumpState(int nodeId, const char* args)
{
  // Convert the space separeted args 
  // string to an int array
  Uint32 args_array[25];
  Uint32 numArgs = 0;

  char buf[10];  
  int b  = 0;
  memset(buf, 0, 10);
  for (size_t i = 0; i <= strlen(args); i++){
    if (args[i] == ' ' || args[i] == 0){
      args_array[numArgs] = atoi(buf);
      numArgs++;
      memset(buf, 0, 10);
      b = 0;
    } else {
      buf[b] = args[i];
      b++;
    }    
  }
  
  return dumpState(nodeId, args_array, numArgs);
}

int
MgmtSrvr::dumpState(int nodeId, const Uint32 args[], Uint32 no)
{
  INIT_SIGNAL_SENDER(ss,nodeId);

  const Uint32 len = no > 25 ? 25 : no;
  
  SimpleSignal ssig;
  DumpStateOrd * const dumpOrd = 
    CAST_PTR(DumpStateOrd, ssig.getDataPtrSend());
  ssig.set(ss,TestOrd::TraceAPI, CMVMI, GSN_DUMP_STATE_ORD, len);
  for(Uint32 i = 0; i<25; i++){
    if (i < len)
      dumpOrd->args[i] = args[i];
    else
      dumpOrd->args[i] = 0;
  }
  
  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}


//****************************************************************************
//****************************************************************************

const char* MgmtSrvr::getErrorText(int errorCode, char *buf, int buf_sz)
{
  ndb_error_string(errorCode, buf, buf_sz);
  buf[buf_sz-1]= 0;
  return buf;
}

void 
MgmtSrvr::handleReceivedSignal(NdbApiSignal* signal)
{
  // The way of handling a received signal is taken from the Ndb class.
  int gsn = signal->readSignalNumber();

  switch (gsn) {
  case GSN_EVENT_SUBSCRIBE_CONF:
    break;
  case GSN_EVENT_SUBSCRIBE_REF:
    break;
  case GSN_EVENT_REP:
  {
    eventReport(signal->getDataPtr(), signal->getLength());
    break;
  }

  case GSN_NF_COMPLETEREP:
    break;
  case GSN_NODE_FAILREP:
    break;

  case GSN_TAMPER_ORD:
    ndbout << "TAMPER ORD" << endl;
    break;

  case GSN_TAKE_OVERTCCONF:
    break;

  default:
    g_eventLogger->error("Unknown signal received. SignalNumber: "
                         "%i from (%d, %x)",
                         gsn,
                         refToNode(signal->theSendersBlockRef),
                         refToBlock(signal->theSendersBlockRef));
  }

  if (theWaitState == NO_WAIT) {
    NdbCondition_Signal(theMgmtWaitForResponseCondPtr);
  }
}

void
MgmtSrvr::handleStatus(NodeId nodeId, Uint32 _event)
{
  DBUG_ENTER("MgmtSrvr::handleStatus");
  union {
    Uint32 theData[25];
    EventReport repData;
  };
  EventReport * rep = &repData;
  NS_Event event = (NS_Event)_event;

  theData[1] = nodeId;
  switch(event){
  case NS_CONNECTED:
    DBUG_VOID_RETURN;
  case NS_NODE_ALIVE:
    if (nodeTypes[nodeId] == NODE_TYPE_DB)
    {
      m_started_nodes.push_back(nodeId);
    }
    rep->setEventType(NDB_LE_Connected);
    break;
  case NS_NODE_FAILED:
    rep->setEventType(NDB_LE_Disconnected);
    break;
  case NS_NODE_NF_COMPLETE:
    DBUG_VOID_RETURN;
  }
  rep->setNodeId(_ownNodeId);
  eventReport(theData, 1);
  DBUG_VOID_RETURN;
}

//****************************************************************************
//****************************************************************************

void 
MgmtSrvr::signalReceivedNotification(void* mgmtSrvr, 
                                     NdbApiSignal* signal,
				     LinearSectionPtr ptr[3]) 
{
  ((MgmtSrvr*)mgmtSrvr)->handleReceivedSignal(signal);
}


//****************************************************************************
//****************************************************************************
void 
MgmtSrvr::nodeStatusNotification(void* mgmSrv, Uint32 nodeId, Uint32 event)
{
  DBUG_ENTER("MgmtSrvr::nodeStatusNotification");
  DBUG_PRINT("enter",("nodeid= %d, event: %u", nodeId, event));
  ((MgmtSrvr*)mgmSrv)->handleStatus(nodeId, event);
  DBUG_VOID_RETURN;
}

enum ndb_mgm_node_type 
MgmtSrvr::getNodeType(NodeId nodeId) const 
{
  if(nodeId >= MAX_NODES)
    return (enum ndb_mgm_node_type)-1;
  
  return nodeTypes[nodeId];
}

const char *MgmtSrvr::get_connect_address(Uint32 node_id)
{
  if (m_connect_address[node_id].s_addr == 0 &&
      theFacade && theFacade->theTransporterRegistry &&
      theFacade->theClusterMgr &&
      getNodeType(node_id) == NDB_MGM_NODE_TYPE_NDB) 
  {
    const ClusterMgr::Node &node=
      theFacade->theClusterMgr->getNodeInfo(node_id);
    if (node.connected)
    {
      m_connect_address[node_id]=
	theFacade->theTransporterRegistry->get_connect_address(node_id);
    }
  }
  return inet_ntoa(m_connect_address[node_id]);  
}

void
MgmtSrvr::get_connected_nodes(NodeBitmask &connected_nodes) const
{
  if (theFacade && theFacade->theClusterMgr) 
  {
    for(Uint32 i = 0; i < MAX_NDB_NODES; i++)
    {
      if (getNodeType(i) == NDB_MGM_NODE_TYPE_NDB)
      {
	const ClusterMgr::Node &node= theFacade->theClusterMgr->getNodeInfo(i);
	connected_nodes.bitOR(node.m_state.m_connected_nodes);
      }
    }
  }
}

int
MgmtSrvr::alloc_node_id_req(NodeId free_node_id,
                            enum ndb_mgm_node_type type,
                            Uint32 timeout_ms)
{
  SignalSender ss(theFacade);
  ss.lock(); // lock will be released on exit

  SimpleSignal ssig;
  AllocNodeIdReq* req = CAST_PTR(AllocNodeIdReq, ssig.getDataPtrSend());
  ssig.set(ss, TestOrd::TraceAPI, QMGR, GSN_ALLOC_NODEID_REQ,
           AllocNodeIdReq::SignalLength);
  
  req->senderRef = ss.getOwnRef();
  req->senderData = 19;
  req->nodeId = free_node_id;
  req->nodeType = type;
  req->timeout = timeout_ms;

  int do_send = 1;
  NodeId nodeId = 0;
  while (1)
  {
    if (nodeId == 0)
    {
      bool next;
      while((next = getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) == true &&
            theFacade->get_node_alive(nodeId) == false);
      if (!next)
        return NO_CONTACT_WITH_DB_NODES;
      do_send = 1;
    }
    if (do_send)
    {
      if (ss.sendSignal(nodeId, &ssig) != SEND_OK) {
        return SEND_OR_RECEIVE_FAILED;
      }
      do_send = 0;
    }
    
    SimpleSignal *signal = ss.waitFor();

    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_ALLOC_NODEID_CONF:
    {
#ifdef NOT_USED
      const AllocNodeIdConf * const conf =
        CAST_CONSTPTR(AllocNodeIdConf, signal->getDataPtr());
#endif
      return 0;
    }
    case GSN_ALLOC_NODEID_REF:
    {
      const AllocNodeIdRef * const ref =
        CAST_CONSTPTR(AllocNodeIdRef, signal->getDataPtr());
      if (ref->errorCode == AllocNodeIdRef::NotMaster ||
          ref->errorCode == AllocNodeIdRef::Busy ||
          ref->errorCode == AllocNodeIdRef::NodeFailureHandlingNotCompleted)
      {
        do_send = 1;
        nodeId = refToNode(ref->masterRef);
	if (!theFacade->get_node_alive(nodeId))
	  nodeId = 0;
        if (ref->errorCode != AllocNodeIdRef::NotMaster)
        {
          /* sleep for a while (100ms) before retrying */
          ss.unlock();
          NdbSleep_MilliSleep(100);  
          ss.lock();
        }
        continue;
      }
      return ref->errorCode;
    }
    case GSN_NF_COMPLETEREP:
    {
      continue;
    }
    case GSN_NODE_FAILREP:{
      /**
       * ok to trap using NODE_FAILREP
       *   as we don't really wait on anything interesting
       */
      const NodeFailRep * const rep =
	CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      if (NdbNodeBitmask::get(rep->theNodes, nodeId))
      {
        do_send = 1;
        nodeId = 0;
      }
      continue;
    }
    case GSN_TAKE_OVERTCCONF:
      continue;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }
  return 0;
}

int
MgmtSrvr::match_hostname(const struct sockaddr *clnt_addr,
                         const char *config_hostname) const
{
  struct in_addr config_addr= {0};
  if (clnt_addr)
  {
    const struct in_addr *clnt_in_addr = &((sockaddr_in*)clnt_addr)->sin_addr;

    if (Ndb_getInAddr(&config_addr, config_hostname) != 0
        || memcmp(&config_addr, clnt_in_addr, sizeof(config_addr)) != 0)
    {
      struct in_addr tmp_addr;
      if (Ndb_getInAddr(&tmp_addr, "localhost") != 0
          || memcmp(&tmp_addr, clnt_in_addr, sizeof(config_addr)) != 0)
      {
        // not localhost
#if 0
        ndbout << "MgmtSrvr::getFreeNodeId compare failed for \""
               << config_hostname
               << "\" id=" << tmp << endl;
#endif
        return -1;
      }

      // connecting through localhost
      // check if config_hostname is local
      if (!SocketServer::tryBind(0, config_hostname))
        return -1;
    }
  }
  else
  {
    if (!SocketServer::tryBind(0, config_hostname))
      return -1;
  }
  return 0;
}

int
MgmtSrvr::find_node_type(unsigned node_id, enum ndb_mgm_node_type type,
                         const struct sockaddr *client_addr,
                         NodeBitmask &nodes,
                         NodeBitmask &exact_nodes,
                         Vector<struct nodeid_and_host> &nodes_info,
                         int &error_code, BaseString &error_string)
{
  const char *found_config_hostname= 0;
  unsigned type_c= (unsigned)type;

  Guard g(m_configMutex);

  ndb_mgm_configuration_iterator
    iter(* _config->m_configValues, CFG_SECTION_NODE);
  for(iter.first(); iter.valid(); iter.next())
  {
    unsigned id;
    if (iter.get(CFG_NODE_ID, &id))
      require(false);
    if (node_id && node_id != id)
      continue;
    if (iter.get(CFG_TYPE_OF_SECTION, &type_c))
      require(false);
    if (type_c != (unsigned)type)
    {
      if (!node_id)
        continue;
      goto error;
    }
    const char *config_hostname= 0;
    if (iter.get(CFG_NODE_HOST, &config_hostname))
      require(false);
    if (config_hostname == 0 || config_hostname[0] == 0)
    {
      config_hostname= "";
    }
    else
    {
      found_config_hostname= config_hostname;
      if (match_hostname(client_addr, config_hostname))
      {
        if (!node_id)
          continue;
        goto error;
      }
      exact_nodes.set(id);
    }
    nodes.set(id);
    struct nodeid_and_host a= {id, config_hostname};
    nodes_info.push_back(a);
    if (node_id)
      break;
  }
  if (nodes_info.size() != 0)
  {
    return 0;
  }

 error:
  /*
    lock on m_configMutex held because found_config_hostname may have
    reference inot config structure
  */
  error_code= NDB_MGM_ALLOCID_CONFIG_MISMATCH;
  if (node_id)
  {
    if (type_c != (unsigned) type)
    {
      BaseString type_string, type_c_string;
      const char *alias, *str;
      alias= ndb_mgm_get_node_type_alias_string(type, &str);
      type_string.assfmt("%s(%s)", alias, str);
      alias= ndb_mgm_get_node_type_alias_string((enum ndb_mgm_node_type)type_c,
                                                &str);
      type_c_string.assfmt("%s(%s)", alias, str);
      error_string.appfmt("Id %d configured as %s, connect attempted as %s.",
                          node_id, type_c_string.c_str(),
                          type_string.c_str());
      return -1;
    }
    if (found_config_hostname)
    {
      struct in_addr config_addr= {0};
      int r_config_addr= Ndb_getInAddr(&config_addr, found_config_hostname);
      error_string.appfmt("Connection with id %d done from wrong host ip %s,",
                          node_id, inet_ntoa(((struct sockaddr_in *)
                                              (client_addr))->sin_addr));
      error_string.appfmt(" expected %s(%s).", found_config_hostname,
                          r_config_addr ?
                          "lookup failed" : inet_ntoa(config_addr));
      return -1;
    }
    error_string.appfmt("No node defined with id=%d in config file.", node_id);
    return -1;
  }

  // node_id == 0 and nodes_info.size() == 0
  if (found_config_hostname)
  {
    error_string.appfmt("Connection done from wrong host ip %s.",
                        (client_addr)?
                        inet_ntoa(((struct sockaddr_in *)
                                   (client_addr))->sin_addr):"");
    return -1;
  }

  error_string.append("No nodes defined in config file.");
  return -1;
}

int
MgmtSrvr::try_alloc(unsigned id, const char *config_hostname,
                    enum ndb_mgm_node_type type,
                    const struct sockaddr *client_addr,
                    Uint32 timeout_ms)
{
  if (client_addr != 0)
  {
    int res = alloc_node_id_req(id, type, timeout_ms);
    switch (res)
    {
    case 0:
      // ok continue
      break;
    case NO_CONTACT_WITH_DB_NODES:
      // ok continue
      break;
    default:
      // something wrong
      return -1;
    }
  }

  DBUG_PRINT("info", ("allocating node id %d",id));
  {
    int r= 0;
    if (client_addr)
    {
      m_connect_address[id]= ((struct sockaddr_in *)client_addr)->sin_addr;
    }
    else if (config_hostname)
    {
      r= Ndb_getInAddr(&(m_connect_address[id]), config_hostname);
    }
    else
    {
      char name[256];
      r= gethostname(name, sizeof(name));
      if (r == 0)
      {
        name[sizeof(name)-1]= 0;
        r= Ndb_getInAddr(&(m_connect_address[id]), name);
      }
    }
    if (r)
    {
      m_connect_address[id].s_addr= 0;
    }
  }
  if (theFacade && id != theFacade->ownId())
  {
    /**
     * Make sure we're ready to accept connections from this node
     */
    theFacade->lock_mutex();
    theFacade->doConnect(id);
    theFacade->unlock_mutex();
  }
    
  char tmp_str[128];
  m_reserved_nodes.getText(tmp_str);
  g_eventLogger->info("Mgmt server state: nodeid %d reserved for ip %s, "
                      "m_reserved_nodes %s.",
                      id, get_connect_address(id), tmp_str);

  return 0;
}

bool
MgmtSrvr::alloc_node_id(NodeId * nodeId,
			enum ndb_mgm_node_type type,
			const struct sockaddr *client_addr,
			SOCKET_SIZE_TYPE *client_addr_len,
			int &error_code, BaseString &error_string,
                        int log_event,
                        int timeout_s)
{
  DBUG_ENTER("MgmtSrvr::alloc_node_id");
  DBUG_PRINT("enter", ("nodeid: %d  type: %d  client_addr: 0x%ld",
		       *nodeId, type, (long) client_addr));

  if (g_no_nodeid_checks) {
    if (*nodeId == 0) {
      error_string.appfmt("no-nodeid-checks set in management server. "
			  "node id must be set explicitly in connectstring");
      error_code = NDB_MGM_ALLOCID_CONFIG_MISMATCH;
      DBUG_RETURN(false);
    }
    DBUG_RETURN(true);
  }

  Uint32 timeout_ms = Uint32(1000 * timeout_s);

  Guard g(m_node_id_mutex);

  NodeBitmask connected_nodes;
  get_connected_nodes(connected_nodes);

  NodeBitmask nodes, exact_nodes;
  Vector<struct nodeid_and_host> nodes_info;

  /* find all nodes with correct type */
  if (find_node_type(*nodeId, type, client_addr, nodes, exact_nodes, nodes_info,
                     error_code, error_string))
    goto error;

  // nodes_info.size() == 0 handled inside find_node_type
  DBUG_ASSERT(nodes_info.size() != 0);

  if (type == NDB_MGM_NODE_TYPE_MGM && nodes_info.size() > 1)
  {
    // mgmt server may only have one match
    error_string.appfmt("Ambiguous node id's %d and %d. "
                        "Suggest specifying node id in connectstring, "
                        "or specifying unique host names in config file.",
                        nodes_info[0].id, nodes_info[1].id);
    error_code= NDB_MGM_ALLOCID_CONFIG_MISMATCH;
    goto error;
  }

  /* remove connected and reserved nodes from possible nodes to allocate */
  nodes.bitANDC(connected_nodes);
  nodes.bitANDC(m_reserved_nodes);

  /* first try all nodes with exact match of hostname */
  for (Uint32 i = 0; i < nodes_info.size(); i++)
  {
    unsigned id= nodes_info[i].id;
    if (!nodes.get(id))
      continue;

    if (!exact_nodes.get(id))
      continue;

    const char *config_hostname= nodes_info[i].host.c_str();
    /**
     * set bit as reserved, release mutex, try-alloc reaquire mutex
     * and clear bit if alloc failed
     */
    m_reserved_nodes.set(id);
    NdbMutex_Unlock(m_node_id_mutex);
    if (!try_alloc(id, config_hostname, type, client_addr, timeout_ms))
    {
      NdbMutex_Lock(m_node_id_mutex);
      // success
      *nodeId= id;
      DBUG_RETURN(true);
    }
    NdbMutex_Lock(m_node_id_mutex);
    m_reserved_nodes.clear(id);
  }

  /* now try the open nodes */
  for (Uint32 i = 0; i < nodes_info.size(); i++)
  {
    unsigned id= nodes_info[i].id;
    if (!nodes.get(id))
      continue;

    /**
     * exact node tried in loop above
     */
    if (exact_nodes.get(id))
      continue;

    /**
     * set bit as reserved, release mutex, try-alloc reaquire mutex
     * and clear bit if alloc failed
     */
    m_reserved_nodes.set(id);
    NdbMutex_Unlock(m_node_id_mutex);
    if (!try_alloc(id, NULL, type, client_addr, timeout_ms))
    {
      NdbMutex_Lock(m_node_id_mutex);
      // success
      *nodeId= id;
      DBUG_RETURN(true);
    }
    NdbMutex_Lock(m_node_id_mutex);
    m_reserved_nodes.clear(id);
  }

  /*
    there are nodes with correct type available but
    allocation failed for some reason
  */
  if (*nodeId)
  {
    error_string.appfmt("Id %d already allocated by another node.",
                        *nodeId);
  }
  else
  {
    const char *alias, *str;
    alias= ndb_mgm_get_node_type_alias_string(type, &str);
    error_string.appfmt("No free node id found for %s(%s).",
                        alias, str);
  }
  error_code = NDB_MGM_ALLOCID_ERROR;

 error:
  if (error_code != NDB_MGM_ALLOCID_CONFIG_MISMATCH)
  {
    // we have a temporary error which might be due to that
    // we have got the latest connect status from db-nodes.  Force update.
    updateStatus();
  }

  if (log_event || error_code == NDB_MGM_ALLOCID_CONFIG_MISMATCH)
  {
    g_eventLogger->warning("Allocate nodeid (%d) failed. Connection from ip %s."
			   " Returned error string \"%s\"",
			   *nodeId,
			   client_addr != 0
			   ? inet_ntoa(((struct sockaddr_in *)
					(client_addr))->sin_addr)
			   : "<none>",
			   error_string.c_str());

    BaseString tmp_connected, tmp_not_connected;
    for(Uint32 i = 0; i < MAX_NODES; i++)
    {
      if (connected_nodes.get(i))
      {
        if (!m_reserved_nodes.get(i))
        {
          tmp_connected.appfmt("%d ", i);
        }
      }
      else if (m_reserved_nodes.get(i))
      {
        tmp_not_connected.appfmt("%d ", i);
      }
    }

    if (tmp_connected.length() > 0)
    {
      g_eventLogger->info
        ("Mgmt server state: node id's %sconnected but not reserved",
         tmp_connected.c_str());
    }

    if (tmp_not_connected.length() > 0)
    {
      g_eventLogger->info
        ("Mgmt server state: node id's %snot connected but reserved",
         tmp_not_connected.c_str());
    }
  }
  DBUG_RETURN(false);
}

bool
MgmtSrvr::getNextNodeId(NodeId * nodeId, enum ndb_mgm_node_type type) const 
{
  NodeId tmp = * nodeId;

  tmp++;
  while(nodeTypes[tmp] != type && tmp < MAX_NODES)
    tmp++;
  
  if(tmp == MAX_NODES){
    return false;
  }

  * nodeId = tmp;
  return true;
}

#include "Services.hpp"

void
MgmtSrvr::eventReport(const Uint32 * theData, Uint32 len)
{
  const EventReport * const eventReport = (EventReport *)&theData[0];
  
  NodeId nodeId = eventReport->getNodeId();
  Ndb_logevent_type type = eventReport->getEventType();
  // Log event
  g_eventLogger->log(type, theData, len, nodeId, 
                     &m_event_listner[0].m_logLevel);
  m_event_listner.log(type, theData, len, nodeId);
}

/***************************************************************************
 * Backup
 ***************************************************************************/

int
MgmtSrvr::startBackup(Uint32& backupId, int waitCompleted, Uint32 input_backupId)
{
  SignalSender ss(theFacade);
  ss.lock(); // lock will be released on exit

  NodeId nodeId = m_master_node;
  if (okToSendTo(nodeId, false) != 0)
  {
    bool next;
    nodeId = m_master_node = 0;
    while((next = getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) == true &&
          okToSendTo(nodeId, false) != 0);
    if(!next)
      return NO_CONTACT_WITH_DB_NODES;
  }

  SimpleSignal ssig;
  BackupReq* req = CAST_PTR(BackupReq, ssig.getDataPtrSend());
  if(input_backupId > 0)
  {
    ssig.set(ss, TestOrd::TraceAPI, BACKUP, GSN_BACKUP_REQ, 
	     BackupReq::SignalLength);
    req->inputBackupId = input_backupId;
  }
  else
    ssig.set(ss, TestOrd::TraceAPI, BACKUP, GSN_BACKUP_REQ, 
	     BackupReq::SignalLength - 1);
  
  req->senderData = 19;
  req->backupDataLen = 0;
  assert(waitCompleted < 3);
  req->flags = waitCompleted & 0x3;

  BackupEvent event;
  int do_send = 1;
  while (1) {
    if (do_send)
    {
      if (ss.sendSignal(nodeId, &ssig) != SEND_OK) {
	return SEND_OR_RECEIVE_FAILED;
      }
      if (waitCompleted == 0)
	return 0;
      do_send = 0;
    }
    SimpleSignal *signal = ss.waitFor();

    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_BACKUP_CONF:{
      const BackupConf * const conf = 
	CAST_CONSTPTR(BackupConf, signal->getDataPtr());
      event.Event = BackupEvent::BackupStarted;
      event.Started.BackupId = conf->backupId;
      event.Nodes.assign(conf->nodes);
#ifdef VM_TRACE
      ndbout_c("Backup(%d) master is %d", conf->backupId,
	       refToNode(signal->header.theSendersBlockRef));
#endif
      backupId = conf->backupId;
      if (waitCompleted == 1)
	return 0;
      // wait for next signal
      break;
    }
    case GSN_BACKUP_COMPLETE_REP:{
      const BackupCompleteRep * const rep = 
	CAST_CONSTPTR(BackupCompleteRep, signal->getDataPtr());
#ifdef VM_TRACE
      ndbout_c("Backup(%d) completed", rep->backupId);
#endif
      event.Event = BackupEvent::BackupCompleted;
      event.Completed.BackupId = rep->backupId;
    
      event.Completed.NoOfBytes = rep->noOfBytesLow;
      event.Completed.NoOfLogBytes = rep->noOfLogBytes;
      event.Completed.NoOfRecords = rep->noOfRecordsLow;
      event.Completed.NoOfLogRecords = rep->noOfLogRecords;
      event.Completed.stopGCP = rep->stopGCP;
      event.Completed.startGCP = rep->startGCP;
      event.Nodes.assign(rep->nodes);

      if (signal->header.theLength >= BackupCompleteRep::SignalLength)
      {
        event.Completed.NoOfBytes += ((Uint64)rep->noOfBytesHigh) << 32;
        event.Completed.NoOfRecords += ((Uint64)rep->noOfRecordsHigh) << 32;
      }

      backupId = rep->backupId;
      return 0;
    }
    case GSN_BACKUP_REF:{
      const BackupRef * const ref = 
	CAST_CONSTPTR(BackupRef, signal->getDataPtr());
      if(ref->errorCode == BackupRef::IAmNotMaster){
	m_master_node = nodeId = refToNode(ref->masterRef);
#ifdef VM_TRACE
	ndbout_c("I'm not master resending to %d", nodeId);
#endif
	do_send = 1; // try again
	if (!theFacade->get_node_alive(nodeId))
	  m_master_node = nodeId = 0;
	continue;
      }
      event.Event = BackupEvent::BackupFailedToStart;
      event.FailedToStart.ErrorCode = ref->errorCode;
      return ref->errorCode;
    }
    case GSN_BACKUP_ABORT_REP:{
      const BackupAbortRep * const rep = 
	CAST_CONSTPTR(BackupAbortRep, signal->getDataPtr());
      event.Event = BackupEvent::BackupAborted;
      event.Aborted.Reason = rep->reason;
      event.Aborted.BackupId = rep->backupId;
      event.Aborted.ErrorCode = rep->reason;
#ifdef VM_TRACE
      ndbout_c("Backup %d aborted", rep->backupId);
#endif
      return rep->reason;
    }
    case GSN_NF_COMPLETEREP:{
      const NFCompleteRep * const rep =
	CAST_CONSTPTR(NFCompleteRep, signal->getDataPtr());
#ifdef VM_TRACE
      ndbout_c("Node %d fail completed", rep->failedNodeId);
#endif
      if (rep->failedNodeId == nodeId ||
	  waitCompleted == 1)
	return 1326;
      // wait for next signal
      // master node will report aborted backup
      break;
    }
    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
	CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      if (NdbNodeBitmask::get(rep->theNodes,nodeId) ||
	  waitCompleted == 1)
	return 1326;
      // wait for next signal
      // master node will report aborted backup
      break;
    }
    case GSN_TAKE_OVERTCCONF:
      continue;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }
}

int 
MgmtSrvr::abortBackup(Uint32 backupId)
{
  SignalSender ss(theFacade);
  ss.lock(); // lock will be released on exit

  bool next;
  NodeId nodeId = 0;
  while((next = getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) == true &&
	theFacade->get_node_alive(nodeId) == false);
  
  if(!next){
    return NO_CONTACT_WITH_DB_NODES;
  }
  
  SimpleSignal ssig;

  AbortBackupOrd* ord = CAST_PTR(AbortBackupOrd, ssig.getDataPtrSend());
  ssig.set(ss, TestOrd::TraceAPI, BACKUP, GSN_ABORT_BACKUP_ORD, 
	   AbortBackupOrd::SignalLength);
  
  ord->requestType = AbortBackupOrd::ClientAbort;
  ord->senderData = 19;
  ord->backupId = backupId;
  
  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}


MgmtSrvr::Allocated_resources::Allocated_resources(MgmtSrvr &m)
  : m_mgmsrv(m)
{
  m_reserved_nodes.clear();
  m_alloc_timeout= 0;
}

MgmtSrvr::Allocated_resources::~Allocated_resources()
{
  if (!m_reserved_nodes.isclear())
  {
    /**
     * No need to aquire mutex if we didn't have any reservation in
     * our sesssion
     */
    Guard g(m_mgmsrv.m_node_id_mutex);
    m_mgmsrv.m_reserved_nodes.bitANDC(m_reserved_nodes);

    // node has been reserved, force update signal to ndb nodes
    m_mgmsrv.updateStatus();

    char tmp_str[128];
    m_mgmsrv.m_reserved_nodes.getText(tmp_str);
    g_eventLogger->info("Mgmt server state: nodeid %d freed, m_reserved_nodes %s.",
                        get_nodeid(), tmp_str);
  }
}

void
MgmtSrvr::Allocated_resources::reserve_node(NodeId id, NDB_TICKS timeout)
{
  m_reserved_nodes.set(id);
  m_alloc_timeout= NdbTick_CurrentMillisecond() + timeout;
}

bool
MgmtSrvr::Allocated_resources::is_timed_out(NDB_TICKS tick)
{
  if (m_alloc_timeout && tick > m_alloc_timeout)
  {
    g_eventLogger->info("Mgmt server state: nodeid %d timed out.",
                        get_nodeid());
    return true;
  }
  return false;
}

NodeId
MgmtSrvr::Allocated_resources::get_nodeid() const
{
  for(Uint32 i = 0; i < MAX_NODES; i++)
  {
    if (m_reserved_nodes.get(i))
      return i;
  }
  return 0;
}

int
MgmtSrvr::setDbParameter(int node, int param, const char * value,
			 BaseString& msg){

  if(NdbMutex_Lock(m_configMutex))
    return -1;

  /**
   * Check parameter
   */
  ndb_mgm_configuration_iterator
    iter(* _config->m_configValues, CFG_SECTION_NODE);
  if(iter.first() != 0){
    msg.assign("Unable to find node section (iter.first())");
    NdbMutex_Unlock(m_configMutex);
    return -1;
  }
  
  Uint32 type = NODE_TYPE_DB + 1;
  if(node != 0){
    if(iter.find(CFG_NODE_ID, node) != 0){
      msg.assign("Unable to find node (iter.find())");
      NdbMutex_Unlock(m_configMutex);
      return -1;
    }
    if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0){
      msg.assign("Unable to get node type(iter.get(CFG_TYPE_OF_SECTION))");
      NdbMutex_Unlock(m_configMutex);
      return -1;
    }
  } else {
    do {
      if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0){
	msg.assign("Unable to get node type(iter.get(CFG_TYPE_OF_SECTION))");
	NdbMutex_Unlock(m_configMutex);
	return -1;
      }
      if(type == NODE_TYPE_DB)
	break;
    } while(iter.next() == 0);
  }
  
  if(type != NODE_TYPE_DB){
    msg.assfmt("Invalid node type or no such node (%d %d)", 
	       type, NODE_TYPE_DB);
    NdbMutex_Unlock(m_configMutex);
    return -1;
  }

  int p_type;
  unsigned val_32;
  Uint64 val_64;
  const char * val_char;
  do {
    p_type = 0;
    if(iter.get(param, &val_32) == 0){
      val_32 = atoi(value);
      break;
    }
    
    p_type++;
    if(iter.get(param, &val_64) == 0){
      val_64 = strtoll(value, 0, 10);
      break;
    }
    p_type++;
    if(iter.get(param, &val_char) == 0){
      val_char = value;
      break;
    }
    msg.assign("Could not get parameter");
    NdbMutex_Unlock(m_configMutex);
    return -1;
  } while(0);
  
  bool res = false;
  do {
    int ret = iter.get(CFG_TYPE_OF_SECTION, &type);
    assert(ret == 0);
    
    if(type != NODE_TYPE_DB)
      continue;
    
    Uint32 node;
    ret = iter.get(CFG_NODE_ID, &node);
    assert(ret == 0);
    
    ConfigValues::Iterator i2(_config->m_configValues->m_config, 
			      iter.m_config);
    switch(p_type){
    case 0:
      res = i2.set(param, val_32);
      ndbout_c("Updating node %d param: %d to %d",  node, param, val_32);
      break;
    case 1:
      res = i2.set(param, val_64);
      ndbout_c("Updating node %d param: %d to %u",  node, param, val_32);
      break;
    case 2:
      res = i2.set(param, val_char);
      ndbout_c("Updating node %d param: %d to %s",  node, param, val_char);
      break;
    default:
      require(false);
    }
    assert(res);
  } while(node == 0 && iter.next() == 0);

  msg.assign("Success");
  NdbMutex_Unlock(m_configMutex);
  return 0;
}
int
MgmtSrvr::setConnectionDbParameter(int node1, 
				   int node2,
				   int param,
				   int value,
				   BaseString& msg){
  Uint32 current_value,new_value;

  DBUG_ENTER("MgmtSrvr::setConnectionDbParameter");

  if(NdbMutex_Lock(m_configMutex))
  {
    DBUG_RETURN(-1);
  }

  ndb_mgm_configuration_iterator 
    iter(* _config->m_configValues, CFG_SECTION_CONNECTION);

  if(iter.first() != 0){
    msg.assign("Unable to find connection section (iter.first())");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-1);
  }

  for(;iter.valid();iter.next()) {
    Uint32 n1,n2;
    iter.get(CFG_CONNECTION_NODE_1, &n1);
    iter.get(CFG_CONNECTION_NODE_2, &n2);
    if((n1 == (unsigned)node1 && n2 == (unsigned)node2)
       || (n1 == (unsigned)node2 && n2 == (unsigned)node1))
      break;
  }
  if(!iter.valid()) {
    msg.assign("Unable to find connection between nodes");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-2);
  }
  
  if(iter.get(param, &current_value) != 0) {
    msg.assign("Unable to get current value of parameter");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-3);
  }

  ConfigValues::Iterator i2(_config->m_configValues->m_config, 
			    iter.m_config);

  if(i2.set(param, (unsigned)value) == false) {
    msg.assign("Unable to set new value of parameter");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-4);
  }
  
  if(iter.get(param, &new_value) != 0) {
    msg.assign("Unable to get parameter after setting it.");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-5);
  }

  msg.assfmt("%u -> %u",current_value,new_value);
  NdbMutex_Unlock(m_configMutex);
  DBUG_RETURN(1);
}


int
MgmtSrvr::getConnectionDbParameter(int node1, 
				   int node2,
				   int param,
				   int *value,
				   BaseString& msg){
  DBUG_ENTER("MgmtSrvr::getConnectionDbParameter");

  if(NdbMutex_Lock(m_configMutex))
  {
    DBUG_RETURN(-1);
  }

  ndb_mgm_configuration_iterator
    iter(* _config->m_configValues, CFG_SECTION_CONNECTION);

  if(iter.first() != 0){
    msg.assign("Unable to find connection section (iter.first())");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-1);
  }

  for(;iter.valid();iter.next()) {
    Uint32 n1=0,n2=0;
    iter.get(CFG_CONNECTION_NODE_1, &n1);
    iter.get(CFG_CONNECTION_NODE_2, &n2);
    if((n1 == (unsigned)node1 && n2 == (unsigned)node2)
       || (n1 == (unsigned)node2 && n2 == (unsigned)node1))
      break;
  }
  if(!iter.valid()) {
    msg.assign("Unable to find connection between nodes");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-1);
  }
  
  if(iter.get(param, (Uint32*)value) != 0) {
    msg.assign("Unable to get current value of parameter");
    NdbMutex_Unlock(m_configMutex);
    DBUG_RETURN(-1);
  }

  msg.assfmt("%d",*value);
  NdbMutex_Unlock(m_configMutex);
  DBUG_RETURN(1);
}


bool
MgmtSrvr::transporter_connect(NDB_SOCKET_TYPE sockfd,
                                   BaseString& errormsg)
{
  DBUG_ENTER("MgmtSrvr::transporter_connect");
  TransporterRegistry* tr= theFacade->get_registry();
  if (!tr->connect_server(sockfd, errormsg))
    DBUG_RETURN(false);

  /*
    Force an update_connections() so that the
    ClusterMgr and TransporterFacade is up to date
    with the new connection.
    Important for correct node id reservation handling
  */
  theFacade->lock_mutex();
  tr->update_connections();
  theFacade->unlock_mutex();

  DBUG_RETURN(true);
}

int MgmtSrvr::connect_to_self(const char * bindaddress)
{
  int r= 0;
  m_local_mgm_handle= ndb_mgm_create_handle();
  snprintf(m_local_mgm_connect_string,sizeof(m_local_mgm_connect_string),
           "%s:%u", bindaddress ? bindaddress : "localhost", getPort());
  ndb_mgm_set_connectstring(m_local_mgm_handle, m_local_mgm_connect_string);

  if((r= ndb_mgm_connect(m_local_mgm_handle, 0, 0, 0)) < 0)
  {
    ndb_mgm_destroy_handle(&m_local_mgm_handle);
    return r;
  }
  // TransporterRegistry now owns this NdbMgmHandle and will destroy it.
  theFacade->get_registry()->set_mgm_handle(m_local_mgm_handle);

  return 0;
}


bool
MgmtSrvr::request_events(NdbNodeBitmask nodes, Uint32 reports_per_node,
                         Uint32 dump_type,
                         Vector<SimpleSignal>& events)
{
  Uint32 nodes_counter[MAX_NDB_NODES];
  SignalSender ss(theFacade);
  ss.lock();

  // Send the dump command to all requested NDB nodes
  const bool all = nodes.isclear();
  for (int i = 1; i < MAX_NDB_NODES; i++)
  {
    // Check if node should be involved
    if (!all && !nodes.get(i))
      continue;

    // Only request from confirmed DB nodes
    const ClusterMgr::Node node = ss.getNodeInfo(i);
    if (node.m_info.getType() != NodeInfo::DB ||
        node.m_api_reg_conf == false)
    {
      nodes.clear(i);
      continue;
    }

    SimpleSignal ssig;
    DumpStateOrd * const dumpOrd = (DumpStateOrd*)ssig.getDataPtrSend();

    dumpOrd->args[0] = dump_type;
    dumpOrd->args[1] = ss.getOwnRef(); // Return to sender

    if (ss.sendSignal(i, ssig, CMVMI, GSN_DUMP_STATE_ORD, 2) == SEND_OK)
    {
      nodes.set(i);
      nodes_counter[i] = 0;
    }
  }


  while (true)
  {
    // Check if all nodes are done
    if (nodes.isclear())
      break;

    SimpleSignal *signal = ss.waitFor();
    switch (signal->readSignalNumber()) {
    case GSN_EVENT_REP:{
      const NodeId nodeid = refToNode(signal->header.theSendersBlockRef);
      const EventReport * const event =
        (const EventReport*)signal->getDataPtr();
      (void)event; // kill warning

      if (!nodes.get(nodeid))
      {
        // The reporting node was not expected
        assert(false);
        return false;
      }

      // Save signal
      events.push_back(SimpleSignal(*signal));

      // Check if node is done
      nodes_counter[nodeid]++;
      if (nodes_counter[nodeid] == reports_per_node)
        nodes.clear(nodeid);

      break;
    }

    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
        (const NodeFailRep*)signal->getDataPtr();
      for (NodeId i = 1; i < MAX_NDB_NODES; i++)
      {
        if (NdbNodeBitmask::get(rep->theNodes, i))
        {
          nodes.clear(i);

          // Remove any previous reports from this node
          // it should not be reported
          for (unsigned j = 0; j < events.size(); j++)
          {
            const SimpleSignal& ssig = events[j];
            const NodeId nodeid = refToNode(ssig.header.theSendersBlockRef);
            if (nodeid == i)
            {
              events.erase(j);
              j--;
            }
          }
        }
      }
      break;
    }

    default:
      // Ignore all other signals
      break;
    }
  }
  ss.unlock();

  return true;
}

template class MutexVector<NodeId>;
template class MutexVector<Ndb_mgmd_event_service::Event_listener>;
template class Vector<EventSubscribeReq>;
template class MutexVector<EventSubscribeReq>;
template class Vector<MgmtSrvr::nodeid_and_host>;
