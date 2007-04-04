/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <../../include/kernel/ndb_limits.h>
#include "ParamInfo.hpp" 
#include <mgmapi_config_parameters.h>

#ifndef MYSQLCLUSTERDIR
#define MYSQLCLUSTERDIR "."
#endif

#define KEY_INTERNAL 0
#define MAX_INT_RNIL 0xfffffeff
#define MAX_PORT_NO 65535

#define _STR_VALUE(x) #x
#define STR_VALUE(x) _STR_VALUE(x)

/****************************************************************************
 * Section names
 ****************************************************************************/
#define DB_TOKEN_PRINT  "ndbd(DB)"
#define MGM_TOKEN_PRINT "ndb_mgmd(MGM)"
#define API_TOKEN_PRINT "mysqld(API)"

/**
 * A MANDATORY parameters must be specified in the config file
 * An UNDEFINED parameter may or may not be specified in the config file
 */
static const char* MANDATORY = (char*)~(UintPtr)0;// Default value for mandatory params.
static const char* UNDEFINED = 0;                 // Default value for undefined params.

extern const ParamInfo ParamInfoArray[];
extern const int ParamInfoNum;

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
const ParamInfo ParamInfoArray[] = {

  /****************************************************************************
   * COMPUTER
   ***************************************************************************/
  {
    KEY_INTERNAL,
    "COMPUTER",
    "COMPUTER",
    "Computer section",
    CI_INTERNAL,
    false,
    CI_SECTION,
    0,
    0, 0 },
  
  {
    KEY_INTERNAL,
    "Id",
    "COMPUTER",
    "Name of computer",
    CI_USED,
    false,
    CI_STRING,
    MANDATORY,
    0, 0 },

  {
    KEY_INTERNAL,
    "HostName",
    "COMPUTER",
    "Hostname of computer (e.g. mysql.com)",
    CI_USED,
    false,
    CI_STRING,
    MANDATORY,
    0, 0 },

  {
    KEY_INTERNAL,
    "ByteOrder",
    "COMPUTER",
    0,
    CI_DEPRICATED,
    false,
    CI_STRING,
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
    CI_USED,
    false,
    CI_SECTION,
    (const char *)CFG_SECTION_SYSTEM,
    0, 0 },

  {
    CFG_SYS_NAME,
    "Name",
    "SYSTEM",
    "Name of system (NDB Cluster)",
    CI_USED,
    false,
    CI_STRING,
    MANDATORY,
    0, 0 },
  
  {
    CFG_SYS_PRIMARY_MGM_NODE,
    "PrimaryMGMNode",
    "SYSTEM",
    "Node id of Primary "MGM_TOKEN_PRINT" node",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SYS_CONFIG_GENERATION,
    "ConfigGenerationNumber",
    "SYSTEM",
    "Configuration generation number",
    CI_USED,
    false,
    CI_INT,
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
    CI_USED,
    false,
    CI_SECTION,
    (const char *)NODE_TYPE_DB, 
    0, 0
  },

  {
    CFG_NODE_HOST,
    "HostName",
    DB_TOKEN,
    "Name of computer for this node",
    CI_INTERNAL,
    false,
    CI_STRING,
    "localhost",
    0, 0 },

  {
    CFG_NODE_SYSTEM,
    "System",
    DB_TOKEN,
    "Name of system for this node",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    KEY_INTERNAL,
    "Id",
    DB_TOKEN,
    "",
    CI_DEPRICATED,
    false,
    CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },

  {
    CFG_NODE_ID,
    "NodeId",
    DB_TOKEN,
    "Number identifying the database node ("DB_TOKEN_PRINT")",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },

  {
    KEY_INTERNAL,
    "ServerPort",
    DB_TOKEN,
    "Port used to setup transporter",
    CI_USED,
    false,
    CI_INT,
    UNDEFINED,
    "1",
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_DB_NO_REPLICAS,
    "NoOfReplicas",
    DB_TOKEN,
    "Number of copies of all data in the database (1-4)",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "1",
    "4" },

  {
    CFG_DB_NO_ATTRIBUTES,
    "MaxNoOfAttributes",
    DB_TOKEN,
    "Total number of attributes stored in database. I.e. sum over all tables",
    CI_USED,
    false,
    CI_INT,
    "1000",
    "32",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_NO_TABLES,
    "MaxNoOfTables",
    DB_TOKEN,
    "Total number of tables stored in the database",
    CI_USED,
    false,
    CI_INT,
    "128",
    "8",
    STR_VALUE(MAX_TABLES) },
  
  {
    CFG_DB_NO_ORDERED_INDEXES,
    "MaxNoOfOrderedIndexes",
    DB_TOKEN,
    "Total number of ordered indexes that can be defined in the system",
    CI_USED,
    false,
    CI_INT,
    "128",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_UNIQUE_HASH_INDEXES,
    "MaxNoOfUniqueHashIndexes",
    DB_TOKEN,
    "Total number of unique hash indexes that can be defined in the system",
    CI_USED,
    false,
    CI_INT,
    "64",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_INDEXES,
    "MaxNoOfIndexes",
    DB_TOKEN,
    "Total number of indexes that can be defined in the system",
    CI_DEPRICATED,
    false,
    CI_INT,
    "128",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_INDEX_OPS,
    "MaxNoOfConcurrentIndexOperations",
    DB_TOKEN,
    "Total number of index operations that can execute simultaneously on one "DB_TOKEN_PRINT" node",
    CI_USED,
    false,
    CI_INT,
    "8K",
    "0",
    STR_VALUE(MAX_INT_RNIL) 
   },

  {
    CFG_DB_NO_TRIGGERS,
    "MaxNoOfTriggers",
    DB_TOKEN,
    "Total number of triggers that can be defined in the system",
    CI_USED,
    false,
    CI_INT,
    "768",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_TRIGGER_OPS,
    "MaxNoOfFiredTriggers",
    DB_TOKEN,
    "Total number of triggers that can fire simultaneously in one "DB_TOKEN_PRINT" node",
    CI_USED,
    false,
    CI_INT,
    "4000",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    DB_TOKEN,
    "String referencing an earlier defined COMPUTER",
    CI_USED,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },
  
  {
    CFG_DB_NO_SAVE_MSGS,
    "MaxNoOfSavedMessages",
    DB_TOKEN,
    "Max number of error messages in error log and max number of trace files",
    CI_USED,
    true,
    CI_INT,
    "25",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_MEMLOCK,
    "LockPagesInMainMemory",
    DB_TOKEN,
    "If set to yes, then NDB Cluster data will not be swapped out to disk",
    CI_USED,
    true,
    CI_INT,
    "0",
    "1",
    "2" },

  {
    CFG_DB_WATCHDOG_INTERVAL,
    "TimeBetweenWatchDogCheck",
    DB_TOKEN,
    "Time between execution checks inside a database node",
    CI_USED,
    true,
    CI_INT,
    "6000",
    "70",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_STOP_ON_ERROR,
    "StopOnError",
    DB_TOKEN,
    "If set to N, "DB_TOKEN_PRINT" automatically restarts/recovers in case of node failure",
    CI_USED,
    true,
    CI_BOOL,
    "true",
    "false",
    "true" },

  { 
    CFG_DB_STOP_ON_ERROR_INSERT,
    "RestartOnErrorInsert",
    DB_TOKEN,
    "See src/kernel/vm/Emulator.hpp NdbRestartType for details",
    CI_INTERNAL,
    true,
    CI_INT,
    "2",
    "0",
    "4" },
  
  {
    CFG_DB_NO_OPS,
    "MaxNoOfConcurrentOperations",
    DB_TOKEN,
    "Max number of operation records in transaction coordinator",
    CI_USED,
    false,
    CI_INT,
    "32k",
    "32",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_LOCAL_OPS,
    "MaxNoOfLocalOperations",
    DB_TOKEN,
    "Max number of operation records defined in the local storage node",
    CI_USED,
    false,
    CI_INT,
    UNDEFINED,
    "32",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_LOCAL_SCANS,
    "MaxNoOfLocalScans",
    DB_TOKEN,
    "Max number of fragment scans in parallel in the local storage node",
    CI_USED,
    false,
    CI_INT,
    UNDEFINED,
    "32",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_BATCH_SIZE,
    "BatchSizePerLocalScan",
    DB_TOKEN,
    "Used to calculate the number of lock records for scan with hold lock",
    CI_USED,
    false,
    CI_INT,
    STR_VALUE(DEF_BATCH_SIZE),
    "1",
    STR_VALUE(MAX_PARALLEL_OP_PER_SCAN) },

  {
    CFG_DB_NO_TRANSACTIONS,
    "MaxNoOfConcurrentTransactions",
    DB_TOKEN,
    "Max number of transaction executing concurrently on the "DB_TOKEN_PRINT" node",
    CI_USED,
    false,
    CI_INT,
    "4096",
    "32",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_NO_SCANS,
    "MaxNoOfConcurrentScans",
    DB_TOKEN,
    "Max number of scans executing concurrently on the "DB_TOKEN_PRINT" node",
    CI_USED,
    false,
    CI_INT,
    "256",
    "2",
    "500" },

  {
    CFG_DB_TRANS_BUFFER_MEM,
    "TransactionBufferMemory",
    DB_TOKEN,
    "Dynamic buffer space (in bytes) for key and attribute data allocated for each "DB_TOKEN_PRINT" node",
    CI_USED,
    false,
    CI_INT,
    "1M",
    "1K",
    STR_VALUE(MAX_INT_RNIL) },
 
  {
    CFG_DB_INDEX_MEM,
    "IndexMemory",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for storing indexes",
    CI_USED,
    false,
    CI_INT64,
    "18M",
    "1M",
    "1024G" },

  {
    CFG_DB_DATA_MEM,
    "DataMemory",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for storing data",
    CI_USED,
    false,
    CI_INT64,
    "80M",
    "1M",
    "1024G" },

  {
    CFG_DB_UNDO_INDEX_BUFFER,
    "UndoIndexBuffer",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for writing UNDO logs for index part",
    CI_USED,
    false,
    CI_INT,
    "2M",
    "1M",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_UNDO_DATA_BUFFER,
    "UndoDataBuffer",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for writing UNDO logs for data part",
    CI_USED,
    false,
    CI_INT,
    "16M",
    "1M",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_REDO_BUFFER,
    "RedoBuffer",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for writing REDO logs",
    CI_USED,
    false,
    CI_INT,
    "8M",
    "1M",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_LONG_SIGNAL_BUFFER,
    "LongMessageBuffer",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for internal long messages",
    CI_USED,
    false,
    CI_INT,
    "1M",
    "512k",
    STR_VALUE(MAX_INT_RNIL)},

  {
    CFG_DB_DISK_PAGE_BUFFER_MEMORY,
    "DiskPageBufferMemory",
    DB_TOKEN,
    "Number bytes on each "DB_TOKEN_PRINT" node allocated for disk page buffer cache",
    CI_USED,
    false,
    CI_INT64,
    "64M",
    "4M",
    "1024G" },

  {
    CFG_DB_SGA,
    "SharedGlobalMemory",
    DB_TOKEN,
    "Total number bytes on each "DB_TOKEN_PRINT" node allocated for any use",
    CI_USED,
    false,
    CI_INT64,
    "20M",
    "0",
    "65536G" }, // 32k pages * 32-bit i value
  
  {
    CFG_DB_START_PARTIAL_TIMEOUT,
    "StartPartialTimeout",
    DB_TOKEN,
    "Time to wait before trying to start wo/ all nodes. 0=Wait forever",
    CI_USED,
    true,
    CI_INT,
    "30000",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_START_PARTITION_TIMEOUT,
    "StartPartitionedTimeout",
    DB_TOKEN,
    "Time to wait before trying to start partitioned. 0=Wait forever",
    CI_USED,
    true,
    CI_INT,
    "60000",
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_START_FAILURE_TIMEOUT,
    "StartFailureTimeout",
    DB_TOKEN,
    "Time to wait before terminating. 0=Wait forever",
    CI_USED,
    true,
    CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalDbDb",
    DB_TOKEN,
    "Time between "DB_TOKEN_PRINT"-"DB_TOKEN_PRINT" heartbeats. "DB_TOKEN_PRINT" considered dead after 3 missed HBs",
    CI_USED,
    true,
    CI_INT,
    "1500",
    "10",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_API_HEARTBEAT_INTERVAL,
    "HeartbeatIntervalDbApi",
    DB_TOKEN,
    "Time between "API_TOKEN_PRINT"-"DB_TOKEN_PRINT" heartbeats. "API_TOKEN_PRINT" connection closed after 3 missed HBs",
    CI_USED,
    true,
    CI_INT,
    "1500",
    "100",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_INTERVAL,
    "TimeBetweenLocalCheckpoints",
    DB_TOKEN,
    "Time between taking snapshots of the database (expressed in 2log of bytes)",
    CI_USED,
    true,
    CI_INT,
    "20",
    "0",
    "31" },

  {
    CFG_DB_GCP_INTERVAL,
    "TimeBetweenGlobalCheckpoints",
    DB_TOKEN,
    "Time between doing group commit of transactions to disk",
    CI_USED,
    true,
    CI_INT,
    "2000",
    "10",
    "32000" },

  {
    CFG_DB_NO_REDOLOG_FILES,
    "NoOfFragmentLogFiles",
    DB_TOKEN,
    "No of 16 Mbyte Redo log files in each of 4 file sets belonging to "DB_TOKEN_PRINT" node",
    CI_USED,
    false,
    CI_INT,
    "16",
    "3",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_MAX_OPEN_FILES,
    "MaxNoOfOpenFiles",
    DB_TOKEN,
    "Max number of files open per "DB_TOKEN_PRINT" node.(One thread is created per file)",
    CI_USED,
    false,
    CI_INT,
    "0",
    "20",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_INITIAL_OPEN_FILES,
    "InitialNoOfOpenFiles",
    DB_TOKEN,
    "Initial number of files open per "DB_TOKEN_PRINT" node.(One thread is created per file)",
    CI_USED,
    false,
    CI_INT,
    "27",
    "20",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_DB_TRANSACTION_CHECK_INTERVAL,
    "TimeBetweenInactiveTransactionAbortCheck",
    DB_TOKEN,
    "Time between inactive transaction checks",
    CI_USED,
    true,
    CI_INT,
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
    CI_USED,
    true,
    CI_INT,
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
    CI_USED,
    true,
    CI_INT,
    "1200",
    "50",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_DISC_PAGES_TUP_SR,
    "NoOfDiskPagesToDiskDuringRestartTUP",
    DB_TOKEN,
    "DiskCheckpointSpeedSr",
    CI_DEPRICATED,
    true,
    CI_INT,
    "40",
    "1",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_DISC_PAGES_TUP,
    "NoOfDiskPagesToDiskAfterRestartTUP",
    DB_TOKEN,
    "DiskCheckpointSpeed",
    CI_DEPRICATED,
    true,
    CI_INT,
    "40",
    "1",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_DISC_PAGES_ACC_SR,
    "NoOfDiskPagesToDiskDuringRestartACC",
    DB_TOKEN,
    "DiskCheckpointSpeedSr",
    CI_DEPRICATED,
    true,
    CI_INT,
    "20",
    "1",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_DB_LCP_DISC_PAGES_ACC,
    "NoOfDiskPagesToDiskAfterRestartACC",
    DB_TOKEN,
    "DiskCheckpointSpeed",
    CI_DEPRICATED,
    true,
    CI_INT,
    "20",
    "1",
    STR_VALUE(MAX_INT_RNIL) },
  

  {
    CFG_DB_DISCLESS,
    "Diskless",
    DB_TOKEN,
    "Run wo/ disk",
    CI_USED,
    true,
    CI_BOOL,
    "false",
    "false",
    "true"},

  {
    KEY_INTERNAL,
    "Discless",
    DB_TOKEN,
    "Diskless",
    CI_DEPRICATED,
    true,
    CI_BOOL,
    "false",
    "false",
    "true"},
  

  
  {
    CFG_DB_ARBIT_TIMEOUT,
    "ArbitrationTimeout",
    DB_TOKEN,
    "Max time (milliseconds) database partion waits for arbitration signal",
    CI_USED,
    false,
    CI_INT,
    "3000",
    "10",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_NODE_DATADIR,
    "DataDir",
    DB_TOKEN,
    "Data directory for this node",
    CI_USED,
    false,
    CI_STRING,
    MYSQLCLUSTERDIR,
    0, 0 },

  {
    CFG_DB_FILESYSTEM_PATH,
    "FileSystemPath",
    DB_TOKEN,
    "Path to directory where the "DB_TOKEN_PRINT" node stores its data (directory must exist)",
    CI_USED,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_LOGLEVEL_STARTUP,
    "LogLevelStartup",
    DB_TOKEN,
    "Node startup info printed on stdout",
    CI_USED,
    false,
    CI_INT,
    "1",
    "0",
    "15" },
  
  {
    CFG_LOGLEVEL_SHUTDOWN,
    "LogLevelShutdown",
    DB_TOKEN,
    "Node shutdown info printed on stdout",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_STATISTICS,
    "LogLevelStatistic",
    DB_TOKEN,
    "Transaction, operation, transporter info printed on stdout",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_CHECKPOINT,
    "LogLevelCheckpoint",
    DB_TOKEN,
    "Local and Global checkpoint info printed on stdout",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_NODERESTART,
    "LogLevelNodeRestart",
    DB_TOKEN,
    "Node restart, node failure info printed on stdout",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_CONNECTION,
    "LogLevelConnection",
    DB_TOKEN,
    "Node connect/disconnect info printed on stdout",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_CONGESTION,
    "LogLevelCongestion",
    DB_TOKEN,
    "Congestion info printed on stdout",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_ERROR,
    "LogLevelError",
    DB_TOKEN,
    "Transporter, heartbeat errors printed on stdout",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    "15" },

  {
    CFG_LOGLEVEL_INFO,
    "LogLevelInfo",
    DB_TOKEN,
    "Heartbeat and log info printed on stdout",
    CI_USED,
    false,
    CI_INT,
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
    CI_NOTIMPLEMENTED,
    false,
    CI_INT,
    "1",
    "1",
    "1" },
  
  { 
    CFG_DB_BACKUP_DATADIR,
    "BackupDataDir",
    DB_TOKEN,
    "Path to where to store backups",
    CI_USED,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },
  
  { 
    CFG_DB_DISK_SYNCH_SIZE,
    "DiskSyncSize",
    DB_TOKEN,
    "Data written to a file before a synch is forced",
    CI_USED,
    false,
    CI_INT,
    "4M",
    "32k",
    STR_VALUE(MAX_INT_RNIL) },
  
  { 
    CFG_DB_CHECKPOINT_SPEED,
    "DiskCheckpointSpeed",
    DB_TOKEN,
    "Bytes per second allowed to be written by checkpoint",
    CI_USED,
    false,
    CI_INT,
    "10M",
    "1M",
    STR_VALUE(MAX_INT_RNIL) },
  
  { 
    CFG_DB_CHECKPOINT_SPEED_SR,
    "DiskCheckpointSpeedInRestart",
    DB_TOKEN,
    "Bytes per second allowed to be written by checkpoint during restart",
    CI_USED,
    false,
    CI_INT,
    "100M",
    "1M",
    STR_VALUE(MAX_INT_RNIL) },
  
  { 
    CFG_DB_BACKUP_MEM,
    "BackupMemory",
    DB_TOKEN,
    "Total memory allocated for backups per node (in bytes)",
    CI_USED,
    false,
    CI_INT,
    "4M", // sum of BackupDataBufferSize and BackupLogBufferSize
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  { 
    CFG_DB_BACKUP_DATA_BUFFER_MEM,
    "BackupDataBufferSize",
    DB_TOKEN,
    "Default size of databuffer for a backup (in bytes)",
    CI_USED,
    false,
    CI_INT,
    "2M", // remember to change BackupMemory
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_BACKUP_LOG_BUFFER_MEM,
    "BackupLogBufferSize",
    DB_TOKEN,
    "Default size of logbuffer for a backup (in bytes)",
    CI_USED,
    false,
    CI_INT,
    "2M", // remember to change BackupMemory
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_BACKUP_WRITE_SIZE,
    "BackupWriteSize",
    DB_TOKEN,
    "Default size of filesystem writes made by backup (in bytes)",
    CI_USED,
    false,
    CI_INT,
    "32K",
    "2K",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_BACKUP_MAX_WRITE_SIZE,
    "BackupMaxWriteSize",
    DB_TOKEN,
    "Max size of filesystem writes made by backup (in bytes)",
    CI_USED,
    false,
    CI_INT,
    "256K",
    "2K",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_STRING_MEMORY,
    "StringMemory",
    DB_TOKEN,
    "Default size of string memory (0 -> 5% of max 1-100 -> %of max, >100 -> actual bytes)",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  { 
    CFG_DB_MEMREPORT_FREQUENCY,
    "MemReportFrequency",
    DB_TOKEN,
    "Frequency of mem reports in seconds, 0 = only when passing %-limits",
    CI_USED,
    false,
    CI_INT,
    "0",
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
    CI_USED,
    false,
    CI_SECTION,
    (const char *)NODE_TYPE_API, 
    0, 0
  },

  {
    CFG_NODE_HOST,
    "HostName",
    API_TOKEN,
    "Name of computer for this node",
    CI_INTERNAL,
    false,
    CI_STRING,
    "",
    0, 0 },

  {
    CFG_NODE_SYSTEM,
    "System",
    API_TOKEN,
    "Name of system for this node",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    KEY_INTERNAL,
    "Id",
    API_TOKEN,
    "",
    CI_DEPRICATED,
    false,
    CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },

  {
    CFG_NODE_ID,
    "NodeId",
    API_TOKEN,
    "Number identifying application node ("API_TOKEN_PRINT")",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },

  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    API_TOKEN,
    "String referencing an earlier defined COMPUTER",
    CI_USED,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_NODE_ARBIT_RANK,
    "ArbitrationRank",
    API_TOKEN,
    "If 0, then "API_TOKEN_PRINT" is not arbitrator. Kernel selects arbitrators in order 1, 2",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    "2" },

  {
    CFG_NODE_ARBIT_DELAY,
    "ArbitrationDelay",
    API_TOKEN,
    "When asked to arbitrate, arbitrator waits this long before voting (msec)",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_MAX_SCAN_BATCH_SIZE,
    "MaxScanBatchSize",
    "API",
    "The maximum collective batch size for one scan",
    CI_USED,
    false,
    CI_INT,
    STR_VALUE(MAX_SCAN_BATCH_SIZE),
    "32k",
    "16M" },
  
  {
    CFG_BATCH_BYTE_SIZE,
    "BatchByteSize",
    "API",
    "The default batch size in bytes",
    CI_USED,
    false,
    CI_INT,
    STR_VALUE(SCAN_BATCH_SIZE),
    "1k",
    "1M" },

  {
    CFG_BATCH_SIZE,
    "BatchSize",
    "API",
    "The default batch size in number of records",
    CI_USED,
    false,
    CI_INT,
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
    CI_USED,
    false,
    CI_SECTION,
    (const char *)NODE_TYPE_MGM, 
    0, 0
  },

  {
    CFG_NODE_HOST,
    "HostName",
    MGM_TOKEN,
    "Name of computer for this node",
    CI_INTERNAL,
    false,
    CI_STRING,
    "",
    0, 0 },

  {
    CFG_NODE_DATADIR,
    "DataDir",
    MGM_TOKEN,
    "Data directory for this node",
    CI_USED,
    false,
    CI_STRING,
    MYSQLCLUSTERDIR,
    0, 0 },

  {
    CFG_NODE_SYSTEM,
    "System",
    MGM_TOKEN,
    "Name of system for this node",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    KEY_INTERNAL,
    "Id",
    MGM_TOKEN,
    "",
    CI_DEPRICATED,
    false,
    CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },
  
  {
    CFG_NODE_ID,
    "NodeId",
    MGM_TOKEN,
    "Number identifying the management server node ("MGM_TOKEN_PRINT")",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "1",
    STR_VALUE(MAX_NODES) },
  
  {
    CFG_LOG_DESTINATION,
    "LogDestination",
    MGM_TOKEN,
    "String describing where logmessages are sent",
    CI_USED,
    false,
    CI_STRING,
    0,
    0, 0 },
  
  {
    KEY_INTERNAL,
    "ExecuteOnComputer",
    MGM_TOKEN,
    "String referencing an earlier defined COMPUTER",
    CI_USED,
    false,
    CI_STRING,
    0,
    0, 0 },

  {
    KEY_INTERNAL,
    "MaxNoOfSavedEvents",
    MGM_TOKEN,
    "",
    CI_USED,
    false,
    CI_INT,
    "100",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_MGM_PORT,
    "PortNumber",
    MGM_TOKEN,
    "Port number to give commands to/fetch configurations from management server",
    CI_USED,
    false,
    CI_INT,
    NDB_PORT,
    "0",
    STR_VALUE(MAX_PORT_NO) },

  {
    KEY_INTERNAL,
    "PortNumberStats",
    MGM_TOKEN,
    "Port number used to get statistical information from a management server",
    CI_USED,
    false,
    CI_INT,
    UNDEFINED,
    "0",
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_NODE_ARBIT_RANK,
    "ArbitrationRank",
    MGM_TOKEN,
    "If 0, then "MGM_TOKEN_PRINT" is not arbitrator. Kernel selects arbitrators in order 1, 2",
    CI_USED,
    false,
    CI_INT,
    "1",
    "0",
    "2" },

  {
    CFG_NODE_ARBIT_DELAY,
    "ArbitrationDelay",
    MGM_TOKEN,
    "",
    CI_USED,
    false,
    CI_INT,
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
    CI_USED,
    false,
    CI_SECTION,
    (const char *)CONNECTION_TYPE_TCP, 
    0, 0
  },

  {
    CFG_CONNECTION_HOSTNAME_1,
    "HostName1",
    "TCP",
    "Name/IP of computer on one side of the connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "TCP",
    "Name/IP of computer on one side of the connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "TCP",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    CI_USED,
    false,
    CI_STRING,
    MANDATORY,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "TCP",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    CI_USED,
    false,
    CI_STRING,
    MANDATORY,
    0, 0 },

  {
    CFG_CONNECTION_GROUP,
    "Group",
    "TCP",
    "",
    CI_USED,
    false,
    CI_INT,
    "55",
    "0", "200" },

  {
    CFG_CONNECTION_NODE_ID_SERVER,
    "NodeIdServer",
    "TCP",
    "",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "1", "63" },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "TCP",
    "Sends id in each signal.  Used in trace files.",
    CI_USED,
    false,
    CI_BOOL,
    "true",
    "false",
    "true" },


  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "TCP",
    "If checksum is enabled, all signals between nodes are checked for errors",
    CI_USED,
    false,
    CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "TCP",
    "Port used for this transporter",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_TCP_SEND_BUFFER_SIZE,
    "SendBufferMemory",
    "TCP",
    "Bytes of buffer for signals sent from this node",
    CI_USED,
    false,
    CI_INT,
    "256K",
    "64K",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_TCP_RECEIVE_BUFFER_SIZE,
    "ReceiveBufferMemory",
    "TCP",
    "Bytes of buffer for signals received by this node",
    CI_USED,
    false,
    CI_INT,
    "64K",
    "16K",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_TCP_PROXY,
    "Proxy",
    "TCP",
    "",
    CI_USED,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "TCP",
    "System for node 1 in connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "TCP",
    "System for node 2 in connection",
    CI_INTERNAL,
    false,
    CI_STRING,
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
    CI_USED,
    false,
    CI_SECTION,
    (const char *)CONNECTION_TYPE_SHM, 
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_1,
    "HostName1",
    "SHM",
    "Name/IP of computer on one side of the connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "SHM",
    "Name/IP of computer on one side of the connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "SHM",
    "Port used for this transporter",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "0", 
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_SHM_SIGNUM,
    "Signum",
    "SHM",
    "Signum to be used for signalling",
    CI_USED,
    false,
    CI_INT,
    UNDEFINED,
    "0", 
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "SHM",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    CI_USED,
    false,
    CI_STRING,
    MANDATORY,
    0, 0 },
  
  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "SHM",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    CI_USED,
    false,
    CI_STRING,
    MANDATORY,
    0, 0 },
  
  {
    CFG_CONNECTION_GROUP,
    "Group",
    "SHM",
    "",
    CI_USED,
    false,
    CI_INT,
    "35",
    "0", "200" },

  {
    CFG_CONNECTION_NODE_ID_SERVER,
    "NodeIdServer",
    "SHM",
    "",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "1", "63" },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "SHM",
    "Sends id in each signal.  Used in trace files.",
    CI_USED,
    false,
    CI_BOOL,
    "false",
    "false",
    "true" },
  
  
  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "SHM",
    "If checksum is enabled, all signals between nodes are checked for errors",
    CI_USED,
    false,
    CI_BOOL,
    "true",
    "false",
    "true" },
  
  {
    CFG_SHM_KEY,
    "ShmKey",
    "SHM",
    "A shared memory key",
    CI_USED,
    false,
    CI_INT,
    UNDEFINED,
    "0",
    STR_VALUE(MAX_INT_RNIL) },
  
  {
    CFG_SHM_BUFFER_MEM,
    "ShmSize",
    "SHM",
    "Size of shared memory segment",
    CI_USED,
    false,
    CI_INT,
    "1M",
    "64K",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "SHM",
    "System for node 1 in connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "SHM",
    "System for node 2 in connection",
    CI_INTERNAL,
    false,
    CI_STRING,
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
    CI_USED,
    false,
    CI_SECTION,
    (const char *)CONNECTION_TYPE_SCI, 
    0, 0 
  },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "SCI",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    CI_USED,
    false,
    CI_STRING,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "SCI",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    CI_USED,
    false,
    CI_STRING,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_GROUP,
    "Group",
    "SCI",
    "",
    CI_USED,
    false,
    CI_INT,
    "15",
    "0", "200" },

  {
    CFG_CONNECTION_NODE_ID_SERVER,
    "NodeIdServer",
    "SCI",
    "",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "1", "63" },

  {
    CFG_CONNECTION_HOSTNAME_1,
    "HostName1",
    "SCI",
    "Name/IP of computer on one side of the connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "SCI",
    "Name/IP of computer on one side of the connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_SERVER_PORT,
    "PortNumber",
    "SCI",
    "Port used for this transporter",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "0", 
    STR_VALUE(MAX_PORT_NO) },

  {
    CFG_SCI_HOST1_ID_0,
    "Host1SciId0",
    "SCI",
    "SCI-node id for adapter 0 on Host1 (a computer can have two adapters)",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST1_ID_1,
    "Host1SciId1",
    "SCI",
    "SCI-node id for adapter 1 on Host1 (a computer can have two adapters)",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST2_ID_0,
    "Host2SciId0",
    "SCI",
    "SCI-node id for adapter 0 on Host2 (a computer can have two adapters)",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_SCI_HOST2_ID_1,
    "Host2SciId1",
    "SCI",
    "SCI-node id for adapter 1 on Host2 (a computer can have two adapters)",
    CI_USED,
    false,
    CI_INT,
    "0",
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "SCI",
    "Sends id in each signal.  Used in trace files.",
    CI_USED,
    false,
    CI_BOOL,
    "true",
    "false",
    "true" },

  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "SCI",
    "If checksum is enabled, all signals between nodes are checked for errors",
    CI_USED,
    false,
    CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_SCI_SEND_LIMIT,
    "SendLimit",
    "SCI",
    "Transporter send buffer contents are sent when this no of bytes is buffered",
    CI_USED,
    false,
    CI_INT,
    "8K",
    "128",
    "32K" },

  {
    CFG_SCI_BUFFER_MEM,
    "SharedBufferSize",
    "SCI",
    "Size of shared memory segment",
    CI_USED,
    false,
    CI_INT,
    "1M",
    "64K",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "SCI",
    "System for node 1 in connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "SCI",
    "System for node 2 in connection",
    CI_INTERNAL,
    false,
    CI_STRING,
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
    CI_USED,
    false,
    CI_SECTION,
    (const char *)CONNECTION_TYPE_OSE, 
    0, 0 
  },

  {
    CFG_CONNECTION_HOSTNAME_1,
    "HostName1",
    "OSE",
    "Name of computer on one side of the connection",
    CI_USED,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_HOSTNAME_2,
    "HostName2",
    "OSE",
    "Name of computer on one side of the connection",
    CI_USED,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_1,
    "NodeId1",
    "OSE",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    CI_USED,
    false,
    CI_INT,
    MANDATORY,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_NODE_2,
    "NodeId2",
    "OSE",
    "Id of node ("DB_TOKEN_PRINT", "API_TOKEN_PRINT" or "MGM_TOKEN_PRINT") on one side of the connection",
    CI_USED,
    false,
    CI_INT,
    UNDEFINED,
    "0",
    STR_VALUE(MAX_INT_RNIL) },

  {
    CFG_CONNECTION_SEND_SIGNAL_ID,
    "SendSignalId",
    "OSE",
    "Sends id in each signal.  Used in trace files.",
    CI_USED,
    false,
    CI_BOOL,
    "true",
    "false",
    "true" },

  {
    CFG_CONNECTION_CHECKSUM,
    "Checksum",
    "OSE",
    "If checksum is enabled, all signals between nodes are checked for errors",
    CI_USED,
    false,
    CI_BOOL,
    "false",
    "false",
    "true" },

  {
    CFG_CONNECTION_NODE_1_SYSTEM,
    "NodeId1_System",
    "OSE",
    "System for node 1 in connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },

  {
    CFG_CONNECTION_NODE_2_SYSTEM,
    "NodeId2_System",
    "OSE",
    "System for node 2 in connection",
    CI_INTERNAL,
    false,
    CI_STRING,
    UNDEFINED,
    0, 0 },
};

const int ParamInfoNum = sizeof(ParamInfoArray) / sizeof(ParamInfo);
