/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbTCP.h>
#include "ConfigInfo.hpp"
#include <mgmapi_config_parameters.h>
#include <ndb_limits.h>
#include "InitConfigFileParser.hpp"
#include "m_string.h"
#include <Bitmask.hpp>
#include <ndb_opts.h>
#include <ndb_version.h>


#include <portlib/ndb_localtime.h>

#define KEY_INTERNAL 0
#define MAX_INT_RNIL 0xfffffeff
#define MAX_INT32 0xffffffff
#define MAX_PORT_NO 65535

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
  "SHM"
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
static bool uniqueConnection(InitConfigFileParser::Context & ctx, const char *);
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
static bool fixDeprecated(InitConfigFileParser::Context & ctx, const char *);
static bool fixFileSystemPath(InitConfigFileParser::Context & ctx, const char * data);
static bool fixBackupDataDir(InitConfigFileParser::Context & ctx, const char * data);
static bool fixShmUniqueId(InitConfigFileParser::Context & ctx, const char * data);
static bool checkLocalhostHostnameMix(InitConfigFileParser::Context & ctx, const char * data);
static bool checkThreadPrioSpec(InitConfigFileParser::Context & ctx, const char * data);
static bool checkThreadConfig(InitConfigFileParser::Context & ctx, const char * data);

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

  { "TCP",  transformConnection, 0 },
  { "SHM",  transformConnection, 0 },
  
  { DB_TOKEN,   fixNodeHostname, 0 },
  { API_TOKEN,  fixNodeHostname, 0 },
  { MGM_TOKEN,  fixNodeHostname, 0 },

  { "TCP",  fixNodeId, "NodeId1" },
  { "TCP",  fixNodeId, "NodeId2" },
  { "SHM",  fixNodeId, "NodeId1" },
  { "SHM",  fixNodeId, "NodeId2" },
  
  { "TCP",  uniqueConnection, "TCP" },
  { "SHM",  uniqueConnection, "SHM" },

  { "TCP",  fixHostname, "HostName1" },
  { "TCP",  fixHostname, "HostName2" },
  { "SHM",  fixHostname, "HostName1" },
  { "SHM",  fixHostname, "HostName2" },
  { "SHM",  fixHostname, "HostName1" },
  { "SHM",  fixHostname, "HostName2" },

  { "TCP",  fixPortNumber, 0 }, // has to come after fixHostName
  { "SHM",  fixPortNumber, 0 }, // has to come after fixHostName

  { "*",    applyDefaultValues, "user" },
  { "*",    fixDeprecated, 0 },
  { "*",    applyDefaultValues, "system" },

  { "SHM",  fixShmKey, 0 }, // has to come after apply default values

  { DB_TOKEN,   checkLocalhostHostnameMix, 0 },
  { API_TOKEN,  checkLocalhostHostnameMix, 0 },
  { MGM_TOKEN,  checkLocalhostHostnameMix, 0 },

  { DB_TOKEN,   fixFileSystemPath, 0 },
  { DB_TOKEN,   fixBackupDataDir, 0 },

  { DB_TOKEN,   checkDbConstraints, 0 },
  { DB_TOKEN,   checkThreadConfig, 0 },

  { API_TOKEN, checkThreadPrioSpec, 0 },
  { MGM_TOKEN, checkThreadPrioSpec, 0 },

  { "TCP",  checkConnectionConstraints, 0 },
  { "SHM",  checkConnectionConstraints, 0 },

  { "TCP",  checkTCPConstraints, "HostName1" },
  { "TCP",  checkTCPConstraints, "HostName2" },
  { "SHM",  checkTCPConstraints, "HostName1" },
  { "SHM",  checkTCPConstraints, "HostName2" },
  
  { "*",    checkMandatory, 0 }
};
const int ConfigInfo::m_NoOfRules = sizeof(m_SectionRules)/sizeof(SectionRule);

/****************************************************************************
 * Config Rules declarations
 ****************************************************************************/
static bool add_system_section(Vector<ConfigInfo::ConfigRuleSection>&sections,
                               struct InitConfigFileParser::Context &ctx,
                               const char * rule_data);
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
static bool check_mutually_exclusive(Vector<ConfigInfo::ConfigRuleSection>&sections, 
                                     struct InitConfigFileParser::Context &ctx, 
                                     const char * rule_data);


static bool saveSectionsInConfigValues(Vector<ConfigInfo::ConfigRuleSection>&,
                                       struct InitConfigFileParser::Context &,
                                       const char * rule_data);

const ConfigInfo::ConfigRule 
ConfigInfo::m_ConfigRules[] = {
  { add_system_section, 0 },
  { sanity_checks, 0 },
  { add_node_connections, 0 },
  { set_connection_priorities, 0 },
  { check_node_vs_replicas, 0 },
  { check_mutually_exclusive, 0 },
  { saveSectionsInConfigValues, "SYSTEM,Node,Connection" },
  { 0, 0 }
};
	  
struct DeprecationTransform {
  const char * m_section;
  const char * m_oldName;
  const char * m_newName;
  double m_add;
  double m_mul;
};

static
const DeprecationTransform f_deprecation[] = {
  { 0, 0, 0, 0, 0}
};

static
const ConfigInfo::Typelib arbit_method_typelib[] = {
  { "Disabled", ARBIT_METHOD_DISABLED },
  { "Default", ARBIT_METHOD_DEFAULT },
  { "WaitExternal", ARBIT_METHOD_WAITEXTERNAL },
  { 0, 0 }
};

static
const ConfigInfo::Typelib default_operation_redo_problem_action_typelib [] = {
  { "abort", OPERATION_REDO_PROBLEM_ACTION_ABORT },
  { "queue", OPERATION_REDO_PROBLEM_ACTION_QUEUE },
  { 0, 0 }
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
    "Node id of Primary " MGM_TOKEN_PRINT " node",
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
    "[DB] section",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)NODE_TYPE_DB,
    0, 0
  },

  {
    CFG_DB_SUBSCRIPTIONS,
    "MaxNoOfSubscriptions",
    DB_TOKEN,
    "Max no of subscriptions (default 0 == MaxNoOfTables)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_SUBSCRIBERS,
    "MaxNoOfSubscribers",
    DB_TOKEN,
    "Max no of subscribers (default 0 == 2 * MaxNoOfTables)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_SUB_OPERATIONS,
    "MaxNoOfConcurrentSubOperations",
    DB_TOKEN,
    "Max no of concurrent subscriber operations",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "256",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_TCPBIND_INADDR_ANY,
    "TcpBind_INADDR_ANY",
    DB_TOKEN,
    "Bind IP_ADDR_ANY so that connections can be made from anywhere (for autogenerated connections)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"},

  {
    CFG_NODE_HOST,
    "HostName",
    DB_TOKEN,
    "Name of computer for this node",
    ConfigInfo::CI_USED,
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
    0,
    0, 0 },

  {
    CFG_NODE_ID,
    "NodeId",
    DB_TOKEN,
    "Number identifying the database node (" DB_TOKEN_PRINT ")",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_DATA_NODE_ID) },

  {
    CFG_DB_SERVER_PORT,
    "ServerPort",
    DB_TOKEN,
    "Port used to setup transporter for incoming connections from API nodes",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "1",
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_DB_NO_REPLICAS,
    "NoOfReplicas",
    DB_TOKEN,
    "Number of copies of all data in the database (1-4)",
    ConfigInfo::CI_USED,
    CI_RESTART_SYSTEM | CI_RESTART_INITIAL,
    ConfigInfo::CI_INT,
    "2",
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
    STR_VALUE(NDB_MAX_TABLES) },
  
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
    CFG_DB_NO_INDEX_OPS,
    "MaxNoOfConcurrentIndexOperations",
    DB_TOKEN,
    "Total number of index operations that can execute simultaneously on one " DB_TOKEN_PRINT " node",
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
    "Total number of triggers that can fire simultaneously in one " DB_TOKEN_PRINT " node",
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
    "HostName",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },
  
  {
    CFG_DB_NO_SAVE_MSGS,
    "MaxNoOfSavedMessages",
    DB_TOKEN,
    "Max number of error messages in error log and max number of trace files",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "25",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_EXECUTE_LOCK_CPU,
    "LockExecuteThreadToCPU",
    DB_TOKEN,
    "CPU list indicating which CPU will run the execution thread(s)",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_BITMASK,
    0,
    0,
    "65535"
  },

  {
    CFG_DB_MAINT_LOCK_CPU,
    "LockMaintThreadsToCPU",
    DB_TOKEN,
    "CPU ID indicating which CPU will run the maintenance threads",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    0,
    "0",
    "65535" },

  {
    CFG_DB_REALTIME_SCHEDULER,
    "RealtimeScheduler",
    DB_TOKEN,
    "If yes, then NDB Cluster threads will be scheduled as real-time threads",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_DB_USE_SHM,
    "UseShm",
    DB_TOKEN,
    "Use shared memory transporter on same host",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_DB_MEMLOCK,
    "LockPagesInMainMemory",
    DB_TOKEN,
    "If set to yes, then NDB Cluster data will not be swapped out to disk",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "2" },

  {
    CFG_DB_WATCHDOG_INTERVAL,
    "TimeBetweenWatchDogCheck",
    DB_TOKEN,
    "Time between execution checks inside a database node",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "6000",
    "70",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_SCHED_EXEC_TIME,
    "SchedulerExecutionTimer",
    DB_TOKEN,
    "Number of microseconds to execute in scheduler before sending",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "50",
    "0",
    "11000" },

  {
    CFG_DB_MAX_SEND_DELAY,
    "MaxSendDelay",
    DB_TOKEN,
    "Max number of microseconds to delay sending in ndbmtd",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "11000" },

  {
    CFG_DB_SCHED_SPIN_TIME,
    "SchedulerSpinTimer",
    DB_TOKEN,
    "Number of microseconds to execute in scheduler before sleeping",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "500" },

  {
    CFG_DB_SCHED_RESPONSIVENESS,
    "SchedulerResponsiveness",
    DB_TOKEN,
    "Value between 0 and 10, high means very responsive, low means throughput-optimised",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "5",
    "0",
    "10" },

  {
    CFG_DB_SCHED_SCAN_PRIORITY,
    "__sched_scan_priority",
    DB_TOKEN,
    "Number of rows scanned per real-time break, higher value gives higher prio to scans",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "6",
    "1",
    "6" },

  {
    CFG_DB_DISK_DATA_FORMAT,
    "__disk_data_format",
    DB_TOKEN,
    "0: Use old v1 format, 1: Use new v2 format",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1",
    "0",
    "1" },

  {
    CFG_DB_WATCHDOG_INTERVAL_INITIAL,
    "TimeBetweenWatchDogCheckInitial",
    DB_TOKEN,
    "Time between execution checks inside a database node in the early start phases when memory is allocated",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "6000",
    "70",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_STOP_ON_ERROR,
    "StopOnError",
    DB_TOKEN,
    "If set to N, " DB_TOKEN_PRINT " automatically restarts/recovers in case of node failure",
    ConfigInfo::CI_USED,
    0,
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
    0,
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
    CFG_DB_MAX_DML_OPERATIONS_PER_TRANSACTION,
    "MaxDMLOperationsPerTransaction",
    DB_TOKEN,
    "Max DML-operations in one transaction (0 == no limit)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    STR_VALUE(MAX_INT32),
    "32",
    STR_VALUE(MAX_INT32)
  },

  {
    CFG_DB_NO_LOCAL_OPS,
    "MaxNoOfLocalOperations",
    DB_TOKEN,
    "Max number of operation records defined in the local storage node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
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
    0,
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
    "Max number of transaction executing concurrently on the " DB_TOKEN_PRINT " node",
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
    "Max number of scans executing concurrently on the " DB_TOKEN_PRINT " node",
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
    "Dynamic buffer space (in bytes) for key and attribute data allocated for each " DB_TOKEN_PRINT " node",
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
    "Number bytes on each " DB_TOKEN_PRINT " node allocated for storing indexes",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT64,
    "0M",
    "1M",
    "1024G" },

  {
    CFG_DB_DATA_MEM,
    "DataMemory",
    DB_TOKEN,
    "Number bytes on each " DB_TOKEN_PRINT " node allocated for storing data",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "98M",
    "1M",
    "1024G" },

  {
    CFG_DB_UNDO_INDEX_BUFFER,
    "UndoIndexBuffer",
    DB_TOKEN,
    "Number bytes on each " DB_TOKEN_PRINT " node allocated for writing UNDO logs for index part",
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
    "Number bytes on each " DB_TOKEN_PRINT " node allocated for writing UNDO logs for data part",
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
    "Number bytes on each " DB_TOKEN_PRINT " node allocated for writing REDO logs",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "32M",
    "1M",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_LONG_SIGNAL_BUFFER,
    "LongMessageBuffer",
    DB_TOKEN,
    "Number bytes on each " DB_TOKEN_PRINT " node allocated for internal long messages",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "64M",
    "512k",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_DISK_PAGE_BUFFER_MEMORY,
    "DiskPageBufferMemory",
    DB_TOKEN,
    "Number bytes on each " DB_TOKEN_PRINT " node allocated for disk page buffer cache",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "64M",
    "4M",
    "1024G" },

  {
    CFG_DB_SGA,
    "SharedGlobalMemory",
    DB_TOKEN,
    "Total number bytes on each " DB_TOKEN_PRINT " node allocated for any use",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
    "20M",
#else
    "128M",
#endif
    "0",
    "65536G" }, // 32k pages * 32-bit i value
  
  {
    CFG_DB_START_PARTIAL_TIMEOUT,
    "StartPartialTimeout",
    DB_TOKEN,
    "Time to wait before trying to start wo/ all nodes. 0=Wait forever",
    ConfigInfo::CI_USED,
    0,
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
    0,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_START_FAILURE_TIMEOUT,
    "StartFailureTimeout",
    DB_TOKEN,
    "Time to wait before terminating. 0=Wait forever",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_START_NO_NODEGROUP_TIMEOUT,
    "StartNoNodegroupTimeout",
    DB_TOKEN,
    "Time to wait for nodes wo/ nodegroup before trying to start (0=forever)",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "15000",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalDbDb",
    DB_TOKEN,
    "Time between " DB_TOKEN_PRINT "-" DB_TOKEN_PRINT " heartbeats. " DB_TOKEN_PRINT " considered dead after 3 missed HBs",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
    "1500",
#else
    "5000",
#endif
    "10",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_CONNECT_CHECK_DELAY,
    "ConnectCheckIntervalDelay",
    DB_TOKEN,
    "Time between " DB_TOKEN_PRINT " connectivity check stages.  " DB_TOKEN_PRINT " considered suspect after 1 and dead after 2 intervals.",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_API_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalDbApi",
    DB_TOKEN,
    "Time between " API_TOKEN_PRINT "-" DB_TOKEN_PRINT " heartbeats. " API_TOKEN_PRINT " connection closed after 3 missed HBs",
    ConfigInfo::CI_USED,
    0,
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
    0,
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
    0,
    ConfigInfo::CI_INT,
    "2000",
    "20",
    "32000" },
  {
    CFG_DB_GCP_TIMEOUT,
    "TimeBetweenGlobalCheckpointsTimeout",
    DB_TOKEN,
    "Minimum timeout for group commit of transactions to disk",
    /*
      Actual timeout may be higher, as there must be sufficient time to 
      correctly detect node failures, such that these are not reported as GCP 
      stop.
    */
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "120000",
    "10",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_MICRO_GCP_INTERVAL,
    "TimeBetweenEpochs",
    DB_TOKEN,
    "Time between epochs (syncronization used e.g for replication)",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "100",
    "0",
    "32000" },

  {
    CFG_DB_MICRO_GCP_TIMEOUT,
    "TimeBetweenEpochsTimeout",
    DB_TOKEN,
    "Timeout for time between epochs.  Exceeding will cause node shutdown.",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
    "4000",
#else
    "0",
#endif
    "0",
    "256000" },

  {
    CFG_DB_MAX_BUFFERED_EPOCHS,
    "MaxBufferedEpochs",
    DB_TOKEN,
    "Allowed numbered of epochs that a subscribing node can lag behind (unprocessed epochs).  Exceeding will cause lagging subscribers to be disconnected.",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "100",
    "1",
    "100000" },

  {
    CFG_DB_MAX_BUFFERED_EPOCH_BYTES,
    "MaxBufferedEpochBytes",
    DB_TOKEN,
    "Total number of bytes allocated for buffering epochs.",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "26214400",
    "26214400",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_REDOLOG_PARTS,
    "NoOfFragmentLogParts",
    DB_TOKEN,
    "Number of file groups of redo log files belonging to " DB_TOKEN_PRINT " node",
    ConfigInfo::CI_USED,
    CI_RESTART_INITIAL,
    ConfigInfo::CI_INT,
    STR_VALUE(NDB_DEFAULT_LOG_PARTS),
    "4",
    STR_VALUE(NDB_MAX_LOG_PARTS)
  },

  {
    CFG_DB_NO_REDOLOG_FILES,
    "NoOfFragmentLogFiles",
    DB_TOKEN,
    "No of Redo log files in each of the file group belonging to " DB_TOKEN_PRINT " node",
    ConfigInfo::CI_USED,
    CI_RESTART_INITIAL,
    ConfigInfo::CI_INT,
    "16",
    "3",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_REDOLOG_FILE_SIZE,
    "FragmentLogFileSize",
    DB_TOKEN,
    "Size of each Redo log file",
    ConfigInfo::CI_USED,
    CI_RESTART_INITIAL,
    ConfigInfo::CI_INT,
    "16M",
    "4M",
    "1G" },

  {
    CFG_DB_INIT_REDO,
    "InitFragmentLogFiles",
    DB_TOKEN,
    "Initialize fragment logfiles (sparse/full)",
    ConfigInfo::CI_USED,
    CI_RESTART_INITIAL,
    ConfigInfo::CI_STRING,
    "sparse",
    0, 0 },

  {
    CFG_DB_THREAD_POOL,
    "DiskIOThreadPool",
    DB_TOKEN,
    "No of unbound threads for file access (currently only for DD)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "2",
    "0",  
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_MAX_OPEN_FILES,
    "MaxNoOfOpenFiles",
    DB_TOKEN,
    "Max number of files open per " DB_TOKEN_PRINT " node.(One thread is created per file)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "20",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_INITIAL_OPEN_FILES,
    "InitialNoOfOpenFiles",
    DB_TOKEN,
    "Initial number of files open per " DB_TOKEN_PRINT " node.(One thread is created per file)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "27",
    "20",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_TRANSACTION_CHECK_INTERVAL,
    "TimeBetweenInactiveTransactionAbortCheck",
    DB_TOKEN,
    "Time between inactive transaction checks",
    ConfigInfo::CI_USED,
    0,
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
    0,
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
    0,
    ConfigInfo::CI_INT,
    "1200",
    "50",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_DISCLESS,
    "Diskless",
    DB_TOKEN,
    "Run wo/ disk",
    ConfigInfo::CI_USED,
    CI_RESTART_INITIAL | CI_RESTART_SYSTEM,
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
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
    "3000",
#else
    "7500",
#endif
    "10",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_ARBIT_METHOD,
    "Arbitration",
    DB_TOKEN,
    "How to perform arbitration to avoid split-brain issue when node(s) fail",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_ENUM,
    "Default", /* Default value */
    (const char*)arbit_method_typelib,
    0
  },

  {
    CFG_NODE_DATADIR,
    "DataDir",
    DB_TOKEN,
    "Data directory for this node",
    ConfigInfo::CI_USED,
    CI_CHECK_WRITABLE | CI_RESTART_INITIAL,
    ConfigInfo::CI_STRING,
    ".",
    0, 0 },

  {
    CFG_DB_FILESYSTEM_PATH,
    "FileSystemPath",
    DB_TOKEN,
    "Path to directory where the " DB_TOKEN_PRINT " node stores its data (directory must exist)",
    ConfigInfo::CI_USED,
    CI_CHECK_WRITABLE | CI_RESTART_INITIAL,
    ConfigInfo::CI_STRING,
    0,
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
    CI_CHECK_WRITABLE | CI_RESTART_INITIAL,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },
  
  { 
    CFG_DB_DISK_SYNCH_SIZE,
    "DiskSyncSize",
    DB_TOKEN,
    "Data written to a file before a synch is forced",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "4M",
    "32k",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_MIN_DISK_WRITE_SPEED,
    "MinDiskWriteSpeed",
    DB_TOKEN,
    "Minimum bytes per second allowed to be written by LCP and backup",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "10M",
    "1M",
    "1024G" },
  
  {
    CFG_DB_MAX_DISK_WRITE_SPEED,
    "MaxDiskWriteSpeed",
    DB_TOKEN,
    "Maximum bytes per second allowed to be written by LCP and backup"
    " when no restarts are ongoing",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "20M",
    "1M",
    "1024G" },

  {
    CFG_DB_MAX_DISK_WRITE_SPEED_OTHER_NODE_RESTART,
    "MaxDiskWriteSpeedOtherNodeRestart",
    DB_TOKEN,
    "Maximum bytes per second allowed to be written by LCP and backup"
    " when another node is restarting",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "50M",
    "1M",
    "1024G" },
 
  {
    CFG_DB_MAX_DISK_WRITE_SPEED_OWN_RESTART,
    "MaxDiskWriteSpeedOwnRestart",
    DB_TOKEN,
    "Maximum bytes per second allowed to be written by LCP and backup"
    " when our node is restarting",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "200M",
    "1M",
    "1024G" },

  {
    CFG_DB_BACKUP_DISK_WRITE_PCT,
    "BackupDiskWriteSpeedPct",
    DB_TOKEN,
    "Percentage of MaxDiskWriteSpeed to reserve for Backup, including "
    "the Backup log",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "50",
    "0",
    "90" },
  
  { 
    CFG_DB_BACKUP_MEM,
    "BackupMemory",
    DB_TOKEN,
    "Total memory allocated for backups per node (in bytes)",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    "32M", // sum of BackupDataBufferSize and BackupLogBufferSize
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  { 
    CFG_DB_BACKUP_DATA_BUFFER_MEM,
    "BackupDataBufferSize",
    DB_TOKEN,
    "Default size of databuffer for a backup (in bytes)",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    "1M",
    "512K",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_BACKUP_LOG_BUFFER_MEM,
    "BackupLogBufferSize",
    DB_TOKEN,
    "Default size of logbuffer for a backup (in bytes)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "16M",
    "2M",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_BACKUP_WRITE_SIZE,
    "BackupWriteSize",
    DB_TOKEN,
    "Default size of filesystem writes made by backup (in bytes)",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    "256K",
    "32K",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_BACKUP_MAX_WRITE_SIZE,
    "BackupMaxWriteSize",
    DB_TOKEN,
    "Max size of filesystem writes made by backup (in bytes)",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    "1M",
    "256K",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_STRING_MEMORY,
    "StringMemory",
    DB_TOKEN,
    "Default size of string memory (1-100 -> %of max, >100 -> actual bytes)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "25",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_MAX_ALLOCATE,
    "MaxAllocate",
    DB_TOKEN,
    "Maximum size of allocation to use when allocating memory for tables",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "32M",
    "1M",
    "1G" },

  { 
    CFG_DB_MEMREPORT_FREQUENCY,
    "MemReportFrequency",
    DB_TOKEN,
    "Frequency of mem reports in seconds, 0 = only when passing %-limits",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  { 
    CFG_DB_BACKUP_REPORT_FREQUENCY,
    "BackupReportFrequency",
    DB_TOKEN,
    "Frequency of backup status reports during backup in seconds",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

   {
    CFG_DB_STARTUP_REPORT_FREQUENCY,
    "StartupStatusReportFrequency",
    DB_TOKEN,
    "Frequency of various status reports during startup in seconds",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },
 
  {
    CFG_DB_O_DIRECT_SYNC_FLAG,
    "ODirectSyncFlag",
    DB_TOKEN,
    "O_DIRECT writes are treated as sync:ed writes",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"},

  {
    CFG_DB_O_DIRECT,
    "ODirect",
    DB_TOKEN,
    "Use O_DIRECT file write/read when possible",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"},

  {
    CFG_DB_COMPRESSED_BACKUP,
    "CompressedBackup",
    DB_TOKEN,
    "Use zlib to compress BACKUPs as they are written",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"},
  {
    CFG_DB_COMPRESSED_LCP,
    "CompressedLCP",
    DB_TOKEN,
    "Write compressed LCPs using zlib",
    ConfigInfo::CI_USED,
    CI_RESTART_INITIAL,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"},

  {
    CFG_EXTRA_SEND_BUFFER_MEMORY,
    "ExtraSendBufferMemory",
    DB_TOKEN,
    "Extra send buffer memory to use for send buffers in all transporters",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "0",
    "0",
    "32G"
  },

  {
    CFG_TOTAL_SEND_BUFFER_MEMORY,
    "TotalSendBufferMemory",
    DB_TOKEN,
    "Total memory to use for send buffers in all transporters",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "256K",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_LOCATION_DOMAIN_ID,
    "LocationDomainId",
    DB_TOKEN,
    "LocationDomainId for node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "0",
    "16"
  },

  {
    CFG_DB_NODEGROUP,
    "Nodegroup",
    DB_TOKEN,
    "Nodegroup for node, only used during initial cluster start",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "0",
    STR_VALUE(NDB_NO_NODEGROUP)
  },

  {
    CFG_DB_MT_THREADS,
    "MaxNoOfExecutionThreads",
    DB_TOKEN,
    "For ndbmtd, specify max no of execution threads",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "2",
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
    "8"
#else
    /**
     * NOTE: The actual maximum number of threads is 98...
     *   but that config is so weird so it's only possible to get
     *   by using ThreadConfig
     */
    "72"
#endif
  },

  {
    CFG_NDBMT_LQH_WORKERS,
    "__ndbmt_lqh_workers",
    DB_TOKEN,
    "For ndbmtd specify no of lqh workers",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "1",
    "4"
  },

  {
    CFG_NDBMT_LQH_THREADS,
    "__ndbmt_lqh_threads",
    DB_TOKEN,
    "For ndbmtd specify no of lqh threads",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "1",
    "4"
  },
  
  {
    CFG_NDBMT_CLASSIC,
    "__ndbmt_classic",
    DB_TOKEN,
    "For ndbmtd use mt-classic",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    0,
    "false",
    "true"
  },

  {
    CFG_DB_MT_THREAD_CONFIG,
    "ThreadConfig",
    DB_TOKEN,
    "Thread configuration",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0,
    0
  },

  {
    CFG_DB_DD_FILESYSTEM_PATH,
    "FileSystemPathDD",
    DB_TOKEN,
    "Path to directory where the " DB_TOKEN_PRINT " node stores its disk-data/undo-files",
    ConfigInfo::CI_USED,
    CI_CHECK_WRITABLE | CI_RESTART_INITIAL,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_DB_DD_DATAFILE_PATH,
    "FileSystemPathDataFiles",
    DB_TOKEN,
    "Path to directory where the " DB_TOKEN_PRINT " node stores its disk-data-files",
    ConfigInfo::CI_USED,
    CI_CHECK_WRITABLE | CI_RESTART_INITIAL,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_DB_DD_UNDOFILE_PATH,
    "FileSystemPathUndoFiles",
    DB_TOKEN,
    "Path to directory where the " DB_TOKEN_PRINT " node stores its disk-undo-files",
    ConfigInfo::CI_USED,
    CI_CHECK_WRITABLE | CI_RESTART_INITIAL,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_DB_DD_LOGFILEGROUP_SPEC,
    "InitialLogfileGroup",
    DB_TOKEN,
    "Logfile group that will be created during initial start",
    ConfigInfo::CI_USED,
    CI_RESTART_SYSTEM | CI_RESTART_INITIAL,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_DB_DD_TABLEPACE_SPEC,
    "InitialTablespace",
    DB_TOKEN,
    "Tablespace that will be created during initial start",
    ConfigInfo::CI_USED,
    CI_RESTART_SYSTEM | CI_RESTART_INITIAL,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_DB_LCP_TRY_LOCK_TIMEOUT,
    "MaxLCPStartDelay",
    DB_TOKEN,
    "Time in seconds that LCP will poll for checkpoint mutex, before putting it self in lock-queue",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "600" },

// 7.0 NodeGroup -> initial, system

  {
    CFG_DB_MT_BUILD_INDEX,
    "BuildIndexThreads",
    DB_TOKEN,
    "No of threads to use for building ordered indexes during system/node restart",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "128",
    "0",
    "128" },

  {
    CFG_DB_HB_ORDER,
    "HeartbeatOrder",
    DB_TOKEN,
    "Heartbeat circle is ordered by the given values "
    "which must be non-zero and distinct",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "65535" },

  {
    CFG_DB_DICT_TRACE,
    "DictTrace",
    DB_TOKEN,
    "Tracelevel for ndbd's dictionary",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "0",
    "100" },

  {
    CFG_DB_MAX_START_FAIL,
    "MaxStartFailRetries",
    DB_TOKEN,
    "Maximum retries when Ndbd fails in startup, requires StopOnError=0.  "
    "0 is infinite.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "3",                      /* Default */
    "0",                      /* Min */
    STR_VALUE(MAX_INT_RNIL)   /* Max */
  },

  {
    CFG_DB_START_FAIL_DELAY_SECS,
    "StartFailRetryDelay",
    DB_TOKEN,
    "Delay in seconds after start failure prior to retry.  "
    "Requires StopOnError= 0",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",                     /* Default */
    "0",                     /* Min */
    STR_VALUE(MAX_INT_RNIL)  /* Max */
  },

  {
    CFG_DB_EVENTLOG_BUFFER_SIZE,
    "EventLogBufferSize",
    DB_TOKEN,
    "Size of circular buffer of ndb_logevent (inside datanodes)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "8192",                  /* Default */
    "0",                     /* Min */
    "64k"                    /* Max : There is no flow control...so set limit*/
  },

  {
    CFG_DB_NUMA,
    "Numa",
    DB_TOKEN,
    "Enable/disable numa support (currently linux only)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1",                     /* Interleave on all numa nodes */
    "0",                     /* Min (no numa action at all)  */
    "1"                      /* Max */
  },

  {
    CFG_DB_REDO_OVERCOMMIT_LIMIT,
    "RedoOverCommitLimit",
    DB_TOKEN,
    "Limit for how long it will take to flush current "
    "RedoBuffer before action is taken (in seconds)",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "20",                    /* Default */
    "0",                     /* Min */
    STR_VALUE(MAX_INT_RNIL)  /* Max */
  },

  {
    CFG_DB_REDO_OVERCOMMIT_COUNTER,
    "RedoOverCommitCounter",
    DB_TOKEN,
    "If RedoOverCommitLimit has been reached RedoOverCommitCounter"
    " in a row times, transactions will be aborted",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "3",                     /* Default */
    "0",                     /* Min */
    STR_VALUE(MAX_INT_RNIL)  /* Max */
  },

  {
    CFG_DB_LATE_ALLOC,
    "LateAlloc",
    DB_TOKEN,
    "Allocate memory after connection to ndb_mgmd has been established",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1",
    "0",                     /* Min */
    "1"                      /* Max */
  },

  {
    CFG_DB_PARALLEL_COPY_THREADS,
    "MaxParallelCopyInstances",
    DB_TOKEN,
    "Number of parallel copies during node restarts, 0 means default",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",                     /* Min */
    "64"                      /* Max */
  },


  {
    CFG_DB_2PASS_INR,
    "TwoPassInitialNodeRestartCopy",
    DB_TOKEN,
    "Copy data in 2 passes for initial node restart, "
    "this enables multi-threaded-ordered index build for initial node restart",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "true",
    "false",                     /* Min */
    "true"                       /* Max */
  },

  {
    CFG_DB_PARALLEL_SCANS_PER_FRAG,
    "MaxParallelScansPerFragment",
    DB_TOKEN,
    "Max parallel scans per fragment (tup or tux). If this limit is reached "
    " scans will be serialized using a queue.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
    "32",
#else
    "256",
#endif
    "1",                         /* Min */
    STR_VALUE(MAX_INT_RNIL)      /* Max */
  },

  /* ordered index stats */

  {
    CFG_DB_INDEX_STAT_AUTO_CREATE,
    "IndexStatAutoCreate",
    DB_TOKEN,
    "Make create index also create initial index stats",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "1"
  },

  {
    CFG_DB_INDEX_STAT_AUTO_UPDATE,
    "IndexStatAutoUpdate",
    DB_TOKEN,
    "Monitor each index for changes and trigger automatic stats updates."
    " See IndexStatTrigger options",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "1"
  },

  {
    CFG_DB_INDEX_STAT_SAVE_SIZE,
    "IndexStatSaveSize",
    DB_TOKEN,
    "Maximum bytes allowed for the saved stats of one index."
    " At least 1 sample is produced regardless of size limit."
    " The size is scaled up by a factor from IndexStatSaveScale."
    " The value affects size of stats saved in NDB system tables"
    " and in mysqld memory cache",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "32768",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_DB_INDEX_STAT_SAVE_SCALE,
    "IndexStatSaveScale",
    DB_TOKEN,
    "Factor to scale up IndexStatSaveSize for a large index."
    " Given in units of 0.01."
    " Multiplied by a logarithmic index size."
    " Value 0 disables scaling",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "100",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_DB_INDEX_STAT_TRIGGER_PCT,
    "IndexStatTriggerPct",
    DB_TOKEN,
    "Percent change (in DML ops) to schedule index stats update."
    " The value is scaled down by a factor from IndexStatTriggerScale."
    " Value 0 disables the trigger",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "100",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_DB_INDEX_STAT_TRIGGER_SCALE,
    "IndexStatTriggerScale",
    DB_TOKEN,
    "Factor to scale down IndexStatTriggerPct for a large index."
    " Given in units of 0.01."
    " Multiplied by a logarithmic index size."
    " Value 0 disables scaling",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "100",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_DB_INDEX_STAT_UPDATE_DELAY,
    "IndexStatUpdateDelay",
    DB_TOKEN,
    "Minimum delay in seconds between automatic index stats updates"
    " for a given index."
    " Value 0 means no delay",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "60",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_DB_CRASH_ON_CORRUPTED_TUPLE,
    "CrashOnCorruptedTuple",
    DB_TOKEN,
    "To be failfast or not, when checksum indicates corruption.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,1)
    "false",
#else
    "true",
#endif
    "false",
    "true"},


  {
    CFG_DB_FREE_PCT,
    "MinFreePct",
    DB_TOKEN,
    "Keep 5% of database free to ensure that we don't get out of memory during restart",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "5",
    "0",
    "100"
  },

  {
    CFG_DEFAULT_HASHMAP_SIZE,
    "DefaultHashmapSize",
    DB_TOKEN,
    "Hashmap size to use for new tables.  Normally this should be left unset, "
    "but can be set to aid downgrade to older versions not supporting as big "
    "hashmaps as current version or to use special hashmap size to gain "
    "better balance for some number of nodes and ldm-threads.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(NDB_DEFAULT_HASHMAP_BUCKETS)
  },

  {
    CFG_DB_LCP_SCAN_WATCHDOG_LIMIT,
    "LcpScanProgressTimeout",
    DB_TOKEN,
    "Maximum time a local checkpoint fragment scan can be stalled for.  "
    "If this is exceeded, the node will shutdown to ensure systemwide "
    "LCP progress.  Warnings are periodically emitted when a fragment scan "
    "stalls for more than one third of this time.  0 indicates no time limit.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "60",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_DB_ENABLE_PARTIAL_LCP,
    "EnablePartialLcp",
    DB_TOKEN,
    "Enable partial LCP, this means a checkpoint only writes the difference"
    " to the last LCP plus some parts that are fully checkpointed. If this"
    " isn't enabled then all LCPs are writing a full checkpoint.",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_BOOL,
    "true",
    "false",
    "true"
  },
  {
    CFG_DB_RECOVERY_WORK,
    "RecoveryWork",
    DB_TOKEN,
    "Percentage of storage overhead for LCP files, increasing the value"
    " means less work in normal operation and more at recovery, decreasing"
    " it means more work in normal operation and less work in recovery",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "50",
    "25",
    "100"
  },

  {
    CFG_DB_AT_RESTART_SKIP_INDEXES,
    "__at_restart_skip_indexes",
    DB_TOKEN,
    "Ignore all index and foreign key info on the node "
    "at (non-initial) restart.  "
    "This is a one-time recovery option for a non-startable database.  "
    "Carefully consult documentation before using.",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"
  },

  {
    CFG_DB_AT_RESTART_SKIP_FKS,
    "__at_restart_skip_fks",
    DB_TOKEN,
    "Ignore all foreign key info on the node "
    "at (non-initial) restart.  "
    "This is a one-time recovery option for a non-startable database.  "
    "Carefully consult documentation before using.",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"
  },
  {
    CFG_MIXOLOGY_LEVEL,
    "__debug_mixology_level",
    DB_TOKEN,
    "Artificial signal flow mixing to expose bugs.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_DB_AT_RESTART_SUBSCRIBER_CONNECT_TIMEOUT,
    "RestartSubscriberConnectTimeout",
    DB_TOKEN,
    "On node restart the time that a data node will wait for "
    "subscribing Api nodes to connect.  If it expires, missing "
    "Api nodes will be disconnected from the cluster.  A zero "
    "value means that timeout is disabled.  Even if unit is "
    "milliseconds the actual resolution of timeout will be "
    "seconds.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "120000",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_DB_UI_BUILD_MAX_BATCHSIZE,
    "MaxUIBuildBatchSize",
    DB_TOKEN,
    "Max scan batch size to use for building unique indexes.  "
    "Increasing this may speed up unique index builds, at the "
    "risk of greater impact to ongoing traffic.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "64",
    "16",
    "512"
  },

  {
    CFG_DB_FK_BUILD_MAX_BATCHSIZE,
    "MaxFKBuildBatchSize",
    DB_TOKEN,
    "Max scan batch size to use for building foreign keys.  "
    "Increasing this may speed up foreign key builds, at the "
    "risk of greater impact to ongoing traffic.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "64",
    "16",
    "512"
  },

  {
    CFG_DB_REORG_BUILD_MAX_BATCHSIZE,
    "MaxReorgBuildBatchSize",
    DB_TOKEN,
    "Max scan batch size to use for reorganising table partitions.  "
    "Increasing this may speed up reorganisation of table partitions, at the "
    "risk of greater impact to ongoing traffic.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "64",
    "16",
    "512"
  },

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
    KEY_INTERNAL,
    "wan",
    API_TOKEN,
    "Use WAN TCP setting as default",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"
  },
  
  {
    CFG_NODE_HOST,
    "HostName",
    API_TOKEN,
    "Name of computer for this node",
    ConfigInfo::CI_USED,
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
    0,
    0, 0 },

  {
    CFG_NODE_ID,
    "NodeId",
    API_TOKEN,
    "Number identifying application node (" API_TOKEN_PRINT ")",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES_ID) },

  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    API_TOKEN,
    "HostName",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_NODE_ARBIT_RANK,
    "ArbitrationRank",
    API_TOKEN,
    "If 0, then " API_TOKEN_PRINT " is not arbitrator. Kernel selects arbitrators in order 1, 2",
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
    0,
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

  {
    KEY_INTERNAL,
    "ConnectionMap",
    "API",
    "Specifies which DB nodes to connect",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0,
    0
  },

  {
    CFG_EXTRA_SEND_BUFFER_MEMORY,
    "ExtraSendBufferMemory",
    API_TOKEN,
    "Extra send buffer memory to use for send buffers in all transporters",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "0",
    "0",
    "32G"
  },

  {
    CFG_TOTAL_SEND_BUFFER_MEMORY,
    "TotalSendBufferMemory",
    "API",
    "Total memory to use for send buffers in all transporters",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "256K",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_LOCATION_DOMAIN_ID,
    "LocationDomainId",
    API_TOKEN,
    "LocationDomainId for node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "0",
    "16"
  },

  {
    CFG_AUTO_RECONNECT,
    "AutoReconnect",
    "API",
    "Specifies if an api node should reconnect when fully disconnected from cluster",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "true",
    "false",
    "true"
  },

  {
    CFG_HB_THREAD_PRIO,
    "HeartbeatThreadPriority",
    API_TOKEN,
    "Specify thread properties of heartbeat thread",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_STRING,
    0, 0, 0 },

  {
    CFG_DEFAULT_OPERATION_REDO_PROBLEM_ACTION,
    "DefaultOperationRedoProblemAction",
    API_TOKEN,
    "If Redo-log is having problem, should operation default "
    "(unless overridden on transaction/operation level) abort "
    "or be put on queue",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_ENUM,
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
    "abort", /* Default value */
#else
    "queue", /* Default value */
#endif
    (const char*)default_operation_redo_problem_action_typelib,
    0
  },

  {
    CFG_DEFAULT_HASHMAP_SIZE,
    "DefaultHashmapSize",
    API_TOKEN,
    "Hashmap size to use for new tables.  Normally this should be left unset, "
    "but can be set to aid downgrade to older versions not supporting as big "
    "hashmaps as current version or to use special hashmap size to gain "
    "better balance for some number of nodes and ldm-threads.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(NDB_DEFAULT_HASHMAP_BUCKETS)
  },
  {
    CFG_MIXOLOGY_LEVEL,
    "__debug_mixology_level",
    API_TOKEN,
    "Artificial signal flow mixing to expose bugs.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_CONNECT_BACKOFF_MAX_TIME,
    "ConnectBackoffMaxTime",
    "API",
    "Specifies the longest time between connection attempts to a "
    "data node from an api node in milliseconds (with "
    "approximately 100ms resolution).  Note that this excludes "
    "any time while a connection attempt are underway, which in "
    "worst case can take several seconds.  To disable the backoff "
    "set it to zero.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "1500",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_START_CONNECT_BACKOFF_MAX_TIME,
    "StartConnectBackoffMaxTime",
    "API",
    "This has the same meaning as ConnectBackoffMaxTime, but "
    "is used instead of it while no data nodes are connected to "
    "the API node.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_API_VERBOSE,
    "ApiVerbose",
    "API",
    "Tracelevel for API nodes.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "0",
    "100"
  },

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
    KEY_INTERNAL,
    "wan",
    MGM_TOKEN,
    "Use WAN TCP setting as default",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true"
  },
  
  {
    CFG_NODE_HOST,
    "HostName",
    MGM_TOKEN,
    "Name of computer for this node",
    ConfigInfo::CI_USED,
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
    CI_CHECK_WRITABLE,
    ConfigInfo::CI_STRING,
    "",
    0, 0 },

  {
    CFG_NODE_SYSTEM,
    "System",
    MGM_TOKEN,
    "Name of system for this node",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_NODE_ID,
    "NodeId",
    MGM_TOKEN,
    "Number identifying the management server node (" MGM_TOKEN_PRINT ")",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES_ID) },
  
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
    "HostName",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_MGM_PORT,
    "PortNumber",
    MGM_TOKEN,
    "Port number to give commands to/fetch configurations from management server",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    STR_VALUE(NDB_PORT),
    "0",
    STR_VALUE(MAX_PORT_NO) },

  {
    KEY_INTERNAL,
    "PortNumberStats",
    MGM_TOKEN,
    "Port number used to get statistical information from a management server",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "0",
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_NODE_ARBIT_RANK,
    "ArbitrationRank",
    MGM_TOKEN,
    "If 0, then " MGM_TOKEN_PRINT " is not arbitrator. Kernel selects arbitrators in order 1, 2",
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

  {
    CFG_EXTRA_SEND_BUFFER_MEMORY,
    "ExtraSendBufferMemory",
    MGM_TOKEN,
    "Extra send buffer memory to use for send buffers in all transporters",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT64,
    "0",
    "0",
    "32G"
  },

  {
    CFG_TOTAL_SEND_BUFFER_MEMORY,
    "TotalSendBufferMemory",
    MGM_TOKEN,
    "Total memory to use for send buffers in all transporters",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "256K",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_LOCATION_DOMAIN_ID,
    "LocationDomainId",
    MGM_TOKEN,
    "LocationDomainId for node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    0,
    "0",
    "16"
  },

  {
    CFG_HB_THREAD_PRIO,
    "HeartbeatThreadPriority",
    MGM_TOKEN,
    "Specify thread properties of heartbeat thread",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_STRING,
    0, 0, 0
  },

  {
    CFG_MGMD_MGMD_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalMgmdMgmd",
    MGM_TOKEN,
    "Time between " MGM_TOKEN_PRINT "-" MGM_TOKEN_PRINT " heartbeats. " 
    MGM_TOKEN_PRINT " considered dead after 3 missed HBs",
    ConfigInfo::CI_USED,
    0,
    ConfigInfo::CI_INT,
    "1500",
    "100",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_MIXOLOGY_LEVEL,
    "__debug_mixology_level",
    MGM_TOKEN,
    "Artificial signal flow mixing to expose bugs.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_DB_DISK_PAGE_BUFFER_ENTRIES,
    "DiskPageBufferEntries",
    DB_TOKEN,
    "Determines number of unique disk page requests to allocate. "
    "Specified as multiple of number of buffer pages "
    "i.e. number of 32k pages in DiskPageBufferMemory. "
    "Each entry takes about 100 bytes. "
    "Large disk data transactions "
    "may require increasing the default.",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "10",
    "1",
    STR_VALUE(MAX_INT32)
  },

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
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "TCP",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "TCP",
    "Id of node (" DB_TOKEN_PRINT ", " API_TOKEN_PRINT " or " MGM_TOKEN_PRINT ") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "TCP",
    "Id of node (" DB_TOKEN_PRINT ", " API_TOKEN_PRINT " or " MGM_TOKEN_PRINT ") on one side of the connection",
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
    CFG_CONNECTION_PRESEND_CHECKSUM,
    "PreSendChecksum",
    "TCP",
    "If PreSendChecksum AND Checksum are enabled,\n"
    "pre-send checksum checks are done, and\n"
    "all signals between nodes are checked for errors",
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
    "PortNumber to be used by data nodes while connecting the transporters",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_TCP_SEND_BUFFER_SIZE,
    "SendBufferMemory",
    "TCP",
    "Bytes of buffer for signals sent from this node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "2M",
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
    "2M",
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
    0,
    0, 0 },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "TCP",
    "System for node 1 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "TCP",
    "System for node 2 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_TCP_SND_BUF_SIZE,
    "TCP_SND_BUF_SIZE",
    "TCP",
    "Value used for SO_SNDBUF",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0", 
    "2G"
  },

  {
    CFG_TCP_RCV_BUF_SIZE,
    "TCP_RCV_BUF_SIZE",
    "TCP",
    "Value used for SO_RCVBUF",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0", 
    "2G" 
  },
  
  {
    CFG_TCP_MAXSEG_SIZE,
    "TCP_MAXSEG_SIZE",
    "TCP",
    "Value used for TCP_MAXSEG",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0", 
    "2G" 
  },

  {
    CFG_TCP_BIND_INADDR_ANY,
    "TcpBind_INADDR_ANY",
    "TCP",
    "Bind InAddrAny instead of hostname for server part of connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false", "true" },

  {
    CFG_CONNECTION_OVERLOAD,
    "OverloadLimit",
    "TCP",
    "Number of unsent bytes that must be in the send buffer before the\n"
    "connection is considered overloaded",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

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
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "SHM",
    "Name/IP of computer on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "SHM",
    "PortNumber to be used by data nodes while connecting the transporters",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0", 
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_SHM_SIGNUM,
    "Signum",
    "SHM",
    "Signum ignored, deprecated",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    0,
    "0", 
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "SHM",
    "Id of node (" DB_TOKEN_PRINT ", " API_TOKEN_PRINT " or " MGM_TOKEN_PRINT ") on one side of the connection",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },
  
  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "SHM",
    "Id of node (" DB_TOKEN_PRINT ", " API_TOKEN_PRINT " or " MGM_TOKEN_PRINT ") on one side of the connection",
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
    CFG_CONNECTION_PRESEND_CHECKSUM,
    "PreSendChecksum",
    "SHM",
    "If PreSendChecksum AND Checksum are enabled,\n"
    "pre-send checksum checks are done, and\n"
    "all signals between nodes are checked for errors",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
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
    0,
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
    "4M",
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
    0,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "SHM",
    "System for node 2 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_CONNECTION_OVERLOAD,
    "OverloadLimit",
    "SHM",
    "Number of unsent bytes that must be in the send buffer before the\n"
    "connection is considered overloaded",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  },

  {
    CFG_SHM_SPINTIME,
    "ShmSpintime",
    "SHM",
    "Number of microseconds to spin before going to sleep when receiving",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    "2000"
  },

  {
    CFG_SHM_SEND_BUFFER_SIZE,
    "SendBufferMemory",
    "SHM",
    "Bytes of buffer for signals sent from this node",
    ConfigInfo::CI_USED,
    false,
    ConfigInfo::CI_INT,
    "2M",
    "64K",
    STR_VALUE(MAX_INT_RNIL)
  },

  /****************************************************************************
   * SCI (Deprecated now)
   ***************************************************************************/
  {
    CFG_SECTION_CONNECTION,
    "SCI",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_SECTION,
    (const char *)CONNECTION_TYPE_SCI,
    0, 0
  },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_STRING,
    MANDATORY,
    0, 0 },

  {
    CFG_CONNECTION_GROUP,
    "Group",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    "15",
    "0", "200" },

  {
    CFG_CONNECTION_NODE_ID_SERVER,
    "NodeIdServer",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "1", "63" },

  {
    CFG_CONNECTION_HOSTNAME_1,
    "HostName1",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "SCI",
    "PortNumber to be used by data nodes while connecting the transporters",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_SCI_HOST1_ID_0,
    "Host1SciId0",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST1_ID_1,
    "Host1SciId1",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST2_ID_0,
    "Host2SciId0",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST2_ID_1,
    "Host2SciId1",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_BOOL,
    "true",
    "false",
    "true" },

  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_CONNECTION_PRESEND_CHECKSUM,
    "PreSendChecksum",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_SCI_SEND_LIMIT,
    "SendLimit",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    "8K",
    "128",
    "32K" },

  {
    CFG_SCI_BUFFER_MEM,
    "SharedBufferSize",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
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
    0,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "SCI",
    "System for node 2 in connection",
    ConfigInfo::CI_INTERNAL,
    false,
    ConfigInfo::CI_STRING,
    0,
    0, 0 },

  {
    CFG_CONNECTION_OVERLOAD,
    "OverloadLimit",
    "SCI",
    "SCI not supported",
    ConfigInfo::CI_DEPRECATED,
    false,
    ConfigInfo::CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL)
  }
};

const int ConfigInfo::m_NoOfParams = sizeof(m_ParamInfo) / sizeof(ParamInfo);

/****************************************************************************
 * Ctor
 ****************************************************************************/
#undef require
#define require(x) require_exit_or_core(x, -1)

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

      // Get copy of section
      m_info.getCopy(param._section, &section);
    }

    // Create pinfo (parameter info) entry 
    Properties pinfo(true); 
    pinfo.put("Id",          param._paramId);
    pinfo.put("Fname",       param._fname);

    /*
      Check that flags are set according to current rules
    */
    const Uint32 flags = param._flags;
    const Uint32 allowed_flags = (CI_ONLINE_UPDATEABLE | CI_CHECK_WRITABLE |
                                  CI_RESTART_SYSTEM | CI_RESTART_INITIAL);
    // Check that no other flags then the defined are set
    require((flags & ~allowed_flags) == 0);

    if (flags & CI_ONLINE_UPDATEABLE)
    {
      // Check that online updateable parameter does
      // not have any CI_RESTART_* flag(s)
      require((flags & CI_RESTART_INITIAL) == 0 &&
              (flags & CI_RESTART_SYSTEM) == 0);

      // Currently no online updatable parameters have been implemented
      require(false);
    }

    // only DB nodes should have CI_RESTART_*
    if ((flags & CI_RESTART_INITIAL) || (flags & CI_RESTART_SYSTEM))
      require(strcmp(param._section, DB_TOKEN) == 0);

    pinfo.put("Flags", flags);

    pinfo.put("Type",        param._type);

    // Check that status is an enum and not used as a bitmask
    const Status status = param._status;
    require(status == CI_USED ||
            status == CI_EXPERIMENTAL ||
            status == CI_DEPRECATED ||
            status == CI_NOTIMPLEMENTED ||
            status == CI_INTERNAL);
    pinfo.put("Status", status);

    // Check description
    const char* desc = param._description;
    if (status == CI_DEPRECATED)
    {
      // The description of a deprecated parameter may be the name
      // of another parameter to use or NULL(in such case use empty
      // string as description)
      if (desc == NULL)
        desc = "";
    }
    else
    {
      // The description may not be NULL
      require(desc);
    }
    pinfo.put("Description", desc);

    switch (param._type) {
      case CI_BOOL:
      {
	bool tmp_bool;
	require(InitConfigFileParser::convertStringToBool(param._min, tmp_bool));
	pinfo.put64("Min", tmp_bool);
	require(InitConfigFileParser::convertStringToBool(param._max, tmp_bool));
	pinfo.put64("Max", tmp_bool);

        if(param._default == MANDATORY)
          pinfo.put("Mandatory", (Uint32)1);
        else if(param._default)
        {
          require(InitConfigFileParser::convertStringToBool(param._default,
                                                            tmp_bool));
          pinfo.put("Default", tmp_bool);
        }

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

        if(param._default == MANDATORY)
          pinfo.put("Mandatory", (Uint32)1);
        else if(param._default)
        {
          require(InitConfigFileParser::convertStringToUint64(param._default,
                                                              tmp_uint64));
          pinfo.put64("Default", tmp_uint64);
        }
	break;
      }
      case CI_SECTION:
	pinfo.put("SectionType", (Uint32)UintPtr(param._default));
	break;
      case CI_ENUM:
      {
       assert(param._min); // Enums typelib pointer is stored in _min
       assert(param._max == 0); // Enums can't have max

       // Check that enum values start at 0 and are consecutively
       // ascending for easier reverse engineering from value to string
       Uint32 i = 0;
       for (const Typelib* entry = ConfigInfo::getTypelibPtr(param);
            entry->name != 0; entry++)
       {
         require(i++ == entry->value);
       }

        Properties values(true); // case insensitive
        // Put the list of allowed enum values in pinfo
        for (const Typelib* entry = ConfigInfo::getTypelibPtr(param);
             entry->name != 0; entry++)
          values.put(entry->name, entry->value);
        require(pinfo.put("values", &values));

        if(param._default == MANDATORY)
          pinfo.put("Mandatory", (Uint32)1);
        else if(param._default)
        {
          /*
            Map default value of enum from string to int since
            enum is stored as int internally
          */
          Uint32 default_value;
          require(values.get(param._default, &default_value));
          require(pinfo.put("Default", default_value));

          /* Also store the default as string */
          require(pinfo.put("DefaultString", param._default));
        }
        break;
      }
      case CI_STRING:
        assert(param._min == 0); // String can't have min value
        assert(param._max == 0); // String can't have max value

        if(param._default == MANDATORY)
          pinfo.put("Mandatory", (Uint32)1);
        else if(param._default)
          pinfo.put("Default", param._default);
	break;

      case CI_BITMASK:
        assert(param._min == 0); // Bitmask can't have min value

	Uint64 tmp_uint64;
	require(InitConfigFileParser::convertStringToUint64(param._max,
                                                            tmp_uint64));
	pinfo.put64("Max", tmp_uint64);

        if(param._default == MANDATORY)
          pinfo.put("Mandatory", (Uint32)1);
        else if(param._default)
          pinfo.put("Default", param._default);
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
    delete section;
    
    if(param._type != ConfigInfo::CI_SECTION){
      Properties * p;
      if(!m_systemDefaults.getCopy(param._section, &p)){
	p = new Properties(true);
      }
      if(param._default &&
	 param._default != MANDATORY){
	switch (param._type)
        {
	  case CI_SECTION:
	    break;
	  case CI_STRING:
          case CI_BITMASK:
	    require(p->put(param._fname, param._default));
	    break;
	  case CI_BOOL:
	    {
	      require(InitConfigFileParser::convertStringToBool(param._default, default_bool));
	      require(p->put(param._fname, default_bool));

	      break;
	    }
	  case CI_INT:
	  case CI_INT64:
	    {
	      require(InitConfigFileParser::convertStringToUint64(param._default, default_uint64));
	      require(p->put64(param._fname, Uint64(default_uint64)));
	      break;
	    }
          case CI_ENUM:
          {
            /*
              Map default value of enum from string to int since
              enum is stored as int internally
            */
            Uint32 default_value;
            require(verify_enum(getInfo(param._section),
                                param._fname, param._default,
                                default_value));
            require(p->put(param._fname, default_value));
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
  const char* val = NULL;
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
ConfigInfo::getDefaultString(const Properties * section,
                             const char* fname) const
{
  switch (getType(section, fname))
  {
  case ConfigInfo::CI_BITMASK:
  case ConfigInfo::CI_STRING:
    return getInfoString(section, fname, "Default");

  case ConfigInfo::CI_ENUM:
  {
    /*
      Default value for enum are stored as int internally
      but also stores the orignal string, use different
      key to get at the default value as string
     */
    return getInfoString(section, fname, "DefaultString");
  }

  default:
    require(false);
  }

  return NULL;
}

bool
ConfigInfo::hasDefault(const Properties * section, const char* fname) const {
  const Properties * p;
  require(section->get(fname, &p));
  return p->contains("Default");
}

bool
ConfigInfo::getMandatory(const Properties * section, const char* fname) const {
  const Properties * p;
  require(section->get(fname, &p));
  return p->contains("Mandatory");
}

const char*
ConfigInfo::getDescription(const Properties * section,
                           const char* fname) const {
  return getInfoString(section, fname, "Description");
}

bool
ConfigInfo::isSection(const char * section) const {
  for (int i = 0; i<m_noOfSectionNames; i++) {
    if(!native_strcasecmp(section, m_sectionNames[i])) return true;
  }
  return false;
}

const char*
ConfigInfo::nameToAlias(const char * name) {
  for (int i = 0; m_sectionNameAliases[i].name != 0; i++)
    if(!native_strcasecmp(name, m_sectionNameAliases[i].name))
      return m_sectionNameAliases[i].alias;
  return 0;
}

const char*
ConfigInfo::getAlias(const char * section) {
  for (int i = 0; m_sectionNameAliases[i].name != 0; i++)
    if(!native_strcasecmp(section, m_sectionNameAliases[i].alias))
      return m_sectionNameAliases[i].name;
  return 0;
}

const char*
ConfigInfo::sectionName(Uint32 section_type, Uint32 type) const {

  switch (section_type){
  case CFG_SECTION_SYSTEM:
    return "SYSTEM";
    break;

  case CFG_SECTION_NODE:
    switch(type){
    case NODE_TYPE_DB:
      return DB_TOKEN_PRINT;
      break;
    case NODE_TYPE_MGM:
      return MGM_TOKEN_PRINT;
      break;
    case NODE_TYPE_API:
      return API_TOKEN_PRINT;
      break;
    default:
      assert(false);
      break;
    }
    break;

  case CFG_SECTION_CONNECTION:
    switch(type){
    case CONNECTION_TYPE_TCP:
      return "TCP";
      break;
    case CONNECTION_TYPE_SHM:
      return "SHM";
      break;
    default:
      assert(false);
      break;
    }
    break;

  default:
    assert(false);
    break;
  }

  return "<unknown section>";
}

const ConfigInfo::AliasPair
section2PrimaryKeys[]={
  {API_TOKEN, "NodeId"},
  {DB_TOKEN,  "NodeId"},
  {MGM_TOKEN, "NodeId"},
  {"TCP", "NodeId1,NodeId2"},
  {"SHM", "NodeId1,NodeId2"},
  {0, 0}
};

static const char*
sectionPrimaryKeys(const char * name) {
  for (int i = 0; section2PrimaryKeys[i].name != 0; i++)
    if(!native_strcasecmp(name, section2PrimaryKeys[i].name))
      return section2PrimaryKeys[i].alias;
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


bool
ConfigInfo::verify_enum(const Properties * section, const char* fname,
                        const char* value, Uint32& value_int) const {
  const Properties * p;
  const Properties * values;
  require(section->get(fname, &p));
  require(p->get("values", &values));

  if (values->get(value, &value_int))
    return true;
  return false;
}


void
ConfigInfo::get_enum_values(const Properties * section, const char* fname,
                      BaseString& list) const {
  const Properties * p;
  const Properties * values;
  require(section->get(fname, &p));
  require(p->get("values", &values));

  const char* separator = "";
  Properties::Iterator it(values);
  for (const char* name = it.first(); name != NULL; name = it.next())
  {
    list.appfmt("%s%s", separator, name);
    separator = " ";
  }
}


ConfigInfo::Type 
ConfigInfo::getType(const Properties * section, const char* fname) const {
  return (ConfigInfo::Type) getInfoInt(section, fname, "Type");
}

ConfigInfo::Status
ConfigInfo::getStatus(const Properties * section, const char* fname) const {
  return (ConfigInfo::Status) getInfoInt(section, fname, "Status");
}

Uint32
ConfigInfo::getFlags(const Properties* section, const char* fname) const {
  return (Uint32)getInfoInt(section, fname, "Flags");
}

/****************************************************************************
 * Printers
 ****************************************************************************/

class ConfigPrinter {
protected:
  FILE* m_out;
public:
  ConfigPrinter(FILE* out = stdout) :
    m_out(out)
    {}
  virtual ~ConfigPrinter() {};

  virtual void start() {}
  virtual void end() {}

  virtual void section_start(const char* name, const char* alias,
                             const char* primarykeys = NULL) {}
  virtual void section_end(const char* name) {}

  virtual void parameter(const char* section_name,
                         const Properties* section,
                         const char* param_name,
                         const ConfigInfo& info){}
};


class PrettyPrinter : public ConfigPrinter {
public:
  PrettyPrinter(FILE* out = stdout) : ConfigPrinter(out) {}
  virtual ~PrettyPrinter() {}

  virtual void section_start(const char* name, const char* alias,
                             const char* primarykeys = NULL) {
    fprintf(m_out, "****** %s ******\n\n", name);
  }

  virtual void parameter(const char* section_name,
                         const Properties* section,
                         const char* param_name,
                         const ConfigInfo& info)
  {
    // Don't print deprecated parameters
    const Uint32 status = info.getStatus(section, param_name);
    if (status == ConfigInfo::CI_DEPRECATED)
      return;

    switch (info.getType(section, param_name)) {
    case ConfigInfo::CI_BOOL:
      fprintf(m_out, "%s (Boolean value)\n", param_name);
      fprintf(m_out, "%s\n", info.getDescription(section, param_name));

      if (info.getMandatory(section, param_name))
        fprintf(m_out, "MANDATORY (Legal values: Y, N)\n");
      else if (info.hasDefault(section, param_name))
      {
        if (info.getDefault(section, param_name) == false)
          fprintf(m_out, "Default: N (Legal values: Y, N)\n");
        else if (info.getDefault(section, param_name) == true)
          fprintf(m_out, "Default: Y (Legal values: Y, N)\n");
        else
          fprintf(m_out, "UNKNOWN\n");
      }
      break;

    case ConfigInfo::CI_INT:
    case ConfigInfo::CI_INT64:
      fprintf(m_out, "%s (Non-negative Integer)\n", param_name);
      fprintf(m_out, "%s\n", info.getDescription(section, param_name));
      if (info.getMandatory(section, param_name))
        fprintf(m_out, "MANDATORY (");
      else if (info.hasDefault(section, param_name))
        fprintf(m_out, "Default: %llu (",
                info.getDefault(section, param_name));
      else
        fprintf(m_out, "(");
      fprintf(m_out, "Min: %llu, ", info.getMin(section, param_name));
      fprintf(m_out, "Max: %llu)\n", info.getMax(section, param_name));
      break;

    case ConfigInfo::CI_BITMASK:
    case ConfigInfo::CI_ENUM:
    case ConfigInfo::CI_STRING:
      fprintf(m_out, "%s (String)\n", param_name);
      fprintf(m_out, "%s\n", info.getDescription(section, param_name));
      if (info.getMandatory(section, param_name))
        fprintf(m_out, "MANDATORY\n");
      else if (info.hasDefault(section, param_name))
        fprintf(m_out, "Default: %s\n",
              info.getDefaultString(section, param_name));
      break;
    case ConfigInfo::CI_SECTION:
      return;
    }

    Uint32 flags = info.getFlags(section, param_name);
    bool comma = false;
    bool new_line_needed = false;
    if (flags & ConfigInfo::CI_CHECK_WRITABLE)
    {
      comma= true;
      new_line_needed = true;
      fprintf(m_out, "writable");
    }
    if (flags & ConfigInfo::CI_RESTART_SYSTEM)
    {
      if (comma)
        fprintf(m_out, ", system");
      else
      {
        comma = true;
        fprintf(m_out, "system");
      }
      new_line_needed = true;
    }
    if (flags & ConfigInfo::CI_RESTART_INITIAL)
    {
      if (comma)
        fprintf(m_out, ", initial");
      else
      {
        comma = true;
        fprintf(m_out, "initial");
      }
      new_line_needed = true;
    }
    if (new_line_needed)
      fprintf(m_out, "\n");
    fprintf(m_out, "\n");
  }
};


class XMLPrinter : public ConfigPrinter {
  int m_indent;

  void print_xml(const char* name, const Properties& pairs,
                 bool close = true) {
    const char* value;
    Properties::Iterator it(&pairs);
    for (int i= 0; i < m_indent; i++)
      fprintf(m_out, "  ");
    fprintf(m_out, "<%s", name);
    for (const char* name = it.first(); name != NULL; name = it.next()) {
      require(pairs.get(name, &value));
      fprintf(m_out, " %s=\"%s\"", name, value);
    }
    if (close)
      fprintf(m_out, "/");
    fprintf(m_out, ">\n");
  }

public:
  XMLPrinter(FILE* out = stdout) : ConfigPrinter(out), m_indent(0) {}
  virtual ~XMLPrinter() {
    assert(m_indent == 0);
  }

  virtual void start() {
    BaseString buf;
    Properties pairs;
    pairs.put("protocolversion", "1");
    pairs.put("ndbversionstring", ndbGetOwnVersionString());
    Uint32 ndbversion = ndbGetOwnVersion();
    buf.assfmt("%u", ndbversion);
    pairs.put("ndbversion", buf.c_str());
    buf.assfmt("%u", ndbGetMajor(ndbversion));
    pairs.put("ndbversionmajor", buf.c_str());
    buf.assfmt("%u", ndbGetMinor(ndbversion));
    pairs.put("ndbversionminor", buf.c_str());
    buf.assfmt("%u", ndbGetBuild(ndbversion));
    pairs.put("ndbversionbuild", buf.c_str());

    print_xml("configvariables", pairs, false);
    m_indent++;
  }
  virtual void end() {
    m_indent--;
    Properties pairs;
    print_xml("/configvariables", pairs, false);
  }

  virtual void section_start(const char* name, const char* alias,
                             const char* primarykeys = NULL) {
    Properties pairs;
    pairs.put("name", alias ? alias : name);
    if (primarykeys)
      pairs.put("primarykeys", primarykeys);
    print_xml("section", pairs, false);
    m_indent++;
  }
  virtual void section_end(const char* name) {
    m_indent--;
    Properties pairs;
    print_xml("/section", pairs, false);
  }

  virtual void parameter(const char* section_name,
                         const Properties* section,
                         const char* param_name,
                         const ConfigInfo& info){
    BaseString buf;
    Properties pairs;
    pairs.put("name", param_name);
    pairs.put("comment", info.getDescription(section, param_name));

    const ConfigInfo::Type param_type = info.getType(section, param_name);
    switch (param_type) {
    case ConfigInfo::CI_BOOL:
      pairs.put("type", "bool");

      if (info.getMandatory(section, param_name))
        pairs.put("mandatory", "true");
      else if (info.hasDefault(section, param_name))
      {
        if (info.getDefault(section, param_name) == false)
          pairs.put("default", "false");
        else if (info.getDefault(section, param_name) == true)
          pairs.put("default", "true");
      }
      break;

    case ConfigInfo::CI_INT:
    case ConfigInfo::CI_INT64:
      pairs.put("type", "unsigned");

      if (info.getMandatory(section, param_name))
        pairs.put("mandatory", "true");
      else if (info.hasDefault(section, param_name))
      {
        buf.assfmt("%llu", info.getDefault(section, param_name));
        pairs.put("default", buf.c_str());
      }
      buf.assfmt("%llu", info.getMin(section, param_name));
      pairs.put("min", buf.c_str());
      buf.assfmt("%llu", info.getMax(section, param_name));
      pairs.put("max", buf.c_str());
    break;

    case ConfigInfo::CI_BITMASK:
    case ConfigInfo::CI_ENUM:
    case ConfigInfo::CI_STRING:
      pairs.put("type", "string");

      if (info.getMandatory(section, param_name))
        pairs.put("mandatory", "true");
      else if (info.hasDefault(section, param_name))
        pairs.put("default", info.getDefaultString(section, param_name));

      if (param_type == ConfigInfo::CI_ENUM)
      {
        // Concatenate the allowed enum values to a space separated string
        info.get_enum_values(section, param_name, buf);
        require(pairs.put("allowed_values", buf.c_str()));
      }
      break;

    case ConfigInfo::CI_SECTION:
      return; // Don't print anything for the section itself
    }

    // Get "check" flag(s)
    Uint32 flags = info.getFlags(section, param_name);
    buf.clear();
    if (flags & ConfigInfo::CI_CHECK_WRITABLE)
      buf.append("writable");

    if (buf.length())
      pairs.put("check", buf.c_str());

    // Get "restart" flag
    if (flags & ConfigInfo::CI_RESTART_SYSTEM)
      pairs.put("restart", "system");

    // Get "initial" flag
    if (flags & ConfigInfo::CI_RESTART_INITIAL)
      pairs.put("initial", "true");

    // Get "supported" flag
    const Uint32 status = info.getStatus(section, param_name);
    buf.clear();
    if (status == ConfigInfo::CI_EXPERIMENTAL)
      buf.append("experimental");

    if (buf.length())
      pairs.put("supported", buf.c_str());

    if (status == ConfigInfo::CI_DEPRECATED)
      pairs.put("deprecated", "true");

    print_xml("param", pairs);
  }
};

void ConfigInfo::print(const char* section) const {
  PrettyPrinter pretty_printer;
  print_impl(section, pretty_printer);
}

void ConfigInfo::print_xml(const char* section) const {
  XMLPrinter xml_printer;
  print_impl(section, xml_printer);
}


bool
ConfigInfo::is_internal_section(const Properties* sec) const
{
  /* Check if the section is marked as internal */
  Properties::Iterator it(sec);
  for (const char* n = it.first(); n != NULL; n = it.next()) {
    if (getStatus(sec, n) == ConfigInfo::CI_INTERNAL &&
        getType(sec, n) == ConfigInfo:: CI_SECTION)
      return true;
  }
  return false;
}


void ConfigInfo::print_impl(const char* section_filter,
                            ConfigPrinter& printer) const {
  printer.start();
  /* Iterate through all sections */
  Properties::Iterator it(&m_info);
  for (const char* s = it.first(); s != NULL; s = it.next()) {
    if (section_filter && strcmp(section_filter, s))
      continue; // Skip this section

    const Properties * sec = getInfo(s);

    if (is_internal_section(sec))
      continue; // Skip whole section

    const char* section_alias = nameToAlias(s);
    printer.section_start(s, section_alias, sectionPrimaryKeys(s));
 
    /* Iterate through all parameters in section */
    Properties::Iterator it(sec);
    for (const char* n = it.first(); n != NULL; n = it.next()) {
      // Skip entries with different F- and P-names
      if (getStatus(sec, n) == ConfigInfo::CI_INTERNAL) continue;
      if (getStatus(sec, n) == ConfigInfo::CI_NOTIMPLEMENTED) continue;
      printer.parameter(s, sec, n, *this);
    }
    printer.section_end(s);

    // Print [<section> DEFAULT] for all sections but SYSTEM
    if (strcmp(s, "SYSTEM") == 0)
      continue; // Skip SYSTEM section

    BaseString default_section_name;
    default_section_name.assfmt("%s %s",
                                section_alias ? section_alias : s,
                                "DEFAULT");
    printer.section_start(s, default_section_name.c_str());

    /* Iterate through all parameters in section */
    for (const char* n = it.first(); n != NULL; n = it.next()) {
      // Skip entries with different F- and P-names
      if (getStatus(sec, n) == ConfigInfo::CI_INTERNAL) continue;
      if (getStatus(sec, n) == ConfigInfo::CI_NOTIMPLEMENTED) continue;
      printer.parameter(s, sec, n, *this);
    }
    printer.section_end(s);

  }
  printer.end();
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
      fprintf(stderr,"Cluster configuration warning line %d: "
	       "Could not use next node id %d for section [%s], "
	       "using next unused node id %d.\n",
	       ctx.m_sectionLineno, nextNodeId, ctx.fname, id);
    }
    ctx.m_currentSection->put("NodeId", id);
  } else if(ctx.m_userProperties.get("AllocatedNodeId_", id, &line)) {
    ctx.reportError("Duplicate nodeid in section "
		    "[%s] starting at line: %d. Previously used on line %d.",
		    ctx.fname, ctx.m_sectionLineno, line);
    return false;
  }

  if(id >= MAX_NODES)
  {
    ctx.reportError("too many nodes configured, only up to %d nodes supported.",
            MAX_NODES);
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
    ctx.reportError("HostName missing in [COMPUTER] (Id: %s) "
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
  if (native_strcasecmp("TCP",ctx.fname) == 0)
  {
    // always enabled
  }
  else if (native_strcasecmp("SHM",ctx.fname) == 0)
  {
    // always enabled
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
  ctx.m_currentSection->put("Type", ctx.fname);
  
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
      (void) ctx.m_info->getStatus(ctx.m_currentInfo, name);
      if(!ctx.m_currentSection->contains(name)){
	switch (ctx.m_info->getType(ctx.m_currentInfo, name)){
	case ConfigInfo::CI_ENUM:
	case ConfigInfo::CI_INT:
	case ConfigInfo::CI_BOOL:{
	  Uint32 val = 0;
	  require(defaults->get(name, &val));
	  ctx.m_currentSection->put(name, val);
          DBUG_PRINT("info",("%s=%d #default",name,val));
	  break;
	}
	case ConfigInfo::CI_INT64:{
	  Uint64 val = 0;
	  require(defaults->get(name, &val));
	  ctx.m_currentSection->put64(name, val);
          DBUG_PRINT("info",("%s=%lld #default",name,val));
	  break;
	}
        case ConfigInfo::CI_BITMASK:
	case ConfigInfo::CI_STRING:{
	  const char * val;
	  require(defaults->get(name, &val));
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
        case ConfigInfo::CI_ENUM:
        case ConfigInfo::CI_INT:
        case ConfigInfo::CI_BOOL:{
          Uint32 val = 0;
          require(ctx.m_currentSection->get(name, &val));
          DBUG_PRINT("info",("%s=%d",name,val));
          break;
        }
        case ConfigInfo::CI_INT64:{
          Uint64 val = 0;
          require(ctx.m_currentSection->get(name, &val));
          DBUG_PRINT("info",("%s=%lld",name,val));
          break;
        }
        case ConfigInfo::CI_BITMASK:
        case ConfigInfo::CI_STRING:{
          const char * val;
          require(ctx.m_currentSection->get(name, &val));
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
    require(ctx.m_currentInfo->get(name, &info));
    Uint32 val;
    if(info->get("Mandatory", &val)){
      const char * fname;
      require(info->get("Fname", &fname));
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
  if(!ctx.m_currentSection->get(buf, &nodeId))
  {
    ctx.reportError("Mandatory parameter %s missing from section"
                    "[%s] starting at line: %d",
                    buf, ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  BaseString str(nodeId);
  Vector<BaseString> token_list;
  int tokens = str.split(token_list, ".", 2);

  Uint32 id;

  if (tokens == 0)
  {
    ctx.reportError("Value for mandatory parameter %s missing from section "
                    "[%s] starting at line: %d",
                    buf, ctx.fname, ctx.m_sectionLineno);
    return false;
  }

  const char* token1 = token_list[0].c_str();
  if (tokens == 1) {                // Only a number given
    errno = 0;
    char* p;
    id = strtol(token1, &p, 10);
    if (errno != 0 || id <= 0x0  || id > MAX_NODES)
    {
      ctx.reportError("Illegal value for mandatory parameter %s from section "
                    "[%s] starting at line: %d",
                    buf, ctx.fname, ctx.m_sectionLineno);
      return false;
    }
    require(ctx.m_currentSection->put(buf, id, true));
  } else {                             // A pair given (e.g. "uppsala.32")
    assert(tokens == 2 && token_list.size() == 2);
    const char* token2 = token_list[1].c_str();

    errno = 0;
    char* p;
    id = strtol(token2, &p, 10);
    if (errno != 0 || id <= 0x0  || id > MAX_NODES)
    {
      ctx.reportError("Illegal value for mandatory parameter %s from section "
                    "[%s] starting at line: %d",
                    buf, ctx.fname, ctx.m_sectionLineno);
      return false;
    }
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

  Uint32 bindAnyAddr = 0;
  node1->get("TcpBind_INADDR_ANY", &bindAnyAddr);
  if (bindAnyAddr)
  {
    ctx.m_currentSection->put("TcpBind_INADDR_ANY", 1, true);
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

      port= base + adder;
      ctx.m_userProperties.put("ServerPort_", id1, port);
    }
  }

  require(ctx.m_currentSection->contains("PortNumber") == false);
  ctx.m_currentSection->put("PortNumber", port);

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
    Uint32 portno= NDB_PORT;
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

  /**
   * In kernel, will calculate the MaxNoOfMeataTables use the following sum:
   * Uint32 noOfMetaTables = noOfTables + noOfOrderedIndexes + 
   *                         noOfUniqueHashIndexes + 2
   * 2 is the number of the SysTables.
   * So must check that the sum does't exceed the max value of Uint32.
   */
  Uint32 noOfTables = 0,
         noOfOrderedIndexes = 0,
         noOfUniqueHashIndexes = 0;
  ctx.m_currentSection->get("MaxNoOfTables", &noOfTables);
  ctx.m_currentSection->get("MaxNoOfOrderedIndexes", &noOfOrderedIndexes);
  ctx.m_currentSection->get("MaxNoOfUniqueHashIndexes", &noOfUniqueHashIndexes);

  Uint64 sum= (Uint64)noOfTables + noOfOrderedIndexes + noOfUniqueHashIndexes;
  
  if (sum > ((Uint32)~0 - 2)) {
    ctx.reportError("The sum of MaxNoOfTables, MaxNoOfOrderedIndexes and"
		    " MaxNoOfUniqueHashIndexes must not exceed %u - [%s]"
                    " starting at line: %d",
		    ((Uint32)~0 - 2), ctx.fname, ctx.m_sectionLineno);
    return false;
  } 

  return true;
}

#include <NdbThread.h>

static
bool
checkThreadPrioSpec(InitConfigFileParser::Context & ctx, const char * unused)
{
  (void)unused;
  const char * spec = 0;
  if (ctx.m_currentSection->get("HeartbeatThreadPriority", &spec))
  {
    int ret = NdbThread_SetHighPrioProperties(spec);
    NdbThread_SetHighPrioProperties(0); // reset
    if (ret)
    {
      ctx.reportError("Unable to parse HeartbeatThreadPriority: %s", spec);
      return false;
    }
    return true;
  }
  return true;
}

#include "../kernel/vm/mt_thr_config.hpp"

static bool
check_2n_number_less_32(Uint32 num)
{
  switch (num)
  {
    case 0:
    case 1:
    case 2:
    case 4:
    case 6:
    case 8:
    case 10:
    case 12:
    case 16:
    case 20:
    case 24:
    case 32:
      return true;
    default:
      return false;
  }
  return false;
}

static
bool
checkThreadConfig(InitConfigFileParser::Context & ctx, const char * unused)
{
  (void)unused;
  Uint32 maxExecuteThreads = 0;
  Uint32 lqhThreads = 0;
  Uint32 classic = 0;
  Uint32 ndbLogParts = 0;
  Uint32 realtimeScheduler = 0;
  Uint32 spinTimer = 0;
  const char * thrconfig = 0;
  const char * locktocpu = 0;

  THRConfig tmp;
  if (ctx.m_currentSection->get("LockExecuteThreadToCPU", &locktocpu))
  {
    tmp.setLockExecuteThreadToCPU(locktocpu);
  }

  ctx.m_currentSection->get("MaxNoOfExecutionThreads", &maxExecuteThreads);
  ctx.m_currentSection->get("__ndbmt_lqh_threads", &lqhThreads);
  ctx.m_currentSection->get("__ndbmt_classic", &classic);
  ctx.m_currentSection->get("NoOfFragmentLogParts", &ndbLogParts);
  ctx.m_currentSection->get("RealtimeScheduler", &realtimeScheduler);
  ctx.m_currentSection->get("SchedulerSpinTimer", &spinTimer);

  if (!check_2n_number_less_32(lqhThreads))
  {
    ctx.reportError("NumLqhThreads must be 0,1,2,4,6,8,10,12,16,20,24 or 32");
    return false;
  }
  if (!check_2n_number_less_32(ndbLogParts) ||
      ndbLogParts < 4)
  {
    ctx.reportError("NoOfLogParts must be 4,6,8,10,12,16,20,24 or 32");
    return false;
  }
  if (ctx.m_currentSection->get("ThreadConfig", &thrconfig))
  {
    int ret = tmp.do_parse(thrconfig, realtimeScheduler, spinTimer);
    if (ret)
    {
      ctx.reportError("Unable to parse ThreadConfig: %s",
                      tmp.getErrorMessage());
      return false;
    }

    if (maxExecuteThreads)
    {
      ctx.reportWarning("ThreadConfig overrides MaxNoOfExecutionThreads");
    }

    if (lqhThreads)
    {
      ctx.reportWarning("ThreadConfig overrides __ndbmt_lqh_threads");
    }

    if (classic)
    {
      ctx.reportWarning("ThreadConfig overrides __ndbmt_classic");
    }
  }
  else if (maxExecuteThreads || lqhThreads || classic)
  {
    int ret = tmp.do_parse(maxExecuteThreads,
                           lqhThreads,
                           classic,
                           realtimeScheduler,
                           spinTimer);
    if (ret)
    {
      ctx.reportError("Unable to set thread configuration: %s",
                      tmp.getErrorMessage());
      return false;
    }
  }

  if (tmp.getInfoMessage())
  {
    ctx.reportWarning("%s", tmp.getInfoMessage());
  }

  if (thrconfig == 0)
  {
    ctx.m_currentSection->put("ThreadConfig", tmp.getConfigString());
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

/**
 * Connection rule: allow only one connection between each node pair.
 */
static bool
uniqueConnection(InitConfigFileParser::Context & ctx, const char * data)
{
  Uint32 lo_node, hi_node;
  BaseString key;       /* Properties key to identify this link */
  BaseString defn;      /* Value stored at key (used in error msgs) */
  
  /* This rule runs *after* fixNodeId, so it is guaranteed that the 
   NodeId1 and NodeId2 properties exist and contain integers */  
  require(ctx.m_currentSection->get("NodeId1", &lo_node) == true);
  require(ctx.m_currentSection->get("NodeId2", &hi_node) == true);
  
  if(lo_node > hi_node) /* sort the node ids, low-node-first */
  {
    const Uint32 tmp_node = hi_node;
    hi_node = lo_node;
    lo_node = tmp_node;
  }
  
  key.assfmt("Link_%d_%d", lo_node, hi_node);
  
  /* The property must not already exist */
  if(ctx.m_userProperties.contains(key.c_str()))
  {
    const char * old_defn;
    if(ctx.m_userProperties.get(key.c_str(), &old_defn))    
      ctx.reportError("%s connection is a duplicate of the existing %s",
                      data, old_defn);
    return false;
  }
  
  /* Set the unique link identifier property */
  defn.assfmt("%s link from line %d", data, ctx.m_sectionLineno);
  ctx.m_userProperties.put(key.c_str(), defn.c_str());
  
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
    ctx.reportError("Unable to handle type conversion w.r.t deprecation %s %s"
		    "- [%s] starting at line: %d",
		    oldName, newName,
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  Uint64 oldVal;
  require(ctx.m_currentSection->get(oldName, &oldVal));

  Uint64 newVal = (Uint64)((Int64)oldVal * mul + add);
  if(!ctx.m_info->verify(ctx.m_currentInfo, newName, newVal)){
    ctx.reportError("Unable to handle deprecation, new value not within bounds"
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
fixDeprecated(InitConfigFileParser::Context & ctx, const char * data){
  const char * name;
  /**
   * Transform old values to new values
   * Transform new values to old values (backward compatible)
   */
  Properties tmp(true);
  Properties::Iterator it(ctx.m_currentSection);
  for (name = it.first(); name != NULL; name = it.next()) {
    const DeprecationTransform * p = &f_deprecation[0];
    while(p->m_section != 0){
      if(strcmp(p->m_section, ctx.fname) == 0){
	double mul = p->m_mul;
	double add = p->m_add;
	if(native_strcasecmp(name, p->m_oldName) == 0){
	  if(!transform(ctx, tmp, name, p->m_newName, add, mul)){
	    return false;
	  }
	} else if(native_strcasecmp(name, p->m_newName) == 0) {
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
      require(ctx.m_currentSection->put(name, val));
      break;
    }
    case PropertiesType_char:{
      const char * val;
      require(tmp.get(name, &val));
      require(ctx.m_currentSection->put(name, val));
      break;
    }
    case PropertiesType_Uint64:{
      Uint64 val;
      require(tmp.get(name, &val));
      require(ctx.m_currentSection->put64(name, val));
      break;
    }
    case PropertiesType_Properties:
    default:
      require(false);
    }
  }
  return true;
}

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

      id = 0;
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
	require(false);
      }
      require(ok);
    }
    ctx.m_configValues.closeSection();
  } while(0);
  return true;
}


static bool
add_system_section(Vector<ConfigInfo::ConfigRuleSection>&sections,
                   struct InitConfigFileParser::Context &ctx,
                   const char * rule_data)
{
  if (!ctx.m_config->contains("SYSTEM")) {
    ConfigInfo::ConfigRuleSection s;

    // Generate a unique name for this new cluster
    time_t now;
    time(&now);

    tm tm_buf;
    ndb_localtime_r(&now, &tm_buf);

    char name_buf[18];
    BaseString::snprintf(name_buf, sizeof(name_buf),
                         "MC_%d%.2d%.2d%.2d%.2d%.2d",
                         tm_buf.tm_year + 1900,
                         tm_buf.tm_mon + 1,
                         tm_buf.tm_mday,
                         tm_buf.tm_hour,
                         tm_buf.tm_min,
                         tm_buf.tm_sec);

    s.m_sectionType = BaseString("SYSTEM");
    s.m_sectionData = new Properties(true);
    s.m_sectionData->put("Name", name_buf);
    s.m_sectionData->put("Type", "SYSTEM");

    // ndbout_c("Generated new SYSTEM section with name '%s'", name_buf);

    sections.push_back(s);
  }
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
    ctx.reportError("At least one database node (ndbd) should be defined in config file");
    return false;
  }
  if (!ctx.m_userProperties.get("MGM", &mgm_nodes)) {
    ctx.reportError("At least one management server node (ndb_mgmd) should be defined in config file");
    return false;
  }
  if (!ctx.m_userProperties.get("API", &api_nodes)) {
    ctx.reportError("At least one application node (for the mysqld) should be defined in config file");
    return false;
  }
  return true;
}

static
int
check_connection(struct InitConfigFileParser::Context &ctx,
		 const char * map,
		 Uint32 nodeId1, const char * hostname,
		 Uint32 nodeId2)
{
  Bitmask<(MAX_NODES+31)/32> bitmap;

  BaseString str(map);
  Vector<BaseString> arr;
  str.split(arr, ",");
  for (Uint32 i = 0; i<arr.size(); i++)
  {
    char *endptr = 0;
    long val = strtol(arr[i].c_str(), &endptr, 10);
    if (*endptr)
    {
      ctx.reportError("Unable to parse ConnectionMap(\"%s\" for "
		      "node: %d, hostname: %s",
		      map, nodeId1, hostname);
      return -1;
    }
    if (! (val > 0 && val < MAX_NDB_NODES))
    {
      ctx.reportError("Invalid node in in ConnectionMap(\"%s\" for "
		      "node: %d, hostname: %s",
		      map, nodeId1, hostname);
      return -1;
    }
    bitmap.set(val);
  }
  return bitmap.get(nodeId2);
}

static
bool
add_a_connection(Vector<ConfigInfo::ConfigRuleSection>&sections,
		 struct InitConfigFileParser::Context &ctx,
		 Uint32 nodeId1, Uint32 nodeId2, bool use_shm)
{
  int ret;
  ConfigInfo::ConfigRuleSection s;
  const char * map = 0;
  const char *hostname1= 0, *hostname2= 0;
  const Properties *tmp;
  
  Uint32 wan = 0;
  Uint32 location_domain1 = 0;
  Uint32 location_domain2 = 0;
  require(ctx.m_config->get("Node", nodeId1, &tmp));
  tmp->get("HostName", &hostname1);
  tmp->get("LocationDomainId", &location_domain1);
  if (!wan)
  {
    tmp->get("wan", &wan);
  }

  if (tmp->get("ConnectionMap", &map))
  {
    if ((ret = check_connection(ctx, map, nodeId1, hostname1, nodeId2)) != 1)
    {
      return ret == 0 ? true : false;
    }
  }

  require(ctx.m_config->get("Node", nodeId2, &tmp));
  tmp->get("HostName", &hostname2);
  tmp->get("LocationDomainId", &location_domain2);
  if (!wan)
  {
    tmp->get("wan", &wan);
  }

  if (!wan)
  {
    if (location_domain1 != 0 &&
        location_domain2 != 0 &&
        location_domain1 !=
          location_domain2)
    {
      wan = 1;
    }
  }
  
  if (tmp->get("ConnectionMap", &map))
  {
    if ((ret = check_connection(ctx, map, nodeId2, hostname2, nodeId1)) != 1)
    {
      return ret == 0 ? true : false;
    }
  }
  
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

    if (wan)
    {
      s.m_sectionData->put("TCP_RCV_BUF_SIZE", 4194304);
      s.m_sectionData->put("TCP_SND_BUF_SIZE", 4194304);
      s.m_sectionData->put("TCP_MAXSEG_SIZE", 61440);
    }
  }
  
  sections.push_back(s);
  return true;
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

  for (i = 0;; i++){
    const Properties * tmp;
    Uint32 nodeId1, nodeId2;

    if(!props->get("Connection", i, &tmp)) break;

    if(!tmp->get("NodeId1", &nodeId1)) continue;
    if(!tmp->get("NodeId2", &nodeId2)) continue;
    p_connections.put("", nodeId1 + (nodeId2<<16), nodeId1);
    p_connections.put("", nodeId2 + (nodeId1<<16), nodeId2);
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
    {
      p_db_nodes.put("", i_db++, i);
    }
    else if (strcmp(type,API_TOKEN) == 0)
      p_api_nodes.put("", i_api++, i);
    else if (strcmp(type,MGM_TOKEN) == 0)
      p_mgm_nodes.put("", i_mgm++, i);
  }

  Uint32 nodeId1, nodeId2, dummy;

  // DB -> DB
  for (i= 0; p_db_nodes.get("", i, &nodeId1); i++){
    for (Uint32 j= i+1;; j++){
      if(!p_db_nodes.get("", j, &nodeId2)) break;
      if(!p_connections.get("", nodeId1+(nodeId2<<16), &dummy)) 
      {
	if (!add_a_connection(sections,ctx,nodeId1,nodeId2,false))
	  goto err;
      }
    }
  }

  // API -> DB
  for (i= 0; p_api_nodes.get("", i, &nodeId1); i++)
  {
    for (Uint32 j= 0; p_db_nodes.get("", j, &nodeId2); j++)
    {
      Uint32 use_shm = 0;
      {
        const Properties *shm_check;
        if (props->get("Node", nodeId2, &shm_check))
        {
          shm_check->get("UseShm", &use_shm);
        }
      }
      if(!p_connections.get("", nodeId1+(nodeId2<<16), &dummy))
      {
	if (!add_a_connection(sections,
                              ctx,
                              nodeId1,
                              nodeId2,
                              (bool)use_shm))
	  goto err;
      }
    }
  }

  // MGM -> DB
  for (i= 0; p_mgm_nodes.get("", i, &nodeId1); i++)
  {
    for (Uint32 j= 0; p_db_nodes.get("", j, &nodeId2); j++)
    {
      if(!p_connections.get("", nodeId1+(nodeId2<<16), &dummy))
      {
	if (!add_a_connection(sections,ctx,nodeId1,nodeId2,0))
	  goto err;
      }
    }
  }

  // MGM -> MGM
  for (i= 0; p_mgm_nodes.get("", i, &nodeId1); i++){
    for (Uint32 j= i+1;; j++){
      if(!p_mgm_nodes.get("", j, &nodeId2)) break;
      if(!p_connections.get("", nodeId1+(nodeId2<<16), &dummy))
      {
	if (!add_a_connection(sections,ctx,nodeId1,nodeId2,0))
	  goto err;
     }
    }
  }

  DBUG_RETURN(true);
err:
  DBUG_RETURN(false);
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
  Uint32 i, n;
  Uint32 n_nodes;
  Uint32 replicas= 0;
  Uint32 db_host_count= 0;
  bool  with_arbitration_rank= false;
  ctx.m_userProperties.get("NoOfNodes", &n_nodes);
  ctx.m_userProperties.get("NoOfReplicas", &replicas);

  /**
   * Register user supplied values
   */
  Uint8 ng_cnt[MAX_NDB_NODES];
  Bitmask<(MAX_NDB_NODES+31)/32> nodes_wo_ng;
  bzero(ng_cnt, sizeof(ng_cnt));

  for (i= 0, n= 0; n < n_nodes; i++)
  {
    const Properties * tmp;
    if(!ctx.m_config->get("Node", i, &tmp)) continue;
    n++;

    const char * type;
    if(!tmp->get("Type", &type)) continue;

    if (strcmp(type,DB_TOKEN) == 0)
    {
      Uint32 id;
      tmp->get("NodeId", &id);

      Uint32 ng;
      if (tmp->get("Nodegroup", &ng))
      {
        if (ng == NDB_NO_NODEGROUP)
        {
          break;
        }
        else if (ng >= MAX_NDB_NODES)
        {
          ctx.reportError("Invalid nodegroup %u for node %u",
                          ng, id);
          return false;
        }
        ng_cnt[ng]++;
      }
      else
      {
        nodes_wo_ng.set(i);
      }
    }
  }

  /**
   * Auto-assign nodegroups if user didnt
   */
  Uint32 next_ng = 0;
  for (;ng_cnt[next_ng] >= replicas; next_ng++);
  for (i = nodes_wo_ng.find(0); i!=BitmaskImpl::NotFound;
       i = nodes_wo_ng.find(i + 1))
  {
    Properties* tmp = 0;
    ctx.m_config->getCopy("Node", i, &tmp);

    tmp->put("Nodegroup", next_ng, true);
    ctx.m_config->put("Node", i, tmp, true);
    ng_cnt[next_ng]++;

    Uint32 id;
    tmp->get("NodeId", &id);

    for (;ng_cnt[next_ng] >= replicas; next_ng++);

    delete tmp;
  }

  /**
   * Check node vs replicas
   */
  for (i = 0; i<MAX_NDB_NODES; i++)
  {
    if (ng_cnt[i] != 0 && ng_cnt[i] != (Uint8)replicas)
    {
      ctx.reportError("Nodegroup %u has %u members, NoOfReplicas=%u",
                      i, ng_cnt[i], replicas);
      return false;
    }
  }

  // check that node groups and arbitrators are ok
  // just issue warning if not
  if(replicas > 1){
    Properties p_db_hosts(true); // store hosts which db nodes run on
    Properties p_arbitrators(true); // store hosts which arbitrators run on
    // arbitrator should not run together with db node on same host
    Uint32 group= 0, i_group= 0;
    BaseString node_group_warning, arbitration_warning;
    const char *arbit_warn_fmt=
      "\n  arbitrator with id %d and db node with id %d on same host %s";
    const char *arbit_warn_fmt2=
      "\n  arbitrator with id %d has no hostname specified";

    ctx.m_userProperties.get("NoOfNodes", &n_nodes);
    for (i= 0, n= 0; n < n_nodes; i++){
      const Properties * tmp;
      if(!ctx.m_config->get("Node", i, &tmp)) continue;
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
          with_arbitration_rank = true;  //check whether MGM or API node configured with rank >0 
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
      ctx.reportWarning("Cluster configuration warning:\n%s",node_group_warning.c_str());
    if (!with_arbitration_rank) 
    {
      ctx.reportWarning("Cluster configuration warning:" 
         "\n  Neither %s nor %s nodes are configured with arbitrator,"
         "\n  may cause complete cluster shutdown in case of host failure.", 
         MGM_TOKEN, API_TOKEN);
    }
    if (db_host_count > 1 && arbitration_warning.length() > 0)
      ctx.reportWarning("Cluster configuration warning:%s%s",arbitration_warning.c_str(),
	       "\n  Running arbitrator on the same host as a database node may"
	       "\n  cause complete cluster shutdown in case of host failure.");
  }
  return true;
}

static bool
check_mutually_exclusive(Vector<ConfigInfo::ConfigRuleSection>&sections, 
                         struct InitConfigFileParser::Context &ctx, 
                         const char * rule_data)
{
  /* This rule checks for configuration settings that are 
   * mutually exclusive and rejects them
   */

  //ctx.m_userProperties.print(stderr);

  Uint32 numNodes, n;
  ctx.m_userProperties.get("NoOfNodes", &numNodes);
  
  for (n=0; n < numNodes; n++)
  {
    const Properties* nodeProperties;
    if (!ctx.m_config->get("Node", n, &nodeProperties)) continue;

    //nodeProperties->print(stderr);

    const char* nodeType;
    if (unlikely(!nodeProperties->get("Type", &nodeType)))
    {
      ctx.reportError("Missing nodeType for node %u", n);
      return false;
    }
    
    if (strcmp(nodeType,DB_TOKEN) == 0)
    {
      {
        /* StopOnError related cross-checks */ 
        Uint32 stopOnError;
        Uint32 maxStartFailRetries;
        Uint32 startFailRetryDelay;
        
        if (unlikely(!nodeProperties->get("StopOnError", &stopOnError)))
        {
          ctx.reportError("Missing StopOnError setting for node %u", n);
          return false;
        }
        
        if (unlikely(!nodeProperties->get("MaxStartFailRetries", &maxStartFailRetries)))
        {
          ctx.reportError("Missing MaxStartFailRetries setting");
          return false;
        }
        
        if (unlikely(!nodeProperties->get("StartFailRetryDelay", &startFailRetryDelay)))
        {
          ctx.reportError("Missing StartFailRetryDelay setting");
          return false;
        }
    
        if (unlikely(((stopOnError != 0) &&
                      ((maxStartFailRetries != 3) ||
                       (startFailRetryDelay != 0)))))
        {
          ctx.reportError("Non default settings for MaxStartFailRetries "
                          "or StartFailRetryDelay with StopOnError != 0");
          return false;
        }
      }
    } /* DB_TOKEN */
  } /* for nodes */

  return true;
}


ConfigInfo::ParamInfoIter::ParamInfoIter(const ConfigInfo& info,
                                         Uint32 section,
                                         Uint32 section_type) :
  m_info(info),
  m_section_name(NULL),
  m_curr_param(0)
{
  /* Find the section's name */
  for (int j=0; j<info.m_NoOfParams; j++) {
    const ConfigInfo::ParamInfo & param = info.m_ParamInfo[j];
    if (param._type == ConfigInfo::CI_SECTION &&
        param._paramId == section &&
        (section_type == ~(Uint32)0 || 
         ConfigInfo::getSectionType(param) == section_type))
    {
      m_section_name= param._section;
      return;
    }
  }
  abort();
}


const ConfigInfo::ParamInfo*
ConfigInfo::ParamInfoIter::next(void) {
  assert(m_curr_param < m_info.m_NoOfParams);
  do {
    /*  Loop through the parameter and return a pointer to the next found */
    const ConfigInfo::ParamInfo* param = &m_info.m_ParamInfo[m_curr_param++];
    if (strcmp(param->_section, m_section_name) == 0 &&
        param->_type != ConfigInfo::CI_SECTION)
      return param;
  }
  while (m_curr_param<m_info.m_NoOfParams);

  return NULL;
}


static bool
is_name_in_list(const char* name, Vector<BaseString>& list)
{
  for (Uint32 i = 0; i<list.size(); i++)
  {
    if (strstr(name, list[i].c_str()))
      return true;
  }
  return false;
}


static
bool
saveSectionsInConfigValues(Vector<ConfigInfo::ConfigRuleSection>& notused,
                           struct InitConfigFileParser::Context &ctx,
                           const char * rule_data)
{
  if (rule_data == 0)
    return true;

  BaseString sections(rule_data);
  Vector<BaseString> list;
  sections.split(list, ",");

  Properties::Iterator it(ctx.m_config);

  {
    // Estimate size of Properties when saved as ConfigValues
    // and expand ConfigValues to that size in order to avoid
    // the need of allocating memory and copying from new to old
    Uint32 keys = 0;
    Uint64 data_sz = 0;
    for (const char * name = it.first(); name != 0; name = it.next())
    {
      PropertiesType pt;
      if (is_name_in_list(name, list) &&
          ctx.m_config->getTypeOf(name, &pt) &&
          pt == PropertiesType_Properties)
      {
        const Properties* tmp;
        require(ctx.m_config->get(name, &tmp) != 0);

        keys += 2; // openSection(key + no)
        keys += 1; // CFG_TYPE_OF_SECTION

        Properties::Iterator it2(tmp);
        for (const char * name2 = it2.first(); name2 != 0; name2 = it2.next())
        {
          keys++;
          require(tmp->getTypeOf(name2, &pt) != 0);
          switch(pt){
          case PropertiesType_char:
            const char* value;
            require(tmp->get(name2, &value) != 0);
            data_sz += 1 + ((strlen(value) + 3) / 4);
            break;

          case PropertiesType_Uint32:
            data_sz += 1;
            break;

          case PropertiesType_Uint64:
            data_sz += 2;
            break;

          case PropertiesType_Properties:
          default:
            require(false);
            break;
          }
        }
      }
    }

    assert(data_sz >> 32 == 0);
    ctx.m_configValues.expand(keys, Uint32(data_sz));
  }

  for (const char * name = it.first(); name != 0; name = it.next())
  {
    PropertiesType pt;
    if (is_name_in_list(name, list) &&
        ctx.m_config->getTypeOf(name, &pt) &&
        pt == PropertiesType_Properties)
    {
      const char * type;
      const Properties* tmp;
      require(ctx.m_config->get(name, &tmp) != 0);
      require(tmp->get("Type", &type) != 0);
      require((ctx.m_currentInfo = ctx.m_info->getInfo(type)) != 0);
      ctx.m_currentSection = const_cast<Properties*>(tmp);
      BaseString::snprintf(ctx.fname, sizeof(ctx.fname), "%s", type);
      saveInConfigValues(ctx, 0);
    }
  }

  return true;
}


template class Vector<ConfigInfo::ConfigRuleSection>;
