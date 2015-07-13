/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>

#include "MgmtSrvr.hpp"
#include "ndb_mgmd_error.h"
#include "Services.hpp"
#include "ConfigManager.hpp"
#include "Defragger.hpp"

#include <NdbOut.hpp>
#include <NdbApiSignal.hpp>
#include <kernel_types.h>
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
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/AllocNodeId.hpp>
#include <signaldata/SchemaTrans.hpp>
#include <signaldata/CreateNodegroup.hpp>
#include <signaldata/DropNodegroup.hpp>
#include <signaldata/Sync.hpp>
#include <signaldata/GetConfig.hpp>
#include <NdbSleep.h>
#include <portlib/NdbDir.hpp>
#include <EventLogger.hpp>
#include <logger/FileLogHandler.hpp>
#include <logger/ConsoleLogHandler.hpp>
#include <logger/SysLogHandler.hpp>
#include <DebuggerNames.hpp>
#include <ndb_version.h>

#include <SocketServer.hpp>
#include <NdbConfig.h>

#include <NdbAutoPtr.hpp>
#include <NdbDir.hpp>
#include <ndberror.h>

#include <mgmapi.h>
#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters.h>

#include <SignalSender.hpp>

int g_errorInsert = 0;
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

extern "C" my_bool opt_core;

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


static int
translateStopRef(Uint32 errCode)
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


MgmtSrvr::MgmtSrvr(const MgmtOpts& opts) :
  m_opts(opts),
  _blockNumber(-1),
  _ownNodeId(0),
  m_port(0),
  m_local_config(NULL),
  _ownReference(0),
  m_config_manager(NULL),
  m_need_restart(false),
  theFacade(NULL),
  _isStopThread(false),
  _logLevelThreadSleep(500),
  m_event_listner(this),
  m_master_node(0),
  _logLevelThread(NULL),
  m_version_string(ndbGetOwnVersionString())
{
  DBUG_ENTER("MgmtSrvr::MgmtSrvr");

  m_local_config_mutex= NdbMutex_Create();
  m_reserved_nodes_mutex= NdbMutex_Create();
  if (!m_local_config_mutex || !m_reserved_nodes_mutex)
  {
    g_eventLogger->error("Failed to create MgmtSrvr mutexes");
    require(false);
  }

  /* Init node arrays */
  for(Uint32 i = 0; i<MAX_NODES; i++) {
    nodeTypes[i] = (enum ndb_mgm_node_type)-1;
    clear_connect_address_cache(i);
  }

  /* Setup clusterlog as client[0] in m_event_listner */
  {
    Ndb_mgmd_event_service::Event_listener se;
    my_socket_invalidate(&(se.m_socket));
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


/*
  check_configdir

  Make sure configdir exist and try to create it if not

*/

const char*
MgmtSrvr::check_configdir() const
{
  if (m_opts.configdir &&
      strcmp(m_opts.configdir, MYSQLCLUSTERDIR) != 0)
  {
    // Specified on commmand line
    if (access(m_opts.configdir, F_OK))
    {
      g_eventLogger->error("Directory '%s' specified with --configdir " \
                           "does not exist. Either create it or pass " \
                           "the path to an already existing directory.",
                           m_opts.configdir);
      return NULL;
    }
    return m_opts.configdir;
  }
  else
  {
    // Compiled in path MYSQLCLUSTERDIR
    if (access(MYSQLCLUSTERDIR, F_OK))
    {
      g_eventLogger->info("The default config directory '%s' "            \
                          "does not exist. Trying to create it...",
                          MYSQLCLUSTERDIR);

      if (!NdbDir::create(MYSQLCLUSTERDIR) ||
          access(MYSQLCLUSTERDIR, F_OK))
      {
        g_eventLogger->error("Could not create directory '%s'. "        \
                             "Either create it manually or "            \
                             "specify a different directory with "      \
                             "--configdir=<path>",
                             MYSQLCLUSTERDIR);
        return NULL;
      }

      g_eventLogger->info("Sucessfully created config directory");
    }
    return MYSQLCLUSTERDIR;
  }
}


bool
MgmtSrvr::init()
{
  DBUG_ENTER("MgmtSrvr::init");

  const char* configdir;

  if (!m_opts.config_cache)
  {
    g_eventLogger->info("Skipping check of config directory since "
                        "config cache is disabled.");
    configdir = NULL;
  }
  else
  {
    if (!(configdir= check_configdir()))
      DBUG_RETURN(false);
  }

  if (!(m_config_manager= new ConfigManager(m_opts, configdir)))
  {
    g_eventLogger->error("Failed to create ConfigManager");
    DBUG_RETURN(false);
  }

  if (m_config_manager->add_config_change_subscriber(this) < 0)
  {
    g_eventLogger->error("Failed to add MgmtSrvr as config change subscriber");
    DBUG_RETURN(false);
  }

  if (!m_config_manager->init())
  {
    DBUG_RETURN(false);
  }

  /* 'config_changed' should have been called from 'init' */
  require(m_local_config != 0);

  if (m_opts.print_full_config)
  {
    print_config();
    DBUG_RETURN(false);
  }

  assert(_ownNodeId);

  DBUG_RETURN(true);
}


bool
MgmtSrvr::start_transporter(const Config* config)
{
  DBUG_ENTER("MgmtSrvr::start_transporter");

  theFacade= new TransporterFacade(0);
  if (theFacade == 0)
  {
    g_eventLogger->error("Could not create TransporterFacade.");
    DBUG_RETURN(false);
  }

  assert(_blockNumber == -1); // Blocknumber shouldn't been allocated yet

  /*
    Register ourself at TransporterFacade to be able to receive signals
    and to be notified when a database process has died.
  */
  Uint32 res;
  if ((res = open(theFacade)) == 0)
  {
    g_eventLogger->error("Failed to open block in TransporterFacade");
    theFacade->stop_instance();
    delete theFacade;
    theFacade = 0;
    DBUG_RETURN(false);
  }
  _blockNumber = refToBlock(res);

  /**
   * Need to call ->open() prior to actually starting TF
   */
  m_config_manager->set_facade(theFacade);

  if (theFacade->start_instance(_ownNodeId,
                                config->m_configValues) < 0)
  {
    g_eventLogger->error("Failed to start transporter");
    delete theFacade;
    theFacade = 0;
    DBUG_RETURN(false);
  }

  _ownReference = numberToRef(_blockNumber, _ownNodeId);

  /*
    set api reg req frequency quite high:

    100 ms interval to make sure we have fairly up-to-date
    info from the nodes.  This to make sure that this info
    is not dependent on heartbeat settings in the
    configuration
  */
  theFacade->ext_set_max_api_reg_req_interval(100);

  DBUG_RETURN(true);
}


bool
MgmtSrvr::start_mgm_service(const Config* config)
{
  DBUG_ENTER("MgmtSrvr::start_mgm_service");

  assert(m_port == 0);
  {
    // Find the portnumber to use for mgm service
    ConfigIter iter(config, CFG_SECTION_NODE);

    if(iter.find(CFG_NODE_ID, _ownNodeId) != 0){
      g_eventLogger->error("Could not find node %d in config", _ownNodeId);
      DBUG_RETURN(false);
    }

    unsigned type;
    if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0 ||
       type != NODE_TYPE_MGM){
      g_eventLogger->error("Node %d is not defined as management server",
                           _ownNodeId);
      DBUG_RETURN(false);
    }

    if(iter.get(CFG_MGM_PORT, &m_port) != 0){
      g_eventLogger->error("PortNumber not defined for node %d", _ownNodeId);
      DBUG_RETURN(false);
    }
  }

  unsigned short port= m_port;
  DBUG_PRINT("info", ("Using port %d", port));
  if (port == 0)
  {
    g_eventLogger->error("Could not find out which port to use"\
                        " for management service");
    DBUG_RETURN(false);
  }

  {
    int count= 5; // no of retries for tryBind
    while(!m_socket_server.tryBind(port, m_opts.bind_address))
    {
      if (--count > 0)
      {
	NdbSleep_SecSleep(1);
	continue;
      }
      g_eventLogger->error("Unable to bind management service port: %s:%d!\n"
                           "Please check if the port is already used,\n"
                           "(perhaps a ndb_mgmd is already running),\n"
                           "and if you are executing on the correct computer",
                           (m_opts.bind_address ? m_opts.bind_address : "*"),
                           port);
      DBUG_RETURN(false);
    }
  }

  {
    MgmApiService * mapi = new MgmApiService(*this);
    if (mapi == NULL)
    {
      g_eventLogger->error("Could not allocate MgmApiService");
      DBUG_RETURN(false);
    }

    if(!m_socket_server.setup(mapi, &port, m_opts.bind_address))
    {
      delete mapi; // Will be deleted by SocketServer in all other cases
      g_eventLogger->error("Unable to setup management service port: %s:%d!\n"
                           "Please check if the port is already used,\n"
                           "(perhaps a ndb_mgmd is already running),\n"
                           "and if you are executing on the correct computer",
                           (m_opts.bind_address ? m_opts.bind_address : "*"),
                           port);
      DBUG_RETURN(false);
    }

    if (port != m_port)
    {
      g_eventLogger->error("Couldn't start management service on the "\
                           "requested port: %d. Got port: %d instead",
                          m_port, port);
      DBUG_RETURN(false);
    }
  }

  m_socket_server.startServer();

  g_eventLogger->info("Id: %d, Command port: %s:%d",
                      _ownNodeId,
                      m_opts.bind_address ? m_opts.bind_address : "*",
                      port);
  DBUG_RETURN(true);
}


bool
MgmtSrvr::start()
{
  DBUG_ENTER("MgmtSrvr::start");

  /* Start transporter */
  if(!start_transporter(m_local_config))
  {
    g_eventLogger->error("Failed to start transporter!");
    DBUG_RETURN(false);
  }

  /* Start mgm service */
  if (!start_mgm_service(m_local_config))
  {
    g_eventLogger->error("Failed to start mangement service!");
    DBUG_RETURN(false);
  }

  /* Use local MGM port for TransporterRegistry */
  if(!connect_to_self())
  {
    g_eventLogger->error("Failed to connect to ourself!");
    DBUG_RETURN(false);
  }

  /* Start config manager */
  if (!m_config_manager->start())
  {
    g_eventLogger->error("Failed to start ConfigManager");
    DBUG_RETURN(false);
  }

  /* Loglevel thread */
  assert(_isStopThread == false);
  _logLevelThread = NdbThread_Create(logLevelThread_C,
				     (void**)this,
                                     0, // default stack size
				     "MgmtSrvr_Loglevel",
				     NDB_THREAD_PRIO_LOW);

  DBUG_RETURN(true);
}


void
MgmtSrvr::configure_eventlogger(const BaseString& logdestination) const
{
  // Close old log handlers before creating the new
  g_eventLogger->close();

  Vector<BaseString> logdestinations;
  logdestination.split(logdestinations, ";");

  for(unsigned i = 0; i < logdestinations.size(); i++)
  {
    // Extract type(everything left of colon)
    Vector<BaseString> v_type_params;
    logdestinations[i].split(v_type_params, ":", 2);
    BaseString type(v_type_params[0]);
    
    // Extract params(everything right of colon)
    BaseString params;
    if(v_type_params.size() >= 2)
      params = v_type_params[1];

    LogHandler *handler = NULL;
    if(type == "FILE")
    {
      char *default_file_name= NdbConfig_ClusterLogFileName(_ownNodeId);
      handler = new FileLogHandler(default_file_name);
      free(default_file_name);
    }
    else if(type == "CONSOLE")
    {
      handler = new ConsoleLogHandler();
    }
#ifndef _WIN32
    else if(type == "SYSLOG")
    {
      handler = new SysLogHandler();
    }
#endif  
    if(handler == NULL)
    {
      ndbout_c("INTERNAL ERROR: Could not create log handler for: '%s'",
               logdestinations[i].c_str());
      continue;
    }

    if(!handler->parseParams(params))
    {
      ndbout_c("Failed to parse parameters for log handler: '%s', error: %d '%s'",
               logdestinations[i].c_str(), handler->getErrorCode(), handler->getErrorStr());
      delete handler;
      continue;
    }

    if (!g_eventLogger->addHandler(handler))
    {
      ndbout_c("INTERNAL ERROR: Could not add %s log handler", handler->handler_type());
      g_eventLogger->error("INTERNAL ERROR: Could not add %s log handler",
                           handler->handler_type());
      delete handler;
      continue;
    }
  }
}


void
MgmtSrvr::setClusterLog(const Config* config)
{
  DBUG_ASSERT(_ownNodeId);

  ConfigIter iter(config, CFG_SECTION_NODE);
  require(iter.find(CFG_NODE_ID, _ownNodeId) == 0);

  // Update DataDir from config
  const char *datadir;
  require(iter.get(CFG_NODE_DATADIR, &datadir) == 0);
  NdbConfig_SetPath(datadir);

  if (NdbDir::chdir(NdbConfig_get_path(NULL)) != 0)
  {
    g_eventLogger->warning("Cannot change directory to '%s', error: %d",
                           NdbConfig_get_path(NULL), errno);
    // Ignore error
  }

  // Get log destination from config
  BaseString logdest;
  const char *value;
  if(iter.get(CFG_LOG_DESTINATION, &value) == 0){
    logdest.assign(value);
  }

  bool logdest_configured = true;
  if(logdest.length() == 0 || logdest == "") {
    // No LogDestination set, use default settings
    char *clusterLog= NdbConfig_ClusterLogFileName(_ownNodeId);
    logdest.assfmt("FILE:filename=%s,maxsize=1000000,maxfiles=6",
		   clusterLog);
    free(clusterLog);
    logdest_configured = false;
  }

  configure_eventlogger(logdest);

  if (logdest_configured == false &&
      m_opts.non_interactive)
  {
    g_eventLogger->createConsoleHandler();
  }

#ifdef _WIN32
  /* Output to Windows event log */
  g_eventLogger->createEventLogHandler("MySQL Cluster Management Server");
#endif

  if (m_opts.verbose)
    g_eventLogger->enable(Logger::LL_DEBUG);
}


void
MgmtSrvr::config_changed(NodeId node_id, const Config* new_config)
{
  DBUG_ENTER("MgmtSrvr::config_changed");

  Guard g(m_local_config_mutex);

  // Don't allow nodeid to change, once it's been set
  require(_ownNodeId == 0 || _ownNodeId == node_id);

  _ownNodeId= node_id;

  if (m_local_config)
    delete m_local_config;

  m_local_config= new Config(new_config); // Copy
  require(m_local_config != 0);

  /* Rebuild node arrays */
  ConfigIter iter(m_local_config, CFG_SECTION_NODE);
  for(Uint32 i = 0; i<MAX_NODES; i++) {

    clear_connect_address_cache(i);

    if (iter.first())
      continue;

    if (iter.find(CFG_NODE_ID, i) == 0){
      unsigned type;
      require(iter.get(CFG_TYPE_OF_SECTION, &type) == 0);

      switch(type){
      case NODE_TYPE_DB:
        nodeTypes[i] = NDB_MGM_NODE_TYPE_NDB;
        break;
      case NODE_TYPE_API:
        nodeTypes[i] = NDB_MGM_NODE_TYPE_API;
        break;
      case NODE_TYPE_MGM:
        nodeTypes[i] = NDB_MGM_NODE_TYPE_MGM;
        break;
      default:
        break;
      }
    }
    else
    {
      nodeTypes[i] = (enum ndb_mgm_node_type)-1;
    }

  }

  // Setup cluster log
  setClusterLog(m_local_config);

  if (theFacade)
  {
    if (!theFacade->configure(_ownNodeId,
                              m_local_config->m_configValues))
    {
      g_eventLogger->warning("Could not reconfigure everything online, "
                             "this node need a restart");
      m_need_restart= true;
    }
  }

  DBUG_VOID_RETURN;
}


bool
MgmtSrvr::get_packed_config(ndb_mgm_node_type node_type,
                            BaseString& buf64, BaseString& error)
{
  return m_config_manager->get_packed_config(node_type, &buf64, error);
}

bool
MgmtSrvr::get_packed_config_from_node(NodeId nodeId,
                            BaseString& buf64, BaseString& error)
{
  DBUG_ENTER("get_packed_config_from_node");

  if (nodeId >= MAX_NODES_ID)
  {
    error.assfmt("Nodeid %d is greater than max nodeid %d. ",
                 nodeId, MAX_NODES_ID);
    DBUG_RETURN(false);
  }

  if (getNodeType(nodeId) == NDB_MGM_NODE_TYPE_UNKNOWN)
  {
    error.assfmt("Nodeid %d does not exist. ", nodeId);
    DBUG_RETURN(false);
  }

  if (getNodeType(nodeId) != NDB_MGM_NODE_TYPE_NDB)
  {
    error.assfmt("Node %d is not a data node. ", nodeId);
    DBUG_RETURN(false);
  }

  trp_node node = getNodeInfo(nodeId);

  if (!node.m_alive)
  {
    error.assfmt("Data node %d is not alive. ", nodeId);
    DBUG_RETURN(false);
  }

  const Uint32 version = node.m_info.m_version;

  if (!ndbd_get_config_supported(version))
  {
    error.assfmt("Data node %d (version %d.%d.%d) does not support getting config. ",
                 nodeId, ndbGetMajor(version),
                 ndbGetMinor(version), ndbGetBuild(version));
    DBUG_RETURN(false);
  }

  INIT_SIGNAL_SENDER(ss,nodeId);

  SimpleSignal ssig;
  GetConfigReq* req = CAST_PTR(GetConfigReq, ssig.getDataPtrSend());
  req->senderRef = ss.getOwnRef();
  req->nodeId = nodeId;

  g_eventLogger->debug("Sending GET_CONFIG_REQ to %d", nodeId);

  ssig.set(ss, TestOrd::TraceAPI, CMVMI, GSN_GET_CONFIG_REQ,
           GetConfigReq::SignalLength);
  if ((ss.sendSignal(nodeId, &ssig)) != SEND_OK)
  {
    DBUG_RETURN(false);
  }

  Defragger defragger;
  while (true)
  {
    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();

    switch (gsn)
    {
    case GSN_GET_CONFIG_CONF:
    {
      if (refToNode(signal->header.theSendersBlockRef) != nodeId)
      {
        error.assfmt("Internal Error: Reply from wrong node %d, expected from %d. ",
                     refToNode(signal->header.theSendersBlockRef),
                     nodeId);
        DBUG_RETURN(false);
      }

      const GetConfigConf * const conf =
	CAST_CONSTPTR(GetConfigConf, signal->getDataPtr());

      if (signal->header.m_noOfSections != 1)
      {
        error.assfmt("Internal Error: Wrong number of sections %d received, expected %d. ",
                     signal->header.m_noOfSections, 1);
        DBUG_RETURN(false);
      }

      if (defragger.defragment(signal))
      {
        ConfigValuesFactory cf;
        require(cf.unpack(signal->ptr[0].p, conf->configLength));

        Config received_config(cf.getConfigValues());
        if (!received_config.pack64(buf64))
        {
          error.assign("Failed to pack64");
          DBUG_RETURN(false);
        }
        DBUG_RETURN(true);
      }
      // wait until all fragments are received
      continue;
    }

    case GSN_GET_CONFIG_REF:
    {
      if (refToNode(ssig.header.theSendersBlockRef) != nodeId)
      {
        error.assfmt("Internal Error: Reply from wrong node %d, expected from %d. ",
                     refToNode(signal->header.theSendersBlockRef),
                     nodeId);
        DBUG_RETURN(false);
      }
      const GetConfigRef * const ref =
	CAST_CONSTPTR(GetConfigRef, signal->getDataPtr());
      error.assfmt("Error in retrieving config from node %d: Internal error: %d",
                   nodeId, ref->error);

      DBUG_RETURN(false);
    }

    case GSN_NF_COMPLETEREP:
    {
      const NFCompleteRep * rep = CAST_CONSTPTR(NFCompleteRep,
                                                signal->getDataPtr());
      if (rep->failedNodeId == nodeId)
      {
        error.assfmt("Node %d is not available", nodeId);
        DBUG_RETURN(false);
      }
      continue;
    }

    case GSN_NODE_FAILREP:
    {
      // Wait until GSN_NODE_COMPLETEREP is received.
      continue;
    }

    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      // Ignore
      continue;

    default:
      report_unknown_signal(signal);
      DBUG_RETURN(false);
    }
  }
  // Should never come here
  require(false);
  DBUG_RETURN(false);
}

MgmtSrvr::~MgmtSrvr()
{
  /* Stop log level thread */
  void* res = 0;
  _isStopThread = true;

  if (_logLevelThread != NULL) {
    NdbThread_WaitFor(_logLevelThread, &res);
    NdbThread_Destroy(&_logLevelThread);
  }

  /* Stop mgm service, don't allow new connections */
  m_socket_server.stopServer();

  /* Stop all active session */
  if (!m_socket_server.stopSessions(true,
                                    2 * MgmApiSession::SOCKET_TIMEOUT))
  {
    g_eventLogger->error("Failed to wait for all sessions to stop, "
                         "continuing with shutdown anyway.");
  }

  /* Stop config manager */
  if (m_config_manager != 0)
  {
    m_config_manager->stop();
    delete m_config_manager;
    m_config_manager= 0;
  }

  this->close(); // close trp_client before stopping TransporterFacade

  // Stop transporter
  if(theFacade != 0){
    theFacade->stop_instance();
    delete theFacade;
    theFacade = 0;
  }

  delete m_local_config;

  NdbMutex_Destroy(m_local_config_mutex);
  NdbMutex_Destroy(m_reserved_nodes_mutex);
}


//****************************************************************************
//****************************************************************************

int MgmtSrvr::okToSendTo(NodeId nodeId, bool unCond) 
{
  if(nodeId == 0 || getNodeType(nodeId) != NDB_MGM_NODE_TYPE_NDB)
    return WRONG_PROCESS_TYPE;
  // Check if we have contact with it
  if(unCond){
    if (getNodeInfo(nodeId).is_confirmed())
      return 0;
  }
  else if (getNodeInfo(nodeId).m_alive == true)
    return 0;
  return NO_CONTACT_WITH_PROCESS;
}

void
MgmtSrvr::report_unknown_signal(SimpleSignal *signal)
{
  signal->print();
  g_eventLogger->error("Unknown signal received. SignalNumber: "
                       "%i from (%d, 0x%x)",
                       signal->readSignalNumber(),
                       refToNode(signal->header.theSendersBlockRef),
                       refToBlock(signal->header.theSendersBlockRef));
  assert(false);
}

/*****************************************************************************
 * Starting and stopping database nodes
 ****************************************************************************/

int 
MgmtSrvr::sendSTART_ORD(int nodeId)
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

void
MgmtSrvr::status_api(int nodeId,
                     ndb_mgm_node_status& node_status,
                     Uint32& version, Uint32& mysql_version,
                     const char **address,
                     char *addr_buf,
                     size_t addr_buf_size)
{
  assert(getNodeType(nodeId) == NDB_MGM_NODE_TYPE_API);
  assert(version == 0 && mysql_version == 0);

  if (sendVersionReq(nodeId,
                     version,
                     mysql_version,
                     address,
                     addr_buf,
                     addr_buf_size) != 0)
  {
    // Couldn't get version from any NDB node.
    assert(version == 0);
    node_status = NDB_MGM_NODE_STATUS_UNKNOWN;
    return;
  }

  if (version)
  {
    assert(mysql_version);
    node_status = NDB_MGM_NODE_STATUS_CONNECTED;
  }
  else
  {
    assert(mysql_version == 0);
    node_status = NDB_MGM_NODE_STATUS_NO_CONTACT;
  }
  return;
}


int
MgmtSrvr::sendVersionReq(int v_nodeId,
			 Uint32 &version,
			 Uint32& mysql_version,
			 const char **address,
                         char *addr_buf,
                         size_t addr_buf_size)
{
  SignalSender ss(theFacade);
  ss.lock();

  SimpleSignal ssig;
  ApiVersionReq* req = CAST_PTR(ApiVersionReq, ssig.getDataPtrSend());
  req->senderRef = ss.getOwnRef();
  req->nodeId = v_nodeId;
  ssig.set(ss, TestOrd::TraceAPI, QMGR,
           GSN_API_VERSION_REQ, ApiVersionReq::SignalLength);

  NodeId nodeId = 0;
  bool do_send = true;
  while(true)
  {
    if (do_send)
    {
      nodeId = ss.get_an_alive_node();
      if (nodeId == 0)
      {
        return NO_CONTACT_WITH_DB_NODES;
      }

      if (ss.sendSignal(nodeId, &ssig) != SEND_OK)
      {
        return SEND_OR_RECEIVE_FAILED;
      }

      do_send = false;
    }

    SimpleSignal *signal = ss.waitFor();

    switch (signal->readSignalNumber()) {
    case GSN_API_VERSION_CONF: {
      const ApiVersionConf * const conf =
	CAST_CONSTPTR(ApiVersionConf, signal->getDataPtr());

      assert((int) conf->nodeId == v_nodeId);

      version = conf->version;
      mysql_version = conf->mysql_version;
      if (version < NDBD_SPLIT_VERSION)
	mysql_version = 0;
      struct in_addr in;
      in.s_addr= conf->m_inet_addr;
      *address= Ndb_inet_ntop(AF_INET,
                              static_cast<void*>(&in),
                              addr_buf,
                              (socklen_t)addr_buf_size);

      return 0;
    }

    case GSN_NF_COMPLETEREP:{
      const NFCompleteRep * const rep =
	CAST_CONSTPTR(NFCompleteRep, signal->getDataPtr());
      if (rep->failedNodeId == nodeId)
	do_send = true; // retry with other node
      continue;
    }

    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
	CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      if (BitmaskImpl::safe_get(len, rep->theAllNodes, nodeId))
      {
	do_send = true; // retry with other node
      }
      continue;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      // Ignore
      continue;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }

  // Should never come here
  require(false);
  return -1;
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
    Guard g(m_local_config_mutex);
    {
      ConfigIter iter(m_local_config, CFG_SECTION_NODE);

      if(iter.first())                       return SEND_OR_RECEIVE_FAILED;
      if(iter.find(CFG_NODE_ID, nodeId))     return SEND_OR_RECEIVE_FAILED;
      if(iter.get(CFG_NODE_HOST, &hostname)) return SEND_OR_RECEIVE_FAILED;
    }
    {
      ConfigIter iter(m_local_config, CFG_SECTION_NODE);

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

/**
 * send STOP_REQ to all DB-nodes
 *   and wait for them to stop or refuse
 *
 */
int
MgmtSrvr::sendall_STOP_REQ(NodeBitmask &stoppedNodes,
                           bool abort,
                           bool stop,
                           bool restart,
                           bool nostart,
                           bool initialStart)
{
  int error = 0;
  DBUG_ENTER("MgmtSrvr::sendall_STOP_REQ");
  DBUG_PRINT("enter", ("abort: %d  stop: %d  restart: %d  "
                       "nostart: %d  initialStart: %d",
                       abort, stop, restart, nostart, initialStart));

  stoppedNodes.clear();

  SignalSender ss(theFacade);
  ss.lock(); // lock will be released on exit

  SimpleSignal ssig;
  StopReq* const stopReq = CAST_PTR(StopReq, ssig.getDataPtrSend());
  ssig.set(ss, TestOrd::TraceAPI, NDBCNTR, GSN_STOP_REQ, StopReq::SignalLength);

  stopReq->requestInfo = 0;
  stopReq->apiTimeout = 5000;
  stopReq->transactionTimeout = 1000;
  stopReq->readOperationTimeout = 1000;
  stopReq->operationTimeout = 1000;
  stopReq->senderData = 12;
  stopReq->senderRef = ss.getOwnRef();
  stopReq->singleuser = 0;
  StopReq::setSystemStop(stopReq->requestInfo, stop);
  StopReq::setPerformRestart(stopReq->requestInfo, restart);
  StopReq::setStopAbort(stopReq->requestInfo, abort);
  StopReq::setNoStart(stopReq->requestInfo, nostart);
  StopReq::setInitialStart(stopReq->requestInfo, initialStart);

  // send the signals
  int failed = 0;
  NodeBitmask nodes;
  {
    NodeId nodeId = 0;
    while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB))
    {
      if (okToSendTo(nodeId, true) == 0)
      {
	SendStatus result = ss.sendSignal(nodeId, &ssig);
	if (result == SEND_OK)
	  nodes.set(nodeId);
        else
          failed++;
      }
    }
  }

  if (nodes.isclear() && failed > 0)
  {
    DBUG_RETURN(SEND_OR_RECEIVE_FAILED);
  }

  // now wait for the replies
  while (!nodes.isclear())
  {
    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_STOP_REF:
    {
      const StopRef * const ref = CAST_CONSTPTR(StopRef, signal->getDataPtr());
      const NodeId nodeId = refToNode(signal->header.theSendersBlockRef);
#ifdef VM_TRACE
      ndbout_c("Node %d refused stop", nodeId);
#endif
      assert(nodes.get(nodeId));
      nodes.clear(nodeId);
      error = translateStopRef(ref->errorCode);
      break;
    }
    case GSN_STOP_CONF:
    {
      const NodeId nodeId = refToNode(signal->header.theSendersBlockRef);
      assert(nodes.get(nodeId));
      nodes.clear(nodeId);
      break;
    }
    case GSN_NF_COMPLETEREP:
    {
      const NFCompleteRep * rep = CAST_CONSTPTR(NFCompleteRep,
                                                signal->getDataPtr());
      if (rep->failedNodeId <= nodes.max_size())
        nodes.clear(rep->failedNodeId); // clear the failed node

      if (rep->failedNodeId <= stoppedNodes.max_size())
        stoppedNodes.set(rep->failedNodeId);
      break;
    }
    case GSN_NODE_FAILREP:
    {
      const NodeFailRep * rep = CAST_CONSTPTR(NodeFailRep,
                                              signal->getDataPtr());
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      NodeBitmask mask;
      mask.assign(len, rep->theAllNodes);
      nodes.bitANDC(mask);
      stoppedNodes.bitOR(mask);
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      continue;
    default:
      report_unknown_signal(signal);
      DBUG_RETURN(SEND_OR_RECEIVE_FAILED);
    }
  }

  DBUG_RETURN(error);
}

int
MgmtSrvr::guess_master_node(SignalSender& ss)
{
  /**
   * First check if m_master_node is started
   */
  NodeId guess = m_master_node;
  if (guess != 0)
  {
    trp_node node = ss.getNodeInfo(guess);
    if (node.m_state.startLevel == NodeState::SL_STARTED)
      return guess;
  }

  /**
   * Check for any started node
   */
  guess = 0;
  while(getNextNodeId(&guess, NDB_MGM_NODE_TYPE_NDB))
  {
    trp_node node = ss.getNodeInfo(guess);
    if (node.m_state.startLevel == NodeState::SL_STARTED)
    {
      return guess;
    }
  }

  /**
   * Check any confirmed node
   */
  guess = 0;
  while(getNextNodeId(&guess, NDB_MGM_NODE_TYPE_NDB))
  {
    trp_node node = ss.getNodeInfo(guess);
    if (node.is_confirmed())
    {
      return guess;
    }
  }

  /**
   * Check any connected node
   */
  guess = 0;
  while(getNextNodeId(&guess, NDB_MGM_NODE_TYPE_NDB))
  {
    trp_node node = ss.getNodeInfo(guess);
    if (node.is_connected())
    {
      return guess;
    }
  }

  return 0; // give up
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
			   NodeBitmask &stoppedNodes,
			   bool abort,
			   bool stop,
			   bool restart,
			   bool nostart,
			   bool initialStart,
                           int* stopSelf)
{
  int error = 0;
  DBUG_ENTER("MgmtSrvr::sendSTOP_REQ");
  DBUG_PRINT("enter", ("no of nodes: %d "
                       "abort: %d  stop: %d  restart: %d  "
                       "nostart: %d  initialStart: %d",
                       node_ids.size(),
                       abort, stop, restart, nostart, initialStart));

  stoppedNodes.clear();
  *stopSelf= 0;

  NodeBitmask ndb_nodes_to_stop;
  NodeBitmask mgm_nodes_to_stop;

  SignalSender ss(theFacade);
  ss.lock(); // lock will be released on exit

  /**
   * First verify arguments
   */
  for (unsigned i = 0; i < node_ids.size(); i++)
  {
    switch(getNodeType(node_ids[i])){
    case NDB_MGM_NODE_TYPE_MGM:
      mgm_nodes_to_stop.set(node_ids[i]);
      break;
    case NDB_MGM_NODE_TYPE_NDB:
      ndb_nodes_to_stop.set(node_ids[i]);
      break;
    default:
      DBUG_RETURN(WRONG_PROCESS_TYPE);
    }
  }

  /**
   * Process ndb_mgmd
   */
  for (Uint32 i = mgm_nodes_to_stop.find(0);
       i != mgm_nodes_to_stop.NotFound;
       i = mgm_nodes_to_stop.find(i + 1))
  {
    if (i != getOwnNodeId())
    {
      error= sendStopMgmd(i, abort, stop, restart,
                          nostart, initialStart);
      if (error == 0)
      {
        stoppedNodes.set(i);
      }
    }
    else
    {
      g_eventLogger->info("Stopping this node");
      * stopSelf = (restart)? -1 : 1;
      stoppedNodes.set(i);
    }
  }

  /**
   * Process ndbd
   */
  SimpleSignal ssig;
  StopReq* const stopReq = CAST_PTR(StopReq, ssig.getDataPtrSend());
  ssig.set(ss, TestOrd::TraceAPI, NDBCNTR, GSN_STOP_REQ, StopReq::SignalLength);

  stopReq->requestInfo = 0;
  stopReq->apiTimeout = 5000;
  stopReq->transactionTimeout = 1000;
  stopReq->readOperationTimeout = 1000;
  stopReq->operationTimeout = 1000;
  stopReq->senderData = 12;
  stopReq->senderRef = ss.getOwnRef();
  stopReq->singleuser = 0;
  StopReq::setSystemStop(stopReq->requestInfo, stop);
  StopReq::setPerformRestart(stopReq->requestInfo, restart);
  StopReq::setStopAbort(stopReq->requestInfo, abort);
  StopReq::setNoStart(stopReq->requestInfo, nostart);
  StopReq::setInitialStart(stopReq->requestInfo, initialStart);

  int use_master_node = 0;
  int do_send = 0;
  if (ndb_nodes_to_stop.count() > 1)
  {
    do_send = 1;
    use_master_node = 1;
    ndb_nodes_to_stop.copyto(NdbNodeBitmask::Size, stopReq->nodes);
    StopReq::setStopNodes(stopReq->requestInfo, 1);
  }
  else if (ndb_nodes_to_stop.count() == 1)
  {
    Uint32 nodeId = ndb_nodes_to_stop.find(0);
    if (okToSendTo(nodeId, true) == 0)
    {
      SendStatus result = ss.sendSignal(nodeId, &ssig);
      if (result != SEND_OK)
      {
        DBUG_RETURN(SEND_OR_RECEIVE_FAILED);
      }
    }
    else
    {
      DBUG_RETURN(SEND_OR_RECEIVE_FAILED);
    }
  }


  // now wait for the replies
  Uint32 sendNodeId = ndb_nodes_to_stop.find(0);
  while (!stoppedNodes.contains(ndb_nodes_to_stop))
  {
    if (do_send)
    {
      assert(use_master_node);
      sendNodeId = guess_master_node(ss);
      if (okToSendTo(sendNodeId, true) != 0)
      {
        DBUG_RETURN(SEND_OR_RECEIVE_FAILED);
      }

      if (ss.sendSignal(sendNodeId, &ssig) != SEND_OK)
      {
        DBUG_RETURN(SEND_OR_RECEIVE_FAILED);
      }
      do_send = 0;
    }

    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_STOP_REF:{
      const StopRef * const ref = CAST_CONSTPTR(StopRef, signal->getDataPtr());
      const NodeId nodeId = refToNode(signal->header.theSendersBlockRef);
      require(nodeId == sendNodeId);
      if (ref->errorCode == StopRef::MultiNodeShutdownNotMaster)
      {
        assert(use_master_node);
        m_master_node= ref->masterNodeId;
        do_send = 1;
        continue;
      }
      DBUG_RETURN(translateStopRef(ref->errorCode));
      break;
    }
    case GSN_STOP_CONF:{
#ifdef NOT_USED
      const StopConf * const ref = CAST_CONSTPTR(StopConf, signal->getDataPtr());
#endif
      const NodeId nodeId = refToNode(signal->header.theSendersBlockRef);
      require(nodeId == sendNodeId);
      stoppedNodes.bitOR(ndb_nodes_to_stop);
      break;
    }
    case GSN_NF_COMPLETEREP:{
      const NFCompleteRep * const rep =
	CAST_CONSTPTR(NFCompleteRep, signal->getDataPtr());
      if (rep->failedNodeId <= stoppedNodes.max_size())
        stoppedNodes.set(rep->failedNodeId);
      break;
    }
    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
	CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      require(len == NodeBitmask::Size); // only full length in ndbapi
      NodeBitmask mask;
      mask.assign(len, rep->theAllNodes);
      stoppedNodes.bitOR(mask);
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      continue;
    default:
      report_unknown_signal(signal);
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
                        int *stopCount, bool abort, bool force,
                        int* stopSelf)
{
  if (force || abort)
    ; // Skip node state checks
  else if (is_any_node_starting())
  {
    /* Refuse to stop since some node(s) are starting */
    return OPERATION_NOT_ALLOWED_START_STOP;
  }

  NodeBitmask nodes;
  int ret = 0;
  if (node_ids.size() > 0)
  {
    ret = sendSTOP_REQ(node_ids, nodes,
                       abort, false, false, false, false,
                       stopSelf);
  }
  else
  {
    ret = sendall_STOP_REQ(nodes,
                           abort, false, false, false, false);
  }

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
      (*stopCount)++;
  }

  *stopSelf= 1;
  (*stopCount)++;

  return 0;
}

/*
 * Perform DB nodes shutdown.
 * MGM servers are left in their current state
 */

int MgmtSrvr::shutdownDB(int * stopCount, bool abort)
{
  NodeBitmask nodes;

  int ret = sendall_STOP_REQ(nodes,
                             abort,
                             true,
                             false,
                             false,
                             false);

  if (stopCount)
    *stopCount = nodes.count();
  return ret;
}

/*
 * Enter single user mode on all live nodes
 */

int MgmtSrvr::enterSingleUser(int * stopCount, Uint32 apiNodeId)
{
  if (getNodeType(apiNodeId) != NDB_MGM_NODE_TYPE_API)
    return NODE_NOT_API_NODE;

  // Init
  if (stopCount)
  {
    * stopCount = 0;
  }

  SignalSender ss(theFacade);
  ss.lock(); // lock will be released on exit

  SimpleSignal ssig;
  StopReq* const stopReq = CAST_PTR(StopReq, ssig.getDataPtrSend());
  ssig.set(ss, TestOrd::TraceAPI, NDBCNTR, GSN_STOP_REQ, StopReq::SignalLength);

  stopReq->requestInfo = 0;
  stopReq->apiTimeout = 5000;
  stopReq->transactionTimeout = 1000;
  stopReq->readOperationTimeout = 1000;
  stopReq->operationTimeout = 1000;
  stopReq->senderData = 12;
  stopReq->senderRef = ss.getOwnRef();
  stopReq->singleuser = 1;
  stopReq->singleUserApi = apiNodeId;
  StopReq::setSystemStop(stopReq->requestInfo, false);
  StopReq::setPerformRestart(stopReq->requestInfo, false);
  StopReq::setStopAbort(stopReq->requestInfo, false);

  NodeBitmask nodes;
  {
    NodeId nodeId = 0;
    Uint32 failed = 0;
    while (getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB))
    {
      if (okToSendTo(nodeId, true) == 0)
      {
	SendStatus result = ss.sendSignal(nodeId, &ssig);
	if (result == SEND_OK)
	  nodes.set(nodeId);
        else
          failed++;
      }
      else
      {
        failed++;
      }
    }
    if (nodes.isclear())
    {
      if (failed)
      {
        return SEND_OR_RECEIVE_FAILED;
      }
      return NO_CONTACT_WITH_DB_NODES;
    }
  }

  int error = 0;
  int ok = 0;
  while (!nodes.isclear())
  {
    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_STOP_REF:
    {
      const StopRef * const ref = CAST_CONSTPTR(StopRef, signal->getDataPtr());
      nodes.clear(refToNode(signal->header.theSendersBlockRef));
      error = translateStopRef(ref->errorCode);
      break;
    }
    case GSN_STOP_CONF:
    {
      ok++;
      nodes.clear(refToNode(signal->header.theSendersBlockRef));
      break;
    }
    case GSN_NF_COMPLETEREP:
    {
      const NFCompleteRep * rep = CAST_CONSTPTR(NFCompleteRep,
                                                signal->getDataPtr());
      if (rep->failedNodeId <= nodes.max_size())
        nodes.clear(rep->failedNodeId);
      break;
    }

    case GSN_NODE_FAILREP:
    {
      const NodeFailRep * rep = CAST_CONSTPTR(NodeFailRep,
                                              signal->getDataPtr());
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      NodeBitmask mask;
      mask.assign(len, rep->theAllNodes);
      nodes.bitANDC(mask);
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      continue;

    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }

  if (stopCount)
  {
    * stopCount = ok;
  }

  return error;
}

/*
 * Perform node restart
 */

bool MgmtSrvr::is_any_node_stopping()
{
  NodeId nodeId = 0;
  trp_node node;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB))
  {
    node = getNodeInfo(nodeId);
    if((node.m_state.startLevel == NodeState::SL_STOPPING_1) || 
       (node.m_state.startLevel == NodeState::SL_STOPPING_2) || 
       (node.m_state.startLevel == NodeState::SL_STOPPING_3) || 
       (node.m_state.startLevel == NodeState::SL_STOPPING_4))
      return true; // At least one node was stopping
  }
  return false; // No node was stopping
}

bool MgmtSrvr::is_any_node_starting()
{
  NodeId nodeId = 0;
  trp_node node;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB))
  {
    node = getNodeInfo(nodeId);
    if (node.m_state.startLevel == NodeState::SL_STARTING)
      return true; // At least one node was starting
  }
  return false; // No node was starting
}

bool MgmtSrvr::is_cluster_single_user()
{
  NodeId nodeId = 0;
  trp_node node;
  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB))
  {
    node = getNodeInfo(nodeId);
    if (node.m_state.startLevel == NodeState::SL_SINGLEUSER)
      return true; // Cluster is in single user modes
  }
  return false; // Cluster is not in single user mode
}

int MgmtSrvr::restartNodes(const Vector<NodeId> &node_ids,
                           int * stopCount, bool nostart,
                           bool initialStart, bool abort,
                           bool force,
                           int *stopSelf,
                           unsigned int num_secs_to_wait_for_node)
{
  if (is_cluster_single_user())
  {
    /*
      Refuse to restart since cluster is in single user mode
      and when the node is restarting it would not be allowed to
      join cluster, see BUG#31056
    */
    return OPERATION_NOT_ALLOWED_START_STOP;
  }

  if (force || abort)
    ; // Skip node state checks
  else if (is_any_node_starting())
  {
    /* Refuse to restart since some node(s) are starting */
    return OPERATION_NOT_ALLOWED_START_STOP;
  }

  NodeBitmask nodes;
  int ret = 0;
  if (node_ids.size() > 0)
  {
    ret = sendSTOP_REQ(node_ids, nodes,
                       abort, false, true, true, initialStart,
                       stopSelf);
  }
  else
  {
    ret = sendall_STOP_REQ(nodes,
                           abort, false, true, true, initialStart);
  }

  if (ret)
    return ret;

  if (stopCount)
    *stopCount = nodes.count();
  
  // start up the nodes again
  const Uint64 waitTime = 12000;
  const NDB_TICKS startTime = NdbTick_getCurrentTicks();
  for (unsigned i = 0; i < node_ids.size(); i++)
  {
    NodeId nodeId= node_ids[i];
    enum ndb_mgm_node_status s;
    s = NDB_MGM_NODE_STATUS_NO_CONTACT;
#ifdef VM_TRACE
    ndbout_c("Waiting for %d not started", nodeId);
#endif
    while (s != NDB_MGM_NODE_STATUS_NOT_STARTED &&
           NdbTick_Elapsed(startTime,NdbTick_getCurrentTicks()).milliSec() < waitTime)
    {
      Uint32 startPhase = 0, version = 0, dynamicId = 0, nodeGroup = 0;
      Uint32 mysql_version = 0;
      Uint32 connectCount = 0;
      bool system;
      const char *address= NULL;
      char addr_buf[NDB_ADDR_STRLEN];
      status(nodeId, &s, &version, &mysql_version, &startPhase, 
             &system, &dynamicId, &nodeGroup, &connectCount,
             &address, addr_buf, sizeof(addr_buf));
      NdbSleep_MilliSleep(100);  
    }
  }

  if (nostart)
    return 0;

  /*
    verify that no nodes are stopping before starting as this would cause
    the starting node to shutdown
  */
  int retry= 600*10;
  for (;is_any_node_stopping();)
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
    unsigned int loop_count = 0;
    do
    {
      int result = sendSTART_ORD(node_ids[i]);
      if (result == SEND_OR_RECEIVE_FAILED ||
          result == NO_CONTACT_WITH_PROCESS)
      {
        if (loop_count >= num_secs_to_wait_for_node)
          break;
        loop_count++;
        NdbSleep_MilliSleep(1000);
      }
      else
      {
        break;
      }
    } while (1);
  }
  return 0;
}

/*
 * Perform restart of all DB nodes
 */

int MgmtSrvr::restartDB(bool nostart, bool initialStart,
                        bool abort, int * stopCount,
                        unsigned int num_secs_to_wait_for_node)
{
  NodeBitmask nodes;

  int ret = sendall_STOP_REQ(nodes,
                             abort,
                             true,
                             true,
                             true,
                             initialStart);

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
  NodeId nodeId = 0;
  const Uint64 waitTime = 12000;
  const NDB_TICKS startTime = NdbTick_getCurrentTicks();


  while(getNextNodeId(&nodeId, NDB_MGM_NODE_TYPE_NDB)) {
    if (!nodes.get(nodeId))
      continue;
    enum ndb_mgm_node_status s;
    s = NDB_MGM_NODE_STATUS_NO_CONTACT;
#ifdef VM_TRACE
    ndbout_c("Waiting for %d not started", nodeId);
#endif
    while (s != NDB_MGM_NODE_STATUS_NOT_STARTED &&
           NdbTick_Elapsed(startTime,NdbTick_getCurrentTicks()).milliSec() < waitTime)
    {
      Uint32 startPhase = 0, version = 0, dynamicId = 0, nodeGroup = 0;
      Uint32 mysql_version = 0;
      Uint32 connectCount = 0;
      bool system;
      const char *address;
      char addr_buf[NDB_ADDR_STRLEN];
      status(nodeId, &s, &version, &mysql_version, &startPhase, 
	     &system, &dynamicId, &nodeGroup, &connectCount,
             &address, addr_buf, sizeof(addr_buf));
      NdbSleep_MilliSleep(100);  
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
    unsigned int loop_count = 0;
    do
    {
      result = sendSTART_ORD(nodeId);
      if (result != SEND_OR_RECEIVE_FAILED &&
          result != NO_CONTACT_WITH_PROCESS)
        break;
      if (loop_count >= num_secs_to_wait_for_node)
        break;
      NdbSleep_MilliSleep(1000);
      loop_count++;
    } while (1);
    g_eventLogger->debug("Started node %d with result %d", nodeId, result);
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

void
MgmtSrvr::status_mgmd(NodeId node_id,
                      ndb_mgm_node_status& node_status,
                      Uint32& version, Uint32& mysql_version,
                      const char **address,
                      char *addr_buf,
                      size_t addr_buf_size)
{
  assert(getNodeType(node_id) == NDB_MGM_NODE_TYPE_MGM);

  if (node_id == getOwnNodeId())
  {
    /*
      Special case to get version of own node
      - version and mysql_version is hardcoded
      - address should be the address seen from ndbd(if it's connected)
        else use HostName from config
    */
    Uint32 tmp_version = 0, tmp_mysql_version = 0;
    sendVersionReq(node_id,
                   tmp_version,
                   tmp_mysql_version,
                   address,
                   addr_buf,
                   addr_buf_size);
    // Check that the version returned is equal to compiled in version
    assert(tmp_version == 0 ||
           (tmp_version == NDB_VERSION &&
            tmp_mysql_version == NDB_MYSQL_VERSION_D));

    version = NDB_VERSION;
    mysql_version = NDB_MYSQL_VERSION_D;
    if(!*address)
    {
      // No address returned from ndbd -> get HostName from config
      Guard g(m_local_config_mutex);
      ConfigIter iter(m_local_config, CFG_SECTION_NODE);
      require(iter.find(CFG_NODE_ID, node_id) == 0);
      require(iter.get(CFG_NODE_HOST, address) == 0);

      /*
        Try to convert HostName to numerical ip address
        (to get same output as if ndbd had replied)
      */
      struct in_addr addr;
      if (Ndb_getInAddr(&addr, *address) == 0)
      {
        *address = Ndb_inet_ntop(AF_INET,
                                 static_cast<void*>(&addr),
                                 addr_buf,
                                 (socklen_t)addr_buf_size);
      }
    }

    node_status = NDB_MGM_NODE_STATUS_CONNECTED;
    return;
  }

  /*
    MGM nodes are connected directly to all other MGM
    node(s), return status as seen by ClusterMgr
  */
  const trp_node node = getNodeInfo(node_id);
  if(node.is_connected())
  {
    version = node.m_info.m_version;
    mysql_version = node.m_info.m_mysql_version;
    node_status = NDB_MGM_NODE_STATUS_CONNECTED;
    *address= get_connect_address(node_id,
                                  addr_buf,
                                  addr_buf_size);
  }
  else
  {
    version = 0;
    mysql_version = 0;
    node_status = NDB_MGM_NODE_STATUS_NO_CONTACT;
  }

  return;
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
		 const char **address,
                 char *addr_buf,
                 size_t addr_buf_size)
{
  switch(getNodeType(nodeId)){
  case NDB_MGM_NODE_TYPE_API:
    status_api(nodeId,
               *_status,
               *version,
               *mysql_version,
               address,
               addr_buf,
               addr_buf_size);
    return 0;
    break;

  case NDB_MGM_NODE_TYPE_MGM:
    status_mgmd(nodeId,
                *_status,
                *version,
                *mysql_version,
                address,
                addr_buf,
                addr_buf_size);
    return 0;
    break;

  case NDB_MGM_NODE_TYPE_NDB:
    break;

  default:
    abort();
    break;
  }

  const trp_node node = getNodeInfo(nodeId);
  assert(getNodeType(nodeId) == NDB_MGM_NODE_TYPE_NDB &&
         node.m_info.getType() == NodeInfo::DB);

  if(!node.is_connected()){
    * _status = NDB_MGM_NODE_STATUS_NO_CONTACT;
    return 0;
  }

  * version = node.m_info.m_version;
  * mysql_version = node.m_info.m_mysql_version;

  *address= get_connect_address(nodeId, addr_buf, addr_buf_size);

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
        if (getNodeInfo(nodeId).is_connected()  == false)
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
      if (getNodeInfo(nodeId).is_connected()  == false)
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
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      require(len == NodeBitmask::Size); // only full length in ndbapi
      NdbNodeBitmask mask;
      // only care about data nodes
      mask.assign(NdbNodeBitmask::Size, rep->theNodes);
      nodes.bitANDC(mask);
      break;
    }
      
    case GSN_NF_COMPLETEREP:{
      const NFCompleteRep * const rep =
	CAST_CONSTPTR(NFCompleteRep, signal->getDataPtr());
      if (rep->failedNodeId <= nodes.max_size())
        nodes.clear(rep->failedNodeId);
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
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
MgmtSrvr::insertError(int nodeId, int errorNo, Uint32 * extra)
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

  if (extra)
  {
    ssig.getDataPtrSend()[1] = * extra;
    ssig.header.theLength++;
  }

  int res = ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 :SEND_OR_RECEIVE_FAILED;

  if (res == 0)
  {
    /**
     * In order to make NDB_TAMPER (almost) syncronous,
     *   make a syncronous request *after* the NDB_TAMPER
     */
    make_sync_req(ss, Uint32(nodeId));
  }

  return res;
}


int
MgmtSrvr::startSchemaTrans(SignalSender& ss, NodeId & out_nodeId,
                           Uint32 transId, Uint32 & out_transKey)
{
  SimpleSignal ssig;

  ssig.set(ss, 0, DBDICT, GSN_SCHEMA_TRANS_BEGIN_REQ,
           SchemaTransBeginReq::SignalLength);

  SchemaTransBeginReq* req =
    CAST_PTR(SchemaTransBeginReq, ssig.getDataPtrSend());

  req->clientRef =  ss.getOwnRef();
  req->transId = transId;
  req->requestInfo = 0;

  NodeId nodeId = ss.get_an_alive_node();

retry:
  if (ss.get_node_alive(nodeId) == false)
  {
    nodeId = ss.get_an_alive_node();
  }

  if (ss.sendSignal(nodeId, &ssig) != SEND_OK)
  {
    return SEND_OR_RECEIVE_FAILED;
  }

  while (true)
  {
    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_SCHEMA_TRANS_BEGIN_CONF: {
      const SchemaTransBeginConf * conf =
        CAST_CONSTPTR(SchemaTransBeginConf, signal->getDataPtr());
      out_transKey = conf->transKey;
      out_nodeId = nodeId;
      return 0;
    }
    case GSN_SCHEMA_TRANS_BEGIN_REF: {
      const SchemaTransBeginRef * ref =
        CAST_CONSTPTR(SchemaTransBeginRef, signal->getDataPtr());

      switch(ref->errorCode){
      case SchemaTransBeginRef::NotMaster:
        nodeId = ref->masterNodeId;
        // Fall-through
      case SchemaTransBeginRef::Busy:
      case SchemaTransBeginRef::BusyWithNR:
        goto retry;
      default:
        return ref->errorCode;
      }
    }
    case GSN_NF_COMPLETEREP:
      // ignore
      break;
    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
        CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      if (BitmaskImpl::safe_get(len, rep->theAllNodes, nodeId))
      {
        nodeId++;
        goto retry;
      }
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      break;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }
}

int
MgmtSrvr::endSchemaTrans(SignalSender& ss, NodeId nodeId,
                         Uint32 transId, Uint32 transKey,
                         Uint32 flags)
{
  SimpleSignal ssig;

  ssig.set(ss, 0, DBDICT, GSN_SCHEMA_TRANS_END_REQ,
           SchemaTransEndReq::SignalLength);

  SchemaTransEndReq* req =
    CAST_PTR(SchemaTransEndReq, ssig.getDataPtrSend());

  req->clientRef =  ss.getOwnRef();
  req->transId = transId;
  req->requestInfo = 0;
  req->transKey = transKey;
  req->flags = flags;

  if (ss.sendSignal(nodeId, &ssig) != SEND_OK)
  {
    return SEND_OR_RECEIVE_FAILED;
  }

  while (true)
  {
    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_SCHEMA_TRANS_END_CONF: {
      return 0;
    }
    case GSN_SCHEMA_TRANS_END_REF: {
      const SchemaTransEndRef * ref =
        CAST_CONSTPTR(SchemaTransEndRef, signal->getDataPtr());
      return ref->errorCode;
    }
    case GSN_NF_COMPLETEREP:
      // ignore
      break;
    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
        CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      if (BitmaskImpl::safe_get(len, rep->theAllNodes, nodeId))
      {
        return -1;
      }
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      break;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }
}

int
MgmtSrvr::createNodegroup(int *nodes, int count, int *ng)
{
  int res;
  SignalSender ss(theFacade);
  ss.lock();

  Uint32 transId = rand();
  Uint32 transKey;
  NodeId nodeId;

  if ((res = startSchemaTrans(ss, nodeId, transId, transKey)))
  {
    return res;
  }

  SimpleSignal ssig;
  ssig.set(ss, 0, DBDICT, GSN_CREATE_NODEGROUP_REQ,
           CreateNodegroupReq::SignalLength);

  CreateNodegroupReq* req =
    CAST_PTR(CreateNodegroupReq, ssig.getDataPtrSend());

  req->transId = transId;
  req->transKey = transKey;
  req->nodegroupId = RNIL;
  req->senderData = 77;
  req->senderRef = ss.getOwnRef();
  bzero(req->nodes, sizeof(req->nodes));

  if (ng)
  {
    if (* ng != -1)
    {
      req->nodegroupId = * ng;
    }
  }
  for (int i = 0; i<count && i<(int)NDB_ARRAY_SIZE(req->nodes); i++)
  {
    req->nodes[i] = nodes[i];
  }

  if (ss.sendSignal(nodeId, &ssig) != SEND_OK)
  {
    return SEND_OR_RECEIVE_FAILED;
  }

  bool wait = true;
  while (wait)
  {
    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_CREATE_NODEGROUP_CONF: {
      const CreateNodegroupConf * conf =
        CAST_CONSTPTR(CreateNodegroupConf, signal->getDataPtr());

      if (ng)
      {
        * ng = conf->nodegroupId;
      }

      wait = false;
      break;
    }
    case GSN_CREATE_NODEGROUP_REF:{
      const CreateNodegroupRef * ref =
        CAST_CONSTPTR(CreateNodegroupRef, signal->getDataPtr());
      Uint32 err = ref->errorCode;
      endSchemaTrans(ss, nodeId, transId, transKey,
                     SchemaTransEndReq::SchemaTransAbort);
      return err;
    }
    case GSN_NF_COMPLETEREP:
      // ignore
      break;
    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
        CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      if (BitmaskImpl::safe_get(len, rep->theAllNodes, nodeId))
      {
        return SchemaTransBeginRef::Nodefailure;
      }
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      break;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }

  return endSchemaTrans(ss, nodeId, transId, transKey, 0);
}

int
MgmtSrvr::dropNodegroup(int ng)
{
  int res;
  SignalSender ss(theFacade);
  ss.lock();

  Uint32 transId = rand();
  Uint32 transKey;
  NodeId nodeId;

  if ((res = startSchemaTrans(ss, nodeId, transId, transKey)))
  {
    return res;
  }

  SimpleSignal ssig;
  ssig.set(ss, 0, DBDICT, GSN_DROP_NODEGROUP_REQ, DropNodegroupReq::SignalLength);

  DropNodegroupReq* req =
    CAST_PTR(DropNodegroupReq, ssig.getDataPtrSend());

  req->transId = transId;
  req->transKey = transKey;
  req->nodegroupId = ng;
  req->senderData = 77;
  req->senderRef = ss.getOwnRef();

  if (ss.sendSignal(nodeId, &ssig) != SEND_OK)
  {
    return SEND_OR_RECEIVE_FAILED;
  }

  bool wait = true;
  while (wait)
  {
    SimpleSignal *signal = ss.waitFor();
    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_DROP_NODEGROUP_CONF: {
      wait = false;
      break;
    }
    case GSN_DROP_NODEGROUP_REF:
    {
      const DropNodegroupRef * ref =
        CAST_CONSTPTR(DropNodegroupRef, signal->getDataPtr());
      endSchemaTrans(ss, nodeId, transId, transKey,
                     SchemaTransEndReq::SchemaTransAbort);
      return ref->errorCode;
    }
    case GSN_NF_COMPLETEREP:
      // ignore
      break;
    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
        CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      if (BitmaskImpl::safe_get(len, rep->theAllNodes, nodeId))
      {
        return SchemaTransBeginRef::Nodefailure;
      }
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      break;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }

  return endSchemaTrans(ss, nodeId, transId, transKey, 0);
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
    for(unsigned i = 0; i < blocks.size(); i++)
    {
      BlockNumber blockNumber = getBlockNo(blocks[i].c_str());
      if (blockNumber == 0)
        return INVALID_BLOCK_NAME;
      testOrd->addSignalLoggerCommand(blockNumber, command, logSpec);
    }
  }

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
  
  int res = ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 :SEND_OR_RECEIVE_FAILED;

  if (res == 0)
  {
    /**
     * In order to make DUMP (almost) syncronous,
     *   make a syncronous request *after* the NDB_TAMPER
     */
    make_sync_req(ss, Uint32(nodeId));
  }

  return res;

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
MgmtSrvr::trp_deliver_signal(const NdbApiSignal* signal,
                             const LinearSectionPtr ptr[3])
{
  int gsn = signal->readSignalNumber();

  switch (gsn) {
  case GSN_EVENT_REP:
  {
    eventReport(signal->getDataPtr(), signal->getLength());
    break;
  }

  case GSN_NF_COMPLETEREP:{
    const NFCompleteRep * rep = CAST_CONSTPTR(NFCompleteRep,
                                               signal->getDataPtr());
    /* Clear local nodeid reservation(if any) */
    release_local_nodeid_reservation(rep->failedNodeId);

     clear_connect_address_cache(rep->failedNodeId);
    break;
  }
  case GSN_TAMPER_ORD:
    ndbout << "TAMPER ORD" << endl;
    break;
  case GSN_API_REGCONF:
  case GSN_TAKE_OVERTCCONF:
    break;
  case GSN_CONNECT_REP:{
    const Uint32 nodeId = signal->getDataPtr()[0];

    /*
      Clear local nodeid reservation since nodeid is
      now reserved by a connected transporter
    */
    release_local_nodeid_reservation(nodeId);

    union {
      Uint32 theData[25];
      EventReport repData;
    };
    EventReport * rep = &repData;
    theData[1] = nodeId;
    rep->setEventType(NDB_LE_Connected);

    if (nodeTypes[nodeId] == NODE_TYPE_DB)
    {
      m_started_nodes.push_back(nodeId);
    }
    rep->setEventType(NDB_LE_Connected);
    rep->setNodeId(_ownNodeId);
    eventReport(theData, 1);
    return;
  }
  case GSN_NODE_FAILREP:
  {
    union {
      Uint32 theData[25];
      EventReport repData;
    };
    bzero(theData, sizeof(theData));
    EventReport * event = &repData;
    event->setEventType(NDB_LE_Disconnected);
    event->setNodeId(_ownNodeId);

    const NodeFailRep *rep = CAST_CONSTPTR(NodeFailRep,
                                           signal->getDataPtr());
    Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
    assert(len == NodeBitmask::Size); // only full length in ndbapi
    for (Uint32 i = BitmaskImpl::find_first(len, rep->theAllNodes);
         i != BitmaskImpl::NotFound;
         i = BitmaskImpl::find_next(len, rep->theAllNodes, i + 1))
    {
      theData[1] = i;
      eventReport(theData, 1);

      /* Clear local nodeid reservation(if any) */
      release_local_nodeid_reservation(i);

      clear_connect_address_cache(i);
    }
    return;
  }
  case GSN_CLOSE_COMREQ:
  {
    theFacade->perform_close_clnt(this);
    break;
  }
  default:
    g_eventLogger->error("Unknown signal received. SignalNumber: "
                         "%i from (%d, 0x%x)",
                         gsn,
                         refToNode(signal->theSendersBlockRef),
                         refToBlock(signal->theSendersBlockRef));
    assert(false);
  }
}


void
MgmtSrvr::trp_node_status(Uint32 nodeId, Uint32 _event)
{
}

enum ndb_mgm_node_type 
MgmtSrvr::getNodeType(NodeId nodeId) const 
{
  if(nodeId >= MAX_NODES)
    return (enum ndb_mgm_node_type)-1;
  
  return nodeTypes[nodeId];
}


const char*
MgmtSrvr::get_connect_address(NodeId node_id,
                              char *addr_buf,
                              size_t addr_buf_size)
{
  assert(node_id < NDB_ARRAY_SIZE(m_connect_address));

  if (m_connect_address[node_id].s_addr == 0)
  {
    // No cached connect address available
    const trp_node &node= getNodeInfo(node_id);
    if (node.is_connected())
    {
      // Cache the connect address, it's valid until
      // node disconnects
      m_connect_address[node_id] = theFacade->ext_get_connect_address(node_id);
    }
  }

  // Return the cached connect address
  return Ndb_inet_ntop(AF_INET,
                       static_cast<void*>(&m_connect_address[node_id]),
                       addr_buf,
                       (socklen_t)addr_buf_size);
}


void
MgmtSrvr::clear_connect_address_cache(NodeId nodeid)
{
  assert(nodeid < NDB_ARRAY_SIZE(m_connect_address));
  if (nodeid < NDB_ARRAY_SIZE(m_connect_address))
  {
    m_connect_address[nodeid].s_addr = 0;
  }
}

/***************************************************************************
 * Alloc nodeid
 ***************************************************************************/

MgmtSrvr::NodeIdReservations::NodeIdReservations()
{
  memset(m_reservations, 0, sizeof(m_reservations));
}


void
MgmtSrvr::NodeIdReservations::check_array(NodeId n) const
{
  assert( n < NDB_ARRAY_SIZE(m_reservations));
}


bool
MgmtSrvr::NodeIdReservations::get(NodeId n) const
{
  check_array(n);

  return (m_reservations[n].m_timeout != 0);
}


void
MgmtSrvr::NodeIdReservations::set(NodeId n, unsigned timeout)
{
  check_array(n);

  Reservation& r = m_reservations[n];
  // Dont't allow double set
  assert(r.m_timeout == 0 && !NdbTick_IsValid(r.m_start));

  r.m_timeout = timeout;
  r.m_start = NdbTick_getCurrentTicks();
}


BaseString
MgmtSrvr::NodeIdReservations::pretty_str() const
{
  const char* sep = "";
  BaseString str;
  for (size_t i = 0; i < NDB_ARRAY_SIZE(m_reservations); i++)
  {
    const Reservation& r = m_reservations[i];
    if (r.m_timeout)
    {
      str.appfmt("%s%u", sep, (unsigned)i);
      sep = ",";
    }
  }
  return str;
}


void
MgmtSrvr::NodeIdReservations::clear(NodeId n)
{
  check_array(n);

  Reservation& r = m_reservations[n];
  // Dont't allow double clear
  assert(r.m_timeout != 0 && NdbTick_IsValid(r.m_start));

  r.m_timeout = 0;
  NdbTick_Invalidate(&r.m_start);
}


bool
MgmtSrvr::NodeIdReservations::has_timedout(NodeId n, NDB_TICKS now) const
{
  check_array(n);

  const Reservation& r = m_reservations[n];
  if (r.m_timeout && 
      NdbTick_Elapsed(r.m_start,now).milliSec() > r.m_timeout)
    return true;
  return false;
}


void
MgmtSrvr::release_local_nodeid_reservation(NodeId nodeid)
{
  NdbMutex_Lock(m_reserved_nodes_mutex);
  if (m_reserved_nodes.get(nodeid))
  {
    g_eventLogger->debug("Releasing local reservation for nodeid %d", nodeid);
    m_reserved_nodes.clear(nodeid);
  }
  NdbMutex_Unlock(m_reserved_nodes_mutex);
}


int
MgmtSrvr::alloc_node_id_req(NodeId free_node_id,
                            enum ndb_mgm_node_type type,
                            Uint32 timeout_ms)
{
  bool first_attempt = true;
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
            getNodeInfo(nodeId).is_confirmed() == false)
        ;
      if (!next)
        return NO_CONTACT_WITH_DB_NODES;
      do_send = 1;
    }
    if (do_send)
    {
      if (ss.sendSignal(nodeId, &ssig) != SEND_OK)
        return SEND_OR_RECEIVE_FAILED;
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
      g_eventLogger->info("Alloc node id %u succeeded", free_node_id);
      return 0;
    }
    case GSN_ALLOC_NODEID_REF:
    {
      const AllocNodeIdRef * const ref =
        CAST_CONSTPTR(AllocNodeIdRef, signal->getDataPtr());
      if (ref->errorCode == AllocNodeIdRef::NotMaster &&
          refToNode(ref->masterRef) == 0xFFFF)
      {
        /*
          The data nodes haven't decided who is the president (yet)
          and thus can't allocate nodeids -> return "no contact"
        */
        g_eventLogger->info("Alloc node id %u failed, no new president yet",
                            free_node_id);
        return NO_CONTACT_WITH_DB_NODES;
      }

      if (ref->errorCode == AllocNodeIdRef::NotMaster ||
          ref->errorCode == AllocNodeIdRef::Busy ||
          ref->errorCode == AllocNodeIdRef::NodeFailureHandlingNotCompleted)
      {
        do_send = 1;
        nodeId = refToNode(ref->masterRef);
	if (!getNodeInfo(nodeId).is_confirmed())
	  nodeId = 0;
        if (ref->errorCode != AllocNodeIdRef::NotMaster)
        {
          if (first_attempt)
          {
            first_attempt = false;
            g_eventLogger->info("Alloc node id %u failed with error code %u, will retry",
                                free_node_id,
                                ref->errorCode);
          }
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
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      if (BitmaskImpl::safe_get(len, rep->theAllNodes, nodeId))
      {
        do_send = 1;
        nodeId = 0;
      }
      continue;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      continue;
    default:
      report_unknown_signal(signal);
      return SEND_OR_RECEIVE_FAILED;
    }
  }
  return 0;
}

static int
match_hostname(const struct sockaddr *clnt_addr,
               const char *config_hostname)
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
MgmtSrvr::find_node_type(NodeId node_id,
                         ndb_mgm_node_type type,
                         const struct sockaddr* client_addr,
                         Vector<PossibleNode>& nodes,
                         int& error_code, BaseString& error_string)
{
  const char* found_config_hostname= 0;
  unsigned type_c= (unsigned)type;

  Guard g(m_local_config_mutex);

  ConfigIter iter(m_local_config, CFG_SECTION_NODE);
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
    bool exact_match = false;
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
      exact_match = true;
    }
    /*
      Insert this node in the nodes list sorted with the
      exact matches ahead of the open nodes
    */
    PossibleNode possible_node= {id, config_hostname, exact_match};
    if (exact_match)
    {
      // Find the position of first !exact match
      unsigned position = 0;
      for (unsigned j = 0; j < nodes.size(); j++)
      {
        if (nodes[j].exact_match)
          position++;
      }
      nodes.push(possible_node, position);
    }
    else
    {
      nodes.push_back(possible_node);
    }

    if (node_id)
      break;
  }
  if (nodes.size() != 0)
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
      char addr_buf[NDB_ADDR_STRLEN];
      char *addr_str;
      struct in_addr config_addr= {0};
      struct in_addr conn_addr =
        ((struct sockaddr_in*)(client_addr))->sin_addr;
      int r_config_addr= Ndb_getInAddr(&config_addr, found_config_hostname);
      addr_str = Ndb_inet_ntop(AF_INET,
                               static_cast<void*>(&conn_addr),
                               addr_buf,
                               (socklen_t)sizeof(addr_buf));
      error_string.appfmt("Connection with id %d done from wrong host ip %s,",
                          node_id, addr_str);
      addr_str = Ndb_inet_ntop(AF_INET,
                               static_cast<void*>(&config_addr),
                               addr_buf,
                               (socklen_t)sizeof(addr_buf));
      error_string.appfmt(" expected %s(%s).", found_config_hostname,
                          r_config_addr ?
                          "lookup failed" : addr_str);
      return -1;
    }
    error_string.appfmt("No node defined with id=%d in config file.", node_id);
    return -1;
  }

  // node_id == 0 and nodes.size() == 0
  if (found_config_hostname)
  {
    char addr_buf[NDB_ADDR_STRLEN];
    struct in_addr conn_addr =
      ((struct sockaddr_in*)(client_addr))->sin_addr;
    char *addr_str = Ndb_inet_ntop(AF_INET,
                                   static_cast<void*>(&conn_addr),
                                   addr_buf,
                                   (socklen_t)sizeof(addr_buf));
    error_string.appfmt("Connection done from wrong host ip %s.",
                        (client_addr) ? addr_str : "");
    return -1;
  }

  error_string.append("No nodes defined in config file.");
  return -1;
}


int
MgmtSrvr::try_alloc(NodeId id,
                    ndb_mgm_node_type type,
                    Uint32 timeout_ms)
{
  assert(type == NDB_MGM_NODE_TYPE_NDB ||
         type == NDB_MGM_NODE_TYPE_API);

  const NDB_TICKS start = NdbTick_getCurrentTicks();
  while (true)
  {
    int res = alloc_node_id_req(id, type, timeout_ms);
    if (res == 0)
    {
      /* Node id allocation suceeded */
      g_eventLogger->debug("Allocated nodeid %u in cluster", id);
      assert(id > 0);
      return id;
    }

    if (res == NO_CONTACT_WITH_DB_NODES &&
        type == NDB_MGM_NODE_TYPE_API)
    {
      const Uint64 retry_timeout = 3000; // milliseconds
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      const Uint64 elapsed = NdbTick_Elapsed(start,now).milliSec();
      if (elapsed > retry_timeout)
      {
        /*
          Have waited long enough time for data nodes to
          decide on a master, return error
        */
        g_eventLogger->debug("Failed to allocate nodeid %u for API node " \
                             "in cluster (retried during %u milliseconds)",
                             id, (unsigned)elapsed);
        return -1;
      }

      g_eventLogger->debug("Retrying allocation of nodeid %u...", id);
      NdbSleep_MilliSleep(100);
      continue;
    }

    if (res == NO_CONTACT_WITH_DB_NODES &&
        type == NDB_MGM_NODE_TYPE_NDB)
    {
      /*
        No reply from data node(s) -> use the requested nodeid
        so that data node can start
      */
      g_eventLogger->debug("Nodeid %u for data node reserved locally "  \
                           "since cluster was not available ", id);
      return id;
    }

    /* Unspecified error */
    return 0;
  }

  assert(false); // Never reached
  return 0;
}


bool
MgmtSrvr::try_alloc_from_list(NodeId& nodeid,
                              ndb_mgm_node_type type,
                              Uint32 timeout_ms,
                              Vector<PossibleNode>& nodes)
{
  for (unsigned i = 0; i < nodes.size(); i++)
  {
    const unsigned id= nodes[i].id;
    if (theFacade->ext_isConnected(id))
    {
      // Node is already reserved(connected via transporter)
      continue;
    }

    NdbMutex_Lock(m_reserved_nodes_mutex);
    if (m_reserved_nodes.get(id))
    {
      // Node is already reserved(locally in this node)
      NdbMutex_Unlock(m_reserved_nodes_mutex);
      continue;
    }

    /*
      Reserve the nodeid locally while checking if it can
      be allocated in the data nodes
    */
    m_reserved_nodes.set(id, timeout_ms);

    NdbMutex_Unlock(m_reserved_nodes_mutex);
    int res = try_alloc(id, type, timeout_ms);
    if (res > 0)
    {
      // Nodeid allocation succeeded
      nodeid= id;

      if (type == NDB_MGM_NODE_TYPE_API)
      {
        /*
          Release the local reservation(which was set to avoid that
          more than one thread asked for same nodeid) since it's
          now reserved in data node
        */
        release_local_nodeid_reservation(id);
      }

      return true;
    }

    /* Release the local reservation */
    release_local_nodeid_reservation(id);

    if (res < 0)
    {
      // Don't try any more nodes from the list
      return false;
    }
  }
  return false;
}


bool
MgmtSrvr::alloc_node_id_impl(NodeId& nodeid,
                             enum ndb_mgm_node_type type,
                             const struct sockaddr* client_addr,
                             int& error_code, BaseString& error_string,
                             Uint32 timeout_s)
{
  if (m_opts.no_nodeid_checks)
  {
    if (nodeid == 0)
    {
      error_string.appfmt("no-nodeid-checks set in management server. "
			  "node id must be set explicitly in connectstring");
      error_code = NDB_MGM_ALLOCID_CONFIG_MISMATCH;
      return false;
    }
    return true;
  }

  /* Don't allow allocation of this ndb_mgmd's nodeid */
  assert(_ownNodeId);
  if (nodeid == _ownNodeId)
  {
    // Fatal error
    error_code= NDB_MGM_ALLOCID_CONFIG_MISMATCH;
    if (type != NDB_MGM_NODE_TYPE_MGM)
    {
      /**
       * be backwards compatile wrt error messages
       */
      BaseString type_string, type_c_string;
      const char *alias, *str;
      alias= ndb_mgm_get_node_type_alias_string(type, &str);
      type_string.assfmt("%s(%s)", alias, str);
      alias= ndb_mgm_get_node_type_alias_string(NDB_MGM_NODE_TYPE_MGM, &str);
      type_c_string.assfmt("%s(%s)", alias, str);
      error_string.appfmt("Id %d configured as %s, connect attempted as %s.",
                          nodeid, type_c_string.c_str(),
                          type_string.c_str());
    }
    else
    {
      error_string.appfmt("Id %d is already allocated by this ndb_mgmd",
                          nodeid);
    }
    return false;
  }

  /* Make sure that config is confirmed before allocating nodeid */
  Uint32 timeout_ms = timeout_s * 1000;
  {
    const NDB_TICKS start = NdbTick_getCurrentTicks();
    BaseString getconfig_message;
    while (!m_config_manager->get_packed_config(type, 0, getconfig_message))
    {
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      if (NdbTick_Elapsed(start,now).milliSec() > timeout_ms)
      {
        error_code = NDB_MGM_ALLOCID_ERROR;
        error_string.append("Unable to allocate nodeid as configuration"
                            " not yet confirmed");
        return false;
      }

      NdbSleep_MilliSleep(20);
    }
  }

  /* Find possible nodeids */
  Vector<PossibleNode> nodes;
  if (find_node_type(nodeid, type, client_addr,
                     nodes, error_code, error_string))
    return false;

  // Print list of possible nodes
  for (unsigned i = 0; i < nodes.size(); i++)
  {
    const PossibleNode& node = nodes[i];
    g_eventLogger->debug(" [%u]: %u, '%s', %d",
                         (unsigned)i, node.id,
                         node.host.c_str(),
                         node.exact_match);
  }

  // nodes.size() == 0 handled inside find_node_type
  DBUG_ASSERT(nodes.size() != 0);

  if (type == NDB_MGM_NODE_TYPE_MGM && nodes.size() > 1)
  {
    // mgmt server may only have one match
    error_string.appfmt("Ambiguous node id's %d and %d. "
                        "Suggest specifying node id in connectstring, "
                        "or specifying unique host names in config file.",
                        nodes[0].id, nodes[1].id);
    error_code= NDB_MGM_ALLOCID_CONFIG_MISMATCH;
    return false;
  }

  /* Check timeout of nodeid reservations for NDB */
  if (type == NDB_MGM_NODE_TYPE_NDB)
  {
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    for (unsigned i = 0; i < nodes.size(); i++)
    {
      const NodeId ndb_nodeid = nodes[i].id;
      {
        Guard g(m_reserved_nodes_mutex);
        if (!m_reserved_nodes.has_timedout(ndb_nodeid, now))
          continue;
      }

      // Found a timedout reservation
      if (theFacade->ext_isConnected(ndb_nodeid))
        continue; // Still connected, ignore the timeout

      g_eventLogger->warning("Found timedout nodeid reservation for %u, " \
                             "releasing it", ndb_nodeid);

      // Clear the reservation
      release_local_nodeid_reservation(ndb_nodeid);
    }
  }

  if (try_alloc_from_list(nodeid, type, timeout_ms, nodes))
  {
    if (type == NDB_MGM_NODE_TYPE_NDB)
    {
      /* Be ready to accept connections from this node */
      theFacade->ext_doConnect(nodeid);
    }

    return true;
  }

  /*
    there are nodes with correct type available but
    allocation failed for some reason
  */
  if (nodeid)
  {
    error_string.appfmt("Id %d already allocated by another node.",
                        nodeid);
  }
  else
  {
    const char *alias, *str;
    alias= ndb_mgm_get_node_type_alias_string(type, &str);
    error_string.appfmt("No free node id found for %s(%s).",
                        alias, str);
  }
  error_code = NDB_MGM_ALLOCID_ERROR;
  return false;
}


bool
MgmtSrvr::alloc_node_id(NodeId& nodeid,
			enum ndb_mgm_node_type type,
			const struct sockaddr* client_addr,
			int& error_code, BaseString& error_string,
                        bool log_event,
                        Uint32 timeout_s)
{
  char addr_buf[NDB_ADDR_STRLEN];
  struct in_addr conn_addr = ((sockaddr_in*)client_addr)->sin_addr;
  const char* type_str = ndb_mgm_get_node_type_string(type);
  char* addr_str = Ndb_inet_ntop(AF_INET,
                                 static_cast<void*>(&conn_addr),
                                 addr_buf,
                                 (socklen_t)sizeof(addr_buf));

  g_eventLogger->debug("Trying to allocate nodeid for %s" \
                       "(nodeid: %u, type: %s)",
                       addr_str, (unsigned)nodeid, type_str);


  if (alloc_node_id_impl(nodeid, type, client_addr,
                         error_code, error_string,
                         timeout_s))
  {
    g_eventLogger->info("Nodeid %u allocated for %s at %s",
                        (unsigned)nodeid, type_str, addr_str);
    return true;
  }

  if (!log_event)
    return false;

  g_eventLogger->warning("Failed to allocate nodeid for %s at %s. "
                         "Returned error: '%s'",
                         type_str, addr_str, error_string.c_str());

  return false;
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
MgmtSrvr::startBackup(Uint32& backupId, int waitCompleted, Uint32 input_backupId, Uint32 backuppoint)
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
  /*
   * Single-threaded backup.  Set instance key 1.  In the kernel
   * this maps to main instance 0 or worker instance 1 (if MT LQH).
   */
  BlockNumber backupBlockNo = numberToBlock(BACKUP, 1);
  if(input_backupId > 0)
  {
    ssig.set(ss, TestOrd::TraceAPI, backupBlockNo, GSN_BACKUP_REQ,
	     BackupReq::SignalLength);
    req->inputBackupId = input_backupId;
  }
  else
    ssig.set(ss, TestOrd::TraceAPI, backupBlockNo, GSN_BACKUP_REQ,
	     BackupReq::SignalLength - 1);
  
  req->senderData = 19;
  req->backupDataLen = 0;
  assert(waitCompleted < 3);
  req->flags = waitCompleted & 0x3;
  if(backuppoint == 1)
    req->flags |= BackupReq::USE_UNDO_LOG;

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
	if (!getNodeInfo(nodeId).m_alive)
	  m_master_node = nodeId = 0;
	continue;
      }
      return ref->errorCode;
    }
    case GSN_BACKUP_ABORT_REP:{
      const BackupAbortRep * const rep = 
	CAST_CONSTPTR(BackupAbortRep, signal->getDataPtr());
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
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      if (BitmaskImpl::safe_get(len, rep->theAllNodes,nodeId) ||
	  waitCompleted == 1)
	return 1326;
      // wait for next signal
      // master node will report aborted backup
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
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
	getNodeInfo(nodeId).m_alive == false);
  
  if(!next){
    return NO_CONTACT_WITH_DB_NODES;
  }
  
  SimpleSignal ssig;

  AbortBackupOrd* ord = CAST_PTR(AbortBackupOrd, ssig.getDataPtrSend());
  /*
   * Single-threaded backup.  Set instance key 1.  In the kernel
   * this maps to main instance 0 or worker instance 1 (if MT LQH).
   */
  BlockNumber backupBlockNo = numberToBlock(BACKUP, 1);
  ssig.set(ss, TestOrd::TraceAPI, backupBlockNo, GSN_ABORT_BACKUP_ORD, 
	   AbortBackupOrd::SignalLength);
  
  ord->requestType = AbortBackupOrd::ClientAbort;
  ord->senderData = 19;
  ord->backupId = backupId;
  
  return ss.sendSignal(nodeId, &ssig) == SEND_OK ? 0 : SEND_OR_RECEIVE_FAILED;
}


int
MgmtSrvr::setDbParameter(int node, int param, const char * value,
			 BaseString& msg)
{

  Guard g(m_local_config_mutex);

  /**
   * Check parameter
   */
  ConfigIter iter(m_local_config, CFG_SECTION_NODE);
  if(iter.first() != 0){
    msg.assign("Unable to find node section (iter.first())");
    return -1;
  }

  Uint32 type = NODE_TYPE_DB + 1;
  if(node != 0){
    // Set parameter only in the specified node
    if(iter.find(CFG_NODE_ID, node) != 0){
      msg.assign("Unable to find node (iter.find())");
      return -1;
    }
    if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0){
      msg.assign("Unable to get node type(iter.get(CFG_TYPE_OF_SECTION))");
      return -1;
    }
  } else {
    // Set parameter in all DB nodes
    do {
      if(iter.get(CFG_TYPE_OF_SECTION, &type) != 0){
	msg.assign("Unable to get node type(iter.get(CFG_TYPE_OF_SECTION))");
	return -1;
      }
      if(type == NODE_TYPE_DB)
	break;
    } while(iter.next() == 0);
  }

  if(type != NODE_TYPE_DB){
    msg.assfmt("Invalid node type or no such node (%d %d)", 
	       type, NODE_TYPE_DB);
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
      val_64 = my_strtoll(value, 0, 10);
      break;
    }
    p_type++;
    if(iter.get(param, &val_char) == 0){
      val_char = value;
      break;
    }
    msg.assign("Could not get parameter");
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

    ConfigValues::Iterator i2(m_local_config->m_configValues->m_config, 
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
  return 0;
}


int
MgmtSrvr::setConnectionDbParameter(int node1, int node2,
                                   int param, int value,
                                   BaseString& msg)
{
  DBUG_ENTER("MgmtSrvr::setConnectionDbParameter");
  DBUG_PRINT("enter", ("node1: %d, node2: %d, param: %d, value: %d",
                       node1, node2, param, value));

  // This function only supports setting dynamic ports
  if (param != CFG_CONNECTION_SERVER_PORT)
  {
    msg.assign("Only param CFG_CONNECTION_SERVER_PORT can be set");
    DBUG_RETURN(-1);
  }

  if (!m_config_manager->set_dynamic_port(node1, node2, value, msg))
    DBUG_RETURN(-1);

  DBUG_PRINT("exit", ("Set parameter(%d) to %d for %d -> %d",
                      param, value, node1, node2));
  DBUG_RETURN(1);
}


bool MgmtSrvr::setDynamicPorts(int node, DynPortSpec ports[],
                               unsigned num_ports, BaseString& msg)
{
  return m_config_manager->set_dynamic_ports(node, ports, num_ports, msg);
}


int
MgmtSrvr::getConnectionDbParameter(int node1, int node2,
                                   int param, int *value,
                                   BaseString& msg)
{
  DBUG_ENTER("MgmtSrvr::getConnectionDbParameter");
  DBUG_PRINT("enter", ("node1: %d, node2: %d, param: %d",
                       node1, node2, param));

  // This function only supports asking about dynamic ports
  if (param != CFG_CONNECTION_SERVER_PORT)
  {
    msg.assign("Only param CFG_CONNECTION_SERVER_PORT can be retrieved");
    DBUG_RETURN(-1);
  }

  if (!m_config_manager->get_dynamic_port(node1, node2, value, msg))
    DBUG_RETURN(-1);

  DBUG_PRINT("exit", ("Return parameter(%d): %u for %d -> %d, msg: %s",
                      param, *value, node1, node2, msg.c_str()));
  DBUG_RETURN(1);
}


bool
MgmtSrvr::transporter_connect(NDB_SOCKET_TYPE sockfd,
                              BaseString& msg,
                              bool& close_with_reset)
{
  DBUG_ENTER("MgmtSrvr::transporter_connect");
  TransporterRegistry* tr= theFacade->get_registry();
  if (!tr->connect_server(sockfd, msg, close_with_reset))
    DBUG_RETURN(false);

  /**
   * TransporterRegistry::update_connections() is responsible
   * for doing the final step of bringing the connection into 
   * CONNECTED state when it detects it 'isConnected()'.
   * This is required due to all such state changes has to 
   * be synchroniced with ::performReceive().
   * To speed up CONNECTED detection, we request it to 
   * happen ASAP. (There is no guarantee when it happen though)
   */
  theFacade->request_connection_check();
  DBUG_RETURN(true);
}


bool MgmtSrvr::connect_to_self()
{
  BaseString buf;
  NdbMgmHandle mgm_handle= ndb_mgm_create_handle();

  buf.assfmt("%s:%u",
             m_opts.bind_address ? m_opts.bind_address : "localhost",
             m_port);
  ndb_mgm_set_connectstring(mgm_handle, buf.c_str());

  if(ndb_mgm_connect(mgm_handle, 0, 0, 0) < 0)
  {
    g_eventLogger->warning("%d %s",
                           ndb_mgm_get_latest_error(mgm_handle),
                           ndb_mgm_get_latest_error_desc(mgm_handle));
    ndb_mgm_destroy_handle(&mgm_handle);
    return false;
  }
  // TransporterRegistry now owns the handle and will destroy it.
  theFacade->get_registry()->set_mgm_handle(mgm_handle);

  return true;
}


bool
MgmtSrvr::change_config(Config& new_config, BaseString& msg)
{
  SignalSender ss(theFacade);
  ss.lock();

  SimpleSignal ssig;
  UtilBuffer buf;
  new_config.pack(buf);
  ssig.ptr[0].p = (Uint32*)buf.get_data();
  ssig.ptr[0].sz = (buf.length() + 3) / 4;
  ssig.header.m_noOfSections = 1;

  ConfigChangeReq *req= CAST_PTR(ConfigChangeReq, ssig.getDataPtrSend());
  req->length = buf.length();

  NodeBitmask mgm_nodes;
  {
    Guard g(m_local_config_mutex);
    m_local_config->get_nodemask(mgm_nodes, NDB_MGM_NODE_TYPE_MGM);
  }

  NodeId nodeId= ss.find_confirmed_node(mgm_nodes);
  if (nodeId == 0)
  {
    msg = "INTERNAL ERROR Could not find any mgmd!";
    return false;
  }

  if (ss.sendFragmentedSignal(nodeId, ssig,
                              MGM_CONFIG_MAN, GSN_CONFIG_CHANGE_REQ,
                              ConfigChangeReq::SignalLength) != 0)
  {
    msg.assfmt("Could not start configuration change, send to "
               "node %d failed", nodeId);
    return false;
  }
  mgm_nodes.clear(nodeId);

  bool done = false;
  while(!done)
  {
    SimpleSignal *signal= ss.waitFor();

    switch(signal->readSignalNumber()){
    case GSN_CONFIG_CHANGE_CONF:
      done= true;
      break;
    case GSN_CONFIG_CHANGE_REF:
    {
      const ConfigChangeRef * const ref =
        CAST_CONSTPTR(ConfigChangeRef, signal->getDataPtr());
      g_eventLogger->debug("Got CONFIG_CHANGE_REF, error: %d", ref->errorCode);
      switch(ref->errorCode)
      {
      case ConfigChangeRef::NotMaster:{
        // Retry with next node if any
        NodeId nodeId= ss.find_confirmed_node(mgm_nodes);
        if (nodeId == 0)
        {
          msg = "INTERNAL ERROR Could not find any mgmd!";
          return false;
        }

        if (ss.sendFragmentedSignal(nodeId, ssig,
                                    MGM_CONFIG_MAN, GSN_CONFIG_CHANGE_REQ,
                                    ConfigChangeReq::SignalLength) != 0)
        {
          msg.assfmt("Could not start configuration change, send to "
                     "node %d failed", nodeId);
          return false;
        }
        mgm_nodes.clear(nodeId);
        break;
      }

      default:
        msg = ConfigChangeRef::errorMessage(ref->errorCode);
        return false;
      }

      break;
    }

    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      // Ignore;
      break;


    case GSN_NODE_FAILREP:
      // ignore, NF_COMPLETEREP will come
      break;

    case GSN_NF_COMPLETEREP:
    {
      NodeId nodeId = refToNode(signal->header.theSendersBlockRef);
      msg.assign("Node %d failed during configuration change", nodeId);
      return false;
      break;
    }

    default:
      report_unknown_signal(signal);
      return false;

    }
  }

  g_eventLogger->info("Config change completed");

  return true;
}


void
MgmtSrvr::print_config(const char* section_filter, NodeId nodeid_filter,
                       const char* param_filter,
                       NdbOut& out)
{
  Guard g(m_local_config_mutex);
  m_local_config->print(section_filter, nodeid_filter,
                        param_filter, out);
}


bool
MgmtSrvr::reload_config(const char* config_filename, bool mycnf,
                        BaseString& msg)
{
  if (config_filename && mycnf)
  {
    msg = "ERROR: Both mycnf and config_filename is not supported";
    return false;
  }

  if (config_filename)
  {
    if (m_opts.mycnf)
    {
      msg.assfmt("ERROR: Can't switch to use config.ini '%s' when "
                 "node was started from my.cnf", config_filename);
      return false;
    }
  }
  else
  {
    if (mycnf)
    {
      // Reload from my.cnf
      if (!m_opts.mycnf)
      {
        if (m_opts.config_filename)
        {
          msg.assfmt("ERROR: Can't switch to use my.cnf when "
                     "node was started from '%s'", m_opts.config_filename);
          return false;
        }
      }
    }
    else
    {
      /* No config file name supplied and not told to use mycnf */
      if (m_opts.config_filename)
      {
        g_eventLogger->info("No config file name supplied, using '%s'",
                            m_opts.config_filename);
        config_filename = m_opts.config_filename;
      }
      else
      {
        msg = "ERROR: Neither config file name or mycnf available";
        return false;
      }
    }
  }

  Config* new_conf_ptr;
  if ((new_conf_ptr= ConfigManager::load_config(config_filename,
                                                mycnf, msg)) == NULL)
    return false;
  Config new_conf(new_conf_ptr);

  {
    Guard g(m_local_config_mutex);

    /* Copy the necessary values from old to new config */
    if (!new_conf.setGeneration(m_local_config->getGeneration()) ||
        !new_conf.setName(m_local_config->getName()) ||
        !new_conf.setPrimaryMgmNode(m_local_config->getPrimaryMgmNode()))
    {
      msg = "Failed to initialize reloaded config";
      return false;
    }
  }

  if (!change_config(new_conf, msg))
    return false;
  return true;
}

void
MgmtSrvr::show_variables(NdbOut& out)
{
  out << "daemon: " << yes_no(m_opts.daemon) << endl;
  out << "non_interactive: " << yes_no(m_opts.non_interactive) << endl;
  out << "interactive: " << yes_no(m_opts.interactive) << endl;
  out << "config_filename: " << str_null(m_opts.config_filename) << endl;
  out << "mycnf: " << yes_no(m_opts.mycnf) << endl;
  out << "bind_address: " << str_null(m_opts.bind_address) << endl;
  out << "no_nodeid_checks: " << yes_no(m_opts.no_nodeid_checks) << endl;
  out << "print_full_config: " << yes_no(m_opts.print_full_config) << endl;
  out << "configdir: " << str_null(m_opts.configdir) << endl;
  out << "config_cache: " << yes_no(m_opts.config_cache) << endl;
  out << "verbose: " << yes_no(m_opts.verbose) << endl;
  out << "reload: " << yes_no(m_opts.reload) << endl;

  out << "nodeid: " << _ownNodeId << endl;
  out << "blocknumber: " << hex <<_blockNumber << endl;
  out << "own_reference: " << hex << _ownReference << endl;
  out << "port: " << m_port << endl;
  out << "need_restart: " << m_need_restart << endl;
  out << "is_stop_thread: " << _isStopThread << endl;
  out << "log_level_thread_sleep: " << _logLevelThreadSleep << endl;
  out << "master_node: " << m_master_node << endl;
}

void
MgmtSrvr::make_sync_req(SignalSender& ss, Uint32 nodeId)
{
  const trp_node node = ss.getNodeInfo(nodeId);
  if (!ndbd_sync_req_support(node.m_info.m_version))
  {
    /* The node hasn't got SYNC_REQ support */
    return;
  }

  /**
   * This subroutine is used to make a async request(error insert/dump)
   *   "more" syncronous, i.e increasing the likelyhood that
   *   the async request has really reached the destination
   *   before returning to the api
   *
   * I.e it's a work-around...
   *
   */
  SimpleSignal ssig;
  SyncReq* req = CAST_PTR(SyncReq, ssig.getDataPtrSend());
  req->senderRef = ss.getOwnRef();
  req->senderData = 12;
  req->prio = 1; // prio b
  ssig.set(ss,TestOrd::TraceAPI, CMVMI, GSN_SYNC_REQ, SyncReq::SignalLength);
  
  if (ss.sendSignal(nodeId, &ssig) != SEND_OK)
  {
    return;
  }

  while (true)
  {
    SimpleSignal *signal = ss.waitFor();
    
    int gsn = signal->readSignalNumber();
    switch (gsn) {
    case GSN_SYNC_REF:
    case GSN_SYNC_CONF:
      return;
      
    case GSN_NF_COMPLETEREP:{
      const NFCompleteRep * const rep =
        CAST_CONSTPTR(NFCompleteRep, signal->getDataPtr());
      if (rep->failedNodeId == nodeId)
        return;
      break;
    }
      
    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
	CAST_CONSTPTR(NodeFailRep, signal->getDataPtr());
      Uint32 len = NodeFailRep::getNodeMaskLength(signal->getLength());
      assert(len == NodeBitmask::Size); // only full length in ndbapi
      if (BitmaskImpl::safe_get(len, rep->theAllNodes,nodeId))
	return;
      break;
    }
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      break;
    default:
      return;
    }
  }
}


bool
MgmtSrvr::request_events(NdbNodeBitmask nodes, Uint32 reports_per_node,
                         Uint32 dump_type,
                         Vector<SimpleSignal>& events)
{
  int nodes_counter[MAX_NDB_NODES];
#ifndef NDEBUG
  NdbNodeBitmask save = nodes;
#endif
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
    const trp_node node = ss.getNodeInfo(i);
    if (node.m_info.getType() != NodeInfo::DB ||
        !node.is_confirmed())
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
      nodes_counter[i] = (int)reports_per_node;
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

      if (!nodes.get(nodeid))
      {
        // The reporting node was not expected
#ifndef NDEBUG
        ndbout_c("nodeid: %u", nodeid);
        ndbout_c("save: %s", BaseString::getPrettyText(save).c_str());
#endif
        assert(false);
        return false;
      }

      if (event->getEventType() == NDB_LE_SavedEvent &&
          signal->getDataPtr()[1] == 0)
      {
        nodes_counter[nodeid] = 1;
      }
      else
      {
        // Save signal
        events.push_back(SimpleSignal(*signal));
      }

      // Check if node is done
      nodes_counter[nodeid]--;
      if (nodes_counter[nodeid] == 0)
        nodes.clear(nodeid);

      break;
    }

    case GSN_NODE_FAILREP:{
      const NodeFailRep * const rep =
        (const NodeFailRep*)signal->getDataPtr();
      // only care about data-nodes
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
template class Vector< Vector<BaseString> >;
template class Vector<MgmtSrvr::PossibleNode>;
template class Vector<Defragger::DefragBuffer*>;
