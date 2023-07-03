/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#ifndef MGMAPI_CONFIG_PARAMTERS_H
#define MGMAPI_CONFIG_PARAMTERS_H

#define CFG_SYS_NAME                  3
#define CFG_SYS_PRIMARY_MGM_NODE      1
#define CFG_SYS_CONFIG_GENERATION     2
#define CFG_SYS_PORT_BASE             8

#define CFG_NODE_ID                   3
#define CFG_NODE_BYTE_ORDER           4
#define CFG_NODE_HOST                 5
#define CFG_NODE_SYSTEM               6
#define CFG_NODE_DATADIR              7
#define CFG_TOTAL_SEND_BUFFER_MEMORY  9
#define CFG_LOCATION_DOMAIN_ID        10
#define CFG_NODE_DEDICATED            11

/**
 * DB config parameters
 */
#define CFG_DB_NO_SAVE_MSGS           100

#define CFG_DB_NO_REPLICAS            101
#define CFG_DB_NO_TABLES              102
#define CFG_DB_NO_ATTRIBUTES          103
#define CFG_DB_NO_TRIGGERS            105

#define CFG_DB_NO_TRANSACTIONS        106
#define CFG_DB_NO_OPS                 107
#define CFG_DB_NO_SCANS               108
#define CFG_DB_NO_TRIGGER_OPS         109
#define CFG_DB_NO_INDEX_OPS           110

#define CFG_DB_TRANS_BUFFER_MEM       111
#define CFG_DB_DATA_MEM               112
#define CFG_DB_INDEX_MEM              113
#define CFG_DB_MEMLOCK                114

#define CFG_DB_START_PARTIAL_TIMEOUT   115
#define CFG_DB_START_PARTITION_TIMEOUT 116
#define CFG_DB_START_FAILURE_TIMEOUT   117

#define CFG_DB_HEARTBEAT_INTERVAL     118
#define CFG_DB_API_HEARTBEAT_INTERVAL 119
#define CFG_DB_LCP_INTERVAL           120
#define CFG_DB_GCP_INTERVAL           121
#define CFG_DB_ARBIT_TIMEOUT          122
#define CFG_DB_ARBIT_METHOD           142

#define CFG_DB_WATCHDOG_INTERVAL      123
#define CFG_DB_STOP_ON_ERROR          124

#define CFG_DB_FILESYSTEM_PATH        125
#define CFG_DB_NO_REDOLOG_FILES       126
#define CFG_DB_NO_REDOLOG_PARTS       632
#define CFG_DB_REDOLOG_FILE_SIZE      140

#define CFG_DB_TRANSACTION_CHECK_INTERVAL   129
#define CFG_DB_TRANSACTION_INACTIVE_TIMEOUT 130
#define CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT 131

#define CFG_DB_PARALLEL_BACKUPS           132
#define CFG_DB_BACKUP_MEM                 133
#define CFG_DB_BACKUP_DATA_BUFFER_MEM     134
#define CFG_DB_BACKUP_LOG_BUFFER_MEM      135
#define CFG_DB_BACKUP_WRITE_SIZE          136
#define CFG_DB_BACKUP_MAX_WRITE_SIZE      139

#define CFG_DB_WATCHDOG_INTERVAL_INITIAL  141

#define CFG_LOG_DESTINATION           147

#define CFG_DB_DISCLESS               148

#define CFG_DB_NO_ORDERED_INDEXES     149
#define CFG_DB_NO_UNIQUE_HASH_INDEXES 150
#define CFG_DB_NO_LOCAL_OPS           151
#define CFG_DB_NO_LOCAL_SCANS         152
#define CFG_DB_BATCH_SIZE             153

#define CFG_DB_UNDO_INDEX_BUFFER      154
#define CFG_DB_UNDO_DATA_BUFFER       155
#define CFG_DB_REDO_BUFFER            156

#define CFG_DB_LONG_SIGNAL_BUFFER     157

#define CFG_DB_BACKUP_DATADIR         158

#define CFG_DB_MAX_OPEN_FILES         159
#define CFG_DB_DISK_PAGE_BUFFER_MEMORY 160 /* used from 5.1 */
#define CFG_DB_STRING_MEMORY          161 /* used from 5.1 */
#define CFG_DB_INITIAL_OPEN_FILES     162 /* used from 5.1 */

#define CFG_DB_DISK_PAGE_BUFFER_MEMORY 160
#define CFG_DB_STRING_MEMORY          161

#define CFG_DB_DISK_SYNCH_SIZE        163

#define CFG_DB_MEMREPORT_FREQUENCY    166
#define CFG_DB_BACKUP_REPORT_FREQUENCY    167

#define CFG_DB_O_DIRECT               168

#define CFG_DB_MAX_ALLOCATE           169
#define CFG_DB_MICRO_GCP_INTERVAL     170 /* micro gcp */
#define CFG_DB_MICRO_GCP_TIMEOUT      171

#define CFG_DB_COMPRESSED_BACKUP      172
#define CFG_DB_COMPRESSED_LCP         173

#define CFG_DB_SCHED_EXEC_TIME        174
#define CFG_DB_SCHED_SPIN_TIME        175
#define CFG_DB_REALTIME_SCHEDULER     176
#define CFG_DB_EXECUTE_LOCK_CPU       177
#define CFG_DB_MAINT_LOCK_CPU         178

#define CFG_DB_SUBSCRIPTIONS          179
#define CFG_DB_SUBSCRIBERS            180
#define CFG_DB_SUB_OPERATIONS         181
#define CFG_DB_MAX_BUFFERED_EPOCHS    182
#define CFG_DB_SUMA_HANDOVER_TIMEOUT  183

#define CFG_DB_STARTUP_REPORT_FREQUENCY 184

#define CFG_DB_NODEGROUP              185
#define CFG_DB_MT_THREADS             186
#define CFG_NDBMT_LQH_THREADS         187
#define CFG_NDBMT_LQH_WORKERS         188

#define CFG_DB_INIT_REDO              189
#define CFG_DB_THREAD_POOL            190

#define CFG_NDBMT_CLASSIC             191

#define CFG_DB_DD_FILESYSTEM_PATH     193
#define CFG_DB_DD_DATAFILE_PATH       194
#define CFG_DB_DD_UNDOFILE_PATH       195
#define CFG_DB_DD_LOGFILEGROUP_SPEC   196
#define CFG_DB_DD_TABLEPACE_SPEC      197

#define CFG_DB_SGA                    198 /* super pool mem */
#define CFG_DB_DATA_MEM_2             199 /* used in special build in 5.1 */

#define CFG_DB_LCP_TRY_LOCK_TIMEOUT   605
#define CFG_DB_MT_BUILD_INDEX         606
#define CFG_DB_HB_ORDER               607

#define CFG_DB_DICT_TRACE             608

#define CFG_DB_MAX_START_FAIL         609 /* For StopOnError=0 */
#define CFG_DB_START_FAIL_DELAY_SECS  610 /* For StopOnError=0 */

#define CFG_DB_REDO_OVERCOMMIT_LIMIT  611
#define CFG_DB_REDO_OVERCOMMIT_COUNTER 612

#define CFG_DB_EVENTLOG_BUFFER_SIZE   613
#define CFG_DB_NUMA                   614
#define CFG_DB_LATE_ALLOC             615

#define CFG_DB_2PASS_INR              616
#define CFG_DB_PARALLEL_SCANS_PER_FRAG 617

#define CFG_DB_CONNECT_CHECK_DELAY    618

#define CFG_DB_START_NO_NODEGROUP_TIMEOUT 619

#define CFG_DB_INDEX_STAT_AUTO_CREATE    620
#define CFG_DB_INDEX_STAT_AUTO_UPDATE    621
#define CFG_DB_INDEX_STAT_SAVE_SIZE      622
#define CFG_DB_INDEX_STAT_SAVE_SCALE     623
#define CFG_DB_INDEX_STAT_TRIGGER_PCT    624
#define CFG_DB_INDEX_STAT_TRIGGER_SCALE  625
#define CFG_DB_INDEX_STAT_UPDATE_DELAY   626

#define CFG_DB_MAX_DML_OPERATIONS_PER_TRANSACTION 627
#define CFG_DB_MT_THREAD_CONFIG          628

#define CFG_DB_CRASH_ON_CORRUPTED_TUPLE  629
#define CFG_DB_FREE_PCT                  630

#define CFG_DB_LCP_SCAN_WATCHDOG_LIMIT   631

/* 632 used for CFG_DB_NO_REDOLOG_PARTS */

#define CFG_DB_AT_RESTART_SKIP_INDEXES   633
#define CFG_DB_AT_RESTART_SKIP_FKS       634
#define CFG_DB_SERVER_PORT               635
#define CFG_DB_TCPBIND_INADDR_ANY        636
#define CFG_DB_AT_RESTART_SUBSCRIBER_CONNECT_TIMEOUT 637

#define CFG_DB_MIN_DISK_WRITE_SPEED   638
#define CFG_DB_MAX_DISK_WRITE_SPEED   639
#define CFG_DB_MAX_DISK_WRITE_SPEED_OTHER_NODE_RESTART   640
#define CFG_DB_MAX_DISK_WRITE_SPEED_OWN_RESTART   641

/* <= 641 used */
#define CFG_MIXOLOGY_LEVEL            642
#define CFG_DB_PARALLEL_COPY_THREADS  643
#define CFG_DB_MAX_SEND_DELAY         644
#define CFG_DB_BACKUP_DISK_WRITE_PCT  645

#define CFG_DB_SCHED_RESPONSIVENESS   646

#define CFG_DB_SCHED_SCAN_PRIORITY    647

#define CFG_DB_DISK_DATA_FORMAT       648
#define CFG_DB_ENABLE_PARTIAL_LCP     649
#define CFG_DB_O_DIRECT_SYNC_FLAG     650

#define CFG_DB_UI_BUILD_MAX_BATCHSIZE 651
#define CFG_DB_FK_BUILD_MAX_BATCHSIZE 652
#define CFG_DB_REORG_BUILD_MAX_BATCHSIZE 653
#define CFG_DB_RECOVERY_WORK          654
#define CFG_DB_USE_SHM                655
#define CFG_DB_INSERT_RECOVERY_WORK   656
#define CFG_DB_WATCHDOG_IMMEDIATE_KILL 657
#define CFG_DB_ENABLE_REDO_CONTROL    658
#define CFG_DB_ENABLE_MT_BACKUP       659

#define CFG_DB_RESERVED_INDEX_OPS     660
#define CFG_DB_RESERVED_TRIGGER_OPS   661
#define CFG_DB_RESERVED_OPS           662
#define CFG_DB_RESERVED_LOCAL_SCANS   663
#define CFG_DB_RESERVED_TRANSACTIONS  664
#define CFG_DB_RESERVED_SCANS         665
#define CFG_DB_RESERVED_TRANS_BUFFER_MEM 666

#define CFG_DB_TRANSACTION_MEM        667
#define CFG_DB_MAX_DD_LATENCY         668
#define CFG_DB_DD_USING_SAME_DISK     669
#define CFG_DB_NODE_GROUP_TRANSPORTERS 670
#define CFG_DB_SPIN_METHOD            671
#define CFG_DB_SPIN_TIME_PER_CALL     672

#define CFG_DB_REQUIRE_ENCRYPTED_BACKUP 673

#define CFG_DB_PARTITIONS_PER_NODE    674
#define CFG_DB_CLASSIC_FRAGMENTATION  675

#define CFG_DB_NUM_CPUS               676
#define CFG_DB_AUTO_THREAD_CONFIG     677
#define CFG_DB_TRP_KEEP_ALIVE_SEND_INTERVAL 678

#define CFG_DB_TRANS_ERROR_LOGLEVEL 679
#define CFG_DB_ENCRYPTED_FILE_SYSTEM  680

#define CFG_NODE_ARBIT_RANK           200
#define CFG_NODE_ARBIT_DELAY          201
#define CFG_EXTRA_SEND_BUFFER_MEMORY  203

#define CFG_MGMD_MGMD_HEARTBEAT_INTERVAL 204

#define CFG_DB_DISK_PAGE_BUFFER_ENTRIES 205

#define CFG_DB_GCP_TIMEOUT           206

#define CFG_MIN_LOGLEVEL          250
#define CFG_LOGLEVEL_STARTUP      250
#define CFG_LOGLEVEL_SHUTDOWN     251
#define CFG_LOGLEVEL_STATISTICS   252
#define CFG_LOGLEVEL_CHECKPOINT   253
#define CFG_LOGLEVEL_NODERESTART  254
#define CFG_LOGLEVEL_CONNECTION   255
#define CFG_LOGLEVEL_INFO         256
#define CFG_LOGLEVEL_WARNING      257
#define CFG_LOGLEVEL_ERROR        258
#define CFG_LOGLEVEL_CONGESTION   259
#define CFG_LOGLEVEL_DEBUG        260
#define CFG_LOGLEVEL_BACKUP       261
#define CFG_LOGLEVEL_SCHEMA       262
#define CFG_MAX_LOGLEVEL          262

#define CFG_MGM_PORT                  300

#define CFG_DB_MAX_BUFFERED_EPOCH_BYTES 350

#define CFG_CONNECTION_NODE_1         400
#define CFG_CONNECTION_NODE_2         401
#define CFG_CONNECTION_SEND_SIGNAL_ID 402
#define CFG_CONNECTION_CHECKSUM       403
#define CFG_CONNECTION_NODE_1_SYSTEM  404
#define CFG_CONNECTION_NODE_2_SYSTEM  405
#define CFG_CONNECTION_SERVER_PORT    406
#define CFG_CONNECTION_HOSTNAME_1     407
#define CFG_CONNECTION_HOSTNAME_2     408
#define CFG_CONNECTION_GROUP          409
#define CFG_CONNECTION_NODE_ID_SERVER 410
#define CFG_CONNECTION_OVERLOAD       411
#define CFG_CONNECTION_PRESEND_CHECKSUM 412
#define CFG_CONNECTION_UNRES_HOSTS    413
#define CFG_CONNECTION_PREFER_IP_VER  414

#define CFG_TCP_SERVER                452
#define CFG_TCP_SEND_BUFFER_SIZE      454
#define CFG_TCP_RECEIVE_BUFFER_SIZE   455
#define CFG_TCP_PROXY                 456
#define CFG_TCP_RCV_BUF_SIZE          457
#define CFG_TCP_SND_BUF_SIZE          458
#define CFG_TCP_MAXSEG_SIZE           459
#define CFG_TCP_BIND_INADDR_ANY       460
#define CFG_TCP_SPINTIME              461

#define CFG_SHM_SEND_SIGNAL_ID        500
#define CFG_SHM_CHECKSUM              501
#define CFG_SHM_KEY                   502
#define CFG_SHM_BUFFER_MEM            503
#define CFG_SHM_SIGNUM                504 //Deprecated
#define CFG_SHM_SPINTIME              505
#define CFG_SHM_SEND_BUFFER_SIZE      506

/**
 * No longer used, deprecated
 */
#define CFG_SCI_HOST1_ID_0            550
#define CFG_SCI_HOST1_ID_1            551
#define CFG_SCI_HOST2_ID_0            552
#define CFG_SCI_HOST2_ID_1            553
#define CFG_SCI_SEND_LIMIT            554
#define CFG_SCI_BUFFER_MEM            555

#define CFG_602                       602 /* Removed: was OSE */
#define CFG_603                       603 /* Removed: was OSE */
#define CFG_604                       604 /* Removed: was OSE */

/**
 * API Config variables
 *
 */
#define CFG_MAX_SCAN_BATCH_SIZE       800
#define CFG_BATCH_BYTE_SIZE           801
#define CFG_BATCH_SIZE                802
#define CFG_AUTO_RECONNECT            803
#define CFG_HB_THREAD_PRIO            804
#define CFG_DEFAULT_OPERATION_REDO_PROBLEM_ACTION 805
#define CFG_DEFAULT_HASHMAP_SIZE      806
#define CFG_CONNECT_BACKOFF_MAX_TIME 807
#define CFG_START_CONNECT_BACKOFF_MAX_TIME 808
#define CFG_API_VERBOSE               809

/**
 * Internal
 */
#define CFG_DB_STOP_ON_ERROR_INSERT   1

#define CFG_TYPE_OF_SECTION           999                 
#define CFG_SECTION_SYSTEM            1000
#define CFG_SECTION_NODE              2000
#define CFG_SECTION_CONNECTION        3000

#define NODE_TYPE_DB                  0
#define NODE_TYPE_API                 1
#define NODE_TYPE_MGM                 2

#define CONNECTION_TYPE_TCP           0
#define CONNECTION_TYPE_SHM           1
#define CONNECTION_TYPE_SCI           2 /* Removed */
#define CONNECTION_TYPE_OSE           3 /* Removed. */

#define ARBIT_METHOD_DISABLED         0
#define ARBIT_METHOD_DEFAULT          1
#define ARBIT_METHOD_WAITEXTERNAL     2

#define OPERATION_REDO_PROBLEM_ACTION_ABORT 0
#define OPERATION_REDO_PROBLEM_ACTION_QUEUE 1

#endif
