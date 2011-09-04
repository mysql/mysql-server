/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "Configuration.hpp"
#include <ErrorHandlingMacros.hpp>
#include "GlobalData.hpp"

#include <ConfigRetriever.hpp>
#include <IPCConfig.hpp>
#include <ndb_version.h>
#include <NdbMem.h>
#include <NdbOut.hpp>
#include <WatchDog.hpp>
#include <NdbConfig.h>

#include <mgmapi_configuration.hpp>
#include <kernel_config_parameters.h>

#include <util/ConfigValues.hpp>
#include <NdbEnv.h>

#include <ndbapi_limits.h>
#include "mt.hpp"

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

extern Uint32 g_start_type;

bool
Configuration::init(int _no_start, int _initial,
                    int _initialstart)
{
  // Check the start flag
  if (_no_start)
    globalData.theRestartFlag = initial_state;
  else 
    globalData.theRestartFlag = perform_start;

  // Check the initial flag
  if (_initial)
    _initialStart = true;

  globalData.ownId= 0;

  if (_initialstart)
  {
    _initialStart = true;
    g_start_type |= (1 << NodeState::ST_INITIAL_START);
  }

  threadIdMutex = NdbMutex_Create();
  if (!threadIdMutex)
  {
    g_eventLogger->error("Failed to create threadIdMutex");
    return false;
  }
  initThreadArray();
  return true;
}

Configuration::Configuration()
{
  _fsPath = 0;
  _backupPath = 0;
  _initialStart = false;
  m_config_retriever= 0;
  m_clusterConfig= 0;
  m_clusterConfigIter= 0;
  m_logLevel= 0;
}

Configuration::~Configuration(){

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
Configuration::fetch_configuration(const char* _connect_string,
                                   int force_nodeid,
                                   const char* _bind_address,
                                   NodeId allocated_nodeid)
{
  /**
   * Fetch configuration from management server
   */
  if (m_config_retriever) {
    delete m_config_retriever;
  }

  m_config_retriever= new ConfigRetriever(_connect_string,
                                          force_nodeid,
                                          NDB_VERSION,
                                          NDB_MGM_NODE_TYPE_NDB,
					  _bind_address);
  if (!m_config_retriever)
  {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC,
              "Failed to create ConfigRetriever", "");
  }

  if (m_config_retriever->hasError())
  {
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG,
	      "Could not initialize handle to management server",
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

  ConfigRetriever &cr= *m_config_retriever;

  if (allocated_nodeid)
  {
    // The angel has already allocated the nodeid, no need to
    // allocate it
    globalData.ownId = allocated_nodeid;
  }
  else
  {

    const int alloc_retries = 2;
    const int alloc_delay = 3;
    globalData.ownId = cr.allocNodeId(alloc_retries, alloc_delay);
    if(globalData.ownId == 0)
    {
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG,
                "Unable to alloc node id",
                m_config_retriever->getErrorString());
    }
  }
  assert(globalData.ownId);

  ndb_mgm_configuration * p = cr.getConfig(globalData.ownId);
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

  const ConfigValues * cfg = (ConfigValues*)m_clusterConfig;
  cfg->pack(m_clusterConfigPacked);

  {
    Uint32 generation;
    ndb_mgm_configuration_iterator sys_iter(*p, CFG_SECTION_SYSTEM);
    if (sys_iter.get(CFG_SYS_CONFIG_GENERATION, &generation))
    {
      g_eventLogger->info("Configuration fetched from '%s:%d', unknown generation!! (likely older ndb_mgmd)",
                          m_config_retriever->get_mgmd_host(),
                          m_config_retriever->get_mgmd_port());
    }
    else
    {
      g_eventLogger->info("Configuration fetched from '%s:%d', generation: %d",
                          m_config_retriever->get_mgmd_host(),
                          m_config_retriever->get_mgmd_port(),
                          generation);
    }
  }

  ndb_mgm_configuration_iterator iter(* p, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, globalData.ownId)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", "DB missing");
  }
  
  if(iter.get(CFG_DB_STOP_ON_ERROR, &_stopOnError)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", 
	      "StopOnError missing");
  }

  const char * datadir;
  if(iter.get(CFG_NODE_DATADIR, &datadir)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched",
	      "DataDir missing");
  }
  NdbConfig_SetPath(datadir);

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

#include "../../common/util/parse_mask.hpp"

void
Configuration::setupConfiguration(){

  DBUG_ENTER("Configuration::setupConfiguration");

  ndb_mgm_configuration * p = m_clusterConfig;

  /**
   * Configure transporters
   */
  if (!globalTransporterRegistry.init(globalData.ownId))
  {
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG,
              "Invalid configuration fetched",
              "Could not init transporter registry");
  }

  if (!IPCConfig::configureTransporters(globalData.ownId,
                                        * p,
                                        globalTransporterRegistry))
  {
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG,
              "Invalid configuration fetched",
              "Could not configure transporters");
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

  Uint32 total_send_buffer = 0;
  iter.get(CFG_TOTAL_SEND_BUFFER_MEMORY, &total_send_buffer);
  globalTransporterRegistry.allocate_send_buffers(total_send_buffer);
  
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

  const char * lockmask = 0;
  {
    if (iter.get(CFG_DB_EXECUTE_LOCK_CPU, &lockmask) == 0)
    {
      int res = m_thr_config.setLockExecuteThreadToCPU(lockmask);
      if (res < 0)
      {
        // Could not parse LockExecuteThreadToCPU mask
        g_eventLogger->warning("Failed to parse 'LockExecuteThreadToCPU=%s' "
                               "(error: %d), ignoring it!",
                               lockmask, res);
      }
    }
  }

  {
    Uint32 maintCPU = NO_LOCK_CPU;
    iter.get(CFG_DB_MAINT_LOCK_CPU, &maintCPU);
    if (maintCPU == 65535)
      maintCPU = NO_LOCK_CPU; // Ignore old default(may come from old mgmd)
    if (maintCPU != NO_LOCK_CPU)
      m_thr_config.setLockIoThreadsToCPU(maintCPU);
  }

  const char * thrconfigstring = NdbEnv_GetEnv("NDB_MT_THREAD_CONFIG",
                                               (char*)0, 0);
  if (thrconfigstring ||
      iter.get(CFG_DB_MT_THREAD_CONFIG, &thrconfigstring) == 0)
  {
    int res = m_thr_config.do_parse(thrconfigstring);
    if (res != 0)
    {
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG,
                "Invalid configuration fetched, invalid ThreadConfig",
                m_thr_config.getErrorMessage());
    }
  }
  else
  {
    Uint32 mtthreads = 0;
    iter.get(CFG_DB_MT_THREADS, &mtthreads);

    Uint32 classic = 0;
    iter.get(CFG_NDBMT_CLASSIC, &classic);
    const char* p = NdbEnv_GetEnv("NDB_MT_LQH", (char*)0, 0);
    if (p != 0)
    {
      if (strstr(p, "NOPLEASE") != 0)
        classic = 1;
    }

    Uint32 lqhthreads = 0;
    iter.get(CFG_NDBMT_LQH_THREADS, &lqhthreads);

    int res = m_thr_config.do_parse(mtthreads, lqhthreads, classic);
    if (res != 0)
    {
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG,
                "Invalid configuration fetched, invalid thread configuration",
                m_thr_config.getErrorMessage());
    }
  }
  if (thrconfigstring)
  {
    ndbout_c("ThreadConfig: input: %s LockExecuteThreadToCPU: %s => parsed: %s",
             thrconfigstring,
             lockmask ? lockmask : "",
             m_thr_config.getConfigString());
  }
  else
  {
    ndbout_c("ThreadConfig (old ndb_mgmd) LockExecuteThreadToCPU: %s => parsed: %s",
             lockmask ? lockmask : "",
             m_thr_config.getConfigString());
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

const ndb_mgm_configuration_iterator * 
Configuration::getOwnConfigIterator() const {
  return m_ownConfigIterator;
}
  
ndb_mgm_configuration_iterator * 
Configuration::getClusterConfigIterator() const {
  return m_clusterConfigIter;
}

Uint32 
Configuration::get_config_generation() const {
  Uint32 generation = ~0;
  ndb_mgm_configuration_iterator sys_iter(*m_clusterConfig,
                                          CFG_SECTION_SYSTEM);
  sys_iter.get(CFG_SYS_CONFIG_GENERATION, &generation);
  return generation;
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
  if (!m_logLevel)
  {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC, "Failed to create LogLevel", "");
  }
  
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

  Uint32 lqhInstances = 1;
  if (globalData.isNdbMtLqh)
  {
    lqhInstances = globalData.ndbMtLqhWorkers;
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

#define DO_DIV(x,y) (((x) + (y - 1)) / (y))

  noOfDataPages = (Uint32)(dataMem / 32768);
  noOfIndexPages = (Uint32)(indexMem / 8192);
  noOfIndexPages = DO_DIV(noOfIndexPages, lqhInstances);

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
  if (noOfMetaTablesDict > NDB_MAX_TABLES)
    noOfMetaTablesDict= NDB_MAX_TABLES;

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
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
    noOfLocalScanRecords = (noOfDBNodes * noOfScanRecords) + 
#else
    noOfLocalScanRecords = 4 * (noOfDBNodes * noOfScanRecords) +
#endif
      1 /* NR */ + 
      1 /* LCP */; 
  }
  if (noOfLocalOperations == 0) {
    noOfLocalOperations= (11 * noOfOperations) / 10;
  }

  Uint32 noOfTCScanRecords = noOfScanRecords;
  Uint32 noOfTCLocalScanRecords = noOfLocalScanRecords;

  noOfLocalOperations = DO_DIV(noOfLocalOperations, lqhInstances);
  noOfLocalScanRecords = DO_DIV(noOfLocalScanRecords, lqhInstances);

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
    
    Uint32 noFragPerTable= (((noOfDBNodes * lqhInstances) + 
                             NO_OF_FRAGS_PER_CHUNK - 1) >>
                            LOG_NO_OF_FRAGS_PER_CHUNK) <<
      LOG_NO_OF_FRAGS_PER_CHUNK;

    cfg.put(CFG_DIH_FRAG_CONNECT, 
	    noFragPerTable *  noOfMetaTables);
    
    cfg.put(CFG_DIH_REPLICAS, 
	    NO_OF_FRAG_PER_NODE * noOfMetaTables *
	    noOfDBNodes * noOfReplicas * lqhInstances);

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
	    noOfTCLocalScanRecords);
    
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
    
    cfg.put(CFG_TUP_TABLE, 
	    noOfMetaTables);
    
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
Configuration::setAllRealtimeScheduler()
{
  Uint32 i;
  for (i = 0; i < threadInfo.size(); i++)
  {
    if (threadInfo[i].type != NotInUse)
    {
      if (setRealtimeScheduler(threadInfo[i].pThread,
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
    if (threadInfo[i].type == NotInUse)
      continue;

    bool run = 
      (exec_thread && threadInfo[i].type == MainThread) ||
      (!exec_thread && threadInfo[i].type != MainThread);

    if (run)
    {
      setLockCPU(threadInfo[i].pThread, threadInfo[i].type);
    }
  }
}

int
Configuration::setRealtimeScheduler(NdbThread* pThread,
                                    enum ThreadTypes type,
                                    bool real_time,
                                    bool init)
{
  /*
    We ignore thread characteristics on platforms where we cannot
    determine the thread id.
  */
  if (!init || real_time)
  {
    int error_no;
    if ((error_no = NdbThread_SetScheduler(pThread, real_time,
                                           (type != MainThread))))
    {
      //Warning, no permission to set scheduler
      return 1;
    }
  }
  return 0;
}

int
Configuration::setLockCPU(NdbThread * pThread,
                          enum ThreadTypes type)
{
  int res = 0;
  if (type != MainThread)
  {
    res = m_thr_config.do_bind_io(pThread);
  }
  else if (!NdbIsMultiThreaded())
  {
    BlockNumber list[] = { CMVMI };
    res = m_thr_config.do_bind(pThread, list, 1);
  }

  if (res != 0)
  {
    if (res > 0)
    {
      ndbout << "Locked to CPU ok" << endl;
      return 0;
    }
    else
    {
      ndbout << "Failed to lock CPU, error_no = " << (-res) << endl;
      return 1;
    }
  }

  return 0;
}

Uint32
Configuration::addThread(struct NdbThread* pThread, enum ThreadTypes type)
{
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
  threadInfo[i].pThread = pThread;
  threadInfo[i].type = type;
  NdbMutex_Unlock(threadIdMutex);
  setRealtimeScheduler(pThread, type, _realtimeScheduler, TRUE);
  if (type != MainThread)
  {
    /**
     * main threads are set in ThreadConfig::ipControlLoop
     * as it's handled differently with mt
     */
    setLockCPU(pThread, type);
  }
  return i;
}

void
Configuration::removeThreadId(Uint32 index)
{
  NdbMutex_Lock(threadIdMutex);
  threadInfo[index].pThread = 0;
  threadInfo[index].type = NotInUse;
  NdbMutex_Unlock(threadIdMutex);
}

void
Configuration::yield_main(Uint32 index, bool start)
{
  if (_realtimeScheduler)
  {
    if (start)
      setRealtimeScheduler(threadInfo[index].pThread,
                           threadInfo[index].type,
                           FALSE,
                           FALSE);
    else
      setRealtimeScheduler(threadInfo[index].pThread,
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
    threadInfo[i].pThread = 0;
    threadInfo[i].type = NotInUse;
  }
  NdbMutex_Unlock(threadIdMutex);
}

template class Vector<struct ThreadInfo>;

