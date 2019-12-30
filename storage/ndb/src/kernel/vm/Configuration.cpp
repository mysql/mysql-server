/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <TransporterRegistry.hpp>
#include "Configuration.hpp"
#include <ErrorHandlingMacros.hpp>
#include "GlobalData.hpp"

#include <ConfigRetriever.hpp>
#include <IPCConfig.hpp>
#include <ndb_version.h>
#include <NdbOut.hpp>
#include <WatchDog.hpp>
#include <NdbConfig.h>

#include <mgmapi_configuration.hpp>
#include <kernel_config_parameters.h>

#include <util/ConfigValues.hpp>
#include <NdbEnv.h>

#include <ndbapi_limits.h>
#include "mt.hpp"

#include "../../common/util/parse_mask.hpp"

#include <EventLogger.hpp>

#define JAM_FILE_ID 301

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
                                   NodeId allocated_nodeid,
                                   int connect_retries, int connect_delay)
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

  if(m_config_retriever->do_connect(connect_retries, connect_delay, 1) == -1){
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

    const int alloc_retries = 10;
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
  cfg->pack_v1(m_clusterConfigPacked_v1);
  if (OUR_V2_VERSION)
  {
    cfg->pack_v2(m_clusterConfigPacked_v2);
  }

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
#ifdef _WIN32
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

  /**
   * Iff we use the 'default' (non-mt) send buffer implementation, the
   * send buffers are allocated here.
   */
  if (getNonMTTransporterSendHandle() != NULL)
  {
    Uint32 total_send_buffer = 0;
    iter.get(CFG_TOTAL_SEND_BUFFER_MEMORY, &total_send_buffer);
    Uint64 extra_send_buffer = 0;
    iter.get(CFG_EXTRA_SEND_BUFFER_MEMORY, &extra_send_buffer);
    getNonMTTransporterSendHandle()->
      allocate_send_buffers(total_send_buffer,
                            extra_send_buffer);
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

  _schedulerResponsiveness = 5;
  iter.get(CFG_DB_SCHED_RESPONSIVENESS, &_schedulerResponsiveness);

  _schedulerExecutionTimer = 50;
  iter.get(CFG_DB_SCHED_EXEC_TIME, &_schedulerExecutionTimer);

  _schedulerSpinTimer = 0;
  iter.get(CFG_DB_SCHED_SPIN_TIME, &_schedulerSpinTimer);

  _maxSendDelay = 0;
  iter.get(CFG_DB_MAX_SEND_DELAY, &_maxSendDelay);

  _realtimeScheduler = 0;
  iter.get(CFG_DB_REALTIME_SCHEDULER, &_realtimeScheduler);

  if(iter.get(CFG_DB_WATCHDOG_INTERVAL_INITIAL, 
              &_timeBetweenWatchDogCheckInitial)){
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, "Invalid configuration fetched", 
	      "TimeBetweenWatchDogCheckInitial missing");
  }

#ifdef ERROR_INSERT
  _mixologyLevel = 0;
  iter.get(CFG_MIXOLOGY_LEVEL, &_mixologyLevel);
  if (_mixologyLevel)
  {
    ndbout_c("Mixology level set to 0x%x", _mixologyLevel);
    globalTransporterRegistry.setMixologyLevel(_mixologyLevel);
  }
#endif
  
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

#ifdef NDB_USE_GET_ENV
  const char * thrconfigstring = NdbEnv_GetEnv("NDB_MT_THREAD_CONFIG",
                                               (char*)0, 0);
#else
  const char * thrconfigstring = NULL;
#endif
  if (thrconfigstring ||
      iter.get(CFG_DB_MT_THREAD_CONFIG, &thrconfigstring) == 0)
  {
    int res = m_thr_config.do_parse(thrconfigstring,
                                    _realtimeScheduler,
                                    _schedulerSpinTimer);
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
#ifdef NDB_USE_GET_ENV
    const char* p = NdbEnv_GetEnv("NDB_MT_LQH", (char*)0, 0);
    if (p != 0)
    {
      if (strstr(p, "NOPLEASE") != 0)
        classic = 1;
    }
#endif
    Uint32 lqhthreads = 0;
    iter.get(CFG_NDBMT_LQH_THREADS, &lqhthreads);

    int res = m_thr_config.do_parse(mtthreads,
                                    lqhthreads,
                                    classic,
                                    _realtimeScheduler,
                                    _schedulerSpinTimer);
    if (res != 0)
    {
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG,
                "Invalid configuration fetched, invalid thread configuration",
                m_thr_config.getErrorMessage());
    }
  }
  if (NdbIsMultiThreaded())
  {
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
  }

  ConfigValues* cf = ConfigValuesFactory::extractCurrentSection(iter.m_config);

  if(m_clusterConfigIter)
    ndb_mgm_destroy_iterator(m_clusterConfigIter);
  m_clusterConfigIter = ndb_mgm_create_configuration_iterator
    (p, CFG_SECTION_NODE);

  /**
   * This is parts of get_multithreaded_config
   */
  do
  {
    globalData.isNdbMt = NdbIsMultiThreaded();
    if (!globalData.isNdbMt)
      break;

    globalData.ndbMtTcThreads = m_thr_config.getThreadCount(THRConfig::T_TC);
    globalData.ndbMtSendThreads =
      m_thr_config.getThreadCount(THRConfig::T_SEND);
    globalData.ndbMtReceiveThreads =
      m_thr_config.getThreadCount(THRConfig::T_RECV);

    globalData.isNdbMtLqh = true;
    {
      if (m_thr_config.getMtClassic())
      {
        globalData.isNdbMtLqh = false;
      }
    }

    if (!globalData.isNdbMtLqh)
      break;

    Uint32 threads = m_thr_config.getThreadCount(THRConfig::T_LDM);
    Uint32 workers = threads;
    iter.get(CFG_NDBMT_LQH_WORKERS, &workers);

#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
    // testing
    {
      const char* p;
      p = NdbEnv_GetEnv("NDBMT_LQH_WORKERS", (char*)0, 0);
      if (p != 0)
        workers = atoi(p);
    }
#endif
#endif


    assert(workers != 0 && workers <= MAX_NDBMT_LQH_WORKERS);
    assert(threads != 0 && threads <= MAX_NDBMT_LQH_THREADS);
    assert(workers % threads == 0);

    globalData.ndbMtLqhWorkers = workers;
    globalData.ndbMtLqhThreads = threads;
  } while (0);

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

Uint32
Configuration::maxSendDelay() const
{
  return _maxSendDelay;
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

#ifdef ERROR_INSERT
Uint32
Configuration::getMixologyLevel() const {
  return _mixologyLevel;
}

void
Configuration::setMixologyLevel(Uint32 l){
  _mixologyLevel = l;
}
#endif

const ndb_mgm_configuration_iterator * 
Configuration::getOwnConfigIterator() const {
  return m_ownConfigIterator;
}

const class ConfigValues*
Configuration::get_own_config_values()
{
  return &m_ownConfig->m_config;
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
Configuration::calcSizeAlt(ConfigValues * ownConfig)
{
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
  unsigned int noOfOperations = 32768;
  unsigned int noOfLocalOperations = 32;
  unsigned int noOfTransactions = 4096;
  unsigned int noOfIndexPages = 0;
  unsigned int noOfDataPages = 0;
  unsigned int noOfScanRecords = 256;
  unsigned int noOfLocalScanRecords = 32;
  unsigned int noBatchSize = 0;
  unsigned int noOfIndexOperations = 8192;
  unsigned int noOfTriggerOperations = 4000;
  unsigned int reservedScanRecords = 256 / 4;
  unsigned int reservedLocalScanRecords = 32 / 4;
  unsigned int reservedOperations = 32768 / 4;
  unsigned int reservedTransactions = 4096 / 4;
  unsigned int reservedIndexOperations = 8192 / 4;
  unsigned int reservedTriggerOperations = 4000 / 4;
  unsigned int transactionBufferBytes = 1048576;
  unsigned int reservedTransactionBufferBytes = 1048576 / 4;
  unsigned int maxOpsPerTrans = ~(Uint32)0;

  m_logLevel = new LogLevel();
  if (!m_logLevel)
  {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC, "Failed to create LogLevel", "");
  }
  
  struct AttribStorage { int paramId; Uint32 * storage; bool computable; };
  AttribStorage tmp[] = {
    { CFG_DB_NO_SCANS, &noOfScanRecords, false },
    { CFG_DB_RESERVED_SCANS, &reservedScanRecords, true },
    { CFG_DB_NO_LOCAL_SCANS, &noOfLocalScanRecords, true },
    { CFG_DB_RESERVED_LOCAL_SCANS, &reservedLocalScanRecords, true },
    { CFG_DB_BATCH_SIZE, &noBatchSize, false },
    { CFG_DB_NO_TABLES, &noOfTables, false },
    { CFG_DB_NO_ORDERED_INDEXES, &noOfOrderedIndexes, false },
    { CFG_DB_NO_UNIQUE_HASH_INDEXES, &noOfUniqueHashIndexes, false },
    { CFG_DB_NO_TRIGGERS, &noOfTriggers, true },
    { CFG_DB_NO_REPLICAS, &noOfReplicas, false },
    { CFG_DB_NO_ATTRIBUTES, &noOfAttributes, false },
    { CFG_DB_NO_OPS, &noOfOperations, false },
    { CFG_DB_RESERVED_OPS, &reservedOperations, true },
    { CFG_DB_NO_LOCAL_OPS, &noOfLocalOperations, true },
    { CFG_DB_NO_TRANSACTIONS, &noOfTransactions, false },
    { CFG_DB_RESERVED_TRANSACTIONS, &reservedTransactions, true },
    { CFG_DB_MAX_DML_OPERATIONS_PER_TRANSACTION, &maxOpsPerTrans, false },
    { CFG_DB_NO_INDEX_OPS, &noOfIndexOperations, true },
    { CFG_DB_RESERVED_INDEX_OPS, &reservedIndexOperations, true },
    { CFG_DB_NO_TRIGGER_OPS, &noOfTriggerOperations, true },
    { CFG_DB_RESERVED_TRIGGER_OPS, &reservedTriggerOperations, true },
    { CFG_DB_TRANS_BUFFER_MEM, &transactionBufferBytes, false },
    { CFG_DB_RESERVED_TRANS_BUFFER_MEM, &reservedTransactionBufferBytes, true },
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

  Uint32 tcInstances = 1;
  if (globalData.ndbMtTcThreads > 1)
  {
    tcInstances = globalData.ndbMtTcThreads;
  }

  Uint64 indexMem = 0, dataMem = 0;
  ndb_mgm_get_int64_parameter(&db, CFG_DB_DATA_MEM, &dataMem);
  ndb_mgm_get_int64_parameter(&db, CFG_DB_INDEX_MEM, &indexMem);
  if(dataMem == 0){
    BaseString::snprintf(buf, sizeof(buf), "ConfigParam: %d not found", CFG_DB_DATA_MEM);
    ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
  }

#define DO_DIV(x,y) (((x) + (y - 1)) / (y))

  noOfDataPages = (Uint32)(dataMem / 32768);
  noOfIndexPages = (Uint32)(indexMem / 8192);
  noOfIndexPages = DO_DIV(noOfIndexPages, lqhInstances);

  for(unsigned j = 0; j<LogLevel::LOGLEVEL_CATEGORIES; j++)
  {
    Uint32 tmp;
    if (!ndb_mgm_get_int_parameter(&db, CFG_MIN_LOGLEVEL+j, &tmp))
    {
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
      3 * noOfTables +            /* for Fully replicated tables, I/U/D */
      noOfOrderedIndexes;         /* for ordered indexes, C */
    if (noOfTriggers < neededNoOfTriggers)
    {
      noOfTriggers= neededNoOfTriggers;
      it2.set(CFG_DB_NO_TRIGGERS, noOfTriggers);
    }
    g_eventLogger->info("MaxNoOfTriggers set to %u", noOfTriggers);
  }

  /**
   * Do size calculations
   */
  ConfigValuesFactory cfg(ownConfig);

  cfg.begin();
  /**
   * Ensure that Backup doesn't fail due to lack of trigger resources
   */
  cfg.put(CFG_TUP_NO_TRIGGERS, noOfTriggers + 3 * noOfTables);

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
    noOfLocalScanRecords = tcInstances * lqhInstances *
      (noOfDBNodes * noOfScanRecords) +
#endif
      1 /* NR */ + 
      1 /* LCP */; 
  }
  if (noOfLocalOperations == 0) {
    if (noOfOperations == 0)
      noOfLocalOperations = 11 * 32768 / 10;
    else
      noOfLocalOperations= (11 * noOfOperations) / 10;
  }

  const Uint32 noOfTCLocalScanRecords = noOfLocalScanRecords;
  const Uint32 noOfTCScanRecords = noOfScanRecords;

  // ReservedXXX defaults to 25% of MaxNoOfXXX
  if (reservedScanRecords == 0)
  {
    reservedScanRecords = noOfScanRecords / 4;
  }
  if (reservedLocalScanRecords == 0)
  {
    reservedLocalScanRecords = noOfLocalScanRecords / 4;
  }
  if (reservedOperations == 0)
  {
    reservedOperations = noOfOperations / 4;
  }
  if (reservedTransactions == 0)
  {
    reservedTransactions = noOfTransactions / 4;
  }
  if (reservedIndexOperations == 0)
  {
    reservedIndexOperations = noOfIndexOperations / 4;
  }
  if (reservedTriggerOperations == 0)
  {
    reservedTriggerOperations = noOfTriggerOperations / 4;
  }
  if (reservedTransactionBufferBytes == 0)
  {
    reservedTransactionBufferBytes = transactionBufferBytes / 4;
  }

  noOfLocalOperations = DO_DIV(noOfLocalOperations, lqhInstances);
  noOfLocalScanRecords = DO_DIV(noOfLocalScanRecords, lqhInstances);

  {
    Uint32 noOfAccTables= noOfMetaTables/*noOfTables+noOfUniqueHashIndexes*/;
    /**
     * Acc Size Alt values
     */
    // Can keep 65536 pages (= 0.5 GByte)
    cfg.put(CFG_ACC_FRAGMENT,
	    NO_OF_FRAG_PER_NODE * noOfAccTables* noOfReplicas);
    
    /*-----------------------------------------------------------------------*/
    // The extra operation records added are used by the scan and node 
    // recovery process. 
    // Node recovery process will have its operations dedicated to ensure
    // that they never have a problem with allocation of the operation record.
    // The remainder are allowed for use by the scan processes.
    /*-----------------------------------------------------------------------*/
    /**
     * We add an extra 150 operations, 100 of those are dedicated to DBUTIL
     * interactions and LCP and Backup scans. The remaining 50 are
     * non-dedicated things for local usage.
     */
#define EXTRA_LOCAL_OPERATIONS 150
    cfg.put(CFG_ACC_OP_RECS,
	    (noOfLocalOperations + EXTRA_LOCAL_OPERATIONS) + 
	    (noOfLocalScanRecords * noBatchSize) +
	    NODE_RECOVERY_SCAN_OP_RECORDS);

    cfg.put(CFG_ACC_TABLE, noOfAccTables);
    
    cfg.put(CFG_ACC_SCAN, noOfLocalScanRecords);
  }
  
  {
    /**
     * Dih Size Alt values
     */
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
	    noOfLocalOperations + EXTRA_LOCAL_OPERATIONS);
    
    cfg.put(CFG_LQH_SCAN, 
	    noOfLocalScanRecords);
  }
  
  {
    /**
     * Spj Size Alt values
     */
    cfg.put(CFG_SPJ_TABLE, 
	    noOfMetaTables);
  }
  
  {
    /**
     * Tc Size Alt values
     */
    const Uint32 takeOverOperations = noOfOperations;
    if (maxOpsPerTrans == ~(Uint32)0)
    {
      maxOpsPerTrans = noOfOperations;
    }
    if (maxOpsPerTrans > noOfOperations)
    {
      BaseString::snprintf(
          buf,
          sizeof(buf),
          "Config param MaxDMLOperationsPerTransaction(%u) must not be bigger"
          " than available failover records given by "
          "MaxNoOfConcurrentOperations(%u)\n",
          maxOpsPerTrans,
          noOfOperations);
      ERROR_SET(fatal, NDBD_EXIT_INVALID_CONFIG, msg, buf);
    }

    cfg.put(CFG_TC_TARGET_FRAG_LOCATION, Uint32(0));
    cfg.put(CFG_TC_MAX_FRAG_LOCATION, UINT32_MAX);
    cfg.put(CFG_TC_RESERVED_FRAG_LOCATION, Uint32(0));

    cfg.put(CFG_TC_TARGET_SCAN_FRAGMENT, noOfTCLocalScanRecords);
    cfg.put(CFG_TC_MAX_SCAN_FRAGMENT, noOfTCLocalScanRecords);
    cfg.put(CFG_TC_RESERVED_SCAN_FRAGMENT, reservedLocalScanRecords / tcInstances);

    cfg.put(CFG_TC_TARGET_SCAN_RECORD, noOfTCScanRecords);
    cfg.put(CFG_TC_MAX_SCAN_RECORD, noOfTCScanRecords);
    cfg.put(CFG_TC_RESERVED_SCAN_RECORD, reservedScanRecords / tcInstances);

    cfg.put(CFG_TC_TARGET_CONNECT_RECORD, noOfOperations + 16 + noOfTransactions);
    cfg.put(CFG_TC_MAX_CONNECT_RECORD, noOfOperations + 16 + noOfTransactions);
    cfg.put(CFG_TC_RESERVED_CONNECT_RECORD, reservedOperations / tcInstances);

    cfg.put(CFG_TC_TARGET_TO_CONNECT_RECORD, takeOverOperations);
    cfg.put(CFG_TC_MAX_TO_CONNECT_RECORD, takeOverOperations);
    cfg.put(CFG_TC_RESERVED_TO_CONNECT_RECORD, takeOverOperations);

    cfg.put(CFG_TC_TARGET_COMMIT_ACK_MARKER, noOfTransactions);
    cfg.put(CFG_TC_MAX_COMMIT_ACK_MARKER, noOfTransactions);
    cfg.put(CFG_TC_RESERVED_COMMIT_ACK_MARKER, reservedTransactions / tcInstances);

    cfg.put(CFG_TC_TARGET_TO_COMMIT_ACK_MARKER, Uint32(0));
    cfg.put(CFG_TC_MAX_TO_COMMIT_ACK_MARKER, Uint32(0));
    cfg.put(CFG_TC_RESERVED_TO_COMMIT_ACK_MARKER, Uint32(0));

    cfg.put(CFG_TC_TARGET_INDEX_OPERATION, noOfIndexOperations);
    cfg.put(CFG_TC_MAX_INDEX_OPERATION, noOfIndexOperations);
    cfg.put(CFG_TC_RESERVED_INDEX_OPERATION, reservedIndexOperations / tcInstances);

    cfg.put(CFG_TC_TARGET_API_CONNECT_RECORD, noOfTransactions);
    cfg.put(CFG_TC_MAX_API_CONNECT_RECORD, noOfTransactions);
    cfg.put(CFG_TC_RESERVED_API_CONNECT_RECORD, reservedTransactions / tcInstances);

    cfg.put(CFG_TC_TARGET_TO_API_CONNECT_RECORD, reservedTransactions);
    cfg.put(CFG_TC_MAX_TO_API_CONNECT_RECORD, noOfTransactions);
    cfg.put(CFG_TC_RESERVED_TO_API_CONNECT_RECORD, reservedTransactions / tcInstances);

    cfg.put(CFG_TC_TARGET_CACHE_RECORD, noOfTransactions);
    cfg.put(CFG_TC_MAX_CACHE_RECORD, noOfTransactions);
    cfg.put(CFG_TC_RESERVED_CACHE_RECORD, reservedTransactions / tcInstances);

    cfg.put(CFG_TC_TARGET_FIRED_TRIGGER_DATA, noOfTriggerOperations);
    cfg.put(CFG_TC_MAX_FIRED_TRIGGER_DATA, noOfTriggerOperations);
    cfg.put(CFG_TC_RESERVED_FIRED_TRIGGER_DATA, reservedTriggerOperations / tcInstances);

    cfg.put(CFG_TC_TARGET_ATTRIBUTE_BUFFER, transactionBufferBytes);
    cfg.put(CFG_TC_MAX_ATTRIBUTE_BUFFER, transactionBufferBytes);
    cfg.put(CFG_TC_RESERVED_ATTRIBUTE_BUFFER, reservedTransactionBufferBytes / tcInstances);

    cfg.put(CFG_TC_TARGET_COMMIT_ACK_MARKER_BUFFER, 2 * noOfTransactions);
    cfg.put(CFG_TC_MAX_COMMIT_ACK_MARKER_BUFFER, 2 * noOfTransactions);
    cfg.put(CFG_TC_RESERVED_COMMIT_ACK_MARKER_BUFFER, 2 * reservedTransactions / tcInstances);

    cfg.put(CFG_TC_TARGET_TO_COMMIT_ACK_MARKER_BUFFER, Uint32(0));
    cfg.put(CFG_TC_MAX_TO_COMMIT_ACK_MARKER_BUFFER, Uint32(0));
    cfg.put(CFG_TC_RESERVED_TO_COMMIT_ACK_MARKER_BUFFER, Uint32(0));

    cfg.put(CFG_TC_TABLE, 
	    noOfMetaTables);
  }
  
  {
    /**
     * Tup Size Alt values
     */
    cfg.put(CFG_TUP_FRAG, 
	    NO_OF_FRAG_PER_NODE * noOfMetaTables* noOfReplicas);
    
    cfg.put(CFG_TUP_OP_RECS, 
	    noOfLocalOperations + EXTRA_LOCAL_OPERATIONS);
    
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

  require(cfg.commit(true));
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
      (exec_thread && threadInfo[i].type == BlockThread) ||
      (!exec_thread && threadInfo[i].type != BlockThread);

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
    bool high_prio = !((type == BlockThread) ||
                       (type == ReceiveThread) ||
                       (type == SendThread));
    if ((error_no = NdbThread_SetScheduler(pThread, real_time, high_prio)))
    {
      //Warning, no permission to set scheduler
      if (init)
      {
        g_eventLogger->info("Failed to set real-time prio on tid = %d,"
                            " error_no = %d",
                            NdbThread_GetTid(pThread), error_no);
        abort(); /* Fail on failures at init */
      }
      return 1;
    }
    else if (init)
    {
      g_eventLogger->info("Successfully set real-time prio on tid = %d",
                          NdbThread_GetTid(pThread));
    }
  }
  return 0;
}

int
Configuration::setLockCPU(NdbThread * pThread,
                          enum ThreadTypes type)
{
  int res = 0;
  if (type != BlockThread &&
      type != SendThread &&
      type != ReceiveThread)
  {
    if (type == NdbfsThread)
    {
      /*
       * NdbfsThread (IO threads).
       */
      res = m_thr_config.do_bind_io(pThread);
    }
    else
    {
      /*
       * WatchDogThread, SocketClientThread, SocketServerThread
       */
      res = m_thr_config.do_bind_watchdog(pThread);
    }
  }
  else if (!NdbIsMultiThreaded())
  {
    BlockNumber list[] = { DBDIH };
    res = m_thr_config.do_bind(pThread, list, 1);
  }

  if (res != 0)
  {
    if (res > 0)
    {
      g_eventLogger->info("Locked tid = %d to CPU ok",
                          NdbThread_GetTid(pThread));
      return 0;
    }
    else
    {
      g_eventLogger->info("Failed to lock tid = %d to CPU, error_no = %d",
                          NdbThread_GetTid(pThread), (-res));
#ifndef HAVE_MAC_OS_X_THREAD_INFO
      abort(); /* We fail when failing to lock to CPUs */
#endif
      return 1;
    }
  }

  return 0;
}

int
Configuration::setThreadPrio(NdbThread * pThread,
                             enum ThreadTypes type)
{
  int res = 0;
  unsigned thread_prio = 0;
  if (type != BlockThread &&
      type != SendThread &&
      type != ReceiveThread)
  {
    if (type == NdbfsThread)
    {
      /*
       * NdbfsThread (IO threads).
       */
      res = m_thr_config.do_thread_prio_io(pThread, thread_prio);
    }
    else
    {
      /*
       * WatchDogThread, SocketClientThread, SocketServerThread
       */
      res = m_thr_config.do_thread_prio_watchdog(pThread, thread_prio);
    }
  }
  else if (!NdbIsMultiThreaded())
  {
    BlockNumber list[] = { DBDIH };
    res = m_thr_config.do_thread_prio(pThread, list, 1, thread_prio);
  }

  if (res != 0)
  {
    if (res > 0)
    {
      g_eventLogger->info("Set thread prio to %u for tid: %d ok",
                          thread_prio, NdbThread_GetTid(pThread));
      return 0;
    }
    else
    {
      g_eventLogger->info("Failed to set thread prio to %u for tid: %d,"
                          " error_no = %d",
                          thread_prio,
                          NdbThread_GetTid(pThread),
                          (-res));
      abort(); /* We fail when failing to set thread prio */
      return 1;
    }
  }
  return 0;
}

bool
Configuration::get_io_real_time() const
{
  return m_thr_config.do_get_realtime_io();
}

const char*
Configuration::get_type_string(enum ThreadTypes type)
{
  const char *type_str;
  switch (type)
  {
    case WatchDogThread:
      type_str = "WatchDogThread";
      break;
    case SocketServerThread:
      type_str = "SocketServerThread";
      break;
    case SocketClientThread:
      type_str = "SocketClientThread";
      break;
    case NdbfsThread:
      type_str = "NdbfsThread";
      break;
    case BlockThread:
      type_str = "BlockThread";
      break;
    case SendThread:
      type_str = "SendThread";
      break;
    case ReceiveThread:
      type_str = "ReceiveThread";
      break;
    default:
      type_str = NULL;
      abort();
  }
  return type_str;
}

Uint32
Configuration::addThread(struct NdbThread* pThread,
                         enum ThreadTypes type,
                         bool single_threaded)
{
  const char *type_str;
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

  type_str = get_type_string(type);

  bool real_time;
  if (single_threaded)
  {
    setRealtimeScheduler(pThread, type, _realtimeScheduler, TRUE);
  }
  else if (type == WatchDogThread ||
           type == SocketClientThread ||
           type == SocketServerThread ||
           type == NdbfsThread)
  {
    if (type != NdbfsThread)
    {
      /**
       * IO threads are handled internally in NDBFS with
       * regard to setting real time properties on the
       * IO thread.
       *
       * WatchDog, SocketServer and SocketClient have no
       * special handling of real-time breaks since we
       * don't expect these threads to long without
       * breaks.
       */
      real_time = m_thr_config.do_get_realtime_wd();
      setRealtimeScheduler(pThread, type, real_time, TRUE);
    }
    /**
     * main threads are set in ThreadConfig::ipControlLoop
     * as it's handled differently with mt
     */
    ndbout_c("Started thread, index = %u, id = %d, type = %s",
             i,
             NdbThread_GetTid(pThread),
             type_str);
    setLockCPU(pThread, type);
  }
  /**
   * All other thread types requires special handling of real-time
   * property which is handled in the thread itself for multithreaded
   * nbdmtd process.
   */
  return i;
}

void
Configuration::removeThread(struct NdbThread *pThread)
{
  NdbMutex_Lock(threadIdMutex);
  for (Uint32 i = 0; i < threadInfo.size(); i++)
  {
    if (threadInfo[i].pThread == pThread)
    {
      threadInfo[i].pThread = 0;
      threadInfo[i].type = NotInUse;
      break;
    }
  }
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

