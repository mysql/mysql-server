/*
  Copyright 2010 Sun Microsystems, Inc.
  Use is subject to license terms.

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
/*
 * MGMAPI.java
 */

package com.mysql.ndbjtie.mgmapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

// MMM! move as static decl into MGMAPI?
/*!public!*/ interface NdbMgmHandleConst
{}
/*!public!*/ class NdbMgmHandle extends Wrapper implements NdbMgmHandleConst
{}
/*!public!*/ interface NdbLogEventHandleConst
{}
/*!public!*/ class NdbLogEventHandle extends Wrapper implements NdbLogEventHandleConst
{}
/*!public!*/ class MGMAPI_ERROR
{
    public interface /*_enum_*/ ndb_mgm_error
    {
        int NDB_MGM_NO_ERROR = 0,
            NDB_MGM_ILLEGAL_CONNECT_STRING = 1001,
            NDB_MGM_ILLEGAL_SERVER_HANDLE = 1005,
            NDB_MGM_ILLEGAL_SERVER_REPLY = 1006,
            NDB_MGM_ILLEGAL_NUMBER_OF_NODES = 1007,
            NDB_MGM_ILLEGAL_NODE_STATUS = 1008,
            NDB_MGM_OUT_OF_MEMORY = 1009,
            NDB_MGM_SERVER_NOT_CONNECTED = 1010,
            NDB_MGM_COULD_NOT_CONNECT_TO_SOCKET = 1011,
            NDB_MGM_BIND_ADDRESS = 1012,
            NDB_MGM_ALLOCID_ERROR = 1101,
            NDB_MGM_ALLOCID_CONFIG_MISMATCH = 1102,
            NDB_MGM_START_FAILED = 2001,
            NDB_MGM_STOP_FAILED = 2002,
            NDB_MGM_RESTART_FAILED = 2003,
            NDB_MGM_COULD_NOT_START_BACKUP = 3001,
            NDB_MGM_COULD_NOT_ABORT_BACKUP = 3002,
            NDB_MGM_COULD_NOT_ENTER_SINGLE_USER_MODE = 4001,
            NDB_MGM_COULD_NOT_EXIT_SINGLE_USER_MODE = 4002,
            NDB_MGM_USAGE_ERROR = 5001;
    }
}
/*!public!*/ interface MGMAPI_CONFIG_PARAMTERS
{
    int CFG_SYS_NAME = 3;
    int CFG_SYS_PRIMARY_MGM_NODE = 1;
    int CFG_SYS_CONFIG_GENERATION = 2;
    int CFG_SYS_PORT_BASE = 8;
    int CFG_NODE_ID = 3;
    int CFG_NODE_BYTE_ORDER = 4;
    int CFG_NODE_HOST = 5;
    int CFG_NODE_SYSTEM = 6;
    int CFG_NODE_DATADIR = 7;
    int CFG_9 = 9;
    int CFG_DB_NO_SAVE_MSGS = 100;
    int CFG_DB_NO_REPLICAS = 101;
    int CFG_DB_NO_TABLES = 102;
    int CFG_DB_NO_ATTRIBUTES = 103;
    int CFG_DB_NO_INDEXES = 104;
    int CFG_DB_NO_TRIGGERS = 105;
    int CFG_DB_NO_TRANSACTIONS = 106;
    int CFG_DB_NO_OPS = 107;
    int CFG_DB_NO_SCANS = 108;
    int CFG_DB_NO_TRIGGER_OPS = 109;
    int CFG_DB_NO_INDEX_OPS = 110;
    int CFG_DB_TRANS_BUFFER_MEM = 111;
    int CFG_DB_DATA_MEM = 112;
    int CFG_DB_INDEX_MEM = 113;
    int CFG_DB_MEMLOCK = 114;
    int CFG_DB_START_PARTIAL_TIMEOUT = 115;
    int CFG_DB_START_PARTITION_TIMEOUT = 116;
    int CFG_DB_START_FAILURE_TIMEOUT = 117;
    int CFG_DB_HEARTBEAT_INTERVAL = 118;
    int CFG_DB_API_HEARTBEAT_INTERVAL = 119;
    int CFG_DB_LCP_INTERVAL = 120;
    int CFG_DB_GCP_INTERVAL = 121;
    int CFG_DB_ARBIT_TIMEOUT = 122;
    int CFG_DB_WATCHDOG_INTERVAL = 123;
    int CFG_DB_STOP_ON_ERROR = 124;
    int CFG_DB_FILESYSTEM_PATH = 125;
    int CFG_DB_NO_REDOLOG_FILES = 126;
    int CFG_DB_REDOLOG_FILE_SIZE = 140;
    int CFG_DB_LCP_DISC_PAGES_TUP = 127;
    int CFG_DB_LCP_DISC_PAGES_TUP_SR = 128;
    int CFG_DB_LCP_DISC_PAGES_ACC = 137;
    int CFG_DB_LCP_DISC_PAGES_ACC_SR = 138;
    int CFG_DB_TRANSACTION_CHECK_INTERVAL = 129;
    int CFG_DB_TRANSACTION_INACTIVE_TIMEOUT = 130;
    int CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT = 131;
    int CFG_DB_PARALLEL_BACKUPS = 132;
    int CFG_DB_BACKUP_MEM = 133;
    int CFG_DB_BACKUP_DATA_BUFFER_MEM = 134;
    int CFG_DB_BACKUP_LOG_BUFFER_MEM = 135;
    int CFG_DB_BACKUP_WRITE_SIZE = 136;
    int CFG_DB_BACKUP_MAX_WRITE_SIZE = 139;
    int CFG_DB_WATCHDOG_INTERVAL_INITIAL = 141;
    int CFG_LOG_DESTINATION = 147;
    int CFG_DB_DISCLESS = 148;
    int CFG_DB_NO_ORDERED_INDEXES = 149;
    int CFG_DB_NO_UNIQUE_HASH_INDEXES = 150;
    int CFG_DB_NO_LOCAL_OPS = 151;
    int CFG_DB_NO_LOCAL_SCANS = 152;
    int CFG_DB_BATCH_SIZE = 153;
    int CFG_DB_UNDO_INDEX_BUFFER = 154;
    int CFG_DB_UNDO_DATA_BUFFER = 155;
    int CFG_DB_REDO_BUFFER = 156;
    int CFG_DB_LONG_SIGNAL_BUFFER = 157;
    int CFG_DB_BACKUP_DATADIR = 158;
    int CFG_DB_MAX_OPEN_FILES = 159;
    int CFG_DB_DISK_PAGE_BUFFER_MEMORY = 160;
    int CFG_DB_STRING_MEMORY = 161;
    int CFG_DB_INITIAL_OPEN_FILES = 162;
    int CFG_DB_DISK_SYNCH_SIZE = 163;
    int CFG_DB_CHECKPOINT_SPEED = 164;
    int CFG_DB_CHECKPOINT_SPEED_RESTART = 165;
    int CFG_DB_MEMREPORT_FREQUENCY = 166;
    int CFG_DB_BACKUP_REPORT_FREQUENCY = 167;
    int CFG_DB_O_DIRECT = 168;
    int CFG_DB_MAX_ALLOCATE = 169;
    int CFG_DB_MICRO_GCP_INTERVAL = 170;
    int CFG_DB_MICRO_GCP_TIMEOUT = 171;
    int CFG_DB_COMPRESSED_BACKUP = 172;
    int CFG_DB_COMPRESSED_LCP = 173;
    int CFG_DB_SCHED_EXEC_TIME = 174;
    int CFG_DB_SCHED_SPIN_TIME = 175;
    int CFG_DB_REALTIME_SCHEDULER = 176;
    int CFG_DB_EXECUTE_LOCK_CPU = 177;
    int CFG_DB_MAINT_LOCK_CPU = 178;
    int CFG_DB_SUBSCRIPTIONS = 179;
    int CFG_DB_SUBSCRIBERS = 180;
    int CFG_DB_SUB_OPERATIONS = 181;
    int CFG_DB_MAX_BUFFERED_EPOCHS = 182;
    int CFG_DB_SUMA_HANDOVER_TIMEOUT = 183;
    int CFG_DB_INIT_REDO = 189;
    int CFG_DB_DD_FILESYSTEM_PATH = 193;
    int CFG_DB_DD_DATAFILE_PATH = 194;
    int CFG_DB_DD_UNDOFILE_PATH = 195;
    int CFG_DB_DD_LOGFILEGROUP_SPEC = 196;
    int CFG_DB_DD_TABLEPACE_SPEC = 197;
    int CFG_DB_SGA = 198;
    int CFG_DB_DATA_MEM_2 = 199;
    int CFG_DB_LCP_TRY_LOCK_TIMEOUT = 605;
    int CFG_NODE_ARBIT_RANK = 200;
    int CFG_NODE_ARBIT_DELAY = 201;
    int CFG_202 = 202;
    int CFG_MIN_LOGLEVEL = 250;
    int CFG_LOGLEVEL_STARTUP = 250;
    int CFG_LOGLEVEL_SHUTDOWN = 251;
    int CFG_LOGLEVEL_STATISTICS = 252;
    int CFG_LOGLEVEL_CHECKPOINT = 253;
    int CFG_LOGLEVEL_NODERESTART = 254;
    int CFG_LOGLEVEL_CONNECTION = 255;
    int CFG_LOGLEVEL_INFO = 256;
    int CFG_LOGLEVEL_WARNING = 257;
    int CFG_LOGLEVEL_ERROR = 258;
    int CFG_LOGLEVEL_CONGESTION = 259;
    int CFG_LOGLEVEL_DEBUG = 260;
    int CFG_LOGLEVEL_BACKUP = 261;
    int CFG_MAX_LOGLEVEL = 261;
    int CFG_MGM_PORT = 300;
    int CFG_CONNECTION_NODE_1 = 400;
    int CFG_CONNECTION_NODE_2 = 401;
    int CFG_CONNECTION_SEND_SIGNAL_ID = 402;
    int CFG_CONNECTION_CHECKSUM = 403;
    int CFG_CONNECTION_NODE_1_SYSTEM = 404;
    int CFG_CONNECTION_NODE_2_SYSTEM = 405;
    int CFG_CONNECTION_SERVER_PORT = 406;
    int CFG_CONNECTION_HOSTNAME_1 = 407;
    int CFG_CONNECTION_HOSTNAME_2 = 408;
    int CFG_CONNECTION_GROUP = 409;
    int CFG_CONNECTION_NODE_ID_SERVER = 410;
    int CFG_411 = 411;
    int CFG_TCP_SERVER = 452;
    int CFG_TCP_SEND_BUFFER_SIZE = 454;
    int CFG_TCP_RECEIVE_BUFFER_SIZE = 455;
    int CFG_TCP_PROXY = 456;
    int CFG_TCP_RCV_BUF_SIZE = 457;
    int CFG_TCP_SND_BUF_SIZE = 458;
    int CFG_TCP_MAXSEG_SIZE = 459;
    int CFG_TCP_BIND_INADDR_ANY = 460;
    int CFG_SHM_SEND_SIGNAL_ID = 500;
    int CFG_SHM_CHECKSUM = 501;
    int CFG_SHM_KEY = 502;
    int CFG_SHM_BUFFER_MEM = 503;
    int CFG_SHM_SIGNUM = 504;
    int CFG_SCI_HOST1_ID_0 = 550;
    int CFG_SCI_HOST1_ID_1 = 551;
    int CFG_SCI_HOST2_ID_0 = 552;
    int CFG_SCI_HOST2_ID_1 = 553;
    int CFG_SCI_SEND_LIMIT = 554;
    int CFG_SCI_BUFFER_MEM = 555;
    int CFG_602 = 602;
    int CFG_603 = 603;
    int CFG_604 = 604;
    int CFG_MAX_SCAN_BATCH_SIZE = 800;
    int CFG_BATCH_BYTE_SIZE = 801;
    int CFG_BATCH_SIZE = 802;
    int CFG_AUTO_RECONNECT = 803;
    int CFG_DB_STOP_ON_ERROR_INSERT = 1;
    int CFG_TYPE_OF_SECTION = 999;
    int CFG_SECTION_SYSTEM = 1000;
    int CFG_SECTION_NODE = 2000;
    int CFG_SECTION_CONNECTION = 3000;
    int NODE_TYPE_DB = 0;
    int NODE_TYPE_API = 1;
    int NODE_TYPE_MGM = 2;
    int CONNECTION_TYPE_TCP = 0;
    int CONNECTION_TYPE_SHM = 1;
    int CONNECTION_TYPE_SCI = 2;
    int CONNECTION_TYPE_OSE = 3;
}
/*!public!*/ class MGMAPI implements MGMAPI_CONFIG_PARAMTERS
{
    static public final int MGM_LOGLEVELS = CFG_MAX_LOGLEVEL - CFG_MIN_LOGLEVEL + 1;
    static public final int NDB_MGM_MAX_LOGLEVEL = 15;
    public interface /*_enum_*/ ndb_mgm_node_type
    {
        int NDB_MGM_NODE_TYPE_UNKNOWN = -1,
            NDB_MGM_NODE_TYPE_API = NODE_TYPE_API,
            NDB_MGM_NODE_TYPE_NDB = NODE_TYPE_DB,
            NDB_MGM_NODE_TYPE_MGM = NODE_TYPE_MGM;
    }
    public interface /*_enum_*/ ndb_mgm_node_status
    {
        int NDB_MGM_NODE_STATUS_UNKNOWN = 0,
            NDB_MGM_NODE_STATUS_NO_CONTACT = 1,
            NDB_MGM_NODE_STATUS_NOT_STARTED = 2,
            NDB_MGM_NODE_STATUS_STARTING = 3,
            NDB_MGM_NODE_STATUS_STARTED = 4,
            NDB_MGM_NODE_STATUS_SHUTTING_DOWN = 5,
            NDB_MGM_NODE_STATUS_RESTARTING = 6,
            NDB_MGM_NODE_STATUS_SINGLEUSER = 7,
            NDB_MGM_NODE_STATUS_RESUME = 8;
    }
    static public class /*_struct_*/ ndb_mgm_node_state {
        int node_id;
        int/*_ndb_mgm_node_type_*/ node_type;
        int/*_ndb_mgm_node_status_*/ node_status;
        int start_phase;
        int dynamic_id;
        int node_group;
        int version;
        int connect_count;
        String/*_char[sizeof("000.000.000.000")+1]_*/ connect_address;
        int mysql_version;
    }
    static public class /*_struct_*/ ndb_mgm_cluster_state {
        int no_of_nodes;
        ndb_mgm_node_state[/*_1_*/] node_states;
    }
    static public class /*_struct_*/ ndb_mgm_reply {
        int return_code;
        String/*_char[256]_*/ message;
    }
    static public class /*_struct_*/ ndb_mgm_severity {
        int/*_ndb_mgm_event_severity_*/ category;
        int/*_unsigned int_*/ value;
    }
    static public class /*_struct_*/ ndb_mgm_loglevel {
        int/*_ndb_mgm_event_category_*/ category;
        int/*_unsigned int_*/ value;
    }
    static public native int ndb_mgm_get_latest_error(NdbMgmHandleConst/*_const ndb_mgm_handle *_*/ handle);
    static public native String/*_const char *_*/ ndb_mgm_get_latest_error_msg(NdbMgmHandleConst/*_const ndb_mgm_handle *_*/ handle);
    static public native String/*_const char *_*/ ndb_mgm_get_latest_error_desc(NdbMgmHandleConst/*_const ndb_mgm_handle *_*/ handle);
    // MMM! static public native void ndb_mgm_set_error_stream(NdbMgmHandle/*_ndb_mgm_handle *_*/ p0, FILE * p1);
    static public native NdbMgmHandle/*_ndb_mgm_handle *_*/ ndb_mgm_create_handle();
    static public native void ndb_mgm_destroy_handle(NdbMgmHandle[]/*_ndb_mgm_handle * *_*/ handle);
    static public native void ndb_mgm_set_name(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, String/*_const char *_*/ name);
    static public native int ndb_mgm_set_ignore_sigpipe(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int val);
    static public native int ndb_mgm_set_connectstring(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, String/*_const char *_*/ connect_string);
    static public native int ndb_mgm_number_of_mgmd_in_connect_string(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle);
    static public native int ndb_mgm_set_configuration_nodeid(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int nodeid);
    static public native int ndb_mgm_set_bindaddress(NdbMgmHandle/*_ndb_mgm_handle *_*/ p0, String/*_const char *_*/ arg);
    static public native String/*_const char *_*/ ndb_mgm_get_connectstring(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, String[]/*_char *_*/ buf, int buf_sz);
    static public native int ndb_mgm_set_connect_timeout(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int/*_unsigned int_*/ seconds);
    static public native int ndb_mgm_set_timeout(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int/*_unsigned int_*/ timeout_ms);
    static public native int ndb_mgm_connect(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int no_retries, int retry_delay_in_seconds, int verbose);
    static public native int ndb_mgm_is_connected(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle);
    static public native int ndb_mgm_disconnect(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle);
    static public native int ndb_mgm_get_configuration_nodeid(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle);
    static public native int ndb_mgm_get_connected_port(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle);
    static public native String/*_const char *_*/ ndb_mgm_get_connected_host(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle);
    static public native String/*_const char *_*/ ndb_mgm_get_connected_bind_address(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle);
    static public native ndb_mgm_cluster_state ndb_mgm_get_status(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle);
    static public native ndb_mgm_cluster_state ndb_mgm_get_status2(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int[]/*_const ndb_mgm_node_type[]_*/ types);
    static public native int ndb_mgm_dump_state(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int nodeId, int[]/*_const int *_*/ args, int num_args, ndb_mgm_reply reply);
    static public native int ndb_mgm_stop(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int no_of_nodes, int[]/*_const int *_*/ node_list);
    static public native int ndb_mgm_stop2(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int no_of_nodes, int[]/*_const int *_*/ node_list, int abort);
    static public native int ndb_mgm_stop3(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int no_of_nodes, int[]/*_const int *_*/ node_list, int abort, int[]/*_int *_*/ disconnect);
    static public native int ndb_mgm_restart(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int no_of_nodes, int[]/*_const int *_*/ node_list);
    static public native int ndb_mgm_restart2(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int no_of_nodes, int[]/*_const int *_*/ node_list, int initial, int nostart, int abort);
    static public native int ndb_mgm_restart3(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int no_of_nodes, int[]/*_const int *_*/ node_list, int initial, int nostart, int abort, int[]/*_int *_*/ disconnect);
    static public native int ndb_mgm_start(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int no_of_nodes, int[]/*_const int *_*/ node_list);
    static public native int ndb_mgm_set_clusterlog_severity_filter(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int/*_ndb_mgm_event_severity_*/ severity, int enable, ndb_mgm_reply reply);
    static public native int ndb_mgm_get_clusterlog_severity_filter(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, ndb_mgm_severity[]/*_ndb_mgm_severity *_*/ severity, int/*_unsigned int_*/ severity_size);
    static public native int ndb_mgm_set_clusterlog_loglevel(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int nodeId, int/*_ndb_mgm_event_category_*/ category, int level, ndb_mgm_reply reply);
    static public native int ndb_mgm_get_clusterlog_loglevel(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, ndb_mgm_loglevel[]/*_ndb_mgm_loglevel *_*/ loglevel, int/*_unsigned int_*/ loglevel_size);
    static public native int ndb_mgm_listen_event(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int[]/*_const int[]_*/ filter);
    static public native NdbLogEventHandle/*_ndb_logevent_handle *_*/ ndb_mgm_create_logevent_handle(NdbMgmHandle/*_ndb_mgm_handle *_*/ p0, int[]/*_const int[]_*/ filter);
    static public native void ndb_mgm_destroy_logevent_handle(NdbLogEventHandle[]/*_ndb_logevent_handle * *_*/ p0);
    static public native int ndb_logevent_get_fd(NdbLogEventHandleConst/*_const ndb_logevent_handle *_*/ p0);
    static public native int ndb_logevent_get_next(NdbLogEventHandleConst/*_const ndb_logevent_handle *_*/ p0, NDB_LOGEVENT.ndb_logevent dst, int/*_unsigned int_*/ timeout_in_milliseconds);
    static public native int ndb_logevent_get_latest_error(NdbLogEventHandleConst/*_const ndb_logevent_handle *_*/ p0);
    static public native String/*_const char *_*/ ndb_logevent_get_latest_error_msg(NdbLogEventHandleConst/*_const ndb_logevent_handle *_*/ p0);
    static public native int ndb_mgm_start_backup(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int wait_completed, int[]/*_unsigned int *_*/ backup_id, ndb_mgm_reply reply);
    static public native int ndb_mgm_start_backup2(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int wait_completed, int[]/*_unsigned int *_*/ backup_id, ndb_mgm_reply reply, int/*_unsigned int_*/ input_backupId);
    static public native int ndb_mgm_abort_backup(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int/*_unsigned int_*/ backup_id, ndb_mgm_reply reply);
    static public native int ndb_mgm_enter_single_user(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, int/*_unsigned int_*/ nodeId, ndb_mgm_reply reply);
    static public native int ndb_mgm_exit_single_user(NdbMgmHandle/*_ndb_mgm_handle *_*/ handle, ndb_mgm_reply reply);
}
/*!public!*/ class NDB_LOGEVENT implements MGMAPI_CONFIG_PARAMTERS
{
    public interface /*_enum_*/ Ndb_logevent_type
    {
        int NDB_LE_ILLEGAL_TYPE = -1,
            NDB_LE_Connected = 0,
            NDB_LE_Disconnected = 1,
            NDB_LE_CommunicationClosed = 2,
            NDB_LE_CommunicationOpened = 3,
            NDB_LE_ConnectedApiVersion = 51,
            NDB_LE_GlobalCheckpointStarted = 4,
            NDB_LE_GlobalCheckpointCompleted = 5,
            NDB_LE_LocalCheckpointStarted = 6,
            NDB_LE_LocalCheckpointCompleted = 7,
            NDB_LE_LCPStoppedInCalcKeepGci = 8,
            NDB_LE_LCPFragmentCompleted = 9,
            NDB_LE_NDBStartStarted = 10,
            NDB_LE_NDBStartCompleted = 11,
            NDB_LE_STTORRYRecieved = 12,
            NDB_LE_StartPhaseCompleted = 13,
            NDB_LE_CM_REGCONF = 14,
            NDB_LE_CM_REGREF = 15,
            NDB_LE_FIND_NEIGHBOURS = 16,
            NDB_LE_NDBStopStarted = 17,
            NDB_LE_NDBStopCompleted = 53,
            NDB_LE_NDBStopForced = 59,
            NDB_LE_NDBStopAborted = 18,
            NDB_LE_StartREDOLog = 19,
            NDB_LE_StartLog = 20,
            NDB_LE_UNDORecordsExecuted = 21,
            NDB_LE_NR_CopyDict = 22,
            NDB_LE_NR_CopyDistr = 23,
            NDB_LE_NR_CopyFragsStarted = 24,
            NDB_LE_NR_CopyFragDone = 25,
            NDB_LE_NR_CopyFragsCompleted = 26,
            NDB_LE_NodeFailCompleted = 27,
            NDB_LE_NODE_FAILREP = 28,
            NDB_LE_ArbitState = 29,
            NDB_LE_ArbitResult = 30,
            NDB_LE_GCP_TakeoverStarted = 31,
            NDB_LE_GCP_TakeoverCompleted = 32,
            NDB_LE_LCP_TakeoverStarted = 33,
            NDB_LE_LCP_TakeoverCompleted = 34,
            NDB_LE_TransReportCounters = 35,
            NDB_LE_OperationReportCounters = 36,
            NDB_LE_TableCreated = 37,
            NDB_LE_UndoLogBlocked = 38,
            NDB_LE_JobStatistic = 39,
            NDB_LE_SendBytesStatistic = 40,
            NDB_LE_ReceiveBytesStatistic = 41,
            NDB_LE_MemoryUsage = 50,
            NDB_LE_ThreadConfigLoop = 68,
            NDB_LE_TransporterError = 42,
            NDB_LE_TransporterWarning = 43,
            NDB_LE_MissedHeartbeat = 44,
            NDB_LE_DeadDueToHeartbeat = 45,
            NDB_LE_WarningEvent = 46,
            NDB_LE_SentHeartbeat = 47,
            NDB_LE_CreateLogBytes = 48,
            NDB_LE_InfoEvent = 49,
            NDB_LE_SingleUser = 52,
            NDB_LE_BackupStarted = 54,
            NDB_LE_BackupFailedToStart = 55,
            NDB_LE_BackupStatus = 62,
            NDB_LE_BackupCompleted = 56,
            NDB_LE_BackupAborted = 57,
            NDB_LE_RestoreMetaData = 63,
            NDB_LE_RestoreData = 64,
            NDB_LE_RestoreLog = 65,
            NDB_LE_RestoreStarted = 66,
            NDB_LE_RestoreCompleted = 67,
            NDB_LE_EventBufferStatus = 58,
            NDB_LE_StartReport = 60,
            NDB_LE_SubscriptionStatus = 69,
            NDB_LE_RedoStatus = 73;
    }
    public interface /*_enum_*/ ndb_mgm_event_severity
    {
        int NDB_MGM_ILLEGAL_EVENT_SEVERITY = -1,
            NDB_MGM_EVENT_SEVERITY_ON = 0,
            NDB_MGM_EVENT_SEVERITY_DEBUG = 1,
            NDB_MGM_EVENT_SEVERITY_INFO = 2,
            NDB_MGM_EVENT_SEVERITY_WARNING = 3,
            NDB_MGM_EVENT_SEVERITY_ERROR = 4,
            NDB_MGM_EVENT_SEVERITY_CRITICAL = 5,
            NDB_MGM_EVENT_SEVERITY_ALERT = 6,
            NDB_MGM_EVENT_SEVERITY_ALL = 7;
    }
    public interface /*_enum_*/ ndb_mgm_event_category
    {
        int NDB_MGM_ILLEGAL_EVENT_CATEGORY = -1,
            NDB_MGM_EVENT_CATEGORY_STARTUP = CFG_LOGLEVEL_STARTUP,
            NDB_MGM_EVENT_CATEGORY_SHUTDOWN = CFG_LOGLEVEL_SHUTDOWN,
            NDB_MGM_EVENT_CATEGORY_STATISTIC = CFG_LOGLEVEL_STATISTICS,
            NDB_MGM_EVENT_CATEGORY_CHECKPOINT = CFG_LOGLEVEL_CHECKPOINT,
            NDB_MGM_EVENT_CATEGORY_NODE_RESTART = CFG_LOGLEVEL_NODERESTART,
            NDB_MGM_EVENT_CATEGORY_CONNECTION = CFG_LOGLEVEL_CONNECTION,
            NDB_MGM_EVENT_CATEGORY_BACKUP = CFG_LOGLEVEL_BACKUP,
            NDB_MGM_EVENT_CATEGORY_CONGESTION = CFG_LOGLEVEL_CONGESTION,
            NDB_MGM_EVENT_CATEGORY_INFO = CFG_LOGLEVEL_INFO,
            NDB_MGM_EVENT_CATEGORY_ERROR = CFG_LOGLEVEL_ERROR;
    }
    static public class /*_struct_*/ ndb_logevent {
        NdbLogEventHandle/*_void *_*/ handle;
        int/*_Ndb_logevent_type_*/ type;
        int/*_unsigned int_*/ time;
        int/*_ndb_mgm_event_category_*/ category;
        int/*_ndb_mgm_event_severity_*/ severity;
        int/*_unsigned int_*/ level;
        int/*_unsigned int_*/ source_nodeid;
        // MMM union {
        Connected _Connected;
        Disconnected _Disconnected;
        CommunicationClosed _CommunicationClosed;
        CommunicationOpened _CommunicationOpened;
        ConnectedApiVersion _ConnectedApiVersion;
        GlobalCheckpointStarted _GlobalCheckpointStarted;
        GlobalCheckpointCompleted _GlobalCheckpointCompleted;
        LocalCheckpointStarted _LocalCheckpointStarted;
        LocalCheckpointCompleted _LocalCheckpointCompleted;
        LCPStoppedInCalcKeepGci _LCPStoppedInCalcKeepGci;
        LCPFragmentCompleted _LCPFragmentCompleted;
        UndoLogBlocked _UndoLogBlocked;
        NDBStartStarted _NDBStartStarted;
        NDBStartCompleted _NDBStartCompleted;
        STTORRYRecieved _STTORRYRecieved;
        StartPhaseCompleted _StartPhaseCompleted;
        CM_REGCONF _CM_REGCONF;
        CM_REGREF _CM_REGREF;
        FIND_NEIGHBOURS _FIND_NEIGHBOURS;
        NDBStopStarted _NDBStopStarted;
        NDBStopCompleted _NDBStopCompleted;
        NDBStopForced _NDBStopForced;
        NDBStopAborted _NDBStopAborted;
        StartREDOLog _StartREDOLog;
        StartLog _StartLog;
        UNDORecordsExecuted _UNDORecordsExecuted;
        NR_CopyDict _NR_CopyDict;
        NR_CopyDistr _NR_CopyDistr;
        NR_CopyFragsStarted _NR_CopyFragsStarted;
        NR_CopyFragDone _NR_CopyFragDone;
        NR_CopyFragsCompleted _NR_CopyFragsCompleted;
        NodeFailCompleted _NodeFailCompleted;
        NODE_FAILREP _NODE_FAILREP;
        ArbitState _ArbitState;
        ArbitResult _ArbitResult;
        GCP_TakeoverStarted _GCP_TakeoverStarted;
        GCP_TakeoverCompleted _GCP_TakeoverCompleted;
        LCP_TakeoverStarted _LCP_TakeoverStarted;
        LCP_TakeoverCompleted _LCP_TakeoverCompleted;
        TransReportCounters _TransReportCounters;
        OperationReportCounters _OperationReportCounters;
        TableCreated _TableCreated;
        JobStatistic _JobStatistic;
        SendBytesStatistic _SendBytesStatistic;
        ReceiveBytesStatistic _ReceiveBytesStatistic;
        MemoryUsage _MemoryUsage;
        TransporterError _TransporterError;
        TransporterWarning _TransporterWarning;
        MissedHeartbeat _MissedHeartbeat;
        DeadDueToHeartbeat _DeadDueToHeartbeat;
        WarningEvent _WarningEvent;
        SentHeartbeat _SentHeartbeat;
        CreateLogBytes _CreateLogBytes;
        InfoEvent _InfoEvent;
        EventBufferStatus _EventBufferStatus;
        BackupStarted _BackupStarted;
        BackupFailedToStart _BackupFailedToStart;
        BackupCompleted _BackupCompleted;
        BackupStatus _BackupStatus;
        BackupAborted _BackupAborted;
        RestoreStarted _RestoreStarted;
        RestoreMetaData _RestoreMetaData;
        RestoreData _RestoreData;
        RestoreLog _RestoreLog;
        RestoreCompleted _RestoreCompleted;
        SingleUser _SingleUser;
        StartReport _StartReport;
        SubscriptionStatus _SubscriptionStatus;
        RedoStatus _RedoStatus;
        // MMM }
        static public class /*_struct_*/ Connected {
            int/*_unsigned int_*/ node;
        };
        static public class /*_struct_*/ Disconnected {
            int/*_unsigned int_*/ node;
        };
        static public class /*_struct_*/ CommunicationClosed {
            int/*_unsigned int_*/ node;
        };
        static public class /*_struct_*/ CommunicationOpened {
            int/*_unsigned int_*/ node;
        };
        static public class /*_struct_*/ ConnectedApiVersion {
            int/*_unsigned int_*/ node;
            int/*_unsigned int_*/ version;
        };
        static public class /*_struct_*/ GlobalCheckpointStarted {
            int/*_unsigned int_*/ gci;
        };
        static public class /*_struct_*/ GlobalCheckpointCompleted {
            int/*_unsigned int_*/ gci;
        };
        static public class /*_struct_*/ LocalCheckpointStarted {
            int/*_unsigned int_*/ lci;
            int/*_unsigned int_*/ keep_gci;
            int/*_unsigned int_*/ restore_gci;
        };
        static public class /*_struct_*/ LocalCheckpointCompleted {
            int/*_unsigned int_*/ lci;
        };
        static public class /*_struct_*/ LCPStoppedInCalcKeepGci {
            int/*_unsigned int_*/ data;
        };
        static public class /*_struct_*/ LCPFragmentCompleted {
            int/*_unsigned int_*/ node;
            int/*_unsigned int_*/ table_id;
            int/*_unsigned int_*/ fragment_id;
        };
        static public class /*_struct_*/ UndoLogBlocked {
            int/*_unsigned int_*/ acc_count;
            int/*_unsigned int_*/ tup_count;
        };
        static public class /*_struct_*/ NDBStartStarted {
            int/*_unsigned int_*/ version;
        };
        static public class /*_struct_*/ NDBStartCompleted {
            int/*_unsigned int_*/ version;
        };
        static public class /*_struct_*/ STTORRYRecieved {
            int/*_unsigned int_*/ _todo;
        };
        static public class /*_struct_*/ StartPhaseCompleted {
            int/*_unsigned int_*/ phase;
            int/*_unsigned int_*/ starttype;
        };
        static public class /*_struct_*/ CM_REGCONF {
            int/*_unsigned int_*/ own_id;
            int/*_unsigned int_*/ president_id;
            int/*_unsigned int_*/ dynamic_id;
        };
        static public class /*_struct_*/ CM_REGREF {
            int/*_unsigned int_*/ own_id;
            int/*_unsigned int_*/ other_id;
            int/*_unsigned int_*/ cause;
        };
        static public class /*_struct_*/ FIND_NEIGHBOURS {
            int/*_unsigned int_*/ own_id;
            int/*_unsigned int_*/ left_id;
            int/*_unsigned int_*/ right_id;
            int/*_unsigned int_*/ dynamic_id;
        };
        static public class /*_struct_*/ NDBStopStarted {
            int/*_unsigned int_*/ stoptype;
        };
        static public class /*_struct_*/ NDBStopCompleted {
            int/*_unsigned int_*/ action;
            int/*_unsigned int_*/ signum;
        };
        static public class /*_struct_*/ NDBStopForced {
            int/*_unsigned int_*/ action;
            int/*_unsigned int_*/ signum;
            int/*_unsigned int_*/ error;
            int/*_unsigned int_*/ sphase;
            int/*_unsigned int_*/ extra;
        };
        static public class /*_struct_*/ NDBStopAborted {
            int/*_unsigned int_*/ _todo;
        };
        static public class /*_struct_*/ StartREDOLog {
            int/*_unsigned int_*/ node;
            int/*_unsigned int_*/ keep_gci;
            int/*_unsigned int_*/ completed_gci;
            int/*_unsigned int_*/ restorable_gci;
        };
        static public class /*_struct_*/ StartLog {
            int/*_unsigned int_*/ log_part;
            int/*_unsigned int_*/ start_mb;
            int/*_unsigned int_*/ stop_mb;
            int/*_unsigned int_*/ gci;
        };
        static public class /*_struct_*/ UNDORecordsExecuted {
            int/*_unsigned int_*/ block;
            int/*_unsigned int_*/ data1;
            int/*_unsigned int_*/ data2;
            int/*_unsigned int_*/ data3;
            int/*_unsigned int_*/ data4;
            int/*_unsigned int_*/ data5;
            int/*_unsigned int_*/ data6;
            int/*_unsigned int_*/ data7;
            int/*_unsigned int_*/ data8;
            int/*_unsigned int_*/ data9;
            int/*_unsigned int_*/ data10;
        };
        static public class /*_struct_*/ NR_CopyDict {
            int/*_unsigned int_*/ _todo;
        };
        static public class /*_struct_*/ NR_CopyDistr {
            int/*_unsigned int_*/ _todo;
        };
        static public class /*_struct_*/ NR_CopyFragsStarted {
            int/*_unsigned int_*/ dest_node;
        };
        static public class /*_struct_*/ NR_CopyFragDone {
            int/*_unsigned int_*/ dest_node;
            int/*_unsigned int_*/ table_id;
            int/*_unsigned int_*/ fragment_id;
        };
        static public class /*_struct_*/ NR_CopyFragsCompleted {
            int/*_unsigned int_*/ dest_node;
        };
        static public class /*_struct_*/ NodeFailCompleted {
            int/*_unsigned int_*/ block;
            int/*_unsigned int_*/ failed_node;
            int/*_unsigned int_*/ completing_node;
        };
        static public class /*_struct_*/ NODE_FAILREP {
            int/*_unsigned int_*/ failed_node;
            int/*_unsigned int_*/ failure_state;
        };
        static public class /*_struct_*/ ArbitState {
            int/*_unsigned int_*/ code;
            int/*_unsigned int_*/ arbit_node;
            int/*_unsigned int_*/ ticket_0;
            int/*_unsigned int_*/ ticket_1;
        };
        static public class /*_struct_*/ ArbitResult {
            int/*_unsigned int_*/ code;
            int/*_unsigned int_*/ arbit_node;
            int/*_unsigned int_*/ ticket_0;
            int/*_unsigned int_*/ ticket_1;
        };
        static public class /*_struct_*/ GCP_TakeoverStarted {
            int/*_unsigned int_*/ _todo;
        };
        static public class /*_struct_*/ GCP_TakeoverCompleted {
            int/*_unsigned int_*/ _todo;
        };
        static public class /*_struct_*/ LCP_TakeoverStarted {
            int/*_unsigned int_*/ _todo;
        };
        static public class /*_struct_*/ LCP_TakeoverCompleted {
            int/*_unsigned int_*/ state;
        };
        static public class /*_struct_*/ TransReportCounters {
            int/*_unsigned int_*/ trans_count;
            int/*_unsigned int_*/ commit_count;
            int/*_unsigned int_*/ read_count;
            int/*_unsigned int_*/ simple_read_count;
            int/*_unsigned int_*/ write_count;
            int/*_unsigned int_*/ attrinfo_count;
            int/*_unsigned int_*/ conc_op_count;
            int/*_unsigned int_*/ abort_count;
            int/*_unsigned int_*/ scan_count;
            int/*_unsigned int_*/ range_scan_count;
        };
        static public class /*_struct_*/ OperationReportCounters {
            int/*_unsigned int_*/ ops;
        };
        static public class /*_struct_*/ TableCreated {
            int/*_unsigned int_*/ table_id;
        };
        static public class /*_struct_*/ JobStatistic {
            int/*_unsigned int_*/ mean_loop_count;
        };
        static public class /*_struct_*/ SendBytesStatistic {
            int/*_unsigned int_*/ to_node;
            int/*_unsigned int_*/ mean_sent_bytes;
        };
        static public class /*_struct_*/ ReceiveBytesStatistic {
            int/*_unsigned int_*/ from_node;
            int/*_unsigned int_*/ mean_received_bytes;
        };
        static public class /*_struct_*/ MemoryUsage {
            int gth;
            int/*_unsigned int_*/ page_size_bytes;
            int/*_unsigned int_*/ pages_used;
            int/*_unsigned int_*/ pages_total;
            int/*_unsigned int_*/ block;
        };
        static public class /*_struct_*/ TransporterError {
            int/*_unsigned int_*/ to_node;
            int/*_unsigned int_*/ code;
        };
        static public class /*_struct_*/ TransporterWarning {
            int/*_unsigned int_*/ to_node;
            int/*_unsigned int_*/ code;
        };
        static public class /*_struct_*/ MissedHeartbeat {
            int/*_unsigned int_*/ node;
            int/*_unsigned int_*/ count;
        };
        static public class /*_struct_*/ DeadDueToHeartbeat {
            int/*_unsigned int_*/ node;
        };
        static public class /*_struct_*/ WarningEvent {
            int/*_unsigned int_*/ _todo;
        };
        static public class /*_struct_*/ SentHeartbeat {
            int/*_unsigned int_*/ node;
        };
        static public class /*_struct_*/ CreateLogBytes {
            int/*_unsigned int_*/ node;
        };
        static public class /*_struct_*/ InfoEvent {
            int/*_unsigned int_*/ _todo;
        };
        static public class /*_struct_*/ EventBufferStatus {
            int/*_unsigned int_*/ usage;
            int/*_unsigned int_*/ alloc;
            int/*_unsigned int_*/ max;
            int/*_unsigned int_*/ apply_gci_l;
            int/*_unsigned int_*/ apply_gci_h;
            int/*_unsigned int_*/ latest_gci_l;
            int/*_unsigned int_*/ latest_gci_h;
        };
        static public class /*_struct_*/ BackupStarted {
            int/*_unsigned int_*/ starting_node;
            int/*_unsigned int_*/ backup_id;
        };
        static public class /*_struct_*/ BackupFailedToStart {
            int/*_unsigned int_*/ starting_node;
            int/*_unsigned int_*/ error;
        };
        static public class /*_struct_*/ BackupCompleted {
            int/*_unsigned int_*/ starting_node;
            int/*_unsigned int_*/ backup_id;
            int/*_unsigned int_*/ start_gci;
            int/*_unsigned int_*/ stop_gci;
            int/*_unsigned int_*/ n_records;
            int/*_unsigned int_*/ n_log_records;
            int/*_unsigned int_*/ n_bytes;
            int/*_unsigned int_*/ n_log_bytes;
            int/*_unsigned int_*/ n_records_hi;
            int/*_unsigned int_*/ n_log_records_hi;
            int/*_unsigned int_*/ n_bytes_hi;
            int/*_unsigned int_*/ n_log_bytes_hi;
        };
        static public class /*_struct_*/ BackupStatus {
            int/*_unsigned int_*/ starting_node;
            int/*_unsigned int_*/ backup_id;
            int/*_unsigned int_*/ n_records_lo;
            int/*_unsigned int_*/ n_records_hi;
            int/*_unsigned int_*/ n_log_records_lo;
            int/*_unsigned int_*/ n_log_records_hi;
            int/*_unsigned int_*/ n_bytes_lo;
            int/*_unsigned int_*/ n_bytes_hi;
            int/*_unsigned int_*/ n_log_bytes_lo;
            int/*_unsigned int_*/ n_log_bytes_hi;
        };
        static public class /*_struct_*/ BackupAborted {
            int/*_unsigned int_*/ starting_node;
            int/*_unsigned int_*/ backup_id;
            int/*_unsigned int_*/ error;
        };
        static public class /*_struct_*/ RestoreStarted {
            int/*_unsigned int_*/ backup_id;
            int/*_unsigned int_*/ node_id;
        };
        static public class /*_struct_*/ RestoreMetaData {
            int/*_unsigned int_*/ backup_id;
            int/*_unsigned int_*/ node_id;
            int/*_unsigned int_*/ n_tables;
            int/*_unsigned int_*/ n_tablespaces;
            int/*_unsigned int_*/ n_logfilegroups;
            int/*_unsigned int_*/ n_datafiles;
            int/*_unsigned int_*/ n_undofiles;
        };
        static public class /*_struct_*/ RestoreData {
            int/*_unsigned int_*/ backup_id;
            int/*_unsigned int_*/ node_id;
            int/*_unsigned int_*/ n_records_lo;
            int/*_unsigned int_*/ n_records_hi;
            int/*_unsigned int_*/ n_bytes_lo;
            int/*_unsigned int_*/ n_bytes_hi;
        };
        static public class /*_struct_*/ RestoreLog {
            int/*_unsigned int_*/ backup_id;
            int/*_unsigned int_*/ node_id;
            int/*_unsigned int_*/ n_records_lo;
            int/*_unsigned int_*/ n_records_hi;
            int/*_unsigned int_*/ n_bytes_lo;
            int/*_unsigned int_*/ n_bytes_hi;
        };
        static public class /*_struct_*/ RestoreCompleted {
            int/*_unsigned int_*/ backup_id;
            int/*_unsigned int_*/ node_id;
        };
        static public class /*_struct_*/ SingleUser {
            int/*_unsigned int_*/ type;
            int/*_unsigned int_*/ node_id;
        };
        static public class /*_struct_*/ StartReport {
            int/*_unsigned int_*/ report_type;
            int/*_unsigned int_*/ remaining_time;
            int/*_unsigned int_*/ bitmask_size;
            int[]/*_unsigned int[1]_*/ bitmask_data;
        };
        static public class /*_struct_*/ SubscriptionStatus {
            int/*_unsigned int_*/ report_type;
            int/*_unsigned int_*/ node_id;
        };
        static public class /*_struct_*/ RedoStatus {
            int/*_unsigned int_*/ log_part;
            int/*_unsigned int_*/ head_file_no;
            int/*_unsigned int_*/ head_mbyte;
            int/*_unsigned int_*/ tail_file_no;
            int/*_unsigned int_*/ tail_mbyte;
            int/*_unsigned int_*/ total_hi;
            int/*_unsigned int_*/ total_lo;
            int/*_unsigned int_*/ free_hi;
            int/*_unsigned int_*/ free_lo;
        };
    }
    public interface /*_enum_*/ ndb_logevent_handle_error
    {
        int NDB_LEH_NO_ERROR = 0,
            NDB_LEH_READ_ERROR = 1,
            NDB_LEH_MISSING_EVENT_SPECIFIER = 2,
            NDB_LEH_UNKNOWN_EVENT_TYPE = 3,
            NDB_LEH_UNKNOWN_EVENT_VARIABLE = 4,
            NDB_LEH_INTERNAL_ERROR = 5;
    }
}
