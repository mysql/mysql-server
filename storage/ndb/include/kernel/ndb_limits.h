/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_LIMITS_H
#define NDB_LIMITS_H

#define RNIL    0xffffff00

/**
 * Note that actual value = MAX_NODES - 1,
 *  since NodeId = 0 can not be used
 */
#define MAX_NDB_NODES 49
#define MAX_NODES     256
#define NDB_UNDEF_NODEGROUP 0xFFFF
#define MAX_BACKUPS   0xFFFFFFFF

/**************************************************************************
 * IT SHOULD BE (MAX_NDB_NODES - 1).
 * WHEN MAX_NDB_NODE IS CHANGED, IT SHOULD BE CHANGED ALSO
 **************************************************************************/
#define MAX_DATA_NODE_ID 48
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
#define MAX_TUPLE_SIZE_IN_WORDS 3500

/**
 * When sending a SUB_TABLE_DATA from SUMA to API
 *
 */
#define MAX_SUMA_MESSAGE_IN_WORDS 8028

/**
 * When sending a SUB_TABLE_DATA
 *  this is is the maximum size that it can become
 */
#define CHECK_SUMA_MESSAGE_SIZE(NO_KEYS,KEY_SIZE_IN_WORDS,NO_COLUMNS,TUPLE_SIZE_IN_WORDS) \
  ((NO_KEYS + KEY_SIZE_IN_WORDS + 2 * (NO_COLUMNS + TUPLE_SIZE_IN_WORDS)) <= MAX_SUMA_MESSAGE_IN_WORDS)

#define MAX_KEY_SIZE_IN_WORDS 1023
#define MAX_FRM_DATA_SIZE 6000
#define MAX_NULL_BITS 4096
#define MAX_FRAGMENT_DATA_BYTES (4+(2 * 8 * MAX_REPLICAS * MAX_NDB_NODES))
#define MAX_NDB_PARTITIONS 1024
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
#define DEF_BATCH_SIZE 64
/*
* When calculating the number of records sent from LQH in each batch
* one uses SCAN_BATCH_SIZE divided by the expected size of signals
* per row. This gives the batch size used for the scan. The NDB API
* will receive one batch from each node at a time so there has to be
* some care taken also so that the NDB API is not overloaded with
* signals.
* This parameter is configurable, this is the default value.
*/
#define SCAN_BATCH_SIZE 32768
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
#define MAX_SCHEMA_TRANSACTIONS 5
#define MAX_SCHEMA_OPERATIONS 256

/*
 * Long signals
 */
#define NDB_SECTION_SEGMENT_SZ 60

/*
 * Restore Buffer in pages
 *   4M
 */
#define LCP_RESTORE_BUFFER (4*32)

#define NDB_DEFAULT_HASHMAP_BUCKTETS 240

/**
 * Bits/mask used for coding/decoding blockno/blockinstance
 */
#define NDBMT_BLOCK_BITS 9
#define NDBMT_BLOCK_MASK ((1 << NDBMT_BLOCK_BITS) - 1)
#define NDBMT_BLOCK_INSTANCE_BITS 7

#define MAX_NDBMT_LQH_WORKERS 4
#define MAX_NDBMT_LQH_THREADS 4
#define MAX_NDBMT_TC_THREADS  2

#define NDB_FILE_BUFFER_SIZE (256*1024)

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

#endif
