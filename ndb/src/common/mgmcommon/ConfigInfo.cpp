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

#include <NdbTCP.h>
#include "ConfigInfo.hpp"
#include <mgmapi_config_parameters.h>
#include <ndb_limits.h>

#define MAX_LINE_LENGTH 255
#define KEY_INTERNAL 0
#define MAX_INT_RNIL (RNIL - 1)
/****************************************************************************
 * Section names
 ****************************************************************************/

const ConfigInfo::AliasPair
ConfigInfo::m_sectionNameAliases[]={
  {"API", "MYSQLD"},
  {"DB", "NDBD"},
  {"MGM", "NDB_MGMD"},
  {0, 0}
};

const char* 
ConfigInfo::m_sectionNames[]={
  "SYSTEM",
  "EXTERNAL SYSTEM",
  "COMPUTER",

  "DB",
  "MGM",
  "API",
  "REP",
  "EXTERNAL REP",

  "TCP",
  "SCI",
  "SHM",
  "OSE"
};
const int ConfigInfo::m_noOfSectionNames = 
sizeof(m_sectionNames)/sizeof(char*);


/****************************************************************************
 * Section Rules declarations
 ****************************************************************************/
static bool transformComputer(InitConfigFileParser::Context & ctx, const char *);
static bool transformSystem(InitConfigFileParser::Context & ctx, const char *);
static bool transformExternalSystem(InitConfigFileParser::Context & ctx, const char *);
static bool transformNode(InitConfigFileParser::Context & ctx, const char *);
static bool transformExtNode(InitConfigFileParser::Context & ctx, const char *);
static bool transformConnection(InitConfigFileParser::Context & ctx, const char *);
static bool applyDefaultValues(InitConfigFileParser::Context & ctx, const char *);
static bool checkMandatory(InitConfigFileParser::Context & ctx, const char *);
static bool fixPortNumber(InitConfigFileParser::Context & ctx, const char *);
static bool fixShmkey(InitConfigFileParser::Context & ctx, const char *);
static bool checkDbConstraints(InitConfigFileParser::Context & ctx, const char *);
static bool checkConnectionConstraints(InitConfigFileParser::Context &, const char *);
static bool checkTCPConstraints(InitConfigFileParser::Context &, const char *);
static bool fixNodeHostname(InitConfigFileParser::Context & ctx, const char * data);
static bool fixHostname(InitConfigFileParser::Context & ctx, const char * data);
static bool fixNodeId(InitConfigFileParser::Context & ctx, const char * data);
static bool fixExtConnection(InitConfigFileParser::Context & ctx, const char * data);
static bool fixDepricated(InitConfigFileParser::Context & ctx, const char *);
static bool saveInConfigValues(InitConfigFileParser::Context & ctx, const char *);

const ConfigInfo::SectionRule 
ConfigInfo::m_SectionRules[] = {
  { "SYSTEM", transformSystem, 0 },
  { "EXTERNAL SYSTEM", transformExternalSystem, 0 },
  { "COMPUTER", transformComputer, 0 },

  { "DB",   transformNode, 0 },
  { "API",  transformNode, 0 },
  { "MGM",  transformNode, 0 },
  { "REP",  transformNode, 0 },
  { "EXTERNAL REP",  transformExtNode, 0 },

  { "TCP",  transformConnection, 0 },
  { "SHM",  transformConnection, 0 },
  { "SCI",  transformConnection, 0 },
  { "OSE",  transformConnection, 0 },

  { "DB",   fixNodeHostname, 0 },
  { "API",  fixNodeHostname, 0 },
  { "MGM",  fixNodeHostname, 0 },
  { "REP",  fixNodeHostname, 0 },
  //{ "EXTERNAL REP",  fixNodeHostname, 0 },

  { "TCP",  fixNodeId, "NodeId1" },
  { "TCP",  fixNodeId, "NodeId2" },
  { "SHM",  fixNodeId, "NodeId1" },
  { "SHM",  fixNodeId, "NodeId2" },
  { "SCI",  fixNodeId, "NodeId1" },
  { "SCI",  fixNodeId, "NodeId2" },
  { "OSE",  fixNodeId, "NodeId1" },
  { "OSE",  fixNodeId, "NodeId2" },
  
  { "TCP",  fixHostname, "HostName1" },
  { "TCP",  fixHostname, "HostName2" },
  { "OSE",  fixHostname, "HostName1" },
  { "OSE",  fixHostname, "HostName2" },

  { "TCP",  fixPortNumber, 0 }, // has to come after fixHostName
  { "SHM",  fixPortNumber, 0 }, // has to come after fixHostName
  //{ "SHM",  fixShmKey, 0 },

  /**
   * fixExtConnection must be after fixNodeId
   */
  { "TCP",  fixExtConnection, 0 },
  { "SHM",  fixExtConnection, 0 },
  { "SCI",  fixExtConnection, 0 },
  { "OSE",  fixExtConnection, 0 },
  
  { "*",    applyDefaultValues, "user" },
  { "*",    fixDepricated, 0 },
  { "*",    applyDefaultValues, "system" },

  { "DB",   checkDbConstraints, 0 },

  /**
   * checkConnectionConstraints must be after fixExtConnection
   */
  { "TCP",  checkConnectionConstraints, 0 },
  { "SHM",  checkConnectionConstraints, 0 },
  { "SCI",  checkConnectionConstraints, 0 },
  { "OSE",  checkConnectionConstraints, 0 },

  { "TCP",  checkTCPConstraints, "HostName1" },
  { "TCP",  checkTCPConstraints, "HostName2" },
  
  { "*",    checkMandatory, 0 },
  
  { "DB",   saveInConfigValues, 0 },
  { "API",  saveInConfigValues, 0 },
  { "MGM",  saveInConfigValues, 0 },
  { "REP",  saveInConfigValues, 0 },

  { "TCP",  saveInConfigValues, 0 },
  { "SHM",  saveInConfigValues, 0 },
  { "SCI",  saveInConfigValues, 0 },
  { "OSE",  saveInConfigValues, 0 }
};
const int ConfigInfo::m_NoOfRules = sizeof(m_SectionRules)/sizeof(SectionRule);

/****************************************************************************
 * Config Rules declarations
 ****************************************************************************/
static bool sanity_checks(Vector<ConfigInfo::ConfigRuleSection>&sections, 
			  struct InitConfigFileParser::Context &ctx, 
			  const char * rule_data);
static bool add_node_connections(Vector<ConfigInfo::ConfigRuleSection>&sections, 
				 struct InitConfigFileParser::Context &ctx, 
				 const char * rule_data);
static bool add_server_ports(Vector<ConfigInfo::ConfigRuleSection>&sections, 
			     struct InitConfigFileParser::Context &ctx, 
			     const char * rule_data);
static bool check_node_vs_replicas(Vector<ConfigInfo::ConfigRuleSection>&sections, 
				   struct InitConfigFileParser::Context &ctx, 
				   const char * rule_data);

const ConfigInfo::ConfigRule 
ConfigInfo::m_ConfigRules[] = {
  { sanity_checks, 0 },
  { add_node_connections, 0 },
  { add_server_ports, 0 },
  { check_node_vs_replicas, 0 },
  { 0, 0 }
};
	  
struct DepricationTransform {
  const char * m_section;
  const char * m_oldName;
  const char * m_newName;
  double m_add;
  double m_mul;
};

static
const DepricationTransform f_deprication[] = {
  { "DB", "Discless", "Diskless", 0, 1 },
  { 0, 0, 0, 0, 0}
};

/**
 * The default constructors create objects with suitable values for the
 * configuration parameters. 
 *
 * Some are however given the value MANDATORY which means that the value
 * must be specified in the configuration file. 
 *
 * Min and max values are also given for some parameters.
 * - Attr1:  Name in file (initial config file)
 * - Attr2:  Name in prop (properties object)
 * - Attr3:  Name of Section (in init config file)
 * - Attr4:  Updateable
 * - Attr5:  Type of parameter (INT or BOOL)
 * - Attr6:  Default Value (number only)
 * - Attr7:  Min value
 * - Attr8:  Max value
 * 
 * Parameter constraints are coded in file Config.cpp.
 *
 * *******************************************************************
 * Parameters used under development should be marked "NOTIMPLEMENTED"
 * *******************************************************************
 */
const ConfigInfo::ParamInfo ConfigInfo::m_ParamInfo[] = {

  /****************************************************************************
   * COMPUTER
   ***************************************************************************/
  {
    KEY_INTERNAL,
    "COMPUTER",
    "COMPUTER",
    "Computer section",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::SECTION,
    0,
    0, 0 },
  
  {
    KEY_INTERNAL,
    "Id",
    "COMPUTER",
    "Name of computer",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    MANDATORY,
    0,
    0 },

  {
    KEY_INTERNAL,
    "HostName",
    "COMPUTER",
    "Hostname of computer (e.g. mysql.com)",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    MANDATORY,
    0,
    MAX_INT_RNIL },
  
  /****************************************************************************
   * SYSTEM
   ***************************************************************************/
  {
    CFG_SECTION_SYSTEM,
    "SYSTEM",
    "SYSTEM",
    "System section",
    ConfigInfo::USED,
    false,
    ConfigInfo::SECTION,
    CFG_SECTION_SYSTEM,
    0,
    0 },

  {
    CFG_SYS_NAME,
    "Name",
    "SYSTEM",
    "Name of system (NDB Cluster)",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    MANDATORY,
    0,
    0 },
  
  {
    CFG_SYS_REPLICATION_ROLE,
    "ReplicationRole",
    "SYSTEM",
    "Role in Global Replication (None, Primary, or Standby)",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    0 },
  
  {
    CFG_SYS_PRIMARY_MGM_NODE,
    "PrimaryMGMNode",
    "SYSTEM",
    "Node id of Primary MGM node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    MAX_INT_RNIL },

  {
    CFG_SYS_CONFIG_GENERATION,
    "ConfigGenerationNumber",
    "SYSTEM",
    "Configuration generation number",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    MAX_INT_RNIL },

  /***************************************************************************
   * DB
   ***************************************************************************/
  {
    CFG_SECTION_NODE,
    "DB",
    "DB",
    "Node section",
    ConfigInfo::USED,
    false,
    ConfigInfo::SECTION,
    NODE_TYPE_DB, 
    0, 0 
  },

  {
    CFG_NODE_HOST,
    "HostName",
    "DB",
    "Name of computer for this node",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_SYSTEM,
    "System",
    "DB",
    "Name of system for this node",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_ID,
    "Id",
    "DB",
    "Number identifying the database node (DB)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    1,
    (MAX_NODES - 1) },

  {
    KEY_INTERNAL,
    "ServerPort",
    "DB",
    "Port used to setup transporter",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    UNDEFINED,
    1,
    65535 },

  {
    CFG_DB_NO_REPLICAS,
    "NoOfReplicas",
    "DB",
    "Number of copies of all data in the database (1-4)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    1,
    4 },

  {
    CFG_DB_NO_ATTRIBUTES,
    "MaxNoOfAttributes",
    "DB",
    "Total number of attributes stored in database. I.e. sum over all tables",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    1000,
    32,
    MAX_INT_RNIL/16 },
  
  {
    CFG_DB_NO_TABLES,
    "MaxNoOfTables",
    "DB",
    "Total number of tables stored in the database",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    128,
    8,
    MAX_INT_RNIL },
  
  {
    CFG_DB_NO_INDEXES,
    "MaxNoOfIndexes",
    "DB",
    "Total number of indexes that can be defined in the system",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    128,
    0,
    MAX_INT_RNIL },

  {
    CFG_DB_NO_INDEX_OPS,
    "MaxNoOfConcurrentIndexOperations",
    "DB",
    "Total number of index operations that can execute simultaneously on one DB node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    8192,
    0,
    MAX_INT_RNIL 
  },

  {
    CFG_DB_NO_TRIGGERS,
    "MaxNoOfTriggers",
    "DB",
    "Total number of triggers that can be defined in the system",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    768,
    0,
    MAX_INT_RNIL },

  {
    CFG_DB_NO_TRIGGER_OPS,
    "MaxNoOfFiredTriggers",
    "DB",
    "Total number of triggers that can fire simultaneously in one DB node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    4000,
    0,
    MAX_INT_RNIL },

  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    "DB",
    "String referencing an earlier defined COMPUTER",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    MANDATORY,
    0,
    MAX_INT_RNIL },
  
  {
    CFG_DB_NO_SAVE_MSGS,
    "MaxNoOfSavedMessages",
    "DB",
    "Max number of error messages in error log and max number of trace files",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    25,
    0,
    MAX_INT_RNIL },

  {
    CFG_DB_MEMLOCK,
    "LockPagesInMainMemory",
    "DB",
    "If set to yes, then NDB Cluster data will not be swapped out to disk",
    ConfigInfo::USED,
    true,
    ConfigInfo::BOOL,
    false,
    0,
    MAX_INT_RNIL },

  {
    CFG_DB_WATCHDOG_INTERVAL,
    "TimeBetweenWatchDogCheck",
    "DB",
    "Time between execution checks inside a database node",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    6000,
    70,
    MAX_INT_RNIL },

  {
    CFG_DB_STOP_ON_ERROR,
    "StopOnError",
    "DB",
    "If set to N, the DB automatically restarts/recovers in case of node failure",
    ConfigInfo::USED,
    true,
    ConfigInfo::BOOL,
    true,
    0,
    MAX_INT_RNIL },

  { 
    CFG_DB_STOP_ON_ERROR_INSERT,
    "RestartOnErrorInsert",
    "DB",
    "See src/kernel/vm/Emulator.hpp NdbRestartType for details",
    ConfigInfo::INTERNAL,
    true,
    ConfigInfo::INT,
    2,
    0,
    4 },
  
  {
    CFG_DB_NO_OPS,
    "MaxNoOfConcurrentOperations",
    "DB",
    "Max no of op:s on DB (op:s within a transaction are concurrently executed)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    32768,
    32,
    MAX_INT_RNIL },

  {
    CFG_DB_NO_TRANSACTIONS,
    "MaxNoOfConcurrentTransactions",
    "DB",
    "Max number of transaction executing concurrently on the DB node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    4096,
    32,
    MAX_INT_RNIL },

  {
    CFG_DB_NO_SCANS,
    "MaxNoOfConcurrentScans",
    "DB",
    "Max number of scans executing concurrently on the DB node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    256,
    2,
    500 },

  {
    CFG_DB_TRANS_BUFFER_MEM,
    "TransactionBufferMemory",
    "DB",
    "Dynamic buffer space (in bytes) for key and attribute data allocated for each DB node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    (1024 * 1024),
    1024,
    MAX_INT_RNIL },
 
  {
    CFG_DB_INDEX_MEM,
    "IndexMemory",
    "DB",
    "Number bytes on each DB node allocated for storing indexes",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT64,
    3 * 1024 * 8192,
    128 * 8192,
    ((Uint64)MAX_INT_RNIL) * ((Uint64)8192) },

  {
    CFG_DB_DATA_MEM,
    "DataMemory",
    "DB",
    "Number bytes on each DB node allocated for storing data",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT64,
    10 * 1024 * 8192,
    128 * 8192,
    ((Uint64)MAX_INT_RNIL) * ((Uint64)8192) },

  {
    CFG_DB_START_PARTIAL_TIMEOUT,
    "StartPartialTimeout",
    "DB",
    "Time to wait before trying to start wo/ all nodes. 0=Wait forever",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    30000,
    0,
    ~0 },

  {
    CFG_DB_START_PARTITION_TIMEOUT,
    "StartPartitionedTimeout",
    "DB",
    "Time to wait before trying to start partitioned. 0=Wait forever",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    60000,
    0,
    ~0 },
  
  {
    CFG_DB_START_FAILURE_TIMEOUT,
    "StartFailureTimeout",
    "DB",
    "Time to wait before terminating. 0=Wait forever",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    0,
    0,
    ~0 },
  
  {
    CFG_DB_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalDbDb",
    "DB",
    "Time between DB-DB heartbeats. DB considered dead after 3 missed HBs",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    1500,
    10,
    MAX_INT_RNIL },

  {
    CFG_DB_API_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalDbApi",
    "DB",
    "Time between API-DB heartbeats. API connection closed after 3 missed HBs",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    1500,
    100,
    MAX_INT_RNIL },

  {
    CFG_DB_LCP_INTERVAL,
    "TimeBetweenLocalCheckpoints",
    "DB",
    "Time between taking snapshots of the database (expressed in 2log of bytes)",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    20,
    0,
    31 },

  {
    CFG_DB_GCP_INTERVAL,
    "TimeBetweenGlobalCheckpoints",
    "DB",
    "Time between doing group commit of transactions to disk",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    2000,
    10,
    32000 },

  {
    CFG_DB_NO_REDOLOG_FILES,
    "NoOfFragmentLogFiles",
    "DB",
    "No of 16 Mbyte Redo log files in each of 4 file sets belonging to DB node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    8,
    1,
    MAX_INT_RNIL },

  {
    KEY_INTERNAL,
    "MaxNoOfOpenFiles",
    "DB",
    "Max number of files open per DB node.(One thread is created per file)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    40,
    20,
    256 },

  
  {
    CFG_DB_TRANSACTION_CHECK_INTERVAL,
    "TimeBetweenInactiveTransactionAbortCheck",
    "DB",
    "Time between inactive transaction checks",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    1000,
    1000,
    MAX_INT_RNIL },
  
  {
    CFG_DB_TRANSACTION_INACTIVE_TIMEOUT,
    "TransactionInactiveTimeout",
    "DB",
    "Time application can wait before executing another transaction part (ms).\n"
    "This is the time the transaction coordinator waits for the application\n"
    "to execute or send another part (query, statement) of the transaction.\n"
    "If the application takes too long time, the transaction gets aborted.\n"
    "Timeout set to 0 means that we don't timeout at all on application wait.",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    MAX_INT_RNIL,
    0,
    MAX_INT_RNIL },

  {
    CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT,
    "TransactionDeadlockDetectionTimeout",
    "DB",
    "Time transaction can be executing in a DB node (ms).\n"
    "This is the time the transaction coordinator waits for each database node\n"
    "of the transaction to execute a request. If the database node takes too\n"
    "long time, the transaction gets aborted.",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    1200,
    50,
    MAX_INT_RNIL },

  {
    KEY_INTERNAL,
    "NoOfDiskPagesToDiskDuringRestartTUP",
    "DB",
    "?",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    40,
    1,
    MAX_INT_RNIL },

  {
    KEY_INTERNAL,
    "NoOfDiskPagesToDiskAfterRestartTUP",
    "DB",
    "?",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    40,
    1,
    MAX_INT_RNIL },

  {
    KEY_INTERNAL,
    "NoOfDiskPagesToDiskDuringRestartACC",
    "DB",
    "?",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    20,
    1,
    MAX_INT_RNIL },

  {
    KEY_INTERNAL,
    "NoOfDiskPagesToDiskAfterRestartACC",
    "DB",
    "?",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    20,
    1,
    MAX_INT_RNIL },
  

  {
    CFG_DB_DISCLESS,
    "Diskless",
    "DB",
    "Run wo/ disk",
    ConfigInfo::USED,
    true,
    ConfigInfo::BOOL,
    0,
    0,
    1},

  {
    KEY_INTERNAL,
    "Discless",
    "DB",
    "Diskless",
    ConfigInfo::DEPRICATED,
    true,
    ConfigInfo::BOOL,
    0,
    0,
    1},
  

  
  {
    CFG_DB_ARBIT_TIMEOUT,
    "ArbitrationTimeout",
    "DB",
    "Max time (milliseconds) database partion waits for arbitration signal",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    3000,
    10,
    MAX_INT_RNIL },

  {
    CFG_DB_FILESYSTEM_PATH,
    "FileSystemPath",
    "DB",
    "Path to directory where the DB node stores its data (directory must exist)",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_LOGLEVEL_STARTUP,
    "LogLevelStartup",
    "DB",
    "Node startup info printed on stdout",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    1,
    0,
    15 },
  
  {
    CFG_LOGLEVEL_SHUTDOWN,
    "LogLevelShutdown",
    "DB",
    "Node shutdown info printed on stdout",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    15 },

  {
    CFG_LOGLEVEL_STATISTICS,
    "LogLevelStatistic",
    "DB",
    "Transaction, operation, transporter info printed on stdout",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    15 },

  {
    CFG_LOGLEVEL_CHECKPOINT,
    "LogLevelCheckpoint",
    "DB",
    "Local and Global checkpoint info printed on stdout",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    15 },

  {
    CFG_LOGLEVEL_NODERESTART,
    "LogLevelNodeRestart",
    "DB",
    "Node restart, node failure info printed on stdout",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    15 },

  {
    CFG_LOGLEVEL_CONNECTION,
    "LogLevelConnection",
    "DB",
    "Node connect/disconnect info printed on stdout",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    15 },

  {
    CFG_LOGLEVEL_ERROR,
    "LogLevelError",
    "DB",
    "Transporter, heartbeat errors printed on stdout",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    15 },

  {
    CFG_LOGLEVEL_INFO,
    "LogLevelInfo",
    "DB",
    "Heartbeat and log info printed on stdout",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    15 },

  /**
   * Backup
   */
  { 
    CFG_DB_PARALLEL_BACKUPS,
    "ParallelBackups",
    "DB",
    "Maximum number of parallel backups",
    ConfigInfo::NOTIMPLEMENTED,
    false,
    ConfigInfo::INT,
    1,
    1,
    1 },
  
  { 
    CFG_DB_BACKUP_MEM,
    "BackupMemory",
    "DB",
    "Total memory allocated for backups per node (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    (2 * 1024 * 1024) + (2 * 1024 * 1024), // sum of BackupDataBufferSize and BackupLogBufferSize
    0,
    MAX_INT_RNIL },
  
  { 
    CFG_DB_BACKUP_DATA_BUFFER_MEM,
    "BackupDataBufferSize",
    "DB",
    "Default size of databuffer for a backup (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    (2 * 1024 * 1024), // remember to change BackupMemory
    0,
    MAX_INT_RNIL },

  { 
    CFG_DB_BACKUP_LOG_BUFFER_MEM,
    "BackupLogBufferSize",
    "DB",
    "Default size of logbuffer for a backup (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    (2 * 1024 * 1024), // remember to change BackupMemory
    0,
    MAX_INT_RNIL },

  { 
    CFG_DB_BACKUP_WRITE_SIZE,
    "BackupWriteSize",
    "DB",
    "Default size of filesystem writes made by backup (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    32768,
    0,
    MAX_INT_RNIL },

  /***************************************************************************
   * REP
   ***************************************************************************/
  {
    CFG_SECTION_NODE,
    "REP",
    "REP",
    "Node section",
    ConfigInfo::USED,
    false,
    ConfigInfo::SECTION,
    NODE_TYPE_REP, 
    0, 0 
  },

  {
    CFG_NODE_HOST,
    "HostName",
    "REP",
    "Name of computer for this node",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_SYSTEM,
    "System",
    "REP",
    "Name of system for this node",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_ID,
    "Id",
    "REP",
    "Number identifying replication node (REP)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    1,
    (MAX_NODES - 1) },

  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    "REP",
    "String referencing an earlier defined COMPUTER",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_REP_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalRepRep",
    "REP",
    "Time between REP-REP heartbeats. Connection closed after 3 missed HBs",
    ConfigInfo::USED,
    true,
    ConfigInfo::INT,
    3000,
    100,
    MAX_INT_RNIL },

  /***************************************************************************
   * API
   ***************************************************************************/
  {
    CFG_SECTION_NODE,
    "API",
    "API",
    "Node section",
    ConfigInfo::USED,
    false,
    ConfigInfo::SECTION,
    NODE_TYPE_API, 
    0, 0 
  },

  {
    CFG_NODE_HOST,
    "HostName",
    "API",
    "Name of computer for this node",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_SYSTEM,
    "System",
    "API",
    "Name of system for this node",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_ID,
    "Id",
    "API",
    "Number identifying application node (API)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    1,
    (MAX_NODES - 1) },

  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    "API",
    "String referencing an earlier defined COMPUTER",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    0,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_ARBIT_RANK,
    "ArbitrationRank",
    "API",
    "If 0, then API is not arbitrator. Kernel selects arbitrators in order 1, 2",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    2 },

  {
    CFG_NODE_ARBIT_DELAY,
    "ArbitrationDelay",
    "API",
    "When asked to arbitrate, arbitrator waits this long before voting (msec)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    MAX_INT_RNIL },

  /****************************************************************************
   * MGM
   ***************************************************************************/
  {
    CFG_SECTION_NODE,
    "MGM",
    "MGM",
    "Node section",
    ConfigInfo::USED,
    false,
    ConfigInfo::SECTION,
    NODE_TYPE_MGM, 
    0, 0 
  },

  {
    CFG_NODE_HOST,
    "HostName",
    "MGM",
    "Name of computer for this node",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_SYSTEM,
    "System",
    "MGM",
    "Name of system for this node",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_ID,
    "Id",
    "MGM",
    "Number identifying the management server node (MGM)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    1,
    (MAX_NODES - 1) },
  
  {
    CFG_LOG_DESTINATION,
    "LogDestination",
    "MGM",
    "String describing where logmessages are sent",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    0,
    0,
    MAX_INT_RNIL },
  
  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    "MGM",
    "String referencing an earlier defined COMPUTER",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    0,
    0,
    MAX_INT_RNIL },
  
  {
    KEY_INTERNAL,
    "MaxNoOfSavedEvents",
    "MGM",
    "",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    100,
    0,
    MAX_INT_RNIL },

  {
    CFG_MGM_PORT,
    "PortNumber",
    "MGM",
    "Port number to give commands to/fetch configurations from management server",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    NDB_BASE_PORT,
    0,
    MAX_INT_RNIL },

  {
    KEY_INTERNAL,
    "PortNumberStats",
    "MGM",
    "Port number used to get statistical information from a management server",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    2199,
    0,
    MAX_INT_RNIL },

  {
    CFG_NODE_ARBIT_RANK,
    "ArbitrationRank",
    "MGM",
    "If 0, then MGM is not arbitrator. Kernel selects arbitrators in order 1, 2",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    1,
    0,
    2 },

  {
    CFG_NODE_ARBIT_DELAY,
    "ArbitrationDelay",
    "MGM",
    "",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    0,
    0,
    MAX_INT_RNIL },

  /****************************************************************************
   * TCP
   ***************************************************************************/
  {
    CFG_SECTION_CONNECTION,
    "TCP",
    "TCP",
    "Connection section",
    ConfigInfo::USED,
    false,
    ConfigInfo::SECTION,
    CONNECTION_TYPE_TCP, 
    0, 0 
  },

  {
    CFG_TCP_HOSTNAME_1,
    "HostName1",
    "TCP",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_TCP_HOSTNAME_2,
    "HostName2",
    "TCP",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "TCP",
    "Id of node (DB, API or MGM) on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "TCP",
    "Id of node (DB, API or MGM) on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "TCP",
    "Sends id in each signal.  Used in trace files.",
    ConfigInfo::USED,
    false,
    ConfigInfo::BOOL,
    true,
    0,
    MAX_INT_RNIL },


  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "TCP",
    "If checksum is enabled, all signals between nodes are checked for errors",
    ConfigInfo::USED,
    false,
    ConfigInfo::BOOL,
    false,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "TCP",
    "Port used for this transporter",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_TCP_SEND_BUFFER_SIZE,
    "SendBufferMemory",
    "TCP",
    "Bytes of buffer for signals sent from this node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    16 * 16384,
    1 * 16384,
    MAX_INT_RNIL },

  {
    CFG_TCP_RECEIVE_BUFFER_SIZE,
    "ReceiveBufferMemory",
    "TCP",
    "Bytes of buffer for signals received by this node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    4 * 16384,
    1 * 16384,
    MAX_INT_RNIL },

  {
    CFG_TCP_PROXY,
    "Proxy",
    "TCP",
    "",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    0 },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "TCP",
    "System for node 1 in connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "TCP",
    "System for node 2 in connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },
  

  /****************************************************************************
   * SHM
   ***************************************************************************/
  {
    CFG_SECTION_CONNECTION,
    "SHM",
    "SHM",
    "Connection section",
    ConfigInfo::USED,
    false,
    ConfigInfo::SECTION,
    CONNECTION_TYPE_SHM, 
    0, 0 
  },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "SHM",
    "Id of node (DB, API or MGM) on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    MANDATORY,
    0,
    MAX_INT_RNIL },
  
  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "SHM",
    "Port used for this transporter",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "SHM",
    "Id of node (DB, API or MGM) on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    MANDATORY,
    0,
    MAX_INT_RNIL },
  
  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "SHM",
    "Sends id in each signal.  Used in trace files.",
    ConfigInfo::USED,
    false,
    ConfigInfo::BOOL,
    false,
    0,
    MAX_INT_RNIL },
  
  
  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "SHM",
    "If checksum is enabled, all signals between nodes are checked for errors",
    ConfigInfo::USED,
    false,
    ConfigInfo::BOOL,
    true,
    0,
    MAX_INT_RNIL },
  
  {
    CFG_SHM_KEY,
    "ShmKey",
    "SHM",
    "A shared memory key",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    0,
    MAX_INT_RNIL },
  
  {
    CFG_SHM_BUFFER_MEM,
    "ShmSize",
    "SHM",
    "Size of shared memory segment",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    1048576,
    4096,
    MAX_INT_RNIL },
  
  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "SHM",
    "System for node 1 in connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "SHM",
    "System for node 2 in connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  /****************************************************************************
   * SCI
   ***************************************************************************/
  {
    CFG_SECTION_CONNECTION,
    "SCI",
    "SCI",
    "Connection section",
    ConfigInfo::USED,
    false,
    ConfigInfo::SECTION,
    CONNECTION_TYPE_SCI, 
    0, 0 
  },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "SCI",
    "Id of node (DB, API or MGM) on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "SCI",
    "Id of node (DB, API or MGM) on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_SCI_ID_0,
    "SciId0",
    "SCI",
    "Local SCI-node id for adapter 0 (a computer can have two adapters)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_SCI_ID_1,
    "SciId1",
    "SCI",
    "Local SCI-node id for adapter 1 (a computer can have two adapters)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "SCI",
    "Sends id in each signal.  Used in trace files.",
    ConfigInfo::USED,
    false,
    ConfigInfo::BOOL,
    true,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "SCI",
    "If checksum is enabled, all signals between nodes are checked for errors",
    ConfigInfo::USED,
    false,
    ConfigInfo::BOOL,
    false,
    0,
    MAX_INT_RNIL },

  {
    CFG_SCI_SEND_LIMIT,
    "SendLimit",
    "SCI",
    "Transporter send buffer contents are sent when this no of bytes is buffered",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    2048,
    512,
    MAX_INT_RNIL },

  {
    CFG_SCI_BUFFER_MEM,
    "SharedBufferSize",
    "SCI",
    "Size of shared memory segment",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    1048576,
    262144,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "SCI",
    "System for node 1 in connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "SCI",
    "System for node 2 in connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  /****************************************************************************
   * OSE
   ***************************************************************************/
  {
    CFG_SECTION_CONNECTION,
    "OSE",
    "OSE",
    "Connection section",
    ConfigInfo::USED,
    false,
    ConfigInfo::SECTION,
    CONNECTION_TYPE_OSE, 
    0, 0 
  },

  {
    CFG_OSE_HOSTNAME_1,
    "HostName1",
    "OSE",
    "Name of computer on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_OSE_HOSTNAME_2,
    "HostName2",
    "OSE",
    "Name of computer on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "OSE",
    "Id of node (DB, API or MGM) on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    MANDATORY,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "OSE",
    "Id of node (DB, API or MGM) on one side of the connection",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    UNDEFINED,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "OSE",
    "Sends id in each signal.  Used in trace files.",
    ConfigInfo::USED,
    false,
    ConfigInfo::BOOL,
    true,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "OSE",
    "If checksum is enabled, all signals between nodes are checked for errors",
    ConfigInfo::USED,
    false,
    ConfigInfo::BOOL,
    false,
    0,
    MAX_INT_RNIL },

  {
    CFG_OSE_PRIO_A_SIZE,
    "PrioASignalSize",
    "OSE",
    "Size of priority A signals (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    1000,
    0,
    MAX_INT_RNIL },

  {
    CFG_OSE_PRIO_B_SIZE,
    "PrioBSignalSize",
    "OSE",
    "Size of priority B signals (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    1000,
    0,
    MAX_INT_RNIL },
  
  {
    CFG_OSE_RECEIVE_ARRAY_SIZE,
    "ReceiveArraySize",
    "OSE",
    "Number of OSE signals checked for correct ordering (in no of OSE signals)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    10,
    0,
    MAX_INT_RNIL },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "OSE",
    "System for node 1 in connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL},

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "OSE",
    "System for node 2 in connection",
    ConfigInfo::INTERNAL,
    false,
    ConfigInfo::STRING,
    UNDEFINED,
    0,
    MAX_INT_RNIL },
};

const int ConfigInfo::m_NoOfParams = sizeof(m_ParamInfo) / sizeof(ParamInfo);


/****************************************************************************
 * Ctor
 ****************************************************************************/
static void require(bool v) { if(!v) abort();}

ConfigInfo::ConfigInfo()
  : m_info(true), m_systemDefaults(true)
{
  int i;
  Properties *section;
  const Properties *oldpinfo;

  for (i=0; i<m_NoOfParams; i++) {
    const ParamInfo & param = m_ParamInfo[i];
    
    // Create new section if it did not exist
    if (!m_info.getCopy(param._section, &section)) {
      Properties newsection(true);
      m_info.put(param._section, &newsection);
    }
    
    // Get copy of section
    m_info.getCopy(param._section, &section);
    
    // Create pinfo (parameter info) entry 
    Properties pinfo(true); 
    pinfo.put("Id",          param._paramId);
    pinfo.put("Fname",       param._fname);
    pinfo.put("Description", param._description);
    pinfo.put("Updateable",  param._updateable);
    pinfo.put("Type",        param._type);
    pinfo.put("Status",      param._status);
    pinfo.put64("Default",     param._default);
    pinfo.put64("Min",         param._min);
    pinfo.put64("Max",         param._max);

    // Check that pinfo is really new
    if (section->get(param._fname, &oldpinfo)) {
      ndbout << "Error: Parameter " << param._fname
	     << " defined twice in section " << param._section
	     << "." << endl;
      exit(-1);
    }
    
    // Add new pinfo to section
    section->put(param._fname, &pinfo);

    // Replace section with modified section
    m_info.put(param._section, section, true);
    
    if(param._type != ConfigInfo::SECTION){
      Properties * p;
      if(!m_systemDefaults.getCopy(param._section, &p)){
	p = new Properties(true);
      }
      if(param._type != STRING && 
	 param._default != UNDEFINED &&
	 param._default != MANDATORY){
	require(p->put(param._fname, param._default));
      }
      require(m_systemDefaults.put(param._section, p, true));
      delete p;
    }
  }
  
  for (i=0; i<m_NoOfParams; i++) {
    if(m_ParamInfo[i]._section == NULL){
      ndbout << "Check that each entry has a section failed." << endl;
      ndbout << "Parameter \"" << m_ParamInfo[i]._fname << endl; 
      ndbout << "Edit file " << __FILE__ << "." << endl;
      exit(-1);
    }
    
    if(m_ParamInfo[i]._type == ConfigInfo::SECTION)
      continue;

    const Properties * p = getInfo(m_ParamInfo[i]._section);
    if (!p || !p->contains(m_ParamInfo[i]._fname)) {
      ndbout << "Check that each pname has an fname failed." << endl;
      ndbout << "Parameter \"" << m_ParamInfo[i]._fname 
	     << "\" does not exist in section \"" 
	     << m_ParamInfo[i]._section << "\"." << endl;
      ndbout << "Edit file " << __FILE__ << "." << endl;
      exit(-1);
    }
  }
}

/****************************************************************************
 * Getters
 ****************************************************************************/
inline void warning(const char * src, const char * arg){
  ndbout << "Illegal call to ConfigInfo::" << src << "() - " << arg << endl;
  abort();
}

const Properties * 
ConfigInfo::getInfo(const char * section) const {
  const Properties * p;
  if(!m_info.get(section, &p)){
    warning("getInfo", section);
  }
  return p;
}

const Properties * 
ConfigInfo::getDefaults(const char * section) const {
  const Properties * p;
  if(!m_systemDefaults.get(section, &p)){
    warning("getDefaults", section);
  }
  return p;
}

static
Uint64
getInfoInt(const Properties * section, 
	   const char* fname, const char * type){
  Uint32 val32;
  const Properties * p;
  if (section->get(fname, &p) && p->get(type, &val32)) {
    return val32;
  }

  Uint64 val64;
  if(p && p->get(type, &val64)){
    return val64;
  }
  
  section->print();
  if(section->get(fname, &p)){
    p->print();
  }

  warning(type, fname);
  return 0;
}

static
const char *
getInfoString(const Properties * section, 
	      const char* fname, const char * type){
  const char* val;
  const Properties * p;
  if (section->get(fname, &p) && p->get(type, &val)) {
    return val;
  }
  warning(type, fname);
  return val;
}

Uint64
ConfigInfo::getMax(const Properties * section, const char* fname) const {
  return getInfoInt(section, fname, "Max");
}

Uint64
ConfigInfo::getMin(const Properties * section, const char* fname) const {
  return getInfoInt(section, fname, "Min");
}

Uint64
ConfigInfo::getDefault(const Properties * section, const char* fname) const {
  return getInfoInt(section, fname, "Default");
}

const char*
ConfigInfo::getDescription(const Properties * section, 
			   const char* fname) const {
  return getInfoString(section, fname, "Description");
}

bool
ConfigInfo::isSection(const char * section) const {
  for (int i = 0; i<m_noOfSectionNames; i++) {
    if(!strcmp(section, m_sectionNames[i])) return true;
  }
  return false;
}

const char*
ConfigInfo::getAlias(const char * section) const {
  for (int i = 0; m_sectionNameAliases[i].name != 0; i++)
    if(!strcmp(section, m_sectionNameAliases[i].alias))
      return m_sectionNameAliases[i].name;
  return 0;
}

bool
ConfigInfo::verify(const Properties * section, const char* fname, 
		   Uint64 value) const {
  Uint64 min, max; min = max + 1;

  min = getInfoInt(section, fname, "Min");
  max = getInfoInt(section, fname, "Max");
  if(min > max){
    warning("verify", fname);
  }
  if (value >= min && value <= max)
    return true;
  else 
    return false;
}

ConfigInfo::Type 
ConfigInfo::getType(const Properties * section, const char* fname) const {
  return (ConfigInfo::Type) getInfoInt(section, fname, "Type");
}

ConfigInfo::Status
ConfigInfo::getStatus(const Properties * section, const char* fname) const {
  return (ConfigInfo::Status) getInfoInt(section, fname, "Status");
}

/****************************************************************************
 * Printers
 ****************************************************************************/

void ConfigInfo::print() const {
  Properties::Iterator it(&m_info);
  for (const char* n = it.first(); n != NULL; n = it.next()) {
    print(n);
  }
}

void ConfigInfo::print(const char* section) const {
  ndbout << "****** " << section << " ******" << endl << endl;
  const Properties * sec = getInfo(section);
  Properties::Iterator it(sec);
  for (const char* n = it.first(); n != NULL; n = it.next()) {
    // Skip entries with different F- and P-names
    if (getStatus(sec, n) == ConfigInfo::INTERNAL) continue;
    if (getStatus(sec, n) == ConfigInfo::DEPRICATED) continue;
    if (getStatus(sec, n) == ConfigInfo::NOTIMPLEMENTED) continue;
    print(sec, n);
  }
}

void ConfigInfo::print(const Properties * section, 
		       const char* parameter) const {
  ndbout << parameter;
  //  ndbout << getDescription(section, parameter) << endl;
  switch (getType(section, parameter)) {
  case ConfigInfo::BOOL:
    ndbout << " (Boolean value)" << endl;
    ndbout << getDescription(section, parameter) << endl;
    if (getDefault(section, parameter) == false) {
      ndbout << "Default: N (Legal values: Y, N)" << endl; 
    } else if (getDefault(section, parameter) == true) {
      ndbout << "Default: Y (Legal values: Y, N)" << endl;
    } else if (getDefault(section, parameter) == MANDATORY) {
      ndbout << "MANDATORY (Legal values: Y, N)" << endl;
    } else {
      ndbout << "UNKNOWN" << endl;
    }
    ndbout << endl;
    break;    
    
  case ConfigInfo::INT:
  case ConfigInfo::INT64:
    ndbout << " (Non-negative Integer)" << endl;
    ndbout << getDescription(section, parameter) << endl;
    if (getDefault(section, parameter) == MANDATORY) {
      ndbout << "MANDATORY (";
    } else if (getDefault(section, parameter) == UNDEFINED) {
      ndbout << "UNDEFINED (";
    } else {
      ndbout << "Default: " << getDefault(section, parameter) << " (";
    }
    ndbout << "Min: " << getMin(section, parameter) << ", ";
    ndbout << "Max: " << getMax(section, parameter) << ")" << endl;
    ndbout << endl;
    break;
    
  case ConfigInfo::STRING:
    ndbout << " (String)" << endl;
    ndbout << getDescription(section, parameter) << endl;
    if (getDefault(section, parameter) == MANDATORY) {
      ndbout << "MANDATORY" << endl;
    } else {
      ndbout << "No default value" << endl;
    }
    ndbout << endl;
    break;
  case ConfigInfo::SECTION:
    break;
  }
}

/****************************************************************************
 * Section Rules
 ****************************************************************************/

/**
 * Node rule: Add "Type" and update "NoOfNodes"
 */
bool
transformNode(InitConfigFileParser::Context & ctx, const char * data){

  Uint32 id;
  if(!ctx.m_currentSection->get("Id", &id)){
    Uint32 nextNodeId= 1;
    ctx.m_userProperties.get("NextNodeId", &nextNodeId);
    id= nextNodeId;
    while (ctx.m_userProperties.get("AllocatedNodeId_", id, &id))
      id++;
    ctx.m_userProperties.put("NextNodeId", id+1, true);
    ctx.m_currentSection->put("Id", id);
#if 0
    ctx.reportError("Mandatory parameter Id missing from section "
		    "[%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
#endif
  } else if(ctx.m_userProperties.get("AllocatedNodeId_", id, &id)) {
    ctx.reportError("Duplicate Id in section "
		    "[%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  ctx.m_userProperties.put("AllocatedNodeId_", id, id);
  snprintf(ctx.pname, sizeof(ctx.pname), "Node_%d", id);
  
  ctx.m_currentSection->put("Type", ctx.fname);

  Uint32 nodes = 0;
  ctx.m_userProperties.get("NoOfNodes", &nodes);
  ctx.m_userProperties.put("NoOfNodes", ++nodes, true);

  /**
   * Update count (per type)
   */
  nodes = 0;
  ctx.m_userProperties.get(ctx.fname, &nodes);
  ctx.m_userProperties.put(ctx.fname, ++nodes, true);

  return true;
}

bool
fixNodeHostname(InitConfigFileParser::Context & ctx, const char * data){
  
  const char * compId;
  if(!ctx.m_currentSection->get("ExecuteOnComputer", &compId)){
    require(ctx.m_currentSection->put("HostName", ""));

    const char * type;
    if(ctx.m_currentSection->get("Type", &type) && strcmp(type,"DB") == 0) {
      ctx.reportError("Parameter \"ExecuteOnComputer\" missing from DB section"
		      " [%s] starting at line: %d",
		      ctx.fname, ctx.m_sectionLineno);
      return false;
    }
    return true;
  }
  
  const Properties * computer;
  char tmp[255];
  snprintf(tmp, sizeof(tmp), "Computer_%s", compId);
  if(!ctx.m_config->get(tmp, &computer)){
    ctx.reportError("Computer \"%s\" not declared"
		    "- [%s] starting at line: %d",
		    compId, ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  
  const char * hostname;
  if(!computer->get("HostName", &hostname)){
    ctx.reportError("HostName missing in [COMPUTER] (Id: %d) "
		    " - [%s] starting at line: %d",
		    compId, ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  
  require(ctx.m_currentSection->put("HostName", hostname));
  return true;
}

bool
transformExtNode(InitConfigFileParser::Context & ctx, const char * data){

  Uint32 id;
  const char * systemName;

  if(!ctx.m_currentSection->get("Id", &id)){
    ctx.reportError("Mandatory parameter 'Id' missing from section "
		    "[%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  if(!ctx.m_currentSection->get("System", &systemName)){
    ctx.reportError("Mandatory parameter 'System' missing from section "
		    "[%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  ctx.m_currentSection->put("Type", ctx.fname);

  Uint32 nodes = 0;
  ctx.m_userProperties.get("ExtNoOfNodes", &nodes);
  require(ctx.m_userProperties.put("ExtNoOfNodes",++nodes, true));

  snprintf(ctx.pname, sizeof(ctx.pname), "EXTERNAL SYSTEM_%s:Node_%d", 
	   systemName, id);

  return true;
}

/**
 * Connection rule: Update "NoOfConnections"
 */
bool
transformConnection(InitConfigFileParser::Context & ctx, const char * data){

  Uint32 connections = 0;
  ctx.m_userProperties.get("NoOfConnections", &connections);
  snprintf(ctx.pname, sizeof(ctx.pname), "Connection_%d", connections);
  ctx.m_userProperties.put("NoOfConnections", ++connections, true);
  
  ctx.m_currentSection->put("Type", ctx.fname);
  return true;
}

/**
 * System rule: Just add it
 */
bool
transformSystem(InitConfigFileParser::Context & ctx, const char * data){

  const char * name;
  if(!ctx.m_currentSection->get("Name", &name)){
    ctx.reportError("Mandatory parameter Name missing from section "
		    "[%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  ndbout << "transformSystem " << name << endl;

  snprintf(ctx.pname, sizeof(ctx.pname), "SYSTEM_%s", name);
  
  return true;
}

/**
 * External system rule: Just add it
 */
bool
transformExternalSystem(InitConfigFileParser::Context & ctx, const char * data){
  const char * name;
  if(!ctx.m_currentSection->get("Name", &name)){
    ctx.reportError("Mandatory parameter Name missing from section "
		    "[%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  snprintf(ctx.pname, sizeof(ctx.pname), "EXTERNAL SYSTEM_%s", name);
  
  return true;
}

/**
 * Computer rule: Update "NoOfComputers", add "Type"
 */
bool
transformComputer(InitConfigFileParser::Context & ctx, const char * data){
  const char * id;
  if(!ctx.m_currentSection->get("Id", &id)){
    ctx.reportError("Mandatory parameter Id missing from section "
		    "[%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  snprintf(ctx.pname, sizeof(ctx.pname), "Computer_%s", id);
  
  Uint32 computers = 0;
  ctx.m_userProperties.get("NoOfComputers", &computers);
  ctx.m_userProperties.put("NoOfComputers", ++computers, true);
  
  const char * hostname = 0;
  ctx.m_currentSection->get("HostName", &hostname);
  if(!hostname){
    return true;
  }
  
  if(!strcmp(hostname, "localhost") || !strcmp(hostname, "127.0.0.1")){
    if(ctx.m_userProperties.get("$computer-localhost", &hostname)){
      ctx.reportError("Mixing of localhost with other hostname(%s) is illegal",
		      hostname);
      return false;
    }
  } else {
    ctx.m_userProperties.put("$computer-localhost", hostname);
  }
  
  return true;
}

/**
 * Apply default values
 */
void 
applyDefaultValues(InitConfigFileParser::Context & ctx,
		   const Properties * defaults){
  if(defaults != NULL){
    Properties::Iterator it(defaults);

    for(const char * name = it.first(); name != NULL; name = it.next()){
      ConfigInfo::Status st = ctx.m_info->getStatus(ctx.m_currentInfo, name);
      if(!ctx.m_currentSection->contains(name)){
	switch (ctx.m_info->getType(ctx.m_currentInfo, name)){
	case ConfigInfo::INT:
	case ConfigInfo::BOOL:{
	  Uint32 val = 0;
	  ::require(defaults->get(name, &val));
	  ctx.m_currentSection->put(name, val);
	  break;
	}
	case ConfigInfo::INT64:{
	  Uint64 val = 0;
	  ::require(defaults->get(name, &val));
	  ctx.m_currentSection->put64(name, val);
	  break;
	}
	case ConfigInfo::STRING:{
	  const char * val;
	  ::require(defaults->get(name, &val));
	  ctx.m_currentSection->put(name, val);
	  break;
	}
	case ConfigInfo::SECTION:
	  break;
	}
      }
    }
  }
}

bool
applyDefaultValues(InitConfigFileParser::Context & ctx, const char * data){
  
  if(strcmp(data, "user") == 0)
    applyDefaultValues(ctx, ctx.m_userDefaults);
  else if (strcmp(data, "system") == 0)
    applyDefaultValues(ctx, ctx.m_systemDefaults);
  else 
    return false;

  return true;
}

/**
 * Check that a section contains all MANDATORY parameters
 */
bool
checkMandatory(InitConfigFileParser::Context & ctx, const char * data){

  Properties::Iterator it(ctx.m_currentInfo);
  for(const char * name = it.first(); name != NULL; name = it.next()){
    const Properties * info = NULL;
    ::require(ctx.m_currentInfo->get(name, &info));
    Uint32 val;
    if(info->get("Default", &val) && val == MANDATORY){
      const char * fname;
      ::require(info->get("Fname", &fname));
      if(!ctx.m_currentSection->contains(fname)){
	ctx.reportError("Mandatory parameter %s missing from section "
			"[%s] starting at line: %d",
			fname, ctx.fname, ctx.m_sectionLineno);
	return false;
      }
    }
  }
  return true;
}

/**
 * Connection rule: Fix node id
 *
 * Transform a string "NodeidX" (e.g. "uppsala.32") 
 * into a Uint32 "NodeIdX" (e.g. 32) and a string "SystemX" (e.g. "uppsala").
 */
static bool fixNodeId(InitConfigFileParser::Context & ctx, const char * data)
{
  char buf[] = "NodeIdX";  buf[6] = data[sizeof("NodeI")];
  char sysbuf[] = "SystemX";  sysbuf[6] = data[sizeof("NodeI")];
  const char* nodeId;
  require(ctx.m_currentSection->get(buf, &nodeId));

  char tmpLine[MAX_LINE_LENGTH];
  strncpy(tmpLine, nodeId, MAX_LINE_LENGTH);
  char* token1 = strtok(tmpLine, ".");
  char* token2 = strtok(NULL, ".");
  Uint32 id;

  if (token2 == NULL) {                // Only a number given
    errno = 0;
    char* p;
    id = strtol(token1, &p, 10);
    if (errno != 0) warning("STRTOK1", nodeId);
    require(ctx.m_currentSection->put(buf, id, true));
  } else {                             // A pair given (e.g. "uppsala.32")
    errno = 0;
    char* p;
    id = strtol(token2, &p, 10);
    if (errno != 0) warning("STRTOK2", nodeId);
    require(ctx.m_currentSection->put(buf, id, true));
    require(ctx.m_currentSection->put(sysbuf, token1));
  }
  return true;
}

/**
 * @returns true if connection is external (one node is external)
 * Also returns: 
 * - name of external system in parameter extSystemName, and 
 * - nodeId of external node in parameter extSystemNodeId.
 */
static bool 
isExtConnection(InitConfigFileParser::Context & ctx, 
		const char **extSystemName, Uint32 * extSystemNodeId){

  Uint32 nodeId1, nodeId2;

  if (ctx.m_currentSection->contains("System1") &&
      ctx.m_currentSection->get("System1", extSystemName) &&
      ctx.m_currentSection->get("NodeId1", &nodeId1)) {
    *extSystemNodeId = nodeId1;
    return true;
  }

  if (ctx.m_currentSection->contains("System2") &&
      ctx.m_currentSection->get("System2", extSystemName) &&
      ctx.m_currentSection->get("NodeId2", &nodeId2)) {
    *extSystemNodeId = nodeId2;
    return true;
  }

  return false;
}

/**
 * External Connection Rule: 
 * If connection is to an external system, then move connection into
 * external system configuration (i.e. a sub-property).
 */
static bool
fixExtConnection(InitConfigFileParser::Context & ctx, const char * data){

  const char * extSystemName;
  Uint32 extSystemNodeId;

  if (isExtConnection(ctx, &extSystemName, &extSystemNodeId)) {

    Uint32 connections = 0;
    ctx.m_userProperties.get("ExtNoOfConnections", &connections);
    require(ctx.m_userProperties.put("ExtNoOfConnections",++connections, true));

    char tmpLine1[MAX_LINE_LENGTH];
    snprintf(tmpLine1, MAX_LINE_LENGTH, "Connection_%d", connections-1);

    /**
     *  Section:   EXTERNAL SYSTEM_<Ext System Name>
     */
    char extSystemPropName[MAX_LINE_LENGTH];
    strncpy(extSystemPropName, "EXTERNAL SYSTEM_", MAX_LINE_LENGTH);
    strncat(extSystemPropName, extSystemName, MAX_LINE_LENGTH);
    strncat(extSystemPropName, ":", MAX_LINE_LENGTH);
    strncat(extSystemPropName, tmpLine1, MAX_LINE_LENGTH);

    /**
     * Increase number of external connections for the system
     *
     * @todo Limitation: Only one external system is allowed
     */
    require(ctx.m_userProperties.put("ExtSystem", extSystemName, true));
    
    /**
     * Make sure section is stored in right place
     */
    strncpy(ctx.pname, extSystemPropName, MAX_LINE_LENGTH);

    /**
     * Since this is an external connection, 
     * decrease number of internal connections
     */
    require(ctx.m_userProperties.get("NoOfConnections", &connections));
    require(ctx.m_userProperties.put("NoOfConnections", --connections, true));
  }

  return true;
}

/**
 * Connection rule: Fix hostname
 * 
 * Unless Hostname is not already specified, do steps:
 * -# Via Connection's NodeId lookup Node
 * -# Via Node's ExecuteOnComputer lookup Hostname
 * -# Add HostName to Connection
 */
static bool
fixHostname(InitConfigFileParser::Context & ctx, const char * data){
  
  char buf[] = "NodeIdX"; buf[6] = data[sizeof("HostNam")];
  char sysbuf[] = "SystemX"; sysbuf[6] = data[sizeof("HostNam")];
  
  if(!ctx.m_currentSection->contains(data)){
    Uint32 id = 0;
    require(ctx.m_currentSection->get(buf, &id));
    
    const Properties * node;
    require(ctx.m_config->get("Node", id, &node));
    
    const char * hostname;
    require(node->get("HostName", &hostname));
    require(ctx.m_currentSection->put(data, hostname));
  }
  return true;
}

/**
 * Connection rule: Fix port number (using a port number adder)
 */
static bool
fixPortNumber(InitConfigFileParser::Context & ctx, const char * data){

  Uint32 id1= 0, id2= 0;
  require(ctx.m_currentSection->get("NodeId1", &id1));
  require(ctx.m_currentSection->get("NodeId2", &id2));
  id1 = id1 < id2 ? id1 : id2;

  const Properties * node;
  require(ctx.m_config->get("Node", id1, &node));
  BaseString hostname;
  require(node->get("HostName", hostname));
  
  if (hostname.c_str()[0] == 0) {
    ctx.reportError("Hostname required on nodeid %d since it will act as server.", id1);
    return false;
  }

  Uint32 port= 0;
  if (!node->get("ServerPort", &port) && !ctx.m_userProperties.get("ServerPort_", id1, &port)) {
    Uint32 adder= 0;
    {
      BaseString server_port_adder(hostname);
      server_port_adder.append("_ServerPortAdder");
      ctx.m_userProperties.get(server_port_adder.c_str(), &adder);
      ctx.m_userProperties.put(server_port_adder.c_str(), adder+1, true);
    }

    Uint32 base= 0;
    if (!ctx.m_userProperties.get("ServerPortBase", &base)){
      if(!(ctx.m_userDefaults && ctx.m_userDefaults->get("PortNumber", &base)) &&
	 !ctx.m_systemDefaults->get("PortNumber", &base)) {
	base= NDB_BASE_PORT+2;
      //      ctx.reportError("Cannot retrieve base port number");
      //      return false;
      }
      ctx.m_userProperties.put("ServerPortBase", base);
    }
    port= base + adder;
    ctx.m_userProperties.put("ServerPort_", id1, port);
  }

  if(ctx.m_currentSection->contains("PortNumber")) {
    ndbout << "PortNumber should no longer be specificied per connection, please remove from config. Will be changed to " << port << endl;
    ctx.m_currentSection->put("PortNumber", port, true);
  } else
    ctx.m_currentSection->put("PortNumber", port);

  return true;
}

/**
 * DB Node rule: Check various constraints
 */
static bool
checkDbConstraints(InitConfigFileParser::Context & ctx, const char *){

  Uint32 t1 = 0, t2 = 0;
  ctx.m_currentSection->get("MaxNoOfConcurrentOperations", &t1);
  ctx.m_currentSection->get("MaxNoOfConcurrentTransactions", &t2);
  
  if (t1 < t2) {
    ctx.reportError("MaxNoOfConcurrentOperations must be greater than "
		    "MaxNoOfConcurrentTransactions - [%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  Uint32 replicas = 0, otherReplicas;
  ctx.m_currentSection->get("NoOfReplicas", &replicas);
  if(ctx.m_userProperties.get("NoOfReplicas", &otherReplicas)){
    if(replicas != otherReplicas){
      ctx.reportError("NoOfReplicas defined differently on different nodes"
		      " - [%s] starting at line: %d",
		      ctx.fname, ctx.m_sectionLineno);
      return false;
    }
  } else {
    ctx.m_userProperties.put("NoOfReplicas", replicas);
  }
  
  return true;
}

/**
 * Connection rule: Check varius constraints
 */
static bool
checkConnectionConstraints(InitConfigFileParser::Context & ctx, const char *){

  Uint32 id1 = 0, id2 = 0;
  ctx.m_currentSection->get("NodeId1", &id1);
  ctx.m_currentSection->get("NodeId2", &id2);
  
  // If external connection, just accept it
  if (ctx.m_currentSection->contains("System1") ||
      ctx.m_currentSection->contains("System2")) 
    return true;

  if(id1 == id2){
    ctx.reportError("Illegal connection from node to itself"
		    " - [%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  const Properties * node1;
  if(!ctx.m_config->get("Node", id1, &node1)){
    ctx.reportError("Connection refering to undefined node: %d"
		    " - [%s] starting at line: %d",
		    id1, ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  const Properties * node2;
  if(!ctx.m_config->get("Node", id2, &node2)){
    ctx.reportError("Connection refering to undefined node: %d"
		    " - [%s] starting at line: %d",
		    id2, ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  const char * type1;
  const char * type2;
  require(node1->get("Type", &type1));
  require(node2->get("Type", &type2));

  /**
   * Report error if the following are true
   * -# None of the nodes is of type DB
   * -# Not both of them are MGMs
   * -# None of them contain a "SystemX" name
   */
  if((strcmp(type1, "DB") != 0 && strcmp(type2, "DB") != 0) &&
     !(strcmp(type1, "MGM") == 0 && strcmp(type2, "MGM") == 0) &&
     !ctx.m_currentSection->contains("System1") &&
     !ctx.m_currentSection->contains("System2")){
    ctx.reportError("Invalid connection between node %d (%s) and node %d (%s)"
		    " - [%s] starting at line: %d",
		    id1, type1, id2, type2, 
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  return true;
}

static bool
checkTCPConstraints(InitConfigFileParser::Context & ctx, const char * data){
  
  const char * host;
  struct in_addr addr;
  if(ctx.m_currentSection->get(data, &host) && strlen(host) && 
     Ndb_getInAddr(&addr, host)){
    ctx.reportError("Unable to lookup/illegal hostname %s"
		    " - [%s] starting at line: %d",
		    host, ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  return true;
}

static
bool
transform(InitConfigFileParser::Context & ctx,
	  Properties & dst, 
	  const char * oldName,
	  const char * newName,
	  double add, double mul){
  
  if(ctx.m_currentSection->contains(newName)){
    ctx.reportError("Both %s and %s specified"
		    " - [%s] starting at line: %d",
		    oldName, newName,
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  
  PropertiesType oldType;
  require(ctx.m_currentSection->getTypeOf(oldName, &oldType));
  ConfigInfo::Type newType = ctx.m_info->getType(ctx.m_currentInfo, newName);  
  if(!((oldType == PropertiesType_Uint32 || oldType == PropertiesType_Uint64) 
       && (newType == ConfigInfo::INT || newType == ConfigInfo::INT64 || newType == ConfigInfo::BOOL))){
    ndbout << "oldType: " << (int)oldType << ", newType: " << (int)newType << endl;
    ctx.reportError("Unable to handle type conversion w.r.t deprication %s %s"
		    "- [%s] starting at line: %d",
		    oldName, newName,
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  Uint64 oldVal;
  require(ctx.m_currentSection->get(oldName, &oldVal));

  Uint64 newVal = (Uint64)(oldVal * mul + add);
  if(!ctx.m_info->verify(ctx.m_currentInfo, newName, newVal)){
    ctx.reportError("Unable to handle deprication, new value not within bounds"
		    "%s %s - [%s] starting at line: %d",
		    oldName, newName,
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  if(newType == ConfigInfo::INT || newType == ConfigInfo::BOOL){
    require(dst.put(newName, (Uint32)newVal));
  } else if(newType == ConfigInfo::INT64) {
    require(dst.put64(newName, newVal));    
  }
  return true;
}

static bool
fixDepricated(InitConfigFileParser::Context & ctx, const char * data){
  const char * name;
  /**
   * Transform old values to new values
   * Transform new values to old values (backward compatible)
   */
  Properties tmp(true);
  Properties::Iterator it(ctx.m_currentSection);
  for (name = it.first(); name != NULL; name = it.next()) {
    const DepricationTransform * p = &f_deprication[0];
    while(p->m_section != 0){
      if(strcmp(p->m_section, ctx.fname) == 0){
	double mul = p->m_mul;
	double add = p->m_add;
	if(strcmp(name, p->m_oldName) == 0){
	  if(!transform(ctx, tmp, name, p->m_newName, add, mul)){
	    return false;
	  }
	} else if(strcmp(name, p->m_newName) == 0) {
	  if(!transform(ctx, tmp, name, p->m_oldName, -add/mul,1.0/mul)){
	    return false;
	  }
	}
      }
      p++;
    }
  }
  
  Properties::Iterator it2(&tmp);
  for (name = it2.first(); name != NULL; name = it2.next()) {
    PropertiesType type;
    require(tmp.getTypeOf(name, &type));
    switch(type){
    case PropertiesType_Uint32:{
      Uint32 val;
      require(tmp.get(name, &val));
      ::require(ctx.m_currentSection->put(name, val));
      break;
    }
    case PropertiesType_char:{
      const char * val;
      require(tmp.get(name, &val));
      ::require(ctx.m_currentSection->put(name, val));
      break;
    }
    case PropertiesType_Uint64:{
      Uint64 val;
      require(tmp.get(name, &val));
      ::require(ctx.m_currentSection->put64(name, val));
      break;
    }
    case PropertiesType_Properties:
    default:
      abort();
    }
  }
  return true;
}

static bool
saveInConfigValues(InitConfigFileParser::Context & ctx, const char * data){
  const Properties * sec;
  if(!ctx.m_currentInfo->get(ctx.fname, &sec)){
    abort();
    return false;
  }
  
  do {
    const char *secName;
    Uint32 id, status, typeVal;
    require(sec->get("Fname", &secName));
    require(sec->get("Id", &id));
    require(sec->get("Status", &status));
    require(sec->get("Default", &typeVal));
    
    if(id == KEY_INTERNAL || status == ConfigInfo::INTERNAL){
      ndbout_c("skipping section %s", ctx.fname);
      break;
    }
    
    Uint32 no = 0;
    ctx.m_userProperties.get("$Section", id, &no);
    ctx.m_userProperties.put("$Section", id, no+1, true);
    
    ctx.m_configValues.openSection(id, no);
    ctx.m_configValues.put(CFG_TYPE_OF_SECTION, typeVal);
    
    Properties::Iterator it(ctx.m_currentSection);
    for (const char* n = it.first(); n != NULL; n = it.next()) {
      const Properties * info;
      if(!ctx.m_currentInfo->get(n, &info))
	continue;

      Uint32 id = 0;
      info->get("Id", &id);
      
      if(id == KEY_INTERNAL)
	continue;

      bool ok = true;
      PropertiesType type;
      require(ctx.m_currentSection->getTypeOf(n, &type));
      switch(type){
      case PropertiesType_Uint32:{
	Uint32 val;
	require(ctx.m_currentSection->get(n, &val));
	ok = ctx.m_configValues.put(id, val);
	break;
      }
      case PropertiesType_Uint64:{
	Uint64 val;
	require(ctx.m_currentSection->get(n, &val));
	ok = ctx.m_configValues.put64(id, val);
	break;
      }
      case PropertiesType_char:{
	const char * val;
	require(ctx.m_currentSection->get(n, &val));
	ok = ctx.m_configValues.put(id, val);
	break;
      }
      default:
	abort();
      }
      require(ok);
    }
    ctx.m_configValues.closeSection();
  } while(0);
  return true;
}

static bool
sanity_checks(Vector<ConfigInfo::ConfigRuleSection>&sections, 
	      struct InitConfigFileParser::Context &ctx, 
	      const char * rule_data)
{
  Uint32 db_nodes = 0;
  Uint32 mgm_nodes = 0;
  Uint32 api_nodes = 0;
  if (!ctx.m_userProperties.get("DB", &db_nodes)) {
    ctx.reportError("At least one database node should be defined in config file");
    return false;
  }
  if (!ctx.m_userProperties.get("MGM", &mgm_nodes)) {
    ctx.reportError("At least one management server node should be defined in config file");
    return false;
  }
  if (!ctx.m_userProperties.get("API", &api_nodes)) {
    ctx.reportError("At least one application node (for the mysqld) should be defined in config file");
    return false;
  }
  return true;
}

static bool
add_node_connections(Vector<ConfigInfo::ConfigRuleSection>&sections, 
		   struct InitConfigFileParser::Context &ctx, 
		   const char * rule_data)
{
  Uint32 i;
  Properties * props= ctx.m_config;
  Properties p_connections(true);
  Properties p_connections2(true);

  for (i = 0;; i++){
    const Properties * tmp;
    Uint32 nodeId1, nodeId2;

    if(!props->get("Connection", i, &tmp)) break;

    if(!tmp->get("NodeId1", &nodeId1)) continue;
    p_connections.put("", nodeId1, nodeId1);
    if(!tmp->get("NodeId2", &nodeId2)) continue;
    p_connections.put("", nodeId2, nodeId2);

    p_connections2.put("", nodeId1 + nodeId2<<16, nodeId1);
    p_connections2.put("", nodeId2 + nodeId1<<16, nodeId2);
  }

  Uint32 nNodes;
  ctx.m_userProperties.get("NoOfNodes", &nNodes);

  Properties p_db_nodes(true);
  Properties p_api_mgm_nodes(true);

  Uint32 i_db= 0, i_api_mgm= 0, n;
  for (i= 0, n= 0; n < nNodes; i++){
    const Properties * tmp;
    if(!props->get("Node", i, &tmp)) continue;
    n++;

    const char * type;
    if(!tmp->get("Type", &type)) continue;

    if (strcmp(type,"DB") == 0)
      p_db_nodes.put("", i_db++, i);
    else if (strcmp(type,"API") == 0 ||
	     strcmp(type,"MGM") == 0)
      p_api_mgm_nodes.put("", i_api_mgm++, i);
  }

  Uint32 nodeId1, nodeId2, dummy;

  for (i= 0; p_db_nodes.get("", i, &nodeId1); i++){
    for (Uint32 j= i+1;; j++){
      if(!p_db_nodes.get("", j, &nodeId2)) break;
      if(!p_connections2.get("", nodeId1+nodeId2<<16, &dummy)) {
	ConfigInfo::ConfigRuleSection s;
	s.m_sectionType= BaseString("TCP");
	s.m_sectionData= new Properties(true);
	char buf[16];
	snprintf(buf, sizeof(buf), "%u", nodeId1);
	s.m_sectionData->put("NodeId1", buf);
	snprintf(buf, sizeof(buf), "%u", nodeId2);
	s.m_sectionData->put("NodeId2", buf);
	sections.push_back(s);
      }
    }
  }

  for (i= 0; p_api_mgm_nodes.get("", i, &nodeId1); i++){
    if(!p_connections.get("", nodeId1, &dummy)) {
      for (Uint32 j= 0;; j++){
	if(!p_db_nodes.get("", j, &nodeId2)) break;
	ConfigInfo::ConfigRuleSection s;
	s.m_sectionType= BaseString("TCP");
	s.m_sectionData= new Properties(true);
	char buf[16];
	snprintf(buf, sizeof(buf), "%u", nodeId1);
	s.m_sectionData->put("NodeId1", buf);
	snprintf(buf, sizeof(buf), "%u", nodeId2);
	s.m_sectionData->put("NodeId2", buf);
	sections.push_back(s);
      }
    }
  }

  return true;
}


static bool add_server_ports(Vector<ConfigInfo::ConfigRuleSection>&sections, 
		      struct InitConfigFileParser::Context &ctx, 
		      const char * rule_data)
{
#if 0
  Properties * props= ctx.m_config;
  Properties computers(true);
  Uint32 port_base = NDB_BASE_PORT+2;

  Uint32 nNodes;
  ctx.m_userProperties.get("NoOfNodes", &nNodes);

  for (Uint32 i= 0, n= 0; n < nNodes; i++){
    Properties * tmp;
    if(!props->get("Node", i, &tmp)) continue;
    n++;

    const char * type;
    if(!tmp->get("Type", &type)) continue;

    Uint32 port;
    if (tmp->get("ServerPort", &port)) continue;

    Uint32 computer;
    if (!tmp->get("ExecuteOnComputer", &computer)) continue;

    Uint32 adder= 0;
    computers.get("",computer, &adder);

    if (strcmp(type,"DB") == 0) {
      adder++;
      tmp->put("ServerPort", port_base+adder);
      computers.put("",computer, adder);
    }
  }
#endif
  return true;
}

static bool
check_node_vs_replicas(Vector<ConfigInfo::ConfigRuleSection>&sections, 
		       struct InitConfigFileParser::Context &ctx, 
		       const char * rule_data)
{
  Uint32 db_nodes = 0;
  Uint32 replicas = 0;
  if (!ctx.m_userProperties.get("DB", &db_nodes)) {
    return true;
  }
  ctx.m_userProperties.get("NoOfReplicas", &replicas);
  if((db_nodes % replicas) != 0){
    ctx.reportError("Invalid no of db nodes wrt no of replicas.\n"
		    "No of nodes must be dividable with no or replicas");
    return false;
  }
  
  return true;
}

template class Vector<ConfigInfo::ConfigRuleSection>;
