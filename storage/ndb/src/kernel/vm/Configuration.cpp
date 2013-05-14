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
#include <ndb_opts.h>

#include "Configuration.hpp"
#include <ErrorHandlingMacros.hpp>
#include "GlobalData.hpp"

#include <ConfigRetriever.hpp>
#include <IPCConfig.hpp>
#include <ndb_version.h>
#include <NdbMem.h>
#include <NdbOut.hpp>
#include <WatchDog.hpp>

#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters_debug.h>
#include <kernel_config_parameters.h>

#include <kernel_types.h>
#include <ndb_limits.h>
#include <ndbapi_limits.h>
#include "pc.hpp"
#include <LogLevel.hpp>
#include <NdbSleep.h>
#include <NdbThread.h>

extern "C" {
  void ndbSetOwnVersion();
}

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

enum ndbd_options {
  OPT_INITIAL = NDB_STD_OPTIONS_LAST,
  OPT_NODAEMON,
  OPT_FOREGROUND,
  OPT_NOWAIT_NODES,
  OPT_INITIAL_START
};

NDB_STD_OPTS_VARS;
// XXX should be my_bool ???
static int _daemon, _no_daemon, _foreground,  _initial, _no_start;
static int _initialstart;
static const char* _nowait_nodes = 0;
static const char* _bind_address = 0;

extern Uint32 g_start_type;
extern NdbNodeBitmask g_nowait_nodes;

const char *load_default_groups[]= { "mysql_cluster","ndbd",0 };

/**
 * Arguments to NDB process
 */ 
static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndbd"),
  { "initial", OPT_INITIAL,
    "Perform initial start of ndbd, including cleaning the file system. "
    "Consult documentation before using this",
    (uchar**) &_initial, (uchar**) &_initial, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "nostart", 'n',
    "Don't start ndbd immediately. Ndbd will await command from ndb_mgmd",
    (uchar**) &_no_start, (uchar**) &_no_start, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "daemon", 'd', "Start ndbd as daemon (default)",
    (uchar**) &_daemon, (uchar**) &_daemon, 0,
    GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0 },
  { "nodaemon", OPT_NODAEMON,
    "Do not start ndbd as daemon, provided for testing purposes",
    (uchar**) &_no_daemon, (uchar**) &_no_daemon, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "foreground", OPT_FOREGROUND,
    "Run real ndbd in foreground, provided for debugging purposes"
    " (implies --nodaemon)",
    (uchar**) &_foreground, (uchar**) &_foreground, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "nowait-nodes", OPT_NOWAIT_NODES, 
    "Nodes that will not be waited for during start",
    (uchar**) &_nowait_nodes, (uchar**) &_nowait_nodes, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "initial-start", OPT_INITIAL_START, 
    "Perform a partial initial start of the cluster.  "
    "Each node should be started with this option, as well as --nowait-nodes",
    (uchar**) &_initialstart, (uchar**) &_initialstart, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "bind-address", OPT_NOWAIT_NODES, 
    "Local bind address",
    (uchar**) &_bind_address, (uchar**) &_bind_address, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void short_usage_sub(void)
{
  printf("Usage: %s [OPTIONS]\n", my_progname);
}
static void usage()
{
  short_usage_sub();
  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

bool
Configuration::init(int argc, char** argv)
{  
  load_defaults("my",load_default_groups,&argc,&argv);

  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndbd.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options,
			       ndb_std_get_one_option)))
    exit(ho_error);

  if (_no_daemon || _foreground) {
    _daemon= 0;
  }

  DBUG_PRINT("info", ("no_start=%d", _no_start));
  DBUG_PRINT("info", ("initial=%d", _initial));
  DBUG_PRINT("info", ("daemon=%d", _daemon));
  DBUG_PRINT("info", ("foreground=%d", _foreground));
  DBUG_PRINT("info", ("connect_str=%s", opt_connect_str));

  ndbSetOwnVersion();

  // Check the start flag
  if (_no_start)
    globalData.theRestartFlag = initial_state;
  else 
    globalData.theRestartFlag = perform_start;

  // Check the initial flag
  if (_initial)
    _initialStart = true;
  
  // Check connectstring
  if (opt_connect_str)
    _connectString = strdup(opt_connect_str);
  
  // Check daemon flag
  if (_daemon)
    _daemonMode = true;
  if (_foreground)
    _foregroundMode = true;

  // Save programname
  if(argc > 0 && argv[0] != 0)
    _programName = strdup(argv[0]);
  else
    _programName = strdup("");
  
  globalData.ownId= 0;

  if (_nowait_nodes)
  {
    int res = g_nowait_nodes.parseMask(_nowait_nodes);
    if(res == -2 || (res > 0 && g_nowait_nodes.get(0)))
    {
      ndbout_c("Invalid nodeid specified in nowait-nodes: %s", 
               _nowait_nodes);
      exit(-1);
    }
    else if (res < 0)
    {
      ndbout_c("Unable to parse nowait-nodes argument: %s",
               _nowait_nodes);
      exit(-1);
    }
  }

  if (_initialstart)
  {
    _initialStart = true;
    g_start_type |= (1 << NodeState::ST_INITIAL_START);
  }

  threadIdMutex = NdbMutex_Create();
  if (!threadIdMutex)
  {
    ndbout_c("Failed to create threadIdMutex");
    exit(-1);
  }
  initThreadArray();
  return true;
}

Configuration::Configuration()
{
  _programName = 0;
  _connectString = 0;
  _fsPath = 0;
  _backupPath = 0;
  _initialStart = false;
  _daemonMode = false;
  _foregroundMode = false;
  m_config_retriever= 0;
  m_clusterConfig= 0;
  m_clusterConfigIter= 0;
  m_logLevel= 0;
}

Configuration::~Configuration(){
  if (opt_connect_str)
    free(_connectString);

  if(_programName != NULL)
    free(_programName);

  if(_fsPath != NULL)
    free(_fsPath);

  if(_backupPath != NULL)
    free(_backupPath);

  if (m_config_retriever) {
    delete m_config_retriever;
  }

  if(m_logLevel) {
    delete m_logLevel;
  }
}

void
Configuration::closeConfiguration(bool end_session){
  m_config_retriever->end_session(end_session);
  if (m_config_retriever) {
    delete m_config_retriever;
  }
  m_config_retriever= 0;
}

void
Configuration::fetch_configuration(){
  /**
   * Fetch configuration from management server
   */
  if (m_config_retriever) {
    delete m_config_retriever;
  }

  m_mgmd_port= 0;
  m_config_retriever= new ConfigRetriever(getConnectString(),
					  NDB_VERSION, 
					  NODE_TYPE_DB,
					  _bind_address);

  if (m_config_retriever->hasError())
  {
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG,
	      "Could not connect initialize handle to management server",
	      m_config_retriever->getErrorString());
  }

  if(m_config_retriever->do_connect(12,5,1) == -1){
    const char * s = m_config_retriever->getErrorString();
    if(s == 0)
      s = "No error given!";
    /* Set stop on error to true otherwise NDB will
       go into an restart loop...
    */
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Could not connect to ndb_mgmd", s);
  }
  
  m_mgmd_port= m_config_retriever->get_mgmd_port();
  m_mgmd_host.assign(m_config_retriever->get_mgmd_host());

  ConfigRetriever &cr= *m_config_retriever;
  
  /**
   * if we have a nodeid set (e.g in a restart situation)
   * reuse it
   */
  if (globalData.ownId)
    cr.setNodeId(globalData.ownId);

  globalData.ownId = cr.allocNodeId(globalData.ownId ? 10 : 2 /*retry*/,
                                    3 /*delay*/);
  
  if(globalData.ownId == 0){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, 
	      "Unable to alloc node id", m_config_retriever->getErrorString());
  }
  
  ndb_mgm_configuration * p = cr.getConfig();
  if(p == 0){
    const char * s = cr.getErrorString();
    if(s == 0)
      s = "No error given!";
    
    /* Set stop on error to true otherwise NDB will
       go into an restart loop...
    */
    
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Could not fetch configuration"
	      "/invalid configuration", s);
  }
  if(m_clusterConfig)
    free(m_clusterConfig);
  
  m_clusterConfig = p;
  
  ndb_mgm_configuration_iterator iter(* p, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, globalData.ownId)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", "DB missing");
  }
  
  if(iter.get(CFG_DB_STOP_ON_ERROR, &_stopOnError)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", 
	      "StopOnError missing");
  }

  /* Retrieve StopOnError start failure handling params if there are
   * any.  Older MGMDs may not have them, so we use defaults.
   */
  _maxStartFailRetries = 3;
  _startFailDelaySecs = 0;
  ndb_mgm_get_int_parameter(&iter,
                            CFG_DB_MAX_START_FAIL, &_maxStartFailRetries);
  ndb_mgm_get_int_parameter(&iter,
                            CFG_DB_START_FAIL_DELAY_SECS, &_startFailDelaySecs);
  
  m_mgmds.clear();
  for(ndb_mgm_first(&iter); ndb_mgm_valid(&iter); ndb_mgm_next(&iter))
  {
    Uint32 nodeType, port;
    char const *hostname;

    ndb_mgm_get_int_parameter(&iter,CFG_TYPE_OF_SECTION,&nodeType);

    if (nodeType != NodeInfo::MGM)
      continue;

    if (ndb_mgm_get_string_parameter(&iter,CFG_NODE_HOST, &hostname) ||
	ndb_mgm_get_int_parameter(&iter,CFG_MGM_PORT, &port) ||
	hostname == 0 || hostname[0] == 0)
    {
      continue;
    }
    BaseString connectstring(hostname);
    connectstring.appfmt(":%d", port);

    m_mgmds.push_back(connectstring);
  }
}

static char * get_and_validate_path(ndb_mgm_configuration_iterator &iter,
				    Uint32 param, const char *param_string)
{ 
  const char* path = NULL;
  if(iter.get(param, &path)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched missing ", 
	      param_string);
  } 
  
  if(path == 0 || strlen(path) == 0){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG,
	      "Invalid configuration fetched. Configuration does not contain valid ",
	      param_string);
  }
  
  // check that it is pointing on a valid directory
  // 
  char buf2[PATH_MAX];
  memset(buf2, 0,sizeof(buf2));
#ifdef NDB_WIN32
  char* szFilePart;
  if(!GetFullPathName(path, sizeof(buf2), buf2, &szFilePart) ||
     (GetFileAttributes(buf2) & FILE_ATTRIBUTE_READONLY))
#else
  if((::realpath(path, buf2) == NULL)||
       (::access(buf2, W_OK) != 0))
#endif
  {
    ERROR_SET(fatal, NDBD_EXIT_AFS_INVALIDPATH, path, param_string);
  }
  
  if (strcmp(&buf2[strlen(buf2) - 1], DIR_SEPARATOR))
    strcat(buf2, DIR_SEPARATOR);
  
  return strdup(buf2);
}

void
Configuration::setupConfiguration(){

  DBUG_ENTER("Configuration::setupConfiguration");

  ndb_mgm_configuration * p = m_clusterConfig;

  /**
   * Configure transporters
   */
  {  
    int res = IPCConfig::configureTransporters(globalData.ownId,
					       * p, 
					       globalTransporterRegistry);
    if(res <= 0){
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", 
		"No transporters configured");
    }
  }

  /**
   * Setup cluster configuration data
   */
  ndb_mgm_configuration_iterator iter(* p, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, globalData.ownId)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", "DB missing");
  }

  unsigned type;
  if(!(iter.get(CFG_TYPE_OF_SECTION, &type) == 0 && type == NODE_TYPE_DB)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched",
	      "I'm wrong type of node");
  }
  
  if(iter.get(CFG_DB_NO_SAVE_MSGS, &_maxErrorLogs)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", 
	      "MaxNoOfSavedMessages missing");
  }
  
  if(iter.get(CFG_DB_MEMLOCK, &_lockPagesInMainMemory)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", 
	      "LockPagesInMainMemory missing");
  }

  if(iter.get(CFG_DB_WATCHDOG_INTERVAL, &_timeBetweenWatchDogCheck)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", 
	      "TimeBetweenWatchDogCheck missing");
  }

  _schedulerExecutionTimer = 50;
  iter.get(CFG_DB_SCHED_EXEC_TIME, &_schedulerExecutionTimer);

  _schedulerSpinTimer = 0;
  iter.get(CFG_DB_SCHED_SPIN_TIME, &_schedulerSpinTimer);

  _realtimeScheduler = 0;
  iter.get(CFG_DB_REALTIME_SCHEDULER, &_realtimeScheduler);

  _executeLockCPU = 65535;
  iter.get(CFG_DB_EXECUTE_LOCK_CPU, &_executeLockCPU);

  _maintLockCPU = 65535;
  iter.get(CFG_DB_MAINT_LOCK_CPU, &_maintLockCPU);

  if(iter.get(CFG_DB_WATCHDOG_INTERVAL_INITIAL, 
              &_timeBetweenWatchDogCheckInitial)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", 
	      "TimeBetweenWatchDogCheckInitial missing");
  }
  
  /**
   * Get paths
   */  
  if (_fsPath)
    free(_fsPath);
  _fsPath= get_and_validate_path(iter, CFG_DB_FILESYSTEM_PATH, "FileSystemPath");
  if (_backupPath)
    free(_backupPath);
  _backupPath= get_and_validate_path(iter, CFG_DB_BACKUP_DATADIR, "BackupDataDir");

  if(iter.get(CFG_DB_STOP_ON_ERROR_INSERT, &m_restartOnErrorInsert)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", 
	      "RestartOnErrorInsert missing");
  }
  
  /**
   * Create the watch dog thread
   */
  { 
    if (_timeBetweenWatchDogCheckInitial < _timeBetweenWatchDogCheck)
      _timeBetweenWatchDogCheckInitial = _timeBetweenWatchDogCheck;
    
    Uint32 t = _timeBetweenWatchDogCheckInitial;
    t = globalEmulatorData.theWatchDog ->setCheckInterval(t);
    _timeBetweenWatchDogCheckInitial = t;
  }
  
  ConfigValues* cf = ConfigValuesFactory::extractCurrentSection(iter.m_config);

  if(m_clusterConfigIter)
    ndb_mgm_destroy_iterator(m_clusterConfigIter);
  m_clusterConfigIter = ndb_mgm_create_configuration_iterator
    (p, CFG_SECTION_NODE);

  calcSizeAlt(cf);

  DBUG_VOID_RETURN;
}

Uint32
Configuration::lockPagesInMainMemory() const {
  return _lockPagesInMainMemory;
}

int 
Configuration::schedulerExecutionTimer() const {
  return _schedulerExecutionTimer;
}

void 
Configuration::schedulerExecutionTimer(int value) {
  if (value < 11000)
    _schedulerExecutionTimer = value;
}

int 
Configuration::schedulerSpinTimer() const {
  return _schedulerSpinTimer;
}

void 
Configuration::schedulerSpinTimer(int value) {
  if (value < 500)
    value = 500;
  _schedulerSpinTimer = value;
}

bool 
Configuration::realtimeScheduler() const
{
  return (bool)_realtimeScheduler;
}

void 
Configuration::realtimeScheduler(bool realtime_on)
{
   bool old_value = (bool)_realtimeScheduler;
  _realtimeScheduler = (Uint32)realtime_on;
  if (old_value != realtime_on)
    setAllRealtimeScheduler();
}

Uint32
Configuration::executeLockCPU() const
{
  return _executeLockCPU;
}

void
Configuration::executeLockCPU(Uint32 value)
{
  Uint32 old_value = _executeLockCPU;
  _executeLockCPU = value;
  if (value != old_value)
    setAllLockCPU(TRUE);
}

Uint32
Configuration::maintLockCPU() const
{
  return _maintLockCPU;
}

void
Configuration::maintLockCPU(Uint32 value)
{
  Uint32 old_value = _maintLockCPU;
  _maintLockCPU = value;
  if (value != old_value)
    setAllLockCPU(FALSE);
}

int 
Configuration::timeBetweenWatchDogCheck() const {
  return _timeBetweenWatchDogCheck;
}

void 
Configuration::timeBetweenWatchDogCheck(int value) {
  _timeBetweenWatchDogCheck = value;
}

int 
Configuration::maxNoOfErrorLogs() const {
  return _maxErrorLogs;
}

void 
Configuration::maxNoOfErrorLogs(int val){
  _maxErrorLogs = val;
}

bool
Configuration::stopOnError() const {
  return _stopOnError;
}

void 
Configuration::stopOnError(bool val){
  _stopOnError = val;
}

int
Configuration::getRestartOnErrorInsert() const {
  return m_restartOnErrorInsert;
}

void
Configuration::setRestartOnErrorInsert(int i){
  m_restartOnErrorInsert = i;
}

const char *
Configuration::getConnectString() const {
  return _connectString;
}

char *
Configuration::getConnectStringCopy() const {
  if(_connectString != 0)
    return strdup(_connectString);
  return 0;
}

Uint32
Configuration::getMaxStartFailRetries() const 
{
  return _maxStartFailRetries;
}

Uint32
Configuration::getStartFailRetryDelaySecs() const
{
  return _startFailDelaySecs;
}

const ndb_mgm_configuration_iterator * 
Configuration::getOwnConfigIterator() const {
  return m_ownConfigIterator;
}
  
ndb_mgm_configuration_iterator * 
Configuration::getClusterConfigIterator() const {
  return m_clusterConfigIter;
}

void
Configuration::calcSizeAlt(ConfigValues * ownConfig){
  const char * msg = "Invalid configuration fetched";
  char buf[255];

  unsigned int noOfTables = 0;
  unsigned int noOfUniqueHashIndexes = 0;
  unsigned int noOfOrderedIndexes = 0;
  unsigned int noOfTriggers = 0;
  unsigned int noOfReplicas = 0;
  unsigned int noOfDBNodes = 0;
  unsigned int noOfAPINodes = 0;
  unsigned int noOfMGMNodes = 0;
  unsigned int noOfNodes = 0;
  unsigned int noOfAttributes = 0;
  unsigned int noOfOperations = 0;
  unsigned int noOfLocalOperations = 0;
  unsigned int noOfTransactions = 0;
  unsigned int noOfIndexPages = 0;
  unsigned int noOfDataPages = 0;
  unsigned int noOfScanRecords = 0;
  unsigned int noOfLocalScanRecords = 0;
  unsigned int noBatchSize = 0;
  m_logLevel = new LogLevel();
  
  struct AttribStorage { int paramId; Uint32 * storage; bool computable; };
  AttribStorage tmp[] = {
    { CFG_DB_NO_SCANS, &noOfScanRecords, false },
    { CFG_DB_NO_LOCAL_SCANS, &noOfLocalScanRecords, true },
    { CFG_DB_BATCH_SIZE, &noBatchSize, false },
    { CFG_DB_NO_TABLES, &noOfTables, false },
    { CFG_DB_NO_ORDERED_INDEXES, &noOfOrderedIndexes, false },
    { CFG_DB_NO_UNIQUE_HASH_INDEXES, &noOfUniqueHashIndexes, false },
    { CFG_DB_NO_TRIGGERS, &noOfTriggers, true },
    { CFG_DB_NO_REPLICAS, &noOfReplicas, false },
    { CFG_DB_NO_ATTRIBUTES, &noOfAttributes, false },
    { CFG_DB_NO_OPS, &noOfOperations, false },
    { CFG_DB_NO_LOCAL_OPS, &noOfLocalOperations, true },
    { CFG_DB_NO_TRANSACTIONS, &noOfTransactions, false }
  };

  ndb_mgm_configuration_iterator db(*(ndb_mgm_configuration*)ownConfig, 0);
  
  const int sz = sizeof(tmp)/sizeof(AttribStorage);
  for(int i = 0; i<sz; i++){
    if(ndb_mgm_get_int_parameter(&db, tmp[i].paramId, tmp[i].storage)){
      if (tmp[i].computable) {
        *tmp[i].storage = 0;
      } else {
        BaseString::snprintf(buf, sizeof(buf),"ConfigParam: %d not found", tmp[i].paramId);
        ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
      }
    }
  }

  Uint64 indexMem = 0, dataMem = 0;
  ndb_mgm_get_int64_parameter(&db, CFG_DB_DATA_MEM, &dataMem);
  ndb_mgm_get_int64_parameter(&db, CFG_DB_INDEX_MEM, &indexMem);
  if(dataMem == 0){
    BaseString::snprintf(buf, sizeof(buf), "ConfigParam: %d not found", CFG_DB_DATA_MEM);
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
  }

  if(indexMem == 0){
    BaseString::snprintf(buf, sizeof(buf), "ConfigParam: %d not found", CFG_DB_INDEX_MEM);
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
  }

  noOfDataPages = (dataMem / 32768);
  noOfIndexPages = (indexMem / 8192);

  for(unsigned j = 0; j<LogLevel::LOGLEVEL_CATEGORIES; j++){
    Uint32 tmp;
    if(!ndb_mgm_get_int_parameter(&db, CFG_MIN_LOGLEVEL+j, &tmp)){
      m_logLevel->setLogLevel((LogLevel::EventCategory)j, tmp);
    }
  }
  
  // tmp
  ndb_mgm_configuration_iterator * p = m_clusterConfigIter;

  Uint32 nodeNo = noOfNodes = 0;
  NodeBitmask nodes;
  for(ndb_mgm_first(p); ndb_mgm_valid(p); ndb_mgm_next(p), nodeNo++){
    
    Uint32 nodeId;
    Uint32 nodeType;
    
    if(ndb_mgm_get_int_parameter(p, CFG_NODE_ID, &nodeId)){
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, "Node data (Id) missing");
    }
    
    if(ndb_mgm_get_int_parameter(p, CFG_TYPE_OF_SECTION, &nodeType)){
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, "Node data (Type) missing");
    }
    
    if(nodeId > MAX_NODES || nodeId == 0){
      BaseString::snprintf(buf, sizeof(buf),
	       "Invalid node id: %d", nodeId);
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
    }
    
    if(nodes.get(nodeId)){
      BaseString::snprintf(buf, sizeof(buf), "Two node can not have the same node id: %d",
	       nodeId);
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
    }
    nodes.set(nodeId);
        
    switch(nodeType){
    case NODE_TYPE_DB:
      noOfDBNodes++; // No of NDB processes
      
      if(nodeId > MAX_NDB_NODES){
		  BaseString::snprintf(buf, sizeof(buf), "Maximum node id for a ndb node is: %d", 
		 MAX_NDB_NODES);
	ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
      }
      break;
    case NODE_TYPE_API:
      noOfAPINodes++; // No of API processes
      break;
    case NODE_TYPE_MGM:
      noOfMGMNodes++; // No of MGM processes
      break;
    default:
      BaseString::snprintf(buf, sizeof(buf), "Unknown node type: %d", nodeType);
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
    }
  }
  noOfNodes = nodeNo;

  noOfTables+= 2; // Add System tables
  noOfAttributes += 9;  // Add System table attributes

  ConfigValues::Iterator it2(*ownConfig, db.m_config);
  it2.set(CFG_DB_NO_TABLES, noOfTables);
  it2.set(CFG_DB_NO_ATTRIBUTES, noOfAttributes);
  {
    Uint32 neededNoOfTriggers =   /* types: Insert/Update/Delete/Custom */
      3 * noOfUniqueHashIndexes + /* for unique hash indexes, I/U/D */
      3 * NDB_MAX_ACTIVE_EVENTS + /* for events in suma, I/U/D */
      3 * noOfTables +            /* for backup, I/U/D */
      noOfOrderedIndexes;         /* for ordered indexes, C */
    if (noOfTriggers < neededNoOfTriggers)
    {
      noOfTriggers= neededNoOfTriggers;
      it2.set(CFG_DB_NO_TRIGGERS, noOfTriggers);
    }
  }

  /**
   * Do size calculations
   */
  ConfigValuesFactory cfg(ownConfig);

  Uint32 noOfMetaTables= noOfTables + noOfOrderedIndexes +
                           noOfUniqueHashIndexes;
  Uint32 noOfMetaTablesDict= noOfMetaTables;
  if (noOfMetaTablesDict > MAX_TABLES)
    noOfMetaTablesDict= MAX_TABLES;

  {
    /**
     * Dict Size Alt values
     */
    cfg.put(CFG_DICT_ATTRIBUTE, 
	    noOfAttributes);

    cfg.put(CFG_DICT_TABLE,
	    noOfMetaTablesDict);
  }


  if (noOfLocalScanRecords == 0) {
    noOfLocalScanRecords = (noOfDBNodes * noOfScanRecords) + 
      1 /* NR */ + 
      1 /* LCP */; 
  }
  if (noOfLocalOperations == 0) {
    noOfLocalOperations= (11 * noOfOperations) / 10;
  }
  Uint32 noOfTCScanRecords = noOfScanRecords;

  {
    Uint32 noOfAccTables= noOfMetaTables/*noOfTables+noOfUniqueHashIndexes*/;
    /**
     * Acc Size Alt values
     */
    // Can keep 65536 pages (= 0.5 GByte)
    cfg.put(CFG_ACC_DIR_RANGE, 
	    2 * NO_OF_FRAG_PER_NODE * noOfAccTables* noOfReplicas); 
    
    cfg.put(CFG_ACC_DIR_ARRAY,
	    (noOfIndexPages >> 8) + 
	    2 * NO_OF_FRAG_PER_NODE * noOfAccTables* noOfReplicas);
    
    cfg.put(CFG_ACC_FRAGMENT,
	    NO_OF_FRAG_PER_NODE * noOfAccTables* noOfReplicas);
    
    /*-----------------------------------------------------------------------*/
    // The extra operation records added are used by the scan and node 
    // recovery process. 
    // Node recovery process will have its operations dedicated to ensure
    // that they never have a problem with allocation of the operation record.
    // The remainder are allowed for use by the scan processes.
    /*-----------------------------------------------------------------------*/
    cfg.put(CFG_ACC_OP_RECS,
	    (noOfLocalOperations + 50) + 
	    (noOfLocalScanRecords * noBatchSize) +
	    NODE_RECOVERY_SCAN_OP_RECORDS);
    
    cfg.put(CFG_ACC_OVERFLOW_RECS,
	    noOfIndexPages + 
	    NO_OF_FRAG_PER_NODE * noOfAccTables* noOfReplicas);
    
    cfg.put(CFG_ACC_PAGE8, 
	    noOfIndexPages + 32);
    
    cfg.put(CFG_ACC_TABLE, noOfAccTables);
    
    cfg.put(CFG_ACC_SCAN, noOfLocalScanRecords);
  }
  
  {
    /**
     * Dih Size Alt values
     */
    cfg.put(CFG_DIH_API_CONNECT, 
	    2 * noOfTransactions);
    
    cfg.put(CFG_DIH_CONNECT, 
	    noOfOperations + noOfTransactions + 46);
    
    Uint32 noFragPerTable= ((noOfDBNodes + NO_OF_FRAGS_PER_CHUNK - 1) >>
                           LOG_NO_OF_FRAGS_PER_CHUNK) <<
                           LOG_NO_OF_FRAGS_PER_CHUNK;

    cfg.put(CFG_DIH_FRAG_CONNECT, 
	    noFragPerTable *  noOfMetaTables);
    
    int temp;
    temp = noOfReplicas - 2;
    if (temp < 0)
      temp = 1;
    else
      temp++;
    cfg.put(CFG_DIH_MORE_NODES, 
	    temp * NO_OF_FRAG_PER_NODE *
	    noOfMetaTables *  noOfDBNodes);

    cfg.put(CFG_DIH_REPLICAS, 
	    NO_OF_FRAG_PER_NODE * noOfMetaTables *
	    noOfDBNodes * noOfReplicas);

    cfg.put(CFG_DIH_TABLE, 
	    noOfMetaTables);
  }
  
  {
    /**
     * Lqh Size Alt values
     */
    cfg.put(CFG_LQH_FRAG, 
	    NO_OF_FRAG_PER_NODE * noOfMetaTables * noOfReplicas);
    
    cfg.put(CFG_LQH_TABLE, 
	    noOfMetaTables);

    cfg.put(CFG_LQH_TC_CONNECT, 
	    noOfLocalOperations + 50);
    
    cfg.put(CFG_LQH_SCAN, 
	    noOfLocalScanRecords);
  }
  
  {
    /**
     * Tc Size Alt values
     */
    cfg.put(CFG_TC_API_CONNECT, 
	    3 * noOfTransactions);
    
    cfg.put(CFG_TC_TC_CONNECT, 
	    (2 * noOfOperations) + 16 + noOfTransactions);
    
    cfg.put(CFG_TC_TABLE, 
	    noOfMetaTables);
    
    cfg.put(CFG_TC_LOCAL_SCAN, 
	    noOfLocalScanRecords);
    
    cfg.put(CFG_TC_SCAN, 
	    noOfTCScanRecords);
  }
  
  {
    /**
     * Tup Size Alt values
     */
    cfg.put(CFG_TUP_FRAG, 
	    NO_OF_FRAG_PER_NODE * noOfMetaTables* noOfReplicas);
    
    cfg.put(CFG_TUP_OP_RECS, 
	    noOfLocalOperations + 50);
    
    cfg.put(CFG_TUP_PAGE, 
	    noOfDataPages);
    
    cfg.put(CFG_TUP_PAGE_RANGE, 
	    2 * NO_OF_FRAG_PER_NODE * noOfMetaTables* noOfReplicas);
    
    cfg.put(CFG_TUP_TABLE, 
	    noOfMetaTables);
    
    cfg.put(CFG_TUP_TABLE_DESC, 
	    6 * NO_OF_FRAG_PER_NODE * noOfAttributes * noOfReplicas +
	    10 * NO_OF_FRAG_PER_NODE * noOfMetaTables * noOfReplicas );
    
    cfg.put(CFG_TUP_STORED_PROC,
	    noOfLocalScanRecords);
  }

  {
    /**
     * Tux Size Alt values
     */
    cfg.put(CFG_TUX_INDEX, 
	    noOfMetaTables /*noOfOrderedIndexes*/);
    
    cfg.put(CFG_TUX_FRAGMENT,
	    NO_OF_FRAG_PER_NODE * noOfOrderedIndexes * noOfReplicas);
    
    cfg.put(CFG_TUX_ATTRIBUTE, 
	    noOfOrderedIndexes * 4);

    cfg.put(CFG_TUX_SCAN_OP, noOfLocalScanRecords); 
  }

  m_ownConfig = (ndb_mgm_configuration*)cfg.getConfigValues();
  m_ownConfigIterator = ndb_mgm_create_configuration_iterator
    (m_ownConfig, 0);
}

void
Configuration::setInitialStart(bool val){
  _initialStart = val;
}

void
Configuration::setAllRealtimeScheduler()
{
  Uint32 i;
  for (i = 0; i < threadInfo.size(); i++)
  {
    if (threadInfo[i].type != NotInUse)
    {
      if (setRealtimeScheduler(threadInfo[i].threadHandle,
                               threadInfo[i].type,
                               _realtimeScheduler,
                               FALSE))
        return;
    }
  }
}

void
Configuration::setAllLockCPU(bool exec_thread)
{
  Uint32 i;
  for (i = 0; i < threadInfo.size(); i++)
  {
    if (threadInfo[i].type != NotInUse)
    {
      if (setLockCPU(threadInfo[i].threadId,
                     threadInfo[i].type,
                     exec_thread,
                     FALSE))
        return;
    }
  }
}

int
Configuration::setRealtimeScheduler(NDB_THAND_TYPE threadHandle,
                                    enum ThreadTypes type,
                                    bool real_time,
                                    bool init)
{
  /*
    We ignore thread characteristics on platforms where we cannot
    determine the thread id.
  */
  if (!threadHandle)
    return 0; 
  if (!init || real_time)
  {
    int error_no;
    if ((error_no = NdbThread_SetScheduler(threadHandle, real_time,
                                           (type != MainThread))))
    {
      //Warning, no permission to set scheduler
      return 1;
    }
  }
  return 0;
}

int
Configuration::setLockCPU(NDB_TID_TYPE threadId,
                          enum ThreadTypes type,
                          bool exec_thread,
                          bool init)
{
  Uint32 cpu_id;
  /*
    We ignore thread characteristics on platforms where we cannot
    determine the thread id.
    We only set new lock CPU characteristics for the threads for which
    it has changed
  */
  if (!threadId)
    return 0;
  if ((exec_thread && type != MainThread) ||
      (!exec_thread && type == MainThread))
    return 0;
  if (type == MainThread)
    cpu_id = _executeLockCPU;
  else
    cpu_id = _maintLockCPU;
  if (!init ||
      cpu_id != NO_LOCK_CPU)
  {
    int error_no;
    ndbout << "Lock threadId = " << (unsigned)threadId;
    ndbout << " to CPU id = " << cpu_id << endl;
    if ((error_no = NdbThread_LockCPU(threadId, cpu_id)))
    {
      ndbout << "Failed to lock CPU, error_no = " << error_no << endl;
      ;//Warning, no permission to lock thread to CPU
      return 1;
    }
  }
  return 0;
}

void ndb_thread_fill_thread_object(void *param, uint *len, my_bool server)
{
  struct ThreadContainer container;

  memset((char*)&container, 0, sizeof(container));
  container.conf = globalEmulatorData.theConfiguration;
  container.type = server ? SocketServerThread : SocketClientThread;
  memcpy((char*)param, (char*)&container, sizeof(container));
  *len = sizeof(container);
}

void*
ndb_thread_add_thread_id(void *param)
{
  struct ThreadContainer container;

  memcpy((char*)&container, param, sizeof(struct ThreadContainer));
  container.index = container.conf->addThreadId(container.type);
  memcpy(param, (char*)&container, sizeof(struct ThreadContainer));
  return NULL;
}

void*
ndb_thread_remove_thread_id(void *param)
{
  struct ThreadContainer container;

  memcpy((char*)&container, param, sizeof(struct ThreadContainer));
  container.conf->removeThreadId(container.index);
  return NULL;
}

Uint32 Configuration::addThreadId(enum ThreadTypes type)
{
  NDB_TID_TYPE threadId;
  NDB_THAND_TYPE threadHandle;
  Uint32 i;
  NdbMutex_Lock(threadIdMutex);
  for (i = 0; i < threadInfo.size(); i++)
  {
    if (threadInfo[i].type == NotInUse)
      break;
  }
  if (i == threadInfo.size())
  {
    struct ThreadInfo tmp;
    threadInfo.push_back(tmp);
  }
  threadHandle = NdbThread_getThreadHandle();
  threadInfo[i].threadHandle = threadHandle;
  threadId = NdbThread_getThreadId();
  threadInfo[i].threadId = threadId;
  threadInfo[i].type = type;
  NdbMutex_Unlock(threadIdMutex);
  setRealtimeScheduler(threadHandle, type, _realtimeScheduler, TRUE);
  setLockCPU(threadId, type, (type == MainThread), TRUE);
  return i;
}

void
Configuration::removeThreadId(Uint32 index)
{
  NdbMutex_Lock(threadIdMutex);
  threadInfo[index].threadId = 0;
  threadInfo[index].threadHandle = 0;
  threadInfo[index].type = NotInUse;
  NdbMutex_Unlock(threadIdMutex);
}

void
Configuration::yield_main(Uint32 index, bool start)
{
  if (_realtimeScheduler)
  {
    if (start)
      setRealtimeScheduler(threadInfo[index].threadHandle,
                           threadInfo[index].type,
                           FALSE,
                           FALSE);
    else
      setRealtimeScheduler(threadInfo[index].threadHandle,
                           threadInfo[index].type,
                           TRUE,
                           FALSE);
  }
}

void
Configuration::initThreadArray()
{
  NdbMutex_Lock(threadIdMutex);
  for (Uint32 i = 0; i < threadInfo.size(); i++)
  {
    threadInfo[i].threadId = 0;
    threadInfo[i].threadHandle = 0;
    threadInfo[i].type = NotInUse;
  }
  NdbMutex_Unlock(threadIdMutex);
}

template class Vector<struct ThreadInfo>;

