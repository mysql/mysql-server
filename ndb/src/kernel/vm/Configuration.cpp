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

#include "Configuration.hpp"
#include <ErrorHandlingMacros.hpp>
#include "GlobalData.hpp"

#include <ConfigRetriever.hpp>
#include <IPCConfig.hpp>
#include <ndb_version.h>
#include <NdbMem.h>
#include <NdbOut.hpp>
#include <WatchDog.hpp>

#include <getarg.h>

#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters_debug.h>
#include <kernel_config_parameters.h>

#include <kernel_types.h>
#include <ndb_limits.h>
#include "pc.hpp"
#include <LogLevel.hpp>
#include <NdbSleep.h>

extern "C" {
  void ndbSetOwnVersion();
}

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

bool
Configuration::init(int argc, const char** argv){

  /**
   * Default values for arguments
   */
  int _no_start = 0;
  int _initial = 0;
  const char* _connect_str = NULL;
  int _deamon = 0;
  int _help = 0;
  int _print_version = 0;
  
  /**
   * Arguments to NDB process
   */ 

  struct getargs args[] = {
    { "version", 'v', arg_flag, &_print_version, "Print ndbd version", "" },
    { "nostart", 'n', arg_flag, &_no_start,
      "Don't start ndbd immediately. Ndbd will await command from ndb_mgmd", "" },
    { "daemon", 'd', arg_flag, &_deamon, "Start ndbd as daemon", "" },
    { "initial", 'i', arg_flag, &_initial,
      "Perform initial start of ndbd, including cleaning the file system. Consult documentation before using this", "" },

    { "connect-string", 'c', arg_string, &_connect_str,
      "Set connect string for connecting to ndb_mgmd. <constr>=\"host=<hostname:port>[;nodeid=<id>]\". Overides specifying entries in NDB_CONNECTSTRING and config file",
      "<constr>" },
    { "usage", '?', arg_flag, &_help, "Print help", "" }
  };
  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0;
  char desc[] = 
    "The MySQL Cluster kernel";
  
  if(getarg(args, num_args, argc, argv, &optind) || _help) {
    arg_printusage(args, num_args, argv[0], desc);
    return false;
  }

#if 0  
  ndbout << "no_start=" <<_no_start<< endl;
  ndbout << "initial=" <<_initial<< endl;
  ndbout << "deamon=" <<_deamon<< endl;
  ndbout << "connect_str="<<_connect_str<<endl;
  arg_printusage(args, num_args, argv[0], desc);
  return false;
#endif

  ndbSetOwnVersion();

  if (_print_version) {
    ndbPrintVersion();
    return false;
  }

  // Check the start flag
  if (_no_start)
    globalData.theRestartFlag = initial_state;
  else 
    globalData.theRestartFlag = perform_start;

  // Check the initial flag
  if (_initial)
    _initialStart = true;
  
  // Check connectstring
  if (_connect_str)
    _connectString = strdup(_connect_str);
  
  // Check deamon flag
  if (_deamon)
    _daemonMode = true;

  // Save programname
  if(argc > 0 && argv[0] != 0)
    _programName = strdup(argv[0]);
  else
    _programName = strdup("");
  
  return true;
}

Configuration::Configuration()
{
  _programName = 0;
  _connectString = 0;
  _fsPath = 0;
  _initialStart = false;
  _daemonMode = false;
  m_config_retriever= 0;
}

Configuration::~Configuration(){
  if(_programName != NULL)
    free(_programName);

  if(_fsPath != NULL)
    free(_fsPath);

  if (m_config_retriever) {
    delete m_config_retriever;
  }
}

void
Configuration::closeConfiguration(){
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

  m_config_retriever= new ConfigRetriever(NDB_VERSION, NODE_TYPE_DB);
  m_config_retriever->setConnectString(_connectString ? _connectString : "");
  if(m_config_retriever->init() == -1 ||
     m_config_retriever->do_connect() == -1){
    
    const char * s = m_config_retriever->getErrorString();
    if(s == 0)
      s = "No error given!";
    
    /* Set stop on error to true otherwise NDB will
       go into an restart loop...
    */
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Could connect to ndb_mgmd", s);
  }
  
  ConfigRetriever &cr= *m_config_retriever;
  
  if((globalData.ownId = cr.allocNodeId()) == 0){
    for(Uint32 i = 0; i<3; i++){
      NdbSleep_SecSleep(3);
      if(globalData.ownId = cr.allocNodeId())
	break;
    }
  }
  
  if(globalData.ownId == 0){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, 
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
    
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Could not fetch configuration"
	      "/invalid configuration", s);
  }
  if(m_clusterConfig)
    free(m_clusterConfig);
  
  m_clusterConfig = p;
  
  ndb_mgm_configuration_iterator iter(* p, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, globalData.ownId)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", "DB missing");
  }
  
  if(iter.get(CFG_DB_STOP_ON_ERROR, &_stopOnError)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "StopOnError missing");
  }
}

void
Configuration::setupConfiguration(){
  ndb_mgm_configuration * p = m_clusterConfig;

  /**
   * Configure transporters
   */
  {  
    int res = IPCConfig::configureTransporters(globalData.ownId,
					       * p, 
					       globalTransporterRegistry);
    if(res <= 0){
      ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
		"No transporters configured");
    }
  }

  /**
   * Setup cluster configuration data
   */
  ndb_mgm_configuration_iterator iter(* p, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, globalData.ownId)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", "DB missing");
  }

  unsigned type;
  if(!(iter.get(CFG_TYPE_OF_SECTION, &type) == 0 && type == NODE_TYPE_DB)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched",
	      "I'm wrong type of node");
  }
  
  if(iter.get(CFG_DB_NO_SAVE_MSGS, &_maxErrorLogs)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "MaxNoOfSavedMessages missing");
  }
  
  if(iter.get(CFG_DB_MEMLOCK, &_lockPagesInMainMemory)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "LockPagesInMainMemory missing");
  }

  if(iter.get(CFG_DB_WATCHDOG_INTERVAL, &_timeBetweenWatchDogCheck)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "TimeBetweenWatchDogCheck missing");
  }

  /**
   * Get filesystem path
   */  
  { 
    const char* pFileSystemPath = NULL;
    if(iter.get(CFG_DB_FILESYSTEM_PATH, &pFileSystemPath)){
      ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
		"FileSystemPath missing");
    } 
    
    if(pFileSystemPath == 0 || strlen(pFileSystemPath) == 0){
      ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
		"Configuration does not contain valid filesystem path");
    }
    
    if(pFileSystemPath[strlen(pFileSystemPath) - 1] == '/')
      _fsPath = strdup(pFileSystemPath);
    else {
      _fsPath = (char *)malloc(strlen(pFileSystemPath) + 2);
      strcpy(_fsPath, pFileSystemPath);
      strcat(_fsPath, "/");
    }
  }
  
  if(iter.get(CFG_DB_STOP_ON_ERROR_INSERT, &m_restartOnErrorInsert)){
    ERROR_SET(fatal, ERR_INVALID_CONFIG, "Invalid configuration fetched", 
	      "RestartOnErrorInsert missing");
  }

  /**
   * Create the watch dog thread
   */
  { 
    Uint32 t = _timeBetweenWatchDogCheck;
    t = globalEmulatorData.theWatchDog ->setCheckInterval(t);
    _timeBetweenWatchDogCheck = t;
  }
  
  ConfigValues* cf = ConfigValuesFactory::extractCurrentSection(iter.m_config);

  m_clusterConfigIter = ndb_mgm_create_configuration_iterator
    (p, CFG_SECTION_NODE);

  calcSizeAlt(cf);
}

bool 
Configuration::lockPagesInMainMemory() const {
  return _lockPagesInMainMemory;
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

char *
Configuration::getConnectStringCopy() const {
  if(_connectString != 0)
    return strdup(_connectString);
  return 0;
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
  unsigned int noOfIndexes = 0;
  unsigned int noOfReplicas = 0;
  unsigned int noOfDBNodes = 0;
  unsigned int noOfAPINodes = 0;
  unsigned int noOfMGMNodes = 0;
  unsigned int noOfNodes = 0;
  unsigned int noOfAttributes = 0;
  unsigned int noOfOperations = 0;
  unsigned int noOfTransactions = 0;
  unsigned int noOfIndexPages = 0;
  unsigned int noOfDataPages = 0;
  unsigned int noOfScanRecords = 0;
  m_logLevel = new LogLevel();
  
  /**
   * {"NoOfConcurrentCheckpointsDuringRestart", &cd.ispValues[1][5] },
   * {"NoOfConcurrentCheckpointsAfterRestart", &cd.ispValues[2][4] },
   * {"NoOfConcurrentProcessesHandleTakeover", &cd.ispValues[1][7] },
   * {"TimeToWaitAlive", &cd.ispValues[0][0] },
   */
  struct AttribStorage { int paramId; Uint32 * storage; };
  AttribStorage tmp[] = {
    { CFG_DB_NO_SCANS, &noOfScanRecords },
    { CFG_DB_NO_TABLES, &noOfTables },
    { CFG_DB_NO_INDEXES, &noOfIndexes },
    { CFG_DB_NO_REPLICAS, &noOfReplicas },
    { CFG_DB_NO_ATTRIBUTES, &noOfAttributes },
    { CFG_DB_NO_OPS, &noOfOperations },
    { CFG_DB_NO_TRANSACTIONS, &noOfTransactions }
#if 0
    { "NoOfDiskPagesToDiskDuringRestartTUP", &cd.ispValues[3][8] },
    { "NoOfDiskPagesToDiskAfterRestartTUP", &cd.ispValues[3][9] },
    { "NoOfDiskPagesToDiskDuringRestartACC", &cd.ispValues[3][10] },
    { "NoOfDiskPagesToDiskAfterRestartACC", &cd.ispValues[3][11] },
#endif
  };

  ndb_mgm_configuration_iterator db(*(ndb_mgm_configuration*)ownConfig, 0);
  
  const int sz = sizeof(tmp)/sizeof(AttribStorage);
  for(int i = 0; i<sz; i++){
    if(ndb_mgm_get_int_parameter(&db, tmp[i].paramId, tmp[i].storage)){
      snprintf(buf, sizeof(buf), "ConfigParam: %d not found", tmp[i].paramId);
      ERROR_SET(fatal, ERR_INVALID_CONFIG, msg, buf);
    }
  }

  Uint64 indexMem = 0, dataMem = 0;
  ndb_mgm_get_int64_parameter(&db, CFG_DB_DATA_MEM, &dataMem);
  ndb_mgm_get_int64_parameter(&db, CFG_DB_INDEX_MEM, &indexMem);
  if(dataMem == 0){
    snprintf(buf, sizeof(buf), "ConfigParam: %d not found", CFG_DB_DATA_MEM);
    ERROR_SET(fatal, ERR_INVALID_CONFIG, msg, buf);
  }

  if(indexMem == 0){
    snprintf(buf, sizeof(buf), "ConfigParam: %d not found", CFG_DB_INDEX_MEM);
    ERROR_SET(fatal, ERR_INVALID_CONFIG, msg, buf);
  }

  noOfDataPages = (dataMem / 8192);
  noOfIndexPages = (indexMem / 8192);

  for(unsigned j = 0; j<LogLevel::LOGLEVEL_CATEGORIES; j++){
    Uint32 tmp;
    if(!ndb_mgm_get_int_parameter(&db, LogLevel::MIN_LOGLEVEL_ID+j, &tmp)){
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
      ERROR_SET(fatal, ERR_INVALID_CONFIG, msg, "Node data (Id) missing");
    }
    
    if(ndb_mgm_get_int_parameter(p, CFG_TYPE_OF_SECTION, &nodeType)){
      ERROR_SET(fatal, ERR_INVALID_CONFIG, msg, "Node data (Type) missing");
    }
    
    if(nodeId > MAX_NODES || nodeId == 0){
      snprintf(buf, sizeof(buf),
	       "Invalid node id: %d", nodeId);
      ERROR_SET(fatal, ERR_INVALID_CONFIG, msg, buf);
    }
    
    if(nodes.get(nodeId)){
      snprintf(buf, sizeof(buf), "Two node can not have the same node id: %d",
	       nodeId);
      ERROR_SET(fatal, ERR_INVALID_CONFIG, msg, buf);
    }
    nodes.set(nodeId);
        
    switch(nodeType){
    case NODE_TYPE_DB:
      noOfDBNodes++; // No of NDB processes
      
      if(nodeId > MAX_NDB_NODES){
	snprintf(buf, sizeof(buf), "Maximum node id for a ndb node is: %d", 
		 MAX_NDB_NODES);
	ERROR_SET(fatal, ERR_INVALID_CONFIG, msg, buf);
      }
      break;
    case NODE_TYPE_API:
      noOfAPINodes++; // No of API processes
      break;
    case NODE_TYPE_REP:
      break;
    case NODE_TYPE_MGM:
      noOfMGMNodes++; // No of MGM processes
      break;
    case NODE_TYPE_EXT_REP:
      break;
    default:
      snprintf(buf, sizeof(buf), "Unknown node type: %d", nodeType);
      ERROR_SET(fatal, ERR_INVALID_CONFIG, msg, buf);
    }
  }
  noOfNodes = nodeNo;
  
  /**
   * Do size calculations
   */
  ConfigValuesFactory cfg(ownConfig);

  noOfTables++;         		 // Remove impact of system table
  noOfTables += noOfIndexes; // Indexes are tables too
  noOfAttributes += 2;  // ---"----
  noOfTables *= 2;      // Remove impact of Dict need 2 ids for each table

  if (noOfDBNodes > 15) {
    noOfDBNodes = 15;
  }//if
  Uint32 noOfLocalScanRecords = (noOfDBNodes * noOfScanRecords) + 1;
  Uint32 noOfTCScanRecords = noOfScanRecords;

  {
    /**
     * Acc Size Alt values
     */
    // Can keep 65536 pages (= 0.5 GByte)
    cfg.put(CFG_ACC_DIR_RANGE, 
	    4 * NO_OF_FRAG_PER_NODE * noOfTables* noOfReplicas); 
    
    cfg.put(CFG_ACC_DIR_ARRAY,
	    (noOfIndexPages >> 8) + 
	    4 * NO_OF_FRAG_PER_NODE * noOfTables* noOfReplicas);
    
    cfg.put(CFG_ACC_FRAGMENT,
	    2 * NO_OF_FRAG_PER_NODE * noOfTables* noOfReplicas);
    
    /*-----------------------------------------------------------------------*/
    // The extra operation records added are used by the scan and node 
    // recovery process. 
    // Node recovery process will have its operations dedicated to ensure
    // that they never have a problem with allocation of the operation record.
    // The remainder are allowed for use by the scan processes.
    /*-----------------------------------------------------------------------*/
    cfg.put(CFG_ACC_OP_RECS,
	    ((11 * noOfOperations) / 10 + 50) + 
	    (noOfLocalScanRecords * MAX_PARALLEL_SCANS_PER_FRAG) +
	    NODE_RECOVERY_SCAN_OP_RECORDS);
    
    cfg.put(CFG_ACC_OVERFLOW_RECS,
	    noOfIndexPages + 
	    2 * NO_OF_FRAG_PER_NODE * noOfTables* noOfReplicas);
    
    cfg.put(CFG_ACC_PAGE8, 
	    noOfIndexPages + 32);
    
    cfg.put(CFG_ACC_ROOT_FRAG, 
	    NO_OF_FRAG_PER_NODE * noOfTables* noOfReplicas);
    
    cfg.put(CFG_ACC_TABLE, noOfTables);
    
    cfg.put(CFG_ACC_SCAN, noOfLocalScanRecords);
  }
  
  {
    /**
     * Dict Size Alt values
     */
    cfg.put(CFG_DICT_ATTRIBUTE, 
	    noOfAttributes);

    cfg.put(CFG_DICT_TABLE, 
	    noOfTables);
  }
  
  {
    /**
     * Dih Size Alt values
     */
    cfg.put(CFG_DIH_API_CONNECT, 
	    2 * noOfTransactions);
    
    cfg.put(CFG_DIH_CONNECT, 
	    noOfOperations + noOfTransactions + 46);
    
    cfg.put(CFG_DIH_FRAG_CONNECT, 
	    NO_OF_FRAG_PER_NODE *  noOfTables *  noOfDBNodes);
    
    int temp;
    temp = noOfReplicas - 2;
    if (temp < 0)
      temp = 1;
    else
      temp++;
    cfg.put(CFG_DIH_MORE_NODES, 
	    temp * NO_OF_FRAG_PER_NODE *
	    noOfTables *  noOfDBNodes);
    
    cfg.put(CFG_DIH_REPLICAS, 
	    NO_OF_FRAG_PER_NODE * noOfTables *
	    noOfDBNodes * noOfReplicas);

    cfg.put(CFG_DIH_TABLE, 
	    noOfTables);
  }
  
  {
    /**
     * Lqh Size Alt values
     */
    cfg.put(CFG_LQH_FRAG, 
	    NO_OF_FRAG_PER_NODE * noOfTables * noOfReplicas);
    
    cfg.put(CFG_LQH_TABLE, 
	    noOfTables);

    cfg.put(CFG_LQH_TC_CONNECT, 
	    (11 * noOfOperations) / 10 + 50);
    
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
	    noOfTables);
    
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
	    2 * NO_OF_FRAG_PER_NODE * noOfTables* noOfReplicas);
    
    cfg.put(CFG_TUP_OP_RECS, 
	    (11 * noOfOperations) / 10 + 50);
    
    cfg.put(CFG_TUP_PAGE, 
	    noOfDataPages);
    
    cfg.put(CFG_TUP_PAGE_RANGE, 
	    4 * NO_OF_FRAG_PER_NODE * noOfTables* noOfReplicas);
    
    cfg.put(CFG_TUP_TABLE, 
	    noOfTables);
    
    cfg.put(CFG_TUP_TABLE_DESC, 
	    4 * NO_OF_FRAG_PER_NODE * noOfAttributes* noOfReplicas +
	    12 * NO_OF_FRAG_PER_NODE * noOfTables* noOfReplicas );
    
    cfg.put(CFG_TUP_STORED_PROC,
	    noOfLocalScanRecords);
  }

  {
    /**
     * Tux Size Alt values
     */
    cfg.put(CFG_TUX_INDEX, 
	    noOfTables);
    
    cfg.put(CFG_TUX_FRAGMENT, 
	    2 * NO_OF_FRAG_PER_NODE * noOfTables * noOfReplicas);
    
    cfg.put(CFG_TUX_ATTRIBUTE, 
	    noOfIndexes * 4);

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
