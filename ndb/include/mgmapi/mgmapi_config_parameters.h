#ifndef MGMAPI_CONFIG_PARAMTERS_H
#define MGMAPI_CONFIG_PARAMTERS_H

#define CFG_SYS_NAME                  3
#define CFG_SYS_PRIMARY_MGM_NODE      1
#define CFG_SYS_CONFIG_GENERATION     2
#define CFG_SYS_REPLICATION_ROLE      7
#define CFG_SYS_PORT_BASE             8

#define CFG_NODE_ID                   3
#define CFG_NODE_BYTE_ORDER           4
#define CFG_NODE_HOST                 5
#define CFG_NODE_SYSTEM               6
#define CFG_NODE_DATADIR              7

/**
 * DB config parameters
 */
#define CFG_DB_NO_SAVE_MSGS           100

#define CFG_DB_NO_REPLICAS            101
#define CFG_DB_NO_TABLES              102
#define CFG_DB_NO_ATTRIBUTES          103
#define CFG_DB_NO_INDEXES             104
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

#define CFG_DB_WATCHDOG_INTERVAL      123
#define CFG_DB_STOP_ON_ERROR          124

#define CFG_DB_FILESYSTEM_PATH        125
#define CFG_DB_NO_REDOLOG_FILES       126
#define CFG_DB_DISC_BANDWIDTH         127
#define CFG_DB_SR_DISC_BANDWITH       128

#define CFG_DB_TRANSACTION_CHECK_INTERVAL   129
#define CFG_DB_TRANSACTION_INACTIVE_TIMEOUT 130
#define CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT 131

#define CFG_DB_PARALLEL_BACKUPS           132
#define CFG_DB_BACKUP_MEM                 133
#define CFG_DB_BACKUP_DATA_BUFFER_MEM     134
#define CFG_DB_BACKUP_LOG_BUFFER_MEM      135
#define CFG_DB_BACKUP_WRITE_SIZE          136

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

#define CFG_NODE_ARBIT_RANK           200
#define CFG_NODE_ARBIT_DELAY          201

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
#define CFG_LOGLEVEL_GREP         259
#define CFG_LOGLEVEL_DEBUG        260
#define CFG_LOGLEVEL_BACKUP       261
#define CFG_MAX_LOGLEVEL          261

#define CFG_MGM_PORT                  300

#define CFG_CONNECTION_NODE_1         400
#define CFG_CONNECTION_NODE_2         401
#define CFG_CONNECTION_SEND_SIGNAL_ID 402
#define CFG_CONNECTION_CHECKSUM       403
#define CFG_CONNECTION_NODE_1_SYSTEM  404
#define CFG_CONNECTION_NODE_2_SYSTEM  405
#define CFG_CONNECTION_SERVER_PORT    406
#define CFG_CONNECTION_HOSTNAME_1     407
#define CFG_CONNECTION_HOSTNAME_2     408

#define CFG_TCP_SERVER                452
#define CFG_TCP_SEND_BUFFER_SIZE      454
#define CFG_TCP_RECEIVE_BUFFER_SIZE   455
#define CFG_TCP_PROXY                 456

#define CFG_SHM_SEND_SIGNAL_ID        500
#define CFG_SHM_CHECKSUM              501
#define CFG_SHM_KEY                   502
#define CFG_SHM_BUFFER_MEM            503

#define CFG_SCI_ID_0                  550
#define CFG_SCI_ID_1                  551
#define CFG_SCI_SEND_LIMIT            552
#define CFG_SCI_BUFFER_MEM            553
#define CFG_SCI_NODE1_ADAPTERS        554
#define CFG_SCI_NODE1_ADAPTER0        555
#define CFG_SCI_NODE1_ADAPTER1        556
#define CFG_SCI_NODE2_ADAPTERS        554
#define CFG_SCI_NODE2_ADAPTER0        555
#define CFG_SCI_NODE2_ADAPTER1        556

#define CFG_OSE_PRIO_A_SIZE           602
#define CFG_OSE_PRIO_B_SIZE           603
#define CFG_OSE_RECEIVE_ARRAY_SIZE    604

#define CFG_REP_HEARTBEAT_INTERVAL    700

/**
 * API Config variables
 *
 */
#define CFG_MAX_SCAN_BATCH_SIZE       800
#define CFG_BATCH_BYTE_SIZE           801
#define CFG_BATCH_SIZE                802

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
#define NODE_TYPE_REP                 3
#define NODE_TYPE_EXT_REP             4

#define CONNECTION_TYPE_TCP           0
#define CONNECTION_TYPE_SHM           1
#define CONNECTION_TYPE_SCI           2
#define CONNECTION_TYPE_OSE           3

#endif
