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

#include "ConfigInfo.hpp"
#define MAX_LINE_LENGTH 255

/****************************************************************************
 * Section names
 ****************************************************************************/
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
bool transformComputer(InitConfigFileParser::Context & ctx, const char *);
bool transformSystem(InitConfigFileParser::Context & ctx, const char *);
bool transformExternalSystem(InitConfigFileParser::Context & ctx, const char *);
bool transformNode(InitConfigFileParser::Context & ctx, const char *);
bool transformExtNode(InitConfigFileParser::Context & ctx, const char *);
bool transformConnection(InitConfigFileParser::Context & ctx, const char *);
bool applyDefaultValues(InitConfigFileParser::Context & ctx, const char *);
bool checkMandatory(InitConfigFileParser::Context & ctx, const char *);
bool fixPortNumber(InitConfigFileParser::Context & ctx, const char *);
bool fixShmkey(InitConfigFileParser::Context & ctx, const char *);
bool checkDbConstraints(InitConfigFileParser::Context & ctx, const char *);
bool checkConnectionConstraints(InitConfigFileParser::Context &, const char *);
bool fixHostname(InitConfigFileParser::Context & ctx, const char * data);
bool fixNodeId(InitConfigFileParser::Context & ctx, const char * data);
bool fixExtConnection(InitConfigFileParser::Context & ctx, const char * data);

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

  { "TCP",  fixPortNumber, 0 },
  //{ "SHM",  fixShmKey, 0 },
  
  { "COMPUTER", applyDefaultValues, 0 },
  
  { "DB",   applyDefaultValues, 0 },
  { "API",  applyDefaultValues, 0 },
  { "MGM",  applyDefaultValues, 0 },
  { "REP",  applyDefaultValues, 0 },
  { "EXTERNAL REP",  applyDefaultValues, 0 },
  
  { "TCP",  applyDefaultValues, 0 },
  { "SHM",  applyDefaultValues, 0 },
  { "SCI",  applyDefaultValues, 0 },
  { "OSE",  applyDefaultValues, 0 },

  { "DB",   checkDbConstraints, 0 },

  { "TCP",  fixNodeId, "NodeId1" },
  { "TCP",  fixNodeId, "NodeId2" },
  { "SHM",  fixNodeId, "NodeId1" },
  { "SHM",  fixNodeId, "NodeId2" },
  { "SCI",  fixNodeId, "NodeId1" },
  { "SCI",  fixNodeId, "NodeId2" },
  { "OSE",  fixNodeId, "NodeId1" },
  { "OSE",  fixNodeId, "NodeId2" },

  /**
   * fixExtConnection must be after fixNodeId
   */
  { "TCP",  fixExtConnection, 0 },
  { "SHM",  fixExtConnection, 0 },
  { "SCI",  fixExtConnection, 0 },
  { "OSE",  fixExtConnection, 0 },
  
  /**
   * checkConnectionConstraints must be after fixExtConnection
   */
  { "TCP",  checkConnectionConstraints, 0 },
  { "SHM",  checkConnectionConstraints, 0 },
  { "SCI",  checkConnectionConstraints, 0 },
  { "OSE",  checkConnectionConstraints, 0 },

  { "COMPUTER", checkMandatory, 0 },

  { "DB",   checkMandatory, 0 },
  { "API",  checkMandatory, 0 },
  { "MGM",  checkMandatory, 0 },
  { "REP",  checkMandatory, 0 },

  { "TCP",  checkMandatory, 0 },
  { "SHM",  checkMandatory, 0 },
  { "SCI",  checkMandatory, 0 },
  { "OSE",  checkMandatory, 0 },

  { "TCP",  fixHostname, "HostName1" },
  { "TCP",  fixHostname, "HostName2" },
  { "OSE",  fixHostname, "HostName1" },
  { "OSE",  fixHostname, "HostName2" },
};
const int ConfigInfo::m_NoOfRules = sizeof(m_SectionRules)/sizeof(SectionRule);

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
   ****************************************************************************/

  {"Id",
   "Id",
   "COMPUTER",
   "Name of computer",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0},

  {"HostName",
   "HostName",
   "COMPUTER",
   "Hostname of computer (e.g. mysql.com)",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"ByteOrder",
   "ByteOrder",
   "COMPUTER",
   "Not yet implemented",
   ConfigInfo::USED,  // Actually not used, but since it is MANDATORY,
                      // we don't want any warning message
   false,
   ConfigInfo::STRING,
   MANDATORY,  // Big == 0, Little == 1, NotSet == 2 (?)
   0,
   0x7FFFFFFF},

  /****************************************************************************
   * DB
   ****************************************************************************/

  {"Id",
   "Id",
   "DB",
   "Number identifying the database node (DB)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   1,
   (MAX_NODES - 1)},

  {"Type",
   "Type",
   "DB",
   "Type of node (Should have value DB)",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0},

  {"NoOfReplicas",
   "NoOfReplicas",
   "DB",
   "Number of copies of all data in the database (1-4)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   1,
   4},

  {"MaxNoOfAttributes",
   "MaxNoOfAttributes",
   "DB",
   "Total number of attributes stored in database. I.e. sum over all tables",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   1000,
   32,
   4096},
  
  {"MaxNoOfTables",
   "MaxNoOfTables",
   "DB",
   "Total number of tables stored in the database",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   32,
   8,
   128},
  
  {"MaxNoOfIndexes",
   "MaxNoOfIndexes",
   "DB",
   "Total number of indexes that can be defined in the system",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   128,
   0,
   2048},

  {"MaxNoOfConcurrentIndexOperations",
   "MaxNoOfConcurrentIndexOperations",
   "DB",
   "Total number of index operations that can execute simultaneously on one DB node",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   8192,
   0,
   1000000
  },

  {"MaxNoOfTriggers",
   "MaxNoOfTriggers",
   "DB",
   "Total number of triggers that can be defined in the system",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   768,
   0,
   2432},

  {"MaxNoOfFiredTriggers",
   "MaxNoOfFiredTriggers",
   "DB",
   "Total number of triggers that can fire simultaneously in one DB node",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   1000,
   0,
   1000000},

  {"ExecuteOnComputer",
   "ExecuteOnComputer",
   "DB",
   "String referencing an earlier defined COMPUTER",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0x7FFFFFFF},
  
  {"MaxNoOfSavedMessages",
   "MaxNoOfSavedMessages",
   "DB",
   "Max number of error messages in error log and max number of trace files",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   25,
   0,
   0x7FFFFFFF},

  {"LockPagesInMainMemory",
   "LockPagesInMainMemory",
   "DB",
   "If set to yes, then NDB Cluster data will not be swapped out to disk",
   ConfigInfo::USED,
   true,
   ConfigInfo::BOOL,
   false,
   0,
   0x7FFFFFFF},

  {"SleepWhenIdle",
   "SleepWhenIdle",
   "DB",
   "?",
   ConfigInfo::DEPRICATED,
   true,
   ConfigInfo::BOOL,
   true,
   0,
   0x7FFFFFFF},

  {"NoOfSignalsToExecuteBetweenCommunicationInterfacePoll",
   "NoOfSignalsToExecuteBetweenCommunicationInterfacePoll",
   "DB",
   "?",
   ConfigInfo::DEPRICATED,
   true,
   ConfigInfo::INT,
   20,
   1,
   0x7FFFFFFF},
  
  {"TimeBetweenWatchDogCheck",
   "TimeBetweenWatchDogCheck",
   "DB",
   "Time between execution checks inside a database node",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   4000,
   70,
   0x7FFFFFFF},

  {"StopOnError",
   "StopOnError",
   "DB",
   "If set to N, the DB automatically restarts/recovers in case of node failure",
   ConfigInfo::USED,
   true,
   ConfigInfo::BOOL,
   true,
   0,
   0x7FFFFFFF},

  { "RestartOnErrorInsert",
    "RestartOnErrorInsert",
    "DB",
    "See src/kernel/vm/Emulator.hpp NdbRestartType for details",
    ConfigInfo::INTERNAL,
    true,
    ConfigInfo::INT,
    2,
    0,
    4 },
  
  {"MaxNoOfConcurrentOperations",
   "MaxNoOfConcurrentOperations",
   "DB",
   "Max no of op:s on DB (op:s within a transaction are concurrently executed)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   8192,
   32,
   1000000},

  {"MaxNoOfConcurrentTransactions",
   "MaxNoOfConcurrentTransactions", 
   "DB",
   "Max number of transaction executing concurrently on the DB node",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   4096,
   32,
   1000000},

  {"MaxNoOfConcurrentScans",
   "MaxNoOfConcurrentScans", 
   "DB",
   "Max number of scans executing concurrently on the DB node",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   25,
   2,
   500},

   {"TransactionBufferMemory",
    "TransactionBufferMemory", 
    "DB",
    "Dynamic buffer space (in bytes) for key and attribute data allocated for each DB node",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    1024000,
    1024,
    0x7FFFFFFF},
 
  {"NoOfIndexPages",
   "NoOfIndexPages",   
   "DB",
   "Number of 8k byte pages on each DB node for storing indexes",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   3000,
   128,
   192000},

  {"MemorySpaceIndexes",
   "NoOfIndexPages",   
   "DB",
   "Depricated",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   128,
   192000},

  {"NoOfDataPages",
   "NoOfDataPages",
   "DB",
   "Number of 8k byte pages on each DB node for storing data",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   10000,
   128,
   400000},

  {"MemorySpaceTuples",
   "NoOfDataPages",
   "DB",
   "Depricated",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   128,
   400000},

  {"NoOfDiskBufferPages",
   "NoOfDiskBufferPages",
   "DB",
   "?",
   ConfigInfo::NOTIMPLEMENTED,
   false,
   ConfigInfo::INT,
   0,
   0,
   0},

  {"MemoryDiskPages",
   "NoOfDiskBufferPages",
   "DB",
   "?",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0},

  {"NoOfFreeDiskClusters",
   "NoOfFreeDiskClusters",
   "DB",
   "?",
   ConfigInfo::NOTIMPLEMENTED,
   false,
   ConfigInfo::INT,
   0,
   0,
   0},

  {"NoOfDiskClusters",
   "NoOfDiskClusters",
   "DB",
   "?",
   ConfigInfo::NOTIMPLEMENTED,
   false,
   ConfigInfo::INT,
   0,
   0,
   0x7FFFFFFF},
  
  {"TimeToWaitAlive",
   "TimeToWaitAlive",
   "DB",
   "Time to wait for other nodes to become alive during initial system start",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   25,
   2,
   4000},

  {"HeartbeatIntervalDbDb",
   "HeartbeatIntervalDbDb",
   "DB",
   "Time between DB-to-DB heartbeats. DB considered dead after 3 missed HBs",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   1500,
   10,
   0x7FFFFFFF},

  {"HeartbeatIntervalDbApi",
   "HeartbeatIntervalDbApi",
   "DB",
   "Time between API-to-DB heartbeats. API connection closed after 3 missed HBs",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   1500,
   100,
   0x7FFFFFFF},

  {"TimeBetweenLocalCheckpoints",
   "TimeBetweenLocalCheckpoints",
   "DB",
   "Time between taking snapshots of the database (expressed in 2log of bytes)",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   20,
   0,
   31},

  {"TimeBetweenGlobalCheckpoints",
   "TimeBetweenGlobalCheckpoints",
   "DB",
   "Time between doing group commit of transactions to disk",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   2000,
   10,
   32000},

  {"NoOfFragmentLogFiles",
   "NoOfFragmentLogFiles",
   "DB",
   "No of 16 Mbyte Redo log files in each of 4 file sets belonging to DB node",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   8,
   1,
   0x7FFFFFFF},

  {"MaxNoOfOpenFiles",
   "MaxNoOfOpenFiles",
   "DB",
   "Max number of files open per DB node.(One thread is created per file)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   40,
   20,
   256},

  {"NoOfConcurrentCheckpointsDuringRestart",
   "NoOfConcurrentCheckpointsDuringRestart",
   "DB",
   "?",
   ConfigInfo::USED,  
   true,
   ConfigInfo::INT,
   1,
   1,
   4},
  
  {"TimeBetweenInactiveTransactionAbortCheck",
   "TimeBetweenInactiveTransactionAbortCheck",
   "DB",
   "Time between inactive transaction checks",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   1000,
   1000,
   0x7FFFFFFF},
  
  {"TransactionInactiveTimeout",
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
   3000,
   0,
   0x7FFFFFFF},

  {"TransactionDeadlockDetectionTimeout",
   "TransactionDeadlockDetectionTimeout",
   "DB",
   "Time transaction can be executing in a DB node (ms).\n"
   "This is the time the transaction coordinator waits for each database node\n"
   "of the transaction to execute a request. If the database node takes too\n"
   "long time, the transaction gets aborted.",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   3000,
   50,
   0x7FFFFFFF},

  {"TransactionInactiveTimeBeforeAbort",
   "TransactionInactiveTimeBeforeAbort",
   "DB",
   "Time a transaction can be inactive before getting aborted (ms)",
   ConfigInfo::DEPRICATED,
   true,
   ConfigInfo::INT,
   3000,
   20,
   0x7FFFFFFF},

  {"NoOfConcurrentProcessesHandleTakeover",
   "NoOfConcurrentProcessesHandleTakeover",
   "DB",
   "?",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   1,
   1,
   15},
  
  {"NoOfConcurrentCheckpointsAfterRestart",
   "NoOfConcurrentCheckpointsAfterRestart",
   "DB",
   "?",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   1,
   1,
   4},
  
  {"NoOfDiskPagesToDiskDuringRestartTUP",
   "NoOfDiskPagesToDiskDuringRestartTUP",
   "DB",
   "?",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   50,
   1,
   0x7FFFFFFF},

  {"NoOfDiskPagesToDiskAfterRestartTUP",
   "NoOfDiskPagesToDiskAfterRestartTUP",
   "DB",
   "?",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   10,
   1,
   0x7FFFFFFF},

  {"NoOfDiskPagesToDiskDuringRestartACC",
   "NoOfDiskPagesToDiskDuringRestartACC",
   "DB",
   "?",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   25,
   1,
   0x7FFFFFFF},

  {"NoOfDiskPagesToDiskAfterRestartACC",
   "NoOfDiskPagesToDiskAfterRestartACC",
   "DB",
   "?",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   5,
   1,
   0x7FFFFFFF},
  
  {"NoOfDiskClustersPerDiskFile",
   "NoOfDiskClustersPerDiskFile",
   "DB",
   "?",
   ConfigInfo::NOTIMPLEMENTED,
   false,
   ConfigInfo::INT,
   0,
   0,
   0x7FFFFFFF},
  
  {"NoOfDiskFiles",
   "NoOfDiskFiles",
   "DB",
   "?",
   ConfigInfo::NOTIMPLEMENTED,
   false,
   ConfigInfo::INT,
   0,
   0,
   0x7FFFFFFF},

  {"ArbitrationTimeout",
   "ArbitrationTimeout",
   "DB",
   "Max time (milliseconds) database partion waits for arbitration signal",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   1000,
   10,
   0x7FFFFFFF},

  {"FileSystemPath",
   "FileSystemPath",
   "DB",
   "Path to directory where the DB node stores its data (directory must exist)",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"LogLevelStartup",
   "LogLevelStartup",
   "DB",
   "Node startup info printed on stdout",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   1,
   0,
   15},
  
  {"LogLevelShutdown",
   "LogLevelShutdown",
   "DB",
   "Node shutdown info printed on stdout",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   15},

  {"LogLevelStatistic",
   "LogLevelStatistic",
   "DB",
   "Transaction, operation, transporter info printed on stdout",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   15},

  {"LogLevelCheckpoint",
   "LogLevelCheckpoint",
   "DB",
   "Local and Global checkpoint info printed on stdout",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   15},

  {"LogLevelNodeRestart",
   "LogLevelNodeRestart",
   "DB",
   "Node restart, node failure info printed on stdout",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   15},

  {"LogLevelConnection",
   "LogLevelConnection",
   "DB",
   "Node connect/disconnect info printed on stdout",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   15},

  {"LogLevelError",
   "LogLevelError",
   "DB",
   "Transporter, heartbeat errors printed on stdout",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   15},

  {"LogLevelInfo",
   "LogLevelInfo",
   "DB",
   "Heartbeat and log info printed on stdout",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   15},

  /**
   * Backup
   */
  { "ParallelBackups",
    "ParallelBackups",
    "DB",
    "Maximum number of parallel backups",
    ConfigInfo::NOTIMPLEMENTED,
    false,
    ConfigInfo::INT,
    1,
    1,
    1 },
  
  { "BackupMemory",
    "BackupMemory",
    "DB",
    "Total memory allocated for backups per node (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    (2 * 1024 * 1024) + (2 * 1024 * 1024), // sum of BackupDataBufferSize and BackupLogBufferSize
    0,
    0x7FFFFFFF },
  
  { "BackupDataBufferSize",
    "BackupDataBufferSize",
    "DB",
    "Default size of databuffer for a backup (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    (2 * 1024 * 1024), // remember to change BackupMemory
    0,
    0x7FFFFFFF },

  { "BackupLogBufferSize",
    "BackupLogBufferSize",
    "DB",
    "Default size of logbuffer for a backup (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    (2 * 1024 * 1024), // remember to change BackupMemory
    0,
    0x7FFFFFFF },

  { "BackupWriteSize",
    "BackupWriteSize",
    "DB",
    "Default size of filesystem writes made by backup (in bytes)",
    ConfigInfo::USED,
    false,
    ConfigInfo::INT,
    32768,
    0,
    0x7FFFFFFF },

  /****************************************************************************
   * REP
   ****************************************************************************/

  {"Id",
   "Id",
   "REP",
   "Number identifying replication node (REP)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   1,
   (MAX_NODES - 1)},

  {"Type",
   "Type",
   "REP",
   "Type of node (Should have value REP)",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0},

  {"ExecuteOnComputer",
   "ExecuteOnComputer",
   "REP",
   "String referencing an earlier defined COMPUTER",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0x7FFFFFFF},

  /****************************************************************************
   * EXTERNAL REP
   ****************************************************************************/

  {"Id",
   "Id",
   "EXTERNAL REP",
   "Number identifying external (i.e. in another NDB Cluster) replication node (REP)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   1,
   (MAX_NODES - 1)},

  {"Type",
   "Type",
   "EXTERNAL REP",
   "Type of node (Should have value REP)",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0},

  {"System",
   "System",
   "EXTERNAL REP",
   "System name of system hosting node",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0},

  {"HeartbeatIntervalRepRep",
   "HeartbeatIntervalRepRep",
   "EXTERNAL REP",
   "Time between REP-REP heartbeats. Connection closed after 3 missed HBs",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   3000,
   100,
   0x7FFFFFFF},

  /****************************************************************************
   * API
   ****************************************************************************/

  {"Id",
   "Id",
   "API",
   "Number identifying application node (API)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   1,
   (MAX_NODES - 1)},

  {"Type",
   "Type",
   "API",
   "Type of node (Should have value API)",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0},

  {"ExecuteOnComputer",
   "ExecuteOnComputer",
   "API",
   "String referencing an earlier defined COMPUTER",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"MaxNoOfSavedMessages",
   "MaxNoOfSavedMessages",
   "API",
   "Max number of error messages in error log and max number of trace files",
   ConfigInfo::USED,
   true,
   ConfigInfo::INT,
   25,
   0,
   0x7FFFFFFF},

  {"SleepWhenIdle",
   "SleepWhenIdle",
   "API",
   "?",
   ConfigInfo::DEPRICATED,
   true,
   ConfigInfo::BOOL,
   true,
   0,
   0x7FFFFFFF},

  {"ArbitrationRank",
   "ArbitrationRank",
   "API",
   "If 0, then API is not arbitrator. Kernel selects arbitrators in order 1, 2",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   2,
   0,
   2},

  {"ArbitrationDelay",
   "ArbitrationDelay",
   "API",
   "When asked to arbitrate, arbitrator waits this long before voting (msec)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   0x7FFFFFFF},

  /****************************************************************************
   * MGM
   ****************************************************************************/

  {"Id",
   "Id",
   "MGM",
   "Number identifying the management server node (MGM)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   1,
   (MAX_NODES - 1)},
  
  {"Type",
   "Type",
   "MGM",
   "Type of node (Should have value MGM)",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0},

  {"ExecuteOnComputer",
   "ExecuteOnComputer",
   "MGM",
   "String referencing an earlier defined COMPUTER",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0x7FFFFFFF},

  // SHOULD THIS REALLY BE DEFINABLE FOR MGM ???
  {"MaxNoOfSavedMessages",
   "MaxNoOfSavedMessages",
   "MGM",
   "Max number of error messages in error log and max number of trace files",
   ConfigInfo::DEPRICATED,
   true,
   ConfigInfo::INT,
   25,
   0,
   0x7FFFFFFF},
  
  {"MaxNoOfSavedEvents",
   "MaxNoOfSavedEvents",
   "MGM",
   "",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   100,
   0,
   0x7FFFFFFF},

  {"PortNumber",
   "PortNumber",
   "MGM",
   "Port number to give commands to/fetch configurations from management server",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   2200,
   0,
   0x7FFFFFFF},

  {"PortNumberStats",
   "PortNumberStats",
   "MGM",
   "Port number used to get statistical information from a management server",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   2199,
   0,
   0x7FFFFFFF},

  {"ArbitrationRank",
   "ArbitrationRank",
   "MGM",
   "If 0, then MGM is not arbitrator. Kernel selects arbitrators in order 1, 2",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   2,
   0,
   2},

  {"ArbitrationDelay",
   "ArbitrationDelay",
   "MGM",
   "",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   0x7FFFFFFF},

  /*****************************************************************************
   * SYSTEM
   ****************************************************************************/

  {"Name",
   "Name",
   "SYSTEM",
   "Name of system (NDB Cluster)",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0},

  {"ReplicationRole",
   "ReplicationRole",
   "SYSTEM",
   "Role in Global Replication (None, Primary, or Standby)",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0},

  {"LogDestination",
   "LogDestination",
   "MGM",
   "String describing where logmessages are sent",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0x7FFFFFFF},

  {"PrimaryMGMNode",
   "PrimaryMGMNode",
   "SYSTEM",
   "Node id of Primary MGM node",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   0x7FFFFFFF},

  {"ConfigGenerationNumber",
   "ConfigGenerationNumber",
   "SYSTEM",
   "Configuration generation number",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   0,
   0,
   0x7FFFFFFF},

  {"Name",
   "Name",
   "EXTERNAL SYSTEM",
   "Name of external system (another NDB Cluster)",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0},

  /*****************************************************************************
   * TCP
   ****************************************************************************/

  {"Type",
   "Type",
   "TCP",
   "",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0x7FFFFFFF},

  {"HostName1",
   "HostName1",
   "TCP",
   "Name of computer on one side of the connection",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"HostName2",
   "HostName2",
   "TCP",
   "Name of computer on one side of the connection",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"NodeId1",
   "NodeId1",
   "TCP",
   "Id of node (DB, API or MGM) on one side of the connection",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"ProcessId1",
   "NodeId1",
   "TCP",
   "Depricated",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"NodeId2",
   "NodeId2",
   "TCP",
   "Id of node (DB, API or MGM) on one side of the connection",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"ProcessId2",
   "NodeId2",
   "TCP",
   "Depricated",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"IpAddress1",
   "HostName1",
   "TCP",
   "IP address of first node in connection.",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"IpAddress2",
   "HostName2",
   "TCP",
   "IP address of second node in connection.",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0},

  {"SendSignalId",
   "SendSignalId",
   "TCP",
   "Sends id in each signal.  Used in trace files.",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   true,
   0,
   0x7FFFFFFF},

  {"Compression",
   "Compression",
   "TCP",
   "If compression is enabled, then all signals between nodes are compressed",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   false,
   0,
   0x7FFFFFFF},

  {"Checksum",
   "Checksum",
   "TCP",
   "If checksum is enabled, all signals between nodes are checked for errors",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   false,
   0,
   0x7FFFFFFF},

  {"PortNumber",
   "PortNumber",
   "TCP",
   "Port used for this transporter",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"SendBufferSize",
   "SendBufferSize",
   "TCP",
   "Size of buffer for signals sent from this node (in no of signals)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   16,
   1,
   0x7FFFFFFF},
  
  {"MaxReceiveSize",
   "MaxReceiveSize",
   "TCP",
   "Size of buffer for signals received by this node (in no of signals)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   4,
   1,
   0x7FFFFFFF},

  {"Proxy",
   "Proxy",
   "TCP",
   "",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0},

  /*****************************************************************************
   * SHM
   ****************************************************************************/

  {"Type",
   "Type",
   "SHM",
   "",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0x7FFFFFFF},
  
  {"NodeId1",
   "NodeId1",
   "SHM",
   "Id of node (DB, API or MGM) on one side of the connection",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0x7FFFFFFF},
  
  {"ProcessId1",
   "NodeId1",
   "SHM",
   "Depricated",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0x7FFFFFFF},
  
  {"NodeId2",
   "NodeId2",
   "SHM",
   "Id of node (DB, API or MGM) on one side of the connection",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   MANDATORY,
   0,
   0x7FFFFFFF},
  
  {"ProcessId2",
   "NodeId2",
   "SHM",
   "Depricated",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0x7FFFFFFF},
  
  {"SendSignalId",
   "SendSignalId",
   "SHM",
   "Sends id in each signal.  Used in trace files.",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   false,
   0,
   0x7FFFFFFF},
  
  {"Compression",
   "Compression",
   "SHM",
   "If compression is enabled, then all signals between nodes are compressed",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   false,
   0,
   0x7FFFFFFF},
  
  {"Checksum",
   "Checksum",
   "SHM",
   "If checksum is enabled, all signals between nodes are checked for errors",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   true,
   0,
   0x7FFFFFFF},
  
  {"ShmKey",
   "ShmKey",
   "SHM",
   "A shared memory key",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   0,
   0x7FFFFFFF },
  
  {"ShmSize",
   "ShmSize",
   "SHM",
   "Size of shared memory segment",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   1048576,
   4096,
   0x7FFFFFFF},
  
  /*****************************************************************************
   * SCI
   ****************************************************************************/

  {"NodeId1",
   "NodeId1",
   "SCI",
   "Id of node (DB, API or MGM) on one side of the connection",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"ProcessId1",
   "NodeId1",
   "SCI",
   "Depricated",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"NodeId2",
   "NodeId2",
   "SCI",
   "Id of node (DB, API or MGM) on one side of the connection",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"ProcessId2",
   "NodeId2",
   "SCI",
   "Depricated",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"SciId0",
   "SciId0",
   "SCI",
   "Local SCI-node id for adapter 0 (a computer can have two adapters)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"SciId1",
   "SciId1",
   "SCI",
   "Local SCI-node id for adapter 1 (a computer can have two adapters)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"SendSignalId",
   "SendSignalId",
   "SCI",
   "Sends id in each signal.  Used in trace files.",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   true,
   0,
   0x7FFFFFFF},

  {"Compression",
   "Compression",
   "SCI",
   "If compression is enabled, then all signals between nodes are compressed",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   false,
   0,
   0x7FFFFFFF},

  {"Checksum",
   "Checksum",
   "SCI",
   "If checksum is enabled, all signals between nodes are checked for errors",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   false,
   0,
   0x7FFFFFFF},

  {"SendLimit",
   "SendLimit",
   "SCI",
   "Transporter send buffer contents are sent when this no of bytes is buffered",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   2048,
   512,
   0x7FFFFFFF},

  {"SharedBufferSize",
   "SharedBufferSize",
   "SCI",
   "Size of shared memory segment",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   1048576,
   262144,
   0x7FFFFFFF},

  {"Node1_NoOfAdapters",
   "Node1_NoOfAdapters",
   "SCI",
   "",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"Node2_NoOfAdapters",
   "Node2_NoOfAdapters",
   "SCI",
   "",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"Node1_Adapter",
   "Node1_Adapter",
   "SCI",
   "",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"Node2_Adapter",
   "Node2_Adapter",
   "SCI",
   "",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  /*****************************************************************************
   * OSE
   ****************************************************************************/

  {"Type",
   "Type",
   "OSE",
   "",
   ConfigInfo::INTERNAL,
   false,
   ConfigInfo::STRING,
   0,
   0,
   0x7FFFFFFF},

  {"HostName1",
   "HostName1",
   "OSE",
   "Name of computer on one side of the connection",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"HostName2",
   "HostName2",
   "OSE",
   "Name of computer on one side of the connection",
   ConfigInfo::USED,
   false,
   ConfigInfo::STRING,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"NodeId1",
   "NodeId1",
   "OSE",
   "Id of node (DB, API or MGM) on one side of the connection",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"ProcessId1",
   "NodeId1",
   "OSE",
   "Depricated",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"NodeId2",
   "NodeId2",
   "OSE",
   "Id of node (DB, API or MGM) on one side of the connection",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   0,
   0x7FFFFFFF},

  {"ProcessId2",
   "NodeId2",
   "OSE",
   "Depricated",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   MANDATORY,
   0,
   0x7FFFFFFF},

  {"SendSignalId",
   "SendSignalId",
   "OSE",
   "Sends id in each signal.  Used in trace files.",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   true,
   0,
   0x7FFFFFFF},

  {"Compression",
   "Compression",
   "OSE",
   "If compression is enabled, then all signals between nodes are compressed",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   false,
   0,
   0x7FFFFFFF},

  {"Checksum",
   "Checksum",
   "OSE",
   "If checksum is enabled, all signals between nodes are checked for errors",
   ConfigInfo::USED,
   false,
   ConfigInfo::BOOL,
   false,
   0,
   0x7FFFFFFF},

  // Should not be part of OSE ?
  {"SharedBufferSize",
   "SharedBufferSize",
   "OSE",
   "?",
   ConfigInfo::DEPRICATED,
   false,
   ConfigInfo::INT,
   UNDEFINED,
   2000,
   0x7FFFFFFF},

  {"PrioASignalSize",
   "PrioASignalSize",
   "OSE",
   "Size of priority A signals (in bytes)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   1000,
   0,
   0x7FFFFFFF},

  {"PrioBSignalSize",
   "PrioBSignalSize",
   "OSE",
   "Size of priority B signals (in bytes)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   1000,
   0,
   0x7FFFFFFF},

  {"ReceiveArraySize",
   "ReceiveArraySize",
   "OSE",
   "Number of OSE signals checked for correct ordering (in no of OSE signals)",
   ConfigInfo::USED,
   false,
   ConfigInfo::INT,
   10,
   0,
   0x7FFFFFFF}
};

const int ConfigInfo::m_NoOfParams = sizeof(m_ParamInfo) / sizeof(ParamInfo);


/****************************************************************************
 * Ctor
 ****************************************************************************/
inline void require(bool v) { if(!v) abort();}

ConfigInfo::ConfigInfo() {
  Properties *section;
  const Properties *oldpinfo;

  m_info.setCaseInsensitiveNames(true);
  m_systemDefaults.setCaseInsensitiveNames(true);

  for (int i=0; i<m_NoOfParams; i++) {
    const ParamInfo & param = m_ParamInfo[i];
    
    // Create new section if it did not exist
    if (!m_info.getCopy(param._section, &section)) {
      Properties newsection;
      newsection.setCaseInsensitiveNames(true);
      m_info.put(param._section, &newsection);
    }

    // Get copy of section
    m_info.getCopy(param._section, &section);
    
    // Create pinfo (parameter info) entry 
    Properties pinfo; 
    pinfo.put("Fname",       param._fname);
    pinfo.put("Pname",       param._pname);
    pinfo.put("Description", param._description);
    pinfo.put("Updateable",  param._updateable);
    pinfo.put("Type",        param._type);
    pinfo.put("Status",      param._status);
    pinfo.put("Default",     param._default);
    pinfo.put("Min",         param._min);
    pinfo.put("Max",         param._max);

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

    {
      Properties * p;
      if(!m_systemDefaults.getCopy(param._section, &p)){
	p = new Properties();
	p->setCaseInsensitiveNames(true);
      }
      if(param._type != STRING && 
	 param._default != UNDEFINED &&
	 param._default != MANDATORY){
	require(p->put(param._pname, param._default));
      }
      require(m_systemDefaults.put(param._section, p, true));
      delete p;
    }
  }

  for (int i=0; i<m_NoOfParams; i++) {
    if(m_ParamInfo[i]._section == NULL){
      ndbout << "Check that each pname has an fname failed." << endl;
      ndbout << "Parameter \"" << m_ParamInfo[i]._pname 
	     << "\" does not exist in section \"" 
	     << m_ParamInfo[i]._section << "\"." << endl;
      ndbout << "Edit file " << __FILE__ << "." << endl;
      exit(-1);
    }
    const Properties * p = getInfo(m_ParamInfo[i]._section);
    if (!p || !p->contains(m_ParamInfo[i]._pname)) {
      ndbout << "Check that each pname has an fname failed." << endl;
      ndbout << "Parameter \"" << m_ParamInfo[i]._pname 
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
Uint32
getInfoInt(const Properties * section, 
	   const char* fname, const char * type){
  Uint32 val;
  const Properties * p;
  if (section->get(fname, &p) && p->get(type, &val)) {
    return val;
  }
  warning(type, fname);
  return val;
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

Uint32
ConfigInfo::getMax(const Properties * section, const char* fname) const {
  return getInfoInt(section, fname, "Max");
}

Uint32
ConfigInfo::getMin(const Properties * section, const char* fname) const {
  return getInfoInt(section, fname, "Min");
}

Uint32
ConfigInfo::getDefault(const Properties * section, const char* fname) const {
  return getInfoInt(section, fname, "Default");
}

const char*
ConfigInfo::getPName(const Properties * section, const char* fname) const {
  return getInfoString(section, fname, "Pname");
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

bool
ConfigInfo::verify(const Properties * section, const char* fname, 
		   Uint32 value) const {
  Uint32 min, max; min = max + 1;

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
    if (strcmp(n, getPName(sec, n))) continue;
    if (getStatus(sec, n) == ConfigInfo::INTERNAL) continue;
    if (getStatus(sec, n) == ConfigInfo::DEPRICATED) continue;
    if (getStatus(sec, n) == ConfigInfo::NOTIMPLEMENTED) continue;
    print(sec, n);
  }
}

void ConfigInfo::print(const Properties * section, 
		       const char* parameter) const {
  ndbout << getPName(section, parameter);
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
    ctx.reportError("Mandatory parameter Id missing from section "
		    "[%s] starting at line: %d",
		    ctx.fname, ctx.m_sectionLineno);
    return false;
  }
  snprintf(ctx.pname, sizeof(ctx.pname), "Node_%d", id);
  
  ctx.m_currentSection->put("Type", ctx.fname);

  Uint32 nodes = 0;
  ctx.m_userProperties.get("NoOfNodes", &nodes);
  ctx.m_userProperties.put("NoOfNodes", ++nodes, true);

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
      if(!ctx.m_currentSection->contains(name)){
	switch (ctx.m_info->getType(ctx.m_currentInfo, name)){
	case ConfigInfo::INT:
	case ConfigInfo::BOOL:{
	  Uint32 val = 0;
	  ::require(defaults->get(name, &val));
	  ctx.m_currentSection->put(name, val);
	  break;
	}
	case ConfigInfo::STRING:{
	  const char * val;
	  ::require(defaults->get(name, &val));
	  ctx.m_currentSection->put(name, val);
	  break;
	}
	}
      }
    }
  }
}

bool
applyDefaultValues(InitConfigFileParser::Context & ctx, const char * data){
  
  applyDefaultValues(ctx, ctx.m_userDefaults);
  applyDefaultValues(ctx, ctx.m_systemDefaults);
  
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
      const char * pname;
      const char * fname;
      ::require(info->get("Pname", &pname));
      ::require(info->get("Fname", &fname));
      if(!ctx.m_currentSection->contains(pname)){
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
bool fixNodeId(InitConfigFileParser::Context & ctx, const char * data){

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
bool 
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
bool
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
bool
fixHostname(InitConfigFileParser::Context & ctx, const char * data){
  
  char buf[] = "NodeIdX"; buf[6] = data[sizeof("HostNam")];
  char sysbuf[] = "SystemX"; sysbuf[6] = data[sizeof("HostNam")];
  
  if(!ctx.m_currentSection->contains(data)){
    Uint32 id = 0;
    require(ctx.m_currentSection->get(buf, &id));

    const Properties * node;
    require(ctx.m_config->get("Node", id, &node));
    
    const char * compId;
    require(node->get("ExecuteOnComputer", &compId));
    
    const Properties * computer;
    char tmp[255];
    snprintf(tmp, sizeof(tmp), "Computer_%s", compId);
    require(ctx.m_config->get(tmp, &computer));

    const char * hostname;
    require(computer->get("HostName", &hostname));
    require(ctx.m_currentSection->put(data, hostname));
  }
  return true;
}

/**
 * Connection rule: Fix port number (using a port number adder)
 */
bool
fixPortNumber(InitConfigFileParser::Context & ctx, const char * data){

  if(!ctx.m_currentSection->contains("PortNumber")){
    Uint32 adder = 0;
    ctx.m_userProperties.get("PortNumberAdder", &adder);
    Uint32 base = 0;
    if(!ctx.m_userDefaults->get("PortNumber", &base) &&
       !ctx.m_systemDefaults->get("PortNumber", &base)){
      return true;
    }
    ctx.m_currentSection->put("PortNumber", base + adder);
    adder++;
    ctx.m_userProperties.put("PortNumberAdder", adder, true);
  }
  return true;
}

/**
 * DB Node rule: Check various constraints
 */
bool
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
bool
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
