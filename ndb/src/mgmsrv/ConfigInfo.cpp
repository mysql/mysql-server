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

#include <ndb_global.h>
#include <ndb_opt_defaults.h>

#include <NdbTCP.h>
#include "ConfigInfo.hpp"
#include <mgmapi_config_parameters.h>
#include <ndb_limits.h>
#include "InitConfigFileParser.hpp"
#include <m_string.h>

extern my_bool opt_ndb_shm;
extern my_bool opt_core;

#define MAX_LINE_LENGTH 255
#define KEY_INTERNAL 0
#define MAX_INT_RNIL 0xfffffeff

#define _STR_VALUE(x) #x
#define STR_VALUE(x) _STR_VALUE(x)

/****************************************************************************
 * Section names
 ****************************************************************************/

#define DB_TOKEN_PRINT  "ndbd(DB)"
#define MGM_TOKEN_PRINT "ndb_mgmd(MGM)"
#define API_TOKEN_PRINT "mysqld(API)"

#define DB_TOKEN "DB"
#define MGM_TOKEN "MGM"
#define API_TOKEN "API"

const ConfigInfo::AliasPair
ConfigInfo::m_sectionNameAliases[]={
  {API_TOKEN, "MYSQLD"},
  {DB_TOKEN,  "NDBD"},
  {MGM_TOKEN, "NDB_MGMD"},
  {0, 0}
};

const char* 
ConfigInfo::m_sectionNames[]={
  "SYSTEM",
  "COMPUTER",

  DB_TOKEN,
  MGM_TOKEN,
  API_TOKEN,

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
static bool transformNode(InitConfigFileParser::Context & ctx, const char *);
static bool checkConnectionSupport(InitConfigFileParser::Context & ctx, const char *);
static bool transformConnection(InitConfigFileParser::Context & ctx, const char *);
static bool applyDefaultValues(InitConfigFileParser::Context & ctx, const char *);
static bool checkMandatory(InitConfigFileParser::Context & ctx, const char *);
static bool fixPortNumber(InitConfigFileParser::Context & ctx, const char *);
static bool fixShmKey(InitConfigFileParser::Context & ctx, const char *);
static bool checkDbConstraints(InitConfigFileParser::Context & ctx, const char *);
static bool checkConnectionConstraints(InitConfigFileParser::Context &, const char *);
static bool checkTCPConstraints(InitConfigFileParser::Context &, const char *);
static bool fixNodeHostname(InitConfigFileParser::Context & ctx, const char * data);
static bool fixHostname(InitConfigFileParser::Context & ctx, const char * data);
static bool fixNodeId(InitConfigFileParser::Context & ctx, const char * data);
static bool fixDepricated(InitConfigFileParser::Context & ctx, const char *);
static bool saveInConfigValues(InitConfigFileParser::Context & ctx, const char *);
static bool fixFileSystemPath(InitConfigFileParser::Context & ctx, const char * data);
static bool fixBackupDataDir(InitConfigFileParser::Context & ctx, const char * data);
static bool fixShmUniqueId(InitConfigFileParser::Context & ctx, const char * data);
static bool checkLocalhostHostnameMix(InitConfigFileParser::Context & ctx, const char * data);

const ConfigInfo::SectionRule 
ConfigInfo::m_SectionRules[] = {
  { "SYSTEM", transformSystem, 0 },
  { "COMPUTER", transformComputer, 0 },

  { DB_TOKEN,   transformNode, 0 },
  { API_TOKEN,  transformNode, 0 },
  { MGM_TOKEN,  transformNode, 0 },

  { MGM_TOKEN,  fixShmUniqueId, 0 },

  { "TCP",  checkConnectionSupport, 0 },
  { "SHM",  checkConnectionSupport, 0 },
  { "SCI",  checkConnectionSupport, 0 },
  { "OSE",  checkConnectionSupport, 0 },

  { "TCP",  transformConnection, 0 },
  { "SHM",  transformConnection, 0 },
  { "SCI",  transformConnection, 0 },
  { "OSE",  transformConnection, 0 },

  { DB_TOKEN,   fixNodeHostname, 0 },
  { API_TOKEN,  fixNodeHostname, 0 },
  { MGM_TOKEN,  fixNodeHostname, 0 },

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
  { "SHM",  fixHostname, "HostName1" },
  { "SHM",  fixHostname, "HostName2" },
  { "SCI",  fixHostname, "HostName1" },
  { "SCI",  fixHostname, "HostName2" },
  { "SHM",  fixHostname, "HostName1" },
  { "SHM",  fixHostname, "HostName2" },
  { "OSE",  fixHostname, "HostName1" },
  { "OSE",  fixHostname, "HostName2" },

  { "TCP",  fixPortNumber, 0 }, // has to come after fixHostName
  { "SHM",  fixPortNumber, 0 }, // has to come after fixHostName
  { "SCI",  fixPortNumber, 0 }, // has to come after fixHostName

  { "*",    applyDefaultValues, "user" },
  { "*",    fixDepricated, 0 },
  { "*",    applyDefaultValues, "system" },

  { "SHM",  fixShmKey, 0 }, // has to come after apply default values

  { DB_TOKEN,   checkLocalhostHostnameMix, 0 },
  { API_TOKEN,  checkLocalhostHostnameMix, 0 },
  { MGM_TOKEN,  checkLocalhostHostnameMix, 0 },

  { DB_TOKEN,   fixFileSystemPath, 0 },
  { DB_TOKEN,   fixBackupDataDir, 0 },

  { DB_TOKEN,   checkDbConstraints, 0 },

  { "TCP",  checkConnectionConstraints, 0 },
  { "SHM",  checkConnectionConstraints, 0 },
  { "SCI",  checkConnectionConstraints, 0 },
  { "OSE",  checkConnectionConstraints, 0 },

  { "TCP",  checkTCPConstraints, "HostName1" },
  { "TCP",  checkTCPConstraints, "HostName2" },
  { "SCI",  checkTCPConstraints, "HostName1" },
  { "SCI",  checkTCPConstraints, "HostName2" },
  { "SHM",  checkTCPConstraints, "HostName1" },
  { "SHM",  checkTCPConstraints, "HostName2" },
  
  { "*",    checkMandatory, 0 },
  
  { DB_TOKEN,   saveInConfigValues, 0 },
  { API_TOKEN,  saveInConfigValues, 0 },
  { MGM_TOKEN,  saveInConfigValues, 0 },

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
static bool set_connection_priorities(Vector<ConfigInfo::ConfigRuleSection>&sections, 
				 struct InitConfigFileParser::Context &ctx, 
				 const char * rule_data);
static bool check_node_vs_replicas(Vector<ConfigInfo::ConfigRuleSection>&sections, 
			    struct InitConfigFileParser::Context &ctx, 
			    const char * rule_data);

const ConfigInfo::ConfigRule 
ConfigInfo::m_ConfigRules[] = {
  { sanity_checks, 0 },
  { add_node_connections, 0 },
  { set_connection_priorities, 0 },
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
  { DB_TOKEN, "Discless", "Diskless", 0, 1 },
  { DB_TOKEN, "Id", "NodeId", 0, 1 },
  { API_TOKEN, "Id", "NodeId", 0, 1 },
  { MGM_TOKEN, "Id", "NodeId", 0, 1 },
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
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_SECTION,
    0,
    0, 0 },
  
  {
    KEY_INTERNAL,
    "Id",
    "COMPUTER",
    "Name of computer",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },

  {
    KEY_INTERNAL,
    "HostName",
    "COMPUTER",
    "Hostname of computer (e.g. mysql.com)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },

  {
    KEY_INTERNAL,
    "ByteOrder",
    "COMPUTER",
    0,
    ConfigInfo::CI_DEPRICATED,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0,
    0 },
  
  /****************************************************************************
   * SYSTEM
   ***************************************************************************/
  {
    CFG_SECTION_SYSTEM,
    "SYSTEM",
    "SYSTEM",
    "System section",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)CFG_SECTION_SYSTEM,
    0, 0 },

  {
    CFG_SYS_NAME,
    "Name",
    "SYSTEM",
    "Name of system (NDB Cluster)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },
  
  {
    CFG_SYS_PRIMARY_MGM_NODE,
    "PrimaryMGMNode",
    "SYSTEM",
    "Node id of Primary "MGM_TOKEN_PRINT" node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SYS_CONFIG_GENERATION,
    "ConfigGenerationNumber",
    "SYSTEM",
    "Configuration generation number",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  /***************************************************************************
   * DB
   ***************************************************************************/
  {
    CFG_SECTION_NODE,
    DB_TOKEN,
    DB_TOKEN,
    "Node section",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)NODE_TYPE_DB, 
    0, 0
  },

  {
    CFG_NODE_HOST,
    "HostName",
    DB_TOKEN,
    "Name of computer for this node",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    "localhost",
    0, 0 },

  {
    CFG_NODE_SYSTEM,
    "System",
    DB_TOKEN,
    "Name of system for this node",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    KEY_INTERNAL,
    "Id",
    DB_TOKEN,
    "",
    ConfigInfo::CI_DEPRICATED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },

  {
    CFG_NODE_ID,
    "NodeId",
    DB_TOKEN,
    "Number identifying the database node ("DB_TOKEN_PRINT")",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },

  {
    KEY_INTERNAL,
    "ServerPort",
    DB_TOKEN,
    "Port used to setup transporter",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    UNDEFINED,
    "1",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_REPLICAS,
    "NoOfReplicas",
    DB_TOKEN,
    "Number of copies of all data in the database (1-4)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    "4" },

  {
    CFG_DB_NO_ATTRIBUTES,
    "MaxNoOfAttributes",
    DB_TOKEN,
    "Total number of attributes stored in database. I.e. sum over all tables",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1000",
    "32",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_NO_TABLES,
    "MaxNoOfTables",
    DB_TOKEN,
    "Total number of tables stored in the database",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "128",
    "8",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_NO_ORDERED_INDEXES,
    "MaxNoOfOrderedIndexes",
    DB_TOKEN,
    "Total number of ordered indexes that can be defined in the system",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "128",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_UNIQUE_HASH_INDEXES,
    "MaxNoOfUniqueHashIndexes",
    DB_TOKEN,
    "Total number of unique hash indexes that can be defined in the system",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "64",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_INDEXES,
    "MaxNoOfIndexes",
    DB_TOKEN,
    "Total number of indexes that can be defined in the system",
    ConfigInfo::CI_DEPRICATED,
    false,
    ConfigInfo::CI_INT,
    "128",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_INDEX_OPS,
    "MaxNoOfConcurrentIndexOperations",
    DB_TOKEN,
    "Total number of index operations that can execute simultaneously on one "DB_TOKEN_PRINT" node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "8K",
    "0",
    STR_VALUE(MAX_INT_RNIL) 
   },

  {
    CFG_DB_NO_TRIGGERS,
    "MaxNoOfTriggers",
    DB_TOKEN,
    "Total number of triggers that can be defined in the system",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "768",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_TRIGGER_OPS,
    "MaxNoOfFiredTriggers",
    DB_TOKEN,
    "Total number of triggers that can fire simultaneously in one "DB_TOKEN_PRINT" node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "4000",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    DB_TOKEN,
    "String referencing an earlier defined COMPUTER",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },
  
  {
    CFG_DB_NO_SAVE_MSGS,
    "MaxNoOfSavedMessages",
    DB_TOKEN,
    "Max number of error messages in error log and max number of trace files",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "25",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_MEMLOCK,
    "LockPagesInMainMemory",
    DB_TOKEN,
    "If set to yes, then NDB Cluster data will not be swapped out to disk",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_DB_WATCHDOG_INTERVAL,
    "TimeBetweenWatchDogCheck",
    DB_TOKEN,
    "Time between execution checks inside a database node",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "6000",
    "70",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_STOP_ON_ERROR,
    "StopOnError",
    DB_TOKEN,
    "If set to N, "DB_TOKEN_PRINT" automatically restarts/recovers in case of node failure",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_BOOL,
    "true",
    "false",
    "true" },

  { 
    CFG_DB_STOP_ON_ERROR_INSERT,
    "RestartOnErrorInsert",
    DB_TOKEN,
    "See src/kernel/vm/Emulator.hpp NdbRestartType for details",
    ConfigInfo::CI_INTERNAL,
    true,
    ConfigInfo::CI_INT,
    "2",
    "0",
    "4" },
  
  {
    CFG_DB_NO_OPS,
    "MaxNoOfConcurrentOperations",
    DB_TOKEN,
    "Max number of operation records in transaction coordinator",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "32k",
    "32",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_LOCAL_OPS,
    "MaxNoOfLocalOperations",
    DB_TOKEN,
    "Max number of operation records defined in the local storage node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    UNDEFINED,
    "32",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_LOCAL_SCANS,
    "MaxNoOfLocalScans",
    DB_TOKEN,
    "Max number of fragment scans in parallel in the local storage node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    UNDEFINED,
    "32",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_BATCH_SIZE,
    "BatchSizePerLocalScan",
    DB_TOKEN,
    "Used to calculate the number of lock records for scan with hold lock",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    STR_VALUE(DEF_BATCH_SIZE),
    "1",
    STR_VALUE(MAX_PARALLEL_OP_PER_SCAN) },

  {
    CFG_DB_NO_TRANSACTIONS,
    "MaxNoOfConcurrentTransactions",
    DB_TOKEN,
    "Max number of transaction executing concurrently on the "DB_TOKEN_PRINT" node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "4096",
    "32",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_SCANS,
    "MaxNoOfConcurrentScans",
    DB_TOKEN,
    "Max number of scans executing concurrently on the "DB_TOKEN_PRINT" node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "256",
    "2",
    "500" },

  {
    CFG_DB_TRANS_BUFFER_MEM,
    "TransactionBufferMemory",
    DB_TOKEN,
    "Dynamic buffer space (in bytes) for key and attribute data allocated for each "DB_TOKEN_PRINT" node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1M",
    "1K",
    STR_VALUE(MAX_INT_RNIL) },
 
  {
    CFG_DB_INDEX_MEM,
    "IndexMemory",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for storing indexes",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "18M",
    "1M",
    "1024G" },

  {
    CFG_DB_DATA_MEM,
    "DataMemory",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for storing data",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "80M",
    "1M",
    "1024G" },

  {
    CFG_DB_UNDO_INDEX_BUFFER,
    "UndoIndexBuffer",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for writing UNDO logs for index part",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "2M",
    "1M",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_UNDO_DATA_BUFFER,
    "UndoDataBuffer",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for writing UNDO logs for data part",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "16M",
    "1M",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_REDO_BUFFER,
    "RedoBuffer",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for writing REDO logs",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "8M",
    "1M",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_LONG_SIGNAL_BUFFER,
    "LongMessageBuffer",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for internal long messages",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1M",
    "512k",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_START_PARTIAL_TIMEOUT,
    "StartPartialTimeout",
    DB_TOKEN,
    "Time to wait before trying to start wo/ all nodes. 0=Wait forever",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "30000",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_START_PARTITION_TIMEOUT,
    "StartPartitionedTimeout",
    DB_TOKEN,
    "Time to wait before trying to start partitioned. 0=Wait forever",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "60000",
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_START_FAILURE_TIMEOUT,
    "StartFailureTimeout",
    DB_TOKEN,
    "Time to wait before terminating. 0=Wait forever",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalDbDb",
    DB_TOKEN,
    "Time between "DB_TOKEN_PRINT"-"DB_TOKEN_PRINT" heartbeats. "DB_TOKEN_PRINT" considered dead after 3 missed HBs",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "1500",
    "10",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_API_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalDbApi",
    DB_TOKEN,
    "Time between "API_TOKEN_PRINT"-"DB_TOKEN_PRINT" heartbeats. "API_TOKEN_PRINT" connection closed after 3 missed HBs",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "1500",
    "100",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_INTERVAL,
    "TimeBetweenLocalCheckpoints",
    DB_TOKEN,
    "Time between taking snapshots of the database (expressed in 2log of bytes)",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "20",
    "0",
    "31" },

  {
    CFG_DB_GCP_INTERVAL,
    "TimeBetweenGlobalCheckpoints",
    DB_TOKEN,
    "Time between doing group commit of transactions to disk",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "2000",
    "10",
    "32000" },

  {
    CFG_DB_NO_REDOLOG_FILES,
    "NoOfFragmentLogFiles",
    DB_TOKEN,
    "No of 16 Mbyte Redo log files in each of 4 file sets belonging to "DB_TOKEN_PRINT" node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "8",
    "1",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_MAX_OPEN_FILES,
    "MaxNoOfOpenFiles",
    DB_TOKEN,
    "Max number of files open per "DB_TOKEN_PRINT" node.(One thread is created per file)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "40",
    "20",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_TRANSACTION_CHECK_INTERVAL,
    "TimeBetweenInactiveTransactionAbortCheck",
    DB_TOKEN,
    "Time between inactive transaction checks",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "1000",
    "1000",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_TRANSACTION_INACTIVE_TIMEOUT,
    "TransactionInactiveTimeout",
    DB_TOKEN,
    "Time application can wait before executing another transaction part (ms).\n"
    "This is the time the transaction coordinator waits for the application\n"
    "to execute or send another part (query, statement) of the transaction.\n"
    "If the application takes too long time, the transaction gets aborted.\n"
    "Timeout set to 0 means that we don't timeout at all on application wait.",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    STR_VALUE(MAX_INT_RNIL),
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT,
    "TransactionDeadlockDetectionTimeout",
    DB_TOKEN,
    "Time transaction can be executing in a DB node (ms).\n"
    "This is the time the transaction coordinator waits for each database node\n"
    "of the transaction to execute a request. If the database node takes too\n"
    "long time, the transaction gets aborted.",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "1200",
    "50",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_DISC_PAGES_TUP_SR,
    "NoOfDiskPagesToDiskDuringRestartTUP",
    DB_TOKEN,
    "?",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "40",
    "1",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_DISC_PAGES_TUP,
    "NoOfDiskPagesToDiskAfterRestartTUP",
    DB_TOKEN,
    "?",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "40",
    "1",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_DISC_PAGES_ACC_SR,
    "NoOfDiskPagesToDiskDuringRestartACC",
    DB_TOKEN,
    "?",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "20",
    "1",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_DISC_PAGES_ACC,
    "NoOfDiskPagesToDiskAfterRestartACC",
    DB_TOKEN,
    "?",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_INT,
    "20",
    "1",
    STR_VALUE(MAX_INT_RNIL) },
  

  {
    CFG_DB_DISCLESS,
    "Diskless",
    DB_TOKEN,
    "Run wo/ disk",
    ConfigInfo::CI_USED,
    true,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"},

  {
    KEY_INTERNAL,
    "Discless",
    DB_TOKEN,
    "Diskless",
    ConfigInfo::CI_DEPRICATED,
    true,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"},
  

  
  {
    CFG_DB_ARBIT_TIMEOUT,
    "ArbitrationTimeout",
    DB_TOKEN,
    "Max time (milliseconds) database partion waits for arbitration signal",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "3000",
    "10",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_NODE_DATADIR,
    "DataDir",
    DB_TOKEN,
    "Data directory for this node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MYSQLCLUSTERDIR,
    0, 0 },

  {
    CFG_DB_FILESYSTEM_PATH,
    "FileSystemPath",
    DB_TOKEN,
    "Path to directory where the "DB_TOKEN_PRINT" node stores its data (directory must exist)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_LOGLEVEL_STARTUP,
    "LogLevelStartup",
    DB_TOKEN,
    "Node startup info printed on stdout",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1",
    "0",
    "15" },
  
  {
    CFG_LOGLEVEL_SHUTDOWN,
    "LogLevelShutdown",
    DB_TOKEN,
    "Node shutdown info printed on stdout",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_STATISTICS,
    "LogLevelStatistic",
    DB_TOKEN,
    "Transaction, operation, transporter info printed on stdout",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_CHECKPOINT,
    "LogLevelCheckpoint",
    DB_TOKEN,
    "Local and Global checkpoint info printed on stdout",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_NODERESTART,
    "LogLevelNodeRestart",
    DB_TOKEN,
    "Node restart, node failure info printed on stdout",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_CONNECTION,
    "LogLevelConnection",
    DB_TOKEN,
    "Node connect/disconnect info printed on stdout",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_CONGESTION,
    "LogLevelCongestion",
    DB_TOKEN,
    "Congestion info printed on stdout",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_ERROR,
    "LogLevelError",
    DB_TOKEN,
    "Transporter, heartbeat errors printed on stdout",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_INFO,
    "LogLevelInfo",
    DB_TOKEN,
    "Heartbeat and log info printed on stdout",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "15" },

  /**
   * Backup
   */
  { 
    CFG_DB_PARALLEL_BACKUPS,
    "ParallelBackups",
    DB_TOKEN,
    "Maximum number of parallel backups",
    ConfigInfo::CI_NOTIMPLEMENTED,
    false,
    ConfigInfo::CI_INT,
    "1",
    "1",
    "1" },
  
  { 
    CFG_DB_BACKUP_DATADIR,
    "BackupDataDir",
    DB_TOKEN,
    "Path to where to store backups",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },
  
  { 
    CFG_DB_BACKUP_MEM,
    "BackupMemory",
    DB_TOKEN,
    "Total memory allocated for backups per node (in bytes)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "4M", // sum of BackupDataBufferSize and BackupLogBufferSize
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  { 
    CFG_DB_BACKUP_DATA_BUFFER_MEM,
    "BackupDataBufferSize",
    DB_TOKEN,
    "Default size of databuffer for a backup (in bytes)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "2M", // remember to change BackupMemory
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_BACKUP_LOG_BUFFER_MEM,
    "BackupLogBufferSize",
    DB_TOKEN,
    "Default size of logbuffer for a backup (in bytes)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "2M", // remember to change BackupMemory
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_BACKUP_WRITE_SIZE,
    "BackupWriteSize",
    DB_TOKEN,
    "Default size of filesystem writes made by backup (in bytes)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "32K",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  /***************************************************************************
   * API
   ***************************************************************************/
  {
    CFG_SECTION_NODE,
    API_TOKEN,
    API_TOKEN,
    "Node section",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)NODE_TYPE_API, 
    0, 0
  },

  {
    CFG_NODE_HOST,
    "HostName",
    API_TOKEN,
    "Name of computer for this node",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    "",
    0, 0 },

  {
    CFG_NODE_SYSTEM,
    "System",
    API_TOKEN,
    "Name of system for this node",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    KEY_INTERNAL,
    "Id",
    API_TOKEN,
    "",
    ConfigInfo::CI_DEPRICATED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },

  {
    CFG_NODE_ID,
    "NodeId",
    API_TOKEN,
    "Number identifying application node ("API_TOKEN_PRINT")",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },

  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    API_TOKEN,
    "String referencing an earlier defined COMPUTER",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_NODE_ARBIT_RANK,
    "ArbitrationRank",
    API_TOKEN,
    "If 0, then "API_TOKEN_PRINT" is not arbitrator. Kernel selects arbitrators in order 1, 2",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "2" },

  {
    CFG_NODE_ARBIT_DELAY,
    "ArbitrationDelay",
    API_TOKEN,
    "When asked to arbitrate, arbitrator waits this long before voting (msec)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_MAX_SCAN_BATCH_SIZE,
    "MaxScanBatchSize",
    "API",
    "The maximum collective batch size for one scan",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    STR_VALUE(MAX_SCAN_BATCH_SIZE),
    "32k",
    "16M" },
  
  {
    CFG_BATCH_BYTE_SIZE,
    "BatchByteSize",
    "API",
    "The default batch size in bytes",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    STR_VALUE(SCAN_BATCH_SIZE),
    "1k",
    "1M" },

  {
    CFG_BATCH_SIZE,
    "BatchSize",
    "API",
    "The default batch size in number of records",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    STR_VALUE(DEF_BATCH_SIZE),
    "1",
    STR_VALUE(MAX_PARALLEL_OP_PER_SCAN) },

  /****************************************************************************
   * MGM
   ***************************************************************************/
  {
    CFG_SECTION_NODE,
    MGM_TOKEN,
    MGM_TOKEN,
    "Node section",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)NODE_TYPE_MGM, 
    0, 0
  },

  {
    CFG_NODE_HOST,
    "HostName",
    MGM_TOKEN,
    "Name of computer for this node",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    "",
    0, 0 },

  {
    CFG_NODE_DATADIR,
    "DataDir",
    MGM_TOKEN,
    "Data directory for this node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MYSQLCLUSTERDIR,
    0, 0 },

  {
    CFG_NODE_SYSTEM,
    "System",
    MGM_TOKEN,
    "Name of system for this node",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    KEY_INTERNAL,
    "Id",
    MGM_TOKEN,
    "",
    ConfigInfo::CI_DEPRICATED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },
  
  {
    CFG_NODE_ID,
    "NodeId",
    MGM_TOKEN,
    "Number identifying the management server node ("MGM_TOKEN_PRINT")",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },
  
  {
    CFG_LOG_DESTINATION,
    "LogDestination",
    MGM_TOKEN,
    "String describing where logmessages are sent",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },
  
  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    MGM_TOKEN,
    "String referencing an earlier defined COMPUTER",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    KEY_INTERNAL,
    "MaxNoOfSavedEvents",
    MGM_TOKEN,
    "",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "100",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_MGM_PORT,
    "PortNumber",
    MGM_TOKEN,
    "Port number to give commands to/fetch configurations from management server",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    NDB_PORT,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    KEY_INTERNAL,
    "PortNumberStats",
    MGM_TOKEN,
    "Port number used to get statistical information from a management server",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    UNDEFINED,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_NODE_ARBIT_RANK,
    "ArbitrationRank",
    MGM_TOKEN,
    "If 0, then "MGM_TOKEN_PRINT" is not arbitrator. Kernel selects arbitrators in order 1, 2",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1",
    "0",
    "2" },

  {
    CFG_NODE_ARBIT_DELAY,
    "ArbitrationDelay",
    MGM_TOKEN,
    "",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  /****************************************************************************
   * TCP
   ***************************************************************************/
  {
    CFG_SECTION_CONNECTION,
    "TCP",
    "TCP",
    "Connection section",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)CONNECTION_TYPE_TCP, 
    0, 0
  },

  {
    CFG_CONNECTION_HOSTNAME_1,
    "HostName1",
    "TCP",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "TCP",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "TCP",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "TCP",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },

  {
    CFG_CONNECTION_GROUP,
    "Group",
    "TCP",
    "",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "55",
    "0", "200" },

  {
    CFG_CONNECTION_NODE_ID_SERVER,
    "NodeIdServer",
    "TCP",
    "",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1", "63" },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "TCP",
    "Sends id in each signal.  Used in trace files.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "true",
    "false",
    "true" },


  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "TCP",
    "If checksum is enabled, all signals between nodes are checked for errors",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "TCP",
    "Port used for this transporter",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_TCP_SEND_BUFFER_SIZE,
    "SendBufferMemory",
    "TCP",
    "Bytes of buffer for signals sent from this node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "256K",
    "64K",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_TCP_RECEIVE_BUFFER_SIZE,
    "ReceiveBufferMemory",
    "TCP",
    "Bytes of buffer for signals received by this node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "64K",
    "16K",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_TCP_PROXY,
    "Proxy",
    "TCP",
    "",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "TCP",
    "System for node 1 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "TCP",
    "System for node 2 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },
  

  /****************************************************************************
   * SHM
   ***************************************************************************/
  {
    CFG_SECTION_CONNECTION,
    "SHM",
    "SHM",
    "Connection section",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)CONNECTION_TYPE_SHM, 
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_1,
    "HostName1",
    "SHM",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "SHM",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "SHM",
    "Port used for this transporter",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "0", 
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SHM_SIGNUM,
    "Signum",
    "SHM",
    "Signum to be used for signalling",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    UNDEFINED,
    "0", 
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "SHM",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },
  
  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "SHM",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },
  
  {
    CFG_CONNECTION_GROUP,
    "Group",
    "SHM",
    "",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "35",
    "0", "200" },

  {
    CFG_CONNECTION_NODE_ID_SERVER,
    "NodeIdServer",
    "SHM",
    "",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1", "63" },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "SHM",
    "Sends id in each signal.  Used in trace files.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true" },
  
  
  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "SHM",
    "If checksum is enabled, all signals between nodes are checked for errors",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "true",
    "false",
    "true" },
  
  {
    CFG_SHM_KEY,
    "ShmKey",
    "SHM",
    "A shared memory key",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    UNDEFINED,
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_SHM_BUFFER_MEM,
    "ShmSize",
    "SHM",
    "Size of shared memory segment",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1M",
    "64K",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "SHM",
    "System for node 1 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "SHM",
    "System for node 2 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  /****************************************************************************
   * SCI
   ***************************************************************************/
  {
    CFG_SECTION_CONNECTION,
    "SCI",
    "SCI",
    "Connection section",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)CONNECTION_TYPE_SCI, 
    0, 0 
  },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "SCI",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "SCI",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_GROUP,
    "Group",
    "SCI",
    "",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "15",
    "0", "200" },

  {
    CFG_CONNECTION_NODE_ID_SERVER,
    "NodeIdServer",
    "SCI",
    "",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1", "63" },

  {
    CFG_CONNECTION_HOSTNAME_1,
    "HostName1",
    "SCI",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "SCI",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "SCI",
    "Port used for this transporter",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "0", 
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST1_ID_0,
    "Host1SciId0",
    "SCI",
    "SCI-node id for adapter 0 on Host1 (a computer can have two adapters)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST1_ID_1,
    "Host1SciId1",
    "SCI",
    "SCI-node id for adapter 1 on Host1 (a computer can have two adapters)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST2_ID_0,
    "Host2SciId0",
    "SCI",
    "SCI-node id for adapter 0 on Host2 (a computer can have two adapters)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST2_ID_1,
    "Host2SciId1",
    "SCI",
    "SCI-node id for adapter 1 on Host2 (a computer can have two adapters)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "SCI",
    "Sends id in each signal.  Used in trace files.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "true",
    "false",
    "true" },

  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "SCI",
    "If checksum is enabled, all signals between nodes are checked for errors",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_SCI_SEND_LIMIT,
    "SendLimit",
    "SCI",
    "Transporter send buffer contents are sent when this no of bytes is buffered",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "8K",
    "128",
    "32K" },

  {
    CFG_SCI_BUFFER_MEM,
    "SharedBufferSize",
    "SCI",
    "Size of shared memory segment",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1M",
    "64K",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "SCI",
    "System for node 1 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "SCI",
    "System for node 2 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  /****************************************************************************
   * OSE
   ***************************************************************************/
  {
    CFG_SECTION_CONNECTION,
    "OSE",
    "OSE",
    "Connection section",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)CONNECTION_TYPE_OSE, 
    0, 0 
  },

  {
    CFG_CONNECTION_HOSTNAME_1,
    "HostName1",
    "OSE",
    "Name of computer on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "OSE",
    "Name of computer on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "OSE",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "OSE",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    UNDEFINED,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "OSE",
    "Sends id in each signal.  Used in trace files.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "true",
    "false",
    "true" },

  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "OSE",
    "If checksum is enabled, all signals between nodes are checked for errors",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_OSE_PRIO_A_SIZE,
    "PrioASignalSize",
    "OSE",
    "Size of priority A signals (in bytes)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1000",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_OSE_PRIO_B_SIZE,
    "PrioBSignalSize",
    "OSE",
    "Size of priority B signals (in bytes)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1000",
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_OSE_RECEIVE_ARRAY_SIZE,
    "ReceiveArraySize",
    "OSE",
    "Number of OSE signals checked for correct ordering (in no of OSE signals)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "10",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "OSE",
    "System for node 1 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "OSE",
    "System for node 2 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    UNDEFINED,
    0, 0 },
};

const int ConfigInfo::m_NoOfParams = sizeof(m_ParamInfo) / sizeof(ParamInfo);


/****************************************************************************
 * Ctor
 ****************************************************************************/
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

ConfigInfo::ConfigInfo()
  : m_info(true), m_systemDefaults(true)
{
  int i;
  Properties *section;
  const Properties *oldpinfo;

  for (i=0; i<m_NoOfParams; i++) {
    const ParamInfo & param = m_ParamInfo[i];
    Uint64 default_uint64;
    bool   default_bool;
    
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

    if(param._default == MANDATORY){
      pinfo.put("Mandatory", (Uint32)1);
    }

    switch (param._type) {
      case CI_BOOL:
      {
	bool tmp_bool;
	require(InitConfigFileParser::convertStringToBool(param._min, tmp_bool));
	pinfo.put64("Min", tmp_bool);
	require(InitConfigFileParser::convertStringToBool(param._max, tmp_bool));
	pinfo.put64("Max", tmp_bool);
	break;
      }
      case CI_INT:
      case CI_INT64:
      {
	Uint64 tmp_uint64;
	require(InitConfigFileParser::convertStringToUint64(param._min, tmp_uint64));
	pinfo.put64("Min", tmp_uint64);
	require(InitConfigFileParser::convertStringToUint64(param._max, tmp_uint64));
	pinfo.put64("Max", tmp_uint64);
	break;
      }
      case CI_SECTION:
	pinfo.put("SectionType", (Uint32)UintPtr(param._default));
	break;
      case CI_STRING:
	break;
    }

    // Check that pinfo is really new
    if (section->get(param._fname, &oldpinfo)) {
      ndbout << "Error: Parameter " << param._fname
	     << " defined twice in section " << param._section
	     << "." << endl;
      require(false);
    }
    
    // Add new pinfo to section
    section->put(param._fname, &pinfo);

    // Replace section with modified section
    m_info.put(param._section, section, true);
    
    if(param._type != ConfigInfo::CI_SECTION){
      Properties * p;
      if(!m_systemDefaults.getCopy(param._section, &p)){
	p = new Properties(true);
      }
      if(param._default != UNDEFINED &&
	 param._default != MANDATORY){
	switch (param._type)
        {
	  case CI_SECTION:
	    break;
	  case CI_STRING:
	    require(p->put(param._fname, param._default));
	    break;
	  case CI_BOOL:
	    {
	      bool tmp_bool;
	      require(InitConfigFileParser::convertStringToBool(param._default, default_bool));
	      require(p->put(param._fname, default_bool));
	      break;
	    }
	  case CI_INT:
	  case CI_INT64:
	    {
	      Uint64 tmp_uint64;
	      require(InitConfigFileParser::convertStringToUint64(param._default, default_uint64));
	      require(p->put(param._fname, default_uint64));
	      break;
	    }
	}
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
      require(false);
    }
    
    if(m_ParamInfo[i]._type == ConfigInfo::CI_SECTION)
      continue;

    const Properties * p = getInfo(m_ParamInfo[i]._section);
    if (!p || !p->contains(m_ParamInfo[i]._fname)) {
      ndbout << "Check that each pname has an fname failed." << endl;
      ndbout << "Parameter \"" << m_ParamInfo[i]._fname 
	     << "\" does not exist in section \"" 
	     << m_ParamInfo[i]._section << "\"." << endl;
      ndbout << "Edit file " << __FILE__ << "." << endl;
      require(false);
    }
  }
}

/****************************************************************************
 * Getters
 ****************************************************************************/
inline void warning(const char * src, const char * arg){
  ndbout << "Illegal call to ConfigInfo::" << src << "() - " << arg << endl;
  require(false);
}

const Properties * 
ConfigInfo::getInfo(const char * section) const {
  const Properties * p;
  if(!m_info.get(section, &p)){
    return 0;
    //    warning("getInfo", section);
  }
  return p;
}

const Properties * 
ConfigInfo::getDefaults(const char * section) const {
  const Properties * p;
  if(!m_systemDefaults.get(section, &p)){
    return 0;
    //warning("getDefaults", section);
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
    if(!strcasecmp(section, m_sectionNames[i])) return true;
  }
  return false;
}

const char*
ConfigInfo::nameToAlias(const char * name) {
  for (int i = 0; m_sectionNameAliases[i].name != 0; i++)
    if(!strcasecmp(name, m_sectionNameAliases[i].name))
      return m_sectionNameAliases[i].alias;
  return 0;
}

const char*
ConfigInfo::getAlias(const char * section) {
  for (int i = 0; m_sectionNameAliases[i].name != 0; i++)
    if(!strcasecmp(section, m_sectionNameAliases[i].alias))
      return m_sectionNameAliases[i].name;
  return 0;
}

bool
ConfigInfo::verify(const Properties * section, const char* fname, 
		   Uint64 value) const {
  Uint64 min, max;

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
    if (getStatus(sec, n) == ConfigInfo::CI_INTERNAL) continue;
    if (getStatus(sec, n) == ConfigInfo::CI_DEPRICATED) continue;
    if (getStatus(sec, n) == ConfigInfo::CI_NOTIMPLEMENTED) continue;
    print(sec, n);
  }
}

void ConfigInfo::print(const Properties * section, 
		       const char* parameter) const {
  ndbout << parameter;
  //  ndbout << getDescription(section, parameter) << endl;
  switch (getType(section, parameter)) {
  case ConfigInfo::CI_BOOL:
    ndbout << " (Boolean value)" << endl;
    ndbout << getDescription(section, parameter) << endl;
    if (getDefault(section, parameter) == false) {
      ndbout << "Default: N (Legal values: Y, N)" << endl; 
    } else if (getDefault(section, parameter) == true) {
      ndbout << "Default: Y (Legal values: Y, N)" << endl;
    } else if (getDefault(section, parameter) == (UintPtr)MANDATORY) {
      ndbout << "MANDATORY (Legal values: Y, N)" << endl;
    } else {
      ndbout << "UNKNOWN" << endl;
    }
    ndbout << endl;
    break;    
    
  case ConfigInfo::CI_INT:
  case ConfigInfo::CI_INT64:
    ndbout << " (Non-negative Integer)" << endl;
    ndbout << getDescription(section, parameter) << endl;
    if (getDefault(section, parameter) == (UintPtr)MANDATORY) {
      ndbout << "MANDATORY (";
    } else if (getDefault(section, parameter) == (UintPtr)UNDEFINED) {
      ndbout << "UNDEFINED (";
    } else {
      ndbout << "Default: " << getDefault(section, parameter) << " (";
    }
    ndbout << "Min: " << getMin(section, parameter) << ", ";
    ndbout << "Max: " << getMax(section, parameter) << ")" << endl;
    ndbout << endl;
    break;
    
  case ConfigInfo::CI_STRING:
    ndbout << " (String)" << endl;
    ndbout << getDescription(section, parameter) << endl;
    if (getDefault(section, parameter) == (UintPtr)MANDATORY) {
      ndbout << "MANDATORY" << endl;
    } else {
      ndbout << "No default value" << endl;
    }
    ndbout << endl;
    break;
  case ConfigInfo::CI_SECTION:
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

  Uint32 id, line;
  if(!ctx.m_currentSection->get("NodeId", &id) && !ctx.m_currentSection->get("Id", &id)){
    Uint32 nextNodeId= 1;
    ctx.m_userProperties.get("NextNodeId", &nextNodeId);
    id= nextNodeId;
    while (ctx.m_userProperties.get("AllocatedNodeId_", id, &line))
      id++;
    if (id != nextNodeId)
    {
      ndbout_c("Cluster configuration warning line %d: "
	       "Could not use next node id %d for section [%s], "
	       "using next unused node id %d.",
	       ctx.m_sectionLineno, nextNodeId, ctx.fname, id);
    }
    ctx.m_currentSection->put("NodeId", id);
  } else if(ctx.m_userProperties.get("AllocatedNodeId_", id, &line)) {
    ctx.reportError("Duplicate nodeid in section "
		    "[%s] starting at line: %d. Previously used on line %d.",
		    ctx.fname, ctx.m_sectionLineno, line);
    return false;
  }

  // next node id _always_ next numbers after last used id
  ctx.m_userProperties.put("NextNodeId", id+1, true);

  ctx.m_userProperties.put("AllocatedNodeId_", id, ctx.m_sectionLineno);
  BaseString::snprintf(ctx.pname, sizeof(ctx.pname), "Node_%d", id);
  
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

static bool checkLocalhostHostnameMix(InitConfigFileParser::Context & ctx, const char * data)
{
  DBUG_ENTER("checkLocalhostHostnameMix");
  const char * hostname= 0;
  ctx.m_currentSection->get("HostName", &hostname);
  if (hostname == 0 || hostname[0] == 0)
    DBUG_RETURN(true);

  Uint32 localhost_used= 0;
  if(!strcmp(hostname, "localhost") || !strcmp(hostname, "127.0.0.1")){
    localhost_used= 1;
    ctx.m_userProperties.put("$computer-localhost-used", localhost_used);
    if(!ctx.m_userProperties.get("$computer-localhost", &hostname))
      DBUG_RETURN(true);
  } else {
    ctx.m_userProperties.get("$computer-localhost-used", &localhost_used);
    ctx.m_userProperties.put("$computer-localhost", hostname);
  }

  if (localhost_used) {
    ctx.reportError("Mixing of localhost (default for [NDBD]HostName) with other hostname(%s) is illegal",
		    hostname);
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}

bool
fixNodeHostname(InitConfigFileParser::Context & ctx, const char * data)
{
  const char * hostname;
  DBUG_ENTER("fixNodeHostname");

  if (ctx.m_currentSection->get("HostName", &hostname))
    DBUG_RETURN(checkLocalhostHostnameMix(ctx,0));

  const char * compId;
  if(!ctx.m_currentSection->get("ExecuteOnComputer", &compId))
    DBUG_RETURN(true);
  
  const Properties * computer;
  char tmp[255];
  BaseString::snprintf(tmp, sizeof(tmp), "Computer_%s", compId);
  if(!ctx.m_config->get(tmp, &computer)){
    ctx.reportError("Computer \"%s\" not declared"
		    "- [%s] starting at line: %d",
		    compId, ctx.fname, ctx.m_sectionLineno);
    DBUG_RETURN(false);
  }
  
  if(!computer->get("HostName", &hostname)){
    ctx.reportError("HostName missing in [COMPUTER] (Id: %d) "
		    " - [%s] starting at line: %d",
		    compId, ctx.fname, ctx.m_sectionLineno);
    DBUG_RETURN(false);
  }
  
  require(ctx.m_currentSection->put("HostName", hostname));
  DBUG_RETURN(checkLocalhostHostnameMix(ctx,0));
}

bool
fixFileSystemPath(InitConfigFileParser::Context & ctx, const char * data){
  DBUG_ENTER("fixFileSystemPath");

  const char * path;
  if (ctx.m_currentSection->get("FileSystemPath", &path))
    DBUG_RETURN(true);

  if (ctx.m_currentSection->get("DataDir", &path)) {
    require(ctx.m_currentSection->put("FileSystemPath", path));
    DBUG_RETURN(true);
  }

  require(false);
  DBUG_RETURN(false);
}

bool
fixBackupDataDir(InitConfigFileParser::Context & ctx, const char * data){
  
  const char * path;
  if (ctx.m_currentSection->get("BackupDataDir", &path))
    return true;

  if (ctx.m_currentSection->get("FileSystemPath", &path)) {
    require(ctx.m_currentSection->put("BackupDataDir", path));
    return true;
  }

  require(false);
  return false;
}

/**
 * Connection rule: Check support of connection
 */
bool
checkConnectionSupport(InitConfigFileParser::Context & ctx, const char * data)
{
  int error= 0;
  if (strcasecmp("TCP",ctx.fname) == 0)
  {
    // always enabled
  }
  else if (strcasecmp("SHM",ctx.fname) == 0)
  {
#ifndef NDB_SHM_TRANSPORTER
    error= 1;
#endif
  }
  else if (strcasecmp("SCI",ctx.fname) == 0)
  {
#ifndef NDB_SCI_TRANSPORTER
    error= 1;
#endif
  }
  else if (strcasecmp("OSE",ctx.fname) == 0)
  {
#ifndef NDB_OSE_TRANSPORTER
    error= 1;
#endif
  }
  if (error)
  {
    ctx.reportError("Binary not compiled with this connection support, "
		    "[%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  return true;
}

/**
 * Connection rule: Update "NoOfConnections"
 */
bool
transformConnection(InitConfigFileParser::Context & ctx, const char * data)
{
  Uint32 connections = 0;
  ctx.m_userProperties.get("NoOfConnections", &connections);
  BaseString::snprintf(ctx.pname, sizeof(ctx.pname), "Connection_%d", connections);
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

  BaseString::snprintf(ctx.pname, sizeof(ctx.pname), "SYSTEM_%s", name);
  
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
  BaseString::snprintf(ctx.pname, sizeof(ctx.pname), "Computer_%s", id);
  
  Uint32 computers = 0;
  ctx.m_userProperties.get("NoOfComputers", &computers);
  ctx.m_userProperties.put("NoOfComputers", ++computers, true);
  
  const char * hostname = 0;
  ctx.m_currentSection->get("HostName", &hostname);
  if(!hostname){
    return true;
  }
  
  return checkLocalhostHostnameMix(ctx,0);
}

/**
 * Apply default values
 */
void 
applyDefaultValues(InitConfigFileParser::Context & ctx,
		   const Properties * defaults)
{
  DBUG_ENTER("applyDefaultValues");
  if(defaults != NULL){
    Properties::Iterator it(defaults);

    for(const char * name = it.first(); name != NULL; name = it.next()){
      ConfigInfo::Status st = ctx.m_info->getStatus(ctx.m_currentInfo, name);
      if(!ctx.m_currentSection->contains(name)){
	switch (ctx.m_info->getType(ctx.m_currentInfo, name)){
	case ConfigInfo::CI_INT:
	case ConfigInfo::CI_BOOL:{
	  Uint32 val = 0;
	  ::require(defaults->get(name, &val));
	  ctx.m_currentSection->put(name, val);
          DBUG_PRINT("info",("%s=%d #default",name,val));
	  break;
	}
	case ConfigInfo::CI_INT64:{
	  Uint64 val = 0;
	  ::require(defaults->get(name, &val));
	  ctx.m_currentSection->put64(name, val);
          DBUG_PRINT("info",("%s=%lld #default",name,val));
	  break;
	}
	case ConfigInfo::CI_STRING:{
	  const char * val;
	  ::require(defaults->get(name, &val));
	  ctx.m_currentSection->put(name, val);
          DBUG_PRINT("info",("%s=%s #default",name,val));
	  break;
	}
	case ConfigInfo::CI_SECTION:
	  break;
	}
      }
#ifndef DBUG_OFF
      else
      {
        switch (ctx.m_info->getType(ctx.m_currentInfo, name)){
        case ConfigInfo::CI_INT:
        case ConfigInfo::CI_BOOL:{
          Uint32 val = 0;
          ::require(ctx.m_currentSection->get(name, &val));
          DBUG_PRINT("info",("%s=%d",name,val));
          break;
        }
        case ConfigInfo::CI_INT64:{
          Uint64 val = 0;
          ::require(ctx.m_currentSection->get(name, &val));
          DBUG_PRINT("info",("%s=%lld",name,val));
          break;
        }
        case ConfigInfo::CI_STRING:{
          const char * val;
          ::require(ctx.m_currentSection->get(name, &val));
          DBUG_PRINT("info",("%s=%s",name,val));
          break;
        }
        case ConfigInfo::CI_SECTION:
          break;
        }
      }
#endif
    }
  }
  DBUG_VOID_RETURN;
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
    if(info->get("Mandatory", &val)){
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
    if(!ctx.m_config->get("Node", id, &node))
    {
      ctx.reportError("Unknown node: \"%d\" specified in connection "
		      "[%s] starting at line: %d",
		      id, ctx.fname, ctx.m_sectionLineno);
      return false;
    }
    
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

  DBUG_ENTER("fixPortNumber");

  Uint32 id1, id2;
  const char *hostName1;
  const char *hostName2;
  require(ctx.m_currentSection->get("NodeId1", &id1));
  require(ctx.m_currentSection->get("NodeId2", &id2));
  require(ctx.m_currentSection->get("HostName1", &hostName1));
  require(ctx.m_currentSection->get("HostName2", &hostName2));
  DBUG_PRINT("info",("NodeId1=%d HostName1=\"%s\"",id1,hostName1));
  DBUG_PRINT("info",("NodeId2=%d HostName2=\"%s\"",id2,hostName2));

  const Properties *node1, *node2;
  require(ctx.m_config->get("Node", id1, &node1));
  require(ctx.m_config->get("Node", id2, &node2));

  const char *type1, *type2;
  require(node1->get("Type", &type1));
  require(node2->get("Type", &type2));

  /* add NodeIdServer info */
  {
    Uint32 nodeIdServer = id1 < id2 ? id1 : id2;
    if(strcmp(type1, API_TOKEN) == 0 || strcmp(type2, MGM_TOKEN) == 0)
      nodeIdServer = id2;
    else if(strcmp(type2, API_TOKEN) == 0 || strcmp(type1, MGM_TOKEN) == 0)
      nodeIdServer = id1;
    ctx.m_currentSection->put("NodeIdServer", nodeIdServer);

    if (id2 == nodeIdServer) {
      {
	const char *tmp= hostName1;
	hostName1= hostName2;
	hostName2= tmp;
      }
      {
	Uint32 tmp= id1;
	id1= id2;
	id2= tmp;
      }
      {
	const Properties *tmp= node1;
	node1= node2;
	node2= tmp;
      }
      {
	const char *tmp= type1;
	type1= type2;
	type2= tmp;
      }
    }
  }

  BaseString hostname(hostName1);
  
  if (hostname.c_str()[0] == 0) {
    ctx.reportError("Hostname required on nodeid %d since it will "
		    "act as server.", id1);
    DBUG_RETURN(false);
  }

  Uint32 port= 0;
  if(strcmp(type1, MGM_TOKEN)==0)
    node1->get("PortNumber",&port);
  else if(strcmp(type2, MGM_TOKEN)==0)
    node2->get("PortNumber",&port);

  if (!port && 
      !node1->get("ServerPort", &port) &&
      !ctx.m_userProperties.get("ServerPort_", id1, &port))
  {
    Uint32 base= 0;
    /*
     * If the connection doesn't involve an mgm server,
     * and a default port number has been set, behave the old
     * way of allocating port numbers for transporters.
     */
    if(ctx.m_userDefaults && ctx.m_userDefaults->get("PortNumber", &base))
    {
      Uint32 adder= 0;
      {
	BaseString server_port_adder(hostname);
	server_port_adder.append("_ServerPortAdder");
	ctx.m_userProperties.get(server_port_adder.c_str(), &adder);
	ctx.m_userProperties.put(server_port_adder.c_str(), adder+1, true);
      }
      
      if (!ctx.m_userProperties.get("ServerPortBase", &base)){
	if(!(ctx.m_userDefaults &&
	   ctx.m_userDefaults->get("PortNumber", &base)) &&
	   !ctx.m_systemDefaults->get("PortNumber", &base)) {
	  base= strtoll(NDB_TCP_BASE_PORT,0,0);
	}
	ctx.m_userProperties.put("ServerPortBase", base);
      }

      port= base + adder;
      ctx.m_userProperties.put("ServerPort_", id1, port);
    }
  }

  if(ctx.m_currentSection->contains("PortNumber")) {
    ndbout << "PortNumber should no longer be specificied "
	   << "per connection, please remove from config. "
	   << "Will be changed to " << port << endl;
    ctx.m_currentSection->put("PortNumber", port, true);
  } 
  else
  {
    ctx.m_currentSection->put("PortNumber", port);
  }

  DBUG_PRINT("info", ("connection %d-%d port %d host %s",
		      id1, id2, port, hostname.c_str()));

  DBUG_RETURN(true);
}

static bool 
fixShmUniqueId(InitConfigFileParser::Context & ctx, const char * data)
{
  DBUG_ENTER("fixShmUniqueId");
  Uint32 nodes= 0;
  ctx.m_userProperties.get(ctx.fname, &nodes);
  if (nodes == 1) // first management server
  {
    Uint32 portno= atoi(NDB_PORT);
    ctx.m_currentSection->get("PortNumber", &portno);
    ctx.m_userProperties.put("ShmUniqueId", portno);
  }
  DBUG_RETURN(true);
}

static 
bool 
fixShmKey(InitConfigFileParser::Context & ctx, const char *)
{
  DBUG_ENTER("fixShmKey");
  {
    static int last_signum= -1;
    Uint32 signum;
    if(!ctx.m_currentSection->get("Signum", &signum))
    {
      signum= OPT_NDB_SHM_SIGNUM_DEFAULT;
      if (signum <= 0)
      {
	  ctx.reportError("Unable to set default parameter for [SHM]Signum"
			  " please specify [SHM DEFAULT]Signum");
	  return false;
      }
      ctx.m_currentSection->put("Signum", signum);
      DBUG_PRINT("info",("Added Signum=%u", signum));
    }
    if ( last_signum != (int)signum && last_signum >= 0 )
    {
      ctx.reportError("All shared memory transporters must have same [SHM]Signum defined."
		      " Use [SHM DEFAULT]Signum");
      return false;
    }
    last_signum= (int)signum;
  }
  {
    Uint32 id1= 0, id2= 0, key= 0;
    require(ctx.m_currentSection->get("NodeId1", &id1));
    require(ctx.m_currentSection->get("NodeId2", &id2));
    if(!ctx.m_currentSection->get("ShmKey", &key))
    {
      require(ctx.m_userProperties.get("ShmUniqueId", &key));
      key= key << 16 | (id1 > id2 ? id1 << 8 | id2 : id2 << 8 | id1);
      ctx.m_currentSection->put("ShmKey", key);
      DBUG_PRINT("info",("Added ShmKey=0x%x", key));
    }
  }
  DBUG_RETURN(true);
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
   */
  if((strcmp(type1, DB_TOKEN) != 0 && strcmp(type2, DB_TOKEN) != 0) &&
     !(strcmp(type1, MGM_TOKEN) == 0 && strcmp(type2, MGM_TOKEN) == 0))
  {
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
       && (newType == ConfigInfo::CI_INT || newType == ConfigInfo::CI_INT64 || newType == ConfigInfo::CI_BOOL))){
    ndbout << "oldType: " << (int)oldType << ", newType: " << (int)newType << endl;
    ctx.reportError("Unable to handle type conversion w.r.t deprication %s %s"
		    "- [%s] starting at line: %d",
		    oldName, newName,
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  Uint64 oldVal;
  require(ctx.m_currentSection->get(oldName, &oldVal));

  Uint64 newVal = (Uint64)((Int64)oldVal * mul + add);
  if(!ctx.m_info->verify(ctx.m_currentInfo, newName, newVal)){
    ctx.reportError("Unable to handle deprication, new value not within bounds"
		    "%s %s - [%s] starting at line: %d",
		    oldName, newName,
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  if(newType == ConfigInfo::CI_INT || newType == ConfigInfo::CI_BOOL){
    require(dst.put(newName, (Uint32)newVal));
  } else if(newType == ConfigInfo::CI_INT64) {
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
	if(strcasecmp(name, p->m_oldName) == 0){
	  if(!transform(ctx, tmp, name, p->m_newName, add, mul)){
	    return false;
	  }
	} else if(strcasecmp(name, p->m_newName) == 0) {
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
      ::require(false);
    }
  }
  return true;
}

extern int g_print_full_config;

static bool
saveInConfigValues(InitConfigFileParser::Context & ctx, const char * data){
  const Properties * sec;
  if(!ctx.m_currentInfo->get(ctx.fname, &sec)){
    require(false);
    return false;
  }
  
  do {
    const char *secName;
    Uint32 id, status, typeVal;
    require(sec->get("Fname", &secName));
    require(sec->get("Id", &id));
    require(sec->get("Status", &status));
    require(sec->get("SectionType", &typeVal));
    
    if(id == KEY_INTERNAL || status == ConfigInfo::CI_INTERNAL){
      ndbout_c("skipping section %s", ctx.fname);
      break;
    }
    
    if (g_print_full_config)
    {
      const char *alias= ConfigInfo::nameToAlias(ctx.fname);
      printf("[%s]\n", alias ? alias : ctx.fname);
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
	if (g_print_full_config)
	  printf("%s=%u\n", n, val);
	break;
      }
      case PropertiesType_Uint64:{
	Uint64 val;
	require(ctx.m_currentSection->get(n, &val));
	ok = ctx.m_configValues.put64(id, val);
	if (g_print_full_config)
	  printf("%s=%llu\n", n, val);
	break;
      }
      case PropertiesType_char:{
	const char * val;
	require(ctx.m_currentSection->get(n, &val));
	ok = ctx.m_configValues.put(id, val);
	if (g_print_full_config)
	  printf("%s=%s\n", n, val);
	break;
      }
      default:
	require(false);
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

static void
add_a_connection(Vector<ConfigInfo::ConfigRuleSection>&sections,
		 struct InitConfigFileParser::Context &ctx,
		 Uint32 nodeId1, Uint32 nodeId2, bool use_shm)
{
  ConfigInfo::ConfigRuleSection s;
  const char *hostname1= 0, *hostname2= 0;
  const Properties *tmp;
  
  require(ctx.m_config->get("Node", nodeId1, &tmp));
  tmp->get("HostName", &hostname1);
  
  require(ctx.m_config->get("Node", nodeId2, &tmp));
  tmp->get("HostName", &hostname2);
  
  char buf[16];
  s.m_sectionData= new Properties(true);
  BaseString::snprintf(buf, sizeof(buf), "%u", nodeId1);
  s.m_sectionData->put("NodeId1", buf);
  BaseString::snprintf(buf, sizeof(buf), "%u", nodeId2);
  s.m_sectionData->put("NodeId2", buf);

  if (use_shm &&
      hostname1 && hostname1[0] &&
      hostname2 && hostname2[0] &&
      strcmp(hostname1,hostname2) == 0)
  {
    s.m_sectionType= BaseString("SHM");
    DBUG_PRINT("info",("adding SHM connection %d %d",nodeId1,nodeId2));
  }
  else
  {
    s.m_sectionType= BaseString("TCP");
    DBUG_PRINT("info",("adding TCP connection %d %d",nodeId1,nodeId2));
  }

  sections.push_back(s);
}

static bool
add_node_connections(Vector<ConfigInfo::ConfigRuleSection>&sections, 
		   struct InitConfigFileParser::Context &ctx, 
		   const char * rule_data)
{
  DBUG_ENTER("add_node_connections");
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
  Properties p_api_nodes(true);
  Properties p_mgm_nodes(true);

  Uint32 i_db= 0, i_api= 0, i_mgm= 0, n;
  for (i= 0, n= 0; n < nNodes; i++){
    const Properties * tmp;
    if(!props->get("Node", i, &tmp)) continue;
    n++;

    const char * type;
    if(!tmp->get("Type", &type)) continue;

    if (strcmp(type,DB_TOKEN) == 0)
      p_db_nodes.put("", i_db++, i);
    else if (strcmp(type,API_TOKEN) == 0)
      p_api_nodes.put("", i_api++, i);
    else if (strcmp(type,MGM_TOKEN) == 0)
      p_mgm_nodes.put("", i_mgm++, i);
  }

  Uint32 nodeId1, nodeId2, dummy;

  for (i= 0; p_db_nodes.get("", i, &nodeId1); i++){
    for (Uint32 j= i+1;; j++){
      if(!p_db_nodes.get("", j, &nodeId2)) break;
      if(!p_connections2.get("", nodeId1+nodeId2<<16, &dummy)) {
	add_a_connection(sections,ctx,nodeId1,nodeId2,opt_ndb_shm);
      }
    }
  }

  for (i= 0; p_api_nodes.get("", i, &nodeId1); i++){
    if(!p_connections.get("", nodeId1, &dummy)) {
      for (Uint32 j= 0;; j++){
	if(!p_db_nodes.get("", j, &nodeId2)) break;
	add_a_connection(sections,ctx,nodeId1,nodeId2,opt_ndb_shm);
      }
    }
  }

  for (i= 0; p_mgm_nodes.get("", i, &nodeId1); i++){
    if(!p_connections.get("", nodeId1, &dummy)) {
      for (Uint32 j= 0;; j++){
	if(!p_db_nodes.get("", j, &nodeId2)) break;
	add_a_connection(sections,ctx,nodeId1,nodeId2,0);
      }
    }
  }

  DBUG_RETURN(true);
}

static bool set_connection_priorities(Vector<ConfigInfo::ConfigRuleSection>&sections, 
				 struct InitConfigFileParser::Context &ctx, 
				 const char * rule_data)
{
  DBUG_ENTER("set_connection_priorities");
  DBUG_RETURN(true);
}

static bool
check_node_vs_replicas(Vector<ConfigInfo::ConfigRuleSection>&sections, 
		       struct InitConfigFileParser::Context &ctx, 
		       const char * rule_data)
{
  Uint32 db_nodes= 0;
  Uint32 replicas= 0;
  Uint32 db_host_count= 0;
  ctx.m_userProperties.get(DB_TOKEN, &db_nodes);
  ctx.m_userProperties.get("NoOfReplicas", &replicas);
  if((db_nodes % replicas) != 0){
    ctx.reportError("Invalid no of db nodes wrt no of replicas.\n"
		    "No of nodes must be dividable with no or replicas");
    return false;
  }
  // check that node groups and arbitrators are ok
  // just issue warning if not
  if(replicas > 1){
    Properties * props= ctx.m_config;
    Properties p_db_hosts(true); // store hosts which db nodes run on
    Properties p_arbitrators(true); // store hosts which arbitrators run on
    // arbitrator should not run together with db node on same host
    Uint32 i, n, group= 0, i_group= 0;
    Uint32 n_nodes;
    BaseString node_group_warning, arbitration_warning;
    const char *arbit_warn_fmt=
      "\n  arbitrator with id %d and db node with id %d on same host %s";
    const char *arbit_warn_fmt2=
      "\n  arbitrator with id %d has no hostname specified";

    ctx.m_userProperties.get("NoOfNodes", &n_nodes);
    for (i= 0, n= 0; n < n_nodes; i++){
      const Properties * tmp;
      if(!props->get("Node", i, &tmp)) continue;
      n++;

      const char * type;
      if(!tmp->get("Type", &type)) continue;

      const char* host= 0;
      tmp->get("HostName", &host);

      if (strcmp(type,DB_TOKEN) == 0)
      {
	{
	  Uint32 ii;
	  if (!p_db_hosts.get(host,&ii))
	    db_host_count++;
	  p_db_hosts.put(host,i);
	  if (p_arbitrators.get(host,&ii))
	  {
	    arbitration_warning.appfmt(arbit_warn_fmt, ii, i, host);
	    p_arbitrators.remove(host); // only one warning per db node
	  }
	}
	{
	  unsigned j;
	  BaseString str, str2;
	  str.assfmt("#group%d_",group);
	  p_db_hosts.put(str.c_str(),i_group,host);
	  str2.assfmt("##group%d_",group);
	  p_db_hosts.put(str2.c_str(),i_group,i);
	  for (j= 0; j < i_group; j++)
	  {
	    const char *other_host;
	    p_db_hosts.get(str.c_str(),j,&other_host);
	    if (strcmp(host,other_host) == 0) {
	      unsigned int other_i, c= 0;
	      p_db_hosts.get(str2.c_str(),j,&other_i);
	      p_db_hosts.get(str.c_str(),&c);
	      if (c == 0) // first warning in this node group
		node_group_warning.appfmt("  Node group %d", group);
	      c|= 1 << j;
	      p_db_hosts.put(str.c_str(),c);

	      node_group_warning.appfmt(",\n    db node with id %d and id %d "
					"on same host %s", other_i, i, host);
	    }
	  }
	  i_group++;
	  DBUG_ASSERT(i_group <= replicas);
	  if (i_group == replicas)
	  {
	    unsigned c= 0;
	    p_db_hosts.get(str.c_str(),&c);
	    if (c+1 == (1u << (replicas-1))) // all nodes on same machine
	      node_group_warning.append(".\n    Host failure will "
					"cause complete cluster shutdown.");
	    else if (c > 0)
	      node_group_warning.append(".\n    Host failure may "
					"cause complete cluster shutdown.");
	    group++;
	    i_group= 0;
	  }
	}
      }
      else if (strcmp(type,API_TOKEN) == 0 ||
	       strcmp(type,MGM_TOKEN) == 0)
      {
	Uint32 rank;
	if(tmp->get("ArbitrationRank", &rank) && rank > 0)
	{
	  if(host && host[0] != 0)
	  {
	    Uint32 ii;
	    p_arbitrators.put(host,i);
	    if (p_db_hosts.get(host,&ii))
	    {
	      arbitration_warning.appfmt(arbit_warn_fmt, i, ii, host);
	    }
	  }
	  else
	  {
	    arbitration_warning.appfmt(arbit_warn_fmt2, i);
	  }
	}
      }
    }
    if (db_host_count > 1 && node_group_warning.length() > 0)
      ndbout_c("Cluster configuration warning:\n%s",node_group_warning.c_str());
    if (db_host_count > 1 && arbitration_warning.length() > 0)
      ndbout_c("Cluster configuration warning:%s%s",arbitration_warning.c_str(),
	       "\n  Running arbitrator on the same host as a database node may"
	       "\n  cause complete cluster shutdown in case of host failure.");
  }
  return true;
}

template class Vector<ConfigInfo::ConfigRuleSection>;
