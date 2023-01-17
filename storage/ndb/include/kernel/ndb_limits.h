/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_LIMITS_H
#define NDB_LIMITS_H

#include "ndb_version.h"  // Limits might depend on NDB version

#define RNIL    0xffffff00

/**
 * Note that actual value = MAX_NODES - 1,
 *  since NodeId = 0 can not be used
 */
#define MAX_NDB_NODES 145
#define MAX_NDB_NODES_v1 49
#define MAX_NDB_NODE_GROUPS 72
#define MAX_NODES     256
#define NDB_UNDEF_NODEGROUP 0xFFFF
#define MAX_BACKUPS   0xFFFFFFFF
#define MAX_INSTANCE_KEYS 1024
#define MAX_NUM_CPUS 2500
#define MAX_USED_NUM_CPUS 1024
#define MAX_QUERY_THREAD_PER_LDM 3
#define MIN_RR_GROUP_SIZE 4
#define MAX_RR_GROUP_SIZE 8

/**************************************************************************
 * IT SHOULD BE (MAX_NDB_NODES - 1).
 * WHEN MAX_NDB_NODE IS CHANGED, IT SHOULD BE CHANGED ALSO
 **************************************************************************/
#define MAX_DATA_NODE_ID 144
/**************************************************************************
 * IT SHOULD BE (MAX_NODES - 1).
 * WHEN MAX_NODES IS CHANGED, IT SHOULD BE CHANGED ALSO
 **************************************************************************/
#define MAX_NODES_ID 255

/**
 * MAX_API_NODES = MAX_NODES - No of NDB Nodes in use
 */

/**
 * The maximum number of replicas in the system
 */
#define MAX_REPLICAS 4

/**
 * The maximum number of transporters allowed
 * A maximum is needed to be able to allocate the array of transporters
 * We need one 
 */
#define MAX_NODE_GROUP_TRANSPORTERS 32
#define MAX_NTRANSPORTERS (MAX_NODES + \
                           ((MAX_REPLICAS - 1) * MAX_NODE_GROUP_TRANSPORTERS))

/**
 * The maximum number of local checkpoints stored at a time
 */
#define MAX_LCP_STORED 3

/**
 * Max LCP used (the reason for keeping MAX_LCP_STORED is that we
 *   need to restore from LCP's with lcp no == 2
 */
#define MAX_LCP_USED 2

/**
 * The maximum number of log execution rounds at system restart
 */
#define MAX_LOG_EXEC 4

/**
 * The maximum number of tuples per page
 **/
#define MAX_TUPLES_PER_PAGE 8191
#define MAX_TUPLES_BITS 13 		/* 13 bits = 8191 tuples per page */
#define NDB_MAX_TABLES 20320                /* SchemaFile.hpp */
#define MAX_TAB_NAME_SIZE 128
#define MAX_ATTR_NAME_SIZE NAME_LEN       /* From mysql_com.h */
#define MAX_ATTR_DEFAULT_VALUE_SIZE ((MAX_TUPLE_SIZE_IN_WORDS + 1) * 4)  //Add 1 word for AttributeHeader
#define MAX_ATTRIBUTES_IN_TABLE 512
#define MAX_ATTRIBUTES_IN_INDEX 32
#define MAX_TUPLE_SIZE_IN_WORDS 7500
#define MAX_KEY_SIZE_IN_WORDS 1023
#define MAX_NULL_BITS 4096

/*
 * Suma block sorts data changes of tables in buckets.
 * Sumas in a node group shares a number of buckets, which is the
 * factorial of the number of replicas, to ensure balance in any
 * node failure situations.
 */
#define MAX_SUMA_BUCKETS_PER_NG     24 /* factorial of MAX_REPLICAS */

/*
 * At any time, one Suma is responsible for streaming bucket data
 * to its subscribers, each bucket uses its own stream aka
 * subscriptions data stream.
 *
 * Note that each subscriber receives filtered data from the
 * stream depending on which objects it subscribes on.
 *
 * A stream sending data from a bucket will have a 16-bit identifier
 * with two parts.  The lower 8 bit determines a non zero stream
 * group.  The upper 8 bit determines an identifier with that group.
 *
 * Stream group identifiers range from 1 to MAX_SUB_DATA_STREAM_GROUPS.
 * Stream identifier within a group range from 0 to MAX_SUB_DATA_STREAMS_PER_GROUP - 1.
 * Stream identifier zero is reserved to not identify any stream.
 */
#define MAX_SUB_DATA_STREAMS (MAX_SUB_DATA_STREAMS_PER_GROUP * MAX_SUB_DATA_STREAM_GROUPS)
#define MAX_SUB_DATA_STREAM_GROUPS      (MAX_NDB_NODES-1)
#define MAX_SUB_DATA_STREAMS_PER_GROUP  (MAX_SUMA_BUCKETS_PER_NG / MAX_REPLICAS)

/*
 * Fragmentation data are Uint16, first two are #replicas,
 * and #fragments, then for each fragment, first log-part-id
 * then nodeid for each replica.
 * See creation in Dbdih::execCREATE_FRAGMENTATION_REQ()
 * and read in Dbdih::execDIADDTABREQ()
 */
#define MAX_FRAGMENT_DATA_ENTRIES (2 + (1 + MAX_REPLICAS) * MAX_NDB_PARTITIONS)
#define MAX_FRAGMENT_DATA_BYTES (2 * MAX_FRAGMENT_DATA_ENTRIES)
#define MAX_FRAGMENT_DATA_WORDS ((MAX_FRAGMENT_DATA_BYTES + 3) / 4)

#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
#define MAX_NDB_PARTITIONS 240
#elif NDB_VERSION_D < NDB_MAKE_VERSION(7,6,8)
#define MAX_NDB_PARTITIONS 2048
#else
#define MAX_NDB_PARTITIONS 8160
#endif

#define NDB_PARTITION_BITS 16
#define NDB_PARTITION_MASK ((Uint32)((1 << NDB_PARTITION_BITS) - 1))

#define MAX_RANGE_DATA (131072+MAX_NDB_PARTITIONS) //0.5 MByte of list data

#define MAX_WORDS_META_FILE 24576

#define MIN_ATTRBUF ((MAX_ATTRIBUTES_IN_TABLE/24) + 1)
/*
 * Max Number of Records to fetch per SCAN_NEXTREQ in a scan in LQH. The
 * API can order a multiple of this number of records at a time since
 * fragments can be scanned in parallel.
 */
#define MAX_PARALLEL_OP_PER_SCAN 992
/*
* The default batch size. Configurable parameter.
*/
#define DEF_BATCH_SIZE 256
/*
* When calculating the number of records sent from LQH in each batch
* one uses SCAN_BATCH_SIZE divided by the expected size of signals
* per row. This gives the batch size used for the scan. The NDB API
* will receive one batch from each node at a time so there has to be
* some care taken also so that the NDB API is not overloaded with
* signals.
* This parameter is configurable, this is the default value.
*/
#define SCAN_BATCH_SIZE 16384
/*
* To protect the NDB API from overload we also define a maximum total
* batch size from all nodes. This parameter should most likely be
* configurable, or dependent on sendBufferSize.
* This parameter is configurable, this is the default value.
*/
#define MAX_SCAN_BATCH_SIZE 262144
/*
 * Maximum number of Parallel Scan queries on one hash index fragment
 */
#define MAX_PARALLEL_SCANS_PER_FRAG 12

/**
 * Computed defines
 */
#define MAXNROFATTRIBUTESINWORDS (MAX_ATTRIBUTES_IN_TABLE / 32)

/*
 * Ordered index constants.  Make configurable per index later.
 */
#define MAX_TTREE_NODE_SIZE 64	    /* total words in node */
#define MAX_TTREE_PREF_SIZE 4	    /* words in min prefix */
#define MAX_TTREE_NODE_SLACK 2	    /* diff between max and min occupancy */

/*
 * Blobs.
 */
#define NDB_BLOB_V1 1
#define NDB_BLOB_V2 2
#define NDB_BLOB_V1_HEAD_SIZE 2     /* sizeof(Uint64) >> 2 */
#define NDB_BLOB_V2_HEAD_SIZE 4     /* 2 + 2 + 4 + 8 bytes, see NdbBlob.hpp */

/*
 * Character sets.
 */
#define MAX_XFRM_MULTIPLY 8         /* max expansion when normalizing */

/**
 * Disk data
 */
#define MAX_FILES_PER_FILEGROUP 1024

/**
 * Page size in global page pool
 */
#define GLOBAL_PAGE_SIZE 32768
#define GLOBAL_PAGE_SIZE_WORDS 8192

/*
 * Schema transactions
 */
#define MAX_SCHEMA_OPERATIONS 256

/*
 * Long signals
 */
#define NDB_SECTION_SEGMENT_SZ 60

/*
 * The maximum size of signal before we split it into a bunch of
 * smaller signals. In words.
 */
#define MAX_SIZE_SINGLE_SIGNAL 7400

/*
 * Restore Buffer in pages
 *   4M
 */
#define LCP_RESTORE_BUFFER (4*32)


/**
 * The hashmap size should support at least one
 * partition per LDM. And also try to make size
 * a multiple of all possible data node counts,
 * so that all partitions are related to the same
 * number of hashmap buckets as possible,
 * otherwise some partitions will be bigger than
 * others.
 *
 * The historical size of hashmaps supported by old
 * versions of NDB is 240.  This guarantees at most
 * 1/6 of unusable data memory for some nodes, since
 * one can have atmost 48 data nodes so each node
 * will relate to at least 5 hashmap buckets.  Also
 * 240 is a multiple of 2, 3, 4, 5, 6, 8, 10, 12,
 * 15, 16, 20, 24, 30, 32, 40, and 48 so having any
 * of these number of nodes guarantees near no
 * unusable data memory.
 *
 * The current value 3840 is 16 times 240, and so gives
 * at least the same guarantees as the old value above,
 * also if up to 16 ldm threads per node is used.
 */

#define NDB_MAX_HASHMAP_BUCKETS (3840 * 2 * 3)
#define NDB_DEFAULT_HASHMAP_MAX_FRAGMENTS 1536

#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
#define NDB_DEFAULT_HASHMAP_BUCKETS 240
#else
#define NDB_DEFAULT_HASHMAP_BUCKETS 3840
#endif

/**
 * Bits/mask used for coding/decoding blockno/blockinstance
 */
#define NDBMT_BLOCK_BITS 9
#define NDBMT_BLOCK_MASK ((1 << NDBMT_BLOCK_BITS) - 1)
#define NDBMT_BLOCK_INSTANCE_BITS 7
#define NDBMT_MAX_INSTANCES 1024

#define NDB_DEFAULT_LOG_PARTS 4

#define NDBMT_MAIN_THREADS         2 /* Without receiver threads */

#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
#define NDB_MAX_LOG_PARTS          4
#define MAX_NDBMT_TC_THREADS       2
#define MAX_NDBMT_RECEIVE_THREADS  1
#define MAX_NDBMT_SEND_THREADS     0
#elif NDB_VERSION_D < NDB_MAKE_VERSION(8,0,23)
#define NDB_MAX_LOG_PARTS         32
#define MAX_NDBMT_TC_THREADS      32
#define MAX_NDBMT_RECEIVE_THREADS 16 
#define MAX_NDBMT_SEND_THREADS    16
#else
#define NDB_MAX_LOG_PARTS         32
#define MAX_NDBMT_TC_THREADS      160
#define MAX_NDBMT_RECEIVE_THREADS 80
#define MAX_NDBMT_SEND_THREADS    80
#endif

#define MAX_NDBMT_LQH_WORKERS 332 
#define MAX_NDBMT_LQH_THREADS 332
#define MAX_NDBMT_QUERY_THREADS 332

#define NDBMT_MAX_BLOCK_INSTANCES (MAX_NDBMT_LQH_THREADS + \
                                   MAX_NDBMT_QUERY_THREADS + \
                                   MAX_NDBMT_TC_THREADS + \
                                   MAX_NDBMT_RECEIVE_THREADS + \
                                   NDBMT_MAIN_THREADS)
/* Proxy block 0 is not a worker */
#define NDBMT_MAX_WORKER_INSTANCES (NDBMT_MAX_BLOCK_INSTANCES - 1)

#define MAX_THREADS_TO_WATCH (MAX_NDBMT_LQH_THREADS + \
                              MAX_NDBMT_QUERY_THREADS + \
                              MAX_NDBMT_TC_THREADS + \
                              MAX_NDBMT_SEND_THREADS + \
                              MAX_NDBMT_RECEIVE_THREADS + \
                              NDBMT_MAIN_THREADS)

#define NDB_FILE_BUFFER_SIZE (256*1024)

/*
 * NDB_FS_RW_PAGES must be big enough for biggest request,
 * probably PACK_TABLE_PAGES (see Dbdih.hpp)
 */
#if NDB_VERSION_D < NDB_MAKE_VERSION(7,2,0)
#define NDB_FS_RW_PAGES 32
#else
#define NDB_FS_RW_PAGES 268 * 4
#endif

/**
 * MAX_ATTRIBUTES_IN_TABLE old handling
 */
#define MAXNROFATTRIBUTESINWORDS_OLD (128 / 32)

/**
 * No of bits available for attribute mask in NDB$EVENTS_0
 */
#define MAX_ATTRIBUTES_IN_TABLE_NDB_EVENTS_0 4096

/**
 * Max treenodes per request SPJ
 *
 * Currently limited by nodemask being shipped back inside 32-bit
 *   word disguised as totalLen in ScanTabConf
 */
#define NDB_SPJ_MAX_TREE_NODES 32

/*
 * Stored ordered index stats uses 2 Longvarbinary pseudo-columns: the
 * packed index keys and the packed values.  Key size is limited by
 * SAMPLES table which has 3 other PK attributes.  Also length bytes is
 * counted as 1 word.  Values currently contain RIR (one word) and RPK
 * (one word for each key level).  The SAMPLEs table STAT_VALUE column
 * is longer to allow future changes.
 *
 * Stats tables are "lifted" to mysql level so for max key size use
 * MAX_KEY_LENGTH/4 instead of the bigger MAX_KEY_SIZE_IN_WORDS.  The
 * definition is not available by default, use 3072 directly now.
 */
#define MAX_INDEX_STAT_KEY_COUNT    MAX_ATTRIBUTES_IN_INDEX
#define MAX_INDEX_STAT_KEY_SIZE     ((3072/4) - 3 - 1)
#define MAX_INDEX_STAT_VALUE_COUNT  (1 + MAX_INDEX_STAT_KEY_COUNT)
#define MAX_INDEX_STAT_VALUE_SIZE   MAX_INDEX_STAT_VALUE_COUNT
#define MAX_INDEX_STAT_VALUE_CSIZE  512 /* Longvarbinary(2048) */
#define MAX_INDEX_STAT_VALUE_FORMAT 1

/**
 * When calculating batch size for unique key builds, reorg builds,
 * and foreign key builds we will treat this as the maximum normal
 * row size, if rows are bigger than this we will decrease the
 * parallelism to adjust for this.
 * See Suma.cpp
 */
#define MAX_NORMAL_ROW_SIZE 2048

/**
 * Maximum size that an EVENT_REP signal can carry in its
 * long signal section.
 */
#define MAX_EVENT_REP_SIZE_WORDS 1024

#define MAX_UNDO_DATA            20 + MAX_TUPLE_SIZE_IN_WORDS
// Max. number of pending undo records allowed per LDM
#define MAX_PENDING_UNDO_RECORDS 100

// Maximum handling of DROP_TRIG_REQs in parallel by LocalProxy
#define NDB_MAX_PROXY_DROP_TRIG_IMPL_REQ 21
/* Maximum number of DROP_TRIGGER_REQs SUMA can send in parallel after the
 * execution of SUB_STOP_REQ.
 *
 * We do not anticipate multiple parallel sub stop reqs from multiple APIs.
 * So, it should be fair to restrict the number of API nodes sending
 * sub stop requests in parallel to 2. Any further sub stop requests from any
 * other API nodes will be delayed. We delay the sub stop requests execution
 * based on outstanding trigger drop requests. Each sub stop request can
 * send a maximum of 3 drop trigger requests. So now a maximum of 6 is
 * allowed to execute in parallel from all api nodes.*/
#define NDB_MAX_SUMA_DROP_TRIG_REQ_SUBSTOP 2 * 3
/* Max DROP_TRIG_REQ allowed from api_fail_subscriber_list
 * This is greater than the maximum requests allowed from SUB_STOP_REQ
 * handling so as to give priority to API failure handling over normal start
 * and stop subscriptions if they both are competing. */
#define NDB_MAX_SUMA_DROP_TRIG_REQ_APIFAIL 3 * 3

// Max. 256 bytes for encryption password given via mgmapi
#define MAX_BACKUP_ENCRYPTION_PASSWORD_LENGTH 256

// Max. 32 bytes for node master key
#define MAX_NODE_MASTER_KEY_LENGTH 64

static_assert(NDB_DEFAULT_HASHMAP_BUCKETS <= NDB_MAX_HASHMAP_BUCKETS);
static_assert(MAX_NDB_PARTITIONS <= NDB_MAX_HASHMAP_BUCKETS);
static_assert(MAX_NDB_PARTITIONS - 1 <= NDB_PARTITION_MASK);

// MAX_NDB_NODES should be 48, but code assumes it is 49
static constexpr Uint32 MAX_NDB_DATA_NODES = MAX_DATA_NODE_ID;
static_assert(MAX_NDB_NODES == MAX_NDB_DATA_NODES + 1);

// Default partitioning is 1 partition per LDM
static_assert(MAX_NDB_DATA_NODES * NDB_MAX_LOG_PARTS <= MAX_NDB_PARTITIONS);

// The default hashmap should at least support the maximum default partitioning
static_assert(MAX_NDB_DATA_NODES * NDB_MAX_LOG_PARTS <= NDB_MAX_HASHMAP_BUCKETS);

#endif
