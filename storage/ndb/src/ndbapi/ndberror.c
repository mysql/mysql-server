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
#include <my_base.h>
#include <ndberror.h>
#include <m_string.h>

#include "../mgmsrv/ndb_mgmd_error.h"


typedef struct ErrorBundle {
  int code;
  int mysql_code;
  ndberror_classification classification;
  const char * message;
} ErrorBundle;

/**
 * Shorter names in table below
 */

#define ST_S ndberror_st_success
#define ST_P ndberror_st_permanent
#define ST_T ndberror_st_temporary
#define ST_U ndberror_st_unknown

#define NE ndberror_cl_none
#define AE ndberror_cl_application
#define CE ndberror_cl_configuration
#define ND ndberror_cl_no_data_found
#define CV ndberror_cl_constraint_violation
#define SE ndberror_cl_schema_error
#define UD ndberror_cl_user_defined

#define IS ndberror_cl_insufficient_space
#define TR ndberror_cl_temporary_resource
#define NR ndberror_cl_node_recovery
#define OL ndberror_cl_overload
#define TO ndberror_cl_timeout_expired
#define NS ndberror_cl_node_shutdown

#define UR ndberror_cl_unknown_result

#define IE ndberror_cl_internal_error
#define NI ndberror_cl_function_not_implemented
#define UE ndberror_cl_unknown_error_code

#define OE ndberror_cl_schema_object_already_exists

#define IT ndberror_cl_internal_temporary

/* default mysql error code for unmapped codes */
#define DMEC -1

static const char* empty_string = "";

/*
 * Error code ranges are reserved for respective block
 *
 *  200 - TC
 *  300 - DIH
 *  400 - LQH
 *  600 - ACC
 *  700 - DICT
 *  800 - TUP
 *  900 - TUX
 * 1200 - LQH
 * 1300 - BACKUP
 * 1400 - SUMA
 * 1500 - LGMAN
 * 1600 - TSMAN
 * 1700 - QMGR
 * 4000 - API
 * 4100 - ""
 * 4200 - ""
 * 4300 - ""
 * 4400 - ""
 * 4500 - ""
 * 4600 - ""
 * 4700 - "" Event
 * 5000 - Management server
 */

static
const 
ErrorBundle ErrorCodes[] = {
  /**
   * No error
   */
  { 0,    0, NE, "No error" },
  
  /**
   * NoDataFound
   */
  { 626,  HA_ERR_KEY_NOT_FOUND, ND, "Tuple did not exist" },

  /**
   * ConstraintViolation 
   */
  { 630,  HA_ERR_FOUND_DUPP_KEY, CV, "Tuple already existed when attempting to insert" },
  { 839,  DMEC, CV, "Illegal null attribute" },
  { 840,  DMEC, CV, "Trying to set a NOT NULL attribute to NULL" },
  { 893,  HA_ERR_FOUND_DUPP_KEY, CV, "Constraint violation e.g. duplicate value in unique index" },

  /**
   * Node recovery errors
   */
  {  286, DMEC, NR, "Node failure caused abort of transaction" }, 
  {  250, DMEC, NR, "Node where lock was held crashed, restart scan transaction" },
  {  499, DMEC, NR, "Scan take over error, restart scan transaction" },  
  { 1204, DMEC, NR, "Temporary failure, distribution changed" },
  { 4002, DMEC, NR, "Send to NDB failed" },
  { 4010, DMEC, NR, "Node failure caused abort of transaction" }, 
  { 4025, DMEC, NR, "Node failure caused abort of transaction" }, 
  { 4027, DMEC, NR, "Node failure caused abort of transaction" },
  { 4028, DMEC, NR, "Node failure caused abort of transaction" },
  { 4029, DMEC, NR, "Node failure caused abort of transaction" },
  { 4031, DMEC, NR, "Node failure caused abort of transaction" },
  { 4033, DMEC, NR, "Send to NDB failed" },
  { 4115, DMEC, NR, 
    "Transaction was committed but all read information was not "
    "received due to node crash" },
  { 4119, DMEC, NR, "Simple/dirty read failed due to node failure" },
  
  /**
   * Node shutdown
   */
  {  280, DMEC, NS, "Transaction aborted due to node shutdown" },
  /* This scan trans had an active fragment scan in a LQH which have crashed */
  {  270, DMEC, NS, "Transaction aborted due to node shutdown" }, 
  { 1223, DMEC, NS, "Read operation aborted due to node shutdown" },
  { 4023, DMEC, NS, "Transaction aborted due to node shutdown" },
  { 4030, DMEC, NS, "Transaction aborted due to node shutdown" },
  { 4034, DMEC, NS, "Transaction aborted due to node shutdown" },


  
  /**
   * Unknown result
   */
  { 4007, DMEC, UR, "Send to ndbd node failed" },
  { 4008, DMEC, UR, "Receive from NDB failed" },
  { 4009, HA_ERR_NO_CONNECTION, UR, "Cluster Failure" },
  { 4012, DMEC, UR, 
    "Request ndbd time-out, maybe due to high load or communication problems"}, 
  { 4013, DMEC, UR, "Request timed out in waiting for node failure"}, 
  { 4024, DMEC, UR, 
    "Time-out, most likely caused by simple read or cluster failure" }, 
  
  /**
   * TemporaryResourceError
   */
  { 217,  DMEC, TR, "217" },
  { 218,  DMEC, TR, "218" },
  { 219,  DMEC, TR, "219" },
  { 233,  DMEC, TR,
    "Out of operation records in transaction coordinator (increase MaxNoOfConcurrentOperations)" },
  { 275,  DMEC, TR, "Out of transaction records for complete phase (increase MaxNoOfConcurrentTransactions)" },
  { 279,  DMEC, TR, "Out of transaction markers in transaction coordinator" },
  { 414,  DMEC, TR, "414" },
  { 418,  DMEC, TR, "Out of transaction buffers in LQH" },
  { 419,  DMEC, TR, "419" },
  { 245,  DMEC, TR, "Too many active scans" },
  { 488,  DMEC, TR, "Too many active scans" },
  { 490,  DMEC, TR, "Too many active scans" },
  { 805,  DMEC, TR, "Out of attrinfo records in tuple manager" },
  { 830,  DMEC, TR, "Out of add fragment operation records" },
  { 873,  DMEC, TR, "Out of attrinfo records for scan in tuple manager" },
  { 899,  DMEC, TR, "Rowid already allocated" },
  { 1217, DMEC, TR, "Out of operation records in local data manager (increase MaxNoOfLocalOperations)" },
  { 1218, DMEC, TR, "Send Buffers overloaded in NDB kernel" },
  { 1220, DMEC, TR, "REDO log files overloaded, consult online manual (increase FragmentLogFileSize)" },
  { 1222, DMEC, TR, "Out of transaction markers in LQH" },
  { 4021, DMEC, TR, "Out of Send Buffer space in NDB API" },
  { 4022, DMEC, TR, "Out of Send Buffer space in NDB API" },
  { 4032, DMEC, TR, "Out of Send Buffer space in NDB API" },
  { 1501, DMEC, TR, "Out of undo space" },
  {  288, DMEC, TR, "Out of index operations in transaction coordinator (increase MaxNoOfConcurrentIndexOperations)" },
  {  289, DMEC, TR, "Out of transaction buffer memory in TC (increase TransactionBufferMemory)" },

  /**
   * InsufficientSpace
   */
  { 623,  HA_ERR_RECORD_FILE_FULL, IS, "623" },
  { 624,  HA_ERR_RECORD_FILE_FULL, IS, "624" },
  { 625,  HA_ERR_INDEX_FILE_FULL, IS, "Out of memory in Ndb Kernel, hash index part (increase IndexMemory)" },
  { 640,  DMEC, IS, "Too many hash indexes (should not happen)" },
  { 826,  HA_ERR_RECORD_FILE_FULL, IS, "Too many tables and attributes (increase MaxNoOfAttributes or MaxNoOfTables)" },
  { 827,  HA_ERR_RECORD_FILE_FULL, IS, "Out of memory in Ndb Kernel, table data (increase DataMemory)" },
  { 902,  HA_ERR_RECORD_FILE_FULL, IS, "Out of memory in Ndb Kernel, ordered index data (increase DataMemory)" },
  { 903,  HA_ERR_INDEX_FILE_FULL, IS, "Too many ordered indexes (increase MaxNoOfOrderedIndexes)" },
  { 904,  HA_ERR_INDEX_FILE_FULL, IS, "Out of fragment records (increase MaxNoOfOrderedIndexes)" },
  { 905,  DMEC, IS, "Out of attribute records (increase MaxNoOfAttributes)" },
  { 1601, HA_ERR_RECORD_FILE_FULL, IS, "Out extents, tablespace full" },
  { 1602, DMEC, IS,"No datafile in tablespace" },

  /**
   * TimeoutExpired 
   */
  { 266,  HA_ERR_LOCK_WAIT_TIMEOUT, TO, "Time-out in NDB, probably caused by deadlock" },
  { 274,  HA_ERR_LOCK_WAIT_TIMEOUT, TO, "Time-out in NDB, probably caused by deadlock" }, /* Scan trans timeout */
  { 296,  HA_ERR_LOCK_WAIT_TIMEOUT, TO, "Time-out in NDB, probably caused by deadlock" }, /* Scan trans timeout */
  { 297,  HA_ERR_LOCK_WAIT_TIMEOUT, TO, "Time-out in NDB, probably caused by deadlock" }, /* Scan trans timeout, temporary!! */
  { 237,  HA_ERR_LOCK_WAIT_TIMEOUT, TO, "Transaction had timed out when trying to commit it" },
  
  /**
   * OverloadError
   */
  { 701,  DMEC, OL, "System busy with other schema operation" },
  { 711,  DMEC, OL, "System busy with node restart, schema operations not allowed" },
  { 410,  DMEC, OL, "REDO log files overloaded, consult online manual (decrease TimeBetweenLocalCheckpoints, and|or increase NoOfFragmentLogFiles)" },
  { 677,  DMEC, OL, "Index UNDO buffers overloaded (increase UndoIndexBuffer)" },
  { 891,  DMEC, OL, "Data UNDO buffers overloaded (increase UndoDataBuffer)" },
  { 1221, DMEC, OL, "REDO buffers overloaded, consult online manual (increase RedoBuffer)" },
  { 4006, DMEC, OL, "Connect failure - out of connection objects (increase MaxNoOfConcurrentTransactions)" }, 


  /*
   * Internal Temporary
   */
  { 702,  DMEC, IT, "Request to non-master" },
  
  /**
   * Internal errors
   */
  { 896,  DMEC, IE, "Tuple corrupted - wrong checksum or column data in invalid format" },
  { 901,  DMEC, IE, "Inconsistent ordered index. The index needs to be dropped and recreated" },
  { 202,  DMEC, IE, "202" },
  { 203,  DMEC, IE, "203" },
  { 207,  DMEC, IE, "207" },
  { 208,  DMEC, IE, "208" },
  { 209,  DMEC, IE, "Communication problem, signal error" },
  { 220,  DMEC, IE, "220" },
  { 230,  DMEC, IE, "230" },
  { 232,  DMEC, IE, "232" },
  { 238,  DMEC, IE, "238" },
  { 271,  DMEC, IE, "Simple Read transaction without any attributes to read" },
  { 272,  DMEC, IE, "Update operation without any attributes to update" },
  { 276,  DMEC, IE, "276" },
  { 277,  DMEC, IE, "277" },
  { 278,  DMEC, IE, "278" },
  { 287,  DMEC, IE, "Index corrupted" },
  { 290,  DMEC, IE, "Corrupt key in TC, unable to xfrm" },
  { 631,  DMEC, IE, "631" },
  { 632,  DMEC, IE, "632" },
  { 706,  DMEC, IE, "Inconsistency during table creation" },
  { 809,  DMEC, IE, "809" },
  { 812,  DMEC, IE, "812" },
  { 829,  DMEC, IE, "829" },
  { 833,  DMEC, IE, "833" },
  { 871,  DMEC, IE, "871" },
  { 882,  DMEC, IE, "882" },
  { 883,  DMEC, IE, "883" },
  { 887,  DMEC, IE, "887" },
  { 888,  DMEC, IE, "888" },
  { 890,  DMEC, IE, "890" },
  { 4000, DMEC, IE, "MEMORY ALLOCATION ERROR" },
  { 4001, DMEC, IE, "Signal Definition Error" },
  { 4005, DMEC, IE, "Internal Error in NdbApi" },
  { 4011, DMEC, IE, "Internal Error in NdbApi" }, 
  { 4107, DMEC, IE, "Simple Transaction and Not Start" },
  { 4108, DMEC, IE, "Faulty operation type" },
  { 4109, DMEC, IE, "Faulty primary key attribute length" },
  { 4110, DMEC, IE, "Faulty length in ATTRINFO signal" },
  { 4111, DMEC, IE, "Status Error in NdbConnection" },
  { 4113, DMEC, IE, "Too many operations received" },
  { 4320, DMEC, IE, "Cannot use the same object twice to create table" },
  { 4321, DMEC, IE, "Trying to start two schema transactions" },
  { 4344, DMEC, IE, "Only DBDICT and TRIX can send requests to TRIX" },
  { 4345, DMEC, IE, "TRIX block is not available yet, probably due to node failure" },
  { 4346, DMEC, IE, "Internal error at index create/build" },
  { 4347, DMEC, IE, "Bad state at alter index" },
  { 4348, DMEC, IE, "Inconsistency detected at alter index" },
  { 4349, DMEC, IE, "Inconsistency detected at index usage" },
  { 4350, DMEC, IE, "Transaction already aborted" },

  /**
   * Application error
   */
  { 281,  HA_ERR_NO_CONNECTION, AE, "Operation not allowed due to cluster shutdown in progress" },
  { 299,  DMEC, AE, "Operation not allowed or aborted due to single user mode" },
  { 763,  DMEC, AE, "Alter table requires cluster nodes to have exact same version" },
  { 823,  DMEC, AE, "Too much attrinfo from application in tuple manager" },
  { 831,  DMEC, AE, "Too many nullable/bitfields in table definition" },
  { 876,  DMEC, AE, "876" },
  { 877,  DMEC, AE, "877" },
  { 878,  DMEC, AE, "878" },
  { 879,  DMEC, AE, "879" },
  { 880,  DMEC, AE, "Tried to read too much - too many getValue calls" },
  { 884,  DMEC, AE, "Stack overflow in interpreter" },
  { 885,  DMEC, AE, "Stack underflow in interpreter" },
  { 886,  DMEC, AE, "More than 65535 instructions executed in interpreter" },
  { 897,  DMEC, AE, "Update attempt of primary key via ndbcluster internal api (if this occurs via the MySQL server it is a bug, please report)" },
  { 892,  DMEC, AE, "Unsupported type in scan filter" },
  { 4256, DMEC, AE, "Must call Ndb::init() before this function" },
  { 4257, DMEC, AE, "Tried to read too much - too many getValue calls" },

  /** 
   * Scan application errors
   */
  { 242,  DMEC, AE, "Zero concurrency in scan"},
  { 244,  DMEC, AE, "Too high concurrency in scan"},
  { 269,  DMEC, AE, "No condition and attributes to read in scan"},
  { 4600, DMEC, AE, "Transaction is already started"},
  { 4601, DMEC, AE, "Transaction is not started"},
  { 4602, DMEC, AE, "You must call getNdbOperation before executeScan" },
  { 4603, DMEC, AE, "There can only be ONE operation in a scan transaction" },
  { 4604, DMEC, AE, "takeOverScanOp, to take over a scanned row one must explicitly request keyinfo on readTuples call" },
  { 4605, DMEC, AE, "You may only call openScanRead or openScanExclusive once for each operation"},
  { 4607, DMEC, AE, "There may only be one operation in a scan transaction"},
  { 4608, DMEC, AE, "You can not takeOverScan unless you have used openScanExclusive"},
  { 4609, DMEC, AE, "You must call nextScanResult before trying to takeOverScan"},
  { 4232, DMEC, AE, "Parallelism can only be between 1 and 240" },

  /** 
   * Event schema errors
   */

  { 4713,  DMEC, SE, "Column defined in event does not exist in table"},
  
  /** 
   * Event application errors
   */

  { 4707,  DMEC, AE, "Too many event have been defined"},
  { 4708,  DMEC, AE, "Event name is too long"},
  { 4709,  DMEC, AE, "Can't accept more subscribers"},
  {  746,  DMEC, OE, "Event name already exists"},
  {  747,  DMEC, IS, "Out of event records"},
  {  748,  DMEC, TR, "Busy during read of event table"},
  { 4710,  DMEC, AE, "Event not found"},
  { 4711,  DMEC, AE, "Creation of event failed"},
  { 4712,  DMEC, AE, "Stopped event operation does not exist. Already stopped?"},

  /** 
   * Event internal errors
   */

  { 4731,  DMEC, IE, "Event not found"},

  /**
   * SchemaError
   */
  { 311,  DMEC, AE, "Undefined partition used in setPartitionId" },
  { 703,  DMEC, SE, "Invalid table format" },
  { 704,  DMEC, SE, "Attribute name too long" },
  { 705,  DMEC, SE, "Table name too long" },
  { 707,  DMEC, SE, "No more table metadata records (increase MaxNoOfTables)" },  
  { 708,  DMEC, SE, "No more attribute metadata records (increase MaxNoOfAttributes)" },
  { 709,  HA_ERR_NO_SUCH_TABLE, SE, "No such table existed" },
  { 710,  DMEC, SE, "Internal: Get by table name not supported, use table id." },
  { 721,  HA_ERR_TABLE_EXIST,   OE, "Table or index with given name already exists" },
  { 723,  HA_ERR_NO_SUCH_TABLE, SE, "No such table existed" },
  { 736,  DMEC, SE, "Unsupported array size" },
  { 737,  HA_WRONG_CREATE_OPTION, SE, "Attribute array size too big" },
  { 738,  HA_WRONG_CREATE_OPTION, SE, "Record too big" },
  { 739,  HA_WRONG_CREATE_OPTION, SE, "Unsupported primary key length" },
  { 740,  HA_WRONG_CREATE_OPTION, SE, "Nullable primary key not supported" },
  { 741,  DMEC, SE, "Unsupported alter table" },
  { 743,  HA_WRONG_CREATE_OPTION, SE, "Unsupported character set in table or index" },
  { 744,  DMEC, SE, "Character string is invalid for given character set" },
  { 745,  HA_WRONG_CREATE_OPTION, SE, "Distribution key not supported for char attribute (use binary attribute)" },
  { 771,  HA_WRONG_CREATE_OPTION, AE, "Given NODEGROUP doesn't exist in this cluster" },
  { 772,  HA_WRONG_CREATE_OPTION, IE, "Given fragmentType doesn't exist" },
  { 749,  HA_WRONG_CREATE_OPTION, IE, "Primary Table in wrong state" },
  { 779,  HA_WRONG_CREATE_OPTION, SE, "Invalid undo buffer size" },
  { 764,  HA_WRONG_CREATE_OPTION, SE, "Invalid extent size" },
  { 765,  DMEC, SE, "Out of filegroup records" },
  { 750,  IE, SE, "Invalid file type" },
  { 751,  DMEC, SE, "Out of file records" },
  { 752,  DMEC, SE, "Invalid file format" },
  { 753,  IE, SE, "Invalid filegroup for file" },
  { 754,  IE, SE, "Invalid filegroup version when creating file" },
  { 755,  HA_WRONG_CREATE_OPTION, SE, "Invalid tablespace" },
  { 756,  DMEC, SE, "Index on disk column is not supported" },
  { 757,  DMEC, SE, "Varsize bitfield not supported" },
  { 758,  DMEC, SE, "Tablespace has changed" },
  { 759,  DMEC, SE, "Invalid tablespace version " },
  { 760,  DMEC, SE, "File already exists", },
  { 761,  DMEC, SE, "Unable to drop table as backup is in progress" },
  { 762,  DMEC, SE, "Unable to alter table as backup is in progress" },
  { 766,  DMEC, SE, "Cant drop file, no such file" },
  { 767,  DMEC, SE, "Cant drop filegroup, no such filegroup" },
  { 768,  DMEC, SE, "Cant drop filegroup, filegroup is used" },
  { 769,  DMEC, SE, "Drop undofile not supported, drop logfile group instead" },
  { 770,  DMEC, SE, "Cant drop file, file is used" },
  { 774,  DMEC, SE, "Invalid schema object for drop" },
  { 241,  HA_ERR_TABLE_DEF_CHANGED, SE, "Invalid schema object version" },
  { 283,  HA_ERR_NO_SUCH_TABLE, SE, "Table is being dropped" },
  { 284,  HA_ERR_TABLE_DEF_CHANGED, SE, "Table not defined in transaction coordinator" },
  { 285,  DMEC, SE, "Unknown table error in transaction coordinator" },
  { 881,  DMEC, SE, "Unable to create table, out of data pages (increase DataMemory) " },
  { 906,  DMEC, SE, "Unsupported attribute type in index" },
  { 907,  DMEC, SE, "Unsupported character set in table or index" },
  { 908,  DMEC, IS, "Invalid ordered index tree node size" },
  { 1225, DMEC, SE, "Table not defined in local query handler" },
  { 1226, DMEC, SE, "Table is being dropped" },
  { 1228, DMEC, SE, "Cannot use drop table for drop index" },
  { 1229, DMEC, SE, "Too long frm data supplied" },
  { 1231, DMEC, SE, "Invalid table or index to scan" },
  { 1232, DMEC, SE, "Invalid table or index to scan" },

  { 1502, DMEC, IE, "Filegroup already exists" },
  { 1503, DMEC, SE, "Out of filegroup records" },
  { 1504, DMEC, SE, "Out of logbuffer memory" },
  { 1505, DMEC, IE, "Invalid filegroup" },
  { 1506, DMEC, IE, "Invalid filegroup version" },
  { 1507, DMEC, IE, "File no already inuse" },
  { 1508, DMEC, SE, "Out of file records" },
  { 1509, DMEC, SE, "File system error, check if path,permissions etc" },
  { 1510, DMEC, IE, "File meta data error" },
  { 1511, DMEC, IE, "Out of memory" },
  { 1512, DMEC, SE, "File read error" },
  { 1513, DMEC, IE, "Filegroup not online" },
  { 1514, DMEC, SE, "Currently there is a limit of one logfile group" },
  { 1515, DMEC, SE, "Currently there is a 4G limit of one undo/data-file in 32-bit host" },
  
  { 773,  DMEC, SE, "Out of string memory, please modify StringMemory config parameter" },
  { 775,  DMEC, SE, "Create file is not supported when Diskless=1" },
  { 776,  DMEC, AE, "Index created on temporary table must itself be temporary" },
  { 777,  DMEC, AE, "Cannot create a temporary index on a non-temporary table" },
  { 778,  DMEC, AE, "A temporary table or index must be specified as not logging" },
  
  /**
   * FunctionNotImplemented
   */
  { 4003, DMEC, NI, "Function not implemented yet" },

  /**
   * Backup error codes
   */ 

  { 1300, DMEC, IE, "Undefined error" },
  { 1301, DMEC, IE, "Backup issued to not master (reissue command to master)" },
  { 1302, DMEC, IE, "Out of backup record" },
  { 1303, DMEC, IS, "Out of resources" },
  { 1304, DMEC, IE, "Sequence failure" },
  { 1305, DMEC, IE, "Backup definition not implemented" },
  { 1306, DMEC, AE, "Backup not supported in diskless mode (change Diskless)" },

  { 1321, DMEC, UD, "Backup aborted by user request" },
  { 1322, DMEC, IE, "Backup already completed" },
  { 1323, DMEC, IE, "1323" },
  { 1324, DMEC, IE, "Backup log buffer full" },
  { 1325, DMEC, IE, "File or scan error" },
  { 1326, DMEC, IE, "Backup abortet due to node failure" },
  { 1327, DMEC, IE, "1327" },
  
  { 1340, DMEC, IE, "Backup undefined error" },
  { 1342, DMEC, AE, "Backup failed to allocate buffers (check configuration)" },
  { 1343, DMEC, AE, "Backup failed to setup fs buffers (check configuration)" },
  { 1344, DMEC, AE, "Backup failed to allocate tables (check configuration)" },
  { 1345, DMEC, AE, "Backup failed to insert file header (check configuration)" },
  { 1346, DMEC, AE, "Backup failed to insert table list (check configuration)" },
  { 1347, DMEC, AE, "Backup failed to allocate table memory (check configuration)" },
  { 1348, DMEC, AE, "Backup failed to allocate file record (check configuration)" },
  { 1349, DMEC, AE, "Backup failed to allocate attribute record (check configuration)" },
  { 1329, DMEC, AE, "Backup during software upgrade not supported" },

  /**
   * Node id allocation error codes
   */ 

  { 1700, DMEC, IE, "Undefined error" },
  { 1701, DMEC, AE, "Node already reserved" },
  { 1702, DMEC, AE, "Node already connected" },
  { 1703, DMEC, IT, "Node failure handling not completed" },
  { 1704, DMEC, AE, "Node type mismatch" },
  
  /**
   * Still uncategorized
   */
  { 720,  DMEC, AE, "Attribute name reused in table definition" },
  { 1405, DMEC, NR, "Subscriber manager busy with node recovery" },
  { 1407, DMEC, SE, "Subscription not found in subscriber manager" },
  { 1411, DMEC, TR, "Subscriber manager busy with adding/removing a subscriber" },
  { 1412, DMEC, IS, "Can't accept more subscribers, out of space in pool" },
  { 1413, DMEC, TR, "Subscriber manager busy with adding the subscription" },
  { 1414, DMEC, TR, "Subscriber manager has subscribers on this subscription" },
  { 1415, DMEC, SE, "Subscription not unique in subscriber manager" },
  { 1416, DMEC, IS, "Can't accept more subscriptions, out of space in pool" },
  { 1417, DMEC, SE, "Table in suscription not defined, probably dropped" },
  { 1418, DMEC, SE, "Subscription dropped, no new subscribers allowed" },
  { 1419, DMEC, SE, "Subscription already dropped" },

  { 1420, DMEC, TR, "Subscriber manager busy with adding/removing a table" },
  { 1421, DMEC, SE, "Partially connected API in NdbOperation::execute()" },

  { 4004, DMEC, AE, "Attribute name or id not found in the table" },
  
  { 4100, DMEC, AE, "Status Error in NDB" },
  { 4101, DMEC, AE, "No connections to NDB available and connect failed" },
  { 4102, DMEC, AE, "Type in NdbTamper not correct" },
  { 4103, DMEC, AE, "No schema connections to NDB available and connect failed" },
  { 4104, DMEC, AE, "Ndb Init in wrong state, destroy Ndb object and create a new" },
  { 4105, DMEC, AE, "Too many Ndb objects" },
  { 4106, DMEC, AE, "All Not NULL attribute have not been defined" },
  { 4114, DMEC, AE, "Transaction is already completed" },
  { 4116, DMEC, AE, "Operation was not defined correctly, probably missing a key" },
  { 4117, DMEC, AE, "Could not start transporter, configuration error"}, 
  { 4118, DMEC, AE, "Parameter error in API call" },
  { 4300, DMEC, AE, "Tuple Key Type not correct" },
  { 4301, DMEC, AE, "Fragment Type not correct" },
  { 4302, DMEC, AE, "Minimum Load Factor not correct" },
  { 4303, DMEC, AE, "Maximum Load Factor not correct" },
  { 4304, DMEC, AE, "Maximum Load Factor smaller than Minimum" },
  { 4305, DMEC, AE, "K value must currently be set to 6" },
  { 4306, DMEC, AE, "Memory Type not correct" },
  { 4307, DMEC, AE, "Invalid table name" },
  { 4308, DMEC, AE, "Attribute Size not correct" },
  { 4309, DMEC, AE, "Fixed array too large, maximum 64000 bytes" },
  { 4310, DMEC, AE, "Attribute Type not correct" },
  { 4311, DMEC, AE, "Storage Mode not correct" },
  { 4312, DMEC, AE, "Null Attribute Type not correct" },
  { 4313, DMEC, AE, "Index only storage for non-key attribute" },
  { 4314, DMEC, AE, "Storage Type of attribute not correct" },
  { 4315, DMEC, AE, "No more key attributes allowed after defining variable length key attribute" },
  { 4316, DMEC, AE, "Key attributes are not allowed to be NULL attributes" },
  { 4317, DMEC, AE, "Too many primary keys defined in table" },
  { 4318, DMEC, AE, "Invalid attribute name or number" },
  { 4319, DMEC, AE, "createAttribute called at erroneus place" },
  { 4322, DMEC, AE, "Attempt to define distribution key when not prepared to" },
  { 4323, DMEC, AE, "Distribution Key set on table but not defined on first attribute" },
  { 4324, DMEC, AE, "Attempt to define distribution group when not prepared to" },
  { 4325, DMEC, AE, "Distribution Group set on table but not defined on first attribute" },
  { 4326, DMEC, AE, "Distribution Group with erroneus number of bits" },
  { 4327, DMEC, AE, "Distribution Group with 1 byte attribute is not allowed" },
  { 4328, DMEC, AE, "Disk memory attributes not yet supported" },
  { 4329, DMEC, AE, "Variable stored attributes not yet supported" },

  { 4400, DMEC, AE, "Status Error in NdbSchemaCon" },
  { 4401, DMEC, AE, "Only one schema operation per schema transaction" },
  { 4402, DMEC, AE, "No schema operation defined before calling execute" },

  { 4501, DMEC, AE, "Insert in hash table failed when getting table information from Ndb" },
  { 4502, DMEC, AE, "GetValue not allowed in Update operation" },
  { 4503, DMEC, AE, "GetValue not allowed in Insert operation" },
  { 4504, DMEC, AE, "SetValue not allowed in Read operation" },
  { 4505, DMEC, AE, "NULL value not allowed in primary key search" },
  { 4506, DMEC, AE, "Missing getValue/setValue when calling execute" },
  { 4507, DMEC, AE, "Missing operation request when calling execute" },

  { 4200, DMEC, AE, "Status Error when defining an operation" },
  { 4201, DMEC, AE, "Variable Arrays not yet supported" },
  { 4202, DMEC, AE, "Set value on tuple key attribute is not allowed" },
  { 4203, DMEC, AE, "Trying to set a NOT NULL attribute to NULL" },
  { 4204, DMEC, AE, "Set value and Read/Delete Tuple is incompatible" },
  { 4205, DMEC, AE, "No Key attribute used to define tuple" },
  { 4206, DMEC, AE, "Not allowed to equal key attribute twice" },
  { 4207, DMEC, AE, "Key size is limited to 4092 bytes" },
  { 4208, DMEC, AE, "Trying to read a non-stored attribute" },
  { 4209, DMEC, AE, "Length parameter in equal/setValue is incorrect" },
  { 4210, DMEC, AE, "Ndb sent more info than the length he specified" },
  { 4211, DMEC, AE, "Inconsistency in list of NdbRecAttr-objects" },
  { 4212, DMEC, AE, "Ndb reports NULL value on Not NULL attribute" },
  { 4213, DMEC, AE, "Not all data of an attribute has been received" },
  { 4214, DMEC, AE, "Not all attributes have been received" },
  { 4215, DMEC, AE, "More data received than reported in TCKEYCONF message" },
  { 4216, DMEC, AE, "More than 8052 bytes in setValue cannot be handled" },
  { 4217, DMEC, AE, "It is not allowed to increment any other than unsigned ints" },
  { 4218, DMEC, AE, "Currently not allowed to increment NULL-able attributes" },
  { 4219, DMEC, AE, "Maximum size of interpretative attributes are 64 bits" },
  { 4220, DMEC, AE, "Maximum size of interpretative attributes are 64 bits" },
  { 4221, DMEC, AE, "Trying to jump to a non-defined label" },
  { 4222, DMEC, AE, "Label was not found, internal error" },
  { 4223, DMEC, AE, "Not allowed to create jumps to yourself" },
  { 4224, DMEC, AE, "Not allowed to jump to a label in a different subroutine" },
  { 4225, DMEC, AE, "All primary keys defined, call setValue/getValue"},
  { 4226, DMEC, AE, "Bad number when defining a label" },
  { 4227, DMEC, AE, "Bad number when defining a subroutine" },
  { 4228, DMEC, AE, "Illegal interpreter function in scan definition" },
  { 4229, DMEC, AE, "Illegal register in interpreter function definition" },
  { 4230, DMEC, AE, "Illegal state when calling getValue, probably not a read" },
  { 4231, DMEC, AE, "Illegal state when calling interpreter routine" },
  { 4233, DMEC, AE, "Calling execute (synchronous) when already prepared asynchronous transaction exists" },
  { 4234, DMEC, AE, "Illegal to call setValue in this state" },
  { 4235, DMEC, AE, "No callback from execute" },
  { 4236, DMEC, AE, "Trigger name too long" },
  { 4237, DMEC, AE, "Too many triggers" },
  { 4238, DMEC, AE, "Trigger not found" },
  { 4239, DMEC, AE, "Trigger with given name already exists"},
  { 4240, DMEC, AE, "Unsupported trigger type"},
  { 4241, DMEC, AE, "Index name too long" },
  { 4242, DMEC, AE, "Too many indexes" },
  { 4243, DMEC, AE, "Index not found" },
  { 4244, HA_ERR_TABLE_EXIST, OE, "Index or table with given name already exists" },
  { 4247, DMEC, AE, "Illegal index/trigger create/drop/alter request" },
  { 4248, DMEC, AE, "Trigger/index name invalid" },
  { 4249, DMEC, AE, "Invalid table" },
  { 4250, DMEC, AE, "Invalid index type or index logging option" },
  { 4251, HA_ERR_FOUND_DUPP_UNIQUE, AE, "Cannot create unique index, duplicate keys found" },
  { 4252, DMEC, AE, "Failed to allocate space for index" },
  { 4253, DMEC, AE, "Failed to create index table" },
  { 4254, DMEC, AE, "Table not an index table" },
  { 4255, DMEC, AE, "Hash index attributes must be specified in same order as table attributes" },
  { 4258, DMEC, AE, "Cannot create unique index, duplicate attributes found in definition" },
  { 4259, DMEC, AE, "Invalid set of range scan bounds" },
  { 4260, DMEC, UD, "NdbScanFilter: Operator is not defined in NdbScanFilter::Group"},
  { 4261, DMEC, UD, "NdbScanFilter: Column is NULL"},
  { 4262, DMEC, UD, "NdbScanFilter: Condition is out of bounds"},
  { 4263, DMEC, IE, "Invalid blob attributes or invalid blob parts table" },
  { 4264, DMEC, AE, "Invalid usage of blob attribute" },
  { 4265, DMEC, AE, "The method is not valid in current blob state" },
  { 4266, DMEC, AE, "Invalid blob seek position" },
  { 4267, DMEC, IE, "Corrupted blob value" },
  { 4268, DMEC, IE, "Error in blob head update forced rollback of transaction" },
  { 4269, DMEC, IE, "No connection to ndb management server" },
  { 4270, DMEC, IE, "Unknown blob error" },
  { 4335, DMEC, AE, "Only one autoincrement column allowed per table. Having a table without primary key uses an autoincremented hidden key, i.e. a table without a primary key can not have an autoincremented column" },
  { 4271, DMEC, AE, "Invalid index object, not retrieved via getIndex()" },
  { 4272, DMEC, AE, "Table definition has undefined column" },
  { 4273, DMEC, IE, "No blob table in dict cache" },
  { 4274, DMEC, IE, "Corrupted main table PK in blob operation" },
  { 4275, DMEC, AE, "The blob method is incompatible with operation type or lock mode" },
  { 4276, DMEC, AE, "Missing NULL ptr in end of keyData list" },
  { 4277, DMEC, AE, "Key part len is to small for column" },
  { 4278, DMEC, AE, "Supplied buffer to small" },
  { 4279, DMEC, AE, "Malformed string" },
  { 4280, DMEC, AE, "Inconsistent key part length" },
  { 4281, DMEC, AE, "Too many keys specified for key bound in scanIndex" },
  { 4282, DMEC, AE, "range_no not strictly increasing in ordered multi-range index scan" },
  { 4283, DMEC, AE, "key_record in index scan is not an index ndbrecord" },
  { 4284, DMEC, AE, "Cannot mix NdbRecAttr and NdbRecord operations" },
  { 4285, DMEC, AE, "NULL NdbRecord pointer" },
  { 4286, DMEC, AE, "Invalid range_no (must be < 4096)" },
  { 4287, DMEC, AE, "The key_record and attribute_record in primary key operation do not belong to the same table" },
  { 4288, DMEC, AE, "Blob handle for column not available" },
  { 4289, DMEC, AE, "API version mismatch or wrong sizeof(NdbDictionary::RecordSpecification)" },
  { 4290, DMEC, AE, "Missing column specification in NdbDictionary::RecordSpecification" },
  { 4291, DMEC, AE, "Duplicate column specification in NdbDictionary::RecordSpecification" },
  { 4292, DMEC, AE, "NdbRecord for tuple access is not an index key NdbRecord" },
  { 4293, DMEC, AE, "Error returned from application scanIndex() callback" },
  { 4294, DMEC, AE, "Scan filter is too large, discarded" },
  { 2810, DMEC, TR, "No space left on the device" },
  { 2815, DMEC, TR, "Error in reading files, please check file system" },

  { NO_CONTACT_WITH_PROCESS, DMEC, AE,
    "No contact with the process (dead ?)."},
  { WRONG_PROCESS_TYPE, DMEC, AE,
   "The process has wrong type. Expected a DB process."},
  { SEND_OR_RECEIVE_FAILED, DMEC, AE,
    "Send to process or receive failed."},
  { INVALID_ERROR_NUMBER, DMEC, AE,
    "Invalid error number. Should be >= 0."},
  { INVALID_TRACE_NUMBER, DMEC, AE,
    "Invalid trace number."},
  { INVALID_BLOCK_NAME, DMEC, AE,
    "Invalid block name"},
  { NODE_SHUTDOWN_IN_PROGESS, DMEC, AE,
    "Node shutdown in progress" },
  { SYSTEM_SHUTDOWN_IN_PROGRESS, DMEC, AE,
    "System shutdown in progress" },
  { NODE_SHUTDOWN_WOULD_CAUSE_SYSTEM_CRASH, DMEC, AE,
   "Node shutdown would cause system crash" },
  { UNSUPPORTED_NODE_SHUTDOWN, DMEC, AE,
   "Unsupported multi node shutdown. Abort option required." },
  { NODE_NOT_API_NODE, DMEC, AE,
    "The specified node is not an API node." },
  { OPERATION_NOT_ALLOWED_START_STOP, DMEC, AE,
   "Operation not allowed while nodes are starting or stopping."},
  { NO_CONTACT_WITH_DB_NODES, DMEC, AE,
    "No contact with database nodes" }
};

static
const
int NbErrorCodes = sizeof(ErrorCodes)/sizeof(ErrorBundle);

typedef struct ErrorStatusMessage {
  ndberror_status status;
  const char * message;
} ErrorStatusMessage;

typedef struct ErrorStatusClassification {
  ndberror_status status;
  ndberror_classification classification;
  const char * message;
} ErrorStatusClassification;

/**
 * Mapping between classification and status
 */
static
const
ErrorStatusMessage StatusMessageMapping[] = {
  { ST_S, "Success"},
  { ST_P, "Permanent error"},
  { ST_T, "Temporary error"},
  { ST_U ,"Unknown result"}
};

static
const
int NbStatus = sizeof(StatusMessageMapping)/sizeof(ErrorStatusMessage);

static
const
ErrorStatusClassification StatusClassificationMapping[] = {
  { ST_S, NE, "No error"},
  { ST_P, AE, "Application error"},
  { ST_P, CE, "Configuration or application error"},
  { ST_P, ND, "No data found"},
  { ST_P, CV, "Constraint violation"},
  { ST_P, SE, "Schema error"},
  { ST_P, UD, "User defined error"},
  { ST_P, IS, "Insufficient space"},
  
  { ST_T, TR, "Temporary Resource error"},
  { ST_T, NR, "Node Recovery error"},
  { ST_T, OL, "Overload error"},
  { ST_T, TO, "Timeout expired"},
  { ST_T, NS, "Node shutdown"},
  { ST_T, IT, "Internal temporary"},
  
  { ST_U , UR, "Unknown result error"},
  { ST_U , UE, "Unknown error code"},
  
  { ST_P, IE, "Internal error"},
  { ST_P, NI, "Function not implemented"}
};

static
const
int NbClassification = sizeof(StatusClassificationMapping)/sizeof(ErrorStatusClassification);

#ifdef NOT_USED
/**
 * Complete all fields of an NdbError given the error code
 * and details
 */
static
void
set(ndberror_struct * error, int code, const char * details, ...){
  error->code = code;
  {
    va_list ap;
    va_start(ap, details);
    vsnprintf(error->details, sizeof(error->details), details, ap);
    va_end(ap);
  }
}
#endif

void
ndberror_update(ndberror_struct * error){

  int found = 0;
  int i;

  for(i = 0; i<NbErrorCodes; i++){
    if(ErrorCodes[i].code == error->code){
      error->classification = ErrorCodes[i].classification;
      error->message        = ErrorCodes[i].message;
      error->mysql_code     = ErrorCodes[i].mysql_code;
      found = 1;
      break;
    }
  }

  if(!found){
    error->classification = UE;
    error->message        = "Unknown error code";
    error->mysql_code     = DMEC;
  }

  found = 0;
  for(i = 0; i<NbClassification; i++){
    if(StatusClassificationMapping[i].classification == error->classification){
      error->status = StatusClassificationMapping[i].status;
      found = 1;
      break;
    }
  }
  if(!found){
    error->status = ST_U;
  }
}

#if CHECK_ERRORCODES
int
checkErrorCodes(){
  int i, j;
  for(i = 0; i<NbErrorCodes; i++)
    for(j = i+1; j<NbErrorCodes; j++)
      if(ErrorCodes[i].code == ErrorCodes[j].code){
	printf("ErrorCode %d is defined multiple times!!\n", 
		 ErrorCodes[i].code);
	assert(0);
      }
  
  return 1;
}

/*static const int a = checkErrorCodes();*/

int main(void){
  checkErrorCodes();
  return 0;
}
#endif

const char *ndberror_status_message(ndberror_status status)
{
  int i;
  for (i= 0; i < NbStatus; i++)
    if (StatusMessageMapping[i].status == status)
      return StatusMessageMapping[i].message;
  return empty_string;
}

const char *ndberror_classification_message(ndberror_classification classification)
{
  int i;
  for (i= 0; i < NbClassification; i++)
    if (StatusClassificationMapping[i].classification == classification)
      return StatusClassificationMapping[i].message;
  return empty_string;
}

int ndb_error_string(int err_no, char *str, int size)
{
  ndberror_struct error;
  int len;

  assert(size > 1);
  if(size <= 1) 
    return 0;
  error.code = err_no;
  ndberror_update(&error);

  len =
    my_snprintf(str, size-1, "%s: %s: %s", error.message,
		ndberror_status_message(error.status),
		ndberror_classification_message(error.classification));
  str[size-1]= '\0';
  
  if (error.classification != UE)
    return len;
  return -len;
}
